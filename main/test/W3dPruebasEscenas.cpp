// ============================================================================
//  W3dPruebasEscenas.cpp — comandos de harness de las ESCENAS 3D, los JUEGOS y los
//  PREFABS (las raices del proyecto: W3dRaices.h, io/RaicesEditor.h). Ver W3dPruebasEscenas.h.
//
//  Comandos:
//    raiznueva escena|juego|prefab [nombre]
//        lo que hacen "New Scene" / "New Game" / "New Prefab": una raiz vacia (el prefab con
//        su objeto raiz) con nombre libre, y la ABRE.
//    raizabrir <nombre> [prefab] [falla]
//        abre esa escena/juego (o prefab) en el editor (W3dActivarRaiz, con sus precondiciones).
//        'falla' asserta que NO se puede (y lo que dice).
//    raizinfo [n N] [escenas N] [juegos N] [prefabs N] [activa X] [tipo escena|juego|prefab]
//             [bloque X] [cargada X 0|1] [inicial X|-] [objetos N] [entrada X <entrada|->]
//             [tipode X escena|juego|prefab]
//        el registro de raices y sus asserts ('objetos' = los de la ACTIVA, recursivo; los
//        tipos son los EFECTIVOS: el de un proyecto viejo sale de sus scripts).
//    raiztipo escena|juego [falla]   "Convert to Scene" / "Convert to Game" de la activa (con undo)
//    raizmodo [kind N] [juego 0|1] [autokey 0|1] [menujuego 0|1]
//        el MODO del timeline: ActiveAnimKind, AnimEsJuego, si el boton Auto Key esta visible y
//        si el selector de animacion ofrece "Juego"
//    raizanim juego|<animacion de escena>   elegir en el selector de animacion (AnimSelPorId):
//        "Juego" (el estado base) o uno de sus CLIPS de escena
//    raizclip <nombre> <objeto> <f0> <x0> <f1> <x1>
//        una animacion de escena NUEVA (no activa) que mueve el objeto en X de x0 (frame f0) a x1
//    raizkey [n N]        Insert Keyframe (la I) sobre lo seleccionado; asserta cuantos keyframes de
//                         escena le agrego al activo (en "Juego" ninguno)
//    raizdope <n>         cuantas filas arma el dope sheet del timeline (0 = sin keyframes)
//    raizscrub <frame> [cur N]   arrastrar el cabezal del timeline a ese frame (SetFrameFromX): en
//                         "Juego" recorre los ticks CACHEADOS (acotado) o, sin partida, se queda en el 1
//    rendercache <carpeta> [rango F0 F1] [n N | sin] [ini N] [fin N]
//        Render Animation con el timeline en "Juego": renderiza el CACHE de la simulacion. 'rango'
//        lo fija a mano (se acota al cache); 'n' asserta cuantos frames guardo, 'sin' que no habia
//        cache (no guardo nada), 'ini'/'fin' el rango que quedo. Render chico (64x48, PNG). Siempre asserta
//        la tarjeta Render en "Juego" (sin cache: Render Animation GRIS y el aviso; con cache: habilitado y
//        su rango en la nota) y que cada PNG sea el de su frame (cache_NNNN.png, uno por frame del rango).
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
//  LAS CINEMATICAS DE UN JUEGO -- los DOS caminos (W3dRaices.h / animation/Animation.h):
//    cineinfo [activa 0|1] [escena X] [juego X]
//        la cinematica de reproducirEscena(): si hay una sonando, que escena y de que juego
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
//    jerruta <raiz> <objeto> <ruta|->        W3dJerRuta: la ruta de un nodo relativa a la raiz
//    jernodo <raiz> <ruta> <objeto|->        W3dJerNodo: el nodo que nombra una ruta
//    jerplay <raiz> <clip> [loop 0|1] [dur D]  animObjeto sin lua (asserta la duracion o -1 si no esta)
//    jertick <dt> [n]                        n ticks del reproductor de clips (W3dAnimObjetosTick)
//    jervel <raiz> <v> / jerretarget <objeto> completo|rotaciones|clip / jerreset
//    jerclipretarget <raiz> <clip> completo|rotaciones   el retarget por DEFECTO de un clip
//    jerframe <raiz> <frame|-> [tol] [termino 0|1] [clip X]   el cabezal de la raiz ('-' = no suena nada)
//    jermem [clips N] [refs N] [sonando N]   los clips de jerarquia en memoria (W3dAnimSetsEstadisticas)
//    rotat <objeto> <rx> <ry> <rz> <tol>     la rotacion (euler, grados) de un objeto de la raiz activa
//    animrango <f0> <f1>                     Start/End de la animacion elegida (la de una vista va a su clip)
//    rigretarget <prefijo> <escala>          dos armatures "<prefijo>A" y "<prefijo>B" (B con los huesos a
//        'escala'), con un clip compartido que mueve la raiz y rota/traslada los hijos (para el retarget)
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
//        la capa n (base 1) de clips de jerarquia de la raiz (objetoCapa sin lua); '-' la saca con las de arriba
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
//        (animObjeto) y dos capas de escena (escenaCapa) que a los 5 ticks reproduce la ESCENA
//        "Cine1" (reproducirEscena con alTerminar); su juez verifica que el juego volvio EXACTAMENTE
//        como estaba, que la cinematica sono entera y que sus animaciones siguieron despues. Ademas
//        tres raices SIN biblioteca reproducen clips ajenos por su nombre calificado: la Grua
//        ("Puerta/abrir" con animObjetoVelocidad 2), la Puerta2 (el doble de grande, animRetarget
//        "rotaciones": sube el doble) y la Plataforma (objetoCapa: "Ascensor/subir" mezclado al 50%). El
//        mismo veredicto en el Play y en el juego compilado (que ademas carga la escena de su
//        entrada para la cinematica y la libera al volver: se mira su whisk3d.log).
//    notif tiene <texto> | notif no <texto> | notif limpiar
//        las notificaciones que se ven: alguna contiene el texto (en ingles, la clave de T(): tambien se busca
//        traducido, como sale en pantalla) / ninguna
//    keyrombo <prop> <comp> [estado N]   el CLICK en el rombo de keyframe de Properties (el hook que cablea el
//        panel, sobre el objeto activo en el frame actual) y el estado que muestra despues
//    menuobjeto <id>               una opcion del menu Object por su id (511 Delete Keyframe, 512 Clear Keyframe...)
//    juegoescenasmin <carpeta> / juegoescenaslog <carpeta> <nombre>
//        el juego 3D minimo con TRES raices: el JUEGO del bloque pasa con cambiarEscena() a la ESCENA
//        "Intro" (una cinematica: su animacion arranca sola y su script, al verla terminar, pide el
//        nivel) y de ahi al JUEGO "Nivel2", cuyo juez verifica que las anteriores se DESCARGARON, que
//        la nueva CARGO desde su entrada, que la cinematica se reprodujo entera y que compartido()
//        sobrevivio. El mismo veredicto en el Play y en el juego compilado.
// ============================================================================
#include "test/W3dPruebasEscenas.h"
#include "test/W3dScript.h"            // W3dRunCommand("juego3dmin ...")
#include "W3dRaices.h"
#include "io/RaicesEditor.h"
#include "io/RecursosProyecto.h"
#include "objects/Objects.h"
#include "objects/Scene.h"
#include "objects/Empty.h"
#include "objects/Camera.h"
#include "objects/Light.h"
#include "objects/Mesh.h"
#include "script/W3dScript.h"
#include "animation/Animation.h"
#include "animation/W3dAnimSet.h"         // los clips de jerarquia: sus bibliotecas, rutas y reproductor
#include "io/W3dRecursos.h"
#include "animation/SkeletalAnimation.h" // el retarget de los armatures
#include "objects/Armature.h"
#include "ViewPorts/ViewPorts.h"
#include "ViewPorts/ViewPort3D.h"
#include "ViewPorts/Outliner.h"
#include "ViewPorts/LayoutInput.h"
#include "ViewPorts/Timeline.h"         // el dope sheet: en "Juego" no hay filas (sin keyframes)
#include "ViewPorts/Properties.h"       // la tarjeta Render: el render del CACHE de un juego
#include "script/SimJuego.h"
#include "WhiskUI/widgets/Button.h"
#include "WhiskUI/widgets/PopupMenu.h"
#include "WhiskUI/draw/icons.h"          // IconoNombre: los iconos de las raices (camera / gamepad / prefab)
#include "config/W3dLang.h"
#include "io/W3dZip.h"
#include "w3dFilesystem.h"
#include "gfx/w3dGraphics.h"              // pantallafoto: w3dEngine::ReadPixelsRGBA
#include "WhiskUI/Propieties/PropList.h"   // las filas del arbol del Mix (MixFilasCount / MixFilaTexto)
#include "WhiskUI/Propieties/PropertieBase.h"   // los hooks del rombo de keyframe (W3dKeyframeToggle / Estado)
#include "ViewPorts/Notificaciones.h"   // lo que se le dijo al usuario (notif)
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <string>
#include <algorithm>

extern ViewportBase* rootViewport;
// (Properties.cpp) el selector de animacion y el render del cache de un juego
extern void AnimSelPorId(int id);
extern void AnimSelJuego();
extern bool AutoKeyOn;
extern void W3dHarnessRenderAnimation();
extern bool W3dRenderJuegoCache(int* mn, int* mx);
extern void W3dRenderJuegoFijar(float ini, float fin);
extern void W3dRenderJuegoAuto();
extern float g_renderJuegoIni, g_renderJuegoFin;
extern void ConstruirMenuAnim(PopupMenu* menu);
// (Properties.cpp) los desplegables de la tarjeta Animacion por su texto (el mismo camino que el click)
extern bool PropsJerLibElegir(const std::string& texto);
extern bool PropsObjRetargetElegir(const std::string& texto);
extern bool PropsClipRetargetElegir(const std::string& texto);
extern bool PropsMixAgregarMenu(const std::string& sub, const std::string& item);
extern void _AnimDelCardFwd();   // el "-" de la tarjeta Animation
extern bool ExportGLTF(const std::string&, bool, bool);
extern bool ImportGLTF(const std::string&);
extern void WeightPaintAsegurarMapa(Mesh* m);

// ---------------------------------------------------------------------------
//  helpers
// ---------------------------------------------------------------------------
static std::string EsEntero(long n) { char b[32]; sprintf(b, "%ld", n); return std::string(b); }
static int TipoDe(const std::string& s) { return W3dRaizTipoDeClave(s, W3D_RAIZ_ESCENA); }
static const char* NombreTipo(int t) { return W3dRaizTipoClave(t); }
static std::string NomObj(const Object* o) { return o ? o->name : std::string("-"); }

static void Hojas(ViewportBase* n, std::vector<ViewportBase*>& out) {
    if (!n) return;
    if (n->isLeaf()) { out.push_back(n); return; }
    if (n->ContainerKind() == 1) { Hojas(((ViewportRow*)n)->childA, out); Hojas(((ViewportRow*)n)->childB, out); }
    else { Hojas(((ViewportColumn*)n)->childA, out); Hojas(((ViewportColumn*)n)->childB, out); }
}
static ViewportBase* PrimeraHoja(int kind) {
    std::vector<ViewportBase*> hs;
    Hojas(rootViewport, hs);
    for (size_t i = 0; i < hs.size(); i++) if (hs[i]->ViewportKind() == kind) return hs[i];
    return NULL;
}
// el boton del selector de la raiz en la barra de ese panel (3D: el de rol BR_Raiz; outliner: btnRaiz)
static Button* BotonSelector(ViewportBase* v, bool outliner) {
    if (!v) return NULL;
    if (outliner) return ((Outliner*)v)->btnRaiz;
    return BarRolBtn(v->BarButtons, BR_Raiz);
}
// CLICK real en un boton de la barra: se trae a la vista (la barra scrollea si no entra) y se clickea
static bool ClickBoton(ViewportBase* v, Button* b) {
    if (!v || !b || !b->visible) return false;
    // si no entra en el ancho del panel, la barra se scrollea (como con la rueda) hasta que se vea entero
    v->barScrollManual = 0;
    v->ActualizarBarra();
    const int sobra = (b->sx + b->width) - (v->x + v->width) + 4;
    if (sobra > 0) { v->barScrollManual = sobra; v->ActualizarBarra(); }
    const int bx = b->sx + b->width / 2, by = b->sy + b->height / 2;
    LayoutClickUI(bx, by);
    LayoutSoltar(bx, by);
    return true;
}
static int ObjetosRec(Object* o) {
    if (!o) return 0;
    int n = 0;
    for (size_t i = 0; i < o->Childrens.size(); i++) n += 1 + ObjetosRec(o->Childrens[i]);
    return n;
}
static bool LeerEntrada(const std::string& zip, const std::string& e, std::string* datos) {
    std::vector<W3dZipEntrada> za;
    if (!W3dZipLeer(zip, &za)) return false;
    for (size_t i = 0; i < za.size(); i++)
        if (za[i].nombre == e) {
            if (datos) datos->assign(za[i].datos.begin(), za[i].datos.end());
            return true;
        }
    return false;
}

// ---------------------------------------------------------------------------
//  los comandos
// ---------------------------------------------------------------------------
static bool CmdRaizNueva(std::istringstream& ss, std::string& err) {
    std::string t, n; ss >> t >> n;
    if (t != "escena" && t != "juego" && t != "prefab") { err = "raiznueva: uso: raiznueva escena|juego|prefab [nombre]"; return false; }
    const int i = W3dRaizCrearYAbrir(TipoDe(t), n);
    if (i < 0) { err = "raiznueva: se creo pero no se pudo abrir"; return false; }
    printf("      [raiznueva] %s '%s' (fila %d) abierta\n", t.c_str(), W3dRaices()[(size_t)i].nombre.c_str(), i);
    return true;
}

static bool CmdRaizAbrir(std::istringstream& ss, std::string& err) {
    std::string n, k; ss >> n;
    int tipo = W3D_RAIZ_ESCENA; bool falla = false;
    while (ss >> k) { if (k == "prefab") tipo = W3D_RAIZ_PREFAB; else if (k == "falla") falla = true; }
    const int i = W3dRaizBuscar(tipo, n);
    if (i < 0) { err = "raizabrir: no hay un " + std::string(NombreTipo(tipo)) + " '" + n + "'"; return false; }
    std::string motivo;
    const bool ok = W3dActivarRaiz(i, &motivo);
    printf("      [raizabrir] %s '%s' -> %s%s%s\n", NombreTipo(tipo), n.c_str(), ok ? "abierta" : "NO",
           motivo.empty() ? "" : ": ", motivo.c_str());
    if (falla == ok) { err = falla ? "raizabrir: se abrio y se esperaba que no" : "raizabrir: no se pudo abrir (" + motivo + ")"; return false; }
    return true;
}

static bool CmdRaizInfo(std::istringstream& ss, std::string& err) {
    const std::vector<W3dRaizFila>& fs = W3dRaices();
    const int act = W3dRaizActiva();
    int ne = 0, nj = 0, np = 0;
    for (size_t i = 0; i < fs.size(); i++) {
        const int t = W3dRaizTipoDe((int)i);
        if (t == W3D_RAIZ_PREFAB) np++; else if (t == W3D_RAIZ_JUEGO) nj++; else ne++;
        printf("      [raizinfo] %c %-7s '%s' entrada='%s' carpeta='%s' %s%s%s%s\n", (int)i == act ? '*' : ' ',
               NombreTipo(t), fs[i].nombre.c_str(), fs[i].entrada.c_str(), fs[i].carpeta.c_str(),
               fs[i].raiz ? "cargada" : "sin cargar", fs[i].bloque ? " (bloque)" : "",
               ((int)i == W3dRaizInicialIdx() && W3dRaizEs3D(t)) ? " (inicial)" : "",
               fs[i].tipoExplicito ? "" : " (tipo deducido)");
    }
    std::string k;
    while (ss >> k) {
        std::string v; if (!(ss >> v)) { err = "raizinfo: falta el valor de '" + k + "'"; return false; }
        if (k == "n") { if ((int)fs.size() != atoi(v.c_str())) { err = "raizinfo: hay " + EsEntero((long)fs.size()) + " raices, se esperaban " + v; return false; } }
        else if (k == "escenas") { if (ne != atoi(v.c_str())) { err = "raizinfo: hay " + EsEntero(ne) + " escenas, se esperaban " + v; return false; } }
        else if (k == "juegos") { if (nj != atoi(v.c_str())) { err = "raizinfo: hay " + EsEntero(nj) + " juegos, se esperaban " + v; return false; } }
        else if (k == "prefabs") { if (np != atoi(v.c_str())) { err = "raizinfo: hay " + EsEntero(np) + " prefabs, se esperaban " + v; return false; } }
        else if (k == "activa") {
            if (act < 0 || fs[(size_t)act].nombre != v) { err = "raizinfo: la activa es '" + (act >= 0 ? fs[(size_t)act].nombre : std::string("?")) + "', se esperaba '" + v + "'"; return false; }
            if (fs[(size_t)act].raiz != SceneCollection) { err = "raizinfo: la activa no es SceneCollection"; return false; }
        }
        else if (k == "tipo") { if (act < 0 || W3dRaizTipoDe(act) != TipoDe(v)) { err = "raizinfo: la activa no es un(a) " + v; return false; } }
        else if (k == "bloque") { const int b = W3dRaizBloque(); if (b < 0 || fs[(size_t)b].nombre != v) { err = "raizinfo: la del bloque no es '" + v + "'"; return false; } }
        else if (k == "inicial") {
            const std::string i = W3dRaizInicial();
            if (i != (v == "-" ? std::string() : v)) { err = "raizinfo: la inicial es '" + i + "', se esperaba '" + v + "'"; return false; }
        }
        else if (k == "objetos") { const int n = ObjetosRec(SceneCollection); if (n != atoi(v.c_str())) { err = "raizinfo: la activa tiene " + EsEntero(n) + " objetos, se esperaban " + v; return false; } }
        else if (k == "cargada" || k == "entrada" || k == "tipode") {
            std::string x; if (!(ss >> x)) { err = "raizinfo: falta el valor de '" + k + " " + v + "'"; return false; }
            int i = -1;
            for (size_t f = 0; f < fs.size() && i < 0; f++) if (fs[f].nombre == v) i = (int)f;
            if (i < 0) { err = "raizinfo: no hay una raiz '" + v + "'"; return false; }
            if (k == "cargada" && (fs[(size_t)i].raiz != NULL) != (x == "1")) { err = "raizinfo: '" + v + "' " + (fs[(size_t)i].raiz ? "esta" : "no esta") + " cargada"; return false; }
            if (k == "entrada" && fs[(size_t)i].entrada != (x == "-" ? std::string() : x)) { err = "raizinfo: la entrada de '" + v + "' es '" + fs[(size_t)i].entrada + "'"; return false; }
            if (k == "tipode" && W3dRaizTipoDe(i) != TipoDe(x)) { err = "raizinfo: '" + v + "' es " + NombreTipo(W3dRaizTipoDe(i)) + ", se esperaba " + x; return false; }
        }
        else { err = "raizinfo: no entiendo '" + k + "'"; return false; }
    }
    return true;
}

static bool CmdRaizContexto(std::istringstream& ss, std::string& err) {
    printf("      [raizcontexto] activo=%s sel=%d camara=%s luces=%d coleccion=%s anims=%d\n", NomObj(ObjActivo).c_str(),
           (int)ObjSelects.size(), NomObj((Object*)CameraActive).c_str(), (int)Lights.size(),
           CollectionActive == SceneCollection ? "(raiz)" : NomObj(CollectionActive).c_str(), (int)SceneAnimations.size());
    std::string k;
    while (ss >> k) {
        std::string v; if (!(ss >> v)) { err = "raizcontexto: falta el valor de '" + k + "'"; return false; }
        if (k == "activo") { if (NomObj(ObjActivo) != v) { err = "raizcontexto: el activo es '" + NomObj(ObjActivo) + "'"; return false; } }
        else if (k == "sel") { if ((int)ObjSelects.size() != atoi(v.c_str())) { err = "raizcontexto: hay " + EsEntero((long)ObjSelects.size()) + " seleccionados"; return false; } }
        else if (k == "camara") { if (NomObj((Object*)CameraActive) != v) { err = "raizcontexto: la camara activa es '" + NomObj((Object*)CameraActive) + "'"; return false; } }
        else if (k == "luces") { if ((int)Lights.size() != atoi(v.c_str())) { err = "raizcontexto: hay " + EsEntero((long)Lights.size()) + " luces"; return false; } }
        else if (k == "coleccion") {
            const std::string c = CollectionActive == SceneCollection ? std::string("-") : NomObj(CollectionActive);
            if (c != v) { err = "raizcontexto: la coleccion activa es '" + c + "'"; return false; }
        }
        else if (k == "anims") { if ((int)SceneAnimations.size() != atoi(v.c_str())) { err = "raizcontexto: hay " + EsEntero((long)SceneAnimations.size()) + " animaciones de escena"; return false; } }
        else { err = "raizcontexto: no entiendo '" + k + "'"; return false; }
    }
    // la seleccion NUNCA nombra objetos de otra raiz
    for (size_t i = 0; i < ObjSelects.size(); i++)
        if (W3dRaizDe(ObjSelects[i]) != SceneCollection) { err = "raizcontexto: '" + ObjSelects[i]->name + "' esta seleccionado y no es de la raiz activa"; return false; }
    if (ObjActivo && W3dRaizDe(ObjActivo) != SceneCollection) { err = "raizcontexto: el activo no es de la raiz activa"; return false; }
    if (CameraActive && W3dRaizDe((Object*)CameraActive) != SceneCollection) { err = "raizcontexto: la camara activa no es de la raiz activa"; return false; }
    for (size_t i = 0; i < Lights.size(); i++)
        if (W3dRaizDe((Object*)Lights[i]) != SceneCollection) { err = "raizcontexto: una luz registrada no es de la raiz activa"; return false; }
    return true;
}

static bool CmdRaizCamara(std::istringstream& ss, std::string& err) {
    std::string n; float x = 0, y = 0, z = 8; ss >> n >> x >> y >> z;
    if (n.empty()) { err = "raizcamara: uso: raizcamara <nombre> [x y z]"; return false; }
    Camera* c = new Camera(NULL, Vector3(x, y, z), Vector3(0, 0, 0));
    c->SetNameObj(n);
    printf("      [raizcamara] '%s' (camara activa '%s')\n", c->name.c_str(), NomObj((Object*)CameraActive).c_str());
    return true;
}

static bool CmdRaizLuces(std::istringstream& ss, std::string& err) {
    int n = 0; std::string k; int esperadas = -1;
    ss >> n;
    if (ss >> k) { if (k == "creadas") ss >> esperadas; }
    int creadas = 0;
    for (int i = 0; i < n; i++) if (Light::Create(NULL, (float)i, 2, 2)) creadas++;
    printf("      [raizluces] %d de %d creadas (luces de la raiz activa: %d)\n", creadas, n, (int)Lights.size());
    if (esperadas >= 0 && creadas != esperadas) { err = "raizluces: se crearon " + EsEntero(creadas) + ", se esperaban " + EsEntero(esperadas); return false; }
    return true;
}

static bool CmdRaizParent(std::istringstream& ss, std::string& err) {
    std::string n; ss >> n;
    Object* o = SceneCollection ? FindObjectByName(SceneCollection, n) : NULL;
    if (!o) { err = "raizparent: no hay un objeto '" + n + "'"; return false; }
    printf("      [raizparent] '%s' Parent=%s\n", n.c_str(), o->Parent == SceneCollection ? "la raiz" : NomObj(o->Parent).c_str());
    if (o->Parent != SceneCollection) { err = "raizparent: '" + n + "' no tiene a la raiz de Parent"; return false; }
    return true;
}

static bool CmdRaizScript(std::istringstream& ss, std::string& err) {
    std::string n, ruta; ss >> n >> ruta;
    Object* o = SceneCollection ? FindObjectByName(SceneCollection, n) : NULL;
    if (!o || ruta.empty()) { err = "raizscript: uso: raizscript <objeto> <ruta.lua> [prop valor]..."; return false; }
    if (!o->scriptDatos) o->scriptDatos = new W3dScriptDatos();
    W3dScriptEntrada e; e.ruta = ruta;
    std::string p, v;
    while (ss >> p >> v) e.refs.push_back(std::make_pair(p, v));
    o->scriptDatos->scripts.push_back(e);
    printf("      [raizscript] '%s' <- %s (%d ref(s))\n", n.c_str(), ruta.c_str(), (int)e.refs.size());
    return true;
}

static bool CmdRaizInicial(std::istringstream& ss, std::string& err) {
    std::string n; ss >> n;
    if (!W3dRaizFijarInicial(n == "-" ? std::string() : n)) { err = "raizinicial: no hay una escena '" + n + "'"; return false; }
    printf("      [raizinicial] '%s'\n", W3dRaizInicial().c_str());
    return true;
}

static bool CmdRaizMenu(std::istringstream& ss, std::string& err) {
    std::string donde, resto; ss >> donde; std::getline(ss, resto);
    size_t a = resto.find_first_not_of(' ');
    resto = (a == std::string::npos) ? std::string() : resto.substr(a);
    const bool out = (donde == "outliner");
    ViewportBase* v = PrimeraHoja(out ? 2 : 1);
    Button* b = BotonSelector(v, out);
    if (!b) { err = "raizmenu: no hay un selector de escena en la barra del " + donde; return false; }
    if (MenuAbierto) MenuAbierto->Cerrar();
    ClickBoton(v, b);
    if (!MenuAbierto || !MenuAbierto->abierto) { err = "raizmenu: el click no abrio el selector"; return false; }
    PopupMenu* m = MenuAbierto;
    MenuItem* it = NULL;
    std::string hay;
    for (size_t i = 0; i < m->items.size(); i++) {
        hay += " [" + m->items[i]->text + (m->items[i]->verde ? "*" : "") + "]";
        if (!it && (m->items[i]->text == resto || m->items[i]->text == T(resto.c_str()))) it = m->items[i];
    }
    printf("      [raizmenu] %s:%s\n", donde.c_str(), hay.c_str());
    if (!it) { m->Cerrar(); err = "raizmenu: el selector no ofrece '" + resto + "'"; return false; }
    m->Cerrar();
    m->Ejecutar(it->id);
    if (rootViewport) rootViewport->Render();   // un cuadro: las barras se sincronizan
    return true;
}

// raizboton escena|juego|prefab: el "+" de la barra del outliner es UN boton desplegable (como el Add
// del 3D): un click real lo abre y se elige "New Scene" / "New Game" / "New Prefab". Comprueba que el
// menu ofrece los TRES (con el icono de lo que crean) y nada mas.
static bool CmdRaizBoton(std::istringstream& ss, std::string& err) {
    std::string t; ss >> t;
    Outliner* o = (Outliner*)PrimeraHoja(2);
    if (!o) { err = "raizboton: no hay outliner"; return false; }
    if (MenuAbierto) MenuAbierto->Cerrar();
    if (!ClickBoton(o, o->btnNueva)) { err = "raizboton: el \"+\" no esta visible"; return false; }
    if (!MenuAbierto || !MenuAbierto->abierto) { err = "raizboton: el click en el \"+\" no abrio su menu"; return false; }
    PopupMenu* m = MenuAbierto;
    static const char* kItems[3] = { "New Scene", "New Game", "New Prefab" };
    static const int kTipos[3] = { W3D_RAIZ_ESCENA, W3D_RAIZ_JUEGO, W3D_RAIZ_PREFAB };
    std::string hay;
    for (size_t i = 0; i < m->items.size(); i++) {
        char ic[16]; snprintf(ic, sizeof(ic), " %d", m->items[i]->icon);
        hay += " [" + m->items[i]->text + ic + "]";
    }
    printf("      [raizboton] +:%s\n", hay.c_str());
    // el "+" de la barra (fase 4b: el UNICO "+" del outliner): primero New Scene / New Game / New Prefab y
    // despues las altas de la biblioteca (New Folder / New Material). Nada mas.
    if (m->items.size() != 5 || m->items[3]->text != T("New Folder") || m->items[4]->text != T("New Material")) {
        m->Cerrar(); err = "raizboton: el \"+\" tiene que ofrecer New Scene / New Game / New Prefab / New Folder / New Material"; return false;
    }
    const int quiero = (t == "prefab") ? 2 : (t == "juego") ? 1 : 0;
    for (int k = 0; k < 3; k++) {
        MenuItem* it = m->items[(size_t)k];
        if (it->text != T(kItems[k]) || it->icon != W3dRaizIcono(kTipos[k])) {
            m->Cerrar(); err = std::string("raizboton: el item ") + (char)('1' + k) + " del \"+\" no es '" + kItems[k] + "' con su icono"; return false;
        }
    }
    const int antes = (int)W3dRaices().size();
    const int id = m->items[(size_t)quiero]->id;
    m->Cerrar();
    m->Ejecutar(id);
    if ((int)W3dRaices().size() != antes + 1) { err = "raizboton: elegir en el \"+\" no creo nada"; return false; }
    printf("      [raizboton] + %s -> '%s'\n", t.c_str(), W3dRaices()[(size_t)W3dRaizActiva()].nombre.c_str());
    if (rootViewport) rootViewport->Render();
    return true;
}

static bool CmdRaizBarra(std::istringstream& ss, std::string& err) {
    std::string donde, texto, k, icono; ss >> donde >> texto;
    if ((ss >> k) && k == "icono") ss >> icono;
    const bool out = (donde == "outliner");
    ViewportBase* v = PrimeraHoja(out ? 2 : 1);
    if (rootViewport) rootViewport->Render();
    Button* b = BotonSelector(v, out);
    if (!b) { err = "raizbarra: no hay un selector en la barra del " + donde; return false; }
    printf("      [raizbarra] %s: '%s' (icono %d, visible %d)\n", donde.c_str(), b->text.c_str(), b->icon, b->visible ? 1 : 0);
    // (el '*' del final es la marca de SIN GUARDAR -io/CambiosProyecto.h-: no es parte del nombre)
    std::string dice = b->text;
    if (!dice.empty() && dice[dice.size() - 1] == '*' && texto[texto.size() - 1] != '*') dice.erase(dice.size() - 1);
    if (dice != texto) { err = "raizbarra: el selector dice '" + b->text + "', se esperaba '" + texto + "'"; return false; }
    if (!icono.empty() && icono != IconoNombre(b->icon)) {
        err = "raizbarra: el selector tiene el icono '" + std::string(IconoNombre(b->icon)) + "', se esperaba '" + icono + "'"; return false;
    }
    return true;
}

static bool CmdPantallaFoto(std::istringstream& ss, std::string& err) {
    std::string ruta; ss >> ruta;
    if (ruta.empty() || !rootViewport) { err = "pantallafoto: uso: pantallafoto <ruta.ppm> (con layout)"; return false; }
    const int w = rootViewport->width, h = rootViewport->height;
    if (w <= 0 || h <= 0) { err = "pantallafoto: el layout no tiene tamanio"; return false; }
    rootViewport->Render();
    LayoutRenderMenu(w, h);   // los desplegables van encima de los paneles (como en el loop del editor)
    std::vector<unsigned char> buf((size_t)w * (size_t)h * 4);
    w3dEngine::ReadPixelsRGBA(0, 0, w, h, &buf[0]);
    FILE* f = fopen(ruta.c_str(), "wb");
    if (!f) { err = "pantallafoto: no pude escribir '" + ruta + "'"; return false; }
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int y = h - 1; y >= 0; y--)
        for (int x = 0; x < w; x++) fwrite(&buf[((size_t)y * (size_t)w + (size_t)x) * 4], 1, 3, f);
    fclose(f);
    printf("      [pantallafoto] %dx%d -> %s\n", w, h, ruta.c_str());
    return true;
}

static bool CmdRaizBorrar(std::istringstream& ss, std::string& err) {
    std::string n, k; ss >> n;
    int tipo = W3D_RAIZ_ESCENA; bool falla = false;
    while (ss >> k) { if (k == "prefab") tipo = W3D_RAIZ_PREFAB; else if (k == "falla") falla = true; }
    std::string motivo;
    const bool ok = W3dRaizBorrar(W3dRaizBuscar(tipo, n), &motivo);
    printf("      [raizborrar] %s '%s' -> %s %s\n", NombreTipo(tipo), n.c_str(), ok ? "borrada" : "NO", motivo.c_str());
    if (ok == falla) { err = falla ? "raizborrar: se borro y se esperaba que no" : "raizborrar: " + motivo; return false; }
    return true;
}

static bool CmdRaizRenombrar(std::istringstream& ss, std::string& err) {
    std::string v, n, k; ss >> v >> n;
    int tipo = W3D_RAIZ_ESCENA;
    if ((ss >> k) && k == "prefab") tipo = W3D_RAIZ_PREFAB;
    std::string quedo;
    if (!W3dRaizRenombrar(W3dRaizBuscar(tipo, v), n, &quedo)) { err = "raizrenombrar: no hay un " + std::string(NombreTipo(tipo)) + " '" + v + "'"; return false; }
    printf("      [raizrenombrar] '%s' -> '%s'\n", v.c_str(), quedo.c_str());
    return true;
}

static bool CmdEntradaExiste(std::istringstream& ss, std::string& err) {
    std::string z, e, x = "1"; ss >> z >> e >> x;
    const bool hay = LeerEntrada(z, e, NULL);
    printf("      [entradaexiste] %s : %s -> %s\n", z.c_str(), e.c_str(), hay ? "esta" : "no esta");
    if (hay != (x == "1")) { err = "entradaexiste: '" + e + "' " + (hay ? "esta" : "no esta") + " en " + z; return false; }
    return true;
}
static bool CmdEntradaTiene(std::istringstream& ss, std::string& err) {
    std::string z, e, t, x = "1"; ss >> z >> e >> t >> x;
    std::string d;
    if (!LeerEntrada(z, e, &d)) { err = "entradatiene: no hay una entrada '" + e + "' en " + z; return false; }
    const bool tiene = d.find(t) != std::string::npos;
    printf("      [entradatiene] %s : %s %s '%s'\n", z.c_str(), e.c_str(), tiene ? "tiene" : "no tiene", t.c_str());
    if (tiene != (x == "1")) { err = "entradatiene: '" + e + "' " + (tiene ? "tiene" : "no tiene") + " '" + t + "'"; return false; }
    return true;
}

// ---------------------------------------------------------------------------
//  juegoescenasmin / juegoescenaslog: cambiarEscena() con escenas 3D, en el Play y compilado
// ---------------------------------------------------------------------------
// el juego A cuenta sus inicio() y a los 3 ticks deja un dato en compartido() y pide la cinematica
static const char* kLuaEscA =
    "-- juego A: pasa a la escena (cinematica) 'Intro' (prueba del motor)\n"
    "local n = 0\n"
    "function inicio() setCompartido(\"a_inicios\", (compartido(\"a_inicios\") or 0) + 1) end\n"
    "function actualizar(dt)\n"
    "  n = n + 1\n"
    "  if n == 3 then setCompartido(\"dato\", 42); cambiarEscena(\"Intro\") end\n"
    "end\n";
// la CINEMATICA: su animacion la arranca el motor al entrar; el script la sigue y al terminar pide el nivel
static const char* kLuaEscC =
    "-- escena Intro: una cinematica (prueba del motor)\n"
    "local pedido = false\n"
    "function inicio() setCompartido(\"c_inicios\", (compartido(\"c_inicios\") or 0) + 1) end\n"
    "function actualizar(dt)\n"
    "  local f = animEscenaActual()\n"
    "  if f == nil then return end\n"
    "  setCompartido(\"intro_f\", f)\n"
    "  local x = posicion(buscar(\"CuboIntro\"))\n"
    "  setCompartido(\"intro_x\", x)\n"
    "  if f >= 11 and not pedido then pedido = true; cambiarEscena(\"Nivel2\") end\n"
    "end\n";
// EL JUEZ de la escena B: la A se descargo (su cubo ya no esta), la B cargo (su cubo y su camara),
// el dato de compartido() sobrevivio y cada escena arranco UNA vez
static const char* kLuaEscB =
    "-- escena B: el juez de la prueba de escenas 3D\n"
    "local t, listo = 0, false\n"
    "function inicio() setCompartido(\"b_inicios\", (compartido(\"b_inicios\") or 0) + 1) end\n"
    "function actualizar(dt)\n"
    "  if listo then return end\n"
    "  t = t + 1\n"
    "  if t < 5 then return end\n"
    "  listo = true\n"
    "  local cb, ca, cam = buscar(\"CuboB\") ~= nil, buscar(\"Cubo\") ~= nil, buscar(\"CamaraB\") ~= nil\n"
    "  local ci = buscar(\"CuboIntro\") ~= nil\n"
    "  local cine = (compartido(\"intro_f\") or 0) >= 11 and (compartido(\"intro_x\") or 0) > 9.9\n"
    "  local ok = compartido(\"dato\") == 42 and cb and cam and not ca and not ci and cine\n"
    "     and compartido(\"a_inicios\") == 1 and compartido(\"b_inicios\") == 1 and compartido(\"c_inicios\") == 1\n"
    "  info(string.format(\"[juegoescenas] %s dato=%s a=%s b=%s c=%s cine=%s(f=%s x=%s) cuboB=%s camaraB=%s cuboA=%s cuboIntro=%s\",\n"
    "    ok and \"OK\" or \"FALTA\", tostring(compartido(\"dato\")), tostring(compartido(\"a_inicios\")),\n"
    "    tostring(compartido(\"b_inicios\")), tostring(compartido(\"c_inicios\")), tostring(cine),\n"
    "    tostring(compartido(\"intro_f\")), tostring(compartido(\"intro_x\")), tostring(cb), tostring(cam),\n"
    "    tostring(ca), tostring(ci)))\n"
    "  setCompartido(\"juez\", ok and \"OK\" or \"FALTA\")\n"
    "  salir()\n"
    "end\n";
static bool EscribirTexto(const std::string& ruta, const char* texto) {
    FILE* f = fopen(ruta.c_str(), "wb");
    if (!f) return false;
    fputs(texto, f);
    fclose(f);
    return true;
}
static void ColgarScript(Object* o, const char* ruta) {
    if (!o->scriptDatos) o->scriptDatos = new W3dScriptDatos();
    W3dScriptEntrada e; e.ruta = ruta;
    o->scriptDatos->scripts.push_back(e);
}

static bool CmdJuegoEscenasMin(std::istringstream& ss, std::string& err) {
    std::string dir; ss >> dir;
    if (dir.empty()) { err = "juegoescenasmin: uso: juegoescenasmin <carpeta>"; return false; }
    std::string e2;
    if (!W3dRunCommand("juego3dmin " + dir, e2)) { err = "juegoescenasmin: " + e2; return false; }
    if (!EscribirTexto(dir + "/esc_a.lua", kLuaEscA) || !EscribirTexto(dir + "/esc_b.lua", kLuaEscB) ||
        !EscribirTexto(dir + "/esc_c.lua", kLuaEscC)) {
        err = "juegoescenasmin: no pude escribir los .lua en " + dir; return false;
    }
    // --- la escena A (la del bloque, la que arma juego3dmin) ---
    Empty* dir_a = new Empty(NULL, Vector3(0, -10, 0));
    dir_a->SetNameObj("DirectorA");
    ColgarScript(dir_a, "esc_a.lua");
    // --- la ESCENA "Intro": su camara, un cubo animado (su animacion ACTIVA, la que reproduce al
    //     entrar) y el script que la sigue. Es una escena aunque tenga script: se creo como escena ---
    const int c = W3dRaizCrearYAbrir(W3D_RAIZ_ESCENA, "Intro");
    if (c < 0) { err = "juegoescenasmin: no se pudo crear la escena Intro"; return false; }
    {
        Camera* camI = new Camera(NULL, Vector3(0, 0, 8), Vector3(0, 0, 0));
        camI->SetNameObj("CamaraIntro");
        camI->aspecto = 1.0f;
        Mesh* ci = (Mesh*)NewMesh(MeshType(MeshType::cube), NULL, false);
        ci->SetNameObj("CuboIntro");
        ColgarScript(ci, "esc_c.lua");
        InitSceneAnimations();
        SceneAnimation* cine = new SceneAnimation("Cine");
        cine->startFrame = 1; cine->endFrame = 11; cine->fps = 30;
        AnimationObject ao; ao.obj = ci; ao.FirstKeyFrame = 0; ao.LastKeyFrame = 0;
        SetKeyCurva(PropertyDeLista(ao.Propertys, AnimPosition, AnimX), 1, 0.0f);
        SetKeyCurva(PropertyDeLista(ao.Propertys, AnimPosition, AnimX), 11, 10.0f);
        ao.UpdateFirstLastFrame();
        cine->objetos.push_back(ao);
        SceneAnimations.push_back(cine);
        AnimSelPorId((int)SceneAnimations.size() - 1);   // (la animacion de la escena = la que se ve)
    }
    // --- el JUEGO B: su camara, su cubo y el juez ---
    const int b = W3dRaizCrearYAbrir(W3D_RAIZ_JUEGO, "Nivel2");
    if (b < 0) { err = "juegoescenasmin: no se pudo crear el juego B"; return false; }
    Camera* cam = new Camera(NULL, Vector3(0, 0, 8), Vector3(0, 0, 0));
    cam->SetNameObj("CamaraB");
    cam->aspecto = 1.0f;
    Mesh* cubo = (Mesh*)NewMesh(MeshType(MeshType::cube), NULL, false);
    cubo->SetNameObj("CuboB");
    Empty* juez = new Empty(NULL, Vector3(0, -10, 0));
    juez->SetNameObj("JuezB");
    ColgarScript(juez, "esc_b.lua");
    // se vuelve a la A: el juego arranca ahi (y "Compilar juego" necesita su HUD)
    std::string motivo;
    if (!W3dActivarRaiz(W3dRaizBloque(), &motivo)) { err = "juegoescenasmin: " + motivo; return false; }
    DeseleccionarTodo(); ObjActivo = NULL;
    printf("      [juegoescenasmin] juego3dmin + DirectorA (juego A) + CamaraIntro/CuboIntro (escena 'Intro', animacion 'Cine')"
           " + CamaraB/CuboB/JuezB (juego 'Nivel2') en '%s'\n", dir.c_str());
    return true;
}

static bool CmdJuegoEscenasLog(std::istringstream& ss, std::string& err) {
    std::string dir, nombre; ss >> dir >> nombre;
    if (dir.empty() || nombre.empty()) { err = "juegoescenaslog: uso: juegoescenaslog <carpeta> <nombre>"; return false; }
    const std::string carpeta = dir + "/build/linux";
    const std::string bin = carpeta + "/" + nombre;
    if (!w3dFileSystem::FileExists(bin)) { err = "juegoescenaslog: no existe el binario compilado '" + bin + "'"; return false; }
    const std::string log = carpeta + "/whisk3d.log";
    remove(log.c_str());
    char cmdRun[2200];
    snprintf(cmdRun, sizeof(cmdRun), "cd \"%s\" && timeout 120 ./%s > /dev/null 2>&1", carpeta.c_str(), nombre.c_str());
    const int r = system(cmdRun);
    FILE* f = fopen(log.c_str(), "rb");
    if (!f) { err = "juegoescenaslog: el juego no dejo whisk3d.log (se compilo en modo debug?)"; return false; }
    std::string veredicto, cargada, cine;
    char buf[2048];
    while (fgets(buf, sizeof(buf), f)) {
        const char* p = strstr(buf, "[juegoescenas]");
        if (p) veredicto = p;
        const char* q = strstr(buf, "[raices] escena 'Nivel2' cargada en la raiz activa");
        if (q) cargada = q;
        const char* r = strstr(buf, "[raices] escena 'Intro': reproduce su animacion 'Cine'");
        if (r) cine = r;
    }
    fclose(f);
    while (!veredicto.empty() && (veredicto[veredicto.size() - 1] == '\n' || veredicto[veredicto.size() - 1] == '\r'))
        veredicto.erase(veredicto.size() - 1);
    printf("      [juegoescenaslog] salida=%d | %s | %s\n", r, veredicto.empty() ? "(sin veredicto del juez)" : veredicto.c_str(),
           cargada.empty() ? "(la escena no se cargo de su entrada)" : "la escena se cargo de su entrada");
    if (veredicto.empty()) { err = "juegoescenaslog: el juez no escribio su veredicto (no se cambio de escena?)"; return false; }
    printf("      [juegoescenaslog] %s\n", cine.empty() ? "(la cinematica no arranco)" : "la ESCENA Intro reprodujo su animacion");
    if (veredicto.find("[juegoescenas] OK") == std::string::npos || veredicto.find("dato=42") == std::string::npos ||
        veredicto.find("cuboA=false") == std::string::npos || cargada.empty() || cine.empty()) {
        err = "juegoescenaslog: en el juego compilado cambiarEscena() no dio lo mismo que en el Play";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
//  ESCENA vs JUEGO: el tipo, el modo del timeline, sus clips y el render del cache
// ---------------------------------------------------------------------------

static bool CmdRaizTipo(std::istringstream& ss, std::string& err) {
    std::string t, k; ss >> t;
    bool falla = false;
    while (ss >> k) if (k == "falla") falla = true;
    if (t != "escena" && t != "juego") { err = "raiztipo: uso: raiztipo escena|juego [falla]"; return false; }
    std::string motivo;
    const bool ok = W3dRaizConvertirActiva(TipoDe(t), &motivo);
    printf("      [raiztipo] -> %s: %s%s%s\n", t.c_str(), ok ? "ok" : "NO", motivo.empty() ? "" : " ", motivo.c_str());
    if (ok == falla) { err = falla ? "raiztipo: se convirtio y se esperaba que no" : "raiztipo: " + motivo; return false; }
    return true;
}

static bool CmdRaizModo(std::istringstream& ss, std::string& err) {
    // el timeline de verdad (si el layout tiene uno) para el boton Auto Key; si no, uno de prueba
    Timeline* tl = (Timeline*)PrimeraHoja(4);
    Timeline* propio = NULL;
    if (!tl) { propio = new Timeline(); propio->Resize(900, 300); tl = propio; }
    tl->SyncFields();
    const bool autokey = tl->btnAutoKey && tl->btnAutoKey->visible;
    PopupMenu m;
    ConstruirMenuAnim(&m);
    bool menuJuego = false;
    for (size_t i = 0; i < m.items.size(); i++) if (m.items[i]->text == "Juego") menuJuego = true;
    printf("      [raizmodo] kind=%d juego=%d autokey=%d menuJuego=%d play=%d\n", ActiveAnimKind, AnimEsJuego ? 1 : 0,
           autokey ? 1 : 0, menuJuego ? 1 : 0, PlayAnimation ? 1 : 0);
    delete propio;
    std::string k;
    while (ss >> k) {
        int v = -1; if (!(ss >> v)) { err = "raizmodo: falta el valor de '" + k + "'"; return false; }
        if (k == "kind" && ActiveAnimKind != v) { err = "raizmodo: ActiveAnimKind=" + EsEntero(ActiveAnimKind) + ", se esperaba " + EsEntero(v); return false; }
        if (k == "juego" && (AnimEsJuego ? 1 : 0) != v) { err = "raizmodo: AnimEsJuego no es " + EsEntero(v); return false; }
        if (k == "autokey" && (autokey ? 1 : 0) != v) { err = "raizmodo: el boton Auto Key " + std::string(autokey ? "se ve" : "no se ve"); return false; }
        if (k == "menujuego" && (menuJuego ? 1 : 0) != v) { err = "raizmodo: el selector " + std::string(menuJuego ? "ofrece" : "no ofrece") + " \"Juego\""; return false; }
        if (k != "kind" && k != "juego" && k != "autokey" && k != "menujuego") { err = "raizmodo: no entiendo '" + k + "'"; return false; }
    }
    return true;
}

static bool CmdRaizAnim(std::istringstream& ss, std::string& err) {
    std::string n; ss >> n;
    if (n == "juego") { AnimSelJuego(); }
    else {
        const int i = W3dAnimEscenaIdx(n.c_str());
        if (i < 0) { err = "raizanim: no hay una animacion de escena '" + n + "'"; return false; }
        AnimSelPorId(i);
    }
    printf("      [raizanim] %s -> kind=%d juego=%d base=%d\n", n.c_str(), ActiveAnimKind, AnimEsJuego ? 1 : 0,
           W3dJuegoBaseHay() ? 1 : 0);
    return true;
}

static bool CmdRaizClip(std::istringstream& ss, std::string& err) {
    std::string n, on; int f0 = 1, f1 = 10; float x0 = 0.0f, x1 = 1.0f;
    if (!(ss >> n >> on >> f0 >> x0 >> f1 >> x1)) { err = "raizclip: uso: raizclip <nombre> <objeto> <f0> <x0> <f1> <x1>"; return false; }
    Object* o = SceneCollection ? FindObjectByName(SceneCollection, on) : NULL;
    if (!o) { err = "raizclip: no hay un objeto '" + on + "'"; return false; }
    InitSceneAnimations();
    SceneAnimation* e = new SceneAnimation(SceneAnimNombreLibre(n, -1));
    e->startFrame = f0; e->endFrame = f1; e->fps = 30;
    AnimationObject ao; ao.obj = o; ao.FirstKeyFrame = 0; ao.LastKeyFrame = 0;
    SetKeyCurva(PropertyDeLista(ao.Propertys, AnimPosition, AnimX), f0, x0);
    SetKeyCurva(PropertyDeLista(ao.Propertys, AnimPosition, AnimX), f1, x1);
    ao.UpdateFirstLastFrame();
    e->objetos.push_back(ao);
    SceneAnimations.push_back(e);
    printf("      [raizclip] '%s': %s.x %g (f%d) -> %g (f%d)\n", e->name.c_str(), on.c_str(), x0, f0, x1, f1);
    return true;
}

// cuantas curvas de escena (de la animacion activa) tiene el objeto activo
static int CurvasDelActivo() {
    for (size_t i = 0; i < AnimationObjects.size(); i++)
        if (AnimationObjects[i].obj == ObjActivo) {
            int n = 0;
            for (size_t p = 0; p < AnimationObjects[i].Propertys.size(); p++)
                n += (int)AnimationObjects[i].Propertys[p].keyframes.size();
            return n;
        }
    return 0;
}
static bool CmdRaizKey(std::istringstream& ss, std::string& err) {
    if (!ObjActivo) { err = "raizkey: no hay objeto activo"; return false; }
    InteractionMode = ObjectMode;
    const int antes = CurvasDelActivo();
    InsertarKeyframeContexto(0);
    const int n = CurvasDelActivo() - antes;
    printf("      [raizkey] Insert Keyframe en el frame %d (kind %d): %d keyframe(s) nuevos en '%s' (tiene %d)\n",
           CurrentFrame, ActiveAnimKind, n, ObjActivo->name.c_str(), antes + n);
    std::string k; int v = -1;
    if (ss >> k >> v && k == "n" && n != v) { err = "raizkey: se insertaron " + EsEntero(n) + " keyframes, se esperaban " + EsEntero(v); return false; }
    return true;
}

static bool CmdRaizDope(std::istringstream& ss, std::string& err) {
    int esperadas = -1; ss >> esperadas;
    Timeline* tl = new Timeline(); tl->Resize(900, 300);
    tl->ConstruirDopeRows();
    const int n = (int)tl->dopeRows.size();
    delete tl;
    printf("      [raizdope] el dope sheet arma %d fila(s) (kind %d)\n", n, ActiveAnimKind);
    if (esperadas >= 0 && n != esperadas) { err = "raizdope: " + EsEntero(n) + " filas, se esperaban " + EsEntero(esperadas); return false; }
    return true;
}

static bool CmdRaizScrub(std::istringstream& ss, std::string& err) {
    int f = 1; ss >> f;
    Timeline* tl = new Timeline(); tl->Resize(900, 300);
    tl->ConstruirDopeRows();
    tl->SetFrameFromX((int)(tl->FrameToX((float)f) + 0.5f));
    delete tl;
    printf("      [raizscrub] al frame %d -> CurrentFrame=%d (tick %d)\n", f, CurrentFrame, SimFrameActual());
    std::string k; int v = 0;
    if (ss >> k >> v && k == "cur" && CurrentFrame != v) { err = "raizscrub: el cabezal quedo en " + EsEntero(CurrentFrame) + ", se esperaba " + EsEntero(v); return false; }
    return true;
}

static int ContarPng(const std::string& dir, const std::string& pref) {
    std::vector<w3dFileSystem::DirEntry> fs;
    w3dFileSystem::ListDir(dir, fs);
    int n = 0;
    for (size_t i = 0; i < fs.size(); i++) {
        const std::string& f = fs[i].name;
        if (!fs[i].isDir && f.compare(0, pref.size(), pref) == 0 && f.size() > 4 && f.compare(f.size() - 4, 4, ".png") == 0) n++;
    }
    return n;
}
static bool CmdRenderCache(std::istringstream& ss, std::string& err) {
    std::string dir; ss >> dir;
    if (dir.empty()) { err = "rendercache: uso: rendercache <carpeta> [rango F0 F1] [n N | sin] [ini N] [fin N]"; return false; }
    Properties* P = (Properties*)PrimeraHoja(3);
    if (!P) { err = "rendercache: el layout no tiene panel de propiedades"; return false; }
    PropsActivo = P;
    if (rootViewport) rootViewport->Render();   // puebla Viewport3DActive
    if (!Viewport3DActive) { err = "rendercache: no hay viewport 3D"; return false; }
    { const std::string c = "mkdir -p \"" + dir + "\""; if (system(c.c_str()) != 0) { err = "rendercache: no pude crear " + dir; return false; } }
    P->renderW = 64.0f; P->renderH = 48.0f;
    if (P->propRenderPath) P->propRenderPath->field.text = dir;
    if (P->propRenderOutput) P->propRenderOutput->field.text = "cache.png";
    int esperadas = -2, ini = -1, fin = -1;
    std::string k;
    while (ss >> k) {
        if (k == "rango") { float a = 0, b = 0; ss >> a >> b; W3dRenderJuegoFijar(a, b); }
        else if (k == "auto") W3dRenderJuegoAuto();
        else if (k == "n") ss >> esperadas;
        else if (k == "sin") esperadas = -1;
        else if (k == "ini") ss >> ini;
        else if (k == "fin") ss >> fin;
        else { err = "rendercache: no entiendo '" + k + "'"; return false; }
    }
    P->RefreshTargetProperties();   // (la tarjeta Render se acomoda al cache: el mismo camino que la UI)
    int mn = 0, mx = -1;
    const bool hay = W3dRenderJuegoCache(&mn, &mx);
    // LA TARJETA lo dice (en "Juego"): sin cache, "Render Animation" en GRIS y el aviso; con cache, habilitado
    // y el rango cacheado en la nota
    if (ActiveAnimKind == 2 && AnimEsJuego) {
        const bool gris = P->propBtnAnimRender && P->propBtnAnimRender->gris;
        const std::string nota = (P->propRenderNota && !P->propRenderNota->oculto) ? P->propRenderNota->name : std::string();
        printf("      [rendercache] tarjeta: Render Animation %s; nota '%s'\n", gris ? "GRIS" : "habilitado", nota.c_str());
        if (!P->propBtnAnimRender || !P->propRenderNota) { err = "rendercache: la tarjeta Render no tiene el boton o la nota"; return false; }
        if (!hay && !gris) { err = "rendercache: sin cache, Render Animation deberia estar deshabilitado (gris)"; return false; }
        if (hay && gris) { err = "rendercache: con cache, Render Animation quedo deshabilitado"; return false; }
        const std::string sinCache = T("No game cache: play the game to record frames. Render Image renders the current frame.");
        if (!hay && nota != sinCache) { err = "rendercache: sin cache la nota deberia avisarlo, dice '" + nota + "'"; return false; }
        if (hay && nota.compare(0, strlen(T("Game cache")), T("Game cache")) != 0) {
            err = "rendercache: con cache la nota deberia mostrar su rango, dice '" + nota + "'"; return false;
        }
    }
    // (los PNG de una corrida anterior se borran: se cuentan los que deja ESTA)
    { const std::string c = "rm -f \"" + dir + "\"/cache_*.png"; if (system(c.c_str()) != 0) { err = "rendercache: no pude limpiar " + dir; return false; } }
    const int tick = SimFrameActual();
    W3dHarnessRenderAnimation();
    const int hechas = ContarPng(dir, "cache_");
    printf("      [rendercache] cache %s [%d, %d] rango [%g, %g] -> %d PNG; el cabezal volvio al tick %d (era %d)\n",
           hay ? "si" : "NO", mn, mx, g_renderJuegoIni, g_renderJuegoFin, hechas, SimFrameActual(), tick);
    if (SimFrameActual() != tick) { err = "rendercache: el cabezal no volvio al tick en el que estaba"; return false; }
    if (esperadas == -1 && (hay || hechas != 0)) { err = "rendercache: se esperaba que no hubiera cache"; return false; }
    if (esperadas >= 0 && hechas != esperadas) { err = "rendercache: guardo " + EsEntero(hechas) + " PNG, se esperaban " + EsEntero(esperadas); return false; }
    // cada PNG es el de SU tick: el render movio el cabezal a cada frame del rango (SimIrA), y el archivo lleva
    // el numero de ese frame (cache_0012.png): ni uno repetido ni uno salteado
    if (hay && hechas > 0)
        for (int f = (int)g_renderJuegoIni; f <= (int)g_renderJuegoFin; f++) {
            char nf[64]; snprintf(nf, sizeof(nf), "/cache_%04d.png", f);
            if (!w3dFileSystem::FileExists(dir + nf)) { err = "rendercache: falta el frame " + EsEntero(f) + " (" + dir + nf + ")"; return false; }
        }
    if (ini >= 0 && (int)g_renderJuegoIni != ini) { err = "rendercache: el inicio quedo en " + EsEntero((long)g_renderJuegoIni); return false; }
    if (fin >= 0 && (int)g_renderJuegoFin != fin) { err = "rendercache: el fin quedo en " + EsEntero((long)g_renderJuegoFin); return false; }
    return true;
}

// ---------------------------------------------------------------------------
//  LAS CINEMATICAS: reproducirEscena (una raiz ESCENA) y los clips de objeto / capas
// ---------------------------------------------------------------------------
static bool CmdCineInfo(std::istringstream& ss, std::string& err) {
    const std::vector<W3dRaizFila>& fs = W3dRaices();
    const int e = W3dCineEscena(), j = W3dCineJuego();
    const std::string ne = (e >= 0 && e < (int)fs.size()) ? fs[(size_t)e].nombre : std::string("-");
    const std::string nj = (j >= 0 && j < (int)fs.size()) ? fs[(size_t)j].nombre : std::string("-");
    printf("      [cineinfo] activa=%d escena=%s juego=%s\n", W3dCineActiva() ? 1 : 0, ne.c_str(), nj.c_str());
    std::string k;
    while (ss >> k) {
        std::string v; if (!(ss >> v)) { err = "cineinfo: falta el valor de '" + k + "'"; return false; }
        if (k == "activa") { if ((W3dCineActiva() ? 1 : 0) != atoi(v.c_str())) { err = std::string("cineinfo: la cinematica ") + (W3dCineActiva() ? "esta" : "no esta") + " sonando"; return false; } }
        else if (k == "escena") { if (ne != v) { err = "cineinfo: suena '" + ne + "', se esperaba '" + v + "'"; return false; } }
        else if (k == "juego") { if (nj != v) { err = "cineinfo: el juego es '" + nj + "', se esperaba '" + v + "'"; return false; } }
        else { err = "cineinfo: no entiendo '" + k + "'"; return false; }
    }
    return true;
}

static bool CmdRaizObjAt(std::istringstream& ss, std::string& err) {
    std::string r, n; float x = 0, y = 0, z = 0, tol = 0.001f;
    if (!(ss >> r >> n >> x >> y >> z >> tol)) { err = "raizobjat: uso: raizobjat <raiz> <objeto> <x> <y> <z> <tol>"; return false; }
    const std::vector<W3dRaizFila>& fs = W3dRaices();
    Object* raiz = NULL;
    for (size_t i = 0; i < fs.size() && !raiz; i++) if (fs[i].nombre == r) raiz = fs[i].raiz;
    if (!raiz) { err = "raizobjat: no hay una raiz cargada '" + r + "'"; return false; }
    Object* o = FindObjectByName(raiz, n);
    if (!o) { err = "raizobjat: '" + r + "' no tiene un objeto '" + n + "'"; return false; }
    printf("      [raizobjat] %s/%s en (%.4f,%.4f,%.4f)%s\n", r.c_str(), n.c_str(), o->pos.x, o->pos.y, o->pos.z,
           raiz == SceneCollection ? " (la activa)" : "");
    if (fabsf(o->pos.x - x) > tol || fabsf(o->pos.y - y) > tol || fabsf(o->pos.z - z) > tol) {
        char b[200]; snprintf(b, sizeof(b), "raizobjat: %s/%s esta en (%.4f,%.4f,%.4f), se esperaba (%.4f,%.4f,%.4f)",
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
static int AnimPorRef(const std::string& ref, bool crearClip, Object** duenioOut) {
    if (duenioOut) *duenioOut = NULL;
    const size_t dp = ref.find(':');
    if (dp == std::string::npos) return W3dAnimEscenaIdx(ref.c_str());
    Object* d = SceneCollection ? FindObjectByName(SceneCollection, ref.substr(0, dp)) : NULL;
    if (duenioOut) *duenioOut = d;
    if (!d) return -1;
    W3dClipJer* c = W3dJerClip(d, ref.substr(dp + 1));
    if (!c && crearClip) c = W3dJerClipNuevo(d, ref.substr(dp + 1));
    return c ? W3dClipVista(d, c) : -1;
}

static bool CmdAnimNueva(std::istringstream& ss, std::string& err) {
    std::string n; int f0 = 1, f1 = 250; ss >> n >> f0 >> f1;
    if (n.empty()) { err = "animnueva: uso: animnueva <nombre> [f0 f1]"; return false; }
    InitSceneAnimations();
    SceneAnimation* e = new SceneAnimation(SceneAnimNombreLibre(n, -1));
    e->startFrame = f0; e->endFrame = f1 < f0 ? f0 : f1; e->fps = 30;
    SceneAnimations.push_back(e);
    printf("      [animnueva] '%s' %d..%d\n", e->name.c_str(), e->startFrame, e->endFrame);
    return true;
}

static bool CmdClipNuevo(std::istringstream& ss, std::string& err) {
    std::string d, n; int f0 = 1, f1 = 250; ss >> d >> n >> f0 >> f1;
    Object* o = SceneCollection ? FindObjectByName(SceneCollection, d) : NULL;
    if (!o || n.empty()) { err = "clipnuevo: uso: clipnuevo <raiz> <nombre> [f0 f1]"; return false; }
    W3dClipJer* c = W3dJerClipNuevo(o, n);
    if (!c) { err = "clipnuevo: no se pudo"; return false; }
    c->inicio = f0; c->fin = f1 < f0 ? f0 : f1; c->version++;
    printf("      [clipnuevo] %s: %s (%d..%d) en la biblioteca '%s' (tamano %.3f)\n", o->name.c_str(), c->nombre.c_str(),
           c->inicio, c->fin, o->clipsJer ? o->clipsJer->animset.c_str() : "?", c->tamano);
    return true;
}

static bool CmdAnimCurva(std::istringstream& ss, std::string& err) {
    std::string ref, on, canal; ss >> ref >> on >> canal;
    Object* duenio = NULL;
    const int idx = AnimPorRef(ref, false, &duenio);
    if (idx < 0) { err = "animcurva: no hay una animacion '" + ref + "'"; return false; }
    Object* o = (on == ".") ? duenio : (SceneCollection ? FindObjectByName(SceneCollection, on) : NULL);
    if (!o) { err = "animcurva: no hay un objeto '" + on + "'"; return false; }
    int prop = AnimPosition, comp = AnimX;
    if (canal == "y") comp = AnimY; else if (canal == "z") comp = AnimZ;
    else if (canal == "rx") prop = AnimRotation; else if (canal == "ry") { prop = AnimRotation; comp = AnimY; }
    else if (canal == "rz") { prop = AnimRotation; comp = AnimZ; }
    else if (canal == "vis") prop = AnimVisible;
    else if (canal != "x") { err = "animcurva: canal x|y|z|rx|ry|rz|vis"; return false; }
    std::vector<AnimationObject>& lista = (idx == SceneAnimActiva) ? AnimationObjects : SceneAnimations[(size_t)idx]->objetos;
    AnimationObject* ao = NULL;
    for (size_t i = 0; i < lista.size() && !ao; i++) if (lista[i].obj == o) ao = &lista[i];
    if (!ao) { AnimationObject n; n.obj = o; n.FirstKeyFrame = 0; n.LastKeyFrame = 0; lista.push_back(n); ao = &lista.back(); }
    int f = 0; float v = 0.0f, nk = 0;
    while (ss >> f >> v) { SetKeyCurva(PropertyDeLista(ao->Propertys, prop, comp), f, v); nk++; }
    ao->UpdateFirstLastFrame();
    // la vista de un clip de jerarquia: lo que se keyeo va a su clip (lo reproducen todas sus raices)
    if (W3dClipVistaViva(idx)) W3dClipVistaEscribir(idx);
    W3dAnimCurvasInvalidar();
    printf("      [animcurva] %s: %s.%s <- %d keyframe(s)\n", W3dAnimEscenaEtiqueta(idx).c_str(), o->name.c_str(), canal.c_str(), (int)nk);
    return true;
}

static void JuntarRaicesJer(Object* o, std::vector<Object*>& out) {
    if (!o) return;
    if (o != SceneCollection && W3dJerAnimSet(o)) out.push_back(o);
    for (size_t i = 0; i < o->Childrens.size(); i++) JuntarRaicesJer(o->Childrens[i], out);
}
static bool CmdClipInfo(std::istringstream& ss, std::string& err) {
    std::string d; ss >> d;
    std::vector<Object*> raices;
    if (d != "*") {
        Object* o = SceneCollection ? FindObjectByName(SceneCollection, d) : NULL;
        if (!o) { err = "clipinfo: no hay un objeto '" + d + "'"; return false; }
        raices.push_back(o);
    } else JuntarRaicesJer(SceneCollection, raices);
    W3dClipsVistasSincronizar();
    std::vector<std::string> nombres;
    for (size_t r = 0; r < raices.size(); r++) {
        const W3dAnimSet* set = W3dJerAnimSet(raices[r]);
        if (!set) continue;
        for (size_t i = 0; i < set->datos.jerarquias.size(); i++) {
            const W3dClipJer* c = set->datos.jerarquias[i];
            nombres.push_back(d != "*" ? c->nombre : raices[r]->name + ": " + c->nombre);
            printf("      [clipinfo] %s: %s (%d..%d, %d fps, %d pista(s)) en '%s'\n", raices[r]->name.c_str(),
                   c->nombre.c_str(), c->inicio, c->fin, c->fps, (int)c->pistas.size(), set->nombre.c_str());
        }
    }
    std::string k;
    while (ss >> k) {
        std::string v; if (!(ss >> v)) { err = "clipinfo: falta el valor de '" + k + "'"; return false; }
        bool esta = false;
        for (size_t i = 0; i < nombres.size(); i++) if (nombres[i] == v) esta = true;
        if (k == "n") { if ((int)nombres.size() != atoi(v.c_str())) { err = "clipinfo: hay " + EsEntero((long)nombres.size()) + " clip(s), se esperaban " + v; return false; } }
        else if (k == "tiene") { if (!esta) { err = "clipinfo: no hay un clip '" + v + "'"; return false; } }
        else if (k == "no") { if (esta) { err = "clipinfo: hay un clip '" + v + "' y no deberia"; return false; } }
        else if (k == "activo") {
            const SceneAnimation* e = W3dAnimEsClip(SceneAnimActiva) ? SceneAnimations[(size_t)SceneAnimActiva] : NULL;
            const bool ok = e && ActiveAnimKind == 0 && W3dClipVistaViva(SceneAnimActiva) &&
                            (d != "*" ? (e->duenio && e->duenio->name == d && e->name == v) : W3dAnimEscenaEtiqueta(SceneAnimActiva) == v);
            if (!ok) { err = "clipinfo: lo que se edita es '" + W3dAnimEscenaEtiqueta(SceneAnimActiva) + "', no el clip '" + v + "'"; return false; }
        }
        else { err = "clipinfo: no entiendo '" + k + "'"; return false; }
    }
    return true;
}

// ---- las bibliotecas y el reproductor de clips de jerarquia, sin lua ----
static Object* ObjetoRaizActiva(const std::string& n) { return SceneCollection ? FindObjectByName(SceneCollection, n) : NULL; }

static bool CmdJerLib(std::istringstream& ss, std::string& err) {
    std::string on, lib; ss >> on >> lib;
    Object* o = ObjetoRaizActiva(on);
    if (!o || lib.empty()) { err = "jerlib: uso: jerlib <objeto> <biblioteca|->"; return false; }
    std::string motivo;
    const bool ok = W3dJerAsignar(o, lib == "-" ? std::string() : lib, &motivo);
    printf("      [jerlib] '%s' -> '%s'%s%s\n", on.c_str(), lib.c_str(), ok ? "" : ": NO: ", ok ? "" : motivo.c_str());
    if (!ok) { err = "jerlib: " + motivo; return false; }
    return true;
}

static bool CmdJerInfo(std::istringstream& ss, std::string& err) {
    std::string on; ss >> on;
    Object* o = ObjetoRaizActiva(on);
    if (!o) { err = "jerinfo: no hay un objeto '" + on + "'"; return false; }
    W3dClipsVistasSincronizar();
    const W3dAnimSet* set = W3dJerAnimSet(o);
    const W3dRecurso* r = W3dJerRecurso(o);
    const std::string lib = (o->clipsJer && !o->clipsJer->animset.empty()) ? o->clipsJer->animset : std::string("-");
    // 'refs' = las de sus USUARIOS (objetos y reproductores): las del historial de undo no cuentan (refsUndo)
    const int refsUso = r ? r->refTotal - (set ? set->refsUndo : 0) : 0;
    printf("      [jerinfo] '%s': biblioteca '%s' (%s), %d clip(s), refs %d (+%d del undo)\n", on.c_str(), lib.c_str(),
           r ? r->id.c_str() : "sin recurso", set ? (int)set->datos.jerarquias.size() : 0, refsUso, set ? set->refsUndo : 0);
    std::string k;
    while (ss >> k) {
        std::string v; if (!(ss >> v)) { err = "jerinfo: falta el valor de '" + k + "'"; return false; }
        if (k == "lib") { if (lib != v) { err = "jerinfo: la biblioteca es '" + lib + "', se esperaba '" + v + "'"; return false; } }
        else if (k == "clips") { const int n = set ? (int)set->datos.jerarquias.size() : 0;
            if (n != atoi(v.c_str())) { err = "jerinfo: tiene " + EsEntero(n) + " clip(s), se esperaban " + v; return false; } }
        else if (k == "refs") { const int n = refsUso;
            if (n != atoi(v.c_str())) { err = "jerinfo: el recurso tiene " + EsEntero(n) + " referencia(s), se esperaban " + v; return false; } }
        else if (k == "refsundo") { const int n = set ? set->refsUndo : 0;
            if (n != atoi(v.c_str())) { err = "jerinfo: el undo tiene " + EsEntero(n) + " referencia(s), se esperaban " + v; return false; } }
        else if (k == "tiene") { if (!W3dJerClip(o, v)) { err = "jerinfo: no tiene un clip '" + v + "'"; return false; } }
        else if (k == "pistas" || k == "ruta") {
            std::string x; ss >> x;
            const W3dClipJer* c = W3dJerClip(o, v);
            if (!c) { err = "jerinfo: no tiene un clip '" + v + "'"; return false; }
            if (k == "pistas") {
                printf("      [jerinfo] %s: %d pista(s):", v.c_str(), (int)c->pistas.size());
                for (size_t p = 0; p < c->pistas.size(); p++) printf(" '%s'", c->pistas[p].ruta.c_str());
                printf("\n");
                if ((int)c->pistas.size() != atoi(x.c_str())) { err = "jerinfo: el clip tiene " + EsEntero((long)c->pistas.size()) + " pista(s), se esperaban " + x; return false; }
            } else {
                bool esta = false;
                for (size_t p = 0; p < c->pistas.size(); p++) if (c->pistas[p].ruta == x) esta = true;
                if (!esta) { err = "jerinfo: el clip '" + v + "' no tiene la pista '" + x + "'"; return false; }
            }
        }
        else if (k == "retarget") {   // retarget <clip> <completo|rotaciones>: el modo POR DEFECTO del clip
            std::string m; ss >> m;
            const W3dClipJer* c = W3dJerClip(o, v);
            if (!c) { err = "jerinfo: no tiene un clip '" + v + "'"; return false; }
            const std::string cur = c->retarget == W3D_RETARGET_ROTACIONES ? "rotaciones" : "completo";
            if (cur != m) { err = "jerinfo: el retarget del clip '" + v + "' es '" + cur + "', se esperaba '" + m + "'"; return false; }
        }
        else if (k == "capas") {      // capas N: cuantas capas de clips de jerarquia tiene (el Mix de la raiz)
            const int n = o->clipsJer ? (int)o->clipsJer->capas.size() : 0;
            if (n != atoi(v.c_str())) { err = "jerinfo: tiene " + EsEntero(n) + " capa(s), se esperaban " + v; return false; }
        }
        else if (k == "tamano") {
            float tol = 0.01f; ss >> tol;
            const W3dClipJer* c = set && !set->datos.jerarquias.empty() ? set->datos.jerarquias[0] : NULL;
            const float t = c ? c->tamano : 0.0f;
            if (fabsf(t - (float)atof(v.c_str())) > tol) { char b[160]; snprintf(b, sizeof(b), "jerinfo: el tamano del clip es %.4f, se esperaba %s", t, v.c_str()); err = b; return false; }
        }
        else { err = "jerinfo: no entiendo '" + k + "'"; return false; }
    }
    return true;
}

static bool CmdJerRuta(std::istringstream& ss, std::string& err) {
    std::string rn, on, esperada; ss >> rn >> on >> esperada;
    Object* r = ObjetoRaizActiva(rn); Object* o = ObjetoRaizActiva(on);
    if (!r || !o) { err = "jerruta: uso: jerruta <raiz> <objeto> <ruta|->"; return false; }
    std::string ruta = W3dJerRuta(r, o);
    if (ruta.empty()) ruta = "-";
    printf("      [jerruta] %s -> %s = '%s'\n", rn.c_str(), on.c_str(), ruta.c_str());
    if (!esperada.empty() && ruta != esperada) { err = "jerruta: da '" + ruta + "', se esperaba '" + esperada + "'"; return false; }
    return true;
}

static bool CmdJerNodo(std::istringstream& ss, std::string& err) {
    std::string rn, ruta, esperado; ss >> rn >> ruta >> esperado;
    Object* r = ObjetoRaizActiva(rn);
    if (!r || ruta.empty()) { err = "jernodo: uso: jernodo <raiz> <ruta> <objeto|->"; return false; }
    Object* n = W3dJerNodo(r, ruta);
    const std::string got = n ? n->name : std::string("-");
    printf("      [jernodo] %s / '%s' = %s\n", rn.c_str(), ruta.c_str(), got.c_str());
    if (!esperado.empty() && got != esperado) { err = "jernodo: da '" + got + "', se esperaba '" + esperado + "'"; return false; }
    return true;
}

static bool CmdJerPlay(std::istringstream& ss, std::string& err) {
    std::string rn, clip, k; int loop = 1; ss >> rn >> clip;
    Object* r = ObjetoRaizActiva(rn);
    if (!r || clip.empty()) { err = "jerplay: uso: jerplay <raiz> <clip> [loop 0|1] [dur D]"; return false; }
    float durEsp = -2.0f;
    while (ss >> k) {
        if (k == "loop") ss >> loop;
        else if (k == "dur") ss >> durEsp;
        else { err = "jerplay: no entiendo '" + k + "'"; return false; }
    }
    W3dClipsVistasSincronizar();
    const float d = W3dAnimObjetoPlay(r, clip, loop != 0);
    printf("      [jerplay] %s: '%s' loop=%d -> %.4f s\n", rn.c_str(), clip.c_str(), loop, d);
    if (durEsp > -1.5f && fabsf(d - durEsp) > 0.001f) { char b[128]; snprintf(b, sizeof(b), "jerplay: dura %.4f, se esperaba %.4f", d, durEsp); err = b; return false; }
    if (durEsp <= -1.5f && d < 0.0f) { err = "jerplay: '" + rn + "' no encuentra el clip '" + clip + "'"; return false; }
    return true;
}

static bool CmdJerTick(std::istringstream& ss, std::string& err) {
    float dt = 0.0f; int n = 1; ss >> dt >> n;
    if (dt <= 0.0f) { err = "jertick: uso: jertick <dt> [n]"; return false; }
    for (int i = 0; i < n; i++) W3dAnimObjetosTick(dt);
    printf("      [jertick] %d x %.4f s (%d raiz(ces) sonando)\n", n, dt, W3dAnimObjetosSonando());
    return true;
}

static bool CmdJerVel(std::istringstream& ss, std::string& err) {
    std::string rn; float v = 1.0f; ss >> rn >> v;
    Object* r = ObjetoRaizActiva(rn);
    if (!r) { err = "jervel: uso: jervel <raiz> <v>"; return false; }
    if (!W3dAnimObjetoVelocidad(r, v)) { err = "jervel: a '" + rn + "' no le suena nada"; return false; }
    printf("      [jervel] %s x %.3f\n", rn.c_str(), v);
    return true;
}

static bool CmdJerRetarget(std::istringstream& ss, std::string& err) {
    std::string on, modo; ss >> on >> modo;
    Object* o = ObjetoRaizActiva(on);
    if (!o || modo.empty()) { err = "jerretarget: uso: jerretarget <objeto> completo|rotaciones|clip"; return false; }
    const int m = (modo == "rotaciones") ? W3D_RETARGET_ROTACIONES : (modo == "clip" ? -1 : W3D_RETARGET_COMPLETO);
    if (o->getType() == ObjectType::armature) W3dArmatureRetarget((Armature*)o, m < 0 ? W3D_RETARGET_COMPLETO : m);
    else W3dAnimObjetoRetarget(o, m);
    printf("      [jerretarget] %s: %s\n", on.c_str(), modo.c_str());
    return true;
}

// jerclipretarget <raiz> <clip> completo|rotaciones: el retarget por DEFECTO de un clip (el desplegable de la
// tarjeta Animacion con su vista elegida)
static bool CmdJerClipRetarget(std::istringstream& ss, std::string& err) {
    std::string rn, cn, modo; ss >> rn >> cn >> modo;
    Object* r = ObjetoRaizActiva(rn);
    W3dClipJer* c = r ? W3dJerClip(r, cn) : NULL;
    if (!c || modo.empty()) { err = "jerclipretarget: uso: jerclipretarget <raiz> <clip> completo|rotaciones"; return false; }
    c->retarget = (modo == "rotaciones") ? W3D_RETARGET_ROTACIONES : W3D_RETARGET_COMPLETO;
    c->version++;
    printf("      [jerclipretarget] %s/%s: %s\n", rn.c_str(), cn.c_str(), modo.c_str());
    return true;
}

static bool CmdJerFrame(std::istringstream& ss, std::string& err) {
    std::string rn, fe; ss >> rn >> fe;
    Object* r = ObjetoRaizActiva(rn);
    if (!r || fe.empty()) { err = "jerframe: uso: jerframe <raiz> <frame|-> [tol] [termino 0|1] [clip X]"; return false; }
    const float f = W3dAnimObjetoFrame(r);
    printf("      [jerframe] %s: frame %.4f clip '%s' termino=%d\n", rn.c_str(), f, W3dAnimObjetoClip(r).c_str(), W3dAnimObjetoTermino(r) ? 1 : 0);
    float tol = 0.001f;
    std::string k;
    if (fe == "-") { if (f >= 0.0f) { err = "jerframe: a '" + rn + "' le suena un clip"; return false; } }
    else {
        if (ss.peek() != EOF) { std::streampos p = ss.tellg(); float t = 0; if (ss >> t) tol = t; else { ss.clear(); ss.seekg(p); } }
        if (fabsf(f - (float)atof(fe.c_str())) > tol) { char b[128]; snprintf(b, sizeof(b), "jerframe: esta en %.4f, se esperaba %s", f, fe.c_str()); err = b; return false; }
    }
    while (ss >> k) {
        std::string v; if (!(ss >> v)) { err = "jerframe: falta el valor de '" + k + "'"; return false; }
        if (k == "termino") { if ((W3dAnimObjetoTermino(r) ? 1 : 0) != atoi(v.c_str())) { err = "jerframe: termino no es " + v; return false; } }
        else if (k == "clip") { if (W3dAnimObjetoClip(r) != v) { err = "jerframe: suena '" + W3dAnimObjetoClip(r) + "', se esperaba '" + v + "'"; return false; } }
        else { err = "jerframe: no entiendo '" + k + "'"; return false; }
    }
    return true;
}

// animrango <f0> <f1>: Start/End de la animacion ELEGIDA (como los campos de la tarjeta; la de una vista va a su clip)
static bool CmdAnimRango(std::istringstream& ss, std::string& err) {
    int f0 = 1, f1 = 250;
    if (!(ss >> f0 >> f1)) { err = "animrango: uso: animrango <f0> <f1>"; return false; }
    AnimSetStart(f0); AnimSetEnd(f1);
    if (W3dClipVistaViva(SceneAnimActiva)) W3dClipVistaEscribir(SceneAnimActiva);
    printf("      [animrango] '%s' %d..%d\n", W3dAnimEscenaEtiqueta(SceneAnimActiva).c_str(), StartFrame, EndFrame);
    return true;
}

// rotat <objeto> <rx> <ry> <rz> <tol>: la rotacion (euler, grados) de un objeto de la raiz activa
static bool CmdRotAt(std::istringstream& ss, std::string& err) {
    std::string on; float x = 0, y = 0, z = 0, tol = 0.01f; ss >> on >> x >> y >> z >> tol;
    Object* o = ObjetoRaizActiva(on);
    if (!o) { err = "rotat: no hay un objeto '" + on + "'"; return false; }
    const Vector3 e = o->rotEuler;
    printf("      [rotat] %s rot (%.3f,%.3f,%.3f)\n", on.c_str(), e.x, e.y, e.z);
    if (fabsf(e.x - x) > tol || fabsf(e.y - y) > tol || fabsf(e.z - z) > tol) {
        char b[200]; snprintf(b, sizeof(b), "rotat: %s rota (%.3f,%.3f,%.3f), se esperaba (%.3f,%.3f,%.3f)", on.c_str(), e.x, e.y, e.z, x, y, z);
        err = b; return false;
    }
    return true;
}

static bool CmdJerMem(std::istringstream& ss, std::string& err) {
    W3dAnimSetsStats st;
    W3dAnimSetsEstadisticas(st);
    printf("      [jermem] clips de jerarquia en memoria %d (%ld keyframes), animsets vivos %d, refs %d, sonando %d\n",
           st.clipsJer, st.keysJer, st.vivos, st.refs, W3dAnimObjetosSonando());
    std::string k;
    while (ss >> k) {
        std::string v; if (!(ss >> v)) { err = "jermem: falta el valor de '" + k + "'"; return false; }
        const int n = atoi(v.c_str());
        if (k == "clips" && st.clipsJer != n) { err = "jermem: hay " + EsEntero(st.clipsJer) + " clip(s) de jerarquia en memoria, se esperaban " + v; return false; }
        if (k == "refs" && st.refs != n) { err = "jermem: los animsets tienen " + EsEntero(st.refs) + " referencia(s), se esperaban " + v; return false; }
        if (k == "sonando" && W3dAnimObjetosSonando() != n) { err = "jermem: suenan " + EsEntero(W3dAnimObjetosSonando()) + ", se esperaban " + v; return false; }
        if (k != "clips" && k != "refs" && k != "sonando") { err = "jermem: no entiendo '" + k + "'"; return false; }
    }
    return true;
}

// ---- el RETARGET de los armatures: dos esqueletos con la MISMA jerarquia de nombres y distinto tamano ----
static Armature* RigRetarget(const std::string& nombre, float escala, float x) {
    Armature* a = new Armature(NULL, Vector3(x, 0, 0));
    a->SetNameObj(nombre);
    const char* nombres[3] = { "Cadera", "Pecho", "Cabeza" };
    for (int k = 0; k < 3; k++) {
        W3dBone b;
        b.name = nombres[k];
        b.parent = k - 1;
        b.head = Vector3(0.0f, escala * (1.0f + (float)k), 0.0f);
        b.tail = Vector3(0.0f, escala * (1.5f + (float)k), 0.0f);
        a->bones.push_back(b);
    }
    PrepararSkinAutorado(a);
    return a;
}
static bool CmdRigRetarget(std::istringstream& ss, std::string& err) {
    std::string pre; float esc = 2.0f; ss >> pre >> esc;
    if (pre.empty()) { err = "rigretarget: uso: rigretarget <prefijo> <escala>"; return false; }
    Armature* A = RigRetarget(pre + "A", 1.0f, -2.0f);
    Armature* B = RigRetarget(pre + "B", esc, 2.0f);
    // el clip se graba en A (CrearAnimacion le anota su tamano): la cadera avanza en X y el pecho rota y
    // (a proposito) se estira en Y -lo que "rotaciones" no tiene que copiar-
    CrearAnimacion(A);
    SkeletalAnimation* c = A->animations.back();
    c->name = "Caminar"; c->FrameRate = 30; c->startFrame = 1; c->endFrame = 11;
    BoneTrack& t0 = c->TrackDe(0);
    SetKeyCurva(t0.PropertyDe(AnimPosition, AnimX), 1, 0.0f);
    SetKeyCurva(t0.PropertyDe(AnimPosition, AnimX), 11, 10.0f);
    SetKeyCurva(t0.PropertyDe(AnimPosition, AnimY), 1, A->bones[0].restT.y);
    SetKeyCurva(t0.PropertyDe(AnimPosition, AnimY), 11, A->bones[0].restT.y);
    BoneTrack& t1 = c->TrackDe(1);
    SetKeyCurva(t1.PropertyDe(AnimRotation, AnimZ), 1, 0.0f);
    SetKeyCurva(t1.PropertyDe(AnimRotation, AnimZ), 11, 90.0f);
    SetKeyCurva(t1.PropertyDe(AnimPosition, AnimY), 1, A->bones[1].restT.y);
    SetKeyCurva(t1.PropertyDe(AnimPosition, AnimY), 11, A->bones[1].restT.y + 3.0f);
    // B usa EL MISMO clip (compartido: un animset en memoria)
    W3dArmatureAnimsVincular(B, A);
    A->animActiva = 0; B->animActiva = 0;
    printf("      [rigretarget] %s (tamano %.3f) y %s (x%.2f, tamano %.3f) con el clip 'Caminar' (tamano de origen %.3f)\n",
           A->name.c_str(), W3dArmatureTamReposo(A), B->name.c_str(), esc, W3dArmatureTamReposo(B), c->alturaReposo);
    return true;
}
static bool CmdHuesoPose(std::istringstream& ss, std::string& err) {
    std::string an, hn, k; float x = 0, y = 0, z = 0, tol = 0.001f; ss >> an >> hn >> x >> y >> z >> tol;
    Object* o = ObjetoRaizActiva(an);
    if (!o || o->getType() != ObjectType::armature) { err = "huesopose: no hay un armature '" + an + "'"; return false; }
    Armature* a = (Armature*)o;
    int h = -1;
    for (size_t i = 0; i < a->bones.size(); i++) if (a->bones[i].name == hn) h = (int)i;
    if (h < 0) { err = "huesopose: '" + an + "' no tiene el hueso '" + hn + "'"; return false; }
    float fr = 1.0f;
    bool hayHead = false; float hx = 0, hy = 0, hz = 0;
    while (ss >> k) {
        if (k == "frame") ss >> fr;
        else if (k == "head") { ss >> hx >> hy >> hz; hayHead = true; }   // la CABEZA del hueso tras el FK (pose)
    }
    // en JUEGO (kind 2) con su cabezal en 'fr' (como lo deja W3dArmaturesJuegoTick); con CAPAS, el de cada capa
    const int kind = ActiveAnimKind;
    ActiveAnimKind = 2;
    if (a->animActiva >= 0 && a->animActiva < (int)a->animations.size())
        a->juegoFrame = fr - (float)a->animations[(size_t)a->animActiva]->startFrame;
    for (size_t c = 0; c < a->capas.size(); c++) {
        const int ci = W3dCapaClip(a, a->capas[c]);
        if (ci >= 0) a->capas[c].juegoFrame = fr - (float)a->animations[(size_t)ci]->startFrame;
    }
    a->lastPoseFrame = -999999; a->mixFirma = 0;
    EvaluarPoseEsqueleto(a, 1);
    ActiveAnimKind = kind;
    const Vector3 T = a->bones[(size_t)h].poseT;
    const Vector3 R = a->bones[(size_t)h].poseR;
    const Vector3 H = a->bones[(size_t)h].poseHead;
    printf("      [huesopose] %s/%s en f%.2f: T (%.4f,%.4f,%.4f) R (%.2f,%.2f,%.2f) head (%.4f,%.4f,%.4f) retarget=%d escala=%.4f capas=%d\n",
           an.c_str(), hn.c_str(), fr, T.x, T.y, T.z, R.x, R.y, R.z, H.x, H.y, H.z, a->retarget, a->retargetEscala, (int)a->capas.size());
    if (fabsf(T.x - x) > tol || fabsf(T.y - y) > tol || fabsf(T.z - z) > tol) {
        char b[200]; snprintf(b, sizeof(b), "huesopose: T es (%.4f,%.4f,%.4f), se esperaba (%.4f,%.4f,%.4f)", T.x, T.y, T.z, x, y, z);
        err = b; return false;
    }
    if (hayHead && (fabsf(H.x - hx) > tol || fabsf(H.y - hy) > tol || fabsf(H.z - hz) > tol)) {
        char b[200]; snprintf(b, sizeof(b), "huesopose: la cabeza esta en (%.4f,%.4f,%.4f), se esperaba (%.4f,%.4f,%.4f)", H.x, H.y, H.z, hx, hy, hz);
        err = b; return false;
    }
    return true;
}

// busca en el menu (y sus submenus) la opcion con ese texto (o su traduccion)
static MenuItem* BuscarEnMenu(PopupMenu* m, const std::string& t, std::string& hay, int prof) {
    if (!m || prof > 3) return NULL;
    for (size_t i = 0; i < m->items.size(); i++) {
        MenuItem* it = m->items[i];
        if (!it) continue;
        hay += " [" + it->text + "]";
        if (!it->submenu && (it->text == t || it->text == T(t.c_str()))) return it;
        MenuItem* r = BuscarEnMenu(it->submenu, t, hay, prof + 1);
        if (r) return r;
    }
    return NULL;
}
static bool CmdClipMenu(std::istringstream& ss, std::string& err) {
    std::string t; std::getline(ss, t);
    const size_t a = t.find_first_not_of(' ');
    t = (a == std::string::npos) ? std::string() : t.substr(a);
    PopupMenu m;
    m.action = AnimSelPorId;
    ConstruirMenuAnim(&m);
    std::string hay;
    MenuItem* it = BuscarEnMenu(&m, t, hay, 0);
    if (!it) { printf("      [clipmenu]%s\n", hay.c_str()); err = "clipmenu: el selector no ofrece '" + t + "'"; return false; }
    AnimSelPorId(it->id);
    printf("      [clipmenu] '%s' -> editando '%s' (kind %d)\n", t.c_str(), W3dAnimEscenaEtiqueta(SceneAnimActiva).c_str(), ActiveAnimKind);
    return true;
}

static bool CmdClipNombre(std::istringstream& ss, std::string& err) {
    std::string n; ss >> n;
    if (n.empty()) { err = "clipnombre: uso: clipnombre <nombre>"; return false; }
    const std::string q = RenombrarEscenaActiva(n);
    printf("      [clipnombre] -> '%s'\n", W3dAnimEscenaEtiqueta(SceneAnimActiva).c_str());
    (void)q;
    return true;
}

// ---------------------------------------------------------------------------
//  juegocinemin / juegocinelog: los DOS caminos de las cinematicas, en el Play y compilado
// ---------------------------------------------------------------------------
// el JUEGO: dos clips de objeto y dos capas de escena a la vez; a los 5 ticks la cinematica
static const char* kLuaCineJuego =
    "-- juego: los dos caminos de las cinematicas (prueba del motor)\n"
    "-- (el dt del juego compilado es de reloj: todo se compara contra lo que el propio dt predice)\n"
    "local t, pedida, antes, tras, juzgado = 0, false, nil, 0, false\n"
    "local tc, antes2, pedida2 = nil, nil, false\n"
    "function inicio()\n"
    "  animObjeto(buscar(\"Puerta\"), \"abrir\", false)\n"
    "  animObjeto(buscar(\"Ascensor\"), \"subir\", true)\n"
    "  escenaCapa(1, \"Luces\", 1, \"mezclar\", false)\n"
    "  escenaCapa(2, \"Cielo\", 1, \"mezclar\", false)\n"
    "  -- clips AJENOS por su nombre calificado: la Grua (sin biblioteca) al doble de velocidad, la Puerta2 (el doble\n"
    "  -- de grande) con retarget por tamano, y la Plataforma MEZCLA el clip del ascensor al 50% en una capa\n"
    "  animObjeto(buscar(\"Grua\"), \"Puerta/abrir\", false)\n"
    "  animObjetoVelocidad(buscar(\"Grua\"), 2)\n"
    "  animRetarget(buscar(\"Puerta2\"), \"rotaciones\")\n"
    "  animObjeto(buscar(\"Puerta2\"), \"Puerta/abrir\", false)\n"
    "  objetoCapa(buscar(\"Plataforma\"), 1, \"Ascensor/subir\", 0.5, \"mezclar\", true)\n"
    "end\n"
    "-- los clips ajenos: la Grua va al doble que la Puerta (acotado al final de su clip), la Puerta2 sube el doble\n"
    "-- que lo que dice su frame (retarget) y la Plataforma la mitad de lo que dice su capa\n"
    "local function ajenos()\n"
    "  local fg, fp, f2 = animObjetoFrame(buscar(\"Grua\")), animObjetoFrame(buscar(\"Puerta\")), animObjetoFrame(buscar(\"Puerta2\"))\n"
    "  local jc = objetoCapaFrame(buscar(\"Plataforma\"), 1)\n"
    "  local yg, y2, yp = select(2, posicion(buscar(\"Grua\"))), select(2, posicion(buscar(\"Puerta2\"))), select(2, posicion(buscar(\"Plataforma\")))\n"
    "  local ok = fg ~= nil and f2 ~= nil and math.abs((fg - 1) - math.min(60, 2 * (fp - 1))) < 0.05 and math.abs(yg - (fg - 1)) < 0.02\n"
    "     and math.abs(y2 - 2 * (f2 - 1)) < 0.02 and jc > 0 and math.abs(yp - 0.5 * math.min(jc, 10)) < 0.02\n"
    "  return ok, string.format(\"grua=%.2f/%.2f puerta2=%.2f/%.2f plataforma=%.2f/%.2f\", fg or -1, yg, f2 or -1, y2, jc, yp)\n"
    "end\n"
    "function actualizar(dt)\n"
    "  t = t + 1\n"
    "  setCompartido(\"t\", t)\n"
    "  if t == 5 then\n"
    "    -- lo que el juego va a tener al terminar ESTE tick (la cinematica arranca al final del frame)\n"
    "    antes = animObjetoFrame(buscar(\"Puerta\")) + dt * 30\n"
    "    pedida = reproducirEscena(\"Cine1\", \"alVolver\")\n"
    "  end\n"
    "  -- medio segundo de juego despues de volver: todo siguio andando\n"
    "  if compartido(\"vuelta\") ~= nil then tras = tras + dt end\n"
    "  -- un rato despues de volver, la cinematica OTRA VEZ, pero esta la CORTA el script de la escena con\n"
    "  -- pararEscena3D(): se vuelve igual (con alCortada) y el juego sigue EXACTAMENTE donde estaba\n"
    "  if tc == nil and tras >= 0.09 then\n"
    "    tc = t\n"
    "    antes2 = animObjetoFrame(buscar(\"Puerta\")) + dt * 30\n"
    "    setCompartido(\"cortar\", true)\n"
    "    pedida2 = reproducirEscena(\"Cine1\", \"alCortada\")\n"
    "  end\n"
    "  if tras >= 0.49 and not juzgado then\n"
    "    juzgado = true\n"
    "    local fp = animObjetoFrame(buscar(\"Puerta\"))\n"
    "    local okA, txtA = ajenos()\n"
    "    -- la cortada: volvio en el mismo tick en que se pidio (el juego en pausa), la corto su script antes del\n"
    "    -- final de su animacion (frames 1..11) y la escena corrio sus scripts otra vez (dos cinematicas)\n"
    "    local okC = pedida2 and compartido(\"cortada\") == tc and compartido(\"iguales2\") == true\n"
    "       and (compartido(\"parada_f\") or 99) < 11 and compartido(\"c_inicios\") == 2\n"
    "    local ok = compartido(\"vuelta\") == 5 and compartido(\"iguales\") == true and (compartido(\"cine_f\") or 0) >= 8\n"
    "       and okC and pedida and fp > antes + 10 and not animObjetoTermino(buscar(\"Puerta\"))\n"
    "       and animObjetoActual(buscar(\"Ascensor\")) == \"subir\" and escenaCapaFrame(2) > 10 and visible(buscar(\"Lampara\")) and okA\n"
    "    info(string.format(\"[juegocine] %s vuelta=%s iguales=%s cine_f=%s c=%s puerta=%.2f antes=%.2f capa2=%.2f %s\"\n"
    "      .. \" cortada=%s/%s iguales2=%s parada_f=%s\",\n"
    "      ok and \"OK\" or \"FALTA\", tostring(compartido(\"vuelta\")), tostring(compartido(\"iguales\")),\n"
    "      tostring(compartido(\"cine_f\")), tostring(compartido(\"c_inicios\")), fp, antes or -1, escenaCapaFrame(2), txtA,\n"
    "      tostring(compartido(\"cortada\")), tostring(tc), tostring(compartido(\"iguales2\")), tostring(compartido(\"parada_f\"))))\n"
    "    setCompartido(\"juez\", ok and \"OK\" or \"FALTA\")\n"
    "    salir()\n"
    "  end\n"
    "end\n"
    "-- al VOLVER de la cinematica: el juego EXACTAMENTE como quedo (en pausa: ni un frame de mas)\n"
    "function alVolver()\n"
    "  local fp = animObjetoFrame(buscar(\"Puerta\"))\n"
    "  local py = select(2, posicion(buscar(\"Puerta\")))\n"
    "  setCompartido(\"vuelta\", t)\n"
    "  setCompartido(\"iguales\", math.abs(fp - antes) < 0.01 and math.abs(py - (fp - 1)) < 0.01\n"
    "                            and escenaReproduciendo() == nil)\n"
    "end\n"
    "-- al volver de la que CORTO pararEscena3D(): tambien exactamente como quedo, en el tick en que se pidio\n"
    "function alCortada()\n"
    "  local fp = animObjetoFrame(buscar(\"Puerta\"))\n"
    "  setCompartido(\"cortar\", false)\n"
    "  setCompartido(\"cortada\", t)\n"
    "  setCompartido(\"iguales2\", math.abs(fp - antes2) < 0.01 and escenaReproduciendo() == nil)\n"
    "end\n";
// la ESCENA de la cinematica: la sigue (su animacion la arranca el motor)
// (la segunda vez que se la pide, el juego marca "cortar": su script la corta con pararEscena3D() en su
//  segundo cuadro, antes de que termine su animacion)
static const char* kLuaCineEscena =
    "-- escena Cine1: la cinematica (prueba del motor)\n"
    "local n = 0\n"
    "function inicio() setCompartido(\"c_inicios\", (compartido(\"c_inicios\") or 0) + 1) end\n"
    "function actualizar(dt)\n"
    "  n = n + 1\n"
    "  local f = animEscenaActual()\n"
    "  if compartido(\"cortar\") then\n"
    "    if n == 2 then setCompartido(\"parada_f\", f or -1); pararEscena3D() end\n"
    "    return\n"
    "  end\n"
    "  if f ~= nil then setCompartido(\"cine_f\", f) end\n"
    "  setCompartido(\"cine_nombre\", escenaReproduciendo() or \"\")\n"
    "end\n";

// un clip de JERARQUIA de 'raiz' (en su biblioteca) con una curva Y lineal f0->f1 (valores v0->v1) sobre 'o'
// (la raiz o uno de sus descendientes, por su ruta)
static void ClipY(Object* raiz, Object* o, const char* nombre, int f0, float v0, int f1, float v1) {
    W3dClipJer* c = W3dJerClipNuevo(raiz, nombre);
    if (!c) return;
    c->inicio = f0; c->fin = f1; c->fps = 30;
    W3dPistaJer p;
    p.ruta = W3dJerRuta(raiz, o);
    SetKeyCurva(PropertyDeLista(p.props, AnimPosition, AnimY), f0, v0);
    SetKeyCurva(PropertyDeLista(p.props, AnimPosition, AnimY), f1, v1);
    c->pistas.push_back(p);
    c->version++;
}
// una animacion de escena (no la activa) con una sola curva sobre 'o'
static void AnimEscenaCurva(const char* nombre, Object* o, int prop, int comp, int f0, float v0, int f1, float v1) {
    InitSceneAnimations();
    SceneAnimation* e = new SceneAnimation(SceneAnimNombreLibre(nombre, -1));
    e->startFrame = f0; e->endFrame = f1; e->fps = 30;
    AnimationObject ao; ao.obj = o; ao.FirstKeyFrame = 0; ao.LastKeyFrame = 0;
    SetKeyCurva(PropertyDeLista(ao.Propertys, prop, comp), f0, v0);
    SetKeyCurva(PropertyDeLista(ao.Propertys, prop, comp), f1, v1);
    ao.UpdateFirstLastFrame();
    e->objetos.push_back(ao);
    SceneAnimations.push_back(e);
}

static bool CmdJuegoCineMin(std::istringstream& ss, std::string& err) {
    std::string dir; ss >> dir;
    if (dir.empty()) { err = "juegocinemin: uso: juegocinemin <carpeta>"; return false; }
    std::string e2;
    if (!W3dRunCommand("juego3dmin " + dir, e2)) { err = "juegocinemin: " + e2; return false; }
    if (!EscribirTexto(dir + "/cine_juego.lua", kLuaCineJuego) || !EscribirTexto(dir + "/cine_escena.lua", kLuaCineEscena)) {
        err = "juegocinemin: no pude escribir los .lua en " + dir; return false;
    }
    // --- el JUEGO (la raiz del bloque): la puerta y el ascensor con SUS clips de jerarquia, una lampara que se prende
    //     con la capa "Luces" y el cubo de juego3dmin que la capa "Cielo" corre en X ---
    Mesh* puerta = (Mesh*)NewMesh(MeshType(MeshType::cube), NULL, false);
    puerta->SetNameObj("Puerta"); puerta->pos = Vector3(-3, 0, 0);
    Mesh* asc = (Mesh*)NewMesh(MeshType(MeshType::cube), NULL, false);
    asc->SetNameObj("Ascensor"); asc->pos = Vector3(3, 0, 0);
    Mesh* lamp = (Mesh*)NewMesh(MeshType(MeshType::cube), NULL, false);
    lamp->SetNameObj("Lampara"); lamp->pos = Vector3(0, 3, 0); lamp->visible = false;
    Object* cubo = FindObjectByName(SceneCollection, "Cubo");
    ClipY(puerta, puerta, "abrir", 1, 0.0f, 61, 60.0f);    // una pasada, larga: y = frame - 1
    ClipY(asc, asc, "subir", 1, 0.0f, 11, 10.0f);          // con loop
    // los que reproducen clips AJENOS por su nombre calificado (sin biblioteca propia): la Grua, la Puerta2 (el
    // doble de grande: retarget) y la Plataforma (una capa que mezcla el del ascensor)
    Mesh* grua = (Mesh*)NewMesh(MeshType(MeshType::cube), NULL, false);
    grua->SetNameObj("Grua"); grua->pos = Vector3(-6, 0, 0);
    Mesh* puerta2 = (Mesh*)NewMesh(MeshType(MeshType::cube), NULL, false);
    puerta2->SetNameObj("Puerta2"); puerta2->pos = Vector3(-9, 0, 0); puerta2->scale = Vector3(2, 2, 2);
    Mesh* plat = (Mesh*)NewMesh(MeshType(MeshType::cube), NULL, false);
    plat->SetNameObj("Plataforma"); plat->pos = Vector3(6, 0, 0);
    AnimEscenaCurva("Luces", lamp, AnimVisible, AnimX, 1, 0.0f, 3, 1.0f);
    if (cubo) AnimEscenaCurva("Cielo", cubo, AnimPosition, AnimX, 1, 0.0f, 21, 2.0f);
    Empty* dirj = new Empty(NULL, Vector3(0, -10, 0));
    dirj->SetNameObj("DirectorCine");
    ColgarScript(dirj, "cine_juego.lua");
    // --- la ESCENA "Cine1": su camara, su luz y un cubo con su animacion (la que se reproduce) ---
    const int c = W3dRaizCrearYAbrir(W3D_RAIZ_ESCENA, "Cine1");
    if (c < 0) { err = "juegocinemin: no se pudo crear la escena Cine1"; return false; }
    {
        Camera* camC = new Camera(NULL, Vector3(0, 0, 8), Vector3(0, 0, 0));
        camC->SetNameObj("CamCine");
        camC->aspecto = 1.0f;
        Mesh* cc = (Mesh*)NewMesh(MeshType(MeshType::cube), NULL, false);
        cc->SetNameObj("CuboCine");
        ColgarScript(cc, "cine_escena.lua");
        InitSceneAnimations();
        SceneAnimation* cine = new SceneAnimation("Anim");
        cine->startFrame = 1; cine->endFrame = 11; cine->fps = 30;
        AnimationObject ao; ao.obj = cc; ao.FirstKeyFrame = 0; ao.LastKeyFrame = 0;
        SetKeyCurva(PropertyDeLista(ao.Propertys, AnimPosition, AnimX), 1, 0.0f);
        SetKeyCurva(PropertyDeLista(ao.Propertys, AnimPosition, AnimX), 11, 10.0f);
        ao.UpdateFirstLastFrame();
        cine->objetos.push_back(ao);
        SceneAnimations.push_back(cine);
        AnimSelPorId((int)SceneAnimations.size() - 1);   // (la animacion de la escena = la que se reproduce)
    }
    std::string motivo;
    if (!W3dActivarRaiz(W3dRaizBloque(), &motivo)) { err = "juegocinemin: " + motivo; return false; }
    DeseleccionarTodo(); ObjActivo = NULL;
    printf("      [juegocinemin] juego3dmin + Puerta/Ascensor (clips de jerarquia) + Grua/Puerta2/Plataforma (clips ajenos:"
           " velocidad, retarget, capa) + Luces/Cielo (capas) + DirectorCine;"
           " escena 'Cine1' (CamCine, CuboCine, animacion 'Anim') en '%s'\n", dir.c_str());
    return true;
}

static bool CmdJuegoCineLog(std::istringstream& ss, std::string& err) {
    std::string dir, nombre; ss >> dir >> nombre;
    if (dir.empty() || nombre.empty()) { err = "juegocinelog: uso: juegocinelog <carpeta> <nombre>"; return false; }
    const std::string carpeta = dir + "/build/linux";
    const std::string bin = carpeta + "/" + nombre;
    if (!w3dFileSystem::FileExists(bin)) { err = "juegocinelog: no existe el binario compilado '" + bin + "'"; return false; }
    const std::string log = carpeta + "/whisk3d.log";
    remove(log.c_str());
    char cmdRun[2200];
    snprintf(cmdRun, sizeof(cmdRun), "cd \"%s\" && timeout 120 ./%s > /dev/null 2>&1", carpeta.c_str(), nombre.c_str());
    const int r = system(cmdRun);
    FILE* f = fopen(log.c_str(), "rb");
    if (!f) { err = "juegocinelog: el juego no dejo whisk3d.log (se compilo en modo debug?)"; return false; }
    std::string veredicto, cargada, vuelta;
    char buf[2048];
    while (fgets(buf, sizeof(buf), f)) {
        const char* p = strstr(buf, "[juegocine]");
        if (p) veredicto = p;
        if (strstr(buf, "[raices] escena 'Cine1' cargada desde")) cargada = "si";   // (W3dRaizCargar: de su entrada)
        const char* q = strstr(buf, "[raices] cinematica 'Cine1' terminada");
        if (q) vuelta = q;
    }
    fclose(f);
    while (!veredicto.empty() && (veredicto[veredicto.size() - 1] == '\n' || veredicto[veredicto.size() - 1] == '\r'))
        veredicto.erase(veredicto.size() - 1);
    printf("      [juegocinelog] salida=%d | %s | %s | %s\n", r, veredicto.empty() ? "(sin veredicto del juez)" : veredicto.c_str(),
           cargada.empty() ? "(la escena no se cargo de su entrada)" : "la escena se cargo de su entrada",
           vuelta.empty() ? "(la cinematica no volvio)" : "la cinematica termino y volvio al juego");
    if (veredicto.find("[juegocine] OK") == std::string::npos || cargada.empty() || vuelta.empty()) {
        err = "juegocinelog: en el juego compilado las cinematicas no dieron lo mismo que en el Play";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
//  EL UNDO Y LA UI DE LOS CLIPS DE JERARQUIA, EL RETARGET Y LAS CAPAS (fase 6, segunda vuelta)
// ---------------------------------------------------------------------------
// el objeto pasa a ser el activo y el unico seleccionado (como un click en el outliner)
static Object* ElegirObjeto(const std::string& on) {
    Object* o = ObjetoRaizActiva(on);
    if (!o) return NULL;
    DeseleccionarTodo();
    o->Seleccionar();
    ObjActivo = o;
    return o;
}
// el resto de la linea (textos con espacios: "New Library", "Rotations only")
static std::string RestoLinea(std::istringstream& ss) {
    std::string t; std::getline(ss, t);
    const size_t a = t.find_first_not_of(' ');
    return (a == std::string::npos) ? std::string() : t.substr(a);
}
// jerlibmenu <objeto> <texto>: el desplegable "Clips" de la tarjeta Animacion (con su paso de undo)
static bool CmdJerLibMenu(std::istringstream& ss, std::string& err) {
    std::string on; ss >> on;
    const std::string t = RestoLinea(ss);
    if (!ElegirObjeto(on) || t.empty()) { err = "jerlibmenu: uso: jerlibmenu <objeto> <(none)|New Library|biblioteca>"; return false; }
    if (!PropsJerLibElegir(t)) { err = "jerlibmenu: el desplegable no ofrece '" + t + "'"; return false; }
    Object* o = ObjActivo;
    printf("      [jerlibmenu] %s -> '%s'\n", on.c_str(), (o && o->clipsJer) ? o->clipsJer->animset.c_str() : "");
    return true;
}
// objretarget <objeto> <texto>: el desplegable "Retarget" del objeto (con su paso de undo). Asserta con 'es':
//   objretarget <objeto> es <completo|rotaciones|clip>
static bool CmdObjRetarget(std::istringstream& ss, std::string& err) {
    std::string on; ss >> on;
    std::string t = RestoLinea(ss);
    Object* o = ObjetoRaizActiva(on);
    if (!o || t.empty()) { err = "objretarget: uso: objretarget <objeto> <Clip default|Complete|Rotations only> | es <modo>"; return false; }
    const int m = (o->getType() == ObjectType::armature) ? ((Armature*)o)->retarget : (o->clipsJer ? o->clipsJer->retarget : -1);
    const std::string cur = m == W3D_RETARGET_ROTACIONES ? "rotaciones" : m == W3D_RETARGET_COMPLETO ? "completo" : "clip";
    if (t.compare(0, 3, "es ") == 0) {
        const std::string esp = t.substr(3);
        printf("      [objretarget] %s: %s\n", on.c_str(), cur.c_str());
        if (esp != cur) { err = "objretarget: el retarget de '" + on + "' es '" + cur + "', se esperaba '" + esp + "'"; return false; }
        return true;
    }
    if (!ElegirObjeto(on) || !PropsObjRetargetElegir(t)) { err = "objretarget: el desplegable no ofrece '" + t + "'"; return false; }
    printf("      [objretarget] %s <- '%s'\n", on.c_str(), t.c_str());
    return true;
}
// clipretargetmenu <texto>: el desplegable "Retarget" del clip de jerarquia elegido (con su paso de undo)
static bool CmdClipRetargetMenu(std::istringstream& ss, std::string& err) {
    const std::string t = RestoLinea(ss);
    if (!W3dClipVistaViva(SceneAnimActiva)) { err = "clipretargetmenu: lo que se edita no es un clip de jerarquia"; return false; }
    if (!PropsClipRetargetElegir(t)) { err = "clipretargetmenu: el desplegable no ofrece '" + t + "'"; return false; }
    printf("      [clipretargetmenu] '%s' <- %s\n", W3dAnimEscenaEtiqueta(SceneAnimActiva).c_str(), t.c_str());
    return true;
}
// animmenos: el "-" de la tarjeta Animation (la animacion elegida; una escena o la vista de un clip: con undo)
static bool CmdAnimMenos(std::istringstream& ss, std::string& err) {
    (void)ss; (void)err;
    const std::string antes = W3dAnimEscenaEtiqueta(SceneAnimActiva);
    _AnimDelCardFwd();
    printf("      [animmenos] '%s' -> queda '%s' (kind %d)\n", antes.c_str(), W3dAnimEscenaEtiqueta(SceneAnimActiva).c_str(), ActiveAnimKind);
    return true;
}
// animlista [n N] [activa X] [tiene X]... [no X]...   las animaciones de escena de la raiz activa (sus etiquetas;
// las vistas HUERFANAS no cuentan: no se listan en la UI) y la elegida. La vista de un clip se escribe "Raiz:clip"
// (la etiqueta de la UI sin el espacio, como en animcurva).
static std::string EtiquetaCompacta(int idx) {
    std::string e = W3dAnimEscenaEtiqueta(idx);
    const size_t p = e.find(": ");
    if (p != std::string::npos && W3dAnimEsClip(idx)) e.erase(p + 1, 1);
    return e;
}
static bool CmdAnimLista(std::istringstream& ss, std::string& err) {
    InitSceneAnimations();
    std::vector<std::string> et;
    std::string txt;
    for (size_t i = 0; i < SceneAnimations.size(); i++) {
        if (SceneAnimations[i]->esClip && !W3dClipVistaViva((int)i)) continue;
        et.push_back(EtiquetaCompacta((int)i));
        txt += " '" + et.back() + "'";
    }
    const std::string act = (ActiveAnimKind == 0) ? EtiquetaCompacta(SceneAnimActiva) : std::string("-");
    printf("      [animlista]%s (activa '%s')\n", txt.c_str(), act.c_str());
    std::string k;
    while (ss >> k) {
        std::string v; if (!(ss >> v)) { err = "animlista: falta el valor de '" + k + "'"; return false; }
        bool esta = false;
        for (size_t i = 0; i < et.size(); i++) if (et[i] == v) esta = true;
        if (k == "n") { if ((int)et.size() != atoi(v.c_str())) { err = "animlista: hay " + EsEntero((long)et.size()) + ", se esperaban " + v; return false; } }
        else if (k == "activa") { if (act != v) { err = "animlista: la elegida es '" + act + "', se esperaba '" + v + "'"; return false; } }
        else if (k == "tiene") { if (!esta) { err = "animlista: no esta '" + v + "'"; return false; } }
        else if (k == "no") { if (esta) { err = "animlista: esta '" + v + "' y no deberia"; return false; } }
        else { err = "animlista: no entiendo '" + k + "'"; return false; }
    }
    return true;
}
// jercapa <raiz> <n> <clip|-> [infl %] [modo mezclar|sumar|restar] [vel V] [desde D] [loop 0|1] [frame F]
//   la capa n (base 1) de clips de jerarquia de la raiz (como objetoCapa de lua); '-' la saca (y las de arriba)
static bool CmdJerCapa(std::istringstream& ss, std::string& err) {
    std::string rn, clip; int n = 0; ss >> rn >> n >> clip;
    Object* r = ObjetoRaizActiva(rn);
    if (!r || n < 1 || clip.empty()) { err = "jercapa: uso: jercapa <raiz> <n> <clip|-> [infl %] [modo m] [vel v] [desde d] [loop 0|1] [frame f]"; return false; }
    if (clip == "-") {
        if (r->clipsJer && (int)r->clipsJer->capas.size() >= n) { r->clipsJer->capas.resize(n - 1); W3dJerCapasCambiaron(r); W3dMixEscenasSoltar(); }
        printf("      [jercapa] %s: %d capa(s)\n", rn.c_str(), r->clipsJer ? (int)r->clipsJer->capas.size() : 0);
        return true;
    }
    if (!r->clipsJer) r->clipsJer = new W3dJerRaiz();
    if ((int)r->clipsJer->capas.size() < n) r->clipsJer->capas.resize(n);
    W3dCapaAnim& c = r->clipsJer->capas[(size_t)n - 1];
    c.anim = clip; c.retClip = 0;
    std::string k;
    while (ss >> k) {
        if (k == "infl") ss >> c.influencia;
        else if (k == "modo") { std::string m; ss >> m; c.modo = (m == "sumar") ? 1 : (m == "restar") ? 2 : 0; }
        else if (k == "vel") ss >> c.vel;
        else if (k == "desde") ss >> c.desde;
        else if (k == "loop") { int l = 1; ss >> l; c.loop = l != 0; }
        else if (k == "frame") ss >> c.juegoFrame;
        else { err = "jercapa: no entiendo '" + k + "'"; return false; }
    }
    W3dJerCapasCambiaron(r);
    printf("      [jercapa] %s capa %d: '%s' %.0f%% modo %d (clip %s)\n", rn.c_str(), n, clip.c_str(), c.influencia, c.modo,
           W3dJerCapaClip(r, c, 0) ? "encontrado" : "NO ENCONTRADO");
    return true;
}
// jermix [frame F] | [tick DT [n]]: el Mix de objetos (capas de escena + de jerarquia) aplicado en el frame F del
// editor, o n ticks del JUEGO (cada capa avanza su cabezal)
static bool CmdJerMix(std::istringstream& ss, std::string& err) {
    std::string k; ss >> k;
    if (k == "frame") { int f = 1; ss >> f; const bool hay = W3dMixEscenasAplicar(f, false);
        printf("      [jermix] frame %d: %s\n", f, hay ? "mezcla aplicada" : "sin capas"); return true; }
    if (k == "tick") { float dt = 0.0f; int n = 1; ss >> dt >> n;
        for (int i = 0; i < n; i++) W3dMixEscenasTick(dt);
        printf("      [jermix] %d tick(s) de %.4f s\n", n, dt); return true; }
    if (k == "soltar") { W3dMixEscenasSoltar(); printf("      [jermix] los objetos vuelven a su base\n"); return true; }
    err = "jermix: uso: jermix frame F | tick DT [n] | soltar";
    return false;
}
// mixjer <raiz> <clip>: el menu "Agregar" del Mix, submenu de esa raiz, item del clip (la capa queda elegida)
static bool CmdMixJer(std::istringstream& ss, std::string& err) {
    std::string rn, clip; ss >> rn >> clip;
    Object* r = ObjetoRaizActiva(rn);
    if (!r || clip.empty()) { err = "mixjer: uso: mixjer <raiz> <clip>"; return false; }
    if (!PropsMixAgregarMenu(rn, clip)) { err = "mixjer: el Mix no ofrece '" + clip + "' para '" + rn + "'"; return false; }
    printf("      [mixjer] %s: capa '%s' (editando '%s')\n", rn.c_str(), clip.c_str(), W3dAnimEscenaEtiqueta(SceneAnimActiva).c_str());
    return true;
}
// mixfilas [n N] [tiene X]...   las filas del arbol del Mix (lo que dibuja la tarjeta)
static bool CmdMixFilas(std::istringstream& ss, std::string& err) {
    const int n = MixFilasCount ? MixFilasCount() : 0;
    std::vector<std::string> fs;
    std::string txt;
    for (int i = 0; i < n; i++) { fs.push_back(MixFilaTexto(i)); txt += " [" + fs.back() + "]"; }
    printf("      [mixfilas]%s\n", txt.c_str());
    std::string k;
    while (ss >> k) {
        std::string v; if (!(ss >> v)) { err = "mixfilas: falta el valor de '" + k + "'"; return false; }
        if (k == "n") { if (n != atoi(v.c_str())) { err = "mixfilas: hay " + EsEntero(n) + " fila(s), se esperaban " + v; return false; } }
        else if (k == "tiene") { bool esta = false;
            for (size_t i = 0; i < fs.size(); i++) if (fs[i].find(v) != std::string::npos) esta = true;
            if (!esta) { err = "mixfilas: ninguna fila dice '" + v + "'"; return false; } }
        else { err = "mixfilas: no entiendo '" + k + "'"; return false; }
    }
    return true;
}
// rigcapa <armature> <clip> [infl %]: una capa del Mix del armature con ese clip (animCapa sin lua)
static bool CmdRigCapa(std::istringstream& ss, std::string& err) {
    std::string an, clip; float infl = 100.0f; ss >> an >> clip >> infl;
    Object* o = ObjetoRaizActiva(an);
    if (!o || o->getType() != ObjectType::armature || clip.empty()) { err = "rigcapa: uso: rigcapa <armature> <clip|-> [infl]"; return false; }
    Armature* a = (Armature*)o;
    if (clip == "-") { a->capas.clear(); a->lastPoseFrame = -999999; printf("      [rigcapa] %s: sin capas\n", an.c_str()); return true; }
    W3dCapaAnim c; c.anim = clip; c.influencia = infl;
    a->capas.push_back(c);
    a->lastPoseFrame = -999999; a->mixFirma = 0;
    printf("      [rigcapa] %s: %d capa(s)\n", an.c_str(), (int)a->capas.size());
    return true;
}
// rigbiped <armature>...: esos armatures pasan a reconstruir el FK desde sus TransformLink (el camino del biped
// de un FBX): la traslacion de cada hueso entra como DELTA contra su rest
static bool CmdRigBiped(std::istringstream& ss, std::string& err) {
    std::string an; int n = 0;
    while (ss >> an) {
        Object* o = ObjetoRaizActiva(an);
        if (!o || o->getType() != ObjectType::armature) { err = "rigbiped: no hay un armature '" + an + "'"; return false; }
        Armature* a = (Armature*)o;
        a->skinReconstruirFK = true;
        a->skinAutorado = false;   // (el tail sale del FK, como en un rig importado)
        a->retargetClip = NULL; a->lastPoseFrame = -999999;
        printf("      [rigbiped] %s: FK reconstruido, tamano %.4f\n", an.c_str(), W3dArmatureTamReposo(a));
        n++;
    }
    if (!n) { err = "rigbiped: uso: rigbiped <armature>..."; return false; }
    return true;
}
// rigskinglb <armature> <ruta.glb>: una malla (un cubo, todo al primer hueso) skinneada al armature, exportada
// SOLA a un GLB (el armature y sus clips van con ella). La malla se borra despues.
static bool CmdRigSkinGlb(std::istringstream& ss, std::string& err) {
    std::string an, ruta; ss >> an >> ruta;
    Object* o = ObjetoRaizActiva(an);
    if (!o || o->getType() != ObjectType::armature || ruta.empty()) { err = "rigskinglb: uso: rigskinglb <armature> <ruta.glb>"; return false; }
    Armature* a = (Armature*)o;
    Mesh* m = (Mesh*)NewMesh(MeshType(MeshType::cube), NULL, false);
    m->SetNameObj("PielGlb");
    m->GenerarRender();
    WeightPaintAsegurarMapa(m);
    VertexGroup* vg = new VertexGroup(a->bones.empty() ? std::string("x") : a->bones[0].name);
    std::vector<char> hecho(m->vertexSize, 0);
    for (int i = 0; i < m->vertexSize && i < (int)m->vertCtrlPoint.size(); i++) {
        const int cp = m->vertCtrlPoint[i];
        if (cp < 0 || cp >= (int)hecho.size() || hecho[cp]) continue;
        hecho[cp] = 1; vg->verts.push_back(cp); vg->pesos.push_back(1.0f);
    }
    m->vertexGroups.push_back(vg);
    m->skinArmature = a;
    DeseleccionarTodo(); m->Seleccionar(); ObjActivo = m;
    const bool ok = ExportGLTF(ruta, true, true);
    printf("      [rigskinglb] %s -> %s: %s\n", an.c_str(), ruta.c_str(), ok ? "OK" : "FALLO");
    m->skinArmature = NULL;
    // fuera del arbol ANTES de liberarla (~Object no se descuelga de su padre)
    DeseleccionarTodo(); ObjActivo = NULL;
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
// del esqueleto que lo grabo (alturaReposo = W3dArmatureTamReposo del armature importado, > 0)
static void JuntarArms(Object* o, std::vector<Armature*>& out) {
    if (!o) return;
    if (o->getType() == ObjectType::armature) out.push_back((Armature*)o);
    for (size_t i = 0; i < o->Childrens.size(); i++) JuntarArms(o->Childrens[i], out);
}
static bool CmdImportClipsAlto(std::istringstream& ss, std::string& err) {
    std::string ruta, k; int nEsp = -1; ss >> ruta;
    while (ss >> k) { if (k == "n") ss >> nEsp; }
    std::vector<Armature*> antes; JuntarArms(SceneCollection, antes);
    if (!ImportGLTF(ruta)) { err = "importclipsalto: no se pudo importar '" + ruta + "'"; return false; }
    std::vector<Armature*> despues; JuntarArms(SceneCollection, despues);
    int clips = 0;
    for (size_t i = 0; i < despues.size(); i++) {
        if (std::find(antes.begin(), antes.end(), despues[i]) != antes.end()) continue;
        Armature* a = despues[i];
        const float tam = W3dArmatureTamReposo(a);
        for (size_t c = 0; c < a->animations.size(); c++) {
            const SkeletalAnimation* cl = a->animations[c];
            if (!cl) continue;
            clips++;
            printf("      [importclipsalto] %s/%s: alturaReposo %.4f (tamano del armature %.4f)\n", a->name.c_str(), cl->name.c_str(), cl->alturaReposo, tam);
            if (cl->alturaReposo <= 0.0f || fabsf(cl->alturaReposo - tam) > 1e-4f) {
                err = "importclipsalto: el clip '" + cl->name + "' no trae el tamano de su esqueleto"; return false;
            }
        }
    }
    if (nEsp >= 0 && clips != nEsp) { err = "importclipsalto: se importaron " + EsEntero(clips) + " clip(s), se esperaban " + EsEntero(nEsp); return false; }
    return true;
}

// ---------------------------------------------------------------------------
//  notif / keyrombo / menuobjeto: lo que se le avisa al usuario y los caminos de la UI que keyean
// ---------------------------------------------------------------------------
static bool CmdNotif(std::istringstream& ss, std::string& err) {
    std::string k; ss >> k;
    if (k == "limpiar") { NotificacionesLimpiar(); return true; }
    const std::string t = RestoLinea(ss);
    if ((k != "tiene" && k != "no") || t.empty()) { err = "notif: uso: notif tiene <texto> | notif no <texto> | notif limpiar"; return false; }
    // (el texto en ingles, como va en T(): se busca tambien traducido, que es como sale en pantalla)
    const std::string tr = T(t.c_str());
    const bool hay = NotificacionVisible(t) || NotificacionVisible(tr);
    printf("      [notif] '%s' ('%s'): %s\n", t.c_str(), tr.c_str(), hay ? "se ve" : "no se ve");
    if (k == "tiene" && !hay) { err = "notif: no hay ninguna notificacion con '" + t + "'"; return false; }
    if (k == "no" && hay) { err = "notif: hay una notificacion con '" + t + "'"; return false; }
    return true;
}
static bool CmdKeyRombo(std::istringstream& ss, std::string& err) {
    int prop = -1, comp = 0; ss >> prop >> comp;
    if (prop < 0) { err = "keyrombo: uso: keyrombo <prop> <comp> [estado N]"; return false; }
    if (!W3dKeyframeToggle || !W3dKeyframeEstado) { err = "keyrombo: el panel no cableo los hooks del rombo"; return false; }
    W3dKeyframeToggle(prop, comp);
    const int est = W3dKeyframeEstado(prop, comp);
    printf("      [keyrombo] prop=%d comp=%d frame=%d -> estado=%d\n", prop, comp, CurrentFrame, est);
    std::string k; int v = 0;
    if (ss >> k >> v && k == "estado" && est != v) { err = "keyrombo: el rombo quedo en " + EsEntero(est) + ", se esperaba " + EsEntero(v); return false; }
    return true;
}
static bool CmdMenuObjeto(std::istringstream& ss, std::string& err) {
    int id = -1; ss >> id;
    if (id < 0) { err = "menuobjeto: uso: menuobjeto <id>"; return false; }
    LayoutAccionObject(id);
    return true;
}

// ============================================================================
//  el despachador
// ============================================================================
bool W3dPruebasEscenasCmd(const std::string& cmd, std::istringstream& ss, std::string& err, bool& manejado) {
    manejado = true;
    if (cmd == "raiznueva")       return CmdRaizNueva(ss, err);
    if (cmd == "raizabrir")       return CmdRaizAbrir(ss, err);
    if (cmd == "raizinfo")        return CmdRaizInfo(ss, err);
    if (cmd == "raizcontexto")    return CmdRaizContexto(ss, err);
    if (cmd == "raizcamara")      return CmdRaizCamara(ss, err);
    if (cmd == "raizluces")       return CmdRaizLuces(ss, err);
    if (cmd == "raizparent")      return CmdRaizParent(ss, err);
    if (cmd == "raizscript")      return CmdRaizScript(ss, err);
    if (cmd == "raizinicial")     return CmdRaizInicial(ss, err);
    if (cmd == "raizmenu")        return CmdRaizMenu(ss, err);
    if (cmd == "raizboton")       return CmdRaizBoton(ss, err);
    if (cmd == "raizbarra")       return CmdRaizBarra(ss, err);
    if (cmd == "pantallafoto")    return CmdPantallaFoto(ss, err);
    if (cmd == "raizborrar")      return CmdRaizBorrar(ss, err);
    if (cmd == "raizrenombrar")   return CmdRaizRenombrar(ss, err);
    if (cmd == "entradaexiste")   return CmdEntradaExiste(ss, err);
    if (cmd == "entradatiene")    return CmdEntradaTiene(ss, err);
    if (cmd == "raiztipo")        return CmdRaizTipo(ss, err);
    if (cmd == "raizmodo")        return CmdRaizModo(ss, err);
    if (cmd == "raizanim")        return CmdRaizAnim(ss, err);
    if (cmd == "raizclip")        return CmdRaizClip(ss, err);
    if (cmd == "raizkey")         return CmdRaizKey(ss, err);
    if (cmd == "raizdope")        return CmdRaizDope(ss, err);
    if (cmd == "raizscrub")       return CmdRaizScrub(ss, err);
    if (cmd == "rendercache")     return CmdRenderCache(ss, err);
    if (cmd == "notif")           return CmdNotif(ss, err);
    if (cmd == "keyrombo")        return CmdKeyRombo(ss, err);
    if (cmd == "menuobjeto")      return CmdMenuObjeto(ss, err);
    if (cmd == "cineinfo")        return CmdCineInfo(ss, err);
    if (cmd == "raizobjat")       return CmdRaizObjAt(ss, err);
    if (cmd == "objvis")          return CmdObjVis(ss, err);
    if (cmd == "animnueva")       return CmdAnimNueva(ss, err);
    if (cmd == "clipnuevo")       return CmdClipNuevo(ss, err);
    if (cmd == "animcurva")       return CmdAnimCurva(ss, err);
    if (cmd == "clipinfo")        return CmdClipInfo(ss, err);
    if (cmd == "clipmenu")        return CmdClipMenu(ss, err);
    if (cmd == "clipnombre")      return CmdClipNombre(ss, err);
    if (cmd == "jerlib")          return CmdJerLib(ss, err);
    if (cmd == "jerinfo")         return CmdJerInfo(ss, err);
    if (cmd == "jerruta")         return CmdJerRuta(ss, err);
    if (cmd == "jernodo")         return CmdJerNodo(ss, err);
    if (cmd == "jerplay")         return CmdJerPlay(ss, err);
    if (cmd == "jertick")         return CmdJerTick(ss, err);
    if (cmd == "jervel")          return CmdJerVel(ss, err);
    if (cmd == "jerretarget")     return CmdJerRetarget(ss, err);
    if (cmd == "jerframe")        return CmdJerFrame(ss, err);
    if (cmd == "jerclipretarget") return CmdJerClipRetarget(ss, err);
    if (cmd == "jerreset")        { W3dAnimObjetosReset(); printf("      [jerreset]\n"); return true; }
    if (cmd == "jermem")          return CmdJerMem(ss, err);
    if (cmd == "rotat")           return CmdRotAt(ss, err);
    if (cmd == "animrango")       return CmdAnimRango(ss, err);
    if (cmd == "rigretarget")     return CmdRigRetarget(ss, err);
    if (cmd == "jerlibmenu")      return CmdJerLibMenu(ss, err);
    if (cmd == "objretarget")     return CmdObjRetarget(ss, err);
    if (cmd == "clipretargetmenu") return CmdClipRetargetMenu(ss, err);
    if (cmd == "animmenos")       return CmdAnimMenos(ss, err);
    if (cmd == "animlista")       return CmdAnimLista(ss, err);
    if (cmd == "jercapa")         return CmdJerCapa(ss, err);
    if (cmd == "jermix")          return CmdJerMix(ss, err);
    if (cmd == "mixjer")          return CmdMixJer(ss, err);
    if (cmd == "mixfilas")        return CmdMixFilas(ss, err);
    if (cmd == "rigcapa")         return CmdRigCapa(ss, err);
    if (cmd == "rigbiped")        return CmdRigBiped(ss, err);
    if (cmd == "rigskinglb")      return CmdRigSkinGlb(ss, err);
    if (cmd == "importclipsalto") return CmdImportClipsAlto(ss, err);
    if (cmd == "huesopose")       return CmdHuesoPose(ss, err);
    if (cmd == "juegocinemin")    return CmdJuegoCineMin(ss, err);
    if (cmd == "juegocinelog")    return CmdJuegoCineLog(ss, err);
    if (cmd == "juegoescenasmin") return CmdJuegoEscenasMin(ss, err);
    if (cmd == "juegoescenaslog") return CmdJuegoEscenasLog(ss, err);
    manejado = false;
    return false;
}
