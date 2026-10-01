// ============================================================================
//  RecursosDatos.cpp — los encabezados de imagenes y sonidos (ver el .h)
// ============================================================================
#include "io/RecursosDatos.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#ifndef W3D_SYMBIAN
    #include "stb/stb_image.h"   // (la implementacion la compila el editor: constructor.cpp)
#endif

static unsigned LeU32BE(const unsigned char* p) { return ((unsigned)p[0] << 24) | ((unsigned)p[1] << 16) | ((unsigned)p[2] << 8) | p[3]; }
static unsigned LeU32LE(const unsigned char* p) { return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24); }
static unsigned LeU16LE(const unsigned char* p) { return (unsigned)p[0] | ((unsigned)p[1] << 8); }
static unsigned LeU16BE(const unsigned char* p) { return ((unsigned)p[0] << 8) | p[1]; }

// ============================================================================
//  IMAGENES
// ============================================================================
// PNG: IHDR (ancho, alto, tipo de color) y un tRNS (transparencia sin canal alfa)
static bool InfoPNG(const unsigned char* b, size_t n, W3dImagenDatos& o) {
    static const unsigned char kFirma[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
    if (n < 33 || memcmp(b, kFirma, 8) != 0) return false;
    o.formato = "PNG";
    o.ancho = (int)LeU32BE(b + 16);
    o.alto = (int)LeU32BE(b + 20);
    const int tipo = b[25];   // 0 gris, 2 RGB, 3 paleta, 4 gris+alfa, 6 RGBA
    o.canales = (tipo == 0) ? 1 : (tipo == 2) ? 3 : (tipo == 3) ? 3 : (tipo == 4) ? 2 : (tipo == 6) ? 4 : 0;
    o.alfa = (tipo == 4 || tipo == 6);
    // un tRNS antes del primer IDAT = transparencia por paleta o por color clave
    size_t p = 8;
    while (p + 8 <= n) {
        const unsigned len = LeU32BE(b + p);
        if (memcmp(b + p + 4, "tRNS", 4) == 0) { o.alfa = true; break; }
        if (memcmp(b + p + 4, "IDAT", 4) == 0 || memcmp(b + p + 4, "IEND", 4) == 0) break;
        if (len > n) break;
        p += 12 + (size_t)len;
    }
    return true;
}
// JPEG: el primer SOFn trae alto y ancho (nunca tiene alfa)
static bool InfoJPEG(const unsigned char* b, size_t n, W3dImagenDatos& o) {
    if (n < 4 || b[0] != 0xFF || b[1] != 0xD8) return false;
    o.formato = "JPEG";
    size_t p = 2;
    while (p + 4 <= n) {
        if (b[p] != 0xFF) { p++; continue; }
        const unsigned char m = b[p + 1];
        if (m == 0xD8 || m == 0x01 || (m >= 0xD0 && m <= 0xD7)) { p += 2; continue; }
        const unsigned len = LeU16BE(b + p + 2);
        if (m >= 0xC0 && m <= 0xCF && m != 0xC4 && m != 0xC8 && m != 0xCC && p + 9 < n) {
            o.alto = (int)LeU16BE(b + p + 5);
            o.ancho = (int)LeU16BE(b + p + 7);
            o.canales = b[p + 9];
            break;
        }
        p += 2 + len;
    }
    o.alfa = false;
    o.alfaTipo = 0;
    return true;
}
static bool InfoGIF(const unsigned char* b, size_t n, W3dImagenDatos& o) {
    if (n < 10 || memcmp(b, "GIF8", 4) != 0) return false;
    o.formato = "GIF";
    o.ancho = (int)LeU16LE(b + 6);
    o.alto = (int)LeU16LE(b + 8);
    o.canales = 3;
    return true;
}
static bool InfoBMP(const unsigned char* b, size_t n, W3dImagenDatos& o) {
    if (n < 30 || b[0] != 'B' || b[1] != 'M') return false;
    o.formato = "BMP";
    o.ancho = (int)LeU32LE(b + 18);
    int h = (int)LeU32LE(b + 22);
    o.alto = h < 0 ? -h : h;
    const int bpp = (int)LeU16LE(b + 28);
    o.canales = bpp / 8;
    o.alfa = (bpp == 32);
    return true;
}
static bool InfoWEBP(const unsigned char* b, size_t n, W3dImagenDatos& o) {
    if (n < 30 || memcmp(b, "RIFF", 4) != 0 || memcmp(b + 8, "WEBP", 4) != 0) return false;
    o.formato = "WEBP";
    if (memcmp(b + 12, "VP8X", 4) == 0) {
        o.alfa = (b[20] & 0x10) != 0;
        o.ancho = 1 + (int)(b[24] | (b[25] << 8) | (b[26] << 16));
        o.alto = 1 + (int)(b[27] | (b[28] << 8) | (b[29] << 16));
    } else if (memcmp(b + 12, "VP8L", 4) == 0 && n >= 25) {
        const unsigned v = LeU32LE(b + 21);
        o.ancho = 1 + (int)(v & 0x3FFF);
        o.alto = 1 + (int)((v >> 14) & 0x3FFF);
        o.alfa = ((v >> 28) & 1) != 0;
    } else if (memcmp(b + 12, "VP8 ", 4) == 0 && n >= 30) {
        o.ancho = (int)(LeU16LE(b + 26) & 0x3FFF);
        o.alto = (int)(LeU16LE(b + 28) & 0x3FFF);
    }
    o.canales = o.alfa ? 4 : 3;
    return true;
}
static bool InfoTGA(const unsigned char* b, size_t n, W3dImagenDatos& o) {
    if (n < 18) return false;
    const int tipo = b[2];
    if (!(tipo == 2 || tipo == 3 || tipo == 10 || tipo == 11)) return false;
    o.formato = "TGA";
    o.ancho = (int)LeU16LE(b + 12);
    o.alto = (int)LeU16LE(b + 14);
    const int bpp = b[16];
    o.canales = bpp / 8;
    o.alfa = (bpp == 32) || ((b[17] & 0x0F) != 0);
    return true;
}

bool W3dImagenInfo(const unsigned char* b, size_t n, W3dImagenDatos& out) {
    out = W3dImagenDatos();
    if (!b || n == 0) return false;
    if (!InfoPNG(b, n, out) && !InfoJPEG(b, n, out) && !InfoGIF(b, n, out) &&
        !InfoBMP(b, n, out) && !InfoWEBP(b, n, out) && !InfoTGA(b, n, out)) return false;
    if (!out.alfa) { out.alfaTipo = 0; return true; }
#ifndef W3D_SYMBIAN
    // el TIPO de alfa sale de los pixeles: binario (recorte: solo 0 y 255) o con transparencias
    if (out.formato != "WEBP") {
        int w = 0, h = 0, ch = 0;
        unsigned char* px = stbi_load_from_memory(b, (int)n, &w, &h, &ch, 4);
        if (px) {
            bool intermedio = false, algunoTransparente = false;
            const long total = (long)w * (long)h;
            for (long i = 0; i < total; i++) {
                const unsigned char a = px[i * 4 + 3];
                if (a != 255) algunoTransparente = true;
                if (a != 0 && a != 255) { intermedio = true; break; }
            }
            stbi_image_free(px);
            out.alfaTipo = intermedio ? 2 : (algunoTransparente ? 1 : 0);
        }
    }
#endif
    return true;
}

// ============================================================================
//  SONIDOS
// ============================================================================
// WAV: los chunks 'fmt ' (formato, canales, frecuencia, bits), 'data' (la duracion) y 'smpl' (el loop)
static bool InfoWAV(const unsigned char* b, size_t n, W3dSonidoDatos& o) {
    if (n < 12 || memcmp(b, "RIFF", 4) != 0 || memcmp(b + 8, "WAVE", 4) != 0) return false;
    o.formato = "WAV";
    unsigned bytesPorSeg = 0, datos = 0;
    bool hayDatos = false;
    size_t p = 12;
    while (p + 8 <= n) {
        const unsigned len = LeU32LE(b + p + 4);
        const unsigned char* c = b + p + 8;
        if (memcmp(b + p, "fmt ", 4) == 0 && p + 8 + 16 <= n) {
            o.canales = (int)LeU16LE(c + 2);
            o.frecuencia = (int)LeU32LE(c + 4);
            bytesPorSeg = LeU32LE(c + 8);
            o.bits = (int)LeU16LE(c + 14);
        } else if (memcmp(b + p, "data", 4) == 0) {
            datos = len;
            hayDatos = true;
        } else if (memcmp(b + p, "smpl", 4) == 0 && p + 8 + 36 <= n) {
            o.loop = LeU32LE(c + 28) > 0;   // cantidad de bucles
        }
        if (len > n) break;
        p += 8 + (size_t)len + (len & 1u);
    }
    if (hayDatos && bytesPorSeg > 0) o.duracion = (float)datos / (float)bytesPorSeg;
    return true;
}
// OGG VORBIS: la cabecera de identificacion (canales, frecuencia) y la posicion del ULTIMO page
// (granule = muestras) para la duracion
static bool InfoOGG(const unsigned char* b, size_t n, W3dSonidoDatos& o) {
    if (n < 64 || memcmp(b, "OggS", 4) != 0) return false;
    o.formato = "OGG";
    for (size_t p = 0; p + 16 <= n && p < 512; p++)
        if (b[p] == 1 && memcmp(b + p + 1, "vorbis", 6) == 0 && p + 16 < n) {
            o.canales = b[p + 11];
            o.frecuencia = (int)LeU32LE(b + p + 12);
            break;
        }
    // el ultimo "OggS" del archivo: su granule position (64 bits, nos alcanzan los 32 de abajo)
    for (size_t p = n - 14; p > 0; p--) {
        if (memcmp(b + p, "OggS", 4) != 0) continue;
        const unsigned lo = LeU32LE(b + p + 6), hi = LeU32LE(b + p + 10);
        const double muestras = (double)lo + (double)hi * 4294967296.0;
        if (o.frecuencia > 0 && muestras > 0) o.duracion = (float)(muestras / (double)o.frecuencia);
        break;
    }
    return true;
}
static bool InfoMP3(const unsigned char* b, size_t n, W3dSonidoDatos& o) {
    if (n < 4) return false;
    if (memcmp(b, "ID3", 3) == 0 || (b[0] == 0xFF && (b[1] & 0xE0) == 0xE0)) { o.formato = "MP3"; return true; }
    return false;
}

bool W3dSonidoInfo(const unsigned char* b, size_t n, W3dSonidoDatos& out) {
    out = W3dSonidoDatos();
    if (!b || n == 0) return false;
    return InfoWAV(b, n, out) || InfoOGG(b, n, out) || InfoMP3(b, n, out);
}

// ============================================================================
//  TEXTOS
// ============================================================================
std::string W3dBytesTexto(double bytes) {
    char b[48];
    if (bytes < 1024.0) snprintf(b, sizeof(b), "%.0f B", bytes);
    else if (bytes < 1024.0 * 1024.0) snprintf(b, sizeof(b), "%.1f KB", bytes / 1024.0);
    else snprintf(b, sizeof(b), "%.2f MB", bytes / (1024.0 * 1024.0));
    return std::string(b);
}
std::string W3dDuracionTexto(float s) {
    char b[48];
    if (s < 0) return std::string("?");
    if (s < 60.0f) snprintf(b, sizeof(b), "%.2f s", s);
    else {
        const int m = (int)(s / 60.0f);
        snprintf(b, sizeof(b), "%d:%04.1f", m, s - (float)m * 60.0f);
    }
    return std::string(b);
}
