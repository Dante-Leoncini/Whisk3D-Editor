// ============================================================================
//  W3dH264.cpp - el codificador H.264 propio (ver el .h). La sintaxis, la transformada entera, la cuantizacion y la
//  reconstruccion son las de ISO/IEC 14496-10 (perfil Baseline, CAVLC); las tablas de CAVLC, las de la norma
//  (W3dH264Tablas.h).
// ============================================================================
#include "W3dH264.h"
#include "W3dH264Tablas.h"
#include <cstring>
#include <cstdlib>
#include <cmath>

namespace {

// ---- los bits de un RBSP ----
struct Bits {
    std::vector<unsigned char>& v;
    unsigned int acum; int n;
    explicit Bits(std::vector<unsigned char>& o) : v(o), acum(0), n(0) {}
    void Poner(unsigned int x, int k) {
        for (int i = k - 1; i >= 0; i--) {
            acum = (acum << 1) | ((x >> i) & 1u);
            if (++n == 8) { v.push_back((unsigned char)acum); acum = 0; n = 0; }
        }
    }
    void Ue(unsigned int x) {
        const unsigned int y = x + 1;
        int len = 0; while ((y >> len) > 1) len++;
        Poner(0, len); Poner(y, len + 1);
    }
    void Se(int x) { Ue(x > 0 ? (unsigned)(2 * x - 1) : (unsigned)(-2 * x)); }
    void Cierre() { Poner(1, 1); while (n) Poner(0, 1); }   // rbsp_trailing_bits
};

// el NAL: codigo de inicio, la cabecera y el RBSP con los bytes de prevencion (00 00 0x -> 00 00 03 0x)
void Nal(std::vector<unsigned char>& out, int refIdc, int tipo, const std::vector<unsigned char>& rbsp) {
    out.push_back(0); out.push_back(0); out.push_back(0); out.push_back(1);
    out.push_back((unsigned char)((refIdc << 5) | tipo));
    int ceros = 0;
    for (size_t i = 0; i < rbsp.size(); i++) {
        const unsigned char b = rbsp[i];
        if (ceros >= 2 && b <= 3) { out.push_back(3); ceros = 0; }
        out.push_back(b);
        ceros = (b == 0) ? ceros + 1 : 0;
    }
}

const int kMF[6][3] = { {13107, 5243, 8066}, {11916, 4660, 7490}, {10082, 4194, 6554},
                        {9362, 3647, 5825}, {8192, 3355, 5243}, {7282, 2893, 4559} };
const int kV[6][3] = { {10, 16, 13}, {11, 18, 14}, {13, 20, 16}, {14, 23, 18}, {16, 25, 20}, {18, 29, 23} };
const int kZig[16] = { 0, 1, 4, 8, 5, 2, 3, 6, 9, 12, 13, 10, 7, 11, 14, 15 };   // (raster y*4+x)
inline int Clase(int pos) { const int x = pos & 3, y = pos >> 2; return ((x & 1) == 0 && (y & 1) == 0) ? 0 : ((x & 1) && (y & 1)) ? 1 : 2; }
inline int QpCroma(int q) {
    static const int t[22] = { 29, 30, 31, 32, 32, 33, 34, 34, 35, 35, 36, 36, 37, 37, 37, 38, 38, 38, 39, 39, 39, 39 };
    return q < 30 ? q : t[q - 30];
}
inline int Clip255(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }
inline int Med(int a, int b, int c) { return a > b ? (b > c ? b : (a > c ? c : a)) : (a > c ? a : (b > c ? c : b)); }

// la transformada directa 4x4 (la de la norma: W = Cf X CfT). in/out raster y*4+x
void Directa(const int* in, int* out) {
    int t[16];
    for (int y = 0; y < 4; y++) {
        const int* r = in + y * 4;
        const int s03 = r[0] + r[3], d03 = r[0] - r[3], s12 = r[1] + r[2], d12 = r[1] - r[2];
        t[y * 4 + 0] = s03 + s12; t[y * 4 + 2] = s03 - s12; t[y * 4 + 1] = 2 * d03 + d12; t[y * 4 + 3] = d03 - 2 * d12;
    }
    for (int x = 0; x < 4; x++) {
        const int a = t[x], b = t[4 + x], c = t[8 + x], d = t[12 + x];
        const int s03 = a + d, d03 = a - d, s12 = b + c, d12 = b - c;
        out[x] = s03 + s12; out[8 + x] = s03 - s12; out[4 + x] = 2 * d03 + d12; out[12 + x] = d03 - 2 * d12;
    }
}
// la inversa EXACTA del decodificador (8.5.12.2: primero las filas, despues las columnas, y (x + 32) >> 6)
void Inversa(const int* d, int* r) {
    int f[16];
    for (int i = 0; i < 4; i++) {
        const int* p = d + i * 4;
        const int e0 = p[0] + p[2], e1 = p[0] - p[2], e2 = (p[1] >> 1) - p[3], e3 = p[1] + (p[3] >> 1);
        f[i * 4 + 0] = e0 + e3; f[i * 4 + 1] = e1 + e2; f[i * 4 + 2] = e1 - e2; f[i * 4 + 3] = e0 - e3;
    }
    for (int j = 0; j < 4; j++) {
        const int a = f[j], b = f[4 + j], c = f[8 + j], e = f[12 + j];
        const int g0 = a + c, g1 = a - c, g2 = (b >> 1) - e, g3 = b + (e >> 1);
        r[j] = (g0 + g3 + 32) >> 6; r[4 + j] = (g1 + g2 + 32) >> 6; r[8 + j] = (g1 - g2 + 32) >> 6; r[12 + j] = (g0 - g3 + 32) >> 6;
    }
}
inline int Cuant(int w, int mf, int f, int qbits) {
    const int a = w < 0 ? -w : w;
    int l = (int)(((long long)a * mf + f) >> qbits);
    if (l > 2000) l = 2000;   // (Baseline: level_prefix hasta 15)
    return w < 0 ? -l : l;
}

// ---- CAVLC: un bloque (los niveles en orden de barrido, 'n' = maxNumCoeff; nC = -1: el DC del croma). Devuelve
//      cuantos no nulos tiene ----
int Bloque(Bits& b, const int* c, int n, int nC) {
    int pos[16], lv[16], total = 0;
    for (int i = n - 1; i >= 0; i--) if (c[i]) { pos[total] = i; lv[total] = c[i]; total++; }   // (de la frecuencia mas alta)
    int t1 = 0;
    for (int k = 0; k < total && t1 < 3; k++) { if (lv[k] == 1 || lv[k] == -1) t1++; else break; }
    if (nC < 0) b.Poner(kH264CdcTokBits[total * 4 + t1], kH264CdcTokLen[total * 4 + t1]);
    else {
        const int t = nC < 2 ? 0 : nC < 4 ? 1 : nC < 8 ? 2 : 3;
        b.Poner(kH264TokBits[t * 68 + total * 4 + t1], kH264TokLen[t * 68 + total * 4 + t1]);
    }
    if (!total) return 0;
    for (int k = 0; k < t1; k++) b.Poner(lv[k] < 0 ? 1 : 0, 1);
    int sl = (total > 10 && t1 < 3) ? 1 : 0;
    for (int k = t1; k < total; k++) {
        const int L = lv[k];
        int code = L > 0 ? 2 * L - 2 : -2 * L - 1;
        if (k == t1 && t1 < 3) code -= 2;
        if (sl == 0) {
            if (code < 14) { b.Poner(0, code); b.Poner(1, 1); }
            else if (code < 30) { b.Poner(0, 14); b.Poner(1, 1); b.Poner((unsigned)(code - 14), 4); }
            else { b.Poner(0, 15); b.Poner(1, 1); b.Poner((unsigned)(code - 30), 12); }
        } else {
            if (code < (15 << sl)) { b.Poner(0, code >> sl); b.Poner(1, 1); b.Poner((unsigned)(code & ((1 << sl) - 1)), sl); }
            else { b.Poner(0, 15); b.Poner(1, 1); b.Poner((unsigned)(code - (15 << sl)), 12); }
        }
        if (sl == 0) sl = 1;
        const int a = L < 0 ? -L : L;
        if (a > (3 << (sl - 1)) && sl < 6) sl++;
    }
    int tz = 0;
    if (total < n) {
        tz = pos[0] + 1 - total;
        if (nC < 0) b.Poner(kH264CdcTzBits[(total - 1) * 4 + tz], kH264CdcTzLen[(total - 1) * 4 + tz]);
        else b.Poner(kH264TzBits[(total - 1) * 16 + tz], kH264TzLen[(total - 1) * 16 + tz]);
    }
    int quedan = tz;
    for (int k = 0; k < total - 1 && quedan > 0; k++) {
        const int run = pos[k] - pos[k + 1] - 1;
        const int fila = (quedan > 7 ? 7 : quedan) - 1;
        b.Poner(kH264RunBits[fila * 16 + run], kH264RunLen[fila * 16 + run]);
        quedan -= run;
    }
    return total;
}

// el bloque 4x4 luma 'blk' (orden de la norma: de a 8x8 en Z) -> su lugar (en bloques de 4) adentro del macrobloque
inline void PosBlk(int blk, int& bx, int& by) { bx = ((blk >> 2) & 1) * 2 + (blk & 1); by = ((blk >> 3) & 1) * 2 + ((blk >> 1) & 1); }

} // namespace

W3dH264Codificador::W3dH264Codificador()
    : w(0), h(0), wSrc(0), fps(30), kbps(1000), gop(30), mbW(0), mbH(0), qp(28), nCuadro(0), idrId(0), bytesMeta(0), bytesHechos(0) {}

bool W3dH264Codificador::Abrir(int ww, int hh, int f, int k, int g) {
    w = ww & ~1; h = hh & ~1; wSrc = ww;
    if (w < 16 || h < 16) return false;
    fps = f > 0 ? f : 30; kbps = k > 16 ? k : 16; gop = g > 0 ? g : fps;
    mbW = (w + 15) / 16; mbH = (h + 15) / 16;
    const size_t PW = (size_t)mbW * 16, PH = (size_t)mbH * 16, tam = PW * PH * 3 / 2;
    cur.assign(tam, 0); rec.assign(tam, 0); ref.assign(tam, 0);
    nzL.assign((size_t)mbW * 4 * mbH * 4, 0); nzU.assign((size_t)mbW * 2 * mbH * 2, 0); nzV = nzU;
    mvx.assign((size_t)mbW * mbH, 0); mvy = mvx; esSalto.assign((size_t)mbW * mbH, 0);
    nCuadro = 0; idrId = 0; bytesHechos = 0;
    // el QP de arranque segun los bits por pixel que da el bitrate
    const double bpp = (double)kbps * 1000.0 / ((double)fps * w * h);
    int q = (int)(26.0 - 6.0 * log(bpp / 0.1) / log(2.0) + 0.5);
    qp = q < 14 ? 14 : (q > 44 ? 44 : q);
    return true;
}

void W3dH264Codificador::ConvertirRgba(const unsigned char* rgba) {
    const int PW = mbW * 16, PH = mbH * 16;
    unsigned char* Y = &cur[0];
    unsigned char* U = Y + (size_t)PW * PH;
    unsigned char* V = U + (size_t)(PW / 2) * (PH / 2);
    const int stride = wSrc;   // (el RGBA viene del ancho pedido)
    for (int y = 0; y < PH; y++) {
        const int sy = y < h ? y : h - 1;
        const unsigned char* fila = rgba + (size_t)sy * stride * 4;
        for (int x = 0; x < PW; x++) {
            const unsigned char* p = fila + (size_t)(x < w ? x : w - 1) * 4;
            Y[(size_t)y * PW + x] = (unsigned char)(((66 * p[0] + 129 * p[1] + 25 * p[2] + 128) >> 8) + 16);
        }
    }
    for (int y = 0; y < PH / 2; y++)
        for (int x = 0; x < PW / 2; x++) {
            int r = 0, g = 0, b = 0;
            for (int k = 0; k < 4; k++) {
                int sx = x * 2 + (k & 1), sy = y * 2 + (k >> 1);
                if (sx >= w) sx = w - 1;
                if (sy >= h) sy = h - 1;
                const unsigned char* p = rgba + ((size_t)sy * stride + sx) * 4;
                r += p[0]; g += p[1]; b += p[2];
            }
            r = (r + 2) >> 2; g = (g + 2) >> 2; b = (b + 2) >> 2;
            U[(size_t)y * (PW / 2) + x] = (unsigned char)Clip255(((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128);
            V[(size_t)y * (PW / 2) + x] = (unsigned char)Clip255(((112 * r - 94 * g - 18 * b + 128) >> 8) + 128);
        }
}

bool W3dH264Codificador::Cuadro(const unsigned char* rgba, std::vector<unsigned char>& salida) {
    salida.clear();
    if (!mbW || !rgba) return false;
    ConvertirRgba(rgba);
    const bool idr = (nCuadro % gop) == 0;
    if (idr) {
        // ---- SPS ----
        std::vector<unsigned char> r; Bits b(r);
        const long fs = (long)mbW * mbH, mbps = fs * fps;
        static const int kNiv[][3] = { {21, 792, 19800}, {22, 1620, 20250}, {30, 1620, 40500}, {31, 3600, 108000},
                                       {32, 5120, 216000}, {40, 8192, 245760}, {42, 8704, 522240}, {50, 22080, 589824},
                                       {51, 36864, 983040}, {52, 36864, 2073600} };
        int nivel = 52;
        for (int i = 0; i < 10; i++) if (fs <= kNiv[i][1] && mbps <= kNiv[i][2]) { nivel = kNiv[i][0]; break; }
        b.Poner(66, 8); b.Poner(0xC0, 8); b.Poner((unsigned)nivel, 8);
        b.Ue(0);            // seq_parameter_set_id
        b.Ue(12);           // log2_max_frame_num_minus4 (16 bits)
        b.Ue(2);            // pic_order_cnt_type 2 (el orden es el de decodificar)
        b.Ue(1);            // max_num_ref_frames
        b.Poner(0, 1);      // gaps_in_frame_num_value_allowed_flag
        b.Ue((unsigned)(mbW - 1)); b.Ue((unsigned)(mbH - 1));
        b.Poner(1, 1);      // frame_mbs_only_flag
        b.Poner(1, 1);      // direct_8x8_inference_flag
        const int cr = (mbW * 16 - w) / 2, cb = (mbH * 16 - h) / 2;
        if (cr || cb) { b.Poner(1, 1); b.Ue(0); b.Ue((unsigned)cr); b.Ue(0); b.Ue((unsigned)cb); } else b.Poner(0, 1);
        b.Poner(0, 1);      // vui_parameters_present_flag
        b.Cierre();
        Nal(salida, 3, 7, r);
        // ---- PPS ----
        std::vector<unsigned char> p; Bits c(p);
        c.Ue(0); c.Ue(0); c.Poner(0, 1); c.Poner(0, 1); c.Ue(0); c.Ue(0); c.Ue(0);
        c.Poner(0, 1); c.Poner(0, 2); c.Se(0); c.Se(0); c.Se(0);
        c.Poner(1, 1);      // deblocking_filter_control_present_flag (para apagarlo en el slice)
        c.Poner(0, 1); c.Poner(0, 1);
        c.Cierre();
        Nal(salida, 3, 8, p);
    }
    std::vector<unsigned char> nal;
    CodificarCuadro(idr, nal);
    salida.insert(salida.end(), nal.begin(), nal.end());
    // EL BITRATE: el QP del proximo cuadro segun lo que midio este (un cuadro clave puede medir varias veces mas)
    {
        const double objetivo = (double)kbps * 1000.0 / 8.0 / fps * (idr ? 4.0 : 1.0);
        const double r = salida.size() / (objetivo > 1.0 ? objetivo : 1.0);
        int d = (int)floor(6.0 * log(r > 0.01 ? r : 0.01) / log(2.0) + 0.5);
        if (d > 4) d = 4;
        if (d < -2) d = -2;
        qp += d;
        if (qp < 12) qp = 12;
        if (qp > 50) qp = 50;
    }
    bytesHechos += (long)salida.size();
    ref.swap(rec);   // (la reconstruccion pasa a ser la referencia; 'rec' se pisa entera en el proximo)
    nCuadro++;
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
//  EL CUADRO: un slice con todos los macrobloques
// ---------------------------------------------------------------------------------------------------------------------
void W3dH264Codificador::CodificarCuadro(bool idr, std::vector<unsigned char>& out) {
    const int PW = mbW * 16, PH = mbH * 16, CW = PW / 2, CH = PH / 2;
    const unsigned char* cY = &cur[0];
    const unsigned char* cU = cY + (size_t)PW * PH;
    const unsigned char* cV = cU + (size_t)CW * CH;
    unsigned char* rY = &rec[0];
    unsigned char* rU = rY + (size_t)PW * PH;
    unsigned char* rV = rU + (size_t)CW * CH;
    const unsigned char* fY = &ref[0];
    const unsigned char* fU = fY + (size_t)PW * PH;
    const unsigned char* fV = fU + (size_t)CW * CH;
    const int qpc = QpCroma(qp);
    const int qbits = 15 + qp / 6, qbitsC = 15 + qpc / 6;
    const int fIntra = (1 << qbits) / 3, fInter = (1 << qbits) / 6;
    const int fIntraC = (1 << qbitsC) / 3, fInterC = (1 << qbitsC) / 6;
    const int nzLW = mbW * 4, nzCW = mbW * 2;

    std::vector<unsigned char> r; Bits b(r);
    // ---- cabecera del slice ----
    b.Ue(0);                          // first_mb_in_slice
    b.Ue(idr ? 7u : 5u);              // slice_type (I / P, todos iguales)
    b.Ue(0);                          // pic_parameter_set_id
    const int frameNum = idr ? 0 : (nCuadro % gop);
    b.Poner((unsigned)(frameNum & 0xFFFF), 16);
    if (idr) { b.Ue((unsigned)(idrId & 1)); idrId++; }
    if (!idr) { b.Poner(0, 1); b.Poner(0, 1); }      // num_ref_idx_active_override_flag, ref_pic_list_modification_flag_l0
    if (idr) { b.Poner(0, 1); b.Poner(0, 1); }       // no_output_of_prior_pics_flag, long_term_reference_flag
    else b.Poner(0, 1);                              // adaptive_ref_pic_marking_mode_flag
    b.Se(qp - 26);                                   // slice_qp_delta
    b.Ue(1);                                         // disable_deblocking_filter_idc: sin filtro

    int saltos = 0;
    for (int my = 0; my < mbH; my++)
        for (int mx = 0; mx < mbW; mx++) {
            const int mb = my * mbW + mx;
            const int px = mx * 16, py = my * 16;
            int predY[256], predU[64], predV[64];
            int levAC[16][16];        // los niveles de cada bloque luma en orden de barrido (I16: 1..15)
            int levDC[16];            // I16: el DC en barrido
            int levCdc[2][4], levCac[2][4][16];
            int tcL[16], tcC[2][4];
            int cbpL = 0, cbpC = 0, modoI = 2;
            int mvX = 0, mvY = 0, mvpX = 0, mvpY = 0;
            bool salto = false;

            // ===== LA PREDICCION =====
            if (idr) {
                const bool hayA = mx > 0, hayB = my > 0;
                int top[16], left[16];
                for (int i = 0; i < 16; i++) {
                    top[i] = hayB ? rY[(size_t)(py - 1) * PW + px + i] : 0;
                    left[i] = hayA ? rY[(size_t)(py + i) * PW + px - 1] : 0;
                }
                int sT = 0, sL = 0; for (int i = 0; i < 16; i++) { sT += top[i]; sL += left[i]; }
                const int dc = (hayA && hayB) ? (sT + sL + 16) >> 5 : hayB ? (sT + 8) >> 4 : hayA ? (sL + 8) >> 4 : 128;
                long mejor = -1;
                for (int m = 0; m < 3; m++) {
                    if (m == 0 && !hayB) continue;
                    if (m == 1 && !hayA) continue;
                    long sad = 0;
                    for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) {
                        const int p = m == 0 ? top[x] : m == 1 ? left[y] : dc;
                        sad += abs((int)cY[(size_t)(py + y) * PW + px + x] - p);
                    }
                    if (mejor < 0 || sad < mejor) { mejor = sad; modoI = m; }
                }
                for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++)
                    predY[y * 16 + x] = modoI == 0 ? top[x] : modoI == 1 ? left[y] : dc;
                // croma: DC por bloque de 4x4 (8.3.4)
                for (int c = 0; c < 2; c++) {
                    const unsigned char* R = c == 0 ? rU : rV;
                    int* pr = c == 0 ? predU : predV;
                    const int cx = mx * 8, cy = my * 8;
                    for (int blk = 0; blk < 4; blk++) {
                        const int xo = (blk & 1) * 4, yo = (blk >> 1) * 4;
                        int sT2 = 0, sL2 = 0;
                        for (int i = 0; i < 4; i++) {
                            if (hayB) sT2 += R[(size_t)(cy - 1) * CW + cx + xo + i];
                            if (hayA) sL2 += R[(size_t)(cy + yo + i) * CW + cx - 1];
                        }
                        int v;
                        if ((xo == 0 && yo == 0) || (xo > 0 && yo > 0)) {
                            v = (hayA && hayB) ? (sT2 + sL2 + 4) >> 3 : hayA ? (sL2 + 2) >> 2 : hayB ? (sT2 + 2) >> 2 : 128;
                        } else if (xo > 0 && yo == 0) {
                            v = hayB ? (sT2 + 2) >> 2 : hayA ? (sL2 + 2) >> 2 : 128;
                        } else {
                            v = hayA ? (sL2 + 2) >> 2 : hayB ? (sT2 + 2) >> 2 : 128;
                        }
                        for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) pr[(yo + y) * 8 + xo + x] = v;
                    }
                }
            } else {
                // el movimiento PREDICHO (8.4.1.3) y el del P_Skip (8.4.1.1)
                const bool hayA = mx > 0, hayB = my > 0;
                bool hayC = my > 0 && mx + 1 < mbW;
                int cIdx = hayC ? mb - mbW + 1 : -1;
                if (!hayC && my > 0 && mx > 0) { hayC = true; cIdx = mb - mbW - 1; }
                int ax = 0, ay = 0, bxv = 0, byv = 0, cxv = 0, cyv = 0;
                if (hayA) { ax = mvx[mb - 1]; ay = mvy[mb - 1]; }
                if (hayB) { bxv = mvx[mb - mbW]; byv = mvy[mb - mbW]; }
                if (hayC) { cxv = mvx[cIdx]; cyv = mvy[cIdx]; }
                if (!hayB && !hayC && hayA) { mvpX = ax; mvpY = ay; }
                else {
                    const int nRef = (hayA ? 1 : 0) + (hayB ? 1 : 0) + (hayC ? 1 : 0);
                    if (nRef == 1) { mvpX = hayA ? ax : hayB ? bxv : cxv; mvpY = hayA ? ay : hayB ? byv : cyv; }
                    else { mvpX = Med(ax, bxv, cxv); mvpY = Med(ay, byv, cyv); }
                }
                int skX = mvpX, skY = mvpY;
                if (!hayA || !hayB || (ax == 0 && ay == 0) || (bxv == 0 && byv == 0)) { skX = 0; skY = 0; }
                // LA BUSQUEDA (pixel entero): el SAD con un costo por el movimiento; candidatos 0, el predicho y el del
                // salto, despues un diamante cada vez mas fino
                const int lam = 1 + qp / 6;
                struct Costo { static long Sad(const unsigned char* C, const unsigned char* F, int PW_, int PH_, int px_, int py_, int dx, int dy) {
                    const int x0 = px_ + dx, y0 = py_ + dy;
                    long s = 0;
                    if (x0 >= 0 && y0 >= 0 && x0 + 16 <= PW_ && y0 + 16 <= PH_) {
                        for (int y = 0; y < 16; y++) {
                            const unsigned char* a = C + (size_t)(py_ + y) * PW_ + px_;
                            const unsigned char* bb = F + (size_t)(y0 + y) * PW_ + x0;
                            for (int x = 0; x < 16; x++) s += abs((int)a[x] - (int)bb[x]);
                        }
                    } else {
                        for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) {
                            int yy = y0 + y, xx = x0 + x;
                            yy = yy < 0 ? 0 : (yy >= PH_ ? PH_ - 1 : yy); xx = xx < 0 ? 0 : (xx >= PW_ ? PW_ - 1 : xx);
                            s += abs((int)C[(size_t)(py_ + y) * PW_ + px_ + x] - (int)F[(size_t)yy * PW_ + xx]);
                        }
                    }
                    return s;
                } };
                const int R = 24;
                int bx = 0, by = 0;
                long best = -1;
                int cand[3][2] = { {0, 0}, {mvpX / 4, mvpY / 4}, {skX / 4, skY / 4} };
                for (int k = 0; k < 3; k++) {
                    const int dx = cand[k][0], dy = cand[k][1];
                    if (dx < -R || dx > R || dy < -R || dy > R) continue;
                    const long s = Costo::Sad(cY, fY, PW, PH, px, py, dx, dy) + lam * (abs(dx * 4 - mvpX) + abs(dy * 4 - mvpY));
                    if (best < 0 || s < best) { best = s; bx = dx; by = dy; }
                }
                for (int paso = 8; paso >= 1; paso >>= 1) {
                    bool mejoro = true;
                    for (int it = 0; it < 8 && mejoro; it++) {
                        mejoro = false;
                        static const int dd[4][2] = { {1, 0}, {-1, 0}, {0, 1}, {0, -1} };
                        const int ox = bx, oy = by;
                        for (int k = 0; k < 4; k++) {
                            const int dx = ox + dd[k][0] * paso, dy = oy + dd[k][1] * paso;
                            if (dx < -R || dx > R || dy < -R || dy > R) continue;
                            const long s = Costo::Sad(cY, fY, PW, PH, px, py, dx, dy) + lam * (abs(dx * 4 - mvpX) + abs(dy * 4 - mvpY));
                            if (s < best) { best = s; bx = dx; by = dy; mejoro = true; }
                        }
                    }
                }
                mvX = bx * 4; mvY = by * 4;
                // la prediccion: luma de pixel entero, croma bilineal en octavos (8.4.2.2.2)
                for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) {
                    int yy = py + y + by, xx = px + x + bx;
                    yy = yy < 0 ? 0 : (yy >= PH ? PH - 1 : yy); xx = xx < 0 ? 0 : (xx >= PW ? PW - 1 : xx);
                    predY[y * 16 + x] = fY[(size_t)yy * PW + xx];
                }
                const int xI = mvX >> 3, yI = mvY >> 3, xF = mvX & 7, yF = mvY & 7;
                for (int c = 0; c < 2; c++) {
                    const unsigned char* F = c == 0 ? fU : fV;
                    int* pr = c == 0 ? predU : predV;
                    for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) {
                        int x0 = mx * 8 + x + xI, y0 = my * 8 + y + yI, x1 = x0 + 1, y1 = y0 + 1;
                        x0 = x0 < 0 ? 0 : (x0 >= CW ? CW - 1 : x0); x1 = x1 < 0 ? 0 : (x1 >= CW ? CW - 1 : x1);
                        y0 = y0 < 0 ? 0 : (y0 >= CH ? CH - 1 : y0); y1 = y1 < 0 ? 0 : (y1 >= CH ? CH - 1 : y1);
                        const int A = F[(size_t)y0 * CW + x0], B = F[(size_t)y0 * CW + x1], C = F[(size_t)y1 * CW + x0], D = F[(size_t)y1 * CW + x1];
                        pr[y * 8 + x] = ((8 - xF) * (8 - yF) * A + xF * (8 - yF) * B + (8 - xF) * yF * C + xF * yF * D + 32) >> 6;
                    }
                }
                (void)skX; (void)skY;
                salto = (mvX == skX && mvY == skY);   // (si ademas no queda residuo: P_Skip)
            }

            // ===== EL RESIDUO: transformada y cuantizacion; y la reconstruccion (como el decodificador) =====
            int wDC[16];
            int deq[16][16];          // los coeficientes descuantizados de cada bloque luma (raster)
            for (int blk = 0; blk < 16; blk++) {
                int bx4, by4; PosBlk(blk, bx4, by4);
                int res[16], W[16];
                for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) {
                    const int yy = by4 * 4 + y, xx = bx4 * 4 + x;
                    res[y * 4 + x] = (int)cY[(size_t)(py + yy) * PW + px + xx] - predY[yy * 16 + xx];
                }
                Directa(res, W);
                const int m = qp % 6;
                int tc = 0;
                for (int k = 0; k < 16; k++) {
                    const int p = kZig[k];
                    if (idr && k == 0) { levAC[blk][0] = 0; continue; }
                    const int l = Cuant(W[p], kMF[m][Clase(p)], idr ? fIntra : fInter, qbits);
                    levAC[blk][k] = l;
                    if (l) tc++;
                }
                if (idr) wDC[by4 * 4 + bx4] = W[0];   // (el DC, a la transformada de Hadamard; raster de bloques)
                tcL[blk] = tc;
                for (int k = 0; k < 16; k++) { const int p = kZig[k]; deq[blk][p] = (levAC[blk][k] * kV[m][Clase(p)]) << (qp / 6); }
            }
            // I16: el DC de los 16 bloques (Hadamard 4x4)
            int dcRec[16];
            if (idr) {
                int t[16], Y[16];
                for (int i = 0; i < 4; i++) {
                    const int* s = wDC + i * 4;
                    const int a = s[0] + s[3], d = s[0] - s[3], b2 = s[1] + s[2], c2 = s[1] - s[2];
                    t[i * 4 + 0] = a + b2; t[i * 4 + 1] = d + c2; t[i * 4 + 2] = a - b2; t[i * 4 + 3] = d - c2;
                }
                for (int j = 0; j < 4; j++) {
                    const int a = t[j] + t[12 + j], d = t[j] - t[12 + j], b2 = t[4 + j] + t[8 + j], c2 = t[4 + j] - t[8 + j];
                    Y[j] = (a + b2) >> 1; Y[4 + j] = (d + c2) >> 1; Y[8 + j] = (a - b2) >> 1; Y[12 + j] = (d - c2) >> 1;
                }
                const int m = qp % 6;
                int c[16];
                for (int k = 0; k < 16; k++) { levDC[k] = Cuant(Y[kZig[k]], kMF[m][0], 2 * fIntra, qbits + 1); c[kZig[k]] = levDC[k]; }
                // la reconstruccion del DC (8.5.10): f = H c H y la escala
                int f2[16];
                for (int i = 0; i < 4; i++) {
                    const int* s = c + i * 4;
                    const int a = s[0] + s[1] + s[2] + s[3], b2 = s[0] + s[1] - s[2] - s[3], c2 = s[0] - s[1] - s[2] + s[3], d = s[0] - s[1] + s[2] - s[3];
                    t[i * 4 + 0] = a; t[i * 4 + 1] = b2; t[i * 4 + 2] = c2; t[i * 4 + 3] = d;
                }
                for (int j = 0; j < 4; j++) {
                    const int s0 = t[j], s1 = t[4 + j], s2 = t[8 + j], s3 = t[12 + j];
                    f2[j] = s0 + s1 + s2 + s3; f2[4 + j] = s0 + s1 - s2 - s3; f2[8 + j] = s0 - s1 - s2 + s3; f2[12 + j] = s0 - s1 + s2 - s3;
                }
                const int LS = 16 * kV[m][0];
                for (int k = 0; k < 16; k++) {
                    if (qp >= 36) dcRec[k] = (f2[k] * LS) << (qp / 6 - 6);
                    else dcRec[k] = (f2[k] * LS + (1 << (5 - qp / 6))) >> (6 - qp / 6);
                }
                bool hayAC = false;
                for (int blk = 0; blk < 16; blk++) if (tcL[blk]) hayAC = true;
                cbpL = hayAC ? 15 : 0;
                for (int blk = 0; blk < 16; blk++) {
                    int bx4, by4; PosBlk(blk, bx4, by4);
                    deq[blk][0] = dcRec[by4 * 4 + bx4];
                }
            } else {
                for (int i8 = 0; i8 < 4; i8++) {
                    bool hay = false;
                    for (int k = 0; k < 4; k++) if (tcL[i8 * 4 + k]) hay = true;
                    if (hay) cbpL |= 1 << i8;
                    else for (int k = 0; k < 4; k++) { for (int q = 0; q < 16; q++) { levAC[i8 * 4 + k][q] = 0; deq[i8 * 4 + k][q] = 0; } }
                }
            }
            // la reconstruccion luma
            for (int blk = 0; blk < 16; blk++) {
                int bx4, by4; PosBlk(blk, bx4, by4);
                int rr[16];
                Inversa(deq[blk], rr);
                for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) {
                    const int yy = by4 * 4 + y, xx = bx4 * 4 + x;
                    rY[(size_t)(py + yy) * PW + px + xx] = (unsigned char)Clip255(predY[yy * 16 + xx] + rr[y * 4 + x]);
                }
            }
            // ---- el croma ----
            bool hayCdc = false, hayCac = false;
            int deqC[2][4][16];
            for (int c = 0; c < 2; c++) {
                const unsigned char* C = c == 0 ? cU : cV;
                const int* pr = c == 0 ? predU : predV;
                int wdc[4];
                const int m = qpc % 6;
                for (int blk = 0; blk < 4; blk++) {
                    const int xo = (blk & 1) * 4, yo = (blk >> 1) * 4;
                    int res[16], W[16];
                    for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++)
                        res[y * 4 + x] = (int)C[(size_t)(my * 8 + yo + y) * CW + mx * 8 + xo + x] - pr[(yo + y) * 8 + xo + x];
                    Directa(res, W);
                    wdc[blk] = W[0];
                    int tc = 0;
                    levCac[c][blk][0] = 0;
                    for (int k = 1; k < 16; k++) {
                        const int p = kZig[k];
                        const int l = Cuant(W[p], kMF[m][Clase(p)], idr ? fIntraC : fInterC, qbitsC);
                        levCac[c][blk][k] = l; if (l) tc++;
                    }
                    tcC[c][blk] = tc;
                    if (tc) hayCac = true;
                }
                // el DC 2x2
                const int f0 = wdc[0] + wdc[1] + wdc[2] + wdc[3], f1 = wdc[0] - wdc[1] + wdc[2] - wdc[3];
                const int f2 = wdc[0] + wdc[1] - wdc[2] - wdc[3], f3 = wdc[0] - wdc[1] - wdc[2] + wdc[3];
                const int Ys[4] = { f0, f1, f2, f3 };
                for (int k = 0; k < 4; k++) { levCdc[c][k] = Cuant(Ys[k], kMF[m][0], 2 * (idr ? fIntraC : fInterC), qbitsC + 1); if (levCdc[c][k]) hayCdc = true; }
            }
            cbpC = hayCac ? 2 : (hayCdc ? 1 : 0);
            for (int c = 0; c < 2; c++) {
                const int m = qpc % 6;
                if (cbpC < 2) for (int blk = 0; blk < 4; blk++) { tcC[c][blk] = 0; for (int k = 0; k < 16; k++) levCac[c][blk][k] = 0; }
                if (cbpC == 0) for (int k = 0; k < 4; k++) levCdc[c][k] = 0;
                // la reconstruccion del DC (8.5.11.2): f = A c A, dcC = ((f * LS) << (qP / 6)) >> 5
                const int* l = levCdc[c];
                const int g[4] = { l[0] + l[1] + l[2] + l[3], l[0] - l[1] + l[2] - l[3], l[0] + l[1] - l[2] - l[3], l[0] - l[1] - l[2] + l[3] };
                const int LS = 16 * kV[m][0];
                for (int blk = 0; blk < 4; blk++) {
                    for (int k = 0; k < 16; k++) { const int p = kZig[k]; deqC[c][blk][p] = (levCac[c][blk][k] * kV[m][Clase(p)]) << (qpc / 6); }
                    deqC[c][blk][0] = ((g[blk] * LS) << (qpc / 6)) >> 5;
                }
                unsigned char* RR = c == 0 ? rU : rV;
                const int* pr = c == 0 ? predU : predV;
                for (int blk = 0; blk < 4; blk++) {
                    const int xo = (blk & 1) * 4, yo = (blk >> 1) * 4;
                    int rr[16];
                    Inversa(deqC[c][blk], rr);
                    for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++)
                        RR[(size_t)(my * 8 + yo + y) * CW + mx * 8 + xo + x] = (unsigned char)Clip255(pr[(yo + y) * 8 + xo + x] + rr[y * 4 + x]);
                }
            }

            // ===== los no nulos de cada bloque (para el nC de los vecinos) =====
            for (int blk = 0; blk < 16; blk++) {
                int bx4, by4; PosBlk(blk, bx4, by4);
                int v = tcL[blk];
                if (idr ? cbpL == 0 : !(cbpL & (1 << (blk >> 2)))) v = 0;
                nzL[(size_t)(my * 4 + by4) * nzLW + mx * 4 + bx4] = (signed char)v;
            }
            for (int blk = 0; blk < 4; blk++) {
                nzU[(size_t)(my * 2 + (blk >> 1)) * nzCW + mx * 2 + (blk & 1)] = (signed char)tcC[0][blk];
                nzV[(size_t)(my * 2 + (blk >> 1)) * nzCW + mx * 2 + (blk & 1)] = (signed char)tcC[1][blk];
            }

            // ===== P_Skip: mismo movimiento que el predicho para el salto y nada de residuo =====
            if (!idr && salto && cbpL == 0 && cbpC == 0) {
                mvx[mb] = (short)mvX; mvy[mb] = (short)mvY; esSalto[mb] = 1;
                saltos++;
                continue;
            }
            esSalto[mb] = 0;
            mvx[mb] = (short)(idr ? 0 : mvX); mvy[mb] = (short)(idr ? 0 : mvY);

            // ===== LOS BITS DEL MACROBLOQUE =====
            struct NC { static int Luma(const std::vector<signed char>& nz, int W_, int x4, int y4) {
                const bool a = x4 > 0, bb = y4 > 0;
                const int nA = a ? nz[(size_t)y4 * W_ + x4 - 1] : 0, nB = bb ? nz[(size_t)(y4 - 1) * W_ + x4] : 0;
                return (a && bb) ? (nA + nB + 1) >> 1 : a ? nA : bb ? nB : 0;
            } };
            if (idr) {
                b.Ue((unsigned)(1 + modoI + 4 * cbpC + (cbpL ? 12 : 0)));   // mb_type I_16x16_<modo>_<cbpC>_<AC>
                b.Ue(0);                                                     // intra_chroma_pred_mode: DC
                b.Se(0);                                                     // mb_qp_delta
                Bloque(b, levDC, 16, NC::Luma(nzL, nzLW, mx * 4, my * 4));   // el DC (el nC del bloque 0)
                if (cbpL) for (int blk = 0; blk < 16; blk++) {
                    int bx4, by4; PosBlk(blk, bx4, by4);
                    Bloque(b, levAC[blk] + 1, 15, NC::Luma(nzL, nzLW, mx * 4 + bx4, my * 4 + by4));
                }
            } else {
                b.Ue((unsigned)saltos); saltos = 0;              // mb_skip_run
                b.Ue(0);                                         // mb_type P_L0_16x16
                b.Se(mvX - mvpX); b.Se(mvY - mvpY);              // mvd_l0
                const int cbp = cbpL | (cbpC << 4);
                int code = 0; for (int i = 0; i < 48; i++) if (kH264GolombACbpInter[i] == cbp) { code = i; break; }
                b.Ue((unsigned)code);                            // coded_block_pattern
                if (cbp) {
                    b.Se(0);                                     // mb_qp_delta
                    for (int blk = 0; blk < 16; blk++) {
                        if (!(cbpL & (1 << (blk >> 2)))) continue;
                        int bx4, by4; PosBlk(blk, bx4, by4);
                        Bloque(b, levAC[blk], 16, NC::Luma(nzL, nzLW, mx * 4 + bx4, my * 4 + by4));
                    }
                }
            }
            if (cbpC) {
                Bloque(b, levCdc[0], 4, -1);
                Bloque(b, levCdc[1], 4, -1);
                if (cbpC == 2) for (int c = 0; c < 2; c++) {
                    const std::vector<signed char>& nz = c == 0 ? nzU : nzV;
                    for (int blk = 0; blk < 4; blk++)
                        Bloque(b, levCac[c][blk] + 1, 15, NC::Luma(nz, nzCW, mx * 2 + (blk & 1), my * 2 + (blk >> 1)));
                }
            }
        }
    if (!idr && saltos) b.Ue((unsigned)saltos);   // (los ultimos saltados)
    b.Cierre();
    Nal(out, idr ? 3 : 2, idr ? 5 : 1, r);
}
