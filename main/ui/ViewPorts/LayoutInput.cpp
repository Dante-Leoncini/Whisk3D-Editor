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
#include "ViewPorts/Console.h"
#include "ViewPorts/Properties.h"
#include "ViewPorts/UVEditor.h"
#include "ViewPorts/Editor2D.h"
#include "ViewPorts/IDE.h"      // editor de texto de scripts lua (selector/Save/Refresh)
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
#include "objects/LOD.h"     // Add > LOD (un hijo por distancia a la camara)
#include "objects/Culling.h" // Add > Culling (frustum culling de sus hijos)
#include "objects/Particulas.h" // Add > Particles (emisor de particulas del Core)
#include "objects/UI.h"
#include "objects/Texto2D.h"
#include "objects/Imagen2D.h"
#include "objects/Rect2D.h"
#include "objects/Contenedor2D.h"
#include "objects/Slice9.h"
#include "objects/Boton2D.h"
#include "objects/Expandir2D.h"
#include "objects/Video2D.h"
#include "objects/Gamepad.h"   // entrada cruda del mando + el objeto Script
#include "io/UI2DFormato.h"     // cargar interfaces .w3dui
#include "W3dPaletas.h"         // paletas del proyecto (AddUI adopta la default)
#include "PopUp/FileBrowser.h"  // AbrirFileBrowser (elegir el archivo)
#include "io/Textura2D.h"       // tamano real de la imagen elegida al crearla
#include "io/Video2DCache.h"    // tamano real del video elegido al crearlo
#include "objects/Armature.h"
#include "animation/SkeletalAnimation.h" // InsertarKeyframeEsqueleto (Pose Mode: Insert Keyframe)
#include "animation/VertexAnimation.h"   // menu Animation del UV editor: frames de la vertex anim activa
#include "animation/Armature2DAnimation.h" // InsertarKeyframeArm2D (pose 2D: Insert Keyframe con clips propios)
#include "animation/Animation.h"         // ActiveAnimKind/ActiveAnimMesh/CurrentFrame (menu Animation del UV)
#include "objects/Instance.h"
#include "objects/Collection.h"
#include "objects/ObjectMode.h"
#include "edit/Modifier.h" // ModifierType::Mirror + target (regen de mirrors al mover objetos)
#include "edit/BoneEdit.h" // Edit Mode de ARMATURE (Fase 3): ops de huesos + menu contexto + prep al salir
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
        case 5: return new Editor2D();
        case 6: return new Console();
        case 7: return new IDE();
    }
    return NULL;
}

// ====================================================================
//  EL LAYOUT POR DEFECTO, en UN solo lugar. Lo usan el arranque sin archivo
//  (constructor.cpp) y el fallback de abrir un .w3d sin bloque Layout
//  (import_w3d.cpp), que antes armaba un outliner flaco encima de un 3D, sin
//  propiedades ni timeline.
//
//     +---------------------------+--------------+
//     |                           |  Outliner    |  40 % de la columna
//     |        Viewport 3D        +--------------+
//     |                           |  Propiedades |
//     +---------------------------+              |
//     |  Timeline (bien chico)    |              |
//     +---------------------------+--------------+
//
//  El outliner va ARRIBA porque es el indice de la escena: se lee de arriba
//  hacia abajo y lo que se elige ahi es lo que muestra el panel de abajo.
//  El timeline no necesita alto: 12 % alcanza para la barra y el dope.
// ====================================================================
ViewportBase* LayoutPorDefecto(int w, int h, Viewport3D** vp3dOut) {
    Viewport3D* vp3d = new Viewport3D();
    if (vp3dOut) *vp3dOut = vp3d;
    // UNA sola logica de layout por defecto, por TAMANO DE PANTALLA (identica en todos los sistemas):
    //   lado menor < 320px  -> 2 viewports: 3D + Propiedades, segun orientacion.
    //   lado menor >= 320px -> 4 viewports: 3D + Timeline + Outliner + Propiedades (el de arriba).
    int lado = (w < h) ? w : h;
    if (lado < 320) {
        if (w >= h)  // horizontal: 3D a la izquierda | Propiedades a la derecha
            return new ViewportRow(vp3d, new Properties(), 0.7f);
        // vertical (N95 240x320): 3D arriba / Propiedades abajo
        return new ViewportColumn(vp3d, new Properties(), 0.7f);
    }
    return new ViewportRow(
        new ViewportColumn(vp3d, new Timeline(), 0.88f),          // 3D grande / timeline chico
        new ViewportColumn(new Outliner(), new Properties(), 0.40f), // outliner 40 % arriba
        0.72f
    );
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
    // LIBERAR EL FOCO DE TECLADO POR HOVER: la accion llega desde el DOWN del click en el menu
    // (que ya puso ViewPortClickDown=true) y el UP va al mouse_button_up del viewport NUEVO. Si
    // ese tipo no lo implementa (le pasaba a Console), nadie lo apagaba y viewPortActive quedaba
    // CLAVADO en el viewport nuevo (las teclas seguian yendo ahi aunque muevas el mouse). Un
    // cambio estructural (cambiar tipo / split / expand) nunca deja un drag legitimo en curso,
    // asi que se libera aca, para CUALQUIER tipo de viewport.
    ViewPortClickDown = false;
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

// MODO JUEGO (.sisx bundleado): full-screen del viewport 3D -> el juego se ve SOLO, sin el
// chrome del editor (los demas viewports). Idempotente: si ya esta maximizado, no hace nada.
void LayoutMaximizar3DParaJuego() {
    if (!rootViewport || g_rootGuardado) return;   // sin layout o ya maximizado
    std::vector<ViewportBase*> hojas;
    LayoutRecolectarHojas(rootViewport, hojas);
    for (size_t i = 0; i < hojas.size(); i++)
        if (hojas[i]->ViewportKind() == 1) { viewPortActive = hojas[i]; break; } // 1 = ViewPort3D
    if (viewPortActive && viewPortActive != rootViewport) LayoutMaximizar();
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
    if (aId == 21) { LayoutDividir(vp, true); return; }   // en columnas (lado a lado)
    if (aId == 22) { LayoutDividir(vp, false); return; }  // en filas (apilados)
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
// LOD: nace sin umbrales (dibuja el ultimo hijo siempre); se cargan en el panel
void AddLOD(){      TrasCrearAdd(new LOD(NULL, cursor3D.pos)); }
void AddCulling(){  TrasCrearAdd(new Culling(NULL, cursor3D.pos)); }
// Particulas: nace emitiendo (cantidad 10/s) pero SIN textura -> no dibuja nada
// hasta que el usuario le carga un PNG en el panel
void AddParticulas(){ TrasCrearAdd(new Particulas(NULL, cursor3D.pos)); }
// interfaz 2D (se edita en el Editor 2D). Si el proyecto aun no tiene
// PALETAS, la default del UI ("Whisk3D") pasa a ser la del proyecto y queda
// seleccionada en la raiz nueva; con paletas ya cargadas no se mezclan
// (el usuario elige desde la tarjeta Paleta del objeto).
void AddUI(){
    UI* u = new UI(NULL, cursor3D.pos);
    if (W3dPaletas().empty()) {
        W3dPaletasAdoptar(u->paletas);
        if (u->paletaActiva >= 0 && u->paletaActiva < (int)u->paletas.size())
            u->paleta = u->paletas[u->paletaActiva].nombre;
    }
    TrasCrearAdd(u);
}
// (aca vivia AddControl, que creaba el objeto Script. Se dio de baja: CUALQUIER
//  objeto acepta scripts desde la pestania "Scripts" del panel, asi que un objeto cuya
//  unica razon de ser era llevar un .lua ya no tiene sentido. Los .w3d que traigan uno
//  siguen abriendo: el importador lo MIGRA a un Empty con los mismos scripts colgados.
//  Ver import_w3d.cpp.)
void AddCamera(){   TrasCrearAdd(new Camera(NULL, cursor3D.pos, Vector3(-35.0f, -45.0f, 0.0f))); }

void AddLight(){
    Light* l = Light::Create(NULL, 0, 0, 0);
    // el nombre se pone ACA y no en el constructor: la Light vive en el Core, y el Core no sabe -ni tiene por que
    // saber- que existen los idiomas. El editor la crea, el editor la nombra.
    if (l){ l->pos = cursor3D.pos; l->SetNameObj(T("Light")); }
    TrasCrearAdd(l);
}
void AddArmature(){
    // un solo hueso desde el origen, 0.3 hacia arriba (Y), en el cursor 3D.
    Armature* arm = new Armature(NULL, cursor3D.pos);
    W3dBone b; b.name = BoneNombreLibre(arm, "Bone", -1); b.parent = -1;
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
    inst->count = 3; inst->pos = Vector3(2, 0, 0); inst->SetNameObj("Array"); TrasCrearAdd(inst);
}
void AddMirror(){
    Instance* inst = new Instance(NULL, ObjActivo);
    inst->mirror = true; inst->mirrorEje = 0; inst->SetNameObj("Mirror"); TrasCrearAdd(inst);
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
    m->SetNameObj("Reference");
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

// importar una INTERFAZ 2D (.w3dui): el arbol entero aparece en la escena, seleccionado
static void UIImportElegido(const std::string& ruta){
    UI* u = UI2DCargar(ruta);
    if (!u) { Notificar(T("Could not read the file"), true); return; }
    // si el archivo vive DENTRO del proyecto abierto, recordarlo con su ruta
    // relativa al .w3d (con subcarpeta, ej "contenido/menu.w3dui"): el guardado
    // lo pisa EN su lugar (UI2DCargar solo guarda el nombre pelado)
    if (!w3dPath.empty()) {
        size_t s = w3dPath.find_last_of("/\\");
        std::string dir = (s == std::string::npos) ? std::string() : w3dPath.substr(0, s + 1);
        if (!dir.empty() && ruta.size() > dir.size() && ruta.compare(0, dir.size(), dir) == 0)
            u->archivoW3dui = ruta.substr(dir.size());
    }
    // MERGE en la escena actual: importar dos veces el mismo .w3dui metia widgets
    // homonimos. La reparacion los renumera con el criterio de siempre (primero-gana)
    // y NO arrastra NINGUN vinculo: las refs de script y los targets que decian "X" ya
    // resolvian al PRIMER "X" y tienen que seguir resolviendo AHI (ver el bloque de
    // W3dNombresRepararEscena; arrastrarlos era el fallo F1). El comentario viejo decia
    // lo contrario y quedo desactualizado desde ese arreglo.
    //
    // LO QUE LA REPARACION NO PUEDE ARREGLAR (y por eso se AVISA): si la RAIZ UI que
    // entra choca con una escena que ya existe, pasa a llamarse "Menu.001", pero los
    // cambiarEscena("Menu") de SU copia del .lua siguen diciendo "Menu" y resuelven a la
    // PRIMERA escena. No hay arreglo automatico posible:
    //   - el nombre de la escena viaja como STRING LITERAL adentro del .lua (no es una
    //     ref registrada en W3dScriptEntrada::refs, que es lo unico que el editor sabe
    //     reescribir), y
    //   - las DOS copias comparten EL MISMO archivo .lua en disco (el .w3dui guarda la
    //     ruta del script), asi que reescribirlo para la copia nueva ROMPERIA a la
    //     original.
    // Lo unico honesto es avisar con el nombre viejo y el nuevo para que el usuario
    // edite (o duplique) el .lua a mano. Es el mismo aviso que ya da el rename manual de
    // una escena en W3dRenombrarObjeto.
    const std::string nomAntes = u->name;
    W3dNombresRepararEscena(true);
    if (u->name != nomAntes) {
        // sin sprintf: los nombres son texto del usuario y no tienen cota de largo
        Notificar("la escena '" + nomAntes + "' ya existia -> '" + u->name +
                  "': los cambiarEscena(\"" + nomAntes + "\") de SU .lua siguen apuntando "
                  "a la primera. Editalos a mano.", true);
    }
    DeseleccionarTodo();
    u->Seleccionar();
    g_redraw = true;
}
void AddImportUI(){
    AbrirFileBrowser(T("Load UI"), T("Open"), ".w3dui .json", UIImportElegido);
}

// submenu Imports (OBJ/FBX/glTF/GLB). Nombres de formato: NO se traducen (son marcas).
static const MenuDef ADD_IMPORTS[] = {
    { "OBJ",  AddImportObj,  NULL, ICONO(IconType::mesh) },
    { "FBX",  AddImportFbx,  NULL, ICONO(IconType::mesh) },
    { "glTF", AddImportGltf, NULL, ICONO(IconType::mesh) },
    { "GLB",  AddImportGlb,  NULL, ICONO(IconType::mesh) },
    { "Whisk3D UI", AddImportUI, NULL, ICONO(IconType::textura) },
};

// submenu Mesh: las PRIMITIVAS. Eran 8 de las 19 filas del menu Add y lo hacian una
// lista larguisima donde la camara y la luz quedaban perdidas al fondo. Agrupadas, el
// menu de primer nivel queda en "que TIPO de objeto" y las mallas en su propia rama.
static const MenuDef ADD_MESHES[] = {
    { "Plane",      AddPlane,           NULL, ICONO(IconType::plane) },
    { "Cube",       AddCube,            NULL, ICONO(IconType::object) },
    { "Circle",     AddCircle,          NULL, ICONO(IconType::circle) },
    { "UV Sphere",  AddUVSphere,        NULL, ICONO(IconType::circle) },
    { "Cone",       AddCone,            NULL, ICONO(IconType::cono) },
    { "Cylinder",   AddCylinder,        NULL, ICONO(IconType::cilindro) },
    { "Vertex",     AddVertex,          NULL, ICONO(IconType::mesh) },
    { "Reference",  AddReference,       NULL, ICONO(IconType::textura) },
};

// EL MENU ADD, de una mirada: texto + accion + icono por fila. Agregar una primitiva = una linea en
// ADD_MESHES, y ya queda conectada (no hay switch que actualizar ni id que inventar).
static const MenuDef ADD[] = {
    { "Mesh",       NULL,               NULL, ICONO(IconType::mesh),   &MenuMallas },
    { "Empty",      AddEmpty,           NULL, ICONO(IconType::empty) },
    { "LOD",        AddLOD,             NULL, ICONO(IconType::array) },
    { "Culling",    AddCulling,         NULL, ICONO(IconType::visible) },
    { "Particles",  AddParticulas,      NULL, ICONO(IconType::circle) },
    { "Armature",   AddArmature,        NULL, ICONO(IconType::armature) },
    { "Camera",     AddCamera,          NULL, ICONO(IconType::camera) },
    { "Light",      AddLight,           NULL, ICONO(IconType::light) },
    { "Collection", AddCollection,      NULL, ICONO(IconType::archive) },
    { "UI",         AddUI,              NULL, ICONO(IconType::textura) },
    { "Imports",    NULL,               NULL, ICONO(IconType::mesh),   &MenuImports },
};
// (aca estaba la fila "Script", que creaba el objeto Script. Se dio de baja: cualquier
//  objeto acepta scripts desde la pestania "Scripts" del panel.)

// arma MenuAdd + sus submenus (Mesh e Imports) desde las tablas. Lo llama ViewPort3D al crear la barra.
void LayoutConstruirMenuAdd(){
    if (!MenuAdd || !MenuImports || !MenuMallas) return;
    MenuImports->Construir(ADD_IMPORTS, (int)(sizeof(ADD_IMPORTS)/sizeof(ADD_IMPORTS[0])));
    MenuMallas->Construir(ADD_MESHES, (int)(sizeof(ADD_MESHES)/sizeof(ADD_MESHES[0])));
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
    // el snap PROYECTA a pantalla las mallas candidatas y compara con el mouse, asi que tiene que
    // ver EXACTAMENTE lo que 'vp' dibuja: la matriz de mundo de un objeto con billboard (propio o
    // de un ancestro) depende de cual fue el ultimo W3dVistaBind, y con dos viewports abiertos el
    // ultimo que dibujo NO es donde esta el mouse. Sin este bind el iman se enganchaba a una
    // proyeccion de la OTRA vista y el punto que devuelve -que despues se escribe en pos- salia de
    // una camara que el usuario ni esta mirando.
    vp->BindVista();
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
                { bool rango=true; for (int k=0;k<nc;k++) if (idx[k]<0||idx[k]>=m->vertexSize){rango=false;break;}
                  if (!rango) continue; }   // faces3d desincronizado con vertex[] (defensivo, como la rama EDGE)
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

// Separate (P en Edit Mode / menu Mesh > Separate): mueve las caras seleccionadas a un mesh NUEVO
// (misma transform + materiales + vertex groups) y las borra del actual. Como Blender P > Selection.
void LayoutSepararEdit() {
    if (InteractionMode != EditMode || !g_editMesh) return;
    if (SepararSeleccionEdit((Mesh*)g_editMesh)) g_redraw = true;
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
    // "desde la vista" es LITERAL: las UV salen de proyectar con la camara de ESTE viewport, asi
    // que la matriz de mundo tiene que ser la que ve ese mismo viewport (con dos abiertos, la
    // efectiva la publica el que dibujo ultimo, que puede ser el otro). Ver Viewport3D::BindVista.
    Viewport3DActive->BindVista();
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

// ===================================================================================================
//  INSERT KEYFRAME: UN SOLO punto de entrada, cuatro contextos.
//  Antes cada camino tenia su tecla y su item de menu por separado (y el case 510 ya hacia medio
//  dispatch a mano). Ahora la 'i' y todos los "Insert Keyframe" de los menus pasan por aca:
//
//    Pose Mode (huesos 3D)      -> PoseInsertKeyframe(canales)        [con menu de canales]
//    UV en modo POSE (huesos 2D)-> InsertarKeyframeArm2D(mesh,canales)[con menu de canales]
//    Edit Mode (vertices)       -> VertexAnimInsertarKeyframe()       [DIRECTO, sin menu]
//    UV en modo EDICION         -> VertexAnimInsertarKeyframeUV()     [DIRECTO, sin menu]
//    Object Mode                -> InsertarKeyframeObjeto(canales)    [con menu de canales]
//
//  POR QUE los vertices NO abren el menu: el menu elige entre localizacion /
//  rotacion / escala, y una pose de VERTICES no tiene esos canales - es UNA sola cosa (el array de
//  posiciones, o el de normales, o el de UV). Abrir un menu de tres opciones donde solo hay una
//  seria un click de mas por keyframe, justo en el flujo donde mas keyframes se insertan.
// ===================================================================================================
// que contexto manda AHORA: 1=pose 3D, 2=pose 2D (UV), 3=vertices (Edit Mode), 4=UV, 0=objeto.
// 'm2d' sale con la malla del rig 2D cuando devuelve 2.
// 'desdeUV' = el pedido viene del UV EDITOR (su tecla I o su menu Animation). Es un dato que NO se
// puede deducir del estado global: con el 3D en Edit Mode y el UV abierto en edicion, la I del 3D
// keyframea VERTICES y la del UV keyframea el MAPEO - dos cosas distintas, como siempre fue.
static int InsertKeyframeContextoActual(Mesh** m2d, bool desdeUV) {
    if (m2d) *m2d = NULL;
    if (desdeUV) {
        if (UVEditorEnModoPose()) {
            Mesh* m = (ObjActivo && ObjActivo->getType() == ObjectType::mesh) ? (Mesh*)ObjActivo : NULL;
            if (m && !m->Arm2DHuesos().empty()) { if (m2d) *m2d = m; return 2; }
        }
        return 4;   // resto de modos del UV: la capa uv de la animacion del objeto
    }
    if (InteractionMode == PoseMode) return 1;
    if (InteractionMode == EditMode && g_editMesh) return 3;
    return 0;
}
void InsertarKeyframeContexto(int canales, bool desdeUV) {
    extern void VertexAnimInsertarKeyframe();
    extern void VertexAnimInsertarKeyframeUV();
    Mesh* m2d = NULL;
    switch (InsertKeyframeContextoActual(&m2d, desdeUV)) {
        case 1: PoseInsertKeyframe(canales); break;
        case 2: {
            // el clip 2D nace al insertar el primer keyframe (como el clip del armature 3D): si el
            // timeline todavia no lo estaba mostrando, se lo pone activo (kind 4) para poder verlo.
            extern void AnimSelArm2D(Mesh*, int);
            InsertarKeyframeArm2D(m2d, canales);
            if (m2d && (ActiveAnimKind != 4 || ActiveAnimMesh != m2d)) AnimSelArm2D(m2d, m2d->Arm2DAnimActiva());
            if (m2d) m2d->skinGeomVersion++;
            break;
        }
        case 3: VertexAnimInsertarKeyframe(); break;   // canales NO aplica (ver comentario de arriba)
        case 4: VertexAnimInsertarKeyframeUV(); break; // idem
        default: InsertarKeyframeObjeto(canales); break;
    }
    g_redraw = true;
}
// El menu desplegable de canales (Todos / Localizacion / Rotacion / Escala). Lo abre la 'i' y el
// item "Insert Keyframe" de los menus Animation/Pose. En los contextos de VERTICES/UV no abre nada:
// inserta DIRECTO (que es lo que hacia siempre).
PopupMenu* MenuInsertKey = NULL;
// El item "Insert Keyframe" de un menu (Animation del 3D, Pose, Animation del UV) abre el submenu
// de canales SOLO donde los canales existen. En vertices/UV queda PLANO (click = inserta), que es
// justamente el pedido 3: ahi no hay Loc/Rot/Scl que elegir.
void LayoutSyncInsertKeySubmenu(PopupMenu* menu, int idxItem, bool desdeUV) {
    if (!menu || idxItem < 0 || idxItem >= (int)menu->items.size()) return;
    int ctx = InsertKeyframeContextoActual(NULL, desdeUV);
    menu->items[idxItem]->submenu = (ctx == 3 || ctx == 4) ? NULL : MenuInsertKey;
}
void LayoutMenuInsertKeyframe(int mx, int my, bool desdeUV) {
    int ctx = InsertKeyframeContextoActual(NULL, desdeUV);
    if (ctx == 3 || ctx == 4) { InsertarKeyframeContexto(KfCanalTodos, desdeUV); return; } // vertices/UV: sin menu
    if (!MenuInsertKey) return;
    if (!MenuInsertKey->action) MenuInsertKey->action = LayoutAccionObject;
    if (MenuAbierto && MenuAbierto != MenuInsertKey) MenuAbierto->Cerrar();
    MenuInsertKey->Abrir(mx, my, MenuPantallaW, MenuPantallaH);
    MenuAbierto = MenuInsertKey;
}

// opcion del menu Object/Mesh y su submenu Transform (ids 100-102, 300=extrude)
void LayoutAccionObject(int aId) {
    switch (aId) {
        case 1: DuplicatedObject(); break; // Duplicate Objects (Shift D)
        case 2: NewInstance();      break; // Duplicate Linked  (Alt D)
        case 3: AbrirConfirmarBorrado(); break; // Delete (X): popup de confirmacion -> Si borra (con undo)
        case 5: JoinObjetos(); break;           // Join (Ctrl J): une las mallas seleccionadas en el objeto activo
        // Insert Keyframe: el dispatch por CONTEXTO vive en InsertarKeyframeContexto (arriba).
        // 510 se conserva como ALIAS de "todos los canales" (era el id plano del menu Animation y
        // lo puede seguir mandando cualquier caller viejo); 530-533 son los items del desplegable.
        case 510:
        case 530: InsertarKeyframeContexto(KfCanalTodos); break; // Todos (localizacion+rotacion+escala)
        case 531: InsertarKeyframeContexto(KfCanalLoc);   break; // Solo localizacion
        // CAPAS de la animacion del objeto (submenu "Insert Keyframe Layer"): explicitas, sin depender
        // del editor desde el que se inserta. Los tres son la misma animacion, distinta capa.
        case 540: { extern void VertexAnimInsertarKeyframe();          VertexAnimInsertarKeyframe();          break; }
        case 541: { extern void VertexAnimInsertarKeyframeNormales();  VertexAnimInsertarKeyframeNormales();  break; }
        case 542: { extern void VertexAnimInsertarKeyframeUV();        VertexAnimInsertarKeyframeUV();        break; }
        case 532: InsertarKeyframeContexto(KfCanalRot);   break; // Solo rotacion
        case 533: InsertarKeyframeContexto(KfCanalScl);   break; // Solo escala
        case 511: BorrarKeyframeObjeto();   break; // Object > Animation: Delete Keyframe (del frame actual)
        case 512: LimpiarKeyframeObjeto();  break; // Animation: Clear Keyframe (toda la animacion del objeto)
        case 513: g_redraw = true; break;          // Animation > Motion Trail: el checkbox ya lo toggleo PopupMenu
                                                   // (y a proposito NO cierra el menu)
        case 514: {                                // Animation: Delete Normals Layer (capa de la vertex anim activa)
            extern void VertexAnimBorrarCapaNormalesActiva();
            VertexAnimBorrarCapaNormalesActiva();
            break;
        }
        case 220: AplicarTransform(0); break;   // Apply Location
        case 221: AplicarTransform(1); break;   // Apply Rotation
        case 222: AplicarTransform(2); break;   // Apply Scale
        case 223: AplicarTransform(3); break;   // Apply All Transforms
        case 100: if (InteractionMode==PoseMode) PoseXformStart(1); else if (!EditXformStart(translacion, ViewAxis)) SetPosicion(); break; // Move  (G)
        case 101: if (InteractionMode==PoseMode) PoseXformStart(2); else if (!EditXformStart(rotacion,    ViewAxis)) SetRotacion(); break; // Rotate(R)
        case 102: if (InteractionMode==PoseMode) PoseXformStart(3); else if (!EditXformStart(EditScale,   XYZ))      SetEscala();   break; // Scale (S)
        case 500: InsertarKeyframeContexto(KfCanalTodos); break; // Pose Mode: Insert Keyframe (alias de "todos")
        case 520: PoseClearTransform(0); break; // Pose Mode: Clear Transform > All (T+R+S de los huesos seleccionados)
        case 521: PoseClearTransform(1); break; // Clear Translation (Alt+G)
        case 522: PoseClearTransform(2); break; // Clear Rotation (Alt+R)
        case 523: PoseClearTransform(3); break; // Clear Scale (Alt+S)
        case 103: LayoutShrinkFatten(); break; // Shrink/Fatten (Alt+S): cada vert por su normal
        case 300: LayoutExtrudeFaces(); break; // Extrude (segun el modo) (E)
        case 310: LayoutNewFaceEdit(); break;  // Vertex > New Edge/Face from Vertices (F)
        case 314: LayoutDuplicarEdit(); break; // Duplicate (Shift D)
        case 316: LayoutSepararEdit();  break; // Separate (P): caras selec -> mesh nuevo
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
        case 203: OlvidarOrigenSeleccionadas(); break; // Set Origin > Clear Original File (borra Mesh::origen)
        // Set Parent (Ctrl P) / Clear Parent (Ctrl Alt P): submenus de Object + standalone. Ids unicos ->
        // despachan por ESTA accion sea como submenu (menu top = Object) o standalone (menu top = el propio).
        case 230: case 231: case 232: case 233: AccionSetParent(aId - 230); break; // Object / Keep T. / Without Inv. / Keep T. Without Inv.
        case 240: case 241: case 242:           AccionClearParent(aId - 240); break; // Clear / Clear+Keep T. / Clear Inverse
        case 380: case 381: case 382: case 383: AccionMerge(aId - 380); break; // Merge: At Center / At Cursor / Collapse / By Distance
        // EDIT MODE de ARMATURE (Fase 3): menu de contexto "Armature" (mismas acciones que E / Shift+D / X)
        case 600: if (BoneEditActivo()) BoneEditExtruirInteractivo(BoneEditArm(), true);  break; // Extrude Bone
        case 601: if (BoneEditActivo()) BoneEditDuplicarInteractivo(BoneEditArm(), true); break; // Duplicate Bones
        case 602: if (BoneEditActivo()) BoneEditBorrar(BoneEditArm());              break; // Delete Bones
        case 604: if (BoneEditActivo() && !BoneGrabActivo()) BoneXformStart(BoneEditArm(), 1); break; // Move (G)
        case 605: if (BoneEditActivo() && !BoneGrabActivo()) BoneXformStart(BoneEditArm(), 2); break; // Rotate (R)
        case 606: if (BoneEditActivo() && !BoneGrabActivo()) BoneXformStart(BoneEditArm(), 3); break; // Scale (S)
        // (607 Set Parent y 608 Clear Parent son SUBMENUS de 2 opciones: despachan por su action
        //  propia en Parent.cpp -Keep Offset/Connected y Disconnect Bone/Clear Parent-, no por aca.
        //  El viejo 603 "Disconnect from Parent" quedo absorbido por ese submenu.)
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
    if (BoneXformModo()) BoneXformReaplicar();                    // G/R/S de huesos: su propio re-aplicar
    else if (estado != editNavegacion) ReestablecerEstado(false);
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
    // CONSTRAINTS + EDIT MODE: el evaluador del Core necesita saber CUAL objeto se esta editando
    // para poder saltear los constraints que tienen apagado "ver en modo edicion" (W3dConstraint.h).
    // Va por la misma puerta que g_editMesh -que es la unica- para que no puedan quedar desfasados.
    W3dConSetObjEditando(g_editMesh);
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
        // genValido=false y el retry se disparaba CADA FRAME (el 'modgen' subia sin parar al orbitar un FBX).
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
    int modoPrevio = InteractionMode;
    if (ObjActivo && ObjActivo->getType() == ObjectType::mesh)
        InteractionMode = aId;
    else if (ObjActivo && ObjActivo->getType() == ObjectType::armature)
        InteractionMode = (aId == PoseMode || aId == EditMode) ? aId : ObjectMode; // Object/Edit/Pose
    else
        InteractionMode = ObjectMode;
    // al SALIR del Edit de huesos: cerrar un grab a medio hacer + preparar el skin autorado
    // (PrepararSkinAutorado) para que el rig recien editado DEFORME la malla sin pasos extra.
    if (modoPrevio == EditMode && InteractionMode != EditMode &&
        ObjActivo && ObjActivo->getType() == ObjectType::armature){
        if (BoneGrabActivo()) BoneGrabCancelar();
        PrepararSkinAutorado((Armature*)ObjActivo);
    }
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
        gMenuTipo->Agregar(T("2D Editor"), 5);
        gMenuTipo->Agregar("Console", 6);
        gMenuTipo->Agregar("IDE", 7);
        if (aVp != rootViewport) gMenuTipo->Agregar(T("Expand"), 20);
        // OJO nombres: dividir "en columnas" = 2 paneles LADO A LADO (ViewportRow);
        // "en filas" = APILADOS (ViewportColumn). Antes decia Fila/Columna al reves.
        gMenuTipo->Agregar(T("Split in Columns"), 21);
        gMenuTipo->Agregar(T("Split in Rows"), 22);
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
static UVEditor*  gMenuUVDe = NULL;   // el editor cuyo boton View abrio el menu (para el encuadre)
// ENCUADRAR: los ids 10/11 son los unicos items con ACCION del menu View del UV (el resto son
// checkboxes que se togglean solos). Mismo label y mismo atajo que el 3D / el Editor 2D / el
// Timeline: "Frame Selected" (Num .) y "Frame All".
static void LayoutAccionUVView(int id) {
    if (!gMenuUVDe) return;
    if (id == 10)      gMenuUVDe->EncuadrarUV(false);
    else if (id == 11) gMenuUVDe->EncuadrarUV(true);
}
static void LayoutAbrirMenuUV(UVEditor* uv, int x, int y) {
    if (!uv) return;
    gMenuUVDe = uv;
    // REGLA DE DISENO de los titulos: un menu que se abre desde algo SIN TEXTO (un icono, o un atajo de
    // teclado) lleva titulo -- es lo unico que te dice que estas mirando. Si lo abre un boton/item que YA
    // decia el texto, NO lleva: repetirlo es ruido. El boton View ahora es un icono -> titulo.
    if (!gMenuUV){ gMenuUV = new PopupMenu(); gMenuUV->titulo = T("View"); }
    gMenuUV->Limpiar();
    // los togglea el propio item (AgregarCheck sobre el bool* del UV editor)
    // Sync Selection (semantica de Blender, documentada entera en UVEditor.h): OFF (DEFAULT) = el
    // 3D FILTRA que caras se ven y la seleccion del UV es PROPIA y POR RENDER-VERT (una copia de
    // una costura no arrastra a sus hermanas); ON = ves todo el mapa y la seleccion ESPEJA la del
    // mesh (ahi un click SI agarra las 3 copias de un vertice 3D). El cambio de estado lo detecta
    // solo el editor (UVEditor::SincronizarFiltro3D re-inicializa al vuelo).
    gMenuUV->AgregarCheck(T("Sync Selection"), 0, &uv->syncSelection);
    gMenuUV->AgregarCheck(T("Repeat Texture"), 1, &uv->repeatTexture);
    gMenuUV->AgregarCheck(T("Show Chrome UV"), 2, &uv->mostrarChromeUV); // overlay LIVE del reflejo equirect (demo)
    // ENCUADRAR (el UV era el UNICO viewport sin esto): la seleccion, o todo el mapa visible.
    gMenuUV->Agregar(T("Frame Selected"), 10)->atajo = "Num .";
    gMenuUV->Agregar(T("Frame All"), 11);
    gMenuUV->action = LayoutAccionUVView;
    if (MenuAbierto && MenuAbierto != gMenuUV) MenuAbierto->Cerrar();
    gMenuUV->Abrir(x, y, MenuPantallaW, MenuPantallaH);
    MenuAbierto = gMenuUV;
}
// ===== menu del boton "Cursor" del UV editor (ops del cursor 2D <-> seleccion).
// OJO CON EL NOMBRE: en el viewport 3D "Snap" es el IMAN (BR_Snap / g_snap) y ESTAS ops viven en
// Mesh > Snap (Shift+S). Tener un boton "Snap" en el UV que hacia otra cosa era el gap de
// consistencia que reporto la auditoria -> el boton se llama "Cursor" y el menu tambien. =====
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
    gMenuUVSnap->titulo = T("Cursor");
    // (los mismos labels que el submenu Mesh > Snap del 3D)
    gMenuUVSnap->Agregar(T("Cursor to Selection"), 0);
    gMenuUVSnap->Agregar(T("Selection to Cursor"), 1);
    gMenuUVSnap->Agregar(T("Cursor to Center"), 2);
    gMenuUVSnap->action = LayoutAccionUVSnap;
    if (MenuAbierto && MenuAbierto != gMenuUVSnap) MenuAbierto->Cerrar();
    gMenuUVSnap->Abrir(x, y, MenuPantallaW, MenuPantallaH);
    MenuAbierto = gMenuUVSnap;
}

// ===== menu SELECT del UV editor: CALCADO del menu Select del viewport 3D (mismos labels, mismos
// atajos y hasta los mismos ids: All=0 / None=1 / Invert=2 / Select Linked=15) pero operando sobre
// lo que edita el UV. Es CONTEXTUAL segun el modo del editor:
//   - EDICION DE UVs  -> la seleccion propia del UV (uvSelVert), respetando SIEMPRE el filtro del
//                        3D: lo que no se ve no se selecciona (ni con All ni con Invert ni con la isla).
//   - Edit Bones / Pose -> los HUESOS 2D del mesh; "Select Linked" es la CADENA conectada del arbol.
//   - Weight Paint     -> el boton NO se muestra (SyncBarra lo oculta): ahi se pinta, no se selecciona.
// =====
static PopupMenu* gMenuUVSelect = NULL;
static UVEditor*  gUVSelectDe = NULL;   // el editor cuyo boton abrio el menu
static void LayoutAccionUVSelect(int id) {
    UVEditor* uv = gUVSelectDe;
    if (!uv) return;
    Mesh* m = (ObjActivo && ObjActivo->getType() == ObjectType::mesh) ? (Mesh*)ObjActivo : NULL;
    if (!m || (Object*)m != g_editMesh) return;
    const bool huesos = (uv->uvModo == UVModoHuesos || uv->uvModo == UVModoPose);
    // Ctrl+Z: SOLO se captura cuando la accion escribe la seleccion del 3D (Sync Selection ON),
    // que es la que el undo sabe guardar -- exactamente lo que hace el menu Select del 3D
    // (UndoCapturarSeleccionEdit). Fuera de sync la seleccion del UV (uvSelVert) y la de los huesos
    // 2D son ESTADO TRANSITORIO del viewport: no hay comando de undo para ellas y tampoco lo tenian
    // el A / Alt+A ni el pick, asi que no se inventa uno nuevo aca (seria el unico que va al stack).
    if (!huesos && uv->syncSelection) UndoCapturarSeleccionEdit(m);
    if (huesos) {
        switch (id) {
            case 0:  uv->Bone2DSeleccionarTodos(m, true);  break;   // All    (A)
            case 1:  uv->Bone2DSeleccionarTodos(m, false); break;   // None   (Alt A)
            case 2:  uv->Bone2DInvertirSeleccion(m);       break;   // Invert (Ctrl I)
            // Select Linked (L) = la CADENA del hueso seleccionado/activo. Sin nada de donde
            // arrancar no hay cadena: se avisa en vez de no hacer nada en silencio.
            case 15: if (!uv->Bone2DSeleccionarVinculado(m, false))
                         Notificar(T("Select Linked: select a bone first"), false);
                     break;
        }
    } else {
        switch (id) {
            case 0:  uv->SeleccionarTodoUV(m, true);  break;        // All    (A)
            case 1:  uv->SeleccionarTodoUV(m, false); break;        // None   (Alt A)
            case 2:  uv->InvertirSeleccionUV(m);      break;        // Invert (Ctrl I)
            // Select Linked desde el MENU: la semilla es lo que YA esta seleccionado (-1). La
            // tecla L, en cambio, arranca de lo que hay bajo el cursor (como la L del 3D).
            case 15: if (!uv->UVSeleccionarVinculado(m, -1, false))
                         Notificar(T("Select Linked: select a UV first (or press L over the island)"), false);
                     break;
        }
    }
    g_redraw = true;
}
static void LayoutAbrirMenuUVSelect(UVEditor* uv, int x, int y) {
    if (!uv) return;
    gUVSelectDe = uv;
    if (!gMenuUVSelect) {
        // (regla de los titulos: el boton es un icono sin texto -> el menu lleva titulo)
        gMenuUVSelect = new PopupMenu();
        gMenuUVSelect->titulo = T("Select");
        gMenuUVSelect->action = LayoutAccionUVSelect;
        gMenuUVSelect->Agregar(T("All"), 0)->atajo = "A";
        gMenuUVSelect->Agregar(T("None"), 1)->atajo = "Alt A";
        gMenuUVSelect->Agregar(T("Invert"), 2)->atajo = "Ctrl I";
        gMenuUVSelect->Agregar(T("Select Linked"), 15)->atajo = "L";
    }
    if (MenuAbierto && MenuAbierto != gMenuUVSelect) MenuAbierto->Cerrar();
    gMenuUVSelect->Abrir(x, y, MenuPantallaW, MenuPantallaH);
    MenuAbierto = gMenuUVSelect;
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
    // UV editor muestra esa textura + las UV de esa parte. (el dropdown es de texturas, no de materiales).
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

// ---- barra del EDITOR 2D: boton [1] "Add" -> menu de elementos 2D --------------------------
static PopupMenu* gMenu2DAdd = NULL;

// el UI donde va a caer el elemento nuevo: el del objeto activo (si estas parado en un texto o
// en un UI) o el primero de la escena. Si no hay ninguno se CREA: los elementos 2D viven si o
// si dentro de un UI.
static UI* UIRaizParaAgregar() {
    for (Object* o = ObjActivo; o; o = o->Parent)
        if (o->getType() == ObjectType::ui) return (UI*)o;
    if (SceneCollection)
        for (size_t i = 0; i < SceneCollection->Childrens.size(); i++)
            if (SceneCollection->Childrens[i]->getType() == ObjectType::ui)
                return (UI*)SceneCollection->Childrens[i];
    return new UI(NULL);
}

// crear la IMAGEN recien cuando se eligio el archivo (cancelar el explorador = no crear);
// nace con la textura puesta y su tamano real
static void Add2DImagenElegida(const std::string& ruta) {
    UI* raiz = UIRaizParaAgregar();
    Imagen2D* img = new Imagen2D(raiz, Vector3(0.0f, 0.0f, 0.0f));
    img->textura = ruta;
    int w = 0, h = 0;
    Textura2DObtener(ruta, &w, &h);
    if (w > 0 && h > 0) { img->ancho = (float)w; img->alto = (float)h; }
    DeseleccionarTodo();
    img->Seleccionar();
    g_redraw = true;
}
// idem para el VIDEO (la preview ademas trae el tamano real via ffprobe)
static void Add2DVideoElegido(const std::string& ruta) {
    UI* raiz = UIRaizParaAgregar();
    Video2D* v = new Video2D(raiz, Vector3(0.0f, 0.0f, 0.0f));
    v->video = ruta;
    const VideoPreview* pv = Video2DPreview(ruta);
    if (pv && pv->anchoReal > 0) { v->ancho = (float)pv->anchoReal; v->alto = (float)pv->altoReal; }
    DeseleccionarTodo();
    v->Seleccionar();
    g_redraw = true;
}

static void LayoutAccion2DAdd(int id) {
    // imagen y video piden PRIMERO el archivo (con vista previa); cancelar no crea nada
    if (id == 1) { AbrirFileBrowser("Cargar imagen", T("Open"), ".png .jpg .jpeg .webp .bmp .tga .gif", Add2DImagenElegida); return; }
    if (id == 8) { AbrirFileBrowser("Cargar video", T("Open"), ".mp4 .webm .gif .mov .avi", Add2DVideoElegido); return; }
    UI* raiz = UIRaizParaAgregar();
    // posicion CERO: con el ancla en el centro (default) cero = centrado en la ventana
    Object* nuevo = NULL;
    if (id == 0) nuevo = new Texto2D(raiz, Vector3(0.0f, 0.0f, 0.0f));
    if (id == 2) nuevo = new Rect2D(raiz, Vector3(0.0f, 0.0f, 0.0f));
    if (id == 3) nuevo = new Contenedor2D(raiz, Vector3(0.0f, 0.0f, 0.0f));
    if (id == 4) nuevo = new Slice9(raiz, Vector3(0.0f, 0.0f, 0.0f));
    if (id == 5) nuevo = new Boton2D(raiz, Vector3(0.0f, 0.0f, 0.0f));
    if (id == 6) nuevo = new Expandir2D(raiz, Vector3(0.0f, 0.0f, 0.0f));
    if (id == 7) { AddImportUI(); return; }   // importar un .w3dui (mismo flujo que Imports)
    if (!nuevo) return;
    DeseleccionarTodo();
    nuevo->Seleccionar();
    g_redraw = true;
}

static void LayoutAbrirMenu2DAdd(int x, int y) {
    if (!gMenu2DAdd) gMenu2DAdd = new PopupMenu();
    gMenu2DAdd->Limpiar();
    gMenu2DAdd->titulo = T("Add");
    gMenu2DAdd->Agregar(T("Text"), 0, (int)IconType::lista);
    gMenu2DAdd->Agregar(T("Image"), 1, (int)IconType::foto);
    gMenu2DAdd->Agregar(T("Rectangle"), 2, (int)IconType::plane);
    gMenu2DAdd->Agregar(T("Container"), 3, (int)IconType::carpeta);
    gMenu2DAdd->Agregar("Slice 9", 4, (int)IconType::cuadricula);
    gMenu2DAdd->Agregar(T("Button"), 5, (int)IconType::object);
    gMenu2DAdd->Agregar(T("Expand"), 6, (int)IconType::arrowRight);
    gMenu2DAdd->Agregar("Video", 8, (int)IconType::camera);
    gMenu2DAdd->Agregar("Importar w3dui", 7, (int)IconType::archive);
    gMenu2DAdd->action = LayoutAccion2DAdd;
    if (MenuAbierto && MenuAbierto != gMenu2DAdd) MenuAbierto->Cerrar();
    gMenu2DAdd->Abrir(x, y, MenuPantallaW, MenuPantallaH);
    MenuAbierto = gMenu2DAdd;
}

static PopupMenu* gMenu2DPivot = NULL;
static Editor2D*  gMenu2DPivotDe = NULL;   // el editor cuyo boton abrio el menu

static void LayoutAccion2DPivot(int id) {
    if (gMenu2DPivotDe) { gMenu2DPivotDe->pivotModo = id; g_redraw = true; }
}
static void LayoutAbrirMenu2DPivot(Editor2D* ed, int x, int y) {
    if (!gMenu2DPivot) { gMenu2DPivot = new PopupMenu(); gMenu2DPivot->action = LayoutAccion2DPivot; }
    gMenu2DPivotDe = ed;
    gMenu2DPivot->Limpiar();
    gMenu2DPivot->titulo = T("Pivot");
    gMenu2DPivot->Agregar(T("Median Point"),   0, (int)IconType::pivotMedian);
    gMenu2DPivot->Agregar(T("Active Element"), 1, (int)IconType::pivotActive);
    gMenu2DPivot->Agregar(T("2D Cursor"),      2, (int)IconType::pivotCursor);
    if (MenuAbierto && MenuAbierto != gMenu2DPivot) MenuAbierto->Cerrar();
    gMenu2DPivot->Abrir(x, y, MenuPantallaW, MenuPantallaH);
    MenuAbierto = gMenu2DPivot;
}

// ---- menu Seleccionar del Editor 2D (solo elementos 2D) ----
static PopupMenu* gMenu2DSelect = NULL;
static void LayoutAccion2DSelect(int id) { Editor2DSeleccionar(id); }
static void LayoutAbrirMenu2DSelect(int x, int y) {
    if (!gMenu2DSelect) {
        gMenu2DSelect = new PopupMenu();
        gMenu2DSelect->titulo = T("Select");
        gMenu2DSelect->action = LayoutAccion2DSelect;
        gMenu2DSelect->Agregar(T("All"), 0)->atajo = "A";
        gMenu2DSelect->Agregar(T("None"), 1)->atajo = "Alt A";
        gMenu2DSelect->Agregar(T("Invert"), 2)->atajo = "Ctrl I";
    }
    if (MenuAbierto && MenuAbierto != gMenu2DSelect) MenuAbierto->Cerrar();
    gMenu2DSelect->Abrir(x, y, MenuPantallaW, MenuPantallaH);
    MenuAbierto = gMenu2DSelect;
}

// ---- menu Objeto del Editor 2D: Transformar (G/R/S) + duplicar/emparentar/borrar ----
static PopupMenu* gMenu2DObj = NULL;
static PopupMenu* gMenu2DXform = NULL;
static Editor2D* gEditor2DMenu = NULL;   // el editor que abrio el menu (para G/R/S)
static void LayoutAccion2DObject(int id) {
    Editor2D* ed = gEditor2DMenu;
    switch (id) {
        case 100: if (ed) ed->IniciarXform2D(1); break;   // Move (G)
        case 101: if (ed) ed->IniciarXform2D(2); break;   // Rotate (R)
        case 102: if (ed) ed->IniciarXform2D(3); break;   // Scale (S)
        case 1:   Editor2DDuplicarSeleccion(ed); break;   // Duplicate (Shift D)
        case 40: {   // EMPARENTAR la seleccion 2D al elemento ACTIVO (o al UI activo)
            if (!ObjActivo) break;
            if (!UI2D_EsElemento2D(ObjActivo) && ObjActivo->getType() != ObjectType::ui) break;
            std::vector<Object*> sel = ObjSelects;   // copia: ReparentSimple no toca la seleccion
            ReparentGrupoIniciar();                  // N elementos emparentados = UN Ctrl+Z
            for (size_t i = 0; i < sel.size(); i++)
                if (sel[i] != ObjActivo && UI2D_EsElemento2D(sel[i]))
                    ReparentSimple(sel[i], ObjActivo);
            ReparentGrupoFin();
            g_redraw = true;
        } break;
        case 41: {   // DESEMPARENTAR: cada elemento vuelve a colgar de su UI raiz
            std::vector<Object*> sel = ObjSelects;
            ReparentGrupoIniciar();
            for (size_t i = 0; i < sel.size(); i++) {
                if (!UI2D_EsElemento2D(sel[i])) continue;
                Object* r = sel[i]->Parent;
                while (r && r->getType() != ObjectType::ui) r = r->Parent;
                if (r && sel[i]->Parent != r) ReparentSimple(sel[i], r);
            }
            ReparentGrupoFin();
            g_redraw = true;
        } break;
        case 3: Editor2DBorrarSeleccion(); break;         // Delete (X)
    }
}
static void LayoutAbrirMenu2DObj(Editor2D* ed, int x, int y) {
    gEditor2DMenu = ed;
    if (!gMenu2DObj) {
        gMenu2DXform = new PopupMenu();
        gMenu2DXform->action = LayoutAccion2DObject;
        gMenu2DXform->Agregar(T("Move"), 100)->atajo = "G";
        gMenu2DXform->Agregar(T("Rotate"), 101)->atajo = "R";
        gMenu2DXform->Agregar(T("Scale"), 102)->atajo = "S";
        gMenu2DObj = new PopupMenu();
        gMenu2DObj->titulo = T("Object");
        gMenu2DObj->action = LayoutAccion2DObject;
        gMenu2DObj->Agregar(T("Transform"), 0, -1, gMenu2DXform);
        gMenu2DObj->Agregar(T("Duplicate Objects"), 1)->atajo = "Shift D";
        gMenu2DObj->Agregar(T("Set Parent"), 40);
        gMenu2DObj->Agregar(T("Clear Parent"), 41);
        gMenu2DObj->Agregar(T("Delete"), 3)->atajo = "X";
    }
    if (MenuAbierto && MenuAbierto != gMenu2DObj) MenuAbierto->Cerrar();
    gMenu2DObj->Abrir(x, y, MenuPantallaW, MenuPantallaH);
    MenuAbierto = gMenu2DObj;
}

// ---- menu View del Editor 2D: la vista (zoom) + la vista de juego ----
static PopupMenu* gMenu2DView = NULL;
static Editor2D* gEditor2DView = NULL;
static MenuItem* gItem2DVistaJuego = NULL;   // el check apunta al miembro del viewport: se RE-APUNTA al abrir
static void LayoutAccion2DView(int id) {
    if (id >= 1 && id <= 4) Editor2DZoomExacto(gEditor2DView, id);
    else if (id == 10) Editor2DEncuadrarSeleccion(gEditor2DView);
    else if (id == 11) g_redraw = true;   // vista de juego: el check ya muto ed->vistaJuego
}
static void LayoutAbrirMenu2DView(Editor2D* ed, int x, int y) {
    gEditor2DView = ed;
    if (!gMenu2DView) {
        gMenu2DView = new PopupMenu();
        gMenu2DView->titulo = T("View");
        gMenu2DView->action = LayoutAccion2DView;
        // VISTA DE JUEGO (numpad 0): la UI encajada en el viewport, sin guias ni puntos de
        // agarre, como se veria en una pantalla de esas proporciones (el analogo de la vista
        // de camara del 3D). El estado es POR VIEWPORT, pero el menu es static y lo comparten
        // todos los Editor2D -> el checkbox se RE-APUNTA abajo, en cada apertura.
        gItem2DVistaJuego = gMenu2DView->AgregarCheck(T("Game View"), 11,
                                                      ed ? &ed->vistaJuego : NULL);
        gItem2DVistaJuego->atajo = "Num 0";
        gMenu2DView->Agregar(T("Frame Selected"), 10)->atajo = "Num .";
        gMenu2DView->Agregar("Zoom 1:1", 1);   // pixel-perfect: un px del lienzo = uno real
        gMenu2DView->Agregar("Zoom 2:1", 2);
        gMenu2DView->Agregar("Zoom 3:1", 3);
        gMenu2DView->Agregar("Zoom 4:1", 4);
    }
    // el menu es UNO solo para todos los viewports 2D: el check de "Game View" tiene que
    // apuntar al miembro del viewport que lo abrio (sino togglea el de otro)
    if (gItem2DVistaJuego && ed) gItem2DVistaJuego->checkbox = &ed->vistaJuego;
    if (MenuAbierto && MenuAbierto != gMenu2DView) MenuAbierto->Cerrar();
    gMenu2DView->Abrir(x, y, MenuPantallaW, MenuPantallaH);
    MenuAbierto = gMenu2DView;
}

// click en la barra del Editor 2D: por ROL (BarRol2D), no por indice -> agregar/reordenar
// botones no rompe el dispatch (mismo esquema que la barra del 3D).
static bool LayoutClickBarra2D(Editor2D* ed, int mx, int my) {
    if (!ed) return false;
    ed->ActualizarBarra(); // sx/sy frescos
    std::vector<Button*>& B = ed->BarButtons;
    for (size_t i = 1; i < B.size(); i++) {
        Button* b = B[i];
        if (!b || !b->visible || !b->Contains(mx, my)) continue;
        int bx = b->sx, by = b->sy + b->height - GlobalScale;
        switch (b->rol) {
            case BR2D_Select: LayoutAbrirMenu2DSelect(bx, by);    return true;
            case BR2D_Add:    LayoutAbrirMenu2DAdd(bx, by);       return true;
            case BR2D_Object: LayoutAbrirMenu2DObj(ed, bx, by);   return true;
            case BR2D_View:   LayoutAbrirMenu2DView(ed, bx, by);  return true;
            case BR2D_Pivot:  LayoutAbrirMenu2DPivot(ed, bx, by); return true;
        }
    }
    return false;
}

// ===== barra del IDE (editor de texto lua): selector de script + Save + Refresh =====
static PopupMenu* gMenuIDEScript = NULL;
static IDE* gIDEMenuDe = NULL;                    // el IDE cuyo selector esta abierto
static std::vector<std::string> gIDEScriptRutas;  // rutas del menu (id = indice)
static std::string gIDECambiarA;                  // destino pendiente del popup de confirmar

// "Si" del popup: descartar los cambios y abrir el otro script
static void IDEConfirmarCambio() {
    if (gIDEMenuDe && !gIDECambiarA.empty()) gIDEMenuDe->AbrirArchivo(gIDECambiarA);
    gIDECambiarA.clear();
}

static void LayoutAccionIDEScript(int id) {
    IDE* ide = gIDEMenuDe;
    if (!ide || id < 0 || id >= (int)gIDEScriptRutas.size()) return;
    const std::string& ruta = gIDEScriptRutas[id];
    if (ruta == ide->archivo) return; // ya es el abierto
    if (ide->sucio) {
        // cambiar con cambios sin guardar AVISA: popup Si/No antes de descartar
        gIDECambiarA = ruta;
        if (!confirmarPopup) confirmarPopup = new ConfirmarPopup();
        confirmarPopup->Abrir("Descartar los cambios de " + IDENombreScript(ide->archivo) + "?",
                              IDEConfirmarCambio);
        return;
    }
    ide->AbrirArchivo(ruta);
}

static void LayoutAbrirMenuIDEScript(IDE* ide, int x, int y) {
    if (!ide) return;
    if (!gMenuIDEScript) {
        gMenuIDEScript = new PopupMenu();
        gMenuIDEScript->action = LayoutAccionIDEScript;
    }
    gIDEMenuDe = ide;
    gMenuIDEScript->Limpiar();
    IDEColectarScripts(&gIDEScriptRutas); // escenas + contenido/*.lua, sin repetidos
    if (gIDEScriptRutas.empty()) {
        gMenuIDEScript->Agregar("(no hay scripts .lua)", -1);
    } else {
        for (size_t i = 0; i < gIDEScriptRutas.size(); i++) {
            bool abierto = (gIDEScriptRutas[i] == ide->archivo);
            // el abierto se MARCA: tilde (icono de exito) + texto en verde accent
            MenuItem* it = gMenuIDEScript->Agregar(IDENombreScript(gIDEScriptRutas[i]), (int)i,
                                                   abierto ? (int)IconType::notifOk : -1);
            it->verde = abierto;
        }
    }
    if (MenuAbierto && MenuAbierto != gMenuIDEScript) MenuAbierto->Cerrar();
    gMenuIDEScript->Abrir(x, y, MenuPantallaW, MenuPantallaH);
    MenuAbierto = gMenuIDEScript;
}

// click en la barra del IDE: por ROL (como la barra del 2D), no por indice
bool LayoutClickBarraIDE(IDE* ide, int mx, int my) {
    if (!ide) return false;
    ide->SincronizarBarra();
    ide->ActualizarBarra(); // sx/sy frescos
    std::vector<Button*>& B = ide->BarButtons;
    for (size_t i = 1; i < B.size(); i++) {
        Button* b = B[i];
        if (!b || !b->visible || !b->Contains(mx, my)) continue;
        int bx = b->sx, by = b->sy + b->height - GlobalScale;
        switch (b->rol) {
            case BRIDE_Script:  LayoutAbrirMenuIDEScript(ide, bx, by); return true;
            case BRIDE_Guardar: ide->Guardar(); return true;
            case BRIDE_Refresh: ide->Refresh(); return true;
        }
    }
    return false;
}

// ===== menu "Animation" del UV editor: keyframes de la CURVA UV de la vertex anim ACTIVA
// (kind 3) del mesh en edicion. La curva de UV es SEPARADA de la de vertices ("son cosas
// distintas"): el insert captura SOLO el mapeo (VertexAnimInsertarKeyframeUV, el mismo
// camino que el auto-key del UV y de la pose 2D), y borrar/limpiar sacan SOLO la capa UV
// de los frames (los keyframes de vertices no se tocan desde aca; misma regla que el dope
// sheet: nunca dejar la anim sin frames). =====
static PopupMenu* gMenuUVAnim = NULL;
static void LayoutAccionUVAnim(int id) {
    extern void VertexAnimInsertarKeyframeUV();
    // Insert Keyframe: va por el dispatch por CONTEXTO (en UVModoPose keyframea la POSE 2D en el
    // clip del armature 2D; en edicion de UVs keyframea la capa uv, como siempre). Los ids 530-533
    // llegan aca cuando el usuario elige un canal en el submenu.
    if (id == 0)   { InsertarKeyframeContexto(KfCanalTodos, true); return; }
    if (id == 530) { InsertarKeyframeContexto(KfCanalTodos, true); return; }
    if (id == 531) { InsertarKeyframeContexto(KfCanalLoc,   true); return; }
    if (id == 532) { InsertarKeyframeContexto(KfCanalRot,   true); return; }
    if (id == 533) { InsertarKeyframeContexto(KfCanalScl,   true); return; }
    if (id == 540 || id == 541 || id == 542) { LayoutAccionObject(id); return; } // capas (Vertices/Normales/UV)
    // Delete / Clear: hace falta la vertex anim activa del mesh en edicion
    if (ActiveAnimKind != 3 || !ActiveAnimMesh || (Object*)ActiveAnimMesh != g_editMesh) {
        Notificar(T("Insert Keyframe: select an object animation in the timeline"), true);
        return;
    }
    Mesh* m = ActiveAnimMesh;
    VertexAnimationActive* va = FindTargetAnim(m);
    int ai = va ? va->currentAnim : -1;
    if (ai < 0 || ai >= (int)m->animations.size() || !m->animations[ai]) return;
    VertexAnimation* an = m->animations[ai];
    extern void UndoKeyframesIniciar(); extern void UndoKeyframesConfirmar();
    UndoKeyframesIniciar(); // Ctrl+Z: snapshot de la anim antes de tocar los frames
    if (id == 1) {          // Delete Keyframe: saca la CAPA UV del cuadro actual
        VertexFrame* vf = NULL;
        for (size_t k = 0; k < an->frames.size(); ++k)
            if (an->frames[k]->frame == CurrentFrame) { vf = an->frames[k]; break; }
        if (!vf || !vf->uvs)
            Notificar(T("Delete Keyframe: no keyframe at the current frame"), true);
        else if (!vf->positions && (int)an->frames.size() <= 1)
            Notificar(T("Delete Keyframe: the animation needs at least one keyframe"), true);
        else
            VertexAnimBorrarCapa(*an, CurrentFrame, 1);
    } else if (id == 2) {   // Clear Keyframes: saca la capa UV de TODOS los frames
        for (size_t k = an->frames.size(); k-- > 0; ) {
            VertexFrame* vf = an->frames[k];
            if (!vf->uvs) continue;
            if (!vf->positions && (int)an->frames.size() <= 1) continue; // dejar 1 frame vivo
            VertexAnimBorrarCapa(*an, vf->frame, 1);
        }
    }
    UndoKeyframesConfirmar();
    m->skinGeomVersion++; // la pose del frame actual pudo cambiar
    g_redraw = true;
}
static void LayoutAbrirMenuUVAnim(UVEditor* uv, int x, int y) {
    if (!uv) return;
    if (!gMenuUVAnim) {
        // (regla de los titulos: el boton es un icono sin texto -> el menu lleva titulo)
        gMenuUVAnim = new PopupMenu();
        gMenuUVAnim->titulo = T("Animation");
        gMenuUVAnim->Agregar(T("Insert Keyframe"), 0, IconType::keyframe);
        // mismas CAPAS que el menu Animation del 3D (la animacion es la misma; cambia la capa)
        static PopupMenu* gMenuUVCapa = NULL;
        gMenuUVCapa = new PopupMenu();
        gMenuUVCapa->Agregar(T("Vertices"), 540, IconType::mesh);
        gMenuUVCapa->Agregar(T("Normals"),  541, IconType::normalVertex);
        gMenuUVCapa->Agregar(T("UV"),       542, IconType::textura);
        gMenuUVAnim->Agregar(T("Insert Keyframe Layer"), 0, IconType::keyframe, gMenuUVCapa);
        gMenuUVAnim->Agregar(T("Delete Keyframe"), 1, IconType::borrar);
        gMenuUVAnim->Agregar(T("Clear Keyframe"),  2);
        gMenuUVAnim->action = LayoutAccionUVAnim;
    }
    LayoutSyncInsertKeySubmenu(gMenuUVAnim, 0, true); // en UVModoPose abre los canales; en edicion queda plano
    if (MenuAbierto && MenuAbierto != gMenuUVAnim) MenuAbierto->Cerrar();
    gMenuUVAnim->Abrir(x, y, MenuPantallaW, MenuPantallaH);
    MenuAbierto = gMenuUVAnim;
}

// ===== selector de MODO del UV editor (Edicion / Pintura de pesos), desplegable como el Mode del
// 3D. Es el modo PROPIO del viewport UV (uv->uvModo), NO el InteractionMode global: pintar pesos
// en el UV no debe sacar al 3D de Edit Mode (decision documentada en UVEditor.h). =====
static PopupMenu* gMenuUVModo = NULL;
static UVEditor*  gUVModoDe = NULL;   // el editor cuyo boton abrio el menu
static void LayoutAccionUVModo(int id) {
    if (!gUVModoDe) return;
    const int modoAntes = gUVModoDe->uvModo;
    // el Tab del UV alterna Edit Bones con el modo del que se VENIA: elegir un modo a mano tambien
    // actualiza esa memoria (sino Tab devolvia a un modo viejo que el usuario ya habia dejado)
    if (id == UVModoHuesos) {
        if (gUVModoDe->uvModo != UVModoHuesos) gUVModoDe->uvModoPrevio = gUVModoDe->uvModo;
    } else {
        gUVModoDe->uvModoPrevio = id;
    }
    gUVModoDe->uvModo = id; // UVModoObjeto / UVModoEdicion / UVModoPesos / UVModoHuesos / UVModoPose
    // el objeto activo del UV sigue al modo elegido a mano (sino el Tab de vuelta entraba a otra cosa)
    if (id == UVModoHuesos || id == UVModoPose) gUVModoDe->uvObjArm = true;
    else if (id == UVModoEdicion)               gUVModoDe->uvObjArm = false;
    else if (id == UVModoObjeto) {
        // entrar a MODO OBJETO deja definido QUE queda seleccionado, igual que el Tab: viniendo de
        // huesos/pose sigue activo el ARMATURE que se estaba editando; de cualquier otro modo
        // (edicion de UVs, pintura de pesos) el objeto activo es la GEOMETRIA. Sin rig 2D, siempre
        // la geometria (no hay armature que seleccionar).
        Mesh* mm = (ObjActivo && ObjActivo->getType() == ObjectType::mesh) ? (Mesh*)ObjActivo : NULL;
        const bool deHuesos = (modoAntes == UVModoHuesos || modoAntes == UVModoPose);
        gUVModoDe->uvObjArm = deHuesos && mm && mm->TieneArm2D();
    }
    // entrar a huesos/pose desde el desplegable tambien lleva el panel a la pestania del rig 2D
    if (id == UVModoHuesos || id == UVModoPose) PropsIrAArmature2D();
    g_redraw = true;
}
static void LayoutAbrirMenuUVModo(UVEditor* uv, int x, int y) {
    if (!uv) return;
    gUVModoDe = uv;
    if (!gMenuUVModo) gMenuUVModo = new PopupMenu();
    gMenuUVModo->Limpiar(); // se rearma para marcar (verde) el modo actual de ESTE editor
    gMenuUVModo->titulo = T("Mode");
    // OBJETO: el modo por defecto del UV (elegir la geometria o un armature 2D con el click)
    gMenuUVModo->Agregar(T("Object Mode"),  UVModoObjeto,  IconType::object)->verde = (uv->uvModo == UVModoObjeto);
    gMenuUVModo->Agregar(T("Edit Mode"),    UVModoEdicion, IconType::mesh)->verde = (uv->uvModo == UVModoEdicion);
    gMenuUVModo->Agregar(T("Weight Paint"), UVModoPesos,   IconType::mesh)->verde = (uv->uvModo == UVModoPesos);
    // modos del ARMATURE 2D DEL MESH: solo si la malla activa TIENE huesos 2D (se crean en Add)
    { Mesh* mm = (ObjActivo && ObjActivo->getType() == ObjectType::mesh) ? (Mesh*)ObjActivo : NULL;
      if (mm && !mm->Arm2DHuesos().empty()) {
          gMenuUVModo->Agregar(T("Edit Bones"), UVModoHuesos, IconType::armature)->verde = (uv->uvModo == UVModoHuesos);
          gMenuUVModo->Agregar(T("Pose Mode"),  UVModoPose,   IconType::armature)->verde = (uv->uvModo == UVModoPose);
      } }
    gMenuUVModo->action = LayoutAccionUVModo;
    if (MenuAbierto && MenuAbierto != gMenuUVModo) MenuAbierto->Cerrar();
    gMenuUVModo->Abrir(x, y, MenuPantallaW, MenuPantallaH);
    MenuAbierto = gMenuUVModo;
}

// ===== menu ADD del UV editor: crea el ARMATURE 2D DEL MESH (huesos en espacio UV que deforman
// los UVs pesados; ver W3dBone2D en el Core) o agrega huesos raiz cuando ya existe. =====
static PopupMenu* gMenuUVAdd = NULL;
static UVEditor*  gUVAddDe = NULL;
static void LayoutAccionUVAdd(int id) {
    if (!gUVAddDe) return;
    Mesh* m = (ObjActivo && ObjActivo->getType() == ObjectType::mesh) ? (Mesh*)ObjActivo : NULL;
    if (!m) return;
    if (id == 0) gUVAddDe->Armature2DCrear(m);   // crea (o entra a editar) el armature 2D
    else if (id == 1) {                          // hueso raiz nuevo (ya en modo huesos)
        gUVAddDe->Bone2DAgregar(m);
        gUVAddDe->uvModo = UVModoHuesos;
    } else if (id == 2) gUVAddDe->Armature2DNuevo(m); // OTRO armature 2D (independiente)
    g_redraw = true;
}
static void LayoutAbrirMenuUVAdd(UVEditor* uv, int x, int y) {
    if (!uv) return;
    gUVAddDe = uv;
    if (!gMenuUVAdd) gMenuUVAdd = new PopupMenu();
    gMenuUVAdd->Limpiar(); // se rearma: "Bone" solo aparece cuando el armature ya existe
    gMenuUVAdd->titulo = T("Add");
    Mesh* m = (ObjActivo && ObjActivo->getType() == ObjectType::mesh) ? (Mesh*)ObjActivo : NULL;
    if (m && m->TieneArm2D()) {
        gMenuUVAdd->Agregar(T("Bone"), 1, IconType::armature);          // hueso en el armature ACTIVO
        gMenuUVAdd->Agregar(T("Armature 2D"), 2, IconType::armature);   // OTRO armature (rig aparte)
    } else gMenuUVAdd->Agregar(T("Armature 2D"), 0, IconType::armature);
    gMenuUVAdd->action = LayoutAccionUVAdd;
    if (MenuAbierto && MenuAbierto != gMenuUVAdd) MenuAbierto->Cerrar();
    gMenuUVAdd->Abrir(x, y, MenuPantallaW, MenuPantallaH);
    MenuAbierto = gMenuUVAdd;
}

// ===== menu ARMATURE del UV editor (solo en UVModoHuesos): las ops de edicion de huesos 2D +
// Set Parent (Ctrl+P) + el submenu Clear Parent de Alt+P (Disconnect Bone / Clear Parent). =====
static PopupMenu* gMenuUVArm = NULL;
static UVEditor*  gUVArmDe = NULL;   // el editor cuyo boton abrio el menu
static void LayoutAccionUVArm(int id) {
    UVEditor* uv = gUVArmDe;
    if (!uv || uv->uvModo != UVModoHuesos) return;
    Mesh* m = (ObjActivo && ObjActivo->getType() == ObjectType::mesh) ? (Mesh*)ObjActivo : NULL;
    if (!m || (Object*)m != g_editMesh) return;
    if (id == 0) {                                   // Extrude: el tail nuevo queda agarrado
        int nb = uv->Bone2DExtruir(m);
        if (nb >= 0) uv->Bone2DDragEnd(m, nb, 2, false);
    } else if (id == 1) {                            // Duplicate: lo duplicado queda agarrado
        if (uv->Bone2DDuplicar(m) >= 0) uv->Bone2DXformStart(m, 1);
    } else if (id == 2) uv->Bone2DBorrar(m);         // Delete
    // (id 3 = Set Parent y 4 = Clear Parent son SUBMENUS de 2 opciones: despachan por su
    //  action propia en Parent.cpp, no por aca)
    g_redraw = true;
}
static void LayoutAbrirMenuUVArmature(UVEditor* uv, int x, int y) {
    if (!uv) return;
    gUVArmDe = uv;
    Mesh* m = (ObjActivo && ObjActivo->getType() == ObjectType::mesh) ? (Mesh*)ObjActivo : NULL;
    BoneAltPContexto2D(m); // el submenu Clear Parent opera sobre los huesos 2D de ESTE mesh
    if (!gMenuUVArm) {
        gMenuUVArm = new PopupMenu();
        gMenuUVArm->titulo = T("Armature");
        gMenuUVArm->action = LayoutAccionUVArm;
        gMenuUVArm->Agregar(T("Extrude"), 0)->atajo = "E";
        gMenuUVArm->Agregar(T("Duplicate"), 1)->atajo = "Shift D";
        gMenuUVArm->Agregar(T("Delete"), 2)->atajo = "X";
        // Set Parent y Clear Parent son SUBMENUS de 2 opciones cada uno (Keep Offset/Connected y
        // Disconnect Bone/Clear Parent): despachan por SU action propia (Parent.cpp)
        gMenuUVArm->Agregar(T("Set Parent"), 3, -1, LayoutSubmenuBoneCtrlP())->atajo = "Ctrl P";
        gMenuUVArm->Agregar(T("Clear Parent"), 4, -1, LayoutSubmenuBoneAltP())->atajo = "Alt P";
    }
    if (MenuAbierto && MenuAbierto != gMenuUVArm) MenuAbierto->Cerrar();
    gMenuUVArm->Abrir(x, y, MenuPantallaW, MenuPantallaH);
    MenuAbierto = gMenuUVArm;
}

// click en la barra del UV editor: por ROL (BarRolUV), no por indice -> agregar/reordenar botones
// no rompe el dispatch. NO static: el comando de test 'uvbar' entra por este mismo camino.
bool LayoutClickBarraUV(UVEditor* uv, int mx, int my) {
    if (!uv) return false;
    uv->ActualizarBarra(); // sx/sy frescos
    std::vector<Button*>& B = uv->BarButtons;
    for (size_t i = 1; i < B.size(); i++) {
        Button* b = B[i];
        if (!b || !b->visible || !b->Contains(mx, my)) continue;
        int bx = b->sx, by = b->sy + b->height - GlobalScale;
        switch (b->rol) {
            case BRUV_Modo:      LayoutAbrirMenuUVModo(uv, bx, by);    return true; // Edicion/Pintura/Huesos/Pose
            case BRUV_Add:       LayoutAbrirMenuUVAdd(uv, bx, by);     return true; // Armature 2D / Bone
            case BRUV_Armature:  LayoutAbrirMenuUVArmature(uv, bx, by); return true; // ops de huesos 2D + Parent
            case BRUV_View:      LayoutAbrirMenuUV(uv, bx, by);        return true; // checkboxes (Sync/Repeat)
            case BRUV_Select:    LayoutAbrirMenuUVSelect(uv, bx, by);  return true; // All/None/Invert/Linked
            case BRUV_SelMode:   LayoutAbrirMenuUVSelMode(uv, bx, by); return true; // Vertex/Edge/Face
            case BRUV_Pivot:     LayoutMenuPivotUV(bx, by);            return true; // variante 2D (sin Active Element)
            case BRUV_Snap:      LayoutAbrirMenuUVSnap(uv, bx, by);    return true;
            case BRUV_Texture:   LayoutAbrirMenuUVTex(uv, bx, by);     return true; // dropdown de texturas
            case BRUV_Animation: LayoutAbrirMenuUVAnim(uv, bx, by);    return true; // keyframes de la vertex anim
        }
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
// TRANSPORTE del juego (Stop / Play-Pausa) de la barra del viewport 3D: es ACCION DIRECTA, asi que
// SOLO se dispara con un CLICK real. Estaba DENTRO de LayoutAbrirMenuDeBarra -- que la llaman tambien el
// hover del mouse y las flechas de navegacion del menu -> pasar POR ENCIMA de Stop reseteaba el juego
// (SimStop). Separado aca: la llama unicamente el path del click. Devuelve true si el punto cae sobre un
// boton de transporte (y ya hizo la accion): el click no sigue al menu. Mismo patron que el Timeline
// (Mover=resalta / Activar=dispara), que ya lo hacia bien.
static bool LayoutTransporteBarra3D(ViewportBase* vp, int mx, int my) {
    if (!vp || !vp->isLeaf() || vp->ViewportKind() != 1) return false;
    std::vector<Button*>& B = vp->BarButtons;
    Button* bs = BarRolBtn(B, BR_JuegoStop);
    Button* bl = BarRolBtn(B, BR_JuegoPlay);
    extern bool SimActiva(); extern void SimStop();
    if (bs && bs->visible && bs->Contains(mx, my)) {
        Viewport3DActive = (Viewport3D*)vp;
        if (SimActiva()) SimStop();       // restaura el estado inicial + descarga scripts
        PlayAnimation = false; g_redraw = true; return true;
    }
    if (bl && bl->visible && bl->Contains(mx, my)) {
        Viewport3DActive = (Viewport3D*)vp;
        if (PlayAnimation) { PlayAnimation = false; }   // pausa
        else {
            // MISMA PUERTA que el Play del timeline: no arrancar con texturas en la cola diferida
            // (el juego salia gris el primer segundo).
            extern bool JuegoEsperarTexturas();
            if (!JuegoEsperarTexturas()) { g_redraw = true; return true; }
            JuegoPrepararViewports(!SimActiva());   // overlays off solo si el juego ARRANCA
            AnimPlayDir = 1; PlayAnimation = true;
        }
        g_redraw = true; return true;
    }
    return false;
}

bool LayoutAbrirMenuDeBarra(ViewportBase* vp, int mx, int my) {
    if (!vp || !vp->isLeaf()) return false;
    // el viewport 3D tiene su cadena propia (abajo); los demas se abren solos por el virtual compartido, asi no
    // hay que repetir aca el if de cada boton de cada panel
    if (vp->ViewportKind() != 1) return vp->AbrirMenuDeBarra(mx, my);
    std::vector<Button*>& B = vp->BarButtons;
    // el menu de la barra de ESTE viewport actua sobre ESTE viewport: sin esto,
    // tras abrir un proyecto en caliente Viewport3DActive podia quedar apuntando
    // al 3D del layout ANTERIOR y View > Viewpoint "no hacia nada"
    Viewport3DActive = (Viewport3D*)vp;
    // (el transporte Stop/Play NO va aca: es accion directa y solo se dispara por click real ->
    //  LayoutTransporteBarra3D, llamada desde el path del click. Ver el comentario de esa funcion.)

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
        if (!MenuAnimation->action) MenuAnimation->action = LayoutAccionObject; // ids 510..513 / 530..533
        LayoutSyncInsertKeySubmenu(MenuAnimation, 0); // "Insert Keyframe": submenu de canales segun el contexto
    } else if (bObj && bObj->visible && bObj->Contains(mx, my)) {
        // Edit Mode -> menu de contexto Vertex/Edge/Face (o "Armature" si se editan huesos);
        // Pose Mode -> menu "Pose"; Object Mode -> menu "Object".
        if (InteractionMode == EditMode) {
            if (MenuAbierto) MenuAbierto->Cerrar();
            if (BoneEditActivo()) LayoutMenuArmEdit(bObj->sx, bObj->sy + bObj->height - GlobalScale);
            else LayoutMenuEditContexto(bObj->sx, bObj->sy + bObj->height - GlobalScale);
            RegistrarMenuBarra(MenuAbierto, bObj);
            return true;
        } else if (InteractionMode == PoseMode) {
            extern PopupMenu* MenuPose;
            if (MenuPose){ objetivo = MenuPose; boton = bObj; if (!MenuPose->action) MenuPose->action = LayoutAccionObject; // reusa ids 100/101/102/500
                           LayoutSyncInsertKeySubmenu(MenuPose, 0); } // "Insert Keyframe" -> submenu de canales
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
        // el else -> abria el menu de tipo/split, que no es lo que se quiere navegar.
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

// MISMO comportamiento que LayoutCambiarMenuBarra pero para el EDITOR UV (su barra tiene menus
// PROPIOS: Modo/View/SelMode/Pivot/Snap/Texture/Animation). Antes la nav izq/der estaba atada a
// Viewport3DActive -> en el UV solo se llegaba al [0] (tipo) y no se podia ir a View/Pivot/Snap
// Mapea por ROL (BarRolUV), como el 3D: el menu abierto dice el rol, BarRolIdx da el
// indice actual -> agregar/reordenar botones no rompe la navegacion por teclado.
static void LayoutCambiarMenuBarraUV(int dir) {
    if (!viewPortActive || !viewPortActive->isLeaf() || viewPortActive->ViewportKind() != 4) return;
    UVEditor* uv = (UVEditor*)viewPortActive;
    std::vector<Button*>& B = uv->BarButtons;
    if (B.size() < 2) return;
    const int maxIdx = (int)B.size() - 1;
    // menu abierto -> ROL del boton que lo abre -> indice ACTUAL de ese boton
    int idx = -1;
    if      (MenuAbierto == gMenuTipo)      idx = 0;
    else {
        int rol = -1;
        if      (MenuAbierto == gMenuUVModo)    rol = BRUV_Modo;
        else if (MenuAbierto == gMenuUVAdd)     rol = BRUV_Add;
        else if (MenuAbierto == gMenuUVArm)     rol = BRUV_Armature;
        else if (MenuAbierto == gMenuUV)        rol = BRUV_View;
        else if (MenuAbierto == gMenuUVSelect)  rol = BRUV_Select;
        else if (MenuAbierto == gMenuUVSelMode) rol = BRUV_SelMode;
        else if (MenuAbierto == gMenuPivot)     rol = BRUV_Pivot;
        else if (MenuAbierto == gMenuUVSnap)    rol = BRUV_Snap;
        else if (MenuAbierto == gMenuUVTex)     rol = BRUV_Texture;
        else if (MenuAbierto == gMenuUVAnim)    rol = BRUV_Animation;
        if (rol >= 0) idx = BarRolIdx(B, rol);
    }
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
    switch (b->rol) {
        case BRUV_Modo:      LayoutAbrirMenuUVModo(uv, mx, my);    break;
        case BRUV_Add:       LayoutAbrirMenuUVAdd(uv, mx, my);     break;
        case BRUV_Armature:  LayoutAbrirMenuUVArmature(uv, mx, my); break;
        case BRUV_View:      LayoutAbrirMenuUV(uv, mx, my);        break;
        case BRUV_Select:    LayoutAbrirMenuUVSelect(uv, mx, my);  break;
        case BRUV_SelMode:   LayoutAbrirMenuUVSelMode(uv, mx, my); break;
        case BRUV_Pivot:     LayoutMenuPivot(mx, my);              break;
        case BRUV_Snap:      LayoutAbrirMenuUVSnap(uv, mx, my);    break;
        case BRUV_Texture:   LayoutAbrirMenuUVTex(uv, mx, my);     break;
        case BRUV_Animation: LayoutAbrirMenuUVAnim(uv, mx, my);    break;
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

// menu de CONTEXTO del EDIT MODE de ARMATURE (Fase 3): Extrude / Duplicate / Delete / Set Parent
// + el submenu Clear Parent (Alt+P: Disconnect Bone / Clear Parent). Se abre desde el boton
// "Armature" de la barra (el rol BR_Object en este modo). Ids 600-607.
static PopupMenu* gMenuArmEdit = NULL;
void LayoutMenuArmEdit(int mx, int my) {
    if (!BoneEditActivo()) return;
    if (!gMenuArmEdit) {
        gMenuArmEdit = new PopupMenu();
        gMenuArmEdit->titulo = "Armature";
        gMenuArmEdit->action = LayoutAccionObject;
        gMenuArmEdit->Agregar(T("Extrude Bone"), 600)->atajo = "E";
        gMenuArmEdit->Agregar(T("Move"), 604)->atajo = "G";
        gMenuArmEdit->Agregar(T("Rotate"), 605)->atajo = "R";
        gMenuArmEdit->Agregar(T("Scale"), 606)->atajo = "S";
        gMenuArmEdit->Agregar(T("Duplicate"), 601)->atajo = "Shift D";
        gMenuArmEdit->Agregar(T("Delete"), 602)->atajo = "X";
        // Set Parent y Clear Parent: submenus de 2 opciones cada uno (Keep Offset/Connected y
        // Disconnect Bone/Clear Parent); despachan por SU action (Parent.cpp), no por los 600s
        gMenuArmEdit->Agregar(T("Set Parent"), 607, -1, LayoutSubmenuBoneCtrlP())->atajo = "Ctrl P";
        gMenuArmEdit->Agregar(T("Clear Parent"), 608, -1, LayoutSubmenuBoneAltP())->atajo = "Alt P";
    }
    BoneAltPContexto3D(); // el submenu Clear Parent opera sobre los huesos 3D en edicion
    gMenuArmEdit->Abrir(mx, my, MenuPantallaW, MenuPantallaH);
    MenuAbierto = gMenuArmEdit;
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

// VARIANTE 2D del menu de pivote (la usa el editor UV). MISMO estado global (g_transformPivot) y
// MISMA accion que el 3D -> no hay dos verdades ni codigo duplicado; lo unico que cambia es QUE se
// ofrece, porque en 2D dos de los items no significan nada:
//   - "Active Element": el editor UV no tiene ELEMENTO ACTIVO (el pick escribe uvSelVert y nada
//     mas). Elegirlo pivoteaba igual en la mediana y el icono de la barra mentia.
//   - "Lock Normals": es del transform de la MALLA en 3D; en el mapa UV no hay normales.
// "Individual Origins" SI se ofrece: en modo CARA el UV lo implementa de verdad (cada cara rota
// alrededor de su centro, ver UVEditor::IniciarXform).
void LayoutMenuPivotUV(int mx, int my) {
    static PopupMenu* gMenuPivotUV = NULL;
    if (!gMenuPivotUV) {
        gMenuPivotUV = new PopupMenu();
        gMenuPivotUV->titulo = T("Transform Pivot Point");
        gMenuPivotUV->action = AccionPivot;
    }
    gMenuPivotUV->Limpiar();
    // el UV llama "3D Cursor" al cursor 2D a proposito: es el MISMO item y el mismo concepto que
    // en el 3D (el pivote es el cursor del viewport), y renombrarlo rompia el paralelo.
    gMenuPivotUV->Agregar(T("3D Cursor"), PivotCursor3D, IconType::pivotCursor)->verde = (g_transformPivot==PivotCursor3D);
    gMenuPivotUV->Agregar(T("Individual Origins"), PivotIndividual, IconType::pivotIndividual)->verde = (g_transformPivot==PivotIndividual);
    gMenuPivotUV->Agregar(T("Median Point"), PivotMedian, IconType::pivotMedian)->verde =
        (g_transformPivot==PivotMedian || g_transformPivot==PivotActive); // Active cae en la mediana
    if (MenuAbierto && MenuAbierto != gMenuPivotUV) MenuAbierto->Cerrar();
    gMenuPivotUV->Abrir(mx, my, MenuPantallaW, MenuPantallaH);
    MenuAbierto = gMenuPivotUV;
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
static Quaternion gEVrg;            // rotacion global de la malla (EFECTIVA, constante en el drag)
static Vector3    gEVsg(1,1,1);     // escala global
static Vector3    gEVorigin;        // origen global (EFECTIVO, ver EditXformIniciar)
// cuanto corrio al objeto un constraint de POSICION: origen efectivo - origen base. Vale (0,0,0)
// sin constraints y tambien con los de pura rotacion (billboard / copy rotation). Lo unico que lo
// usa es el pivote del CURSOR 3D, que es un punto del mundo BASE (ver ahi el por que).
static Vector3    gEVoffCons;
static Vector3    gEVpivot;         // pivote en MUNDO
// acumuladores (segun 'estado' solo uno esta activo)
static Vector3    gEVtrans;         // translacion de mundo acumulada
static Quaternion gEVrotTotal(1,0,0,0); // rotacion acumulada
static float      gEVscaleAmt = 0;  // factor de escala acumulado (f = 1 + amt); en SHRINK = distancia por la normal
static bool       gEVshrink = false; // SHRINK/FATTEN (Alt+S): reusa EditScale pero cada vert se mueve por SU normal
bool EditShrinkActivo(){ return gEVshrink; }
// EXTRUDE / orientacion NORMAL: la translacion se constriñe a gTransformNormal (la normal en mundo).
// gEVuseCustom + gTransformNormal son GLOBALES (variables.h) para que CiclarEje/EjeOrientado los vean.

// hay algun constraint en la cadena (el objeto o cualquier ancestro)? Sin ninguno la transform
// efectiva y la base son LA MISMA CUENTA (Objects.h), asi que se entra por el camino de siempre
// -RotGlobalDe/GetGlobalPositionBase- y una escena sin constraints (el 99%) da los mismos floats
// que antes de que esto existiera, sin pasar por la descomposicion de la matriz.
static bool EVHayConstraints(const Object* o){
    // por el predicado del Core y no por constraints.empty(): "tiene uno" no es "hace algo". Con el
    // "ver en modo edicion" apagado -que es el caso normal MIENTRAS SE EDITA- el objeto no tiene
    // ningun constraint efectivo, y entonces el G/R/S entra por el camino de siempre y da los mismos
    // floats que una escena sin constraints.
    for (const Object* p = o; p; p = p->Parent) if (W3dObjTieneConstraintEfectivo(p)) return true;
    return false;
}

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
            if (eg*2+1 >= e->lineIdx.size()) break; // lineIdx desincronizado (defensivo, como SnapNeedleActivo)
            selEdit[e->lineIdx[eg*2]]=1; selEdit[e->lineIdx[eg*2+1]]=1;
        }
    } else {
        for (size_t f=0; f<e->faces.size(); f++) if (f<e->faceSel.size() && e->faceSel[f])
            for (size_t c=0;c<e->faces[f].size();c++) selEdit[e->faces[f][c]]=1;
    }

    gEVmesh = m;
    // ------------------------------------------------------------------
    // el trio rotacion/escala/origen es la ida y vuelta mundo<->local de EVLocalAMundo/EVMundoALocal,
    // y va la transform **EFECTIVA**: el G/R/S de Edit Mode MIDE EN PANTALLA (gEVtrans sale de
    // camRight/camUp, el pivote se dibuja, el snap engancha en pixeles), o sea el caso de la regla de
    // Objects.h que pide la efectiva. Lo importante es que sea la MISMA en las DOS puntas del viaje:
    // con world0 en un espacio y la vuelta a local en otro, lo que se DIBUJA se desplaza por
    // R_efectiva * R_base^-1 * gEVtrans y el vertice no va donde apunta el mouse (con un billboard se
    // iba por el DOBLE del yaw de la camara: el bloqueante de los billboards de un nivel).
    // Y NO hornea, JUSTAMENTE por usarse en las dos puntas: la vista se CANCELA. Lo que se escribe es
    // el delta local (R_ef*S)^-1 * gEVtrans, y con un billboard al 100 (el caso de los arboles del
    // billboards) R_ef^-1 * camRight da (1,0,0) EXACTO, asi que el mismo arrastre escribe lo mismo desde
    // CUALQUIER camara. La version base es la que dejaba la camara adentro del vertice (R_base^-1 *
    // camRight SI depende del yaw), o sea que esta es MAS determinista, no menos.
    // Lo mide 'consedit' (main/test/W3dScript.cpp) corriendo el mismo arrastre desde dos vistas.
    // Ojo con el ORIGEN: solo entra en juego con el pivote del CURSOR 3D (en el resto se cancela al
    // restar world0 - pivote), y ahi se compensa con gEVoffCons; ver el pivote mas abajo.
    // ------------------------------------------------------------------
    gEVsg = ScaleGlobalDe(m); // la escala NUNCA la toca un constraint: base y efectiva son la misma cuenta
    const Vector3 origenBase = m->GetGlobalPositionBase();
    Quaternion rgEf; Vector3 origenEf; bool hayEf = false;
    if (EVHayConstraints(m)){
        // la matriz efectiva es la de la ULTIMA vista bindeada y el G/R/S arranca por TECLADO, sin
        // dibujar nada en el medio: con dos viewports abiertos esa puede ser la del OTRO. Se bindea el
        // viewport donde esta el usuario, igual que BoneGrabStartEx (BoneEdit.cpp) y PoseW2N.
        if (Viewport3DActive) Viewport3DActive->BindVista();
        Matrix4 W; m->GetWorldMatrix(W);
        hayEf = W3dQuatDeMatriz(W, rgEf); // false = base espejada o chata: no hay rotacion que sacar
        if (hayEf) origenEf = Vector3(W.m[12], W.m[13], W.m[14]); // la MISMA W: la efectiva no es pura
    }
    if (hayEf){ gEVrg = rgEf; gEVorigin = origenEf; }
    else      { gEVrg = RotGlobalDe(m); gEVorigin = origenBase; }
    gEVoffCons = gEVorigin - origenBase;

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
        // el cursor 3D es un punto del mundo BASE para TODO lo que escribe vertices (Snap Selection
        // to Cursor y Merge At Cursor lo bajan a local con la cadena base, ver ObjectMode.cpp): no
        // depende de ninguna camara y no tiene que meter en la geometria el salto que un constraint
        // de POSICION esta haciendo al vuelo. Como el resto de la cuenta vive en el espacio EFECTIVO,
        // se lo trae con el mismo desplazamiento que corrio al objeto y asi los dos lados quedan en
        // UN SOLO espacio: un scale x2 desde el cursor duplica los locales, con Copy Location o sin
        // el (lo fija 'conshornea' D). Con un billboard/copy rotation gEVoffCons es (0,0,0).
        gEVpivot = cursor3D.pos + gEVoffCons;
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
    Viewport3DActive->BindVista();   // el rayo sale de ESTA vista: ver SnapBuscarTarget
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
            { bool rango=true; for (int k=0;k<nc;k++) if (idx[k]<0||idx[k]>=m->vertexSize){rango=false;break;}
              if (!rango) continue; }   // faces3d desincronizado con vertex[] (defensivo)
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

static bool SnapNeedleActivo(Vector3& out); // definida mas abajo (la usa el Snap Base = Active)

// ajusta gEVtrans para PEGAR la seleccion al target de snap (base Closest). Constreñido al mismo eje/plano que el
// move; libre = offset completo (proyeccion a la cara = retopologia). Setea g_snapHit + pos para el recuadro verde.
// LOS DOS LADOS DE LA COMPARACION VIVEN EN EL MISMO ESPACIO, y no es casualidad: SnapBuscarTarget devuelve un punto
// del mundo EFECTIVO (proyecta a pantalla lo que se DIBUJA) y los world0 de gEVsnap tambien salen de la efectiva
// (ver EditXformIniciar). Con world0 en base el iman se veia enganchar en un lado y el vertice quedaba en otro.
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
        // el ACTIVO segun el MODO (vert/borde/cara): activeIdx indexa vertSel/edgeSel/faceSel,
        // NO editVerts -> lo resuelve SnapNeedleActivo (igual que el snap de rotacion). Antes se
        // comparaba editK==activeIdx, que solo era correcto en modo Vertice.
        Vector3 B0;
        if (SnapNeedleActivo(B0)){ B = B0 + gEVtrans; any = true; }
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
        // INDEPENDIENTES encimados, rederivar posRep por posicion los SOLDARIA (caras y edit desincronizados).
        m->CalcularBordes(false, false);      // posRep(conserva)/centroGeom/bordes; conserva el edit
        if (!g_editLockNormales) m->RecalcularNormales();
        // AUTO MERGE (opt-in, OFF por defecto): recien AHORA, si el usuario lo pidio, se sueldan los verts movidos
        // con cualquier vert a <= threshold. No-op si no hay nada cerca (MergeVertsEdit retorna sin tocar nada).
        if (g_autoMerge) MergeVertsEdit(m, 3 /*By Distance*/, g_autoMergeThreshold, Vector3(0,0,0));
        if (!m->modificadores.empty()) m->GenerarMallaModificada(); // regen final (toma las normales recalculadas)
    }
    // AUTO KEY de VERTICES: con la vertex anim de ESTA malla activa en el timeline, confirmar
    // el move guarda la malla como keyframe. VA DESPUES de RecalcularNormales: asi el keyframe
    // captura las NORMALES nuevas de la pose deformada (sino la iluminacion quedaba en la vieja).
    { extern bool AutoKeyOn; extern void VertexAnimInsertarKeyframe();
      if (AutoKeyOn && ActiveAnimKind == 3 && gEVmesh && (Mesh*)ActiveAnimMesh == gEVmesh)
          VertexAnimInsertarKeyframe(); }
    gEVsnap.clear(); gEVmesh = NULL;
    estado = editNavegacion;
    g_extrudeEnCurso = false; // termino el transform
    // el motion del transform (controles.cpp) dejo ViewPortClickDown en true para CONGELAR el foco
    // en el viewport mientras dura el G/R/S. Al terminar por ESC/ENTER/boton de barra (sin un
    // mouse-up sobre el viewport, que es lo que normalmente lo libera) hay que APAGARLO aca, sino
    // viewPortActive queda clavado en este viewport y el editor UV (u otro) no recibe los clicks:
    // el mouse esta abajo pero el foco sigue arriba. Igual que el Timeline/Editor2D.
    extern bool ViewPortClickDown; ViewPortClickDown = false;
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
    // liberar el foco congelado: ver la nota en EditXformConfirmar (cancelar por ESC no genera
    // mouse-up, asi que ViewPortClickDown quedaba trabado en true y clavaba viewPortActive aca).
    extern bool ViewPortClickDown; ViewPortClickDown = false;
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
    // cursor 3d (MUNDO) -> LOCAL del mesh (para At Cursor): misma matematica que Snap Selection to Cursor,
    // y por lo mismo BASE: el merge MUEVE VERTICES y el cursor 3D no depende de ninguna camara (Objects.h).
    Quaternion rg = RotGlobalDe(m); Vector3 sg = ScaleGlobalDe(m);
    Vector3 dloc = rg.Inverted() * (cursor3D.pos - m->GetGlobalPositionBase());
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
    MenuSliderDragSoltar(); // fin del drag del item-slider de un menu (si habia)
    // Symbian (tap-agarra / tap-suelta): el up del MISMO tap que agarro (down+up
    // en ~8ms) no suelta, solo si hubo movimiento. En PC (hold-drag) el up SIEMPRE
    // suelta: sino un click sin movimiento dejaba la barra AGARRADA y el panel
    // scrolleaba siguiendo al mouse con el boton levantado.
#ifdef W3D_SYMBIAN
    if (gScrollBarDrag && gScrollMovio) {
#else
    if (gScrollBarDrag) {
#endif
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
        // DRAG del item-slider (AgregarFloat): el down sobre la fila lo armo (Click); mientras
        // el boton siga apretado, el arrastre horizontal EDITA el valor en vivo. Mismo dispatch
        // que el click (Ejecutar en el menu TOP) para que las acciones del item corran igual.
        // No resalta otras filas ni auto-cierra el menu mientras se arrastra.
        if (MenuSliderDragActivo()) {
            if (leftMouseDown) {
                int idSl = MenuSliderDragMover(mx);
                if (idSl >= 0 && MenuAbierto) MenuAbierto->Ejecutar(idSl);
                g_redraw = true;
                return true;
            }
            MenuSliderDragSoltar(); // up perdido (solto fuera de la ventana): cortar el drag
        }
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

    // 0) la barra de HERRAMIENTAS (abajo), COMPARTIDA: 3D (historial/orientacion/ejes/tilde-cruz),
    // UV editor y Editor 2D (G/R/S). El hit + despacho por rol viven en ViewportBase (ToolbarBase.cpp);
    // un viewport sin toolbar devuelve false y el click sigue su camino normal.
    if (under->ToolbarClick(mx, my)) return true;

    // 1) la barra de botones del viewport
    if (under->BarClick(mx, my)) {
        if (LayoutClickBotonTipo(under, mx, my)) return true;
        if (under->ViewportKind() == 3) {
            ((Properties*)under)->ClickTab(mx, my); // pestanias Objeto/Mesh
        } else if (under->ViewportKind() == 4) {
            LayoutClickBarraUV((UVEditor*)under, mx, my); // boton "View" -> checkboxes
        } else if (under->ViewportKind() == 5) {
            ((Timeline*)under)->ClickBarButton(mx, my); // transporte + campos Start/End
        } else if (under->ViewportKind() == 6) {
            LayoutClickBarra2D((Editor2D*)under, mx, my); // boton "Add" del editor 2D
        } else if (under->ViewportKind() == 8) {
            LayoutClickBarraIDE((IDE*)under, mx, my); // selector de script / Save / Refresh
        } else {
            // transporte (Stop/Play) SOLO por click real; si no fue transporte, abrir el menu
            if (!LayoutTransporteBarra3D(under, mx, my)) LayoutAbrirMenuDeBarra(under, mx, my); // Select/Add/Object/Overlays
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
    if (under->ViewportKind() == 8) {
        // IDE: el click posiciona el cursor de texto (shift extiende la seleccion)
        ((IDE*)under)->ClickContenido(mx, my, LShiftPressed);
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
            case LayoutKey::Up:    changeSelect(SelectMode::PrevSingle, true); out->AsegurarVisible(); return true;
            case LayoutKey::Down:  changeSelect(SelectMode::NextSingle, true); out->AsegurarVisible(); return true;
            case LayoutKey::Left:  SetDesplegado(false); return true;
            case LayoutKey::Right: SetDesplegado(true);  return true;
        }
        return false;
    }
    if (viewPortActive->ViewportKind() == 4) { // UV editor: las flechas PANEAN la vista
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
// por diseno), o ZOOM centrado si 0 esta mantenido (0 + arriba/abajo). Devuelve true si el viewport activo
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
    // Desde el EDIT de huesos, Tab tambien sale (a Pose): cierra el grab pendiente y prepara el
    // skin autorado (el rig editado tiene que deformar apenas se posa).
    if (ObjActivo->getType() == ObjectType::armature) {
        UndoCapturarModo(); // Ctrl+Z: guarda el modo PREVIO
        if (InteractionMode == EditMode){
            if (BoneGrabActivo()) BoneGrabCancelar();
            PrepararSkinAutorado((Armature*)ObjActivo);
        }
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
    if (!Textures.empty() && Textures[0]) w3dEngine::BindTexture(Textures[0]->iID);
    MenuAbierto->Render();
}
