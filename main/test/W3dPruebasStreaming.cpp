// ============================================================================
//  W3dPruebasStreaming.cpp — comandos de harness del STREAMING (io/Streaming.h):
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
//        veces (2): espera en cada punto, cuenta las cargadas con cargado() y deja la lista en compartido
//        ("veredicto") y en el log ("[streaming] OK puntos=13 cargadas=0,..."): OK si cada vuelta da lo mismo y
//        afuera no queda ninguna. La guarda en <carpeta>/juego/streaming.w3d. Con 'siempre' las instancias son de
//        SIEMPRE (el mismo nivel sin streaming: para medir contra el).
//    streamlua <carpeta> <textura.png> [sincontrol]
//        el juego 3D minimo + el prefab "Poste" con tres instancias: "Fija" (siempre), "Cerca" (diferida, a 5 m de la
//        camara) y "Lejos" (diferida, a 38 m), las dos con distancia 8, y un "Control" (control.lua) que usa
//        cargar(), descargar(), cargaAuto() y cargado() y deja su veredicto ("[streaming] OK lua=..."). La guarda en
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
//    streamfijar <objeto> cargar|descargar|auto   lo mismo que cargar()/descargar()/cargaAuto() de lua
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
//    streamundolimpiar                      vacia el historial de undo (UndoLimpiar: los pasos sueltan lo que retienen)
//    streamscript <ruta.lua> <version>      reescribe un .lua: un inicio() que suma 'version' en
//                                           compartido("version") y cuenta en compartido("arranques")
//    streamveredicto                        anota el veredicto del Play (compartido("veredicto")): el Stop lo borra
//    streamjuegolog <carpeta> <nombre> [igual]
//        corre el juego compilado y exige su "[streaming] OK ..."; 'igual' = el MISMO veredicto que el Play (el que
//        anoto streamveredicto)
// ============================================================================
#include "test/W3dPruebasStreaming.h"
#include "test/W3dScript.h"            // W3dRunCommand("juego3dmin ...", "libcrear ...")
#include "io/Streaming.h"
#include "io/Prefabs.h"
#include "io/PrefabsEditor.h"          // W3dInstanciaCargaCambiar
#include "io/Librerias.h"
#include "io/BibliotecaExterna.h"      // W3dLibreriaVincular
#include "io/RaicesEditor.h"           // W3dActivarRaiz
#include "io/MallasProyecto.h"         // W3dMallaCrearRecurso
#include "io/GuardarW3D.h"
#include "io/W3dRecursos.h"
#include "objects/Objects.h"
#include "objects/InstanciaPrefab.h"
#include "objects/ProxyW3d.h"
#include "objects/Mesh.h"
#include "objects/MallaRecurso.h"
#include "objects/Materials.h"
#include "objects/Textures.h"
#include "objects/Empty.h"
#include "objects/Particulas.h"        // W3dParticulasTick (lo que simplay corre por tick)
#include "objects/VisZona.h"           // W3dVisZonasTick
#include "animation/VertexAnimation.h" // UpdateAnimations
#include "animation/Animation.h"       // PlayAnimation
#include "io/TexturaEditada.h"
#include "undo/Undo.h"                 // streamundolimpiar
#include "importers/import_obj.h"      // la cola diferida y las texturas dormidas (streambench)
#include "ViewPorts/Properties.h"      // streamtarjeta: la tarjeta de la instancia
#include "ViewPorts/LayoutArbol.h"     // LayoutRaizCompleta
#include "ViewPorts/ViewPorts.h"       // ViewportRow / ViewportColumn (recorrer el layout)
#include "WhiskUI/Propieties/PropLabel.h"
#include "W3dLang.h"                   // T(): lo que muestra la tarjeta
#include "script/W3dScript.h"
#include "W3dRaices.h"
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
extern void SimTickPlay(float);   // (main/script/SimJuego.cpp: un tick de la partida, como simplay)
extern bool SimActiva();

namespace {

// ---------------------------------------------------------------------------
//  LOS SCRIPTS de las pruebas
// ---------------------------------------------------------------------------
// el RECORRIDO: la camara va de punto en punto (un tramo de 'tramo' frames) y espera 'espera' frames en cada uno; ahi
// cuenta las instancias cargadas (las "S000".."Snnn"). Cada vuelta tiene que contar lo mismo (el streaming solo
// depende del camino) y afuera de la grilla no puede quedar ninguna
const char* kLuaRecorrido =
    "-- recorrido de la prueba de streaming: la camara pasa por puntos fijos y en cada uno cuenta las cargadas\n"
    "propiedades = { cam = \"objeto\", n = 256, vueltas = 2, espera = 40, tramo = 50 }\n"
    "local vuelta = { {0, 0}, {75, 75}, {150, 0}, {150, 150}, {75, 75}, {-80, -80} }\n"
    "local puntos = { {-80, -80} }\n"
    "local k, f, fase = 1, 0, \"espera\"\n"
    "local cuentas = {}\n"
    "local function contar()\n"
    "  local c = 0\n"
    "  for i = 0, tonumber(propiedad(\"n\")) - 1 do\n"
    "    local o = buscar(string.format(\"S%03d\", i))\n"
    "    if o ~= nil and cargado(o) then c = c + 1 end\n"
    "  end\n"
    "  return c\n"
    "end\n"
    "function inicio()\n"
    "  for v = 1, tonumber(propiedad(\"vueltas\")) do\n"
    "    for i = 1, #vuelta do puntos[#puntos + 1] = vuelta[i] end\n"
    "  end\n"
    "  setPosicion(objeto(\"cam\"), puntos[1][1], 6, puntos[1][2])\n"
    "end\n"
    "function actualizar(dt)\n"
    "  if fase == \"fin\" then return end\n"
    "  f = f + 1\n"
    "  if fase == \"espera\" then\n"
    "    if f < tonumber(propiedad(\"espera\")) then return end\n"
    "    cuentas[#cuentas + 1] = contar()\n"
    "    setCompartido(\"cuenta\" .. #cuentas, cuentas[#cuentas])\n"
    "    if k == #puntos then\n"
    "      local ok = cuentas[1] == 0 and cuentas[#cuentas] == 0\n"
    "      local nv = #vuelta\n"
    "      for i = 2 + nv, #cuentas do if cuentas[i] ~= cuentas[i - nv] then ok = false end end\n"
    "      local s = table.concat(cuentas, \",\")\n"
    "      setCompartido(\"veredicto\", s)\n"
    "      setCompartido(\"juez\", ok and \"OK\" or \"FALTA\")\n"
    "      info(string.format(\"[streaming] %s puntos=%d cargadas=%s\", ok and \"OK\" or \"FALTA\", #puntos, s))\n"
    "      fase = \"fin\"\n"
    "      salir()\n"
    "      return\n"
    "    end\n"
    "    fase = \"tramo\"; f = 0\n"
    "  else\n"
    "    local a, b = puntos[k], puntos[k + 1]\n"
    "    local t = f / tonumber(propiedad(\"tramo\"))\n"
    "    if t > 1 then t = 1 end\n"
    "    setPosicion(objeto(\"cam\"), a[1] + (b[1] - a[1]) * t, 6, a[2] + (b[2] - a[2]) * t)\n"
    "    if t >= 1 then k = k + 1; fase = \"espera\"; f = 0 end\n"
    "  end\n"
    "end\n";

// cargar()/descargar()/cargaAuto()/cargado() de lua: tres pasos con su condicion (y un tope de frames cada uno)
const char* kLuaControl =
    "-- la prueba de cargar()/descargar()/cargaAuto()/cargado(): fija/cerca/lejos son instancias del Poste\n"
    "propiedades = { fija = \"objeto\", cerca = \"objeto\", lejos = \"objeto\", cam = \"objeto\" }\n"
    "local paso, f, notas, ok = 0, 0, {}, true\n"
    "local function c(o) return cargado(objeto(o)) and \"1\" or \"0\" end\n"
    "local function esperar(cond, nombre)\n"
    "  if cond then notas[#notas + 1] = nombre; paso = paso + 1; f = 0; return true end\n"
    "  if f > 150 then notas[#notas + 1] = nombre .. \"-TIEMPO\"; ok = false; paso = paso + 1; f = 0; return true end\n"
    "  return false\n"
    "end\n"
    "function actualizar(dt)\n"
    "  f = f + 1\n"
    "  if paso == 0 then\n"
    "    -- arranque: la de siempre y la cercana YA cargadas (la cercana, desde el primer frame), la lejana no\n"
    "    notas[#notas + 1] = \"F\" .. c(\"fija\") .. \"C\" .. c(\"cerca\") .. \"L\" .. c(\"lejos\")\n"
    "    if c(\"fija\") ~= \"1\" or c(\"cerca\") ~= \"1\" or c(\"lejos\") ~= \"0\" then ok = false end\n"
    "    -- lo que no es una instancia: false (y cargar() no hace nada)\n"
    "    if cargado(objeto(\"cam\")) ~= false or cargar(objeto(\"cam\")) ~= false then ok = false; notas[#notas + 1] = \"cam\" end\n"
    "    descargar(objeto(\"fija\")); descargar(objeto(\"cerca\")); cargar(objeto(\"lejos\"))\n"
    "    paso = 1; f = 0\n"
    "  elseif paso == 1 then\n"
    "    if esperar(c(\"fija\") == \"0\" and c(\"cerca\") == \"0\" and c(\"lejos\") == \"1\", \"fijadas\") then\n"
    "      -- de vuelta a su carga: la lejana se va, la cercana vuelve; la de siempre, a mano\n"
    "      cargaAuto(objeto(\"lejos\")); cargaAuto(objeto(\"cerca\")); cargar(objeto(\"fija\"))\n"
    "    end\n"
    "  elseif paso == 2 then\n"
    "    esperar(c(\"fija\") == \"1\" and c(\"cerca\") == \"1\" and c(\"lejos\") == \"0\", \"auto\")\n"
    "  elseif paso == 3 then\n"
    "    local s = table.concat(notas, \",\")\n"
    "    setCompartido(\"veredicto\", s)\n"
    "    setCompartido(\"juez\", ok and \"OK\" or \"FALTA\")\n"
    "    info(string.format(\"[streaming] %s lua=%s\", ok and \"OK\" or \"FALTA\", s))\n"
    "    paso = 4\n"
    "    salir()\n"
    "  end\n"
    "end\n";

// ---------------------------------------------------------------------------
//  AYUDAS
// ---------------------------------------------------------------------------
std::string Ent(long n) { char b[32]; sprintf(b, "%ld", n); return b; }
bool Escribir(const std::string& ruta, const char* texto) {
    FILE* f = fopen(ruta.c_str(), "wb");
    if (!f) return false;
    fputs(texto, f);
    return fclose(f) == 0;
}
bool Copiar(const std::string& de, const std::string& a) {
    std::vector<unsigned char> d;
    if (!w3dFileSystem::ReadFileBytes(de, d)) return false;
    FILE* f = fopen(a.c_str(), "wb");
    if (!f) return false;
    if (!d.empty()) fwrite(&d[0], 1, d.size(), f);
    return fclose(f) == 0;
}
std::string Absoluta(const std::string& r) {
    if (!r.empty() && r[0] == '/') return r;
    char cwd[2048];
    const std::string d = getcwd(cwd, sizeof(cwd)) ? std::string(cwd) : std::string(".");
    return d + "/" + r;
}
void Script(Object* o, const char* ruta) {
    if (!o->scriptDatos) o->scriptDatos = new W3dScriptDatos();
    W3dScriptEntrada e; e.ruta = ruta;
    o->scriptDatos->scripts.push_back(e);
}
void Ref(Object* o, const std::string& k, const std::string& v) {
    if (o->scriptDatos && !o->scriptDatos->scripts.empty())
        o->scriptDatos->scripts.back().refs.push_back(std::make_pair(k, v));
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
std::string JsonNumTextoLocal(float v) { char b[32]; sprintf(b, "%g", (double)v); return b; }
InstanciaPrefab* Inst(const std::string& n, std::string& err, const char* quien) {
    Object* o = SceneCollection ? FindObjectByName(SceneCollection, n) : NULL;
    if (!o || !W3dEsTipoInstancia(o->getType())) { err = std::string(quien) + ": no hay una instancia ni un proxy '" + n + "'"; return NULL; }
    return (InstanciaPrefab*)o;
}
const char* NombreEstado(int e) {
    return e == W3D_STREAM_CARGADA ? "cargada" : e == W3D_STREAM_PIDIENDO ? "pidiendo" : "descargada";
}

// el prefab DEL PROYECTO "Poste": un cubo "Tronco" (recurso "Poste") con la "Madera" CON TEXTURA
bool CrearPoste(const std::string& png, std::string& err) {
    const int ip = W3dRaizCrearYAbrir(W3D_RAIZ_PREFAB, "Poste");
    if (ip < 0 || !SceneCollection || SceneCollection->Childrens.empty()) { err = "no pude crear el prefab Poste"; return false; }
    Object* raiz = SceneCollection->Childrens[0];
    Material* madera = new Material("Madera");
    madera->diffuse[0] = 0.6f; madera->diffuse[1] = 0.45f; madera->diffuse[2] = 0.3f;
    madera->texture = TexturaTomar(png);   // (la ranura del material es duena de esta referencia)
    madera->textureOn = true;
    if (!madera->texture) { err = "no pude cargar la textura '" + png + "'"; return false; }
    Mesh* m = (Mesh*)NewMesh(MeshType(MeshType::cube), raiz, false);
    if (!m) { err = "NewMesh fallo"; return false; }
    m->SetNameObj("Tronco");
    if (!m->malla || m->malla->nombre != "Poste") W3dMallaCrearRecurso(m, "Poste");
    for (size_t g = 0; g < m->materialsGroup.size(); g++) m->materialsGroup[g].material = madera;
    if (m->malla) for (size_t g = 0; g < m->malla->partes.size(); g++) W3dMallaRecursoCambiarMaterial(m->malla, (int)g, madera);
    std::string motivo;
    if (!W3dActivarRaiz(W3dRaizBloque(), &motivo)) { err = "volver a la escena: " + motivo; return false; }
    return true;
}

// ---------------------------------------------------------------------------
//  streamcrear
// ---------------------------------------------------------------------------
bool CmdStreamCrear(std::istringstream& ss, std::string& err) {
    std::string dir, png, k;
    int lado = 16, vueltas = 2; float paso = 10.0f, dist = 25.0f;
    bool siempre = false;
    ss >> dir >> png;
    while (ss >> k) {
        if (k == "lado") ss >> lado; else if (k == "paso") ss >> paso;
        else if (k == "distancia") ss >> dist; else if (k == "vueltas") ss >> vueltas;
        else if (k == "siempre") siempre = true;
    }
    if (dir.empty() || png.empty() || lado < 1 || lado > 60) {
        err = "streamcrear: uso: streamcrear <carpeta> <textura.png> [lado N] [paso M] [distancia D] [vueltas V]"; return false;
    }
    std::string e2;
    // la LIBRERIA con el Personaje (y su textura)
    if (!W3dRunCommand("libcrear " + dir + "/lib personajes.w3d " + png + " prefab Personaje", e2)) { err = "streamcrear: " + e2; return false; }
    // el JUEGO: la camara, el HUD y un cubo
    const std::string jd = dir + "/juego";
    if (!W3dRunCommand("juego3dmin " + jd, e2)) { err = "streamcrear: " + e2; return false; }
    if (!Escribir(jd + "/recorrido.lua", kLuaRecorrido)) { err = "streamcrear: no pude escribir recorrido.lua"; return false; }
    if (!Copiar(png, jd + "/madera.png")) { err = "streamcrear: no pude copiar la textura"; return false; }
    if (!CrearPoste(jd + "/madera.png", e2)) { err = "streamcrear: " + e2; return false; }
    std::string motivo;
    if (!W3dLibreriaVincular(Absoluta(dir + "/lib/personajes.w3d"), &motivo)) { err = "streamcrear: no se vinculo: " + motivo; return false; }
    const std::string lib = W3dLibsFila(W3dLibsCantidad() - 1).nombre;
    // la GRILLA: proxies del Personaje y postes en damero, todos diferidos
    int proxies = 0, postes = 0;
    for (int i = 0; i < lado * lado; i++) {
        const int cx = i % lado, cz = i / lado;
        const Vector3 p((float)cx * paso, 0.0f, (float)cz * paso);
        InstanciaPrefab* ip = NULL;
        if ((cx + cz) % 2 == 0) { ip = W3dProxyCrear(lib, W3D_LIB_PREFAB, "Personaje", NULL, p, 0.0f); proxies++; }
        else { ip = W3dPrefabCrearInstancia("Poste", NULL, p, 0.0f); postes++; }
        if (!ip) { err = "streamcrear: no pude crear la instancia " + Ent(i); return false; }
        char n[16]; sprintf(n, "S%03d", i);
        ip->SetNameObj(n);
        ip->carga = siempre ? W3D_CARGA_SIEMPRE : W3D_CARGA_DISTANCIA;
        ip->distancia = dist;
        ip->select = false;
    }
    // el DIRECTOR del recorrido
    Empty* d = new Empty(NULL, Vector3(0, -10, 0));
    d->SetNameObj("Director");
    Script(d, "recorrido.lua");
    Ref(d, "cam", "Camara");
    Ref(d, "n", Ent(lado * lado));
    Ref(d, "vueltas", Ent(vueltas));
    DeseleccionarTodo(); ObjActivo = NULL;
    const std::string ruta = jd + "/streaming.w3d";
    if (!GuardarW3D(ruta)) { err = "streamcrear: no pude guardar '" + ruta + "'"; return false; }
    printf("      [streamcrear] '%s': %d proxies de %s/Personaje + %d postes (grilla %dx%d cada %.0f m, distancia %.0f m) "
           "+ Director (%d vueltas)\n", ruta.c_str(), proxies, lib.c_str(), postes, lado, lado, paso, dist, vueltas);
    return true;
}

// ---------------------------------------------------------------------------
//  streamlua
// ---------------------------------------------------------------------------
bool CmdStreamLua(std::istringstream& ss, std::string& err) {
    std::string dir, png, k; ss >> dir >> png;
    bool conControl = true;
    while (ss >> k) if (k == "sincontrol") conControl = false;
    if (dir.empty() || png.empty()) { err = "streamlua: uso: streamlua <carpeta> <textura.png>"; return false; }
    std::string e2;
    if (!W3dRunCommand("juego3dmin " + dir, e2)) { err = "streamlua: " + e2; return false; }
    if (!Escribir(dir + "/control.lua", kLuaControl)) { err = "streamlua: no pude escribir control.lua"; return false; }
    if (!Copiar(png, dir + "/madera.png")) { err = "streamlua: no pude copiar la textura"; return false; }
    if (!CrearPoste(dir + "/madera.png", e2)) { err = "streamlua: " + e2; return false; }
    // la camara de juego3dmin esta en (0, 0, 8)
    struct { const char* n; float x, z; int carga; } defs[3] = {
        { "Fija", 6.0f, 0.0f, W3D_CARGA_SIEMPRE }, { "Cerca", 0.0f, 3.0f, W3D_CARGA_DISTANCIA }, { "Lejos", 0.0f, -30.0f, W3D_CARGA_DISTANCIA } };
    for (int i = 0; i < 3; i++) {
        InstanciaPrefab* ip = W3dPrefabCrearInstancia("Poste", NULL, Vector3(defs[i].x, 0.0f, defs[i].z), 0.0f);
        if (!ip) { err = "streamlua: no pude crear la instancia"; return false; }
        ip->SetNameObj(defs[i].n);
        ip->carga = defs[i].carga;
        ip->distancia = 8.0f;
        ip->select = false;
    }
    if (conControl) {
        Empty* c = new Empty(NULL, Vector3(0, -10, 0));
        c->SetNameObj("Control");
        Script(c, "control.lua");
        Ref(c, "fija", "Fija"); Ref(c, "cerca", "Cerca"); Ref(c, "lejos", "Lejos"); Ref(c, "cam", "Camara");
    }
    DeseleccionarTodo(); ObjActivo = NULL;
    const std::string ruta = dir + "/streamlua.w3d";
    if (!GuardarW3D(ruta)) { err = "streamlua: no pude guardar '" + ruta + "'"; return false; }
    printf("      [streamlua] '%s': Fija (siempre), Cerca y Lejos (distancia 8)%s\n", ruta.c_str(), conControl ? " + Control" : "");
    return true;
}

// ---------------------------------------------------------------------------
//  streamanidada
// ---------------------------------------------------------------------------
bool CmdStreamAnidada(std::istringstream& ss, std::string& err) {
    std::string dir, png; ss >> dir >> png;
    if (dir.empty() || png.empty()) { err = "streamanidada: uso: streamanidada <carpeta> <textura.png>"; return false; }
    std::string e2;
    if (!W3dRunCommand("juego3dmin " + dir, e2)) { err = "streamanidada: " + e2; return false; }
    if (!Copiar(png, dir + "/madera.png")) { err = "streamanidada: no pude copiar la textura"; return false; }
    if (!CrearPoste(dir + "/madera.png", e2)) { err = "streamanidada: " + e2; return false; }
    // la CASA: su objeto raiz con un Poste DIFERIDO adentro
    const int ic = W3dRaizCrearYAbrir(W3D_RAIZ_PREFAB, "Casa");
    if (ic < 0 || !SceneCollection || SceneCollection->Childrens.empty()) { err = "streamanidada: no pude crear la Casa"; return false; }
    InstanciaPrefab* poste = W3dPrefabCrearInstancia("Poste", SceneCollection->Childrens[0], Vector3(0, 0, 0), 0.0f);
    if (!poste) { err = "streamanidada: no pude poner el Poste en la Casa"; return false; }
    poste->SetNameObj("Poste");
    poste->carga = W3D_CARGA_DISTANCIA;
    poste->distancia = 8.0f;
    std::string motivo;
    if (!W3dActivarRaiz(W3dRaizBloque(), &motivo)) { err = "streamanidada: " + motivo; return false; }
    InstanciaPrefab* casa = W3dPrefabCrearInstancia("Casa", NULL, Vector3(0, 0, 0), 0.0f);
    if (!casa) { err = "streamanidada: no pude crear la instancia de la Casa"; return false; }
    casa->SetNameObj("Casa");
    casa->select = false;
    DeseleccionarTodo(); ObjActivo = NULL;
    const std::string ruta = dir + "/anidada.w3d";
    if (!GuardarW3D(ruta)) { err = "streamanidada: no pude guardar '" + ruta + "'"; return false; }
    printf("      [streamanidada] '%s': Casa con un Poste diferido adentro\n", ruta.c_str());
    return true;
}

// ---------------------------------------------------------------------------
//  streaminfo / streamestado
// ---------------------------------------------------------------------------
bool CmdStreamInfo(std::istringstream& ss, std::string& err) {
    W3dStreamingStats st;
    W3dStreamingEstadisticas(st);
    printf("      [streaminfo] instancias=%d cargadas=%d pidiendo=%d descargadas=%d pedidos=%d | generadas=%ld descargadas=%ld "
           "| tick %.3f ms (peor %.3f) | vista previa %s\n", st.instancias, st.cargadas, st.pidiendo, st.descargadas,
           st.pedidosVivos, st.generadasTotal, st.descargadasTotal, st.tickMs, st.tickPeorMs, g_w3dStreamingVistaPrevia ? "si" : "no");
    std::string k; long v = 0;
    while (ss >> k) {
        if (!(ss >> v)) { err = "streaminfo: falta el valor de '" + k + "'"; return false; }
        long real = -1;
        if (k == "instancias") real = st.instancias;
        else if (k == "cargadas") real = st.cargadas;
        else if (k == "pidiendo") real = st.pidiendo;
        else if (k == "descargadas") real = st.descargadas;
        else if (k == "pedidos") real = st.pedidosVivos;
        else { err = "streaminfo: assert desconocido '" + k + "'"; return false; }
        if (real != v) { err = "streaminfo " + k + ": hay " + Ent(real) + " y se esperaba " + Ent(v); return false; }
    }
    return true;
}
bool CmdStreamEstado(std::istringstream& ss, std::string& err) {
    std::string n, e; ss >> n >> e;
    InstanciaPrefab* ip = Inst(n, err, "streamestado"); if (!ip) return false;
    printf("      [streamestado] %s: %s (%d hijo(s), carga %s %.1f m%s%s)\n", n.c_str(), NombreEstado(ip->streamEstado),
           (int)ip->Childrens.size(), ip->carga == W3D_CARGA_DISTANCIA ? "distancia" : "siempre", ip->distancia,
           ip->objetivo.empty() ? "" : ", objetivo ", ip->objetivo.c_str());
    if (!e.empty() && e != NombreEstado(ip->streamEstado)) {
        err = "streamestado " + n + ": esta " + NombreEstado(ip->streamEstado) + " y se esperaba " + e; return false;
    }
    // cargada = con lo suyo generado; descargada = sin hijos
    if (e == "descargada" && !ip->Childrens.empty()) { err = "streamestado " + n + ": descargada pero con hijos"; return false; }
    return true;
}

// ---------------------------------------------------------------------------
//  streammem
// ---------------------------------------------------------------------------
struct Mem { int vivos[W3DREC_TIPOS]; long bytes[W3DREC_TIPOS]; int tex; long texBytes; long vbo; bool ok; Mem() : tex(0), texBytes(0), vbo(0), ok(false) {} };
Mem gBase;
Mem Medir() {
    Mem m;
    for (int t = 0; t < W3DREC_TIPOS; t++) { m.vivos[t] = W3dRecursosVivos(t); m.bytes[t] = W3dRecursosBytes(t); }
    m.tex = TexturasVivas() - TexturasBase();
    m.texBytes = TexturasBytes();
    m.vbo = W3dMallasVBOBytes();
    m.ok = true;
    return m;
}
bool CmdStreamMem(std::istringstream& ss, std::string& err) {
    const Mem m = Medir();
    printf("      [streammem] mallas=%d (%ld B, VBO %ld B) animsets=%d (%ld B) texturas=%d (%ld B de GPU) scripts=%d (%ld B)\n",
           m.vivos[W3DREC_MALLA], m.bytes[W3DREC_MALLA], m.vbo, m.vivos[W3DREC_ANIMSET], m.bytes[W3DREC_ANIMSET],
           m.tex, m.texBytes, m.vivos[W3DREC_SCRIPT], m.bytes[W3DREC_SCRIPT]);
    std::string k;
    while (ss >> k) {
        if (k == "base") { gBase = m; printf("      [streammem] base anotada\n"); continue; }
        if (k == "igual") {
            if (!gBase.ok) { err = "streammem igual: no hay base anotada"; return false; }
            const int tipos[3] = { W3DREC_MALLA, W3DREC_ANIMSET, W3DREC_SCRIPT };
            const char* nombres[3] = { "mallas", "animsets", "scripts" };
            for (int i = 0; i < 3; i++)
                if (m.vivos[tipos[i]] != gBase.vivos[tipos[i]] || m.bytes[tipos[i]] != gBase.bytes[tipos[i]]) {
                    err = std::string("streammem igual: ") + nombres[i] + " = " + Ent(m.vivos[tipos[i]]) + " (" + Ent(m.bytes[tipos[i]]) +
                          " B) y la base tenia " + Ent(gBase.vivos[tipos[i]]) + " (" + Ent(gBase.bytes[tipos[i]]) + " B)";
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
        if (k == "mallas") real = m.vivos[W3DREC_MALLA];
        else if (k == "animsets") real = m.vivos[W3DREC_ANIMSET];
        else if (k == "texturas") real = m.tex;
        else if (k == "scripts") real = m.vivos[W3DREC_SCRIPT];
        else { err = "streammem: assert desconocido '" + k + "'"; return false; }
        if (real != v) { err = "streammem " + k + ": hay " + Ent(real) + " y se esperaba " + Ent(v); return false; }
    }
    return true;
}

// ---------------------------------------------------------------------------
//  streamobjetivo / streamtick / streamvista / streamcarga / streamconfig
// ---------------------------------------------------------------------------
bool CmdStreamObjetivo(std::istringstream& ss, std::string& err) {
    std::string a; ss >> a;
    if (a == "off") { W3dStreamingObjetivoForzado(false, Vector3(0, 0, 0)); printf("      [streamobjetivo] suelto\n"); return true; }
    float x = 0, y = 0, z = 0;
    x = (float)atof(a.c_str());
    if (!(ss >> y >> z)) { err = "streamobjetivo: uso: streamobjetivo <x> <y> <z> | off"; return false; }
    W3dStreamingObjetivoForzado(true, Vector3(x, y, z));
    printf("      [streamobjetivo] (%.1f, %.1f, %.1f)\n", x, y, z);
    return true;
}
bool CmdStreamTick(std::istringstream& ss) {
    int n = 1; ss >> n; if (n < 1) n = 1;
    for (int i = 0; i < n; i++) W3dStreamingTickEditor();
    printf("      [streamtick] %d tick(s) de la vista previa (%s)\n", n, g_w3dStreamingVistaPrevia ? "prendida" : "APAGADA: no hace nada");
    return true;
}
bool CmdStreamVista(std::istringstream& ss) {
    int on = 1; ss >> on;
    W3dStreamingVistaPreviaFijar(on != 0);
    printf("      [streamvista] vista previa %s\n", on ? "prendida" : "apagada");
    return true;
}
bool CmdStreamCarga(std::istringstream& ss, std::string& err) {
    std::string n, modo, k; ss >> n >> modo;
    InstanciaPrefab* ip = Inst(n, err, "streamcarga"); if (!ip) return false;
    float d = ip->distancia; std::string obj = ip->objetivo;
    if (modo != "siempre" && modo != "distancia") { err = "streamcarga: uso: streamcarga <objeto> siempre|distancia [D] [objetivo X]"; return false; }
    while (ss >> k) {
        if (k == "objetivo") { ss >> obj; if (obj == "-") obj.clear(); }
        else d = (float)atof(k.c_str());
    }
    W3dInstanciaCargaCambiar(ip, modo == "distancia" ? W3D_CARGA_DISTANCIA : W3D_CARGA_SIEMPRE, d, obj);
    printf("      [streamcarga] %s: %s %.1f m%s%s (%s)\n", n.c_str(), modo.c_str(), d, obj.empty() ? "" : " objetivo ", obj.c_str(),
           NombreEstado(ip->streamEstado));
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
    Texture* f1 = TexturaTomar(a);   // (las referencias de los cuadros son del material animado)
    Texture* f2 = TexturaTomar(b);
    if (!f1 || !f2) { delete am; err = "streamanimmat: no pude cargar los cuadros"; return false; }
    am->frameTextures.push_back(f1); am->frameDurations.push_back(1);
    am->frameTextures.push_back(f2); am->frameDurations.push_back(1);
    AnimatedMaterials.push_back(am);
    for (size_t g = 0; g < m->materialsGroup.size(); g++) m->materialsGroup[g].material = mat;
    if (m->malla) for (size_t g = 0; g < m->malla->partes.size(); g++) W3dMallaRecursoCambiarMaterial(m->malla, (int)g, mat);
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
    std::string ruta; int version = 0; ss >> ruta >> version;
    if (ruta.empty()) { err = "streamscript: uso: streamscript <ruta.lua> <version>"; return false; }
    char b[512];
    snprintf(b, sizeof(b),
             "-- marca de version (prueba de streaming: el editor corre siempre la fuente actual)\n"
             "function inicio()\n"
             "  setCompartido(\"version\", (compartido(\"version\") or 0) + %d)\n"
             "  setCompartido(\"arranques\", (compartido(\"arranques\") or 0) + 1)\n"
             "end\n", version);
    if (!Escribir(ruta, b)) { err = "streamscript: no pude escribir '" + ruta + "'"; return false; }
    printf("      [streamscript] '%s': version %d\n", ruta.c_str(), version);
    return true;
}
bool CmdStreamFijar(std::istringstream& ss, std::string& err) {
    std::string n, q; ss >> n >> q;
    InstanciaPrefab* ip = Inst(n, err, "streamfijar"); if (!ip) return false;
    const int pedido = (q == "cargar") ? 1 : (q == "descargar") ? -1 : (q == "auto") ? 2 : 0;
    if (!pedido) { err = "streamfijar: uso: streamfijar <objeto> cargar|descargar|auto"; return false; }
    W3dStreamingPedido(ip, pedido);
    printf("      [streamfijar] %s: %s (se aplica en el proximo tick)\n", n.c_str(), q.c_str());
    return true;
}
bool CmdStreamConfig(std::istringstream& ss) {
    std::string k; float v = 0;
    while (ss >> k >> v) {
        if (k == "cargas") g_w3dStreamingConfig.cargasPorFrame = (int)v;
        else if (k == "generar") g_w3dStreamingConfig.generarPorFrame = (int)v;
        else if (k == "descargar") g_w3dStreamingConfig.descargarPorFrame = (int)v;
        else if (k == "presupuesto") g_w3dStreamingConfig.presupuestoMs = v;
    }
    printf("      [streamconfig] cargas/frame=%d generar/frame=%d descargar/frame=%d histeresis=%.0f%% presupuesto=%.1f ms\n",
           g_w3dStreamingConfig.cargasPorFrame, g_w3dStreamingConfig.generarPorFrame, g_w3dStreamingConfig.descargarPorFrame,
           g_w3dStreamingConfig.histeresis * 100.0f, g_w3dStreamingConfig.presupuestoMs);
    return true;
}

// ---------------------------------------------------------------------------
//  streambench
// ---------------------------------------------------------------------------
struct Bench { long rss0, rssMax, rssFin, heap0, heapMax, heapFin; double peor; };
std::map<std::string, Bench> gBenchs;
bool CmdStreamBench(std::istringstream& ss, std::string& err) {
    int frames = 0; ss >> frames;
    std::string k, etiqueta, como;
    double cotaPeor = -1.0; long cotaHeap = -1, margenRss = 0;
    while (ss >> k) {
        if (k == "etiqueta") ss >> etiqueta;
        else if (k == "peor") ss >> cotaPeor;
        else if (k == "heapcrece") ss >> cotaHeap;
        else if (k == "rssmaxcomo") ss >> como >> margenRss;
    }
    if (frames < 1) { err = "streambench: uso: streambench <frames> [etiqueta T] [peor MS] [heapcrece KB] [rssmaxcomo T2 KB]"; return false; }
    const float dt = 1.0f / 30.0f;
    std::vector<double> t;
    t.reserve((size_t)frames);
    W3dStreamingStatsReset();
    Bench b;
    b.rss0 = b.rssMax = RssKb();
    b.heap0 = b.heapMax = HeapKb();
    int cargMax = 0;
    for (int i = 0; i < frames; i++) {
        const double t0 = W3dNowMs();
        // (lo mismo que simplay por tick)
        SimTickPlay(dt);
        W3dParticulasTick(dt);
        W3dVisZonasTick();
        if (PlayAnimation) UpdateAnimations(dt);
        t.push_back(W3dNowMs() - t0);
        if (i % 5 == 0 || i + 1 == frames) {
            const long r = RssKb(), h = HeapKb();
            if (r > b.rssMax) b.rssMax = r;
            if (h > b.heapMax) b.heapMax = h;
            W3dStreamingStats st;
            W3dStreamingEstadisticas(st);
            if (st.cargadas > cargMax) cargMax = st.cargadas;
        }
    }
    b.rssFin = RssKb();
    b.heapFin = HeapKb();
    std::vector<double> o = t;
    std::sort(o.begin(), o.end());
    double suma = 0.0;
    for (size_t i = 0; i < t.size(); i++) suma += t[i];
    b.peor = o.back();
    const double p99 = o[(size_t)((double)(o.size() - 1) * 0.99)];
    W3dStreamingStats st;
    W3dStreamingEstadisticas(st);
    printf("      [streambench] %s frames=%d peor=%.2f ms p99=%.2f ms medio=%.3f ms | tick streaming peor=%.2f ms | "
           "rssKB=%ld/%ld/%ld heapKB=%ld/%ld/%ld (inicio/max/fin) | cargadasMax=%d generadas=%ld descargadas=%ld | "
           "texturas en cola=%d dormidas=%d | sim %s\n",
           etiqueta.empty() ? "-" : etiqueta.c_str(), frames, b.peor, p99, suma / (double)frames, st.tickPeorMs,
           b.rss0, b.rssMax, b.rssFin, b.heap0, b.heapMax, b.heapFin, cargMax, st.generadasTotal, st.descargadasTotal,
           TexturasPendientes(), TexturasDormidasCantidad(), SimActiva() ? "activa" : "SIN partida");
    if (!etiqueta.empty()) gBenchs[etiqueta] = b;
    if (cotaPeor > 0.0 && b.peor > cotaPeor) {
        char m[160]; sprintf(m, "streambench: el peor frame tardo %.2f ms (cota %.1f ms)", b.peor, cotaPeor); err = m; return false;
    }
    // (con un ASIGNADOR INSTRUMENTADO -ASan: mallinfo2 da 0 y lo liberado queda en cuarentena, el RSS solo crece- la
    //  memoria no se puede asertar: se informa y se sigue)
    const bool medible = b.heap0 > 0;
    if (!medible && (cotaHeap >= 0 || !como.empty()))
        printf("      [streambench] el heap no se puede medir aca (asignador instrumentado): sin cotas de memoria\n");
    if (medible && cotaHeap >= 0 && b.heapFin - b.heap0 > cotaHeap) {
        char m[160]; sprintf(m, "streambench: el heap en uso crecio %ld KB (cota %ld KB)", b.heapFin - b.heap0, cotaHeap); err = m; return false;
    }
    if (medible && !como.empty()) {
        std::map<std::string, Bench>::iterator it = gBenchs.find(como);
        if (it == gBenchs.end()) { err = "streambench: no hay una corrida '" + como + "'"; return false; }
        if (b.rssMax > it->second.rssMax + margenRss) {
            char m[200]; sprintf(m, "streambench: el RSS maximo (%ld KB) paso el de '%s' (%ld KB) por mas de %ld KB", b.rssMax,
                                 como.c_str(), it->second.rssMax, margenRss);
            err = m; return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
//  streamtexrefs
// ---------------------------------------------------------------------------
bool CmdStreamTexRefs(std::istringstream& ss, std::string& err) {
    std::string k; bool exacto = false;
    while (ss >> k) if (k == "exacto") exacto = true;
    std::map<const Texture*, int> ranuras;
    for (size_t i = 0; i < Materials.size(); i++) {
        const Material* m = Materials[i];
        if (!m) continue;
        if (m->texture) ranuras[m->texture]++;
        if (m->normalTexture) ranuras[m->normalTexture]++;
        for (size_t c = 0; c < m->capas.size(); c++) if (m->capas[c].tex) ranuras[m->capas[c].tex]++;
    }
    for (size_t i = 0; i < AnimatedMaterials.size(); i++)
        if (AnimatedMaterials[i])
            for (size_t f = 0; f < AnimatedMaterials[i]->frameTextures.size(); f++)
                if (AnimatedMaterials[i]->frameTextures[f]) ranuras[AnimatedMaterials[i]->frameTextures[f]]++;
    // los PEDIDOS del streaming (las instancias de todas las raices cargadas)
    std::vector<Object*> raices;
    W3dRaicesEnOrden(raices);
    if (SceneCollection && std::find(raices.begin(), raices.end(), SceneCollection) == raices.end()) raices.push_back(SceneCollection);
    for (size_t r = 0; r < raices.size(); r++) {
        std::vector<Object*> pila(1, raices[r]);
        while (!pila.empty()) {
            Object* o = pila.back(); pila.pop_back();
            for (size_t i = 0; i < o->Childrens.size(); i++) if (o->Childrens[i]) pila.push_back(o->Childrens[i]);
            if (!W3dEsTipoInstancia(o->getType())) continue;
            const InstanciaPrefab* ip = (const InstanciaPrefab*)o;
            for (size_t q = 0; q < ip->streamPedidos.size(); q++) {
                const W3dPedidoStream& pd = ip->streamPedidos[q];
                if (!W3dRecursoVigente(pd.r, pd.serie)) continue;
                const W3dRecurso* r = pd.r;
                if (r->tipo == W3DREC_TEXTURA && r->estado == W3DREC_LISTO && r->dato) ranuras[(const Texture*)r->dato]++;
            }
        }
    }
    int malas = 0, distintas = 0, n = 0;
    for (size_t i = (size_t)TexturasBase(); i < Textures.size(); i++) {
        const Texture* t = Textures[i];
        if (!t) continue;
        W3dRecurso* r = W3dRecursoBuscar(W3DREC_TEXTURA, t->path);
        const int refs = (r && r->dato == (void*)t) ? r->refTotal : t->refs;
        const int nr = ranuras.count(t) ? ranuras[t] : 0;
        n++;
        if (refs < nr) { malas++; printf("      [streamtexrefs] MAL %s: %d referencia(s) y %d dueno(s)\n", t->path.c_str(), refs, nr); }
        else if (refs != nr) { distintas++; printf("      [streamtexrefs] %s: %d referencia(s) y %d dueno(s)\n", t->path.c_str(), refs, nr); }
    }
    // una RANURA COLGADA: apunta a una textura que no esta viva ni en el cementerio (se libero en uso). Solo se
    // compara el puntero (no se lee: ya no existe)
    std::set<const Texture*> existen(Textures.begin(), Textures.end());
    existen.insert(TexturasPurgadas().begin(), TexturasPurgadas().end());
    for (std::map<const Texture*, int>::const_iterator it = ranuras.begin(); it != ranuras.end(); ++it)
        if (!existen.count(it->first)) { malas++; printf("      [streamtexrefs] MAL una ranura apunta a una textura liberada (%p)\n", (const void*)it->first); }
    printf("      [streamtexrefs] %d textura(s): %d con menos referencias que duenos, %d con otra cuenta\n", n, malas, distintas);
    if (malas) { err = "streamtexrefs: hay texturas con menos referencias que duenos (se podrian liberar en uso)"; return false; }
    if (exacto && distintas) { err = "streamtexrefs: hay texturas con mas referencias que duenos"; return false; }
    return true;
}

// ---------------------------------------------------------------------------
//  streamtarjeta
// ---------------------------------------------------------------------------
ViewportBase* HojaProps(ViewportBase* n) {
    if (!n) return NULL;
    if (n->isLeaf()) return n->ViewportKind() == 3 ? n : NULL;
    ViewportBase* a = NULL; ViewportBase* b = NULL;
    if (n->ContainerKind() == 1) { a = ((ViewportRow*)n)->childA; b = ((ViewportRow*)n)->childB; }
    else { a = ((ViewportColumn*)n)->childA; b = ((ViewportColumn*)n)->childB; }
    ViewportBase* r = HojaProps(a);
    return r ? r : HojaProps(b);
}
void RefrescarProps(Properties* p) {
    p->RefreshTargetProperties();
    p->ActualizarPestanias();
    PropsActivo = p;
    if (p->pestaniaActiva != 2 && p->BarTabs.size() > 2 && p->BarTabs[2]->visible) { p->pestaniaActiva = 2; p->ActualizarPestanias(); }
    rootViewport->Render();
}
bool CmdStreamTarjeta(std::istringstream& ss, std::string& err) {
    Properties* p = (Properties*)HojaProps(LayoutRaizCompleta());
    if (!p) { err = "streamtarjeta: no hay panel de propiedades"; return false; }
    RefrescarProps(p);
    std::vector<std::pair<std::string, std::string> > asserts;
    std::string k, v;
    while (ss >> k >> v) {
        if (k == "editardistancia") {
            if (!p->propPfDistancia || !p->propPfDistancia->value) { err = "streamtarjeta: la fila Distance no esta a la vista"; return false; }
            p->propPfDistancia->Set((float)atof(v.c_str()));   // (lo que hace el arrastre)
            RefrescarProps(p);                                 // (el mouse esta suelto: el paso se anota)
            RefrescarProps(p);
        } else asserts.push_back(std::make_pair(k, v));
    }
    const bool vis = p->propPrefab && p->propPrefab->visible;
    const std::string modo = (vis && p->propPfCarga && !p->propPfCarga->oculto) ? p->propPfCarga->button->text : std::string("-");
    const bool dist = vis && p->propPfDistancia && p->propPfDistancia->value;
    const bool obj = vis && p->propPfObjetivo && !p->propPfObjetivo->oculto;
    const bool vista = vis && p->propPfVista && p->propPfVista->value;
    printf("      [streamtarjeta] '%s': tarjeta %s | carga '%s' | distancia %s | objetivo %s | vista previa %s | %s\n",
           ObjActivo ? ObjActivo->name.c_str() : "-", vis ? "visible" : "oculta", modo.c_str(),
           dist ? JsonNumTextoLocal(*p->propPfDistancia->value).c_str() : "-", obj ? "visible" : "-", vista ? "visible" : "-",
           (vis && p->propPfInfo) ? p->propPfInfo->name.c_str() : "");
    for (size_t i = 0; i < asserts.size(); i++) {
        const std::string& a = asserts[i].first;
        const std::string& b = asserts[i].second;
        bool ok = true;
        if (a == "visible") ok = (vis ? 1 : 0) == atoi(b.c_str());
        else if (a == "modo") ok = modo == (b == "distancia" ? std::string(T("By Distance")) : std::string(T("Always")));
        else if (a == "distancia") ok = dist && fabsf(*p->propPfDistancia->value - (float)atof(b.c_str())) < 1e-3f;
        else if (a == "objetivo") ok = (obj ? 1 : 0) == atoi(b.c_str());
        else if (a == "vista") ok = (vista ? 1 : 0) == atoi(b.c_str());
        else { err = "streamtarjeta: assert desconocido '" + a + "'"; return false; }
        if (!ok) { err = "streamtarjeta: " + a + " no es " + b; return false; }
    }
    return true;
}

// ---------------------------------------------------------------------------
//  streamveredicto / streamjuegolog
// ---------------------------------------------------------------------------
std::string gVeredictoPlay;
bool CmdStreamVeredicto(std::string& err) {
    gVeredictoPlay.clear();
    for (int i = 0; i < W3dScriptCompartidoCantidad(); i++) {
        std::string n, v;
        if (W3dScriptCompartidoPar(i, &n, &v) && n == "veredicto") gVeredictoPlay = v;
    }
    printf("      [streamveredicto] el Play dijo: %s\n", gVeredictoPlay.empty() ? "(nada)" : gVeredictoPlay.c_str());
    if (gVeredictoPlay.empty()) { err = "streamveredicto: el Play no dejo compartido(\"veredicto\")"; return false; }
    return true;
}
bool CmdStreamJuegoLog(std::istringstream& ss, std::string& err) {
    std::string dir, nombre, k; ss >> dir >> nombre;
    bool igual = false;
    while (ss >> k) if (k == "igual") igual = true;
    const std::string carpeta = dir + "/build/linux";
    const std::string bin = carpeta + "/" + nombre;
    if (!w3dFileSystem::FileExists(bin)) { err = "streamjuegolog: no existe el binario compilado '" + bin + "'"; return false; }
    const std::string delPlay = gVeredictoPlay;   // (lo anoto streamveredicto)
    const std::string log = carpeta + "/whisk3d.log";
    remove(log.c_str());
    char cmdRun[2200];
    snprintf(cmdRun, sizeof(cmdRun), "cd \"%s\" && timeout 120 ./%s > /dev/null 2>&1", carpeta.c_str(), nombre.c_str());
    const int r = system(cmdRun);
    FILE* f = fopen(log.c_str(), "rb");
    if (!f) { err = "streamjuegolog: el juego no dejo whisk3d.log (se compilo en modo debug?)"; return false; }
    std::string veredicto;
    char buf[4096];
    while (fgets(buf, sizeof(buf), f)) {
        const char* p = strstr(buf, "[streaming] ");
        if (p && (strstr(p, " OK ") || strstr(p, " FALTA "))) veredicto = p;
    }
    fclose(f);
    while (!veredicto.empty() && (veredicto[veredicto.size() - 1] == '\n' || veredicto[veredicto.size() - 1] == '\r'))
        veredicto.erase(veredicto.size() - 1);
    printf("      [streamjuegolog] salida=%d | %s\n", r, veredicto.empty() ? "(sin veredicto)" : veredicto.c_str());
    if (veredicto.find("[streaming] OK") == std::string::npos) {
        err = "streamjuegolog: el juego compilado no dio el veredicto OK"; return false;
    }
    if (igual) {
        if (delPlay.empty()) { err = "streamjuegolog igual: no hay veredicto del Play (streamveredicto)"; return false; }
        // el veredicto termina en "=<lista>": la lista del Play tiene que ser la misma
        const size_t eq = veredicto.rfind('=');
        const std::string lista = (eq == std::string::npos) ? std::string() : veredicto.substr(eq + 1);
        printf("      [streamjuegolog] Play: %s | juego: %s\n", delPlay.c_str(), lista.c_str());
        if (lista != delPlay) { err = "streamjuegolog: el juego compilado dio '" + lista + "' y el Play '" + delPlay + "'"; return false; }
    }
    return true;
}

} // namespace

bool W3dPruebasStreamingCmd(const std::string& cmd, std::istringstream& ss, std::string& err, bool& manejado) {
    manejado = true;
    if (cmd == "streamcrear")    return CmdStreamCrear(ss, err);
    if (cmd == "streamlua")      return CmdStreamLua(ss, err);
    if (cmd == "streamanidada")  return CmdStreamAnidada(ss, err);
    if (cmd == "streaminfo")     return CmdStreamInfo(ss, err);
    if (cmd == "streamestado")   return CmdStreamEstado(ss, err);
    if (cmd == "streammem")      return CmdStreamMem(ss, err);
    if (cmd == "streamobjetivo") return CmdStreamObjetivo(ss, err);
    if (cmd == "streamtick")     return CmdStreamTick(ss);
    if (cmd == "streamvista")    return CmdStreamVista(ss);
    if (cmd == "streamcarga")    return CmdStreamCarga(ss, err);
    if (cmd == "streamconfig")   return CmdStreamConfig(ss);
    if (cmd == "streambench")    return CmdStreamBench(ss, err);
    if (cmd == "streamtexrefs")  return CmdStreamTexRefs(ss, err);
    if (cmd == "streamjuegolog") return CmdStreamJuegoLog(ss, err);
    if (cmd == "streamveredicto") return CmdStreamVeredicto(err);
    if (cmd == "streamtarjeta")  return CmdStreamTarjeta(ss, err);
    if (cmd == "streamfijar")    return CmdStreamFijar(ss, err);
    if (cmd == "streamanimmat")  return CmdStreamAnimMat(ss, err);
    if (cmd == "streamanimtick") return CmdStreamAnimTick(ss);
    if (cmd == "streamundolimpiar") { UndoLimpiar(); printf("      [streamundolimpiar] historial vacio\n"); return true; }
    if (cmd == "streamscript")   return CmdStreamScript(ss, err);
    manejado = false;
    return false;
}
