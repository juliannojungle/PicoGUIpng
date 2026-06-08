# Build Skill – PicoGUIpng

## Objetivo
Compilar o projeto PicoGUIpng no WSL (Ubuntu) a partir do Windows e copiar o binário gerado para `C:\temp`.

## Raiz do projeto (WSL)
`~/PicoGUIpng`

## Regras de decisão

### Compilação completa (usar quando):
- O diretório `build/` não existe, **ou**
- O arquivo `CMakeLists.txt` foi modificado desde a última compilação

Comandos WSL a executar em sequência:
```bash
cd ~/PicoGUIpng
rm -rf build
mkdir build
cd build
cmake ..
make
```

### Compilação incremental (usar quando):
- O diretório `build/` já existe **e**
- O arquivo `CMakeLists.txt` **não** foi modificado

Comandos WSL a executar em sequência:
```bash
cd ~/PicoGUIpng/build
clear
make
```

## Após qualquer compilação bem-sucedida
Copiar o executável gerado para o Windows:
```bash
cp ~/PicoGUIpng/build/picoguipng.uf2 /mnt/c/temp
```

## Como executar via terminal do Windows (cmd)
Para compilação completa:
```cmd
wsl -e bash -c "cd ~/PicoGUIpng && rm -rf build && mkdir build && cd build && cmake .. && make && cp ./picoguipng.uf2 /mnt/c/temp"
```

Para compilação incremental:
```cmd
wsl -e bash -c "cd ~/PicoGUIpng/build && make && cp ./picoguipng.uf2 /mnt/c/temp"
```

## Notas
- O binário final é `build/picoguipng.uf2` (formato para Raspberry Pi Pico).
- O destino no Windows é `C:\temp` — certifique-se que o diretório existe antes de copiar.
- Para verificar se `CMakeLists.txt` mudou, compare a data de modificação do arquivo com a data do diretório `build/`.
