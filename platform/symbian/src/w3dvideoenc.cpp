// ============================================================================
//  w3dvideoenc.cpp - EL RENDER A VIDEO DEL TELEFONO (lado del exe). El codificador vive en w3dvideo.dll (inc/w3dvideodll.h:
//  por que es una DLL aparte), que se carga con RLibrary recien cuando hace falta; lo que codifica vuelve por un gancho
//  y va al contenedor MP4 (main/io/W3dMp4.h). Este archivo NO importa nada de devvideo.dll. El sonido: el codificador
//  AAC propio (main/io/W3dAac.h).
// ============================================================================
#include <e32base.h>
#include <e32std.h>
#include "w3dlog.h"
#include "w3dvideodll.h"
#include "io/W3dAac.h"
#include <f32file.h>
#include "io/W3dMp4.h"
#include <string>

TInt W3dEscribirArchivo(const char* path, const unsigned char* datos, int len);   // w3dtexload.cpp (RFile)

namespace {
RLibrary gDll;
TInt gCargada = -1;          // -1 sin probar, 0 no cargo, 1 lista
W3dVideoTabla gTabla;

void LogDll(const char* t) { w3dLogf("%s", t); }

TBool Cargar() {
    if (gCargada >= 0) return gCargada == 1;
    gCargada = 0;
    const TInt r = gDll.Load(_L("w3dvideo.dll"));
    if (r != KErrNone) { w3dLogf("[video] w3dvideo.dll no cargo (err %d): sin render a MP4", r); return EFalse; }
    W3dVideoDllTablaFn f = (W3dVideoDllTablaFn)gDll.Lookup(1);
    if (!f) { gDll.Close(); w3dLog("[video] w3dvideo.dll sin su funcion"); return EFalse; }
    Mem::FillZ(&gTabla, sizeof(gTabla));
    f(&gTabla);
    if (gTabla.version != W3D_VIDEO_DLL_VERSION || !gTabla.Abrir) { gDll.Close(); w3dLog("[video] w3dvideo.dll de otra version"); return EFalse; }
    gCargada = 1;
    return ETrue;
}

// el video en curso
struct TVideo {
    void* cod;
    W3dMp4Escritor mp4;
    W3dVideoHost host;
    TBool fallo;
    TVideo() : cod(NULL), fallo(EFalse) {}
};
TVideo* gVideo = NULL;
TBool gOcupado = EFalse;

void Salida(void* ctx, const unsigned char* d, int n) {
    TVideo* v = (TVideo*)ctx;
    if (!v || n <= 0) return;
    if (!v->mp4.Cuadro(d, (size_t)n)) w3dLogf("[video] el MP4 no tomo un pedazo: %s", v->mp4.error.c_str());
}
bool EscribirMp4(const char* ruta, const unsigned char* d, size_t n) { return W3dEscribirArchivo(ruta, d, (int)n) == KErrNone; }
}

// (al arrancar, una vez: Whisk3DContainer) los codificadores del telefono, al log
void W3dVideoEncSondear() {
    if (!Cargar()) return;
    W3dVideoHost h; h.log = LogDll; h.salida = NULL; h.ctx = NULL;
    gTabla.Sondear(&h);
}

// el render del telefono NO dibuja mientras se espera al codificador (su espera anidada deja correr el DrawCallBack)
TBool W3dVideoSymbianOcupado() { return gOcupado; }

// hay codificador de video (H.264 o MPEG-4) en este telefono? (se pregunta una vez)
// los codificadores del telefono: 1 = H.264, 2 = MPEG-4 (se pregunta una vez)
TInt W3dVideoSymbianCodecs() {
    static TInt hay = -1;
    if (hay < 0) hay = Cargar() ? gTabla.Hay() : 0;
    return hay;
}
TBool W3dVideoSymbianHay() { return W3dVideoSymbianCodecs() != 0 ? ETrue : EFalse; }

// un cuadro del render (el primero abre el codificador). false = fallo (el video se corta)
TBool W3dVideoSymbianCuadro(const char* ruta, const unsigned char* rgba, TInt w, TInt h, TInt fps, TInt kbps, TInt codecElegido) {
    if (!Cargar()) return EFalse;
    if (!gVideo) {
        gVideo = new TVideo();
        if (!gVideo) return EFalse;
        gVideo->host.log = LogDll; gVideo->host.salida = Salida; gVideo->host.ctx = gVideo;
        int codec = 0, wr = 0, hr = 0, err = 0;
        gOcupado = ETrue;
        gVideo->cod = gTabla.Abrir(&gVideo->host, w, h, fps, kbps, codecElegido /*0 H.264, 1 MPEG-4*/, &codec, &wr, &hr, &err);
        gOcupado = EFalse;
        if (!gVideo->cod) { w3dLogf("[video] no se pudo abrir el codificador: %d", err); gVideo->fallo = ETrue; return EFalse; }
        if (!gVideo->mp4.Abrir(std::string(ruta), codec == 0 ? W3D_MP4_H264 : W3D_MP4_MPEG4, wr, hr, fps)) { gVideo->fallo = ETrue; return EFalse; }
    }
    if (gVideo->fallo || !gVideo->cod) return EFalse;
    gOcupado = ETrue;
    const int ok = gTabla.Cuadro(gVideo->cod, rgba, w, h);
    gOcupado = EFalse;
    if (!ok) gVideo->fallo = ETrue;
    return ok ? ETrue : EFalse;
}

// ---- EL SONIDO: el WAV del render a AAC con el codificador PROPIO (main/io/W3dAac.h; el conversor de audio del N95
//      no pasa un archivo a AAC: solo graba del microfono) y su pista al MP4 ----
namespace {
// una ruta de Whisk3D (con '/') a la de Symbian (con '\')
void Ruta(TDes& d, const char* s) {
    d.Zero();
    for (const char* p = s; *p && d.Length() < d.MaxLength(); p++) d.Append(*p == '/' ? (TChar)'\\' : (TChar)(TUint8)*p);
}
TBool LeerTodo(const char* ruta, std::vector<unsigned char>& v) {
    RFs fs; if (fs.Connect() != KErrNone) return EFalse;
    TFileName n; Ruta(n, ruta);
    RFile f; TInt tam = 0; TBool ok = EFalse;
    if (f.Open(fs, n, EFileRead | EFileShareReadersOnly) == KErrNone) {
        if (f.Size(tam) == KErrNone && tam > 0) {
            v.resize((size_t)tam);
            TPtr8 p(&v[0], 0, tam);
            ok = (f.Read(p, tam) == KErrNone && p.Length() == tam) ? ETrue : EFalse;
        }
        f.Close();
    }
    static_cast<RHandleBase&>(fs).Close();   // (RFs::Close del SDK nuevo es el ordinal 390 de efsrv: el N95 no lo tiene)
    return ok;
}
// el WAV a AAC (con el log de cuanto tardo)
TBool Codificar(const char* wav, TInt kbps, W3dAacCodificador& cod, int& frec, int& can) {
    std::vector<unsigned char> w;
    if (!wav || !LeerTodo(wav, w)) { w3dLog("[aac] no se pudo leer el WAV del render"); return EFalse; }
    const TUint t0 = User::NTickCount();
    if (!W3dAacDesdeWav(w, kbps, cod, frec, can)) { w3dLogf("[aac] no se pudo codificar: %s", cod.error.c_str()); return EFalse; }
    w3dLogf("[aac] %d muestras a %d Hz, %d canales -> %d cuadros AAC (%d bytes, %d kbps) en %d ms",
            (TInt)cod.muestras, frec, can, (TInt)cod.tamanios.size(), (TInt)cod.datos.size(), kbps, (TInt)(User::NTickCount() - t0));
    return ETrue;
}
}

// el sonido del video en curso: el WAV (16 bits) a AAC de 'kbps', a la pista del MP4. Se llama antes de
// W3dVideoSymbianCerrar. false = el video sale sin sonido (el motivo, al log)
TBool W3dVideoSymbianAudio(const char* wav, TInt frecuencia, TInt kbps) {
    (void)frecuencia;   // (la del WAV)
    if (!gVideo || !gVideo->mp4.Abierto()) return EFalse;
    W3dAacCodificador cod; int frec = 0, can = 0;
    if (!Codificar(wav, kbps, cod, frec, can)) return EFalse;
    gVideo->mp4.AudioAac(cod.asc, cod.datos, cod.tamanios, frec, can, kbps, cod.retardo, cod.muestras);
    return ETrue;
}

// "Renderizar audio" en AAC: el WAV a un .m4a ('salida'). false = no se pudo
TBool W3dAudioSymbianAac(const char* wav, const char* salida, TInt frecuencia, TInt kbps) {
    (void)frecuencia;
    if (!salida) return EFalse;
    W3dAacCodificador cod; int frec = 0, can = 0;
    if (!Codificar(wav, kbps, cod, frec, can)) return EFalse;
    W3dMp4Escritor m4a;
    m4a.Abrir(std::string(salida), W3D_MP4_SOLO_AUDIO, 0, 0, 0);
    m4a.AudioAac(cod.asc, cod.datos, cod.tamanios, frec, can, kbps, cod.retardo, cod.muestras);
    if (!m4a.Cerrar(EscribirMp4)) { w3dLogf("[aac] el .m4a no se escribio: %s", m4a.error.c_str()); return EFalse; }
    return ETrue;
}

// fin del video: lo que queda del codificador y el MP4 al disco
TBool W3dVideoSymbianCerrar() {
    if (!gVideo) return EFalse;
    TBool ok = EFalse;
    if (gVideo->cod) {
        gOcupado = ETrue;
        const int c = gTabla.Cerrar(gVideo->cod);
        gOcupado = EFalse;
        gVideo->cod = NULL;
        ok = c && !gVideo->fallo;
    }
    w3dLogf("[video] al MP4: %d cuadros", gVideo->mp4.Cuadros());
    if (gVideo->mp4.Abierto()) {
        if (!gVideo->mp4.Cerrar(EscribirMp4)) { w3dLogf("[video] el MP4 no se escribio: %s", gVideo->mp4.error.c_str()); ok = EFalse; }
    }
    delete gVideo; gVideo = NULL;
    return ok;
}
