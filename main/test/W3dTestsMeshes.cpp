// ============================================================================
//  W3dTestsMeshes.cpp — comandos de harness de las MALLAS 3D COMO RECURSO
//  (libs/Whisk3DCore/objects/MeshResource.h + main/io/ProjectMeshes.h).
//  Los despacha W3dTestsResourcesCmd (test/W3dTestsResources.cpp) ANTES que los
//  suyos: viven aparte para no pisarse con los comandos de otras areas.
//
//  Comandos:
//    mallainfo <objeto|@ultimo> [recurso N|-] [usuarios N] [vbo compartido|pose|propio]
//              [edicion pendiente|cargada] [comparte <otro>] [nocomparte <otro>] [material <parte> <nombre>]
//              [grupos N] [caras N] [flipbook si|no] [carpeta <ruta|->]
//        la malla del objeto: su recurso (- = suelta), cuantos objetos de la escena lo usan,
//        de donde dibuja (compartido = los VBO del recurso; pose = comparte getColor/uv/indices
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
//        ListaCarga "malla:"): el op Cargar del tipo lee la entrada .w3db. 'purge' la suelta.
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
//        los VBO de CADA malla de un recurso en la escena (despues de un 'meshrender'): cuantas
//        tienen algun VBO PROPIO (vboPos/Nor/Col/UV/Idx != 0: una subida por objeto), cuantas
//        dibujaron TODAS sus capas con los VBO del recurso (sharedVbo = las capas que el
//        recurso tiene) y cuantas se dibujaron (sharedVbo != 0). Mil arboles = un juego de
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
//        y la POSA en 'drawframe' con su controlador de reproduccion: una malla posada por su anim.
//    mallamirar <objeto> [aristas N] [caras N]
//        MIRAR una malla con la edicion PENDIENTE sin materializarla: la topologia compartida
//        del recurso (aristas/caras, vs una copia materializada), el snap a su arista y a su
//        cara, la seleccion por caja sobre su cara, y el WIREFRAME: la vista entera dibujada con
//        la edicion pendiente es IDENTICA (pixel a pixel) a la misma vista con todo materializado.
// ============================================================================
#include "test/W3dTestsResources.h"
#include "objects/Objects.h"
#include "objects/Mesh.h"
#include "objects/MeshResource.h"
#include "objects/Armature.h"
#include "objects/Materials.h"
#include "objects/Collection.h"
#include "objects/ObjectMode.h"        // DuplicatedObject / NewInstance / W3dDuplicateOne
#include "io/ProjectMeshes.h"
#include "io/ProjectResources.h"      // renombrar / mover la malla 3D: la biblioteca
#include "io/W3dResources.h"
#include "io/W3dMeshBin.h"
#include "edit/MeshEdit.h"
#include "edit/Modifier.h"
#include "edit/WeightPaint.h"          // WeightPaintEnsureMap
#include "animation/SkeletalAnimation.h" // EvaluatePoseSkeleton / SkinMesh / PrepareAuthoredSkin
#include "animation/Animation.h"   // CurrentFrame
#include "undo/Undo.h"
#include "base/W3dInteractionState.h"
#include "graphics/w3dGraphics.h"
#include "ViewPorts/ViewPorts.h"       // rootViewport
#include "ViewPorts/ViewPort3D.h"      // Viewport3D (proyectar al viewport)
#include "ViewPorts/LayoutInput.h"     // SnapFindTarget / g_snap
#include "ViewPorts/Pick3D.h"          // BoxSelectApply3D
#include "WhiskUI/draw/glesdraw.h"     // W3dScreenHeight (leer el framebuffer)
#include "ViewPorts/Properties.h"      // la tarjeta "Malla 3D" (rename / carpeta por la UI)
#include "WhiskUI/widgets/TextField.h" // g_activeTextField: el input del rename in-place
#include "io/SaveW3D.h"                // g_w3dMeshesLegacy
#include "io/W3dZip.h"                 // w3dsinentrada: copiar el contenedor sin una entrada
#include <map>
#include <stdio.h>
#include <string.h>
#include <math.h>

extern void RestartScene();
extern void RebindMaterialMeshPart();
extern Viewport3D* Viewport3DActive;
extern bool RenameActive();   // Properties.cpp: el rename in-place de la tarjeta
extern void RenameCommit();
extern void RenameCancel();

namespace {

Object* gLast = NULL;   // el ultimo objeto que creo malladup (@ultimo)
std::map<std::string, Vector3> gSavedVerts;

Object* FindObj(Object* o, const std::string& n) {
    if (!o) return NULL;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        if (!h) continue;
        if (h->name == n) return h;
        Object* r = FindObj(h, n);
        if (r) return r;
    }
    return NULL;
}
Mesh* MeshOf(const std::string& n, std::string& err, const char* cmd) {
    Object* o = (n == "@ultimo") ? gLast : FindObj(SceneCollection, n);
    if (!o || o->getType() != ObjectType::mesh) { err = std::string(cmd) + ": no hay una malla '" + n + "'"; return NULL; }
    return (Mesh*)o;
}
std::string Name(const MeshResource* r) { return r ? r->name : std::string("-"); }

// de donde dibuja la malla (la misma decision que Mesh::RenderObject)
std::string VBOMode(const Mesh* m) {
    if (!m->mesh || !m->dataComp) return "self";
    if (m->skinArmature) return (m->dataComp & (W3DMD_UV | W3DMD_COL | W3DMD_FACES)) ? "pose" : "self";
    return (m->dataComp & W3DMD_POS) ? "shared" : "self";
}

bool CmdMeshInfo(std::istringstream& ss, std::string& err) {
    W3dMeshesEnsureResources();   // (como la UI al listar: la malla que nacio sin cuadro de por medio ya tiene la suya)
    std::string n; ss >> n;
    Mesh* m = MeshOf(n, err, "meshcheck");
    if (!m) return false;
    const int us = m->mesh ? W3dMeshResourceUsersInScene(m->mesh) : 1;
    printf("      [mallainfo] '%s': recurso '%s' (entrada '%s') usuarios=%d (vinculados=%d) vbo=%s edicion=%s "
           "comparte=0x%02x rverts=%d tris=%d caras=%d\n",
           m->name.c_str(), Name(m->mesh).c_str(), m->mesh ? m->mesh->entry.c_str() : "",
           us, m->mesh ? (int)m->mesh->users.size() : 0, VBOMode(m).c_str(),
           m->pendingEdit.empty() ? "loaded" : "pending", (unsigned)m->dataComp,
           m->vertexSize, m->facesSize / 3, (int)m->faces3d.size());
    std::string k;
    while (ss >> k) {
        std::string v;
        if (!(ss >> v)) { err = "meshcheck: falta el valor de '" + k + "'"; return false; }
        if (k == "resource") { if (Name(m->mesh) != v) { err = "meshcheck: recurso '" + Name(m->mesh) + "', se esperaba '" + v + "'"; return false; } }
        else if (k == "users") {
            if (us != atoi(v.c_str())) { char b[96]; snprintf(b, sizeof(b), "meshcheck: %d usuarios, se esperaban %s", us, v.c_str()); err = b; return false; }
        }
        else if (k == "vbo") { if (VBOMode(m) != v) { err = "meshcheck: vbo " + VBOMode(m) + ", se esperaba " + v; return false; } }
        else if (k == "edit") {
            const std::string e = m->pendingEdit.empty() ? "loaded" : "pending";
            if (e != v) { err = "meshcheck: edicion " + e + ", se esperaba " + v; return false; }
        }
        else if (k == "material") {   // material <parte> <nombre>: el material de esa parte
            std::string mn; ss >> mn;
            const int p = atoi(v.c_str());
            if (p < 0 || p >= (int)m->materialsGroup.size()) { err = "meshcheck: parte fuera de rango"; return false; }
            const Material* mat = m->materialsGroup[(size_t)p].material;
            const std::string real = mat ? mat->name : std::string("-");
            if (real != mn) { err = "meshcheck: la parte " + v + " tiene el material '" + real + "', se esperaba '" + mn + "'"; return false; }
        }
        else if (k == "faces") {    // cuantas caras LOGICAS tiene (las suyas o, pendiente, las del recurso)
            const std::vector<MeshFace>* F = W3dMeshFacesView(m);
            const int nc = F ? (int)F->size() : 0;
            if (nc != atoi(v.c_str())) {
                char b[96]; snprintf(b, sizeof(b), "meshcheck: %d caras, se esperaban %s", nc, v.c_str());
                err = b; return false;
            }
        }
        else if (k == "groups") {   // cuantos vertex groups tiene la malla (los extras por instancia)
            if ((int)m->vertexGroups.size() != atoi(v.c_str())) {
                char b[96]; snprintf(b, sizeof(b), "meshcheck: %d vertex groups, se esperaban %s", (int)m->vertexGroups.size(), v.c_str());
                err = b; return false;
            }
        }
        else if (k == "flipbook") {   // si|no: la malla anima sus UV
            const bool has = m->flipbook != NULL;
            if (has != (v == "si")) { err = std::string("meshcheck: flipbook ") + (has ? "si" : "no") + ", se esperaba " + v; return false; }
        }
        else if (k == "folder") {   // la carpeta del recurso ('-' = sin carpeta)
            const std::string c = m->mesh ? m->mesh->folder : std::string();
            if ((c.empty() ? std::string("-") : c) != v) { err = "meshcheck: carpeta '" + c + "', se esperaba '" + v + "'"; return false; }
        }
        else if (k == "shares" || k == "noshare") {
            Mesh* o = MeshOf(v, err, "meshcheck");
            if (!o) return false;
            const bool same = m->vertex && m->vertex == o->vertex && m->faces == o->faces;
            if (same != (k == "shares")) {
                err = "meshcheck: '" + m->name + "' y '" + o->name + (same ? "' SI" : "' NO") + " comparten la geometria";
                return false;
            }
        }
        else { err = "meshcheck: assert desconocido '" + k + "'"; return false; }
    }
    return true;
}

bool CmdMeshesRegistry(std::istringstream& ss, std::string& err) {
    W3dMeshesEnsureResources();
    const std::vector<MeshResource*>& reg = W3dMeshesRegistry();
    for (size_t i = 0; i < reg.size(); i++) {
        const MeshResource* r = reg[i];
        printf("      [mallasregistro] '%s' entrada='%s' carpeta='%s' usuarios=%d vinculados=%d %s%s%s\n",
               r->name.c_str(), r->entry.c_str(), r->folder.c_str(), W3dMeshResourceUsersInScene(r),
               (int)r->users.size(), r->Loaded() ? "loaded" : "sin cargar",
               r->vboPos ? " vbo" : "", r->modified ? " modificada" : "");
    }
    const int n = (int)reg.size(), loaded = W3dMeshesResourcesLoaded(), vbo = W3dMeshesVBOShared();
    int withExtras = 0;   // recursos que todavia guardan los extras (grupos/formas/ctrl) ademas de sus usuarios
    for (size_t i = 0; i < reg.size(); i++) if (reg[i]->extrasInResource) withExtras++;
    printf("      [mallasregistro] %d malla(s), %d cargada(s), %d con VBO compartido (%ld B de GPU), %d con extras propios\n",
           n, loaded, vbo, W3dMeshesVBOBytes(), withExtras);
    std::string k;
    while (ss >> k) {
        std::string v;
        if (!(ss >> v)) { err = "meshesregistry: falta el valor de '" + k + "'"; return false; }
        char b[128];
        if (k == "n" && n != atoi(v.c_str())) { snprintf(b, sizeof(b), "meshesregistry: %d mallas, se esperaban %s", n, v.c_str()); err = b; return false; }
        if (k == "loaded" && loaded != atoi(v.c_str())) { snprintf(b, sizeof(b), "meshesregistry: %d cargadas, se esperaban %s", loaded, v.c_str()); err = b; return false; }
        if (k == "vbo" && vbo != atoi(v.c_str())) { snprintf(b, sizeof(b), "meshesregistry: %d con VBO, se esperaban %s", vbo, v.c_str()); err = b; return false; }
        if (k == "has" && !W3dMeshResourceByName(v)) { err = "meshesregistry: no esta la malla '" + v + "'"; return false; }
        if (k == "hasnot" && W3dMeshResourceByName(v)) { err = "meshesregistry: esta la malla '" + v + "'"; return false; }
        if (k == "extras" && withExtras != atoi(v.c_str())) { snprintf(b, sizeof(b), "meshesregistry: %d con extras propios, se esperaban %s", withExtras, v.c_str()); err = b; return false; }
        if (k == "folder") {   // carpeta <malla> <ruta|->
            std::string path; ss >> path;
            const MeshResource* r = W3dMeshResourceByName(v);
            if (!r) { err = "meshesregistry: no esta la malla '" + v + "'"; return false; }
            if ((r->folder.empty() ? std::string("-") : r->folder) != path) {
                err = "meshesregistry: la malla '" + v + "' tiene la carpeta '" + r->folder + "', se esperaba '" + path + "'";
                return false;
            }
        }
        if (k != "n" && k != "loaded" && k != "vbo" && k != "has" && k != "hasnot" && k != "extras" && k != "folder") {
            err = "meshesregistry: assert desconocido '" + k + "'"; return false;
        }
    }
    return true;
}

bool CmdMeshRender(std::string& err) {
    if (!rootViewport) { err = "meshrender: no hay layout"; return false; }
    rootViewport->Render();
    w3dEngine::Finish();
    return true;
}

// deja el editor NAVEGANDO en Modo Objeto con 'o' como unico seleccionado (lo que exigen
// Shift+D / Alt+D / la tarjeta)
void OnlySelected(Object* o) {
    InteractionMode = ObjectMode;
    state = editNavigation;
    DeselectAll();
    o->Select();
    ActiveObj = o;
}

bool CmdMeshUnique(std::istringstream& ss, std::string& err, bool newValue) {
    std::string n; ss >> n;
    Mesh* m = MeshOf(n, err, newValue ? "meshnew" : "meshunique");
    if (!m) return false;
    OnlySelected(m);
    MeshResource* r = newValue ? W3dMeshNewCopy(m) : W3dMeshMakeUnique(m);
    printf("      [%s] '%s' -> '%s'\n", newValue ? "meshnew" : "meshunique", m->name.c_str(), Name(r).c_str());
    if (!r) { err = "no se pudo"; return false; }
    return true;
}

bool CmdMeshAssign(std::istringstream& ss, std::string& err) {
    std::string n, rn; ss >> n >> rn;
    Mesh* m = MeshOf(n, err, "meshassign");
    if (!m) return false;
    MeshResource* r = W3dMeshResourceByName(rn);
    if (!r) { err = "meshassign: no hay una malla '" + rn + "' en el registro"; return false; }
    OnlySelected(m);
    if (!W3dMeshAssign(m, r)) { err = "meshassign: no se pudo"; return false; }
    return true;
}

// la PRIMERA hoja Properties del layout (la tarjeta "Malla 3D" vive ahi)
void LeavesOf(ViewportBase* n, int kind, ViewportBase*& out) {
    if (!n || out) return;
    if (n->isLeaf()) { if (n->ViewportKind() == kind) out = n; return; }
    if (n->ContainerKind() == 1) { LeavesOf(((ViewportRow*)n)->childA, kind, out); LeavesOf(((ViewportRow*)n)->childB, kind, out); }
    else { LeavesOf(((ViewportColumn*)n)->childA, kind, out); LeavesOf(((ViewportColumn*)n)->childB, kind, out); }
}
// EL RENAME / LA CARPETA de la malla 3D de un objeto: hoy se hacen en la BIBLIOTECA (el outliner), no en la
// tarjeta "Malla 3D" (que solo elige que malla usa el objeto). La MISMA puerta que el renombrar en linea y el
// "Move to Folder": W3dViewRecRename / W3dViewRecMove, con sus pasos de undo. 'text' ya viene traducido.
bool RenameByCard(Mesh* m, bool folder, const std::string& text, std::string& err, const char* cmd) {
    W3dMeshesEnsureResources();   // TODA malla es un recurso (la que nacio en este cuadro tambien)
    if (!m->mesh) { err = std::string(cmd) + ": la malla no tiene su malla 3D"; return false; }
    OnlySelected(m);
    const std::string id = m->mesh->name;
    const bool ok = folder ? W3dViewRecMove(W3D_VIEW_MESHES, id, W3dFolderNormalize(text))
                            : W3dViewRecRename(W3D_VIEW_MESHES, id, text, NULL);
    if (!ok) { err = std::string(cmd) + ": la biblioteca no pudo"; return false; }
    return true;
}

bool CmdMeshRename(std::istringstream& ss, std::string& err) {
    std::string n, newValue; ss >> n >> newValue;
    Mesh* m = MeshOf(n, err, "meshrename");
    if (!m) return false;
    if (!RenameByCard(m, false, newValue == "-" ? std::string() : newValue, err, "meshrename")) return false;
    printf("      [mallarenombrar] '%s' -> '%s'\n", n.c_str(), m->mesh ? m->mesh->name.c_str() : "(suelta)");
    return true;
}

bool CmdMeshDup(std::istringstream& ss, std::string& err) {
    std::string n, mode; ss >> n >> mode;
    Mesh* m = MeshOf(n, err, "meshdup");
    if (!m) return false;
    if (mode != "linked" && mode != "copy") { err = "meshdup: uso: meshdup <objeto> vinculado|copia"; return false; }
    OnlySelected(m);
    if (mode == "linked") NewInstance(); else DuplicatedObject();
    gLast = ActiveObj;
    state = editNavigation;   // los dos dejan el modal de MOVER abierto (como el teclado)
    if (!gLast || gLast == m) { err = "meshdup: no se creo nada"; return false; }
    printf("      [malladup] '%s' %s -> '%s'\n", m->name.c_str(), mode.c_str(), gLast->name.c_str());
    return true;
}

bool CmdMeshVert(std::istringstream& ss, std::string& err) {
    std::string n; int i = -1; ss >> n >> i;
    Mesh* m = MeshOf(n, err, "meshvert");
    if (!m) return false;
    if (!m->vertex || i < 0 || i >= m->vertexSize) { err = "meshvert: indice fuera de rango"; return false; }
    const Vector3 p(m->vertex[i*3], m->vertex[i*3+1], m->vertex[i*3+2]);
    printf("      [mallavert] '%s'[%d] = (%.5f, %.5f, %.5f)\n", m->name.c_str(), i, p.x, p.y, p.z);
    std::string k;
    while (ss >> k) {
        if (k == "save") { std::string c; ss >> c; gSavedVerts[c] = p; continue; }
        if (k == "delta") {
            std::string c; float dx = 0, dy = 0, dz = 0, tol = 1e-4f;
            ss >> c >> dx >> dy >> dz;
            { std::streampos pos = ss.tellg(); float t; if (ss >> t) tol = t; else { ss.clear(); ss.seekg(pos); } }
            if (!gSavedVerts.count(c)) { err = "meshvert: no hay una posicion guardada '" + c + "'"; return false; }
            const Vector3 o = gSavedVerts[c];
            if (fabsf(p.x - o.x - dx) > tol || fabsf(p.y - o.y - dy) > tol || fabsf(p.z - o.z - dz) > tol) {
                char b[200];
                snprintf(b, sizeof(b), "meshvert: '%s'[%d] se movio (%.4f,%.4f,%.4f), se esperaba (%.4f,%.4f,%.4f)",
                         m->name.c_str(), i, p.x - o.x, p.y - o.y, p.z - o.z, dx, dy, dz);
                err = b; return false;
            }
            continue;
        }
        err = "meshvert: argumento desconocido '" + k + "'"; return false;
    }
    return true;
}

bool CmdMeshMaterial(std::istringstream& ss, std::string& err) {
    std::string n, mn; int k = -1; ss >> n >> k >> mn;
    Mesh* m = MeshOf(n, err, "meshmaterial");
    if (!m) return false;
    if (k < 0 || k >= (int)m->materialsGroup.size()) { err = "meshmaterial: parte fuera de rango"; return false; }
    Material* mat = FindMaterialByName(mn);
    if (!mat) mat = new Material(mn);
    OnlySelected(m);
    UndoCaptureMaterial(m, k);
    m->materialsGroup[(size_t)k].material = mat;
    RebindMaterialMeshPart();   // la puerta de la tarjeta Material
    return true;
}

// ---------------------------------------------------------------------------
//  mallaadquirir <entrada> bloqueante|async: una referencia TRANSITORIA sobre una malla del
//  registro por el almacen de recursos, como la pide una ListaCarga "malla:" (el op Cargar lee
//  la entrada). Con async queda EN_VUELO hasta el 'pump'; 'purge' la suelta.
// ---------------------------------------------------------------------------
bool CmdMeshAcquire(std::istringstream& ss, std::string& err) {
    std::string id, mode; ss >> id >> mode;
    if (id.empty()) { err = "meshacquire: uso: meshacquire <entrada> [bloqueante|async]"; return false; }
    W3dMeshesEnsureOps();
    W3dResource* r = W3dResourceAcquire(W3DREC_MESH, id, W3DREC_TRANSIENT,
                                       mode == "async" ? W3DREC_ASYNC : W3DREC_BLOCKING, "harness");
    if (!r) { err = "meshacquire: el almacen no devolvio descriptor"; return false; }
    static const char* kEst[] = { "NO_CARGADO", "EN_VUELO", "FIXUP", "LISTO", "FALLO" };
    printf("      [mallaadquirir] %s -> %s\n", id.c_str(), kEst[r->state]);
    return true;
}

// ---------------------------------------------------------------------------
//  mallaskindos: skinning POR INSTANCIA con un recurso compartido
// ---------------------------------------------------------------------------
Armature* BuildRig(Object* parent, const char* name, const Vector3& pos) {
    Armature* a = new Armature(parent, pos);
    a->name = name;
    { W3dBone r; r.name = "raiz";  r.parent = -1; r.head = Vector3(0,0,0); r.tail = Vector3(0,1,0); a->bones.push_back(r); }
    { W3dBone c; c.name = "brazo"; c.parent = 0;  c.head = Vector3(0,1,0); c.tail = Vector3(0,2,0); a->bones.push_back(c); }
    PrepareAuthoredSkin(a);
    return a;
}
void Rig(Mesh* s, Armature* a) {
    for (size_t k = 0; k < s->modifiers.size(); k++)
        if (s->modifiers[k] && s->modifiers[k]->type == ModifierType::Armature) s->modifiers[k]->target = (Object*)a;
    s->skinArmature = a;
    s->lastSkinFrame = -999999;
}
Mesh* SkinOf(const char* name) {
    Object* o = FindObj(SceneCollection, name);
    return (o && o->getType() == ObjectType::mesh) ? (Mesh*)o : NULL;
}
Armature* RigOf(const char* name) {
    Object* o = FindObj(SceneCollection, name);
    return (o && o->getType() == ObjectType::armature) ? (Armature*)o : NULL;
}
void Pose(Armature* a, float degrees) {
    // primero la pose del frame actual (un cambio de frame la refresca desde la curva/rest y
    // pisaria lo posado); despues se posa A MANO (poseDirty), como en Pose Mode
    EvaluatePoseSkeleton(a, CurrentFrame);
    a->bones[1].poseR = Vector3(0, 0, degrees);
    a->poseDirty = true;
    EvaluatePoseSkeleton(a, CurrentFrame);
}
bool SamePose(Mesh* a, Mesh* b) {
    if (!a->skinVertex || !b->skinVertex || a->vertexSize != b->vertexSize) return false;
    return memcmp(a->skinVertex, b->skinVertex, (size_t)a->vertexSize * 3 * sizeof(GLfloat)) == 0;
}

bool CmdMeshSkinTwo(std::istringstream& ss, std::string& err) {
    std::string op; ss >> op;
    if (op == "build") {
        RestartScene();
        Collection* col = new Collection(SceneCollection);
        col->SetNameObj("Dos");
        CollectionActive = col;
        Armature* a = BuildRig(col, "RigA", Vector3(0, 0, 0));
        Armature* b = BuildRig(col, "RigB", Vector3(3, 0, 0));
        Mesh* s = (Mesh*)NewMesh(MeshType::cylinder, col, false);
        s->SetNameObj("PersonajeA");
        s->GenerateRender();
        WeightPaintEnsureMap(s);
        VertexGroup* vr = new VertexGroup("raiz"); VertexGroup* vb = new VertexGroup("brazo");
        std::vector<char> done(s->vertexSize, 0);
        for (int i = 0; i < s->vertexSize; i++) {
            const int cp = s->vertCtrlPoint[i];
            if (done[cp]) continue;
            done[cp] = 1;
            if (s->vertex[i*3+1] > 0.0f) { vb->verts.push_back(cp); vb->weights.push_back(1.0f); }
            else { vr->verts.push_back(cp); vr->weights.push_back(0.7f); }
        }
        s->vertexGroups.push_back(vr); s->vertexGroups.push_back(vb);
        Modifier* md = new Modifier(ModifierType::Armature, ModifierTypeName(ModifierType::Armature));
        md->target = (Object*)a; s->modifiers.push_back(md);
        s->skinArmature = a;
        // la malla pasa a ser un RECURSO y el segundo personaje es su duplicado VINCULADO (Alt+D)
        if (!W3dMeshCreateResource(s, "Personaje")) { err = "meshskintwo: no se pudo crear el recurso"; return false; }
        OnlySelected(s);
        NewInstance();
        state = editNavigation;
        Mesh* s2 = (ActiveObj && ActiveObj != s && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
        if (!s2) { err = "meshskintwo: Alt+D no creo la malla"; return false; }
        s2->SetNameObj("PersonajeB");
        s2->pos = Vector3(3, 0, 0);
        Rig(s2, b);
        DeselectAll(); ActiveObj = NULL;
        printf("      [mallaskindos] 'PersonajeA' (RigA) y 'PersonajeB' (RigB) usan la malla '%s'\n", Name(s->mesh).c_str());
        return true;
    }
    if (op != "verify") { err = "meshskintwo: uso: meshskintwo armar|verificar"; return false; }
    Mesh* A = SkinOf("PersonajeA"); Mesh* B = SkinOf("PersonajeB");
    Armature* ra = RigOf("RigA"); Armature* rb = RigOf("RigB");
    if (!A || !B || !ra || !rb) { err = "meshskintwo: falta PersonajeA/PersonajeB/RigA/RigB (armar primero)"; return false; }
    if (!A->mesh || A->mesh != B->mesh) { err = "meshskintwo: los dos personajes no usan el MISMO recurso"; return false; }
    if (A->vertex != B->vertex) { err = "meshskintwo: la geometria de reposo no es la misma memoria"; return false; }
    // dos poses DISTINTAS
    Pose(ra, 35.0f);
    Pose(rb, -50.0f);
    A->lastSkinFrame = -999999; B->lastSkinFrame = -999999;
    SkinMesh(A); SkinMesh(B);
    if (!A->skinVertex || !B->skinVertex) { err = "meshskintwo: el skinning no produjo pose"; return false; }
    if (SamePose(A, B)) { err = "meshskintwo: los dos personajes quedaron con la MISMA pose"; return false; }
    // la referencia: copias SUELTAS (arrays propios) skinneadas con cada esqueleto
    Mesh* ca = (Mesh*)W3dDuplicateOne(A); Mesh* cb = (Mesh*)W3dDuplicateOne(B);
    if (!ca || !cb || ca->mesh || cb->mesh) { err = "meshskintwo: la copia de referencia no es suelta"; return false; }
    Rig(ca, ra); Rig(cb, rb);
    SkinMesh(ca); SkinMesh(cb);
    const bool okA = SamePose(A, ca), okB = SamePose(B, cb);
    // la geometria del recurso NO se toco (el skinning escribe skinVertex, que es de cada uno)
    const bool rest = (A->vertex == A->mesh->vertex);
    printf("      [mallaskindos] pose A == copia suelta A: %s | pose B == copia suelta B: %s | A != B | reposo compartido: %s\n",
           okA ? "SI" : "NO", okB ? "SI" : "NO", rest ? "SI" : "NO");
    // las copias de referencia se borran (no quedan en la escena). Primero se suelta la
    // seleccion: el constructor de cada copia la dejo seleccionada y activa
    DeselectAll(); ActiveObj = NULL;
    Object* refs[2] = { ca, cb };
    for (int k = 0; k < 2; k++) {
        Object* p = refs[k]->Parent ? refs[k]->Parent : SceneCollection;
        for (size_t i = 0; i < p->Childrens.size(); i++) if (p->Childrens[i] == refs[k]) { p->Childrens.erase(p->Childrens.begin() + (long)i); break; }
        W3dFreeSubtree(refs[k]);
    }
    DeselectAll(); ActiveObj = NULL;
    Pose(ra, 0.0f); Pose(rb, 0.0f);
    if (!okA || !okB) { err = "meshskintwo: la pose de un personaje con la malla compartida no es la de su esqueleto"; return false; }
    if (!rest) { err = "meshskintwo: el skinning desinstancio la geometria de reposo"; return false; }
    return true;
}

// ---------------------------------------------------------------------------
//  mallacarpeta / mallaspurgar / mallaslegado
// ---------------------------------------------------------------------------
bool CmdMeshFolder(std::istringstream& ss, std::string& err) {
    std::string n, path; ss >> n >> path;
    Mesh* m = MeshOf(n, err, "meshfolder");
    if (!m) return false;
    const std::string text = (path == "-") ? std::string() : (path == "__") ? std::string("   ") : path;
    if (!RenameByCard(m, true, text, err, "meshfolder")) return false;
    printf("      [mallacarpeta] '%s' -> '%s'\n", m->mesh ? m->mesh->name.c_str() : "(suelta)",
           m->mesh ? m->mesh->folder.c_str() : "");
    return true;
}

// ---------------------------------------------------------------------------
//  mallasvbo: los VBO de cada objeto (no solo los del recurso)
// ---------------------------------------------------------------------------
void CollectMeshesScene(Object* o, std::vector<Mesh*>& out);   // (mas abajo, con mallamirar)
bool CmdMeshesVBO(std::istringstream& ss, std::string& err) {
    std::vector<Mesh*> all;
    CollectMeshesScene(SceneCollection, all);
    int own = 0, ofResource = 0, drawn = 0, fromResource = 0;
    for (size_t i = 0; i < all.size(); i++) {
        const Mesh* m = all[i];
        if (!m->mesh) continue;
        fromResource++;
        if (m->vboPos || m->vboNor || m->vboCol || m->vboUV || m->vboIdx) own++;
        if (m->sharedVbo) drawn++;
        const MeshResource* r = m->mesh;
        unsigned mask = 0;
        if (r->vertex)      mask |= W3DMD_POS;
        if (r->normals)     mask |= W3DMD_NOR;
        if (r->uv)          mask |= W3DMD_UV;
        if (r->vertexColor) mask |= W3DMD_COL;
        if (r->faces)       mask |= W3DMD_FACES;
        if (mask && (unsigned)m->sharedVbo == mask) ofResource++;
    }
    printf("      [mallasvbo] %d malla(s) de un recurso: %d con VBO propio, %d dibujadas con TODO del recurso, %d dibujadas\n",
           fromResource, own, ofResource, drawn);
    std::string k;
    while (ss >> k) {
        int v = -1;
        if (!(ss >> v)) { err = "meshesvbo: falta el valor de '" + k + "'"; return false; }
        const int real = (k == "own") ? own : (k == "delresource") ? ofResource : (k == "drawn") ? drawn : -999;
        if (real == -999) { err = "meshesvbo: assert desconocido '" + k + "'"; return false; }
        if (real != v) { char b[128]; snprintf(b, sizeof(b), "meshesvbo: %s = %d, se esperaba %d", k.c_str(), real, v); err = b; return false; }
    }
    return true;
}
bool CmdMeshesPurge(std::istringstream& ss, std::string& err) {
    const int n = W3dMeshesPurgeUnused();
    printf("      [mallaspurgar] %d recurso(s) purgado(s)\n", n);
    std::string k; int expected = -1;
    if ((ss >> k) && k == "n" && (ss >> expected) && expected != n) {
        char b[96]; snprintf(b, sizeof(b), "meshespurge: purgo %d, se esperaban %d", n, expected); err = b; return false;
    }
    return true;
}
bool CmdMeshesLegacy(std::istringstream& ss, std::string& err) {
    std::string v; ss >> v;
    if (v != "on" && v != "off") { err = "mesheslegacy: uso: mesheslegacy on|off"; return false; }
    g_w3dMeshesLegacy = (v == "on");
    printf("      [mallaslegado] %s\n", g_w3dMeshesLegacy ? "el guardado escribe una malla por objeto (esquema viejo)" : "registro de mallas");
    return true;
}

// ---------------------------------------------------------------------------
//  mallaflip / mallauv: una malla POSADA por su flipbook (la pose de la sesion)
// ---------------------------------------------------------------------------
bool CmdMeshFlip(std::istringstream& ss, std::string& err) {
    std::string n; int frames = 0, offset = 0; ss >> n >> frames >> offset;
    Mesh* m = MeshOf(n, err, "meshflip");
    if (!m) return false;
    if (frames < 2 || offset < 1 || offset >= frames) { err = "meshflip: uso: meshflip <objeto> <cuadros> <desfase 1..cuadros-1>"; return false; }
    m->SetUVAnimStrip(frames, 0.0f, 0, offset);   // fps 0: el cuadro no avanza solo
    m->TickUVAnimStrip(0.0f);                     // aplica el cuadro 'desfase' YA
    printf("      [mallaflip] '%s': tira de %d cuadros, en el cuadro %d (comparte=0x%02x)\n",
           m->name.c_str(), frames, m->flipApplied, (unsigned)m->dataComp);
    if (m->flipApplied != offset) { err = "meshflip: el cuadro no se aplico"; return false; }
    return true;
}
bool CmdMeshUV(std::istringstream& ss, std::string& err) {
    std::string n; int i = -1; ss >> n >> i;
    Mesh* m = MeshOf(n, err, "meshuv");
    if (!m) return false;
    if (!m->uv || i < 0 || i >= m->vertexSize) { err = "meshuv: indice fuera de rango"; return false; }
    const Vector3 p(m->uv[i*2], m->uv[i*2+1], 0.0f);
    printf("      [mallauv] '%s'[%d] = (%.5f, %.5f)\n", m->name.c_str(), i, p.x, p.y);
    std::string k;
    while (ss >> k) {
        if (k == "save") { std::string c; ss >> c; gSavedVerts["uv:" + c] = p; continue; }
        if (k == "delta") {
            std::string c; float du = 0, dv = 0, tol = 1e-5f;
            ss >> c >> du >> dv;
            { std::streampos pos = ss.tellg(); float t; if (ss >> t) tol = t; else { ss.clear(); ss.seekg(pos); } }
            if (!gSavedVerts.count("uv:" + c)) { err = "meshuv: no hay una uv guardada '" + c + "'"; return false; }
            const Vector3 o = gSavedVerts["uv:" + c];
            if (fabsf(p.x - o.x - du) > tol || fabsf(p.y - o.y - dv) > tol) {
                char b[200];
                snprintf(b, sizeof(b), "meshuv: '%s'[%d] se corrio (%.4f,%.4f), se esperaba (%.4f,%.4f)",
                         m->name.c_str(), i, p.x - o.x, p.y - o.y, du, dv);
                err = b; return false;
            }
            continue;
        }
        err = "meshuv: argumento desconocido '" + k + "'"; return false;
    }
    return true;
}

bool CmdMeshVA(std::istringstream& ss, std::string& err) {
    std::string n; float frame = 0.0f; ss >> n >> frame;
    Mesh* m = MeshOf(n, err, "meshva");
    if (!m) return false;
    if (!m->vertex || m->vertexSize <= 0 || frame <= 1.0f || frame > 10.0f) { err = "meshva: uso: meshva <objeto> <cuadro 1..10>"; return false; }
    VertexAnimation* an = new VertexAnimation(m, "Crece");
    an->startFrame = 1; an->endFrame = 10; an->fps = 30;
    const size_t n3 = (size_t)m->vertexSize * 3;
    std::vector<GLfloat> rest(m->vertex, m->vertex + n3), isDouble(rest);
    for (size_t i = 0; i < n3; i++) isDouble[i] *= 2.0f;
    VertexAnimSetKey(*an, 1, &rest[0], NULL, NULL, 1);
    VertexAnimSetKey(*an, 10, &isDouble[0], NULL, NULL, 1);
    NewActiveVertexAnimation(m, an);
    VertexAnimationActive* va = FindTargetAnim(m);
    if (!va) { err = "meshva: la malla no tiene controlador"; return false; }
    va->currentAnim = va->nextAnim = (int)m->animations.size() - 1;
    va->playFrame = frame;
    EvalVertexAnim(*an, m, frame);
    printf("      [mallava] '%s': anim '%s' posada en el cuadro %.1f (posada=%s, comparte=0x%02x)\n",
           m->name.c_str(), an->name.c_str(), frame, m->posedByAnim ? "si" : "no", (unsigned)m->dataComp);
    if (!m->posedByAnim) { err = "meshva: la anim no poso la malla"; return false; }
    return true;
}

// ---------------------------------------------------------------------------
//  mallamirar: la topologia COMPARTIDA para mirar una malla sin materializar su edicion
// ---------------------------------------------------------------------------
// la vista 3D en un buffer (RGBA, de abajo hacia arriba)
void ReadView(Viewport3D* vp, std::vector<unsigned char>& buf) {
    rootViewport->Render();
    w3dEngine::Finish();
    const int vpGLY = W3dScreenHeight - vp->y - vp->height;   // arbol arriba-izq -> GL
    buf.assign((size_t)vp->width * vp->height * 4, 0);
    w3dEngine::ReadPixelsRGBA(vp->x, vpGLY, vp->width, vp->height, &buf[0]);
}
void CollectMeshesScene(Object* o, std::vector<Mesh*>& out) {
    if (!o) return;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        if (!h) continue;
        if (h->getType() == ObjectType::mesh) out.push_back((Mesh*)h);
        CollectMeshesScene(h, out);
    }
}

bool CmdMeshLook(std::istringstream& ss, std::string& err) {
    std::string n; ss >> n;
    Mesh* m = MeshOf(n, err, "meshlook");
    if (!m) return false;
    if (!rootViewport) { err = "meshlook: no hay layout"; return false; }
    // nada seleccionado (sin contornos, y el snap de Modo Objeto saltea lo seleccionado)
    InteractionMode = ObjectMode; state = editNavigation;
    DeselectAll(); ActiveObj = NULL;
    if (m->pendingEdit != W3D_RESOURCE_EDIT) { err = "meshlook: la malla no tiene la edicion pendiente en su recurso"; return false; }
    const std::vector<int>* A = W3dMeshEdgesView(m);
    const std::vector<MeshFace>* F = W3dMeshFacesView(m);
    const std::vector<GLfloat>* B = W3dMeshBordersView(m);
    const int nA = A ? (int)A->size() / 2 : -1, nF = F ? (int)F->size() : -1;
    const bool resourceBorders = B && m->mesh && B == &m->mesh->borders;
    printf("      [mallamirar] '%s': aristas=%d caras=%d lineas=%s (edicion %s)\n", m->name.c_str(), nA, nF,
           resourceBorders ? "las del recurso" : (B ? "own" : "none"),
           m->pendingEdit.empty() ? "loaded" : "pending");
    if (!A || !F) { err = "meshlook: la malla no tiene topologia para mirar"; return false; }
    if (!resourceBorders) { err = "meshlook: las lineas del alambre no son las compartidas del recurso"; return false; }
    // ... y son EXACTAMENTE las de la malla materializada (una copia suelta con su edicion)
    {
        Mesh* ref = (Mesh*)W3dDuplicateOne(m);   // materializa la edicion de m... y la copia
        bool equal = ref && ref->edges == *A && ref->faces3d.size() == F->size();
        for (size_t f = 0; equal && f < F->size(); f++) if (ref->faces3d[f].idx != (*F)[f].idx) equal = false;
        DeselectAll(); ActiveObj = NULL;
        if (ref) {
            Object* p = ref->Parent ? ref->Parent : SceneCollection;
            for (size_t i = 0; i < p->Childrens.size(); i++) if (p->Childrens[i] == ref) { p->Childrens.erase(p->Childrens.begin() + (long)i); break; }
            W3dFreeSubtree(ref);
        }
        W3dMeshReleaseEdit(m);   // (la copia de referencia la habia materializado)
        if (!equal) { err = "meshlook: la topologia compartida no es la de la malla materializada"; return false; }
        A = W3dMeshEdgesView(m); F = W3dMeshFacesView(m);
        if (!A || !F || m->pendingEdit.empty()) { err = "meshlook: la malla no volvio a quedar pendiente"; return false; }
    }
    std::string k;
    while (ss >> k) {
        int v = 0; ss >> v;
        if (k == "edges" && nA != v) { char b[96]; snprintf(b, sizeof(b), "meshlook: %d aristas, se esperaban %d", nA, v); err = b; return false; }
        if (k == "faces" && nF != v)   { char b[96]; snprintf(b, sizeof(b), "meshlook: %d caras, se esperaban %d", nF, v); err = b; return false; }
    }
    rootViewport->Render();
    Viewport3D* vp = Viewport3DActive;
    if (!vp) { err = "meshlook: no hay viewport 3D"; return false; }
    vp->BindView();
    Matrix4 W; m->GetWorldMatrix(W);
    // SNAP a la arista y a la cara: el cursor sobre el centro de la primera de cada una
    const SnapCfg snapPrev = g_snap;
    g_snap.tsActive = g_snap.tsEdited = g_snap.tsNonEdited = true;
    bool snapOk = true;
    for (int pass = 0; pass < 2 && snapOk; pass++) {
        Vector3 c(0, 0, 0);
        if (pass == 0) {
            // la arista MAS LARGA en pantalla: la 0 de una esfera es del polo, donde 32 centros caen a menos de un pixel
            // entre si y el cursor (redondeado a pixel) engancha al vecino
            float bestLen = -1.0f;
            for (size_t e = 0; e + 1 < A->size(); e += 2) {
                const int a = (*A)[e], b = (*A)[e + 1];
                const Vector3 va(m->vertex[a*3], m->vertex[a*3+1], m->vertex[a*3+2]), vb(m->vertex[b*3], m->vertex[b*3+1], m->vertex[b*3+2]);
                float ax, ay, bx, by;
                if (!vp->ProjectPoint(W * va, ax, ay) || !vp->ProjectPoint(W * vb, bx, by)) continue;
                const float len = (bx - ax) * (bx - ax) + (by - ay) * (by - ay);
                if (len > bestLen) { bestLen = len; c = (va + vb) * 0.5f; }
            }
        } else {
            const std::vector<int>& id = (*F)[0].idx;
            for (size_t q = 0; q < id.size(); q++) c = c + Vector3(m->vertex[id[q]*3], m->vertex[id[q]*3+1], m->vertex[id[q]*3+2]);
            c = c * (1.0f / (float)id.size());
        }
        const Vector3 wc = W * c;
        float sx = 0, sy = 0;
        if (!vp->ProjectPoint(wc, sx, sy)) { err = "meshlook: el punto no cae en la vista"; snapOk = false; break; }
        g_snap.target = (pass == 0) ? SNAP_EDGECENTER : SNAP_FACECENTER;
        Vector3 hit; float hx = 0, hy = 0;
        const bool enc = SnapFindTarget((int)(sx + 0.5f), (int)(sy + 0.5f), vp, hit, hx, hy);
        const Vector3 d = hit - wc;
        const float dist = sqrtf(d.x*d.x + d.y*d.y + d.z*d.z);
        printf("      [mallamirar] snap al centro de %s: %s (a %.5f del esperado)\n", pass == 0 ? "la arista" : "la cara",
               enc ? "hooked" : "NO engancho", enc ? dist : -1.0f);
        if (!enc || dist > 1e-3f) { err = std::string("meshlook: el snap no engancho el centro de ") + (pass == 0 ? "la arista" : "la cara"); snapOk = false; }
    }
    g_snap = snapPrev;
    if (!snapOk) return false;
    // SELECCION POR CAJA (azul, "tocar"): una caja chica adentro de la primera cara
    {
        Vector3 c(0, 0, 0);
        const std::vector<int>& id = (*F)[0].idx;
        for (size_t q = 0; q < id.size(); q++) c = c + Vector3(m->vertex[id[q]*3], m->vertex[id[q]*3+1], m->vertex[id[q]*3+2]);
        c = c * (1.0f / (float)id.size());
        float sx = 0, sy = 0;
        vp->ProjectPoint(W * c, sx, sy);
        DeselectAll(); ActiveObj = NULL;
        BoxSelectApply3D((int)sx - 1, (int)sy - 1, (int)sx + 1, (int)sy + 1, true, false);
        const bool sel = m->select;
        printf("      [mallamirar] caja sobre la cara: %s\n", sel ? "la selecciono" : "NO la selecciono");
        DeselectAll(); ActiveObj = NULL;
        if (!sel) { err = "meshlook: la seleccion por caja no agarro la cara"; return false; }
    }
    // EL ALAMBRE: la vista en wireframe con las ediciones pendientes == con todo materializado
    const RenderType viewPrev = vp->view;
    vp->view = RenderType::Wireframe;   // el modo de dibujo del viewport (lo aplica cada render)
    std::vector<unsigned char> pend, mat;
    ReadView(vp, pend);
    std::vector<Mesh*> all; CollectMeshesScene(SceneCollection, all);
    std::vector<Mesh*> borrowed;
    for (size_t i = 0; i < all.size(); i++)
        if (all[i]->pendingEdit == W3D_RESOURCE_EDIT && W3dMeshBinMaterializeEdit(all[i]))
            borrowed.push_back(all[i]);
    ReadView(vp, mat);
    for (size_t i = 0; i < borrowed.size(); i++) W3dMeshReleaseEdit(borrowed[i]);
    // ...y la prueba es SENSIBLE: sin la topologia compartida (el camino de antes: el alambre de
    // los triangulos del render) la vista sale distinta (las diagonales de los quads)
    std::vector<unsigned char> tri;
    MeshResource* rm = m->mesh;
    rm->topoState = -1;
    ReadView(vp, tri);
    rm->topoState = 0;   // se vuelve a leer cuando alguien la pida
    vp->view = viewPrev;
    long distinct = 0, withTri = 0;
    for (size_t i = 0; i + 3 < pend.size(); i += 4) {
        if (pend[i] != mat[i] || pend[i+1] != mat[i+1] || pend[i+2] != mat[i+2]) distinct++;
        if (pend[i] != tri[i] || pend[i+1] != tri[i+1] || pend[i+2] != tri[i+2]) withTri++;
    }
    printf("      [mallamirar] alambre: %ld px distintos entre pendiente y materializado (%d mallas materializadas); "
           "con triangulos serian %ld px distintos\n", distinct, (int)borrowed.size(), withTri);
    if (distinct != 0) { err = "meshlook: el alambre de la malla pendiente no es el de su edicion"; return false; }
    if (withTri == 0) { err = "meshlook: la prueba del alambre no distingue aristas de triangulos"; return false; }
    if (m->pendingEdit.empty()) { err = "meshlook: mirar la malla la materializo"; return false; }
    return true;
}

// ---------------------------------------------------------------------------
//  meminfo: la linea de las mallas como recurso (el resto lo imprime W3dScript.cpp)
// ---------------------------------------------------------------------------
bool CmdMemInfoMeshes(std::istringstream& ss, std::string& err, bool& handled) {
    const std::vector<MeshResource*>& reg = W3dMeshesRegistry();
    int linked = 0;
    for (size_t i = 0; i < reg.size(); i++) linked += (int)reg[i]->users.size();
    const int loaded = W3dMeshesResourcesLoaded(), vbo = W3dMeshesVBOShared();
    printf("      [meminfo] mallas como recurso: registro=%d cargadas=%d objetos vinculados=%d "
           "VBO compartidos=%d (%ld B de GPU)\n", (int)reg.size(), loaded, linked, vbo, W3dMeshesVBOBytes());
    // las que soltaron su RAM (opcion de proyecto "mallasSoloGPU": las dibujan solo rutinas y viven en sus VBO)
    long savingGPU = 0;
    const int onlyGPU = W3dMeshesOnlyGPUCount(&savingGPU);
    printf("      [meminfo] mallas solo en GPU: %d (opcion %s) RAM ahorrada=%ld B (%.1f KB) recargas=%d\n", onlyGPU,
           W3dMeshesOnlyGPUActive() ? "on" : "off", savingGPU, savingGPU / 1024.0, W3dMeshesOnlyGPUReloads());
    // los asserts NUEVOS se atienden aca; los de siempre siguen en W3dScript.cpp (ss intacto)
    const std::streampos pos = ss.tellg();
    std::string sub; long expected = -1;
    if ((ss >> sub) && (sub == "meshresources" || sub == "sharedvbo" || sub == "gpuonly")) {
        handled = true;
        if (!(ss >> expected)) { err = "meminfo: falta el numero del assert"; return false; }
        const long real = (sub == "meshresources") ? loaded : (sub == "gpuonly") ? onlyGPU : vbo;
        if (real != expected) {
            char b[128]; snprintf(b, sizeof(b), "meminfo %s: hay %ld y se esperaba %ld", sub.c_str(), real, expected);
            err = b; return false;
        }
        printf("      [meminfo] assert OK (%s = %ld)\n", sub.c_str(), real);
        return true;
    }
    ss.clear();
    ss.seekg(pos);
    handled = false;
    return false;
}

// ---------------------------------------------------------------------------
//  objcaras / mallarefs / w3dsinentrada
// ---------------------------------------------------------------------------
bool CmdObjFaces(std::istringstream& ss, std::string& err) {
    std::string path; long expected = -1;
    ss >> path >> expected;
    if (path.empty() || expected < 0) { err = "objfaces: uso: objfaces <ruta.obj> <N>"; return false; }
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) { err = "objfaces: no pude abrir " + path; return false; }
    long faces = 0, verts = 0;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == 'f' && line[1] == ' ') faces++;
        else if (line[0] == 'v' && line[1] == ' ') verts++;
    }
    fclose(f);
    printf("      [objcaras] %s: %ld cara(s), %ld vertice(s)\n", path.c_str(), faces, verts);
    if (faces != expected) { char b[128]; snprintf(b, sizeof(b), "objfaces: %ld caras, se esperaban %ld", faces, expected); err = b; return false; }
    return true;
}
bool CmdMeshRefs(std::istringstream& ss, std::string& err) {
    std::string id; long n = 0;
    ss >> id >> n;
    W3dResource* r = id.empty() ? NULL : W3dResourceFind(W3DREC_MESH, id);
    if (!r || n == 0) { err = "meshrefs: uso: meshrefs <entrada cargada> <+N|-N>"; return false; }
    if (n > 0) for (long i = 0; i < n; i++) W3dResourceRetain(r, W3DREC_PERMANENT);
    else {
        if (r->refTotal + n < 1) { err = "meshrefs: no se pueden soltar tantas (liberaria la malla)"; return false; }
        for (long i = 0; i < -n; i++) W3dResourceRelease(r, W3DREC_PERMANENT);
    }
    printf("      [mallarefs] %s: refs=%d\n", id.c_str(), r->refTotal);
    return true;
}
bool CmdMatRename(std::istringstream& ss, std::string& err) {
    std::string old, newValue; ss >> old >> newValue;
    Material* mt = old.empty() ? NULL : FindMaterialByName(old);
    if (!mt || newValue.empty()) { err = "matrename: uso: matrename <material existente> <nombre>"; return false; }
    int im = -1;
    for (size_t k = 0; k < Materials.size(); k++) if (Materials[k] == mt) { im = (int)k; break; }
    if (im >= 0) UndoCaptureRename(W3dDestGlobal(W3dRenameDest::MaterialG, im));
    mt->name = newValue;
    printf("      [matrenombrar] '%s' -> '%s'\n", old.c_str(), mt->name.c_str());
    return true;
}
bool CmdW3dNoEntry(std::istringstream& ss, std::string& err) {
    std::string orig, dest, remove;
    ss >> orig >> dest >> remove;
    if (orig.empty() || dest.empty() || remove.empty()) { err = "w3dnoentry: uso: w3dnoentry <origen.w3d> <destino.w3d> <entrada>"; return false; }
    std::vector<W3dZipEntry> ents;
    if (!W3dZipRead(orig, &ents)) { err = "w3dnoentry: no pude leer " + orig; return false; }
    W3dZipWriter w;
    for (size_t i = 0; i < ents.size(); i++)   // mimetype PRIMERO (estilo ODF)
        if (ents[i].name == "mimetype") w.Add(ents[i].name, ents[i].data);
    bool was = false;
    for (size_t i = 0; i < ents.size(); i++) {
        if (ents[i].name == "mimetype") continue;
        if (ents[i].name == remove) { was = true; continue; }
        w.Add(ents[i].name, ents[i].data);
    }
    if (!was) { err = "w3dsinentrada: " + orig + " no tiene la entrada '" + remove + "'"; return false; }
    if (!w.Save(dest)) { err = "w3dnoentry: no pude escribir " + dest; return false; }
    printf("      [w3dsinentrada] %s -> %s sin '%s'\n", orig.c_str(), dest.c_str(), remove.c_str());
    return true;
}

} // namespace

// el despachador de esta area (lo llama W3dTestsResourcesCmd)
bool W3dTestsMeshesCmd(const std::string& cmd, std::istringstream& ss, std::string& err, bool& handled) {
    handled = true;
    if (cmd == "meshcheck")      return CmdMeshInfo(ss, err);
    if (cmd == "meshesregistry") return CmdMeshesRegistry(ss, err);
    if (cmd == "meshrender")    return CmdMeshRender(err);
    if (cmd == "meshunique")     return CmdMeshUnique(ss, err, false);
    if (cmd == "meshnew")     return CmdMeshUnique(ss, err, true);
    if (cmd == "meshassign")   return CmdMeshAssign(ss, err);
    if (cmd == "meshrename") return CmdMeshRename(ss, err);
    if (cmd == "meshdup")       return CmdMeshDup(ss, err);
    if (cmd == "meshvert")      return CmdMeshVert(ss, err);
    if (cmd == "meshmaterial")  return CmdMeshMaterial(ss, err);
    if (cmd == "meshskintwo")   return CmdMeshSkinTwo(ss, err);
    if (cmd == "meshacquire")  return CmdMeshAcquire(ss, err);
    if (cmd == "meshfolder")   return CmdMeshFolder(ss, err);
    if (cmd == "meshespurge")   return CmdMeshesPurge(ss, err);
    if (cmd == "mesheslegacy")   return CmdMeshesLegacy(ss, err);
    if (cmd == "meshflip")      return CmdMeshFlip(ss, err);
    if (cmd == "meshuv")        return CmdMeshUV(ss, err);
    if (cmd == "meshva")        return CmdMeshVA(ss, err);
    if (cmd == "meshlook")     return CmdMeshLook(ss, err);
    if (cmd == "meshesvbo")      return CmdMeshesVBO(ss, err);
    if (cmd == "objfaces")       return CmdObjFaces(ss, err);
    if (cmd == "meshrefs")      return CmdMeshRefs(ss, err);
    if (cmd == "w3dnoentry")  return CmdW3dNoEntry(ss, err);
    if (cmd == "matrename")   return CmdMatRename(ss, err);
    if (cmd == "meminfo")        return CmdMemInfoMeshes(ss, err, handled);
    handled = false;
    return false;
}
