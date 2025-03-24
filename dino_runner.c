#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/i2c.h"

#include "inc/ssd1306.h" // Biblioteca para controlar o display
#include "ws2818b.pio.h" // Programa PIO para controlar os LEDs RGB

// Função auxiliar: retorna o maior valor entre a e b
#define max(a, b) ((a) > (b) ? (a) : (b))

// Quantidade de LEDs na matriz (5x5 = 25)
#define LED_COUNT 25
// Pino onde a fita/matriz de LEDs está conectada
#define LED_PIN 7
// Fator de brilho: 0.3f = 30% da intensidade
#define LED_BRIGHTNESS 0.3f

// Pinos dos botões
#define BUTTON_PIN 5  // Botão de pulo
#define BUTTON_PIN2 6 // Botão de abaixar

// Pinos I2C para o display (SSD1306)
const uint I2C_SDA = 14;
const uint I2C_SCL = 15;

// Estrutura de cada LED: compõe-se de valores G, R, B
struct pixel_t
{
  uint8_t G, R, B;
};
// Alias para pixel_t
typedef struct pixel_t pixel_t;
typedef pixel_t npLED_t;

// Array global que representa nosso buffer de LEDs
npLED_t leds[LED_COUNT];

// Variáveis para trabalhar com PIO
PIO np_pio;
uint sm;

//------------------------------------------------------------------------------
// getIndex: Converte (x,y) em um índice para acessar o array leds[]
//------------------------------------------------------------------------------
// Observação: Como a fiação da matriz nem sempre é linear, precisamos de um
// mapeamento personalizado.
// Neste caso, se a linha y é par, lemos da esquerda pra direita,
// se ímpar, da direita pra esquerda. Ajuste se sua ligação for diferente.
//------------------------------------------------------------------------------
int getIndex(int x, int y)
{
  if (y % 2 == 0)
  {
    return 24 - (y * 5 + x);
  }
  else
  {
    return 24 - (y * 5 + (4 - x));
  }
}

//------------------------------------------------------------------------------
// npSetLED: Ajusta as cores de um pixel específico do buffer, aplicando o brilho.
//------------------------------------------------------------------------------
void npSetLED(uint index, uint8_t r, uint8_t g, uint8_t b)
{
  leds[index].R = (uint8_t)(r * LED_BRIGHTNESS);
  leds[index].G = (uint8_t)(g * LED_BRIGHTNESS);
  leds[index].B = (uint8_t)(b * LED_BRIGHTNESS);
}

//------------------------------------------------------------------------------
// setLeds: Copia uma matriz 5x5x3 ("sprite") para o buffer de LEDs global.
// Cada pixel da matriz vira um LED na posição mapeada por getIndex.
//------------------------------------------------------------------------------
void setLeds(int matriz[5][5][3])
{
  for (int linha = 0; linha < 5; linha++)
  {
    for (int coluna = 0; coluna < 5; coluna++)
    {
      int pos = getIndex(linha, coluna);
      npSetLED(pos,
               matriz[coluna][linha][0],
               matriz[coluna][linha][1],
               matriz[coluna][linha][2]);
    }
  }
}

//------------------------------------------------------------------------------
// npInit: Inicializa a PIO para enviar dados aos LEDs do tipo WS2812
//------------------------------------------------------------------------------
void npInit(uint pin)
{
  // Adiciona programa PIO no pio0
  uint offset = pio_add_program(pio0, &ws2818b_program);
  np_pio = pio0;

  // Tenta obter uma state machine livre
  sm = pio_claim_unused_sm(np_pio, false);
  if (sm < 0)
  {
    // Se não houver, tentamos o pio1
    np_pio = pio1;
    sm = pio_claim_unused_sm(np_pio, true);
  }

  // Inicializa a SM com a frequencia de 800kHz para NeoPixel
  ws2818b_program_init(np_pio, sm, offset, pin, 800000.f);

  // Zera o buffer inicial
  for (uint i = 0; i < LED_COUNT; i++)
  {
    leds[i].R = 0;
    leds[i].G = 0;
    leds[i].B = 0;
  }
}

//------------------------------------------------------------------------------
// npClear: Apaga todos os LEDs do buffer (atribui 0 a cada componente)
//------------------------------------------------------------------------------
void npClear()
{
  for (uint i = 0; i < LED_COUNT; i++)
  {
    npSetLED(i, 0, 0, 0);
  }
}

//------------------------------------------------------------------------------
// npWrite: Envia o conteúdo do buffer leds[] para a fita/matriz WS2812
//------------------------------------------------------------------------------
void npWrite()
{
  for (uint i = 0; i < LED_COUNT; i++)
  {
    pio_sm_put_blocking(np_pio, sm, leds[i].G);
    pio_sm_put_blocking(np_pio, sm, leds[i].R);
    pio_sm_put_blocking(np_pio, sm, leds[i].B);
  }
  // Pequeno delay para resetar o sinal dos WS2812
  sleep_us(100);
}

//------------------------------------------------------------------------------
// inimigo_colide: Verifica se o sprite do dino e o inimigo se sobrepõem.
//------------------------------------------------------------------------------
// Retorna != 0 se as coordenadas do inimigo contiverem algum pixel != 0 no
// sprite do dino, caracterizando colisão.
//------------------------------------------------------------------------------
int inimigo_colide(int posicao[2], int matriz[5][5][3])
{
  int linha = posicao[0];
  int coluna = posicao[1];
  return (matriz[linha][coluna][0] != 0 ||
          matriz[linha][coluna][1] != 0 ||
          matriz[linha][coluna][2] != 0);
}

//------------------------------------------------------------------------------
// copia_sprite: Copia o conteúdo de uma matriz 5x5x3 para outra
//------------------------------------------------------------------------------
void copia_sprite(int src[5][5][3], int dst[5][5][3])
{
  for (int i = 0; i < 5; i++)
  {
    for (int j = 0; j < 5; j++)
    {
      dst[i][j][0] = src[i][j][0];
      dst[i][j][1] = src[i][j][1];
      dst[i][j][2] = src[i][j][2];
    }
  }
}

//------------------------------------------------------------------------------
// limpa_sprite: Zera todos os pixels de uma matriz 5x5x3
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// main: Função principal do jogo Dino Runner (com 1 inimigo)
//------------------------------------------------------------------------------
int main()
{
  // Usa tempo em microssegundos desde o boot como semente de rand(),
  // garantindo seeds mais variadas.
  srand((unsigned)to_us_since_boot(get_absolute_time()));

  // Flag que indica se houve colisão (Game Over)
  int colidiu = 0;

  // Inicializa GPIO dos botões (pulo e abaixar)
  gpio_init(BUTTON_PIN);
  gpio_set_dir(BUTTON_PIN, GPIO_IN);
  gpio_pull_up(BUTTON_PIN);

  gpio_init(BUTTON_PIN2);
  gpio_set_dir(BUTTON_PIN2, GPIO_IN);
  gpio_pull_up(BUTTON_PIN2);

  // Habilita entrada/saída padrão
  stdio_init_all();

  // Inicia LEDs e limpa
  npInit(LED_PIN);
  npClear();

  // Sprites do dinossauro em diferentes posições (em pé, pulando, abaixado)
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

  // Controle do dino
  int pulo = 0;
  int pulo_delay = 200;
  int abaixado = 0;
  int abaixado_delay = 200;

  // Variáveis do inimigo: posicao_inimigo[0] => linha, [1] => coluna
  // Inicia fora da tela (coluna -1)
  int posicao_inimigo[2] = {4, -1};

  // Sprite temporário do dino + inimigo
  int sprite_atual[5][5][3];
  limpa_sprite(sprite_atual);

  // Tempo total de um "ciclo" e steps
  int total_delay = 1000;
  int sleep_delay = 10;
  int steps = total_delay / sleep_delay;

  // Ajustes de delay do inimigo
  int inimigo_delay_minimo = 50;
  int inimigo_delay = 200;
  int inimigo_delay_reducao = 25;
  int segundos_para_reducao_de_velocidade = 5;
  int inimigo_delay_atual = inimigo_delay;

  // Placar de inimigos desviados
  int inimigos_desviados = 0;
  int atualiza_mensagem_de_inimigos_desviados = 0;

  // Inicializa I2C e o display SSD1306
  i2c_init(i2c1, ssd1306_i2c_clock * 1000);
  gpio_set_function(14, GPIO_FUNC_I2C);
  gpio_set_function(15, GPIO_FUNC_I2C);
  gpio_pull_up(14);
  gpio_pull_up(15);

  ssd1306_init();

  // Prepara a "área" de renderização no display (full screen)
  struct render_area frame_area = {
      .start_column = 0,
      .end_column = ssd1306_width - 1,
      .start_page = 0,
      .end_page = ssd1306_n_pages - 1};
  calculate_render_area_buffer_length(&frame_area);

  // Limpa o display
  uint8_t ssd[ssd1306_buffer_length];
  memset(ssd, 0, ssd1306_buffer_length);
  render_on_display(ssd, &frame_area);

  // Desenha o dinossauro em pé inicialmente
  setLeds(sprite_dinossauro_em_pe);
  npWrite();

  // Usado para medir tempo de redução de velocidade do inimigo
  clock_t tempo_desde_velocidade_atualizada = clock();

  // Loop principal do jogo
  while (true)
  {

    for (int i = 0; i < steps; i++)
    {
      // A cada "passo", resetamos a flag de msg no display
      atualiza_mensagem_de_inimigos_desviados = 0;

      // Limpa sprite_atual para refazer o frame
      limpa_sprite(sprite_atual);

      // Leitura dos botões (puxados para cima, logo 0 = pressionado)
      if (gpio_get(BUTTON_PIN) == 0)
      {
        pulo = 1;
        pulo_delay = 100;
      }
      else if (pulo_delay <= 0)
      {
        pulo = 0;
      }

      if (gpio_get(BUTTON_PIN2) == 0)
      {
        abaixado = 1;
      }
      else if (abaixado_delay <= 0)
      {
        abaixado = 0;
      }

      // Escolhe qual sprite de dino usar (em pé, pulando ou abaixado)
      if (abaixado)
      {
        copia_sprite(sprite_dinossauro_abaixado, sprite_atual);
      }
      else if (pulo)
      {
        copia_sprite(sprite_dinossauro_pulando, sprite_atual);
      }
      else
      {
        copia_sprite(sprite_dinossauro_em_pe, sprite_atual);
      }

      // Lógica de movimentação do inimigo
      if (inimigo_delay_atual <= 0)
      {
        // Se inimigo saiu da tela, reaparece
        if (posicao_inimigo[1] == -1)
        {
          posicao_inimigo[1] = 4;              // coluna inicial
          posicao_inimigo[0] = rand() % 3 + 2; // linha aleatória (2..4)
        }
        else
        {
          // Move o inimigo para a esquerda
          posicao_inimigo[1]--;
          // Se saiu da coluna -1, significa que o dino desviou
          if (posicao_inimigo[1] == -1)
          {
            inimigos_desviados++;
            atualiza_mensagem_de_inimigos_desviados = 1;
          }
        }
        // Se passou X segundos, reduz delay (aumenta velocidade)
        clock_t tempo_agora = clock();
        if ((tempo_agora - tempo_desde_velocidade_atualizada) / CLOCKS_PER_SEC >= segundos_para_reducao_de_velocidade)
        {
          inimigo_delay = max(inimigo_delay - inimigo_delay_reducao, inimigo_delay_minimo);
          tempo_desde_velocidade_atualizada = tempo_agora;
        }
        // Reinicia o contador de delay do inimigo
        inimigo_delay_atual = inimigo_delay;
      }

      // Verifica colisão (Game Over se colidiu)
      if (inimigo_colide(posicao_inimigo, sprite_atual))
      {
        colidiu = 1;
      }

      // Se o inimigo está visível, pinta ele de vermelho
      if (posicao_inimigo[1] != -1)
      {
        int L = posicao_inimigo[0];
        int C = posicao_inimigo[1];
        sprite_atual[L][C][0] = 255;
        sprite_atual[L][C][1] = 0;
        sprite_atual[L][C][2] = 0;
      }

      // Copia sprite para o buffer de LEDs e escreve fisicamente
      setLeds(sprite_atual);
      npWrite();

      // Pequeno delay entre cada "step"
      sleep_ms(sleep_delay);

      // Decrementa timers de pulo, abaixado e do inimigo
      if (pulo_delay >= 0)
        pulo_delay -= sleep_delay;
      if (abaixado_delay >= 0)
        abaixado_delay -= sleep_delay;
      if (inimigo_delay_atual >= 0)
        inimigo_delay_atual -= sleep_delay;

      // Se desviou, atualiza mensagem no display
      if (atualiza_mensagem_de_inimigos_desviados)
      {
        // Apaga a tela
        memset(ssd, 0, ssd1306_buffer_length);
        render_on_display(ssd, &frame_area);

        // Cria mensagem de "Dino Runner" + quantidade de desviados
        char str[16];
        sprintf(str, "  Desviados: %d", inimigos_desviados);

        char *text[] = {
            "  Dino Runner   ",
            str};

        // Desenha essas linhas no buffer do display
        int y = 0;
        for (uint j = 0; j < count_of(text); j++)
        {
          ssd1306_draw_string(ssd, 5, y, text[j]);
          y += 8;
        }
        render_on_display(ssd, &frame_area);
      }

      // Se colidiu, encerramos o jogo (retorna ao sistema)
      if (colidiu)
      {
        return 0;
      }
    }
  }
}
