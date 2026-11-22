// ============================================================================
//  W3dTestsLibs.cpp — comandos de harness de las LIBRERIAS EXTERNAS y los
//  PROXIES W3D (io/Libraries.h, objects/ProxyW3d.h, io/ExternalLibrary.h).
//
//  Comandos:
//    libcrear <carpeta> <archivo.w3d> <textura.png> [prefab <nombre>]
//        una LIBRERIA de prueba: prefabenemigo (el prefab "Enemigo": Esqueleto con dos clips + Cuerpo
//        skinneado + Sensor + enemigo.lua) y ademas: el Cuerpo con el material "Piel" CON TEXTURA (la png
//        dada, copiada a la carpeta), un prefab "Arbol" (un cubo "Tronco", recurso "Tronco", material
//        "Corteza"), un prefab "Lanzador" (un vacio con lanzador.lua, que al arrancar hace instantiate("Arbol") -el
//        de SU libreria- y deja en getShared("lanzado") el tipo de lo creado) y una ESCENA "Decorado" (dos cubos
//        "Roca" y "Piedra" que comparten el recurso "Roca").
//        Con 'prefab <nombre>' el prefab del personaje (y su objeto raiz) se llama asi en vez de "Enemigo".
//        La guarda en <carpeta>/<archivo.w3d> (queda abierta como proyecto).
//    libproxy <lib> <prefab|escena> <elem> <x> <y> <z> [veces N] [falla]
//        proxies nuevos (W3dProxyAdd: la puerta de Add y de soltar, con Ctrl+Z), N en fila cada 2 en x
//    libproxyinfo <proxy> [libreria L] [elemento E] [tipo prefab|escena] [generados N] [vacio 0|1]
//    libcompartido <lib> <prefab|escena> <elem> [proxies N] [mallas N] [animsets N] [materiales N] [texturas N]
//                  [scripts N] [hitboxes N]
//        TODOS los proxies de ese elemento (en todas las raices cargadas): cuantos recursos DISTINTOS usa lo
//        que generan (mallas, animsets, materiales de sus mallas, texturas de esos materiales, scripts por su
//        .lua), y que todos son de la LIBRERIA (malla con 'libreria', animset con entrada "lib:<lib>/", material
//        con 'libreria' y prefijo, textura y script con ruta "lib:<lib>/"). 'hitboxes' = cuantos hitbox genera
//        entre todos (cada proxy tiene los suyos). Carga antes las texturas de la cola diferida
//    libinfo <lib> [vinculada 0|1] [montada 0|1] [prefabs N] [escenas N] [cantidad N]
//    librecurso malla|material|animset|textura <nombre> [existe 0|1] [libreria L] [biblioteca 0|1] [color R G B]
//        un recurso por su nombre CON prefijo (la textura: su ruta "lib:..."); 'biblioteca' = si la
//        biblioteca DEL PROYECTO lo lista (los de las librerias no son del proyecto: 0); 'color' = el difuso
//        de un material (tolerancia 1e-3)
//    libcambiar <proxy> <lib> <prefab|escena> <elem> [falla]   W3dProxyChange (con Ctrl+Z)
//    libeditar <proxy> <ruta>
//        lo generado por un proxy es de SOLO LECTURA: ni Tab ni Mode > Edit entran en Edit Mode
//    libruta <ruta> [lee 0|1] [existe 0|1] [bytes N]   ReadFileBytes / FileExists de una ruta ("lib:...")
//    libdistintos <proxyA> <proxyB>
//        dos proxies de elementos HOMONIMOS de DOS librerias (las mismas entradas y los mismos nombres adentro de
//        cada .w3d): lo que generan NO comparte nada (otra malla, otro material, otra textura con otros bytes,
//        otro animset): los prefijos los separan
//    libpropsmat [lectura 0|1] [tarjeta proxy|prefab|-]
//        el panel de propiedades (la pestania 2, re-bindeada con el objeto activo): 'lectura' = la tarjeta Material
//        esta en SOLO LECTURA (el material de una libreria, o la parte de una malla que genera un proxy: el aviso
//        de la libreria a la vista y sin selector de material); 'tarjeta' = que tarjeta de instancia muestra
//        ("proxy": la de un Proxy W3D con su libreria y su elemento; "prefab": la de una instancia; "-": ninguna)
//    libjuegomin <carpeta> <lib.w3d> [prefab <nombre>] [proxies N]
//        el juego 3D minimo (juego3dmin) + la libreria VINCULADA + N proxies (3 por defecto; 0 = la libreria se usa
//        SOLO desde lua) de su "Enemigo" (o del prefab <nombre>; el segundo con el override vida=25) + un "Director"
//        (libdirector.lua) que JUGANDO instancia 5 mas con instantiate("<lib>/<prefab>", ...) y deja su veredicto en
//        getShared("juez") y en el log ("[librerias] OK creados=5 enemigos=8 scope=8 vida=95" con 3)
//    libjuegolog <carpeta> <nombre> [claves]...     corre el juego compilado y exige ese veredicto
//    libcrearextras <carpeta> <archivo.w3d> <textura.png> <otra.w3d>
//        una SEGUNDA libreria ("extras") que VINCULA a <otra.w3d> (una libreria ADENTRO de otra) y trae:
//          prefab "Cartel"  : un cubo "Tablero" con el material "Pintura" cuya textura queda AFUERA de su .w3d
//                             (fuera/cartel.png, "ext:") y un script que tambien vive afuera (fuera/externo.lua):
//                             hace importW3D() de un archivo SUELTO al lado de la libreria (anexos/piedra.w3d)
//                             y de una ENTRADA suya (extra/roca.w3d), y deja getShared("anexos"/"externo");
//          prefab "Escuadra": un proxy del "Enemigo" de <otra> (la de adentro) + un cubo "Viga" con la malla
//                             "<otra>/Tronco" (un recurso de la de adentro);
//          prefab "Sonoro"  : sonoro.lua hace sound() de uno SUELTO (sounds/bip.wav) y de una ENTRADA
//                             (sounds/entrada.wav) de SU libreria;
//          prefab "Llamador": llamador.lua hace instantiate("personajes/Arbol") (la de adentro, por el nombre de SU
//                             registro) y deja getShared("llamado"/"tronco");
//          escena "Panel"   : una escena UI "Tablerito" con dos imagenes: "Fondo" (textures/logo.png) e "Icono"
//                             (sin textura; su panel.lua le pone "textures/logo.png" con setTexture).
//    libmarca <proxy> <ruta|proxy> <icono|->   la MARCA del outliner despues del nombre (el candado de lo
//        generado por un proxy; "-" = ninguna). 'proxy' = el proxy mismo
//    libpropsmalla <pestania 2|3|4> [lectura 0|1] [intentar]
//        la malla ACTIVA en Properties: 'lectura' = sus filas de edicion (partes / grupos, mapas UV y capas /
//        modificadores, segun la pestania) estan ocultas y se ve el aviso de la libreria. 'intentar' llama a
//        las acciones igual (New Mesh Part / Add Vertex Group y Add UV Map / Remove de un modificador) y exige que
//        no cambien nada
//    libsonido <ruta> [cacheado 0|1]      sound() ya resolvio esa ruta (el cache de BindsJuego)
//    libtexcargar                         carga ya las texturas de la cola diferida (lo que despertaron los proxies)
//    libanidada <de> <nombre> [como G]    W3dLibsNested: con que nombre vive la libreria de adentro
//    libimagen <nombre> [textura T] [script S]   una Image2D (de cualquier lado del arbol): su textura y su script
//    libjuegoextras <carpeta> <extras.w3d>
//        el juego 3D minimo + la libreria "extras" VINCULADA (la de adentro NO: se monta oculta) + un proxy de
//        Cartel, Escuadra y Sonoro + un Director con el veredicto ("[librerias] OK externo=OK anexos=Piedra+Roca
//        enemigos=1 sonoro=listo")
//    libmover <de> <a>                    renombra un archivo de disco (una libreria que FALTA y despues vuelve)
// ============================================================================
#include "W3dLang.h"   // T(): el aviso del material sale en el idioma del sistema
#include "test/W3dTestsLibs.h"
#include "test/W3dScript.h"            // W3dRunCommand("prefabenemigo ...")
#include "objects/Objects.h"
#include "objects/InstancePrefab.h"
#include "objects/ProxyW3d.h"
#include "objects/Mesh.h"
#include "objects/MeshResource.h"
#include "objects/Materials.h"
#include "objects/Textures.h"
#include "objects/Armature.h"
#include "objects/Empty.h"
#include "animation/W3dAnimSet.h"
#include "io/Libraries.h"
#include "io/ExternalLibrary.h"
#include "io/Prefabs.h"
#include "io/PrefabsEditor.h"
#include "io/EditorRoots.h"
#include "io/ProjectResources.h"
#include "io/ProjectMeshes.h"
#include "io/SaveW3D.h"
#include "io/W3dResources.h"
#include "io/W3dStore.h"           // W3dStoreByName / W3dSplitLibPath
#include "importers/import_obj.h"      // LoadAllPendingTextures
#include "io/W3dContainer.h"           // W3dImportAsset / W3dExternalRefMark (la libreria "extras")
#include "io/UI2DFormat.h"
#include "objects/UI.h"
#include "objects/Image2D.h"
#include "script/BindsGame.h"          // W3dSoundCached
#include "objects/ObjectMode.h"        // W3dRenameObject
#include "variables.h"                 // w3dPath (la libreria "extras" se guarda y se re-guarda)
#include "ViewPorts/Outliner.h"        // OutlinerObjectMark
#include "WhiskUI/draw/icons.h"        // IconName
#include "ViewPorts/LayoutInput.h"     // LayoutToggleEditMode / LayoutModePick
#include "ViewPorts/Properties.h"      // libpropsmat: la tarjeta Material y la del proxy
#include "WhiskUI/Propieties/PropLabel.h"
#include "ViewPorts/LayoutTree.h"     // LayoutCompleteRoot
#include "ViewPorts/ViewPorts.h"      // ViewportRow / ViewportColumn (recorrer el layout)
#include "base/W3dInteractionState.h"
#include "script/W3dScript.h"
#include "W3dRoots.h"
#include "w3dFilesystem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>                      // fabsf (librecurso ... color)
#ifdef _WIN32
    #include <direct.h>                // _getcwd (las rutas relativas del harness)
    #define getcwd _getcwd
#else
    #include <unistd.h>                // getcwd (las rutas relativas del harness)
#endif
#include <set>
#include <vector>
#include <string>

namespace {

// un script DE LA LIBRERIA: instantiate("Arbol") nombra el Arbol de SU libreria (un proxy), no uno del proyecto
const char* kLuaLauncher =
    "-- un script de la libreria de prueba: instanciar() de un prefab de la MISMA libreria, por su nombre pelado\n"
    "function start()\n"
    "  local e = instantiate(\"Arbol\", 0, 5, 0)\n"
    "  setShared(\"lanzado\", e and getType(e) or \"nada\")\n"
    "end\n";

const char* kLuaDirector =
    "-- director de la prueba de librerias: instanciar() de un prefab de una LIBRERIA vinculada, con un juez\n"
    "properties = { lib = \"personajes\", elem = \"Enemigo\", base = 3, vida = 95 }\n"
    "local phase, wait, created = 0, 0, 0\n"
    "function update(dt)\n"
    "  if phase == 0 then\n"
    "    for i = 1, 5 do\n"
    "      local e = instantiate(property(\"lib\") .. \"/\" .. property(\"elem\"), i * 2, -20, 0, 90)\n"
    "      if e then created = created + 1 end\n"
    "    end\n"
    "    phase = 1\n"
    "    return\n"
    "  end\n"
    "  if phase == 1 then\n"
    "    wait = wait + 1\n"
    "    if wait < 5 then return end\n"
    "    local n = tonumber(property(\"base\")) + created\n"
    "    local ok = created == 5 and (getShared(\"enemigos\") or 0) == n and (getShared(\"scope_ok\") or 0) == n\n"
    "      and (getShared(\"vida_total\") or 0) == tonumber(property(\"vida\"))\n"
    "    info(string.format(\"[librerias] %s creados=%d enemigos=%d scope=%d vida=%d\", ok and \"OK\" or \"FALTA\",\n"
    "      created, getShared(\"enemigos\") or -1, getShared(\"scope_ok\") or -1, getShared(\"vida_total\") or -1))\n"
    "    setShared(\"juez\", ok and \"OK\" or \"FALTA\")\n"
    "    phase = 2\n"
    "    quit()\n"
    "  end\n"
    "end\n";

std::string Ent(long n) { char b[32]; sprintf(b, "%ld", n); return b; }
int TypeOf(const std::string& t) { return t == "scene" ? W3D_LIB_SCENE : W3D_LIB_PREFAB; }

bool Copy(const std::string& of, const std::string& a) {
    std::vector<unsigned char> d;
    if (!w3dFileSystem::ReadFileBytes(of, d)) return false;
    FILE* f = fopen(a.c_str(), "wb");
    if (!f) return false;
    if (!d.empty()) fwrite(&d[0], 1, d.size(), f);
    return fclose(f) == 0;
}
bool Write(const std::string& path, const char* text) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    fputs(text, f);
    return fclose(f) == 0;
}
std::string Absolute(const std::string& r) {
    if (!r.empty() && r[0] == '/') return r;
    char cwd[2048];
    const std::string d = getcwd(cwd, sizeof(cwd)) ? std::string(cwd) : std::string(".");
    return d + "/" + r;
}
ProxyW3d* Px(const std::string& n, std::string& err, const char* who) {
    Object* o = SceneCollection ? FindObjectByName(SceneCollection, n) : NULL;
    if (!o || o->getType() != ObjectType::proxy) { err = std::string(who) + ": no hay un proxy '" + n + "'"; return NULL; }
    return (ProxyW3d*)o;
}
int Count(Object* o) {
    int n = 0;
    for (size_t i = 0; i < o->Childrens.size(); i++) n += 1 + Count(o->Childrens[i]);
    return n;
}
void CollectType(Object* o, ObjectType t, std::vector<Object*>& out) {
    if (!o) return;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        if (o->Childrens[i]->getType() == t) out.push_back(o->Childrens[i]);
        CollectType(o->Childrens[i], t, out);
    }
}
void Script(Object* o, const char* path);   // (mas abajo)
// un cubo con su recurso de malla y un material nuevo (la libreria de prueba)
Mesh* Cube(Object* parent, const char* name, const char* resource, Material* mat) {
    Mesh* m = (Mesh*)NewMesh(MeshType(MeshType::cube), parent, false);
    if (!m) return NULL;
    m->SetNameObj(name);
    if (!m->mesh || m->mesh->name != resource) W3dMeshCreateResource(m, resource);
    if (mat) {
        for (size_t g = 0; g < m->materialsGroup.size(); g++) m->materialsGroup[g].material = mat;
        if (m->mesh) for (size_t g = 0; g < m->mesh->parts.size(); g++) W3dMeshResourceChangeMaterial(m->mesh, (int)g, mat);
    }
    return m;
}

// ---------------------------------------------------------------------------
bool CmdLibCreate(std::istringstream& ss, std::string& err) {
    std::string dir, file, png, k, character = "Enemigo"; ss >> dir >> file >> png;
    while (ss >> k) { if (k == "prefab") ss >> character; }
    if (dir.empty() || file.empty() || png.empty() || character.empty()) {
        err = "libcreate: uso: libcreate <carpeta> <archivo.w3d> <textura.png> [prefab <nombre>]"; return false;
    }
    std::string e2;
    if (!W3dRunCommand("prefabenemy " + dir, e2)) { err = "libcrear: " + e2; return false; }
    const std::string tex = dir + "/piel.png";
    if (!Copy(png, tex)) { err = "libcreate: no pude copiar la textura '" + png + "'"; return false; }
    std::string reason;
    // el CUERPO del prefab "Enemigo" con un material CON TEXTURA
    const int ie = W3dRootFind(W3D_ROOT_PREFAB, "Enemigo");
    if (ie < 0 || !W3dActivateRoot(ie, &reason)) { err = "libcreate: no pude abrir el prefab Enemigo " + reason; return false; }
    Mesh* body = (Mesh*)FindObjectByName(SceneCollection, "Cuerpo");
    if (!body || body->getType() != ObjectType::mesh) { err = "libcreate: el prefab no tiene Cuerpo"; return false; }
    Material* skin = new Material("Piel");
    skin->diffuse[0] = 0.9f; skin->diffuse[1] = 0.7f; skin->diffuse[2] = 0.6f;
    skin->texture = TextureTake(tex);
    skin->textureOn = true;
    if (!skin->texture) { err = "libcreate: no pude cargar la textura"; return false; }
    for (size_t g = 0; g < body->materialsGroup.size(); g++) body->materialsGroup[g].material = skin;
    if (body->mesh) for (size_t g = 0; g < body->mesh->parts.size(); g++) W3dMeshResourceChangeMaterial(body->mesh, (int)g, skin);
    // (otro NOMBRE para el prefab del personaje: el prefab y su objeto raiz, como si el usuario lo hubiera renombrado)
    if (character != "Enemigo") {
        Object* rootObj = FindObjectByName(SceneCollection, "Enemigo");
        std::string remained;
        if (!rootObj || !W3dRootRename(ie, character, &remained) || remained != character) {
            err = "libcreate: no pude renombrar el prefab Enemigo a '" + character + "'"; return false;
        }
        rootObj->SetNameObj(character);
        W3dRenamedPrefab("Enemigo", character);
    }
    // un prefab "Arbol": un cubo "Tronco" con material "Corteza"
    const int ia = W3dRootCreateAndOpen(W3D_ROOT_PREFAB, "Arbol");
    if (ia < 0 || SceneCollection->Childrens.empty()) { err = "libcreate: no pude crear el prefab Arbol"; return false; }
    Material* bark = new Material("Corteza");
    bark->diffuse[0] = 0.4f; bark->diffuse[1] = 0.25f; bark->diffuse[2] = 0.1f;
    if (!Cube(SceneCollection->Childrens[0], "Tronco", "Tronco", bark)) { err = "libcreate: NewMesh fallo"; return false; }
    // un prefab "Lanzador": su script instancia el Arbol de la MISMA libreria
    if (!Write(dir + "/lanzador.lua", kLuaLauncher)) { err = "libcreate: no pude escribir lanzador.lua"; return false; }
    const int il = W3dRootCreateAndOpen(W3D_ROOT_PREFAB, "Lanzador");
    if (il < 0 || SceneCollection->Childrens.empty()) { err = "libcreate: no pude crear el prefab Lanzador"; return false; }
    Script(SceneCollection->Childrens[0], "lanzador.lua");
    // una ESCENA "Decorado": dos cubos que COMPARTEN el recurso "Roca"
    const int id = W3dRootCreateAndOpen(W3D_ROOT_SCENE, "Decorado");
    if (id < 0) { err = "libcreate: no pude crear la escena Decorado"; return false; }
    Mesh* rock = Cube(NULL, "Roca", "Roca", bark);
    Mesh* stone = (Mesh*)NewMesh(MeshType(MeshType::cube), NULL, false);
    if (!rock || !stone) { err = "libcreate: NewMesh fallo"; return false; }
    stone->SetNameObj("Piedra");
    stone->pos = Vector3(3, 0, 0);
    if (!rock->mesh || !W3dMeshAssign(stone, rock->mesh)) { err = "libcreate: no pude compartir la Roca"; return false; }
    if (!W3dActivateRoot(W3dRootBlock(), &reason)) { err = "libcrear: " + reason; return false; }
    DeselectAll(); ActiveObj = NULL;
    // los .lua de prefabenemigo van por nombre pelado ("enemigo.lua"): con su ruta de la carpeta, el guardado los
    // mete ADENTRO de la libreria (una libreria de verdad trae sus scripts; asi el juego compilado los empaqueta)
    {
        const std::vector<W3dRootRow>& fs = W3dRoots();
        for (size_t f = 0; f < fs.size(); f++) {
            std::vector<Object*> stack;
            if (fs[f].root) stack.push_back(fs[f].root);
            while (!stack.empty()) {
                Object* o = stack.back(); stack.pop_back();
                for (size_t i = 0; i < o->Childrens.size(); i++) stack.push_back(o->Childrens[i]);
                if (!o->scriptData) continue;
                for (size_t k = 0; k < o->scriptData->scripts.size(); k++) {
                    std::string& r = o->scriptData->scripts[k].path;
                    if (!r.empty() && r[0] != '/' && r.find(':') == std::string::npos && w3dFileSystem::FileExists(dir + "/" + r))
                        r = dir + "/" + r;
                }
            }
        }
    }
    const std::string path = dir + "/" + file;
    if (!SaveW3D(path)) { err = "libcreate: no pude guardar '" + path + "'"; return false; }
    printf("      [libcrear] libreria '%s': prefabs %s (Piel con textura) y Arbol, escena Decorado\n", path.c_str(),
           character.c_str());
    return true;
}

bool CmdLibProxy(std::istringstream& ss, std::string& err) {
    std::string lib, type, elem, k; float x = 0, y = 0, z = 0; int times = 1; bool fault = false;
    ss >> lib >> type >> elem >> x >> y >> z;
    while (ss >> k) { if (k == "times") ss >> times; else if (k == "fails") fault = true; }
    InteractionMode = ObjectMode; state = editNavigation;
    for (int i = 0; i < times; i++) {
        std::string reason;
        ProxyW3d* px = W3dProxyAdd(lib, TypeOf(type), elem, Vector3(x + 2.0f * (float)i, y, z), &reason);
        if (fault) {
            if (px) { err = "libproxy: se creo y no debia"; return false; }
            printf("      [libproxy] no se crea (bien): %s\n", reason.c_str());
            return true;
        }
        if (!px) { err = "libproxy: " + reason; return false; }
        if (i + 1 == times) printf("      [libproxy] %d proxy(s) de '%s/%s' (el ultimo: '%s', %d objeto(s) generados)\n",
                                   times, lib.c_str(), elem.c_str(), px->name.c_str(), Count(px));
    }
    return true;
}

bool CmdLibProxyInfo(std::istringstream& ss, std::string& err) {
    std::string n, k; ss >> n;
    ProxyW3d* px = Px(n, err, "libproxyinfo"); if (!px) return false;
    const int gen = Count(px);
    printf("      [libproxyinfo] '%s': libreria '%s' elemento '%s' (%s) clave '%s' generados %d%s\n", n.c_str(),
           px->Lib().c_str(), px->Element().c_str(), px->ElementType() == W3D_LIB_SCENE ? "scene" : "prefab",
           px->prefab.c_str(), gen, px->notGenerated ? " VACIO" : "");
    while (ss >> k) {
        std::string v; ss >> v;
        if (k == "lib" && px->Lib() != v) { err = "libproxyinfo: la libreria es '" + px->Lib() + "'"; return false; }
        if (k == "element" && px->Element() != v) { err = "libproxyinfo: el elemento es '" + px->Element() + "'"; return false; }
        if (k == "type" && TypeOf(v) != px->ElementType()) { err = "libproxyinfo: otro tipo de elemento"; return false; }
        if (k == "generated" && gen != atoi(v.c_str())) { err = "libproxyinfo: genero " + Ent(gen); return false; }
        if (k == "empty" && (px->notGenerated ? 1 : 0) != atoi(v.c_str())) { err = "libproxyinfo: vacio = " + Ent(px->notGenerated ? 1 : 0); return false; }
    }
    return true;
}

// TODOS los proxies de 'clave' en el arbol 'o', tambien los de ADENTRO de lo que genera otro (una libreria de adentro)
void CollectProxies(Object* o, const std::string& key, std::vector<InstancePrefab*>& out) {
    if (!o) return;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        if (!h) continue;
        if (W3dIsInstanceType(h->getType()) && ((InstancePrefab*)h)->prefab == key) out.push_back((InstancePrefab*)h);
        CollectProxies(h, key, out);
    }
}
bool CmdLibShared(std::istringstream& ss, std::string& err) {
    std::string lib, type, elem, k; ss >> lib >> type >> elem;
    const std::string key = W3dLibsKey(lib, TypeOf(type), elem);
    std::vector<InstancePrefab*> v;
    {
        std::vector<Object*> roots;
        W3dRootsInOrder(roots);
        for (size_t r = 0; r < roots.size(); r++) CollectProxies(roots[r], key, v);
    }
    LoadAllPendingTextures();
    std::set<const void*> meshes, sets, mats, texs;
    std::set<std::string> luas;
    int hitboxes = 0;
    const std::string pre = "lib:" + lib + "/";
    for (size_t i = 0; i < v.size(); i++) {
        std::vector<Object*> ms, as, hs;
        CollectType(v[i], ObjectType::mesh, ms);
        CollectType(v[i], ObjectType::armature, as);
        CollectType(v[i], ObjectType::hitbox, hs);
        hitboxes += (int)hs.size();
        // los SCRIPTS de lo generado: el .lua es una entrada de la libreria (uno solo para todos los proxies; el
        // estado de cada uno es suyo)
        {
            std::vector<Object*> stack(1, (Object*)v[i]);
            while (!stack.empty()) {
                Object* o = stack.back(); stack.pop_back();
                for (size_t h = 0; h < o->Childrens.size(); h++) stack.push_back(o->Childrens[h]);
                if (!o->scriptData) continue;
                for (size_t s = 0; s < o->scriptData->scripts.size(); s++) {
                    const std::string& r = o->scriptData->scripts[s].path;
                    if (r.compare(0, pre.size(), pre) != 0) { err = "libshared: el script '" + r + "' no es de la libreria"; return false; }
                    luas.insert(r);
                }
            }
        }
        for (size_t q = 0; q < ms.size(); q++) {
            Mesh* m = (Mesh*)ms[q];
            if (!m->mesh || m->mesh->lib != lib) { err = "libshared: '" + m->name + "' no usa una malla de la libreria"; return false; }
            if (m->vertex != m->mesh->vertex) { err = "libshared: la geometria de '" + m->name + "' no es la de su recurso"; return false; }
            meshes.insert(m->mesh);
            for (size_t g = 0; g < m->materialsGroup.size(); g++) {
                Material* mt = m->materialsGroup[g].material;
                if (!mt || mt == MaterialDefault) continue;
                if (mt->lib != lib || mt->name.compare(0, lib.size() + 1, lib + "/") != 0) {
                    err = "libshared: el material '" + mt->name + "' no es de la libreria"; return false;
                }
                mats.insert(mt);
                if (mt->texture) {
                    if (mt->texture->path.compare(0, pre.size(), pre) != 0) { err = "libshared: la textura '" + mt->texture->path + "' no es de la libreria"; return false; }
                    texs.insert(mt->texture);
                }
            }
        }
        for (size_t q = 0; q < as.size(); q++) {
            Armature* a = (Armature*)as[q];
            if (a->animations.empty()) continue;
            W3dResource* r = W3dArmatureAnimSetResource(a);
            if (!r || r->id.compare(0, pre.size(), pre) != 0) { err = "libshared: el esqueleto de '" + v[i]->name + "' no usa un animset de la libreria"; return false; }
            sets.insert(r);
        }
    }
    printf("      [libcompartido] %s: %d proxy(s), %d malla(s), %d animset(s), %d material(es), %d textura(s), %d script(s), "
           "%d hitbox(es)\n", key.c_str(), (int)v.size(), (int)meshes.size(), (int)sets.size(), (int)mats.size(),
           (int)texs.size(), (int)luas.size(), hitboxes);
    while (ss >> k) {
        int val = -1; ss >> val;
        const int has = (k == "proxies") ? (int)v.size() : (k == "meshes") ? (int)meshes.size() : (k == "animsets") ? (int)sets.size()
                      : (k == "materials") ? (int)mats.size() : (k == "textures") ? (int)texs.size()
                      : (k == "scripts") ? (int)luas.size() : (k == "hitboxes") ? hitboxes : -999;
        if (has == -999) { err = "libshared: no se que es '" + k + "'"; return false; }
        if (has != val) { err = "libcompartido: " + k + " = " + Ent(has) + ", se esperaba " + Ent(val); return false; }
    }
    return true;
}

bool CmdLibInfo(std::istringstream& ss, std::string& err) {
    std::string lib, k; ss >> lib;
    const bool vinc = W3dLibsFind(lib) >= 0;
    const bool mont = W3dStoreByName(lib) != NULL;
    std::vector<std::string> ps, is;
    if (vinc) { W3dLibsElements(lib, W3D_LIB_PREFAB, ps); W3dLibsElements(lib, W3D_LIB_SCENE, is); }
    printf("      [libinfo] '%s': vinculada %d montada %d (%d vinculadas) prefabs %d escenas %d | archivo %s\n", lib.c_str(),
           vinc ? 1 : 0, (W3dStoreByName(lib) != NULL) ? 1 : 0, W3dLibsCount(), (int)ps.size(), (int)is.size(),
           vinc ? W3dLibsRow(W3dLibsFind(lib)).jsonPath.c_str() : "-");
    while (ss >> k) {
        int val = -1; ss >> val;
        if (k == "linked" && (vinc ? 1 : 0) != val) { err = "libinfo: vinculada = " + Ent(vinc ? 1 : 0); return false; }
        if (k == "mounted" && (mont ? 1 : 0) != val) { err = "libinfo: montada = " + Ent(mont ? 1 : 0); return false; }
        if (k == "prefabs" && (int)ps.size() != val) { err = "libinfo: prefabs = " + Ent((long)ps.size()); return false; }
        if (k == "scenes" && (int)is.size() != val) { err = "libinfo: escenas = " + Ent((long)is.size()); return false; }
        if (k == "count" && W3dLibsCount() != val) { err = "libinfo: vinculadas = " + Ent(W3dLibsCount()); return false; }
    }
    return true;
}

bool CmdLibResource(std::istringstream& ss, std::string& err) {
    std::string type, n, k; ss >> type >> n;
    bool exists = false; std::string lib;
    int view = -1;
    if (type == "mesh") {
        MeshResource* r = W3dMeshResourceByName(n);
        exists = r != NULL; if (r) lib = r->lib;
        view = W3D_VIEW_MESHES;
    } else if (type == "material") {
        Material* m = FindMaterialByName(n);
        exists = m != NULL; if (m) lib = m->lib;
        view = W3D_VIEW_MATERIALS;
    } else if (type == "animset") {
        exists = !W3dAnimSetsEntryOf(n).empty();
        if (W3dAnimSetIsFromLib(n)) { const size_t b = n.find('/'); lib = n.substr(0, b); }
        view = W3D_VIEW_ANIMATIONS;
    } else if (type == "texture") {
        Texture* t = TextureFind(n);
        exists = t != NULL;
        std::string l2, e2; if (W3dSplitLibPath(n, &l2, &e2)) lib = l2;
        view = W3D_VIEW_TEXTURES;
    } else { err = "libresource: malla|material|animset|textura"; return false; }
    bool inBib = false;
    { std::vector<W3dResourceItem> its; W3dLibraryList(-1, its);
      for (size_t i = 0; i < its.size(); i++) if (its[i].type == view && (its[i].id == n || its[i].name == n)) inBib = true; }
    printf("      [librecurso] %s '%s': existe %d libreria '%s' en la biblioteca del proyecto %d\n", type.c_str(), n.c_str(),
           exists ? 1 : 0, lib.c_str(), inBib ? 1 : 0);
    while (ss >> k) {
        std::string v; ss >> v;
        if (k == "color") {
            // (el color DIFUSO de un material: R G B, tolerancia 1e-3)
            Material* m = (type == "material") ? FindMaterialByName(n) : NULL;
            float c[3] = { (float)atof(v.c_str()), 0.0f, 0.0f };
            ss >> c[1] >> c[2];
            if (!m) { err = "libresource: 'color' es de un material que existe"; return false; }
            for (int q = 0; q < 3; q++)
                if (fabsf(m->diffuse[q] - c[q]) > 1e-3f) {
                    char b[160];
                    snprintf(b, sizeof(b), "libresource: el color de '%s' es (%.3f, %.3f, %.3f)", n.c_str(), m->diffuse[0],
                             m->diffuse[1], m->diffuse[2]);
                    err = b; return false;
                }
            continue;
        }
        if (k == "exists" && (exists ? 1 : 0) != atoi(v.c_str())) { err = "libresource: existe = " + Ent(exists ? 1 : 0); return false; }
        if (k == "lib" && lib != (v == "-" ? std::string() : v)) { err = "libresource: es de la libreria '" + lib + "'"; return false; }
        if (k == "library" && (inBib ? 1 : 0) != atoi(v.c_str())) { err = "libresource: en la biblioteca del proyecto = " + Ent(inBib ? 1 : 0); return false; }
    }
    return true;
}

bool CmdLibChange(std::istringstream& ss, std::string& err) {
    std::string n, lib, type, elem, k; ss >> n >> lib >> type >> elem;
    const bool fault = (ss >> k) && k == "fails";
    ProxyW3d* px = Px(n, err, "libchange"); if (!px) return false;
    std::string reason;
    const bool ok = W3dProxyChange(px, lib, TypeOf(type), elem, &reason);
    printf("      [libcambiar] '%s' -> '%s/%s': %s %s\n", n.c_str(), lib.c_str(), elem.c_str(), ok ? "change" : "NO", reason.c_str());
    if (ok == fault) { err = "libcambiar: " + std::string(ok ? "cambio y no debia" : "no cambio: " + reason); return false; }
    return true;
}

bool CmdLibEdit(std::istringstream& ss, std::string& err) {
    std::string n, path; ss >> n >> path;
    ProxyW3d* px = Px(n, err, "libedit"); if (!px) return false;
    Object* r = px->RootGenerated();
    Object* o = r ? W3dHierNode(r, path) : NULL;
    if (!o) { err = "libedit: el proxy no genero '" + path + "'"; return false; }
    InteractionMode = ObjectMode; state = editNavigation;
    DeselectAll();
    o->Select();
    LayoutToggleEditMode();
    const bool tab = (InteractionMode == EditMode);
    InteractionMode = ObjectMode;
    LayoutModePick(EditMode);
    const bool menu = (InteractionMode == EditMode);
    InteractionMode = ObjectMode;
    UpdateActiveEditMesh();
    printf("      [libeditar] '%s' (%s): Tab %s, Mode > Edit %s\n", path.c_str(), o->name.c_str(),
           tab ? "ENTRO" : "no entra", menu ? "ENTRO" : "no entra");
    if (tab || menu) { err = "libedit: lo generado por un proxy entro en Edit Mode"; return false; }
    return true;
}

bool CmdLibPath(std::istringstream& ss, std::string& err) {
    std::string r, k; ss >> r;
    std::vector<unsigned char> d;
    const bool lee = w3dFileSystem::ReadFileBytes(r, d);
    const bool exists = w3dFileSystem::FileExists(r);
    printf("      [libruta] '%s': lee %d (%d bytes) existe %d\n", r.c_str(), lee ? 1 : 0, (int)d.size(), exists ? 1 : 0);
    while (ss >> k) {
        int v = -1; ss >> v;
        if (k == "lee" && (lee ? 1 : 0) != v) { err = "libpath: lee = " + Ent(lee ? 1 : 0); return false; }
        if (k == "exists" && (exists ? 1 : 0) != v) { err = "libpath: existe = " + Ent(exists ? 1 : 0); return false; }
        if (k == "bytes" && (int)d.size() != v) { err = "libruta: " + Ent((long)d.size()) + " bytes"; return false; }
    }
    return true;
}

// la primera malla generada por un proxy y lo que usa
struct UseMesh { Mesh* m; Material* mat; Texture* tex; W3dResource* set; };
bool UseOf(ProxyW3d* px, UseMesh& u, std::string& err) {
    std::vector<Object*> ms, as;
    CollectType(px, ObjectType::mesh, ms);
    CollectType(px, ObjectType::armature, as);
    if (ms.empty() || as.empty()) { err = "libdistinct: '" + px->name + "' no genero malla y esqueleto"; return false; }
    u.m = (Mesh*)ms[0];
    u.mat = u.m->materialsGroup.empty() ? NULL : u.m->materialsGroup[0].material;
    u.tex = u.mat ? u.mat->texture : NULL;
    u.set = W3dArmatureAnimSetResource((Armature*)as[0]);
    return true;
}
bool CmdLibDistinct(std::istringstream& ss, std::string& err) {
    std::string a, b; ss >> a >> b;
    ProxyW3d* pa = Px(a, err, "libdistinct"); if (!pa) return false;
    ProxyW3d* pb = Px(b, err, "libdistinct"); if (!pb) return false;
    LoadAllPendingTextures();
    UseMesh ua, ub;
    if (!UseOf(pa, ua, err) || !UseOf(pb, ub, err)) return false;
    std::vector<unsigned char> ba, bb;
    if (ua.tex) w3dFileSystem::ReadFileBytes(ua.tex->path, ba);
    if (ub.tex) w3dFileSystem::ReadFileBytes(ub.tex->path, bb);
    printf("      [libdistintos] '%s' malla %s mat %s tex %s (%d B) | '%s' malla %s mat %s tex %s (%d B)\n",
           a.c_str(), ua.m->mesh ? ua.m->mesh->name.c_str() : "-", ua.mat ? ua.mat->name.c_str() : "-",
           ua.tex ? ua.tex->path.c_str() : "-", (int)ba.size(),
           b.c_str(), ub.m->mesh ? ub.m->mesh->name.c_str() : "-", ub.mat ? ub.mat->name.c_str() : "-",
           ub.tex ? ub.tex->path.c_str() : "-", (int)bb.size());
    if (!ua.m->mesh || ua.m->mesh == ub.m->mesh || ua.m->vertex == ub.m->vertex) { err = "libdistinct: comparten la malla"; return false; }
    if (!ua.mat || ua.mat == ub.mat) { err = "libdistinct: comparten el material"; return false; }
    if (!ua.tex || ua.tex == ub.tex || ba == bb) { err = "libdistinct: comparten la textura (o sus bytes)"; return false; }
    if (!ua.set || ua.set == ub.set) { err = "libdistinct: comparten el animset"; return false; }
    return true;
}

ViewportBase* LeafProps(ViewportBase* n) {
    if (!n) return NULL;
    if (n->isLeaf()) return n->ViewportKind() == 3 ? n : NULL;
    ViewportBase* a = NULL; ViewportBase* b = NULL;
    if (n->ContainerKind() == 1) { a = ((ViewportRow*)n)->childA; b = ((ViewportRow*)n)->childB; }
    else { a = ((ViewportColumn*)n)->childA; b = ((ViewportColumn*)n)->childB; }
    ViewportBase* r = LeafProps(a);
    return r ? r : LeafProps(b);
}
bool CmdLibPropsMat(std::istringstream& ss, std::string& err) {
    Properties* p = (Properties*)LeafProps(LayoutCompleteRoot());
    if (!p) { err = "libpropsmat: no hay panel de propiedades"; return false; }
    p->RefreshTargetProperties();
    p->UpdateTabs();
    PropsActive = p;
    if (p->activeTab != 2 && p->BarTabs.size() > 2 && p->BarTabs[2]->visible) { p->activeTab = 2; p->UpdateTabs(); }
    rootViewport->Render();
    if (ActiveObj && ActiveObj->getType() == ObjectType::mesh) p->Rebind();
    const bool reading = p->propMsgDefault && !p->propMsgDefault->hidden && p->propMsgDefault->name == T("It belongs to a library (read-only): open the library to edit it") &&
                         p->propBtnNewMaterial && p->propBtnNewMaterial->hidden;
    std::string card = "-";
    if (p->propPrefab && p->propPrefab->visible)
        card = (p->propPxLib && !p->propPxLib->hidden) ? "proxy" : "prefab";
    printf("      [libpropsmat] '%s' (pestania %d): material %s | tarjeta %s%s%s\n", ActiveObj ? ActiveObj->name.c_str() : "-",
           p->activeTab, reading ? "de SOLO LECTURA" : "editable", card.c_str(),
           card == "proxy" ? (" libreria '" + p->propPxLib->button->text + "'").c_str() : "",
           card != "-" ? (" elemento '" + p->propPfSel->button->text + "'").c_str() : "");
    std::string k, v;
    while (ss >> k >> v) {
        if (k == "lectura" && (reading ? 1 : 0) != atoi(v.c_str())) { err = "libpropsmat: lectura = " + Ent(reading ? 1 : 0); return false; }
        if (k == "card" && card != v) { err = "libpropsmat: la tarjeta es '" + card + "'"; return false; }
    }
    return true;
}

void Script(Object* o, const char* path) {
    if (!o->scriptData) o->scriptData = new W3dScriptData();
    W3dScriptEntry e; e.path = path;
    o->scriptData->scripts.push_back(e);
}

bool CmdLibGameMin(std::istringstream& ss, std::string& err) {
    std::string dir, libW3d, k, elem = "Enemigo"; ss >> dir >> libW3d;
    int nNext = 3;
    while (ss >> k) { if (k == "prefab") ss >> elem; else if (k == "proxies") ss >> nNext; }
    if (nNext < 0) nNext = 0;
    if (dir.empty() || libW3d.empty() || elem.empty()) { err = "libgamemin: uso: libgamemin <carpeta> <lib.w3d> [prefab <nombre>]"; return false; }
    std::string e2;
    if (!W3dRunCommand("game3dmin " + dir, e2)) { err = "libjuegomin: " + e2; return false; }
    if (!Write(dir + "/libdirector.lua", kLuaDirector)) { err = "libgamemin: no pude escribir el director"; return false; }
    std::string reason;
    if (!W3dExtLibLink(Absolute(libW3d), &reason)) { err = "libgamemin: no se vinculo: " + reason; return false; }
    const std::string lib = W3dLibsRow(W3dLibsCount() - 1).name;
    InteractionMode = ObjectMode; state = editNavigation;
    for (int i = 0; i < nNext; i++) {
        ProxyW3d* px = W3dProxyAdd(lib, W3D_LIB_PREFAB, elem, Vector3(-4.0f + 4.0f * (float)i, 0.0f, -6.0f), &reason);
        if (!px) { err = "libjuegomin: " + reason; return false; }
        if (i == 1) W3dPrefabOverrideProp(px, "vida", "25");   // (el override de una propiedad, como en una instancia)
    }
    Empty* d = new Empty(NULL, Vector3(0, -10, 0));
    d->SetNameObj("Director");
    Script(d, "libdirector.lua");
    d->scriptData->scripts.back().refs.push_back(std::make_pair(std::string("lib"), lib));
    d->scriptData->scripts.back().refs.push_back(std::make_pair(std::string("elem"), elem));
    // lo que el director espera: los proxies de arranque (cada uno vida 10; el del medio 25) + 5 instanciados por lua
    const int life = 10 * nNext + (nNext >= 2 ? 15 : 0) + 50;
    d->scriptData->scripts.back().refs.push_back(std::make_pair(std::string("base"), Ent(nNext)));
    d->scriptData->scripts.back().refs.push_back(std::make_pair(std::string("vida"), Ent(life)));
    DeselectAll(); ActiveObj = NULL;
    printf("      [libjuegomin] juego3dmin + libreria '%s' + %d proxies de su %s (el del medio con vida=25) + Director en '%s'\n",
           lib.c_str(), nNext, elem.c_str(), dir.c_str());
    return true;
}

bool CmdLibGameLog(std::istringstream& ss, std::string& err) {
    std::string dir, name, k; ss >> dir >> name;
    const std::string folder = dir + "/build/linux";
    const std::string bin = folder + "/" + name;
    if (!w3dFileSystem::FileExists(bin)) { err = "libgamelog: no existe el binario compilado '" + bin + "'"; return false; }
    const std::string log = folder + "/whisk3d.log";
    remove(log.c_str());
    char cmdRun[2200];
    snprintf(cmdRun, sizeof(cmdRun), "cd \"%s\" && timeout 120 ./%s > /dev/null 2>&1", folder.c_str(), name.c_str());
    const int r = system(cmdRun);
    FILE* f = fopen(log.c_str(), "rb");
    if (!f) { err = "libgamelog: el juego no dejo whisk3d.log (se compilo en modo debug?)"; return false; }
    std::string verdict;
    char buf[2048];
    while (fgets(buf, sizeof(buf), f)) {
        const char* p = strstr(buf, "[librerias] ");
        if (p && (strstr(p, " OK ") || strstr(p, " FALTA ")) && (strstr(p, "creados=") || strstr(p, "externo="))) verdict = p;
    }
    fclose(f);
    while (!verdict.empty() && (verdict[verdict.size() - 1] == '\n' || verdict[verdict.size() - 1] == '\r'))
        verdict.erase(verdict.size() - 1);
    printf("      [libjuegolog] salida=%d | %s\n", r, verdict.empty() ? "(sin veredicto del director)" : verdict.c_str());
    if (verdict.find("[librerias] OK") == std::string::npos) {
        err = "libgamelog: en el juego compilado los proxies / instanciar() de la libreria no dieron lo mismo que en el Play";
        return false;
    }
    while (ss >> k) if (verdict.find(k) == std::string::npos) { err = "libgamelog: el veredicto no dice '" + k + "'"; return false; }
    return true;
}

// ---------------------------------------------------------------------------
//  LA LIBRERIA "extras": lo de adentro de una libreria (afuera de su .w3d, sueltos, UI, librerias de adentro)
// ---------------------------------------------------------------------------
const char* kLuaExternal =
    "-- un script de la libreria que vive AFUERA de su .w3d: lo que nombra por ruta es de SU libreria\n"
    "function start()\n"
    "  local a = importW3D(\"anexos/piedra.w3d\")   -- suelto al lado del .w3d de la libreria\n"
    "  local b = importW3D(\"extra/roca.w3d\")      -- una entrada de la libreria\n"
    "  setShared(\"anexos\", (a and getName(a) or \"nada\") .. \"+\" .. (b and getName(b) or \"nada\"))\n"
    "  setShared(\"externo\", \"OK\")\n"
    "end\n";
const char* kLuaSound =
    "-- sonido() de un script de la libreria: primero lo de SU libreria\n"
    "function start()\n"
    "  sound(\"sounds/bip.wav\")       -- suelto al lado de la libreria\n"
    "  sound(\"sounds/entrada.wav\")   -- una entrada de la libreria\n"
    "  setShared(\"sonoro\", \"listo\")\n"
    "end\n";
const char* kLuaCaller =
    "-- un script de la libreria instancia un prefab de la libreria de ADENTRO, con el nombre de SU registro\n"
    "function start()\n"
    "  local e = instantiate(\"personajes/Arbol\", 0, 5, 0)\n"
    "  setShared(\"llamado\", e and getType(e) or \"nada\")\n"
    "  setShared(\"tronco\", (e and find(\"Tronco\", e)) and \"si\" or \"no\")\n"
    "end\n";
const char* kLuaPanel =
    "-- la imagen de la escena UI de la libreria: su textura es de SU libreria\n"
    "function start() setTexture(self(), \"textures/logo.png\") end\n";
const char* kLuaDirectorExtras =
    "-- director de la prueba de librerias (2): lo de adentro de una libreria, en el juego\n"
    "local wait = 0\n"
    "function update(dt)\n"
    "  wait = wait + 1\n"
    "  if wait < 5 then return end\n"
    "  if wait > 5 then return end\n"
    "  local ok = getShared(\"externo\") == \"OK\" and getShared(\"anexos\") == \"Piedra+Roca\"\n"
    "    and (getShared(\"enemigos\") or 0) == 1 and getShared(\"sonoro\") == \"listo\"\n"
    "  info(string.format(\"[librerias] %s externo=%s anexos=%s enemigos=%d sonoro=%s\", ok and \"OK\" or \"FALTA\",\n"
    "    tostring(getShared(\"externo\")), tostring(getShared(\"anexos\")), getShared(\"enemigos\") or -1,\n"
    "    tostring(getShared(\"sonoro\"))))\n"
    "  setShared(\"juez\", ok and \"OK\" or \"FALTA\")\n"
    "  quit()\n"
    "end\n";
// un .w3d v3 plano (lo unico que importW3D anexa) con un objeto
std::string W3dPlane(const char* name) {
    return std::string("{\n  \"scene\": { \"objects\": [ { \"type\": \"object\", \"name\": \"") + name + "\" } ] }\n}\n";
}
bool Folder(const std::string& d) {
    const std::string c = "mkdir -p \"" + d + "\"";
    return system(c.c_str()) == 0;
}

bool CmdLibCreateExtras(std::istringstream& ss, std::string& err) {
    std::string dir0, file, png, other; ss >> dir0 >> file >> png >> other;
    if (dir0.empty() || file.empty() || png.empty() || other.empty()) {
        err = "libcreateextras: uso: libcreateextras <carpeta> <archivo.w3d> <textura.png> <otra.w3d>"; return false;
    }
    const std::string dir = Absolute(dir0);
    std::string e2, reason;
    if (!W3dRunCommand("scenedefault", e2)) { err = "libcrearextras: " + e2; return false; }
    w3dPath = "";
    if (!Folder(dir + "/fuera") || !Folder(dir + "/sounds") || !Folder(dir + "/anexos")) { err = "libcreateextras: no pude crear las carpetas"; return false; }
    // los archivos: lo que queda AFUERA de su .w3d (fuera/), los SUELTOS que nombran sus scripts (sounds/, anexos/)
    // y lo que entra (logo.png por la UI; entrada.wav y roca.w3d importados; los .lua de la carpeta)
    const std::string bip = "sounds/bip.wav";
    std::string w1 = W3dPlane("Piedra"), w2 = W3dPlane("Roca");
    if (!Copy(png, dir + "/logo.png") || !Copy(png, dir + "/fuera/cartel.png") ||
        !Copy(bip, dir + "/sounds/bip.wav") || !Copy(bip, dir + "/entrada.wav") ||
        !Write(dir + "/anexos/piedra.w3d", w1.c_str()) || !Write(dir + "/roca.w3d", w2.c_str()) ||
        !Write(dir + "/fuera/externo.lua", kLuaExternal) || !Write(dir + "/sonoro.lua", kLuaSound) ||
        !Write(dir + "/panel.lua", kLuaPanel) || !Write(dir + "/llamador.lua", kLuaCaller)) {
        err = "libcreateextras: no pude escribir los archivos"; return false;
    }
    // la libreria de ADENTRO (vinculada en ESTA libreria)
    if (!W3dExtLibLink(Absolute(other), &reason)) { err = "libcreateextras: no se vinculo la de adentro: " + reason; return false; }
    const std::string sub = W3dLibsRow(W3dLibsCount() - 1).name;
    // prefab "Cartel": la textura y el script AFUERA del .w3d
    const int ic = W3dRootCreateAndOpen(W3D_ROOT_PREFAB, "Cartel");
    if (ic < 0 || SceneCollection->Childrens.empty()) { err = "libcreateextras: no pude crear el prefab Cartel"; return false; }
    Material* paint = new Material("Pintura");
    paint->texture = TextureTake(dir + "/fuera/cartel.png");
    paint->textureOn = true;
    if (!paint->texture) { err = "libcreateextras: no pude cargar la textura de afuera"; return false; }
    W3dExternalRefMark(dir + "/fuera/cartel.png");
    if (!Cube(SceneCollection->Childrens[0], "Tablero", "Tablero", paint)) { err = "libcreateextras: NewMesh fallo"; return false; }
    Script(SceneCollection->Childrens[0], (dir + "/fuera/externo.lua").c_str());
    W3dExternalRefMark(dir + "/fuera/externo.lua");
    // prefab "Escuadra": un proxy de la de ADENTRO + un cubo con una malla suya
    const int ie = W3dRootCreateAndOpen(W3D_ROOT_PREFAB, "Escuadra");
    if (ie < 0 || SceneCollection->Childrens.empty()) { err = "libcreateextras: no pude crear el prefab Escuadra"; return false; }
    Object* rootE = SceneCollection->Childrens[0];
    ProxyW3d* nested = W3dProxyCreate(sub, W3D_LIB_PREFAB, "Enemigo", rootE, Vector3(0, 0, 0), 0.0f);
    if (!nested || nested->Childrens.empty()) { err = "libcreateextras: el proxy de la de adentro no genero nada"; return false; }
    Mesh* beam = Cube(rootE, "Viga", "Viga", NULL);
    MeshResource* trunk = W3dMeshResourceByName(sub + "/Tronco");
    if (!beam || !trunk || !W3dMeshAssign(beam, trunk)) { err = "libcreateextras: no pude usar la malla '" + sub + "/Tronco'"; return false; }
    beam->pos = Vector3(3, 0, 0);
    // prefab "Sonoro"
    const int is = W3dRootCreateAndOpen(W3D_ROOT_PREFAB, "Sonoro");
    if (is < 0 || SceneCollection->Childrens.empty()) { err = "libcreateextras: no pude crear el prefab Sonoro"; return false; }
    Script(SceneCollection->Childrens[0], (dir + "/sonoro.lua").c_str());
    // prefab "Llamador": instancia el Arbol de la de ADENTRO ("personajes/Arbol": el nombre de SU registro)
    const int il = W3dRootCreateAndOpen(W3D_ROOT_PREFAB, "Llamador");
    if (il < 0 || SceneCollection->Childrens.empty()) { err = "libcreateextras: no pude crear el prefab Llamador"; return false; }
    Script(SceneCollection->Childrens[0], (dir + "/llamador.lua").c_str());
    // escena "Panel": una escena UI
    const int ip = W3dRootCreateAndOpen(W3D_ROOT_SCENE, "Panel");
    if (ip < 0) { err = "libcreateextras: no pude crear la escena Panel"; return false; }
    UI* u = new UI(NULL);
    W3dRenameObject(u, "Tablerito", false);
    Image2D* background = new Image2D(u);
    W3dRenameObject(background, "Fondo", false);
    background->texture = dir + "/logo.png";
    Image2D* icon = new Image2D(u);
    W3dRenameObject(icon, "Icono", false);
    Script(icon, (dir + "/panel.lua").c_str());
    if (!W3dActivateRoot(W3dRootBlock(), &reason)) { err = "libcrearextras: " + reason; return false; }
    DeselectAll(); ActiveObj = NULL;
    const std::string path = dir + "/" + file;
    if (!SaveW3D(path)) { err = "libcreateextras: no pude guardar '" + path + "'"; return false; }
    w3dPath = path;
    // lo IMPORTADO a la libreria (entradas que ningun objeto nombra: las nombran sus scripts)
    const std::string ew = W3dImportAsset(dir + "/entrada.wav"), er = W3dImportAsset(dir + "/roca.w3d");
    if (ew != "sounds/entrada.wav" || er != "extra/roca.w3d") { err = "libcreateextras: importo '" + ew + "' y '" + er + "'"; return false; }
    if (!SaveW3D(path)) { err = "libcreateextras: no pude re-guardar '" + path + "'"; return false; }
    w3dPath = "";   // (como libcrear: la libreria no queda como "el proyecto abierto" de lo que sigue)
    printf("      [libcrearextras] '%s' (vincula '%s'): Cartel (textura y script afuera), Escuadra (proxy de adentro), "
           "Sonoro, escena Panel (UI)\n", path.c_str(), sub.c_str());
    return true;
}

bool CmdLibMark(std::istringstream& ss, std::string& err) {
    std::string n, path, ic; ss >> n >> path >> ic;
    ProxyW3d* px = Px(n, err, "libmark"); if (!px) return false;
    Object* o = px;
    if (path != "proxy") {
        Object* r = px->RootGenerated();
        o = r ? (path == "." ? r : W3dHierNode(r, path)) : NULL;
        if (!o) { err = "libmark: el proxy no genero '" + path + "'"; return false; }
    }
    const int m = OutlinerObjectMark(o);
    const std::string name = (m >= 0) ? IconName(m) : "-";
    printf("      [libmarca] '%s' (%s): marca '%s'\n", o->name.c_str(), path.c_str(), name.c_str());
    if (name != ic) { err = "libmark: la marca es '" + name + "', se esperaba '" + ic + "'"; return false; }
    return true;
}

bool CmdLibPropsMesh(std::istringstream& ss, std::string& err) {
    int tab = -1; ss >> tab;
    Properties* p = (Properties*)LeafProps(LayoutCompleteRoot());
    if (!p) { err = "libpropsmesh: no hay panel de propiedades"; return false; }
    if (!ActiveObj || ActiveObj->getType() != ObjectType::mesh) { err = "libpropsmesh: el activo no es una malla"; return false; }
    Mesh* m = (Mesh*)ActiveObj;
    PropsActive = p;
    p->RefreshTargetProperties();
    p->activeTab = tab;
    p->UpdateTabs();
    p->Resize(p->width, p->height);
    rootViewport->Render();
    p->UpdateTabs();
    bool reading = false;
    if (tab == 2) {
        bool rows = false;
        if (p->propRowPartOps) for (size_t b = 0; b < p->propRowPartOps->buttons.size(); b++) rows |= p->propRowPartOps->buttons[b]->visible;
        if (p->propRowDelRow) for (size_t b = 0; b < p->propRowDelRow->buttons.size(); b++) rows |= p->propRowDelRow->buttons[b]->visible;
        reading = p->propBtnNewPart && p->propBtnNewPart->hidden && !rows;
    } else if (tab == 3) {
        reading = p->propLblMeshLib && !p->propLblMeshLib->hidden && p->propBtnAddVG && p->propBtnAddVG->hidden &&
                  p->propBtnAddUV && p->propBtnAddUV->hidden && p->propBtnAddCol && p->propBtnAddCol->hidden &&
                  !(p->propMeshEdit && p->propMeshEdit->visible);
    } else if (tab == 4) {
        reading = p->propLblModsLib && !p->propLblModsLib->hidden && p->propRowMod && !p->propRowMod->buttons.empty() &&
                  !p->propRowMod->buttons[0]->visible;
    } else { err = "libpropsmesh: pestania 2, 3 o 4"; return false; }
    printf("      [libpropsmalla] '%s' pestania %d: %s\n", m->name.c_str(), tab, reading ? "SOLO LECTURA" : "editable");
    std::string k;
    while (ss >> k) {
        if (k == "lectura") {
            int v = -1; ss >> v;
            if ((reading ? 1 : 0) != v) { err = "libpropsmesh: lectura = " + Ent(reading ? 1 : 0); return false; }
        } else if (k == "try") {
            // las ACCIONES tambien estan cerradas (no solo sus botones): nada cambia
            const size_t parts = m->materialsGroup.size(), groups = m->vertexGroups.size(), uvs = m->uvMaps.size(),
                         mods = m->modifiers.size();
            if (tab == 2 && p->propBtnNewPart && p->propBtnNewPart->action) p->propBtnNewPart->action();
            if (tab == 3) {
                if (p->propBtnAddVG && p->propBtnAddVG->action) p->propBtnAddVG->action();
                if (p->propBtnAddUV && p->propBtnAddUV->action) p->propBtnAddUV->action();
            }
            if (tab == 4 && p->propRowMod && p->propRowMod->actions.size() > 1 && p->propRowMod->actions[1]) p->propRowMod->actions[1]();
            printf("      [libpropsmalla] intentar: partes %d->%d grupos %d->%d uv %d->%d modificadores %d->%d\n",
                   (int)parts, (int)m->materialsGroup.size(), (int)groups, (int)m->vertexGroups.size(), (int)uvs,
                   (int)m->uvMaps.size(), (int)mods, (int)m->modifiers.size());
            if (parts != m->materialsGroup.size() || groups != m->vertexGroups.size() || uvs != m->uvMaps.size() ||
                mods != m->modifiers.size()) { err = "libpropsmesh: una accion edito una malla de solo lectura"; return false; }
        } else { err = "libpropsmesh: no entiendo '" + k + "'"; return false; }
    }
    return true;
}

bool CmdLibSound(std::istringstream& ss, std::string& err) {
    std::string r, k; ss >> r;
    if (r.compare(0, 4, "lib:") != 0) r = Absolute(r);
    const bool c = W3dSoundCached(r);
    printf("      [libsonido] '%s': %s\n", r.c_str(), c ? "cached" : "no");
    while (ss >> k) {
        int v = -1; ss >> v;
        if (k == "cached" && (c ? 1 : 0) != v) { err = "libsound: cacheado = " + Ent(c ? 1 : 0); return false; }
    }
    return true;
}

bool CmdLibMove(std::istringstream& ss, std::string& err) {
    std::string of, a; ss >> of >> a;
    if (of.empty() || a.empty()) { err = "libmove: uso: libmove <de> <a>"; return false; }
    if (rename(of.c_str(), a.c_str()) != 0) { err = "libmove: no pude mover '" + of + "' a '" + a + "'"; return false; }
    printf("      [libmover] '%s' -> '%s'\n", of.c_str(), a.c_str());
    return true;
}

bool CmdLibTexLoad() {
    LoadAllPendingTextures();
    printf("      [libtexcargar] texturas de la cola diferida cargadas\n");
    return true;
}

bool CmdLibNested(std::istringstream& ss, std::string& err) {
    std::string of, n, k, v; ss >> of >> n;
    const std::string g = W3dLibsNested(of, n);
    printf("      [libanidada] '%s' de adentro de '%s' -> '%s' (%s)\n", n.c_str(), of.c_str(), g.c_str(),
           W3dLibsDiskPathOf(g).c_str());
    while (ss >> k >> v)
        if (k == "as" && g != v) { err = "libnested: vive como '" + g + "'"; return false; }
    return true;
}

bool CmdLibImage(std::istringstream& ss, std::string& err) {
    std::string n, k, v; ss >> n;
    Object* o = SceneCollection ? FindObjectByName(SceneCollection, n) : NULL;
    if (!o || o->getType() != ObjectType::image2d) { err = "libimage: no hay una imagen 2D '" + n + "'"; return false; }
    Image2D* im = (Image2D*)o;
    const std::string sc = (im->scriptData && !im->scriptData->scripts.empty()) ? im->scriptData->scripts[0].path : std::string();
    printf("      [libimagen] '%s': textura '%s' script '%s'\n", n.c_str(), im->texture.c_str(), sc.c_str());
    while (ss >> k >> v) {
        if (v == "-") v.clear();
        if (k == "texture" && im->texture != v) { err = "libimage: la textura es '" + im->texture + "'"; return false; }
        if (k == "script" && sc != v) { err = "libimage: el script es '" + sc + "'"; return false; }
    }
    return true;
}

bool CmdLibGameExtras(std::istringstream& ss, std::string& err) {
    std::string dir, extras; ss >> dir >> extras;
    if (dir.empty() || extras.empty()) { err = "libgameextras: uso: libgameextras <carpeta> <extras.w3d>"; return false; }
    std::string e2, reason;
    if (!W3dRunCommand("game3dmin " + dir, e2)) { err = "libjuegoextras: " + e2; return false; }
    if (!Write(dir + "/libdirector2.lua", kLuaDirectorExtras)) { err = "libgameextras: no pude escribir el director"; return false; }
    if (!W3dExtLibLink(Absolute(extras), &reason)) { err = "libgameextras: no se vinculo: " + reason; return false; }
    const std::string lib = W3dLibsRow(W3dLibsCount() - 1).name;
    InteractionMode = ObjectMode; state = editNavigation;
    const char* elems[3] = { "Cartel", "Escuadra", "Sonoro" };
    for (int i = 0; i < 3; i++)
        if (!W3dProxyAdd(lib, W3D_LIB_PREFAB, elems[i], Vector3(-4.0f + 4.0f * (float)i, 0.0f, -6.0f), &reason)) {
            err = "libjuegoextras: " + reason; return false;
        }
    Empty* d = new Empty(NULL, Vector3(0, -10, 0));
    d->SetNameObj("Director");
    Script(d, "libdirector2.lua");
    DeselectAll(); ActiveObj = NULL;
    printf("      [libjuegoextras] juego3dmin + libreria '%s' (Cartel, Escuadra, Sonoro) + Director en '%s'\n", lib.c_str(), dir.c_str());
    return true;
}

} // namespace

bool W3dTestsLibsCmd(const std::string& cmd, std::istringstream& ss, std::string& err, bool& handled) {
    handled = true;
    if (cmd == "libcreate")       return CmdLibCreate(ss, err);
    if (cmd == "libproxy")       return CmdLibProxy(ss, err);
    if (cmd == "libproxyinfo")   return CmdLibProxyInfo(ss, err);
    if (cmd == "libshared")  return CmdLibShared(ss, err);
    if (cmd == "libinfo")        return CmdLibInfo(ss, err);
    if (cmd == "libresource")     return CmdLibResource(ss, err);
    if (cmd == "libchange")     return CmdLibChange(ss, err);
    if (cmd == "libedit")      return CmdLibEdit(ss, err);
    if (cmd == "libpath")        return CmdLibPath(ss, err);
    if (cmd == "libdistinct")   return CmdLibDistinct(ss, err);
    if (cmd == "libpropsmat")    return CmdLibPropsMat(ss, err);
    if (cmd == "libgamemin")    return CmdLibGameMin(ss, err);
    if (cmd == "libgamelog")    return CmdLibGameLog(ss, err);
    if (cmd == "libcreateextras") return CmdLibCreateExtras(ss, err);
    if (cmd == "libmark")       return CmdLibMark(ss, err);
    if (cmd == "libpropsmesh")  return CmdLibPropsMesh(ss, err);
    if (cmd == "libsound")      return CmdLibSound(ss, err);
    if (cmd == "libnested")     return CmdLibNested(ss, err);
    if (cmd == "libtexload")   return CmdLibTexLoad();
    if (cmd == "libmove")       return CmdLibMove(ss, err);
    if (cmd == "libimage")      return CmdLibImage(ss, err);
    if (cmd == "libgameextras") return CmdLibGameExtras(ss, err);
    handled = false;
    return false;
}
