// ============================================================================
//  W3dPruebasLibrerias.cpp — comandos de harness de las LIBRERIAS EXTERNAS y los
//  PROXIES W3D (io/Librerias.h, objects/ProxyW3d.h, io/BibliotecaExterna.h).
//
//  Comandos:
//    libcrear <carpeta> <archivo.w3d> <textura.png> [prefab <nombre>]
//        una LIBRERIA de prueba: prefabenemigo (el prefab "Enemigo": Esqueleto con dos clips + Cuerpo
//        skinneado + Sensor + enemigo.lua) y ademas: el Cuerpo con el material "Piel" CON TEXTURA (la png
//        dada, copiada a la carpeta), un prefab "Arbol" (un cubo "Tronco", recurso "Tronco", material
//        "Corteza"), un prefab "Lanzador" (un vacio con lanzador.lua, que al arrancar hace instanciar("Arbol") -el
//        de SU libreria- y deja en compartido("lanzado") el tipo de lo creado) y una ESCENA "Decorado" (dos cubos
//        "Roca" y "Piedra" que comparten el recurso "Roca").
//        Con 'prefab <nombre>' el prefab del personaje (y su objeto raiz) se llama asi en vez de "Enemigo".
//        La guarda en <carpeta>/<archivo.w3d> (queda abierta como proyecto).
//    libproxy <lib> <prefab|escena> <elem> <x> <y> <z> [veces N] [falla]
//        proxies nuevos (W3dProxyAgregar: la puerta de Add y de soltar, con Ctrl+Z), N en fila cada 2 en x
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
//    libcambiar <proxy> <lib> <prefab|escena> <elem> [falla]   W3dProxyCambiar (con Ctrl+Z)
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
//        (libdirector.lua) que JUGANDO instancia 5 mas con instanciar("<lib>/<prefab>", ...) y deja su veredicto en
//        compartido("juez") y en el log ("[librerias] OK creados=5 enemigos=8 scope=8 vida=95" con 3)
//    libjuegolog <carpeta> <nombre> [claves]...     corre el juego compilado y exige ese veredicto
//    libcrearextras <carpeta> <archivo.w3d> <textura.png> <otra.w3d>
//        una SEGUNDA libreria ("extras") que VINCULA a <otra.w3d> (una libreria ADENTRO de otra) y trae:
//          prefab "Cartel"  : un cubo "Tablero" con el material "Pintura" cuya textura queda AFUERA de su .w3d
//                             (fuera/cartel.png, "ext:") y un script que tambien vive afuera (fuera/externo.lua):
//                             hace importarW3D() de un archivo SUELTO al lado de la libreria (anexos/piedra.w3d)
//                             y de una ENTRADA suya (extra/roca.w3d), y deja compartido("anexos"/"externo");
//          prefab "Escuadra": un proxy del "Enemigo" de <otra> (la de adentro) + un cubo "Viga" con la malla
//                             "<otra>/Tronco" (un recurso de la de adentro);
//          prefab "Sonoro"  : sonoro.lua hace sonido() de uno SUELTO (sonidos/bip.wav) y de una ENTRADA
//                             (sonidos/entrada.wav) de SU libreria;
//          prefab "Llamador": llamador.lua hace instanciar("personajes/Arbol") (la de adentro, por el nombre de SU
//                             registro) y deja compartido("llamado"/"tronco");
//          escena "Panel"   : una escena UI "Tablerito" con dos imagenes: "Fondo" (texturas/logo.png) e "Icono"
//                             (sin textura; su panel.lua le pone "texturas/logo.png" con setTextura).
//    libmarca <proxy> <ruta|proxy> <icono|->   la MARCA del outliner despues del nombre (el candado de lo
//        generado por un proxy; "-" = ninguna). 'proxy' = el proxy mismo
//    libpropsmalla <pestania 2|3|4> [lectura 0|1] [intentar]
//        la malla ACTIVA en Properties: 'lectura' = sus filas de edicion (partes / grupos, mapas UV y capas /
//        modificadores, segun la pestania) estan ocultas y se ve el aviso de la libreria. 'intentar' llama a
//        las acciones igual (New Mesh Part / Add Vertex Group y Add UV Map / Remove de un modificador) y exige que
//        no cambien nada
//    libsonido <ruta> [cacheado 0|1]      sonido() ya resolvio esa ruta (el cache de BindsJuego)
//    libtexcargar                         carga ya las texturas de la cola diferida (lo que despertaron los proxies)
//    libanidada <de> <nombre> [como G]    W3dLibsAnidada: con que nombre vive la libreria de adentro
//    libimagen <nombre> [textura T] [script S]   una Imagen2D (de cualquier lado del arbol): su textura y su script
//    libjuegoextras <carpeta> <extras.w3d>
//        el juego 3D minimo + la libreria "extras" VINCULADA (la de adentro NO: se monta oculta) + un proxy de
//        Cartel, Escuadra y Sonoro + un Director con el veredicto ("[librerias] OK externo=OK anexos=Piedra+Roca
//        enemigos=1 sonoro=listo")
//    libmover <de> <a>                    renombra un archivo de disco (una libreria que FALTA y despues vuelve)
// ============================================================================
#include "test/W3dPruebasLibrerias.h"
#include "test/W3dScript.h"            // W3dRunCommand("prefabenemigo ...")
#include "objects/Objects.h"
#include "objects/InstanciaPrefab.h"
#include "objects/ProxyW3d.h"
#include "objects/Mesh.h"
#include "objects/MallaRecurso.h"
#include "objects/Materials.h"
#include "objects/Textures.h"
#include "objects/Armature.h"
#include "objects/Empty.h"
#include "animation/W3dAnimSet.h"
#include "io/Librerias.h"
#include "io/BibliotecaExterna.h"
#include "io/Prefabs.h"
#include "io/PrefabsEditor.h"
#include "io/RaicesEditor.h"
#include "io/RecursosProyecto.h"
#include "io/MallasProyecto.h"
#include "io/GuardarW3D.h"
#include "io/W3dRecursos.h"
#include "io/W3dAlmacen.h"           // W3dAlmacenPorNombre / W3dRutaDeLibreria
#include "importers/import_obj.h"      // CargarTodasTexturasPendientes
#include "io/W3dContenedor.h"          // W3dImportarAsset / W3dRefExternaMarcar (la libreria "extras")
#include "io/UI2DFormato.h"
#include "objects/UI.h"
#include "objects/Imagen2D.h"
#include "script/BindsJuego.h"         // W3dSonidoCacheado
#include "objects/ObjectMode.h"        // W3dRenombrarObjeto
#include "variables.h"                 // w3dPath (la libreria "extras" se guarda y se re-guarda)
#include "ViewPorts/Outliner.h"        // OutlinerMarcaDeObjeto
#include "WhiskUI/draw/icons.h"        // IconoNombre
#include "ViewPorts/LayoutInput.h"     // LayoutToggleEditMode / LayoutModoElegir
#include "ViewPorts/Properties.h"      // libpropsmat: la tarjeta Material y la del proxy
#include "WhiskUI/Propieties/PropLabel.h"
#include "ViewPorts/LayoutArbol.h"     // LayoutRaizCompleta
#include "ViewPorts/ViewPorts.h"       // ViewportRow / ViewportColumn (recorrer el layout)
#include "base/W3dInteractionState.h"
#include "script/W3dScript.h"
#include "W3dRaices.h"
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

// un script DE LA LIBRERIA: instanciar("Arbol") nombra el Arbol de SU libreria (un proxy), no uno del proyecto
const char* kLuaLanzador =
    "-- un script de la libreria de prueba: instanciar() de un prefab de la MISMA libreria, por su nombre pelado\n"
    "function inicio()\n"
    "  local e = instanciar(\"Arbol\", 0, 0, 5)\n"
    "  setCompartido(\"lanzado\", e and tipo(e) or \"nada\")\n"
    "end\n";

const char* kLuaDirector =
    "-- director de la prueba de librerias: instanciar() de un prefab de una LIBRERIA vinculada, con un juez\n"
    "propiedades = { lib = \"personajes\", elem = \"Enemigo\", base = 3, vida = 95 }\n"
    "local fase, espera, creados = 0, 0, 0\n"
    "function actualizar(dt)\n"
    "  if fase == 0 then\n"
    "    for i = 1, 5 do\n"
    "      local e = instanciar(propiedad(\"lib\") .. \"/\" .. propiedad(\"elem\"), i * 2, 0, -20, 90)\n"
    "      if e then creados = creados + 1 end\n"
    "    end\n"
    "    fase = 1\n"
    "    return\n"
    "  end\n"
    "  if fase == 1 then\n"
    "    espera = espera + 1\n"
    "    if espera < 5 then return end\n"
    "    local n = tonumber(propiedad(\"base\")) + creados\n"
    "    local ok = creados == 5 and (compartido(\"enemigos\") or 0) == n and (compartido(\"scope_ok\") or 0) == n\n"
    "      and (compartido(\"vida_total\") or 0) == tonumber(propiedad(\"vida\"))\n"
    "    info(string.format(\"[librerias] %s creados=%d enemigos=%d scope=%d vida=%d\", ok and \"OK\" or \"FALTA\",\n"
    "      creados, compartido(\"enemigos\") or -1, compartido(\"scope_ok\") or -1, compartido(\"vida_total\") or -1))\n"
    "    setCompartido(\"juez\", ok and \"OK\" or \"FALTA\")\n"
    "    fase = 2\n"
    "    salir()\n"
    "  end\n"
    "end\n";

std::string Ent(long n) { char b[32]; sprintf(b, "%ld", n); return b; }
int TipoDe(const std::string& t) { return t == "escena" ? W3D_LIB_ESCENA : W3D_LIB_PREFAB; }

bool Copiar(const std::string& de, const std::string& a) {
    std::vector<unsigned char> d;
    if (!w3dFileSystem::ReadFileBytes(de, d)) return false;
    FILE* f = fopen(a.c_str(), "wb");
    if (!f) return false;
    if (!d.empty()) fwrite(&d[0], 1, d.size(), f);
    return fclose(f) == 0;
}
bool Escribir(const std::string& ruta, const char* texto) {
    FILE* f = fopen(ruta.c_str(), "wb");
    if (!f) return false;
    fputs(texto, f);
    return fclose(f) == 0;
}
std::string Absoluta(const std::string& r) {
    if (!r.empty() && r[0] == '/') return r;
    char cwd[2048];
    const std::string d = getcwd(cwd, sizeof(cwd)) ? std::string(cwd) : std::string(".");
    return d + "/" + r;
}
ProxyW3d* Px(const std::string& n, std::string& err, const char* quien) {
    Object* o = SceneCollection ? FindObjectByName(SceneCollection, n) : NULL;
    if (!o || o->getType() != ObjectType::proxy) { err = std::string(quien) + ": no hay un proxy '" + n + "'"; return NULL; }
    return (ProxyW3d*)o;
}
int Contar(Object* o) {
    int n = 0;
    for (size_t i = 0; i < o->Childrens.size(); i++) n += 1 + Contar(o->Childrens[i]);
    return n;
}
void JuntarTipo(Object* o, ObjectType t, std::vector<Object*>& out) {
    if (!o) return;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        if (o->Childrens[i]->getType() == t) out.push_back(o->Childrens[i]);
        JuntarTipo(o->Childrens[i], t, out);
    }
}
void Script(Object* o, const char* ruta);   // (mas abajo)
// un cubo con su recurso de malla y un material nuevo (la libreria de prueba)
Mesh* Cubo(Object* padre, const char* nombre, const char* recurso, Material* mat) {
    Mesh* m = (Mesh*)NewMesh(MeshType(MeshType::cube), padre, false);
    if (!m) return NULL;
    m->SetNameObj(nombre);
    if (!m->malla || m->malla->nombre != recurso) W3dMallaCrearRecurso(m, recurso);
    if (mat) {
        for (size_t g = 0; g < m->materialsGroup.size(); g++) m->materialsGroup[g].material = mat;
        if (m->malla) for (size_t g = 0; g < m->malla->partes.size(); g++) W3dMallaRecursoCambiarMaterial(m->malla, (int)g, mat);
    }
    return m;
}

// ---------------------------------------------------------------------------
bool CmdLibCrear(std::istringstream& ss, std::string& err) {
    std::string dir, archivo, png, k, personaje = "Enemigo"; ss >> dir >> archivo >> png;
    while (ss >> k) { if (k == "prefab") ss >> personaje; }
    if (dir.empty() || archivo.empty() || png.empty() || personaje.empty()) {
        err = "libcrear: uso: libcrear <carpeta> <archivo.w3d> <textura.png> [prefab <nombre>]"; return false;
    }
    std::string e2;
    if (!W3dRunCommand("prefabenemigo " + dir, e2)) { err = "libcrear: " + e2; return false; }
    const std::string tex = dir + "/piel.png";
    if (!Copiar(png, tex)) { err = "libcrear: no pude copiar la textura '" + png + "'"; return false; }
    std::string motivo;
    // el CUERPO del prefab "Enemigo" con un material CON TEXTURA
    const int ie = W3dRaizBuscar(W3D_RAIZ_PREFAB, "Enemigo");
    if (ie < 0 || !W3dActivarRaiz(ie, &motivo)) { err = "libcrear: no pude abrir el prefab Enemigo " + motivo; return false; }
    Mesh* cuerpo = (Mesh*)FindObjectByName(SceneCollection, "Cuerpo");
    if (!cuerpo || cuerpo->getType() != ObjectType::mesh) { err = "libcrear: el prefab no tiene Cuerpo"; return false; }
    Material* piel = new Material("Piel");
    piel->diffuse[0] = 0.9f; piel->diffuse[1] = 0.7f; piel->diffuse[2] = 0.6f;
    piel->texture = TexturaTomar(tex);
    piel->textureOn = true;
    if (!piel->texture) { err = "libcrear: no pude cargar la textura"; return false; }
    for (size_t g = 0; g < cuerpo->materialsGroup.size(); g++) cuerpo->materialsGroup[g].material = piel;
    if (cuerpo->malla) for (size_t g = 0; g < cuerpo->malla->partes.size(); g++) W3dMallaRecursoCambiarMaterial(cuerpo->malla, (int)g, piel);
    // (otro NOMBRE para el prefab del personaje: el prefab y su objeto raiz, como si el usuario lo hubiera renombrado)
    if (personaje != "Enemigo") {
        Object* raizObj = FindObjectByName(SceneCollection, "Enemigo");
        std::string quedo;
        if (!raizObj || !W3dRaizRenombrar(ie, personaje, &quedo) || quedo != personaje) {
            err = "libcrear: no pude renombrar el prefab Enemigo a '" + personaje + "'"; return false;
        }
        raizObj->SetNameObj(personaje);
        W3dPrefabRenombrado("Enemigo", personaje);
    }
    // un prefab "Arbol": un cubo "Tronco" con material "Corteza"
    const int ia = W3dRaizCrearYAbrir(W3D_RAIZ_PREFAB, "Arbol");
    if (ia < 0 || SceneCollection->Childrens.empty()) { err = "libcrear: no pude crear el prefab Arbol"; return false; }
    Material* corteza = new Material("Corteza");
    corteza->diffuse[0] = 0.4f; corteza->diffuse[1] = 0.25f; corteza->diffuse[2] = 0.1f;
    if (!Cubo(SceneCollection->Childrens[0], "Tronco", "Tronco", corteza)) { err = "libcrear: NewMesh fallo"; return false; }
    // un prefab "Lanzador": su script instancia el Arbol de la MISMA libreria
    if (!Escribir(dir + "/lanzador.lua", kLuaLanzador)) { err = "libcrear: no pude escribir lanzador.lua"; return false; }
    const int il = W3dRaizCrearYAbrir(W3D_RAIZ_PREFAB, "Lanzador");
    if (il < 0 || SceneCollection->Childrens.empty()) { err = "libcrear: no pude crear el prefab Lanzador"; return false; }
    Script(SceneCollection->Childrens[0], "lanzador.lua");
    // una ESCENA "Decorado": dos cubos que COMPARTEN el recurso "Roca"
    const int id = W3dRaizCrearYAbrir(W3D_RAIZ_ESCENA, "Decorado");
    if (id < 0) { err = "libcrear: no pude crear la escena Decorado"; return false; }
    Mesh* roca = Cubo(NULL, "Roca", "Roca", corteza);
    Mesh* piedra = (Mesh*)NewMesh(MeshType(MeshType::cube), NULL, false);
    if (!roca || !piedra) { err = "libcrear: NewMesh fallo"; return false; }
    piedra->SetNameObj("Piedra");
    piedra->pos = Vector3(3, 0, 0);
    if (!roca->malla || !W3dMallaAsignar(piedra, roca->malla)) { err = "libcrear: no pude compartir la Roca"; return false; }
    if (!W3dActivarRaiz(W3dRaizBloque(), &motivo)) { err = "libcrear: " + motivo; return false; }
    DeseleccionarTodo(); ObjActivo = NULL;
    // los .lua de prefabenemigo van por nombre pelado ("enemigo.lua"): con su ruta de la carpeta, el guardado los
    // mete ADENTRO de la libreria (una libreria de verdad trae sus scripts; asi el juego compilado los empaqueta)
    {
        const std::vector<W3dRaizFila>& fs = W3dRaices();
        for (size_t f = 0; f < fs.size(); f++) {
            std::vector<Object*> pila;
            if (fs[f].raiz) pila.push_back(fs[f].raiz);
            while (!pila.empty()) {
                Object* o = pila.back(); pila.pop_back();
                for (size_t i = 0; i < o->Childrens.size(); i++) pila.push_back(o->Childrens[i]);
                if (!o->scriptDatos) continue;
                for (size_t k = 0; k < o->scriptDatos->scripts.size(); k++) {
                    std::string& r = o->scriptDatos->scripts[k].ruta;
                    if (!r.empty() && r[0] != '/' && r.find(':') == std::string::npos && w3dFileSystem::FileExists(dir + "/" + r))
                        r = dir + "/" + r;
                }
            }
        }
    }
    const std::string ruta = dir + "/" + archivo;
    if (!GuardarW3D(ruta)) { err = "libcrear: no pude guardar '" + ruta + "'"; return false; }
    printf("      [libcrear] libreria '%s': prefabs %s (Piel con textura) y Arbol, escena Decorado\n", ruta.c_str(),
           personaje.c_str());
    return true;
}

bool CmdLibProxy(std::istringstream& ss, std::string& err) {
    std::string lib, tipo, elem, k; float x = 0, y = 0, z = 0; int veces = 1; bool falla = false;
    ss >> lib >> tipo >> elem >> x >> y >> z;
    while (ss >> k) { if (k == "veces") ss >> veces; else if (k == "falla") falla = true; }
    InteractionMode = ObjectMode; estado = editNavegacion;
    for (int i = 0; i < veces; i++) {
        std::string motivo;
        ProxyW3d* px = W3dProxyAgregar(lib, TipoDe(tipo), elem, Vector3(x + 2.0f * (float)i, y, z), &motivo);
        if (falla) {
            if (px) { err = "libproxy: se creo y no debia"; return false; }
            printf("      [libproxy] no se crea (bien): %s\n", motivo.c_str());
            return true;
        }
        if (!px) { err = "libproxy: " + motivo; return false; }
        if (i + 1 == veces) printf("      [libproxy] %d proxy(s) de '%s/%s' (el ultimo: '%s', %d objeto(s) generados)\n",
                                   veces, lib.c_str(), elem.c_str(), px->name.c_str(), Contar(px));
    }
    return true;
}

bool CmdLibProxyInfo(std::istringstream& ss, std::string& err) {
    std::string n, k; ss >> n;
    ProxyW3d* px = Px(n, err, "libproxyinfo"); if (!px) return false;
    const int gen = Contar(px);
    printf("      [libproxyinfo] '%s': libreria '%s' elemento '%s' (%s) clave '%s' generados %d%s\n", n.c_str(),
           px->Libreria().c_str(), px->Elemento().c_str(), px->TipoElemento() == W3D_LIB_ESCENA ? "escena" : "prefab",
           px->prefab.c_str(), gen, px->noGenerada ? " VACIO" : "");
    while (ss >> k) {
        std::string v; ss >> v;
        if (k == "libreria" && px->Libreria() != v) { err = "libproxyinfo: la libreria es '" + px->Libreria() + "'"; return false; }
        if (k == "elemento" && px->Elemento() != v) { err = "libproxyinfo: el elemento es '" + px->Elemento() + "'"; return false; }
        if (k == "tipo" && TipoDe(v) != px->TipoElemento()) { err = "libproxyinfo: otro tipo de elemento"; return false; }
        if (k == "generados" && gen != atoi(v.c_str())) { err = "libproxyinfo: genero " + Ent(gen); return false; }
        if (k == "vacio" && (px->noGenerada ? 1 : 0) != atoi(v.c_str())) { err = "libproxyinfo: vacio = " + Ent(px->noGenerada ? 1 : 0); return false; }
    }
    return true;
}

// TODOS los proxies de 'clave' en el arbol 'o', tambien los de ADENTRO de lo que genera otro (una libreria de adentro)
void JuntarProxies(Object* o, const std::string& clave, std::vector<InstanciaPrefab*>& out) {
    if (!o) return;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        if (!h) continue;
        if (W3dEsTipoInstancia(h->getType()) && ((InstanciaPrefab*)h)->prefab == clave) out.push_back((InstanciaPrefab*)h);
        JuntarProxies(h, clave, out);
    }
}
bool CmdLibCompartido(std::istringstream& ss, std::string& err) {
    std::string lib, tipo, elem, k; ss >> lib >> tipo >> elem;
    const std::string clave = W3dLibsClave(lib, TipoDe(tipo), elem);
    std::vector<InstanciaPrefab*> v;
    {
        std::vector<Object*> raices;
        W3dRaicesEnOrden(raices);
        for (size_t r = 0; r < raices.size(); r++) JuntarProxies(raices[r], clave, v);
    }
    CargarTodasTexturasPendientes();
    std::set<const void*> mallas, sets, mats, texs;
    std::set<std::string> luas;
    int hitboxes = 0;
    const std::string pre = "lib:" + lib + "/";
    for (size_t i = 0; i < v.size(); i++) {
        std::vector<Object*> ms, as, hs;
        JuntarTipo(v[i], ObjectType::mesh, ms);
        JuntarTipo(v[i], ObjectType::armature, as);
        JuntarTipo(v[i], ObjectType::hitbox, hs);
        hitboxes += (int)hs.size();
        // los SCRIPTS de lo generado: el .lua es una entrada de la libreria (uno solo para todos los proxies; el
        // estado de cada uno es suyo)
        {
            std::vector<Object*> pila(1, (Object*)v[i]);
            while (!pila.empty()) {
                Object* o = pila.back(); pila.pop_back();
                for (size_t h = 0; h < o->Childrens.size(); h++) pila.push_back(o->Childrens[h]);
                if (!o->scriptDatos) continue;
                for (size_t s = 0; s < o->scriptDatos->scripts.size(); s++) {
                    const std::string& r = o->scriptDatos->scripts[s].ruta;
                    if (r.compare(0, pre.size(), pre) != 0) { err = "libcompartido: el script '" + r + "' no es de la libreria"; return false; }
                    luas.insert(r);
                }
            }
        }
        for (size_t q = 0; q < ms.size(); q++) {
            Mesh* m = (Mesh*)ms[q];
            if (!m->malla || m->malla->libreria != lib) { err = "libcompartido: '" + m->name + "' no usa una malla de la libreria"; return false; }
            if (m->vertex != m->malla->vertex) { err = "libcompartido: la geometria de '" + m->name + "' no es la de su recurso"; return false; }
            mallas.insert(m->malla);
            for (size_t g = 0; g < m->materialsGroup.size(); g++) {
                Material* mt = m->materialsGroup[g].material;
                if (!mt || mt == MaterialDefecto) continue;
                if (mt->libreria != lib || mt->name.compare(0, lib.size() + 1, lib + "/") != 0) {
                    err = "libcompartido: el material '" + mt->name + "' no es de la libreria"; return false;
                }
                mats.insert(mt);
                if (mt->texture) {
                    if (mt->texture->path.compare(0, pre.size(), pre) != 0) { err = "libcompartido: la textura '" + mt->texture->path + "' no es de la libreria"; return false; }
                    texs.insert(mt->texture);
                }
            }
        }
        for (size_t q = 0; q < as.size(); q++) {
            Armature* a = (Armature*)as[q];
            if (a->animations.empty()) continue;
            W3dRecurso* r = W3dArmatureAnimSetRecurso(a);
            if (!r || r->id.compare(0, pre.size(), pre) != 0) { err = "libcompartido: el esqueleto de '" + v[i]->name + "' no usa un animset de la libreria"; return false; }
            sets.insert(r);
        }
    }
    printf("      [libcompartido] %s: %d proxy(s), %d malla(s), %d animset(s), %d material(es), %d textura(s), %d script(s), "
           "%d hitbox(es)\n", clave.c_str(), (int)v.size(), (int)mallas.size(), (int)sets.size(), (int)mats.size(),
           (int)texs.size(), (int)luas.size(), hitboxes);
    while (ss >> k) {
        int val = -1; ss >> val;
        const int hay = (k == "proxies") ? (int)v.size() : (k == "mallas") ? (int)mallas.size() : (k == "animsets") ? (int)sets.size()
                      : (k == "materiales") ? (int)mats.size() : (k == "texturas") ? (int)texs.size()
                      : (k == "scripts") ? (int)luas.size() : (k == "hitboxes") ? hitboxes : -999;
        if (hay == -999) { err = "libcompartido: no se que es '" + k + "'"; return false; }
        if (hay != val) { err = "libcompartido: " + k + " = " + Ent(hay) + ", se esperaba " + Ent(val); return false; }
    }
    return true;
}

bool CmdLibInfo(std::istringstream& ss, std::string& err) {
    std::string lib, k; ss >> lib;
    const bool vinc = W3dLibsBuscar(lib) >= 0;
    const bool mont = W3dAlmacenPorNombre(lib) != NULL;
    std::vector<std::string> ps, es;
    if (vinc) { W3dLibsElementos(lib, W3D_LIB_PREFAB, ps); W3dLibsElementos(lib, W3D_LIB_ESCENA, es); }
    printf("      [libinfo] '%s': vinculada %d montada %d (%d vinculadas) prefabs %d escenas %d | archivo %s\n", lib.c_str(),
           vinc ? 1 : 0, (W3dAlmacenPorNombre(lib) != NULL) ? 1 : 0, W3dLibsCantidad(), (int)ps.size(), (int)es.size(),
           vinc ? W3dLibsFila(W3dLibsBuscar(lib)).rutaJson.c_str() : "-");
    while (ss >> k) {
        int val = -1; ss >> val;
        if (k == "vinculada" && (vinc ? 1 : 0) != val) { err = "libinfo: vinculada = " + Ent(vinc ? 1 : 0); return false; }
        if (k == "montada" && (mont ? 1 : 0) != val) { err = "libinfo: montada = " + Ent(mont ? 1 : 0); return false; }
        if (k == "prefabs" && (int)ps.size() != val) { err = "libinfo: prefabs = " + Ent((long)ps.size()); return false; }
        if (k == "escenas" && (int)es.size() != val) { err = "libinfo: escenas = " + Ent((long)es.size()); return false; }
        if (k == "cantidad" && W3dLibsCantidad() != val) { err = "libinfo: vinculadas = " + Ent(W3dLibsCantidad()); return false; }
    }
    return true;
}

bool CmdLibRecurso(std::istringstream& ss, std::string& err) {
    std::string tipo, n, k; ss >> tipo >> n;
    bool existe = false; std::string lib;
    int vista = -1;
    if (tipo == "malla") {
        MallaRecurso* r = W3dMallaRecursoPorNombre(n);
        existe = r != NULL; if (r) lib = r->libreria;
        vista = W3D_VISTA_MALLAS;
    } else if (tipo == "material") {
        Material* m = BuscarMaterialPorNombre(n);
        existe = m != NULL; if (m) lib = m->libreria;
        vista = W3D_VISTA_MATERIALES;
    } else if (tipo == "animset") {
        existe = !W3dAnimSetsEntradaDe(n).empty();
        if (W3dAnimSetEsDeLibreria(n)) { const size_t b = n.find('/'); lib = n.substr(0, b); }
        vista = W3D_VISTA_ANIMACIONES;
    } else if (tipo == "textura") {
        Texture* t = TexturaBuscar(n);
        existe = t != NULL;
        std::string l2, e2; if (W3dRutaDeLibreria(n, &l2, &e2)) lib = l2;
        vista = W3D_VISTA_TEXTURAS;
    } else { err = "librecurso: malla|material|animset|textura"; return false; }
    bool enBib = false;
    { std::vector<W3dRecursoItem> its; W3dBibliotecaListar(-1, its);
      for (size_t i = 0; i < its.size(); i++) if (its[i].tipo == vista && (its[i].id == n || its[i].nombre == n)) enBib = true; }
    printf("      [librecurso] %s '%s': existe %d libreria '%s' en la biblioteca del proyecto %d\n", tipo.c_str(), n.c_str(),
           existe ? 1 : 0, lib.c_str(), enBib ? 1 : 0);
    while (ss >> k) {
        std::string v; ss >> v;
        if (k == "color") {
            // (el color DIFUSO de un material: R G B, tolerancia 1e-3)
            Material* m = (tipo == "material") ? BuscarMaterialPorNombre(n) : NULL;
            float c[3] = { (float)atof(v.c_str()), 0.0f, 0.0f };
            ss >> c[1] >> c[2];
            if (!m) { err = "librecurso: 'color' es de un material que existe"; return false; }
            for (int q = 0; q < 3; q++)
                if (fabsf(m->diffuse[q] - c[q]) > 1e-3f) {
                    char b[160];
                    snprintf(b, sizeof(b), "librecurso: el color de '%s' es (%.3f, %.3f, %.3f)", n.c_str(), m->diffuse[0],
                             m->diffuse[1], m->diffuse[2]);
                    err = b; return false;
                }
            continue;
        }
        if (k == "existe" && (existe ? 1 : 0) != atoi(v.c_str())) { err = "librecurso: existe = " + Ent(existe ? 1 : 0); return false; }
        if (k == "libreria" && lib != (v == "-" ? std::string() : v)) { err = "librecurso: es de la libreria '" + lib + "'"; return false; }
        if (k == "biblioteca" && (enBib ? 1 : 0) != atoi(v.c_str())) { err = "librecurso: en la biblioteca del proyecto = " + Ent(enBib ? 1 : 0); return false; }
    }
    return true;
}

bool CmdLibCambiar(std::istringstream& ss, std::string& err) {
    std::string n, lib, tipo, elem, k; ss >> n >> lib >> tipo >> elem;
    const bool falla = (ss >> k) && k == "falla";
    ProxyW3d* px = Px(n, err, "libcambiar"); if (!px) return false;
    std::string motivo;
    const bool ok = W3dProxyCambiar(px, lib, TipoDe(tipo), elem, &motivo);
    printf("      [libcambiar] '%s' -> '%s/%s': %s %s\n", n.c_str(), lib.c_str(), elem.c_str(), ok ? "cambio" : "NO", motivo.c_str());
    if (ok == falla) { err = "libcambiar: " + std::string(ok ? "cambio y no debia" : "no cambio: " + motivo); return false; }
    return true;
}

bool CmdLibEditar(std::istringstream& ss, std::string& err) {
    std::string n, ruta; ss >> n >> ruta;
    ProxyW3d* px = Px(n, err, "libeditar"); if (!px) return false;
    Object* r = px->RaizGenerada();
    Object* o = r ? W3dJerNodo(r, ruta) : NULL;
    if (!o) { err = "libeditar: el proxy no genero '" + ruta + "'"; return false; }
    InteractionMode = ObjectMode; estado = editNavegacion;
    DeseleccionarTodo();
    o->Seleccionar();
    LayoutToggleEditMode();
    const bool tab = (InteractionMode == EditMode);
    InteractionMode = ObjectMode;
    LayoutModoElegir(EditMode);
    const bool menu = (InteractionMode == EditMode);
    InteractionMode = ObjectMode;
    ActualizarEditMeshActivo();
    printf("      [libeditar] '%s' (%s): Tab %s, Mode > Edit %s\n", ruta.c_str(), o->name.c_str(),
           tab ? "ENTRO" : "no entra", menu ? "ENTRO" : "no entra");
    if (tab || menu) { err = "libeditar: lo generado por un proxy entro en Edit Mode"; return false; }
    return true;
}

bool CmdLibRuta(std::istringstream& ss, std::string& err) {
    std::string r, k; ss >> r;
    std::vector<unsigned char> d;
    const bool lee = w3dFileSystem::ReadFileBytes(r, d);
    const bool existe = w3dFileSystem::FileExists(r);
    printf("      [libruta] '%s': lee %d (%d bytes) existe %d\n", r.c_str(), lee ? 1 : 0, (int)d.size(), existe ? 1 : 0);
    while (ss >> k) {
        int v = -1; ss >> v;
        if (k == "lee" && (lee ? 1 : 0) != v) { err = "libruta: lee = " + Ent(lee ? 1 : 0); return false; }
        if (k == "existe" && (existe ? 1 : 0) != v) { err = "libruta: existe = " + Ent(existe ? 1 : 0); return false; }
        if (k == "bytes" && (int)d.size() != v) { err = "libruta: " + Ent((long)d.size()) + " bytes"; return false; }
    }
    return true;
}

// la primera malla generada por un proxy y lo que usa
struct UsoMalla { Mesh* m; Material* mat; Texture* tex; W3dRecurso* set; };
bool UsoDe(ProxyW3d* px, UsoMalla& u, std::string& err) {
    std::vector<Object*> ms, as;
    JuntarTipo(px, ObjectType::mesh, ms);
    JuntarTipo(px, ObjectType::armature, as);
    if (ms.empty() || as.empty()) { err = "libdistintos: '" + px->name + "' no genero malla y esqueleto"; return false; }
    u.m = (Mesh*)ms[0];
    u.mat = u.m->materialsGroup.empty() ? NULL : u.m->materialsGroup[0].material;
    u.tex = u.mat ? u.mat->texture : NULL;
    u.set = W3dArmatureAnimSetRecurso((Armature*)as[0]);
    return true;
}
bool CmdLibDistintos(std::istringstream& ss, std::string& err) {
    std::string a, b; ss >> a >> b;
    ProxyW3d* pa = Px(a, err, "libdistintos"); if (!pa) return false;
    ProxyW3d* pb = Px(b, err, "libdistintos"); if (!pb) return false;
    CargarTodasTexturasPendientes();
    UsoMalla ua, ub;
    if (!UsoDe(pa, ua, err) || !UsoDe(pb, ub, err)) return false;
    std::vector<unsigned char> ba, bb;
    if (ua.tex) w3dFileSystem::ReadFileBytes(ua.tex->path, ba);
    if (ub.tex) w3dFileSystem::ReadFileBytes(ub.tex->path, bb);
    printf("      [libdistintos] '%s' malla %s mat %s tex %s (%d B) | '%s' malla %s mat %s tex %s (%d B)\n",
           a.c_str(), ua.m->malla ? ua.m->malla->nombre.c_str() : "-", ua.mat ? ua.mat->name.c_str() : "-",
           ua.tex ? ua.tex->path.c_str() : "-", (int)ba.size(),
           b.c_str(), ub.m->malla ? ub.m->malla->nombre.c_str() : "-", ub.mat ? ub.mat->name.c_str() : "-",
           ub.tex ? ub.tex->path.c_str() : "-", (int)bb.size());
    if (!ua.m->malla || ua.m->malla == ub.m->malla || ua.m->vertex == ub.m->vertex) { err = "libdistintos: comparten la malla"; return false; }
    if (!ua.mat || ua.mat == ub.mat) { err = "libdistintos: comparten el material"; return false; }
    if (!ua.tex || ua.tex == ub.tex || ba == bb) { err = "libdistintos: comparten la textura (o sus bytes)"; return false; }
    if (!ua.set || ua.set == ub.set) { err = "libdistintos: comparten el animset"; return false; }
    return true;
}

ViewportBase* HojaProps(ViewportBase* n) {
    if (!n) return NULL;
    if (n->isLeaf()) return n->ViewportKind() == 3 ? n : NULL;
    ViewportBase* a = NULL; ViewportBase* b = NULL;
    if (n->ContainerKind() == 1) { a = ((ViewportRow*)n)->childA; b = ((ViewportRow*)n)->childB; }
    else { a = ((ViewportColumn*)n)->childA; b = ((ViewportColumn*)n)->childB; }
    ViewportBase* r = HojaProps(a);
    return r ? r : HojaProps(b);
}
bool CmdLibPropsMat(std::istringstream& ss, std::string& err) {
    Properties* p = (Properties*)HojaProps(LayoutRaizCompleta());
    if (!p) { err = "libpropsmat: no hay panel de propiedades"; return false; }
    p->RefreshTargetProperties();
    p->ActualizarPestanias();
    PropsActivo = p;
    if (p->pestaniaActiva != 2 && p->BarTabs.size() > 2 && p->BarTabs[2]->visible) { p->pestaniaActiva = 2; p->ActualizarPestanias(); }
    rootViewport->Render();
    if (ObjActivo && ObjActivo->getType() == ObjectType::mesh) p->Rebind();
    const bool lectura = p->propMsgDefault && !p->propMsgDefault->oculto && p->propMsgDefault->name.find("library") != std::string::npos &&
                         p->propBtnNewMaterial && p->propBtnNewMaterial->oculto;
    std::string tarjeta = "-";
    if (p->propPrefab && p->propPrefab->visible)
        tarjeta = (p->propPxLib && !p->propPxLib->oculto) ? "proxy" : "prefab";
    printf("      [libpropsmat] '%s' (pestania %d): material %s | tarjeta %s%s%s\n", ObjActivo ? ObjActivo->name.c_str() : "-",
           p->pestaniaActiva, lectura ? "de SOLO LECTURA" : "editable", tarjeta.c_str(),
           tarjeta == "proxy" ? (" libreria '" + p->propPxLib->button->text + "'").c_str() : "",
           tarjeta != "-" ? (" elemento '" + p->propPfSel->button->text + "'").c_str() : "");
    std::string k, v;
    while (ss >> k >> v) {
        if (k == "lectura" && (lectura ? 1 : 0) != atoi(v.c_str())) { err = "libpropsmat: lectura = " + Ent(lectura ? 1 : 0); return false; }
        if (k == "tarjeta" && tarjeta != v) { err = "libpropsmat: la tarjeta es '" + tarjeta + "'"; return false; }
    }
    return true;
}

void Script(Object* o, const char* ruta) {
    if (!o->scriptDatos) o->scriptDatos = new W3dScriptDatos();
    W3dScriptEntrada e; e.ruta = ruta;
    o->scriptDatos->scripts.push_back(e);
}

bool CmdLibJuegoMin(std::istringstream& ss, std::string& err) {
    std::string dir, libW3d, k, elem = "Enemigo"; ss >> dir >> libW3d;
    int nProx = 3;
    while (ss >> k) { if (k == "prefab") ss >> elem; else if (k == "proxies") ss >> nProx; }
    if (nProx < 0) nProx = 0;
    if (dir.empty() || libW3d.empty() || elem.empty()) { err = "libjuegomin: uso: libjuegomin <carpeta> <lib.w3d> [prefab <nombre>]"; return false; }
    std::string e2;
    if (!W3dRunCommand("juego3dmin " + dir, e2)) { err = "libjuegomin: " + e2; return false; }
    if (!Escribir(dir + "/libdirector.lua", kLuaDirector)) { err = "libjuegomin: no pude escribir el director"; return false; }
    std::string motivo;
    if (!W3dLibreriaVincular(Absoluta(libW3d), &motivo)) { err = "libjuegomin: no se vinculo: " + motivo; return false; }
    const std::string lib = W3dLibsFila(W3dLibsCantidad() - 1).nombre;
    InteractionMode = ObjectMode; estado = editNavegacion;
    for (int i = 0; i < nProx; i++) {
        ProxyW3d* px = W3dProxyAgregar(lib, W3D_LIB_PREFAB, elem, Vector3(-4.0f + 4.0f * (float)i, 0.0f, -6.0f), &motivo);
        if (!px) { err = "libjuegomin: " + motivo; return false; }
        if (i == 1) W3dPrefabOverrideProp(px, "vida", "25");   // (el override de una propiedad, como en una instancia)
    }
    Empty* d = new Empty(NULL, Vector3(0, -10, 0));
    d->SetNameObj("Director");
    Script(d, "libdirector.lua");
    d->scriptDatos->scripts.back().refs.push_back(std::make_pair(std::string("lib"), lib));
    d->scriptDatos->scripts.back().refs.push_back(std::make_pair(std::string("elem"), elem));
    // lo que el director espera: los proxies de arranque (cada uno vida 10; el del medio 25) + 5 instanciados por lua
    const int vida = 10 * nProx + (nProx >= 2 ? 15 : 0) + 50;
    d->scriptDatos->scripts.back().refs.push_back(std::make_pair(std::string("base"), Ent(nProx)));
    d->scriptDatos->scripts.back().refs.push_back(std::make_pair(std::string("vida"), Ent(vida)));
    DeseleccionarTodo(); ObjActivo = NULL;
    printf("      [libjuegomin] juego3dmin + libreria '%s' + %d proxies de su %s (el del medio con vida=25) + Director en '%s'\n",
           lib.c_str(), nProx, elem.c_str(), dir.c_str());
    return true;
}

bool CmdLibJuegoLog(std::istringstream& ss, std::string& err) {
    std::string dir, nombre, k; ss >> dir >> nombre;
    const std::string carpeta = dir + "/build/linux";
    const std::string bin = carpeta + "/" + nombre;
    if (!w3dFileSystem::FileExists(bin)) { err = "libjuegolog: no existe el binario compilado '" + bin + "'"; return false; }
    const std::string log = carpeta + "/whisk3d.log";
    remove(log.c_str());
    char cmdRun[2200];
    snprintf(cmdRun, sizeof(cmdRun), "cd \"%s\" && timeout 120 ./%s > /dev/null 2>&1", carpeta.c_str(), nombre.c_str());
    const int r = system(cmdRun);
    FILE* f = fopen(log.c_str(), "rb");
    if (!f) { err = "libjuegolog: el juego no dejo whisk3d.log (se compilo en modo debug?)"; return false; }
    std::string veredicto;
    char buf[2048];
    while (fgets(buf, sizeof(buf), f)) {
        const char* p = strstr(buf, "[librerias] ");
        if (p && (strstr(p, " OK ") || strstr(p, " FALTA ")) && (strstr(p, "creados=") || strstr(p, "externo="))) veredicto = p;
    }
    fclose(f);
    while (!veredicto.empty() && (veredicto[veredicto.size() - 1] == '\n' || veredicto[veredicto.size() - 1] == '\r'))
        veredicto.erase(veredicto.size() - 1);
    printf("      [libjuegolog] salida=%d | %s\n", r, veredicto.empty() ? "(sin veredicto del director)" : veredicto.c_str());
    if (veredicto.find("[librerias] OK") == std::string::npos) {
        err = "libjuegolog: en el juego compilado los proxies / instanciar() de la libreria no dieron lo mismo que en el Play";
        return false;
    }
    while (ss >> k) if (veredicto.find(k) == std::string::npos) { err = "libjuegolog: el veredicto no dice '" + k + "'"; return false; }
    return true;
}

// ---------------------------------------------------------------------------
//  LA LIBRERIA "extras": lo de adentro de una libreria (afuera de su .w3d, sueltos, UI, librerias de adentro)
// ---------------------------------------------------------------------------
const char* kLuaExterno =
    "-- un script de la libreria que vive AFUERA de su .w3d: lo que nombra por ruta es de SU libreria\n"
    "function inicio()\n"
    "  local a = importarW3D(\"anexos/piedra.w3d\")   -- suelto al lado del .w3d de la libreria\n"
    "  local b = importarW3D(\"extra/roca.w3d\")      -- una entrada de la libreria\n"
    "  setCompartido(\"anexos\", (a and nombre(a) or \"nada\") .. \"+\" .. (b and nombre(b) or \"nada\"))\n"
    "  setCompartido(\"externo\", \"OK\")\n"
    "end\n";
const char* kLuaSonoro =
    "-- sonido() de un script de la libreria: primero lo de SU libreria\n"
    "function inicio()\n"
    "  sonido(\"sonidos/bip.wav\")       -- suelto al lado de la libreria\n"
    "  sonido(\"sonidos/entrada.wav\")   -- una entrada de la libreria\n"
    "  setCompartido(\"sonoro\", \"listo\")\n"
    "end\n";
const char* kLuaLlamador =
    "-- un script de la libreria instancia un prefab de la libreria de ADENTRO, con el nombre de SU registro\n"
    "function inicio()\n"
    "  local e = instanciar(\"personajes/Arbol\", 0, 0, 5)\n"
    "  setCompartido(\"llamado\", e and tipo(e) or \"nada\")\n"
    "  setCompartido(\"tronco\", (e and buscar(\"Tronco\", e)) and \"si\" or \"no\")\n"
    "end\n";
const char* kLuaPanel =
    "-- la imagen de la escena UI de la libreria: su textura es de SU libreria\n"
    "function inicio() setTextura(yo(), \"texturas/logo.png\") end\n";
const char* kLuaDirectorExtras =
    "-- director de la prueba de librerias (2): lo de adentro de una libreria, en el juego\n"
    "local espera = 0\n"
    "function actualizar(dt)\n"
    "  espera = espera + 1\n"
    "  if espera < 5 then return end\n"
    "  if espera > 5 then return end\n"
    "  local ok = compartido(\"externo\") == \"OK\" and compartido(\"anexos\") == \"Piedra+Roca\"\n"
    "    and (compartido(\"enemigos\") or 0) == 1 and compartido(\"sonoro\") == \"listo\"\n"
    "  info(string.format(\"[librerias] %s externo=%s anexos=%s enemigos=%d sonoro=%s\", ok and \"OK\" or \"FALTA\",\n"
    "    tostring(compartido(\"externo\")), tostring(compartido(\"anexos\")), compartido(\"enemigos\") or -1,\n"
    "    tostring(compartido(\"sonoro\"))))\n"
    "  setCompartido(\"juez\", ok and \"OK\" or \"FALTA\")\n"
    "  salir()\n"
    "end\n";
// un .w3d v3 plano (lo unico que importarW3D anexa) con un objeto
std::string W3dPlano(const char* nombre) {
    return std::string("{\n  \"escena\": { \"objetos\": [ { \"tipo\": \"objeto\", \"nombre\": \"") + nombre + "\" } ] }\n}\n";
}
bool Carpeta(const std::string& d) {
    const std::string c = "mkdir -p \"" + d + "\"";
    return system(c.c_str()) == 0;
}

bool CmdLibCrearExtras(std::istringstream& ss, std::string& err) {
    std::string dir0, archivo, png, otra; ss >> dir0 >> archivo >> png >> otra;
    if (dir0.empty() || archivo.empty() || png.empty() || otra.empty()) {
        err = "libcrearextras: uso: libcrearextras <carpeta> <archivo.w3d> <textura.png> <otra.w3d>"; return false;
    }
    const std::string dir = Absoluta(dir0);
    std::string e2, motivo;
    if (!W3dRunCommand("escenadefecto", e2)) { err = "libcrearextras: " + e2; return false; }
    w3dPath = "";
    if (!Carpeta(dir + "/fuera") || !Carpeta(dir + "/sonidos") || !Carpeta(dir + "/anexos")) { err = "libcrearextras: no pude crear las carpetas"; return false; }
    // los archivos: lo que queda AFUERA de su .w3d (fuera/), los SUELTOS que nombran sus scripts (sonidos/, anexos/)
    // y lo que entra (logo.png por la UI; entrada.wav y roca.w3d importados; los .lua de la carpeta)
    const std::string bip = "sonidos/bip.wav";
    std::string w1 = W3dPlano("Piedra"), w2 = W3dPlano("Roca");
    if (!Copiar(png, dir + "/logo.png") || !Copiar(png, dir + "/fuera/cartel.png") ||
        !Copiar(bip, dir + "/sonidos/bip.wav") || !Copiar(bip, dir + "/entrada.wav") ||
        !Escribir(dir + "/anexos/piedra.w3d", w1.c_str()) || !Escribir(dir + "/roca.w3d", w2.c_str()) ||
        !Escribir(dir + "/fuera/externo.lua", kLuaExterno) || !Escribir(dir + "/sonoro.lua", kLuaSonoro) ||
        !Escribir(dir + "/panel.lua", kLuaPanel) || !Escribir(dir + "/llamador.lua", kLuaLlamador)) {
        err = "libcrearextras: no pude escribir los archivos"; return false;
    }
    // la libreria de ADENTRO (vinculada en ESTA libreria)
    if (!W3dLibreriaVincular(Absoluta(otra), &motivo)) { err = "libcrearextras: no se vinculo la de adentro: " + motivo; return false; }
    const std::string sub = W3dLibsFila(W3dLibsCantidad() - 1).nombre;
    // prefab "Cartel": la textura y el script AFUERA del .w3d
    const int ic = W3dRaizCrearYAbrir(W3D_RAIZ_PREFAB, "Cartel");
    if (ic < 0 || SceneCollection->Childrens.empty()) { err = "libcrearextras: no pude crear el prefab Cartel"; return false; }
    Material* pintura = new Material("Pintura");
    pintura->texture = TexturaTomar(dir + "/fuera/cartel.png");
    pintura->textureOn = true;
    if (!pintura->texture) { err = "libcrearextras: no pude cargar la textura de afuera"; return false; }
    W3dRefExternaMarcar(dir + "/fuera/cartel.png");
    if (!Cubo(SceneCollection->Childrens[0], "Tablero", "Tablero", pintura)) { err = "libcrearextras: NewMesh fallo"; return false; }
    Script(SceneCollection->Childrens[0], (dir + "/fuera/externo.lua").c_str());
    W3dRefExternaMarcar(dir + "/fuera/externo.lua");
    // prefab "Escuadra": un proxy de la de ADENTRO + un cubo con una malla suya
    const int ie = W3dRaizCrearYAbrir(W3D_RAIZ_PREFAB, "Escuadra");
    if (ie < 0 || SceneCollection->Childrens.empty()) { err = "libcrearextras: no pude crear el prefab Escuadra"; return false; }
    Object* raizE = SceneCollection->Childrens[0];
    ProxyW3d* anid = W3dProxyCrear(sub, W3D_LIB_PREFAB, "Enemigo", raizE, Vector3(0, 0, 0), 0.0f);
    if (!anid || anid->Childrens.empty()) { err = "libcrearextras: el proxy de la de adentro no genero nada"; return false; }
    Mesh* viga = Cubo(raizE, "Viga", "Viga", NULL);
    MallaRecurso* tronco = W3dMallaRecursoPorNombre(sub + "/Tronco");
    if (!viga || !tronco || !W3dMallaAsignar(viga, tronco)) { err = "libcrearextras: no pude usar la malla '" + sub + "/Tronco'"; return false; }
    viga->pos = Vector3(3, 0, 0);
    // prefab "Sonoro"
    const int is = W3dRaizCrearYAbrir(W3D_RAIZ_PREFAB, "Sonoro");
    if (is < 0 || SceneCollection->Childrens.empty()) { err = "libcrearextras: no pude crear el prefab Sonoro"; return false; }
    Script(SceneCollection->Childrens[0], (dir + "/sonoro.lua").c_str());
    // prefab "Llamador": instancia el Arbol de la de ADENTRO ("personajes/Arbol": el nombre de SU registro)
    const int il = W3dRaizCrearYAbrir(W3D_RAIZ_PREFAB, "Llamador");
    if (il < 0 || SceneCollection->Childrens.empty()) { err = "libcrearextras: no pude crear el prefab Llamador"; return false; }
    Script(SceneCollection->Childrens[0], (dir + "/llamador.lua").c_str());
    // escena "Panel": una escena UI
    const int ip = W3dRaizCrearYAbrir(W3D_RAIZ_ESCENA, "Panel");
    if (ip < 0) { err = "libcrearextras: no pude crear la escena Panel"; return false; }
    UI* u = new UI(NULL);
    W3dRenombrarObjeto(u, "Tablerito", false);
    Imagen2D* fondo = new Imagen2D(u);
    W3dRenombrarObjeto(fondo, "Fondo", false);
    fondo->textura = dir + "/logo.png";
    Imagen2D* icono = new Imagen2D(u);
    W3dRenombrarObjeto(icono, "Icono", false);
    Script(icono, (dir + "/panel.lua").c_str());
    if (!W3dActivarRaiz(W3dRaizBloque(), &motivo)) { err = "libcrearextras: " + motivo; return false; }
    DeseleccionarTodo(); ObjActivo = NULL;
    const std::string ruta = dir + "/" + archivo;
    if (!GuardarW3D(ruta)) { err = "libcrearextras: no pude guardar '" + ruta + "'"; return false; }
    w3dPath = ruta;
    // lo IMPORTADO a la libreria (entradas que ningun objeto nombra: las nombran sus scripts)
    const std::string ew = W3dImportarAsset(dir + "/entrada.wav"), er = W3dImportarAsset(dir + "/roca.w3d");
    if (ew != "sonidos/entrada.wav" || er != "extra/roca.w3d") { err = "libcrearextras: importo '" + ew + "' y '" + er + "'"; return false; }
    if (!GuardarW3D(ruta)) { err = "libcrearextras: no pude re-guardar '" + ruta + "'"; return false; }
    w3dPath = "";   // (como libcrear: la libreria no queda como "el proyecto abierto" de lo que sigue)
    printf("      [libcrearextras] '%s' (vincula '%s'): Cartel (textura y script afuera), Escuadra (proxy de adentro), "
           "Sonoro, escena Panel (UI)\n", ruta.c_str(), sub.c_str());
    return true;
}

bool CmdLibMarca(std::istringstream& ss, std::string& err) {
    std::string n, ruta, ic; ss >> n >> ruta >> ic;
    ProxyW3d* px = Px(n, err, "libmarca"); if (!px) return false;
    Object* o = px;
    if (ruta != "proxy") {
        Object* r = px->RaizGenerada();
        o = r ? (ruta == "." ? r : W3dJerNodo(r, ruta)) : NULL;
        if (!o) { err = "libmarca: el proxy no genero '" + ruta + "'"; return false; }
    }
    const int m = OutlinerMarcaDeObjeto(o);
    const std::string nom = (m >= 0) ? IconoNombre(m) : "-";
    printf("      [libmarca] '%s' (%s): marca '%s'\n", o->name.c_str(), ruta.c_str(), nom.c_str());
    if (nom != ic) { err = "libmarca: la marca es '" + nom + "', se esperaba '" + ic + "'"; return false; }
    return true;
}

bool CmdLibPropsMalla(std::istringstream& ss, std::string& err) {
    int tab = -1; ss >> tab;
    Properties* p = (Properties*)HojaProps(LayoutRaizCompleta());
    if (!p) { err = "libpropsmalla: no hay panel de propiedades"; return false; }
    if (!ObjActivo || ObjActivo->getType() != ObjectType::mesh) { err = "libpropsmalla: el activo no es una malla"; return false; }
    Mesh* m = (Mesh*)ObjActivo;
    PropsActivo = p;
    p->RefreshTargetProperties();
    p->pestaniaActiva = tab;
    p->ActualizarPestanias();
    p->Resize(p->width, p->height);
    rootViewport->Render();
    p->ActualizarPestanias();
    bool lectura = false;
    if (tab == 2) {
        bool filas = false;
        if (p->propRowPartOps) for (size_t b = 0; b < p->propRowPartOps->botones.size(); b++) filas |= p->propRowPartOps->botones[b]->visible;
        if (p->propRowDelRen) for (size_t b = 0; b < p->propRowDelRen->botones.size(); b++) filas |= p->propRowDelRen->botones[b]->visible;
        lectura = p->propBtnNewPart && p->propBtnNewPart->oculto && !filas;
    } else if (tab == 3) {
        lectura = p->propLblMallaLib && !p->propLblMallaLib->oculto && p->propBtnAddVG && p->propBtnAddVG->oculto &&
                  p->propBtnAddUV && p->propBtnAddUV->oculto && p->propBtnAddCol && p->propBtnAddCol->oculto &&
                  !(p->propMeshEdicion && p->propMeshEdicion->visible);
    } else if (tab == 4) {
        lectura = p->propLblModsLib && !p->propLblModsLib->oculto && p->propRowMod && !p->propRowMod->botones.empty() &&
                  !p->propRowMod->botones[0]->visible;
    } else { err = "libpropsmalla: pestania 2, 3 o 4"; return false; }
    printf("      [libpropsmalla] '%s' pestania %d: %s\n", m->name.c_str(), tab, lectura ? "SOLO LECTURA" : "editable");
    std::string k;
    while (ss >> k) {
        if (k == "lectura") {
            int v = -1; ss >> v;
            if ((lectura ? 1 : 0) != v) { err = "libpropsmalla: lectura = " + Ent(lectura ? 1 : 0); return false; }
        } else if (k == "intentar") {
            // las ACCIONES tambien estan cerradas (no solo sus botones): nada cambia
            const size_t partes = m->materialsGroup.size(), grupos = m->vertexGroups.size(), uvs = m->uvMaps.size(),
                         mods = m->modificadores.size();
            if (tab == 2 && p->propBtnNewPart && p->propBtnNewPart->action) p->propBtnNewPart->action();
            if (tab == 3) {
                if (p->propBtnAddVG && p->propBtnAddVG->action) p->propBtnAddVG->action();
                if (p->propBtnAddUV && p->propBtnAddUV->action) p->propBtnAddUV->action();
            }
            if (tab == 4 && p->propRowMod && p->propRowMod->acciones.size() > 1 && p->propRowMod->acciones[1]) p->propRowMod->acciones[1]();
            printf("      [libpropsmalla] intentar: partes %d->%d grupos %d->%d uv %d->%d modificadores %d->%d\n",
                   (int)partes, (int)m->materialsGroup.size(), (int)grupos, (int)m->vertexGroups.size(), (int)uvs,
                   (int)m->uvMaps.size(), (int)mods, (int)m->modificadores.size());
            if (partes != m->materialsGroup.size() || grupos != m->vertexGroups.size() || uvs != m->uvMaps.size() ||
                mods != m->modificadores.size()) { err = "libpropsmalla: una accion edito una malla de solo lectura"; return false; }
        } else { err = "libpropsmalla: no entiendo '" + k + "'"; return false; }
    }
    return true;
}

bool CmdLibSonido(std::istringstream& ss, std::string& err) {
    std::string r, k; ss >> r;
    if (r.compare(0, 4, "lib:") != 0) r = Absoluta(r);
    const bool c = W3dSonidoCacheado(r);
    printf("      [libsonido] '%s': %s\n", r.c_str(), c ? "cacheado" : "no");
    while (ss >> k) {
        int v = -1; ss >> v;
        if (k == "cacheado" && (c ? 1 : 0) != v) { err = "libsonido: cacheado = " + Ent(c ? 1 : 0); return false; }
    }
    return true;
}

bool CmdLibMover(std::istringstream& ss, std::string& err) {
    std::string de, a; ss >> de >> a;
    if (de.empty() || a.empty()) { err = "libmover: uso: libmover <de> <a>"; return false; }
    if (rename(de.c_str(), a.c_str()) != 0) { err = "libmover: no pude mover '" + de + "' a '" + a + "'"; return false; }
    printf("      [libmover] '%s' -> '%s'\n", de.c_str(), a.c_str());
    return true;
}

bool CmdLibTexCargar() {
    CargarTodasTexturasPendientes();
    printf("      [libtexcargar] texturas de la cola diferida cargadas\n");
    return true;
}

bool CmdLibAnidada(std::istringstream& ss, std::string& err) {
    std::string de, n, k, v; ss >> de >> n;
    const std::string g = W3dLibsAnidada(de, n);
    printf("      [libanidada] '%s' de adentro de '%s' -> '%s' (%s)\n", n.c_str(), de.c_str(), g.c_str(),
           W3dLibsRutaDiscoDe(g).c_str());
    while (ss >> k >> v)
        if (k == "como" && g != v) { err = "libanidada: vive como '" + g + "'"; return false; }
    return true;
}

bool CmdLibImagen(std::istringstream& ss, std::string& err) {
    std::string n, k, v; ss >> n;
    Object* o = SceneCollection ? FindObjectByName(SceneCollection, n) : NULL;
    if (!o || o->getType() != ObjectType::imagen2d) { err = "libimagen: no hay una imagen 2D '" + n + "'"; return false; }
    Imagen2D* im = (Imagen2D*)o;
    const std::string sc = (im->scriptDatos && !im->scriptDatos->scripts.empty()) ? im->scriptDatos->scripts[0].ruta : std::string();
    printf("      [libimagen] '%s': textura '%s' script '%s'\n", n.c_str(), im->textura.c_str(), sc.c_str());
    while (ss >> k >> v) {
        if (v == "-") v.clear();
        if (k == "textura" && im->textura != v) { err = "libimagen: la textura es '" + im->textura + "'"; return false; }
        if (k == "script" && sc != v) { err = "libimagen: el script es '" + sc + "'"; return false; }
    }
    return true;
}

bool CmdLibJuegoExtras(std::istringstream& ss, std::string& err) {
    std::string dir, extras; ss >> dir >> extras;
    if (dir.empty() || extras.empty()) { err = "libjuegoextras: uso: libjuegoextras <carpeta> <extras.w3d>"; return false; }
    std::string e2, motivo;
    if (!W3dRunCommand("juego3dmin " + dir, e2)) { err = "libjuegoextras: " + e2; return false; }
    if (!Escribir(dir + "/libdirector2.lua", kLuaDirectorExtras)) { err = "libjuegoextras: no pude escribir el director"; return false; }
    if (!W3dLibreriaVincular(Absoluta(extras), &motivo)) { err = "libjuegoextras: no se vinculo: " + motivo; return false; }
    const std::string lib = W3dLibsFila(W3dLibsCantidad() - 1).nombre;
    InteractionMode = ObjectMode; estado = editNavegacion;
    const char* elems[3] = { "Cartel", "Escuadra", "Sonoro" };
    for (int i = 0; i < 3; i++)
        if (!W3dProxyAgregar(lib, W3D_LIB_PREFAB, elems[i], Vector3(-4.0f + 4.0f * (float)i, 0.0f, -6.0f), &motivo)) {
            err = "libjuegoextras: " + motivo; return false;
        }
    Empty* d = new Empty(NULL, Vector3(0, -10, 0));
    d->SetNameObj("Director");
    Script(d, "libdirector2.lua");
    DeseleccionarTodo(); ObjActivo = NULL;
    printf("      [libjuegoextras] juego3dmin + libreria '%s' (Cartel, Escuadra, Sonoro) + Director en '%s'\n", lib.c_str(), dir.c_str());
    return true;
}

} // namespace

bool W3dPruebasLibreriasCmd(const std::string& cmd, std::istringstream& ss, std::string& err, bool& manejado) {
    manejado = true;
    if (cmd == "libcrear")       return CmdLibCrear(ss, err);
    if (cmd == "libproxy")       return CmdLibProxy(ss, err);
    if (cmd == "libproxyinfo")   return CmdLibProxyInfo(ss, err);
    if (cmd == "libcompartido")  return CmdLibCompartido(ss, err);
    if (cmd == "libinfo")        return CmdLibInfo(ss, err);
    if (cmd == "librecurso")     return CmdLibRecurso(ss, err);
    if (cmd == "libcambiar")     return CmdLibCambiar(ss, err);
    if (cmd == "libeditar")      return CmdLibEditar(ss, err);
    if (cmd == "libruta")        return CmdLibRuta(ss, err);
    if (cmd == "libdistintos")   return CmdLibDistintos(ss, err);
    if (cmd == "libpropsmat")    return CmdLibPropsMat(ss, err);
    if (cmd == "libjuegomin")    return CmdLibJuegoMin(ss, err);
    if (cmd == "libjuegolog")    return CmdLibJuegoLog(ss, err);
    if (cmd == "libcrearextras") return CmdLibCrearExtras(ss, err);
    if (cmd == "libmarca")       return CmdLibMarca(ss, err);
    if (cmd == "libpropsmalla")  return CmdLibPropsMalla(ss, err);
    if (cmd == "libsonido")      return CmdLibSonido(ss, err);
    if (cmd == "libanidada")     return CmdLibAnidada(ss, err);
    if (cmd == "libtexcargar")   return CmdLibTexCargar();
    if (cmd == "libmover")       return CmdLibMover(ss, err);
    if (cmd == "libimagen")      return CmdLibImagen(ss, err);
    if (cmd == "libjuegoextras") return CmdLibJuegoExtras(ss, err);
    manejado = false;
    return false;
}
