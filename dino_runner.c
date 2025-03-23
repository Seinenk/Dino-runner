#include <stdio.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "time.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "pico/binary_info.h"
#include "inc/ssd1306.h"
#include "hardware/i2c.h"

#define max(a, b) ((a) > (b) ? (a) : (b))

// Biblioteca gerada pelo arquivo .pio durante compilação.
#include "ws2818b.pio.h"

// Definição do número total de LEDs e do pino de controle.
#define LED_COUNT 25
#define LED_PIN 7

// Fator de brilho para ajuste de intensidade luminosa
#define LED_BRIGHTNESS 0.3f

const uint I2C_SDA = 14;
const uint I2C_SCL = 15;

// Estrutura para representar um pixel na matriz de LEDs.
struct pixel_t
{
  uint8_t G, R, B; // Cada LED é composto por valores RGB.
};
typedef struct pixel_t pixel_t;
typedef pixel_t npLED_t; // Alias para npLED_t, representando um LED na matriz.

// Buffer de LEDs para representar a matriz 5x5 do jogo.
npLED_t leds[LED_COUNT];

// Variáveis para configuração da máquina de estado PIO.
PIO np_pio;
uint sm;

// Converte as coordenadas (x, y) da matriz de LEDs para um índice no buffer.
// Isso é necessário porque a matriz não segue um mapeamento linear direto.
int getIndex(int x, int y)
{
  // Se a linha for par (0, 2, 4), percorremos da esquerda para a direita.
  // Se a linha for ímpar (1, 3), percorremos da direita para a esquerda.
  if (y % 2 == 0)
  {
    return 24 - (y * 5 + x); // Linha par (esquerda para direita).
  }
  else
  {
    return 24 - (y * 5 + (4 - x)); // Linha ímpar (direita para esquerda).
  }
}

// Define a cor de um LED específico na matriz.
void npSetLED(const uint index, const uint8_t r, const uint8_t g, const uint8_t b)
{
  leds[index].R = (uint8_t)(r * LED_BRIGHTNESS);
  leds[index].G = (uint8_t)(g * LED_BRIGHTNESS);
  leds[index].B = (uint8_t)(b * LED_BRIGHTNESS);
}

// Atualiza o buffer de LEDs com os valores de um sprite fornecido.
void setLeds(int matriz[5][5][3])
{
  for (int linha = 0; linha < 5; linha++)
  {
    for (int coluna = 0; coluna < 5; coluna++)
    {
      int posicao = getIndex(linha, coluna);
      npSetLED(posicao, matriz[coluna][linha][0], matriz[coluna][linha][1], matriz[coluna][linha][2]);
    }
  }
}

// Inicializa a máquina de estado PIO para controlar os LEDs NeoPixel.
void npInit(uint pin)
{

  // Cria programa PIO.
  uint offset = pio_add_program(pio0, &ws2818b_program);
  np_pio = pio0;

  // Toma posse de uma máquina PIO.
  sm = pio_claim_unused_sm(np_pio, false);
  if (sm < 0)
  {
    np_pio = pio1;
    sm = pio_claim_unused_sm(np_pio, true); // Se nenhuma máquina estiver livre, panic!
  }

  // Inicia programa na máquina PIO obtida.
  ws2818b_program_init(np_pio, sm, offset, pin, 800000.f);

  // Inicializa buffer de LEDs com valores apagados.
  for (uint i = 0; i < LED_COUNT; ++i)
  {
    leds[i].R = 0;
    leds[i].G = 0;
    leds[i].B = 0;
  }
}

// Apaga toda a matriz de LEDs.
void npClear()
{
  for (uint i = 0; i < LED_COUNT; ++i)
    npSetLED(i, 0, 0, 0);
}

// Atualiza fisicamente a matriz de LEDs com os valores do buffer.
void npWrite()
{
  // Escreve cada dado de 8-bits dos pixels em sequência no buffer da máquina PIO.
  for (uint i = 0; i < LED_COUNT; ++i)
  {
    pio_sm_put_blocking(np_pio, sm, leds[i].G);
    pio_sm_put_blocking(np_pio, sm, leds[i].R);
    pio_sm_put_blocking(np_pio, sm, leds[i].B);
  }
  sleep_us(100); // Espera 100us, sinal de RESET do datasheet.
}

// Verifica se há colisão entre o dinossauro e um inimigo na matriz de LEDs.
int inimigo_colide(int posicao[2], int matriz[5][5][3])
{
  int linha = posicao[0];
  int coluna = posicao[1];

  return matriz[linha][coluna][0] != 0 || matriz[linha][coluna][1] != 0 || matriz[linha][coluna][2] != 0;
}

// Copia os dados de um sprite para outro.
void copia_sprite(int sprite1[5][5][3], int sprite2[5][5][3])
{
  for (int i = 0; i < 5; i++)
  {
    for (int j = 0; j < 5; j++)
    {
      sprite2[i][j][0] = sprite1[i][j][0];
      sprite2[i][j][1] = sprite1[i][j][1];
      sprite2[i][j][2] = sprite1[i][j][2];
    }
  }
}

// Reseta todos os pixels de um sprite para valores zerados.
void limpa_sprite(int sprite[5][5][3])
{
  for (int i = 0; i < 5; i++)
  {
    for (int j = 0; j < 5; j++)
    {
      sprite[i][j][0] = 0;
      sprite[i][j][1] = 0;
      sprite[i][j][2] = 0;
    }
  }
}

// Loop principal do jogo: gerencia a lógica do Dino Runner.
int main()
{
  srand(time(NULL));

  int inimigo = 1;

  // Inicializa os botões de entrada.
  const uint BUTTON_PIN = 5;
  gpio_init(BUTTON_PIN);
  gpio_set_dir(BUTTON_PIN, GPIO_IN);
  gpio_pull_up(BUTTON_PIN);

  const uint BUTTON_PIN2 = 6;
  gpio_init(BUTTON_PIN2);
  gpio_set_dir(BUTTON_PIN2, GPIO_IN);
  gpio_pull_up(BUTTON_PIN2);

  // Inicializa entradas e saídas.
  stdio_init_all();

  // Inicializa matriz de LEDs NeoPixel.
  npInit(LED_PIN);
  npClear();

  // Aqui, você desenha nos LEDs.

  int sprite_dinossauro_em_pe[5][5][3] = {
      {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
      {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
      {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
      {{0, 0, 255}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
      {{0, 0, 255}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
  };

  int sprite_dinossauro_pulando[5][5][3] = {
      {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
      {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
      {{0, 0, 255}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
      {{0, 0, 255}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
      {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
  };

  int sprite_dinossauro_abaixado[5][5][3] = {
      {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
      {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
      {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
      {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
      {{0, 0, 255}, {0, 0, 255}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
  };

  int colidiu = 0;
  int pulo = 0;
  int pulo_delay = 200;
  int abaixado = 0;
  int abaixado_delay = 200;

  int posicao_inimigo[2] = {4, 4};
  int posicao_inimigo2[2] = {3, -1};

  int sprite_atual[5][5][3] = {
      {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
      {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
      {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
      {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
      {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
  };

  int total_delay = 1000;
  int sleep_delay = 10;
  int steps = total_delay / sleep_delay;

  int inimigo_delay_minimo = 50;
  int inimigo_delay_minimo2 = 50;

  int inimigo_delay = 200;
  int inimigo_delay2 = 200;

  int inimigo_delay_reducao = 25;
  int inimigo_delay_reducao2 = 25;

  int segundos_para_reducao_de_velocidade = 5;

  int inimigo_delay_atual = inimigo_delay;
  int inimigo_delay_atual2 = inimigo_delay2;

  int inimigos_desviados = 0;
  int atualiza_mensagem_de_inimigos_desviados = 0;

  // Inicialização do i2c
  i2c_init(i2c1, ssd1306_i2c_clock * 1000);
  gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
  gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
  gpio_pull_up(I2C_SDA);
  gpio_pull_up(I2C_SCL);

  // Processo de inicialização completo do OLED SSD1306
  ssd1306_init();

  // Preparar área de renderização para o display (ssd1306_width pixels por ssd1306_n_pages páginas)
  struct render_area frame_area = {
    start_column : 0,
    end_column : ssd1306_width - 1,
    start_page : 0,
    end_page : ssd1306_n_pages - 1
  };

  calculate_render_area_buffer_length(&frame_area);

  // zera o display inteiro
  uint8_t ssd[ssd1306_buffer_length];
  memset(ssd, 0, ssd1306_buffer_length);
  render_on_display(ssd, &frame_area);

  clock_t tempo_desde_velocidade_atualizada = clock();

  setLeds(sprite_dinossauro_em_pe);

  npWrite(); // Escreve os dados nos LEDs.

  // Não faz mais nada. Loop infinito.
  while (true)
  {

    for (int i = 0; i < steps; i++)
    {
      atualiza_mensagem_de_inimigos_desviados = 0;

      if (posicao_inimigo[1] == -1 && posicao_inimigo2[1] == -1)
      {
        if (rand() % 2 == 0)
        {
          inimigo = 1;
        }
        else
        {
          inimigo = 2;
        }
      }

      limpa_sprite(sprite_atual);

      if (gpio_get(BUTTON_PIN) == 0)
      { // Botão pressionado (nível lógico baixo)
        pulo = 1;
        pulo_delay = 100;
      }
      else if (pulo_delay <= 0)
      {
        pulo = 0;
      }

      if (gpio_get(BUTTON_PIN2) == 0)
      { // Botão pressionado (nível lógico baixo)
        abaixado = 1;
      }
      else if (abaixado_delay <= 0)
      {
        abaixado = 0;
      }

      if (abaixado == 1)
      {
        copia_sprite(sprite_dinossauro_abaixado, sprite_atual);
      }
      else if (pulo == 1)
      {
        copia_sprite(sprite_dinossauro_pulando, sprite_atual);
      }
      else
      {
        copia_sprite(sprite_dinossauro_em_pe, sprite_atual);
      }

      if (inimigo_delay_atual <= 0 && inimigo == 1)
      {
        if (posicao_inimigo[1] == -1)
        {
          posicao_inimigo[1] = 4;
        }
        else
        {
          posicao_inimigo[1]--;

          if (posicao_inimigo[1] == -1)
          {
            inimigos_desviados++;
            atualiza_mensagem_de_inimigos_desviados = 1;
          }
        }

        clock_t tempo_agora = clock();

        if ((tempo_agora - tempo_desde_velocidade_atualizada) / CLOCKS_PER_SEC >= segundos_para_reducao_de_velocidade)
        {
          inimigo_delay = max(inimigo_delay - inimigo_delay_reducao, inimigo_delay_minimo);
          tempo_desde_velocidade_atualizada = tempo_agora;
        }

        inimigo_delay_atual = inimigo_delay;
      }

      if (inimigo_delay_atual2 <= 0 && inimigo == 2)
      {
        if (posicao_inimigo2[1] == -1)
        {
          posicao_inimigo2[1] = 4;
        }
        else
        {
          posicao_inimigo2[1]--;

          if (posicao_inimigo2[1] == -1)
          {
            inimigos_desviados++;
            atualiza_mensagem_de_inimigos_desviados = 1;
          }
        }

        clock_t tempo_agora = clock();

        if ((tempo_agora - tempo_desde_velocidade_atualizada) / CLOCKS_PER_SEC >= segundos_para_reducao_de_velocidade)
        {
          inimigo_delay2 = max(inimigo_delay2 - inimigo_delay_reducao2, inimigo_delay_minimo2);
          tempo_desde_velocidade_atualizada = tempo_agora;
        }

        inimigo_delay_atual2 = inimigo_delay2;
      }

      if (inimigo_colide(posicao_inimigo, sprite_atual) || inimigo_colide(posicao_inimigo2, sprite_atual))
      {
        // game over
        colidiu = 1;
      }

      if (posicao_inimigo[1] != -1)
      {
        int linha = posicao_inimigo[0];
        int coluna = posicao_inimigo[1];

        // vermelho
        sprite_atual[linha][coluna][0] = 255;
        sprite_atual[linha][coluna][1] = 0;
        sprite_atual[linha][coluna][2] = 0;
      }

      if (posicao_inimigo2[1] != -1)
      {
        int linha = posicao_inimigo2[0];
        int coluna = posicao_inimigo2[1];

        // vermelho
        sprite_atual[linha][coluna][0] = 255;
        sprite_atual[linha][coluna][1] = 0;
        sprite_atual[linha][coluna][2] = 0;
      }

      setLeds(sprite_atual);

      npWrite();

      sleep_ms(sleep_delay);

      if (pulo_delay >= 0)
      {
        pulo_delay -= sleep_delay;
      }

      if (abaixado_delay >= 0)
      {
        abaixado_delay -= sleep_delay;
      }

      if (inimigo_delay_atual >= 0)
      {
        inimigo_delay_atual -= sleep_delay;
      }

      if (inimigo_delay_atual2 >= 0)
      {
        inimigo_delay_atual2 -= sleep_delay;
      }

      if (atualiza_mensagem_de_inimigos_desviados == 1)
      {
        uint8_t ssd[ssd1306_buffer_length];
        memset(ssd, 0, ssd1306_buffer_length);
        render_on_display(ssd, &frame_area);

        // Parte do código para exibir a mensagem no display (opcional: mudar ssd1306_height para 32 em ssd1306_i2c.h)
        // /**
        char str[16]; // maximo de caracteres (128 / 8)
        sprintf(str, "  Desviados: %d", inimigos_desviados);

        char *text[] = {
            "  Dino Runner   ",
            str};

        int y = 0;
        for (uint i = 0; i < count_of(text); i++)
        {
          ssd1306_draw_string(ssd, 5, y, text[i]);
          y += 8;
        }
        render_on_display(ssd, &frame_area);
      }

      if (colidiu == 1)
      {
        return 0;
      }
    }
  }
}
