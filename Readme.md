# 🦖 Dino Runner – Jogo para Raspberry Pico W

Dino Runner é uma implementação minimalista do clássico jogo do dinossauro do Google Chrome, desenvolvida para a **Raspberry Pico W** utilizando **linguagem C**. O jogo utiliza uma **matriz de LEDs 5x5** para exibir o dinossauro e os obstáculos, e uma tela **OLED SSD1306** para mostrar a pontuação do jogador.

## 🚀 Características
- **🎮 Controles simples** – Dois botões físicos para pular e abaixar.
- **🌈 Exibição via LEDs** – Dinossauro e obstáculos representados em uma matriz 5x5.
- **📟 Tela OLED** – Exibe a pontuação do jogador em tempo real.
- **⚡️ Aumento de dificuldade** – A velocidade dos obstáculos aumenta progressivamente.
- **💾 Código otimizado** – Utiliza máquina de estados para gerenciar a lógica do jogo.

## 📁 Estrutura do Projeto
📂 Dino Runner

├── 📄 dino_runner.c – Código principal do jogo

├── 📄 ws2818b.pio – Configuração do PIO para LEDs

├── 📄 CMakeLists.txt – Configuração da compilação

├── 📄 pico_sdk_import.cmake – Importação do SDK do Raspberry Pico

├── 📄 Readme.md – Este arquivo

└── 📄 .gitignore – Arquivos a serem ignorados no repositório

## 🔧 Tecnologias Utilizadas
- **Raspberry Pico W**
- **Linguagem C**
- **PIO (Programmable I/O) para controle dos LEDs**
- **Display OLED SSD1306 via I2C**
- **GPIO para interação com botões físicos**

## 📦 Dependências
Para compilar o projeto, você precisará do **Pico SDK** configurado no seu ambiente. Certifique-se de tê-lo instalado antes de prosseguir.

## 🛠️ Como Compilar e Executar
1. Clone o repositório:
   ```sh
   git clone https://github.com/seu-usuario/dino-runner.git
   cd dino-runner
2. Configure o Pico SDK:
   ```sh
   export PICO_SDK_PATH="/caminho/para/pico-sdk"
3. Crie um diretório para build e compile o código:
   ```sh
   mkdir build
   cd build
   cmake ..
   make
4. Carregue o arquivo .uf2 gerado na Raspberry Pico W.
5. Conecte os botões físicos e a matriz de LEDs conforme especificado no código.
6. Ligue a placa e jogue!

## 📜 Licença
Este projeto é de código aberto e distribuído sob a licença MIT.
