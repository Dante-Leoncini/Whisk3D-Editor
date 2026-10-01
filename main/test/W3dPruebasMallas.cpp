// ============================================================================
//  W3dPruebasMallas.cpp — comandos de harness de las MALLAS 3D COMO RECURSO
//  (libs/Whisk3DCore/objects/MallaRecurso.h + main/io/MallasProyecto.h).
//  Los despacha W3dPruebasRecursosCmd (test/W3dPruebasRecursos.cpp) ANTES que los
//  suyos: viven aparte para no pisarse con los comandos de otras areas.
//
//  Comandos:
//    mallainfo <objeto|@ultimo> [recurso N|-] [usuarios N] [vbo compartido|pose|propio]
//              [edicion pendiente|cargada] [comparte <otro>] [nocomparte <otro>] [material <parte> <nombre>]
//              [grupos N] [caras N] [flipbook si|no] [carpeta <ruta|->]
//        la malla del objeto: su recurso (- = suelta), cuantos objetos de la escena lo usan,
//        de donde dibuja (compartido = los VBO del recurso; pose = comparte color/uv/indices
//        y sube solo su pose skinneada; propio = sus VBO), si su edicion esta en memoria, y
//        si su geometria es LA MISMA memoria que la de otro objeto (mismos punteros).
//    mallasregistro [n N] [cargados N] [vbo N] [tiene NOMBRE] [notiene NOMBRE]
//        el registro de mallas del proyecto: cuantas, cuantas con los arrays en memoria,
//        cuantas con VBO compartido subido.
//    mallarender
//        dibuja un frame del layout (sube los VBO perezosos) y drena la GPU.
//    mallaunica <objeto>         "Make Single User" de la tarjeta (con undo)
//    mallanueva <objeto>         "New Copy" del selector (con undo)
//    mallaasignar <objeto> <recurso>   el selector de la tarjeta (con undo)
//    mallarenombrar <objeto> <nombre|->  renombra el RECURSO del objeto POR LA TARJETA "Malla 3D"
//        del primer panel de Properties (el boton se vuelve input, se escribe y se acepta: el
//        mismo RenameCommit de la UI, con undo y uniquificado). '-' = el texto vacio. Una malla
//        suelta pasa a tener su recurso recien al aceptar (Ctrl+Z la vuelve suelta).
//    malladup <objeto> vinculado|copia
//        Alt+D (duplicado vinculado: mismo recurso) o Shift+D (copia con recurso propio) por la
//        MISMA puerta que el teclado; el objeto nuevo queda como @ultimo.
//    mallavert <objeto> <i> [guardar clave] [delta clave dx dy dz [tol]]
//        el render-vert i del objeto; 'guardar' anota su posicion; 'delta' exige que se haya
//        movido (dx,dy,dz) desde la anotada.
//    mallamaterial <objeto> <parte> <material>   el material de una parte (por la tarjeta)
//    mallaadquirir <entrada> [bloqueante|async]
//        una referencia TRANSITORIA sobre una malla del registro por el almacen (lo que pide una
//        ListaCarga "malla:"): el op Cargar del tipo lee la entrada .w3db. 'purgar' la suelta.
//    mallaskindos armar|verificar
//        dos personajes con la MISMA malla (recurso) y DOS esqueletos con poses distintas: la
//        geometria es la misma memoria, cada uno se deforma con su pose, y cada pose es
//        IDENTICA (bit a bit) a la de una copia suelta skinneada con el mismo esqueleto.
//    meminfo ... recursosmalla N | vbocompartidos N
//        agrega la linea de las mallas como recurso a meminfo (y sus dos asserts); el resto
//        de meminfo sigue siendo el de siempre.
//    mallacarpeta <objeto> <ruta|-|__>  la CARPETA del recurso del objeto, por la tarjeta como
//        mallarenombrar (con undo, normalizada; '-' = el texto vacio, '__' = solo espacios:
//        los dos dejan la malla sin carpeta)
//    mallasvbo [propios N] [delrecurso N] [dibujadas N]
//        los VBO de CADA malla de un recurso en la escena (despues de un 'mallarender'): cuantas
//        tienen algun VBO PROPIO (vboPos/Nor/Col/UV/Idx != 0: una subida por objeto), cuantas
//        dibujaron TODAS sus capas con los VBO del recurso (vboCompartidas = las capas que el
//        recurso tiene) y cuantas se dibujaron (vboCompartidas != 0). Mil arboles = un juego de
//        VBO: propios 0.
//    objcaras <ruta.obj> <N>
//        el .obj exportado tiene N lineas de cara ('f '): el export no pierde las caras de las
//        mallas con la edicion pendiente en su recurso.
//    mallarefs <entrada> <+N|-N>
//        suma (o suelta) N referencias PERMANENTES al descriptor de una malla del almacen: los
//        N objetos que la usan, sin armar N objetos (probar mas de 32767 usuarios).
//    w3dsinentrada <origen.w3d> <destino.w3d> <entrada>
//        copia el contenedor SIN esa entrada (un proyecto editado a mano o una entrada perdida).
//    matrenombrar <material> <nombre>
//        renombra un material del proyecto (lo que hace su rename en la tarjeta), aunque ningun
//        objeto de la escena lo use (el de una malla huerfana).
//    mallaspurgar [n N]               purga los recursos de la sesion sin usuarios ni undo
//    mallaslegado on|off              el guardado escribe las mallas como antes (proyecto viejo)
//    mallaflip <objeto> <cuadros> <desfase>
//        le pone a la malla una animacion UV propia (tira de 'cuadros') y aplica el cuadro
//        'desfase' YA (uv[] = base + ese cuadro): una malla POSADA por su flipbook.
//    mallauv <objeto> <i> [guardar clave] [delta clave du dv [tol]]   como mallavert, con uv[]
//    mallava <objeto> <cuadro>
//        le pone a la malla una vertex anim propia (cuadro 1 = su reposo, cuadro 10 = el doble)
//        y la POSA en 'cuadro' con su controlador de reproduccion: una malla posada por su anim.
//    mallamirar <objeto> [aristas N] [caras N]
//        MIRAR una malla con la edicion PENDIENTE sin materializarla: la topologia compartida
//        del recurso (aristas/caras, vs una copia materializada), el snap a su arista y a su
//        cara, la seleccion por caja sobre su cara, y el WIREFRAME: la vista entera dibujada con
//        la edicion pendiente es IDENTICA (pixel a pixel) a la misma vista con todo materializado.
// ============================================================================
#include "test/W3dPruebasRecursos.h"
#include "objects/Objects.h"
#include "objects/Mesh.h"
#include "objects/MallaRecurso.h"
#include "objects/Armature.h"
#include "objects/Materials.h"
#include "objects/Collection.h"
#include "objects/ObjectMode.h"        // DuplicatedObject / NewInstance / W3dDuplicarUno
#include "io/MallasProyecto.h"
#include "io/RecursosProyecto.h"      // renombrar / mover la malla 3D: la biblioteca
#include "io/W3dRecursos.h"
#include "io/W3dMallaBin.h"
#include "edit/MeshEdit.h"
#include "edit/Modifier.h"
#include "edit/WeightPaint.h"          // WeightPaintAsegurarMapa
#include "animation/SkeletalAnimation.h" // EvaluarPoseEsqueleto / SkinearMesh / PrepararSkinAutorado
#include "animation/Animation.h"   // CurrentFrame
#include "undo/Undo.h"
#include "base/W3dInteractionState.h"
#include "gfx/w3dGraphics.h"
#include "ViewPorts/ViewPorts.h"       // rootViewport
#include "ViewPorts/ViewPort3D.h"      // Viewport3D (proyectar al viewport)
#include "ViewPorts/LayoutInput.h"     // SnapBuscarTarget / g_snap
#include "ViewPorts/Pick3D.h"          // BoxSelectAplicar3D
#include "WhiskUI/draw/glesdraw.h"     // W3dPantallaAlto (leer el framebuffer)
#include "ViewPorts/Properties.h"      // la tarjeta "Malla 3D" (rename / carpeta por la UI)
#include "WhiskUI/widgets/TextField.h" // g_textFieldActivo: el input del rename in-place
#include "io/GuardarW3D.h"             // g_w3dMallasLegado
#include "io/W3dZip.h"                 // w3dsinentrada: copiar el contenedor sin una entrada
#include <map>
#include <stdio.h>
#include <string.h>
#include <math.h>

extern void ReiniciarEscena();
extern void RebindMaterialMeshPart();
extern Viewport3D* Viewport3DActive;
extern bool RenameActivo();   // Properties.cpp: el rename in-place de la tarjeta
extern void RenameCommit();
extern void RenameCancel();

namespace {

Object* gUltimo = NULL;   // el ultimo objeto que creo malladup (@ultimo)
std::map<std::string, Vector3> gVertsGuardados;

Object* BuscarObj(Object* o, const std::string& n) {
    if (!o) return NULL;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        if (!h) continue;
        if (h->name == n) return h;
        Object* r = BuscarObj(h, n);
        if (r) return r;
    }
    return NULL;
}
Mesh* MallaDe(const std::string& n, std::string& err, const char* cmd) {
    Object* o = (n == "@ultimo") ? gUltimo : BuscarObj(SceneCollection, n);
    if (!o || o->getType() != ObjectType::mesh) { err = std::string(cmd) + ": no hay una malla '" + n + "'"; return NULL; }
    return (Mesh*)o;
}
std::string Nombre(const MallaRecurso* r) { return r ? r->nombre : std::string("-"); }

// de donde dibuja la malla (la misma decision que Mesh::RenderObject)
std::string ModoVBO(const Mesh* m) {
    if (!m->malla || !m->datosComp) return "propio";
    if (m->skinArmature) return (m->datosComp & (W3DMD_UV | W3DMD_COL | W3DMD_FACES)) ? "pose" : "propio";
    return (m->datosComp & W3DMD_POS) ? "compartido" : "propio";
}

bool CmdMallaInfo(std::istringstream& ss, std::string& err) {
    W3dMallasAsegurarRecursos();   // (como la UI al listar: la malla que nacio sin cuadro de por medio ya tiene la suya)
    std::string n; ss >> n;
    Mesh* m = MallaDe(n, err, "mallainfo");
    if (!m) return false;
    const int us = m->malla ? W3dMallaRecursoUsuariosEnEscena(m->malla) : 1;
    printf("      [mallainfo] '%s': recurso '%s' (entrada '%s') usuarios=%d (vinculados=%d) vbo=%s edicion=%s "
           "comparte=0x%02x rverts=%d tris=%d caras=%d\n",
           m->name.c_str(), Nombre(m->malla).c_str(), m->malla ? m->malla->entrada.c_str() : "",
           us, m->malla ? (int)m->malla->usuarios.size() : 0, ModoVBO(m).c_str(),
           m->edicionPendiente.empty() ? "cargada" : "pendiente", (unsigned)m->datosComp,
           m->vertexSize, m->facesSize / 3, (int)m->faces3d.size());
    std::string k;
    while (ss >> k) {
        std::string v;
        if (!(ss >> v)) { err = "mallainfo: falta el valor de '" + k + "'"; return false; }
        if (k == "recurso") { if (Nombre(m->malla) != v) { err = "mallainfo: recurso '" + Nombre(m->malla) + "', se esperaba '" + v + "'"; return false; } }
        else if (k == "usuarios") {
            if (us != atoi(v.c_str())) { char b[96]; snprintf(b, sizeof(b), "mallainfo: %d usuarios, se esperaban %s", us, v.c_str()); err = b; return false; }
        }
        else if (k == "vbo") { if (ModoVBO(m) != v) { err = "mallainfo: vbo " + ModoVBO(m) + ", se esperaba " + v; return false; } }
        else if (k == "edicion") {
            const std::string e = m->edicionPendiente.empty() ? "cargada" : "pendiente";
            if (e != v) { err = "mallainfo: edicion " + e + ", se esperaba " + v; return false; }
        }
        else if (k == "material") {   // material <parte> <nombre>: el material de esa parte
            std::string mn; ss >> mn;
            const int p = atoi(v.c_str());
            if (p < 0 || p >= (int)m->materialsGroup.size()) { err = "mallainfo: parte fuera de rango"; return false; }
            const Material* mat = m->materialsGroup[(size_t)p].material;
            const std::string real = mat ? mat->name : std::string("-");
            if (real != mn) { err = "mallainfo: la parte " + v + " tiene el material '" + real + "', se esperaba '" + mn + "'"; return false; }
        }
        else if (k == "caras") {    // cuantas caras LOGICAS tiene (las suyas o, pendiente, las del recurso)
            const std::vector<MeshFace>* F = W3dMallaCarasVista(m);
            const int nc = F ? (int)F->size() : 0;
            if (nc != atoi(v.c_str())) {
                char b[96]; snprintf(b, sizeof(b), "mallainfo: %d caras, se esperaban %s", nc, v.c_str());
                err = b; return false;
            }
        }
        else if (k == "grupos") {   // cuantos vertex groups tiene la malla (los extras por instancia)
            if ((int)m->vertexGroups.size() != atoi(v.c_str())) {
                char b[96]; snprintf(b, sizeof(b), "mallainfo: %d vertex groups, se esperaban %s", (int)m->vertexGroups.size(), v.c_str());
                err = b; return false;
            }
        }
        else if (k == "flipbook") {   // si|no: la malla anima sus UV
            const bool tiene = m->flipbook != NULL;
            if (tiene != (v == "si")) { err = std::string("mallainfo: flipbook ") + (tiene ? "si" : "no") + ", se esperaba " + v; return false; }
        }
        else if (k == "carpeta") {   // la carpeta del recurso ('-' = sin carpeta)
            const std::string c = m->malla ? m->malla->carpeta : std::string();
            if ((c.empty() ? std::string("-") : c) != v) { err = "mallainfo: carpeta '" + c + "', se esperaba '" + v + "'"; return false; }
        }
        else if (k == "comparte" || k == "nocomparte") {
            Mesh* o = MallaDe(v, err, "mallainfo");
            if (!o) return false;
            const bool misma = m->vertex && m->vertex == o->vertex && m->faces == o->faces;
            if (misma != (k == "comparte")) {
                err = "mallainfo: '" + m->name + "' y '" + o->name + (misma ? "' SI" : "' NO") + " comparten la geometria";
                return false;
            }
        }
        else { err = "mallainfo: assert desconocido '" + k + "'"; return false; }
    }
    return true;
}

bool CmdMallasRegistro(std::istringstream& ss, std::string& err) {
    W3dMallasAsegurarRecursos();
    const std::vector<MallaRecurso*>& reg = W3dMallasRegistro();
    for (size_t i = 0; i < reg.size(); i++) {
        const MallaRecurso* r = reg[i];
        printf("      [mallasregistro] '%s' entrada='%s' carpeta='%s' usuarios=%d vinculados=%d %s%s%s\n",
               r->nombre.c_str(), r->entrada.c_str(), r->carpeta.c_str(), W3dMallaRecursoUsuariosEnEscena(r),
               (int)r->usuarios.size(), r->Cargada() ? "cargada" : "sin cargar",
               r->vboPos ? " vbo" : "", r->modificado ? " modificada" : "");
    }
    const int n = (int)reg.size(), cargados = W3dMallasRecursosCargados(), vbo = W3dMallasVBOCompartidos();
    int conExtras = 0;   // recursos que todavia guardan los extras (grupos/formas/ctrl) ademas de sus usuarios
    for (size_t i = 0; i < reg.size(); i++) if (reg[i]->extrasEnRecurso) conExtras++;
    printf("      [mallasregistro] %d malla(s), %d cargada(s), %d con VBO compartido (%ld B de GPU), %d con extras propios\n",
           n, cargados, vbo, W3dMallasVBOBytes(), conExtras);
    std::string k;
    while (ss >> k) {
        std::string v;
        if (!(ss >> v)) { err = "mallasregistro: falta el valor de '" + k + "'"; return false; }
        char b[128];
        if (k == "n" && n != atoi(v.c_str())) { snprintf(b, sizeof(b), "mallasregistro: %d mallas, se esperaban %s", n, v.c_str()); err = b; return false; }
        if (k == "cargados" && cargados != atoi(v.c_str())) { snprintf(b, sizeof(b), "mallasregistro: %d cargadas, se esperaban %s", cargados, v.c_str()); err = b; return false; }
        if (k == "vbo" && vbo != atoi(v.c_str())) { snprintf(b, sizeof(b), "mallasregistro: %d con VBO, se esperaban %s", vbo, v.c_str()); err = b; return false; }
        if (k == "tiene" && !W3dMallaRecursoPorNombre(v)) { err = "mallasregistro: no esta la malla '" + v + "'"; return false; }
        if (k == "notiene" && W3dMallaRecursoPorNombre(v)) { err = "mallasregistro: esta la malla '" + v + "'"; return false; }
        if (k == "extras" && conExtras != atoi(v.c_str())) { snprintf(b, sizeof(b), "mallasregistro: %d con extras propios, se esperaban %s", conExtras, v.c_str()); err = b; return false; }
        if (k == "carpeta") {   // carpeta <malla> <ruta|->
            std::string ruta; ss >> ruta;
            const MallaRecurso* r = W3dMallaRecursoPorNombre(v);
            if (!r) { err = "mallasregistro: no esta la malla '" + v + "'"; return false; }
            if ((r->carpeta.empty() ? std::string("-") : r->carpeta) != ruta) {
                err = "mallasregistro: la malla '" + v + "' tiene la carpeta '" + r->carpeta + "', se esperaba '" + ruta + "'";
                return false;
            }
        }
        if (k != "n" && k != "cargados" && k != "vbo" && k != "tiene" && k != "notiene" && k != "extras" && k != "carpeta") {
            err = "mallasregistro: assert desconocido '" + k + "'"; return false;
        }
    }
    return true;
}

bool CmdMallaRender(std::string& err) {
    if (!rootViewport) { err = "mallarender: no hay layout"; return false; }
    rootViewport->Render();
    w3dEngine::Finish();
    return true;
}

// deja el editor NAVEGANDO en Modo Objeto con 'o' como unico seleccionado (lo que exigen
// Shift+D / Alt+D / la tarjeta)
void SoloSeleccionado(Object* o) {
    InteractionMode = ObjectMode;
    estado = editNavegacion;
    DeseleccionarTodo();
    o->Seleccionar();
    ObjActivo = o;
}

bool CmdMallaUnica(std::istringstream& ss, std::string& err, bool nueva) {
    std::string n; ss >> n;
    Mesh* m = MallaDe(n, err, nueva ? "mallanueva" : "mallaunica");
    if (!m) return false;
    SoloSeleccionado(m);
    MallaRecurso* r = nueva ? W3dMallaNuevaCopia(m) : W3dMallaHacerUnica(m);
    printf("      [%s] '%s' -> '%s'\n", nueva ? "mallanueva" : "mallaunica", m->name.c_str(), Nombre(r).c_str());
    if (!r) { err = "no se pudo"; return false; }
    return true;
}

bool CmdMallaAsignar(std::istringstream& ss, std::string& err) {
    std::string n, rn; ss >> n >> rn;
    Mesh* m = MallaDe(n, err, "mallaasignar");
    if (!m) return false;
    MallaRecurso* r = W3dMallaRecursoPorNombre(rn);
    if (!r) { err = "mallaasignar: no hay una malla '" + rn + "' en el registro"; return false; }
    SoloSeleccionado(m);
    if (!W3dMallaAsignar(m, r)) { err = "mallaasignar: no se pudo"; return false; }
    return true;
}

// la PRIMERA hoja Properties del layout (la tarjeta "Malla 3D" vive ahi)
void HojasDe(ViewportBase* n, int kind, ViewportBase*& out) {
    if (!n || out) return;
    if (n->isLeaf()) { if (n->ViewportKind() == kind) out = n; return; }
    if (n->ContainerKind() == 1) { HojasDe(((ViewportRow*)n)->childA, kind, out); HojasDe(((ViewportRow*)n)->childB, kind, out); }
    else { HojasDe(((ViewportColumn*)n)->childA, kind, out); HojasDe(((ViewportColumn*)n)->childB, kind, out); }
}
// EL RENAME / LA CARPETA de la malla 3D de un objeto: hoy se hacen en la BIBLIOTECA (el outliner), no en la
// tarjeta "Malla 3D" (que solo elige que malla usa el objeto). La MISMA puerta que el renombrar en linea y el
// "Move to Folder": W3dVistaRecRenombrar / W3dVistaRecMover, con sus pasos de undo. 'texto' ya viene traducido.
bool RenamePorTarjeta(Mesh* m, bool carpeta, const std::string& texto, std::string& err, const char* cmd) {
    W3dMallasAsegurarRecursos();   // TODA malla es un recurso (la que nacio en este cuadro tambien)
    if (!m->malla) { err = std::string(cmd) + ": la malla no tiene su malla 3D"; return false; }
    SoloSeleccionado(m);
    const std::string id = m->malla->nombre;
    const bool ok = carpeta ? W3dVistaRecMover(W3D_VISTA_MALLAS, id, W3dCarpetaNormalizar(texto))
                            : W3dVistaRecRenombrar(W3D_VISTA_MALLAS, id, texto, NULL);
    if (!ok) { err = std::string(cmd) + ": la biblioteca no pudo"; return false; }
    return true;
}

bool CmdMallaRenombrar(std::istringstream& ss, std::string& err) {
    std::string n, nuevo; ss >> n >> nuevo;
    Mesh* m = MallaDe(n, err, "mallarenombrar");
    if (!m) return false;
    if (!RenamePorTarjeta(m, false, nuevo == "-" ? std::string() : nuevo, err, "mallarenombrar")) return false;
    printf("      [mallarenombrar] '%s' -> '%s'\n", n.c_str(), m->malla ? m->malla->nombre.c_str() : "(suelta)");
    return true;
}

bool CmdMallaDup(std::istringstream& ss, std::string& err) {
    std::string n, modo; ss >> n >> modo;
    Mesh* m = MallaDe(n, err, "malladup");
    if (!m) return false;
    if (modo != "vinculado" && modo != "copia") { err = "malladup: uso: malladup <objeto> vinculado|copia"; return false; }
    SoloSeleccionado(m);
    if (modo == "vinculado") NewInstance(); else DuplicatedObject();
    gUltimo = ObjActivo;
    estado = editNavegacion;   // los dos dejan el modal de MOVER abierto (como el teclado)
    if (!gUltimo || gUltimo == m) { err = "malladup: no se creo nada"; return false; }
    printf("      [malladup] '%s' %s -> '%s'\n", m->name.c_str(), modo.c_str(), gUltimo->name.c_str());
    return true;
}

bool CmdMallaVert(std::istringstream& ss, std::string& err) {
    std::string n; int i = -1; ss >> n >> i;
    Mesh* m = MallaDe(n, err, "mallavert");
    if (!m) return false;
    if (!m->vertex || i < 0 || i >= m->vertexSize) { err = "mallavert: indice fuera de rango"; return false; }
    const Vector3 p(m->vertex[i*3], m->vertex[i*3+1], m->vertex[i*3+2]);
    printf("      [mallavert] '%s'[%d] = (%.5f, %.5f, %.5f)\n", m->name.c_str(), i, p.x, p.y, p.z);
    std::string k;
    while (ss >> k) {
        if (k == "guardar") { std::string c; ss >> c; gVertsGuardados[c] = p; continue; }
        if (k == "delta") {
            std::string c; float dx = 0, dy = 0, dz = 0, tol = 1e-4f;
            ss >> c >> dx >> dy >> dz;
            { std::streampos pos = ss.tellg(); float t; if (ss >> t) tol = t; else { ss.clear(); ss.seekg(pos); } }
            if (!gVertsGuardados.count(c)) { err = "mallavert: no hay una posicion guardada '" + c + "'"; return false; }
            const Vector3 o = gVertsGuardados[c];
            if (fabsf(p.x - o.x - dx) > tol || fabsf(p.y - o.y - dy) > tol || fabsf(p.z - o.z - dz) > tol) {
                char b[200];
                snprintf(b, sizeof(b), "mallavert: '%s'[%d] se movio (%.4f,%.4f,%.4f), se esperaba (%.4f,%.4f,%.4f)",
                         m->name.c_str(), i, p.x - o.x, p.y - o.y, p.z - o.z, dx, dy, dz);
                err = b; return false;
            }
            continue;
        }
        err = "mallavert: argumento desconocido '" + k + "'"; return false;
    }
    return true;
}

bool CmdMallaMaterial(std::istringstream& ss, std::string& err) {
    std::string n, mn; int k = -1; ss >> n >> k >> mn;
    Mesh* m = MallaDe(n, err, "mallamaterial");
    if (!m) return false;
    if (k < 0 || k >= (int)m->materialsGroup.size()) { err = "mallamaterial: parte fuera de rango"; return false; }
    Material* mat = BuscarMaterialPorNombre(mn);
    if (!mat) mat = new Material(mn);
    SoloSeleccionado(m);
    UndoCapturarMaterial(m, k);
    m->materialsGroup[(size_t)k].material = mat;
    RebindMaterialMeshPart();   // la puerta de la tarjeta Material
    return true;
}

// ---------------------------------------------------------------------------
//  mallaadquirir <entrada> bloqueante|async: una referencia TRANSITORIA sobre una malla del
//  registro por el almacen de recursos, como la pide una ListaCarga "malla:" (el op Cargar lee
//  la entrada). Con async queda EN_VUELO hasta el 'pump'; 'purgar' la suelta.
// ---------------------------------------------------------------------------
bool CmdMallaAdquirir(std::istringstream& ss, std::string& err) {
    std::string id, modo; ss >> id >> modo;
    if (id.empty()) { err = "mallaadquirir: uso: mallaadquirir <entrada> [bloqueante|async]"; return false; }
    W3dMallasAsegurarOps();
    W3dRecurso* r = W3dRecursoAdquirir(W3DREC_MALLA, id, W3DREC_TRANSITORIA,
                                       modo == "async" ? W3DREC_ASYNC : W3DREC_BLOQUEANTE, "harness");
    if (!r) { err = "mallaadquirir: el almacen no devolvio descriptor"; return false; }
    static const char* kEst[] = { "NO_CARGADO", "EN_VUELO", "FIXUP", "LISTO", "FALLO" };
    printf("      [mallaadquirir] %s -> %s\n", id.c_str(), kEst[r->estado]);
    return true;
}

// ---------------------------------------------------------------------------
//  mallaskindos: skinning POR INSTANCIA con un recurso compartido
// ---------------------------------------------------------------------------
Armature* ArmarRig(Object* padre, const char* nombre, const Vector3& pos) {
    Armature* a = new Armature(padre, pos);
    a->name = nombre;
    { W3dBone r; r.name = "raiz";  r.parent = -1; r.head = Vector3(0,0,0); r.tail = Vector3(0,1,0); a->bones.push_back(r); }
    { W3dBone c; c.name = "brazo"; c.parent = 0;  c.head = Vector3(0,1,0); c.tail = Vector3(0,2,0); a->bones.push_back(c); }
    PrepararSkinAutorado(a);
    return a;
}
void Rigguear(Mesh* s, Armature* a) {
    for (size_t k = 0; k < s->modificadores.size(); k++)
        if (s->modificadores[k] && s->modificadores[k]->tipo == ModifierType::Armature) s->modificadores[k]->target = (Object*)a;
    s->skinArmature = a;
    s->lastSkinFrame = -999999;
}
Mesh* SkinDe(const char* nombre) {
    Object* o = BuscarObj(SceneCollection, nombre);
    return (o && o->getType() == ObjectType::mesh) ? (Mesh*)o : NULL;
}
Armature* RigDe(const char* nombre) {
    Object* o = BuscarObj(SceneCollection, nombre);
    return (o && o->getType() == ObjectType::armature) ? (Armature*)o : NULL;
}
void Posar(Armature* a, float grados) {
    // primero la pose del frame actual (un cambio de frame la refresca desde la curva/rest y
    // pisaria lo posado); despues se posa A MANO (poseDirty), como en Pose Mode
    EvaluarPoseEsqueleto(a, CurrentFrame);
    a->bones[1].poseR = Vector3(0, 0, grados);
    a->poseDirty = true;
    EvaluarPoseEsqueleto(a, CurrentFrame);
}
bool MismaPose(Mesh* a, Mesh* b) {
    if (!a->skinVertex || !b->skinVertex || a->vertexSize != b->vertexSize) return false;
    return memcmp(a->skinVertex, b->skinVertex, (size_t)a->vertexSize * 3 * sizeof(GLfloat)) == 0;
}

bool CmdMallaSkinDos(std::istringstream& ss, std::string& err) {
    std::string op; ss >> op;
    if (op == "armar") {
        ReiniciarEscena();
        Collection* col = new Collection(SceneCollection);
        col->SetNameObj("Dos");
        CollectionActive = col;
        Armature* a = ArmarRig(col, "RigA", Vector3(0, 0, 0));
        Armature* b = ArmarRig(col, "RigB", Vector3(3, 0, 0));
        Mesh* s = (Mesh*)NewMesh(MeshType::cylinder, col, false);
        s->SetNameObj("PersonajeA");
        s->GenerarRender();
        WeightPaintAsegurarMapa(s);
        VertexGroup* vr = new VertexGroup("raiz"); VertexGroup* vb = new VertexGroup("brazo");
        std::vector<char> hecho(s->vertexSize, 0);
        for (int i = 0; i < s->vertexSize; i++) {
            const int cp = s->vertCtrlPoint[i];
            if (hecho[cp]) continue;
            hecho[cp] = 1;
            if (s->vertex[i*3+1] > 0.0f) { vb->verts.push_back(cp); vb->pesos.push_back(1.0f); }
            else { vr->verts.push_back(cp); vr->pesos.push_back(0.7f); }
        }
        s->vertexGroups.push_back(vr); s->vertexGroups.push_back(vb);
        Modifier* md = new Modifier(ModifierType::Armature, NombreTipoModificador(ModifierType::Armature));
        md->target = (Object*)a; s->modificadores.push_back(md);
        s->skinArmature = a;
        // la malla pasa a ser un RECURSO y el segundo personaje es su duplicado VINCULADO (Alt+D)
        if (!W3dMallaCrearRecurso(s, "Personaje")) { err = "mallaskindos: no se pudo crear el recurso"; return false; }
        SoloSeleccionado(s);
        NewInstance();
        estado = editNavegacion;
        Mesh* s2 = (ObjActivo && ObjActivo != s && ObjActivo->getType() == ObjectType::mesh) ? (Mesh*)ObjActivo : NULL;
        if (!s2) { err = "mallaskindos: Alt+D no creo la malla"; return false; }
        s2->SetNameObj("PersonajeB");
        s2->pos = Vector3(3, 0, 0);
        Rigguear(s2, b);
        DeseleccionarTodo(); ObjActivo = NULL;
        printf("      [mallaskindos] 'PersonajeA' (RigA) y 'PersonajeB' (RigB) usan la malla '%s'\n", Nombre(s->malla).c_str());
        return true;
    }
    if (op != "verificar") { err = "mallaskindos: uso: mallaskindos armar|verificar"; return false; }
    Mesh* A = SkinDe("PersonajeA"); Mesh* B = SkinDe("PersonajeB");
    Armature* ra = RigDe("RigA"); Armature* rb = RigDe("RigB");
    if (!A || !B || !ra || !rb) { err = "mallaskindos: falta PersonajeA/PersonajeB/RigA/RigB (armar primero)"; return false; }
    if (!A->malla || A->malla != B->malla) { err = "mallaskindos: los dos personajes no usan el MISMO recurso"; return false; }
    if (A->vertex != B->vertex) { err = "mallaskindos: la geometria de reposo no es la misma memoria"; return false; }
    // dos poses DISTINTAS
    Posar(ra, 35.0f);
    Posar(rb, -50.0f);
    A->lastSkinFrame = -999999; B->lastSkinFrame = -999999;
    SkinearMesh(A); SkinearMesh(B);
    if (!A->skinVertex || !B->skinVertex) { err = "mallaskindos: el skinning no produjo pose"; return false; }
    if (MismaPose(A, B)) { err = "mallaskindos: los dos personajes quedaron con la MISMA pose"; return false; }
    // la referencia: copias SUELTAS (arrays propios) skinneadas con cada esqueleto
    Mesh* ca = (Mesh*)W3dDuplicarUno(A); Mesh* cb = (Mesh*)W3dDuplicarUno(B);
    if (!ca || !cb || ca->malla || cb->malla) { err = "mallaskindos: la copia de referencia no es suelta"; return false; }
    Rigguear(ca, ra); Rigguear(cb, rb);
    SkinearMesh(ca); SkinearMesh(cb);
    const bool okA = MismaPose(A, ca), okB = MismaPose(B, cb);
    // la geometria del recurso NO se toco (el skinning escribe skinVertex, que es de cada uno)
    const bool reposo = (A->vertex == A->malla->vertex);
    printf("      [mallaskindos] pose A == copia suelta A: %s | pose B == copia suelta B: %s | A != B | reposo compartido: %s\n",
           okA ? "SI" : "NO", okB ? "SI" : "NO", reposo ? "SI" : "NO");
    // las copias de referencia se borran (no quedan en la escena). Primero se suelta la
    // seleccion: el constructor de cada copia la dejo seleccionada y activa
    DeseleccionarTodo(); ObjActivo = NULL;
    Object* refs[2] = { ca, cb };
    for (int k = 0; k < 2; k++) {
        Object* p = refs[k]->Parent ? refs[k]->Parent : SceneCollection;
        for (size_t i = 0; i < p->Childrens.size(); i++) if (p->Childrens[i] == refs[k]) { p->Childrens.erase(p->Childrens.begin() + (long)i); break; }
        W3dLiberarSubarbol(refs[k]);
    }
    DeseleccionarTodo(); ObjActivo = NULL;
    Posar(ra, 0.0f); Posar(rb, 0.0f);
    if (!okA || !okB) { err = "mallaskindos: la pose de un personaje con la malla compartida no es la de su esqueleto"; return false; }
    if (!reposo) { err = "mallaskindos: el skinning desinstancio la geometria de reposo"; return false; }
    return true;
}

// ---------------------------------------------------------------------------
//  mallacarpeta / mallaspurgar / mallaslegado
// ---------------------------------------------------------------------------
bool CmdMallaCarpeta(std::istringstream& ss, std::string& err) {
    std::string n, ruta; ss >> n >> ruta;
    Mesh* m = MallaDe(n, err, "mallacarpeta");
    if (!m) return false;
    const std::string texto = (ruta == "-") ? std::string() : (ruta == "__") ? std::string("   ") : ruta;
    if (!RenamePorTarjeta(m, true, texto, err, "mallacarpeta")) return false;
    printf("      [mallacarpeta] '%s' -> '%s'\n", m->malla ? m->malla->nombre.c_str() : "(suelta)",
           m->malla ? m->malla->carpeta.c_str() : "");
    return true;
}

// ---------------------------------------------------------------------------
//  mallasvbo: los VBO de cada objeto (no solo los del recurso)
// ---------------------------------------------------------------------------
void JuntarMallasEscena(Object* o, std::vector<Mesh*>& out);   // (mas abajo, con mallamirar)
bool CmdMallasVBO(std::istringstream& ss, std::string& err) {
    std::vector<Mesh*> todas;
    JuntarMallasEscena(SceneCollection, todas);
    int propios = 0, delRecurso = 0, dibujadas = 0, deRecurso = 0;
    for (size_t i = 0; i < todas.size(); i++) {
        const Mesh* m = todas[i];
        if (!m->malla) continue;
        deRecurso++;
        if (m->vboPos || m->vboNor || m->vboCol || m->vboUV || m->vboIdx) propios++;
        if (m->vboCompartidas) dibujadas++;
        const MallaRecurso* r = m->malla;
        unsigned mask = 0;
        if (r->vertex)      mask |= W3DMD_POS;
        if (r->normals)     mask |= W3DMD_NOR;
        if (r->uv)          mask |= W3DMD_UV;
        if (r->vertexColor) mask |= W3DMD_COL;
        if (r->faces)       mask |= W3DMD_FACES;
        if (mask && (unsigned)m->vboCompartidas == mask) delRecurso++;
    }
    printf("      [mallasvbo] %d malla(s) de un recurso: %d con VBO propio, %d dibujadas con TODO del recurso, %d dibujadas\n",
           deRecurso, propios, delRecurso, dibujadas);
    std::string k;
    while (ss >> k) {
        int v = -1;
        if (!(ss >> v)) { err = "mallasvbo: falta el valor de '" + k + "'"; return false; }
        const int real = (k == "propios") ? propios : (k == "delrecurso") ? delRecurso : (k == "dibujadas") ? dibujadas : -999;
        if (real == -999) { err = "mallasvbo: assert desconocido '" + k + "'"; return false; }
        if (real != v) { char b[128]; snprintf(b, sizeof(b), "mallasvbo: %s = %d, se esperaba %d", k.c_str(), real, v); err = b; return false; }
    }
    return true;
}
bool CmdMallasPurgar(std::istringstream& ss, std::string& err) {
    const int n = W3dMallasPurgarSinUso();
    printf("      [mallaspurgar] %d recurso(s) purgado(s)\n", n);
    std::string k; int esperado = -1;
    if ((ss >> k) && k == "n" && (ss >> esperado) && esperado != n) {
        char b[96]; snprintf(b, sizeof(b), "mallaspurgar: purgo %d, se esperaban %d", n, esperado); err = b; return false;
    }
    return true;
}
bool CmdMallasLegado(std::istringstream& ss, std::string& err) {
    std::string v; ss >> v;
    if (v != "on" && v != "off") { err = "mallaslegado: uso: mallaslegado on|off"; return false; }
    g_w3dMallasLegado = (v == "on");
    printf("      [mallaslegado] %s\n", g_w3dMallasLegado ? "el guardado escribe una malla por objeto (esquema viejo)" : "registro de mallas");
    return true;
}

// ---------------------------------------------------------------------------
//  mallaflip / mallauv: una malla POSADA por su flipbook (la pose de la sesion)
// ---------------------------------------------------------------------------
bool CmdMallaFlip(std::istringstream& ss, std::string& err) {
    std::string n; int cuadros = 0, desfase = 0; ss >> n >> cuadros >> desfase;
    Mesh* m = MallaDe(n, err, "mallaflip");
    if (!m) return false;
    if (cuadros < 2 || desfase < 1 || desfase >= cuadros) { err = "mallaflip: uso: mallaflip <objeto> <cuadros> <desfase 1..cuadros-1>"; return false; }
    m->SetUVAnimTira(cuadros, 0.0f, 0, desfase);   // fps 0: el cuadro no avanza solo
    m->TickUVAnimTira(0.0f);                       // aplica el cuadro 'desfase' YA
    printf("      [mallaflip] '%s': tira de %d cuadros, en el cuadro %d (comparte=0x%02x)\n",
           m->name.c_str(), cuadros, m->flipAplicado, (unsigned)m->datosComp);
    if (m->flipAplicado != desfase) { err = "mallaflip: el cuadro no se aplico"; return false; }
    return true;
}
bool CmdMallaUV(std::istringstream& ss, std::string& err) {
    std::string n; int i = -1; ss >> n >> i;
    Mesh* m = MallaDe(n, err, "mallauv");
    if (!m) return false;
    if (!m->uv || i < 0 || i >= m->vertexSize) { err = "mallauv: indice fuera de rango"; return false; }
    const Vector3 p(m->uv[i*2], m->uv[i*2+1], 0.0f);
    printf("      [mallauv] '%s'[%d] = (%.5f, %.5f)\n", m->name.c_str(), i, p.x, p.y);
    std::string k;
    while (ss >> k) {
        if (k == "guardar") { std::string c; ss >> c; gVertsGuardados["uv:" + c] = p; continue; }
        if (k == "delta") {
            std::string c; float du = 0, dv = 0, tol = 1e-5f;
            ss >> c >> du >> dv;
            { std::streampos pos = ss.tellg(); float t; if (ss >> t) tol = t; else { ss.clear(); ss.seekg(pos); } }
            if (!gVertsGuardados.count("uv:" + c)) { err = "mallauv: no hay una uv guardada '" + c + "'"; return false; }
            const Vector3 o = gVertsGuardados["uv:" + c];
            if (fabsf(p.x - o.x - du) > tol || fabsf(p.y - o.y - dv) > tol) {
                char b[200];
                snprintf(b, sizeof(b), "mallauv: '%s'[%d] se corrio (%.4f,%.4f), se esperaba (%.4f,%.4f)",
                         m->name.c_str(), i, p.x - o.x, p.y - o.y, du, dv);
                err = b; return false;
            }
            continue;
        }
        err = "mallauv: argumento desconocido '" + k + "'"; return false;
    }
    return true;
}

bool CmdMallaVA(std::istringstream& ss, std::string& err) {
    std::string n; float cuadro = 0.0f; ss >> n >> cuadro;
    Mesh* m = MallaDe(n, err, "mallava");
    if (!m) return false;
    if (!m->vertex || m->vertexSize <= 0 || cuadro <= 1.0f || cuadro > 10.0f) { err = "mallava: uso: mallava <objeto> <cuadro 1..10>"; return false; }
    VertexAnimation* an = new VertexAnimation(m, "Crece");
    an->startFrame = 1; an->endFrame = 10; an->fps = 30;
    const size_t n3 = (size_t)m->vertexSize * 3;
    std::vector<GLfloat> reposo(m->vertex, m->vertex + n3), doble(reposo);
    for (size_t i = 0; i < n3; i++) doble[i] *= 2.0f;
    VertexAnimSetKey(*an, 1, &reposo[0], NULL, NULL, 1);
    VertexAnimSetKey(*an, 10, &doble[0], NULL, NULL, 1);
    NewActiveVertexAnimation(m, an);
    VertexAnimationActive* va = FindTargetAnim(m);
    if (!va) { err = "mallava: la malla no tiene controlador"; return false; }
    va->currentAnim = va->nextAnim = (int)m->animations.size() - 1;
    va->playFrame = cuadro;
    EvalVertexAnim(*an, m, cuadro);
    printf("      [mallava] '%s': anim '%s' posada en el cuadro %.1f (posada=%s, comparte=0x%02x)\n",
           m->name.c_str(), an->name.c_str(), cuadro, m->posadaPorAnim ? "si" : "no", (unsigned)m->datosComp);
    if (!m->posadaPorAnim) { err = "mallava: la anim no poso la malla"; return false; }
    return true;
}

// ---------------------------------------------------------------------------
//  mallamirar: la topologia COMPARTIDA para mirar una malla sin materializar su edicion
// ---------------------------------------------------------------------------
// la vista 3D en un buffer (RGBA, de abajo hacia arriba)
void LeerVista(Viewport3D* vp, std::vector<unsigned char>& buf) {
    rootViewport->Render();
    w3dEngine::Finish();
    const int vpGLY = W3dPantallaAlto - vp->y - vp->height;   // arbol arriba-izq -> GL
    buf.assign((size_t)vp->width * vp->height * 4, 0);
    w3dEngine::ReadPixelsRGBA(vp->x, vpGLY, vp->width, vp->height, &buf[0]);
}
void JuntarMallasEscena(Object* o, std::vector<Mesh*>& out) {
    if (!o) return;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        if (!h) continue;
        if (h->getType() == ObjectType::mesh) out.push_back((Mesh*)h);
        JuntarMallasEscena(h, out);
    }
}

bool CmdMallaMirar(std::istringstream& ss, std::string& err) {
    std::string n; ss >> n;
    Mesh* m = MallaDe(n, err, "mallamirar");
    if (!m) return false;
    if (!rootViewport) { err = "mallamirar: no hay layout"; return false; }
    // nada seleccionado (sin contornos, y el snap de Modo Objeto saltea lo seleccionado)
    InteractionMode = ObjectMode; estado = editNavegacion;
    DeseleccionarTodo(); ObjActivo = NULL;
    if (m->edicionPendiente != W3D_EDICION_DEL_RECURSO) { err = "mallamirar: la malla no tiene la edicion pendiente en su recurso"; return false; }
    const std::vector<int>* A = W3dMallaAristasVista(m);
    const std::vector<MeshFace>* F = W3dMallaCarasVista(m);
    const std::vector<GLfloat>* B = W3dMallaBordesVista(m);
    const int nA = A ? (int)A->size() / 2 : -1, nF = F ? (int)F->size() : -1;
    const bool bordesDelRecurso = B && m->malla && B == &m->malla->bordes;
    printf("      [mallamirar] '%s': aristas=%d caras=%d lineas=%s (edicion %s)\n", m->name.c_str(), nA, nF,
           bordesDelRecurso ? "las del recurso" : (B ? "propias" : "ninguna"),
           m->edicionPendiente.empty() ? "cargada" : "pendiente");
    if (!A || !F) { err = "mallamirar: la malla no tiene topologia para mirar"; return false; }
    if (!bordesDelRecurso) { err = "mallamirar: las lineas del alambre no son las compartidas del recurso"; return false; }
    // ... y son EXACTAMENTE las de la malla materializada (una copia suelta con su edicion)
    {
        Mesh* ref = (Mesh*)W3dDuplicarUno(m);   // materializa la edicion de m... y la copia
        bool igual = ref && ref->edges == *A && ref->faces3d.size() == F->size();
        for (size_t f = 0; igual && f < F->size(); f++) if (ref->faces3d[f].idx != (*F)[f].idx) igual = false;
        DeseleccionarTodo(); ObjActivo = NULL;
        if (ref) {
            Object* p = ref->Parent ? ref->Parent : SceneCollection;
            for (size_t i = 0; i < p->Childrens.size(); i++) if (p->Childrens[i] == ref) { p->Childrens.erase(p->Childrens.begin() + (long)i); break; }
            W3dLiberarSubarbol(ref);
        }
        W3dMallaSoltarEdicion(m);   // (la copia de referencia la habia materializado)
        if (!igual) { err = "mallamirar: la topologia compartida no es la de la malla materializada"; return false; }
        A = W3dMallaAristasVista(m); F = W3dMallaCarasVista(m);
        if (!A || !F || m->edicionPendiente.empty()) { err = "mallamirar: la malla no volvio a quedar pendiente"; return false; }
    }
    std::string k;
    while (ss >> k) {
        int v = 0; ss >> v;
        if (k == "aristas" && nA != v) { char b[96]; snprintf(b, sizeof(b), "mallamirar: %d aristas, se esperaban %d", nA, v); err = b; return false; }
        if (k == "caras" && nF != v)   { char b[96]; snprintf(b, sizeof(b), "mallamirar: %d caras, se esperaban %d", nF, v); err = b; return false; }
    }
    rootViewport->Render();
    Viewport3D* vp = Viewport3DActive;
    if (!vp) { err = "mallamirar: no hay viewport 3D"; return false; }
    vp->BindVista();
    Matrix4 W; m->GetWorldMatrix(W);
    // SNAP a la arista y a la cara: el cursor sobre el centro de la primera de cada una
    const SnapCfg snapAntes = g_snap;
    g_snap.tsActive = g_snap.tsEdited = g_snap.tsNonEdited = true;
    bool snapOk = true;
    for (int pasada = 0; pasada < 2 && snapOk; pasada++) {
        Vector3 c(0, 0, 0);
        if (pasada == 0) {
            const int a = (*A)[0], b = (*A)[1];
            c = (Vector3(m->vertex[a*3], m->vertex[a*3+1], m->vertex[a*3+2]) + Vector3(m->vertex[b*3], m->vertex[b*3+1], m->vertex[b*3+2])) * 0.5f;
        } else {
            const std::vector<int>& id = (*F)[0].idx;
            for (size_t q = 0; q < id.size(); q++) c = c + Vector3(m->vertex[id[q]*3], m->vertex[id[q]*3+1], m->vertex[id[q]*3+2]);
            c = c * (1.0f / (float)id.size());
        }
        const Vector3 wc = W * c;
        float sx = 0, sy = 0;
        if (!vp->ProyectarPunto(wc, sx, sy)) { err = "mallamirar: el punto no cae en la vista"; snapOk = false; break; }
        g_snap.target = (pasada == 0) ? SNAP_EDGECENTER : SNAP_FACECENTER;
        Vector3 hit; float hx = 0, hy = 0;
        const bool enc = SnapBuscarTarget((int)(sx + 0.5f), (int)(sy + 0.5f), vp, hit, hx, hy);
        const Vector3 d = hit - wc;
        const float dist = sqrtf(d.x*d.x + d.y*d.y + d.z*d.z);
        printf("      [mallamirar] snap al centro de %s: %s (a %.5f del esperado)\n", pasada == 0 ? "la arista" : "la cara",
               enc ? "engancho" : "NO engancho", enc ? dist : -1.0f);
        if (!enc || dist > 1e-3f) { err = std::string("mallamirar: el snap no engancho el centro de ") + (pasada == 0 ? "la arista" : "la cara"); snapOk = false; }
    }
    g_snap = snapAntes;
    if (!snapOk) return false;
    // SELECCION POR CAJA (azul, "tocar"): una caja chica adentro de la primera cara
    {
        Vector3 c(0, 0, 0);
        const std::vector<int>& id = (*F)[0].idx;
        for (size_t q = 0; q < id.size(); q++) c = c + Vector3(m->vertex[id[q]*3], m->vertex[id[q]*3+1], m->vertex[id[q]*3+2]);
        c = c * (1.0f / (float)id.size());
        float sx = 0, sy = 0;
        vp->ProyectarPunto(W * c, sx, sy);
        DeseleccionarTodo(); ObjActivo = NULL;
        BoxSelectAplicar3D((int)sx - 1, (int)sy - 1, (int)sx + 1, (int)sy + 1, true, false);
        const bool sel = m->select;
        printf("      [mallamirar] caja sobre la cara: %s\n", sel ? "la selecciono" : "NO la selecciono");
        DeseleccionarTodo(); ObjActivo = NULL;
        if (!sel) { err = "mallamirar: la seleccion por caja no agarro la cara"; return false; }
    }
    // EL ALAMBRE: la vista en wireframe con las ediciones pendientes == con todo materializado
    const RenderType vistaAntes = vp->view;
    vp->view = RenderType::Wireframe;   // el modo de dibujo del viewport (lo aplica cada render)
    std::vector<unsigned char> pend, mat;
    LeerVista(vp, pend);
    std::vector<Mesh*> todas; JuntarMallasEscena(SceneCollection, todas);
    std::vector<Mesh*> prestadas;
    for (size_t i = 0; i < todas.size(); i++)
        if (todas[i]->edicionPendiente == W3D_EDICION_DEL_RECURSO && W3dMallaBinMaterializarEdicion(todas[i]))
            prestadas.push_back(todas[i]);
    LeerVista(vp, mat);
    for (size_t i = 0; i < prestadas.size(); i++) W3dMallaSoltarEdicion(prestadas[i]);
    // ...y la prueba es SENSIBLE: sin la topologia compartida (el camino de antes: el alambre de
    // los triangulos del render) la vista sale distinta (las diagonales de los quads)
    std::vector<unsigned char> tri;
    MallaRecurso* rm = m->malla;
    rm->topoEstado = -1;
    LeerVista(vp, tri);
    rm->topoEstado = 0;   // se vuelve a leer cuando alguien la pida
    vp->view = vistaAntes;
    long distintos = 0, conTri = 0;
    for (size_t i = 0; i + 3 < pend.size(); i += 4) {
        if (pend[i] != mat[i] || pend[i+1] != mat[i+1] || pend[i+2] != mat[i+2]) distintos++;
        if (pend[i] != tri[i] || pend[i+1] != tri[i+1] || pend[i+2] != tri[i+2]) conTri++;
    }
    printf("      [mallamirar] alambre: %ld px distintos entre pendiente y materializado (%d mallas materializadas); "
           "con triangulos serian %ld px distintos\n", distintos, (int)prestadas.size(), conTri);
    if (distintos != 0) { err = "mallamirar: el alambre de la malla pendiente no es el de su edicion"; return false; }
    if (conTri == 0) { err = "mallamirar: la prueba del alambre no distingue aristas de triangulos"; return false; }
    if (m->edicionPendiente.empty()) { err = "mallamirar: mirar la malla la materializo"; return false; }
    return true;
}

// ---------------------------------------------------------------------------
//  meminfo: la linea de las mallas como recurso (el resto lo imprime W3dScript.cpp)
// ---------------------------------------------------------------------------
bool CmdMemInfoMallas(std::istringstream& ss, std::string& err, bool& manejado) {
    const std::vector<MallaRecurso*>& reg = W3dMallasRegistro();
    int vinculados = 0;
    for (size_t i = 0; i < reg.size(); i++) vinculados += (int)reg[i]->usuarios.size();
    const int cargados = W3dMallasRecursosCargados(), vbo = W3dMallasVBOCompartidos();
    printf("      [meminfo] mallas como recurso: registro=%d cargadas=%d objetos vinculados=%d "
           "VBO compartidos=%d (%ld B de GPU)\n", (int)reg.size(), cargados, vinculados, vbo, W3dMallasVBOBytes());
    // los asserts NUEVOS se atienden aca; los de siempre siguen en W3dScript.cpp (ss intacto)
    const std::streampos pos = ss.tellg();
    std::string sub; long esperado = -1;
    if ((ss >> sub) && (sub == "recursosmalla" || sub == "vbocompartidos")) {
        manejado = true;
        if (!(ss >> esperado)) { err = "meminfo: falta el numero del assert"; return false; }
        const long real = (sub == "recursosmalla") ? cargados : vbo;
        if (real != esperado) {
            char b[128]; snprintf(b, sizeof(b), "meminfo %s: hay %ld y se esperaba %ld", sub.c_str(), real, esperado);
            err = b; return false;
        }
        printf("      [meminfo] assert OK (%s = %ld)\n", sub.c_str(), real);
        return true;
    }
    ss.clear();
    ss.seekg(pos);
    manejado = false;
    return false;
}

// ---------------------------------------------------------------------------
//  objcaras / mallarefs / w3dsinentrada
// ---------------------------------------------------------------------------
bool CmdObjCaras(std::istringstream& ss, std::string& err) {
    std::string ruta; long esperado = -1;
    ss >> ruta >> esperado;
    if (ruta.empty() || esperado < 0) { err = "objcaras: uso: objcaras <ruta.obj> <N>"; return false; }
    FILE* f = fopen(ruta.c_str(), "rb");
    if (!f) { err = "objcaras: no pude abrir " + ruta; return false; }
    long caras = 0, verts = 0;
    char linea[512];
    while (fgets(linea, sizeof(linea), f)) {
        if (linea[0] == 'f' && linea[1] == ' ') caras++;
        else if (linea[0] == 'v' && linea[1] == ' ') verts++;
    }
    fclose(f);
    printf("      [objcaras] %s: %ld cara(s), %ld vertice(s)\n", ruta.c_str(), caras, verts);
    if (caras != esperado) { char b[128]; snprintf(b, sizeof(b), "objcaras: %ld caras, se esperaban %ld", caras, esperado); err = b; return false; }
    return true;
}
bool CmdMallaRefs(std::istringstream& ss, std::string& err) {
    std::string id; long n = 0;
    ss >> id >> n;
    W3dRecurso* r = id.empty() ? NULL : W3dRecursoBuscar(W3DREC_MALLA, id);
    if (!r || n == 0) { err = "mallarefs: uso: mallarefs <entrada cargada> <+N|-N>"; return false; }
    if (n > 0) for (long i = 0; i < n; i++) W3dRecursoRetener(r, W3DREC_PERMANENTE);
    else {
        if (r->refTotal + n < 1) { err = "mallarefs: no se pueden soltar tantas (liberaria la malla)"; return false; }
        for (long i = 0; i < -n; i++) W3dRecursoSoltar(r, W3DREC_PERMANENTE);
    }
    printf("      [mallarefs] %s: refs=%d\n", id.c_str(), r->refTotal);
    return true;
}
bool CmdMatRenombrar(std::istringstream& ss, std::string& err) {
    std::string viejo, nuevo; ss >> viejo >> nuevo;
    Material* mt = viejo.empty() ? NULL : BuscarMaterialPorNombre(viejo);
    if (!mt || nuevo.empty()) { err = "matrenombrar: uso: matrenombrar <material existente> <nombre>"; return false; }
    int im = -1;
    for (size_t k = 0; k < Materials.size(); k++) if (Materials[k] == mt) { im = (int)k; break; }
    if (im >= 0) UndoCapturarRename(W3dDestGlobal(W3dRenameDest::MaterialG, im));
    mt->name = nuevo;
    printf("      [matrenombrar] '%s' -> '%s'\n", viejo.c_str(), mt->name.c_str());
    return true;
}
bool CmdW3dSinEntrada(std::istringstream& ss, std::string& err) {
    std::string orig, dest, quitar;
    ss >> orig >> dest >> quitar;
    if (orig.empty() || dest.empty() || quitar.empty()) { err = "w3dsinentrada: uso: w3dsinentrada <origen.w3d> <destino.w3d> <entrada>"; return false; }
    std::vector<W3dZipEntrada> ents;
    if (!W3dZipLeer(orig, &ents)) { err = "w3dsinentrada: no pude leer " + orig; return false; }
    W3dZipWriter w;
    for (size_t i = 0; i < ents.size(); i++)   // mimetype PRIMERO (estilo ODF)
        if (ents[i].nombre == "mimetype") w.Agregar(ents[i].nombre, ents[i].datos);
    bool estaba = false;
    for (size_t i = 0; i < ents.size(); i++) {
        if (ents[i].nombre == "mimetype") continue;
        if (ents[i].nombre == quitar) { estaba = true; continue; }
        w.Agregar(ents[i].nombre, ents[i].datos);
    }
    if (!estaba) { err = "w3dsinentrada: " + orig + " no tiene la entrada '" + quitar + "'"; return false; }
    if (!w.Guardar(dest)) { err = "w3dsinentrada: no pude escribir " + dest; return false; }
    printf("      [w3dsinentrada] %s -> %s sin '%s'\n", orig.c_str(), dest.c_str(), quitar.c_str());
    return true;
}

} // namespace

// el despachador de esta area (lo llama W3dPruebasRecursosCmd)
bool W3dPruebasMallasCmd(const std::string& cmd, std::istringstream& ss, std::string& err, bool& manejado) {
    manejado = true;
    if (cmd == "mallainfo")      return CmdMallaInfo(ss, err);
    if (cmd == "mallasregistro") return CmdMallasRegistro(ss, err);
    if (cmd == "mallarender")    return CmdMallaRender(err);
    if (cmd == "mallaunica")     return CmdMallaUnica(ss, err, false);
    if (cmd == "mallanueva")     return CmdMallaUnica(ss, err, true);
    if (cmd == "mallaasignar")   return CmdMallaAsignar(ss, err);
    if (cmd == "mallarenombrar") return CmdMallaRenombrar(ss, err);
    if (cmd == "malladup")       return CmdMallaDup(ss, err);
    if (cmd == "mallavert")      return CmdMallaVert(ss, err);
    if (cmd == "mallamaterial")  return CmdMallaMaterial(ss, err);
    if (cmd == "mallaskindos")   return CmdMallaSkinDos(ss, err);
    if (cmd == "mallaadquirir")  return CmdMallaAdquirir(ss, err);
    if (cmd == "mallacarpeta")   return CmdMallaCarpeta(ss, err);
    if (cmd == "mallaspurgar")   return CmdMallasPurgar(ss, err);
    if (cmd == "mallaslegado")   return CmdMallasLegado(ss, err);
    if (cmd == "mallaflip")      return CmdMallaFlip(ss, err);
    if (cmd == "mallauv")        return CmdMallaUV(ss, err);
    if (cmd == "mallava")        return CmdMallaVA(ss, err);
    if (cmd == "mallamirar")     return CmdMallaMirar(ss, err);
    if (cmd == "mallasvbo")      return CmdMallasVBO(ss, err);
    if (cmd == "objcaras")       return CmdObjCaras(ss, err);
    if (cmd == "mallarefs")      return CmdMallaRefs(ss, err);
    if (cmd == "w3dsinentrada")  return CmdW3dSinEntrada(ss, err);
    if (cmd == "matrenombrar")   return CmdMatRenombrar(ss, err);
    if (cmd == "meminfo")        return CmdMemInfoMallas(ss, err, manejado);
    manejado = false;
    return false;
}
