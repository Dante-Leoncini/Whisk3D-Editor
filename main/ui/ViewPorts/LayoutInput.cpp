#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "W3dLang.h"   // T(): los textos salen en el idioma del sistema
#include "Undo.h" // Ctrl+Z: capturar modo / seleccion
#include "ViewPorts/PopUp/ConfirmarPopup.h" // AbrirConfirmarBorrado (popup de confirmar borrado)
#include "ViewPorts/LayoutInput.h"
#include "ViewPorts/PoseTransform.h" // Pose Mode transform (extraido a su propio archivo)
#include "ViewPorts/Notificaciones.h" // toasts (extraido a su propio archivo)
#include "ViewPorts/NumInput.h" // entrada numerica/formulas (extraido a su propio archivo)
#include "ViewPorts/Parent.h" // emparentar/desemparentar (extraido a su propio archivo)
#include "ViewPorts/Pick3D.h" // pick/seleccion 3D + loop cut (extraido a su propio archivo)
#include "ViewPorts/ViewPort3D.h"
#include "ViewPorts/Outliner.h"
#include "ViewPorts/Properties.h"
#include "ViewPorts/UVEditor.h"
#include "ViewPorts/Timeline.h"
#include "WhiskUI/draw/glesdraw.h"
#include "WhiskUI/draw/rectangle.h" // el velo del modo foco
#include "objects/Objects.h"
#include "objects/Mesh.h"
#include "objects/Materials.h" // Material (mat->texture) para el dropdown "Texture" del UV editor
#include "objects/Textures.h"  // Texture (path) para las etiquetas del dropdown
#include "objects/EditMesh.h"
#include "objects/Light.h"
#include "objects/Camera.h"
#include "objects/Empty.h"
#include "objects/Armature.h"
#include "animation/SkeletalAnimation.h" // InsertarKeyframeEsqueleto (Pose Mode: Insert Keyframe)
#include "objects/Instance.h"
#include "objects/Collection.h"
#include "objects/ObjectMode.h"
#include "edit/Modifier.h" // ModifierType::Mirror + target (regen de mirrors al mover objetos)
#include "objects/Primitivas.h"
#include "variables.h"
#include "render/OpcionesRender.h" // g_fpsActual
#include "ViewPorts/PopUp/PopUpBase.h"
#include "ViewPorts/PopUp/RedoMeshPanel.h"
#include "WhiskUI/widgets/card.h"        // tarjeta de las notificaciones
#include "WhiskUI/text/bitmapText.h"  // texto de las notificaciones
#include "WhiskUI/draw/icons.h"       // iconos notifOk / notifError
#include "WhiskUI/theme/colores.h"     // ColorID
#include "w3dlog.h"         // las notificaciones tambien van al log
// (los tipos GL + el dibujo vienen del engine: w3dGraphics.h / w3dEngine, ya incluido arriba)

void (*LayoutImportObj)() = NULL;
void (*LayoutImportFbx)() = NULL; // "Add > Imports > FBX": abre el explorador filtrado a .fbx (lo cablea la plataforma)
void (*LayoutImportGltf)() = NULL; // "Add > Imports > glTF": explorador filtrado a .gltf
void (*LayoutImportGlb)() = NULL;  // "Add > Imports > GLB": explorador filtrado a .glb
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
        case 4: return new Timeline();
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
    // acciones de layout: ids ALTOS para no chocar con los tipos de viewport (0..9 = tipos)
    if (aId == 23) { LayoutMaximizar(); return; }         // Maximize / Minimize (fullscreen del activo)
    if (aId == 20) { LayoutExpandir(vp); return; }
    if (aId == 21) { LayoutDividir(vp, true); return; }   // Split Row
    if (aId == 22) { LayoutDividir(vp, false); return; }  // Split Column
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
// ===================================================================================================
//  MENU ADD (declarativo). Cada item crea SU objeto y hace SU post-procesado; la tabla ADD[] de mas abajo los
//  lista con su texto e icono. No hay switch ni id magico: la accion vive en la propia fila.
// ===================================================================================================

// el tail comun a todo lo que crea un objeto: deseleccionar el resto, dejarlo elegido, y -si es una primitiva
// regenerable- abrir la ventanita "Add ..." con sus parametros. Las excepciones (Vertex/Reference) lo llaman y
// despues hacen lo suyo, o no lo llaman.
static void TrasCrearAdd(Object* nuevo){
    if (!nuevo) return;
    DeseleccionarTodo();
    nuevo->Seleccionar();
    if (nuevo->getType() == ObjectType::mesh && ((Mesh*)nuevo)->meshTipo >= 0)
        AbrirRedoMeshPanel((Mesh*)nuevo);
}

void AddPlane(){    TrasCrearAdd(NewMesh(MeshType::plane, NULL, false)); }
void AddCube(){     TrasCrearAdd(NewMesh(MeshType::cube, NULL, false)); }
void AddCircle(){   TrasCrearAdd(NewMesh(MeshType::circle, NULL, false)); }
void AddUVSphere(){ TrasCrearAdd(NewMesh(MeshType::UVsphere, NULL, false)); }
void AddCone(){     TrasCrearAdd(NewMesh(MeshType::cone, NULL, false)); }
void AddCylinder(){ TrasCrearAdd(NewMesh(MeshType::cylinder, NULL, false)); }
void AddEmpty(){    TrasCrearAdd(new Empty(NULL, cursor3D.pos)); }
void AddCamera(){   TrasCrearAdd(new Camera(NULL, cursor3D.pos, Vector3(-35.0f, -45.0f, 0.0f))); }

void AddLight(){
    Light* l = Light::Create(NULL, 0, 0, 0);
    // el nombre se pone ACA y no en el constructor: la Light vive en el Core, y el Core no sabe -ni tiene por que
    // saber- que existen los idiomas. El editor la crea, el editor la nombra.
    if (l){ l->pos = cursor3D.pos; l->name = T("Light"); }
    TrasCrearAdd(l);
}
void AddArmature(){
    // un solo hueso desde el origen, 0.3 hacia arriba (Y), en el cursor 3D.
    Armature* arm = new Armature(NULL, cursor3D.pos);
    W3dBone b; b.name = "Bone"; b.parent = -1;
    b.head = Vector3(0.0f, 0.0f, 0.0f); b.tail = Vector3(0.0f, 0.3f, 0.0f);
    arm->bones.push_back(b);
    TrasCrearAdd(arm);
}
void AddCollection(){
    TrasCrearAdd(new Collection(CollectionActive ? CollectionActive : SceneCollection));
}
// objetos LINKEADOS a un target (el activo, o NULL): renderizan a ese target una vez / N veces / espejado.
void AddDuplicateLinked(){
    Instance* inst = new Instance(NULL, ObjActivo); inst->pos = cursor3D.pos; TrasCrearAdd(inst);
}
void AddArray(){
    Instance* inst = new Instance(NULL, ObjActivo);
    inst->count = 3; inst->pos = Vector3(2, 0, 0); inst->name = "Array"; TrasCrearAdd(inst);
}
void AddMirror(){
    Instance* inst = new Instance(NULL, ObjActivo);
    inst->mirror = true; inst->mirrorEje = 0; inst->name = "Mirror"; TrasCrearAdd(inst);
}
void AddVertex(){
    // se crea y ya entras a EDITARLO, con el vertice elegido. Un vert suelto es lo unico que no se puede tocar
    // desde Object Mode (ni se ve): crearlo y tener que entrar a mano cada vez era un paso al pedo.
    Mesh* m = (Mesh*)NewMesh(MeshType::vertice, NULL, false);
    TrasCrearAdd(m);
    if (m){ if (InteractionMode != EditMode) LayoutToggleEditMode(); m->EditSeleccionarTodo(true); }
}
void AddReference(){
    // un plano PARADO (90 en X) = de frente en la vista, con su material y el selector de textura ya abierto:
    // imagen de referencia en un paso. NO abre el panel "Add Plane" (el selector ocupa la pantalla).
    Mesh* m = (Mesh*)NewMesh(MeshType::plane, NULL, false);
    if (!m) return;
    m->name = "Reference";
    m->SetRotEuler(Vector3(90.0f, 0.0f, 0.0f));
    DeseleccionarTodo(); m->Seleccionar();
    Material* mat = NuevoMaterialEnMeshPart(m, 0);
    // sin LIGHTING: una referencia es una imagen, no una superficie. Sombreada por la luz de la escena se ve mas
    // oscura de un lado y no es fiel a lo que estas calcando.
    if (mat){ mat->textureOn = true; mat->lighting = false; }
    if (mat && DialogoCargarTextura) DialogoCargarTextura(mat);
}
void AddImportObj(){  if (LayoutImportObj)  LayoutImportObj(); }
void AddImportFbx(){  if (LayoutImportFbx)  LayoutImportFbx(); }
void AddImportGltf(){ if (LayoutImportGltf) LayoutImportGltf(); }
void AddImportGlb(){  if (LayoutImportGlb)  LayoutImportGlb(); }

// submenu Imports (OBJ/FBX/glTF/GLB). Nombres de formato: NO se traducen (son marcas).
static const MenuDef ADD_IMPORTS[] = {
    { "OBJ",  AddImportObj,  NULL, ICONO(IconType::mesh) },
    { "FBX",  AddImportFbx,  NULL, ICONO(IconType::mesh) },
    { "glTF", AddImportGltf, NULL, ICONO(IconType::mesh) },
    { "GLB",  AddImportGlb,  NULL, ICONO(IconType::mesh) },
};

// EL MENU ADD, de una mirada: texto + accion + icono por fila. Agregar una primitiva = una linea aca, y ya queda
// conectada (no hay switch que actualizar ni id que inventar).
static const MenuDef ADD[] = {
    { "Plane",      AddPlane,           NULL, ICONO(IconType::plane) },
    { "Reference",  AddReference,       NULL, ICONO(IconType::textura) },
    { "Cube",       AddCube,            NULL, ICONO(IconType::object) },
    { "Circle",     AddCircle,          NULL, ICONO(IconType::circle) },
    { "UV Sphere",  AddUVSphere,        NULL, ICONO(IconType::circle) },
    { "Cone",       AddCone,            NULL, ICONO(IconType::cono) },
    { "Cylinder",   AddCylinder,        NULL, ICONO(IconType::cilindro) },
    { "Vertex",     AddVertex,          NULL, ICONO(IconType::mesh) },
    { "Empty",      AddEmpty,           NULL, ICONO(IconType::empty) },
    { "Armature",   AddArmature,        NULL, ICONO(IconType::armature) },
    { "Camera",     AddCamera,          NULL, ICONO(IconType::camera) },
    { "Light",      AddLight,           NULL, ICONO(IconType::light) },
    { "Collection", AddCollection,      NULL, ICONO(IconType::archive) },
    { "Imports",    NULL,               NULL, ICONO(IconType::mesh), &MenuImports },
};

// arma MenuAdd + su submenu Imports desde las tablas. Lo llama ViewPort3D al crear la barra.
void LayoutConstruirMenuAdd(){
    if (!MenuAdd || !MenuImports) return;
    MenuImports->Construir(ADD_IMPORTS, (int)(sizeof(ADD_IMPORTS)/sizeof(ADD_IMPORTS[0])));
    MenuAdd->Construir(ADD, (int)(sizeof(ADD)/sizeof(ADD[0])));
}
// opcion del menu Select: 0 All / 1 None / 2 Invert
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
    MenuSelect->Agregar(T("All"), 0)->atajo = "A";
    MenuSelect->Agregar(T("None"), 1)->atajo = "Alt A";
    MenuSelect->Agregar(T("Invert"), 2)->atajo = "Ctrl I";
    if (InteractionMode == EditMode) {
        // Select Linked (L): selecciona la ISLA conexa. Desde el menu = guiado (pide click sobre el elemento).
        MenuSelect->Agregar(T("Select Linked"), 15)->atajo = "L";
        if (EditSelectMode == SelEdge) {
            MenuSelect->Agregar(T("Loop Select (Edge Loop)"), 11)->atajo = "Shift Alt Click";
            MenuSelect->Agregar(T("Loop Select (Edge Ring)"), 12);
        } else if (EditSelectMode == SelFace) {
            MenuSelect->Agregar(T("Loop Select"), 10)->atajo = "Shift Alt Click";
        } else { // VERTICE: el loop se define por un BORDE -> modo guiado (pedi click sobre un borde)
            MenuSelect->Agregar(T("Loop Select (Edge Loop)"), 16)->atajo = "Shift Alt Click";
        }
        // Pick Shortest Path: en los 3 sub-modos. Guiado por cartel (click 1ro -> click 2do).
        MenuSelect->Agregar(T("Pick Shortest Path"), 13)->atajo = "Ctrl Click";
        MenuSelect->Agregar(T("Shortest Path (Fill Region)"), 14)->atajo = "Ctrl Shift Click";
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
SnapCfg g_snap = { false, SNAP_CLOSEST, SNAP_VERTEX, true,true,true, true,true,true, false,false };
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
        // en MODO OBJETO no se snapea a la propia seleccion que se mueve (todos los objetos seleccionados)
        if (InteractionMode==ObjectMode && m->select) continue;
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
    if (!m->RipSeleccionEdit()) { Notificar(T("Rip: the selection does not separate the mesh"), true); g_redraw = true; return; }
    Notificar(T("Rip: mesh separated"), false);
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

void LayoutAccionObject(int aId); // (definida mas abajo; la usa el menu UV + Parent.cpp)
static void LayoutAccionMesh(int aId);   // (def. mas abajo) accion del menu "Mesh" de Edit Mode
static void AccionMerge(int modo);       // (def. mas abajo) Merge de la seleccion (At Center/Cursor/Collapse/By Distance)

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
    if (!hay) { Notificar(T("Project from View: select faces first"), true); return; }
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
// El menu de barra ABIERTO y el ROL del boton que lo abrio. Se registran juntos en RegistrarMenuBarra(), que llama
// el UNICO lugar que abre estos menus. El rol sale del BOTON, asi que un menu nuevo NO SE PUEDE olvidar de
// registrarse: viene con su boton puesto.
//
// Antes esto era una lista paralela de "if (MenuAbierto == MenuX) rol = BR_X" que habia que acordarse de ampliar a
// mano con cada menu nuevo. Nadie se acordaba, y el sintoma era siempre el mismo: izq/der se clavaba en ese menu y
// no se podia salir. Paso con View, con Snap, con UV y con Animation -- cuatro veces el mismo bug.
static PopupMenu* gMenuBarraAbierto = NULL;
static int        gMenuBarraRol = -1;

// El par (menu, rol) va JUNTO: si despues se abre otro menu por otro camino, MenuAbierto ya no coincide con
// gMenuBarraAbierto y el rol se descarta en vez de aplicarse al menu equivocado.
static void RegistrarMenuBarra(PopupMenu* m, Button* b){
    gMenuBarraAbierto = m;
    gMenuBarraRol = b ? b->rol : -1;
}

static PopupMenu* gMenuSnapTool = NULL; // idem: LayoutCambiarMenuBarra lo necesita (izq/der para salir del menu Snap)
void LayoutMenuUV(int mx, int my) {
    if (InteractionMode != EditMode || !g_editMesh) return;
    if (!gMenuUVops) {
        gMenuUVops = new PopupMenu(); gMenuUVops->titulo = "UV"; gMenuUVops->action = LayoutAccionObject;
        gMenuUVops->Agregar(T("Unwrap"), 350)->atajo = "soon";
        gMenuUVops->Agregar(T("Smart UV Project"), 351)->atajo = "soon";
        gMenuUVops->Agregar(T("Follow Active Quads"), 352)->atajo = "soon";
        gMenuUVops->Agregar(T("Cube Projection"), 353);
        gMenuUVops->Agregar(T("Cylinder Projection"), 354);
        gMenuUVops->Agregar(T("Sphere Projection"), 355);
        gMenuUVops->Agregar(T("Project from View"), 356);
        gMenuUVops->Agregar(T("Project from View (Bounds)"), 357);
        gMenuUVops->Agregar(T("Mark Seam"), 358);
        gMenuUVops->Agregar(T("Clear Seam"), 359);
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
    if (m->TriangularSeleccionEdit()) { Notificar(T("Faces triangulated"), false); g_redraw = true; } // false = exito (verde)
    else Notificar(T("Select faces with more than 3 sides to triangulate"), true);                    // true = error (rojo)
}

static void AccionDelete(int aId); // ejecuta Delete Vertices/Edges/Faces/Edge Loops (ids 361-364); definida mas abajo

// opcion del menu Object/Mesh y su submenu Transform (ids 100-102, 300=extrude)
void LayoutAccionObject(int aId) {
    switch (aId) {
        case 1: DuplicatedObject(); break; // Duplicate Objects (Shift D)
        case 2: NewInstance();      break; // Duplicate Linked  (Alt D)
        case 3: AbrirConfirmarBorrado(); break; // Delete (X): popup de confirmacion -> Si borra (con undo)
        case 5: JoinObjetos(); break;           // Join (Ctrl J): une las mallas seleccionadas en el objeto activo
        case 510: InsertarKeyframeObjeto(); break; // Object > Animation: Insert Keyframe (pos/rot/escala en el frame actual)
        case 511: BorrarKeyframeObjeto();   break; // Object > Animation: Delete Keyframe (del frame actual)
        case 512: LimpiarKeyframeObjeto();  break; // Animation: Clear Keyframe (toda la animacion del objeto)
        case 513: g_redraw = true; break;          // Animation > Motion Trail: el checkbox ya lo toggleo PopupMenu
                                                   // (y a proposito NO cierra el menu)
        case 220: AplicarTransform(0); break;   // Apply Location
        case 221: AplicarTransform(1); break;   // Apply Rotation
        case 222: AplicarTransform(2); break;   // Apply Scale
        case 223: AplicarTransform(3); break;   // Apply All Transforms
        case 100: if (InteractionMode==PoseMode) PoseXformStart(1); else if (!EditXformStart(translacion, ViewAxis)) SetPosicion(); break; // Move  (G)
        case 101: if (InteractionMode==PoseMode) PoseXformStart(2); else if (!EditXformStart(rotacion,    ViewAxis)) SetRotacion(); break; // Rotate(R)
        case 102: if (InteractionMode==PoseMode) PoseXformStart(3); else if (!EditXformStart(EditScale,   XYZ))      SetEscala();   break; // Scale (S)
        case 500: PoseInsertKeyframe(); break; // Pose Mode: Insert Keyframe
        case 520: PoseClearTransform(0); break; // Pose Mode: Clear Transform > All (T+R+S de los huesos seleccionados)
        case 521: PoseClearTransform(1); break; // Clear Translation (Alt+G)
        case 522: PoseClearTransform(2); break; // Clear Rotation (Alt+R)
        case 523: PoseClearTransform(3); break; // Clear Scale (Alt+S)
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
        case 350: Notificar(T("Unwrap: not implemented yet"), false); break;              // UV > Unwrap (pendiente: LSCM)
        case 351: Notificar(T("Smart UV Project: not implemented yet"), false); break;    // UV > Smart UV Project (pendiente)
        case 352: Notificar(T("Follow Active Quads: not implemented yet"), false); break; // UV > Follow Active Quads (pendiente)
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
        case 421: LayoutLockOrbitToggle(); break; // Lock Orbit: orbitar -> panear
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
        // solo re-generar si hay un modificador que PRODUCE geometria (Mirror/Array/Subsurf/Screw/Boolean). Un stack de
        // SOLO Armature no genera malla (el skinning se aplica aparte en el render -> genValido queda false, que es lo
        // correcto: el render usa la malla base + skinVertex). Sin este filtro, GenerarMallaModificada dejaba
        // genValido=false y el retry se disparaba CADA FRAME (el 'modgen' subia sin parar al orbitar un FBX, pedido Dante).
        bool hayGeomMod = false;
        for (size_t k = 0; k < m->modificadores.size(); k++)
            if (m->modificadores[k]->tipo != ModifierType::Armature) { hayGeomMod = true; break; }
        if (hayGeomMod && !m->genValido) { m->GenerarMallaModificada(); g_redraw = true; }
        // SKINNING: sincronizar con el modificador Armature (target / "Display in viewport" / "Display in Edit Mode").
        // Barato (recorre modificadores); mantiene el skinning coherente con los flags y el modo actual.
        extern void SincronizarSkinConModificador(Mesh*);
        if (!m->modificadores.empty()) SincronizarSkinConModificador(m);
    }
}


// opcion del menu Mode: cambia el modo del objeto ACTIVO.
//  - MALLA:     Object/Edit/Paint (Edit y Paint todavia son placeholders).
//  - ARMATURE:  Object / Edit (placeholder) / Pose (posa el esqueleto).
static void LayoutAccionMode(int aId) {
    if (ObjActivo && ObjActivo->getType() == ObjectType::mesh)
        InteractionMode = aId;
    else if (ObjActivo && ObjActivo->getType() == ObjectType::armature)
        InteractionMode = (aId == PoseMode || aId == EditMode) ? aId : ObjectMode; // Object/Edit/Pose
    else
        InteractionMode = ObjectMode;
    ActualizarEditMeshActivo(); // refresca g_editMesh (PC + Symbian)
}

// REARMA el menu Mode segun el objeto activo (como LayoutRebuildMenuSelect):
//   malla -> Object/Edit/Vertex/Weight/Texture ; armature -> Object/Edit/Pose.
// Asi el mismo boton sirve para los dos tipos sin mostrar modos que no aplican.
static void LayoutRebuildMenuMode() {
    if (!MenuMode) return;
    MenuMode->Limpiar();
    bool esArm = (ObjActivo && ObjActivo->getType() == ObjectType::armature);
    MenuMode->Agregar(T("Object Mode"), ObjectMode, IconType::object);
    MenuMode->Agregar(T("Edit Mode"),   EditMode,   esArm ? IconType::armature : IconType::mesh);
    if (esArm) {
        MenuMode->Agregar(T("Pose Mode"), PoseMode, IconType::armature);
    } else {
        MenuMode->Agregar(T("Vertex Paint"),  VertexPaint,  IconType::mesh);
        MenuMode->Agregar(T("Weight Paint"),  WeightPaint,  IconType::mesh);
        MenuMode->Agregar(T("Texture Paint"), TexturePaint, IconType::mesh);
    }
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
        gMenuTipo->Agregar(T("Minimize"), 23);
    } else {
        gMenuTipo->Agregar(T("3D Viewport"), 0);
        gMenuTipo->Agregar("Outliner", 1);
        gMenuTipo->Agregar(T("Properties"), 2);
        gMenuTipo->Agregar(T("UV Editor"), 3);
        gMenuTipo->Agregar(T("Timeline"), 4);
        if (aVp != rootViewport) gMenuTipo->Agregar(T("Expand"), 20);
        gMenuTipo->Agregar(T("Split Row"), 21);
        gMenuTipo->Agregar(T("Split Column"), 22);
        if (aVp != rootViewport) gMenuTipo->Agregar(T("Maximize"), 23); // fullscreen del viewport activo
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
    // REGLA DE DISENO de los titulos: un menu que se abre desde algo SIN TEXTO (un icono, o un atajo de
    // teclado) lleva titulo -- es lo unico que te dice que estas mirando. Si lo abre un boton/item que YA
    // decia el texto, NO lleva: repetirlo es ruido. El boton View ahora es un icono -> titulo.
    if (!gMenuUV){ gMenuUV = new PopupMenu(); gMenuUV->titulo = T("View"); }
    gMenuUV->Limpiar();
    // los togglea el propio item (AgregarCheck sobre el bool* del UV editor)
    gMenuUV->AgregarCheck(T("Sync Selection"), 0, &uv->syncSelection);
    gMenuUV->AgregarCheck(T("Repeat Texture"), 1, &uv->repeatTexture);
    gMenuUV->AgregarCheck(T("Show Chrome UV"), 2, &uv->mostrarChromeUV); // overlay LIVE del reflejo equirect (demo)
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
    gMenuUVSnap->Agregar(T("Cursor to Selection"), 0);
    gMenuUVSnap->Agregar(T("Selection to Cursor"), 1);
    gMenuUVSnap->Agregar(T("Cursor to Center"), 2);
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
    gMenuUVSelMode->titulo = T("Select Mode");
    int cur = uv->ModoUV();
    gMenuUVSelMode->Agregar(T("Vertex"), SelVertex, (int)IconType::selVertex)->verde = (cur == SelVertex);
    gMenuUVSelMode->Agregar(T("Edge"),   SelEdge,   (int)IconType::selEdge)->verde   = (cur == SelEdge);
    gMenuUVSelMode->Agregar(T("Face"),   SelFace,   (int)IconType::selFace)->verde   = (cur == SelFace);
    gMenuUVSelMode->action = LayoutAccionUVSelMode;
    if (MenuAbierto && MenuAbierto != gMenuUVSelMode) MenuAbierto->Cerrar();
    gMenuUVSelMode->Abrir(x, y, MenuPantallaW, MenuPantallaH);
    MenuAbierto = gMenuUVSelMode;
}

// click en la barra del UV editor: [1]=View, [2]=SelMode (propio), [3]=Pivot (= menu del 3D), [4]=Snap
// dropdown "Texture" del UV editor (boton [5] de la barra): elegir a mano que textura/parte del modelo ver.
static UVEditor*  gUVTexTarget = NULL;
static Mesh*      gUVTexMesh   = NULL;
static PopupMenu* gMenuUVTex   = NULL;
static void LayoutAccionUVTex(int id) {
    if (!gUVTexMesh) return;
    if (id >= 9000) UVSetTexOverride(gUVTexMesh, -1);   // "Auto": vuelve a seguir la parte activa
    else            UVSetTexOverride(gUVTexMesh, id);   // ver a mano la parte (material) 'id'
    g_redraw = true;
}
static void LayoutAbrirMenuUVTex(UVEditor* uv, int x, int y) {
    if (!uv) return;
    Mesh* m = (ObjActivo && ObjActivo->getType() == ObjectType::mesh) ? (Mesh*)ObjActivo : NULL;
    if (!m || m->materialsGroup.empty()) return;
    gUVTexTarget = uv; gUVTexMesh = m;
    if (!gMenuUVTex) gMenuUVTex = new PopupMenu();
    gMenuUVTex->Limpiar();
    gMenuUVTex->titulo = T("Texture");
    gMenuUVTex->Agregar(T("Auto (active part)"), 9000);
    // lista las TEXTURAS DISTINTAS del modelo (dedup por puntero). El id de cada opcion = la parte que la usa, asi el
    // UV editor muestra esa textura + las UV de esa parte. (Dante: "el dropdown es de texturas, no de materiales".)
    std::vector<Texture*> vistas;
    for (size_t i = 0; i < m->materialsGroup.size(); i++) {
        Material* mm = m->materialsGroup[i].material;
        Texture* t = mm ? mm->texture : NULL;
        if (!t) continue;
        bool dup = false; for (size_t k = 0; k < vistas.size(); k++) if (vistas[k] == t) { dup = true; break; }
        if (dup) continue;
        vistas.push_back(t);
        std::string lbl; char buf[24]; sprintf(buf, "Texture %d", (int)vistas.size());
        if (!t->path.empty()){ size_t sl = t->path.find_last_of("/\\"); lbl = (sl==std::string::npos) ? t->path : t->path.substr(sl+1); }
        else lbl = buf;
        gMenuUVTex->Agregar(lbl, (int)i); // id = parte que usa esta textura
    }
    gMenuUVTex->action = LayoutAccionUVTex;
    if (MenuAbierto && MenuAbierto != gMenuUVTex) MenuAbierto->Cerrar();
    gMenuUVTex->Abrir(x, y, MenuPantallaW, MenuPantallaH);
    MenuAbierto = gMenuUVTex;
}

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
    if (B.size() > 5 && B[5]->visible && B[5]->Contains(mx, my)) {        // Texture (dropdown)
        uv->ActualizarBarra();
        LayoutAbrirMenuUVTex(uv, B[5]->sx, B[5]->sy + B[5]->height - GlobalScale);
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
    if (!vp || !vp->isLeaf()) return false;
    // el viewport 3D tiene su cadena propia (abajo); los demas se abren solos por el virtual compartido, asi no
    // hay que repetir aca el if de cada boton de cada panel
    if (vp->ViewportKind() != 1) return vp->AbrirMenuDeBarra(mx, my);
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
    Button* bAnim = BarRolBtn(B, BR_Animation);
    if (MenuMode && bMode && bMode->visible && bMode->Contains(mx, my)) {
        objetivo = MenuMode; boton = bMode;
        LayoutRebuildMenuMode();   // mode-aware: malla -> Paints ; armature -> Pose
        if (!MenuMode->action) MenuMode->action = LayoutAccionMode;
    } else if (MenuSelMode && bSelM && bSelM->visible && bSelM->Contains(mx, my)) {
        objetivo = MenuSelMode; boton = bSelM;
        if (!MenuSelMode->action) MenuSelMode->action = LayoutAccionSelMode;
    } else if (bPiv && bPiv->visible && bPiv->Contains(mx, my)) {
        // Pivot: el menu se REARMA cada vez (marca el activo) -> via LayoutMenuPivot
        if (MenuAbierto) MenuAbierto->Cerrar();
        LayoutMenuPivot(bPiv->sx, bPiv->sy + bPiv->height - GlobalScale);
        RegistrarMenuBarra(MenuAbierto, bPiv);
        return true;
    } else if (bSnap && bSnap->visible && bSnap->Contains(mx, my)) {
        // Snap: el menu se REARMA cada vez (Base/Target marcan el activo + labels) -> via LayoutMenuSnapTool
        if (MenuAbierto) MenuAbierto->Cerrar();
        LayoutMenuSnapTool(bSnap->sx, bSnap->sy + bSnap->height - GlobalScale);
        RegistrarMenuBarra(MenuAbierto, bSnap);
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
    } else if (MenuMesh && bMesh && bMesh->visible && bMesh->Contains(mx, my)) {
        // Edit Mode: menu "Mesh" (Transform/Snap/Delete), comun a vertice/borde/cara.
        objetivo = MenuMesh; boton = bMesh;
        if (!MenuMesh->action) MenuMesh->action = LayoutAccionMesh;
    } else if (MenuAnimation && bAnim && bAnim->visible && bAnim->Contains(mx, my)) {
        // menu "Animation": keyframes del objeto + Motion Trail. Es su PROPIO boton de la barra -> rama propia
        // (estaba anidado adentro del Contains de "Object": pedia el cursor sobre los DOS botones a la vez y no
        //  se abria nunca).
        objetivo = MenuAnimation; boton = bAnim;
        if (!MenuAnimation->action) MenuAnimation->action = LayoutAccionObject; // ids 510..513
    } else if (bObj && bObj->visible && bObj->Contains(mx, my)) {
        // Edit Mode -> menu de contexto Vertex/Edge/Face; Pose Mode -> menu "Pose"; Object Mode -> menu "Object".
        if (InteractionMode == EditMode) {
            if (MenuAbierto) MenuAbierto->Cerrar();
            LayoutMenuEditContexto(bObj->sx, bObj->sy + bObj->height - GlobalScale);
            RegistrarMenuBarra(MenuAbierto, bObj);
            return true;
        } else if (InteractionMode == PoseMode) {
            extern PopupMenu* MenuPose;
            if (MenuPose){ objetivo = MenuPose; boton = bObj; if (!MenuPose->action) MenuPose->action = LayoutAccionObject; } // reusa ids 100/101/102/500
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
        RegistrarMenuBarra(MenuOverlays, boton);
        return true;
    }
    if (MenuAbierto == objetivo) return true; // ese menu ya esta abierto
    if (MenuAbierto) MenuAbierto->Cerrar();    // cerrar el otro (cambio de menu)
    objetivo->Abrir(boton->sx, menuY, MenuPantallaW, MenuPantallaH);
    MenuAbierto = objetivo;
    RegistrarMenuBarra(objetivo, boton);
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
    } else if (vp->ViewportKind() == 5) {
        // Timeline: entra/sale del foco de la barra de TRANSPORTE (play/inicio/fin/Start/End/anim). Antes caia en
        // el else -> abria el menu de tipo/split, que no es lo que se quiere navegar (pedido Dante).
        LayoutTimelineBarToggle();
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
        // El rol lo registro el que ABRIO el menu, sacandolo de su boton (RegistrarMenuBarra). Se exige que el
        // menu abierto sea EL que se registro: si no, lo abrio otro camino y no es un menu de barra.
        const int rol = (MenuAbierto == gMenuBarraAbierto) ? gMenuBarraRol : -1;
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
        if (!((Mesh*)g_editMesh)->BorrarEdgeLoopEdit()) Notificar(T("Delete Edge Loops: select an edge loop first"), true);
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
    gMenuDelete->titulo = T("Delete");
    // OJO: action = LayoutAccionObject (NO AccionDelete). Como submenu, sus items se despachan por la accion del menu
    // TOP (el de contexto = LayoutAccionObject); usar la misma accion + ids unicos 361-364 hace que funcione IGUAL
    // sea como submenu (menu de contexto) o standalone (atajo X, donde este ES el menu top).
    gMenuDelete->action = LayoutAccionObject;
    gMenuDelete->Agregar(T("Vertices"), 361);
    gMenuDelete->Agregar(T("Edges"), 362);
    gMenuDelete->Agregar(T("Faces"), 363);
    gMenuDelete->Agregar(T("Edge Loops"), 364); // disuelve el loop (inverso del loop cut)
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
            gMenuFace = new PopupMenu(); gMenuFace->titulo = T("Face"); gMenuFace->action = LayoutAccionObject;
            gMenuFace->Agregar(T("Extrude Faces"), 300)->atajo = "E";
            gMenuFace->Agregar(T("Loop Cut and Slide"), 340)->atajo = "Ctrl R";
            gMenuFace->Agregar(T("Rip"), 341)->atajo = "V";
            gMenuFace->Agregar(T("Shade Smooth"), 320);
            gMenuFace->Agregar(T("Shade Flat"), 321);
            gMenuFace->Agregar(T("Recalculate Normals"), 322);
            gMenuFace->Agregar(T("Triangulate Faces"), 323)->atajo = "Ctrl T";
            // (Delete se movio al menu "Mesh": es comun a vertice/borde/cara)
        }
        m = gMenuFace;
    } else if (EditSelectMode == SelEdge) {
        if (!gMenuEdge) {
            gMenuEdge = new PopupMenu(); gMenuEdge->titulo = T("Edge"); gMenuEdge->action = LayoutAccionObject;
            gMenuEdge->Agregar(T("Extrude Edges"), 300)->atajo = "E";
            gMenuEdge->Agregar(T("Loop Cut and Slide"), 340)->atajo = "Ctrl R";
            gMenuEdge->Agregar(T("Rip"), 341)->atajo = "V";
            gMenuEdge->Agregar(T("Mark Sharp"), 330)->atajo = "W";
            gMenuEdge->Agregar(T("Clear Sharp"), 331);
            // (Delete se movio al menu "Mesh": es comun a vertice/borde/cara)
        }
        m = gMenuEdge;
    } else {
        if (!gMenuVertex) {
            gMenuVertex = new PopupMenu(); gMenuVertex->titulo = T("Vertex"); gMenuVertex->action = LayoutAccionObject;
            gMenuVertex->Agregar(T("New Edge/Face from Vertices"), 310)->atajo = "F";
            gMenuVertex->Agregar(T("Extrude Vertices"), 300)->atajo = "E";
            gMenuVertex->Agregar(T("Rip"), 341)->atajo = "V";
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
        gMenuSharp = new PopupMenu(); gMenuSharp->titulo = T("Edge"); gMenuSharp->action = LayoutAccionObject;
        gMenuSharp->Agregar(T("Mark Sharp"), 330);
        gMenuSharp->Agregar(T("Clear Sharp"), 331);
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
static PopupMenu* gMenuSnapBase=NULL, *gMenuSnapTarget=NULL, *gMenuSnapIndiv=NULL; // gMenuSnapTool: declarado arriba
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
    if (!gMenuSnapBase){ gMenuSnapBase=new PopupMenu(); gMenuSnapBase->titulo=T("Snap Base"); }
    gMenuSnapBase->Limpiar();
    for (int b=SNAP_CLOSEST;b<=SNAP_ACTIVE;b++) gMenuSnapBase->Agregar(SnapBaseNom(b), 100+b)->verde=(g_snap.base==b);
    if (!gMenuSnapTarget){ gMenuSnapTarget=new PopupMenu(); gMenuSnapTarget->titulo=T("Snap Target"); }
    gMenuSnapTarget->Limpiar();
    for (int t=SNAP_VERTEX;t<=SNAP_FACECENTER;t++) gMenuSnapTarget->Agregar(SnapTargetNom(t), 200+t)->verde=(g_snap.target==t);
    // sin titulo: el boton "Snap" de la barra ya lo dice; una cabecera "Snap" gastaria una fila (240p Symbian)
    // REGLA DE DISENO de los titulos: un menu que se abre desde algo SIN TEXTO (un icono, o un atajo de teclado)
    // lleva titulo -- es lo unico que te dice que estas mirando. Si lo abre un boton/item que YA decia el texto, NO
    // lleva: repetirlo es ruido. El boton Snap ahora es un iman -> titulo.
    if (!gMenuSnapTool){ gMenuSnapTool=new PopupMenu(); gMenuSnapTool->titulo="Snap"; gMenuSnapTool->action=AccionSnapRouter; }
    gMenuSnapTool->Limpiar();
    gMenuSnapTool->AgregarCheck(T("Enable"), 0, &g_snap.enabled)->atajo="Shift Tab";
    // el resto se ve en GRIS cuando el snap esta apagado (->gris = &g_snap.enabled): sin snap
    // ninguna de estas opciones hace efecto, igual que "Show Overlays" grisa sus hijos.
    gMenuSnapTool->Agregar(std::string("Snap Base: ")+SnapBaseNom(g_snap.base), 0, -1, gMenuSnapBase)->gris = &g_snap.enabled;
    gMenuSnapTool->Agregar(std::string("Snap Target: ")+SnapTargetNom(g_snap.target), 0, -1, gMenuSnapTarget)->gris = &g_snap.enabled;
    // SOLO en target FACE: proyeccion POR VERTICE (retopologia). Submenu con 2 tildes INDEPENDIENTES (no radio):
    // Face Project (a lo largo del rayo de la vista) y Face Nearest (al punto mas cercano de la superficie).
    if (g_snap.target == SNAP_FACE){
        if (!gMenuSnapIndiv){ gMenuSnapIndiv=new PopupMenu(); gMenuSnapIndiv->titulo=T("Individual Elements"); }
        gMenuSnapIndiv->Limpiar();
        gMenuSnapIndiv->AgregarCheck(T("Face Project"), 0, &g_snap.faceProject);
        gMenuSnapIndiv->AgregarCheck(T("Face Nearest"), 0, &g_snap.faceNearest);
        gMenuSnapTool->Agregar(T("Snap Target for Individual Elements"), 0, -1, gMenuSnapIndiv)->gris = &g_snap.enabled;
    }
    gMenuSnapTool->AgregarCheck(T("Affect Move"),   0, &g_snap.afMove)->gris = &g_snap.enabled;
    gMenuSnapTool->AgregarCheck(T("Affect Rotate"), 0, &g_snap.afRot)->gris = &g_snap.enabled;
    gMenuSnapTool->AgregarCheck(T("Affect Scale"),  0, &g_snap.afScale)->gris = &g_snap.enabled;
    gMenuSnapTool->AgregarCheck(T("Include Active"),     0, &g_snap.tsActive)->gris = &g_snap.enabled;
    gMenuSnapTool->AgregarCheck(T("Include Edited"),     0, &g_snap.tsEdited)->gris = &g_snap.enabled;
    gMenuSnapTool->AgregarCheck(T("Include Non-Edited"), 0, &g_snap.tsNonEdited)->gris = &g_snap.enabled;
    if (MenuAbierto) MenuAbierto->Cerrar();
    gMenuSnapTool->Abrir(mx, my, MenuPantallaW, MenuPantallaH);
    MenuAbierto = gMenuSnapTool;
}

void LayoutMenuPivot(int mx, int my) {
    if (!gMenuPivot) {
        gMenuPivot = new PopupMenu();
        gMenuPivot->titulo = T("Transform Pivot Point");
        gMenuPivot->action = AccionPivot;
    }
    // se rearma cada vez: cada opcion con su ICONO y la ACTIVA en verde
    gMenuPivot->Limpiar();
    gMenuPivot->Agregar(T("3D Cursor"), PivotCursor3D, IconType::pivotCursor)->verde = (g_transformPivot==PivotCursor3D);
    gMenuPivot->Agregar(T("Individual Origins"), PivotIndividual, IconType::pivotIndividual)->verde = (g_transformPivot==PivotIndividual);
    gMenuPivot->Agregar(T("Median Point"), PivotMedian, IconType::pivotMedian)->verde = (g_transformPivot==PivotMedian);
    gMenuPivot->Agregar(T("Active Element"), PivotActive, IconType::pivotActive)->verde = (g_transformPivot==PivotActive);
    gMenuPivot->AgregarCheck(T("Lock Normals"), 9, &g_editLockNormales);
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

// ===== Face Project / Nearest (retopologia): proyectar CADA vert por separado sobre la superficie de OTRA geometria =====
static bool SnapRayTri(const Vector3& ro, const Vector3& rd, const Vector3& a, const Vector3& b, const Vector3& c, float& tOut){
    Vector3 e1=b-a, e2=c-a;
    Vector3 pv(rd.y*e2.z-rd.z*e2.y, rd.z*e2.x-rd.x*e2.z, rd.x*e2.y-rd.y*e2.x); // rd x e2
    float det=e1.Dot(pv); if (det>-1e-8f && det<1e-8f) return false;
    float inv=1.0f/det; Vector3 tv=ro-a;
    float u=tv.Dot(pv)*inv; if (u<-1e-4f||u>1.0f+1e-4f) return false;
    Vector3 qv(tv.y*e1.z-tv.z*e1.y, tv.z*e1.x-tv.x*e1.z, tv.x*e1.y-tv.y*e1.x); // tv x e1
    float v=rd.Dot(qv)*inv; if (v<-1e-4f||u+v>1.0f+1e-4f) return false;
    tOut=e2.Dot(qv)*inv; return tOut>1e-5f; // delante de la camara
}
// punto mas cercano de un triangulo a p (Ericson, Real-Time Collision Detection)
static Vector3 SnapClosestTri(const Vector3& p, const Vector3& a, const Vector3& b, const Vector3& c){
    Vector3 ab=b-a, ac=c-a, ap=p-a; float d1=ab.Dot(ap), d2=ac.Dot(ap);
    if (d1<=0 && d2<=0) return a;
    Vector3 bp=p-b; float d3=ab.Dot(bp), d4=ac.Dot(bp);
    if (d3>=0 && d4<=d3) return b;
    float vc=d1*d4-d3*d2; if (vc<=0 && d1>=0 && d3<=0){ float t=d1/(d1-d3); return a+ab*t; }
    Vector3 cp=p-c; float d5=ab.Dot(cp), d6=ac.Dot(cp);
    if (d6>=0 && d5<=d6) return c;
    float vb=d5*d2-d1*d6; if (vb<=0 && d2>=0 && d6<=0){ float t=d2/(d2-d6); return a+ac*t; }
    float va=d3*d6-d5*d4; if (va<=0 && (d4-d3)>=0 && (d5-d6)>=0){ float t=(d4-d3)/((d4-d3)+(d5-d6)); return b+(c-b)*t; }
    float den=1.0f/(va+vb+vc); float v=vb*den, w=vc*den; return a+ab*v+ac*w;
}
// proyecta p (mundo) sobre las caras de la geometria candidata (menos la malla en edicion). modo 0=rayo de la vista,
// 1=punto mas cercano. Devuelve false si no hay superficie -> el vert se queda donde estaba (move normal).
static bool SnapFaceProyectarPunto(const Vector3& p, int modo, Vector3& out){
    if (!SceneCollection || !Viewport3DActive) return false;
    std::vector<Mesh*> meshes; SnapRecolectar(SceneCollection, meshes);
    if (meshes.empty()) return false;
    Mesh* em = (InteractionMode==EditMode) ? (Mesh*)g_editMesh : NULL;
    Vector3 cam = Viewport3DActive->viewPos;
    Vector3 rd = p - cam; float rl=sqrtf(rd.Dot(rd)); if (rl<1e-9f) return false; rd=rd*(1.0f/rl);
    bool found=false; float bestT=1e30f, bestD=1e30f; Vector3 best;
    for (size_t mi=0; mi<meshes.size(); mi++){
        Mesh* m=meshes[mi]; if (m==em) continue; // no proyectar sobre la propia malla en edicion (retopo -> otra geometria)
        if (!m->vertex || m->vertexSize<=0) continue;
        Matrix4 W; m->GetWorldMatrix(W);
        for (size_t f=0; f<m->faces3d.size(); f++){
            const std::vector<int>& idx=m->faces3d[f].idx; int nc=(int)idx.size(); if (nc<3) continue;
            Vector3 w0=W*Vector3(m->vertex[idx[0]*3],m->vertex[idx[0]*3+1],m->vertex[idx[0]*3+2]);
            for (int t=1; t+1<nc; t++){
                Vector3 w1=W*Vector3(m->vertex[idx[t]*3],  m->vertex[idx[t]*3+1],  m->vertex[idx[t]*3+2]);
                Vector3 w2=W*Vector3(m->vertex[idx[t+1]*3],m->vertex[idx[t+1]*3+1],m->vertex[idx[t+1]*3+2]);
                if (modo==0){ float tt; if (SnapRayTri(cam,rd,w0,w1,w2,tt) && tt<bestT){ bestT=tt; best=cam+rd*tt; found=true; } }
                else { Vector3 c=SnapClosestTri(p,w0,w1,w2); Vector3 d=c-p; float dd=d.Dot(d); if (dd<bestD){ bestD=dd; best=c; found=true; } }
            }
        }
    }
    if (found) out=best;
    return found;
}
// proyeccion POR VERTICE activa? Solo en target FACE, Edit Mode, move LIBRE (desde la vista, sin eje ni extrude).
static bool SnapFaceIndividualActivo(){
    return g_snap.enabled && g_snap.afMove && g_snap.target==SNAP_FACE &&
           (g_snap.faceProject || g_snap.faceNearest) &&
           InteractionMode==EditMode && gEVmesh && !gEVuseCustom && axisSelect==ViewAxis;
}
// posicion final del vert 'wn' (ya trasladado) con Face Project/Nearest. Project (rayo) tiene prioridad; si no pega,
// prueba Nearest; si tampoco, queda donde estaba (move normal).
static Vector3 SnapFaceIndividualPunto(const Vector3& wn){
    Vector3 pr;
    if (g_snap.faceProject && SnapFaceProyectarPunto(wn, 0, pr)) return pr;
    if (g_snap.faceNearest && SnapFaceProyectarPunto(wn, 1, pr)) return pr;
    return wn;
}

// recomputa cada vertice desde su world0 + el acumulado activo y lo escribe en la
// malla (todos los GPU del grupo) + refresca el overlay.
static void EVEscribir(){
    if (!gEVmesh) return;
    Mesh* m = gEVmesh;
    const bool faceIndiv = (estado == translacion) && SnapFaceIndividualActivo(); // proyeccion por-vertice (retopo)
    for (size_t i=0;i<gEVsnap.size();i++){
        EditVtxSnap& s = gEVsnap[i];
        Vector3 wn;
        if (estado == translacion){
            wn = s.world0 + gEVtrans;
            if (faceIndiv) wn = SnapFaceIndividualPunto(wn); // cada vert se pega a la superficie de atras (o queda igual)
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

// SNAP de ESCALA: escala la seleccion desde el pivote (gEVpivot = centro/cursor) de modo que el vert BASE (el activo
// en modo Active, o el mas cercano al target en Closest) se mueva RADIALMENTE -"como un radio"- hasta el punto mas
// cercano al target sobre su recta de escala. Puede NO tocar el target (queda en el punto mas cercano de esa recta);
// toca exacto cuando el target esta sobre la recta (ej: rotar primero la aguja al target y luego escalar).
static void SnapAjustarEditScale(){
    g_snapHit = false;
    if (!g_snap.enabled || !g_snap.afScale || !Viewport3DActive || InteractionMode!=EditMode || !gEVmesh) return;
    if (gEVsnap.empty() || gEVshrink) return; // Shrink/Fatten mueve cada vert por su normal -> no aplica este snap
    Vector3 T; float sx=0, sy=0;
    if (!SnapBuscarTarget(g_snapCurX, g_snapCurY, Viewport3DActive, T, sx, sy)) return;
    const Vector3 P = gEVpivot;
    // BASE: Active = el elemento activo; Closest (y otros) = el vert de la seleccion mas cercano al target
    Vector3 B0; bool any=false;
    if (g_snap.base==SNAP_ACTIVE) any = SnapNeedleActivo(B0);
    if (!any){ float bd=1e30f; for (size_t i=0;i<gEVsnap.size();i++){ Vector3 d=gEVsnap[i].world0-T; float dd=d.Dot(d); if(dd<bd){bd=dd;B0=gEVsnap[i].world0;any=true;} } }
    if (!any) return;
    // direccion en la que la ESCALA mueve al base (segun el eje): D=off0 (uniforme), la componente del eje (X/Y/Z),
    // o la del plano. B(amt) = B0 + amt*D -> amt que lo acerca al target = proyeccion de (T-B0) sobre D.
    Vector3 off0 = B0 - P, D;
    if (axisSelect==X||axisSelect==Y||axisSelect==Z){ Vector3 a=EjeOrientado(*gEVmesh,axisSelect); D=a*off0.Dot(a); }
    else if (axisSelect==PlaneX||axisSelect==PlaneY||axisSelect==PlaneZ){ int ex=(axisSelect==PlaneX)?X:(axisSelect==PlaneY)?Y:Z; Vector3 a=EjeOrientado(*gEVmesh,ex); D=off0 - a*off0.Dot(a); }
    else D=off0;
    float dd=D.Dot(D); if (dd<1e-12f) return; // el base esta en el pivote/eje -> la escala no lo mueve
    gEVscaleAmt = (T - B0).Dot(D) / dd;
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
    // con proyeccion POR VERTICE (Face Project/Nearest) NO se hace el snap de la seleccion entera: cada vert se
    // proyecta solo (en EVEscribir). Sino, el imantado normal de la seleccion al target bajo el cursor.
    if (!SnapFaceIndividualActivo()) SnapAjustarEditTrans();
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
    gEVscaleAmt += (dx+dy)*factor;
    SnapAjustarEditScale(); // imanta el vert base al target radialmente (si snap ON)
    EVEscribir();
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
        gMenuSnap->Agregar(T("Selection to Grid"), 0);
        gMenuSnap->Agregar(T("Selection to Cursor"), 1);
        gMenuSnap->Agregar(T("Selection to Cursor (Keep Offset)"), 2);
        gMenuSnap->Agregar(T("Selection to Active"), 3);
        gMenuSnap->Agregar(T("Cursor to Selected"), 4);
        gMenuSnap->Agregar(T("Cursor to World Origin"), 5);
        gMenuSnap->Agregar(T("Cursor to Grid"), 6);
        gMenuSnap->Agregar(T("Cursor to Active"), 7);
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
        gMenuMerge->titulo = T("Merge");
        gMenuMerge->action = LayoutAccionObject;
        gMenuMerge->Agregar(T("At Center"), 380);
        gMenuMerge->Agregar(T("At Cursor"), 381);
        gMenuMerge->Agregar(T("Collapse"), 382);
        gMenuMerge->Agregar(T("By Distance"), 383);
    }
    return gMenuMerge;
}

// submenu Normals (Recalculate + Flip) para embeber en el menu "Mesh". Ids 322/324 + accion LayoutAccionObject.
static PopupMenu* gMenuNormals = NULL;
PopupMenu* LayoutSubmenuNormals() {
    if (!gMenuNormals) {
        gMenuNormals = new PopupMenu();
        gMenuNormals->titulo = T("Normals");
        gMenuNormals->action = LayoutAccionObject;
        gMenuNormals->Agregar(T("Recalculate Normals"), 322)->atajo = "Shift N"; // orienta hacia afuera (cubo/esfera OK)
        gMenuNormals->Agregar(T("Flip"), 324);                                    // simplemente invierte las normales
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

// Cada viewport dice SOLO si scrollea (ViewportBase::ComoScrollable). Antes esto era una tabla de downcasts a
// mano por ViewportKind() y habia que acordarse de venir a tocarla aca por cada viewport nuevo.
static Scrollable* ComoScrollable(ViewportBase* v) { return v ? v->ComoScrollable() : NULL; }

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

// DRAG-SCROLL de menus desplegables largos (ej: armature con 129 clips): arrastrar el dedo/mouse scrollea la lista;
// un toque sin arrastrar selecciona. La seleccion normal es en el DOWN, pero si el menu es SCROLLABLE se DIFIERE al
// UP para distinguir tap (selecciona) de drag (scrollea).
static PopupMenu* g_menuDrag = NULL;   // submenu mas profundo que se esta arrastrando (NULL = no)
static int  g_menuDragY0 = 0, g_menuDragScroll0 = 0;
static bool g_menuDragMoved = false;
static PopupMenu* MenuScrollBajoCursor(int mx, int my){
    if (!MenuAbierto) return NULL;
    PopupMenu* t = MenuAbierto;
    while (t->submenuAbierto && t->submenuAbierto->abierto && t->submenuAbierto->Contains(mx, my)) t = t->submenuAbierto;
    return (t->Contains(mx, my) && t->MaxScroll() > 0) ? t : NULL;
}
// llamado en el DOWN sobre un menu SCROLLABLE: arranca un posible drag/tap (difiere la seleccion). true = diferido.
bool LayoutMenuDragArrancar(int mx, int my){
    PopupMenu* sc = MenuScrollBajoCursor(mx, my);
    if (!sc) return false;
    g_menuDrag = sc; g_menuDragY0 = my; g_menuDragScroll0 = sc->scroll; g_menuDragMoved = false;
    return true;
}
// llamado en el UP: si NO se arrastro (fue un tap), selecciona el item bajo el cursor. true = consumido.
bool LayoutMenuDragSoltar(int mx, int my){
    if (!g_menuDrag) return false;
    bool moved = g_menuDragMoved; g_menuDrag = NULL;
    if (!moved && MenuAbierto){
        extern void MenuLimpiarGuardAbrir(); MenuLimpiarGuardAbrir(); // el tap diferido es DELIBERADO: no lo bloquea el guard de submenu-recien-abierto
        PopupMenu* m = MenuAbierto; // Click puede CERRAR el menu (MenuAbierto=NULL) -> capturar antes de usar m->action
        int id = m->Click(mx, my);
        if (id >= 0) m->Ejecutar(id);   // accion propia del item (declarativo) o el action(id) viejo
    }
    g_redraw = true;
    return true;
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
        // DRAG-SCROLL: si el boton esta apretado sobre un menu scrollable, arrastrar mueve la LISTA (no resalta ni
        // cambia de menu). Un umbral chico distingue drag de tap.
        if (g_menuDrag) {
            int dy = my - g_menuDragY0;
            if (!g_menuDragMoved && (dy > 6 * GlobalScale || dy < -6 * GlobalScale)) g_menuDragMoved = true;
            if (g_menuDragMoved) {
                int rowH = RenglonHeightGS + gapGS;
                int s = g_menuDragScroll0 - dy / rowH; // arrastrar hacia ABAJO muestra items ANTERIORES (grab & pull)
                int ms = g_menuDrag->MaxScroll(); if (s < 0) s = 0; if (s > ms) s = ms;
                g_menuDrag->scroll = s; g_redraw = true;
            }
            return true;
        }
        // el cursor esta SOBRE el desplegable abierto (su lista o cualquier submenu abierto)? Entonces el movimiento
        // es DEL MENU: NO tocar las barras/viewport de atras. Sin este guard, un desplegable dibujado ENCIMA de la
        // barra de otro panel disparaba LayoutAbrirMenuDeBarra/HoverArbol al pasar el mouse -> cambiaba/cerraba el
        // menu y resaltaba botones de atras (el hover "se colaba" al viewport).
        bool sobreMenu = false;
        for (PopupMenu* mm = MenuAbierto; mm && mm->abierto; mm = mm->submenuAbierto){
            if (mm->Contains(mx, my)){ sobreMenu = true; break; }
            if (!mm->submenuAbierto || !mm->submenuAbierto->abierto) break;
        }
        if (!sobreMenu){
            // el cursor SALIO del menu (esta sobre una barra): permitir el SLIDE entre menus de barra sin click
            ViewportBase* bajo = FindViewportUnderMouse(rootViewport, mx, my);
            LayoutAbrirMenuDeBarra(bajo, mx, my);
            LayoutHoverArbol(rootViewport, bajo, mx, my);
        }
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
        // menu SCROLLABLE (ej: 129 clips): NO seleccionar en el DOWN -> diferir al UP para poder arrastrar y
        // scrollear (el UP decide: tap=selecciona, drag=solo scrolleo). Menus cortos: click normal en el down.
        if (LayoutMenuDragArrancar(mx, my)) return true;
        // el menu se queda con el click (adentro o el que lo cierra)
        PopupMenu* m = MenuAbierto;
        int id = m->Click(mx, my);
        if (id >= 0) m->Ejecutar(id);   // accion propia del item (declarativo) o el action(id) viejo
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
        } else if (under->ViewportKind() == 5) {
            ((Timeline*)under)->ClickBarButton(mx, my); // transporte + campos Start/End
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
                if (id >= 0) m->Ejecutar(id);   // accion propia del item (declarativo) o el action(id) viejo
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
    if (viewPortActive->ViewportKind() == 5) { // Timeline
        Timeline* tl = (Timeline*)viewPortActive;
        if (tl->barFocusIndex >= 0) { // foco de barra (soft-izq): flechas mueven el foco, OK activa, C sale
            switch (tecla) {
                case LayoutKey::Left:   LayoutTimelineBarMover(-1); return true;
                case LayoutKey::Right:  LayoutTimelineBarMover(+1); return true;
                case LayoutKey::Enter:  LayoutTimelineBarActivar(); return true;
                case LayoutKey::Cancel: tl->barFocusIndex = -1; g_redraw = true; return true;
            }
            return false;
        }
        if (tecla == LayoutKey::Enter) { tl->TogglePlay(+1); return true; } // sin foco: OK = play (flechas -> NavFrame)
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

// nav del Timeline con flechas MANTENIDAS (frame-based, Symbian): izq/der = mover el frame actual (scrub),
// 0-mantenido + arriba/abajo = ZOOM centrado, * -mantenido + izq/der = PANEO de la vista. Devuelve true si el
// viewport activo es el Timeline (asi AplicarFlechas3D corta y no orbita la camara 3D). Con dx=dy=0 no hace
// nada pero igual devuelve true -> sirve de query "el activo es el Timeline?".
// ---------------------------------------------------------------------------------------------------------------
//  CLICK y TECLA al viewport que corresponde. Es el ruteo que PC ya tenia (main/controles.cpp) y que en el telefono
//  NO EXISTIA: como las 4 firmas de input pedian un SDL_Event, esos metodos se compilaban afuera y cada tecla y cada
//  click habia que reinventarlos como caso especial, casi siempre apuntando al viewport 3D. De ahi salia que una
//  tecla apretada en el timeline terminara moviendo el modelo.
// ---------------------------------------------------------------------------------------------------------------

// El cursor virtual del telefono no tiene un mouse detras: manda el MISMO down que manda el mouse en PC, al mismo
// viewport. El que esta bajo el cursor pasa a ACTIVO y recibe button_left(). Mantener el boton apretado sale gratis
// como arrastre: el cursor ya emite event_mouse_motion mientras se mueve, que es de donde los viewports lo sacan.
// false = no habia viewport abajo.
// Lock Orbit: el arrastre pasa a PANEAR en vez de orbitar (modo tablero 2D). Lo comparten el menu del viewport3d
// y el 9 del telefono. El cartel es la unica senal de que cambio: en el menu se ve la marca verde, pero por atajo
// no hay nada que mirar y el mismo gesto pasa a hacer otra cosa.
bool LayoutLockOrbitToggle(){
    if (!Viewport3DActive) return false;
    Viewport3DActive->lockOrbit = !Viewport3DActive->lockOrbit;
    Notificar(Viewport3DActive->lockOrbit ? "Orbit locked" : "Orbit unlocked", false);
    g_redraw = true;
    return true;
}

bool LayoutClickViewport(int mx, int my){
    ViewportBase* vp = FindViewportUnderMouse(rootViewport, mx, my);
    if (!vp) return false;
    viewPortActive = vp;
    lastMouseX = mx; lastMouseY = my;   // button_left() los lee: son SU idea de donde ocurrio el click
    vp->button_left();
    return true;
}
bool LayoutSoltarViewport(int mx, int my){
    if (!viewPortActive) return false;
    lastMouseX = mx; lastMouseY = my;
    viewPortActive->mouse_button_up(W3dMB_IZQ);
    return true;
}
// La tecla va al viewport ACTIVO, como en PC. false = no hay activo (o la plataforma no supo traducirla).
//
// OJO con el valor de retorno: dice "se la mande", NO "el viewport hizo algo con ella". event_key_down devuelve
// void, asi que un viewport no tiene forma de contestar "esta tecla no es mia" y quien llama no puede saberlo.
// Mientras sea asi, el que llama tiene que estar seguro de que ESE viewport maneja ESA tecla antes de mandarsela:
// si no, la tecla se pierde en el no-op de la base (paso: las flechas de Properties dejaron de navegar el panel
// porque en el telefono Properties todavia no overridea event_key_down y la base se las comia en silencio).
// El arreglo de fondo es que event_key_down devuelva bool.
bool LayoutTeclaViewport(int tecla, bool repeticion){
    if (!viewPortActive || tecla == W3dK_NADA) return false;
    viewPortActive->event_key_down(tecla, repeticion);
    return true;
}
bool LayoutTeclaViewportUp(int tecla){
    if (!viewPortActive || tecla == W3dK_NADA) return false;
    viewPortActive->event_key_up(tecla);
    return true;
}

bool LayoutTimelineNavFrame(int dx, int dy, bool zoomMode, bool panMode) {
    if (!viewPortActive || !viewPortActive->isLeaf() || viewPortActive->ViewportKind() != 5) return false;
    Timeline* tl = (Timeline*)viewPortActive;
    if (tl->barFocusIndex >= 0) return false; // foco de barra activo (soft-izq): las flechas navegan la barra, no scrollean
    if (panMode) {                                       // * + flechas = panear la vista
        if (dx) tl->PanFrames((float)dx * 1.5f);         // horizontal: frames
        // vertical: en CURVAS panea el VALOR; en dope sheet scrollea las filas. Antes arriba/abajo con * no
        // hacia NADA (solo se miraba dx).
        if (dy){
            if (tl->modo == Timeline::TL_MODO_CURVAS)
                tl->PanValor((float)dy * 8.0f / (tl->pxPerUnit > 1e-6f ? tl->pxPerUnit : 1e-6f));
            else { tl->PosY += dy * 8; if (tl->PosY > 0) tl->PosY = 0; g_redraw = true; }
        }
    } else if (zoomMode) {                               // 0 + flechas = zoom
        // los DOS ejes por separado: izq/der = tiempo, arriba/abajo = VALOR (solo en curvas, que es donde el eje
        // vertical significa algo). Antes arriba/abajo tambien hacian zoom HORIZONTAL, que no tiene sentido.
        if (dx) tl->ZoomBy(dx > 0 ? 1.06f : 0.94f, tl->CentroTimeline()); // centro del STRIP (excluye el panel)
        if (dy){
            if (tl->modo == Timeline::TL_MODO_CURVAS) tl->ZoomVBy(dy < 0 ? 1.06f : 0.94f);
            else tl->ZoomBy(dy < 0 ? 1.06f : 0.94f, tl->CentroTimeline()); // dope sheet: no hay eje vertical propio
        }
    } else {                                             // flechas solas
        // izq/der = mover el frame actual (scrub). El paso depende del ZOOM: siempre ~10px en pantalla, asi
        // recorrer la animacion cuesta lo mismo con cualquier zoom.
        if (dx) tl->ScrubFlecha(dx > 0 ? 1 : -1);
        // arriba/abajo NO van aca: saltar de keyframe es UNO POR PULSACION y esta funcion la llama la repeticion de
        // flecha mantenida (cada frame). Va como tecla al viewport activo, desde el key-down.
    }
    return true;
}

// ---- Timeline: navegacion de la BARRA de transporte por teclado (Symbian, sin mouse). soft-izq entra/sale del
// modo foco (barFocusIndex>=0); flechas mueven el foco entre los botones VISIBLES (salteando el [0] tipo/split y
// los ocultos); OK activa el enfocado via ClickBarButton (play/inicio/fin/Start/End/dropdown de animacion). Con el
// foco activo, LayoutTimelineNavFrame devuelve false -> las flechas navegan la barra en vez de scrollear. ----
static Timeline* TimelineActivoPtr() {
    if (viewPortActive && viewPortActive->isLeaf() && viewPortActive->ViewportKind() == 5) return (Timeline*)viewPortActive;
    return NULL;
}
void LayoutTimelineBarMover(int dir) {
    Timeline* tl = TimelineActivoPtr(); if (!tl) return;
    std::vector<Button*>& B = tl->BarButtons;
    int maxIdx = (int)B.size() - 1;
    if (maxIdx < 0) return;
    int idx = tl->barFocusIndex;
    for (int k = 0; k <= maxIdx; k++) {
        idx += dir;
        if (idx > maxIdx) idx = 0;                // wrap
        if (idx < 0) idx = maxIdx;
        if (idx == 0 || B[idx]->visible) break;   // el [0] (tipo/split) SIEMPRE es navegable -> asi se puede cambiar el viewport
    }
    tl->barFocusIndex = idx;
    tl->ActualizarBarra();                        // auto-scroll para mostrar el enfocado (RenderBar centra el foco)
    g_redraw = true;
}
void LayoutTimelineBarToggle() {
    Timeline* tl = TimelineActivoPtr(); if (!tl) return;
    if (tl->barFocusIndex >= 0) tl->barFocusIndex = -1;   // salir del modo foco
    else { tl->barFocusIndex = 0; LayoutTimelineBarMover(+1); } // entrar: primer boton de transporte (izq llega al [0] tipo)
    g_redraw = true;
}
void LayoutTimelineBarActivar() {
    Timeline* tl = TimelineActivoPtr(); if (!tl) return;
    int idx = tl->barFocusIndex;
    if (idx < 0 || idx > (int)tl->BarButtons.size() - 1) return;
    tl->ActualizarBarra();                        // sx/sy frescos antes del hit-test
    if (idx == 0) { LayoutAbrirMenuTipo(tl); return; } // [0] = menu tipo/split: cambiar el viewport a otro (3D/outliner/etc.)
    Button* b = tl->BarButtons[idx];
    if (b->visible) tl->ClickBarButton(b->sx + b->width / 2, b->sy + b->height / 2);
}

// ====================================================================
// overlay: el desplegable abierto, encima de todo
// ====================================================================

// FPS compartido: cada plataforma lo llama UNA vez por frame dibujado con su reloj de pared en ms (PC:
// SDL_GetTicks; Symbian: User::NTickCount). Mide el TIEMPO ENTRE FRAMES REALES con promedio exponencial ->
// g_fpsActual = 1000/ms-por-frame. Refleja el costo REAL del frame en curso (liviano=UI/escena vacia -> fps
// alto; pesado=skinning en play -> fps bajo): son valores CORRECTOS, no un bug, el numero sigue a lo que se dibuja.
void LayoutTickFPS(unsigned long wallMs) {
    static unsigned long lastFrame = 0;
    static float frameMsProm = 0.0f; // ms por frame (promedio exponencial)
    if (lastFrame == 0) { lastFrame = wallMs; return; } // 1er frame: sin delta todavia
    unsigned long dt = wallMs - lastFrame;
    lastFrame = wallMs;
    // Render EVENT-DRIVEN: si nada cambia (quieto) NO se dibujan frames. Al retomar, el 1er frame trae un HUECO
    // enorme (todo el tiempo quieto) que NO es "lento" -> no promediarlo (marcaria fps bajos falsos). Se deja
    // g_fpsActual en el ultimo valor ACTIVO (lo que rinde CUANDO dibuja). Un render continuo >=2.5fps no entra aca.
    if (dt == 0 || dt > 400) return;
    if (frameMsProm <= 0.0f) frameMsProm = (float)dt;
    else                     frameMsProm = frameMsProm * 0.8f + (float)dt * 0.2f; // suavizado: estable y responsivo
    g_fpsActual = 1000.0f / frameMsProm;
}

// Tab (PC) / tecla equivalente (Symbian BT): alterna Object <-> Edit Mode del
// objeto ACTIVO si es una malla. Devuelve true si alterno (sino el caller hace
// otra cosa, p.ej. ciclar el viewport). La logica va aca -> PC y Symbian la usan.
bool LayoutToggleEditMode() {
    if (estado != editNavegacion) return false; // no en medio de un transform
    if (!ObjActivo) return false;
    // ARMATURE: Tab alterna Object <-> Pose (antes Tab no hacia nada con un esqueleto seleccionado).
    if (ObjActivo->getType() == ObjectType::armature) {
        UndoCapturarModo(); // Ctrl+Z: guarda el modo PREVIO
        InteractionMode = (InteractionMode == PoseMode) ? ObjectMode : PoseMode;
        ActualizarEditMeshActivo();
        return true;
    }
    if (ObjActivo->getType() != ObjectType::mesh) return false;
    UndoCapturarModo(); // Ctrl+Z: guarda el modo PREVIO antes de togglear
    InteractionMode = (InteractionMode == EditMode) ? ObjectMode : EditMode; // MALLA: Object <-> Edit
    ActualizarEditMeshActivo(); // refresca g_editMesh (PC + Symbian)
    return true;
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
