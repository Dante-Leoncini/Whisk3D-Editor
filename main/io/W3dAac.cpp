// ============================================================================
//  W3dAac.cpp - el codificador AAC-LC propio (ver el .h). La sintaxis del flujo es la de ISO/IEC 14496-3 (raw_data_block
//  con SCE / CPE y END); las tablas Huffman, las de la norma (W3dAacTables.h).
// ============================================================================
#include "W3dAac.h"
#include "W3dAacTables.h"
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
    unsigned int accum; int nAccum; long total;
    explicit Bits(std::vector<unsigned char>* o) : out(o), accum(0), nAccum(0), total(0) {}
    void Set(unsigned int v, int n) {
        total += n;
        if (!out) return;
        for (int i = n - 1; i >= 0; i--) {
            accum = (accum << 1) | ((v >> i) & 1u); nAccum++;
            if (nAccum == 8) { out->push_back((unsigned char)accum); accum = 0; nAccum = 0; }
        }
    }
    void Align() { const int r = (int)(total & 7); if (r) Set(0, 8 - r); }
};

// los libros: el codigo y el largo de un indice; cuantos valores agrupa; con signo o no; el valor mas grande
struct Book { const unsigned short* cod; const unsigned char* bits; int dim; bool withSign; int lav; };
const Book kBooks[12] = {
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
    for (int i = 0; i < n - 4; i++) b.Set(1, 1);
    b.Set(0, 1);
    b.Set((unsigned)(v - (1 << n)), n);
}
int BitsEscape(int v) { int n = 4; while ((1 << (n + 1)) <= v) n++; return (n - 4) + 1 + n; }

// una banda [ini, fin) con el libro 'l': escribe (o cuenta) sus bits
long BandBits(const int* q, int start, int end, int l, Bits* b) {
    const Book& L = kBooks[l];
    long bits = 0;
    for (int i = start; i < end; i += L.dim) {
        int idx = 0, signs = 0, nSigns = 0;
        if (L.dim == 4) {
            if (L.withSign) idx = 27 * (q[i] + 1) + 9 * (q[i + 1] + 1) + 3 * (q[i + 2] + 1) + (q[i + 3] + 1);
            else idx = 27 * Abs(q[i]) + 9 * Abs(q[i + 1]) + 3 * Abs(q[i + 2]) + Abs(q[i + 3]);
        } else if (L.withSign) {
            idx = 9 * (q[i] + 4) + (q[i + 1] + 4);
        } else {
            const int a = Abs(q[i]), c = Abs(q[i + 1]);
            if (l == 11) idx = 17 * (a > 16 ? 16 : a) + (c > 16 ? 16 : c);
            else idx = (l <= 8 ? 8 : 13) * a + c;
        }
        if (!L.withSign)
            for (int k = 0; k < L.dim; k++) if (q[i + k]) { signs = (signs << 1) | (q[i + k] < 0 ? 1 : 0); nSigns++; }
        bits += L.bits[idx] + nSigns;
        if (b) { b->Set(L.cod[idx], L.bits[idx]); if (nSigns) b->Set((unsigned)signs, nSigns); }
        if (l == 11) for (int k = 0; k < 2; k++) {
            const int a = Abs(q[i + k]);
            if (a >= 16) { bits += BitsEscape(a); if (b) Escape(*b, a); }
        }
    }
    return bits;
}

const int kFrequencies[12] = { 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000 };

// el FFT complejo de kH puntos (in situ, radix 2)
void Fft(float* re, float* im, const std::vector<float>& c, const std::vector<float>& s) {
    for (int i = 1, j = 0; i < kH; i++) {
        int bit = kH >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { float t = re[i]; re[i] = re[j]; re[j] = t; t = im[i]; im[i] = im[j]; im[j] = t; }
    }
    for (int len = 2; len <= kH; len <<= 1) {
        const int step = kH / len, half = len >> 1;
        for (int i = 0; i < kH; i += len)
            for (int k = 0; k < half; k++) {
                const float wr = c[k * step], wi = -s[k * step];
                const int a = i + k, b = a + half;
                const float tr = re[b] * wr - im[b] * wi, ti = re[b] * wi + im[b] * wr;
                re[b] = re[a] - tr; im[b] = im[a] - ti;
                re[a] += tr; im[a] += ti;
            }
    }
}

} // namespace

W3dAacEncoder::W3dAacEncoder()
    : samples(0), delay(kM), freq(0), can(0), kbps(0), idxFreq(-1), nBands(0), maxSfb(0), swb(0), full(0) {}

bool W3dAacEncoder::Open(int f, int c, int k) {
    error.clear(); asc.clear(); data.clear(); sizes.clear(); samples = 0; full = 0;
    idxFreq = -1;
    for (int i = 0; i < 12; i++) if (kFrequencies[i] == f) idxFreq = i;
    if (idxFreq < 0) { error = "frecuencia que el AAC no tiene"; return false; }
    if (c < 1 || c > 2) { error = "solo 1 o 2 canales"; return false; }
    freq = f; can = c; kbps = k < 16 ? 16 : (k > 320 ? 320 : k);
    switch (idxFreq) {
        case 0: case 1: swb = kAacSwb96; nBands = 41; break;
        case 2:         swb = kAacSwb64; nBands = 47; break;
        case 3: case 4: swb = kAacSwb48; nBands = 49; break;
        case 5:         swb = kAacSwb32; nBands = 51; break;
        case 6: case 7: swb = kAacSwb24; nBands = 47; break;
        case 8: case 9: case 10: swb = kAacSwb16; nBands = 43; break;
        default:        swb = kAacSwb8; nBands = 40; break;
    }
    // EL ANCHO DE BANDA: lo que el bitrate por canal alcanza a cubrir (lo de arriba se descarta: mejor sin agudos que
    // con agudos rotos)
    {
        const int byChannel = kbps / can;
        float cut = byChannel >= 96 ? 20000.0f : byChannel >= 64 ? 16000.0f : byChannel >= 48 ? 14000.0f :
                      byChannel >= 32 ? 11000.0f : byChannel >= 24 ? 8000.0f : 6000.0f;
        if (cut > f * 0.5f) cut = f * 0.5f;
        const int bin = (int)(cut / (f * 0.5f) * kM);
        maxSfb = 0;
        while (maxSfb < nBands && swb[maxSfb] < bin) maxSfb++;
    }
    // AudioSpecificConfig: AAC LC (2), la frecuencia, los canales; 1024 por cuadro
    asc.push_back((unsigned char)((2 << 3) | (idxFreq >> 1)));
    asc.push_back((unsigned char)(((idxFreq & 1) << 7) | (can << 3)));
    for (int ch = 0; ch < 2; ch++) { previous[ch].assign(kM, 0.0f); current[ch].assign(kM, 0.0f); }
    tz.resize(kN); tX.resize(kM); txp.resize(kM); tu.resize(kM); tre.resize(kH); tim.resize(kH);
    tq[0].assign(kM, 0); tq[1].assign(kM, 0); tqs.assign(kM, 0);
    window.resize(kN);
    for (int i = 0; i < kN; i++) window[i] = (float)sin(kPi / kN * (i + 0.5f));
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
void W3dAacEncoder::Mdct(const float* x, float* X) {
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

void W3dAacEncoder::Samples(const short* pcm, int n) {
    if (idxFreq < 0 || !pcm) return;
    samples += n;
    for (int i = 0; i < n; i++) {
        for (int ch = 0; ch < can; ch++) current[ch][full] = (float)pcm[i * can + ch];
        if (++full == kM) Frame();
    }
}

void W3dAacEncoder::Finish() {
    if (idxFreq < 0) return;
    if (full > 0) { for (int ch = 0; ch < can; ch++) for (int i = full; i < kM; i++) current[ch][i] = 0.0f; full = kM; Frame(); }
    // el ultimo solapamiento: un cuadro de silencio
    for (int ch = 0; ch < can; ch++) current[ch].assign(kM, 0.0f);
    full = kM; Frame();
}

// UN CUADRO: la MDCT de cada canal, el scalefactor que entra en el presupuesto, y el raw_data_block
void W3dAacEncoder::Frame() {
    full = 0;
    int* q[2] = { &tq[0][0], &tq[1][0] };
    int global[2] = { 100, 100 };
    int book[2][64];
    int usedMax[2] = { 0, 0 };
    // el presupuesto por canal (bits del cuadro / canales), con lugar para la cabecera de cada canal
    long budget = (long)((double)kbps * 1000.0 * kM / freq / can) - 40;
    if (budget > 6000) budget = 6000;   // (la norma: hasta 6144 bits por canal y cuadro)
    if (budget < 200) budget = 200;
    for (int ch = 0; ch < can; ch++) {
        float* z = &tz[0]; float* X = &tX[0]; float* xp = &txp[0];
        for (int i = 0; i < kM; i++) { z[i] = previous[ch][i] * window[i]; z[kM + i] = current[ch][i] * window[kM + i]; }
        Mdct(z, X);
        float maxX = 0.0f;
        const int end = swb[maxSfb];
        for (int i = 0; i < kM; i++) {
            const float a = (float)fabs(X[i]);
            xp[i] = (i < end) ? (float)pow(a, 0.75f) : 0.0f;
            if (i < end && a > maxX) maxX = a;
        }
        // el scalefactor mas fino que entra en el presupuesto (biseccion: mas grande = mas grueso = menos bits). El
        // minimo: que ningun valor pase de 8191
        int lo = 0, hi = 255;
        if (maxX > 0.0f) {
            const float lim = (float)pow(maxX, 0.75f) / 8191.0f;
            while (lo < 255 && (float)pow(2.0, -0.1875 * (lo - 100)) * 1.0f > 1.0f / lim) lo++;
        }
        int picked = 255;
        int* qs = &tqs[0];
        for (int it = 0; it < 9 && lo <= hi; it++) {
            const int g = (lo + hi) / 2;
            const float esc = (float)pow(2.0, -0.1875 * (g - 100));
            long bits = 0;
            for (int i = 0; i < end; i++) { int v = (int)(xp[i] * esc + 0.4054f); qs[i] = X[i] < 0 ? -v : v; }
            for (int b = 0; b < maxSfb; b++) {
                int m = 0;
                for (int i = swb[b]; i < swb[b + 1]; i++) { const int a = Abs(qs[i]); if (a > m) m = a; }
                if (m == 0) continue;
                long best = 0x7fffffff;
                for (int l = 1; l <= 11; l++) if (m <= kBooks[l].lav || l == 11) {
                    const long c = BandBits(qs, swb[b], swb[b + 1], l, 0);
                    if (c < best) best = c;
                }
                bits += best + 1 + 9;   // (+ el scalefactor y, a lo sumo, una seccion)
            }
            if (bits <= budget) { picked = g; hi = g - 1; } else lo = g + 1;
        }
        if (picked == 255 && lo <= 255) picked = lo > 255 ? 255 : lo;
        global[ch] = picked;
        const float esc = (float)pow(2.0, -0.1875 * (picked - 100));
        for (int i = 0; i < kM; i++) { int v = (i < end) ? (int)(xp[i] * esc + 0.4054f) : 0; if (v > 8191) v = 8191; q[ch][i] = X[i] < 0 ? -v : v; }
        for (int b = 0; b < maxSfb; b++) {
            int m = 0;
            for (int i = swb[b]; i < swb[b + 1]; i++) { const int a = Abs(q[ch][i]); if (a > m) m = a; }
            if (m == 0) { book[ch][b] = 0; continue; }
            long best = 0x7fffffff; int ml = 11;
            for (int l = 1; l <= 11; l++) if (m <= kBooks[l].lav || l == 11) {
                const long c = BandBits(q[ch], swb[b], swb[b + 1], l, 0);
                if (c < best) { best = c; ml = l; }
            }
            book[ch][b] = ml;
            usedMax[ch] = b + 1;
        }
        previous[ch] = current[ch];
    }
    // ---- el raw_data_block ----
    const size_t prev = data.size();
    Bits bw(&data);
    if (can == 1) { bw.Set(0, 3); bw.Set(0, 4); }          // SCE, etiqueta 0
    else { bw.Set(1, 3); bw.Set(0, 4); bw.Set(0, 1); }     // CPE, etiqueta 0, sin ventana comun
    for (int ch = 0; ch < can; ch++) {
        const int ms = usedMax[ch];
        bw.Set((unsigned)global[ch], 8);
        // ics_info: reservado, ONLY_LONG_SEQUENCE, ventana seno, max_sfb, sin prediccion
        bw.Set(0, 1); bw.Set(0, 2); bw.Set(0, 1); bw.Set((unsigned)ms, 6); bw.Set(0, 1);
        // section_data: las bandas seguidas con el mismo libro, juntas (largo de 5 bits, 31 = sigue)
        for (int b = 0; b < ms; ) {
            int e = b;
            while (e < ms && book[ch][e] == book[ch][b]) e++;
            bw.Set((unsigned)book[ch][b], 4);
            int len = e - b;
            while (len >= 31) { bw.Set(31, 5); len -= 31; }
            bw.Set((unsigned)len, 5);
            b = e;
        }
        // scale_factor_data: todas las bandas con datos usan el global (diferencia 0 = el indice 60)
        for (int b = 0; b < ms; b++) if (book[ch][b]) bw.Set(kAacSfCod[60], kAacSfBits[60]);
        bw.Set(0, 1); bw.Set(0, 1); bw.Set(0, 1);         // sin pulse, sin TNS, sin gain control
        for (int b = 0; b < ms; b++) if (book[ch][b]) BandBits(q[ch], swb[b], swb[b + 1], book[ch][b], &bw);
    }
    bw.Set(7, 3);   // END
    bw.Align();
    sizes.push_back((unsigned int)(data.size() - prev));
}

// ---- un WAV entero ----
static unsigned Le32(const unsigned char* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((unsigned)p[3] << 24); }
static unsigned Le16(const unsigned char* p) { return p[0] | (p[1] << 8); }

bool W3dAacFromWav(const std::vector<unsigned char>& w, int kbps, W3dAacEncoder& cod, int& frequency, int& channels) {
    frequency = channels = 0;
    if (w.size() < 44 || memcmp(&w[0], "RIFF", 4) != 0 || memcmp(&w[8], "WAVE", 4) != 0) { cod.error = "no es un WAV"; return false; }
    size_t p = 12, data = 0, nData = 0;
    int bitsSample = 0, format = 0;
    while (p + 8 <= w.size()) {
        const unsigned length = Le32(&w[p + 4]);
        if (memcmp(&w[p], "fmt ", 4) == 0 && p + 8 + 16 <= w.size()) {
            format = (int)Le16(&w[p + 8]); channels = (int)Le16(&w[p + 10]);
            frequency = (int)Le32(&w[p + 12]); bitsSample = (int)Le16(&w[p + 22]);
        } else if (memcmp(&w[p], "data", 4) == 0) {
            data = p + 8; nData = length;
            if (data + nData > w.size()) nData = w.size() - data;
            break;
        }
        p += 8 + length + (length & 1);
    }
    if (format != 1 || bitsSample != 16 || !data) { cod.error = "el WAV no es PCM de 16 bits"; return false; }
    if (!cod.Open(frequency, channels, kbps)) return false;
    const int perFrame = channels;
    const size_t total = nData / (2 * perFrame);
    std::vector<short> buf(4096 * perFrame);
    for (size_t done = 0; done < total; ) {
        const size_t n = (total - done) > 4096 ? 4096 : (total - done);
        const unsigned char* s = &w[data + done * 2 * perFrame];
        for (size_t i = 0; i < n * perFrame; i++) buf[i] = (short)Le16(s + i * 2);
        cod.Samples(&buf[0], (int)n);
        done += n;
    }
    cod.Finish();
    return true;
}
