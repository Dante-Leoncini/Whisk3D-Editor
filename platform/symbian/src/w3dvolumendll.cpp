// ============================================================================
//  w3dvolumendll.cpp - w3dvolumen.dll: LAS TECLAS DE VOLUMEN del costado del telefono. En S60 no llegan como teclas
//  comunes: las reparte el sistema por RemCon (el control remoto: la misma via que un auricular Bluetooth). Va en una
//  DLL aparte, como el codificador de video (inc/w3dvideodll.h): el SDK es mas nuevo que el del N95 y si le faltara
//  algo de remcon, ESTA no carga y Whisk3D sigue andando (sin las teclas de volumen).
//  Exporta una funcion (ordinal 1): Abrir(gancho) -> cada toque llama gancho(+1) o gancho(-1). Sin datos globales.
// ============================================================================
#include <e32base.h>
#include <remconinterfaceselector.h>
#include <remconcoreapitarget.h>
#include <remconcoreapitargetobserver.h>

namespace {
class CVolumen : public CBase, public MRemConCoreApiTargetObserver {
public:
    CRemConInterfaceSelector* iSel;
    CRemConCoreApiTarget* iTarget;   // (es del selector: no se borra aparte)
    void (*iGancho)(int dir);
    CVolumen() : iSel(NULL), iTarget(NULL), iGancho(NULL) {}
    ~CVolumen() { delete iSel; }
    void ConstruirL() {
        iSel = CRemConInterfaceSelector::NewL();
        iTarget = CRemConCoreApiTarget::NewL(*iSel, *this);
        iSel->OpenTargetL();
    }
    // un toque (o el apretar de una tecla mantenida): un paso
    void MrccatoCommand(TRemConCoreApiOperationId aOp, TRemConCoreApiButtonAction aAccion) {
        if (aAccion == ERemConCoreApiButtonRelease || !iGancho) return;
        if (aOp == ERemConCoreApiVolumeUp) iGancho(+1);
        else if (aOp == ERemConCoreApiVolumeDown) iGancho(-1);
    }
};

void* Abrir(void (*gancho)(int dir)) {
    CVolumen* v = new CVolumen();
    if (!v) return NULL;
    v->iGancho = gancho;
    TRAPD(err, v->ConstruirL());
    if (err != KErrNone) { delete v; return NULL; }
    return v;
}
void Cerrar(void* h) { delete (CVolumen*)h; }
}

struct W3dVolumenTabla { int version; void* (*Abrir)(void (*gancho)(int)); void (*Cerrar)(void*); };
EXPORT_C void W3dVolumenDllTabla(W3dVolumenTabla* t) {
    if (!t) return;
    t->version = 1; t->Abrir = Abrir; t->Cerrar = Cerrar;
}
