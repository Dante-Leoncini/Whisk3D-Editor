// ============================================================================
//  w3dvideodll.cpp - w3dvideo.dll: el codificador de video del telefono (DevVideoRecord) para el render a MP4. Ver
//  inc/w3dvideodll.h (por que es una DLL aparte y que exporta).
//
//  Los cuadros RGBA del render van a YUV 4:2:0 y al codificador (H.264 si lo tiene; si no, MPEG-4); lo que devuelve sale
//  por el gancho del exe. El codificador es ASINCRONICO (avisa por el observador): mientras se espera -que termine de
//  inicializar, que devuelva el cuadro, que entregue el final- corre un CActiveSchedulerWait anidado.
//  Sin STL, sin libc, sin datos globales escribibles.
// ============================================================================
#include <e32base.h>
#include <e32std.h>
#include <mmf/devvideo/devvideorecord.h>
#include <mmf/devvideo/devvideobase.h>
#include "w3dvideodll.h"

namespace {
_LIT8(KMimeH264, "video/H264");
_LIT8(KMimeMpeg4, "video/mp4v-es");

// el log del exe, con formato (el de los descriptores de Symbian: %d %u %x %s)
void Log(const W3dVideoHost* host, TRefByValue<const TDesC8> aFmt, ...) {
    if (!host || !host->log) return;
    VA_LIST list;
    VA_START(list, aFmt);
    TBuf8<320> b;
    b.FormatList(aFmt, list);
    VA_END(list);
    host->log((const char*)b.PtrZ());
}

// un descriptor (8 o 16 bits) a texto, cortado
void A8(const TDesC& d, TDes8& out) { out.Zero(); for (TInt i = 0; i < d.Length() && out.Length() < out.MaxLength() - 1; i++) out.Append((TChar)(d[i] & 0xFF)); }

class TMuteObserver : public MMMFDevVideoRecordObserver {
public:
    void MdvroReturnPicture(TVideoPicture*) {}
    void MdvroSupplementalInfoSent() {}
    void MdvroNewBuffers() {}
    void MdvroFatalError(TInt) {}
    void MdvroInitializeComplete(TInt) {}
    void MdvroStreamEnd() {}
};

// ---- los codificadores, al log ----
void PollL(const W3dVideoHost* host) {
    TMuteObserver obs;
    CMMFDevVideoRecord* dv = CMMFDevVideoRecord::NewL(obs);
    CleanupStack::PushL(dv);
    RArray<TUid> list;
    CleanupClosePushL(list);
    dv->GetEncoderListL(list);
    Log(host, _L8("[video] codificadores del telefono: %d"), list.Count());
    for (TInt i = 0; i < list.Count(); i++) {
        CVideoEncoderInfo* inf = NULL;
        TRAPD(err, inf = dv->VideoEncoderInfoLC(list[i]); CleanupStack::Pop(inf));
        if (err != KErrNone || !inf) { Log(host, _L8("[video]  %d: uid %08x (sin info, err %d)"), i, list[i].iUid, err); continue; }
        TBuf8<48> fab; A8(inf->Manufacturer(), fab);
        TBuf8<64> id; A8(inf->Identifier(), id);
        const TSize mx = inf->MaxPictureSize();
        Log(host, _L8("[video]  %d: uid %08x '%S' de '%S' hw=%d max %dx%d %u bps"), i, list[i].iUid, &id, &fab,
            (TInt)inf->Accelerated(), mx.iWidth, mx.iHeight, inf->MaxBitrate());
        Log(host, _L8("[video]     unidades 0x%x encapsulado 0x%x"), (TInt)inf->SupportedDataUnitTypes(),
            (TInt)inf->SupportedDataUnitEncapsulations());
        const RPointerArray<CCompressedVideoFormat>& sal = inf->SupportedOutputFormats();
        for (TInt k = 0; k < sal.Count(); k++) {
            TPtrC8 m = sal[k]->MimeType();
            Log(host, _L8("[video]     sale: %S"), &m);
        }
        const RArray<TUncompressedVideoFormat>& ent = inf->SupportedInputFormats();
        for (TInt k = 0; k < ent.Count(); k++) {
            const TUncompressedVideoFormat& f = ent[k];
            if (f.iDataFormat == EYuvRawData)
                Log(host, _L8("[video]     entra: YUV patron 0x%x orden 0x%x coef 0x%x"), (TInt)f.iYuvFormat.iPattern,
                    (TInt)f.iYuvFormat.iDataLayout, (TInt)f.iYuvFormat.iCoefficients);
            else
                Log(host, _L8("[video]     entra: tipo %d formato 0x%x"), (TInt)f.iDataFormat, (TInt)f.iRgbFormat);
        }
        delete inf;
    }
    CleanupStack::PopAndDestroy(2, dv);   // lista, dv
}
void Poll(const W3dVideoHost* host) {
    TRAPD(err, PollL(host));
    if (err != KErrNone) Log(host, _L8("[video] no se pudo preguntar por los codificadores (err %d)"), err);
}

// un codificador que SALGA en ese formato. Se recorren TODOS y se compara solo el PRINCIPIO del tipo (sin mayusculas):
// el del telefono puede anunciarse con parametros ("video/mp4v-es; profile-level-id=8", "video/H264; profile=...") y la
// busqueda exacta de FindEncodersL no lo encontraba. Primero los de hardware
TBool HasMime(CMMFDevVideoRecord* dv, const TDesC8& aMime, TUid* aUid) {
    RArray<TUid> list;
    TRAPD(err, dv->GetEncoderListL(list));
    TBool has = EFalse;
    TUid best = TUid::Null(); TBool bestHw = EFalse;
    for (TInt i = 0; err == KErrNone && i < list.Count(); i++) {
        CVideoEncoderInfo* inf = NULL;
        TRAPD(e2, inf = dv->VideoEncoderInfoLC(list[i]); CleanupStack::Pop(inf));
        if (e2 != KErrNone || !inf) continue;
        const RPointerArray<CCompressedVideoFormat>& sal = inf->SupportedOutputFormats();
        for (TInt k = 0; k < sal.Count(); k++) {
            const TDesC8& m = sal[k]->MimeType();
            if (m.Length() >= aMime.Length() && m.Left(aMime.Length()).CompareF(aMime) == 0) {
                if (!has || (inf->Accelerated() && !bestHw)) { best = list[i]; bestHw = inf->Accelerated(); }
                has = ETrue;
                break;
            }
        }
        delete inf;
    }
    list.Close();
    if (has && aUid) *aUid = best;
    return has;
}
int Has() {
    TMuteObserver obs;
    CMMFDevVideoRecord* dv = NULL;
    TRAPD(err, dv = CMMFDevVideoRecord::NewL(obs));
    if (err != KErrNone || !dv) return 0;
    // (1 = H.264, 2 = MPEG-4: los dos que se ofrecen en la tarjeta)
    const int has = (HasMime(dv, KMimeH264, NULL) ? 1 : 0) | (HasMime(dv, KMimeMpeg4, NULL) ? 2 : 0);
    delete dv;
    return has;
}

// ---- el codificador ----
class CEncoder : public CBase, public MMMFDevVideoRecordObserver {
public:
    const W3dVideoHost* iHost;
    CMMFDevVideoRecord* iDv;
    THwDeviceId iHw;
    CActiveSchedulerWait* iWait;
    TInt iStateInit;
    TBool iReturnedFrame, iEnd, iWaiting;
    TInt iError;
    TVideoPicture iFrame;
    HBufC8* iYuv;
    TPtr8* iYuvPtr;
    TInt iW, iH, iFps, iN, iOutputs;

    CEncoder(const W3dVideoHost* aHost) : iHost(aHost), iDv(NULL), iHw(0), iWait(NULL), iStateInit(KRequestPending),
        iReturnedFrame(ETrue), iEnd(EFalse), iWaiting(EFalse), iError(KErrNone), iYuv(NULL), iYuvPtr(NULL),
        iW(0), iH(0), iFps(25), iN(0), iOutputs(0) {}
    ~CEncoder() {
        delete iDv;
        delete iWait;
        delete iYuvPtr;
        delete iYuv;
    }
    void MdvroReturnPicture(TVideoPicture*) { iReturnedFrame = ETrue; Wake(); }
    void MdvroSupplementalInfoSent() {}
    void MdvroNewBuffers() { Remove(); Wake(); }
    void MdvroFatalError(TInt aError) { iError = aError; Log(iHost, _L8("[video] error fatal del codificador: %d"), aError); Wake(); }
    void MdvroInitializeComplete(TInt aError) { iStateInit = aError; Wake(); }
    void MdvroStreamEnd() { iEnd = ETrue; Wake(); }

    void Wake() { if (iWaiting && iWait && iWait->IsStarted()) { iWaiting = EFalse; iWait->AsyncStop(); } }
    void Wait() { iWaiting = ETrue; iWait->Start(); }
    void Remove() {
        if (!iDv) return;
        for (;;) {
            TVideoOutputBuffer* b = NULL;
            TRAPD(err, b = iDv->NextBufferL());
            if (err != KErrNone || !b) break;
            const TPtrC8& d = b->iData;
            if (d.Length() > 0) {
                if (iOutputs < 3)
                    Log(iHost, _L8("[video] salida %d: %d bytes, empieza %02x %02x %02x %02x %02x"), iOutputs, d.Length(),
                        d.Length() > 0 ? d[0] : 0, d.Length() > 1 ? d[1] : 0, d.Length() > 2 ? d[2] : 0,
                        d.Length() > 3 ? d[3] : 0, d.Length() > 4 ? d[4] : 0);
                iOutputs++;
                if (iHost && iHost->output) iHost->output(iHost->ctx, d.Ptr(), d.Length());
            }
            iDv->ReturnBuffer(b);
        }
    }

    void OpenL(TInt aW, TInt aH, TInt aFps, TInt aKbps, TInt aPreferred, TInt& aCodec) {
        iWait = new (ELeave) CActiveSchedulerWait;
        iDv = CMMFDevVideoRecord::NewL(*this);
        TUid uid;
        TBool h264 = EFalse;
        if (aPreferred != 1 && HasMime(iDv, KMimeH264, &uid)) h264 = ETrue;
        else if (!HasMime(iDv, KMimeMpeg4, &uid)) {
            if (!HasMime(iDv, KMimeH264, &uid)) User::Leave(KErrNotSupported);
            h264 = ETrue;
        }
        aCodec = h264 ? 0 : 1;
        iW = aW & ~15; iH = aH & ~15; iFps = aFps > 0 ? aFps : 25;   // (multiplos de 16: lo que sobra se recorta)
        if (iW < 16 || iH < 16) User::Leave(KErrArgument);
        Log(iHost, _L8("[video] abrir %S %dx%d %d fps %d kbps (codificador %08x)"), h264 ? &KMimeH264() : &KMimeMpeg4(),
            iW, iH, iFps, aKbps, uid.iUid);
        iHw = iDv->SelectEncoderL(uid);
        CCompressedVideoFormat* fmt = CCompressedVideoFormat::NewL(h264 ? KMimeH264() : KMimeMpeg4());
        CleanupStack::PushL(fmt);
        iDv->SetOutputFormatL(iHw, *fmt, EDuCodedPicture, EDuElementaryStream, EFalse);
        CleanupStack::PopAndDestroy(fmt);
        TUncompressedVideoFormat ent;
        ent.iDataFormat = EYuvRawData;
        ent.iYuvFormat.iCoefficients = EYuvBt601Range0;
        ent.iYuvFormat.iPattern = EYuv420Chroma1;
        ent.iYuvFormat.iDataLayout = EYuvDataPlanar;
        ent.iYuvFormat.iYuv2RgbMatrix = NULL;
        ent.iYuvFormat.iRgb2YuvMatrix = NULL;
        ent.iYuvFormat.iAspectRatioNum = 1;
        ent.iYuvFormat.iAspectRatioDenom = 1;
        iDv->SetInputFormatL(iHw, ent, TSize(iW, iH));
        iDv->SetSourceMemoryL((TReal)iFps, ETrue, EFalse);   // (no es en vivo: cada cuadro tarda lo que tarde)
        iDv->SetMinRandomAccessRate(1.0);                       // (un cuadro clave por segundo)
        iDv->Initialize();
        if (iStateInit == KRequestPending) Wait();
        if (iStateInit != KErrNone) { Log(iHost, _L8("[video] no inicializo: %d"), iStateInit); User::Leave(iStateInit); }
        TRateControlOptions rc;
        rc.iControl = EBrControlStream;
        rc.iBitrate = (TUint)aKbps * 1000;
        rc.iPictureQuality = 75;
        rc.iPictureRate = (TReal)iFps;
        rc.iQualityTemporalTradeoff = 0.5;
        rc.iLatencyQualityTradeoff = 1.0;
        iDv->SetRateControlOptions(0, rc);
        iDv->Start();
        const TInt n = iW * iH * 3 / 2;
        iYuv = HBufC8::NewL(n);
        iYuvPtr = new (ELeave) TPtr8(iYuv->Des());
        iYuvPtr->SetLength(n);
    }

    // RGBA (de arriba hacia abajo) -> YUV 4:2:0 plano (BT.601, rango limitado), recortado a iW x iH
    void Convert(const unsigned char* rgba, int w, int h) {
        TUint8* Y = (TUint8*)iYuvPtr->Ptr();
        TUint8* U = Y + iW * iH;
        TUint8* V = U + (iW / 2) * (iH / 2);
        for (int y = 0; y < iH; y++) {
            const unsigned char* row = rgba + (y < h ? y : h - 1) * w * 4;
            for (int x = 0; x < iW; x++) {
                const unsigned char* p = row + (x < w ? x : w - 1) * 4;
                Y[y * iW + x] = (TUint8)(((66 * p[0] + 129 * p[1] + 25 * p[2] + 128) >> 8) + 16);
            }
        }
        for (int y = 0; y < iH / 2; y++)
            for (int x = 0; x < iW / 2; x++) {
                int r = 0, g = 0, b = 0;
                for (int dy = 0; dy < 2; dy++) for (int dx = 0; dx < 2; dx++) {
                    const int sx = x * 2 + dx < w ? x * 2 + dx : w - 1, sy = y * 2 + dy < h ? y * 2 + dy : h - 1;
                    const unsigned char* p = rgba + (sy * w + sx) * 4;
                    r += p[0]; g += p[1]; b += p[2];
                }
                r >>= 2; g >>= 2; b >>= 2;
                U[y * (iW / 2) + x] = (TUint8)(((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128);
                V[y * (iW / 2) + x] = (TUint8)(((112 * r - 94 * g - 18 * b + 128) >> 8) + 128);
            }
    }

    TBool FrameL(const unsigned char* rgba, int w, int h) {
        while (!iReturnedFrame && iError == KErrNone) Wait();   // (el anterior todavia lo esta leyendo)
        if (iError != KErrNone) return EFalse;
        Convert(rgba, w, h);
        iFrame.iData.iDataFormat = EYuvRawData;
        iFrame.iData.iDataSize = TSize(iW, iH);
        iFrame.iData.iRawData = iYuvPtr;
        iFrame.iOptions = TVideoPicture::ETimestamp;
        iFrame.iTimestamp = TTimeIntervalMicroSeconds((TInt64)iN * 1000000 / iFps);
        iReturnedFrame = EFalse;
        iDv->WritePictureL(&iFrame);
        iN++;
        while (!iReturnedFrame && iError == KErrNone) Wait();   // (el buffer se reusa en el que sigue)
        Remove();
        return iError == KErrNone;
    }

    TBool CloseL() {
        if (iDv && iError == KErrNone) {
            iDv->InputEnd();
            while (!iEnd && iError == KErrNone) Wait();
            Remove();
            iDv->Stop();
        }
        Log(iHost, _L8("[video] cerrar: %d cuadros entraron, %d salidas (err %d)"), iN, iOutputs, iError);
        return iError == KErrNone;
    }
};

void* Open(const W3dVideoHost* host, int w, int h, int fps, int kbps, int preferred, int* codec, int* wReal, int* hReal,
            int* error) {
    CEncoder* c = new CEncoder(host);
    if (!c) { if (error) *error = KErrNoMemory; return NULL; }
    TInt cod = 0;
    TRAPD(err, c->OpenL(w, h, fps, kbps, preferred, cod));
    if (error) *error = err;
    if (err != KErrNone) { delete c; return NULL; }
    if (codec) *codec = cod;
    if (wReal) *wReal = c->iW;
    if (hReal) *hReal = c->iH;
    return c;
}
int Frame(void* cod, const unsigned char* rgba, int w, int h) {
    CEncoder* c = (CEncoder*)cod;
    if (!c || !rgba) return 0;
    TBool ok = EFalse;
    TRAPD(err, ok = c->FrameL(rgba, w, h));
    if (err != KErrNone) Log(c->iHost, _L8("[video] cuadro %d: err %d"), c->iN, err);
    return (err == KErrNone && ok) ? 1 : 0;
}
int Close(void* cod) {
    CEncoder* c = (CEncoder*)cod;
    if (!c) return 0;
    TBool ok = EFalse;
    TRAPD(err, ok = c->CloseL());
    delete c;
    return (err == KErrNone && ok) ? 1 : 0;
}

}

// ordinal 1: llena la tabla del exe (sin datos globales en la DLL)
EXPORT_C void W3dVideoDllTable(W3dVideoTable* t) {
    if (!t) return;
    t->version = W3D_VIDEO_DLL_VERSION;
    t->Poll    = Poll; t->Has = Has; t->Open = Open; t->Frame = Frame; t->Close = Close;
}
