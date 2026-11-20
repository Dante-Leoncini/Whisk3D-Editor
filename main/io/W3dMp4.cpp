// ============================================================================
//  W3dMp4.cpp - el contenedor MP4 de un video ya codificado (ver el .h). Las cajas son las de ISO/IEC 14496-12 (el
//  formato de archivo) y 14496-15 (avcC) / 14496-1 (esds), escritas a mano.
// ============================================================================
#include "W3dMp4.h"
#include <cstdio>
#include <cstring>

namespace {
// ---- escribir de a numeros big-endian en un vector ----
struct Caja {
    std::vector<unsigned char>& v;
    size_t inicio;
    // abre una caja: deja el lugar del tamanio (se completa al cerrarla)
    Caja(std::vector<unsigned char>& out, const char* tipo) : v(out), inicio(out.size()) {
        U32(0); v.insert(v.end(), tipo, tipo + 4);
    }
    ~Caja() {
        const size_t n = v.size() - inicio;
        v[inicio] = (unsigned char)(n >> 24); v[inicio + 1] = (unsigned char)(n >> 16);
        v[inicio + 2] = (unsigned char)(n >> 8); v[inicio + 3] = (unsigned char)n;
    }
    void U8(unsigned x) { v.push_back((unsigned char)x); }
    void U16(unsigned x) { U8(x >> 8); U8(x); }
    void U24(unsigned x) { U8(x >> 16); U8(x >> 8); U8(x); }
    void U32(unsigned x) { U8(x >> 24); U8(x >> 16); U8(x >> 8); U8(x); }
    void Ceros(size_t n) { v.insert(v.end(), n, (unsigned char)0); }
    void Bytes(const std::vector<unsigned char>& b) { v.insert(v.end(), b.begin(), b.end()); }
    void Texto4(const char* t) { v.insert(v.end(), t, t + 4); }
    void VersionFlags(unsigned ver, unsigned flags) { U8(ver); U24(flags); }
    void Matriz() {   // la identidad (16.16 y 2.30)
        U32(0x00010000); U32(0); U32(0);
        U32(0); U32(0x00010000); U32(0);
        U32(0); U32(0); U32(0x40000000);
    }
};

// los NAL de un cuadro en Annex B: [inicio, fin) de cada uno, sin el codigo de inicio
void NalsAnnexB(const unsigned char* d, size_t n, std::vector<std::pair<size_t, size_t> >& out) {
    out.clear();
    size_t i = 0, ini = (size_t)-1;
    while (i + 2 < n) {
        if (d[i] == 0 && d[i + 1] == 0 && d[i + 2] == 1) {
            if (ini != (size_t)-1) {
                size_t fin = i;
                while (fin > ini && d[fin - 1] == 0) fin--;   // (el cero de mas de un 00 00 00 01)
                out.push_back(std::make_pair(ini, fin));
            }
            i += 3; ini = i;
            continue;
        }
        i++;
    }
    if (ini != (size_t)-1 && ini < n) out.push_back(std::make_pair(ini, n));
}
// el cuadro no trae codigos de inicio: cada NAL con su largo de 4 bytes adelante (AVCC). Algunos codificadores lo dan asi
bool NalsConLargo(const unsigned char* d, size_t n, std::vector<std::pair<size_t, size_t> >& out) {
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
bool EmpiezaConInicio(const unsigned char* d, size_t n) {
    return (n >= 3 && d[0] == 0 && d[1] == 0 && d[2] == 1) || (n >= 4 && d[0] == 0 && d[1] == 0 && d[2] == 0 && d[3] == 1);
}
}

W3dMp4Escritor::W3dMp4Escritor() : abierto(false), codec(W3D_MP4_H264), w(0), h(0), fps(25), aFrec(0), aCan(0), aKbps(0),
    aRetardo(0), aMuestras(0) {}

void W3dMp4Escritor::AudioAac(const std::vector<unsigned char>& asc, const std::vector<unsigned char>& d,
                              const std::vector<unsigned int>& t, int frecuencia, int canales, int kbps,
                              int retardo, long muestras) {
    aAsc = asc; aDatos = d; aTam = t; aFrec = frecuencia; aCan = canales; aKbps = kbps;
    aRetardo = retardo > 0 ? retardo : 0; aMuestras = muestras > 0 ? muestras : 0;
}

bool W3dMp4Escritor::Abrir(const std::string& r, int c, int ancho, int alto, int f) {
    ruta = r; codec = c; w = ancho; h = alto; fps = f > 0 ? f : 25;
    datos.clear(); tam.clear(); claves.clear(); sps.clear(); pps.clear(); dsi.clear(); error.clear();
    aAsc.clear(); aDatos.clear(); aTam.clear(); aFrec = aCan = aKbps = aRetardo = 0; aMuestras = 0;
    abierto = (w > 0 && h > 0) || codec == W3D_MP4_SOLO_AUDIO;
    if (!abierto) error = "tamanio invalido";
    return abierto;
}

void W3dMp4Escritor::Configuracion(const unsigned char* d, size_t n) {
    if (!d || n == 0) return;
    if (codec == W3D_MP4_H264) {
        std::vector<std::pair<size_t, size_t> > nals;
        if (EmpiezaConInicio(d, n)) NalsAnnexB(d, n, nals); else if (!NalsConLargo(d, n, nals)) return;
        for (size_t k = 0; k < nals.size(); k++) {
            const unsigned char* p = d + nals[k].first; const size_t l = nals[k].second - nals[k].first;
            if (l == 0) continue;
            const int t = p[0] & 0x1F;
            if (t == 7 && sps.empty()) sps.assign(p, p + l);
            if (t == 8 && pps.empty()) pps.assign(p, p + l);
        }
    } else if (dsi.empty()) {
        // hasta el primer VOP (si lo hay)
        size_t fin = n;
        for (size_t i = 0; i + 3 < n; i++) if (d[i] == 0 && d[i + 1] == 0 && d[i + 2] == 1 && d[i + 3] == 0xB6) { fin = i; break; }
        dsi.assign(d, d + fin);
    }
}

bool W3dMp4Escritor::CuadroH264(const unsigned char* d, size_t n) {
    std::vector<std::pair<size_t, size_t> > nals;
    if (EmpiezaConInicio(d, n)) NalsAnnexB(d, n, nals);
    else if (!NalsConLargo(d, n, nals)) { error = "H.264 sin codigos de inicio ni largos"; return false; }
    const size_t antes = datos.size();
    bool clave = false, imagen = false;
    for (size_t k = 0; k < nals.size(); k++) {
        const unsigned char* p = d + nals[k].first; const size_t l = nals[k].second - nals[k].first;
        if (l == 0) continue;
        const int t = p[0] & 0x1F;
        if (t == 7) { if (sps.empty()) sps.assign(p, p + l); continue; }   // (al avcC)
        if (t == 8) { if (pps.empty()) pps.assign(p, p + l); continue; }
        if (t == 9) continue;                                              // (el delimitador: sobra)
        if (t == 5) clave = true;
        if (t >= 1 && t <= 5) imagen = true;
        datos.push_back((unsigned char)(l >> 24)); datos.push_back((unsigned char)(l >> 16));
        datos.push_back((unsigned char)(l >> 8)); datos.push_back((unsigned char)l);
        datos.insert(datos.end(), p, p + l);
    }
    if (!imagen) { datos.resize(antes); return true; }   // (solo configuracion: no es un cuadro)
    tam.push_back((unsigned int)(datos.size() - antes));
    if (clave) claves.push_back((unsigned int)tam.size());
    return true;
}

bool W3dMp4Escritor::CuadroMpeg4(const unsigned char* d, size_t n) {
    size_t vop = n;
    for (size_t i = 0; i + 3 < n; i++) if (d[i] == 0 && d[i + 1] == 0 && d[i + 2] == 1 && d[i + 3] == 0xB6) { vop = i; break; }
    if (vop > 0 && dsi.empty()) dsi.assign(d, d + vop);   // (la configuracion viene pegada al primero)
    if (vop >= n) { return true; }                          // (solo configuracion)
    // I-VOP: los dos bits de arriba despues del B6 son 00
    const bool clave = (vop + 4 < n) && ((d[vop + 4] >> 6) == 0);
    datos.insert(datos.end(), d + vop, d + n);
    tam.push_back((unsigned int)(n - vop));
    if (clave) claves.push_back((unsigned int)tam.size());
    return true;
}

bool W3dMp4Escritor::Cuadro(const unsigned char* d, size_t n) {
    if (!abierto) { error = "no esta abierto"; return false; }
    if (!d || n == 0) return false;
    return codec == W3D_MP4_H264 ? CuadroH264(d, n) : CuadroMpeg4(d, n);
}

static bool EscribirStdio(const char* r, const unsigned char* d, size_t n) {
    FILE* f = fopen(r, "wb");
    if (!f) return false;
    const size_t e = n ? fwrite(d, 1, n, f) : 0;
    const bool ok = (fclose(f) == 0) && e == n;
    return ok;
}

bool W3dMp4Escritor::Cerrar(bool (*escribir)(const char*, const unsigned char*, size_t)) {
    if (!abierto) return false;
    abierto = false;
    const bool conVideo = codec != W3D_MP4_SOLO_AUDIO;
    const bool hayAudio = !aTam.empty() && aFrec > 0 && !aAsc.empty();
    if (conVideo && tam.empty()) { error = "sin cuadros"; return false; }
    if (!conVideo && !hayAudio) { error = "sin audio"; return false; }
    if (codec == W3D_MP4_H264 && (sps.size() < 4 || pps.empty())) { error = "el codificador no dio el SPS / PPS"; return false; }
    const unsigned escala = 90000, delta = 90000u / (unsigned)fps;
    const unsigned nCuadros = (unsigned)tam.size();
    const unsigned durMs = (unsigned)((unsigned long long)nCuadros * 1000u / (unsigned)fps);
    const unsigned durEscala = nCuadros * delta;
    const bool conAudio = !aTam.empty() && aFrec > 0 && !aAsc.empty();
    // (lo que suena de verdad: sin el retardo del codificador ni el relleno del ultimo cuadro, si se sabe)
    const unsigned long long muestrasA = conAudio ? (aMuestras > 0 ? (unsigned long long)aMuestras
                                         : (unsigned long long)aTam.size() * 1024u - (unsigned)aRetardo) : 0u;
    const unsigned durAudioMs = conAudio ? (unsigned)(muestrasA * 1000u / (unsigned)aFrec) : 0u;
    const unsigned durTotalMs = durAudioMs > durMs ? durAudioMs : durMs;

    std::vector<unsigned char> out;
    out.reserve(datos.size() + 4096 + tam.size() * 4);
    { Caja c(out, "ftyp");
      if (!conVideo) { c.Texto4("M4A "); c.U32(0x200); c.Texto4("M4A "); c.Texto4("isom"); c.Texto4("mp42"); }
      else { c.Texto4("isom"); c.U32(0x200); c.Texto4("isom"); c.Texto4("iso2"); c.Texto4(codec == W3D_MP4_H264 ? "avc1" : "mp41"); c.Texto4("mp41"); } }
    size_t posStco = 0;   // donde va el offset del mdat (se completa cuando se sabe cuanto mide el moov)
    size_t posStcoA = 0;  // (el del audio: despues de los cuadros de video)
    { Caja moov(out, "moov");
      { Caja c(out, "mvhd"); c.VersionFlags(0, 0); c.U32(0); c.U32(0); c.U32(1000); c.U32(durTotalMs);
        c.U32(0x00010000); c.U16(0x0100); c.Ceros(10); c.Matriz(); c.Ceros(24); c.U32((conVideo ? 2u : 1u) + (conAudio ? 1u : 0u)); }
      if (conVideo) { Caja trak(out, "trak");
        { Caja c(out, "tkhd"); c.VersionFlags(0, 3); c.U32(0); c.U32(0); c.U32(1); c.U32(0); c.U32(durMs);
          c.Ceros(8); c.U16(0); c.U16(0); c.U16(0); c.U16(0); c.Matriz(); c.U32((unsigned)w << 16); c.U32((unsigned)h << 16); }
        { Caja mdia(out, "mdia");
          { Caja c(out, "mdhd"); c.VersionFlags(0, 0); c.U32(0); c.U32(0); c.U32(escala); c.U32(durEscala); c.U16(0x55C4); c.U16(0); }
          { Caja c(out, "hdlr"); c.VersionFlags(0, 0); c.U32(0); c.Texto4("vide"); c.Ceros(12);
            const char* nom = "Whisk3D video"; out.insert(out.end(), nom, nom + strlen(nom) + 1); }
          { Caja minf(out, "minf");
            { Caja c(out, "vmhd"); c.VersionFlags(0, 1); c.Ceros(8); }
            { Caja dinf(out, "dinf"); { Caja dref(out, "dref"); dref.VersionFlags(0, 0); dref.U32(1);
                { Caja u(out, "url "); u.VersionFlags(0, 1); } } }
            { Caja stbl(out, "stbl");
              { Caja stsd(out, "stsd"); stsd.VersionFlags(0, 0); stsd.U32(1);
                { Caja e(out, codec == W3D_MP4_H264 ? "avc1" : "mp4v");
                  e.Ceros(6); e.U16(1); e.Ceros(16); e.U16((unsigned)w); e.U16((unsigned)h);
                  e.U32(0x00480000); e.U32(0x00480000); e.U32(0); e.U16(1);
                  { unsigned char nombre[32]; memset(nombre, 0, 32); const char* t = "Whisk3D"; nombre[0] = (unsigned char)strlen(t);
                    memcpy(nombre + 1, t, strlen(t)); out.insert(out.end(), nombre, nombre + 32); }
                  e.U16(0x0018); e.U16(0xFFFF);
                  if (codec == W3D_MP4_H264) {
                      Caja a(out, "avcC");
                      a.U8(1); a.U8(sps[1]); a.U8(sps[2]); a.U8(sps[3]); a.U8(0xFF);   // (largos de 4 bytes)
                      a.U8(0xE1); a.U16((unsigned)sps.size()); a.Bytes(sps);
                      a.U8(1); a.U16((unsigned)pps.size()); a.Bytes(pps);
                  } else {
                      // esds: ES_Descriptor > DecoderConfigDescriptor (MPEG-4 Visual) > DecoderSpecificInfo (el VOL) +
                      // SLConfigDescriptor. Los largos de los descriptores en la forma de 4 bytes (sirve para cualquiera)
                      Caja s(out, "esds"); s.VersionFlags(0, 0);
                      struct L { static void Largo(Caja& c, unsigned x) {
                          c.U8(0x80 | ((x >> 21) & 0x7F)); c.U8(0x80 | ((x >> 14) & 0x7F)); c.U8(0x80 | ((x >> 7) & 0x7F)); c.U8(x & 0x7F); } };
                      const unsigned nDsi = (unsigned)dsi.size();
                      const unsigned nDcd = 13 + (nDsi ? 5 + nDsi : 0);
                      const unsigned nEs = 3 + 5 + nDcd + 6;
                      s.U8(0x03); L::Largo(s, nEs); s.U16(1); s.U8(0);
                      s.U8(0x04); L::Largo(s, nDcd); s.U8(0x20); s.U8(0x11); s.U24(0); s.U32(0); s.U32(0);
                      if (nDsi) { s.U8(0x05); L::Largo(s, nDsi); s.Bytes(dsi); }
                      s.U8(0x06); L::Largo(s, 1); s.U8(0x02);
                  }
                }
              }
              { Caja c(out, "stts"); c.VersionFlags(0, 0); c.U32(1); c.U32(nCuadros); c.U32(delta); }
              if (claves.size() < tam.size()) {   // (todos clave: sin stss)
                  Caja c(out, "stss"); c.VersionFlags(0, 0); c.U32((unsigned)claves.size());
                  for (size_t i = 0; i < claves.size(); i++) c.U32(claves[i]);
              }
              { Caja c(out, "stsc"); c.VersionFlags(0, 0); c.U32(1); c.U32(1); c.U32(nCuadros); c.U32(1); }
              { Caja c(out, "stsz"); c.VersionFlags(0, 0); c.U32(0); c.U32(nCuadros);
                for (size_t i = 0; i < tam.size(); i++) c.U32(tam[i]); }
              { Caja c(out, "stco"); c.VersionFlags(0, 0); c.U32(1); posStco = out.size(); c.U32(0); }
            }
          }
        }
      }
      // ---- LA PISTA DE AUDIO (AAC: mp4a + esds) ----
      if (conAudio) {
        Caja trak(out, "trak");
        const unsigned nA = (unsigned)aTam.size();
        { Caja c(out, "tkhd"); c.VersionFlags(0, 3); c.U32(0); c.U32(0); c.U32(conVideo ? 2 : 1); c.U32(0); c.U32(durAudioMs);
          c.Ceros(8); c.U16(0); c.U16(1); c.U16(0x0100); c.U16(0); c.Matriz(); c.U32(0); c.U32(0); }
        // la LISTA DE EDICION: saltear el retardo del codificador (el sonido arranca junto con el primer cuadro)
        if (aRetardo > 0 || aMuestras > 0) {
            Caja edts(out, "edts");
            Caja c(out, "elst"); c.VersionFlags(0, 0); c.U32(1);
            c.U32(durAudioMs); c.U32((unsigned)aRetardo); c.U16(1); c.U16(0);
        }
        { Caja mdia(out, "mdia");
          { Caja c(out, "mdhd"); c.VersionFlags(0, 0); c.U32(0); c.U32(0); c.U32((unsigned)aFrec); c.U32(nA * 1024u); c.U16(0x55C4); c.U16(0); }
          { Caja c(out, "hdlr"); c.VersionFlags(0, 0); c.U32(0); c.Texto4("soun"); c.Ceros(12);
            const char* nom = "Whisk3D audio"; out.insert(out.end(), nom, nom + strlen(nom) + 1); }
          { Caja minf(out, "minf");
            { Caja c(out, "smhd"); c.VersionFlags(0, 0); c.U16(0); c.U16(0); }
            { Caja dinf(out, "dinf"); { Caja dref(out, "dref"); dref.VersionFlags(0, 0); dref.U32(1);
                { Caja u(out, "url "); u.VersionFlags(0, 1); } } }
            { Caja stbl(out, "stbl");
              { Caja stsd(out, "stsd"); stsd.VersionFlags(0, 0); stsd.U32(1);
                { Caja e(out, "mp4a");
                  e.Ceros(6); e.U16(1); e.Ceros(8); e.U16((unsigned)aCan); e.U16(16); e.U16(0); e.U16(0); e.U32((unsigned)aFrec << 16);
                  Caja s(out, "esds"); s.VersionFlags(0, 0);
                  struct L { static void Largo(Caja& c, unsigned x) {
                      c.U8(0x80 | ((x >> 21) & 0x7F)); c.U8(0x80 | ((x >> 14) & 0x7F)); c.U8(0x80 | ((x >> 7) & 0x7F)); c.U8(x & 0x7F); } };
                  const unsigned nDsi = (unsigned)aAsc.size();
                  const unsigned nDcd = 13 + 5 + nDsi;
                  const unsigned nEs = 3 + 5 + nDcd + 6;
                  const unsigned br = (unsigned)(aKbps > 0 ? aKbps : 128) * 1000u;
                  s.U8(0x03); L::Largo(s, nEs); s.U16(2); s.U8(0);
                  s.U8(0x04); L::Largo(s, nDcd); s.U8(0x40); s.U8(0x15); s.U24(0); s.U32(br); s.U32(br);
                  s.U8(0x05); L::Largo(s, nDsi); s.Bytes(aAsc);
                  s.U8(0x06); L::Largo(s, 1); s.U8(0x02);
                }
              }
              { Caja c(out, "stts"); c.VersionFlags(0, 0); c.U32(1); c.U32(nA); c.U32(1024); }
              { Caja c(out, "stsc"); c.VersionFlags(0, 0); c.U32(1); c.U32(1); c.U32(nA); c.U32(1); }
              { Caja c(out, "stsz"); c.VersionFlags(0, 0); c.U32(0); c.U32(nA); for (size_t i = 0; i < aTam.size(); i++) c.U32(aTam[i]); }
              { Caja c(out, "stco"); c.VersionFlags(0, 0); c.U32(1); posStcoA = out.size(); c.U32(0); }
            }
          }
        }
      }
    }
    // el mdat: los cuadros. El offset del primero (despues de la cabecera del mdat) va al stco
    const size_t ofs = out.size() + 8;
    if (conVideo) {
        out[posStco] = (unsigned char)(ofs >> 24); out[posStco + 1] = (unsigned char)(ofs >> 16);
        out[posStco + 2] = (unsigned char)(ofs >> 8); out[posStco + 3] = (unsigned char)ofs;
    }
    if (conAudio) {
        const size_t ofsA = ofs + datos.size();
        out[posStcoA] = (unsigned char)(ofsA >> 24); out[posStcoA + 1] = (unsigned char)(ofsA >> 16);
        out[posStcoA + 2] = (unsigned char)(ofsA >> 8); out[posStcoA + 3] = (unsigned char)ofsA;
    }
    { Caja m(out, "mdat"); out.insert(out.end(), datos.begin(), datos.end()); if (conAudio) out.insert(out.end(), aDatos.begin(), aDatos.end()); }
    std::vector<unsigned char>().swap(datos);
    const bool ok = (escribir ? escribir : EscribirStdio)(ruta.c_str(), &out[0], out.size());
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
bool Hija(const std::vector<unsigned char>& f, size_t a, size_t b, const char* tipo, size_t& ini, size_t& fin) {
    size_t p = a;
    while (p + 8 <= b) {
        size_t n = B32(&f[p]); size_t cab = 8;
        if (n == 1 && p + 16 <= b) { n = (size_t)(((unsigned long long)B32(&f[p + 8]) << 32) | B32(&f[p + 12])); cab = 16; }
        else if (n == 0) n = b - p;
        if (n < cab || p + n > b) return false;
        if (memcmp(&f[p + 4], tipo, 4) == 0) { ini = p + cab; fin = p + n; return true; }
        p += n;
    }
    return false;
}
// el largo de un descriptor de 14496-1 (1 a 4 bytes de 7 bits)
size_t LargoDesc(const std::vector<unsigned char>& f, size_t& p, size_t fin) {
    size_t n = 0;
    for (int k = 0; k < 4 && p < fin; k++) { const unsigned char c = f[p++]; n = (n << 7) | (c & 0x7F); if (!(c & 0x80)) break; }
    return n;
}
}

bool W3dMp4LeerAac(const std::vector<unsigned char>& f, std::vector<unsigned char>& asc, std::vector<unsigned char>& datos,
                   std::vector<unsigned int>& tamanios, int& frecuencia, int& canales, std::string* error) {
    asc.clear(); datos.clear(); tamanios.clear(); frecuencia = canales = 0;
    size_t mi, mf;
    if (!Hija(f, 0, f.size(), "moov", mi, mf)) { if (error) *error = "sin moov"; return false; }
    // la pista de SONIDO (hdlr 'soun')
    size_t p = mi;
    while (p + 8 <= mf) {
        size_t ti, tf;
        if (!Hija(f, p, mf, "trak", ti, tf)) break;
        p = tf;
        size_t di, df, hi, hf, ni, nf, si, sf;
        if (!Hija(f, ti, tf, "mdia", di, df) || !Hija(f, di, df, "hdlr", hi, hf) || hf - hi < 12) continue;
        if (memcmp(&f[hi + 8], "soun", 4) != 0) continue;
        if (!Hija(f, di, df, "minf", ni, nf) || !Hija(f, ni, nf, "stbl", si, sf)) continue;
        size_t a, b;
        // stsd > mp4a > esds > ... > DecoderSpecificInfo (tag 5)
        if (!Hija(f, si, sf, "stsd", a, b) || b - a < 8) continue;
        size_t ei, ef;
        if (!Hija(f, a + 8, b, "mp4a", ei, ef) || ef - ei < 28) { if (error) *error = "el audio no es AAC (mp4a)"; return false; }
        canales = (int)B16(&f[ei + 16]);
        frecuencia = (int)(B32(&f[ei + 24]) >> 16);
        size_t xi, xf;
        if (!Hija(f, ei + 28, ef, "esds", xi, xf)) { if (error) *error = "sin esds"; return false; }
        size_t q = xi + 4;
        while (q < xf) {
            const unsigned char tag = f[q++];
            const size_t n = LargoDesc(f, q, xf);
            if (tag == 0x03) { q += 3; continue; }                       // ES_Descriptor: ES_ID + flags, y adentro
            if (tag == 0x04) { q += 13; continue; }                      // DecoderConfig: y adentro
            if (tag == 0x05) { if (q + n <= xf) asc.assign(f.begin() + q, f.begin() + q + n); break; }
            q += n;
        }
        if (asc.size() < 2) { if (error) *error = "sin la configuracion del AAC"; return false; }
        // la frecuencia de verdad sale del ASC (un archivo con SBR puede decir otra en el mp4a)
        // tamanios (stsz), los pedazos (stco / co64) y cuantos cuadros por pedazo (stsc)
        if (!Hija(f, si, sf, "stsz", a, b) || b - a < 12) { if (error) *error = "sin stsz"; return false; }
        const unsigned fijo = B32(&f[a + 4]), nS = B32(&f[a + 8]);
        for (unsigned k = 0; k < nS; k++) tamanios.push_back(fijo ? fijo : (a + 12 + k * 4 + 4 <= b ? B32(&f[a + 12 + k * 4]) : 0));
        std::vector<unsigned long long> ofs;
        if (Hija(f, si, sf, "stco", a, b)) { const unsigned n = B32(&f[a + 4]); for (unsigned k = 0; k < n && a + 8 + k * 4 + 4 <= b; k++) ofs.push_back(B32(&f[a + 8 + k * 4])); }
        else if (Hija(f, si, sf, "co64", a, b)) { const unsigned n = B32(&f[a + 4]); for (unsigned k = 0; k < n && a + 8 + k * 8 + 8 <= b; k++)
            ofs.push_back(((unsigned long long)B32(&f[a + 8 + k * 8]) << 32) | B32(&f[a + 12 + k * 8])); }
        if (!Hija(f, si, sf, "stsc", a, b)) { if (error) *error = "sin stsc"; return false; }
        const unsigned nE = B32(&f[a + 4]);
        size_t muestra = 0;
        for (size_t c = 0; c < ofs.size() && muestra < tamanios.size(); c++) {
            unsigned porPedazo = 1;   // (la entrada de stsc que le toca a este pedazo: la ultima con first_chunk <= c+1)
            for (unsigned e = 0; e < nE && a + 8 + e * 12 + 12 <= b; e++) if (B32(&f[a + 8 + e * 12]) <= (unsigned)(c + 1)) porPedazo = B32(&f[a + 12 + e * 12]);
            unsigned long long o = ofs[c];
            for (unsigned k = 0; k < porPedazo && muestra < tamanios.size(); k++) {
                const unsigned n = tamanios[muestra++];
                if (o + n > f.size()) { if (error) *error = "un cuadro afuera del archivo"; return false; }
                datos.insert(datos.end(), f.begin() + (size_t)o, f.begin() + (size_t)o + n);
                o += n;
            }
        }
        if (muestra != tamanios.size()) { if (error) *error = "faltan cuadros"; return false; }
        return !tamanios.empty();
    }
    if (error) *error = "sin pista de sonido";
    return false;
}

bool W3dAdtsLeerAac(const std::vector<unsigned char>& f, std::vector<unsigned char>& asc, std::vector<unsigned char>& datos,
                    std::vector<unsigned int>& tamanios, int& frecuencia, int& canales, std::string* error) {
    static const int kFrec[13] = { 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350 };
    asc.clear(); datos.clear(); tamanios.clear(); frecuencia = canales = 0;
    size_t p = 0;
    while (p + 7 <= f.size()) {
        if (f[p] != 0xFF || (f[p + 1] & 0xF0) != 0xF0) { p++; continue; }   // (busca la sincronia)
        const bool sinCrc = (f[p + 1] & 1) != 0;
        const int perfil = (f[p + 2] >> 6) & 3, sf = (f[p + 2] >> 2) & 15;
        const int ch = ((f[p + 2] & 1) << 2) | ((f[p + 3] >> 6) & 3);
        const size_t largo = ((size_t)(f[p + 3] & 3) << 11) | ((size_t)f[p + 4] << 3) | ((f[p + 5] >> 5) & 7);
        const size_t cab = sinCrc ? 7 : 9;
        if (largo < cab || p + largo > f.size() || sf > 12) break;
        if (asc.empty()) {
            const unsigned v = ((unsigned)(perfil + 1) << 11) | ((unsigned)sf << 7) | ((unsigned)ch << 3);
            asc.push_back((unsigned char)(v >> 8)); asc.push_back((unsigned char)v);
            frecuencia = kFrec[sf]; canales = ch;
        }
        datos.insert(datos.end(), f.begin() + p + cab, f.begin() + p + largo);
        tamanios.push_back((unsigned)(largo - cab));
        p += largo;
    }
    if (tamanios.empty()) { if (error) *error = "no es un .aac (ADTS)"; return false; }
    return true;
}

