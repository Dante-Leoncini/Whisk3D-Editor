#include "WhiskUI/text/font.h"   // UTF8_Largo
#include "io/EditorRoots.h"   // el selector de escena/prefab de las barras (3D y outliner)
#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "W3dLang.h"   // T(): los textos salen en el idioma del sistema
#include "Undo.h" // Ctrl+Z: capturar modo / seleccion
#include "app/W3dShortcuts.h" // atajos reasignados: la tecla apretada -> la de la funcion
#include "ViewPorts/PopUp/ConfirmPopup.h" // OpenConfirmDelete (popup de confirmar borrado)
#include "ViewPorts/PopUp/NamePopup.h"    // "Guardar layout": el nombre
#include "config/W3dLayouts.h"            // los layouts guardados (del programa, no del proyecto)
#include "ViewPorts/LayoutInput.h"
#include "ViewPorts/LayoutTree.h" // aviso de destruccion de viewports + borrar un arbol entero
#include "edit/MeshEdit.h"
#include "edit/UVUnwrap.h"
#include "edit/Proportional.h"
#include "PopUp/NewTexturePopup.h"
#include "ViewPorts/PopUp/ExportUVPopup.h"   // Export UV Layout (menu guardar del editor UV)
#include "edit/ModeMemory.h"            // la memoria de los modos: se suelta al dejarlos
#include "io/EditedTexture.h"          // Save Texture / Make External / Make Internal + el "*" de las sin guardar // "New Texture..." del dropdown Texture del UV editor    // proportional editing: pesos de los vertices no seleccionados
#include <queue>
#include <set>             // el iman con las flechas: los vertices que se mueven
#include <functional>      // menu U: unwrap / smart project / lightmap / follow quads / reset   // W3dTriangulateFace
#include "ViewPorts/PoseTransform.h" // Pose Mode transform (extraido a su propio archivo)
#include "ViewPorts/Notifications.h" // toasts (extraido a su propio archivo)
#include "ViewPorts/NumInput.h" // entrada numerica/formulas (extraido a su propio archivo)
#include "ViewPorts/Parent.h" // emparentar/desemparentar (extraido a su propio archivo)
#include "ViewPorts/Pick3D.h"  // pick/seleccion 3D + loop cut (extraido a su propio archivo)
#include "input/W3dClipboard.h"      // el portapapeles del sistema (cortar / copiar / pegar objetos)
#include <time.h>              // (la carpeta de cada copia)
#include "edit/BoxSelect.h"    // Box Select (tecla B / menu Select)
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
#include "objects/Routine.h"   // Add > Core: las rutinas
#include "objects/MeshResource.h"   // W3dMeshEdgesView / W3dMeshFacesView: el snap a una malla de un recurso
#include "physics/W3dRigid.h" // Add > Physics: W3dRigidDef del objeto activo
#include "objects/Curve.h"   // edicion de riel/path via proxy de malla (CurveEnterEdit)
#include "w3dlog.h"          // aviso al pasar una curva de .cap a autorada
#include "objects/Materials.h" // Material (mat->texture) para el dropdown "Texture" del UV editor
#include "importers/import_obj.h" // PendingTextureOf: texturas encoladas y aun sin subir (dropdown Texture)
#include "w3dTexture.h"        // w3dEngine::TextureSize (los niveles del menu Mipmap)
#include "io/Texture2D.h"      // Texture2DList: la UI del JUEGO en el dropdown Texture
#include "objects/Textures.h"  // Texture (path) para las etiquetas del dropdown
#include "objects/EditMesh.h"
#include "objects/Light.h"
#include "objects/Camera.h"
#include "objects/Empty.h"
#include "objects/LOD.h"        // Add > LOD (un hijo por distancia a la camara)
#include "objects/Culling.h"    // Add > Culling (frustum culling de sus hijos)
#include "objects/Fog.h"        // Add > Fog (niebla en el arbol)
#include "objects/Sound.h"      // Add > Sound (audio en la escena)
#include "objects/ClipRegion.h" // Add > Clear Depth / Screen Rect
#include "objects/PostProcessObj.h" // Add > Post Processing
#include "objects/Hitbox.h"  // Add > Hitbox (caja de deteccion con eventos lua)
#include "objects/LightProbe.h"   // Add > Light Probe
#include "objects/ReflectionProbe.h"   // Add > Reflection Probe
#include "io/PrefabsEditor.h"   // Add > Prefab (una instancia en el cursor 3D) + "Create Prefab" / "Unpack"
#include "objects/InstancePrefab.h"
#include "objects/ProxyW3d.h"   // Add > Proxy W3D (un prefab o una escena de una libreria externa)
#include "io/Libraries.h"
#include "io/ExternalLibrary.h"   // W3dExtLibLink: Add > Proxy W3D vincula el .w3d elegido
#include "objects/Particles.h" // Add > Particles (emisor de particulas del Core)
#include "objects/UI.h"
#include "objects/Text2D.h"
#include "objects/Image2D.h"
#include "objects/Rect2D.h"
#include "objects/Container2D.h"
#include "objects/Slice9.h"
#include "objects/Button2D.h"
#include "objects/Expand2D.h"
#include "objects/Video2D.h"
#include "objects/Gamepad.h"   // entrada cruda del mando + el objeto Script
#include "io/UI2DFormat.h"      // cargar interfaces .w3dui
#include "W3dPalettes.h"        // paletas del proyecto (AddUI adopta la default)
#include "PopUp/FileBrowser.h"  // OpenFileBrowser (elegir el archivo)
#include "io/W3dContainer.h"     // W3dImportAsset ("Load .lua..." del IDE copia el script adentro del proyecto)
#include "io/ProjectResources.h"   // menu Archivo: carpeta de la biblioteca / dentro o fuera del .w3d (W3dViewRec*)
#include "io/SaveW3D.h"            // SaveW3D: "Guardar" de un archivo INTERNO guarda tambien el .w3d
#include "ViewPorts/PopUp/SaveAsPopup.h"   // "Guardar como..." (scripts del IDE y texturas del editor UV)
#include "io/Texture2D.h"       // tamano real de la imagen elegida al crearla
#include "io/Video2DCache.h"    // tamano real del video elegido al crearlo
#include "objects/Armature.h"
#include "animation/SkeletalAnimation.h" // InsertKeyframeSkeleton (Pose Mode: Insert Keyframe)
#include "animation/VertexAnimation.h"   // menu Animation del UV editor: frames de la vertex anim activa
#include "animation/Armature2DAnimation.h" // InsertKeyframeArm2D (pose 2D: Insert Keyframe con clips propios)
#include "animation/Animation.h"         // ActiveAnimKind/ActiveAnimMesh/CurrentFrame (menu Animation del UV)
#include "objects/Instance.h"
#include "objects/Mirror.h"     // Add > Modifier Objects > Mirror
#include "objects/Collection.h"
#include "objects/ObjectMode.h"
#include "edit/Modifier.h" // ModifierType::Mirror + target (regen de mirrors al mover objetos)
#include "edit/BoneEdit.h" // Edit Mode de ARMATURE (Fase 3): ops de huesos + menu contexto + prep al salir
#include "objects/Primitives.h"
#include "variables.h"
#include "render/RenderOptions.h" // g_currentFps
#include "ViewPorts/PopUp/PopUpBase.h"
#include "ViewPorts/PopUp/RedoMeshPanel.h"
#include "WhiskUI/widgets/card.h"        // tarjeta de las notificaciones
#include "WhiskUI/text/bitmapText.h"  // texto de las notificaciones
#include "WhiskUI/draw/icons.h"       // iconos notifOk / notifError
#include "config/W3dProfile.h"     // W3dNowMs (el cartelito de ayuda)
#include "WhiskUI/theme/colors.h"     // ColorID
#include "w3dlog.h"         // las notificaciones tambien van al log
// (los tipos GL + el dibujo vienen del engine: w3dGraphics.h / w3dEngine, ya incluido arriba)

// rename en curso (mesh part / material / hueso): Properties.cpp. Enter escribe el nombre uniquificado, Esc descarta.
extern bool RenameActive();
extern void RenameCommit();
extern void RenameCancel();

void (*LayoutImportObj)() = NULL;
void (*LayoutImportFbx)() = NULL; // "Add > Imports > FBX": abre el explorador filtrado a .fbx (lo cablea la plataforma)
void (*LayoutImportGltf)() = NULL; // "Add > Imports > glTF": explorador filtrado a .gltf
void (*LayoutImportGlb)() = NULL;  // "Add > Imports > GLB": explorador filtrado a .glb
void (*LayoutImportStl)() = NULL;  // "Add > Imports > STL": explorador filtrado a .stl
void (*LayoutWarpMouse)(int x, int y) = NULL;
void (*LayoutChangedTree)() = NULL;

// ====================================================================
// arbol: helpers (Row y Column se distinguen por ContainerKind, sin RTTI)
// ====================================================================

static ViewportBase* LayoutCreateViewport(int aId) {
    switch (aId) {
        case 0: return new Viewport3D();
        case 1: return new Outliner();
        case 2: return new Properties();
        case 3: return new UVEditor();
        case 4: return new Timeline();
        case 5: return new Editor2D();
        case 6: return new Console();
        case 7: return new IDE();
        case 8: return new Library();
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
// la pantalla es de un TELEFONO o una tablet que se usa con el dedo: Android siempre; en la web, si el puntero principal
// del navegador es el dedo (main.cpp lo averigua al arrancar). Decide el layout de fabrica de abajo
#ifdef __ANDROID__
bool g_w3dPhone = true;
#else
bool g_w3dPhone = false;
#endif
ViewportBase* LayoutDefault(int w, int h, Viewport3D** vp3dOut) {
    Viewport3D* vp3d = new Viewport3D();
    if (vp3dOut) *vp3dOut = vp3d;
    // UNA sola logica de layout por defecto, por TAMANO DE PANTALLA (identica en todos los sistemas):
    //   lado menor < 320px  -> 2 viewports: 3D + Propiedades, segun orientacion.
    //   lado menor >= 320px -> 4 viewports: 3D + Timeline + Outliner + Propiedades (el de arriba).
    int side = (w < h) ? w : h;
    if (side < 320) {
        if (w >= h)  // horizontal: 3D a la izquierda | Propiedades a la derecha
            return new ViewportRow(vp3d, new Properties(), 0.7f);
        // vertical (N95 240x320): 3D arriba / Propiedades abajo
        return new ViewportColumn(vp3d, new Properties(), 0.7f);
    }
    if (g_w3dPhone) {
    // TELEFONO (Android, o el navegador de un telefono): el espacio es oro. Sin outliner (la biblioteca/escena se abre cambiando el tipo de un
    // viewport cuando hace falta) y, parado, sin timeline: solo el visor 3D arriba y las propiedades abajo.
    //     vertical               horizontal
    //     +-------------+        +-------------------+---------+
    //     |  3D         |        |  3D               | Props   |
    //     +-------------+        +-------------------+         |
    //     | Propiedades |        |  Timeline (chico) |         |
    //     +-------------+        +-------------------+---------+
    if (h > w) return new ViewportColumn(vp3d, new Properties(), 0.58f);
    return new ViewportRow(new ViewportColumn(vp3d, new Timeline(), 0.86f), new Properties(), 0.68f);
    }
    if (h > w) {
        // VERTICAL con pantalla grande (un telefono parado): el mismo reparto pero apilado. El layout
        // apaisado (columna a la derecha) en vertical dejaba outliner y propiedades aplastados y sin texto.
        //     +---------------------------+
        //     |  3D                       |
        //     |  Timeline (bien chico)    |
        //     +-------------+-------------+
        //     |  Outliner   | Propiedades |
        //     +-------------+-------------+
        return new ViewportColumn(
            new ViewportColumn(vp3d, new Timeline(), 0.86f),        // 3D grande / timeline chico
            new ViewportRow(new Outliner(), new Properties(), 0.40f), // outliner 40 % a la izquierda
            0.62f);                                                   // arriba 62 % del alto
    }
    return new ViewportRow(
        new ViewportColumn(vp3d, new Timeline(), 0.88f),          // 3D grande / timeline chico
        new ViewportColumn(new Outliner(), new Properties(), 0.40f), // outliner 40 % arriba
        0.72f
    );
}

static bool LayoutReplaceInTree(ViewportBase* aNode, ViewportBase* aOld,
                                    ViewportBase* aNew) {
    if (!aNode || aNode->isLeaf()) return false;
    if (aNode->ContainerKind() == 1) {
        ViewportRow* r = (ViewportRow*)aNode;
        if (r->childA == aOld) { r->childA = aNew; return true; }
        if (r->childB == aOld) { r->childB = aNew; return true; }
        if (LayoutReplaceInTree(r->childA, aOld, aNew)) return true;
        return LayoutReplaceInTree(r->childB, aOld, aNew);
    }
    if (aNode->ContainerKind() == 2) {
        ViewportColumn* c = (ViewportColumn*)aNode;
        if (c->childA == aOld) { c->childA = aNew; return true; }
        if (c->childB == aOld) { c->childB = aNew; return true; }
        if (LayoutReplaceInTree(c->childA, aOld, aNew)) return true;
        return LayoutReplaceInTree(c->childB, aOld, aNew);
    }
    return false;
}

// los dos hijos de una division (una fila: lado a lado; una columna: apilados), por referencia: se pueden soltar
static ViewportBase*& LayoutChildA(ViewportBase* aNode) {
    return aNode->ContainerKind() == 1 ? ((ViewportRow*)aNode)->childA : ((ViewportColumn*)aNode)->childA;
}
static ViewportBase*& LayoutChildB(ViewportBase* aNode) {
    return aNode->ContainerKind() == 1 ? ((ViewportRow*)aNode)->childB : ((ViewportColumn*)aNode)->childB;
}

static ViewportBase* LayoutParentOf(ViewportBase* aNode, ViewportBase* aChild) {
    if (!aNode || aNode->isLeaf()) return NULL;
    ViewportBase* a = LayoutChildA(aNode);
    ViewportBase* b = LayoutChildB(aNode);
    if (a == aChild || b == aChild) return aNode;
    ViewportBase* r = LayoutParentOf(a, aChild);
    if (r) return r;
    return LayoutParentOf(b, aChild);
}

// borra un subarbol completo (los dtors de Row/Column no borran childA): es el MISMO
// borrado que usa abrir un proyecto para soltar el layout anterior (LayoutTree.h)
static void LayoutDeleteSubtree(ViewportBase* aNode) {
    ViewportDeleteTree(aNode);
}

static Viewport3D* LayoutFirst3D(ViewportBase* aNode) {
    if (!aNode) return NULL;
    if (aNode->isLeaf()) {
        return aNode->ViewportKind() == 1 ? (Viewport3D*)aNode : NULL;
    }
    Viewport3D* r = LayoutFirst3D(LayoutChildA(aNode));
    if (r) return r;
    return LayoutFirst3D(LayoutChildB(aNode));
}
// la primera hoja (un viewport, no una division) de un subarbol: la de arriba a la izquierda
static ViewportBase* LayoutFirstLeaf(ViewportBase* aNode) {
    while (aNode && !aNode->isLeaf()) aNode = LayoutChildA(aNode);
    return aNode;
}
// (el proyecto nuevo con un layout guardado: import_w3d.cpp, NewProjectNow) la primera Vista 3D de un arbol
Viewport3D* LayoutFirst3DOf(ViewportBase* aNode) { return LayoutFirst3D(aNode); }

// recolecta las hojas (viewports con borde) en orden DFS izquierda->right
static void LayoutCollectLeaves(ViewportBase* aNode, std::vector<ViewportBase*>& out) {
    if (!aNode) return;
    if (aNode->isLeaf()) { out.push_back(aNode); return; }
    ViewportBase* a; ViewportBase* b;
    if (aNode->ContainerKind() == 1) { a = ((ViewportRow*)aNode)->childA;    b = ((ViewportRow*)aNode)->childB; }
    else                             { a = ((ViewportColumn*)aNode)->childA; b = ((ViewportColumn*)aNode)->childB; }
    LayoutCollectLeaves(a, out);
    LayoutCollectLeaves(b, out);
}

// vuelca a disco/overlay los IDE con cambios SIN guardar (sucio). Lo llama SimPlay ANTES de arrancar la partida:
// sin esto, editar un valor DENTRO del .lua desde el IDE y dar Play corria el .lua VIEJO (el buffer del IDE no
// se habia persistido, y el '*' de sucio lo delataba). Recorre las hojas y guarda cada IDE (kind 8) sucio.
void IDESaveDirty() {
    if (!rootViewport) return;
    std::vector<ViewportBase*> leaves;
    LayoutCollectLeaves(rootViewport, leaves);
    for (size_t i = 0; i < leaves.size(); i++)
        if (leaves[i]->ViewportKind() == 8) { IDE* ide = (IDE*)leaves[i]; if (ide->dirty) ide->Save(); }
}

// cambia el viewport ACTIVO (borde verde) a la siguiente hoja (dir=+1) o la
// anterior (dir=-1), dando la vuelta. Sin mouse (Symbian) es la unica forma de
// elegir viewport: la tecla verde de llamada lo cicla. Con mouse, el hover lo
// pisa en el siguiente movimiento (el mouse manda cuando esta).
void LayoutCycleActiveViewport(int dir) {
    std::vector<ViewportBase*> leaves;
    LayoutCollectLeaves(rootViewport, leaves);
    if (leaves.empty()) return;
    int n = (int)leaves.size();
    int idx = -1;
    for (int i = 0; i < n; i++) if (leaves[i] == viewPortActive) { idx = i; break; }
    int next = (idx < 0) ? 0 : (((idx + dir) % n) + n) % n;
    viewPortActive = leaves[next];
    if (viewPortActive->isLeaf() && viewPortActive->ViewportKind() == 1)
        Viewport3DActive = (Viewport3D*)viewPortActive;
    // al ENTRAR (verde) a un panel de Propiedades sin mouse: SIGUE en la fila donde estaba (pedido del dueno: volver
    // a buscarla con las flechas era muy molesto) y recentra el scroll para que se vea; solo si esa fila ya no esta
    // arranca del primer grupo visible (ResumeSelection)
    if (viewPortActive->isLeaf() && viewPortActive->ViewportKind() == 3)
        ((Properties*)viewPortActive)->ResumeSelection();
}

// redimensiona el viewport ACTIVO en UN eje (dx!=0 izq/der; dy!=0 arr/ab).
// Sube al ANCESTRO mas cercano del tipo correcto (Row para horizontal, Column
// para vertical): asi estando en el outliner (dentro de un Row dentro de un
// Column), izq/der mueve el divisor del Row, y arr/ab mueve el del Column (le
// roba espacio al 3D de arriba). childA = izquierda/arriba; der/abajo => frac+.
void LayoutResizeActiveViewport(int dx, int dy, float step) {
    if (!viewPortActive || !rootViewport) return;
    if (dx == 0 && dy == 0) return;
    bool horizontal = (dx != 0); // izq/der -> Row ; arr/ab -> Column
    ViewportBase* node = viewPortActive;
    ViewportBase* anc = NULL;
    while (node) {
        ViewportBase* parent = LayoutParentOf(rootViewport, node);
        if (!parent) break;
        bool isRow = (parent->ContainerKind() == 1);
        if (horizontal == isRow) { anc = parent; break; } // Row<->H, Column<->V
        node = parent;
    }
    if (!anc) return; // no hay divisor en ese eje
    float* frac = (anc->ContainerKind() == 1) ? &((ViewportRow*)anc)->splitFrac
                                              : &((ViewportColumn*)anc)->splitFrac;
    // der/abajo agrandan childA (frac+); izq/arriba lo achican. El activo crece o
    // se achica segun sea childA o childB del divisor (sale natural).
    float delta = horizontal ? (dx > 0 ? step : -step)
                             : (dy > 0 ? step : -step);
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
void LayoutResizeCorner(ViewportBase* aVp, int dx, int dy) {
    if (!aVp || !rootViewport) return;
    // borde IZQUIERDO: primer ancestro Row donde aVp cae del lado childB (derecha del split)
    if (dx != 0) {
        ViewportBase* node = aVp;
        while (node) {
            ViewportBase* parent = LayoutParentOf(rootViewport, node);
            if (!parent) break;
            if (parent->ContainerKind() == 1 && ((ViewportRow*)parent)->childB == node) {
                ((ViewportRow*)parent)->SetSizeChildrens(dx); break; // dx<0 (arrastrar a la izq) = crece aVp
            }
            node = parent;
        }
    }
    // borde SUPERIOR: primer ancestro Column donde aVp cae del lado childB (abajo del split)
    if (dy != 0) {
        ViewportBase* node = aVp;
        while (node) {
            ViewportBase* parent = LayoutParentOf(rootViewport, node);
            if (!parent) break;
            if (parent->ContainerKind() == 2 && ((ViewportColumn*)parent)->childB == node) {
                ((ViewportColumn*)parent)->SetSizeChildrens(dy); break; // dy<0 (arrastrar arriba) = crece aVp
            }
            node = parent;
        }
    }
}

// tras un cambio estructural: punteros frescos + relayout completo
static void LayoutRescan(ViewportBase* aFocus, int aW, int aH) {
    viewPortActive = aFocus;
    // LIBERAR EL FOCO DE TECLADO POR HOVER: la accion llega desde el DOWN del click en el menu
    // (que ya puso ViewPortClickDown=true) y el UP va al mouse_button_up del viewport NUEVO. Si
    // ese tipo no lo implementa (le pasaba a Console), nadie lo apagaba y viewPortActive quedaba
    // CLAVADO en el viewport nuevo (las teclas seguian yendo ahi aunque muevas el mouse). Un
    // cambio estructural (cambiar tipo / split / expand) nunca deja un drag legitimo en curso,
    // asi que se libera aca, para CUALQUIER tipo de viewport.
    ViewPortClickDown = false;
    if (aFocus && aFocus->isLeaf() && aFocus->ViewportKind() == 1) {
        Viewport3DActive = (Viewport3D*)aFocus;
    } else {
        Viewport3DActive = LayoutFirst3D(rootViewport); // puede ser NULL
    }
    rootViewport->x = 0;
    rootViewport->y = 0;
    rootViewport->Resize(aW, aH);
    if (LayoutChangedTree) LayoutChangedTree();
}

// la division que contiene al viewport desaparece: uno de sus dos hijos toma su lugar y el otro se borra.
// keepSelf: se queda el viewport (Expandir); si no, se queda el hermano (Cerrar)
static void LayoutCollapse(ViewportBase* aVp, bool keepSelf) {
    ViewportBase* parent = LayoutParentOf(rootViewport, aVp);
    if (!parent) return;   // es el root: no hay division que deshacer
    const int w = rootViewport->width, h = rootViewport->height;
    ViewportBase*& a = LayoutChildA(parent);
    ViewportBase*& b = LayoutChildB(parent);
    ViewportBase* sibling = (a == aVp) ? b : a;
    a = NULL; b = NULL;
    ViewportBase* keep = keepSelf ? aVp : sibling;
    if (parent == rootViewport) rootViewport = keep;
    else LayoutReplaceInTree(rootViewport, parent, keep);
    LayoutDeleteSubtree(keepSelf ? sibling : aVp);
    delete parent;
    LayoutRescan(LayoutFirstLeaf(keep), w, h);
}
// Expand: el viewport toma el lugar de su division (el hermano se borra)
static void LayoutExpand(ViewportBase* aVp) { LayoutCollapse(aVp, true); }
// Close: el viewport se borra y su hermano toma el lugar de la division
static void LayoutClose(ViewportBase* aVp) { LayoutCollapse(aVp, false); }

// Swap: el viewport y su hermano (otro viewport, o una division entera) cambian de lugar en su fila o columna: el de
// arriba pasa abajo, el de la izquierda a la derecha. Cada uno se lleva su tamanio (la division se da vuelta)
static void LayoutSwap(ViewportBase* aVp) {
    ViewportBase* parent = LayoutParentOf(rootViewport, aVp);
    if (!parent) return;   // el root: no tiene hermano
    const int w = rootViewport->width, h = rootViewport->height;
    if (parent->ContainerKind() == 1) {
        ViewportRow* r = (ViewportRow*)parent;
        ViewportBase* t = r->childA; r->childA = r->childB; r->childB = t;
        r->splitFrac = 1.0f - r->splitFrac;
    } else {
        ViewportColumn* c = (ViewportColumn*)parent;
        ViewportBase* t = c->childA; c->childA = c->childB; c->childB = t;
        c->splitFrac = 1.0f - c->splitFrac;
    }
    LayoutRescan(aVp, w, h);
}

// Split: en el lugar del viewport aparece una fila/columna con el
// original y un viewport NUEVO del mismo tipo (no se clona)
static void LayoutSplit(ViewportBase* aVp, bool aInRow) {
    ViewportBase* newValue = LayoutCreateViewport(aVp->ViewportKind() - 1);
    if (!newValue) return;
    int w = rootViewport->width;
    int h = rootViewport->height;
    ViewportBase* cont;
    if (aInRow) {
        cont = new ViewportRow(aVp, newValue, 0.5f);
    } else {
        cont = new ViewportColumn(aVp, newValue, 0.5f);
    }
    if (aVp == rootViewport) {
        rootViewport = cont;
    } else if (!LayoutReplaceInTree(rootViewport, aVp, cont)) {
        rootViewport = cont; // (no deberia pasar)
    }
    LayoutRescan(aVp, w, h);
}

// ROTAR: la fila (lado a lado) que tiene a este viewport pasa a ser una columna (apilados) y al reves, con los mismos
// dos paneles y la misma proporcion
static void LayoutRotate(ViewportBase* aVp) {
    ViewportBase* parent = rootViewport ? LayoutParentOf(rootViewport, aVp) : NULL;
    if (!parent) return;
    const int w = rootViewport->width, h = rootViewport->height;
    ViewportBase* newValue = NULL;
    if (parent->ContainerKind() == 1) { ViewportRow* r = (ViewportRow*)parent; newValue = new ViewportColumn(r->childA, r->childB, r->splitFrac); }
    else                             { ViewportColumn* c = (ViewportColumn*)parent; newValue = new ViewportRow(c->childA, c->childB, c->splitFrac); }
    if (parent == rootViewport) rootViewport = newValue;
    else if (!LayoutReplaceInTree(rootViewport, parent, newValue)) {   // (no deberia pasar) nada cambia
        if (newValue->ContainerKind() == 1) { ((ViewportRow*)newValue)->childA = NULL; ((ViewportRow*)newValue)->childB = NULL; }
        else { ((ViewportColumn*)newValue)->childA = NULL; ((ViewportColumn*)newValue)->childB = NULL; }
        delete newValue;
        return;
    }
    // el contenedor viejo se va SIN sus paneles (ahora son del nuevo)
    if (parent->ContainerKind() == 1) { ((ViewportRow*)parent)->childA = NULL; ((ViewportRow*)parent)->childB = NULL; }
    else { ((ViewportColumn*)parent)->childA = NULL; ((ViewportColumn*)parent)->childB = NULL; }
    delete parent;
    LayoutRescan(aVp, w, h);
}
// (harness) el Rotar por la misma puerta que el menu
void LayoutRotateViewport(ViewportBase* v) { LayoutRotate(v); }

// INSTALAR un arbol de viewports ENTERO (el de fabrica, un layout guardado, el de la otra orientacion del telefono):
// reemplaza al actual y lo libera, como al abrir un proyecto (import_w3d.cpp, OpenProjectNow)
static void LayoutInstall(ViewportBase* newValue) {
    if (!newValue) return;
    ViewportBase* old = LayoutCompleteRoot();
    LayoutResetMaximized();
    rootViewport = newValue;
    rootViewport->x = 0; rootViewport->y = 0;
    if (MenuScreenW > 0 && MenuScreenH > 0) rootViewport->Resize(MenuScreenW, MenuScreenH);
    if (old && old != newValue) ViewportDeleteTree(old);
    LayoutAnchorActive();
    { ViewportBase* v3 = LayoutFirst3D(rootViewport); viewPortActive = v3 ? v3 : rootViewport; }
    if (LayoutChangedTree) LayoutChangedTree();
    g_redraw = true;
}

// el LAYOUT ELEGIDO en el menu se pone al empezar el proximo cuadro (como abrir un proyecto: el menu se abrio desde
// un viewport del arbol que se va a liberar). Lo llama ProcessPendingProject (import_w3d.cpp)
static int gLayoutPending = 0;           // 0 nada, 1 el de fabrica, 2 uno guardado (gLayoutPendingName)
static std::string gLayoutPendingName;
extern ViewportBase* W3dLayoutFromText(const std::string& text);   // import_w3d.cpp
extern std::string W3dLayoutTextOf(ViewportBase* root);            // io/SaveW3D.cpp (NULL = el actual)
void LayoutProcessPending() {
    const int that = gLayoutPending;
    gLayoutPending = 0;
    if (that == 1) LayoutInstall(LayoutDefault(MenuScreenW, MenuScreenH, NULL));
    else if (that == 2) {
        ViewportBase* n = W3dLayoutFromText(W3dLayoutText(gLayoutPendingName));
        if (n) LayoutInstall(n);
        else Notify(std::string(T("Could not load the layout")) + ": " + gLayoutPendingName, true);
    }
}
// (harness y el menu) pedir un layout: "" = el de fabrica
void LayoutRequest(const std::string& name) {
    gLayoutPending = name.empty() ? 1 : 2;
    gLayoutPendingName = name;
    g_redraw = true;
}

// TELEFONO: un layout para cada ORIENTACION (parado / acostado). Al girar la pantalla, el de la orientacion que se deja
// queda anotado y vuelve el de la otra; la primera vez, el de fabrica de esa orientacion (LayoutDefault ya arma uno
// distinto para cada una). Lo llama main.cpp al cambiar el tamano de la ventana (y al arrancar, para saber cual es).
// En la PC no hace nada: achicar la ventana no es girar un telefono
static int gOrientation = -1;                 // 0 parado, 1 acostado (-1: todavia no se midio)
static std::string gLayoutOfOrientation[2];   // el texto "Layout { ... }" de cada una
void LayoutScreenSize(int w, int h) {
    extern bool g_w3dPhone;
    if (!g_w3dPhone || w <= 0 || h <= 0 || !rootViewport) return;
    const int o = (w > h) ? 1 : 0;
    if (gOrientation < 0) { gOrientation = o; return; }
    if (o == gOrientation) return;
    gLayoutOfOrientation[gOrientation] = W3dLayoutTextOf(NULL);
    gOrientation = o;
    ViewportBase* n = gLayoutOfOrientation[o].empty() ? NULL : W3dLayoutFromText(gLayoutOfOrientation[o]);
    if (!n) n = LayoutDefault(w, h, NULL);
    LayoutInstall(n);
}
// (harness) olvidar la orientacion (cada prueba arranca de cero)
void LayoutOrientationForget() { gOrientation = -1; gLayoutOfOrientation[0].clear(); gLayoutOfOrientation[1].clear(); }

// ====================================================================
// menus desplegables compartidos
// ====================================================================

static PopupMenu* gMenuType = NULL;
static ViewportBase* gMenuTypeOf = NULL; // de que viewport se abrio

// MAXIMIZAR un viewport (fullscreen TEMPORAL, no destructivo): guarda el arbol y apunta rootViewport al
// viewport activo; restaurar vuelve el arbol intacto. Distinto de Expand (que BORRA el hermano). En
// fullscreen no se puede split/expand/cambiar tipo (el menu solo ofrece Minimize).
static ViewportBase* g_savedRoot = NULL; // != NULL => hay un viewport MAXIMIZADO
bool LayoutIsMaximized() { return g_savedRoot != NULL; }
// limpiar el estado "maximizado" SIN restaurar nada: al abrir un proyecto el arbol es NUEVO y el g_savedRoot
// viejo apunta a un arbol huerfano; si no se limpia, el menu ofrece solo "Minimizar" y minimizar instalaria ese
// arbol muerto (layout roto). Lo llama el camino de apertura de proyecto.
void LayoutResetMaximized() { g_savedRoot = NULL; }
void LayoutMaximize() {
    if (!rootViewport) return;
    if (g_savedRoot) { // ya maximizado -> RESTAURAR el arbol guardado
        int w = rootViewport->width, h = rootViewport->height;
        rootViewport = g_savedRoot; g_savedRoot = NULL;
        rootViewport->x = 0; rootViewport->y = 0; rootViewport->Resize(w, h);
    } else { // MAXIMIZAR el viewport activo (si ya es el unico, nada)
        if (!viewPortActive || viewPortActive == rootViewport) return;
        int w = rootViewport->width, h = rootViewport->height;
        g_savedRoot = rootViewport;
        rootViewport = viewPortActive;
        rootViewport->x = 0; rootViewport->y = 0; rootViewport->Resize(w, h);
    }
    g_redraw = true;
}

// MODO JUEGO (.sisx bundleado): full-screen del viewport 3D -> el juego se ve SOLO, sin el
// chrome del editor (los demas viewports). Idempotente: si ya esta maximizado, no hace nada.
void LayoutMaximize3DForGame() {
    if (!rootViewport || g_savedRoot) return;   // sin layout o ya maximizado
    std::vector<ViewportBase*> leaves;
    LayoutCollectLeaves(rootViewport, leaves);
    for (size_t i = 0; i < leaves.size(); i++)
        if (leaves[i]->ViewportKind() == 1) { viewPortActive = leaves[i]; break; } // 1 = ViewPort3D
    if (viewPortActive && viewPortActive != rootViewport) LayoutMaximize();
}

// MODO JUEGO PURO (pedido del dueno, N95: VERDE + '0'): "para ver esto al
// MAXIMO rendimiento" -- pantalla completa el viewport 3D con la vista de la
// camara del juego y NADA del editor (ni barra de menu, ni headers, ni
// overlays/gizmos, ni texto). Re-apretar restaura todo tal cual estaba.
static bool gPureMaximized = false;   // este toggle maximizo (o ya estaba)
static bool gPureViewCam   = false;   // ViewFromCameraActive previo del 3D
void LayoutPureGameToggle() {
    extern int g_pureGame;
    if (!rootViewport) return;
    if (g_pureGame == 0) {
        // ENTRAR (estado 1, juego con BANDAS): si hay OTRO panel maximizado
        // (no el 3D), des-maximizar primero
        if (g_savedRoot && rootViewport->ViewportKind() != 1) LayoutMaximize();
        bool alreadyMax = LayoutIsMaximized();
        LayoutMaximize3DForGame();
        gPureMaximized = !alreadyMax && LayoutIsMaximized();
        // la vista del JUEGO: mirar por la camara activa (aspect de la camara)
        gPureViewCam = false;
        if (viewPortActive && viewPortActive->ViewportKind() == 1 && CameraActive) {
            Viewport3D* v3 = (Viewport3D*)viewPortActive;
            gPureViewCam = v3->ViewFromCameraActive;
            v3->SetViewFromCameraActive(true);
        }
        g_pureGame = 1;
        // el CONSTRUCTOR de las rutinas: en solo-juego corre UNA vez (de nuevo al entrar)
        g_w3dOnlyGame = true; g_w3dConstructorGen++;
    } else if (g_pureGame == 1) {
        // estado 2, COVER: solo cambia la proyeccion/encuadre (ViewPort3D lo
        // lee); el layout maximizado y la vista de camara quedan como estan
        g_pureGame = 2;
    } else {
        // SALIR: restaurar vista y layout como estaban
        g_pureGame = 0;
        g_w3dOnlyGame = false;
        if (viewPortActive && viewPortActive->ViewportKind() == 1 && !gPureViewCam)
            ((Viewport3D*)viewPortActive)->SetViewFromCameraActive(false);
        if (gPureMaximized && LayoutIsMaximized()) LayoutMaximize();
        gPureMaximized = false;
    }
    g_redraw = true;
}

static void LayoutActionType(int aId);
// (harness) cambia el TIPO de un viewport por la misma puerta que el menu de tipo (ids de LayoutCreateViewport)
void LayoutChangeViewportType(ViewportBase* v, int aId) { gMenuTypeOf = v; LayoutActionType(aId); }
// opcion del menu de tipo: cambiar / expand / split / maximizar
static void LayoutActionType(int aId) {
    if (!gMenuTypeOf || !rootViewport) return;
    ViewportBase* vp = gMenuTypeOf;
    gMenuTypeOf = NULL;
    if (aId >= 100) return; // ids de los checkbox del UV editor: el item los togglea solo
    // acciones de layout: ids ALTOS para no chocar con los tipos de viewport (0..9 = tipos)
    // Maximize / Minimize: el viewport del MENU (no el activo: con el dedo, el toque en la opcion ya hizo activo al
    // viewport que esta debajo de ella -la Linea de tiempo- y se maximizaba ese)
    if (aId == 23) { if (!g_savedRoot) viewPortActive = vp; LayoutMaximize(); return; }
    if (aId == 20) { LayoutExpand(vp); return; }
    if (aId == 27) { LayoutClose(vp); return; }         // Close: el hermano toma su lugar
    if (aId == 21) { LayoutSplit(vp, true); return; }   // en columnas (lado a lado)
    if (aId == 22) { LayoutSplit(vp, false); return; }  // en filas (apilados)
    if (aId == 24) { LayoutSwap(vp); return; }          // Swap: con su hermano de la fila / columna
    if (aId == 25) { LayoutRotate(vp); return; }        // Rotar: la fila pasa a ser columna y al reves
    if (aId == 26) return;                              // (el submenu Layout: lo suyo lo hace LayoutActionLayouts)
    if (vp->ViewportKind() == aId + 1) return; // ya es de ese tipo
    int w = rootViewport->width;
    int h = rootViewport->height;
    ViewportBase* newValue = LayoutCreateViewport(aId);
    if (!newValue) return;
    if (vp == rootViewport) {
        rootViewport = newValue;
    } else if (!LayoutReplaceInTree(rootViewport, vp, newValue)) {
        delete newValue;
        return;
    }
    delete vp;
    LayoutRescan(newValue, w, h);
    // el viewport RECIEN cambiado queda ACTIVO y sin foco de barra (barFocusIndex = -1). Asi un IDE nuevo arranca
    // editando texto (no en la barra: sino en el N95 quedabas atascado, la izquierda abria el menu de tipo).
    viewPortActive = newValue;
    newValue->barFocusIndex = -1;
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
static void AfterCreateAdd(Object* newValue){
    if (!newValue) return;
    // TODA malla es un recurso de la biblioteca desde que nace ("Cubo", el siguiente "Cubo.001")
    if (newValue->getType() == ObjectType::mesh && W3dMeshSpawnedHook) W3dMeshSpawnedHook();
    // editando un PREFAB lo nuevo va adentro de su objeto raiz: suelto al lado no lo generaria ninguna instancia
    W3dPrefabAdoptNew(newValue);
    // Ctrl+Z SACA lo creado (y devuelve la seleccion de antes) y Ctrl+Y lo devuelve: UN paso, como pegar o duplicar.
    // (Antes crear no era un paso: deshacer se salteaba el objeto nuevo y deshacia lo de antes, y el objeto quedaba)
    UndoGroupStart();
    UndoCaptureSelection();
    DeselectAll();
    newValue->Select();
    UndoCaptureCreation();
    UndoGroupEnd();
    if (newValue->getType() == ObjectType::mesh && ((Mesh*)newValue)->meshType >= 0)
        OpenRedoMeshPanel((Mesh*)newValue);
}

// ============================================================================
//  EDICION DE CURVE (riel/path) VIA PROXY DE MALLA (pedido del dueno: "si
//  selecciono un riel tendria que poder entrar en modo edicion... misma edicion
//  que cualquier mesh normal. solo moves puntos... el riel no tiene caras").
//
//  Tab sobre una Curve arma una malla "<nombre> (edit)" con sus nodos + la
//  CADENA de aristas y la pone en Edit Mode: G mueve, E extruye el proximo
//  nodo -- toda la maquinaria de mesh, gratis. Al salir de Edit los nodos
//  vuelven a la curva RECORRIENDO la cadena de aristas desde una punta (el
//  orden interno de los vertices del proxy no importa). Si la cantidad de
//  nodos cambio, los canales por-nodo del .cap (rotacion/fov) ya no mapean y
//  se sueltan; la curva pasa a AUTORADA (origen vacio -> se guarda inline).
// ============================================================================
void UpdateActiveEditMesh();      // (definida mas abajo; la edicion de curve la usa al entrar)
bool g_selFaceAvailable = true;   // false = editando una malla sin caras (path): Face gris
static Curve* gCurveInEdit = NULL;   // la curva cuya malla INTERNA se esta editando

// la malla de edicion es PARTE de la curva (Curve::edicion): no hay ningun objeto de
// escena de por medio -- no se puede seleccionar, ni borrar, ni aparece en el outliner
// ("adivina que pasa si ese objeto despues se borra" -el dueno-; ya no existe ese caso).
// El viewport la dibuja explicito tras la escena para que el overlay de edit se vea.
Object* W3dActiveCurveProxy() {
    return (gCurveInEdit && gCurveInEdit->edit) ? (Object*)gCurveInEdit->edit : NULL;
}

// true si 'o' sigue vivo en la escena (la curva pudo morir con un cierre de proyecto)
static bool ObjectInScene(Object* root, Object* o){
    if (!root || !o) return false;
    if (root == o) return true;
    for (size_t i = 0; i < root->Childrens.size(); i++)
        if (ObjectInScene(root->Childrens[i], o)) return true;
    return false;
}

static void CurveEnterEdit(Curve* cv);
static void CurveFinishEdit();
// ============================================================================
//  EDICION DE SPLINES (Bezier / poly) CON MANIJAS (pedido del dueno: "falta poder editar los handlers y
//  esas cosas al entrar en modo edicion").
//
//  Mismo esquema que el path: la malla de edicion interna de la curva es el proxy, asi que seleccionar
//  (click, B, A), mover / rotar / escalar (G/R/S, gizmo, numeros, snap, proportional) y el undo de la
//  seleccion son los de siempre. Lo que cambia es QUE hay en el proxy: por cada punto Bezier tres
//  vertices sueltos -manija izquierda, punto, manija derecha- y por cada punto poly uno solo. Ninguna
//  arista: la linea que se ve es la teselacion de la curva, y las manijas las dibuja la curva con el
//  color de su tipo (Curve::RenderObject).
//
//  CurveBezSync corre en cada cuadro (UpdateActiveEditMesh) y antes de cada operacion: compara el proxy
//  con como lo dejo la vez anterior y le aplica a las splines las reglas de Blender:
//   - elegir un punto elige sus dos manijas (y soltarlo las suelta);
//   - mover el punto lleva las manijas que no se movieron;
//   - una seleccion PARCIAL del trio que se mueve pasa las Auto a Aligned y la Vector movida a Free
//     (BKE_nurb_bezt_handle_test);
//   - despues recalcula Auto / Vector / Aligned (Curve::CalcHandles) y devuelve al proxy lo corregido.
//  Las operaciones de topologia (extrude, borrar, subdividir, cerrar, invertir, tipos) se hacen sobre
//  las splines y el proxy se rearma entero. Una op de MALLA que cambie la cantidad de vertices del
//  proxy (merge, duplicar...) no tiene a que punto mapear lo nuevo: se descarta y se rearma.
// ============================================================================
struct BezVtx { int s, p, role; };                // role: 0 = manija izq, 1 = punto, 2 = manija der
static std::vector<BezVtx> gBezMap;               // vertice del proxy -> su punto
static std::vector<float> gBezLast;               // posiciones del proxy tras la ultima sincronizacion
static std::vector<unsigned char> gBezLastSel;    // ...y su seleccion
static bool gBezNeedFull = false;                 // un arrastre teselo "en vivo": falta el arbol KD

static bool CurveBezEditing() { return gCurveInEdit && gCurveInEdit->HasSplines() && gCurveInEdit->edit; }
bool LayoutCurveEditing() { return gCurveInEdit != NULL; }
bool LayoutCurveBezEditing() { return CurveBezEditing(); }
Curve* LayoutCurveInEdit() { return gCurveInEdit; }

// el holder de la malla de edicion (se crea una vez y se reusa en cada sesion)
static Mesh* CurveEditHolder(Curve* cv) {
    if (!cv->edit) {
        // SUELTO desde que nace (W3dRootCreating): es un MIEMBRO de la curva, no un objeto de escena (muere con
        // la curva en ~Curve). Asi el ctor de Object tampoco lo ELIGE: antes la seleccion pasaba al proxy, la
        // curva dejaba de ser la activa y la sesion de edicion se cerraba apenas se abria
        W3dRootCreating = true;
        cv->edit = new Mesh(NULL, Vector3(0,0,0));
        W3dRootCreating = false;
    }
    return cv->edit;
}
// el proxy en el MISMO lugar que la curva: posicion, giro y escala de MUNDO (esta suelto: su local ES su
// global). Con solo la posicion, una curva girada mostraba sus puntos en otro lado y el G los movia mal.
static void CurveEditPlace(Curve* cv, Mesh* m) {
    m->pos = cv->GetGlobalPosition();
    m->SetRot(RotGlobalOf(cv));
    m->scale = ScaleGlobalOf(cv);
}

static Vector3 BezRolePos(const CurvePoint& q, int role) { return role == 0 ? q.left : (role == 2 ? q.right : q.co); }
static unsigned char BezRoleBit(int role) { return role == 0 ? W3dCurveSelL : (role == 2 ? W3dCurveSelR : W3dCurveSelP); }

// arma el proxy desde las splines (posiciones + seleccion de cada punto)
static void CurveBezBuildProxy(Curve* cv) {
    Mesh* m = CurveEditHolder(cv);
    m->InvalidateEdit();
    delete[] m->vertex; delete[] m->normals; delete[] m->uv; delete[] m->vertexColor; delete[] m->faces;
    m->vertex = NULL; m->normals = NULL; m->uv = NULL; m->vertexColor = NULL; m->faces = NULL; m->facesSize = 0;
    m->looseVerts.clear(); m->looseEdges.clear(); m->materialsGroup.clear(); m->faces3d.clear();
    gBezMap.clear();
    for (size_t s = 0; s < cv->splines.size(); s++) {
        const CurveSpline& sp = cv->splines[s];
        for (size_t p = 0; p < sp.points.size(); p++) {
            BezVtx b; b.s = (int)s; b.p = (int)p;
            if (sp.type == W3dSplineBezier) {
                b.role = 0; gBezMap.push_back(b); b.role = 1; gBezMap.push_back(b); b.role = 2; gBezMap.push_back(b);
            } else { b.role = 1; gBezMap.push_back(b); }
        }
    }
    const int n = (int)gBezMap.size();
    m->vertexSize = n;
    if (n > 0) {
        m->vertex = new GLfloat[n * 3]; m->normals = new GLbyte[n * 3];
        m->uv = new GLfloat[n * 2];     m->vertexColor = new GLubyte[n * 4];
        for (int i = 0; i < n; i++) {
            const BezVtx& b = gBezMap[i];
            const Vector3 v = BezRolePos(cv->splines[b.s].points[b.p], b.role);
            m->vertex[i*3] = v.x; m->vertex[i*3+1] = v.y; m->vertex[i*3+2] = v.z;
            m->normals[i*3] = 0; m->normals[i*3+1] = 127; m->normals[i*3+2] = 0;
            m->uv[i*2] = m->uv[i*2+1] = 0.0f;
            m->vertexColor[i*4] = m->vertexColor[i*4+1] = m->vertexColor[i*4+2] = m->vertexColor[i*4+3] = 255;
            m->looseVerts.push_back(i);
        }
    }
    MaterialGroup g; g.startDrawn = 0; g.material = MaterialDefault;
    m->materialsGroup.push_back(g);
    m->name = cv->name;
    CurveEditPlace(cv, m);
    m->noEditable = false; m->visible = true;
    // UN VERTICE POR MANIJA aunque esten encima del punto (una Vector recien creada, una manija de largo
    // cero): el agrupamiento por posicion del editor los fundiria en uno y el mapeo se perderia. Se fija la
    // identidad y ComputeBorders la conserva (regroupPosRep = false).
    m->posRep.resize(n);
    for (int i = 0; i < n; i++) m->posRep[i] = i;
    m->ComputeBorders(true, false);
    m->EnsureEdit();
    if (m->edit) {
        for (int i = 0; i < n && i < (int)m->edit->vertSel.size(); i++) {
            const BezVtx& b = gBezMap[i];
            m->edit->vertSel[i] = (cv->splines[b.s].points[b.p].sel & BezRoleBit(b.role)) ? 1 : 0;
        }
        m->edit->activeIdx = -1;
        m->edit->Recolor();
        gBezLast.assign(m->edit->pos.begin(), m->edit->pos.end());
        gBezLastSel = m->edit->vertSel;
    } else { gBezLast.clear(); gBezLastSel.clear(); }
    g_redraw = true;
}

// el proxy -> las splines (ver el bloque de arriba). true = la forma cambio.
static bool CurveBezSync(Curve* cv) {
    Mesh* m = cv ? cv->edit : NULL;
    if (!m || !cv->HasSplines()) return false;
    const int n = (int)gBezMap.size();
    if (m->vertexSize != n || (n > 0 && !m->vertex)) { CurveBezBuildProxy(cv); return true; }
    m->EnsureEdit();
    EditMesh* e = m->edit;
    if (!e || e->NumVerts() != n || (int)e->pos.size() != n * 3 || (int)e->vertSel.size() != n ||
        (int)gBezLast.size() != n * 3 || (int)gBezLastSel.size() != n) { CurveBezBuildProxy(cv); return true; }
    // 1) SELECCION (como Blender): el punto recien elegido se lleva sus dos manijas (click, caja). Soltar SOLO el
    //    punto (shift-click) suelta las manijas; si en la misma pasada cambio una manija, el usuario eligio esa
    //    (el click en una manija de un punto entero elegido deja elegida solo la manija).
    bool selChanged = false;
    for (int k = 1; k + 1 < n; k++) {
        if (gBezMap[k].role != 1 || gBezMap[k-1].role != 0) continue;   // (un punto poly no tiene manijas)
        const bool now = e->vertSel[k] != 0, before = gBezLastSel[k] != 0;
        if (now == before) continue;
        if (now) { e->vertSel[k-1] = e->vertSel[k+1] = 1; selChanged = true; }
        else if (e->vertSel[k-1] == gBezLastSel[k-1] && e->vertSel[k+1] == gBezLastSel[k+1]) {
            e->vertSel[k-1] = e->vertSel[k+1] = 0; selChanged = true;
        }
    }
    for (int k = 0; k < n; k++) {
        if (e->vertSel[k] != gBezLastSel[k]) selChanged = true;
        CurvePoint& q = cv->splines[gBezMap[k].s].points[gBezMap[k].p];
        const unsigned char bit = BezRoleBit(gBezMap[k].role);
        if (e->vertSel[k]) q.sel |= bit; else q.sel &= (unsigned char)~bit;
    }
    if (selChanged) e->Recolor();
    // 2) POSICIONES
    std::vector<char> touched(cv->splines.size(), 0);
    bool moved = false;
    for (int k = 0; k < n; k++) {
        const BezVtx& b = gBezMap[k];
        if (b.role != 1) continue;
        CurvePoint& q = cv->splines[b.s].points[b.p];
        const float* P = &e->pos[k*3]; const float* L0 = &gBezLast[k*3];
        const Vector3 K(P[0], P[1], P[2]), Kl(L0[0], L0[1], L0[2]);
        const bool mK = (P[0] != L0[0] || P[1] != L0[1] || P[2] != L0[2]);
        const bool bez = (k > 0 && gBezMap[k-1].role == 0);
        if (!bez) { if (mK) { q.co = K; touched[b.s] = 1; moved = true; } continue; }
        const float* PL = &e->pos[(k-1)*3]; const float* LL = &gBezLast[(k-1)*3];
        const float* PR = &e->pos[(k+1)*3]; const float* LR = &gBezLast[(k+1)*3];
        const bool mL = (PL[0] != LL[0] || PL[1] != LL[1] || PL[2] != LL[2]);
        const bool mR = (PR[0] != LR[0] || PR[1] != LR[1] || PR[2] != LR[2]);
        if (!mK && !mL && !mR) continue;
        touched[b.s] = 1; moved = true;
        Vector3 L(PL[0], PL[1], PL[2]), R(PR[0], PR[1], PR[2]);
        // SELECCION PARCIAL del trio que se mueve (BKE_nurb_bezt_handle_test de Blender): la Auto deja de
        // serlo (pasa a Aligned: el usuario la esta acomodando a mano) y la Vector que se separa de su
        // punto pasa a Free
        const bool sL = e->vertSel[k-1] != 0, sK = e->vertSel[k] != 0, sR = e->vertSel[k+1] != 0;
        if (!(sL == sK && sK == sR)) {
            if (q.typeL == W3dHandleAuto) q.typeL = W3dHandleAligned;
            if (q.typeR == W3dHandleAuto) q.typeR = W3dHandleAligned;
            if (q.typeL == W3dHandleVector && sL != sK) q.typeL = W3dHandleFree;
            if (q.typeR == W3dHandleVector && sR != sK) q.typeR = W3dHandleFree;
        }
        // el punto se lleva las manijas que no se movieron solas
        if (mK) { const Vector3 d = K - Kl; if (!mL) L += d; if (!mR) R += d; }
        q.co = K; q.left = L; q.right = R;
    }
    if (moved) {
        for (size_t s = 0; s < touched.size(); s++) if (touched[s]) cv->CalcHandlesSpline((int)s);
        // lo corregido (manijas que acompanian, Auto/Vector/Aligned recalculadas) vuelve al proxy
        bool wrote = false;
        for (int k = 0; k < n; k++) {
            const BezVtx& b = gBezMap[k];
            const Vector3 v = BezRolePos(cv->splines[b.s].points[b.p], b.role);
            float* P = &e->pos[k*3];
            if (P[0] != v.x || P[1] != v.y || P[2] != v.z) { P[0] = v.x; P[1] = v.y; P[2] = v.z; wrote = true; }
        }
        if (wrote) { e->PushPositions(); e->RefreshOverlay(); }
        // arrastrando, la teselacion va EN VIVO (sin el arbol KD); al soltar se completa
        const bool live = EditXformActive();
        cv->Tessellate(!live);
        gBezNeedFull = live;
    } else if (gBezNeedFull && !EditXformActive()) {
        cv->Tessellate(true);
        gBezNeedFull = false;
    }
    if (moved || selChanged) {
        gBezLast.assign(e->pos.begin(), e->pos.end());
        gBezLastSel = e->vertSel;
        g_redraw = true;
    }
    return moved;
}
// el harness lo llama para no depender de que se dibuje un cuadro
bool LayoutCurveBezSync() { return CurveBezEditing() ? CurveBezSync(gCurveInEdit) : false; }

// ---- UNDO de las splines: un paso que intercambia el estado guardado con el vivo -----------------------
// Se guarda TODO lo que una op de curva puede tocar: las splines (con su seleccion) y, por si la op
// convirtio una curva de NODOS en splines, los nodos de antes con sus canales y su origen (.cap).
struct BezState {
    std::vector<CurveSpline> splines;
    std::vector<GLfloat> nodes, rot, fov;
    std::vector<GLushort> edges;
    std::string origin;
};
static void BezCapture(Curve* cv, BezState& st) {
    st.splines = cv->splines;
    st.nodes.clear(); st.rot.clear(); st.fov.clear(); st.edges.clear();
    if (!cv->HasSplines() && cv->vertex) {   // (de una curva de splines los nodos son derivados: no hace falta)
        st.nodes.assign(cv->vertex, cv->vertex + cv->vertexSize * 3);
        if (cv->rotNode) st.rot.assign(cv->rotNode, cv->rotNode + cv->vertexSize * 3);
        if (cv->fovNode) st.fov.assign(cv->fovNode, cv->fovNode + cv->vertexSize);
        st.edges = cv->edges;
    }
    st.origin = cv->origin;
}
static void CurveReenterEdit(Curve* cv);
static void BezRestore(Curve* cv, const BezState& st) {
    cv->splines = st.splines;
    cv->origin = st.origin;
    if (cv->HasSplines()) cv->Tessellate(true);
    else {
        const int n = (int)st.nodes.size() / 3;
        delete[] cv->vertex; cv->vertex = NULL; delete[] cv->indices; cv->indices = NULL;
        delete[] cv->rotNode; cv->rotNode = NULL; delete[] cv->fovNode; cv->fovNode = NULL;
        cv->vertexSize = n;
        if (n > 0) {
            cv->vertex = new GLfloat[n * 3]; for (int i = 0; i < n * 3; i++) cv->vertex[i] = st.nodes[i];
            cv->indices = new GLushort[n]; for (int i = 0; i < n; i++) cv->indices[i] = (GLushort)i;
            if ((int)st.rot.size() == n * 3) { cv->rotNode = new GLfloat[n * 3]; for (int i = 0; i < n * 3; i++) cv->rotNode[i] = st.rot[i]; }
            if ((int)st.fov.size() == n) { cv->fovNode = new GLfloat[n]; for (int i = 0; i < n; i++) cv->fovNode[i] = st.fov[i]; }
        }
        cv->edges = st.edges;
        cv->tessVersion++;
        cv->BuildKDTree();
        cv->RecalculateBranches();
    }
    if (gCurveInEdit == cv) CurveReenterEdit(cv);
    g_redraw = true;
}
struct BezUndo { Curve* cv; BezState st; };
static void BezUndoApply(void* d) {
    BezUndo* u = (BezUndo*)d;
    if (!u->cv) return;
    BezState now; BezCapture(u->cv, now);
    BezRestore(u->cv, u->st);
    u->st = now;
}
static void BezUndoUnlink(void* d, Object* deleted) { BezUndo* u = (BezUndo*)d; if ((Object*)u->cv == deleted) u->cv = NULL; }
static void BezUndoFree(void* d) { delete (BezUndo*)d; }
static void BezUndoPushState(Curve* cv, const BezState& before) {
    UndoExternal f; f.apply = BezUndoApply; f.unlink = BezUndoUnlink; f.free = BezUndoFree;
    BezUndo* u = new BezUndo(); u->cv = cv; u->st = before;
    UndoPushExternal(f, u);
}
// el paso de undo de una op de curva: el estado de ANTES (llamar antes de tocar nada)
static void CurveBezUndoPush(Curve* cv) {
    if (!cv) return;
    if (CurveBezEditing() && gCurveInEdit == cv) CurveBezSync(cv);
    BezState st; BezCapture(cv, st);
    BezUndoPushState(cv, st);
}
static bool BezSameShape(const std::vector<CurveSpline>& a, const std::vector<CurveSpline>& b) {
    if (a.size() != b.size()) return false;
    for (size_t s = 0; s < a.size(); s++) {
        const CurveSpline& x = a[s]; const CurveSpline& y = b[s];
        if (x.type != y.type || x.cyclic != y.cyclic || x.resolution != y.resolution || x.points.size() != y.points.size()) return false;
        for (size_t i = 0; i < x.points.size(); i++) {
            const CurvePoint& p = x.points[i]; const CurvePoint& q = y.points[i];
            if (p.co.x != q.co.x || p.co.y != q.co.y || p.co.z != q.co.z) return false;
            if (p.left.x != q.left.x || p.left.y != q.left.y || p.left.z != q.left.z) return false;
            if (p.right.x != q.right.x || p.right.y != q.right.y || p.right.z != q.right.z) return false;
            if (p.typeL != q.typeL || p.typeR != q.typeR || p.radius != q.radius || p.tilt != q.tilt) return false;
        }
    }
    return true;
}
// G/R/S sobre una curva de splines: el undo es de las SPLINES (no de las posiciones del proxy), asi deshace
// tambien los tipos que el arrastre cambio (Auto -> Aligned) y cancelar devuelve todo exacto
static bool gBezXformPending = false;
static BezState gBezXformBefore;
static void CurveBezXformBegin() {
    if (!CurveBezEditing()) return;
    CurveBezSync(gCurveInEdit);
    BezCapture(gCurveInEdit, gBezXformBefore);
    gBezXformPending = true;
}
static void CurveBezXformEnd(bool confirm) {
    if (!gBezXformPending) return;
    gBezXformPending = false;
    if (!CurveBezEditing()) return;
    Curve* cv = gCurveInEdit;
    if (confirm) {
        CurveBezSync(cv);
        if (!BezSameShape(cv->splines, gBezXformBefore.splines)) BezUndoPushState(cv, gBezXformBefore);
    } else {
        cv->splines = gBezXformBefore.splines;   // (el cancel de la malla repone despues las posiciones: son estas)
        cv->Tessellate(true);
        CurveBezBuildProxy(cv);
    }
}

// ---- OPERACIONES DE CURVA (Edit Mode) -------------------------------------------------------------------
static bool BezKnotSel(const CurvePoint& q) { return (q.sel & W3dCurveSelP) != 0; }
static bool BezSplineHasSel(const CurveSpline& sp) {
    for (size_t i = 0; i < sp.points.size(); i++) if (sp.points[i].sel) return true;
    return false;
}
static void BezSelectPoint(CurvePoint& q, bool on) { q.sel = on ? (W3dCurveSelL | W3dCurveSelP | W3dCurveSelR) : 0; }
// cierre comun de una op de topologia: manijas, teselacion y el proxy rearmado
void W3dTouchedChanges();   // (io/ProjectChanges.h: el proyecto tiene cambios sin guardar)
static void CurveBezAfterOp(Curve* cv) {
    cv->CalcHandles();
    cv->Tessellate(true);
    if (gCurveInEdit == cv) CurveBezBuildProxy(cv);
    W3dTouchedChanges();
    g_redraw = true;
}

// E: un punto NUEVO despues de cada punta seleccionada (copia de la punta, encima de ella). El move libre
// que lo lleva lo arranca LayoutExtrudeFaces (la puerta de E y del toolbar): como el extrude de la malla,
// extruir + mover es UN paso de undo. Una curva sin puntos (se borraron todos) arranca uno en el cursor 3D.
static bool CurveBezExtrude() {
    if (!CurveBezEditing()) return false;
    Curve* cv = gCurveInEdit;
    CurveBezSync(cv);
    bool any = false, empty = false;
    for (size_t s = 0; s < cv->splines.size(); s++) {
        const CurveSpline& sp = cv->splines[s];
        if (sp.points.empty()) { empty = true; continue; }
        if (sp.cyclic) continue;
        if (BezKnotSel(sp.points.front()) || BezKnotSel(sp.points.back())) any = true;
    }
    if (!any && !empty) { Notify(T("Select an end point of an open spline"), true); return false; }
    CurveBezUndoPush(cv);
    for (size_t s = 0; s < cv->splines.size(); s++) {
        CurveSpline& sp = cv->splines[s];
        if (sp.points.empty()) {
            if (any) continue;
            Matrix4 W; cv->GetWorldMatrixBase(W);
            CurvePoint q; q.co = W.Inverse() * cursor3D.pos; q.left = q.co; q.right = q.co;
            BezSelectPoint(q, true);
            sp.points.push_back(q);
            continue;
        }
        if (sp.cyclic) { for (size_t i = 0; i < sp.points.size(); i++) sp.points[i].sel = 0; continue; }
        const bool atEnd = BezKnotSel(sp.points.back());
        const bool atStart = BezKnotSel(sp.points.front()) && sp.points.size() > 1;
        for (size_t i = 0; i < sp.points.size(); i++) sp.points[i].sel = 0;   // (queda elegido SOLO lo nuevo)
        if (atEnd)   { CurvePoint q = sp.points.back();  BezSelectPoint(q, true); sp.points.push_back(q); }
        if (atStart) { CurvePoint q = sp.points.front(); BezSelectPoint(q, true); sp.points.insert(sp.points.begin(), q); }
    }
    CurveBezAfterOp(cv);
    return true;
}

// X > Vertices: borra los puntos elegidos (una spline sin puntos se va; si no queda ninguna, queda una
// vacia: la curva sigue siendo de splines y E arranca de nuevo en el cursor)
static bool CurveBezDeleteVertices() {
    if (!CurveBezEditing()) return false;
    Curve* cv = gCurveInEdit;
    CurveBezSync(cv);
    bool any = false;
    for (size_t s = 0; s < cv->splines.size(); s++)
        for (size_t i = 0; i < cv->splines[s].points.size(); i++) if (BezKnotSel(cv->splines[s].points[i])) any = true;
    if (!any) return false;
    CurveBezUndoPush(cv);
    std::vector<CurveSpline> out;
    for (size_t s = 0; s < cv->splines.size(); s++) {
        CurveSpline sp = cv->splines[s];
        std::vector<CurvePoint> keep;
        for (size_t i = 0; i < sp.points.size(); i++) if (!BezKnotSel(sp.points[i])) keep.push_back(sp.points[i]);
        if (keep.empty()) continue;
        sp.points.swap(keep);
        if (sp.points.size() < 3) sp.cyclic = false;
        out.push_back(sp);
    }
    if (out.empty()) { CurveSpline e; e.type = cv->splines.empty() ? (int)W3dSplineBezier : cv->splines[0].type; out.push_back(e); }
    cv->splines.swap(out);
    CurveBezAfterOp(cv);
    return true;
}

// X > Segments: borra los TRAMOS entre dos puntos elegidos seguidos (la spline se parte; los pedazos de un
// solo punto se van)
static bool CurveBezDeleteSegments() {
    if (!CurveBezEditing()) return false;
    Curve* cv = gCurveInEdit;
    CurveBezSync(cv);
    bool any = false;
    for (size_t s = 0; s < cv->splines.size() && !any; s++) {
        const CurveSpline& sp = cv->splines[s];
        const int np = (int)sp.points.size(), nseg = cv->SegmentCount((int)s);
        for (int g = 0; g < nseg; g++) if (BezKnotSel(sp.points[g]) && BezKnotSel(sp.points[(g + 1) % np])) { any = true; break; }
    }
    if (!any) return false;
    CurveBezUndoPush(cv);
    std::vector<CurveSpline> out;
    for (size_t s = 0; s < cv->splines.size(); s++) {
        const CurveSpline& sp = cv->splines[s];
        const int np = (int)sp.points.size(), nseg = cv->SegmentCount((int)s);
        std::vector<char> del(nseg > 0 ? nseg : 0, 0); int ndel = 0;
        for (int g = 0; g < nseg; g++) if (BezKnotSel(sp.points[g]) && BezKnotSel(sp.points[(g + 1) % np])) { del[g] = 1; ndel++; }
        if (ndel == 0) { out.push_back(sp); continue; }
        // la secuencia de puntos a recorrer: una cerrada arranca justo despues del primer tramo borrado
        int start = 0;
        if (sp.cyclic) { for (int g = 0; g < nseg; g++) if (del[g]) { start = (g + 1) % np; break; } }
        CurveSpline piece = sp; piece.cyclic = false; piece.points.clear();
        for (int c = 0; c < np; c++) {
            const int i = (start + c) % np;
            piece.points.push_back(sp.points[i]);
            const int g = i;   // el tramo que sale de este punto
            const bool last = (c == np - 1);
            if (last || (g < nseg && del[g])) {
                if (piece.points.size() >= 2) out.push_back(piece);
                piece.points.clear();
            }
        }
    }
    cv->splines.swap(out);
    if (cv->splines.empty()) { CurveSpline e; cv->splines.push_back(e); }
    CurveBezAfterOp(cv);
    return true;
}

// SUBDIVIDE: un punto en el medio de cada tramo con sus dos puntas elegidas. Bezier: de Casteljau en 0.5
// (la forma NO cambia: las manijas vecinas se acortan a la mitad y el punto nuevo queda alineado)
static bool CurveBezSubdivide() {
    if (!CurveBezEditing()) return false;
    Curve* cv = gCurveInEdit;
    CurveBezSync(cv);
    bool any = false;
    for (size_t s = 0; s < cv->splines.size() && !any; s++) {
        const CurveSpline& sp = cv->splines[s];
        const int np = (int)sp.points.size(), nseg = cv->SegmentCount((int)s);
        for (int g = 0; g < nseg; g++) if (BezKnotSel(sp.points[g]) && BezKnotSel(sp.points[(g + 1) % np])) { any = true; break; }
    }
    if (!any) { Notify(T("Select two adjacent points"), true); return false; }
    CurveBezUndoPush(cv);
    for (size_t s = 0; s < cv->splines.size(); s++) {
        CurveSpline& sp = cv->splines[s];
        const int np = (int)sp.points.size(), nseg = cv->SegmentCount((int)s);
        if (nseg <= 0) continue;
        std::vector<CurvePoint> out;
        for (int i = 0; i < np; i++) {
            out.push_back(sp.points[i]);
            if (i >= nseg) continue;
            const int j = (i + 1) % np;
            if (!BezKnotSel(sp.points[i]) || !BezKnotSel(sp.points[j])) continue;
            CurvePoint& a = out.back();
            const CurvePoint& b = sp.points[j];
            CurvePoint mid;
            BezSelectPoint(mid, true);
            mid.radius = (a.radius + b.radius) * 0.5f; mid.tilt = (a.tilt + b.tilt) * 0.5f;
            if (sp.type == W3dSplineBezier) {
                const Vector3 p01 = (a.co + a.right) * 0.5f, p12 = (a.right + b.left) * 0.5f, p23 = (b.left + b.co) * 0.5f;
                const Vector3 p012 = (p01 + p12) * 0.5f, p123 = (p12 + p23) * 0.5f;
                mid.co = (p012 + p123) * 0.5f; mid.left = p012; mid.right = p123;
                const bool autoBoth = (a.typeR == W3dHandleAuto && b.typeL == W3dHandleAuto);
                if (autoBoth) { mid.typeL = mid.typeR = W3dHandleAuto; }
                else {
                    mid.typeL = mid.typeR = W3dHandleAligned;
                    // las manijas vecinas pasan a las de de Casteljau: dejan de ser automaticas (si no, la
                    // forma se iria apenas se recalculen)
                    a.right = p01;
                    if (a.typeR == W3dHandleAuto) { a.typeR = W3dHandleAligned; if (a.typeL == W3dHandleAuto) a.typeL = W3dHandleAligned; }
                    if (a.typeR == W3dHandleVector) a.typeR = W3dHandleFree;
                    CurvePoint& bb = (j == 0) ? out[0] : sp.points[j];   // (cerrada: el siguiente del ultimo ya esta en 'out')
                    bb.left = p23;
                    if (bb.typeL == W3dHandleAuto) { bb.typeL = W3dHandleAligned; if (bb.typeR == W3dHandleAuto) bb.typeR = W3dHandleAligned; }
                    if (bb.typeL == W3dHandleVector) bb.typeL = W3dHandleFree;
                }
            } else {
                mid.co = (a.co + b.co) * 0.5f; mid.left = mid.co; mid.right = mid.co;
            }
            out.push_back(mid);
        }
        sp.points.swap(out);
    }
    CurveBezAfterOp(cv);
    return true;
}

// V: el tipo de las manijas elegidas (las dos de un punto elegido, o la manija sola). 0 Auto, 1 Vector,
// 2 Aligned, 3 Free, 4 = alternar Free / Aligned (como Blender)
static bool CurveBezSetHandleType(int t) {
    if (!CurveBezEditing()) return false;
    Curve* cv = gCurveInEdit;
    CurveBezSync(cv);
    // alternar: si alguna afectada no es Free, todas a Free; si no, todas a Aligned
    bool anyNotFree = false, any = false;
    for (size_t s = 0; s < cv->splines.size(); s++) {
        if (cv->splines[s].type != W3dSplineBezier) continue;
        for (size_t i = 0; i < cv->splines[s].points.size(); i++) {
            const CurvePoint& q = cv->splines[s].points[i];
            const bool l = (q.sel & (W3dCurveSelP | W3dCurveSelL)) != 0, r = (q.sel & (W3dCurveSelP | W3dCurveSelR)) != 0;
            if (l) { any = true; if (q.typeL != W3dHandleFree) anyNotFree = true; }
            if (r) { any = true; if (q.typeR != W3dHandleFree) anyNotFree = true; }
        }
    }
    if (!any) return false;
    static const unsigned char kType[4] = { W3dHandleAuto, W3dHandleVector, W3dHandleAligned, W3dHandleFree };
    const unsigned char want = (t == 4) ? (anyNotFree ? W3dHandleFree : W3dHandleAligned) : kType[(t >= 0 && t < 4) ? t : 0];
    CurveBezUndoPush(cv);
    for (size_t s = 0; s < cv->splines.size(); s++) {
        if (cv->splines[s].type != W3dSplineBezier) continue;
        for (size_t i = 0; i < cv->splines[s].points.size(); i++) {
            CurvePoint& q = cv->splines[s].points[i];
            if (q.sel & (W3dCurveSelP | W3dCurveSelL)) q.typeL = want;
            if (q.sel & (W3dCurveSelP | W3dCurveSelR)) q.typeR = want;
        }
    }
    CurveBezAfterOp(cv);
    return true;
}

// Alt+C: abre / cierra las splines con algo elegido
static bool CurveBezToggleCyclic() {
    if (!CurveBezEditing()) return false;
    Curve* cv = gCurveInEdit;
    CurveBezSync(cv);
    bool any = false;
    for (size_t s = 0; s < cv->splines.size(); s++) if (BezSplineHasSel(cv->splines[s]) && cv->splines[s].points.size() >= 2) any = true;
    if (!any) return false;
    CurveBezUndoPush(cv);
    for (size_t s = 0; s < cv->splines.size(); s++)
        if (BezSplineHasSel(cv->splines[s]) && cv->splines[s].points.size() >= 2) cv->splines[s].cyclic = !cv->splines[s].cyclic;
    CurveBezAfterOp(cv);
    return true;
}

// da vuelta una spline: el orden de los puntos y, en cada uno, sus manijas (y sus tipos) cambian de lado
static void BezReverse(CurveSpline& sp) {
    std::reverse(sp.points.begin(), sp.points.end());
    for (size_t i = 0; i < sp.points.size(); i++) {
        CurvePoint& q = sp.points[i];
        const Vector3 t = q.left; q.left = q.right; q.right = t;
        const unsigned char ty = q.typeL; q.typeL = q.typeR; q.typeR = ty;
        const unsigned char sl = q.sel & W3dCurveSelL, sr = q.sel & W3dCurveSelR;
        q.sel = (unsigned char)((q.sel & W3dCurveSelP) | (sl ? W3dCurveSelR : 0) | (sr ? W3dCurveSelL : 0));
    }
}

// F (Make Segment, como Blender): dos PUNTAS elegidas se unen con un tramo. De la misma spline -> se cierra;
// de dos splines -> quedan una sola (las del mismo tipo)
static bool CurveBezMakeSegment() {
    if (!CurveBezEditing()) return false;
    Curve* cv = gCurveInEdit;
    CurveBezSync(cv);
    std::vector<int> es, ee;   // spline y punta (0 = principio, 1 = final) de cada punta elegida
    for (size_t s = 0; s < cv->splines.size(); s++) {
        const CurveSpline& sp = cv->splines[s];
        if (sp.cyclic || sp.points.empty()) continue;
        if (BezKnotSel(sp.points.front())) { es.push_back((int)s); ee.push_back(0); }
        if (sp.points.size() > 1 && BezKnotSel(sp.points.back())) { es.push_back((int)s); ee.push_back(1); }
    }
    if (es.size() != 2) { Notify(T("Select two end points"), true); return false; }
    if (es[0] == es[1]) {
        if (cv->splines[es[0]].points.size() < 3) { Notify(T("Select two end points"), true); return false; }
        CurveBezUndoPush(cv);
        cv->splines[es[0]].cyclic = true;
        CurveBezAfterOp(cv);
        return true;
    }
    if (cv->splines[es[0]].type != cv->splines[es[1]].type) { Notify(T("Both splines must be of the same type"), true); return false; }
    CurveBezUndoPush(cv);
    CurveSpline a = cv->splines[es[0]], b = cv->splines[es[1]];
    if (ee[0] == 0) BezReverse(a);   // la punta elegida de 'a' tiene que ser su final...
    if (ee[1] == 1) BezReverse(b);   // ...y la de 'b' su principio
    for (size_t i = 0; i < b.points.size(); i++) a.points.push_back(b.points[i]);
    std::vector<CurveSpline> out;
    for (size_t s = 0; s < cv->splines.size(); s++) {
        if ((int)s == es[0]) out.push_back(a);
        else if ((int)s != es[1]) out.push_back(cv->splines[s]);
    }
    cv->splines.swap(out);
    CurveBezAfterOp(cv);
    return true;
}

// Shift+D: los tramos elegidos (puntos seguidos con el punto elegido) se copian como splines NUEVAS, quedan
// elegidas solo ellas y arranca el move libre (lo arranca LayoutDuplicateEdit, como en la malla)
static bool CurveBezDuplicate() {
    if (!CurveBezEditing()) return false;
    Curve* cv = gCurveInEdit;
    CurveBezSync(cv);
    std::vector<CurveSpline> add;
    for (size_t s = 0; s < cv->splines.size(); s++) {
        const CurveSpline& sp = cv->splines[s];
        const int np = (int)sp.points.size();
        bool all = np > 0;
        for (int i = 0; i < np; i++) if (!BezKnotSel(sp.points[i])) all = false;
        if (all) { CurveSpline c = sp; add.push_back(c); continue; }   // (entera: con su cierre)
        CurveSpline run = sp; run.cyclic = false; run.points.clear();
        for (int i = 0; i <= np; i++) {
            if (i < np && BezKnotSel(sp.points[i])) { run.points.push_back(sp.points[i]); continue; }
            if (!run.points.empty()) { add.push_back(run); run.points.clear(); }
        }
    }
    if (add.empty()) return false;
    CurveBezUndoPush(cv);
    for (size_t s = 0; s < cv->splines.size(); s++)
        for (size_t i = 0; i < cv->splines[s].points.size(); i++) cv->splines[s].points[i].sel = 0;
    for (size_t k = 0; k < add.size(); k++) {
        for (size_t i = 0; i < add[k].points.size(); i++) BezSelectPoint(add[k].points[i], true);
        cv->splines.push_back(add[k]);
    }
    CurveBezAfterOp(cv);
    return true;
}

// Switch Direction: da vuelta las splines con algo elegido (el principio del riel pasa a ser el final)
static bool CurveBezSwitchDirection() {
    if (!CurveBezEditing()) return false;
    Curve* cv = gCurveInEdit;
    CurveBezSync(cv);
    bool any = false;
    for (size_t s = 0; s < cv->splines.size(); s++) if (BezSplineHasSel(cv->splines[s])) any = true;
    if (!any) return false;
    CurveBezUndoPush(cv);
    for (size_t s = 0; s < cv->splines.size(); s++)
        if (BezSplineHasSel(cv->splines[s])) BezReverse(cv->splines[s]);
    CurveBezAfterOp(cv);
    return true;
}

// una curva de NODOS -> splines (una por cadena del grafo; un nodo con mas de dos vecinos es una rama y
// una spline no las tiene). false + aviso si no se puede.
static bool CurveNodesToSplines(Curve* cv, int type) {
    const int n = cv->vertexSize;
    std::vector< std::vector<int> > ady(n > 0 ? n : 0);
    if (cv->edges.empty()) { for (int i = 0; i + 1 < n; i++) { ady[i].push_back(i + 1); ady[i + 1].push_back(i); } }
    else for (size_t k = 0; k + 1 < cv->edges.size(); k += 2) {
        const int a = cv->edges[k], b = cv->edges[k + 1];
        if (a < 0 || b < 0 || a >= n || b >= n || a == b) continue;
        ady[a].push_back(b); ady[b].push_back(a);
    }
    for (int i = 0; i < n; i++) if (ady[i].size() > 2) { Notify(T("A path with branches can't become a spline"), true); return false; }
    std::vector<char> seen(n > 0 ? n : 0, 0);
    std::vector<CurveSpline> out;
    // primero las cadenas abiertas (desde una punta), despues los ciclos
    for (int pass = 0; pass < 2; pass++)
        for (int s0 = 0; s0 < n; s0++) {
            if (seen[s0] || (pass == 0 && ady[s0].size() == 2)) continue;
            CurveSpline sp; sp.type = type; sp.cyclic = (pass == 1);
            int prev = -1, cur = s0;
            while (cur >= 0 && !seen[cur]) {
                seen[cur] = 1;
                CurvePoint q; q.co = cv->GetPoint(cur); q.left = q.co; q.right = q.co;
                sp.points.push_back(q);
                int nx = -1;
                for (size_t k = 0; k < ady[cur].size(); k++) if (ady[cur][k] != prev && !seen[ady[cur][k]]) { nx = ady[cur][k]; break; }
                prev = cur; cur = nx;
            }
            if (sp.points.size() < 3) sp.cyclic = false;
            out.push_back(sp);
        }
    if (out.empty()) { CurveSpline e; e.type = type; out.push_back(e); }
    cv->splines.swap(out);
    cv->signZ = 1.0f;
    if (!cv->origin.empty()) {
        w3dLogf("[Curve] '%s' pasa a splines: AUTORADA (se guarda inline, ya no sigue al .cap)", cv->name.c_str());
        cv->origin.clear();
    }
    return true;
}

// Set Spline Type: poly <-> Bezier de las splines con algo elegido (o todas si no hay nada elegido). Sobre
// una curva de NODOS (un path / un riel), la convierte entera.
static bool CurveSetSplineType(int type) {
    Curve* cv = gCurveInEdit;
    if (!cv) return false;
    if (!cv->HasSplines()) {
        CurveFinishEdit();               // los nodos editados vuelven a la curva
        BezState before; BezCapture(cv, before);
        if (!CurveNodesToSplines(cv, type)) { CurveEnterEdit(cv); return false; }
        BezUndoPushState(cv, before);
        for (size_t s = 0; s < cv->splines.size(); s++)
            for (size_t i = 0; i < cv->splines[s].points.size(); i++) BezSelectPoint(cv->splines[s].points[i], true);
        cv->CalcHandles();
        cv->Tessellate(true);
        CurveEnterEdit(cv);
        W3dTouchedChanges();
        return true;
    }
    CurveBezSync(cv);
    bool anySel = false;
    for (size_t s = 0; s < cv->splines.size(); s++) if (BezSplineHasSel(cv->splines[s])) anySel = true;
    CurveBezUndoPush(cv);
    for (size_t s = 0; s < cv->splines.size(); s++) {
        CurveSpline& sp = cv->splines[s];
        if ((anySel && !BezSplineHasSel(sp)) || sp.type == type) continue;
        sp.type = type;
        if (type == W3dSplineBezier)   // de poly: manijas automaticas (la curva suaviza los quiebres)
            for (size_t i = 0; i < sp.points.size(); i++) {
                CurvePoint& q = sp.points[i];
                q.typeL = q.typeR = W3dHandleAuto; q.left = q.co; q.right = q.co;
                if (q.sel & W3dCurveSelP) q.sel = W3dCurveSelL | W3dCurveSelP | W3dCurveSelR;
            }
    }
    CurveBezAfterOp(cv);
    return true;
}

// LA PUERTA DE LAS TECLAS / MENUS de curvas (la llaman el harness y los menus): id de CurveMenuAction
void LayoutCurveOp(int id);
void LayoutActionObject(int aId);   // (mas abajo: Delete > Vertices de la malla, para el path de nodos)
void LayoutDuplicateEdit();          // (mas abajo: Shift+D)

// ---- el menu "Curve" (boton de contexto de la barra en Edit Mode y la tecla W) --------------------------
static PopupMenu* gMenuCurve = NULL;
static PopupMenu* gMenuCurveHandles = NULL;
static PopupMenu* gMenuCurveType = NULL;
static PopupMenu* gMenuCurveDelete = NULL;
enum {
    CURVE_EXTRUDE = 1, CURVE_SUBDIVIDE, CURVE_DEL_VERTS, CURVE_DEL_SEGS, CURVE_CYCLIC, CURVE_SWITCH,
    CURVE_SEGMENT, CURVE_DUPLICATE,
    CURVE_HANDLE_AUTO = 10, CURVE_HANDLE_VECTOR, CURVE_HANDLE_ALIGNED, CURVE_HANDLE_FREE, CURVE_HANDLE_TOGGLE,
    CURVE_TYPE_POLY = 20, CURVE_TYPE_BEZIER
};
static void CurveMenuAction(int id) { LayoutCurveOp(id); }
void LayoutCurveOp(int id) {
    switch (id) {
        case CURVE_EXTRUDE:   LayoutExtrudeFaces(); break;   // (la misma puerta que E y el boton del toolbar)
        case CURVE_SUBDIVIDE: CurveBezSubdivide(); break;
        case CURVE_DEL_VERTS:
            if (CurveBezEditing()) CurveBezDeleteVertices();
            else LayoutActionObject(361);   // (path de nodos: el Delete > Vertices de la malla, con su undo)
            break;
        case CURVE_DEL_SEGS:  CurveBezDeleteSegments(); break;
        case CURVE_CYCLIC:    CurveBezToggleCyclic(); break;
        case CURVE_SWITCH:    CurveBezSwitchDirection(); break;
        case CURVE_SEGMENT:   CurveBezMakeSegment(); break;
        case CURVE_DUPLICATE: LayoutDuplicateEdit(); break;   // (la misma puerta que Shift+D: arranca el move)
        case CURVE_HANDLE_AUTO: case CURVE_HANDLE_VECTOR: case CURVE_HANDLE_ALIGNED: case CURVE_HANDLE_FREE: case CURVE_HANDLE_TOGGLE:
            CurveBezSetHandleType(id - CURVE_HANDLE_AUTO); break;
        case CURVE_TYPE_POLY:   CurveSetSplineType(W3dSplinePoly); break;
        case CURVE_TYPE_BEZIER: CurveSetSplineType(W3dSplineBezier); break;
    }
    g_redraw = true;
}
static PopupMenu* CurveMenuHandles() {
    if (!gMenuCurveHandles) {
        gMenuCurveHandles = new PopupMenu(); gMenuCurveHandles->title = T("Set Handle Type"); gMenuCurveHandles->action = CurveMenuAction;
        gMenuCurveHandles->Add(T("Automatic"), CURVE_HANDLE_AUTO);
        gMenuCurveHandles->Add(T("Vector"), CURVE_HANDLE_VECTOR);
        gMenuCurveHandles->Add(T("Aligned"), CURVE_HANDLE_ALIGNED);
        gMenuCurveHandles->Add(T("Free"), CURVE_HANDLE_FREE);
        gMenuCurveHandles->Add(T("Toggle Free/Aligned"), CURVE_HANDLE_TOGGLE);
    }
    return gMenuCurveHandles;
}
static PopupMenu* CurveMenuType() {
    if (!gMenuCurveType) {
        gMenuCurveType = new PopupMenu(); gMenuCurveType->title = T("Set Spline Type"); gMenuCurveType->action = CurveMenuAction;
        gMenuCurveType->Add(T("Poly"), CURVE_TYPE_POLY);
        gMenuCurveType->Add(T("Bezier"), CURVE_TYPE_BEZIER);
    }
    return gMenuCurveType;
}
static PopupMenu* CurveMenuDelete() {
    if (!gMenuCurveDelete) {
        gMenuCurveDelete = new PopupMenu(); gMenuCurveDelete->title = T("Delete"); gMenuCurveDelete->action = CurveMenuAction;
        gMenuCurveDelete->Add(T("Vertices"), CURVE_DEL_VERTS);
        gMenuCurveDelete->Add(T("Segments"), CURVE_DEL_SEGS);
    }
    return gMenuCurveDelete;
}
static void CurveMenuOpen(PopupMenu* m, int mx, int my) {
    if (OpenMenu) OpenMenu->Close();
    m->Open(mx, my, MenuScreenW, MenuScreenH);
    OpenMenu = m;
}
// el menu de contexto de la curva: con splines, todas las ops; sobre un path de NODOS, lo que tiene sentido
// para el (extruir, borrar) y convertirlo en spline
void LayoutMenuCurveEdit(int mx, int my) {
    if (!gCurveInEdit) return;
    if (!gMenuCurve) { gMenuCurve = new PopupMenu(); gMenuCurve->title = T("Curve"); gMenuCurve->action = CurveMenuAction; }
    gMenuCurve->Clear();
    const bool bez = gCurveInEdit->HasSplines();
    gMenuCurve->Add(T("Extrude"), CURVE_EXTRUDE)->shortcut = "@3d.extrude";
    if (bez) {
        gMenuCurve->Add(T("Subdivide"), CURVE_SUBDIVIDE);
        gMenuCurve->Add(T("Make Segment"), CURVE_SEGMENT)->shortcut = "@3d.face";
        gMenuCurve->Add(T("Duplicate"), CURVE_DUPLICATE)->shortcut = "@3d.duplicate";
        gMenuCurve->Add(T("Delete"), 0, IconType::trash, CurveMenuDelete())->shortcut = "@3d.delete";
        gMenuCurve->Add(T("Set Handle Type"), 0, -1, CurveMenuHandles())->shortcut = "V";
        gMenuCurve->Add(T("Toggle Cyclic"), CURVE_CYCLIC)->shortcut = "@3d.cyclic";
        gMenuCurve->Add(T("Switch Direction"), CURVE_SWITCH);
    } else {
        gMenuCurve->Add(T("Delete Vertices"), CURVE_DEL_VERTS, IconType::trash)->shortcut = "@3d.delete";
    }
    gMenuCurve->Add(T("Set Spline Type"), 0, -1, CurveMenuType());
    CurveMenuOpen(gMenuCurve, mx, my);
}
// atajos directos (los consulta el handler de teclado del 3D antes que los de malla)
bool LayoutCurveHandleMenu(int mx, int my) {   // V
    if (!CurveBezEditing()) return false;
    CurveMenuOpen(CurveMenuHandles(), mx, my);
    return true;
}
bool LayoutCurveToggleCyclic() {   // Alt+C
    if (!CurveBezEditing()) return false;
    CurveBezToggleCyclic();
    return true;
}

static void CurveEnterEdit(Curve* cv){
    if (!cv || gCurveInEdit) return;
    if (cv->HasSplines()) {   // CURVA DE SPLINES: el proxy de puntos + manijas (ver el bloque de arriba)
        CurveBezBuildProxy(cv);
        gCurveInEdit = cv;
        cv->editing = true;
        gBezNeedFull = false; gBezXformPending = false;
        if (EditSelectMode != SelVertex) EditSelectMode = SelVertex;   // (puntos sueltos: no hay aristas ni caras)
        InteractionMode = EditMode;
        UpdateActiveEditMesh();
        g_redraw = true;
        return;
    }
    Mesh* m = CurveEditHolder(cv);
    // REUSO del holder: limpiar lo del uso anterior y cargar los nodos de ESTA curva
    m->InvalidateEdit();
    delete[] m->vertex; delete[] m->normals; delete[] m->uv; delete[] m->vertexColor;
    m->looseVerts.clear(); m->looseEdges.clear(); m->materialsGroup.clear();
    int n = cv->vertexSize > 0 ? cv->vertexSize : 1;
    m->vertexSize  = n;
    m->vertex      = new GLfloat[n * 3];
    m->normals     = new GLbyte [n * 3];
    m->uv          = new GLfloat[n * 2];
    m->vertexColor = new GLubyte[n * 4];
    for (int i = 0; i < n; i++) {
        if (cv->vertex && i < cv->vertexSize) {
            m->vertex[i*3] = cv->vertex[i*3]; m->vertex[i*3+1] = cv->vertex[i*3+1]; m->vertex[i*3+2] = cv->vertex[i*3+2];
        } else { m->vertex[i*3] = m->vertex[i*3+1] = m->vertex[i*3+2] = 0.0f; }
        m->normals[i*3] = 0; m->normals[i*3+1] = 127; m->normals[i*3+2] = 0;
        m->uv[i*2] = m->uv[i*2+1] = 0.0f;
        m->vertexColor[i*4] = m->vertexColor[i*4+1] = m->vertexColor[i*4+2] = m->vertexColor[i*4+3] = 255;
    }
    // aristas: el grafo explicito de la curva, o la cadena implicita de la polilinea
    if (!cv->edges.empty())
        for (size_t k = 0; k < cv->edges.size(); k++) m->looseEdges.push_back((int)cv->edges[k]);
    else
        for (int i = 0; i + 1 < n; i++) { m->looseEdges.push_back(i); m->looseEdges.push_back(i + 1); }
    if (n == 1) m->looseVerts.push_back(0);
    MaterialGroup g; g.startDrawn = 0; g.material = MaterialDefault;
    m->materialsGroup.push_back(g);
    m->name = cv->name;                    // (interno: solo para logs/overlays)
    CurveEditPlace(cv, m);                 // suelto: su transform ES la global de la curva
    m->noEditable = false; m->visible = true;
    m->ComputeBorders();
    gCurveInEdit = cv;
    cv->editing = true;   // (la geometria no tapa los nodos)
    // NO se toca la seleccion (la curva sigue siendo el objeto activo) ni su visibilidad:
    // la linea del riel queda abajo y encima se dibujan los nodos editables.
    if (EditSelectMode == SelFace) EditSelectMode = SelVertex;   // un path no tiene caras
    InteractionMode = EditMode;
    UpdateActiveEditMesh();
    m->EditSelectAll(true);
    g_redraw = true;
}

static void CurveFinishEdit(){
    Curve* cv = gCurveInEdit;
    gCurveInEdit = NULL;
    if (!cv) return;
    cv->editing = false;
    // la curva pudo BORRARSE mientras se editaba (X): el camino real de borrado la deja viva
    // en el undo (y su malla interna con ella), asi que aca solo hay que no volcar nada.
    bool curveLive = ObjectInScene(SceneCollection, cv);
    Mesh* m = cv->edit;
    if (!m) return;
    if (cv->HasSplines()) {   // SPLINES: lo ultimo del proxy a las splines y la teselacion completa (arbol KD)
        if (curveLive) { CurveBezSync(cv); cv->Tessellate(true); }
        gBezMap.clear(); gBezLast.clear(); gBezLastSel.clear();
        gBezNeedFull = false; gBezXformPending = false;
        m->InvalidateEdit();
        g_redraw = true;
        return;
    }
    if (curveLive && m->vertex && m->vertexSize > 0) {
        // WRITE-BACK EN ORDEN DE VERTICES (1:1 con el proxy): los nodos existentes CONSERVAN su
        // indice (el mapeo con las celdas del .w3dvis no se rompe con un move) y los extruidos
        // se agregan al final. Las ARISTAS son el grafo tal cual (ramas incluidas).
        const int nn = m->vertexSize;
        GLfloat* nv = new GLfloat[nn * 3];
        for (int i = 0; i < nn * 3; i++) nv[i] = m->vertex[i];
        bool changeN = (nn != cv->vertexSize);
        delete[] cv->vertex;  cv->vertex = nv; cv->vertexSize = nn;
        delete[] cv->indices; cv->indices = new GLushort[nn];
        for (int i = 0; i < nn; i++) cv->indices[i] = (GLushort)i;
        cv->edges.clear();
        for (size_t k = 0; k < m->looseEdges.size(); k++) cv->edges.push_back((GLushort)m->looseEdges[k]);
        if (changeN) {   // los canales por-nodo del .cap ya no mapean nodo a nodo
            delete[] cv->rotNode; cv->rotNode = NULL;
            delete[] cv->fovNode; cv->fovNode = NULL;
        }
        if (!cv->origin.empty()) {
            w3dLogf("[Curve] '%s' editada en el editor: pasa a AUTORADA (se guarda inline, ya no sigue al .cap)",
                    cv->name.c_str());
            cv->origin.clear();
        }
        cv->BuildKDTree();
        cv->RecalculateBranches();
        cv->tessVersion++;   // (los nodos cambiaron: el tubo / la cinta, si tiene, se rehace)
    }
    // la malla interna queda en la curva (se reusa en la proxima edicion); solo soltar el cage
    m->InvalidateEdit();
    g_redraw = true;
}

// cierre EXTERNO de la edicion de curve (lo llama el guardado del proyecto: los nodos
// editados tienen que estar volcados en la curva ANTES de serializarla)
void W3dCurveEditClose() {
    if (gCurveInEdit) { InteractionMode = ObjectMode; CurveFinishEdit(); }
}
// volver a armar la sesion de edicion de 'cv' SIN volcar el proxy (lo vivo de la curva ya es lo que vale:
// lo acaba de reponer el undo de una op de curva)
static void CurveReenterEdit(Curve* cv) {
    gCurveInEdit = NULL;
    gBezMap.clear(); gBezXformPending = false; gBezNeedFull = false;
    if (cv->edit) cv->edit->InvalidateEdit();
    CurveEnterEdit(cv);
}
// el vertice del proxy de un punto (role 0 = manija izq, 1 = punto, 2 = manija der); -1 = no esta (harness)
int LayoutCurveProxyIndex(int s, int p, int role) {
    if (!CurveBezEditing()) return -1;
    for (size_t k = 0; k < gBezMap.size(); k++)
        if (gBezMap[k].s == s && gBezMap[k].p == p && gBezMap[k].role == role) return (int)k;
    return -1;
}

void AddPlane(){    AfterCreateAdd(NewMesh(MeshType::plane, NULL, false)); }
void AddCube(){     AfterCreateAdd(NewMesh(MeshType::cube, NULL, false)); }
void AddCircle(){   AfterCreateAdd(NewMesh(MeshType::circle, NULL, false)); }
void AddUVSphere(){ AfterCreateAdd(NewMesh(MeshType::UVsphere, NULL, false)); }
void AddCone(){     AfterCreateAdd(NewMesh(MeshType::cone, NULL, false)); }
void AddCylinder(){ AfterCreateAdd(NewMesh(MeshType::cylinder, NULL, false)); }
// el Core lo bautiza "Empty" (no conoce T()): como la luz y el sonido, el editor le pone el nombre en el idioma
// elegido ("Vacio" en castellano, igual que en el menu Aniadir)
void AddEmpty(){    Empty* e = new Empty(NULL, cursor3D.pos); e->SetNameObj(T("Empty")); AfterCreateAdd(e); }
// LOD: nace sin umbrales (dibuja el ultimo hijo siempre); se cargan en el panel
void AddLOD(){      AfterCreateAdd(new LOD(NULL, cursor3D.pos)); }
void AddCulling(){  AfterCreateAdd(new Culling(NULL, cursor3D.pos)); }
void AddFog(){   AfterCreateAdd(new Fog(NULL, cursor3D.pos)); }
// un SONIDO en el cursor 3D (con el primer sonido del proyecto, si hay; se cambia en su tarjeta)
void AddSound(){
    // nace SIN sonido (Ninguno), como todo objeto nuevo: el archivo lo elige el usuario en Propiedades. Antes tomaba
    // el primero de la biblioteca, que no tiene por que ser el que se quiere.
    Sound* s = new Sound(NULL, cursor3D.pos);
    s->SetNameObj(T("Sound"));
    AfterCreateAdd(s);
}
void AddClearZ(){ AfterCreateAdd(new ClearZ(NULL, cursor3D.pos)); }
// (nace con un efecto, para que se note que hace algo: el grano, el mas barato)
void AddPostProcess(){ PostProcess* p = new PostProcess(NULL, cursor3D.pos); W3dPostEffect e; W3dPostDefaults(e, W3dPostGrain);
                       p->opt.effects.push_back(e); p->opt.active = 0; AfterCreateAdd(p); }
void AddClipRegion(){  AfterCreateAdd(new ClipRegion(NULL, cursor3D.pos)); }
// GridCull: atajo que crea un Culling con metodo=Grid (celda 16, grilla 2D XZ); se le cuelgan los objetos
// y se toca "Recalcular". El objeto es el mismo Culling: el metodo se puede cambiar despues en el panel.
void AddGridCull(){ Culling* c = new Culling(NULL, cursor3D.pos); c->method = Culling::Grid; AfterCreateAdd(c); }
// Particulas: nace emitiendo (cantidad 10/s) pero SIN textura -> no dibuja nada
// hasta que el usuario le carga un PNG en el panel
void AddParticles(){ AfterCreateAdd(new Particles(NULL, cursor3D.pos)); }

// ---- Add > Core: RUTINAS (objetos avanzados: una lista de pasos del render, sin transformacion) ----
// Cada fila crea una Rutina con ESE paso y con su nombre (el outliner se lee como el programa). Si el activo es
// una Rutina, la nueva va JUSTO DESPUES de ella y en el mismo padre: el orden del arbol es el orden de ejecucion.
static void AddRoutineWith(int type) {
    Object* active = ActiveObj;
    const bool empty = (type < 0 || type >= StepN);   // "Empty routine" (id StepN)
    Routine* r = new Routine(NULL, empty ? T("Routine") : T(W3dStepLabel(type)));
    if (!empty) { r->lists[Routine::ModeDefault].push_back(W3dNewStep(type)); r->activeStep = 0; }
    if (type == StepN + 1) {
        // BUCLE DE RUTINA: 20 vueltas y un "Trasladar" para empezar (se le agregan los pasos que dibujan)
        r->name = T("Routine loop"); r->repeat = 20.0f;
        W3dStep t = W3dNewStep(StepTranslate); t.n[0].v = 2.0f;
        r->lists[Routine::ModeDefault].push_back(t); r->activeStep = 0;
    }
    if (active && active->getType() == ObjectType::routine && active->Parent) {
        Object* old = r->Parent;
        if (old) {
            std::vector<Object*>& h = old->Childrens;
            for (size_t i = 0; i < h.size(); i++) if (h[i] == r) { h.erase(h.begin() + (long)i); break; }
        }
        Object* p = active->Parent;
        std::vector<Object*>& hp = p->Childrens;
        size_t pos = hp.size();
        for (size_t i = 0; i < hp.size(); i++) if (hp[i] == active) { pos = i + 1; break; }
        hp.insert(hp.begin() + (long)pos, r);
        r->Parent = p;
    }
    AfterCreateAdd(r);
}
// el menu Core: los pasos AGRUPADOS (Transform, Draw, Texture, Fog, Lights...) con sus iconos, el mismo del panel
// (edit/RoutineEditor.cpp); el id de cada item es el tipo de paso
void W3dRoutineMenuSteps(PopupMenu* root, std::vector<PopupMenu*>& subs, void (*action)(int), bool withEmpty);
static std::vector<PopupMenu*> gSubsCore;

// ---- Add > Physics: cuerpo rigido para el objeto ACTIVO --------------------
// No crea un objeto: le cuelga la DEFINICION de fisica (physics/W3dRigid.h)
// al activo. La caja default ENVUELVE el subarbol (mallas incluidas), medida
// en el espacio LOCAL del objeto; un Empty pelado queda con el cubo unitario.
// Tipo/masa/caja/centro/friccion/rebote se afinan en el panel (seccion Physics).
static void BoundsLocalRec(Object* o, const Matrix4& aRoot, bool isRoot,
                           float mn[3], float mx[3], bool* has) {
    Matrix4 m = aRoot;
    if (!isRoot) {
        Matrix4 loc;
        o->GetMatrixBase(loc);
        m = aRoot * loc;
    }
    if (o->getType() == ObjectType::mesh) {
        Mesh* me = (Mesh*)o;
        if (me->vertex && me->vertexSize > 0) {
            for (int i = 0; i < me->vertexSize; i++) {
                Vector3 p = m * Vector3(me->vertex[i * 3], me->vertex[i * 3 + 1],
                                        me->vertex[i * 3 + 2]);
                for (int k = 0; k < 3; k++) {
                    if (!*has || p[k] < mn[k]) mn[k] = p[k];
                    if (!*has || p[k] > mx[k]) mx[k] = p[k];
                }
                *has = true;
            }
        }
    }
    for (size_t i = 0; i < o->Childrens.size(); i++)
        BoundsLocalRec(o->Childrens[i], m, false, mn, mx, has);
}
void AddPhysics(){
    Object* o = ActiveObj;
    if (!o) { Notify(T("No active object"), true); return; }
    if (o->physics) { Notify(T("It already has physics"), true); return; }
    W3dRigidDef* d = new W3dRigidDef();
    float mn[3] = {0, 0, 0}, mx[3] = {0, 0, 0};
    bool has = false;
    Matrix4 ident; ident.Identity();
    BoundsLocalRec(o, ident, true, mn, mx, &has);
    if (has) {
        for (int k = 0; k < 3; k++) {
            d->box[k] = mx[k] - mn[k];
            if (d->box[k] < 0.02f) d->box[k] = 0.02f;
            d->center[k] = 0.5f * (mx[k] + mn[k]);
        }
    }
    o->physics = d;
    g_redraw = true;
}
// ---- Add > Hitbox: caja de deteccion (objects/Hitbox.h) ----------------------
// Con un objeto 3D ACTIVO nace como su HIJO y del tamano de sus mallas (el trigger de una
// puerta, el sensor de un enemigo); sin activo, suelta en el cursor 3D con el cubo unitario.
// ---- Add > Light Probe: la sonda de luz (objects/LightProbe.h), en el cursor 3D. Se hornea desde su tarjeta ----
void AddReflectionProbe(){   // la sonda de reflejo (objects/ReflectionProbe.h): se hornea desde su tarjeta
    AfterCreateAdd(new ReflectionProbe(NULL, cursor3D.pos));
}
void AddLightProbe(){
    AfterCreateAdd(new LightProbe(NULL, cursor3D.pos));
}
void AddHitbox(){
    Object* parent = (ActiveObj && ActiveObj->select && HitboxParentValid(ActiveObj)) ? ActiveObj : NULL;
    AfterCreateAdd(parent ? (Object*)HitboxCreateFitted(parent) : (Object*)new Hitbox(NULL, cursor3D.pos));
}
// interfaz 2D (se edita en el Editor 2D). Si el proyecto aun no tiene
// PALETAS, la default del UI ("Whisk3D") pasa a ser la del proyecto y queda
// seleccionada en la raiz nueva; con paletas ya cargadas no se mezclan
// (el usuario elige desde la tarjeta Paleta del objeto).
void AddUI(){
    UI* u = new UI(NULL, cursor3D.pos);
    if (W3dPalettes().empty()) {
        W3dPalettesAdopt(u->palettes);
        if (u->activePalette >= 0 && u->activePalette < (int)u->palettes.size())
            u->palette = u->palettes[u->activePalette].name;
    }
    AfterCreateAdd(u);
}
// (aca vivia AddControl, que creaba el objeto Script. Se dio de baja: CUALQUIER
//  objeto acepta scripts desde la pestania "Scripts" del panel, asi que un objeto cuya
//  unica razon de ser era llevar un .lua ya no tiene sentido. Los .w3d que traigan uno
//  siguen abriendo: el importador lo MIGRA a un Empty con los mismos scripts colgados.
//  Ver import_w3d.cpp.)
void AddCamera(){   AfterCreateAdd(new Camera(NULL, cursor3D.pos, Vector3(-35.0f, -45.0f, 0.0f))); }

void AddLight(){
    Light* l = Light::Create(NULL, 0, 0, 0);
    // el nombre se pone ACA y no en el constructor: la Light vive en el Core, y el Core no sabe -ni tiene por que
    // saber- que existen los idiomas. El editor la crea, el editor la nombra.
    if (l){ l->pos = cursor3D.pos; l->SetNameObj(T("Light")); }
    AfterCreateAdd(l);
}
// el SOL, el FOCO y la de AREA: la misma luz con otro tipo (el sol mira hacia abajo en diagonal; el foco y la de area, hacia abajo)
static void AddLightType(int type, const char* name, float rotX) {
    Light* l = Light::Create(NULL, 0, 0, 0);
    if (l){ l->pos = cursor3D.pos; l->SetNameObj(T(name)); W3dLightSetType(l, type); l->SetRotEuler(Vector3(rotX, 0.0f, 0.0f)); }
    AfterCreateAdd(l);
}
void AddLightSun(){  AddLightType(1, "Sun", -45.0f); }
void AddLightFocus(){ AddLightType(2, "Spot", -90.0f); }
void AddLightArea(){ AddLightType(3, "Area", -90.0f); }   // (alumbra hacia abajo, como un panel en el techo)
void AddArmature(){
    // un solo hueso desde el origen, 0.3 hacia arriba (Y), en el cursor 3D.
    Armature* arm = new Armature(NULL, cursor3D.pos);
    W3dBone b; b.name = BoneFreeName(arm, "Bone", -1); b.parent = -1;
    b.head = Vector3(0.0f, 0.0f, 0.0f); b.tail = Vector3(0.0f, 0.3f, 0.0f);
    arm->bones.push_back(b);
    AfterCreateAdd(arm);
}
void AddCollection(){
    AfterCreateAdd(new Collection(CollectionActive ? CollectionActive : SceneCollection));
}
// ---- Add > Proxy W3D: un PROXY de una libreria externa en el cursor 3D (objects/ProxyW3d.h) -------------
// Primero el EXPLORADOR DE ARCHIVOS: el .w3d que es el proxy (se vincula como libreria; si ya estaba, se usa esa). Despues
// el .w3d POR DENTRO (OpenFileBrowserLib): sus escenas se recorren como carpetas hasta el objeto (o la coleccion)
// que se quiere traer, con todo lo que cuelga de el; o un prefab, o una escena entera. El proxy nace en el cursor 3D
static std::string gPxAddLib;
static Vector3 gPxAddPos;
// lo elegido adentro de la libreria ("prefab:X", "escena:X", "Escena#Padre/Hijo") -> el proxy
bool LayoutPickedProxy(const std::string& lib, const std::string& picked, int* type, std::string* elem) {
    if (lib.empty() || picked.empty()) return false;
    if (picked.compare(0, 7, "prefab:") == 0) { *type = W3D_LIB_PREFAB; *elem = picked.substr(7); }
    else if (picked.compare(0, 6, "scene:") == 0) { *type = W3D_LIB_SCENE; *elem = picked.substr(6); }
    else { *type = W3D_LIB_SCENE; *elem = picked; }   // (un objeto de una escena)
    return !elem->empty();
}
static void ProxyAddPicked(const std::string& picked) {
    int type = W3D_LIB_PREFAB; std::string elem;
    if (!LayoutPickedProxy(gPxAddLib, picked, &type, &elem)) return;
    std::string reason;
    if (!W3dProxyAdd(gPxAddLib, type, elem, gPxAddPos, &reason)) { Notify(std::string(T(reason.c_str())), true); return; }
    // (se llama como lo que trae: el objeto, el prefab o la escena)
    const size_t d = elem.find_last_of("/#");
    if (ActiveObj && ActiveObj->getType() == ObjectType::proxy) ActiveObj->SetNameObj(d == std::string::npos ? elem : elem.substr(d + 1));
    g_redraw = true;
}
// el .w3d elegido: vinculado (o el que ya estaba) -> su contenido
static void ProxyAddW3dPicked(const std::string& path) {
    std::string lib;
    for (int i = 0; i < W3dLibsCount() && lib.empty(); i++) if (W3dLibsDiskPath(i) == path) lib = W3dLibsRow(i).name;
    if (lib.empty()) {
        std::string reason;
        if (!W3dExtLibLink(path, &reason)) { Notify(std::string(T("Not linked")) + ": " + T(reason.c_str()), true); return; }
        if (W3dLibsCount() > 0) lib = W3dLibsRow(W3dLibsCount() - 1).name;
    }
    if (lib.empty()) return;
    gPxAddLib = lib;
    OpenFileBrowserLib(T("Proxy W3D"), T("Add"), lib, ProxyAddPicked);
}
// (el harness: como si el explorador hubiera elegido ese .w3d)
void LayoutProxyW3dFrom(const std::string& path) { gPxAddPos = cursor3D.pos; ProxyAddW3dPicked(path); }
void AddProxyW3d(){
    gPxAddPos = cursor3D.pos;
    OpenFileBrowser(T("Proxy W3D"), T("Open"), ".w3d", ProxyAddW3dPicked, false);
}
// ---- Add > Modifier Objects: objetos que dibujan OTRO objeto (su target: el activo al crearlos, o ninguno; se cambia
// en su tarjeta) -------------------------------------------------------------------------------------------------
// INSTANCIA: el target una vez mas, donde esta la instancia
void AddDuplicateLinked(){
    Instance* inst = new Instance(NULL, ActiveObj); inst->pos = cursor3D.pos; AfterCreateAdd(inst);
}
// ARRAY: el target N veces, cada copia corrida por la posicion y la rotacion del Array (la cantidad: su tarjeta)
void AddArray(){
    Instance* inst = new Instance(NULL, ActiveObj);
    inst->count = 3; inst->pos = Vector3(2, 0, 0); inst->SetNameObj("Array"); AfterCreateAdd(inst);
}
// ESPEJO: el target reflejado por el plano del espejo (objects/Mirror.h: los ejes, otros targets, el reflejo en agua,
// el recorte al rectangulo del plano). Nace en el cursor 3D reflejando en X
void AddMirror(){
    Object* tgt = ActiveObj;
    Mirror* mi = new Mirror(NULL, cursor3D.pos, true, false, false);
    mi->SetNameObj("Mirror");
    if (tgt && tgt != (Object*)mi) { mi->target = tgt; mi->targetName = tgt->name; }
    AfterCreateAdd(mi);
}
void AddVertex(){
    // se crea y ya entras a EDITARLO, con el vertice elegido. Un vert suelto es lo unico que no se puede tocar
    // desde Object Mode (ni se ve): crearlo y tener que entrar a mano cada vez era un paso al pedo.
    Mesh* m = (Mesh*)NewMesh(MeshType::vertex, NULL, false);
    AfterCreateAdd(m);
    if (m){ if (InteractionMode != EditMode) LayoutToggleEditMode(); m->EditSelectAll(true); }
}
void AddPath(){
    // un PATH = una CURVE de verdad (el mismo objeto que hace de riel de la camara y del
    // modificador Oclusion -- "vi que creo un riel... y no es un riel! es un mesh!" -el
    // dueno-). Nace AUTORADA (sin .cap, se guarda inline) con 2 nodos en el cursor, y
    // entra DIRECTO a edicion via el proxy de malla: G mueve, E extruye el proximo nodo.
    Curve* cv = new Curve(NULL, cursor3D.pos);
    cv->SetNameObj("Path");
    cv->vertexSize = 2;
    cv->vertex = new GLfloat[6];
    cv->vertex[0] = 0.0f; cv->vertex[1] = 0.0f; cv->vertex[2] = 0.0f;
    cv->vertex[3] = 1.0f; cv->vertex[4] = 0.0f; cv->vertex[5] = 0.0f;
    cv->indices = new GLushort[2]; cv->indices[0] = 0; cv->indices[1] = 1;
    cv->signZ = 1.0f;               // autorada en el espacio del motor (sin espejo de .cap)
    AfterCreateAdd(cv);
    CurveEnterEdit(cv);
}
// ---- Add > Curve: BEZIER y CIRCULO (curvas de splines con manijas) -------------------------------------
// Como en Blender nacen en Modo Objeto (Tab para editarlas) y acostadas en el piso: el plano XY de Blender
// es el XZ de aca (Y arriba). La Bezier trae las manijas Aligned de la de Blender; el circulo, 4 puntos Auto.
static Curve* NewSplineCurve(const char* name) {
    Curve* cv = new Curve(NULL, cursor3D.pos);
    cv->SetNameObj(name);
    cv->signZ = 1.0f;               // autorada en el espacio del motor (sin espejo de .cap)
    cv->splines.push_back(CurveSpline());
    return cv;
}
Curve* AddBezierCurveObj() {
    Curve* cv = NewSplineCurve("BezierCurve");
    CurveSpline& sp = cv->splines[0];
    static const float K[2][3][3] = {   // { izq, punto, der } de cada punto
        { { -1.5f, 0.0f, 0.5f }, { -1.0f, 0.0f, 0.0f }, { -0.5f, 0.0f, -0.5f } },
        { {  0.5f, 0.0f, 0.5f }, {  1.0f, 0.0f, 0.0f }, {  1.5f, 0.0f, -0.5f } },
    };
    for (int i = 0; i < 2; i++) {
        CurvePoint q;
        q.left = Vector3(K[i][0][0], K[i][0][1], K[i][0][2]);
        q.co   = Vector3(K[i][1][0], K[i][1][1], K[i][1][2]);
        q.right = Vector3(K[i][2][0], K[i][2][1], K[i][2][2]);
        q.typeL = q.typeR = W3dHandleAligned;
        q.sel = W3dCurveSelL | W3dCurveSelP | W3dCurveSelR;   // (como Blender: al entrar a editarla, todo elegido)
        sp.points.push_back(q);
    }
    cv->Tessellate(true);
    return cv;
}
Curve* AddBezierCircleObj() {
    Curve* cv = NewSplineCurve("BezierCircle");
    CurveSpline& sp = cv->splines[0];
    sp.cyclic = true;
    for (int i = 0; i < 4; i++) {
        const float a = 6.2831853f * (float)i / 4.0f;
        CurvePoint q; q.co = Vector3(cosf(a), 0.0f, -sinf(a));   // (antihorario visto desde arriba, como Blender)
        q.left = q.right = q.co;
        q.sel = W3dCurveSelL | W3dCurveSelP | W3dCurveSelR;
        sp.points.push_back(q);
    }
    cv->CalcHandles();
    cv->Tessellate(true);
    return cv;
}
void AddBezierCurve()  { AfterCreateAdd(AddBezierCurveObj()); }
void AddBezierCircle() { AfterCreateAdd(AddBezierCircleObj()); }

// ============================================================================
//  CONVERT TO MESH (Object > Convert to Mesh, tarjeta Curve): la curva activa pasa a ser una MALLA comun con
//  su forma (el tubo / la cinta) o, sin forma, su linea como aristas sueltas. UN paso de undo: la malla nueva
//  aparece en el lugar de la curva (mismo padre, transform, nombre y material) y la curva se va al undo.
// ============================================================================
bool LayoutConvertCurveToMesh() {
    Curve* cv = (ActiveObj && ActiveObj->getType() == ObjectType::curve) ? (Curve*)ActiveObj : NULL;
    if (!cv) { Notify(T("Select a curve"), true); return false; }
    if (!cv->Childrens.empty()) { Notify(T("The curve has children: unparent them first"), true); return false; }
    if (gCurveInEdit == cv) { InteractionMode = ObjectMode; UpdateActiveEditMesh(); }   // (vuelca la edicion)
    if (cv->HasSplines()) cv->Tessellate(true);
    if (!cv->vertex || cv->vertexSize < 1) { Notify(T("The curve is empty"), true); return false; }
    Object* parent = cv->Parent;
    Mesh* m = new Mesh(parent, cv->pos);
    m->SetRotSnapshot(cv->Rot(), cv->rotEuler);
    m->rotMode = cv->rotMode;
    m->scale = cv->scale;
    if (!cv->BuildShapeMesh(m)) {
        // SIN FORMA: la linea teselada como vertices + aristas sueltas (como un path de malla)
        const int n = cv->vertexSize;
        m->vertexSize = n;
        m->vertex = new GLfloat[n * 3]; m->normals = new GLbyte[n * 3];
        m->uv = new GLfloat[n * 2];     m->vertexColor = new GLubyte[n * 4];
        for (int i = 0; i < n; i++) {
            m->vertex[i*3] = cv->vertex[i*3]; m->vertex[i*3+1] = cv->vertex[i*3+1]; m->vertex[i*3+2] = cv->vertex[i*3+2];
            m->normals[i*3] = 0; m->normals[i*3+1] = 127; m->normals[i*3+2] = 0;
            m->uv[i*2] = m->uv[i*2+1] = 0.0f;
            m->vertexColor[i*4] = m->vertexColor[i*4+1] = m->vertexColor[i*4+2] = m->vertexColor[i*4+3] = 255;
        }
        if (!cv->edges.empty()) for (size_t k = 0; k < cv->edges.size(); k++) m->looseEdges.push_back((int)cv->edges[k]);
        else for (int i = 0; i + 1 < n; i++) { m->looseEdges.push_back(i); m->looseEdges.push_back(i + 1); }
        if (n == 1) m->looseVerts.push_back(0);
        MaterialGroup g; g.startDrawn = 0; g.material = cv->material ? cv->material : MaterialDefault;
        m->materialsGroup.push_back(g);
        m->ComputeBorders();
    }
    const std::string name = cv->name;
    UndoGroupStart();
    UndoCaptureSelection();
    // la malla nueva, en el lugar de la curva en su padre (el outliner no la manda al final)
    if (W3dMeshSpawnedHook) W3dMeshSpawnedHook();
    MoveNextTo(m, cv, true);
    DeselectAll(); m->Select();
    UndoCaptureCreation();
    // la curva se va (detachada: Ctrl+Z la devuelve)
    DeselectAll(); cv->Select();
    UndoCaptureDeleted(false);
    // con la curva afuera, la malla se queda con su nombre
    if (m->FreeName(name) == name) W3dRenameObject(m, name, false);
    DeselectAll(); m->Select();
    UndoGroupSequentialEnd();
    W3dTouchedChanges();
    g_redraw = true;
    return true;
}

void AddReference(){
    // un plano PARADO (90 en X) = de frente en la vista, con su material y el selector de textura ya abierto:
    // imagen de referencia en un paso. NO abre el panel "Add Plane" (el selector ocupa la pantalla).
    Mesh* m = (Mesh*)NewMesh(MeshType::plane, NULL, false);
    if (!m) return;
    m->SetNameObj("Reference");
    m->SetRotEuler(Vector3(90.0f, 0.0f, 0.0f));
    DeselectAll(); m->Select();
    Material* mat = NewMaterialInMeshPart(m, 0);
    // sin LIGHTING: una referencia es una imagen, no una superficie. Sombreada por la luz de la escena se ve mas
    // oscura de un lado y no es fiel a lo que estas calcando.
    if (mat){ mat->textureOn = true; mat->lighting = false; }
    if (mat && DialogLoadTexture) DialogLoadTexture(mat);
}
void AddImportObj(){  if (LayoutImportObj)  LayoutImportObj(); }
void AddImportFbx(){  if (LayoutImportFbx)  LayoutImportFbx(); }
void AddImportGltf(){ if (LayoutImportGltf) LayoutImportGltf(); }
void AddImportGlb(){  if (LayoutImportGlb)  LayoutImportGlb(); }
void AddImportStl(){  if (LayoutImportStl)  LayoutImportStl(); }

// importar una INTERFAZ 2D (.w3dui): el arbol entero aparece en la escena, seleccionado
static void UIImportPicked(const std::string& path){
    UI* u = UI2DLoad(path);
    if (!u) { Notify(T("Could not read the file"), true); return; }
    // si el archivo vive DENTRO del proyecto abierto, recordarlo con su ruta
    // relativa al .w3d (con subcarpeta, ej "content/menu.w3dui"): el guardado
    // lo pisa EN su lugar (UI2DLoad solo guarda el nombre pelado)
    if (!w3dPath.empty()) {
        size_t s = w3dPath.find_last_of("/\\");
        std::string dir = (s == std::string::npos) ? std::string() : w3dPath.substr(0, s + 1);
        if (!dir.empty() && path.size() > dir.size() && path.compare(0, dir.size(), dir) == 0)
            u->fileW3dui = path.substr(dir.size());
    }
    // MERGE en la escena actual: importar dos veces el mismo .w3dui metia widgets
    // homonimos. La reparacion los renumera con el criterio de siempre (primero-gana)
    // y NO arrastra NINGUN vinculo: las refs de script y los targets que decian "X" ya
    // resolvian al PRIMER "X" y tienen que seguir resolviendo AHI (ver el bloque de
    // W3dNamesRepairScene; arrastrarlos era el fallo F1). El comentario viejo decia
    // lo contrario y quedo desactualizado desde ese arreglo.
    //
    // LO QUE LA REPARACION NO PUEDE ARREGLAR (y por eso se AVISA): si la RAIZ UI que
    // entra choca con una escena que ya existe, pasa a llamarse "Menu.001", pero los
    // cambiarEscena("Menu") de SU copia del .lua siguen diciendo "Menu" y resuelven a la
    // PRIMERA escena. No hay arreglo automatico posible:
    //   - el nombre de la escena viaja como STRING LITERAL adentro del .lua (no es una
    //     ref registrada en W3dScriptEntry::refs, que es lo unico que el editor sabe
    //     reescribir), y
    //   - las DOS copias comparten EL MISMO archivo .lua en disco (el .w3dui guarda la
    //     ruta del script), asi que reescribirlo para la copia nueva ROMPERIA a la
    //     original.
    // Lo unico honesto es avisar con el nombre viejo y el nuevo para que el usuario
    // edite (o duplique) el .lua a mano. Es el mismo aviso que ya da el rename manual de
    // una escena en W3dRenameObject.
    const std::string namePrev = u->name;
    W3dNamesRepairScene(true);
    if (u->name != namePrev) {
        // sin sprintf: los nombres son texto del usuario y no tienen cota de largo
        Notify(std::string(T("A scene with that name already existed, renamed:")) + " '" + namePrev + "' -> '" + u->name + "'. " +
                  T("The cambiarEscena() calls in its .lua still point to the first one: edit them by hand."), true);
    }
    DeselectAll();
    u->Select();
    g_redraw = true;
}
void AddImportUI(){
    OpenFileBrowser(T("Load UI"), T("Open"), ".w3dui .json", UIImportPicked);
}

// submenu Imports (OBJ/FBX/glTF/GLB/STL). Nombres de formato: NO se traducen (son marcas).
static const MenuDef ADD_IMPORTS[] = {
    { "OBJ",  AddImportObj,  NULL, ICON(IconType::mesh) },
    { "FBX",  AddImportFbx,  NULL, ICON(IconType::mesh) },
    { "glTF", AddImportGltf, NULL, ICON(IconType::mesh) },
    { "GLB",  AddImportGlb,  NULL, ICON(IconType::mesh) },
    { "STL",  AddImportStl,  NULL, ICON(IconType::mesh) },
    { "Whisk3D UI", AddImportUI, NULL, ICON(IconType::texture) },
};

// submenu Mesh: las PRIMITIVAS. Eran 8 de las 19 filas del menu Add y lo hacian una
// lista larguisima donde la camara y la luz quedaban perdidas al fondo. Agrupadas, el
// menu de primer nivel queda en "que TIPO de objeto" y las mallas en su propia rama.
static const MenuDef ADD_MESHES[] = {
    { "Plane",      AddPlane,           NULL, ICON(IconType::plane) },
    { "Cube",       AddCube,            NULL, ICON(IconType::object) },
    { "Circle",     AddCircle,          NULL, ICON(IconType::circle) },
    { "UV Sphere",  AddUVSphere,        NULL, ICON(IconType::circle) },
    { "Cone",       AddCone,            NULL, ICON(IconType::cone) },
    { "Cylinder",   AddCylinder,        NULL, ICON(IconType::cylinder) },
    { "Vertex",     AddVertex,          NULL, ICON(IconType::mesh) },
    { "Reference",  AddReference,       NULL, ICON(IconType::texture) },
};

// submenu Modifier Objects: los objetos que dibujan a otro (el activo al crearlos)
static const MenuDef ADD_MODIFIERS[] = {
    { "Mirror",     AddMirror,          NULL, ICON(IconType::mirror) },
    { "Array",      AddArray,           NULL, ICON(IconType::array) },
    { "Instance",   AddDuplicateLinked, NULL, ICON(IconType::instance) },
};

// submenu Curve: las curvas (Bezier y circulo con manijas; el path de nodos de siempre, que admite ramas)
static PopupMenu* MenuCurvesAdd = NULL;
static const MenuDef ADD_CURVES[] = {
    { "Bezier",     AddBezierCurve,     NULL, ICON(IconType::curve) },
    { "Circle",     AddBezierCircle,    NULL, ICON(IconType::circle) },
    { "Path",       AddPath,            NULL, ICON(IconType::curve) },
};

// submenu Lights: las luces y la sonda de luz (la luz indirecta horneada)
static const MenuDef ADD_LIGHTS[] = {
    { "Light",       AddLight,          NULL, ICON(IconType::light) },
    { "Sun",         AddLightSun,         NULL, ICON(IconType::light) },
    { "Spot",        AddLightFocus,        NULL, ICON(IconType::light) },
    { "Area",        AddLightArea,         NULL, ICON(IconType::light) },
    { "Light Probe", AddLightProbe,       NULL, ICON(IconType::sphere) },
    { "Reflection Probe", AddReflectionProbe, NULL, ICON(IconType::photo) },
};

// EL MENU ADD, de una mirada: texto + accion + icono por fila. Agregar una primitiva = una linea en
// ADD_MESHES, y ya queda conectada (no hay switch que actualizar ni id que inventar).
static const MenuDef ADD[] = {
    { "Mesh",       NULL,               NULL, ICON(IconType::mesh),   &MenuMeshes },
    { "Empty",      AddEmpty,           NULL, ICON(IconType::empty) },
    { "Modifier Objects", NULL,         NULL, ICON(IconType::instance), &MenuModifiers },
    { "LOD",        AddLOD,             NULL, ICON(IconType::array) },
    { "Culling",    AddCulling,         NULL, ICON(IconType::visible) },
    { "Grid Cull",  AddGridCull,        NULL, ICON(IconType::grid) },
    { "Particles",  AddParticles,      NULL, ICON(IconType::circle) },
    { "Physics",    AddPhysics,          NULL, ICON(IconType::object) },
    { "Hitbox",     AddHitbox,          NULL, ICON(IconType::hitbox) },
    { "Curve",      NULL,               NULL, ICON(IconType::curve),  &MenuCurvesAdd },   // Bezier, circulo y el path de nodos
    { "Armature",   AddArmature,        NULL, ICON(IconType::armature) },
    { "Camera",     AddCamera,          NULL, ICON(IconType::camera) },
    { "Lights",     NULL,               NULL, ICON(IconType::light),  &MenuLightsAdd },   // luz, sol, foco y la sonda de luz
    { "Sound",      AddSound,          NULL, ICON(IconType::sound) },
    { "Fog",        AddFog,          NULL, ICON(IconType::fog) },
    { "Clear Depth", AddClearZ,       NULL, ICON(IconType::clearz) },
    { "Post Processing", AddPostProcess, NULL, ICON(IconType::monitor) },
    { "Screen Rect", AddClipRegion,        NULL, ICON(IconType::clip) },
    { "Collection", AddCollection,      NULL, ICON(IconType::archive) },
    { "UI",         AddUI,              NULL, ICON(IconType::texture) },
    { "Prefab",     NULL,               NULL, ICON(IconType::prefab), &MenuPrefabsAdd },   // los del proyecto
    { "Scene",      NULL,               NULL, ICON(IconType::camera), &MenuScenesAdd },    // otra escena, instanciada
    { "Proxy W3D",  AddProxyW3d,        NULL, ICON(IconType::lib) },   // un prefab/escena de una libreria
    { "Imports",    NULL,               NULL, ICON(IconType::mesh),   &MenuImports },
    { "Core (advanced)", NULL,          NULL, ICON(IconType::list),  &MenuCore },   // rutinas: pasos del render
};
// (aca estaba la fila "Script", que creaba el objeto Script. Se dio de baja: cualquier
//  objeto acepta scripts desde la pestania "Scripts" del panel.)

// arma MenuAdd + sus submenus (Mesh e Imports) desde las tablas. Lo llama ViewPort3D al crear la barra.
void LayoutBuildMenuAdd(){
    if (!MenuAdd || !MenuImports || !MenuMeshes) return;
    // "Add > Prefab": su contenido es DEL PROYECTO (se rearma cada vez que se abre el Add: LayoutSyncMenuPrefabs)
    if (MenuPrefabsAdd) { MenuPrefabsAdd->action = W3dPrefabMenuAddAction; W3dPrefabMenuAddBuild(MenuPrefabsAdd); }
    if (MenuScenesAdd) { MenuScenesAdd->action = W3dSceneMenuAddAction; W3dSceneMenuAddBuild(MenuScenesAdd); }
    MenuImports->Build(ADD_IMPORTS, (int)(sizeof(ADD_IMPORTS)/sizeof(ADD_IMPORTS[0])));
    MenuMeshes->Build(ADD_MESHES, (int)(sizeof(ADD_MESHES)/sizeof(ADD_MESHES[0])));
    if (MenuModifiers)
        MenuModifiers->Build(ADD_MODIFIERS, (int)(sizeof(ADD_MODIFIERS)/sizeof(ADD_MODIFIERS[0])));
    if (MenuLightsAdd)
        MenuLightsAdd->Build(ADD_LIGHTS, (int)(sizeof(ADD_LIGHTS)/sizeof(ADD_LIGHTS[0])));
    if (!MenuCurvesAdd) MenuCurvesAdd = new PopupMenu();   // (tiene que existir antes de armar el Add: la fila apunta a el)
    MenuCurvesAdd->Build(ADD_CURVES, (int)(sizeof(ADD_CURVES)/sizeof(ADD_CURVES[0])));
    if (MenuCore) W3dRoutineMenuSteps(MenuCore, gSubsCore, AddRoutineWith, true);
    MenuAdd->Build(ADD, (int)(sizeof(ADD)/sizeof(ADD[0])));
}
// opcion del menu Select: 0 All / 1 None / 2 Invert
static void LayoutActionSelect(int aId) {
    // estas funciones ya hacen lo correcto segun el modo (object/edit): la
    // logica vive adentro, NO aca (ni en el handler de teclado de cada OS).
    // Ctrl+Z: All/None/Invert cambian la seleccion de sub-elementos en Edit Mode (el loop -10/11/12-
    // captura solo en LayoutLoopSelectActive; 13/14 solo arman un modo, no cambian nada todavia).
    if (InteractionMode == EditMode && g_editMesh && aId <= 2) UndoCaptureSelectionEdit((Mesh*)g_editMesh);
    switch (aId) {
        case 0: SelectAllForced(); break;      // All  (A)
        case 1: DeselectAll();      break;     // None (Alt A)
        case 2: InvertSelection();      break; // Invert (Ctrl I)
        case 10: LayoutLoopSelectActive(2); break; // Loop Select (Face Loop)  - modo cara
        case 11: LayoutLoopSelectActive(0); break; // Loop Select (Edge Loop)  - modo borde
        case 12: LayoutLoopSelectActive(1); break; // Loop Select (Edge Ring)  - modo borde
        case 13: LayoutPickPathStart(false); break;   // Pick Shortest Path (caminito)
        case 14: LayoutPickPathStart(true);  break;   // + Fill Region (rellena)
        case 15: LayoutSelectLinkedGuided();   break; // Select Linked (isla conexa) en modo guiado: pide click
        case 16: LayoutLoopSelectGuided();     break; // Loop Select en modo VERTICE: guiado (click sobre un borde)
        case 30: // Box Select: arma la cruz guia y espera el arrastre (idem tecla B)
            { extern void TimelineBoxForget(); TimelineBoxForget(); }   // (la del 3D, no la del timeline)
            BoxSelectBuild(); BoxSelectMove(lastMouseX, lastMouseY); break;
    }
}

// reconstruye el menu Select segun el modo: All/None/Invert siempre; en Edit Mode agrega el
// Loop Select del sub-modo (en BORDE aclara el tipo: Edge Loop vs Edge Ring).
static void LayoutFillMenuSelect(PopupMenu* MenuSelect);
static void LayoutRebuildMenuSelect() { LayoutFillMenuSelect(MenuSelect); }
// el submenu "Select" de los menus de contexto Vertex / Edge / Face (W): lo MISMO que el boton Select de la barra, para
// elegir rapido un loop, lo vinculado, el camino mas corto... sin ir hasta arriba. Se rearma al abrirlos (depende del
// sub-modo: el loop de cara, de borde o de vertice)
static PopupMenu* gMenuCtxSelect = NULL;
static void LayoutActionSelect(int aId);
static PopupMenu* LayoutCtxSelectMenu() {
    if (!gMenuCtxSelect) { gMenuCtxSelect = new PopupMenu(); gMenuCtxSelect->title = T("Select"); gMenuCtxSelect->action = LayoutActionSelect; }
    LayoutFillMenuSelect(gMenuCtxSelect);
    return gMenuCtxSelect;
}
static void LayoutFillMenuSelect(PopupMenu* MenuSelect) {
    if (!MenuSelect) return;
    MenuSelect->Clear();
    MenuSelect->Add(T("All"), 0)->shortcut = "@3d.selectAll";
    MenuSelect->Add(T("None"), 1)->shortcut = "@3d.deselect";
    MenuSelect->Add(T("Invert"), 2)->shortcut = "@3d.invert";
    // BOX SELECT: arma la caja (igual que la tecla). El atajo va en el item para que se
    // aprenda solo -- desde el menu hace exactamente lo mismo que apretar B.
    MenuSelect->Add(T("Box Select"), 30)->shortcut = "@3d.box";
    // (sin "Ver seleccion": lo elegido se ve siempre con su contorno; que se ve y que no lo maneja el Outliner)
    if (InteractionMode == EditMode) {
        // Select Linked (L): selecciona la ISLA conexa. Desde el menu = guiado (pide click sobre el elemento).
        MenuSelect->Add(T("Select Linked"), 15)->shortcut = "@3d.linked";
        if (EditSelectMode == SelEdge) {
            MenuSelect->Add(T("Loop Select (Edge Loop)"), 11)->shortcut = "Shift Alt Click";
            MenuSelect->Add(T("Loop Select (Edge Ring)"), 12);
        } else if (EditSelectMode == SelFace) {
            MenuSelect->Add(T("Loop Select"), 10)->shortcut = "Shift Alt Click";
        } else { // VERTICE: el loop se define por un BORDE -> modo guiado (pedi click sobre un borde)
            MenuSelect->Add(T("Loop Select (Edge Loop)"), 16)->shortcut = "Shift Alt Click";
        }
        // Pick Shortest Path: en los 3 sub-modos. Guiado por cartel (click 1ro -> click 2do).
        MenuSelect->Add(T("Pick Shortest Path"), 13)->shortcut = "Ctrl Click";
        MenuSelect->Add(T("Shortest Path (Fill Region)"), 14)->shortcut = "Ctrl Shift Click";
    }
}

// arranca un transform en EDIT MODE (sobre la seleccion de malla). Devuelve true si
// estamos en Edit Mode (lo manejo aca); false = Object Mode (usar los BeginTranslate...).
// el transform de malla en curso es un EXTRUDE (no un Move/Rotate/Scale comun). Lo usa el boton
// "Repeat" del toolbar (solo aparece en extrude): confirma y vuelve a extruir la seleccion.
static bool g_extrudeInProgress = false;
bool ExtrudeInProgress(){ return g_extrudeInProgress; }

bool EditXformStart(int est, int axis) { // expuesto (lo usa el harness para testear el move undo)
    if (InteractionMode != EditMode || !g_editMesh) return false;
    // ENCADENAR G->R->S sin click de confirmacion: si ya hay un transform en curso, CONFIRMARLO primero
    // (pushea su undo + finaliza). Sino UndoEditMoveStart borraba el pendiente anterior SIN guardarlo ->
    // se perdia ese paso del Ctrl+Z (bug: extrude+move+rotate -> Ctrl+Z deshacia el extrude, no el rotate).
    if (EditXformActive()) EditXformConfirm();
    state = est; axisSelect = axis;
    if (est == rotation) gTrackballCap = false; // re-captura el angulo del trackball
    // Ctrl+Z: captura posiciones PREVIAS (move PURO; se confirma al aceptar). Una curva de splines guarda sus
    // SPLINES (tipos de manija incluidos), no las posiciones del proxy
    if (CurveBezEditing()) CurveBezXformBegin(); else UndoEditMoveStart((Mesh*)g_editMesh);
    EditXformSnapshot();
    if (!EditXformActive()) state = editNavigation; // sin seleccion: no-op
    else ToolbarRegisterAction(est == rotation ? TBRotate : est == EditScale ? TBScale : TBMove); // historial
    return true;
}

// ============================================================================
//  SNAP (imantado): estado + buscador del punto de snap bajo el cursor.
// ============================================================================
SnapCfg g_snap = { false, SNAP_CLOSEST, SNAP_VERTEX, true,true,true, true,true,true, false,false };
bool SnapEnabled(){ return g_snap.enabled; }
void SnapToggle(){ g_snap.enabled = !g_snap.enabled; Notify(T(g_snap.enabled ? "Snap: ON" : "Snap: OFF"), false); g_redraw = true; }
// resultado del ultimo snap (para dibujar el recuadro verde en el viewport)
bool  g_snapHit = false;   // hubo snap en el ultimo move
float g_snapSx = 0, g_snapSy = 0; // su posicion en pantalla (viewport-relativa)

// una malla candidata a snap? (segun Target Selection). isEdited/isActive: la editada / el objeto activo.
static bool SnapMeshCandidate(Mesh* m){
    bool isEdited = ((Object*)m == g_editMesh);
    bool isActive = (ActiveObj == (Object*)m);
    if (isEdited) return g_snap.tsEdited;
    if (isActive) return g_snap.tsActive;
    return g_snap.tsNonEdited;
}
// recolecta las mallas VISIBLES de la escena
static void SnapCollect(Object* o, std::vector<Mesh*>& out){
    if (!o) return;
    if (o->getType()==ObjectType::mesh && o->visible && SnapMeshCandidate((Mesh*)o)) out.push_back((Mesh*)o);
    for (size_t i=0;i<o->Childrens.size();i++) SnapCollect(o->Childrens[i], out);
}

// busca el punto de snap bajo el cursor. Devuelve true + el punto en MUNDO + su posicion en pantalla.
bool SnapFindTarget(int mx, int my, Viewport3D* vp, Vector3& outWorld, float& outSx, float& outSy,
                      Vector3* outEdgeA, Vector3* outEdgeB){
    if (!vp || !SceneCollection) return false;
    // el snap PROYECTA a pantalla las mallas candidatas y compara con el mouse, asi que tiene que
    // ver EXACTAMENTE lo que 'vp' dibuja: la matriz de mundo de un objeto con billboard (propio o
    // de un ancestro) depende de cual fue el ultimo W3dViewBind, y con dos viewports abiertos el
    // ultimo que dibujo NO es donde esta el mouse. Sin este bind el iman se enganchaba a una
    // proyeccion de la OTRA vista y el punto que devuelve -que despues se escribe en pos- salia de
    // una camara que el usuario ni esta mirando.
    vp->BindView();
    float lmx = (float)mx - (float)vp->x, lmy = (float)my - (float)vp->y; // a coords del viewport (ProjectPoint)
    std::vector<Mesh*> meshes; SnapCollect(SceneCollection, meshes);
    if (meshes.empty()) return false;
    // radio de enganche en pantalla (px): crece con la escala de la UI (en la PC a escala 2 son los 18 de siempre) y con
    // el DEDO mucho mas (la yema tapa el vertice: con 18 px fijos en un telefono era casi imposible enganchar)
    extern bool ToolbarUsesTouch();
    const float gsSnap = (float)(GlobalScale > 0 ? GlobalScale : 1);
    const float RAD = (ToolbarUsesTouch() ? 24.0f : 9.0f) * gsSnap;
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
        bool isEdit = (m == em);

        if (g_snap.target==SNAP_VERTEX){
            for (int v=0; v<m->vertexSize; v++){
                if (isEdit && ee){ // saltar los verts seleccionados (se estan moviendo)
                    // busca el editable de esta pos rep; si esta seleccionado, saltar
                    int rep = (int)m->posRep.size()==m->vertexSize ? m->posRep[v] : v;
                    bool sel=false; for (size_t k=0;k<ee->editVerts.size();k++) if (ee->editVerts[k]==rep && k<ee->vertSel.size() && ee->vertSel[k]){ sel=true; break; }
                    if (sel) continue;
                }
                Vector3 wp = W * Vector3(m->vertex[v*3], m->vertex[v*3+1], m->vertex[v*3+2]);
                float sx,sy; if (!vp->ProjectPoint(wp, sx, sy)) continue;
                float dx=sx-lmx, dy=sy-lmy, d=dx*dx+dy*dy;
                if (d<bestD){ bestD=d; outWorld=wp; outSx=sx; outSy=sy; found=true; }
            }
        } else if (g_snap.target==SNAP_EDGECENTER || g_snap.target==SNAP_EDGE){
            // las aristas de la malla, o las COMPARTIDAS de su recurso si su edicion sigue pendiente
            // (objects/MeshResource.h): snapear a mil arboles no carga la edicion de ninguno
            const std::vector<int>* pE = W3dMeshEdgesView(m);
            if (!pE) continue;
            const std::vector<int>& E = *pE;
            for (size_t e=0; e+1<E.size(); e+=2){
                int a=E[e], b=E[e+1];
                if (a<0||b<0||a>=m->vertexSize||b>=m->vertexSize) continue;
                Vector3 wa = W * Vector3(m->vertex[a*3],m->vertex[a*3+1],m->vertex[a*3+2]);
                Vector3 wb = W * Vector3(m->vertex[b*3],m->vertex[b*3+1],m->vertex[b*3+2]);
                if (g_snap.target==SNAP_EDGECENTER){
                    Vector3 wc = (wa+wb)*0.5f;
                    float sx,sy; if (!vp->ProjectPoint(wc,sx,sy)) continue;
                    float dx=sx-lmx,dy=sy-lmy,d=dx*dx+dy*dy;
                    if (d<bestD){ bestD=d; outWorld=wc; outSx=sx; outSy=sy; found=true; }
                } else { // EDGE: punto mas cercano del segmento (en pantalla) al cursor
                    float sax,say,sbx,sby; if (!vp->ProjectPoint(wa,sax,say)||!vp->ProjectPoint(wb,sbx,sby)) continue;
                    float ex=sbx-sax, ey=sby-say; float len2=ex*ex+ey*ey; float t=0.0f;
                    if (len2>1e-4f) t=((lmx-sax)*ex+(lmy-say)*ey)/len2; if (t<0)t=0; if (t>1)t=1;
                    float px=sax+ex*t, py=say+ey*t; float dx=px-lmx,dy=py-lmy,d=dx*dx+dy*dy;
                    if (d<bestD){ bestD=d; outWorld=wa+(wb-wa)*t; outSx=px; outSy=py; found=true;
                        if (outEdgeA) *outEdgeA=wa; if (outEdgeB) *outEdgeB=wb; } // extremos del borde ganador (mundo)
                }
            }
        } else { // SNAP_FACE / SNAP_FACECENTER: recorre las caras (trianguladas por abanico)
            // (las de la malla, o las COMPARTIDAS de su recurso: ver la rama de aristas)
            const std::vector<MeshFace>* pF = W3dMeshFacesView(m);
            if (!pF) continue;
            const std::vector<MeshFace>& F = *pF;
            for (size_t f=0; f<F.size(); f++){
                const std::vector<int>& idx = F[f].idx; int nc=(int)idx.size(); if (nc<3) continue;
                { bool range=true; for (int k=0;k<nc;k++) if (idx[k]<0||idx[k]>=m->vertexSize){range=false;break;}
                  if (!range) continue; }   // faces3d desincronizado con vertex[] (defensivo, como la rama EDGE)
                if (g_snap.target==SNAP_FACECENTER){
                    Vector3 c(0,0,0); for (int k=0;k<nc;k++){ int vi=idx[k]; c=c+Vector3(m->vertex[vi*3],m->vertex[vi*3+1],m->vertex[vi*3+2]); }
                    c = W * (c*(1.0f/(float)nc));
                    float sx,sy; if (!vp->ProjectPoint(c,sx,sy)) continue;
                    float dx=sx-lmx,dy=sy-lmy,d=dx*dx+dy*dy;
                    if (d<bestD){ bestD=d; outWorld=c; outSx=sx; outSy=sy; found=true; }
                } else { // FACE: proyecta el cursor sobre la cara (baricentrico en pantalla) -> retopologia
                    std::vector<MeshIndex> triFace; W3dTriangulateFace(m->vertex, idx, triFace);   // misma triangulacion que el render
                    for (size_t t=0; t+2<triFace.size(); t+=3){
                        int i0=triFace[t], i1=triFace[t+1], i2=triFace[t+2];
                        Vector3 w0=W*Vector3(m->vertex[i0*3],m->vertex[i0*3+1],m->vertex[i0*3+2]);
                        Vector3 w1=W*Vector3(m->vertex[i1*3],m->vertex[i1*3+1],m->vertex[i1*3+2]);
                        Vector3 w2=W*Vector3(m->vertex[i2*3],m->vertex[i2*3+1],m->vertex[i2*3+2]);
                        float x0,y0,x1,y1,x2,y2, pw0,pw1,pw2;
                        if (!vp->ProjectPoint(w0,x0,y0,&pw0)||!vp->ProjectPoint(w1,x1,y1,&pw1)||!vp->ProjectPoint(w2,x2,y2,&pw2)) continue;
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
    // CURVA DE SPLINES: un punto nuevo en cada punta elegida, y el move libre que lo lleva (un solo undo)
    if (CurveBezEditing()) {
        if (EditXformActive()) EditXformConfirm();
        if (!CurveBezExtrude()) return;
        state = translation; axisSelect = ViewAxis;
        EditXformSnapshot();
        if (!EditXformActive()) state = editNavigation;
        else { ToolbarRegisterAction(TBExtrude); g_extrudeInProgress = true; }
        return;
    }
    Mesh* m = (Mesh*)g_editMesh;
    Vector3 dir; bool constrain = false;
    if (!m->ExtrudeEdit(dir, constrain)) return;
    if (constrain) {
        EditXformStartExtrude(dir); // move por la normal promedio
    } else {
        state = translation; axisSelect = ViewAxis; // move LIBRE
        EditXformSnapshot();
        if (!EditXformActive()) state = editNavigation;
    }
    if (EditXformActive()){ ToolbarRegisterAction(TBExtrude); g_extrudeInProgress = true; } // historial + marca extrude
}

// DUPLICATE en Edit Mode (Shift+D): copia la seleccion y arranca un move LIBRE.
void LayoutDuplicateEdit() {
    if (InteractionMode != EditMode || !g_editMesh) return;
    if (CurveBezEditing()) {   // curva de splines: los tramos elegidos como splines nuevas + el move libre
        if (EditXformActive()) EditXformConfirm();
        if (!CurveBezDuplicate()) return;
        state = translation; axisSelect = ViewAxis;
        EditXformSnapshot();
        if (!EditXformActive()) state = editNavigation;
        return;
    }
    Mesh* m = (Mesh*)g_editMesh;
    if (!m->DuplicateSelectionEdit()) return;
    state = translation; axisSelect = ViewAxis;
    EditXformSnapshot();
    if (!EditXformActive()) state = editNavigation;
}

// RIP (V) en Edit Mode: SEPARA la malla a lo largo de la seleccion (loop de bordes, verts o caras). Deja
// seleccionada la pieza nueva (separada). No arranca move: la idea es separar -> L una mitad -> borrar.
void LayoutRipEdit() {
    if (InteractionMode != EditMode || !g_editMesh) return;
    if (LayoutCurveHandleMenu(lastMouseX, lastMouseY)) return;   // curva de splines: V = Set Handle Type (Blender)
    Mesh* m = (Mesh*)g_editMesh;
    if (!m->RipSelectionEdit()) { Notify(T("Rip: the selection does not separate the mesh"), true); g_redraw = true; return; }
    Notify(T("Rip: mesh separated"), false);
    g_redraw = true;
}

// Connect Vertex Path (J en Edit Mode / menu Vertex): corte RECTO en pantalla entre 2 vertices.
// La "pantalla" es el viewport 3D activo: el corte sigue la linea que ves.
namespace { struct ConnProjCtx { Viewport3D* vp; Matrix4 W; };
static bool ConnProj(void* ctx, const Vector3& local, float& sx, float& sy) {
    ConnProjCtx* c = (ConnProjCtx*)ctx; return c->vp->ProjectPoint(c->W * local, sx, sy); } }
void LayoutConnectVerticesEdit() {
    if (InteractionMode != EditMode || !g_editMesh) return;
    Mesh* m = (Mesh*)g_editMesh;
    ConnProjCtx c; c.vp = NULL;
    if (viewPortActive && viewPortActive->ViewportKind() == 1) { c.vp = (Viewport3D*)viewPortActive; c.vp->BindView(); m->GetWorldMatrix(c.W); }
    std::string msg;
    const bool ok = ConnectVerticesEdit(m, c.vp ? ConnProj : NULL, c.vp ? (void*)&c : NULL, msg);
    if (!msg.empty()) Notify(T(msg.c_str()), !ok);   // rojo si no se pudo
    g_redraw = true;
}

// Separate (P en Edit Mode / menu Mesh > Separate): mueve las caras seleccionadas a un mesh NUEVO
// (misma transform + materiales + vertex groups) y las borra del actual. Como Blender P > Selection.
void LayoutSeparateEdit() {
    if (InteractionMode != EditMode || !g_editMesh) return;
    if (SeparateSelectionEdit((Mesh*)g_editMesh)) g_redraw = true;
}

// F = "New Edge/Face from Vertices" (menu Vertex): conecta los verts seleccionados.
void LayoutNewFaceEdit() {
    if (InteractionMode != EditMode || !g_editMesh) return;
    if (CurveBezEditing()) { CurveBezMakeSegment(); return; }   // curva de splines: F = Make Segment (Blender)
    Mesh* m = (Mesh*)g_editMesh;
    m->CreateFaceEdit();
}

// Shade Smooth/Flat (menu Face): redondea/aplana las caras seleccionadas.
void LayoutShade(bool smooth) {
    if (InteractionMode != EditMode || !g_editMesh) return;
    Mesh* m = (Mesh*)g_editMesh;
    m->ShadeEdit(smooth);
}

// Mark Sharp / Clear Sharp (menu Edge o tecla W): marca/desmarca como filosos los bordes
// seleccionados. En una malla SMOOTH, un borde sharp NO promedia -> queda flat (cilindro:
// lados suaves + tapas planas + aro filoso). Ver Mesh::MarkSharpEdit / CornerNormalWithSharp.
void LayoutMarkSharp(bool sharp) {
    if (InteractionMode != EditMode || !g_editMesh) return;
    Mesh* m = (Mesh*)g_editMesh;
    m->MarkSharpEdit(sharp);
    g_redraw = true;
}

void LayoutActionObject(int aId); // (definida mas abajo; la usa el menu UV + Parent.cpp)
static void LayoutActionMesh(int aId);   // (def. mas abajo) accion del menu "Mesh" de Edit Mode
static void ActionMerge(int mode);       // (def. mas abajo) Merge de la seleccion (At Center/Cursor/Collapse/By Distance)

// ===== menu UV (tecla U / "UV"): Mark/Clear Seam + proyecciones =====
// Mark/Clear Seam: bordes MAGENTA donde el unwrap abre la costura del UV.
void LayoutMarkSeam(bool seam) {
    if (InteractionMode != EditMode || !g_editMesh) return;
    ((Mesh*)g_editMesh)->MarkSeamEdit(seam);
    g_redraw = true;
}
// Cube(0)/Cylinder(1)/Sphere(2) projection sobre las caras seleccionadas.
void LayoutProjectUV(int type) {
    if (InteractionMode != EditMode || !g_editMesh) return;
    ((Mesh*)g_editMesh)->ProjectUVFaces(type);
    g_redraw = true;
    const char* n = (type == 0) ? "cube" : (type == 1) ? "cylinder" : "sphere";
    Notify(std::string("UV: ") + T(n) + " " + T("projection"), false);
}
// Project from View (+Bounds): proyecta los verts de las caras seleccionadas a coords de
// PANTALLA -> UV (con la camara actual). bounds = re-normaliza la seleccion a [0,1].
void LayoutProjectUVFromView(bool bounds) {
    if (InteractionMode != EditMode || !g_editMesh || !Viewport3DActive) return;
    Mesh* m = (Mesh*)g_editMesh; m->EnsureEdit();
    if (!m->edit || !m->vertex) return;
    EditMesh* e = m->edit;
    std::vector<unsigned char> sel3d(m->faces3d.size(), 0); bool has = false;
    for (size_t f = 0; f < e->faceSel.size(); f++)
        if (e->faceSel[f] && f < e->faceSrc.size()) { int f3 = e->faceSrc[f]; if (f3>=0 && f3<(int)m->faces3d.size()) { sel3d[f3]=1; has=true; } }
    if (!has) { Notify(T("Project from View: select faces first"), true); return; }
    // "desde la vista" es LITERAL: las UV salen de proyectar con la camara de ESTE viewport, asi
    // que la matriz de mundo tiene que ser la que ve ese mismo viewport (con dos abiertos, la
    // efectiva la publica el que dibujo ultimo, que puede ser el otro). Ver Viewport3D::BindView.
    Viewport3DActive->BindView();
    Matrix4 W; m->GetWorldMatrix(W);
    const int nC = m->CountCorners();
    std::vector<float> uvL((size_t)nC*2, 0.0f);
    float vw = (float)Viewport3DActive->width, vh = (float)Viewport3DActive->height; if (vw<1) vw=1; if (vh<1) vh=1;
    float umin=1e30f, vmin=1e30f, umax=-1e30f, vmax=-1e30f;
    int L = 0;
    for (size_t f = 0; f < m->faces3d.size(); f++) {
        const std::vector<int>& idx = m->faces3d[f].idx; int cnt = (int)idx.size();
        if (f < sel3d.size() && sel3d[f]) for (int c = 0; c < cnt; c++) {
            const float* p = &m->vertex[idx[c]*3];
            Vector3 wp = W * Vector3(p[0], p[1], p[2]);
            float sx = 0, sy = 0; Viewport3DActive->ProjectPoint(wp, sx, sy);
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
    m->WriteUVProjection(uvL);
    g_redraw = true;
    Notify(T(bounds ? "UV: projected from view (bounds)" : "UV: projected from view"), false);
}
// las operaciones del menu UV que DESPLIEGAN (edit/UVUnwrap): 0..2 unwrap (angular/conforme/estiramiento),
// 3 smart project, 4 lightmap, 5 follow active quads, 6 reset. Avisan el resultado (islas) o el error.
static void LayoutUVOperation(int op) {
    if (InteractionMode != EditMode || !g_editMesh) return;
    Mesh* m = (Mesh*)g_editMesh; std::string msg; int n = 0; bool ok = false; char num[32]; num[0] = 0;
    switch (op) {
        case 0: case 1: case 2:
            ok = W3dUnwrap(m, op, 0.02f, msg, &n);
            if (ok) { sprintf(num, "%d", n); msg = std::string(T("Unwrap")) + ": " + num + " " + T("islands"); }
            break;
        case 3:
            ok = W3dSmartUVProject(m, 66.0f, 0.02f, msg, &n);
            if (ok) { sprintf(num, "%d", n); msg = std::string(T("Smart UV Project")) + ": " + num + " " + T("islands"); }
            break;
        case 4: ok = W3dLightmapPack(m, 0.02f, msg); if (ok) msg = T("Lightmap Pack"); break;
        case 5:
            ok = W3dFollowActiveQuads(m, msg, &n);
            if (ok) { sprintf(num, "%d", n); msg = std::string(T("Follow Active Quads")) + ": " + num + " " + T("quads"); }
            break;
        case 6: ok = W3dUVReset(m, msg); if (ok) msg = std::string(T("Reset")) + " UV"; break;
        default: return;
    }
    Notify(ok ? msg : std::string(T(msg.c_str())), !ok);
    g_redraw = true;
}
// el menu UV (tecla U o el header "UV"): operaciones sobre las CARAS seleccionadas.
static PopupMenu* gMenuUVops = NULL; // file-static: LayoutChangeMenuBar lo necesita (izq/der para salir del menu UV)
// El menu de barra ABIERTO y el ROL del boton que lo abrio. Se registran juntos en RegisterMenuBar(), que llama
// el UNICO lugar que abre estos menus. El rol sale del BOTON, asi que un menu nuevo NO SE PUEDE olvidar de
// registrarse: viene con su boton puesto.
//
// Antes esto era una lista paralela de "if (OpenMenu == MenuX) rol = BR_X" que habia que acordarse de ampliar a
// mano con cada menu nuevo. Nadie se acordaba, y el sintoma era siempre el mismo: izq/der se clavaba en ese menu y
// no se podia salir. Paso con View, con Snap, con UV y con Animation -- cuatro veces el mismo bug.
static PopupMenu* gMenuOpenBar = NULL;
static int        gMenuBarRole = -1;

// El par (menu, rol) va JUNTO: si despues se abre otro menu por otro camino, OpenMenu ya no coincide con
// gMenuOpenBar y el rol se descarta en vez de aplicarse al menu equivocado.
static void RegisterMenuBar(PopupMenu* m, Button* b){
    gMenuOpenBar = m;
    gMenuBarRole = b ? b->role : -1;
}

static PopupMenu* gMenuSnapTool = NULL; // idem: LayoutChangeMenuBar lo necesita (izq/der para salir del menu Snap)
void LayoutMenuUV(int mx, int my) {
    if (InteractionMode != EditMode || !g_editMesh) return;
    if (!gMenuUVops) {
        gMenuUVops = new PopupMenu(); gMenuUVops->title = T("UV"); gMenuUVops->action = LayoutActionObject;
        gMenuUVops->Add(T("Unwrap Angle Based"), 350);
        gMenuUVops->Add(T("Unwrap Conformal"), 365);
        gMenuUVops->Add(T("Unwrap Minimum Stretch"), 366);
        gMenuUVops->Add(T("Smart UV Project"), 351);
        gMenuUVops->Add(T("Lightmap Pack"), 367);
        gMenuUVops->Add(T("Follow Active Quads"), 352);
        gMenuUVops->Add(T("Cube Projection"), 353);
        gMenuUVops->Add(T("Cylinder Projection"), 354);
        gMenuUVops->Add(T("Sphere Projection"), 355);
        gMenuUVops->Add(T("Project from View"), 356);
        gMenuUVops->Add(T("Project from View (Bounds)"), 357);
        gMenuUVops->Add(T("Mark Seam"), 358);
        gMenuUVops->Add(T("Clear Seam"), 359);
        gMenuUVops->Add(T("Reset"), 368);
    }
    if (OpenMenu) OpenMenu->Close();
    gMenuUVops->Open(mx, my, MenuScreenW, MenuScreenH);
    OpenMenu = gMenuUVops;
}

// Recalculate Normals (menu Face): re-orienta las caras seleccionadas (o todas) hacia
// AFUERA y abre el panel "redo" con la tilde Inside (misma tarjeta que el panel de Add).
void LayoutRecalcNormals() {
    if (InteractionMode != EditMode || !g_editMesh) return;
    Mesh* m = (Mesh*)g_editMesh;
    if (m->RecalculateOrientationEdit(false)) {
        OpenRedoNormalsPanel(m);
        g_redraw = true;
    }
}

// Flip Normals (menu Mesh > Normals > Flip): invierte las normales de la seleccion (o todas). Simple.
void LayoutFlipNormals() {
    if (InteractionMode != EditMode || !g_editMesh) return;
    Mesh* m = (Mesh*)g_editMesh;
    if (m->FlipNormalsEdit()) g_redraw = true;
}

// Triangulate Faces (Ctrl+T, menu Face): parte las caras seleccionadas de >3 lados en triangulos.
void LayoutTriangulate() {
    if (InteractionMode != EditMode || !g_editMesh) return;
    Mesh* m = (Mesh*)g_editMesh;
    if (m->TriangulateSelectionEdit()) { Notify(T("Faces triangulated"), false); g_redraw = true; } // false = exito (verde)
    else Notify(T("Select faces with more than 3 sides to triangulate"), true);                     // true = error (rojo)
}

static void ActionDelete(int aId); // ejecuta Delete Vertices/Edges/Faces/Edge Loops (ids 361-364); definida mas abajo

// ===================================================================================================
//  INSERT KEYFRAME: UN SOLO punto de entrada, cuatro contextos.
//  Antes cada camino tenia su tecla y su item de menu por separado (y el case 510 ya hacia medio
//  dispatch a mano). Ahora la 'i' y todos los "Insert Keyframe" de los menus pasan por aca:
//
//    Pose Mode (huesos 3D)      -> PoseInsertKeyframe(canales)        [con menu de canales]
//    UV en modo POSE (huesos 2D)-> InsertKeyframeArm2D(mesh,canales)[con menu de canales]
//    Edit Mode (vertices)       -> VertexAnimInsertKeyframe()       [DIRECTO, sin menu]
//    UV en modo EDICION         -> VertexAnimInsertKeyframeUV()     [DIRECTO, sin menu]
//    Object Mode                -> InsertKeyframeObject(canales)    [con menu de canales]
//
//  POR QUE los vertices NO abren el menu: el menu elige entre localizacion /
//  rotacion / escala, y una pose de VERTICES no tiene esos canales - es UNA sola cosa (el array de
//  posiciones, o el de normales, o el de UV). Abrir un menu de tres opciones donde solo hay una
//  seria un click de mas por keyframe, justo en el flujo donde mas keyframes se insertan.
// ===================================================================================================
// que contexto manda AHORA: 1=pose 3D, 2=pose 2D (UV), 3=vertices (Edit Mode), 4=UV, 0=objeto.
// 'm2d' sale con la malla del rig 2D cuando devuelve 2.
// 'fromUV' = el pedido viene del UV EDITOR (su tecla I o su menu Animation). Es un dato que NO se
// puede deducir del estado global: con el 3D en Edit Mode y el UV abierto en edicion, la I del 3D
// keyframea VERTICES y la del UV keyframea el MAPEO - dos cosas distintas, como siempre fue.
static int InsertKeyframeCurrentContext(Mesh** m2d, bool fromUV) {
    if (m2d) *m2d = NULL;
    if (fromUV) {
        if (UVEditorInPoseMode()) {
            Mesh* m = (ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
            if (m && !m->Arm2DBones().empty()) { if (m2d) *m2d = m; return 2; }
        }
        return 4;   // resto de modos del UV: la capa uv de la animacion del objeto
    }
    if (InteractionMode == PoseMode) return 1;
    if (InteractionMode == EditMode && g_editMesh) return 3;
    return 0;
}
// "JUEGO" (el estado base de un juego, ver W3dRoots.h) no tiene keyframes: se animan sus CLIPS
// (eligiendolos en el timeline). true = se freno (y se aviso).
static bool KeyframeInGame() {
    if (ActiveAnimKind != 2) return false;
    Notify(T("A game has no keyframes: choose one of its clips in the timeline to animate"), true);
    return true;
}
void InsertKeyframeContext(int channels, bool fromUV) {
    extern void VertexAnimInsertKeyframe();
    extern void VertexAnimInsertKeyframeUV();
    if (KeyframeInGame()) return;
    Mesh* m2d = NULL;
    switch (InsertKeyframeCurrentContext(&m2d, fromUV)) {
        case 1: PoseInsertKeyframe(channels); break;
        case 2: {
            // el clip 2D nace al insertar el primer keyframe (como el clip del armature 3D): si el
            // timeline todavia no lo estaba mostrando, se lo pone activo (kind 4) para poder verlo.
            extern void AnimSelArm2D(Mesh*, int);
            InsertKeyframeArm2D(m2d, channels);
            if (m2d && (ActiveAnimKind != 4 || ActiveAnimMesh != m2d)) AnimSelArm2D(m2d, m2d->Arm2DActiveAnim());
            if (m2d) m2d->skinGeomVersion++;
            break;
        }
        case 3: VertexAnimInsertKeyframe(); break;   // canales NO aplica (ver comentario de arriba)
        case 4: VertexAnimInsertKeyframeUV(); break; // idem
        default: InsertKeyframeObject(channels); break;
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
void LayoutSyncInsertKeySubmenu(PopupMenu* menu, int idxItem, bool fromUV) {
    if (!menu || idxItem < 0 || idxItem >= (int)menu->items.size()) return;
    int ctx = InsertKeyframeCurrentContext(NULL, fromUV);
    menu->items[idxItem]->submenu = (ctx == 3 || ctx == 4) ? NULL : MenuInsertKey;
}
void LayoutMenuInsertKeyframe(int mx, int my, bool fromUV) {
    if (KeyframeInGame()) return;
    int ctx = InsertKeyframeCurrentContext(NULL, fromUV);
    if (ctx == 3 || ctx == 4) { InsertKeyframeContext(KfChannelAll, fromUV); return; } // vertices/UV: sin menu
    if (!MenuInsertKey) return;
    if (!MenuInsertKey->action) MenuInsertKey->action = LayoutActionObject;
    if (OpenMenu && OpenMenu != MenuInsertKey) OpenMenu->Close();
    MenuInsertKey->Open(mx, my, MenuScreenW, MenuScreenH);
    OpenMenu = MenuInsertKey;
}


// ============================================================================
//  CORTAR / COPIAR / PEGAR OBJETOS (Ctrl+X / Ctrl+C / Ctrl+V y el menu del click derecho). ENTRE VENTANAS DE WHISK3D:
//  copiar deja una carpeta propia en el portapapeles de Whisk3D (la carpeta de datos del usuario, la misma para
//  todos) y en el portapapeles del SISTEMA una marca con esa carpeta; otro Whisk3D (u otro proyecto) la lee al pegar.
//  Cada copia tiene su carpeta: lo que se pego antes sigue nombrando sus texturas aunque despues se copie otra cosa.
// ============================================================================
static const char* kMarkClipboard = "whisk3d-portapapeles:";
extern bool W3dClipboardCopy(const std::vector<Object*>& objs, const std::string& folder);   // SaveW3D.cpp
extern bool W3dClipboardPaste(const std::string& folder, std::vector<Object*>& newValue);       // import_w3d.cpp
static std::string FolderClipboard() { return w3dFileSystem::GetUserDataDir() + "/clipboard"; }
// lo elegido sin lo que ya viaja adentro de otro elegido (los hijos van con su padre)
static void PickedUp(std::vector<Object*>& out) {
    out.clear();
    for (size_t i = 0; i < ObjSelects.size(); i++) {
        Object* o = ObjSelects[i];
        if (!o || o == SceneCollection) continue;
        bool inside = false;
        for (Object* p = o->Parent; p && p != SceneCollection && !inside; p = p->Parent) if (p->select) inside = true;
        if (!inside) out.push_back(o);
    }
}
void LayoutObjectsCopy(bool cut) {
    if (InteractionMode != ObjectMode) return;
    std::vector<Object*> objs; PickedUp(objs);
    if (objs.empty()) { w3dLogfE("[portapapeles] nada elegido para copiar"); Notify(T("Select objects first"), true); return; }
    // (una carpeta POR COPIA: la hora + un contador + la direccion de algo de este proceso, para que dos Whisk3D que
    //  copian en el mismo segundo no se pisen)
    static unsigned nCopy = 0; nCopy++;
    char sub[80]; snprintf(sub, sizeof(sub), "/c%lu_%u_%lx", (unsigned long)time(NULL), nCopy, (unsigned long)(size_t)&nCopy);
    const std::string folder = FolderClipboard() + sub;
    if (!W3dClipboardCopy(objs, folder)) { w3dLogfE("[portapapeles] no pude copiar %d objeto(s) a %s", (int)objs.size(), folder.c_str()); Notify(T("Could not copy the objects"), true); return; }
    w3dEngine::W3dClipboardSet(std::string(kMarkClipboard) + folder);
    { FILE* f = fopen((FolderClipboard() + "/ultimo.txt").c_str(), "wb"); if (f) { fputs(folder.c_str(), f); fclose(f); } }
    char b[96]; snprintf(b, sizeof(b), "%s: %d", cut ? T("Cut") : T("Copied"), (int)objs.size());
    if (cut) Remove(true);   // (con su paso de undo, como Supr)
    Notify(b, false);
    g_redraw = true;
}
void LayoutObjectsPaste() {
    if (InteractionMode != ObjectMode) return;
    std::string folder, clip = w3dEngine::W3dClipboardGet();
    if (clip.compare(0, strlen(kMarkClipboard), kMarkClipboard) == 0) folder = clip.substr(strlen(kMarkClipboard));
    else {   // (el portapapeles del sistema tiene otra cosa: el ultimo que copio algun Whisk3D)
        std::vector<unsigned char> b;
        if (w3dFileSystem::ReadFileBytes(FolderClipboard() + "/ultimo.txt", b) && !b.empty()) folder.assign((const char*)&b[0], b.size());
    }
    if (folder.empty()) { Notify(T("Nothing to paste"), true); return; }
    std::vector<Object*> newValue;
    if (!W3dClipboardPaste(folder, newValue)) { Notify(T("Nothing to paste"), true); return; }
    UndoCaptureSelection();
    DeselectAll();
    for (size_t i = 0; i < newValue.size(); i++) { W3dPrefabAdoptNew(newValue[i]); newValue[i]->Select(); }
    UndoCaptureCreation();   // Ctrl+Z los saca (y Ctrl+Y los devuelve), como un duplicado
    char b[96]; snprintf(b, sizeof(b), "%s: %d", T("Pasted"), (int)newValue.size());
    Notify(b, false);
    g_redraw = true;
}

// opcion del menu Object/Mesh y su submenu Transform (ids 100-102, 300=extrude)
void LayoutActionObject(int aId) {
    switch (aId) {
        case 720: LayoutObjectsCopy(true); break;    // Cut (Ctrl+X)
        case 721: LayoutObjectsCopy(false); break;   // Copy (Ctrl+C)
        case 722: LayoutObjectsPaste(); break;       // Paste (Ctrl+V)
        case 1: DuplicatedObject(); break; // Duplicate Objects (Shift D)
        case 2: NewInstance();      break; // Duplicate Linked  (Alt D)
        case 3: OpenConfirmDelete(); break; // Delete (X): popup de confirmacion -> Si borra (con undo)
        case 5: JoinObjects(); break;       // Join (Ctrl J): une las mallas seleccionadas en el objeto activo
        // PREFABS: "Create Prefab" (lo elegido pasa a un prefab nuevo) / "Unpack Prefab" (la instancia activa)
        case 700: { std::string name, reason;
                    if (!W3dPrefabCreateFromSelection(&name, &reason)) Notify(std::string(T(reason.c_str())), true);
                    else Notify(std::string(T("Prefab created")) + ": " + name, false);
                    break; }
        case 702: LayoutConvertCurveToMesh(); break;   // Convert to Mesh (la curva activa -> malla)
        case 701: { std::string reason;
                    InstancePrefab* ip = (ActiveObj && ActiveObj->getType() == ObjectType::prefab) ? (InstancePrefab*)ActiveObj : NULL;
                    if (!ip) Notify(std::string(T("Select a prefab instance")), true);
                    else if (!W3dPrefabUnpack(ip, &reason)) Notify(std::string(T(reason.c_str())), true);
                    break; }
        // Insert Keyframe: el dispatch por CONTEXTO vive en InsertKeyframeContext (arriba).
        // 510 se conserva como ALIAS de "todos los canales" (era el id plano del menu Animation y
        // lo puede seguir mandando cualquier caller viejo); 530-533 son los items del desplegable.
        case 510:
        case 530: InsertKeyframeContext(KfChannelAll); break;   // Todos (localizacion+rotacion+escala)
        case 531: InsertKeyframeContext(KfChannelLoc);   break; // Solo localizacion
        // CAPAS de la animacion de la malla (submenu "Insert Mesh Keyframe"): explicitas, sin depender
        // del editor desde el que se inserta. Los tres son la misma animacion, distinta capa.
        case 540: { extern void VertexAnimInsertKeyframe();          VertexAnimInsertKeyframe();          break; }
        case 541: { extern void VertexAnimInsertKeyframeNormals();  VertexAnimInsertKeyframeNormals();  break; }
        case 542: { extern void VertexAnimInsertKeyframeUV();        VertexAnimInsertKeyframeUV();        break; }
        case 532: InsertKeyframeContext(KfChannelRot);   break; // Solo rotacion
        case 533: InsertKeyframeContext(KfChannelScl);   break; // Solo escala
        case 511: if (InteractionMode == PoseMode) PoseDeleteKeyframe();   else DeleteKeyframeObject();  break; // Delete Keyframe (frame actual)
        case 512: if (InteractionMode == PoseMode) PoseClearKeyframes(); else ClearKeyframeObject(); break;     // Clear Keyframe (toda la animacion)
        case 513: g_redraw = true; break;          // Animation > Motion Trail: el checkbox ya lo toggleo PopupMenu
                                                   // (y a proposito NO cierra el menu)
        case 514: {                                // (compat) Delete Normals Layer: hoy es Delete Mesh Layer > Normals
            extern void VertexAnimDeleteLayerActiveNormals();
            VertexAnimDeleteLayerActiveNormals();
            break;
        }
        // Animation > Delete Mesh Layer: la capa entera de la animacion de la malla (0 vertices, 1 UV, 2 normales)
        case 550: { extern void VertexAnimDeleteActiveLayer(int); VertexAnimDeleteActiveLayer(0); break; }
        case 551: { extern void VertexAnimDeleteActiveLayer(int); VertexAnimDeleteActiveLayer(2); break; }
        case 552: { extern void VertexAnimDeleteActiveLayer(int); VertexAnimDeleteActiveLayer(1); break; }
        case 220: ApplyTransform(0); break;   // Apply Location
        case 221: ApplyTransform(1); break;   // Apply Rotation
        case 222: ApplyTransform(2); break;   // Apply Scale
        case 223: ApplyTransform(3); break;   // Apply All Transforms
        case 100: if (InteractionMode==PoseMode) PoseXformStart(1); else if (!EditXformStart(translation, ViewAxis)) BeginTranslate(); break; // Move  (G)
        case 101: if (InteractionMode==PoseMode) PoseXformStart(2); else if (!EditXformStart(rotation,    ViewAxis)) BeginRotate(); break;    // Rotate(R)
        case 102: if (InteractionMode==PoseMode) PoseXformStart(3); else if (!EditXformStart(EditScale,   XYZ))      BeginScale();   break;   // Scale (S)
        case 500: InsertKeyframeContext(KfChannelAll); break; // Pose Mode: Insert Keyframe (alias de "todos")
        case 520: PoseClearTransform(0); break; // Pose Mode: Clear Transform > All y Animation > Reset Pose (T+R+S de los huesos seleccionados)
        case 521: PoseClearTransform(1); break; // Clear Translation (Alt+G)
        case 522: PoseClearTransform(2); break; // Clear Rotation (Alt+R)
        case 523: PoseClearTransform(3); break; // Clear Scale (Alt+S)
        case 103: LayoutShrinkFatten(); break; // Shrink/Fatten (Alt+S): cada vert por su normal
        case 300: LayoutExtrudeFaces(); break; // Extrude (segun el modo) (E)
        case 344: InsetStart(lastMouseX, lastMouseY); break; // Inset Faces (I)
        case 345: BevelStart(lastMouseX, lastMouseY, 1); break; // Bevel Edges (Ctrl+B)
        case 346: BevelStart(lastMouseX, lastMouseY, 0); break; // Bevel Vertices (Ctrl+Shift+B)
        case 310: LayoutNewFaceEdit(); break;   // Vertex > New Edge/Face from Vertices (F)
        case 314: LayoutDuplicateEdit(); break; // Duplicate (Shift D)
        case 316: LayoutSeparateEdit();  break; // Separate (P): caras selec -> mesh nuevo
        case 341: LayoutRipEdit();      break;  // Rip (V): separa la malla por la seleccion
        case 342: LayoutConnectVerticesEdit(); break; // Connect Vertex Path (J)
        case 343: LayoutSlideVerticesEdit(); break;   // Slide Vertices (Shift+V, o G-G)
        // Delete: los items del submenu/atajo-X despachan por ESTA accion (el menu top es el de contexto, no gMenuDelete)
        case 361: case 362: case 363: case 364: ActionDelete(aId); break; // Vertices/Edges/Faces/Edge Loops
        case 315: break;                       // UV > Unwrap (pendiente)
        case 320: LayoutShade(true);  break;   // Face > Shade Smooth
        case 321: LayoutShade(false); break;   // Face > Shade Flat
        case 322: LayoutRecalcNormals(); break;  // Recalculate Normals (Face / Mesh>Normals)
        case 323: LayoutTriangulate();    break; // Face > Triangulate Faces (Ctrl T)
        case 324: LayoutFlipNormals();   break;  // Mesh > Normals > Flip
        case 330: LayoutMarkSharp(true);  break; // Edge > Mark Sharp
        case 331: LayoutMarkSharp(false); break; // Edge > Clear Sharp
        case 340: LayoutLoopCutFromActive(); break; // Edge/Face > Loop Cut and Slide (elemento activo)
        case 350: LayoutUVOperation(0); break; // UV > Unwrap Angle Based
        case 365: LayoutUVOperation(1); break; // UV > Unwrap Conformal
        case 366: LayoutUVOperation(2); break; // UV > Unwrap Minimum Stretch
        case 351: LayoutUVOperation(3); break; // UV > Smart UV Project
        case 367: LayoutUVOperation(4); break; // UV > Lightmap Pack
        case 352: LayoutUVOperation(5); break; // UV > Follow Active Quads
        case 368: LayoutUVOperation(6); break; // UV > Reset
        case 353: LayoutProjectUV(0); break;   // UV > Cube Projection
        case 354: LayoutProjectUV(1); break;   // UV > Cylinder Projection
        case 355: LayoutProjectUV(2); break;   // UV > Sphere Projection
        case 356: LayoutProjectUVFromView(false); break; // UV > Project from View
        case 357: LayoutProjectUVFromView(true);  break; // UV > Project from View (Bounds)
        case 358: LayoutMarkSeam(true);  break; // UV > Mark Seam
        case 359: LayoutMarkSeam(false); break; // UV > Clear Seam
        case 430: case 431: case 432: case 433: case 434: case 435: case 436: case 437:
            ProportionalSetType(aId - 430); break;   // Proportional Editing > curva de caida
        case 200: SetOriginGeometryToOrigin(); break; // Set Origin > Geometry to Origin
        case 201: SetOriginOriginToGeometry(); break; // Set Origin > Origin to Geometry
        case 202: SetOriginToCursor();         break; // Set Origin > Origin to 3D Cursor
        case 203: ForgetOriginSelected(); break; // Set Origin > Clear Original File (borra Mesh::origen)
        // Set Parent (Ctrl P) / Clear Parent (Alt P): submenus de Object + standalone. Ids unicos ->
        // despachan por ESTA accion sea como submenu (menu top = Object) o standalone (menu top = el propio).
        case 230: case 231: case 232: case 233: ActionSetParent(aId - 230); break; // Object / Keep T. / Without Inv. / Keep T. Without Inv.
        case 240: case 241: case 242:           ActionClearParent(aId - 240); break; // Clear / Clear+Keep T. / Clear Inverse
        case 380: case 381: case 382: case 383: ActionMerge(aId - 380); break; // Merge: At Center / At Cursor / Collapse / By Distance
        // EDIT MODE de ARMATURE (Fase 3): menu de contexto "Armature" (mismas acciones que E / Shift+D / X)
        case 600: if (BoneEditActive()) BoneEditExtrudeInteractive(BoneEditArm(), true);  break;  // Extrude Bone
        case 601: if (BoneEditActive()) BoneEditDuplicateInteractive(BoneEditArm(), true); break; // Duplicate Bones
        case 602: if (BoneEditActive()) BoneEditDelete(BoneEditArm());              break; // Delete Bones
        case 604: if (BoneEditActive() && !BoneGrabActive()) BoneXformStart(BoneEditArm(), 1); break; // Move (G)
        case 605: if (BoneEditActive() && !BoneGrabActive()) BoneXformStart(BoneEditArm(), 2); break; // Rotate (R)
        case 606: if (BoneEditActive() && !BoneGrabActive()) BoneXformStart(BoneEditArm(), 3); break; // Scale (S)
        // (607 Set Parent y 608 Clear Parent son SUBMENUS de 2 opciones: despachan por su action
        //  propia en Parent.cpp -Keep Offset/Connected y Disconnect Bone/Clear Parent-, no por aca.
        //  El viejo 603 "Disconnect from Parent" quedo absorbido por ese submenu.)
    }
}

// opcion del menu Render: el modo de vista del viewport 3D activo
static void LayoutActionRender(int aId) {
    if (!Viewport3DActive) return;
    switch (aId) {
        case 0: Viewport3DActive->view = RenderType::Rendered;        break;
        case 1: Viewport3DActive->view = RenderType::MaterialPreview; break;
        case 2: Viewport3DActive->view = RenderType::Solid;           break;
        case 3: Viewport3DActive->view = RenderType::Wireframe;       break;
        case 4: Viewport3DActive->view = RenderType::ZBuffer;         break;
        case 5: Viewport3DActive->view = RenderType::NormalView;      break;
        case 6: Viewport3DActive->view = RenderType::Alpha;           break;
        case 8: Viewport3DActive->view = RenderType::Vectors;        break;
    }
}

// opcion del menu Orient: orientacion usada al constrenir a un eje (X/Y/Z)
static void LayoutActionOrient(int aId) {
    if (aId == 0)      transformOrientation = GlobalOrient;
    else if (aId == 1) transformOrientation = LocalOrient;
    else if (aId == 2) transformOrientation = ViewOrient;
    else if (aId == 3) transformOrientation = NormalOrient; // = la normal de la seleccion (extrude)
    // VIEW no tiene eje Y (es la profundidad de la vista): si el constraint lo incluia, se libera
    if (transformOrientation == ViewOrient &&
        (axisSelect == Y || axisSelect == PlaneX || axisSelect == PlaneZ))
        axisSelect = (state == EditScale) ? XYZ : ViewAxis;
    // con un transform EN CURSO la nueva orientacion se re-aplica al instante (como las teclas X/Y/Z)
    if (BoneXformMode()) BoneXformReapply();                    // G/R/S de huesos: su propio re-aplicar
    else if (state != editNavigation) RestoreState(false);
}

// abre el menu de ORIENTACION desde la barra de HERRAMIENTAS (abajo): el menu crece hacia
// ARRIBA del boton (syTop = borde superior de la barra) para no salirse de la pantalla.
void LayoutMenuOrientToolbar(int sx, int syTop){
    if (!MenuOrient) return;
    MenuOrient->action = LayoutActionOrient;
    MenuOrient->Resize();
    int my = syTop - MenuOrient->height;
    if (my < 0) my = 0;
    MenuOrient->Open(sx, my, MenuScreenW, MenuScreenH);
    OpenMenu = MenuOrient;
}

// opcion del menu View > Viewpoint: cambia el punto de vista del viewport activo (los MISMOS atajos del numpad,
// ahora VISIBLES en el menu -> nada oculto). ids 400-406.
static void LayoutActionView(int aId) {
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
        case 420: Viewport3DActive->FocusObject(); break; // Frame Selected (Numpad .): enfoca la seleccion
        case 421: LayoutLockOrbitToggle(); break; // Lock Orbit: orbitar -> panear
        case 422: Viewport3DActive->LocalViewToggle(); break; // Local View (/): aisla la seleccion en el viewport activo
        // el candado de la vista de camara (Viewport3D::LockCameraClick): navegar mueve la camara activa
        case 423: Viewport3DActive->CameraToView = !Viewport3DActive->CameraToView; g_redraw = true; break;
    }
}

// deriva g_editMesh (la malla que se esta editando) del modo + objeto activo. HAY QUE
// llamarla cada vez que cambia InteractionMode o ActiveObj. COMPARTIDA PC+Symbian: antes
// solo la seteaba el render de PC (ViewPort3D::Render), asi que en Symbian g_editMesh
// quedaba NULL -> en Edit Mode no se podia ni seleccionar ni mover sub-elementos.
// BOOLEAN cuyo TARGET fue editado: el target sube su geoVersion en cada cambio de geometria;
// aca se compara con la version con la que se genero. UN entero por modificador, y solo se
// recorre cuando g_meshesEdited avisa que alguna malla cambio (no por frame).
static void RegenerateBooleansWithEditedTargetRec(Object* node){
    if (!node) return;
    for (size_t i=0;i<node->Childrens.size();i++){
        Object* o = node->Childrens[i];
        if (o->getType()==ObjectType::mesh){
            Mesh* m=(Mesh*)o;
            for (size_t k=0;k<m->modifiers.size();k++){
                Modifier* md = m->modifiers[k];
                if (md->type!=ModifierType::Boolean || !md->target || md->target->getType()!=ObjectType::mesh) continue;
                if (((Mesh*)md->target)->geoVersion != md->boolTargetGeoVer){ m->GenerateMeshModified(); break; }
            }
        }
        RegenerateBooleansWithEditedTargetRec(o);
    }
}

// ---- la memoria de los modos (ver edit/ModeMemory.h) ----
namespace {
struct ModeRelease { unsigned modes; W3dModeReleaseFn release; };
std::vector<ModeRelease>& ModeReleases() { static std::vector<ModeRelease> v; return v; }   // (antes que los static que anotan)
}
int W3dModeMemoryRegister(unsigned modes, W3dModeReleaseFn release) {
    if (release && modes) { ModeRelease r = { modes, release }; ModeReleases().push_back(r); }
    return (int)ModeReleases().size();
}
void W3dModeMemoryTick(unsigned activeModes) {
    static unsigned prev = 0;
    const unsigned left = prev & ~activeModes;   // los modos que se dejaron en este cuadro
    prev = activeModes;
    if (!left) return;
    const std::vector<ModeRelease>& v = ModeReleases();
    for (size_t i = 0; i < v.size(); i++)
        if ((v[i].modes & left) && !(v[i].modes & activeModes)) v[i].release();   // (salvo que siga en otro de los suyos)
}

void UpdateActiveEditMesh() {
    // EDICION DE CURVE: si la curva dejo de ser el objeto activo en Edit (Tab afuera,
    // click en otro objeto, cambio de modo), volcar los nodos y cerrar la sesion.
    if (gCurveInEdit && (InteractionMode != EditMode || ActiveObj != (Object*)gCurveInEdit))
        CurveFinishEdit();
    // (noEditable: red de seguridad app-wide; los gates de Tab/menu ya avisaron)
    g_editMesh = (InteractionMode == EditMode && ActiveObj &&
                  ActiveObj->getType() == ObjectType::mesh &&
                  !((Mesh*)ActiveObj)->noEditable) ? ActiveObj : NULL;
    // curva en edicion: la MALLA INTERNA de la curva es la editable (el activo sigue siendo la curva)
    if (gCurveInEdit && InteractionMode == EditMode && ActiveObj == (Object*)gCurveInEdit)
        g_editMesh = (Object*)gCurveInEdit->edit;
    // SPLINES: lo que la maquinaria de malla hizo con el proxy (mover, elegir) pasa a las manijas
    if (CurveBezEditing() && InteractionMode == EditMode && g_editMesh == (Object*)gCurveInEdit->edit)
        CurveBezSync(gCurveInEdit);
    // el modo Face del menu SelMode se grisa editando una malla sin caras (path/riel)
    g_selFaceAvailable = !(g_editMesh && ((Mesh*)g_editMesh)->facesSize == 0);
    // CONSTRAINTS + EDIT MODE: el evaluador del Core necesita saber CUAL objeto se esta editando
    // para poder saltear los constraints que tienen apagado "ver en modo edicion" (W3dConstraint.h).
    // Va por la misma puerta que g_editMesh -que es la unica- para que no puedan quedar desfasados.
    W3dConstraintSetObjEditing(g_editMesh);
    // LAS MALLAS COMO RECURSO (io/ProjectMeshes.h): la malla que sale de Edit/Weight/Vertex
    // Paint se publica en su recurso (la ven todos sus objetos) y lo seleccionado tiene su edicion
    // en memoria. Por la misma puerta que g_editMesh: cada frame y en cada cambio de modo.
    { extern void W3dMeshesTickEditor(); W3dMeshesTickEditor(); }
    // MIRROR con TARGET: si se movio algun objeto (flag que prenden los transforms de objeto), su plano cambio ->
    // regenerar SOLO esos previews. Chequeo barato (1 bool/frame); no corre nada al orbitar/idle.
    if (g_objectsMoved) {
        g_objectsMoved = false;
        // los modificadores que dependen de DONDE ESTA otro objeto (Mirror y Boolean con target): si se movio algo, su
        // resultado cambio -> regenerar SOLO esos (el resto sigue cacheado en genValid). Las mallas de la escena salen
        // de W3dLiveOfType: jugando esto corre en cada cuadro (todo se mueve) y recorrer el arbol eran miles de nodos
        if (SceneCollection) {
            const std::vector<Object*>& meshes = W3dLiveOfType(ObjectType::mesh);
            for (size_t i = 0; i < meshes.size(); i++) {
                Mesh* m = (Mesh*)meshes[i];
                for (size_t k = 0; k < m->modifiers.size(); k++) {
                    const int t = m->modifiers[k]->type;
                    if ((t == ModifierType::Mirror || t == ModifierType::Boolean || t == ModifierType::Array) && m->modifiers[k]->target) {
                        if (W3dAttachedTo(m, SceneCollection)) m->GenerateMeshModified();
                        break;
                    }
                }
            }
        }
        g_redraw = true;
    }
    // BOOLEAN con el target EDITADO (mover un vertice del cilindro tiene que rehacer el corte del
    // cubo): mismo esquema, gateado por g_meshesEdited para que en idle no se recorra nada.
    if (g_meshesEdited) {
        g_meshesEdited = false;
        if (SceneCollection) RegenerateBooleansWithEditedTargetRec(SceneCollection);
    }
    // LA MEMORIA DE LOS MODOS (edit/ModeMemory.h): lo que las herramientas reservaron para el modo que se deja se suelta
    // (la pintura de textura del editor UV cuenta como Texture Paint)
    W3dModeMemoryTick(W3D_MODE_BIT(InteractionMode) | (UVEditorPaintingTexture() ? W3D_MODE_BIT(TexturePaint) : 0u));
    // Esta funcion se llama CADA FRAME (ViewPort3D::Render). El UNICO motivo para regenerar aca es el CAMBIO DE MODO
    // (entrar/salir de Edit): el filtro showEdit puede saltear un modificador en Edit, asi que el preview cambia.
    // NO se regenera por seleccionar/activar un objeto (seleccionar no cambia la geometria -> el preview ya esta
    // cacheado en genValid), NI cada frame (antes se recalculaba la subdivision/screw en cada redibujo -> lentisimo
    // en el N95). Los demas cambios (params del modificador, mover verts, cortes, undo) los regeneran por su cuenta;
    // el mirror con TARGET lo regenera el confirm de mover objetos.
    static int prevMode = -999;
    if (InteractionMode != prevMode) {
        prevMode = InteractionMode;
        if (ActiveObj && ActiveObj->getType() == ObjectType::mesh) {
            Mesh* m = (Mesh*)ActiveObj;
            if (!m->modifiers.empty()) { m->GenerateMeshModified(); g_redraw = true; }
        }
    }
    // EDICION QUE CAMBIA LA TOPOLOGIA (extrude, loop cut, delete, merge, subdivide...): todas llaman
    // GenerateRender() -> genValid=false (la malla generada quedo stale). Regeneramos el modificador
    // UNA sola vez: al hacerlo genValid vuelve a true, asi que NO es un regen por-frame (GenerateRender
    // solo lo llaman las ops de edicion, nunca el render). Sin esto, tras extrude/loop cut el
    // modificador (subdivision/screw) mostraba geometria vieja hasta mover un vertice.
    if (ActiveObj && ActiveObj->getType() == ObjectType::mesh) {
        Mesh* m = (Mesh*)ActiveObj;
        // solo re-generar si hay un modificador que PRODUCE geometria (Mirror/Array/Subsurf/Screw/Boolean). Un stack de
        // SOLO Armature no genera malla (el skinning se aplica aparte en el render -> genValid queda false, que es lo
        // correcto: el render usa la malla base + skinVertex). Sin este filtro, GenerateMeshModified dejaba
        // genValid=false y el retry se disparaba CADA FRAME (el 'modgen' subia sin parar al orbitar un FBX).
        bool hasGeomMod = false;
        for (size_t k = 0; k < m->modifiers.size(); k++)
            if (m->modifiers[k]->type != ModifierType::Armature) { hasGeomMod = true; break; }
        if (hasGeomMod && !m->genValid) { m->GenerateMeshModified(); g_redraw = true; }
        // SKINNING: sincronizar con el modificador Armature (target / "Display in viewport" / "Display in Edit Mode").
        // Barato (recorre modificadores); mantiene el skinning coherente con los flags y el modo actual.
        extern void SyncSkinWithModifier(Mesh*);
        if (!m->modifiers.empty()) SyncSkinWithModifier(m);
    }
}


// lo que es de una LIBRERIA externa no se edita (es de su .w3d, de solo lectura: se edita abriendo la libreria):
// lo que genera un PROXY y una malla que usa una malla de una libreria. true = no se puede (con aviso)
static bool IsFromLibNotice(Object* o) {
    if (!o) return false;
    bool lib = (o->getType() == ObjectType::mesh && ((Mesh*)o)->mesh && !((Mesh*)o)->mesh->lib.empty());
    for (Object* p = W3dInstanceOf(o); p && !lib; p = W3dInstanceOf(p)) if (p->getType() == ObjectType::proxy) lib = true;
    if (lib) Notify(T("It belongs to a library (read-only): open the library to edit it"), true);
    return lib;
}

// opcion del menu Mode: cambia el modo del objeto ACTIVO.
//  - MALLA:     Object/Edit/Paint (Edit y Paint todavia son placeholders).
//  - ARMATURE:  Object / Edit (placeholder) / Pose (posa el esqueleto).
static void LayoutActionMode(int aId) {
    int previousMode = InteractionMode;
    // (lo de una libreria: solo Object, y Pose para posar un esqueleto de un proxy)
    if (aId != ObjectMode && aId != PoseMode && InteractionMode == ObjectMode && IsFromLibNotice(ActiveObj)) return;
    if (ActiveObj && ActiveObj->getType() == ObjectType::mesh) {
        if (aId == EditMode && ((Mesh*)ActiveObj)->noEditable) {   // escenario cerrado a edicion
            Notify(T("Mesh not editable (Convert to editable mesh, Mesh card)"), true);
            return;
        }
        InteractionMode = aId;
    }
    else if (ActiveObj && ActiveObj->getType() == ObjectType::armature)
        InteractionMode = (aId == PoseMode || aId == EditMode) ? aId : ObjectMode; // Object/Edit/Pose
    else if (ActiveObj && ActiveObj->getType() == ObjectType::curve && aId == EditMode) {
        CurveEnterEdit((Curve*)ActiveObj);   // riel/path: editar sus nodos via el proxy
        return;
    }
    else
        InteractionMode = ObjectMode;
    // al SALIR del Edit de huesos: cerrar un grab a medio hacer + preparar el skin autorado
    // (PrepareAuthoredSkin) para que el rig recien editado DEFORME la malla sin pasos extra.
    if (previousMode == EditMode && InteractionMode != EditMode &&
        ActiveObj && ActiveObj->getType() == ObjectType::armature){
        if (BoneGrabActive()) BoneGrabCancel();
        PrepareAuthoredSkin((Armature*)ActiveObj);
    }
    UpdateActiveEditMesh(); // refresca g_editMesh (PC + Symbian)
}

// el MISMO camino que elegir la opcion en el menu Mode, publico para el harness de tests
// (asi 'mode weight' ejercita lo que ejercita el click y no una asignacion propia).
void LayoutModePick(int mode) { LayoutActionMode(mode); }

// REARMA el menu Mode segun el objeto activo (como LayoutRebuildMenuSelect):
//   malla -> Object/Edit/Vertex/Weight/Texture ; armature -> Object/Edit/Pose.
// Asi el mismo boton sirve para los dos tipos sin mostrar modos que no aplican.
static void LayoutRebuildMenuMode() {
    if (!MenuMode) return;
    MenuMode->Clear();
    bool isArm = (ActiveObj && ActiveObj->getType() == ObjectType::armature);
    bool isCurve = (ActiveObj && ActiveObj->getType() == ObjectType::curve);   // (curva: Object / Edit, nada mas)
    MenuMode->Add(T("Object Mode"), ObjectMode, IconType::object);
    MenuMode->Add(T("Edit Mode"),   EditMode,   isArm ? IconType::armature : isCurve ? IconType::curve : IconType::mesh);
    if (isArm) {
        MenuMode->Add(T("Pose Mode"), PoseMode, IconType::armature);
    } else if (!isCurve) {
        MenuMode->Add(T("Vertex Paint"),  VertexPaint,  IconType::mesh);
        MenuMode->Add(T("Weight Paint"),  WeightPaint,  IconType::mesh);
        MenuMode->Add(T("Texture Paint"), TexturePaint, IconType::mesh);
    }
}

// opcion del menu SelMode (edit): sub-elemento Vertex/Edge/Face. Al cambiar de
// modo hay que RECOLOREAR (sino quedan los colores del modo anterior, ej: el
// degradado de vertex en edge) y se resetea el activo (no hay activo del modo nuevo).
static void LayoutActionSelMode(int aId) {
    // malla SIN caras (un path/riel en edicion): el modo Face no existe
    if (aId == SelFace && g_editMesh && ((Mesh*)g_editMesh)->facesSize == 0) return;
    const int prev = EditSelectMode;
    EditSelectMode = aId; // SelVertex/SelEdge/SelFace
    if (g_editMesh) {
        Mesh* m = (Mesh*)g_editMesh;
        m->EnsureEdit();
        // la seleccion PASA al modo nuevo (un quad -> sus 4 aristas -> sus 4 vertices)
        if (m->edit) { m->edit->ConvertSelection(prev, aId); m->edit->activeIdx = -1; m->edit->Recolor(); }
    }
}
void LayoutSetSelMode(int aId) { LayoutActionSelMode(aId); }   // (lo mismo que elegirlo en el menu: los scripts)

// ---- el submenu LAYOUT (la esquina de cada viewport): "Por defecto", los guardados (config/W3dLayouts.h), y separados
//      con una linea "Guardar layout..." y "Borrar layout..." ----
enum { LAY_DEFAULT = 0, LAY_SAVED0 = 1, LAY_SAVE = 900, LAY_DELETE = 901, LAY_DELETE0 = 1000 };
static PopupMenu* gMenuLayouts = NULL;
static PopupMenu* gMenuDeleteLayout = NULL;
static std::vector<std::string> gLayoutsMenu;    // los nombres, en el orden del menu (los ids LAY_SAVED0 + i)
static std::string gLayoutToDelete;
static void LayoutActionLayouts(int id);
static void LayoutBuildMenuLayouts() {
    if (!gMenuLayouts) { gMenuLayouts = new PopupMenu(); gMenuLayouts->action = LayoutActionLayouts; }
    gMenuLayouts->Clear();
    gMenuLayouts->Add(T("Default"), LAY_DEFAULT, (int)IconType::grid);
    gLayoutsMenu = W3dLayoutsNames();
    for (size_t i = 0; i < gLayoutsMenu.size(); i++) gMenuLayouts->Add(gLayoutsMenu[i], LAY_SAVED0 + (int)i, (int)IconType::grid);
    { MenuItem* it = gMenuLayouts->Add(T("Save layout..."), LAY_SAVE, (int)IconType::save); if (it) it->lineUp = true; }
    gMenuLayouts->Add(T("Delete layout..."), LAY_DELETE, (int)IconType::trash);
}
static void LayoutSaveWithName(const std::string& name) {
    const bool existed = !W3dLayoutText(name).empty();
    if (W3dLayoutSave(name, W3dLayoutTextOf(NULL)))
        Notify(std::string(T(existed ? "Layout replaced" : "Layout saved")) + ": " + name, false);
    else Notify(T("Could not save the layout"), true);
}
static void LayoutDeleteConfirmed() {
    if (W3dLayoutDelete(gLayoutToDelete)) Notify(std::string(T("Layout deleted")) + ": " + gLayoutToDelete, false);
    gLayoutToDelete.clear();
}
static void LayoutActionDeletePicked(int id) {
    const int i = id - LAY_DELETE0;
    if (i < 0 || i >= (int)gLayoutsMenu.size()) return;
    gLayoutToDelete = gLayoutsMenu[(size_t)i];
    if (!confirmPopup) confirmPopup = new ConfirmPopup();
    confirmPopup->Open(std::string(T("Delete the layout")) + " \"" + gLayoutToDelete + "\"?", LayoutDeleteConfirmed);
}
static void LayoutActionLayouts(int id) {
    if (id == LAY_DEFAULT) { LayoutRequest(std::string()); return; }
    if (id == LAY_SAVE) {
        // la ventanita del nombre (con uno que se pueda pisar si ya existe: "Layout 1", "Layout 2"...)
        std::string prop;
        for (int k = 1; k < 1000; k++) {
            char b[48]; snprintf(b, sizeof(b), "%s %d", T("Layout"), k);
            if (W3dLayoutText(b).empty()) { prop = b; break; }
        }
        OpenNamePopup(T("Save layout"), T("Save"), prop, LayoutSaveWithName);
        return;
    }
    if (id == LAY_DELETE) {
        gLayoutsMenu = W3dLayoutsNames();
        if (gLayoutsMenu.empty()) { Notify(T("There are no saved layouts to delete"), false); return; }
        // la lista de los guardados ("Por defecto" no esta: no se borra), donde estaba el menu
        if (!gMenuDeleteLayout) { gMenuDeleteLayout = new PopupMenu(); gMenuDeleteLayout->action = LayoutActionDeletePicked; }
        gMenuDeleteLayout->Clear();
        gMenuDeleteLayout->title = T("Delete layout");
        for (size_t i = 0; i < gLayoutsMenu.size(); i++) gMenuDeleteLayout->Add(gLayoutsMenu[i], LAY_DELETE0 + (int)i, (int)IconType::trash);
        int mx = MenuScreenW / 3, my = MenuScreenH / 4;
        if (gMenuLayouts) { mx = gMenuLayouts->x; my = gMenuLayouts->y; }
        if (OpenMenu && OpenMenu != gMenuDeleteLayout) OpenMenu->Close();
        gMenuDeleteLayout->Open(mx, my, MenuScreenW, MenuScreenH);
        OpenMenu = gMenuDeleteLayout;
        g_redraw = true;
        return;
    }
    const int i = id - LAY_SAVED0;
    if (i >= 0 && i < (int)gLayoutsMenu.size()) LayoutRequest(gLayoutsMenu[(size_t)i]);
}

// (harness) una opcion del submenu Layout por la misma puerta que el menu: 900 = Guardar layout..., 901 = Borrar layout...
void LayoutMenuLayoutsAction(int id) { LayoutBuildMenuLayouts(); LayoutActionLayouts(id); }

// abre el menu de TIPO/split del viewport (boton [0] de la barra), por codigo
// (sin hit-test). Lo usan el click en la flechita Y la navegacion por teclado
// (soft-izq en outliner/propiedades, o izquierda desde Select en el 3D).
void LayoutOpenMenuType(ViewportBase* aVp) {
    if (!aVp || aVp->BarButtons.empty()) return;
    if (!gMenuType) {
        gMenuType = new PopupMenu();
        gMenuType->action = LayoutActionType;
    }
    // se reconstruye en cada apertura: "Expand" no existe para el root.
    gMenuType->Clear();
    if (LayoutIsMaximized()) {
        // en FULLSCREEN no se cambia tipo/split/expand: solo restaurar el layout
        gMenuType->Add(T("Minimize"), 23, (int)IconType::minimize);
    } else {
        // (cada tipo con el icono que muestra su barra: W3dViewportIcon)
        extern int W3dViewportIcon(int kind);
        gMenuType->Add(T("3D Viewport"), 0, W3dViewportIcon(1));
        gMenuType->Add(T("Outliner"), 1, W3dViewportIcon(2));
        gMenuType->Add(T("Library"), 8, W3dViewportIcon(9));   // los recursos del proyecto (su propio viewport)
        gMenuType->Add(T("Properties"), 2, W3dViewportIcon(3));
        gMenuType->Add(T("UV Editor"), 3, W3dViewportIcon(4));
        gMenuType->Add(T("Timeline"), 4, W3dViewportIcon(5));
        gMenuType->Add(T("2D Editor"), 5, W3dViewportIcon(6));
        gMenuType->Add(T("Console"), 6, W3dViewportIcon(7));
        gMenuType->Add("IDE", 7, W3dViewportIcon(8));
        // (una LINEA separa los tipos de viewport de lo que se hace con el: expandir, dividir, maximizar)
        // (Expandir y Cerrar: no para el root, que es el ultimo viewport)
        { MenuItem* it = (aVp != rootViewport) ? gMenuType->Add(T("Expand"), 20, (int)IconType::expand) : NULL;
          if (aVp != rootViewport) gMenuType->Add(T("Close Viewport"), 27, (int)IconType::close);
          MenuItem* div = gMenuType->Add(T("Split in Columns"), 21, (int)IconType::columns);
          (it ? it : div)->lineUp = true; }
        // OJO nombres: dividir "en columnas" = 2 paneles LADO A LADO (ViewportRow);
        // "en filas" = APILADOS (ViewportColumn). Antes decia Fila/Columna al reves.
        gMenuType->Add(T("Split in Rows"), 22, (int)IconType::rows);
        // (con un hermano en su fila o columna: los dos cambian de lugar)
        if (aVp != rootViewport) gMenuType->Add(T("Swap"), 24, (int)IconType::swap);
        if (aVp != rootViewport) gMenuType->Add(T("Maximize"), 23, (int)IconType::maximize); // fullscreen del viewport activo
        // (dentro de una fila o columna: la fila pasa a ser columna y al reves)
        if (aVp != rootViewport) gMenuType->Add(T("Rotate"), 25, (int)IconType::redo);
        // LAYOUT: elegir uno (el de fabrica o uno guardado), guardar el actual, borrar uno guardado
        LayoutBuildMenuLayouts();
        { MenuItem* it = gMenuType->Add(T("Layout"), 26, (int)IconType::grid, gMenuLayouts); if (it) it->lineUp = true; }
    }
    gMenuTypeOf = aVp;
    if (OpenMenu && OpenMenu != gMenuType) OpenMenu->Close();
    aVp->barFocusIndex = 0; // resaltar [0] + auto-scroll de la barra
    aVp->UpdateBar();       // sx/sy de [0] YA con el scroll
    Button* b = aVp->BarButtons[0];
    gMenuType->Open(b->sx, b->sy + b->height - GlobalScale,
                     MenuScreenW, MenuScreenH);
    OpenMenu = gMenuType;
}

// click en la flechita de la barra: abre el menu de tipo de viewport
static bool LayoutClickButtonType(ViewportBase* aVp, int aX, int aY) {
    if (!aVp || aVp->BarButtons.empty()) return false;
    if (!aVp->BarButtons[0]->Contains(aX, aY)) return false;
    LayoutOpenMenuType(aVp);
    return true;
}

// ===== menu del boton "View" del UV Editor (checkboxes) =====
static PopupMenu* gMenuUV = NULL;
static UVEditor*  gMenuUVOf = NULL;   // el editor cuyo boton View abrio el menu (para el encuadre)
// ENCUADRAR: los ids 10/11 son los unicos items con ACCION del menu View del UV (el resto son
// checkboxes que se togglean solos). Mismo label y mismo atajo que el 3D / el Editor 2D / el
// Timeline: "Frame Selected" (Num .) y "Frame All".
static void LayoutActionUVView(int id) {
    if (!gMenuUVOf) return;
    if (id == 10)      gMenuUVOf->FrameUV(false);
    else if (id == 11) gMenuUVOf->FrameUV(true);
}
static void LayoutOpenMenuUV(UVEditor* uv, int x, int y) {
    if (!uv) return;
    gMenuUVOf = uv;
    // REGLA DE DISENO de los titulos: un menu que se abre desde algo SIN TEXTO (un icono, o un atajo de
    // teclado) lleva titulo -- es lo unico que te dice que estas mirando. Si lo abre un boton/item que YA
    // decia el texto, NO lleva: repetirlo es ruido. El boton View ahora es un icono -> titulo.
    if (!gMenuUV){ gMenuUV = new PopupMenu(); gMenuUV->title = T("View"); }
    gMenuUV->Clear();
    // los togglea el propio item (AddCheck sobre el bool* del UV editor)
    // Sync Selection (semantica de Blender, documentada entera en UVEditor.h): OFF (DEFAULT) = el
    // 3D FILTRA que caras se ven y la seleccion del UV es PROPIA y POR RENDER-VERT (una copia de
    // una costura no arrastra a sus hermanas); ON = ves todo el mapa y la seleccion ESPEJA la del
    // mesh (ahi un click SI agarra las 3 copias de un vertice 3D). El cambio de estado lo detecta
    // solo el editor (UVEditor::SyncFilter3D re-inicializa al vuelo).
    gMenuUV->AddCheck(T("Sync Selection"), 0, &uv->syncSelection);
    gMenuUV->AddCheck(T("Repeat Texture"), 1, &uv->repeatTexture);
    gMenuUV->AddCheck(T("Show Chrome UV"), 2, &uv->showChromeUV); // overlay LIVE del reflejo equirect (demo)
    // ENCUADRAR (el UV era el UNICO viewport sin esto): la seleccion, o todo el mapa visible.
    gMenuUV->Add(T("Frame Selected"), 10)->shortcut = "@uv.focus";
    gMenuUV->Add(T("Frame All"), 11);
    gMenuUV->action = LayoutActionUVView;
    if (OpenMenu && OpenMenu != gMenuUV) OpenMenu->Close();
    gMenuUV->Open(x, y, MenuScreenW, MenuScreenH);
    OpenMenu = gMenuUV;
}
// ===== menu del boton "Cursor" del UV editor (ops del cursor 2D <-> seleccion).
// OJO CON EL NOMBRE: en el viewport 3D "Snap" es el IMAN (BR_Snap / g_snap) y ESTAS ops viven en
// Mesh > Snap (Shift+S). Tener un boton "Snap" en el UV que hacia otra cosa era el gap de
// consistencia que reporto la auditoria -> el boton se llama "Cursor" y el menu tambien. =====
static PopupMenu* gMenuUVSnap = NULL;
static UVEditor*  gUVSnapTarget = NULL; // sobre que editor opera el snap (se setea al abrir)
static void LayoutActionUVSnap(int id) {
    if (!gUVSnapTarget) return;
    if (id == 0) gUVSnapTarget->SnapCursorToSel();
    else if (id == 1) gUVSnapTarget->SnapSelToCursor();
    else if (id == 2) gUVSnapTarget->CursorToCenter();
}
static void LayoutOpenMenuUVSnap(UVEditor* uv, int x, int y) {
    if (!uv) return;
    gUVSnapTarget = uv;
    if (!gMenuUVSnap) gMenuUVSnap = new PopupMenu();
    gMenuUVSnap->Clear();
    gMenuUVSnap->title = T("Cursor");
    // (los mismos labels que el submenu Mesh > Snap del 3D)
    gMenuUVSnap->Add(T("Cursor to Selection"), 0);
    gMenuUVSnap->Add(T("Selection to Cursor"), 1);
    gMenuUVSnap->Add(T("Cursor to Center"), 2);
    gMenuUVSnap->action = LayoutActionUVSnap;
    if (OpenMenu && OpenMenu != gMenuUVSnap) OpenMenu->Close();
    gMenuUVSnap->Open(x, y, MenuScreenW, MenuScreenH);
    OpenMenu = gMenuUVSnap;
}

// ===== menu SELECT del UV editor: CALCADO del menu Select del viewport 3D (mismos labels, mismos
// atajos y hasta los mismos ids: All=0 / None=1 / Invert=2 / Select Linked=15) pero operando sobre
// lo que edita el UV. Es CONTEXTUAL segun el modo del editor:
//   - EDICION DE UVs  -> la seleccion propia del UV (uvSelVert), respetando SIEMPRE el filtro del
//                        3D: lo que no se ve no se selecciona (ni con All ni con Invert ni con la isla).
//   - Edit Bones / Pose -> los HUESOS 2D del mesh; "Select Linked" es la CADENA conectada del arbol.
//   - Weight Paint     -> el boton NO se muestra (SyncBar lo oculta): ahi se pinta, no se selecciona.
// =====
static PopupMenu* gMenuUVSelect = NULL;
static UVEditor*  gUVSelectOf = NULL;   // el editor cuyo boton abrio el menu
static void LayoutActionUVSelect(int id) {
    UVEditor* uv = gUVSelectOf;
    if (!uv) return;
    Mesh* m = (ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
    if (!m || (Object*)m != g_editMesh) return;
    const bool bones = (uv->uvMode == UVModeBones || uv->uvMode == UVModePose);
    // Ctrl+Z: SOLO se captura cuando la accion escribe la seleccion del 3D (Sync Selection ON),
    // que es la que el undo sabe guardar -- exactamente lo que hace el menu Select del 3D
    // (UndoCaptureSelectionEdit). Fuera de sync la seleccion del UV (uvSelVert) y la de los huesos
    // 2D son ESTADO TRANSITORIO del viewport: no hay comando de undo para ellas y tampoco lo tenian
    // el A / Alt+A ni el pick, asi que no se inventa uno nuevo aca (seria el unico que va al stack).
    if (!bones && uv->syncSelection) UndoCaptureSelectionEdit(m);
    if (bones) {
        switch (id) {
            case 0:  uv->Bone2DSelectAll(m, true);  break;        // All    (A)
            case 1:  uv->Bone2DSelectAll(m, false); break;        // None   (Alt A)
            case 2:  uv->Bone2DInvertSelection(m);       break;   // Invert (Ctrl I)
            // Select Linked (L) = la CADENA del hueso seleccionado/activo. Sin nada de donde
            // arrancar no hay cadena: se avisa en vez de no hacer nada en silencio.
            case 15: if (!uv->Bone2DSelectLinked(m, false))
                         Notify(T("Select Linked: select a bone first"), false);
                     break;
        }
    } else {
        switch (id) {
            case 0:  uv->SelectAllUV(m, true);  break;            // All    (A)
            case 1:  uv->SelectAllUV(m, false); break;            // None   (Alt A)
            case 2:  uv->InvertSelectionUV(m);      break;        // Invert (Ctrl I)
            // Select Linked desde el MENU: la semilla es lo que YA esta seleccionado (-1). La
            // tecla L, en cambio, arranca de lo que hay bajo el cursor (como la L del 3D).
            case 15: if (!uv->UVSelectLinked(m, -1, false))
                         Notify(T("Select Linked: select a UV first (or press L over the island)"), false);
                     break;
        }
    }
    g_redraw = true;
}
static void LayoutOpenMenuUVSelect(UVEditor* uv, int x, int y) {
    if (!uv) return;
    gUVSelectOf = uv;
    if (!gMenuUVSelect) {
        // (regla de los titulos: el boton es un icono sin texto -> el menu lleva titulo)
        gMenuUVSelect = new PopupMenu();
        gMenuUVSelect->title  = T("Select");
        gMenuUVSelect->action = LayoutActionUVSelect;
        gMenuUVSelect->Add(T("All"), 0)->shortcut = "@uv.selectAll";
        gMenuUVSelect->Add(T("None"), 1)->shortcut = "@uv.deselect";
        gMenuUVSelect->Add(T("Invert"), 2)->shortcut = "@uv.invert";
        gMenuUVSelect->Add(T("Select Linked"), 15)->shortcut = "@uv.linked";
    }
    if (OpenMenu && OpenMenu != gMenuUVSelect) OpenMenu->Close();
    gMenuUVSelect->Open(x, y, MenuScreenW, MenuScreenH);
    OpenMenu = gMenuUVSelect;
}

// ===== selector de MODO de seleccion del UV (Vertex/Edge/Face), PROPIO del editor =====
static PopupMenu* gMenuUVSelMode = NULL;
static UVEditor*  gUVModeTarget = NULL;
static void LayoutActionUVSelMode(int id) {
    if (!gUVModeTarget) return;
    if (gUVModeTarget->syncSelection) {   // sincronizado: cambia el modo del 3D (espeja)
        const int prev = EditSelectMode;
        EditSelectMode = id;
        if (g_editMesh) { Mesh* m = (Mesh*)g_editMesh; m->EnsureEdit();
            if (m->edit) { m->edit->ConvertSelection(prev, id); m->edit->activeIdx = -1; m->edit->Recolor(); } }
    } else {
        gUVModeTarget->uvSelMode = id;    // independiente del 3D
    }
}
static void LayoutOpenMenuUVSelMode(UVEditor* uv, int x, int y) {
    if (!uv) return;
    gUVModeTarget = uv;
    if (!gMenuUVSelMode) gMenuUVSelMode = new PopupMenu();
    gMenuUVSelMode->Clear();
    gMenuUVSelMode->title = T("Select Mode");
    int cur = uv->UVMode();
    gMenuUVSelMode->Add(T("Vertex"), SelVertex, (int)IconType::selVertex)->green = (cur == SelVertex);
    gMenuUVSelMode->Add(T("Edge"),   SelEdge,   (int)IconType::selEdge)->green   = (cur == SelEdge);
    gMenuUVSelMode->Add(T("Face"),   SelFace,   (int)IconType::selFace)->green   = (cur == SelFace);
    gMenuUVSelMode->action = LayoutActionUVSelMode;
    if (OpenMenu && OpenMenu != gMenuUVSelMode) OpenMenu->Close();
    gMenuUVSelMode->Open(x, y, MenuScreenW, MenuScreenH);
    OpenMenu = gMenuUVSelMode;
}

// click en la barra del UV editor: [1]=View, [2]=SelMode (propio), [3]=Pivot (= menu del 3D), [4]=Snap
// dropdown "Texture" del UV editor (boton [5] de la barra): elegir que textura ver -- una parte del
// modelo (auto-cae al cambiar de seleccion) o CUALQUIER textura del proyecto (persiste; sirve para
// auditar cuantas texturas hay cargadas y probar como calzan las UV en otro aspecto).
static UVEditor*  gUVTexTarget = NULL;
static Mesh*      gUVTexMesh   = NULL;
static PopupMenu* gMenuUVTex   = NULL;
static std::vector<std::string> gUVTexPaths;   // rutas de las opciones de proyecto (id 9001+k)
static void LayoutActionUVTex(int id) {
    if (id == 8999) { OpenNewTexturePopup(); g_redraw = true; return; }   // "New Texture...": el formulario
    if (id >= 9001) {                                   // textura del PROYECTO (por ruta)
        size_t k = (size_t)(id - 9001);
        if (k < gUVTexPaths.size()) UVSetTexProject(gUVTexPaths[k]);
        g_redraw = true;
        return;
    }
    UVSetTexProject(std::string());                    // parte del modelo: apaga el de proyecto
    if (!gUVTexMesh) return;
    if (id >= 9000) UVSetTexOverride(gUVTexMesh, -1);   // "Auto": vuelve a seguir la parte activa
    else            UVSetTexOverride(gUVTexMesh, id);   // ver a mano la parte (material) 'id'
    g_redraw = true;
}
// ---- menu GUARDAR del UV editor (icono diskette): la textura mostrada, que vive en memoria hasta guardarse ----
static PopupMenu* gMenuUVSave = NULL;
// el menu de guardar se abrio desde la barra del 3D (pintura de textura): trabaja con la textura PINTADA (la de la parte
// activa), no con la que muestre el editor UV
static bool gSaveFrom3D = false;
static Texture* UVSaveTexture() {
    if (gSaveFrom3D) { extern Texture* WP3DPaintedTexture(); return WP3DPaintedTexture(); }
    Mesh* m = (ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
    return UVShownTexture(m);
}
static bool SaveProjectToDisk();                               // (definidas con el menu Archivo del IDE)
static std::string FolderLibraryOf(int view, const std::string& id);
static void UVSaveExternalPicked(const std::string& picked) {
    Texture* t = UVSaveTexture(); TextureEditable* te = t ? TexEditGet(t) : NULL;
    if (!te) return;
    std::string name = t->path;
    const size_t sl = name.find_last_of("/\\"); if (sl != std::string::npos) name = name.substr(sl + 1);
    const size_t pt = name.rfind('.'); if (pt != std::string::npos) name = name.substr(0, pt);
    const std::string path = W3dOutputPath(picked, name, ".png");
    std::string msg;
    if (TexEditMakeExternal(te, path, msg)) {
        SaveProjectToDisk();   // el proyecto recuerda que ahora la textura esta afuera
        Notify(std::string(T("Texture saved")) + ": " + path, false);
    }
    else Notify(T(msg.c_str()), true);
    g_redraw = true;
}
// "Guardar como..." de la textura mostrada: adentro (textures/<nombre>.png, en una carpeta de la biblioteca) o afuera
static bool UVSaveAsAction(const SaveAsDestination& d, bool force, bool* exists, std::string& msg) {
    Texture* t = UVSaveTexture(); TextureEditable* te = t ? TexEditGet(t) : NULL;
    if (!te) { msg = "Save Texture: no texture"; return false; }
    if (d.internal) {
        if (!W3dContainerHasMounted()) { msg = "Save the project first"; return false; }
        if (!TexEditSaveAsInternal(te, d.name, force, exists, msg)) return false;
        W3dResourcesViewInvalidate();
        W3dViewRecMove(W3D_VIEW_TEXTURES, te->tex->path, d.folderProject);
    } else {
        const std::string path = W3dPathInFolder(d.folderDisk, d.name, "");
        if (path != te->tex->path && w3dFileSystem::FileExists(path) && !force) { if (exists) *exists = true; return false; }
        if (!TexEditMakeExternal(te, path, msg)) return false;
    }
    SaveProjectToDisk();
    Notify(std::string(T("Texture saved")) + ": " + TexEditLabel(te->tex), false);
    g_redraw = true;
    return true;
}
static void LayoutActionUVSave(int id) {
    if (id == 4) { OpenExportUVPopup(); return; }   // el mapa UV a un PNG con transparencia (no hace falta textura)
    Texture* t = UVSaveTexture(); TextureEditable* te = t ? TexEditGet(t) : NULL;
    if (!te) { Notify(T("Save Texture: no texture"), true); return; }
    std::string msg;
    if (id == 1) {   // Guardar: la de adentro queda en el .w3d del disco (se guarda el proyecto)
        if (TexEditSave(te, msg)) {
            const bool toW3d = !TexEditIsExternal(t) && SaveProjectToDisk();
            Notify(std::string(T("Texture saved")) + ": " + TexEditLabel(t) + (toW3d ? std::string(" (") + T("saved inside the .w3d") + ")" : std::string()), false);
        } else Notify(T(msg.c_str()), true);
    }
    else if (id == 2) { OpenFileBrowser(T("Save outside the .w3d..."), T("Use this folder"), ".png", UVSaveExternalPicked, true); return; }
    else if (id == 3) {
        if (!W3dContainerHasMounted()) { Notify(T("Save the project first"), true); return; }
        if (TexEditMakeInternal(te, msg)) { SaveProjectToDisk(); Notify(std::string(T("Texture saved")) + ": " + TexEditLabel(t) + " (" + T("saved inside the .w3d") + ")", false); }
        else Notify(T(msg.c_str()), true);
    }
    else if (id == 5) {
        std::string name = t->path;
        const size_t sl = name.find_last_of("/\\"); if (sl != std::string::npos) name = name.substr(sl + 1);
        const size_t pt = name.rfind('.'); if (pt != std::string::npos) name = name.substr(0, pt);
        OpenSaveAsPopup(T("Save As..."), name + ".png", ".png", !TexEditIsExternal(t),
                              FolderLibraryOf(W3D_VIEW_TEXTURES, t->path), UVSaveAsAction);
        return;
    }
    g_redraw = true;
}
// (harness) acciones del menu guardar del editor UV sin abrirlo; "fuera del w3d" con la carpeta ya elegida
void LayoutUVSaveAction(int id) { gSaveFrom3D = false; LayoutActionUVSave(id); }
void LayoutUVSaveAction3D(int id) { gSaveFrom3D = true; LayoutActionUVSave(id); }
void LayoutUVOutsideOfW3d(const std::string& folder) { UVSaveExternalPicked(folder); }
static void LayoutOpenMenuUVSave(UVEditor* uv, int x, int y) {
    gSaveFrom3D = (uv == NULL);   // NULL = la barra del 3D en pintura de textura
    Texture* t = UVSaveTexture();
    if (!gMenuUVSave) { gMenuUVSave = new PopupMenu(); gMenuUVSave->action = LayoutActionUVSave; }
    gMenuUVSave->Clear();
    gMenuUVSave->title = t ? TexEditLabel(t) : std::string(T("UV"));
    // la TEXTURA mostrada (mismo menu que el Archivo del IDE): Guardar solo las que se editaron (en memoria);
    // Guardar como... y dentro / fuera del .w3d, cualquiera
    if (t) {
        if (TexEditFind(t)) gMenuUVSave->Add(T("Save"), 1, (int)IconType::save);
        gMenuUVSave->Add(T("Save As..."), 5);
        if (TexEditIsExternal(t)) gMenuUVSave->Add(T("Save inside the .w3d"), 3);       // vuelve al .w3d
        else                     gMenuUVSave->Add(T("Save outside the .w3d..."), 2);    // un PNG afuera (se referencia por ruta)
    }
    if (!gSaveFrom3D) gMenuUVSave->Add(T("Export UV Layout..."), 4);   // siempre (en el editor UV): el mapa de las UV
    if (OpenMenu && OpenMenu != gMenuUVSave) OpenMenu->Close();
    gMenuUVSave->Open(x, y, MenuScreenW, MenuScreenH);
    OpenMenu = gMenuUVSave;
}
static void LayoutOpenMenuUVTex(UVEditor* uv, int x, int y) {
    if (!uv) return;
    Mesh* m = (ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
    gUVTexTarget = uv; gUVTexMesh = m;   // sin malla el menu sale igual (solo el proyecto)
    if (!gMenuUVTex) gMenuUVTex = new PopupMenu();
    gMenuUVTex->Clear();
    gMenuUVTex->Add(T("New Texture..."), 8999);   // generar una textura de prueba (liso / grilla UV / grilla de colores)
    const bool noProject = UVTexProjectPath().empty();
    gMenuUVTex->Add(T("Auto (active part)"), 9000)->green = noProject && UVTexOverridePart() < 0;
    // las TEXTURAS DISTINTAS del modelo (dedup por puntero). El id de cada opcion = la parte que la
    // usa, asi el UV editor muestra esa textura + las UV de esa parte.
    std::vector<Texture*> views;
    for (size_t i = 0; m && i < m->materialsGroup.size(); i++) {
        Material* mm = m->materialsGroup[i].material;
        Texture* t = mm ? mm->texture : NULL;
        if (!t) continue;
        bool dup = false; for (size_t k = 0; k < views.size(); k++) if (views[k] == t) { dup = true; break; }
        if (dup) continue;
        views.push_back(t);
        std::string lbl; char buf[64]; snprintf(buf, sizeof buf, T("Texture %d"), (int)views.size());
        if (!t->path.empty()){ size_t sl = t->path.find_last_of("/\\"); lbl = (sl==std::string::npos) ? t->path : t->path.substr(sl+1); }
        else lbl = buf;
        if (TexEditUnsaved(t)) lbl += "*";   // en memoria / con cambios sin guardar
        gMenuUVTex->Add(lbl, (int)i)->green = (noProject && UVTexOverridePart() == (int)i);
    }
    // ...y TODAS las del PROYECTO: las texturas de los MATERIALES de la escena
    // MAS la UI del JUEGO (el cache 2D: HUD/Image2D). La UI del propio editor
    // (skin/iconos/fuente) NO es del proyecto y queda afuera (pedido del dueno).
    // Dedup por NOMBRE DE ARCHIVO: la misma textura aparece con ruta absoluta
    // (ya subida) o relativa (encolada) segun el momento, y con esto el contador
    // del titulo es SIEMPRE el mismo, haya lo que haya seleccionado.
    gUVTexPaths.clear();
    std::vector<std::string> candidate;
    for (size_t i = 0; i < Materials.size(); i++) {
        Material* mm = Materials[i];
        if (!mm) continue;
        std::string path = (mm->texture && !mm->texture->path.empty())
                               ? mm->texture->path
                               : PendingTextureOf(mm);   // encolada, aun sin subir
        if (!path.empty()) candidate.push_back(path);
    }
    Texture2DList(candidate);                            // la UI del juego
    // ...y las texturas CARGADAS del proyecto que ningun material tiene puesta (las generadas con "New Texture"
    // que quedaron reemplazadas, las que se probaron y se sacaron): siguen en Textures[] despues de las de la UI
    for (size_t i = (size_t)TexturesBase(); i < Textures.size(); i++)
        if (Textures[i] && Textures[i]->iID && !Textures[i]->path.empty()) candidate.push_back(Textures[i]->path);
    std::vector<std::string> seenNames;
    for (size_t i = 0; i < candidate.size(); i++) {
        const std::string& path = candidate[i];
        size_t sl = path.find_last_of("/\\");
        std::string lbl = (sl == std::string::npos) ? path : path.substr(sl + 1);
        { Texture* tt = TextureFind(path); if (tt && TexEditUnsaved(tt)) lbl += "*"; }   // sin guardar
        bool dup = false;
        for (size_t k = 0; k < seenNames.size(); k++) if (seenNames[k] == lbl) { dup = true; break; }
        if (dup) continue;
        seenNames.push_back(lbl);
        MenuItem* it = gMenuUVTex->Add(lbl, (int)(9001 + gUVTexPaths.size()));
        it->green = (!noProject && path == UVTexProjectPath());
        gUVTexPaths.push_back(path);
    }
    { char tit[48]; sprintf(tit, "%s (%d)", T("Texture"), (int)gUVTexPaths.size());
      gMenuUVTex->title = tit; }
    gMenuUVTex->action = LayoutActionUVTex;
    if (OpenMenu && OpenMenu != gMenuUVTex) OpenMenu->Close();
    gMenuUVTex->Open(x, y, MenuScreenW, MenuScreenH);
    OpenMenu = gMenuUVTex;
}

// menu "Mipmap" del UV editor: inspector de la piramide de la textura MOSTRADA.
// "Auto" = el filtro elige el nivel por zoom (comportamiento normal del render);
// elegir un tamano lo clava para inspeccionarlo (GL_TEXTURE_BASE_LEVEL, solo PC).
static PopupMenu* gMenuUVMip = NULL;
// ---- barra del EDITOR UV: el MAPA UV que se edita (los de la malla, el activo en verde) + uno nuevo ----
static PopupMenu* gMenuUVMaps = NULL;
static void LayoutActionUVMap(int id) {
    Mesh* m = (ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
    if (!m) return;
    if (id == 9000) {   // New UV Map: una copia de la activa, que pasa a ser la activa (como el "+" de Properties)
        if (W3dReadOnlyMeshNotice(m, true)) return;
        DuplicateActiveUVMap(m);
    } else if (id >= 0 && id < (int)m->uvMaps.size()) {
        m->uvActiveMap = id;
    } else return;
    m->ApplyLayersToRender();   // el render (y el editor UV) pasan a mostrar la activa
    { extern void PropsLayoutDirty(); PropsLayoutDirty(); }   // (la tarjeta UV Maps de Properties)
    g_redraw = true;
}
static void LayoutOpenMenuUVMaps(UVEditor* uv, int x, int y) {
    if (!uv) return;
    Mesh* m = (ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
    if (!m) return;
    m->PopulateLayers();   // (las capas por corner existen: la primera sale del UV del render)
    if (!gMenuUVMaps) gMenuUVMaps = new PopupMenu();
    gMenuUVMaps->Clear();
    gMenuUVMaps->title = T("UV Maps");
    for (size_t i = 0; i < m->uvMaps.size(); i++)
        gMenuUVMaps->Add(m->uvMaps[i]->name, (int)i)->green = ((int)i == m->uvActiveMap);
    gMenuUVMaps->Add(T("New UV Map"), 9000, IconType::plus);
    gMenuUVMaps->action = LayoutActionUVMap;
    if (OpenMenu && OpenMenu != gMenuUVMaps) OpenMenu->Close();
    gMenuUVMaps->Open(x, y, MenuScreenW, MenuScreenH);
    OpenMenu = gMenuUVMaps;
}
static void LayoutActionUVMip(int id) {
    UVSetMipLevel(id >= 9000 ? -1 : id);
    g_redraw = true;
}
static void LayoutOpenMenuUVMip(UVEditor* uv, int x, int y) {
    if (!uv) return;
    Mesh* m = (ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
    unsigned int tid = UVShownTextureId(m);
    if (!tid) return;
    if (!gMenuUVMip) gMenuUVMip = new PopupMenu();
    gMenuUVMip->Clear();
    gMenuUVMip->title = T("Mipmap");
    gMenuUVMip->Add(T("Auto"), 9000)->green = (UVMipLevel() < 0);
    if (w3dEngine::TexHasMips(tid) && w3dEngine::MipmapsGlobal()) {
        int tw = 0, th = 0;
        w3dEngine::TextureSize(tid, tw, th);
        int level = 0;
        while (tw >= 1 && th >= 1) {
            char b[24]; sprintf(b, "%dx%d", tw, th);
            gMenuUVMip->Add(b, level)->green = (UVMipLevel() == level);
            if (tw == 1 && th == 1) break;
            if (tw > 1) tw /= 2;
            if (th > 1) th /= 2;
            level++;
        }
    } else {
        // sin piramide: mipmaps apagados (Ajustes/proyecto) o textura NPOT
        gMenuUVMip->Add(T("No mipmaps (off or NPOT texture)"), 9001);
    }
    gMenuUVMip->action = LayoutActionUVMip;
    if (OpenMenu && OpenMenu != gMenuUVMip) OpenMenu->Close();
    gMenuUVMip->Open(x, y, MenuScreenW, MenuScreenH);
    OpenMenu = gMenuUVMip;
}

// ---- barra del EDITOR 2D: boton [1] "Add" -> menu de elementos 2D --------------------------
static PopupMenu* gMenu2DAdd = NULL;

// el UI donde va a caer el elemento nuevo: el del objeto activo (si estas parado en un texto o
// en un UI) o el primero de la escena. Si no hay ninguno se CREA: los elementos 2D viven si o
// si dentro de un UI.
static UI* UIRootToAdd() {
    for (Object* o = ActiveObj; o; o = o->Parent)
        if (o->getType() == ObjectType::ui) return (UI*)o;
    if (SceneCollection)
        for (size_t i = 0; i < SceneCollection->Childrens.size(); i++)
            if (SceneCollection->Childrens[i]->getType() == ObjectType::ui)
                return (UI*)SceneCollection->Childrens[i];
    return new UI(NULL);
}

// crear la IMAGEN recien cuando se eligio el archivo (cancelar el explorador = no crear);
// nace con la textura puesta y su tamano real
static void Add2DPickedImage(const std::string& path) {
    UI* root = UIRootToAdd();
    Image2D* img = new Image2D(root, Vector3(0.0f, 0.0f, 0.0f));
    img->texture = path;
    int w = 0, h = 0;
    Texture2DGet(path, &w, &h);
    if (w > 0 && h > 0) { img->width = (float)w; img->height = (float)h; }
    DeselectAll();
    img->Select();
    g_redraw = true;
}
// idem para el VIDEO (la preview ademas trae el tamano real via ffprobe)
static void Add2DPickedVideo(const std::string& path) {
    UI* root = UIRootToAdd();
    Video2D* v = new Video2D(root, Vector3(0.0f, 0.0f, 0.0f));
    v->video = path;
    const VideoPreview* pv = Video2DPreview(path);
    if (pv && pv->widthReal > 0) { v->width = (float)pv->widthReal; v->height = (float)pv->heightReal; }
    DeselectAll();
    v->Select();
    g_redraw = true;
}

static void LayoutAction2DAdd(int id) {
    // imagen y video piden PRIMERO el archivo (con vista previa); cancelar no crea nada
    if (id == 1) { OpenFileBrowser(T("Load image"), T("Open"), ".png .jpg .jpeg .webp .bmp .tga .gif", Add2DPickedImage); return; }
    if (id == 8) { OpenFileBrowser(T("Load video"), T("Open"), ".mp4 .webm .gif .mov .avi", Add2DPickedVideo); return; }
    UI* root = UIRootToAdd();
    // posicion CERO: con el ancla en el centro (default) cero = centrado en la ventana
    Object* newValue = NULL;
    if (id == 0) newValue = new Text2D(root, Vector3(0.0f, 0.0f, 0.0f));
    if (id == 2) newValue = new Rect2D(root, Vector3(0.0f, 0.0f, 0.0f));
    if (id == 3) newValue = new Container2D(root, Vector3(0.0f, 0.0f, 0.0f));
    if (id == 4) newValue = new Slice9(root, Vector3(0.0f, 0.0f, 0.0f));
    if (id == 5) newValue = new Button2D(root, Vector3(0.0f, 0.0f, 0.0f));
    if (id == 6) newValue = new Expand2D(root, Vector3(0.0f, 0.0f, 0.0f));
    if (id == 7) { AddImportUI(); return; }   // importar un .w3dui (mismo flujo que Imports)
    if (!newValue) return;
    DeselectAll();
    newValue->Select();
    g_redraw = true;
}

static void LayoutOpenMenu2DAdd(int x, int y) {
    if (!gMenu2DAdd) gMenu2DAdd = new PopupMenu();
    gMenu2DAdd->Clear();
    gMenu2DAdd->title = T("Add");
    gMenu2DAdd->Add(T("Text"), 0, (int)IconType::list);
    gMenu2DAdd->Add(T("Image"), 1, (int)IconType::photo);
    gMenu2DAdd->Add(T("Rectangle"), 2, (int)IconType::plane);
    gMenu2DAdd->Add(T("Container"), 3, (int)IconType::folder);
    gMenu2DAdd->Add("Slice 9", 4, (int)IconType::grid);
    gMenu2DAdd->Add(T("Button"), 5, (int)IconType::object);
    gMenu2DAdd->Add(T("Expand"), 6, (int)IconType::arrowRight);
    gMenu2DAdd->Add(T("Video"), 8, (int)IconType::camera);
    gMenu2DAdd->Add(T("Import w3dui"), 7, (int)IconType::archive);
    gMenu2DAdd->action = LayoutAction2DAdd;
    if (OpenMenu && OpenMenu != gMenu2DAdd) OpenMenu->Close();
    gMenu2DAdd->Open(x, y, MenuScreenW, MenuScreenH);
    OpenMenu = gMenu2DAdd;
}

static PopupMenu* gMenu2DPivot = NULL;
static Editor2D*  gMenu2DPivotOf = NULL;   // el editor cuyo boton abrio el menu

static void LayoutAction2DPivot(int id) {
    if (gMenu2DPivotOf) { gMenu2DPivotOf->pivotMode = id; g_redraw = true; }
}
static void LayoutOpenMenu2DPivot(Editor2D* ed, int x, int y) {
    if (!gMenu2DPivot) { gMenu2DPivot = new PopupMenu(); gMenu2DPivot->action = LayoutAction2DPivot; }
    gMenu2DPivotOf = ed;
    gMenu2DPivot->Clear();
    gMenu2DPivot->title = T("Pivot");
    gMenu2DPivot->Add(T("Median Point"),   0, (int)IconType::pivotMedian);
    gMenu2DPivot->Add(T("Active Element"), 1, (int)IconType::pivotActive);
    gMenu2DPivot->Add(T("2D Cursor"),      2, (int)IconType::pivotCursor);
    if (OpenMenu && OpenMenu != gMenu2DPivot) OpenMenu->Close();
    gMenu2DPivot->Open(x, y, MenuScreenW, MenuScreenH);
    OpenMenu = gMenu2DPivot;
}

// ---- menu Seleccionar del Editor 2D (solo elementos 2D) ----
static PopupMenu* gMenu2DSelect = NULL;
static void LayoutAction2DSelect(int id) { Editor2DSelect(id); }
static void LayoutOpenMenu2DSelect(int x, int y) {
    if (!gMenu2DSelect) {
        gMenu2DSelect = new PopupMenu();
        gMenu2DSelect->title  = T("Select");
        gMenu2DSelect->action = LayoutAction2DSelect;
        gMenu2DSelect->Add(T("All"), 0)->shortcut = "@2d.selectAll";
        gMenu2DSelect->Add(T("None"), 1)->shortcut = "@2d.deselect";
        gMenu2DSelect->Add(T("Invert"), 2)->shortcut = "@2d.invert";
    }
    if (OpenMenu && OpenMenu != gMenu2DSelect) OpenMenu->Close();
    gMenu2DSelect->Open(x, y, MenuScreenW, MenuScreenH);
    OpenMenu = gMenu2DSelect;
}

// ---- menu Objeto del Editor 2D: Transformar (G/R/S) + duplicar/emparentar/borrar ----
static PopupMenu* gMenu2DObj = NULL;
static PopupMenu* gMenu2DXform = NULL;
static Editor2D* gEditor2DMenu = NULL;   // el editor que abrio el menu (para G/R/S)
static void LayoutAction2DObject(int id) {
    Editor2D* ed = gEditor2DMenu;
    switch (id) {
        case 100: if (ed) ed->StartXform2D(1); break;      // Move (G)
        case 101: if (ed) ed->StartXform2D(2); break;      // Rotate (R)
        case 102: if (ed) ed->StartXform2D(3); break;      // Scale (S)
        case 1:   Editor2DDuplicateSelection(ed); break;   // Duplicate (Shift D)
        case 40: {   // EMPARENTAR la seleccion 2D al elemento ACTIVO (o al UI activo)
            if (!ActiveObj) break;
            if (!UI2D_IsElement2D(ActiveObj) && ActiveObj->getType() != ObjectType::ui) break;
            std::vector<Object*> sel = ObjSelects;   // copia: ReparentSimple no toca la seleccion
            ReparentGroupStart();                    // N elementos emparentados = UN Ctrl+Z
            for (size_t i = 0; i < sel.size(); i++)
                if (sel[i] != ActiveObj && UI2D_IsElement2D(sel[i]))
                    ReparentSimple(sel[i], ActiveObj);
            ReparentGroupEnd();
            g_redraw = true;
        } break;
        case 41: {   // DESEMPARENTAR: cada elemento vuelve a colgar de su UI raiz
            std::vector<Object*> sel = ObjSelects;
            ReparentGroupStart();
            for (size_t i = 0; i < sel.size(); i++) {
                if (!UI2D_IsElement2D(sel[i])) continue;
                Object* r = sel[i]->Parent;
                while (r && r->getType() != ObjectType::ui) r = r->Parent;
                if (r && sel[i]->Parent != r) ReparentSimple(sel[i], r);
            }
            ReparentGroupEnd();
            g_redraw = true;
        } break;
        case 3: Editor2DDeleteSelection(); break;         // Delete (X)
    }
}
static void LayoutOpenMenu2DObj(Editor2D* ed, int x, int y) {
    gEditor2DMenu = ed;
    if (!gMenu2DObj) {
        gMenu2DXform = new PopupMenu();
        gMenu2DXform->action = LayoutAction2DObject;
        gMenu2DXform->Add(T("Move"), 100)->shortcut = "@2d.move";
        gMenu2DXform->Add(T("Rotate"), 101)->shortcut = "@2d.rotate";
        gMenu2DXform->Add(T("Scale"), 102)->shortcut = "@2d.scale";
        gMenu2DObj = new PopupMenu();
        gMenu2DObj->title  = T("Object");
        gMenu2DObj->action = LayoutAction2DObject;
        gMenu2DObj->Add(T("Transform"), 0, -1, gMenu2DXform);
        gMenu2DObj->Add(T("Duplicate Objects"), 1)->shortcut = "@2d.duplicate";
        gMenu2DObj->Add(T("Set Parent"), 40);
        gMenu2DObj->Add(T("Clear Parent"), 41);
        gMenu2DObj->Add(T("Delete"), 3, IconType::trash)->shortcut = "@2d.delete";
    }
    if (OpenMenu && OpenMenu != gMenu2DObj) OpenMenu->Close();
    gMenu2DObj->Open(x, y, MenuScreenW, MenuScreenH);
    OpenMenu = gMenu2DObj;
}

// ---- menu View del Editor 2D: la vista (zoom) + la vista de juego ----
static PopupMenu* gMenu2DView = NULL;
static Editor2D* gEditor2DView = NULL;
static MenuItem* gItem2DGameView = NULL;   // el check apunta al miembro del viewport: se RE-APUNTA al abrir
static void LayoutAction2DView(int id) {
    if (id >= 1 && id <= 4) Editor2DZoomExact(gEditor2DView, id);
    else if (id == 10) Editor2DFrameSelection(gEditor2DView);
    else if (id == 11) g_redraw = true;   // vista de juego: el check ya muto ed->gameView
}
static void LayoutOpenMenu2DView(Editor2D* ed, int x, int y) {
    gEditor2DView = ed;
    if (!gMenu2DView) {
        gMenu2DView = new PopupMenu();
        gMenu2DView->title  = T("View");
        gMenu2DView->action = LayoutAction2DView;
        // VISTA DE JUEGO (numpad 0): la UI encajada en el viewport, sin guias ni puntos de
        // agarre, como se veria en una pantalla de esas proporciones (el analogo de la vista
        // de camara del 3D). El estado es POR VIEWPORT, pero el menu es static y lo comparten
        // todos los Editor2D -> el checkbox se RE-APUNTA abajo, en cada apertura.
        gItem2DGameView = gMenu2DView->AddCheck(T("Game View"), 11,
                                                      ed ? &ed->gameView : NULL);
        gItem2DGameView->shortcut = "@2d.gameView";
        gMenu2DView->Add(T("Frame Selected"), 10)->shortcut = "@2d.focus";
        gMenu2DView->Add(T("Zoom 1:1"), 1);   // pixel-perfect: un px del lienzo = uno real
        gMenu2DView->Add(T("Zoom 2:1"), 2);
        gMenu2DView->Add(T("Zoom 3:1"), 3);
        gMenu2DView->Add(T("Zoom 4:1"), 4);
    }
    // el menu es UNO solo para todos los viewports 2D: el check de "Game View" tiene que
    // apuntar al miembro del viewport que lo abrio (sino togglea el de otro)
    if (gItem2DGameView && ed) gItem2DGameView->checkbox = &ed->gameView;
    if (OpenMenu && OpenMenu != gMenu2DView) OpenMenu->Close();
    gMenu2DView->Open(x, y, MenuScreenW, MenuScreenH);
    OpenMenu = gMenu2DView;
}

// click en la barra del Editor 2D: por ROL (BarRole2D), no por indice -> agregar/reordenar
// botones no rompe el dispatch (mismo esquema que la barra del 3D).
static bool LayoutClickBar2D(Editor2D* ed, int mx, int my) {
    if (!ed) return false;
    ed->UpdateBar(); // sx/sy frescos
    std::vector<Button*>& B = ed->BarButtons;
    for (size_t i = 1; i < B.size(); i++) {
        Button* b = B[i];
        if (!b || !b->visible || !b->Contains(mx, my)) continue;
        int bx = b->sx, by = b->sy + b->height - GlobalScale;
        switch (b->role) {
            case BR2D_Select: LayoutOpenMenu2DSelect(bx, by);    return true;
            case BR2D_Add:    LayoutOpenMenu2DAdd(bx, by);       return true;
            case BR2D_Object: LayoutOpenMenu2DObj(ed, bx, by);   return true;
            case BR2D_View:   LayoutOpenMenu2DView(ed, bx, by);  return true;
            case BR2D_Pivot:  LayoutOpenMenu2DPivot(ed, bx, by); return true;
        }
    }
    return false;
}

// ===== barra del IDE (editor de texto lua): selector de script + Save + Refresh =====
static PopupMenu* gMenuIDEScript = NULL;
static IDE* gIDEMenuOf = NULL;                    // el IDE cuyo selector esta abierto
static std::vector<std::string> gIDEScriptPaths;  // rutas del menu (id = indice)
static std::string gIDEChangeTo;                  // destino pendiente del popup de confirmar

// "Si" del popup: descartar los cambios y abrir el otro script
static void IDEConfirmChange() {
    if (gIDEMenuOf && !gIDEChangeTo.empty()) gIDEMenuOf->OpenFile(gIDEChangeTo);
    gIDEChangeTo.clear();
}

// "Load .lua...": un script de AFUERA (descargas, otra carpeta del celular). Se IMPORTA como cualquier asset: con
// un proyecto montado queda copiado adentro (scripts/) y aparece en esta lista; sin proyecto se edita el archivo.
static const int kIDELoadLua = 100000;
static void IDELoadLuaPicked(const std::string& picked) {
    IDE* ide = gIDEMenuOf;
    if (!ide || picked.empty()) return;
    const std::string path = W3dLoadExternal(picked);
    if (ide->dirty && path != ide->file) {   // con cambios sin guardar avisa antes de descartarlos
        gIDEChangeTo = path;
        if (!confirmPopup) confirmPopup = new ConfirmPopup();
        confirmPopup->Open(std::string(T("Discard the unsaved changes of")) + " " + IDEScriptName(ide->file) + "?", IDEConfirmChange);
        return;
    }
    ide->OpenFile(path);
}
// (harness) lo mismo que elegir el archivo en el explorador de "Load .lua..."
void LayoutIDELoadLua(IDE* ide, const std::string& path) { gIDEMenuOf = ide; IDELoadLuaPicked(path); }
static void LayoutActionIDEScript(int id) {
    IDE* ide = gIDEMenuOf;
    if (ide && id == kIDELoadLua) { OpenFileBrowser(T("Load script"), T("Open"), ".lua", IDELoadLuaPicked); return; }
    if (!ide || id < 0 || id >= (int)gIDEScriptPaths.size()) return;
    const std::string& path = gIDEScriptPaths[id];
    if (path == ide->file) return; // ya es el abierto
    if (ide->dirty) {
        // cambiar con cambios sin guardar AVISA: popup Si/No antes de descartar
        gIDEChangeTo = path;
        if (!confirmPopup) confirmPopup = new ConfirmPopup();
        confirmPopup->Open(std::string(T("Discard the unsaved changes of")) + " " + IDEScriptName(ide->file) + "?",
                              IDEConfirmChange);
        return;
    }
    ide->OpenFile(path);
}

static void LayoutOpenMenuIDEScript(IDE* ide, int x, int y) {
    if (!ide) return;
    if (!gMenuIDEScript) {
        gMenuIDEScript = new PopupMenu();
        gMenuIDEScript->action = LayoutActionIDEScript;
    }
    gIDEMenuOf = ide;
    gMenuIDEScript->Clear();
    IDECollectScripts(&gIDEScriptPaths); // escenas + content/*.lua, sin repetidos
    gMenuIDEScript->Add(T("Load .lua..."), kIDELoadLua, (int)IconType::folder);   // un .lua de afuera (importar)
    if (gIDEScriptPaths.empty()) {
        gMenuIDEScript->Add(T("(no .lua scripts)"), -1);
    } else {
        for (size_t i = 0; i < gIDEScriptPaths.size(); i++) {
            bool open = (gIDEScriptPaths[i] == ide->file);
            // el abierto se MARCA: tilde (icono de exito) + texto en verde accent
            MenuItem* it = gMenuIDEScript->Add(IDEScriptName(gIDEScriptPaths[i]), (int)i,
                                                   open ? (int)IconType::notifOk : -1);
            it->green = open;
        }
    }
    if (OpenMenu && OpenMenu != gMenuIDEScript) OpenMenu->Close();
    gMenuIDEScript->Open(x, y, MenuScreenW, MenuScreenH);
    OpenMenu = gMenuIDEScript;
}

// ===== menu ARCHIVO del IDE: Guardar / Guardar como... / Guardar en el w3d (o fuera) / Cargar .lua... =====
static PopupMenu* gMenuIDEFile = NULL;
static IDE* gIDEFileOf = NULL;
enum { IDEA_Save = 1, IDEA_SaveAs, IDEA_ToW3d, IDEA_OutsideW3d, IDEA_Load };
// la carpeta de la biblioteca en la que esta un archivo ("" = la raiz o no esta en la biblioteca)
static std::string FolderLibraryOf(int view, const std::string& id) {
    if (id.empty()) return std::string();
    std::vector<W3dResourceItem> items; W3dViewRecList(view, items);
    for (size_t i = 0; i < items.size(); i++) if (items[i].id == id || items[i].entry == id) return items[i].folder;
    return std::string();
}
// guardar el .w3d del disco (lo que vive ADENTRO recien queda en el archivo despues de esto)
static bool SaveProjectToDisk() {
    extern std::string w3dPath;
    return !w3dPath.empty() && W3dContainerHasMounted() && !W3dContainerInMemory() && SaveW3D(w3dPath);
}
// UN SCRIPT GUARDADO ADENTRO DEL PROYECTO: ya esta listo para usar (la biblioteca lo lista, los objetos lo cargan). En la
// PC con el .w3d en el disco, el archivo se reescribe; en el NAVEGADOR no (ahi guardar el proyecto es DESCARGARLO, y eso
// lo pide el usuario: guardar un script no puede bajar nada) y sin .w3d todavia el script queda en memoria. El aviso dice
// que falta para no perderlo; el '*' del proyecto sigue hasta que se guarde (o se descargue)
void IDESavedInProject(const std::string& name) {
#ifdef __EMSCRIPTEN__
    Notify(name + " " + T("saved in the project. Save or download the project so you don't lose it"), false);
#else
    if (SaveProjectToDisk()) { Notify(name + " " + T("saved inside the .w3d"), false); return; }
    Notify(name + " " + T("saved in the project (in memory). Save the project, or the script as a file, so you don't lose it"), false);
#endif
}
static bool IDESaveAsAction(const SaveAsDestination& d, bool force, bool* exists, std::string& msg) {
    IDE* ide = gIDEFileOf;
    if (!ide) { msg = "Nothing to save"; return false; }
    if (d.internal) {
        W3dContainerEnsure();   // (un proyecto que todavia no es un archivo: el script queda en memoria)
        std::string base = d.name;
        if (base.size() > 4 && base.substr(base.size() - 4) == ".lua") base = base.substr(0, base.size() - 4);
        const std::string entry = "scripts/" + W3dSlugEntry(base) + ".lua";
        if (entry != ide->file && W3dContainerExists(entry) && !force) { if (exists) *exists = true; return false; }
        const std::string t = ide->GetText();
        if (!W3dContainerWriteEntry(entry, t.empty() ? "" : t.c_str(), t.size())) { msg = "Could not write the file"; return false; }
        ide->file = entry; ide->dirty = false;
        W3dResourcesViewInvalidate();
        W3dViewRecMove(W3D_VIEW_SCRIPTS, entry, d.folderProject);   // en esa carpeta de la biblioteca
        IDESavedInProject(IDEScriptName(entry));
        g_redraw = true;
        return true;
    }
    const std::string path = W3dPathInFolder(d.folderDisk, d.name, "");
    if (path != ide->file && w3dFileSystem::FileExists(path) && !force) { if (exists) *exists = true; return false; }
    const std::string prev = ide->file;
    ide->file = path;
    if (!ide->Save()) { ide->file = prev; msg = "Could not write the file"; return false; }
    g_redraw = true;
    return true;
}
void IDEOpenSaveAs(IDE* ide) {
    if (!ide) return;
    gIDEFileOf = ide;
    const std::string name = ide->file.empty() ? std::string("script.lua") : IDEScriptName(ide->file);
    // (uno nuevo va adentro del proyecto: aunque no haya .w3d todavia, queda en memoria)
    const bool internal = ide->file.empty() ? true : W3dIsEntryName(ide->file);
    OpenSaveAsPopup(T("Save As..."), name, ".lua", internal, FolderLibraryOf(W3D_VIEW_SCRIPTS, ide->file), IDESaveAsAction);
}
// un script de AFUERA pasa ADENTRO del .w3d (las referencias de los objetos lo siguen) y se guarda el proyecto
static void IDEMoveToW3d(IDE* ide) {
    if (!ide || ide->file.empty()) return;
    W3dContainerEnsure();   // (sin .w3d todavia: queda en memoria)
    if (ide->dirty && !ide->Save()) return;   // que el archivo de afuera tenga lo ultimo
    std::string newValue, reason;
    if (!W3dViewRecSetLocation(W3D_VIEW_SCRIPTS, ide->file, 0, std::string(), &newValue, &reason)) {
        Notify(T(reason.empty() ? "It couldn't be copied into the .w3d" : reason.c_str()), true);
        return;
    }
    ide->file = newValue; ide->dirty = false;
    IDESavedInProject(IDEScriptName(newValue));
    g_redraw = true;
}
// un script de ADENTRO sale a un archivo del disco (donde se elija); los objetos pasan a usar ese archivo
static void IDEOutsidePicked(const std::string& picked) {
    IDE* ide = gIDEFileOf;
    if (!ide || ide->file.empty()) return;
    std::string base = IDEScriptName(ide->file);
    if (base.size() > 4 && base.substr(base.size() - 4) == ".lua") base = base.substr(0, base.size() - 4);
    const std::string path = W3dOutputPath(picked, base, ".lua");
    if (ide->dirty) { ide->silent = true; ide->Save(); ide->silent = false; }   // la entrada tiene lo ultimo
    std::string newValue, reason;
    if (!W3dViewRecSetLocation(W3D_VIEW_SCRIPTS, ide->file, 1, path, &newValue, &reason)) {
        Notify(T(reason.empty() ? "The file couldn't be written" : reason.c_str()), true);
        return;
    }
    ide->file = newValue; ide->dirty = false;
    SaveProjectToDisk();   // el proyecto recuerda que ahora el script esta afuera
    Notify(IDEScriptName(newValue) + " " + T("saved outside the .w3d"), false);
    g_redraw = true;
}
static void LayoutActionIDEFile(int id) {
    IDE* ide = gIDEFileOf;
    if (!ide) return;
    switch (id) {
        case IDEA_Save:     ide->SaveInProject(); break;
        case IDEA_SaveAs: IDEOpenSaveAs(ide); break;
        case IDEA_ToW3d:       IDEMoveToW3d(ide); break;
        case IDEA_OutsideW3d:    OpenFileBrowser(T("Save outside the .w3d..."), T("Use this folder"), ".lua", IDEOutsidePicked, true); break;
        case IDEA_Load:      gIDEMenuOf = ide; OpenFileBrowser(T("Load script"), T("Open"), ".lua", IDELoadLuaPicked); break;
    }
    g_redraw = true;
}
static void LayoutOpenMenuIDEFile(IDE* ide, int x, int y) {
    if (!ide) return;
    gIDEFileOf = ide;
    if (!gMenuIDEFile) { gMenuIDEFile = new PopupMenu(); gMenuIDEFile->action = LayoutActionIDEFile; }
    gMenuIDEFile->Clear();
    gMenuIDEFile->Add(T("Save"), IDEA_Save, (int)IconType::save);
    gMenuIDEFile->Add(T("Save As..."), IDEA_SaveAs);
    if (!ide->file.empty()) {
        if (W3dIsEntryName(ide->file)) gMenuIDEFile->Add(T("Save outside the .w3d..."), IDEA_OutsideW3d);
        else                                    gMenuIDEFile->Add(T("Save inside the .w3d"), IDEA_ToW3d);
    }
    gMenuIDEFile->Add(T("Load .lua..."), IDEA_Load, (int)IconType::folder);
    if (OpenMenu && OpenMenu != gMenuIDEFile) OpenMenu->Close();
    gMenuIDEFile->Open(x, y, MenuScreenW, MenuScreenH);
    OpenMenu = gMenuIDEFile;
}
// ===== menus EDITAR y SELECCIONAR del IDE =====
static PopupMenu* gMenuIDEEdit = NULL;
static PopupMenu* gMenuIDESelect = NULL;
static IDE* gIDEEditFrom = NULL;
enum { IDEE_Undo = 1, IDEE_Redo, IDEE_Cut, IDEE_Copy, IDEE_Paste, IDEE_Find, IDEE_Replace, IDEE_All };
static void LayoutActionIDEEdit(int id) {
    IDE* ide = gIDEEditFrom;
    if (!ide) return;
    extern void IDEOpenFind(IDE*, bool);
    switch (id) {
        case IDEE_Undo:   ide->UndoText(); break;
        case IDEE_Redo:    ide->RedoText(); break;
        case IDEE_Cut:     ide->Cut(); break;
        case IDEE_Copy:     ide->Copy(); break;
        case IDEE_Paste:      ide->Paste(); break;
        case IDEE_Find:     IDEOpenFind(ide, false); break;
        case IDEE_Replace: IDEOpenFind(ide, true); break;
        case IDEE_All:       ide->SelectAll(); break;
    }
    g_redraw = true;
}
static void LayoutOpenMenuIDEEdit(IDE* ide, int x, int y) {
    if (!ide) return;
    gIDEEditFrom = ide;
    if (!gMenuIDEEdit) { gMenuIDEEdit = new PopupMenu(); gMenuIDEEdit->action = LayoutActionIDEEdit; }
    gMenuIDEEdit->Clear();
    gMenuIDEEdit->Add(T("Undo"), IDEE_Undo, (int)IconType::undo)->shortcut = "@ide.undo";
    gMenuIDEEdit->Add(T("Redo"), IDEE_Redo, (int)IconType::redo)->shortcut = "@ide.redo";
    gMenuIDEEdit->Add(T("Cut"), IDEE_Cut, IconType::scissors)->shortcut = "@ide.cut";
    gMenuIDEEdit->Add(T("Copy"), IDEE_Copy, IconType::copy)->shortcut = "@ide.copy";
    gMenuIDEEdit->Add(T("Paste"), IDEE_Paste, IconType::paste)->shortcut = "@ide.paste";
    gMenuIDEEdit->Add(T("Find..."), IDEE_Find)->shortcut = "@ide.find";
    gMenuIDEEdit->Add(T("Replace..."), IDEE_Replace)->shortcut = "@ide.replace";
    if (OpenMenu && OpenMenu != gMenuIDEEdit) OpenMenu->Close();
    gMenuIDEEdit->Open(x, y, MenuScreenW, MenuScreenH);
    OpenMenu = gMenuIDEEdit;
}
static void LayoutOpenMenuIDESelect(IDE* ide, int x, int y) {
    if (!ide) return;
    gIDEEditFrom = ide;
    if (!gMenuIDESelect) { gMenuIDESelect = new PopupMenu(); gMenuIDESelect->action = LayoutActionIDEEdit; }
    gMenuIDESelect->Clear();
    gMenuIDESelect->Add(T("Select All"), IDEE_All)->shortcut = "@ide.all";
    if (OpenMenu && OpenMenu != gMenuIDESelect) OpenMenu->Close();
    gMenuIDESelect->Open(x, y, MenuScreenW, MenuScreenH);
    OpenMenu = gMenuIDESelect;
}
// (harness) una accion de Editar / Seleccionar sin abrir el menu (ids: 1 deshacer .. 8 seleccionar todo)
void LayoutIDEEditAction(IDE* ide, int id) { gIDEEditFrom = ide; LayoutActionIDEEdit(id); }

// (harness) accion del menu Archivo del IDE sin abrir el menu; "fuera del w3d" con la carpeta ya elegida
void LayoutIDEFileAction(IDE* ide, int id) { gIDEFileOf = ide; LayoutActionIDEFile(id); }
void LayoutIDEOutsideOfW3d(IDE* ide, const std::string& folder) { gIDEFileOf = ide; IDEOutsidePicked(folder); }

// click en la barra del IDE: por ROL (como la barra del 2D), no por indice
bool LayoutClickBarIDE(IDE* ide, int mx, int my) {
    if (!ide) return false;
    ide->SyncBar();
    ide->UpdateBar(); // sx/sy frescos
    std::vector<Button*>& B = ide->BarButtons;
    for (size_t i = 1; i < B.size(); i++) {
        Button* b = B[i];
        if (!b || !b->visible || !b->Contains(mx, my)) continue;
        int bx = b->sx, by = b->sy + b->height - GlobalScale;
        switch (b->role) {
            case BRIDE_Script:  LayoutOpenMenuIDEScript(ide, bx, by); return true;
            case BRIDE_Save: LayoutOpenMenuIDEFile(ide, bx, by); return true;   // menu Archivo
            case BRIDE_Refresh: ide->Refresh(); return true;
            case BRIDE_Edit:  LayoutOpenMenuIDEEdit(ide, bx, by); return true;       // menu Editar
            case BRIDE_Select: LayoutOpenMenuIDESelect(ide, bx, by); return true;
            case BRIDE_Compile: ide->CompileShader(); return true;   // (un shader .glsl)
        }
    }
    return false;
}

// ---- navegacion GENERICA de la barra de un viewport por teclado (Symbian, sin mouse), modelada en la del Timeline:
// mueve barFocusIndex entre los botones VISIBLES (el [0] tipo/split SIEMPRE navegable) y activa el enfocado
// simulando el click en su centro. La usan Editor2D (kind 6) e IDE (kind 8). El foco se DIBUJA solo (RenderBar lee
// barFocusIndex); para que no se resetee por frame hay que exceptuar estos kinds en ViewPorts.cpp (como el Timeline). ----
static void LayoutBarFocusMove(ViewportBase* vp, int dir) {
    if (!vp) return;
    std::vector<Button*>& B = vp->BarButtons;
    int maxIdx = (int)B.size() - 1;
    if (maxIdx < 0) return;
    int idx = vp->barFocusIndex;
    if (idx < 0) idx = 0;                          // primera vez: arrancar en el [0]
    for (int k = 0; k <= maxIdx; k++) {
        idx += dir;
        if (idx > maxIdx) idx = 0;                 // wrap
        if (idx < 0) idx = maxIdx;
        if (idx == 0 || B[idx]->visible) break;    // el [0] SIEMPRE es navegable (cambia el tipo de viewport)
    }
    vp->barFocusIndex = idx;
    vp->UpdateBar();                         // auto-scroll para mostrar el enfocado
    g_redraw = true;
}
static bool LayoutBarFocusActivate(ViewportBase* vp) {
    if (!vp) return false;
    int idx = vp->barFocusIndex;
    if (idx < 0 || idx > (int)vp->BarButtons.size() - 1) return false;
    vp->UpdateBar();                         // sx/sy frescos antes del hit-test
    if (idx == 0) { LayoutOpenMenuType(vp); return true; } // [0] = menu tipo/split (cambiar el viewport a 3D/outliner/etc.)
    Button* b = vp->BarButtons[idx];
    if (!b->visible) return false;
    int mx = b->sx + b->width / 2, my = b->sy + b->height / 2;
    if (vp->ViewportKind() == 6) return LayoutClickBar2D((Editor2D*)vp, mx, my);
    if (vp->ViewportKind() == 8) return LayoutClickBarIDE((IDE*)vp, mx, my);
    return false;
}

// ===== menu "Animation" del UV editor: keyframes de la CURVA UV de la vertex anim ACTIVA
// (kind 3) del mesh en edicion. La curva de UV es SEPARADA de la de vertices ("son cosas
// distintas"): el insert captura SOLO el mapeo (VertexAnimInsertKeyframeUV, el mismo
// camino que el auto-key del UV y de la pose 2D), y borrar/limpiar sacan SOLO la capa UV
// de los frames (los keyframes de vertices no se tocan desde aca; misma regla que el dope
// sheet: nunca dejar la anim sin frames). =====
static PopupMenu* gMenuUVAnim = NULL;
static void LayoutActionUVAnim(int id) {
    extern void VertexAnimInsertKeyframeUV();
    // Insert Keyframe: va por el dispatch por CONTEXTO (en UVModePose keyframea la POSE 2D en el
    // clip del armature 2D; en edicion de UVs keyframea la capa uv, como siempre). Los ids 530-533
    // llegan aca cuando el usuario elige un canal en el submenu.
    if (id == 0)   { InsertKeyframeContext(KfChannelAll, true); return; }
    if (id == 530) { InsertKeyframeContext(KfChannelAll, true); return; }
    if (id == 531) { InsertKeyframeContext(KfChannelLoc,   true); return; }
    if (id == 532) { InsertKeyframeContext(KfChannelRot,   true); return; }
    if (id == 533) { InsertKeyframeContext(KfChannelScl,   true); return; }
    if (id == 540 || id == 541 || id == 542) { LayoutActionObject(id); return; } // capas (Vertices/Normales/UV)
    // Delete / Clear: hace falta la vertex anim activa del mesh en edicion
    if (ActiveAnimKind != 3 || !ActiveAnimMesh || (Object*)ActiveAnimMesh != g_editMesh) {
        Notify(T("Insert Keyframe: select an object animation in the timeline"), true);
        return;
    }
    Mesh* m = ActiveAnimMesh;
    VertexAnimationActive* va = FindTargetAnim(m);
    int ai = va ? va->currentAnim : -1;
    if (ai < 0 || ai >= (int)m->animations.size() || !m->animations[ai]) return;
    VertexAnimation* an = m->animations[ai];
    extern void UndoKeyframesStart(); extern void UndoKeyframesConfirm();
    UndoKeyframesStart(); // Ctrl+Z: snapshot de la anim antes de tocar los frames
    if (id == 1) {        // Delete Keyframe: saca la CAPA UV del cuadro actual
        VertexFrame* vf = NULL;
        for (size_t k = 0; k < an->frames.size(); ++k)
            if (an->frames[k]->frame == CurrentFrame) { vf = an->frames[k]; break; }
        if (!vf || !vf->uvs)
            Notify(T("Delete Keyframe: no keyframe at the current frame"), true);
        else if (!vf->positions && (int)an->frames.size() <= 1)
            Notify(T("Delete Keyframe: the animation needs at least one keyframe"), true);
        else
            VertexAnimDeleteLayer(*an, CurrentFrame, 1);
    } else if (id == 2) {   // Clear Keyframes: saca la capa UV de TODOS los frames
        for (size_t k = an->frames.size(); k-- > 0; ) {
            VertexFrame* vf = an->frames[k];
            if (!vf->uvs) continue;
            if (!vf->positions && (int)an->frames.size() <= 1) continue; // dejar 1 frame vivo
            VertexAnimDeleteLayer(*an, vf->frame, 1);
        }
    }
    UndoKeyframesConfirm();
    m->skinGeomVersion++; // la pose del frame actual pudo cambiar
    g_redraw = true;
}
static void LayoutOpenMenuUVAnim(UVEditor* uv, int x, int y) {
    if (!uv) return;
    if (!gMenuUVAnim) {
        // (regla de los titulos: el boton es un icono sin texto -> el menu lleva titulo)
        gMenuUVAnim = new PopupMenu();
        gMenuUVAnim->title = T("Animation");
        gMenuUVAnim->Add(T("Insert Keyframe"), 0, IconType::keyframe);
        // mismas CAPAS que el menu Animation del 3D (la animacion es la misma; cambia la capa)
        static PopupMenu* gMenuUVLayer = NULL;
        gMenuUVLayer = new PopupMenu();
        gMenuUVLayer->Add(T("Vertices"), 540, IconType::mesh);
        gMenuUVLayer->Add(T("Normals"),  541, IconType::normalVertex);
        gMenuUVLayer->Add(T("UV"),       542, IconType::texture);
        gMenuUVAnim->Add(T("Insert Mesh Keyframe"), 0, IconType::mesh, gMenuUVLayer);
        gMenuUVAnim->Add(T("Delete Keyframe"), 1, IconType::trash);
        gMenuUVAnim->Add(T("Clear Keyframe"),  2);
        gMenuUVAnim->action = LayoutActionUVAnim;
    }
    LayoutSyncInsertKeySubmenu(gMenuUVAnim, 0, true); // en UVModePose abre los canales; en edicion queda plano
    if (OpenMenu && OpenMenu != gMenuUVAnim) OpenMenu->Close();
    gMenuUVAnim->Open(x, y, MenuScreenW, MenuScreenH);
    OpenMenu = gMenuUVAnim;
}

// ===== selector de MODO del UV editor (Edicion / Pintura de pesos), desplegable como el Mode del
// 3D. Es el modo PROPIO del viewport UV (uv->uvMode), NO el InteractionMode global: pintar pesos
// en el UV no debe sacar al 3D de Edit Mode (decision documentada en UVEditor.h). =====
static PopupMenu* gMenuUVMode = NULL;
static UVEditor*  gUVModeOf = NULL;   // el editor cuyo boton abrio el menu
static void LayoutActionUVMode(int id) {
    if (!gUVModeOf) return;
    const int modePrev = gUVModeOf->uvMode;
    // el Tab del UV alterna Edit Bones con el modo del que se VENIA: elegir un modo a mano tambien
    // actualiza esa memoria (sino Tab devolvia a un modo viejo que el usuario ya habia dejado)
    if (id == UVModeBones) {
        if (gUVModeOf->uvMode != UVModeBones) gUVModeOf->uvPreviousMode = gUVModeOf->uvMode;
    } else {
        gUVModeOf->uvPreviousMode = id;
    }
    gUVModeOf->uvMode = id; // UVModeObject / UVModeEdit / UVModeWeights / UVModeBones / UVModePose
    // el objeto activo del UV sigue al modo elegido a mano (sino el Tab de vuelta entraba a otra cosa)
    if (id == UVModeBones || id == UVModePose) gUVModeOf->uvObjArm = true;
    else if (id == UVModeEdit)               gUVModeOf->uvObjArm = false;
    else if (id == UVModeObject) {
        // entrar a MODO OBJETO deja definido QUE queda seleccionado, igual que el Tab: viniendo de
        // huesos/pose sigue activo el ARMATURE que se estaba editando; de cualquier otro modo
        // (edicion de UVs, pintura de pesos) el objeto activo es la GEOMETRIA. Sin rig 2D, siempre
        // la geometria (no hay armature que seleccionar).
        Mesh* mm = (ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
        const bool fromBones = (modePrev == UVModeBones || modePrev == UVModePose);
        gUVModeOf->uvObjArm  = fromBones && mm && mm->HasArm2D();
    }
    // entrar a huesos/pose desde el desplegable tambien lleva el panel a la pestania del rig 2D
    if (id == UVModeBones || id == UVModePose) PropsGoToArmature2D();
    g_redraw = true;
}
static void LayoutOpenMenuUVMode(UVEditor* uv, int x, int y) {
    if (!uv) return;
    gUVModeOf = uv;
    if (!gMenuUVMode) gMenuUVMode = new PopupMenu();
    gMenuUVMode->Clear(); // se rearma para marcar (verde) el modo actual de ESTE editor
    gMenuUVMode->title = T("Mode");
    // OBJETO: el modo por defecto del UV (elegir la geometria o un armature 2D con el click)
    gMenuUVMode->Add(T("Object Mode"),  UVModeObject,  IconType::object)->green = (uv->uvMode == UVModeObject);
    gMenuUVMode->Add(T("Edit Mode"),    UVModeEdit, IconType::mesh)->green = (uv->uvMode == UVModeEdit);
    gMenuUVMode->Add(T("Weight Paint"), UVModeWeights,   IconType::mesh)->green = (uv->uvMode == UVModeWeights);
    // modos del ARMATURE 2D DEL MESH: solo si la malla activa TIENE huesos 2D (se crean en Add)
    { Mesh* mm = (ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
      if (mm && !mm->Arm2DBones().empty()) {
          gMenuUVMode->Add(T("Edit Bones"), UVModeBones, IconType::armature)->green = (uv->uvMode == UVModeBones);
          gMenuUVMode->Add(T("Pose Mode"),  UVModePose,   IconType::armature)->green = (uv->uvMode == UVModePose);
      } }
    gMenuUVMode->action = LayoutActionUVMode;
    if (OpenMenu && OpenMenu != gMenuUVMode) OpenMenu->Close();
    gMenuUVMode->Open(x, y, MenuScreenW, MenuScreenH);
    OpenMenu = gMenuUVMode;
}

// ===== menu ADD del UV editor: crea el ARMATURE 2D DEL MESH (huesos en espacio UV que deforman
// los UVs pesados; ver W3dBone2D en el Core) o agrega huesos raiz cuando ya existe. =====
static PopupMenu* gMenuUVAdd = NULL;
static UVEditor*  gUVAddOf = NULL;
static void LayoutActionUVAdd(int id) {
    if (!gUVAddOf) return;
    Mesh* m = (ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
    if (!m) return;
    if (id == 0) gUVAddOf->Armature2DCreate(m);   // crea (o entra a editar) el armature 2D
    else if (id == 1) {                           // hueso raiz nuevo (ya en modo huesos)
        gUVAddOf->Bone2DAdd(m);
        gUVAddOf->uvMode = UVModeBones;
    } else if (id == 2) gUVAddOf->Armature2DNew(m); // OTRO armature 2D (independiente)
    g_redraw = true;
}
static void LayoutOpenMenuUVAdd(UVEditor* uv, int x, int y) {
    if (!uv) return;
    gUVAddOf = uv;
    if (!gMenuUVAdd) gMenuUVAdd = new PopupMenu();
    gMenuUVAdd->Clear(); // se rearma: "Bone" solo aparece cuando el armature ya existe
    gMenuUVAdd->title = T("Add");
    Mesh* m = (ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
    if (m && m->HasArm2D()) {
        gMenuUVAdd->Add(T("Bone"), 1, IconType::armature);          // hueso en el armature ACTIVO
        gMenuUVAdd->Add(T("Armature 2D"), 2, IconType::armature);   // OTRO armature (rig aparte)
    } else gMenuUVAdd->Add(T("Armature 2D"), 0, IconType::armature);
    gMenuUVAdd->action = LayoutActionUVAdd;
    if (OpenMenu && OpenMenu != gMenuUVAdd) OpenMenu->Close();
    gMenuUVAdd->Open(x, y, MenuScreenW, MenuScreenH);
    OpenMenu = gMenuUVAdd;
}

// ===== menu ARMATURE del UV editor (solo en UVModeBones): las ops de edicion de huesos 2D +
// Set Parent (Ctrl+P) + el submenu Clear Parent de Alt+P (Disconnect Bone / Clear Parent). =====
static PopupMenu* gMenuUVArm = NULL;
static UVEditor*  gUVArmOf = NULL;   // el editor cuyo boton abrio el menu
static void LayoutActionUVArm(int id) {
    UVEditor* uv = gUVArmOf;
    if (!uv || uv->uvMode != UVModeBones) return;
    Mesh* m = (ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
    if (!m || (Object*)m != g_editMesh) return;
    if (id == 0) {                                   // Extrude: el tail nuevo queda agarrado
        int nb = uv->Bone2DExtrude(m);
        if (nb >= 0) uv->Bone2DDragEnd(m, nb, 2, false);
    } else if (id == 1) {                            // Duplicate: lo duplicado queda agarrado
        if (uv->Bone2DDuplicate(m) >= 0) uv->Bone2DXformStart(m, 1);
    } else if (id == 2) uv->Bone2DDelete(m);         // Delete
    // (id 3 = Set Parent y 4 = Clear Parent son SUBMENUS de 2 opciones: despachan por su
    //  action propia en Parent.cpp, no por aca)
    g_redraw = true;
}
static void LayoutOpenMenuUVArmature(UVEditor* uv, int x, int y) {
    if (!uv) return;
    gUVArmOf = uv;
    Mesh* m = (ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
    BoneAltPContext2D(m); // el submenu Clear Parent opera sobre los huesos 2D de ESTE mesh
    if (!gMenuUVArm) {
        gMenuUVArm = new PopupMenu();
        gMenuUVArm->title  = T("Armature");
        gMenuUVArm->action = LayoutActionUVArm;
        gMenuUVArm->Add(T("Extrude"), 0)->shortcut = "@uv.extrude";
        gMenuUVArm->Add(T("Duplicate"), 1)->shortcut = "@uv.duplicate";
        gMenuUVArm->Add(T("Delete"), 2)->shortcut = "@uv.delete";
        // Set Parent y Clear Parent son SUBMENUS de 2 opciones cada uno (Keep Offset/Connected y
        // Disconnect Bone/Clear Parent): despachan por SU action propia (Parent.cpp)
        gMenuUVArm->Add(T("Set Parent"), 3, -1, LayoutSubmenuBoneCtrlP())->shortcut = "@global.parent";
        gMenuUVArm->Add(T("Clear Parent"), 4, -1, LayoutSubmenuBoneAltP())->shortcut = "@global.clearParent";
    }
    if (OpenMenu && OpenMenu != gMenuUVArm) OpenMenu->Close();
    gMenuUVArm->Open(x, y, MenuScreenW, MenuScreenH);
    OpenMenu = gMenuUVArm;
}

// click en la barra del UV editor: por ROL (BarRoleUV), no por indice -> agregar/reordenar botones
// no rompe el dispatch. NO static: el comando de test 'uvbar' entra por este mismo camino.
bool LayoutClickBarUV(UVEditor* uv, int mx, int my) {
    if (!uv) return false;
    uv->UpdateBar(); // sx/sy frescos
    std::vector<Button*>& B = uv->BarButtons;
    for (size_t i = 1; i < B.size(); i++) {
        Button* b = B[i];
        if (!b || !b->visible || !b->Contains(mx, my)) continue;
        int bx = b->sx, by = b->sy + b->height - GlobalScale;
        switch (b->role) {
            case BRUV_Mode:      LayoutOpenMenuUVMode(uv, bx, by);    return true; // Edicion/Pintura/Huesos/Pose
            case BRUV_Add:       LayoutOpenMenuUVAdd(uv, bx, by);     return true; // Armature 2D / Bone
            case BRUV_Armature:  LayoutOpenMenuUVArmature(uv, bx, by); return true; // ops de huesos 2D + Parent
            case BRUV_View:      LayoutOpenMenuUV(uv, bx, by);        return true;  // checkboxes (Sync/Repeat)
            case BRUV_Select:    LayoutOpenMenuUVSelect(uv, bx, by);  return true;  // All/None/Invert/Linked
            case BRUV_SelMode:   LayoutOpenMenuUVSelMode(uv, bx, by); return true;  // Vertex/Edge/Face
            case BRUV_Pivot:     LayoutMenuPivotUV(bx, by);            return true; // variante 2D (sin Active Element)
            case BRUV_Snap:      LayoutOpenMenuUVSnap(uv, bx, by);    return true;
            case BRUV_UVMap:     LayoutOpenMenuUVMaps(uv, bx, by);   return true;  // que mapa UV se edita (+ nuevo)
            case BRUV_Texture:   LayoutOpenMenuUVTex(uv, bx, by);     return true; // dropdown de texturas
            case BRUV_Mipmap:    LayoutOpenMenuUVMip(uv, bx, by);     return true; // inspector de la piramide
            case BRUV_Edit:    UVToggleEditTexture(uv);            return true;    // pintura de textura on/off
            case BRUV_Save:   LayoutOpenMenuUVSave(uv, bx, by); return true;       // Save Texture / externa / interna
            case BRUV_Animation: LayoutOpenMenuUVAnim(uv, bx, by);    return true; // keyframes de la vertex anim
        }
    }
    return false;
}

bool LayoutMenuOpen() {
    return OpenMenu && OpenMenu->open;
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
// SOLO se dispara con un CLICK real. Estaba DENTRO de LayoutOpenBarMenu -- que la llaman tambien el
// hover del mouse y las flechas de navegacion del menu -> pasar POR ENCIMA de Stop reseteaba el juego
// (SimStop). Separado aca: la llama unicamente el path del click. Devuelve true si el punto cae sobre un
// boton de transporte (y ya hizo la accion): el click no sigue al menu. Mismo patron que el Timeline
// (Mover=resalta / Activar=dispara), que ya lo hacia bien.
// true si el boton es del TRANSPORTE del modo juego (Stop/Play): accion DIRECTA, sin desplegable.
static inline bool IsButtonTransport(const Button* b) {
    return b && (b->role == BR_GameStop || b->role == BR_GamePlay);
}

// true si 'vp' es un viewport 3D cuyo FOCO DE BARRA (barFocusIndex) apunta a un boton de transporte
// Stop/Play VISIBLE. Lo consultan: RenderBar (para NO apagar el foco por-frame, como ya hace con el
// Timeline), el container Symbian via W3dLayoutFocusTransport (para SOLTAR las flechas al dispatch del
// panel en vez de orbitar la camara) y el bloque kind==1 de LayoutKeyPanelActive. Sin esto el foco de
// Stop/Play se borraba cada frame y las flechas se las comia la orbita -> no se llegaba de Stop a Play.
bool LayoutFocusInTransport(ViewportBase* vp) {
    if (!vp || !vp->isLeaf() || vp->ViewportKind() != 1) return false;
    int fi = vp->barFocusIndex;
    std::vector<Button*>& B = vp->BarButtons;
    return (fi >= 0 && fi < (int)B.size() && B[fi]->visible && IsButtonTransport(B[fi]));
}

// true si 'vp' es el editor UV con el foco de barra sobre "Edit" (BRUV_Edit): accion DIRECTA sin
// desplegable, mismo esquema que el transporte Stop/Play del 3D. Lo consultan RenderBar (retener el
// foco por-frame), el bloque kind 4 de LayoutKeyPanelActive (OK activa / izq-der siguen / C suelta)
// y el container Symbian via W3dLayoutFocusUVEdit (soltar las flechas al panel en vez de panear).
bool LayoutFocusInUVEdit(ViewportBase* vp) {
    if (!vp || !vp->isLeaf() || vp->ViewportKind() != 4) return false;
    int fi = vp->barFocusIndex;
    std::vector<Button*>& B = vp->BarButtons;
    return (fi > 0 && fi < (int)B.size() && B[fi]->visible && B[fi]->role == BRUV_Edit);
}

static bool LayoutTransportBar3D(ViewportBase* vp, int mx, int my) {
    if (!vp || !vp->isLeaf() || vp->ViewportKind() != 1) return false;
    std::vector<Button*>& B = vp->BarButtons;
    Button* bs = BarRoleBtn(B, BR_GameStop);
    Button* bl = BarRoleBtn(B, BR_GamePlay);
    extern bool SimActive(); extern void SimStop();
    if (bs && bs->visible && bs->Contains(mx, my)) {
        Viewport3DActive = (Viewport3D*)vp;
        if (SimActive()) SimStop();       // restaura el estado inicial + descarga scripts
        PlayAnimation = false; g_redraw = true; return true;
    }
    if (bl && bl->visible && bl->Contains(mx, my)) {
        Viewport3DActive = (Viewport3D*)vp;
        if (PlayAnimation) { PlayAnimation = false; }   // pausa
        else {
            // MISMA PUERTA que el Play del timeline: no arrancar con texturas en la cola diferida
            // (el juego salia gris el primer segundo).
            extern bool GameWaitTextures();
            if (!GameWaitTextures()) { g_redraw = true; return true; }
            GamePrepareViewports(!SimActive());   // overlays off solo si el juego ARRANCA
            AnimPlayDir = 1; PlayAnimation = true;
        }
        g_redraw = true; return true;
    }
    return false;
}

bool LayoutOpenBarMenu(ViewportBase* vp, int mx, int my) {
    if (!vp || !vp->isLeaf()) return false;
    // el viewport 3D tiene su cadena propia (abajo); los demas se abren solos por el virtual compartido, asi no
    // hay que repetir aca el if de cada boton de cada panel
    if (vp->ViewportKind() != 1) return vp->OpenBarMenu(mx, my);
    std::vector<Button*>& B = vp->BarButtons;
    // el menu de la barra de ESTE viewport actua sobre ESTE viewport: sin esto,
    // tras abrir un proyecto en caliente Viewport3DActive podia quedar apuntando
    // al 3D del layout ANTERIOR y View > Viewpoint "no hacia nada"
    Viewport3DActive = (Viewport3D*)vp;
    // (el transporte Stop/Play NO va aca: es accion directa y solo se dispara por click real ->
    //  LayoutTransportBar3D, llamada desde el path del click. Ver el comentario de esa funcion.)

    PopupMenu* target = NULL;     // menu desplegable a abrir
    Button* button = NULL;        // su boton en la barra
    bool overlays  = false;       // el de overlays se abre distinto (flags)
    // botones por ROL (NO por indice): reordenar la barra (mover Orient, etc.) no rompe esto.
    Button* bMode = BarRoleBtn(B, BR_Mode);   Button* bSelM = BarRoleBtn(B, BR_SelMode);
    Button* bPiv  = BarRoleBtn(B, BR_Pivot);  Button* bSel  = BarRoleBtn(B, BR_Select);
    Button* bAdd  = BarRoleBtn(B, BR_Add);    Button* bObj  = BarRoleBtn(B, BR_Object);
    Button* bOvl  = BarRoleBtn(B, BR_Overlays); Button* bRnd = BarRoleBtn(B, BR_Render);
    Button* bOri  = BarRoleBtn(B, BR_Orient); Button* bUV   = BarRoleBtn(B, BR_UV);
    Button* bView = BarRoleBtn(B, BR_View);   Button* bMesh = BarRoleBtn(B, BR_Mesh);
    Button* bSnap = BarRoleBtn(B, BR_Snap);
    Button* bProp = BarRoleBtn(B, BR_Proportional);
    Button* bAnim = BarRoleBtn(B, BR_Animation);
    Button* bRoot = BarRoleBtn(B, BR_Root);
    Button* bTexG = BarRoleBtn(B, BR_TexSave);
    if (bTexG && bTexG->visible && bTexG->Contains(mx, my)) {   // pintura de textura: guardar lo pintado
        if (OpenMenu) OpenMenu->Close();
        LayoutOpenMenuUVSave(NULL, bTexG->sx, bTexG->sy + bTexG->height - GlobalScale);
        if (!OpenMenu) Notify(T("Texture Paint: the active part has no texture"), true);
        else RegisterMenuBar(OpenMenu, bTexG);
        return true;
    }
    if (bRoot && bRoot->visible && bRoot->Contains(mx, my)) {
        // la ESCENA / el PREFAB que se edita: el menu se REARMA cada vez (la lista cambia)
        static PopupMenu* menuRoot = NULL;
        if (!menuRoot) { menuRoot = new PopupMenu(); menuRoot->action = W3dRootsMenuAction; }
        if (OpenMenu == menuRoot && menuRoot->open) return true;   // ya abierto (el hover que se desliza)
        W3dRootsMenuBuild(menuRoot);
        target = menuRoot; button = bRoot;
    } else if (MenuMode && bMode && bMode->visible && bMode->Contains(mx, my)) {
        target = MenuMode; button = bMode;
        LayoutRebuildMenuMode();   // mode-aware: malla -> Paints ; armature -> Pose
        if (!MenuMode->action) MenuMode->action = LayoutActionMode;
    } else if (MenuSelMode && bSelM && bSelM->visible && bSelM->Contains(mx, my)) {
        target = MenuSelMode; button = bSelM;
        if (!MenuSelMode->action) MenuSelMode->action = LayoutActionSelMode;
    } else if (bPiv && bPiv->visible && bPiv->Contains(mx, my)) {
        // Pivot: el menu se REARMA cada vez (marca el activo) -> via LayoutMenuPivot
        if (OpenMenu) OpenMenu->Close();
        LayoutMenuPivot(bPiv->sx, bPiv->sy + bPiv->height - GlobalScale);
        RegisterMenuBar(OpenMenu, bPiv);
        return true;
    } else if (bSnap && bSnap->visible && bSnap->Contains(mx, my)) {
        // Snap: el menu se REARMA cada vez (Base/Target marcan el activo + labels) -> via LayoutMenuSnapTool
        if (OpenMenu) OpenMenu->Close();
        LayoutMenuSnapTool(bSnap->sx, bSnap->sy + bSnap->height - GlobalScale);
        RegisterMenuBar(OpenMenu, bSnap);
        return true;
    } else if (bProp && bProp->visible && bProp->Contains(mx, my)) {
        if (OpenMenu) OpenMenu->Close();
        LayoutProportionalMenu(bProp->sx, bProp->sy + bProp->height - GlobalScale);
        RegisterMenuBar(OpenMenu, bProp);
        return true;
    } else if (MenuView && bView && bView->visible && bView->Contains(mx, my)) {
        target = MenuView; button = bView;   // "View" (antes de Select): submenu Viewpoint
        if (!MenuView->action) MenuView->action = LayoutActionView;
        // refrescar el tilde de "Lock Orbit" con el estado del viewport activo (el menu se arma 1 sola vez)
        extern MenuItem* MenuItemLockOrbit;
        if (MenuItemLockOrbit && Viewport3DActive) MenuItemLockOrbit->green = Viewport3DActive->lockOrbit;
        extern MenuItem* MenuItemLockedCamera;
        if (MenuItemLockedCamera && Viewport3DActive) MenuItemLockedCamera->green = Viewport3DActive->CameraToView;
        extern MenuItem* MenuItemLocalView;
        if (MenuItemLocalView && Viewport3DActive) MenuItemLocalView->green = Viewport3DActive->localViewActive;
    } else if (MenuSelect && bSel && bSel->visible && bSel->Contains(mx, my)) {
        target = MenuSelect; button = bSel;
        LayoutRebuildMenuSelect();   // mode-aware: agrega Loop Select en Edit (cara/borde)
        if (!MenuSelect->action) MenuSelect->action = LayoutActionSelect;
    } else if (MenuAdd && bAdd && bAdd->visible && bAdd->Contains(mx, my)) {
        target = MenuAdd; button = bAdd;
        W3dPrefabMenuAddBuild(MenuPrefabsAdd);   // "Add > Prefab": los prefabs del proyecto de AHORA
        W3dSceneMenuAddBuild(MenuScenesAdd);     // "Add > Scene": las escenas de AHORA
    } else if (MenuMesh && bMesh && bMesh->visible && bMesh->Contains(mx, my)) {
        // Edit Mode: menu "Mesh" (Transform/Snap/Delete), comun a vertice/borde/cara.
        target = MenuMesh; button = bMesh;
        if (!MenuMesh->action) MenuMesh->action = LayoutActionMesh;
        LayoutCtxSelectMenu();   // (Vertex / Edge / Face > Select, segun el sub-modo de ahora)
    } else if (MenuAnimation && bAnim && bAnim->visible && bAnim->Contains(mx, my)) {
        // menu "Animation": keyframes del objeto + Motion Trail. Es su PROPIO boton de la barra -> rama propia
        // (estaba anidado adentro del Contains de "Object": pedia el cursor sobre los DOS botones a la vez y no
        //  se abria nunca).
        target = MenuAnimation; button = bAnim;
        if (!MenuAnimation->action) MenuAnimation->action = LayoutActionObject; // ids 510..514 / 520 / 530..542
        { extern void MenuAnimationBuild(); MenuAnimationBuild(); }   // segun el modo y el objeto activo
        LayoutSyncInsertKeySubmenu(MenuAnimation, 0); // "Insert Keyframe": submenu de canales segun el contexto
    } else if (bObj && bObj->visible && bObj->Contains(mx, my)) {
        // Edit Mode -> menu de contexto Vertex/Edge/Face (o "Armature" si se editan huesos);
        // Pose Mode -> menu "Pose"; Object Mode -> menu "Object".
        if (InteractionMode == EditMode) {
            if (OpenMenu) OpenMenu->Close();
            if (BoneEditActive()) LayoutMenuArmEdit(bObj->sx, bObj->sy + bObj->height - GlobalScale);
            else LayoutMenuEditContext(bObj->sx, bObj->sy + bObj->height - GlobalScale);
            RegisterMenuBar(OpenMenu, bObj);
            return true;
        } else if (InteractionMode == PoseMode) {
            extern PopupMenu* MenuPose;
            if (MenuPose){ target = MenuPose; button = bObj; if (!MenuPose->action) MenuPose->action = LayoutActionObject; // reusa ids 100/101/102/500
                           LayoutSyncInsertKeySubmenu(MenuPose, 0); } // "Insert Keyframe" -> submenu de canales
        } else if (MenuObject) {
            target = MenuObject; button = bObj;
            if (!MenuObject->action) MenuObject->action = LayoutActionObject;
        }
    } else if (MenuRender && bRnd && bRnd->visible && bRnd->Contains(mx, my)) {
        target = MenuRender; button = bRnd;
        if (!MenuRender->action) MenuRender->action = LayoutActionRender;
    } else if (MenuOrient && bOri && bOri->visible && bOri->Contains(mx, my)) {
        target = MenuOrient; button = bOri;
        if (!MenuOrient->action) MenuOrient->action = LayoutActionOrient;
    } else if (bUV && bUV->visible && bUV->Contains(mx, my)) {
        // UV (edit mode): menu Mark Seam + proyecciones
        if (OpenMenu) OpenMenu->Close();
        LayoutMenuUV(bUV->sx, bUV->sy + bUV->height - GlobalScale);
        return true;
    } else if (bOvl && bOvl->visible && bOvl->Contains(mx, my)) {
        overlays = true; button = bOvl;
    }
    if (!button) return false;

    // el desplegable toca el borde INFERIOR del boton (solapado 1px para que
    // su borde superior se funda con el del boton y no quede doble linea)
    int menuY = button->sy + button->height - GlobalScale;
    if (overlays) {
        // OJO: la 1ra vez MenuOverlays es NULL (se crea al abrirlo). Sin el
        // "MenuOverlays &&", NULL==NULL daba "ya abierto" y no abria nada.
        if (MenuOverlays && OpenMenu == MenuOverlays) return true; // ya abierto
        if (OpenMenu) OpenMenu->Close();
        static_cast<Viewport3D*>(vp)->OpenMenuOverlays(button->sx, menuY);
        RegisterMenuBar(MenuOverlays, button);
        return true;
    }
    if (OpenMenu == target) return true; // ese menu ya esta abierto
    if (OpenMenu) OpenMenu->Close();    // cerrar el otro (cambio de menu)
    target->Open(button->sx, menuY, MenuScreenW, MenuScreenH);
    OpenMenu = target;
    RegisterMenuBar(target, button);
    return true;
}

// abre/cierra la barra de menu del viewport ACTIVO (soft-izquierda en Symbian).
// Abre el PRIMER menu visible (saltea el icono de tipo, B[0]) SIN preseleccionar
// item (Abrir deja selectIndex=-1): asi izq/der cambian de menu y recien abajo
// se entra al desplegable. Si ya hay un menu abierto, lo cierra (toggle).
// EL SOFTKEY DERECHO (un toque): entra / sale del foco de la barra de HERRAMIENTAS (abajo) del viewport activo (la
// Vista 3D, el Timeline, el editor UV...). false = ese viewport no tiene barra de herramientas a la vista
bool LayoutToolbarFocusToggle() {
    ViewportBase* vp = viewPortActive;
    if (!vp || !vp->isLeaf()) return false;
    if (vp->toolFocus >= 0) { vp->toolFocus = -1; g_redraw = true; return true; }
    // UNA barra con foco a la vez: el softkey derecho SALTA desde la de arriba (y su menu, las pestanias de Propiedades,
    // el panel de curvas del timeline) a la de herramientas
    if (LayoutMenuOpen()) { if (OpenMenu) OpenMenu->Close(); }
    vp->barFocusIndex = -1;
    if (vp->ViewportKind() == 3) ((Properties*)vp)->focusInTabs = false;
    if (vp->ViewportKind() == 5) ((Timeline*)vp)->panelFocus = -1;
    return vp->ToolbarFocusMove(+1);
}
bool LayoutToolbarFocusActive() {
    ViewportBase* vp = viewPortActive;
    if (!vp || !vp->isLeaf() || vp->toolFocus < 0) return false;
    if (vp->ToolButtons.empty() || !vp->ToolbarVisible()) { vp->toolFocus = -1; return false; }
    return true;
}
void LayoutToolbarReleaseFocus() { if (viewPortActive && viewPortActive->toolFocus >= 0) { viewPortActive->toolFocus = -1; g_redraw = true; } }
// con el foco en la barra de abajo: izq/der recorren, OK aprieta, C o arriba salen
bool LayoutToolbarKey(int key) {
    if (!LayoutToolbarFocusActive()) return false;
    ViewportBase* vp = viewPortActive;
    switch (key) {
        case LayoutKey::Left:   vp->ToolbarFocusMove(-1); return true;
        case LayoutKey::Right:  vp->ToolbarFocusMove(+1); return true;
        case LayoutKey::Enter:  vp->ToolbarFocusActivate(); return true;
        case LayoutKey::Up:
        case LayoutKey::Cancel: vp->toolFocus = -1; g_redraw = true; return true;
        case LayoutKey::Down:   return true;
    }
    return false;
}

void LayoutToggleBarActiveViewport() {
    // UNA barra con foco a la vez: el softkey izquierdo SALTA desde la de herramientas (abajo) a la de arriba
    if (viewPortActive && viewPortActive->toolFocus >= 0) { viewPortActive->toolFocus = -1; g_redraw = true; }
    if (LayoutMenuOpen()) { if (OpenMenu) OpenMenu->Close(); return; }
    ViewportBase* vp = viewPortActive;
    if (!vp || !vp->isLeaf()) return;
    if (vp->ViewportKind() == 1) {
        // 3D: abre el primer menu visible (Select). Izquierda llega al [0].
        std::vector<Button*>& B = vp->BarButtons;
        for (int i = 1; i < (int)B.size(); i++) {
            if (B[i]->visible) {
                vp->barFocusIndex = i;     // la barra se auto-scrollea para centrarlo
                vp->UpdateBar();           // recalcula sx/sy YA con el scroll
                LayoutOpenBarMenu(vp, B[i]->sx + B[i]->width / 2, B[i]->sy + B[i]->height / 2);
                return;
            }
        }
    } else if (vp->ViewportKind() == 3) {
        // propiedades: foco en la PESTAÑA activa (te ahorra subir hasta arriba).
        // Desde ahi izq/der cambian de pestaña; izq en la 1ra llega al [0].
        ((Properties*)vp)->focusInTabs = true;
    } else if (vp->ViewportKind() == 5) {
        // Timeline: entra/sale del foco de la barra de TRANSPORTE (play/inicio/fin/Start/End/anim). Antes caia en
        // el else -> abria el menu de tipo/split, que no es lo que se quiere navegar.
        LayoutTimelineBarToggle();
    } else if (VpIsOutliner(vp)) {
        // Outliner: abre el SELECTOR DE VISTA ([1]: escena / mallas / materiales...). Izq/der recorren
        // la barra ([0] tipo de viewport, [2] acciones de la vista de recursos).
        if (!((Outliner*)vp)->OpenMenuButton(1)) LayoutOpenMenuType(vp);
    } else {
        // outliner (u otros sin menus de barra): abre el menu de tipo/split ([0])
        LayoutOpenMenuType(vp);
    }
}

// OUTLINER: izq/der con un menu de SU barra abierto ([0] tipo, [1] vista, [2] acciones) salta al
// boton vecino visible y abre su desplegable (el N95 recorre la barra sin mouse). false = el menu
// abierto no es de la barra del outliner activo.
static bool LayoutChangeMenuBarOutliner(int dir) {
    if (!viewPortActive || !viewPortActive->isLeaf() || !VpIsOutliner(viewPortActive)) return false;
    if (!LayoutMenuOpen()) return false;
    Outliner* out = (Outliner*)viewPortActive;
    int idx = out->OpenMenuButtonIndex();
    if (idx < 0 && OpenMenu == gMenuType && gMenuTypeOf == viewPortActive) idx = 0;
    if (idx < 0) return false;
    std::vector<Button*>& B = out->BarButtons;
    const int maxIdx = (int)B.size() - 1;
    for (int k = 0; k <= maxIdx; k++) {
        idx += dir;
        if (idx > maxIdx) idx = 0;
        if (idx < 0) idx = maxIdx;
        // (el interruptor lista/cuadricula no tiene menu: se saltea; sin mouse se alterna desde el menu de la vista)
        if (idx == 0 || (B[(size_t)idx] && B[(size_t)idx]->visible && B[(size_t)idx] != out->btnGrid)) break;
    }
    if (idx == 0) { LayoutOpenMenuType(out); return true; }
    out->OpenMenuButton(idx);
    return true;
}

// flechas izq/der con un menu de barra abierto: salta al boton de menu vecino
// (Select/Add/Object/Overlays) salteando los ocultos, y abre su desplegable
static void LayoutChangeMenuBar(int dir) {
    if (LayoutChangeMenuBarOutliner(dir)) return;   // la barra del outliner (vista / acciones)
    // el menu de tipo/split de un panel NO-3D (outliner/propiedades) no tiene
    // menus hermanos para ciclar: izq/der no hacen nada ahi. (Solo si ese menu esta REALMENTE abierto: con
    // foco de transporte OpenMenu puede quedar apuntando a un gMenuType ya cerrado -> no cortar la nav.)
    if (LayoutMenuOpen() && OpenMenu == gMenuType && gMenuTypeOf != (ViewportBase*)Viewport3DActive) return;
    Viewport3D* vp = Viewport3DActive;
    if (!vp || vp->BarButtons.size() < 2) return;
    std::vector<Button*>& B = vp->BarButtons;
    const int maxIdx = (int)B.size() - 1; // rango DINAMICO (no hardcodear: el ultimo
                                          // boton -Orient/etc.- quedaba afuera y se salteaba)
    // mapeo MENU->ROL (estable) y role->INDICE via BarRoleIdx (dinamico) -> reordenar no rompe la nav.
    int idx = -1;
    // idx del boton "actual": SOLO si hay un menu de barra REALMENTE abierto. OpenMenu puede seguir apuntando
    // a un menu YA cerrado (Close() baja ->open pero NO NULea el puntero), asi que sin el LayoutMenuOpen()
    // de guarda, tras cerrar sobre Stop la nav recalculaba idx desde el menu viejo (Render) y oscilaba
    // Render<->Stop sin llegar a Play. El rol lo registro el que ABRIO el menu (RegisterMenuBar).
    if (LayoutMenuOpen() && OpenMenu == gMenuType) idx = 0; // boton [0] = tipo/split del viewport
    else if (LayoutMenuOpen() && OpenMenu == gMenuOpenBar && gMenuBarRole >= 0) idx = BarRoleIdx(B, gMenuBarRole);
    // SIN menu de barra abierto pero CON foco (transporte Stop/Play, ver abajo): ciclar DESDE el foco actual.
    // Solo si el 3D activo (vp==Viewport3DActive) ES el viewport enfocado: esta funcion tambien corre para
    // menus de barra de paneles NO-3D, y ahi no hay que desviar la nav al barFocusIndex del 3D (cross-viewport).
    if (idx < 0 && (ViewportBase*)vp == viewPortActive
        && vp->barFocusIndex >= 0 && vp->barFocusIndex <= maxIdx) idx = vp->barFocusIndex;
    if (idx < 0) return; // ni menu ni foco -> nada que ciclar
    // el [0] (tipo/split) SIEMPRE es navegable; el resto salta los ocultos
    for (int k = 0; k <= maxIdx; k++) {
        idx += dir;
        if (idx > maxIdx) idx = 0;
        if (idx < 0) idx = maxIdx;
        if (idx == 0 || B[idx]->visible) break;
    }
    if (idx == 0) { LayoutOpenMenuType(vp); return; } // [0]: abre tipo/split
    Button* b = B[idx];
    vp->barFocusIndex = idx;   // la barra se auto-scrollea para centrar el nuevo
    vp->UpdateBar();           // recalcula sx/sy YA con el scroll antes del hit-test
    // Stop/Play son ACCION DIRECTA (sin desplegable): al aterrizar en ellos cerramos el menu que estuviera
    // abierto (Render, etc.) y dejamos SOLO el foco -> el bloque kind 1 de LayoutKeyPanelActive rutea OK a
    // LayoutTransportBar3D. Antes LayoutOpenBarMenu no tenia rama de transporte -> devolvia false, el
    // menu de Render quedaba abierto y de Stop no se llegaba a Play.
    if (IsButtonTransport(b)) { if (OpenMenu) OpenMenu->Close(); g_redraw = true; return; }
    LayoutOpenBarMenu(vp, b->sx + b->width / 2, b->sy + b->height / 2);
}

// MISMO comportamiento que LayoutChangeMenuBar pero para el EDITOR UV (su barra tiene menus
// PROPIOS: Modo/View/SelMode/Pivot/Snap/Texture/Animation). Antes la nav izq/der estaba atada a
// Viewport3DActive -> en el UV solo se llegaba al [0] (tipo) y no se podia ir a View/Pivot/Snap
// Mapea por ROL (BarRoleUV), como el 3D: el menu abierto dice el rol, BarRoleIdx da el
// indice actual -> agregar/reordenar botones no rompe la navegacion por teclado.
static void LayoutChangeMenuBarUV(int dir) {
    if (!viewPortActive || !viewPortActive->isLeaf() || viewPortActive->ViewportKind() != 4) return;
    UVEditor* uv = (UVEditor*)viewPortActive;
    std::vector<Button*>& B = uv->BarButtons;
    if (B.size() < 2) return;
    const int maxIdx = (int)B.size() - 1;
    // menu abierto -> ROL del boton que lo abre -> indice ACTUAL de ese boton. Guarda LayoutMenuOpen():
    // Close() NO nulea OpenMenu, y tras aterrizar en "Edit" (que cierra el menu) el puntero queda stale
    // -> sin la guarda, la nav recalculaba idx desde el menu viejo y oscilaba (mismo bug que el Stop/Play del 3D).
    int idx = -1;
    if      (LayoutMenuOpen() && OpenMenu == gMenuType) idx = 0;
    else if (LayoutMenuOpen()) {
        int role = -1;
        if      (OpenMenu == gMenuUVMode)    role = BRUV_Mode;
        else if (OpenMenu == gMenuUVAdd)     role = BRUV_Add;
        else if (OpenMenu == gMenuUVArm)     role = BRUV_Armature;
        else if (OpenMenu == gMenuUV)        role = BRUV_View;
        else if (OpenMenu == gMenuUVSelect)  role = BRUV_Select;
        else if (OpenMenu == gMenuUVSelMode) role = BRUV_SelMode;
        else if (OpenMenu == gMenuPivot)     role = BRUV_Pivot;
        else if (OpenMenu == gMenuUVSnap)    role = BRUV_Snap;
        else if (OpenMenu == gMenuUVTex)     role = BRUV_Texture;
        else if (OpenMenu == gMenuUVMaps)   role = BRUV_UVMap;
        else if (OpenMenu == gMenuUVMip)     role = BRUV_Mipmap;
        else if (OpenMenu == gMenuUVSave) role = BRUV_Save;
        else if (OpenMenu == gMenuUVAnim)    role = BRUV_Animation;
        if (role >= 0) idx = BarRoleIdx(B, role);
    }
    // SIN menu abierto pero CON foco (el boton "Edit", accion directa): ciclar DESDE el foco actual
    if (idx < 0 && LayoutFocusInUVEdit(uv) && uv == viewPortActive) idx = uv->barFocusIndex;
    if (idx < 0) return;
    for (int k = 0; k <= maxIdx; k++) {       // avanza saltando los ocultos ([0] siempre navegable)
        idx += dir;
        if (idx > maxIdx) idx = 0;
        if (idx < 0) idx = maxIdx;
        if (idx == 0 || B[idx]->visible) break;
    }
    if (idx == 0) { LayoutOpenMenuType(uv); return; }
    uv->barFocusIndex = idx;
    uv->UpdateBar();
    Button* b = B[idx];
    int mx = b->sx, my = b->sy + b->height - GlobalScale;
    switch (b->role) {
        case BRUV_Mode:      LayoutOpenMenuUVMode(uv, mx, my);    break;
        case BRUV_Add:       LayoutOpenMenuUVAdd(uv, mx, my);     break;
        case BRUV_Armature:  LayoutOpenMenuUVArmature(uv, mx, my); break;
        case BRUV_View:      LayoutOpenMenuUV(uv, mx, my);        break;
        case BRUV_Select:    LayoutOpenMenuUVSelect(uv, mx, my);  break;
        case BRUV_SelMode:   LayoutOpenMenuUVSelMode(uv, mx, my); break;
        case BRUV_Pivot:     LayoutMenuPivot(mx, my);              break;
        case BRUV_Snap:      LayoutOpenMenuUVSnap(uv, mx, my);    break;
        case BRUV_UVMap:     LayoutOpenMenuUVMaps(uv, mx, my);   break;
        case BRUV_Texture:   LayoutOpenMenuUVTex(uv, mx, my);     break;
        case BRUV_Mipmap:    LayoutOpenMenuUVMip(uv, mx, my);     break;
        // "Edit" es ACCION DIRECTA (sin desplegable), como Stop/Play del 3D: al aterrizar se cierra el
        // menu que estuviera abierto y queda SOLO el foco -> OK la dispara (bloque kind 4 del dispatch).
        // Antes NO tenia case: la nav moria aca (nada se abria y el proximo izq/der arrancaba del menu
        // stale) -> "editar" y "guardar" eran INALCANZABLES con el keypad del N95.
        case BRUV_Edit:    if (OpenMenu) OpenMenu->Close(); g_redraw = true; break;
        case BRUV_Save:   LayoutOpenMenuUVSave(uv, mx, my);  break;
        case BRUV_Animation: LayoutOpenMenuUVAnim(uv, mx, my);    break;
    }
}


// ====================================================================
// menu DELETE de Edit Mode (X / Backspace): borra vertices/aristas/caras.
// Sale CERCA DEL CURSOR (no en el objeto) para que sea usable con el mouse.
// ====================================================================
static PopupMenu* gMenuDelete = NULL;

// 361 Vertices, 362 Edges, 363 Faces, 364 Edge Loops (ver Mesh::DeleteSelectionEdit/DeleteEdgeLoopEdit). Ids UNICOS
// (no 0/1/2/3) porque el submenu "Delete" de los menus de contexto despacha por LayoutActionObject, no por esta.
static void ActionDelete(int aId) {
    if (state != editNavigation) return;
    if (InteractionMode != EditMode || !g_editMesh) return;
    if (aId == 364) { // Edge Loops: disuelve el loop seleccionado (inverso del loop cut)
        if (!((Mesh*)g_editMesh)->DeleteEdgeLoopEdit()) Notify(T("Delete Edge Loops: select an edge shared by two faces"), true);
        g_redraw = true; return;
    }
    int dt = (aId == 361) ? SelVertex : (aId == 362) ? SelEdge : SelFace;
    ((Mesh*)g_editMesh)->DeleteSelectionEdit(dt);
    g_redraw = true;
}

// crea (1 vez) el menu Delete. Lo comparten el atajo X (LayoutDeleteEdit, lo abre EN EL CURSOR) y el SUBMENU
// "Delete" (flecha a la derecha) al fondo de los menus de contexto Vertex/Edge/Face. Misma instancia para los dos.
static void EnsureMenuDelete() {
    if (gMenuDelete) return;
    gMenuDelete = new PopupMenu();
    gMenuDelete->title = T("Delete");
    // OJO: action = LayoutActionObject (NO ActionDelete). Como submenu, sus items se despachan por la accion del menu
    // TOP (el de contexto = LayoutActionObject); usar la misma accion + ids unicos 361-364 hace que funcione IGUAL
    // sea como submenu (menu de contexto) o standalone (atajo X, donde este ES el menu top).
    gMenuDelete->action = LayoutActionObject;
    gMenuDelete->Add(T("Vertices"), 361);
    gMenuDelete->Add(T("Edges"), 362);
    gMenuDelete->Add(T("Faces"), 363);
    gMenuDelete->Add(T("Edge Loops"), 364); // disuelve el loop (inverso del loop cut)
}

// abre el menu Delete EN EL CURSOR (atajo X) si estamos en Edit Mode. Devuelve true si lo abrio
// (el caller NO debe borrar objetos). En Object Mode devuelve false.
bool LayoutDeleteEdit(int mx, int my) {
    if (state != editNavigation) return false;
    if (InteractionMode != EditMode || !g_editMesh) return false;
    if (CurveBezEditing()) { CurveMenuOpen(CurveMenuDelete(), mx, my); return true; }   // curva: Vertices / Segments
    EnsureMenuDelete();
    gMenuDelete->Open(mx, my, MenuScreenW, MenuScreenH);
    OpenMenu = gMenuDelete;
    return true;
}

// APPLY (Ctrl+A, Object Mode): abre el menu Apply (Location/Rotation/Scale/All) EN EL CURSOR. Comparte MenuApply
// (submenu de Object, construido en ViewPort3D.cpp) -> sus items 220-223 despachan por LayoutActionObject sea
// como submenu (menu top = Object) o standalone (menu top = MenuApply, por eso le seteamos la action aca).
void LayoutApplyMenu(int mx, int my) {
    if (state != editNavigation || InteractionMode != ObjectMode || !MenuApply) return;
    if (!MenuApply->action) MenuApply->action = LayoutActionObject;
    MenuApply->Open(mx, my, MenuScreenW, MenuScreenH);
    OpenMenu = MenuApply;
}

// menu de CONTEXTO del EDIT MODE de ARMATURE (Fase 3): Extrude / Duplicate / Delete / Set Parent
// + el submenu Clear Parent (Alt+P: Disconnect Bone / Clear Parent). Se abre desde el boton
// "Armature" de la barra (el rol BR_Object en este modo). Ids 600-607.
static PopupMenu* gMenuArmEdit = NULL;
void LayoutMenuArmEdit(int mx, int my) {
    if (!BoneEditActive()) return;
    if (!gMenuArmEdit) {
        gMenuArmEdit = new PopupMenu();
        gMenuArmEdit->title  = T("Armature");
        gMenuArmEdit->action = LayoutActionObject;
        gMenuArmEdit->Add(T("Extrude Bone"), 600)->shortcut = "@3d.extrude";
        gMenuArmEdit->Add(T("Move"), 604)->shortcut = "@3d.move";
        gMenuArmEdit->Add(T("Rotate"), 605)->shortcut = "@3d.rotate";
        gMenuArmEdit->Add(T("Scale"), 606)->shortcut = "@3d.scale";
        gMenuArmEdit->Add(T("Duplicate"), 601)->shortcut = "@3d.duplicate";
        gMenuArmEdit->Add(T("Delete"), 602)->shortcut = "@3d.delete";
        // Set Parent y Clear Parent: submenus de 2 opciones cada uno (Keep Offset/Connected y
        // Disconnect Bone/Clear Parent); despachan por SU action (Parent.cpp), no por los 600s
        gMenuArmEdit->Add(T("Set Parent"), 607, -1, LayoutSubmenuBoneCtrlP())->shortcut = "@global.parent";
        gMenuArmEdit->Add(T("Clear Parent"), 608, -1, LayoutSubmenuBoneAltP())->shortcut = "@global.clearParent";
    }
    BoneAltPContext3D(); // el submenu Clear Parent opera sobre los huesos 3D en edicion
    gMenuArmEdit->Open(mx, my, MenuScreenW, MenuScreenH);
    OpenMenu = gMenuArmEdit;
}

// MENU CONTEXTUAL del viewport 3D (click DERECHO en el contenido). Abre EN EL MOUSE el menu
// que corresponda al modo -- exactamente el mismo que abre el boton de la barra, no una copia:
//   Object Mode -> "Object"  (solo con algo seleccionado: sin seleccion no hay nada que hacerle)
//   Edit Mode   -> el contexto de la MALLA segun el sub-modo (Vertex / Edge / Face), o el de
//                  huesos si se esta editando un armature
//   Pose Mode   -> "Pose"
// el menu CONTEXTUAL lleva arriba DESHACER y REHACER (las flechas curvas). El menu de base es el MISMO que abre el
// boton de la barra: para no meterle esas dos opciones al de la barra, el contextual es una COPIA (cada opcion con su
// id, su accion, su submenu y su atajo: elegirla hace exactamente lo mismo) con las dos flechas arriba
static PopupMenu* g_menuCtxWithUndo = NULL;
static bool g_ctxHasUndo = false, g_ctxHasRedo = false;
static void CtxUndo() { extern void RebindMaterialMeshPart(); UndoUndo(); RebindMaterialMeshPart(); g_redraw = true; }
static void CtxRedo()  { extern void RebindMaterialMeshPart(); UndoRedo();  RebindMaterialMeshPart(); g_redraw = true; }
static void ContextWithUndo(int mx, int my) {
    PopupMenu* base = LayoutMenuOpen() ? OpenMenu : NULL;
    if (!base) return;
    if (!g_menuCtxWithUndo) g_menuCtxWithUndo = new PopupMenu();
    PopupMenu* c = g_menuCtxWithUndo;
    if (base == c) return;
    c->Clear();
    c->title = base->title; c->action = base->action;
    g_ctxHasUndo = UndoHasSomething(); g_ctxHasRedo = UndoHasRedo();
    const int idUndo = 900001, idRedo = 900002;   // (no chocan con los de ningun menu)
    MenuItem* u = c->Add(T("Undo"), idUndo, (int)IconType::undo); u->action = CtxUndo; u->shortcut = "@global.undo"; u->gray = &g_ctxHasUndo;
    MenuItem* r = c->Add(T("Redo"), idRedo, (int)IconType::redo);  r->action = CtxRedo;  r->shortcut = "@global.redo";  r->gray = &g_ctxHasRedo;
    for (size_t i = 0; i < base->items.size(); i++) {
        const MenuItem* o = base->items[i];
        MenuItem* n = c->Add(o->text, o->id, o->icon, o->submenu);
        n->checkbox = o->checkbox; n->gray = o->gray; n->action = o->action; n->shortcut = o->shortcut; n->green = o->green;
        n->valueFloat = o->valueFloat; n->vmin = o->vmin; n->vmax = o->vmax;
        n->lineUp = o->lineUp || i == 0;   // (una linea entre las flechas y el menu de siempre)
    }
    base->Close();
    c->Resize();
    c->Open(mx, my, MenuScreenW, MenuScreenH);
    OpenMenu = c;
}
// ============================================================================
//  EL MENU CONTEXTUAL POR TECLADO (el OK MANTENIDO del N95, como mantener apretado el dedo): el del elemento que tiene
//  el foco en el viewport activo -la fila de Propiedades, la fila del Outliner o de la Biblioteca, los keyframes del
//  Timeline, el objeto activo en la Vista 3D-. LayoutMenuContextKeyboardPossible dice si el OK que se acaba de apretar
//  PUEDE terminar en un menu (si no, el OK hace lo de siempre en el acto: no hay por que esperar)
// ============================================================================
bool LayoutMenuContextKeyboardPossible() {
    if (LayoutMenuOpen() || PopUpActive || g_activeTextField) return false;
    if (!viewPortActive || !viewPortActive->isLeaf()) return false;
    const int k = viewPortActive->ViewportKind();
    if (k == 3) { Properties* p = (Properties*)viewPortActive; return !p->focusInTabs && (!p->editing || p->ListKeyboard(NULL, NULL)); }
    if (VpIsOutliner(viewPortActive)) { Outliner* o = (Outliner*)viewPortActive; return !o->ModeMove() && !o->movingRec; }
    if (k == 5) return true;
    if (k == 1) {
        Viewport3D* v = (Viewport3D*)viewPortActive;
        // (el foco en la barra del 3D: el OK aprieta ese boton)
        return state == editNavigation && v->barFocusIndex < 0 && (ActiveObj || InteractionMode == EditMode);
    }
    return false;
}
bool LayoutMenuContextKeyboard() {
    if (!LayoutMenuContextKeyboardPossible()) return false;
    const int k = viewPortActive->ViewportKind();
    if (k == 3) return ((Properties*)viewPortActive)->MenuContextKeyboard();
    if (VpIsOutliner(viewPortActive)) return ((Outliner*)viewPortActive)->MenuContextKeyboard();
    if (k == 5) return ((Timeline*)viewPortActive)->MenuContextKeyboard();
    if (k == 1) {
        // en el objeto activo (su origen en la pantalla); si no se ve, en el medio de la vista
        Viewport3D* v = (Viewport3D*)viewPortActive;
        int mx = v->x + v->width / 2, my = v->y + v->height / 2;
        if (ActiveObj) {
            Matrix4 W; ActiveObj->GetWorldMatrix(W);
            float sx = 0, sy = 0;
            if (v->ProjectPoint(Vector3(W.m[12], W.m[13], W.m[14]), sx, sy) && sx >= 0 && sy >= 0 && sx < v->width && sy < v->height) {
                mx = v->x + (int)sx; my = v->y + (int)sy;
            }
        }
        LayoutMenuContext3D(mx, my);
        return LayoutMenuOpen();
    }
    return false;
}

// el MISMO menu en un punto (el OK mantenido con el MOUSE VIRTUAL del N95 quieto: como el click derecho de la PC): el
// del viewport que esta abajo (Vista 3D, Timeline, Propiedades, Outliner / Biblioteca)
bool LayoutMenuContextIn(int mx, int my) {
    if (LayoutMenuOpen() || PopUpActive) return false;
    ViewportBase* v = FindViewportUnderMouse(rootViewport, mx, my);
    if (!v || !v->isLeaf() || v->OnBar(mx, my)) return false;
    const int k = v->ViewportKind();
    if (k == 1) {
        if (v->OnToolbar(mx, my) || v->OnBrushBar(mx, my) || state != editNavigation) return false;
        LayoutMenuContext3D(mx, my);
        return LayoutMenuOpen();
    }
    if (k == 5) return ((Timeline*)v)->MenuContext(mx, my);
    if (k == 3) return ((Properties*)v)->MenuContext(mx, my);
    if (VpIsOutliner(v)) return ((Outliner*)v)->MenuContext(mx, my);
    return false;
}

void LayoutMenuContext3D(int mx, int my) {
    if (OpenMenu) OpenMenu->Close();
    if (InteractionMode == EditMode) {
        if (BoneEditActive()) LayoutMenuArmEdit(mx, my);
        else                  LayoutMenuEditContext(mx, my);
        ContextWithUndo(mx, my);
        return;
    }
    PopupMenu* m = NULL;
    if (InteractionMode == PoseMode) {
        extern PopupMenu* MenuPose;
        m = MenuPose;
        if (m) { if (!m->action) m->action = LayoutActionObject; LayoutSyncInsertKeySubmenu(m, 0); }
    } else {
        if (!ActiveObj) return;      // Object Mode sin seleccion: no se abre nada
        m = MenuObject;
        if (m && !m->action) m->action = LayoutActionObject;
    }
    if (!m) return;
    m->Resize();
    m->Open(mx, my, MenuScreenW, MenuScreenH);
    OpenMenu = m;
    ContextWithUndo(mx, my);
    g_redraw = true;
}

// menus de CONTEXTO de Edit Mode (Vertex / Edge / Face), separados (no submenus de
// "Mesh"). Se abren con W o el boton de barra, EN (mx,my). Comparten LayoutActionObject.
// (gMenuVertex/Edge/Face se declaran mas arriba: los usan dispatch + nav.)
// los menus Vertex / Edge / Face: los abre W en el cursor (segun el sub-modo) y son submenus de "Mesh" (los tres)
PopupMenu* LayoutSubmenuEditFace() {
    if (!gMenuFace) {
        gMenuFace = new PopupMenu(); gMenuFace->title = T("Face"); gMenuFace->action = LayoutActionObject;
        gMenuFace->Add(T("Select"), 0, -1, LayoutCtxSelectMenu());   // (loop, vinculado, camino... ver LayoutCtxSelectMenu)
        gMenuFace->Add(T("Extrude Faces"), 300)->shortcut = "@3d.extrude";
        gMenuFace->Add(T("Inset Faces"), 344)->shortcut = "I";   // (texto: la I del 3D es Insert Keyframe en la tabla)
        gMenuFace->Add(T("Bevel"), 345)->shortcut = "Ctrl+B";      // (los bordes de las caras elegidas)
        gMenuFace->Add(T("Loop Cut and Slide"), 340)->shortcut = "@3d.loopcut";
        gMenuFace->Add(T("Rip"), 341)->shortcut = "@3d.rip";
        gMenuFace->Add(T("Shade Smooth"), 320);
        gMenuFace->Add(T("Shade Flat"), 321);
        gMenuFace->Add(T("Recalculate Normals"), 322);
        gMenuFace->Add(T("Triangulate Faces"), 323)->shortcut = "@3d.triangulate";
        // (Delete se movio al menu "Mesh": es comun a vertice/borde/cara)
    }
    return gMenuFace;
}
PopupMenu* LayoutSubmenuEditEdge() {
    if (!gMenuEdge) {
        gMenuEdge = new PopupMenu(); gMenuEdge->title = T("Edge"); gMenuEdge->action = LayoutActionObject;
        gMenuEdge->Add(T("Select"), 0, -1, LayoutCtxSelectMenu());
        gMenuEdge->Add(T("Extrude Edges"), 300)->shortcut = "@3d.extrude";
        gMenuEdge->Add(T("Loop Cut and Slide"), 340)->shortcut = "@3d.loopcut";
        gMenuEdge->Add(T("Bevel Edges"), 345)->shortcut = "Ctrl+B";
        gMenuEdge->Add(T("Rip"), 341)->shortcut = "@3d.rip";
        gMenuEdge->Add(T("Mark Sharp"), 330);
        gMenuEdge->Add(T("Clear Sharp"), 331);
        gMenuEdge->Add(T("Mark Seam"), 358);   // costuras del unwrap (tambien en el menu U)
        gMenuEdge->Add(T("Clear Seam"), 359);
        // (Delete se movio al menu "Mesh": es comun a vertice/borde/cara)
    }
    return gMenuEdge;
}
PopupMenu* LayoutSubmenuEditVertex() {
    if (!gMenuVertex) {
        gMenuVertex = new PopupMenu(); gMenuVertex->title = T("Vertex"); gMenuVertex->action = LayoutActionObject;
        gMenuVertex->Add(T("Select"), 0, -1, LayoutCtxSelectMenu());
        gMenuVertex->Add(T("New Edge/Face from Vertices"), 310)->shortcut = "@3d.face";
        gMenuVertex->Add(T("Connect Vertex Path"), 342)->shortcut = "@3d.connect";
        gMenuVertex->Add(T("Extrude Vertices"), 300)->shortcut = "@3d.extrude";
        gMenuVertex->Add(T("Bevel Vertices"), 346)->shortcut = "Ctrl+Shift+B";
        gMenuVertex->Add(T("Slide Vertices"), 343)->shortcut = "@3d.slide";
        gMenuVertex->Add(T("Rip"), 341)->shortcut = "@3d.rip";
        // (Delete se movio al menu "Mesh": es comun a vertice/borde/cara)
    }
    return gMenuVertex;
}
void LayoutMenuEditContext(int mx, int my) {
    if (InteractionMode != EditMode) return;
    if (gCurveInEdit) { LayoutMenuCurveEdit(mx, my); return; }   // una curva: el menu Curve (puntos y manijas)
    PopupMenu* m = (EditSelectMode == SelFace) ? LayoutSubmenuEditFace()
                 : (EditSelectMode == SelEdge) ? LayoutSubmenuEditEdge() : LayoutSubmenuEditVertex();
    LayoutCtxSelectMenu();   // (el submenu Select, segun el sub-modo de ahora)
    if (OpenMenu) OpenMenu->Close();
    m->Open(mx, my, MenuScreenW, MenuScreenH);
    OpenMenu = m;
}

// menu ADD en el cursor (Shift+A en Object Mode): es el MISMO MenuAdd de la barra,
// abierto donde esta el mouse (como Blender). Reusa LayoutAccionAdd.
void LayoutMenuAdd(int mx, int my) {
    if (!MenuAdd) return; // se crea en el setup de menus (1er frame)
    if (OpenMenu) OpenMenu->Close();
    W3dPrefabMenuAddBuild(MenuPrefabsAdd);   // (los prefabs del proyecto de AHORA)
    W3dSceneMenuAddBuild(MenuScenesAdd);
    MenuAdd->Open(mx, my, MenuScreenW, MenuScreenH); // EN EL CURSOR
    OpenMenu = MenuAdd;
}

// menu SHARP en el cursor (tecla W en Edit Mode): elegir si los bordes seleccionados son
// afilados (Mark Sharp) o suaves (Clear Sharp). Reusa el dispatch LayoutActionObject (330/331).
static PopupMenu* gMenuSharp = NULL;
void LayoutMenuSharp(int mx, int my) {
    if (InteractionMode != EditMode || !g_editMesh) return;
    if (gCurveInEdit) { LayoutMenuCurveEdit(mx, my); return; }   // (una curva no tiene bordes que afilar: su menu)
    if (!gMenuSharp) {
        gMenuSharp = new PopupMenu(); gMenuSharp->title = T("Edge"); gMenuSharp->action = LayoutActionObject;
        gMenuSharp->Add(T("Mark Sharp"), 330);
        gMenuSharp->Add(T("Clear Sharp"), 331);
        gMenuSharp->Add(T("Mark Seam"), 358);
        gMenuSharp->Add(T("Clear Seam"), 359);
    }
    if (OpenMenu) OpenMenu->Close();
    gMenuSharp->Open(mx, my, MenuScreenW, MenuScreenH);
    OpenMenu = gMenuSharp;
}

// ====================================================================
// menu TRANSFORM PIVOT POINT (objeto + edit): desde donde/como rotan-escalan.
// 4 modos (Bloquear normales es de la malla: Propiedades > Malla 3D > Edicion). Sale en el cursor.
// (gMenuPivot se declara mas arriba, junto al dispatch de la barra.)
// ====================================================================
static void ActionPivot(int aId) {
    if (aId >= 0 && aId <= 3) g_transformPivot = aId; // 0..3 = enum TransformPivot
}

// ===== menu SNAP (boton "Snap" de la barra): Enable + Snap Base + Snap Target + Affect + Target Selection =====
static PopupMenu* gMenuSnapBase=NULL, *gMenuSnapTarget=NULL, *gMenuSnapIndiv=NULL; // gMenuSnapTool: declarado arriba
static void ActionSnapBase(int id){ g_snap.base=id; g_redraw=true; }
static void ActionSnapTarget(int id){ g_snap.target=id; g_redraw=true; }
// El dispatch (LayoutClickUI) SIEMPRE llama al action del menu TOP con el id del item elegido,
// aunque el item venga de un submenu. Por eso el id de Base/Target se rutea aca por RANGO
// (100+base, 200+target); los checkbox del top usan id 0 y su toggle lo hace el propio item.
static void ActionSnapRouter(int id){
    if (id >= 200) ActionSnapTarget(id - 200);
    else if (id >= 100) ActionSnapBase(id - 100);
}
static const char* SnapBaseName(int b){ return b==SNAP_CLOSEST?"Closest":b==SNAP_CENTER?"Center":b==SNAP_MEDIAN?"Median":"Active"; }
static const char* SnapTargetName(int t){ return t==SNAP_VERTEX?"Vertex":t==SNAP_EDGE?"Edge":t==SNAP_FACE?"Face":t==SNAP_EDGECENTER?"Edge Center":"Face Center"; }
// ===== menu PROPORTIONAL EDITING (el icono de la curva de la barra / tecla O) =====
static PopupMenu* gMenuProp = NULL;
void LayoutProportionalMenu(int mx, int my){
    if (!gMenuProp){ gMenuProp = new PopupMenu(); gMenuProp->title = T("Proportional Editing"); gMenuProp->action = LayoutActionObject; }
    gMenuProp->Clear();
    gMenuProp->AddCheck(T("Enable"), 0, &g_prop.on)->shortcut = "@3d.proportional";
    // el resto en GRIS con el proportional apagado (como el menu Snap)
    gMenuProp->AddCheck(T("Connected Only"), 0, &g_prop.connected)->gray = &g_prop.on;
    for (int t = 0; t < PropTypes; t++){
        MenuItem* it = gMenuProp->Add(T(ProportionalTypeName(t)), 430 + t, ProportionalIconType(t));
        it->green = (g_prop.type == t); it->gray = &g_prop.on;
    }
    gMenuProp->AddFloat(T("Proportional Size"), 0, &g_prop.radius, 0.01f, 20.0f)->gray = &g_prop.on;
    if (OpenMenu) OpenMenu->Close();
    gMenuProp->Open(mx, my, MenuScreenW, MenuScreenH);
    OpenMenu = gMenuProp;
}

void LayoutMenuSnapTool(int mx, int my){
    if (!gMenuSnapBase){ gMenuSnapBase=new PopupMenu(); gMenuSnapBase->title=T("Snap Base"); }
    gMenuSnapBase->Clear();
    for (int b=SNAP_CLOSEST;b<=SNAP_ACTIVE;b++) gMenuSnapBase->Add(T(SnapBaseName(b)), 100+b)->green=(g_snap.base==b);
    if (!gMenuSnapTarget){ gMenuSnapTarget=new PopupMenu(); gMenuSnapTarget->title=T("Snap Target"); }
    gMenuSnapTarget->Clear();
    for (int t=SNAP_VERTEX;t<=SNAP_FACECENTER;t++) gMenuSnapTarget->Add(T(SnapTargetName(t)), 200+t)->green=(g_snap.target==t);
    // sin titulo: el boton "Snap" de la barra ya lo dice; una cabecera "Snap" gastaria una fila (240p Symbian)
    // REGLA DE DISENO de los titulos: un menu que se abre desde algo SIN TEXTO (un icono, o un atajo de teclado)
    // lleva titulo -- es lo unico que te dice que estas mirando. Si lo abre un boton/item que YA decia el texto, NO
    // lleva: repetirlo es ruido. El boton Snap ahora es un iman -> titulo.
    if (!gMenuSnapTool){ gMenuSnapTool=new PopupMenu(); gMenuSnapTool->title=T("Snap"); gMenuSnapTool->action=ActionSnapRouter; }
    gMenuSnapTool->Clear();
    gMenuSnapTool->AddCheck(T("Enable"), 0, &g_snap.enabled)->shortcut="Shift Tab";
    // el resto se ve en GRIS cuando el snap esta apagado (->gray = &g_snap.enabled): sin snap
    // ninguna de estas opciones hace efecto, igual que "Show Overlays" grisa sus hijos.
    gMenuSnapTool->Add(std::string(T("Snap Base")) + ": " + T(SnapBaseName(g_snap.base)), 0, -1, gMenuSnapBase)->gray = &g_snap.enabled;
    gMenuSnapTool->Add(std::string(T("Snap Target")) + ": " + T(SnapTargetName(g_snap.target)), 0, -1, gMenuSnapTarget)->gray = &g_snap.enabled;
    // SOLO en target FACE: proyeccion POR VERTICE (retopologia). Submenu con 2 tildes INDEPENDIENTES (no radio):
    // Face Project (a lo largo del rayo de la vista) y Face Nearest (al punto mas cercano de la superficie).
    if (g_snap.target == SNAP_FACE){
        if (!gMenuSnapIndiv){ gMenuSnapIndiv=new PopupMenu(); gMenuSnapIndiv->title=T("Individual Elements"); }
        gMenuSnapIndiv->Clear();
        gMenuSnapIndiv->AddCheck(T("Face Project"), 0, &g_snap.faceProject);
        gMenuSnapIndiv->AddCheck(T("Face Nearest"), 0, &g_snap.faceNearest);
        gMenuSnapTool->Add(T("Snap Target for Individual Elements"), 0, -1, gMenuSnapIndiv)->gray = &g_snap.enabled;
    }
    gMenuSnapTool->AddCheck(T("Affect Move"),   0, &g_snap.afMove)->gray = &g_snap.enabled;
    gMenuSnapTool->AddCheck(T("Affect Rotate"), 0, &g_snap.afRot)->gray = &g_snap.enabled;
    gMenuSnapTool->AddCheck(T("Affect Scale"),  0, &g_snap.afScale)->gray = &g_snap.enabled;
    gMenuSnapTool->AddCheck(T("Include Active"),     0, &g_snap.tsActive)->gray = &g_snap.enabled;
    gMenuSnapTool->AddCheck(T("Include Edited"),     0, &g_snap.tsEdited)->gray = &g_snap.enabled;
    gMenuSnapTool->AddCheck(T("Include Non-Edited"), 0, &g_snap.tsNonEdited)->gray = &g_snap.enabled;
    if (OpenMenu) OpenMenu->Close();
    gMenuSnapTool->Open(mx, my, MenuScreenW, MenuScreenH);
    OpenMenu = gMenuSnapTool;
}

void LayoutMenuPivot(int mx, int my) {
    if (!gMenuPivot) {
        gMenuPivot = new PopupMenu();
        gMenuPivot->title  = T("Transform Pivot Point");
        gMenuPivot->action = ActionPivot;
    }
    // se rearma cada vez: cada opcion con su ICONO y la ACTIVA en verde
    gMenuPivot->Clear();
    gMenuPivot->Add(T("3D Cursor"), PivotCursor3D, IconType::pivotCursor)->green = (g_transformPivot==PivotCursor3D);
    gMenuPivot->Add(T("Individual Origins"), PivotIndividual, IconType::pivotIndividual)->green = (g_transformPivot==PivotIndividual);
    gMenuPivot->Add(T("Median Point"), PivotMedian, IconType::pivotMedian)->green = (g_transformPivot==PivotMedian);
    gMenuPivot->Add(T("Active Element"), PivotActive, IconType::pivotActive)->green = (g_transformPivot==PivotActive);
    gMenuPivot->Open(mx, my, MenuScreenW, MenuScreenH); // EN EL CURSOR
    OpenMenu = gMenuPivot;
}

// VARIANTE 2D del menu de pivote (la usa el editor UV). MISMO estado global (g_transformPivot) y
// MISMA accion que el 3D -> no hay dos verdades ni codigo duplicado; lo unico que cambia es QUE se
// ofrece, porque en 2D dos de los items no significan nada:
//   - "Active Element": el editor UV no tiene ELEMENTO ACTIVO (el pick escribe uvSelVert y nada
//     mas). Elegirlo pivoteaba igual en la mediana y el icono de la barra mentia.
// "Individual Origins" SI se ofrece: en modo CARA el UV lo implementa de verdad (cada cara rota
// alrededor de su centro, ver UVEditor::StartXform).
void LayoutMenuPivotUV(int mx, int my) {
    static PopupMenu* gMenuPivotUV = NULL;
    if (!gMenuPivotUV) {
        gMenuPivotUV = new PopupMenu();
        gMenuPivotUV->title  = T("Transform Pivot Point");
        gMenuPivotUV->action = ActionPivot;
    }
    gMenuPivotUV->Clear();
    // el UV llama "3D Cursor" al cursor 2D a proposito: es el MISMO item y el mismo concepto que
    // en el 3D (el pivote es el cursor del viewport), y renombrarlo rompia el paralelo.
    gMenuPivotUV->Add(T("3D Cursor"), PivotCursor3D, IconType::pivotCursor)->green = (g_transformPivot==PivotCursor3D);
    gMenuPivotUV->Add(T("Individual Origins"), PivotIndividual, IconType::pivotIndividual)->green = (g_transformPivot==PivotIndividual);
    gMenuPivotUV->Add(T("Median Point"), PivotMedian, IconType::pivotMedian)->green =
        (g_transformPivot==PivotMedian || g_transformPivot==PivotActive); // Active cae en la mediana
    if (OpenMenu && OpenMenu != gMenuPivotUV) OpenMenu->Close();
    gMenuPivotUV->Open(mx, my, MenuScreenW, MenuScreenH);
    OpenMenu = gMenuPivotUV;
}

// ====================================================================
//  TRANSFORM de sub-elementos en EDIT MODE (G/R/S sobre verts/aristas/caras)
// ====================================================================
// Mueve/rota/escala los VERTICES seleccionados de la malla EN EDICION (no el
// objeto). Vive en el EDITOR: reusa el estado y la matematica del transform de
// objetos (estado/axisSelect/transformOrientation/cam/pivot + AxisOriented) pero
// el TARGET son los vertices. Trabaja en MUNDO (TRS global de la malla, constante
// durante el drag): snapshot de las posiciones de mundo al empezar, acumula la
// transform y reescribe desde el snapshot (sin drift). Al CONFIRMAR recalcula
// bordes + normales (salvo Bloquear normales); al CANCELAR restaura el snapshot.

struct EditVtxSnap {
    int editK;      // indice del vertice editable; su pos[] es autoritativa (un valor por
                    // posicion; los GPU duplicados los empuja PushPositions via posRep)
    Vector3 world0; // posicion de MUNDO al empezar
    Vector3 worldNormal; // normal del vertice en MUNDO (para Shrink/Fatten: cada vert se mueve por SU normal)
};
static std::vector<EditVtxSnap> gEVsnap;
// PROPORTIONAL EDITING: los vertices NO seleccionados que vienen arrastrados, con su distancia a lo seleccionado
// (medida UNA vez al arrancar el transform) y el peso que les toca con el radio actual (se re-evalua si el
// radio cambia con la rueda o la barra). Escribirlos es reescribir desde world0, como con la seleccion.
struct EditVtxProp { int editK; Vector3 world0; Vector3 worldNormal; float dist; float weight; };
static std::vector<EditVtxProp> gEVprop;
static void EVWrite();
static Mesh*      gEVmesh   = NULL;
static Quaternion gEVrg;            // rotacion global de la malla (EFECTIVA, constante en el drag)
static Vector3    gEVsg(1,1,1);     // escala global
static Vector3    gEVorigin;        // origen global (EFECTIVO, ver EditXformSnapshot)
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
static bool       gEVshrinkRequest = false; // (lo prende LayoutShrinkFatten ANTES del snapshot: calcula las normales por vertice)
bool EditShrinkActive(){ return gEVshrink; }
// VERTEX SLIDE (Shift+V, o G-G): el move deja de trasladar y DESLIZA cada vertice por UNA de sus
// aristas. t=0 lo deja donde estaba, t=1 lo deja encima del vecino. El riel se elige con la direccion
// del primer arrastre. Confirmar entra por EditXformConfirm, o sea que el Auto Merge (si esta
// prendido) suelda los verts encimados y MergeVertsEdit borra las caras que colapsan.
static bool  gEVslide = false;
static std::vector<std::vector<Vector3> > gEVslideCand; // vecinos por arista (posicion de MUNDO al empezar)
static std::vector<Vector3> gEVslideDest;               // el vecino ELEGIDO, por entrada de gEVsnap
static float gEVslideT = 0.0f;                          // 0..1
static float gEVslideAccX = 0.0f, gEVslideAccY = 0.0f;  // arrastre acumulado, en pixeles de pantalla
static int   gEVslideIdx = 0;                           // riel elegido con las flechas (Symbian / sin mouse)
static bool  gEVslideKeyboard = false;                  // manda el teclado: el mouse no re-elige hasta que se mueva
bool EditSlideActive(){ return gEVslide; }
float EditSlideFactor(){ return gEVslideT; }
// EXTRUDE / orientacion NORMAL: la translacion se constriñe a gTransformNormal (la normal en mundo).
// gEVuseCustom + gTransformNormal son GLOBALES (variables.h) para que CiclarEje/AxisOriented los vean.

// hay algun constraint en la cadena (el objeto o cualquier ancestro)? Sin ninguno la transform
// efectiva y la base son LA MISMA CUENTA (Objects.h), asi que se entra por el camino de siempre
// -RotGlobalOf/GetGlobalPositionBase- y una escena sin constraints (el 99%) da los mismos floats
// que antes de que esto existiera, sin pasar por la descomposicion de la matriz.
static bool EVHasConstraints(const Object* o){
    // por el predicado del Core y no por constraints.empty(): "tiene uno" no es "hace algo". Con el
    // "ver en modo edicion" apagado -que es el caso normal MIENTRAS SE EDITA- el objeto no tiene
    // ningun constraint efectivo, y entonces el G/R/S entra por el camino de siempre y da los mismos
    // floats que una escena sin constraints.
    for (const Object* p = o; p; p = p->Parent) if (W3dObjHasConstraintEffective(p)) return true;
    return false;
}

static Vector3 EVLocalToWorld(const Vector3& p){
    return gEVorigin + gEVrg * Vector3(gEVsg.x*p.x, gEVsg.y*p.y, gEVsg.z*p.z);
}
static Vector3 EVWorldToLocal(const Vector3& w){
    Vector3 d = gEVrg.Inverted() * (w - gEVorigin);
    return Vector3(gEVsg.x!=0.0f?d.x/gEVsg.x:d.x,
                   gEVsg.y!=0.0f?d.y/gEVsg.y:d.y,
                   gEVsg.z!=0.0f?d.z/gEVsg.z:d.z);
}

bool EditXformActive(){ return gEVmesh != NULL; }
// valores acumulados para la barra de estado (la rotacion usa gAngleTransform)
Vector3 EditXformTransDelta(){ return gEVtrans; }       // translacion de MUNDO (engine xyz)
float   EditXformScaleFactor(){ return 1.0f + gEVscaleAmt; }
float   EditXformShrinkAmt(){ return gEVscaleAmt; }     // distancia por la normal (Shrink/Fatten)

static void EVProportionalWeights(){
    for (size_t i = 0; i < gEVprop.size(); i++) gEVprop[i].weight = ProportionalWeight(gEVprop[i].dist, gEVprop[i].editK);
}
// mide la distancia de cada vertice NO seleccionado a lo seleccionado: euclidea al mas cercano, o por las ARISTAS
// (Connected Only: caminos mas cortos desde todos los seleccionados a la vez, con largos de mundo; una isla
// suelta queda fuera). Se hace una sola vez por transform: en el N95 no hay tiempo de medir por frame.
static void EVProportionalCapture(Mesh* m, EditMesh* e, const std::vector<char>& selEdit){
    gEVprop.clear();
    if (!g_prop.on) return;
    const size_t N = e->editVerts.size();
    std::vector<Vector3> W(N);
    for (size_t k = 0; k < N; k++) W[k] = EVLocalToWorld(Vector3(e->pos[k*3], e->pos[k*3+1], e->pos[k*3+2]));
    std::vector<float> dist(N, 1e30f);
    if (g_prop.connected){
        std::vector<std::vector<int> > ady(N);
        for (size_t eg = 0; eg + 1 < e->lineIdx.size(); eg += 2){
            const int a = e->lineIdx[eg], b = e->lineIdx[eg+1];
            if (a < 0 || b < 0 || (size_t)a >= N || (size_t)b >= N) continue;
            ady[(size_t)a].push_back(b); ady[(size_t)b].push_back(a);
        }
        std::priority_queue<std::pair<float,int>, std::vector<std::pair<float,int> >, std::greater<std::pair<float,int> > > queue;
        for (size_t k = 0; k < N; k++) if (selEdit[k]){ dist[k] = 0.0f; queue.push(std::make_pair(0.0f, (int)k)); }
        while (!queue.empty()){
            const float d = queue.top().first; const int k = queue.top().second; queue.pop();
            if (d > dist[(size_t)k]) continue;
            const std::vector<int>& v = ady[(size_t)k];
            for (size_t j = 0; j < v.size(); j++){
                const int nb = v[j]; const float nd = d + (W[(size_t)nb] - W[(size_t)k]).Length();
                if (nd < dist[(size_t)nb]){ dist[(size_t)nb] = nd; queue.push(std::make_pair(nd, nb)); }
            }
        }
    } else {
        std::vector<int> sel; for (size_t k = 0; k < N; k++) if (selEdit[k]) sel.push_back((int)k);
        for (size_t k = 0; k < N; k++){
            if (selEdit[k]) continue;
            float best = 1e30f;
            for (size_t s = 0; s < sel.size(); s++){ const Vector3 d = W[k] - W[(size_t)sel[s]]; const float dd = d.x*d.x + d.y*d.y + d.z*d.z; if (dd < best) best = dd; }
            dist[k] = sqrtf(best);
        }
    }
    const int nV = m->vertexSize;
    for (size_t k = 0; k < N; k++){
        if (selEdit[k] || dist[k] >= 1e29f) continue;
        EditVtxProp p; p.editK = (int)k; p.world0 = W[k]; p.dist = dist[k]; p.weight = 0.0f;
        const int rep = e->editVerts[k];
        Vector3 ln = (m->normals && rep >= 0 && rep < nV) ? Vector3(m->normals[rep*3]/127.0f, m->normals[rep*3+1]/127.0f, m->normals[rep*3+2]/127.0f) : Vector3(0,1,0);
        p.worldNormal = (gEVrg * ln).Normalized();
        gEVprop.push_back(p);
    }
    EVProportionalWeights();
}
void ProportionalEditXformUpdate(){ if (!gEVmesh) return; EVProportionalWeights(); EVWrite(); }

void EditXformSnapshot(){
    g_xformFirstMov = true; // el primer motion arranca en cero (no usa el delta viejo)
    gEVsnap.clear(); gEVprop.clear(); gEVmesh = NULL;
    gEVshrink = false; // por defecto es un transform comun; el starter de Shrink/Fatten lo prende despues
    ClipMirrorReset(); // nuevo transform: ningun vert esta "pegado" al plano del mirror todavia
    if (InteractionMode != EditMode || !g_editMesh) return;
    Mesh* m = (Mesh*)g_editMesh; m->EnsureEdit();
    if (!m->edit || !m->vertex) return;
    EditMesh* e = m->edit;
    const int nV = m->vertexSize;
    const bool hasRep = ((int)m->posRep.size() == nV);

    // que VERTICES editables estan seleccionados segun el MODO (vertex/edge/face)
    std::vector<char> selEdit(e->editVerts.size(), 0);
    if (EditSelectMode == SelVertex){
        for (size_t k=0;k<e->vertSel.size();k++) if (e->vertSel[k]) selEdit[k]=1;
    } else if (EditSelectMode == SelEdge){
        for (size_t eg=0; eg<e->edgeSel.size(); eg++) if (e->edgeSel[eg]){
            if (eg*2+1 >= e->lineIdx.size()) break; // lineIdx desincronizado (defensivo, como SnapNeedleActive)
            selEdit[e->lineIdx[eg*2]]=1; selEdit[e->lineIdx[eg*2+1]]=1;
        }
    } else {
        for (size_t f=0; f<e->faces.size(); f++) if (f<e->faceSel.size() && e->faceSel[f])
            for (size_t c=0;c<e->faces[f].size();c++) selEdit[e->faces[f][c]]=1;
    }

    gEVmesh = m;
    // ------------------------------------------------------------------
    // el trio rotacion/escala/origen es la ida y vuelta mundo<->local de EVLocalToWorld/EVWorldToLocal,
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
    gEVsg = ScaleGlobalOf(m); // la escala NUNCA la toca un constraint: base y efectiva son la misma cuenta
    const Vector3 originBase = m->GetGlobalPositionBase();
    Quaternion rgEf; Vector3 originEf; bool hasEf = false;
    if (EVHasConstraints(m)){
        // la matriz efectiva es la de la ULTIMA vista bindeada y el G/R/S arranca por TECLADO, sin
        // dibujar nada en el medio: con dos viewports abiertos esa puede ser la del OTRO. Se bindea el
        // viewport donde esta el usuario, igual que BoneGrabStartEx (BoneEdit.cpp) y PoseW2N.
        if (Viewport3DActive) Viewport3DActive->BindView();
        Matrix4 W; m->GetWorldMatrix(W);
        hasEf = W3dMatrixQuat(W, rgEf); // false = base espejada o chata: no hay rotacion que sacar
        if (hasEf) originEf = Vector3(W.m[12], W.m[13], W.m[14]); // la MISMA W: la efectiva no es pura
    }
    if (hasEf){ gEVrg = rgEf; gEVorigin = originEf; }
    else      { gEVrg = RotGlobalOf(m); gEVorigin = originBase; }
    gEVoffCons = gEVorigin - originBase;

    // grupo de GPU verts por POSICION de cada editable seleccionado (mover uno mueve
    // a todos sus duplicados de UV/normales en el mismo lugar)
    Vector3 nNormAccum(0,0,0); // suma de las normales de los verts seleccionados (orientacion Normal)
    // NORMAL DE CADA VERTICE EDITABLE para Shrink/Fatten: el promedio de las caras que lo usan, pesado por el ANGULO
    // que cada cara abre en ese vertice. NO la normal de render: en una malla con bordes duros (un cubo) cada copia del
    // vertice tiene la de UNA cara, y cada esquina salia para un lado distinto (se abrian las intersecciones). Con el
    // peso por angulo, inflar un cubo o una esfera da lo mismo que escalarlos, aunque las caras esten trianguladas.
    std::vector<float> vnEd;
    if (gEVshrinkRequest) {
        const size_t nE = e->editVerts.size();
        vnEd.assign(nE * 3, 0.0f);
        for (size_t f = 0; f < e->faces.size(); f++) {
            const IntRows::Row F = e->faces[f];
            const size_t n = F.size(); if (n < 3) continue;
            float nx = 0, ny = 0, nz = 0;   // Newell (sirve para poligonos de n lados)
            for (size_t j = 0; j < n; j++) {
                const float* p0 = &e->pos[(size_t)F[j] * 3]; const float* p1 = &e->pos[(size_t)F[(j + 1) % n] * 3];
                nx += (p0[1] - p1[1]) * (p0[2] + p1[2]); ny += (p0[2] - p1[2]) * (p0[0] + p1[0]); nz += (p0[0] - p1[0]) * (p0[1] + p1[1]);
            }
            const float l = sqrtf(nx*nx + ny*ny + nz*nz); if (l < 1e-12f) continue;
            nx /= l; ny /= l; nz /= l;
            for (size_t j = 0; j < n; j++) {
                const int v = F[j]; if (v < 0 || (size_t)v >= nE) continue;
                const float* pc = &e->pos[(size_t)v * 3];
                const float* pa = &e->pos[(size_t)F[(j + n - 1) % n] * 3]; const float* pb = &e->pos[(size_t)F[(j + 1) % n] * 3];
                float ax = pa[0]-pc[0], ay = pa[1]-pc[1], az = pa[2]-pc[2], bx = pb[0]-pc[0], by = pb[1]-pc[1], bz = pb[2]-pc[2];
                const float la = sqrtf(ax*ax + ay*ay + az*az), lb = sqrtf(bx*bx + by*by + bz*bz);
                if (la < 1e-12f || lb < 1e-12f) continue;
                float c = (ax*bx + ay*by + az*bz) / (la * lb); if (c > 1.0f) c = 1.0f; if (c < -1.0f) c = -1.0f;
                const float ang = acosf(c);
                vnEd[(size_t)v*3] += nx * ang; vnEd[(size_t)v*3+1] += ny * ang; vnEd[(size_t)v*3+2] += nz * ang;
            }
        }
    }
    for (size_t k=0;k<e->editVerts.size();k++){
        if (!selEdit[k]) continue;
        int rep = e->editVerts[k]; if (rep<0||rep>=nV) continue;
        EditVtxSnap s; s.editK=(int)k;
        // posicion EDITABLE (autoritativa) -> mundo. No lee el render (vertex[]).
        Vector3 l0(e->pos[k*3], e->pos[k*3+1], e->pos[k*3+2]);
        s.world0 = EVLocalToWorld(l0);
        // normal del vertice en MUNDO (para Shrink/Fatten). rotacion global (la escala no cambia el sentido).
        Vector3 ln = m->normals ? Vector3(m->normals[rep*3]/127.0f, m->normals[rep*3+1]/127.0f, m->normals[rep*3+2]/127.0f) : Vector3(0,1,0);
        Vector3 lnS = ln;   // Shrink/Fatten: la del vertice (todas sus caras), si tiene caras
        if (!vnEd.empty()) { Vector3 v(vnEd[k*3], vnEd[k*3+1], vnEd[k*3+2]); if (v.x*v.x + v.y*v.y + v.z*v.z > 1e-12f) lnS = v.Normalized(); }
        s.worldNormal = (gEVrg * lnS).Normalized();
        gEVsnap.push_back(s);
        if (m->normals) nNormAccum = nNormAccum + ln;
    }
    if (gEVsnap.empty()){ gEVmesh = NULL; return; } // nada seleccionado
    EVProportionalCapture(m, e, selEdit); // proportional editing: los no seleccionados que vienen arrastrados

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
        if (e->SelectionCenter(cx,cy,cz)) gEVpivot = EVLocalToWorld(Vector3(cx,cy,cz));
        else gEVpivot = gEVorigin;
    }
    TransformPivotPoint = gEVpivot; // el gizmo (linea punteada + ejes) se dibuja aca
    gEVtrans = Vector3(0,0,0); gEVrotTotal = Quaternion(1,0,0,0); gEVscaleAmt = 0;
    gAngleTransform = 0.0f;
    // orientacion NORMAL (menu): el move se constrine a la normal de la seleccion (mismo path
    // que el extrude). El extrude la pisa con su propia normal en EditXformStartExtrude.
    gEVuseCustom = false;
    if (transformOrientation == NormalOrient) {
        Vector3 nw = gEVrg * nNormAccum; // a mundo (rotacion global de la malla)
        float ln = sqrtf(nw.x*nw.x + nw.y*nw.y + nw.z*nw.z);
        if (ln > 1e-6f) { gTransformNormal = Vector3(nw.x/ln, nw.y/ln, nw.z/ln); gEVuseCustom = true; }
    }
}

// arranca el MOVE del extrude: la tapa ya esta seleccionada (ExtruirCarasEdit la
// dejo asi); se mueve constreñida a la normal promedio (en MUNDO).
void EditXformStartExtrude(const Vector3& normalLocal){
    state = translation;
    EditXformSnapshot(); gEVprop.clear(); // (sin proportional: la base coincide con la tapa recien extruida y se iria con ella) // snapshot de la tapa seleccionada
    if (!EditXformActive()){ state = editNavigation; return; }
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
static bool SnapFaceProjectPoint(const Vector3& p, int mode, Vector3& out){
    if (!SceneCollection || !Viewport3DActive) return false;
    Viewport3DActive->BindView();   // el rayo sale de ESTA vista: ver SnapFindTarget
    std::vector<Mesh*> meshes; SnapCollect(SceneCollection, meshes);
    if (meshes.empty()) return false;
    Mesh* em = (InteractionMode==EditMode) ? (Mesh*)g_editMesh : NULL;
    Vector3 cam = Viewport3DActive->viewPos;
    Vector3 rd = p - cam; float rl=sqrtf(rd.Dot(rd)); if (rl<1e-9f) return false; rd=rd*(1.0f/rl);
    bool found=false; float bestT=1e30f, bestD=1e30f; Vector3 best;
    for (size_t mi=0; mi<meshes.size(); mi++){
        Mesh* m=meshes[mi]; if (m==em) continue; // no proyectar sobre la propia malla en edicion (retopo -> otra geometria)
        if (!m->vertex || m->vertexSize<=0) continue;
        Matrix4 W; m->GetWorldMatrix(W);
        // las caras de la malla, o las COMPARTIDAS de su recurso (edicion pendiente, MeshResource.h)
        const std::vector<MeshFace>* pF = W3dMeshFacesView(m);
        if (!pF) continue;
        const std::vector<MeshFace>& F = *pF;
        for (size_t f=0; f<F.size(); f++){
            const std::vector<int>& idx=F[f].idx; int nc=(int)idx.size(); if (nc<3) continue;
            { bool range=true; for (int k=0;k<nc;k++) if (idx[k]<0||idx[k]>=m->vertexSize){range=false;break;}
              if (!range) continue; }   // faces3d desincronizado con vertex[] (defensivo)
            Vector3 w0=W*Vector3(m->vertex[idx[0]*3],m->vertex[idx[0]*3+1],m->vertex[idx[0]*3+2]);
            for (int t=1; t+1<nc; t++){
                Vector3 w1=W*Vector3(m->vertex[idx[t]*3],  m->vertex[idx[t]*3+1],  m->vertex[idx[t]*3+2]);
                Vector3 w2=W*Vector3(m->vertex[idx[t+1]*3],m->vertex[idx[t+1]*3+1],m->vertex[idx[t+1]*3+2]);
                if (mode==0){ float tt; if (SnapRayTri(cam,rd,w0,w1,w2,tt) && tt<bestT){ bestT=tt; best=cam+rd*tt; found=true; } }
                else { Vector3 c=SnapClosestTri(p,w0,w1,w2); Vector3 d=c-p; float dd=d.Dot(d); if (dd<bestD){ bestD=dd; best=c; found=true; } }
            }
        }
    }
    if (found) out=best;
    return found;
}
// proyeccion POR VERTICE activa? Solo en target FACE, Edit Mode, move LIBRE (desde la vista, sin eje ni extrude).
static bool SnapFaceIndividualActive(){
    return g_snap.enabled && g_snap.afMove && g_snap.target==SNAP_FACE &&
           (g_snap.faceProject || g_snap.faceNearest) &&
           InteractionMode==EditMode && gEVmesh && !gEVuseCustom && axisSelect==ViewAxis;
}
// posicion final del vert 'wn' (ya trasladado) con Face Project/Nearest. Project (rayo) tiene prioridad; si no pega,
// prueba Nearest; si tampoco, queda donde estaba (move normal).
static Vector3 SnapFaceIndividualPoint(const Vector3& wn){
    Vector3 pr;
    if (g_snap.faceProject && SnapFaceProjectPoint(wn, 0, pr)) return pr;
    if (g_snap.faceNearest && SnapFaceProjectPoint(wn, 1, pr)) return pr;
    return wn;
}

// recomputa cada vertice desde su world0 + el acumulado activo y lo escribe en la
// malla (todos los GPU del grupo) + refresca el overlay.
// escala DIRECCIONAL de un offset al pivote segun el eje/plano (orientacion); amt = factor - 1
static Vector3 EVScaleOff(Mesh* m, const Vector3& off, float amt){
    if (axisSelect==X||axisSelect==Y||axisSelect==Z){ Vector3 a = AxisOriented(*m, axisSelect); return off + a*(off.Dot(a)*amt); }
    if (axisSelect==PlaneX||axisSelect==PlaneY||axisSelect==PlaneZ){
        int ex=(axisSelect==PlaneX)?X:(axisSelect==PlaneY)?Y:Z;
        Vector3 a=AxisOriented(*m, ex); Vector3 inPlane = off - a*off.Dot(a);
        return off + inPlane*amt;
    }
    return off*(1.0f + amt); // libre: uniforme
}

static void EVWrite(){
    if (!gEVmesh) return;
    Mesh* m = gEVmesh;
    const bool faceIndiv = (state == translation) && SnapFaceIndividualActive(); // proyeccion por-vertice (retopo)
    for (size_t i=0;i<gEVsnap.size();i++){
        EditVtxSnap& s = gEVsnap[i];
        Vector3 wn;
        if (gEVslide){   // deslizar por el riel: interpola entre donde estaba y el vecino elegido
            wn = (i < gEVslideDest.size()) ? s.world0 + (gEVslideDest[i] - s.world0) * gEVslideT : s.world0;
        } else if (state == translation){
            wn = s.world0 + gEVtrans;
            if (faceIndiv) wn = SnapFaceIndividualPoint(wn); // cada vert se pega a la superficie de atras (o queda igual)
        } else if (state == rotation){
            wn = gEVpivot + gEVrotTotal * (s.world0 - gEVpivot);
        } else if (gEVshrink){ // SHRINK/FATTEN: cada vert se mueve por SU normal (mundo) * distancia acumulada
            wn = s.world0 + s.worldNormal * gEVscaleAmt;
        } else { // EditScale: escala DIRECCIONAL segun el eje/plano (orientacion)
            wn = gEVpivot + EVScaleOff(m, s.world0 - gEVpivot, gEVscaleAmt);
        }
        Vector3 ln = EVWorldToLocal(wn);
        // escribe la posicion EDITABLE (autoritativa); NO toca vertex[] a mano
        int k = s.editK;
        if (m->edit && k>=0 && k*3+2 < (int)m->edit->pos.size()){
            m->edit->pos[k*3]=ln.x; m->edit->pos[k*3+1]=ln.y; m->edit->pos[k*3+2]=ln.z;
        }
    }
    // PROPORTIONAL EDITING: los no seleccionados reciben el MISMO transform a su peso (peso 0 o apagado = vuelven
    // a su lugar). El deslizar por arista (slide) no arrastra a nadie.
    if (!gEVprop.empty() && !gEVslide){
        const bool on = g_prop.on;
        for (size_t i = 0; i < gEVprop.size(); i++){
            const EditVtxProp& p = gEVprop[i];
            const float w = on ? p.weight : 0.0f;
            Vector3 wn = p.world0;
            if (w > 0.0f){
                if (state == translation) wn = p.world0 + gEVtrans * w;
                else if (state == rotation) wn = gEVpivot + Quaternion::Slerp(Quaternion(), gEVrotTotal, w) * (p.world0 - gEVpivot);
                else if (gEVshrink) wn = p.world0 + p.worldNormal * (gEVscaleAmt * w);
                else wn = gEVpivot + EVScaleOff(m, p.world0 - gEVpivot, gEVscaleAmt * w);
            }
            Vector3 ln = EVWorldToLocal(wn);
            const int k = p.editK;
            if (m->edit && k >= 0 && k*3+2 < (int)m->edit->pos.size()){
                m->edit->pos[k*3]=ln.x; m->edit->pos[k*3+1]=ln.y; m->edit->pos[k*3+2]=ln.z;
            }
        }
    }
    // CLIPPING (Mirror con clipping ON): impide que los verts CRUCEN el plano al moverlos (half-space). Pasa la
    // pos LOCAL inicial de cada vert (world0 -> local) para saber de que lado arranco. No-op si ningun Mirror clippea.
    if (m->edit && !m->modifiers.empty()){
        std::vector<int> editKs; editKs.reserve(gEVsnap.size());
        std::vector<Vector3> startLocal; startLocal.reserve(gEVsnap.size());
        for (size_t i=0;i<gEVsnap.size();i++){ editKs.push_back(gEVsnap[i].editK); startLocal.push_back(EVWorldToLocal(gEVsnap[i].world0)); }
        for (size_t i=0;i<gEVprop.size();i++){ editKs.push_back(gEVprop[i].editK); startLocal.push_back(EVWorldToLocal(gEVprop[i].world0)); }
        m->ClipMirrorVerts(editKs, startLocal);
    }
    // edicion IN-PLACE solo de POSICIONES (rapido, tiempo real): pos[] -> render + overlay,
    // sin re-merge ni re-copiar atributos (que no cambian al mover). No rehace topologia.
    if (m->edit){ m->edit->PushPositions(); m->edit->RefreshOverlay(); }
    if (!m->modifiers.empty()) m->GenerateMeshModified(); // preview de modificadores en tiempo real (barato si no hay / si edit-display OFF)
}

void EditXformRestart(){ // cambio de eje (X/Y/Z): restaura al snapshot
    if (!gEVmesh) return;
    gEVtrans = Vector3(0,0,0); gEVrotTotal = Quaternion(1,0,0,0); gEVscaleAmt = 0;
    EVWrite();
}

// cursor actual (coords de pantalla) durante un transform -> lo usa el snap para buscar el target bajo el mouse.
int g_snapCurX = 0, g_snapCurY = 0;

// EJE BLOQUEADO + target BORDE: cuanto hay que avanzar desde B por el eje 'a' (unitario) para TOCAR el segmento
// [E0,E1] -> el punto de la recta (B + s*a) mas cercano al segmento (interseccion de bordes en la vista). Devuelve
// false si el segmento es degenerado o el eje es paralelo al borde (no hay toque util -> se usa el fallback viejo).
static bool SnapAxisTouchesSegment(const Vector3& B, const Vector3& a, const Vector3& E0, const Vector3& E1, float& outS){
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

static bool SnapNeedleActive(Vector3& out); // definida mas abajo (la usa el Snap Base = Active)

// ajusta gEVtrans para PEGAR la seleccion al target de snap (base Closest). Constreñido al mismo eje/plano que el
// move; libre = offset completo (proyeccion a la cara = retopologia). Setea g_snapHit + pos para el recuadro verde.
// LOS DOS LADOS DE LA COMPARACION VIVEN EN EL MISMO ESPACIO, y no es casualidad: SnapFindTarget devuelve un punto
// del mundo EFECTIVO (proyecta a pantalla lo que se DIBUJA) y los world0 de gEVsnap tambien salen de la efectiva
// (ver EditXformSnapshot). Con world0 en base el iman se veia enganchar en un lado y el vertice quedaba en otro.
bool g_snapNoCursor = false;   // un salto del iman con las FLECHAS (SnapStepArrow): el del cursor no lo pisa
static void SnapFitEditTrans(){
    if (g_snapNoCursor) return;
    g_snapHit = false;
    if (!g_snap.enabled || !g_snap.afMove || !Viewport3DActive || InteractionMode!=EditMode || !gEVmesh) return;
    Vector3 T; float sx=0, sy=0;
    Vector3 eA, eB; // extremos del borde target (si target==SNAP_EDGE): para el snap con eje bloqueado
    if (!SnapFindTarget(g_snapCurX, g_snapCurY, Viewport3DActive, T, sx, sy, &eA, &eB)) return;
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
        // NO editVerts -> lo resuelve SnapNeedleActive (igual que el snap de rotacion). Antes se
        // comparaba editK==activeIdx, que solo era correcto en modo Vertice.
        Vector3 B0;
        if (SnapNeedleActive(B0)){ B = B0 + gEVtrans; any = true; }
    }
    if (!any){ // MEDIAN (y fallback de Active/Center si algo faltara)
        Vector3 c(0,0,0); for (size_t i=0;i<gEVsnap.size();i++) c += gEVsnap[i].world0;
        B = c*(1.0f/(float)gEVsnap.size()) + gEVtrans;
    }
    Vector3 off = T - B;
    // EJE BLOQUEADO (X/Y/Z o la normal del extrude): si el target es un BORDE, en vez de solo igualar la coordenada
    // del eje, avanzamos por el eje hasta TOCAR el borde (la recta del eje ∩ el segmento) -> intersecar 2 bordes facil.
    bool axisUnique = false; Vector3 axisA;
    if (gEVuseCustom){ axisUnique=true; axisA=gTransformNormal; }                                   // extrude / orientacion Normal
    else if (axisSelect==X||axisSelect==Y||axisSelect==Z){ axisUnique=true; axisA=AxisOriented(*gEVmesh,axisSelect); }
    if (axisUnique){
        float s;
        if (g_snap.target==SNAP_EDGE && SnapAxisTouchesSegment(B, axisA, eA, eB, s)) off = axisA*s; // tocar el borde por el eje
        else off = axisA*off.Dot(axisA);                                                          // fallback: igualar coordenada
    }
    else if (axisSelect==PlaneX||axisSelect==PlaneY||axisSelect==PlaneZ){ int ex=(axisSelect==PlaneX)?X:(axisSelect==PlaneY)?Y:Z; Vector3 a=AxisOriented(*gEVmesh,ex); off = off - a*off.Dot(a); }
    gEVtrans += off;
    // el recuadro verde en el TARGET (el punto bajo/cerca del cursor al que se pega), como devuelve SnapFindTarget
    g_snapHit = true; g_snapSx = sx; g_snapSy = sy;
}

// world0 (posicion original) del ELEMENTO ACTIVO (vert/borde/cara segun el modo): la "aguja" del snap de rotacion.
// activeIdx indexa vertSel / edgeSel / faceSel segun EditSelectMode. Los verts del activo estan seleccionados ->
// su world0 esta en gEVsnap. Borde = punto medio de sus 2 verts; cara = centro. false si no hay activo.
static bool SnapNeedleActive(Vector3& out){
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
static void SnapFitEditRot(){
    g_snapHit = false;
    if (!g_snap.enabled || !g_snap.afRot || !Viewport3DActive || InteractionMode!=EditMode || !gEVmesh) return;
    if (gEVsnap.empty() || axisSelect==OrbitalAxis) return;
    Vector3 axis = (axisSelect==ViewAxis||axisSelect==XYZ) ? camForward : AxisOriented(*gEVmesh, axisSelect);
    { float to=sqrtf(axis.Dot(axis)); if (to<1e-6f) return; axis=axis*(1.0f/to); }
    Vector3 B0; if (!SnapNeedleActive(B0)) return; // sin activo -> no hay aguja (hay que seleccionar una "punta")
    Vector3 T; float sx=0, sy=0;
    if (!SnapFindTarget(g_snapCurX, g_snapCurY, Viewport3DActive, T, sx, sy)) return;
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
    gAngleTransform += deltaDeg;
    // recuadro verde en el TARGET (el vertice bajo/cerca del cursor al que apunta la aguja), no en la punta activa
    g_snapHit = true; g_snapSx = sx; g_snapSy = sy;
}

// SNAP de ESCALA: escala la seleccion desde el pivote (gEVpivot = centro/cursor) de modo que el vert BASE (el activo
// en modo Active, o el mas cercano al target en Closest) se mueva RADIALMENTE -"como un radio"- hasta el punto mas
// cercano al target sobre su recta de escala. Puede NO tocar el target (queda en el punto mas cercano de esa recta);
// toca exacto cuando el target esta sobre la recta (ej: rotar primero la aguja al target y luego escalar).
static void SnapFitEditScale(){
    g_snapHit = false;
    if (!g_snap.enabled || !g_snap.afScale || !Viewport3DActive || InteractionMode!=EditMode || !gEVmesh) return;
    if (gEVsnap.empty() || gEVshrink) return; // Shrink/Fatten mueve cada vert por su normal -> no aplica este snap
    Vector3 T; float sx=0, sy=0;
    if (!SnapFindTarget(g_snapCurX, g_snapCurY, Viewport3DActive, T, sx, sy)) return;
    const Vector3 P = gEVpivot;
    // BASE: Active = el elemento activo; Closest (y otros) = el vert de la seleccion mas cercano al target
    Vector3 B0; bool any=false;
    if (g_snap.base==SNAP_ACTIVE) any = SnapNeedleActive(B0);
    if (!any){ float bd=1e30f; for (size_t i=0;i<gEVsnap.size();i++){ Vector3 d=gEVsnap[i].world0-T; float dd=d.Dot(d); if(dd<bd){bd=dd;B0=gEVsnap[i].world0;any=true;} } }
    if (!any) return;
    // direccion en la que la ESCALA mueve al base (segun el eje): D=off0 (uniforme), la componente del eje (X/Y/Z),
    // o la del plano. B(amt) = B0 + amt*D -> amt que lo acerca al target = proyeccion de (T-B0) sobre D.
    Vector3 off0 = B0 - P, D;
    if (axisSelect==X||axisSelect==Y||axisSelect==Z){ Vector3 a=AxisOriented(*gEVmesh,axisSelect); D=a*off0.Dot(a); }
    else if (axisSelect==PlaneX||axisSelect==PlaneY||axisSelect==PlaneZ){ int ex=(axisSelect==PlaneX)?X:(axisSelect==PlaneY)?Y:Z; Vector3 a=AxisOriented(*gEVmesh,ex); D=off0 - a*off0.Dot(a); }
    else D=off0;
    float dd=D.Dot(D); if (dd<1e-12f) return; // el base esta en el pivote/eje -> la escala no lo mueve
    gEVscaleAmt = (T - B0).Dot(D) / dd;
    g_snapHit = true; g_snapSx = sx; g_snapSy = sy;
}

void EditXformTranslation(int dx,int dy,float speed){
    if (!gEVmesh) return;
    // EXTRUDE: constreñido a un eje arbitrario (la normal). Proyecta el mouse igual
    // que un eje normal, pero con gTransformNormal.
    if (gEVuseCustom){
        Vector3 a = gTransformNormal;
        float amt = (dx*a.Dot(camRight) - dy*a.Dot(camUp))*speed;
        gEVtrans += a*amt; SnapFitEditTrans(); EVWrite(); return;
    }
    Vector3 free = camRight*(dx*speed) + camUp*(-dy*speed); // plano de la camara
    Vector3 T;
    if (axisSelect==X||axisSelect==Y||axisSelect==Z){
        Vector3 a = AxisOriented(*gEVmesh, axisSelect);
        float amt = (dx*a.Dot(camRight) - dy*a.Dot(camUp))*speed;
        T = a*amt;
    } else if (axisSelect==PlaneX||axisSelect==PlaneY||axisSelect==PlaneZ){
        int ex=(axisSelect==PlaneX)?X:(axisSelect==PlaneY)?Y:Z;
        Vector3 a=AxisOriented(*gEVmesh, ex);
        T = free - a*free.Dot(a);
    } else T = free;
    gEVtrans += T;
    // con proyeccion POR VERTICE (Face Project/Nearest) NO se hace el snap de la seleccion entera: cada vert se
    // proyecta solo (en EVWrite). Sino, el imantado normal de la seleccion al target bajo el cursor.
    if (!SnapFaceIndividualActive()) SnapFitEditTrans();
    EVWrite();
}
// traslacion ABSOLUTA de mundo (gizmo): el punto agarrado sigue al puntero, no se acumulan deltas por pixel
void EditXformTranslationWorld(const Vector3& T){
    if (!gEVmesh) return;
    gEVtrans = T;
    if (!SnapFaceIndividualActive()) SnapFitEditTrans();
    EVWrite();
}

// ============================================================================
//  EL IMAN CON LAS FLECHAS (sin puntero: el telefono; y las flechas de la PC, que se portan igual). Moviendo con el iman
//  prendido, cada flecha SALTA al siguiente nivel de la geometria de alrededor en esa direccion, en vez de deslizar. Los
//  vertices se agrupan por su coordenada sobre el eje del movimiento: un cubo, moviendo en Z, son DOS paradas (los 4 de
//  arriba y los 4 de abajo), no una por vertice. El eje sale del transform elegido: X / Y / Z (Global, Local o View) o,
//  sin eje, la flecha en la pantalla (izq/der = a lo ancho de la vista, arr/ab = a lo alto; en un plano, lo que la flecha
//  tiene adentro del plano). Lo que llega al nivel es el PIVOTE de lo que se mueve; el cuadrado del iman se ve en el
//  vertice del grupo alcanzado mas cercano
// ============================================================================
bool SnapByArrows() {
    if (!g_snap.enabled || !g_snap.afMove || state != translation || !Viewport3DActive) return false;
    if (InteractionMode == EditMode) return gEVmesh && !gEVslide && !gEVshrink;
    return InteractionMode == ObjectMode && !objectState.empty();
}
// el eje (de mundo, unitario, ya en el sentido de la flecha) por el que salta. fx/fy: la flecha en la pantalla
// (x a la derecha, y para abajo). false = esa flecha no mueve (perpendicular a un plano de canto)
static bool SnapArrowAxis(int fx, int fy, Vector3& a) {
    Object* ref = (InteractionMode == EditMode) ? (Object*)gEVmesh : (ActiveObj ? ActiveObj : objectState[0].obj);
    if (!ref) return false;
    const Vector3 right = camRight, arr = camUp;
    const bool positive = (fx > 0 || fy < 0);   // derecha / arriba
    if (axisSelect == X || axisSelect == Y || axisSelect == Z) {
        a = AxisOriented(*ref, axisSelect);
        const float l = a.Length(); if (l < 1e-6f) return false;
        a = a * (1.0f / l);
        // el sentido: el de la flecha sobre la pantalla. Una flecha de costado al eje (arriba con el eje X) o un eje
        // que en la pantalla queda de punta (la profundidad): derecha / arriba van hacia donde el eje apunta en la
        // pantalla (a la derecha o para arriba), izquierda / abajo al reves
        const float ex = a.Dot(right), ey = a.Dot(arr);
        const float s = ex * (float)fx - ey * (float)fy;
        float sign;
        if (s * s > 0.05f * (ex * ex + ey * ey) && ex * ex + ey * ey > 0.01f) sign = (s > 0.0f) ? 1.0f : -1.0f;
        else sign = (positive ? 1.0f : -1.0f) * ((ex + ey >= 0.0f) ? 1.0f : -1.0f);
        a = a * sign;
        return true;
    }
    Vector3 d = (fx != 0) ? right * (float)fx : arr * (float)(-fy);
    if (axisSelect == PlaneX || axisSelect == PlaneY || axisSelect == PlaneZ) {
        const int ex = (axisSelect == PlaneX) ? X : (axisSelect == PlaneY) ? Y : Z;
        Vector3 n = AxisOriented(*ref, ex);
        const float ln = n.Length();
        if (ln > 1e-6f) { n = n * (1.0f / ln); d = d - n * d.Dot(n); }
    }
    const float l = d.Length(); if (l < 1e-3f) return false;
    a = d * (1.0f / l);
    return true;
}
// los vertices de la geometria de alrededor (en MUNDO): las mallas candidatas del iman que no se mueven; editando, ademas
// los vertices de la malla que no entran en el movimiento
static void SnapFixedPoints(std::vector<Vector3>& out) {
    out.clear();
    if (!SceneCollection) return;
    std::vector<Mesh*> meshes;
    SnapCollect(SceneCollection, meshes);
    std::set<int> mobile;   // editando: los vertices de la GPU que se mueven (por su representante)
    if (InteractionMode == EditMode && gEVmesh && gEVmesh->edit)
        for (size_t i = 0; i < gEVsnap.size(); i++) {
            const int k = gEVsnap[i].editK;
            if (k >= 0 && k < (int)gEVmesh->edit->editVerts.size()) mobile.insert(gEVmesh->edit->editVerts[(size_t)k]);
        }
    for (size_t mi = 0; mi < meshes.size(); mi++) {
        Mesh* m = meshes[mi];
        if (!m->vertex || m->vertexSize <= 0) continue;
        const bool edited = (InteractionMode == EditMode && (Object*)m == (Object*)gEVmesh);
        if (InteractionMode == ObjectMode && m->select) continue;   // (lo que se mueve)
        Matrix4 W; m->GetWorldMatrix(W);
        const bool withRep = (int)m->posRep.size() == m->vertexSize;
        for (int v = 0; v < m->vertexSize; v++) {
            if (edited && mobile.count(withRep ? m->posRep[(size_t)v] : v)) continue;
            out.push_back(W * Vector3(m->vertex[v * 3], m->vertex[v * 3 + 1], m->vertex[v * 3 + 2]));
        }
    }
}
bool SnapStepArrow(int fx, int fy) {
    if (!SnapByArrows() || (fx == 0 && fy == 0)) return false;
    Vector3 a;
    if (!SnapArrowAxis(fx, fy, a)) return false;
    // donde esta ahora el pivote de lo que se mueve: el de la foto del arranque + lo ya movido
    Vector3 p0, moved;
    if (InteractionMode == EditMode) { p0 = gEVpivot; moved = gEVtrans; }
    else {
        size_t r = 0;
        for (size_t i = 0; i < objectState.size(); i++) if (objectState[i].obj == ActiveObj) r = i;
        p0 = objectState[r].worldPos;
        moved = objectState[r].obj->GetGlobalPosition() - p0;
    }
    const float current = (p0 + moved).Dot(a);
    // los NIVELES: la coordenada de cada vertice sobre el eje; el proximo hacia adelante (los que estan a la misma
    // altura son una sola parada: la tolerancia es relativa al tamano de la escena)
    std::vector<Vector3> pts;
    SnapFixedPoints(pts);
    if (pts.empty()) return false;
    float tol = 1e-4f;
    for (size_t i = 0; i < pts.size(); i++) { const float t = fabsf(pts[i].Dot(a)); if (t * 1e-5f > tol) tol = t * 1e-5f; }
    bool has = false; float level = 0.0f;
    for (size_t i = 0; i < pts.size(); i++) {
        const float t = pts[i].Dot(a);
        if (t > current + tol && (!has || t < level)) { level = t; has = true; }
    }
    if (!has) return false;   // no hay nada mas adelante: se queda donde esta
    const Vector3 total = moved + a * (level - current);
    g_snapNoCursor = true;
    if (InteractionMode == EditMode) EditXformTranslationWorld(total);
    else SetTranslationObjectsWorld(total);
    g_snapNoCursor = false;
    // el cuadrado del iman: el vertice del grupo alcanzado mas cercano al pivote (ya movido)
    const Vector3 piv = p0 + total;
    float best = 1e30f; Vector3 rep = piv;
    for (size_t i = 0; i < pts.size(); i++) {
        if (fabsf(pts[i].Dot(a) - level) > tol) continue;
        const Vector3 d = pts[i] - piv; const float dd = d.Dot(d);
        if (dd < best) { best = dd; rep = pts[i]; }
    }
    float sx = 0.0f, sy = 0.0f;
    g_snapHit = Viewport3DActive->ProjectPoint(rep, sx, sy);
    g_snapSx = sx; g_snapSy = sy;
    g_redraw = true;
    return true;
}
void EditXformRotAxis(int dx,int dy){
    if (!gEVmesh) return;
    float ang=(dx+dy)*0.1f; gAngleTransform+=ang;
    Vector3 axis = (axisSelect==ViewAxis||axisSelect==XYZ)?camForward:AxisOriented(*gEVmesh,axisSelect);
    gEVrotTotal = Quaternion::FromAxisAngle(axis,ang)*gEVrotTotal; gEVrotTotal.normalize();
    SnapFitEditRot(); // imanta la aguja al target (si snap ON)
    EVWrite();
}
void EditXformRotOrbital(int dx,int dy){
    if (!gEVmesh) return;
    float yaw=dx*0.1f, pitch=dy*0.1f; gAngleTransform+=(dx+dy)*0.1f;
    Quaternion q = Quaternion::FromAxisAngle(camUp,yaw)*Quaternion::FromAxisAngle(camRight,pitch);
    gEVrotTotal = q*gEVrotTotal; gEVrotTotal.normalize();
    EVWrite();
}
void EditXformRotAbs(const Quaternion& qAbs){ // trackball: rotacion absoluta desde el inicio
    if (!gEVmesh) return;
    gEVrotTotal = qAbs;
    SnapFitEditRot(); // imanta la aguja al target (si snap ON) -> gira desde el pivote hacia el target
    EVWrite();
}
void EditXformScaleSet(float f){
    if (!gEVmesh) return;
    gEVscaleAmt = f - 1.0f;
    SnapFitEditScale();
    EVWrite();
}
Vector3 EditXformPivotWorld(){ return gEVpivot; }
void EditXformScale(int dx,int dy,float factor){
    if (!gEVmesh) return;
    gEVscaleAmt += (dx+dy)*factor;
    SnapFitEditScale(); // imanta el vert base al target radialmente (si snap ON)
    EVWrite();
}

// suma de los ejes ACTIVOS (en MUNDO, segun la orientacion) para el valor numerico
static Vector3 EVActiveAxes(){
    Object& o = *(Object*)gEVmesh;
    if (axisSelect==X||axisSelect==Y||axisSelect==Z) return AxisOriented(o, axisSelect);
    if (axisSelect==PlaneX) return AxisOriented(o,Y)+AxisOriented(o,Z);
    if (axisSelect==PlaneY) return AxisOriented(o,X)+AxisOriented(o,Z);
    if (axisSelect==PlaneZ) return AxisOriented(o,X)+AxisOriented(o,Y);
    return AxisOriented(o,X)+AxisOriented(o,Y)+AxisOriented(o,Z); // libre
}
// aplica un valor EXACTO (entrada numerica) al transform de malla en curso, segun
// 'estado' y el eje/orientacion. translate=distancia, rotacion=grados, escala=factor.
void EditXformNumValue(float v){
    if (!gEVmesh) return;
    if (state==translation){
        gEVtrans = (gEVuseCustom ? gTransformNormal : EVActiveAxes()) * v;
    } else if (state==rotation){
        Vector3 ax;
        if (gEVuseCustom) ax = gTransformNormal;
        else if (axisSelect==ViewAxis||axisSelect==XYZ||axisSelect==OrbitalAxis) ax = camForward;
        else ax = AxisOriented(*gEVmesh, axisSelect);
        gEVrotTotal = Quaternion::FromAxisAngle(ax, v); gAngleTransform = v;
    } else if (gEVshrink){ // Shrink/Fatten: el valor es la DISTANCIA por la normal
        gEVscaleAmt = v;
    } else { // EditScale
        gEVscaleAmt = v - 1.0f;
    }
    EVWrite();
}

// SHRINK/FATTEN (Alt+S, menu Mesh > Transform): cada vertice seleccionado se mueve por SU normal (engorda/
// adelgaza). Reusa la maquina de EditScale (toolbar/confirmar/cancelar/tactil) con el flag gEVshrink -> solo
// cambia el calculo (mover por normal en vez de escalar desde el pivote). Confirma/cancela como cualquier transform.
void LayoutShrinkFatten() {
    if (InteractionMode != EditMode || !g_editMesh) return;
    if (EditXformActive()) EditXformConfirm(); // encadenar: confirma el transform anterior
    state = EditScale; axisSelect = XYZ;       // libre (la direccion la da la normal de cada vertice)
    if (CurveBezEditing()) CurveBezXformBegin(); else UndoEditMoveStart((Mesh*)g_editMesh);   // Ctrl+Z: posiciones previas
    gEVshrinkRequest = true;
    EditXformSnapshot();                          // snapshot de la seleccion (calcula las normales por vert)
    gEVshrinkRequest = false;
    if (!EditXformActive()){ state = editNavigation; return; }
    gEVshrink = true;                           // <- ahora si: mover por la normal
    ToolbarRegisterAction(TBScale);             // historial (reusa el de escala)
}

static bool EVSlideByAngle(const std::pair<float,Vector3>& a, const std::pair<float,Vector3>& b){ return a.first < b.first; }
// Los rieles de cada vertice del snapshot: sus vecinos por arista, en MUNDO y CONGELADOS al empezar
// (leerlos en vivo daria mal si el vecino tambien esta seleccionado y se esta moviendo).
static void EVSlideRails(){
    gEVslideCand.clear(); gEVslideDest.clear();
    gEVslideT = 0.0f; gEVslideAccX = gEVslideAccY = 0.0f; gEVslideIdx = 0; gEVslideKeyboard = false;
    if (!gEVmesh || !gEVmesh->edit) return;
    EditMesh* e = gEVmesh->edit;
    gEVslideCand.resize(gEVsnap.size()); gEVslideDest.resize(gEVsnap.size());
    for (size_t i = 0; i < gEVsnap.size(); i++){
        gEVslideDest[i] = gEVsnap[i].world0;      // sin riel elegido, el vertice se queda quieto
        const int k = gEVsnap[i].editK;
        for (int eg = 0; eg < e->NumEdges(); eg++){
            if ((size_t)eg*2+1 >= e->lineIdx.size()) break;
            const int u = e->lineIdx[(size_t)eg*2], v = e->lineIdx[(size_t)eg*2+1];
            const int o = (u == k) ? v : (v == k) ? u : -1;
            if (o < 0 || o*3+2 >= (int)e->pos.size()) continue;
            gEVslideCand[i].push_back(EVLocalToWorld(Vector3(e->pos[(size_t)o*3], e->pos[(size_t)o*3+1], e->pos[(size_t)o*3+2])));
        }
    }
    // Las direcciones POSIBLES dependen de la vista: una arista que apunta a la camara se proyecta casi a
    // un punto y no se puede apuntar, asi que sale de la lista (de ahi que un vertice de cubo de 1, 2 o 3
    // segun desde donde mires). El resto queda ORDENADO por angulo en pantalla, para que las flechas las
    // recorran dando la vuelta y no a los saltos.
    if (!viewPortActive || viewPortActive->ViewportKind() != 1) return;
    Viewport3D* vp = (Viewport3D*)viewPortActive; vp->BindView();
    for (size_t i = 0; i < gEVsnap.size(); i++){
        float sx0, sy0; if (!vp->ProjectPoint(gEVsnap[i].world0, sx0, sy0)) continue;
        std::vector<std::pair<float,Vector3> > useful;
        for (size_t c = 0; c < gEVslideCand[i].size(); c++){
            float sx, sy; if (!vp->ProjectPoint(gEVslideCand[i][c], sx, sy)) continue;
            const float ex = sx - sx0, ey = sy - sy0;
            if (ex*ex + ey*ey < 4.0f) continue;                       // de frente a la camara: no se puede apuntar
            useful.push_back(std::make_pair(atan2f(ey, ex), gEVslideCand[i][c]));
        }
        std::sort(useful.begin(), useful.end(), EVSlideByAngle);
        gEVslideCand[i].clear();
        for (size_t c = 0; c < useful.size(); c++) gEVslideCand[i].push_back(useful[c].second);
    }
}

// el riel numero 'gEVslideIdx' de cada vertice (las flechas arriba/abajo lo van corriendo)
static void EVSlideByIndex(){
    for (size_t i = 0; i < gEVsnap.size() && i < gEVslideCand.size(); i++){
        const int n = (int)gEVslideCand[i].size();
        if (n <= 0) continue;
        int k = gEVslideIdx % n; if (k < 0) k += n;
        gEVslideDest[i] = gEVslideCand[i][(size_t)k];
    }
}

// Prende/apaga el slide sobre un transform YA EN CURSO (la segunda G). Al apagarlo el move sigue
// desde donde estaba, porque gEVtrans nunca se toco.
void EditSlideToggle(){
    if (!gEVmesh) return;
    gEVslide = !gEVslide;
    if (gEVslide) EVSlideRails();
    EVWrite();
    Notify(T(gEVslide ? "Slide Vertices" : "Move"), false);
}

// Menu Vertex / Shift+V: arranca el move y lo pone en slide de una.
void LayoutSlideVerticesEdit(){
    if (InteractionMode != EditMode || !g_editMesh) return;
    if (!EditXformStart(translation, ViewAxis)) return;
    gEVslide = true; EVSlideRails(); EVWrite();
}

// El mouse durante el slide. Se trabaja con el DELTA acumulado (no con la posicion absoluta) porque
// ProjectPoint devuelve pixeles LOCALES del viewport y el delta es el mismo en los dos espacios.
void EditSlideMouse(int dx, int dy){
    if (!gEVslide || !gEVmesh || gEVsnap.empty() || gEVslideDest.empty()) return;
    if (!viewPortActive || viewPortActive->ViewportKind() != 1) return;
    Viewport3D* vp = (Viewport3D*)viewPortActive; vp->BindView();
    gEVslideAccX += (float)dx; gEVslideAccY += (float)dy;
    const float length = sqrtf(gEVslideAccX*gEVslideAccX + gEVslideAccY*gEVslideAccY);
    if (length < 4.0f) return;                           // hace falta arrastrar para saber hacia donde
    gEVslideKeyboard = false;                            // se movio el mouse: vuelve a mandar el
    // El riel se re-elige EN CADA movimiento (no se congela con el primer arrastre): asi, sin soltar,
    // pasas de una direccion posible a otra con solo apuntar para el otro lado.
    { const float rx = gEVslideAccX/length, ry = gEVslideAccY/length;
      for (size_t i = 0; i < gEVsnap.size() && i < gEVslideCand.size(); i++){
        float sx0, sy0; if (!vp->ProjectPoint(gEVsnap[i].world0, sx0, sy0)) continue;
        float best = -2.0f; int picked = -1;          // la arista mas parecida al arrastre, en pantalla
        for (size_t c = 0; c < gEVslideCand[i].size(); c++){
            float sx, sy; if (!vp->ProjectPoint(gEVslideCand[i][c], sx, sy)) continue;
            const float ex = sx - sx0, ey = sy - sy0, el = sqrtf(ex*ex + ey*ey);
            if (el < 0.001f) continue;
            const float d = (ex/el)*rx + (ey/el)*ry;
            if (d > best){ best = d; picked = (int)c; }
        }
        if (picked >= 0){ gEVslideDest[i] = gEVslideCand[i][(size_t)picked];
            if (i == 0) gEVslideIdx = picked; }        // las flechas siguen desde donde lo dejo el mouse
      } }
    // el factor lo manda el PRIMER vertice: todos deslizan lo mismo, cada uno por su riel (como Blender)
    float ax, ay, bx, by;
    if (vp->ProjectPoint(gEVsnap[0].world0, ax, ay) && vp->ProjectPoint(gEVslideDest[0], bx, by)){
        const float ex = bx - ax, ey = by - ay, len2 = ex*ex + ey*ey;
        if (len2 > 0.000001f){
            float t = (gEVslideAccX*ex + gEVslideAccY*ey) / len2;
            if (t < 0.0f) t = 0.0f; if (t > 1.0f) t = 1.0f;
            gEVslideT = t;
        }
    }
    EVWrite();
}

// Teclado durante el slide (Symbian / sin mouse, mismo flujo que el Loop Cut): 0=izq 1=der 2=arriba
// 3=abajo. Arriba/abajo van CAMBIANDO la direccion entre las posibles; izquierda/derecha van del 0% al
// 100% por esa direccion. Devuelve false si no habia slide en curso.
bool EditSlideKey(int which){
    if (!gEVslide || !gEVmesh || gEVsnap.empty()) return false;
    if (which == 2 || which == 3){
        if (gEVslideKeyboard) gEVslideIdx += (which == 2) ? 1 : -1;  // la primera vez toma el riel 0
        gEVslideKeyboard = true;
        EVSlideByIndex();
    } else {
        const float step = 0.05f;
        gEVslideT += (which == 1) ? step : -step;
        if (gEVslideT < 0.0f) gEVslideT = 0.0f; if (gEVslideT > 1.0f) gEVslideT = 1.0f;
    }
    EVWrite(); g_redraw = true; return true;
}

// fija el resultado: recalcula bordes/centro/posRep (sin invalidar el edit) y las
// NORMALES (salvo Bloquear normales, de la malla). El overlay ya esta sincronizado (SyncPos).
void EditXformConfirm(){
    // un solo Ctrl+Z: el move Y el keyframe del auto key de vertices (solo si va a haber keyframe: sin auto key el
    // Auto Merge sigue siendo su propio paso)
    extern bool AutoKeyOn;
    const bool akGroup = AutoKeyOn && ActiveAnimKind == 3 && gEVmesh && (Mesh*)ActiveAnimMesh == gEVmesh;
    if (akGroup) UndoGroupStart();
    CurveBezXformEnd(true); // (curva de splines: su paso de undo, si el move cambio algo)
    UndoEditMoveConfirm(); // Ctrl+Z: el move PURO se acepto -> pushea el pendiente (NULL si fue extrude)
    if (gEVmesh){
        Mesh* m = gEVmesh;
        // CONSERVA posRep (regroupPosRep=false): un move NO cambia la topologia, y si el snap dejo 2 verts
        // INDEPENDIENTES encimados, rederivar posRep por posicion los SOLDARIA (caras y edit desincronizados).
        m->ComputeBorders(false, false);      // posRep(conserva)/centerGeom/bordes; conserva el edit
        if (!m->lockNormals) m->RecalculateNormals();
        // AUTO MERGE (opt-in, OFF por defecto): recien AHORA, si el usuario lo pidio, se sueldan los verts movidos
        // con cualquier vert a <= threshold. No-op si no hay nada cerca (MergeVertsEdit retorna sin tocar nada).
        if (g_autoMerge) MergeVertsEdit(m, 3 /*By Distance*/, g_autoMergeThreshold, Vector3(0,0,0));
        if (!m->modifiers.empty()) m->GenerateMeshModified(); // regen final (toma las normales recalculadas)
    }
    // AUTO KEY de VERTICES: con la vertex anim de ESTA malla activa en el timeline, confirmar
    // el move guarda la malla como keyframe. VA DESPUES de RecalculateNormals: asi el keyframe
    // captura las NORMALES nuevas de la pose deformada (sino la iluminacion quedaba en la vieja).
    { extern bool AutoKeyOn; extern void VertexAnimInsertKeyframe();
      if (AutoKeyOn && ActiveAnimKind == 3 && gEVmesh && (Mesh*)ActiveAnimMesh == gEVmesh)
          VertexAnimInsertKeyframe(); }
    if (akGroup) UndoGroupEnd();
    gEVsnap.clear(); gEVprop.clear(); gEVmesh = NULL;
    gEVslide = false; gEVslideCand.clear(); gEVslideDest.clear();
    state = editNavigation;
    g_extrudeInProgress = false; // termino el transform
    // el motion del transform (controls.cpp) dejo ViewPortClickDown en true para CONGELAR el foco
    // en el viewport mientras dura el G/R/S. Al terminar por ESC/ENTER/boton de barra (sin un
    // mouse-up sobre el viewport, que es lo que normalmente lo libera) hay que APAGARLO aca, sino
    // viewPortActive queda clavado en este viewport y el editor UV (u otro) no recibe los clicks:
    // el mouse esta abajo pero el foco sigue arriba. Igual que el Timeline/Editor2D.
    extern bool ViewPortClickDown; ViewPortClickDown = false;
}

// descarta: restaura las posiciones del snapshot (mundo -> local) y limpia.
void EditXformCancel(){
    CurveBezXformEnd(false); // (curva de splines: vuelven EXACTAS, tambien lo que la regla movio sin estar elegido)
    UndoEditMoveCancel(); // Ctrl+Z: move cancelado -> descarta el pendiente (no deja undo no-op)
    if (gEVmesh){
        Mesh* m = gEVmesh;
        for (size_t i=0;i<gEVsnap.size();i++){
            EditVtxSnap& s = gEVsnap[i];
            Vector3 ln = EVWorldToLocal(s.world0);
            int k = s.editK; // restaura la posicion EDITABLE (no toca vertex[] a mano)
            if (m->edit && k>=0 && k*3+2 < (int)m->edit->pos.size()){
                m->edit->pos[k*3]=ln.x; m->edit->pos[k*3+1]=ln.y; m->edit->pos[k*3+2]=ln.z;
            }
        }
        for (size_t i=0;i<gEVprop.size();i++){ // proportional editing: los arrastrados vuelven a su lugar
            Vector3 ln = EVWorldToLocal(gEVprop[i].world0);
            int k = gEVprop[i].editK;
            if (m->edit && k>=0 && k*3+2 < (int)m->edit->pos.size()){
                m->edit->pos[k*3]=ln.x; m->edit->pos[k*3+1]=ln.y; m->edit->pos[k*3+2]=ln.z;
            }
        }
        if (m->edit){ m->edit->PushPositions(); m->edit->RefreshOverlay(); }
        if (!m->modifiers.empty()) m->GenerateMeshModified(); // preview vuelve al estado previo al cancelar
    }
    gEVsnap.clear(); gEVprop.clear(); gEVmesh = NULL;
    gEVslide = false; gEVslideCand.clear(); gEVslideDest.clear();
    state = editNavigation;
    g_extrudeInProgress = false; // termino el transform
    // liberar el foco congelado: ver la nota en EditXformConfirm (cancelar por ESC no genera
    // mouse-up, asi que ViewPortClickDown quedaba trabado en true y clavaba viewPortActive aca).
    extern bool ViewPortClickDown; ViewPortClickDown = false;
}


// ====================================================================
// menu de SNAP (shift+s, ): mueve seleccion / cursor 3D
// ====================================================================
static PopupMenu* gMenuSnap = NULL;

static void ActionSnap(int aId) {
    switch (aId) {
        case 0: SnapSelectionToGrid(); break;
        case 1: SnapSelectionToCursor(false); break;
        case 2: SnapSelectionToCursor(true); break;   // keep offset
        case 3: SnapSelectionToActive(); break;
        case 4: SnapCursorToSelected(); break;
        case 5: SnapCursorToOrigin(); break;
        case 6: SnapCursorToGrid(); break;
        case 7: SnapCursorToActive(); break;
    }
}

// El menu Snap se usa standalone (Shift+S) Y como submenu del menu "Mesh" de Edit Mode. Ids 0-7 + accion
// ActionSnap: como submenu del Mesh, LayoutActionMesh rutea esos mismos ids a ActionSnap (no chocan con el
// resto del Mesh, que usa 100-102 y 361-364).
PopupMenu* LayoutSubmenuSnap() {
    if (!gMenuSnap) {
        gMenuSnap = new PopupMenu();
        gMenuSnap->title  = T("Snap");
        gMenuSnap->action = ActionSnap;
        gMenuSnap->Add(T("Selection to Grid"), 0);
        gMenuSnap->Add(T("Selection to Cursor"), 1);
        gMenuSnap->Add(T("Selection to Cursor (Keep Offset)"), 2);
        gMenuSnap->Add(T("Selection to Active"), 3);
        gMenuSnap->Add(T("Cursor to Selected"), 4);
        gMenuSnap->Add(T("Cursor to World Origin"), 5);
        gMenuSnap->Add(T("Cursor to Grid"), 6);
        gMenuSnap->Add(T("Cursor to Active"), 7);
    }
    return gMenuSnap;
}

// submenu Delete (vertices/aristas/caras/loops) para embeber en el menu Mesh. Es el MISMO gMenuDelete del
// atajo X (ids 361-364 + accion LayoutActionObject) -> anda igual embebido o standalone.
PopupMenu* LayoutSubmenuDelete() {
    EnsureMenuDelete();
    return gMenuDelete;
}

void LayoutMenuSnap(int mx, int my) {
    PopupMenu* m = LayoutSubmenuSnap();
    m->Open(mx, my, MenuScreenW, MenuScreenH);
    OpenMenu = m;
}

// ===== menu MERGE (tecla M / submenu del menu "Mesh"): suelda los verts seleccionados =====
static PopupMenu* gMenuMerge = NULL;

// modo: 0 At Center, 1 At Cursor, 2 Collapse, 3 By Distance. Corre sobre la malla en Edit Mode.
static void ActionMerge(int mode) {
    if (InteractionMode != EditMode || !g_editMesh) return;
    Mesh* m = (Mesh*)g_editMesh;
    // cursor 3d (MUNDO) -> LOCAL del mesh (para At Cursor): misma matematica que Snap Selection to Cursor,
    // y por lo mismo BASE: el merge MUEVE VERTICES y el cursor 3D no depende de ninguna camara (Objects.h).
    Quaternion rg = RotGlobalOf(m); Vector3 sg = ScaleGlobalOf(m);
    Vector3 dloc = rg.Inverted() * (cursor3D.pos - m->GetGlobalPositionBase());
    Vector3 cursorLocal(sg.x!=0.0f?dloc.x/sg.x:dloc.x, sg.y!=0.0f?dloc.y/sg.y:dloc.y, sg.z!=0.0f?dloc.z/sg.z:dloc.z);
    MergeVertsEdit(m, mode, g_mergeDist, cursorLocal);
    g_redraw = true;
}

// submenu Merge (ids 380-383 + accion LayoutActionObject): anda igual embebido en el menu "Mesh" o standalone (tecla M)
PopupMenu* LayoutSubmenuMerge() {
    if (!gMenuMerge) {
        gMenuMerge = new PopupMenu();
        gMenuMerge->title  = T("Merge");
        gMenuMerge->action = LayoutActionObject;
        gMenuMerge->Add(T("At Center"), 380);
        gMenuMerge->Add(T("At Cursor"), 381);
        gMenuMerge->Add(T("Collapse"), 382);
        gMenuMerge->Add(T("By Distance"), 383);
    }
    return gMenuMerge;
}

// submenu Normals (Recalculate + Flip) para embeber en el menu "Mesh". Ids 322/324 + accion LayoutActionObject.
static PopupMenu* gMenuNormals = NULL;
PopupMenu* LayoutSubmenuNormals() {
    if (!gMenuNormals) {
        gMenuNormals = new PopupMenu();
        gMenuNormals->title  = T("Normals");
        gMenuNormals->action = LayoutActionObject;
        gMenuNormals->Add(T("Recalculate Normals"), 322)->shortcut = "@3d.normals"; // orienta hacia afuera (cubo/esfera OK)
        gMenuNormals->Add(T("Flip"), 324);                                    // simplemente invierte las normales
    }
    return gMenuNormals;
}

// tecla M en Edit Mode: abre el menu Merge en el cursor
void LayoutMenuMerge(int mx, int my) {
    if (InteractionMode != EditMode) return;
    PopupMenu* m = LayoutSubmenuMerge();
    if (OpenMenu) OpenMenu->Close();
    m->Open(mx, my, MenuScreenW, MenuScreenH);
    OpenMenu = m;
}

// accion del menu "Mesh" (Edit Mode): los ids del submenu Snap (0-7) van a ActionSnap; el resto (Transform
// 100-102, Delete 361-364) lo maneja el dispatcher comun de objeto/malla.
static void LayoutActionMesh(int aId) {
    if (aId >= 0 && aId <= 7) { ActionSnap(aId); return; }
    LayoutActionObject(aId);
}

// ====================================================================
// arrastre de la BARRA de scroll: click la agarra; en PC se suelta al
// soltar el boton (LayoutRelease) y en Symbian con otro click (el driver
// HID no manda movimiento con el boton mantenido)
// ====================================================================

static ViewportBase* gScrollBarDrag = NULL;
static bool gScrollBarVertical = true;
static bool gScrollMoved = false; // hubo movimiento desde el agarre
static int gScrollPrevX = 0;
static int gScrollPrevY = 0;

// Cada viewport dice SOLO si scrollea (ViewportBase::AsScrollable). Antes esto era una tabla de downcasts a
// mano por ViewportKind() y habia que acordarse de venir a tocarla aca por cada viewport nuevo.
static Scrollable* AsScrollable(ViewportBase* v) { return v ? v->AsScrollable() : NULL; }

// suelta la barra agarrada (si habia): el panel deja de seguir al mouse
static void ScrollBarDragRelease() {
    Scrollable* sc = AsScrollable(gScrollBarDrag);
    if (sc) {
        sc->mouseOverScrollYpress = false;
        sc->mouseOverScrollXpress = false;
    }
    gScrollBarDrag = NULL;
}

// la ZONA de la barra de scroll de un panel (un "scrollbar area" reservada al borde derecho / de abajo): la MISMA para
// el hover y para el agarre del click
static bool ScrollBarZone(ViewportBase* v, Scrollable* sc, int mx, int my, bool& zoneV, bool& zoneH) {
    int areaScroll = borderGS + GlobalScale * 9;
    zoneV = sc->scrollY &&
        mx >= v->x + v->width - areaScroll &&
        my >= v->y + v->BarTopOffset();
    zoneH = !zoneV && sc->scrollX &&
        my >= v->y + v->height - areaScroll &&
        mx < v->x + v->width - (sc->scrollY ? areaScroll : 0);
    return zoneV || zoneH;
}

// EL BORDE DE LA VENTANA: FindViewportUnderMouse le descuenta a cada viewport un margen (paddingViewportGS) que es para
// la division con el vecino, pero contra el borde de la ventana no hay vecino ni division y ahi termina la barra de
// scroll de un panel pegado a la derecha (o abajo). Empujar el mouse contra el borde de la pantalla para agarrarla es
// lo natural y caia en ese margen: el click no era de nadie, seguia de largo al viewport ACTIVO (button_left: en
// Propiedades es la flecha izquierda del teclado y plegaba la tarjeta; viniendo rapido del 3D, el 3D) y el arrastre
// ENVOLVIA el cursor en los bordes de ESE viewport (lo teletransportaba a la otra punta y no lo dejaba salir). La hoja
// que tiene el punto, sin el margen: solo cuando FindViewportUnderMouse no devolvio nada (sobre una division devuelve
// el contenedor, y eso sigue siendo para redimensionar)
static ViewportBase* LeafAtWindowEdge(ViewportBase* aNode, int mx, int my) {
    if (!aNode || mx < aNode->x || mx >= aNode->x + aNode->width || my < aNode->y || my >= aNode->y + aNode->height)
        return NULL;
    if (aNode->isLeaf()) return aNode;
    if (aNode->ContainerKind() == 1) {
        ViewportBase* a = LeafAtWindowEdge(((ViewportRow*)aNode)->childA, mx, my);
        return a ? a : LeafAtWindowEdge(((ViewportRow*)aNode)->childB, mx, my);
    }
    if (aNode->ContainerKind() == 2) {
        ViewportBase* a = LeafAtWindowEdge(((ViewportColumn*)aNode)->childA, mx, my);
        return a ? a : LeafAtWindowEdge(((ViewportColumn*)aNode)->childB, mx, my);
    }
    return NULL;
}

// el click en la zona de la barra de scroll de un panel la AGARRA (y en el riel, el agarre salta ahi: TeleportY)
static bool ScrollBarGrab(ViewportBase* v, int mx, int my) {
    Scrollable* sc = AsScrollable(v);
    bool zoneV = false, zoneH = false;
    if (!sc || !ScrollBarZone(v, sc, mx, my, zoneV, zoneH)) return false;
    gScrollBarDrag = v;
    gScrollBarVertical = zoneV;
    gScrollMoved = false; // el up del MISMO tap no la suelta
    gScrollPrevX = mx;
    gScrollPrevY = my;
    if (zoneV) { sc->mouseOverScrollYpress = true; sc->TeleportY(my - v->y, v->height); g_redraw = true; }
    else sc->mouseOverScrollXpress = true;
    return true;
}

bool LayoutInDrag() {
    return gScrollBarDrag != NULL;
}

// el popup activo (color picker) esta arrastrando un valor?
bool LayoutPopupDragging() {
    return PopUpActive && PopUpActive->Dragging();
}

static void ReleaseDragOutliners(ViewportBase* aNode, int mx, int my) {
    if (!aNode) return;
    if (aNode->isLeaf()) {
        if (VpIsOutliner(aNode)) ((Outliner*)aNode)->ReleaseDrag(mx, my);
        return;
    }
    if (aNode->ContainerKind() == 1) {
        ReleaseDragOutliners(((ViewportRow*)aNode)->childA, mx, my);
        ReleaseDragOutliners(((ViewportRow*)aNode)->childB, mx, my);
    } else if (aNode->ContainerKind() == 2) {
        ReleaseDragOutliners(((ViewportColumn*)aNode)->childA, mx, my);
        ReleaseDragOutliners(((ViewportColumn*)aNode)->childB, mx, my);
    }
}

void LayoutRelease(int mx, int my) {
    MenuSliderDragRelease(); // fin del drag del item-slider de un menu (si habia)
    MenuScrollDragRelease(); // ...y el de la barra de scroll de un menu
    BrushBarDragRelease();   // fin del gesto en la fila de barras del pincel; sin arrastre = tap -> editar
    // Symbian (tap-agarra / tap-suelta): el up del MISMO tap que agarro (down+up
    // en ~8ms) no suelta, solo si hubo movimiento. En PC (hold-drag) el up SIEMPRE
    // suelta: sino un click sin movimiento dejaba la barra AGARRADA y el panel
    // scrolleaba siguiendo al mouse con el boton levantado.
#ifdef W3D_SYMBIAN
    if (gScrollBarDrag && gScrollMoved) ScrollBarDragRelease();
#else
    ScrollBarDragRelease();
#endif
    if (PopUpActive) PopUpActive->Release(); // el picker (hold-drag PC)
    // drop del arrastre del outliner (reordenar / emparentar)
    if (rootViewport) ReleaseDragOutliners(rootViewport, mx, my);
}

// ====================================================================
// hover / click / teclado
// ====================================================================

// hover de barras + limpiar el hover de los viewports sin mouse encima
static void LayoutHoverTree(ViewportBase* aNode, ViewportBase* aUnder,
                             int aMx, int aMy) {
    if (!aNode) return;
    if (aNode->isLeaf()) {
        aNode->BarHover(aMx, aMy);
        if (aNode != aUnder) aNode->ClearHover();
        return;
    }
    if (aNode->ContainerKind() == 1) {
        LayoutHoverTree(((ViewportRow*)aNode)->childA, aUnder, aMx, aMy);
        LayoutHoverTree(((ViewportRow*)aNode)->childB, aUnder, aMx, aMy);
    } else if (aNode->ContainerKind() == 2) {
        LayoutHoverTree(((ViewportColumn*)aNode)->childA, aUnder, aMx, aMy);
        LayoutHoverTree(((ViewportColumn*)aNode)->childB, aUnder, aMx, aMy);
    }
}

// DRAG-SCROLL de menus desplegables largos (ej: armature con 129 clips): arrastrar el dedo/mouse scrollea la lista;
// un toque sin arrastrar selecciona. La seleccion normal es en el DOWN, pero si el menu es SCROLLABLE se DIFIERE al
// UP para distinguir tap (selecciona) de drag (scrollea).
static PopupMenu* g_menuDrag = NULL;   // submenu mas profundo que se esta arrastrando (NULL = no)
static int  g_menuDragY0 = 0, g_menuDragScroll0 = 0;
static bool g_menuDragMoved = false;
static PopupMenu* MenuScrollUnderCursor(int mx, int my){
    if (!OpenMenu) return NULL;
    // el MAS PROFUNDO que contiene el punto (se dibuja encima). Caminar "mientras el siguiente contenga" se
    // frenaba en un submenu que no contiene el punto aunque su hijo si (un sub-submenu se extiende mas alla)
    PopupMenu* t = NULL;
    for (PopupMenu* m = OpenMenu; m && m->open; m = m->openSubmenu) if (m->Contains(mx, my)) t = m;
    return (t && t->MaxScroll() > 0) ? t : NULL;
}
// llamado en el DOWN sobre un menu SCROLLABLE: arranca un posible drag/tap (difiere la seleccion). true = diferido.
bool LayoutMenuDragStart(int mx, int my){
    PopupMenu* sc = MenuScrollUnderCursor(mx, my);
    if (!sc) return false;
    g_menuDrag = sc; g_menuDragY0 = my; g_menuDragScroll0 = sc->scroll; g_menuDragMoved = false;
    return true;
}
// ============================================================================================================
//  EL MENU DESPLEGABLE CON EL DEDO: mantener, deslizar y soltar. Apoyar el dedo sobre un menu abierto NO elige: la
//  opcion de abajo del dedo se RESALTA mientras se desliza (y una que abre un submenu lo abre, como el hover del mouse)
//  y SOLTAR elige esa. Soltar AFUERA del menu lo cierra sin elegir nada. En un menu largo (con barra de scroll), llevar
//  el dedo al borde de arriba o de abajo lo va scrolleando solo (mas rapido cuanto mas al borde).
//  Lo arranca controls.cpp con el down de un dedo. La pulsacion larga del 3D abre el menu contextual con el dedo
//  todavia apoyado: ahi soltar sin haberlo movido deja el menu abierto (para tocar la opcion despues).
//  El mouse sigue como siempre (click elige).
// ============================================================================================================
extern void MenuClearGuardOpen();   // PopupMenu.cpp
static bool g_mtActive = false;
static bool g_mtLength = false;     // lo abrio una pulsacion larga (ver arriba)
static bool g_mtMoved  = false;
static int  g_mtX0 = 0, g_mtY0 = 0, g_mtX = 0, g_mtY = 0;
static unsigned g_mtLastScroll = 0;
// el menu (el mas hondo) donde se APOYO el dedo. Un TOQUE (sin deslizar) solo elige en ESE menu: tocar "Mallas" del
// menu Anadir abre su submenu y en un telefono angosto el submenu cae justo debajo del dedo -> soltar elegia su
// primera opcion sin querer. Elegir en un submenu que se abrio con este mismo dedo es solo deslizando hasta la opcion
static PopupMenu* g_mtMenu0 = NULL;
// el menu abierto mas hondo que contiene el punto (se dibuja encima de los otros)
static PopupMenu* MenuMoreDeepIn(int mx, int my){
    PopupMenu* t = NULL;
    for (PopupMenu* m = OpenMenu; m && m->open; m = m->openSubmenu) if (m->Contains(mx, my)) t = m;
    return t;
}
static int MenuRow(PopupMenu* m, int my){
    const int oy = m->title.empty() ? 0 : (RowHeightGS + gapGS);
    return m->scroll + (my - m->y - borderGS - oy) / (RowHeightGS + gapGS);
}
static void MenuTouchHighlight(int mx, int my){
    if (!OpenMenu) return;
    if (OpenMenu->ContainsWithSubmenus(mx, my)) { OpenMenu->MouseMove(mx, my); return; }   // (resalta + submenus)
    // afuera: nada resaltado (soltar no elige)
    PopupMenu* d = OpenMenu;
    while (d->openSubmenu && d->openSubmenu->open) d = d->openSubmenu;
    d->selectIndex = -1;
}
bool LayoutMenuTouchStart(int mx, int my, bool fromLength){
    g_mtActive = false;
    if (!LayoutMenuOpen()) return false;
    const bool inside = OpenMenu->ContainsWithSubmenus(mx, my);
    if (!inside && !fromLength) return false;   // afuera: el click de siempre lo cierra (LayoutClickUI)
    // una fila SLIDER (un valor con su barra) sigue con su arrastre de siempre
    PopupMenu* m = inside ? MenuMoreDeepIn(mx, my) : NULL;
    if (m) { const int f = MenuRow(m, my);
             if (f >= 0 && f < (int)m->items.size() && m->items[f]->valueFloat) return false; }
    g_mtActive = true; g_mtLength = fromLength; g_mtMoved = false;
    g_mtX0 = g_mtX = mx; g_mtY0 = g_mtY = my; g_mtLastScroll = 0;
    g_mtMenu0 = m;   // (ANTES de resaltar: resaltar puede abrir un submenu encima del dedo)
    if (inside) MenuTouchHighlight(mx, my);
    g_redraw = true;
    return true;
}
bool LayoutMenuTouchActive(){
    if (g_mtActive && !LayoutMenuOpen()) g_mtActive = false;   // (el menu se cerro por otro lado)
    return g_mtActive;
}
void LayoutMenuTouchMove(int mx, int my){
    if (!LayoutMenuTouchActive()) return;
    g_mtX = mx; g_mtY = my;
    if (abs(mx - g_mtX0) + abs(my - g_mtY0) > 8 * GlobalScale) g_mtMoved = true;
    MenuTouchHighlight(mx, my);
    g_redraw = true;
}
void LayoutMenuTouchRelease(int mx, int my){
    if (!LayoutMenuTouchActive()) { g_mtActive = false; return; }
    g_mtActive = false;
    g_redraw = true;
    if (g_mtLength && !g_mtMoved) return;   // el dedo de la pulsacion larga: el menu queda abierto
    PopupMenu* m = OpenMenu;
    if (!m->ContainsWithSubmenus(mx, my)) { m->Close(); return; }   // soltado afuera: se cierra sin elegir
    // UN TOQUE que abrio un submenu encima del dedo: el submenu queda abierto y no se elige nada (ver g_mtMenu0)
    if (!g_mtMoved && MenuMoreDeepIn(mx, my) != g_mtMenu0) return;
    MenuClearGuardOpen();   // (deslizar y soltar es deliberado: elige aunque el submenu se abrio con este mismo dedo)
    const int id = m->Click(mx, my);
    if (id >= 0) m->Run(id);
}
void LayoutMenuTouchCancel(){ g_mtActive = false; }   // (un 2do dedo)
// cada cuadro: el dedo en el borde de arriba o de abajo de un menu largo lo va scrolleando
void LayoutMenuTouchTick(unsigned nowMs){
    if (!LayoutMenuTouchActive()) return;
    PopupMenu* m = MenuMoreDeepIn(g_mtX, g_mtY);
    if (!m || m->MaxScroll() <= 0) return;
    const int rowH = RowHeightGS + gapGS;
    const int oy = m->title.empty() ? 0 : rowH;
    const int up = m->y + borderGS + oy, down = m->y + m->height - borderGS;
    int dir = 0, prof = 0;
    if (g_mtY < up + rowH)      { dir = -1; prof = up + rowH - g_mtY; }
    else if (g_mtY > down - rowH)  { dir = +1; prof = g_mtY - (down - rowH); }
    if (!dir) return;
    g_redraw = true;   // (mientras el dedo este en el borde, el loop no duerme)
    // recien entrado al borde: una fila cada 160 ms; pegado al borde: cada 50
    int interval = 160 - prof * 110 / (rowH > 0 ? rowH : 1);
    if (interval < 50) interval = 50; if (interval > 160) interval = 160;
    if (g_mtLastScroll != 0 && nowMs - g_mtLastScroll < (unsigned)interval) return;
    g_mtLastScroll = nowMs;
    int s = m->scroll + dir;
    if (s < 0) s = 0; if (s > m->MaxScroll()) s = m->MaxScroll();
    if (s != m->scroll) { m->scroll = s; g_mtMoved = true; MenuTouchHighlight(g_mtX, g_mtY); }
}
// llamado en el UP: si NO se arrastro (fue un tap), selecciona el item bajo el cursor. true = consumido.
bool LayoutMenuDragRelease(int mx, int my){
    if (!g_menuDrag) return false;
    bool moved = g_menuDragMoved; g_menuDrag = NULL;
    if (!moved && OpenMenu){
        extern void MenuClearGuardOpen(); MenuClearGuardOpen(); // el tap diferido es DELIBERADO: no lo bloquea el guard de submenu-recien-abierto
        PopupMenu* m = OpenMenu; // Click puede CERRAR el menu (OpenMenu=NULL) -> capturar antes de usar m->action
        int id = m->Click(mx, my);
        if (id >= 0) m->Run(id);   // accion propia del item (declarativo) o el action(id) viejo
    }
    g_redraw = true;
    return true;
}

bool LayoutMotionUI(int mx, int my) {
    if (!rootViewport) return false;
    // ARRASTRE de una barra del pincel: el gesto ya quedo lockeado a esa barra en el down, asi que
    // manda sobre todo lo demas (mismo criterio que el drag del item-slider de los menus).
    if (BrushBarDragActive()) {
        if (leftMouseDown) { BrushBarDragMove(mx, my); return true; }
        BrushBarDragRelease(); // up perdido (se solto fuera de la ventana): cerrar el gesto
    }
    if (PopUpActive && (!PopUpActive->NoModal() || PopUpActive->Dragging())) {
        // popup modal (selector de color): se queda con el mouse (uno NO modal, solo mientras lo arrastran)
        PopUpActive->Motion(mx, my);
        // el SEMI-MODAL (panel de la ultima operacion, abajo a la izquierda): AFUERA de el el mouse sigue siendo
        // del layout -> el viewport activo sigue al cursor (si no, quedaba clavado en el 3D y el click en otro
        // viewport envolvia el cursor del 3D: saltaba a la esquina opuesta). El click afuera lo confirma.
        if (!(PopUpActive && PopUpActive->ClosesWithViewport() && !PopUpActive->Dragging() &&
              !PopUpActive->Contains(mx, my)))
            return true;
    }
#ifndef W3D_SYMBIAN
    // UP PERDIDO (se solto y el soltar no llego): sin el boton apretado la barra NO sigue agarrada, igual que el pincel
    // y los menus de arriba. Antes el panel seguia scrolleando detras del mouse con el boton levantado. (Symbian agarra
    // y suelta con dos taps: ahi la barra va suelta del boton y la suelta el 2do click, LayoutClickUI)
    if (gScrollBarDrag && !leftMouseDown) ScrollBarDragRelease();
#endif
    if (gScrollBarDrag) {
        // la barra agarrada sigue al mouse (factor del recorrido real)
        gScrollMoved = true;
        Scrollable* sc = AsScrollable(gScrollBarDrag);
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
    if (LayoutMenuOpen()) {
        // DRAG del item-slider (AddFloat): el down sobre la fila lo armo (Click); mientras
        // el boton siga apretado, el arrastre horizontal EDITA el valor en vivo. Mismo dispatch
        // que el click (Ejecutar en el menu TOP) para que las acciones del item corran igual.
        // No resalta otras filas ni auto-cierra el menu mientras se arrastra.
        if (MenuSliderDragActive()) {
            if (leftMouseDown) {
                int idSl = MenuSliderDragMove(mx);
                if (idSl >= 0 && OpenMenu) OpenMenu->Run(idSl);
                g_redraw = true;
                return true;
            }
            MenuSliderDragRelease(); // up perdido (solto fuera de la ventana): cortar el drag
        }
        // la BARRA DE SCROLL de un menu largo, agarrada: sigue al puntero (mouse o dedo) hasta soltar
        if (MenuScrollDragActive()) {
            if (leftMouseDown) { MenuScrollDragMove(my); g_redraw = true; return true; }
            MenuScrollDragRelease();
        }
        // DRAG-SCROLL: si el boton esta apretado sobre un menu scrollable, arrastrar mueve la LISTA (no resalta ni
        // cambia de menu). Un umbral chico distingue drag de tap.
        if (g_menuDrag) {
            int dy = my - g_menuDragY0;
            if (!g_menuDragMoved && (dy > 6 * GlobalScale || dy < -6 * GlobalScale)) g_menuDragMoved = true;
            if (g_menuDragMoved) {
                int rowH = RowHeightGS + gapGS;
                int s = g_menuDragScroll0 - dy / rowH; // arrastrar hacia ABAJO muestra items ANTERIORES (grab & pull)
                int ms = g_menuDrag->MaxScroll(); if (s < 0) s = 0; if (s > ms) s = ms;
                g_menuDrag->scroll = s; g_redraw = true;
            }
            return true;
        }
        // el cursor esta SOBRE el desplegable abierto (su lista o cualquier submenu abierto)? Entonces el movimiento
        // es DEL MENU: NO tocar las barras/viewport de atras. Sin este guard, un desplegable dibujado ENCIMA de la
        // barra de otro panel disparaba LayoutOpenBarMenu/HoverArbol al pasar el mouse -> cambiaba/cerraba el
        // menu y resaltaba botones de atras (el hover "se colaba" al viewport).
        bool overMenu = false;
        for (PopupMenu* mm = OpenMenu; mm && mm->open; mm = mm->openSubmenu){
            if (mm->Contains(mx, my)){ overMenu = true; break; }
            if (!mm->openSubmenu || !mm->openSubmenu->open) break;
        }
        // (un menu que no se abrio en el mouse -el popup centrado de la pantalla externa- no se cambia por el de una
        // barra al pasarle por encima: se cierra solo eligiendo o con un click afuera)
        if (!overMenu && (!OpenMenu || OpenMenu->closeFar)){
            // el cursor SALIO del menu (esta sobre una barra): permitir el SLIDE entre menus de barra sin click
            ViewportBase* underVp = FindViewportUnderMouse(rootViewport, mx, my);
            LayoutOpenBarMenu(underVp, mx, my);
            LayoutHoverTree(rootViewport, underVp, mx, my);
        }
        // hover de las opciones + auto-cierre si el mouse se aleja
        if (OpenMenu) OpenMenu->MouseMove(mx, my);
        return true; // el menu se queda con el movimiento
    }
    ViewportBase* under = FindViewportUnderMouse(rootViewport, mx, my);
    ViewportBase* leaf = (under && under->isLeaf()) ? under : NULL;
    // con varios viewports 3D el ACTIVO es el de abajo del mouse
    if (leaf && leaf->ViewportKind() == 1) {
        Viewport3DActive = (Viewport3D*)leaf;
    }
    // hover de la barra de scroll: la MISMA zona que usa el agarre del
    // click (un "scrollbar area" reservada al borde del panel), tambien contra el borde de la ventana
    {
        ViewportBase* barLeaf = leaf ? leaf : (!under ? LeafAtWindowEdge(rootViewport, mx, my) : NULL);
        Scrollable* sc = AsScrollable(barLeaf);
        if (sc) {
            bool zoneV = false, zoneH = false;
            ScrollBarZone(barLeaf, sc, mx, my, zoneV, zoneH);
            sc->mouseOverScrollY = zoneV;
            sc->mouseOverScrollX = zoneH;
        }
    }
    LayoutHoverTree(rootViewport, leaf, mx, my);
    return false;
}

bool LayoutClickUI(int mx, int my) {
    if (!rootViewport) return false;
    if (PopUpActive) {
        if (PopUpActive->Click(mx, my)) return true; // adentro: el popup lo maneja
        // afuera: cerrarlo. Si es semi-modal (redo-panel) el click ademas cae al
        // viewport (selecciona/orbita); si es modal, se consume y listo. Uno NO modal (Buscar del IDE) sigue
        // abierto y el click va a lo de abajo.
        if (!PopUpActive->NoModal()) {
            bool semimodal = PopUpActive->ClosesWithViewport();
            PopUpActive->Close();
            if (!semimodal) return true;
        }
    }
    if (LayoutMenuOpen()) {
        // menu SCROLLABLE (ej: 129 clips): NO seleccionar en el DOWN -> diferir al UP para poder arrastrar y
        // scrollear (el UP decide: tap=selecciona, drag=solo scrolleo). Menus cortos: click normal en el down.
        if (LayoutMenuDragStart(mx, my)) return true;
        // el menu se queda con el click (adentro o el que lo cierra)
        PopupMenu* m = OpenMenu;
        int id = m->Click(mx, my);
        if (id >= 0) m->Run(id);   // accion propia del item (declarativo) o el action(id) viejo
        return true;
    }
    // un click con la barra de scroll agarrada la SUELTA (Symbian)
    if (gScrollBarDrag) {
        ScrollBarDragRelease();
        return true;
    }

    ViewportBase* under = FindViewportUnderMouse(rootViewport, mx, my);
    // contra el BORDE DE LA VENTANA (el margen que no es de ningun viewport): solo la barra de scroll que termina ahi
    if (!under) return ScrollBarGrab(LeafAtWindowEdge(rootViewport, mx, my), mx, my);
    if (!under->isLeaf()) return false;

    // 0) la barra de HERRAMIENTAS (abajo), COMPARTIDA: 3D (historial/orientacion/ejes/tilde-cruz),
    // UV editor y Editor 2D (G/R/S). El hit + despacho por rol viven en ViewportBase (ToolbarBase.cpp);
    // un viewport sin toolbar devuelve false y el click sigue su camino normal.
    if (under->ToolbarClick(mx, my)) return true;

    // 0b) FILA DE BARRAS DEL PINCEL (radio | valor), justo arriba de la toolbar. El down solo ARMA el
    // gesto: si despues hay arrastre mueve el valor en vivo (LayoutMotionUI) y si no lo hubo, al soltar
    // abre la edicion numerica (LayoutRelease). Consumir aca es lo que evita que el click PINTE.
    if (under->BrushBarClick(mx, my)) return true;

    // 1) la barra de botones del viewport
    if (under->BarClick(mx, my)) {
        if (LayoutClickButtonType(under, mx, my)) return true;
        if (under->ViewportKind() == 3) {
            ((Properties*)under)->ClickTab(mx, my); // pestanias Objeto/Mesh
        } else if (under->ViewportKind() == 4) {
            LayoutClickBarUV((UVEditor*)under, mx, my); // boton "View" -> checkboxes
        } else if (under->ViewportKind() == 5) {
            ((Timeline*)under)->ClickBarButton(mx, my); // transporte + campos Start/End
        } else if (under->ViewportKind() == 6) {
            LayoutClickBar2D((Editor2D*)under, mx, my); // boton "Add" del editor 2D
        } else if (under->ViewportKind() == 8) {
            LayoutClickBarIDE((IDE*)under, mx, my); // selector de script / Save / Refresh
        } else if (VpIsOutliner(under) && ((Outliner*)under)->ClickBar(mx, my)) {
            // outliner: lo directo de su barra (el interruptor lista / cuadricula de la biblioteca)
        } else {
            // transporte (Stop/Play) SOLO por click real; si no fue transporte, abrir el menu
            if (!LayoutTransportBar3D(under, mx, my)) LayoutOpenBarMenu(under, mx, my); // Select/Add/Object/Overlays
        }
        return true;
    }

    // 2) la BARRA DE SCROLL: el click la agarra (antes caia en las
    // filas del panel y era imposible arrastrarla)
    if (ScrollBarGrab(under, mx, my)) return true;

    // 3) los paneles
    if (under->ViewportKind() == 3) {
        ((Properties*)under)->ClickIn(mx, my);
        return true;
    }
    if (VpIsOutliner(under)) {
        ((Outliner*)under)->ClickSelect(mx, my);
        return true;
    }
    if (under->ViewportKind() == 8) {
        // IDE: el click posiciona el cursor de texto (shift extiende la seleccion)
        ((IDE*)under)->ClickContent(mx, my, LShiftPressed);
        return true;
    }
    return false; // 3D: la seleccion/transform la maneja la plataforma
}

// DOBLE CLICK / DOBLE TAP (controls.cpp: el clicks == 2 de SDL, o dos toques seguidos): el outliner renombra
// en linea la fila bajo el puntero (las dos vistas). true = lo uso alguien.
bool LayoutDoubleClickUI(int mx, int my) {
    if (PopUpActive || LayoutMenuOpen()) return false;
    ViewportBase* under = FindViewportUnderMouse(rootViewport, mx, my);
    if (!under || !under->isLeaf()) return false;
    if (VpIsOutliner(under)) return ((Outliner*)under)->DoubleClick(mx, my);
    if (under->ViewportKind() == 3) return ((Properties*)under)->DoubleClick(mx, my);   // (una fila de una lista: renombrarla)
    return false;
}
// PULSACION LARGA (tactil: el dedo quieto ~0,5 s sobre un panel): el outliner AGARRA la fila (despues se
// arrastra, o al soltar sin mover abre el menu contextual). true = lo uso alguien (el gesto deja de ser scroll).
bool LayoutLongPressUI(int mx, int my) {
    if (PopUpActive || LayoutMenuOpen()) return false;
    ViewportBase* under = FindViewportUnderMouse(rootViewport, mx, my);
    if (!under || !under->isLeaf()) return false;
    if (VpIsOutliner(under)) {
        if (!((Outliner*)under)->PressLong(mx, my)) return false;
        viewPortActive = under;
        return true;
    }
    return false;
}

// algun panel de propiedades esta editando? (tiene el foco del teclado)
static Properties* LayoutPropsEditing(ViewportBase* aNode) {
    if (!aNode) return NULL;
    if (aNode->isLeaf()) {
        if (aNode->ViewportKind() == 3 && ((Properties*)aNode)->editing) {
            return (Properties*)aNode;
        }
        return NULL;
    }
    ViewportBase* a = NULL;
    ViewportBase* b = NULL;
    if (aNode->ContainerKind() == 1) {
        a = ((ViewportRow*)aNode)->childA;
        b = ((ViewportRow*)aNode)->childB;
    } else {
        a = ((ViewportColumn*)aNode)->childA;
        b = ((ViewportColumn*)aNode)->childB;
    }
    Properties* r = LayoutPropsEditing(a);
    if (r) return r;
    return LayoutPropsEditing(b);
}

// el submenu ABIERTO mas profundo (el que tiene el foco del teclado)
static PopupMenu* LayoutDeepMenu() {
    PopupMenu* deep = OpenMenu;
    while (deep && deep->openSubmenu && deep->openSubmenu->open)
        deep = deep->openSubmenu;
    return deep;
}

// cierra SOLO el submenu mas profundo (vuelve al padre). true si habia uno.
static bool LayoutCloseSubmenuDeep() {
    if (!OpenMenu || !OpenMenu->open) return false;
    PopupMenu* parent = NULL; PopupMenu* deep = OpenMenu;
    while (deep->openSubmenu && deep->openSubmenu->open) {
        parent = deep; deep = deep->openSubmenu;
    }
    if (!parent) return false; // no hay submenu abierto, solo el menu raiz
    deep->Close();
    parent->openSubmenu = NULL;
    return true;
}

// flecha MANTENIDA (frame-based del keypad N95): rutea al popup activo SOLO para ajustar valores
// (el popup decide; el color picker ajusta R/G/B/A o circulo/value, los demas no hacen nada). false si no hay popup.
bool LayoutPopupRepeat(int key) {
    if (PopUpActive) return PopUpActive->KeyRepeat(key);
    return false;
}

bool LayoutKeyUI(int key, int mx, int my) {
    if (!rootViewport) return false;

    if (PopUpActive) {
        return PopUpActive->Key(key); // popup modal
    }

    // un menu desplegable abierto se queda con el teclado. El foco va al submenu
    // ABIERTO mas profundo; izq/der dependen de la opcion resaltada (slider ->
    // mueve el valor; submenu -> abre/cierra; comun -> cambia de menu de barra).
    if (LayoutMenuOpen()) {
        PopupMenu* deep = LayoutDeepMenu();
        MenuItem* it = deep ? deep->CurrentItem() : NULL;
        switch (key) {
            case LayoutKey::Up:   if (deep) deep->button_up();   return true;
            case LayoutKey::Down: if (deep) deep->button_down(); return true;
            case LayoutKey::Right:
                if (it && it->valueFloat) { deep->FitSlider(+1); return true; }
                if (it && it->submenu)    { deep->OpenSubmenuCurrent(); return true; }
                if (viewPortActive && viewPortActive->ViewportKind() == 4) LayoutChangeMenuBarUV(+1); // editor UV: sus menus
                else LayoutChangeMenuBar(+1);                                                         // 3D: opcion comun, menu de al lado
                return true;
            case LayoutKey::Left:
                if (it && it->valueFloat) { deep->FitSlider(-1); return true; }
                if (deep != OpenMenu)  { LayoutCloseSubmenuDeep(); return true; }
                if (viewPortActive && viewPortActive->ViewportKind() == 4) LayoutChangeMenuBarUV(-1);
                else LayoutChangeMenuBar(-1);
                return true;
            case LayoutKey::Enter: {
                // Enter() recorre hasta el submenu mas profundo: abre el submenu
                // de la fila o selecciona/togglea y cierra la cadena si es terminal
                PopupMenu* m = OpenMenu;
                int id = m->Enter();
                if (id >= 0) m->Run(id);   // accion propia del item (declarativo) o el action(id) viejo
                return true;
            }
            case LayoutKey::Cancel:
                OpenMenu->Close();
                return true;
        }
        return true; // mientras este abierto no le roban teclas
    }

    // CUALQUIER campo de texto enfocado (PropFloat numerico O PropText de nombre/save/path): el teclado va al
    // campo. Enter CONFIRMA y sale, Cancel descarta, izq/der = caret. Los caracteres los inyecta el contenedor
    // Symbian (T9) o SDL_TEXTINPUT (PC). Antes solo miraba NumEditActive(): un PropText no marcaba edicion y las
    // flechas colapsaban la tarjeta / OK no salia -> quedabas atorado.
    if (g_activeTextField) {
        switch (key) {
            // al aceptar/cancelar hay que SALIR de la edicion del panel (editando=false): sino button_up/down
            // seguirian navegando/ajustando y te clavaban en la propiedad.
            case LayoutKey::Enter:
                if (RenameActive()) RenameCommit(); else if (NumEditActive()) NumEditCommit(); else g_activeTextField = NULL;
                if (PropsActive) PropsActive->editing = false; g_redraw = true; return true;
            case LayoutKey::Cancel:   // descarta: un campo comun vuelve al texto de antes (TextFieldCancel)
                if (RenameActive()) RenameCancel(); else if (NumEditActive()) NumEditCancel(); else TextFieldCancel();
                if (PropsActive) PropsActive->editing = false; g_redraw = true; return true;
            case LayoutKey::Left:   g_activeTextField->CaretLeft(); g_redraw = true; return true;
            case LayoutKey::Right:  g_activeTextField->CaretRight(); g_redraw = true; return true;
        }
        return true; // mientras edita, consume el resto (no navega ni ajusta la escena)
    }

    // editando una propiedad: ese panel tiene el foco SIN importar hover
    Properties* pe = LayoutPropsEditing(rootViewport);
    if (pe) {
        switch (key) {
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
        switch (key) {
            case LayoutKey::Up:    p->button_up();    return true;
            case LayoutKey::Down:  p->button_down();  return true;
            case LayoutKey::Left:  p->button_left();  return true;
            case LayoutKey::Right: p->button_right(); return true;
            case LayoutKey::Enter: p->key_down_return(); return true;
        }
        return false;
    }
    if (VpIsOutliner(under)) {
        // vista de RECURSOS: flechas/OK recorren y eligen su lista (no la seleccion de la escena). En PC
        // esta es LA puerta de las flechas (controls.cpp las manda aca antes que a event_key_down): el
        // modo de PC (Shift+flechas suma, sin Shift la fila queda sola); en el N95, el del keypad.
#ifndef W3D_SYMBIAN
        if (((Outliner*)under)->view != W3D_VIEW_SCENE) return ((Outliner*)under)->KeyResourcesPC(key);
#else
        if (((Outliner*)under)->view != W3D_VIEW_SCENE) return ((Outliner*)under)->KeyResources(key);
#endif
        switch (key) {
            case LayoutKey::Up:
                changeSelect(SelectMode::PrevSingle, true);
                return true;
            case LayoutKey::Down:
                changeSelect(SelectMode::NextSingle, true);
                return true;
            case LayoutKey::Left:
                OutlinerCollapseLeft();   // plegar; si ya plegado, subir al padre
                return true;
            case LayoutKey::Right:
                SetExpanded(true);
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
bool LayoutKeyPanelActive(int key) {
    // CUALQUIER campo de texto enfocado (PropFloat numerico O PropText de nombre/save/path): Enter CONFIRMA y sale,
    // Cancel descarta, izq/der = caret (los caracteres los mete el contenedor / T9). Va ANTES de todo asi el keypad
    // no navega la escena mientras se tipea. Antes solo NumEditActive() -> un PropText quedaba atorado.
    if (g_activeTextField) {
        switch (key) {
            // al aceptar/cancelar hay que SALIR de la edicion del panel (editando=false): sino button_up/down siguen
            // navegando/ajustando -> te quedabas CLAVADO en la propiedad. (Bug reportado en Symbian.)
            case LayoutKey::Enter:
                if (RenameActive()) RenameCommit(); else if (NumEditActive()) NumEditCommit(); else g_activeTextField = NULL;
                if (PropsActive) PropsActive->editing = false; g_redraw = true; return true;
            case LayoutKey::Cancel:   // descarta: un campo comun vuelve al texto de antes (TextFieldCancel)
                if (RenameActive()) RenameCancel(); else if (NumEditActive()) NumEditCancel(); else TextFieldCancel();
                if (PropsActive) PropsActive->editing = false; g_redraw = true; return true;
            case LayoutKey::Left:   g_activeTextField->CaretLeft(); g_redraw = true; return true;
            case LayoutKey::Right:  g_activeTextField->CaretRight(); g_redraw = true; return true;
        }
        return true;
    }
    if (!viewPortActive || !viewPortActive->isLeaf()) return false;
    // MENU/desplegable abierto: el panel NO maneja teclas (las navega el menu via LayoutKeyUI). Sin esto, con un
    // menu abierto el OK caeria a la accion del panel (o al caso 6/8 nuevo) en vez de activar el item del menu.
    if (LayoutMenuOpen()) return false;
    if (viewPortActive->ViewportKind() == 3) {
        Properties* p = (Properties*)viewPortActive;
        switch (key) {
            case LayoutKey::Up:    p->button_up();        return true;
            case LayoutKey::Down:  p->button_down();      return true;
            case LayoutKey::Left:
                // en las pestañas, izquierda en la 1ra abre el menu tipo/split ([0])
                if (p->focusInTabs && p->activeTab == 0) LayoutOpenMenuType(p);
                else p->button_left();
                return true;
            case LayoutKey::Right: p->button_right();     return true;
            case LayoutKey::Enter: p->key_down_return();  return true;
        }
        return false;
    }
    if (VpIsOutliner(viewPortActive)) {
        Outliner* out = (Outliner*)viewPortActive;
        // MODO MOVER (sin mouse, N95): las flechas reordenan/reparentan el objeto
        // activo en vez de navegar; OK confirma; C/backspace/Esc cancela.
        if (out->ModeMove()) {
            switch (key) {
                case LayoutKey::Up:     out->MoveStep(0);      return true;
                case LayoutKey::Down:   out->MoveStep(1);      return true;
                case LayoutKey::Left:   out->MoveStep(2);      return true; // izquierda = SACAR (unparent)
                case LayoutKey::Right:  out->MoveStep(3);      return true; // derecha = METER (parent)
                case LayoutKey::Enter:  out->MoveConfirm();  return true;
                case LayoutKey::Cancel: out->MoveCancel();   return true;
            }
            return true; // en modo mover se traga todo (que nada mas se cuele)
        }
        // VISTA DE RECURSOS (sin mouse, N95): flechas = recorrer la lista / plegar carpetas, OK = elegir
        // (Properties lo muestra) o plegar. Se come todo menos C, asi el OK nunca cae al Edit Mode del 3D.
        if (out->view != W3D_VIEW_SCENE) {
            // (C solo se come si solto una seleccion de varias; si no, sigue su camino)
            const bool used = out->KeyResources(key);
            return used || key != LayoutKey::Cancel;
        }
        // DOS OK SEGUIDOS (como el doble click): renombrar el objeto activo en su fila. El primero sigue su camino
        if (key == LayoutKey::Enter) {
            static unsigned int sLastMs = 0;
            extern unsigned int w3dGetTicks();
            const unsigned int now = w3dGetTicks();
            if (sLastMs && now - sLastMs < 450 && ActiveObj) {
                sLastMs = 0;
                if (out->RenameObjectInLine(ActiveObj)) return true;
            }
            sLastMs = now;
        }
        switch (key) {
            case LayoutKey::Up:    changeSelect(SelectMode::PrevSingle, true); out->EnsureVisible(); return true;
            case LayoutKey::Down:  changeSelect(SelectMode::NextSingle, true); out->EnsureVisible(); return true;
            case LayoutKey::Left:  OutlinerCollapseLeft(); return true; // plegar / subir al padre
            case LayoutKey::Right: SetExpanded(true);  return true;
        }
        return false;
    }
    if (viewPortActive->ViewportKind() == 1 && (ViewportBase*)Viewport3DActive == viewPortActive) {
        // 3D en FOCO DE BARRA sobre el transporte Stop/Play (sin menu abierto -garantizado por el return de
        // arriba-): flechas ciclan por la barra, OK dispara el transporte, C sale del foco. Solo engancha si el
        // foco esta REALMENTE en Stop/Play -> si quedo un foco viejo en un menu-boton, cae al 'return false' de
        // abajo y el 3D orbita/edita como siempre (sin regresion).
        Viewport3D* v3 = Viewport3DActive;
        // WEIGHT PAINT: la FILA DE BARRAS (radio | valor) se navega con el keypad. Arriba/abajo turnan
        // entre las dos, OK entra a ajustarlas y ahi izquierda/derecha mueven el valor, C suelta el foco.
        // Se queda SOLO con las teclas que usa: sin foco, izq/der/C siguen su camino de siempre.
        if (v3->BrushBarVisible() && BrushBarKey(v3, key)) return true;
        std::vector<Button*>& B = v3->BarButtons;
        int fi = v3->barFocusIndex;
        // exige VISIBLE: si quedo un foco viejo sobre un Stop/Play ya oculto (al salir del modo juego se ponen
        // visible=false), NO enganchar -> cae al 'return false' y el 3D orbita/edita normal (sin tragar teclas).
        if (fi >= 0 && fi < (int)B.size() && B[fi]->visible && IsButtonTransport(B[fi])) {
            switch (key) {
                case LayoutKey::Left:   LayoutChangeMenuBar(-1); return true;
                case LayoutKey::Right:  LayoutChangeMenuBar(+1); return true;
                case LayoutKey::Enter:  LayoutTransportBar3D(v3, B[fi]->sx + B[fi]->width / 2, B[fi]->sy + B[fi]->height / 2); return true;
                case LayoutKey::Cancel: v3->barFocusIndex = -1; g_redraw = true; return true;
            }
            return true;
        }
        return false; // sin foco de transporte: el 3D sigue como antes (orbita / edit mode)
    }
    if (viewPortActive->ViewportKind() == 4) { // UV editor: las flechas PANEAN la vista
        UVEditor* uv = (UVEditor*)viewPortActive;
        // FOCO en el boton "Edit" de la barra (accion directa, sin menu; ver LayoutChangeMenuBarUV):
        // OK entra/sale de la pintura de textura, izq/der siguen navegando la barra, C suelta el foco.
        if (LayoutFocusInUVEdit(uv) && !LayoutMenuOpen()) {
            switch (key) {
                case LayoutKey::Enter:  UVToggleEditTexture(uv); uv->barFocusIndex = -1; g_redraw = true; return true;
                case LayoutKey::Cancel: uv->barFocusIndex = -1; g_redraw = true; return true;
                case LayoutKey::Left:   LayoutChangeMenuBarUV(-1); return true;
                case LayoutKey::Right:  LayoutChangeMenuBarUV(+1); return true;
            }
            return false;
        }
        const float pp = (float)GlobalScale * 16.0f;
        switch (key) {
            case LayoutKey::Left:  uv->Pan(+pp, 0); return true;
            case LayoutKey::Right: uv->Pan(-pp, 0); return true;
            case LayoutKey::Up:    uv->Pan(0, +pp); return true;
            case LayoutKey::Down:  uv->Pan(0, -pp); return true;
        }
        return false;
    }
    if (viewPortActive->ViewportKind() == 5) { // Timeline
        Timeline* tl = (Timeline*)viewPortActive;
        if (tl->barFocusIndex >= 0) { // foco de barra (soft-izq): flechas mueven el foco, OK activa, C sale
            // FOCO SOBRE EL SELECTOR DE ANIMACION: arriba/abajo CAMBIAN de animacion
            // directamente, sin abrir el menu (pedido N95: ciclar rapido para irlas
            // viendo; abrir el menu, entrar a Escenas y buscar la siguiente por CADA
            // animacion era un dolor de cabeza). Abajo = siguiente (como bajar en la
            // lista del menu), arriba = anterior. En los demas botones las flechas
            // verticales siguen sin hacer nada (como antes).
            if ((key == LayoutKey::Up || key == LayoutKey::Down) &&
                tl->barFocusIndex < (int)tl->BarButtons.size() &&
                tl->BarButtons[tl->barFocusIndex] == tl->btnAnim && tl->btnAnim->visible) {
                extern void AnimSelCycle(int dir);   // Properties.cpp (selector compartido)
                AnimSelCycle(key == LayoutKey::Down ? +1 : -1);
                return true;
            }
            // ARRIBA / ABAJO en la barra (play, atras...): al PANEL DE CURVAS, a navegarlo como el outliner
            if ((key == LayoutKey::Up || key == LayoutKey::Down) && tl->PanelFocusEnter(key == LayoutKey::Down ? +1 : -1)) { g_redraw = true; return true; }
            switch (key) {
                case LayoutKey::Left:   LayoutTimelineBarMove(-1); return true;
                case LayoutKey::Right:  LayoutTimelineBarMove(+1); return true;
                case LayoutKey::Enter:  LayoutTimelineBarActivate(); return true;
                case LayoutKey::Cancel: tl->barFocusIndex = -1; g_redraw = true; return true;
            }
            return false;
        }
        // EL FOCO EN EL PANEL DE CURVAS: arriba/abajo recorren (arriba de la primera vuelve a la barra), izq/der
        // pliegan / despliegan, OK sale (la fila queda elegida). La C sigue de largo: borra lo elegido
        if (tl->panelFocus >= 0) {
            switch (key) {
                // pasandose de la primera o de la ultima: a la BARRA (la fila queda elegida); otra vez, al otro extremo
                case LayoutKey::Up:
                case LayoutKey::Down:
                    if (!tl->PanelFocusMove(key == LayoutKey::Down ? +1 : -1)) { tl->panelFocus = -1; tl->barFocusIndex = 0; LayoutTimelineBarMove(+1); }
                    return true;
                case LayoutKey::Left:  tl->PanelFocusCollapse(true); return true;
                case LayoutKey::Right: tl->PanelFocusCollapse(false); return true;
                case LayoutKey::Enter: tl->panelFocus = -1; g_redraw = true; return true;
            }
            return false;
        }
        if (key == LayoutKey::Enter) { tl->TogglePlay(+1); return true; } // sin foco: OK = play (flechas -> NavFrame)
        return false;
    }
    // Editor2D (6) e IDE (8): las flechas navegan los botones de la BARRA (foco tipo Timeline), OK activa el enfocado
    // (abre su menu / Save / Refresh / cambia el tipo de viewport), C sale del foco. Consumen SIEMPRE asi el OK NUNCA
    // cae al toggle de Edit Mode del cubo 3D (bug: apretar OK en el IDE metia el cubo seleccionado en Edit Mode).
    if (viewPortActive->ViewportKind() == 6 || viewPortActive->ViewportKind() == 8) {
        switch (key) {
            case LayoutKey::Left:
            case LayoutKey::Up:     LayoutBarFocusMove(viewPortActive, -1); return true;
            case LayoutKey::Right:
            case LayoutKey::Down:   LayoutBarFocusMove(viewPortActive, +1); return true;
            case LayoutKey::Enter:  LayoutBarFocusActivate(viewPortActive);   return true;
            case LayoutKey::Cancel: viewPortActive->barFocusIndex = -1; g_redraw = true; return true;
        }
        return true;
    }
    // Consola (7): las flechas SCROLLEAN el contenido (vertical y horizontal). El resto se consume (no toca la escena).
    if (viewPortActive->ViewportKind() == 7) {
        Console* c = (Console*)viewPortActive;
        int step = RowHeightGS * 2;
        switch (key) {
            case LayoutKey::Up:    c->ScrollByTouch(0, +step); g_redraw = true; return true;
            case LayoutKey::Down:  c->ScrollByTouch(0, -step); g_redraw = true; return true;
            case LayoutKey::Left:  c->ScrollByTouch(+step, 0); g_redraw = true; return true;
            case LayoutKey::Right: c->ScrollByTouch(-step, 0); g_redraw = true; return true;
        }
        return true;
    }
    return false;
}

// nav del editor UV con flechas MANTENIDAS (frame-based, Symbian): paneo CONSTANTE y suave (sin aceleracion,
// por diseno), o ZOOM centrado si 0 esta mantenido (0 + arriba/abajo). Devuelve true si el viewport activo
// es el UV editor (asi el caller -ApplyArrows3D- corta y no orbita la camara 3D).
bool LayoutUVNavFrame(int dx, int dy, bool zoomMode) {
    if (!viewPortActive || !viewPortActive->isLeaf() || viewPortActive->ViewportKind() != 4) return false;
    UVEditor* uv = (UVEditor*)viewPortActive;
    if (zoomMode) {
        if (dy != 0) uv->ZoomCenter(dy < 0 ? 1 : -1); // 0 + arriba (dy<0) = acercar
    } else {
        const float pp = (float)GlobalScale * 6.0f;   // paso constante chico por frame (suave)
        if (dx || dy) uv->Pan(-(float)dx * pp, -(float)dy * pp); // signos = iguales al per-key (Left=+pp)
    }
    return true;
}

// nav del Timeline con flechas MANTENIDAS (frame-based, Symbian): izq/der = mover el frame actual (scrub),
// 0-mantenido + arriba/abajo = ZOOM centrado, * -mantenido + izq/der = PANEO de la vista. Devuelve true si el
// viewport activo es el Timeline (asi ApplyArrows3D corta y no orbita la camara 3D). Con dx=dy=0 no hace
// nada pero igual devuelve true -> sirve de query "el activo es el Timeline?".
// ---------------------------------------------------------------------------------------------------------------
//  CLICK y TECLA al viewport que corresponde. Es el ruteo que PC ya tenia (main/controls.cpp) y que en el telefono
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
    Notify(T(Viewport3DActive->lockOrbit ? "Orbit locked" : "Orbit unlocked"), false);
    g_redraw = true;
    return true;
}

bool LayoutClickViewport(int mx, int my){
    ViewportBase* vp = FindViewportUnderMouse(rootViewport, mx, my);
    if (!vp) return false;
    viewPortActive = vp;
    lastMouseX = mx; lastMouseY = my;   // button_left() los lee: son SU idea de donde ocurrio el click
    // el boton queda APRETADO hasta el soltar (como el mouse de la PC): sin esto, mantener el OK y mover el cursor no
    // arrastraba nada (el timeline arranca el arrastre de un keyframe solo con el izquierdo apretado)
    extern bool leftMouseDown; leftMouseDown = true;
    vp->button_left();
    return true;
}
bool LayoutReleaseViewport(int mx, int my){
    extern bool leftMouseDown; leftMouseDown = false;
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
// traducir: la tecla es la APRETADA y pasa por los atajos reasignados (app/W3dShortcuts.h). false = ya es la tecla por
// defecto de una funcion (el telefono manda su 1/2/3 como la g/r/s de PC) y no hay que traducirla otra vez.
bool LayoutKeyViewport(int key, bool repeat, bool translate){
    if (!viewPortActive || key == W3dK_NOTHING) return false;
    W3dTempMods mods;
    if (translate) {
        int ctx[6]; const int n = W3dShortcutContexts(ctx);
        const int m0 = W3dCurrentMods(); int m = m0;
        if (!W3dShortcutTranslate(ctx, n, key, m, repeat)) return true;   // combinacion sin funcion (o corrio su accion): se traga
        if (m != m0) mods.Set(m);
    }
    viewPortActive->event_key_down(key, repeat);
    return true;
}
bool LayoutKeyViewportUp(int key){
    if (!viewPortActive || key == W3dK_NOTHING) return false;
    viewPortActive->event_key_up(key);
    return true;
}

bool LayoutTimelineNavFrame(int dx, int dy, bool zoomMode, bool panMode) {
    if (!viewPortActive || !viewPortActive->isLeaf() || viewPortActive->ViewportKind() != 5) return false;
    Timeline* tl = (Timeline*)viewPortActive;
    if (tl->barFocusIndex >= 0) return false; // foco de barra activo (soft-izq): las flechas navegan la barra, no scrollean
    if (panMode) {                                       // * + flechas = panear la vista
        // ARRANCA LENTO y va acelerando mientras se mantiene (como mover / rotar / escalar con las flechas): un cuarto
        // de la velocidad de antes al principio, 2.5 veces al segundo y medio. Se reinicia al soltar (un rato sin llamadas)
        static unsigned int sLast = 0; static int sRamp = 0;
        extern unsigned int w3dGetTicks();
        const unsigned int now = w3dGetTicks();
        if (now - sLast > 150) sRamp = 0;
        sLast   = now;
        float t = (float)sRamp / 45.0f; if (t > 1.0f) t = 1.0f;
        const float f = 0.25f + 2.25f * t * t;
        sRamp++;
        if (dx) tl->PanFrames((float)dx * 1.5f * f);     // horizontal: frames
        // vertical: en CURVAS panea el VALOR; en dope sheet scrollea las filas. Antes arriba/abajo con * no
        // hacia NADA (solo se miraba dx).
        if (dy){
            if (tl->mode == Timeline::TL_MODE_CURVES)   // (arriba = ver valores mas altos: estaba al reves)
                tl->PanValue(-(float)dy * 8.0f * f / (tl->pxPerUnit > 1e-6f ? tl->pxPerUnit : 1e-6f));
            else {
                // KEYFRAMES (dope sheet): las FILAS (curvas, sonidos) para arriba / abajo, con la misma rampa y sin pasarse
                // del final (antes iba al reves y se podia correr sin limite)
                tl->PosY -= (int)((float)dy * 8.0f * f);
                const int total = (int)tl->dopeVisible * tl->rowH, height = tl->height - tl->stripY;
                const int min = height < total ? height - total : 0;
                if (tl->PosY < min) tl->PosY = min;
                if (tl->PosY > 0) tl->PosY = 0;
                g_redraw = true;
            }
        }
    } else if (zoomMode) {                               // 0 + flechas = zoom
        // los DOS ejes por separado: izq/der = tiempo, arriba/abajo = VALOR (solo en curvas, que es donde el eje
        // vertical significa algo). Antes arriba/abajo tambien hacian zoom HORIZONTAL, que no tiene sentido.
        if (dx) tl->ZoomBy(dx > 0 ? 1.06f : 0.94f, tl->CenterTimeline()); // centro del STRIP (excluye el panel)
        if (dy){
            if (tl->mode == Timeline::TL_MODE_CURVES) tl->ZoomVBy(dy < 0 ? 1.06f : 0.94f);
            else tl->ZoomBy(dy < 0 ? 1.06f : 0.94f, tl->CenterTimeline()); // dope sheet: no hay eje vertical propio
        }
    } else {                                             // flechas solas
        // izq/der = mover el frame actual (scrub). El paso depende del ZOOM: siempre ~10px en pantalla, asi
        // recorrer la animacion cuesta lo mismo con cualquier zoom.
        if (dx) tl->ScrubArrow(dx > 0 ? 1 : -1);
        // arriba/abajo NO van aca: saltar de keyframe es UNO POR PULSACION y esta funcion la llama la repeticion de
        // flecha mantenida (cada frame). Va como tecla al viewport activo, desde el key-down.
    }
    return true;
}

// ---- Timeline: navegacion de la BARRA de transporte por teclado (Symbian, sin mouse). soft-izq entra/sale del
// modo foco (barFocusIndex>=0); flechas mueven el foco entre los botones VISIBLES (salteando el [0] tipo/split y
// los ocultos); OK activa el enfocado via ClickBarButton (play/inicio/fin/Start/End/dropdown de animacion). Con el
// foco activo, LayoutTimelineNavFrame devuelve false -> las flechas navegan la barra en vez de scrollear. ----
static Timeline* ActiveTimelinePtr() {
    if (viewPortActive && viewPortActive->isLeaf() && viewPortActive->ViewportKind() == 5) return (Timeline*)viewPortActive;
    return NULL;
}
void LayoutTimelineBarMove(int dir) {
    Timeline* tl = ActiveTimelinePtr(); if (!tl) return;
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
    tl->UpdateBar();                        // auto-scroll para mostrar el enfocado (RenderBar centra el foco)
    g_redraw = true;
}
void LayoutTimelineBarToggle() {
    Timeline* tl = ActiveTimelinePtr(); if (!tl) return;
    if (tl->panelFocus >= 0) { tl->panelFocus = -1; g_redraw = true; return; }   // (del panel: sale del todo)
    if (tl->barFocusIndex >= 0) tl->barFocusIndex = -1;   // salir del modo foco
    else { tl->barFocusIndex = 0; LayoutTimelineBarMove(+1); } // entrar: primer boton de transporte (izq llega al [0] tipo)
    g_redraw = true;
}
void LayoutTimelineBarActivate() {
    Timeline* tl = ActiveTimelinePtr(); if (!tl) return;
    int idx = tl->barFocusIndex;
    if (idx < 0 || idx > (int)tl->BarButtons.size() - 1) return;
    tl->UpdateBar();                        // sx/sy frescos antes del hit-test
    if (idx == 0) { LayoutOpenMenuType(tl); return; } // [0] = menu tipo/split: cambiar el viewport a otro (3D/outliner/etc.)
    Button* b = tl->BarButtons[idx];
    if (b->visible) tl->ClickBarButton(b->sx + b->width / 2, b->sy + b->height / 2);
}

// ====================================================================
// overlay: el desplegable abierto, encima de todo
// ====================================================================

// FPS compartido: cada plataforma lo llama UNA vez por frame dibujado con su reloj de pared en ms (PC:
// SDL_GetTicks; Symbian: User::NTickCount). Mide el TIEMPO ENTRE FRAMES REALES con promedio exponencial ->
// g_currentFps = 1000/ms-por-frame. Refleja el costo REAL del frame en curso (liviano=UI/escena vacia -> fps
// alto; pesado=skinning en play -> fps bajo): son valores CORRECTOS, no un bug, el numero sigue a lo que se dibuja.
void LayoutTickFPS(unsigned long wallMs) {
    static unsigned long lastFrame = 0;
    static float frameMsProm = 0.0f; // ms por frame (promedio exponencial)
    if (lastFrame == 0) { lastFrame = wallMs; return; } // 1er frame: sin delta todavia
    unsigned long dt = wallMs - lastFrame;
    lastFrame = wallMs;
    // Render EVENT-DRIVEN: si nada cambia (quieto) NO se dibujan frames. Al retomar, el 1er frame trae un HUECO
    // enorme (todo el tiempo quieto) que NO es "lento" -> no promediarlo (marcaria fps bajos falsos). Se deja
    // g_currentFps en el ultimo valor ACTIVO (lo que rinde CUANDO dibuja). Un render continuo >=2.5fps no entra aca.
    if (dt == 0 || dt > 400) return;
    if (frameMsProm <= 0.0f) frameMsProm = (float)dt;
    else                     frameMsProm = frameMsProm * 0.8f + (float)dt * 0.2f; // suavizado: estable y responsivo
    g_currentFps = 1000.0f / frameMsProm;
}

// Tab (PC) / tecla equivalente (Symbian BT): alterna Object <-> Edit Mode del
// objeto ACTIVO si es una malla. Devuelve true si alterno (sino el caller hace
// otra cosa, p.ej. ciclar el viewport). La logica va aca -> PC y Symbian la usan.
bool LayoutToggleEditMode() {
    if (state != editNavigation) return false; // no en medio de un transform
    if (!ActiveObj) return false;
    // ARMATURE: Tab alterna Object <-> Pose (antes Tab no hacia nada con un esqueleto seleccionado).
    // Desde el EDIT de huesos, Tab tambien sale (a Pose): cierra el grab pendiente y prepara el
    // skin autorado (el rig editado tiene que deformar apenas se posa).
    if (ActiveObj->getType() == ObjectType::armature) {
        UndoCaptureMode(); // Ctrl+Z: guarda el modo PREVIO
        if (InteractionMode == EditMode){
            if (BoneGrabActive()) BoneGrabCancel();
            PrepareAuthoredSkin((Armature*)ActiveObj);
        }
        InteractionMode = (InteractionMode == PoseMode) ? ObjectMode : PoseMode;
        UpdateActiveEditMesh();
        return true;
    }
    if (ActiveObj->getType() == ObjectType::curve) {
        // CURVE (riel/path): Tab alterna la edicion de su malla INTERNA de nodos.
        if (InteractionMode != EditMode) CurveEnterEdit((Curve*)ActiveObj);
        else { InteractionMode = ObjectMode; UpdateActiveEditMesh(); }   // el watchdog vuelca y cierra
        return true;
    }
    if (ActiveObj->getType() != ObjectType::mesh) return false;
    // una malla de una LIBRERIA (la genera un proxy o usa una malla de una libreria): de solo lectura
    if (InteractionMode != EditMode && IsFromLibNotice(ActiveObj)) return false;
    // malla NO EDITABLE (escenario): sin datos de edicion, el Tab no entra. El boton
    // "Convertir en mesh editable" de la card Mesh la reabre (recalcula bordes/edges).
    if (InteractionMode != EditMode && ((Mesh*)ActiveObj)->noEditable) {
        Notify(T("Mesh not editable (Convert to editable mesh, Mesh card)"), true);
        return false;
    }
    UndoCaptureMode(); // Ctrl+Z: guarda el modo PREVIO antes de togglear
    InteractionMode = (InteractionMode == EditMode) ? ObjectMode : EditMode; // MALLA: Object <-> Edit
    UpdateActiveEditMesh(); // refresca g_editMesh (PC + Symbian)
    return true;
}


void LayoutRenderMenu(int screenW, int screenH) {
    // (lo ultimo de CADA cuadro de la UI, PC y Symbian) el foco de texto que se puso a mano en este
    // cuadro anota su texto de antes, asi un Esc posterior lo descarta; y el campo que un Esc
    // descarto ya lo re-escribieron sus duenos en vivo (ver TextFieldCancel)
    TextFieldFrameEnd();
    // el recurso de la BIBLIOTECA que se arrastra afuera del outliner (al 3D, a Properties): junto al puntero,
    // con los destinos resaltados (Outliner.h, g_outDrag). Encima de todo, menos de los menus y popups.
    OutlinerDragRender(screenW, screenH);
    bool hasMenu = LayoutMenuOpen();
    if (!hasMenu && !PopUpActive) return;

    if (PopUpActive) PopUpActive->Render(); // popup modal (color picker)
    if (!hasMenu) return;
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
    OpenMenu->Render();
}

// ====================================================================
//  VIDA DEL ARBOL (LayoutTree.h): la raiz completa, re-anclar los activos y el gancho
//  que suelta los punteros de ESTE archivo a un viewport que muere
// ====================================================================
ViewportBase* LayoutCompleteRoot() {
    return g_savedRoot ? g_savedRoot : rootViewport;
}

void LayoutAnchorActive() {
    if (!rootViewport) return;
    std::vector<ViewportBase*> leaves;
    LayoutCollectLeaves(rootViewport, leaves);
    if (!viewPortActive) {
        // como el arranque sin archivo (constructor.cpp) y el shell de Symbian: el 3D si hay
        Viewport3D* v3 = LayoutFirst3D(rootViewport);
        viewPortActive = v3 ? (ViewportBase*)v3 : (leaves.empty() ? rootViewport : leaves[0]);
    }
    if (!PropsActive) {
        for (size_t i = 0; i < leaves.size(); i++)
            if (leaves[i]->ViewportKind() == 3) { PropsActive = (Properties*)leaves[i]; break; }
    }
}

// el viewport que muere puede ser el que abrio un menu (los "...De"), el objetivo de un
// menu del UV/2D/IDE, el de la barra de scroll agarrada o el arbol guardado al maximizar.
// Se compara SUBIENDO a ViewportBase*: aca los tipos estan completos.
static void LayoutForgetViewport(ViewportBase* vp) {
    if (!vp) return;
    if (gMenuTypeOf == vp)    gMenuTypeOf = NULL;
    if (g_savedRoot == vp) g_savedRoot = NULL;
    if (gScrollBarDrag == vp) gScrollBarDrag = NULL;
    UVEditor** uvs[] = { &gMenuUVOf, &gUVSnapTarget, &gUVSelectOf, &gUVModeTarget, &gUVTexTarget,
                         &gUVModeOf, &gUVAddOf, &gUVArmOf };
    for (size_t i = 0; i < sizeof(uvs) / sizeof(uvs[0]); i++)
        if (*uvs[i] && (ViewportBase*)*uvs[i] == vp) *uvs[i] = NULL;
    Editor2D** e2d[] = { &gMenu2DPivotOf, &gEditor2DMenu, &gEditor2DView };
    for (size_t i = 0; i < sizeof(e2d) / sizeof(e2d[0]); i++)
        if (*e2d[i] && (ViewportBase*)*e2d[i] == vp) *e2d[i] = NULL;
    if (gIDEMenuOf && (ViewportBase*)gIDEMenuOf == vp) gIDEMenuOf = NULL;
}
struct LayoutHookForget { LayoutHookForget() { ViewportForgetRegister(LayoutForgetViewport); } };
static LayoutHookForget g_layoutHookForget;


// ============================================================================
//  EL CARTELITO DE AYUDA de los botones de SOLO ICONO (Button::ayuda): dejar el mouse quieto encima un momento
//  muestra que es ("Overlays", "Snap"...) en un recuadro chico debajo del boton, y se va solo despues de unos
//  segundos o al salir. Con un menu o un popup abierto no sale (el menu abierto ya lleva su titulo). Se dibuja
//  encima de todo: lo llama NotificationsRender, que corre al final del cuadro en todas las plataformas
// ============================================================================
// DONDE ESTA EL PUNTERO AHORA (lo deja controls.cpp en cada movimiento; -10000 = no hay: se levanto el dedo). El
// cartelito de los botones de adentro de Propiedades miraba lastMouseX/Y, que hoverando no se actualiza: el cartel quedaba
// clavado aunque el mouse ya se hubiera ido
int g_mouseHoverX = -10000, g_mouseHoverY = -10000;
static Button* HelpButtonUnderMouse(ViewportBase* n) {
    if (!n) return NULL;
    if (n->isLeaf()) {
        for (size_t i = 0; i < n->BarButtons.size(); i++) {
            Button* b = n->BarButtons[i];
            if (b && b->visible && b->hover && !b->help.empty()) return b;
        }
        // (la TOOLBAR de abajo: sus botones de solo icono tambien dicen que son)
        if (n->ToolbarVisible())
            for (size_t i = 0; i < n->ToolButtons.size(); i++) {
                Button* b = n->ToolButtons[i];
                if (b && b->visible && b->hover && !b->help.empty()) return b;
            }
        // (Properties: los botones chicos de los mapas del material, adentro de sus filas)
        if (n->ViewportKind() == 3) { extern Button* PropsHelpIn(Properties* p, int mx, int my); return PropsHelpIn((Properties*)n, g_mouseHoverX, g_mouseHoverY); }
        return NULL;
    }
    ViewportBase* a = NULL; ViewportBase* c = NULL;
    if (n->ContainerKind() == 1)      { a = ((ViewportRow*)n)->childA;    c = ((ViewportRow*)n)->childB; }
    else if (n->ContainerKind() == 2) { a = ((ViewportColumn*)n)->childA; c = ((ViewportColumn*)n)->childB; }
    Button* b = HelpButtonUnderMouse(a);
    return b ? b : HelpButtonUnderMouse(c);
}
void W3dHelpRender(int screenW, int screenH) {
    static Button* sButton = NULL;
    static double sFrom = 0.0;
    const double kWait  = 600.0, kHard = 3500.0;   // ms: cuanto hay que quedarse encima, y cuanto se ve
    Button* b = (OpenMenu && OpenMenu->open) || PopUpActive ? NULL : HelpButtonUnderMouse(rootViewport);
    const double now = W3dNowMs();
    if (b != sButton) { sButton = b; sFrom = now; }
    if (!b) return;
    const double t = now - sFrom;
    if (t < kWait) { g_redraw = true; return; }   // (todavia no: el cuadro siguiente vuelve a mirar el reloj)
    if (t > kWait + kHard) return;                 // ya se mostro: hasta que el mouse se vaya y vuelva
    g_redraw = true;                               // (para que se vaya solo al cumplir el tiempo)
    const std::string txt = T(b->help.c_str());
    const int pad = gapGS * 2;
    const int w = UTF8_Length(txt) * CharacterWidthGS + 2 * pad;
    const int h = (int)RowHeightGS + 2 * GlobalScale;
    int x0 = b->sx;
    int y0 = b->sy + b->height + 2 * GlobalScale;              // debajo del boton...
    if (y0 + h > screenH) y0 = b->sy - h - 2 * GlobalScale;    // ...o arriba si es una barra de abajo
    if (x0 + w > screenW) x0 = screenW - w;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    w3dEngine::Viewport(0, 0, screenW, screenH);
    w3dEngine::Disable(w3dEngine::ScissorTest);
    w3dEngine::MatrixMode(w3dEngine::Projection); w3dEngine::LoadIdentity();
    w3dEngine::Ortho(0, screenW, screenH, 0, -1, 1);
    w3dEngine::MatrixMode(w3dEngine::ModelView); w3dEngine::LoadIdentity();
    w3dEngine::Disable(w3dEngine::DepthTest); w3dEngine::Disable(w3dEngine::Lighting); w3dEngine::Disable(w3dEngine::Fog);
    w3dEngine::Enable(w3dEngine::Blend); w3dEngine::BlendAlpha();
    w3dEngine::EnableArray(w3dEngine::VertexArray); w3dEngine::EnableArray(w3dEngine::TexCoordArray);
    w3dEngine::DisableArray(w3dEngine::NormalArray); w3dEngine::DisableArray(w3dEngine::ColorArray);
    static Rec2D* background = NULL;
    if (!background) background = new Rec2D();
    w3dEngine::Disable(w3dEngine::Texture2D);
    SetColorID(ColorID::grayUI, 1.0f);                          // el borde (1 px)
    background->SetSize((GLshort)(x0 - GlobalScale), (GLshort)(y0 - GlobalScale), (GLshort)(w + 2 * GlobalScale), (GLshort)(h + 2 * GlobalScale));
    background->RenderObject(false);
    SetColorID(ColorID::background, 1.0f);
    background->SetSize((GLshort)x0, (GLshort)y0, (GLshort)w, (GLshort)h);
    background->RenderObject(false);
    w3dEngine::Enable(w3dEngine::Texture2D);
    if (!Textures.empty()) w3dEngine::BindTexture(Textures[0]->iID);
    SetColorID(ColorID::white, 1.0f);
    w3dEngine::PushMatrix();
    w3dEngine::Translatef((GLfloat)(x0 + pad), (GLfloat)(y0 + GlobalScale), 0);
    RenderBitmapText(txt);
    w3dEngine::PopMatrix();
}

// ============================================================================
// ATAJOS DE LAS OPERACIONES SIN TECLA DE FABRICA (app/W3dShortcuts.h): las de los menus de la vista 3D, el editor UV y
// el 2D. La combinacion que el usuario les asigna corre la MISMA accion que la opcion del menu, sobre el viewport
// que tiene el foco (el que recibe el teclado).
// ============================================================================
static UVEditor* ShortcutUV() {
    return (viewPortActive && viewPortActive->isLeaf() && viewPortActive->ViewportKind() == 4) ? (UVEditor*)viewPortActive : NULL;
}
static Editor2D* Shortcut2D() {
    return (viewPortActive && viewPortActive->isLeaf() && viewPortActive->ViewportKind() == 6) ? (Editor2D*)viewPortActive : NULL;
}
static void ShortcutUVView(int id)  { UVEditor* uv = ShortcutUV(); if (!uv) return; gMenuUVOf = uv; LayoutActionUVView(id); g_redraw = true; }
static void ShortcutUVCursor(int id) { UVEditor* uv = ShortcutUV(); if (!uv) return; gUVSnapTarget = uv; LayoutActionUVSnap(id); g_redraw = true; }
static void Shortcut2DObject(int id) { Editor2D* ed = Shortcut2D(); if (!ed) return; gEditor2DMenu = ed; LayoutAction2DObject(id); g_redraw = true; }
static void Shortcut2DView(int id)  { Editor2D* ed = Shortcut2D(); if (!ed) return; gEditor2DView = ed; LayoutAction2DView(id); g_redraw = true; }
// (las acciones de los menus de la 3D operan sobre Viewport3DActive: que sea la vista que recibe el teclado)
static void ShortcutFocus3D() {
    if (viewPortActive && viewPortActive->isLeaf() && viewPortActive->ViewportKind() == 1) Viewport3DActive = (Viewport3D*)viewPortActive;
}
static void ShortcutObject(int id)   { ShortcutFocus3D(); LayoutActionObject(id); g_redraw = true; }
static void ShortcutView3D(int id)  { ShortcutFocus3D(); LayoutActionView(id); g_redraw = true; }
static void ShortcutShading(int id){ ShortcutFocus3D(); LayoutActionRender(id); g_redraw = true; }
static struct RegistryShortcutsLayout {
    RegistryShortcutsLayout() {
        W3dShortcutRegisterDispatch("obj", ShortcutObject);
        W3dShortcutRegisterDispatch("view3d", ShortcutView3D);
        W3dShortcutRegisterDispatch("shading", ShortcutShading);
        W3dShortcutRegisterDispatch("uvvista", ShortcutUVView);
        W3dShortcutRegisterDispatch("uvcursor", ShortcutUVCursor);
        W3dShortcutRegisterDispatch("2dobj", Shortcut2DObject);
        W3dShortcutRegisterDispatch("2dvista", Shortcut2DView);
    }
} sRegistryShortcutsLayout;
