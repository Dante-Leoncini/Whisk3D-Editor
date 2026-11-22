// ============================================================================
//  w3dvolumendll.cpp - w3dvolumen.dll: LAS TECLAS DE VOLUMEN del costado del telefono. En S60 no llegan como teclas
//  comunes: las reparte el sistema por RemCon (el control remoto: la misma via que un auricular Bluetooth). Va en una
//  DLL aparte, como el codificador de video (inc/w3dvideodll.h): el SDK es mas nuevo que el del N95 y si le faltara
//  algo de remcon, ESTA no carga y Whisk3D sigue andando (sin las teclas de volumen).
//  Exporta una funcion (ordinal 1): Open(gancho) -> cada toque llama hook(+1) o hook(-1). Sin datos globales.
// ============================================================================
#include <e32base.h>
#include <remconinterfaceselector.h>
#include <remconcoreapitarget.h>
#include <remconcoreapitargetobserver.h>

namespace {
class CVolume : public CBase, public MRemConCoreApiTargetObserver {
public:
    CRemConInterfaceSelector* iSel;
    CRemConCoreApiTarget* iTarget;   // (es del selector: no se borra aparte)
    void (*iHook)(int dir);
    CVolume() : iSel(NULL), iTarget(NULL), iHook(NULL) {}
    ~CVolume() { delete iSel; }
    void BuildL() {
        iSel = CRemConInterfaceSelector::NewL();
        iTarget = CRemConCoreApiTarget::NewL(*iSel, *this);
        iSel->OpenTargetL();
    }
    // un toque (o el apretar de una tecla mantenida): un paso
    void MrccatoCommand(TRemConCoreApiOperationId aOp, TRemConCoreApiButtonAction aAction) {
        if (aAction == ERemConCoreApiButtonRelease || !iHook) return;
        if (aOp == ERemConCoreApiVolumeUp) iHook(+1);
        else if (aOp == ERemConCoreApiVolumeDown) iHook(-1);
    }
};

void* Open(void (*hook)(int dir)) {
    CVolume* v = new CVolume();
    if (!v) return NULL;
    v->iHook = hook;
    TRAPD(err, v->BuildL());
    if (err != KErrNone) { delete v; return NULL; }
    return v;
}
void Close(void* h) { delete (CVolume*)h; }
}

struct W3dVolumeTable { int version; void* (*Open)(void (*hook)(int)); void (*Close)(void*); };
EXPORT_C void W3dVolumeDllTable(W3dVolumeTable* t) {
    if (!t) return;
    t->version = 1; t->Open = Open; t->Close = Close;
}
