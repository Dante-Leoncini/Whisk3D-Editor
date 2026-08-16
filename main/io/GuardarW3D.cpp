// ============================================================================
//  GuardarW3D.cpp — ver GuardarW3D.h.
//
//  FORMATO v4: el .w3d es un CONTENEDOR (zip) con TODO el proyecto adentro. El
//  JSON no cambio: es el mismo esquema, la misma indentacion, los mismos nombres
//  de campo y el mismo orden que el v3, con dos lineas nuevas ("version": 4 y el
//  bloque "contenedor") y las rutas reescritas como NOMBRES DE ENTRADA. Un
//  proyecto.json extraido del zip se edita con cualquier editor de texto, se
//  vuelve a meter y abre: la propiedad que gano el dueno cuando pidio "que los
//  w3d sean json standard" se conserva entera.
//
//  ARMADO EN DOS FASES:
//    1) se camina la escena escribiendo el JSON, y cada ruta de asset pasa por
//       W3dContenedorEscritor::Ingerir, que mete el archivo adentro y devuelve su
//       nombre de entrada. Los .w3dui y los .glb salen a un TEMPORAL en la
//       carpeta del destino (UI2DGuardar solo sabe escribir a un
//       path) y de ahi entran al zip; el temporal se borra siempre.
//    2) se verifica que toda referencia interna tenga entrada, se vuelca el zip
//       al .w3dtmp en orden determinista y se hace UN rename.
//
//  LA GEOMETRIA VA EN .w3dm (formato propio, libs/Whisk3DCore/io/W3dMalla.h) bajo mallas/. El GLB
//  dejo de ser formato de GUARDADO: queda SOLO para importar y para el "Export to..." del usuario.
//  Con eso se fueron tres parches que existian PORQUE el GLB triangula y re-splitea los vertices:
//    - el bloque "topologia" del JSON (los quads/ngons ahora son NATIVOS del .w3dm);
//    - los "vgroups"/"uvgroups" por CLAVE GEOMETRICA (ahora los INDICES son el dato: VG y UVG);
//    - el swap manual de uv2dRest antes de exportar (ahora el rest viaja en el bloque UVREST).
//  Y ademas dejaron de perderse las capas de color, la segunda capa UV, las costuras, los bordes
//  marcados, las normales del usuario y la geometria suelta.
//
//  MATERIALES: como viajaban ADENTRO del GLB, ahora tienen su propio bloque raiz "materiales" del
//  proyecto.json (referenciado POR NOMBRE desde el bloque PARTMAT de cada .w3dm). Las texturas
//  entran al contenedor como cualquier otro asset (texturas/).
//
//  RETROCOMPAT: un .w3d con mallas/*.glb (con o sin bloque "topologia") se sigue ABRIENDO por el
//  camino de siempre y queda convertido a .w3dm AL GUARDAR. Lo que ya se habia perdido no se
//  resucita (no existe en el .glb): la migracion corta la perdida hacia adelante.
// ============================================================================
#include "io/GuardarW3D.h"
#include "io/JsonW3d.h"                // JsonNumTexto: floats con round-trip EXACTO
#include "io/UI2DFormato.h"
#include "io/W3dContenedor.h"          // FORMATO v4: el .w3d ES un zip y todo va adentro
#include "W3dEscena.h"                // escenaInicial / modoEscenas (se guardan con el proyecto)
#include "W3dPaletas.h"               // las paletas del PROYECTO (raiz "paletas" del .w3d)
#include "objects/Objects.h"
#include "objects/ObjectMode.h"        // W3dAplicarCurvasEnFrame: el objeto se guarda EN REPOSO
#include "objects/UI.h"
#include "objects/Camera.h"
#include "objects/Light.h"
#include "objects/Gamepad.h"
#include "objects/Mesh.h"
#include "objects/Mirror.h"            // AUDIT versiones: el objeto Espejo ahora SI se guarda
#include "objects/Instance.h"          // idem Instance (duplicado enlazado / array de copias)
#include "objects/LOD.h"               // objeto LOD (un hijo por distancia): distancias
#include "objects/Culling.h"           // objeto Culling (frustum culling): soloCamaraActiva
#include "objects/Particulas.h"        // objeto Particulas (emisor): textura + config del cono
#include "objects/VisZona.h"           // objeto VisZona (celda de visibilidad): modo + grilla + refs
#include "objects/Collection.h"        // Collection: ordenarPorCamara/ordenarUnaVez (transparentes)
#include "objects/Curve.h"             // idem Curve (riel de camara; se guarda su archivo de origen)
#include "objects/Armature.h"          // los ARMATURES ahora SI se guardan (Fase 3)
#include "animation/SkeletalAnimation.h" // clips del esqueleto (tracks/curvas por hueso)
#include "animation/Armature2DAnimation.h" // clips del armature 2D (tracks/curvas por hueso 2D)
#include "edit/Modifier.h"             // modificador Armature del mesh (referencia por nombre)
#include "animation/VertexAnimation.h"
#include "script/W3dScript.h"
#include "io/W3dMalla.h"               // .w3dm: el formato de geometria propio (reemplaza al GLB)
#include "importers/import_obj.h"     // TexturaPendienteDe: la carga de texturas es DIFERIDA
#include "objects/Materials.h"         // los MATERIALES viajaban dentro del GLB: ahora van al JSON
#include "objects/Textures.h"
#include "render/UIOverlay.h"          // UI2D_EsElemento2D: los hijos 2D de un UI viajan en SU .w3dui
#include "ViewPorts/Notificaciones.h"
#include "W3dAviso.h"                 // W3dAvisof: los avisos del guardado se VEN, no solo se loguean
#include "ViewPorts/ViewPorts.h"      // rootViewport: el arbol VIVO de viewports (layout)
#include "ViewPorts/ViewPort3D.h"     // la VISTA (pivot/orbit/rotacion) viaja con el layout
#include "ViewPorts/Timeline.h"       // el MODO del timeline (dope/curvas) tambien viaja con el layout
#include "ViewPorts/PopUp/FileBrowser.h"
#include "ViewPorts/PopUp/ConfirmarPopup.h"  // "Guardar como": confirmar antes de pisar un .w3d
#include "render/OpcionesRender.h"
#include "animation/Animation.h"   // AnimFPS (se guarda con el proyecto)
#include "variables.h"   // w3dPath (el archivo abierto)
#include "w3dFilesystem.h"
#include "w3dlog.h"
#include <stdio.h>
#include <string>
#include <vector>
#include <algorithm>   // std::sort: los refs de script se escriben en orden deterministico
#include <sstream>     // el nombre del PRIMER frame de una vertex anim (base + 001 + .obj)
#include <iomanip>     // std::setw/std::setfill: el mismo padding que VertexAnimation::LoadFrames
#ifdef _WIN32
    #include <direct.h>
    #define W3D_MKDIR(p) _mkdir(p)
#else
    #include <sys/stat.h>
    #define W3D_MKDIR(p) mkdir(p, 0755)
#endif

// ICONO del juego (tarjeta Juego): ruta al PNG, relativa al .w3d cuando se puede.
// Va al escena.json como "icono" (ruta EXTERNA, no se mete al zip a proposito).
std::string g_proyIcono;

// TEST del harness (ver GuardarW3D.h): fallo de escritura inyectado. Siempre
// false en el editor real.
bool g_w3dFallarEscritura = false;

// ESCENA UI QUE NO CARGO en la apertura actual (import_w3d.cpp). Vacio = ninguna.
// Freno de guardado: ver el chequeo al entrar a GuardarW3D.
extern std::string g_w3dUINoCargo;

// CONFIG de la tarjeta Juego (Compilar juego): viaja con el proyecto como
// objeto raiz "compilar" del .w3d (ver GuardarW3D.h). Arranca en los defaults.
W3dCompilarCfg g_proyCompilar = { 1, 0, 0, 0, true, true, false, 0, 100 };

void W3dCompilarReset() {
    g_proyCompilar.modoVentana = 1;      // Pantalla completa (el default de siempre)
    g_proyCompilar.orientacion = 0;      // Todas
    g_proyCompilar.assetsModo  = 0;      // Sueltos (editables)
    g_proyCompilar.plataforma  = 0;      // Linux .deb
    g_proyCompilar.usarFisica  = true;
    g_proyCompilar.usarSonido  = true;
    g_proyCompilar.modoDebug   = false;  // produccion
    g_proyCompilar.uid         = 0;       // sin UID: un proyecto sin "uid" no hereda el del anterior. El save/load lo persiste.
    g_proyCompilar.volumen     = 100;     // volumen del gameplay al maximo por defecto
}

// int <-> string legible del bloque "compilar" (el JSON se edita a mano: nada
// de indices crudos). Un string desconocido cae al default del campo.
const char* W3dCompilarModoVentanaStr(int m) {
    return (m == 0) ? "ventana" : (m == 2) ? "sinBordes" : "pantallaCompleta";
}
int W3dCompilarModoVentanaInt(const std::string& s) {
    if (s == "ventana")   return 0;
    if (s == "sinBordes") return 2;
    return 1;
}
const char* W3dCompilarOrientacionStr(int o) {
    return (o == 1) ? "vertical" : (o == 2) ? "horizontal" : "todas";
}
int W3dCompilarOrientacionInt(const std::string& s) {
    if (s == "vertical")   return 1;
    if (s == "horizontal") return 2;
    return 0;
}
const char* W3dCompilarAssetsStr(int a) { return (a == 1) ? "empaquetados" : "sueltos"; }
int W3dCompilarAssetsInt(const std::string& s) { return (s == "empaquetados") ? 1 : 0; }
const char* W3dCompilarPlataformaStr(int p) {
    return (p == 5) ? "symbian" : (p == 4) ? "windows" :
           (p == 3) ? "android" : (p == 2) ? "web" : (p == 1) ? "appimage" : "linux-deb";
}
int W3dCompilarPlataformaInt(const std::string& s) {
    if (s == "appimage") return 1;
    if (s == "web")      return 2;
    if (s == "android")  return 3;
    if (s == "windows")  return 4;
    if (s == "symbian")  return 5;
    return 0;
}

static int gNoCubiertos = 0;    // curvas SIN archivo de origen (unico caso que el guardado no cubre)

static std::string Carpeta(const std::string& r) {
    size_t s = r.find_last_of("/\\");
    return (s == std::string::npos) ? std::string(".") : r.substr(0, s);
}
static std::string Base(const std::string& r) {
    size_t s = r.find_last_of("/\\");
    return (s == std::string::npos) ? r : r.substr(s + 1);
}
// crea la ruta de carpetas completa ("a/b/c"): mkdir por segmento (existente = ok).
// Hace falta para la estructura contenido/ (rutas anidadas como contenido/modelos)
static void MkdirRec(const std::string& dir) {
    for (size_t i = 1; i < dir.size(); i++)
        if (dir[i] == '/' || dir[i] == '\\')
            W3D_MKDIR(dir.substr(0, i).c_str());
    W3D_MKDIR(dir.c_str());
}
static std::string BaseSinExt(const std::string& r) {
    std::string b = Base(r);
    size_t p = b.find_last_of('.');
    return (p == std::string::npos) ? b : b.substr(0, p);
}
// ---------------------------------------------------------------------------
//  ESCRITURA ATOMICA (temp + rename)
//
//  Todo lo que genera el guardado se escribe primero a "<destino><kTmpSufijo>",
//  en la MISMA carpeta que el destino final (rename entre carpetas distintas no
//  es atomico y puede ni funcionar entre volumenes), y los renames se hacen
//  TODOS JUNTOS al final, cuando ya se sabe que el .w3d entero se pudo escribir.
//  Si algo falla antes se borran los temporales y en disco no cambio NADA.
// ---------------------------------------------------------------------------
static const char* const kTmpSufijo = ".w3dtmp";

// renombra tmp -> fin. POSIX: atomico, pisa el destino. Windows: rename() no
// pisa, hay que sacar el destino antes (mismo patron que LuaCompilar).
static bool RenombrarSobre(const std::string& tmp, const std::string& fin) {
#ifdef _WIN32
    remove(fin.c_str());
#endif
    if (rename(tmp.c_str(), fin.c_str()) != 0) {
        w3dLogfE("GuardarW3D: no pude renombrar %s -> %s", tmp.c_str(), fin.c_str());
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
//  LA COPIA AUTOCONTENIDA YA NO SE PROGRAMA: SALE DE REGALO.
//
//  Antes, "guardar como" a otra carpeta tenia que ir copiando asset por asset
//  (CtxCopia / W3dCopiarAssetAlProyecto / VarianteRel) y remapeando cada ruta, o
//  la copia quedaba con rutas ABSOLUTAS al proyecto original y editar un asset de
//  la copia editaba el del original. Con el contenedor todo lo referenciado entra
//  al .w3d por construccion, asi que la copia es autocontenida SIEMPRE y no hay
//  nada que copiar ni que remapear. Se fueron esas ~70 lineas y su hook.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
//  armado del JSON (a un string; sin dependencias)
// ---------------------------------------------------------------------------
static void JEsc(std::string& s, const std::string& v) {
    s += '"';
    for (size_t i = 0; i < v.size(); i++) {
        char c = v[i];
        if (c == '"' || c == '\\') { s += '\\'; s += c; }
        else if (c == '\n') s += "\\n";
        else s += c;
    }
    s += '"';
}
// UN FLOAT DEL JSON. Escribe el minimo de digitos que vuelve al MISMO float32
// releido con el parser de JsonW3d.h (ver JsonNumTexto). Antes era un "%g" pelado
// = 6 cifras significativas, o sea que NINGUN float del proyecto.json hacia
// round-trip: 3.14159265 volvia 3.14159. Afectaba a todo (pos/rot/escala de cada
// objeto, colores, valores de keyframe y offsets de handles) y no se veia nunca,
// porque la diferencia aparece recien en el decimal 6.
static void JNum(std::string& s, float v) { s += JsonNumTexto(v); }
// UN ENTERO del JSON. Existe porque muchisimos campos enteros se escribian como
// JNum((float)x): para valores chicos da lo mismo, pero de 1e6 para arriba salia
// notacion cientifica ("1.23457e+06") y ese texto ya no es el entero que era. Un
// numero de frame grande entra justo en ese rango.
static void JInt(std::string& s, int v) {
    char b[24]; snprintf(b, sizeof(b), "%d", v); s += b;
}
static void JSang(std::string& s, int n) { for (int i = 0; i < n; i++) s += "  "; }

// contexto del guardado en curso
struct CtxGuardar {
    std::string dirW3d;   // carpeta del .w3d (de ahi salen los temporales)
    int  vtxN;            // desempate de nombre de los blobs de vertex anim
    bool error;           // algo no se pudo escribir: se aborta TODO el guardado
    W3dContenedorEscritor* esc;   // el contenedor que se esta armando
};

// ---------------------------------------------------------------------------
//  FORMATO v4: EL CONTENEDOR EN CURSO
//
//  Mientras dura un GuardarW3D, 'gEsc' apunta al escritor y TODA ruta de asset
//  pasa por el: se mete el archivo adentro del zip y lo que se escribe en el
//  JSON es su NOMBRE DE ENTRADA ("texturas/pausa.png"). 'gQuien' es solo para
//  el EXTERNOS.txt (de quien es la referencia que quedo afuera).
//
//  El .w3dui llega por el HOOK g_w3dRefEmit (ver UI2DFormato.h): ese .cpp lo
//  compila tambien el runtime de los juegos y no puede linkear con esto.
//
//  SE FUERON, y con ellos su clase entera de bugs: Pendiente /
//  DescartarPendientes / CommitPendientes (varios renames = varios puntos de
//  falla, y el comentario de CommitPendientes ya admitia que "la mitad
//  renombrada y la mitad en .tmp" era el mejor de los males), EscribirExterno,
//  PodarHuerfanos (en un zip que se reconstruye entero no puede haber
//  huerfanos), NombreSeguro aplicado a CARPETAS y los contadores glbN/vtxN como
//  nombre de archivo.
// ---------------------------------------------------------------------------
static W3dContenedorEscritor* gEsc = NULL;
static std::string gQuien;

static std::string W3dRefEmitir(std::string& ruta) {
    if (!gEsc) return ruta;
    return gEsc->Ingerir(ruta, NULL, gQuien);
}

// LA ruta de un asset del usuario -> lo que se escribe en el JSON. El asset se
// METE adentro del .w3d y en el archivo va su NOMBRE DE ENTRADA
// ("texturas/pausa.png"), o "ext:..." si el usuario lo dejo afuera a proposito o
// si no se pudo leer. 'rutaDisco' NO es const: queda con lo que vale EN MEMORIA
// (el nombre de entrada para los internos), que es lo que resuelve ReadFileBytes.
static std::string Asset(CtxGuardar* cx, std::string& rutaDisco) {
    if (rutaDisco.empty()) return rutaDisco;
    return cx->esc->Ingerir(rutaDisco, NULL, gQuien);
}

static void CamposComunes(std::string& s, int ind, Object* o) {
    JSang(s, ind); s += "\"nombre\": "; JEsc(s, o->name); s += ",\n";
    if (!o->visible) { JSang(s, ind); s += "\"visible\": false,\n"; }
    // LINEAS PARENTALES: solo se escribe cuando se APAGO (el default es true), asi los
    // proyectos que no la tocan quedan byte a byte como antes.
    if (!o->showRelantionshipsLines) { JSang(s, ind); s += "\"lineasParentales\": false,\n"; }
    // PALETA elegida por el objeto, POR NOMBRE (ausente = hereda del padre)
    if (!o->paleta.empty()) { JSang(s, ind); s += "\"paleta\": "; JEsc(s, o->paleta); s += ",\n"; }
    JSang(s, ind); s += "\"pos\": ["; JNum(s, o->pos.x); s += ", "; JNum(s, o->pos.y); s += ", "; JNum(s, o->pos.z); s += "],\n";
    JSang(s, ind); s += "\"rot\": ["; JNum(s, o->rotEuler.x); s += ", "; JNum(s, o->rotEuler.y); s += ", "; JNum(s, o->rotEuler.z); s += "],\n";
    JSang(s, ind); s += "\"escala\": ["; JNum(s, o->scale.x); s += ", "; JNum(s, o->scale.y); s += ", "; JNum(s, o->scale.z); s += "]";
}

// un Vector3 -> "[x, y, z]"
static void JVec(std::string& s, float x, float y, float z) {
    s += "["; JNum(s, x); s += ", "; JNum(s, y); s += ", "; JNum(s, z); s += "]";
}
// una Matrix4 (16 floats, column-major) -> "[m0, ..., m15]"
static void JMat(std::string& s, const Matrix4& m) {
    s += "[";
    for (int i = 0; i < 16; i++) { if (i) s += ","; JNum(s, m.m[i]); }
    s += "]";
}

// una CURVA (AnimProperty) a JSON compacto: {p, c, k:[[frame,value,interp,htype,inDF,inDV,outDF,outDV],...]}
//
// LOS CUATRO OFFSETS DE HANDLE SE ESCRIBEN SIEMPRE, tengan o no efecto. Solo
// valen para HFree/HAligned (los otros tres tipos los CALCULA HandleEfectivo
// desde los vecinos, ver Animation.h), asi que uno podria omitirlos cuando no
// aplican. No se hace, y a proposito: el usuario mueve un handle, cambia el tipo
// a Auto para ver como queda y lo vuelve a Free -> si el archivo no los hubiera
// guardado, su curva volveria distinta. Son dato del keyframe, no cache del tipo
// vigente. Ademas el largo fijo de 8 campos hace el bloque legible y diffeable.
static void EscribirCurva(std::string& s, const AnimProperty& ap) {
    s += "{ \"p\": "; JInt(s, ap.Property);
    s += ", \"c\": "; JInt(s, ap.component);
    s += ", \"k\": [";
    for (size_t i = 0; i < ap.keyframes.size(); i++) {
        const keyFrame& k = ap.keyframes[i];
        if (i) s += ",";
        s += "["; JInt(s, k.frame); s += ","; JNum(s, k.value);
        s += ","; JInt(s, k.Interpolation); s += ","; JInt(s, k.handleType);
        s += ","; JNum(s, k.inDF); s += ","; JNum(s, k.inDV);
        s += ","; JNum(s, k.outDF); s += ","; JNum(s, k.outDV); s += "]";
    }
    s += "] }";
}

// ---------------------------------------------------------------------------
//  LA GEOMETRIA DE UNA MALLA -> mallas/<slug>.w3dm (formato propio, ver W3dMalla.h)
//
//  Reemplaza al par "exportar un GLB + parchear el JSON con el bloque topologia". El .w3dm lleva
//  los poligonos NATIVOS (nada de triangular y reconstruir), los puntos de control, las normales
//  por esquina, TODAS las capas UV y de color, las costuras, los bordes marcados, la geometria
//  suelta, los mesh parts, los vertex groups y los UV groups. Todo eso o se perdia o viajaba
//  parchado en el JSON con claves geometricas.
//
//  El nombre sale del nombre del OBJETO, sin contador: dos guardados sin cambios dan el mismo
//  archivo (round-trip byte a byte). Si dos objetos derivan el mismo slug, el segundo lleva "-2".
//  dedup=false a proposito: dos mallas con geometria IDENTICA son dos entradas, porque el nombre
//  de entrada es lo que el JSON referencia y aliasarlas haria que renombrar una pisara a la otra.
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
//  SIDECARS DEL MODELO: <base>.pvs.json (y <base>.grupos.json)
//
//  Un .obj puede traer al lado archivos DERIVADOS que el motor lee pero que
//  ningun editor externo produce: los sectores del PVS y los pesos de los grupos
//  de vertices. Antes ninguno pasaba por el contenedor: al empaquetar un .w3d v4
//  y mandarlo a otra maquina, el .obj entraba como "modelos/x.obj" y el sidecar
//  no entraba a ningun lado -- `sinExt + ".pvs.json"` daba "modelos/x.pvs.json",
//  que no existe como entrada, y el PVS desaparecia SIN AVISO (ni siquiera
//  figuraba en EXTERNOS.txt, a diferencia del resto de las refs externas).
//
//  Ahora se ingieren como cualquier asset y el nodo guarda su nombre REAL de
//  entrada, en vez de derivarlo. (Los grupos de vertices, ademas, ya viajan
//  dentro del .w3dm de la malla; el sidecar es solo su ORIGEN de importacion,
//  pero se ingiere igual para poder re-importar el .obj desde el paquete.)
// ---------------------------------------------------------------------------
static void IngerirSidecars(CtxGuardar* cx, Mesh* m) {
    if (!m) return;
    // 1) las declaraciones EXPLICITAS del modificador (`vis:` / `pvs:`) MANDAN:
    //    se ingiere ESE archivo y el nodo guarda su nombre real de entrada.
    //    Antes este paso no existia y el paso 2 PISABA la declaracion con el
    //    sidecar derivado del origen: un modificador que apuntaba a otro dato
    //    (p.ej. un `.w3dvis` alternativo para la misma malla) perdia su archivo
    //    al empaquetar, EN SILENCIO.
    for (size_t k = 0; k < m->modificadores.size(); k++) {
        Modifier* md = m->modificadores[k];
        if (!md || md->tipo != ModifierType::CullingTri) continue;
        if (!md->pvsArchivo.empty() && w3dFileSystem::FileExists(md->pvsArchivo))
            md->pvsArchivo = cx->esc->Ingerir(md->pvsArchivo, NULL, gQuien);
        if (!md->visArchivo.empty() && w3dFileSystem::FileExists(md->visArchivo))
            md->visArchivo = cx->esc->Ingerir(md->visArchivo, NULL, gQuien);
    }
    // 2) los DERIVADOS del origen (compatibilidad + re-importacion): se ingieren
    //    igual, pero solo RELLENAN el nombre del modificador si estaba vacio.
    if (m->origen.empty()) return;
    std::string base = m->origen;
    size_t punto = base.find_last_of('.');
    size_t barra = base.find_last_of("/\\");
    if (punto != std::string::npos && (barra == std::string::npos || punto > barra))
        base = base.substr(0, punto);
    static const char* kSufijos[] = { ".pvs.json", ".grupos.json", ".w3dvis" };
    for (int i = 0; i < 3; i++) {
        std::string ruta = base + kSufijos[i];
        if (!w3dFileSystem::FileExists(ruta)) continue;
        std::string entrada = cx->esc->Ingerir(ruta, NULL, gQuien);
        if (i == 0 || i == 2) {   // los datos de visibilidad los consume el modificador
            for (size_t k = 0; k < m->modificadores.size(); k++) {
                Modifier* md = m->modificadores[k];
                if (!md || md->tipo != ModifierType::CullingTri) continue;
                if (i == 0) { if (md->pvsArchivo.empty()) md->pvsArchivo = entrada; }
                else        { if (md->visArchivo.empty()) md->visArchivo = entrada; }
            }
        }
    }
}

static std::string EscribirMallaW3dm(CtxGuardar* cx, Mesh* m, const std::string& nombreObj) {
    std::string base = W3dSlugEntrada(nombreObj.empty() ? std::string("malla") : nombreObj);
    std::string nom = "mallas/" + base + ".w3dm";
    for (int k = 2; cx->esc->Tiene(nom); k++) {
        char sf[16]; snprintf(sf, sizeof(sf), "-%d", k);
        nom = "mallas/" + base + sf + ".w3dm";
    }
    // EL REST DEL SKINNING 2D ES DATO, NO CACHE: en memoria se captura LAZY (la primera vez que
    // se posa el rig o al reabrir el proyecto), asi que una malla con armature 2D todavia sin
    // posar no lo tiene. Si no se captura aca, el archivo sale SIN bloque UVREST, al reabrirlo el
    // editor lo captura, y el guardado siguiente sale distinto: el round-trip byte a byte se
    // rompia por un dato que en realidad no cambio.
    if (m->TieneArm2D()) m->Armature2DRestCapturar();

    // ---- EL FRENO DE MANO, ANTES DE TOCAR NADA (spec seccion 17) --------------
    //  El .w3dm decia "requiere <X>" y esta version no entiende X: guardar encima
    //  degradaria el archivo EN SILENCIO, que es exactamente lo que esa directiva
    //  existe para impedir. Antes info.soloLectura solo se logueaba y el guardado
    //  seguia igual, o sea el freno no frenaba nada.
    const std::string falta = W3dMallaBloqueQueFalta(m);
    if (!falta.empty()) {
        w3dLogfE("[W3D] '%s': el .w3dm REQUIERE el bloque '%s' y esta version no lo entiende: NO guardo encima",
                 nombreObj.c_str(), falta.c_str());
        W3dAvisof(true, "No guardo '%s': su malla necesita el bloque '%s' de una version mas nueva de Whisk3D",
                  W3dNombreCorto(nombreObj).c_str(), W3dNombreCorto(falta, 16).c_str());
        cx->error = true;   // no hay medio guardado posible: o sale entero o no sale
        return nom;
    }
    // ---- NO CARGO: LA MALLA DE LA ESCENA NO ES LA DEL ARCHIVO -----------------
    //  El lector RECHAZO el .w3dm entero (no es W3DMESH / lexico de una version mas
    //  nueva / falta V o F) y lo que quedo en la escena es un Mesh VACIO con el
    //  nombre y el transform del proyecto. Guardar encima HORNEA esa nada: el .w3dm
    //  sale con 0 puntos / 0 caras y la geometria del usuario no existe mas. Se
    //  frena IGUAL que con "requiere": el proyecto no se guarda y se dice por que.
    if (m->w3dmAjenos.noCargo) {
        w3dLogfE("[W3D] '%s': su geometria NO se pudo leer (la malla quedo vacia): NO guardo encima",
                 nombreObj.c_str());
        W3dAvisof(true, "No guardo '%s': su geometria no se pudo leer al abrir y guardarla la borraria",
                  W3dNombreCorto(nombreObj).c_str());
        cx->error = true;   // no hay medio guardado posible: o sale entero o no sale
        return nom;
    }
    // ---- ABIERTA CON ERRORES (spec 19f) ---------------------------------------
    //  La malla vino de un .w3dm truncado: se cargo lo que habia y guardar encima
    //  HORNEA esa perdida. La spec pide CONFIRMACION; el guardado de hoy es
    //  sincronico de punta a punta y el popup es asincronico, asi que por ahora se
    //  avisa FUERTE (no se hornea en silencio) y queda pendiente el confirmar.
    if (m->w3dmAjenos.truncado)
        W3dAvisof(true, "'%s' se habia abierto CON ERRORES (archivo cortado): al guardar, lo que faltaba se pierde",
                  W3dNombreCorto(nombreObj).c_str());

    // ---- LA MALLA SE GUARDA EN REPOSO, NO EN LA POSE DEL PLAYHEAD --------------
    //  El playhead es estado de la SESION, no del modelo: guardar con el playhead
    //  fuera del cuadro base tiene que dar el MISMO archivo que guardar sin haberlo
    //  movido. Sin esto pasaban las dos mitades del mismo desastre: la geometria
    //  salia HORNEADA EN LA POSE (el cubo se guardaba deformado y al reabrir esa
    //  deformacion ERA el modelo) y las claves de sharp/seam -que viven en el
    //  espacio de REPOSO- no resolvian contra las posiciones posadas, asi que el
    //  escritor no escribia NI UNA MARCA. Ese es, literal, el reporte del dueno:
    //  marcar, animar, guardar, y perder las marcas. El camino viejo por GLB
    //  evaluaba el frame base antes de exportar; al conectar el .w3dm se perdio.
    //  El guard devuelve la pose EXACTA al salir del scope (ver VertexAnimation.h).
    W3dReposoVertexAnim reposo(m);
    // ...y lo MISMO con la animacion UV "tira de atlas": el cuadro aplicado es estado
    // de la sesion, no del modelo. Se deja uv[] en base ANTES de escribir (el proximo
    // tick reaplica el cuadro y re-sube el VBO solo, ver Mesh::ReposarUVAnimTira).
    m->ReposarUVAnimTira();

    std::string txt;
    std::vector<std::string> avisos;
    if (!W3dMallaEscribir(m, txt, &avisos)) {
        w3dLogfE("GuardarW3D: no pude escribir la geometria de '%s'", nombreObj.c_str());
        for (size_t i = 0; i < avisos.size(); i++)
            w3dLogfE("[W3D] '%s': %s", nombreObj.c_str(), avisos[i].c_str());
        cx->error = true;   // aborta el guardado entero: mejor eso que un .w3d que apunta a la nada
        return nom;
    }
    // SILENCIOSO JAMAS: todo lo que NO entro entero al archivo (un bloque de una version
    // mas nueva que no sobrevivio al cambio de topologia, una capa UV/color que no mide lo
    // que la topologia dice, un borde sharp/seam que quedo sin punto, un peso de vertex
    // group que choca) sale por el log Y POR PANTALLA. Una linea en un log que nadie abre
    // no es "avisar": asi se perdio una capa de color entera sin que el dueno se enterara.
    for (size_t i = 0; i < avisos.size(); i++)
        w3dLogfW("[W3D] '%s': %s", nombreObj.c_str(), avisos[i].c_str());
    if (!avisos.empty())
        W3dAvisof(true, "'%s': %s%s", W3dNombreCorto(nombreObj).c_str(), avisos[0].c_str(),
                  avisos.size() > 1 ? " (y mas: ver whisk3d.log)" : "");
    if (!cx->esc->AgregarBytes(nom, txt, false)) {
        w3dLogfE("GuardarW3D: no pude meter %s adentro del .w3d", nom.c_str());
        cx->error = true;
    }
    return nom;
}

// ---------------------------------------------------------------------------
//  MATERIALES DEL PROYECTO (bloque raiz "materiales")
//
//  Antes viajaban ADENTRO del GLB de cada malla (con su textura embebida) y volvian por
//  ImportGLTF. Sin GLB de guardado hay que escribirlos, o el proyecto reabriria entero en gris.
//  Van una sola vez a la raiz porque en Whisk3D el espacio de nombres de materiales es GLOBAL
//  (BuscarMaterialPorNombre) y el mismo material lo comparten varias mallas; cada .w3dm lo
//  referencia POR NOMBRE en su bloque PARTMAT.
//
//  Se juntan durante el recorrido de la escena, en orden de PRIMERA APARICION (deterministico),
//  y no se escribe el registro global entero: un material que ya no usa nadie no vuelve.
// ---------------------------------------------------------------------------
static std::vector<Material*> gMats;

static void MatRegistrar(Mesh* m) {
    for (size_t g = 0; g < m->materialsGroup.size(); g++) {
        Material* mat = m->materialsGroup[g].material;
        if (!mat) continue;
        bool ya = false;
        for (size_t i = 0; i < gMats.size() && !ya; i++) if (gMats[i] == mat) ya = true;
        if (!ya) gMats.push_back(mat);
    }
}

// una textura -> nombre de entrada ("texturas/piso.png"), ingiriendola al contenedor.
// Texture::path queda con lo que vale EN MEMORIA (el nombre de entrada), que es lo que
// resuelve ReadFileBytes la proxima vez que haya que recargarla.
static std::string MatTextura(CtxGuardar* cx, Texture* t) {
    if (!t || t->path.empty()) return std::string();
    return Asset(cx, t->path);
}
// la textura BASE de un material, contemplando que la carga es DIFERIDA: recien abierto el
// proyecto, mat->texture sigue NULL y la ruta solo esta en la cola. Sin esto, guardar antes de
// que la cola se vacie perdia la textura de cada material EN SILENCIO.
//
//  LA RUTA DE LA COLA SE PASA POR REFERENCIA, IGUAL QUE LA DEL Texture*. Asset()
//  no solo devuelve el nombre de entrada: REESCRIBE la ruta que le pasan con lo
//  que vale EN MEMORIA de ahi en mas. Sobre una COPIA local esa reescritura se
//  perdia, la cola se quedaba con la ruta de DISCO y al reabrir el proyecto
//  W3dMismoMaterial comparaba "/home/.../piso.png" contra "texturas/piso.png":
//  distinto -> material NUEVO, y TODOS los materiales de un modelo recien
//  importado volvian renombrados a ".001".
static std::string MatTexturaBase(CtxGuardar* cx, Material* mt) {
    if (mt->texture && !mt->texture->path.empty()) return Asset(cx, mt->texture->path);
    std::string* p = TexturaPendienteRefDe(mt);
    if (!p || p->empty()) return std::string();
    return Asset(cx, *p);
}

static void EscribirMateriales(std::string& s, CtxGuardar* cx) {
    if (gMats.empty()) return;
    s += "  \"materiales\": [\n";
    for (size_t i = 0; i < gMats.size(); i++) {
        Material* mt = gMats[i];
        gQuien = "material \"" + mt->name + "\"";
        s += "    { \"nombre\": "; JEsc(s, mt->name);
        s += ",\n      \"difuso\": ["; JNum(s, mt->diffuse[0]); s += ", "; JNum(s, mt->diffuse[1]);
        s += ", "; JNum(s, mt->diffuse[2]); s += ", "; JNum(s, mt->diffuse[3]); s += "]";
        s += ",\n      \"especular\": ["; JNum(s, mt->specular[0]); s += ", "; JNum(s, mt->specular[1]);
        s += ", "; JNum(s, mt->specular[2]); s += ", "; JNum(s, mt->specular[3]); s += "]";
        s += ",\n      \"emision\": ["; JNum(s, mt->emission[0]); s += ", "; JNum(s, mt->emission[1]);
        s += ", "; JNum(s, mt->emission[2]); s += ", "; JNum(s, mt->emission[3]); s += "]";
        s += ",\n      \"ambiente\": ["; JNum(s, mt->ambient[0]); s += ", "; JNum(s, mt->ambient[1]);
        s += ", "; JNum(s, mt->ambient[2]); s += ", "; JNum(s, mt->ambient[3]); s += "]";
        s += ",\n      \"brillo\": "; JNum(s, mt->shininess);
        s += ", \"interpolacion\": "; JNum(s, (float)mt->interpolacion);
        s += ", \"reflejoModo\": "; JNum(s, (float)mt->reflectMode);
        // DECAL / mezcla: se escriben SOLO si se apartan del default, asi un proyecto que no los
        // usa guarda exactamente el mismo texto de siempre (los diffs del .w3d siguen legibles).
        if (mt->depth_bias != 0.0f) { s += ", \"sesgoProfundidad\": "; JNum(s, mt->depth_bias); }
        if (mt->orden_pasada != 0)  { s += ", \"ordenPasada\": ";      JNum(s, (float)mt->orden_pasada); }
        if (mt->mezcla != 0)        { s += ", \"mezcla\": ";           JNum(s, (float)mt->mezcla); }
        // LINEAS (aristas por material): solo si esta prendido (mismo criterio que decal)
        if (mt->lineas) {
            s += ", \"lineas\": true";
            if (mt->grosorLinea != 1.0f) { s += ", \"grosorLinea\": "; JNum(s, mt->grosorLinea); }
        }
        s += ",\n      \"flags\": {";
        s += " \"textura\": ";     s += mt->textureOn   ? "true" : "false";
        s += ", \"filtrado\": ";   s += mt->filtrado    ? "true" : "false";
        s += ", \"repetir\": ";    s += mt->repeat      ? "true" : "false";
        s += ", \"transparente\": "; s += mt->transparent ? "true" : "false";
        s += ", \"luz\": ";        s += mt->lighting    ? "true" : "false";
        s += ", \"colorVertice\": "; s += mt->vertexColor ? "true" : "false";
        s += ", \"culling\": ";    s += mt->culling     ? "true" : "false";
        s += ", \"profundidad\": "; s += mt->depth_test ? "true" : "false";
        s += ", \"escribeProfundidad\": "; s += mt->depth_write ? "true" : "false";
        s += ", \"reflejo\": ";    s += mt->chrome      ? "true" : "false";
        s += ", \"normalMap\": ";  s += mt->normalMap   ? "true" : "false";
        s += ", \"uv8bit\": ";     s += mt->uv8bit      ? "true" : "false";
        s += " }";
        std::string tex = MatTexturaBase(cx, mt);
        if (!tex.empty()) { s += ",\n      \"textura\": "; JEsc(s, tex); }
        std::string nrm = MatTextura(cx, mt->normalTexture);
        if (!nrm.empty()) { s += ",\n      \"normalTextura\": "; JEsc(s, nrm); }
        // capas de textura EXTRA (multi-pass): su textura y como se mezcla con lo de abajo
        if (!mt->capas.empty()) {
            s += ",\n      \"capas\": [";
            bool primera = true;
            for (size_t c = 0; c < mt->capas.size(); c++) {
                std::string ct = MatTextura(cx, mt->capas[c].tex);
                if (ct.empty()) continue;
                if (!primera) s += ",";
                primera = false;
                s += " { \"textura\": "; JEsc(s, ct);
                s += ", \"mezcla\": "; JNum(s, (float)mt->capas[c].blend);
                s += ", \"on\": "; s += mt->capas[c].on ? "true" : "false"; s += " }";
            }
            s += " ]";
        }
        s += " }";
        if (i + 1 < gMats.size()) s += ",";
        s += "\n";
    }
    gQuien.clear();
    s += "  ],\n";
}

// bloque "anims" de una malla: cada vertex anim con su rango/fps, los FRAMES de vertices
// como blob BINARIO compacto externo (vtxanim/N.bin al lado del .w3d -> no depende de los
// .obj de origen) y sus curvas de transform. Reemplaza al viejo guardado (solo basePath).
static void EscribirAnimsVertex(std::string& s, Mesh* m, int ind, CtxGuardar* cx) {
    if (m->animations.empty()) return;
    // ---- EL FRENO DE MANO DE LAS ANIMS (hermano del de la malla, mas arriba) -------------
    //  Una vertex anim cuyo blob de frames NO se pudo leer al abrir quedo con 0 keyframes.
    //  Guardar encima la destruye SIN RASTRO: sin frames no se emite "buffers", y el
    //  contenedor descarta a proposito lo huerfano bajo "animaciones/" -> el blob viejo se va
    //  del zip y la anim queda como cascara vacia PARA SIEMPRE. Se frena el guardado entero,
    //  igual que con una malla que no cargo: o sale entero o no sale.
    for (size_t i = 0; i < m->animations.size(); i++) {
        VertexAnimation* a = m->animations[i];
        if (!a || !a->noCargo) continue;
        w3dLogfE("[W3D] '%s': los frames de la vertex anim '%s' no se pudieron leer al abrir: NO guardo encima",
                 m->name.c_str(), a->name.c_str());
        W3dAvisof(true, "No guardo '%s': los frames de su animacion '%s' no se pudieron leer al abrir y guardarlos los borraria",
                  W3dNombreCorto(m->name).c_str(), W3dNombreCorto(a->name).c_str());
        cx->error = true;
        return;
    }
    s += ",\n"; JSang(s, ind); s += "\"anims\": [\n";
    for (size_t i = 0; i < m->animations.size(); i++) {
        VertexAnimation* a = m->animations[i];
        JSang(s, ind + 1); s += "{ \"nombre\": "; JEsc(s, a->name);
        s += ", \"inicio\": "; JNum(s, (float)a->startFrame);
        s += ", \"fin\": "; JNum(s, (float)a->endFrame);
        s += ", \"fps\": "; JNum(s, (float)a->fps);
        s += ", \"velocidad\": "; JNum(s, a->speed);
        s += ", \"repetir\": "; s += a->repeat ? "true" : "false";
        s += ", \"normales\": "; s += a->UseNormals ? "true" : "false";
        s += ", \"proxima\": "; JNum(s, (float)a->proximaAnimacion);
        // referencia .obj legacy (retrocompat / info); la fuente real es el blob de abajo.
        // v4: los .obj los lee ImportWOBJ con std::ifstream (no pasa por el VFS), asi
        // que NO puede vivir adentro del contenedor: queda EXTERNO y a la vista.
        if (!a->basePath.empty()) {
            W3dRefExternaMarcar(a->basePath);
            std::string ref = Asset(cx, a->basePath);
            // EL basePath NO ES UN ARCHIVO: es el PREFIJO de una secuencia de frames
            // ("modelos/heroe/agachado/agachado" -> agachado001.obj, agachado002.obj...
            // ver VertexAnimation::LoadFrames). Ingerir lo comprobaba con FileExists
            // contra ese nombre pelado, que nunca existe -> TODAS las vertex anims
            // entraban al EXTERNOS.txt como "FALTA" y cada guardado terminaba con el
            // cartel ROJO "Guardado, pero N archivo(s) externo(s) NO estan" sin que
            // faltara un solo archivo (52 en el proyecto medido: eso era el "salieron
            // errores al guardar"). El archivo REAL es el primer frame: se pregunta
            // por el. Y si la anim ya viaja con sus frames adentro del contenedor
            // (el blob "buffers" de abajo, el caso normal), no falta nada aunque el
            // .obj original ya no este: el proyecto se abre igual.
            if (a->frames.empty()) {
                std::ostringstream f1;
                f1 << a->basePath << std::setw(a->padding) << std::setfill('0') << 1 << ".obj";
                cx->esc->ExternoCorregirExiste(ref, w3dFileSystem::FileExists(f1.str()));
            } else {
                cx->esc->ExternoCorregirExiste(ref, true);
            }
            s += ", \"base\": "; JEsc(s, ref);
            s += ", \"padding\": "; JNum(s, (float)a->padding);
        }
        // FRAMES de vertices -> blob binario EXTERNO (vtxanim/N.bin). Necesita target
        // para saber el vertexCount; lo fijamos por las dudas (las anims del .w3d traen NULL).
        a->target = m;
        if (!a->frames.empty()) {
            std::vector<unsigned char> blob; VertexAnimSerializar(*a, blob);
            if (!blob.empty()) {
                // una ENTRADA del contenedor bajo animaciones/. Se acabaron los
                // blobs sueltos, el contador que arrancaba en 0 cada vez y la poda
                // de huerfanos que hacia falta por eso.
                std::string base = W3dSlugEntrada(a->name.empty() ? std::string("anim") : a->name);
                std::string nom = "animaciones/" + base + ".bin";
                for (int k = 2; cx->esc->Tiene(nom); k++) {
                    char sf[16]; snprintf(sf, sizeof(sf), "-%d", k);
                    nom = "animaciones/" + base + sf + ".bin";
                }
                if (cx->esc->AgregarBytes(nom, std::string(blob.begin(), blob.end()))) {
                    s += ", \"buffers\": "; JEsc(s, nom);
                } else cx->error = true;
            }
        }
        // curvas de transform PROPIAS de la anim del objeto (pos/rot/escala/visible/render)
        if (!a->curvas.empty()) {
            s += ", \"curvas\": [";
            for (size_t c = 0; c < a->curvas.size(); c++) { if (c) s += ","; EscribirCurva(s, a->curvas[c]); }
            s += "]";
        }
        s += " }";
        if (i + 1 < m->animations.size()) s += ",";
        s += "\n";
    }
    JSang(s, ind); s += "]";
}

// ---------------------------------------------------------------------------
//  LAYOUT vivo -> texto. Antes aca se escribia el literal "2d" y el layout que
//  el usuario armo (splits, Console, UV, etc.) se PERDIA al guardar. Ahora se
//  serializa el arbol REAL de rootViewport en el formato "Layout { ... }" del
//  .w3d de texto viejo, que la lectura reconstruye con el MISMO BuildLayout
//  probado (import_w3d). El texto viaja como string JSON (JEsc escapa los \n).
// ---------------------------------------------------------------------------

// nombre de guardado de una HOJA segun ViewportKind(). Tiene que estar en
// sincronia con BuildLayout (import_w3d.cpp) y LayoutCrearViewport (menu de
// tipos): un viewport nuevo se agrega en los TRES lados.
static const char* LayoutNombreDeHoja(int kind) {
    switch (kind) {
        case 1: return "Viewport3D";
        case 2: return "Outliner";
        case 3: return "Properties";
        case 4: return "UVEditor";
        case 5: return "Timeline";
        case 6: return "Editor2D";
        case 7: return "Console";
        case 8: return "IDE";
    }
    return NULL;   // kind sin nombre todavia: el caller avisa y cae a Viewport3D
}

static void LayoutEscribirNodo(std::string& s, ViewportBase* v, int ind) {
    if (!v) return;   // hueco en un contenedor: BuildLayout lo rellena con un 3D
    int ck = v->ContainerKind();
    if (ck == 1 || ck == 2) {
        ViewportBase* a; ViewportBase* b; float split;
        if (ck == 1) { ViewportRow* r = (ViewportRow*)v; a = r->childA; b = r->childB; split = r->splitFrac; }
        else         { ViewportColumn* c = (ViewportColumn*)v; a = c->childA; b = c->childB; split = c->splitFrac; }
        JSang(s, ind); s += (ck == 1) ? "ViewportRow {\n" : "ViewportColumn {\n";
        LayoutEscribirNodo(s, a, ind + 1);
        LayoutEscribirNodo(s, b, ind + 1);
        JSang(s, ind + 1); s += "Split: "; JNum(s, split); s += "\n";
        JSang(s, ind); s += "}\n";
        return;
    }
    const char* nombre = LayoutNombreDeHoja(v->ViewportKind());
    if (!nombre) {
        // viewport nuevo sin nombre de guardado: no se aborta el layout, va un
        // 3D en su lugar y queda el aviso (agregarlo a LayoutNombreDeHoja)
        w3dLogfW("GuardarW3D: viewport kind=%d sin nombre de guardado, va como Viewport3D", v->ViewportKind());
        nombre = "Viewport3D";
    }
    // ---- LA VISTA DEL VIEWPORT 3D VIAJA CON EL LAYOUT -------------------------
    //  Reporte del dueno: "no se esta guardando la posicion del viewport3d". El
    //  bloque "layout" guardaba la ESTRUCTURA (que viewports hay y como estan
    //  divididos) pero NADA del estado de camara, asi que al reabrir el proyecto
    //  la vista volvia al default y habia que reencuadrar a mano cada vez.
    //  Va ACA, como props de la hoja, y no en un bloque aparte: si hay tres
    //  viewports 3D cada uno tiene SU vista, y atarla a la hoja es lo unico que
    //  no necesita inventar un identificador de viewport.
    //  Los nombres posX/posY/posZ/orbitDistance son los que el lector del formato
    //  de texto viejo YA entendia (ApplyViewport3DProps, import_w3d.cpp), con su
    //  mapeo historico posY<->pivot.z / posZ<->pivot.y: se respeta tal cual para
    //  que un .w3d viejo con esas props siga abriendo igual.
    //  La ROTACION va como QUATERNION (rotQX..W) y no como los euler rotX/rotY/rotZ
    //  del formato viejo: la vista se guarda en un quaternion y pasar por euler y
    //  volver NO es la identidad (gimbal + orden YXZ), o sea que el encuadre
    //  guardado no era el que volvia. Los euler viejos se siguen LEYENDO.
    if (v->ViewportKind() == 1) {
        Viewport3D* v3 = (Viewport3D*)v;
        JSang(s, ind); s += nombre; s += " {\n";
        JSang(s, ind + 1); s += "posX: "; JNum(s, v3->pivot.x); s += "\n";
        JSang(s, ind + 1); s += "posY: "; JNum(s, v3->pivot.z); s += "\n";
        JSang(s, ind + 1); s += "posZ: "; JNum(s, v3->pivot.y); s += "\n";
        JSang(s, ind + 1); s += "orbitDistance: "; JNum(s, v3->orbitDistance); s += "\n";
        JSang(s, ind + 1); s += "rotQX: "; JNum(s, v3->viewRot.x); s += "\n";
        JSang(s, ind + 1); s += "rotQY: "; JNum(s, v3->viewRot.y); s += "\n";
        JSang(s, ind + 1); s += "rotQZ: "; JNum(s, v3->viewRot.z); s += "\n";
        JSang(s, ind + 1); s += "rotQW: "; JNum(s, v3->viewRot.w); s += "\n";
        JSang(s, ind); s += "}\n";
        return;
    }
    // ---- EL MODO Y LA VISTA DEL TIMELINE VIAJAN CON EL LAYOUT -----------------
    //  Reporte del dueno: "no recuerda si quedo en curva o dope sheet el timeline"
    //  y, despues, "falta guardar el zoom y posicion del visor en el timeline en
    //  dope sheet y curvas".
    //  Va como prop de la HOJA por la misma razon que la vista del Viewport3D: el
    //  modo es de ESE timeline (puede haber mas de uno abierto) y atarlo a la hoja
    //  no obliga a inventar un identificador de viewport. Se escribe la PALABRA y
    //  no el numero del enum: el .w3d se edita a mano. Una hoja vieja sin la prop
    //  abre en dope sheet, que es el default de siempre.
    //
    //  POR QUE AL .w3d Y NO AL config.ini: es la misma frontera de siempre. El
    //  encuadre del timeline habla de LOS FRAMES DE ESTE PROYECTO (donde estan sus
    //  keyframes, que tramo estabas mirando); en otro archivo no significa nada,
    //  igual que el frame actual y la seleccion del bloque "sesion". Lo que va al
    //  config.ini son los MODOS DE TRABAJO que acompanan al usuario de proyecto en
    //  proyecto (el AUTO KEY, por ejemplo).
    //
    //  UNA VISTA POR TIMELINE, NO UNA POR CONTEXTO. El timeline puede estar
    //  mostrando la animacion de la escena, un clip de armature, la anim propia de
    //  una malla o un armature 2D. Aun asi la vista guardada es UNA, la del
    //  viewport, por tres razones que van juntas:
    //    (1) el eje horizontal es EL MISMO en todos los contextos (frames, con el
    //        Start/End y el playhead globales del proyecto), asi que el encuadre
    //        que dejaste sirve igual cuando volves a otra animacion;
    //    (2) es lo que hace el viewport 3D, que tambien guarda UNA camara aunque
    //        adentro le cambies la escena entera;
    //    (3) una vista POR CONTEXTO habria que indexarla por el contexto, y la
    //        identidad de un contexto en el dope es "#<serial>" -- un contador de
    //        sesion que NO sobrevive a guardar/cargar (ver el comentario de
    //        DopeIdDueno en Timeline.h). Guardar eso en un archivo seria guardar
    //        una clave que al reabrir ya no apunta a nada.
    //  EL EJE VERTICAL SOLO EN CURVAS: el dope sheet no tiene eje de valor, asi que
    //  en dope no se escriben zoomValor/centroValor. Escribirlos igual seria
    //  inventarle al usuario un encuadre vertical que nunca eligio; sin ellos, la
    //  primera vez que entre a curvas manda el auto-encuadre, que es justo lo que
    //  ya hace hoy.
    if (v->ViewportKind() == 5) {
        Timeline* tl = (Timeline*)v;
        JSang(s, ind); s += nombre; s += " {\n";
        JSang(s, ind + 1); s += "modo: ";
        s += (tl->modo == Timeline::TL_MODO_CURVAS) ? "curvas" : "dope";
        s += "\n";
        JSang(s, ind + 1); s += "zoomFrame: ";  JNum(s, tl->pxPerFrame);  s += "\n"; // px por frame
        JSang(s, ind + 1); s += "frameIzq: ";   JNum(s, tl->viewStartF);  s += "\n"; // frame del borde izquierdo
        if (tl->modo == Timeline::TL_MODO_CURVAS) {
            JSang(s, ind + 1); s += "zoomValor: ";   JNum(s, tl->pxPerUnit);   s += "\n"; // px por unidad de valor
            JSang(s, ind + 1); s += "centroValor: "; JNum(s, tl->viewCenterV); s += "\n"; // valor en el centro
        }
        JSang(s, ind); s += "}\n";
        return;
    }
    JSang(s, ind); s += nombre; s += "\n";
}

// el layout ACTUAL entero como snippet "Layout { ... }" (lo que entiende
// BuildLayout). "" si no hay arbol (guardado sin editor arriba).
static std::string LayoutSerializarVivo() {
    if (!rootViewport) return std::string();
    std::string s = "Layout {\n";
    LayoutEscribirNodo(s, rootViewport, 1);
    s += "}";
    return s;
}

// ---------------------------------------------------------------------------
//  BLOQUE "sesion": DONDE ESTABA TRABAJANDO EL USUARIO EN ESTE PROYECTO
//
//  Reportes del dueno: "no se esta guardando lo que estaba seleccionado" y "no se
//  guarda en que parte del timeline/frame actual estaba".
//
//  QUE VA ACA Y QUE NO. Lo que va al .w3d es lo que solo tiene sentido CON ESTE
//  proyecto abierto: el frame en el que estabas y los objetos que tenias elegidos
//  no significan nada en otro archivo. El AUTO KEY, en cambio, es un modo de
//  trabajo del editor (grabar o no grabar mientras transformas) y acompana al
//  usuario de proyecto en proyecto -> ese vive en el config.ini (ver variables.h).
//  El MODO del timeline (dope/curvas) va con SU hoja del layout, no aca: puede
//  haber mas de un timeline abierto y cada uno tiene el suyo.
//
//  LA SELECCION VA POR NOMBRE, no por indice: los nombres son UNICOS por escena
//  (regla ya vigente) y un indice se corre en cuanto alguien edita el .w3d o el
//  orden de los hijos cambia. Al leer, un nombre que no aparece se IGNORA: la
//  seleccion es un comodidad, nunca un motivo para no abrir el proyecto.
//
//  Solo se nombran objetos que esten COLGANDO DE LA ESCENA: ObjActivo puede
//  apuntar a algo que no es parte del arbol que se guarda, y entonces el nombre
//  que quedaria escrito no lo va a poder resolver nadie.
// ---------------------------------------------------------------------------
static bool EnLaEscena(Object* raiz, Object* o) {
    if (!raiz || !o) return false;
    if (raiz == o) return true;
    for (size_t i = 0; i < raiz->Childrens.size(); i++)
        if (EnLaEscena(raiz->Childrens[i], o)) return true;
    return false;
}

static void EscribirSesion(std::string& s) {
    s += "  \"sesion\": {\n";
    // JInt y no JNum((float)f): de 1e6 para arriba el float sale en notacion cientifica
    // ("1.23457e+06") y ese texto ya no es el entero que era (ver JInt)
    s += "    \"frame\": "; JInt(s, CurrentFrame); s += ",\n";
    s += "    \"activo\": ";
    JEsc(s, (ObjActivo && EnLaEscena(SceneCollection, ObjActivo)) ? ObjActivo->name : std::string());
    s += ",\n";
    s += "    \"seleccion\": [";
    {
        bool primero = true;
        for (size_t i = 0; i < ObjSelects.size(); i++) {
            Object* o = ObjSelects[i];
            if (!o || !EnLaEscena(SceneCollection, o)) continue;
            if (!primero) s += ", ";
            JEsc(s, o->name);
            primero = false;
        }
    }
    s += "]\n";
    s += "  },\n";
}

// ---------------------------------------------------------------------------
//  ARMATURE -> JSON (Fase 3). Esquema:
//    { "tipo": "armature", <comunes>, "autorado": true?, "animActiva": N?,
//      "bones": [ { "nombre", "padre", "head": [xyz], "tail": [xyz],
//                   (solo rigs importados con rest:) "rest": true, "restT/R/S", "preRot", "postRot",
//                   "rotOrder", "bind": [16], "cluster": [16] } ],
//      "anims": [ { "nombre", "fps", "inicio", "fin",
//                   "tracks": [ { "hueso": N, "curvas": [ {p,c,k:[...]} ] } ] } ],
//      "hijos": [...] }
//  En un rig AUTORADO el rest se DERIVA de head/tail (PrepararSkinAutorado al abrir) -> no se escribe.
//  En un rig importado (FBX/glTF) se escriben rest + bind/cluster para reconstruir el FK/skinning
//  (PrepararSkin al abrir) sin depender del archivo original.
// ---------------------------------------------------------------------------
static void EscribirArmature(std::string& s, Armature* a, int ind) {
    if (a->skinAutorado) { s += ",\n"; JSang(s, ind); s += "\"autorado\": true"; }
    if (a->animActiva >= 0) { s += ",\n"; JSang(s, ind); s += "\"animActiva\": "; JNum(s, (float)a->animActiva); }
    s += ",\n"; JSang(s, ind); s += "\"bones\": [\n";
    for (size_t i = 0; i < a->bones.size(); i++) {
        const W3dBone& b = a->bones[i];
        JSang(s, ind + 1); s += "{ \"nombre\": "; JEsc(s, b.name);
        s += ", \"padre\": "; JNum(s, (float)b.parent);
        s += ", \"head\": "; JVec(s, b.head.x, b.head.y, b.head.z);
        s += ", \"tail\": "; JVec(s, b.tail.x, b.tail.y, b.tail.z);
        // "Disconnect Bone": emparentado SIN soldar. Solo se escribe cuando aplica (round-trip
        // estable; los .w3d viejos sin el campo cargan conectado=true = comportamiento de siempre)
        if (b.parent >= 0 && !b.conectado) s += ", \"suelto\": true";
        if (b.hasRest && !a->skinAutorado) { // rig importado: fidelidad del FK/skinning
            s += ", \"rest\": true";
            s += ", \"restT\": "; JVec(s, b.restT.x, b.restT.y, b.restT.z);
            s += ", \"restR\": "; JVec(s, b.restR.x, b.restR.y, b.restR.z);
            s += ", \"restS\": "; JVec(s, b.restS.x, b.restS.y, b.restS.z);
            s += ", \"preRot\": "; JVec(s, b.preRot.x, b.preRot.y, b.preRot.z);
            s += ", \"postRot\": "; JVec(s, b.postRot.x, b.postRot.y, b.postRot.z);
            s += ", \"rotOrder\": "; JNum(s, (float)b.rotOrder);
            s += ", \"bind\": "; JMat(s, b.bind);
            s += ", \"cluster\": "; JMat(s, b.clusterTransform);
        }
        s += " }";
        if (i + 1 < a->bones.size()) s += ",";
        s += "\n";
    }
    JSang(s, ind); s += "]";
    if (!a->animations.empty()) {
        s += ",\n"; JSang(s, ind); s += "\"anims\": [\n";
        for (size_t i = 0; i < a->animations.size(); i++) {
            SkeletalAnimation* an = a->animations[i];
            JSang(s, ind + 1); s += "{ \"nombre\": "; JEsc(s, an ? an->name : std::string("clip"));
            if (an) {
                s += ", \"fps\": "; JNum(s, (float)an->FrameRate);
                s += ", \"inicio\": "; JNum(s, (float)an->startFrame);
                s += ", \"fin\": "; JNum(s, (float)an->endFrame);
                if (!an->tracks.empty()) {
                    s += ", \"tracks\": [";
                    for (size_t t = 0; t < an->tracks.size(); t++) {
                        if (t) s += ",";
                        s += " { \"hueso\": "; JNum(s, (float)an->tracks[t].bone);
                        s += ", \"curvas\": [";
                        for (size_t c = 0; c < an->tracks[t].Propertys.size(); c++) {
                            if (c) s += ",";
                            EscribirCurva(s, an->tracks[t].Propertys[c]);
                        }
                        s += "] }";
                    }
                    s += " ]";
                }
            }
            s += " }";
            if (i + 1 < a->animations.size()) s += ",";
            s += "\n";
        }
        JSang(s, ind); s += "]";
    }
}

// ---------------------------------------------------------------------------
//  RIG de una malla (Fase 3): la referencia al modificador Armature (POR NOMBRE del objeto; se
//  resuelve al abrir) + los VERTEX GROUPS. El GLB de una malla suelta NO lleva JOINTS/WEIGHTS
//  (el skin de glTF exige exportar el esqueleto junto), asi que los pesos van en el .w3d, POR
//  POSICION del control-point: robusto al re-split de vertices del GLB al reabrir.
// ---------------------------------------------------------------------------
static void EscribirRigMesh(std::string& s, Mesh* m, int ind) {
    Modifier* mod = NULL;
    for (size_t i = 0; i < m->modificadores.size(); i++)
        if (m->modificadores[i] && m->modificadores[i]->tipo == ModifierType::Armature) { mod = m->modificadores[i]; break; }
    if (mod && mod->target) {
        s += ",\n"; JSang(s, ind); s += "\"modArmature\": "; JEsc(s, mod->target->name);
        if (mod->cacheAnim) { s += ",\n"; JSang(s, ind); s += "\"modArmatureCache\": true"; }
        if (mod->cacheSkip > 0.5f) { s += ",\n"; JSang(s, ind); s += "\"modArmatureSkip\": "; JNum(s, mod->cacheSkip); }
    }
    // ARMATURES 2D DEL MESH (huesos en espacio UV; binding POR NOMBRE con los uvgroups de abajo).
    // Viajan como campo del objeto malla, junto a vgroups/modArmature. FORMATO NUEVO (multi-rig):
    //   "armatures2d": [ { "nombre":..., "huesos":[...], "anims":[...], "animActiva":N }, ... ]
    // El formato VIEJO (un solo rig suelto en "armature2d" + "anims2d" + "anim2dActiva") se sigue
    // LEYENDO (import_w3d lo migra al armature 0) pero ya no se escribe.
    // "pose" solo si no es identidad (round-trip estable: lo que no se escribe no puede diferir
    // al re-guardar).
    if (m->TieneArm2D()) {
        s += ",\n"; JSang(s, ind); s += "\"armatures2d\": [\n";
        for (size_t a = 0; a < m->armatures2d.size(); a++) {
            const Armature2D* arm = m->armatures2d[a];
            JSang(s, ind + 1); s += "{ \"nombre\": "; JEsc(s, arm ? arm->nombre : std::string("Armature 2D"));
            if (arm) {
                s += ",\n"; JSang(s, ind + 2); s += "\"huesos\": [\n";
                for (size_t b = 0; b < arm->huesos.size(); b++) {
                    const W3dBone2D& hb = arm->huesos[b];
                    JSang(s, ind + 3); s += "{ \"nombre\": "; JEsc(s, hb.nombre);
                    s += ", \"padre\": "; JNum(s, (float)hb.padre);
                    s += ", \"head\": ["; JNum(s, hb.headU); s += ", "; JNum(s, hb.headV); s += "]";
                    s += ", \"tail\": ["; JNum(s, hb.tailU); s += ", "; JNum(s, hb.tailV); s += "]";
                    // "Disconnect Bone": emparentado SIN soldar (solo si aplica, como el armature 3D)
                    if (hb.padre >= 0 && !hb.conectado) s += ", \"suelto\": true";
                    if (!hb.PoseIdentidad()) {
                        s += ", \"pose\": ["; JNum(s, hb.poseTU); s += ", "; JNum(s, hb.poseTV);
                        s += ", "; JNum(s, hb.poseRot); s += ", "; JNum(s, hb.poseSX);
                        s += ", "; JNum(s, hb.poseSY); s += "]";
                    }
                    s += " }";
                    if (b + 1 < arm->huesos.size()) s += ",";
                    s += "\n";
                }
                JSang(s, ind + 2); s += "]";
                // CLIPS de ESTE armature (curvas por hueso). MISMO formato que el "anims" del
                // armature 3D -reusa EscribirCurva sin tocarla- pero con los 5 canales 2D:
                // (AnimPosition,X/Y) = U/V, (AnimRotation,X) = grados, (AnimScale,X/Y).
                if (!arm->anims.empty()) {
                    s += ",\n"; JSang(s, ind + 2); s += "\"anims\": [\n";
                    for (size_t i = 0; i < arm->anims.size(); i++) {
                        Armature2DAnimation* an = arm->anims[i];
                        JSang(s, ind + 3); s += "{ \"nombre\": "; JEsc(s, an ? an->name : std::string("Anim2D"));
                        if (an) {
                            s += ", \"fps\": "; JNum(s, (float)an->fps);
                            s += ", \"inicio\": "; JNum(s, (float)an->startFrame);
                            s += ", \"fin\": "; JNum(s, (float)an->endFrame);
                            // CURVAS VACIAS: no se escriben. Un track puede tener AnimProperty sin
                            // ningun keyframe (las creaba la evaluacion vieja, y tambien quedan al
                            // borrar el ultimo keyframe de un canal); guardarlas engorda el .w3d y
                            // hace ruido en el dope sheet al reabrir. Un track que queda SIN curvas
                            // tampoco se escribe.
                            bool hayTracks = false;
                            for (size_t t = 0; t < an->tracks.size() && !hayTracks; t++)
                                for (size_t c = 0; c < an->tracks[t].Propertys.size() && !hayTracks; c++)
                                    if (!an->tracks[t].Propertys[c].keyframes.empty()) hayTracks = true;
                            if (hayTracks) {
                                s += ", \"tracks\": [";
                                bool primerTrack = true;
                                for (size_t t = 0; t < an->tracks.size(); t++) {
                                    bool hayCurvas = false;
                                    for (size_t c = 0; c < an->tracks[t].Propertys.size() && !hayCurvas; c++)
                                        if (!an->tracks[t].Propertys[c].keyframes.empty()) hayCurvas = true;
                                    if (!hayCurvas) continue;
                                    if (!primerTrack) s += ",";
                                    primerTrack = false;
                                    s += " { \"hueso\": "; JNum(s, (float)an->tracks[t].bone);
                                    s += ", \"curvas\": [";
                                    bool primerCurva = true;
                                    for (size_t c = 0; c < an->tracks[t].Propertys.size(); c++) {
                                        if (an->tracks[t].Propertys[c].keyframes.empty()) continue;
                                        if (!primerCurva) s += ",";
                                        primerCurva = false;
                                        EscribirCurva(s, an->tracks[t].Propertys[c]);
                                    }
                                    s += "] }";
                                }
                                s += " ]";
                            }
                        }
                        s += " }";
                        if (i + 1 < arm->anims.size()) s += ",";
                        s += "\n";
                    }
                    JSang(s, ind + 2); s += "]";
                    if (arm->animActiva >= 0) { s += ",\n"; JSang(s, ind + 2); s += "\"animActiva\": "; JNum(s, (float)arm->animActiva); }
                }
                s += "\n"; JSang(s, ind + 1);
            }
            s += "}";
            if (a + 1 < m->armatures2d.size()) s += ",";
            s += "\n";
        }
        JSang(s, ind); s += "]";
        // el armature ACTIVO solo si NO es el 0 (asi un proyecto de 1 rig no engorda ni cambia)
        if (m->armature2dActivo > 0) { s += ",\n"; JSang(s, ind); s += "\"armature2dActivo\": "; JNum(s, (float)m->armature2dActivo); }
    }
    // LOS VERTEX GROUPS Y LOS UV GROUPS YA NO ESTAN ACA: se fueron a los bloques VG y UVG del
    // .w3dm. Existian en el JSON, y ademas por CLAVE GEOMETRICA (los vgroups por la posicion del
    // control-point, los uvgroups por pos+uv+normal del corner), SOLO porque el GLB re-splitea los
    // vertices y ningun indice sobrevivia la ida y vuelta. Con puntos de control y esquinas
    // explicitas los INDICES son el dato y no hace falta re-matchear nada por cercania.
    // El LECTOR los sigue entendiendo: es como abren los .w3d ya guardados (ver import_w3d).
}

// ---------------------------------------------------------------------------
//  STACK de modificadores de la malla (AUDIT del guardado por versiones): antes
//  solo viajaba el Armature (campo "modArmature") y un Mirror/Screw/SubSurf/etc
//  se PERDIAN al reabrir el proyecto. El Armature se SIGUE guardando aparte como
//  "modArmature" (retrocompat: los .w3d existentes y el lector viejo lo entienden);
//  aca van LOS DEMAS tipos, en el orden del stack (el orden importa). Como el
//  Armature no genera malla (deforma por-frame en el render), reconstruirlo al
//  final del stack al abrir no cambia el resultado. Se escriben TODOS los params
//  siempre -> el re-guardado da byte-identico (round-trip estable).
// ---------------------------------------------------------------------------
static const char* TipoModStr(int t) {
    switch (t) {
        case ModifierType::Screw:              return "screw";
        case ModifierType::Mirror:             return "mirror";
        case ModifierType::Array:              return "array";
        case ModifierType::SubdivisionSurface: return "subsurf";
        case ModifierType::Boolean:            return "boolean";
        case ModifierType::CullingTri:         return "cullingtri";
    }
    return NULL;   // Armature va aparte (modArmature); un tipo nuevo se agrega aca y en el lector
}

static void EscribirModificadores(std::string& s, Mesh* m, int ind) {
    bool alguno = false;
    for (size_t i = 0; i < m->modificadores.size(); i++)
        if (m->modificadores[i] && TipoModStr(m->modificadores[i]->tipo)) { alguno = true; break; }
    if (!alguno) return;
    s += ",\n"; JSang(s, ind); s += "\"modificadores\": [\n";
    bool primero = true;
    for (size_t i = 0; i < m->modificadores.size(); i++) {
        Modifier* md = m->modificadores[i];
        const char* tn = md ? TipoModStr(md->tipo) : NULL;
        if (!tn) continue;
        if (!primero) s += ",\n";
        primero = false;
        JSang(s, ind + 1); s += "{ \"tipo\": "; JEsc(s, tn);
        s += ", \"nombre\": "; JEsc(s, md->nombre);
        if (!md->mostrarViewport) s += ", \"viewport\": false";
        if (!md->mostrarEdit)     s += ", \"edit\": false";
        if (md->tipo == ModifierType::Mirror) {
            s += ", \"ejes\": [";
            s += md->ejeX ? "true" : "false"; s += ", ";
            s += md->ejeY ? "true" : "false"; s += ", ";
            s += md->ejeZ ? "true" : "false"; s += "]";
            if (md->target) { s += ", \"target\": "; JEsc(s, md->target->name); }
            s += ", \"merge\": "; s += md->merge ? "true" : "false";
            s += ", \"mergeDist\": "; JNum(s, md->mergeDist);
            s += ", \"clipping\": "; s += md->clipping ? "true" : "false";
        } else if (md->tipo == ModifierType::SubdivisionSurface) {
            s += ", \"nivel\": "; JNum(s, md->subLevel);
            s += ", \"nivelRender\": "; JNum(s, md->subRenderLevel);
            s += ", \"simple\": "; s += md->subSimple ? "true" : "false";
        } else if (md->tipo == ModifierType::Screw) {
            s += ", \"angulo\": "; JNum(s, md->screwAngle);
            s += ", \"altura\": "; JNum(s, md->screwHeight);
            s += ", \"pasos\": "; JNum(s, md->screwSteps);
            s += ", \"pasosRender\": "; JNum(s, md->screwRenderSteps);
            s += ", \"eje\": "; JNum(s, (float)md->screwAxis);
            s += ", \"stretchU\": "; s += md->screwStretchU ? "true" : "false";
            s += ", \"stretchV\": "; s += md->screwStretchV ? "true" : "false";
            s += ", \"suave\": "; s += md->screwSmooth ? "true" : "false";
            s += ", \"soldar\": "; s += md->screwMerge ? "true" : "false";
            s += ", \"flip\": "; s += md->screwFlip ? "true" : "false";
        } else if (md->tipo == ModifierType::CullingTri) {
            // Culling por triangulo: el metodo, el sector/celda activo y el NOMBRE del
            // sidecar. Los datos no se copian al .w3d (son megabytes de indices): el
            // sidecar (JSON de sectores o `.w3dvis`) se INGIERE al contenedor como
            // cualquier otro asset y se relee de ahi. metodo 1 se escribe "celdas"
            // (el viejo "bsp" nunca corto nada; al releer, "bsp" cae a "celdas").
            s += ", \"metodo\": "; JEsc(s, std::string(md->metodoPVS == 1 ? "celdas" : "triangulos"));
            s += ", \"sector\": "; JNum(s, (float)md->sectorPVS);
            // fallback de celda vacia (-1 = completa, N = celda N). Solo si se usa,
            // para que los proyectos que no lo declaran queden byte a byte como antes.
            if (md->sectorFallback != 0) { s += ", \"sectorFallback\": "; JNum(s, (float)md->sectorFallback); }
            if (!md->pvsArchivo.empty()) { s += ", \"pvs\": "; JEsc(s, md->pvsArchivo); }
            if (!md->visArchivo.empty()) { s += ", \"vis\": "; JEsc(s, md->visArchivo); }
        }
        // Array y Boolean: todavia sin params ni generacion (se guarda tipo/nombre
        // para no perder el stack que armo el usuario)
        s += " }";
    }
    s += "\n"; JSang(s, ind); s += "]";
}

// ---------------------------------------------------------------------------
//  EL STACK DE CONSTRAINTS (objects/W3dConstraint.h): una PROPIEDAD de CUALQUIER
//  objeto, no un tipo de objeto.
//
//  El 'tipo' viaja como STRING y NUNCA como el numero del enum. Es la misma regla
//  que TipoModStr y no es cosmetica: el enum es orden de codigo, asi que meter un
//  tipo nuevo en el MEDIO de W3dConstraintTipo re-interpretaria en silencio todos
//  los archivos ya guardados (un Billboard volveria como Copy Rotation). Con el
//  string, el que no se conoce se saltea y se dice.
//
//  UN OBJETO SIN CONSTRAINTS NO ESCRIBE NADA (ni la clave vacia): el 99% de los
//  objetos no tiene, y un proyecto que no los usa tiene que salir byte a byte
//  igual que antes de que esto existiera.
// ---------------------------------------------------------------------------
static const char* TipoConsStr(int t) {
    switch (t) {
        case W3dConstraintTipo::CopyLocation: return "copyLocation";
        case W3dConstraintTipo::CopyRotation: return "copyRotation";
        case W3dConstraintTipo::Billboard:    return "billboard";
    }
    return NULL;   // un tipo nuevo se agrega aca y en el lector (TipoConsInt)
}

static void EscribirConstraints(std::string& s, Object* o, int ind) {
    bool alguno = false;
    for (size_t i = 0; i < o->constraints.size(); i++)
        if (o->constraints[i] && TipoConsStr(o->constraints[i]->tipo)) { alguno = true; break; }
    if (!alguno) return;
    s += ",\n"; JSang(s, ind); s += "\"constraints\": [\n";
    bool primero = true;
    for (size_t i = 0; i < o->constraints.size(); i++) {
        W3dConstraint* c = o->constraints[i];
        const char* tn = c ? TipoConsStr(c->tipo) : NULL;
        if (!tn) continue;
        if (!primero) s += ",\n";
        primero = false;
        JSang(s, ind + 1); s += "{ \"tipo\": "; JEsc(s, tn);
        s += ", \"nombre\": "; JEsc(s, c->nombre);
        if (!c->activo) s += ", \"activo\": false";     // el "ojito": solo se escribe apagado
        // "ver en modo edicion": el default es OFF, asi que solo se escribe PRENDIDO (mismo
        // criterio que arriba: la clave ausente = el default, y un .w3d tipico no la lleva)
        if (c->mostrarEdit) s += ", \"vered\": true";
        s += ", \"influencia\": "; JNum(s, c->influencia);
        if (c->tipo == W3dConstraintTipo::Billboard) {
            s += ", \"yaw\": ";   s += c->bbYaw   ? "true" : "false";
            s += ", \"pitch\": "; s += c->bbPitch ? "true" : "false";
        } else {
            // LA FUENTE. El TIPO es explicito ("vista" / "objeto") y no se deduce de que
            // haya nombre: sin el, borrar el objeto fuente (que deja el puntero en NULL)
            // convertiria un Copy Location en un "seguir a la camara" al reabrir.
            const bool vista = (c->fuenteTipo == W3dConstraintFuente::Vista);
            s += ", \"fuente\": "; JEsc(s, vista ? std::string("vista") : std::string("objeto"));
            if (!vista) {
                // se prefiere el nombre del objeto VIVO: si lo renombraron sin pasar por el
                // rename del editor, el puntero es la verdad y el nombre cacheado el rezago.
                std::string fu = c->fuenteObj ? c->fuenteObj->name : c->fuenteNombre;
                if (!fu.empty()) { s += ", \"objeto\": "; JEsc(s, fu); }
            }
            s += ", \"ejes\": [";
            s += c->ejeX ? "true" : "false"; s += ", ";
            s += c->ejeY ? "true" : "false"; s += ", ";
            s += c->ejeZ ? "true" : "false"; s += "]";
        }
        s += " }";
    }
    s += "\n"; JSang(s, ind); s += "]";
}

static void EscribirObjeto(std::string& s, Object* o, int ind, CtxGuardar* cx, bool* primero,
                           const std::string* padre2d = NULL);

// un hijo que NO va en el .w3d: los ELEMENTOS 2D de una escena UI viven en SU
// .w3dui (UI2DFormato::EscribirHijos). Escribirlos tambien aca los DUPLICARIA al
// reabrir (el .w3dui los crea y despues el .w3d los volveria a crear).
static bool HijoVaEnOtroArchivo(Object* padre, Object* hijo) {
    return padre->getType() == ObjectType::ui && UI2D_EsElemento2D(hijo);
}

// ---------------------------------------------------------------------------
//  FALLO B: LO QUE CUELGA DE UN ELEMENTO 2D NO SE PERDIA "UN POCO", SE PERDIA ENTERO
//
//  El .w3d corta la recursion en los elementos 2D (arriba) y el .w3dui escribe SOLO
//  tipos 2D (lista blanca en UI2DFormato::EscribirHijos), asi que un objeto 3D
//  emparentado bajo un widget -por Ctrl+P, por el drag&drop del outliner, por el modo
//  mover o por la tarjeta Parent- no lo guardaba NINGUNO de los dos archivos y
//  DESAPARECIA al guardar, sin un solo aviso.
//
//  DECISION: viaja en el .w3d, que es el archivo maestro (tiene el arbol entero y es el
//  unico de los dos que sabe serializar una malla, una camara o una luz; el .w3dui
//  ademas lo consume el runtime de los juegos, que no instancia objetos 3D). Se escribe
//  como hijo del NODO UI con el campo "padre2d" = el nombre del elemento del que
//  cuelga, y al abrir se lo re-cuelga de ese elemento exacto (import_w3d.cpp), que para
//  entonces ya existe porque lo creo el .w3dui. El nombre alcanza como identidad: el
//  espacio de nombres de los objetos es POR ESCENA (W3dNombreScopeDe) y la escena UI ES
//  un scope, asi que dentro de una UI no hay dos elementos con el mismo nombre.
//  El sentido contrario (un elemento 2D bajo un objeto 3D) NO llega hasta aca: se
//  bloquea en el reparent, con aviso (ver CruceUIValido en ObjectMode.cpp).
// ---------------------------------------------------------------------------
struct HuespedUI { Object* obj; std::string padre2d; };
static void JuntarHuespedesUI(Object* nodo2d, std::vector<HuespedUI>& out) {
    for (size_t i = 0; i < nodo2d->Childrens.size(); i++) {
        Object* h = nodo2d->Childrens[i];
        if (UI2D_EsElemento2D(h)) { JuntarHuespedesUI(h, out); continue; }   // sigue en el .w3dui
        HuespedUI g; g.obj = h; g.padre2d = nodo2d->name; out.push_back(g);  // y su subarbol entero
    }
}

// ---------------------------------------------------------------------------
//  LOS HIJOS SE ESCRIBEN EN UN SOLO LUGAR (ver EscribirObjeto, al final)
//
//  Antes cada rama de EscribirObjeto llamaba -o se OLVIDABA de llamar- a esta
//  funcion: coleccion/espejo/instancia/constraint/curva/armature si, y ui,
//  script, camara, luz, malla (modelo y glb) y el generico NO. O sea que
//  guardar PERDIA, sin un solo aviso, todo lo que colgara de una camara, una
//  luz, un script, una UI o una malla ("add cube / parent Cubo.001 Cubo /
//  guardarw3d" y Cubo.001 no estaba en el archivo). Ahora la llamada esta UNA
//  vez, despues del if/else, asi que una rama nueva hereda la jerarquia sin que
//  su autor tenga que acordarse de nada.
// ---------------------------------------------------------------------------
static void EscribirHijos(std::string& s, Object* o, int ind, CtxGuardar* cx) {
    // los NO-2D que cuelgan de los elementos 2D de esta UI: van aca, con "padre2d" (fallo B)
    std::vector<HuespedUI> huespedes;
    if (o->getType() == ObjectType::ui)
        for (size_t i = 0; i < o->Childrens.size(); i++)
            if (UI2D_EsElemento2D(o->Childrens[i])) JuntarHuespedesUI(o->Childrens[i], huespedes);
    size_t n = huespedes.size();
    for (size_t i = 0; i < o->Childrens.size(); i++)
        if (!HijoVaEnOtroArchivo(o, o->Childrens[i])) n++;
    if (n == 0) return;
    s += ",\n"; JSang(s, ind); s += "\"hijos\": [\n";
    bool primero = true;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        if (HijoVaEnOtroArchivo(o, o->Childrens[i])) continue;
        EscribirObjeto(s, o->Childrens[i], ind + 1, cx, &primero);
    }
    for (size_t i = 0; i < huespedes.size(); i++)
        EscribirObjeto(s, huespedes[i].obj, ind + 1, cx, &primero, &huespedes[i].padre2d);
    s += "\n"; JSang(s, ind); s += "]";
}

// los scripts lua de un objeto (estilo Unity): [{archivo, refs{prop: valor}}]
static void EscribirScripts(std::string& s, Object* o, int ind, CtxGuardar* cx) {
    if (!o->scriptDatos || o->scriptDatos->scripts.empty()) return;
    s += ",\n"; JSang(s, ind); s += "\"scripts\": [\n";
    for (size_t i = 0; i < o->scriptDatos->scripts.size(); i++) {
        // NO const: guardando a otra carpeta, Asset() copia el .lua adentro del
        // proyecto nuevo y deja la entrada apuntando a la copia
        W3dScriptEntrada& e = o->scriptDatos->scripts[i];
        JSang(s, ind + 1); s += "{ \"archivo\": "; JEsc(s, Asset(cx, e.ruta));
        if (!e.refs.empty()) {
            // ORDEN DETERMINISTICO: los refs se escriben SIEMPRE por nombre. En memoria
            // son un vector (orden de llegada) pero el JSON los relee a un std::map, o
            // sea ALFABETICO: guardar en orden de llegada hacia que el primer guardado
            // (venido del .w3d de texto: ref_* y despues val_*) y el segundo (venido del
            // v4, todo junto y ordenado) dieran BYTES DISTINTOS sin que cambiara un solo
            // dato. Ordenar aca deja el guardado idempotente sin tocar la semantica: los
            // refs son un diccionario nombre->valor, no una secuencia.
            std::vector<std::pair<std::string, std::string> > ord(e.refs.begin(), e.refs.end());
            std::sort(ord.begin(), ord.end());
            s += ", \"refs\": { ";
            for (size_t r = 0; r < ord.size(); r++) {
                if (r) s += ", ";
                JEsc(s, ord[r].first); s += ": "; JEsc(s, ord[r].second);
            }
            s += " }";
        }
        s += " }";
        if (i + 1 < o->scriptDatos->scripts.size()) s += ",";
        s += "\n";
    }
    JSang(s, ind); s += "]";
}

static void EscribirObjeto(std::string& s, Object* o, int ind, CtxGuardar* cx, bool* primero,
                           const std::string* padre2d) {
    if (!o) return;
    ObjectType t = o->getType();

    // AUDIT versiones: curve/constraint/mirror/instance ahora SI se guardan (antes
    // se salteaban y se PERDIAN al re-guardar un proyecto de texto viejo). El unico
    // caso que sigue afuera A PROPOSITO es la GEOMETRIA de una Curve sin archivo de
    // origen: la clase no guarda sus vertices (los carga de un .txt "count/p x y z")
    // y serializar la geometria inline no tiene editor que la produzca hoy. Se cuenta
    // y se avisa. El NODO igual se escribe (nombre/transform/HIJOS): saltearlo entero
    // se llevaba puesto todo el subarbol que colgara de la curva, en silencio.
    if (t == ObjectType::curve && ((Curve*)o)->origen.empty()) gNoCubiertos++;

    if (!*primero) s += ",\n";
    *primero = false;

    // de quien es cada referencia que quede AFUERA (EXTERNOS.txt)
    gQuien = "objeto \"" + o->name + "\"";

    JSang(s, ind); s += "{\n";
    // el nodo cuelga de un ELEMENTO 2D de esta escena UI (ver JuntarHuespedesUI): el .w3d lo
    // escribe bajo el nodo UI y este campo dice de QUE elemento re-colgarlo al abrir. Ausente
    // en todos los demas nodos -> el archivo sale byte a byte igual que antes.
    if (padre2d && !padre2d->empty()) { JSang(s, ind + 1); s += "\"padre2d\": "; JEsc(s, *padre2d); s += ",\n"; }

    if (t == ObjectType::ui) {
        // cada escena UI es OTRO json (.w3dui) EXTERNO, referenciado por ruta
        // relativa al .w3d: se pisa el archivo del que vino (UI::archivoW3dui,
        // que conserva la subcarpeta, ej "contenido/menu.w3dui"). Una UI nueva
        // toma "<nombre>.w3dui", bajo contenido/ con la estructura nueva.
        UI* u = (UI*)o;
        // la escena es una ENTRADA del contenedor (escenas/<slug>.w3dui). El nombre
        // sale del nombre VISIBLE de la escena, slugueado; el nombre visible no se
        // toca nunca (vive en el .w3dui) y el de entrada es un derivado.
        std::string visible = u->archivoW3dui.empty()
                                  ? (o->name.empty() ? std::string("ui") : o->name)
                                  : BaseSinExt(u->archivoW3dui);
        std::string nom = "escenas/" + W3dSlugEntrada(visible) + ".w3dui";
        for (int k = 2; cx->esc->Tiene(nom); k++) {
            char sf[16]; snprintf(sf, sizeof(sf), "-%d", k);
            nom = "escenas/" + W3dSlugEntrada(visible) + sf + ".w3dui";
        }
        u->archivoW3dui = nom;
        // se escribe a un temporal EN LA CARPETA DEL DESTINO y de ahi entra al zip
        // (UI2DGuardar solo sabe escribir a un path). El temporal se borra siempre.
        std::string tmpUi = cx->esc->RutaTemporal("e");
        std::string quienAntes = gQuien;
        gQuien = "escena \"" + o->name + "\"";
        bool okUi = UI2DGuardar(u, tmpUi, std::string());
        gQuien = quienAntes;
        if (!okUi || !cx->esc->AgregarTemporal(nom, tmpUi)) {
            remove(tmpUi.c_str());
            w3dLogfE("GuardarW3D: no pude escribir la escena UI %s", nom.c_str());
            cx->error = true;
        }
        JSang(s, ind + 1); s += "\"tipo\": \"ui\",\n";
        JSang(s, ind + 1); s += "\"archivo\": "; JEsc(s, nom);
    }
    // (el objeto de SCRIPT no tiene rama propia A PROPOSITO: cae por el 'else'
    //  generico del final y se guarda como "objeto" + CamposComunes + sus scripts.
    //  Antes escribia un bloque "fisica" con velocidad / gravedad / potenciaSalto /
    //  velocidadMaximaCaida y cuatro limites de mundo: vocabulario de UN genero de
    //  juego horneado en el formato de guardado. Y ademas ningun lector lo consumia
    //  -- los dos importadores lo descartan y migran el nodo a un Empty. Esos valores
    //  son datos del proyecto: viven en las propiedades del script, o sea la tabla
    //  `propiedades` del .lua o los `val_` del .w3d.)
    else if (t == ObjectType::camera) {
        Camera* c = (Camera*)o;
        JSang(s, ind + 1); s += "\"tipo\": \"camara\",\n";
        CamposComunes(s, ind + 1, o);
        // LENTE de la camara (fov + ortografica): propios de la camara, editables y animables
        s += ",\n"; JSang(s, ind + 1); s += "\"fov\": "; JNum(s, c->fov);
        s += ",\n"; JSang(s, ind + 1); s += "\"orto\": "; s += (c->orthographic ? "true" : "false");
        s += ",\n"; JSang(s, ind + 1); s += "\"near\": "; JNum(s, c->nearClip);
        s += ",\n"; JSang(s, ind + 1); s += "\"far\": "; JNum(s, c->farClip);
        // ENCUADRE DECLARADO: solo si la camara lo declara, asi un proyecto que no
        // lo usa guarda exactamente el mismo texto de siempre (ver Camera::aspecto).
        if (c->aspecto > 0.01f) { s += ",\n"; JSang(s, ind + 1); s += "\"aspecto\": "; JNum(s, c->aspecto); }
        if (c->target) { s += ",\n"; JSang(s, ind + 1); s += "\"target\": "; JEsc(s, c->target->name); }
        // RIEL de la camara (AUDIT versiones: el texto viejo lo traia y el JSON lo perdia):
        // referencia POR NOMBRE a una Curve de la escena + el offset sobre el riel
        {
            std::string riel = c->Riel ? c->Riel->name : c->RielName;
            if (!riel.empty()) {
                s += ",\n"; JSang(s, ind + 1); s += "\"riel\": "; JEsc(s, riel);
                s += ",\n"; JSang(s, ind + 1); s += "\"offsetRiel\": "; JNum(s, (float)c->offsetRiel);
                // MIRADA DEL RIEL: se escribe SOLO si esta prendida, asi un proyecto que
                // no la usa guarda exactamente el mismo texto de siempre.
                if (c->usarRotacionDelRiel) { s += ",\n"; JSang(s, ind + 1); s += "\"miradaRiel\": true"; }
            }
        }
    }
    else if (t == ObjectType::light) {
        Light* l = (Light*)o;
        JSang(s, ind + 1); s += "\"tipo\": \"luz\",\n";
        CamposComunes(s, ind + 1, o);
        s += ",\n"; JSang(s, ind + 1); s += "\"color\": [";
        JNum(s, l->diffuse[0]); s += ", "; JNum(s, l->diffuse[1]); s += ", "; JNum(s, l->diffuse[2]); s += "]";
    }
    else if (t == ObjectType::collection) {
        JSang(s, ind + 1); s += "\"tipo\": \"coleccion\",\n";
        // CamposComunes y no solo el nombre: una Collection oculta (ojito) o movida
        // PERDIA visible/pos/rot/escala al guardar y volvia visible al reabrir
        // ("oculto y se sigue viendo"). El lector siempre fue tolerante (JsonComunes
        // con defaults), asi que los archivos viejos abren igual.
        CamposComunes(s, ind + 1, o);
        // orden de dibujo para transparentes: solo se escribe si esta prendido.
        // El cast es seguro: aca solo llegan hijos de la escena (la raiz Scene no
        // pasa por aca).
        Collection* col = (Collection*)o;
        if (col->ordenarPorCamara) { s += ",\n"; JSang(s, ind + 1); s += "\"ordenarPorCamara\": true"; }
        if (col->ordenarUnaVez)    { s += ",\n"; JSang(s, ind + 1); s += "\"ordenarUnaVez\": true"; }
        // lote estatico (P4): solo se escribe si esta prendido (los .w3d que no lo
        // usan quedan byte a byte como antes)
        if (col->lote == 1)        { s += ",\n"; JSang(s, ind + 1); s += "\"lote\": \"estatico\""; }
    }
    else if (t == ObjectType::mirror) {
        // objeto ESPEJO (renderiza el reflejo de sus hijos/target): ejes + target por nombre
        Mirror* mi = (Mirror*)o;
        JSang(s, ind + 1); s += "\"tipo\": \"espejo\",\n";
        CamposComunes(s, ind + 1, o);
        s += ",\n"; JSang(s, ind + 1); s += "\"ejes\": [";
        s += mi->mirrorX ? "true" : "false"; s += ", ";
        s += mi->mirrorY ? "true" : "false"; s += ", ";
        s += mi->mirrorZ ? "true" : "false"; s += "]";
        {
            std::string tg = mi->target ? mi->target->name : mi->targetName;
            if (!tg.empty()) { s += ",\n"; JSang(s, ind + 1); s += "\"target\": "; JEsc(s, tg); }
            // MULTI-TARGET: la lista SOLO se escribe si hay mas de uno. Asi un
            // espejo de un solo target guarda exactamente el mismo JSON que
            // antes, y el archivo nuevo lo abre igual un lector que ignore
            // "targets" (se queda con "target", el primero de la lista).
            if (!mi->targetsExtraNombre.empty()) {
                s += ",\n"; JSang(s, ind + 1); s += "\"targets\": "; JEsc(s, mi->TargetsTexto());
            }
        }
        // recorte del reflejo (ver Mirror.h): solo se escriben si se usan, para
        // no ensuciar los espejos comunes. "limites" = [u0, u1, v0, v1], medidos
        // en el PLANO del espejo (para el horizontal sin rotar, U = X y V = Z, o
        // sea los mismos numeros de siempre: el formato no cambio un byte).
        if (mi->sinProfundidad) {
            s += ",\n"; JSang(s, ind + 1); s += "\"sinProfundidad\": true";
        }
        if (mi->recorteDeclarado) {
            s += ",\n"; JSang(s, ind + 1); s += "\"recorte\": "; s += mi->recorte ? "true" : "false";
        }
        // HUNDIMIENTO MAXIMO DEL REFLEJO (ver Mirror.h): 0 = espejo exacto, no se escribe.
        if (mi->hundimientoMaximo > 0.0f) {
            s += ",\n"; JSang(s, ind + 1); s += "\"hundimientoMaximo\": "; JNum(s, mi->hundimientoMaximo);
        }
        if (mi->usaLimites) {
            s += ",\n"; JSang(s, ind + 1); s += "\"limites\": [";
            JNum(s, mi->limU0); s += ", "; JNum(s, mi->limU1); s += ", ";
            JNum(s, mi->limV0); s += ", "; JNum(s, mi->limV1); s += "]";
        }
    }
    else if (t == ObjectType::instance) {
        // INSTANCE (duplicado enlazado / array de copias del target, opcionalmente espejado)
        Instance* in = (Instance*)o;
        JSang(s, ind + 1); s += "\"tipo\": \"instancia\",\n";
        CamposComunes(s, ind + 1, o);
        {
            std::string tg = in->target ? in->target->name : in->targetName;
            if (!tg.empty()) { s += ",\n"; JSang(s, ind + 1); s += "\"target\": "; JEsc(s, tg); }
        }
        s += ",\n"; JSang(s, ind + 1); s += "\"count\": "; JNum(s, (float)in->count);
        s += ",\n"; JSang(s, ind + 1); s += "\"espejado\": "; s += in->mirror ? "true" : "false";
        s += ",\n"; JSang(s, ind + 1); s += "\"espejoEje\": "; JNum(s, (float)in->mirrorEje);
    }
    else if (t == ObjectType::lod) {
        // LOD (un hijo por distancia a la camara): los umbrales, en orden (idioma del "ejes")
        LOD* l = (LOD*)o;
        JSang(s, ind + 1); s += "\"tipo\": \"lod\",\n";
        CamposComunes(s, ind + 1, o);
        s += ",\n"; JSang(s, ind + 1); s += "\"distancias\": [";
        for (size_t i = 0; i < l->distancias.size(); i++) {
            if (i) s += ", ";
            JNum(s, l->distancias[i]);
        }
        s += "]";
        // mismo flag que el Culling: solo se escribe si se usa, para que los proyectos
        // que no lo tocan queden byte a byte como antes
        if (l->soloCamaraActiva) { s += ",\n"; JSang(s, ind + 1); s += "\"soloCamaraActiva\": true"; }
    }
    else if (t == ObjectType::culling) {
        // Culling (frustum culling por AABB de los hijos)
        Culling* cu = (Culling*)o;
        JSang(s, ind + 1); s += "\"tipo\": \"culling\",\n";
        CamposComunes(s, ind + 1, o);
        // interruptor del recorte: solo se escribe si esta APAGADO, asi los .w3d que
        // no lo tocan quedan byte a byte como antes (el default al leer es true)
        if (!cu->activo) { s += ",\n"; JSang(s, ind + 1); s += "\"activo\": false"; }
        s += ",\n"; JSang(s, ind + 1); s += "\"soloCamaraActiva\": ";
        s += cu->soloCamaraActiva ? "true" : "false";
        // culling por DISTANCIA (0 = sin limite): solo se escribe si se usa, para
        // que los proyectos que no lo tocan queden byte a byte como antes
        if (cu->distanciaMax > 0.0f) {
            s += ",\n"; JSang(s, ind + 1); s += "\"distanciaMax\": "; JNum(s, cu->distanciaMax);
        }
    }
    else if (t == ObjectType::viszona) {
        // VISZONA (celda de visibilidad): todas sus propiedades, siempre (tipo nuevo:
        // no hay archivos viejos que cuidar byte a byte). objetivo/ancla viajan por
        // NOMBRE (se resuelven en la escena al usarse, como los targets).
        VisZona* vz = (VisZona*)o;
        JSang(s, ind + 1); s += "\"tipo\": \"viszona\",\n";
        CamposComunes(s, ind + 1, o);
        static const char* kModos[] = { "manual", "grilla", "volumenes", "curva" };
        int mi = (vz->modo >= 0 && vz->modo <= 3) ? vz->modo : 0;
        s += ",\n"; JSang(s, ind + 1); s += "\"modo\": "; JEsc(s, std::string(kModos[mi]));
        s += ",\n"; JSang(s, ind + 1); s += "\"objetivo\": "; JEsc(s, vz->objetivoNombre);
        s += ",\n"; JSang(s, ind + 1); s += "\"ancla\": ";    JEsc(s, vz->anclaNombre);
        s += ",\n"; JSang(s, ind + 1); s += "\"nx\": "; JNum(s, (float)vz->nx);
        s += ", \"ny\": "; JNum(s, (float)vz->ny);
        s += ", \"nz\": "; JNum(s, (float)vz->nz);
        s += ",\n"; JSang(s, ind + 1); s += "\"tamCelda\": "; JNum(s, vz->tamCelda);
        s += ",\n"; JSang(s, ind + 1); s += "\"pasoMax\": ";  JNum(s, (float)vz->pasoMax);
        // multi-riel (ver VisZona.h): solo si se usa, para que los proyectos que
        // no lo declaran queden byte a byte como antes
        if (!vz->rielNombre.empty()) {
            s += ",\n"; JSang(s, ind + 1); s += "\"riel\": "; JEsc(s, vz->rielNombre);
        }
        if (vz->celdaFallback != 0) {
            s += ",\n"; JSang(s, ind + 1); s += "\"fallback\": "; JNum(s, (float)vz->celdaFallback);
        }
    }
    else if (t == ObjectType::particulas) {
        // PARTICULAS (emisor de billboards del Core): todas sus propiedades, siempre
        // (tipo nuevo: no hay archivos viejos que cuidar byte a byte). La textura
        // entra al contenedor v4 como cualquier asset (texturas/).
        Particulas* pt = (Particulas*)o;
        JSang(s, ind + 1); s += "\"tipo\": \"particulas\",\n";
        CamposComunes(s, ind + 1, o);
        s += ",\n"; JSang(s, ind + 1); s += "\"textura\": ";
        JEsc(s, pt->textura.empty() ? std::string() : Asset(cx, pt->textura));
        s += ",\n"; JSang(s, ind + 1); s += "\"cantidad\": ";   JNum(s, pt->cantidad);
        s += ",\n"; JSang(s, ind + 1); s += "\"vida\": ";       JNum(s, pt->vida);
        s += ",\n"; JSang(s, ind + 1); s += "\"tam\": ";        JNum(s, pt->tam);
        s += ",\n"; JSang(s, ind + 1); s += "\"vel\": ";        JNum(s, pt->vel);
        s += ",\n"; JSang(s, ind + 1); s += "\"dispersion\": "; JNum(s, pt->dispersion);
        s += ",\n"; JSang(s, ind + 1); s += "\"gravedad\": ";   JNum(s, pt->gravedad);
        s += ",\n"; JSang(s, ind + 1); s += "\"aditivo\": ";    s += pt->aditivo ? "true" : "false";
        // solo se escribe si se usa: los proyectos que no la tocan quedan igual que antes
        if (pt->sustractivo) { s += ",\n"; JSang(s, ind + 1); s += "\"sustractivo\": true"; }
        s += ",\n"; JSang(s, ind + 1); s += "\"color\": [";
        JNum(s, pt->color[0]); s += ", "; JNum(s, pt->color[1]); s += ", ";
        JNum(s, pt->color[2]); s += ", "; JNum(s, pt->color[3]); s += "]";
        s += ",\n"; JSang(s, ind + 1); s += "\"desvanecer\": "; s += pt->desvanecer ? "true" : "false";
        s += ",\n"; JSang(s, ind + 1); s += "\"activo\": ";     s += pt->activo ? "true" : "false";
        s += ",\n"; JSang(s, ind + 1); s += "\"variacion\": ";   JNum(s, pt->variacion);
        s += ",\n"; JSang(s, ind + 1); s += "\"turbulencia\": "; JNum(s, pt->turbulencia);
        // rotacion del billboard: solo si se usa (los proyectos que no la tocan
        // quedan byte a byte como antes, mismo criterio que "sustractivo")
        if (pt->rotacion)            { s += ",\n"; JSang(s, ind + 1); s += "\"rotacion\": true"; }
        if (pt->velRotacion != 0.0f) { s += ",\n"; JSang(s, ind + 1); s += "\"velRotacion\": "; JNum(s, pt->velRotacion); }
    }
    // (aca vivia la rama del objeto Constraint VIEJO. Se dio de baja: un constraint es
    //  una propiedad del objeto y lo escribe EscribirConstraints, que corre para TODOS
    //  los tipos mas abajo. Los .w3d viejos con nodos "constraint" se siguen LEYENDO:
    //  el lector los migra al abrir, ver import_w3d.cpp)
    else if (t == ObjectType::curve) {
        // CURVE cargada de archivo (riel de camara): se referencia su archivo de
        // origen (la rama sin origen ya se conto arriba en gNoCubiertos)
        Curve* cv = (Curve*)o;
        JSang(s, ind + 1); s += "\"tipo\": \"curva\",\n";
        CamposComunes(s, ind + 1, o);
        // sin origen: "archivo" vacio (la curva vuelve VACIA, pero el nodo y su
        // subarbol vuelven). Asset() no se llama con "" (no hay nada que copiar).
        // v4: Curve::LoadFromFile usa std::ifstream (no pasa por el VFS), asi que su
        // .txt de origen NO puede vivir adentro del contenedor: queda EXTERNO
        if (!cv->origen.empty()) W3dRefExternaMarcar(cv->origen);
        s += ",\n"; JSang(s, ind + 1); s += "\"archivo\": ";
        JEsc(s, cv->origen.empty() ? std::string() : Asset(cx, cv->origen));
        // LISTA DE CARGA (streaming): el sidecar .cargas.json declarado. Solo
        // si hay (los .w3d sin streaming quedan byte a byte como siempre).
        // Externo por la misma razon que el .cap (se lee junto a el).
        if (!cv->cargasArchivo.empty()) {
            W3dRefExternaMarcar(cv->cargasArchivo);
            s += ",\n"; JSang(s, ind + 1); s += "\"cargas\": ";
            JEsc(s, Asset(cx, cv->cargasArchivo));
        }
    }
    else if (t == ObjectType::armature) {
        // ARMATURE (Fase 3): nombre/transform + huesos + clips de animacion + hijos (mallas del rig)
        Armature* a = (Armature*)o;
        JSang(s, ind + 1); s += "\"tipo\": \"armature\",\n";
        CamposComunes(s, ind + 1, o);
        EscribirArmature(s, a, ind + 1);
    }
    else if (t == ObjectType::mesh) {
        // ==================================================================
        //  UNA SOLA RAMA PARA TODAS LAS MALLAS: la geometria SE HORNEA en un
        //  .w3dm adentro del contenedor, venga de una primitiva del editor o de
        //  un .obj/.fbx importado.
        //
        //  Antes habia dos ramas y las dos tenian un agujero: la modelada
        //  exportaba un GLB (que triangula, re-splitea los vertices y no lleva
        //  ni colores ni capas UV extra ni costuras), y la IMPORTADA no
        //  guardaba geometria en absoluto -solo "origen" + la ruta al archivo
        //  del usuario-, asi que mover ese .obj de lugar dejaba la malla vacia
        //  y el proyecto NO era autocontenido.
        //
        //  Ahora la malla carga SIEMPRE de "geometria" (interna, siempre).
        //  "origen" queda para "reimportar desde el original" y para que el
        //  usuario sepa de donde salio: si falta, se avisa y NO pasa nada.
        // ==================================================================
        Mesh* m = (Mesh*)o;
        MatRegistrar(m);                 // sus materiales van al bloque raiz "materiales"
        std::string nomGeo = EscribirMallaW3dm(cx, m, o->name);
        JSang(s, ind + 1); s += "\"tipo\": \"malla\",\n";
        CamposComunes(s, ind + 1, o);
        s += ",\n"; JSang(s, ind + 1); s += "\"geometria\": "; JEsc(s, nomGeo);
        if (!m->origen.empty()) {
            // el .obj/.fbx del usuario lo edita OTRO programa: es suyo y queda EXTERNO
            // (listado en EXTERNOS.txt). Ya no es de donde carga la malla.
            W3dRefExternaMarcar(m->origen);
            // LOS SIDECARS DEL MODELO (<base>.pvs.json) SI ENTRAN AL CONTENEDOR.
            // Son datos derivados que ningun otro programa edita, no hay forma de
            // regenerarlos en la maquina de destino, y sin esto se perdian en
            // silencio al empaquetar: no viajaban ni figuraban en EXTERNOS.txt.
            IngerirSidecars(cx, m);
            s += ",\n"; JSang(s, ind + 1); s += "\"origen\": "; JEsc(s, Asset(cx, m->origen));
        }
        // ANIMACION UV "tira de atlas" (autoplay del Core, ver Mesh.h): 4 parametros
        // planos. El estado de reproduccion (cuadro/acum/base) NO se guarda a proposito.
        if (m->uvAnim) {
            s += ",\n"; JSang(s, ind + 1); s += "\"animUV\": { \"frames\": ";
            JNum(s, (float)m->uvAnim->frames);
            s += ", \"fps\": "; JNum(s, m->uvAnim->fps);
            s += ", \"eje\": "; JEsc(s, std::string(m->uvAnim->eje ? "v" : "u"));
            s += ", \"desfase\": "; JNum(s, (float)m->uvAnim->desfase);
            s += " }";
        }
        EscribirAnimsVertex(s, m, ind + 1, cx);   // vertex anims (frames binarios + curvas)
        EscribirRigMesh(s, m, ind + 1);           // modificador Armature (por nombre) + armatures 2D
        EscribirModificadores(s, m, ind + 1);     // el resto del stack (Mirror/Screw/SubSurf/...)
    }
    else {
        // tipo sin rama propia: al menos el nombre/transform (no rompe el abrir)
        JSang(s, ind + 1); s += "\"tipo\": \"objeto\",\n";
        CamposComunes(s, ind + 1, o);
    }

    // EL STACK DE CONSTRAINTS, DE TODOS LOS TIPOS, DE UNA SOLA VEZ. Va aca por el
    // MISMO motivo que EscribirHijos: es una propiedad de Object, no de una rama.
    //
    // *** Y NO CUELGA DE CamposComunes A PROPOSITO ***: la rama 'coleccion' NO la
    // llama (escribe tipo + nombre y nada mas), asi que los constraints de una
    // Coleccion se perderian al guardar sin un solo aviso. Es exactamente el fallo
    // que ya se pago una vez con los hijos.
    EscribirConstraints(s, o, ind + 1);

    // LOS SCRIPTS LUA, DE TODOS LOS TIPOS, DE UNA SOLA VEZ (estilo Unity: una
    // una malla, un vacio o una luz llevan scripts igual que el objeto Script).
    // Antes esto se llamaba SOLO en las ramas gamepad y malla: un script colgado
    // de cualquier otro tipo se perdia al guardar, sin aviso. Mismo criterio (y
    // mismo motivo) que EscribirConstraints/EscribirHijos. El lector espejo es
    // el call site unico de JsonScripts en JsonObjeto (import_w3d.cpp).
    EscribirScripts(s, o, ind + 1, cx);

    // LOS HIJOS, DE TODOS LOS TIPOS, DE UNA SOLA VEZ (ver EscribirHijos arriba).
    // Va al FINAL a proposito: para las ramas que YA lo llamaban (coleccion,
    // espejo, instancia, constraint, curva, armature) era su ultima linea, asi
    // que el .w3d sale byte a byte IGUAL que antes.
    EscribirHijos(s, o, ind + 1, cx);

    s += "\n"; JSang(s, ind); s += "}";
}

// ===========================================================================
//  ANIMACIONES DE ESCENA (bloque raiz "animaciones")
//
//  ESTO NO SE GUARDABA NUNCA. No es una regresion del contenedor v4 ni del
//  .w3dm: ni "AnimationObjects" ni "SceneAnimations" aparecian una sola vez en
//  este archivo, y el lector solo las LIMPIABA. O sea: el usuario animaba un
//  objeto con curvas (que es el camino POR DEFECTO del timeline, ActiveAnimKind
//  0), guardaba, reabria y la animacion no existia mas. Lo reporto el dueno con
//  estas palabras: "no vi que se guarde la animacion que hice con curvas".
//
//  DONDE VA, Y POR QUE ACA Y NO EN UN ARCHIVO PROPIO BAJO animaciones/:
//   - animaciones/ es la carpeta de los BLOBS BINARIOS de vertex anim (las
//     posiciones de cada cuadro). Esto es lo contrario: datos estructurados y
//     chicos, que el dueno tiene que poder LEER y EDITAR abriendo el zip.
//   - las curvas referencian objetos POR NOMBRE, y los objetos viven en el
//     proyecto.json. Partirlos en dos archivos crea un segundo archivo que hay
//     que mantener en sincronia y que puede faltar; el mismo archivo no puede
//     desincronizarse de si mismo.
//   - el resto de lo estructurado del proyecto (materiales, paletas, layout,
//     compilar) ya vive en la raiz del proyecto.json: esto es de la misma clase.
//
//  LA REFERENCIA AL OBJETO ES SU NOMBRE, CON SU SCOPE. Los nombres son unicos
//  POR ESCENA (W3dNombreScopeDe: la UI mas cercana que contiene al objeto, o el
//  arbol global), asi que el nombre solo NO alcanza si el objeto vive dentro de
//  una escena UI: ahi se escribe tambien "escena" (el nombre de esa UI) y al
//  abrir se busca DENTRO de ella. Sin "escena" se busca en el arbol global, que
//  es exactamente el scope que le corresponde. Es la misma regla que ya usa
//  "padre2d" para los huespedes de una UI.
//
//  EL INVARIANTE DEL SWAP: las curvas de la escena ACTIVA no viven en su
//  SceneAnimation sino en el global AnimationObjects (SetEscenaActiva swapea las
//  listas, ver Animation.cpp). Guardar leyendo esc->objetos a secas perderia
//  JUSTO la escena que el usuario esta viendo, que es el unico caso que reporta.
// ===========================================================================

// la lista de curvas de la escena 'si': la global si es la ACTIVA, la suya si no
static const std::vector<AnimationObject>& CurvasDeEscena(int si) {
    return (si == SceneAnimActiva) ? (const std::vector<AnimationObject>&)AnimationObjects
                                   : (const std::vector<AnimationObject>&)SceneAnimations[si]->objetos;
}

// una AnimationObject aporta algo solo si tiene AL MENOS un keyframe. Una curva sin
// keyframes la crea sola la evaluacion (PropertyDeLista) y tambien queda al borrar el
// ultimo key de un canal: escribirla engorda el archivo y ensucia el dope al reabrir.
// Mismo criterio que ya usan los clips del armature 2D.
static bool AnimObjTieneKeys(const AnimationObject& ao) {
    for (size_t p = 0; p < ao.Propertys.size(); p++)
        if (!ao.Propertys[p].keyframes.empty()) return true;
    return false;
}

// true si el estado de animaciones es EXACTAMENTE el que crea InitSceneAnimations
// (una escena "Scene", rango/fps default, sin una sola curva). En ese caso el bloque
// no se escribe: un proyecto sin animar sale byte a byte como antes de este cambio.
static bool AnimacionesSonElDefault() {
    if (SceneAnimations.size() != 1 || !SceneAnimations[0]) return false;
    const SceneAnimation* e = SceneAnimations[0];
    if (e->name != "Scene" || e->startFrame != 1 || e->endFrame != 250 || e->fps != 30) return false;
    const std::vector<AnimationObject>& objs = CurvasDeEscena(0);
    for (size_t i = 0; i < objs.size(); i++) if (AnimObjTieneKeys(objs[i])) return false;
    return true;
}

static void EscribirAnimacionesEscena(std::string& s) {
    InitSceneAnimations();   // idempotente: garantiza que exista "Scene"
    if (AnimacionesSonElDefault()) return;
    s += "  \"animaciones\": {\n";
    s += "    \"activa\": "; JInt(s, SceneAnimActiva); s += ",\n";
    s += "    \"escenas\": [\n";
    for (size_t si = 0; si < SceneAnimations.size(); si++) {
        const SceneAnimation* e = SceneAnimations[si];
        s += "      { \"nombre\": "; JEsc(s, e ? e->name : std::string("Scene"));
        s += ", \"inicio\": "; JInt(s, e ? e->startFrame : 1);
        s += ", \"fin\": ";    JInt(s, e ? e->endFrame : 250);
        s += ", \"fps\": ";    JInt(s, e ? e->fps : 30);
        const std::vector<AnimationObject>& objs = CurvasDeEscena((int)si);
        bool hayAlguno = false;
        for (size_t i = 0; i < objs.size() && !hayAlguno; i++)
            if (objs[i].obj && AnimObjTieneKeys(objs[i])) hayAlguno = true;
        if (hayAlguno) {
            s += ",\n        \"objetos\": [\n";
            bool primero = true;
            for (size_t i = 0; i < objs.size(); i++) {
                const AnimationObject& ao = objs[i];
                if (!ao.obj || !AnimObjTieneKeys(ao)) continue;
                if (!primero) s += ",\n";
                primero = false;
                s += "          { \"objeto\": "; JEsc(s, ao.obj->name);
                // SCOPE del nombre: si el objeto vive dentro de una escena UI, el nombre
                // solo es unico AHI ADENTRO -> se escribe de que UI se trata.
                Object* scope = W3dNombreScopeDe(ao.obj);
                if (scope && scope != SceneCollection) {
                    s += ", \"escena\": "; JEsc(s, scope->name);
                }
                s += ",\n            \"curvas\": [";
                bool primerCurva = true;
                for (size_t p = 0; p < ao.Propertys.size(); p++) {
                    if (ao.Propertys[p].keyframes.empty()) continue;   // curva sin keys: no es dato
                    if (!primerCurva) s += ",\n                        ";
                    primerCurva = false;
                    EscribirCurva(s, ao.Propertys[p]);
                }
                s += "] }";
            }
            s += "\n        ]";
        }
        s += " }";
        if (si + 1 < SceneAnimations.size()) s += ",";
        s += "\n";
    }
    s += "    ]\n";
    s += "  },\n";
}

// ===========================================================================
//  EL OBJETO SE GUARDA EN REPOSO, NO EN LA POSE DEL PLAYHEAD
//
//  Hermano exacto de W3dReposoVertexAnim (que hace lo mismo con la GEOMETRIA de
//  una malla, ver VertexAnimation.h). El playhead es estado de la SESION, no del
//  modelo: guardar con el cursor en el frame 50 tiene que dar el MISMO archivo
//  que guardar sin haberlo movido. Sin esto el .w3d salia con
//  {"nombre":"Cubo.001","pos":[...]} = la posicion donde lo dejo el scrub, o sea
//  la animacion HORNEADA encima del transform del objeto: reabrir y volver al
//  frame 1 daba un objeto que ya no estaba donde el usuario lo puso.
//
//  QUE TRANSFORM SE ESCRIBE: el del FRAME DE INICIO de la animacion que poso al
//  objeto (el 'startFrame' de SU animacion de escena, o el de su anim propia). Es el
//  analogo del "cuadro base" que usa W3dReposoVertexAnim, es INDEPENDIENTE del
//  playhead (que era todo el problema) y es la pose que el usuario ve al
//  rebobinar. Los canales SIN curva no se tocan: su valor no depende del frame.
//
//  NO ALCANZA CON pos/rot/escala: el scrub tambien pisa visible, el fov y los
//  clips de una camara y todos los colores/atenuaciones de una luz, y esos
//  campos tambien se escriben al JSON. Se restaura TODO lo que la evaluacion
//  toca, con los valores EXACTOS que habia (nada se re-evalua al salir: una
//  edicion manual hecha entre frames, que la animacion todavia no piso, no se
//  puede perder por haber guardado).
// ===========================================================================
namespace {
struct ReposoUno {
    Object*    o;
    Vector3    pos, scale, rotEuler;
    Quaternion rot;
    bool       visible, renderizable;
    // camara
    float fov, nearClip, farClip;
    // luz
    float dif[3], amb[3], spe[3];
    float attC, attL, attQ, spotCut, spotExp;
    GLenum lightID;
    bool  direccional;
};

class ReposoAnimObjetos {
    public:
        // QUE SE CUBRE YA NO DEPENDE DEL ActiveAnimKind (el kind solo decide el ORDEN, o sea
        // quien gana si dos fuentes animan el mismo canal). Antes SI: cubria solo el kind 0 (escena) y el
        // 3 (anim propia de la malla activa) y se iba con un 'return' en los demas, asi que
        // con el timeline en un clip de armature (1), en "Es un juego" (2) o en un clip de
        // armature 2D (4) el .w3d salia con la pose del scrub HORNEADA en el transform. La
        // pose no se borra al cambiar de kind: el objeto queda posado donde lo dejo el
        // playhead, guarde con el timeline donde guarde. Lo mismo con la animacion de escena
        // que deja de ser la ACTIVA (SetEscenaActiva hace swap de curvas, pero los objetos
        // que poso siguen posados) y con la malla que se scrubeo en kind 3 y ya no es la
        // ActiveAnimMesh.
        //
        // Entonces se cubre TODA fuente que pueda haber posado a alguien: las curvas de
        // TODAS las animaciones de escena (la activa y las que no) y las curvas propias de
        // TODAS las mallas. Lo que MANDA (se aplica ULTIMO, o sea que gana el canal
        // compartido) es la fuente que el playhead esta aplicando ahora: con kind 3 la anim
        // propia de la malla, en cualquier otro caso la escena activa. Restaurar sigue siendo
        // por SNAPSHOT (~ReposoAnimObjetos), asi que aparecer en varias listas no mueve nada.
        ReposoAnimObjetos() {
            const bool propiaManda = (ActiveAnimKind == 3);   // el playhead esta en la anim propia de una malla
            InitSceneAnimations();
            // 1) las animaciones de escena que NO son la activa: sus curvas viven en el
            //    SceneAnimation (el swap las guardo ahi), pero los objetos que posaron
            //    siguen posados.
            for (size_t si = 0; si < SceneAnimations.size(); si++) {
                if ((int)si == SceneAnimActiva || !SceneAnimations[si]) continue;
                std::vector<AnimationObject>& objs = SceneAnimations[si]->objetos;
                const int base = SceneAnimations[si]->startFrame;
                for (size_t i = 0; i < objs.size(); i++)
                    Aplicar(objs[i].obj, objs[i].Propertys, base);
            }
            if (propiaManda) { EscenaActiva(); Mallas(); }
            else             { Mallas(); EscenaActiva(); }
        }
        ~ReposoAnimObjetos() {
            // al reves de como se guardaron: si un objeto entro dos veces (no deberia,
            // pero un archivo raro puede duplicarlo) gana el estado ORIGINAL
            for (size_t i = previos.size(); i-- > 0; ) {
                const ReposoUno& g = previos[i];
                Object* o = g.o;
                if (!o) continue;
                o->pos = g.pos; o->scale = g.scale; o->SetRotSnapshot(g.rot, g.rotEuler);
                o->visible = g.visible; o->renderizable = g.renderizable;
                if (o->getType() == ObjectType::camera) {
                    Camera* c = (Camera*)o;
                    c->fov = g.fov; c->nearClip = g.nearClip; c->farClip = g.farClip;
                } else if (o->getType() == ObjectType::light) {
                    Light* l = (Light*)o;
                    for (int k = 0; k < 3; k++) { l->diffuse[k] = g.dif[k]; l->ambient[k] = g.amb[k]; l->specular[k] = g.spe[k]; }
                    l->attConstant = g.attC; l->attLinear = g.attL; l->attQuadratic = g.attQ;
                    l->spotCutoff = g.spotCut; l->spotExponent = g.spotExp;
                    l->SetLightID(g.lightID);
                    l->direccional = g.direccional;
                }
            }
        }
    private:
        // las curvas de la animacion de escena ACTIVA: viven en el global AnimationObjects
        // (invariante del swap, ver Animation.h), no en su SceneAnimation.
        void EscenaActiva() {
            if (SceneAnimActiva < 0 || SceneAnimActiva >= (int)SceneAnimations.size()) return;
            if (!SceneAnimations[SceneAnimActiva]) return;
            const int base = SceneAnimations[SceneAnimActiva]->startFrame;
            for (size_t i = 0; i < AnimationObjects.size(); i++)
                Aplicar(AnimationObjects[i].obj, AnimationObjects[i].Propertys, base);
        }
        // la anim PROPIA (kind 3) de UNA malla: su reposo es el inicio de ESA anim
        void Propia(Object* o) {
            if (!o || o->getType() != ObjectType::mesh) return;
            Mesh* m = (Mesh*)o;
            VertexAnimationActive* va = FindTargetAnim(m);
            const int idx = va ? va->currentAnim : -1;
            if (idx < 0 || idx >= (int)m->animations.size() || !m->animations[idx]) return;
            VertexAnimation* an = m->animations[idx];
            Aplicar(o, an->curvas, an->startFrame);
        }
        void Recorrer(Object* o, Object* saltar) {
            if (!o) return;
            if (o != saltar) Propia(o);
            for (size_t i = 0; i < o->Childrens.size(); i++) Recorrer(o->Childrens[i], saltar);
        }
        // TODAS las mallas, con la del timeline (kind 3) al final: es la que manda si el
        // playhead esta parado ahi. Una malla sin curvas propias no cuesta nada (Aplicar
        // se va sin tocar nada si no hay keyframes).
        void Mallas() {
            Recorrer(SceneCollection, (Object*)ActiveAnimMesh);
            if (ActiveAnimMesh) Propia((Object*)ActiveAnimMesh);
        }
        void Aplicar(Object* o, std::vector<AnimProperty>& props, int frameBase) {
            if (!o) return;
            // SOLO lo que el playhead poso DE VERDAD (Object::posadoPorCurvas, que prende
            // W3dAplicarCurvasEnFrame). "Tener curvas" no alcanza: una malla con curvas propias
            // que el timeline nunca aplico (no estaba en su animacion) se mueve A MANO, y
            // re-posarla aca escribia el cuadro base y PERDIA el movimiento del usuario, en
            // silencio y en el flujo mas comun (abrir el proyecto, arrastrar la malla, Ctrl+S).
            if (!o->posadoPorCurvas) return;
            bool hay = false;
            for (size_t p = 0; p < props.size() && !hay; p++)
                if (!props[p].keyframes.empty()) hay = true;
            if (!hay) return;   // sin keyframes no hay pose que deshacer (caso normal: gratis)
            ReposoUno g;
            g.o = o;
            o->ActualizarDisplayRot();
            g.pos = o->pos; g.scale = o->scale; g.rot = o->Rot(); g.rotEuler = o->rotEuler;
            g.visible = o->visible; g.renderizable = o->renderizable;
            g.fov = g.nearClip = g.farClip = 0.0f;
            for (int k = 0; k < 3; k++) { g.dif[k] = g.amb[k] = g.spe[k] = 0.0f; }
            g.attC = g.attL = g.attQ = g.spotCut = g.spotExp = 0.0f;
            g.lightID = GL_LIGHT0; g.direccional = false;
            if (o->getType() == ObjectType::camera) {
                Camera* c = (Camera*)o;
                g.fov = c->fov; g.nearClip = c->nearClip; g.farClip = c->farClip;
            } else if (o->getType() == ObjectType::light) {
                Light* l = (Light*)o;
                for (int k = 0; k < 3; k++) { g.dif[k] = l->diffuse[k]; g.amb[k] = l->ambient[k]; g.spe[k] = l->specular[k]; }
                g.attC = l->attConstant; g.attL = l->attLinear; g.attQ = l->attQuadratic;
                g.spotCut = l->spotCutoff; g.spotExp = l->spotExponent;
                g.lightID = l->LightID; g.direccional = l->direccional;
            }
            previos.push_back(g);
            W3dAplicarCurvasEnFrame(o, props, frameBase);
        }
        std::vector<ReposoUno> previos;
        ReposoAnimObjetos(const ReposoAnimObjetos&);              // sin copia (C++03)
        ReposoAnimObjetos& operator=(const ReposoAnimObjetos&);
};
} // namespace

// ---------------------------------------------------------------------------
//  guardar el proyecto completo (v3: el .w3d ES el JSON, plano y editable)
// ---------------------------------------------------------------------------
bool GuardarW3D(const std::string& ruta) {
    if (!SceneCollection) return false;
    // ---- FRENO: UNA ESCENA UI DEL PROYECTO NO CARGO --------------------------------------
    //  El .w3dui faltaba o no parseaba, asi que su nodo NO se creo. Guardar escribe un
    //  proyecto.json SIN esa escena: los bytes del .w3dui sobreviven (PreservarPasajeras) pero
    //  ya no los referencia nadie, o sea que el usuario pierde la escena de vista y no se
    //  entera. Mismo criterio que una malla o una vertex anim que no cargo: no se guarda
    //  encima y se dice por que. Reabrir el proyecto arreglado limpia el freno.
    if (!g_w3dUINoCargo.empty()) {
        w3dLogfE("[W3D] la escena UI '%s' no cargo al abrir: NO guardo encima (se perderia del proyecto)",
                 g_w3dUINoCargo.c_str());
        W3dAvisof(true, "No guardo: la escena UI '%s' no se pudo leer al abrir y guardar la sacaria del proyecto",
                  W3dNombreCorto(g_w3dUINoCargo).c_str());
        return false;
    }
    gNoCubiertos = 0;
    gMats.clear();   // los materiales se juntan durante el recorrido de la escena

    // ------------------------------------------------------------------
    //  FORMATO v4: TODO el proyecto adentro de UN archivo (un zip estandar).
    //  Se escribe SIEMPRE asi; los formatos viejos (JSON plano v3, zip v2 y el
    //  texto Whisk3D{}) se siguen ABRIENDO y se MIGRAN en este mismo guardado.
    // ------------------------------------------------------------------
    W3dContenedorEscritor esc;
    esc.Iniciar(ruta, W3dContenedorLector());
    gEsc = &esc;
    gQuien.clear();
    g_w3dRefEmit = W3dRefEmitir;      // el .w3dui emite nombres de entrada (ver UI2DFormato.h)
    g_w3dDirProyecto = Carpeta(ruta);

    CtxGuardar cx;
    cx.esc = &esc;
    cx.dirW3d = Carpeta(ruta);
    cx.vtxN = 0;
    cx.error = false;
    MkdirRec(cx.dirW3d);   // "guardar como" a una carpeta que todavia no existe

    std::string s;
    s += "{\n";
    // "version" es el esquema de DATOS; "contenedor.formato" es la version del
    // LAYOUT DE CARPETAS. Separados a proposito: un cambio de esquema no obliga a
    // mover archivos y al reves. PROHIBIDO meter aca fecha, usuario, hostname o
    // rutas de la maquina: cualquier campo volatil rompe el round-trip byte a byte.
    s += "  \"version\": 4,\n";
    // formato 2 = el contenedor estilo OpenDocument (mimetype primero + LEEME.txt)
    s += "  \"contenedor\": { \"formato\": 2, \"generador\": \"Whisk3D\" },\n";
    // ICONO del juego: entra al contenedor bajo proyecto/ (el PNG en maxima
    // definicion; Compilar juego genera de ahi los tamanos chicos)
    if (!g_proyIcono.empty()) {
        gQuien = "icono del proyecto";
        // EL ARTE FUENTE DEL ICONO VIAJA CON EL: al lado de icono.png suele haber un
        // icono.svg que NO lo referencia NADIE (es de donde sale el png). Una
        // migracion que solo camine las referencias lo perderia de vista y el dueno
        // se enteraria el dia que quiera reeditar el icono. Se traen los hermanos
        // con el MISMO nombre y otra extension, y solo eso: nada de arrastrar la
        // carpeta entera. Al re-guardar ya viajan como entradas preservadas.
        std::string carpetaIco = Carpeta(g_proyIcono);
        std::string baseIco    = BaseSinExt(g_proyIcono);
        bool eraDeDisco = !W3dEsNombreDeEntrada(g_proyIcono) &&
                          w3dFileSystem::FileExists(g_proyIcono);
        std::string ref = esc.Ingerir(g_proyIcono, "proyecto", gQuien);
        if (eraDeDisco && ref.compare(0, 4, "ext:") != 0) {
            std::vector<w3dFileSystem::DirEntry> ents;
            if (!baseIco.empty() && w3dFileSystem::ListDir(carpetaIco, ents))
                for (size_t i = 0; i < ents.size(); i++) {
                    if (ents[i].isDir) continue;
                    std::string n = ents[i].name;
                    size_t pp = n.find_last_of('.');
                    if (pp == std::string::npos || n.substr(0, pp) != baseIco) continue;
                    std::string hermano = carpetaIco + "/" + n;
                    if (hermano == g_proyIcono) continue;
                    std::string tmp = hermano;
                    esc.Ingerir(tmp, "proyecto", "arte fuente del icono");
                }
        }
        s += "  \"icono\": "; JEsc(s, ref); s += ",\n";
    }
    // MULTI-ESCENA: la escena que ARRANCA + el modo (antes NO se guardaban en el
    // v2 zip y se perdian al re-guardar un proyecto de texto viejo)
    if (!W3dEscenaInicial().empty()) {
        s += "  \"escenaInicial\": "; JEsc(s, W3dEscenaInicial()); s += ",\n";
    }
    if (W3dEscenaModo()) s += "  \"modoEscenas\": true,\n";
    // PIXELADO GLOBAL: solo se escribe si esta PRENDIDO (los .w3d que ya existen
    // no cambian ni un byte al re-guardarlos).
    if (w3dEngine::PixeladoGlobal()) s += "  \"pixelado\": true,\n";
    // CONFIG de la tarjeta Juego (Compilar juego): los valores VIGENTES del
    // editor, con strings legibles para editarlos a mano. Un .w3d viejo sin el
    // bloque abre con los defaults (W3dCompilarReset, ver import_w3d).
    s += "  \"compilar\": {\n";
    s += "    \"modoVentana\": "; JEsc(s, W3dCompilarModoVentanaStr(g_proyCompilar.modoVentana)); s += ",\n";
    s += "    \"orientacion\": "; JEsc(s, W3dCompilarOrientacionStr(g_proyCompilar.orientacion)); s += ",\n";
    s += "    \"fisica\": ";  s += g_proyCompilar.usarFisica ? "true" : "false"; s += ",\n";
    s += "    \"sonido\": ";  s += g_proyCompilar.usarSonido ? "true" : "false"; s += ",\n";
    { char volbuf[16]; snprintf(volbuf, sizeof(volbuf), "%d", g_proyCompilar.volumen);
      s += "    \"volumen\": "; s += volbuf; s += ",\n"; }   // 0..100 volumen del gameplay
    s += "    \"debug\": ";   s += g_proyCompilar.modoDebug  ? "true" : "false"; s += ",\n";
    s += "    \"assets\": ";  JEsc(s, W3dCompilarAssetsStr(g_proyCompilar.assetsModo)); s += ",\n";
    s += "    \"plataforma\": "; JEsc(s, W3dCompilarPlataformaStr(g_proyCompilar.plataforma)); s += ",\n";
    { char uidhex[16]; snprintf(uidhex, sizeof(uidhex), "0x%08X", g_proyCompilar.uid);
      s += "    \"uid\": "; JEsc(s, uidhex); s += "\n"; }   // UID3 de Symbian del juego (0 = sin asignar)
    s += "  },\n";
    // PALETAS del proyecto (v3): la fuente de verdad. Ademas se BAKEAN dentro
    // de cada .w3dui (UI2DGuardar) para el runtime standalone.
    {
        std::vector<Paleta>& ps = W3dPaletas();
        if (!ps.empty()) {
            s += "  \"paletas\": [\n";
            for (size_t i = 0; i < ps.size(); i++) {
                s += "    { \"nombre\": "; JEsc(s, ps[i].nombre);
                s += ", \"colores\": [\n";
                for (size_t c = 0; c < ps[i].colores.size(); c++) {
                    const PaletaColor& pc = ps[i].colores[c];
                    s += "      { \"nombre\": "; JEsc(s, pc.nombre);
                    s += ", \"color\": [";
                    JNum(s, pc.rgba[0]); s += ", "; JNum(s, pc.rgba[1]); s += ", ";
                    JNum(s, pc.rgba[2]); s += ", "; JNum(s, pc.rgba[3]);
                    s += "] }";
                    if (c + 1 < ps[i].colores.size()) s += ",";
                    s += "\n";
                }
                s += "    ] }";
                if (i + 1 < ps.size()) s += ",";
                s += "\n";
            }
            s += "  ],\n";
        }
    }
    s += "  \"fps\": "; JNum(s, (float)AnimFPS); s += ",\n";
    s += "  \"fullscreen\": "; s += cfg.fullscreen ? "true" : "false"; s += ",\n";
    // estado de REPRODUCCION al guardar (v3): reabrir el proyecto respeta si estaba
    // en PLAY o en PAUSA (guardado en pausa -> abre en pausa). En archivos viejos el
    // campo no existe y AbrirW3D cae al auto-play de siempre (retrocompat).
    s += "  \"reproduciendo\": "; s += PlayAnimation ? "true" : "false"; s += ",\n";
    s += "  \"escena\": {\n";
    s += "    \"objetos\": [\n";
    {
        // EL PLAYHEAD NO SE HORNEA: mientras dura el recorrido de la escena, todo
        // objeto animado vuelve a su cuadro base y al cerrar el scope recupera su
        // pose EXACTA (ver el comentario grande de ReposoAnimObjetos).
        ReposoAnimObjetos reposo;
        bool primero = true;
        for (size_t i = 0; i < SceneCollection->Childrens.size(); i++)
            EscribirObjeto(s, SceneCollection->Childrens[i], 3, &cx, &primero);
    }
    s += "\n    ]\n";
    s += "  },\n";
    // ANIMACIONES DE ESCENA (las curvas de transform de los objetos): van DESPUES de
    // la escena porque referencian a los objetos por nombre y asi se leen en orden.
    EscribirAnimacionesEscena(s);
    // MATERIALES: van DESPUES de la escena porque la lista se arma recorriendola (orden de
    // primera aparicion = deterministico). En el JSON el orden de las claves no significa nada:
    // el lector los resuelve por nombre, y los carga ANTES de armar los objetos.
    EscribirMateriales(s, &cx);
    // SESION: donde estaba trabajando el usuario EN ESTE proyecto. Va DESPUES de la escena
    // porque nombra objetos (ver el comentario grande de EscribirSesion).
    EscribirSesion(s);
    // LAYOUT: el arbol VIVO (tipos + splits). Los .w3d guardados con el literal
    // "2d" siguen abriendo por el template estandar (ver AplicarLayoutTexto).
    {
        std::string lay = LayoutSerializarVivo();
        s += "  \"layout\": ";
        JEsc(s, lay.empty() ? std::string("2d") : lay);
        s += "\n";
    }
    s += "}\n";

    g_w3dRefEmit = NULL;      // de aca en adelante ya no se ingiere nada
    gEsc = NULL;

    // ==================================================================
    //  ESCRITURA DEL CONTENEDOR v4: UN SOLO ARCHIVO, UN SOLO RENAME
    //
    //  1. armar el zip ENTERO en "<destino>.w3dtmp", en la MISMA carpeta
    //  2. VERIFICAR que toda referencia interna tenga su entrada. Si falta
    //     una sola: ABORTAR sin renombrar (sin esto el proyecto abre "sin
    //     la textura" y no falla nada, que es el fallo mas caro del diseno)
    //  3. rename encima. Si algo falla, el archivo ANTERIOR queda INTACTO.
    //
    //  EL CICLO DEL ARCHIVO ABIERTO: el contenedor mantiene su FILE* abierto
    //  toda la sesion para leer por demanda. En POSIX el rename sobre un
    //  archivo abierto anda; en Windows falla. Por eso el orden es
    //  cerrar el zip nuevo -> DESMONTAR -> rename -> volver a montar.
    // ==================================================================
    {
        std::string tmpZip = ruta + kTmpSufijo;
        bool ok = !cx.error;
        if (ok && !esc.AgregarBytes("proyecto.json", s)) ok = false;
        // estilo OpenDocument: "mimetype" (que Escribir() manda primera) y el
        // LEEME.txt que le explica el arbol al que abra el zip con un descompresor
        if (ok) esc.EscribirCabeceraOdf();
        // lo que el editor no referencia y venia en el .w3d se preserva VERBATIM
        // (alguien pudo meter un notas.txt a mano con un descompresor)
        if (ok) esc.PreservarPasajeras();
        if (ok) esc.EscribirExternos();
        std::string falta;
        if (ok && !esc.Verificar(falta)) {
            w3dLogfE("[W3D] el guardado se ABORTA: la referencia interna '%s' no tiene entrada en el .w3d",
                     falta.c_str());
            Notificar("Guardar ABORTADO: falta '" + falta + "' adentro del proyecto; NO se toco " + Base(ruta), true);
            ok = false;
        }
        // el harness inyecta aca el disco lleno (comando 'saveatom')
        if (ok && g_w3dFallarEscritura) {
            FILE* fh = fopen(tmpZip.c_str(), "wb");
            if (fh) { fwrite(s.c_str(), 1, s.size() / 2, fh); fclose(fh); }
            ok = false;
        }
        if (ok) ok = esc.Escribir(tmpZip);
        esc.LimpiarTemporales();
        if (!ok) {
            remove(tmpZip.c_str());
            if (falta.empty())
                Notificar("Guardar: escritura incompleta; NO se toco " + Base(ruta), true);
            return false;
        }
        // el contenedor viejo tiene el archivo destino ABIERTO: soltarlo antes del rename
        bool estabaMontado = W3dContenedorHayMontado();
        W3dContenedorDesmontar();
        if (!RenombrarSobre(tmpZip, ruta)) {
            remove(tmpZip.c_str());
            if (estabaMontado) W3dContenedorMontar(w3dPath);   // volver a lo que habia
            Notificar("Guardar: no pude reemplazar " + Base(ruta) + " (quedo la version anterior)", true);
            return false;
        }
        // y se monta el que acabamos de escribir: de aca en adelante las rutas en
        // memoria son sus nombres de entrada y ReadFileBytes las resuelve
        if (!W3dContenedorMontar(ruta))
            w3dLogfE("[W3D] guarde %s pero no lo pude volver a montar", ruta.c_str());
        size_t nExt = esc.CantidadExternos(), nFaltan = esc.CantidadExternosQueFaltan();
        char b[256];
        if (nFaltan > 0) {
            // EL AVISO TIENE QUE DECIR COMO APAGARLO. El caso comun de lejos es el .obj/.fbx
            // del que se importo una malla: la geometria ya viaja horneada adentro, o sea que
            // ese renglon es SOLO procedencia y el aviso, sin salida, era ruido para siempre.
            snprintf(b, sizeof(b), "Guardado, pero %d archivo(s) externo(s) NO estan (ver EXTERNOS.txt). "
                     "Si es el original de una malla importada: Object > Set Origin > Clear Original File",
                     (int)nFaltan);
            Notificar(b, true);
        } else if (nExt > 0) {
            snprintf(b, sizeof(b), "Proyecto guardado: %s (%d referencia(s) externa(s), ver EXTERNOS.txt)",
                     Base(ruta).c_str(), (int)nExt);
            Notificar(b, false);
        } else if (gNoCubiertos > 0) {
            snprintf(b, sizeof(b), "Guardado (ojo: %d curva(s) sin archivo de origen no se guardan)",
                     gNoCubiertos);
            Notificar(b, true);
        } else {
            Notificar("Proyecto guardado: " + Base(ruta), false);
        }
        w3dLogf("[W3D] guardado %s (contenedor v4 estilo ODF: mimetype + LEEME.txt + "
                "proyecto.json + escenas/ + scripts/ + assets)", ruta.c_str());
        return true;
    }
}

// nombre sugerido del .w3d cuando el explorador devuelve una CARPETA: lo tipeado en
// la tarjeta Archivo ("Nombre"), sino el nombre del proyecto abierto, sino sin_titulo.
static std::string NombreProyectoSugerido() {
    extern std::string ProyectoNombreCampo(); // Properties.cpp (tarjeta Archivo)
    std::string n = ProyectoNombreCampo();
    if (n.empty() && !w3dPath.empty()) n = Base(w3dPath);
    if (n.empty()) n = "sin_titulo";
    return n;
}

// El destino elegido, entre el explorador y la confirmacion de sobrescritura
// (ConfirmarPopup::onSi no lleva argumentos, mismo patron que Render/Export).
static std::string g_guardarPendiente;

static void GuardarPendienteAhora() {
    if (g_guardarPendiente.empty()) return;
    std::string r = g_guardarPendiente;
    if (GuardarW3D(r)) {
        w3dPath = r;   // el proximo Ctrl+S va directo aca
        extern void ProyectoSincronizarCampos();
        ProyectoSincronizarCampos();
    }
}

// El explorador (modo elegir carpeta) devuelve una CARPETA ("Usar carpeta actual") o un
// .w3d existente (sobrescribir). W3dRutaDeSalida arma la ruta final en los dos casos:
// antes se le pegaba ".w3d" a la carpeta y salia "/ruta/Documentos.w3d".
// Si el .w3d de destino YA EXISTE se pide confirmacion antes de pisarlo (antes se
// escribia encima sin decir nada; Render y Export si preguntan).
static void GuardarElegido(const std::string& elegido) {
    g_guardarPendiente = W3dRutaDeSalida(elegido, NombreProyectoSugerido(), ".w3d");
    if (w3dFileSystem::FileExists(g_guardarPendiente)) {
        if (!confirmarPopup) confirmarPopup = new ConfirmarPopup();
        confirmarPopup->Abrir("The file \"" + Base(g_guardarPendiente) +
                              "\" already exists. Do you want to replace it?", GuardarPendienteAhora);
        return;
    }
    GuardarPendienteAhora();
}

void GuardarProyecto() {
    if (!w3dPath.empty()) { GuardarW3D(w3dPath); return; }
    AbrirFileBrowser("Guardar proyecto", "Guardar", ".w3d", GuardarElegido, true /*guardar*/);
}

void GuardarProyectoComo() {
    AbrirFileBrowser("Guardar proyecto como", "Guardar", ".w3d", GuardarElegido, true /*guardar*/);
}
