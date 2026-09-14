// ============================================================================
//  TexturaGenerada.cpp - generador de texturas de prueba (ver TexturaGenerada.h).
//  Todo dibujado a mano sobre el buffer: damero, lineas, cruces y un alfabeto de 5x7 para los rotulos.
// ============================================================================
#include "io/TexturaGenerada.h"
#include "io/TexturaEditada.h"     // TexEditCrear: la textura nueva vive en memoria hasta que se guarda
#include "objects/Textures.h"
#include "ui/ViewPorts/UVEditor.h" // UVSetTexProyecto: el UV editor la muestra
#include <stdio.h>
#include <string.h>
#include <math.h>

static const char* kTipoNombres[TexGenTipos] = { "Blank", "UV Grid", "Color Grid" };
const char* TexGenTipoNombre(int tipo) { return (tipo >= 0 && tipo < TexGenTipos) ? kTipoNombres[tipo] : ""; }

// ---------------------------------------------------------------------------
//  alfabeto 5x7 de los rotulos: A..Z (filas) y 0..9 (columnas). '#' = pixel encendido
// ---------------------------------------------------------------------------
static const char* kGlifos[36][7] = {
    { " ### ", "#   #", "#   #", "#####", "#   #", "#   #", "#   #" },  // A
    { "#### ", "#   #", "#   #", "#### ", "#   #", "#   #", "#### " },  // B
    { " ### ", "#   #", "#    ", "#    ", "#    ", "#   #", " ### " },  // C
    { "#### ", "#   #", "#   #", "#   #", "#   #", "#   #", "#### " },  // D
    { "#####", "#    ", "#    ", "#### ", "#    ", "#    ", "#####" },  // E
    { "#####", "#    ", "#    ", "#### ", "#    ", "#    ", "#    " },  // F
    { " ### ", "#   #", "#    ", "# ###", "#   #", "#   #", " ### " },  // G
    { "#   #", "#   #", "#   #", "#####", "#   #", "#   #", "#   #" },  // H
    { " ### ", "  #  ", "  #  ", "  #  ", "  #  ", "  #  ", " ### " },  // I
    { "  ###", "   # ", "   # ", "   # ", "   # ", "#  # ", " ##  " },  // J
    { "#   #", "#  # ", "# #  ", "##   ", "# #  ", "#  # ", "#   #" },  // K
    { "#    ", "#    ", "#    ", "#    ", "#    ", "#    ", "#####" },  // L
    { "#   #", "## ##", "# # #", "# # #", "#   #", "#   #", "#   #" },  // M
    { "#   #", "##  #", "# # #", "#  ##", "#   #", "#   #", "#   #" },  // N
    { " ### ", "#   #", "#   #", "#   #", "#   #", "#   #", " ### " },  // O
    { "#### ", "#   #", "#   #", "#### ", "#    ", "#    ", "#    " },  // P
    { " ### ", "#   #", "#   #", "#   #", "# # #", "#  # ", " ## #" },  // Q
    { "#### ", "#   #", "#   #", "#### ", "# #  ", "#  # ", "#   #" },  // R
    { " ####", "#    ", "#    ", " ### ", "    #", "    #", "#### " },  // S
    { "#####", "  #  ", "  #  ", "  #  ", "  #  ", "  #  ", "  #  " },  // T
    { "#   #", "#   #", "#   #", "#   #", "#   #", "#   #", " ### " },  // U
    { "#   #", "#   #", "#   #", "#   #", "#   #", " # # ", "  #  " },  // V
    { "#   #", "#   #", "#   #", "# # #", "# # #", "## ##", "#   #" },  // W
    { "#   #", "#   #", " # # ", "  #  ", " # # ", "#   #", "#   #" },  // X
    { "#   #", "#   #", " # # ", "  #  ", "  #  ", "  #  ", "  #  " },  // Y
    { "#####", "    #", "   # ", "  #  ", " #   ", "#    ", "#####" },  // Z
    { " ### ", "#   #", "#  ##", "# # #", "##  #", "#   #", " ### " },  // 0
    { "  #  ", " ##  ", "  #  ", "  #  ", "  #  ", "  #  ", " ### " },  // 1
    { " ### ", "#   #", "    #", "   # ", "  #  ", " #   ", "#####" },  // 2
    { " ### ", "#   #", "    #", "  ## ", "    #", "#   #", " ### " },  // 3
    { "   # ", "  ## ", " # # ", "#  # ", "#####", "   # ", "   # " },  // 4
    { "#####", "#    ", "#### ", "    #", "    #", "#   #", " ### " },  // 5
    { " ### ", "#    ", "#    ", "#### ", "#   #", "#   #", " ### " },  // 6
    { "#####", "    #", "   # ", "  #  ", "  #  ", "  #  ", "  #  " },  // 7
    { " ### ", "#   #", "#   #", " ### ", "#   #", "#   #", " ### " },  // 8
    { " ### ", "#   #", "#   #", " ####", "    #", "    #", " ### " },  // 9
};
static int GlifoDe(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= '0' && c <= '9') return 26 + (c - '0');
    return -1;
}
// el rotulo de la celda (fila, columna): letras estilo planilla (A..Z, AA, AB...) + numero de columna desde 1
static std::string RotuloCelda(int fila, int col) {
    std::string letras; int f = fila;
    do { letras.insert(letras.begin(), (char)('A' + f % 26)); f = f / 26 - 1; } while (f >= 0);
    char num[16]; sprintf(num, "%d", col + 1);
    return letras + num;
}
// pixel (ux,uy) en unidades de glifo del rotulo (cada glifo 5 de ancho + 1 de gap, 7 de alto)
static bool RotuloPixel(const std::string& rot, int ux, int uy) {
    if (uy < 0 || uy >= 7 || ux < 0) return false;
    const int g = ux / 6, gx = ux % 6;
    if (g >= (int)rot.size() || gx >= 5) return false;
    const int gl = GlifoDe(rot[(size_t)g]);
    return gl >= 0 && kGlifos[gl][uy][gx] == '#';
}

static void HSVaRGB(float h, float s, float v, unsigned char* rgb) {
    h -= floorf(h); h *= 6.0f;
    const int i = (int)h; const float f = h - i;
    const float p = v * (1 - s), q = v * (1 - s * f), t = v * (1 - s * (1 - f));
    float r, g, b;
    switch (i % 6) {
        case 0: r = v; g = t; b = p; break; case 1: r = q; g = v; b = p; break; case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break; case 4: r = t; g = p; b = v; break; default: r = v; g = p; b = q; break;
    }
    rgb[0] = (unsigned char)(r * 255.0f + 0.5f); rgb[1] = (unsigned char)(g * 255.0f + 0.5f); rgb[2] = (unsigned char)(b * 255.0f + 0.5f);
}
static inline unsigned char Clamp255(int v) { return (unsigned char)(v < 0 ? 0 : v > 255 ? 255 : v); }

// lado de la celda en pixeles: FIJO (64 px) para que las celdas sean CUADRADAS aunque la imagen sea rectangular y
// para que una textura mas grande tenga mas celdas (y una chica, menos). Imagenes chicas: 4 celdas por lado.
int W3dTexGenCelda(int w, int h) {
    const int menor = (w < h) ? w : h;
    if (menor >= 256) return 64;
    int c = menor / 4; if (c < 8) c = 8;
    return c;
}

void W3dGenerarTextura(int tipo, int w, int h, const float* color, bool alpha, std::vector<unsigned char>& out) {
    if (w < 1) w = 1; if (h < 1) h = 1;
    out.assign((size_t)w * h * 4, 255);
    if (tipo == TexGenBlank) {
        const unsigned char r = Clamp255((int)(color[0] * 255.0f + 0.5f)), g = Clamp255((int)(color[1] * 255.0f + 0.5f));
        const unsigned char b = Clamp255((int)(color[2] * 255.0f + 0.5f)), a = alpha ? Clamp255((int)(color[3] * 255.0f + 0.5f)) : 255;
        for (size_t i = 0; i < (size_t)w * h; i++) { out[i*4] = r; out[i*4+1] = g; out[i*4+2] = b; out[i*4+3] = a; }
        return;
    }
    const int celda = W3dTexGenCelda(w, h);
    const int nx = (w + celda - 1) / celda;                 // columnas de celdas
    const int ny = (h + celda - 1) / celda;                 // filas de celdas (la ultima puede quedar cortada)
    const int arm = celda / 6, grosor = celda / 64;         // la cruz de la grilla UV
    const int k = (celda / 40 > 1) ? celda / 40 : 1;        // escala del rotulo (glifos de 5x7 unidades)
    const int sub = (celda >= 16) ? celda / 4 : 0;          // subgrilla de la grilla de colores
    std::string rot; int rotFila = -1, rotCol = -1;
    for (int y = 0; y < h; y++) {
        const int j = y / celda;
        for (int x = 0; x < w; x++) {
            const int i = x / celda;
            unsigned char* p = &out[((size_t)y * w + x) * 4];
            const bool borde = (x % celda == 0) || (y % celda == 0);
            if (tipo == TexGenUVGrid) {
                const int base = ((i + j) & 1) ? 150 : 72;
                unsigned char v = Clamp255(borde ? base + 45 : base);
                p[0] = p[1] = p[2] = v; p[3] = 255;
                // cruz de color en el centro: el tono va corriendo por la diagonal (delata espejados y giros)
                const int cx = i * celda + celda / 2, cy = j * celda + celda / 2;
                const int dx = x - cx, dy = y - cy;
                const bool enCruz = (dx >= -grosor && dx <= grosor && dy >= -arm && dy <= arm) ||
                                    (dy >= -grosor && dy <= grosor && dx >= -arm && dx <= arm);
                if (enCruz && celda >= 12) HSVaRGB((float)((i * 5 + j * 3) % 12) / 12.0f, 0.9f, 1.0f, p);
            } else {
                const int fila = ny - 1 - j;                // A abajo, hacia arriba
                // el TONO recorre todo el ancho (una vuelta completa) y la CLARIDAD toda la altura (abajo oscuro,
                // arriba claro): cada celda tiene su color, sin repetirse
                const float hue = (float)i / (float)nx;
                const float val = (ny > 1) ? 0.28f + 0.62f * (float)fila / (float)(ny - 1) : 0.6f;
                unsigned char rgb[3]; HSVaRGB(hue, 0.78f, val, rgb);
                int r = rgb[0], g = rgb[1], b = rgb[2];
                // sub-cuadros 4x4 adentro de la celda (damero suave): se ve el estirado dentro de cada celda
                if (sub) { const int sx = (x % celda) / sub, sy = (y % celda) / sub; const int d = ((sx + sy) & 1) ? 9 : -9; r += d; g += d; b += d; }
                // lineas NEGRAS entre celdas; cada 4 celdas (desde abajo y desde la izquierda) mas gruesa
                const bool gruesa = ((x % celda) == 1 && (i % 4) == 0) || ((y % celda) == 1 && ((ny - j) % 4) == 0);
                if (borde || gruesa) { r = g = b = 18; }
                // rotulo "A1", "B7", "AC12"... centrado en la celda: blanco con contorno oscuro
                if (celda >= 24) {
                    if (fila != rotFila || i != rotCol) { rot = RotuloCelda(fila, i); rotFila = fila; rotCol = i; }
                    const int anchoU = (int)rot.size() * 6 - 1;
                    const int rx0 = i * celda + (celda - anchoU * k) / 2, ry0 = j * celda + (celda - 7 * k) / 2;
                    const int ux = (x - rx0) / k, uy = (y - ry0) / k;
                    if (x >= rx0 && y >= ry0 && RotuloPixel(rot, ux, uy)) { r = g = b = 250; }
                    else {
                        bool contorno = false;
                        for (int oy = -1; oy <= 1 && !contorno; oy++) for (int ox = -1; ox <= 1 && !contorno; ox++) {
                            const int vx = (x + ox * k - rx0), vy = (y + oy * k - ry0);
                            if (vx >= 0 && vy >= 0 && RotuloPixel(rot, vx / k, vy / k)) contorno = true;
                        }
                        if (contorno) { r = g = b = 30; }
                    }
                }
                p[0] = Clamp255(r); p[1] = Clamp255(g); p[2] = Clamp255(b); p[3] = 255;
            }
        }
    }
}


std::string W3dCrearTexturaProyecto(const std::string& nombre, int tipo, int w, int h, const float* color, bool alpha, std::string& msg) {
    if (w < 1 || h < 1 || w > 8192 || h > 8192) { msg = "New Texture: invalid size"; return std::string(); }
    std::vector<unsigned char> px; W3dGenerarTextura(tipo, w, h, color, alpha, px);
    // en MEMORIA (con "*" hasta que se guarde): no se asigna a ningun material, solo se muestra en el UV editor
    Texture* t = TexEditCrear(nombre, px, w, h, alpha);
    if (!t) { msg = "New Texture: could not load"; return std::string(); }
    UVSetTexProyecto(t->path);
    msg.clear();
    return t->path;
}

int W3dTexturasGeneradasLimpiar() { return 0; }   // (ya no se escribe nada al crear: viven en memoria)
