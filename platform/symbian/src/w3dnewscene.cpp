/*
 * ==============================================================================
 *  w3dnewscene.cpp — Fase 3c-2: el viewport 3D de PC en el N95
 *
 *  Este es el UNICO TU de Symbian que ve los headers del core (el Object
 *  viejo choca de nombre). Renderiza SOLO el modelo nuevo, con la convencion
 *  de PC: 1 unidad = 1 metro, camara perspectiva 60 grados (initGL de
 *  main.cpp), luz real GLES1 y modos Solid / MaterialPreview.
 * ==============================================================================
 */

#include "w3dnewscene.h"
#include "w3dlayout.h"

#ifndef GL_POINT_SPRITE_OES
#define GL_POINT_SPRITE_OES 0x8861
#endif
#ifndef GL_COORD_REPLACE_OES
#define GL_COORD_REPLACE_OES 0x8862
#endif


#include "objects/Scene.h"
#include "objects/Mesh.h"
#include "objects/Light.h"
#include "objects/Camera.h"
#include "objects/ObjectMode.h" // estado/G-R-S REALES de PC
#include "importers/import_obj.h"
#include "ViewPorts/ViewPort3D.h" // el viewport REAL de PC
#include "objects/Empty.h"
#include <string.h> // strncpy
#include "ui/W3dColors.h"
#include "w3dlog.h"
#include "ViewPorts/LayoutInput.h" // ScenePick3D compartido
#include "objects/Collection.h"
#include <GLES/gl.h>
#include <math.h>

// ---- camara orbital propia, en unidades de PC (metros) ----
static float nRotX = 30.0f;   // yaw
static float nRotY = 25.0f;   // pitch
static float nDist = 7.0f;    // metros al pivot
static float nPivX = 0.0f, nPivY = 0.0f, nPivZ = 0.0f;

// modo de vista: 0 = Solid (sin luz), 1 = MaterialPreview (luz + materiales)
static int nViewMode = 1;

// piso/grilla estilo PC: lineas de -10..10m cada 1m (arrays construidos una
// sola vez: estables entre frames, driver-safe)
// (el render puente se borro: el Viewport3D REAL de PC dibuja todo)

// textura blanca 1x1: el baseline del frame la deja bindeada para que los
// draws sin textura modulen por blanco (la usan Mesh y los paneles)
static GLuint gWhiteTex = 0;

// reconstruye el quaternion desde los euler (mismo orden YXZ que Object)
static void RotDesdeEuler(Object* aObj) {
    Quaternion qX = Quaternion::FromAxisAngle(Vector3(1,0,0), aObj->rotEuler.x);
    Quaternion qY = Quaternion::FromAxisAngle(Vector3(0,1,0), aObj->rotEuler.y);
    Quaternion qZ = Quaternion::FromAxisAngle(Vector3(0,0,1), aObj->rotEuler.z);
    aObj->rot = qY * qX * qZ;
}

unsigned int W3dNewWhiteTex() {
    if (gWhiteTex == 0) {
        static const unsigned char px[4] = { 255, 255, 255, 255 };
        glGenTextures(1, &gWhiteTex);
        glBindTexture(GL_TEXTURE_2D, gWhiteTex);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, px);
    }
    return gWhiteTex;
}

// (gizmos/grilla/billboards del puente: borrados, los dibuja render.cpp real)

// raiz (SceneCollection) + escena default, como ConstructUniversal de PC
void W3dNewSceneInit() {
    W3dModelInit();

    if (SceneCollection && SceneCollection->Childrens.empty()) {
        // todo cuelga de una "Collection" (igual que el constructor de PC)
        CollectionActive = new Collection(SceneCollection);
        // luz de escena default (como Blender): arriba-derecha-atras
        Light* l = Light::Create(CollectionActive, 0, 0, 0);
        if (l) { l->pos = Vector3(3.0f, 4.0f, 2.0f); }
        // camara default, misma pose que el constructor de PC
        new Camera(CollectionActive, Vector3(-3.0f, 2.5f, 1.8f), Vector3(-35.0f, -45.0f, 0.0f));
        // el cubo se crea ULTIMO y arranca seleccionado
        Object* cubo = NewMesh(MeshType::cube, CollectionActive, false);
        if (cubo) {
            DeseleccionarTodo();
            cubo->Seleccionar();
        }
    }
    // (los grupos de propiedades los construye cada panel en su ctor)
    // el importador de PC cuelga lo que importa de CollectionActive
    if (!CollectionActive) {
        CollectionActive = SceneCollection;
    }
    w3dLogf("nuevoModelo: escena default (hijos: %d)",
        SceneCollection ? (TInt)SceneCollection->Childrens.size() : -1);
}

// ----------------------------------------------------------------------------
// transformaciones G/R/S sobre el estado REAL de PC (ObjectMode.cpp)
// ----------------------------------------------------------------------------
static TInt gTMode = 0; // 0 nada, 1 mover, 2 rotar, 3 escalar

TBool W3dNewTransformStart(TInt aMode) {
    // EDIT MODE: el transform actua sobre los vertices/aristas/caras SELECCIONADOS, igual
    // que en PC (usa el EditXform COMPARTIDO de LayoutInput). Antes Symbian solo hacia
    // Object Mode -> en edicion no se podian mover los verts ("no podia hacer nada").
    if (InteractionMode == EditMode && g_editMesh) {
        // el transform de EDICION se rastrea por el estado COMPARTIDO (EditXformActivo()
        // + estado), NO por gTMode (que es de Object Mode). Asi extrude/duplicar -que
        // arrancan el move sin pasar por aca- tambien quedan "activos" para Symbian.
        if (aMode == 1)      { estado = translacion; axisSelect = ViewAxis; }
        else if (aMode == 2) { estado = rotacion;    axisSelect = ViewAxis; }
        else                 { estado = EditScale;   axisSelect = XYZ; }
        EditXformIniciar(); // snapshot de la seleccion de malla
        if (!EditXformActivo()) { estado = editNavegacion; return EFalse; }
        w3dLogf("transform EDIT: inicio modo %d", aMode);
        return ETrue;
    }
    // OBJECT MODE: el transform del objeto entero
    if (!ObjActivo || !ObjActivo->select) return EFalse;
    gTMode = aMode;
    if (aMode == 1) {
        SetPosicion(); // estado = translacion + guardarEstado + ViewAxis
    } else {
        if (!guardarEstado()) { gTMode = 0; return EFalse; }
        estado = (aMode == 2) ? rotacion : EditScale;
        axisSelect = ViewAxis;
    }
    w3dLogf("transform: inicio modo %d (estado=%d)", aMode, estado);
    return ETrue;
}

TBool W3dNewTransformActive() {
    // un transform de EDICION de malla (lo arranca G/R/S del keypad, extrude o duplicar)
    // tambien cuenta como activo -> la maquinaria de Symbian (flechas/BT move, accept/
    // cancel, todo gateado por esta funcion) lo maneja igual que el de objetos.
    if (InteractionMode == EditMode && EditXformActivo()) return ETrue;
    return gTMode != 0;
}

// 0 nada, 1 mover, 2 rotar, 3 escalar (para el paso de las flechas)
TInt W3dNewTransformModo() {
    if (InteractionMode == EditMode && EditXformActivo()) {
        if (estado == rotacion)  return 2;
        if (estado == EditScale) return 3;
        return 1; // translacion (move / extrude / duplicar)
    }
    return gTMode;
}

void W3dNewTransformMove(TInt aDx, TInt aDy) {
    // velocidad de arrastre = mundo-por-pixel a la PROFUNDIDAD del pivot de transform (igual que el
    // mouse/flechas de PC). Asi el paso de las flechas del N95 se siente CONSTANTE en pantalla a
    // cualquier zoom (antes era 0.01 fijo: de cerca iba rapido, de lejos lento).
    float velMov = Viewport3DActive ? Viewport3DActive->VelocidadArrastreMundo() : 0.01f;
    // EDIT MODE: aplica el delta a la seleccion de MALLA segun el estado COMPARTIDO
    // (vale para cualquier starter: keypad G/R/S, extrude, duplicar). VA ANTES del
    // gate de gTMode porque extrude/duplicar no setean gTMode.
    if (InteractionMode == EditMode && EditXformActivo()) {
        if (estado == rotacion)       EditXformRotEje(aDx, aDy);
        else if (estado == EditScale) EditXformScale(aDx, aDy, 0.001f);
        else                          EditXformTraslacion(aDx, aDy, velMov);
        return;
    }
    if (!gTMode) return;
    // OBJECT MODE: transform COMPARTIDO con PC (respeta axisSelect/orientacion: 1/2/3=X/Y/Z,
    // direccion relativa a la vista, etc.). Lo usan el mouse BT y las flechas.
    if (gTMode == 1)      SetTranslacionObjetos(aDx, aDy, velMov);
    else if (gTMode == 2) {
        if (axisSelect == OrbitalAxis) RotarOrbital(aDx, aDy); // orbital: camUp/camRight
        else                           SetRotacion(aDx, aDy);
    }
    else if (gTMode == 3) SetScale(aDx, aDy, 0.001f);
}

// 1/2/3 durante un transform = X/Y/Z (como apretar X/Y/Z en PC): constriñe al
// eje y re-apretar cicla global->local->view->libre.
void W3dNewTransformEje(TInt aEje) {
    CiclarEjeTransform(aEje);
}

// 0 durante una rotacion: alterna trackball (desde la vista) <-> orbital/gimbal
void W3dNewToggleOrbital() {
    ToggleRotacionOrbital();
}

// actualiza la linea punteada pivot->mouse (verde) durante rotar/escalar
void W3dNewTransformLinea(TInt aMx, TInt aMy) {
    if (Viewport3DActive) Viewport3DActive->ActualizarLineaTransform(aMx, aMy);
}

// estamos rotando LIBRE (desde la vista)? -> el mouse usa el trackball
TBool W3dNewEsRotarDesdeVista() {
    return (estado == rotacion && axisSelect == ViewAxis) ? ETrue : EFalse;
}
void W3dNewRotarDesdeVista(TInt aMx, TInt aMy) {
    if (Viewport3DActive) Viewport3DActive->RotarDesdeVista(aMx, aMy);
}

void W3dNewOrbit(TInt aDx, TInt aDy) {
    if (Viewport3DActive) Viewport3DActive->OrbitarFlecha(aDx, aDy);
}
// modificadores del keypad (N95): 0+flechas=zoom, *+flechas=paneo, #+flechas=primera persona.
void W3dNewZoom(TInt aDelta) { if (Viewport3DActive) Viewport3DActive->Zoom((float)aDelta); }
void W3dNewPan(TInt aDx, TInt aDy)  { if (Viewport3DActive) Viewport3DActive->PanFlecha(-aDx, -aDy); } // invertido (Dante: las flechas iban al reves)
void W3dNewLook(TInt aDx, TInt aDy) { if (Viewport3DActive) Viewport3DActive->MirarPrimeraPersona(aDx, aDy); }

void W3dNewTransformEnd(TBool aCancel) {
    // EDIT MODE: confirmar (fija + recalcula bordes/normales) o cancelar (restaura snapshot)
    // el transform de MALLA (compartido con PC).
    if (InteractionMode == EditMode && EditXformActivo()) {
        if (aCancel) EditXformCancelar(); else EditXformConfirmar();
        estado = editNavegacion;
        gTMode = 0;
        w3dLogf("transform EDIT: fin (cancel=%d)", (TInt)aCancel);
        return;
    }
    // OBJECT MODE
    if (aCancel) {
        Cancelar(); // el REAL de PC: restaura desde estadoObjetos
    } else {
        // ACEPTAR: limpiar lo guardado SIN restaurar (ReestablecerEstado
        // restaura posiciones: es el mecanismo interno del cancel)
        estadoObjetos.clear();
        estado = editNavegacion;
    }
    w3dLogf("transform: fin (cancel=%d)", (TInt)aCancel);
    gTMode = 0;
}

// agregar objetos al modelo NUEVO (menu de Symbian). Tipos:
// 0=cubo 1=plano 2=circulo 3=vertice 4=luz 5=camara 6=empty
void W3dNewAdd(TInt aTipo) {
    Object* nuevo = NULL;
    switch (aTipo) {
        case 0: nuevo = NewMesh(MeshType::cube, NULL, false); break;
        case 1: nuevo = NewMesh(MeshType::plane, NULL, false); break;
        case 2: nuevo = NewMesh(MeshType::circle, NULL, false); break;
        case 3: nuevo = NewMesh(MeshType::vertice, NULL, false); break;
        case 4: {
            Light* l = Light::Create(NULL, 0, 0, 0);
            if (l) { l->pos = cursor3D.pos; }
            nuevo = l;
            break;
        }
        case 5: nuevo = new Camera(NULL, cursor3D.pos, Vector3(-35.0f, -45.0f, 0.0f)); break;
        case 6: nuevo = new Empty(NULL, cursor3D.pos); break;
    }
    if (nuevo) {
        DeseleccionarTodo();
        nuevo->Seleccionar();
        w3dLogf("add: tipo %d agregado", aTipo);
    }
}

// ctrl+p / alt+p: emparentar / desemparentar (compartido)
void W3dNewSetParent() { SetParentSeleccion(); }
void W3dNewClearParent() { ClearParentSeleccion(); }

// 'a': seleccionar/deseleccionar todo (el compartido de Objects.cpp)
void W3dNewSeleccionarTodo() {
    SeleccionarTodo(true);
}

// alt+d: duplicado LINKEADO (instancias de PC)
void W3dNewInstancia() {
    NewInstance();
}

// puente para los TUs viejos (no ven Objects.h por el clash de Object)
void W3dNewDeseleccionarTodo() {
    DeseleccionarTodo();
}

// enfocar el objeto activo (el del Viewport3D REAL de PC)
void W3dNewEnfocar() {
    if (Viewport3DActive) {
        Viewport3DActive->EnfocarObject();
    }
}

// tecla 6 / 0: modos de vista del viewport REAL
void W3dNewSceneToggleRenderMode() {
    if (Viewport3DActive) {
        Viewport3DActive->ChangeViewType();
    }
}

void W3dNewSceneCycleViewMode() {
    W3dNewSceneToggleRenderMode();
}

// ---- input (lo llaman los handlers HID de Whisk3D.cpp) ----

void W3dNewSceneOrbit(TInt aDx, TInt aDy) {
    // el viewport REAL de PC: quaternion + orbita sandwich (RotateOrbit)
    if (!Viewport3DActive) return;
    dx = aDx;
    dy = aDy;
    Viewport3DActive->RotateOrbit();
}

void W3dNewSceneZoom(TInt aDelta) {
    if (Viewport3DActive) {
        Viewport3DActive->Zoom((float)aDelta);
    }
}

// ---- seleccion por color picking: COMPARTIDA (LayoutInput.cpp) ----

TBool W3dNewScenePick(TInt aX, TInt aY, TInt aVx, TInt aVy, TInt aVw, TInt aVh, TInt aScreenH) {
    if (ScenePick3D(aX, aY, aVx, aVy, aVw, aVh, aScreenH)) {
        return ETrue;
    }
    return EFalse;
}

// ---- panel de propiedades sobre el modelo NUEVO (Fase 3c-2) ----

TBool W3dNewSelInfo(char* aName, TInt aNameMax,
                    float aPos[3], float aRot[3], float aScale[3]) {
    if (!ObjActivo || !ObjActivo->select) return EFalse;
    strncpy(aName, ObjActivo->name.c_str(), aNameMax - 1);
    aName[aNameMax - 1] = 0;
    aPos[0] = ObjActivo->pos.x;   aPos[1] = ObjActivo->pos.y;   aPos[2] = ObjActivo->pos.z;
    aRot[0] = ObjActivo->rotEuler.x; aRot[1] = ObjActivo->rotEuler.y; aRot[2] = ObjActivo->rotEuler.z;
    aScale[0] = ObjActivo->scale.x; aScale[1] = ObjActivo->scale.y; aScale[2] = ObjActivo->scale.z;
    return ETrue;
}

void W3dNewAdjust(TInt aRow, TInt aDelta) {
    if (!ObjActivo) return;
    float d = (float)aDelta;
    switch (aRow) {
        case 0: ObjActivo->pos.x += d * 0.1f; break;
        case 1: ObjActivo->pos.y += d * 0.1f; break;
        case 2: ObjActivo->pos.z += d * 0.1f; break;
        case 3: ObjActivo->rotEuler.x += d; break;
        case 4: ObjActivo->rotEuler.y += d; break;
        case 5: ObjActivo->rotEuler.z += d; break;
        case 6: ObjActivo->scale.x += d * 0.1f; break;
        case 7: ObjActivo->scale.y += d * 0.1f; break;
        case 8: ObjActivo->scale.z += d * 0.1f; break;
    }
    if (aRow >= 3 && aRow <= 5) {
        RotDesdeEuler(ObjActivo);
    }
}

// ---- arbol de la escena para el outliner (DFS, indice 0 = Collection) ----

static TInt TreeCountRec(Object* aObj) {
    TInt n = 1;
    for (size_t c = 0; c < aObj->Childrens.size(); c++) {
        n += TreeCountRec(aObj->Childrens[c]);
    }
    return n;
}

static Object* TreeNth(Object* aObj, TInt& aCounter, TInt aWant, TInt aDepth, TInt* aOutDepth) {
    if (aCounter == aWant) {
        if (aOutDepth) { *aOutDepth = aDepth; }
        return aObj;
    }
    aCounter++;
    for (size_t c = 0; c < aObj->Childrens.size(); c++) {
        Object* r = TreeNth(aObj->Childrens[c], aCounter, aWant, aDepth + 1, aOutDepth);
        if (r) return r;
    }
    return NULL;
}

TInt W3dNewTreeCount() {
    if (!SceneCollection) return 0;
    return TreeCountRec(SceneCollection);
}

TBool W3dNewTreeItem(TInt aIdx, char* aName, TInt aNameMax,
                     TInt& aDepth, TBool& aSel, TInt& aTipo) {
    if (!SceneCollection || !aName || aNameMax < 2) return EFalse;
    TInt counter = 0;
    TInt depth = 0;
    Object* o = TreeNth(SceneCollection, counter, aIdx, 0, &depth);
    if (!o) return EFalse;
    strncpy(aName, o->name.c_str(), aNameMax - 1);
    aName[aNameMax - 1] = 0;
    aDepth = depth;
    aSel = o->select ? (TBool)ETrue : (TBool)EFalse;
    if (o == SceneCollection) aTipo = 0;
    else if (o->getType() == ObjectType::mesh) aTipo = 1;
    else if (o->getType() == ObjectType::light) aTipo = 2;
    else if (o->getType() == ObjectType::camera) aTipo = 4;
    else aTipo = 3;
    return ETrue;
}

// importar un .obj con el importador COMPARTIDO de PC (fase 4)
TBool W3dNewImportObj(const char* aPath) {
    if (!aPath || !aPath[0]) return EFalse;
    bool ok = ImportOBJ(std::string(aPath), false);
    w3dLogf("importObj: ok=%d hijos=%d", (TInt)ok,
        SceneCollection ? (TInt)SceneCollection->Childrens.size() : -1);
    return ok ? ETrue : EFalse;
}

// tecla lapiz del telefono: cicla la seleccion activa por el arbol (DFS)
// OK del telefono con una malla seleccionada = Tab de PC (entra/sale de Edit Mode)
void W3dNewToggleEdit() { LayoutToggleEditMode(); }

void W3dNewCycleSelect() {
    // El lapiz (tap) cicla el activo: deselec el activo + selec el siguiente, el RESTO queda.
    // Mode-aware (LayoutInput.cpp): en Edit Mode sobre el sub-elemento; en Object Mode sobre el
    // objeto, con el MISMO comportamiento exacto (ya no reemplaza todo con DeseleccionarTodo).
    EditSelAvanzar(1, false);
}

// tecla borrar del telefono: borra el objeto ACTIVO del modelo nuevo
static void BorrarRec(Object* aObj) {
    for (size_t c = 0; c < aObj->Childrens.size(); c++) {
        BorrarRec(aObj->Childrens[c]);
    }
    aObj->Childrens.clear();
    for (size_t i = 0; i < Lights.size(); i++) {
        if ((Object*)Lights[i] == aObj) {
            Lights.erase(Lights.begin() + i);
            break;
        }
    }
    delete aObj;
}

TBool W3dNewDeleteActive() {
    // borra TODOS los seleccionados con el Eliminar compartido de PC
    if (!SceneCollection) return EFalse;
    if (!HayObjetosSeleccionados(false)) return EFalse;
    // OJO con las luces: Light::~Light no se saca de Lights -> limpiar
    // las seleccionadas del vector ANTES de que Eliminar las destruya
    for (TInt i = (TInt)Lights.size() - 1; i >= 0; i--) {
        if (Lights[i]->select) {
            glDisable(Lights[i]->LightID);
            Lights.erase(Lights.begin() + i);
        }
    }
    Eliminar(true);
    ObjActivo = NULL;
    return ETrue;
}

void W3dNewTreeSelect(TInt aIdx) {
    if (!SceneCollection) return;
    TInt counter = 0;
    Object* o = TreeNth(SceneCollection, counter, aIdx, 0, NULL);
    if (!o || o == SceneCollection) return;
    DeseleccionarTodo();
    o->Seleccionar();
    w3dLogf("outliner: seleccionado idx=%d", aIdx);
}
