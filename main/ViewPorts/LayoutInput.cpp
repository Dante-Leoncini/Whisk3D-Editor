#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "Undo.h" // Ctrl+Z: capturar modo / seleccion
#include "ViewPorts/PopUp/ConfirmarPopup.h" // AbrirConfirmarBorrado (popup de confirmar borrado)
#include "ViewPorts/LayoutInput.h"
#include "ViewPorts/ViewPort3D.h"
#include "ViewPorts/Outliner.h"
#include "ViewPorts/Properties.h"
#include "ViewPorts/UVEditor.h"
#include "WhiskUI/glesdraw.h"
#include "WhiskUI/rectangle.h" // el velo del modo foco
#include "objects/Objects.h"
#include "objects/Mesh.h"
#include "objects/EditMesh.h"
#include "objects/Light.h"
#include "objects/Camera.h"
#include "objects/Empty.h"
#include "objects/Instance.h"
#include "objects/Collection.h"
#include "objects/ObjectMode.h"
#include "edit/Modifier.h" // ModifierType::Mirror + target (regen de mirrors al mover objetos)
#include "objects/Primitivas.h"
#include "objects/Textures.h"
#include "variables.h"
#include "render/OpcionesRender.h" // g_fpsActual
#include "ViewPorts/PopUp/PopUpBase.h"
#include "ViewPorts/PopUp/RedoMeshPanel.h"
#include "WhiskUI/card.h"        // tarjeta de las notificaciones
#include "WhiskUI/bitmapText.h"  // texto de las notificaciones
#include "WhiskUI/icons.h"       // iconos notifOk / notifError
#include "WhiskUI/colores.h"     // ColorID
#include "w3dlog.h"         // las notificaciones tambien van al log
// (los tipos GL + el dibujo vienen del engine: w3dGraphics.h / w3dEngine, ya incluido arriba)

void (*LayoutImportObj)() = NULL;
void (*LayoutWarpMouse)(int x, int y) = NULL;
void (*LayoutArbolCambiado)() = NULL;

// ====================================================================
// arbol: helpers (Row y Column se distinguen por ContainerKind, sin RTTI)
// ====================================================================

static ViewportBase* LayoutCrearViewport(int aId) {
    switch (aId) {
        case 0: return new Viewport3D();
        case 1: return new Outliner();
        case 2: return new Properties();
        case 3: return new UVEditor();
    }
    return NULL;
}

static bool LayoutReemplazarEnArbol(ViewportBase* aNodo, ViewportBase* aViejo,
                                    ViewportBase* aNuevo) {
    if (!aNodo || aNodo->isLeaf()) return false;
    if (aNodo->ContainerKind() == 1) {
        ViewportRow* r = (ViewportRow*)aNodo;
        if (r->childA == aViejo) { r->childA = aNuevo; return true; }
        if (r->childB == aViejo) { r->childB = aNuevo; return true; }
        if (LayoutReemplazarEnArbol(r->childA, aViejo, aNuevo)) return true;
        return LayoutReemplazarEnArbol(r->childB, aViejo, aNuevo);
    }
    if (aNodo->ContainerKind() == 2) {
        ViewportColumn* c = (ViewportColumn*)aNodo;
        if (c->childA == aViejo) { c->childA = aNuevo; return true; }
        if (c->childB == aViejo) { c->childB = aNuevo; return true; }
        if (LayoutReemplazarEnArbol(c->childA, aViejo, aNuevo)) return true;
        return LayoutReemplazarEnArbol(c->childB, aViejo, aNuevo);
    }
    return false;
}

static ViewportBase* LayoutPadreDe(ViewportBase* aNodo, ViewportBase* aHijo) {
    if (!aNodo || aNodo->isLeaf()) return NULL;
    ViewportBase* a = NULL;
    ViewportBase* b = NULL;
    if (aNodo->ContainerKind() == 1) {
        a = ((ViewportRow*)aNodo)->childA;
        b = ((ViewportRow*)aNodo)->childB;
    } else {
        a = ((ViewportColumn*)aNodo)->childA;
        b = ((ViewportColumn*)aNodo)->childB;
    }
    if (a == aHijo || b == aHijo) return aNodo;
    ViewportBase* r = LayoutPadreDe(a, aHijo);
    if (r) return r;
    return LayoutPadreDe(b, aHijo);
}

// borra un subarbol completo (los dtors de Row/Column no borran childA)
static void LayoutBorrarSubarbol(ViewportBase* aNodo) {
    if (!aNodo) return;
    if (!aNodo->isLeaf()) {
        if (aNodo->ContainerKind() == 1) {
            ViewportRow* r = (ViewportRow*)aNodo;
            LayoutBorrarSubarbol(r->childA);
            LayoutBorrarSubarbol(r->childB);
            r->childA = NULL;
            r->childB = NULL;
        } else {
            ViewportColumn* c = (ViewportColumn*)aNodo;
            LayoutBorrarSubarbol(c->childA);
            LayoutBorrarSubarbol(c->childB);
            c->childA = NULL;
            c->childB = NULL;
        }
    }
    delete aNodo;
}

static Viewport3D* LayoutPrimer3D(ViewportBase* aNodo) {
    if (!aNodo) return NULL;
    if (aNodo->isLeaf()) {
        return aNodo->ViewportKind() == 1 ? (Viewport3D*)aNodo : NULL;
    }
    ViewportBase* a = NULL;
    ViewportBase* b = NULL;
    if (aNodo->ContainerKind() == 1) {
        a = ((ViewportRow*)aNodo)->childA;
        b = ((ViewportRow*)aNodo)->childB;
    } else {
        a = ((ViewportColumn*)aNodo)->childA;
        b = ((ViewportColumn*)aNodo)->childB;
    }
    Viewport3D* r = LayoutPrimer3D(a);
    if (r) return r;
    return LayoutPrimer3D(b);
}

// recolecta las hojas (viewports con borde) en orden DFS izquierda->derecha
static void LayoutRecolectarHojas(ViewportBase* aNodo, std::vector<ViewportBase*>& out) {
    if (!aNodo) return;
    if (aNodo->isLeaf()) { out.push_back(aNodo); return; }
    ViewportBase* a; ViewportBase* b;
    if (aNodo->ContainerKind() == 1) { a = ((ViewportRow*)aNodo)->childA;    b = ((ViewportRow*)aNodo)->childB; }
    else                             { a = ((ViewportColumn*)aNodo)->childA; b = ((ViewportColumn*)aNodo)->childB; }
    LayoutRecolectarHojas(a, out);
    LayoutRecolectarHojas(b, out);
}

// cambia el viewport ACTIVO (borde verde) a la siguiente hoja (dir=+1) o la
// anterior (dir=-1), dando la vuelta. Sin mouse (Symbian) es la unica forma de
// elegir viewport: la tecla verde de llamada lo cicla. Con mouse, el hover lo
// pisa en el siguiente movimiento (el mouse manda cuando esta).
void LayoutCiclarViewportActivo(int dir) {
    std::vector<ViewportBase*> hojas;
    LayoutRecolectarHojas(rootViewport, hojas);
    if (hojas.empty()) return;
    int n = (int)hojas.size();
    int idx = -1;
    for (int i = 0; i < n; i++) if (hojas[i] == viewPortActive) { idx = i; break; }
    int next = (idx < 0) ? 0 : (((idx + dir) % n) + n) % n;
    viewPortActive = hojas[next];
    if (viewPortActive->isLeaf() && viewPortActive->ViewportKind() == 1)
        Viewport3DActive = (Viewport3D*)viewPortActive;
}

// redimensiona el viewport ACTIVO en UN eje (dx!=0 izq/der; dy!=0 arr/ab).
// Sube al ANCESTRO mas cercano del tipo correcto (Row para horizontal, Column
// para vertical): asi estando en el outliner (dentro de un Row dentro de un
// Column), izq/der mueve el divisor del Row, y arr/ab mueve el del Column (le
// roba espacio al 3D de arriba). childA = izquierda/arriba; der/abajo => frac+.
void LayoutRedimensionarViewportActivo(int dx, int dy, float paso) {
    if (!viewPortActive || !rootViewport) return;
    if (dx == 0 && dy == 0) return;
    bool horizontal = (dx != 0); // izq/der -> Row ; arr/ab -> Column
    ViewportBase* nodo = viewPortActive;
    ViewportBase* anc = NULL;
    while (nodo) {
        ViewportBase* padre = LayoutPadreDe(rootViewport, nodo);
        if (!padre) break;
        bool esRow = (padre->ContainerKind() == 1);
        if (horizontal == esRow) { anc = padre; break; } // Row<->H, Column<->V
        nodo = padre;
    }
    if (!anc) return; // no hay divisor en ese eje
    float* frac = (anc->ContainerKind() == 1) ? &((ViewportRow*)anc)->splitFrac
                                              : &((ViewportColumn*)anc)->splitFrac;
    // der/abajo agrandan childA (frac+); izq/arriba lo achican. El activo crece o
    // se achica segun sea childA o childB del divisor (sale natural).
    float delta = horizontal ? (dx > 0 ? paso : -paso)
                             : (dy > 0 ? paso : -paso);
    *frac += delta;
    if (*frac < 0.08f) *frac = 0.08f;
    if (*frac > 0.92f) *frac = 0.92f;
    rootViewport->Resize(rootViewport->width, rootViewport->height); // relayout
}

// ARRASTRE de la ESQUINA (boton de menu, arriba-izq de cada viewport), estilo esquina de Windows:
// mueve el borde IZQUIERDO (dx) y el SUPERIOR (dy) del viewport a la vez. Solo se mueve un borde si
// el viewport esta del lado childB del divisor de ese eje (o sea, tiene un vecino a la IZQUIERDA /
// ARRIBA). Si esta pegado a ese borde de la ventana (no hay vecino), ese eje no se mueve: el viewport
// superior-izquierdo no se puede redimensionar, y uno pegado al borde izquierdo solo sube/baja.
void LayoutResizeEsquina(ViewportBase* aVp, int dx, int dy) {
    if (!aVp || !rootViewport) return;
    // borde IZQUIERDO: primer ancestro Row donde aVp cae del lado childB (derecha del split)
    if (dx != 0) {
        ViewportBase* nodo = aVp;
        while (nodo) {
            ViewportBase* padre = LayoutPadreDe(rootViewport, nodo);
            if (!padre) break;
            if (padre->ContainerKind() == 1 && ((ViewportRow*)padre)->childB == nodo) {
                ((ViewportRow*)padre)->SetSizeChildrens(dx); break; // dx<0 (arrastrar a la izq) = crece aVp
            }
            nodo = padre;
        }
    }
    // borde SUPERIOR: primer ancestro Column donde aVp cae del lado childB (abajo del split)
    if (dy != 0) {
        ViewportBase* nodo = aVp;
        while (nodo) {
            ViewportBase* padre = LayoutPadreDe(rootViewport, nodo);
            if (!padre) break;
            if (padre->ContainerKind() == 2 && ((ViewportColumn*)padre)->childB == nodo) {
                ((ViewportColumn*)padre)->SetSizeChildrens(dy); break; // dy<0 (arrastrar arriba) = crece aVp
            }
            nodo = padre;
        }
    }
}

// tras un cambio estructural: punteros frescos + relayout completo
static void LayoutRescan(ViewportBase* aFoco, int aW, int aH) {
    viewPortActive = aFoco;
    if (aFoco && aFoco->isLeaf() && aFoco->ViewportKind() == 1) {
        Viewport3DActive = (Viewport3D*)aFoco;
    } else {
        Viewport3DActive = LayoutPrimer3D(rootViewport); // puede ser NULL
    }
    rootViewport->x = 0;
    rootViewport->y = 0;
    rootViewport->Resize(aW, aH);
    if (LayoutArbolCambiado) LayoutArbolCambiado();
}

// Expand: borra al hermano y al contenedor; el viewport toma su lugar
static void LayoutExpandir(ViewportBase* aVp) {
    ViewportBase* padre = LayoutPadreDe(rootViewport, aVp);
    if (!padre) return; // es el root: no hay nada que expandir
    int w = rootViewport->width;
    int h = rootViewport->height;
    ViewportBase* hermano = NULL;
    if (padre->ContainerKind() == 1) {
        ViewportRow* r = (ViewportRow*)padre;
        hermano = (r->childA == aVp) ? r->childB : r->childA;
        r->childA = NULL;
        r->childB = NULL;
    } else {
        ViewportColumn* c = (ViewportColumn*)padre;
        hermano = (c->childA == aVp) ? c->childB : c->childA;
        c->childA = NULL;
        c->childB = NULL;
    }
    if (padre == rootViewport) {
        rootViewport = aVp;
    } else {
        LayoutReemplazarEnArbol(rootViewport, padre, aVp);
    }
    aVp->parent = NULL;
    LayoutBorrarSubarbol(hermano);
    delete padre;
    LayoutRescan(aVp, w, h);
}

// Split: en el lugar del viewport aparece una fila/columna con el
// original y un viewport NUEVO del mismo tipo (no se clona)
static void LayoutDividir(ViewportBase* aVp, bool aEnFila) {
    ViewportBase* nuevo = LayoutCrearViewport(aVp->ViewportKind() - 1);
    if (!nuevo) return;
    int w = rootViewport->width;
    int h = rootViewport->height;
    ViewportBase* cont;
    if (aEnFila) {
        cont = new ViewportRow(aVp, nuevo, 0.5f);
    } else {
        cont = new ViewportColumn(aVp, nuevo, 0.5f);
    }
    if (aVp == rootViewport) {
        rootViewport = cont;
    } else if (!LayoutReemplazarEnArbol(rootViewport, aVp, cont)) {
        rootViewport = cont; // (no deberia pasar)
    }
    LayoutRescan(aVp, w, h);
}

// ====================================================================
// menus desplegables compartidos
// ====================================================================

static PopupMenu* gMenuTipo = NULL;
static ViewportBase* gMenuTipoDe = NULL; // de que viewport se abrio

// MAXIMIZAR un viewport (fullscreen TEMPORAL, no destructivo): guarda el arbol y apunta rootViewport al
// viewport activo; restaurar vuelve el arbol intacto. Distinto de Expand (que BORRA el hermano). En
// fullscreen no se puede split/expand/cambiar tipo (el menu solo ofrece Minimize).
static ViewportBase* g_rootGuardado = NULL; // != NULL => hay un viewport MAXIMIZADO
bool LayoutEstaMaximizado() { return g_rootGuardado != NULL; }
void LayoutMaximizar() {
    if (!rootViewport) return;
    if (g_rootGuardado) { // ya maximizado -> RESTAURAR el arbol guardado
        int w = rootViewport->width, h = rootViewport->height;
        rootViewport = g_rootGuardado; g_rootGuardado = NULL;
        rootViewport->x = 0; rootViewport->y = 0; rootViewport->Resize(w, h);
    } else { // MAXIMIZAR el viewport activo (si ya es el unico, nada)
        if (!viewPortActive || viewPortActive == rootViewport) return;
        int w = rootViewport->width, h = rootViewport->height;
        g_rootGuardado = rootViewport;
        rootViewport = viewPortActive;
        rootViewport->x = 0; rootViewport->y = 0; rootViewport->Resize(w, h);
    }
    g_redraw = true;
}

// opcion del menu de tipo: cambiar / expand / split / maximizar
static void LayoutAccionTipo(int aId) {
    if (!gMenuTipoDe || !rootViewport) return;
    ViewportBase* vp = gMenuTipoDe;
    gMenuTipoDe = NULL;
    if (aId >= 100) return; // ids de los checkbox del UV editor: el item los togglea solo
    if (aId == 7) { LayoutMaximizar(); return; }          // Maximize / Minimize (fullscreen del activo)
    if (aId == 4) { LayoutExpandir(vp); return; }
    if (aId == 5) { LayoutDividir(vp, true); return; }   // Split Row
    if (aId == 6) { LayoutDividir(vp, false); return; }  // Split Column
    if (vp->ViewportKind() == aId + 1) return; // ya es de ese tipo
    int w = rootViewport->width;
    int h = rootViewport->height;
    ViewportBase* nuevo = LayoutCrearViewport(aId);
    if (!nuevo) return;
    if (vp == rootViewport) {
        rootViewport = nuevo;
    } else if (!LayoutReemplazarEnArbol(rootViewport, vp, nuevo)) {
        delete nuevo;
        return;
    }
    delete vp;
    LayoutRescan(nuevo, w, h);
}

// opcion del menu Add: crea el objeto en el cursor 3D (codigo compartido)
// ids: 0 Plane, 1 Cube, 2 Circle, 3 Vertex, 4 Empty, 5 Camera, 6 Light,
//      7 import Wavefront (dialogo de cada plataforma)
static void LayoutAccionAdd(int aId) {
    Object* nuevo = NULL;
    switch (aId) {
        case 0: nuevo = NewMesh(MeshType::plane, NULL, false); break;
        case 1: nuevo = NewMesh(MeshType::cube, NULL, false); break;
        case 2: nuevo = NewMesh(MeshType::circle, NULL, false); break;
        case 3: nuevo = NewMesh(MeshType::vertice, NULL, false); break;
        case 4: nuevo = new Empty(NULL, cursor3D.pos); break;
        case 5: nuevo = new Camera(NULL, cursor3D.pos, Vector3(-35.0f, -45.0f, 0.0f)); break;
        case 6: {
            Light* l = Light::Create(NULL, 0, 0, 0);
            if (l) { l->pos = cursor3D.pos; }
            nuevo = l;
            break;
        }
        case 7: if (LayoutImportObj) LayoutImportObj(); break;
        case 8:
            // una Collection nueva (colgada de la activa)
            nuevo = new Collection(CollectionActive ? CollectionActive
                                                    : SceneCollection);
            break;
        // objetos especiales: linkeados a un TARGET (el objeto seleccionado, o
        // NULL si no hay). Renderizan a ese target una vez / N veces / espejado.
        case 9: { // Duplicate Linked
            Instance* inst = new Instance(NULL, ObjActivo);
            inst->pos = cursor3D.pos;
            nuevo = inst;
            break;
        }
        case 10: { // Array
            Instance* inst = new Instance(NULL, ObjActivo);
            inst->count = 3;                 // cantidad de copias (editable)
            inst->pos = Vector3(2, 0, 0);    // offset por copia (sino se pisan)
            inst->name = "Array";
            nuevo = inst;
            break;
        }
        case 11: { // Mirror
            Instance* inst = new Instance(NULL, ObjActivo);
            inst->mirror = true;             // muestra el target espejado
            inst->mirrorEje = 0;             // eje X (editable)
            inst->name = "Mirror";
            nuevo = inst;
            break;
        }
        case 12: nuevo = NewMesh(MeshType::UVsphere, NULL, false); break;
        case 13: nuevo = NewMesh(MeshType::cone, NULL, false); break;
        case 14: nuevo = NewMesh(MeshType::cylinder, NULL, false); break;
    }
    if (nuevo) {
        DeseleccionarTodo();
        nuevo->Seleccionar();
    }
    // cualquier primitiva regenerable (cube/plane/circle/UVsphere): ventanita "Add ..."
    if (nuevo && nuevo->getType() == ObjectType::mesh &&
        ((Mesh*)nuevo)->meshTipo >= 0) AbrirRedoMeshPanel((Mesh*)nuevo);
}

// opcion del menu Select: 0 All / 1 None / 2 Invert
void LayoutSelectLinkedGuiado(); // definida abajo (modo guiado: pide click sobre el elemento a seleccionar)
void LayoutLoopSelectGuiado();   // definida abajo (modo guiado: pide click sobre un borde para el edge loop)
static void LayoutAccionSelect(int aId) {
    // estas funciones ya hacen lo correcto segun el modo (object/edit): la
    // logica vive adentro, NO aca (ni en el handler de teclado de cada OS).
    // Ctrl+Z: All/None/Invert cambian la seleccion de sub-elementos en Edit Mode (el loop -10/11/12-
    // captura solo en LayoutLoopSelectActivo; 13/14 solo arman un modo, no cambian nada todavia).
    if (InteractionMode == EditMode && g_editMesh && aId <= 2) UndoCapturarSeleccionEdit((Mesh*)g_editMesh);
    switch (aId) {
        case 0: SeleccionarTodoForzado(); break; // All  (A)
        case 1: DeseleccionarTodo();      break; // None (Alt A)
        case 2: InvertirSeleccion();      break; // Invert (Ctrl I)
        case 10: LayoutLoopSelectActivo(2); break; // Loop Select (Face Loop)  - modo cara
        case 11: LayoutLoopSelectActivo(0); break; // Loop Select (Edge Loop)  - modo borde
        case 12: LayoutLoopSelectActivo(1); break; // Loop Select (Edge Ring)  - modo borde
        case 13: LayoutPickPathIniciar(false); break; // Pick Shortest Path (caminito)
        case 14: LayoutPickPathIniciar(true);  break; // + Fill Region (rellena)
        case 15: LayoutSelectLinkedGuiado();   break; // Select Linked (isla conexa) en modo guiado: pide click
        case 16: LayoutLoopSelectGuiado();     break; // Loop Select en modo VERTICE: guiado (click sobre un borde)
    }
}

// reconstruye el menu Select segun el modo: All/None/Invert siempre; en Edit Mode agrega el
// Loop Select del sub-modo (en BORDE aclara el tipo: Edge Loop vs Edge Ring).
static void LayoutRebuildMenuSelect() {
    if (!MenuSelect) return;
    MenuSelect->Limpiar();
    MenuSelect->Agregar("All", 0)->atajo = "A";
    MenuSelect->Agregar("None", 1)->atajo = "Alt A";
    MenuSelect->Agregar("Invert", 2)->atajo = "Ctrl I";
    if (InteractionMode == EditMode) {
        // Select Linked (L): selecciona la ISLA conexa. Desde el menu = guiado (pide click sobre el elemento).
        MenuSelect->Agregar("Select Linked", 15)->atajo = "L";
        if (EditSelectMode == SelEdge) {
            MenuSelect->Agregar("Loop Select (Edge Loop)", 11)->atajo = "Shift Alt Click";
            MenuSelect->Agregar("Loop Select (Edge Ring)", 12);
        } else if (EditSelectMode == SelFace) {
            MenuSelect->Agregar("Loop Select", 10)->atajo = "Shift Alt Click";
        } else { // VERTICE: el loop se define por un BORDE -> modo guiado (pedi click sobre un borde)
            MenuSelect->Agregar("Loop Select (Edge Loop)", 16)->atajo = "Shift Alt Click";
        }
        // Pick Shortest Path: en los 3 sub-modos. Guiado por cartel (click 1ro -> click 2do).
        MenuSelect->Agregar("Pick Shortest Path", 13)->atajo = "Ctrl Click";
        MenuSelect->Agregar("Shortest Path (Fill Region)", 14)->atajo = "Ctrl Shift Click";
    }
}

// arranca un transform en EDIT MODE (sobre la seleccion de malla). Devuelve true si
// estamos en Edit Mode (lo manejo aca); false = Object Mode (usar los SetPosicion...).
// el transform de malla en curso es un EXTRUDE (no un Move/Rotate/Scale comun). Lo usa el boton
// "Repeat" del toolbar (solo aparece en extrude): confirma y vuelve a extruir la seleccion.
static bool g_extrudeEnCurso = false;
bool ExtrudeEnCurso(){ return g_extrudeEnCurso; }

bool EditXformStart(int est, int eje) { // expuesto (lo usa el harness para testear el move undo)
    if (InteractionMode != EditMode || !g_editMesh) return false;
    // ENCADENAR G->R->S sin click de confirmacion: si ya hay un transform en curso, CONFIRMARLO primero
    // (pushea su undo + finaliza). Sino UndoEditMoveIniciar borraba el pendiente anterior SIN guardarlo ->
    // se perdia ese paso del Ctrl+Z (bug: extrude+move+rotate -> Ctrl+Z deshacia el extrude, no el rotate).
    if (EditXformActivo()) EditXformConfirmar();
    estado = est; axisSelect = eje;
    if (est == rotacion) gTrackballCap = false; // re-captura el angulo del trackball
    UndoEditMoveIniciar((Mesh*)g_editMesh); // Ctrl+Z: captura posiciones PREVIAS (move PURO; se confirma al aceptar)
    EditXformIniciar();
    if (!EditXformActivo()) estado = editNavegacion; // sin seleccion: no-op
    else ToolbarRegistrarAccion(est == rotacion ? TBRotate : est == EditScale ? TBScale : TBMove); // historial
    return true;
}

// ============================================================================
//  SNAP (imantado): estado + buscador del punto de snap bajo el cursor.
// ============================================================================
SnapCfg g_snap = { false, SNAP_CLOSEST, SNAP_VERTEX, true,true,true, true,true,true };
bool SnapEnabled(){ return g_snap.enabled; }
void SnapToggle(){ g_snap.enabled = !g_snap.enabled; Notificar(g_snap.enabled ? "Snap: ON" : "Snap: OFF", false); g_redraw = true; }
// resultado del ultimo snap (para dibujar el recuadro verde en el viewport)
bool  g_snapHit = false;   // hubo snap en el ultimo move
float g_snapSx = 0, g_snapSy = 0; // su posicion en pantalla (viewport-relativa)

// una malla candidata a snap? (segun Target Selection). isEdited/isActive: la editada / el objeto activo.
static bool SnapMallaCandidata(Mesh* m){
    bool isEdited = ((Object*)m == g_editMesh);
    bool isActive = (ObjActivo == (Object*)m);
    if (isEdited) return g_snap.tsEdited;
    if (isActive) return g_snap.tsActive;
    return g_snap.tsNonEdited;
}
// recolecta las mallas VISIBLES de la escena
static void SnapRecolectar(Object* o, std::vector<Mesh*>& out){
    if (!o) return;
    if (o->getType()==ObjectType::mesh && o->visible && SnapMallaCandidata((Mesh*)o)) out.push_back((Mesh*)o);
    for (size_t i=0;i<o->Childrens.size();i++) SnapRecolectar(o->Childrens[i], out);
}

// busca el punto de snap bajo el cursor. Devuelve true + el punto en MUNDO + su posicion en pantalla.
bool SnapBuscarTarget(int mx, int my, Viewport3D* vp, Vector3& outWorld, float& outSx, float& outSy,
                      Vector3* outEdgeA, Vector3* outEdgeB){
    if (!vp || !SceneCollection) return false;
    float lmx = (float)mx - (float)vp->x, lmy = (float)my - (float)vp->y; // a coords del viewport (ProyectarPunto)
    std::vector<Mesh*> meshes; SnapRecolectar(SceneCollection, meshes);
    if (meshes.empty()) return false;
    const float RAD = 18.0f; // radio de enganche en pantalla (px)
    float bestD = RAD*RAD; bool found = false;
    // en modo edicion no se snapea a la PROPIA seleccion que se mueve (sino se pega a si misma)
    Mesh* em = (InteractionMode==EditMode) ? (Mesh*)g_editMesh : NULL;
    EditMesh* ee = (em && em->edit) ? em->edit : NULL;

    for (size_t mi=0; mi<meshes.size(); mi++){
        Mesh* m = meshes[mi];
        if (!m->vertex || m->vertexSize<=0) continue;
        Matrix4 W; m->GetWorldMatrix(W);
        bool esEdit = (m == em);

        if (g_snap.target==SNAP_VERTEX){
            for (int v=0; v<m->vertexSize; v++){
                if (esEdit && ee){ // saltar los verts seleccionados (se estan moviendo)
                    // busca el editable de esta pos rep; si esta seleccionado, saltar
                    int rep = (int)m->posRep.size()==m->vertexSize ? m->posRep[v] : v;
                    bool sel=false; for (size_t k=0;k<ee->editVerts.size();k++) if (ee->editVerts[k]==rep && k<ee->vertSel.size() && ee->vertSel[k]){ sel=true; break; }
                    if (sel) continue;
                }
                Vector3 wp = W * Vector3(m->vertex[v*3], m->vertex[v*3+1], m->vertex[v*3+2]);
                float sx,sy; if (!vp->ProyectarPunto(wp, sx, sy)) continue;
                float dx=sx-lmx, dy=sy-lmy, d=dx*dx+dy*dy;
                if (d<bestD){ bestD=d; outWorld=wp; outSx=sx; outSy=sy; found=true; }
            }
        } else if (g_snap.target==SNAP_EDGECENTER || g_snap.target==SNAP_EDGE){
            for (size_t e=0; e+1<m->edges.size(); e+=2){
                int a=m->edges[e], b=m->edges[e+1];
                if (a<0||b<0||a>=m->vertexSize||b>=m->vertexSize) continue;
                Vector3 wa = W * Vector3(m->vertex[a*3],m->vertex[a*3+1],m->vertex[a*3+2]);
                Vector3 wb = W * Vector3(m->vertex[b*3],m->vertex[b*3+1],m->vertex[b*3+2]);
                if (g_snap.target==SNAP_EDGECENTER){
                    Vector3 wc = (wa+wb)*0.5f;
                    float sx,sy; if (!vp->ProyectarPunto(wc,sx,sy)) continue;
                    float dx=sx-lmx,dy=sy-lmy,d=dx*dx+dy*dy;
                    if (d<bestD){ bestD=d; outWorld=wc; outSx=sx; outSy=sy; found=true; }
                } else { // EDGE: punto mas cercano del segmento (en pantalla) al cursor
                    float sax,say,sbx,sby; if (!vp->ProyectarPunto(wa,sax,say)||!vp->ProyectarPunto(wb,sbx,sby)) continue;
                    float ex=sbx-sax, ey=sby-say; float len2=ex*ex+ey*ey; float t=0.0f;
                    if (len2>1e-4f) t=((lmx-sax)*ex+(lmy-say)*ey)/len2; if (t<0)t=0; if (t>1)t=1;
                    float px=sax+ex*t, py=say+ey*t; float dx=px-lmx,dy=py-lmy,d=dx*dx+dy*dy;
                    if (d<bestD){ bestD=d; outWorld=wa+(wb-wa)*t; outSx=px; outSy=py; found=true;
                        if (outEdgeA) *outEdgeA=wa; if (outEdgeB) *outEdgeB=wb; } // extremos del borde ganador (mundo)
                }
            }
        } else { // SNAP_FACE / SNAP_FACECENTER: recorre las caras (trianguladas por abanico)
            for (size_t f=0; f<m->faces3d.size(); f++){
                const std::vector<int>& idx = m->faces3d[f].idx; int nc=(int)idx.size(); if (nc<3) continue;
                if (g_snap.target==SNAP_FACECENTER){
                    Vector3 c(0,0,0); for (int k=0;k<nc;k++){ int vi=idx[k]; c=c+Vector3(m->vertex[vi*3],m->vertex[vi*3+1],m->vertex[vi*3+2]); }
                    c = W * (c*(1.0f/(float)nc));
                    float sx,sy; if (!vp->ProyectarPunto(c,sx,sy)) continue;
                    float dx=sx-lmx,dy=sy-lmy,d=dx*dx+dy*dy;
                    if (d<bestD){ bestD=d; outWorld=c; outSx=sx; outSy=sy; found=true; }
                } else { // FACE: proyecta el cursor sobre la cara (baricentrico en pantalla) -> retopologia
                    for (int t=1; t+1<nc; t++){
                        int i0=idx[0], i1=idx[t], i2=idx[t+1];
                        Vector3 w0=W*Vector3(m->vertex[i0*3],m->vertex[i0*3+1],m->vertex[i0*3+2]);
                        Vector3 w1=W*Vector3(m->vertex[i1*3],m->vertex[i1*3+1],m->vertex[i1*3+2]);
                        Vector3 w2=W*Vector3(m->vertex[i2*3],m->vertex[i2*3+1],m->vertex[i2*3+2]);
                        float x0,y0,x1,y1,x2,y2, pw0,pw1,pw2;
                        if (!vp->ProyectarPunto(w0,x0,y0,&pw0)||!vp->ProyectarPunto(w1,x1,y1,&pw1)||!vp->ProyectarPunto(w2,x2,y2,&pw2)) continue;
                        // baricentrico del cursor en el triangulo de PANTALLA
                        float d00=(x1-x0), d01=(y1-y0), d10=(x2-x0), d11=(y2-y0);
                        float den=d00*d11-d10*d01; if (fabsf(den)<1e-4f) continue;
                        float vx=lmx-x0, vy=lmy-y0;
                        float bb=(vx*d11 - vy*d10)/den; float cc=(d00*vy - d01*vx)/den; float aa=1.0f-bb-cc;
                        if (aa< -0.001f||bb< -0.001f||cc< -0.001f) continue; // el cursor NO esta dentro del triangulo
                        // PERSPECTIVE-CORRECT: el baricentrico de PANTALLA no interpola bien la posicion de MUNDO en
                        // perspectiva (mismo problema que el texturado afin). Se divide cada peso por la profundidad
                        // del vertice y se renormaliza -> el punto cae EXACTO bajo el cursor. (En ortho pw=1 -> afin.)
                        float ia=aa/pw0, ib=bb/pw1, ic=cc/pw2, isum=ia+ib+ic;
                        if (fabsf(isum)<1e-8f) continue; ia/=isum; ib/=isum; ic/=isum;
                        Vector3 wp = w0*ia + w1*ib + w2*ic; // punto sobre la cara (mundo), corregido por perspectiva
                        // nearest a la CAMARA: usamos la profundidad en pantalla como desempate (mas cerca = gana)
                        float d = 0.0f; // el cursor esta dentro -> priorizamos por depth (aprox: menor |mundo-cam|)
                        Vector3 dcam = wp - vp->viewPos; d = dcam.x*dcam.x+dcam.y*dcam.y+dcam.z*dcam.z;
                        if (!found || d<bestD){ bestD=d; outWorld=wp; outSx=lmx; outSy=lmy; found=true; }
                    }
                }
            }
        }
    }
    return found;
}

// EXTRUDE (E / menus Vertex-Edge-Face): extruye la seleccion segun el modo y arranca
// el move de la tapa. Con caras adyacentes el move se constriñe a la normal; si no
// (verts/aristas sueltas) es LIBRE (plano de la camara). Solo en Edit Mode.
void LayoutExtrudeFaces() {
    if (InteractionMode != EditMode || !g_editMesh) return;
    Mesh* m = (Mesh*)g_editMesh;
    Vector3 dir; bool constrain = false;
    if (!m->ExtruirEdit(dir, constrain)) return;
    if (constrain) {
        EditXformIniciarExtrude(dir); // move por la normal promedio
    } else {
        estado = translacion; axisSelect = ViewAxis; // move LIBRE
        EditXformIniciar();
        if (!EditXformActivo()) estado = editNavegacion;
    }
    if (EditXformActivo()){ ToolbarRegistrarAccion(TBExtrude); g_extrudeEnCurso = true; } // historial + marca extrude
}

// DUPLICATE en Edit Mode (Shift+D): copia la seleccion y arranca un move LIBRE.
void LayoutDuplicarEdit() {
    if (InteractionMode != EditMode || !g_editMesh) return;
    Mesh* m = (Mesh*)g_editMesh;
    if (!m->DuplicarSeleccionEdit()) return;
    estado = translacion; axisSelect = ViewAxis;
    EditXformIniciar();
    if (!EditXformActivo()) estado = editNavegacion;
}

// RIP (V) en Edit Mode: SEPARA la malla a lo largo de la seleccion (loop de bordes, verts o caras). Deja
// seleccionada la pieza nueva (separada). No arranca move: la idea es separar -> L una mitad -> borrar.
void LayoutRipEdit() {
    if (InteractionMode != EditMode || !g_editMesh) return;
    Mesh* m = (Mesh*)g_editMesh;
    if (!m->RipSeleccionEdit()) { Notificar("Rip: the selection does not separate the mesh", true); g_redraw = true; return; }
    Notificar("Rip: mesh separated", false);
    g_redraw = true;
}

// F = "New Edge/Face from Vertices" (menu Vertex): conecta los verts seleccionados.
void LayoutNewFaceEdit() {
    if (InteractionMode != EditMode || !g_editMesh) return;
    Mesh* m = (Mesh*)g_editMesh;
    m->CrearCaraEdit();
}

// Shade Smooth/Flat (menu Face): redondea/aplana las caras seleccionadas.
void LayoutShade(bool smooth) {
    if (InteractionMode != EditMode || !g_editMesh) return;
    Mesh* m = (Mesh*)g_editMesh;
    m->ShadeEdit(smooth);
}

// Mark Sharp / Clear Sharp (menu Edge o tecla W): marca/desmarca como filosos los bordes
// seleccionados. En una malla SMOOTH, un borde sharp NO promedia -> queda flat (cilindro:
// lados suaves + tapas planas + aro filoso). Ver Mesh::MarcarSharpEdit / CornerNormalConSharp.
void LayoutMarkSharp(bool sharp) {
    if (InteractionMode != EditMode || !g_editMesh) return;
    Mesh* m = (Mesh*)g_editMesh;
    m->MarcarSharpEdit(sharp);
    g_redraw = true;
}

static void LayoutAccionObject(int aId); // (definida mas abajo; la usa el menu UV)
static void LayoutAccionMesh(int aId);   // (def. mas abajo) accion del menu "Mesh" de Edit Mode
static void AccionMerge(int modo);       // (def. mas abajo) Merge de la seleccion (At Center/Cursor/Collapse/By Distance)
static void AccionSetParent(int aId);    // (def. mas abajo) emparentar los seleccionados al activo
static void AccionClearParent(int aId);  // (def. mas abajo) desemparentar los seleccionados

// ===== menu UV (tecla U / "UV"): Mark/Clear Seam + proyecciones =====
// Mark/Clear Seam: bordes MAGENTA donde el unwrap abre la costura del UV.
void LayoutMarkSeam(bool seam) {
    if (InteractionMode != EditMode || !g_editMesh) return;
    ((Mesh*)g_editMesh)->MarcarSeamEdit(seam);
    g_redraw = true;
}
// Cube(0)/Cylinder(1)/Sphere(2) projection sobre las caras seleccionadas.
void LayoutProyectarUV(int tipo) {
    if (InteractionMode != EditMode || !g_editMesh) return;
    ((Mesh*)g_editMesh)->ProyectarUVCaras(tipo);
    g_redraw = true;
    const char* n = (tipo == 0) ? "cube" : (tipo == 1) ? "cylinder" : "sphere";
    Notificar(std::string("UV: ") + n + " projection", false);
}
// Project from View (+Bounds): proyecta los verts de las caras seleccionadas a coords de
// PANTALLA -> UV (con la camara actual). bounds = re-normaliza la seleccion a [0,1].
void LayoutProyectarUVDesdeVista(bool bounds) {
    if (InteractionMode != EditMode || !g_editMesh || !Viewport3DActive) return;
    Mesh* m = (Mesh*)g_editMesh; m->EnsureEdit();
    if (!m->edit || !m->vertex) return;
    EditMesh* e = m->edit;
    std::vector<unsigned char> sel3d(m->faces3d.size(), 0); bool hay = false;
    for (size_t f = 0; f < e->faceSel.size(); f++)
        if (e->faceSel[f] && f < e->faceSrc.size()) { int f3 = e->faceSrc[f]; if (f3>=0 && f3<(int)m->faces3d.size()) { sel3d[f3]=1; hay=true; } }
    if (!hay) { Notificar("Project from View: select faces first", true); return; }
    Matrix4 W; m->GetWorldMatrix(W);
    const int nC = m->ContarCorners();
    std::vector<float> uvL((size_t)nC*2, 0.0f);
    float vw = (float)Viewport3DActive->width, vh = (float)Viewport3DActive->height; if (vw<1) vw=1; if (vh<1) vh=1;
    float umin=1e30f, vmin=1e30f, umax=-1e30f, vmax=-1e30f;
    int L = 0;
    for (size_t f = 0; f < m->faces3d.size(); f++) {
        const std::vector<int>& idx = m->faces3d[f].idx; int cnt = (int)idx.size();
        if (f < sel3d.size() && sel3d[f]) for (int c = 0; c < cnt; c++) {
            const float* p = &m->vertex[idx[c]*3];
            Vector3 wp = W * Vector3(p[0], p[1], p[2]);
            float sx = 0, sy = 0; Viewport3DActive->ProyectarPunto(wp, sx, sy);
            float u = sx/vw, v = sy/vh;
            uvL[(size_t)(L+c)*2] = u; uvL[(size_t)(L+c)*2+1] = v;
            if (u<umin) umin=u; if (u>umax) umax=u; if (v<vmin) vmin=v; if (v>vmax) vmax=v;
        }
        L += cnt;
    }
    if (bounds && umax > umin && vmax > vmin) {
        int L2 = 0;
        for (size_t f = 0; f < m->faces3d.size(); f++) {
            int cnt = (int)m->faces3d[f].idx.size();
            if (f < sel3d.size() && sel3d[f]) for (int c = 0; c < cnt; c++) {
                uvL[(size_t)(L2+c)*2]   = (uvL[(size_t)(L2+c)*2]   - umin) / (umax-umin);
                uvL[(size_t)(L2+c)*2+1] = (uvL[(size_t)(L2+c)*2+1] - vmin) / (vmax-vmin);
            }
            L2 += cnt;
        }
    }
    m->EscribirUVProyeccion(uvL);
    g_redraw = true;
    Notificar(bounds ? "UV: projected from view (bounds)" : "UV: projected from view", false);
}
// el menu UV (tecla U o el header "UV"): operaciones sobre las CARAS seleccionadas.
static PopupMenu* gMenuUVops = NULL; // file-static: LayoutCambiarMenuBarra lo necesita (izq/der para salir del menu UV)
void LayoutMenuUV(int mx, int my) {
    if (InteractionMode != EditMode || !g_editMesh) return;
    if (!gMenuUVops) {
        gMenuUVops = new PopupMenu(); gMenuUVops->titulo = "UV"; gMenuUVops->action = LayoutAccionObject;
        gMenuUVops->Agregar("Unwrap", 350)->atajo = "soon";
        gMenuUVops->Agregar("Smart UV Project", 351)->atajo = "soon";
        gMenuUVops->Agregar("Follow Active Quads", 352)->atajo = "soon";
        gMenuUVops->Agregar("Cube Projection", 353);
        gMenuUVops->Agregar("Cylinder Projection", 354);
        gMenuUVops->Agregar("Sphere Projection", 355);
        gMenuUVops->Agregar("Project from View", 356);
        gMenuUVops->Agregar("Project from View (Bounds)", 357);
        gMenuUVops->Agregar("Mark Seam", 358);
        gMenuUVops->Agregar("Clear Seam", 359);
    }
    if (MenuAbierto) MenuAbierto->Cerrar();
    gMenuUVops->Abrir(mx, my, MenuPantallaW, MenuPantallaH);
    MenuAbierto = gMenuUVops;
}

// Recalculate Normals (menu Face): re-orienta las caras seleccionadas (o todas) hacia
// AFUERA y abre el panel "redo" con la tilde Inside (misma tarjeta que el panel de Add).
void LayoutRecalcNormales() {
    if (InteractionMode != EditMode || !g_editMesh) return;
    Mesh* m = (Mesh*)g_editMesh;
    if (m->RecalcularOrientacionEdit(false)) {
        AbrirRedoNormalesPanel(m);
        g_redraw = true;
    }
}

// Flip Normals (menu Mesh > Normals > Flip): invierte las normales de la seleccion (o todas). Simple.
void LayoutFlipNormales() {
    if (InteractionMode != EditMode || !g_editMesh) return;
    Mesh* m = (Mesh*)g_editMesh;
    if (m->FlipNormalesEdit()) g_redraw = true;
}

// Triangulate Faces (Ctrl+T, menu Face): parte las caras seleccionadas de >3 lados en triangulos.
void LayoutTriangulate() {
    if (InteractionMode != EditMode || !g_editMesh) return;
    Mesh* m = (Mesh*)g_editMesh;
    if (m->TriangularSeleccionEdit()) { Notificar("Faces triangulated", false); g_redraw = true; } // false = exito (verde)
    else Notificar("Select faces with more than 3 sides to triangulate", true);                    // true = error (rojo)
}

static void AccionDelete(int aId); // ejecuta Delete Vertices/Edges/Faces/Edge Loops (ids 361-364); definida mas abajo

// opcion del menu Object/Mesh y su submenu Transform (ids 100-102, 300=extrude)
static void LayoutAccionObject(int aId) {
    switch (aId) {
        case 1: DuplicatedObject(); break; // Duplicate Objects (Shift D)
        case 2: NewInstance();      break; // Duplicate Linked  (Alt D)
        case 3: AbrirConfirmarBorrado(); break; // Delete (X): popup de confirmacion -> Si borra (con undo)
        case 5: JoinObjetos(); break;           // Join (Ctrl J): une las mallas seleccionadas en el objeto activo
        case 220: AplicarTransform(0); break;   // Apply Location
        case 221: AplicarTransform(1); break;   // Apply Rotation
        case 222: AplicarTransform(2); break;   // Apply Scale
        case 223: AplicarTransform(3); break;   // Apply All Transforms
        case 100: if (!EditXformStart(translacion, ViewAxis)) SetPosicion(); break; // Move  (G)
        case 101: if (!EditXformStart(rotacion,    ViewAxis)) SetRotacion(); break; // Rotate(R)
        case 102: if (!EditXformStart(EditScale,   XYZ))      SetEscala();   break; // Scale (S)
        case 103: LayoutShrinkFatten(); break; // Shrink/Fatten (Alt+S): cada vert por su normal
        case 300: LayoutExtrudeFaces(); break; // Extrude (segun el modo) (E)
        case 310: LayoutNewFaceEdit(); break;  // Vertex > New Edge/Face from Vertices (F)
        case 314: LayoutDuplicarEdit(); break; // Duplicate (Shift D)
        case 341: LayoutRipEdit();      break; // Rip (V): separa la malla por la seleccion
        // Delete: los items del submenu/atajo-X despachan por ESTA accion (el menu top es el de contexto, no gMenuDelete)
        case 361: case 362: case 363: case 364: AccionDelete(aId); break; // Vertices/Edges/Faces/Edge Loops
        case 315: break;                       // UV > Unwrap (pendiente)
        case 320: LayoutShade(true);  break;   // Face > Shade Smooth
        case 321: LayoutShade(false); break;   // Face > Shade Flat
        case 322: LayoutRecalcNormales(); break; // Recalculate Normals (Face / Mesh>Normals)
        case 323: LayoutTriangulate();    break; // Face > Triangulate Faces (Ctrl T)
        case 324: LayoutFlipNormales();   break; // Mesh > Normals > Flip
        case 330: LayoutMarkSharp(true);  break; // Edge > Mark Sharp
        case 331: LayoutMarkSharp(false); break; // Edge > Clear Sharp
        case 340: LayoutLoopCutDesdeActivo(); break; // Edge/Face > Loop Cut and Slide (elemento activo)
        case 350: Notificar("Unwrap: not implemented yet", false); break;              // UV > Unwrap (pendiente: LSCM)
        case 351: Notificar("Smart UV Project: not implemented yet", false); break;    // UV > Smart UV Project (pendiente)
        case 352: Notificar("Follow Active Quads: not implemented yet", false); break; // UV > Follow Active Quads (pendiente)
        case 353: LayoutProyectarUV(0); break; // UV > Cube Projection
        case 354: LayoutProyectarUV(1); break; // UV > Cylinder Projection
        case 355: LayoutProyectarUV(2); break; // UV > Sphere Projection
        case 356: LayoutProyectarUVDesdeVista(false); break; // UV > Project from View
        case 357: LayoutProyectarUVDesdeVista(true);  break; // UV > Project from View (Bounds)
        case 358: LayoutMarkSeam(true);  break; // UV > Mark Seam
        case 359: LayoutMarkSeam(false); break; // UV > Clear Seam
        case 200: SetOriginGeometryToOrigin(); break; // Set Origin > Geometry to Origin
        case 201: SetOriginOriginToGeometry(); break; // Set Origin > Origin to Geometry
        case 202: SetOriginToCursor();         break; // Set Origin > Origin to 3D Cursor
        // Set Parent (Ctrl P) / Clear Parent (Ctrl Alt P): submenus de Object + standalone. Ids unicos ->
        // despachan por ESTA accion sea como submenu (menu top = Object) o standalone (menu top = el propio).
        case 230: case 231: case 232: case 233: AccionSetParent(aId - 230); break; // Object / Keep T. / Without Inv. / Keep T. Without Inv.
        case 240: case 241: case 242:           AccionClearParent(aId - 240); break; // Clear / Clear+Keep T. / Clear Inverse
        case 380: case 381: case 382: case 383: AccionMerge(aId - 380); break; // Merge: At Center / At Cursor / Collapse / By Distance
    }
}

// opcion del menu Render: el modo de vista del viewport 3D activo
static void LayoutAccionRender(int aId) {
    if (!Viewport3DActive) return;
    switch (aId) {
        case 0: Viewport3DActive->view = RenderType::Rendered;        break;
        case 1: Viewport3DActive->view = RenderType::MaterialPreview; break;
        case 2: Viewport3DActive->view = RenderType::Solid;           break;
        case 3: Viewport3DActive->view = RenderType::Wireframe;       break;
        case 4: Viewport3DActive->view = RenderType::ZBuffer;         break;
        case 5: Viewport3DActive->view = RenderType::NormalView;      break;
        case 6: Viewport3DActive->view = RenderType::Alpha;           break;
    }
}

// opcion del menu Orient: orientacion usada al constrenir a un eje (X/Y/Z)
static void LayoutAccionOrient(int aId) {
    if (aId == 0)      transformOrientation = GlobalOrient;
    else if (aId == 1) transformOrientation = LocalOrient;
    else if (aId == 2) transformOrientation = ViewOrient;
    else if (aId == 3) transformOrientation = NormalOrient; // = la normal de la seleccion (extrude)
    // VIEW no tiene eje Y (es la profundidad de la vista): si el constraint lo incluia, se libera
    if (transformOrientation == ViewOrient &&
        (axisSelect == Y || axisSelect == PlaneX || axisSelect == PlaneZ))
        axisSelect = (estado == EditScale) ? XYZ : ViewAxis;
    // con un transform EN CURSO la nueva orientacion se re-aplica al instante (como las teclas X/Y/Z)
    if (estado != editNavegacion) ReestablecerEstado(false);
}

// abre el menu de ORIENTACION desde la barra de HERRAMIENTAS (abajo): el menu crece hacia
// ARRIBA del boton (syTop = borde superior de la barra) para no salirse de la pantalla.
void LayoutMenuOrientToolbar(int sx, int syTop){
    if (!MenuOrient) return;
    MenuOrient->action = LayoutAccionOrient;
    MenuOrient->Resize();
    int my = syTop - MenuOrient->height;
    if (my < 0) my = 0;
    MenuOrient->Abrir(sx, my, MenuPantallaW, MenuPantallaH);
    MenuAbierto = MenuOrient;
}

// opcion del menu View > Viewpoint: cambia el punto de vista del viewport activo (los MISMOS atajos del numpad,
// ahora VISIBLES en el menu -> nada oculto). ids 400-406.
static void LayoutAccionView(int aId) {
    if (!Viewport3DActive) return;
    switch (aId) {
        case 400: Viewport3DActive->SetViewFromCameraActive(!Viewport3DActive->ViewFromCameraActive); break; // Num 0: vista desde la camara (toggle, igual que la tecla 0; SetViewpoint no tiene caso camera)
        case 401: Viewport3DActive->SetViewpoint(Viewpoint::top);    break; // Num 7
        case 402: Viewport3DActive->SetViewpoint(Viewpoint::bottom); break; // Ctrl Num 7
        case 403: Viewport3DActive->SetViewpoint(Viewpoint::front);  break; // Num 1
        case 404: Viewport3DActive->SetViewpoint(Viewpoint::back);   break; // Ctrl Num 1
        case 405: Viewport3DActive->SetViewpoint(Viewpoint::right);  break; // Num 3
        case 406: Viewport3DActive->SetViewpoint(Viewpoint::left);   break; // Ctrl Num 3
        case 407: Viewport3DActive->ChangePerspective();            break; // Num 5: alterna perspectiva/ortografica
        // submenu Cameras:
        case 410: SetActiveObjectAsCamera(); break; // Set Active Object as Camera (Ctrl Num 0): SOLO setea la camara activa, NO cambia la vista
        case 411: Viewport3DActive->SetViewFromCameraActive(!Viewport3DActive->ViewFromCameraActive);   break; // Active Camera (Num 0): ver desde la camara
        case 420: Viewport3DActive->EnfocarObject(); break; // Frame Selected (Numpad .): enfoca la seleccion
        case 421: Viewport3DActive->lockOrbit = !Viewport3DActive->lockOrbit; break; // Lock Orbit: orbitar -> panear
    }
}

// deriva g_editMesh (la malla que se esta editando) del modo + objeto activo. HAY QUE
// llamarla cada vez que cambia InteractionMode o ObjActivo. COMPARTIDA PC+Symbian: antes
// solo la seteaba el render de PC (ViewPort3D::Render), asi que en Symbian g_editMesh
// quedaba NULL -> en Edit Mode no se podia ni seleccionar ni mover sub-elementos.
// regenera el preview SOLO de las mallas que tienen un modificador MIRROR con TARGET (su plano de espejo sale del
// mundo del target relativo al objeto -> si cualquiera de los dos se movio, cambia). El resto de modificadores es
// local y no depende de la posicion. Recorre el arbol; barato: los que no tienen modificadores se saltean.
static void RegenerarMirrorsConTargetRec(Object* nodo){
    if (!nodo) return;
    for (size_t i=0;i<nodo->Childrens.size();i++){
        Object* o = nodo->Childrens[i];
        if (o->getType()==ObjectType::mesh){
            Mesh* m=(Mesh*)o;
            for (size_t k=0;k<m->modificadores.size();k++)
                if (m->modificadores[k]->tipo==ModifierType::Mirror && m->modificadores[k]->target){ m->GenerarMallaModificada(); break; }
        }
        RegenerarMirrorsConTargetRec(o);
    }
}

void ActualizarEditMeshActivo() {
    g_editMesh = (InteractionMode == EditMode && ObjActivo &&
                  ObjActivo->getType() == ObjectType::mesh) ? ObjActivo : NULL;
    // MIRROR con TARGET: si se movio algun objeto (flag que prenden los transforms de objeto), su plano cambio ->
    // regenerar SOLO esos previews. Chequeo barato (1 bool/frame); no corre nada al orbitar/idle.
    if (g_objetosMovidos) {
        g_objetosMovidos = false;
        if (SceneCollection) RegenerarMirrorsConTargetRec(SceneCollection);
        g_redraw = true;
    }
    // Esta funcion se llama CADA FRAME (ViewPort3D::Render). El UNICO motivo para regenerar aca es el CAMBIO DE MODO
    // (entrar/salir de Edit): el filtro mostrarEdit puede saltear un modificador en Edit, asi que el preview cambia.
    // NO se regenera por seleccionar/activar un objeto (seleccionar no cambia la geometria -> el preview ya esta
    // cacheado en genValido), NI cada frame (antes se recalculaba la subdivision/screw en cada redibujo -> lentisimo
    // en el N95). Los demas cambios (params del modificador, mover verts, cortes, undo) los regeneran por su cuenta;
    // el mirror con TARGET lo regenera el confirm de mover objetos.
    static int prevMode = -999;
    if (InteractionMode != prevMode) {
        prevMode = InteractionMode;
        if (ObjActivo && ObjActivo->getType() == ObjectType::mesh) {
            Mesh* m = (Mesh*)ObjActivo;
            if (!m->modificadores.empty()) { m->GenerarMallaModificada(); g_redraw = true; }
        }
    }
    // EDICION QUE CAMBIA LA TOPOLOGIA (extrude, loop cut, delete, merge, subdivide...): todas llaman
    // GenerarRender() -> genValido=false (la malla generada quedo stale). Regeneramos el modificador
    // UNA sola vez: al hacerlo genValido vuelve a true, asi que NO es un regen por-frame (GenerarRender
    // solo lo llaman las ops de edicion, nunca el render). Sin esto, tras extrude/loop cut el
    // modificador (subdivision/screw) mostraba geometria vieja hasta mover un vertice.
    if (ObjActivo && ObjActivo->getType() == ObjectType::mesh) {
        Mesh* m = (Mesh*)ObjActivo;
        if (!m->modificadores.empty() && !m->genValido) { m->GenerarMallaModificada(); g_redraw = true; }
    }
}

// opcion del menu Mode: cambia el modo del objeto ACTIVO (Object/Edit/Paint).
// Solo aplica a una MALLA; Edit y los Paint todavia no hacen nada (placeholder).
static void LayoutAccionMode(int aId) {
    if (ObjActivo && ObjActivo->getType() == ObjectType::mesh)
        InteractionMode = aId;
    else
        InteractionMode = ObjectMode;
    ActualizarEditMeshActivo(); // refresca g_editMesh (PC + Symbian)
}

// opcion del menu SelMode (edit): sub-elemento Vertex/Edge/Face. Al cambiar de
// modo hay que RECOLOREAR (sino quedan los colores del modo anterior, ej: el
// degradado de vertex en edge) y se resetea el activo (no hay activo del modo nuevo).
static void LayoutAccionSelMode(int aId) {
    EditSelectMode = aId; // SelVertex/SelEdge/SelFace
    if (g_editMesh) {
        Mesh* m = (Mesh*)g_editMesh;
        m->EnsureEdit();
        if (m->edit) { m->edit->activeIdx = -1; m->edit->Recolorear(); }
    }
}

// abre el menu de TIPO/split del viewport (boton [0] de la barra), por codigo
// (sin hit-test). Lo usan el click en la flechita Y la navegacion por teclado
// (soft-izq en outliner/propiedades, o izquierda desde Select en el 3D).
void LayoutAbrirMenuTipo(ViewportBase* aVp) {
    if (!aVp || aVp->BarButtons.empty()) return;
    if (!gMenuTipo) {
        gMenuTipo = new PopupMenu();
        gMenuTipo->action = LayoutAccionTipo;
    }
    // se reconstruye en cada apertura: "Expand" no existe para el root.
    gMenuTipo->Limpiar();
    if (LayoutEstaMaximizado()) {
        // en FULLSCREEN no se cambia tipo/split/expand: solo restaurar el layout
        gMenuTipo->Agregar("Minimize", 7);
    } else {
        gMenuTipo->Agregar("3D Viewport", 0);
        gMenuTipo->Agregar("Outliner", 1);
        gMenuTipo->Agregar("Properties", 2);
        gMenuTipo->Agregar("UV Editor", 3);
        if (aVp != rootViewport) gMenuTipo->Agregar("Expand", 4);
        gMenuTipo->Agregar("Split Row", 5);
        gMenuTipo->Agregar("Split Column", 6);
        if (aVp != rootViewport) gMenuTipo->Agregar("Maximize", 7); // fullscreen del viewport activo
    }
    gMenuTipoDe = aVp;
    if (MenuAbierto && MenuAbierto != gMenuTipo) MenuAbierto->Cerrar();
    aVp->barFocusIndex = 0; // resaltar [0] + auto-scroll de la barra
    aVp->ActualizarBarra(); // sx/sy de [0] YA con el scroll
    Button* b = aVp->BarButtons[0];
    gMenuTipo->Abrir(b->sx, b->sy + b->height - GlobalScale,
                     MenuPantallaW, MenuPantallaH);
    MenuAbierto = gMenuTipo;
}

// click en la flechita de la barra: abre el menu de tipo de viewport
static bool LayoutClickBotonTipo(ViewportBase* aVp, int aX, int aY) {
    if (!aVp || aVp->BarButtons.empty()) return false;
    if (!aVp->BarButtons[0]->Contains(aX, aY)) return false;
    LayoutAbrirMenuTipo(aVp);
    return true;
}

// ===== menu del boton "View" del UV Editor (checkboxes) =====
static PopupMenu* gMenuUV = NULL;
static void LayoutAbrirMenuUV(UVEditor* uv, int x, int y) {
    if (!uv) return;
    if (!gMenuUV) gMenuUV = new PopupMenu();
    gMenuUV->Limpiar();
    // los togglea el propio item (AgregarCheck sobre el bool* del UV editor)
    gMenuUV->AgregarCheck("Sync Selection", 0, &uv->syncSelection);
    gMenuUV->AgregarCheck("Repeat Texture", 1, &uv->repeatTexture);
    gMenuUV->AgregarCheck("Show Chrome UV", 2, &uv->mostrarChromeUV); // overlay LIVE del reflejo equirect (demo)
    gMenuUV->action = NULL;
    if (MenuAbierto && MenuAbierto != gMenuUV) MenuAbierto->Cerrar();
    gMenuUV->Abrir(x, y, MenuPantallaW, MenuPantallaH);
    MenuAbierto = gMenuUV;
}
// ===== menu "Snap" del UV editor (cursor 2D <-> seleccion) =====
static PopupMenu* gMenuUVSnap = NULL;
static UVEditor*  gUVSnapTarget = NULL; // sobre que editor opera el snap (se setea al abrir)
static void LayoutAccionUVSnap(int id) {
    if (!gUVSnapTarget) return;
    if (id == 0) gUVSnapTarget->SnapCursorToSel();
    else if (id == 1) gUVSnapTarget->SnapSelToCursor();
    else if (id == 2) gUVSnapTarget->CursorToCenter();
}
static void LayoutAbrirMenuUVSnap(UVEditor* uv, int x, int y) {
    if (!uv) return;
    gUVSnapTarget = uv;
    if (!gMenuUVSnap) gMenuUVSnap = new PopupMenu();
    gMenuUVSnap->Limpiar();
    gMenuUVSnap->titulo = "Snap";
    gMenuUVSnap->Agregar("Cursor to Selection", 0);
    gMenuUVSnap->Agregar("Selection to Cursor", 1);
    gMenuUVSnap->Agregar("Cursor to Center", 2);
    gMenuUVSnap->action = LayoutAccionUVSnap;
    if (MenuAbierto && MenuAbierto != gMenuUVSnap) MenuAbierto->Cerrar();
    gMenuUVSnap->Abrir(x, y, MenuPantallaW, MenuPantallaH);
    MenuAbierto = gMenuUVSnap;
}

// ===== selector de MODO de seleccion del UV (Vertex/Edge/Face), PROPIO del editor =====
static PopupMenu* gMenuUVSelMode = NULL;
static UVEditor*  gUVModeTarget = NULL;
static void LayoutAccionUVSelMode(int id) {
    if (!gUVModeTarget) return;
    if (gUVModeTarget->syncSelection) {   // sincronizado: cambia el modo del 3D (espeja)
        EditSelectMode = id;
        if (g_editMesh) { Mesh* m = (Mesh*)g_editMesh; m->EnsureEdit();
            if (m->edit) { m->edit->activeIdx = -1; m->edit->Recolorear(); } }
    } else {
        gUVModeTarget->uvSelMode = id;    // independiente del 3D
    }
}
static void LayoutAbrirMenuUVSelMode(UVEditor* uv, int x, int y) {
    if (!uv) return;
    gUVModeTarget = uv;
    if (!gMenuUVSelMode) gMenuUVSelMode = new PopupMenu();
    gMenuUVSelMode->Limpiar();
    gMenuUVSelMode->titulo = "Select Mode";
    int cur = uv->ModoUV();
    gMenuUVSelMode->Agregar("Vertex", SelVertex, (int)IconType::selVertex)->verde = (cur == SelVertex);
    gMenuUVSelMode->Agregar("Edge",   SelEdge,   (int)IconType::selEdge)->verde   = (cur == SelEdge);
    gMenuUVSelMode->Agregar("Face",   SelFace,   (int)IconType::selFace)->verde   = (cur == SelFace);
    gMenuUVSelMode->action = LayoutAccionUVSelMode;
    if (MenuAbierto && MenuAbierto != gMenuUVSelMode) MenuAbierto->Cerrar();
    gMenuUVSelMode->Abrir(x, y, MenuPantallaW, MenuPantallaH);
    MenuAbierto = gMenuUVSelMode;
}

// click en la barra del UV editor: [1]=View, [2]=SelMode (propio), [3]=Pivot (= menu del 3D), [4]=Snap
static bool LayoutClickBarraUV(UVEditor* uv, int mx, int my) {
    if (!uv) return false;
    std::vector<Button*>& B = uv->BarButtons;
    if (B.size() > 1 && B[1]->visible && B[1]->Contains(mx, my)) {       // View (checkboxes)
        uv->ActualizarBarra(); // sx/sy frescos
        LayoutAbrirMenuUV(uv, B[1]->sx, B[1]->sy + B[1]->height - GlobalScale);
        return true;
    }
    if (B.size() > 2 && B[2]->visible && B[2]->Contains(mx, my)) {        // SelMode (Vertex/Edge/Face)
        uv->ActualizarBarra();
        LayoutAbrirMenuUVSelMode(uv, B[2]->sx, B[2]->sy + B[2]->height - GlobalScale);
        return true;
    }
    if (B.size() > 3 && B[3]->visible && B[3]->Contains(mx, my)) {        // Pivot (reusa gMenuPivot)
        uv->ActualizarBarra();
        LayoutMenuPivot(B[3]->sx, B[3]->sy + B[3]->height - GlobalScale);
        return true;
    }
    if (B.size() > 4 && B[4]->visible && B[4]->Contains(mx, my)) {        // Snap
        uv->ActualizarBarra();
        LayoutAbrirMenuUVSnap(uv, B[4]->sx, B[4]->sy + B[4]->height - GlobalScale);
        return true;
    }
    return false;
}

bool LayoutMenuAbierto() {
    return MenuAbierto && MenuAbierto->abierto;
}

// menu Transform Pivot Point (boton [3] de la barra). Se declara aca arriba porque
// lo usan el dispatch + la navegacion de la barra; se arma en LayoutMenuPivot (abajo).
static PopupMenu* gMenuPivot = NULL;
// menus de contexto de Edit Mode (boton [6] / W). Aca arriba: los usan dispatch + nav.
static PopupMenu* gMenuVertex = NULL;
static PopupMenu* gMenuEdge   = NULL;
static PopupMenu* gMenuFace   = NULL;

// abre el menu del boton de barra del 3D bajo (mx,my): [1] Select, [2] Add,
// [3] Object, [4] Overlays. Si ya hay OTRO menu abierto lo cierra y abre el
// nuevo (cambio por hover / click); si el de ese boton ya esta abierto, nada.
// Devuelve true si quedo abierto un menu de la barra.
bool LayoutAbrirMenuDeBarra(ViewportBase* vp, int mx, int my) {
    if (!vp || !vp->isLeaf() || vp->ViewportKind() != 1) return false;
    std::vector<Button*>& B = vp->BarButtons;

    PopupMenu* objetivo = NULL;     // menu desplegable a abrir
    Button* boton = NULL;           // su boton en la barra
    bool overlays = false;          // el de overlays se abre distinto (flags)
    // botones por ROL (NO por indice): reordenar la barra (mover Orient, etc.) no rompe esto.
    Button* bMode = BarRolBtn(B, BR_Mode);   Button* bSelM = BarRolBtn(B, BR_SelMode);
    Button* bPiv  = BarRolBtn(B, BR_Pivot);  Button* bSel  = BarRolBtn(B, BR_Select);
    Button* bAdd  = BarRolBtn(B, BR_Add);    Button* bObj  = BarRolBtn(B, BR_Object);
    Button* bOvl  = BarRolBtn(B, BR_Overlays); Button* bRnd = BarRolBtn(B, BR_Render);
    Button* bOri  = BarRolBtn(B, BR_Orient); Button* bUV   = BarRolBtn(B, BR_UV);
    Button* bView = BarRolBtn(B, BR_View);   Button* bMesh = BarRolBtn(B, BR_Mesh);
    Button* bSnap = BarRolBtn(B, BR_Snap);
    if (MenuMode && bMode && bMode->visible && bMode->Contains(mx, my)) {
        objetivo = MenuMode; boton = bMode;
        if (!MenuMode->action) MenuMode->action = LayoutAccionMode;
    } else if (MenuSelMode && bSelM && bSelM->visible && bSelM->Contains(mx, my)) {
        objetivo = MenuSelMode; boton = bSelM;
        if (!MenuSelMode->action) MenuSelMode->action = LayoutAccionSelMode;
    } else if (bPiv && bPiv->visible && bPiv->Contains(mx, my)) {
        // Pivot: el menu se REARMA cada vez (marca el activo) -> via LayoutMenuPivot
        if (MenuAbierto) MenuAbierto->Cerrar();
        LayoutMenuPivot(bPiv->sx, bPiv->sy + bPiv->height - GlobalScale);
        return true;
    } else if (bSnap && bSnap->visible && bSnap->Contains(mx, my)) {
        // Snap: el menu se REARMA cada vez (Base/Target marcan el activo + labels) -> via LayoutMenuSnapTool
        if (MenuAbierto) MenuAbierto->Cerrar();
        LayoutMenuSnapTool(bSnap->sx, bSnap->sy + bSnap->height - GlobalScale);
        return true;
    } else if (MenuView && bView && bView->visible && bView->Contains(mx, my)) {
        objetivo = MenuView; boton = bView;   // "View" (antes de Select): submenu Viewpoint
        if (!MenuView->action) MenuView->action = LayoutAccionView;
        // refrescar el tilde de "Lock Orbit" con el estado del viewport activo (el menu se arma 1 sola vez)
        extern MenuItem* MenuItemLockOrbit;
        if (MenuItemLockOrbit && Viewport3DActive) MenuItemLockOrbit->verde = Viewport3DActive->lockOrbit;
    } else if (MenuSelect && bSel && bSel->visible && bSel->Contains(mx, my)) {
        objetivo = MenuSelect; boton = bSel;
        LayoutRebuildMenuSelect();   // mode-aware: agrega Loop Select en Edit (cara/borde)
        if (!MenuSelect->action) MenuSelect->action = LayoutAccionSelect;
    } else if (MenuAdd && bAdd && bAdd->visible && bAdd->Contains(mx, my)) {
        objetivo = MenuAdd; boton = bAdd;
        if (!MenuAdd->action) MenuAdd->action = LayoutAccionAdd;
    } else if (MenuMesh && bMesh && bMesh->visible && bMesh->Contains(mx, my)) {
        // Edit Mode: menu "Mesh" (Transform/Snap/Delete), comun a vertice/borde/cara.
        objetivo = MenuMesh; boton = bMesh;
        if (!MenuMesh->action) MenuMesh->action = LayoutAccionMesh;
    } else if (bObj && bObj->visible && bObj->Contains(mx, my)) {
        // Edit Mode -> menu de contexto Vertex/Edge/Face; Object Mode -> menu "Object".
        if (InteractionMode == EditMode) {
            if (MenuAbierto) MenuAbierto->Cerrar();
            LayoutMenuEditContexto(bObj->sx, bObj->sy + bObj->height - GlobalScale);
            return true;
        } else if (MenuObject) {
            objetivo = MenuObject; boton = bObj;
            if (!MenuObject->action) MenuObject->action = LayoutAccionObject;
        }
    } else if (MenuRender && bRnd && bRnd->visible && bRnd->Contains(mx, my)) {
        objetivo = MenuRender; boton = bRnd;
        if (!MenuRender->action) MenuRender->action = LayoutAccionRender;
    } else if (MenuOrient && bOri && bOri->visible && bOri->Contains(mx, my)) {
        objetivo = MenuOrient; boton = bOri;
        if (!MenuOrient->action) MenuOrient->action = LayoutAccionOrient;
    } else if (bUV && bUV->visible && bUV->Contains(mx, my)) {
        // UV (edit mode): menu Mark Seam + proyecciones
        if (MenuAbierto) MenuAbierto->Cerrar();
        LayoutMenuUV(bUV->sx, bUV->sy + bUV->height - GlobalScale);
        return true;
    } else if (bOvl && bOvl->visible && bOvl->Contains(mx, my)) {
        overlays = true; boton = bOvl;
    }
    if (!boton) return false;

    // el desplegable toca el borde INFERIOR del boton (solapado 1px para que
    // su borde superior se funda con el del boton y no quede doble linea)
    int menuY = boton->sy + boton->height - GlobalScale;
    if (overlays) {
        // OJO: la 1ra vez MenuOverlays es NULL (se crea al abrirlo). Sin el
        // "MenuOverlays &&", NULL==NULL daba "ya abierto" y no abria nada.
        if (MenuOverlays && MenuAbierto == MenuOverlays) return true; // ya abierto
        if (MenuAbierto) MenuAbierto->Cerrar();
        static_cast<Viewport3D*>(vp)->AbrirMenuOverlays(boton->sx, menuY);
        return true;
    }
    if (MenuAbierto == objetivo) return true; // ese menu ya esta abierto
    if (MenuAbierto) MenuAbierto->Cerrar();    // cerrar el otro (cambio de menu)
    objetivo->Abrir(boton->sx, menuY, MenuPantallaW, MenuPantallaH);
    MenuAbierto = objetivo;
    return true;
}

// abre/cierra la barra de menu del viewport ACTIVO (soft-izquierda en Symbian).
// Abre el PRIMER menu visible (saltea el icono de tipo, B[0]) SIN preseleccionar
// item (Abrir deja selectIndex=-1): asi izq/der cambian de menu y recien abajo
// se entra al desplegable. Si ya hay un menu abierto, lo cierra (toggle).
void LayoutToggleBarraViewportActivo() {
    if (LayoutMenuAbierto()) { if (MenuAbierto) MenuAbierto->Cerrar(); return; }
    ViewportBase* vp = viewPortActive;
    if (!vp || !vp->isLeaf()) return;
    if (vp->ViewportKind() == 1) {
        // 3D: abre el primer menu visible (Select). Izquierda llega al [0].
        std::vector<Button*>& B = vp->BarButtons;
        for (int i = 1; i < (int)B.size(); i++) {
            if (B[i]->visible) {
                vp->barFocusIndex = i;     // la barra se auto-scrollea para centrarlo
                vp->ActualizarBarra();     // recalcula sx/sy YA con el scroll
                LayoutAbrirMenuDeBarra(vp, B[i]->sx + B[i]->width / 2, B[i]->sy + B[i]->height / 2);
                return;
            }
        }
    } else if (vp->ViewportKind() == 3) {
        // propiedades: foco en la PESTAÑA activa (te ahorra subir hasta arriba).
        // Desde ahi izq/der cambian de pestaña; izq en la 1ra llega al [0].
        ((Properties*)vp)->focoEnTabs = true;
    } else {
        // outliner (u otros sin menus de barra): abre el menu de tipo/split ([0])
        LayoutAbrirMenuTipo(vp);
    }
}

// flechas izq/der con un menu de barra abierto: salta al boton de menu vecino
// (Select/Add/Object/Overlays) salteando los ocultos, y abre su desplegable
static void LayoutCambiarMenuBarra(int dir) {
    // el menu de tipo/split de un panel NO-3D (outliner/propiedades) no tiene
    // menus hermanos para ciclar: izq/der no hacen nada ahi.
    if (MenuAbierto == gMenuTipo && gMenuTipoDe != (ViewportBase*)Viewport3DActive) return;
    Viewport3D* vp = Viewport3DActive;
    if (!vp || vp->BarButtons.size() < 2) return;
    std::vector<Button*>& B = vp->BarButtons;
    const int maxIdx = (int)B.size() - 1; // rango DINAMICO (no hardcodear: el ultimo
                                          // boton -Orient/etc.- quedaba afuera y se salteaba)
    // mapeo MENU->ROL (estable) y rol->INDICE via BarRolIdx (dinamico) -> reordenar no rompe la nav.
    int idx = -1;
    if (MenuAbierto == gMenuTipo) idx = 0; // boton [0] = tipo/split del viewport (idx fijo)
    else {
        int rol = -1;
        if (MenuAbierto == MenuMode) rol = BR_Mode;
        else if (MenuAbierto == MenuSelMode) rol = BR_SelMode;
        else if (MenuAbierto == gMenuPivot) rol = BR_Pivot;
        else if (MenuAbierto == MenuSelect) rol = BR_Select;
        else if (MenuAbierto == MenuAdd) rol = BR_Add;
        else if (MenuAbierto == MenuObject || MenuAbierto == gMenuVertex ||
                 MenuAbierto == gMenuEdge  || MenuAbierto == gMenuFace) rol = BR_Object;
        else if (MenuAbierto == MenuMesh) rol = BR_Mesh; // menu "Mesh" de Edit Mode
        else if (MenuAbierto == MenuOverlays) rol = BR_Overlays;
        else if (MenuAbierto == MenuView) rol = BR_View; // FIX: faltaba -> izq/der se clavaba en el menu View
        else if (MenuAbierto == MenuRender) rol = BR_Render;
        else if (MenuAbierto == MenuOrient) rol = BR_Orient;
        else if (MenuAbierto == gMenuUVops) rol = BR_UV; // FIX: faltaba -> izq/der no salia del menu UV
        if (rol >= 0) idx = BarRolIdx(B, rol);
    }
    if (idx < 0) return; // el abierto no es de la barra
    // el [0] (tipo/split) SIEMPRE es navegable; el resto salta los ocultos
    for (int k = 0; k <= maxIdx; k++) {
        idx += dir;
        if (idx > maxIdx) idx = 0;
        if (idx < 0) idx = maxIdx;
        if (idx == 0 || B[idx]->visible) break;
    }
    if (idx == 0) { LayoutAbrirMenuTipo(vp); return; } // [0]: abre tipo/split
    Button* b = B[idx];
    vp->barFocusIndex = idx;   // la barra se auto-scrollea para centrar el nuevo
    vp->ActualizarBarra();     // recalcula sx/sy YA con el scroll antes del hit-test
    LayoutAbrirMenuDeBarra(vp, b->sx + b->width / 2, b->sy + b->height / 2);
}

// MISMO comportamiento que LayoutCambiarMenuBarra pero para el EDITOR UV (su barra tiene menus PROPIOS:
// [1]View [2]SelMode [3]Pivot [4]Snap). Antes la nav izq/der estaba atada a Viewport3DActive -> en el UV
// solo se llegaba al [0] (tipo) y no se podia ir a View/Pivot/Snap (Dante). Indices FIJOS (la barra UV no se
// reordena), a diferencia del 3D que mapea por ROL.
static void LayoutCambiarMenuBarraUV(int dir) {
    if (!viewPortActive || !viewPortActive->isLeaf() || viewPortActive->ViewportKind() != 4) return;
    UVEditor* uv = (UVEditor*)viewPortActive;
    std::vector<Button*>& B = uv->BarButtons;
    if (B.size() < 2) return;
    const int maxIdx = (int)B.size() - 1;
    int idx = -1;
    if      (MenuAbierto == gMenuTipo)      idx = 0;
    else if (MenuAbierto == gMenuUV)        idx = 1;
    else if (MenuAbierto == gMenuUVSelMode) idx = 2;
    else if (MenuAbierto == gMenuPivot)     idx = 3;
    else if (MenuAbierto == gMenuUVSnap)    idx = 4;
    if (idx < 0) return;
    for (int k = 0; k <= maxIdx; k++) {       // avanza saltando los ocultos ([0] siempre navegable)
        idx += dir;
        if (idx > maxIdx) idx = 0;
        if (idx < 0) idx = maxIdx;
        if (idx == 0 || B[idx]->visible) break;
    }
    if (idx == 0) { LayoutAbrirMenuTipo(uv); return; }
    uv->barFocusIndex = idx;
    uv->ActualizarBarra();
    Button* b = B[idx];
    int mx = b->sx, my = b->sy + b->height - GlobalScale;
    if      (idx == 1) LayoutAbrirMenuUV(uv, mx, my);
    else if (idx == 2) LayoutAbrirMenuUVSelMode(uv, mx, my);
    else if (idx == 3) LayoutMenuPivot(mx, my);
    else if (idx == 4) LayoutAbrirMenuUVSnap(uv, mx, my);
}

// ====================================================================
// menus de emparentar (ctrl+P / alt+P), como Blender
// ====================================================================

static PopupMenu* gMenuSetParent = NULL;
static PopupMenu* gMenuClearParent = NULL;

// los seleccionados (sin contar al activo / a los ya-raiz)
static void RecolectarSeleccionados(Object* aNodo, std::vector<Object*>& aSel,
                                    Object* aExcluir) {
    if (!aNodo) return;
    for (size_t i = 0; i < aNodo->Childrens.size(); i++) {
        Object* o = aNodo->Childrens[i];
        if (o->select && o != aExcluir) aSel.push_back(o);
        RecolectarSeleccionados(o, aSel, aExcluir);
    }
}

// 0 Object, 1 Keep Transform, 2 Without Inverse, 3 Keep T. Without Inv.
// (sin matrices de inversa propias: 0==2 y 1==3 en nuestro modelo)
static void AccionSetParent(int aId) {
    if (!ObjActivo || !SceneCollection) return;
    std::vector<Object*> sel;
    RecolectarSeleccionados(SceneCollection, sel, ObjActivo);
    for (size_t i = 0; i < sel.size(); i++) {
        if (aId == 1 || aId == 3) ReparentKeepTransform(sel[i], ObjActivo);
        else ReparentSimple(sel[i], ObjActivo);
    }
}

// 0 Clear Parent, 1 Clear and Keep Transformation, 2 Clear Parent Inverse
static void AccionClearParent(int aId) {
    if (!SceneCollection) return;
    std::vector<Object*> sel;
    RecolectarSeleccionados(SceneCollection, sel, NULL);
    for (size_t i = 0; i < sel.size(); i++) {
        if (!sel[i]->Parent || sel[i]->Parent == SceneCollection) continue;
        if (aId == 1) ReparentKeepTransform(sel[i], SceneCollection);
        else ReparentSimple(sel[i], SceneCollection);
    }
}

// Los menus Set/Clear Parent se usan de DOS formas: standalone (Ctrl+P / Ctrl+Alt+P) y como SUBMENUS del menu
// "Object" de la barra. Para que anden igual en ambos casos usan ids UNICOS (230-233 / 240-242) + la accion
// LayoutAccionObject (que rutea esos ids a AccionSetParent/AccionClearParent), como el submenu Apply/Delete.
PopupMenu* LayoutSubmenuSetParent() {
    if (!gMenuSetParent) {
        gMenuSetParent = new PopupMenu();
        gMenuSetParent->titulo = "Set Parent To";
        gMenuSetParent->action = LayoutAccionObject;
        gMenuSetParent->Agregar("Object", 230);
        gMenuSetParent->Agregar("Object (Keep Transform)", 231);
        gMenuSetParent->Agregar("Object (Without Inverse)", 232);
        gMenuSetParent->Agregar("Object (Keep Transform Without Inverse)", 233);
    }
    return gMenuSetParent;
}
PopupMenu* LayoutSubmenuClearParent() {
    if (!gMenuClearParent) {
        gMenuClearParent = new PopupMenu();
        gMenuClearParent->titulo = "Clear Parent";
        gMenuClearParent->action = LayoutAccionObject;
        gMenuClearParent->Agregar("Clear Parent", 240);
        gMenuClearParent->Agregar("Clear and Keep Transformation", 241);
        gMenuClearParent->Agregar("Clear Parent Inverse", 242);
    }
    return gMenuClearParent;
}

void LayoutMenuParent(bool aClear, int mx, int my) {
    if (aClear) {
        PopupMenu* m = LayoutSubmenuClearParent();
        m->Abrir(mx, my, MenuPantallaW, MenuPantallaH);
        MenuAbierto = m;
    } else {
        if (!ObjActivo) return; // no hay a quien emparentar
        PopupMenu* m = LayoutSubmenuSetParent();
        m->Abrir(mx, my, MenuPantallaW, MenuPantallaH);
        MenuAbierto = m;
    }
}

// ====================================================================
// menu DELETE de Edit Mode (X / Backspace): borra vertices/aristas/caras.
// Sale CERCA DEL CURSOR (no en el objeto) para que sea usable con el mouse.
// ====================================================================
static PopupMenu* gMenuDelete = NULL;

// 361 Vertices, 362 Edges, 363 Faces, 364 Edge Loops (ver Mesh::BorrarSeleccionEdit/BorrarEdgeLoopEdit). Ids UNICOS
// (no 0/1/2/3) porque el submenu "Delete" de los menus de contexto despacha por LayoutAccionObject, no por esta.
static void AccionDelete(int aId) {
    if (estado != editNavegacion) return;
    if (InteractionMode != EditMode || !g_editMesh) return;
    if (aId == 364) { // Edge Loops: disuelve el loop seleccionado (inverso del loop cut)
        if (!((Mesh*)g_editMesh)->BorrarEdgeLoopEdit()) Notificar("Delete Edge Loops: select an edge loop first", true);
        g_redraw = true; return;
    }
    int dt = (aId == 361) ? SelVertex : (aId == 362) ? SelEdge : SelFace;
    ((Mesh*)g_editMesh)->BorrarSeleccionEdit(dt);
    g_redraw = true;
}

// crea (1 vez) el menu Delete. Lo comparten el atajo X (LayoutDeleteEdit, lo abre EN EL CURSOR) y el SUBMENU
// "Delete" (flecha a la derecha) al fondo de los menus de contexto Vertex/Edge/Face. Misma instancia para los dos.
static void EnsureMenuDelete() {
    if (gMenuDelete) return;
    gMenuDelete = new PopupMenu();
    gMenuDelete->titulo = "Delete";
    // OJO: action = LayoutAccionObject (NO AccionDelete). Como submenu, sus items se despachan por la accion del menu
    // TOP (el de contexto = LayoutAccionObject); usar la misma accion + ids unicos 361-364 hace que funcione IGUAL
    // sea como submenu (menu de contexto) o standalone (atajo X, donde este ES el menu top).
    gMenuDelete->action = LayoutAccionObject;
    gMenuDelete->Agregar("Vertices", 361);
    gMenuDelete->Agregar("Edges", 362);
    gMenuDelete->Agregar("Faces", 363);
    gMenuDelete->Agregar("Edge Loops", 364); // disuelve el loop (inverso del loop cut)
}

// abre el menu Delete EN EL CURSOR (atajo X) si estamos en Edit Mode. Devuelve true si lo abrio
// (el caller NO debe borrar objetos). En Object Mode devuelve false.
bool LayoutDeleteEdit(int mx, int my) {
    if (estado != editNavegacion) return false;
    if (InteractionMode != EditMode || !g_editMesh) return false;
    EnsureMenuDelete();
    gMenuDelete->Abrir(mx, my, MenuPantallaW, MenuPantallaH);
    MenuAbierto = gMenuDelete;
    return true;
}

// APPLY (Ctrl+A, Object Mode): abre el menu Apply (Location/Rotation/Scale/All) EN EL CURSOR. Comparte MenuApply
// (submenu de Object, construido en ViewPort3D.cpp) -> sus items 220-223 despachan por LayoutAccionObject sea
// como submenu (menu top = Object) o standalone (menu top = MenuApply, por eso le seteamos la action aca).
void LayoutApplyMenu(int mx, int my) {
    if (estado != editNavegacion || InteractionMode != ObjectMode || !MenuApply) return;
    if (!MenuApply->action) MenuApply->action = LayoutAccionObject;
    MenuApply->Abrir(mx, my, MenuPantallaW, MenuPantallaH);
    MenuAbierto = MenuApply;
}

// menus de CONTEXTO de Edit Mode (Vertex / Edge / Face), separados (no submenus de
// "Mesh"). Se abren con W o el boton de barra, EN (mx,my). Comparten LayoutAccionObject.
// (gMenuVertex/Edge/Face se declaran mas arriba: los usan dispatch + nav.)
void LayoutMenuEditContexto(int mx, int my) {
    if (InteractionMode != EditMode) return;
    PopupMenu* m = NULL;
    if (EditSelectMode == SelFace) {
        if (!gMenuFace) {
            gMenuFace = new PopupMenu(); gMenuFace->titulo = "Face"; gMenuFace->action = LayoutAccionObject;
            gMenuFace->Agregar("Extrude Faces", 300)->atajo = "E";
            gMenuFace->Agregar("Loop Cut and Slide", 340)->atajo = "Ctrl R";
            gMenuFace->Agregar("Rip", 341)->atajo = "V";
            gMenuFace->Agregar("Shade Smooth", 320);
            gMenuFace->Agregar("Shade Flat", 321);
            gMenuFace->Agregar("Recalculate Normals", 322);
            gMenuFace->Agregar("Triangulate Faces", 323)->atajo = "Ctrl T";
            // (Delete se movio al menu "Mesh": es comun a vertice/borde/cara)
        }
        m = gMenuFace;
    } else if (EditSelectMode == SelEdge) {
        if (!gMenuEdge) {
            gMenuEdge = new PopupMenu(); gMenuEdge->titulo = "Edge"; gMenuEdge->action = LayoutAccionObject;
            gMenuEdge->Agregar("Extrude Edges", 300)->atajo = "E";
            gMenuEdge->Agregar("Loop Cut and Slide", 340)->atajo = "Ctrl R";
            gMenuEdge->Agregar("Rip", 341)->atajo = "V";
            gMenuEdge->Agregar("Mark Sharp", 330)->atajo = "W";
            gMenuEdge->Agregar("Clear Sharp", 331);
            // (Delete se movio al menu "Mesh": es comun a vertice/borde/cara)
        }
        m = gMenuEdge;
    } else {
        if (!gMenuVertex) {
            gMenuVertex = new PopupMenu(); gMenuVertex->titulo = "Vertex"; gMenuVertex->action = LayoutAccionObject;
            gMenuVertex->Agregar("New Edge/Face from Vertices", 310)->atajo = "F";
            gMenuVertex->Agregar("Extrude Vertices", 300)->atajo = "E";
            gMenuVertex->Agregar("Rip", 341)->atajo = "V";
            // (Delete se movio al menu "Mesh": es comun a vertice/borde/cara)
        }
        m = gMenuVertex;
    }
    if (MenuAbierto) MenuAbierto->Cerrar();
    m->Abrir(mx, my, MenuPantallaW, MenuPantallaH);
    MenuAbierto = m;
}

// menu ADD en el cursor (Shift+A en Object Mode): es el MISMO MenuAdd de la barra,
// abierto donde esta el mouse (como Blender). Reusa LayoutAccionAdd.
void LayoutMenuAdd(int mx, int my) {
    if (!MenuAdd) return; // se crea en el setup de menus (1er frame)
    if (!MenuAdd->action) MenuAdd->action = LayoutAccionAdd;
    if (MenuAbierto) MenuAbierto->Cerrar();
    MenuAdd->Abrir(mx, my, MenuPantallaW, MenuPantallaH); // EN EL CURSOR
    MenuAbierto = MenuAdd;
}

// menu SHARP en el cursor (tecla W en Edit Mode): elegir si los bordes seleccionados son
// afilados (Mark Sharp) o suaves (Clear Sharp). Reusa el dispatch LayoutAccionObject (330/331).
static PopupMenu* gMenuSharp = NULL;
void LayoutMenuSharp(int mx, int my) {
    if (InteractionMode != EditMode || !g_editMesh) return;
    if (!gMenuSharp) {
        gMenuSharp = new PopupMenu(); gMenuSharp->titulo = "Edge"; gMenuSharp->action = LayoutAccionObject;
        gMenuSharp->Agregar("Mark Sharp", 330);
        gMenuSharp->Agregar("Clear Sharp", 331);
    }
    if (MenuAbierto) MenuAbierto->Cerrar();
    gMenuSharp->Abrir(mx, my, MenuPantallaW, MenuPantallaH);
    MenuAbierto = gMenuSharp;
}

// ====================================================================
// menu TRANSFORM PIVOT POINT (objeto + edit): desde donde/como rotan-escalan.
// 4 modos estilo Blender + checkbox "Lock Normals". Sale en el cursor.
// (gMenuPivot se declara mas arriba, junto al dispatch de la barra.)
// ====================================================================
static void AccionPivot(int aId) {
    if (aId >= 0 && aId <= 3) g_transformPivot = aId; // 0..3 = enum TransformPivot
    // id 9 = el checkbox Lock Normals (lo maneja AgregarCheck, no hace falta accion)
}

// ===== menu SNAP (boton "Snap" de la barra): Enable + Snap Base + Snap Target + Affect + Target Selection =====
static PopupMenu* gMenuSnapTool=NULL, *gMenuSnapBase=NULL, *gMenuSnapTarget=NULL;
static void AccionSnapBase(int id){ g_snap.base=id; g_redraw=true; }
static void AccionSnapTarget(int id){ g_snap.target=id; g_redraw=true; }
// El dispatch (LayoutClickUI) SIEMPRE llama al action del menu TOP con el id del item elegido,
// aunque el item venga de un submenu. Por eso el id de Base/Target se rutea aca por RANGO
// (100+base, 200+target); los checkbox del top usan id 0 y su toggle lo hace el propio item.
static void AccionSnapRouter(int id){
    if (id >= 200) AccionSnapTarget(id - 200);
    else if (id >= 100) AccionSnapBase(id - 100);
}
static const char* SnapBaseNom(int b){ return b==SNAP_CLOSEST?"Closest":b==SNAP_CENTER?"Center":b==SNAP_MEDIAN?"Median":"Active"; }
static const char* SnapTargetNom(int t){ return t==SNAP_VERTEX?"Vertex":t==SNAP_EDGE?"Edge":t==SNAP_FACE?"Face":t==SNAP_EDGECENTER?"Edge Center":"Face Center"; }
void LayoutMenuSnapTool(int mx, int my){
    if (!gMenuSnapBase){ gMenuSnapBase=new PopupMenu(); gMenuSnapBase->titulo="Snap Base"; }
    gMenuSnapBase->Limpiar();
    for (int b=SNAP_CLOSEST;b<=SNAP_ACTIVE;b++) gMenuSnapBase->Agregar(SnapBaseNom(b), 100+b)->verde=(g_snap.base==b);
    if (!gMenuSnapTarget){ gMenuSnapTarget=new PopupMenu(); gMenuSnapTarget->titulo="Snap Target"; }
    gMenuSnapTarget->Limpiar();
    for (int t=SNAP_VERTEX;t<=SNAP_FACECENTER;t++) gMenuSnapTarget->Agregar(SnapTargetNom(t), 200+t)->verde=(g_snap.target==t);
    if (!gMenuSnapTool){ gMenuSnapTool=new PopupMenu(); gMenuSnapTool->titulo="Snap"; gMenuSnapTool->action=AccionSnapRouter; }
    gMenuSnapTool->Limpiar();
    gMenuSnapTool->AgregarCheck("Enable", 0, &g_snap.enabled)->atajo="Shift Tab";
    // el resto se ve en GRIS cuando el snap esta apagado (->gris = &g_snap.enabled): sin snap
    // ninguna de estas opciones hace efecto, igual que "Show Overlays" grisa sus hijos.
    gMenuSnapTool->Agregar(std::string("Snap Base: ")+SnapBaseNom(g_snap.base), 0, -1, gMenuSnapBase)->gris = &g_snap.enabled;
    gMenuSnapTool->Agregar(std::string("Snap Target: ")+SnapTargetNom(g_snap.target), 0, -1, gMenuSnapTarget)->gris = &g_snap.enabled;
    gMenuSnapTool->AgregarCheck("Affect Move",   0, &g_snap.afMove)->gris = &g_snap.enabled;
    gMenuSnapTool->AgregarCheck("Affect Rotate", 0, &g_snap.afRot)->gris = &g_snap.enabled;
    gMenuSnapTool->AgregarCheck("Affect Scale",  0, &g_snap.afScale)->gris = &g_snap.enabled;
    gMenuSnapTool->AgregarCheck("Include Active",     0, &g_snap.tsActive)->gris = &g_snap.enabled;
    gMenuSnapTool->AgregarCheck("Include Edited",     0, &g_snap.tsEdited)->gris = &g_snap.enabled;
    gMenuSnapTool->AgregarCheck("Include Non-Edited", 0, &g_snap.tsNonEdited)->gris = &g_snap.enabled;
    if (MenuAbierto) MenuAbierto->Cerrar();
    gMenuSnapTool->Abrir(mx, my, MenuPantallaW, MenuPantallaH);
    MenuAbierto = gMenuSnapTool;
}

void LayoutMenuPivot(int mx, int my) {
    if (!gMenuPivot) {
        gMenuPivot = new PopupMenu();
        gMenuPivot->titulo = "Transform Pivot Point";
        gMenuPivot->action = AccionPivot;
    }
    // se rearma cada vez: cada opcion con su ICONO y la ACTIVA en verde
    gMenuPivot->Limpiar();
    gMenuPivot->Agregar("3D Cursor", PivotCursor3D, IconType::pivotCursor)->verde = (g_transformPivot==PivotCursor3D);
    gMenuPivot->Agregar("Individual Origins", PivotIndividual, IconType::pivotIndividual)->verde = (g_transformPivot==PivotIndividual);
    gMenuPivot->Agregar("Median Point", PivotMedian, IconType::pivotMedian)->verde = (g_transformPivot==PivotMedian);
    gMenuPivot->Agregar("Active Element", PivotActive, IconType::pivotActive)->verde = (g_transformPivot==PivotActive);
    gMenuPivot->AgregarCheck("Lock Normals", 9, &g_editLockNormales);
    gMenuPivot->Abrir(mx, my, MenuPantallaW, MenuPantallaH); // EN EL CURSOR
    MenuAbierto = gMenuPivot;
}

// ====================================================================
//  TRANSFORM de sub-elementos en EDIT MODE (G/R/S sobre verts/aristas/caras)
// ====================================================================
// Mueve/rota/escala los VERTICES seleccionados de la malla EN EDICION (no el
// objeto). Vive en el EDITOR: reusa el estado y la matematica del transform de
// objetos (estado/axisSelect/transformOrientation/cam/pivot + EjeOrientado) pero
// el TARGET son los vertices. Trabaja en MUNDO (TRS global de la malla, constante
// durante el drag): snapshot de las posiciones de mundo al empezar, acumula la
// transform y reescribe desde el snapshot (sin drift). Al CONFIRMAR recalcula
// bordes + normales (salvo Lock Normals); al CANCELAR restaura el snapshot.

struct EditVtxSnap {
    int editK;      // indice del vertice editable; su pos[] es autoritativa (un valor por
                    // posicion; los GPU duplicados los empuja EmpujarPosiciones via posRep)
    Vector3 world0; // posicion de MUNDO al empezar
    Vector3 worldNormal; // normal del vertice en MUNDO (para Shrink/Fatten: cada vert se mueve por SU normal)
};
static std::vector<EditVtxSnap> gEVsnap;
static Mesh*      gEVmesh   = NULL;
static Quaternion gEVrg;            // rotacion global de la malla (constante en el drag)
static Vector3    gEVsg(1,1,1);     // escala global
static Vector3    gEVorigin;        // origen global
static Vector3    gEVpivot;         // pivote en MUNDO
// acumuladores (segun 'estado' solo uno esta activo)
static Vector3    gEVtrans;         // translacion de mundo acumulada
static Quaternion gEVrotTotal(1,0,0,0); // rotacion acumulada
static float      gEVscaleAmt = 0;  // factor de escala acumulado (f = 1 + amt); en SHRINK = distancia por la normal
static bool       gEVshrink = false; // SHRINK/FATTEN (Alt+S): reusa EditScale pero cada vert se mueve por SU normal
bool EditShrinkActivo(){ return gEVshrink; }
// EXTRUDE / orientacion NORMAL: la translacion se constriñe a gTransformNormal (la normal en mundo).
// gEVuseCustom + gTransformNormal son GLOBALES (variables.h) para que CiclarEje/EjeOrientado los vean.

static Vector3 EVLocalAMundo(const Vector3& p){
    return gEVorigin + gEVrg * Vector3(gEVsg.x*p.x, gEVsg.y*p.y, gEVsg.z*p.z);
}
static Vector3 EVMundoALocal(const Vector3& w){
    Vector3 d = gEVrg.Inverted() * (w - gEVorigin);
    return Vector3(gEVsg.x!=0.0f?d.x/gEVsg.x:d.x,
                   gEVsg.y!=0.0f?d.y/gEVsg.y:d.y,
                   gEVsg.z!=0.0f?d.z/gEVsg.z:d.z);
}

bool EditXformActivo(){ return gEVmesh != NULL; }
// valores acumulados para la barra de estado (la rotacion usa gAnguloTransform)
Vector3 EditXformTransDelta(){ return gEVtrans; }       // translacion de MUNDO (engine xyz)
float   EditXformScaleFactor(){ return 1.0f + gEVscaleAmt; }
float   EditXformShrinkAmt(){ return gEVscaleAmt; }     // distancia por la normal (Shrink/Fatten)

void EditXformIniciar(){
    g_xformPrimerMov = true; // el primer motion arranca en cero (no usa el delta viejo)
    gEVsnap.clear(); gEVmesh = NULL;
    gEVshrink = false; // por defecto es un transform comun; el starter de Shrink/Fatten lo prende despues
    ClipMirrorReset(); // nuevo transform: ningun vert esta "pegado" al plano del mirror todavia
    if (InteractionMode != EditMode || !g_editMesh) return;
    Mesh* m = (Mesh*)g_editMesh; m->EnsureEdit();
    if (!m->edit || !m->vertex) return;
    EditMesh* e = m->edit;
    const int nV = m->vertexSize;
    const bool hayRep = ((int)m->posRep.size() == nV);

    // que VERTICES editables estan seleccionados segun el MODO (vertex/edge/face)
    std::vector<char> selEdit(e->editVerts.size(), 0);
    if (EditSelectMode == SelVertex){
        for (size_t k=0;k<e->vertSel.size();k++) if (e->vertSel[k]) selEdit[k]=1;
    } else if (EditSelectMode == SelEdge){
        for (size_t eg=0; eg<e->edgeSel.size(); eg++) if (e->edgeSel[eg]){
            selEdit[e->lineIdx[eg*2]]=1; selEdit[e->lineIdx[eg*2+1]]=1;
        }
    } else {
        for (size_t f=0; f<e->faces.size(); f++) if (f<e->faceSel.size() && e->faceSel[f])
            for (size_t c=0;c<e->faces[f].size();c++) selEdit[e->faces[f][c]]=1;
    }

    gEVmesh = m;
    gEVrg = RotGlobalDe(m); gEVsg = ScaleGlobalDe(m); gEVorigin = m->GetGlobalPosition();

    // grupo de GPU verts por POSICION de cada editable seleccionado (mover uno mueve
    // a todos sus duplicados de UV/normales en el mismo lugar)
    Vector3 nNormAcum(0,0,0); // suma de las normales de los verts seleccionados (orientacion Normal)
    for (size_t k=0;k<e->editVerts.size();k++){
        if (!selEdit[k]) continue;
        int rep = e->editVerts[k]; if (rep<0||rep>=nV) continue;
        EditVtxSnap s; s.editK=(int)k;
        // posicion EDITABLE (autoritativa) -> mundo. No lee el render (vertex[]).
        Vector3 l0(e->pos[k*3], e->pos[k*3+1], e->pos[k*3+2]);
        s.world0 = EVLocalAMundo(l0);
        // normal del vertice en MUNDO (para Shrink/Fatten). rotacion global (la escala no cambia el sentido).
        Vector3 ln = m->normals ? Vector3(m->normals[rep*3]/127.0f, m->normals[rep*3+1]/127.0f, m->normals[rep*3+2]/127.0f) : Vector3(0,1,0);
        s.worldNormal = (gEVrg * ln).Normalized();
        gEVsnap.push_back(s);
        if (m->normals) nNormAcum = nNormAcum + ln;
    }
    if (gEVsnap.empty()){ gEVmesh = NULL; return; } // nada seleccionado

    // pivote en MUNDO segun el modo (3D cursor o el centro de la seleccion)
    if (g_transformPivot == PivotCursor3D){
        gEVpivot = cursor3D.pos;
    } else {
        float cx,cy,cz;
        if (e->CentroSeleccion(cx,cy,cz)) gEVpivot = EVLocalAMundo(Vector3(cx,cy,cz));
        else gEVpivot = gEVorigin;
    }
    TransformPivotPoint = gEVpivot; // el gizmo (linea punteada + ejes) se dibuja aca
    gEVtrans = Vector3(0,0,0); gEVrotTotal = Quaternion(1,0,0,0); gEVscaleAmt = 0;
    gAnguloTransform = 0.0f;
    // orientacion NORMAL (menu): el move se constrine a la normal de la seleccion (mismo path
    // que el extrude). El extrude la pisa con su propia normal en EditXformIniciarExtrude.
    gEVuseCustom = false;
    if (transformOrientation == NormalOrient) {
        Vector3 nw = gEVrg * nNormAcum; // a mundo (rotacion global de la malla)
        float ln = sqrtf(nw.x*nw.x + nw.y*nw.y + nw.z*nw.z);
        if (ln > 1e-6f) { gTransformNormal = Vector3(nw.x/ln, nw.y/ln, nw.z/ln); gEVuseCustom = true; }
    }
}

// arranca el MOVE del extrude: la tapa ya esta seleccionada (ExtruirCarasEdit la
// dejo asi); se mueve constreñida a la normal promedio (en MUNDO).
void EditXformIniciarExtrude(const Vector3& normalLocal){
    estado = translacion;
    EditXformIniciar(); // snapshot de la tapa seleccionada
    if (!EditXformActivo()){ estado = editNavegacion; return; }
    Vector3 a = gEVrg * normalLocal;          // a mundo (la rotacion global de la malla)
    float ln = sqrtf(a.x*a.x + a.y*a.y + a.z*a.z);
    if (ln > 1e-6f){ gTransformNormal = Vector3(a.x/ln, a.y/ln, a.z/ln); gEVuseCustom = true; }
}

// recomputa cada vertice desde su world0 + el acumulado activo y lo escribe en la
// malla (todos los GPU del grupo) + refresca el overlay.
static void EVEscribir(){
    if (!gEVmesh) return;
    Mesh* m = gEVmesh;
    for (size_t i=0;i<gEVsnap.size();i++){
        EditVtxSnap& s = gEVsnap[i];
        Vector3 wn;
        if (estado == translacion){
            wn = s.world0 + gEVtrans;
        } else if (estado == rotacion){
            wn = gEVpivot + gEVrotTotal * (s.world0 - gEVpivot);
        } else if (gEVshrink){ // SHRINK/FATTEN: cada vert se mueve por SU normal (mundo) * distancia acumulada
            wn = s.world0 + s.worldNormal * gEVscaleAmt;
        } else { // EditScale: escala DIRECCIONAL segun el eje/plano (orientacion)
            Vector3 off = s.world0 - gEVpivot;
            if (axisSelect==X||axisSelect==Y||axisSelect==Z){
                Vector3 a = EjeOrientado(*m, axisSelect);
                wn = gEVpivot + off + a*(off.Dot(a)*gEVscaleAmt);
            } else if (axisSelect==PlaneX||axisSelect==PlaneY||axisSelect==PlaneZ){
                int ex=(axisSelect==PlaneX)?X:(axisSelect==PlaneY)?Y:Z;
                Vector3 a = EjeOrientado(*m, ex);
                Vector3 inPlane = off - a*off.Dot(a);
                wn = gEVpivot + off + inPlane*gEVscaleAmt;
            } else { // libre: uniforme
                wn = gEVpivot + off*(1.0f + gEVscaleAmt);
            }
        }
        Vector3 ln = EVMundoALocal(wn);
        // escribe la posicion EDITABLE (autoritativa); NO toca vertex[] a mano
        int k = s.editK;
        if (m->edit && k>=0 && k*3+2 < (int)m->edit->pos.size()){
            m->edit->pos[k*3]=ln.x; m->edit->pos[k*3+1]=ln.y; m->edit->pos[k*3+2]=ln.z;
        }
    }
    // CLIPPING (Mirror con clipping ON): impide que los verts CRUCEN el plano al moverlos (half-space). Pasa la
    // pos LOCAL inicial de cada vert (world0 -> local) para saber de que lado arranco. No-op si ningun Mirror clippea.
    if (m->edit && !m->modificadores.empty()){
        std::vector<int> editKs; editKs.reserve(gEVsnap.size());
        std::vector<Vector3> startLocal; startLocal.reserve(gEVsnap.size());
        for (size_t i=0;i<gEVsnap.size();i++){ editKs.push_back(gEVsnap[i].editK); startLocal.push_back(EVMundoALocal(gEVsnap[i].world0)); }
        m->ClipMirrorVerts(editKs, startLocal);
    }
    // edicion IN-PLACE solo de POSICIONES (rapido, tiempo real): pos[] -> render + overlay,
    // sin re-merge ni re-copiar atributos (que no cambian al mover). No rehace topologia.
    if (m->edit){ m->edit->EmpujarPosiciones(); m->edit->RefrescarOverlay(); }
    if (!m->modificadores.empty()) m->GenerarMallaModificada(); // preview de modificadores en tiempo real (barato si no hay / si edit-display OFF)
}

void EditXformReiniciar(){ // cambio de eje (X/Y/Z): restaura al snapshot
    if (!gEVmesh) return;
    gEVtrans = Vector3(0,0,0); gEVrotTotal = Quaternion(1,0,0,0); gEVscaleAmt = 0;
    EVEscribir();
}

// cursor actual (coords de pantalla) durante un transform -> lo usa el snap para buscar el target bajo el mouse.
int g_snapCurX = 0, g_snapCurY = 0;

// EJE BLOQUEADO + target BORDE: cuanto hay que avanzar desde B por el eje 'a' (unitario) para TOCAR el segmento
// [E0,E1] -> el punto de la recta (B + s*a) mas cercano al segmento (interseccion de bordes en la vista). Devuelve
// false si el segmento es degenerado o el eje es paralelo al borde (no hay toque util -> se usa el fallback viejo).
static bool SnapEjeTocaSegmento(const Vector3& B, const Vector3& a, const Vector3& E0, const Vector3& E1, float& outS){
    Vector3 d = E1 - E0;
    float E = d.Dot(d); if (E < 1e-12f) return false;         // segmento degenerado
    float Bd = a.Dot(d);
    float denom = E - Bd*Bd; if (denom < 1e-9f) return false; // eje || borde -> sin interseccion util
    Vector3 r = B - E0;
    float u = (d.Dot(r) - Bd*a.Dot(r)) / denom;               // parametro sobre la recta del borde
    if (u < 0.0f) u = 0.0f; if (u > 1.0f) u = 1.0f;           // clamp al SEGMENTO
    Vector3 Q = E0 + d*u;                                     // punto del borde mas cercano a la recta del eje
    outS = a.Dot(Q - B);                                      // proyeccion sobre el eje (a unitario) -> avance
    return true;
}

// ajusta gEVtrans para PEGAR la seleccion al target de snap (base Closest). Constreñido al mismo eje/plano que el
// move; libre = offset completo (proyeccion a la cara = retopologia). Setea g_snapHit + pos para el recuadro verde.
static void SnapAjustarEditTrans(){
    g_snapHit = false;
    if (!g_snap.enabled || !g_snap.afMove || !Viewport3DActive || InteractionMode!=EditMode || !gEVmesh) return;
    Vector3 T; float sx=0, sy=0;
    Vector3 eA, eB; // extremos del borde target (si target==SNAP_EDGE): para el snap con eje bloqueado
    if (!SnapBuscarTarget(g_snapCurX, g_snapCurY, Viewport3DActive, T, sx, sy, &eA, &eB)) return;
    // BASE: el punto de la seleccion (ya movida por gEVtrans) que se PEGA al target.
    //  Closest = el vert mas cercano al target;  Center = centro del bounding box;
    //  Median = promedio de los verts;  Active = el vertice activo (o Median si no hay).
    Vector3 B; bool any=false;
    if (gEVsnap.empty()) return;
    if (g_snap.base==SNAP_CLOSEST){
        float bd = 1e30f;
        for (size_t i=0;i<gEVsnap.size();i++){ Vector3 p=gEVsnap[i].world0+gEVtrans; Vector3 d=p-T; float dd=d.Dot(d); if (dd<bd){bd=dd;B=p;any=true;} }
    } else if (g_snap.base==SNAP_CENTER){
        Vector3 mn=gEVsnap[0].world0, mx=gEVsnap[0].world0;
        for (size_t i=1;i<gEVsnap.size();i++){ const Vector3& w=gEVsnap[i].world0;
            if (w.x<mn.x)mn.x=w.x; if (w.y<mn.y)mn.y=w.y; if (w.z<mn.z)mn.z=w.z;
            if (w.x>mx.x)mx.x=w.x; if (w.y>mx.y)mx.y=w.y; if (w.z>mx.z)mx.z=w.z; }
        B = (mn+mx)*0.5f + gEVtrans; any=true;
    } else if (g_snap.base==SNAP_ACTIVE){
        int act = (gEVmesh->edit) ? gEVmesh->edit->activeIdx : -1;
        for (size_t i=0;i<gEVsnap.size();i++) if (gEVsnap[i].editK==act){ B=gEVsnap[i].world0+gEVtrans; any=true; break; }
    }
    if (!any){ // MEDIAN (y fallback de Active/Center si algo faltara)
        Vector3 c(0,0,0); for (size_t i=0;i<gEVsnap.size();i++) c += gEVsnap[i].world0;
        B = c*(1.0f/(float)gEVsnap.size()) + gEVtrans;
    }
    Vector3 off = T - B;
    // EJE BLOQUEADO (X/Y/Z o la normal del extrude): si el target es un BORDE, en vez de solo igualar la coordenada
    // del eje, avanzamos por el eje hasta TOCAR el borde (la recta del eje ∩ el segmento) -> intersecar 2 bordes facil.
    bool ejeUnico = false; Vector3 ejeA;
    if (gEVuseCustom){ ejeUnico=true; ejeA=gTransformNormal; }                                   // extrude / orientacion Normal
    else if (axisSelect==X||axisSelect==Y||axisSelect==Z){ ejeUnico=true; ejeA=EjeOrientado(*gEVmesh,axisSelect); }
    if (ejeUnico){
        float s;
        if (g_snap.target==SNAP_EDGE && SnapEjeTocaSegmento(B, ejeA, eA, eB, s)) off = ejeA*s; // tocar el borde por el eje
        else off = ejeA*off.Dot(ejeA);                                                          // fallback: igualar coordenada
    }
    else if (axisSelect==PlaneX||axisSelect==PlaneY||axisSelect==PlaneZ){ int ex=(axisSelect==PlaneX)?X:(axisSelect==PlaneY)?Y:Z; Vector3 a=EjeOrientado(*gEVmesh,ex); off = off - a*off.Dot(a); }
    gEVtrans += off;
    // el recuadro verde en el TARGET (el punto bajo/cerca del cursor al que se pega), como devuelve SnapBuscarTarget
    g_snapHit = true; g_snapSx = sx; g_snapSy = sy;
}

// world0 (posicion original) del ELEMENTO ACTIVO (vert/borde/cara segun el modo): la "aguja" del snap de rotacion.
// activeIdx indexa vertSel / edgeSel / faceSel segun EditSelectMode. Los verts del activo estan seleccionados ->
// su world0 esta en gEVsnap. Borde = punto medio de sus 2 verts; cara = centro. false si no hay activo.
static bool SnapNeedleActivo(Vector3& out){
    if (!gEVmesh || !gEVmesh->edit) return false;
    EditMesh* e = gEVmesh->edit;
    int act = e->activeIdx; if (act < 0) return false;
    std::vector<int> ks; // editVerts que forman el elemento activo
    if (EditSelectMode==SelVertex){ ks.push_back(act); }
    else if (EditSelectMode==SelEdge){ if (act*2+1 < (int)e->lineIdx.size()){ ks.push_back(e->lineIdx[act*2]); ks.push_back(e->lineIdx[act*2+1]); } }
    else { if (act < (int)e->faces.size()) for (size_t c=0;c<e->faces[act].size();c++) ks.push_back(e->faces[act][c]); }
    if (ks.empty()) return false;
    Vector3 sum(0,0,0); int n=0;
    for (size_t j=0;j<ks.size();j++){ int k=ks[j];
        for (size_t i=0;i<gEVsnap.size();i++) if (gEVsnap[i].editK==k){ sum+=gEVsnap[i].world0; n++; break; } }
    if (n==0) return false;
    out = sum*(1.0f/(float)n); return true;
}

// SNAP de ROTACION (comportamiento propio de Whisk3D, distinto a Blender): gira la seleccion alrededor del pivote
// (gEVpivot = cursor 3d o centro de la geometria, NUNCA el activo) como una AGUJA de reloj hasta que el ELEMENTO
// ACTIVO (vert/borde/cara) apunte al target de snap. La aguja es SIEMPRE el activo (no depende del Snap Base).
// Ajuste MINIMO sobre lo que venia del drag. Solo con un eje definido (ViewAxis o X/Y/Z); el orbital no tiene plano.
static void SnapAjustarEditRot(){
    g_snapHit = false;
    if (!g_snap.enabled || !g_snap.afRot || !Viewport3DActive || InteractionMode!=EditMode || !gEVmesh) return;
    if (gEVsnap.empty() || axisSelect==OrbitalAxis) return;
    Vector3 axis = (axisSelect==ViewAxis||axisSelect==XYZ) ? camForward : EjeOrientado(*gEVmesh, axisSelect);
    { float al=sqrtf(axis.Dot(axis)); if (al<1e-6f) return; axis=axis*(1.0f/al); }
    Vector3 B0; if (!SnapNeedleActivo(B0)) return; // sin activo -> no hay aguja (hay que seleccionar una "punta")
    Vector3 T; float sx=0, sy=0;
    if (!SnapBuscarTarget(g_snapCurX, g_snapCurY, Viewport3DActive, T, sx, sy)) return;
    const Vector3 P = gEVpivot;
    // direcciones (en el plano perpendicular al eje): de la aguja YA rotada, y del target -> angulo firmado entre ellas
    Vector3 Bc = P + gEVrotTotal*(B0 - P);
    Vector3 vB = Bc - P; vB = vB - axis*vB.Dot(axis);
    Vector3 vT = T  - P; vT = vT - axis*vT.Dot(axis);
    float lB=sqrtf(vB.Dot(vB)), lT=sqrtf(vT.Dot(vT));
    if (lB<1e-5f || lT<1e-5f) return; // aguja o target sobre el eje -> sin angulo
    vB=vB*(1.0f/lB); vT=vT*(1.0f/lT);
    float cosA=vB.Dot(vT); if(cosA>1)cosA=1; if(cosA<-1)cosA=-1;
    Vector3 cross(vB.y*vT.z-vB.z*vT.y, vB.z*vT.x-vB.x*vT.z, vB.x*vT.y-vB.y*vT.x);
    float deltaDeg = atan2f(cross.Dot(axis), cosA) * 180.0f / 3.14159265f; // firmado alrededor del eje
    gEVrotTotal = Quaternion::FromAxisAngle(axis, deltaDeg) * gEVrotTotal; gEVrotTotal.normalize();
    gAnguloTransform += deltaDeg;
    // recuadro verde en el TARGET (el vertice bajo/cerca del cursor al que apunta la aguja), no en la punta activa
    g_snapHit = true; g_snapSx = sx; g_snapSy = sy;
}

void EditXformTraslacion(int dx,int dy,float speed){
    if (!gEVmesh) return;
    // EXTRUDE: constreñido a un eje arbitrario (la normal). Proyecta el mouse igual
    // que un eje normal, pero con gTransformNormal.
    if (gEVuseCustom){
        Vector3 a = gTransformNormal;
        float amt = (dx*a.Dot(camRight) - dy*a.Dot(camUp))*speed;
        gEVtrans += a*amt; SnapAjustarEditTrans(); EVEscribir(); return;
    }
    Vector3 libre = camRight*(dx*speed) + camUp*(-dy*speed); // plano de la camara
    Vector3 T;
    if (axisSelect==X||axisSelect==Y||axisSelect==Z){
        Vector3 a = EjeOrientado(*gEVmesh, axisSelect);
        float amt = (dx*a.Dot(camRight) - dy*a.Dot(camUp))*speed;
        T = a*amt;
    } else if (axisSelect==PlaneX||axisSelect==PlaneY||axisSelect==PlaneZ){
        int ex=(axisSelect==PlaneX)?X:(axisSelect==PlaneY)?Y:Z;
        Vector3 a=EjeOrientado(*gEVmesh, ex);
        T = libre - a*libre.Dot(a);
    } else T = libre;
    gEVtrans += T;
    SnapAjustarEditTrans(); // imantar la seleccion al target de snap (si esta ON)
    EVEscribir();
}
void EditXformRotEje(int dx,int dy){
    if (!gEVmesh) return;
    float ang=(dx+dy)*0.1f; gAnguloTransform+=ang;
    Vector3 axis = (axisSelect==ViewAxis||axisSelect==XYZ)?camForward:EjeOrientado(*gEVmesh,axisSelect);
    gEVrotTotal = Quaternion::FromAxisAngle(axis,ang)*gEVrotTotal; gEVrotTotal.normalize();
    SnapAjustarEditRot(); // imanta la aguja al target (si snap ON)
    EVEscribir();
}
void EditXformRotOrbital(int dx,int dy){
    if (!gEVmesh) return;
    float yaw=dx*0.1f, pitch=dy*0.1f; gAnguloTransform+=(dx+dy)*0.1f;
    Quaternion q = Quaternion::FromAxisAngle(camUp,yaw)*Quaternion::FromAxisAngle(camRight,pitch);
    gEVrotTotal = q*gEVrotTotal; gEVrotTotal.normalize();
    EVEscribir();
}
void EditXformRotAbs(const Quaternion& qAbs){ // trackball: rotacion absoluta desde el inicio
    if (!gEVmesh) return;
    gEVrotTotal = qAbs;
    SnapAjustarEditRot(); // imanta la aguja al target (si snap ON) -> gira desde el pivote hacia el target
    EVEscribir();
}
void EditXformScale(int dx,int dy,float factor){
    if (!gEVmesh) return;
    gEVscaleAmt += (dx+dy)*factor; EVEscribir();
}

// suma de los ejes ACTIVOS (en MUNDO, segun la orientacion) para el valor numerico
static Vector3 EVEjesActivos(){
    Object& o = *(Object*)gEVmesh;
    if (axisSelect==X||axisSelect==Y||axisSelect==Z) return EjeOrientado(o, axisSelect);
    if (axisSelect==PlaneX) return EjeOrientado(o,Y)+EjeOrientado(o,Z);
    if (axisSelect==PlaneY) return EjeOrientado(o,X)+EjeOrientado(o,Z);
    if (axisSelect==PlaneZ) return EjeOrientado(o,X)+EjeOrientado(o,Y);
    return EjeOrientado(o,X)+EjeOrientado(o,Y)+EjeOrientado(o,Z); // libre
}
// aplica un valor EXACTO (entrada numerica) al transform de malla en curso, segun
// 'estado' y el eje/orientacion. translate=distancia, rotacion=grados, escala=factor.
void EditXformNumValor(float v){
    if (!gEVmesh) return;
    if (estado==translacion){
        gEVtrans = (gEVuseCustom ? gTransformNormal : EVEjesActivos()) * v;
    } else if (estado==rotacion){
        Vector3 ax;
        if (gEVuseCustom) ax = gTransformNormal;
        else if (axisSelect==ViewAxis||axisSelect==XYZ||axisSelect==OrbitalAxis) ax = camForward;
        else ax = EjeOrientado(*gEVmesh, axisSelect);
        gEVrotTotal = Quaternion::FromAxisAngle(ax, v); gAnguloTransform = v;
    } else if (gEVshrink){ // Shrink/Fatten: el valor es la DISTANCIA por la normal
        gEVscaleAmt = v;
    } else { // EditScale
        gEVscaleAmt = v - 1.0f;
    }
    EVEscribir();
}

// SHRINK/FATTEN (Alt+S, menu Mesh > Transform): cada vertice seleccionado se mueve por SU normal (engorda/
// adelgaza). Reusa la maquina de EditScale (toolbar/confirmar/cancelar/tactil) con el flag gEVshrink -> solo
// cambia el calculo (mover por normal en vez de escalar desde el pivote). Confirma/cancela como cualquier transform.
void LayoutShrinkFatten() {
    if (InteractionMode != EditMode || !g_editMesh) return;
    if (EditXformActivo()) EditXformConfirmar(); // encadenar: confirma el transform anterior
    estado = EditScale; axisSelect = XYZ;        // libre (la direccion la da la normal de cada vertice)
    UndoEditMoveIniciar((Mesh*)g_editMesh);      // Ctrl+Z: captura posiciones previas
    EditXformIniciar();                          // snapshot de la seleccion (calcula las normales por vert)
    if (!EditXformActivo()){ estado = editNavegacion; return; }
    gEVshrink = true;                            // <- ahora si: mover por la normal
    ToolbarRegistrarAccion(TBScale);             // historial (reusa el de escala)
}

// fija el resultado: recalcula bordes/centro/posRep (sin invalidar el edit) y las
// NORMALES (salvo Lock Normals). El overlay ya esta sincronizado (SincronizarPos).
void EditXformConfirmar(){
    UndoEditMoveConfirmar(); // Ctrl+Z: el move PURO se acepto -> pushea el pendiente (NULL si fue extrude)
    if (gEVmesh){
        Mesh* m = gEVmesh;
        // CONSERVA posRep (reagruparPosRep=false): un move NO cambia la topologia, y si el snap dejo 2 verts
        // INDEPENDIENTES encimados, rederivar posRep por posicion los SOLDARIA (caras y edit desincronizados: bug Dante).
        m->CalcularBordes(false, false);      // posRep(conserva)/centroGeom/bordes; conserva el edit
        if (!g_editLockNormales) m->RecalcularNormales();
        // AUTO MERGE (opt-in, OFF por defecto): recien AHORA, si el usuario lo pidio, se sueldan los verts movidos
        // con cualquier vert a <= threshold. No-op si no hay nada cerca (MergeVertsEdit retorna sin tocar nada).
        if (g_autoMerge) MergeVertsEdit(m, 3 /*By Distance*/, g_autoMergeThreshold, Vector3(0,0,0));
        if (!m->modificadores.empty()) m->GenerarMallaModificada(); // regen final (toma las normales recalculadas)
    }
    gEVsnap.clear(); gEVmesh = NULL;
    estado = editNavegacion;
    g_extrudeEnCurso = false; // termino el transform
}

// descarta: restaura las posiciones del snapshot (mundo -> local) y limpia.
void EditXformCancelar(){
    UndoEditMoveCancelar(); // Ctrl+Z: move cancelado -> descarta el pendiente (no deja undo no-op)
    if (gEVmesh){
        Mesh* m = gEVmesh;
        for (size_t i=0;i<gEVsnap.size();i++){
            EditVtxSnap& s = gEVsnap[i];
            Vector3 ln = EVMundoALocal(s.world0);
            int k = s.editK; // restaura la posicion EDITABLE (no toca vertex[] a mano)
            if (m->edit && k>=0 && k*3+2 < (int)m->edit->pos.size()){
                m->edit->pos[k*3]=ln.x; m->edit->pos[k*3+1]=ln.y; m->edit->pos[k*3+2]=ln.z;
            }
        }
        if (m->edit){ m->edit->EmpujarPosiciones(); m->edit->RefrescarOverlay(); }
        if (!m->modificadores.empty()) m->GenerarMallaModificada(); // preview vuelve al estado previo al cancelar
    }
    gEVsnap.clear(); gEVmesh = NULL;
    estado = editNavegacion;
    g_extrudeEnCurso = false; // termino el transform
}

// ====================================================================
//  ENTRADA NUMERICA / FORMULAS durante un transform (COMPARTIDA 4 OS)
// ====================================================================
// Mientras se mueve/rota/escala, el usuario puede TIPEAR un valor exacto en vez de
// hacerlo a ojo con el mouse: numeros, punto decimal, parentesis y * / + (formulas
// tipo "(3+10)/2"). El '-' alterna el signo (no es resta). Es PLATFORM-AGNOSTIC: el
// nucleo (buffer + evaluador + apply) vive aca; cada SO alimenta los caracteres por
// NumInputChar (PC desde SDL_TEXTINPUT; Symbian desde su teclado/keypad).
static std::string gNumBuf;       // la expresion tipeada (sin el signo)
static bool        gNumActivo = false;
static bool        gNumNegar = false; // el toggle del '-'
static int         gNumCaret = 0;     // posicion del caret en gNumBuf (para editar en el medio; teclado tactil ← →)

// --- evaluador de expresiones (+ * / parentesis, numeros con punto). Parse de
//     numero MANUAL (sin strtod) para no depender del locale (',' vs '.'). ---
struct ExprP { const char* s; bool ok; };
static float ExprExpr(ExprP& p);
static void ExprSkip(ExprP& p){ while(*p.s==' ') p.s++; }
static float ExprNum(ExprP& p){
    ExprSkip(p);
    if (*p.s=='('){ p.s++; float v=ExprExpr(p); ExprSkip(p); if(*p.s==')') p.s++; else p.ok=false; return v; }
    if (*p.s=='+'){ p.s++; return ExprNum(p); }
    if (*p.s=='-'){ p.s++; return -ExprNum(p); }
    bool any=false; float ip=0;
    while(*p.s>='0'&&*p.s<='9'){ ip=ip*10.0f+(float)(*p.s-'0'); p.s++; any=true; }
    if (*p.s=='.'){ p.s++; float sc=1.0f; while(*p.s>='0'&&*p.s<='9'){ sc*=0.1f; ip+=(float)(*p.s-'0')*sc; p.s++; any=true; } }
    if (!any){ p.ok=false; return 0.0f; }
    return ip;
}
static float ExprTerm(ExprP& p){
    float v=ExprNum(p);
    for(;;){ ExprSkip(p); char c=*p.s;
        if(c=='*'){ p.s++; v*=ExprNum(p); }
        else if(c=='/'){ p.s++; float d=ExprNum(p); v = (d!=0.0f)? v/d : 0.0f; }
        else break; }
    return v;
}
static float ExprExpr(ExprP& p){
    float v=ExprTerm(p);
    for(;;){ ExprSkip(p); char c=*p.s;
        if(c=='+'){ p.s++; v+=ExprTerm(p); }
        else if(c=='-'){ p.s++; v-=ExprTerm(p); }
        else break; }
    return v;
}
static bool EvalExpr(const std::string& str, float& out){
    if (str.empty()){ out=0.0f; return true; }
    ExprP p; p.s=str.c_str(); p.ok=true;
    float v=ExprExpr(p); ExprSkip(p);
    if (!p.ok || *p.s!='\0') return false; // expresion incompleta/invalida
    out=v; return true;
}

// CAJA DE TEXTO EDITABLE: el campo enfocado + el ruteo de caracteres (compartido).
TextField* g_textFieldActivo = NULL;
// edicion numerica por texto de un PropFloat (WhiskUI/PropFloat): Enter aplica, Cancel descarta.
extern bool NumEditActivo();
extern void NumEditCommit();
extern void NumEditCancel();
bool TextFieldInputChar(int c){
    if (!g_textFieldActivo) return false;
    if (c == 8)       g_textFieldActivo->Backspace();
    else if (c == 127)g_textFieldActivo->DelForward();
    else if (c >= 32 && c < 127) g_textFieldActivo->InsertChar(c); // ASCII imprimible
    else return false;
    g_redraw = true;
    return true;
}

bool NumInputActivo(){ return gNumActivo; }
void NumInputReset(){ gNumBuf.clear(); gNumActivo=false; gNumNegar=false; gNumCaret=0; }
const std::string& NumInputBuffer(){ return gNumBuf; }
int  NumInputCaret(){ return gNumCaret; }                 // posicion del caret (para dibujarlo en la barra)
void NumInputLeft(){  if (gNumCaret>0) { gNumCaret--; g_redraw=true; } }
void NumInputRight(){ if (gNumCaret<(int)gNumBuf.size()) { gNumCaret++; g_redraw=true; } }
bool NumInputNegado(){ return gNumNegar; }
// valor actual (false si la expresion esta incompleta -> no aplicar todavia)
bool NumInputValor(float& out){
    float v; if (!EvalExpr(gNumBuf, v)) return false;
    out = gNumNegar ? -v : v; return true;
}

// aplica el valor exacto al transform en curso (malla o objeto)
static void NumInputAplicar(){
    if (!gNumActivo) return;
    float v; if (!NumInputValor(v)) return; // incompleta: espero mas caracteres
    if (InteractionMode == EditMode && EditXformActivo()) EditXformNumValor(v);
    else                                                  SetTransformNumerico(v);
}

// COMPARTIDA: cada plataforma alimenta los caracteres tipeados aca. Devuelve true si
// lo consumio (hay un transform activo y el caracter es relevante). c==8 = backspace.
bool NumInputChar(int c){
    if (estado == editNavegacion) return false; // solo durante un transform
    if (c == 8){ // backspace: borra el char ANTES del caret (o el signo si esta vacio)
        if (gNumCaret > 0 && !gNumBuf.empty()) { gNumBuf.erase(gNumCaret-1, 1); gNumCaret--; }
        else if (gNumBuf.empty()) gNumNegar = false;
    } else if (c == '-'){
        gNumNegar = !gNumNegar; gNumActivo = true;
    } else if ((c>='0'&&c<='9') || c=='.' || c=='(' || c==')' || c=='*' || c=='/' || c=='+'){
        if (gNumCaret < 0) gNumCaret = 0; if (gNumCaret > (int)gNumBuf.size()) gNumCaret = (int)gNumBuf.size();
        gNumBuf.insert(gNumBuf.begin()+gNumCaret, (char)c); gNumCaret++; gNumActivo = true; // inserta EN el caret
    } else {
        return false; // no es un caracter numerico
    }
    if (gNumBuf.empty() && !gNumNegar) gNumActivo = false; // se vacio: vuelve el mouse
    NumInputAplicar();
    g_redraw = true;
    return true;
}

// El teclado tactil (NumPad en modo transform) llama a estas para editar el transform
// que se muestra ARRIBA (sin popup/textinput propio), igual que un teclado fisico en PC.
void NumInputBegin(){ // marca la entrada activa asi la barra muestra "[|]" apenas se abre el teclado
    if (!gNumActivo){ gNumBuf.clear(); gNumCaret=0; gNumNegar=false; gNumActivo=true; }
    g_redraw = true;
}
void NumInputConfirmar(){ // OK del teclado: confirma el transform (Aceptar ya hace NumInputReset)
    if (Viewport3DActive) Viewport3DActive->Aceptar();
    g_redraw = true;
}
void NumInputCancelar(){ // X del teclado: descarta el transform (mismo camino que la cruz de la barra)
    if (InteractionMode == EditMode && EditXformActivo()) EditXformCancelar();
    else Cancelar(); // objeto: descarta el transform (free function de ObjectMode)
    NumInputReset();
    g_redraw = true;
}
// hay un transform en curso? (mismo criterio que la barra de estado que lo muestra). El teclado
// tactil en modo transform se cierra solo si esto deja de ser cierto (se confirmo/cancelo).
bool NumInputTransformEnCurso(){
    if (!Viewport3DActive) return false;
    if (!(estado == translacion || estado == rotacion || estado == EditScale)) return false;
    if (InteractionMode == ObjectMode) return true;
    if (InteractionMode == EditMode && EditXformActivo()) return true;
    return false;
}

// ====================================================================
// menu de SNAP (shift+s, estilo Blender): mueve seleccion / cursor 3D
// ====================================================================
static PopupMenu* gMenuSnap = NULL;

static void AccionSnap(int aId) {
    switch (aId) {
        case 0: SnapSeleccionAlGrid(); break;
        case 1: SnapSeleccionAlCursor(false); break;
        case 2: SnapSeleccionAlCursor(true); break;   // keep offset
        case 3: SnapSeleccionAlActivo(); break;
        case 4: SnapCursorALoSeleccionado(); break;
        case 5: SnapCursorAlOrigen(); break;
        case 6: SnapCursorAlGrid(); break;
        case 7: SnapCursorAlActivo(); break;
    }
}

// El menu Snap se usa standalone (Shift+S) Y como submenu del menu "Mesh" de Edit Mode. Ids 0-7 + accion
// AccionSnap: como submenu del Mesh, LayoutAccionMesh rutea esos mismos ids a AccionSnap (no chocan con el
// resto del Mesh, que usa 100-102 y 361-364).
PopupMenu* LayoutSubmenuSnap() {
    if (!gMenuSnap) {
        gMenuSnap = new PopupMenu();
        gMenuSnap->titulo = "Snap";
        gMenuSnap->action = AccionSnap;
        gMenuSnap->Agregar("Selection to Grid", 0);
        gMenuSnap->Agregar("Selection to Cursor", 1);
        gMenuSnap->Agregar("Selection to Cursor (Keep Offset)", 2);
        gMenuSnap->Agregar("Selection to Active", 3);
        gMenuSnap->Agregar("Cursor to Selected", 4);
        gMenuSnap->Agregar("Cursor to World Origin", 5);
        gMenuSnap->Agregar("Cursor to Grid", 6);
        gMenuSnap->Agregar("Cursor to Active", 7);
    }
    return gMenuSnap;
}

// submenu Delete (vertices/aristas/caras/loops) para embeber en el menu Mesh. Es el MISMO gMenuDelete del
// atajo X (ids 361-364 + accion LayoutAccionObject) -> anda igual embebido o standalone.
PopupMenu* LayoutSubmenuDelete() {
    EnsureMenuDelete();
    return gMenuDelete;
}

void LayoutMenuSnap(int mx, int my) {
    PopupMenu* m = LayoutSubmenuSnap();
    m->Abrir(mx, my, MenuPantallaW, MenuPantallaH);
    MenuAbierto = m;
}

// ===== menu MERGE (tecla M / submenu del menu "Mesh"): suelda los verts seleccionados =====
static PopupMenu* gMenuMerge = NULL;

// modo: 0 At Center, 1 At Cursor, 2 Collapse, 3 By Distance. Corre sobre la malla en Edit Mode.
static void AccionMerge(int modo) {
    if (InteractionMode != EditMode || !g_editMesh) return;
    Mesh* m = (Mesh*)g_editMesh;
    // cursor 3d (MUNDO) -> LOCAL del mesh (para At Cursor): misma matematica que Snap Selection to Cursor.
    Quaternion rg = RotGlobalDe(m); Vector3 sg = ScaleGlobalDe(m);
    Vector3 dloc = rg.Inverted() * (cursor3D.pos - m->GetGlobalPosition());
    Vector3 cursorLocal(sg.x!=0.0f?dloc.x/sg.x:dloc.x, sg.y!=0.0f?dloc.y/sg.y:dloc.y, sg.z!=0.0f?dloc.z/sg.z:dloc.z);
    MergeVertsEdit(m, modo, g_mergeDist, cursorLocal);
    g_redraw = true;
}

// submenu Merge (ids 380-383 + accion LayoutAccionObject): anda igual embebido en el menu "Mesh" o standalone (tecla M)
PopupMenu* LayoutSubmenuMerge() {
    if (!gMenuMerge) {
        gMenuMerge = new PopupMenu();
        gMenuMerge->titulo = "Merge";
        gMenuMerge->action = LayoutAccionObject;
        gMenuMerge->Agregar("At Center", 380);
        gMenuMerge->Agregar("At Cursor", 381);
        gMenuMerge->Agregar("Collapse", 382);
        gMenuMerge->Agregar("By Distance", 383);
    }
    return gMenuMerge;
}

// submenu Normals (Recalculate + Flip) para embeber en el menu "Mesh". Ids 322/324 + accion LayoutAccionObject.
static PopupMenu* gMenuNormals = NULL;
PopupMenu* LayoutSubmenuNormals() {
    if (!gMenuNormals) {
        gMenuNormals = new PopupMenu();
        gMenuNormals->titulo = "Normals";
        gMenuNormals->action = LayoutAccionObject;
        gMenuNormals->Agregar("Recalculate Normals", 322)->atajo = "Shift N"; // orienta hacia afuera (cubo/esfera OK)
        gMenuNormals->Agregar("Flip", 324);                                    // simplemente invierte las normales
    }
    return gMenuNormals;
}

// tecla M en Edit Mode: abre el menu Merge en el cursor
void LayoutMenuMerge(int mx, int my) {
    if (InteractionMode != EditMode) return;
    PopupMenu* m = LayoutSubmenuMerge();
    if (MenuAbierto) MenuAbierto->Cerrar();
    m->Abrir(mx, my, MenuPantallaW, MenuPantallaH);
    MenuAbierto = m;
}

// accion del menu "Mesh" (Edit Mode): los ids del submenu Snap (0-7) van a AccionSnap; el resto (Transform
// 100-102, Delete 361-364) lo maneja el dispatcher comun de objeto/malla.
static void LayoutAccionMesh(int aId) {
    if (aId >= 0 && aId <= 7) { AccionSnap(aId); return; }
    LayoutAccionObject(aId);
}

// ====================================================================
// arrastre de la BARRA de scroll: click la agarra; en PC se suelta al
// soltar el boton (LayoutSoltar) y en Symbian con otro click (el driver
// HID no manda movimiento con el boton mantenido)
// ====================================================================

static ViewportBase* gScrollBarDrag = NULL;
static bool gScrollBarVertical = true;
static bool gScrollMovio = false; // hubo movimiento desde el agarre
static int gScrollPrevX = 0;
static int gScrollPrevY = 0;

static Scrollable* ComoScrollable(ViewportBase* v) {
    if (!v) return NULL;
    if (v->ViewportKind() == 2) return (Outliner*)v;
    if (v->ViewportKind() == 3) return (Properties*)v;
    return NULL;
}

bool LayoutEnArrastre() {
    return gScrollBarDrag != NULL;
}

// el popup activo (color picker) esta arrastrando un valor?
bool LayoutPopupArrastrando() {
    return PopUpActive && PopUpActive->Arrastrando();
}

static void SoltarDragOutliners(ViewportBase* aNodo, int mx, int my) {
    if (!aNodo) return;
    if (aNodo->isLeaf()) {
        if (aNodo->ViewportKind() == 2) ((Outliner*)aNodo)->SoltarDrag(mx, my);
        return;
    }
    if (aNodo->ContainerKind() == 1) {
        SoltarDragOutliners(((ViewportRow*)aNodo)->childA, mx, my);
        SoltarDragOutliners(((ViewportRow*)aNodo)->childB, mx, my);
    } else if (aNodo->ContainerKind() == 2) {
        SoltarDragOutliners(((ViewportColumn*)aNodo)->childA, mx, my);
        SoltarDragOutliners(((ViewportColumn*)aNodo)->childB, mx, my);
    }
}

void LayoutSoltar(int mx, int my) {
    // el up del MISMO tap que agarro (Symbian: down+up en ~8ms) no
    // suelta: solo si hubo movimiento (el hold-drag de PC)
    if (gScrollBarDrag && gScrollMovio) {
        Scrollable* sc = ComoScrollable(gScrollBarDrag);
        if (sc) {
            sc->mouseOverScrollYpress = false;
            sc->mouseOverScrollXpress = false;
        }
        gScrollBarDrag = NULL;
    }
    if (PopUpActive) PopUpActive->Soltar(); // el picker (hold-drag PC)
    // drop del arrastre del outliner (reordenar / emparentar)
    if (rootViewport) SoltarDragOutliners(rootViewport, mx, my);
}

// ====================================================================
// hover / click / teclado
// ====================================================================

// hover de barras + limpiar el hover de los viewports sin mouse encima
static void LayoutHoverArbol(ViewportBase* aNodo, ViewportBase* aUnder,
                             int aMx, int aMy) {
    if (!aNodo) return;
    if (aNodo->isLeaf()) {
        aNodo->BarHover(aMx, aMy);
        if (aNodo != aUnder) aNodo->ClearHover();
        return;
    }
    if (aNodo->ContainerKind() == 1) {
        LayoutHoverArbol(((ViewportRow*)aNodo)->childA, aUnder, aMx, aMy);
        LayoutHoverArbol(((ViewportRow*)aNodo)->childB, aUnder, aMx, aMy);
    } else if (aNodo->ContainerKind() == 2) {
        LayoutHoverArbol(((ViewportColumn*)aNodo)->childA, aUnder, aMx, aMy);
        LayoutHoverArbol(((ViewportColumn*)aNodo)->childB, aUnder, aMx, aMy);
    }
}

bool LayoutMotionUI(int mx, int my) {
    if (!rootViewport) return false;
    if (PopUpActive) {
        // popup modal (selector de color): se queda con el mouse
        PopUpActive->Motion(mx, my);
        return true;
    }
    if (gScrollBarDrag) {
        // la barra agarrada sigue al mouse (factor del recorrido real)
        gScrollMovio = true;
        Scrollable* sc = ComoScrollable(gScrollBarDrag);
        if (sc) {
            if (gScrollBarVertical) {
                sc->PosY -= (int)((my - gScrollPrevY) * sc->scrollDragFactor);
                if (sc->PosY > 0) sc->PosY = 0;
                if (sc->PosY < sc->MaxPosY) sc->PosY = sc->MaxPosY;
            } else {
                sc->PosX -= (int)((mx - gScrollPrevX) * sc->scrollDragFactorX);
                if (sc->PosX > 0) sc->PosX = 0;
                if (sc->PosX < sc->MaxPosX) sc->PosX = sc->MaxPosX;
            }
        }
        gScrollPrevX = mx;
        gScrollPrevY = my;
        return true;
    }
    if (LayoutMenuAbierto()) {
        // si el mouse pasa por OTRO boton de menu de la barra, cambiar de menu
        // (cierra el actual y abre el del boton) sin necesidad de click
        ViewportBase* bajo = FindViewportUnderMouse(rootViewport, mx, my);
        LayoutAbrirMenuDeBarra(bajo, mx, my);
        // resaltar el boton bajo el mouse y APAGAR el del que se abrio el menu
        // (sino quedaba con borde blanco el primero clickeado)
        LayoutHoverArbol(rootViewport, bajo, mx, my);
        // hover de las opciones + auto-cierre si el mouse se aleja
        if (MenuAbierto) MenuAbierto->MouseMove(mx, my);
        return true; // el menu se queda con el movimiento
    }
    ViewportBase* under = FindViewportUnderMouse(rootViewport, mx, my);
    ViewportBase* hoja = (under && under->isLeaf()) ? under : NULL;
    // con varios viewports 3D el ACTIVO es el de abajo del mouse
    if (hoja && hoja->ViewportKind() == 1) {
        Viewport3DActive = (Viewport3D*)hoja;
    }
    // hover de la barra de scroll: la MISMA zona que usa el agarre del
    // click (un "scrollbar area" reservada al borde del panel)
    if (hoja) {
        Scrollable* sc = ComoScrollable(hoja);
        if (sc) {
            int areaScroll = borderGS + GlobalScale * 9;
            bool zonaV = sc->scrollY &&
                mx >= hoja->x + hoja->width - areaScroll &&
                my >= hoja->y + hoja->BarTopOffset();
            bool zonaH = !zonaV && sc->scrollX &&
                my >= hoja->y + hoja->height - areaScroll &&
                mx < hoja->x + hoja->width - (sc->scrollY ? areaScroll : 0);
            sc->mouseOverScrollY = zonaV;
            sc->mouseOverScrollX = zonaH;
        }
    }
    LayoutHoverArbol(rootViewport, hoja, mx, my);
    return false;
}

bool LayoutClickUI(int mx, int my) {
    if (!rootViewport) return false;
    if (PopUpActive) {
        if (PopUpActive->Click(mx, my)) return true; // adentro: el popup lo maneja
        // afuera: cerrarlo. Si es semi-modal (redo-panel) el click ademas cae al
        // viewport (selecciona/orbita); si es modal, se consume y listo.
        bool semimodal = PopUpActive->CierraConViewport();
        PopUpActive->Cerrar();
        if (!semimodal) return true;
    }
    if (LayoutMenuAbierto()) {
        // el menu se queda con el click (adentro o el que lo cierra)
        PopupMenu* m = MenuAbierto;
        int id = m->Click(mx, my);
        if (id >= 0 && m->action) m->action(id);
        return true;
    }
    // un click con la barra de scroll agarrada la SUELTA (Symbian)
    if (gScrollBarDrag) {
        Scrollable* sc = ComoScrollable(gScrollBarDrag);
        if (sc) {
            sc->mouseOverScrollYpress = false;
            sc->mouseOverScrollXpress = false;
        }
        gScrollBarDrag = NULL;
        return true;
    }

    ViewportBase* under = FindViewportUnderMouse(rootViewport, mx, my);
    if (!under || !under->isLeaf()) return false;

    // 0) la barra de HERRAMIENTAS del viewport 3D (abajo): historial + orientacion + ejes + tilde/cruz
    if (under->ViewportKind() == 1 && ((Viewport3D*)under)->ToolbarClick(mx, my)) return true;

    // 1) la barra de botones del viewport
    if (under->BarClick(mx, my)) {
        if (LayoutClickBotonTipo(under, mx, my)) return true;
        if (under->ViewportKind() == 3) {
            ((Properties*)under)->ClickTab(mx, my); // pestanias Objeto/Mesh
        } else if (under->ViewportKind() == 4) {
            LayoutClickBarraUV((UVEditor*)under, mx, my); // boton "View" -> checkboxes
        } else {
            LayoutAbrirMenuDeBarra(under, mx, my); // Select/Add/Object/Overlays
        }
        return true;
    }

    // 2) la BARRA DE SCROLL: el click la agarra (antes caia en las
    // filas del panel y era imposible arrastrarla)
    {
        Scrollable* sc = ComoScrollable(under);
        if (sc) {
            int areaScroll = borderGS + GlobalScale * 9;
            bool zonaV = sc->scrollY &&
                mx >= under->x + under->width - areaScroll &&
                my >= under->y + under->BarTopOffset();
            bool zonaH = !zonaV && sc->scrollX &&
                my >= under->y + under->height - areaScroll &&
                mx < under->x + under->width - (sc->scrollY ? areaScroll : 0);
            if (zonaV || zonaH) {
                gScrollBarDrag = under;
                gScrollBarVertical = zonaV;
                gScrollMovio = false; // el up del MISMO tap no la suelta
                gScrollPrevX = mx;
                gScrollPrevY = my;
                if (zonaV) sc->mouseOverScrollYpress = true;
                else sc->mouseOverScrollXpress = true;
                return true;
            }
        }
    }

    // 3) los paneles
    if (under->ViewportKind() == 3) {
        ((Properties*)under)->ClickEn(mx, my);
        return true;
    }
    if (under->ViewportKind() == 2) {
        ((Outliner*)under)->ClickSeleccionar(mx, my);
        return true;
    }
    return false; // 3D: la seleccion/transform la maneja la plataforma
}

// algun panel de propiedades esta editando? (tiene el foco del teclado)
static Properties* LayoutPropsEditando(ViewportBase* aNodo) {
    if (!aNodo) return NULL;
    if (aNodo->isLeaf()) {
        if (aNodo->ViewportKind() == 3 && ((Properties*)aNodo)->editando) {
            return (Properties*)aNodo;
        }
        return NULL;
    }
    ViewportBase* a = NULL;
    ViewportBase* b = NULL;
    if (aNodo->ContainerKind() == 1) {
        a = ((ViewportRow*)aNodo)->childA;
        b = ((ViewportRow*)aNodo)->childB;
    } else {
        a = ((ViewportColumn*)aNodo)->childA;
        b = ((ViewportColumn*)aNodo)->childB;
    }
    Properties* r = LayoutPropsEditando(a);
    if (r) return r;
    return LayoutPropsEditando(b);
}

// el submenu ABIERTO mas profundo (el que tiene el foco del teclado)
static PopupMenu* LayoutMenuProfundo() {
    PopupMenu* deep = MenuAbierto;
    while (deep && deep->submenuAbierto && deep->submenuAbierto->abierto)
        deep = deep->submenuAbierto;
    return deep;
}

// cierra SOLO el submenu mas profundo (vuelve al padre). true si habia uno.
static bool LayoutCerrarSubmenuProfundo() {
    if (!MenuAbierto || !MenuAbierto->abierto) return false;
    PopupMenu* parent = NULL; PopupMenu* deep = MenuAbierto;
    while (deep->submenuAbierto && deep->submenuAbierto->abierto) {
        parent = deep; deep = deep->submenuAbierto;
    }
    if (!parent) return false; // no hay submenu abierto, solo el menu raiz
    deep->Cerrar();
    parent->submenuAbierto = NULL;
    return true;
}

// flecha MANTENIDA (frame-based del keypad N95): rutea al popup activo SOLO para ajustar valores
// (el popup decide; el color picker ajusta R/G/B/A o circulo/value, los demas no hacen nada). false si no hay popup.
bool LayoutPopupRepeat(int tecla) {
    if (PopUpActive) return PopUpActive->TeclaRepeat(tecla);
    return false;
}

bool LayoutTeclaUI(int tecla, int mx, int my) {
    if (!rootViewport) return false;

    if (PopUpActive) {
        return PopUpActive->Tecla(tecla); // popup modal
    }

    // un menu desplegable abierto se queda con el teclado. El foco va al submenu
    // ABIERTO mas profundo; izq/der dependen de la opcion resaltada (slider ->
    // mueve el valor; submenu -> abre/cierra; comun -> cambia de menu de barra).
    if (LayoutMenuAbierto()) {
        PopupMenu* deep = LayoutMenuProfundo();
        MenuItem* it = deep ? deep->ItemActual() : NULL;
        switch (tecla) {
            case LayoutKey::Up:   if (deep) deep->button_up();   return true;
            case LayoutKey::Down: if (deep) deep->button_down(); return true;
            case LayoutKey::Right:
                if (it && it->valorFloat) { deep->AjustarSlider(+1); return true; }
                if (it && it->submenu)    { deep->AbrirSubmenuActual(); return true; }
                if (viewPortActive && viewPortActive->ViewportKind() == 4) LayoutCambiarMenuBarraUV(+1); // editor UV: sus menus
                else LayoutCambiarMenuBarra(+1);                                                         // 3D: opcion comun, menu de al lado
                return true;
            case LayoutKey::Left:
                if (it && it->valorFloat) { deep->AjustarSlider(-1); return true; }
                if (deep != MenuAbierto)  { LayoutCerrarSubmenuProfundo(); return true; }
                if (viewPortActive && viewPortActive->ViewportKind() == 4) LayoutCambiarMenuBarraUV(-1);
                else LayoutCambiarMenuBarra(-1);
                return true;
            case LayoutKey::Enter: {
                // Enter() recorre hasta el submenu mas profundo: abre el submenu
                // de la fila o selecciona/togglea y cierra la cadena si es terminal
                PopupMenu* m = MenuAbierto;
                int id = m->Enter();
                if (id >= 0 && m->action) m->action(id);
                return true;
            }
            case LayoutKey::Cancel:
                MenuAbierto->Cerrar();
                return true;
        }
        return true; // mientras este abierto no le roban teclas
    }

    // EDICION NUMERICA por texto en curso (un PropFloat): el teclado va al campo. Enter APLICA, Cancel DESCARTA,
    // izq/der mueven el caret. Los digitos 0-9 / '*'(punto) los inyecta el contenedor Symbian o SDL_TEXTINPUT (PC).
    if (NumEditActivo()) {
        switch (tecla) {
            // aceptar/cancelar tiene que SALIR del editando del panel, sino te clava en la propiedad (button_up/down
            // seguirian ajustando el valor en vez de navegar).
            case LayoutKey::Enter:  NumEditCommit(); if (PropsActivo) PropsActivo->editando = false; return true;
            case LayoutKey::Cancel: NumEditCancel(); if (PropsActivo) PropsActivo->editando = false; return true;
            case LayoutKey::Left:   if (g_textFieldActivo) g_textFieldActivo->CaretIzq(); g_redraw = true; return true;
            case LayoutKey::Right:  if (g_textFieldActivo) g_textFieldActivo->CaretDer(); g_redraw = true; return true;
        }
        return true; // mientras edita, consume el resto (no navega ni ajusta la escena)
    }

    // editando una propiedad: ese panel tiene el foco SIN importar hover
    Properties* pe = LayoutPropsEditando(rootViewport);
    if (pe) {
        switch (tecla) {
            case LayoutKey::Enter:  pe->EnterPropertieSelect(); return true;
            case LayoutKey::Cancel: pe->Cancel();               return true;
            case LayoutKey::Up:     pe->button_up();            return true;
            case LayoutKey::Down:   pe->button_down();          return true;
            case LayoutKey::Left:   pe->button_left();          return true;
            case LayoutKey::Right:  pe->button_right();         return true;
        }
        return false;
    }

    // sin edicion: el panel bajo el mouse recibe el teclado (como PC)
    ViewportBase* under = FindViewportUnderMouse(rootViewport, mx, my);
    if (!under || !under->isLeaf()) return false;

    if (under->ViewportKind() == 3) {
        Properties* p = (Properties*)under;
        switch (tecla) {
            case LayoutKey::Up:    p->button_up();    return true;
            case LayoutKey::Down:  p->button_down();  return true;
            case LayoutKey::Left:  p->button_left();  return true;
            case LayoutKey::Right: p->button_right(); return true;
            case LayoutKey::Enter: p->key_down_return(); return true;
        }
        return false;
    }
    if (under->ViewportKind() == 2) {
        switch (tecla) {
            case LayoutKey::Up:
                changeSelect(SelectMode::PrevSingle, true);
                return true;
            case LayoutKey::Down:
                changeSelect(SelectMode::NextSingle, true);
                return true;
            case LayoutKey::Left:
                SetDesplegado(false);
                return true;
            case LayoutKey::Right:
                SetDesplegado(true);
                return true;
        }
        return false;
    }
    // sobre el 3D: las flechas quedan para el futuro (ejes de transform)
    return false;
}

// rutea una flecha/OK al viewport ACTIVO (borde verde), SIN mouse: propiedades
// (kind 3) y outliner (kind 2). El 3D (kind 1) devuelve false: lo maneja la
// orbita/transform. Es lo que usa Symbian con el keypad cuando no hay mouse BT.
bool LayoutTeclaPanelActivo(int tecla) {
    // edicion numerica por texto en curso: Enter aplica, Cancel descarta, izq/der = caret (los digitos los mete
    // el contenedor). Va ANTES de todo asi el keypad no navega la escena mientras se tipea un valor.
    if (NumEditActivo()) {
        switch (tecla) {
            // al aceptar/cancelar hay que SALIR de la edicion del panel (editando=false): sino button_up/down siguen
            // ajustando el valor en vez de navegar -> te quedabas CLAVADO en la propiedad. (Bug reportado en Symbian.)
            case LayoutKey::Enter:  NumEditCommit(); if (PropsActivo) PropsActivo->editando = false; return true;
            case LayoutKey::Cancel: NumEditCancel(); if (PropsActivo) PropsActivo->editando = false; return true;
            case LayoutKey::Left:   if (g_textFieldActivo) g_textFieldActivo->CaretIzq(); g_redraw = true; return true;
            case LayoutKey::Right:  if (g_textFieldActivo) g_textFieldActivo->CaretDer(); g_redraw = true; return true;
        }
        return true;
    }
    if (!viewPortActive || !viewPortActive->isLeaf()) return false;
    if (viewPortActive->ViewportKind() == 3) {
        Properties* p = (Properties*)viewPortActive;
        switch (tecla) {
            case LayoutKey::Up:    p->button_up();        return true;
            case LayoutKey::Down:  p->button_down();      return true;
            case LayoutKey::Left:
                // en las pestañas, izquierda en la 1ra abre el menu tipo/split ([0])
                if (p->focoEnTabs && p->pestaniaActiva == 0) LayoutAbrirMenuTipo(p);
                else p->button_left();
                return true;
            case LayoutKey::Right: p->button_right();     return true;
            case LayoutKey::Enter: p->key_down_return();  return true;
        }
        return false;
    }
    if (viewPortActive->ViewportKind() == 2) {
        Outliner* out = (Outliner*)viewPortActive;
        // MODO MOVER (sin mouse, N95): las flechas reordenan/reparentan el objeto
        // activo en vez de navegar; OK confirma; C/backspace/Esc cancela.
        if (out->ModoMover()) {
            switch (tecla) {
                case LayoutKey::Up:     out->MoverPaso(0);      return true;
                case LayoutKey::Down:   out->MoverPaso(1);      return true;
                case LayoutKey::Left:   out->MoverPaso(2);      return true; // izquierda = SACAR (unparent)
                case LayoutKey::Right:  out->MoverPaso(3);      return true; // derecha = METER (parent)
                case LayoutKey::Enter:  out->MoverConfirmar();  return true;
                case LayoutKey::Cancel: out->MoverCancelar();   return true;
            }
            return true; // en modo mover se traga todo (que nada mas se cuele)
        }
        switch (tecla) {
            case LayoutKey::Up:    changeSelect(SelectMode::PrevSingle, true); return true;
            case LayoutKey::Down:  changeSelect(SelectMode::NextSingle, true); return true;
            case LayoutKey::Left:  SetDesplegado(false); return true;
            case LayoutKey::Right: SetDesplegado(true);  return true;
        }
        return false;
    }
    if (viewPortActive->ViewportKind() == 4) { // UV editor: las flechas PANEAN la vista (pedido Dante)
        UVEditor* uv = (UVEditor*)viewPortActive;
        const float pp = (float)GlobalScale * 16.0f;
        switch (tecla) {
            case LayoutKey::Left:  uv->Panear(+pp, 0); return true;
            case LayoutKey::Right: uv->Panear(-pp, 0); return true;
            case LayoutKey::Up:    uv->Panear(0, +pp); return true;
            case LayoutKey::Down:  uv->Panear(0, -pp); return true;
        }
        return false;
    }
    return false;
}

// nav del editor UV con flechas MANTENIDAS (frame-based, Symbian): paneo CONSTANTE y suave (sin aceleracion,
// pedido Dante), o ZOOM centrado si 0 esta mantenido (0 + arriba/abajo). Devuelve true si el viewport activo
// es el UV editor (asi el caller -AplicarFlechas3D- corta y no orbita la camara 3D).
bool LayoutUVNavFrame(int dx, int dy, bool zoomMode) {
    if (!viewPortActive || !viewPortActive->isLeaf() || viewPortActive->ViewportKind() != 4) return false;
    UVEditor* uv = (UVEditor*)viewPortActive;
    if (zoomMode) {
        if (dy != 0) uv->ZoomCentro(dy < 0 ? 1 : -1); // 0 + arriba (dy<0) = acercar
    } else {
        const float pp = (float)GlobalScale * 6.0f;   // paso constante chico por frame (suave)
        if (dx || dy) uv->Panear(-(float)dx * pp, -(float)dy * pp); // signos = iguales al per-key (Left=+pp)
    }
    return true;
}

// ====================================================================
// overlay: el desplegable abierto, encima de todo
// ====================================================================

// FPS compartido: cada plataforma lo llama UNA vez por frame con su reloj de
// pared en milisegundos (PC: SDL_GetTicks; Symbian: User::NTickCount). El
// promedio se refresca cada ~500ms para que el numero no titile.
void LayoutTickFPS(unsigned long wallMs) {
    static unsigned long t0 = 0;
    static int frames = 0;
    if (t0 == 0) t0 = wallMs;
    frames++;
    unsigned long dt = wallMs - t0;
    if (dt >= 500) {
        g_fpsActual = (float)frames * 1000.0f / (float)dt;
        frames = 0;
        t0 = wallMs;
    }
}

// Tab (PC) / tecla equivalente (Symbian BT): alterna Object <-> Edit Mode del
// objeto ACTIVO si es una malla. Devuelve true si alterno (sino el caller hace
// otra cosa, p.ej. ciclar el viewport). La logica va aca -> PC y Symbian la usan.
bool LayoutToggleEditMode() {
    if (estado != editNavegacion) return false; // no en medio de un transform
    if (!ObjActivo || ObjActivo->getType() != ObjectType::mesh) return false;
    UndoCapturarModo(); // Ctrl+Z: guarda el modo PREVIO antes de togglear
    InteractionMode = (InteractionMode == EditMode) ? ObjectMode : EditMode;
    ActualizarEditMeshActivo(); // refresca g_editMesh (PC + Symbian)
    return true;
}

// ====================================================================
//  NOTIFICACIONES (toasts) — alineadas con el log. Tarjetas abajo a la izquierda
//  con icono verde (OK) o cruz roja (error) + texto. Las de EXITO se cierran solas
//  a los pocos segundos; las de ERROR quedan hasta la 'x' en PC (en Symbian se auto-cierran).
//  El nuevo va ARRIBA (se apilan en columna). Compartido 4 OS.
// ====================================================================
struct Notif {
    std::string msg;
    bool error;
    bool hint;            // cartel-tutorial (azul, persistente, sin 'x'): se cierra por codigo
    float ttl;            // segundos restantes; <= 0 = no se auto-cierra (error en PC, y los hints)
    int rx, ry, rw, rh;   // rect calculado al render
    int xx, xy, xw, xh;   // rect de la 'x' (cerrar), solo error
};
static std::vector<Notif> gNotifs;
static Card* gNotifCard = NULL;
static int  gNotifMx = -1, gNotifMy = -1; // mouse (para el hover de la 'x')
static bool gNotifSobreX = false;         // estaba sobre alguna 'x' el frame previo

void Notificar(const std::string& msg, bool error) {
    w3dLog(msg.c_str());  // tambien al log de diagnostico
    Notif n; n.msg = msg; n.error = error; n.hint = false;
    // TODAS las plataformas: se auto-cierran con un timer (como Symbian). El error dura mas para alcanzar
    // a leerlo. Ademas se pueden cerrar tocando el mensaje (NotificacionesClick). (Dante: exito 6s, error 10s.)
    n.ttl = error ? 10.0f : 6.0f;
    n.rx=n.ry=n.rw=n.rh=n.xx=n.xy=n.xw=n.xh=0;
    gNotifs.insert(gNotifs.begin(), n); // el NUEVO arriba de todo
    if (gNotifs.size() > 8) gNotifs.pop_back();
    g_redraw = true;
}

// cartel-tutorial (azul, persistente): reemplaza el hint anterior. Lo usa el modo guiado de
// "Pick Shortest Path" para ir pidiendo "click the first..." / "...the second...".
void NotificarHint(const std::string& msg) {
    for (size_t i=0;i<gNotifs.size();) { if (gNotifs[i].hint) gNotifs.erase(gNotifs.begin()+i); else i++; }
    Notif n; n.msg = msg; n.error = false; n.hint = true; n.ttl = 0.0f;
    n.rx=n.ry=n.rw=n.rh=n.xx=n.xy=n.xw=n.xh=0;
    gNotifs.insert(gNotifs.begin(), n);
    g_redraw = true;
}
void NotificarHintClear() {
    bool habia = false;
    for (size_t i=0;i<gNotifs.size();) { if (gNotifs[i].hint) { gNotifs.erase(gNotifs.begin()+i); habia=true; } else i++; }
    if (habia) g_redraw = true;
}

void NotificacionesTick(float dt) {
    bool hayTimer = false;
    for (size_t i = 0; i < gNotifs.size(); ) {
        // ttl > 0 = se auto-cierra (exito siempre; error solo en Symbian, donde no hay
        // como tocar la 'x'). ttl <= 0 = persistente (error en PC, y los hints).
        if (!gNotifs[i].hint && gNotifs[i].ttl > 0.0f) {
            gNotifs[i].ttl -= dt;
            if (gNotifs[i].ttl <= 0.0f) { gNotifs.erase(gNotifs.begin()+i); g_redraw = true; continue; }
            hayTimer = true;
        }
        i++;
    }
    if (hayTimer) g_redraw = true; // seguir renderizando para que corra el timer
}

// click sobre una notificacion -> la cierra. Se cierra tocando CUALQUIER parte del mensaje (no solo la 'x',
// que en tactil es dificil de embocar). Los hints (tutorial guiado) NO se cierran a mano (los maneja el codigo).
// true si lo consumio.
bool NotificacionesClick(int mx, int my) {
    for (size_t i = 0; i < gNotifs.size(); i++) {
        Notif& n = gNotifs[i];
        if (n.hint) continue; // el cartel-tutorial se cierra por codigo, no con el toque
        if (mx >= n.rx && mx < n.rx + n.rw && my >= n.ry && my < n.ry + n.rh) {
            gNotifs.erase(gNotifs.begin()+i); g_redraw = true; return true;
        }
    }
    return false;
}

// mouse sobre las notifs: guarda la pos para el hover de la 'x' (gris->blanca).
// Solo re-renderiza al entrar/salir de una 'x' (no en cada pixel de movimiento).
void NotificacionesMotion(int mx, int my) {
    gNotifMx = mx; gNotifMy = my;
    bool sobre = false;
    for (size_t i = 0; i < gNotifs.size(); i++) {
        Notif& n = gNotifs[i];
        if (n.error && mx >= n.xx && mx < n.xx + n.xw && my >= n.xy && my < n.xy + n.xh) { sobre = true; break; }
    }
    if (sobre != gNotifSobreX) { gNotifSobreX = sobre; g_redraw = true; }
}

// envuelve un texto a <= maxLin lineas de a lo sumo 'cpl' caracteres (fuente monoespaciada). Corta por
// palabras; una palabra mas larga que la linea se parte. Si no entra todo, la ultima linea termina en "..."
static std::vector<std::string> NotifWrap(const std::string& s, int cpl, int maxLin){
    std::vector<std::string> out;
    if (cpl < 1) cpl = 1;
    if (maxLin < 1) maxLin = 1;
    std::vector<std::string> words; std::string w;
    for (size_t i = 0; i < s.size(); i++){
        char c = s[i];
        if (c == ' ' || c == '\n' || c == '\t'){ if (!w.empty()){ words.push_back(w); w.clear(); } }
        else w += c;
    }
    if (!w.empty()) words.push_back(w);
    std::string cur; bool corte = false;
    for (size_t wi = 0; wi < words.size() && !corte; wi++){
        std::string word = words[wi];
        while ((int)word.size() > cpl){                 // palabra mas larga que la linea: partirla
            if (!cur.empty()){ out.push_back(cur); cur.clear(); }
            if ((int)out.size() >= maxLin){ corte = true; break; }
            out.push_back(word.substr(0, cpl));
            word = word.substr(cpl);
        }
        if (corte) break;
        if (cur.empty()) cur = word;
        else if ((int)(cur.size() + 1 + word.size()) <= cpl) cur += " " + word;
        else {
            out.push_back(cur); cur.clear();
            if ((int)out.size() >= maxLin){ corte = true; break; }
            cur = word;
        }
    }
    if (!corte && !cur.empty() && (int)out.size() < maxLin) out.push_back(cur);
    else if (!cur.empty()) corte = true; // quedo texto sin colocar -> se trunco
    if (corte && !out.empty()){          // marcar el recorte con "..."
        std::string& last = out.back();
        int room = cpl - 3; if (room < 0) room = 0;
        if ((int)last.size() > room) last = last.substr(0, room);
        last += "...";
    }
    if (out.empty()) out.push_back("");
    return out;
}

void NotificacionesRender(int screenW, int screenH) {
    if (gNotifs.empty()) return;
    if (!gNotifCard) gNotifCard = new Card(NULL, 10, 10);

    w3dEngine::Viewport(0, 0, screenW, screenH);
    w3dEngine::Disable(w3dEngine::ScissorTest);
    w3dEngine::MatrixMode(w3dEngine::Projection); w3dEngine::LoadIdentity();
    w3dEngine::Ortho(0, screenW, screenH, 0, -1, 1);
    w3dEngine::MatrixMode(w3dEngine::ModelView); w3dEngine::LoadIdentity();
    w3dEngine::Disable(w3dEngine::DepthTest); w3dEngine::Disable(w3dEngine::Lighting); w3dEngine::Disable(w3dEngine::Fog);
    w3dEngine::Enable(w3dEngine::Blend); w3dEngine::BlendAlpha();
    w3dEngine::EnableArray(w3dEngine::VertexArray); w3dEngine::EnableArray(w3dEngine::TexCoordArray);
    w3dEngine::DisableArray(w3dEngine::NormalArray);
    if (!Textures.empty()) w3dEngine::BindTexture(Textures[0]->iID);

    const float* gris   = ListaColores[static_cast<int>(ColorID::gris)];
    const float* grisUI = ListaColores[static_cast<int>(ColorID::grisUI)];
    const float* accent = ListaColores[static_cast<int>(ColorID::accent)];
    const float* blanco = ListaColores[static_cast<int>(ColorID::blanco)];
    const float rojo[3] = { 0.92f, 0.28f, 0.24f };
    const float azul[3] = { 0.30f, 0.60f, 0.95f }; // hint (tutorial)

    const int margin = gapGS * 2;
    const int pad    = gapGS;
    const int lineH  = RenglonHeightGS;   // alto de cada linea de texto
    const int xBtnW  = RenglonHeightGS;
    int yBottom = screenH - margin;       // borde inferior de la pila (el mas viejo va abajo); sube hacia arriba

    for (int i = (int)gNotifs.size() - 1; i >= 0; i--) { // viejo abajo -> nuevo arriba
        Notif& n = gNotifs[i];
        const float* col = n.error ? rojo : (n.hint ? azul : accent);
        int textW  = (int)n.msg.size() * CharacterWidthGS;
        int maxCardW = screenW - margin*2;
        int cardW  = pad + IconSizeGS + gapGS + textW + pad + (n.error ? xBtnW : 0);
        bool wrap = false;
        if (cardW > maxCardW){ cardW = maxCardW; wrap = true; } // no cabe en 1 linea -> envolver
#ifdef W3D_SYMBIAN
        cardW = maxCardW; wrap = true;                          // N95: ancho COMPLETO (poca resolucion)
#endif
        int txtMax = cardW - pad - IconSizeGS - gapGS - pad - (n.error ? xBtnW : 0);
        // hasta 3 lineas (pedido Dante): un mensaje largo NUNCA se recorta a media palabra sin avisar
        std::vector<std::string> lineas;
        if (wrap && CharacterWidthGS > 0) lineas = NotifWrap(n.msg, txtMax / CharacterWidthGS, 3);
        else lineas.push_back(n.msg);
        int nlin  = (int)lineas.size(); if (nlin < 1) nlin = 1;
        int cardH = lineH * nlin + pad * 2;

        int x = margin;
        int y = yBottom - cardH;                 // top de esta card
        n.rx=x; n.ry=y; n.rw=cardW; n.rh=cardH;

        // fondo + borde de color
        w3dEngine::Enable(w3dEngine::Texture2D);
        w3dEngine::PushMatrix(); w3dEngine::Translatef((GLfloat)x, (GLfloat)y, 0);
        gNotifCard->Resize(cardW, cardH);
        w3dEngine::Color4f(gris[0], gris[1], gris[2], 0.96f);  gNotifCard->RenderObject(false);
        w3dEngine::Color4f(col[0], col[1], col[2], 1.0f);      gNotifCard->RenderBorder(false);
        w3dEngine::PopMatrix();

        // icono (tinte rojo/verde) a la izquierda, alineado con la PRIMERA linea
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)(x + pad), (GLfloat)(y + pad + (lineH - IconSizeGS)/2), 0);
        w3dEngine::Color4f(col[0], col[1], col[2], 1.0f);
        int icon = n.error ? (int)IconType::notifError : (int)IconType::notifOk;
        W3dDrawStrip4(IconMesh, IconsUV[icon]->uvs);
        w3dEngine::PopMatrix();

        // texto (1 a 3 lineas)
        w3dEngine::Color4f(blanco[0], blanco[1], blanco[2], 1.0f);
        for (int L = 0; L < nlin; L++){
            w3dEngine::PushMatrix();
            w3dEngine::Translatef((GLfloat)(x + pad + IconSizeGS + gapGS), (GLfloat)(y + pad + L*lineH), 0);
            RenderBitmapText(lineas[L], textAlign::left, txtMax);
            w3dEngine::PopMatrix();
        }

        // 'x' de cerrar (solo error): chica, en la ESQUINA superior derecha.
        // gris por defecto; blanca al pasar el mouse. El glyph 'x' del font tiene
        // su tinta (5px) ABAJO del cell (11px) con pixeles vacios arriba, asi que
        // lo subo esa cantidad para que quede pegado a la esquina superior.
        if (n.error) {
            int bw = CharacterWidthGS + gapGS;          // ancho clickeable
            int bx = x + cardW - bw - gapGS / 2 + 1;    // pegada a la derecha (+1px: no tocar el borde)
            int inset = GlobalScale * 2;                // separacion chica del borde
            int vacioArriba = LetterHeightGS - 5 * GlobalScale; // px vacios sobre la 'x'
            int byGlyph = y + inset - vacioArriba + 7;  // baja la 'x' de la esquina (+7px: no tocar el borde superior)
            n.xx = bx; n.xy = y; n.xw = bw; n.xh = RenglonHeightGS; // hit-rect del corner
            bool hover = (gNotifMx >= n.xx && gNotifMx < n.xx + n.xw &&
                          gNotifMy >= n.xy && gNotifMy < n.xy + n.xh);
            const float* cx = hover ? blanco : grisUI;
            w3dEngine::PushMatrix(); w3dEngine::Translatef((GLfloat)bx, (GLfloat)byGlyph, 0);
            w3dEngine::Color4f(cx[0], cx[1], cx[2], 1.0f);
            RenderBitmapText("x", textAlign::center, bw);
            w3dEngine::PopMatrix();
        }
        yBottom = y - gapGS; // la proxima (mas nueva) va ARRIBA de esta
    }
}

void LayoutRenderMenu(int screenW, int screenH) {
    bool hayMenu = LayoutMenuAbierto();
    if (!hayMenu && !PopUpActive) return;

    if (PopUpActive) PopUpActive->Render(); // popup modal (color picker)
    if (!hayMenu) return;
    w3dEngine::Viewport(0, 0, screenW, screenH);
    w3dEngine::Disable(w3dEngine::ScissorTest);
    w3dEngine::MatrixMode(w3dEngine::Projection);
    w3dEngine::LoadIdentity();
    w3dEngine::Ortho(0, screenW, screenH, 0, -1, 1);
    w3dEngine::MatrixMode(w3dEngine::ModelView);
    w3dEngine::LoadIdentity();
    w3dEngine::Disable(w3dEngine::DepthTest);
    w3dEngine::Disable(w3dEngine::Lighting);
    w3dEngine::Disable(w3dEngine::Fog);
    w3dEngine::Enable(w3dEngine::Texture2D);
    w3dEngine::Enable(w3dEngine::Blend);
    w3dEngine::BlendAlpha();
    w3dEngine::EnableArray(w3dEngine::VertexArray);
    w3dEngine::EnableArray(w3dEngine::TexCoordArray);
    w3dEngine::DisableArray(w3dEngine::NormalArray);
    w3dEngine::BindTexture(Textures[0]->iID);
    MenuAbierto->Render();
}

// ====================================================================
// pick 3D por color (antes vivia en w3dnewscene.cpp, solo Symbian)
// ====================================================================

static int pickCounter = 0;
static Object* pickFound = NULL;

// tipos seleccionables por click: malla (su geometria) o un objeto con icono
// (camara, luz, empty, instancia) -> un punto del tamaño del icono en su origen
static bool PickSeleccionable(Object* obj) {
    if (!obj->visible) return false;
    ObjectType t = obj->getType();
    return t == ObjectType::mesh || t == ObjectType::camera ||
           t == ObjectType::light || t == ObjectType::empty ||
           t == ObjectType::instance;
}

static void PickPaint(Object* obj) {
    if (!obj) return;
    if (PickSeleccionable(obj)) {
        pickCounter++;
        int id = pickCounter; // 1..N
        w3dEngine::Color4f(((id & 0x1F)) / 31.0f, ((id >> 5) & 0x3F) / 63.0f, 0.0f, 1.0f);
        w3dEngine::PushMatrix();
        Matrix4 M;
        obj->GetMatrix(M);
        w3dEngine::MultMatrix(M.m);
        if (obj->getType() == ObjectType::mesh) {
            Mesh* m = (Mesh*)obj;
            // con modificadores (subdiv/screw) se dibuja la malla GENERADA para el pick: es lo que se VE.
            // Sin esto, un Screw (perfil sin caras) no pinta nada -> no se podia clickear/seleccionar.
            if (m->genValido && m->genVertex && m->genFaces && m->genFacesSize > 0) {
                w3dEngine::VertexPointer3f(0, m->genVertex);
                w3dEngine::DrawTriangles(m->genFacesSize, m->genFaces);
            } else if (m->vertex && m->faces) {
                w3dEngine::VertexPointer3f(0, m->vertex);
                w3dEngine::DrawTriangles(m->facesSize, m->faces);
            }
        } else {
            // icono (camara/luz/empty/instancia): un punto clickeable en el
            // origen, del MISMO tamaño que el icono 3D (16 * GlobalScale)
            static const GLfloat origen[3] = { 0.0f, 0.0f, 0.0f };
            w3dEngine::PointSize(16.0f * GlobalScale);
            w3dEngine::VertexPointer3f(0, origen);
            w3dEngine::DrawPoints(1);
        }
        w3dEngine::PopMatrix();
    }
    for (size_t i = 0; i < obj->Childrens.size(); i++) {
        PickPaint(obj->Childrens[i]);
    }
}

static void PickResolve(Object* obj, int target) {
    if (!obj || pickFound) return;
    if (PickSeleccionable(obj)) {
        pickCounter++;
        if (pickCounter == target) { pickFound = obj; return; }
    }
    for (size_t i = 0; i < obj->Childrens.size(); i++) {
        PickResolve(obj->Childrens[i], target);
    }
}

// hubo input TACTIL en la sesion? (para agrandar el pick de vertices en pantallas tactiles)
#ifndef W3D_SYMBIAN
extern Uint32 g_lastFingerTicks; // controles.cpp
static bool PickEsTactil(){ return g_lastFingerTicks != 0; }
#else
static bool PickEsTactil(){ return false; }
#endif

// EDIT MODE: pick de VERTICE o ARISTA por color-ID (segun EditSelectMode). Dibuja
// los sub-elementos con un color = (id+1) en R5G6B5 (tolera framebuffer 16-bit:
// hasta 65535), lee el pixel bajo el click y resuelve. Sin shift = solo ese; con
// shift = toggle. Usa la EditMesh (datos de edicion separados de la malla de render).
// Pickea por color-ID el sub-elemento bajo (mx,my) en el MODO dado (SelVertex/Edge/
// Face, NO necesariamente el modo activo: el loop-select pickea un BORDE en modo cara).
// Devuelve el indice 0-based o -1. NO modifica la seleccion.
static int EditPickIndex(int modo, int mx, int my, int vx, int vy, int vw, int vh, int screenH) {
    Mesh* m = (Mesh*)g_editMesh;
    if (!m) return -1;
    m->EnsureEdit();
    EditMesh* e = m->edit;
    if (!e || e->pos.empty()) return -1;
    const bool edgeMode = (modo == SelEdge);
    const bool faceMode = (modo == SelFace);
    const int N = faceMode ? e->NumFaces() : edgeMode ? e->NumEdges() : e->NumVerts();
    if (N <= 0) return -1;

    // misma proyeccion + camara que el viewport real
    int glY = screenH - vy - vh;
    w3dEngine::Viewport(vx, glY, vw, vh);
    w3dEngine::Scissor(vx, glY, vw, vh);
    w3dEngine::Enable(w3dEngine::ScissorTest);
    w3dEngine::MatrixMode(w3dEngine::Projection);
    w3dEngine::LoadIdentity();
    float pkNear = Viewport3DActive ? Viewport3DActive->nearClip : 0.01f;
    float pkFar  = Viewport3DActive ? Viewport3DActive->farClip  : 1000.0f;
    float pkAspect = (float)vw / (float)vh;
    if (Viewport3DActive && Viewport3DActive->orthographic) {
        // MISMA proyeccion ortografica que Render() (size = orbitDistance*tan(fov/2)); antes pickeaba SIEMPRE con
        // perspectiva -> en orto el click caia corrido. Ahora el color-id usa el mismo volumen que se ve.
        float size = Viewport3DActive->orbitDistance * tanf(fovDeg * 0.5f * 3.14159265f / 180.0f);
        if (size < 0.001f) size = 0.001f;
        w3dEngine::Ortho(-size * pkAspect, size * pkAspect, -size, size, pkNear, Viewport3DActive->orbitDistance + pkFar);
    } else {
        w3dEngine::Perspective(fovDeg, pkAspect, pkNear, pkFar);
    }
    w3dEngine::MatrixMode(w3dEngine::ModelView);
    w3dEngine::LoadIdentity();
    if (Viewport3DActive) Viewport3DActive->UpdateViewOrbit();
    // matriz de MUNDO del objeto: la MISMA que usa el foco y el render. Antes el
    // pick rearmaba la cadena a mano con la condicion "o->Parent", que salteaba la
    // matriz del PROPIO objeto si era top-level (Parent NULL: el ctor lo agrega a
    // SceneCollection->Childrens pero NO le setea Parent) y la del padre top -> a
    // escala!=1 o emparentado, el pick caia corrido (a escala 1 ~= identidad y zafaba).
    {
        Matrix4 W; m->GetWorldMatrix(W);
        w3dEngine::MultMatrix(W.m);
    }

    w3dEngine::Disable(w3dEngine::Dither);
    // MSAA *mezcla* los colores-ID en los bordes (4 samples promediados) -> el color
    // leido decodifica a un indice CUALQUIERA (lejano) -> el pick "agarra otra cosa de
    // la pantalla", impredecible. Apagarlo deja cada pixel cubierto con el ID solido.
    w3dEngine::Disable(w3dEngine::Multisample);
    w3dEngine::Disable(w3dEngine::Lighting);
    w3dEngine::Disable(w3dEngine::Texture2D);
    w3dEngine::Disable(w3dEngine::Blend);
    w3dEngine::DisableArray(w3dEngine::NormalArray);
    w3dEngine::DisableArray(w3dEngine::TexCoordArray);
    w3dEngine::EnableArray(w3dEngine::VertexArray);
    w3dEngine::EnableArray(w3dEngine::ColorArray);
    w3dEngine::Enable(w3dEngine::DepthTest);
    w3dEngine::ClearColor(0, 0, 0, 1);
    w3dEngine::Clear(w3dEngine::ColorBuffer | w3dEngine::DepthBuffer);

    // OCLUSION: dibujar la malla RELLENA solo en el depth (sin color), un toque
    // atras (polygon offset), para que los sub-elementos de ATRAS -tapados por la
    // malla en el render- NO sean clickeables. Solo se pickea lo que se VE adelante.
    // (En PC el depth-test de los puntos ya lo resolvia; en Symbian no, y se
    // seleccionaba un elemento de atras que caia cerca en pantalla.)
    if (m->vertex && m->faces && m->facesSize >= 3) {
        w3dEngine::ColorMask(false, false, false, false);
        w3dEngine::Disable(w3dEngine::CullFace);
        w3dEngine::DisableArray(w3dEngine::ColorArray);
        w3dEngine::Enable(w3dEngine::PolygonOffsetFill);
        w3dEngine::PolygonOffset(1.0f, 1.0f);
        w3dEngine::VertexPointer3f(0, m->vertex);
        w3dEngine::DrawTriangles(m->facesSize, m->faces);
        w3dEngine::Disable(w3dEngine::PolygonOffsetFill);
        w3dEngine::ColorMask(true, true, true, true);
        w3dEngine::EnableArray(w3dEngine::ColorArray);
    }

    if (faceMode) {
        // caras: triangulos EXPANDIDOS con color-ID por cara (vertices propios por
        // cara: un vertice compartido no puede tener 2 colores de cara distintos)
        std::vector<GLfloat> tp; std::vector<GLubyte> tc;
        for (int f = 0; f < N; f++) {
            int id = f + 1;
            GLubyte r=(GLubyte)((id&0x1F)<<3), g=(GLubyte)(((id>>5)&0x3F)<<2), b=(GLubyte)(((id>>11)&0x1F)<<3);
            const std::vector<int>& p = e->faces[(size_t)f];
            for (size_t t = 1; t + 1 < p.size(); t++) {
                int tri[3] = { p[0], p[t], p[t+1] };
                for (int j = 0; j < 3; j++) {
                    int v = tri[j];
                    tp.push_back(e->pos[v*3]); tp.push_back(e->pos[v*3+1]); tp.push_back(e->pos[v*3+2]);
                    tc.push_back(r); tc.push_back(g); tc.push_back(b); tc.push_back(255);
                }
            }
        }
        if (!tp.empty()) {
            w3dEngine::VertexPointer3f(0, &tp[0]);
            w3dEngine::ColorPointer4ub(&tc[0]);
            w3dEngine::DrawTrianglesArray((int)(tp.size() / 3));
        }
    } else if (edgeMode) {
        // aristas como lineas GORDAS con color-ID por arista (los 2 vertices iguales)
        const int NV = (int)(e->linePos.size() / 3); // = N*2
        std::vector<GLubyte> idcol((size_t)NV * 4);
        for (int eg = 0; eg < N; eg++) {
            int id = eg + 1;
            GLubyte r=(GLubyte)((id&0x1F)<<3), g=(GLubyte)(((id>>5)&0x3F)<<2), b=(GLubyte)(((id>>11)&0x1F)<<3);
            for (int s = 0; s < 2; s++) { int v = eg*2+s; idcol[v*4]=r; idcol[v*4+1]=g; idcol[v*4+2]=b; idcol[v*4+3]=255; }
        }
        w3dEngine::VertexPointer3f(0, &e->linePos[0]);
        w3dEngine::ColorPointer4ub(&idcol[0]);
        w3dEngine::LineWidth(9.0f); // gorda: facil de clickear
        w3dEngine::DrawLines(NV);
        w3dEngine::LineWidth(1.0f);
    } else {
        std::vector<GLubyte> idcol((size_t)N * 4);
        for (int k = 0; k < N; k++) {
            int id = k + 1;
            idcol[k*4]   = (GLubyte)((id & 0x1F) << 3);
            idcol[k*4+1] = (GLubyte)(((id >> 5) & 0x3F) << 2);
            idcol[k*4+2] = (GLubyte)(((id >> 11) & 0x1F) << 3);
            idcol[k*4+3] = 255;
        }
        w3dEngine::VertexPointer3f(0, &e->pos[0]);
        w3dEngine::ColorPointer4ub(&idcol[0]);
        // TOUCH: punto de pick un poco mas grande (los vertices son puntitos, dificiles con el dedo), pero
        // SIN exagerar: puntos muy grandes se solapan en mallas densas y sesgan el voto. La tolerancia real
        // la da el area de lectura ampliada (abajo). Con mouse queda igual (22).
        w3dEngine::PointSize(PickEsTactil() ? 28.0f : 22.0f);
        w3dEngine::DrawPoints(N);
        w3dEngine::PointSize(1.0f);
    }

    // leer un AREA chica (no 1 pixel) y tomar el ID mas cercano al centro: tolera
    // que el cursor virtual del telefono caiga un par de pixeles corrido + el
    // clamping del tamano de punto/linea del driver. Clampeado al viewport.
#ifdef __EMSCRIPTEN__
    // WebGL clampea glLineWidth a 1px (las aristas del pick se dibujan de 1px en vez de 9) y limita
    // el tamano de punto -> leemos un AREA MAS GRANDE para pescar aristas/caras finas bajo el click.
    int RAD = 7;
#else
    int RAD = 3;
#endif
    // VERTICE + TOUCH: area de lectura GRANDE -> el vertice mas cercano al dedo gana (el voto por
    // pixeles favorece al que tiene el punto mas centrado). Aristas/caras y mouse quedan igual.
    if (!faceMode && !edgeMode && PickEsTactil()) RAD = 16;
    const int WD = 2 * RAD + 1;
    int cxw = mx, cyw = screenH - 1 - my; // centro en coords de ventana (GL)
    int x0 = cxw - RAD, y0 = cyw - RAD;
    if (x0 < vx) x0 = vx; if (x0 + WD > vx + vw) x0 = vx + vw - WD;
    if (y0 < glY) y0 = glY; if (y0 + WD > glY + vh) y0 = glY + vh - WD;
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0;
    GLubyte area[33 * 33 * 4]; // dimensionado para el WD mas grande (vertice+touch: 33); otros modos usan menos
    w3dEngine::ReadPixelsRGBA(x0, y0, WD, WD, area);
    w3dEngine::Enable(w3dEngine::Dither);
    w3dEngine::Enable(w3dEngine::Multisample); // restaurar el MSAA para el render normal
    w3dEngine::Disable(w3dEngine::ScissorTest);
    w3dEngine::EnableArray(w3dEngine::TexCoordArray); // baseline de la UI

    int ccx = cxw - x0, ccy = cyw - y0; // donde cae el centro real dentro del area
    // MSAA-robusto: NO tomamos el pixel valido mas cercano al centro (que puede ser BASURA de un borde que el
    // antialiasing mezclo -> decodifica a un id CUALQUIERA y selecciona un vert lejano en cualquier direccion).
    // En su lugar VOTAMOS: cada id suma 1 por pixel. Un vert real cubre un BLOQUE solido; un artefacto de MSAA es
    // un pixel suelto -> pierde. Desempate por cercania al centro (para elegir el vert justo bajo el click).
    int id = 0;
    {
        std::map<int,int> cnt; std::map<int,long> nearest;
        for (int dy = 0; dy < WD; dy++) for (int dx = 0; dx < WD; dx++) {
            GLubyte* px = &area[(dy*WD + dx) * 4];
            int cand = (px[0] >> 3) | ((px[1] >> 2) << 5) | ((px[2] >> 3) << 11);
            if (cand <= 0 || cand > N) continue;
            long ex = dx - ccx, ey = dy - ccy, d = ex*ex + ey*ey;
            cnt[cand]++;
            std::map<int,long>::iterator itn = nearest.find(cand);
            if (itn == nearest.end() || d < itn->second) nearest[cand] = d;
        }
        int bestCnt = 0; long bestNear = 1L << 30;
        for (std::map<int,int>::iterator it = cnt.begin(); it != cnt.end(); ++it) {
            long nr = nearest[it->first];
            if (it->second > bestCnt || (it->second == bestCnt && nr < bestNear)) { bestCnt = it->second; bestNear = nr; id = it->first; }
        }
    }
    int k = id - 1;
    return (k >= 0 && k < N) ? k : -1;
}

// click normal de seleccion: pickea en el modo activo y togglea (sin shift = solo ese).
static bool EditPickVert(int mx, int my, int vx, int vy, int vw, int vh, int screenH) {
    Mesh* m = (Mesh*)g_editMesh;
    if (!m) return true;
    m->EnsureEdit();
    EditMesh* e = m->edit;
    if (!e) return true;
    int k = EditPickIndex(EditSelectMode, mx, my, vx, vy, vw, vh, screenH);
    if (k >= 0) {
        if (EditSelectMode == SelFace)      e->TogglearFace(k, !LShiftPressed);
        else if (EditSelectMode == SelEdge) e->TogglearEdge(k, !LShiftPressed);
        else                                e->TogglearVert(k, !LShiftPressed);
    } else if (!LShiftPressed) {
        e->SeleccionarTodo(false); // click al vacio sin shift: deseleccionar todo
    }
    return true; // el frame siguiente redibuja la escena real
}

// ===== Navegacion de seleccion por TECLADO en Edit Mode (lapiz + flechas de Symbian) =====
// Opera sobre vert/edge/face segun EditSelectMode, tocando el array de seleccion + activeIdx;
// Recolorear() refresca el visual (incluye el relleno de caras). Devuelve false si no aplica.
static unsigned char* EditSelArray(EditMesh* e, int& N){
    if (EditSelectMode == SelFace){ N=(int)e->faceSel.size(); return e->faceSel.empty()?NULL:&e->faceSel[0]; }
    if (EditSelectMode == SelEdge){ N=(int)e->edgeSel.size(); return e->edgeSel.empty()?NULL:&e->edgeSel[0]; }
    N=(int)e->vertSel.size(); return e->vertSel.empty()?NULL:&e->vertSel[0];
}
static EditMesh* EditSelMesh(){
    if (InteractionMode != EditMode || !g_editMesh) return NULL;
    Mesh* m=(Mesh*)g_editMesh; m->EnsureEdit(); return m->edit;
}

// ===== Navegacion de seleccion de OBJETOS (Object Mode) =====
// Espejo EXACTO de la de Edit Mode, pero sobre los objetos de la escena (mismo lapiz +
// flechas de Symbian). El "activo" es ObjActivo; el "array" es la lista plana de objetos
// (sin colecciones) en orden de outliner. Seleccionar()/Deseleccionar() mantienen select +
// ObjSelects + ObjActivo. NO tocar la seleccion durante un transform (estado != editNavegacion,
// como DeseleccionarTodo: estadoObjetos apunta a los seleccionados y crashea si cambia).
static void ObjSelRecolectar(Object* nodo, std::vector<Object*>& out){
    if (!nodo) return;
    for (size_t i=0;i<nodo->Childrens.size();i++){
        Object* h = nodo->Childrens[i];
        if (!h) continue;
        if (h->getType() != ObjectType::collection) out.push_back(h); // los objetos, no colecciones
        ObjSelRecolectar(h, out);                                     // recursivo (sub-colecciones)
    }
}
static bool ObjSelLista(std::vector<Object*>& v){
    if (!SceneCollection) return false;
    ObjSelRecolectar(SceneCollection, v);
    return !v.empty();
}
static int ObjSelActivoIdx(const std::vector<Object*>& v){
    for (size_t i=0;i<v.size();i++) if (v[i]==ObjActivo) return (int)i;
    return -1;
}
static bool ObjSelAvanzar(int paso, bool extender){
    if (estado != editNavegacion) return true;        // no durante un transform
    std::vector<Object*> v; if (!ObjSelLista(v)) return true;
    int N=(int)v.size();
    int act = ObjSelActivoIdx(v);
    if (act<0){ act=0; for (int i=0;i<N;i++) if (v[i]->select){ act=i; break; } } // sin activo: 1er sel (o 0)
    if (!extender) v[act]->Deseleccionar();           // deselecciona SOLO el activo (el resto queda)
    int next = act + paso; if (next<0) next=N-1; if (next>=N) next=0;
    v[next]->Seleccionar();                            // select=true + ObjActivo=v[next]
    g_redraw = true;
    return true;
}
static bool ObjSelTodoToggle(){
    if (estado != editNavegacion) return true;
    std::vector<Object*> v; if (!ObjSelLista(v)) return true;
    int N=(int)v.size();
    bool todo = true; for (int i=0;i<N;i++) if (!v[i]->select){ todo=false; break; }
    if (todo){ for (int i=0;i<N;i++) v[i]->Deseleccionar(); ObjActivo = NULL; }
    else     { for (int i=0;i<N;i++) v[i]->Seleccionar(); ObjActivo = v[0]; }
    g_redraw = true;
    return true;
}
static bool ObjSelToggleActual(){
    if (estado != editNavegacion) return true;
    std::vector<Object*> v; if (!ObjSelLista(v)) return true;
    int N=(int)v.size();
    int act = ObjSelActivoIdx(v);
    if (act<0){ act=0; for (int i=0;i<N;i++) if (v[i]->select){ act=i; break; } if (!v[act]->select) return true; }
    if (v[act]->select){ v[act]->Deseleccionar(); ObjActivo = NULL; }
    else                 v[act]->Seleccionar();        // Seleccionar setea ObjActivo
    g_redraw = true;
    return true;
}

// lapiz solo (extender=false): DESELECCIONA el activo + selecciona el siguiente (el resto
// queda). lapiz+flecha (extender=true): mantiene todo + agrega el siguiente/anterior. El
// nuevo siempre queda ACTIVO. paso = +1 (siguiente) / -1 (anterior).
// Mode-aware: en Object Mode delega a la version de objetos (mismo comportamiento exacto).
bool EditSelAvanzar(int paso, bool extender){
    if (InteractionMode != EditMode) return ObjSelAvanzar(paso, extender);
    EditMesh* e = EditSelMesh(); if (!e) return false;
    int N=0; unsigned char* sel = EditSelArray(e, N);
    if (!sel || N<=0) return true;
    int act = e->activeIdx;
    if (act<0 || act>=N){ act=0; for (int i=0;i<N;i++) if (sel[i]){ act=i; break; } } // sin activo: 1er sel (o 0)
    if (!extender) sel[act] = 0;          // deselecciona el activo
    int next = act + paso;
    if (next < 0) next = N-1;
    if (next >= N) next = 0;
    sel[next] = 1;                        // selecciona el siguiente (si ya estaba, queda)
    e->activeIdx = next;
    e->Recolorear(); g_redraw = true;
    return true;
}
// lapiz+arriba: si TODO esta seleccionado -> nada; sino -> todo.
bool EditSelTodoToggle(){
    if (InteractionMode != EditMode) return ObjSelTodoToggle();
    EditMesh* e = EditSelMesh(); if (!e) return false;
    int N=0; unsigned char* sel = EditSelArray(e, N);
    if (!sel || N<=0) return true;
    bool todo = true; for (int i=0;i<N;i++) if (!sel[i]){ todo=false; break; }
    for (int i=0;i<N;i++) sel[i] = todo ? 0 : 1;
    e->activeIdx = todo ? -1 : 0;
    e->Recolorear(); g_redraw = true;
    return true;
}
// lapiz+abajo: togglea el indice ACTIVO. Si lo deselecciona, pierde el activo.
bool EditSelToggleActual(){
    if (InteractionMode != EditMode) return ObjSelToggleActual();
    EditMesh* e = EditSelMesh(); if (!e) return false;
    int N=0; unsigned char* sel = EditSelArray(e, N);
    if (!sel || N<=0) return true;
    int act = e->activeIdx;
    if (act<0 || act>=N){ act=0; for (int i=0;i<N;i++) if (sel[i]){ act=i; break; } if (act<0) return true; }
    sel[act] = sel[act] ? 0 : 1;
    e->activeIdx = sel[act] ? act : -1;
    e->Recolorear(); g_redraw = true;
    return true;
}

// L: Select Linked — selecciona la ISLA (componente conexa) bajo el mouse, en el
// modo activo. Sin shift reemplaza; con shift agrega. Usa el rect del viewport activo.
void LayoutSelectLinked(int mx, int my) {
    if (InteractionMode != EditMode || !g_editMesh || !Viewport3DActive) return;
    Mesh* m = (Mesh*)g_editMesh;
    m->EnsureEdit();
    if (!m->edit) return;
    UndoCapturarSeleccionEdit(m); // Ctrl+Z: Select Linked va a cambiar la seleccion
    int vx = Viewport3DActive->x, vy = Viewport3DActive->y;
    int vw = Viewport3DActive->width, vh = Viewport3DActive->height;
    int k = EditPickIndex(EditSelectMode, mx, my, vx, vy, vw, vh, W3dPantallaAlto);
    if (k >= 0) { m->edit->SeleccionarLinked(k, EditSelectMode, !LShiftPressed); g_redraw = true; }
}

// ===== LOOP SELECT desde el sub-elemento ACTIVO (menu Select / Symbian lapiz+OK) =====
// Modo BORDE: el borde activo da la direccion (sin modal). Modo CARA: una cara tiene 2 sentidos,
// asi que arranca un modal de orientacion: las flechas alternan, OK/click/enter acepta. La
// seleccion resaltada ES el preview (no hace falta geometria aparte).
static bool gLoopSelOrient = false;
static int  gLoopSelFace   = -1;
static int  gLoopSelDir    = 0;   // 0 / 1 (faceEdges[face][0] o [1])
bool LoopSelOrientando(){ return gLoopSelOrient; }

void LayoutLoopSelectGuiado(); // definida mas abajo (modo guiado: pide click cuando no hay elemento activo)
void LayoutLoopSelectActivo(int tipo){
    if (InteractionMode != EditMode || !g_editMesh) return;
    Mesh* m=(Mesh*)g_editMesh; m->EnsureEdit(); if (!m->edit) return;
    EditMesh* e=m->edit;
    if (tipo == 2){                                   // FACE LOOP desde la cara activa
        int act = e->activeIdx;
        if (act<0) for (size_t f=0;f<e->faceSel.size();f++) if (e->faceSel[f]){ act=(int)f; break; }
        if (act<0){ LayoutLoopSelectGuiado(); return; }  // SIN cara activa -> modo guiado (pedi click o cancela)
        if (act>=(int)e->faceEdges.size() || e->faceEdges[act].size()<2) return;
        UndoCapturarSeleccionEdit(m); // Ctrl+Z: el loop select (+ refinar direccion con flechas) = 1 sola accion
        gLoopSelFace = act; gLoopSelDir = 0; gLoopSelOrient = true;
        e->SeleccionarLoopFace(e->faceEdges[act][0], true);
        NotificarHint("Loop Select: arrows = direction, Enter to confirm");
    } else {                                          // EDGE LOOP / RING desde el borde activo
        int act = e->activeIdx;
        if (act<0) for (size_t k=0;k<e->edgeSel.size();k++) if (e->edgeSel[k]){ act=(int)k; break; }
        if (act<0){ LayoutLoopSelectGuiado(); return; }  // SIN borde activo -> modo guiado
        UndoCapturarSeleccionEdit(m);
        if (tipo==0) e->SeleccionarLoopEdge(act, true);
        else         e->SeleccionarRingEdge(act, true);
    }
    g_redraw = true;
}
void LoopSelTecla(int dir){
    if (!gLoopSelOrient || !g_editMesh) return;
    Mesh* m=(Mesh*)g_editMesh; if (!m->edit) return;
    EditMesh* e=m->edit;
    if (gLoopSelFace<0 || gLoopSelFace>=(int)e->faceEdges.size() || e->faceEdges[gLoopSelFace].size()<2) return;
    gLoopSelDir = (dir==1 || dir==3) ? 1 : 0;
    e->SeleccionarLoopFace(e->faceEdges[gLoopSelFace][gLoopSelDir], true);
    g_redraw = true;
}
void LoopSelConfirm(){ gLoopSelOrient = false; NotificarHintClear(); g_redraw = true; }

// Loop select en una POSICION (cursor): pickea el borde bajo (mx,my) y aplica el loop del modo
// (cara -> Face Loop ; borde -> Edge Loop). El cursor da la direccion, asi que NO hay modal.
// Lo usa el mouse virtual de Symbian (lapiz + OK). Devuelve false si no pickeo nada.
bool LayoutLoopSelectEnPos(int mx, int my, int vx, int vy, int vw, int vh, int screenH){
    if (InteractionMode != EditMode || !g_editMesh) return false;
    Mesh* m=(Mesh*)g_editMesh; m->EnsureEdit(); if (!m->edit) return false;
    int eg = EditPickIndex(SelEdge, mx, my, vx, vy, vw, vh, screenH); // siempre pickea un BORDE de referencia
    if (eg < 0) return false;
    UndoCapturarSeleccionEdit(m); // Ctrl+Z: loop select por cursor (Symbian) va a cambiar la seleccion
    EditMesh* ed = m->edit;
    if (EditSelectMode == SelFace)      ed->SeleccionarLoopFace(eg, true);
    else if (EditSelectMode == SelEdge) ed->SeleccionarLoopEdge(eg, true);
    else                                ed->SeleccionarLoopEdgeVerts(eg, true); // VERTICE: verts del loop, sin acumular edgeSel
    g_redraw = true;
    return true;
}

// ===== PICK SHORTEST PATH guiado desde el menu Select (2 clicks con cartel-tutorial) =====
// Al activarlo desde el menu sale un hint "click the first <elem>"; el 1er click fija el activo
// y el cartel pasa a "...the second <elem>"; el 2do click hace el shortest path (con/sin fill).
// El click se intercepta en ScenePick3D, asi anda IGUAL en PC (mouse) y Symbian (mouse virtual + OK).
static bool gPathGuided = false;
static bool gPathFill   = false;
static int  gPathStep   = 0;   // 0 = espera el primero, 1 = espera el segundo
bool PickPathGuiado(){ return gPathGuided; }

static const char* PathElemNombre(){
    if (EditSelectMode == SelFace) return "face";
    if (EditSelectMode == SelEdge) return "edge";
    return "vertex";
}
void LayoutPickPathIniciar(bool fill){
    if (InteractionMode != EditMode || !g_editMesh) return;
    gPathGuided = true; gPathFill = fill; gPathStep = 0;
    NotificarHint(std::string("Pick Shortest Path: click the first ") + PathElemNombre());
    g_redraw = true;
}
void LayoutPickPathCancelar(){
    if (!gPathGuided) return;
    gPathGuided = false; NotificarHintClear(); g_redraw = true;
}
// click durante el modo guiado: true si lo consumio (ScenePick3D NO sigue con el pick normal).
static bool PickPathClick(int mx,int my,int vx,int vy,int vw,int vh,int screenH){
    if (!gPathGuided || !g_editMesh) return false;
    Mesh* m=(Mesh*)g_editMesh; m->EnsureEdit();
    if (!m->edit){ gPathGuided=false; NotificarHintClear(); return true; }
    int k = EditPickIndex(EditSelectMode, mx,my,vx,vy,vw,vh,screenH);
    if (k < 0) return true;   // click al vacio: ignora (sigue esperando el mismo paso)
    if (gPathStep == 0){
        if (EditSelectMode==SelFace)      m->edit->TogglearFace(k, true);  // selecciona el primero (activo)
        else if (EditSelectMode==SelEdge) m->edit->TogglearEdge(k, true);
        else                              m->edit->TogglearVert(k, true);
        gPathStep = 1;
        NotificarHint(std::string("Now click the second ") + PathElemNombre());
    } else {
        m->edit->SeleccionarShortestPath(k, gPathFill);
        gPathGuided = false;
        NotificarHintClear();
        Notificar(gPathFill ? "Shortest path: region filled" : "Shortest path selected", false);
    }
    g_redraw = true;
    return true;
}

// ===== modo guiado de 1 CLICK (Select Linked desde el menu; Loop Select sin elemento activo) =====
// Sale un hint pidiendo click; el click hace la operacion y termina (1 solo click, vs los 2 del shortest path).
// El click se intercepta en ScenePick3D -> anda IGUAL en PC (mouse) y Symbian (mouse virtual + OK).
static int gGuiadoOp = 0; // 0 = ninguno, 1 = Select Linked, 2 = Loop Select
bool GuiadoUnClickActivo(){ return gGuiadoOp != 0; }
void LayoutGuiadoCancelar(){ if (!gGuiadoOp) return; gGuiadoOp = 0; NotificarHintClear(); g_redraw = true; }
static const char* GuiadoElemNombre(){ return (EditSelectMode==SelFace)?"face":(EditSelectMode==SelEdge)?"edge":"vertex"; }
void LayoutSelectLinkedGuiado(){
    if (InteractionMode != EditMode || !g_editMesh) return;
    gGuiadoOp = 1;
    NotificarHint(std::string("Select Linked: click the ") + GuiadoElemNombre() + " you want to select (Esc to cancel)");
    g_redraw = true;
}
void LayoutLoopSelectGuiado(){ // fallback de Loop Select cuando no hay elemento activo (pide click)
    if (InteractionMode != EditMode || !g_editMesh) return;
    gGuiadoOp = 2;
    NotificarHint(std::string("Loop Select: click an ") + ((EditSelectMode==SelFace)?"edge of a face":"edge") + " (Esc to cancel)");
    g_redraw = true;
}
// click durante el modo guiado: true si lo consumio (ScenePick3D NO sigue con el pick normal).
static bool GuiadoUnClickClick(int mx,int my,int vx,int vy,int vw,int vh,int screenH){
    if (!gGuiadoOp || !g_editMesh) return false;
    Mesh* m=(Mesh*)g_editMesh; m->EnsureEdit();
    if (!m->edit){ gGuiadoOp=0; NotificarHintClear(); return true; }
    if (gGuiadoOp == 1){ // Select Linked: la isla conexa que contiene al elemento clickeado
        int k = EditPickIndex(EditSelectMode, mx,my,vx,vy,vw,vh,screenH);
        if (k < 0) return true; // click al vacio: sigue esperando
        UndoCapturarSeleccionEdit(m);
        m->edit->SeleccionarLinked(k, EditSelectMode, !LShiftPressed);
    } else { // Loop Select en la posicion clickeada (cara->Face Loop, borde->Edge Loop; el cursor da la direccion)
        if (!LayoutLoopSelectEnPos(mx,my,vx,vy,vw,vh,screenH)) return true; // click al vacio: sigue esperando
    }
    gGuiadoOp = 0; NotificarHintClear(); g_redraw = true;
    return true;
}

// ====================================================================
//  LOOP CUT AND SLIDE (Ctrl+R) — herramienta modal
// ====================================================================
//  Fase PREVIEW: hover -> dibuja el corte que se generaria (gLoopCutSegs); rueda = mas
//  cortes. Click izq -> aplica el corte (snapshot) y entra en SLIDE: mover el mouse
//  ajusta el factor (re-corta desde el snapshot). Click izq confirma; click der deja el
//  factor en 0 (centro) y confirma. Al confirmar sale el panel redo (cortes + factor).
//  Esc cancela. Compartido 4 OS (cada plataforma rutea sus eventos).
static bool gLoopCutOn = false;
static bool gLoopCutSlide = false;
static int  gLoopCutCortes = 1;
static float gLoopCutFactor = 0.0f; // -1..1 (0 = centro)
static int  gLoopCutEdge = -1;      // arista (edit mesh) bajo el mouse = start del loop
static int  gLoopCutSlideX0 = 0;    // X del mouse al empezar el slide
static int  gLoopCutSlideW = 800;   // ancho del viewport (escala del slide)
static std::vector<float> gLoopCutSegs; // preview (segmentos locales) -> lo dibuja el viewport
bool gLoopCutEsPunto = false; // true = el corte es de UN borde suelto (perfil): el preview son PUNTOS, no lineas.
                              // Lo setea Mesh::LoopCutPreview (MeshEdit) segun si hay anillo de quads o no.
// fase ORIENTACION: solo al arrancar el loop cut desde un QUAD activo (menu). Antes de
// elegir los cortes, las flechas eligen cual de las 2 direcciones del quad cortar.
static bool gLoopCutOrientando = false;
static int  gLoopCutOrient = 0;           // 0 / 1
static int  gLoopCutOrientEdge[2] = {-1,-1}; // los 2 bordes perpendiculares del quad
bool LoopCutOrientando(){ return gLoopCutOrientando; }
// PICKER de direccion (click/tactil): en la fase de orientacion se dibujan los DOS cortes posibles
// (amarillo 0.5) + 4 PUNTOS grandes amarillos (centro de cada borde del quad). Tocar/clickear un punto
// elige la direccion de una. 2 puntos = una direccion, los otros 2 = la otra. gLoopCutSegs2 = el 2do corte.
static std::vector<float> gLoopCutSegs2;      // preview del corte de la OTRA direccion (solo en orientacion)
static float gLoopCutOrientPt[4][3];          // 4 midpoints (LOCAL) de los bordes del quad
static int   gLoopCutOrientPtDir[4] = {0,0,0,0}; // a que direccion (0/1) pertenece cada punto
static int   gLoopCutOrientNPts = 0;          // 4 cuando el picker esta activo
static void LoopCutAplicarYPanel(); // (def mas abajo) aplica el corte + abre el panel de edicion
// en tactil no hay rueda ni flechas -> el loop cut se ajusta desde el panel redo (cortes + slide)
#ifndef W3D_SYMBIAN
extern Uint32 g_lastFingerTicks; // controles.cpp: hubo input tactil en la sesion
static bool LoopCutTactil(){ return g_lastFingerTicks != 0; }
#else
static bool LoopCutTactil(){ return false; } // Symbian: va a teclado (flechas + Enter)
#endif

// snapshot de la geometria PRE-corte (para re-cortar en slide / redo)
struct LCSnap { std::vector<GLfloat> vp; std::vector<GLbyte> vn; std::vector<GLfloat> vu;
                std::vector<GLubyte> vc; int vsz; std::vector<MeshFace> f3d;
                std::vector<MaterialGroup> mg; std::vector<int> le, lv; bool tiene; }; // le/lv = looseEdges/looseVerts
static LCSnap gLCSnap;

static void LCGuardar(Mesh* m){
    gLCSnap.vsz = m->vertexSize;
    gLCSnap.vp.assign(m->vertex, m->vertex + m->vertexSize*3);
    if (m->normals)     gLCSnap.vn.assign(m->normals, m->normals + m->vertexSize*3); else gLCSnap.vn.clear();
    if (m->uv)          gLCSnap.vu.assign(m->uv, m->uv + m->vertexSize*2); else gLCSnap.vu.clear();
    if (m->vertexColor) gLCSnap.vc.assign(m->vertexColor, m->vertexColor + m->vertexSize*4); else gLCSnap.vc.clear();
    gLCSnap.f3d = m->faces3d; gLCSnap.mg = m->materialsGroup;
    gLCSnap.le = m->looseEdges; gLCSnap.lv = m->looseVerts; // sin esto el corte de un borde SUELTO (perfil) perdia las lineas al re-cortar
    gLCSnap.tiene = true;
}
static void LCRestaurar(Mesh* m){
    if (!gLCSnap.tiene) return;
    m->vertexSize = gLCSnap.vsz;
    delete[] m->vertex; m->vertex = new GLfloat[gLCSnap.vsz*3];
    for (int i=0;i<gLCSnap.vsz*3;i++) m->vertex[i]=gLCSnap.vp[i];
    if (!gLCSnap.vn.empty()){ delete[] m->normals; m->normals=new GLbyte[gLCSnap.vsz*3]; for (int i=0;i<gLCSnap.vsz*3;i++) m->normals[i]=gLCSnap.vn[i]; }
    if (!gLCSnap.vu.empty()){ delete[] m->uv; m->uv=new GLfloat[gLCSnap.vsz*2]; for (int i=0;i<gLCSnap.vsz*2;i++) m->uv[i]=gLCSnap.vu[i]; }
    if (!gLCSnap.vc.empty()){ delete[] m->vertexColor; m->vertexColor=new GLubyte[gLCSnap.vsz*4]; for (int i=0;i<gLCSnap.vsz*4;i++) m->vertexColor[i]=gLCSnap.vc[i]; }
    m->faces3d = gLCSnap.f3d; m->materialsGroup = gLCSnap.mg;
    m->looseEdges = gLCSnap.le; m->looseVerts = gLCSnap.lv; // restaurar el perfil suelto (sino desaparece al re-cortar)
    std::vector<GLushort> tris;
    for (size_t f=0;f<m->faces3d.size();f++){ const std::vector<int>& idx=m->faces3d[f].idx;
        for (size_t k=1;k+1<idx.size();k++){ tris.push_back((GLushort)idx[0]);tris.push_back((GLushort)idx[k]);tris.push_back((GLushort)idx[k+1]); } }
    m->facesSize=(int)tris.size(); delete[] m->faces; m->faces=new MeshIndex[m->facesSize>0?m->facesSize:1];
    for (int i=0;i<m->facesSize;i++) m->faces[i]=tris[i];
    m->CalcularBordes(); m->RecalcularNormales();
}

bool LoopCutActivo(){ return gLoopCutOn; }

static void LoopCutActualizarPreview(int mx, int my){
    gLoopCutSegs.clear(); gLoopCutEdge = -1;
    if (!Viewport3DActive || !g_editMesh) return;
    Mesh* m=(Mesh*)g_editMesh; m->EnsureEdit(); if (!m->edit) return;
    int eg = EditPickIndex(SelEdge, mx, my, Viewport3DActive->x, Viewport3DActive->y,
                           Viewport3DActive->width, Viewport3DActive->height, W3dPantallaAlto);
    gLoopCutEdge = eg;
    if (eg>=0) m->LoopCutPreview(eg, gLoopCutCortes, 0.0f, gLoopCutSegs);
}

// cartel-tutorial del loop cut, segun la fase (orientacion / cortes / slide). Se cierra al confirmar/cancelar.
static void LoopCutHint(){
    if (!gLoopCutOn){ NotificarHintClear(); return; }
    if (gLoopCutOrientando) NotificarHint("Loop Cut: tap a yellow dot to pick the cut direction (or arrows + Enter)");
    else if (gLoopCutSlide) NotificarHint("Loop Cut: move = slide, Enter to confirm, Esc = center");
    else                    NotificarHint("Loop Cut: scroll = number of cuts, Enter to confirm");
}

void LoopCutIniciar(int mx, int my){
    if (InteractionMode!=EditMode || !g_editMesh){ Notificar("Loop Cut: enter Edit Mode first", true); return; }
    gLoopCutOn=true; gLoopCutSlide=false; gLoopCutOrientando=false; gLoopCutCortes=1; gLoopCutFactor=0.0f; gLCSnap.tiene=false;
    LoopCutActualizarPreview(mx,my);
    g_redraw=true;
    LoopCutHint();
}

// setup del loop cut sobre un BORDE conocido (direccion automatica). En tactil aplica+panel;
// en PC/Symbian sigue el flujo clasico (rueda = cortes, click aplica + slide).
static void LoopCutIniciarBorde(Mesh* m, int edgeIdx){
    gLoopCutOn=true; gLoopCutSlide=false; gLoopCutOrientando=false;
    gLoopCutCortes=1; gLoopCutFactor=0.0f; gLCSnap.tiene=false;
    gLoopCutEdge = edgeIdx;
    gLoopCutSegs.clear(); m->LoopCutPreview(gLoopCutEdge, gLoopCutCortes, 0.0f, gLoopCutSegs);
    gLoopCutSegs2.clear(); gLoopCutOrientNPts=0;
    g_redraw=true;
    if (LoopCutTactil()){ LoopCutAplicarYPanel(); return; }
    LoopCutHint();
}

// setup del loop cut sobre un QUAD conocido: fase ORIENTACION (2 cortes en 0.5 + 4 puntos para
// elegir direccion con un click/toque; en PC/Symbian tambien flechas + Enter).
static void LoopCutIniciarQuad(Mesh* m, EditMesh* e, int faceIdx){
    gLoopCutOn=true; gLoopCutSlide=false; gLoopCutOrientando=true;
    gLoopCutCortes=1; gLoopCutFactor=0.0f; gLCSnap.tiene=false;
    gLoopCutOrient=0;
    gLoopCutOrientEdge[0] = e->faceEdges[faceIdx][0]; // las 2 direcciones perpendiculares
    gLoopCutOrientEdge[1] = e->faceEdges[faceIdx][1];
    gLoopCutEdge = gLoopCutOrientEdge[0];
    // preview de LAS DOS direcciones (se dibujan ambas en 0.5)
    gLoopCutSegs.clear();  m->LoopCutPreview(gLoopCutOrientEdge[0], gLoopCutCortes, 0.0f, gLoopCutSegs);
    gLoopCutSegs2.clear(); m->LoopCutPreview(gLoopCutOrientEdge[1], gLoopCutCortes, 0.0f, gLoopCutSegs2);
    // 4 PUNTOS = midpoint de cada borde del quad. Bordes 0,2 -> direccion 0; bordes 1,3 -> direccion 1.
    gLoopCutOrientNPts = 0;
    for (int k=0; k<4 && k<(int)e->faceEdges[faceIdx].size(); k++){
        int eg = e->faceEdges[faceIdx][k];
        if (eg<0 || eg*2+1 >= (int)e->lineIdx.size()) continue;
        int a = e->lineIdx[eg*2], b = e->lineIdx[eg*2+1];
        int n = gLoopCutOrientNPts;
        gLoopCutOrientPt[n][0] = (e->pos[a*3+0]+e->pos[b*3+0])*0.5f;
        gLoopCutOrientPt[n][1] = (e->pos[a*3+1]+e->pos[b*3+1])*0.5f;
        gLoopCutOrientPt[n][2] = (e->pos[a*3+2]+e->pos[b*3+2])*0.5f;
        gLoopCutOrientPtDir[n] = (k % 2 == 0) ? 0 : 1; // bordes opuestos 0/2 y 1/3
        gLoopCutOrientNPts++;
    }
    g_redraw=true;
    LoopCutHint();
}

// Loop Cut desde el menu Edge/Face/Vertex, sobre el elemento ACTIVO / la SELECCION (sin hover):
//  - modo BORDE: el borde activo -> el corte cruza ese borde directo (direccion obvia).
//  - modo CARA:  el quad activo  -> fase ORIENTACION (elegir 1 de las 2 direcciones). SOLO quads.
//  - modo VERTICE: por la seleccion. 2 verts = un borde (direccion obvia); 4 verts = un quad
//                  (elegir direccion). Cualquier otra cantidad (3, sueltos, etc.) = invalido.
void LayoutLoopCutDesdeActivo(){
    if (InteractionMode!=EditMode || !g_editMesh){ Notificar("Loop Cut: enter Edit Mode first", true); return; }
    Mesh* m=(Mesh*)g_editMesh; m->EnsureEdit(); if (!m->edit) return;
    EditMesh* e = m->edit;
    int act = e->activeIdx;
    if (EditSelectMode == SelEdge){
        if (act < 0 || act >= e->NumEdges()){ Notificar("Loop Cut: no active edge (click an edge first)", true); return; }
        LoopCutIniciarBorde(m, act);
    } else if (EditSelectMode == SelFace){
        if (act < 0 || act >= (int)e->faces.size()){ Notificar("Loop Cut: no active face (click a face first)", true); return; }
        if (e->faces[act].size() != 4){ Notificar("Loop Cut: the active face is not a quad", true); return; } // SOLO quads
        if (act >= (int)e->faceEdges.size() || e->faceEdges[act].size() < 2){ Notificar("Loop Cut: the active face has no edges", true); return; }
        LoopCutIniciarQuad(m, e, act);
    } else { // SelVertex: decide por la SELECCION
        std::vector<int> sel;
        for (int k=0; k<(int)e->vertSel.size(); k++) if (e->vertSel[k]) sel.push_back(k);
        if (sel.size() == 2){
            // 2 verts: el borde que los une -> direccion obvia
            int eg = -1;
            for (int i=0; i<e->NumEdges(); i++){
                int a=e->lineIdx[i*2], b=e->lineIdx[i*2+1];
                if ((a==sel[0]&&b==sel[1]) || (a==sel[1]&&b==sel[0])){ eg=i; break; }
            }
            if (eg<0){ Notificar("Loop Cut: the 2 selected vertices are not an edge", true); return; }
            LoopCutIniciarBorde(m, eg);
        } else if (sel.size() == 4){
            // 4 verts: la cara (quad) cuyos 4 vertices son EXACTAMENTE los seleccionados -> elegir direccion
            int f = -1;
            for (int i=0; i<(int)e->faces.size(); i++){
                if (e->faces[i].size()!=4) continue;
                bool todos=true;
                for (int j=0;j<4;j++){ if (!e->vertSel[e->faces[i][j]]){ todos=false; break; } }
                if (todos){ f=i; break; }
            }
            if (f<0 || f>=(int)e->faceEdges.size() || e->faceEdges[f].size()<2){ Notificar("Loop Cut: select the 4 vertices of one quad", true); return; }
            LoopCutIniciarQuad(m, e, f);
        } else {
            Notificar("Loop Cut: select an edge (2 verts) or a quad (4 verts)", true); // 3 o algo raro = invalido
        }
    }
}

static void LoopCutConfirmar(){
    gLoopCutOn=false; gLoopCutSlide=false; gLoopCutSegs.clear(); gLoopCutSegs2.clear(); gLoopCutOrientNPts=0;
    NotificarHintClear();
    AbrirRedoLoopCutPanel((Mesh*)g_editMesh); // panel redo (cortes + factor); usa el snapshot
    g_redraw=true;
}

// aplica el corte con los defaults (1 corte, factor 0) y abre el panel redo para editar cortes/slide.
// Es el camino de UN CLICK/TOUCH: como no hay flechas ni rueda en tactil, se ajusta desde el panel.
static void LoopCutAplicarYPanel(){
    if (gLoopCutEdge<0 || !g_editMesh){ LoopCutCancelar(); return; }
    Mesh* m=(Mesh*)g_editMesh;
    UndoCapturarMallaGeo(m);           // Ctrl+Z: 1 sola captura, PRE-corte
    LCGuardar(m);
    m->LoopCutEdit(gLoopCutEdge, gLoopCutCortes, 0.0f);
    LoopCutConfirmar();                // cierra el modal + abre el panel (cortes + factor)
}

// ENTER en la fase de orientacion (teclado PC/Symbian): confirma la direccion elegida con las
// flechas y sigue al flujo clasico (preview con rueda -> click aplica+slide). El PICKER de puntos
// (click/tactil) NO pasa por aca: ese va directo a aplicar + panel.
void LoopCutOrientConfirmarTeclado(){
    if (!gLoopCutOrientando) return;
    gLoopCutOrientando=false; gLoopCutOrientNPts=0; gLoopCutSegs2.clear();
    gLoopCutSegs.clear();
    if (g_editMesh) ((Mesh*)g_editMesh)->LoopCutPreview(gLoopCutEdge, gLoopCutCortes, 0.0f, gLoopCutSegs);
    g_redraw=true;
    LoopCutHint(); // ayuda de la fase de cortes
}

void LoopCutMotion(int mx, int my){
    if (!gLoopCutOn) return;
    if (gLoopCutOrientando) return; // orientacion: los 2 previews son fijos (no re-pickear por hover)
    if (!gLoopCutSlide){ LoopCutActualizarPreview(mx,my); g_redraw=true; }
    else {
        float f = (float)(mx - gLoopCutSlideX0) / (float)(gLoopCutSlideW*0.4f);
        if (f>1.0f) f=1.0f; if (f<-1.0f) f=-1.0f;
        gLoopCutFactor=f;
        Mesh* m=(Mesh*)g_editMesh; LCRestaurar(m); m->LoopCutEdit(gLoopCutEdge, gLoopCutCortes, f);
        g_redraw=true;
    }
}

void LoopCutWheel(int dir){
    if (!gLoopCutOn || gLoopCutSlide || gLoopCutOrientando) return; // rueda = cortes (fase preview)
    gLoopCutCortes += dir; if (gLoopCutCortes<1) gLoopCutCortes=1; if (gLoopCutCortes>32) gLoopCutCortes=32;
    gLoopCutSegs.clear();
    if (gLoopCutEdge>=0 && g_editMesh) ((Mesh*)g_editMesh)->LoopCutPreview(gLoopCutEdge, gLoopCutCortes, 0.0f, gLoopCutSegs);
    g_redraw=true;
}

// FLECHAS durante el modal (PC y Symbian). dir: 0=izq 1=der 2=arriba 3=abajo.
//  orientacion (quad): izq/arriba = direccion 1, der/abajo = direccion 2.
//  cortes (preview):   arriba/derecha = +1, abajo/izquierda = -1.
//  slide:              izquierda/derecha mueve el factor.
void LoopCutTecla(int dir){
    if (!gLoopCutOn || !g_editMesh) return;
    Mesh* m=(Mesh*)g_editMesh;
    if (gLoopCutOrientando){
        gLoopCutOrient = (dir==1 || dir==3) ? 1 : 0;
        gLoopCutEdge = gLoopCutOrientEdge[gLoopCutOrient];
        gLoopCutSegs.clear(); m->LoopCutPreview(gLoopCutEdge, gLoopCutCortes, 0.0f, gLoopCutSegs);
        g_redraw=true;
        return;
    }
    if (!gLoopCutSlide){
        LoopCutWheel((dir==2 || dir==1) ? +1 : -1); // arriba/derecha +1 corte; abajo/izq -1
        return;
    }
    float paso = 0.08f; // slide con flechas: izquierda/derecha
    if (dir==0) gLoopCutFactor -= paso;
    else if (dir==1) gLoopCutFactor += paso;
    else return;
    if (gLoopCutFactor>1.0f) gLoopCutFactor=1.0f;
    if (gLoopCutFactor<-1.0f) gLoopCutFactor=-1.0f;
    LCRestaurar(m); m->LoopCutEdit(gLoopCutEdge, gLoopCutCortes, gLoopCutFactor);
    g_redraw=true;
}

// click IZQUIERDO durante el loop cut: en preview aplica+entra al slide; en slide confirma
void LoopCutClickIzq(int mx, int my){
    if (!gLoopCutOn) return;
    if (gLoopCutOrientando){
        // PICKER: elegir la direccion tocando/clickeando el PUNTO mas cercano (uno de los 4
        // midpoints del quad). Con un solo click sacamos la direccion; despues se aplica el
        // corte y se abre el panel (cortes + slide) porque en tactil no hay flechas ni rueda.
        int best = -1; float bestD = 1e18f;
        if (Viewport3DActive && g_editMesh && gLoopCutOrientNPts > 0){
            Mesh* m=(Mesh*)g_editMesh; Matrix4 W; m->GetWorldMatrix(W);
            float lmx = (float)mx - (float)Viewport3DActive->x;
            float lmy = (float)my - (float)Viewport3DActive->y;
            for (int i=0;i<gLoopCutOrientNPts;i++){
                Vector3 wp = W * Vector3(gLoopCutOrientPt[i][0], gLoopCutOrientPt[i][1], gLoopCutOrientPt[i][2]);
                float sx=0, sy=0;
                if (!Viewport3DActive->ProyectarPunto(wp, sx, sy)) continue;
                float dx=sx-lmx, dy=sy-lmy; float d=dx*dx+dy*dy;
                if (d < bestD){ bestD = d; best = i; }
            }
        }
        if (best >= 0){
            gLoopCutOrient = gLoopCutOrientPtDir[best];
            gLoopCutEdge   = gLoopCutOrientEdge[gLoopCutOrient];
            gLoopCutOrientando=false;
            LoopCutAplicarYPanel();   // aplica + abre el panel de edicion (cortes/slide)
        } else {
            // sin puntos proyectables: fallback al confirm clasico (sigue al preview)
            LoopCutOrientConfirmarTeclado();
        }
        g_redraw=true;
        return;
    }
    if (!gLoopCutSlide){
        if (gLoopCutEdge<0 || !g_editMesh){ LoopCutCancelar(); return; }
        Mesh* m=(Mesh*)g_editMesh;
        UndoCapturarMallaGeo(m); // Ctrl+Z: 1 sola captura, PRE-corte (el slide/redo re-cortan sin recapturar)
        LCGuardar(m);
        m->LoopCutEdit(gLoopCutEdge, gLoopCutCortes, 0.0f);
        gLoopCutSlide=true; gLoopCutFactor=0.0f; gLoopCutSlideX0=mx;
        gLoopCutSlideW = Viewport3DActive ? Viewport3DActive->width : 800;
        gLoopCutSegs.clear();
        g_redraw=true;
        LoopCutHint();            // ayuda de la fase de slide
    } else LoopCutConfirmar();
}

// click DERECHO: en slide deja el factor en 0 (centro) + confirma; en preview cancela
void LoopCutClickDer(){
    if (!gLoopCutOn) return;
    if (!gLoopCutSlide){ LoopCutCancelar(); return; }
    gLoopCutFactor=0.0f;
    Mesh* m=(Mesh*)g_editMesh; LCRestaurar(m); m->LoopCutEdit(gLoopCutEdge, gLoopCutCortes, 0.0f);
    LoopCutConfirmar();
}

void LoopCutCancelar(){
    if (!gLoopCutOn) return;
    if (gLoopCutSlide && gLCSnap.tiene){ LCRestaurar((Mesh*)g_editMesh); gLCSnap.tiene=false; }
    gLoopCutOn=false; gLoopCutSlide=false; gLoopCutSegs.clear();
    NotificarHintClear();
    g_redraw=true;
}

// la usa el panel redo: re-corta desde el snapshot con nuevos parametros
void LoopCutRedoAplicar(int cortes, float factor){
    if (!gLCSnap.tiene || gLoopCutEdge<0 || !g_editMesh) return;
    if (cortes<1) cortes=1;
    gLoopCutCortes=cortes; gLoopCutFactor=factor;
    Mesh* m=(Mesh*)g_editMesh; LCRestaurar(m); m->LoopCutEdit(gLoopCutEdge, cortes, factor);
    g_redraw=true;
}
int   LoopCutGetCortes(){ return gLoopCutCortes; }
float LoopCutGetFactor(){ return gLoopCutFactor; }

// dibuja el preview del corte (segmentos locales) con la matriz de mundo del objeto.
// Lo llama el viewport 3D despues de renderizar la escena.
void LoopCutRenderPreview(){
    if (!gLoopCutOn || !g_editMesh) return;
    if (gLoopCutSegs.empty() && !gLoopCutOrientando) return;
    Mesh* m=(Mesh*)g_editMesh;
    Matrix4 W; m->GetWorldMatrix(W);
    w3dEngine::PushMatrix(); w3dEngine::MultMatrix(W.m);
    w3dEngine::Disable(w3dEngine::Lighting); w3dEngine::Disable(w3dEngine::Texture2D); w3dEngine::Disable(w3dEngine::DepthTest);
    w3dEngine::DisableArray(w3dEngine::NormalArray); w3dEngine::DisableArray(w3dEngine::ColorArray); w3dEngine::DisableArray(w3dEngine::TexCoordArray);
    w3dEngine::EnableArray(w3dEngine::VertexArray);

    if (gLoopCutOrientando){
        // FASE ORIENTACION (quad): los DOS cortes posibles en amarillo 0.5 + 4 PUNTOS grandes 100% opacos.
        w3dEngine::Color4f(1.0f, 0.95f, 0.2f, 0.5f); // amarillo semitransparente
        w3dEngine::LineWidth(3.0f);
        if (!gLoopCutSegs.empty()){  w3dEngine::VertexPointer3f(0, &gLoopCutSegs[0]);  w3dEngine::DrawLines((int)(gLoopCutSegs.size()/3)); }
        if (!gLoopCutSegs2.empty()){ w3dEngine::VertexPointer3f(0, &gLoopCutSegs2[0]); w3dEngine::DrawLines((int)(gLoopCutSegs2.size()/3)); }
        w3dEngine::LineWidth(1.0f);
        if (gLoopCutOrientNPts > 0){ // los puntos que se tocan para elegir la direccion
            w3dEngine::Color4f(1.0f, 0.95f, 0.2f, 1.0f); // amarillo opaco
            w3dEngine::PointSize(22.0f);
            w3dEngine::VertexPointer3f(0, &gLoopCutOrientPt[0][0]);
            w3dEngine::DrawPoints(gLoopCutOrientNPts);
            w3dEngine::PointSize(1.0f);
        }
    } else {
        w3dEngine::Color4f(1.0f, 0.95f, 0.2f, 1.0f); // amarillo (como Blender)
        w3dEngine::VertexPointer3f(0, &gLoopCutSegs[0]);
        if (gLoopCutEsPunto){ // corte de un BORDE SUELTO: el corte es un PUNTO en la arista, no una linea
            w3dEngine::PointSize(9.0f);
            w3dEngine::DrawPoints((int)(gLoopCutSegs.size()/3));
            w3dEngine::PointSize(1.0f);
        } else {
            w3dEngine::LineWidth(3.0f);
            w3dEngine::DrawLines((int)(gLoopCutSegs.size()/3));
            w3dEngine::LineWidth(1.0f);
        }
    }
    w3dEngine::Enable(w3dEngine::DepthTest);
    w3dEngine::PopMatrix();
}

bool ScenePick3D(int mx, int my, int vx, int vy, int vw, int vh, int screenH) {
    if (!SceneCollection) return false;
    // Ctrl+Z: en Object Mode el click va a cambiar la seleccion -> guardar la previa.
    // (la seleccion de sub-elementos en Edit Mode es 2da tanda)
    if (InteractionMode != EditMode) UndoCapturarSeleccion();
    // en Edit Mode el click selecciona sub-elementos (no objetos)
    if (InteractionMode == EditMode && g_editMesh) {
        UndoCapturarSeleccionEdit((Mesh*)g_editMesh); // Ctrl+Z: el click va a cambiar la seleccion de sub-elementos
        // modo guiado "Pick Shortest Path" (desde el menu): el click elige 1ro/2do elemento
        if (PickPathClick(mx, my, vx, vy, vw, vh, screenH)) return true;
        if (GuiadoUnClickClick(mx, my, vx, vy, vw, vh, screenH)) return true; // Select Linked / Loop Select guiados (1 click)
        // Loop Select por Alt+Click (el borde de referencia es el que esta bajo el cursor):
        //   Shift+Alt+Click -> en CUALQUIER modo (CARA=Face Loop, BORDE/VERTICE=Edge Loop). El Shift ademas AGREGA.
        //   Ctrl+Alt+Click (modo BORDE) -> Edge Loop tambien (compat con el atajo viejo; sin shift = reemplaza).
        // (Fix Dante: antes Shift+Alt solo andaba en modo cara; en borde/vertice no seleccionaba el loop.)
        if (LAltPressed && (LShiftPressed || (LCtrlPressed && EditSelectMode == SelEdge))) {
            Mesh* m = (Mesh*)g_editMesh; m->EnsureEdit();
            if (m->edit) {
                int eg = EditPickIndex(SelEdge, mx, my, vx, vy, vw, vh, screenH); // siempre pickea un BORDE de referencia
                if (eg >= 0) {
                    bool soloEste = !LShiftPressed; // Shift = agrega a la seleccion; sino reemplaza
                    if (EditSelectMode == SelFace)      m->edit->SeleccionarLoopFace(eg, soloEste);
                    else if (EditSelectMode == SelEdge) m->edit->SeleccionarLoopEdge(eg, soloEste);
                    else                                m->edit->SeleccionarLoopEdgeVerts(eg, true); // VERTICE: REEMPLAZA (Shift es el atajo, no un "add"; ademas no acumula el edgeSel)
                }
            }
            return true;
        }
        // Ctrl+Click (sin Alt) = Pick Shortest Path desde el ACTIVO hasta el clickeado.
        // +Shift = Fill Region (rellena la region en vez de un solo caminito). Agrega.
        if (LCtrlPressed && !LAltPressed) {
            Mesh* m = (Mesh*)g_editMesh; m->EnsureEdit();
            if (m->edit) {
                int k = EditPickIndex(EditSelectMode, mx, my, vx, vy, vw, vh, screenH);
                if (k >= 0) m->edit->SeleccionarShortestPath(k, LShiftPressed);
            }
            return true;
        }
        return EditPickVert(mx, my, vx, vy, vw, vh, screenH);
    }

    // misma proyeccion+camara que el Viewport3D REAL (el pick tiene que
    // ver EXACTAMENTE lo que se dibuja)
    {
        int glY = screenH - vy - vh;
        w3dEngine::Viewport(vx, glY, vw, vh);
        w3dEngine::Scissor(vx, glY, vw, vh);
        w3dEngine::Enable(w3dEngine::ScissorTest);
        w3dEngine::MatrixMode(w3dEngine::Projection);
        w3dEngine::LoadIdentity();
        float pkNear = Viewport3DActive ? Viewport3DActive->nearClip : 0.01f;
        float pkFar  = Viewport3DActive ? Viewport3DActive->farClip  : 1000.0f;
        float pkAspect = (float)vw / (float)vh;
        if (Viewport3DActive && Viewport3DActive->orthographic) {
            // orto: mismo volumen que Render() (sino el pick de objetos cae corrido al zoomear en ortografica)
            float size = Viewport3DActive->orbitDistance * tanf(fovDeg * 0.5f * 3.14159265f / 180.0f);
            if (size < 0.001f) size = 0.001f;
            w3dEngine::Ortho(-size * pkAspect, size * pkAspect, -size, size, pkNear, Viewport3DActive->orbitDistance + pkFar);
        } else {
            w3dEngine::Perspective(fovDeg, pkAspect, pkNear, pkFar);
        }
        w3dEngine::MatrixMode(w3dEngine::ModelView);
        w3dEngine::LoadIdentity();
        if (Viewport3DActive) Viewport3DActive->UpdateViewOrbit();
    }
    w3dEngine::Disable(w3dEngine::Dither);
    w3dEngine::Disable(w3dEngine::Lighting);
    w3dEngine::Disable(w3dEngine::Texture2D);
    w3dEngine::Disable(w3dEngine::Blend);
    w3dEngine::DisableArray(w3dEngine::ColorArray);
    w3dEngine::DisableArray(w3dEngine::NormalArray);
    w3dEngine::DisableArray(w3dEngine::TexCoordArray);
    w3dEngine::Enable(w3dEngine::DepthTest);
    w3dEngine::ClearColor(0, 0, 0, 1);
    w3dEngine::Clear(w3dEngine::ColorBuffer | w3dEngine::DepthBuffer);

    pickCounter = 0;
    PickPaint(SceneCollection);

    GLubyte pix[4] = {0, 0, 0, 0};
    w3dEngine::ReadPixelsRGBA(mx, screenH - 1 - my, 1, 1, pix);
    w3dEngine::Enable(w3dEngine::Dither);
    w3dEngine::Disable(w3dEngine::ScissorTest);

    w3dEngine::EnableArray(w3dEngine::TexCoordArray); // baseline de la UI

    int id = ((pix[1] >> 2) << 5) | (pix[0] >> 3);

    if (!LShiftPressed) {
        DeseleccionarTodo();
    }
    if (id > 0) {
        pickCounter = 0;
        pickFound = NULL;
        PickResolve(SceneCollection, id);
        if (pickFound) {
            if (LShiftPressed && pickFound->select) {
                pickFound->select = false; // shift: sacar de la seleccion
            } else {
                pickFound->Seleccionar();
            }
        }
    }
    return true; // el frame siguiente redibuja la escena real
}
