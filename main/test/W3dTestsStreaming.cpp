// ============================================================================
//  W3dTestsStreaming.cpp — comandos de harness del STREAMING (io/Streaming.h):
//  las instancias de prefab y los proxies DIFERIDOS ("carga": "distancia") que
//  cargan y descargan en tiempo real segun la distancia a su objetivo.
//
//  Comandos:
//    streamcrear <carpeta> <textura.png> [lado N] [paso M] [distancia D] [vueltas V] [siempre]
//        el NIVEL de prueba: una libreria "personajes" (libcrear: el prefab "Personaje" -Esqueleto con su animset,
//        Cuerpo skinneado con la Piel CON TEXTURA, Sensor, enemigo.lua-) en <carpeta>/lib, y el juego 3D minimo
//        (juego3dmin) en <carpeta>/juego con un prefab DEL PROYECTO "Poste" (un cubo con la Madera, CON TEXTURA),
//        una GRILLA de N x N (16: 256) instancias DIFERIDAS cada M metros (10) -proxies del Personaje y postes,
//        alternados en damero, "S000".."S255"- con 'distancia' D (25) y un "Director" (recorrido.lua) que lleva la
//        camara por un camino FIJO (afuera, una esquina, el centro, otra esquina, la opuesta, el centro, afuera) V
//        veces (2): espera en cada punto, cuenta las cargadas con isLoaded() y deja la lista en compartido
//        ("veredicto") y en el log ("[streaming] OK puntos=13 cargadas=0,..."): OK si cada vuelta da lo mismo y
//        afuera no queda ninguna. La guarda en <carpeta>/juego/streaming.w3d. Con 'siempre' las instancias son de
//        SIEMPRE (el mismo nivel sin streaming: para medir contra el).
//    streamlua <carpeta> <textura.png> [sincontrol]
//        el juego 3D minimo + el prefab "Poste" con tres instancias: "Fija" (siempre), "Cerca" (diferida, a 5 m de la
//        camara) y "Lejos" (diferida, a 38 m), las dos con distancia 8, y un "Control" (control.lua) que usa
//        streamLoad(), streamUnload(), streamAuto() y isLoaded() y deja su veredicto ("[streaming] OK lua=..."). La guarda en
//        <carpeta>/streamlua.w3d. Con 'sincontrol' no lleva el Control (solo las tres instancias).
//    streamanidada <carpeta> <textura.png>
//        el juego 3D minimo + el prefab "Poste" + un prefab "Casa" cuyo objeto raiz tiene ADENTRO una instancia
//        DIFERIDA del Poste (distancia 8) + una instancia de la Casa en el origen. La guarda en <carpeta>/anidada.w3d
//    streaminfo [instancias N] [cargadas N] [pidiendo N] [descargadas N] [pedidos N]
//    streamestado <objeto> cargada|descargada|pidiendo
//    streammem [base] [igual] [mallas N] [animsets N] [texturas N] [scripts N]
//        lo que el almacen tiene CARGADO por tipo (y sus bytes); 'base' lo anota, 'igual' exige lo mismo que la base:
//        todo lo que trajo el streaming se fue (los arrays y VBO de las mallas, los animsets, las texturas de la GPU)
//    streamobjetivo <x> <y> <z> | off      el objetivo por defecto forzado (la vista previa sin viewport)
//    streamtick [n]                         n ticks de la vista previa del editor (sin jugar)
//    streamvista 0|1                        la vista previa del streaming (opcion del proyecto)
//    streamcarga <objeto> siempre|distancia [D] [objetivo X]   como se carga una instancia (con Ctrl+Z)
//    streamconfig [cargas N] [generar N] [descargar N] [presupuesto MS]
//    streamfijar <objeto> cargar|descargar|auto   lo mismo que streamLoad()/streamUnload()/streamAuto() de lua
//    streambench <frames> [etiqueta T] [peor MS] [heapcrece KB] [rssmaxcomo T2 KB]
//        corre la partida (SimTickPlay + lo que simplay agrega) frame por frame con el reloj de pared: el PEOR
//        frame, el p99, el promedio y el peor tick del streaming; el RSS y el heap EN USO (inicial / maximo / final)
//        y cuantas instancias llego a tener cargadas a la vez. Cotas: 'peor' (ms del peor frame), 'heapcrece' (lo
//        que el heap del final puede pasar al del principio), 'rssmaxcomo' (el RSS maximo no pasa el de la corrida
//        T2 por mas de KB: estable de una vuelta a la otra)
//    streamtexrefs [exacto]
//        el REFCOUNT de las texturas: por cada textura viva, sus referencias en el almacen contra sus DUENOS: las
//        RANURAS que la apuntan (texturas base, normal maps, capas, cuadros animados) y los PEDIDOS del streaming (una
//        instancia cargada retiene lo que pidio). Menos referencias que duenos = una textura que se podria liberar con
//        alguien usandola (falla), y una ranura que apunta a una textura ya liberada tambien falla. Con 'exacto',
//        tienen que ser iguales
//    streamtarjeta [editardistancia D] [visible 0|1] [modo siempre|distancia] [distancia D] [objetivo 0|1] [vista 0|1]
//        la tarjeta de la instancia activa en Properties (pestania 2): si se ve, que modo de carga muestra, si muestra
//        la distancia (y cuanto) y el objetivo, y la fila de la vista previa. 'editardistancia' la cambia como el
//        arrastre del numero (el paso de Ctrl+Z se anota al soltar)
//    streamanimmat <objeto> <frame1.png> <frame2.png>
//        un MATERIAL ANIMADO (como el map_Kd_ANIM de un .mtl): "Anim", sin textura base, con un AnimatedMaterial de dos
//        cuadros (sus referencias son del material animado) puesto en las partes del objeto
//    streamanimtick [n]                     n pasos de los materiales animados (UpdateAnimatedMaterials, 1/30 s)
//    streamundolimpiar                      vacia el historial de undo (UndoClear: los pasos sueltan lo que retienen)
//    streamscript <ruta.lua> <version>      reescribe un .lua: un start() que suma 'version' en
//                                           getShared("version") y cuenta en getShared("arranques")
//    streamveredicto                        anota el veredicto del Play (getShared("veredicto")): el Stop lo borra
//    paquetetest
//        el .w3d como paquete listo para cargar: una textura .w3dt sube con su piramide y vuelve a pixeles, y la
//        lectura por rango del zip devuelve solo esos bytes
//    streamjuegolog <carpeta> <nombre> [igual]
//        corre el juego compilado y exige su "[streaming] OK ..."; 'igual' = el MISMO veredicto que el Play (el que
//        anoto streamveredicto)
// ============================================================================
#include "test/W3dTestsStreaming.h"
#include "test/W3dScript.h"            // W3dRunCommand("juego3dmin ...", "libcrear ...")
#include "io/Streaming.h"
#include "io/Prefabs.h"
#include "io/PrefabsEditor.h"          // W3dInstanceLoadChange
#include "io/Libraries.h"
#include "io/ExternalLibrary.h"      // W3dExtLibLink
#include "io/EditorRoots.h"          // W3dActivateRoot
#include "io/ProjectMeshes.h"        // W3dMeshCreateResource
#include "io/SaveW3D.h"
#include "io/W3dResources.h"
#include "objects/Objects.h"
#include "objects/InstancePrefab.h"
#include "objects/ProxyW3d.h"
#include "objects/Mesh.h"
#include "objects/MeshResource.h"
#include "objects/Materials.h"
#include "objects/Textures.h"
#include "objects/Empty.h"
#include "objects/Particles.h"         // W3dParticlesTick (lo que simplay corre por tick)
#include "objects/VisZone.h"           // W3dVisZonesTick
#include "animation/VertexAnimation.h" // UpdateAnimations
#include "animation/Animation.h"       // PlayAnimation
#include "io/EditedTexture.h"
#include "undo/Undo.h"                 // streamundolimpiar
#include "graphics/w3dTexture.h"            // paquetetest: .w3dt
#include "graphics/w3dGraphics.h"           // TexHasMips / MipmapsGlobal
#include "io/W3dZip.h"                // paquetetest: ReadRange
#include "importers/import_obj.h"     // la cola diferida y las texturas dormidas (streambench)
#include "ViewPorts/Properties.h"     // streamtarjeta: la tarjeta de la instancia
#include "ViewPorts/LayoutTree.h"     // LayoutCompleteRoot
#include "ViewPorts/ViewPorts.h"      // ViewportRow / ViewportColumn (recorrer el layout)
#include "WhiskUI/Propieties/PropLabel.h"
#include "W3dLang.h"                   // T(): lo que muestra la tarjeta
#include "script/W3dScript.h"
#include "W3dRoots.h"
#include "w3dFilesystem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <algorithm>
#include <map>
#include <set>
#include <vector>
#include <string>
#ifdef __linux__
    #include <malloc.h>                // mallinfo2: el heap EN USO
#endif
#ifdef _WIN32
    #include <direct.h>
    #define getcwd _getcwd
#else
    #include <unistd.h>
#endif

extern double W3dNowMs();
extern void SimTickPlay(float);   // (main/script/SimGame.cpp: un tick de la partida, como simplay)
extern bool SimActive();

namespace {

// ---------------------------------------------------------------------------
//  LOS SCRIPTS de las pruebas
// ---------------------------------------------------------------------------
// el RECORRIDO: la camara va de punto en punto (un tramo de 'tramo' frames) y espera 'espera' frames en cada uno; ahi
// cuenta las instancias cargadas (las "S000".."Snnn"). Cada vuelta tiene que contar lo mismo (el streaming solo
// depende del camino) y afuera de la grilla no puede quedar ninguna
const char* kLuaTraversal =
    "-- recorrido de la prueba de streaming: la camara pasa por puntos fijos y en cada uno cuenta las cargadas\n"
    "properties = { cam = \"object\", n = 256, vueltas = 2, espera = 40, tramo = 50 }\n"
    "-- los puntos son (x, y) del piso y la camara va a 6 de altura (la z: Z arriba)\n"
    "local turn = { {0, 0}, {75, 75}, {150, 0}, {150, 150}, {75, 75}, {-80, -80} }\n"
    "local points = { {-80, -80} }\n"
    "local k, f, phase = 1, 0, \"espera\"\n"
    "local counts = {}\n"
    "local function count()\n"
    "  local c = 0\n"
    "  for i = 0, tonumber(property(\"n\")) - 1 do\n"
    "    local o = find(string.format(\"S%03d\", i))\n"
    "    if o ~= nil and isLoaded(o) then c = c + 1 end\n"
    "  end\n"
    "  return c\n"
    "end\n"
    "function start()\n"
    "  for v = 1, tonumber(property(\"vueltas\")) do\n"
    "    for i = 1, #turn do points[#points + 1] = turn[i] end\n"
    "  end\n"
    "  setPosition(object(\"cam\"), points[1][1], points[1][2], 6)\n"
    "end\n"
    "function update(dt)\n"
    "  if phase == \"fin\" then return end\n"
    "  f = f + 1\n"
    "  if phase == \"espera\" then\n"
    "    if f < tonumber(property(\"espera\")) then return end\n"
    "    counts[#counts + 1] = count()\n"
    "    setShared(\"cuenta\" .. #counts, counts[#counts])\n"
    "    if k == #points then\n"
    "      local ok = counts[1] == 0 and counts[#counts] == 0\n"
    "      local nv = #turn\n"
    "      for i = 2 + nv, #counts do if counts[i] ~= counts[i - nv] then ok = false end end\n"
    "      local s = table.concat(counts, \",\")\n"
    "      setShared(\"veredicto\", s)\n"
    "      setShared(\"juez\", ok and \"OK\" or \"FALTA\")\n"
    "      info(string.format(\"[streaming] %s puntos=%d cargadas=%s\", ok and \"OK\" or \"FALTA\", #points, s))\n"
    "      phase = \"fin\"\n"
    "      quit()\n"
    "      return\n"
    "    end\n"
    "    phase = \"tramo\"; f = 0\n"
    "  else\n"
    "    local a, b = points[k], points[k + 1]\n"
    "    local t = f / tonumber(property(\"tramo\"))\n"
    "    if t > 1 then t = 1 end\n"
    "    setPosition(object(\"cam\"), a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t, 6)\n"
    "    if t >= 1 then k = k + 1; phase = \"espera\"; f = 0 end\n"
    "  end\n"
    "end\n";

// streamLoad()/streamUnload()/streamAuto()/isLoaded() de lua: tres pasos con su condicion (y un tope de frames cada uno)
const char* kLuaControl =
    "-- la prueba de cargar()/descargar()/cargaAuto()/cargado(): fija/cerca/lejos son instancias del Poste\n"
    "properties = { fija = \"object\", cerca = \"object\", lejos = \"object\", cam = \"object\" }\n"
    "local paso, f, notes, ok = 0, 0, {}, true\n"
    "local function c(o) return isLoaded(object(o)) and \"1\" or \"0\" end\n"
    "local function wait(cond, name)\n"
    "  if cond then notes[#notes + 1] = name; paso = paso + 1; f = 0; return true end\n"
    "  if f > 150 then notes[#notes + 1] = name .. \"-TIEMPO\"; ok = false; paso = paso + 1; f = 0; return true end\n"
    "  return false\n"
    "end\n"
    "function update(dt)\n"
    "  f = f + 1\n"
    "  if paso == 0 then\n"
    "    -- arranque: la de siempre y la cercana YA cargadas (la cercana, desde el primer frame), la lejana no\n"
    "    notes[#notes + 1] = \"F\" .. c(\"fija\") .. \"C\" .. c(\"cerca\") .. \"L\" .. c(\"lejos\")\n"
    "    if c(\"fija\") ~= \"1\" or c(\"cerca\") ~= \"1\" or c(\"lejos\") ~= \"0\" then ok = false end\n"
    "    -- lo que no es una instancia: false (y cargar() no hace nada)\n"
    "    if isLoaded(object(\"cam\")) ~= false or streamLoad(object(\"cam\")) ~= false then ok = false; notes[#notes + 1] = \"cam\" end\n"
    "    streamUnload(object(\"fija\")); streamUnload(object(\"cerca\")); streamLoad(object(\"lejos\"))\n"
    "    paso = 1; f = 0\n"
    "  elseif paso == 1 then\n"
    "    if wait(c(\"fija\") == \"0\" and c(\"cerca\") == \"0\" and c(\"lejos\") == \"1\", \"fijadas\") then\n"
    "      -- de vuelta a su carga: la lejana se va, la cercana vuelve; la de siempre, a mano\n"
    "      streamAuto(object(\"lejos\")); streamAuto(object(\"cerca\")); streamLoad(object(\"fija\"))\n"
    "    end\n"
    "  elseif paso == 2 then\n"
    "    wait(c(\"fija\") == \"1\" and c(\"cerca\") == \"1\" and c(\"lejos\") == \"0\", \"auto\")\n"
    "  elseif paso == 3 then\n"
    "    local s = table.concat(notes, \",\")\n"
    "    setShared(\"veredicto\", s)\n"
    "    setShared(\"juez\", ok and \"OK\" or \"FALTA\")\n"
    "    info(string.format(\"[streaming] %s lua=%s\", ok and \"OK\" or \"FALTA\", s))\n"
    "    paso = 4\n"
    "    quit()\n"
    "  end\n"
    "end\n";

// ---------------------------------------------------------------------------
//  AYUDAS
// ---------------------------------------------------------------------------
std::string Ent(long n) { char b[32]; sprintf(b, "%ld", n); return b; }
bool Write(const std::string& path, const char* text) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    fputs(text, f);
    return fclose(f) == 0;
}
bool Copy(const std::string& of, const std::string& a) {
    std::vector<unsigned char> d;
    if (!w3dFileSystem::ReadFileBytes(of, d)) return false;
    FILE* f = fopen(a.c_str(), "wb");
    if (!f) return false;
    if (!d.empty()) fwrite(&d[0], 1, d.size(), f);
    return fclose(f) == 0;
}
std::string Absolute(const std::string& r) {
    if (!r.empty() && r[0] == '/') return r;
    char cwd[2048];
    const std::string d = getcwd(cwd, sizeof(cwd)) ? std::string(cwd) : std::string(".");
    return d + "/" + r;
}
void Script(Object* o, const char* path) {
    if (!o->scriptData) o->scriptData = new W3dScriptData();
    W3dScriptEntry e; e.path = path;
    o->scriptData->scripts.push_back(e);
}
void Ref(Object* o, const std::string& k, const std::string& v) {
    if (o->scriptData && !o->scriptData->scripts.empty())
        o->scriptData->scripts.back().refs.push_back(std::make_pair(k, v));
}
long RssKb() {
#ifdef __linux__
    FILE* f = fopen("/proc/self/statm", "r");
    if (!f) return -1;
    long tot = 0, resid = 0;
    if (fscanf(f, "%ld %ld", &tot, &resid) != 2) resid = -1;
    fclose(f);
    return resid > 0 ? resid * 4 : -1;
#else
    return -1;
#endif
}
long HeapKb() {
#if defined(__linux__) && defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 33))
    const struct mallinfo2 mi = mallinfo2();
    return (long)((mi.uordblks + mi.hblkhd) / 1024);   // (tambien los bloques grandes, que van por mmap)
#else
    return -1;
#endif
}
std::string JsonNumTextLocal(float v) { char b[32]; sprintf(b, "%g", (double)v); return b; }
InstancePrefab* Inst(const std::string& n, std::string& err, const char* who) {
    Object* o = SceneCollection ? FindObjectByName(SceneCollection, n) : NULL;
    if (!o || !W3dIsInstanceType(o->getType())) { err = std::string(who) + ": no hay una instancia ni un proxy '" + n + "'"; return NULL; }
    return (InstancePrefab*)o;
}
const char* StateName(int e) {
    return e == W3D_STREAM_LOADED ? "loaded" : e == W3D_STREAM_REQUESTING ? "requesting" : "unloaded";
}

// el prefab DEL PROYECTO "Poste": un cubo "Tronco" (recurso "Poste") con la "Madera" CON TEXTURA
bool CreatePost(const std::string& png, std::string& err) {
    const int ip = W3dRootCreateAndOpen(W3D_ROOT_PREFAB, "Poste");
    if (ip < 0 || !SceneCollection || SceneCollection->Childrens.empty()) { err = "no pude crear el prefab Poste"; return false; }
    Object* root = SceneCollection->Childrens[0];
    Material* madera = new Material("Madera");
    madera->diffuse[0] = 0.6f; madera->diffuse[1] = 0.45f; madera->diffuse[2] = 0.3f;
    madera->texture = TextureTake(png);   // (la ranura del material es duena de esta referencia)
    madera->textureOn = true;
    if (!madera->texture) { err = "no pude cargar la textura '" + png + "'"; return false; }
    Mesh* m = (Mesh*)NewMesh(MeshType(MeshType::cube), root, false);
    if (!m) { err = "NewMesh fallo"; return false; }
    m->SetNameObj("Tronco");
    if (!m->mesh || m->mesh->name != "Poste") W3dMeshCreateResource(m, "Poste");
    for (size_t g = 0; g < m->materialsGroup.size(); g++) m->materialsGroup[g].material = madera;
    if (m->mesh) for (size_t g = 0; g < m->mesh->parts.size(); g++) W3dMeshResourceChangeMaterial(m->mesh, (int)g, madera);
    std::string reason;
    if (!W3dActivateRoot(W3dRootBlock(), &reason)) { err = "volver a la escena: " + reason; return false; }
    return true;
}

// ---------------------------------------------------------------------------
//  streamcrear
// ---------------------------------------------------------------------------
bool CmdStreamCreate(std::istringstream& ss, std::string& err) {
    std::string dir, png, k;
    int side = 16, turns = 2; float step = 10.0f, dist = 25.0f;
    bool always = false;
    ss >> dir >> png;
    while (ss >> k) {
        if (k == "side") ss >> side; else if (k == "step") ss >> step;
        else if (k == "distance") ss >> dist; else if (k == "turns") ss >> turns;
        else if (k == "always") always = true;
    }
    if (dir.empty() || png.empty() || side < 1 || side > 60) {
        err = "streamcreate: uso: streamcreate <carpeta> <textura.png> [lado N] [paso M] [distancia D] [vueltas V]"; return false;
    }
    std::string e2;
    // la LIBRERIA con el Personaje (y su textura)
    if (!W3dRunCommand("libcreate " + dir + "/lib personajes.w3d " + png + " prefab Personaje", e2)) { err = "streamcrear: " + e2; return false; }
    // el JUEGO: la camara, el HUD y un cubo
    const std::string jd = dir + "/juego";
    if (!W3dRunCommand("game3dmin " + jd, e2)) { err = "streamcrear: " + e2; return false; }
    if (!Write(jd + "/recorrido.lua", kLuaTraversal)) { err = "streamcreate: no pude escribir recorrido.lua"; return false; }
    if (!Copy(png, jd + "/madera.png")) { err = "streamcreate: no pude copiar la textura"; return false; }
    if (!CreatePost(jd + "/madera.png", e2)) { err = "streamcrear: " + e2; return false; }
    std::string reason;
    if (!W3dExtLibLink(Absolute(dir + "/lib/personajes.w3d"), &reason)) { err = "streamcreate: no se vinculo: " + reason; return false; }
    const std::string lib = W3dLibsRow(W3dLibsCount() - 1).name;
    // la GRILLA: proxies del Personaje y postes en damero, todos diferidos
    int proxies = 0, posts = 0;
    for (int i = 0; i < side * side; i++) {
        const int cx = i % side, cz = i / side;
        const Vector3 p((float)cx * step, 0.0f, (float)cz * step);
        InstancePrefab* ip = NULL;
        if ((cx + cz) % 2 == 0) { ip = W3dProxyCreate(lib, W3D_LIB_PREFAB, "Personaje", NULL, p, 0.0f); proxies++; }
        else { ip = W3dPrefabCreateInstance("Poste", NULL, p, 0.0f); posts++; }
        if (!ip) { err = "streamcreate: no pude crear la instancia " + Ent(i); return false; }
        char n[16]; sprintf(n, "S%03d", i);
        ip->SetNameObj(n);
        ip->load = always ? W3D_LOAD_ALWAYS : W3D_LOAD_DISTANCE;
        ip->distance = dist;
        ip->select = false;
    }
    // el DIRECTOR del recorrido
    Empty* d = new Empty(NULL, Vector3(0, -10, 0));
    d->SetNameObj("Director");
    Script(d, "recorrido.lua");
    Ref(d, "cam", "Camara");
    Ref(d, "n", Ent(side * side));
    Ref(d, "vueltas", Ent(turns));
    DeselectAll(); ActiveObj = NULL;
    const std::string path = jd + "/streaming.w3d";
    if (!SaveW3D(path)) { err = "streamcreate: no pude guardar '" + path + "'"; return false; }
    printf("      [streamcrear] '%s': %d proxies de %s/Personaje + %d postes (grilla %dx%d cada %.0f m, distancia %.0f m) "
           "+ Director (%d vueltas)\n", path.c_str(), proxies, lib.c_str(), posts, side, side, step, dist, turns);
    return true;
}

// ---------------------------------------------------------------------------
//  streamlua
// ---------------------------------------------------------------------------
bool CmdStreamLua(std::istringstream& ss, std::string& err) {
    std::string dir, png, k; ss >> dir >> png;
    bool withControl = true;
    while (ss >> k) if (k == "nocontrol") withControl = false;
    if (dir.empty() || png.empty()) { err = "streamlua: uso: streamlua <carpeta> <textura.png>"; return false; }
    std::string e2;
    if (!W3dRunCommand("game3dmin " + dir, e2)) { err = "streamlua: " + e2; return false; }
    if (!Write(dir + "/control.lua", kLuaControl)) { err = "streamlua: no pude escribir control.lua"; return false; }
    if (!Copy(png, dir + "/madera.png")) { err = "streamlua: no pude copiar la textura"; return false; }
    if (!CreatePost(dir + "/madera.png", e2)) { err = "streamlua: " + e2; return false; }
    // la camara de juego3dmin esta en (0, 0, 8)
    struct { const char* n; float x, z; int load; } defs[3] = {
        { "Fija", 6.0f, 0.0f, W3D_LOAD_ALWAYS }, { "Cerca", 0.0f, 3.0f, W3D_LOAD_DISTANCE }, { "Lejos", 0.0f, -30.0f, W3D_LOAD_DISTANCE } };
    for (int i = 0; i < 3; i++) {
        InstancePrefab* ip = W3dPrefabCreateInstance("Poste", NULL, Vector3(defs[i].x, 0.0f, defs[i].z), 0.0f);
        if (!ip) { err = "streamlua: no pude crear la instancia"; return false; }
        ip->SetNameObj(defs[i].n);
        ip->load = defs[i].load;
        ip->distance = 8.0f;
        ip->select = false;
    }
    if (withControl) {
        Empty* c = new Empty(NULL, Vector3(0, -10, 0));
        c->SetNameObj("Control");
        Script(c, "control.lua");
        Ref(c, "fija", "Fija"); Ref(c, "cerca", "Cerca"); Ref(c, "lejos", "Lejos"); Ref(c, "cam", "Camara");
    }
    DeselectAll(); ActiveObj = NULL;
    const std::string path = dir + "/streamlua.w3d";
    if (!SaveW3D(path)) { err = "streamlua: no pude guardar '" + path + "'"; return false; }
    printf("      [streamlua] '%s': Fija (siempre), Cerca y Lejos (distancia 8)%s\n", path.c_str(), withControl ? " + Control" : "");
    return true;
}

// ---------------------------------------------------------------------------
//  streamanidada
// ---------------------------------------------------------------------------
bool CmdStreamNested(std::istringstream& ss, std::string& err) {
    std::string dir, png; ss >> dir >> png;
    if (dir.empty() || png.empty()) { err = "streamnested: uso: streamnested <carpeta> <textura.png>"; return false; }
    std::string e2;
    if (!W3dRunCommand("game3dmin " + dir, e2)) { err = "streamanidada: " + e2; return false; }
    if (!Copy(png, dir + "/madera.png")) { err = "streamnested: no pude copiar la textura"; return false; }
    if (!CreatePost(dir + "/madera.png", e2)) { err = "streamanidada: " + e2; return false; }
    // la CASA: su objeto raiz con un Poste DIFERIDO adentro
    const int ic = W3dRootCreateAndOpen(W3D_ROOT_PREFAB, "Casa");
    if (ic < 0 || !SceneCollection || SceneCollection->Childrens.empty()) { err = "streamnested: no pude crear la Casa"; return false; }
    InstancePrefab* post = W3dPrefabCreateInstance("Poste", SceneCollection->Childrens[0], Vector3(0, 0, 0), 0.0f);
    if (!post) { err = "streamnested: no pude poner el Poste en la Casa"; return false; }
    post->SetNameObj("Poste");
    post->load = W3D_LOAD_DISTANCE;
    post->distance = 8.0f;
    std::string reason;
    if (!W3dActivateRoot(W3dRootBlock(), &reason)) { err = "streamanidada: " + reason; return false; }
    InstancePrefab* house = W3dPrefabCreateInstance("Casa", NULL, Vector3(0, 0, 0), 0.0f);
    if (!house) { err = "streamnested: no pude crear la instancia de la Casa"; return false; }
    house->SetNameObj("Casa");
    house->select = false;
    DeselectAll(); ActiveObj = NULL;
    const std::string path = dir + "/anidada.w3d";
    if (!SaveW3D(path)) { err = "streamnested: no pude guardar '" + path + "'"; return false; }
    printf("      [streamanidada] '%s': Casa con un Poste diferido adentro\n", path.c_str());
    return true;
}

// ---------------------------------------------------------------------------
//  streaminfo / streamestado
// ---------------------------------------------------------------------------
bool CmdStreamInfo(std::istringstream& ss, std::string& err) {
    W3dStreamingStats st;
    W3dStreamingGetStats(st);
    printf("      [streaminfo] instancias=%d cargadas=%d pidiendo=%d descargadas=%d pedidos=%d | generadas=%ld descargadas=%ld "
           "| tick %.3f ms (peor %.3f) | vista previa %s\n", st.instances, st.loaded, st.requesting, st.unloaded,
           st.liveRequests, st.generatedTotal, st.unloadedTotal, st.tickMs, st.tickWorstMs, g_w3dStreamingPreviewView ? "si" : "no");
    std::string k; long v = 0;
    while (ss >> k) {
        if (!(ss >> v)) { err = "streaminfo: falta el valor de '" + k + "'"; return false; }
        long real = -1;
        if (k == "instances") real = st.instances;
        else if (k == "loaded") real = st.loaded;
        else if (k == "requesting") real = st.requesting;
        else if (k == "unloaded") real = st.unloaded;
        else if (k == "requests") real = st.liveRequests;
        else { err = "streaminfo: assert desconocido '" + k + "'"; return false; }
        if (real != v) { err = "streaminfo " + k + ": hay " + Ent(real) + " y se esperaba " + Ent(v); return false; }
    }
    return true;
}
bool CmdStreamState(std::istringstream& ss, std::string& err) {
    std::string n, e; ss >> n >> e;
    InstancePrefab* ip = Inst(n, err, "streamstate"); if (!ip) return false;
    printf("      [streamestado] %s: %s (%d hijo(s), carga %s %.1f m%s%s)\n", n.c_str(), StateName(ip->streamState),
           (int)ip->Childrens.size(), ip->load == W3D_LOAD_DISTANCE ? "distance" : "always", ip->distance,
           ip->target.empty() ? "" : ", objetivo ", ip->target.c_str());
    if (!e.empty() && e != StateName(ip->streamState)) {
        err = "streamstate " + n + ": esta " + StateName(ip->streamState) + " y se esperaba " + e; return false;
    }
    // cargada = con lo suyo generado; descargada = sin hijos
    if (e == "unloaded" && !ip->Childrens.empty()) { err = "streamstate " + n + ": descargada pero con hijos"; return false; }
    return true;
}

// ---------------------------------------------------------------------------
//  streammem
// ---------------------------------------------------------------------------
struct Mem { int live[W3DREC_TYPES]; long bytes[W3DREC_TYPES]; int tex; long texBytes; long vbo; bool ok; Mem() : tex(0), texBytes(0), vbo(0), ok(false) {} };
Mem gBase;
Mem Measure() {
    Mem m;
    for (int t = 0; t < W3DREC_TYPES; t++) { m.live[t] = W3dLiveResources(t); m.bytes[t] = W3dResourcesBytes(t); }
    m.tex = LiveTextures() - TexturesBase();
    m.texBytes = TexturesBytes();
    m.vbo = W3dMeshesVBOBytes();
    m.ok = true;
    return m;
}
bool CmdStreamMem(std::istringstream& ss, std::string& err) {
    const Mem m = Measure();
    printf("      [streammem] mallas=%d (%ld B, VBO %ld B) animsets=%d (%ld B) texturas=%d (%ld B de GPU) scripts=%d (%ld B)\n",
           m.live[W3DREC_MESH], m.bytes[W3DREC_MESH], m.vbo, m.live[W3DREC_ANIMSET], m.bytes[W3DREC_ANIMSET],
           m.tex, m.texBytes, m.live[W3DREC_SCRIPT], m.bytes[W3DREC_SCRIPT]);
    std::string k;
    while (ss >> k) {
        if (k == "base") { gBase = m; printf("      [streammem] base anotada\n"); continue; }
        if (k == "equal") {
            if (!gBase.ok) { err = "streammem igual: no hay base anotada"; return false; }
            const int types[3] = { W3DREC_MESH, W3DREC_ANIMSET, W3DREC_SCRIPT };
            const char* names[3] = { "meshes", "animsets", "scripts" };
            for (int i = 0; i < 3; i++)
                if (m.live[types[i]] != gBase.live[types[i]] || m.bytes[types[i]] != gBase.bytes[types[i]]) {
                    err = std::string("streammem igual: ") + names[i] + " = " + Ent(m.live[types[i]]) + " (" + Ent(m.bytes[types[i]]) +
                          " B) y la base tenia " + Ent(gBase.live[types[i]]) + " (" + Ent(gBase.bytes[types[i]]) + " B)";
                    return false;
                }
            if (m.tex != gBase.tex || m.texBytes != gBase.texBytes) {
                err = "streammem igual: texturas = " + Ent(m.tex) + " (" + Ent(m.texBytes) + " B) y la base tenia " + Ent(gBase.tex) +
                      " (" + Ent(gBase.texBytes) + " B)";
                return false;
            }
            if (m.vbo != gBase.vbo) { err = "streammem igual: VBO = " + Ent(m.vbo) + " B y la base tenia " + Ent(gBase.vbo); return false; }
            printf("      [streammem] igual que la base (todo lo del streaming se libero)\n");
            continue;
        }
        long v = 0;
        if (!(ss >> v)) { err = "streammem: falta el valor de '" + k + "'"; return false; }
        long real = -1;
        if (k == "meshes") real = m.live[W3DREC_MESH];
        else if (k == "animsets") real = m.live[W3DREC_ANIMSET];
        else if (k == "textures") real = m.tex;
        else if (k == "scripts") real = m.live[W3DREC_SCRIPT];
        else { err = "streammem: assert desconocido '" + k + "'"; return false; }
        if (real != v) { err = "streammem " + k + ": hay " + Ent(real) + " y se esperaba " + Ent(v); return false; }
    }
    return true;
}

// ---------------------------------------------------------------------------
//  streamobjetivo / streamtick / streamvista / streamcarga / streamconfig
// ---------------------------------------------------------------------------
bool CmdStreamTarget(std::istringstream& ss, std::string& err) {
    std::string a; ss >> a;
    if (a == "off") { W3dStreamingForcedTarget(false, Vector3(0, 0, 0)); printf("      [streamobjetivo] suelto\n"); return true; }
    float x = 0, y = 0, z = 0;
    x = (float)atof(a.c_str());
    if (!(ss >> y >> z)) { err = "streamtarget: uso: streamtarget <x> <y> <z> | off"; return false; }
    W3dStreamingForcedTarget(true, Vector3(x, y, z));
    printf("      [streamobjetivo] (%.1f, %.1f, %.1f)\n", x, y, z);
    return true;
}
bool CmdStreamTick(std::istringstream& ss) {
    int n = 1; ss >> n; if (n < 1) n = 1;
    for (int i = 0; i < n; i++) W3dStreamingTickEditor();
    printf("      [streamtick] %d tick(s) de la vista previa (%s)\n", n, g_w3dStreamingPreviewView ? "on" : "APAGADA: no hace nada");
    return true;
}
// ---- packagetest: el .w3d como PAQUETE listo para cargar --------------------------------------------------------
//  - una textura .w3dt (RGB565, 4x4 con su piramide 2x2 y 1x1, cada nivel de otro color) sube con sus mipmaps, sin
//    decodificar, y vuelve a pixeles (DecodeImage: el nivel 0; DecodeThumbnail: el nivel que entra)
//  - ReadRange de una entrada STORE del zip: solo esos bytes; fuera de rango, false; Leer dos veces (el CRC se
//    verifica la primera)
static void PqU16(std::vector<unsigned char>& v, unsigned x) { v.push_back((unsigned char)(x & 0xFF)); v.push_back((unsigned char)(x >> 8)); }
bool CmdPackageTest(std::istringstream&, std::string& err) {
    // la textura: rojo / verde / azul por nivel
    std::vector<unsigned char> t;
    t.push_back('W'); t.push_back('3'); t.push_back('D'); t.push_back('T');
    PqU16(t, 1); PqU16(t, 1); PqU16(t, 4); PqU16(t, 4); t.push_back(3); t.push_back(0); PqU16(t, 0);
    const unsigned colors[3] = { 0xF800, 0x07E0, 0x001F };
    for (int k = 0, side = 4; k < 3; k++, side /= 2) {
        for (int i = 0; i < side * side; i++) PqU16(t, colors[k]);
        while (t.size() % 4) t.push_back(0);
    }
    const std::string path = "paquetetest.w3dt";
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) { err = "packagetest: no pude escribir " + path; return false; }
    fwrite(&t[0], 1, t.size(), f);
    fclose(f);
    bool ok = true;
    unsigned id = 0;
    int w = 0, h = 0;
    if (!w3dEngine::IsW3dt(path.c_str()) || !w3dEngine::LoadTexture(path.c_str(), id, &w, &h) || !id) {
        err = "packagetest: el .w3dt no subio"; ok = false;
    }
    int tw = 0, th = 0;
    if (ok && (w != 4 || h != 4 || !w3dEngine::TextureSize(id, tw, th) || tw != 4 || th != 4)) {
        err = "packagetest: el tamano del .w3dt no es 4x4"; ok = false;
    }
    if (ok && w3dEngine::MipmapsGlobal() && !w3dEngine::TexHasMips(id)) {
        err = "packagetest: el .w3dt no subio su piramide"; ok = false;
    }
    if (id) w3dEngine::DeleteTexture(id);
    unsigned char* px = NULL;
    if (ok && (!w3dEngine::DecodeImage(path.c_str(), &px, &w, &h) || w != 4 || px[0] != 255 || px[1] != 0 || px[2] != 0 || px[3] != 255)) {
        err = "packagetest: DecodeImage del .w3dt no da el nivel 0 rojo"; ok = false;
    }
    if (px) { w3dEngine::FreeImage(px); px = NULL; }
    if (ok && (!w3dEngine::DecodeThumbnail(path.c_str(), 2, &px, &w, &h) || w != 2 || px[0] != 0 || px[1] != 255 || px[2] != 0)) {
        err = "packagetest: la miniatura del .w3dt no es su nivel 1 (verde, 2x2)"; ok = false;
    }
    if (px) w3dEngine::FreeImage(px);
    remove(path.c_str());
    // el zip: lectura por rango
    if (ok) {
        std::vector<unsigned char> d(1024);
        for (size_t i = 0; i < d.size(); i++) d[i] = (unsigned char)(i * 7);
        const std::string zr = "paquetetest.zip";
        W3dZipWriter zw;
        zw.Add("meshes/a.w3db", d);
        if (!zw.Save(zr)) { err = "packagetest: no pude escribir " + zr; return false; }
        W3dZipReader zl;
        std::vector<unsigned char> r;
        if (!zl.Open(zr)) { err = "packagetest: no pude abrir el zip"; ok = false; }
        else if (!zl.ReadRange("meshes/a.w3db", 100, 10, r) || r.size() != 10 || r[0] != d[100] || r[9] != d[109]) {
            err = "packagetest: LeerRango no devolvio los bytes 100..109"; ok = false;
        } else if (zl.ReadRange("meshes/a.w3db", 1020, 10, r)) {
            err = "packagetest: LeerRango fuera de la entrada no fallo"; ok = false;
        } else if (!zl.Read("meshes/a.w3db", r) || r != d || !zl.Read("meshes/a.w3db", r) || r != d) {
            err = "packagetest: Leer (dos veces) no devolvio la entrada"; ok = false;
        }
        zl.Close();
        remove(zr.c_str());
    }
    if (ok) printf("      [paquetetest] .w3dt (565, 3 niveles) + LeerRango del zip: OK\n");
    return ok;
}

bool CmdStreamView(std::istringstream& ss) {
    int on = 1; ss >> on;
    W3dStreamingPreviewViewSet(on != 0);
    printf("      [streamvista] vista previa %s\n", on ? "on" : "off");
    return true;
}
bool CmdStreamLoad(std::istringstream& ss, std::string& err) {
    std::string n, mode, k; ss >> n >> mode;
    InstancePrefab* ip = Inst(n, err, "streamload"); if (!ip) return false;
    float d = ip->distance; std::string obj = ip->target;
    if (mode != "always" && mode != "distance") { err = "streamload: uso: streamload <objeto> siempre|distancia [D] [objetivo X]"; return false; }
    while (ss >> k) {
        if (k == "target") { ss >> obj; if (obj == "-") obj.clear(); }
        else d = (float)atof(k.c_str());
    }
    W3dInstanceLoadChange(ip, mode == "distance" ? W3D_LOAD_DISTANCE : W3D_LOAD_ALWAYS, d, obj);
    printf("      [streamcarga] %s: %s %.1f m%s%s (%s)\n", n.c_str(), mode.c_str(), d, obj.empty() ? "" : " objetivo ", obj.c_str(),
           StateName(ip->streamState));
    return true;
}
bool CmdStreamAnimMat(std::istringstream& ss, std::string& err) {
    std::string n, a, b; ss >> n >> a >> b;
    Object* o = SceneCollection ? FindObjectByName(SceneCollection, n) : NULL;
    if (!o || o->getType() != ObjectType::mesh || a.empty() || b.empty()) {
        err = "streamanimmat: uso: streamanimmat <malla> <frame1.png> <frame2.png>"; return false;
    }
    Mesh* m = (Mesh*)o;
    Material* mat = new Material("Anim");
    AnimatedMaterial* am = new AnimatedMaterial();
    am->targets.push_back(mat);
    Texture* f1 = TextureTake(a);   // (las referencias de los cuadros son del material animado)
    Texture* f2 = TextureTake(b);
    if (!f1 || !f2) { delete am; err = "streamanimmat: no pude cargar los cuadros"; return false; }
    am->frameTextures.push_back(f1); am->frameDurations.push_back(1);
    am->frameTextures.push_back(f2); am->frameDurations.push_back(1);
    AnimatedMaterials.push_back(am);
    for (size_t g = 0; g < m->materialsGroup.size(); g++) m->materialsGroup[g].material = mat;
    if (m->mesh) for (size_t g = 0; g < m->mesh->parts.size(); g++) W3dMeshResourceChangeMaterial(m->mesh, (int)g, mat);
    printf("      [streamanimmat] '%s': material animado 'Anim' (%s, %s)\n", n.c_str(), a.c_str(), b.c_str());
    return true;
}
bool CmdStreamAnimTick(std::istringstream& ss) {
    int n = 1; ss >> n; if (n < 1) n = 1;
    for (int i = 0; i < n; i++) UpdateAnimatedMaterials(1.0f / 30.0f);
    printf("      [streamanimtick] %d paso(s)\n", n);
    return true;
}
bool CmdStreamScript(std::istringstream& ss, std::string& err) {
    std::string path; int version = 0; ss >> path >> version;
    if (path.empty()) { err = "streamscript: uso: streamscript <ruta.lua> <version>"; return false; }
    char b[512];
    snprintf(b, sizeof(b),
             "-- marca de version (prueba de streaming: el editor corre siempre la fuente actual)\n"
             "function start()\n"
             "  setShared(\"version\", (getShared(\"version\") or 0) + %d)\n"
             "  setShared(\"arranques\", (getShared(\"arranques\") or 0) + 1)\n"
             "end\n", version);
    if (!Write(path, b)) { err = "streamscript: no pude escribir '" + path + "'"; return false; }
    printf("      [streamscript] '%s': version %d\n", path.c_str(), version);
    return true;
}
bool CmdStreamSet(std::istringstream& ss, std::string& err) {
    std::string n, q; ss >> n >> q;
    InstancePrefab* ip = Inst(n, err, "streamset"); if (!ip) return false;
    const int request = (q == "load") ? 1 : (q == "unload") ? -1 : (q == "auto") ? 2 : 0;
    if (!request) { err = "streamset: uso: streamset <objeto> cargar|descargar|auto"; return false; }
    W3dStreamingRequest(ip, request);
    printf("      [streamfijar] %s: %s (se aplica en el proximo tick)\n", n.c_str(), q.c_str());
    return true;
}
bool CmdStreamConfig(std::istringstream& ss) {
    std::string k; float v = 0;
    while (ss >> k >> v) {
        if (k == "loads") g_w3dStreamingConfig.loadsPerFrame = (int)v;
        else if (k == "generate") g_w3dStreamingConfig.generatePerFrame = (int)v;
        else if (k == "unload") g_w3dStreamingConfig.unloadPerFrame = (int)v;
        else if (k == "budget") g_w3dStreamingConfig.budgetMs = v;
    }
    printf("      [streamconfig] cargas/frame=%d generar/frame=%d descargar/frame=%d histeresis=%.0f%% presupuesto=%.1f ms\n",
           g_w3dStreamingConfig.loadsPerFrame, g_w3dStreamingConfig.generatePerFrame, g_w3dStreamingConfig.unloadPerFrame,
           g_w3dStreamingConfig.hysteresis * 100.0f, g_w3dStreamingConfig.budgetMs);
    return true;
}

// ---------------------------------------------------------------------------
//  streambench
// ---------------------------------------------------------------------------
struct Bench { long rss0, rssMax, rssEnd, heap0, heapMax, heapEnd; double worst; };
std::map<std::string, Bench> gBenches;
bool CmdStreamBench(std::istringstream& ss, std::string& err) {
    int frames = 0; ss >> frames;
    std::string k, label, as;
    double worstBound = -1.0; long boundHeap = -1, marginRss = 0;
    while (ss >> k) {
        if (k == "label") ss >> label;
        else if (k == "worst") ss >> worstBound;
        else if (k == "heapgrows") ss >> boundHeap;
        else if (k == "rssmaxas") ss >> as >> marginRss;
    }
    if (frames < 1) { err = "streambench: uso: streambench <frames> [etiqueta T] [peor MS] [heapcrece KB] [rssmaxcomo T2 KB]"; return false; }
    const float dt = 1.0f / 30.0f;
    std::vector<double> t;
    t.reserve((size_t)frames);
    W3dStreamingStatsReset();
    Bench b;
    b.rss0 = b.rssMax = RssKb();
    b.heap0 = b.heapMax = HeapKb();
    int loadMax = 0;
    for (int i = 0; i < frames; i++) {
        const double t0 = W3dNowMs();
        // (lo mismo que simplay por tick)
        SimTickPlay(dt);
        W3dParticlesTick(dt);
        W3dVisZonesTick();
        if (PlayAnimation) UpdateAnimations(dt);
        t.push_back(W3dNowMs() - t0);
        if (i % 5 == 0 || i + 1 == frames) {
            const long r = RssKb(), h = HeapKb();
            if (r > b.rssMax) b.rssMax = r;
            if (h > b.heapMax) b.heapMax = h;
            W3dStreamingStats st;
            W3dStreamingGetStats(st);
            if (st.loaded > loadMax) loadMax = st.loaded;
        }
    }
    b.rssEnd = RssKb();
    b.heapEnd = HeapKb();
    std::vector<double> o = t;
    std::sort(o.begin(), o.end());
    double sum = 0.0;
    for (size_t i = 0; i < t.size(); i++) sum += t[i];
    b.worst = o.back();
    const double p99 = o[(size_t)((double)(o.size() - 1) * 0.99)];
    W3dStreamingStats st;
    W3dStreamingGetStats(st);
    printf("      [streambench] %s frames=%d peor=%.2f ms p99=%.2f ms medio=%.3f ms | tick streaming peor=%.2f ms | "
           "rssKB=%ld/%ld/%ld heapKB=%ld/%ld/%ld (inicio/max/fin) | cargadasMax=%d generadas=%ld descargadas=%ld | "
           "texturas en cola=%d dormidas=%d | sim %s\n",
           label.empty() ? "-" : label.c_str(), frames, b.worst, p99, sum / (double)frames, st.tickWorstMs,
           b.rss0, b.rssMax, b.rssEnd, b.heap0, b.heapMax, b.heapEnd, loadMax, st.generatedTotal, st.unloadedTotal,
           PendingTextures(), AsleepTexturesCount(), SimActive() ? "active" : "SIN partida");
    if (!label.empty()) gBenches[label] = b;
    if (worstBound > 0.0 && b.worst > worstBound) {
        char m[160]; sprintf(m, "streambench: el peor frame tardo %.2f ms (cota %.1f ms)", b.worst, worstBound); err = m; return false;
    }
    // (con un ASIGNADOR INSTRUMENTADO -ASan: mallinfo2 da 0 y lo liberado queda en cuarentena, el RSS solo crece- la
    //  memoria no se puede asertar: se informa y se sigue)
    const bool measurable = b.heap0 > 0;
    if (!measurable && (boundHeap >= 0 || !as.empty()))
        printf("      [streambench] el heap no se puede medir aca (asignador instrumentado): sin cotas de memoria\n");
    if (measurable && boundHeap >= 0 && b.heapEnd - b.heap0 > boundHeap) {
        char m[160]; sprintf(m, "streambench: el heap en uso crecio %ld KB (cota %ld KB)", b.heapEnd - b.heap0, boundHeap); err = m; return false;
    }
    if (measurable && !as.empty()) {
        std::map<std::string, Bench>::iterator it = gBenches.find(as);
        if (it == gBenches.end()) { err = "streambench: no hay una corrida '" + as + "'"; return false; }
        if (b.rssMax > it->second.rssMax + marginRss) {
            char m[200]; sprintf(m, "streambench: el RSS maximo (%ld KB) paso el de '%s' (%ld KB) por mas de %ld KB", b.rssMax,
                                 as.c_str(), it->second.rssMax, marginRss);
            err = m; return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
//  streamtexrefs
// ---------------------------------------------------------------------------
bool CmdStreamTexRefs(std::istringstream& ss, std::string& err) {
    std::string k; bool exact = false;
    while (ss >> k) if (k == "exact") exact = true;
    std::map<const Texture*, int> slots;
    for (size_t i = 0; i < Materials.size(); i++) {
        const Material* m = Materials[i];
        if (!m) continue;
        if (m->texture) slots[m->texture]++;
        if (m->normalTexture) slots[m->normalTexture]++;
        if (m->textureB) slots[m->textureB]++;
        for (size_t c = 0; c < m->layers.size(); c++) if (m->layers[c].tex) slots[m->layers[c].tex]++;
    }
    for (size_t i = 0; i < AnimatedMaterials.size(); i++)
        if (AnimatedMaterials[i])
            for (size_t f = 0; f < AnimatedMaterials[i]->frameTextures.size(); f++)
                if (AnimatedMaterials[i]->frameTextures[f]) slots[AnimatedMaterials[i]->frameTextures[f]]++;
    // los PEDIDOS del streaming (las instancias de todas las raices cargadas)
    std::vector<Object*> roots;
    W3dRootsInOrder(roots);
    if (SceneCollection && std::find(roots.begin(), roots.end(), SceneCollection) == roots.end()) roots.push_back(SceneCollection);
    for (size_t r = 0; r < roots.size(); r++) {
        std::vector<Object*> stack(1, roots[r]);
        while (!stack.empty()) {
            Object* o = stack.back(); stack.pop_back();
            for (size_t i = 0; i < o->Childrens.size(); i++) if (o->Childrens[i]) stack.push_back(o->Childrens[i]);
            if (!W3dIsInstanceType(o->getType())) continue;
            const InstancePrefab* ip = (const InstancePrefab*)o;
            for (size_t q = 0; q < ip->streamRequests.size(); q++) {
                const W3dRequestStream& pd = ip->streamRequests[q];
                if (!W3dResourceCurrent(pd.r, pd.series)) continue;
                const W3dResource* r = pd.r;
                if (r->type == W3DREC_TEXTURE && r->state == W3DREC_READY && r->data) slots[(const Texture*)r->data]++;
            }
        }
    }
    int bad = 0, distinct = 0, n = 0;
    for (size_t i = (size_t)TexturesBase(); i < Textures.size(); i++) {
        const Texture* t = Textures[i];
        if (!t) continue;
        W3dResource* r = W3dResourceFind(W3DREC_TEXTURE, t->path);
        const int refs = (r && r->data == (void*)t) ? r->refTotal : t->refs;
        const int nr = slots.count(t) ? slots[t] : 0;
        n++;
        if (refs < nr) { bad++; printf("      [streamtexrefs] MAL %s: %d referencia(s) y %d dueno(s)\n", t->path.c_str(), refs, nr); }
        else if (refs != nr) { distinct++; printf("      [streamtexrefs] %s: %d referencia(s) y %d dueno(s)\n", t->path.c_str(), refs, nr); }
    }
    // una RANURA COLGADA: apunta a una textura que no esta viva ni en el cementerio (se libero en uso). Solo se
    // compara el puntero (no se lee: ya no existe)
    std::set<const Texture*> exist(Textures.begin(), Textures.end());
    exist.insert(TexturesPurged().begin(), TexturesPurged().end());
    for (std::map<const Texture*, int>::const_iterator it = slots.begin(); it != slots.end(); ++it)
        if (!exist.count(it->first)) { bad++; printf("      [streamtexrefs] MAL una ranura apunta a una textura liberada (%p)\n", (const void*)it->first); }
    printf("      [streamtexrefs] %d textura(s): %d con menos referencias que duenos, %d con otra cuenta\n", n, bad, distinct);
    if (bad) { err = "streamtexrefs: hay texturas con menos referencias que duenos (se podrian liberar en uso)"; return false; }
    if (exact && distinct) { err = "streamtexrefs: hay texturas con mas referencias que duenos"; return false; }
    return true;
}

// ---------------------------------------------------------------------------
//  streamtarjeta
// ---------------------------------------------------------------------------
ViewportBase* LeafProps(ViewportBase* n) {
    if (!n) return NULL;
    if (n->isLeaf()) return n->ViewportKind() == 3 ? n : NULL;
    ViewportBase* a = NULL; ViewportBase* b = NULL;
    if (n->ContainerKind() == 1) { a = ((ViewportRow*)n)->childA; b = ((ViewportRow*)n)->childB; }
    else { a = ((ViewportColumn*)n)->childA; b = ((ViewportColumn*)n)->childB; }
    ViewportBase* r = LeafProps(a);
    return r ? r : LeafProps(b);
}
void RefreshProps(Properties* p) {
    p->RefreshTargetProperties();
    p->UpdateTabs();
    PropsActive = p;
    if (p->activeTab != 2 && p->BarTabs.size() > 2 && p->BarTabs[2]->visible) { p->activeTab = 2; p->UpdateTabs(); }
    rootViewport->Render();
}
bool CmdStreamCard(std::istringstream& ss, std::string& err) {
    Properties* p = (Properties*)LeafProps(LayoutCompleteRoot());
    if (!p) { err = "streamcard: no hay panel de propiedades"; return false; }
    RefreshProps(p);
    std::vector<std::pair<std::string, std::string> > asserts;
    std::string k, v;
    while (ss >> k >> v) {
        if (k == "editdistance") {
            if (!p->propPfDistance || !p->propPfDistance->value) { err = "streamcard: la fila Distance no esta a la vista"; return false; }
            p->propPfDistance->Set((float)atof(v.c_str()));   // (lo que hace el arrastre)
            RefreshProps(p);                                  // (el mouse esta suelto: el paso se anota)
            RefreshProps(p);
        } else asserts.push_back(std::make_pair(k, v));
    }
    const bool vis = p->propPrefab && p->propPrefab->visible;
    const std::string mode = (vis && p->propPfLoad && !p->propPfLoad->hidden) ? p->propPfLoad->button->text : std::string("-");
    const bool dist = vis && p->propPfDistance && p->propPfDistance->value;
    const bool obj = vis && p->propPfTarget && !p->propPfTarget->hidden;
    const bool view = vis && p->propPfView && p->propPfView->value;
    printf("      [streamtarjeta] '%s': tarjeta %s | carga '%s' | distancia %s | objetivo %s | vista previa %s | %s\n",
           ActiveObj ? ActiveObj->name.c_str() : "-", vis ? "visible" : "hidden", mode.c_str(),
           dist ? JsonNumTextLocal(*p->propPfDistance->value).c_str() : "-", obj ? "visible" : "-", view ? "visible" : "-",
           (vis && p->propPfInfo) ? p->propPfInfo->name.c_str() : "");
    for (size_t i = 0; i < asserts.size(); i++) {
        const std::string& a = asserts[i].first;
        const std::string& b = asserts[i].second;
        bool ok = true;
        if (a == "visible") ok = (vis ? 1 : 0) == atoi(b.c_str());
        else if (a == "mode") ok = mode == (b == "distance" ? std::string(T("By Distance")) : std::string(T("Always")));
        else if (a == "distance") ok = dist && fabsf(*p->propPfDistance->value - (float)atof(b.c_str())) < 1e-3f;
        else if (a == "target") ok = (obj ? 1 : 0) == atoi(b.c_str());
        else if (a == "view") ok = (view ? 1 : 0) == atoi(b.c_str());
        else { err = "streamcard: assert desconocido '" + a + "'"; return false; }
        if (!ok) { err = "streamtarjeta: " + a + " no es " + b; return false; }
    }
    return true;
}

// ---------------------------------------------------------------------------
//  streamveredicto / streamjuegolog
// ---------------------------------------------------------------------------
std::string gVerdictPlay;
bool CmdStreamVerdict(std::string& err) {
    gVerdictPlay.clear();
    for (int i = 0; i < W3dSharedScriptCount(); i++) {
        std::string n, v;
        if (W3dSharedScriptPair(i, &n, &v) && n == "verdict") gVerdictPlay = v;
    }
    printf("      [streamveredicto] el Play dijo: %s\n", gVerdictPlay.empty() ? "(nada)" : gVerdictPlay.c_str());
    if (gVerdictPlay.empty()) { err = "streamverdict: el Play no dejo compartido(\"veredicto\")"; return false; }
    return true;
}
bool CmdStreamGameLog(std::istringstream& ss, std::string& err) {
    std::string dir, name, k; ss >> dir >> name;
    bool equal = false;
    while (ss >> k) if (k == "equal") equal = true;
    const std::string folder = dir + "/build/linux";
    const std::string bin = folder + "/" + name;
    if (!w3dFileSystem::FileExists(bin)) { err = "streamgamelog: no existe el binario compilado '" + bin + "'"; return false; }
    const std::string ofPlay = gVerdictPlay;   // (lo anoto streamveredicto)
    const std::string log = folder + "/whisk3d.log";
    remove(log.c_str());
    char cmdRun[2200];
    snprintf(cmdRun, sizeof(cmdRun), "cd \"%s\" && timeout 120 ./%s > /dev/null 2>&1", folder.c_str(), name.c_str());
    const int r = system(cmdRun);
    FILE* f = fopen(log.c_str(), "rb");
    if (!f) { err = "streamgamelog: el juego no dejo whisk3d.log (se compilo en modo debug?)"; return false; }
    std::string verdict;
    char buf[4096];
    while (fgets(buf, sizeof(buf), f)) {
        const char* p = strstr(buf, "[streaming] ");
        if (p && (strstr(p, " OK ") || strstr(p, " FALTA "))) verdict = p;
    }
    fclose(f);
    while (!verdict.empty() && (verdict[verdict.size() - 1] == '\n' || verdict[verdict.size() - 1] == '\r'))
        verdict.erase(verdict.size() - 1);
    printf("      [streamjuegolog] salida=%d | %s\n", r, verdict.empty() ? "(sin veredicto)" : verdict.c_str());
    if (verdict.find("[streaming] OK") == std::string::npos) {
        err = "streamgamelog: el juego compilado no dio el veredicto OK"; return false;
    }
    if (equal) {
        if (ofPlay.empty()) { err = "streamgamelog igual: no hay veredicto del Play (streamveredicto)"; return false; }
        // el veredicto termina en "=<lista>": la lista del Play tiene que ser la misma
        const size_t eq = verdict.rfind('=');
        const std::string list = (eq == std::string::npos) ? std::string() : verdict.substr(eq + 1);
        printf("      [streamjuegolog] Play: %s | juego: %s\n", ofPlay.c_str(), list.c_str());
        if (list != ofPlay) { err = "streamgamelog: el juego compilado dio '" + list + "' y el Play '" + ofPlay + "'"; return false; }
    }
    return true;
}

} // namespace

bool W3dTestsStreamingCmd(const std::string& cmd, std::istringstream& ss, std::string& err, bool& handled) {
    handled = true;
    if (cmd == "streamcreate")    return CmdStreamCreate(ss, err);
    if (cmd == "streamlua")      return CmdStreamLua(ss, err);
    if (cmd == "streamnested")  return CmdStreamNested(ss, err);
    if (cmd == "streaminfo")     return CmdStreamInfo(ss, err);
    if (cmd == "streamstate")   return CmdStreamState(ss, err);
    if (cmd == "streammem")      return CmdStreamMem(ss, err);
    if (cmd == "streamtarget") return CmdStreamTarget(ss, err);
    if (cmd == "streamtick")     return CmdStreamTick(ss);
    if (cmd == "streamview")    return CmdStreamView(ss);
    if (cmd == "packagetest")    return CmdPackageTest(ss, err);
    if (cmd == "streamload")    return CmdStreamLoad(ss, err);
    if (cmd == "streamconfig")   return CmdStreamConfig(ss);
    if (cmd == "streambench")    return CmdStreamBench(ss, err);
    if (cmd == "streamtexrefs")  return CmdStreamTexRefs(ss, err);
    if (cmd == "streamgamelog") return CmdStreamGameLog(ss, err);
    if (cmd == "streamverdict") return CmdStreamVerdict(err);
    if (cmd == "streamcard")  return CmdStreamCard(ss, err);
    if (cmd == "streamset")    return CmdStreamSet(ss, err);
    if (cmd == "streamanimmat")  return CmdStreamAnimMat(ss, err);
    if (cmd == "streamanimtick") return CmdStreamAnimTick(ss);
    if (cmd == "streamundoclear") { UndoClear(); printf("      [streamundolimpiar] historial vacio\n"); return true; }
    if (cmd == "streamscript")   return CmdStreamScript(ss, err);
    handled = false;
    return false;
}
