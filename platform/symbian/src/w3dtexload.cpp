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
#include "io/w3dFilesystem.h" // ReadFileBytes: resuelve el contenedor v4 (VFS) y disco

// globales compartidos del modelo de PC (Textures.cpp es PC-only, no se compila en Symbian).
// g_w3dTexturasGen = contador de generacion del cache de texturas (lo incrementa TexturaCache al
// borrar una textura); las RUTINAS (RutinaEditor.cpp) lo consultan para invalidar su cache. Sin
// esta definicion el link del N95 falla con L6218E: Undefined symbol g_w3dTexturasGen.
std::vector<Texture*> Textures;
unsigned g_w3dTexturasGen = 1;

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

    // CONTENEDOR v4: una entrada del zip NO es un archivo del filesystem, asi
    // que FileNewL fallaba y el juego empaquetado quedaba SIN TEXTURAS en el
    // N95 (las mallas si cargaban: van por ReadFileBytes). Se leen los bytes
    // por la abstraccion del Core (resuelve el montaje del contenedor Y los
    // archivos comunes) y se decodifica DE MEMORIA con DataNewL; si la ruta
    // no aparece por ahi, el camino viejo por archivo queda de fallback.
    HBufC8* datos = NULL;
    {
        std::vector<unsigned char> bytes;
        if (w3dFileSystem::ReadFileBytes(aFilename ? aFilename : "", bytes) && !bytes.empty()) {
            datos = HBufC8::NewLC((TInt)bytes.size());
            datos->Des().Copy(&bytes[0], (TInt)bytes.size());
        }
    }
    CImageDecoder* dec = datos ? CImageDecoder::DataNewL(fs, *datos)
                               : CImageDecoder::FileNewL(fs, nombre);
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
    if (datos) CleanupStack::PopAndDestroy(datos);
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

// GLES 1.1 del N95: NPOT es zona gris (el driver puede aceptar la subida pero sin REPEAT ni
// mips, o rechazarla con INVALID_VALUE). Las texturas de MATERIAL necesitan REPEAT (tiling),
// asi que lo que no es potencia de dos se REMUESTREA (vecino mas cercano) al POT siguiente,
// tope 1024 (una foto de 5MP del N95 baja a 1024x1024): los UV 0..1 no cambian.
static unsigned char* ReescalarPOT(const unsigned char* rgba, int w, int h, int& outW, int& outH) {
    int pw = 1; while (pw < w && pw < 1024) { pw <<= 1; }
    int ph = 1; while (ph < h && ph < 1024) { ph <<= 1; }
    outW = pw; outH = ph;
    unsigned char* dst = new unsigned char[(TInt)pw * ph * 4];
    if (!dst) { return NULL; }
    for (int y = 0; y < ph; y++) {
        const unsigned char* fila = rgba + (TInt)(y * h / ph) * w * 4;
        unsigned char* out = dst + (TInt)y * pw * 4;
        for (int x = 0; x < pw; x++) {
            const unsigned char* p = fila + (x * w / pw) * 4;
            out[x*4] = p[0]; out[x*4+1] = p[1]; out[x*4+2] = p[2]; out[x*4+3] = p[3];
        }
    }
    return dst;
}

// LOAD (firma del motor): decode + POT + upload comun + free.
bool LoadTexture(const char* path, unsigned int& outId, int* outW, int* outH) {
    unsigned char* rgba = NULL;
    int w = 0, h = 0;
    if (!DecodeImage(path, &rgba, &w, &h)) { return false; }
    const bool pot = ((w & (w - 1)) == 0) && ((h & (h - 1)) == 0) && w <= 1024 && h <= 1024;
    if (!pot) {
        int pw = 0, ph = 0;
        unsigned char* esc = ReescalarPOT(rgba, w, h, pw, ph);
        FreeImage(rgba);
        if (!esc) { return false; }
        rgba = esc; w = pw; h = ph;
    }
    // limpiar errores GL VIEJOS encolados (p.ej. la miniatura NPOT del file browser): sin esto,
    // el glGetError() de abajo los atribuia a ESTA subida y "Cargar textura" fallaba en SILENCIO
    // (el browser se cerraba y la textura nunca aparecia) aunque el upload hubiera salido bien.
    // el PROYECTO pidio sus texturas a la MITAD en el telefono (TexturasMitadDesde: un proyecto pensado para PC no
    // entra en la memoria). Se informa el tamano de antes (las UV van de 0 a 1: el dibujo no cambia)
    const int origW = w, origH = h;
    const int mitad = TexturasMitadDesde();
    if (mitad > 0 && (w > mitad || h > mitad)) {
        int mw = 0, mh = 0;
        unsigned char* m = ReducirMitadRGBA(rgba, w, h, mw, mh);
        if (m) { FreeImage(rgba); rgba = m; w = mw; h = mh; }
    }
    while (glGetError() != GL_NO_ERROR) {}
    outId = UploadRGBA(rgba, w, h, true, true, false, true);   // (16 bits si entra sin perdida: la mitad de memoria)
    GLenum e = glGetError();
    w3dLogf("LoadTexture: subida %dx%d glErr=%x id=%d", w, h, e, (TInt)outId);
    FreeImage(rgba);
    if (outW) { *outW = origW; }
    if (outH) { *outH = origH; }
    return (e == 0 && outId != 0);
}

// SAVE (firma del motor): EncodePNG (portable, w3dTexture.cpp) + escritura con RFile. En PC/Web
// SavePNG usa stdio (w3dTexture.cpp); aca es el sink de N95. Sin leaves: usa codigos de retorno.
bool SavePNG(const char* path, const unsigned char* rgba, int w, int h, bool flipY) {
    if (!path) { return false; }
    int len = 0;
    unsigned char* png = EncodePNG(rgba, w, h, flipY, &len); // buffer PNG en el heap
    if (!png) { return false; }

    // char* -> TFileName (los slashes de unix -> backslash de Symbian)
    TFileName nombre;
    for (const char* p = path; *p && nombre.Length() < 255; p++) {
        TChar c = (TUint8)*p;
        if (c == '/') { c = '\\'; }
        nombre.Append(c);
    }

    RFs fs;
    TInt err = fs.Connect();
    if (err == KErrNone) {
        fs.MkDirAll(nombre); // crea la carpeta (E:\whisk3d\render\) si no existe; ignora KErrAlreadyExists
        RFile file;
        err = file.Replace(fs, nombre, EFileWrite); // crea/sobrescribe
        if (err == KErrNone) {
            TPtrC8 ptr((const TUint8*)png, len);
            err = file.Write(ptr);
            file.Close();
        }
        FsCloseCompat(fs); // cierre de RFs compatible (fscompat.h)
    }
    w3dLogf("SavePNG: '%s' %dx%d %d bytes err=%d", path, w, h, len, (TInt)err);
    delete[] png;
    return (err == KErrNone);
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
