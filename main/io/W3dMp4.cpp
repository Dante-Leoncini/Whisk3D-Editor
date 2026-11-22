// ============================================================================
//  W3dMp4.cpp - el contenedor MP4 de un video ya codificado (ver el .h). Las cajas son las de ISO/IEC 14496-12 (el
//  formato de archivo) y 14496-15 (avcC) / 14496-1 (esds), escritas a mano.
// ============================================================================
#include "W3dMp4.h"
#include <cstdio>
#include <cstring>

namespace {
// ---- escribir de a numeros big-endian en un vector ----
struct Box {
    std::vector<unsigned char>& v;
    size_t start;
    // abre una caja: deja el lugar del tamanio (se completa al cerrarla)
    Box(std::vector<unsigned char>& out, const char* type) : v(out), start(out.size()) {
        U32(0); v.insert(v.end(), type, type + 4);
    }
    ~Box() {
        const size_t n = v.size() - start;
        v[start] = (unsigned char)(n >> 24); v[start + 1] = (unsigned char)(n >> 16);
        v[start + 2] = (unsigned char)(n >> 8); v[start + 3] = (unsigned char)n;
    }
    void U8(unsigned x) { v.push_back((unsigned char)x); }
    void U16(unsigned x) { U8(x >> 8); U8(x); }
    void U24(unsigned x) { U8(x >> 16); U8(x >> 8); U8(x); }
    void U32(unsigned x) { U8(x >> 24); U8(x >> 16); U8(x >> 8); U8(x); }
    void Zeros(size_t n) { v.insert(v.end(), n, (unsigned char)0); }
    void Bytes(const std::vector<unsigned char>& b) { v.insert(v.end(), b.begin(), b.end()); }
    void Text4(const char* t) { v.insert(v.end(), t, t + 4); }
    void VersionFlags(unsigned show, unsigned flags) { U8(show); U24(flags); }
    void Matrix() {   // la identidad (16.16 y 2.30)
        U32(0x00010000); U32(0); U32(0);
        U32(0); U32(0x00010000); U32(0);
        U32(0); U32(0); U32(0x40000000);
    }
};

// los NAL de un cuadro en Annex B: [inicio, fin) de cada uno, sin el codigo de inicio
void NalsAnnexB(const unsigned char* d, size_t n, std::vector<std::pair<size_t, size_t> >& out) {
    out.clear();
    size_t i = 0, start = (size_t)-1;
    while (i + 2 < n) {
        if (d[i] == 0 && d[i + 1] == 0 && d[i + 2] == 1) {
            if (start != (size_t)-1) {
                size_t end = i;
                while (end > start && d[end - 1] == 0) end--;   // (el cero de mas de un 00 00 00 01)
                out.push_back(std::make_pair(start, end));
            }
            i += 3; start = i;
            continue;
        }
        i++;
    }
    if (start != (size_t)-1 && start < n) out.push_back(std::make_pair(start, n));
}
// el cuadro no trae codigos de inicio: cada NAL con su largo de 4 bytes adelante (AVCC). Algunos codificadores lo dan asi
bool NalsWithLength(const unsigned char* d, size_t n, std::vector<std::pair<size_t, size_t> >& out) {
    out.clear();
    size_t i = 0;
    while (i + 4 <= n) {
        const size_t l = ((size_t)d[i] << 24) | ((size_t)d[i + 1] << 16) | ((size_t)d[i + 2] << 8) | d[i + 3];
        if (l == 0 || i + 4 + l > n) return false;
        out.push_back(std::make_pair(i + 4, i + 4 + l));
        i += 4 + l;
    }
    return i == n && !out.empty();
}
bool BeginsWithStart(const unsigned char* d, size_t n) {
    return (n >= 3 && d[0] == 0 && d[1] == 0 && d[2] == 1) || (n >= 4 && d[0] == 0 && d[1] == 0 && d[2] == 0 && d[3] == 1);
}
}

W3dMp4Writer::W3dMp4Writer() : open(false), codec(W3D_MP4_H264), w(0), h(0), fps(25), aFreq(0), aCan(0), aKbps(0),
    aDelay(0), aSamples(0) {}

void W3dMp4Writer::AudioAac(const std::vector<unsigned char>& asc, const std::vector<unsigned char>& d,
                              const std::vector<unsigned int>& t, int frequency, int channels, int kbps,
                              int delay, long samples) {
    aAsc = asc; aData = d; aSize = t; aFreq = frequency; aCan = channels; aKbps = kbps;
    aDelay = delay > 0 ? delay : 0; aSamples = samples > 0 ? samples : 0;
}

bool W3dMp4Writer::Open(const std::string& r, int c, int width, int height, int f) {
    path = r; codec = c; w = width; h = height; fps = f > 0 ? f : 25;
    data.clear(); size.clear(); keys.clear(); sps.clear(); pps.clear(); dsi.clear(); error.clear();
    aAsc.clear(); aData.clear(); aSize.clear(); aFreq = aCan = aKbps = aDelay = 0; aSamples = 0;
    open = (w > 0 && h > 0) || codec == W3D_MP4_ONLY_AUDIO;
    if (!open) error = "tamanio invalido";
    return open;
}

void W3dMp4Writer::Config(const unsigned char* d, size_t n) {
    if (!d || n == 0) return;
    if (codec == W3D_MP4_H264) {
        std::vector<std::pair<size_t, size_t> > nals;
        if (BeginsWithStart(d, n)) NalsAnnexB(d, n, nals); else if (!NalsWithLength(d, n, nals)) return;
        for (size_t k = 0; k < nals.size(); k++) {
            const unsigned char* p = d + nals[k].first; const size_t l = nals[k].second - nals[k].first;
            if (l == 0) continue;
            const int t = p[0] & 0x1F;
            if (t == 7 && sps.empty()) sps.assign(p, p + l);
            if (t == 8 && pps.empty()) pps.assign(p, p + l);
        }
    } else if (dsi.empty()) {
        // hasta el primer VOP (si lo hay)
        size_t end = n;
        for (size_t i = 0; i + 3 < n; i++) if (d[i] == 0 && d[i + 1] == 0 && d[i + 2] == 1 && d[i + 3] == 0xB6) { end = i; break; }
        dsi.assign(d, d + end);
    }
}

bool W3dMp4Writer::FrameH264(const unsigned char* d, size_t n) {
    std::vector<std::pair<size_t, size_t> > nals;
    if (BeginsWithStart(d, n)) NalsAnnexB(d, n, nals);
    else if (!NalsWithLength(d, n, nals)) { error = "H.264 sin codigos de inicio ni largos"; return false; }
    const size_t prev = data.size();
    bool key = false, image = false;
    for (size_t k = 0; k < nals.size(); k++) {
        const unsigned char* p = d + nals[k].first; const size_t l = nals[k].second - nals[k].first;
        if (l == 0) continue;
        const int t = p[0] & 0x1F;
        if (t == 7) { if (sps.empty()) sps.assign(p, p + l); continue; }   // (al avcC)
        if (t == 8) { if (pps.empty()) pps.assign(p, p + l); continue; }
        if (t == 9) continue;                                              // (el delimitador: sobra)
        if (t == 5) key = true;
        if (t >= 1 && t <= 5) image = true;
        data.push_back((unsigned char)(l >> 24)); data.push_back((unsigned char)(l >> 16));
        data.push_back((unsigned char)(l >> 8)); data.push_back((unsigned char)l);
        data.insert(data.end(), p, p + l);
    }
    if (!image) { data.resize(prev); return true; }   // (solo configuracion: no es un cuadro)
    size.push_back((unsigned int)(data.size() - prev));
    if (key) keys.push_back((unsigned int)size.size());
    return true;
}

bool W3dMp4Writer::FrameMpeg4(const unsigned char* d, size_t n) {
    size_t vop = n;
    for (size_t i = 0; i + 3 < n; i++) if (d[i] == 0 && d[i + 1] == 0 && d[i + 2] == 1 && d[i + 3] == 0xB6) { vop = i; break; }
    if (vop > 0 && dsi.empty()) dsi.assign(d, d + vop);   // (la configuracion viene pegada al primero)
    if (vop >= n) { return true; }                          // (solo configuracion)
    // I-VOP: los dos bits de arriba despues del B6 son 00
    const bool key = (vop + 4 < n) && ((d[vop + 4] >> 6) == 0);
    data.insert(data.end(), d + vop, d + n);
    size.push_back((unsigned int)(n - vop));
    if (key) keys.push_back((unsigned int)size.size());
    return true;
}

bool W3dMp4Writer::Frame(const unsigned char* d, size_t n) {
    if (!open) { error = "no esta abierto"; return false; }
    if (!d || n == 0) return false;
    return codec == W3D_MP4_H264 ? FrameH264(d, n) : FrameMpeg4(d, n);
}

static bool WriteStdio(const char* r, const unsigned char* d, size_t n) {
    FILE* f = fopen(r, "wb");
    if (!f) return false;
    const size_t e = n ? fwrite(d, 1, n, f) : 0;
    const bool ok = (fclose(f) == 0) && e == n;
    return ok;
}

bool W3dMp4Writer::Close(bool (*write)(const char*, const unsigned char*, size_t)) {
    if (!open) return false;
    open = false;
    const bool withVideo = codec != W3D_MP4_ONLY_AUDIO;
    const bool hasAudio  = !aSize.empty() && aFreq > 0 && !aAsc.empty();
    if (withVideo && size.empty()) { error = "sin cuadros"; return false; }
    if (!withVideo && !hasAudio) { error = "sin audio"; return false; }
    if (codec == W3D_MP4_H264 && (sps.size() < 4 || pps.empty())) { error = "el codificador no dio el SPS / PPS"; return false; }
    const unsigned scale = 90000, delta = 90000u / (unsigned)fps;
    const unsigned nFrames = (unsigned)size.size();
    const unsigned durMs = (unsigned)((unsigned long long)nFrames * 1000u / (unsigned)fps);
    const unsigned durScale = nFrames * delta;
    const bool withAudio = !aSize.empty() && aFreq > 0 && !aAsc.empty();
    // (lo que suena de verdad: sin el retardo del codificador ni el relleno del ultimo cuadro, si se sabe)
    const unsigned long long samplesA = withAudio ? (aSamples > 0 ? (unsigned long long)aSamples
                                         : (unsigned long long)aSize.size() * 1024u - (unsigned)aDelay) : 0u;
    const unsigned durAudioMs = withAudio ? (unsigned)(samplesA * 1000u / (unsigned)aFreq) : 0u;
    const unsigned durTotalMs = durAudioMs > durMs ? durAudioMs : durMs;

    std::vector<unsigned char> out;
    out.reserve(data.size() + 4096 + size.size() * 4);
    { Box c(out, "ftyp");
      if (!withVideo) { c.Text4("M4A "); c.U32(0x200); c.Text4("M4A "); c.Text4("isom"); c.Text4("mp42"); }
      else { c.Text4("isom"); c.U32(0x200); c.Text4("isom"); c.Text4("iso2"); c.Text4(codec == W3D_MP4_H264 ? "avc1" : "mp41"); c.Text4("mp41"); } }
    size_t posStco = 0;   // donde va el offset del mdat (se completa cuando se sabe cuanto mide el moov)
    size_t posStcoA = 0;  // (el del audio: despues de los cuadros de video)
    { Box moov(out, "moov");
      { Box c(out, "mvhd"); c.VersionFlags(0, 0); c.U32(0); c.U32(0); c.U32(1000); c.U32(durTotalMs);
        c.U32(0x00010000); c.U16(0x0100); c.Zeros(10); c.Matrix(); c.Zeros(24); c.U32((withVideo ? 2u : 1u) + (withAudio ? 1u : 0u)); }
      if (withVideo) { Box trak(out, "trak");
        { Box c(out, "tkhd"); c.VersionFlags(0, 3); c.U32(0); c.U32(0); c.U32(1); c.U32(0); c.U32(durMs);
          c.Zeros(8); c.U16(0); c.U16(0); c.U16(0); c.U16(0); c.Matrix(); c.U32((unsigned)w << 16); c.U32((unsigned)h << 16); }
        { Box mdia(out, "mdia");
          { Box c(out, "mdhd"); c.VersionFlags(0, 0); c.U32(0); c.U32(0); c.U32(scale); c.U32(durScale); c.U16(0x55C4); c.U16(0); }
          { Box c(out, "hdlr"); c.VersionFlags(0, 0); c.U32(0); c.Text4("vide"); c.Zeros(12);
            const char* name = "Whisk3D video"; out.insert(out.end(), name, name + strlen(name) + 1); }
          { Box minf(out, "minf");
            { Box c(out, "vmhd"); c.VersionFlags(0, 1); c.Zeros(8); }
            { Box dinf(out, "dinf"); { Box dref(out, "dref"); dref.VersionFlags(0, 0); dref.U32(1);
                { Box u(out, "url "); u.VersionFlags(0, 1); } } }
            { Box stbl(out, "stbl");
              { Box stsd(out, "stsd"); stsd.VersionFlags(0, 0); stsd.U32(1);
                { Box e(out, codec == W3D_MP4_H264 ? "avc1" : "mp4v");
                  e.Zeros(6); e.U16(1); e.Zeros(16); e.U16((unsigned)w); e.U16((unsigned)h);
                  e.U32(0x00480000); e.U32(0x00480000); e.U32(0); e.U16(1);
                  { unsigned char name[32]; memset(name, 0, 32); const char* t = "Whisk3D"; name[0] = (unsigned char)strlen(t);
                    memcpy(name + 1, t, strlen(t)); out.insert(out.end(), name, name + 32); }
                  e.U16(0x0018); e.U16(0xFFFF);
                  if (codec == W3D_MP4_H264) {
                      Box a(out, "avcC");
                      a.U8(1); a.U8(sps[1]); a.U8(sps[2]); a.U8(sps[3]); a.U8(0xFF);   // (largos de 4 bytes)
                      a.U8(0xE1); a.U16((unsigned)sps.size()); a.Bytes(sps);
                      a.U8(1); a.U16((unsigned)pps.size()); a.Bytes(pps);
                  } else {
                      // esds: ES_Descriptor > DecoderConfigDescriptor (MPEG-4 Visual) > DecoderSpecificInfo (el VOL) +
                      // SLConfigDescriptor. Los largos de los descriptores en la forma de 4 bytes (sirve para cualquiera)
                      Box s(out, "esds"); s.VersionFlags(0, 0);
                      struct L { static void Length(Box& c, unsigned x) {
                          c.U8(0x80 | ((x >> 21) & 0x7F)); c.U8(0x80 | ((x >> 14) & 0x7F)); c.U8(0x80 | ((x >> 7) & 0x7F)); c.U8(x & 0x7F); } };
                      const unsigned nDsi = (unsigned)dsi.size();
                      const unsigned nDcd = 13 + (nDsi ? 5 + nDsi : 0);
                      const unsigned nIs = 3 + 5 + nDcd + 6;
                      s.U8(0x03); L::Length(s, nIs); s.U16(1); s.U8(0);
                      s.U8(0x04); L::Length(s, nDcd); s.U8(0x20); s.U8(0x11); s.U24(0); s.U32(0); s.U32(0);
                      if (nDsi) { s.U8(0x05); L::Length(s, nDsi); s.Bytes(dsi); }
                      s.U8(0x06); L::Length(s, 1); s.U8(0x02);
                  }
                }
              }
              { Box c(out, "stts"); c.VersionFlags(0, 0); c.U32(1); c.U32(nFrames); c.U32(delta); }
              if (keys.size() < size.size()) {   // (todos clave: sin stss)
                  Box c(out, "stss"); c.VersionFlags(0, 0); c.U32((unsigned)keys.size());
                  for (size_t i = 0; i < keys.size(); i++) c.U32(keys[i]);
              }
              { Box c(out, "stsc"); c.VersionFlags(0, 0); c.U32(1); c.U32(1); c.U32(nFrames); c.U32(1); }
              { Box c(out, "stsz"); c.VersionFlags(0, 0); c.U32(0); c.U32(nFrames);
                for (size_t i = 0; i < size.size(); i++) c.U32(size[i]); }
              { Box c(out, "stco"); c.VersionFlags(0, 0); c.U32(1); posStco = out.size(); c.U32(0); }
            }
          }
        }
      }
      // ---- LA PISTA DE AUDIO (AAC: mp4a + esds) ----
      if (withAudio) {
        Box trak(out, "trak");
        const unsigned nA = (unsigned)aSize.size();
        { Box c(out, "tkhd"); c.VersionFlags(0, 3); c.U32(0); c.U32(0); c.U32(withVideo ? 2 : 1); c.U32(0); c.U32(durAudioMs);
          c.Zeros(8); c.U16(0); c.U16(1); c.U16(0x0100); c.U16(0); c.Matrix(); c.U32(0); c.U32(0); }
        // la LISTA DE EDICION: saltear el retardo del codificador (el sonido arranca junto con el primer cuadro)
        if (aDelay > 0 || aSamples > 0) {
            Box edts(out, "edts");
            Box c(out, "elst"); c.VersionFlags(0, 0); c.U32(1);
            c.U32(durAudioMs); c.U32((unsigned)aDelay); c.U16(1); c.U16(0);
        }
        { Box mdia(out, "mdia");
          { Box c(out, "mdhd"); c.VersionFlags(0, 0); c.U32(0); c.U32(0); c.U32((unsigned)aFreq); c.U32(nA * 1024u); c.U16(0x55C4); c.U16(0); }
          { Box c(out, "hdlr"); c.VersionFlags(0, 0); c.U32(0); c.Text4("soun"); c.Zeros(12);
            const char* name = "Whisk3D audio"; out.insert(out.end(), name, name + strlen(name) + 1); }
          { Box minf(out, "minf");
            { Box c(out, "smhd"); c.VersionFlags(0, 0); c.U16(0); c.U16(0); }
            { Box dinf(out, "dinf"); { Box dref(out, "dref"); dref.VersionFlags(0, 0); dref.U32(1);
                { Box u(out, "url "); u.VersionFlags(0, 1); } } }
            { Box stbl(out, "stbl");
              { Box stsd(out, "stsd"); stsd.VersionFlags(0, 0); stsd.U32(1);
                { Box e(out, "mp4a");
                  e.Zeros(6); e.U16(1); e.Zeros(8); e.U16((unsigned)aCan); e.U16(16); e.U16(0); e.U16(0); e.U32((unsigned)aFreq << 16);
                  Box s(out, "esds"); s.VersionFlags(0, 0);
                  struct L { static void Length(Box& c, unsigned x) {
                      c.U8(0x80 | ((x >> 21) & 0x7F)); c.U8(0x80 | ((x >> 14) & 0x7F)); c.U8(0x80 | ((x >> 7) & 0x7F)); c.U8(x & 0x7F); } };
                  const unsigned nDsi = (unsigned)aAsc.size();
                  const unsigned nDcd = 13 + 5 + nDsi;
                  const unsigned nIs = 3 + 5 + nDcd + 6;
                  const unsigned br = (unsigned)(aKbps > 0 ? aKbps : 128) * 1000u;
                  s.U8(0x03); L::Length(s, nIs); s.U16(2); s.U8(0);
                  s.U8(0x04); L::Length(s, nDcd); s.U8(0x40); s.U8(0x15); s.U24(0); s.U32(br); s.U32(br);
                  s.U8(0x05); L::Length(s, nDsi); s.Bytes(aAsc);
                  s.U8(0x06); L::Length(s, 1); s.U8(0x02);
                }
              }
              { Box c(out, "stts"); c.VersionFlags(0, 0); c.U32(1); c.U32(nA); c.U32(1024); }
              { Box c(out, "stsc"); c.VersionFlags(0, 0); c.U32(1); c.U32(1); c.U32(nA); c.U32(1); }
              { Box c(out, "stsz"); c.VersionFlags(0, 0); c.U32(0); c.U32(nA); for (size_t i = 0; i < aSize.size(); i++) c.U32(aSize[i]); }
              { Box c(out, "stco"); c.VersionFlags(0, 0); c.U32(1); posStcoA = out.size(); c.U32(0); }
            }
          }
        }
      }
    }
    // el mdat: los cuadros. El offset del primero (despues de la cabecera del mdat) va al stco
    const size_t ofs = out.size() + 8;
    if (withVideo) {
        out[posStco] = (unsigned char)(ofs >> 24); out[posStco + 1] = (unsigned char)(ofs >> 16);
        out[posStco + 2] = (unsigned char)(ofs >> 8); out[posStco + 3] = (unsigned char)ofs;
    }
    if (withAudio) {
        const size_t ofsA = ofs + data.size();
        out[posStcoA] = (unsigned char)(ofsA >> 24); out[posStcoA + 1] = (unsigned char)(ofsA >> 16);
        out[posStcoA + 2] = (unsigned char)(ofsA >> 8); out[posStcoA + 3] = (unsigned char)ofsA;
    }
    { Box m(out, "mdat"); out.insert(out.end(), data.begin(), data.end()); if (withAudio) out.insert(out.end(), aData.begin(), aData.end()); }
    std::vector<unsigned char>().swap(data);
    const bool ok = (write ? write : WriteStdio)(path.c_str(), &out[0], out.size());
    if (!ok) error = "no se pudo escribir el archivo";
    return ok;
}

// ============================================================================
//  LEER LA PISTA AAC de un MP4 ya escrito (el del conversor del telefono, ffmpeg...)
// ============================================================================
namespace {
unsigned int B32(const unsigned char* p) { return ((unsigned)p[0] << 24) | ((unsigned)p[1] << 16) | ((unsigned)p[2] << 8) | p[3]; }
unsigned int B16(const unsigned char* p) { return ((unsigned)p[0] << 8) | p[1]; }
// la primera caja 'tipo' adentro de [a, b) (sin bajar); devuelve donde empieza su contenido y su fin
bool Child(const std::vector<unsigned char>& f, size_t a, size_t b, const char* type, size_t& start, size_t& end) {
    size_t p = a;
    while (p + 8 <= b) {
        size_t n = B32(&f[p]); size_t cab = 8;
        if (n == 1 && p + 16 <= b) { n = (size_t)(((unsigned long long)B32(&f[p + 8]) << 32) | B32(&f[p + 12])); cab = 16; }
        else if (n == 0) n = b - p;
        if (n < cab || p + n > b) return false;
        if (memcmp(&f[p + 4], type, 4) == 0) { start = p + cab; end = p + n; return true; }
        p += n;
    }
    return false;
}
// el largo de un descriptor de 14496-1 (1 a 4 bytes de 7 bits)
size_t LengthDesc(const std::vector<unsigned char>& f, size_t& p, size_t end) {
    size_t n = 0;
    for (int k = 0; k < 4 && p < end; k++) { const unsigned char c = f[p++]; n = (n << 7) | (c & 0x7F); if (!(c & 0x80)) break; }
    return n;
}
}

bool W3dMp4ReadAac(const std::vector<unsigned char>& f, std::vector<unsigned char>& asc, std::vector<unsigned char>& data,
                   std::vector<unsigned int>& sizes, int& frequency, int& channels, std::string* error) {
    asc.clear(); data.clear(); sizes.clear(); frequency = channels = 0;
    size_t mi, mf;
    if (!Child(f, 0, f.size(), "moov", mi, mf)) { if (error) *error = "sin moov"; return false; }
    // la pista de SONIDO (hdlr 'soun')
    size_t p = mi;
    while (p + 8 <= mf) {
        size_t ti, tf;
        if (!Child(f, p, mf, "trak", ti, tf)) break;
        p = tf;
        size_t di, df, hi, hf, ni, nf, yes, sf;
        if (!Child(f, ti, tf, "mdia", di, df) || !Child(f, di, df, "hdlr", hi, hf) || hf - hi < 12) continue;
        if (memcmp(&f[hi + 8], "soun", 4) != 0) continue;
        if (!Child(f, di, df, "minf", ni, nf) || !Child(f, ni, nf, "stbl", yes, sf)) continue;
        size_t a, b;
        // stsd > mp4a > esds > ... > DecoderSpecificInfo (tag 5)
        if (!Child(f, yes, sf, "stsd", a, b) || b - a < 8) continue;
        size_t ei, ef;
        if (!Child(f, a + 8, b, "mp4a", ei, ef) || ef - ei < 28) { if (error) *error = "el audio no es AAC (mp4a)"; return false; }
        channels = (int)B16(&f[ei + 16]);
        frequency = (int)(B32(&f[ei + 24]) >> 16);
        size_t xi, xf;
        if (!Child(f, ei + 28, ef, "esds", xi, xf)) { if (error) *error = "sin esds"; return false; }
        size_t q = xi + 4;
        while (q < xf) {
            const unsigned char tag = f[q++];
            const size_t n = LengthDesc(f, q, xf);
            if (tag == 0x03) { q += 3; continue; }                       // ES_Descriptor: ES_ID + flags, y adentro
            if (tag == 0x04) { q += 13; continue; }                      // DecoderConfig: y adentro
            if (tag == 0x05) { if (q + n <= xf) asc.assign(f.begin() + q, f.begin() + q + n); break; }
            q += n;
        }
        if (asc.size() < 2) { if (error) *error = "sin la configuracion del AAC"; return false; }
        // la frecuencia de verdad sale del ASC (un archivo con SBR puede decir otra en el mp4a)
        // tamanios (stsz), los pedazos (stco / co64) y cuantos cuadros por pedazo (stsc)
        if (!Child(f, yes, sf, "stsz", a, b) || b - a < 12) { if (error) *error = "sin stsz"; return false; }
        const unsigned fixed = B32(&f[a + 4]), nS = B32(&f[a + 8]);
        for (unsigned k = 0; k < nS; k++) sizes.push_back(fixed ? fixed : (a + 12 + k * 4 + 4 <= b ? B32(&f[a + 12 + k * 4]) : 0));
        std::vector<unsigned long long> ofs;
        if (Child(f, yes, sf, "stco", a, b)) { const unsigned n = B32(&f[a + 4]); for (unsigned k = 0; k < n && a + 8 + k * 4 + 4 <= b; k++) ofs.push_back(B32(&f[a + 8 + k * 4])); }
        else if (Child(f, yes, sf, "co64", a, b)) { const unsigned n = B32(&f[a + 4]); for (unsigned k = 0; k < n && a + 8 + k * 8 + 8 <= b; k++)
            ofs.push_back(((unsigned long long)B32(&f[a + 8 + k * 8]) << 32) | B32(&f[a + 12 + k * 8])); }
        if (!Child(f, yes, sf, "stsc", a, b)) { if (error) *error = "sin stsc"; return false; }
        const unsigned nE = B32(&f[a + 4]);
        size_t sample = 0;
        for (size_t c = 0; c < ofs.size() && sample < sizes.size(); c++) {
            unsigned byPiece = 1;   // (la entrada de stsc que le toca a este pedazo: la ultima con first_chunk <= c+1)
            for (unsigned e = 0; e < nE && a + 8 + e * 12 + 12 <= b; e++) if (B32(&f[a + 8 + e * 12]) <= (unsigned)(c + 1)) byPiece = B32(&f[a + 12 + e * 12]);
            unsigned long long o = ofs[c];
            for (unsigned k = 0; k < byPiece && sample < sizes.size(); k++) {
                const unsigned n = sizes[sample++];
                if (o + n > f.size()) { if (error) *error = "un cuadro afuera del archivo"; return false; }
                data.insert(data.end(), f.begin() + (size_t)o, f.begin() + (size_t)o + n);
                o += n;
            }
        }
        if (sample != sizes.size()) { if (error) *error = "faltan cuadros"; return false; }
        return !sizes.empty();
    }
    if (error) *error = "sin pista de sonido";
    return false;
}

bool W3dAdtsReadAac(const std::vector<unsigned char>& f, std::vector<unsigned char>& asc, std::vector<unsigned char>& data,
                    std::vector<unsigned int>& sizes, int& frequency, int& channels, std::string* error) {
    static const int kFreq[13] = { 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350 };
    asc.clear(); data.clear(); sizes.clear(); frequency = channels = 0;
    size_t p = 0;
    while (p + 7 <= f.size()) {
        if (f[p] != 0xFF || (f[p + 1] & 0xF0) != 0xF0) { p++; continue; }   // (busca la sincronia)
        const bool noCrc = (f[p + 1] & 1) != 0;
        const int profile = (f[p + 2] >> 6) & 3, sf = (f[p + 2] >> 2) & 15;
        const int ch = ((f[p + 2] & 1) << 2) | ((f[p + 3] >> 6) & 3);
        const size_t length = ((size_t)(f[p + 3] & 3) << 11) | ((size_t)f[p + 4] << 3) | ((f[p + 5] >> 5) & 7);
        const size_t cab = noCrc ? 7 : 9;
        if (length < cab || p + length > f.size() || sf > 12) break;
        if (asc.empty()) {
            const unsigned v = ((unsigned)(profile + 1) << 11) | ((unsigned)sf << 7) | ((unsigned)ch << 3);
            asc.push_back((unsigned char)(v >> 8)); asc.push_back((unsigned char)v);
            frequency = kFreq[sf]; channels = ch;
        }
        data.insert(data.end(), f.begin() + p + cab, f.begin() + p + length);
        sizes.push_back((unsigned)(length - cab));
        p += length;
    }
    if (sizes.empty()) { if (error) *error = "no es un .aac (ADTS)"; return false; }
    return true;
}

