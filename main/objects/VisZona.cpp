// ============================================================================
//  VisZona.cpp — ver VisZona.h. La zona elige la celda; la aplica por el hook
//  del bind setSector (W3dPVSSetSectorHook, instalado por el editor): esta
//  clase no conoce Mesh ni MeshEdit.
// ============================================================================
#include "VisZona.h"
#include "Camera.h"   // multi-riel: CameraActive->Riel (por que riel viaja la camara)
#include <math.h>

// el hook del bind setSector (definido en BindsJuego.cpp, instalado por MeshEdit.cpp)
extern bool (*W3dPVSSetSectorHook)(Object*, int);

Object* VisZona::Objetivo() const {
    if (objetivoNombre.empty() || !SceneCollection) return NULL;
    return FindObjectByName(SceneCollection, objetivoNombre);
}

Object* VisZona::Ancla() const {
    if (anclaObj) return anclaObj;
    if (anclaNombre.empty() || !SceneCollection) return NULL;
    return FindObjectByName(SceneCollection, anclaNombre);
}

// posicion 'p' (mundo) -> celda 1-based de la grilla local de la zona.
// 0 = fuera de la grilla (fallback: malla completa, nunca un pozo negro).
int VisZona::CeldaGrilla(const Vector3& p) const {
    Matrix4 W; GetWorldMatrix(W);
    Matrix4 inv;
    if (!Matrix4::InvertirAfin(W, inv)) inv = W.Inverse();
    Vector3 l = inv * p;
    if (tamCelda <= 1e-6f) return 0;
    int ix = (int)floorf(l.x / tamCelda);
    int iy = (int)floorf(l.y / tamCelda);
    int iz = (int)floorf(l.z / tamCelda);
    if (ix < 0 || ix >= nx || iy < 0 || iy >= ny || iz < 0 || iz >= nz) return 0;
    return 1 + ix + iy * nx + iz * nx * ny;
}

// posicion 'p' (mundo) -> indice 1-based del PRIMER hijo cuya caja unitaria
// [-1,1] (escalada/rotada por su transform, como un Empty) contiene a 'p'.
int VisZona::CeldaVolumenes(const Vector3& p) const {
    for (size_t i = 0; i < Childrens.size(); i++) {
        Object* h = Childrens[i];
        if (!h || !h->visible) continue;
        Matrix4 W; h->GetWorldMatrix(W);
        Matrix4 inv;
        if (!Matrix4::InvertirAfin(W, inv)) inv = W.Inverse();
        Vector3 l = inv * p;
        if (l.x >= -1.0f && l.x <= 1.0f && l.y >= -1.0f && l.y <= 1.0f &&
            l.z >= -1.0f && l.z <= 1.0f) return (int)i + 1;
    }
    return 0;
}

void VisZona::Aplicar(int celda) {
    if (celda == celdaActual) return;
    celdaActual = celda;
    Object* obj = Objetivo();
    if (obj && W3dPVSSetSectorHook) W3dPVSSetSectorHook(obj, celda);
}

// MULTI-RIEL: el t que tiene la zona, es del riel que gobierna sus celdas?
// Se decide con la MEJOR evidencia disponible, en este orden:
//   1. el identificador que vino con el ultimo setVisCurvaT (se consume por
//      tick: un identificador viejo no puede decidir ticks futuros);
//   2. el riel de la Camera ACTIVA (setRiel del juego): si viaja por OTRO riel
//      -o por ninguno- el indice que la zona tiene guardado no es de aca;
//   3. sin camara activa no hay evidencia: se acepta (harness / escenas sin
//      camara siguen andando igual que siempre).
bool VisZona::RielGobiernaOk() {
    if (rielNombre.empty()) return true;   // legacy: la zona no declara riel
    if (rielBindHay) {
        rielBindHay = false;               // evidencia de ESTE tick, no de los proximos
        return rielBind == rielNombre;
    }
    if (CameraActive) return CameraActive->Riel && CameraActive->Riel->name == rielNombre;
    return true;
}

void VisZona::Tick() {
    if (modo == ModoManual) return;   // setSector manda (override manual de siempre)
    if (modo == ModoCurva) {
        // MULTI-RIEL: con `riel:` declarado y la camara en OTRO riel, el t
        // guardado no indexa estas celdas -> fallback declarado (celda 0 =
        // malla completa, o la celda que pida el archivo). Ver VisZona.h.
        if (!RielGobiernaOk()) {
            fueraDeRiel = true;
            Aplicar(celdaFallback > 0 ? celdaFallback : 0);
            return;
        }
        // el stepper del original: UNA celda por tick hacia el objetivo; un salto
        // mas largo que pasoMax se aplica directo (VisSet::Delta re-decodifica
        // desde la celda-clave cuando el paso no es +-1, asi que es barato igual).
        int objetivo = 1 + (int)floorf(curvaT < 0.0f ? 0.0f : curvaT);
        // reentrada al riel propio: salto DIRECTO al objetivo (el stepper no
        // tiene que pasear por celdas que nunca estuvieron activas)
        if (fueraDeRiel) { fueraDeRiel = false; Aplicar(objetivo); return; }
        if (celdaActual <= 0) { Aplicar(objetivo); return; }
        int d = objetivo - celdaActual;
        if (d == 0) return;
        if (d > pasoMax || d < -pasoMax) { Aplicar(objetivo); return; }
        Aplicar(celdaActual + (d > 0 ? 1 : -1));
        return;
    }
    Object* a = Ancla();
    if (!a) return;
    Vector3 p = a->GetGlobalPosition();
    Aplicar(modo == ModoGrilla ? CeldaGrilla(p) : CeldaVolumenes(p));
}

// ---------------------------------------------------------------------------
//  el tick global (una vez por tick del juego; mismo contrato que las particulas)
// ---------------------------------------------------------------------------
void W3dVisZonasTick() {
    if (!SceneCollection) return;
    std::vector<Object*> st; st.push_back(SceneCollection);
    while (!st.empty()) {
        Object* o = st.back(); st.pop_back();
        if (!o->visible) continue;   // oculta (con su subarbol): no decide nada
        if (o->getType() == ObjectType::viszona) ((VisZona*)o)->Tick();
        for (size_t i = 0; i < o->Childrens.size(); i++) st.push_back(o->Childrens[i]);
    }
}
