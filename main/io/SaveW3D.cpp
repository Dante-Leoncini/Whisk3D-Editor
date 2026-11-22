// ============================================================================
//  SaveW3D.cpp — ver SaveW3D.h.
//
//  FORMATO v4: el .w3d es un CONTENEDOR (zip) con TODO el proyecto adentro. El
//  JSON no cambio: es el mismo esquema, la misma indentacion, los mismos nombres
//  de campo y el mismo orden que el v3, con dos lineas nuevas ("version": 4 y el
//  bloque "contenedor") y las rutas reescritas como NOMBRES DE ENTRADA. Un
//  project.json extraido del zip se edita con cualquier editor de texto, se
//  vuelve a meter y abre: la propiedad que gano el dueno cuando pidio "que los
//  w3d sean json standard" se conserva entera.
//
//  ARMADO EN DOS FASES:
//    1) se camina la escena escribiendo el JSON, y cada ruta de asset pasa por
//       W3dContainerWriter::Ingerir, que mete el archivo adentro y devuelve su
//       nombre de entrada. Los .w3dui y los .glb salen a un TEMPORAL en la
//       carpeta del destino (UI2DSave solo sabe escribir a un
//       path) y de ahi entran al zip; el temporal se borra siempre.
//    2) se verifica que toda referencia interna tenga entrada, se vuelca el zip
//       al .w3dtmp en orden determinista y se hace UN rename.
//
//  LA GEOMETRIA VA EN .w3dm (formato propio, libs/Whisk3DCore/io/W3dMesh.h) bajo meshes/. El GLB
//  dejo de ser formato de GUARDADO: queda SOLO para importar y para el "Export to..." del usuario.
//  Con eso se fueron tres parches que existian PORQUE el GLB triangula y re-splitea los vertices:
//    - el bloque "topologia" del JSON (los quads/ngons ahora son NATIVOS del .w3dm);
//    - los "vgroups"/"uvgroups" por CLAVE GEOMETRICA (ahora los INDICES son el dato: VG y UVG);
//    - el swap manual de uv2dRest antes de exportar (ahora el rest viaja en el bloque UVREST).
//  Y ademas dejaron de perderse las capas de color, la segunda capa UV, las costuras, los bordes
//  marcados, las normales del usuario y la geometria suelta.
//
//  MATERIALES: como viajaban ADENTRO del GLB, ahora tienen su propio bloque raiz "materiales" del
//  project.json (referenciado POR NOMBRE desde el bloque PARTMAT de cada .w3dm). Las texturas
//  entran al contenedor como cualquier otro asset (textures/).
//
//  RETROCOMPAT: un .w3d con meshes/*.glb (con o sin bloque "topologia") se sigue ABRIENDO por el
//  camino de siempre y queda convertido a .w3dm AL GUARDAR. Lo que ya se habia perdido no se
//  resucita (no existe en el .glb): la migracion corta la perdida hacia adelante.
// ============================================================================
#include "io/W3dWeb.h"   // navegador: guardar = descargar
#include "W3dLang.h"   // T(): los avisos salen en el idioma del editor
#include "io/SaveW3D.h"
#include "io/JsonW3d.h"                // JsonNumText: floats con round-trip EXACTO
#include "io/UI2DFormat.h"
#include "io/W3dContainer.h"          // FORMATO v4: el .w3d ES un zip y todo va adentro
#include "io/W3dZip.h"                // el contenedor VIEJO (las entradas de escenas/prefabs que ya no son de nadie)
#include "io/EditedTexture.h"   // texturas generadas/pintadas que solo viven en memoria: entran al zip
#include "W3dScene.h"                // escenaInicial / modoEscenas (se guardan con el proyecto)
#include "W3dRoots.h"                // las ESCENAS 3D, los JUEGOS y los PREFABS: una entrada por raiz (.w3de / .w3dp)
#include "io/EditorRoots.h"          // el estado base de un juego que se guarda editando uno de sus clips
#include "W3dPalettes.h"             // las paletas del PROYECTO (raiz "paletas" del .w3d)
#include "objects/Objects.h"
#include "objects/Empty.h"          // el dibujo del Vacio (forma, tamanio, giro)
#include "objects/ObjectMode.h"        // W3dApplyCurvesInFrame: el objeto se guarda EN REPOSO
#include "physics/W3dRigid.h"          // W3dRigidDef: el bloque "fisica" del objeto
#include "objects/UI.h"
#include "objects/Camera.h"
#include <errno.h>
#include "objects/Light.h"
#include "objects/Gamepad.h"
#include "objects/Mesh.h"
#include "objects/Mirror.h"              // AUDIT versiones: el objeto Espejo ahora SI se guarda
#include "objects/Instance.h"            // idem Instance (duplicado enlazado / array de copias)
#include "objects/LOD.h"                 // objeto LOD (un hijo por distancia): distancias
#include "objects/Culling.h"             // objeto Culling (frustum culling): onlyCameraActive
#include "objects/Routine.h"             // objeto Rutina (lista de pasos del render)
#include "objects/Fog.h"                 // objeto Niebla (glFog en el arbol)
#include "objects/Sound.h"               // objeto Sonido (audio puesto en la escena)
#include "objects/ClipRegion.h"          // ClearZ / Recorte
#include "objects/Hitbox.h"              // HITBOX (sus campos los escribe objects/Hitbox.cpp)
#include "objects/LightProbe.h"   // la sonda de luz (sus campos)
#include "objects/ReflectionProbe.h"   // la sonda de reflejo (sus campos)
#include "objects/InstancePrefab.h"   // INSTANCIA DE PREFAB: se escribe el nodo (prefab + overrides), NO lo que genera
#include "objects/ProxyW3d.h"         // PROXY W3D: el nodo (libreria + elemento + overrides); lo que genera es de la libreria
#include "io/Libraries.h"             // las referencias a una LIBRERIA que no esta (se escriben tal cual)
#include "io/Prefabs.h"                // la definicion EN MEMORIA de un prefab (W3dPrefabSerializeHook)
#include "objects/Particles.h"         // objeto Particulas (emisor): textura + config del cono
#include "objects/VisZone.h"           // objeto VisZone (celda de visibilidad): modo + grilla + refs
#include "objects/Collection.h"        // Collection: sortByCamera/sortOnce (transparentes)
#include "objects/Curve.h"             // idem Curve (riel de camara; se guarda su archivo de origen)
#include "objects/Armature.h"          // los ARMATURES ahora SI se guardan (Fase 3)
#include "animation/SkeletalAnimation.h" // clips del esqueleto (tracks/curvas por hueso)
#include "animation/W3dAnimSet.h"        // la raiz de clips de jerarquia de un objeto: su retarget y sus capas
#include "io/SaveAnimSets.h"       // los clips van a ANIMSETS binarios (.w3da), deduplicados por clip
#include "animation/Armature2DAnimation.h" // clips del armature 2D (tracks/curvas por hueso 2D)
#include "edit/Modifier.h"             // modificador Armature del mesh (referencia por nombre)
#include "animation/VertexAnimation.h"
#include "script/W3dScript.h"
#include "io/W3dMesh.h"                // .w3dm: el formato de geometria propio (reemplaza al GLB)
#include "io/W3dMeshBin.h"             // .w3db: la misma geometria en BINARIO, lista para memoria (el default)
#include "importers/import_w3d.h"      // W3dFormatMeshes: binario o texto (opcion del proyecto)
#include "edit/MeshEdit.h"             // W3dMeshCanonicalIndices: el index buffer que hornea el .w3db
#include "importers/import_obj.h"     // PendingTextureOf: la carga de texturas es DIFERIDA
#include "objects/Materials.h"         // los MATERIALES viajaban dentro del GLB: ahora van al JSON
#include "objects/Textures.h"
#include "render/UIOverlay.h"          // UI2D_IsElement2D: los hijos 2D de un UI viajan en SU .w3dui
#include "ViewPorts/Notifications.h"
#include "W3dNotice.h"                // W3dAvisof: los avisos del guardado se VEN, no solo se loguean
#include "ViewPorts/ViewPorts.h"      // rootViewport: el arbol VIVO de viewports (layout)
#include "ViewPorts/ViewPort3D.h"     // la VISTA (pivot/orbit/rotacion) viaja con el layout
#include "ViewPorts/Timeline.h"       // el MODO del timeline (dope/curvas) tambien viaja con el layout
#include "ViewPorts/Outliner.h"       // la VISTA del outliner (escena / mallas / materiales...) tambien
#include "io/ProjectResources.h"      // carpetas del outliner por recursos + texturas purgadas
#include "io/ExternalLibrary.h"   // "librerias": las vinculadas a la biblioteca
#include "ViewPorts/PopUp/FileBrowser.h"
#include "ViewPorts/PopUp/ConfirmPopup.h"  // "Guardar como": confirmar antes de pisar un .w3d
#include "render/RenderOptions.h"
#include "render/RealTimeShadows.h"   // g_shadows: la tarjeta "Sombras en tiempo real"
#include "render/RayTracer.h"   // g_rt: el trazado de rayos del proyecto
#include "render/PostProcess.h"   // los efectos del objeto Postproceso
#include "objects/PostProcessObj.h"
#include "animation/Animation.h"   // AnimFPS (se guarda con el proyecto)
#include "variables.h"   // w3dPath (el archivo abierto)
#include "w3dFilesystem.h"
#include "w3dlog.h"
#include "io/ProjectMeshes.h"   // las mallas 3D como RECURSO: plan de guardado + registro "mallas"
#include "objects/MeshResource.h"
#include <stdio.h>
#include <string.h>    // strlen: el buffer de los avisos se mide con el formato traducido
#include <string>
#include <vector>
#include <algorithm>   // std::sort: los refs de script se escriben en orden deterministico
#include <set>
#include <sstream>     // el nombre del PRIMER frame de una vertex anim (base + 001 + .obj)
#include <iomanip>     // std::setw/std::setfill: el mismo padding que VertexAnimation::LoadFrames
#include "graphics/w3dTexture.h"   // Textures16Always: se guarda en la cabecera
#ifdef _WIN32
    #include <direct.h>
    #define W3D_MKDIR(p) _mkdir(p)
#else
    #include <sys/stat.h>
    #define W3D_MKDIR(p) mkdir(p, 0755)
#endif

// ICONO del juego (tarjeta Juego): ruta al PNG, relativa al .w3d cuando se puede.
// Va al scene.json como "icono" (ruta EXTERNA, no se mete al zip a proposito).
std::string g_projIcon;

// TEST del harness (ver SaveW3D.h): fallo de escritura inyectado. Siempre
// false en el editor real.
bool g_w3dFailWriting = false;
// TEST del harness (ver SaveW3D.h): guardar las mallas como una plataforma de indices
// de 16 bits. Siempre false en el editor real.
bool g_w3dIndices16Simulated = false;
// TEST del harness (ver SaveW3D.h): el esquema de mallas de antes. Siempre false en el editor real.
bool g_w3dMeshesLegacy = false;

// ESCENA UI QUE NO CARGO en la apertura actual (import_w3d.cpp). Vacio = ninguna.
// Freno de guardado: ver el chequeo al entrar a SaveW3D.
extern std::string g_w3dUINotLoaded;

// CONFIG de la tarjeta Juego (Compilar juego): viaja con el proyecto como
// objeto raiz "compilar" del .w3d (ver SaveW3D.h). Arranca en los defaults.
W3dCompileCfg g_projCompile = { 1, 0, 0, 0, true, true, false, 0, 100, 100, -1 };   // (volumen y resolucion al 100)

void W3dCompileReset() {
    g_projCompile.windowMode  = 1;      // Pantalla completa (el default de siempre)
    g_projCompile.orientation = 0;      // Todas
    g_projCompile.assetsMode  = 0;      // Sueltos (editables)
    g_projCompile.platform    = 0;      // Linux .deb
    g_projCompile.usePhysics  = true;
    g_projCompile.useSound    = true;
    g_projCompile.debugMode   = false;  // produccion
    g_projCompile.uid         = 0;       // sin UID: un proyecto sin "uid" no hereda el del anterior. El save/load lo persiste.
    g_projCompile.volume      = 100;     // volumen del gameplay al maximo por defecto
    g_projCompile.resolution  = 100;     // la pantalla nativa
    g_projCompile.backend     = -1;      // el de siempre de cada plataforma
}

// int <-> string legible del bloque "compilar" (el JSON se edita a mano: nada
// de indices crudos). Un string desconocido cae al default del campo.
const char* W3dCompileWindowModeStr(int m) {
    return (m == 0) ? "window" : (m == 2) ? "borderless" : "fullscreen";
}
int W3dCompileWindowModeInt(const std::string& s) {
    if (s == "window")   return 0;
    if (s == "borderless") return 2;
    return 1;
}
const char* W3dCompileOrientationStr(int o) {
    return (o == 1) ? "vertical" : (o == 2) ? "horizontal" : "all";
}
int W3dCompileOrientationInt(const std::string& s) {
    if (s == "vertical")   return 1;
    if (s == "horizontal") return 2;
    return 0;
}
const char* W3dCompileAssetsStr(int a) { return (a == 1) ? "packed" : "loose"; }
int W3dCompileAssetsInt(const std::string& s) { return (s == "packed") ? 1 : 0; }
const char* W3dCompilePlatformStr(int p) {
    return (p == 10) ? "dreamcast" : (p == 9) ? "3ds" : (p == 8) ? "vita" : (p == 7) ? "psp" :
           (p == 6) ? "android23" : (p == 5) ? "symbian" : (p == 4) ? "windows" :
           (p == 3) ? "android" : (p == 2) ? "web" : (p == 1) ? "appimage" : "linux-deb";
}
int W3dCompilePlatformInt(const std::string& s) {
    if (s == "appimage") return 1;
    if (s == "web")      return 2;
    if (s == "android")  return 3;
    if (s == "windows")  return 4;
    if (s == "symbian")  return 5;
    if (s == "android23") return 6;   // (Android 2.3: el APK de platform/android-viejo)
    if (s == "psp")       return 7;
    if (s == "vita")      return 8;
    if (s == "3ds")       return 9;
    if (s == "dreamcast") return 10;
    return 0;
}
const char* W3dCompileBackendStr(int b) { return (b == 2) ? "vulkan" : (b == 1) ? "shaders" : (b == 0) ? "fixed" : "auto"; }
int W3dCompileBackendInt(const std::string& s) {
    return (s == "vulkan") ? 2 : (s == "shaders") ? 1 : (s == "fixed") ? 0 : -1;
}
int W3dCompileBackends(int p) {
    if (p == 0 || p == 1 || p == 3 || p == 4) return 3;   // Linux, Windows y Android: fijo, shaders y Vulkan
    if (p == 5) return 2;                                 // Symbian: sin shaders y con shaders
    return 1;
}
const char* W3dCompileBackendName(int p, int b) {
    switch (p) {
        case 0: case 1: case 4:   // (el de shaders, sobre GL 2.1; Vulkan cae a OpenGL donde no esta)
            return (b == 2) ? "Vulkan" : (b == 1) ? "OpenGL ES 2.0" : "OpenGL";
        case 3:  return (b == 2) ? "Vulkan" : (b == 1) ? "OpenGL ES 2.0" : "OpenGL ES 1.1";
        case 5:  return (b == 1) ? "OpenGL ES 2.0" : "OpenGL ES 1.1";            // (el N95: 1.1; el N8 y los Belle: 2.0)
        case 2:  return "WebGL";
        case 6:  return "OpenGL ES 2.0";
        case 7:  return "sceGu (GE)";
        case 8:  return "vitaGL";
        case 9:  return "citro3d";
        case 10: return "GLdc";
        default: return "OpenGL";
    }
}
int W3dCompileBackendOf(int p) {
    const int n = W3dCompileBackends(p);
    if (n > 1 && g_projCompile.backend >= 0 && g_projCompile.backend < n) return g_projCompile.backend;
    // (el de siempre: Android y la web dibujan con shaders; la PC, Symbian y las consolas con el fijo o el suyo)
    return (p == 3 || p == 6) ? 1 : 0;
}

static int gNotCovered = 0;    // curvas SIN archivo de origen (unico caso que el guardado no cubre)

static std::string Folder(const std::string& r) {
    size_t s = r.find_last_of("/\\");
    return (s == std::string::npos) ? std::string(".") : r.substr(0, s);
}
static std::string Base(const std::string& r) {
    size_t s = r.find_last_of("/\\");
    return (s == std::string::npos) ? r : r.substr(s + 1);
}
// crea la ruta de carpetas completa ("a/b/c"): mkdir por segmento (existente = ok).
// Hace falta para la estructura content/ (rutas anidadas como contenido/modelos)
static void MkdirRec(const std::string& dir) {
    for (size_t i = 1; i < dir.size(); i++)
        if (dir[i] == '/' || dir[i] == '\\')
            W3D_MKDIR(dir.substr(0, i).c_str());
    W3D_MKDIR(dir.c_str());
}
static std::string BaseNoExt(const std::string& r) {
    std::string b = Base(r);
    size_t p = b.find_last_of('.');
    return (p == std::string::npos) ? b : b.substr(0, p);
}
// ---------------------------------------------------------------------------
//  ESCRITURA ATOMICA (temp + rename)
//
//  Todo lo que genera el guardado se escribe primero a "<destino><kTmpSuffix>",
//  en la MISMA carpeta que el destino final (rename entre carpetas distintas no
//  es atomico y puede ni funcionar entre volumenes), y los renames se hacen
//  TODOS JUNTOS al final, cuando ya se sabe que el .w3d entero se pudo escribir.
//  Si algo falla antes se borran los temporales y en disco no cambio NADA.
// ---------------------------------------------------------------------------
static const char* const kTmpSuffix = ".w3dtmp";

// renombra tmp -> fin (w3dFileSystem::ReplaceFile: tambien donde rename() no pisa un destino que ya existe, Windows,
// Symbian y la 3DS). Reporte del dueno: "en symbian al abrir un w3d y querer guardarlo no se puede guardar" -- abrir
// deja w3dPath apuntando a un archivo que existe y el rename fallaba SIEMPRE (un proyecto nuevo se guardaba bien
// solo la primera vez); en la 3DS pasaba lo mismo.
static bool RenameOver(const std::string& tmp, const std::string& end) {
    if (w3dFileSystem::ReplaceFile(tmp, end)) return true;
    w3dLogfE("SaveW3D: no pude renombrar %s -> %s (errno %d)", tmp.c_str(), end.c_str(), errno);
    return false;
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
// releido con el parser de JsonW3d.h (ver JsonNumText). Antes era un "%g" pelado
// = 6 cifras significativas, o sea que NINGUN float del project.json hacia
// round-trip: 3.14159265 volvia 3.14159. Afectaba a todo (pos/rot/escala de cada
// objeto, colores, valores de keyframe y offsets de handles) y no se veia nunca,
// porque la diferencia aparece recien en el decimal 6.
static void JNum(std::string& s, float v) { s += JsonNumText(v); }
// UN ENTERO del JSON. Existe porque muchisimos campos enteros se escribian como
// JNum((float)x): para valores chicos da lo mismo, pero de 1e6 para arriba salia
// notacion cientifica ("1.23457e+06") y ese texto ya no es el entero que era. Un
// numero de frame grande entra justo en ese rango.
static void JInt(std::string& s, int v) {
    char b[24]; snprintf(b, sizeof(b), "%d", v); s += b;
}
static void JSang(std::string& s, int n) { for (int i = 0; i < n; i++) s += "  "; }

// contexto del guardado en curso
struct CtxSave {
    std::string dirW3d;   // carpeta del .w3d (de ahi salen los temporales)
    int  vtxN;            // desempate de nombre de los blobs de vertex anim
    bool error;           // algo no se pudo escribir: se aborta TODO el guardado
    W3dContainerWriter* esc;   // el contenedor que se esta armando
};

// ---------------------------------------------------------------------------
//  FORMATO v4: EL CONTENEDOR EN CURSO
//
//  Mientras dura un SaveW3D, 'gEsc' apunta al escritor y TODA ruta de asset
//  pasa por el: se mete el archivo adentro del zip y lo que se escribe en el
//  JSON es su NOMBRE DE ENTRADA ("textures/pausa.png"). 'gWho' es solo para
//  el EXTERNALS.txt (de quien es la referencia que quedo afuera).
//
//  El .w3dui llega por el HOOK g_w3dRefEmit (ver UI2DFormat.h): ese .cpp lo
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
static W3dContainerWriter* gEsc = NULL;
static std::string gWho;
// SERIALIZACION EN SECO (io/Prefabs.h): el JSON de un subarbol SIN contenedor ni plan de guardado. La usa la
// definicion en memoria de un prefab que se esta editando (lo que ven sus instancias antes de guardar). Nada se
// ingiere ni se renombra: las rutas van como estan en memoria, las mallas por el nombre de su recurso, cada nodo
// lleva su "_origen" (el serial del objeto) y lo que el JSON no puede llevar sin contenedor (los clips de un
// armature, los frames de una vertex anim) lo copia la generacion de la plantilla (W3dPrefabFixHook).
static bool gDry = false;

static std::string W3dRefEmit(std::string& path) {
    if (!gEsc) return path;
    return gEsc->Ingest(path, NULL, gWho);
}

// LA ruta de un asset del usuario -> lo que se escribe en el JSON. El asset se
// METE adentro del .w3d y en el archivo va su NOMBRE DE ENTRADA
// ("textures/pausa.png"), o "ext:..." si el usuario lo dejo afuera a proposito o
// si no se pudo leer. 'diskPath' NO es const: queda con lo que vale EN MEMORIA
// (el nombre de entrada para los internos), que es lo que resuelve ReadFileBytes.
static std::string Asset(CtxSave* cx, std::string& diskPath) {
    if (diskPath.empty()) return diskPath;
    if (gDry || !cx->esc) return diskPath;   // (en seco: la ruta tal cual esta en memoria)
    { std::string png;   // textura interna que solo vive en memoria (generada / pintada sin guardar): sus bytes
      if (TexEditBytesToSave(diskPath, png)) { cx->esc->AddBytes(diskPath, png, true); return diskPath; } }
    const std::string prev = diskPath;
    const std::string e = cx->esc->Ingest(diskPath, NULL, gWho);
    // (un archivo de DISCO que entro adentro recien ahora -un proyecto que no se habia guardado-: su origen, para
    //  "Actualizar textura")
    if (!e.empty() && e != prev && !W3dIsEntryName(prev) && !W3dRefIsExternal(prev)) W3dAssetOriginAnnotate(e, prev);
    return e;
}

// LA IMAGEN 360 DE UNA SONDA DE REFLEJO: entra al .w3d como cualquier textura (la horneada que vive en memoria, la del
// .w3d abierto -aunque se haya purgado en la biblioteca- o la de afuera de una personalizada). Es un dato DERIVADO (se
// vuelve a hornear; la de tiempo real ni la usa), asi que nunca traba el guardado: si ya no esta en ningun lado, la
// sonda se guarda SIN imagen. Antes la nombraba igual y el guardado se cancelaba ("falta adentro del proyecto").
static bool ImageProbeHas(const std::string& path) {
    if (path.empty()) return false;
    if (W3dRefIsExternal(path)) return true;                      // (una de afuera, elegida a proposito)
    Texture* t = TextureFind(path);
    if (t && TexEditFind(t)) return true;                      // (horneada en esta sesion: en memoria)
    if (W3dIsEntryName(path)) return W3dContainerExists(path);
    return w3dFileSystem::FileExists(path);                      // (un archivo del disco)
}
static std::string ImageProbe(CtxSave* cx, ReflectionProbe* sr) {
    if (sr->texture.empty() || gDry || !cx->esc) return sr->texture;
    if (!ImageProbeHas(sr->texture)) {
        w3dLogfW("[W3D] la sonda '%s' nombra '%s', que ya no esta: se guarda sin imagen", sr->name.c_str(), sr->texture.c_str());
        return std::string();
    }
    return Asset(cx, sr->texture);   // (deja sr->texture con su nombre de entrada, como a cualquier textura)
}

// la textura de un paso "Bind texture" de una Rutina: entra al .w3d como la de un material
static std::string RoutineEmitTexture(std::string& path, void* cx) { return Asset((CtxSave*)cx, path); }

static void CommonFields(std::string& s, int ind, Object* o) {
    JSang(s, ind); s += "\"name\": "; JEsc(s, o->name); s += ",\n";
    if (!o->visible) { JSang(s, ind); s += "\"visible\": false,\n"; }
    // ESTATICO: solo se escribe cuando es DINAMICO (default true = estatico), asi los proyectos que no lo
    // tocan quedan igual. Lo usa W3dGridCull (celda cacheada vs re-evaluada por frame).
    if (!o->isStatic) { JSang(s, ind); s += "\"static\": false,\n"; }
    // LAS SOMBRAS EN TIEMPO REAL: solo lo apagado (de fabrica proyecta y recibe)
    if (!o->projectsShadow) { JSang(s, ind); s += "\"castsShadow\": false,\n"; }
    if (!o->receivesShadow) { JSang(s, ind); s += "\"receivesShadow\": false,\n"; }
    // LA LUZ PRECALCULADA: solo lo que no es de fabrica (rebota; no es captador de sombras)
    if (!o->bouncesLight) { JSang(s, ind); s += "\"bouncesLight\": false,\n"; }
    if (o->catchesShadows) { JSang(s, ind); s += "\"catchesShadows\": true,\n"; }
    if (!o->inProbes) { JSang(s, ind); s += "\"inProbes\": false,\n"; }   // (las sondas no lo ven)
    // LINEAS PARENTALES: solo se escribe cuando se APAGO (el default es true), asi los
    // proyectos que no la tocan quedan byte a byte como antes.
    if (!o->showRelationshipsLines) { JSang(s, ind); s += "\"parentLines\": false,\n"; }
    // PALETA elegida por el objeto, POR NOMBRE (ausente = hereda del padre)
    if (!o->palette.empty()) { JSang(s, ind); s += "\"palette\": "; JEsc(s, o->palette); s += ",\n"; }
    // FISICA de cuerpo rigido (opcional): espejo exacto de la lectura en
    // CommonJson de import_w3d (ver physics/W3dRigid.h)
    if (o->physics) {
        const W3dRigidDef* d = o->physics;
        JSang(s, ind); s += "\"physics\": { \"type\": ";
        s += (d->type == 0) ? "\"static\"" : (d->type == 2) ? "\"character\"" : (d->type == 3) ? "\"mesh\"" : "\"dynamic\"";
        s += ", \"mass\": "; JNum(s, d->mass);
        s += ", \"box\": ["; JNum(s, d->box[0]); s += ", "; JNum(s, d->box[1]); s += ", "; JNum(s, d->box[2]);
        s += "], \"center\": ["; JNum(s, d->center[0]); s += ", "; JNum(s, d->center[1]); s += ", "; JNum(s, d->center[2]);
        s += "], \"friction\": "; JNum(s, d->friction);
        s += ", \"bounce\": "; JNum(s, d->bounce);
        s += " },\n";
    }
    // SIN TRANSFORMACION (la Rutina): no tiene posicion, rotacion ni escala -> no se escriben (el campo anterior
    // ya cerro con ",\n": se lo saca, el que sigue pone su propia coma)
    if (o->noTransform) {
        if (s.size() >= 2 && s.compare(s.size() - 2, 2, ",\n") == 0) s.erase(s.size() - 2);
        return;
    }
    JSang(s, ind); s += "\"pos\": ["; JNum(s, o->pos.x); s += ", "; JNum(s, o->pos.y); s += ", "; JNum(s, o->pos.z); s += "],\n";
    JSang(s, ind); s += "\"rot\": ["; JNum(s, o->rotEuler.x); s += ", "; JNum(s, o->rotEuler.y); s += ", "; JNum(s, o->rotEuler.z); s += "],\n";
    JSang(s, ind); s += "\"scale\": ["; JNum(s, o->scale.x); s += ", "; JNum(s, o->scale.y); s += ", "; JNum(s, o->scale.z); s += "]";
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
// valen para HFree/HAligned (los otros tres tipos los CALCULA EffectiveHandle
// desde los vecinos, ver Animation.h), asi que uno podria omitirlos cuando no
// aplican. No se hace, y a proposito: el usuario mueve un handle, cambia el tipo
// a Auto para ver como queda y lo vuelve a Free -> si el archivo no los hubiera
// guardado, su curva volveria distinta. Son dato del keyframe, no cache del tipo
// vigente. Ademas el largo fijo de 8 campos hace el bloque legible y diffeable.
static void WriteCurve(std::string& s, const AnimProperty& ap) {
    s += "{ \"p\": "; JInt(s, ap.Property);
    s += ", \"c\": "; JInt(s, ap.component);
    if (ap.cyclePrev)   { s += ", \"ca\": "; JInt(s, ap.cyclePrev); }     // el modificador Repetir
    if (ap.cycleAfter) { s += ", \"cd\": "; JInt(s, ap.cycleAfter); }
    // la PILA DE MODIFICADORES de la curva (Animation.h, KfModifier): una fila de numeros por modificador, en el
    // orden de AFloats (agregar campos va al final: un archivo viejo trae menos y el resto queda en su default).
    // Sin modificadores no se escribe nada: el archivo de una curva comun es el mismo de siempre.
    if (!ap.mods.empty()) {
        s += ", \"m\": [";
        for (size_t i = 0; i < ap.mods.size(); i++) {
            float f[KfModifier::NFloats]; ap.mods[i].AFloats(f);
            if (i) s += ",";
            s += "[";
            for (int q = 0; q < KfModifier::NFloats; q++) { if (q) s += ","; JNum(s, f[q]); }
            s += "]";
        }
        s += "]";
    }
    s += ", \"k\": [";
    for (size_t i = 0; i < ap.keyframes.size(); i++) {
        const keyFrame k = ap.keyframes.Key(i);   // (por valor: guardar no expande una curva compacta)
        if (i) s += ",";
        s += "["; JInt(s, k.frame); s += ","; JNum(s, k.value);
        s += ","; JInt(s, k.Interpolation); s += ","; JInt(s, k.handleType);
        s += ","; JNum(s, k.inDF); s += ","; JNum(s, k.inDV);
        s += ","; JNum(s, k.outDF); s += ","; JNum(s, k.outDV); s += "]";
    }
    s += "] }";
}

// ---------------------------------------------------------------------------
//  LA GEOMETRIA DE UNA MALLA -> meshes/<slug>.w3dm (formato propio, ver W3dMesh.h)
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
//  SIDECARS DEL MODELO: <base>.pvs.json (y <base>.groups.json)
//
//  Un .obj puede traer al lado archivos DERIVADOS que el motor lee pero que
//  ningun editor externo produce: los sectores del PVS y los pesos de los grupos
//  de vertices. Antes ninguno pasaba por el contenedor: al empaquetar un .w3d v4
//  y mandarlo a otra maquina, el .obj entraba como "models/x.obj" y el sidecar
//  no entraba a ningun lado -- `noExt + ".pvs.json"` daba "models/x.pvs.json",
//  que no existe como entrada, y el PVS desaparecia SIN AVISO (ni siquiera
//  figuraba en EXTERNALS.txt, a diferencia del resto de las refs externas).
//
//  Ahora se ingieren como cualquier asset y el nodo guarda su nombre REAL de
//  entrada, en vez de derivarlo. (Los grupos de vertices, ademas, ya viajan
//  dentro del .w3dm de la malla; el sidecar es solo su ORIGEN de importacion,
//  pero se ingiere igual para poder re-importar el .obj desde el paquete.)
// ---------------------------------------------------------------------------
static void IngestSidecars(CtxSave* cx, Mesh* m) {
    if (!m || gDry || !cx->esc) return;
    // 1) las declaraciones EXPLICITAS del modificador (`vis:` / `pvs:`) MANDAN:
    //    se ingiere ESE archivo y el nodo guarda su nombre real de entrada.
    //    Antes este paso no existia y el paso 2 PISABA la declaracion con el
    //    sidecar derivado del origen: un modificador que apuntaba a otro dato
    //    (p.ej. un `.w3dvis` alternativo para la misma malla) perdia su archivo
    //    al empaquetar, EN SILENCIO.
    for (size_t k = 0; k < m->modifiers.size(); k++) {
        Modifier* md = m->modifiers[k];
        if (!md || md->type != ModifierType::CullingTri) continue;
        if (!md->pvsFile.empty() && w3dFileSystem::FileExists(md->pvsFile))
            md->pvsFile = cx->esc->Ingest(md->pvsFile, NULL, gWho);
        if (!md->visFile.empty() && w3dFileSystem::FileExists(md->visFile))
            md->visFile = cx->esc->Ingest(md->visFile, NULL, gWho);
    }
    // 2) los DERIVADOS del origen (compatibilidad + re-importacion): se ingieren
    //    igual, pero solo RELLENAN el nombre del modificador si estaba vacio.
    if (m->origin.empty()) return;
    std::string base = m->origin;
    size_t point = base.find_last_of('.');
    size_t bar   = base.find_last_of("/\\");
    if (point != std::string::npos && (bar == std::string::npos || point > bar))
        base = base.substr(0, point);
    static const char* kSuffixes[] = { ".pvs.json", ".groups.json", ".w3dvis" };
    for (int i = 0; i < 3; i++) {
        std::string path = base + kSuffixes[i];
        if (!w3dFileSystem::FileExists(path)) continue;
        std::string entry = cx->esc->Ingest(path, NULL, gWho);
        if (i == 0 || i == 2) {   // los datos de visibilidad los consume el modificador
            for (size_t k = 0; k < m->modifiers.size(); k++) {
                Modifier* md = m->modifiers[k];
                if (!md || md->type != ModifierType::CullingTri) continue;
                if (i == 0) { if (md->pvsFile.empty()) md->pvsFile = entry; }
                else        { if (md->visFile.empty()) md->visFile = entry; }
            }
        }
    }
}

// ---------------------------------------------------------------------------
//  BINARIO (.w3db) o TEXTO (.w3dm): lo decide la opcion del proyecto "formatoMallas"
//  (W3dFormatMeshes, default binario). Los DOS guardan la misma malla (ver W3dMeshBin.h):
//  el binario ademas HORNEA lo que el texto obliga a derivar al abrir (index buffer por
//  parte + Forsyth, aristas, AABB, capas activas en el render, identidad de los puntos),
//  asi abrir el proyecto es leer la entrada y copiar. Los frenos de abajo valen igual.
//  EXCEPCION: con indices de 16 bits (el N95) una malla de mas de 65535 render-verts no
//  tiene index buffer correcto en memoria (se trunca modulo 65536) y el .w3db no se puede
//  escribir (W3dMeshBinIndicesReach): esa sale en TEXTO, que guarda las caras con int.
//  El PC la abre entera y el telefono la vuelve a abrir como antes. Se avisa.
// ---------------------------------------------------------------------------
static std::string WriteMeshW3dm(CtxSave* cx, Mesh* m, const std::string& objName) {
    if (gDry || !cx->esc) return std::string();   // (en seco no hay donde escribirla: va por su recurso)
    const int nRV = m->vertex ? m->vertexSize : 0;
    // una malla de un RECURSO con la edicion pendiente se materializa para escribirla y despues
    // la suelta (no hay por que dejar en memoria las caras de mil arboles por un guardado)
    const bool editBorrowed = m->mesh && !m->pendingEdit.empty();
    const bool requestsBinary = !W3dMeshResourceInText(m->mesh);   // (el formato es de cada malla: su tarjeta Recurso)
    const bool notEnters = !W3dMeshBinIndicesReach(nRV) || (g_w3dIndices16Simulated && nRV > 65535);
    const bool binary    = requestsBinary && !notEnters;
    const char* ext = binary ? ".w3db" : ".w3dm";
    std::string base = W3dSlugEntry(objName.empty() ? std::string("mesh") : objName);
    std::string name = "meshes/" + base + ext;
    for (int k = 2; cx->esc->Has(name); k++) {
        char sf[16]; snprintf(sf, sizeof(sf), "-%d", k);
        name = "meshes/" + base + sf + ext;
    }
    // EL REST DEL SKINNING 2D ES DATO, NO CACHE: en memoria se captura LAZY (la primera vez que
    // se posa el rig o al reabrir el proyecto), asi que una malla con armature 2D todavia sin
    // posar no lo tiene. Si no se captura aca, el archivo sale SIN bloque UVREST, al reabrirlo el
    // editor lo captura, y el guardado siguiente sale distinto: el round-trip byte a byte se
    // rompia por un dato que en realidad no cambio.
    if (m->HasArm2D()) m->Armature2DRestCapture();

    // ---- EL FRENO DE MANO, ANTES DE TOCAR NADA (spec seccion 17) --------------
    //  El .w3dm decia "requiere <X>" y esta version no entiende X: guardar encima
    //  degradaria el archivo EN SILENCIO, que es exactamente lo que esa directiva
    //  existe para impedir. Antes info.readOnly solo se logueaba y el guardado
    //  seguia igual, o sea el freno no frenaba nada.
    const std::string missing = W3dMeshMissingBlock(m);
    if (!missing.empty()) {
        w3dLogfE("[W3D] '%s': el .w3dm REQUIERE el bloque '%s' y esta version no lo entiende: NO guardo encima",
                 objName.c_str(), missing.c_str());
        W3dAvisof(true, T("Not saving '%s': its mesh needs the block '%s' from a newer version of Whisk3D"),
                  W3dShortName(objName).c_str(), W3dShortName(missing, 16).c_str());
        cx->error = true;   // no hay medio guardado posible: o sale entero o no sale
        return name;
    }
    // ---- NO CARGO: LA MALLA DE LA ESCENA NO ES LA DEL ARCHIVO -----------------
    //  El lector RECHAZO el .w3dm entero (no es W3DMESH / lexico de una version mas
    //  nueva / falta V o F) y lo que quedo en la escena es un Mesh VACIO con el
    //  nombre y el transform del proyecto. Guardar encima HORNEA esa nada: el .w3dm
    //  sale con 0 puntos / 0 caras y la geometria del usuario no existe mas. Se
    //  frena IGUAL que con "requiere": el proyecto no se guarda y se dice por que.
    if (m->w3dmForeign.notLoaded) {
        w3dLogfE("[W3D] '%s': su geometria NO se pudo leer (la malla quedo vacia): NO guardo encima",
                 objName.c_str());
        W3dAvisof(true, T("Not saving '%s': its geometry could not be read when opening and saving would erase it"),
                  W3dShortName(objName).c_str());
        cx->error = true;   // no hay medio guardado posible: o sale entero o no sale
        return name;
    }
    // ---- EDICION PENDIENTE: UNA .w3db LEIDA COMO EL JUEGO (solo render) -------
    //  Sus caras, capas y aristas siguen en la entrada (Mesh::pendingEdit): se
    //  materializan ANTES de escribir. Si no se puede, guardar hornearia una malla sin
    //  caras: se frena igual que con "no cargo".
    if (!m->pendingEdit.empty() && !W3dMeshBinMaterializeEdit(m)) {
        w3dLogfE("[W3D] '%s': no pude leer las caras de su entrada .w3db: NO guardo encima", objName.c_str());
        W3dAvisof(true, T("Not saving '%s': could not read its faces and saving would erase them"),
                  W3dShortName(objName).c_str());
        cx->error = true;
        return name;
    }
    // ---- ABIERTA CON ERRORES (spec 19f) ---------------------------------------
    //  La malla vino de un .w3dm truncado: se cargo lo que habia y guardar encima
    //  HORNEA esa perdida. La spec pide CONFIRMACION; el guardado de hoy es
    //  sincronico de punta a punta y el popup es asincronico, asi que por ahora se
    //  avisa FUERTE (no se hornea en silencio) y queda pendiente el confirmar.
    if (m->w3dmForeign.truncated)
        W3dAvisof(true, T("'%s' was opened WITH ERRORS (truncated file): when saving, whatever was missing is lost"),
                  W3dShortName(objName).c_str());

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
    W3dRestVertexAnim rest(m);
    // ...y lo MISMO con la animacion UV "tira de atlas": el cuadro aplicado es estado
    // de la sesion, no del modelo. Se deja uv[] en base ANTES de escribir (el proximo
    // tick reaplica el cuadro y re-sube el VBO solo, ver Mesh::RestUVAnimStrip).
    m->RestUVAnimStrip();

    std::string txt;
    std::vector<std::string> notices;
    bool written;
    if (binary) {
        // el index buffer CANONICO (el que armaria RegroupMeshParts al abrir el .w3dm): se
        // hornea aca, una vez, y abrir no triangula ni reordena nada. No toca la malla viva.
        W3dMeshBinIndices canon;
        W3dMeshCanonicalIndices(m, canon);
        written = W3dMeshBinWrite(m, &canon, txt, &notices);
    } else {
        if (requestsBinary) {   // el proyecto pedia binario y esta malla no entra (ver arriba)
            w3dLogfW("[W3D] '%s': %d vertices de render y esta plataforma usa indices de 16 bits: la guardo en texto (.w3dm)",
                     objName.c_str(), nRV);
            W3dAvisof(false, T("'%s' has more than 65535 vertices: saving it as text (.w3dm), not binary"),
                      W3dShortName(objName).c_str());
        }
        written = W3dMeshWrite(m, txt, &notices);
    }
    if (!written) {
        w3dLogfE("GuardarW3D: no pude escribir la geometria de '%s'", objName.c_str());
        for (size_t i = 0; i < notices.size(); i++)
            w3dLogfE("[W3D] '%s': %s", objName.c_str(), notices[i].c_str());
        cx->error = true;   // aborta el guardado entero: mejor eso que un .w3d que apunta a la nada
        return name;
    }
    // SILENCIOSO JAMAS: todo lo que NO entro entero al archivo (un bloque de una version
    // mas nueva que no sobrevivio al cambio de topologia, una capa UV/color que no mide lo
    // que la topologia dice, un borde sharp/seam que quedo sin punto, un peso de vertex
    // group que choca) sale por el log Y POR PANTALLA. Una linea en un log que nadie abre
    // no es "avisar": asi se perdio una capa de color entera sin que el dueno se enterara.
    for (size_t i = 0; i < notices.size(); i++)
        w3dLogfW("[W3D] '%s': %s", objName.c_str(), notices[i].c_str());
    if (!notices.empty())
        W3dAvisof(true, notices.size() > 1 ? T("'%s': %s (and more: see whisk3d.log)") : "'%s': %s",
                  W3dShortName(objName).c_str(), notices[0].c_str());
    if (!cx->esc->AddBytes(name, txt, false)) {
        w3dLogfE("GuardarW3D: no pude meter %s adentro del .w3d", name.c_str());
        cx->error = true;
    }
    if (editBorrowed) W3dMeshReleaseEdit(m);
    return name;
}

// ---------------------------------------------------------------------------
//  MATERIALES DEL PROYECTO (bloque raiz "materiales")
//
//  Antes viajaban ADENTRO del GLB de cada malla (con su textura embebida) y volvian por
//  ImportGLTF. Sin GLB de guardado hay que escribirlos, o el proyecto reabriria entero en gris.
//  Van una sola vez a la raiz porque en Whisk3D el espacio de nombres de materiales es GLOBAL
//  (FindMaterialByName) y el mismo material lo comparten varias mallas; cada .w3dm lo
//  referencia POR NOMBRE en su bloque PARTMAT.
//
//  Se juntan durante el recorrido de la escena, en orden de PRIMERA APARICION (deterministico),
//  y no se escribe el registro global entero: un material que ya no usa nadie no vuelve.
// ---------------------------------------------------------------------------
static std::vector<Material*> gMats;

static void MatRegisterOne(Material* mat) {
    if (!mat) return;
    if (!mat->lib.empty()) return;   // (uno de una LIBRERIA: se nombra, no se escribe)
    for (size_t i = 0; i < gMats.size(); i++) if (gMats[i] == mat) return;
    gMats.push_back(mat);
}
static void MatRegister(Mesh* m) {
    for (size_t g = 0; g < m->materialsGroup.size(); g++) {
        MatRegisterOne(m->materialsGroup[g].material);
        for (size_t c = 0; c < m->materialsGroup[g].layersMat.size(); c++) MatRegisterOne(m->materialsGroup[g].layersMat[c]);   // (las capas de encima)
    }
}

// una textura -> nombre de entrada ("textures/piso.png"), ingiriendola al contenedor.
// Texture::path queda con lo que vale EN MEMORIA (el nombre de entrada), que es lo que
// resuelve ReadFileBytes la proxima vez que haya que recargarla.
static std::string MatTexture(CtxSave* cx, Texture* t) {
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
//  W3dSameMaterial comparaba "/home/.../piso.png" contra "textures/piso.png":
//  distinto -> material NUEVO, y TODOS los materiales de un modelo recien
//  importado volvian renombrados a ".001".
static std::string MatTextureBase(CtxSave* cx, Material* mt) {
    if (mt->texture && !mt->texture->path.empty()) return Asset(cx, mt->texture->path);
    std::string* p = PendingTextureRefOf(mt);
    if (!p || p->empty()) return std::string();
    return Asset(cx, *p);
}

static void WriteMaterials(std::string& s, CtxSave* cx) {
    if (gMats.empty()) return;
    s += "  \"materials\": [\n";
    for (size_t i = 0; i < gMats.size(); i++) {
        Material* mt = gMats[i];
        gWho = "material \"" + mt->name + "\"";
        s += "    { \"name\": "; JEsc(s, mt->name);
        // CARPETA cosmetica del outliner por recursos: solo si tiene (sin carpeta, el texto de siempre)
        if (!mt->folder.empty()) { s += ", \"folder\": "; JEsc(s, W3dCosmeticPathJson(mt->folder)); }
        s += ",\n      \"diffuse\": ["; JNum(s, mt->diffuse[0]); s += ", "; JNum(s, mt->diffuse[1]);
        s += ", "; JNum(s, mt->diffuse[2]); s += ", "; JNum(s, mt->diffuse[3]); s += "]";
        s += ",\n      \"specular\": ["; JNum(s, mt->specular[0]); s += ", "; JNum(s, mt->specular[1]);
        s += ", "; JNum(s, mt->specular[2]); s += ", "; JNum(s, mt->specular[3]); s += "]";
        s += ",\n      \"emission\": ["; JNum(s, mt->emission[0]); s += ", "; JNum(s, mt->emission[1]);
        s += ", "; JNum(s, mt->emission[2]); s += ", "; JNum(s, mt->emission[3]); s += "]";
        s += ",\n      \"ambient\": ["; JNum(s, mt->ambient[0]); s += ", "; JNum(s, mt->ambient[1]);
        s += ", "; JNum(s, mt->ambient[2]); s += ", "; JNum(s, mt->ambient[3]); s += "]";
        s += ",\n      \"brightness\": "; JNum(s, mt->shininess);
        s += ", \"interpolation\": "; JNum(s, (float)mt->interpolation);
        s += ", \"reflectionMode\": "; JNum(s, (float)mt->reflectMode);
        if (mt->reflectionProbe) s += ", \"reflectionProbe\": true";
        // TRAZADO DE RAYOS: solo si se apartan del default (0.5 / 0), mismo criterio que decal
        if (mt->rtRoughness != 0.5f) { s += ", \"roughness\": "; JNum(s, mt->rtRoughness); }
        if (mt->rtMetallic != 0.0f)  { s += ", \"metallic\": ";  JNum(s, mt->rtMetallic); }
        // DECAL / mezcla: se escriben SOLO si se apartan del default, asi un proyecto que no los
        // usa guarda exactamente el mismo texto de siempre (los diffs del .w3d siguen legibles).
        if (mt->depth_bias != 0.0f) { s += ", \"depthBias\": "; JNum(s, mt->depth_bias); }
        if (mt->bias_meters != 0.0f) { s += ", \"biasMeters\": "; JNum(s, mt->bias_meters); }
        if (mt->alphaTest != 0.0f)  { s += ", \"alphaCut\": ";        JNum(s, mt->alphaTest); }
        if (mt->order_pass != 0)  { s += ", \"passOrder\": ";      JNum(s, (float)mt->order_pass); }
        if (mt->mix != 0)        { s += ", \"mix\": ";           JNum(s, (float)mt->mix); }
        if (mt->perPixel) s += ", \"perPixelLight\": true";   // (sombreado por pixel: solo si esta prendido)
        if (mt->lightIndirect) s += ", \"indirectLight\": true";   // (la luz de una sonda por vertice en el pipeline fijo)
        if (mt->ssr) s += ", \"screenReflections\": true";   // (reflejos en pantalla: solo si estan prendidos)
        // LINEAS (aristas por material): solo si esta prendido (mismo criterio que decal)
        if (mt->lines) {
            s += ", \"lines\": true";
            if (mt->thicknessLine != 1.0f) { s += ", \"lineThickness\": "; JNum(s, mt->thicknessLine); }
        }
        s += ",\n      \"flags\": {";
        s += " \"texture\": ";     s += mt->textureOn   ? "true" : "false";
        s += ", \"filtered\": ";   s += mt->filtered    ? "true" : "false";
        s += ", \"repeat\": ";    s += mt->repeat      ? "true" : "false";
        s += ", \"transparent\": "; s += mt->transparent ? "true" : "false";
        s += ", \"light\": ";        s += mt->lighting    ? "true" : "false";
        s += ", \"vertexColor\": "; s += mt->vertexColor ? "true" : "false";
        s += ", \"culling\": ";    s += mt->culling     ? "true" : "false";
        s += ", \"depth\": "; s += mt->depth_test ? "true" : "false";
        s += ", \"depthWrite\": "; s += mt->depth_write ? "true" : "false";
        s += ", \"reflection\": ";    s += mt->chrome      ? "true" : "false";
        s += ", \"normalMap\": ";  s += mt->normalMap   ? "true" : "false";
        s += ", \"uv8bit\": ";     s += mt->uv8bit      ? "true" : "false";
        s += " }";
        std::string tex = MatTextureBase(cx, mt);
        if (!tex.empty()) { s += ",\n      \"texture\": "; JEsc(s, tex); }
        // un material HUERFANO recien abierto tiene sus texturas DORMIDAS (import_obj.h): sus rutas
        // estan ahi, no en los Texture*. Se ingieren por referencia, como la base de la cola.
        AsleepTex* dz = AsleepTexturesOf(mt);
        std::string nrm = MatTexture(cx, mt->normalTexture);
        if (nrm.empty() && dz && !dz->normal.empty()) nrm = Asset(cx, dz->normal);
        if (!nrm.empty()) { s += ",\n      \"normalTexture\": "; JEsc(s, nrm); }
        // LA TEXTURA B (Material::textureB): la RAMPA del toon. La mezcla de texturas de antes ahora es una capa con la
        // mezcla Mix y su factor (al abrir, import_w3d la convierte); una que quedo de una sesion vieja se guarda igual
        std::string tb = MatTexture(cx, mt->textureB);
        if (tb.empty() && dz && !dz->texB.empty()) tb = Asset(cx, dz->texB);
        if (!tb.empty()) { s += ",\n      \"textureB\": "; JEsc(s, tb); s += ", \"mixB\": "; JNum(s, mt->mixB); }
        // LOS MAPAS (W3dMatMap): la capa UV de cada uno, si es numero o textura y la textura de los del trazado de
        // rayos (la base y el normal map ya van arriba). Solo los que no son el default
        {
            std::string ms;
            for (int k = 0; k < W3D_MAPS; k++) {
                std::string tx;
                if (k >= W3D_MAP_EMISSION) {
                    tx = MatTexture(cx, mt->mapTex[k]);
                    if (tx.empty() && dz && !dz->maps[k].empty()) tx = Asset(cx, dz->maps[k]);
                }
                if (tx.empty() && !mt->mapWithTexture[k] && mt->uvMap[k] == 0) continue;
                ms += ms.empty() ? "\n        " : ",\n        ";
                ms += "{ \"map\": \""; ms += W3dMatMapKey(k); ms += "\"";
                if (mt->uvMap[k] != 0) { ms += ", \"uv\": "; JNum(ms, (float)mt->uvMap[k]); }
                if (mt->mapWithTexture[k]) ms += ", \"withTexture\": true";
                if (!tx.empty()) { ms += ", \"texture\": "; JEsc(ms, tx); }
                ms += " }";
            }
            if (!ms.empty()) { s += ",\n      \"maps\": ["; s += ms; s += " ]"; }
            if (mt->ior < 0.9999f || mt->ior > 1.0001f) { s += ",\n      \"ior\": "; JNum(s, mt->ior); }     // (1 = sin refraccion)
            if (mt->transmission > 0.0f) { s += ",\n      \"transmission\": "; JNum(s, mt->transmission); }   // (0 = nada de vidrio)
        }
        // SHADER ESCRITO (objects/WrittenShader.h): el archivo (la entrada "shaders/x.glsl" o una ruta del disco) y el tilde
        if (!mt->shaderFile.empty()) {
            // (uno del disco -el de un proyecto que todavia no se habia guardado- entra al .w3d como las texturas, y el
            //  material y el IDE pasan a usar la entrada)
            const std::string prev = mt->shaderFile;
            const std::string ref = Asset(cx, mt->shaderFile);
            if (mt->shaderFile != prev) { extern void IDEFileMoved(const std::string&, const std::string&); IDEFileMoved(prev, mt->shaderFile); }
            s += ",\n      \"shader\": "; JEsc(s, ref);
            if (mt->shaderOn) s += ",\n      \"shaderOn\": true";
        }
        // SOMBREADO TOON: la textureB es la rampa (solo si no es el default, como el resto de lo opcional)
        if (mt->toon != 0) { s += ",\n      \"toon\": "; JNum(s, (float)mt->toon); }
        if (mt->outline > 0.0f) { s += ",\n      \"outline\": "; JNum(s, mt->outline); }
        // capas de textura EXTRA (multi-pass): su textura y como se mezcla con lo de abajo (las
        // dormidas eran las primeras del material: van adelante, como cuando despiertan)
        if (!mt->layers.empty() || (dz && !dz->layers.empty())) {
            s += ",\n      \"layers\": [";
            bool first = true;
            const size_t nDorm = dz ? dz->layers.size() : 0;
            for (size_t c = 0; c < nDorm + mt->layers.size(); c++) {
                const bool asleep = c < nDorm;
                std::string ct = asleep ? Asset(cx, dz->layers[c].texture) : MatTexture(cx, mt->layers[c - nDorm].tex);
                if (ct.empty()) continue;
                const int mix     = asleep ? dz->layers[c].mix : mt->layers[c - nDorm].blend;
                const int uvLayer = asleep ? dz->layers[c].uv : mt->layers[c - nDorm].uvMap;
                const bool on = asleep ? dz->layers[c].on : mt->layers[c - nDorm].on;
                const float force = asleep ? dz->layers[c].force : mt->layers[c - nDorm].force;
                const bool baked = asleep ? dz->layers[c].baked : mt->layers[c - nDorm].baked;
                // (la mascara vive en el alfa de la textura: una EXTERNA horneada y sin guardar todavia tiene la imagen de
                //  antes en el disco, sin alfa; la marca va recien cuando se guarde la textura)
                const Texture* tLayer = asleep ? NULL : mt->layers[c - nDorm].tex;
                const bool maskSun = asleep ? dz->layers[c].maskSun
                                                : (mt->layers[c - nDorm].maskSun && !(tLayer && TexEditIsExternal(tLayer) && TexEditUnsaved(tLayer)));
                if (!first) s += ",";
                first = false;
                s += " { \"texture\": "; JEsc(s, ct);
                s += ", \"mix\": "; JNum(s, (float)mix);
                if (uvLayer != 0) { s += ", \"uv\": "; JNum(s, (float)uvLayer); }   // solo si no es la base (diffs estables)
                if (force != 1.0f) { s += ", \"force\": "; JNum(s, force); }       // el factor de la mezcla (ausente = 1)
                if (baked) s += ", \"baked\": true";   // (la luz horneada: el material sin luz de tiempo real)
                if (baked && maskSun) s += ", \"sunMask\": true";   // (su alfa: cuanto de la luz es del sol)
                s += ", \"on\": "; s += on ? "true" : "false"; s += " }";
            }
            s += " ]";
        }
        s += " }";
        if (i + 1 < gMats.size()) s += ",";
        s += "\n";
    }
    gWho.clear();
    s += "  ],\n";
}

// bloque "anims" de una malla: cada vertex anim con su rango/fps, los FRAMES de vertices
// como blob BINARIO compacto externo (vtxanim/N.bin al lado del .w3d -> no depende de los
// .obj de origen) y sus curvas de transform. Reemplaza al viejo guardado (solo basePath).
static void WriteAnimsVertex(std::string& s, Mesh* m, int ind, CtxSave* cx) {
    if (m->animations.empty()) return;
    if (gDry) return;   // (en seco: sus frames no tienen entrada; la generacion los copia de la plantilla)
    // ---- EL FRENO DE MANO DE LAS ANIMS (hermano del de la malla, mas arriba) -------------
    //  Una vertex anim cuyo blob de frames NO se pudo leer al abrir quedo con 0 keyframes.
    //  Guardar encima la destruye SIN RASTRO: sin frames no se emite "buffers", y el
    //  contenedor descarta a proposito lo huerfano bajo "animations/" -> el blob viejo se va
    //  del zip y la anim queda como cascara vacia PARA SIEMPRE. Se frena el guardado entero,
    //  igual que con una malla que no cargo: o sale entero o no sale.
    for (size_t i = 0; i < m->animations.size(); i++) {
        VertexAnimation* a = m->animations[i];
        if (!a || !a->notLoaded) continue;
        w3dLogfE("[W3D] '%s': los frames de la vertex anim '%s' no se pudieron leer al abrir: NO guardo encima",
                 m->name.c_str(), a->name.c_str());
        W3dAvisof(true, T("Not saving '%s': the frames of its animation '%s' could not be read when opening and saving would erase them"),
                  W3dShortName(m->name).c_str(), W3dShortName(a->name).c_str());
        cx->error = true;
        return;
    }
    s += ",\n"; JSang(s, ind); s += "\"anims\": [\n";
    for (size_t i = 0; i < m->animations.size(); i++) {
        VertexAnimation* a = m->animations[i];
        JSang(s, ind + 1); s += "{ \"name\": "; JEsc(s, a->name);
        s += ", \"start\": "; JNum(s, (float)a->startFrame);
        s += ", \"end\": "; JNum(s, (float)a->endFrame);
        s += ", \"fps\": "; JNum(s, (float)a->fps);
        s += ", \"speed\": "; JNum(s, a->speed);
        s += ", \"repeat\": "; s += a->repeat ? "true" : "false";
        s += ", \"normals\": "; s += a->UseNormals ? "true" : "false";
        s += ", \"next\": "; JNum(s, (float)a->nextAnimation);
        // referencia .obj legacy (retrocompat / info); la fuente real es el blob de abajo.
        // v4: los .obj los lee ImportWOBJ con std::ifstream (no pasa por el VFS), asi
        // que NO puede vivir adentro del contenedor: queda EXTERNO y a la vista.
        if (!a->basePath.empty()) {
            W3dExternalRefMark(a->basePath);
            std::string ref = Asset(cx, a->basePath);
            // EL basePath NO ES UN ARCHIVO: es el PREFIJO de una secuencia de frames
            // ("modelos/heroe/agachado/agachado" -> agachado001.obj, agachado002.obj...
            // ver VertexAnimation::LoadFrames). Ingerir lo comprobaba con FileExists
            // contra ese nombre pelado, que nunca existe -> TODAS las vertex anims
            // entraban al EXTERNALS.txt como "FALTA" y cada guardado terminaba con el
            // cartel ROJO "Guardado, pero N file(s) external(s) NO estan" sin que
            // faltara un solo archivo (52 en el proyecto medido: eso era el "salieron
            // errores al guardar"). El archivo REAL es el primer frame: se pregunta
            // por el. Y si la anim ya viaja con sus frames adentro del contenedor
            // (el blob "buffers" de abajo, el caso normal), no falta nada aunque el
            // .obj original ya no este: el proyecto se abre igual.
            if (a->frames.empty()) {
                std::ostringstream f1;
                f1 << a->basePath << std::setw(a->padding) << std::setfill('0') << 1 << ".obj";
                cx->esc->ExternalCorrectExists(ref, w3dFileSystem::FileExists(f1.str()));
            } else {
                cx->esc->ExternalCorrectExists(ref, true);
            }
            s += ", \"base\": "; JEsc(s, ref);
            s += ", \"padding\": "; JNum(s, (float)a->padding);
        }
        // FRAMES de vertices -> blob binario EXTERNO (vtxanim/N.bin). Necesita target
        // para saber el vertexCount; lo fijamos por las dudas (las anims del .w3d traen NULL).
        a->target = m;
        if (!a->frames.empty()) {
            std::vector<unsigned char> blob; VertexAnimSerialize(*a, blob);
            if (!blob.empty()) {
                // una ENTRADA del contenedor bajo animations/. Se acabaron los
                // blobs sueltos, el contador que arrancaba en 0 cada vez y la poda
                // de huerfanos que hacia falta por eso.
                std::string base = W3dSlugEntry(a->name.empty() ? std::string("anim") : a->name);
                std::string name = "animations/" + base + ".bin";
                for (int k = 2; cx->esc->Has(name); k++) {
                    char sf[16]; snprintf(sf, sizeof(sf), "-%d", k);
                    name = "animations/" + base + sf + ".bin";
                }
                if (cx->esc->AddBytes(name, std::string(blob.begin(), blob.end()))) {
                    s += ", \"buffers\": "; JEsc(s, name);
                } else cx->error = true;
            }
        }
        // curvas de transform PROPIAS de la anim del objeto (pos/rot/escala/visible/render)
        if (!a->curves.empty()) {
            s += ", \"curves\": [";
            for (size_t c = 0; c < a->curves.size(); c++) { if (c) s += ","; WriteCurve(s, a->curves[c]); }
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
// sincronia con BuildLayout (import_w3d.cpp) y LayoutCreateViewport (menu de
// tipos): un viewport nuevo se agrega en los TRES lados.
static const char* LayoutLeafName(int kind) {
    switch (kind) {
        case 1: return "Viewport3D";
        case 2: return "Outliner";
        case 3: return "Properties";
        case 4: return "UVEditor";
        case 5: return "Timeline";
        case 6: return "Editor2D";
        case 7: return "Console";
        case 8: return "IDE";
        case 9: return "Library";
    }
    return NULL;   // kind sin nombre todavia: el caller avisa y cae a Viewport3D
}

static void LayoutWriteNode(std::string& s, ViewportBase* v, int ind) {
    if (!v) return;   // hueco en un contenedor: BuildLayout lo rellena con un 3D
    int ck = v->ContainerKind();
    if (ck == 1 || ck == 2) {
        ViewportBase* a; ViewportBase* b; float split;
        if (ck == 1) { ViewportRow* r = (ViewportRow*)v; a = r->childA; b = r->childB; split = r->splitFrac; }
        else         { ViewportColumn* c = (ViewportColumn*)v; a = c->childA; b = c->childB; split = c->splitFrac; }
        JSang(s, ind); s += (ck == 1) ? "ViewportRow {\n" : "ViewportColumn {\n";
        LayoutWriteNode(s, a, ind + 1);
        LayoutWriteNode(s, b, ind + 1);
        JSang(s, ind + 1); s += "Split: "; JNum(s, split); s += "\n";
        JSang(s, ind); s += "}\n";
        return;
    }
    const char* name = LayoutLeafName(v->ViewportKind());
    if (!name) {
        // viewport nuevo sin nombre de guardado: no se aborta el layout, va un
        // 3D en su lugar y queda el aviso (agregarlo a LayoutLeafName)
        w3dLogfW("GuardarW3D: viewport kind=%d sin nombre de guardado, va como Viewport3D", v->ViewportKind());
        name = "Viewport3D";
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
        JSang(s, ind); s += name; s += " {\n";
        JSang(s, ind + 1); s += "posX: "; JNum(s, v3->pivot.x); s += "\n";
        JSang(s, ind + 1); s += "posY: "; JNum(s, v3->pivot.z); s += "\n";
        JSang(s, ind + 1); s += "posZ: "; JNum(s, v3->pivot.y); s += "\n";
        JSang(s, ind + 1); s += "orbitDistance: "; JNum(s, v3->orbitDistance); s += "\n";
        JSang(s, ind + 1); s += "rotQX: "; JNum(s, v3->viewRot.x); s += "\n";
        JSang(s, ind + 1); s += "rotQY: "; JNum(s, v3->viewRot.y); s += "\n";
        JSang(s, ind + 1); s += "rotQZ: "; JNum(s, v3->viewRot.z); s += "\n";
        JSang(s, ind + 1); s += "rotQW: "; JNum(s, v3->viewRot.w); s += "\n";
        // OVERLAYS POR TIPO (submenu Overlays > Objects): el lector ya los entendia pero no se escribian, asi
        // que apagar el overlay del esqueleto (o de las luces...) se perdia al guardar. Solo si difieren del
        // default del viewport: un layout comun sale igual que antes.
        // "showOverlays" (el ojo de Overlays) tampoco se escribia: un proyecto guardado con los overlays apagados
        // volvia a abrir con ellos prendidos, y como el Play respeta ese tilde, el juego del editor tambien.
        struct { const char* k; bool v, def; } ov[] = {
            { "showOverlays", v3->showOverlays, true },
            { "showArmature", v3->showArmature, true }, { "showLights", v3->showLights, true },
            { "showCamera", v3->showCamera, true }, { "showEmpty", v3->showEmpty, true },
            { "showParticles", v3->showParticles, true }, { "showCurves", v3->showCurves, true },
            { "showHitbox", v3->showHitbox, true }, { "showSounds", v3->showSounds, true },
            { "ShowRelationshipsLines", v3->ShowRelationshipsLines, true },
            // el PISO, los EJES X / Y, los ORIGENES y el CURSOR 3D: el lector los entendia y tampoco se escribian (apagar el
            // piso o los ejes volvia a prenderse al abrir el proyecto)
            { "showFloor", v3->showFloor, true }, { "showXaxis", v3->showXaxis, true }, { "showYaxis", v3->showYaxis, true },
            { "showOrigins", v3->showOrigins, true }, { "show3DCursor", v3->show3DCursor, true } };
        for (size_t k = 0; k < sizeof(ov) / sizeof(ov[0]); k++)
            if (ov[k].v != ov[k].def) { JSang(s, ind + 1); s += ov[k].k; s += ": "; s += ov[k].v ? "true" : "false"; s += "\n"; }
        // MIRAR POR LA CAMARA, el MODO de sombreado y el FAR: el lector (ApplyViewport3DProps) ya los entendia pero
        // no se escribian -> un layout de dos vistas (una por la camara en Rendered, otra libre para ver aparecer y
        // desaparecer el culling) volvia como dos vistas iguales. Solo si difieren del default del viewport.
        if (v3->ViewFromCameraActive) { JSang(s, ind + 1); s += "ViewFromCameraActive: true\n"; }
        if (v3->OverlayFps) { JSang(s, ind + 1); s += "OverlayFps: true\n"; }   // (el contador de fps del overlay)
        if (v3->view != RenderType::MaterialPreview) {
            static const char* kView[] = { "Solid", "MaterialPreview", "Rendered", "ZBuffer", "Wireframe", "NormalView", "Alpha", "MotionVectors" };
            const int vi = (int)v3->view;
            if (vi >= 0 && vi < 8) { JSang(s, ind + 1); s += "view: "; s += kView[vi]; s += "\n"; }
        }
        if (v3->farClip != 1000.0f) { JSang(s, ind + 1); s += "farClip: "; JNum(s, v3->farClip); s += "\n"; }
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
    //  POR QUE AL .w3d Y NO AL config.start: es la misma frontera de siempre. El
    //  encuadre del timeline habla de LOS FRAMES DE ESTE PROYECTO (donde estan sus
    //  keyframes, que tramo estabas mirando); en otro archivo no significa nada,
    //  igual que el frame actual y la seleccion del bloque "sesion". Lo que va al
    //  config.start son los MODOS DE TRABAJO que acompanan al usuario de proyecto en
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
    //        DopeIdOwner en Timeline.h). Guardar eso en un archivo seria guardar
    //        una clave que al reabrir ya no apunta a nada.
    //  EL EJE VERTICAL SOLO EN CURVAS: el dope sheet no tiene eje de valor, asi que
    //  en dope no se escriben zoomValor/centroValor. Escribirlos igual seria
    //  inventarle al usuario un encuadre vertical que nunca eligio; sin ellos, la
    //  primera vez que entre a curvas manda el auto-encuadre, que es justo lo que
    //  ya hace hoy.
    if (v->ViewportKind() == 5) {
        Timeline* tl = (Timeline*)v;
        JSang(s, ind); s += name; s += " {\n";
        JSang(s, ind + 1); s += "mode: ";
        s += (tl->mode == Timeline::TL_MODE_CURVES) ? "curves" : "dope";
        s += "\n";
        JSang(s, ind + 1); s += "zoomFrame: ";  JNum(s, tl->pxPerFrame);  s += "\n"; // px por frame
        JSang(s, ind + 1); s += "frameLeft: ";   JNum(s, tl->viewStartF);  s += "\n"; // frame del borde izquierdo
        if (tl->mode == Timeline::TL_MODE_CURVES) {
            JSang(s, ind + 1); s += "zoomValue: ";   JNum(s, tl->pxPerUnit);   s += "\n"; // px por unidad de valor
            JSang(s, ind + 1); s += "centerValue: "; JNum(s, tl->viewCenterV); s += "\n"; // valor en el centro
        }
        JSang(s, ind); s += "}\n";
        return;
    }
    // ---- LA VISTA DEL OUTLINER (por recursos) VIAJA CON EL LAYOUT ----------------
    //  Como el modo del timeline: es de ESA hoja (puede haber dos outliners, uno en la
    //  escena y otro en los materiales). Va por la CLAVE ("mallas"), no por el numero.
    //  En la vista Escena (la de siempre) no se escribe nada: un layout comun sale igual.
    //  La BIBLIOTECA (kind 9) siempre lleva su vista (el proyecto o una libreria), el filtro y lista / cuadricula. El
    //  outliner de la escena (kind 2) solo la lleva si el harness lo paso a los recursos (al leerlo sale una Biblioteca)
    if ((v->ViewportKind() == 2 && ((Outliner*)v)->view != OUT_VIEW_SCENE) || v->ViewportKind() == 9) {
        const Outliner* o = (const Outliner*)v;
        JSang(s, ind); s += name; s += " {\n";
        JSang(s, ind + 1); s += "view: "; s += o->ViewKey(); s += "\n";
        if (o->filter > W3D_VIEW_SCENE) { JSang(s, ind + 1); s += "filter: "; s += W3dViewKey(o->filter); s += "\n"; }
        if (v->ViewportKind() == 9) { JSang(s, ind + 1); s += o->grid ? "grid: 1\n" : "grid: 0\n"; }
        else if (o->grid) { JSang(s, ind + 1); s += "grid: 1\n"; }
        JSang(s, ind); s += "}\n";
        return;
    }
    JSang(s, ind); s += name; s += "\n";
}

// el layout ACTUAL entero como snippet "Layout { ... }" (lo que entiende
// BuildLayout). "" si no hay arbol (guardado sin editor arriba).
static std::string LayoutSerializeLive() {
    if (!rootViewport) return std::string();
    std::string s = "Layout {\n";
    LayoutWriteNode(s, rootViewport, 1);
    s += "}";
    return s;
}
// el de un arbol cualquiera (los layouts guardados del programa: config/W3dLayouts.h, y el de cada orientacion del
// telefono). NULL = el actual entero (si hay uno maximizado, el arbol completo)
std::string W3dLayoutTextOf(ViewportBase* root) {
    if (!root) { extern ViewportBase* LayoutCompleteRoot(); root = LayoutCompleteRoot(); }
    if (!root) return std::string();
    std::string s = "Layout {\n";
    LayoutWriteNode(s, root, 1);
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
//  usuario de proyecto en proyecto -> ese vive en el config.start (ver variables.h).
//  El MODO del timeline (dope/curvas) va con SU hoja del layout, no aca: puede
//  haber mas de un timeline abierto y cada uno tiene el suyo.
//
//  LA SELECCION VA POR NOMBRE, no por indice: los nombres son UNICOS por escena
//  (regla ya vigente) y un indice se corre en cuanto alguien edita el .w3d o el
//  orden de los hijos cambia. Al leer, un nombre que no aparece se IGNORA: la
//  seleccion es un comodidad, nunca un motivo para no abrir el proyecto.
//
//  Solo se nombran objetos que esten COLGANDO DE LA ESCENA: ActiveObj puede
//  apuntar a algo que no es parte del arbol que se guarda, y entonces el nombre
//  que quedaria escrito no lo va a poder resolver nadie.
// ---------------------------------------------------------------------------
static bool InScene(Object* root, Object* o) {
    if (!root || !o) return false;
    if (root == o) return true;
    for (size_t i = 0; i < root->Childrens.size(); i++)
        if (InScene(root->Childrens[i], o)) return true;
    return false;
}

static void WriteSession(std::string& s) {
    s += "  \"session\": {\n";
    // JInt y no JNum((float)f): de 1e6 para arriba el float sale en notacion cientifica
    // ("1.23457e+06") y ese texto ya no es el entero que era (ver JInt)
    s += "    \"frame\": "; JInt(s, CurrentFrame); s += ",\n";
    s += "    \"active\": ";
    JEsc(s, (ActiveObj && InScene(SceneCollection, ActiveObj)) ? ActiveObj->name : std::string());
    s += ",\n";
    s += "    \"selection\": [";
    {
        bool first = true;
        for (size_t i = 0; i < ObjSelects.size(); i++) {
            Object* o = ObjSelects[i];
            if (!o || !InScene(SceneCollection, o)) continue;
            if (!first) s += ", ";
            JEsc(s, o->name);
            first = false;
        }
    }
    s += "]";
    // la RAIZ que se esta editando, si no es la escena del bloque (la seleccion de arriba es suya).
    // Ausente = la del bloque: un proyecto de una sola escena sale como siempre.
    {
        const int act = W3dActiveRoot(), blq = W3dRootBlock();
        if (act >= 0 && act != blq && act < (int)W3dRoots().size()) {
            const W3dRootRow& f = W3dRoots()[(size_t)act];
            s += ",\n    \"root\": "; JEsc(s, f.name);
            s += ", \"rootType\": "; JEsc(s, W3dRootTypeKey(W3dRootTypeOf(act)));
        }
    }
    // QUE MIRA LA LINEA DE TIEMPO, si no es una animacion de escena (esa ya la nombra "animaciones"."activa"): un clip
    // de un armature (cual clip: su "activeAnim"), la animacion de una malla, un clip de su armature 2D o un flipbook.
    // Sin esto, al abrir volvia a la animacion de escena: con cientos de clips habia que buscar otra vez el que se
    // estaba editando. Por NOMBRE (como "activo"); el de una malla, ademas, su indice
    {
        extern Flipbook* g_flipActive; extern bool g_animPickedSceneInGame;
        const char* type = NULL; std::string obj; int clip = -1;
        if (ActiveAnimKind == 1 && ActiveAnimArm && InScene(SceneCollection, ActiveAnimArm)) {
            type = "armature"; obj = ActiveAnimArm->name;
        } else if (ActiveAnimKind == 3 && ActiveAnimMesh && InScene(SceneCollection, ActiveAnimMesh)) {
            VertexAnimationActive* va = FindTargetAnim(ActiveAnimMesh);
            type = "object"; obj = ActiveAnimMesh->name; clip = va ? va->currentAnim : -1;
        } else if (ActiveAnimKind == 4 && ActiveAnimMesh && InScene(SceneCollection, ActiveAnimMesh)) {
            type = "armature2d"; obj = ActiveAnimMesh->name;
        } else if (ActiveAnimKind == 5 && g_flipActive) {
            type = "flipbook"; obj = g_flipActive->name;
        } else if (ActiveAnimKind == 0 && W3dActiveRootIsGame() && g_animPickedSceneInGame) {
            type = "scene";   // (un JUEGO abre en "Juego": el clip de escena que se ELIGIO para editar se nombra)
        }
        if (type) {
            s += ",\n    \"animType\": "; JEsc(s, std::string(type));
            s += ", \"animObject\": "; JEsc(s, obj);
            if (clip >= 0) { s += ", \"animClip\": "; JInt(s, clip); }
        }
    }
    s += "\n";
    s += "  },\n";
}

// ---------------------------------------------------------------------------
//  ARMATURE -> JSON (Fase 3). Esquema:
//    { "tipo": "armature", <comunes>, "autorado": true?, "activeAnim": N?,
//      "bones": [ { "nombre", "padre", "head": [xyz], "tail": [xyz],
//                   (solo rigs importados con rest:) "rest": true, "restT/R/S", "preRot", "postRot",
//                   "rotOrder", "bind": [16], "cluster": [16] } ],
//      "anims": [ { "nombre", "fps", "inicio", "fin",
//                   "tracks": [ { "hueso": N, "curvas": [ {p,c,k:[...]} ] } ] } ],
//      "hijos": [...] }
//  En un rig AUTORADO el rest se DERIVA de head/tail (PrepareAuthoredSkin al abrir) -> no se escribe.
//  En un rig importado (FBX/glTF) se escriben rest + bind/cluster para reconstruir el FK/skinning
//  (PrepareSkin al abrir) sin depender del archivo original.
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
//  CAPAS DEL MIX (W3dLayerAnim): la misma forma para las capas de un armature ("capas") y las de escena
//  ("mix".escenas). Solo se escriben los campos que difieren del default; el orden de la lista es el de la mezcla.
// ---------------------------------------------------------------------------
static void WriteLayers(std::string& s, const std::vector<W3dLayerAnim>& layers, int ind) {
    s += "[\n";
    for (size_t k = 0; k < layers.size(); k++) {
        const W3dLayerAnim& c = layers[k];
        JSang(s, ind + 1); s += "{ \"anim\": "; JEsc(s, c.anim);
        if (c.influence != 100.0f) { s += ", \"influence\": "; JNum(s, c.influence); }
        if (c.mode != 0) { s += ", \"mode\": "; s += (c.mode == 1) ? "\"add\"" : "\"subtract\""; }
        if (!c.visible) s += ", \"visible\": false";
        if (!c.bone.empty()) { s += ", \"bone\": "; JEsc(s, c.bone); }
        if (c.from != 0.0f) { s += ", \"from\": "; JNum(s, c.from); }
        if (c.vel != 1.0f)   { s += ", \"vel\": ";   JNum(s, c.vel); }
        if (!c.loop) s += ", \"loop\": false";
        s += " }";
        if (k + 1 < layers.size()) s += ",";
        s += "\n";
    }
    JSang(s, ind); s += "]";
}

// el bloque raiz "mix": el modo Mix del timeline, su rango y las capas de escena. Ausente = sin mix.
// LA TARJETA RENDER (Properties.cpp, g_render de render/RenderOptions.h): el
// formato, el tamano, los pases, la composicion final y el audio. Se escribe solo lo que no es el valor de fabrica (sin
// nada distinto, no hay bloque); la carpeta, solo si no es la de salida por defecto (una ruta de ESTA maquina)
static void WriteRender(std::string& s, CtxSave* cx) {
    const W3dRenderOpt d;
    const W3dRenderOpt& r = g_render;
    std::string c;
    struct E {
        static void F(std::string& c, const char* k, float v) { c += ", \""; c += k; c += "\": "; JNum(c, v); }
        static void B(std::string& c, const char* k, bool v) { c += ", \""; c += k; c += "\": "; c += v ? "true" : "false"; }
        static void I(std::string& c, const char* k, int v) { c += ", \""; c += k; c += "\": "; JInt(c, v); }
        static void Tx(std::string& c, const char* k, const std::string& v) { c += ", \""; c += k; c += "\": "; JEsc(c, v); }
    };
    if (!r.folder.empty() && r.folder != w3dFileSystem::GetDefaultOutputDir()) E::Tx(c, "folder", r.folder);
    if (r.name != d.name) E::Tx(c, "name", r.name);
    if (r.w != d.w) E::F(c, "width", r.w);
    if (r.h != d.h) E::F(c, "height", r.h);
    if (r.format != d.format) E::I(c, "format", r.format);
    if (r.quality != d.quality) E::F(c, "quality", r.quality);
    if (r.zbuffer != d.zbuffer) E::B(c, "zbuffer", r.zbuffer);
    if (r.normal != d.normal) E::B(c, "normal", r.normal);
    if (r.alpha != d.alpha) E::B(c, "alpha", r.alpha);
    if (r.vectors != d.vectors) E::B(c, "vectors", r.vectors);
    if (r.blur != d.blur) E::B(c, "blur", r.blur);
    if (r.shutter != d.shutter) E::F(c, "shutter", r.shutter);
    if (r.final != d.final) E::B(c, "final", r.final);
    if (r.finalAlpha != d.finalAlpha) E::B(c, "finalAlpha", r.finalAlpha);
    if (r.post != d.post) E::B(c, "post", r.post);
    if (r.sound != d.sound) E::B(c, "sound", r.sound);
    if (r.audioFormat != d.audioFormat) E::I(c, "audio", r.audioFormat);
    if (r.audioKbps != d.audioKbps) E::F(c, "audioKbps", r.audioKbps);
    if (r.videoCodec != d.videoCodec) E::I(c, "videoCodec", r.videoCodec);
    if (r.videoKbps != d.videoKbps) E::F(c, "videoKbps", r.videoKbps);
    if (r.antialias != d.antialias) E::I(c, "antialias", r.antialias);
    // EL FONDO (antes no se guardaba: volvia a negro al abrir): el color, color o textura 360, la rotacion y las texturas
    // que se usaron de fondo (la pista animable de QUE fondo las nombra por su lugar en esta lista)
    if (g_renderBg[0] != 0.0f || g_renderBg[1] != 0.0f || g_renderBg[2] != 0.0f) {
        c += ", \"background\": ["; JNum(c, g_renderBg[0]); c += ", "; JNum(c, g_renderBg[1]); c += ", "; JNum(c, g_renderBg[2]); c += "]"; }
    if (g_renderBgMode != 0) E::I(c, "backgroundMode", g_renderBgMode);
    if (g_renderLightBackground != 1.0f) E::F(c, "backgroundLight", g_renderLightBackground);   // (la luz del fondo; ausente = 100%)
    // LAS SOMBRAS EN TIEMPO REAL (la tarjeta de abajo de Render): lo que no es de fabrica
    { const W3dShadowsOpt so = g_shadows, sd;
      if (so.on != sd.on) E::B(c, "shadows", so.on);
      if (so.distance != sd.distance) E::F(c, "shadowDistance", so.distance);
      if (so.cascades != sd.cascades) E::I(c, "shadowCascades", so.cascades);
      if (so.cuts[0] != sd.cuts[0] || so.cuts[1] != sd.cuts[1] || so.cuts[2] != sd.cuts[2]) {
          c += ", \"shadowSplits\": ["; JNum(c, so.cuts[0]); c += ", "; JNum(c, so.cuts[1]); c += ", "; JNum(c, so.cuts[2]); c += "]"; }
      if (so.border != sd.border) E::F(c, "shadowBorder", so.border);
      if (so.blur != sd.blur) E::F(c, "shadowBlur", so.blur);
      if (so.resolution != sd.resolution) E::I(c, "shadowResolution", so.resolution);
      if (so.smooth != sd.smooth) E::B(c, "softShadows", so.smooth);
      if (so.intensity != sd.intensity) E::F(c, "shadowIntensity", so.intensity);
      if (so.bias != sd.bias) E::F(c, "shadowBias", so.bias);
      if (so.color[0] != sd.color[0] || so.color[1] != sd.color[1] || so.color[2] != sd.color[2]) {
          c += ", \"shadowColor\": ["; JNum(c, so.color[0]); c += ", "; JNum(c, so.color[1]); c += ", "; JNum(c, so.color[2]); c += "]"; } }
    if (g_renderBgRot[0] != 0.0f || g_renderBgRot[1] != 0.0f || g_renderBgRot[2] != 0.0f) {
        c += ", \"backgroundRot\": ["; JNum(c, g_renderBgRot[0]); c += ", "; JNum(c, g_renderBgRot[1]); c += ", "; JNum(c, g_renderBgRot[2]); c += "]"; }
    if (!g_renderBgTexs.empty()) {
        c += ", \"backgroundTextures\": [";
        for (size_t i = 0; i < g_renderBgTexs.size(); i++) {
            if (i) c += ", ";
            Texture* t = g_renderBgTexs[i];
            if (t && !t->path.empty()) JEsc(c, Asset(cx, t->path)); else JEsc(c, std::string());
        }
        c += "]";
        if (W3dBackgroundTextureIndex(g_renderBgTex)) E::I(c, "backgroundTexture", W3dBackgroundTextureIndex(g_renderBgTex));
    }
    if (c.empty()) return;
    s += "  \"render\": { "; s += c.substr(2); s += " },\n";
}

static void WriteMix(std::string& s) {
    if (!g_animMix && g_mixScenes.empty() && g_mixStart == 1.0f && g_mixEnd == 250.0f) return;
    s += "  \"mix\": {\n";
    s += "    \"active\": "; s += g_animMix ? "true" : "false"; s += ",\n";
    s += "    \"start\": "; JNum(s, g_mixStart); s += ", \"end\": "; JNum(s, g_mixEnd);
    if (!g_mixScenes.empty()) { s += ",\n    \"scenes\": "; WriteLayers(s, g_mixScenes, 2); }
    s += "\n  },\n";
}

static void WriteArmature(std::string& s, Armature* a, int ind) {
    if (a->skinAuthored) { s += ",\n"; JSang(s, ind); s += "\"authored\": true"; }
    // rig glTF (FK estandar Y-up, skinMatrix = world * inverseBindMatrix): sin este flag, al reabrir caia
    // por el camino FBX (NodeToYup) -> huesos acostados 90 grados y skinning roto
    if (a->skinGltf && !a->skinAuthored) { s += ",\n"; JSang(s, ind); s += "\"gltf\": true"; }
    if (a->activeAnim >= 0) { s += ",\n"; JSang(s, ind); s += "\"activeAnim\": "; JNum(s, (float)a->activeAnim); }
    s += ",\n"; JSang(s, ind); s += "\"bones\": [\n";
    for (size_t i = 0; i < a->bones.size(); i++) {
        const W3dBone& b = a->bones[i];
        JSang(s, ind + 1); s += "{ \"name\": "; JEsc(s, b.name);
        s += ", \"parent\": "; JNum(s, (float)b.parent);
        s += ", \"head\": "; JVec(s, b.head.x, b.head.y, b.head.z);
        s += ", \"tail\": "; JVec(s, b.tail.x, b.tail.y, b.tail.z);
        // "Disconnect Bone": emparentado SIN soldar. Solo se escribe cuando aplica (round-trip
        // estable; los .w3d viejos sin el campo cargan conectado=true = comportamiento de siempre)
        if (b.parent >= 0 && !b.connected) s += ", \"loose\": true";
        if (b.hasRest && !a->skinAuthored) { // rig importado: fidelidad del FK/skinning
            s += ", \"rest\": true";
            s += ", \"restT\": "; JVec(s, b.restT.x, b.restT.y, b.restT.z);
            s += ", \"restR\": "; JVec(s, b.restR.x, b.restR.y, b.restR.z);
            s += ", \"restS\": "; JVec(s, b.restS.x, b.restS.y, b.restS.z);
            s += ", \"preRot\": "; JVec(s, b.preRot.x, b.preRot.y, b.preRot.z);
            s += ", \"postRot\": "; JVec(s, b.postRot.x, b.postRot.y, b.postRot.z);
            s += ", \"rotOrder\": "; JNum(s, (float)b.rotOrder);
            s += ", \"bind\": "; JMat(s, b.bind);
            s += ", \"cluster\": "; JMat(s, b.clusterTransform);
            if (a->skinGltf) { s += ", \"invBind\": "; JMat(s, b.skinInvBind); } // la inverseBindMatrix del glTF
        }
        s += " }";
        if (i + 1 < a->bones.size()) s += ",";
        s += "\n";
    }
    JSang(s, ind); s += "]";
    if (!a->layers.empty()) { s += ",\n"; JSang(s, ind); s += "\"layers\": "; WriteLayers(s, a->layers, ind); }
    // LOS CLIPS van a un ANIMSET (animations/<slug>.w3da, ver SaveAnimSets.h): aca solo el
    // nombre del animset y que clips usa de el. El INLINE de abajo queda para el harness
    // (g_w3dAnimsInline: archivos "de antes" para probar la migracion) y como red: un armature
    // que el plan no vio no pierde sus clips.
    std::string animSet; std::vector<int> clipsIdx;
    if (gDry) {
        // (en seco: los clips los COMPARTE la generacion con los de la plantilla -W3dPrefabFixHook-: N
        //  instancias de un prefab sin guardar usan los mismos clips en memoria, como un Alt+D)
    } else if (!g_w3dAnimsInline && SaveAnimSetsOf(a, animSet, clipsIdx)) {
        s += ",\n"; JSang(s, ind); s += "\"anims\": "; JEsc(s, animSet);
        s += ",\n"; JSang(s, ind); s += "\"clips\": [";
        for (size_t k = 0; k < clipsIdx.size(); k++) { if (k) s += ", "; JInt(s, clipsIdx[k]); }
        s += "]";
    } else if (!a->animations.empty()) {
        s += ",\n"; JSang(s, ind); s += "\"anims\": [\n";
        for (size_t i = 0; i < a->animations.size(); i++) {
            SkeletalAnimation* an = a->animations[i];
            JSang(s, ind + 1); s += "{ \"name\": "; JEsc(s, an ? an->name : std::string("clip"));
            if (an) {
                s += ", \"fps\": "; JNum(s, (float)an->FrameRate);
                s += ", \"start\": "; JNum(s, (float)an->startFrame);
                s += ", \"end\": "; JNum(s, (float)an->endFrame);
                if (!an->tracks.empty()) {
                    s += ", \"tracks\": [";
                    for (size_t t = 0; t < an->tracks.size(); t++) {
                        if (t) s += ",";
                        s += " { \"bone\": "; JNum(s, (float)an->tracks[t].bone);
                        s += ", \"curves\": [";
                        for (size_t c = 0; c < an->tracks[t].Properties.size(); c++) {
                            if (c) s += ",";
                            WriteCurve(s, an->tracks[t].Properties[c]);
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
static void WriteRigMesh(std::string& s, Mesh* m, int ind) {
    Modifier* mod = NULL;
    for (size_t i = 0; i < m->modifiers.size(); i++)
        if (m->modifiers[i] && m->modifiers[i]->type == ModifierType::Armature) { mod = m->modifiers[i]; break; }
    if (mod && mod->target) {
        s += ",\n"; JSang(s, ind); s += "\"modArmature\": "; JEsc(s, mod->target->name);
    }
    // ARMATURES 2D DEL MESH (huesos en espacio UV; binding POR NOMBRE con los uvgroups de abajo).
    // Viajan como campo del objeto malla, junto a vgroups/modArmature. FORMATO NUEVO (multi-rig):
    //   "armatures2d": [ { "nombre":..., "huesos":[...], "anims":[...], "activeAnim":N }, ... ]
    // El formato VIEJO (un solo rig suelto en "armature2d" + "anims2d" + "anim2dActiva") se sigue
    // LEYENDO (import_w3d lo migra al armature 0) pero ya no se escribe.
    // "pose" solo si no es identidad (round-trip estable: lo que no se escribe no puede diferir
    // al re-guardar).
    if (m->HasArm2D()) {
        s += ",\n"; JSang(s, ind); s += "\"armatures2d\": [\n";
        for (size_t a = 0; a < m->armatures2d.size(); a++) {
            const Armature2D* arm = m->armatures2d[a];
            JSang(s, ind + 1); s += "{ \"name\": "; JEsc(s, arm ? arm->name : std::string("Armature 2D"));
            if (arm) {
                s += ",\n"; JSang(s, ind + 2); s += "\"bones\": [\n";
                for (size_t b = 0; b < arm->bones.size(); b++) {
                    const W3dBone2D& hb = arm->bones[b];
                    JSang(s, ind + 3); s += "{ \"name\": "; JEsc(s, hb.name);
                    s += ", \"parent\": "; JNum(s, (float)hb.parent);
                    s += ", \"head\": ["; JNum(s, hb.headU); s += ", "; JNum(s, hb.headV); s += "]";
                    s += ", \"tail\": ["; JNum(s, hb.tailU); s += ", "; JNum(s, hb.tailV); s += "]";
                    // "Disconnect Bone": emparentado SIN soldar (solo si aplica, como el armature 3D)
                    if (hb.parent >= 0 && !hb.connected) s += ", \"loose\": true";
                    if (!hb.PoseIdentity()) {
                        s += ", \"pose\": ["; JNum(s, hb.poseTU); s += ", "; JNum(s, hb.poseTV);
                        s += ", "; JNum(s, hb.poseRot); s += ", "; JNum(s, hb.poseSX);
                        s += ", "; JNum(s, hb.poseSY); s += "]";
                    }
                    s += " }";
                    if (b + 1 < arm->bones.size()) s += ",";
                    s += "\n";
                }
                JSang(s, ind + 2); s += "]";
                // CLIPS de ESTE armature (curvas por hueso). MISMO formato que el "anims" del
                // armature 3D -reusa WriteCurve sin tocarla- pero con los 5 canales 2D:
                // (AnimPosition,X/Y) = U/V, (AnimRotation,X) = grados, (AnimScale,X/Y).
                if (!arm->anims.empty()) {
                    s += ",\n"; JSang(s, ind + 2); s += "\"anims\": [\n";
                    for (size_t i = 0; i < arm->anims.size(); i++) {
                        Armature2DAnimation* an = arm->anims[i];
                        JSang(s, ind + 3); s += "{ \"name\": "; JEsc(s, an ? an->name : std::string("Anim2D"));
                        if (an) {
                            s += ", \"fps\": "; JNum(s, (float)an->fps);
                            s += ", \"start\": "; JNum(s, (float)an->startFrame);
                            s += ", \"end\": "; JNum(s, (float)an->endFrame);
                            // CURVAS VACIAS: no se escriben. Un track puede tener AnimProperty sin
                            // ningun keyframe (las creaba la evaluacion vieja, y tambien quedan al
                            // borrar el ultimo keyframe de un canal); guardarlas engorda el .w3d y
                            // hace ruido en el dope sheet al reabrir. Un track que queda SIN curvas
                            // tampoco se escribe.
                            bool hasTracks = false;
                            for (size_t t = 0; t < an->tracks.size() && !hasTracks; t++)
                                for (size_t c = 0; c < an->tracks[t].Properties.size() && !hasTracks; c++)
                                    if (!an->tracks[t].Properties[c].keyframes.empty()) hasTracks = true;
                            if (hasTracks) {
                                s += ", \"tracks\": [";
                                bool firstTrack = true;
                                for (size_t t = 0; t < an->tracks.size(); t++) {
                                    bool hasCurves = false;
                                    for (size_t c = 0; c < an->tracks[t].Properties.size() && !hasCurves; c++)
                                        if (!an->tracks[t].Properties[c].keyframes.empty()) hasCurves = true;
                                    if (!hasCurves) continue;
                                    if (!firstTrack) s += ",";
                                    firstTrack = false;
                                    s += " { \"bone\": "; JNum(s, (float)an->tracks[t].bone);
                                    s += ", \"curves\": [";
                                    bool firstCurve = true;
                                    for (size_t c = 0; c < an->tracks[t].Properties.size(); c++) {
                                        if (an->tracks[t].Properties[c].keyframes.empty()) continue;
                                        if (!firstCurve) s += ",";
                                        firstCurve = false;
                                        WriteCurve(s, an->tracks[t].Properties[c]);
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
                    if (arm->activeAnim >= 0) { s += ",\n"; JSang(s, ind + 2); s += "\"activeAnim\": "; JNum(s, (float)arm->activeAnim); }
                }
                s += "\n"; JSang(s, ind + 1);
            }
            s += "}";
            if (a + 1 < m->armatures2d.size()) s += ",";
            s += "\n";
        }
        JSang(s, ind); s += "]";
        // el armature ACTIVO solo si NO es el 0 (asi un proyecto de 1 rig no engorda ni cambia)
        if (m->armature2dActive > 0) { s += ",\n"; JSang(s, ind); s += "\"activeArmature2d\": "; JNum(s, (float)m->armature2dActive); }
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
static const char* ModTypeStr(int t) {
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

static void WriteModifiers(std::string& s, Mesh* m, int ind) {
    bool any = false;
    for (size_t i = 0; i < m->modifiers.size(); i++)
        if (m->modifiers[i] && ModTypeStr(m->modifiers[i]->type)) { any = true; break; }
    if (!any) return;
    s += ",\n"; JSang(s, ind); s += "\"modifiers\": [\n";
    bool first = true;
    for (size_t i = 0; i < m->modifiers.size(); i++) {
        Modifier* md = m->modifiers[i];
        const char* tn = md ? ModTypeStr(md->type) : NULL;
        if (!tn) continue;
        if (!first) s += ",\n";
        first = false;
        JSang(s, ind + 1); s += "{ \"type\": "; JEsc(s, tn);
        s += ", \"name\": "; JEsc(s, md->name);
        if (!md->showViewport) s += ", \"viewport\": false";
        if (!md->showEdit)     s += ", \"edit\": false";
        if (md->type == ModifierType::Mirror) {
            s += ", \"axes\": [";
            s += md->axisX ? "true" : "false"; s += ", ";
            s += md->axisY ? "true" : "false"; s += ", ";
            s += md->axisZ ? "true" : "false"; s += "]";
            if (md->target) { s += ", \"target\": "; JEsc(s, md->target->name); }
            s += ", \"merge\": "; s += md->merge ? "true" : "false";
            s += ", \"mergeDist\": "; JNum(s, md->mergeDist);
            s += ", \"clipping\": "; s += md->clipping ? "true" : "false";
        } else if (md->type == ModifierType::Array) {
            s += ", \"copies\": "; JNum(s, md->arrCount);
            s += ", \"relative\": "; s += md->arrRelOn ? "true" : "false";
            s += ", \"rel\": ["; JNum(s, md->arrRelX); s += ", "; JNum(s, md->arrRelY); s += ", "; JNum(s, md->arrRelZ); s += "]";
            s += ", \"constant\": "; s += md->arrConstOn ? "true" : "false";
            s += ", \"const\": ["; JNum(s, md->arrConstX); s += ", "; JNum(s, md->arrConstY); s += ", "; JNum(s, md->arrConstZ); s += "]";
            if (md->target) { s += ", \"target\": "; JEsc(s, md->target->name); }
            s += ", \"merge\": "; s += md->arrMerge ? "true" : "false";
            s += ", \"mergeDist\": "; JNum(s, md->arrMergeDist);
        } else if (md->type == ModifierType::SubdivisionSurface) {
            s += ", \"level\": "; JNum(s, md->subLevel);
            s += ", \"renderLevel\": "; JNum(s, md->subRenderLevel);
            s += ", \"simple\": "; s += md->subSimple ? "true" : "false";
        } else if (md->type == ModifierType::Screw) {
            s += ", \"angle\": "; JNum(s, md->screwAngle);
            s += ", \"height\": "; JNum(s, md->screwHeight);
            s += ", \"steps\": "; JNum(s, md->screwSteps);
            s += ", \"renderSteps\": "; JNum(s, md->screwRenderSteps);
            s += ", \"axis\": "; JNum(s, (float)md->screwAxis);
            s += ", \"stretchU\": "; s += md->screwStretchU ? "true" : "false";
            s += ", \"stretchV\": "; s += md->screwStretchV ? "true" : "false";
            s += ", \"smooth\": "; s += md->screwSmooth ? "true" : "false";
            s += ", \"weld\": "; s += md->screwMerge ? "true" : "false";
            s += ", \"flip\": "; s += md->screwFlip ? "true" : "false";
        } else if (md->type == ModifierType::CullingTri) {
            // Culling por triangulo: el metodo, el sector/celda activo y el NOMBRE del
            // sidecar. Los datos no se copian al .w3d (son megabytes de indices): el
            // sidecar (JSON de sectores o `.w3dvis`) se INGIERE al contenedor como
            // cualquier otro asset y se relee de ahi. metodo 1 se escribe "celdas"
            // (el viejo "bsp" nunca corto nada; al releer, "bsp" cae a "celdas").
            s += ", \"method\": "; JEsc(s, std::string(md->methodPVS == 1 ? "cells" : "triangles"));
            s += ", \"sector\": "; JNum(s, (float)md->sectorPVS);
            // fallback de celda vacia (-1 = completa, N = celda N). Solo si se usa,
            // para que los proyectos que no lo declaran queden byte a byte como antes.
            if (md->sectorFallback != 0) { s += ", \"sectorFallback\": "; JNum(s, (float)md->sectorFallback); }
            if (!md->pvsFile.empty()) { s += ", \"pvs\": "; JEsc(s, md->pvsFile); }
            if (!md->visFile.empty()) { s += ", \"vis\": "; JEsc(s, md->visFile); }
            // path del recorrido (el motor elige la celda solo) + desde que ojo se mide
            if (!md->pathName.empty()) { s += ", \"path\": "; JEsc(s, md->pathName); }
            if (!md->onlyCameraActive)   { s += ", \"activeCameraOnly\": false"; }
            // ramas habilitadas del path ("1101" = rama 2 apagada); solo si alguna esta OFF
            { bool hasOff = false;
              for (size_t r = 0; r < md->branchesOn.size(); r++) if (!md->branchesOn[r]) { hasOff = true; break; }
              if (hasOff) {
                  std::string mask; for (size_t r = 0; r < md->branchesOn.size(); r++) mask += md->branchesOn[r] ? '1' : '0';
                  s += ", \"branches\": "; JEsc(s, mask);
              } }
        }
        else if (md->type == ModifierType::Boolean) {
            // la operacion viaja como STRING (misma regla que el tipo: el enum es orden de codigo);
            // el target POR NOMBRE, como el del Mirror (se resuelve al final de la carga)
            s += ", \"op\": "; JEsc(s, std::string(md->boolOp == 0 ? "intersect" : md->boolOp == 1 ? "union" : "difference"));
            if (md->target) { s += ", \"target\": "; JEsc(s, md->target->name); }
        }
        // Array: todavia sin params ni generacion (se guarda tipo/nombre para no perder el stack)
        s += " }";
    }
    s += "\n"; JSang(s, ind); s += "]";
}

// ---------------------------------------------------------------------------
//  EL STACK DE CONSTRAINTS (objects/W3dConstraint.h): una PROPIEDAD de CUALQUIER
//  objeto, no un tipo de objeto.
//
//  El 'tipo' viaja como STRING y NUNCA como el numero del enum. Es la misma regla
//  que ModTypeStr y no es cosmetica: el enum es orden de codigo, asi que meter un
//  tipo nuevo en el MEDIO de W3dConstraintType re-interpretaria en silencio todos
//  los archivos ya guardados (un Billboard volveria como Copy Rotation). Con el
//  string, el que no se conoce se saltea y se dice.
//
//  UN OBJETO SIN CONSTRAINTS NO ESCRIBE NADA (ni la clave vacia): el 99% de los
//  objetos no tiene, y un proyecto que no los usa tiene que salir byte a byte
//  igual que antes de que esto existiera.
// ---------------------------------------------------------------------------
static const char* ConsTypeStr(int t) {
    switch (t) {
        case W3dConstraintType::CopyLocation: return "copyLocation";
        case W3dConstraintType::CopyRotation: return "copyRotation";
        case W3dConstraintType::Billboard:    return "billboard";
        case W3dConstraintType::ChildOf:      return "childOf";
    }
    return NULL;   // un tipo nuevo se agrega aca y en el lector (ConsTypeInt)
}

static void WriteConstraints(std::string& s, Object* o, int ind) {
    bool any = false;
    for (size_t i = 0; i < o->constraints.size(); i++)
        if (o->constraints[i] && ConsTypeStr(o->constraints[i]->type)) { any = true; break; }
    if (!any) return;
    s += ",\n"; JSang(s, ind); s += "\"constraints\": [\n";
    bool first = true;
    for (size_t i = 0; i < o->constraints.size(); i++) {
        W3dConstraint* c = o->constraints[i];
        const char* tn = c ? ConsTypeStr(c->type) : NULL;
        if (!tn) continue;
        if (!first) s += ",\n";
        first = false;
        JSang(s, ind + 1); s += "{ \"type\": "; JEsc(s, tn);
        s += ", \"name\": "; JEsc(s, c->name);
        if (!c->active) s += ", \"active\": false";     // el "ojito": solo se escribe apagado
        // "ver en modo edicion": el default es OFF, asi que solo se escribe PRENDIDO (mismo
        // criterio que arriba: la clave ausente = el default, y un .w3d tipico no la lleva)
        if (c->showEdit) s += ", \"vered\": true";
        s += ", \"influence\": "; JNum(s, c->influence);
        if (c->type == W3dConstraintType::Billboard) {
            s += ", \"yaw\": ";   s += c->bbYaw   ? "true" : "false";
            s += ", \"pitch\": "; s += c->bbPitch ? "true" : "false";
        } else {
            // LA FUENTE. El TIPO es explicito ("vista" / "objeto") y no se deduce de que
            // haya nombre: sin el, borrar el objeto fuente (que deja el puntero en NULL)
            // convertiria un Copy Location en un "seguir a la camara" al reabrir.
            const bool view = (c->sourceType == W3dConstraintSource::View);
            s += ", \"font\": "; JEsc(s, view ? std::string("view") : std::string("object"));
            if (!view) {
                // se prefiere el nombre del objeto VIVO: si lo renombraron sin pasar por el
                // rename del editor, el puntero es la verdad y el nombre cacheado el rezago.
                std::string fu = c->sourceObj ? c->sourceObj->name : c->sourceName;
                if (!fu.empty()) { s += ", \"object\": "; JEsc(s, fu); }
            }
            s += ", \"axes\": [";
            s += c->axisX ? "true" : "false"; s += ", ";
            s += c->axisY ? "true" : "false"; s += ", ";
            s += c->axisZ ? "true" : "false"; s += "]";}
        if (c->type == W3dConstraintType::ChildOf) {
            // el hueso (si la fuente es un armature), que se hereda (loc/rot/escala) y la inversa
            if (!c->bone.empty()) { s += ", \"bone\": "; JEsc(s, c->bone); }
            s += ", \"inherits\": [";
            s += c->coLoc ? "true" : "false"; s += ", ";
            s += c->coRot ? "true" : "false"; s += ", ";
            s += c->coEsc ? "true" : "false"; s += "]";
            s += ", \"inverse\": "; JMat(s, c->inverse);
        }
        s += " }";
    }
    s += "\n"; JSang(s, ind); s += "]";
}

static void WriteObject(std::string& s, Object* o, int ind, CtxSave* cx, bool* first,
                           const std::string* parent2d = NULL);
static void WriteClipsHierarchy(std::string& s, const Object* o, int ind);   // (definida con las animaciones de escena)

// un hijo que NO va en el .w3d: los ELEMENTOS 2D de una escena UI viven en SU
// .w3dui (UI2DFormato::WriteChildren). Escribirlos tambien aca los DUPLICARIA al
// reabrir (el .w3dui los crea y despues el .w3d los volveria a crear).
static bool ChildVaInOtherFile(Object* parent, Object* child) {
    return parent->getType() == ObjectType::ui && UI2D_IsElement2D(child);
}

// ---------------------------------------------------------------------------
//  FALLO B: LO QUE CUELGA DE UN ELEMENTO 2D NO SE PERDIA "UN POCO", SE PERDIA ENTERO
//
//  El .w3d corta la recursion en los elementos 2D (arriba) y el .w3dui escribe SOLO
//  tipos 2D (lista blanca en UI2DFormato::WriteChildren), asi que un objeto 3D
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
//  espacio de nombres de los objetos es POR ESCENA (W3dScopeNameOf) y la escena UI ES
//  un scope, asi que dentro de una UI no hay dos elementos con el mismo nombre.
//  El sentido contrario (un elemento 2D bajo un objeto 3D) NO llega hasta aca: se
//  bloquea en el reparent, con aviso (ver CrossingUIValid en ObjectMode.cpp).
// ---------------------------------------------------------------------------
struct GuestUI { Object* obj; std::string parent2d; };
static void CollectGuestsUI(Object* node2d, std::vector<GuestUI>& out) {
    for (size_t i = 0; i < node2d->Childrens.size(); i++) {
        Object* h = node2d->Childrens[i];
        if (UI2D_IsElement2D(h)) { CollectGuestsUI(h, out); continue; }     // sigue en el .w3dui
        GuestUI g; g.obj = h; g.parent2d = node2d->name; out.push_back(g);  // y su subarbol entero
    }
}

// ---------------------------------------------------------------------------
//  LOS HIJOS SE ESCRIBEN EN UN SOLO LUGAR (ver WriteObject, al final)
//
//  Antes cada rama de WriteObject llamaba -o se OLVIDABA de llamar- a esta
//  funcion: coleccion/espejo/instancia/constraint/curva/armature si, y ui,
//  script, camara, luz, malla (modelo y glb) y el generico NO. O sea que
//  guardar PERDIA, sin un solo aviso, todo lo que colgara de una camara, una
//  luz, un script, una UI o una malla ("add cube / parent Cubo.001 Cubo /
//  guardarw3d" y Cubo.001 no estaba en el archivo). Ahora la llamada esta UNA
//  vez, despues del if/else, asi que una rama nueva hereda la jerarquia sin que
//  su autor tenga que acordarse de nada.
// ---------------------------------------------------------------------------
static void WriteChildren(std::string& s, Object* o, int ind, CtxSave* cx) {
    // una INSTANCIA DE PREFAB no escribe sus hijos: los GENERA de la definicion de su prefab al abrir (y un PROXY
    // de una libreria tampoco: los genera la libreria)
    if (W3dIsInstanceType(o->getType())) return;
    // los NO-2D que cuelgan de los elementos 2D de esta UI: van aca, con "padre2d" (fallo B)
    std::vector<GuestUI> guests;
    if (o->getType() == ObjectType::ui)
        for (size_t i = 0; i < o->Childrens.size(); i++)
            if (UI2D_IsElement2D(o->Childrens[i])) CollectGuestsUI(o->Childrens[i], guests);
    size_t n = guests.size();
    for (size_t i = 0; i < o->Childrens.size(); i++)
        if (!ChildVaInOtherFile(o, o->Childrens[i])) n++;
    if (n == 0) return;
    s += ",\n"; JSang(s, ind); s += "\"children\": [\n";
    bool first = true;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        if (ChildVaInOtherFile(o, o->Childrens[i])) continue;
        WriteObject(s, o->Childrens[i], ind + 1, cx, &first);
    }
    for (size_t i = 0; i < guests.size(); i++)
        WriteObject(s, guests[i].obj, ind + 1, cx, &first, &guests[i].parent2d);
    s += "\n"; JSang(s, ind); s += "]";
}

// los scripts lua de un objeto (estilo Unity): [{archivo, refs{prop: valor}}]
static void WriteScripts(std::string& s, Object* o, int ind, CtxSave* cx) {
    if (!o->scriptData || o->scriptData->scripts.empty()) return;
    s += ",\n"; JSang(s, ind); s += "\"scripts\": [\n";
    for (size_t i = 0; i < o->scriptData->scripts.size(); i++) {
        // NO const: guardando a otra carpeta, Asset() copia el .lua adentro del
        // proyecto nuevo y deja la entrada apuntando a la copia
        W3dScriptEntry& e = o->scriptData->scripts[i];
        JSang(s, ind + 1); s += "{ \"file\": "; JEsc(s, Asset(cx, e.path));
        if (e.off) s += ", \"off\": true";   // (prendido no se escribe: es lo de siempre)
        if (!e.refs.empty()) {
            // ORDEN DETERMINISTICO: los refs se escriben SIEMPRE por nombre. En memoria
            // son un vector (orden de llegada) pero el JSON los relee a un std::map, o
            // sea ALFABETICO: guardar en orden de llegada hacia que el primer guardado
            // (venido del .w3d de texto: ref_* y despues val_*) y el segundo (venido del
            // v4, todo junto y ordenado) dieran BYTES DISTINTOS sin que cambiara un solo
            // dato. Ordenar aca deja el guardado idempotente sin tocar la semantica: los
            // refs son un diccionario name->value, no una secuencia.
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
        if (i + 1 < o->scriptData->scripts.size()) s += ",";
        s += "\n";
    }
    JSang(s, ind); s += "]";
}

static void WriteObject(std::string& s, Object* o, int ind, CtxSave* cx, bool* first,
                           const std::string* parent2d) {
    if (!o) return;
    ObjectType t = o->getType();

    // AUDIT versiones: curve/constraint/mirror/instance ahora SI se guardan (antes
    // se salteaban y se PERDIAN al re-guardar un proyecto de texto viejo). El unico
    // caso que sigue afuera A PROPOSITO es la GEOMETRIA de una Curve sin archivo de
    // origen: la clase no guarda sus vertices (los carga de un .txt "count/p x y z")
    // y serializar la geometria inline no tiene editor que la produzca hoy. Se cuenta
    // y se avisa. El NODO igual se escribe (nombre/transform/HIJOS): saltearlo entero
    // se llevaba puesto todo el subarbol que colgara de la curva, en silencio.
    if (t == ObjectType::curve && ((Curve*)o)->origin.empty()) gNotCovered++;

    if (!*first) s += ",\n";
    *first = false;

    // de quien es cada referencia que quede AFUERA (EXTERNALS.txt)
    gWho = "objeto \"" + o->name + "\"";

    JSang(s, ind); s += "{\n";
    // (en seco: de que objeto de la plantilla sale este nodo; ver gDry)
    // (el serial SIN signo y entero: pasado por int -o leido como float- se corria a partir de 2^31 / 2^24)
    if (gDry) { JSang(s, ind + 1); s += "\"_origin\": "; { char nb[16]; snprintf(nb, sizeof(nb), "%u", o->serial); s += nb; } s += ",\n"; }
    // el nodo cuelga de un ELEMENTO 2D de esta escena UI (ver CollectGuestsUI): el .w3d lo
    // escribe bajo el nodo UI y este campo dice de QUE elemento re-colgarlo al abrir. Ausente
    // en todos los demas nodos -> el archivo sale byte a byte igual que antes.
    if (parent2d && !parent2d->empty()) { JSang(s, ind + 1); s += "\"parent2d\": "; JEsc(s, *parent2d); s += ",\n"; }

    if (t == ObjectType::ui) {
        // cada escena UI es OTRO json (.w3dui) EXTERNO, referenciado por ruta
        // relativa al .w3d: se pisa el archivo del que vino (UI::fileW3dui,
        // que conserva la subcarpeta, ej "content/menu.w3dui"). Una UI nueva
        // toma "<nombre>.w3dui", bajo content/ con la estructura nueva.
        UI* u = (UI*)o;
        if (gDry || !cx->esc) {
            // (en seco: la escena UI tal como quedo en su entrada; lo que se le edito se ve al guardar)
            JSang(s, ind + 1); s += "\"type\": \"ui\",\n";
            JSang(s, ind + 1); s += "\"file\": "; JEsc(s, u->fileW3dui);
        } else {
        // la escena es una ENTRADA del contenedor (scenes/<slug>.w3dui). El nombre
        // sale del nombre VISIBLE de la escena, slugueado; el nombre visible no se
        // toca nunca (vive en el .w3dui) y el de entrada es un derivado.
        std::string visible = u->fileW3dui.empty()
                                  ? (o->name.empty() ? std::string("ui") : o->name)
                                  : BaseNoExt(u->fileW3dui);
        std::string name = "scenes/" + W3dSlugEntry(visible) + ".w3dui";
        for (int k = 2; cx->esc->Has(name); k++) {
            char sf[16]; snprintf(sf, sizeof(sf), "-%d", k);
            name = "scenes/" + W3dSlugEntry(visible) + sf + ".w3dui";
        }
        u->fileW3dui = name;
        // se escribe a un temporal EN LA CARPETA DEL DESTINO y de ahi entra al zip
        // (UI2DSave solo sabe escribir a un path). El temporal se borra siempre.
        std::string tmpUi = cx->esc->TempPath("e");
        std::string whoPrev = gWho;
        gWho = "escena \"" + o->name + "\"";
        bool okUi = UI2DSave(u, tmpUi, std::string());
        gWho = whoPrev;
        if (!okUi || !cx->esc->AddTemp(name, tmpUi)) {
            remove(tmpUi.c_str());
            w3dLogfE("GuardarW3D: no pude escribir la escena UI %s", name.c_str());
            cx->error = true;
        }
        JSang(s, ind + 1); s += "\"type\": \"ui\",\n";
        JSang(s, ind + 1); s += "\"file\": "; JEsc(s, name);
        }
    }
    // (el objeto de SCRIPT no tiene rama propia A PROPOSITO: cae por el 'else'
    //  generico del final y se guarda como "objeto" + CommonFields + sus scripts.
    //  Antes escribia un bloque "fisica" con velocidad / gravedad / potenciaSalto /
    //  velocidadMaximaCaida y cuatro limites de mundo: vocabulario de UN genero de
    //  juego horneado en el formato de guardado. Y ademas ningun lector lo consumia
    //  -- los dos importadores lo descartan y migran el nodo a un Empty. Esos valores
    //  son datos del proyecto: viven en las propiedades del script, o sea la tabla
    //  `propiedades` del .lua o los `val_` del .w3d.)
    else if (t == ObjectType::camera) {
        Camera* c = (Camera*)o;
        JSang(s, ind + 1); s += "\"type\": \"camera\",\n";
        CommonFields(s, ind + 1, o);
        // LENTE de la camara (fov + ortografica): propios de la camara, editables y animables
        s += ",\n"; JSang(s, ind + 1); s += "\"fov\": "; JNum(s, c->fov);
        s += ",\n"; JSang(s, ind + 1); s += "\"ortho\": "; s += (c->orthographic ? "true" : "false");
        s += ",\n"; JSang(s, ind + 1); s += "\"near\": "; JNum(s, c->nearClip);
        s += ",\n"; JSang(s, ind + 1); s += "\"far\": "; JNum(s, c->farClip);
        // ENCUADRE DECLARADO: solo si la camara lo declara, asi un proyecto que no
        // lo usa guarda exactamente el mismo texto de siempre (ver Camera::aspecto).
        if (c->aspect > 0.01f) { s += ",\n"; JSang(s, ind + 1); s += "\"aspect\": "; JNum(s, c->aspect); }
        // PROFUNDIDAD DE CAMPO (Camera::dof): solo si se toco algo (un proyecto viejo guarda el mismo texto de siempre)
        if (c->dof || c->fStop != 2.8f || c->focusDist != 5.0f || !c->focusObject.empty()) {
            s += ",\n"; JSang(s, ind + 1); s += "\"dof\": "; s += (c->dof ? "true" : "false");
            s += ",\n"; JSang(s, ind + 1); s += "\"fStop\": "; JNum(s, c->fStop);
            s += ",\n"; JSang(s, ind + 1); s += "\"focus\": "; JNum(s, c->focusDist);
            if (!c->focusObject.empty()) { s += ",\n"; JSang(s, ind + 1); s += "\"focusObject\": "; JEsc(s, c->focusObject); }
        }
        if (c->target) { s += ",\n"; JSang(s, ind + 1); s += "\"target\": "; JEsc(s, c->target->name); }
        // RIEL de la camara (AUDIT versiones: el texto viejo lo traia y el JSON lo perdia):
        // referencia POR NOMBRE a una Curve de la escena + el offset sobre el riel
        {
            std::string rail = c->Rail ? c->Rail->name : c->RailName;
            if (!rail.empty()) {
                s += ",\n"; JSang(s, ind + 1); s += "\"rail\": "; JEsc(s, rail);
                s += ",\n"; JSang(s, ind + 1); s += "\"railOffset\": "; JNum(s, (float)c->offsetRail);
                // MIRADA DEL RIEL: se escribe SOLO si esta prendida, asi un proyecto que
                // no la usa guarda exactamente el mismo texto de siempre.
                if (c->useRailRotation) { s += ",\n"; JSang(s, ind + 1); s += "\"railLook\": true"; }
            }
        }
    }
    else if (t == ObjectType::light) {
        Light* l = (Light*)o;
        JSang(s, ind + 1); s += "\"type\": \"light\",\n";
        CommonFields(s, ind + 1, o);
        s += ",\n"; JSang(s, ind + 1); s += "\"color\": [";
        JNum(s, l->diffuse[0]); s += ", "; JNum(s, l->diffuse[1]); s += ", "; JNum(s, l->diffuse[2]); s += "]";
        // tipo/atenuacion/trazado de rayos: SOLO si no son los defaults (un proyecto que no los toca guarda igual)
        if (l->directional) { s += ",\n"; JSang(s, ind + 1); s += "\"directional\": true"; }
        if (!l->ignoreOrder) { s += ",\n"; JSang(s, ind + 1); s += "\"ignoreOrder\": false"; }   // default true
        if (l->attConstant != 0.5f || l->attLinear != 0.1f || l->attQuadratic != 0.0f) {
            s += ",\n"; JSang(s, ind + 1); s += "\"atenuacion\": [";
            JNum(s, l->attConstant); s += ", "; JNum(s, l->attLinear); s += ", "; JNum(s, l->attQuadratic); s += "]";
        }
        // (el radio SIEMPRE: sin la clave se lee 0, como los proyectos de antes)
        { s += ",\n"; JSang(s, ind + 1); s += "\"rtRadius\": "; JNum(s, l->rtRadius); }
        if (l->rtRays > 0.0f) { s += ",\n"; JSang(s, ind + 1); s += "\"rtRays\": "; JNum(s, l->rtRays); }
        if (l->intensity != 1.0f) { s += ",\n"; JSang(s, ind + 1); s += "\"intensity\": "; JNum(s, l->intensity); }
        if (l->distance > 0.0f) { s += ",\n"; JSang(s, ind + 1); s += "\"distance\": "; JNum(s, l->distance); }
        // FOCO y AMBIENTE: solo si no son los de fabrica (180 = sin cono; ambiente negro)
        { if (l->spotCutoff != 180.0f || l->spotExponent != 0.0f) {
              s += ",\n"; JSang(s, ind + 1); s += "\"spot\": ["; JNum(s, l->spotCutoff); s += ", "; JNum(s, l->spotExponent); s += "]"; }
          if (l->ambient[0] != 0.0f || l->ambient[1] != 0.0f || l->ambient[2] != 0.0f) {
              s += ",\n"; JSang(s, ind + 1); s += "\"ambient\": ["; JNum(s, l->ambient[0]); s += ", "; JNum(s, l->ambient[1]);
              s += ", "; JNum(s, l->ambient[2]); s += "]"; } }
    }
    else if (t == ObjectType::collection) {
        JSang(s, ind + 1); s += "\"type\": \"collection\",\n";
        // CommonFields y no solo el nombre: una Collection oculta (ojito) o movida
        // PERDIA visible/pos/rot/escala al guardar y volvia visible al reabrir
        // ("oculto y se sigue viendo"). El lector siempre fue tolerante (CommonJson
        // con defaults), asi que los archivos viejos abren igual.
        CommonFields(s, ind + 1, o);
        // orden de dibujo para transparentes: solo se escribe si esta prendido.
        // El cast es seguro: aca solo llegan hijos de la escena (la raiz Scene no
        // pasa por aca).
        Collection* col = (Collection*)o;
        if (col->sortByCamera) { s += ",\n"; JSang(s, ind + 1); s += "\"sortByCamera\": true"; }
        if (col->sortOnce)    { s += ",\n"; JSang(s, ind + 1); s += "\"sortOnce\": true"; }
        // lote estatico (P4): solo se escribe si esta prendido (los .w3d que no lo
        // usan quedan byte a byte como antes)
        if (col->batch == 1)        { s += ",\n"; JSang(s, ind + 1); s += "\"batch\": \"static\""; }
    }
    else if (t == ObjectType::mirror) {
        // objeto ESPEJO (renderiza el reflejo de sus hijos/target): ejes + target por nombre
        Mirror* mi = (Mirror*)o;
        JSang(s, ind + 1); s += "\"type\": \"mirror\",\n";
        CommonFields(s, ind + 1, o);
        s += ",\n"; JSang(s, ind + 1); s += "\"axes\": [";
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
            if (!mi->targetsExtraName.empty()) {
                s += ",\n"; JSang(s, ind + 1); s += "\"targets\": "; JEsc(s, mi->TargetsText());
            }
        }
        // recorte del reflejo (ver Mirror.h): solo se escriben si se usan, para
        // no ensuciar los espejos comunes. "limites" = [u0, u1, v0, v1], medidos
        // en el PLANO del espejo (para el horizontal sin rotar, U = X y V = Z, o
        // sea los mismos numeros de siempre: el formato no cambio un byte).
        if (mi->noDepth) {
            s += ",\n"; JSang(s, ind + 1); s += "\"noDepth\": true";
        }
        if (mi->clipDeclared) {
            s += ",\n"; JSang(s, ind + 1); s += "\"clip\": "; s += mi->clip ? "true" : "false";
        }
        // HUNDIMIENTO MAXIMO DEL REFLEJO (ver Mirror.h): 0 = espejo exacto, no se escribe.
        if (mi->sinkMax > 0.0f) {
            s += ",\n"; JSang(s, ind + 1); s += "\"maxSink\": "; JNum(s, mi->sinkMax);
        }
        if (mi->usesLimits) {
            s += ",\n"; JSang(s, ind + 1); s += "\"limits\": [";
            JNum(s, mi->limU0); s += ", "; JNum(s, mi->limU1); s += ", ";
            JNum(s, mi->limV0); s += ", "; JNum(s, mi->limV1); s += "]";
        }
    }
    else if (t == ObjectType::instance) {
        // INSTANCE (duplicado enlazado / array de copias del target, opcionalmente espejado)
        Instance* in = (Instance*)o;
        JSang(s, ind + 1); s += "\"type\": \"instance\",\n";
        CommonFields(s, ind + 1, o);
        {
            std::string tg = in->target ? in->target->name : in->targetName;
            if (!tg.empty()) { s += ",\n"; JSang(s, ind + 1); s += "\"target\": "; JEsc(s, tg); }
        }
        s += ",\n"; JSang(s, ind + 1); s += "\"count\": "; JNum(s, (float)in->count);
        s += ",\n"; JSang(s, ind + 1); s += "\"mirrored\": "; s += in->mirror ? "true" : "false";
        s += ",\n"; JSang(s, ind + 1); s += "\"mirrorAxis\": "; JNum(s, (float)in->mirrorAxis);
    }
    else if (t == ObjectType::lod) {
        // LOD (un hijo por distancia a la camara): los umbrales, en orden (idioma del "ejes")
        LOD* l = (LOD*)o;
        JSang(s, ind + 1); s += "\"type\": \"lod\",\n";
        CommonFields(s, ind + 1, o);
        s += ",\n"; JSang(s, ind + 1); s += "\"distances\": [";
        for (size_t i = 0; i < l->distances.size(); i++) {
            if (i) s += ", ";
            JNum(s, l->distances[i]);
        }
        s += "]";
        // mismo flag que el Culling: solo se escribe si se usa, para que los proyectos
        // que no lo tocan queden byte a byte como antes
        if (l->onlyCameraActive) { s += ",\n"; JSang(s, ind + 1); s += "\"activeCameraOnly\": true"; }
    }
    else if (t == ObjectType::sound) {
        // SONIDO: todas sus propiedades, siempre (los tiempos en SEGUNDOS: cambiar los fps no los corre)
        Sound* so = (Sound*)o;
        JSang(s, ind + 1); s += "\"type\": \"sound\",\n";
        CommonFields(s, ind + 1, o);
        s += ",\n"; JSang(s, ind + 1); s += "\"file\": "; JEsc(s, so->file);
        s += ",\n"; JSang(s, ind + 1); s += "\"active\": "; s += so->active ? "true" : "false";
        s += ",\n"; JSang(s, ind + 1); s += "\"volume\": "; JNum(s, so->volume);
        s += ",\n"; JSang(s, ind + 1); s += "\"pitch\": "; JNum(s, so->tone);
        s += ",\n"; JSang(s, ind + 1); s += "\"reverb\": "; JNum(s, so->reverb);
        s += ",\n"; JSang(s, ind + 1); s += "\"pan\": "; JNum(s, so->pan);
        s += ",\n"; JSang(s, ind + 1); s += "\"start\": "; JNum(s, so->start);
        s += ",\n"; JSang(s, ind + 1); s += "\"clipStart\": "; JNum(s, so->clipStart);
        s += ",\n"; JSang(s, ind + 1); s += "\"clipEnd\": "; JNum(s, so->clipEnd);
        s += ",\n"; JSang(s, ind + 1); s += "\"loop\": "; s += so->loop ? "true" : "false";
        s += ",\n"; JSang(s, ind + 1); s += "\"positional\": "; s += so->positional ? "true" : "false";
        s += ",\n"; JSang(s, ind + 1); s += "\"radius\": "; JNum(s, so->radius);
        s += ",\n"; JSang(s, ind + 1); s += "\"range\": "; JNum(s, so->range);
    }
    else if (t == ObjectType::fog) {
        // NIEBLA: todas sus propiedades, siempre (tipo nuevo, no hay archivos viejos que cuidar)
        Fog* ni = (Fog*)o;
        static const char* kModes[] = { "linear", "exp", "exp2" };
        JSang(s, ind + 1); s += "\"type\": \"fog\",\n";
        CommonFields(s, ind + 1, o);
        s += ",\n"; JSang(s, ind + 1); s += "\"active\": "; s += ni->active ? "true" : "false";
        s += ",\n"; JSang(s, ind + 1); s += "\"mode\": "; JEsc(s, std::string(kModes[(ni->mode >= 0 && ni->mode <= 2) ? ni->mode : 0]));
        s += ",\n"; JSang(s, ind + 1); s += "\"start\": "; JNum(s, ni->start);
        s += ",\n"; JSang(s, ind + 1); s += "\"end\": "; JNum(s, ni->end);
        s += ",\n"; JSang(s, ind + 1); s += "\"density\": "; JNum(s, ni->density);
        s += ",\n"; JSang(s, ind + 1); s += "\"color\": ["; JNum(s, ni->color[0]); s += ", "; JNum(s, ni->color[1]);
        s += ", "; JNum(s, ni->color[2]); s += "]";
        s += ",\n"; JSang(s, ind + 1); s += "\"background\": "; s += ni->background ? "true" : "false";
    }
    else if (t == ObjectType::postProcess) {
        const W3dPostOptions& po = ((PostProcess*)o)->opt;
        JSang(s, ind + 1); s += "\"type\": \"postProcess\",\n";
        CommonFields(s, ind + 1, o);
        s += ",\n"; JSang(s, ind + 1); s += "\"on\": "; s += po.on ? "true" : "false";
        s += ",\n"; JSang(s, ind + 1); s += "\"selected\": "; JInt(s, po.active);
        s += ",\n"; JSang(s, ind + 1); s += "\"effects\": [";
        for (size_t i = 0; i < po.effects.size(); i++) {
            const W3dPostEffect& e = po.effects[i];
            if (i) s += ", ";
            s += "{ \"type\": "; JInt(s, e.type);
            s += ", \"active\": "; s += e.active ? "true" : "false";
            s += ", \"opt\": "; s += e.opt ? "true" : "false";
            const int nv = W3dPostNumbers(e.type);
            s += ", \"v\": ["; for (int k = 0; k < nv; k++) { if (k) s += ", "; JNum(s, e.v[k]); } s += "]";
            if (e.type == W3dPostBloom) { s += ", \"col\": ["; for (int k = 0; k < 3; k++) { if (k) s += ", "; JNum(s, e.col[k]); } s += "]"; }
            s += " }";
        }
        s += "]";
    }
    else if (t == ObjectType::clearz) {
        JSang(s, ind + 1); s += "\"type\": \"clearz\",\n";
        CommonFields(s, ind + 1, o);
        s += ",\n"; JSang(s, ind + 1); s += "\"active\": "; s += ((ClearZ*)o)->active ? "true" : "false";
    }
    else if (t == ObjectType::clip) {
        ClipRegion* rc = (ClipRegion*)o;
        JSang(s, ind + 1); s += "\"type\": \"clip\",\n";
        CommonFields(s, ind + 1, o);
        s += ",\n"; JSang(s, ind + 1); s += "\"active\": "; s += rc->active ? "true" : "false";
        s += ",\n"; JSang(s, ind + 1); s += "\"x\": "; JNum(s, rc->x);
        s += ",\n"; JSang(s, ind + 1); s += "\"y\": "; JNum(s, rc->y);
        s += ",\n"; JSang(s, ind + 1); s += "\"width\": "; JNum(s, rc->width);
        s += ",\n"; JSang(s, ind + 1); s += "\"height\": "; JNum(s, rc->height);
        s += ",\n"; JSang(s, ind + 1); s += "\"camera\": "; JEsc(s, rc->camera);
        s += ",\n"; JSang(s, ind + 1); s += "\"clearZ\": "; s += rc->clearZ ? "true" : "false";
        s += ",\n"; JSang(s, ind + 1); s += "\"background\": "; s += rc->background ? "true" : "false";
        s += ",\n"; JSang(s, ind + 1); s += "\"color\": ["; JNum(s, rc->color[0]); s += ", "; JNum(s, rc->color[1]);
        s += ", "; JNum(s, rc->color[2]); s += ", "; JNum(s, rc->color[3]); s += "]";
    }
    else if (t == ObjectType::hitbox) {
        JSang(s, ind + 1); s += "\"type\": \"hitbox\",\n";
        CommonFields(s, ind + 1, o);
        HitboxWriteFields(s, ind + 1, (Hitbox*)o);
    }
    else if (t == ObjectType::sondaluz) {   // SONDA DE LUZ: la caja, la grilla y la luz horneada (objects/LightProbe.cpp)
        JSang(s, ind + 1); s += "\"type\": \"sondaluz\",\n";
        CommonFields(s, ind + 1, o);
        LightProbeWriteFields(s, ind + 1, (LightProbe*)o);
    }
    else if (t == ObjectType::sondareflejo) {   // SONDA DE REFLEJO: la caja, la captura y su imagen 360 (objects/ReflectionProbe.cpp)
        JSang(s, ind + 1); s += "\"type\": \"reflectionprobe\",\n";
        CommonFields(s, ind + 1, o);
        // (la imagen: la que de verdad entra al .w3d, o ninguna. El objeto no se toca si se fue)
        ReflectionProbe* sr = (ReflectionProbe*)o;
        const std::string image = ImageProbe(cx, sr);
        const std::string inMemory = sr->texture;
        sr->texture = image;
        ReflectionProbeWriteFields(s, ind + 1, sr);
        sr->texture = inMemory;
    }
    else if (t == ObjectType::prefab) {
        // INSTANCIA DE PREFAB: el nodo, su prefab y sus overrides (sus hijos NO: ver WriteChildren)
        JSang(s, ind + 1); s += "\"type\": \"prefab\",\n";
        CommonFields(s, ind + 1, o);
        // (los overrides son lo que el usuario cambio en lo generado: el visible de un hijo, un valor de script)
        W3dPrefabSyncOverrides((InstancePrefab*)o);
        InstancePrefabWriteFields(s, ind + 1, (InstancePrefab*)o);
    }
    else if (t == ObjectType::proxy) {
        // PROXY W3D: el nodo, su libreria, su elemento y sus overrides. Lo que genera NO (ver WriteChildren) y sus
        // recursos tampoco: son de la libreria (no se ingiere nada)
        JSang(s, ind + 1); s += "\"type\": \"proxy\",\n";
        CommonFields(s, ind + 1, o);
        W3dPrefabSyncOverrides((InstancePrefab*)o);
        ProxyW3dWriteFields(s, ind + 1, (ProxyW3d*)o);
    }
    else if (t == ObjectType::routine) {
        // RUTINA: la lista de pasos del render (y las propias de cada modo)
        JSang(s, ind + 1); s += "\"type\": \"routine\",\n";
        CommonFields(s, ind + 1, o);
        RoutineWriteFields(s, ind + 1, (Routine*)o, RoutineEmitTexture, cx);
    }
    else if (t == ObjectType::culling) {
        // Culling: contenedor de culling unificado (metodo frustum/grid/triangulo/bsp)
        Culling* cu = (Culling*)o;
        JSang(s, ind + 1); s += "\"type\": \"culling\",\n";
        CommonFields(s, ind + 1, o);
        // metodo: solo se escribe si NO es frustum, asi los Culling clasicos quedan byte a byte como antes
        if (cu->method != Culling::Frustum) {
            s += ",\n"; JSang(s, ind + 1); s += "\"method\": ";
            JEsc(s, std::string(CullingMethodName(cu->method)));
        }
        // interruptor del recorte: solo se escribe si esta APAGADO, asi los .w3d que
        // no lo tocan quedan byte a byte como antes (el default al leer es true)
        if (!cu->active) { s += ",\n"; JSang(s, ind + 1); s += "\"active\": false"; }
        s += ",\n"; JSang(s, ind + 1); s += "\"activeCameraOnly\": ";
        s += cu->onlyCameraActive ? "true" : "false";
        // culling por DISTANCIA (0 = sin limite): solo se escribe si se usa, para
        // que los proyectos que no lo tocan queden byte a byte como antes
        if (cu->distanceMax > 0.0f) {
            s += ",\n"; JSang(s, ind + 1); s += "\"maxDistance\": "; JNum(s, cu->distanceMax);
        }
        if (cu->orderAlpha) { s += ",\n"; JSang(s, ind + 1); s += "\"alphaOrder\": true"; }
        if (cu->orderProximity) { s += ",\n"; JSang(s, ind + 1); s += "\"distanceOrder\": true"; }
        // campos del metodo Grid: solo se escriben con metodo=Grid (los demas metodos no los usan)
        if (cu->method == Culling::Grid) {
            s += ",\n"; JSang(s, ind + 1); s += "\"cellSize\": "; JNum(s, cu->cellSize);
            if (cu->mode3D) { s += ",\n"; JSang(s, ind + 1); s += "\"mode3D\": true"; }
        }
        // campos del metodo Riel: la Curve del recorrido + el .w3dvis de hijos por nodo
        if (cu->method == Culling::Rail) {
            if (!cu->railName.empty())      { s += ",\n"; JSang(s, ind + 1); s += "\"rail\": ";     JEsc(s, cu->railName); }
            if (!cu->visChildrenFile.empty()) { s += ",\n"; JSang(s, ind + 1); s += "\"visChildren\": "; JEsc(s, cu->visChildrenFile); }
        }
        // campo del metodo Bsp: el .w3dbsp (arbol + PVS + clusters por hijo)
        if (cu->method == Culling::Bsp && !cu->bspFile.empty()) {
            s += ",\n"; JSang(s, ind + 1); s += "\"bsp\": "; JEsc(s, cu->bspFile);
            // las texturas del MUNDO del .w3dbsp v2 (nombradas adentro del dato): entran al contenedor como las de
            // un material, asi un proyecto que solo las usa por el mundo no las pierde al guardar
            for (size_t i = 0; i < cu->world.materials.size(); i++) {
                std::string t = cu->world.materials[i].texture, l = cu->world.materials[i].lightmap;
                if (!t.empty()) Asset((CtxSave*)cx, t);
                if (!l.empty()) Asset((CtxSave*)cx, l);
            }
        }
        if (!cu->visibleMemory.empty()) { s += ",\n"; JSang(s, ind + 1); s += "\"visibleMemory\": "; JEsc(s, cu->visibleMemory); }
    }
    else if (t == ObjectType::viszona) {
        // VISZONA (celda de visibilidad): todas sus propiedades, siempre (tipo nuevo:
        // no hay archivos viejos que cuidar byte a byte). objetivo/ancla viajan por
        // NOMBRE (se resuelven en la escena al usarse, como los targets).
        VisZone* vz = (VisZone*)o;
        JSang(s, ind + 1); s += "\"type\": \"viszona\",\n";
        CommonFields(s, ind + 1, o);
        static const char* kModes[] = { "manual", "grid", "volumes", "curve" };
        int mi = (vz->mode >= 0 && vz->mode <= 3) ? vz->mode : 0;
        s += ",\n"; JSang(s, ind + 1); s += "\"mode\": "; JEsc(s, std::string(kModes[mi]));
        s += ",\n"; JSang(s, ind + 1); s += "\"target\": "; JEsc(s, vz->targetName);
        s += ",\n"; JSang(s, ind + 1); s += "\"anchor\": ";    JEsc(s, vz->anchorName);
        s += ",\n"; JSang(s, ind + 1); s += "\"nx\": "; JNum(s, (float)vz->nx);
        s += ", \"ny\": "; JNum(s, (float)vz->ny);
        s += ", \"nz\": "; JNum(s, (float)vz->nz);
        s += ",\n"; JSang(s, ind + 1); s += "\"cellSize\": "; JNum(s, vz->cellSize);
        s += ",\n"; JSang(s, ind + 1); s += "\"maxStep\": ";  JNum(s, (float)vz->stepMax);
        // multi-riel (ver VisZone.h): solo si se usa, para que los proyectos que
        // no lo declaran queden byte a byte como antes
        if (!vz->railName.empty()) {
            s += ",\n"; JSang(s, ind + 1); s += "\"rail\": "; JEsc(s, vz->railName);
        }
        if (vz->cellFallback != 0) {
            s += ",\n"; JSang(s, ind + 1); s += "\"fallback\": "; JNum(s, (float)vz->cellFallback);
        }
    }
    else if (t == ObjectType::particles) {
        // PARTICULAS (emisor de billboards del Core): todas sus propiedades, siempre
        // (tipo nuevo: no hay archivos viejos que cuidar byte a byte). La textura
        // entra al contenedor v4 como cualquier asset (textures/).
        Particles* pt = (Particles*)o;
        JSang(s, ind + 1); s += "\"type\": \"particles\",\n";
        CommonFields(s, ind + 1, o);
        s += ",\n"; JSang(s, ind + 1); s += "\"texture\": ";
        JEsc(s, pt->texture.empty() ? std::string() : Asset(cx, pt->texture));
        s += ",\n"; JSang(s, ind + 1); s += "\"count\": ";   JNum(s, pt->count);
        s += ",\n"; JSang(s, ind + 1); s += "\"life\": ";       JNum(s, pt->life);
        s += ",\n"; JSang(s, ind + 1); s += "\"size\": ";        JNum(s, pt->size);
        s += ",\n"; JSang(s, ind + 1); s += "\"vel\": ";        JNum(s, pt->vel);
        s += ",\n"; JSang(s, ind + 1); s += "\"dispersion\": "; JNum(s, pt->dispersion);
        s += ",\n"; JSang(s, ind + 1); s += "\"gravity\": ";   JNum(s, pt->gravity);
        // MODO DE MEZCLA (w3dEngine::Mezcla): reemplaza a aditivo/sustractivo. Solo se escribe si NO es el
        // default MixAlpha, asi los emisores comunes quedan igual que antes.
        if (pt->mix != w3dEngine::MixAlpha) {
            s += ",\n"; JSang(s, ind + 1); s += "\"mix\": "; JNum(s, (float)pt->mix);
        }
        s += ",\n"; JSang(s, ind + 1); s += "\"color\": [";
        JNum(s, pt->color[0]); s += ", "; JNum(s, pt->color[1]); s += ", ";
        JNum(s, pt->color[2]); s += ", "; JNum(s, pt->color[3]); s += "]";
        s += ",\n"; JSang(s, ind + 1); s += "\"fade\": "; s += pt->fade ? "true" : "false";
        s += ",\n"; JSang(s, ind + 1); s += "\"active\": ";     s += pt->active ? "true" : "false";
        // FILTRADO de la textura: default true (suave); solo se escribe si NO filtra (pixelada),
        // asi los emisores comunes y los .w3d viejos quedan byte a byte como antes.
        if (!pt->filtered) { s += ",\n"; JSang(s, ind + 1); s += "\"filtered\": false"; }
        s += ",\n"; JSang(s, ind + 1); s += "\"variation\": ";   JNum(s, pt->variation);
        s += ",\n"; JSang(s, ind + 1); s += "\"turbulence\": "; JNum(s, pt->turbulence);
        // emissionRadius: solo si difiere del default (round-trip byte a byte)
        if (pt->emissionRadius != 45.0f) { s += ",\n"; JSang(s, ind + 1); s += "\"emissionRadius\": "; JNum(s, pt->emissionRadius); }
        // rotacion del billboard: solo si se usa (los proyectos que no la tocan
        // quedan byte a byte como antes, mismo criterio que "sustractivo")
        if (pt->rotation)            { s += ",\n"; JSang(s, ind + 1); s += "\"rotation\": true"; }
        if (pt->velRotation != 0.0f) { s += ",\n"; JSang(s, ind + 1); s += "\"rotationSpeed\": "; JNum(s, pt->velRotation); }
        if (pt->flipFrames > 0) {   // FLIPBOOK por edad (solo si anima)
            s += ",\n"; JSang(s, ind + 1); s += "\"flipFrames\": "; JNum(s, (float)pt->flipFrames);
            s += ", \"flipCols\": ";  JNum(s, (float)pt->flipCols);
            s += ", \"flipRows\": "; JNum(s, (float)pt->flipRows);
        }
        // SUB-RECT del atlas unico: solo si difiere de la textura entera
        if (pt->uvRect[0] != 0.0f || pt->uvRect[1] != 0.0f ||
            pt->uvRect[2] != 1.0f || pt->uvRect[3] != 1.0f) {
            s += ",\n"; JSang(s, ind + 1); s += "\"uvRect\": [";
            for (int k = 0; k < 4; k++) { if (k) s += ", "; JNum(s, pt->uvRect[k]); }
            s += "]";
        }
        // LO AVANZADO: cada campo solo si difiere del default (un emisor comun sale igual que antes)
        {
            const char* v3[4] = { "velLocal", "randomVel", "randomPos", "acceleration" };
            const float* d3[4] = { pt->velLocal, pt->velRandom, pt->posRandom, pt->acceleration };
            for (int q = 0; q < 4; q++) {
                if (d3[q][0] == 0.0f && d3[q][1] == 0.0f && d3[q][2] == 0.0f) continue;
                s += ",\n"; JSang(s, ind + 1); s += "\""; s += v3[q]; s += "\": [";
                JNum(s, d3[q][0]); s += ", "; JNum(s, d3[q][1]); s += ", "; JNum(s, d3[q][2]); s += "]";
            }
            struct { const char* k; float v, def; } fl[] = {
                { "drag", pt->drag, 1.0f }, { "growth", pt->growth, 0.0f },
                { "growthBrake", pt->brakeGrowth, 1.0f }, { "alphaDecay", pt->alphaDecays, 1.0f },
                { "alphaDeath", pt->alphaDeath, 0.0f }, { "initialRot", pt->initialRot, 0.0f },
                { "randomRot", pt->rotRandom, 0.0f }, { "randomRotVel", pt->velRotRandom, 0.0f },
                { "flipFps", pt->flipFps, 0.0f } };
            for (size_t q = 0; q < sizeof(fl) / sizeof(fl[0]); q++) {
                if (fl[q].v == fl[q].def) continue;
                s += ",\n"; JSang(s, ind + 1); s += "\""; s += fl[q].k; s += "\": "; JNum(s, fl[q].v);
            }
            if (!pt->turnSignRandom) { s += ",\n"; JSang(s, ind + 1); s += "\"randomSpinSign\": false"; }
            if (pt->appear != 0.0f) { s += ",\n"; JSang(s, ind + 1); s += "\"appear\": "; JNum(s, pt->appear); }
            if (pt->fadeNear[1] > 0.0f) {
                s += ",\n"; JSang(s, ind + 1); s += "\"fadeNear\": ["; JNum(s, pt->fadeNear[0]); s += ", ";
                JNum(s, pt->fadeNear[1]); s += "]";
            }
            if (pt->fadeFar[1] > 0.0f) {
                s += ",\n"; JSang(s, ind + 1); s += "\"fadeFar\": ["; JNum(s, pt->fadeFar[0]); s += ", ";
                JNum(s, pt->fadeFar[1]); s += "]";
            }
            if (pt->flipOnce)     { s += ",\n"; JSang(s, ind + 1); s += "\"flipOnce\": true"; }
            if (pt->useColorFinal) {
                s += ",\n"; JSang(s, ind + 1); s += "\"colorFinal\": [";
                for (int k = 0; k < 4; k++) { if (k) s += ", "; JNum(s, pt->colorFinal[k]); }
                s += "]";
            }
            if (pt->shape != w3dEngine::ShapeBillboard) {
                s += ",\n"; JSang(s, ind + 1); s += "\"shape\": ";
                s += (pt->shape == w3dEngine::ShapeStretched) ? "\"stretched\"" : "\"line\"";
                if (pt->shape == w3dEngine::ShapeStretched) { s += ", \"stretch\": "; JNum(s, pt->stretch); }
                else {
                    s += ", \"trailPoints\": "; JNum(s, (float)pt->trailPoints);
                    s += ", \"trailStep\": ";   JNum(s, pt->trailStep);
                    s += ", \"lineThickness\": ";  JNum(s, pt->thicknessLine);
                }
            }
        }
    }
    // (aca vivia la rama del objeto Constraint VIEJO. Se dio de baja: un constraint es
    //  una propiedad del objeto y lo escribe WriteConstraints, que corre para TODOS
    //  los tipos mas abajo. Los .w3d viejos con nodos "constraint" se siguen LEYENDO:
    //  el lector los migra al abrir, ver import_w3d.cpp)
    else if (t == ObjectType::curve) {
        // CURVE cargada de archivo (riel de camara): se referencia su archivo de
        // origen (la rama sin origen ya se conto arriba en gNotCovered)
        Curve* cv = (Curve*)o;
        JSang(s, ind + 1); s += "\"type\": \"curve\",\n";
        CommonFields(s, ind + 1, o);
        // sin origen: "archivo" vacio (la curva vuelve VACIA, pero el nodo y su
        // subarbol vuelven). Asset() no se llama con "" (no hay nada que copiar).
        // v4: el .cap SE INGIERE al contenedor -- Curve::LoadFromFile lee por
        // w3dFileSystem (VFS incluido); el comentario viejo sobre std::ifstream
        // estaba stale y dejaba el riel AFUERA del zip (el juego no viajaba solo).
        s += ",\n"; JSang(s, ind + 1); s += "\"file\": ";
        JEsc(s, cv->origin.empty() ? std::string() : Asset(cx, cv->origin));
        // curva AUTORADA (Add > Path o riel editado en el editor): sin .cap de origen,
        // los nodos van INLINE (x y z por nodo, ya en el espacio del motor)
        if (cv->origin.empty() && cv->vertex && cv->vertexSize > 0) {
            s += ",\n"; JSang(s, ind + 1); s += "\"points\": [";
            for (int i = 0; i < cv->vertexSize * 3; i++) { if (i) s += ", "; JNum(s, cv->vertex[i]); }
            s += "]";
            if (!cv->edges.empty()) {   // GRAFO con ramas (pares de indices); sin esto = polilinea
                s += ",\n"; JSang(s, ind + 1); s += "\"edges\": [";
                for (size_t i = 0; i < cv->edges.size(); i++) { if (i) s += ", "; JNum(s, (float)cv->edges[i]); }
                s += "]";
            }
        }
        // LISTA DE CARGA (streaming): el sidecar .cargas.json declarado. Solo
        // si hay (los .w3d sin streaming quedan byte a byte como siempre).
        // Se INGIERE como el .cap (LoadLoadList lee por ReadFileBytes = VFS).
        if (!cv->loadsFile.empty()) {
            s += ",\n"; JSang(s, ind + 1); s += "\"loads\": ";
            JEsc(s, Asset(cx, cv->loadsFile));
        }
    }
    else if (t == ObjectType::armature) {
        // ARMATURE (Fase 3): nombre/transform + huesos + clips de animacion + hijos (mallas del rig)
        Armature* a = (Armature*)o;
        JSang(s, ind + 1); s += "\"type\": \"armature\",\n";
        CommonFields(s, ind + 1, o);
        WriteArmature(s, a, ind + 1);
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
        if (!gDry) MatRegister(m);     // sus materiales van al bloque raiz "materiales"
        // LA MALLA COMO RECURSO: el objeto nombra su malla del registro "mallas" (una entrada
        // por malla -.w3db, o .w3dm en el formato de texto- compartida por todos sus objetos; el
        // plan del guardado ya la escribio). Una malla que no entra al plan (mas de 65535
        // vertices con indices de 16 bits) va por el camino de siempre: su "geometria" propia.
        // (en seco: la malla por el nombre de su RECURSO en memoria; toda malla tiene uno)
        // (una malla de una LIBRERIA externa -soltada desde su biblioteca- se NOMBRA con su prefijo y no se escribe:
        //  el plan del guardado no la toca, io/ProjectMeshes.cpp)
        // (y la de una libreria que NO ESTA -desvinculada o sin su archivo, io/Libraries.h-: el objeto quedo vacio pero
        //  su referencia se escribe tal cual, y vuelve a resolverse al vincularla)
        const W3dLibRef* refLib = m->mesh ? NULL : W3dLibRefOf(m, W3D_LIBREF_MESH);
        const bool fromLib = m->mesh && !m->mesh->lib.empty();
        const std::string nameMesh = refLib ? refLib->name
                                           : (gDry || fromLib) ? (m->mesh ? m->mesh->name : std::string())
                                           : (g_w3dMeshesLegacy ? std::string() : W3dMeshesSaveNameOf(m));
        std::string nameGeo;
        // (con el plan fallado el guardado ya se aborto y se aviso: no se repiten los frenos)
        const bool planFailed = !g_w3dMeshesLegacy && cx->error;
        if (nameMesh.empty() && !planFailed) nameGeo = WriteMeshW3dm(cx, m, o->name);
        JSang(s, ind + 1); s += "\"type\": \"mesh\",\n";
        CommonFields(s, ind + 1, o);
        if (!nameMesh.empty()) { s += ",\n"; JSang(s, ind + 1); s += "\"mesh\": "; JEsc(s, nameMesh); }
        else { s += ",\n"; JSang(s, ind + 1); s += "\"geometry\": "; JEsc(s, nameGeo); }
        // ESCENARIO CERRADO A EDICION: sin esto el reabrir recalculaba bordes
        // (edges 0 -> N) y la malla dejaba de ser identica a la original.
        if (m->noEditable) { s += ",\n"; JSang(s, ind + 1); s += "\"noEditable\": true"; }
        if (m->onlyRoutine) { s += ",\n"; JSang(s, ind + 1); s += "\"routineOnly\": true"; }
        if (m->lockNormals) { s += ",\n"; JSang(s, ind + 1); s += "\"lockNormals\": true"; }
        // LAS CAPAS DE MATERIAL de encima de cada parte (MaterialGroup::layersMat), POR NOMBRE: la capa 1 es la de siempre
        // (el bloque PARTMAT de la malla); estas van con el objeto
        { bool has = false;
          for (size_t g = 0; g < m->materialsGroup.size() && !has; g++) if (!m->materialsGroup[g].layersMat.empty()) has = true;
          if (has) {
              s += ",\n"; JSang(s, ind + 1); s += "\"materialLayers\": [";
              bool first = true;
              for (size_t g = 0; g < m->materialsGroup.size(); g++) {
                  const std::vector<Material*>& cm = m->materialsGroup[g].layersMat;
                  if (cm.empty()) continue;
                  s += first ? " " : ", "; first = false;
                  s += "{ \"part\": "; JNum(s, (float)g); s += ", \"materials\": [";
                  bool p1 = true;
                  for (size_t c = 0; c < cm.size(); c++) {
                      if (!cm[c]) continue;
                      if (!p1) s += ", "; p1 = false;
                      JEsc(s, cm[c]->name);
                  }
                  s += "] }";
              }
              s += " ]";
          } }
        // el UV PERSONALIZADO de los mapas de sus materiales (Mesh::uvPersonal): material, mapa y la capa UV por nombre
        // (por NOMBRE de su capa UV, o un INDICE propio)
        if (!m->uvPersonal.empty() || !m->uvPersonalIdx.empty()) {
            s += ",\n"; JSang(s, ind + 1); s += "\"uvMaterials\": [";
            bool first = true;
            for (std::map<std::string, std::string>::const_iterator it = m->uvPersonal.begin(); it != m->uvPersonal.end(); ++it) {
                const size_t c = it->first.find('\x1f');
                if (c == std::string::npos || it->second.empty()) continue;
                s += first ? " " : ", "; first = false;
                s += "{ \"material\": "; JEsc(s, it->first.substr(0, c));
                s += ", \"map\": "; JEsc(s, it->first.substr(c + 1));
                s += ", \"uv\": "; JEsc(s, it->second); s += " }";
            }
            for (std::map<std::string, int>::const_iterator it = m->uvPersonalIdx.begin(); it != m->uvPersonalIdx.end(); ++it) {
                const size_t c = it->first.find('\x1f');
                if (c == std::string::npos || it->second < 0 || m->uvPersonal.count(it->first)) continue;
                s += first ? " " : ", "; first = false;
                s += "{ \"material\": "; JEsc(s, it->first.substr(0, c));
                s += ", \"map\": "; JEsc(s, it->first.substr(c + 1));
                s += ", \"index\": "; JNum(s, (float)it->second); s += " }";
            }
            s += " ]";
        }
        if (!m->origin.empty()) {
            // el .obj/.fbx del usuario lo edita OTRO programa: es suyo y queda EXTERNO
            // (listado en EXTERNALS.txt). Ya no es de donde carga la malla.
            if (!gDry) W3dExternalRefMark(m->origin);
            // LOS SIDECARS DEL MODELO (<base>.pvs.json) SI ENTRAN AL CONTENEDOR.
            // Son datos derivados que ningun otro programa edita, no hay forma de
            // regenerarlos en la maquina de destino, y sin esto se perdian en
            // silencio al empaquetar: no viajaban ni figuraban en EXTERNALS.txt.
            IngestSidecars(cx, m);
            s += ",\n"; JSang(s, ind + 1); s += "\"origin\": "; JEsc(s, Asset(cx, m->origin));
        }
        // ANIMACION UV "tira de atlas" (autoplay del Core, ver Mesh.h): 4 parametros
        // planos. El estado de reproduccion (cuadro/acum/base) NO se guarda a proposito.
        if (m->flipbook && !m->flipbookOwn) {   // flipbook CON NOMBRE compartido: se guarda la REFERENCIA
            s += ",\n"; JSang(s, ind + 1); s += "\"flipbook\": "; JEsc(s, m->flipbook->name);
            if (m->flipPlay.offset) { s += ", \"flipOffset\": "; JNum(s, (float)m->flipPlay.offset); }
        } else if (m->flipbook) {   // flipbook PROPIO de la malla: la tira inline (animUV)
            s += ",\n"; JSang(s, ind + 1); s += "\"animUV\": { \"frames\": ";
            JNum(s, (float)m->flipbook->frames);
            s += ", \"fps\": "; JNum(s, m->flipbook->fps);
            s += ", \"axis\": "; JEsc(s, std::string(m->flipbook->rows > 1 ? "v" : "u"));
            s += ", \"offset\": "; JNum(s, (float)m->flipPlay.offset);
            // sub-tira en el atlas unico (default 1 = textura entera): solo si difiere
            if (m->flipbook->stripWidth != 1.0f) { s += ", \"width\": "; JNum(s, m->flipbook->stripWidth); }
            if (m->flipbook->stripHeight  != 1.0f) { s += ", \"height\": ";  JNum(s, m->flipbook->stripHeight); }
            s += " }";
        }
        WriteAnimsVertex(s, m, ind + 1, cx);   // vertex anims (frames binarios + curvas)
        WriteRigMesh(s, m, ind + 1);           // modificador Armature (por nombre) + armatures 2D
        WriteModifiers(s, m, ind + 1);         // el resto del stack (Mirror/Screw/SubSurf/...)
    }
    else {
        // tipo sin rama propia: al menos el nombre/transform (no rompe el abrir)
        JSang(s, ind + 1); s += "\"type\": \"object\",\n";
        CommonFields(s, ind + 1, o);
        // el VACIO: como se dibuja (solo la vista). Sigue siendo el nodo generico "objeto": un editor viejo ignora estos
        // campos, y un archivo viejo (sin ellos) abre con el dibujo por defecto
        if (t == ObjectType::empty) {
            const Empty* e = (const Empty*)o;
            s += ",\n"; JSang(s, ind + 1); s += "\"shape\": "; JNum(s, (float)e->shape);
            s += ",\n"; JSang(s, ind + 1); s += "\"drawSize\": "; JNum(s, e->drawingSize);
            s += ",\n"; JSang(s, ind + 1); s += "\"drawRot\": ["; JNum(s, e->rotDrawing.x); s += ", "; JNum(s, e->rotDrawing.y);
            s += ", "; JNum(s, e->rotDrawing.z); s += "]";
        }
    }

    // EL STACK DE CONSTRAINTS, DE TODOS LOS TIPOS, DE UNA SOLA VEZ. Va aca por el
    // MISMO motivo que WriteChildren: es una propiedad de Object, no de una rama.
    //
    // *** Y NO CUELGA DE CommonFields A PROPOSITO ***: la rama 'coleccion' NO la
    // llama (escribe tipo + nombre y nada mas), asi que los constraints de una
    // Coleccion se perderian al guardar sin un solo aviso. Es exactamente el fallo
    // que ya se pago una vez con los hijos.
    WriteConstraints(s, o, ind + 1);

    // LOS SCRIPTS LUA, DE TODOS LOS TIPOS, DE UNA SOLA VEZ (estilo Unity: una
    // una malla, un vacio o una luz llevan scripts igual que el objeto Script).
    // Antes esto se llamaba SOLO en las ramas gamepad y malla: un script colgado
    // de cualquier otro tipo se perdia al guardar, sin aviso. Mismo criterio (y
    // mismo motivo) que WriteConstraints/WriteChildren. El lector espejo es
    // el call site unico de JsonScripts en JsonObject (import_w3d.cpp).
    WriteScripts(s, o, ind + 1, cx);

    // SU BIBLIOTECA DE CLIPS DE JERARQUIA (animation/W3dAnimSet.h): el nombre del animset compartido con
    // el que se anima a si mismo y a sus hijos. Sin biblioteca no se escribe nada (el archivo no cambia).
    WriteClipsHierarchy(s, o, ind + 1);

    // LOS HIJOS, DE TODOS LOS TIPOS, DE UNA SOLA VEZ (ver WriteChildren arriba).
    // Va al FINAL a proposito: para las ramas que YA lo llamaban (coleccion,
    // espejo, instancia, constraint, curva, armature) era su ultima linea, asi
    // que el .w3d sale byte a byte IGUAL que antes.
    WriteChildren(s, o, ind + 1, cx);

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
//  DONDE VA, Y POR QUE ACA Y NO EN UN ARCHIVO PROPIO BAJO animations/:
//   - animations/ es la carpeta de los BLOBS BINARIOS de vertex anim (las
//     posiciones de cada cuadro). Esto es lo contrario: datos estructurados y
//     chicos, que el dueno tiene que poder LEER y EDITAR abriendo el zip.
//   - las curvas referencian objetos POR NOMBRE, y los objetos viven en el
//     project.json. Partirlos en dos archivos crea un segundo archivo que hay
//     que mantener en sincronia y que puede faltar; el mismo archivo no puede
//     desincronizarse de si mismo.
//   - el resto de lo estructurado del proyecto (materiales, paletas, layout,
//     compilar) ya vive en la raiz del project.json: esto es de la misma clase.
//
//  LA REFERENCIA AL OBJETO ES SU NOMBRE, CON SU SCOPE. Los nombres son unicos
//  POR ESCENA (W3dScopeNameOf: la UI mas cercana que contiene al objeto, o el
//  arbol global), asi que el nombre solo NO alcanza si el objeto vive dentro de
//  una escena UI: ahi se escribe tambien "escena" (el nombre de esa UI) y al
//  abrir se busca DENTRO de ella. Sin "escena" se busca en el arbol global, que
//  es exactamente el scope que le corresponde. Es la misma regla que ya usa
//  "padre2d" para los huespedes de una UI.
//
//  EL INVARIANTE DEL SWAP: las curvas de la escena ACTIVA no viven en su
//  SceneAnimation sino en el global AnimationObjects (SetActiveScene swapea las
//  listas, ver Animation.cpp). Guardar leyendo esc->objects a secas perderia
//  JUSTO la escena que el usuario esta viendo, que es el unico caso que reporta.
// ===========================================================================

// la lista de curvas de la escena 'si': la global si es la ACTIVA, la suya si no
static const std::vector<AnimationObject>& SceneCurves(int yes) {
    return (yes == ActiveSceneAnim) ? (const std::vector<AnimationObject>&)AnimationObjects
                                   : (const std::vector<AnimationObject>&)SceneAnimations[yes]->objects;
}

// una AnimationObject aporta algo solo si tiene AL MENOS un keyframe. Una curva sin
// keyframes la crea sola la evaluacion (ListProperty) y tambien queda al borrar el
// ultimo key de un canal: escribirla engorda el archivo y ensucia el dope al reabrir.
// Mismo criterio que ya usan los clips del armature 2D.
static bool AnimObjHasKeys(const AnimationObject& ao) {
    for (size_t p = 0; p < ao.Properties.size(); p++)
        if (!ao.Properties[p].keyframes.empty()) return true;
    return false;
}

// true si el estado de animaciones es EXACTAMENTE el que crea InitSceneAnimations
// (una escena "Scene", rango/fps default, sin una sola curva). En ese caso el bloque
// no se escribe: un proyecto sin animar sale byte a byte como antes de este cambio.
// (los CLIPS DE OBJETO no van en este bloque: se guardan con su objeto, ver EscribirClipsObjeto)
static bool AnimationsAreDefault() {
    std::vector<int> esc;
    for (size_t i = 0; i < SceneAnimations.size(); i++) if (SceneAnimations[i] && !SceneAnimations[i]->isClip) esc.push_back((int)i);
    if (esc.size() != 1 || W3dAnimIsClip(ActiveSceneAnim)) return false;
    const SceneAnimation* e = SceneAnimations[(size_t)esc[0]];
    if (e->name != "Scene" || e->startFrame != 1 || e->endFrame != 250 || e->fps != 30) return false;
    if (!e->skeletons.empty()) return false;   // (tiene tiras)
    if (!e->camTrack.empty() && !e->camTrack[0].keyframes.empty()) return false;   // tiene cortes de camara
    const std::vector<AnimationObject>& objs = SceneCurves(esc[0]);
    for (size_t i = 0; i < objs.size(); i++) if (AnimObjHasKeys(objs[i])) return false;
    return true;
}

// las curvas de UNA animacion (la lista "objetos" de una animacion de escena o de un clip de objeto):
// el objeto por su nombre (con su escena UI si vive en una). 'duenio' = el dueno de un clip de objeto:
// sus curvas propias se escriben con el nombre vacio (el clip viaja con el objeto aunque se renombre).
static void WriteObjectCurves(std::string& s, const std::vector<AnimationObject>& objs, const char* indent,
                                    const Object* owner) {
    bool first = true;
    for (size_t i = 0; i < objs.size(); i++) {
        const AnimationObject& ao = objs[i];
        if (!ao.obj || !AnimObjHasKeys(ao)) continue;
        // (lo GENERADO por una instancia de prefab no se guarda: sus curvas no tendrian a quien nombrar al abrir)
        if (W3dIsGenerated(ao.obj)) continue;
        if (!first) s += ",\n";
        first = false;
        s += indent; s += "{ \"object\": "; JEsc(s, (owner && ao.obj == owner) ? std::string() : ao.obj->name);
        // SCOPE del nombre: si el objeto vive dentro de una escena UI, el nombre
        // solo es unico AHI ADENTRO -> se escribe de que UI se trata.
        Object* scope = W3dScopeNameOf(ao.obj);
        if (ao.obj != owner && scope && scope != SceneCollection) {
            s += ", \"scene\": "; JEsc(s, scope->name);
        }
        s += ",\n"; s += indent; s += "  \"curves\": [";
        bool firstCurve = true;
        for (size_t p = 0; p < ao.Properties.size(); p++) {
            if (ao.Properties[p].keyframes.empty()) continue;   // curva sin keys: no es dato
            if (!firstCurve) { s += ",\n"; s += indent; s += "              "; }
            firstCurve = false;
            WriteCurve(s, ao.Properties[p]);
        }
        s += "] }";
    }
}

static void WriteAnimationsScene(std::string& s) {
    InitSceneAnimations();   // idempotente: garantiza que exista "Scene"
    if (AnimationsAreDefault()) return;
    // las animaciones de ESCENA (los clips de objeto van con su objeto): "activa" es su indice ENTRE
    // ELLAS; si lo que se estaba editando era un clip, "activaClip" dice de que objeto y cual
    std::vector<int> esc;
    int active = 0;
    for (size_t i = 0; i < SceneAnimations.size(); i++) {
        if (!SceneAnimations[i] || SceneAnimations[i]->isClip) continue;
        if ((int)i == ActiveSceneAnim) active = (int)esc.size();
        esc.push_back((int)i);
    }
    s += "  \"animations\": {\n";
    s += "    \"active\": "; JInt(s, active); s += ",\n";
    if (W3dAnimIsClip(ActiveSceneAnim) && SceneAnimations[ActiveSceneAnim]->owner) {
        const SceneAnimation* c = SceneAnimations[ActiveSceneAnim];
        s += "    \"activeClip\": { \"object\": "; JEsc(s, c->owner->name);
        s += ", \"clip\": "; JEsc(s, c->name); s += " },\n";
    }
    s += "    \"scenes\": [\n";
    for (size_t k = 0; k < esc.size(); k++) {
        const size_t yes = (size_t)esc[k];
        const SceneAnimation* e = SceneAnimations[yes];
        s += "      { \"name\": "; JEsc(s, e ? e->name : std::string("Scene"));
        s += ", \"start\": "; JInt(s, e ? e->startFrame : 1);
        s += ", \"end\": ";    JInt(s, e ? e->endFrame : 250);
        s += ", \"fps\": ";    JInt(s, e ? e->fps : 30);
        // pista "CAMARA ACTIVA" (los cortes de una cinematica): frame -> nombre de la camara
        if (e && !e->camTrack.empty() && !e->camTrack[0].keyframes.empty()) {
            s += ",\n        \"activeCamera\": [";
            const std::vector<keyFrame>& k = e->camTrack[0].keyframes;
            for (size_t q = 0; q < k.size(); q++) {
                const int idx = (int)(k[q].value + 0.5f);
                if (q) s += ", ";
                s += "{ \"frame\": "; JInt(s, k[q].frame); s += ", \"camera\": ";
                JEsc(s, (idx >= 0 && idx < (int)e->camNames.size()) ? e->camNames[idx] : std::string());
                s += " }";
            }
            s += "]";
        }
        // las TIRAS de esqueleto (los clips que tocan los armatures)
        if (e && !e->skeletons.empty()) {
            s += ",\n        \"armatures\": [";
            for (size_t q = 0; q < e->skeletons.size(); q++) {
                if (q) s += ", ";
                s += "{ \"armature\": "; JEsc(s, e->skeletons[q].armature); s += ", \"clip\": "; JEsc(s, e->skeletons[q].clip);
                s += ", \"frame\": "; JInt(s, e->skeletons[q].frame); s += " }";
            }
            s += "]";
        }
        const std::vector<AnimationObject>& objs = SceneCurves((int)yes);
        bool hasAny = false;
        for (size_t i = 0; i < objs.size() && !hasAny; i++)
            if (objs[i].obj && AnimObjHasKeys(objs[i]) && !W3dIsGenerated(objs[i].obj)) hasAny = true;
        if (hasAny) {
            s += ",\n        \"objects\": [\n";
            WriteObjectCurves(s, objs, "          ", NULL);
            s += "\n        ]";
        }
        s += " }";
        if (k + 1 < esc.size()) s += ",";
        s += "\n";
    }
    s += "    ]\n";
    s += "  },\n";
}

// LA BIBLIOTECA DE CLIPS DE JERARQUIA de 'o' ("clipsJerarquia": el animset que le toco en el plan del
// guardado, SaveAnimSetsHierOf: la suya, u otra igual con la que se deduplico)
// ...su RETARGET PROPIO ("retarget": "completo" | "rotaciones"; un armature, el de sus clips de esqueleto, que
// solo se escribe si no es el de siempre; cualquier otro objeto, el de sus clips de jerarquia, que solo se escribe
// si pisa el de cada clip) y sus CAPAS del Mix ("layersHier", la forma de siempre de las capas). Sin nada de eso no
// se escribe nada (el archivo no cambia).
static void WriteClipsHierarchy(std::string& s, const Object* o, int ind) {
    std::string lib;
    // (en seco, la biblioteca por su nombre en memoria: W3dHierAssign la encuentra aunque no tenga entrada)
    const bool hasLib = gDry ? (o->clipsHier && !o->clipsHier->animset.empty() && (lib = o->clipsHier->animset, true))
                              : SaveAnimSetsHierOf(o, lib);
    if (hasLib) { s += ",\n"; JSang(s, ind); s += "\"hierarchyClips\": "; JEsc(s, lib); }
    int ret = -1;
    if (const_cast<Object*>(o)->getType() == ObjectType::armature) {
        const Armature* a = (const Armature*)o;
        if (a->retarget == W3D_RETARGET_ROTATIONS) ret = W3D_RETARGET_ROTATIONS;
    } else if (o->clipsHier && o->clipsHier->retarget >= 0) ret = o->clipsHier->retarget;
    if (ret >= 0) {
        s += ",\n"; JSang(s, ind); s += "\"retarget\": ";
        s += (ret == W3D_RETARGET_ROTATIONS) ? "\"rotations\"" : "\"complete\"";
    }
    if (o->clipsHier && !o->clipsHier->layers.empty()) {
        s += ",\n"; JSang(s, ind); s += "\"hierLayers\": "; WriteLayers(s, o->clipsHier->layers, ind);
    }
}

// FLIPBOOKS CON NOMBRE de la escena (assets compartidos). Bloque raiz "flipbooks": solo la
// config (nombre/atlas/grilla/fps); las 8 curvas de UV se regeneran al cargar (GenerateCells).
// Sin flipbooks no se escribe nada (proyecto viejo sale igual).
static void WriteFlipbooks(std::string& s) {
    // (los de una LIBRERIA -Flipbook::libreria, o un atlas "lib:..."- no son del proyecto: se registran al usarla)
    std::vector<const Flipbook*> fs;
    for (size_t i = 0; i < SceneFlipbooks.size(); i++)
        if (SceneFlipbooks[i] && SceneFlipbooks[i]->lib.empty() && SceneFlipbooks[i]->atlas.compare(0, 4, "lib:") != 0)
            fs.push_back(SceneFlipbooks[i]);
    if (fs.empty()) return;
    s += "  \"flipbooks\": [\n";
    for (size_t i = 0; i < fs.size(); i++) {
        const Flipbook* f = fs[i];
        s += "    { \"name\": "; JEsc(s, f->name);
        s += ", \"atlas\": ";     JEsc(s, f->atlas);
        s += ", \"cols\": ";      JInt(s, f->cols);
        s += ", \"rows\": ";     JInt(s, f->rows);
        s += ", \"frames\": ";   JInt(s, f->frames);
        s += ", \"fps\": ";       JNum(s, f->fps);
        if (f->crossfade) s += ", \"crossfade\": true";
        s += " }";
        if (i + 1 < fs.size()) s += ",";
        s += "\n";
    }
    s += "  ],\n";
}

// ===========================================================================
//  EL OBJETO SE GUARDA EN REPOSO, NO EN LA POSE DEL PLAYHEAD
//
//  Hermano exacto de W3dRestVertexAnim (que hace lo mismo con la GEOMETRIA de
//  una malla, ver VertexAnimation.h). El playhead es estado de la SESION, no del
//  modelo: guardar con el cursor en el frame 50 tiene que dar el MISMO archivo
//  que guardar sin haberlo movido. Sin esto el .w3d salia con
//  {"nombre":"Cubo.001","pos":[...]} = la posicion donde lo dejo el scrub, o sea
//  la animacion HORNEADA encima del transform del objeto: reabrir y volver al
//  frame 1 daba un objeto que ya no estaba donde el usuario lo puso.
//
//  QUE TRANSFORM SE ESCRIBE: el del FRAME DE INICIO de la animacion que poso al
//  objeto (el 'startFrame' de SU animacion de escena, o el de su anim propia). Es el
//  analogo del "cuadro base" que usa W3dRestVertexAnim, es INDEPENDIENTE del
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
struct RestOne {
    Object*    o;
    Vector3    pos, scale, rotEuler;
    Quaternion rot;
    bool       visible, renderable;
    // camara
    float fov, nearClip, farClip, focusDist;
    // luz
    float dif[3], amb[3], spe[3];
    float attC, attL, attQ, spotCut, spotExp, intens;
    GLenum lightID;
    bool  directional;
};

class RestAnimObjects {
    public:
        // QUE SE CUBRE YA NO DEPENDE DEL ActiveAnimKind (el kind solo decide el ORDEN, o sea
        // quien gana si dos fuentes animan el mismo canal). Antes SI: cubria solo el kind 0 (escena) y el
        // 3 (anim propia de la malla activa) y se iba con un 'return' en los demas, asi que
        // con el timeline en un clip de armature (1), en "Es un juego" (2) o en un clip de
        // armature 2D (4) el .w3d salia con la pose del scrub HORNEADA en el transform. La
        // pose no se borra al cambiar de kind: el objeto queda posado donde lo dejo el
        // playhead, guarde con el timeline donde guarde. Lo mismo con la animacion de escena
        // que deja de ser la ACTIVA (SetActiveScene hace swap de curvas, pero los objetos
        // que poso siguen posados) y con la malla que se scrubeo en kind 3 y ya no es la
        // ActiveAnimMesh.
        //
        // Entonces se cubre TODA fuente que pueda haber posado a alguien: las curvas de
        // TODAS las animaciones de escena (la activa y las que no) y las curvas propias de
        // TODAS las mallas. Lo que MANDA (se aplica ULTIMO, o sea que gana el canal
        // compartido) es la fuente que el playhead esta aplicando ahora: con kind 3 la anim
        // propia de la malla, en cualquier otro caso la escena activa. Restaurar sigue siendo
        // por SNAPSHOT (~RestAnimObjects), asi que aparecer en varias listas no mueve nada.
        RestAnimObjects() {
            const bool ownRules = (ActiveAnimKind == 3);   // el playhead esta en la anim propia de una malla
            InitSceneAnimations();
            // 0) lo que poso el MIX (las capas de escena y las de clips de jerarquia de cada raiz) vuelve a su
            //    BASE: la transformacion que tenia antes de que el Mix lo tocara. Una raiz que solo anima el Mix
            //    (sus capas usan un clip sin vista abierta) no esta en ninguna lista de curvas y se guardaba con
            //    la mezcla del frame del cabezal. Va PRIMERO: lo que ademas tiene curvas vuelve despues al cuadro
            //    base de su animacion, como siempre.
            MixBases();
            // 1) las animaciones de escena que NO son la activa: sus curvas viven en el
            //    SceneAnimation (el swap las guardo ahi), pero los objetos que posaron
            //    siguen posados.
            for (size_t yes = 0; yes < SceneAnimations.size(); yes++) {
                if ((int)yes == ActiveSceneAnim || !SceneAnimations[yes]) continue;
                std::vector<AnimationObject>& objs = SceneAnimations[yes]->objects;
                const int base = SceneAnimations[yes]->startFrame;
                for (size_t i = 0; i < objs.size(); i++)
                    Apply(objs[i].obj, objs[i].Properties, base);
            }
            if (ownRules) { ActiveScene(); Meshes(); }
            else             { Meshes(); ActiveScene(); }
            // un JUEGO editando uno de sus clips se guarda en su ESTADO BASE (su frame 1), no en el
            // cuadro de inicio del clip: lo reposado vuelve a la foto que se tomo al salir de "Juego"
            // (el destructor le devuelve a cada uno la pose de la pantalla, igual que siempre)
            if (W3dGameBaseHas())
                for (size_t i = 0; i < previous.size(); i++) W3dGameBaseApplyTo(previous[i].o);
        }
        ~RestAnimObjects() {
            // al reves de como se guardaron: si un objeto entro dos veces (no deberia,
            // pero un archivo raro puede duplicarlo) gana el estado ORIGINAL
            for (size_t i = previous.size(); i-- > 0; ) {
                const RestOne& g = previous[i];
                Object* o = g.o;
                if (!o) continue;
                o->pos = g.pos; o->scale = g.scale; o->SetRotSnapshot(g.rot, g.rotEuler);
                o->visible = g.visible; o->renderable = g.renderable;
                if (o->getType() == ObjectType::camera) {
                    Camera* c = (Camera*)o;
                    c->fov = g.fov; c->nearClip = g.nearClip; c->farClip = g.farClip; c->focusDist = g.focusDist;
                } else if (o->getType() == ObjectType::light) {
                    Light* l = (Light*)o;
                    for (int k = 0; k < 3; k++) { l->diffuse[k] = g.dif[k]; l->ambient[k] = g.amb[k]; l->specular[k] = g.spe[k]; }
                    l->attConstant = g.attC; l->attLinear = g.attL; l->attQuadratic = g.attQ;
                    l->spotCutoff = g.spotCut; l->spotExponent = g.spotExp; l->intensity = g.intens;
                    l->SetLightID(g.lightID);
                    l->directional = g.directional;
                }
            }
        }
    private:
        // las curvas de la animacion de escena ACTIVA: viven en el global AnimationObjects
        // (invariante del swap, ver Animation.h), no en su SceneAnimation.
        void ActiveScene() {
            if (ActiveSceneAnim < 0 || ActiveSceneAnim >= (int)SceneAnimations.size()) return;
            if (!SceneAnimations[ActiveSceneAnim]) return;
            const int base = SceneAnimations[ActiveSceneAnim]->startFrame;
            for (size_t i = 0; i < AnimationObjects.size(); i++)
                Apply(AnimationObjects[i].obj, AnimationObjects[i].Properties, base);
        }
        // la anim PROPIA (kind 3) de UNA malla: su reposo es el inicio de ESA anim
        void Own(Object* o) {
            if (!o || o->getType() != ObjectType::mesh) return;
            Mesh* m = (Mesh*)o;
            VertexAnimationActive* va = FindTargetAnim(m);
            const int idx = va ? va->currentAnim : -1;
            if (idx < 0 || idx >= (int)m->animations.size() || !m->animations[idx]) return;
            VertexAnimation* an = m->animations[idx];
            Apply(o, an->curves, an->startFrame);
        }
        void Traverse(Object* o, Object* skip) {
            if (!o) return;
            if (o != skip) Own(o);
            for (size_t i = 0; i < o->Childrens.size(); i++) Traverse(o->Childrens[i], skip);
        }
        // TODAS las mallas, con la del timeline (kind 3) al final: es la que manda si el
        // playhead esta parado ahi. Una malla sin curvas propias no cuesta nada (Aplicar
        // se va sin tocar nada si no hay keyframes).
        void Meshes() {
            Traverse(SceneCollection, (Object*)ActiveAnimMesh);
            if (ActiveAnimMesh) Own((Object*)ActiveAnimMesh);
        }
        // las bases del Mix (ver el paso 0 del constructor)
        void MixBases() {
            std::vector<Object*> os; std::vector<Vector3> bp, br, be;
            W3dMixScenesBases(os, bp, br, be);
            for (size_t i = 0; i < os.size(); i++) {
                Object* o = os[i];
                if (!o) continue;
                Snapshot(o);
                o->pos = bp[i]; o->SetRotEuler(br[i]); o->scale = be[i];
            }
        }
        void Apply(Object* o, std::vector<AnimProperty>& props, int frameBase) {
            if (!o) return;
            // SOLO lo que el playhead poso DE VERDAD (Object::posedByCurves, que prende
            // W3dApplyCurvesInFrame). "Tener curvas" no alcanza: una malla con curvas propias
            // que el timeline nunca aplico (no estaba en su animacion) se mueve A MANO, y
            // re-posarla aca escribia el cuadro base y PERDIA el movimiento del usuario, en
            // silencio y en el flujo mas comun (abrir el proyecto, arrastrar la malla, Ctrl+S).
            if (!o->posedByCurves) return;
            bool has = false;
            for (size_t p = 0; p < props.size() && !has; p++)
                if (!props[p].keyframes.empty()) has = true;
            if (!has) return;   // sin keyframes no hay pose que deshacer (caso normal: gratis)
            Snapshot(o);
            W3dApplyCurvesInFrame(o, props, frameBase);
        }
        // la pose de la pantalla de 'o' (el destructor se la devuelve)
        void Snapshot(Object* o) {
            RestOne g;
            g.o = o;
            o->UpdateDisplayRot();
            g.pos = o->pos; g.scale = o->scale; g.rot = o->Rot(); g.rotEuler = o->rotEuler;
            g.visible = o->visible; g.renderable = o->renderable;
            g.fov = g.nearClip = g.farClip = g.focusDist = 0.0f;
            for (int k = 0; k < 3; k++) { g.dif[k] = g.amb[k] = g.spe[k] = 0.0f; }
            g.attC = g.attL = g.attQ = g.spotCut = g.spotExp = 0.0f; g.intens = 1.0f;
            g.lightID = GL_LIGHT0; g.directional = false;
            if (o->getType() == ObjectType::camera) {
                Camera* c = (Camera*)o;
                g.fov = c->fov; g.nearClip = c->nearClip; g.farClip = c->farClip; g.focusDist = c->focusDist;
            } else if (o->getType() == ObjectType::light) {
                Light* l = (Light*)o;
                for (int k = 0; k < 3; k++) { g.dif[k] = l->diffuse[k]; g.amb[k] = l->ambient[k]; g.spe[k] = l->specular[k]; }
                g.attC = l->attConstant; g.attL = l->attLinear; g.attQ = l->attQuadratic;
                g.spotCut = l->spotCutoff; g.spotExp = l->spotExponent; g.intens = l->intensity;
                g.lightID = l->LightID; g.directional = l->directional;
            }
            previous.push_back(g);
        }
        std::vector<RestOne> previous;
        RestAnimObjects(const RestAnimObjects&);              // sin copia (C++03)
        RestAnimObjects& operator=(const RestAnimObjects&);
};
} // namespace

// ===========================================================================
//  LAS OTRAS RAICES DEL PROYECTO (escenas 3D y prefabs, ver W3dRoots.h): cada una
//  va a SU entrada, con el MISMO escritor de objetos (asi sus mallas, materiales,
//  animsets y scripts entran al contenedor como los de la escena del bloque).
// ===========================================================================
// el nombre de entrada de cada fila del registro ("" = la del bloque, que va en project.json):
// scenes/<slug>.w3de y prefabs/<slug>.w3dp, sin repetir. Sale del NOMBRE: re-guardar sin
// cambios da las mismas entradas; renombrar la escena muda su entrada.
static void RootEntries(W3dContainerWriter* esc, std::vector<std::string>& out) {
    const std::vector<W3dRootRow>& fs = W3dRoots();
    out.assign(fs.size(), std::string());
    std::set<std::string> taken;
    for (size_t i = 0; i < fs.size(); i++) {
        if (fs[i].block) continue;
        const bool pf = (fs[i].type == W3D_ROOT_PREFAB);
        const std::string carp = pf ? "prefabs/" : "scenes/";
        const std::string ext  = pf ? ".w3dp" : ".w3de";
        const std::string slug = W3dSlugEntry(fs[i].name);
        std::string n = carp + slug + ext;
        for (int k = 2; taken.count(n) || esc->Has(n); k++) {
            char sf[16]; snprintf(sf, sizeof(sf), "-%d", k);
            n = carp + slug + sf + ext;
        }
        taken.insert(n);
        out[i] = n;
    }
}

// el JSON de UNA raiz, que en este momento es SceneCollection (con SU contexto: sus animaciones
// de escena y sus capas del Mix). Las animaciones van antes que los objetos solo para que el
// JSON cierre sin coma colgando (el lector no mira el orden de las claves).
static void WriteContentRoot(std::string& s, const W3dRootRow& f, CtxSave* cx) {
    s += "{\n";
    s += "  \"version\": 4,\n";
    s += "  \"name\": "; JEsc(s, f.name); s += ",\n";
    WriteAnimationsScene(s);
    if (!g_mixScenes.empty()) { s += "  \"mix\": { \"scenes\": "; WriteLayers(s, g_mixScenes, 2); s += " },\n"; }
    RestAnimObjects rest;   // (el playhead no se hornea: ver SaveW3D)
    const std::vector<Object*>& ch = SceneCollection->Childrens;
    bool first = true;
    if (f.type == W3D_ROOT_PREFAB) {
        // "raiz": el objeto que una instancia genera (el primero del primer nivel). Lo que haya
        // quedado suelto a su lado se conserva en "sueltos" (la instancia no lo usa).
        s += "  \"root\":";
        if (ch.empty()) s += " null";
        else { s += "\n"; WriteObject(s, ch[0], 1, cx, &first); }
        if (ch.size() > 1) {
            s += ",\n  \"loose\": [\n";
            first = true;
            for (size_t i = 1; i < ch.size(); i++) WriteObject(s, ch[i], 2, cx, &first);
            s += "\n  ]";
        }
        s += "\n";
    } else {
        s += "  \"objects\": [\n";
        for (size_t i = 0; i < ch.size(); i++) WriteObject(s, ch[i], 2, cx, &first);
        s += "\n  ]\n";
    }
    s += "}\n";
}

// el registro en project.json (solo si hay algo que decir: una escena "Scene" sola no se escribe).
// "escenas3d" lleva las escenas Y los juegos (comparten nombres: cambiarEscena los nombra igual),
// cada uno con su "tipo"; "prefabs", los prefabs.
static void WriteRegistryRoots(std::string& s, const std::vector<std::string>& entries) {
    if (!W3dRootsHasRegistry()) return;
    const std::vector<W3dRootRow>& fs = W3dRoots();
    static const int kClasses[2] = { W3D_ROOT_SCENE, W3D_ROOT_PREFAB };
    for (int k = 0; k < 2; k++) {
        const int cls = kClasses[k];
        bool has = false;
        for (size_t i = 0; i < fs.size() && !has; i++) has = (W3dRootClass(fs[i].type) == cls);
        if (!has) continue;
        s += (cls == W3D_ROOT_SCENE) ? "  \"scenes3d\": [\n" : "  \"prefabs\": [\n";
        bool first = true;
        for (size_t i = 0; i < fs.size(); i++) {
            if (W3dRootClass(fs[i].type) != cls) continue;
            if (!first) s += ",\n";
            first = false;
            s += "    { \"name\": "; JEsc(s, fs[i].name);
            // el tipo EFECTIVO (el de un proyecto viejo sale de su contenido: al reabrir es el mismo)
            if (cls == W3D_ROOT_SCENE) { s += ", \"type\": "; JEsc(s, W3dRootTypeKey(W3dRootTypeOf((int)i))); }
            if (!entries[i].empty()) { s += ", \"entry\": "; JEsc(s, entries[i]); }
            // (la carpeta es COSMETICA: con la barra adelante si parece un nombre de entrada, como las
            //  de los demas recursos, asi la verificacion del guardado no la toma por una referencia)
            if (!fs[i].folder.empty()) { s += ", \"folder\": "; JEsc(s, W3dCosmeticPathJson(fs[i].folder)); }
            s += " }";
        }
        s += "\n  ],\n";
        if (cls == W3D_ROOT_SCENE && !W3dInitialRoot().empty()) {
            s += "  \"initialScene3d\": "; JEsc(s, W3dInitialRoot()); s += ",\n";
        }
        // las escenas ABIERTAS en el editor y las del ojo cerrado (cosa del editor: el juego no las mira)
        if (cls == W3D_ROOT_SCENE) W3dOpenRootsWrite(s);
    }
}

// ===========================================================================
//  LA DEFINICION EN MEMORIA DE UN PREFAB (io/Prefabs.h): su objeto raiz escrito EN SECO (ver gDry) como
//  {"raiz": <objeto>}. Es lo que generan sus instancias mientras el prefab se edita sin guardar.
// ===========================================================================
static bool SerializePrefabDry(Object* root, std::string& json) {
    if (!root) return false;
    CtxSave cx;
    cx.esc = NULL;
    cx.vtxN = 0;
    cx.error = false;
    const std::string whoPrev = gWho;
    gDry = true;
    std::string s = "{\n  \"root\":\n";
    bool first = true;
    WriteObject(s, root, 1, &cx, &first);
    s += "\n}\n";
    gDry = false;
    gWho = whoPrev;
    json.swap(s);
    return !cx.error;
}
// UNA ESCENA CARGADA como definicion de sus instancias (io/Prefabs.h: "escena:<nombre>"): sus objetos de primer nivel
// debajo de un objeto raiz con el nombre de la escena, EN SECO, menos lo que es de la escena y no de sus objetos (las
// UI 2D, las rutinas constructor y las que limpian la pantalla)
static bool IsFromScene(Object* o) {
    if (!o) return true;
    if (o->getType() == ObjectType::ui) return true;
    if (o->getType() != ObjectType::routine) return false;
    Routine* r = (Routine*)o;
    if (r->constructor) return true;
    for (int m = 0; m < Routine::ModeN; m++)
        for (size_t i = 0; i < r->lists[m].size(); i++)
            if (r->lists[m][i].type == StepClear || r->lists[m][i].type == StepColorClear) return true;
    return false;
}
static bool SerializeSceneDry(Object* rootScene, const std::string& name, std::string& json) {
    if (!rootScene) return false;
    CtxSave cx;
    cx.esc = NULL;
    cx.vtxN = 0;
    cx.error = false;
    const std::string whoPrev = gWho;
    gDry = true;
    std::string s = "{\n  \"root\": {\n    \"type\": \"object\",\n    \"name\": ";
    JEsc(s, name);
    s += ",\n    \"children\": [\n";
    bool first = true;
    for (size_t i = 0; i < rootScene->Childrens.size(); i++)
        if (!IsFromScene(rootScene->Childrens[i])) WriteObject(s, rootScene->Childrens[i], 3, &cx, &first);
    s += "\n    ]\n  }\n}\n";
    gDry = false;
    gWho = whoPrev;
    json.swap(s);
    return !cx.error;
}
// UNA RAIZ ENTERA EN SECO ("Convert to Game", io/EditorRoots.h: la escena se DUPLICA como juego): TODOS sus objetos de
// primer nivel -tambien las UI 2D y las rutinas de la escena: la copia es una raiz entera, no una instancia- como
// {"objetos": [...]}, cada nodo con su "_origen". Van TAL COMO ESTAN EN PANTALLA: sin el reposo del guardado
// (RestAnimObjects), porque la pose del cuadro actual es justamente lo que se copia. Las animaciones de escena no van.
// La lee W3dRootReadCopyDry (importers/import_w3d.cpp).
bool W3dRootSerializeDry(Object* root, std::string& json) {
    if (!root) return false;
    CtxSave cx;
    cx.esc = NULL;
    cx.vtxN = 0;
    cx.error = false;
    const std::string whoPrev = gWho;
    gDry = true;
    std::string s = "{\n  \"objects\": [\n";
    bool first = true;
    for (size_t i = 0; i < root->Childrens.size(); i++) WriteObject(s, root->Childrens[i], 2, &cx, &first);
    s += "\n  ]\n}\n";
    gDry = false;
    gWho = whoPrev;
    json.swap(s);
    return !cx.error;
}
namespace {
struct RegisterSerializerPrefab {
    RegisterSerializerPrefab() { W3dPrefabSerializeHook = SerializePrefabDry; W3dSceneSerializeHook = SerializeSceneDry; }
} gRegisterSerializerPrefab;
}

// vuelve a la raiz que estaba activa pase lo que pase (el guardado cambia de raiz para escribir cada una)
namespace {
struct ReturnToRoot {
    int idx;
    explicit ReturnToRoot(int i) : idx(i) {}
    ~ReturnToRoot() { if (idx >= 0) W3dRootUse(idx); }
};
}

// ---------------------------------------------------------------------------
//  guardar el proyecto completo (v3: el .w3d ES el JSON, plano y editable)
// ---------------------------------------------------------------------------

// ============================================================================
//  EL PORTAPAPELES DE OBJETOS (Ctrl+C / Ctrl+X / Ctrl+V, el menu del click derecho): copiar escribe lo elegido en una
//  CARPETA (la del portapapeles, compartida por todos los Whisk3D de la maquina) con TODO lo que hace falta para
//  armarlo en otro proyecto: los objetos (en seco, como la definicion de un prefab: sus mallas por el nombre del
//  recurso), los BYTES de cada malla (.w3db), sus materiales y los archivos que nombran (texturas, shaders) copiados
//  al lado. Pegar (import_w3d.cpp: W3dClipboardPaste) registra las mallas con un nombre libre, trae los materiales
//  y arma los objetos con el lector de siempre.
// ============================================================================
static void CollectMeshesClip(Object* o, std::vector<Mesh*>& out) {
    if (!o) return;
    if (o->getType() == ObjectType::mesh) out.push_back((Mesh*)o);
    for (size_t i = 0; i < o->Childrens.size(); i++) CollectMeshesClip(o->Childrens[i], out);
}
static bool IsFileCopyable(const std::string& v) {
    const size_t p = v.find_last_of('.');
    if (p == std::string::npos || v.size() > 512) return false;
    std::string e = v.substr(p + 1);
    for (size_t i = 0; i < e.size(); i++) if (e[i] >= 'A' && e[i] <= 'Z') e[i] = (char)(e[i] - 'A' + 'a');
    return e == "png" || e == "jpg" || e == "jpeg" || e == "tga" || e == "bmp" || e == "webp" || e == "gif" ||
           e == "ktx" || e == "glsl" || e == "hdr";
}
bool W3dClipboardCopy(const std::vector<Object*>& objs, const std::string& folder) {
    if (objs.empty()) return false;
    MkdirRec(folder + "/meshes"); MkdirRec(folder + "/files");
    CtxSave cx; cx.esc = NULL; cx.vtxN = 0; cx.error = false; cx.dirW3d = folder;
    const std::string whoPrev = gWho;
    // LOS OBJETOS, en seco (cada uno con sus hijos)
    std::string so = "  \"objects\": [\n";
    gDry = true;
    bool first = true;
    for (size_t i = 0; i < objs.size(); i++) WriteObject(so, objs[i], 1, &cx, &first);
    gDry = false;
    so += "\n  ]\n";
    // LAS MALLAS: los bytes de cada recurso que usan (una vez)
    std::vector<Mesh*> ms;
    for (size_t i = 0; i < objs.size(); i++) CollectMeshesClip(objs[i], ms);
    // (lo editado que todavia no se publico en su recurso: la geometria de HOY, no la del ultimo guardado)
    { extern bool W3dMeshPublish(Mesh* m); for (size_t i = 0; i < ms.size(); i++) W3dMeshPublish(ms[i]); }
    std::vector<MeshResource*> rs;
    std::string sm = "  \"meshes\": [";
    for (size_t i = 0; i < ms.size(); i++) {
        if (ms[i]->mesh == NULL) continue;
        bool already = false; for (size_t k = 0; k < rs.size() && !already; k++) if (rs[k] == ms[i]->mesh) already = true;
        if (already) continue;
        std::string bytes;
        if (!W3dMeshResourceReadBytes(ms[i]->mesh, bytes) || bytes.empty()) continue;
        char name[32]; snprintf(name, sizeof(name), "meshes/%d.w3db", (int)rs.size());
        FILE* f = fopen((folder + "/" + name).c_str(), "wb");
        if (!f) continue;
        fwrite(bytes.data(), 1, bytes.size(), f); fclose(f);
        if (!rs.empty()) sm += ",";
        sm += "\n    { \"name\": "; JEsc(sm, ms[i]->mesh->name); sm += ", \"file\": "; JEsc(sm, name); sm += " }";
        rs.push_back(ms[i]->mesh);
    }
    sm += "\n  ],\n";
    // LOS MATERIALES de esas mallas (el bloque de siempre, con las rutas como estan en memoria)
    gMats.clear();
    for (size_t i = 0; i < ms.size(); i++) MatRegister(ms[i]);
    std::string smat;
    WriteMaterials(smat, &cx);
    gMats.clear();
    gWho = whoPrev;
    // los ARCHIVOS que nombran (texturas, shaders): se copian al lado y el JSON los nombra desde la carpeta
    std::string all = smat + so;
    std::string out; out.reserve(all.size());
    std::map<std::string, std::string> copied;
    for (size_t i = 0; i < all.size(); ) {
        if (all[i] != '"') { out += all[i++]; continue; }
        size_t j = i + 1; std::string val;
        while (j < all.size() && all[j] != '"') { if (all[j] == '\\' && j + 1 < all.size()) { val += all[j+1]; j += 2; continue; } val += all[j++]; }
        const std::string lit = all.substr(i, j + 1 - i);
        i = j + 1;
        if (IsFileCopyable(val)) {
            std::map<std::string, std::string>::iterator c = copied.find(val);
            if (c == copied.end()) {
                std::vector<unsigned char> b;
                if (w3dFileSystem::ReadFileBytes(val, b) && !b.empty()) {
                    char pre[16]; snprintf(pre, sizeof(pre), "%d_", (int)copied.size());
                    const std::string newValue = std::string("files/") + pre + Base(val);
                    FILE* f = fopen((folder + "/" + newValue).c_str(), "wb");
                    if (f) { fwrite(&b[0], 1, b.size(), f); fclose(f); copied[val] = newValue; }
                }
                c = copied.find(val);
            }
            if (c != copied.end()) { std::string q; JEsc(q, c->second); out += q; continue; }
        }
        out += lit;
    }
    std::string s = "{\n  \"whisk3dClipboard\": 1,\n";
    s += sm; s += out; s += "}\n";
    FILE* f = fopen((folder + "/clipboard.json").c_str(), "wb");
    if (!f) return false;
    fwrite(s.data(), 1, s.size(), f); fclose(f);
    return !cx.error;
}

bool SaveW3D(const std::string& path) {
    if (!SceneCollection) return false;
    // ---- CON EL PLAY ANDANDO: la ESTRUCTURA del usuario ------------------------------------------
    //  Jugando, lo que un script destruyo esta DESCOLGADO hasta el Stop y lo que instanciar() creo son nodos
    //  reales que el Stop borra. Guardar asi perdia para siempre los objetos destruidos y escribia los creados.
    //  Mientras dura el guardado lo destruido vuelve a su lugar y lo creado sale del arbol (SimGame.cpp); al
    //  salir (salga o no el guardado) todo queda como estaba y la partida sigue. Lo que los scripts movieron se
    //  guarda como esta, igual que siempre.
    struct PlayStructureUser {
        PlayStructureUser() { extern void SimSavedStart(); SimSavedStart(); }
        ~PlayStructureUser() { extern void SimSavedEnd(); SimSavedEnd(); }
    } playStructure;
    // una CURVE a medio editar (proxy de nodos abierto): volcar los nodos ANTES de serializar
    { extern void W3dCurveEditClose(); W3dCurveEditClose(); }
    // ---- FRENO: UNA ESCENA UI DEL PROYECTO NO CARGO --------------------------------------
    //  El .w3dui faltaba o no parseaba, asi que su nodo NO se creo. Guardar escribe un
    //  project.json SIN esa escena: los bytes del .w3dui sobreviven (PreserveTransient) pero
    //  ya no los referencia nadie, o sea que el usuario pierde la escena de vista y no se
    //  entera. Mismo criterio que una malla o una vertex anim que no cargo: no se guarda
    //  encima y se dice por que. Reabrir el proyecto arreglado limpia el freno.
    if (!g_w3dUINotLoaded.empty()) {
        w3dLogfE("[W3D] la escena UI '%s' no cargo al abrir: NO guardo encima (se perderia del proyecto)",
                 g_w3dUINotLoaded.c_str());
        W3dAvisof(true, T("Not saving: the UI scene '%s' could not be read when opening and saving would remove it from the project"),
                  W3dShortName(g_w3dUINotLoaded).c_str());
        return false;
    }
    // ---- TODAS LAS RAICES DEL PROYECTO (escenas 3D y prefabs, ver W3dRoots.h) --------------
    //  El guardado las necesita CARGADAS: cada una se re-escribe con los nombres de HOY de los
    //  materiales, mallas, animsets y texturas que nombra (una entrada vieja copiada tal cual
    //  podria nombrar un recurso renombrado o purgado desde que se guardo), y los nombres de
    //  entrada que el guardado reparte (.w3dui, texturas) no pueden pisar los de una que no se ve.
    //  Las que se cargan SOLO para esto se DESCARGAN al salir (salga o no el guardado): ya estan en su
    //  entrada y se vuelven a leer de ahi si se las abre. Si no, desde el primer Ctrl+S todo el proyecto
    //  quedaba en memoria hasta cerrarlo (la carga perezosa del editor se perdia).
    struct UnloadOnExit {
        std::vector<int> rows;
        ~UnloadOnExit() { W3dRootsUnload(rows); }
    } unload;
    W3dRootsLoadAll(&unload.rows);
    // la VISTA de un clip de jerarquia que se esta editando se escribe en su clip (la biblioteca es lo
    // que se guarda; las de las raices que no se miran se escribieron al dejarlas)
    W3dClipsViewsSync();
    {
        const std::vector<W3dRootRow>& fs = W3dRoots();
        for (size_t i = 0; i < fs.size(); i++)
            if (fs[i].loadFailed) {
                w3dLogfE("[W3D] %s '%s' no se pudo leer al abrirla: NO guardo encima (se perderia)",
                         W3dRootTypeKey(fs[i].type), fs[i].name.c_str());
                W3dAvisof(true, T("Not saving: '%s' could not be read and saving would leave it empty"),
                          W3dShortName(fs[i].name).c_str());
                return false;
            }
    }
    gNotCovered = 0;
    gMats.clear();   // los materiales se juntan durante el recorrido de la escena

    // ------------------------------------------------------------------
    //  FORMATO v4: TODO el proyecto adentro de UN archivo (un zip estandar).
    //  Se escribe SIEMPRE asi; los formatos viejos (JSON plano v3, zip v2 y el
    //  texto Whisk3D{}) se siguen ABRIENDO y se MIGRAN en este mismo guardado.
    // ------------------------------------------------------------------
    W3dContainerWriter esc;
    esc.Start(path, W3dContainerReader());
    gEsc = &esc;
    gWho.clear();
    g_w3dRefEmit = W3dRefEmit;      // el .w3dui emite nombres de entrada (ver UI2DFormat.h)
    g_w3dDirProject = Folder(path);

    CtxSave cx;
    cx.esc = &esc;
    cx.dirW3d = Folder(path);
    cx.vtxN = 0;
    cx.error = false;
    MkdirRec(cx.dirW3d);   // "guardar como" a una carpeta que todavia no existe
    // LAS MALLAS COMO RECURSO (io/ProjectMeshes.h), en los DOS formatos: antes de recorrer la
    // escena se agrupan por contenido y cada malla entra UNA vez al contenedor (.w3db, o .w3dm
    // en texto). El plan se descarta al salir pase lo que pase (Confirmar lo consume si el
    // guardado sale).
    struct PlanMeshesGuard { ~PlanMeshesGuard() { W3dMeshesSaveDiscard(); } } planMeshes;
    const bool meshesResource = !g_w3dMeshesLegacy;
    if (meshesResource && !W3dMeshesSavePrepare(&esc)) cx.error = true;

    std::string s;
    s += "{\n";
    // "version" es el esquema de DATOS; "contenedor.format" es la version del
    // LAYOUT DE CARPETAS. Separados a proposito: un cambio de esquema no obliga a
    // mover archivos y al reves. PROHIBIDO meter aca fecha, usuario, hostname o
    // rutas de la maquina: cualquier campo volatil rompe el round-trip byte a byte.
    s += "  \"version\": 4,\n";
    // formato 2 = el contenedor estilo OpenDocument (mimetype primero + README.txt)
    s += "  \"container\": { \"format\": 2, \"generator\": \"Whisk3D\" },\n";
    // ICONO del juego: entra al contenedor bajo project/ (el PNG en maxima
    // definicion; Compilar juego genera de ahi los tamanos chicos)
    if (!g_projIcon.empty()) {
        gWho = "icono del proyecto";
        // EL ARTE FUENTE DEL ICONO VIAJA CON EL: al lado de icono.png suele haber un
        // icono.svg que NO lo referencia NADIE (es de donde sale el png). Una
        // migracion que solo camine las referencias lo perderia de vista y el dueno
        // se enteraria el dia que quiera reeditar el icono. Se traen los hermanos
        // con el MISMO nombre y otra extension, y solo eso: nada de arrastrar la
        // carpeta entera. Al re-guardar ya viajan como entradas preservadas.
        std::string folderIco = Folder(g_projIcon);
        std::string baseIco   = BaseNoExt(g_projIcon);
        bool wasFromDisk = !W3dIsEntryName(g_projIcon) &&
                          w3dFileSystem::FileExists(g_projIcon);
        std::string ref = esc.Ingest(g_projIcon, "project", gWho);
        if (wasFromDisk && ref.compare(0, 4, "ext:") != 0) {
            std::vector<w3dFileSystem::DirEntry> ents;
            if (!baseIco.empty() && w3dFileSystem::ListDir(folderIco, ents))
                for (size_t i = 0; i < ents.size(); i++) {
                    if (ents[i].isDir) continue;
                    std::string n = ents[i].name;
                    size_t pp = n.find_last_of('.');
                    if (pp == std::string::npos || n.substr(0, pp) != baseIco) continue;
                    std::string sibling = folderIco + "/" + n;
                    if (sibling == g_projIcon) continue;
                    std::string tmp = sibling;
                    esc.Ingest(tmp, "project", "arte fuente del icono");
                }
        }
        s += "  \"icon\": "; JEsc(s, ref); s += ",\n";
    }
    // MULTI-ESCENA: la escena que ARRANCA + el modo (antes NO se guardaban en el
    // v2 zip y se perdian al re-guardar un proyecto de texto viejo)
    if (!W3dInitialScene().empty()) {
        s += "  \"initialScene\": "; JEsc(s, W3dInitialScene()); s += ",\n";
    }
    if (W3dSceneMode()) s += "  \"sceneMode\": true,\n";
    // PIXELADO GLOBAL: solo se escribe si esta PRENDIDO (los .w3d que ya existen
    // no cambian ni un byte al re-guardarlos).
    if (w3dEngine::PixelatedGlobal()) s += "  \"pixelated\": true,\n";
    if (!w3dEngine::MipmapsGlobal()) s += "  \"mipmaps\": false,\n";   // default true: solo se guarda el apagado
    // EL TRAZADO DE RAYOS del proyecto (Propiedades > Render): solo si se toco algo (un proyecto comun sale igual)
    {
        const W3dRTOptions def;
        if (g_rt.on != def.on || g_rt.rays != def.rays || g_rt.samples != def.samples || g_rt.passes != def.passes ||
            g_rt.denoiser != def.denoiser || g_rt.gpu != def.gpu || g_rt.onlyFinal != def.onlyFinal || g_rt.scale != def.scale ||
            g_rt.layers != def.layers || g_rt.seedVariable != def.seedVariable || g_rt.upscaler != def.upscaler ||
            g_rt.blur != def.blur || g_rt.shutter != def.shutter || g_rt.shutterPos != def.shutterPos ||
            g_rt.bounces != def.bounces) {
            s += "  \"raytraced\": { \"on\": "; s += g_rt.on ? "true" : "false";
            s += ", \"rays\": "; JInt(s, g_rt.rays); s += ", \"samples\": "; JInt(s, g_rt.samples);
            s += ", \"passes\": "; JInt(s, g_rt.passes); s += ", \"denoiser\": "; JInt(s, g_rt.denoiser);
            s += ", \"gpu\": "; s += g_rt.gpu ? "true" : "false";
            s += ", \"finalOnly\": "; s += g_rt.onlyFinal ? "true" : "false";
            s += ", \"scale\": "; JInt(s, g_rt.scale); s += ", \"upscaler\": "; JInt(s, g_rt.upscaler);
            s += ", \"layers\": "; JInt(s, g_rt.layers);
            s += ", \"variableSeed\": "; s += g_rt.seedVariable ? "true" : "false";
            s += ", \"blur\": "; s += g_rt.blur ? "true" : "false";
            s += ", \"shutter\": "; JNum(s, g_rt.shutter); s += ", \"shutterPos\": "; JInt(s, g_rt.shutterPos);
            if (g_rt.bounces != def.bounces) { s += ", \"bounces\": "; JInt(s, g_rt.bounces); }
            s += " },\n";
        }
    }
    // EL POSTPROCESADO (render/PostProcess.h): la pila de efectos en orden. Solo si hay algo (prendido o con efectos)
    if (false) {   // (el postprocesado es un OBJETO del arbol: se guarda con el, ver EscribirPostEfectos)
        const W3dPostOptions g_post;
        for (size_t i = 0; i < g_post.effects.size(); i++) {
            const W3dPostEffect& e = g_post.effects[i];
            if (i) s += ", ";
            s += "{ \"type\": "; JInt(s, e.type);
            s += ", \"active\": "; s += e.active ? "true" : "false";
            s += ", \"opt\": "; s += e.opt ? "true" : "false";
            const int nv = W3dPostNumbers(e.type);
            s += ", \"v\": ["; for (int k = 0; k < nv; k++) { if (k) s += ", "; JNum(s, e.v[k]); } s += "]";
            if (e.type == W3dPostBloom) {   // (el tinte: solo el que tiene color)
                s += ", \"col\": ["; for (int k = 0; k < 3; k++) { if (k) s += ", "; JNum(s, e.col[k]); } s += "]";
            }
            s += " }";
        }
        s += "] },\n";
    }
    if (w3dEngine::Textures16Always()) s += "  \"textures16\": true,\n";   // default false (solo las de 16 bits exactas)
    if (w3dEngine::Textures16Mobile()) s += "  \"textures16Mobile\": true,\n";   // default false (en el N95 como en PC)
    if (w3dEngine::TexturesHalfFrom() > 0) {   // default 0 (en el N95 todas a su tamano)
        char b[64]; snprintf(b, sizeof b, "  \"halfTexturesFrom\": %d,\n", w3dEngine::TexturesHalfFrom()); s += b;
    }
    // FORMATO DE LAS MALLAS: solo se escribe el que NO es el default (texto). Es la opcion del
    // PROYECTO; el forzado del harness no se guarda.
    if (g_w3dFormatMeshesProject == W3D_MESHES_TEXT) s += "  \"meshFormat\": \"text\",\n";
    // MALLAS SOLO EN GPU (objects/MeshResource.h): solo prendida (default false: los .w3d de siempre no cambian)
    if (W3dMeshesOnlyGPUActive()) s += "  \"gpuOnlyMeshes\": true,\n";
    // CONFIG de la tarjeta Juego (Compilar juego): los valores VIGENTES del
    // editor, con strings legibles para editarlos a mano. Un .w3d viejo sin el
    // bloque abre con los defaults (W3dCompileReset, ver import_w3d).
    s += "  \"compile\": {\n";
    s += "    \"windowMode\": "; JEsc(s, W3dCompileWindowModeStr(g_projCompile.windowMode)); s += ",\n";
    s += "    \"orientation\": "; JEsc(s, W3dCompileOrientationStr(g_projCompile.orientation)); s += ",\n";
    s += "    \"physics\": ";  s += g_projCompile.usePhysics ? "true" : "false"; s += ",\n";
    s += "    \"sound\": ";  s += g_projCompile.useSound ? "true" : "false"; s += ",\n";
    { char volbuf[16]; snprintf(volbuf, sizeof(volbuf), "%d", g_projCompile.volume);
      s += "    \"volume\": "; s += volbuf; s += ",\n"; }   // 0..100 volumen del gameplay
    { char resbuf[16]; snprintf(resbuf, sizeof(resbuf), "%d", g_projCompile.resolution);
      s += "    \"resolution\": "; s += resbuf; s += ",\n"; }   // 25..100 % de la pantalla (Android 2.3)
    s += "    \"debug\": ";   s += g_projCompile.debugMode  ? "true" : "false"; s += ",\n";
    s += "    \"assets\": ";  JEsc(s, W3dCompileAssetsStr(g_projCompile.assetsMode)); s += ",\n";
    s += "    \"platform\": "; JEsc(s, W3dCompilePlatformStr(g_projCompile.platform)); s += ",\n";
    s += "    \"backend\": "; JEsc(s, W3dCompileBackendStr(g_projCompile.backend)); s += ",\n";
    { char uidhex[16]; snprintf(uidhex, sizeof(uidhex), "0x%08X", g_projCompile.uid);
      s += "    \"uid\": "; JEsc(s, uidhex); s += "\n"; }   // UID3 de Symbian del juego (0 = sin asignar)
    s += "  },\n";
    // PALETAS del proyecto (v3): la fuente de verdad. Ademas se BAKEAN dentro
    // de cada .w3dui (UI2DSave) para el runtime standalone.
    {
        std::vector<Palette>& ps = W3dPalettes();
        if (!ps.empty()) {
            s += "  \"palettes\": [\n";
            for (size_t i = 0; i < ps.size(); i++) {
                s += "    { \"name\": "; JEsc(s, ps[i].name);
                s += ", \"colors\": [\n";
                for (size_t c = 0; c < ps[i].colors.size(); c++) {
                    const PaletteColor& pc = ps[i].colors[c];
                    s += "      { \"name\": "; JEsc(s, pc.name);
                    s += ", \"color\": [";
                    JNum(s, pc.rgba[0]); s += ", "; JNum(s, pc.rgba[1]); s += ", ";
                    JNum(s, pc.rgba[2]); s += ", "; JNum(s, pc.rgba[3]);
                    s += "] }";
                    if (c + 1 < ps[i].colors.size()) s += ",";
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
    // TOPE DE RENDER (distinto de fps=AnimFPS): solo se escribe si NO es el default 60, asi los proyectos que
    // no lo tocan quedan igual. Lo lee el loop de escritorio (Symbian lo clampea a 60).
    { extern int g_fpsCap; if (g_fpsCap != 60) { s += "  \"fpsCap\": "; JNum(s, (float)g_fpsCap); s += ",\n"; } }
    s += "  \"fullscreen\": "; s += cfg.fullscreen ? "true" : "false"; s += ",\n";
    // la VISTA PREVIA del streaming en el editor (io/Streaming.h): solo prendida (default apagada)
    { extern bool g_w3dStreamingPreviewView; if (g_w3dStreamingPreviewView) s += "  \"streamingPreview\": true,\n"; }
    // estado de REPRODUCCION al guardar (v3): reabrir el proyecto respeta si estaba
    // en PLAY o en PAUSA (guardado en pausa -> abre en pausa). En archivos viejos el
    // campo no existe y OpenW3D cae al auto-play de siempre (retrocompat).
    s += "  \"playing\": "; s += PlayAnimation ? "true" : "false"; s += ",\n";
    if (meshesResource) W3dMeshesSaveRegistry(s);   // "mallas": el registro (antes de los objetos que las nombran)
    // ANIMSETS: el plan (que animset y que clips le toca a cada armature) y sus entradas .w3da,
    // ANTES de recorrer la escena (el nodo de cada armature los nombra). Un animset que no se
    // pudo leer al abrir frena el guardado (ya avisado).
    // (NULL = los armatures de TODAS las raices: las escenas y prefabs que no se estan mirando tambien)
    if (!SaveAnimSetsPrepare(&esc, NULL)) cx.error = true;
    // ESCENAS 3D Y PREFABS: el registro y la entrada de cada uno (la escena del bloque va abajo)
    std::vector<std::string> entriesRoots;
    RootEntries(&esc, entriesRoots);
    WriteRegistryRoots(s, entriesRoots);
    // la escena del BLOQUE "escena" es la que se escribe aca (si se esta editando otra raiz, se usa
    // la del bloque mientras tanto: sus objetos, sus animaciones y su Mix)
    ReturnToRoot returnToRoot(W3dActiveRoot());
    if (!W3dRootUse(W3dRootBlock())) {
        // (sin la del bloque el bloque "escena" saldria con los objetos de la raiz activa, que ademas va a su
        //  entrada: repetidos al reabrir. W3dRootBlock siempre promueve una; esto es por las dudas)
        w3dLogfE("[W3D] no hay escena del bloque para escribir: NO guardo");
        cx.error = true;
    }
    s += "  \"scene\": {\n";
    s += "    \"objects\": [\n";
    {
        // EL PLAYHEAD NO SE HORNEA: mientras dura el recorrido de la escena, todo
        // objeto animado vuelve a su cuadro base y al cerrar el scope recupera su
        // pose EXACTA (ver el comentario grande de RestAnimObjects).
        RestAnimObjects rest;
        bool first = true;
        for (size_t i = 0; i < SceneCollection->Childrens.size(); i++)
            WriteObject(s, SceneCollection->Childrens[i], 3, &cx, &first);
    }
    s += "\n    ]\n";
    s += "  },\n";
    // ANIMACIONES DE ESCENA (las curvas de transform de los objetos): van DESPUES de
    // la escena porque referencian a los objetos por nombre y asi se leen en orden.
    WriteAnimationsScene(s);
    // ...y cada OTRA raiz a su entrada (.w3de / .w3dp), con el mismo contexto de escritura
    {
        const std::vector<W3dRootRow>& fs = W3dRoots();
        for (size_t i = 0; i < fs.size(); i++) {
            if (fs[i].block || entriesRoots[i].empty()) continue;
            if (!W3dRootUse((int)i)) { cx.error = true; continue; }
            std::string js;
            gWho = std::string(W3dRootTypeKey(fs[i].type)) + " \"" + fs[i].name + "\"";
            WriteContentRoot(js, fs[i], &cx);
            gWho.clear();
            if (!esc.AddBytes(entriesRoots[i], js)) cx.error = true;
        }
        W3dRootUse(returnToRoot.idx);   // la de verdad (la sesion y el layout son suyos)
    }
    WriteFlipbooks(s);   // assets de flipbook con nombre (config; las curvas se regeneran al cargar)
    // MATERIALES: van DESPUES de la escena porque la lista se arma recorriendola (orden de
    // primera aparicion = deterministico). En el JSON el orden de las claves no significa nada:
    // el lector los resuelve por nombre, y los carga ANTES de armar los objetos.
    // los materiales de las mallas HUERFANAS (nadie las usa en la escena, pero se guardan)
    if (meshesResource) {
        std::vector<Material*> mh; W3dMeshesSaveMaterials(mh);
        for (size_t i = 0; i < mh.size(); i++)
            if (mh[i] && mh[i]->lib.empty() && std::find(gMats.begin(), gMats.end(), mh[i]) == gMats.end())
                gMats.push_back(mh[i]);
    }
    // TODOS LOS MATERIALES DEL PROYECTO, no solo los que usa una malla: un material sin usuarios
    // es un HUERFANO del proyecto (el outliner por recursos lo muestra y lo purga a pedido) y se
    // conserva, como las mallas y los animsets huerfanos. Van DESPUES de los usados, en el orden
    // de la lista global (el de la carga): re-guardar sin cambios da los mismos bytes. Los de la
    // UI (debajo de la marca) y el material por defecto no son del proyecto.
    for (size_t i = (size_t)MaterialsBase(); i < Materials.size(); i++) {
        Material* mt = Materials[i];
        if (!mt || mt == MaterialDefault) continue;
        if (!mt->lib.empty()) continue;   // (los de las LIBRERIAS no son del proyecto: viven en su .w3d)
        if (std::find(gMats.begin(), gMats.end(), mt) == gMats.end()) gMats.push_back(mt);
    }
    // la carpeta de cada textura sigue a su ruta (la ingesta de los materiales la cambia)
    W3dResourcesViewSaveBefore();
    WriteMaterials(s, &cx);
    // las texturas del proyecto que viven SOLO EN MEMORIA y nadie usa (huerfanas): ningun Asset()
    // las llevo, y un huerfano se conserva hasta purgarlo (antes del JSON: su carpeta tambien va)
    W3dResourcesViewSaveInMemory(&esc);
    SaveAnimSetsRegistry(s);   // el registro "animsets" (lo arma SaveAnimSetsPrepare)
    // CARPETAS del outliner por recursos ("carpetas") y la carpeta de cada textura ("texturas"):
    // despues de los materiales (la ingesta ya dejo cada textura con su nombre de entrada)
    W3dResourcesViewSaveJson(s, &esc);
    W3dLibsSaveJson(s);   // las librerias externas vinculadas (la biblioteca del outliner)
    // SESION: donde estaba trabajando el usuario EN ESTE proyecto. Va DESPUES de la escena
    // porque nombra objetos (ver el comentario grande de WriteSession).
    WriteRender(s, &cx);
    // (el Mix de escenas es de la escena del BLOQUE; el modo y el rango, del proyecto)
    if (W3dActiveRoot() != W3dRootBlock()) { W3dRootUse(W3dRootBlock()); WriteMix(s); W3dRootUse(returnToRoot.idx); }
    else WriteMix(s);
    WriteSession(s);
    // EL ORIGEN de los assets importados (entrada -> el archivo de disco del que se copio): "Actualizar textura" vuelve a
    // leerlo cuando se edito afuera. Va al FINAL: lo que este guardado acaba de meter adentro (Asset) ya esta anotado.
    // Solo las entradas que lleva el .w3d nuevo
    {
        std::vector<std::pair<std::string, std::string> > og;
        W3dAssetOriginsList(og);
        bool has = false;
        for (size_t i = 0; i < og.size(); i++) {
            if (gEsc && !gEsc->Has(og[i].first)) continue;
            s += has ? ",\n    " : "  \"origins\": {\n    ";
            JEsc(s, og[i].first); s += ": "; JEsc(s, og[i].second);
            has = true;
        }
        if (has) s += "\n  },\n";
    }
    // LAYOUT: el arbol VIVO (tipos + splits). Los .w3d guardados con el literal
    // "2d" siguen abriendo por el template estandar (ver ApplyLayoutText).
    {
        std::string lay = LayoutSerializeLive();
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
        std::string tmpZip = path + kTmpSuffix;
        bool ok = !cx.error;
        if (ok && !esc.AddBytes("project.json", s)) ok = false;
        // estilo OpenDocument: "mimetype" (que Write() manda primera) y el
        // README.txt que le explica el arbol al que abra el zip con un descompresor
        if (ok) esc.WriteHeaderOdf();
        // lo que el editor no referencia y venia en el .w3d se preserva VERBATIM
        // (alguien pudo meter un notas.txt a mano con un descompresor)
        // las texturas PURGADAS en el outliner (entradas huerfanas) no se conservan
        if (ok) W3dResourcesViewSaveDiscards(&esc);
        // las entradas de escenas/prefabs que ya no son de nadie (una escena borrada o renombrada:
        // todas las raices se acaban de escribir) no se conservan como pasajeras
        if (ok && W3dContainerReader()) {
            std::vector<std::string> old;
            W3dContainerReader()->List(old);
            std::set<std::string> newValue(entriesRoots.begin(), entriesRoots.end());
            for (size_t i = 0; i < old.size(); i++) {
                const std::string& e = old[i];
                const bool isRoot = (e.size() > 13 && e.compare(0, 7, "scenes/") == 0 && e.compare(e.size() - 5, 5, ".w3de") == 0) ||
                                    (e.size() > 13 && e.compare(0, 8, "prefabs/") == 0 && e.compare(e.size() - 5, 5, ".w3dp") == 0);
                if (isRoot && !newValue.count(e)) esc.Discard(e);
            }
        }
        if (ok) esc.PreserveTransient();
        if (ok) esc.WriteExternal();
        std::string missing;
        if (ok && !esc.Verify(missing)) {
            w3dLogfE("[W3D] el guardado se ABORTA: la referencia interna '%s' no tiene entrada en el .w3d",
                     missing.c_str());
            Notify(std::string(T("Save CANCELLED: missing inside the project:")) + " '" + missing + "'. " + T("The file was not touched:") + " " + Base(path), true);
            ok = false;
        }
        // el harness inyecta aca el disco lleno (comando 'saveatom')
        if (ok && g_w3dFailWriting) {
            FILE* fh = fopen(tmpZip.c_str(), "wb");
            if (fh) { fwrite(s.c_str(), 1, s.size() / 2, fh); fclose(fh); }
            ok = false;
        }
        if (ok) ok = esc.Write(tmpZip);
        esc.ClearTemp();
        if (!ok) {
            remove(tmpZip.c_str());
            if (missing.empty())
                Notify(std::string(T("Save: incomplete write. The file was not touched:")) + " " + Base(path), true);
            return false;
        }
        // el contenedor viejo tiene el archivo destino ABIERTO: soltarlo antes del rename
        // (el de MEMORIA -un proyecto que todavia no era un archivo- no tiene nada abierto: se queda hasta que el
        //  archivo nuevo este en su lugar, asi un fallo no pierde lo que solo vivia ahi)
        bool wasMounted = W3dContainerHasMounted();
        const bool inMemory = W3dContainerInMemory();
        if (!inMemory) W3dContainerUnmount();
        if (!RenameOver(tmpZip, path)) {
            remove(tmpZip.c_str());
            if (wasMounted && !inMemory) W3dContainerMount(w3dPath);   // volver a lo que habia
            Notify(std::string(T("Save: could not replace the file (the previous version was kept):")) + " " + Base(path), true);
            return false;
        }
        // y se monta el que acabamos de escribir: de aca en adelante las rutas en
        // memoria son sus nombres de entrada y ReadFileBytes las resuelve
        if (!W3dContainerMount(path))
            w3dLogfE("[W3D] guarde %s pero no lo pude volver a montar", path.c_str());
        // las mallas en memoria siguen al archivo: entradas, nombres y objetos re-vinculados
        if (meshesResource) W3dMeshesSaveConfirm();
        SaveAnimSetsConfirm();  // el registro en memoria = el escrito; los recursos, a sus entradas nuevas
        // (Confirmar pasa las vistas de los clips de una biblioteca que se junto con otra a la que queda; si aun
        //  asi la que se esta editando quedo huerfana, no se sigue keyeando en ella: se pierde lo que se le ponga.
        //  La de una raiz BORRADA con su Ctrl+Z pendiente no: esa vuelve a editarse con el Ctrl+Z)
        if (W3dAnimIsClip(ActiveSceneAnim) && SceneAnimations[(size_t)ActiveSceneAnim]->owner &&
            W3dAttachedRoot(SceneAnimations[(size_t)ActiveSceneAnim]->owner))
            W3dClipsActiveViewCheck();
        W3dRootsSaved(entriesRoots);   // cada escena/prefab nombra su entrada nueva
        W3dResourcesSavedView();  // lo purgado ya no esta; el listado del contenedor cambio
        TexEditSavedProject();    // las texturas que vivian en memoria ya estan adentro del .w3d
        { extern void W3dChangesSnapshot(); W3dChangesSnapshot(); }   // todo guardado: se apagan los '*' (CambiosProyecto)
        esc.MissingExternals(g_w3dExternalMissing);   // (la biblioteca: "Archivos que faltan")
        // EL ORIGINAL DE UNA MALLA IMPORTADA (el .glb/.obj de donde vino) es SOLO PROCEDENCIA: la geometria, las
        // texturas y las animaciones ya viajan adentro. Que no este (el .w3d se abrio en otro telefono) no pierde
        // nada -> no es un error del guardado ni cuenta como "externo" (estaba en la biblioteca, en "Archivos que
        // faltan", con su "olvidar"). El aviso rojo queda para lo que el proyecto NECESITA y no esta, y NOMBRA el
        // archivo: "1 archivo externo NO esta (ver EXTERNALS.txt)" no decia cual, y en Android no hay como abrirlo
        size_t nExt = 0;
        std::vector<const W3dExternal*> missingExternals;
        {
            const std::vector<W3dExternal>& ex = esc.Externals();
            for (size_t k = 0; k < ex.size(); k++) {
                if (W3dPathIsMeshOrigin(ex[k].original)) continue;
                nExt++;
                if (!ex[k].exists) missingExternals.push_back(&ex[k]);
            }
        }
        // (el texto sale en el idioma del editor: las traducciones en cirilico ocupan el doble de bytes, y el
        //  nombre del archivo no tiene cota -> el buffer se dimensiona con el formato traducido y el nombre)
        const std::string nameArch = Base(path);
        if (!missingExternals.empty()) {
            const W3dExternal& x = *missingExternals[0];
            std::string missing = Base(x.original.empty() ? x.ref.substr(x.ref.compare(0, 4, "ext:") == 0 ? 4 : 0) : x.original);
            const char* fmt = missingExternals.size() == 1
                ? T("Saved, but %s is missing (it is outside the .w3d). Library > Missing files")
                : T("Saved, but %s and %d more file(s) are missing (outside the .w3d). Library > Missing files");
            std::vector<char> b(strlen(fmt) + missing.size() + 64);
            if (missingExternals.size() == 1) snprintf(&b[0], b.size(), fmt, missing.c_str());
            else snprintf(&b[0], b.size(), fmt, missing.c_str(), (int)missingExternals.size() - 1);
            Notify(&b[0], true);
        } else if (nExt > 0) {
            const char* fmt = T("Project saved: %s (%d external file(s), marked with an arrow in the Library)");
            std::vector<char> b(strlen(fmt) + nameArch.size() + 64);
            snprintf(&b[0], b.size(), fmt, nameArch.c_str(), (int)nExt);
            Notify(&b[0], false);
        } else if (gNotCovered > 0) {
            const char* fmt = T("Saved (note: %d curve(s) without a source file are not saved)");
            std::vector<char> b(strlen(fmt) + 64);
            snprintf(&b[0], b.size(), fmt, gNotCovered);
            Notify(&b[0], true);
        } else {
            Notify(T("Project saved: ") + Base(path), false);
        }
        W3dDownloadIfWeb(path);   // navegador: el .w3d guardado baja a la PC (es el unico disco que hay)
        w3dLogf("[W3D] guardado %s (contenedor v4 estilo ODF: mimetype + README.txt + "
                "project.json + escenas/ + scripts/ + assets)", path.c_str());
        return true;
    }
}

// nombre sugerido del .w3d cuando el explorador devuelve una CARPETA: lo tipeado en
// la tarjeta Archivo ("Nombre"), sino el nombre del proyecto abierto, sino sin_titulo.
static std::string ProjectNameSuggested() {
    extern std::string ProjectFieldName(); // Properties.cpp (tarjeta Archivo)
    std::string n = ProjectFieldName();
    if (n.empty() && !w3dPath.empty()) n = Base(w3dPath);
    if (n.empty()) n = "untitled";
    return n;
}

// El destino elegido, entre el explorador y la confirmacion de sobrescritura
// (ConfirmPopup::onYes no lleva argumentos, mismo patron que Render/Export).
static std::string g_savePending;

static void SavePendingNow() {
    if (g_savePending.empty()) return;
    std::string r = g_savePending;
    if (SaveW3D(r)) {
        w3dPath = r;   // el proximo Ctrl+S va directo aca
        extern void ProjectSyncFields();
        ProjectSyncFields();
    }
}

// El explorador (modo elegir carpeta) devuelve una CARPETA ("Usar carpeta actual") o un
// .w3d existente (sobrescribir). W3dOutputPath arma la ruta final en los dos casos:
// antes se le pegaba ".w3d" a la carpeta y salia "/ruta/Documentos.w3d".
// Si el .w3d de destino YA EXISTE se pide confirmacion antes de pisarlo (antes se
// escribia encima sin decir nada; Render y Export si preguntan).
static void SavePicked(const std::string& picked) {
    g_savePending = W3dOutputPath(picked, ProjectNameSuggested(), ".w3d");
    if (w3dFileSystem::FileExists(g_savePending)) {
        if (!confirmPopup) confirmPopup = new ConfirmPopup();
        confirmPopup->Open(std::string(T("The file \"")) + Base(g_savePending) +
                              T("\" already exists. Do you want to replace it?"), SavePendingNow);
        return;
    }
    SavePendingNow();
}

void SaveProject() {
    if (!w3dPath.empty()) { SaveW3D(w3dPath); return; }
    OpenFileBrowser(T("Save project"), T("Save"), ".w3d", SavePicked, true /*guardar*/);
}

void SaveProjectAs() {
    OpenFileBrowser(T("Save As"), T("Save"), ".w3d", SavePicked, true /*guardar*/);
}
