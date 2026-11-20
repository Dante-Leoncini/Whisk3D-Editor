// ============================================================================
//  W3dAac.cpp - el codificador AAC-LC propio (ver el .h). La sintaxis del flujo es la de ISO/IEC 14496-3 (raw_data_block
//  con SCE / CPE y END); las tablas Huffman, las de la norma (W3dAacTablas.h).
// ============================================================================
#include "W3dAac.h"
#include "W3dAacTablas.h"
#include <cmath>
#include <cstring>

namespace {

const float kPi = 3.14159265358979f;
const int kM = 1024;         // coeficientes por cuadro
const int kN = 2048;         // largo de la ventana
const int kH = 512;          // la FFT compleja de la MDCT

// ---- escribir de a bits (o solo contarlos: 'out' NULL) ----
struct Bits {
    std::vector<unsigned char>* out;
    unsigned int acum; int nAcum; long total;
    explicit Bits(std::vector<unsigned char>* o) : out(o), acum(0), nAcum(0), total(0) {}
    void Poner(unsigned int v, int n) {
        total += n;
        if (!out) return;
        for (int i = n - 1; i >= 0; i--) {
            acum = (acum << 1) | ((v >> i) & 1u); nAcum++;
            if (nAcum == 8) { out->push_back((unsigned char)acum); acum = 0; nAcum = 0; }
        }
    }
    void Alinear() { const int r = (int)(total & 7); if (r) Poner(0, 8 - r); }
};

// los libros: el codigo y el largo de un indice; cuantos valores agrupa; con signo o no; el valor mas grande
struct Libro { const unsigned short* cod; const unsigned char* bits; int dim; bool conSigno; int lav; };
const Libro kLibros[12] = {
    { 0, 0, 0, false, 0 },
    { kAacCod1, kAacBits1, 4, true, 1 },   { kAacCod2, kAacBits2, 4, true, 1 },
    { kAacCod3, kAacBits3, 4, false, 2 },  { kAacCod4, kAacBits4, 4, false, 2 },
    { kAacCod5, kAacBits5, 2, true, 4 },   { kAacCod6, kAacBits6, 2, true, 4 },
    { kAacCod7, kAacBits7, 2, false, 7 },  { kAacCod8, kAacBits8, 2, false, 7 },
    { kAacCod9, kAacBits9, 2, false, 12 }, { kAacCod10, kAacBits10, 2, false, 12 },
    { kAacCod11, kAacBits11, 2, false, 16 },
};

inline int Abs(int v) { return v < 0 ? -v : v; }

// el escape del libro 11 (|v| >= 16): N-4 unos, un cero y |v| - 2^N en N bits
void Escape(Bits& b, int v) {
    int n = 4;
    while ((1 << (n + 1)) <= v) n++;
    for (int i = 0; i < n - 4; i++) b.Poner(1, 1);
    b.Poner(0, 1);
    b.Poner((unsigned)(v - (1 << n)), n);
}
int BitsEscape(int v) { int n = 4; while ((1 << (n + 1)) <= v) n++; return (n - 4) + 1 + n; }

// una banda [ini, fin) con el libro 'l': escribe (o cuenta) sus bits
long BandaBits(const int* q, int ini, int fin, int l, Bits* b) {
    const Libro& L = kLibros[l];
    long bits = 0;
    for (int i = ini; i < fin; i += L.dim) {
        int idx = 0, signos = 0, nSignos = 0;
        if (L.dim == 4) {
            if (L.conSigno) idx = 27 * (q[i] + 1) + 9 * (q[i + 1] + 1) + 3 * (q[i + 2] + 1) + (q[i + 3] + 1);
            else idx = 27 * Abs(q[i]) + 9 * Abs(q[i + 1]) + 3 * Abs(q[i + 2]) + Abs(q[i + 3]);
        } else if (L.conSigno) {
            idx = 9 * (q[i] + 4) + (q[i + 1] + 4);
        } else {
            const int a = Abs(q[i]), c = Abs(q[i + 1]);
            if (l == 11) idx = 17 * (a > 16 ? 16 : a) + (c > 16 ? 16 : c);
            else idx = (l <= 8 ? 8 : 13) * a + c;
        }
        if (!L.conSigno)
            for (int k = 0; k < L.dim; k++) if (q[i + k]) { signos = (signos << 1) | (q[i + k] < 0 ? 1 : 0); nSignos++; }
        bits += L.bits[idx] + nSignos;
        if (b) { b->Poner(L.cod[idx], L.bits[idx]); if (nSignos) b->Poner((unsigned)signos, nSignos); }
        if (l == 11) for (int k = 0; k < 2; k++) {
            const int a = Abs(q[i + k]);
            if (a >= 16) { bits += BitsEscape(a); if (b) Escape(*b, a); }
        }
    }
    return bits;
}

const int kFrecuencias[12] = { 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000 };

// el FFT complejo de kH puntos (in situ, radix 2)
void Fft(float* re, float* im, const std::vector<float>& c, const std::vector<float>& s) {
    for (int i = 1, j = 0; i < kH; i++) {
        int bit = kH >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { float t = re[i]; re[i] = re[j]; re[j] = t; t = im[i]; im[i] = im[j]; im[j] = t; }
    }
    for (int len = 2; len <= kH; len <<= 1) {
        const int paso = kH / len, mitad = len >> 1;
        for (int i = 0; i < kH; i += len)
            for (int k = 0; k < mitad; k++) {
                const float wr = c[k * paso], wi = -s[k * paso];
                const int a = i + k, b = a + mitad;
                const float tr = re[b] * wr - im[b] * wi, ti = re[b] * wi + im[b] * wr;
                re[b] = re[a] - tr; im[b] = im[a] - ti;
                re[a] += tr; im[a] += ti;
            }
    }
}

} // namespace

W3dAacCodificador::W3dAacCodificador()
    : muestras(0), retardo(kM), frec(0), can(0), kbps(0), idxFrec(-1), nBandas(0), maxSfb(0), swb(0), llenas(0) {}

bool W3dAacCodificador::Abrir(int f, int c, int k) {
    error.clear(); asc.clear(); datos.clear(); tamanios.clear(); muestras = 0; llenas = 0;
    idxFrec = -1;
    for (int i = 0; i < 12; i++) if (kFrecuencias[i] == f) idxFrec = i;
    if (idxFrec < 0) { error = "frecuencia que el AAC no tiene"; return false; }
    if (c < 1 || c > 2) { error = "solo 1 o 2 canales"; return false; }
    frec = f; can = c; kbps = k < 16 ? 16 : (k > 320 ? 320 : k);
    switch (idxFrec) {
        case 0: case 1: swb = kAacSwb96; nBandas = 41; break;
        case 2:         swb = kAacSwb64; nBandas = 47; break;
        case 3: case 4: swb = kAacSwb48; nBandas = 49; break;
        case 5:         swb = kAacSwb32; nBandas = 51; break;
        case 6: case 7: swb = kAacSwb24; nBandas = 47; break;
        case 8: case 9: case 10: swb = kAacSwb16; nBandas = 43; break;
        default:        swb = kAacSwb8; nBandas = 40; break;
    }
    // EL ANCHO DE BANDA: lo que el bitrate por canal alcanza a cubrir (lo de arriba se descarta: mejor sin agudos que
    // con agudos rotos)
    {
        const int porCanal = kbps / can;
        float corte = porCanal >= 96 ? 20000.0f : porCanal >= 64 ? 16000.0f : porCanal >= 48 ? 14000.0f :
                      porCanal >= 32 ? 11000.0f : porCanal >= 24 ? 8000.0f : 6000.0f;
        if (corte > f * 0.5f) corte = f * 0.5f;
        const int bin = (int)(corte / (f * 0.5f) * kM);
        maxSfb = 0;
        while (maxSfb < nBandas && swb[maxSfb] < bin) maxSfb++;
    }
    // AudioSpecificConfig: AAC LC (2), la frecuencia, los canales; 1024 por cuadro
    asc.push_back((unsigned char)((2 << 3) | (idxFrec >> 1)));
    asc.push_back((unsigned char)(((idxFrec & 1) << 7) | (can << 3)));
    for (int ch = 0; ch < 2; ch++) { previo[ch].assign(kM, 0.0f); actual[ch].assign(kM, 0.0f); }
    tz.resize(kN); tX.resize(kM); txp.resize(kM); tu.resize(kM); tre.resize(kH); tim.resize(kH);
    tq[0].assign(kM, 0); tq[1].assign(kM, 0); tqs.assign(kM, 0);
    ventana.resize(kN);
    for (int i = 0; i < kN; i++) ventana[i] = (float)sin(kPi / kN * (i + 0.5f));
    cosR.resize(kH); sinR.resize(kH); cosT.resize(kH); sinT.resize(kH); cosF.resize(kH); sinF.resize(kH);
    for (int m = 0; m < kH; m++) {
        cosR[m] = (float)cos(kPi * (m + 0.25f) / kM); sinR[m] = (float)sin(kPi * (m + 0.25f) / kM);
        cosT[m] = (float)cos(kPi * m / kM);          sinT[m] = (float)sin(kPi * m / kM);
        cosF[m] = (float)cos(2.0f * kPi * m / kH);   sinF[m] = (float)sin(2.0f * kPi * m / kH);
    }
    return true;
}

// LA MDCT de 2048 (ya con la ventana) por un FFT de 512: la rotacion a una DCT-IV de 1024 y esa por el FFT. Con el
// factor 2 de la norma (la IMDCT del decodificador divide por N/2)
void W3dAacCodificador::Mdct(const float* x, float* X) {
    float* u = &tu[0]; float* re = &tre[0]; float* im = &tim[0];
    for (int i = 0; i < kM; i++) {
        if (i < kM / 2) u[i] = -x[3 * kN / 4 + i] - x[3 * kN / 4 - 1 - i];
        else            u[i] = x[i - kN / 4] - x[3 * kN / 4 - 1 - i];
    }
    for (int m = 0; m < kH; m++) {
        const float a = u[2 * m], b = u[kM - 1 - 2 * m];
        re[m] = a * cosR[m] + b * sinR[m];
        im[m] = b * cosR[m] - a * sinR[m];
    }
    Fft(re, im, cosF, sinF);
    for (int m = 0; m < kH; m++) {
        const float yr = re[m] * cosT[m] + im[m] * sinT[m];
        const float yi = im[m] * cosT[m] - re[m] * sinT[m];
        X[2 * m] = 2.0f * yr;
        X[kM - 1 - 2 * m] = -2.0f * yi;
    }
}

void W3dAacCodificador::Muestras(const short* pcm, int n) {
    if (idxFrec < 0 || !pcm) return;
    muestras += n;
    for (int i = 0; i < n; i++) {
        for (int ch = 0; ch < can; ch++) actual[ch][llenas] = (float)pcm[i * can + ch];
        if (++llenas == kM) Cuadro();
    }
}

void W3dAacCodificador::Terminar() {
    if (idxFrec < 0) return;
    if (llenas > 0) { for (int ch = 0; ch < can; ch++) for (int i = llenas; i < kM; i++) actual[ch][i] = 0.0f; llenas = kM; Cuadro(); }
    // el ultimo solapamiento: un cuadro de silencio
    for (int ch = 0; ch < can; ch++) actual[ch].assign(kM, 0.0f);
    llenas = kM; Cuadro();
}

// UN CUADRO: la MDCT de cada canal, el scalefactor que entra en el presupuesto, y el raw_data_block
void W3dAacCodificador::Cuadro() {
    llenas = 0;
    int* q[2] = { &tq[0][0], &tq[1][0] };
    int global[2] = { 100, 100 };
    int libro[2][64];
    int maxUsado[2] = { 0, 0 };
    // el presupuesto por canal (bits del cuadro / canales), con lugar para la cabecera de cada canal
    long presupuesto = (long)((double)kbps * 1000.0 * kM / frec / can) - 40;
    if (presupuesto > 6000) presupuesto = 6000;   // (la norma: hasta 6144 bits por canal y cuadro)
    if (presupuesto < 200) presupuesto = 200;
    for (int ch = 0; ch < can; ch++) {
        float* z = &tz[0]; float* X = &tX[0]; float* xp = &txp[0];
        for (int i = 0; i < kM; i++) { z[i] = previo[ch][i] * ventana[i]; z[kM + i] = actual[ch][i] * ventana[kM + i]; }
        Mdct(z, X);
        float maxX = 0.0f;
        const int fin = swb[maxSfb];
        for (int i = 0; i < kM; i++) {
            const float a = (float)fabs(X[i]);
            xp[i] = (i < fin) ? (float)pow(a, 0.75f) : 0.0f;
            if (i < fin && a > maxX) maxX = a;
        }
        // el scalefactor mas fino que entra en el presupuesto (biseccion: mas grande = mas grueso = menos bits). El
        // minimo: que ningun valor pase de 8191
        int lo = 0, hi = 255;
        if (maxX > 0.0f) {
            const float lim = (float)pow(maxX, 0.75f) / 8191.0f;
            while (lo < 255 && (float)pow(2.0, -0.1875 * (lo - 100)) * 1.0f > 1.0f / lim) lo++;
        }
        int elegido = 255;
        int* qs = &tqs[0];
        for (int it = 0; it < 9 && lo <= hi; it++) {
            const int g = (lo + hi) / 2;
            const float esc = (float)pow(2.0, -0.1875 * (g - 100));
            long bits = 0;
            for (int i = 0; i < fin; i++) { int v = (int)(xp[i] * esc + 0.4054f); qs[i] = X[i] < 0 ? -v : v; }
            for (int b = 0; b < maxSfb; b++) {
                int m = 0;
                for (int i = swb[b]; i < swb[b + 1]; i++) { const int a = Abs(qs[i]); if (a > m) m = a; }
                if (m == 0) continue;
                long mejor = 0x7fffffff;
                for (int l = 1; l <= 11; l++) if (m <= kLibros[l].lav || l == 11) {
                    const long c = BandaBits(qs, swb[b], swb[b + 1], l, 0);
                    if (c < mejor) mejor = c;
                }
                bits += mejor + 1 + 9;   // (+ el scalefactor y, a lo sumo, una seccion)
            }
            if (bits <= presupuesto) { elegido = g; hi = g - 1; } else lo = g + 1;
        }
        if (elegido == 255 && lo <= 255) elegido = lo > 255 ? 255 : lo;
        global[ch] = elegido;
        const float esc = (float)pow(2.0, -0.1875 * (elegido - 100));
        for (int i = 0; i < kM; i++) { int v = (i < fin) ? (int)(xp[i] * esc + 0.4054f) : 0; if (v > 8191) v = 8191; q[ch][i] = X[i] < 0 ? -v : v; }
        for (int b = 0; b < maxSfb; b++) {
            int m = 0;
            for (int i = swb[b]; i < swb[b + 1]; i++) { const int a = Abs(q[ch][i]); if (a > m) m = a; }
            if (m == 0) { libro[ch][b] = 0; continue; }
            long mejor = 0x7fffffff; int ml = 11;
            for (int l = 1; l <= 11; l++) if (m <= kLibros[l].lav || l == 11) {
                const long c = BandaBits(q[ch], swb[b], swb[b + 1], l, 0);
                if (c < mejor) { mejor = c; ml = l; }
            }
            libro[ch][b] = ml;
            maxUsado[ch] = b + 1;
        }
        previo[ch] = actual[ch];
    }
    // ---- el raw_data_block ----
    const size_t antes = datos.size();
    Bits bw(&datos);
    if (can == 1) { bw.Poner(0, 3); bw.Poner(0, 4); }            // SCE, etiqueta 0
    else { bw.Poner(1, 3); bw.Poner(0, 4); bw.Poner(0, 1); }     // CPE, etiqueta 0, sin ventana comun
    for (int ch = 0; ch < can; ch++) {
        const int ms = maxUsado[ch];
        bw.Poner((unsigned)global[ch], 8);
        // ics_info: reservado, ONLY_LONG_SEQUENCE, ventana seno, max_sfb, sin prediccion
        bw.Poner(0, 1); bw.Poner(0, 2); bw.Poner(0, 1); bw.Poner((unsigned)ms, 6); bw.Poner(0, 1);
        // section_data: las bandas seguidas con el mismo libro, juntas (largo de 5 bits, 31 = sigue)
        for (int b = 0; b < ms; ) {
            int e = b;
            while (e < ms && libro[ch][e] == libro[ch][b]) e++;
            bw.Poner((unsigned)libro[ch][b], 4);
            int len = e - b;
            while (len >= 31) { bw.Poner(31, 5); len -= 31; }
            bw.Poner((unsigned)len, 5);
            b = e;
        }
        // scale_factor_data: todas las bandas con datos usan el global (diferencia 0 = el indice 60)
        for (int b = 0; b < ms; b++) if (libro[ch][b]) bw.Poner(kAacSfCod[60], kAacSfBits[60]);
        bw.Poner(0, 1); bw.Poner(0, 1); bw.Poner(0, 1);         // sin pulse, sin TNS, sin gain control
        for (int b = 0; b < ms; b++) if (libro[ch][b]) BandaBits(q[ch], swb[b], swb[b + 1], libro[ch][b], &bw);
    }
    bw.Poner(7, 3);   // END
    bw.Alinear();
    tamanios.push_back((unsigned int)(datos.size() - antes));
}

// ---- un WAV entero ----
static unsigned Le32(const unsigned char* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((unsigned)p[3] << 24); }
static unsigned Le16(const unsigned char* p) { return p[0] | (p[1] << 8); }

bool W3dAacDesdeWav(const std::vector<unsigned char>& w, int kbps, W3dAacCodificador& cod, int& frecuencia, int& canales) {
    frecuencia = canales = 0;
    if (w.size() < 44 || memcmp(&w[0], "RIFF", 4) != 0 || memcmp(&w[8], "WAVE", 4) != 0) { cod.error = "no es un WAV"; return false; }
    size_t p = 12, datos = 0, nDatos = 0;
    int bitsMuestra = 0, formato = 0;
    while (p + 8 <= w.size()) {
        const unsigned largo = Le32(&w[p + 4]);
        if (memcmp(&w[p], "fmt ", 4) == 0 && p + 8 + 16 <= w.size()) {
            formato = (int)Le16(&w[p + 8]); canales = (int)Le16(&w[p + 10]);
            frecuencia = (int)Le32(&w[p + 12]); bitsMuestra = (int)Le16(&w[p + 22]);
        } else if (memcmp(&w[p], "data", 4) == 0) {
            datos = p + 8; nDatos = largo;
            if (datos + nDatos > w.size()) nDatos = w.size() - datos;
            break;
        }
        p += 8 + largo + (largo & 1);
    }
    if (formato != 1 || bitsMuestra != 16 || !datos) { cod.error = "el WAV no es PCM de 16 bits"; return false; }
    if (!cod.Abrir(frecuencia, canales, kbps)) return false;
    const int porCuadro = canales;
    const size_t total = nDatos / (2 * porCuadro);
    std::vector<short> buf(4096 * porCuadro);
    for (size_t hecho = 0; hecho < total; ) {
        const size_t n = (total - hecho) > 4096 ? 4096 : (total - hecho);
        const unsigned char* s = &w[datos + hecho * 2 * porCuadro];
        for (size_t i = 0; i < n * porCuadro; i++) buf[i] = (short)Le16(s + i * 2);
        cod.Muestras(&buf[0], (int)n);
        hecho += n;
    }
    cod.Terminar();
    return true;
}
