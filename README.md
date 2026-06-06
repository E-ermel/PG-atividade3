# Tilemap Isométrico (Diamond) — Pato

Atividade 3 de Processamento Gráfico. Um tilemap isométrico (projeção *diamond*)
renderizado em **C++ / OpenGL** (GLEW + GLFW), onde um pato se desloca pelas
células e cada tile pisado avança de cor. O objetivo é deixar todos os tiles da
mesma cor.

## Descrição

- O mapa é uma matriz 3×3 de tiles desenhados em projeção isométrica *diamond*.
- O pato anda nas **8 vizinhanças** (cardeais e diagonais) da matriz.
- A cada passo, o tile pisado **avança de cor** (índices `0..6`).
- Ao chegar na cor **rosa (6)**, o tile fica **bloqueado** e não pode mais ser pisado.
- **Vitória:** quando todos os tiles ficam da mesma cor.

### Tiles (cores)

| Índice | Cor        |
|:------:|------------|
| 0      | bege       |
| 1      | verde      |
| 2      | preto      |
| 3      | lava       |
| 4      | azul-claro |
| 5      | azul       |
| 6      | rosa (bloqueia) |

## Controles

| Tecla | Ação           |
|:-----:|----------------|
| `W`   | cima           |
| `S`   | baixo          |
| `A`   | esquerda       |
| `D`   | direita        |
| `Q`   | diagonal cima-esquerda  |
| `E`   | diagonal cima-direita   |
| `Z`   | diagonal baixo-esquerda |
| `C`   | diagonal baixo-direita  |
| `ESC` | sair           |

## Estrutura do projeto

```
PG-atividade3/
├── src/
│   ├── main.cpp        # código principal (render, lógica, shaders)
│   └── stb_image.h     # carregamento de imagens (biblioteca header-only)
├── yellow/             # sprites do pato (animações)
│   └── individual_animations/idle/png_sequence/duckee_idle1.png
└── README.md
```

## Assets

O código carrega em tempo de execução:

- **Tileset:** `assets/tileset.png`. Caso não exista, um tileset de 7 losangos é
  **gerado proceduralmente** em memória (`makeProceduralTileset`).
- **Pato (idle):** `yellow/individual_animations/idle/png_sequence/duckee_idle1.png`.

## Compilação (macOS / Apple Silicon)

Dependências via Homebrew:

```bash
brew install glew glfw
```

Compilar:

```bash
clang++ -std=c++17 -O2 src/main.cpp -o game \
  -I$(brew --prefix glew)/include -I$(brew --prefix glfw)/include \
  -L$(brew --prefix glew)/lib    -L$(brew --prefix glfw)/lib \
  -lGLEW -lglfw \
  -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
```

> O warning de *deprecation* do OpenGL no macOS é esperado e não impede a execução.

## Execução

```bash
./game
```

## Detalhes técnicos

- **OpenGL 3.3 Core Profile**, com shaders GLSL embutidos no código (`VERT_SRC` / `FRAG_SRC`).
- Projeção **ortográfica** com origem no canto superior-esquerdo (Y cresce para baixo).
- Render dos tiles via **painter's algorithm** (linhas/colunas crescentes desenham do fundo para a frente).
- Um único *quad* (VAO/VBO) é reutilizado para todos os sprites, variando os *uniforms* `uPos`, `uSize` e `uTile`.
- Recorte do tileset por `uTile` / `uNumTiles` diretamente no vertex shader.
