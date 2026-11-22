// ============================================================================
//  VisZone.cpp — ver VisZone.h. La zona elige la celda; la aplica por el hook
//  del bind setSector (W3dPVSSetSectorHook, instalado por el editor): esta
//  clase no conoce Mesh ni MeshEdit.
// ============================================================================
#include "VisZone.h"
#include "Camera.h"   // multi-riel: CameraActive->Rail (por que riel viaja la camara)
#include <math.h>

// el hook del bind setSector (definido en BindsGame.cpp, instalado por MeshEdit.cpp)
extern bool (*W3dPVSSetSectorHook)(Object*, int);

Object* VisZone::GetTarget() const {
    if (targetName.empty() || !SceneCollection) return NULL;
    return W3dFindNameFrom(const_cast<VisZone*>(this), targetName, SceneCollection);   // (por scope)
}

Object* VisZone::Anchor() const {
    if (anchorObj) return anchorObj;
    if (anchorName.empty() || !SceneCollection) return NULL;
    return W3dFindNameFrom(const_cast<VisZone*>(this), anchorName, SceneCollection);
}

// posicion 'p' (mundo) -> celda 1-based de la grilla local de la zona.
// 0 = fuera de la grilla (fallback: malla completa, nunca un pozo negro).
int VisZone::CellGrid(const Vector3& p) const {
    Matrix4 W; GetWorldMatrix(W);
    Matrix4 inv;
    if (!Matrix4::InvertAffine(W, inv)) inv = W.Inverse();
    Vector3 l = inv * p;
    if (cellSize <= 1e-6f) return 0;
    int ix = (int)floorf(l.x / cellSize);
    int iy = (int)floorf(l.y / cellSize);
    int iz = (int)floorf(l.z / cellSize);
    if (ix < 0 || ix >= nx || iy < 0 || iy >= ny || iz < 0 || iz >= nz) return 0;
    return 1 + ix + iy * nx + iz * nx * ny;
}

// posicion 'p' (mundo) -> indice 1-based del PRIMER hijo cuya caja unitaria
// [-1,1] (escalada/rotada por su transform, como un Empty) contiene a 'p'.
int VisZone::CellVolumes(const Vector3& p) const {
    for (size_t i = 0; i < Childrens.size(); i++) {
        Object* h = Childrens[i];
        if (!h || !h->visible) continue;
        Matrix4 W; h->GetWorldMatrix(W);
        Matrix4 inv;
        if (!Matrix4::InvertAffine(W, inv)) inv = W.Inverse();
        Vector3 l = inv * p;
        if (l.x >= -1.0f && l.x <= 1.0f && l.y >= -1.0f && l.y <= 1.0f &&
            l.z >= -1.0f && l.z <= 1.0f) return (int)i + 1;
    }
    return 0;
}

void VisZone::Apply(int cell) {
    if (cell == currentCell) return;
    currentCell = cell;
    Object* obj = GetTarget();
    if (obj && W3dPVSSetSectorHook) W3dPVSSetSectorHook(obj, cell);
}

// MULTI-RIEL: el t que tiene la zona, es del riel que gobierna sus celdas?
// Se decide con la MEJOR evidencia disponible, en este orden:
//   1. el identificador que vino con el ultimo setVisCurvaT (se consume por
//      tick: un identificador viejo no puede decidir ticks futuros);
//   2. el riel de la Camera ACTIVA (setRiel del juego): si viaja por OTRO riel
//      -o por ninguno- el indice que la zona tiene guardado no es de aca;
//   3. sin camara activa no hay evidencia: se acepta (harness / escenas sin
//      camara siguen andando igual que siempre).
bool VisZone::RailGovernsOk() {
    if (railName.empty()) return true;   // legacy: la zona no declara riel
    if (railBindHas) {
        railBindHas = false;               // evidencia de ESTE tick, no de los proximos
        return railBind == railName;
    }
    if (CameraActive) return CameraActive->Rail && CameraActive->Rail->name == railName;
    return true;
}

void VisZone::Tick() {
    if (mode == ModeManual) return;   // setSector manda (override manual de siempre)
    if (mode == ModeCurve) {
        // MULTI-RIEL: con `riel:` declarado y la camara en OTRO riel, el t
        // guardado no indexa estas celdas -> fallback declarado (celda 0 =
        // malla completa, o la celda que pida el archivo). Ver VisZone.h.
        if (!RailGovernsOk()) {
            outsideRail = true;
            Apply(cellFallback > 0 ? cellFallback : 0);
            return;
        }
        // el stepper del original: UNA celda por tick hacia el objetivo; un salto
        // mas largo que stepMax se aplica directo (VisSet::Delta re-decodifica
        // desde la celda-clave cuando el paso no es +-1, asi que es barato igual).
        int target = 1 + (int)floorf(curveT < 0.0f ? 0.0f : curveT);
        // reentrada al riel propio: salto DIRECTO al objetivo (el stepper no
        // tiene que pasear por celdas que nunca estuvieron activas)
        if (outsideRail) { outsideRail = false; Apply(target); return; }
        if (currentCell <= 0) { Apply(target); return; }
        int d = target - currentCell;
        // FPS BAJO (N95: 4-8 fps): la camara avanza MUCHAS celdas por tick. El stepper de +-1 del original
        // (pensado para 60 fps) quedaba ATRAS -> se aplicaba un sector PASADO -> AGUJEROS y partes oscuras en
        // el mapa (en PC a 60 fps no se nota: d~=0/+-1). Si la camara se movio MAS de 1 celda desde el ultimo
        // tick, saltar DIRECTO al objetivo (el re-decode desde la celda-clave es barato: no cuesta mas rebinds
        // de IBO que el paso). El paso incremental de +-1 queda solo para 60 fps (d==+-1).
        if (d == 0) return;
        if (d > 1 || d < -1) { Apply(target); return; }   // (antes: > stepMax; a fps bajo eso quedaba atras)
        Apply(currentCell + (d > 0 ? 1 : -1));
        return;
    }
    Object* a = Anchor();
    if (!a) return;
    Vector3 p = a->GetGlobalPosition();
    Apply(mode == ModeGrid ? CellGrid(p) : CellVolumes(p));
}

// ---------------------------------------------------------------------------
//  el tick global (una vez por tick del juego; mismo contrato que las particulas)
// ---------------------------------------------------------------------------
void W3dVisZonesTick() {
    if (!SceneCollection) return;
    std::vector<Object*> st; st.push_back(SceneCollection);
    while (!st.empty()) {
        Object* o = st.back(); st.pop_back();
        if (!o->visible) continue;   // oculta (con su subarbol): no decide nada
        if (o->getType() == ObjectType::viszona) ((VisZone*)o)->Tick();
        for (size_t i = 0; i < o->Childrens.size(); i++) st.push_back(o->Childrens[i]);
    }
}
