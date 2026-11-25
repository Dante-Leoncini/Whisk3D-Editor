#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#ifdef _WIN32
    #include <windows.h>
#endif

#include "Outliner.h"
#include "app/W3dTouchEditor.h"   // W3D_ONLY_BY_CONTROLS (Symbian: el soltar de PC solo con el dedo o el mouse)
#include "WhiskUI/draw/glesdraw.h"
#include "render/RenderOptions.h" // g_redraw (CenterSelection pide redibujar)
#include "objects/ObjectMode.h" // reparent del drag&drop
#include "objects/Instance.h"   // ObjectIcon: array/mirror/instance segun el modo
#include "io/PrefabsEditor.h"   // lo generado por una instancia de prefab: en gris, sin arrastre ni mudanza
#include "objects/Mesh.h"       // Mesh::armatures2d (una fila VIRTUAL azul por rig 2D bajo la malla)
#include "objects/Routine.h"    // la Rutina mal armada va en rojo, con su motivo
#include "W3dLang.h"
#include "config/W3dProfile.h"   // W3dNowMs: el auto-scroll del arrastre va por tiempo
#include "ViewPorts/PopUp/ConfirmPopup.h" // OpenConfirmDelete (confirmar antes de borrar)
#include "ViewPorts/Notifications.h"      // Notificar (renames del modo mover)
#include "Undo.h"                         // UndoCaptureRenames (1 solo paso al confirmar el mover)
#include "WhiskUI/widgets/Button.h"          // los botones de la barra: vista y acciones (por recursos)
#include <cstdio>                           // snprintf (el aviso de los renames)
#include <cstring>                          // strlen
#include "io/Prefabs.h"             // W3dPrefabIsScene: el icono de una escena instanciada
#include "objects/InstancePrefab.h"
#include "W3dRoots.h"              // varias escenas abiertas: una fila por escena arriba del arbol
#include "io/EditorRoots.h"        // W3dActivateRootOf: un click en otra escena abierta la activa
#ifdef W3D_SYMBIAN
    #include <GLES/gl.h>
    extern int W3dScreenHeight;  // alto de pantalla (flip de Y; glesdraw.cpp)
    extern int ShiftCount;
    extern bool middleMouseDown;
    extern bool MouseWheel;
#else
    #include <GL/gl.h>
#endif

// El icono del objeto se DERIVA de su tipo (Fase D: el core ya no guarda iconos de UI).
// Mapeo tipo de objeto -> IconType (el catalogo de iconos vive en la UI, que el editor SI ve).
int W3dRoutineIcon(Routine* r);   // (edit/RoutineEditor.cpp)
size_t ObjectIcon(Object* o) {
    switch (o->getType().v) {
        case ObjectType::mesh:       // no editable (escenario) = CUBO/objeto; editable = el de mesh de siempre
            return (size_t)(((Mesh*)o)->noEditable ? IconType::object : IconType::mesh);
        case ObjectType::light:      return (size_t)IconType::light;
        case ObjectType::camera:     return (size_t)IconType::camera;
        case ObjectType::collection: return (size_t)IconType::archive;
        case ObjectType::empty:      return (size_t)IconType::empty;
        case ObjectType::armature:   return (size_t)IconType::armature;
        case ObjectType::curve:      return (size_t)IconType::curve;
        case ObjectType::mirror:     return (size_t)IconType::mirror;
        case ObjectType::script:     return (size_t)IconType::gamepad;
        case ObjectType::instance: { // array / mirror / instance segun el modo
            Instance* in = (Instance*)o;
            if (in->mirror)    return (size_t)IconType::mirror;
            if (in->count > 1) return (size_t)IconType::array;
            return (size_t)IconType::instance;
        }
        case ObjectType::lod:        return (size_t)IconType::array;     // LOD: niveles de detalle
        case ObjectType::culling:    return (size_t)IconType::visible;   // Culling: que se ve / que se corta
        case ObjectType::fog:     return (size_t)IconType::fog;          // Niebla (glFog en el arbol)
        case ObjectType::sound:     return (size_t)IconType::sound;      // Sonido (el parlante)
        case ObjectType::clearz:   return (size_t)IconType::clearz;
        case ObjectType::postProcess: return (size_t)IconType::monitor;
        case ObjectType::clip:    return (size_t)IconType::clip;
        case ObjectType::hitbox:     return (size_t)IconType::hitbox;     // Hitbox: caja de deteccion
        case ObjectType::lightProbe:   return (size_t)IconType::sphere;     // Sonda de luz: la luz indirecta horneada
        case ObjectType::reflectionProbe: return (size_t)IconType::photo;    // Sonda de reflejo: una foto de 360 grados
        case ObjectType::prefab:     // instancia de prefab (sus hijos: generados); la de una ESCENA, el icono de escena
            return W3dPrefabIsScene(((InstancePrefab*)o)->prefab) ? (size_t)IconType::camera : (size_t)IconType::prefab;
        case ObjectType::proxy:      return (size_t)IconType::lib;    // proxy W3D: lo que genera es de una libreria
        case ObjectType::particles: return (size_t)IconType::circle;  // Particulas: emisor (puntitos redondos)
        case ObjectType::routine:     return (size_t)W3dRoutineIcon((Routine*)o);   // Rutina: lista de pasos (la de UN paso, el de su grupo)
        case ObjectType::ui:         return (size_t)IconType::texture;   // interfaz 2D
        case ObjectType::image2d:   return (size_t)IconType::photo;      // elemento imagen 2D
        case ObjectType::rect2d:     return (size_t)IconType::plane;     // elemento rectangulo 2D
        case ObjectType::cont2d:     return (size_t)IconType::folder;    // contenedor 2D (ordena hijos)
        case ObjectType::slice9:     return (size_t)IconType::grid;     // imagen con bordes fijos
        case ObjectType::button2d:    return (size_t)IconType::object;  // boton de interfaz
        case ObjectType::expand2d: return (size_t)IconType::arrowRight; // resorte de layout
        case ObjectType::video2d:    return (size_t)IconType::camera;   // video (sin sonido)
        default:                     return (size_t)IconType::archive;
    }
}

// ===================================================================================================
//  FILAS VIRTUALES "Armature 2D": los huesos 2D NO son objetos de escena, viven DENTRO de la malla
//  (Mesh::armatures2d). El outliner lo AVISA con UNA fila informativa POR ARMATURE 2D (una malla
//  puede tener varios rigs 2D independientes) colgando de la malla y en
//  AZUL (el mismo azul con el que el editor UV dibuja los huesos), para dejar claro que el rig 2D es
//  parte del mesh. Es una fila INERTE: no se selecciona, no se arrastra, no es destino de drop, no
//  se puede desemparentar (no es un objeto) y NO se despliega -- los huesos se listan y se editan en
//  la pestania "Armature 2D" del panel Properties y en el propio editor UV, que es donde se los ve
//  dibujados (tenerlos DOS veces era ruido: decision del dueno).
//  TODOS los recorridos (nombres, ojos, fila<->object, alto del contenido) se alinean con la MISMA
//  cuenta Arm2DCount(obj): no hay conteo de filas duplicado que mantener.
// ===================================================================================================
static const float kArm2DBlue[3] = { 0.28f, 0.55f, 1.00f }; // = el azul de los huesos del UV editor

// la malla con armatures 2D de este objeto (NULL = no aplica; tambien es "suma filas virtuales?")
static Mesh* Arm2DOf(Object* o){
    if (!o || o->getType() != ObjectType::mesh) return NULL;
    Mesh* m = (Mesh*)o;
    return m->HasArm2D() ? m : NULL;
}
// CUANTAS filas virtuales suma este objeto (= cuantos armatures 2D tiene la malla; 0 = ninguna)
static int Arm2DCount(Object* o){ Mesh* m = Arm2DOf(o); return m ? (int)m->armatures2d.size() : 0; }
// 'o' tiene algo que desplegar? (hijos objeto O filas virtuales de armature 2D)
static bool OutHasChildren(Object* o){ return (o->Childrens.size() >= 1) || (Arm2DCount(o) > 0); }

// resultado de ubicar una fila del arbol: un objeto REAL, o la fila VIRTUAL del armature 2D
struct OutRow {
    Object* obj;    // objeto real (NULL si la fila es virtual)
    Mesh*   arm2d;  // != NULL -> fila virtual "Armature 2D" de esta malla
    int     arm2dIdx; // indice del armature 2D de esa fila (solo si arm2d != NULL)
    int     prof;   // nivel de sangria
    OutRow() : obj(NULL), arm2d(NULL), arm2dIdx(-1), prof(0) {}
};

static bool IsConstructor(Object* o) {
    return o && o->getType() == ObjectType::routine && ((Routine*)o)->constructor;
}

// FILTROS de la vista Escena (menu del OJO de la barra): ocultar el Constructor, las demas rutinas, y las columnas de
// iconos de visibilidad (ojo) y de render (camara)
bool g_outShowConstructor = true;
bool g_outShowRoutines = true;
bool g_outShowEyes = true;
bool g_outShowRender = true;
static bool OutHidden(Object* o) {
    if (!o || o->getType() != ObjectType::routine) return false;
    return IsConstructor(o) ? !g_outShowConstructor : !g_outShowRoutines;
}

// ===================================================================================================
//  VARIAS ESCENAS ABIERTAS (W3dRoots.h): con dos o mas, lo de ARRIBA del arbol son sus RAICES, en el orden de la lista
//  (la prioridad del Play): una fila por escena, desplegable, con su ojo (muestra u oculta la escena entera en el
//  viewport) y sus objetos adentro. La de la escena ACTIVA (donde van los objetos nuevos) va resaltada; un click en su
//  nombre la activa y arrastrarla cambia el orden. Un objeto no se muda de escena arrastrandolo. Con una sola escena
//  abierta lo de arriba son los hijos de la activa, como siempre.
// ===================================================================================================
static std::vector<Object*> gOutRoots;   // (se rearma en cada OutLimit: se recorre por indice)
static const std::vector<Object*>& OutLimit() {
    static const std::vector<Object*> empty;
    if (!SceneCollection) return empty;
    gOutRoots.clear();
    std::vector<int> ab;
    W3dOpenRoots(ab);
    if (ab.size() >= 2) {
        const std::vector<W3dRootRow>& fs = W3dRoots();
        for (size_t i = 0; i < ab.size(); i++)
            if (ab[i] >= 0 && ab[i] < (int)fs.size() && fs[(size_t)ab[i]].root) gOutRoots.push_back(fs[(size_t)ab[i]].root);
        if (gOutRoots.size() >= 2) return gOutRoots;
        gOutRoots.clear();
    }
    return SceneCollection->Childrens;
}
// la fila es la de una ESCENA (una raiz del proyecto: no tiene padre; solo aparece arriba con varias abiertas)
static bool OutIsRoot(Object* o) { return o && !o->Parent && W3dObjectRoot(o) >= 0; }
static Object* OutRootOf(Object* o) { while (o && o->Parent) o = o->Parent; return o; }
static std::string OutRootName(Object* r) {
    const int i = W3dObjectRoot(r);
    const std::vector<W3dRootRow>& fs = W3dRoots();
    return (i >= 0 && i < (int)fs.size()) ? fs[(size_t)i].name : r->name;
}
static int OutIconRoot(Object* r) { return W3dRootIcon(W3dRootTypeOf(W3dObjectRoot(r))); }
// arrastrar una escena: va antes o despues de 'junto' en la lista de las abiertas
static void OutMoveScene(Object* root, Object* next, bool after) {
    const int idx = W3dObjectRoot(root), ref = W3dObjectRoot(next);
    if (idx < 0 || ref < 0 || idx == ref) return;
    std::vector<int> ab;
    W3dOpenRoots(ab);
    int pos = 0;
    for (size_t i = 0; i < ab.size(); i++) { if (ab[i] == idx) continue; if (ab[i] == ref) break; pos++; }
    W3dRootMoveOpen(idx, after ? pos + 1 : pos);
}
// ===================================================================================================
//  EL CORTE CONSTRUCTOR / CADA CUADRO: 3 pixeles NEGROS (de la escala de la interfaz) entre lo de arriba -el
//  CONSTRUCTOR, con lo que tenga adentro: corre UNA vez al empezar el juego (en el editor, en cada cuadro: la
//  interfaz pisa su estado)- y lo de abajo, que corre en CADA cuadro (primero "Limpiar pantalla"). No es una fila:
//  corre para abajo las que siguen. Va entre dos hermanos de los que uno es constructor y el otro no (con varias
//  escenas abiertas, uno adentro de cada una). TODA cuenta fila <-> Y del arbol pasa por OutYOfRow / OutRowOfY.
// ===================================================================================================
static std::vector<int> gOutCuts;   // las filas que llevan el corte ARRIBA, en orden (OutCutsBuild)
static int OutCutHeight() { return 3 * GlobalScale; }
static int OutRowsOf(Object* o) {   // las filas visibles de 'o' y lo que tiene desplegado (como la cuenta de Render)
    if (OutHidden(o)) return 0;
    int n = 1;
    if (OutHasChildren(o) && o->expanded) {
        n += Arm2DCount(o);
        for (size_t i = 0; i < o->Childrens.size(); i++) n += OutRowsOf(o->Childrens[i]);
    }
    return n;
}
static void OutCutsSiblings(const std::vector<Object*>& hs, int& row, bool roots) {
    // (despues del ultimo constructor -o de la ultima escena- no hay mas cortes: no se cuentan las filas que siguen)
    size_t last = hs.size();
    for (size_t i = 0; i < hs.size(); i++)
        if (hs[i] && !OutHidden(hs[i]) && (IsConstructor(hs[i]) || (roots && OutIsRoot(hs[i])))) last = i;
    if (last == hs.size()) return;
    bool hasPrev = false, prevConstructor = false;
    for (size_t i = 0; i < hs.size(); i++) {
        Object* o = hs[i];
        if (!o || OutHidden(o)) continue;
        const bool isC = IsConstructor(o);
        if (hasPrev && isC != prevConstructor) gOutCuts.push_back(row);
        hasPrev = true; prevConstructor = isC;
        if (i > last) return;
        if (roots && OutIsRoot(o)) {   // una ESCENA (varias abiertas): su fila y, desplegada, su constructor adentro
            row++;
            if (OutHasChildren(o) && o->expanded) { row += Arm2DCount(o); OutCutsSiblings(o->Childrens, row, false); }
        } else row += OutRowsOf(o);
    }
}
static void OutCutsBuild() {
    gOutCuts.clear();
    if (!SceneCollection) return;
    int row = 0;
    OutCutsSiblings(OutLimit(), row, true);
}
// la Y (desde arriba del arbol) donde empieza la fila 'fila'
static int OutYOfRow(int row) {
    int y = row * (int)RowHeightGS;
    for (size_t i = 0; i < gOutCuts.size() && gOutCuts[i] <= row; i++) y += OutCutHeight();
    return y;
}
// la fila de una Y desde arriba del arbol; 'resto' = la Y adentro de la fila. Sobre un corte: el borde de ABAJO de la
// fila de arriba (un drop ahi va "despues" de ella). Una Y negativa (arriba del arbol) da lo de siempre
static int OutRowOfY(int rel, int* rest = NULL) {
    const int height = (int)RowHeightGS;
    int extra = 0;
    for (size_t i = 0; i < gOutCuts.size(); i++) {
        const int yc = gOutCuts[i] * height + extra;   // el corte empieza aca (arriba de su fila)
        if (rel < yc) break;
        if (rel < yc + OutCutHeight()) {
            int r = yc - 1 - extra; if (r < 0) r = 0;
            if (rest) *rest = r % height;
            return r / height;
        }
        extra += OutCutHeight();
    }
    rel -= extra;
    if (rest) *rest = rel % height;
    return rel / height;
}
// (lo mismo para OutlinerResources.cpp y el harness: rearman los cortes, el arbol pudo cambiar)
int OutlinerYOfRow(int row) { OutCutsBuild(); return OutYOfRow(row); }
int OutlinerRowOfY(int rel) { OutCutsBuild(); return OutRowOfY(rel); }
void OutlinerCuts(std::vector<int>& out) { OutCutsBuild(); out = gOutCuts; }   // (el harness: 'outcortes')

// un objeto de otra escena abierta: su escena pasa a ser la ACTIVA (antes de elegirlo). false = no se pudo (avisado)
bool OutlinerActivateSceneOf(Object* o) {
    Object* r = OutRootOf(o);
    return !r || r == SceneCollection || W3dActivateRootOf(r);
}

// Constructor
static Object* W3dObjectInRow(int row, int* profOut = 0); // profOut (opcional) = profundidad del objeto
static bool W3dRowInTree(int row, OutRow& out);           // fila -> objeto real o fila virtual (false = vacio)
static int W3dRowOf(Object* target);                      // fila visible de un objeto (-1 si esta plegado)
// la Y (desde el tope del contenido) de la fila del objeto ACTIVO; -1 = no tiene fila (plegado). El menu por teclado
int OutlinerYOfActive() {
    if (!ActiveObj) return -1;
    const int f = W3dRowOf(ActiveObj);
    if (f < 0) return -1;
    OutCutsBuild();
    return OutYOfRow(f);
}

Outliner::Outliner() : ViewportBase() {
    Row = new Rec2D();
    RowCount = 5;
    lastContentRows = -1;
    lastCuts = 0;
    hoverRow = -1;
    dragObject = NULL;
    dragging = false;
    dragY0 = 0;
    dragMx = dragMy = 0; autoScrollMs = 0.0; autoScrollRest = 0.0f;
    dropRow  = -1;
    dropZone = -2;
    dropProf = 0;
    moving   = false;
    moveObj  = NULL;
    moveParentOrig = NULL;
    movePrevOrig = NULL;
    // LA BIBLIOTECA (OutlinerResources.cpp): arranca en la ESCENA, el arbol de siempre
    view = OUT_VIEW_SCENE;
    filter = -1;
    onlyUnused = false;
    grid       = false;
    showMemory = true;
    cellZoom = 1.0f;
    enteredMs = 0;
    hoverMx   = hoverMy = -1;
    showSizes = false;   // (se prende desde el ojo: en un outliner angosto le come el ancho a los nombres)
    showType = false;    // (idem: la columna Tipo se prende desde el ojo)
    showUses = true;
    cursorIsFolder = false;
    recDrag = recDragging = false;
    recDragFolder = recOnlyOnRelease = recPickOnRelease = false;
    selMarked = false;
    recDragY0 = recDragX0 = 0; recDropRow = -1;
    recInsRow = -1; recInsAfter = false;
    colWidth[0] = colWidth[1] = colWidth[2] = -1;
    colDrag     = colHover = -1;
    grabRow = -1; grabMoved = false; grabX = grabY = 0;
    renaming = renameFolder = renameObject = false;
    renameSerial = 0;
    renameView = OUT_VIEW_SCENE;
    movingRec = false;
    selSceneSerial = W3dSelectionSerial;
    BarCreate();
    // [1] la VISTA: QUE se ve en el outliner (Escena / Biblioteca / cada libreria), con su nombre y su icono (la
    // camara en la escena, la carpeta en la biblioteca)
    btnView = new Button("", (int)IconType::camera);
    btnView->role = 1;
    btnView->dropdown = true;
    btnView->caretMenu = true;
    BarButtons.push_back(btnView);
    // [1b] el OJO: COMO se ve (lista / cuadricula, la fila de memoria, los tamanos; en la escena, sus columnas)
    btnOptions = new Button("", (int)IconType::visible);
    btnOptions->role = 8;
    btnOptions->dropdown = true;
    btnOptions->help = "View options";
    BarButtons.push_back(btnOptions);
    // [2] el menu OBJETO (borrar, renombrar, mover, ocultar, duplicar... / en la biblioteca: sus acciones)
    btnObject = new Button("", -1);
    btnObject->role = 2;
    btnObject->dropdown = true;
    BarButtons.push_back(btnObject);
    // [3] el menu SELECCION (todo / nada / invertir), en las dos vistas
    btnSelection = new Button("", -1);
    btnSelection->role = 3;
    btnSelection->dropdown = true;
    BarButtons.push_back(btnSelection);
    // [4] UN solo "+" desplegable, como el Add del viewport 3D (icono sin texto: el titulo va en el menu):
    // Nueva escena / Nuevo juego / Nuevo prefab / Nueva carpeta / Nuevo material
    newBtn = new Button("", (int)IconType::plus);
    newBtn->role = 4;
    newBtn->dropdown = true;
    newBtn->help = "Add";
    BarButtons.push_back(newBtn);
    // [5] el FILTRO de la biblioteca (todo / un tipo) y [6] lista / cuadricula: solo en la biblioteca
    btnFilter = new Button("", (int)IconType::filter);
    btnFilter->role = 5;
    btnFilter->dropdown = true;
    btnFilter->square = true;   // (solo el icono, cuadrado y sin la flechita: como el resto de la barra)
    btnFilter->visible = false;
    BarButtons.push_back(btnFilter);
    btnGrid = new Button("", (int)IconType::grid);
    btnGrid->role = 6;
    btnGrid->square = true;
    btnGrid->visible = false;
    BarButtons.push_back(btnGrid);
    // [7] la raiz que se edita (escena / juego / prefab; el icono lo pone W3dRootsButtonSync): en la
    // vista Escena (el arbol que se ve es el de esa raiz)
    btnRoot = new Button("", (int)IconType::camera);
    btnRoot->role = 7;
    btnRoot->dropdown = true;
    btnRoot->caretMenu = true;
    BarButtons.push_back(btnRoot);
    SyncBarView();
}

//para hacer el calculo si o si hay que hacerlo de forma recursiva
void Outliner::ComputeRow(Object* obj, int* MaxPosXtemp, int* MaxPosYtemp){
    if (OutHidden(obj)) return;   // filtrado por el menu del ojo: no ocupa fila
    int rowWidth = marginGS + IconSizeGS + gapGS + IconSizeGS + gapGS + IconSizeGS + marginGS;
    *MaxPosYtemp -= RowHeightGS;
    int textWidth = obj->name.size() * LetterWidthGS;
    rowWidth += textWidth + gapGS;

    // guardar ancho máximo
    if (rowWidth > *MaxPosXtemp) *MaxPosXtemp = rowWidth;

    //si no tiene hijos. o no esta desplegado se ahorra todos los bucles siguentes
    if (!OutHasChildren(obj) || !obj->expanded) return;
    *MaxPosYtemp -= RowHeightGS * Arm2DCount(obj); // una fila virtual por armature 2D

    //std::cout << "textWidth: " << textWidth << " rowWidth: " << rowWidth << std::endl;
    for (size_t o = 0; o < obj->Childrens.size(); o++) {
        ComputeRow(obj->Childrens[o], MaxPosXtemp, MaxPosYtemp);
        /*int rowWidthObj = marginGS + IconSizeGS + gapGS + IconSizeGS + gapGS + IconSizeGS + gapGS + IconSizeGS + marginGS;
        *MaxPosYtemp -= RowHeightGS;

        // texto del objeto
        int textWidthObj = reinterpret_cast<Text*>(obj->Childrens[o]->name->data)->letters.size() * LetterWidthGS;
        rowWidthObj += textWidthObj + gapGS;

        if (rowWidthObj > *MaxPosXtemp) *MaxPosXtemp = rowWidthObj;
        //std::cout << "caracteres obj: " << rowWidthObj << std::endl;*/
    }
}

void Outliner::Resize(int newW, int newH){
    ViewportBase::Resize(newW, newH);
    ResizeBorder(newW, newH);

    // (el ancho de las franjas se ajusta al final, cuando ya se sabe
    // si existe la barra vertical)

    // Calcular cuántos renglones entran en la altura
    // (ceil portable: std::ceil es ambiguo en RVCT)
    RowCount = (size_t)((height + RowHeightGS - 1) / RowHeightGS);

    int MaxPosXtemp = 0;
    int MaxPosYtemp = 0;

    if (view != OUT_VIEW_SCENE) { ResizeResources(); return; }   // la biblioteca
    if (!SceneCollection) {
        ResizeScrollbar(newW, newH, 0, 0, BarTopOffset());
        Row->SetSize(0, 0, (GLshort)width, RowHeightGS);
        return;
    }
    const std::vector<Object*>& limit = OutLimit();
    for (size_t c = 0; c < limit.size(); c++) ComputeRow(limit[c], &MaxPosXtemp, &MaxPosYtemp);
    OutCutsBuild();
    MaxPosYtemp -= OutCutHeight() * (int)gOutCuts.size();   // los cortes constructor / cada cuadro
    //este es el gap para la barra de desplazamiento de abajo
    MaxPosYtemp -= marginGS;
    //std::cout << "MaxPosXtemp: " << MaxPosXtemp << " width: " << width << std::endl;
    //std::cout << "MaxPosYtemp: "<< MaxPosYtemp << std::endl;
    //std::cout << "Ancho: " << newW << " Alto: "<< newH << std::endl;
    ResizeScrollbar(newW, newH, MaxPosXtemp, MaxPosYtemp, BarTopOffset());

    // las franjas le dejan el "scrollbar area" SOLO si hay barra
    // vertical; sin barra el espacio no se desperdicia
    int reserveV = scrollY ? (borderGS + GlobalScale * 9 + 2) : 0;
    Row->SetSize(0, 0, (GLshort)(width - reserveV), RowHeightGS);
}

void Outliner::Render(){
    // la barra al dia (el selector de escena/prefab cambia cuando se abre otra raiz)
    SyncBarView();
    // un renombrar en linea que perdio el foco (Enter / click afuera) se confirma
    if (view == OUT_VIEW_SCENE) RenameSync();
    // el auto-scroll del arrastre de filas (mouse en el borde de la lista o afuera)
    if (view == OUT_VIEW_SCENE && dragging) AutoScrollDrag();
    // AUTO-REFRESH del scrollbar: si cambio la cantidad de FILAS VISIBLES (importar/agregar/borrar/desplegar) se
    // recalcula el rango de scroll. Antes solo se recalculaba al REDIMENSIONAR el viewport -> tras importar objetos
    // el scrollbar quedaba viejo y no se podia scrollear hasta cambiar el tamanio de un viewport a mano.
    // vista de recursos: que filas estan en la SELECCION MULTIPLE (fondo resaltado)
    std::vector<char> rowSel;
    // las filas de la biblioteca: UNA vez por cuadro (RenderResources dibuja estas; armarlas lista la biblioteca
    // entera y con un proyecto grande no es gratis)
    std::vector<OutRowRec> rows;
    if (view != OUT_VIEW_SCENE) {
        // un renombrar en linea que perdio el foco se confirma ANTES de armar las filas (cambia un nombre)
        RenameSync();
        // la lista de recursos: el mismo auto-refresh, contando SUS filas (RowsResources)
        RowsResources(rows);
        if ((int)rows.size() != lastContentRows) { lastContentRows = (int)rows.size(); Resize(width, height); }
        if (SelectionCount() > 1) {
            rowSel.assign(rows.size(), 0);
            for (size_t i = 0; i < rows.size(); i++) rowSel[i] = RowInSelection(rows[i]) ? 1 : 0;
        }
    }
    else if (SceneCollection){
        struct C { static int rec(Object* o){ if (OutHidden(o)) return 0; int n = 1;
            if (OutHasChildren(o) && o->expanded){
                n += Arm2DCount(o);                        // una fila virtual por armature 2D
                for (size_t i = 0; i < o->Childrens.size(); i++) n += rec(o->Childrens[i]);
            }
            return n; } };
        int rows = 0;
        const std::vector<Object*>& limit = OutLimit();
        for (size_t c = 0; c < limit.size(); c++) rows += C::rec(limit[c]);
        OutCutsBuild();
        if (rows != lastContentRows || (int)gOutCuts.size() != lastCuts) {
            lastContentRows = rows; lastCuts = (int)gOutCuts.size(); Resize(width, height);
        }
    }
    w3dEngine::MatrixMode(w3dEngine::Projection);
    w3dEngine::LoadIdentity();

    w3dEngine::MatrixMode(w3dEngine::ModelView);
    w3dEngine::LoadIdentity();

    // arbol con origen ARRIBA-izquierda (4 OS): GL quiere abajo-izquierda
    const int glY = W3dScreenHeight - y - height;
    if (!SceneCollection && view == OUT_VIEW_SCENE) return;

    // Limpiar pantalla
    w3dEngine::Enable(w3dEngine::ScissorTest);
    w3dEngine::Scissor(x, glY, width, height); // igual a tu viewport
    w3dEngine::ClearColor(ColorList[static_cast<int>(ColorID::background)][0],
        ColorList[static_cast<int>(ColorID::background)][1],
        ColorList[static_cast<int>(ColorID::background)][2],
        ColorList[static_cast<int>(ColorID::background)][3]);
    w3dEngine::Clear(w3dEngine::ColorBuffer | w3dEngine::DepthBuffer);
    w3dEngine::Disable(w3dEngine::ScissorTest);

    w3dEngine::Viewport(x, glY, width, height); // x, y, ancho, alto
    w3dEngine::Ortho(0, width, height, 0, -1, 1);

    w3dEngine::Disable(w3dEngine::Fog);
    w3dEngine::Disable(w3dEngine::DepthTest);
    w3dEngine::Disable(w3dEngine::CullFace);
    w3dEngine::Disable(w3dEngine::Lighting);
    w3dEngine::Disable(w3dEngine::Texture2D);
    w3dEngine::Disable(w3dEngine::Blend);
    w3dEngine::Enable(w3dEngine::ColorMaterial);

    w3dEngine::EnableArray(w3dEngine::VertexArray);
    w3dEngine::DisableArray(w3dEngine::TexCoordArray);
    w3dEngine::DisableArray(w3dEngine::NormalArray);

    //de aca en adelante es como antes
    w3dEngine::PushMatrix();
    w3dEngine::Translatef(0, PosY + borderGS + TopContent(), 0);   // (la biblioteca en lista: debajo de los titulos)
    // la franja i es la FILA i de la lista (mismo origen que los nombres: PosY). Con la lista
    // scrolleada se dibujan las que caen en el panel, desde la primera visible: antes solo existian
    // las franjas 0..RowCount-1 y las filas de mas abajo quedaban sin fondo (sin cebra, sin
    // el resaltado de la seleccion multiple ni el del destino de un arrastre). (+1: la de abajo, a medias)
    // (la escena: con los CORTES constructor / cada cuadro -la biblioteca no tiene-)
    if (view != OUT_VIEW_SCENE) gOutCuts.clear();
    const size_t firstRow = (PosY < 0) ? (size_t)OutRowOfY(-PosY) : 0;
    // (la CUADRICULA de la biblioteca no tiene franjas: sus celdas y carpetas pintan su propio fondo)
    const size_t stripes = (view != OUT_VIEW_SCENE && grid) ? 0 : RowCount + 1;
    for (size_t i = firstRow; i < firstRow + stripes; i++) {
        w3dEngine::PushMatrix();
        w3dEngine::Translatef(0, (GLfloat)OutYOfRow((int)i), 0);
        // Renglón Seleccionado
        if ((dragging && dropZone == 1 && (int)i == dropRow) ||
            (view != OUT_VIEW_SCENE && recDragging && (int)i == recDropRow)) {
            // vista previa: este seria el futuro PADRE del drop
            w3dEngine::Color4ub(ColorListUbyte[static_cast<int>(ColorID::accentDark)][0],
                       ColorListUbyte[static_cast<int>(ColorID::accentDark)][1],
                       ColorListUbyte[static_cast<int>(ColorID::accentDark)][2], 255);
        }
        else if (i < rowSel.size() && rowSel[i]) {
            // en la SELECCION MULTIPLE: el acento oscuro, mezclado con el fondo (el acento pleno es el drop)
            const GLubyte* a = ColorListUbyte[static_cast<int>(ColorID::accentDark)];
            const GLubyte* b = ColorListUbyte[static_cast<int>(ColorID::background)];
            w3dEngine::Color4ub((GLubyte)((a[0] * 3 + b[0] * 2) / 5), (GLubyte)((a[1] * 3 + b[1] * 2) / 5),
                                (GLubyte)((a[2] * 3 + b[2] * 2) / 5), 255);
        }
        else if ((int)i == hoverRow) {
            // hover: feedback antes de hacer click
            w3dEngine::Color4ub(ColorListUbyte[static_cast<int>(ColorID::headerColor)][0], ColorListUbyte[static_cast<int>(ColorID::headerColor)][1], ColorListUbyte[static_cast<int>(ColorID::headerColor)][2], 255);
        }
        else if (i % 2 == 0) {
            w3dEngine::Color4ub(ColorListUbyte[static_cast<int>(ColorID::gray)][0], ColorListUbyte[static_cast<int>(ColorID::gray)][1], ColorListUbyte[static_cast<int>(ColorID::gray)][2], 255);
        }
        else {
            // Renglón impar
            w3dEngine::Color4ub(ColorListUbyte[static_cast<int>(ColorID::background)][0], ColorListUbyte[static_cast<int>(ColorID::background)][1], ColorListUbyte[static_cast<int>(ColorID::background)][2], 255);
        }
        //RenderObject2D(*Renglon);
        Row->Render(false);
        w3dEngine::PopMatrix();
    }
    // los CORTES constructor / cada cuadro: 3 pixeles negros arriba de su fila, a lo ancho de las franjas
    if (!gOutCuts.empty()) {
        static Rec2D* cut = NULL;
        if (!cut) cut = new Rec2D();
        const int reserveV = scrollY ? (borderGS + GlobalScale * 9 + 2) : 0;   // (lo mismo que deja Renglon)
        cut->SetSize(0, 0, (GLshort)(width - reserveV), (GLshort)OutCutHeight());
        w3dEngine::Color4ub(0, 0, 0, 255);
        for (size_t i = 0; i < gOutCuts.size(); i++) {
            w3dEngine::PushMatrix();
            w3dEngine::Translatef(0, (GLfloat)(OutYOfRow(gOutCuts[i]) - OutCutHeight()), 0);
            cut->Render(false);
            w3dEngine::PopMatrix();
        }
    }
    w3dEngine::PopMatrix();

    w3dEngine::BindTexture(Textures[0]->iID);
    w3dEngine::EnableArray(w3dEngine::TexCoordArray);
    w3dEngine::Enable(w3dEngine::Texture2D);
    w3dEngine::Enable(w3dEngine::Blend);
    w3dEngine::BlendAlpha();
#ifndef W3D_SYMBIAN
    w3dEngine::TexFilter(false);
    w3dEngine::TexFilter(false);
#endif
    SetColorID(ColorID::grayUI);

    // LA BIBLIOTECA: la lista / cuadricula (carpetas + recursos con su EN USO / HUERFANO) en vez del arbol
    if (view != OUT_VIEW_SCENE) {
        RenderResources(glY, rows);
        RenderBar();
        DrawBorders(this);
        DrawScrollbar(this);
#ifdef W3D_SYMBIAN
        w3dEngine::EnableArray(w3dEngine::NormalArray); // baseline que asume la escena
#endif
        return;
    }

    //esto es para recortar y que no se ponga el texto encima de los ojos de la derecha
    w3dEngine::Enable(w3dEngine::ScissorTest);
    if (scrollX){
        w3dEngine::Scissor(x, glY + marginGS, width - 2*IconSizeGS - gapGS - marginGS - borderGS - gapGS - (scrollY ? (GlobalScale*9 + gapGS) : 0), height - marginGS); // - ojos+camaras+barra
    }
    else {
        w3dEngine::Scissor(x, glY, width - 2*IconSizeGS - gapGS - marginGS - borderGS - gapGS - (scrollY ? (GlobalScale*9 + gapGS) : 0), height); // - ojos+camaras+barra
    }

    cullBaseY = PosY + borderGS + BarTopOffset(); rowDFS = 0; // culling: Y de la 1er fila del recorrido de NOMBRES
    w3dEngine::PushMatrix();
    w3dEngine::Translatef(marginGS + PosX, PosY + borderGS + BarTopOffset(), 0);
    const std::vector<Object*>& limit = OutLimit();
    for (size_t c = 0; c < limit.size(); c++) DrawRow(limit[c], !limit[c]->visible, 0);
    w3dEngine::PopMatrix();

    SetColorID(ColorID::grayUI);

    w3dEngine::PushMatrix();
    //no usa PosX porque los ojos siempre estan en la misma posicion en X. al borde
    int reserveBar = scrollY ? (GlobalScale * 9 + gapGS) : 0;   // solo el ancho de la barra + un gap chico
    w3dEngine::Translatef(width - 2*IconSizeGS - gapGS - marginGS - borderGS - reserveBar, GlobalScale + PosY + borderGS + BarTopOffset(), 0);

    if (scrollX){
        w3dEngine::Scissor(x, glY + marginGS, width - marginGS - borderGS, height - marginGS); // igual a tu viewport - los ojos
    }
    else {
        w3dEngine::Scissor(x, glY, width - marginGS - borderGS, height); // igual a tu viewport - los ojos
    }

    cullBaseY = GlobalScale + PosY + borderGS + BarTopOffset(); rowDFS = 0; // culling: Y de la 1er fila del recorrido de OJOS
    for (size_t c = 0; c < limit.size(); c++) DrawEyes(limit[c], !limit[c]->visible, !limit[c]->renderable);
    w3dEngine::PopMatrix();
    w3dEngine::Disable(w3dEngine::ScissorTest);

    // vista previa del drag: LINEA VERDE donde se va a insertar
    if (dragging && (dropZone == 0 || dropZone == 2)) {
        static Rec2D* line = NULL;
        if (!line) line = new Rec2D();
        int lineY = borderGS + PosY + BarTopOffset()
                    + OutYOfRow(dropRow) + (dropZone == 2 ? (int)RowHeightGS : 0);
        w3dEngine::Disable(w3dEngine::Texture2D);
        SetColorID(ColorID::accent);
        // la linea se INDENTA al nivel del destino: si el objeto va a quedar emparentado (nivel > 0)
        // arranca mas a la derecha y es mas angosta; a la raiz (nivel 0) ocupa todo el ancho.
        int indent = dropProf * (IconSizeGS + gapGS);
        line->SetSize((GLshort)(borderGS + indent), (GLshort)(lineY - GlobalScale),
                       (GLshort)(width - bordersGS - indent), (GLshort)(2 * GlobalScale));
        line->RenderObject(false);
        w3dEngine::Enable(w3dEngine::Texture2D);
    }

    RenderBar();
    DrawBorders(this);
    DrawScrollbar(this);
#ifdef W3D_SYMBIAN
    w3dEngine::EnableArray(w3dEngine::NormalArray); // baseline que asume la escena
#endif
}

// una RUTINA mal armada (el editor la revisa antes de dibujarla; aca tambien, por si no se dibujo): va en ROJO,
// con la marca de error y el motivo despues del nombre
bool W3dRoutineValidate(Routine* r);
static bool OutlinerRoutineInvalid(Object* obj) {
    if (obj->getType() != ObjectType::routine) return false;
    return !W3dRoutineValidate((Routine*)obj);
}

// la MARCA que va despues del nombre de una fila de la escena (-1 = ninguna): el CANDADO en lo que genera un proxy
// de una libreria (a cualquier profundidad: una instancia anidada adentro de lo que genera tambien es de ella)
int OutlinerObjectMark(Object* obj) {
    for (Object* p = W3dInstanceOf(obj); p; p = W3dInstanceOf(p))
        if (p->getType() == ObjectType::proxy) return (int)IconType::lock;
    if (OutlinerRoutineInvalid(obj)) return (int)IconType::notifError;
    return -1;
}

// la fila cae en el area visible (con margen de 1 fila arriba y abajo: no cortar las del borde)
bool Outliner::RowInView(unsigned row) const {
    const int myY = cullBaseY + OutYOfRow((int)row);
    return (myY + (int)RowHeightGS * 2 > 0) && (myY < (int)height + (int)RowHeightGS);
}

void Outliner::DrawRow(Object* obj, bool hidden, int prof){
    if (OutHidden(obj)) return;
    // CULLING: solo se dibuja la fila que cae en el area visible, ubicada por su numero y su profundidad (sin recorrer
    // la matriz por las demas: una coleccion desplegada con 900 hijos eran 12 mil llamadas por cuadro)
    const unsigned row = rowDFS++;
    if (RowInView(row)) {
    w3dEngine::PushMatrix();
    w3dEngine::Translatef(0, (GLfloat)OutYOfRow((int)row), 0);
    // las LINEAS del arbol: una por ancestro (sus columnas, a la izquierda de la flecha)
    if (prof > 0) SetColorID(ColorID::grayUI);
    for (int d = 0; d < prof; d++) {
        W3dDrawStrip4(IconLineMesh, IconsUV[static_cast<size_t>(IconType::line)]->uvs);
        w3dEngine::Translatef(IconSizeGS + gapGS, 0, 0);
    }
    GLfloat opacityRow = hidden ? 0.5f : 1.0f;
    // lo GENERADO por una instancia de prefab va en GRIS (se ve y se elige, pero es del prefab: no se edita)
    if (W3dIsGenerated(obj)) opacityRow *= 0.55f;

    if (OutIsRoot(obj) && !(dragging && obj == dragObject)) {
        // la fila de una ESCENA (varias abiertas): la ACTIVA en blanco
        SetColorID(obj == SceneCollection ? ColorID::white : ColorID::grayUI, opacityRow);
    }
    else if (moving && obj == moveObj){
        // MODO MOVER con teclado: el objeto que se esta moviendo se resalta (accent),
        // para que en el N95 (sin mouse) se vea claro cual se esta reordenando.
        SetColorID(ColorID::accent, opacityRow);
    }
    else if (dragging){
        // mientras se ARRASTRA el outliner entero se ve deseleccionado;
        // solo el objeto arrastrado queda marcado (la seleccion vuelve
        // a verse normal al soltar)
        if (obj == dragObject){
            SetColorID(ColorID::accent, opacityRow);
        } else {
            SetColorID(ColorID::grayUI, opacityRow);
        }
    }
    else if (obj == ActiveObj){
        //std::cout << "Objeto activo en el outliner: " << reinterpret_cast<Text*>(SceneCollection->Childrens[c]->name->data)->value << "\n";
        if (obj->select){
            SetColorID(ColorID::accent, opacityRow);
        }
        else {
            SetColorID(ColorID::white, opacityRow);
        }
    }
    else if (obj->select){
        SetColorID(ColorID::accentDark, opacityRow);
    }
    else {
        SetColorID(ColorID::grayUI, opacityRow);
    }
    const bool routineBad = OutlinerRoutineInvalid(obj);
    if (routineBad) w3dEngine::Color4f(0.92f, 0.28f, 0.24f, opacityRow);   // (el rojo de los errores del editor)

    //icono desplegar (si no tiene hijos: flecha a la derecha). El armature 2D VIRTUAL tambien
    //cuenta como "hijo" (una malla sin hijos objeto pero con huesos 2D se puede desplegar).
    if (!OutHasChildren(obj) || !obj->expanded){
        W3dDrawStrip4(IconMesh, IconsUV[static_cast<size_t>(IconType::arrowRight)]->uvs);
    }
    else {
        W3dDrawStrip4(IconMesh, IconsUV[static_cast<size_t>(IconType::arrow)]->uvs);
    }

    //icono de la coleccion
    w3dEngine::Translatef(IconSizeGS + gapGS, 0, 0);
    W3dDrawStrip4(IconMesh, IconsUV[OutIsRoot(obj) ? OutIconRoot(obj) : ObjectIcon(obj)]->uvs);

    //texto render (o el campo del RENOMBRAR EN LINEA si es el objeto que se esta renombrando)
    w3dEngine::Translatef(IconSizeGS + gapGS, 0, 0);
    if (renaming && renameObject && obj->serial == renameSerial) {
        SetColorID(ColorID::white);
        RenderTextField(rename, g_activeTextField == &rename, textAlign::left, 1920);
    } else if (OutIsRoot(obj)) RenderBitmapText(OutRootName(obj));   // (el nombre de la escena en el proyecto)
    else RenderBitmapText(obj->name);
    // lo que genera un PROXY de una libreria lleva el CANDADO de solo lectura despues del nombre: no se edita ni se
    // guarda, es de su .w3d (OutlinerObjectMark)
    {
        const int mark = OutlinerObjectMark(obj);
        if (mark >= 0) {
            w3dEngine::Translatef((GLfloat)((int)(obj->name.size() + 1) * (int)LetterWidthGS), 0, 0);
            W3dDrawStrip4(IconMesh, IconsUV[(size_t)mark]->uvs);
            // la rutina mal armada dice POR QUE (se calcula al validar: ver main/edit/RoutineEditor.cpp)
            if (routineBad) {
                w3dEngine::Translatef(IconSizeGS + gapGS, 0, 0);
                RenderBitmapText(((Routine*)obj)->reason);
            }
        }
    }

    w3dEngine::PopMatrix();
    } // fin del DRAW de la fila (culling); el traversal de hijos de abajo corre siempre

    //si no tiene hijos. o no esta desplegado se ahorra todos los bucles siguentes
    if (!OutHasChildren(obj) || !obj->expanded) return;
    Mesh* virt = Arm2DOf(obj); // NULL = la malla no tiene armatures 2D (no suma filas)
    for (int k = 0, n = Arm2DCount(obj); k < n; k++)   // los ARMATURES 2D van primero (son DATO de la malla)
        DrawArm2D(virt, k, hidden, prof + 1);
    for (size_t o = 0; o < obj->Childrens.size(); o++)
        DrawRow(obj->Childrens[o], hidden ? true : !obj->Childrens[o]->visible, prof + 1);
}

// fila VIRTUAL de UN armature 2D de la malla: una fila informativa (icono de armature + nombre del
// armature), en AZUL (= parte del mesh, no un objeto de escena) y SIN flechita (no se despliega:
// los huesos se listan en la pestania "Armature 2D" del panel). Ocupa 1 renglon y no mueve la
// matriz. El armature ACTIVO va en azul PLENO y los demas apagados, como en el editor UV.
void Outliner::DrawArm2D(Mesh* m, int idx, bool hidden, int prof){
    if (!m || idx < 0 || idx >= (int)m->armatures2d.size()) return;
    const GLfloat op = (hidden || idx != m->armature2dActive) ? 0.5f : 1.0f; // el ACTIVO, pleno
    const unsigned row = rowDFS++;
    if (!RowInView(row)) return;
    w3dEngine::PushMatrix();
    w3dEngine::Translatef(0, (GLfloat)OutYOfRow((int)row), 0);
    SetColorID(ColorID::grayUI);
    for (int d = 0; d < prof; d++) {   // (las lineas del arbol)
        W3dDrawStrip4(IconLineMesh, IconsUV[static_cast<size_t>(IconType::line)]->uvs);
        w3dEngine::Translatef(IconSizeGS + gapGS, 0, 0);
    }
    w3dEngine::Color4f(kArm2DBlue[0], kArm2DBlue[1], kArm2DBlue[2], op);
    // la columna de la flechita queda VACIA (no hay nada que desplegar): solo se saltea para que
    // el icono y el texto queden alineados con los de las filas de objetos
    w3dEngine::Translatef(IconSizeGS + gapGS, 0, 0);
    W3dDrawStrip4(IconMesh, IconsUV[static_cast<size_t>(IconType::armature)]->uvs);
    w3dEngine::Translatef(IconSizeGS + gapGS, 0, 0);
    RenderBitmapText(m->armatures2d[idx] ? m->armatures2d[idx]->name : std::string(T("Armature 2D")));
    w3dEngine::PopMatrix();
}

void Outliner::DrawEyes(Object* obj, bool hidden, bool noRender){
    if (OutHidden(obj)) return;
    // CULLING: mismo criterio que DrawRow (solo las filas que se ven, ubicadas por su numero)
    const unsigned row = rowDFS++;
    if (RowInView(row)) {
        w3dEngine::PushMatrix();
        w3dEngine::Translatef(0, (GLfloat)OutYOfRow((int)row), 0);
        // OJO (visible): ocultado por si mismo o por un PADRE oculto -> tenue. Sin la columna de la CAMARA, el ojo
        // ocupa su lugar (pegado a la derecha)
        if (!g_outShowRender) w3dEngine::Translatef((GLfloat)(IconSizeGS + gapGS), 0, 0);
        SetColorID(ColorID::grayUI, hidden ? 0.5f : 1.0f);
        if (g_outShowEyes) W3dDrawStrip4(IconMesh, IconsUV[static_cast<size_t>(obj->visible ? IconType::visible : IconType::hidden)]->uvs);
        // CAMARA (renderizar) a la DERECHA del ojo: si el objeto NO se renderiza -> camara_off;
        // si SI pero un PADRE no se renderiza -> camara encendida pero SEMI-TRANSPARENTE (igual que el ojo)
        w3dEngine::Translatef((GLfloat)(IconSizeGS + gapGS), 0, 0);
        SetColorID(ColorID::grayUI, noRender ? 0.5f : 1.0f);
        if (g_outShowRender && !OutIsRoot(obj))   // (una escena no tiene camara de render: solo su ojo)
            W3dDrawStrip4(IconMesh, IconsUV[static_cast<size_t>(obj->renderable ? IconType::camera : IconType::camera_off)]->uvs);
        w3dEngine::PopMatrix();
    }

    //si no tiene hijos. o no esta desplegado se ahorra todos los bucles siguentes
    if (!OutHasChildren(obj) || !obj->expanded) return;

    // las filas VIRTUALES de armature 2D no tienen ojo ni camara (no son objetos), pero SI ocupan fila
    rowDFS += (unsigned)Arm2DCount(obj);

    for (size_t o = 0; o < obj->Childrens.size(); o++){
        DrawEyes(obj->Childrens[o],
                    hidden   ? true : !obj->Childrens[o]->visible,
                    noRender ? true : !obj->Childrens[o]->renderable);
    }
}

void Outliner::button_left(){
    if (mouseOverScrollY){
        mouseOverScrollYpress = true;
    }
}

void Outliner::mouse_button_up(int button){
    W3D_ONLY_BY_CONTROLS();   // (Symbian: el OK del cursor virtual sigue como estaba)
    ViewPortClickDown = false;
    if (button == W3dMB_IZQ) {
        mouseOverScrollYpress = false;
        mouseOverScrollXpress = false;
    }
    //else if (boton == W3dMB_MIDDLE) {
    //    middleMouseDown = false;
    //}
    FindMouseOver(lastMouseX,lastMouseY);
}

// ZOOM de la CUADRICULA (Ctrl + rueda / dos dedos / la fila Zoom del menu Vista): el tamanio de las celdas, entre 60% y
// cuatro veces el de siempre
void OutlinerZoomCells(Outliner* o, float z) {
    if (z < 0.6f) z = 0.6f;
    if (z > 4.0f) z = 4.0f;
    if (z == o->cellZoom) return;
    o->cellZoom = z;
    o->lastContentRows = -1;
    o->Resize(o->width, o->height);
    g_redraw = true;
}
static void ZoomCells(Outliner* o, float factor) {
    float z = o->cellZoom * factor;
    if (z < 0.6f) z = 0.6f;
    if (z > 4.0f) z = 4.0f;
    if (z == o->cellZoom) return;
    o->cellZoom = z;
    o->lastContentRows = -1;
    o->Resize(o->width, o->height);
    g_redraw = true;
}
void Outliner::event_finger_gesture(float zoomDelta, float panDx, float panDy){
    (void)panDx; (void)panDy;
    if (InLibrary() && grid && zoomDelta != 0.0f) ZoomCells(this, 1.0f + zoomDelta * 0.05f);
}
void Outliner::event_mouse_wheel(float dy, int mx, int my){
    {
      if (BarScrollHorizontal(mx, my, (int)(dy * 40))) return; } // sobre la barra -> horizontal
    // la CUADRICULA: la rueda SCROLLEA, como en todos lados; Ctrl + rueda agranda / achica las celdas
    extern bool LCtrlPressed;
    if (InLibrary() && grid && LCtrlPressed) { ZoomCells(this, dy > 0 ? 1.12f : 1.0f / 1.12f); return; }
    MouseWheel = true;
    ScrollY(dy*6*GlobalScale);
    MouseWheel = false;
}

void Outliner::FindMouseOver(int mx, int my){
    // (el hover de las barras lo calcula LayoutInput con la zona del
    // agarre; la llamada vieja a ScrollMouseOver pisaba ese estado)
}

// TOUCH: arrastrar 1 dedo sobre el CONTENIDO = scroll (v/h). La barra la maneja el gesto lockeado.
bool Outliner::event_finger_scroll(int px, int py, int dx, int dy){
    ScrollByTouch(dx, dy);
    return true;
}

void Outliner::event_mouse_motion(int mx, int my) {
    if (hoverMx != mx || hoverMy != my) { hoverMx = mx; hoverMy = my; if (view != OUT_VIEW_SCENE) g_redraw = true; }
    // hover: fila bajo el mouse (feedback antes del click)
    if (view == OUT_VIEW_SCENE) OutCutsBuild(); else gOutCuts.clear();
    hoverRow = OutRowOfY(my - y - borderGS - PosY - BarTopOffset());
    // el "scrollbar area" esta reservada: ahi no hay hover de renglon
    if (mouseOverScrollY || mouseOverScrollX) {
        hoverRow = -1;
    }
    // la BIBLIOTECA: el arrastre de un recurso (a una carpeta, al 3D, a Properties) o el scroll de siempre
    if (view != OUT_VIEW_SCENE) {
        // el DIVISOR de una columna: con el mouse encima se pone blanco; arrastrandolo, cambia el ancho
        if (colDrag >= 0 && leftMouseDown) {
            int xT, xTm, xE, xF; ColumnsList(xT, xTm, xE, xF);
            const int right = colDrag == 0 ? xTm : colDrag == 1 ? xE : xF;   // el borde derecho de esa columna
            int w = right - (mx - x);
            const int min = 3 * CharacterWidthGS;
            if (w < min) w = min;
            if (w > width / 2) w = width / 2;
            colWidth[colDrag] = w / (GlobalScale > 0 ? GlobalScale : 1);
            g_redraw = true;
            return;
        }
        const int hc = (!leftMouseDown && !grid) ? DivisorInPoint(mx, my) : -1;
        if (hc != colHover) { colHover = hc; g_redraw = true; }
        if ((leftMouseDown || grabRow >= 0) && recDrag) {
            MotionResources(mx, my);
            if (recDragging || grabRow >= 0) return;   // mientras se arrastra no scrollea
        }
        if (middleMouseDown || leftMouseDown) { ViewPortClickDown = true; ScrollX(dx); ScrollY(grid ? dy : dy); }
        return;
    }
    // TACTIL: el objeto AGARRADO con la pulsacion larga se arrastra (sin el boton del mouse de por medio)
    if (grabRow >= 0 && dragObject) {
        int dm = my - grabY; if (dm < 0) dm = -dm;
        if (dm > (int)RowHeightGS / 2) grabMoved = true;
    }
    if ((leftMouseDown || grabRow >= 0) && dragObject) {
        int d = my - dragY0;
        if (d < 0) d = -d;
        if (!dragging && d > RowHeightGS / 2) { dragging = true; autoScrollMs = 0.0; autoScrollRest = 0.0f; }
        if (dragging) {
            dragMx = mx; dragMy = my;   // (el auto-scroll del borde lo sigue en Render)
            UpdateDrop(mx, my);
            return; // mientras se arrastra no scrollea
        }
    }
    if (middleMouseDown || leftMouseDown) {
        ViewPortClickDown = true;

        ScrollX(dx);
        ScrollY(dy);
        return;
    }
    //si no se esta haciendo click. entonces miras si el mouse esta encima de algo
    else if (scrollY){
        FindMouseOver(mx, my);
    }
}

void Outliner::event_key_down(int keyCode, bool repeat){
    const int key = keyCode;
    // vista de RECURSOS: sus propias teclas (flechas, Enter, F2, Supr, G = mover a carpeta)
    if (view != OUT_VIEW_SCENE) { KeyPCResources(keyCode); return; }
    if (repeat == 0) {
        // MODO MOVER: las flechas reordenan/reparentan en vez de navegar; OK confirma; C/backspace/Esc cancela.
        if (moving) {
            switch (key) {
                case W3dK_UP:    MoveStep(0); return;
                case W3dK_DOWN:  MoveStep(1); return;
                case W3dK_LEFT:  MoveStep(2); return; // izquierda = SACAR (unparent)
                case W3dK_RIGHT: MoveStep(3); return; // derecha = METER (parent bajo el hermano anterior)
                case W3dK_RETURN: case W3dK_KP_ENTER: MoveConfirm(); return;
                case W3dK_ESCAPE: case W3dK_BACKSPACE: case W3dK_C: MoveCancel(); return;
                default: return; // en modo mover se traga el resto
            }
        }
        switch (key) {
            case W3dK_G: // g = entrar en modo MOVER (reordenar / reparentar el objeto activo)
                MoveStart();
                break;
            case W3dK_F2: // F2 = renombrar EN LINEA el objeto activo (su fila se vuelve un campo)
                RenameObjectInLine(ActiveObj);
                break;
            case W3dK_A:
                if (LAltPressed) SelectionScene(1);   // Alt+A: deseleccionar todo (el atajo del menu Seleccion)
                else SelectAll(true);
                break;
            case W3dK_I:
                if (LCtrlPressed) SelectionScene(2);  // Ctrl+I: invertir la seleccion
                break;
            case W3dK_D:   // los atajos del menu Objeto: Shift+D duplica, Alt+D duplica vinculado
                if (LAltPressed) DuplicateScene(true);
                else if (LShiftPressed) DuplicateScene(false);
                break;
            case W3dK_H:
                UndoCaptureVisibility();   // Ctrl+Z: guarda el 'visible' PREVIO antes de togglear
                ChangeVisibilityObj();
                break;
            case W3dK_X:
            case W3dK_DELETE:   // Supr borra igual que X
                if (state == editNavigation){
                    OpenConfirmDelete(true); // popup de confirmacion (incluye colecciones); Si -> borra con undo
                }
                break;
            case W3dK_LEFT:
                SetExpanded(false);
                break;
            case W3dK_RIGHT:
                SetExpanded(true);
                break;
            case W3dK_UP:
                changeSelect(SelectMode::PrevSingle, true);
                EnsureVisible();
                break;
            case W3dK_DOWN:
                changeSelect(SelectMode::NextSingle, true);
                EnsureVisible();
                break;
            case W3dK_KP_PERIOD: // numpad "." = centrar la seleccion (llega por hover, como todas
                CenterSelection(); // las teclas por-viewport: el ruteo manda al viewport bajo el mouse)
                break;
        };
    }
}

void Outliner::event_key_up(int keyCode){
    const int key = keyCode;
    switch (key) {
        case W3dK_LSHIFT:
        #ifdef W3D_SYMBIAN
            // el TAP de Shift (el lapiz del N95, sin mouse) recorre los objetos: es un idioma del TELEFONO
            if (ShiftCount < 20){
                changeSelect(SelectMode::NextSingle, true);
                EnsureVisible();
            }
        #endif
            // En la PC, Android y la web Shift es SOLO modificador, como en la Vista 3D (ViewPort3D::event_key_up): el
            // tap que cambiaba de objeto saltaba al escribir una mayuscula al RENOMBRAR una fila (la letra la toma el
            // campo de texto, el Shift no se anulaba como modificador y al soltarlo el activo pasaba al primer objeto)
            ShiftCount = 0;
            LShiftPressed = false;
            break;
        case W3dK_LALT:
            LAltPressed = false;
            break;
    }
}

// fila visible N (respetando desplegado) -> objeto REAL o fila VIRTUAL del armature 2D.
// Devuelve true si la fila cayo en este subarbol (out queda cargado).
static bool W3dRowVisibleRec(Object* obj, int& row, int prof, OutRow& out) {
    if (OutHidden(obj)) return false;   // filtrado: no ocupa fila
    if (row == 0) { out.obj = obj; out.arm2d = NULL; out.prof = prof; return true; }
    row--;
    if (obj->expanded) {
        Mesh* am = Arm2DOf(obj);
        for (int k = 0, n = Arm2DCount(obj); k < n; k++) { // una fila por armature 2D (sin hijos)
            if (row == 0) { out.obj = NULL; out.arm2d = am; out.arm2dIdx = k; out.prof = prof + 1; return true; }
            row--;
        }
        for (size_t o = 0; o < obj->Childrens.size(); o++)
            if (W3dRowVisibleRec(obj->Childrens[o], row, prof + 1, out)) return true;
    }
    return false;
}

// fila N del arbol completo: objeto real o fila virtual. false = la fila esta en el VACIO
// (debajo del ultimo renglon) -> ahi el drop desemparenta a la raiz, como siempre.
static bool W3dRowInTree(int row, OutRow& out) {
    if (!SceneCollection) return false;
    const std::vector<Object*>& limit = OutLimit();
    for (size_t c = 0; c < limit.size(); c++)
        if (W3dRowVisibleRec(limit[c], row, 0, out)) return true;
    return false;
}

// objeto en la fila visible N (+ su profundidad en profOut). NULL = vacio o fila VIRTUAL
// (la fila del armature 2D no es un objeto: no se selecciona ni es destino de drop).
static Object* W3dObjectInRow(int row, int* profOut) {
    OutRow f;
    if (!W3dRowInTree(row, f)) return NULL;
    if (profOut) *profOut = f.prof;
    return f.obj;
}

// numero de fila visible de un objeto (-1 si no esta a la vista)
static bool W3dFindRow(Object* obj, Object* target, int* row) {
    if (OutHidden(obj)) return false;
    if (obj == target) return true;
    (*row)++;
    if (obj->expanded) {
        (*row) += Arm2DCount(obj);    // las filas virtuales de armature 2D corren la numeracion
        for (size_t o = 0; o < obj->Childrens.size(); o++) {
            if (W3dFindRow(obj->Childrens[o], target, row)) return true;
        }
    }
    return false;
}

// el objeto de la fila visible N de la escena (NULL = vacio o una fila virtual): lo usan el doble click, el menu
// contextual y la pulsacion larga de OutlinerResources.cpp
// (la fila de una ESCENA no es un objeto: NULL)
Object* OutlinerObjectInRow(int row) { Object* o = row < 0 ? NULL : W3dObjectInRow(row); return OutIsRoot(o) ? NULL : o; }
// la profundidad (sangria) de la fila de un objeto: donde empieza su NOMBRE (-1 = vacio o fila virtual)
int OutlinerRowProf(int row) { int p = 0; return (row >= 0 && W3dObjectInRow(row, &p)) ? p : -1; }
static int W3dRowOf(Object* target);
int OutlinerObjectRow(Object* o) { return W3dRowOf(o); }   // (el harness: la fila de un objeto)

static int W3dRowOf(Object* target) {
    if (!target || !SceneCollection) return -1;
    int row = 0;
    const std::vector<Object*>& limit = OutLimit();
    for (size_t c = 0; c < limit.size(); c++)
        if (W3dFindRow(limit[c], target, &row)) return row;
    return -1;
}

// NUMPAD "." = centrar la SELECCION en la vista,
// por diseno). Scrollea PosY para que la fila objetivo quede en el MEDIO del area de
// contenido. Con varios seleccionados se centra el PROMEDIO de sus filas VISIBLES (un objeto
// dentro de un padre plegado no tiene fila: no aporta); sin seleccion visible cae al objeto
// ACTIVO. Solo scroll vertical: no toca PosX, la seleccion ni el desplegado.
void Outliner::CenterSelection() {
    if (!SceneCollection) return;
    long sum = 0; int n = 0;
    for (size_t i = 0; i < ObjSelects.size(); i++) {
        int f = W3dRowOf(ObjSelects[i]);
        if (f >= 0) { sum += f; n++; }
    }
    if (n == 0 && ActiveObj) {
        int f = W3dRowOf(ActiveObj);
        if (f >= 0) { sum = f; n = 1; }
    }
    if (n == 0) return;                     // nada seleccionado a la vista: no hay que encuadrar
    int row = (int)(sum / n);
    // geometria de las filas (la misma cuenta que Render/ClickSelect):
    //   y_local(fila) = borderGS + PosY + BarTopOffset() + OutYOfRow(fila)
    // se pide y_local(fila) + RowHeightGS/2 == centro del area de contenido.
    int top = BarTopOffset();
    int center = top + (height - top) / 2;
    OutCutsBuild();
    int newValue = center - (int)RowHeightGS / 2 - borderGS - top - OutYOfRow(row);
    if (newValue > 0) newValue = 0;               // no scrollear "antes" del inicio
    if (newValue < MaxPosY) newValue = MaxPosY;   // ni mas alla del final (MaxPosY es <= 0)
    if (newValue != PosY) { PosY = newValue; g_redraw = true; }
}

// AUTO-SCROLL de la navegacion por teclado: el scroll MINIMO para que la fila del
// objeto ACTIVO entre entera en el area de contenido. Si ya se ve, no toca nada (por
// eso no reusa CenterSelection: centrar en cada flecha, con 315 objetos, marea).
// Antes no habia auto-scroll de ningun tipo: el unico encuadre era el numpad ".", asi
// que bajando con la flecha el highlight se iba abajo de todo y no se veia mas.
void Outliner::EnsureVisible() {
    if (!SceneCollection || !ActiveObj) return;
    int row = W3dRowOf(ActiveObj);
    if (row < 0) return;                   // adentro de una rama plegada: no tiene fila
    const int top = BarTopOffset();
    const int visibleHeight = (int)RowHeightGS;
    // y_local(fila) = borderGS + PosY + top + OutYOfRow(fila)   (la misma cuenta de Render)
    OutCutsBuild();
    const int yRow = borderGS + PosY + top + OutYOfRow(row);
    int newValue = PosY;
    if (yRow < top)                  newValue = PosY + (top - yRow);                                // se fue por ARRIBA
    else if (yRow + visibleHeight > height)   newValue = PosY - (yRow + visibleHeight - height);    // se fue por ABAJO
    if (newValue > 0) newValue = 0;               // no scrollear "antes" del inicio
    if (newValue < MaxPosY) newValue = MaxPosY;   // ni mas alla del final (MaxPosY es <= 0)
    if (newValue != PosY) { PosY = newValue; g_redraw = true; }
}

// la VISTA PREVIA del drop bajo (mx, my): linea de insercion (antes / despues de la fila) o futuro padre (el
// centro de la fila). Afuera de la lista: nada (soltar ahi no hace nada).
void Outliner::UpdateDrop(int mx, int my) {
    dropZone = -2;
    if (!Contains(mx, my)) return;
    int rel = my - y - borderGS - PosY - BarTopOffset();
    if (rel < 0) return;
    OutCutsBuild();
    int rest = 0;
    dropRow = OutRowOfY(rel, &rest);
    int f = dropRow;
    OutRow hitF;
    bool has = W3dRowInTree(f, hitF);
    // fila VIRTUAL del armature 2D: NO es destino de drop (no es un objeto; el
    // armature 2D no se puede re-emparentar desde el outliner)
    if (has && hitF.arm2d) { dropZone = -2; return; }
    Object* destination = has ? hitF.obj : NULL;
    dropProf = has ? hitF.prof : 0; // profundidad del destino: la linea se indenta ahi
    // VARIAS ESCENAS: una escena arrastrada va antes o despues de otra (solo sobre las filas de las escenas); un objeto no
    // sale de su escena (sobre la fila de su escena = a su primer nivel)
    if (OutIsRoot(dragObject)) {
        if (!OutIsRoot(destination) || destination == dragObject) { dropZone = -2; return; }
        dropZone = (rest < RowHeightGS / 2) ? 0 : 2;
        return;
    }
    if (destination && OutRootOf(destination) != OutRootOf(dragObject)) { dropZone = -2; return; }
    if (OutIsRoot(destination)) { dropZone = 1; return; }
    if (!destination) dropZone = -1; // al vacio: a la raiz
    else if (destination == dragObject) dropZone = -2;
    else if (rest < RowHeightGS / 4) dropZone = 0;
    else if (rest > (RowHeightGS * 3) / 4) dropZone = 2;
    else dropZone = 1;
}

// AUTO-SCROLL del arrastre de filas: sin esto era imposible llevar algo arriba de todo o abajo de todo de una lista
// mas larga que el panel. En la franja de 2 filas de arriba (o de abajo) del area de la lista, la lista se mueve
// sola: despacio al entrar a la franja, mas rapido cuanto mas cerca del borde y a la velocidad MAXIMA con el mouse
// afuera de la lista (arriba o abajo). Va por TIEMPO: con el mouse quieto en el borde sigue scrolleando.
void Outliner::AutoScrollDrag() {
    if (!dragging || !(leftMouseDown || grabRow >= 0)) { autoScrollMs = 0.0; return; }
    const int up = y + borderGS + BarTopOffset();     // la primera fila visible (pantalla)
    const int down = y + height - borderGS;
    const int zone = (int)RowHeightGS * 2;
    const float vMin = 2.0f, vMax = 40.0f;                // filas por segundo
    float f = 0.0f; int dir = 0;
    if (dragMy < up + zone)     { f = (float)(up + zone - dragMy) / (float)zone; dir = +1; }   // sube la lista
    else if (dragMy > down - zone) { f = (float)(dragMy - (down - zone)) / (float)zone; dir = -1; }
    if (dir == 0) { autoScrollMs = 0.0; autoScrollRest = 0.0f; return; }
    if (f > 1.0f) f = 1.0f;                                // afuera de la lista: al maximo
    const double now = W3dNowMs();
    double dt = autoScrollMs > 0.0 ? now - autoScrollMs : 16.0;
    if (dt > 100.0) dt = 100.0;                            // (un cuadro trabado no pega un salto)
    autoScrollMs = now;
    const float rowsSeg = vMin + (vMax - vMin) * f * f;
    autoScrollRest += (float)dir * rowsSeg * (float)RowHeightGS * (float)dt / 1000.0f;
    const int d = (int)autoScrollRest;
    autoScrollRest -= (float)d;
    g_redraw = true;                                       // sigue mientras el mouse este en la franja
    if (d == 0) return;
    int newValue = PosY + d;
    if (newValue > 0) newValue = 0;
    if (newValue < MaxPosY) newValue = MaxPosY;
    if (newValue == PosY) return;
    PosY = newValue;
    UpdateDrop(dragMx, dragMy);                        // la fila bajo el mouse cambio
}

// suelta el arrastre de una fila: reordena (bordes de la fila destino),
// emparenta (centro de la fila, manteniendo la transformacion) o manda
// a la raiz (soltar en el vacio)
void Outliner::ReleaseDrag(int mx, int my) {
    if (view != OUT_VIEW_SCENE) {
        if (colDrag >= 0) { colDrag = -1; g_redraw = true; return; }   // (se solto el divisor de una columna)
        ReleaseResources(mx, my); return;   // soltar un recurso (carpeta, 3D, Properties)
    }
    Object* obj = dragObject;
    bool was    = dragging;
    const bool grabbed = grabRow >= 0, moved = grabMoved;
    grabRow    = -1; grabMoved = false;
    dragObject = NULL;
    dragging = false;
    dropZone = -2;
    // TACTIL: soltar SIN MOVER lo que se agarro con la pulsacion larga = el menu contextual (Objeto)
    if (grabbed && !moved && !was) { MenuContext(mx, my); return; }
    if (!obj || !was) return;
    if (!Contains(mx, my)) return;
    int rel = my - y - borderGS - PosY - BarTopOffset();
    if (rel < 0) return;
    OutCutsBuild();
    int rest = 0;
    int row = OutRowOfY(rel, &rest);
    int f = row;
    OutRow hitF;
    bool has = W3dRowInTree(f, hitF);
    if (has && hitF.arm2d) return;   // fila VIRTUAL (armature 2D): no acepta drops
    Object* destination = has ? hitF.obj : NULL;
    // una ESCENA: cambia su lugar en la lista de las abiertas (la prioridad del Play)
    if (OutIsRoot(obj)) {
        if (OutIsRoot(destination) && destination != obj) OutMoveScene(obj, destination, rest >= RowHeightGS / 2);
        Resize(width, height);
        return;
    }
    if (destination && OutRootOf(destination) != OutRootOf(obj)) return;   // (a otra escena: no se muda)
    if (OutIsRoot(destination)) destination = NULL;                          // (sobre su escena: a su primer nivel)
    if (!destination) {
        // al vacio: desemparenta hacia la raiz, sin moverse del lugar
        ReparentKeepTransform(obj, SceneCollection);
    } else if (destination != obj) {
        if (rest < RowHeightGS / 4) {
            MoveNextTo(obj, destination, false); // reordenar: antes
        } else if (rest > (RowHeightGS * 3) / 4) {
            MoveNextTo(obj, destination, true); // reordenar: despues
        } else {
            // al centro: pasa a ser HIJO del destino (keep transform)
            ReparentKeepTransform(obj, destination);
            destination->expanded = true;
        }
    }
    Resize(width, height);
}

// ---- MODO MOVER con teclado (reordenar / reparentar sin mouse) ----
// NOMBRES: meter un objeto ADENTRO de otra escena (dir 3) le cambia el ESPACIO DE NOMBRES
// y el embudo del reparent lo puede renumerar (ver W3dAttachTo en ObjectMode.cpp).
//
// EL MODAL SUPRIME LOS RENAMES Y LOS RESUELVE AL CONFIRMAR. Motivo: cada paso que cruzaba
// de escena renumeraba el subarbol Y empujaba su propio MultiRenameUndo. Cancelar
// restauraba los nombres a mano, pero esos pasos QUEDABAN en el historial convertidos en
// no-ops (sus valores viejos ya eran los actuales): N Ctrl+Z que no hacen nada. Y no hay
// forma de sacarlos del stack (el modulo de undo no expone un "pop"). Entonces se invierte
// el orden: durante el modal se levanta W3dNamesLoading -LA MISMA puerta que usan los
// loaders, que hace que W3dAttachTo no renumere ni capture undo- y la uniquificacion se
// hace UNA sola vez al CONFIRMAR, con UN solo paso de undo. Cancelar ya no tiene nada que
// sacar del historial (y el snapshot de nombres queda igual, como red de seguridad).
// Los nombres pueden verse repetidos MIENTRAS dura el modal: es transitorio y se resuelve
// al soltar, igual que la posicion del objeto.
static std::vector<std::pair<Object*, std::string> > gMoveNames;
static void MoveCollectNames(Object* o) {
    if (!o) return;
    gMoveNames.push_back(std::make_pair(o, o->name));
    for (size_t i = 0; i < o->Childrens.size(); i++) MoveCollectNames(o->Childrens[i]);
}
// PRE-ORDEN (el mismo recorrido que usa la busqueda por nombre y que junta W3dAttachTo):
// asi el que conserva el nombre pelado es el mismo objeto en los dos caminos.
static void MoveCollectSubtree(Object* o, std::vector<Object*>& out) {
    if (!o) return;
    out.push_back(o);
    for (size_t i = 0; i < o->Childrens.size(); i++) MoveCollectSubtree(o->Childrens[i], out);
}
// al CONFIRMAR: re-uniquifica el subarbol movido en su scope definitivo, en UN paso de
// undo (misma receta que W3dAttachTo, que aca quedo cortocircuitado por el guard).
static void MoveUniquifySubtree(Object* root) {
    if (!root) return;
    std::vector<Object*> sub;
    MoveCollectSubtree(root, sub);
    bool hasChange = false;
    for (size_t i = 0; i < sub.size() && !hasChange; i++)
        if (sub[i]->FreeName(sub[i]->name) != sub[i]->name) hasChange = true;
    if (!hasChange) return;
    // Object::name = destino 'Directo' legitimo (ver W3dRenameDest en Undo.h)
    std::vector<W3dRenameDest> tips;
    for (size_t i = 0; i < sub.size(); i++) tips.push_back(W3dDestName(&sub[i]->name));
    UndoCaptureRenames(tips);   // TODAS las puntas en UN comando
    int n = 0; std::string first;
    for (size_t i = 0; i < sub.size(); i++) {
        const std::string old = sub[i]->name;
        sub[i]->SetNameObj(old);
        if (sub[i]->name == old) continue;
        n++;
        if (n <= 3) { if (!first.empty()) first += ", "; first += old + " -> " + sub[i]->name; }
    }
    if (n > 0) {
        const char* fmt = T("%d name(s) renumbered when changing scene (%s%s). Check your Lua scripts.");
        std::vector<char> msg(strlen(fmt) + first.size() + 64);
        snprintf(&msg[0], msg.size(), fmt, n, first.c_str(), n > 3 ? ", ..." : "");
        Notify(std::string(&msg[0]), true);
    }
}

// El objeto que se mueve es el ACTIVO. Guarda su posicion original para poder cancelar.
void Outliner::MoveStart() {
    // en la BIBLIOTECA "mover" es el MISMO modo mover que en la escena, pero por las CARPETAS: lo elegido
    // viaja con las flechas y OK confirma (la G de PC y el "1" del N95 hacen lo mismo en las dos vistas)
    if (view != OUT_VIEW_SCENE) { MoveRecStart(); return; }
    if (moving) return;
    if (!ActiveObj || !SceneCollection) return;
    // (lo GENERADO por una instancia de prefab no se muda: es la estructura del prefab)
    if (W3dPrefabIsGeneratedNotice(ActiveObj, true)) return;
    moveObj = ActiveObj;
    gMoveNames.clear();
    MoveCollectNames(moveObj);
    // UNDO: los pasos de adentro del modal NO capturan (W3dNamesLoading esta levantado, ver
    // MoveStep). El estado PREVIO se toma ACA, una sola vez, y se empuja al CONFIRMAR: asi el
    // modal deja EXACTAMENTE un paso -mudanza + renumeracion- y el cancelar no deja ninguno.
    UndoReparentStart(moveObj);
    moveParentOrig = moveObj->Parent;   // NULL = raiz
    movePrevOrig = NULL;
    Object* parent = moveObj->Parent ? moveObj->Parent : SceneCollection;
    for (size_t i = 0; i < parent->Childrens.size(); i++)
        if (parent->Childrens[i] == moveObj) { if (i > 0) movePrevOrig = parent->Childrens[i-1]; break; }
    moving = true;
}

// dir: 0=arriba 1=abajo (reordena entre hermanos) 2=outside(unparent) 3=inside(parent bajo el hermano anterior)
void Outliner::MoveStep(int dir) {
    if (!moving || !moveObj || !SceneCollection) return;
    Object* parent = moveObj->Parent ? moveObj->Parent : SceneCollection;
    int idx = -1;
    for (size_t i = 0; i < parent->Childrens.size(); i++)
        if (parent->Childrens[i] == moveObj) { idx = (int)i; break; }
    if (idx < 0) return;
    // los renames (y su undo) se posponen al MoveConfirm: ver el bloque de arriba
    struct MoveGuard { bool prev; MoveGuard(){ prev = W3dNamesLoading; W3dNamesLoading = true; }
                        ~MoveGuard(){ W3dNamesLoading = prev; } } moveGuard;
    if (dir == 0) {                         // ARRIBA: antes del hermano anterior
        if (idx > 0) { Object* ref = parent->Childrens[idx-1]; MoveNextTo(moveObj, ref, false); }
    } else if (dir == 1) {                  // ABAJO: despues del hermano siguiente
        if (idx + 1 < (int)parent->Childrens.size()) { Object* ref = parent->Childrens[idx+1]; MoveNextTo(moveObj, ref, true); }
    } else if (dir == 2) {                  // AFUERA (unparent): al abuelo, justo despues del padre
        if (parent != SceneCollection) {
            Object* oldParent = parent;
            Object* grandparent = parent->Parent ? parent->Parent : SceneCollection;
            ReparentKeepTransform(moveObj, grandparent);
            MoveNextTo(moveObj, oldParent, true);
        }
    } else if (dir == 3) {                  // ADENTRO (parent): hijo del hermano anterior
        if (idx > 0) {
            Object* newParent = parent->Childrens[idx-1];
            ReparentKeepTransform(moveObj, newParent);
            newParent->expanded = true;
        }
    }
    Resize(width, height); // recalcular el scroll
}

void Outliner::MoveConfirm() {
    if (moving && moveObj) {
        UndoGroupStart();          // la mudanza y los nombres, en UN SOLO Ctrl+Z
        UndoReparentConfirm();     // el paso del ARBOL (capturado en MoveStart) va PRIMERO
        MoveUniquifySubtree(moveObj); // recien ACA se renumera (y empuja su paso)
        UndoGroupEnd();
    } else {
        UndoReparentCancel();
    }
    moving = false; moveObj = NULL; gMoveNames.clear();
}

void Outliner::MoveCancel() {
    UndoReparentCancel();   // el objeto vuelve solo: el modal no deja NINGUN paso (test 'nombresmover')
    if (moving && moveObj && SceneCollection) {
        // el regreso tampoco tiene que renumerar ni empujar undo (los pasos ya no lo hicieron)
        struct MoveGuard { bool prev; MoveGuard(){ prev = W3dNamesLoading; W3dNamesLoading = true; }
                            ~MoveGuard(){ W3dNamesLoading = prev; } } moveGuard;
        Object* parentOrig = moveParentOrig ? moveParentOrig : SceneCollection;
        ReparentKeepTransform(moveObj, parentOrig); // restaura el padre + el transform local (el mundo se mantuvo)
        if (movePrevOrig) {
            MoveNextTo(moveObj, movePrevOrig, true); // justo despues del hermano que estaba antes
        } else {
            for (size_t i = 0; i < parentOrig->Childrens.size(); i++) // era el PRIMERO: al frente
                if (parentOrig->Childrens[i] != moveObj) { MoveNextTo(moveObj, parentOrig->Childrens[i], false); break; }
        }
        // ...y los NOMBRES (el reparent los pudo renumerar al cruzar de escena)
        for (size_t i = 0; i < gMoveNames.size(); i++)
            if (gMoveNames[i].first) gMoveNames[i].first->SetNameObj(gMoveNames[i].second);
        Resize(width, height);
    }
    moving = false; moveObj = NULL; gMoveNames.clear();
}

void Outliner::ClickSelect(int mx, int my) {
    if (view != OUT_VIEW_SCENE) { ClickResources(mx, my); return; }   // la biblioteca
    if (!SceneCollection) return;
    int rel = my - y - borderGS - PosY - BarTopOffset();
    if (rel < 0) return;   // franja del borde: la division de un negativo chico daba fila 0
    OutCutsBuild();
    int row = OutRowOfY(rel);
    int rowClick = row; // (el walk de abajo consume "fila")
    // columnas de la derecha: CAMARA (renderizar) y OJO (a su izquierda), corridas
    // por la barra de scroll vertical si esta presente
    int reserveBar = scrollY ? (GlobalScale * 9 + gapGS) : 0;
    bool inCamera = (mx - x) >= (width - IconSizeGS - marginGS - borderGS - reserveBar) &&
                    (mx - x) <  (width - marginGS - borderGS - reserveBar);
    bool inEye = !inCamera && (mx - x) >= (width - 2*IconSizeGS - gapGS - marginGS - borderGS - reserveBar) &&
                 (mx - x) < (width - IconSizeGS - gapGS - marginGS - borderGS - reserveBar);
    // columnas ocultas por el menu del ojo: no se tocan (el click cae en la fila). Sin la CAMARA, el ojo esta en su
    // lugar (pegado a la derecha)
    if (!g_outShowRender) { inEye = inCamera; inCamera = false; }
    if (!g_outShowEyes) inEye = false;
    // ---- fila VIRTUAL del armature 2D: INERTE. No se selecciona, no se arrastra, no se empareja,
    // no tiene ojo/camara y no se despliega (los huesos se listan y se editan en la pestania
    // "Armature 2D" del panel Properties). El click se COME aca para que no caiga en la malla.
    {
        int fv = row; OutRow hv;
        if (W3dRowInTree(fv, hv) && hv.arm2d) return;
    }
    const std::vector<Object*>& limit = OutLimit();
    for (size_t c = 0; c < limit.size(); c++) {
        int prof = 0;
        OutRow hf;
        Object* hit = W3dRowVisibleRec(limit[c], row, 0, hf) ? hf.obj : NULL;
        prof = hf.prof;
        if (hit) {
            // la FLECHA de desplegar (primer icono, segun la profundidad)
            int xArrow = x + marginGS + PosX + prof * (IconSizeGS + gapGS);
            if (!inEye && !inCamera && mx >= xArrow && mx < xArrow + IconSizeGS &&
                !hit->Childrens.empty()) {
                hit->expanded = !hit->expanded;
                Resize(width, height); // recalcular el scroll
                return;
            }
            // la fila de una ESCENA (varias abiertas): su ojo la muestra u oculta entera; su nombre la ACTIVA (y se puede
            // arrastrar para cambiar el orden). No se selecciona: no es un objeto de la escena
            if (OutIsRoot(hit)) {
                if (inEye) W3dRootSetOpenVisible(W3dObjectRoot(hit), !hit->visible);
                else if (!inCamera && W3dActivateRootOf(hit)) { dragObject = hit; dragging = false; dragY0 = my; }
                g_redraw = true;
                return;
            }
            // un objeto de OTRA escena abierta: su escena pasa a ser la activa antes de elegirlo (el ojo y la camara no)
            if (!inEye && !inCamera && !OutlinerActivateSceneOf(hit)) return;
            if (!inEye && !inCamera) {
                // el click puede convertirse en ARRASTRE (reordenar /
                // emparentar): se confirma al moverse con el boton
                dragObject = W3dIsGenerated(hit) ? NULL : hit;   // (lo generado no se arrastra)
                dragging = false;
                dragY0 = my;
            }
            if (inCamera) {
                hit->renderable = !hit->renderable;
            } else if (inEye) {
                hit->visible = !hit->visible;
            } else if (LShiftPressed) {
                // shift+click: RANGO desde el activo hasta la fila
                // clickeada (1 y 4 -> 1,2,3,4), sumando a la seleccion
                int from = W3dRowOf(ActiveObj);
                if (from < 0) {
                    hit->Select();
                } else {
                    int a = from < rowClick ? from : rowClick;
                    int b = from < rowClick ? rowClick : from;
                    for (int f = a; f <= b; f++) {
                        Object* o = W3dObjectInRow(f);
                        // el rango tiene que quedar en ObjSelects, no solo con select=true:
                        // operaciones como Join arman su lista desde ObjSelects pero borran por el
                        // flag select -> un rango select=true fuera de ObjSelects se borra SIN unirse
                        // (las mallas intermedias "desaparecen"). Mantener ambos consistentes.
                        if (o && !o->select && !OutIsRoot(o) && OutRootOf(o) == SceneCollection) {   // (de su escena)
                            o->select = true; ObjSelects.push_back(o);
                        }
                    }
                    hit->Select(); // el clickeado queda activo
                }
            } else if (LCtrlPressed) {
                // ctrl+click: agregar/sacar UNO de la seleccion (Deseleccionar lo saca de ObjSelects,
                // no solo select=false -> sino quedaba en ObjSelects y ops como Join lo procesaban igual)
                if (hit->select) { hit->Deselect(); if (ActiveObj == hit) ActiveObj = NULL; }
                else { hit->Select(); }
            } else if (!inEye && !inCamera) {
                DeselectAll();
                hit->Select();
            }
            return;
        }
    }
}

void Outliner::key_down_return(){
}

Outliner::~Outliner() {
    // el rename en linea escribia en un campo de ESTE outliner: el foco del teclado no puede
    // quedar apuntando a memoria liberada
    if (g_activeTextField == &rename) g_activeTextField = NULL;
    // el MODO MOVER de la biblioteca a medias (cambiar el tipo del viewport, abrir otro proyecto): el grupo de
    // carpetas que abrio se cancela (lo elegido vuelve a donde estaba). Abierto para siempre, ninguna operacion
    // de carpetas volvia a tener undo y el proximo Esc aplicaba una foto vieja
    if (movingRec) MoveRecCancel();
    delete Row;
}