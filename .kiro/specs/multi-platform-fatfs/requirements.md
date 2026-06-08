# Requirements Document

## Introduction

Este documento descreve os requisitos para refatorar o projeto PicoGUIpng de uma arquitetura mono-plataforma (RP2040) para multi-plataforma, adicionando suporte ao ESP32. A refatoração substitui a dependência `no-OS-FatFS-SD-SDIO-SPI-RPi-Pico` (específica para RP2040) pelo FatFS puro de ChaN, implementando a interface `diskio.h` separadamente para cada plataforma-alvo via SPI. O código de aplicação (`fileHelper.c`, `pngHelper.c`, `sample.c`) deve permanecer inalterado quanto ao uso da API FatFS.

## Glossary

- **FatFS**: Biblioteca FAT/exFAT filesystem portável de ChaN (http://elm-chan.org/fsw/ff/00index_e.html)
- **diskio**: Interface de baixo nível do FatFS composta pelas funções `disk_initialize`, `disk_status`, `disk_read`, `disk_write` e `disk_ioctl`, declaradas em `diskio.h`
- **Platform_Layer**: Camada de abstração de hardware responsável por implementar a interface `diskio` para cada plataforma-alvo
- **RP2040**: Microcontrolador da Raspberry Pi Foundation, plataforma já suportada pelo projeto
- **ESP32**: Microcontrolador da Espressif Systems, plataforma a ser adicionada
- **Pico_SDK**: SDK oficial da Raspberry Pi para desenvolvimento no RP2040, usado com CMake
- **ESP-IDF**: Framework oficial da Espressif para desenvolvimento no ESP32, usado como dependência CMake (sem exigir `idf.py`)
- **SPI**: Serial Peripheral Interface — barramento de comunicação usado para acessar o cartão SD em ambas as plataformas
- **SD_Card**: Cartão de memória SD acessado via SPI para leitura e escrita de arquivos
- **Application_Code**: Conjunto de arquivos `fileHelper.c`, `pngHelper.c` e `sample.c` que consomem a API FatFS de alto nível
- **no-OS-FatFS**: Submódulo atual (`no-OS-FatFS-SD-SDIO-SPI-RPi-Pico`) específico para RP2040, a ser removido

## Requirements

### Requirement 1: Substituição da dependência no-OS-FatFS pelo FatFS puro

**User Story:** Como desenvolvedor do PicoGUIpng, quero substituir o submódulo `no-OS-FatFS` pelo FatFS puro de ChaN, para que a camada de filesystem seja portável e não dependa de uma implementação específica para RP2040.

#### Acceptance Criteria

1. THE Build_System SHALL incluir o FatFS puro de ChaN como submódulo Git em `src/Dependency/fatfs`
2. THE Build_System SHALL remover o submódulo `no-OS-FatFS-SD-SDIO-SPI-RPi-Pico` de `src/Dependency/no-OS-FatFS`
3. IF o submódulo `no-OS-FatFS` ainda estiver presente em `src/Dependency/no-OS-FatFS`, THEN THE Build_System SHALL interromper a compilação com erro antes de compilar qualquer arquivo do FatFS puro
4. THE Build_System SHALL compilar os arquivos-fonte do FatFS (`ff.c`, `ffsystem.c`, `ffunicode.c`) como parte do projeto
5. WHEN o projeto for compilado para qualquer plataforma suportada, THE Build_System SHALL falhar na compilação se os cabeçalhos `ff.h` e `diskio.h` do FatFS puro não forem encontrados

### Requirement 2: Interface diskio implementada para RP2040

**User Story:** Como desenvolvedor, quero uma implementação da interface `diskio.h` para o RP2040, para que o FatFS puro possa controlar o cartão SD via SPI usando os periféricos nativos do Pico SDK.

#### Acceptance Criteria

1. THE Platform_Layer SHALL implementar a função `disk_initialize` para o RP2040 inicializando o periférico SPI via `hardware/spi.h` do Pico SDK
2. THE Platform_Layer SHALL implementar a função `disk_status` para o RP2040 retornando o estado atual do cartão SD
3. WHEN `disk_read` for chamado com setor e contagem válidos, THE Platform_Layer SHALL ler os dados do cartão SD via SPI e copiá-los para o buffer informado
4. WHEN `disk_write` for chamado com setor e contagem válidos, THE Platform_Layer SHALL escrever os dados do buffer no cartão SD via SPI
5. THE Platform_Layer SHALL implementar `disk_ioctl` para o RP2040 respondendo aos controles `CTRL_SYNC`, `GET_SECTOR_COUNT`, `GET_SECTOR_SIZE` e `GET_BLOCK_SIZE`
6. THE RP2040_diskio SHALL residir em `src/lib/Platform/RP2040/diskio.c`

### Requirement 3: Interface diskio implementada para ESP32

**User Story:** Como desenvolvedor, quero uma implementação da interface `diskio.h` para o ESP32, para que o FatFS puro possa controlar o cartão SD via SPI usando os periféricos nativos do ESP-IDF.

#### Acceptance Criteria

1. THE Platform_Layer SHALL implementar a função `disk_initialize` para o ESP32 inicializando o periférico SPI via `driver/spi_master.h` do ESP-IDF
2. THE Platform_Layer SHALL implementar a função `disk_status` para o ESP32 retornando o estado atual do cartão SD
3. WHEN `disk_read` for chamado com setor e contagem válidos, THE Platform_Layer SHALL ler os dados do cartão SD via SPI usando as APIs do ESP-IDF e copiá-los para o buffer informado
4. WHEN `disk_write` for chamado com setor e contagem válidos, THE Platform_Layer SHALL escrever os dados do buffer no cartão SD via SPI usando as APIs do ESP-IDF
5. THE Platform_Layer SHALL implementar `disk_ioctl` para o ESP32 respondendo aos controles `CTRL_SYNC`, `GET_SECTOR_COUNT`, `GET_SECTOR_SIZE` e `GET_BLOCK_SIZE`
6. THE ESP32_diskio SHALL residir em `src/lib/Platform/ESP32/diskio.c`

### Requirement 4: Isolamento de código específico de plataforma

**User Story:** Como desenvolvedor, quero que todo código específico de hardware esteja isolado dentro dos diretórios `Platform/RP2040` e `Platform/ESP32`, para que o código de aplicação não precise conhecer qual plataforma está sendo usada.

#### Acceptance Criteria

1. THE Application_Code SHALL usar exclusivamente funções da API FatFS (`f_open`, `f_read`, `f_write`, `f_close`, `f_mount`, `f_chdrive`, `f_unmount`) sem incluir cabeçalhos específicos de plataforma
2. THE Application_Code SHALL ser compilável para RP2040 e ESP32 sem modificações nos arquivos `fileHelper.c`, `pngHelper.c` e `sample.c`
3. IF um cabeçalho específico de plataforma (por exemplo, `hardware/spi.h`, `driver/spi_master.h`) for incluído em qualquer arquivo fora de `src/lib/Platform/`, THEN THE Build_System SHALL falhar na compilação
4. THE Platform_Layer SHALL expor uma função de inicialização `Platform_SDCard_Init` que encapsula a configuração de pinos SPI e parâmetros de velocidade para cada plataforma

### Requirement 5: fileHelper.c refatorado para usar FatFS puro

**User Story:** Como desenvolvedor, quero que `fileHelper.c` seja refatorado para usar apenas a API FatFS e a nova abstração de plataforma, para que as dependências de `hw_config.h` e dos tipos `spi_t`/`sd_card_t` do no-OS-FatFS sejam removidas.

#### Acceptance Criteria

1. THE Application_Code SHALL remover a inclusão de `hw_config.h` e quaisquer referências aos tipos `spi_t` e `sd_card_t` provenientes do no-OS-FatFS
2. THE Application_Code SHALL remover as implementações das funções `sd_get_num`, `sd_get_by_num`, `spi_get_num` e `spi_get_by_num` que existem exclusivamente para satisfazer a interface do no-OS-FatFS
3. WHEN `fileHelper.c` for compilado, THE Build_System SHALL não requerer nenhum cabeçalho do no-OS-FatFS
4. THE Application_Code SHALL chamar `Platform_SDCard_Init` durante a inicialização antes de invocar `f_mount`, e IF `Platform_SDCard_Init` retornar falha, THEN THE Application_Code SHALL abortar a inicialização sem chamar `f_mount`

### Requirement 6: Sistema de build para RP2040 (CMake + Pico SDK)

**User Story:** Como desenvolvedor, quero que o build para RP2040 continue usando CMake e Pico SDK, compilando o FatFS puro e a implementação `diskio` do RP2040, para que o projeto continue gerando o binário `.uf2` sem alterações no fluxo de desenvolvimento existente.

#### Acceptance Criteria

1. THE Build_System SHALL mover os arquivos `src/pre-executable.cmake` e `src/pos-executable.cmake` para `src/lib/Platform/RP2040/pre-executable.cmake` e `src/lib/Platform/RP2040/pos-executable.cmake`
2. THE RP2040 pre-executable.cmake SHALL incluir o FatFS puro e a implementação `diskio` do RP2040 em vez da referência ao `no-OS-FatFS`
3. THE RP2040 pos-executable.cmake SHALL linkar o alvo de biblioteca gerado para o FatFS puro em vez de `FatFs_SPI`
4. WHEN o build para RP2040 for executado, THE Build_System SHALL gerar o binário `.uf2` sem erros de link

### Requirement 7: Sistema de build multi-plataforma via CMake

**User Story:** Como desenvolvedor, quero que o build para ESP32 use CMake puro (sem dependência de `idf.py` ou Python), com seleção de plataforma via variável `PLATFORM_NAME`, para que o projeto pai possa incluir os fragmentos `.cmake` necessários para a plataforma desejada sem alterar o fluxo de build.

#### Acceptance Criteria

1. THE Build_System SHALL suportar a variável CMake `PLATFORM_NAME` com valores `RP2040` (padrão) e `ESP32`
2. THE Build_System SHALL expor a variável `ESP_IDF_PATH` (com padrão `~/esp-idf`) de forma análoga a `PICO_SDK_PATH`, permitindo que o projeto pai a sobrescreva via `-DESP_IDF_PATH=<caminho>`
3. THE Build_System SHALL incluir `src/lib/Platform/ESP32/pre-executable.cmake` e `src/lib/Platform/ESP32/pos-executable.cmake` para o build ESP32
4. THE Build_System SHALL compilar o FatFS puro e a implementação `diskio` do ESP32 quando `PLATFORM_NAME` for `ESP32`
5. THE CMakeLists.txt da raiz SHALL servir como exemplo funcional de compilação para ambas as plataformas, incluindo `src/lib/Platform/${PLATFORM_NAME}/pre-executable.cmake` e `src/lib/Platform/${PLATFORM_NAME}/pos-executable.cmake` com base na variável `PLATFORM_NAME`
6. THE Build_System SHALL incluir os cabeçalhos do ESP-IDF (`driver/spi_master.h`, `driver/gpio.h`) somente nos arquivos dentro de `src/lib/Platform/ESP32/`
7. THE Build_System SHALL não exigir Python, `idf.py`, nem qualquer ferramenta além de CMake e o toolchain apropriado para compilar o projeto

### Requirement 8: Configuração de pinos SPI por plataforma

**User Story:** Como desenvolvedor, quero que os pinos SPI usados para comunicação com o cartão SD sejam configuráveis por plataforma via constantes de compilação, para que o projeto possa ser adaptado a diferentes layouts de hardware sem alterar o código-fonte.

#### Acceptance Criteria

1. THE Platform_Layer SHALL definir os pinos padrão SCLK, MOSI, MISO e CS para o SD Card do RP2040 como constantes em `src/lib/Platform/RP2040/platform_config.h`
2. THE Platform_Layer SHALL definir os pinos padrão SCLK, MOSI, MISO e CS para o SD Card do ESP32 como constantes em `src/lib/Platform/ESP32/platform_config.h`
3. WHERE a constante de pino for redefinida via flag de compilação (`-DSD_SPI_SCLK=X`), THE Platform_Layer SHALL usar o valor fornecido em vez do padrão
4. THE Platform_Layer SHALL usar velocidade SPI nominal de 25 MHz para leitura/escrita de dados no cartão SD em ambas as plataformas, admitindo variações resultantes de restrições do divisor de clock do hardware

### Requirement 9: Tratamento de erros na camada diskio

**User Story:** Como desenvolvedor, quero que a camada `diskio` trate erros de comunicação SPI e retorne os códigos de resultado apropriados do FatFS, para que o código de aplicação possa reagir adequadamente a falhas no cartão SD.

#### Acceptance Criteria

1. IF `disk_initialize` falhar na inicialização do SPI ou detecção do cartão SD, THEN THE Platform_Layer SHALL retornar o status `STA_NOINIT`
2. WHILE o disco estiver no estado `STA_NOINIT`, THE Platform_Layer SHALL retornar erro imediatamente em qualquer chamada a `disk_read`, `disk_write` ou `disk_ioctl` sem tentar acessar o hardware
3. IF `disk_read` receber um número de setor fora dos limites da capacidade do cartão, THEN THE Platform_Layer SHALL retornar `RES_PARERR`
4. IF `disk_write` receber um número de setor fora dos limites da capacidade do cartão, THEN THE Platform_Layer SHALL retornar `RES_PARERR`
5. IF uma operação SPI falhar durante `disk_read` ou `disk_write`, THEN THE Platform_Layer SHALL retornar `RES_ERROR`
6. IF `disk_ioctl` for chamado com um comando não suportado, THEN THE Platform_Layer SHALL retornar `RES_PARERR`
