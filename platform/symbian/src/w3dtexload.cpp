/*
 * ==============================================================================
 *  w3dtexload.cpp — LoadTexture COMPARTIDO (firma de PC) para Symbian
 *
 *  El importador de PC (main/importers/import_obj.cpp) llama a
 *  LoadTexture(path, GLuint&) para las texturas del MTL. En PC/Android lo
 *  resuelve SDL/stb; aca lo resuelve ICL (CImageDecoder) de forma
 *  SINCRONICA con CActiveSchedulerWait, y se sube a GL con filtros (sin
 *  filtros la textura queda incompleta en GLES y no dibuja).
 *
 *  Tambien define el vector global compartido 'Textures' (en PC lo define
 *  core/objects/Textures.cpp, que no se compila en Symbian).
 * ==============================================================================
 */

#include <e32base.h>
#include <fbs.h>
#include <imageconversion.h>
#include <GLES/gl.h>
#include <string>
#include <vector>

#include "fscompat.h"
#include "w3dlog.h"
#include "objects/Textures.h"
#include "w3dTexture.h" // engine: UploadRGBA (subida comun a los 4 OS)

// global compartido del modelo de PC (Textures.cpp es PC-only)
std::vector<Texture*> Textures;

namespace {

// espera sincronica de un Convert del decoder (el import corre en el hilo
// de UI con el scheduler activo: el wait anidado bombea los AOs del ICL)
class CEsperaDecode : public CActive {
    public:
        CActiveSchedulerWait iWait;

        CEsperaDecode() : CActive(EPriorityStandard) {
            CActiveScheduler::Add(this);
        }
        ~CEsperaDecode() {
            Cancel();
        }
        void Esperar() {
            SetActive();
            iWait.Start();
        }
        void RunL() {
            if (iWait.IsStarted()) iWait.AsyncStop();
        }
        void DoCancel() {
            if (iWait.IsStarted()) iWait.AsyncStop();
        }
};

} // namespace

// DECODE puro: imagen de disco -> pixeles RGBA en el heap (new[]), via ICL.
// NO sube a GL. El buffer se devuelve por *aRgba (el que llama lo libera con
// w3dEngine::FreeImage). Despues de asignar *aRgba no hay llamadas que dejen
// (leave), asi que el buffer no necesita ir al CleanupStack.
static void DecodeImageSymbianL(const char* aFilename,
                                unsigned char** aRgba, TInt& aW, TInt& aH) {
    *aRgba = NULL; aW = 0; aH = 0;

    TFileName nombre;
    for (const char* p = aFilename; *p && nombre.Length() < 255; p++) {
        TChar c = (TUint8)*p;
        if (c == '/') { c = '\\'; } // el MTL puede traer slashes de unix
        nombre.Append(c);
    }

    RFs fs;
    User::LeaveIfError(fs.Connect());
    CleanupCloseFsPushL(fs);

    CImageDecoder* dec = CImageDecoder::FileNewL(fs, nombre);
    CleanupStack::PushL(dec);

    const TFrameInfo& info = dec->FrameInfo();
    TInt w = info.iOverallSizeInPixels.iWidth;
    TInt h = info.iOverallSizeInPixels.iHeight;
    TBool alpha = (info.iFlags & TFrameInfo::ETransparencyPossible) != 0;

    CFbsBitmap* bmp = new (ELeave) CFbsBitmap();
    CleanupStack::PushL(bmp);
    User::LeaveIfError(bmp->Create(TSize(w, h), EColor16MU));

    CFbsBitmap* mask = NULL;
    if (alpha) {
        mask = new (ELeave) CFbsBitmap();
        CleanupStack::PushL(mask);
        User::LeaveIfError(mask->Create(TSize(w, h), EGray256));
    }

    CEsperaDecode* espera = new (ELeave) CEsperaDecode();
    CleanupStack::PushL(espera);
    if (mask) {
        dec->Convert(&espera->iStatus, *bmp, *mask, 0);
    } else {
        dec->Convert(&espera->iStatus, *bmp, 0);
    }
    espera->Esperar();
    TInt st = espera->iStatus.Int();
    char nom8[256];
    TInt nlen = nombre.Length() > 255 ? 255 : nombre.Length();
    for (TInt i = 0; i < nlen; i++) { nom8[i] = (char)nombre[i]; }
    nom8[nlen] = 0;
    w3dLogf("DecodeImage: '%s' %dx%d alpha=%d status=%d", nom8, w, h, (int)alpha, st);

    if (st == KErrNone && w > 0 && h > 0) {
        unsigned char* rgba = new (ELeave) unsigned char[w * h * 4];

        // alpha primero (a un buffer aparte para no anidar locks del FBS)
        if (mask) {
            mask->LockHeap();
            const TUint8* msrc = (const TUint8*)mask->DataAddress();
            TInt mstride = mask->DataStride();
            for (TInt y = 0; y < h; y++) {
                for (TInt x = 0; x < w; x++) {
                    rgba[(y * w + x) * 4 + 3] = msrc[y * mstride + x];
                }
            }
            mask->UnlockHeap();
        } else {
            for (TInt i = 0; i < w * h; i++) {
                rgba[i * 4 + 3] = 255;
            }
        }

        bmp->LockHeap();
        const TUint32* src = (const TUint32*)bmp->DataAddress();
        TInt strideW = bmp->DataStride() / 4;
        for (TInt y = 0; y < h; y++) {
            for (TInt x = 0; x < w; x++) {
                TUint32 px = src[y * strideW + x]; // EColor16MU = XRGB
                TUint8* d = &rgba[(y * w + x) * 4];
                d[0] = (TUint8)((px >> 16) & 0xFF);
                d[1] = (TUint8)((px >> 8) & 0xFF);
                d[2] = (TUint8)(px & 0xFF);
            }
        }
        bmp->UnlockHeap();

        *aRgba = rgba; aW = w; aH = h;
    }

    CleanupStack::PopAndDestroy(espera);
    if (mask) {
        CleanupStack::PopAndDestroy(mask);
    }
    CleanupStack::PopAndDestroy(bmp);
    CleanupStack::PopAndDestroy(dec);
    CleanupStack::PopAndDestroy(); // fs
}

namespace w3dEngine {

// DECODE (firma del motor): wrapper TRAP del decode ICL de arriba.
bool DecodeImage(const char* path, unsigned char** outRGBA, int* outW, int* outH) {
    if (!outRGBA) { return false; }
    *outRGBA = NULL;
    TInt w = 0, h = 0;
    unsigned char* rgba = NULL;
    TRAPD(err, DecodeImageSymbianL(path, &rgba, w, h));
    if (err != KErrNone || !rgba) {
        w3dLogf("DecodeImage: leave/fallo %d", err);
        if (rgba) { delete[] rgba; }
        return false;
    }
    *outRGBA = rgba;
    if (outW) { *outW = (int)w; }
    if (outH) { *outH = (int)h; }
    return true;
}

// LOAD (firma del motor): decode + upload comun + free.
bool LoadTexture(const char* path, unsigned int& outId, int* outW, int* outH) {
    unsigned char* rgba = NULL;
    int w = 0, h = 0;
    if (!DecodeImage(path, &rgba, &w, &h)) { return false; }
    outId = UploadRGBA(rgba, w, h, true);
    GLenum e = glGetError(); // GLES1 del N95 exige potencia de dos
    w3dLogf("LoadTexture: subida %dx%d glErr=%x id=%d", w, h, e, (TInt)outId);
    FreeImage(rgba);
    if (outW) { *outW = w; }
    if (outH) { *outH = h; }
    return (e == 0 && outId != 0);
}

} // namespace w3dEngine

// firma vieja (core/objects/Textures.h) que usan el importador OBJ y el
// selector de texturas: delega en el cargador del motor.
bool LoadTexture(const char* filename, GLuint &textureID) {
    unsigned int id = 0;
    if (!w3dEngine::LoadTexture(filename, id)) {
        textureID = 0;
        return false;
    }
    textureID = id;
    return true;
}
