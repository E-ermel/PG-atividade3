// Tilemap Isometrico (DIAMOND) - C++ / OpenGL (GLEW + GLFW)
// Pato anda nas 8 vizinhancas; cada tile pisado avanca de cor (0..6).
// Ao virar rosa (6) o tile fica bloqueado. Vence quando todos ficam iguais.

#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

static const int WIN_W = 800;
static const int WIN_H = 600;

static const int NUM_TILES   = 7;   // 0=bege 1=verde 2=preto 3=lava 4=azulclaro 5=azul 6=rosa
static const int CURSOR_TILE = 6;   // rosa = ultima cor / tile bloqueado

int gDuckW = 64;
int gDuckH = 64;

int gTileW = 128;
int gTileH = 64;     // proporcao 2:1

static const float ORIGIN_X = 400.0f;
static const float ORIGIN_Y = 130.0f;

// Mapa inicial (indice do tile em cada celula).
static const int MAP_ROWS = 3;
static const int MAP_COLS = 3;
int gMap[MAP_ROWS][MAP_COLS] = {
    { 1, 1, 4 },
    { 4, 1, 4 },
    { 4, 4, 1 },
};

int  gCurRow = 0;
int  gCurCol = 0;
bool gWon    = false;

static const char* VERT_SRC = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aUV;

uniform mat4 uProj;
uniform vec2 uPos;
uniform vec2 uSize;
uniform int  uTile;
uniform int  uNumTiles;

out vec2 vUV;

void main()
{
    vec2 world = uPos + aPos * uSize;
    gl_Position = uProj * vec4(world, 0.0, 1.0);
    float u = (aUV.x + float(uTile)) / float(uNumTiles);
    vUV = vec2(u, aUV.y);
}
)";

static const char* FRAG_SRC = R"(
#version 330 core
in  vec2 vUV;
out vec4 FragColor;
uniform sampler2D uTex;
void main()
{
    vec4 c = texture(uTex, vUV);
    if (c.a < 0.05) discard;
    FragColor = c;
}
)";

static GLuint compileShader(GLenum type, const char* src)
{
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        std::fprintf(stderr, "Erro compilando shader: %s\n", log);
        std::exit(EXIT_FAILURE);
    }
    return s;
}

static GLuint makeProgram(const char* vs, const char* fs)
{
    GLuint v = compileShader(GL_VERTEX_SHADER, vs);
    GLuint f = compileShader(GL_FRAGMENT_SHADER, fs);
    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(p, sizeof(log), nullptr, log);
        std::fprintf(stderr, "Erro linkando programa: %s\n", log);
        std::exit(EXIT_FAILURE);
    }
    glDeleteShader(v);
    glDeleteShader(f);
    return p;
}

// ortho(0, W, H, 0, -1, 1): origem no canto superior-esquerdo, Y cresce p/ baixo.
static void orthoMatrix(float* m, float l, float r, float b, float t,
                        float n, float f)
{
    for (int i = 0; i < 16; ++i) m[i] = 0.0f;
    m[0]  =  2.0f / (r - l);
    m[5]  =  2.0f / (t - b);
    m[10] = -2.0f / (f - n);
    m[12] = -(r + l) / (r - l);
    m[13] = -(t + b) / (t - b);
    m[14] = -(f + n) / (f - n);
    m[15] =  1.0f;
}

static void uploadTexture(const unsigned char* rgba, int w, int h)
{
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, rgba);
}

// Gera um spritesheet de 7 losangos quando assets/tileset.png nao existe.
static void makeProceduralTileset()
{
    gTileW = 128;
    gTileH = 64;
    const int W = NUM_TILES * gTileW;
    const int H = gTileH;
    std::vector<unsigned char> px(W * H * 4, 0);

    unsigned char col[NUM_TILES][3] = {
        { 225, 210, 160 },  // 0 bege
        {  70, 150,  60 },  // 1 verde
        {  40,  40,  45 },  // 2 preto
        { 230,  90,  30 },  // 3 lava
        { 175, 215, 240 },  // 4 azul-claro
        {  60, 110, 200 },  // 5 azul
        { 235, 120, 180 },  // 6 rosa
    };

    const float cx = gTileW * 0.5f;
    const float cy = gTileH * 0.5f;

    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            int t  = x / gTileW;
            int lx = x % gTileW;
            float d = std::fabs((lx + 0.5f) - cx) / (gTileW * 0.5f)
                    + std::fabs((y  + 0.5f) - cy) / (gTileH * 0.5f);
            int idx = (y * W + x) * 4;
            if (d <= 1.0f) {
                float s = (d > 0.82f) ? 0.6f : 1.0f;
                px[idx + 0] = (unsigned char)(col[t][0] * s);
                px[idx + 1] = (unsigned char)(col[t][1] * s);
                px[idx + 2] = (unsigned char)(col[t][2] * s);
                px[idx + 3] = 255;
            }
        }
    uploadTexture(px.data(), W, H);
}

static GLuint loadTileset(const char* path)
{
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    stbi_set_flip_vertically_on_load(false);
    int w = 0, h = 0, ch = 0;
    unsigned char* data = stbi_load(path, &w, &h, &ch, 4);
    if (data) {
        gTileW = w / NUM_TILES;
        gTileH = h;
        uploadTexture(data, w, h);
        stbi_image_free(data);
    } else {
        makeProceduralTileset();
    }
    return tex;
}

static GLuint loadDuck(const char* path)
{
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    stbi_set_flip_vertically_on_load(false);
    int w = 0, h = 0, ch = 0;
    unsigned char* data = stbi_load(path, &w, &h, &ch, 4);
    if (data) {
        gDuckW = w;
        gDuckH = h;
        uploadTexture(data, w, h);
        stbi_image_free(data);
    }
    return tex;
}

// DIAMOND: (i,j) -> centro do losango na tela.
static void tileCenter(int row, int col, float& sx, float& sy)
{
    sx = ORIGIN_X + (col - row) * (gTileW * 0.5f);
    sy = ORIGIN_Y + (col + row) * (gTileH * 0.5f);
}

// Vitoria: todas as celulas com a mesma cor.
static bool allSameColor()
{
    int first = gMap[0][0];
    for (int i = 0; i < MAP_ROWS; ++i)
        for (int j = 0; j < MAP_COLS; ++j)
            if (gMap[i][j] != first) return false;
    return true;
}

static void checkWin(GLFWwindow* win)
{
    if (!gWon && allSameColor()) {
        gWon = true;
        std::printf("\n*** VOCE VENCEU! Todos os tiles na mesma cor. ***\n\n");
        glfwSetWindowTitle(win, "VOCE VENCEU! - Tilemap Isometrico");
    }
}

// Anda 1 passo na matriz. Bloqueia tile rosa e avanca a cor do tile pisado.
static bool tryMove(GLFWwindow* win, int di, int dj)
{
    int ni = gCurRow + di;
    int nj = gCurCol + dj;
    if (ni < 0 || ni >= MAP_ROWS || nj < 0 || nj >= MAP_COLS)
        return false;
    if (gMap[ni][nj] >= CURSOR_TILE)   // rosa = bloqueado
        return false;

    gCurRow = ni;
    gCurCol = nj;
    ++gMap[ni][nj];
    checkWin(win);
    return true;
}

// WASD = cardeais na tela; Q/E/Z/C = diagonais na tela.
static void keyCallback(GLFWwindow* win, int key, int, int action, int)
{
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;
    switch (key) {
        case GLFW_KEY_W: tryMove(win, -1, -1); break;  // cima
        case GLFW_KEY_S: tryMove(win, +1, +1); break;  // baixo
        case GLFW_KEY_A: tryMove(win, +1, -1); break;  // esquerda
        case GLFW_KEY_D: tryMove(win, -1, +1); break;  // direita
        case GLFW_KEY_Q: tryMove(win,  0, -1); break;  // cima-esq
        case GLFW_KEY_E: tryMove(win, -1,  0); break;  // cima-dir
        case GLFW_KEY_Z: tryMove(win, +1,  0); break;  // baixo-esq
        case GLFW_KEY_C: tryMove(win,  0, +1); break;  // baixo-dir
        case GLFW_KEY_ESCAPE: glfwSetWindowShouldClose(win, GLFW_TRUE); break;
        default: break;
    }
}

int main()
{
    if (!glfwInit()) {
        std::fprintf(stderr, "Falha ao inicializar GLFW\n");
        return EXIT_FAILURE;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* win = glfwCreateWindow(WIN_W, WIN_H,
                        "Tilemap Isometrico - Diamond", nullptr, nullptr);
    if (!win) {
        std::fprintf(stderr, "Falha ao criar janela\n");
        glfwTerminate();
        return EXIT_FAILURE;
    }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);
    glfwSetKeyCallback(win, keyCallback);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::fprintf(stderr, "Falha ao inicializar GLEW\n");
        return EXIT_FAILURE;
    }

    std::printf("Tilemap Isometrico (DIAMOND)\n");
    std::printf("  WASD: cima/baixo/esq/dir   Q/E/Z/C: diagonais   ESC: sair\n");
    std::printf("  Cada tile pisado avanca de cor; rosa (6) bloqueia.\n");
    std::printf("  Vence quando todos os tiles ficam da mesma cor.\n");

    float quad[] = {
        0.f,0.f, 0.f,0.f,   1.f,0.f, 1.f,0.f,   1.f,1.f, 1.f,1.f,
        0.f,0.f, 0.f,0.f,   1.f,1.f, 1.f,1.f,   0.f,1.f, 0.f,1.f,
    };
    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float),
                          (void*)(2*sizeof(float)));
    glEnableVertexAttribArray(1);

    GLuint prog    = makeProgram(VERT_SRC, FRAG_SRC);
    GLuint tex     = loadTileset("assets/tileset.png");
    GLuint duckTex = loadDuck("yellow/individual_animations/idle/png_sequence/duckee_idle1.png");

    glUseProgram(prog);
    GLint uProj     = glGetUniformLocation(prog, "uProj");
    GLint uPos      = glGetUniformLocation(prog, "uPos");
    GLint uSize     = glGetUniformLocation(prog, "uSize");
    GLint uTile     = glGetUniformLocation(prog, "uTile");
    GLint uNumTiles = glGetUniformLocation(prog, "uNumTiles");
    GLint uTex      = glGetUniformLocation(prog, "uTex");

    float proj[16];
    orthoMatrix(proj, 0.f, (float)WIN_W, (float)WIN_H, 0.f, -1.f, 1.f);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUniformMatrix4fv(uProj, 1, GL_FALSE, proj);
    glUniform1i(uNumTiles, NUM_TILES);
    glUniform1i(uTex, 0);

    while (!glfwWindowShouldClose(win)) {
        glClearColor(0.10f, 0.10f, 0.13f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(prog);
        glBindVertexArray(vao);
        glActiveTexture(GL_TEXTURE0);

        // Tiles (painter's algorithm: row/col crescentes desenham do fundo).
        glBindTexture(GL_TEXTURE_2D, tex);
        glUniform1i(uNumTiles, NUM_TILES);
        glUniform2f(uSize, (float)gTileW, (float)gTileH);
        for (int i = 0; i < MAP_ROWS; ++i)
            for (int j = 0; j < MAP_COLS; ++j) {
                float sx, sy;
                tileCenter(i, j, sx, sy);
                glUniform2f(uPos, sx - gTileW * 0.5f, sy - gTileH * 0.5f);
                glUniform1i(uTile, gMap[i][j]);
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }

        // Personagem sobre a celula atual.
        {
            float sx, sy;
            tileCenter(gCurRow, gCurCol, sx, sy);
            float px = sx - gDuckW * 0.5f;
            float py = (sy + gTileH * 0.25f) - gDuckH;

            glBindTexture(GL_TEXTURE_2D, duckTex);
            glUniform1i(uNumTiles, 1);
            glUniform2f(uSize, (float)gDuckW, (float)gDuckH);
            glUniform2f(uPos, px, py);
            glUniform1i(uTile, 0);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        glfwSwapBuffers(win);
        glfwPollEvents();
    }

    glDeleteTextures(1, &duckTex);
    glDeleteTextures(1, &tex);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(prog);
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
