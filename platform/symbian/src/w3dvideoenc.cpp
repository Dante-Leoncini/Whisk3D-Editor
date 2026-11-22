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

TInt W3dWriteFile(const char* path, const unsigned char* data, int len);   // w3dtexload.cpp (RFile)

namespace {
RLibrary gDll;
TInt gLoaded = -1;          // -1 sin probar, 0 no cargo, 1 lista
W3dVideoTable gTable;

void LogDll(const char* t) { w3dLogf("%s", t); }

TBool Load() {
    if (gLoaded >= 0) return gLoaded == 1;
    gLoaded = 0;
    const TInt r = gDll.Load(_L("w3dvideo.dll"));
    if (r != KErrNone) { w3dLogf("[video] w3dvideo.dll no cargo (err %d): sin render a MP4", r); return EFalse; }
    W3dVideoDllTableFn f = (W3dVideoDllTableFn)gDll.Lookup(1);
    if (!f) { gDll.Close(); w3dLog("[video] w3dvideo.dll sin su funcion"); return EFalse; }
    Mem::FillZ(&gTable, sizeof(gTable));
    f(&gTable);
    if (gTable.version != W3D_VIDEO_DLL_VERSION || !gTable.Open) { gDll.Close(); w3dLog("[video] w3dvideo.dll de otra version"); return EFalse; }
    gLoaded = 1;
    return ETrue;
}

// el video en curso
struct TVideo {
    void* cod;
    W3dMp4Writer mp4;
    W3dVideoHost host;
    TBool failed;
    TVideo() : cod(NULL), failed(EFalse) {}
};
TVideo* gVideo = NULL;
TBool gBusy    = EFalse;

void Output(void* ctx, const unsigned char* d, int n) {
    TVideo* v = (TVideo*)ctx;
    if (!v || n <= 0) return;
    if (!v->mp4.Frame(d, (size_t)n)) w3dLogf("[video] el MP4 no tomo un pedazo: %s", v->mp4.error.c_str());
}
bool WriteMp4(const char* path, const unsigned char* d, size_t n) { return W3dWriteFile(path, d, (int)n) == KErrNone; }
}

// (al arrancar, una vez: Whisk3DContainer) los codificadores del telefono, al log
void W3dVideoEncPoll() {
    if (!Load()) return;
    W3dVideoHost h; h.log = LogDll; h.output = NULL; h.ctx = NULL;
    gTable.Poll(&h);
}

// el render del telefono NO dibuja mientras se espera al codificador (su espera anidada deja correr el DrawCallBack)
TBool W3dBusyVideoSymbian() { return gBusy; }

// hay codificador de video (H.264 o MPEG-4) en este telefono? (se pregunta una vez)
// los codificadores del telefono: 1 = H.264, 2 = MPEG-4 (se pregunta una vez)
TInt W3dVideoSymbianCodecs() {
    static TInt has = -1;
    if (has < 0) has = Load() ? gTable.Has() : 0;
    return has;
}
TBool W3dVideoSymbianHas() { return W3dVideoSymbianCodecs() != 0 ? ETrue : EFalse; }

// un cuadro del render (el primero abre el codificador). false = fallo (el video se corta)
TBool W3dVideoSymbianFrame(const char* path, const unsigned char* rgba, TInt w, TInt h, TInt fps, TInt kbps, TInt pickedCodec) {
    if (!Load()) return EFalse;
    if (!gVideo) {
        gVideo = new TVideo();
        if (!gVideo) return EFalse;
        gVideo->host.log = LogDll; gVideo->host.output = Output; gVideo->host.ctx = gVideo;
        int codec = 0, wr = 0, hr = 0, err = 0;
        gBusy = ETrue;
        gVideo->cod = gTable.Open(&gVideo->host, w, h, fps, kbps, pickedCodec /*0 H.264, 1 MPEG-4*/, &codec, &wr, &hr, &err);
        gBusy = EFalse;
        if (!gVideo->cod) { w3dLogf("[video] no se pudo abrir el codificador: %d", err); gVideo->failed = ETrue; return EFalse; }
        if (!gVideo->mp4.Open(std::string(path), codec == 0 ? W3D_MP4_H264 : W3D_MP4_MPEG4, wr, hr, fps)) { gVideo->failed = ETrue; return EFalse; }
    }
    if (gVideo->failed || !gVideo->cod) return EFalse;
    gBusy = ETrue;
    const int ok = gTable.Frame(gVideo->cod, rgba, w, h);
    gBusy = EFalse;
    if (!ok) gVideo->failed = ETrue;
    return ok ? ETrue : EFalse;
}

// ---- EL SONIDO: el WAV del render a AAC con el codificador PROPIO (main/io/W3dAac.h; el conversor de audio del N95
//      no pasa un archivo a AAC: solo graba del microfono) y su pista al MP4 ----
namespace {
// una ruta de Whisk3D (con '/') a la de Symbian (con '\')
void Path(TDes& d, const char* s) {
    d.Zero();
    for (const char* p = s; *p && d.Length() < d.MaxLength(); p++) d.Append(*p == '/' ? (TChar)'\\' : (TChar)(TUint8)*p);
}
TBool ReadAll(const char* path, std::vector<unsigned char>& v) {
    RFs fs; if (fs.Connect() != KErrNone) return EFalse;
    TFileName n; Path(n, path);
    RFile f; TInt size = 0; TBool ok = EFalse;
    if (f.Open(fs, n, EFileRead | EFileShareReadersOnly) == KErrNone) {
        if (f.Size(size) == KErrNone && size > 0) {
            v.resize((size_t)size);
            TPtr8 p(&v[0], 0, size);
            ok = (f.Read(p, size) == KErrNone && p.Length() == size) ? ETrue : EFalse;
        }
        f.Close();
    }
    static_cast<RHandleBase&>(fs).Close();   // (RFs::Close del SDK nuevo es el ordinal 390 de efsrv: el N95 no lo tiene)
    return ok;
}
// el WAV a AAC (con el log de cuanto tardo)
TBool Encode(const char* wav, TInt kbps, W3dAacEncoder& cod, int& freq, int& can) {
    std::vector<unsigned char> w;
    if (!wav || !ReadAll(wav, w)) { w3dLog("[aac] no se pudo leer el WAV del render"); return EFalse; }
    const TUint t0 = User::NTickCount();
    if (!W3dAacFromWav(w, kbps, cod, freq, can)) { w3dLogf("[aac] no se pudo codificar: %s", cod.error.c_str()); return EFalse; }
    w3dLogf("[aac] %d muestras a %d Hz, %d canales -> %d cuadros AAC (%d bytes, %d kbps) en %d ms",
            (TInt)cod.samples, freq, can, (TInt)cod.sizes.size(), (TInt)cod.data.size(), kbps, (TInt)(User::NTickCount() - t0));
    return ETrue;
}
}

// el sonido del video en curso: el WAV (16 bits) a AAC de 'kbps', a la pista del MP4. Se llama antes de
// W3dVideoSymbianClose. false = el video sale sin sonido (el motivo, al log)
TBool W3dVideoSymbianAudio(const char* wav, TInt frequency, TInt kbps) {
    (void)frequency;   // (la del WAV)
    if (!gVideo || !gVideo->mp4.IsOpen()) return EFalse;
    W3dAacEncoder cod; int freq = 0, can = 0;
    if (!Encode(wav, kbps, cod, freq, can)) return EFalse;
    gVideo->mp4.AudioAac(cod.asc, cod.data, cod.sizes, freq, can, kbps, cod.delay, cod.samples);
    return ETrue;
}

// "Renderizar audio" en AAC: el WAV a un .m4a ('salida'). false = no se pudo
TBool W3dAudioSymbianAac(const char* wav, const char* output, TInt frequency, TInt kbps) {
    (void)frequency;
    if (!output) return EFalse;
    W3dAacEncoder cod; int freq = 0, can = 0;
    if (!Encode(wav, kbps, cod, freq, can)) return EFalse;
    W3dMp4Writer m4a;
    m4a.Open(std::string(output), W3D_MP4_ONLY_AUDIO, 0, 0, 0);
    m4a.AudioAac(cod.asc, cod.data, cod.sizes, freq, can, kbps, cod.delay, cod.samples);
    if (!m4a.Close(WriteMp4)) { w3dLogf("[aac] el .m4a no se escribio: %s", m4a.error.c_str()); return EFalse; }
    return ETrue;
}

// fin del video: lo que queda del codificador y el MP4 al disco
TBool W3dVideoSymbianClose() {
    if (!gVideo) return EFalse;
    TBool ok = EFalse;
    if (gVideo->cod) {
        gBusy = ETrue;
        const int c = gTable.Close(gVideo->cod);
        gBusy = EFalse;
        gVideo->cod = NULL;
        ok = c && !gVideo->failed;
    }
    w3dLogf("[video] al MP4: %d cuadros", gVideo->mp4.Frames());
    if (gVideo->mp4.IsOpen()) {
        if (!gVideo->mp4.Close(WriteMp4)) { w3dLogf("[video] el MP4 no se escribio: %s", gVideo->mp4.error.c_str()); ok = EFalse; }
    }
    delete gVideo; gVideo = NULL;
    return ok;
}
