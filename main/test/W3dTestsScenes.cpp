// ============================================================================
//  W3dTestsScenes.cpp — comandos de harness de las ESCENAS 3D, los JUEGOS y los
//  PREFABS (las raices del proyecto: W3dRoots.h, io/EditorRoots.h). Ver W3dTestsScenes.h.
//
//  Comandos:
//    raiznueva escena|juego|prefab [nombre]
//        lo que hacen "New Scene" / "New Game" / "New Prefab": una raiz vacia (el prefab con
//        su objeto raiz) con nombre libre, y la ABRE.
//    raizabrir <nombre> [prefab] [falla]
//        abre esa escena/juego (o prefab) en el editor (W3dActivateRoot, con sus precondiciones).
//        'falla' asserta que NO se puede (y lo que dice).
//    raizinfo [n N] [escenas N] [juegos N] [prefabs N] [activa X] [tipo escena|juego|prefab]
//             [bloque X] [cargada X 0|1] [inicial X|-] [objetos N] [entrada X <entrada|->]
//             [tipode X escena|juego|prefab]
//        el registro de raices y sus asserts ('objetos' = los de la ACTIVA, recursivo; los
//        tipos son los EFECTIVOS: el de un proyecto viejo sale de sus scripts).
//    raiztipo escena|juego [falla]   cambia el TIPO de la activa en el lugar, con undo (W3dRootConvertActive: lo
//                                    que hace "Convert to Scene"; "Convert to Game" DUPLICA la escena como un juego
//                                    nuevo, se elige con 'rootmenu 3d Convert to Game': prueba_escena_a_juego.w3s)
//
//  LAS ESCENAS ABIERTAS A LA VEZ (W3dRoots.h: la casilla de la lista; el nombre va al final, con espacios):
//    escenatildar abrir|cerrar|cerrarcartel|cerrarfalla <nombre>
//        la casilla de la lista (W3dRootCheck). 'cerrarcartel' asserta que pregunta por los cambios sin guardar (el
//        cartel se responde con cartelresponder); 'cerrarfalla' que no se puede cerrar
//    escenasabiertas <A,B,...>      la lista de las abiertas, en su orden
//    escenaocultas <A,B,...|->      las que tienen el ojo cerrado
//    escenaactiva <nombre>          la activa (SceneCollection)
//    escenacargada 0|1 <nombre>     esta en memoria (cerrarla la libera, salvo la del proyecto o una sin guardar)
//    escenaojo 0|1 <nombre>         el ojo de esa escena (sin el outliner)
//    escenamover <pos> <nombre>     su lugar en la lista
//    escenafila <fila> <nombre>     la fila del outliner de esa escena (su nodo) o de ese objeto de cualquier abierta
//    escenaclick nombre|ojo|flecha <nombre>   CLICK en el outliner en el nombre, el ojo o la flechita de la fila
//    escenaarrastrar antes|despues <A,B>      arrastra la fila de la escena A y la suelta antes / despues de la B
//    escenatecla h                  la H con el mouse en el outliner: oculta lo elegido (con su paso de undo)
//    raizmodo [kind N] [juego 0|1] [autokey 0|1] [menujuego 0|1]
//        el MODO del timeline: ActiveAnimKind, AnimIsGame, si el boton Auto Key esta visible y
//        si el selector de animacion ofrece "Juego"
//    raizanim juego|<animacion de escena>   elegir en el selector de animacion (AnimSelById):
//        "Juego" (el estado base) o uno de sus CLIPS de escena
//    raizclip <nombre> <objeto> <f0> <x0> <f1> <x1>
//        una animacion de escena NUEVA (no activa) que mueve el objeto en X de x0 (frame f0) a x1
//    raizkey [n N]        Insert Keyframe (la I) sobre lo seleccionado; asserta cuantos keyframes de
//                         escena le agrego al activo (en "Juego" ninguno)
//    raizdope <n>         cuantas filas arma el dope sheet del timeline (0 = sin keyframes)
//    raizscrub <frame> [cur N]   arrastrar el cabezal del timeline a ese frame (SetFrameFromX): en
//                         "Juego" sin partida se queda en el 1
//    raizcontexto [activo X|-] [sel N] [camara X|-] [luces N] [coleccion X|-] [anims N]
//        el CONTEXTO de la raiz activa (lo que se guarda y vuelve por raiz): objeto activo,
//        cuantos seleccionados, camara activa, luces registradas (el tope de 8 es por raiz),
//        coleccion activa y cuantas animaciones de escena.
//    raizcamara <nombre> [x y z]   una camara nueva en la raiz activa
//    raizluces <n> [creadas N]     crea n luces con Light::Create (las que entren) y asserta cuantas
//    raizparent <objeto>           el objeto de primer nivel tiene Parent = la raiz (normalizado)
//    raizscript <objeto> <ruta.lua> [prop valor]...   le cuelga un script con esas refs
//    raizinicial <nombre|->        la escena con la que arranca el juego ('-' = la del bloque)
//    raizmenu 3d|outliner <texto>  CLICK en el selector de la barra (del viewport 3D o del outliner)
//                                  y elige la opcion con ese texto (un nombre, "New Scene"...)
//    raizboton escena|juego|prefab CLICK en el "+" desplegable de la barra del outliner y elegir "New ..."
//    raizbarra 3d|outliner <texto> [icono <nombre>]  el selector de la barra muestra ese nombre (y ese
//                                  icono: camera = escena, gamepad = juego, prefab)
//    pantallafoto <ruta.ppm>       dibuja un cuadro ENTERO (paneles + el menu abierto) y lo vuelca a un
//                                  PPM (diagnostico a ojo de las barras y los desplegables)
//    raizborrar <nombre> [prefab] [falla]      el modelo (sin UI): borra la escena/prefab
//    raizrenombrar <viejo> <nuevo> [prefab]    idem, renombra (sin undo: el de la vista lo tiene)
//    entradaexiste <ruta.w3d> <entrada> [0|1] / entradatiene <ruta.w3d> <entrada> <texto> [0|1]
//        el contenedor guardado tiene (o no) esa entrada / esa entrada tiene (o no) el texto
//
//  LAS CINEMATICAS DE UN JUEGO -- los DOS caminos (W3dRoots.h / animation/Animation.h):
//    cineinfo [activa 0|1] [escena X] [juego X]
//        la cinematica de playScene(): si hay una sonando, que escena y de que juego
//    raizobjat <raiz> <objeto> <x> <y> <z> <tol>
//        la posicion de un objeto de CUALQUIER raiz cargada (la activa o no): lo que quedo en la
//        escena de la cinematica al volver, lo que no se movio en el juego mientras sonaba
//    objvis <objeto> 0|1           el objeto (de la raiz activa) esta visible o no
//    animnueva <nombre> [f0 f1]    una animacion de escena NUEVA (no la activa) con ese rango
//
//  LOS CLIPS DE JERARQUIA (animation/W3dAnimSet.h: una raiz anima a si misma y a sus hijos por ruta
//  de nombres; viven COMPARTIDOS en una biblioteca = un animset .w3da):
//    clipnuevo <raiz> <nombre> [f0 f1]   un clip de jerarquia NUEVO en la biblioteca de la raiz (se le crea
//                                        una con su nombre si no tiene). No lo elige.
//    animcurva <anim|raiz:clip> <objeto|.> <x|y|z|rx|ry|rz|vis> <f> <v> [<f> <v>]...
//        keyframes en una curva de esa animacion de escena (o de ese clip de jerarquia, a traves de su
//        VISTA sobre la raiz, que se escribe en el clip al terminar: '.' = la raiz)
//    clipinfo <raiz|*> [n N] [tiene X] [no X] [activo X]
//        los clips de la biblioteca de la raiz ('*' = los de todas las raices de la activa) y cual se edita
//    clipmenu <texto>   elige en el selector de animacion (el mismo menu del timeline y de la tarjeta)
//                       la opcion con ese texto ("Hierarchy Clip", "Puerta: abrir", una escena...)
//    clipnombre <nombre>          Rename de la animacion elegida (un clip: unico entre los de su biblioteca)
//    jerlib <objeto> <biblioteca|->          el objeto pasa a usar esa biblioteca ('-' = ninguna)
//    jerinfo <objeto> [lib X|-] [clips N] [refs N] [tiene C] [pistas C N] [ruta C R] [tamano T tol]
//        la biblioteca de un objeto: su nombre, cuantos clips, cuantas referencias tiene el recurso
//        (cuantos objetos/reproductores la usan: UNA copia para todos) y lo de un clip
//    jerruta <raiz> <objeto> <ruta|->        W3dHierPath: la ruta de un nodo relativa a la raiz
//    jernodo <raiz> <ruta> <objeto|->        W3dHierNode: el nodo que nombra una ruta
//    jerplay <raiz> <clip> [loop 0|1] [dur D]  animObject sin lua (asserta la duracion o -1 si no esta)
//    jertick <dt> [n]                        n ticks del reproductor de clips (W3dAnimObjectsTick)
//    jervel <raiz> <v> / jerretarget <objeto> completo|rotaciones|clip / jerreset
//    jerclipretarget <raiz> <clip> completo|rotaciones   el retarget por DEFECTO de un clip
//    jerframe <raiz> <frame|-> [tol] [termino 0|1] [clip X]   el cabezal de la raiz ('-' = no suena nada)
//    jermem [clips N] [refs N] [sonando N]   los clips de jerarquia en memoria (W3dAnimSetsGetStats)
//    rotat <objeto> <rx> <ry> <rz> <tol>     la rotacion (euler, grados) de un objeto de la raiz activa
//    animrango <f0> <f1>                     Start/End de la animacion elegida (la de una vista va a su clip)
//    rigretarget <prefijo> <escala>          dos armatures "<prefijo>A" y "<prefijo>B" (B con los huesos a
//        'scale'), con un clip compartido que mueve la raiz y rota/traslada los hijos (para el retarget)
//    huesopose <armature> <hueso> <x> <y> <z> <tol> [frame F]   la traslacion de la POSE del hueso (en juego,
//        con el cabezal del clip en F)
//    jerlibmenu <objeto> <(none)|New Library|biblioteca>   el desplegable "Clips" de la tarjeta Animacion (con undo)
//    objretarget <objeto> <Clip default|Complete|Rotations only> | objretarget <objeto> es <clip|completo|rotaciones>
//        el desplegable "Retarget" del objeto (su retarget PROPIO, con undo; un armature no tiene "Clip default")
//    clipretargetmenu <Complete|Rotations only>   el "Retarget" por defecto del clip de jerarquia elegido (con undo)
//    animmenos                     el "-" de la tarjeta Animation (una escena o la vista de un clip: deshacible)
//    animlista [n N] [activa X] [tiene X]... [no X]...   las animaciones de escena ("Raiz:clip" = una vista)
//    jerinfo ... [retarget C completo|rotaciones] [capas N] [refsundo N]   (mas asserts de jerinfo)
//    jercapa <raiz> <n> <clip|-> [infl %] [modo mezclar|sumar|restar] [vel V] [desde D] [loop 0|1] [frame F]
//        la capa n (base 1) de clips de jerarquia de la raiz (objectLayer sin lua); '-' la saca con las de arriba
//    jermix frame F | tick DT [n] | soltar   el Mix de objetos (capas de escena + de jerarquia): en el frame F del
//        editor, n ticks del juego, o devolver los objetos a su base
//    mixjer <raiz> <clip>          "Agregar" del Mix: una capa con ese clip de jerarquia (queda elegida; entra al Mix)
//    mixfilas [n N] [tiene X]...   las filas del arbol del Mix
//    rigcapa <armature> <clip|-> [infl]   una capa del Mix del armature ('-' las saca)
//    rigbiped <armature>...        esos armatures reconstruyen el FK desde sus TransformLink (el camino biped)
//    huesopose ... [head X Y Z]    (ademas asserta la cabeza del hueso despues del FK; con capas, cada capa en F)
//    rigskinglb <armature> <ruta.glb>   un cubo skinneado al armature, exportado solo a un GLB (con sus clips)
//    importclipsalto <ruta> [n N]  importa un glTF/GLB y verifica que cada clip trae el tamano de su esqueleto
//    juegocinemin <carpeta> / juegocinelog <carpeta> <nombre>
//        el juego 3D minimo con los DOS caminos: un JUEGO con dos clips de jerarquia sonando a la vez
//        (animObject) y dos capas de escena (sceneLayer) que a los 5 ticks reproduce la ESCENA
//        "Cine1" (playScene con alTerminar); su juez verifica que el juego volvio EXACTAMENTE
//        como estaba, que la cinematica sono entera y que sus animaciones siguieron despues. Ademas
//        tres raices SIN biblioteca reproducen clips ajenos por su nombre calificado: la Grua
//        ("Puerta/abrir" con animObjectSpeed 2), la Puerta2 (el doble de grande, animRetarget
//        "rotaciones": sube el doble) y la Plataforma (objectLayer: "Ascensor/subir" mezclado al 50%). El
//        mismo veredicto en el Play y en el juego compilado (que ademas carga la escena de su
//        entrada para la cinematica y la libera al volver: se mira su whisk3d.log).
//    notif tiene <texto> | notif no <texto> | notif limpiar
//        las notificaciones que se ven: alguna contiene el texto (en ingles, la clave de T(): tambien se busca
//        traducido, como sale en pantalla) / ninguna
//    keyrombo <prop> <comp> [estado N]   el CLICK en el rombo de keyframe de Properties (el hook que cablea el
//        panel, sobre el objeto activo en el frame actual) y el estado que muestra despues
//    menuobjeto <id>               una opcion del menu Object por su id (511 Delete Keyframe, 512 Clear Keyframe...)
//    juegoescenasmin <carpeta> / juegoescenaslog <carpeta> <nombre>
//        el juego 3D minimo con TRES raices: el JUEGO del bloque pasa con changeScene() a la ESCENA
//        "Intro" (una cinematica: su animacion arranca sola y su script, al verla terminar, pide el
//        nivel) y de ahi al JUEGO "Nivel2", cuyo juez verifica que las anteriores se DESCARGARON, que
//        la nueva CARGO desde su entrada, que la cinematica se reprodujo entera y que getShared()
//        sobrevivio. El mismo veredicto en el Play y en el juego compilado.
// ============================================================================
#include "test/W3dTestsScenes.h"
#include "test/W3dScript.h"            // W3dRunCommand("juego3dmin ...")
#include "W3dRoots.h"
#include "io/EditorRoots.h"
#include "io/ProjectResources.h"
#include "objects/Objects.h"
#include "objects/Scene.h"
#include "objects/Empty.h"
#include "objects/Camera.h"
#include "objects/Light.h"
#include "objects/Mesh.h"
#include "script/W3dScript.h"
#include "animation/Animation.h"
#include "animation/W3dAnimSet.h"         // los clips de jerarquia: sus bibliotecas, rutas y reproductor
#include "io/W3dResources.h"
#include "animation/SkeletalAnimation.h" // el retarget de los armatures
#include "objects/Armature.h"
#include "ViewPorts/ViewPorts.h"
#include "ViewPorts/ViewPort3D.h"
#include "ViewPorts/Outliner.h"
#include "ViewPorts/LayoutInput.h"
#include "ViewPorts/Timeline.h"         // el dope sheet: en "Juego" no hay filas (sin keyframes)
#include "ViewPorts/Properties.h"       // la tarjeta Render: el render del CACHE de un juego
#include "script/SimGame.h"
#include "WhiskUI/widgets/Button.h"
#include "WhiskUI/widgets/PopupMenu.h"
#include "WhiskUI/draw/icons.h"          // IconName: los iconos de las raices (camera / gamepad / prefab)
#include "config/W3dLang.h"
#include "io/W3dZip.h"
#include "w3dFilesystem.h"
#include "graphics/w3dGraphics.h"              // pantallafoto: w3dEngine::ReadPixelsRGBA
#include "WhiskUI/Propieties/PropList.h"   // las filas del arbol del Mix (MixRowsCount / MixRowText)
#include "WhiskUI/Propieties/PropertieBase.h"   // los hooks del rombo de keyframe (W3dKeyframeToggle / Estado)
#include "ViewPorts/Notifications.h"   // lo que se le dijo al usuario (notif)
#include "objects/UI.h"                // pruebacapas: las escenas 2D
#include "W3dScene.h"                  // pruebacapas: la escena 2D inicial
#include "ViewPorts/W3dInput.h"        // escenatecla: W3dK_H
#include "io/ProjectChanges.h"         // escenatildar: el cartel de los cambios sin guardar al cerrar una escena
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <string>
#include <algorithm>

extern ViewportBase* rootViewport;
// (Properties.cpp) el selector de animacion
extern void AnimSelById(int id);
extern void AnimSelGame();
extern bool AutoKeyOn;
extern void BuildMenuAnim(PopupMenu* menu);
// (Properties.cpp) los desplegables de la tarjeta Animacion por su texto (el mismo camino que el click)
extern bool PropsHierLibPick(const std::string& text);
extern bool PropsObjRetargetPick(const std::string& text);
extern bool PropsClipRetargetPick(const std::string& text);
extern bool PropsMixAddMenu(const std::string& sub, const std::string& item);
extern void _AnimOfCardFwd();   // el "-" de la tarjeta Animation
extern bool ExportGLTF(const std::string&, bool, bool);
extern bool ImportGLTF(const std::string&);
extern void WeightPaintEnsureMap(Mesh* m);

// ---------------------------------------------------------------------------
//  helpers
// ---------------------------------------------------------------------------
static std::string IsInteger(long n) { char b[32]; sprintf(b, "%ld", n); return std::string(b); }
static int TypeOf(const std::string& s) { return W3dRootTypeByKey(s, W3D_ROOT_SCENE); }
static const char* TypeName(int t) { return W3dRootTypeKey(t); }
static std::string NameObj(const Object* o) { return o ? o->name : std::string("-"); }

static void Leaves(ViewportBase* n, std::vector<ViewportBase*>& out) {
    if (!n) return;
    if (n->isLeaf()) { out.push_back(n); return; }
    if (n->ContainerKind() == 1) { Leaves(((ViewportRow*)n)->childA, out); Leaves(((ViewportRow*)n)->childB, out); }
    else { Leaves(((ViewportColumn*)n)->childA, out); Leaves(((ViewportColumn*)n)->childB, out); }
}
static ViewportBase* FirstLeaf(int kind) {
    std::vector<ViewportBase*> hs;
    Leaves(rootViewport, hs);
    for (size_t i = 0; i < hs.size(); i++) if (hs[i]->ViewportKind() == kind) return hs[i];
    return NULL;
}
// el boton del selector de la raiz en la barra de ese panel (3D: el de rol BR_Root; outliner: btnRoot)
static Button* ButtonSelector(ViewportBase* v, bool outliner) {
    if (!v) return NULL;
    if (outliner) return ((Outliner*)v)->btnRoot;
    return BarRoleBtn(v->BarButtons, BR_Root);
}
// CLICK real en un boton de la barra: se trae a la vista (la barra scrollea si no entra) y se clickea
static bool ClickButton(ViewportBase* v, Button* b) {
    if (!v || !b || !b->visible) return false;
    // si no entra en el ancho del panel, la barra se scrollea (como con la rueda) hasta que se vea entero
    v->barScrollManual = 0;
    v->UpdateBar();
    const int extra = (b->sx + b->width) - (v->x + v->width) + 4;
    if (extra > 0) { v->barScrollManual = extra; v->UpdateBar(); }
    const int bx = b->sx + b->width / 2, by = b->sy + b->height / 2;
    LayoutClickUI(bx, by);
    LayoutRelease(bx, by);
    return true;
}
static int ObjectsRec(Object* o) {
    if (!o) return 0;
    int n = 0;
    for (size_t i = 0; i < o->Childrens.size(); i++) n += 1 + ObjectsRec(o->Childrens[i]);
    return n;
}
static bool ReadEntry(const std::string& zip, const std::string& e, std::string* data) {
    std::vector<W3dZipEntry> za;
    if (!W3dZipRead(zip, &za)) return false;
    for (size_t i = 0; i < za.size(); i++)
        if (za[i].name == e) {
            if (data) data->assign(za[i].data.begin(), za[i].data.end());
            return true;
        }
    return false;
}

// ---------------------------------------------------------------------------
//  los comandos
// ---------------------------------------------------------------------------
static bool CmdNewRoot(std::istringstream& ss, std::string& err) {
    std::string t, n; ss >> t >> n;
    if (t != "scene" && t != "game" && t != "prefab") { err = "rootnew: uso: rootnew escena|juego|prefab [nombre]"; return false; }
    const int i = W3dRootCreateAndOpen(TypeOf(t), n);
    if (i < 0) { err = "rootnew: se creo pero no se pudo abrir"; return false; }
    printf("      [raiznueva] %s '%s' (fila %d) abierta\n", t.c_str(), W3dRoots()[(size_t)i].name.c_str(), i);
    return true;
}

static bool CmdRootOpen(std::istringstream& ss, std::string& err) {
    std::string n, k; ss >> n;
    int type = W3D_ROOT_SCENE; bool fault = false;
    while (ss >> k) { if (k == "prefab") type = W3D_ROOT_PREFAB; else if (k == "fails") fault = true; }
    const int i = W3dRootFind(type, n);
    if (i < 0) { err = "rootopen: no hay un " + std::string(TypeName(type)) + " '" + n + "'"; return false; }
    std::string reason;
    const bool ok = W3dActivateRoot(i, &reason);
    printf("      [raizabrir] %s '%s' -> %s%s%s\n", TypeName(type), n.c_str(), ok ? "open" : "NO",
           reason.empty() ? "" : ": ", reason.c_str());
    if (fault == ok) { err = fault ? "rootopen: se abrio y se esperaba que no" : "rootopen: no se pudo abrir (" + reason + ")"; return false; }
    return true;
}

static bool CmdRootInfo(std::istringstream& ss, std::string& err) {
    const std::vector<W3dRootRow>& fs = W3dRoots();
    const int act = W3dActiveRoot();
    int ne = 0, nj = 0, np = 0;
    for (size_t i = 0; i < fs.size(); i++) {
        const int t = W3dRootTypeOf((int)i);
        if (t == W3D_ROOT_PREFAB) np++; else if (t == W3D_ROOT_GAME) nj++; else ne++;
        printf("      [raizinfo] %c %-7s '%s' entrada='%s' carpeta='%s' %s%s%s%s\n", (int)i == act ? '*' : ' ',
               TypeName(t), fs[i].name.c_str(), fs[i].entry.c_str(), fs[i].folder.c_str(),
               fs[i].root ? "loaded" : "sin cargar", fs[i].block ? " (bloque)" : "",
               ((int)i == W3dInitialRootIdx() && W3dRootEs3D(t)) ? " (inicial)" : "",
               fs[i].typeExplicit ? "" : " (tipo deducido)");
    }
    std::string k;
    while (ss >> k) {
        std::string v; if (!(ss >> v)) { err = "rootinfo: falta el valor de '" + k + "'"; return false; }
        if (k == "n") { if ((int)fs.size() != atoi(v.c_str())) { err = "rootinfo: hay " + IsInteger((long)fs.size()) + " raices, se esperaban " + v; return false; } }
        else if (k == "scenes") { if (ne != atoi(v.c_str())) { err = "rootinfo: hay " + IsInteger(ne) + " escenas, se esperaban " + v; return false; } }
        else if (k == "games") { if (nj != atoi(v.c_str())) { err = "rootinfo: hay " + IsInteger(nj) + " juegos, se esperaban " + v; return false; } }
        else if (k == "prefabs") { if (np != atoi(v.c_str())) { err = "rootinfo: hay " + IsInteger(np) + " prefabs, se esperaban " + v; return false; } }
        else if (k == "active") {
            if (act < 0 || fs[(size_t)act].name != v) { err = "rootinfo: la activa es '" + (act >= 0 ? fs[(size_t)act].name : std::string("?")) + "', se esperaba '" + v + "'"; return false; }
            if (fs[(size_t)act].root != SceneCollection) { err = "rootinfo: la activa no es SceneCollection"; return false; }
        }
        else if (k == "type") { if (act < 0 || W3dRootTypeOf(act) != TypeOf(v)) { err = "rootinfo: la activa no es un(a) " + v; return false; } }
        else if (k == "block") { const int b = W3dRootBlock(); if (b < 0 || fs[(size_t)b].name != v) { err = "rootinfo: la del bloque no es '" + v + "'"; return false; } }
        else if (k == "initial") {
            const std::string i = W3dInitialRoot();
            if (i != (v == "-" ? std::string() : v)) { err = "rootinfo: la inicial es '" + i + "', se esperaba '" + v + "'"; return false; }
        }
        else if (k == "objects") { const int n = ObjectsRec(SceneCollection); if (n != atoi(v.c_str())) { err = "rootinfo: la activa tiene " + IsInteger(n) + " objetos, se esperaban " + v; return false; } }
        else if (k == "loaded" || k == "entry" || k == "tipode") {
            std::string x; if (!(ss >> x)) { err = "rootinfo: falta el valor de '" + k + " " + v + "'"; return false; }
            int i = -1;
            for (size_t f = 0; f < fs.size() && i < 0; f++) if (fs[f].name == v) i = (int)f;
            if (i < 0) { err = "rootinfo: no hay una raiz '" + v + "'"; return false; }
            if (k == "loaded" && (fs[(size_t)i].root != NULL) != (x == "1")) { err = "rootinfo: '" + v + "' " + (fs[(size_t)i].root ? "is" : "no esta") + " cargada"; return false; }
            if (k == "entry" && fs[(size_t)i].entry != (x == "-" ? std::string() : x)) { err = "rootinfo: la entrada de '" + v + "' es '" + fs[(size_t)i].entry + "'"; return false; }
            if (k == "tipode" && W3dRootTypeOf(i) != TypeOf(x)) { err = "rootinfo: '" + v + "' es " + TypeName(W3dRootTypeOf(i)) + ", se esperaba " + x; return false; }
        }
        else { err = "rootinfo: no entiendo '" + k + "'"; return false; }
    }
    return true;
}

static bool CmdRootContext(std::istringstream& ss, std::string& err) {
    printf("      [raizcontexto] activo=%s sel=%d camara=%s luces=%d coleccion=%s anims=%d\n", NameObj(ActiveObj).c_str(),
           (int)ObjSelects.size(), NameObj((Object*)CameraActive).c_str(), (int)Lights.size(),
           CollectionActive == SceneCollection ? "(raiz)" : NameObj(CollectionActive).c_str(), (int)SceneAnimations.size());
    std::string k;
    while (ss >> k) {
        std::string v; if (!(ss >> v)) { err = "rootcontext: falta el valor de '" + k + "'"; return false; }
        if (k == "active") { if (NameObj(ActiveObj) != v) { err = "rootcontext: el activo es '" + NameObj(ActiveObj) + "'"; return false; } }
        else if (k == "sel") { if ((int)ObjSelects.size() != atoi(v.c_str())) { err = "rootcontext: hay " + IsInteger((long)ObjSelects.size()) + " seleccionados"; return false; } }
        else if (k == "camera") { if (NameObj((Object*)CameraActive) != v) { err = "rootcontext: la camara activa es '" + NameObj((Object*)CameraActive) + "'"; return false; } }
        else if (k == "lights") { if ((int)Lights.size() != atoi(v.c_str())) { err = "rootcontext: hay " + IsInteger((long)Lights.size()) + " luces"; return false; } }
        else if (k == "collection") {
            const std::string c = CollectionActive == SceneCollection ? std::string("-") : NameObj(CollectionActive);
            if (c != v) { err = "rootcontext: la coleccion activa es '" + c + "'"; return false; }
        }
        else if (k == "anims") { if ((int)SceneAnimations.size() != atoi(v.c_str())) { err = "rootcontext: hay " + IsInteger((long)SceneAnimations.size()) + " animaciones de escena"; return false; } }
        else { err = "rootcontext: no entiendo '" + k + "'"; return false; }
    }
    // la seleccion NUNCA nombra objetos de otra raiz
    for (size_t i = 0; i < ObjSelects.size(); i++)
        if (W3dRootOf(ObjSelects[i]) != SceneCollection) { err = "rootcontext: '" + ObjSelects[i]->name + "' esta seleccionado y no es de la raiz activa"; return false; }
    if (ActiveObj && W3dRootOf(ActiveObj) != SceneCollection) { err = "rootcontext: el activo no es de la raiz activa"; return false; }
    if (CameraActive && W3dRootOf((Object*)CameraActive) != SceneCollection) { err = "rootcontext: la camara activa no es de la raiz activa"; return false; }
    for (size_t i = 0; i < Lights.size(); i++)
        if (W3dRootOf((Object*)Lights[i]) != SceneCollection) { err = "rootcontext: una luz registrada no es de la raiz activa"; return false; }
    return true;
}

static bool CmdRootCamera(std::istringstream& ss, std::string& err) {
    std::string n; float x = 0, y = 0, z = 8; ss >> n >> x >> y >> z;
    if (n.empty()) { err = "rootcamera: uso: rootcamera <nombre> [x y z]"; return false; }
    Camera* c = new Camera(NULL, Vector3(x, y, z), Vector3(0, 0, 0));
    c->SetNameObj(n);
    printf("      [raizcamara] '%s' (camara activa '%s')\n", c->name.c_str(), NameObj((Object*)CameraActive).c_str());
    return true;
}

static bool CmdRootLights(std::istringstream& ss, std::string& err) {
    int n = 0; std::string k; int expected = -1;
    ss >> n;
    if (ss >> k) { if (k == "created") ss >> expected; }
    int created = 0;
    for (int i = 0; i < n; i++) if (Light::Create(NULL, (float)i, 2, 2)) created++;
    printf("      [raizluces] %d de %d creadas (luces de la raiz activa: %d)\n", created, n, (int)Lights.size());
    if (expected >= 0 && created != expected) { err = "rootlights: se crearon " + IsInteger(created) + ", se esperaban " + IsInteger(expected); return false; }
    return true;
}

static bool CmdRootParent(std::istringstream& ss, std::string& err) {
    std::string n; ss >> n;
    Object* o = SceneCollection ? FindObjectByName(SceneCollection, n) : NULL;
    if (!o) { err = "rootparent: no hay un objeto '" + n + "'"; return false; }
    printf("      [raizparent] '%s' Parent=%s\n", n.c_str(), o->Parent == SceneCollection ? "la raiz" : NameObj(o->Parent).c_str());
    if (o->Parent != SceneCollection) { err = "rootparent: '" + n + "' no tiene a la raiz de Parent"; return false; }
    return true;
}

static bool CmdRootScript(std::istringstream& ss, std::string& err) {
    std::string n, path; ss >> n >> path;
    Object* o = SceneCollection ? FindObjectByName(SceneCollection, n) : NULL;
    if (!o || path.empty()) { err = "rootscript: uso: rootscript <objeto> <ruta.lua> [prop valor]..."; return false; }
    if (!o->scriptData) o->scriptData = new W3dScriptData();
    W3dScriptEntry e; e.path = path;
    std::string p, v;
    while (ss >> p >> v) e.refs.push_back(std::make_pair(p, v));
    o->scriptData->scripts.push_back(e);
    printf("      [raizscript] '%s' <- %s (%d ref(s))\n", n.c_str(), path.c_str(), (int)e.refs.size());
    return true;
}

static bool CmdRootInitial(std::istringstream& ss, std::string& err) {
    std::string n; ss >> n;
    if (!W3dRootSetInitial(n == "-" ? std::string() : n)) { err = "rootinitial: no hay una escena '" + n + "'"; return false; }
    printf("      [raizinicial] '%s'\n", W3dInitialRoot().c_str());
    return true;
}

static bool CmdRootMenu(std::istringstream& ss, std::string& err) {
    std::string where, rest; ss >> where; std::getline(ss, rest);
    size_t a = rest.find_first_not_of(' ');
    rest = (a == std::string::npos) ? std::string() : rest.substr(a);
    const bool out = (where == "outliner");
    ViewportBase* v = FirstLeaf(out ? 2 : 1);
    Button* b = ButtonSelector(v, out);
    if (!b) { err = "rootmenu: no hay un selector de escena en la barra del " + where; return false; }
    if (OpenMenu) OpenMenu->Close();
    ClickButton(v, b);
    if (!OpenMenu || !OpenMenu->open) { err = "rootmenu: el click no abrio el selector"; return false; }
    PopupMenu* m = OpenMenu;
    MenuItem* it = NULL;
    std::string has;
    for (size_t i = 0; i < m->items.size(); i++) {
        has += " [" + m->items[i]->text + (m->items[i]->green ? "*" : "") + "]";
        if (!it && (m->items[i]->text == rest || m->items[i]->text == T(rest.c_str()))) it = m->items[i];
    }
    printf("      [raizmenu] %s:%s\n", where.c_str(), has.c_str());
    if (!it) { m->Close(); err = "rootmenu: el selector no ofrece '" + rest + "'"; return false; }
    m->Close();
    m->Run(it->id);
    if (rootViewport) rootViewport->Render();   // un cuadro: las barras se sincronizan
    return true;
}

// raizboton escena|juego|prefab: el SELECTOR de la escena de la barra del outliner (el outliner ya no tiene "+": la
// biblioteca es otro viewport) ofrece, debajo de las escenas / juegos / prefabs del proyecto, "New Scene" / "New Game" /
// "New Prefab" con el icono de lo que crean. Un click real lo abre y se elige uno.
static bool CmdRootButton(std::istringstream& ss, std::string& err) {
    std::string t; ss >> t;
    Outliner* o = (Outliner*)FirstLeaf(2);
    if (!o && FirstLeaf(9)) {   // (la prueba lo habia pasado a la biblioteca: vuelve a ser el outliner, como el usuario)
        extern void LayoutChangeViewportType(ViewportBase* v, int aId);
        LayoutChangeViewportType(FirstLeaf(9), 1);
        o = (Outliner*)FirstLeaf(2);
        if (rootViewport) rootViewport->Render();
    }
    if (!o) { err = "rootbutton: no hay outliner"; return false; }
    if (OpenMenu) OpenMenu->Close();
    if (o->newBtn && o->newBtn->visible) { err = "rootbutton: el outliner de la escena no lleva \"+\""; return false; }
    if (!ClickButton(o, o->btnRoot)) { err = "rootbutton: el selector de la escena no esta visible"; return false; }
    if (!OpenMenu || !OpenMenu->open) { err = "rootbutton: el click en el selector no abrio su menu"; return false; }
    PopupMenu* m = OpenMenu;
    static const char* kItems[3] = { "New Scene", "New Game", "New Prefab" };
    std::string has;
    for (size_t i = 0; i < m->items.size(); i++) {
        char ic[16]; snprintf(ic, sizeof(ic), " %d", m->items[i]->icon);
        has += " [" + m->items[i]->text + ic + "]";
    }
    printf("      [raizboton] selector:%s\n", has.c_str());
    const int want = (t == "prefab") ? 2 : (t == "game") ? 1 : 0;
    int id = -1;
    for (size_t i = 0; i < m->items.size(); i++) if (m->items[i]->text == T(kItems[want])) { id = m->items[i]->id; break; }
    if (id < 0) { m->Close(); err = std::string("rootbutton: el selector no ofrece '") + kItems[want] + "'"; return false; }
    const int prev = (int)W3dRoots().size();
    m->Close();
    m->Run(id);
    if ((int)W3dRoots().size() != prev + 1) { err = "rootbutton: elegir en el selector no creo nada"; return false; }
    printf("      [raizboton] %s -> '%s'\n", t.c_str(), W3dRoots()[(size_t)W3dActiveRoot()].name.c_str());
    if (rootViewport) rootViewport->Render();
    return true;
}

static bool CmdRootBar(std::istringstream& ss, std::string& err) {
    std::string where, text, k, icon; ss >> where >> text;
    if ((ss >> k) && k == "icon") ss >> icon;
    const bool out = (where == "outliner");
    ViewportBase* v = FirstLeaf(out ? 2 : 1);
    if (rootViewport) rootViewport->Render();
    Button* b = ButtonSelector(v, out);
    if (!b) { err = "rootbar: no hay un selector en la barra del " + where; return false; }
    printf("      [raizbarra] %s: '%s' (icono %d, visible %d)\n", where.c_str(), b->text.c_str(), b->icon, b->visible ? 1 : 0);
    // (el '*' del final es la marca de SIN GUARDAR -io/ProjectChanges.h-: no es parte del nombre)
    std::string dice = b->text;
    if (!dice.empty() && dice[dice.size() - 1] == '*' && text[text.size() - 1] != '*') dice.erase(dice.size() - 1);
    if (dice != text) { err = "rootbar: el selector dice '" + b->text + "', se esperaba '" + text + "'"; return false; }
    if (!icon.empty() && icon != IconName(b->icon)) {
        err = "rootbar: el selector tiene el icono '" + std::string(IconName(b->icon)) + "', se esperaba '" + icon + "'"; return false;
    }
    return true;
}

static bool CmdScreenSnapshot(std::istringstream& ss, std::string& err) {
    std::string path; ss >> path;
    if (path.empty() || !rootViewport) { err = "screensnapshot: uso: screensnapshot <ruta.ppm> (con layout)"; return false; }
    const int w = rootViewport->width, h = rootViewport->height;
    if (w <= 0 || h <= 0) { err = "screensnapshot: el layout no tiene tamanio"; return false; }
    rootViewport->Render();
    LayoutRenderMenu(w, h);   // los desplegables van encima de los paneles (como en el loop del editor)
    std::vector<unsigned char> buf((size_t)w * (size_t)h * 4);
    w3dEngine::ReadPixelsRGBA(0, 0, w, h, &buf[0]);
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) { err = "screensnapshot: no pude escribir '" + path + "'"; return false; }
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int y = h - 1; y >= 0; y--)
        for (int x = 0; x < w; x++) fwrite(&buf[((size_t)y * (size_t)w + (size_t)x) * 4], 1, 3, f);
    fclose(f);
    printf("      [pantallafoto] %dx%d -> %s\n", w, h, path.c_str());
    return true;
}

static bool CmdRootDelete(std::istringstream& ss, std::string& err) {
    std::string n, k; ss >> n;
    int type = W3D_ROOT_SCENE; bool fault = false;
    while (ss >> k) { if (k == "prefab") type = W3D_ROOT_PREFAB; else if (k == "fails") fault = true; }
    std::string reason;
    const bool ok = W3dRootDelete(W3dRootFind(type, n), &reason);
    printf("      [raizborrar] %s '%s' -> %s %s\n", TypeName(type), n.c_str(), ok ? "deleted" : "NO", reason.c_str());
    if (ok == fault) { err = fault ? "rootdelete: se borro y se esperaba que no" : "raizborrar: " + reason; return false; }
    return true;
}

static bool CmdRootRename(std::istringstream& ss, std::string& err) {
    std::string v, n, k; ss >> v >> n;
    int type = W3D_ROOT_SCENE;
    if ((ss >> k) && k == "prefab") type = W3D_ROOT_PREFAB;
    std::string remained;
    if (!W3dRootRename(W3dRootFind(type, v), n, &remained)) { err = "rootrename: no hay un " + std::string(TypeName(type)) + " '" + v + "'"; return false; }
    printf("      [raizrenombrar] '%s' -> '%s'\n", v.c_str(), remained.c_str());
    return true;
}

static bool CmdEntryExists(std::istringstream& ss, std::string& err) {
    std::string z, e, x = "1"; ss >> z >> e >> x;
    const bool has = ReadEntry(z, e, NULL);
    printf("      [entradaexiste] %s : %s -> %s\n", z.c_str(), e.c_str(), has ? "is" : "no esta");
    if (has != (x == "1")) { err = "entryexists: '" + e + "' " + (has ? "is" : "no esta") + " en " + z; return false; }
    return true;
}
static bool CmdEntryHas(std::istringstream& ss, std::string& err) {
    std::string z, e, t, x = "1"; ss >> z >> e >> t >> x;
    std::string d;
    if (!ReadEntry(z, e, &d)) { err = "entryhas: no hay una entrada '" + e + "' en " + z; return false; }
    const bool has = d.find(t) != std::string::npos;
    printf("      [entradatiene] %s : %s %s '%s'\n", z.c_str(), e.c_str(), has ? "has" : "no tiene", t.c_str());
    if (has != (x == "1")) { err = "entryhas: '" + e + "' " + (has ? "has" : "no tiene") + " '" + t + "'"; return false; }
    return true;
}

// ---------------------------------------------------------------------------
//  juegoescenasmin / juegoescenaslog: changeScene() con escenas 3D, en el Play y compilado
// ---------------------------------------------------------------------------
// el juego A cuenta sus start() y a los 3 ticks deja un dato en getShared() y pide la cinematica
static const char* kLuaEscA =
    "-- juego A: pasa a la escena (cinematica) 'Intro' (prueba del motor)\n"
    "local n = 0\n"
    "function start() setShared(\"a_inicios\", (getShared(\"a_inicios\") or 0) + 1) end\n"
    "function update(dt)\n"
    "  n = n + 1\n"
    "  if n == 3 then setShared(\"dato\", 42); changeScene(\"Intro\") end\n"
    "end\n";
// la CINEMATICA: su animacion la arranca el motor al entrar; el script la sigue y al terminar pide el nivel
static const char* kLuaEscC =
    "-- escena Intro: una cinematica (prueba del motor)\n"
    "local request = false\n"
    "function start() setShared(\"c_inicios\", (getShared(\"c_inicios\") or 0) + 1) end\n"
    "function update(dt)\n"
    "  local f = animSceneCurrent()\n"
    "  if f == nil then return end\n"
    "  setShared(\"intro_f\", f)\n"
    "  local x = getPosition(find(\"CuboIntro\"))\n"
    "  setShared(\"intro_x\", x)\n"
    "  if f >= 11 and not request then request = true; changeScene(\"Nivel2\") end\n"
    "end\n";
// EL JUEZ de la escena B: la A se descargo (su cubo ya no esta), la B cargo (su cubo y su camara),
// el dato de getShared() sobrevivio y cada escena arranco UNA vez
static const char* kLuaEscB =
    "-- escena B: el juez de la prueba de escenas 3D\n"
    "local t, ready = 0, false\n"
    "function start() setShared(\"b_inicios\", (getShared(\"b_inicios\") or 0) + 1) end\n"
    "function update(dt)\n"
    "  if ready then return end\n"
    "  t = t + 1\n"
    "  if t < 5 then return end\n"
    "  ready = true\n"
    "  local cb, ca, cam = find(\"CuboB\") ~= nil, find(\"Cubo\") ~= nil, find(\"CamaraB\") ~= nil\n"
    "  local ci = find(\"CuboIntro\") ~= nil\n"
    "  local cinematic = (getShared(\"intro_f\") or 0) >= 11 and (getShared(\"intro_x\") or 0) > 9.9\n"
    "  local ok = getShared(\"dato\") == 42 and cb and cam and not ca and not ci and cinematic\n"
    "     and getShared(\"a_inicios\") == 1 and getShared(\"b_inicios\") == 1 and getShared(\"c_inicios\") == 1\n"
    "  info(string.format(\"[juegoescenas] %s dato=%s a=%s b=%s c=%s cine=%s(f=%s x=%s) cuboB=%s camaraB=%s cuboA=%s cuboIntro=%s\",\n"
    "    ok and \"OK\" or \"FALTA\", tostring(getShared(\"dato\")), tostring(getShared(\"a_inicios\")),\n"
    "    tostring(getShared(\"b_inicios\")), tostring(getShared(\"c_inicios\")), tostring(cinematic),\n"
    "    tostring(getShared(\"intro_f\")), tostring(getShared(\"intro_x\")), tostring(cb), tostring(cam),\n"
    "    tostring(ca), tostring(ci)))\n"
    "  setShared(\"juez\", ok and \"OK\" or \"FALTA\")\n"
    "  quit()\n"
    "end\n";
static bool WriteText(const std::string& path, const char* text) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    fputs(text, f);
    fclose(f);
    return true;
}
static void HangScript(Object* o, const char* path) {
    if (!o->scriptData) o->scriptData = new W3dScriptData();
    W3dScriptEntry e; e.path = path;
    o->scriptData->scripts.push_back(e);
}

static bool CmdGameScenesMin(std::istringstream& ss, std::string& err) {
    std::string dir; ss >> dir;
    if (dir.empty()) { err = "gamescenesmin: uso: gamescenesmin <carpeta>"; return false; }
    std::string e2;
    if (!W3dRunCommand("game3dmin " + dir, e2)) { err = "juegoescenasmin: " + e2; return false; }
    if (!WriteText(dir + "/esc_a.lua", kLuaEscA) || !WriteText(dir + "/esc_b.lua", kLuaEscB) ||
        !WriteText(dir + "/esc_c.lua", kLuaEscC)) {
        err = "gamescenesmin: no pude escribir los .lua en " + dir; return false;
    }
    // --- la escena A (la del bloque, la que arma juego3dmin) ---
    Empty* dir_a = new Empty(NULL, Vector3(0, -10, 0));
    dir_a->SetNameObj("DirectorA");
    HangScript(dir_a, "esc_a.lua");
    // --- la ESCENA "Intro": su camara, un cubo animado (su animacion ACTIVA, la que reproduce al
    //     entrar) y el script que la sigue. Es una escena aunque tenga script: se creo como escena ---
    const int c = W3dRootCreateAndOpen(W3D_ROOT_SCENE, "Intro");
    if (c < 0) { err = "gamescenesmin: no se pudo crear la escena Intro"; return false; }
    {
        Camera* camI = new Camera(NULL, Vector3(0, 0, 8), Vector3(0, 0, 0));
        camI->SetNameObj("CamaraIntro");
        camI->aspect = 1.0f;
        Mesh* ci = (Mesh*)NewMesh(MeshType(MeshType::cube), NULL, false);
        ci->SetNameObj("CuboIntro");
        HangScript(ci, "esc_c.lua");
        InitSceneAnimations();
        SceneAnimation* cinematic = new SceneAnimation("Cine");
        cinematic->startFrame = 1; cinematic->endFrame = 11; cinematic->fps = 30;
        AnimationObject ao; ao.obj = ci; ao.FirstKeyFrame = 0; ao.LastKeyFrame = 0;
        SetKeyCurve(ListProperty(ao.Properties, AnimPosition, AnimX), 1, 0.0f);
        SetKeyCurve(ListProperty(ao.Properties, AnimPosition, AnimX), 11, 10.0f);
        ao.UpdateFirstLastFrame();
        cinematic->objects.push_back(ao);
        SceneAnimations.push_back(cinematic);
        AnimSelById((int)SceneAnimations.size() - 1);   // (la animacion de la escena = la que se ve)
    }
    // --- el JUEGO B: su camara, su cubo y el juez ---
    const int b = W3dRootCreateAndOpen(W3D_ROOT_GAME, "Nivel2");
    if (b < 0) { err = "gamescenesmin: no se pudo crear el juego B"; return false; }
    Camera* cam = new Camera(NULL, Vector3(0, 0, 8), Vector3(0, 0, 0));
    cam->SetNameObj("CamaraB");
    cam->aspect = 1.0f;
    Mesh* cube = (Mesh*)NewMesh(MeshType(MeshType::cube), NULL, false);
    cube->SetNameObj("CuboB");
    Empty* judge = new Empty(NULL, Vector3(0, -10, 0));
    judge->SetNameObj("JuezB");
    HangScript(judge, "esc_b.lua");
    // se vuelve a la A: el juego arranca ahi (y "Compilar juego" necesita su HUD)
    std::string reason;
    if (!W3dActivateRoot(W3dRootBlock(), &reason)) { err = "juegoescenasmin: " + reason; return false; }
    DeselectAll(); ActiveObj = NULL;
    printf("      [juegoescenasmin] juego3dmin + DirectorA (juego A) + CamaraIntro/CuboIntro (escena 'Intro', animacion 'Cine')"
           " + CamaraB/CuboB/JuezB (juego 'Nivel2') en '%s'\n", dir.c_str());
    return true;
}

static bool CmdGameScenesLog(std::istringstream& ss, std::string& err) {
    std::string dir, name; ss >> dir >> name;
    if (dir.empty() || name.empty()) { err = "gamesceneslog: uso: gamesceneslog <carpeta> <nombre>"; return false; }
    const std::string folder = dir + "/build/linux";
    const std::string bin = folder + "/" + name;
    if (!w3dFileSystem::FileExists(bin)) { err = "gamesceneslog: no existe el binario compilado '" + bin + "'"; return false; }
    const std::string log = folder + "/whisk3d.log";
    remove(log.c_str());
    char cmdRun[2200];
    snprintf(cmdRun, sizeof(cmdRun), "cd \"%s\" && timeout 120 ./%s > /dev/null 2>&1", folder.c_str(), name.c_str());
    const int r = system(cmdRun);
    FILE* f = fopen(log.c_str(), "rb");
    if (!f) { err = "gamesceneslog: el juego no dejo whisk3d.log (se compilo en modo debug?)"; return false; }
    std::string verdict, loaded, cinematic;
    char buf[2048];
    while (fgets(buf, sizeof(buf), f)) {
        const char* p = strstr(buf, "[juegoescenas]");
        if (p) verdict = p;
        const char* q = strstr(buf, "[raices] escena 'Nivel2' cargada en la raiz activa");
        if (q) loaded = q;
        const char* r = strstr(buf, "[raices] escena 'Intro': reproduce su animacion 'Cine'");
        if (r) cinematic = r;
    }
    fclose(f);
    while (!verdict.empty() && (verdict[verdict.size() - 1] == '\n' || verdict[verdict.size() - 1] == '\r'))
        verdict.erase(verdict.size() - 1);
    printf("      [juegoescenaslog] salida=%d | %s | %s\n", r, verdict.empty() ? "(sin veredicto del juez)" : verdict.c_str(),
           loaded.empty() ? "(la escena no se cargo de su entrada)" : "la escena se cargo de su entrada");
    if (verdict.empty()) { err = "gamesceneslog: el juez no escribio su veredicto (no se cambio de escena?)"; return false; }
    printf("      [juegoescenaslog] %s\n", cinematic.empty() ? "(la cinematica no arranco)" : "la ESCENA Intro reprodujo su animacion");
    if (verdict.find("[juegoescenas] OK") == std::string::npos || verdict.find("dato=42") == std::string::npos ||
        verdict.find("cuboA=false") == std::string::npos || loaded.empty() || cinematic.empty()) {
        err = "gamesceneslog: en el juego compilado cambiarEscena() no dio lo mismo que en el Play";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
//  ESCENA vs JUEGO: el tipo, el modo del timeline, sus clips y el render del cache
// ---------------------------------------------------------------------------

static bool CmdRootType(std::istringstream& ss, std::string& err) {
    std::string t, k; ss >> t;
    bool fault = false;
    while (ss >> k) if (k == "fails") fault = true;
    if (t != "scene" && t != "game") { err = "roottype: uso: roottype escena|juego [falla]"; return false; }
    std::string reason;
    const bool ok = W3dRootConvertActive(TypeOf(t), &reason);
    printf("      [raiztipo] -> %s: %s%s%s\n", t.c_str(), ok ? "ok" : "NO", reason.empty() ? "" : " ", reason.c_str());
    if (ok == fault) { err = fault ? "roottype: se convirtio y se esperaba que no" : "raiztipo: " + reason; return false; }
    return true;
}

static bool CmdRootMode(std::istringstream& ss, std::string& err) {
    // el timeline de verdad (si el layout tiene uno) para el boton Auto Key; si no, uno de prueba
    Timeline* tl = (Timeline*)FirstLeaf(4);
    Timeline* own = NULL;
    if (!tl) { own = new Timeline(); own->Resize(900, 300); tl = own; }
    tl->SyncFields();
    const bool autokey = tl->btnAutoKey && tl->btnAutoKey->visible;
    PopupMenu m;
    BuildMenuAnim(&m);
    bool menuGame = false;
    for (size_t i = 0; i < m.items.size(); i++) if (m.items[i]->text == "Juego") menuGame = true;
    printf("      [raizmodo] kind=%d juego=%d autokey=%d menuJuego=%d play=%d\n", ActiveAnimKind, AnimIsGame ? 1 : 0,
           autokey ? 1 : 0, menuGame ? 1 : 0, PlayAnimation ? 1 : 0);
    delete own;
    std::string k;
    while (ss >> k) {
        int v = -1; if (!(ss >> v)) { err = "rootmode: falta el valor de '" + k + "'"; return false; }
        if (k == "kind" && ActiveAnimKind != v) { err = "rootmode: ActiveAnimKind=" + IsInteger(ActiveAnimKind) + ", se esperaba " + IsInteger(v); return false; }
        if (k == "game" && (AnimIsGame ? 1 : 0) != v) { err = "rootmode: AnimEsJuego no es " + IsInteger(v); return false; }
        if (k == "autokey" && (autokey ? 1 : 0) != v) { err = "rootmode: el boton Auto Key " + std::string(autokey ? "se ve" : "no se ve"); return false; }
        if (k == "gamemenu" && (menuGame ? 1 : 0) != v) { err = "rootmode: el selector " + std::string(menuGame ? "offers" : "no ofrece") + " \"Juego\""; return false; }
        if (k != "kind" && k != "game" && k != "autokey" && k != "gamemenu") { err = "rootmode: no entiendo '" + k + "'"; return false; }
    }
    return true;
}

static bool CmdRootAnim(std::istringstream& ss, std::string& err) {
    std::string n; ss >> n;
    if (n == "game") { AnimSelGame(); }
    else {
        const int i = W3dAnimSceneIdx(n.c_str());
        if (i < 0) { err = "rootanim: no hay una animacion de escena '" + n + "'"; return false; }
        AnimSelById(i);
    }
    printf("      [raizanim] %s -> kind=%d juego=%d base=%d\n", n.c_str(), ActiveAnimKind, AnimIsGame ? 1 : 0,
           W3dGameBaseHas() ? 1 : 0);
    return true;
}

static bool CmdRootClip(std::istringstream& ss, std::string& err) {
    std::string n, on; int f0 = 1, f1 = 10; float x0 = 0.0f, x1 = 1.0f;
    if (!(ss >> n >> on >> f0 >> x0 >> f1 >> x1)) { err = "rootclip: uso: rootclip <nombre> <objeto> <f0> <x0> <f1> <x1>"; return false; }
    Object* o = SceneCollection ? FindObjectByName(SceneCollection, on) : NULL;
    if (!o) { err = "rootclip: no hay un objeto '" + on + "'"; return false; }
    InitSceneAnimations();
    SceneAnimation* e = new SceneAnimation(SceneAnimFreeName(n, -1));
    e->startFrame = f0; e->endFrame = f1; e->fps = 30;
    AnimationObject ao; ao.obj = o; ao.FirstKeyFrame = 0; ao.LastKeyFrame = 0;
    SetKeyCurve(ListProperty(ao.Properties, AnimPosition, AnimX), f0, x0);
    SetKeyCurve(ListProperty(ao.Properties, AnimPosition, AnimX), f1, x1);
    ao.UpdateFirstLastFrame();
    e->objects.push_back(ao);
    SceneAnimations.push_back(e);
    printf("      [raizclip] '%s': %s.x %g (f%d) -> %g (f%d)\n", e->name.c_str(), on.c_str(), x0, f0, x1, f1);
    return true;
}

// cuantas curvas de escena (de la animacion activa) tiene el objeto activo
static int ActiveCurves() {
    for (size_t i = 0; i < AnimationObjects.size(); i++)
        if (AnimationObjects[i].obj == ActiveObj) {
            int n = 0;
            for (size_t p = 0; p < AnimationObjects[i].Properties.size(); p++)
                n += (int)AnimationObjects[i].Properties[p].keyframes.size();
            return n;
        }
    return 0;
}
static bool CmdRootKey(std::istringstream& ss, std::string& err) {
    if (!ActiveObj) { err = "rootkey: no hay objeto activo"; return false; }
    InteractionMode = ObjectMode;
    const int prev  = ActiveCurves();
    InsertKeyframeContext(0);
    const int n = ActiveCurves() - prev;
    printf("      [raizkey] Insert Keyframe en el frame %d (kind %d): %d keyframe(s) nuevos en '%s' (tiene %d)\n",
           CurrentFrame, ActiveAnimKind, n, ActiveObj->name.c_str(), prev + n);
    std::string k; int v = -1;
    if (ss >> k >> v && k == "n" && n != v) { err = "rootkey: se insertaron " + IsInteger(n) + " keyframes, se esperaban " + IsInteger(v); return false; }
    return true;
}

static bool CmdRootDope(std::istringstream& ss, std::string& err) {
    int expected = -1; ss >> expected;
    Timeline* tl = new Timeline(); tl->Resize(900, 300);
    tl->BuildDopeRows();
    const int n = (int)tl->dopeRows.size();
    delete tl;
    printf("      [raizdope] el dope sheet arma %d fila(s) (kind %d)\n", n, ActiveAnimKind);
    if (expected >= 0 && n != expected) { err = "raizdope: " + IsInteger(n) + " filas, se esperaban " + IsInteger(expected); return false; }
    return true;
}

static bool CmdRootScrub(std::istringstream& ss, std::string& err) {
    int f = 1; ss >> f;
    Timeline* tl = new Timeline(); tl->Resize(900, 300);
    tl->BuildDopeRows();
    tl->SetFrameFromX((int)(tl->FrameToX((float)f) + 0.5f));
    delete tl;
    printf("      [raizscrub] al frame %d -> CurrentFrame=%d (tick %d)\n", f, CurrentFrame, SimCurrentFrame());
    std::string k; int v = 0;
    if (ss >> k >> v && k == "cur" && CurrentFrame != v) { err = "rootscrub: el cabezal quedo en " + IsInteger(CurrentFrame) + ", se esperaba " + IsInteger(v); return false; }
    return true;
}

// ---------------------------------------------------------------------------
//  LAS CINEMATICAS: playScene (una raiz ESCENA) y los clips de objeto / capas
// ---------------------------------------------------------------------------
static bool CmdCinematicInfo(std::istringstream& ss, std::string& err) {
    const std::vector<W3dRootRow>& fs = W3dRoots();
    const int e = W3dCinematicScene(), j = W3dCinematicGame();
    const std::string ne = (e >= 0 && e < (int)fs.size()) ? fs[(size_t)e].name : std::string("-");
    const std::string nj = (j >= 0 && j < (int)fs.size()) ? fs[(size_t)j].name : std::string("-");
    printf("      [cineinfo] activa=%d escena=%s juego=%s\n", W3dCinematicActive() ? 1 : 0, ne.c_str(), nj.c_str());
    std::string k;
    while (ss >> k) {
        std::string v; if (!(ss >> v)) { err = "cinematicinfo: falta el valor de '" + k + "'"; return false; }
        if (k == "active") { if ((W3dCinematicActive() ? 1 : 0) != atoi(v.c_str())) { err = std::string("cinematicinfo: la cinematica ") + (W3dCinematicActive() ? "is" : "no esta") + " sonando"; return false; } }
        else if (k == "scene") { if (ne != v) { err = "cinematicinfo: suena '" + ne + "', se esperaba '" + v + "'"; return false; } }
        else if (k == "game") { if (nj != v) { err = "cinematicinfo: el juego es '" + nj + "', se esperaba '" + v + "'"; return false; } }
        else { err = "cinematicinfo: no entiendo '" + k + "'"; return false; }
    }
    return true;
}

static bool CmdRootObjAt(std::istringstream& ss, std::string& err) {
    std::string r, n; float x = 0, y = 0, z = 0, tol = 0.001f;
    if (!(ss >> r >> n >> x >> y >> z >> tol)) { err = "rootobjat: uso: rootobjat <raiz> <objeto> <x> <y> <z> <tol>"; return false; }
    const std::vector<W3dRootRow>& fs = W3dRoots();
    Object* root = NULL;
    for (size_t i = 0; i < fs.size() && !root; i++) if (fs[i].name == r) root = fs[i].root;
    if (!root) { err = "rootobjat: no hay una raiz cargada '" + r + "'"; return false; }
    Object* o = FindObjectByName(root, n);
    if (!o) { err = "rootobjat: '" + r + "' no tiene un objeto '" + n + "'"; return false; }
    printf("      [raizobjat] %s/%s en (%.4f,%.4f,%.4f)%s\n", r.c_str(), n.c_str(), o->pos.x, o->pos.y, o->pos.z,
           root == SceneCollection ? " (la activa)" : "");
    if (fabsf(o->pos.x - x) > tol || fabsf(o->pos.y - y) > tol || fabsf(o->pos.z - z) > tol) {
        char b[200]; snprintf(b, sizeof(b), "rootobjat: %s/%s esta en (%.4f,%.4f,%.4f), se esperaba (%.4f,%.4f,%.4f)",
                              r.c_str(), n.c_str(), o->pos.x, o->pos.y, o->pos.z, x, y, z);
        err = b; return false;
    }
    return true;
}

static bool CmdObjVis(std::istringstream& ss, std::string& err) {
    std::string n; int v = 1; ss >> n >> v;
    Object* o = SceneCollection ? FindObjectByName(SceneCollection, n) : NULL;
    if (!o) { err = "objvis: no hay un objeto '" + n + "'"; return false; }
    printf("      [objvis] '%s' visible=%d\n", n.c_str(), o->visible ? 1 : 0);
    if ((o->visible ? 1 : 0) != v) { err = "objvis: '" + n + "' " + (o->visible ? "esta visible" : "no esta visible"); return false; }
    return true;
}

// la animacion nombrada: "Escena" (una animacion de escena) o "Raiz:clip" (la VISTA de un clip de jerarquia
// sobre esa raiz: su indice en la lista de animaciones)
static int AnimByRef(const std::string& ref, bool createClip, Object** ownerOut) {
    if (ownerOut) *ownerOut = NULL;
    const size_t dp = ref.find(':');
    if (dp == std::string::npos) return W3dAnimSceneIdx(ref.c_str());
    Object* d = SceneCollection ? FindObjectByName(SceneCollection, ref.substr(0, dp)) : NULL;
    if (ownerOut) *ownerOut = d;
    if (!d) return -1;
    W3dClipHier* c = W3dHierClip(d, ref.substr(dp + 1));
    if (!c && createClip) c = W3dHierNewClip(d, ref.substr(dp + 1));
    return c ? W3dClipView(d, c) : -1;
}

static bool CmdNewAnim(std::istringstream& ss, std::string& err) {
    std::string n; int f0 = 1, f1 = 250; ss >> n >> f0 >> f1;
    if (n.empty()) { err = "animnew: uso: animnew <nombre> [f0 f1]"; return false; }
    InitSceneAnimations();
    SceneAnimation* e = new SceneAnimation(SceneAnimFreeName(n, -1));
    e->startFrame = f0; e->endFrame = f1 < f0 ? f0 : f1; e->fps = 30;
    SceneAnimations.push_back(e);
    printf("      [animnueva] '%s' %d..%d\n", e->name.c_str(), e->startFrame, e->endFrame);
    return true;
}

static bool CmdNewClip(std::istringstream& ss, std::string& err) {
    std::string d, n; int f0 = 1, f1 = 250; ss >> d >> n >> f0 >> f1;
    Object* o = SceneCollection ? FindObjectByName(SceneCollection, d) : NULL;
    if (!o || n.empty()) { err = "clipnew: uso: clipnew <raiz> <nombre> [f0 f1]"; return false; }
    W3dClipHier* c = W3dHierNewClip(o, n);
    if (!c) { err = "clipnew: no se pudo"; return false; }
    c->start = f0; c->end = f1 < f0 ? f0 : f1; c->version++;
    printf("      [clipnuevo] %s: %s (%d..%d) en la biblioteca '%s' (tamano %.3f)\n", o->name.c_str(), c->name.c_str(),
           c->start, c->end, o->clipsHier ? o->clipsHier->animset.c_str() : "?", c->size);
    return true;
}

static bool CmdAnimCurve(std::istringstream& ss, std::string& err) {
    std::string ref, on, channel; ss >> ref >> on >> channel;
    Object* owner = NULL;
    const int idx = AnimByRef(ref, false, &owner);
    if (idx < 0) { err = "animcurve: no hay una animacion '" + ref + "'"; return false; }
    Object* o = (on == ".") ? owner : (SceneCollection ? FindObjectByName(SceneCollection, on) : NULL);
    if (!o) { err = "animcurve: no hay un objeto '" + on + "'"; return false; }
    int prop = AnimPosition, comp = AnimX;
    if (channel == "y") comp = AnimY; else if (channel == "z") comp = AnimZ;
    else if (channel == "rx") prop = AnimRotation; else if (channel == "ry") { prop = AnimRotation; comp = AnimY; }
    else if (channel == "rz") { prop = AnimRotation; comp = AnimZ; }
    else if (channel == "vis") prop = AnimVisible;
    else if (channel != "x") { err = "animcurve: canal x|y|z|rx|ry|rz|vis"; return false; }
    std::vector<AnimationObject>& list = (idx == ActiveSceneAnim) ? AnimationObjects : SceneAnimations[(size_t)idx]->objects;
    AnimationObject* ao = NULL;
    for (size_t i = 0; i < list.size() && !ao; i++) if (list[i].obj == o) ao = &list[i];
    if (!ao) { AnimationObject n; n.obj = o; n.FirstKeyFrame = 0; n.LastKeyFrame = 0; list.push_back(n); ao = &list.back(); }
    int f = 0; float v = 0.0f, nk = 0;
    while (ss >> f >> v) { SetKeyCurve(ListProperty(ao->Properties, prop, comp), f, v); nk++; }
    ao->UpdateFirstLastFrame();
    // la vista de un clip de jerarquia: lo que se keyeo va a su clip (lo reproducen todas sus raices)
    if (W3dClipViewLive(idx)) W3dClipViewWrite(idx);
    W3dAnimCurvesInvalidate();
    printf("      [animcurva] %s: %s.%s <- %d keyframe(s)\n", W3dAnimSceneLabel(idx).c_str(), o->name.c_str(), channel.c_str(), (int)nk);
    return true;
}

static void CollectRootsHier(Object* o, std::vector<Object*>& out) {
    if (!o) return;
    if (o != SceneCollection && W3dHierAnimSet(o)) out.push_back(o);
    for (size_t i = 0; i < o->Childrens.size(); i++) CollectRootsHier(o->Childrens[i], out);
}
static bool CmdClipInfo(std::istringstream& ss, std::string& err) {
    std::string d; ss >> d;
    std::vector<Object*> roots;
    if (d != "*") {
        Object* o = SceneCollection ? FindObjectByName(SceneCollection, d) : NULL;
        if (!o) { err = "clipinfo: no hay un objeto '" + d + "'"; return false; }
        roots.push_back(o);
    } else CollectRootsHier(SceneCollection, roots);
    W3dClipsViewsSync();
    std::vector<std::string> names;
    for (size_t r = 0; r < roots.size(); r++) {
        const W3dAnimSet* set = W3dHierAnimSet(roots[r]);
        if (!set) continue;
        for (size_t i = 0; i < set->data.hierarchies.size(); i++) {
            const W3dClipHier* c = set->data.hierarchies[i];
            names.push_back(d != "*" ? c->name : roots[r]->name + ": " + c->name);
            printf("      [clipinfo] %s: %s (%d..%d, %d fps, %d pista(s)) en '%s'\n", roots[r]->name.c_str(),
                   c->name.c_str(), c->start, c->end, c->fps, (int)c->tracks.size(), set->name.c_str());
        }
    }
    std::string k;
    while (ss >> k) {
        std::string v; if (!(ss >> v)) { err = "clipinfo: falta el valor de '" + k + "'"; return false; }
        bool is = false;
        for (size_t i = 0; i < names.size(); i++) if (names[i] == v) is = true;
        if (k == "n") { if ((int)names.size() != atoi(v.c_str())) { err = "clipinfo: hay " + IsInteger((long)names.size()) + " clip(s), se esperaban " + v; return false; } }
        else if (k == "has") { if (!is) { err = "clipinfo: no hay un clip '" + v + "'"; return false; } }
        else if (k == "no") { if (is) { err = "clipinfo: hay un clip '" + v + "' y no deberia"; return false; } }
        else if (k == "active") {
            const SceneAnimation* e = W3dAnimIsClip(ActiveSceneAnim) ? SceneAnimations[(size_t)ActiveSceneAnim] : NULL;
            const bool ok = e && ActiveAnimKind == 0 && W3dClipViewLive(ActiveSceneAnim) &&
                            (d != "*" ? (e->owner && e->owner->name == d && e->name == v) : W3dAnimSceneLabel(ActiveSceneAnim) == v);
            if (!ok) { err = "clipinfo: lo que se edita es '" + W3dAnimSceneLabel(ActiveSceneAnim) + "', no el clip '" + v + "'"; return false; }
        }
        else { err = "clipinfo: no entiendo '" + k + "'"; return false; }
    }
    return true;
}

// ---- las bibliotecas y el reproductor de clips de jerarquia, sin lua ----
static Object* ObjectActiveRoot(const std::string& n) { return SceneCollection ? FindObjectByName(SceneCollection, n) : NULL; }

static bool CmdHierLib(std::istringstream& ss, std::string& err) {
    std::string on, lib; ss >> on >> lib;
    Object* o = ObjectActiveRoot(on);
    if (!o || lib.empty()) { err = "hierlib: uso: hierlib <objeto> <biblioteca|->"; return false; }
    std::string reason;
    const bool ok = W3dHierAssign(o, lib == "-" ? std::string() : lib, &reason);
    printf("      [jerlib] '%s' -> '%s'%s%s\n", on.c_str(), lib.c_str(), ok ? "" : ": NO: ", ok ? "" : reason.c_str());
    if (!ok) { err = "jerlib: " + reason; return false; }
    return true;
}

static bool CmdHierInfo(std::istringstream& ss, std::string& err) {
    std::string on; ss >> on;
    Object* o = ObjectActiveRoot(on);
    if (!o) { err = "hierinfo: no hay un objeto '" + on + "'"; return false; }
    W3dClipsViewsSync();
    const W3dAnimSet* set = W3dHierAnimSet(o);
    const W3dResource* r = W3dHierResource(o);
    const std::string lib = (o->clipsHier && !o->clipsHier->animset.empty()) ? o->clipsHier->animset : std::string("-");
    // 'refs' = las de sus USUARIOS (objetos y reproductores): las del historial de undo no cuentan (refsUndo)
    const int refsUse = r ? r->refTotal - (set ? set->refsUndo : 0) : 0;
    printf("      [jerinfo] '%s': biblioteca '%s' (%s), %d clip(s), refs %d (+%d del undo)\n", on.c_str(), lib.c_str(),
           r ? r->id.c_str() : "sin recurso", set ? (int)set->data.hierarchies.size() : 0, refsUse, set ? set->refsUndo : 0);
    std::string k;
    while (ss >> k) {
        std::string v; if (!(ss >> v)) { err = "hierinfo: falta el valor de '" + k + "'"; return false; }
        if (k == "lib") { if (lib != v) { err = "hierinfo: la biblioteca es '" + lib + "', se esperaba '" + v + "'"; return false; } }
        else if (k == "clips") { const int n = set ? (int)set->data.hierarchies.size() : 0;
            if (n != atoi(v.c_str())) { err = "hierinfo: tiene " + IsInteger(n) + " clip(s), se esperaban " + v; return false; } }
        else if (k == "refs") { const int n = refsUse;
            if (n != atoi(v.c_str())) { err = "hierinfo: el recurso tiene " + IsInteger(n) + " referencia(s), se esperaban " + v; return false; } }
        else if (k == "refsundo") { const int n = set ? set->refsUndo : 0;
            if (n != atoi(v.c_str())) { err = "hierinfo: el undo tiene " + IsInteger(n) + " referencia(s), se esperaban " + v; return false; } }
        else if (k == "has") { if (!W3dHierClip(o, v)) { err = "hierinfo: no tiene un clip '" + v + "'"; return false; } }
        else if (k == "tracks" || k == "path") {
            std::string x; ss >> x;
            const W3dClipHier* c = W3dHierClip(o, v);
            if (!c) { err = "hierinfo: no tiene un clip '" + v + "'"; return false; }
            if (k == "tracks") {
                printf("      [jerinfo] %s: %d pista(s):", v.c_str(), (int)c->tracks.size());
                for (size_t p = 0; p < c->tracks.size(); p++) printf(" '%s'", c->tracks[p].path.c_str());
                printf("\n");
                if ((int)c->tracks.size() != atoi(x.c_str())) { err = "hierinfo: el clip tiene " + IsInteger((long)c->tracks.size()) + " pista(s), se esperaban " + x; return false; }
            } else {
                bool is = false;
                for (size_t p = 0; p < c->tracks.size(); p++) if (c->tracks[p].path == x) is = true;
                if (!is) { err = "hierinfo: el clip '" + v + "' no tiene la pista '" + x + "'"; return false; }
            }
        }
        else if (k == "retarget") {   // retarget <clip> <completo|rotaciones>: el modo POR DEFECTO del clip
            std::string m; ss >> m;
            const W3dClipHier* c = W3dHierClip(o, v);
            if (!c) { err = "hierinfo: no tiene un clip '" + v + "'"; return false; }
            const std::string cur = c->retarget == W3D_RETARGET_ROTATIONS ? "rotations" : "complete";
            if (cur != m) { err = "hierinfo: el retarget del clip '" + v + "' es '" + cur + "', se esperaba '" + m + "'"; return false; }
        }
        else if (k == "layers") {      // capas N: cuantas capas de clips de jerarquia tiene (el Mix de la raiz)
            const int n = o->clipsHier ? (int)o->clipsHier->layers.size() : 0;
            if (n != atoi(v.c_str())) { err = "hierinfo: tiene " + IsInteger(n) + " capa(s), se esperaban " + v; return false; }
        }
        else if (k == "size") {
            float tol = 0.01f; ss >> tol;
            const W3dClipHier* c = set && !set->data.hierarchies.empty() ? set->data.hierarchies[0] : NULL;
            const float t = c ? c->size : 0.0f;
            if (fabsf(t - (float)atof(v.c_str())) > tol) { char b[160]; snprintf(b, sizeof(b), "hierinfo: el tamano del clip es %.4f, se esperaba %s", t, v.c_str()); err = b; return false; }
        }
        else { err = "hierinfo: no entiendo '" + k + "'"; return false; }
    }
    return true;
}

static bool CmdHierPath(std::istringstream& ss, std::string& err) {
    std::string rn, on, expected; ss >> rn >> on >> expected;
    Object* r = ObjectActiveRoot(rn); Object* o = ObjectActiveRoot(on);
    if (!r || !o) { err = "hierpath: uso: hierpath <raiz> <objeto> <ruta|->"; return false; }
    std::string path = W3dHierPath(r, o);
    if (path.empty()) path = "-";
    printf("      [jerruta] %s -> %s = '%s'\n", rn.c_str(), on.c_str(), path.c_str());
    if (!expected.empty() && path != expected) { err = "hierpath: da '" + path + "', se esperaba '" + expected + "'"; return false; }
    return true;
}

static bool CmdHierNode(std::istringstream& ss, std::string& err) {
    std::string rn, path, expected; ss >> rn >> path >> expected;
    Object* r = ObjectActiveRoot(rn);
    if (!r || path.empty()) { err = "hiernode: uso: hiernode <raiz> <ruta> <objeto|->"; return false; }
    Object* n = W3dHierNode(r, path);
    const std::string got = n ? n->name : std::string("-");
    printf("      [jernodo] %s / '%s' = %s\n", rn.c_str(), path.c_str(), got.c_str());
    if (!expected.empty() && got != expected) { err = "hiernode: da '" + got + "', se esperaba '" + expected + "'"; return false; }
    return true;
}

static bool CmdHierPlay(std::istringstream& ss, std::string& err) {
    std::string rn, clip, k; int loop = 1; ss >> rn >> clip;
    Object* r = ObjectActiveRoot(rn);
    if (!r || clip.empty()) { err = "hierplay: uso: hierplay <raiz> <clip> [loop 0|1] [dur D]"; return false; }
    float durEsp = -2.0f;
    while (ss >> k) {
        if (k == "loop") ss >> loop;
        else if (k == "dur") ss >> durEsp;
        else { err = "hierplay: no entiendo '" + k + "'"; return false; }
    }
    W3dClipsViewsSync();
    const float d = W3dAnimObjectPlay(r, clip, loop != 0);
    printf("      [jerplay] %s: '%s' loop=%d -> %.4f s\n", rn.c_str(), clip.c_str(), loop, d);
    if (durEsp > -1.5f && fabsf(d - durEsp) > 0.001f) { char b[128]; snprintf(b, sizeof(b), "hierplay: dura %.4f, se esperaba %.4f", d, durEsp); err = b; return false; }
    if (durEsp <= -1.5f && d < 0.0f) { err = "hierplay: '" + rn + "' no encuentra el clip '" + clip + "'"; return false; }
    return true;
}

static bool CmdHierTick(std::istringstream& ss, std::string& err) {
    float dt = 0.0f; int n = 1; ss >> dt >> n;
    if (dt <= 0.0f) { err = "hiertick: uso: hiertick <dt> [n]"; return false; }
    for (int i = 0; i < n; i++) W3dAnimObjectsTick(dt);
    printf("      [jertick] %d x %.4f s (%d raiz(ces) sonando)\n", n, dt, W3dAnimObjectsPlaying());
    return true;
}

static bool CmdHierVel(std::istringstream& ss, std::string& err) {
    std::string rn; float v = 1.0f; ss >> rn >> v;
    Object* r = ObjectActiveRoot(rn);
    if (!r) { err = "hiervel: uso: hiervel <raiz> <v>"; return false; }
    if (!W3dAnimObjectVelocity(r, v)) { err = "hiervel: a '" + rn + "' no le suena nada"; return false; }
    printf("      [jervel] %s x %.3f\n", rn.c_str(), v);
    return true;
}

static bool CmdHierRetarget(std::istringstream& ss, std::string& err) {
    std::string on, mode; ss >> on >> mode;
    Object* o = ObjectActiveRoot(on);
    if (!o || mode.empty()) { err = "hierretarget: uso: hierretarget <objeto> completo|rotaciones|clip"; return false; }
    const int m = (mode == "rotations") ? W3D_RETARGET_ROTATIONS : (mode == "clip" ? -1 : W3D_RETARGET_COMPLETE);
    if (o->getType() == ObjectType::armature) W3dArmatureRetarget((Armature*)o, m < 0 ? W3D_RETARGET_COMPLETE : m);
    else W3dAnimObjectRetarget(o, m);
    printf("      [jerretarget] %s: %s\n", on.c_str(), mode.c_str());
    return true;
}

// jerclipretarget <raiz> <clip> completo|rotaciones: el retarget por DEFECTO de un clip (el desplegable de la
// tarjeta Animacion con su vista elegida)
static bool CmdHierClipRetarget(std::istringstream& ss, std::string& err) {
    std::string rn, cn, mode; ss >> rn >> cn >> mode;
    Object* r = ObjectActiveRoot(rn);
    W3dClipHier* c = r ? W3dHierClip(r, cn) : NULL;
    if (!c || mode.empty()) { err = "hierclipretarget: uso: hierclipretarget <raiz> <clip> completo|rotaciones"; return false; }
    c->retarget = (mode == "rotations") ? W3D_RETARGET_ROTATIONS : W3D_RETARGET_COMPLETE;
    c->version++;
    printf("      [jerclipretarget] %s/%s: %s\n", rn.c_str(), cn.c_str(), mode.c_str());
    return true;
}

static bool CmdHierFrame(std::istringstream& ss, std::string& err) {
    std::string rn, fe; ss >> rn >> fe;
    Object* r = ObjectActiveRoot(rn);
    if (!r || fe.empty()) { err = "hierframe: uso: hierframe <raiz> <frame|-> [tol] [termino 0|1] [clip X]"; return false; }
    const float f = W3dAnimObjectFrame(r);
    printf("      [jerframe] %s: frame %.4f clip '%s' termino=%d\n", rn.c_str(), f, W3dAnimObjectClip(r).c_str(), W3dAnimObjectEnded(r) ? 1 : 0);
    float tol = 0.001f;
    std::string k;
    if (fe == "-") { if (f >= 0.0f) { err = "hierframe: a '" + rn + "' le suena un clip"; return false; } }
    else {
        if (ss.peek() != EOF) { std::streampos p = ss.tellg(); float t = 0; if (ss >> t) tol = t; else { ss.clear(); ss.seekg(p); } }
        if (fabsf(f - (float)atof(fe.c_str())) > tol) { char b[128]; snprintf(b, sizeof(b), "hierframe: esta en %.4f, se esperaba %s", f, fe.c_str()); err = b; return false; }
    }
    while (ss >> k) {
        std::string v; if (!(ss >> v)) { err = "hierframe: falta el valor de '" + k + "'"; return false; }
        if (k == "ended") { if ((W3dAnimObjectEnded(r) ? 1 : 0) != atoi(v.c_str())) { err = "hierframe: termino no es " + v; return false; } }
        else if (k == "clip") { if (W3dAnimObjectClip(r) != v) { err = "hierframe: suena '" + W3dAnimObjectClip(r) + "', se esperaba '" + v + "'"; return false; } }
        else { err = "hierframe: no entiendo '" + k + "'"; return false; }
    }
    return true;
}

// animrango <f0> <f1>: Start/End de la animacion ELEGIDA (como los campos de la tarjeta; la de una vista va a su clip)
static bool CmdAnimRange(std::istringstream& ss, std::string& err) {
    int f0 = 1, f1 = 250;
    if (!(ss >> f0 >> f1)) { err = "animrange: uso: animrange <f0> <f1>"; return false; }
    AnimSetStart(f0); AnimSetEnd(f1);
    if (W3dClipViewLive(ActiveSceneAnim)) W3dClipViewWrite(ActiveSceneAnim);
    printf("      [animrango] '%s' %d..%d\n", W3dAnimSceneLabel(ActiveSceneAnim).c_str(), StartFrame, EndFrame);
    return true;
}

// rotat <objeto> <rx> <ry> <rz> <tol>: la rotacion (euler, grados) de un objeto de la raiz activa
static bool CmdRotAt(std::istringstream& ss, std::string& err) {
    std::string on; float x = 0, y = 0, z = 0, tol = 0.01f; ss >> on >> x >> y >> z >> tol;
    Object* o = ObjectActiveRoot(on);
    if (!o) { err = "rotat: no hay un objeto '" + on + "'"; return false; }
    const Vector3 e = o->rotEuler;
    printf("      [rotat] %s rot (%.3f,%.3f,%.3f)\n", on.c_str(), e.x, e.y, e.z);
    if (fabsf(e.x - x) > tol || fabsf(e.y - y) > tol || fabsf(e.z - z) > tol) {
        char b[200]; snprintf(b, sizeof(b), "rotat: %s rota (%.3f,%.3f,%.3f), se esperaba (%.3f,%.3f,%.3f)", on.c_str(), e.x, e.y, e.z, x, y, z);
        err = b; return false;
    }
    return true;
}

static bool CmdHierMem(std::istringstream& ss, std::string& err) {
    W3dAnimSetsStats st;
    W3dAnimSetsGetStats(st);
    printf("      [jermem] clips de jerarquia en memoria %d (%ld keyframes), animsets vivos %d, refs %d, sonando %d\n",
           st.clipsHier, st.keysHier, st.live, st.refs, W3dAnimObjectsPlaying());
    std::string k;
    while (ss >> k) {
        std::string v; if (!(ss >> v)) { err = "hiermem: falta el valor de '" + k + "'"; return false; }
        const int n = atoi(v.c_str());
        if (k == "clips" && st.clipsHier != n) { err = "hiermem: hay " + IsInteger(st.clipsHier) + " clip(s) de jerarquia en memoria, se esperaban " + v; return false; }
        if (k == "refs" && st.refs != n) { err = "hiermem: los animsets tienen " + IsInteger(st.refs) + " referencia(s), se esperaban " + v; return false; }
        if (k == "playing" && W3dAnimObjectsPlaying() != n) { err = "hiermem: suenan " + IsInteger(W3dAnimObjectsPlaying()) + ", se esperaban " + v; return false; }
        if (k != "clips" && k != "refs" && k != "playing") { err = "hiermem: no entiendo '" + k + "'"; return false; }
    }
    return true;
}

// ---- el RETARGET de los armatures: dos esqueletos con la MISMA jerarquia de nombres y distinto tamano ----
static Armature* RigRetarget(const std::string& name, float scale, float x) {
    Armature* a = new Armature(NULL, Vector3(x, 0, 0));
    a->SetNameObj(name);
    const char* names[3] = { "Cadera", "Pecho", "Cabeza" };
    for (int k = 0; k < 3; k++) {
        W3dBone b;
        b.name = names[k];
        b.parent = k - 1;
        b.head = Vector3(0.0f, scale * (1.0f + (float)k), 0.0f);
        b.tail = Vector3(0.0f, scale * (1.5f + (float)k), 0.0f);
        a->bones.push_back(b);
    }
    PrepareAuthoredSkin(a);
    return a;
}
static bool CmdRigRetarget(std::istringstream& ss, std::string& err) {
    std::string pre; float esc = 2.0f; ss >> pre >> esc;
    if (pre.empty()) { err = "rigretarget: uso: rigretarget <prefijo> <escala>"; return false; }
    Armature* A = RigRetarget(pre + "A", 1.0f, -2.0f);
    Armature* B = RigRetarget(pre + "B", esc, 2.0f);
    // el clip se graba en A (CreateAnimation le anota su tamano): la cadera avanza en X y el pecho rota y
    // (a proposito) se estira en Y -lo que "rotaciones" no tiene que copiar-
    CreateAnimation(A);
    SkeletalAnimation* c = A->animations.back();
    c->name = "Caminar"; c->FrameRate = 30; c->startFrame = 1; c->endFrame = 11;
    BoneTrack& t0 = c->TrackOf(0);
    SetKeyCurve(t0.PropertyOf(AnimPosition, AnimX), 1, 0.0f);
    SetKeyCurve(t0.PropertyOf(AnimPosition, AnimX), 11, 10.0f);
    SetKeyCurve(t0.PropertyOf(AnimPosition, AnimY), 1, A->bones[0].restT.y);
    SetKeyCurve(t0.PropertyOf(AnimPosition, AnimY), 11, A->bones[0].restT.y);
    BoneTrack& t1 = c->TrackOf(1);
    SetKeyCurve(t1.PropertyOf(AnimRotation, AnimZ), 1, 0.0f);
    SetKeyCurve(t1.PropertyOf(AnimRotation, AnimZ), 11, 90.0f);
    SetKeyCurve(t1.PropertyOf(AnimPosition, AnimY), 1, A->bones[1].restT.y);
    SetKeyCurve(t1.PropertyOf(AnimPosition, AnimY), 11, A->bones[1].restT.y + 3.0f);
    // B usa EL MISMO clip (compartido: un animset en memoria)
    W3dArmatureAnimsLink(B, A);
    A->activeAnim = 0; B->activeAnim = 0;
    printf("      [rigretarget] %s (tamano %.3f) y %s (x%.2f, tamano %.3f) con el clip 'Caminar' (tamano de origen %.3f)\n",
           A->name.c_str(), W3dArmatureRestSize(A), B->name.c_str(), esc, W3dArmatureRestSize(B), c->heightRest);
    return true;
}
static bool CmdBonePose(std::istringstream& ss, std::string& err) {
    std::string an, hn, k; float x = 0, y = 0, z = 0, tol = 0.001f; ss >> an >> hn >> x >> y >> z >> tol;
    Object* o = ObjectActiveRoot(an);
    if (!o || o->getType() != ObjectType::armature) { err = "checkbonepose: no hay un armature '" + an + "'"; return false; }
    Armature* a = (Armature*)o;
    int h = -1;
    for (size_t i = 0; i < a->bones.size(); i++) if (a->bones[i].name == hn) h = (int)i;
    if (h < 0) { err = "checkbonepose: '" + an + "' no tiene el hueso '" + hn + "'"; return false; }
    float fr = 1.0f;
    bool hasHead = false; float hx = 0, hy = 0, hz = 0;
    while (ss >> k) {
        if (k == "frame") ss >> fr;
        else if (k == "head") { ss >> hx >> hy >> hz; hasHead = true; }   // la CABEZA del hueso tras el FK (pose)
    }
    // en JUEGO (kind 2) con su cabezal en 'fr' (como lo deja W3dArmaturesGameTick); con CAPAS, el de cada capa
    const int kind = ActiveAnimKind;
    ActiveAnimKind = 2;
    if (a->activeAnim >= 0 && a->activeAnim < (int)a->animations.size())
        a->gameFrame = fr - (float)a->animations[(size_t)a->activeAnim]->startFrame;
    for (size_t c = 0; c < a->layers.size(); c++) {
        const int ci = W3dLayerClip(a, a->layers[c]);
        if (ci >= 0) a->layers[c].gameFrame = fr - (float)a->animations[(size_t)ci]->startFrame;
    }
    a->lastPoseFrame = -999999; a->mixSignature = 0;
    EvaluatePoseSkeleton(a, 1);
    ActiveAnimKind = kind;
    const Vector3 T = a->bones[(size_t)h].poseT;
    const Vector3 R = a->bones[(size_t)h].poseR;
    const Vector3 H = a->bones[(size_t)h].poseHead;
    printf("      [huesopose] %s/%s en f%.2f: T (%.4f,%.4f,%.4f) R (%.2f,%.2f,%.2f) head (%.4f,%.4f,%.4f) retarget=%d escala=%.4f capas=%d\n",
           an.c_str(), hn.c_str(), fr, T.x, T.y, T.z, R.x, R.y, R.z, H.x, H.y, H.z, a->retarget, a->retargetScale, (int)a->layers.size());
    if (fabsf(T.x - x) > tol || fabsf(T.y - y) > tol || fabsf(T.z - z) > tol) {
        char b[200]; snprintf(b, sizeof(b), "checkbonepose: T es (%.4f,%.4f,%.4f), se esperaba (%.4f,%.4f,%.4f)", T.x, T.y, T.z, x, y, z);
        err = b; return false;
    }
    if (hasHead && (fabsf(H.x - hx) > tol || fabsf(H.y - hy) > tol || fabsf(H.z - hz) > tol)) {
        char b[200]; snprintf(b, sizeof(b), "checkbonepose: la cabeza esta en (%.4f,%.4f,%.4f), se esperaba (%.4f,%.4f,%.4f)", H.x, H.y, H.z, hx, hy, hz);
        err = b; return false;
    }
    return true;
}

// busca en el menu (y sus submenus) la opcion con ese texto (o su traduccion)
static MenuItem* FindInMenu(PopupMenu* m, const std::string& t, std::string& has, int prof) {
    if (!m || prof > 3) return NULL;
    for (size_t i = 0; i < m->items.size(); i++) {
        MenuItem* it = m->items[i];
        if (!it) continue;
        has += " [" + it->text + "]";
        if (!it->submenu && (it->text == t || it->text == T(t.c_str()))) return it;
        MenuItem* r = FindInMenu(it->submenu, t, has, prof + 1);
        if (r) return r;
    }
    return NULL;
}
static bool CmdClipMenu(std::istringstream& ss, std::string& err) {
    std::string t; std::getline(ss, t);
    const size_t a = t.find_first_not_of(' ');
    t = (a == std::string::npos) ? std::string() : t.substr(a);
    PopupMenu m;
    m.action = AnimSelById;
    BuildMenuAnim(&m);
    std::string has;
    MenuItem* it = FindInMenu(&m, t, has, 0);
    if (!it) { printf("      [clipmenu]%s\n", has.c_str()); err = "clipmenu: el selector no ofrece '" + t + "'"; return false; }
    AnimSelById(it->id);
    printf("      [clipmenu] '%s' -> editando '%s' (kind %d)\n", t.c_str(), W3dAnimSceneLabel(ActiveSceneAnim).c_str(), ActiveAnimKind);
    return true;
}

// animactiva escena | armature <objeto> <clip> [frame F]: lo que mira la LINEA DE TIEMPO (lo que guarda la sesion del
// .w3d) y que el selector de animaciones lo marque en VERDE (con cientos de clips abre scrolleado a el)
static bool CmdAnimActive(std::istringstream& ss, std::string& err) {
    std::string type; ss >> type;
    std::string obj, clip;
    bool ok = true;
    if (type == "armature") {
        ss >> obj >> clip;
        const bool has = ActiveAnimKind == 1 && ActiveAnimArm && ActiveAnimArm->activeAnim >= 0 &&
                         ActiveAnimArm->activeAnim < (int)ActiveAnimArm->animations.size();
        const std::string o = has ? ActiveAnimArm->name : std::string("-");
        const std::string c = (has && ActiveAnimArm->animations[(size_t)ActiveAnimArm->activeAnim]) ?
                              ActiveAnimArm->animations[(size_t)ActiveAnimArm->activeAnim]->name : std::string("-");
        printf("      [animactiva] kind %d, armature '%s', clip '%s'\n", ActiveAnimKind, o.c_str(), c.c_str());
        ok = has && o == obj && c == clip;
    } else if (type == "scene") {
        printf("      [animactiva] kind %d\n", ActiveAnimKind);
        ok = ActiveAnimKind == 0;
    } else { err = "animactive: uso: animactive escena | armature <objeto> <clip> [frame F]"; return false; }
    if (!ok) { err = "animactive: la linea de tiempo no esta en esa animacion"; return false; }
    std::string k; int f = 0;
    if (ss >> k >> f && k == "frame" && CurrentFrame != f) { err = "animactive: el cuadro es " + IsInteger(CurrentFrame) + ", se esperaba " + IsInteger(f); return false; }
    if (type == "armature") {   // el selector: la opcion verde es ese clip, y la entrada de su submenu tambien
        PopupMenu m; m.action = AnimSelById;
        BuildMenuAnim(&m);
        std::string green, parent;
        for (size_t i = 0; i < m.items.size(); i++) {
            MenuItem* it = m.items[i];
            if (!it || !it->submenu) continue;
            for (size_t j = 0; j < it->submenu->items.size(); j++)
                if (it->submenu->items[j] && it->submenu->items[j]->green) { green = it->submenu->items[j]->text; parent = it->green ? it->text : std::string("(sin verde)"); }
        }
        printf("      [animactiva] el selector marca '%s' en '%s'\n", green.c_str(), parent.c_str());
        if (green != clip || parent != obj) { err = "animactive: el selector no marca ese clip en verde"; return false; }
    }
    return true;
}
static bool CmdClipName(std::istringstream& ss, std::string& err) {
    std::string n; ss >> n;
    if (n.empty()) { err = "clipname: uso: clipname <nombre>"; return false; }
    const std::string q = RenameActiveScene(n);
    printf("      [clipnombre] -> '%s'\n", W3dAnimSceneLabel(ActiveSceneAnim).c_str());
    (void)q;
    return true;
}

// ---------------------------------------------------------------------------
//  juegocinemin / juegocinelog: los DOS caminos de las cinematicas, en el Play y compilado
// ---------------------------------------------------------------------------
// el JUEGO: dos clips de objeto y dos capas de escena a la vez; a los 5 ticks la cinematica
static const char* kLuaCinematicGame =
    "-- juego: los dos caminos de las cinematicas (prueba del motor)\n"
    "-- (el dt del juego compilado es de reloj: todo se compara contra lo que el propio dt predice)\n"
    "local t, requested, before, after, juzgado = 0, false, nil, 0, false\n"
    "local tc, prev2, requested2 = nil, nil, false\n"
    "function start()\n"
    "  animObject(find(\"Puerta\"), \"abrir\", false)\n"
    "  animObject(find(\"Ascensor\"), \"subir\", true)\n"
    "  sceneLayer(1, \"Luces\", 1, \"mix\", false)\n"
    "  sceneLayer(2, \"Cielo\", 1, \"mix\", false)\n"
    "  -- clips AJENOS por su nombre calificado: la Grua (sin biblioteca) al doble de velocidad, la Puerta2 (el doble\n"
    "  -- de grande) con retarget por tamano, y la Plataforma MEZCLA el clip del ascensor al 50% en una capa\n"
    "  animObject(find(\"Grua\"), \"Puerta/abrir\", false)\n"
    "  animObjectSpeed(find(\"Grua\"), 2)\n"
    "  animRetarget(find(\"Puerta2\"), \"rotations\")\n"
    "  animObject(find(\"Puerta2\"), \"Puerta/abrir\", false)\n"
    "  objectLayer(find(\"Plataforma\"), 1, \"Ascensor/subir\", 0.5, \"mix\", true)\n"
    "end\n"
    "-- los clips ajenos: la Grua va al doble que la Puerta (acotado al final de su clip), la Puerta2 sube el doble\n"
    "-- que lo que dice su frame (retarget) y la Plataforma la mitad de lo que dice su capa\n"
    "local function foreign()\n"
    "  local fg, fp, f2 = animObjectFrame(find(\"Grua\")), animObjectFrame(find(\"Puerta\")), animObjectFrame(find(\"Puerta2\"))\n"
    "  local jc = objectLayerFrame(find(\"Plataforma\"), 1)\n"
    "  -- (la altura de cada uno: la z de posicion, Z arriba)\n"
    "  local yg, y2, yp = select(3, getPosition(find(\"Grua\"))), select(3, getPosition(find(\"Puerta2\"))), select(3, getPosition(find(\"Plataforma\")))\n"
    "  local ok = fg ~= nil and f2 ~= nil and math.abs((fg - 1) - math.min(60, 2 * (fp - 1))) < 0.05 and math.abs(yg - (fg - 1)) < 0.02\n"
    "     and math.abs(y2 - 2 * (f2 - 1)) < 0.02 and jc > 0 and math.abs(yp - 0.5 * math.min(jc, 10)) < 0.02\n"
    "  return ok, string.format(\"grua=%.2f/%.2f puerta2=%.2f/%.2f plataforma=%.2f/%.2f\", fg or -1, yg, f2 or -1, y2, jc, yp)\n"
    "end\n"
    "function update(dt)\n"
    "  t = t + 1\n"
    "  setShared(\"t\", t)\n"
    "  if t == 5 then\n"
    "    -- lo que el juego va a tener al terminar ESTE tick (la cinematica arranca al final del frame)\n"
    "    before = animObjectFrame(find(\"Puerta\")) + dt * 30\n"
    "    requested = playScene(\"Cine1\", \"onReturn\")\n"
    "  end\n"
    "  -- medio segundo de juego despues de volver: todo siguio andando\n"
    "  if getShared(\"vuelta\") ~= nil then after = after + dt end\n"
    "  -- un rato despues de volver, la cinematica OTRA VEZ, pero esta la CORTA el script de la escena con\n"
    "  -- pararEscena3D(): se vuelve igual (con alCortada) y el juego sigue EXACTAMENTE donde estaba\n"
    "  if tc == nil and after >= 0.09 then\n"
    "    tc = t\n"
    "    prev2 = animObjectFrame(find(\"Puerta\")) + dt * 30\n"
    "    setShared(\"cortar\", true)\n"
    "    requested2 = playScene(\"Cine1\", \"toCut\")\n"
    "  end\n"
    "  if after >= 0.49 and not juzgado then\n"
    "    juzgado = true\n"
    "    local fp = animObjectFrame(find(\"Puerta\"))\n"
    "    local okA, txtA = foreign()\n"
    "    -- la cortada: volvio en el mismo tick en que se pidio (el juego en pausa), la corto su script antes del\n"
    "    -- final de su animacion (frames 1..11) y la escena corrio sus scripts otra vez (dos cinematicas)\n"
    "    local okC = requested2 and getShared(\"cortada\") == tc and getShared(\"iguales2\") == true\n"
    "       and (getShared(\"parada_f\") or 99) < 11 and getShared(\"c_inicios\") == 2\n"
    "    local ok = getShared(\"vuelta\") == 5 and getShared(\"iguales\") == true and (getShared(\"cine_f\") or 0) >= 8\n"
    "       and okC and requested and fp > before + 10 and not animObjectFinished(find(\"Puerta\"))\n"
    "       and animObjectCurrent(find(\"Ascensor\")) == \"subir\" and sceneLayerFrame(2) > 10 and isVisible(find(\"Lampara\")) and okA\n"
    "    info(string.format(\"[juegocine] %s vuelta=%s iguales=%s cine_f=%s c=%s puerta=%.2f antes=%.2f capa2=%.2f %s\"\n"
    "      .. \" cortada=%s/%s iguales2=%s parada_f=%s\",\n"
    "      ok and \"OK\" or \"FALTA\", tostring(getShared(\"vuelta\")), tostring(getShared(\"iguales\")),\n"
    "      tostring(getShared(\"cine_f\")), tostring(getShared(\"c_inicios\")), fp, before or -1, sceneLayerFrame(2), txtA,\n"
    "      tostring(getShared(\"cortada\")), tostring(tc), tostring(getShared(\"iguales2\")), tostring(getShared(\"parada_f\"))))\n"
    "    setShared(\"juez\", ok and \"OK\" or \"FALTA\")\n"
    "    quit()\n"
    "  end\n"
    "end\n"
    "-- al VOLVER de la cinematica: el juego EXACTAMENTE como quedo (en pausa: ni un frame de mas)\n"
    "function onReturn()\n"
    "  local fp = animObjectFrame(find(\"Puerta\"))\n"
    "  local py = select(3, getPosition(find(\"Puerta\")))   -- la altura (Z arriba)\n"
    "  setShared(\"vuelta\", t)\n"
    "  setShared(\"iguales\", math.abs(fp - before) < 0.01 and math.abs(py - (fp - 1)) < 0.01\n"
    "                            and scenePlaying() == nil)\n"
    "end\n"
    "-- al volver de la que CORTO pararEscena3D(): tambien exactamente como quedo, en el tick en que se pidio\n"
    "function toCut()\n"
    "  local fp = animObjectFrame(find(\"Puerta\"))\n"
    "  setShared(\"cortar\", false)\n"
    "  setShared(\"cortada\", t)\n"
    "  setShared(\"iguales2\", math.abs(fp - prev2) < 0.01 and scenePlaying() == nil)\n"
    "end\n";
// la ESCENA de la cinematica: la sigue (su animacion la arranca el motor)
// (la segunda vez que se la pide, el juego marca "cortar": su script la corta con stopScene3D() en su
//  segundo cuadro, antes de que termine su animacion)
static const char* kLuaCinematicScene =
    "-- escena Cine1: la cinematica (prueba del motor)\n"
    "local n = 0\n"
    "function start() setShared(\"c_inicios\", (getShared(\"c_inicios\") or 0) + 1) end\n"
    "function update(dt)\n"
    "  n = n + 1\n"
    "  local f = animSceneCurrent()\n"
    "  if getShared(\"cortar\") then\n"
    "    if n == 2 then setShared(\"parada_f\", f or -1); stopScene3D() end\n"
    "    return\n"
    "  end\n"
    "  if f ~= nil then setShared(\"cine_f\", f) end\n"
    "  setShared(\"cine_nombre\", scenePlaying() or \"\")\n"
    "end\n";

// un clip de JERARQUIA de 'root' (en su biblioteca) con una curva Y lineal f0->f1 (valores v0->v1) sobre 'o'
// (la raiz o uno de sus descendientes, por su ruta)
static void ClipY(Object* root, Object* o, const char* name, int f0, float v0, int f1, float v1) {
    W3dClipHier* c = W3dHierNewClip(root, name);
    if (!c) return;
    c->start = f0; c->end = f1; c->fps = 30;
    W3dHierTrack p;
    p.path = W3dHierPath(root, o);
    SetKeyCurve(ListProperty(p.props, AnimPosition, AnimY), f0, v0);
    SetKeyCurve(ListProperty(p.props, AnimPosition, AnimY), f1, v1);
    c->tracks.push_back(p);
    c->version++;
}
// una animacion de escena (no la activa) con una sola curva sobre 'o'
static void AnimSceneCurve(const char* name, Object* o, int prop, int comp, int f0, float v0, int f1, float v1) {
    InitSceneAnimations();
    SceneAnimation* e = new SceneAnimation(SceneAnimFreeName(name, -1));
    e->startFrame = f0; e->endFrame = f1; e->fps = 30;
    AnimationObject ao; ao.obj = o; ao.FirstKeyFrame = 0; ao.LastKeyFrame = 0;
    SetKeyCurve(ListProperty(ao.Properties, prop, comp), f0, v0);
    SetKeyCurve(ListProperty(ao.Properties, prop, comp), f1, v1);
    ao.UpdateFirstLastFrame();
    e->objects.push_back(ao);
    SceneAnimations.push_back(e);
}

static bool CmdGameCinematicMin(std::istringstream& ss, std::string& err) {
    std::string dir; ss >> dir;
    if (dir.empty()) { err = "gamecinematicmin: uso: gamecinematicmin <carpeta>"; return false; }
    std::string e2;
    if (!W3dRunCommand("game3dmin " + dir, e2)) { err = "juegocinemin: " + e2; return false; }
    if (!WriteText(dir + "/cine_juego.lua", kLuaCinematicGame) || !WriteText(dir + "/cine_escena.lua", kLuaCinematicScene)) {
        err = "gamecinematicmin: no pude escribir los .lua en " + dir; return false;
    }
    // --- el JUEGO (la raiz del bloque): la puerta y el ascensor con SUS clips de jerarquia, una lampara que se prende
    //     con la capa "Luces" y el cubo de juego3dmin que la capa "Cielo" corre en X ---
    Mesh* door = (Mesh*)NewMesh(MeshType(MeshType::cube), NULL, false);
    door->SetNameObj("Puerta"); door->pos = Vector3(-3, 0, 0);
    Mesh* asc = (Mesh*)NewMesh(MeshType(MeshType::cube), NULL, false);
    asc->SetNameObj("Ascensor"); asc->pos = Vector3(3, 0, 0);
    Mesh* lamp = (Mesh*)NewMesh(MeshType(MeshType::cube), NULL, false);
    lamp->SetNameObj("Lampara"); lamp->pos = Vector3(0, 3, 0); lamp->visible = false;
    Object* cube = FindObjectByName(SceneCollection, "Cubo");
    ClipY(door, door, "abrir", 1, 0.0f, 61, 60.0f);    // una pasada, larga: y = frame - 1
    ClipY(asc, asc, "subir", 1, 0.0f, 11, 10.0f);      // con loop
    // los que reproducen clips AJENOS por su nombre calificado (sin biblioteca propia): la Grua, la Puerta2 (el
    // doble de grande: retarget) y la Plataforma (una capa que mezcla el del ascensor)
    Mesh* crane = (Mesh*)NewMesh(MeshType(MeshType::cube), NULL, false);
    crane->SetNameObj("Grua"); crane->pos = Vector3(-6, 0, 0);
    Mesh* door2 = (Mesh*)NewMesh(MeshType(MeshType::cube), NULL, false);
    door2->SetNameObj("Puerta2"); door2->pos = Vector3(-9, 0, 0); door2->scale = Vector3(2, 2, 2);
    Mesh* plat = (Mesh*)NewMesh(MeshType(MeshType::cube), NULL, false);
    plat->SetNameObj("Plataforma"); plat->pos = Vector3(6, 0, 0);
    AnimSceneCurve("Luces", lamp, AnimVisible, AnimX, 1, 0.0f, 3, 1.0f);
    if (cube) AnimSceneCurve("Cielo", cube, AnimPosition, AnimX, 1, 0.0f, 21, 2.0f);
    Empty* dirj = new Empty(NULL, Vector3(0, -10, 0));
    dirj->SetNameObj("DirectorCine");
    HangScript(dirj, "cine_juego.lua");
    // --- la ESCENA "Cine1": su camara, su luz y un cubo con su animacion (la que se reproduce) ---
    const int c = W3dRootCreateAndOpen(W3D_ROOT_SCENE, "Cine1");
    if (c < 0) { err = "gamecinematicmin: no se pudo crear la escena Cine1"; return false; }
    {
        Camera* camC = new Camera(NULL, Vector3(0, 0, 8), Vector3(0, 0, 0));
        camC->SetNameObj("CamCine");
        camC->aspect = 1.0f;
        Mesh* cc = (Mesh*)NewMesh(MeshType(MeshType::cube), NULL, false);
        cc->SetNameObj("CuboCine");
        HangScript(cc, "cine_escena.lua");
        InitSceneAnimations();
        SceneAnimation* cinematic = new SceneAnimation("Anim");
        cinematic->startFrame = 1; cinematic->endFrame = 11; cinematic->fps = 30;
        AnimationObject ao; ao.obj = cc; ao.FirstKeyFrame = 0; ao.LastKeyFrame = 0;
        SetKeyCurve(ListProperty(ao.Properties, AnimPosition, AnimX), 1, 0.0f);
        SetKeyCurve(ListProperty(ao.Properties, AnimPosition, AnimX), 11, 10.0f);
        ao.UpdateFirstLastFrame();
        cinematic->objects.push_back(ao);
        SceneAnimations.push_back(cinematic);
        AnimSelById((int)SceneAnimations.size() - 1);   // (la animacion de la escena = la que se reproduce)
    }
    std::string reason;
    if (!W3dActivateRoot(W3dRootBlock(), &reason)) { err = "juegocinemin: " + reason; return false; }
    DeselectAll(); ActiveObj = NULL;
    printf("      [juegocinemin] juego3dmin + Puerta/Ascensor (clips de jerarquia) + Grua/Puerta2/Plataforma (clips ajenos:"
           " velocidad, retarget, capa) + Luces/Cielo (capas) + DirectorCine;"
           " escena 'Cine1' (CamCine, CuboCine, animacion 'Anim') en '%s'\n", dir.c_str());
    return true;
}

static bool CmdGameCinematicLog(std::istringstream& ss, std::string& err) {
    std::string dir, name; ss >> dir >> name;
    if (dir.empty() || name.empty()) { err = "gamecinematiclog: uso: gamecinematiclog <carpeta> <nombre>"; return false; }
    const std::string folder = dir + "/build/linux";
    const std::string bin = folder + "/" + name;
    if (!w3dFileSystem::FileExists(bin)) { err = "gamecinematiclog: no existe el binario compilado '" + bin + "'"; return false; }
    const std::string log = folder + "/whisk3d.log";
    remove(log.c_str());
    char cmdRun[2200];
    snprintf(cmdRun, sizeof(cmdRun), "cd \"%s\" && timeout 120 ./%s > /dev/null 2>&1", folder.c_str(), name.c_str());
    const int r = system(cmdRun);
    FILE* f = fopen(log.c_str(), "rb");
    if (!f) { err = "gamecinematiclog: el juego no dejo whisk3d.log (se compilo en modo debug?)"; return false; }
    std::string verdict, loaded, turn;
    char buf[2048];
    while (fgets(buf, sizeof(buf), f)) {
        const char* p = strstr(buf, "[juegocine]");
        if (p) verdict = p;
        if (strstr(buf, "[raices] scene 'Cine1' cargada desde")) loaded = "si";   // (W3dRootLoad: de su entrada)
        const char* q = strstr(buf, "[raices] cinematica 'Cine1' terminada");
        if (q) turn = q;
    }
    fclose(f);
    while (!verdict.empty() && (verdict[verdict.size() - 1] == '\n' || verdict[verdict.size() - 1] == '\r'))
        verdict.erase(verdict.size() - 1);
    printf("      [juegocinelog] salida=%d | %s | %s | %s\n", r, verdict.empty() ? "(sin veredicto del juez)" : verdict.c_str(),
           loaded.empty() ? "(la escena no se cargo de su entrada)" : "la escena se cargo de su entrada",
           turn.empty() ? "(la cinematica no volvio)" : "la cinematica termino y volvio al juego");
    if (verdict.find("[juegocine] OK") == std::string::npos || loaded.empty() || turn.empty()) {
        err = "gamecinematiclog: en el juego compilado las cinematicas no dieron lo mismo que en el Play";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
//  EL UNDO Y LA UI DE LOS CLIPS DE JERARQUIA, EL RETARGET Y LAS CAPAS (fase 6, segunda vuelta)
// ---------------------------------------------------------------------------
// el objeto pasa a ser el activo y el unico seleccionado (como un click en el outliner)
static Object* PickObject(const std::string& on) {
    Object* o = ObjectActiveRoot(on);
    if (!o) return NULL;
    DeselectAll();
    o->Select();
    ActiveObj = o;
    return o;
}
// el resto de la linea (textos con espacios: "New Library", "Rotations only")
static std::string RestLine(std::istringstream& ss) {
    std::string t; std::getline(ss, t);
    const size_t a = t.find_first_not_of(' ');
    return (a == std::string::npos) ? std::string() : t.substr(a);
}
// jerlibmenu <objeto> <texto>: el desplegable "Clips" de la tarjeta Animacion (con su paso de undo)
static bool CmdHierLibMenu(std::istringstream& ss, std::string& err) {
    std::string on; ss >> on;
    const std::string t = RestLine(ss);
    if (!PickObject(on) || t.empty()) { err = "hierlibmenu: uso: hierlibmenu <objeto> <(none)|New Library|biblioteca>"; return false; }
    if (!PropsHierLibPick(t)) { err = "hierlibmenu: el desplegable no ofrece '" + t + "'"; return false; }
    Object* o = ActiveObj;
    printf("      [jerlibmenu] %s -> '%s'\n", on.c_str(), (o && o->clipsHier) ? o->clipsHier->animset.c_str() : "");
    return true;
}
// objretarget <objeto> <texto>: el desplegable "Retarget" del objeto (con su paso de undo). Asserta con 'es':
//   objretarget <objeto> es <completo|rotaciones|clip>
static bool CmdObjRetarget(std::istringstream& ss, std::string& err) {
    std::string on; ss >> on;
    std::string t = RestLine(ss);
    Object* o = ObjectActiveRoot(on);
    if (!o || t.empty()) { err = "objretarget: uso: objretarget <objeto> <Clip default|Complete|Rotations only> | es <modo>"; return false; }
    const int m = (o->getType() == ObjectType::armature) ? ((Armature*)o)->retarget : (o->clipsHier ? o->clipsHier->retarget : -1);
    const std::string cur = m == W3D_RETARGET_ROTATIONS ? "rotations" : m == W3D_RETARGET_COMPLETE ? "complete" : "clip";
    if (t.compare(0, 3, "es ") == 0) {
        const std::string esp = t.substr(3);
        printf("      [objretarget] %s: %s\n", on.c_str(), cur.c_str());
        if (esp != cur) { err = "objretarget: el retarget de '" + on + "' es '" + cur + "', se esperaba '" + esp + "'"; return false; }
        return true;
    }
    if (!PickObject(on) || !PropsObjRetargetPick(t)) { err = "objretarget: el desplegable no ofrece '" + t + "'"; return false; }
    printf("      [objretarget] %s <- '%s'\n", on.c_str(), t.c_str());
    return true;
}
// clipretargetmenu <texto>: el desplegable "Retarget" del clip de jerarquia elegido (con su paso de undo)
static bool CmdClipRetargetMenu(std::istringstream& ss, std::string& err) {
    const std::string t = RestLine(ss);
    if (!W3dClipViewLive(ActiveSceneAnim)) { err = "clipretargetmenu: lo que se edita no es un clip de jerarquia"; return false; }
    if (!PropsClipRetargetPick(t)) { err = "clipretargetmenu: el desplegable no ofrece '" + t + "'"; return false; }
    printf("      [clipretargetmenu] '%s' <- %s\n", W3dAnimSceneLabel(ActiveSceneAnim).c_str(), t.c_str());
    return true;
}
// animmenos: el "-" de la tarjeta Animation (la animacion elegida; una escena o la vista de un clip: con undo)
static bool CmdAnimMinus(std::istringstream& ss, std::string& err) {
    (void)ss; (void)err;
    const std::string prev = W3dAnimSceneLabel(ActiveSceneAnim);
    _AnimOfCardFwd();
    printf("      [animmenos] '%s' -> queda '%s' (kind %d)\n", prev.c_str(), W3dAnimSceneLabel(ActiveSceneAnim).c_str(), ActiveAnimKind);
    return true;
}
// animlista [n N] [activa X] [tiene X]... [no X]...   las animaciones de escena de la raiz activa (sus etiquetas;
// las vistas HUERFANAS no cuentan: no se listan en la UI) y la elegida. La vista de un clip se escribe "Raiz:clip"
// (la etiqueta de la UI sin el espacio, como en animcurva).
static std::string CompactLabel(int idx) {
    std::string e = W3dAnimSceneLabel(idx);
    const size_t p = e.find(": ");
    if (p != std::string::npos && W3dAnimIsClip(idx)) e.erase(p + 1, 1);
    return e;
}
static bool CmdAnimList(std::istringstream& ss, std::string& err) {
    InitSceneAnimations();
    std::vector<std::string> et;
    std::string txt;
    for (size_t i = 0; i < SceneAnimations.size(); i++) {
        if (SceneAnimations[i]->isClip && !W3dClipViewLive((int)i)) continue;
        et.push_back(CompactLabel((int)i));
        txt += " '" + et.back() + "'";
    }
    const std::string act = (ActiveAnimKind == 0) ? CompactLabel(ActiveSceneAnim) : std::string("-");
    printf("      [animlista]%s (activa '%s')\n", txt.c_str(), act.c_str());
    std::string k;
    while (ss >> k) {
        std::string v; if (!(ss >> v)) { err = "animlist: falta el valor de '" + k + "'"; return false; }
        bool is = false;
        for (size_t i = 0; i < et.size(); i++) if (et[i] == v) is = true;
        if (k == "n") { if ((int)et.size() != atoi(v.c_str())) { err = "animlist: hay " + IsInteger((long)et.size()) + ", se esperaban " + v; return false; } }
        else if (k == "active") { if (act != v) { err = "animlist: la elegida es '" + act + "', se esperaba '" + v + "'"; return false; } }
        else if (k == "has") { if (!is) { err = "animlist: no esta '" + v + "'"; return false; } }
        else if (k == "no") { if (is) { err = "animlist: esta '" + v + "' y no deberia"; return false; } }
        else { err = "animlist: no entiendo '" + k + "'"; return false; }
    }
    return true;
}
// jercapa <raiz> <n> <clip|-> [infl %] [modo mezclar|sumar|restar] [vel V] [desde D] [loop 0|1] [frame F]
//   la capa n (base 1) de clips de jerarquia de la raiz (como objectLayer de lua); '-' la saca (y las de arriba)
static bool CmdHierLayer(std::istringstream& ss, std::string& err) {
    std::string rn, clip; int n = 0; ss >> rn >> n >> clip;
    Object* r = ObjectActiveRoot(rn);
    if (!r || n < 1 || clip.empty()) { err = "hierlayer: uso: hierlayer <raiz> <n> <clip|-> [infl %] [modo m] [vel v] [desde d] [loop 0|1] [frame f]"; return false; }
    if (clip == "-") {
        if (r->clipsHier && (int)r->clipsHier->layers.size() >= n) { r->clipsHier->layers.resize(n - 1); W3dHierLayersChanged(r); W3dMixScenesRelease(); }
        printf("      [jercapa] %s: %d capa(s)\n", rn.c_str(), r->clipsHier ? (int)r->clipsHier->layers.size() : 0);
        return true;
    }
    if (!r->clipsHier) r->clipsHier = new W3dHierRoot();
    if ((int)r->clipsHier->layers.size() < n) r->clipsHier->layers.resize(n);
    W3dLayerAnim& c = r->clipsHier->layers[(size_t)n - 1];
    c.anim = clip; c.retClip = 0;
    std::string k;
    while (ss >> k) {
        if (k == "infl") ss >> c.influence;
        else if (k == "mode") { std::string m; ss >> m; c.mode = (m == "add") ? 1 : (m == "subtract") ? 2 : 0; }
        else if (k == "vel") ss >> c.vel;
        else if (k == "from") ss >> c.from;
        else if (k == "loop") { int l = 1; ss >> l; c.loop = l != 0; }
        else if (k == "frame") ss >> c.gameFrame;
        else { err = "hierlayer: no entiendo '" + k + "'"; return false; }
    }
    W3dHierLayersChanged(r);
    printf("      [jercapa] %s capa %d: '%s' %.0f%% modo %d (clip %s)\n", rn.c_str(), n, clip.c_str(), c.influence, c.mode,
           W3dHierLayerClip(r, c, 0) ? "found" : "NO ENCONTRADO");
    return true;
}
// jermix [frame F] | [tick DT [n]]: el Mix de objetos (capas de escena + de jerarquia) aplicado en el frame F del
// editor, o n ticks del JUEGO (cada capa avanza su cabezal)
static bool CmdHierMix(std::istringstream& ss, std::string& err) {
    std::string k; ss >> k;
    if (k == "frame") { int f = 1; ss >> f; const bool has = W3dMixScenesApply(f, false);
        printf("      [jermix] frame %d: %s\n", f, has ? "mezcla aplicada" : "sin capas"); return true; }
    if (k == "tick") { float dt = 0.0f; int n = 1; ss >> dt >> n;
        for (int i = 0; i < n; i++) W3dMixScenesTick(dt);
        printf("      [jermix] %d tick(s) de %.4f s\n", n, dt); return true; }
    if (k == "release") { W3dMixScenesRelease(); printf("      [jermix] los objetos vuelven a su base\n"); return true; }
    err = "hiermix: uso: hiermix frame F | tick DT [n] | soltar";
    return false;
}
// mixjer <raiz> <clip>: el menu "Agregar" del Mix, submenu de esa raiz, item del clip (la capa queda elegida)
static bool CmdMixHier(std::istringstream& ss, std::string& err) {
    std::string rn, clip; ss >> rn >> clip;
    Object* r = ObjectActiveRoot(rn);
    if (!r || clip.empty()) { err = "mixhier: uso: mixhier <raiz> <clip>"; return false; }
    if (!PropsMixAddMenu(rn, clip)) { err = "mixhier: el Mix no ofrece '" + clip + "' para '" + rn + "'"; return false; }
    printf("      [mixjer] %s: capa '%s' (editando '%s')\n", rn.c_str(), clip.c_str(), W3dAnimSceneLabel(ActiveSceneAnim).c_str());
    return true;
}
// mixfilas [n N] [tiene X]...   las filas del arbol del Mix (lo que dibuja la tarjeta)
static bool CmdMixRows(std::istringstream& ss, std::string& err) {
    const int n = MixRowsCount ? MixRowsCount() : 0;
    std::vector<std::string> fs;
    std::string txt;
    for (int i = 0; i < n; i++) { fs.push_back(MixRowText(i)); txt += " [" + fs.back() + "]"; }
    printf("      [mixfilas]%s\n", txt.c_str());
    std::string k;
    while (ss >> k) {
        std::string v; if (!(ss >> v)) { err = "mixrows: falta el valor de '" + k + "'"; return false; }
        if (k == "n") { if (n != atoi(v.c_str())) { err = "mixrows: hay " + IsInteger(n) + " fila(s), se esperaban " + v; return false; } }
        else if (k == "has") { bool is = false;
            for (size_t i = 0; i < fs.size(); i++) if (fs[i].find(v) != std::string::npos) is = true;
            if (!is) { err = "mixrows: ninguna fila dice '" + v + "'"; return false; } }
        else { err = "mixrows: no entiendo '" + k + "'"; return false; }
    }
    return true;
}
// rigcapa <armature> <clip> [infl %]: una capa del Mix del armature con ese clip (animLayer sin lua)
static bool CmdRigLayer(std::istringstream& ss, std::string& err) {
    std::string an, clip; float infl = 100.0f; ss >> an >> clip >> infl;
    Object* o = ObjectActiveRoot(an);
    if (!o || o->getType() != ObjectType::armature || clip.empty()) { err = "riglayer: uso: riglayer <armature> <clip|-> [infl]"; return false; }
    Armature* a = (Armature*)o;
    if (clip == "-") { a->layers.clear(); a->lastPoseFrame = -999999; printf("      [rigcapa] %s: sin capas\n", an.c_str()); return true; }
    W3dLayerAnim c; c.anim = clip; c.influence = infl;
    a->layers.push_back(c);
    a->lastPoseFrame = -999999; a->mixSignature = 0;
    printf("      [rigcapa] %s: %d capa(s)\n", an.c_str(), (int)a->layers.size());
    return true;
}
// rigbiped <armature>...: esos armatures pasan a reconstruir el FK desde sus TransformLink (el camino del biped
// de un FBX): la traslacion de cada hueso entra como DELTA contra su rest
static bool CmdRigBiped(std::istringstream& ss, std::string& err) {
    std::string an; int n = 0;
    while (ss >> an) {
        Object* o = ObjectActiveRoot(an);
        if (!o || o->getType() != ObjectType::armature) { err = "rigbiped: no hay un armature '" + an + "'"; return false; }
        Armature* a = (Armature*)o;
        a->skinRebuildFK = true;
        a->skinAuthored = false;   // (el tail sale del FK, como en un rig importado)
        a->retargetClip = NULL; a->lastPoseFrame = -999999;
        printf("      [rigbiped] %s: FK reconstruido, tamano %.4f\n", an.c_str(), W3dArmatureRestSize(a));
        n++;
    }
    if (!n) { err = "rigbiped: uso: rigbiped <armature>..."; return false; }
    return true;
}
// rigskinglb <armature> <ruta.glb>: una malla (un cubo, todo al primer hueso) skinneada al armature, exportada
// SOLA a un GLB (el armature y sus clips van con ella). La malla se borra despues.
static bool CmdRigSkinGlb(std::istringstream& ss, std::string& err) {
    std::string an, path; ss >> an >> path;
    Object* o = ObjectActiveRoot(an);
    if (!o || o->getType() != ObjectType::armature || path.empty()) { err = "rigskinglb: uso: rigskinglb <armature> <ruta.glb>"; return false; }
    Armature* a = (Armature*)o;
    Mesh* m = (Mesh*)NewMesh(MeshType(MeshType::cube), NULL, false);
    m->SetNameObj("PielGlb");
    m->GenerateRender();
    WeightPaintEnsureMap(m);
    VertexGroup* vg = new VertexGroup(a->bones.empty() ? std::string("x") : a->bones[0].name);
    std::vector<char> done(m->vertexSize, 0);
    for (int i = 0; i < m->vertexSize && i < (int)m->vertCtrlPoint.size(); i++) {
        const int cp = m->vertCtrlPoint[i];
        if (cp < 0 || cp >= (int)done.size() || done[cp]) continue;
        done[cp] = 1; vg->verts.push_back(cp); vg->weights.push_back(1.0f);
    }
    m->vertexGroups.push_back(vg);
    m->skinArmature = a;
    DeselectAll(); m->Select(); ActiveObj = m;
    const bool ok = ExportGLTF(path, true, true);
    printf("      [rigskinglb] %s -> %s: %s\n", an.c_str(), path.c_str(), ok ? "OK" : "FALLO");
    m->skinArmature = NULL;
    // fuera del arbol ANTES de liberarla (~Object no se descuelga de su padre)
    DeselectAll(); ActiveObj = NULL;
    if (m->Parent) {
        std::vector<Object*>& hs = m->Parent->Childrens;
        for (size_t i = 0; i < hs.size(); i++) if (hs[i] == m) { hs.erase(hs.begin() + (long)i); break; }
        m->Parent = NULL;
    }
    delete m;
    if (!ok) { err = "rigskinglb: no se pudo exportar"; return false; }
    return true;
}
// importclipsalto <ruta.glb|.gltf> [n N]: importa y verifica que CADA clip de los armatures nuevos trae el TAMANO
// del esqueleto que lo grabo (heightRest = W3dArmatureRestSize del armature importado, > 0)
static void CollectArms(Object* o, std::vector<Armature*>& out) {
    if (!o) return;
    if (o->getType() == ObjectType::armature) out.push_back((Armature*)o);
    for (size_t i = 0; i < o->Childrens.size(); i++) CollectArms(o->Childrens[i], out);
}
static bool CmdImportClipsHeight(std::istringstream& ss, std::string& err) {
    std::string path, k; int nEsp = -1; ss >> path;
    while (ss >> k) { if (k == "n") ss >> nEsp; }
    std::vector<Armature*> prev; CollectArms(SceneCollection, prev);
    if (!ImportGLTF(path)) { err = "importclipsheight: no se pudo importar '" + path + "'"; return false; }
    std::vector<Armature*> after; CollectArms(SceneCollection, after);
    int clips = 0;
    for (size_t i = 0; i < after.size(); i++) {
        if (std::find(prev.begin(), prev.end(), after[i]) != prev.end()) continue;
        Armature* a = after[i];
        const float size = W3dArmatureRestSize(a);
        for (size_t c = 0; c < a->animations.size(); c++) {
            const SkeletalAnimation* cl = a->animations[c];
            if (!cl) continue;
            clips++;
            printf("      [importclipsalto] %s/%s: alturaReposo %.4f (tamano del armature %.4f)\n", a->name.c_str(), cl->name.c_str(), cl->heightRest, size);
            if (cl->heightRest <= 0.0f || fabsf(cl->heightRest - size) > 1e-4f) {
                err = "importclipsheight: el clip '" + cl->name + "' no trae el tamano de su esqueleto"; return false;
            }
        }
    }
    if (nEsp >= 0 && clips != nEsp) { err = "importclipsheight: se importaron " + IsInteger(clips) + " clip(s), se esperaban " + IsInteger(nEsp); return false; }
    return true;
}

// ---------------------------------------------------------------------------
//  notif / keyrombo / menuobjeto: lo que se le avisa al usuario y los caminos de la UI que keyean
// ---------------------------------------------------------------------------
static bool CmdNotif(std::istringstream& ss, std::string& err) {
    std::string k; ss >> k;
    if (k == "clear") { NotificationsClear(); return true; }
    const std::string t = RestLine(ss);
    if ((k != "has" && k != "no") || t.empty()) { err = "notif: uso: notif tiene <texto> | notif no <texto> | notif limpiar"; return false; }
    // (el texto en ingles, como va en T(): se busca tambien traducido, que es como sale en pantalla)
    const std::string tr = T(t.c_str());
    const bool has = NotificationVisible(t) || NotificationVisible(tr);
    printf("      [notif] '%s' ('%s'): %s\n", t.c_str(), tr.c_str(), has ? "se ve" : "no se ve");
    if (k == "has" && !has) { err = "notif: no hay ninguna notificacion con '" + t + "'"; return false; }
    if (k == "no" && has) { err = "notif: hay una notificacion con '" + t + "'"; return false; }
    return true;
}
static bool CmdKeyDiamond(std::istringstream& ss, std::string& err) {
    int prop = -1, comp = 0; ss >> prop >> comp;
    if (prop < 0) { err = "keydiamond: uso: keydiamond <prop> <comp> [estado N]"; return false; }
    if (!W3dKeyframeToggle || !W3dKeyframeState) { err = "keydiamond: el panel no cableo los hooks del rombo"; return false; }
    W3dKeyframeToggle(prop, comp);
    const int est = W3dKeyframeState(prop, comp);
    printf("      [keyrombo] prop=%d comp=%d frame=%d -> estado=%d\n", prop, comp, CurrentFrame, est);
    std::string k; int v = 0;
    if (ss >> k >> v && k == "state" && est != v) { err = "keydiamond: el rombo quedo en " + IsInteger(est) + ", se esperaba " + IsInteger(v); return false; }
    return true;
}
static bool CmdMenuObject(std::istringstream& ss, std::string& err) {
    int id = -1; ss >> id;
    if (id < 0) { err = "menuobject: uso: menuobject <id>"; return false; }
    LayoutActionObject(id);
    return true;
}

// ============================================================================
//  pruebacapas <carpeta> : el proyecto de prueba de las CAPAS (W3dScene.h): dos juegos ("Nivel 1", la base, y
//  "Nivel 2"), una escena 3D ("Decorado") y dos escenas 2D en el Nivel 1 ("HUD", la inicial, y "Menu"). Cada una con
//  un script que anota lo que le pasa en getShared() y en el log (depurar):
//    nivel1.lua   start(): loadScene("Menu") y loadScene("Decorado"); cuenta sus update(); alRecibir
//    menu.lua     start(): send("Nivel 1", "hola", {n = 5}); en sus actualizar 5/10 pausa y reanuda el Nivel 1, en
//                 el 15 changeScene("Nivel 2") (el menu sigue) y en el 25 unloadScene("Decorado")
//    deco.lua     cuenta sus update()        nivel2.lua   start() y alRecibir
//  Despues: simplay, scriptinfo compartido ..., guardarw3d, compilarjuego.
// ============================================================================
static bool WriteLuaLayers(const std::string& path, const char* t) {
    FILE* f = fopen(path.c_str(), "w");
    if (!f) return false;
    fputs(t, f);
    return fclose(f) == 0;
}
static Mesh* CubeWithScript(const char* name, const std::string& lua, float x) {
    Mesh* c = (Mesh*)NewMesh(MeshType(MeshType::cube), NULL, false);
    if (!c) return NULL;
    c->SetNameObj(name);
    c->pos = Vector3(x, 0, 0);
    if (!lua.empty()) {
        if (!c->scriptData) c->scriptData = new W3dScriptData();
        W3dScriptEntry e; e.path = lua; c->scriptData->scripts.push_back(e);
    }
    return c;
}
static bool CmdTestLayers(std::istringstream& ss, std::string& err) {
    std::string dir; ss >> dir;
    if (dir.empty()) { err = "testlayers: uso: testlayers <carpeta>"; return false; }
    { std::string cmd = "mkdir -p \"" + dir + "\""; if (system(cmd.c_str())) {} }
    const bool ok =
        WriteLuaLayers(dir + "/nivel1.lua",
            "local t = 0\n"
            "function start()\n"
            "  loadScene(\"Menu\")\n  loadScene(\"Decorado\")\n"
            "  setShared(\"nivel1\", \"inicio\")\n  debugLog(\"[capas] nivel1 inicio\")\nend\n"
            "function update(dt)\n  t = t + 1\n  setShared(\"nivel1_ticks\", t)\nend\n"
            "function onReceive(msg, data, from)\n"
            "  local r = msg .. \":\" .. tostring(data and data.n) .. \":\" .. tostring(from)\n"
            "  setShared(\"nivel1_recibio\", r)\n  debugLog(\"[capas] nivel1 recibio \" .. r)\nend\n") &&
        WriteLuaLayers(dir + "/menu.lua",
            "local t = 0\n"
            "function start()\n  setShared(\"menu\", \"inicio\")\n  debugLog(\"[capas] menu inicio\")\n"
            "  send(\"Nivel 1\", \"hola\", {n = 5})\nend\n"
            "function update(dt)\n  t = t + 1\n  setShared(\"menu_ticks\", t)\n"
            "  if t == 5 then pauseScene(\"Nivel 1\") end\n"
            "  if t == 10 then resumeScene(\"Nivel 1\") end\n"
            "  if t == 15 then debugLog(\"[capas] menu: cambiarEscena Nivel 2\") changeScene(\"Nivel 2\") end\n"
            "  if t == 20 then send(\"Nivel 2\", \"hola\", {n = 7}) end\n"
            "  if t == 25 then unloadScene(\"Decorado\") end\n"
            "  if t % 10 == 0 then debugLog(\"[capas] menu t=\" .. t .. \" decorado=\" .. tostring(sceneLoaded(\"Decorado\"))) end\nend\n") &&
        WriteLuaLayers(dir + "/deco.lua",
            "local t = 0\nfunction update(dt)\n  t = t + 1\n  setShared(\"deco_ticks\", t)\nend\n") &&
        WriteLuaLayers(dir + "/nivel2.lua",
            "function start()\n  setShared(\"nivel2\", \"inicio\")\n  debugLog(\"[capas] nivel2 inicio\")\nend\n"
            "function onReceive(msg, data, from)\n"
            "  local r = msg .. \":\" .. tostring(data and data.n) .. \":\" .. tostring(from)\n"
            "  setShared(\"nivel2_recibio\", r)\n  debugLog(\"[capas] nivel2 recibio \" .. r)\nend\n");
    if (!ok) { err = "testlayers: no pude escribir los .lua"; return false; }
    extern void RestartScene();
    RestartScene();
    // EL NIVEL 1 (la raiz de arranque): su cubo, la camara, una luz y sus dos escenas 2D
    W3dRootRename(W3dActiveRoot(), "Nivel 1", NULL);
    CubeWithScript("Cubo1", dir + "/nivel1.lua", 0);
    { Camera* cam = new Camera(NULL, Vector3(0, 1.5f, 8), Vector3(-10.0f, 0, 0)); cam->SetNameObj("Camara1");
      extern Camera* CameraActive; CameraActive = cam; }
    { Light* l = Light::Create(NULL, 3.0f, 4.0f, 5.0f); l->SetNameObj("Luz1"); }
    UI* hud = new UI(NULL, Vector3(0, 0, 0)); hud->SetNameObj("HUD"); hud->sameAsRender = true;
    UI* menu = new UI(NULL, Vector3(0, 0, 0)); menu->SetNameObj("Menu"); menu->sameAsRender = true;
    if (!menu->scriptData) menu->scriptData = new W3dScriptData();
    { W3dScriptEntry e; e.path = dir + "/menu.lua"; menu->scriptData->scripts.push_back(e); }
    W3dSceneSetInitial("HUD");
    // EL NIVEL 2 (otro juego) y EL DECORADO (una escena 3D que se carga encima)
    if (W3dRootCreateAndOpen(W3D_ROOT_GAME, "Nivel 2") < 0) { err = "testlayers: no pude crear 'Nivel 2'"; return false; }
    CubeWithScript("Cubo2", dir + "/nivel2.lua", 0);
    { Camera* cam = new Camera(NULL, Vector3(0, 1.5f, 8), Vector3(-10.0f, 0, 0)); cam->SetNameObj("Camara2");
      extern Camera* CameraActive; CameraActive = cam; }
    { Light* l = Light::Create(NULL, 3.0f, 4.0f, 5.0f); l->SetNameObj("Luz2"); }
    if (W3dRootCreateAndOpen(W3D_ROOT_SCENE, "Decorado") < 0) { err = "testlayers: no pude crear 'Decorado'"; return false; }
    CubeWithScript("CuboDeco", dir + "/deco.lua", 3.0f);
    // de vuelta al Nivel 1, que es con el que arranca
    std::string reason;
    if (!W3dActivateRoot(W3dRootFind(W3D_ROOT_SCENE, "Nivel 1"), &reason)) { err = "pruebacapas: " + reason; return false; }
    W3dRootSetInitial("Nivel 1");
    printf("      [pruebacapas] Nivel 1 (HUD, Menu), Nivel 2 y Decorado en '%s'\n", dir.c_str());
    return true;
}

// ============================================================================
//  LAS ESCENAS ABIERTAS A LA VEZ
// ============================================================================
extern int OutlinerObjectRow(Object* o);   // Outliner.cpp
extern int OutlinerYOfRow(int row);        // Outliner.cpp (con los cortes constructor / cada cuadro)
extern bool g_w3dChangesNoDialog;          // ProjectChanges.cpp: el harness no pregunta (salvo a proposito)
static std::string LineRest(std::istringstream& ss) {
    std::string r;
    std::getline(ss, r);
    const size_t a = r.find_first_not_of(" \t");
    if (a == std::string::npos) return std::string();
    const size_t b = r.find_last_not_of(" \t\r");
    return r.substr(a, b - a + 1);
}
static void SplitCommas(const std::string& t, std::vector<std::string>& out) {
    out.clear();
    size_t i = 0;
    while (i <= t.size()) {
        const size_t c = t.find(',', i);
        const std::string p = t.substr(i, c == std::string::npos ? std::string::npos : c - i);
        if (!p.empty()) out.push_back(p);
        if (c == std::string::npos) break;
        i = c + 1;
    }
}
static std::string OpenText() {
    std::vector<int> ab;
    W3dOpenRoots(ab);
    std::string t;
    for (size_t i = 0; i < ab.size(); i++) { if (i) t += ","; t += W3dRoots()[(size_t)ab[i]].name; }
    return t;
}
static int SceneIdx(const std::string& n, std::string& err, const char* cmd) {
    const int i = W3dRootFind(W3D_ROOT_SCENE, n);
    if (i < 0) err = std::string(cmd) + ": no hay una escena '" + n + "'";
    return i;
}
// el nodo de una escena abierta (por su nombre en el proyecto) o un objeto de cualquiera de las abiertas
static Object* RowObject(const std::string& n) {
    const int i = W3dRootFind(W3D_ROOT_SCENE, n);
    if (i >= 0 && W3dRootIsOpen(i)) return W3dRoots()[(size_t)i].root;
    std::vector<int> ab;
    W3dOpenRoots(ab);
    for (size_t k = 0; k < ab.size(); k++) {
        Object* r = W3dRoots()[(size_t)ab[k]].root;
        Object* o = r ? FindObjectByName(r, n) : NULL;
        if (o) return o;
    }
    return NULL;
}
static Outliner* ElOutlinerScene(std::string& err, const char* cmd) {
    std::vector<ViewportBase*> hs;
    Leaves(rootViewport, hs);
    for (size_t i = 0; i < hs.size(); i++)
        if (VpIsOutliner(hs[i]) && ((Outliner*)hs[i])->view == OUT_VIEW_SCENE) return (Outliner*)hs[i];
    err = std::string(cmd) + ": no hay un outliner en la vista Escena";
    return NULL;
}
static bool CmdSceneCheck(std::istringstream& ss, std::string& err) {
    std::string mode; ss >> mode;
    const std::string n = LineRest(ss);
    const int i = SceneIdx(n, err, "scenecheck"); if (i < 0) return false;
    const bool open = (mode == "open");
    const bool dialog = (mode == "closedialog");
    // (el cartel esta apagado en el producto: aca se prende a proposito para probarlo)
    const bool noPrev = g_w3dChangesNoDialog, activePrev = g_w3dChangesDialogActive;
    if (dialog) { g_w3dChangesNoDialog = false; g_w3dChangesDialogActive = true; }
    std::string reason;
    const bool ok = W3dRootCheck(i, open, &reason);
    g_w3dChangesNoDialog = noPrev; g_w3dChangesDialogActive = activePrev;
    const bool popup = W3dChangesDialogOpen();
    printf("      [escenatildar] %s '%s' -> %s%s%s%s; abiertas: %s\n", mode.c_str(), n.c_str(), ok ? "si" : "NO",
           reason.empty() ? "" : ": ", reason.c_str(), popup ? " (pregunta por los cambios)" : "", OpenText().c_str());
    if (mode == "closefail") { if (ok) { err = "scenecheck: se cerro y se esperaba que no"; return false; } return true; }
    if (!ok) { err = "scenecheck: no se pudo (" + reason + ")"; return false; }
    if (dialog != popup) { err = std::string("escenatildar: ") + (popup ? "asked" : "no pregunto") + " por los cambios"; return false; }
    return true;
}
static bool CmdScenesOpen(std::istringstream& ss, std::string& err) {
    const std::string expected = LineRest(ss), has = OpenText();
    printf("      [escenasabiertas] %s\n", has.c_str());
    if (has != expected) { err = "scenesopen: son '" + has + "', se esperaban '" + expected + "'"; return false; }
    return true;
}
static bool CmdSceneHidden(std::istringstream& ss, std::string& err) {
    std::string expected = LineRest(ss);
    if (expected == "-") expected.clear();
    std::vector<int> ab;
    W3dOpenRoots(ab);
    std::string has;
    for (size_t i = 0; i < ab.size(); i++)
        if (!W3dOpenRootVisible(ab[i])) { if (!has.empty()) has += ","; has += W3dRoots()[(size_t)ab[i]].name; }
    printf("      [escenaocultas] '%s'\n", has.c_str());
    if (has != expected) { err = "scenehidden: son '" + has + "', se esperaban '" + expected + "'"; return false; }
    return true;
}
static bool CmdSceneActive(std::istringstream& ss, std::string& err) {
    const std::string n = LineRest(ss);
    const int act = W3dActiveRoot();
    const std::string has = act >= 0 ? W3dRoots()[(size_t)act].name : std::string("?");
    printf("      [escenaactiva] '%s'\n", has.c_str());
    if (has != n) { err = "sceneactive: la activa es '" + has + "', se esperaba '" + n + "'"; return false; }
    if (W3dRoots()[(size_t)act].root != SceneCollection) { err = "sceneactive: la activa no es SceneCollection"; return false; }
    return true;
}
static bool CmdSceneLoaded(std::istringstream& ss, std::string& err) {
    int v = 1; ss >> v;
    const std::string n = LineRest(ss);
    const int i = SceneIdx(n, err, "sceneloaded"); if (i < 0) return false;
    const bool has = W3dRoots()[(size_t)i].root != NULL;
    printf("      [escenacargada] '%s': %s\n", n.c_str(), has ? "en memoria" : "sin cargar");
    if (has != (v != 0)) { err = "sceneloaded: '" + n + "' " + (has ? "is" : "no esta") + " en memoria"; return false; }
    return true;
}
static bool CmdSceneEye(std::istringstream& ss, std::string& err) {
    int v = 1; ss >> v;
    const std::string n = LineRest(ss);
    const int i = SceneIdx(n, err, "sceneeye"); if (i < 0) return false;
    W3dRootSetOpenVisible(i, v != 0);
    return true;
}
static bool CmdSceneMove(std::istringstream& ss, std::string& err) {
    int pos = 0; ss >> pos;
    const std::string n = LineRest(ss);
    const int i = SceneIdx(n, err, "scenemove"); if (i < 0) return false;
    if (!W3dRootMoveOpen(i, pos)) { err = "scenemove: '" + n + "' no esta abierta"; return false; }
    printf("      [escenamover] %s\n", OpenText().c_str());
    return true;
}
static bool CmdSceneRow(std::istringstream& ss, std::string& err) {
    int f = 0; ss >> f;
    const std::string n = LineRest(ss);
    Object* o = RowObject(n);
    if (!o) { err = "scenerow: no hay una escena abierta ni un objeto '" + n + "'"; return false; }
    const int has = OutlinerObjectRow(o);
    printf("      [escenafila] '%s': fila %d\n", n.c_str(), has);
    if (has != f) { err = "scenerow: '" + n + "' esta en la fila " + IsInteger(has) + ", se esperaba la " + IsInteger(f); return false; }
    return true;
}
// el punto de una fila del outliner: en su NOMBRE, su OJO o su FLECHITA
static bool RowPoint(Outliner* ol, Object* o, const std::string& where, int& mx, int& my) {
    const int row = OutlinerObjectRow(o);
    if (row < 0) return false;
    int prof = 0;
    for (Object* p = o->Parent; p; p = p->Parent) prof++;   // (el nodo de una escena es la fila 0 de su arbol)
    std::vector<int> ab;
    W3dOpenRoots(ab);
    if (ab.size() < 2) prof--;   // con una sola abierta no hay nodo: lo de arriba es su primer nivel
    my = ol->y + borderGS + ol->PosY + ol->BarTopOffset() + OutlinerYOfRow(row) + (int)RowHeightGS / 2;
    const int reserveBar = ol->scrollY ? (GlobalScale * 9 + gapGS) : 0;
    if (where == "eye") mx = ol->x + ol->width - 2 * IconSizeGS - gapGS - marginGS - borderGS - reserveBar + IconSizeGS / 2;
    else if (where == "arrow") mx = ol->x + marginGS + ol->PosX + prof * (IconSizeGS + gapGS) + IconSizeGS / 2;
    else mx = ol->x + marginGS + ol->PosX + (prof + 2) * (IconSizeGS + gapGS) + LetterWidthGS;
    return true;
}
static bool CmdSceneClick(std::istringstream& ss, std::string& err) {
    Outliner* ol = ElOutlinerScene(err, "sceneclick"); if (!ol) return false;
    std::string where; ss >> where;
    const std::string n = LineRest(ss);
    Object* o = RowObject(n);
    int mx = 0, my = 0;
    if (!o || !RowPoint(ol, o, where, mx, my)) { err = "sceneclick: no hay una fila '" + n + "'"; return false; }
    ol->ClickSelect(mx, my);
    ol->ReleaseDrag(mx, my);
    rootViewport->Render();
    printf("      [escenaclick] %s '%s' -> activa '%s', objeto activo '%s'\n", where.c_str(), n.c_str(),
           W3dActiveRoot() >= 0 ? W3dRoots()[(size_t)W3dActiveRoot()].name.c_str() : "?",
           ActiveObj ? ActiveObj->name.c_str() : "-");
    return true;
}
static bool CmdSceneKey(std::istringstream& ss, std::string& err) {
    Outliner* ol = ElOutlinerScene(err, "scenekey"); if (!ol) return false;
    std::string t; ss >> t;
    if (t != "h") { err = "scenekey: uso: scenekey h"; return false; }
    ol->event_key_down(W3dK_H, false);
    return true;
}
static bool CmdSceneDrag(std::istringstream& ss, std::string& err) {
    Outliner* ol = ElOutlinerScene(err, "scenedrag"); if (!ol) return false;
    std::string where; ss >> where;
    std::vector<std::string> ns;
    SplitCommas(LineRest(ss), ns);
    if (ns.size() != 2) { err = "scenedrag: uso: scenedrag antes|despues <A>,<B>"; return false; }
    Object* a = RowObject(ns[0]);
    Object* b = RowObject(ns[1]);
    int ax = 0, ay = 0, bx = 0, by = 0;
    if (!a || !b || !RowPoint(ol, a, "name", ax, ay) || !RowPoint(ol, b, "name", bx, by)) {
        err = "scenedrag: no estan las dos filas"; return false;
    }
    by += (where == "before" ? -1 : 1) * ((int)RowHeightGS / 2 - 1);   // (el borde de arriba o el de abajo de la fila)
    ol->ClickSelect(ax, ay);
    if (ol->dragObject != a) { err = "scenedrag: el click no agarro la fila de '" + ns[0] + "'"; return false; }
    ol->dragging = true;
    ol->UpdateDrop(bx, by);
    ol->ReleaseDrag(bx, by);
    printf("      [escenaarrastrar] %s: %s\n", where.c_str(), OpenText().c_str());
    return true;
}

// ============================================================================
//  el despachador
// ============================================================================
bool W3dTestsScenesCmd(const std::string& cmd, std::istringstream& ss, std::string& err, bool& handled) {
    handled = true;
    if (cmd == "rootnew")       return CmdNewRoot(ss, err);
    if (cmd == "testlayers")     return CmdTestLayers(ss, err);
    if (cmd == "scenecheck")    return CmdSceneCheck(ss, err);
    if (cmd == "scenesopen") return CmdScenesOpen(ss, err);
    if (cmd == "scenehidden")   return CmdSceneHidden(ss, err);
    if (cmd == "sceneactive")    return CmdSceneActive(ss, err);
    if (cmd == "sceneeye")       return CmdSceneEye(ss, err);
    if (cmd == "sceneloaded")   return CmdSceneLoaded(ss, err);
    if (cmd == "scenemove")     return CmdSceneMove(ss, err);
    if (cmd == "scenerow")      return CmdSceneRow(ss, err);
    if (cmd == "sceneclick")     return CmdSceneClick(ss, err);
    if (cmd == "scenedrag") return CmdSceneDrag(ss, err);
    if (cmd == "scenekey")     return CmdSceneKey(ss, err);
    if (cmd == "rootopen")       return CmdRootOpen(ss, err);
    if (cmd == "rootinfo")        return CmdRootInfo(ss, err);
    if (cmd == "rootcontext")    return CmdRootContext(ss, err);
    if (cmd == "rootcamera")      return CmdRootCamera(ss, err);
    if (cmd == "rootlights")       return CmdRootLights(ss, err);
    if (cmd == "rootparent")      return CmdRootParent(ss, err);
    if (cmd == "rootscript")      return CmdRootScript(ss, err);
    if (cmd == "rootinitial")     return CmdRootInitial(ss, err);
    if (cmd == "rootmenu")        return CmdRootMenu(ss, err);
    if (cmd == "rootbutton")       return CmdRootButton(ss, err);
    if (cmd == "rootbar")       return CmdRootBar(ss, err);
    if (cmd == "screensnapshot")    return CmdScreenSnapshot(ss, err);
    if (cmd == "rootdelete")      return CmdRootDelete(ss, err);
    if (cmd == "rootrename")   return CmdRootRename(ss, err);
    if (cmd == "entryexists")   return CmdEntryExists(ss, err);
    if (cmd == "entryhas")    return CmdEntryHas(ss, err);
    if (cmd == "roottype")        return CmdRootType(ss, err);
    if (cmd == "rootmode")        return CmdRootMode(ss, err);
    if (cmd == "rootanim")        return CmdRootAnim(ss, err);
    if (cmd == "rootclip")        return CmdRootClip(ss, err);
    if (cmd == "rootkey")         return CmdRootKey(ss, err);
    if (cmd == "rootdope")        return CmdRootDope(ss, err);
    if (cmd == "rootscrub")       return CmdRootScrub(ss, err);
    if (cmd == "notif")           return CmdNotif(ss, err);
    if (cmd == "keydiamond")        return CmdKeyDiamond(ss, err);
    if (cmd == "menuobject")      return CmdMenuObject(ss, err);
    if (cmd == "cinematicinfo")        return CmdCinematicInfo(ss, err);
    if (cmd == "rootobjat")       return CmdRootObjAt(ss, err);
    if (cmd == "objvis")          return CmdObjVis(ss, err);
    if (cmd == "animnew")       return CmdNewAnim(ss, err);
    if (cmd == "clipnew")       return CmdNewClip(ss, err);
    if (cmd == "animcurve")       return CmdAnimCurve(ss, err);
    if (cmd == "clipinfo")        return CmdClipInfo(ss, err);
    if (cmd == "clipmenu")        return CmdClipMenu(ss, err);
    if (cmd == "animactive")      return CmdAnimActive(ss, err);
    if (cmd == "clipname")      return CmdClipName(ss, err);
    if (cmd == "hierlib")          return CmdHierLib(ss, err);
    if (cmd == "hierinfo")         return CmdHierInfo(ss, err);
    if (cmd == "hierpath")         return CmdHierPath(ss, err);
    if (cmd == "hiernode")         return CmdHierNode(ss, err);
    if (cmd == "hierplay")         return CmdHierPlay(ss, err);
    if (cmd == "hiertick")         return CmdHierTick(ss, err);
    if (cmd == "hiervel")          return CmdHierVel(ss, err);
    if (cmd == "hierretarget")     return CmdHierRetarget(ss, err);
    if (cmd == "hierframe")        return CmdHierFrame(ss, err);
    if (cmd == "hierclipretarget") return CmdHierClipRetarget(ss, err);
    if (cmd == "hierreset")        { W3dAnimObjectsReset(); printf("      [jerreset]\n"); return true; }
    if (cmd == "hiermem")          return CmdHierMem(ss, err);
    if (cmd == "rotat")           return CmdRotAt(ss, err);
    if (cmd == "animrange")       return CmdAnimRange(ss, err);
    if (cmd == "rigretarget")     return CmdRigRetarget(ss, err);
    if (cmd == "hierlibmenu")      return CmdHierLibMenu(ss, err);
    if (cmd == "objretarget")     return CmdObjRetarget(ss, err);
    if (cmd == "clipretargetmenu") return CmdClipRetargetMenu(ss, err);
    if (cmd == "animminus")       return CmdAnimMinus(ss, err);
    if (cmd == "animlist")       return CmdAnimList(ss, err);
    if (cmd == "hierlayer")         return CmdHierLayer(ss, err);
    if (cmd == "hiermix")          return CmdHierMix(ss, err);
    if (cmd == "mixhier")          return CmdMixHier(ss, err);
    if (cmd == "mixrows")        return CmdMixRows(ss, err);
    if (cmd == "riglayer")         return CmdRigLayer(ss, err);
    if (cmd == "rigbiped")        return CmdRigBiped(ss, err);
    if (cmd == "rigskinglb")      return CmdRigSkinGlb(ss, err);
    if (cmd == "importclipsheight") return CmdImportClipsHeight(ss, err);
    if (cmd == "checkbonepose")       return CmdBonePose(ss, err);
    if (cmd == "gamecinematicmin")    return CmdGameCinematicMin(ss, err);
    if (cmd == "gamecinematiclog")    return CmdGameCinematicLog(ss, err);
    if (cmd == "gamescenesmin") return CmdGameScenesMin(ss, err);
    if (cmd == "gamesceneslog") return CmdGameScenesLog(ss, err);
    handled = false;
    return false;
}
