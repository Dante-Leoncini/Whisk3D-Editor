// ============================================================================
//  W3dTestsResources.cpp — comandos de harness de RECURSOS, CARGA y FORMATOS.
//  Ver W3dTestsResources.h: W3dRunCommand (test/W3dScript.cpp) llama aca ANTES
//  de su cadena de comandos. Este archivo compila en segundos; W3dScript.cpp en
//  minutos. Todo comando nuevo de recursos/carga/formatos va ACA.
//
//  Comandos:
//    cargabench <ruta.w3d> [n] [clave valor]...
//        abre el proyecto n veces (default 1) con vsync y barra de progreso
//        APAGADOS, reloj de PARED, y reparte el tiempo por fase. Imprime por
//        apertura y al final UNA linea parseable con el promedio:
//          [cargabench] total=.. ms json=.. mallas=.. clips=.. tex=.. render=.. rssKB=.. (+ extras)
//        Los tiempos son el PROMEDIO de las n aperturas (minTotal = la mejor). rssKB es
//        la memoria residente tras la PRIMERA apertura (el proyecto recien abierto, con
//        el anterior cerrado); rssBaseKB la de antes de abrir; rssUltKB la de despues de
//        la ultima; peakKB el pico (VmHWM) mas alto visto DURANTE una apertura.
//        heapKB = heap EN USO tras la primera apertura (glibc mallinfo2: sin lo que el
//        allocator retiene); leakKB = heap que sobrevive a cada ciclo cerrar+abrir (el
//        RSS solo no alcanza para decir "fuga": glibc no devuelve todo al sistema).
//        Asserts opcionales (sobre la ULTIMA apertura), en cualquier orden:
//          mallas N | armatures N | clips N | objetos N | texturas N  (cantidades exactas;
//          texturas = las de CONTENIDO, sin las fijadas de la UI, como texinfo)
//          mallasbin N | derivadas N  (mallas que salieron de un .w3db sin derivar nada /
//          mallas que pagaron el cierre clasico al abrir: Reagrupar, Forsyth, ComputeBorders...)
//          maxms X                                                    (cota del total promedio)
//          fugakb X      (cota de leakKB; pide n >= 3; fuera de glibc no se asserta)
//        Siempre (sin pedirlo) exige que el REPARTO haya medido: la fase json > 0, la de
//        mallas > 0 si el arbol tiene mallas, la de clips > 0 si tiene armatures, y que el
//        lector haya contado las mismas mallas/armatures/clips que quedaron en el arbol.
//        'sinreloj 1' abre SIN instalar el reloj del lector (el reparto queda en cero):
//        solo sirve para probar, con 'fail', que ese guardia salta.
//    heapuso [max KB]
//        heap EN USO (glibc) y su delta desde el heapuso anterior; con 'max' falla si
//        crecio mas de KB. Para asertar que hacer/deshacer algo N veces no deja memoria.
//    cachesproyecto [sonidos N|>=N] [tex2d N|>=N]
//        cuantos WAV e imagenes 2D tiene cacheados el proyecto abierto (precarga +
//        primer uso). Tras cerrar el proyecto (RestartScene) tienen que ser 0.
//    juego2dmin <carpeta> / juego2dpx <carpeta> <nombre>
//        el juego 2D PURO minimo (HUD + script, nada 3D) y el chequeo por pixel del
//        binario compilado: "Compilar juego" lo arma solo con kSourcesBase.
//    escenadefecto
//        RestartScene + la escena de arrancar sin archivo (la del constructor).
//    viewports [hojas N]
//        viewports VIVOS en el proceso contra los nodos del arbol actual (el completo:
//        con uno maximizado, el guardado). Tienen que ser iguales: abrir un proyecto
//        libera el layout anterior. Ademas viewPortActive tiene que ser una hoja del
//        arbol, y Viewport3DActive/PropsActive NULL o una hoja (nunca uno muerto).
//    layoutmaximizar <kind>
//        maximiza (LayoutMaximize) la primera hoja de ese ViewportKind (1=3D 3=props).
//    propsfoco [nulo]
//        enfoca el campo Nombre del primer panel de propiedades (g_activeTextField);
//        con 'nulo' asserta que no hay campo enfocado (el del panel liberado se solto).
//    propsrename [nulo]
//        arranca el rename del material del objeto activo desde el primer panel de
//        propiedades (el boton "Rename Material" se vuelve input); con 'nulo' asserta
//        que no hay ningun rename en curso (el del panel liberado se cancelo).
//    redopanel [0|1]
//        sin argumento: un cubo nuevo + su panel "Add" sobre el primer viewport 3D;
//        con 0/1 asserta si el panel sigue abierto (muere con el 3D que lo creo).
//    propsarm2d huesos|pose [paneles N]
//        pone el primer editor UV del layout en Edit Bones o Pose y, despues de que un
//        panel de propiedades TEMPORAL nazca y muera (queda como el ultimo construido),
//        abre la pestania Armature 2D en CADA panel del layout y asserta que cada uno
//        refresca SUS filas: Pos X/Y siempre (con hueso activo), Rotation/Scale X/Y solo en
//        pose. Necesita la malla activa en Edit Mode con un armature 2D (bone2dnew).
//    iconospkg
//        los iconos del enum (IconName) contra el skin y los .pkg de Symbian: cada uno
//        tiene su res/Skins/Whisk3D/atlas/icons/<nombre>.png de 10x10 y una linea en los
//        DOS .pkg, y cada icono que instala un .pkg existe. Se busca desde tools/pruebas.
//    formatomallas [texto|binario] | proyecto [texto|binario] | es texto|binario
//        el formato con que se GUARDAN las mallas: forzado para todo el proceso (los tests
//        del .w3dm fijan texto), la opcion del proyecto ("formatoMallas"), o el assert.
//    logcargamallas <w3db> <w3dm> <rechazadas>
//        la ultima linea "[CARGA] mallas .w3db=" del log (la que se lee en el telefono)
//        dice esas cuentas: .w3db listas, .w3dm derivadas al abrir, .w3db rechazadas.
//    w3dbinfo <ruta.w3d> [detalle] [w3db N] [w3dm N] [tiene XXXX] [notiene XXXX]
//        las mallas binarias/de texto de un contenedor, los bloques de sus .w3db, y que
//        CADA .w3db se lea. tiene/notiene miran todas las .w3db.
//    mallafoto <nombre> [frame N]... [sinedicion]  /  mallafotoigual <a> <b> [sinedicion]
//        la FOTO de todas las mallas del arbol (un hash por campo: arrays de render, index
//        buffer, parts, puntos, grupos, formas, AABB, malla generada; y los de edicion:
//        caras, capas, marcas, aristas, posRep, preservacion), mas el skinning y las
//        vertex anims en cada 'frame N' y las shape keys a 0.5. mallafotoigual exige que
//        dos fotos sean IDENTICAS (la equivalencia .w3dm <-> .w3db).
//    w3dbrender <ruta.w3d>
//        con ESE proyecto abierto: cada .w3db leido por el editor, por el juego (solo render
//        + MaterializarEdicion) y por W3dMeshBinReadRender da lo mismo; y el modo noEditable
//        contrario al guardado da lo mismo que la carga clasica.
//    w3dbcorrupto <ruta.w3d>
//        robustez del lector: cortes, bytes al azar, version nueva, bloque desconocido
//        (se preserva) y requerido (solo lectura + freno al guardar).
//    w3dbromper <origen.w3d> <destino.w3d> corte|version
//        copia el contenedor con su primera .w3db rota: al abrirlo esa malla no carga
//        y guardar encima se frena (notLoaded), como con un .w3dm ilegible.
//    mallarica [grande]
//        escena NUEVA con una malla de cada caso del formato (capas, parts, marcas, grupos,
//        forma, UV group, sueltos, noEditable, rig con clip, vertex anim, Mirror, flipbook);
//        'grande' agrega una esfera de mas de 65535 render-verts.
//    formatomallasui <malla> [clic] [tilde 0|1]...
//        el tilde "Mesh as text" de la tarjeta del RECURSO de esa malla: 'clic' lo toca como un
//        click (EditPropertie) y 'tilde' asserta lo que muestra (el formato con que se guarda ESA malla).
//    zipcrc
//        el CRC32 del contenedor (de a 8 bytes) contra los vectores de control y contra la
//        definicion bit a bit en todos los largos/alineaciones; informa MB/s.
//    w3dbregistro [contenedor.w3d]
//        los fourcc que entiende el codigo = las filas "w3db:" vivas de formato/bloques.tsv, y
//        cada uno documentado en formato/w3db.md; con contenedor, todo bloque de sus .w3db esta
//        registrado. Sin formato/ (gitignoreado) se saltea con "SIN CORPUS:".
//    juego3dbool <carpeta> / juego3dboolpx <carpeta> <nombre>
//        el juego 3D minimo con un Boolean DIFFERENCE cuyo target (oculto) solo tiene la
//        edicion PENDIENTE en el runtime (se materializa por BuildPolyMesh), y el chequeo
//        por pixel del binario compilado: por el agujero se ve el fondo.
//    indices16 0|1
//        el GUARDADO de las mallas como en una plataforma de indices de 16 bits (el N95):
//        una malla de mas de 65535 render-verts sale en texto (.w3dm), no en .w3db.
//    w3dbjuego <ruta.w3d>
//        escena nueva (cubos sin modificadores / solo Oclusion / Mirror apagado / Mirror)
//        guardada en binario y abierta COMO EL JUEGO: solo el stack que genera malla carga
//        la edicion; lo pendiente se materializa al pedirlo y al guardar (<ruta>_2.w3d).
//    w3dbmarcas <ruta.w3d>
//        las claves SHRP/SEAM de cada .w3db: little-endian con la mitad menor primero, y
//        con las mitades dadas vuelta abren con las mismas marcas y se reescriben igual.
//    w3dbajenos <prefijo>
//        un .w3dm con bloques de una version mas nueva y un "requiere" no default:
//        texto -> binario (AJEN) -> texto da el mismo .w3dm; sin una cara, el bloque de
//        cara se descarta avisando y el global sigue.
//    escenasintetica <n> [ruta.w3d] [segmentos anillos]
//        escena NUEVA con n mallas IGUALES ("Arbol", una esfera UV de
//        segmentos x anillos, default 32x16) en una grilla + camara + luz.
//        Con ruta la guarda (SaveW3D). Es la escena de "mil arboles iguales"
//        contra la que se mide la comparticion de mallas.
// ============================================================================
#include "test/W3dTestsResources.h"
#include "test/W3dTestsAnims.h"     // los comandos de los ANIMSETS (clips de esqueleto .w3da)
#include "test/W3dTestsHitbox.h"      // los comandos del HITBOX (se despachan antes que los de aca, ver abajo)
#include "test/W3dTestsIntegration.h" // la prueba CRUZADA: mallas + animsets + hitbox en la misma escena
#include "test/W3dTestsOutliner.h"   // el OUTLINER POR RECURSOS: vistas, carpetas, en uso / huerfano
#include "test/W3dCaptures.h"        // las CAPTURAS de los tutoriales: mouse, dedo, rueda y teclas reales
#include "test/W3dTestsScenes.h"     // escenas 3D y prefabs (las raices del proyecto)
#include "test/W3dTestsPrefabs.h"    // las INSTANCIAS de prefab (generar, scope, overrides, instantiate/destroy)
#include "test/W3dTestsLibs.h"       // las LIBRERIAS externas y los PROXIES W3D
#include "test/W3dTestsStreaming.h"  // el STREAMING de las instancias y proxies diferidos
#include "importers/import_w3d.h"    // g_w3dLoadPhases / g_w3dLoadClock: el reparto por fase
#include "importers/import_obj.h"    // LoadAllPendingTextures
#include "config/W3dProfile.h"       // W3dNowMs: reloj de PARED (SDL, alta resolucion)
#include "objects/Objects.h"
#include "objects/Mesh.h"
#include "objects/Armature.h"
#include "objects/Materials.h"
#include "objects/Textures.h"
#include "objects/Camera.h"
#include "objects/Light.h"
#include "objects/Collection.h"
#include "io/W3dResources.h"
#include "WhiskUI/io/Texture2D.h"             // Texture2DList: el cache de imagenes 2D del proyecto
#include "w3dFilesystem.h"
#include "graphics/w3dGraphics.h"           // Finish: drenar la GPU (la subida de VBOs es perezosa)
#include "w3dTexture.h"                // juego2dpx: DecodeImage de la captura del binario
#include "ViewPorts/ViewPorts.h"       // rootViewport
#include "ViewPorts/LayoutTree.h"      // viewports: LiveViewports / LayoutCompleteRoot
#include "ViewPorts/LayoutInput.h"     // layoutmaximizar: LayoutMaximize / LayoutIsMaximized
#include "ViewPorts/ViewPort3D.h"      // viewports: Viewport3DActive
#include "ViewPorts/Properties.h"      // viewports/propsfoco: PropsActive, el campo Nombre del proyecto
#include "ViewPorts/UVEditor.h"        // propsarm2d: el modo del editor UV (Edit Bones / Pose)
#include "WhiskUI/draw/icons.h"        // iconospkg: IconName / ICON_TOTAL
#include "ViewPorts/PopUp/RedoMeshPanel.h" // redopanel: OpenRedoMeshPanel / RedoMeshPanelActive
#include "ViewPorts/PopUp/ProgressPopup.h" // LayoutSwapBuffers: la barra de progreso hace swaps
#include <SDL2/SDL.h>                 // SDL_GL_SetSwapInterval: vsync apagado mientras se mide
#include "WhiskUI/objects/UI.h"               // juego2dmin: la UI raiz (lo que compila "Compilar juego")
#include "WhiskUI/objects/Rect2D.h"           // juego2dmin: la franja que prende el script
#include "script/W3dScript.h"         // juego2dmin: W3dScriptData / W3dScriptEntry
#include "io/W3dMesh.h"               // .w3dm: el escritor de texto (w3dbcorrupto: lo que el texto no puede llevar)
#include "io/W3dMeshBin.h"            // .w3db: la malla binaria (formato, equivalencia, robustez)
#include "io/W3dZip.h"                // W3dZipRead: las entradas de mallas de un .w3d
#include "io/SaveW3D.h"               // indices16: g_w3dIndices16Simulated (el guardado como en el N95)
#include "io/ProjectMeshes.h"         // escenasintetica: los N arboles son UNA malla (un recurso)
#include "objects/MeshResource.h"
#include "w3dlog.h"                    // logcargamallas: el ring del log (lo que va a whisk3d.log)
#include "edit/MeshEdit.h"             // mallarica: GenerateRender / Reagrupar / ComputeBorders
#include "edit/Modifier.h"             // mallarica: Armature + Mirror
#include "edit/PolyMesh.h"             // w3dbrender: BuildPolyMesh (la puerta que materializa la edicion)
#include "edit/WeightPaint.h"          // mallarica: WeightPaintEnsureMap
#include "animation/Animation.h"       // CurrentFrame / SetKeyCurve
#include "animation/SkeletalAnimation.h" // SkinMesh / CreateAnimation / PrepareAuthoredSkin
#include "animation/VertexAnimation.h" // EvalVertexAnim / VertexAnimSetKey
#include "test/W3dScript.h"            // juego3dbool: W3dRunCommand("juego3dmin ...") (el juego 3D minimo)
#include <set>
#include <sys/stat.h>                  // juego2dmin: crear la carpeta del proyecto
#ifdef _WIN32
#include <direct.h>                    // _mkdir
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <vector>
#if defined(__linux__)
#include <unistd.h>                    // sysconf(_SC_PAGESIZE): paginas de /proc/self/statm
#endif
#if defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 33))
#include <malloc.h>                    // mallinfo2: el heap EN USO (no lo que el allocator retiene)
#define W3D_PR_HEAP 1
#endif

extern void RestartScene();
extern void OpenProjectNow(const std::string& path);
extern bool SaveW3D(const std::string& path);

// ============================================================================
//  Helpers
// ============================================================================

// ruta como argumento: un token, o "entre comillas" si tiene espacios
static std::string PrReadPath(std::istringstream& ss) {
    std::string t;
    if (!(ss >> t)) return std::string();
    if (t.empty() || t[0] != '"') return t;
    std::string r = t.substr(1);
    while (r.empty() || r[r.size() - 1] != '"') {
        std::string more;
        if (!(ss >> more)) break;
        r += " " + more;
    }
    if (!r.empty() && r[r.size() - 1] == '"') r.erase(r.size() - 1);
    return r;
}

// memoria RESIDENTE del proceso en KB (/proc/self/statm). -1 = no se puede medir aca.
static long PrRssKb() {
#if defined(__linux__)
    FILE* f = fopen("/proc/self/statm", "r");
    if (!f) return -1;
    unsigned long total = 0, resid = 0;
    const int n = fscanf(f, "%lu %lu", &total, &resid);
    fclose(f);
    if (n != 2) return -1;
    const long page = sysconf(_SC_PAGESIZE);
    return (long)(resid * (unsigned long)(page > 0 ? page : 4096) / 1024u);
#else
    return -1;
#endif
}

// PICO de memoria residente (VmHWM) en KB. -1 = no se puede medir aca.
static long PrPeakKb() {
#if defined(__linux__)
    FILE* f = fopen("/proc/self/status", "r");
    if (!f) return -1;
    char line[256];
    long kb = -1;
    while (fgets(line, sizeof(line), f))
        if (strncmp(line, "VmHWM:", 6) == 0) { kb = atol(line + 6); break; }
    fclose(f);
    return kb;
#else
    return -1;
#endif
}

// HEAP EN USO en KB (lo pedido con malloc/new y no devuelto). A diferencia del RSS no
// cuenta lo que el allocator se queda despues de un free: si esto crece al cerrar y
// reabrir el mismo proyecto, es una FUGA de verdad. -1 = no se puede medir aca (fuera de glibc).
static long PrHeapKb() {
#ifdef W3D_PR_HEAP
    const struct mallinfo2 mi = mallinfo2();
    return (long)((mi.uordblks + mi.hblkhd) / 1024u);
#else
    return -1;
#endif
}

// reinicia el PICO (VmHWM) al residente actual: asi el pico que se lee despues es
// el de ESTA apertura y no el de toda la vida del proceso. Linux >= 4.0; si el
// kernel no lo deja, el pico queda acumulado (se sigue informando igual).
static void PrPeakReset() {
#if defined(__linux__)
    FILE* f = fopen("/proc/self/clear_refs", "w");
    if (!f) return;
    fputs("5", f);
    fclose(f);
#endif
}

// cantidades del arbol: objetos (sin la raiz), mallas, armatures y clips de esqueleto
struct PrCounts { int objects, meshes, armatures, clips; };
static void PrCount(Object* o, PrCounts& c, bool isRoot) {
    if (!o) return;
    if (!isRoot) {
        c.objects++;
        if (o->getType() == ObjectType::mesh) c.meshes++;
        else if (o->getType() == ObjectType::armature) {
            c.armatures++;
            c.clips += (int)((Armature*)o)->animations.size();
        }
    }
    for (size_t i = 0; i < o->Childrens.size(); i++) PrCount(o->Childrens[i], c, false);
}

// una apertura medida (todo en ms de pared)
struct PrMeasure {
    double restart;  // cerrar el proyecto anterior (fuera del total)
    double open;     // OpenProjectNow entero
    double pending;  // texturas que quedaron en la cola diferida
    double render;   // PRIMER frame del layout + glFinish (sube VBOs/texturas perezosas)
    double frame2;   // el SEGUNDO frame (ya sin subidas): render - frame2 ~ costo de subida
    W3dLoadPhases f; // el reparto interno del lector
    long rssKB, peakKB;
    long heapKB;       // heap en uso con el proyecto abierto (tras el segundo frame)
    long closedHeapKB;// heap en uso con el proyecto ANTERIOR ya cerrado (antes de abrir)
    PrMeasure() : restart(0), open(0), pending(0), render(0), frame2(0), rssKB(-1), peakKB(-1),
                 heapKB(-1), closedHeapKB(-1) {}
};

// LA LINEA PARSEABLE de cargabench: "[cargabench] total=<n> ms json=<n> mallas=<n> clips=<n>
// tex=<n> render=<n> rssKB=<n>" al PRINCIPIO y en ESE orden (lo que sigue es libre). La leen
// herramientas de afuera (la linea base de medidas, las comparaciones entre fases): cambiarla
// sin querer las rompe en silencio, asi que cargabench la valida antes de dar OK.
static bool PrLineParseable(const std::string& l, std::string& reason) {
    static const char* keys[7] = { "total=", "json=", "mallas=", "clips=", "tex=", "render=", "rssKB=" };
    const std::string pre = "[cargabench] ";
    if (l.compare(0, pre.size(), pre) != 0) { reason = "no empieza con '[cargabench] '"; return false; }
    size_t pos = pre.size();
    for (int i = 0; i < 7; i++) {
        const size_t lc = strlen(keys[i]);
        if (l.compare(pos, lc, keys[i]) != 0) { reason = std::string("falta '") + keys[i] + "' en su lugar"; return false; }
        pos += lc;
        const char* start = l.c_str() + pos;
        char* end = 0;
        strtod(start, &end);
        if (!end || end == start) { reason = std::string("'") + keys[i] + "' sin numero"; return false; }
        pos += (size_t)(end - start);
        if (i == 0) {
            if (l.compare(pos, 3, " ms") != 0) { reason = "falta ' ms' despues de total"; return false; }
            pos += 3;
        }
        // separador: un espacio (o el final, despues de la ultima clave)
        if (pos < l.size()) {
            if (l[pos] != ' ') { reason = std::string("basura despues de '") + keys[i] + "'"; return false; }
            pos++;
        } else if (i < 6) { reason = "la linea termina antes de tiempo"; return false; }
    }
    return true;
}

// ============================================================================
//  cargabench
// ============================================================================
static bool CmdLoadBench(std::istringstream& ss, std::string& err) {
    const std::string path = PrReadPath(ss);
    if (path.empty()) { err = "loadbench: uso: loadbench <ruta.w3d> [n] [clave valor]..."; return false; }
    int n = 1;
    // asserts opcionales (clave valor); el primer token numerico suelto es n
    std::vector<std::string> keys; std::vector<double> values;
    {
        std::string t;
        bool first = true;
        while (ss >> t) {
            char* end = 0;
            const double v = strtod(t.c_str(), &end);
            if (first && end && *end == '\0') { n = (int)v; first = false; continue; }
            first = false;
            std::string val;
            if (!(ss >> val)) { err = "loadbench: falta el valor de '" + t + "'"; return false; }
            end = 0;
            const double vv = strtod(val.c_str(), &end);
            if (!end || *end != '\0') { err = "loadbench: '" + val + "' no es un numero"; return false; }
            keys.push_back(t); values.push_back(vv);
        }
    }
    if (n < 1) n = 1;
    if (!w3dFileSystem::FileExists(path)) { err = "loadbench: no existe " + path; return false; }
    // 'sinreloj 1' NO es un assert: abre sin el reloj del lector (el reparto no mide nada).
    // Existe para probar con 'fail' que el guardia de abajo lo detecta.
    bool noClock = false;
    for (size_t i = 0; i < keys.size(); ) {
        if (keys[i] == "noclock") {
            noClock = (values[i] != 0.0);
            keys.erase(keys.begin() + i); values.erase(values.begin() + i);
        } else i++;
    }

    // ---- sin vsync y sin barra de progreso: la espera del vblank se comia ~80% ----
    //      de la apertura de un archivo chico (cada swap de la barra = 1 vblank)
    void (*swapPrev)() = LayoutSwapBuffers;
    LayoutSwapBuffers = NULL;
    SDL_GL_SetSwapInterval(0);
    g_w3dLoadClock = noClock ? NULL : W3dNowMs;

    std::vector<PrMeasure> med;
    PrCounts cnt; cnt.objects = cnt.meshes = cnt.armatures = cnt.clips = 0;
    long rssBase = -1;
    for (int it = 0; it < n; it++) {
        PrMeasure m;
        double t0 = W3dNowMs();
        RestartScene();                           // cerrar el anterior: se mide APARTE
        m.restart = W3dNowMs() - t0;
        m.closedHeapKB = PrHeapKb();                 // lo que sobrevive al cierre
        if (it == 0) rssBase = PrRssKb();            // el proceso sin proyecto abierto
        PrPeakReset();
        g_w3dLoadPhases.Reset();
        W3dResourcesStatsReset(-1);
        t0 = W3dNowMs();
        OpenProjectNow(path);
        m.open = W3dNowMs() - t0;
        t0 = W3dNowMs();
        LoadAllPendingTextures();
        m.pending = W3dNowMs() - t0;
        m.render = m.frame2 = 0.0;
        if (rootViewport) {
            t0 = W3dNowMs();
            rootViewport->Render();
            w3dEngine::Finish();
            m.render = W3dNowMs() - t0;
            t0 = W3dNowMs();
            rootViewport->Render();
            w3dEngine::Finish();
            m.frame2 = W3dNowMs() - t0;
        }
        m.f = g_w3dLoadPhases;
        m.rssKB = PrRssKb();
        m.peakKB = PrPeakKb();
        m.heapKB = PrHeapKb();
        med.push_back(m);
        const double total = m.open + m.pending + m.render;
        printf("      [cargabench #%d] total=%.1f ms abrir=%.1f montaje=%.1f json=%.1f objetos=%.1f "
               "mallas=%.1f clips=%.1f animEscena=%.1f precarga=%.1f pendientes=%.1f render=%.1f "
               "frame2=%.1f reinicio=%.1f rssKB=%ld picoKB=%ld heapKB=%ld heapCerradoKB=%ld\n",
               it + 1, total, m.open, m.f.mountMs, m.f.jsonMs, m.f.objectsMs, m.f.meshesMs,
               m.f.clipsMs, m.f.animSceneMs, m.f.preloadMs, m.pending, m.render, m.frame2,
               m.restart, m.rssKB, m.peakKB, m.heapKB, m.closedHeapKB);
    }

    g_w3dLoadClock = NULL;
    SDL_GL_SetSwapInterval(1);                       // como arranca la app
    LayoutSwapBuffers = swapPrev;

    // ---- promedio de las n aperturas (la primera paga el cache frio del disco) ----
    PrMeasure p;   // acumulador (arranca en cero)
    double minTotal = 1e30;
    long peakMax = -1;
    for (size_t i = 0; i < med.size(); i++) {
        const PrMeasure& m = med[i];
        p.restart += m.restart; p.open += m.open; p.pending += m.pending;
        p.render += m.render; p.frame2 += m.frame2;
        p.f.mountMs += m.f.mountMs; p.f.jsonMs += m.f.jsonMs; p.f.objectsMs += m.f.objectsMs;
        p.f.meshesMs += m.f.meshesMs; p.f.clipsMs += m.f.clipsMs;
        p.f.meshReadMs += m.f.meshReadMs; p.f.meshParseMs += m.f.meshParseMs;
        p.f.derivedMeshMs += m.f.derivedMeshMs;
        p.f.animSceneMs += m.f.animSceneMs; p.f.preloadMs += m.f.preloadMs;
        const double total = m.open + m.pending + m.render;
        if (total < minTotal) minTotal = total;
        if (m.peakKB > peakMax) peakMax = m.peakKB;
    }
    const double k = 1.0 / (double)med.size();
    const PrMeasure& last = med.back();
    const double total = (p.open + p.pending + p.render) * k;
    // lo que el reparto no ve: reparacion de nombres, targets/constraints, layout, sesion...
    const double rest = (p.open - p.f.mountMs - p.f.jsonMs - p.f.objectsMs - p.f.animSceneMs
                          - p.f.preloadMs) * k;
    const double tex = (p.f.preloadMs + p.pending) * k;
    // FUGA POR REAPERTURA: cuanto heap sobrevive a cada ciclo cerrar+abrir del MISMO proyecto
    // (heap en uso tras cerrar, apertura a apertura). La primera vuelta se descarta si hay con
    // que (n >= 3): paga las inicializaciones perezosas de una sola vez (shaders, fuentes...).
    // -1 = no se puede medir (n < 2 o fuera de glibc).
    long leakKB = -1;
    if (med.size() >= 2 && med[0].closedHeapKB >= 0) {
        const size_t d = (med.size() >= 3) ? 1 : 0;
        leakKB = (last.closedHeapKB - med[d].closedHeapKB) / (long)(med.size() - 1 - d);
    }
    // la linea PARSEABLE: se arma, se valida su formato y recien ahi se imprime
    char line[1024];
    snprintf(line, sizeof(line),
             "[cargabench] total=%.1f ms json=%.1f mallas=%.1f clips=%.1f tex=%.1f render=%.1f "
             "rssKB=%ld montaje=%.1f objetos=%.1f animEscena=%.1f precarga=%.1f pendientes=%.1f "
             "resto=%.1f frame2=%.1f reinicio=%.1f picoKB=%ld rssBaseKB=%ld rssUltKB=%ld "
             "heapKB=%ld fugaKB=%ld minTotal=%.1f n=%d",
             total, p.f.jsonMs * k, p.f.meshesMs * k, p.f.clipsMs * k, tex, p.render * k,
             med[0].rssKB, p.f.mountMs * k, p.f.objectsMs * k, p.f.animSceneMs * k,
             p.f.preloadMs * k, p.pending * k, rest, p.frame2 * k, p.restart * k,
             peakMax, rssBase, last.rssKB, med[0].heapKB, leakKB, minTotal, (int)med.size());
    printf("      %s\n", line);
    { std::string reason;
      if (!PrLineParseable(line, reason)) { err = "loadbench: la linea parseable se rompio: " + reason; return false; } }
    // de que esta hecha la fase de mallas: leer la entrada, parsear el texto, DERIVAR (index
    // buffer, bordes, capas, mapa de puntos) y el resto (transform, anims, modificadores...)
    printf("      [cargabench] desglose mallas: leer=%.1f parse=%.1f derivados=%.1f otros=%.1f ms | "
           "binarias=%d derivadas=%d (de %d)\n",
           p.f.meshReadMs * k, p.f.meshParseMs * k, p.f.derivedMeshMs * k,
           (p.f.meshesMs - p.f.meshReadMs - p.f.meshParseMs - p.f.derivedMeshMs) * k,
           last.f.meshesBin, last.f.derivedMeshes, last.f.meshes);

    // ---- cantidad de recursos (la ultima apertura) ----
    PrCount(SceneCollection, cnt, true);
    // texturas DE CONTENIDO: las vivas menos las fijadas de la UI (mismo criterio que texinfo)
    const int textures = LiveTextures() - TexturesBase();
    printf("      [cargabench] recursos objetos=%d mallas=%d armatures=%d clips=%d materiales=%d "
           "texturas=%d texturasUI=%d almacen.tex=%d almacen.malla=%d almacen.anim=%d almacen.vis=%d "
           "lector.mallas=%d lector.armatures=%d lector.clips=%d\n",
           cnt.objects, cnt.meshes, cnt.armatures, cnt.clips, (int)Materials.size(), textures,
           TexturesBase(),
           W3dLiveResources(W3DREC_TEXTURE), W3dLiveResources(W3DREC_MESH),
           W3dLiveResources(W3DREC_ANIM), W3dLiveResources(W3DREC_LIST_VIS),
           last.f.meshes, last.f.armatures, last.f.clips);

    // ---- asserts ----
    for (size_t i = 0; i < keys.size(); i++) {
        const std::string& c = keys[i];
        const double v = values[i];
        double has = 0.0;
        if      (c == "meshes")    has = cnt.meshes;
        else if (c == "armatures") has = cnt.armatures;
        else if (c == "clips")     has = cnt.clips;
        else if (c == "objects")   has = cnt.objects;
        else if (c == "textures")  has = textures;
        // de las mallas de la ULTIMA apertura: cuantas salieron de un .w3db (sin derivar nada)
        // y cuantas pagaron el cierre clasico (Reagrupar/Forsyth/ComputeBorders/capas)
        else if (c == "meshesbin") has = last.f.meshesBin;
        else if (c == "derived") has = last.f.derivedMeshes;
        else if (c == "maxms") {
            if (total > v) {
                char b[160]; sprintf(b, "loadbench: total %.1f ms supera la cota %.1f ms", total, v);
                err = b; return false;
            }
            continue;
        }
        else if (c == "leakkb") {
            // sin medida (fuera de glibc) no se asserta: no es una falla del proyecto
            if (leakKB < 0 && PrHeapKb() < 0) continue;
            if (med.size() < 3) { err = "loadbench: fugakb necesita n >= 3 (la primera vuelta se descarta)"; return false; }
            if ((double)leakKB > v) {
                char b[200];
                sprintf(b, "loadbench: cada reapertura deja %ld KB de heap vivo (cota %.0f KB): algo del "
                           "proyecto no se libera al cerrarlo", leakKB, v);
                err = b; return false;
            }
            continue;
        }
        else { err = "loadbench: assert desconocido '" + c + "'"; return false; }
        if ((long)has != (long)v) {
            char b[160]; sprintf(b, "loadbench: %s=%ld y se esperaba %ld", c.c_str(), (long)has, (long)v);
            err = b; return false;
        }
    }
    // EL REPARTO TIENE QUE HABER MEDIDO. Sin este guardia, si el reloj del lector deja de
    // instalarse (o el lector deja de acumular) todas las fases salen en 0 y los asserts de
    // cantidad pasan igual, porque cuentan el ARBOL. Se exige siempre, sin pedirlo:
    //  - la fase json > 0: los TRES formatos parsean algo (el JSON v3, el contenedor v4 y el
    //    texto viejo, donde "json" = parsear el texto);
    //  - la de mallas > 0 si el arbol tiene mallas, la de clips > 0 si tiene armatures;
    //  - el lector vio las mismas mallas/armatures/clips que quedaron en el arbol (si no, el
    //    reparto esta midiendo otra cosa). BuildScene + ImportWOBJ reparten en los mismos
    //    acumuladores que el JSON (mallas = leer/parsear el .obj + derivados).
    {
        char b[260];
        if (!(last.f.jsonMs > 0.0)) {
            sprintf(b, "loadbench: el reparto no midio la fase json (%.3f ms): el reloj del lector no "
                       "esta instalado o el lector dejo de acumular", last.f.jsonMs);
            err = b; return false;
        }
        if (cnt.meshes > 0 && !(last.f.meshesMs > 0.0)) {
            sprintf(b, "loadbench: el arbol tiene %d mallas y la fase mallas midio %.3f ms", cnt.meshes, last.f.meshesMs);
            err = b; return false;
        }
        if (cnt.armatures > 0 && !(last.f.clipsMs > 0.0)) {
            sprintf(b, "loadbench: el arbol tiene %d armatures y la fase clips midio %.3f ms", cnt.armatures, last.f.clipsMs);
            err = b; return false;
        }
        if (last.f.meshes != cnt.meshes || last.f.armatures != cnt.armatures || last.f.clips != cnt.clips) {
            sprintf(b, "loadbench: el reparto conto %d mallas/%d armatures/%d clips y el arbol tiene %d/%d/%d",
                    last.f.meshes, last.f.armatures, last.f.clips, cnt.meshes, cnt.armatures, cnt.clips);
            err = b; return false;
        }
    }
    return true;
}

// ============================================================================
//  escenasintetica
// ============================================================================
static bool CmdSceneSynthetic(std::istringstream& ss, std::string& err) {
    int n = 0; ss >> n;
    if (n < 1) { err = "scenesynthetic: uso: scenesynthetic <n> [ruta.w3d] [segmentos anillos]"; return false; }
    const std::string path = PrReadPath(ss);
    int seg = 32, ani = 16;
    { int a = 0, b = 0; if (ss >> a >> b) { if (a >= 3) seg = a; if (b >= 3) ani = b; } }

    RestartScene();
    Collection* col = new Collection(SceneCollection);
    col->SetNameObj("Bosque");
    CollectionActive = col;
    const int side = (int)ceil(sqrt((double)n));
    const float sep = 3.0f;                         // la esfera tiene radio 1: 1 m de aire entre copas
    const float ext = (side - 1) * sep;
    // camara de la escena mirando la grilla en diagonal desde arriba (como la default del editor)
    new Camera(col, Vector3(-ext * 0.5f - 6.0f, ext * 0.4f + 5.0f, ext * 0.5f + 6.0f),
               Vector3(-35.0f, -45.0f, 0.0f));
    Light* L = Light::Create(col, 1.0f, ext * 0.5f + 4.0f, 2.25f);
    L->SetDiffuse(1, 1, 1);
    // LOS N ARBOLES SON LA MISMA MALLA (un recurso, como N duplicados vinculados): el primero la
    // crea y los demas se vinculan a ella. Antes eran N mallas sueltas iguales que el guardado
    // juntaba por contenido; eso ahora es solo para las sueltas de un ARCHIVO viejo (dos mallas
    // nuevas iguales no se vinculan solas: Mesh::dedupByContent).
    MeshResource* rec = NULL;
    for (int i = 0; i < n; i++) {
        Mesh* m = (Mesh*)NewMesh(MeshType::UVsphere, col, false);
        if (!m) { err = "scenesynthetic: NewMesh devolvio NULL"; return false; }
        if (!rec) {
            m->meshVerts = seg;
            m->meshVerts2 = ani;
            m->Regenerate();
        }
        m->SetNameObj("Arbol");
        m->pos = Vector3((i % side) * sep - ext * 0.5f, 1.0f, (i / side) * sep - ext * 0.5f);
        if (!rec) {
            rec = W3dMeshCreateResource(m, "Arbol");
            if (!rec) { err = "scenesynthetic: no se pudo crear la malla 'Arbol'"; return false; }
        } else if (!W3dMeshLink(m, rec)) { err = "scenesynthetic: no se pudo vincular a la malla 'Arbol'"; return false; }
    }
    DeselectAll();
    ActiveObj = NULL;
    Mesh* m0 = NULL;
    for (size_t i = 0; i < col->Childrens.size() && !m0; i++)
        if (col->Childrens[i]->getType() == ObjectType::mesh) m0 = (Mesh*)col->Childrens[i];
    printf("      [escenasintetica] %d mallas iguales (esfera %dx%d: %d render-verts, %d tris cada una)\n",
           n, seg, ani, m0 ? m0->vertexSize : 0, m0 ? m0->facesSize / 3 : 0);
    if (!path.empty()) {
        const bool ok = SaveW3D(path);
        printf("      [escenasintetica] guardada en %s -> %s\n", path.c_str(), ok ? "OK" : "FALLO");
        if (!ok) { err = "scenesynthetic: no pude guardar " + path; return false; }
    }
    return true;
}

// ============================================================================
//  heapuso
// ============================================================================
// heap EN USO y cuanto cambio desde el heapuso anterior del mismo .w3s: la forma de
// asertar "hacer y deshacer N veces no deja memoria viva" (crear/borrar objetos,
// instantiate/destroy, streamLoad/streamUnload). 'max X' falla si el delta supera X KB.
static long gHeapUsePrev = -1;
static bool CmdHeapUse(std::istringstream& ss, std::string& err) {
    const long kb = PrHeapKb();
    const long delta = (gHeapUsePrev >= 0 && kb >= 0) ? kb - gHeapUsePrev : 0;
    printf("      [heapuso] heap en uso=%ld KB delta=%+ld KB%s\n", kb, delta,
           kb < 0 ? " (no se puede medir en esta plataforma)" : (gHeapUsePrev < 0 ? " (primera medida)" : ""));
    gHeapUsePrev = kb;
    std::string key;
    while (ss >> key) {
        double v = 0.0;
        if (!(ss >> v)) { err = "heapusage: falta el valor de '" + key + "'"; return false; }
        if (key != "max") { err = "heapusage: assert desconocido '" + key + "' (uso: heapusage [max KB])"; return false; }
        if (kb >= 0 && (double)delta > v) {
            char b[160]; sprintf(b, "heapusage: el heap crecio %ld KB desde la medida anterior (cota %.0f KB)", delta, v);
            err = b; return false;
        }
    }
    return true;
}

// ============================================================================
//  cachesproyecto
// ============================================================================
// lo que el proyecto ABIERTO dejo en los caches del lado del juego (se llenan en la
// precarga de OpenW3D y en el primer uso). Al cerrar el proyecto (RestartScene)
// tienen que quedar en 0: si no, el audio/las imagenes del cerrado siguen vivos y otro
// proyecto con una entrada del mismo nombre usaria lo del anterior.
static bool CmdCachesProject(std::istringstream& ss, std::string& err) {
    extern int W3dSoundsCached();
    std::vector<std::string> tex2d;
    Texture2DList(tex2d);
    const int nSound = W3dSoundsCached(), nTex = (int)tex2d.size();
    printf("      [cachesproyecto] sonidos=%d tex2d=%d\n", nSound, nTex);
    std::string key;
    while (ss >> key) {
        std::string op; int v = 0;
        if (!(ss >> op)) { err = "projectcaches: falta el valor de '" + key + "'"; return false; }
        // "N" exacto o ">=N" (cuantos carga la precarga depende del mixer: sin audio cachea NULL igual)
        const bool toMinus = (op.size() > 2 && op[0] == '>' && op[1] == '=');
        v = atoi(toMinus ? op.c_str() + 2 : op.c_str());
        int has = 0;
        if (key == "sounds") has = nSound;
        else if (key == "tex2d") has = nTex;
        else { err = "projectcaches: assert desconocido '" + key + "' (sonidos|tex2d)"; return false; }
        if (toMinus ? (has < v) : (has != v)) {
            char b[160]; sprintf(b, "projectcaches: %s=%d y se esperaba %s", key.c_str(), has, op.c_str());
            err = b; return false;
        }
    }
    return true;
}

// ============================================================================
//  juego2dmin / juego2dpx
// ============================================================================
// el JUEGO 2D PURO minimo (sin un solo objeto 3D): "Compilar juego" lo arma con
// kSourcesBase SOLO (sin kSources3D), que es justo lo que no probaba nadie: el
// juego 3D (prueba_juego3d) linkea con import_w3d.cpp adentro y tapa cualquier
// simbolo de ahi que el codigo compartido 2D nombre (BindsJuego -> importW3D).
// La franja nace OCULTA y la prende el .lua solo si los binds 2D existen en el
// binario (importW3D incluido): juego2dpx la busca por pixel en la captura.
static bool PrCreateFolder(const std::string& dir) {
#ifdef _WIN32
    _mkdir(dir.c_str());
#else
    mkdir(dir.c_str(), 0755);
#endif
    struct stat st;
    return stat(dir.c_str(), &st) == 0;
}

static bool CmdGame2DMin(std::istringstream& ss, std::string& err) {
    const std::string dir = PrReadPath(ss);
    if (dir.empty()) { err = "game2dmin: uso: game2dmin <carpeta>"; return false; }
    if (!PrCreateFolder(dir)) { err = "game2dmin: no pude crear " + dir; return false; }
    { FILE* f = fopen((dir + "/juego2dmin.lua").c_str(), "w");
      if (!f) { err = "game2dmin: no pude escribir el .lua"; return false; }
      fputs("-- juego 2D minimo (prueba del runtime compilado SIN 3D)\n"
            "function start() end\n"
            "function update(dt)\n"
            "  local w, h = screen()                  -- set 2D compartido\n"
            "  local s = sound(\"no-existe.wav\")       -- nil: no hay wav, pero el bind existe\n"
            "  local imp = (type(importW3D) == \"function\") -- vive en el lector 3D\n"
            "  if w and w > 0 and imp then show(object(\"franja\"), true) end\n"
            "end\n", f);
      fclose(f); }
    RestartScene();
    UI* ui = new UI(NULL, Vector3(0, 0, 0));
    ui->name = "HUD";
    ui->sameAsRender = true;
    if (!ui->scriptData) ui->scriptData = new W3dScriptData();
    { W3dScriptEntry e; e.path = "juego2dmin.lua";
      e.refs.push_back(std::make_pair(std::string("franja"), std::string("Franja")));
      ui->scriptData->scripts.push_back(e); }
    Rect2D* stripe = new Rect2D(ui, Vector3(0.0f, 0.0f, 0.0f));   // centrada
    stripe->name = "Franja";
    stripe->width = 200.0f; stripe->height = 60.0f;
    stripe->color[0] = 0.0f; stripe->color[1] = 1.0f; stripe->color[2] = 0.0f; stripe->color[3] = 1.0f;
    stripe->visible = false;   // la prende el script (solo si los binds existen en el binario)
    DeselectAll(); ActiveObj = NULL;
    printf("      [juego2dmin] HUD con script + franja oculta (sin 3D) en '%s'\n", dir.c_str());
    return true;
}

static bool CmdGame2DPx(std::istringstream& ss, std::string& err) {
    std::string dir, name; ss >> dir >> name;
    if (dir.empty() || name.empty()) { err = "game2dpx: uso: game2dpx <carpeta> <nombre>"; return false; }
    const std::string bin = dir + "/build/linux/" + name;
    if (!w3dFileSystem::FileExists(bin)) { err = "game2dpx: no existe el binario compilado '" + bin + "'"; return false; }
    const std::string png = dir + "/captura.png";
    remove(png.c_str());
    char cmdRun[2200];
    snprintf(cmdRun, sizeof(cmdRun),
             "cd \"%s/build/linux\" && W3D_CAPTURE=\"%s\" W3D_CAPTURE_FRAME=20 timeout 120 ./%s > /dev/null 2>&1",
             dir.c_str(), png.c_str(), name.c_str());
    if (system(cmdRun) != 0) { err = "game2dpx: el juego no corrio"; return false; }
    unsigned char* px = 0; int iw = 0, ih = 0;
    if (!w3dEngine::DecodeImage(png.c_str(), &px, &iw, &ih) || !px || iw < 8 || ih < 8) {
        err = "game2dpx: el juego no dejo la captura (no dibujo ningun frame)"; return false;
    }
    const unsigned char* c = px + ((size_t)(ih / 2) * iw + (size_t)(iw / 2)) * 4;   // top-left, sin flip
    const bool okHud = (c[1] > 150 && c[0] < 120 && c[2] < 120);
    printf("      [juego2dpx] %dx%d | centro rgb=%d,%d,%d %s\n", iw, ih, c[0], c[1], c[2],
           okHud ? "OK" : "<-- MAL (la franja no aparecio: al binario le falta un bind 2D)");
    w3dEngine::FreeImage(px);
    if (!okHud) { err = "game2dpx: el juego 2D compilado no corre su script (ver " + png + ")"; return false; }
    return true;
}

// ============================================================================
//  escenadefecto
// ============================================================================
// cierra lo que haya y arma la escena de ARRANCAR SIN ARCHIVO (la misma funcion que
// usa el arranque). Los tests que trabajan "sobre la escena del usuario" (la coleccion
// activa no es la raiz: w3dmf2, w3dmf10, *marcas) la necesitan para poder correr en un
// mismo proceso despues de otros tests.
// 'scenedefault [editor]': la escena por defecto. Las pruebas usan la de SIEMPRE (el cubo en el origen, sin piso); con
// 'editor', la que ve la persona al abrir Whisk3D (el cubo apoyado en el piso de 8 x 8 m, todo 1 m arriba)
static bool CmdSceneDefault(std::istringstream& ss, std::string& err) {
    extern void W3dCreateDefaultScene(); extern bool g_w3dTestScene;
    std::string q; ss >> q;
    const bool prev = g_w3dTestScene;
    if (q == "editor") g_w3dTestScene = false;
    RestartScene();
    W3dCreateDefaultScene();
    g_w3dTestScene = prev;
    if (CollectionActive == SceneCollection) { err = "scenedefault: la coleccion activa quedo en la raiz"; return false; }
    printf("      [escenadefecto] coleccion del usuario + camara + luz + cubo%s\n", q == "editor" ? " + piso (la del editor)" : "");
    return true;
}

// ============================================================================
//  noapilaarmar / noapila : lo que no dibuja nada en el juego no le cuesta NADA al GL. El recorrido de la escena
//  saltea (ni push, ni mult, ni pop) el subarbol que no dibuja (Object::DrawsSomething): un emisor sin particulas vivas,
//  una hitbox, una camara y un armature (sus huesos los dibuja el editor aparte) con los overlays apagados
// ============================================================================
#include "objects/Empty.h"
#include "objects/Particles.h"
#include "objects/Hitbox.h"
// noapilaarmar: un vacio "NoApila" con un emisor (sin particulas), una hitbox, una camara (no la activa) y un armature
static bool CmdNotStacksBuild(std::string& err) {
    if (!SceneCollection) { err = "nostackbuild: no hay escena"; return false; }
    Object* parent = CollectionActive ? (Object*)CollectionActive : (Object*)SceneCollection;
    Empty* v = new Empty(parent, Vector3(0, 0, 0));
    v->name = "NoApila";
    Particles* pa = new Particles(v, Vector3(0, 1, 0));
    pa->name = "NoApila_emisor";
    Hitbox* hb = new Hitbox(v, Vector3(1, 0, 0));
    hb->name = "NoApila_hitbox";
    Camera* ca = new Camera(v, Vector3(0, 0, 3), Vector3(0, 0, 0));
    ca->name = "NoApila_camara";
    Armature* ar = new Armature(v, Vector3(-1, 0, 0));
    ar->name = "NoApila_armature";
    printf("      [noapilaarmar] NoApila: emisor (%d particulas), hitbox, camara y armature\n", (int)pa->sys.parts.size());
    return true;
}
// noapila <objeto> [cuesta] : el render del viewport activo con el objeto visible y oculto hace las MISMAS llamadas GL
// (draws + binds + estados + otras). Con "cuesta" (el control) tienen que ser distintas
static bool CmdNotStacks(std::istringstream& ss, std::string& err) {
    std::string name, mode; ss >> name >> mode;
    Object* o = SceneCollection ? FindObjectByName(SceneCollection, name) : NULL;
    if (!o) { err = "nostack: no existe '" + name + "'"; return false; }
    Viewport3D* vp = Viewport3DActive;
    if (!vp) { err = "nostack: no hay viewport 3D"; return false; }
    const bool prev = o->visible;
    int gl[2];
    for (int k = 0; k < 2; k++) {
        o->visible = (k == 0);
        vp->Render();   // (el primer cuadro arrastra el estado que dejo el anterior: se mide el segundo)
        vp->Render();
        gl[k] = vp->statDrawsFrame + vp->statBindsFrame + vp->statStatesFrame + vp->statOtherFrame;
    }
    o->visible = prev;
    printf("      [noapila] '%s': visible %d llamadas GL, oculto %d\n", name.c_str(), gl[0], gl[1]);
    if (mode == "cuesta") {
        if (gl[0] == gl[1]) { err = "nostack: '" + name + "' tendria que costar llamadas (control) y no cuesta"; return false; }
    } else if (gl[0] != gl[1]) {
        char b[160]; snprintf(b, sizeof(b), "nostack: '%s' cuesta %d llamadas GL sin dibujar nada", name.c_str(), gl[0] - gl[1]);
        err = b; return false;
    }
    return true;
}

// ============================================================================
//  viewports / layoutmaximizar / propsfoco / redopanel
// ============================================================================
// abrir un proyecto LIBERA el layout anterior (antes quedaba huerfano: ~0,2 MB de heap por
// apertura). Estos comandos prueban las dos mitades: que no quede ningun viewport vivo fuera
// del arbol, y que ningun puntero global quede apuntando a uno liberado.
static void PrNodes(ViewportBase* n, std::vector<ViewportBase*>& all, std::vector<ViewportBase*>& leaves) {
    if (!n) return;
    all.push_back(n);
    if (n->isLeaf()) { leaves.push_back(n); return; }
    if (n->ContainerKind() == 1) {
        PrNodes(((ViewportRow*)n)->childA, all, leaves); PrNodes(((ViewportRow*)n)->childB, all, leaves);
    } else {
        PrNodes(((ViewportColumn*)n)->childA, all, leaves); PrNodes(((ViewportColumn*)n)->childB, all, leaves);
    }
}
static bool PrIsLeaf(const std::vector<ViewportBase*>& leaves, const ViewportBase* v) {
    for (size_t i = 0; i < leaves.size(); i++) if (leaves[i] == v) return true;
    return false;
}
static ViewportBase* PrFirstLeaf(int kind) {
    std::vector<ViewportBase*> all, leaves;
    PrNodes(rootViewport, all, leaves);
    for (size_t i = 0; i < leaves.size(); i++) if (leaves[i]->ViewportKind() == kind) return leaves[i];
    return NULL;
}

static bool CmdViewports(std::istringstream& ss, std::string& err) {
    std::vector<ViewportBase*> all, leaves, allVis, leavesVis;
    PrNodes(LayoutCompleteRoot(), all, leaves);   // el arbol COMPLETO (con el maximizado adentro)
    PrNodes(rootViewport, allVis, leavesVis);     // lo que se ve (con uno maximizado, esa hoja)
    const int live = LiveViewports();
    const bool actOk = viewPortActive && PrIsLeaf(leaves, viewPortActive);
    const bool v3dOk = !Viewport3DActive || PrIsLeaf(leaves, (ViewportBase*)Viewport3DActive);
    const bool propsOk = !PropsActive || PrIsLeaf(leaves, (ViewportBase*)PropsActive);
    printf("      [viewports] vivos=%d arbol=%d hojas=%d (visibles %d) maximizado=%s | activo=%s 3d=%s props=%s\n",
           live, (int)all.size(), (int)leaves.size(), (int)leavesVis.size(), LayoutIsMaximized() ? "si" : "no",
           actOk ? "leaf" : (viewPortActive ? "AJENO" : "NULL"),
           Viewport3DActive ? (v3dOk ? "leaf" : "AJENO") : "NULL",
           PropsActive ? (propsOk ? "leaf" : "AJENO") : "NULL");
    if (live != (int)all.size()) {
        char b[200]; sprintf(b, "viewports: hay %d viewports vivos y el arbol tiene %d (un layout quedo huerfano)",
                             live, (int)all.size());
        err = b; return false;
    }
    if (!actOk)   { err = "viewports: viewPortActive no es una hoja del layout actual"; return false; }
    if (!v3dOk)   { err = "viewports: Viewport3DActive apunta a un viewport que no es del layout"; return false; }
    if (!propsOk) { err = "viewports: PropsActivo apunta a un panel que no es del layout"; return false; }
    std::string key;
    while (ss >> key) {
        int v = 0;
        if (!(ss >> v)) { err = "viewports: falta el valor de '" + key + "'"; return false; }
        if (key != "leaves") { err = "viewports: assert desconocido '" + key + "' (uso: viewports [hojas N])"; return false; }
        if ((int)leaves.size() != v) {
            char b[120]; sprintf(b, "viewports: hojas=%d y se esperaba %d", (int)leaves.size(), v);
            err = b; return false;
        }
    }
    return true;
}

static bool CmdLayoutMaximize(std::istringstream& ss, std::string& err) {
    int kind = 0; ss >> kind;
    ViewportBase* v = PrFirstLeaf(kind);
    if (!v) { err = "layoutmaximize: no hay una hoja de ese tipo en el layout"; return false; }
    if (LayoutIsMaximized()) { err = "layoutmaximize: ya hay un viewport maximizado"; return false; }
    viewPortActive = v;
    LayoutMaximize();
    if (!LayoutIsMaximized() || rootViewport != v) { err = "layoutmaximize: no quedo maximizado"; return false; }
    printf("      [layoutmaximizar] hoja kind %d a pantalla completa\n", kind);
    return true;
}

// ---- inputstate [mark] [clave valor]... : lo que deja TRABADO un soltar del mouse que no se proceso: el boton izq/medio
//      apretados, el foco congelado (ViewPortClickDown), la barra de scroll agarrada (LayoutInDrag), el viewport activo,
//      el scroll del panel de propiedades (PosY, hover y agarre de su barra) y la posicion del mouse de SDL (en --script
//      solo la mueve la TELETRANSPORTACION del cursor: CheckWarpMouseInViewport). 'mark' anota PosY y el mouse de SDL;
//      despues 'posy mark' / 'scrolled 1' / 'warped 0' comparan contra eso.
//      Claves: left middle frozen bar press active posy scrolled warped ----
static int gInputMarkPosY = 0, gInputMarkMx = 0, gInputMarkMy = 0;
static bool CmdInputState(std::istringstream& ss, std::string& err) {
    Properties* pr = (Properties*)PrFirstLeaf(3);
    int smx = 0, smy = 0; SDL_GetMouseState(&smx, &smy);
    const int active = viewPortActive ? viewPortActive->ViewportKind() : -1;
    const int posY = pr ? pr->PosY : 0;
    const bool press = pr && (pr->mouseOverScrollYpress || pr->mouseOverScrollXpress);
    printf("      [inputstate] izq=%d medio=%d congelado=%d barra=%d activo=%d | props PosY=%d (max %d%s) sobreBarra=%d agarre=%d"
           " zonaBarra x>=%d | mouseSDL=%d,%d\n",
           leftMouseDown ? 1 : 0, middleMouseDown ? 1 : 0, ViewPortClickDown ? 1 : 0, LayoutInDrag() ? 1 : 0, active,
           posY, pr ? pr->MaxPosY : 0, (pr && pr->scrollY) ? "" : ", sin scroll", (pr && pr->mouseOverScrollY) ? 1 : 0, press ? 1 : 0,
           pr ? pr->x + pr->width - (borderGS + GlobalScale * 9) : -1, smx, smy);
    std::string key;
    while (ss >> key) {
        if (key == "mark") { gInputMarkPosY = posY; gInputMarkMx = smx; gInputMarkMy = smy; continue; }
        std::string val;
        if (!(ss >> val)) { err = "inputstate: falta el valor de '" + key + "'"; return false; }
        int want = (val == "mark") ? gInputMarkPosY : atoi(val.c_str());
        int got = 0;
        if (key == "left") got = leftMouseDown ? 1 : 0;
        else if (key == "middle") got = middleMouseDown ? 1 : 0;
        else if (key == "frozen") got = ViewPortClickDown ? 1 : 0;
        else if (key == "bar") got = LayoutInDrag() ? 1 : 0;
        else if (key == "press") got = press ? 1 : 0;
        else if (key == "active") got = active;
        else if (key == "posy") got = posY;
        else if (key == "scrolled") got = (posY != gInputMarkPosY) ? 1 : 0;
        else if (key == "warped") got = (smx != gInputMarkMx || smy != gInputMarkMy) ? 1 : 0;
        else { err = "inputstate: clave desconocida '" + key + "' (left middle frozen bar press active posy scrolled warped)"; return false; }
        if (got != want) {
            char b[160]; sprintf(b, "inputstate: %s=%d y se esperaba %d", key.c_str(), got, want);
            err = b; return false;
        }
    }
    return true;
}

static bool CmdPropsFocus(std::istringstream& ss, std::string& err) {
    std::string arg; ss >> arg;
    if (arg == "null") {
        printf("      [propsfoco] campo enfocado=%s\n", g_activeTextField ? "SI" : "none");
        if (g_activeTextField) { err = "propsfocus: sigue enfocado un campo (el panel que lo tenia se libero?)"; return false; }
        return true;
    }
    if (!arg.empty()) { err = "propsfocus: uso: propsfocus [nulo]"; return false; }
    Properties* pr = (Properties*)PrFirstLeaf(3);
    if (!pr || !pr->propProjName) { err = "propsfocus: no hay panel de propiedades en el layout"; return false; }
    g_activeTextField = &pr->propProjName->field;
    printf("      [propsfoco] enfocado el campo Nombre del proyecto\n");
    return true;
}

static bool CmdPropsRename(std::istringstream& ss, std::string& err) {
    extern bool RenameActive();
    std::string arg; ss >> arg;
    if (arg == "null") {
        printf("      [propsrename] rename en curso=%s\n", RenameActive() ? "SI" : "no");
        if (RenameActive()) { err = "propsrename: sigue un rename en curso (su boton era de un panel liberado)"; return false; }
        return true;
    }
    if (!arg.empty()) { err = "propsrename: uso: propsrename [nulo]"; return false; }
    Properties* pr = (Properties*)PrFirstLeaf(3);
    if (!pr || !pr->propBtnRenameMat || !pr->propBtnRenameMat->action) { err = "propsrename: no hay panel de propiedades"; return false; }
    PropsActive = pr;                      // las acciones de la tarjeta operan sobre el panel activo
    pr->propBtnRenameMat->action();
    if (!RenameActive()) { err = "propsrename: no arranco (el objeto activo tiene un material propio?)"; return false; }
    printf("      [propsrename] rename del material en curso\n");
    return true;
}

static bool CmdRedoPanel(std::istringstream& ss, std::string& err) {
    int expected = -1;
    if (ss >> expected) {
        const bool active = RedoMeshPanelActive();
        printf("      [redopanel] abierto=%s\n", active ? "si" : "no");
        if ((expected != 0) != active) { err = active ? "redopanel: el panel sigue abierto" : "redopanel: el panel no esta abierto"; return false; }
        return true;
    }
    ViewportBase* v = PrFirstLeaf(1);
    if (!v) { err = "redopanel: no hay viewport 3D en el layout"; return false; }
    Viewport3DActive = (Viewport3D*)v;   // el panel recuerda el 3D ACTIVO como su creador
    Mesh* m = (Mesh*)NewMesh(MeshType::cube, NULL, false);
    if (!m) { err = "redopanel: NewMesh devolvio NULL"; return false; }
    OpenRedoMeshPanel(m);
    if (!RedoMeshPanelActive()) { err = "redopanel: el panel no se abrio"; return false; }
    printf("      [redopanel] cubo nuevo + panel Add abierto sobre el primer 3D\n");
    return true;
}

// ============================================================================
//  propsarm2d
// ============================================================================
// Las filas Pos/Rotation/Scale de la tarjeta Armature 2D eran punteros ESTATICOS a las filas
// del ULTIMO panel construido: al liberarse ese panel (cambiarle el tipo, Expand sobre el otro,
// cualquier panel temporal del harness) quedaban colgando y UpdateTabs de OTRO panel
// escribia sobre filas liberadas (corrupcion del heap en Release), mientras sus propias filas
// nunca se refrescaban. Aca un panel temporal nace y muere ANTES de mirar, y cada panel del
// layout tiene que show/ocultar SUS filas segun el modo.
static bool CmdPropsArm2D(std::istringstream& ss, std::string& err) {
    std::string mode; ss >> mode;
    if (mode != "bones" && mode != "pose") { err = "propsarm2d: uso: propsarm2d huesos|pose [paneles N]"; return false; }
    const bool pose = (mode == "pose");
    int expectedPanels = -1;
    { std::string key;
      while (ss >> key) {
          int v = 0;
          if (!(ss >> v)) { err = "propsarm2d: falta el valor de '" + key + "'"; return false; }
          if (key != "panels") { err = "propsarm2d: assert desconocido '" + key + "' (uso: propsarm2d huesos|pose [paneles N])"; return false; }
          expectedPanels = v;
      } }
    UVEditor* uv = (UVEditor*)PrFirstLeaf(4);
    if (!uv) { err = "propsarm2d: no hay editor UV en el layout"; return false; }
    if (!ActiveObj || ActiveObj->getType() != ObjectType::mesh || ActiveObj != g_editMesh ||
        !((Mesh*)ActiveObj)->HasArm2D() || ((Mesh*)ActiveObj)->Arm2DBones().empty()) {
        err = "propsarm2d: hace falta la malla activa en Edit Mode con un armature 2D y algun hueso (bone2dnew)";
        return false;
    }
    Mesh* m = (Mesh*)ActiveObj;
    if (m->Arm2DActiveBone() < 0 || m->Arm2DActiveBone() >= (int)m->Arm2DBones().size())
        m->Arm2DActiveBone() = 0;                  // las filas del hueso piden un hueso ACTIVO
    uv->uvMode = pose ? UVModePose : UVModeBones;  // queda asi (el script sigue en ese modo)
    // el panel temporal: pasa a ser el ULTIMO construido y se libera enseguida
    { Properties* tmp = new Properties(); delete tmp; }

    std::vector<ViewportBase*> all, leaves;
    PrNodes(LayoutCompleteRoot(), all, leaves);
    int panels = 0;
    bool ok = true;
    for (size_t i = 0; i < leaves.size(); i++) {
        if (leaves[i]->ViewportKind() != 3) continue;
        Properties* pr = (Properties*)leaves[i];
        panels++;
        pr->activeTab = 6;                     // la pestania Armature 2D
        pr->UpdateTabs();
        const bool card = pr->propBones2D && pr->propBones2D->visible && pr->activeTab == 6;
        const bool pos = pr->propBone2DPosX && pr->propBone2DPosX->value && pr->propBone2DPosY && pr->propBone2DPosY->value;
        const bool rot = pr->propBone2DRot && pr->propBone2DRot->value;
        const bool scl = pr->propBone2DSclX && pr->propBone2DSclX->value && pr->propBone2DSclY && pr->propBone2DSclY->value;
        const bool good = card && pos && rot == pose && scl == pose;
        printf("      [propsarm2d] panel %d (%s): tarjeta=%s Pos=%s Rotation=%s Scale=%s -> %s\n",
               panels, mode.c_str(), card ? "si" : "NO", pos ? "visible" : "hidden",
               rot ? "visible" : "hidden", scl ? "visible" : "hidden", good ? "OK" : "MAL");
        if (!good) ok = false;
    }
    if (panels == 0) { err = "propsarm2d: no hay paneles de propiedades en el layout"; return false; }
    if (expectedPanels >= 0 && panels != expectedPanels) {
        char b[120]; sprintf(b, "propsarm2d: hay %d paneles y se esperaban %d", panels, expectedPanels);
        err = b; return false;
    }
    if (!ok) {
        err = pose ? "propsarm2d: algun panel no muestra Pos + Rotation + Scale posando (no refresca SUS filas)"
                   : "propsarm2d: algun panel no muestra Pos (u oculta Rotation/Scale) editando huesos";
        return false;
    }
    return true;
}

// ============================================================================
//  iconospkg
// ============================================================================
// Un icono nuevo va en cuatro lugares (enum + KNames + png del skin + los DOS .pkg). Los
// .pkg se mantienen a mano y nadie los compila en PC: un icono que se olvida ahi no se instala
// en el celular y el atlas cae al arte legacy de otro icono, sin ningun error. Esto cruza las
// cuatro listas desde la PC.
static bool CmdIconsPkg(std::string& err) {
    const char* bases[5] = { "", "../", "../../", "../../../", "../../../../" };
    std::string root; bool has = false;
    for (int i = 0; i < 5 && !has; i++)
        if (w3dFileSystem::FileExists(std::string(bases[i]) + "platform/symbian/sis/Whisk3D_armv5.pkg")) {
            root = bases[i]; has = true;
        }
    if (!has) { err = "iconspkg: no encuentro platform/symbian/sis (correr desde tools/pruebas)"; return false; }
    const char* pkgs[2] = { "Whisk3D_armv5.pkg", "Whisk3D_gcce.pkg" };
    std::string text[2];
    for (int k = 0; k < 2; k++) {
        std::vector<unsigned char> b;
        if (!w3dFileSystem::ReadFileBytes(root + "platform/symbian/sis/" + pkgs[k], b) || b.empty()) {
            err = std::string("iconspkg: no pude leer ") + pkgs[k]; return false;
        }
        text[k].assign((const char*)&b[0], b.size());
    }
    const std::string dirIcons = root + "res/Skins/Whisk3D/atlas/icons/";
    int missingPng = 0, badSize = 0, missingPkg = 0, extraPkg = 0;
    for (int i = 0; i < (int)ICON_TOTAL; i++) {
        const std::string n = IconName(i);
        if (n.empty()) { char b[80]; sprintf(b, "iconspkg: el icono %d no tiene nombre (KNombres)", i); err = b; return false; }
        const std::string png = dirIcons + n + ".png";
        if (!w3dFileSystem::FileExists(png)) {
            printf("      [iconospkg] '%s': falta %s\n", n.c_str(), png.c_str()); missingPng++;
        } else {
            unsigned char* px = 0; int w = 0, h = 0;
            const bool dec = w3dEngine::DecodeImage(png.c_str(), &px, &w, &h) && px;
            if (px) w3dEngine::FreeImage(px);
            if (!dec || w != 10 || h != 10) {
                printf("      [iconospkg] '%s': el png no es de 10x10 (%dx%d)\n", n.c_str(), w, h); badSize++;
            }
        }
        // cada .pkg lo nombra DOS veces: el origen (res\...) y el destino (!:\private\...)
        const std::string ref = "\\atlas\\icons\\" + n + ".png\"";
        for (int k = 0; k < 2; k++) {
            int times = 0;
            for (size_t p = text[k].find(ref); p != std::string::npos; p = text[k].find(ref, p + 1)) times++;
            if (times < 2) { printf("      [iconospkg] '%s': falta en %s\n", n.c_str(), pkgs[k]); missingPkg++; }
        }
    }
    // al reves: todo png del skin que instala un .pkg existe (sino makesis corta el empaquetado)
    const std::string mark = "res\\Skins\\Whisk3D\\atlas\\icons\\";
    int installed[2] = { 0, 0 };
    for (int k = 0; k < 2; k++) {
        for (size_t p = text[k].find(mark); p != std::string::npos; p = text[k].find(mark, p + 1)) {
            const size_t a = p + mark.size();
            const size_t e = text[k].find('"', a);
            if (e == std::string::npos) break;
            const std::string file = text[k].substr(a, e - a);
            installed[k]++;
            if (!w3dFileSystem::FileExists(dirIcons + file)) {
                printf("      [iconospkg] %s instala '%s' y no existe en el skin\n", pkgs[k], file.c_str()); extraPkg++;
            }
        }
    }
    printf("      [iconospkg] ICON_TOTAL=%d | %s instala %d, %s instala %d | faltan png=%d, png que no son 10x10=%d, "
           "faltan en los .pkg=%d, .pkg que instalan un png inexistente=%d\n",
           (int)ICON_TOTAL, pkgs[0], installed[0], pkgs[1], installed[1], missingPng, badSize, missingPkg, extraPkg);
    if (missingPng || badSize || missingPkg || extraPkg) {
        err = "iconspkg: los iconos del enum, el skin y los .pkg de Symbian no coinciden (ver arriba)";
        return false;
    }
    return true;
}

// ============================================================================
//  LA MALLA BINARIA (.w3db, io/W3dMeshBin.h): formato, equivalencia y robustez
// ============================================================================

// ---- formatmeshes [texto|binario] | proyecto [texto|binario] | es texto|binario | todas texto|binario ----
// 'todas': el tilde "Mesh as text" de CADA malla del proyecto (lo mismo que tocarlo en la tarjeta de cada una)
static bool CmdFormatMeshes(std::istringstream& ss, std::string& err) {
    std::string a, b; ss >> a >> b;
    if (a == "text")        g_w3dFormatForcedMeshes = W3D_MESHES_TEXT;
    else if (a == "binary") g_w3dFormatForcedMeshes = W3D_MESHES_BINARY;
    else if (a == "project") {
        g_w3dFormatForcedMeshes = -1;
        if (b == "text")        g_w3dFormatMeshesProject = W3D_MESHES_TEXT;
        else if (b == "binary") g_w3dFormatMeshesProject = W3D_MESHES_BINARY;
        else if (!b.empty()) { err = "formatmeshes: 'proyecto' acepta texto|binario"; return false; }
    } else if (a == "es") {
        const int want = (b == "text") ? W3D_MESHES_TEXT : W3D_MESHES_BINARY;
        if (b != "text" && b != "binary") { err = "formatmeshes: uso: formatmeshes es texto|binario"; return false; }
        if (W3dFormatMeshes() != want) {
            err = std::string("formatmeshes: el formato vigente es ") + (W3dFormatMeshes() == W3D_MESHES_TEXT ? "text" : "binary");
            return false;
        }
    } else if (a == "all") {
        if (b != "text" && b != "binary") { err = "formatmeshes: uso: formatmeshes todas texto|binario"; return false; }
        const std::vector<MeshResource*>& reg = W3dMeshesRegistry();
        int n = 0;
        for (size_t i = 0; i < reg.size(); i++) if (!reg[i]->deleted) { W3dMeshResourceSetText(reg[i], b == "text"); n++; }
        printf("      [formatomallas] %d malla(s) a %s\n", n, b.c_str());
    } else if (!a.empty()) { err = "formatmeshes: uso: formatmeshes [texto|binario|proyecto [texto|binario]|es texto|binario|todas texto|binario]"; return false; }
    printf("      [formatomallas] vigente=%s (proyecto=%s, forzado=%s)\n",
           W3dFormatMeshes() == W3D_MESHES_TEXT ? "text" : "binary",
           g_w3dFormatMeshesProject == W3D_MESHES_TEXT ? "text" : "binary",
           g_w3dFormatForcedMeshes < 0 ? "no" : (g_w3dFormatForcedMeshes == W3D_MESHES_TEXT ? "text" : "binary"));
    return true;
}

// ---- logloadmeshes <w3db> <w3dm> <rechazadas> ----
// La ULTIMA linea "[CARGA] mallas .w3db=" del log (la escribe OpenW3D en cada apertura, con o sin
// reloj de medicion) tiene que decir exactamente esas cuentas. Es lo que se lee en el log del
// telefono (no tiene harness) para saber que las .w3db abrieron listas, sin derivar nada.
static bool CmdLogLoadMeshes(std::istringstream& ss, std::string& err) {
    int bin = -1, tex = -1, rejected = -1;
    if (!(ss >> bin >> tex >> rejected)) { err = "logloadmeshes: uso: logloadmeshes <w3db> <w3dm> <rechazadas>"; return false; }
    const char* pre = "[CARGA] mallas .w3db=";
    for (int i = w3dLogRingCount() - 1; i >= 0; i--) {
        const char* l = strstr(w3dLogRingLine(i), pre);
        if (!l) continue;
        int b = -1, t = -1, r = -1;
        if (sscanf(l, "[CARGA] mallas .w3db=%d (listas, sin derivar)  .w3dm=%d (derivadas al abrir)  .w3db rechazadas=%d", &b, &t, &r) != 3) {
            err = std::string("logloadmeshes: la linea no tiene el formato esperado: ") + l; return false;
        }
        printf("      [logcargamallas] .w3db=%d .w3dm=%d rechazadas=%d\n", b, t, r);
        if (b != bin || t != tex || r != rejected) {
            char x[160]; sprintf(x, "logloadmeshes: el log dice .w3db=%d .w3dm=%d rechazadas=%d y se esperaba %d %d %d", b, t, r, bin, tex, rejected);
            err = x; return false;
        }
        return true;
    }
    err = "logloadmeshes: no hay ninguna linea \"[CARGA] mallas .w3db=\" en el log"; return false;
}

// ---- las entradas de mallas de un .w3d (nombre + bytes) ----
static bool PrEntriesMeshes(const std::string& path, std::vector<W3dZipEntry>& out, int& nBin, int& nTxt) {
    out.clear(); nBin = nTxt = 0;
    std::vector<W3dZipEntry> ents;
    if (!W3dZipRead(path, &ents)) return false;
    for (size_t i = 0; i < ents.size(); i++) {
        const std::string& n = ents[i].name;
        if (n.compare(0, 7, "meshes/") != 0) continue;
        const bool bin = n.size() > 5 && n.compare(n.size() - 5, 5, ".w3db") == 0;
        const bool txt = n.size() > 5 && n.compare(n.size() - 5, 5, ".w3dm") == 0;
        if (bin) nBin++;
        if (txt) nTxt++;
        if (bin) out.push_back(ents[i]);
    }
    return true;
}

// ---- w3dbinfo <ruta.w3d> [detalle] [w3db N] [w3dm N] [tiene XXXX] [notiene XXXX] ----
//      cuenta las mallas binarias y de texto del contenedor, lista los bloques de las
//      binarias (todas con 'detalle'; si no, la primera) y valida que CADA .w3db se lea
//      con el lector de render. 'tiene'/'notiene' miran TODAS las .w3db ("RUV" = "RUV ").
static bool CmdW3dbInfo(std::istringstream& ss, std::string& err) {
    const std::string path = PrReadPath(ss);
    if (path.empty()) { err = "w3dbinfo: uso: w3dbinfo <ruta.w3d> [detalle] [w3db N] [w3dm N] [tiene XXXX] [notiene XXXX]"; return false; }
    bool detail = false;
    std::vector<std::string> keys, values;
    std::string t;
    while (ss >> t) {
        if (t == "detail") { detail = true; continue; }
        std::string v;
        if (!(ss >> v)) { err = "w3dbinfo: falta el valor de '" + t + "'"; return false; }
        while (v.size() < 4 && (t == "has" || t == "hasnot")) v += ' ';
        keys.push_back(t); values.push_back(v);
    }
    std::vector<W3dZipEntry> bins; int nBin = 0, nTxt = 0;
    if (!PrEntriesMeshes(path, bins, nBin, nTxt)) { err = "w3dbinfo: no pude leer " + path; return false; }
    int broken = 0;
    long bytesBin = 0;
    std::map<std::string, int> withBlock;   // fourcc -> en cuantas .w3db esta
    for (size_t i = 0; i < bins.size(); i++) {
        const std::vector<unsigned char>& d = bins[i].data;
        bytesBin += (long)d.size();
        std::vector<std::string> lines;
        const bool ok = !d.empty() && W3dMeshBinList(&d[0], d.size(), lines);
        W3dMeshRender r; W3dMeshInfo info;
        const bool leeRender = ok && W3dMeshBinReadRender(&d[0], d.size(), r, &info);
        if (!leeRender) broken++;
        if (detail || i == 0) {
            printf("      [w3dbinfo] %s (%d B)%s\n", bins[i].name.c_str(), (int)d.size(), leeRender ? "" : " <-- NO SE LEE");
            for (size_t k = 0; k < lines.size(); k++) printf("      [w3dbinfo]    %s\n", lines[k].c_str());
        }
        for (size_t k = 0; k < lines.size(); k++) withBlock[lines[k].substr(0, 4)]++;
    }
    printf("      [w3dbinfo] %s: mallas binarias=%d (%ld B) de texto=%d | no se leen=%d\n",
           path.c_str(), nBin, bytesBin, nTxt, broken);
    if (broken > 0) { err = "w3dbinfo: hay .w3db que el lector de render no acepta"; return false; }
    for (size_t i = 0; i < keys.size(); i++) {
        const std::string& c = keys[i]; const std::string& v = values[i];
        char b[200];
        if (c == "w3db" || c == "w3dm") {
            const int esp = atoi(v.c_str()), has = (c == "w3db") ? nBin : nTxt;
            if (has != esp) { sprintf(b, "w3dbinfo: %s=%d y se esperaba %d", c.c_str(), has, esp); err = b; return false; }
        } else if (c == "has" || c == "hasnot") {
            const int n = withBlock.count(v) ? withBlock[v] : 0;
            if (c == "has" && n != nBin) { sprintf(b, "w3dbinfo: el bloque '%s' esta en %d de %d .w3db", v.c_str(), n, nBin); err = b; return false; }
            if (c == "hasnot" && n != 0)  { sprintf(b, "w3dbinfo: el bloque '%s' aparece en %d .w3db", v.c_str(), n); err = b; return false; }
        } else { err = "w3dbinfo: assert desconocido '" + c + "'"; return false; }
    }
    return true;
}

// ---------------------------------------------------------------------------
//  mallafoto / mallafotoigual: la FOTO de todas las mallas del arbol, campo por
//  campo (un hash de los bytes de cada uno), para asertar que dos cargas dejan
//  EXACTAMENTE la misma malla en memoria. Con 'frame N' ademas se evalua en ese
//  cuadro el skinning (si tiene armature) y cada vertex anim; y siempre las shape
//  keys a peso 0.5.
// ---------------------------------------------------------------------------
struct PrHash {
    unsigned a, b; unsigned long n;
    PrHash() : a(2166136261u), b(5381u), n(0) {}
    void Bytes(const void* p, size_t k) {
        const unsigned char* c = (const unsigned char*)p;
        for (size_t i = 0; i < k; i++) { a ^= c[i]; a *= 16777619u; b = b * 33u + c[i]; }
        n += (unsigned long)k;
    }
    void Int(int v) { Bytes(&v, sizeof(v)); }
    void Float(float f) { Bytes(&f, sizeof(f)); }
    void Str(const std::string& s) { Int((int)s.size()); if (!s.empty()) Bytes(s.data(), s.size()); }
    bool Equal(const PrHash& o) const { return a == o.a && b == o.b && n == o.n; }
};
typedef std::map<std::string, PrHash> PrFields;
typedef std::map<std::string, PrFields> PrSnapshot;     // ruta del objeto -> campos
static std::map<std::string, PrSnapshot> gPrSnapshots;

static std::string PrObjPath(Object* o) {
    std::string r = o->name;
    for (Object* p = o->Parent; p && p != SceneCollection; p = p->Parent) r = p->name + "/" + r;
    return r;
}
static void PrCollectMeshes(Object* o, std::vector<Mesh*>& out) {
    if (!o) return;
    if (o != SceneCollection && o->getType() == ObjectType::mesh) out.push_back((Mesh*)o);
    for (size_t i = 0; i < o->Childrens.size(); i++) PrCollectMeshes(o->Childrens[i], out);
}

// los campos de UNA malla (render siempre; edicion salvo 'noEdit')
static void PrSnapshotFields(Mesh* m, PrFields& c, bool noEdit) {
    const int nV = m->vertex ? m->vertexSize : 0;
    { PrHash h; h.Int(nV); if (nV) h.Bytes(m->vertex, (size_t)nV * 12); c["vertex"] = h; }
    { PrHash h; if (nV && m->normals) h.Bytes(m->normals, (size_t)nV * 3); c["normals"] = h; }
    { PrHash h; if (nV && m->uv) h.Bytes(m->uv, (size_t)nV * 8); c["uv"] = h; }
    { PrHash h; if (nV && m->vertexColor) h.Bytes(m->vertexColor, (size_t)nV * 4); c["vertexColor"] = h; }
    { PrHash h; h.Int(m->facesSize); if (m->faces && m->facesSize > 0) h.Bytes(m->faces, (size_t)m->facesSize * sizeof(MeshIndex)); c["faces"] = h; }
    { PrHash h;
      for (size_t g = 0; g < m->materialsGroup.size(); g++) {
          const MaterialGroup& mg = m->materialsGroup[g];
          h.Str(mg.name); h.Str(mg.material ? mg.material->name : std::string("<ninguno>"));
          h.Int(mg.startDrawn); h.Int(mg.indicesDrawnCount); h.Int(mg.start); h.Int(mg.count);
      }
      c["materialsGroup"] = h; }
    { PrHash h; h.Int((int)m->vertCtrlPoint.size()); if (!m->vertCtrlPoint.empty()) h.Bytes(&m->vertCtrlPoint[0], m->vertCtrlPoint.size() * 4); c["vertCtrlPoint"] = h; }
    { PrHash h;
      for (size_t g = 0; g < m->vertexGroups.size(); g++) {
          const VertexGroup* vg = m->vertexGroups[g]; if (!vg) { h.Int(-1); continue; }
          h.Str(vg->name); h.Int((int)vg->verts.size());
          if (!vg->verts.empty()) h.Bytes(&vg->verts[0], vg->verts.size() * 4);
          if (!vg->weights.empty()) h.Bytes(&vg->weights[0], vg->weights.size() * 4);
      }
      h.Int(m->activeGroup); c["vertexGroups"] = h; }
    { PrHash h;
      for (size_t k = 0; k < m->shapeKeys.size(); k++) {
          const W3dShapeKey& sk = m->shapeKeys[k];
          h.Str(sk.name); h.Int((int)sk.idx.size());
          if (!sk.idx.empty()) h.Bytes(&sk.idx[0], sk.idx.size() * 4);
          if (!sk.d.empty()) h.Bytes(&sk.d[0], sk.d.size() * 4);
      }
      h.Int((int)m->shapeWeights.size()); c["shapeKeys"] = h; }
    { PrHash h;
      for (size_t g = 0; g < m->uvGroups.size(); g++) {
          const UVGroup* ug = m->uvGroups[g]; if (!ug) { h.Int(-1); continue; }
          h.Str(ug->name); h.Int((int)ug->verts.size());
          if (!ug->verts.empty()) h.Bytes(&ug->verts[0], ug->verts.size() * 4);
          if (!ug->weights.empty()) h.Bytes(&ug->weights[0], ug->weights.size() * 4);
      }
      h.Int(m->uvActiveGroup); c["uvGroups"] = h; }
    { PrHash h; h.Int((int)m->uv2dRest.size()); if (!m->uv2dRest.empty()) h.Bytes(&m->uv2dRest[0], m->uv2dRest.size() * 4); c["uv2dRest"] = h; }
    { PrHash h; h.Int(m->aabbOk ? 1 : 0);
      if (m->aabbOk) { h.Float(m->aabbMin.x); h.Float(m->aabbMin.y); h.Float(m->aabbMin.z);
                       h.Float(m->aabbMax.x); h.Float(m->aabbMax.y); h.Float(m->aabbMax.z); }
      c["aabb"] = h; }
    { PrHash h; h.Float(m->centerGeom.x); h.Float(m->centerGeom.y); h.Float(m->centerGeom.z); h.Float(m->radiusGeom); c["radiusCenter"] = h; }
    { PrHash h; h.Int(m->meshSmooth ? 1 : 0); h.Int(m->meshType);
      if (m->meshType >= 0) { h.Float(m->meshSize); h.Float(m->meshSize2); h.Float(m->meshDepth); h.Int(m->meshVerts); h.Int(m->meshVerts2); }
      h.Int(m->noEditable ? 1 : 0); c["header"] = h; }
    // la malla GENERADA por el stack de modificadores (si tiene): se regenera al abrir
    { PrHash h; h.Int(m->genValid ? 1 : 0);
      if (m->genValid) {
          h.Int(m->genVertexSize); h.Int(m->genFacesSize);
          if (m->genVertex && m->genVertexSize > 0) h.Bytes(m->genVertex, (size_t)m->genVertexSize * 12);
          if (m->genFaces && m->genFacesSize > 0) h.Bytes(m->genFaces, (size_t)m->genFacesSize * sizeof(MeshIndex));
      }
      c["gen"] = h; }
    if (noEdit) return;
    { PrHash h;
      for (size_t f = 0; f < m->faces3d.size(); f++) {
          const MeshFace& F = m->faces3d[f];
          h.Int((int)F.idx.size()); if (!F.idx.empty()) h.Bytes(&F.idx[0], F.idx.size() * 4);
          h.Int(F.mat); h.Int(F.smooth);
      }
      c["faces3d"] = h; }
    { PrHash h; h.Int((int)m->looseEdges.size()); if (!m->looseEdges.empty()) h.Bytes(&m->looseEdges[0], m->looseEdges.size() * 4);
      h.Int((int)m->looseVerts.size()); if (!m->looseVerts.empty()) h.Bytes(&m->looseVerts[0], m->looseVerts.size() * 4);
      c["loose"] = h; }
    { PrHash h; h.Int((int)m->cornerNormal.size()); if (!m->cornerNormal.empty()) h.Bytes(&m->cornerNormal[0], m->cornerNormal.size()); c["cornerNormal"] = h; }
    { PrHash h;
      for (size_t l = 0; l < m->uvMaps.size(); l++) { h.Str(m->uvMaps[l]->name); h.Int((int)m->uvMaps[l]->uv.size());
          if (!m->uvMaps[l]->uv.empty()) h.Bytes(&m->uvMaps[l]->uv[0], m->uvMaps[l]->uv.size() * 4); }
      h.Int(m->uvActiveMap); c["uvMaps"] = h; }
    { PrHash h;
      for (size_t l = 0; l < m->colorLayers.size(); l++) { h.Str(m->colorLayers[l]->name); h.Int(m->colorLayers[l]->perVertex ? 1 : 0);
          h.Int((int)m->colorLayers[l]->color.size());
          if (!m->colorLayers[l]->color.empty()) h.Bytes(&m->colorLayers[l]->color[0], m->colorLayers[l]->color.size()); }
      h.Int(m->activeColor); c["colorLayers"] = h; }
    { PrHash h;
      for (std::set<std::string>::const_iterator it = m->sharpEdges.begin(); it != m->sharpEdges.end(); ++it) h.Str(*it);
      h.Int(-7);
      for (std::set<std::string>::const_iterator it = m->seamEdges.begin(); it != m->seamEdges.end(); ++it) h.Str(*it);
      c["marks"] = h; }
    { PrHash h; h.Int((int)m->edges.size()); if (!m->edges.empty()) h.Bytes(&m->edges[0], m->edges.size() * 4);
      h.Int((int)m->bordersBuf.size()); if (!m->bordersBuf.empty()) h.Bytes(&m->bordersBuf[0], m->bordersBuf.size() * 4);
      c["edges"] = h; }
    { PrHash h; h.Int((int)m->posRep.size()); if (!m->posRep.empty()) h.Bytes(&m->posRep[0], m->posRep.size() * 4); h.Int(m->groupedVerts); c["posRep"] = h; }
    { PrHash h;
      for (size_t i = 0; i < m->w3dmForeign.requiresValue.size(); i++) h.Str(m->w3dmForeign.requiresValue[i]);
      for (size_t i = 0; i < m->w3dmForeign.blocks.size(); i++) { h.Str(m->w3dmForeign.blocks[i].name); h.Str(m->w3dmForeign.blocks[i].text); }
      for (size_t i = 0; i < m->w3dmForeign.blocksBin.size(); i++) { h.Str(m->w3dmForeign.blocksBin[i].name); h.Str(m->w3dmForeign.blocksBin[i].text); }
      c["preservacion"] = h; }
}

// lo DINAMICO en el cuadro f: skinning, vertex anims y shape keys (se evalua y se restaura)
static void PrSnapshotFrame(Mesh* m, int f, PrFields& c) {
    char k[48];
    const int nV = m->vertex ? m->vertexSize : 0;
    if (m->skinArmature && nV > 0) {
        const int prev = CurrentFrame;
        CurrentFrame = f;
        m->skinArmature->lastPoseFrame = -999999;
        m->lastSkinFrame = -999999;
        SkinMesh(m);
        PrHash h;
        if (m->skinVertex) h.Bytes(m->skinVertex, (size_t)nV * 12);
        if (m->skinNormals) h.Bytes(m->skinNormals, (size_t)nV * 3);
        sprintf(k, "skin@%d", f); c[k] = h;
        CurrentFrame = prev;
    }
    for (size_t a = 0; a < m->animations.size() && nV > 0; a++) {
        VertexAnimation* an = m->animations[a];
        if (!an) continue;
        std::vector<GLfloat> pv(m->vertex, m->vertex + (size_t)nV * 3), uv;
        std::vector<GLbyte> nv;
        if (m->normals) nv.assign(m->normals, m->normals + (size_t)nV * 3);
        if (m->uv) uv.assign(m->uv, m->uv + (size_t)nV * 2);
        const bool posed = m->posedByAnim;
        EvalVertexAnim(*an, m, (float)f);
        PrHash h;
        h.Bytes(m->vertex, (size_t)nV * 12);
        if (m->normals) h.Bytes(m->normals, (size_t)nV * 3);
        if (m->uv) h.Bytes(m->uv, (size_t)nV * 8);
        sprintf(k, "va%d@%d", (int)a, f); c[k] = h;
        memcpy(m->vertex, &pv[0], pv.size() * 4);
        if (m->normals && !nv.empty()) memcpy(m->normals, &nv[0], nv.size());
        if (m->uv && !uv.empty()) memcpy(m->uv, &uv[0], uv.size() * 4);
        m->posedByAnim = posed;
    }
}
static void PrSnapshotShapes(Mesh* m, PrFields& c) {
    const int nV = m->vertex ? m->vertexSize : 0;
    if (m->shapeKeys.empty() || nV <= 0 || (int)m->vertCtrlPoint.size() != nV) return;
    int nCp = 0;
    for (int i = 0; i < nV; i++) if (m->vertCtrlPoint[i] + 1 > nCp) nCp = m->vertCtrlPoint[i] + 1;
    std::vector<float> d((size_t)nCp * 3, 0.0f);
    for (size_t k = 0; k < m->shapeKeys.size(); k++) {
        const W3dShapeKey& sk = m->shapeKeys[k];
        for (size_t j = 0; j < sk.idx.size() && j * 3 + 2 < sk.d.size(); j++) {
            const int cp = sk.idx[j]; if (cp < 0 || cp >= nCp) continue;
            d[(size_t)cp*3] += 0.5f * sk.d[j*3]; d[(size_t)cp*3+1] += 0.5f * sk.d[j*3+1]; d[(size_t)cp*3+2] += 0.5f * sk.d[j*3+2];
        }
    }
    PrHash h;
    for (int i = 0; i < nV; i++) {
        const int cp = m->vertCtrlPoint[i];
        float p[3] = { m->vertex[i*3] + d[(size_t)cp*3], m->vertex[i*3+1] + d[(size_t)cp*3+1], m->vertex[i*3+2] + d[(size_t)cp*3+2] };
        h.Bytes(p, 12);
    }
    c["formas@0.5"] = h;
}

// ---- meshsnapshot <nombre> [frame N]... [sinedicion] ----
static bool CmdMeshSnapshot(std::istringstream& ss, std::string& err) {
    std::string name; ss >> name;
    if (name.empty()) { err = "meshsnapshot: uso: meshsnapshot <nombre> [frame N]... [sinedicion]"; return false; }
    std::vector<int> frames; bool noEdit = false;
    std::string t;
    while (ss >> t) {
        if (t == "noedit") { noEdit = true; continue; }
        if (t == "frame") { int f = 0; if (!(ss >> f)) { err = "meshsnapshot: 'frame' sin numero"; return false; } frames.push_back(f); continue; }
        err = "meshsnapshot: argumento desconocido '" + t + "'"; return false;
    }
    std::vector<Mesh*> ms;
    PrCollectMeshes(SceneCollection, ms);
    PrSnapshot snapshot;
    long fields = 0;
    for (size_t i = 0; i < ms.size(); i++) {
        // la edicion de una malla de un RECURSO compartido queda pendiente al abrir (se lee al
        // pedirla): la foto con edicion compara LA MALLA, asi que primero se materializa
        if (!noEdit && ms[i]->mesh && !ms[i]->pendingEdit.empty()) W3dMeshBinMaterializeEdit(ms[i]);
        PrFields& c = snapshot[PrObjPath(ms[i])];
        PrSnapshotFields(ms[i], c, noEdit);
        PrSnapshotShapes(ms[i], c);
        for (size_t f = 0; f < frames.size(); f++) PrSnapshotFrame(ms[i], frames[f], c);
        fields += (long)c.size();
    }
    gPrSnapshots[name] = snapshot;
    printf("      [mallafoto] '%s': %d mallas, %ld campos%s\n", name.c_str(), (int)ms.size(), fields,
           noEdit ? " (sin los de edicion)" : "");
    if (ms.empty()) { err = "meshsnapshot: no hay ninguna malla en la escena"; return false; }
    return true;
}

// ---- meshsnapshotequal <a> <b> [sinedicion]: las dos fotos tienen las MISMAS mallas con
//      los MISMOS bytes en cada campo (con 'sinedicion' se ignoran los de edicion) ----
static bool CmdMeshSnapshotEqual(std::istringstream& ss, std::string& err) {
    std::string a, b, op; ss >> a >> b >> op;
    if (!gPrSnapshots.count(a) || !gPrSnapshots.count(b)) { err = "meshsnapshotequal: no existe la foto '" + (gPrSnapshots.count(a) ? b : a) + "'"; return false; }
    const PrSnapshot& A = gPrSnapshots[a];
    const PrSnapshot& B = gPrSnapshots[b];
    static const char* kEdit[] = { "faces3d", "loose", "cornerNormal", "uvMaps", "colorLayers", "marks", "edges", "posRep", "preservacion", 0 };
    const bool noEdit = (op == "noedit");
    std::vector<std::string> dif;
    for (PrSnapshot::const_iterator it = A.begin(); it != A.end(); ++it) {
        PrSnapshot::const_iterator o = B.find(it->first);
        if (o == B.end()) { dif.push_back("'" + it->first + "' falta en " + b); continue; }
        for (PrFields::const_iterator c = it->second.begin(); c != it->second.end(); ++c) {
            bool isEd = false;
            for (int k = 0; kEdit[k]; k++) if (c->first == kEdit[k]) isEd = true;
            if (noEdit && isEd) continue;
            PrFields::const_iterator d = o->second.find(c->first);
            if (d == o->second.end()) { dif.push_back("'" + it->first + "'." + c->first + " falta en " + b); continue; }
            if (!c->second.Equal(d->second)) {
                char buf[64]; sprintf(buf, " (%lu B contra %lu B)", c->second.n, d->second.n);
                dif.push_back("'" + it->first + "'." + c->first + buf);
            }
        }
        for (PrFields::const_iterator c = o->second.begin(); c != o->second.end(); ++c) {
            bool isEd = false;
            for (int k = 0; kEdit[k]; k++) if (c->first == kEdit[k]) isEd = true;
            if (noEdit && isEd) continue;
            if (!it->second.count(c->first)) dif.push_back("'" + it->first + "'." + c->first + " sobra en " + b);
        }
    }
    for (PrSnapshot::const_iterator it = B.begin(); it != B.end(); ++it)
        if (!A.count(it->first)) dif.push_back("'" + it->first + "' sobra en " + b);
    printf("      [mallafotoigual] %s vs %s: %d mallas, %d diferencia(s)\n", a.c_str(), b.c_str(), (int)A.size(), (int)dif.size());
    for (size_t i = 0; i < dif.size() && i < 16; i++) printf("      [mallafotoigual]   %s\n", dif[i].c_str());
    if (!dif.empty()) { err = "meshsnapshotequal: las mallas NO quedaron iguales: " + dif[0]; return false; }
    return true;
}

// ---- una malla suelta (fuera del arbol) para leer entradas en el harness ----
static Mesh* PrLooseMesh() {
    Mesh* m = new Mesh(SceneCollection, Vector3(0, 0, 0));
    Object* p = m->Parent;
    if (p) for (size_t i = 0; i < p->Childrens.size(); i++)
        if (p->Childrens[i] == (Object*)m) { p->Childrens.erase(p->Childrens.begin() + (long)i); break; }
    m->Parent = NULL;
    return m;
}

// ---- w3dbrender <ruta.w3d>: con ESE proyecto abierto, cada .w3db se lee de los tres
//      caminos y tienen que dar lo mismo: el del editor (todo), el del juego (solo render +
//      MaterializarEdicion desde el contenedor montado) y el de la malla de render sin Mesh
//      (W3dMeshBinReadRender, la base del recurso compartido). Ademas el modo "contrario"
//      (noEditable al reves de como se guardo) tiene que dar lo que da la carga clasica. ----
static bool CmdW3dbRender(std::istringstream& ss, std::string& err) {
    const std::string path = PrReadPath(ss);
    if (path.empty()) { err = "w3dbrender: uso: w3dbrender <ruta.w3d> (con ese proyecto abierto)"; return false; }
    std::vector<W3dZipEntry> bins; int nBin = 0, nTxt = 0;
    if (!PrEntriesMeshes(path, bins, nBin, nTxt)) { err = "w3dbrender: no pude leer " + path; return false; }
    if (bins.empty()) { err = "w3dbrender: " + path + " no tiene mallas .w3db"; return false; }
    int ok = 0, opposite = 0;
    for (size_t i = 0; i < bins.size(); i++) {
        const std::vector<unsigned char>& d = bins[i].data;
        const std::string& name = bins[i].name;
        { std::vector<unsigned char> mounted;
          if (!w3dFileSystem::ReadFileBytes(name, mounted) || mounted != d) { err = "w3dbrender: la entrada " + name + " no es la del contenedor montado (abri " + path + " antes)"; return false; } }
        W3dMeshRender R; W3dMeshInfo iR;
        if (!W3dMeshBinReadRender(&d[0], d.size(), R, &iR)) { err = "w3dbrender: LeerRender rechazo " + name; return false; }
        W3dMeshBinOptions op; op.noEditable = R.statsNoEditable;
        Mesh* A = PrLooseMesh(); W3dMeshInfo iA;
        Mesh* B = PrLooseMesh(); W3dMeshInfo iB;
        bool good = W3dMeshBinRead(&d[0], d.size(), A, &iA, op);
        A->noEditable = op.noEditable;   // (el flag lo pone el que carga, desde el proyecto)
        W3dMeshBinOptions opJ = op; opJ.edit = false;
        good = good && W3dMeshBinRead(&d[0], d.size(), B, &iB, opJ);
        std::string reason;
        if (!good) reason = "la lectura fallo";
        // el juego: sin edicion en memoria, y se materializa desde el contenedor
        if (reason.empty() && (!B->faces3d.empty() || !B->uvMaps.empty() || !B->edges.empty())) reason = "el camino del juego cargo datos de edicion";
        B->noEditable = op.noEditable;
        B->pendingEdit = name;
        // por la PUERTA REAL: el stack de modificadores (y el Boolean) arman sus poligonos con
        // BuildPolyMesh, que materializa lo pendiente la primera vez
        if (reason.empty()) {
            PolyMesh W;
            BuildPolyMesh(B, W);
            if (!B->pendingEdit.empty()) reason = "ConstruirPolyMesh no materializo la edicion pendiente";
            else if (B->faces3d.size() != A->faces3d.size()) reason = "MaterializarEdicion no trajo las caras";
            else if (W3dMeshBinMaterializeEdit(B)) reason = "MaterializarEdicion no es idempotente (volvio a leer)";
        }
        if (reason.empty()) {
            PrFields cA, cB; PrSnapshotFields(A, cA, false); PrSnapshotFields(B, cB, false);
            for (PrFields::iterator it = cA.begin(); it != cA.end() && reason.empty(); ++it)
                if (!cB.count(it->first) || !it->second.Equal(cB[it->first])) reason = "juego+materializar difiere en " + it->first;
        }
        // la malla de render sin Mesh contra la del editor
        if (reason.empty()) {
            const int nV = A->vertex ? A->vertexSize : 0;
            if (R.nRV != nV) reason = "LeerRender: otra cantidad de render-verts";
            else if (nV && (memcmp(&R.pos[0], A->vertex, (size_t)nV * 12) || memcmp(&R.nrm[0], A->normals, (size_t)nV * 3) ||
                            memcmp(&R.uv[0], A->uv, (size_t)nV * 8) || memcmp(&R.col[0], A->vertexColor, (size_t)nV * 4) ||
                            memcmp(&R.ctrl[0], &A->vertCtrlPoint[0], (size_t)nV * 4))) reason = "LeerRender: los arrays de render difieren";
            else if ((int)R.idx.size() != A->facesSize || (A->facesSize && memcmp(&R.idx[0], A->faces, (size_t)A->facesSize * sizeof(MeshIndex)))) reason = "LeerRender: el index buffer difiere";
            else if (R.parts.size() != A->materialsGroup.size()) reason = "LeerRender: otras mesh parts";
            else if (R.groups.size() != A->vertexGroups.size() || R.shapes.size() != A->shapeKeys.size() || R.uvGroups.size() != A->uvGroups.size()) reason = "LeerRender: otros grupos/formas";
            else if (R.Bytes() <= 0 && nV > 0) reason = "LeerRender: Bytes() no cuenta nada";
        }
        // el modo CONTRARIO: tiene que dar lo mismo que la carga clasica (ComputeAABBOnly o
        // ComputeBorders(true,false) con los puntos del archivo)
        if (reason.empty() && A->vertexSize > 0) {
            Mesh* C = PrLooseMesh(); W3dMeshInfo iC;
            W3dMeshBinOptions opC = op; opC.noEditable = !op.noEditable;
            if (!W3dMeshBinRead(&d[0], d.size(), C, &iC, opC)) reason = "la lectura en modo contrario fallo";
            else {
                A->noEditable = opC.noEditable;
                if (opC.noEditable) A->ComputeAABBOnly();
                else { A->posRep = A->vertCtrlPoint; A->ComputeBorders(true, false); }
                PrFields cA, cC; PrSnapshotFields(A, cA, false); PrSnapshotFields(C, cC, false);
                static const char* kLook[] = { "aabb", "radiusCenter", "edges", "posRep", 0 };
                for (int k = 0; kLook[k] && reason.empty(); k++)
                    if (!cA[kLook[k]].Equal(cC[kLook[k]])) reason = std::string("modemenu contrario difiere en ") + kLook[k];
                opposite++;
            }
            delete C;
        }
        delete A; delete B;
        if (!reason.empty()) { err = "w3dbrender: " + name + ": " + reason; return false; }
        ok++;
    }
    printf("      [w3dbrender] %d .w3db: editor == juego+materializar == render sin Mesh; modo contrario == carga clasica en %d\n", ok, opposite);
    return true;
}

// ---- un .w3db armado a mano: tabla de bloques (parse / re-emision) ----
struct PrBlock { std::string id; unsigned dom, count; std::string data; };
static unsigned PrU32(const unsigned char* p) { return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24); }
static void PrSetU32(std::string& s, unsigned v) { char b[4] = { (char)(v & 255u), (char)((v >> 8) & 255u), (char)((v >> 16) & 255u), (char)((v >> 24) & 255u) }; s.append(b, 4); }
static bool PrSplitW3db(const std::vector<unsigned char>& d, std::vector<PrBlock>& out) {
    out.clear();
    if (d.size() < 20) return false;
    const unsigned nb = PrU32(&d[12]);
    size_t off = 20;
    for (unsigned i = 0; i < nb; i++) {
        if (d.size() - off < 16) return false;
        PrBlock b;
        b.id.assign((const char*)&d[off], 4);
        b.dom = PrU32(&d[off + 4]); b.count = PrU32(&d[off + 8]);
        const unsigned by = PrU32(&d[off + 12]);
        if (by > d.size() - off - 16) return false;
        b.data.assign((const char*)&d[off + 16], by);
        out.push_back(b);
        off += 16 + ((by + 3u) & ~3u);
    }
    return true;
}
static std::vector<unsigned char> PrBuildW3db(const std::vector<PrBlock>& bl, unsigned version) {
    std::string s = "W3DB";
    PrSetU32(s, version); PrSetU32(s, 0); PrSetU32(s, (unsigned)bl.size()); PrSetU32(s, 0);
    for (size_t i = 0; i < bl.size(); i++) {
        s += bl[i].id; PrSetU32(s, bl[i].dom); PrSetU32(s, bl[i].count); PrSetU32(s, (unsigned)bl[i].data.size());
        s += bl[i].data;
        while (s.size() % 4) s += '\0';
    }
    return std::vector<unsigned char>(s.begin(), s.end());
}

// ---- w3dbcorrupt <ruta.w3d>: la ROBUSTEZ del lector sobre las .w3db del contenedor:
//      cortado en cada borde de bloque y en cientos de lugares, bytes cambiados al azar
//      (determinista), version mas nueva, un bloque que no conoce (se PRESERVA al
//      reescribir) y uno que no conoce pero esta en REQ (abre, y guardar encima se frena).
//      Un archivo roto NUNCA crashea ni deja la malla a medio llenar. ----
static bool CmdW3dbCorrupt(std::istringstream& ss, std::string& err) {
    const std::string path = PrReadPath(ss);
    if (path.empty()) { err = "w3dbcorrupt: uso: w3dbcorrupt <ruta.w3d>"; return false; }
    std::vector<W3dZipEntry> bins; int nBin = 0, nTxt = 0;
    if (!PrEntriesMeshes(path, bins, nBin, nTxt) || bins.empty()) { err = "w3dbcorrupto: " + path + " no tiene mallas .w3db"; return false; }
    // la malla testigo: tiene que quedar INTACTA cada vez que el lector rechaza un archivo
    Mesh* witness = PrLooseMesh();
    { W3dMeshInfo i0; W3dMeshBinOptions op;
      if (!W3dMeshBinRead(&bins[0].data[0], bins[0].data.size(), witness, &i0, op)) { delete witness; err = "w3dbcorrupt: la entrada sana no se lee"; return false; } }
    PrFields prev; PrSnapshotFields(witness, prev, false);
    int tested = 0, rejected = 0, accepted = 0, touched = 0;
    unsigned seed = 12345u;
    for (size_t e = 0; e < bins.size() && e < 6; e++) {
        const std::vector<unsigned char>& d = bins[e].data;
        std::vector<size_t> cuts;
        for (size_t c = 0; c < d.size(); c += (d.size() > 4000 ? d.size() / 400 : 4)) cuts.push_back(c);
        for (size_t k = 0; k < cuts.size(); k++) {
            std::vector<unsigned char> x(d.begin(), d.begin() + (long)cuts[k]);
            W3dMeshInfo inf; W3dMeshBinOptions op;
            const bool read = !x.empty() && W3dMeshBinRead(&x[0], x.size(), witness, &inf, op);
            tested++;
            if (read) { accepted++; W3dMeshBinRead(&bins[0].data[0], bins[0].data.size(), witness, &inf, op); }
            else {
                rejected++;
                PrFields now; PrSnapshotFields(witness, now, false);
                for (PrFields::iterator it = prev.begin(); it != prev.end(); ++it) if (!it->second.Equal(now[it->first])) { touched++; break; }
            }
        }
        for (int k = 0; k < 400; k++) {
            std::vector<unsigned char> x = d;
            for (int q = 0; q < 4; q++) {
                seed = seed * 1103515245u + 12345u;
                const size_t pos = (size_t)((seed >> 8) % (unsigned)x.size());
                seed = seed * 1103515245u + 12345u;
                x[pos] = (unsigned char)(seed >> 16);
            }
            W3dMeshInfo inf; W3dMeshBinOptions op;
            op.edit = (k % 2) == 0;
            tested++;
            if (W3dMeshBinRead(&x[0], x.size(), witness, &inf, op)) {
                accepted++;
                W3dMeshBinOptions sane; W3dMeshBinRead(&bins[0].data[0], bins[0].data.size(), witness, &inf, sane);
            } else {
                rejected++;
                PrFields now; PrSnapshotFields(witness, now, false);
                for (PrFields::iterator it = prev.begin(); it != prev.end(); ++it) if (!it->second.Equal(now[it->first])) { touched++; break; }
            }
        }
    }
    bool ok = (touched == 0);
    printf("      [w3dbcorrupto] %d archivos rotos: rechazados=%d aceptados=%d | malla tocada al rechazar=%d\n",
           tested, rejected, accepted, touched);
    // ---- version mas nueva: se niega y lo dice ----
    const std::vector<unsigned char>& d0 = bins[0].data;
    std::vector<PrBlock> bl;
    if (!PrSplitW3db(d0, bl)) { delete witness; err = "w3dbcorrupt: no pude partir la entrada en bloques"; return false; }
    { std::vector<unsigned char> x = PrBuildW3db(bl, 2);
      W3dMeshInfo inf; W3dMeshBinOptions op;
      const bool read = W3dMeshBinRead(&x[0], x.size(), witness, &inf, op);
      bool dice = false; for (size_t i = 0; i < inf.notices.size(); i++) if (inf.notices[i].find("mas nueva") != std::string::npos) dice = true;
      printf("      [w3dbcorrupto] version 2: %s, aviso 'mas nueva'=%s\n", read ? "LA LEYO <-- MAL" : "rejected", dice ? "si" : "NO");
      if (read || !dice) ok = false; }
    // ---- bloque desconocido NO requerido: se preserva al reescribir (y otra vez al releer) ----
    { std::vector<PrBlock> b2 = bl;
      PrBlock x; x.id = "XPRB"; x.dom = W3DB_GLOBAL; x.count = 3; x.data = "dato del futuro"; b2.push_back(x);
      std::vector<unsigned char> y = PrBuildW3db(b2, 1);
      Mesh* m = PrLooseMesh(); W3dMeshInfo inf; W3dMeshBinOptions op;
      const bool read = W3dMeshBinRead(&y[0], y.size(), m, &inf, op);
      std::string re; std::vector<std::string> av;
      const bool wrote = read && W3dMeshBinWrite(m, NULL, re, &av);
      Mesh* m2 = PrLooseMesh(); W3dMeshInfo inf2;
      const bool reread = wrote && W3dMeshBinRead((const unsigned char*)re.data(), re.size(), m2, &inf2, op);
      const bool travels = reread && m2->w3dmForeign.blocksBin.size() == 1 && m2->w3dmForeign.blocksBin[0].name == "XPRB" &&
                         m2->w3dmForeign.blocksBin[0].text == "dato del futuro" && m2->w3dmForeign.blocksBin[0].count == 3;
      printf("      [w3dbcorrupto] bloque desconocido XPRB: leyo=%s preservado al reescribir=%s\n", read ? "si" : "NO", travels ? "si" : "NO");
      if (!read || !travels) ok = false;
      // ...y el TEXTO no lo puede llevar: se pierde, pero avisando
      std::string txt; std::vector<std::string> avT;
      W3dMeshWrite(m2, txt, &avT);
      bool notifies = false; for (size_t i = 0; i < avT.size(); i++) if (avT[i].find("XPRB") != std::string::npos) notifies = true;
      printf("      [w3dbcorrupto] guardarlo en TEXTO avisa que XPRB se pierde: %s\n", notifies ? "si" : "NO");
      if (!notifies) ok = false;
      delete m; delete m2; }
    // ---- bloque desconocido REQUERIDO: abre en solo lectura y guardar encima se FRENA ----
    { std::vector<PrBlock> b2 = bl;
      for (size_t i = 0; i < b2.size(); i++) if (b2[i].id == "REQ ") { b2[i].data += "XREQ"; b2[i].count++; }
      PrBlock x; x.id = "XREQ"; x.dom = W3DB_GLOBAL; x.count = 1; x.data = "obligatorio"; b2.push_back(x);
      std::vector<unsigned char> y = PrBuildW3db(b2, 1);
      Mesh* m = PrLooseMesh(); W3dMeshInfo inf; W3dMeshBinOptions op;
      const bool read = W3dMeshBinRead(&y[0], y.size(), m, &inf, op);
      const std::string missing = W3dMeshMissingBlock(m);
      std::string re; std::vector<std::string> av;
      const bool wrote = W3dMeshBinWrite(m, NULL, re, &av);
      std::string txt; const bool wroteTxt = W3dMeshWrite(m, txt, &av);
      printf("      [w3dbcorrupto] REQ con XREQ: leyo=%s soloLectura=%s falta='%s' guardar binario=%s texto=%s\n",
             read ? "si" : "NO", inf.readOnly ? "si" : "NO", missing.c_str(), wrote ? "SE ESCRIBIO <-- MAL" : "frenado",
             wroteTxt ? "SE ESCRIBIO <-- MAL" : "frenado");
      if (!read || !inf.readOnly || missing != "XREQ" || wrote || wroteTxt) ok = false;
      delete m; }
    delete witness;
    if (!ok) { err = "w3dbcorrupt: el lector no se banca un archivo roto como deberia (ver arriba)"; return false; }
    return true;
}

// ---- w3dbbreak <origen.w3d> <destino.w3d> corte|version: copia el contenedor con
//      la PRIMERA .w3db rota (cortada a la mitad, o de una version del formato mas
//      nueva). Al abrirlo esa malla no carga y guardar encima se tiene que FRENAR
//      (W3dForeign::notLoaded), igual que con un .w3dm que no se pudo leer. ----
static bool CmdW3dbBreak(std::istringstream& ss, std::string& err) {
    const std::string orig = PrReadPath(ss), dest = PrReadPath(ss);
    std::string mode; ss >> mode;
    if (orig.empty() || dest.empty() || (mode != "cut" && mode != "version")) {
        err = "w3dbbreak: uso: w3dbbreak <origen.w3d> <destino.w3d> corte|version"; return false;
    }
    std::vector<W3dZipEntry> ents;
    if (!W3dZipRead(orig, &ents)) { err = "w3dbbreak: no pude leer " + orig; return false; }
    W3dZipWriter w;
    for (size_t i = 0; i < ents.size(); i++)   // mimetype PRIMERO (estilo ODF)
        if (ents[i].name == "mimetype") w.Add(ents[i].name, ents[i].data);
    std::string broken;
    for (size_t i = 0; i < ents.size(); i++) {
        const std::string& n = ents[i].name;
        if (n == "mimetype") continue;
        std::vector<unsigned char> d = ents[i].data;
        const bool isBin = n.size() > 5 && n.compare(n.size() - 5, 5, ".w3db") == 0;
        if (isBin && broken.empty() && d.size() > 24) {
            if (mode == "cut") d.resize(d.size() / 2);
            else { d[4] = 2; d[5] = d[6] = d[7] = 0; }
            broken = n;
        }
        w.Add(n, d);
    }
    if (broken.empty()) { err = "w3dbromper: " + orig + " no tiene ninguna .w3db"; return false; }
    if (!w.Save(dest)) { err = "w3dbbreak: no pude escribir " + dest; return false; }
    printf("      [w3dbromper] %s -> %s: '%s' %s\n", orig.c_str(), dest.c_str(), broken.c_str(),
           mode == "cut" ? "cortada a la mitad" : "con version 2 (del futuro)");
    return true;
}

// ---------------------------------------------------------------------------
//  mallarica [grande]: una escena NUEVA con una malla de cada caso que el formato
//  tiene que llevar entero: capas UV y de color (una por vertice), dos mesh parts con
//  material, shading por cara, sharp/seam, vertex groups + shape key + UV group +
//  reposo 2D, geometria suelta, un escenario noEditable, un rig con clip (skinning),
//  una vertex anim, un modificador Mirror y una tira de flipbook. Con 'grande', una
//  esfera de mas de 65535 render-verts (indices de 32 bits).
// ---------------------------------------------------------------------------
static Mesh* PrPrimitive(Object* parent, MeshType::Enum type, const char* name, int segs, int rings) {
    Mesh* m = (Mesh*)NewMesh(MeshType(type), parent, false);
    if (segs > 0) { m->meshVerts = segs; m->meshVerts2 = rings; m->Regenerate(); }
    m->SetNameObj(name);
    m->GenerateRender();   // el orden de render-verts CANONICO (el mismo merge que el lector de texto)
    return m;
}
static bool CmdMeshRich(std::istringstream& ss, std::string& err) {
    std::string op; ss >> op;
    RestartScene();
    PlayAnimation = false;
    Collection* col = new Collection(SceneCollection);
    col->SetNameObj("Rica");
    CollectionActive = col;
    Material* matA = new Material("MatRicaA");
    Material* matB = new Material("MatRicaB");
    matB->diffuse[0] = 0.2f;

    // ---- 1) la malla con TODO ----
    Mesh* m = PrPrimitive(col, MeshType::UVsphere, "Rica", 12, 6);
    m->PopulateLayers();
    const int nC = m->CountCorners();
    { UVMap* u2 = new UVMap("UV2"); u2->uv = m->uvMaps[0]->uv;
      for (size_t i = 0; i < u2->uv.size(); i++) u2->uv[i] = u2->uv[i] * 0.5f + 0.25f;
      m->uvMaps.push_back(u2); }
    { ColorLayer* c = new ColorLayer("Mascara"); c->perVertex = true; c->color.resize((size_t)nC * 4);
      for (int L = 0; L < nC; L++) { c->color[(size_t)L*4] = (GLubyte)(L * 7); c->color[(size_t)L*4+1] = (GLubyte)(255 - L * 3);
                                     c->color[(size_t)L*4+2] = (GLubyte)(L * 13); c->color[(size_t)L*4+3] = 255; }
      m->colorLayers.push_back(c); m->activeColor = (int)m->colorLayers.size() - 1; }
    // dos mesh parts con material + shading por cara
    m->materialsGroup[0].material = matA;
    { MaterialGroup g2; g2.name = "Parte2"; g2.material = matB; m->materialsGroup.push_back(g2); }
    for (size_t f = 0; f < m->faces3d.size(); f++) {
        if (f % 3 == 0) m->faces3d[f].mat = 1;
        m->faces3d[f].smooth = (f % 4 == 0) ? 0 : (f % 4 == 1 ? 1 : -1);
    }
    // marcas en aristas REALES (las del primer y el segundo poligono)
    { const MeshFace& F = m->faces3d[0];
      m->sharpEdges.insert(Mesh::SharpEdgeKey(&m->vertex[F.idx[0]*3], &m->vertex[F.idx[1]*3]));
      const MeshFace& G = m->faces3d[m->faces3d.size() / 2];   // un quad del medio (no el polo)
      m->seamEdges.insert(Mesh::SharpEdgeKey(&m->vertex[G.idx[0]*3], &m->vertex[G.idx[1]*3]));
      m->seamEdges.insert(Mesh::SharpEdgeKey(&m->vertex[G.idx[1]*3], &m->vertex[G.idx[2]*3])); }
    // pesos por control-point, una forma y un UV group
    WeightPaintEnsureMap(m);
    { VertexGroup* a = new VertexGroup("Hueso1"); VertexGroup* b = new VertexGroup("Hueso2");
      W3dShapeKey sk; sk.name = "Boca";
      std::vector<char> done(m->vertexSize, 0);
      for (int i = 0; i < m->vertexSize; i++) {
          const int cp = m->vertCtrlPoint[i];
          if (done[cp]) continue;
          done[cp] = 1;
          if (m->vertex[i*3+1] > 0.0f) { a->verts.push_back(cp); a->weights.push_back(0.25f + 0.5f * m->vertex[i*3+1]); }
          else { b->verts.push_back(cp); b->weights.push_back(0.8f); }
          if (m->vertex[i*3] > 0.3f) { sk.idx.push_back(cp); sk.d.push_back(0.1f); sk.d.push_back(0.0f); sk.d.push_back(-0.05f * m->vertex[i*3+2]); }
      }
      m->vertexGroups.push_back(a); m->vertexGroups.push_back(b); m->activeGroup = 1;
      m->shapeKeys.push_back(sk); m->shapeWeights.assign(1, 0.0f); }
    { UVGroup* ug = new UVGroup("Grupo2D");
      for (int i = 0; i < m->vertexSize; i += 3) { ug->verts.push_back(i); ug->weights.push_back(0.5f); }
      m->uvGroups.push_back(ug); m->uvActiveGroup = 0;
      m->uv2dRest.assign(m->uv, m->uv + (size_t)m->vertexSize * 2); }
    // geometria SUELTA al final del array (una arista y un vertice, lejos de todo)
    { const int n0 = m->vertexSize, n1 = n0 + 3;
      GLfloat* v = new GLfloat[(size_t)n1 * 3]; GLbyte* nn = new GLbyte[(size_t)n1 * 3];
      GLfloat* uv = new GLfloat[(size_t)n1 * 2]; GLubyte* cc = new GLubyte[(size_t)n1 * 4];
      memcpy(v, m->vertex, (size_t)n0 * 12); memcpy(nn, m->normals, (size_t)n0 * 3);
      memcpy(uv, m->uv, (size_t)n0 * 8); memcpy(cc, m->vertexColor, (size_t)n0 * 4);
      const float extra[9] = { 5, 0, 0,  6, 0, 0,  7, 1, 0 };
      for (int k = 0; k < 3; k++) {
          for (int q = 0; q < 3; q++) v[(size_t)(n0 + k) * 3 + q] = extra[k * 3 + q];
          nn[(size_t)(n0 + k) * 3] = 0; nn[(size_t)(n0 + k) * 3 + 1] = 127; nn[(size_t)(n0 + k) * 3 + 2] = 0;
          uv[(size_t)(n0 + k) * 2] = 0; uv[(size_t)(n0 + k) * 2 + 1] = 0;
          for (int q = 0; q < 4; q++) cc[(size_t)(n0 + k) * 4 + q] = 255;
          m->vertCtrlPoint.push_back(n0 + k);
          m->uv2dRest.push_back(0.0f); m->uv2dRest.push_back(0.0f);
      }
      delete[] m->vertex; delete[] m->normals; delete[] m->uv; delete[] m->vertexColor;
      m->vertex = v; m->normals = nn; m->uv = uv; m->vertexColor = cc; m->vertexSize = n1;
      m->looseEdges.push_back(n0); m->looseEdges.push_back(n0 + 1);
      m->looseVerts.push_back(n0 + 2); }
    m->RegroupMeshParts();
    m->posRep = m->vertCtrlPoint;
    m->ComputeBorders(true, false);
    m->ApplyLayersToRender();

    // ---- 2) escenario cerrado a edicion ----
    { Mesh* e = PrPrimitive(col, MeshType::cube, "Escenario", 0, 0);
      e->pos = Vector3(4, 0, 0); e->noEditable = true; e->ComputeAABBOnly(); }

    // ---- 3) un rig con clip + su malla skinneada ----
    { Armature* a = new Armature(col, Vector3(0, 0, 3)); a->name = "RigRico";
      { W3dBone r; r.name = "root";  r.parent = -1; r.head = Vector3(0,0,0); r.tail = Vector3(0,1,0); a->bones.push_back(r); }
      { W3dBone c; c.name = "child"; c.parent = 0;  c.head = Vector3(0,1,0); c.tail = Vector3(0,2,0); a->bones.push_back(c); }
      CreateAnimation(a);
      SkeletalAnimation* an = a->animations[a->activeAnim];
      an->name = "Saludo"; an->FrameRate = 24; an->startFrame = 1; an->endFrame = 20;
      { BoneTrack& tr = an->TrackOf(1);
        AnimProperty& ap = tr.PropertyOf(AnimRotation, AnimZ); SetKeyCurve(ap, 1, 0.0f); SetKeyCurve(ap, 10, 50.0f); SetKeyCurve(ap, 20, 0.0f); }
      PrepareAuthoredSkin(a);
      Mesh* s = PrPrimitive(col, MeshType::cylinder, "Cuerpo", 0, 0);
      s->pos = Vector3(0, 0, 3);
      WeightPaintEnsureMap(s);
      VertexGroup* vr = new VertexGroup("root"); VertexGroup* vc = new VertexGroup("child");
      std::vector<char> done(s->vertexSize, 0);
      for (int i = 0; i < s->vertexSize; i++) {
          const int cp = s->vertCtrlPoint[i];
          if (done[cp]) continue;
          done[cp] = 1;
          if (s->vertex[i*3+1] > 0.0f) { vc->verts.push_back(cp); vc->weights.push_back(1.0f); }
          else { vr->verts.push_back(cp); vr->weights.push_back(0.7f); }
      }
      s->vertexGroups.push_back(vr); s->vertexGroups.push_back(vc);
      Modifier* md = new Modifier(ModifierType::Armature, ModifierTypeName(ModifierType::Armature));
      md->target = (Object*)a; s->modifiers.push_back(md);
      s->skinArmature = a; }

    // ---- 4) una vertex anim de dos keyframes (posiciones + normales) ----
    { Mesh* v = PrPrimitive(col, MeshType::UVsphere, "Onda", 8, 4);
      v->pos = Vector3(-4, 0, 0);
      VertexAnimation* an = new VertexAnimation(v, "Latido", true);
      an->target = v; an->startFrame = 1; an->endFrame = 20; an->fps = 24;
      NewActiveVertexAnimation(v, an);
      const int nV = v->vertexSize;
      std::vector<GLfloat> p(v->vertex, v->vertex + (size_t)nV * 3);
      VertexAnimSetKey(*an, 1, &p[0], v->normals, NULL, 0);
      for (int i = 0; i < nV * 3; i++) p[(size_t)i] *= 1.3f;
      VertexAnimSetKey(*an, 10, &p[0], v->normals, NULL, 0); }

    // ---- 5) un modificador geometrico (Mirror): la malla generada se rehace al abrir ----
    { Mesh* e = PrPrimitive(col, MeshType::cone, "Espejo", 0, 0);
      e->pos = Vector3(0, 0, -4);
      Modifier* md = new Modifier(ModifierType::Mirror, ModifierTypeName(ModifierType::Mirror));
      e->modifiers.push_back(md); e->activeModifier = 0;
      e->GenerateMeshModified(); }

    // ---- 6) una tira de flipbook (el guardado deja las UV en su base) ----
    { Mesh* t = PrPrimitive(col, MeshType::plane, "Tira", 0, 0);
      t->pos = Vector3(0, 3, 0);
      t->SetUVAnimStrip(4, 8.0f, 0, 0); }

    // ---- 7) con 'grande': mas de 65535 render-verts (indices de 32 bits) ----
    if (op == "big") {
        Mesh* g = PrPrimitive(col, MeshType::UVsphere, "Grande", 300, 222);
        g->pos = Vector3(0, -6, 0);
        printf("      [mallarica] 'Grande': %d render-verts\n", g->vertexSize);
        if (g->vertexSize <= 65535) { err = "meshrich: la malla grande no pasa los 65535 render-verts"; return false; }
    }
    DeselectAll(); ActiveObj = NULL;
    std::vector<Mesh*> ms; PrCollectMeshes(SceneCollection, ms);
    printf("      [mallarica] %d mallas en 'Rica' ('Rica': %d render-verts, %d caras, %d capas UV, %d de color, "
           "%d grupos, %d formas, %d sueltas)\n", (int)ms.size(), m->vertexSize, (int)m->faces3d.size(),
           (int)m->uvMaps.size(), (int)m->colorLayers.size(), (int)m->vertexGroups.size(), (int)m->shapeKeys.size(),
           (int)(m->looseEdges.size() / 2 + m->looseVerts.size()));
    return true;
}

// ============================================================================
//  formatomallasui <malla>: el TILDE "Mesh as text" de la tarjeta del RECURSO (cada malla)
// ============================================================================
// El formato con que se guarda una malla es DE ESA MALLA (antes era una opcion de todo el
// proyecto en la tarjeta Archivo). Se elige la malla en la biblioteca (W3dResourceActivate, lo
// mismo que un click en el outliner por recursos) y se mira su tarjeta como en cada frame
// (Properties::UpdateTabs, pestania 3). 'clic' toca el tilde por la MISMA puerta que
// un click (EditPropertie: da vuelta el bool y llama al onChange); 'tilde 0|1' asserta lo que
// muestra y que coincide con el formato con que se va a guardar esa malla.
static bool CmdFormatMeshesUI(std::istringstream& ss, std::string& err) {
    Properties* P = PropsActive;
    if (!P) {   // el primer panel de propiedades del layout (el activo puede ser otro viewport)
        ViewportBase* h = PrFirstLeaf(3);
        P = h ? (Properties*)h : NULL;
    }
    std::string mesh; ss >> mesh;
    MeshResource* r = W3dMeshResourceByName(mesh);
    if (!P || !P->propRecMeshText) {
        err = "formatmeshesui: no hay panel de propiedades con el tilde del formato de la malla"; return false;
    }
    if (!r) { err = "formatmeshesui: no hay una malla '" + mesh + "'"; return false; }
    W3dResourceActivate(W3D_VIEW_MESHES, mesh);
    PropBool* pb = P->propRecMeshText;
    struct Show { static void Refresh(Properties* P) { P->activeTab = 3; P->UpdateTabs(); } };
    std::string t;
    while (ss >> t) {
        if (t == "click") {
            Show::Refresh(P);   // lo que la UI mostraba justo antes del click
            if (!pb->value) { err = "formatmeshesui: el tilde no se ve en la tarjeta de la malla"; return false; }
            pb->EditPropertie();
            printf("      [formatomallasui] %s: clic -> tilde=%d se guarda en %s\n", mesh.c_str(), *pb->value ? 1 : 0,
                   W3dMeshResourceInText(r) ? "text" : "binary");
        } else if (t == "check") {
            int want = -1; ss >> want;
            Show::Refresh(P);
            if (!pb->value) { err = "formatmeshesui: el tilde no se ve en la tarjeta de la malla"; return false; }
            const int has = *pb->value ? 1 : 0;
            const int fmt = W3dMeshResourceInText(r) ? 1 : 0;
            printf("      [formatomallasui] %s: tilde=%d se guarda en %s (entrada '%s')\n", mesh.c_str(), has,
                   fmt ? "text" : "binary", r->entry.c_str());
            if (has != fmt) { err = "formatmeshesui: el tilde no refleja el formato de la malla"; return false; }
            if (want != has) { char b[120]; sprintf(b, "formatmeshesui: tilde=%d y se esperaba %d", has, want); err = b; return false; }
        } else { err = "formatmeshesui: uso: formatmeshesui <malla> [clic] [tilde 0|1]..."; return false; }
    }
    return true;
}

// ============================================================================
//  zipcrc: el CRC32 del contenedor
// ============================================================================
// W3dZipCrc32 va de a 8 bytes ("slicing by 8"): se prueba contra los vectores de
// control del CRC-32 del zip y contra la version de siempre (bit a bit, la definicion)
// en TODOS los largos y alineaciones que ejercitan la vuelta de a 8 y la cola. Un CRC
// mal calculado seria consistente consigo mismo (el mismo modulo escribe y verifica):
// solo un vector externo lo delata, y un zip que otro programa no puede abrir.
static unsigned PrCrcBitABit(const unsigned char* d, size_t n) {
    unsigned c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) {
        c ^= d[i];
        for (int k = 0; k < 8; k++) c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
    }
    return c ^ 0xFFFFFFFFu;
}
static bool CmdZipCrc(std::string& err) {
    struct Vec { const char* s; unsigned crc; };
    static const Vec kVec[] = {
        { "", 0x00000000u },
        { "a", 0xE8B7BE43u },
        { "123456789", 0xCBF43926u },   // el "check" del CRC-32 del zip
        { "The quick brown fox jumps over the lazy dog", 0x414FA339u },
        { 0, 0 }
    };
    int bad = 0;
    for (int i = 0; kVec[i].s; i++) {
        const unsigned c = W3dZipCrc32((const unsigned char*)kVec[i].s, strlen(kVec[i].s));
        if (c != kVec[i].crc) { printf("      [zipcrc] '%s' -> %08X (se esperaba %08X)\n", kVec[i].s, c, kVec[i].crc); bad++; }
    }
    // datos pseudo-aleatorios deterministas (LCG): offsets 0..8 x largos 0..300, y 1 MB + 7
    std::vector<unsigned char> d((size_t)(1u << 20) + 7u);
    unsigned x = 12345u;
    for (size_t i = 0; i < d.size(); i++) { x = x * 1103515245u + 12345u; d[i] = (unsigned char)(x >> 16); }
    for (size_t off = 0; off <= 8; off++)
        for (size_t n = 0; n <= 300; n++)
            if (W3dZipCrc32(&d[off], n) != PrCrcBitABit(&d[off], n)) bad++;
    if (W3dZipCrc32(&d[3], d.size() - 3) != PrCrcBitABit(&d[3], d.size() - 3)) bad++;
    // velocidad (informativo): 16 pasadas de 1 MB
    const double t0 = W3dNowMs();
    unsigned acc = 0;
    for (int k = 0; k < 16; k++) acc = acc * 31u + W3dZipCrc32(&d[0], d.size());   // (que el compilador no la saltee)
    const double ms = W3dNowMs() - t0;
    printf("      [zipcrc] vectores de control + %d largos/alineaciones contra la definicion: %s | "
           "%.0f MB/s (acc %08X)\n", 9 * 301 + 1, bad ? "MAL" : "OK", ms > 0.0 ? 16.0 / (ms / 1000.0) : 0.0, acc);
    if (bad) { char b[80]; sprintf(b, "zipcrc: %d CRC distintos de la definicion", bad); err = b; return false; }
    return true;
}

// ============================================================================
//  w3dbregistro [contenedor.w3d]: el codigo, el registro y la spec dicen lo mismo
// ============================================================================
// Los fourcc que esta version entiende (W3dMeshBinBlocksKnown) tienen que ser
// EXACTAMENTE las filas "w3db:" vivas de formato/bloques.tsv, y cada uno tiene que estar
// documentado en formato/w3db.md. Con un contenedor, ademas, todo bloque de todas sus
// .w3db tiene que estar registrado. formato/ es gitignoreado: sin la carpeta se saltea
// avisando "SIN CORPUS:" (como las partes del corpus de oro; W3D_CORPUS_OBLIGATORIO=1
// la exige).
static bool PrReadText(const std::string& path, std::string& out) {
    std::vector<unsigned char> b;
    if (!w3dFileSystem::ReadFileBytes(path, b)) return false;
    out.assign(b.empty() ? "" : (const char*)&b[0], b.size());
    return true;
}
static bool CmdW3dbRegistry(std::istringstream& ss, std::string& err) {
    const std::string cont = PrReadPath(ss);
    static const char* kBases[] = { "format", "../formato", "../../formato", "../../../formato", "../../../../formato", 0 };
    std::string base;
    for (int i = 0; kBases[i] && base.empty(); i++)
        if (w3dFileSystem::FileExists(std::string(kBases[i]) + "/bloques.tsv")) base = kBases[i];
    if (base.empty()) {
        const char* obl = getenv("W3D_CORPUS_OBLIGATORIO");
        if (obl && *obl && strcmp(obl, "0") != 0) { err = "w3dbregistry: no encuentro formato/bloques.tsv"; return false; }
        printf("      [w3dbregistro] SIN CORPUS: salteo el cruce con formato/bloques.tsv y formato/w3db.md "
               "(formato/ no esta: es gitignoreado; W3D_CORPUS_OBLIGATORIO=1 lo exige)\n");
        return true;
    }
    std::string tsv, md;
    if (!PrReadText(base + "/bloques.tsv", tsv)) { err = "w3dbregistry: no pude leer " + base + "/bloques.tsv"; return false; }
    if (!PrReadText(base + "/w3db.md", md))      { err = "w3dbregistry: no pude leer " + base + "/w3db.md"; return false; }
    // las filas w3db: (el '_' final es el espacio de un fourcc de 3 letras)
    std::set<std::string> registered, obsolete;
    { size_t i = 0;
      while (i < tsv.size()) {
          size_t j = tsv.find('\n', i); if (j == std::string::npos) j = tsv.size();
          const std::string ln = tsv.substr(i, j - i);
          i = j + 1;
          if (ln.compare(0, 5, "w3db:") != 0) continue;
          std::vector<std::string> col;
          { size_t a = 0; while (true) { size_t tb = ln.find('\t', a); col.push_back(ln.substr(a, tb == std::string::npos ? std::string::npos : tb - a));
                                          if (tb == std::string::npos) break; a = tb + 1; } }
          std::string f = col[0].substr(5);
          for (size_t k = 0; k < f.size(); k++) if (f[k] == '_') f[k] = ' ';
          if (f.size() != 4 || col.size() < 6) { printf("      [w3dbregistro] fila rota: '%s'\n", ln.c_str()); err = "w3dbregistry: una fila w3db: no es <fourcc> + 5 columnas"; return false; }
          if (col[4] == "obsolete") obsolete.insert(f); else registered.insert(f);
      } }
    std::vector<std::string> known, required;
    W3dMeshBinBlocksKnown(known, &required);
    int bad = 0;
    for (size_t i = 0; i < known.size(); i++) {
        const std::string& f = known[i];
        if (!registered.count(f)) { printf("      [w3dbregistro] '%s' lo entiende el codigo y NO esta en bloques.tsv\n", f.c_str()); bad++; }
        if (md.find("`" + f + "`") == std::string::npos) { printf("      [w3dbregistro] '%s' no esta documentado en w3db.md\n", f.c_str()); bad++; }
    }
    for (std::set<std::string>::const_iterator it = registered.begin(); it != registered.end(); ++it)
        if (!W3dMeshBinBlockKnown(*it)) { printf("      [w3dbregistro] '%s' esta registrado vivo y el codigo no lo entiende\n", it->c_str()); bad++; }
    for (size_t i = 0; i < required.size(); i++)
        if (!W3dMeshBinBlockKnown(required[i])) { printf("      [w3dbregistro] el requerido '%s' no es un bloque conocido\n", required[i].c_str()); bad++; }
    // con un contenedor: todo bloque de sus .w3db esta registrado
    int nBin = 0, nTxt = 0, blocks = 0;
    if (!cont.empty()) {
        std::vector<W3dZipEntry> bins;
        if (!PrEntriesMeshes(cont, bins, nBin, nTxt)) { err = "w3dbregistry: no pude leer " + cont; return false; }
        for (size_t i = 0; i < bins.size(); i++) {
            std::vector<std::string> lines;
            if (bins[i].data.empty() || !W3dMeshBinList(&bins[i].data[0], bins[i].data.size(), lines)) continue;
            for (size_t k = 0; k < lines.size(); k++) {
                blocks++;
                const std::string f = lines[k].substr(0, 4);
                if (!registered.count(f)) { printf("      [w3dbregistro] %s escribe '%s', que no esta registrado\n", bins[i].name.c_str(), f.c_str()); bad++; }
            }
        }
    }
    printf("      [w3dbregistro] codigo=%d registrados=%d (obsoletos %d) requeridos=%d -> %s\n",
           (int)known.size(), (int)registered.size(), (int)obsolete.size(), (int)required.size(),
           bad ? "MAL" : "OK");
    if (!cont.empty()) printf("      [w3dbregistro] %s: %d .w3db, %d bloques mirados\n", cont.c_str(), nBin, blocks);
    if (!cont.empty() && nBin == 0) { err = "w3dbregistry: el contenedor no tiene ninguna .w3db para mirar"; return false; }
    if (bad) { err = "w3dbregistry: el codigo, formato/bloques.tsv y formato/w3db.md no coinciden (ver arriba)"; return false; }
    return true;
}

// ============================================================================
//  juego3dbool / juego3dboolpx: el TARGET de un Boolean en el juego compilado
// ============================================================================
// El runtime lee cada .w3db SOLO con los bloques de render. Una malla cuyo stack no genera
// malla queda con la edicion PENDIENTE (Mesh::pendingEdit) y BuildPolyMesh la
// materializa recien cuando alguien pide sus poligonos: el target de un Boolean. Si esa
// materializacion se rompe, el Boolean se queda sin poligonos del target y NO CORTA
// NADA, en silencio: el juego se ve "casi" bien. juego3dbool arma, sobre el juego 3D
// minimo (juego3dmin), un Boolean DIFFERENCE en el cubo con un target OCULTO que lo
// perfora de adelante hacia atras (arriba del centro, lejos de la franja del HUD), y
// verifica el agujero en el editor. juego3dboolpx corre el binario compilado y mira por
// el agujero: tiene que verse el FONDO y no la cara del cubo.
static bool CmdGame3DBool(std::istringstream& ss, std::string& err) {
    const std::string dir = PrReadPath(ss);
    if (dir.empty()) { err = "game3dbool: uso: game3dbool <carpeta>"; return false; }
    std::string e2;
    if (!W3dRunCommand("game3dmin " + dir + " light", e2)) { err = "game3dbool: " + e2; return false; }   // (compara pixeles)
    Object* oc = SceneCollection ? FindObjectByName(SceneCollection, "Cubo") : NULL;
    if (!oc || oc->getType() != ObjectType::mesh) { err = "game3dbool: juego3dmin no dejo el 'Cubo'"; return false; }
    Mesh* cube = (Mesh*)oc;
    // el TARGET: una caja que atraviesa el cubo (lado 4, centrado) de punta a punta en Z,
    // de 1,6 x 1,6 y corrida +1 en Y. OCULTA: no se dibuja, solo corta.
    Mesh* hole = (Mesh*)NewMesh(MeshType(MeshType::cube), cube->Parent, false);
    if (!hole) { err = "game3dbool: NewMesh fallo"; return false; }
    hole->SetNameObj("Hueco");
    hole->pos = Vector3(0.0f, 1.0f, 0.0f);
    hole->scale = Vector3(0.8f, 0.8f, 4.0f);
    hole->visible = false;
    { extern bool g_objectsMoved; g_objectsMoved = true; }
    // LOS TRES CUBOS SON LA MISMA MALLA del proyecto (un recurso, como duplicados vinculados):
    // en el juego el cubo que corta la materializa al abrir (su stack genera malla) y el target,
    // sin modificadores, la deja PENDIENTE hasta que el Boolean pide sus poligonos. (Tres mallas
    // nuevas iguales ya no se juntan solas al guardar: hay que vincularlas.)
    MeshResource* rec = W3dMeshCreateResource(cube, "Cubo");
    if (!rec || !W3dMeshLink(hole, rec)) { err = "game3dbool: no pude vincular el target a la malla del cubo"; return false; }
    Object* outside = FindObjectByName(SceneCollection, "CuboAfuera");
    if (outside && outside->getType() == ObjectType::mesh && !W3dMeshLink((Mesh*)outside, rec)) {
        err = "game3dbool: no pude vincular el testigo a la malla del cubo"; return false;
    }
    Modifier* md = new Modifier(ModifierType::Boolean, ModifierTypeName(ModifierType::Boolean));
    md->boolOp = 2;   // Difference
    md->target = hole;
    cube->modifiers.push_back(md);
    cube->activeModifier = (int)cube->modifiers.size() - 1;
    cube->GenerateMeshModified();
    // en el EDITOR ya hay agujero: la malla generada tiene vertices en su borde (espacio
    // LOCAL del cubo, escala 2: x = +-0,4 e y = 0,1 .. 0,9 en la cara de adelante z = 1)
    int borders = 0;
    if (cube->genValid && cube->genVertex)
        for (int i = 0; i < cube->genVertexSize; i++) {
            const float x = cube->genVertex[i * 3], y = cube->genVertex[i * 3 + 1], z = cube->genVertex[i * 3 + 2];
            if (fabsf(fabsf(x) - 0.4f) < 1e-3f && (fabsf(y - 0.1f) < 1e-3f || fabsf(y - 0.9f) < 1e-3f) && fabsf(z - 1.0f) < 1e-3f) borders++;
        }
    printf("      [juego3dbool] Boolean DIFFERENCE en 'Cubo' con target oculto 'Hueco': malla generada %s, "
           "%d render-verts, %d esquinas del agujero en la cara de adelante\n",
           cube->genValid ? "OK" : "NO", cube->genValid ? cube->genVertexSize : 0, borders);
    if (!cube->genValid || borders < 4) { err = "game3dbool: el Boolean no perforo el cubo en el editor"; return false; }
    DeselectAll(); ActiveObj = NULL;
    return true;
}

static bool CmdGame3DBoolPx(std::istringstream& ss, std::string& err) {
    std::string dir, name; ss >> dir >> name;
    if (dir.empty() || name.empty()) { err = "game3dboolpx: uso: game3dboolpx <carpeta> <nombre>"; return false; }
    const std::string bin = dir + "/build/linux/" + name;
    if (!w3dFileSystem::FileExists(bin)) { err = "game3dboolpx: no existe el binario compilado '" + bin + "'"; return false; }
    const std::string png = dir + "/captura.png";
    remove(png.c_str());
    char cmdRun[2200];
    snprintf(cmdRun, sizeof(cmdRun),
             "cd \"%s/build/linux\" && W3D_CAPTURE=\"%s\" W3D_CAPTURE_FRAME=20 timeout 120 ./%s > /dev/null 2>&1",
             dir.c_str(), png.c_str(), name.c_str());
    if (system(cmdRun) != 0) { err = "game3dboolpx: el juego no corrio"; return false; }
    unsigned char* px = 0; int iw = 0, ih = 0;
    if (!w3dEngine::DecodeImage(png.c_str(), &px, &iw, &ih) || !px || iw < 8 || ih < 8) {
        err = "game3dboolpx: el juego no dejo la captura (no dibujo ningun frame)"; return false;
    }
    // la captura viene top-left. Camara en z=8 mirando al origen, fov 45, encuadre 1:1 (la
    // ventana es 4:3 -> bandas a los costados). La cara de adelante del cubo cubre y 0,10..0,90.
    struct LP {
        static void Get(const unsigned char* p, int w, int h, float fx, float fy, int* out) {
            int x = (int)(fx * (float)w); if (x < 0) x = 0; if (x >= w) x = w - 1;
            int y = (int)(fy * (float)h); if (y < 0) y = 0; if (y >= h) y = h - 1;
            const unsigned char* q = p + ((size_t)y * w + x) * 4;
            out[0] = q[0]; out[1] = q[1]; out[2] = q[2];
        }
        static int Dif(const int* a, const int* b) { return abs(a[0] - b[0]) + abs(a[1] - b[1]) + abs(a[2] - b[2]); }
    };
    int background[3], hole[3], face[3];
    LP::Get(px, iw, ih, 0.50f, 0.04f, background);   // adentro del encuadre, arriba del cubo
    LP::Get(px, iw, ih, 0.50f, 0.30f, hole);         // mirando por el agujero (se ve de punta a punta)
    LP::Get(px, iw, ih, 0.35f, 0.30f, face);         // la misma altura, fuera del agujero: la cara del cubo
    w3dEngine::FreeImage(px);
    const bool okFace = LP::Dif(face, background) >= 40;   // el cubo se dibuja y se distingue del fondo
    const bool okHole = LP::Dif(hole, background) <= 24;   // por el agujero se ve el fondo
    printf("      [juego3dboolpx] %dx%d | fondo rgb=%d,%d,%d | agujero rgb=%d,%d,%d %s | cara rgb=%d,%d,%d %s\n",
           iw, ih, background[0], background[1], background[2],
           hole[0], hole[1], hole[2], okHole ? "OK" : "<-- MAL (el Boolean no corto: el target no tenia poligonos)",
           face[0], face[1], face[2], okFace ? "OK" : "<-- MAL (el cubo no se distingue del fondo)");
    if (!okFace || !okHole) {
        err = "game3dboolpx: el juego compilado no dibuja el agujero del Boolean (ver " + png + ")"; return false;
    }
    return true;
}

// ============================================================================
//  indices16 / w3dbjuego / w3dbmarcas / w3dbajenos: lo que la malla binaria no puede
//  hacer MAL en silencio (index buffer de 16 bits, edicion en el juego compilado,
//  claves de las marcas, preservacion del .w3dm de texto)
// ============================================================================

// ---- indices16 0|1: el GUARDADO de las mallas como en una plataforma de indices de 16 bits
//      (el N95). Ahi una malla de mas de 65535 render-verts tiene el index buffer TRUNCADO en
//      memoria y no entra en el .w3db (W3dMeshBinIndicesReach): sale en texto, que guarda
//      las caras con int. En el PC no hay nada truncado; lo que se prueba es la puerta del
//      guardado (WriteMeshW3dm). La negativa del escritor binario se prueba compilando
//      el Core con W3D_SYMBIAN. ----
static bool CmdIndices16(std::istringstream& ss, std::string& err) {
    int on = -1; ss >> on;
    if (on != 0 && on != 1) { err = "indices16: uso: indices16 0|1"; return false; }
    g_w3dIndices16Simulated = (on == 1);
    printf("      [indices16] guardado de mallas %s (MeshIndex de este build: %d bits)\n",
           on ? "COMO una plataforma de indices de 16 bits" : "normal", (int)sizeof(MeshIndex) * 8);
    return true;
}

// ---- w3dbgame <ruta.w3d>: la EDICION de las .w3db en el JUEGO COMPILADO ----
//      El juego lee cada .w3db SOLO con sus bloques de render. Los de edicion (caras, capas,
//      normales por esquina, marcas, aristas, posRep) quedan PENDIENTES y se leen recien si
//      el stack de la malla GENERA malla (al cargar, de los bytes que ya estan en memoria) o
//      si alguien pide sus poligonos (BuildPolyMesh: el Boolean que la usa de target).
//      Un stack que no genera nada (solo el PVS/Oclusion, o un modificador apagado en el
//      viewport) NO los carga: la malla mas grande de un nivel con PVS no puede entrar al
//      juego con toda su edicion en memoria sin que nadie la use.
//      Escena NUEVA con cuatro cubos (sin modificadores, solo Oclusion, un Mirror apagado y
//      un Mirror encendido), guardada en binario y abierta COMO EL JUEGO
//      (g_w3dMeshesAsGame): los tres primeros quedan pendientes y el cuarto trae su
//      edicion y su malla generada. Despues, lo pendiente se materializa por BuildPolyMesh
//      y guardar lo abierto asi materializa lo que falte: las mallas salen IDENTICAS
//      (<ruta>_2.w3d). Deja la escena por defecto (la abierta como el juego no es editable).
struct PrCaseGame { const char* name; int type; bool viewport; bool pending; };
static bool PrNoEdit(const Mesh* m) {
    return m->faces3d.empty() && m->looseEdges.empty() && m->cornerNormal.empty() && m->uvMaps.empty() &&
           m->colorLayers.empty() && m->sharpEdges.empty() && m->seamEdges.empty() && m->edges.empty() &&
           m->bordersBuf.empty() && m->posRep.empty();
}
static bool CmdW3dbGame(std::istringstream& ss, std::string& err) {
    const std::string path = PrReadPath(ss);
    if (path.size() < 5 || path.compare(path.size() - 4, 4, ".w3d") != 0) { err = "w3dbgame: uso: w3dbgame <ruta.w3d>"; return false; }
    const std::string path2 = path.substr(0, path.size() - 4) + "_2.w3d";
    static const PrCaseGame kCases[] = {
        { "Comun",         -1,                            true,  true  },
        { "SoloOclusion",  (int)ModifierType::CullingTri, true,  true  },
        { "EspejoApagado", (int)ModifierType::Mirror,     false, true  },
        { "Espejo",        (int)ModifierType::Mirror,     true,  false },
    };
    const int nCases = (int)(sizeof(kCases) / sizeof(kCases[0]));
    RestartScene();
    PlayAnimation = false;
    Collection* col = new Collection(SceneCollection);
    col->SetNameObj("Juego");
    CollectionActive = col;
    for (int i = 0; i < nCases; i++) {
        Mesh* m = PrPrimitive(col, MeshType::cube, kCases[i].name, 0, 0);
        m->pos = Vector3(3.0f * (float)i, 0.0f, 0.0f);
        if (kCases[i].type < 0) continue;
        Modifier* md = new Modifier(kCases[i].type, ModifierTypeName(kCases[i].type));
        md->showViewport = kCases[i].viewport;
        m->modifiers.push_back(md);
        m->activeModifier = 0;
        m->GenerateMeshModified();
    }
    DeselectAll(); ActiveObj = NULL;
    const int forcedPrev = g_w3dFormatForcedMeshes;
    g_w3dFormatForcedMeshes = W3D_MESHES_BINARY;
    const bool saved = SaveW3D(path);
    g_w3dFormatForcedMeshes = forcedPrev;
    if (!saved) { err = "w3dbgame: no pude guardar " + path; return false; }

    // ---- ABRIR COMO EL JUEGO ----
    g_w3dMeshesAsGame = true;
    OpenProjectNow(path);
    g_w3dMeshesAsGame = false;
    std::string reason;
    for (int i = 0; i < nCases && reason.empty(); i++) {
        Object* o = SceneCollection ? FindObjectByName(SceneCollection, kCases[i].name) : NULL;
        if (!o || o->getType() != ObjectType::mesh) { reason = std::string("'") + kCases[i].name + "' no volvio"; break; }
        Mesh* m = (Mesh*)o;
        const bool pend = !m->pendingEdit.empty();
        const char* stack = kCases[i].type < 0 ? "sin modificadores" :
                            kCases[i].type == (int)ModifierType::CullingTri ? "solo Oclusion" :
                            kCases[i].viewport ? "Mirror" : "Mirror apagado";
        printf("      [w3dbjuego] '%s' (%s): edicion %s | caras=%d capasUV=%d aristas=%d posRep=%d | "
               "render %d verts %d tris | generada=%s\n", kCases[i].name, stack, pend ? "PENDIENTE" : "en memoria",
               (int)m->faces3d.size(), (int)m->uvMaps.size(), (int)(m->edges.size() / 2), (int)m->posRep.size(),
               m->vertexSize, m->facesSize / 3, m->genValid ? "si" : "no");
        const std::string q = std::string("'") + kCases[i].name + "' (" + stack + "): ";
        if (m->vertexSize <= 0 || m->facesSize <= 0) reason = q + "no trajo su malla de render";
        else if (kCases[i].pending && (!pend || !PrNoEdit(m)))
            reason = q + "cargo sus datos de edicion y su stack no genera malla (en el juego nadie los usa)";
        else if (!kCases[i].pending && (pend || m->faces3d.size() != 6 || m->edges.empty() || (int)m->posRep.size() != m->vertexSize))
            reason = q + "su stack genera malla y la edicion no quedo en memoria";
        else if (!kCases[i].pending && !m->genValid) reason = q + "la malla generada del stack no se armo";
        else if (kCases[i].pending && m->genValid) reason = q + "armo una malla generada con un stack que no genera";
    }
    // lo pendiente sigue andando: el primer BuildPolyMesh (el Boolean que la usa de target) lo trae
    if (reason.empty()) {
        Mesh* m = (Mesh*)FindObjectByName(SceneCollection, "SoloOclusion");
        PolyMesh W;
        BuildPolyMesh(m, W);
        printf("      [w3dbjuego] 'SoloOclusion' tras ConstruirPolyMesh: caras=%d poligonos=%d pendiente=%s\n",
               (int)m->faces3d.size(), (int)W.F.size(), m->pendingEdit.empty() ? "no" : "SI");
        if (!m->pendingEdit.empty() || m->faces3d.size() != 6 || W.F.size() != 6)
            reason = "'SoloOclusion': ConstruirPolyMesh no materializo la edicion pendiente";
    }
    // guardar lo abierto como el juego: lo pendiente se materializa ANTES de escribir (sin
    // eso las mallas saldrian sin caras) y las mallas del contenedor salen identicas
    if (reason.empty()) {
        g_w3dFormatForcedMeshes = W3D_MESHES_BINARY;
        const bool g2 = SaveW3D(path2);
        g_w3dFormatForcedMeshes = forcedPrev;
        std::string e2;
        if (!g2) reason = "no pude guardar la escena abierta como el juego";
        else if (!W3dRunCommand("w3dequal " + path + " " + path2 + " meshes/", e2))
            reason = "guardar lo abierto como el juego no da las mismas mallas: " + e2;
    }
    // la escena abierta como el juego no es para editar: se cierra
    extern void W3dCreateDefaultScene();
    RestartScene();
    W3dCreateDefaultScene();
    if (!reason.empty()) { err = "w3dbjuego: " + reason; return false; }
    printf("      [w3dbjuego] solo el stack que genera malla carga la edicion; lo pendiente se materializa al pedirlo y al guardar\n");
    return true;
}

// ---- w3dbmarks <ruta.w3d>: las CLAVES de SHRP/SEAM de cada .w3db del contenedor ----
//      Cada clave son las dos posiciones de la arista, 3 f32 LITTLE-ENDIAN cada una, la
//      MENOR primero comparando esos bytes: el archivo es el mismo en cualquier host. En
//      memoria la clave (Mesh::SharpEdgeKey) ordena las mitades comparando los bytes DEL
//      HOST, que en uno big-endian dan otro orden: por eso el lector la rearma desde los
//      floats y no depende del orden de las mitades en el archivo. Una .w3db con TODAS las
//      mitades dadas vuelta (la que escribiria un host big-endian copiando su clave de
//      memoria) abre con las MISMAS marcas y se reescribe con los mismos bytes. ----
static bool CmdW3dbMarks(std::istringstream& ss, std::string& err) {
    const std::string path = PrReadPath(ss);
    if (path.empty()) { err = "w3dbmarks: uso: w3dbmarks <ruta.w3d>"; return false; }
    std::vector<W3dZipEntry> bins; int nBin = 0, nTxt = 0;
    if (!PrEntriesMeshes(path, bins, nBin, nTxt) || bins.empty()) { err = "w3dbmarcas: " + path + " no tiene mallas .w3db"; return false; }
    int withMarks = 0, keys = 0;
    for (size_t e = 0; e < bins.size(); e++) {
        const std::vector<unsigned char>& d = bins[e].data;
        const std::string& name = bins[e].name;
        std::vector<PrBlock> bl;
        if (!PrSplitW3db(d, bl)) { err = "w3dbmarks: no pude partir " + name; return false; }
        bool has = false;
        for (size_t k = 0; k < bl.size(); k++) if (bl[k].id == "SHRP" || bl[k].id == "SEAM") has = true;
        if (!has) continue;
        withMarks++;
        std::string reason;
        Mesh* A = PrLooseMesh(); W3dMeshInfo iA;
        Mesh* B = PrLooseMesh(); W3dMeshInfo iB;
        W3dMeshBinOptions op;
        if (!W3dMeshBinRead(&d[0], d.size(), A, &iA, op)) reason = "no se lee";
        // las posiciones de la malla en little-endian: cada mitad tiene que ser una de estas
        std::set<std::string> posLE;
        for (int i = 0; reason.empty() && i < A->vertexSize; i++) {
            std::string k;
            for (int q = 0; q < 3; q++) { unsigned u; memcpy(&u, &A->vertex[i * 3 + q], 4); PrSetU32(k, u); }
            posLE.insert(k);
        }
        std::vector<PrBlock> turn = bl;
        int distinct = 0;
        for (size_t k = 0; k < turn.size() && reason.empty(); k++) {
            PrBlock& b = turn[k];
            if (b.id != "SHRP" && b.id != "SEAM") continue;
            if (b.data.size() != (size_t)b.count * 24) { reason = b.id + " no mide 24 bytes por clave"; break; }
            for (unsigned c = 0; c < b.count; c++) {
                const std::string h1 = b.data.substr((size_t)c * 24, 12), h2 = b.data.substr((size_t)c * 24 + 12, 12);
                if (memcmp(h1.data(), h2.data(), 12) > 0) { reason = b.id + ": una clave trae la mitad MAYOR primero"; break; }
                if (!posLE.count(h1) || !posLE.count(h2)) { reason = b.id + ": una mitad no es la posicion (little-endian) de ningun vertice"; break; }
                if (h1 != h2) distinct++;
                b.data.replace((size_t)c * 24, 24, h2 + h1);   // las mitades dadas vuelta
                keys++;
            }
        }
        if (reason.empty() && distinct == 0) reason = "ninguna clave tiene dos mitades distintas para dar vuelta";
        // la version con las mitades dadas vuelta: las MISMAS marcas y los MISMOS bytes al reescribir
        if (reason.empty()) {
            const std::vector<unsigned char> d2 = PrBuildW3db(turn, W3DB_VERSION);
            std::string reA, reB;
            if (!W3dMeshBinRead(&d2[0], d2.size(), B, &iB, op)) reason = "con las mitades dadas vuelta no se lee";
            else if (B->sharpEdges != A->sharpEdges || B->seamEdges != A->seamEdges)
                reason = "el lector depende del orden de las mitades en el archivo (las claves no son las del host)";
            else if (!W3dMeshBinWrite(A, NULL, reA) || !W3dMeshBinWrite(B, NULL, reB)) reason = "no se reescribe";
            else if (reA != reB) reason = "reescrita desde las mitades dadas vuelta no da los mismos bytes";
            else {
                // y el reescrito trae las claves ORIGINALES (el escritor pone la menor primero)
                std::vector<unsigned char> r(reA.begin(), reA.end());
                std::vector<PrBlock> blR;
                if (!PrSplitW3db(r, blR)) reason = "no pude partir la reescrita";
                for (size_t k = 0; k < bl.size() && reason.empty(); k++) {
                    if (bl[k].id != "SHRP" && bl[k].id != "SEAM") continue;
                    bool equal = false;
                    for (size_t q = 0; q < blR.size(); q++) if (blR[q].id == bl[k].id) equal = (blR[q].data == bl[k].data);
                    if (!equal) reason = bl[k].id + " reescrito no trae las mismas claves";
                }
            }
        }
        printf("      [w3dbmarcas] %s: %d sharp + %d seam | mitades en orden y en little-endian, lector y escritor "
               "independientes del orden: %s\n", name.c_str(), (int)A->sharpEdges.size(), (int)A->seamEdges.size(),
               reason.empty() ? "OK" : reason.c_str());
        delete A; delete B;
        if (!reason.empty()) { err = "w3dbmarcas: " + name + ": " + reason; return false; }
    }
    if (withMarks == 0) { err = "w3dbmarks: ninguna .w3db de " + path + " tiene SHRP/SEAM para mirar"; return false; }
    printf("      [w3dbmarcas] %d .w3db con marcas, %d claves\n", withMarks, keys);
    return true;
}

// ---- w3dbforeign <prefijo>: la PRESERVACION del .w3dm de texto a traves del binario (AJEN) ----
//      Un .w3dm escrito por un Whisk3D mas nuevo trae bloques que esta version no entiende
//      (uno de cara y uno global) y un "requiere" que no es el default. Guardado en binario
//      (el default) viajan en el bloque AJEN; abierto y vuelto a guardar en texto, el .w3dm
//      sale IDENTICO al original. Y la regla del SELLO: si la topologia cambio, el bloque de
//      cara se DESCARTA avisando y el global y el "requiere" siguen. Escribe <prefijo>_t.w3d
//      (texto), <prefijo>_b.w3d (binario) y <prefijo>_t2.w3d (texto de nuevo). Deja la
//      escena por defecto. ----
static bool PrEntryMesh(const std::string& path, const std::string& ext, std::string& data) {
    std::vector<W3dZipEntry> ents;
    if (!W3dZipRead(path, &ents)) return false;
    for (size_t i = 0; i < ents.size(); i++) {
        const std::string& n = ents[i].name;
        if (n.compare(0, 7, "meshes/") != 0 || n.size() < ext.size() || n.compare(n.size() - ext.size(), ext.size(), ext) != 0) continue;
        data.assign(ents[i].data.begin(), ents[i].data.end());
        return true;
    }
    return false;
}
static bool CmdW3dbForeign(std::istringstream& ss, std::string& err) {
    const std::string pre = PrReadPath(ss);
    if (pre.empty()) { err = "w3dbforeign: uso: w3dbforeign <prefijo>"; return false; }
    const std::string rT = pre + "_t.w3d", rB = pre + "_b.w3d", rT2 = pre + "_t2.w3d";
    const int forcedPrev = g_w3dFormatForcedMeshes;
    RestartScene();
    PlayAnimation = false;
    Collection* col = new Collection(SceneCollection);
    col->SetNameObj("Ajenos");
    CollectionActive = col;
    // el .w3dm "del futuro": el de un cubo, con un "requiere" de bloques conocidos que no es el
    // default y dos bloques que esta version no conoce (uno de cara, uno global)
    Mesh* m = PrPrimitive(col, MeshType::cube, "Futuro", 0, 0);
    for (size_t g = 0; g < m->materialsGroup.size(); g++) m->materialsGroup[g].material = NULL;
    std::string txt;
    W3dMeshWrite(m, txt, 0);
    const std::string reqDef = "requires V F\n";
    const size_t pr = txt.find(reqDef);
    if (pr == std::string::npos) { err = "w3dbforeign: el .w3dm del cubo no trae 'requires V F'"; return false; }
    txt.replace(pr, reqDef.size(), "requires V F NRM UV\n");
    txt += "\n# bloques de una version mas nueva de Whisk3D\n";
    txt += "X.PC2.PLIEGUE 2 face peso=alto\n 0 0.25\n 1 0.75\nEND\n";
    txt += "X.PC2.NOTA 1 global\n hecho en 2029\nEND\n";
    { W3dMeshInfo inf;
      if (!W3dMeshRead(txt.data(), txt.size(), m, &inf) || inf.readOnly) { err = "w3dbforeign: no pude leer el .w3dm del futuro"; return false; }
      // el cierre de la carga clasica (lo que hace OpenSceneJson con un .w3dm)
      m->RegroupMeshParts();
      m->ComputeBorders(true, false);
      m->ApplyLayersToRender();
      WeightPaintEnsureMap(m); }
    if (m->w3dmForeign.blocks.size() != 2 || m->w3dmForeign.requiresValue.size() != 4) {
        err = "w3dbforeign: el lector de texto no preservo los bloques ajenos / el requiere"; return false;
    }
    DeselectAll(); ActiveObj = NULL;
    std::string reason, e0, eb, e2;
    // 1) texto: la referencia
    g_w3dFormatForcedMeshes = W3D_MESHES_TEXT;
    if (!SaveW3D(rT) || !PrEntryMesh(rT, ".w3dm", e0)) reason = "no pude guardar el texto de referencia";
    else if (e0.find("requires V F NRM UV\n") == std::string::npos || e0.find("X.PC2.PLIEGUE") == std::string::npos ||
             e0.find("X.PC2.NOTA") == std::string::npos) reason = "el .w3dm de referencia no trae el requiere o los bloques ajenos";
    // 2) binario: van en AJEN
    if (reason.empty()) {
        g_w3dFormatForcedMeshes = W3D_MESHES_BINARY;
        std::vector<std::string> lines;
        if (!SaveW3D(rB) || !PrEntryMesh(rB, ".w3db", eb)) reason = "no pude guardar el binario";
        else if (!W3dMeshBinList((const unsigned char*)eb.data(), eb.size(), lines)) reason = "la .w3db no se lista";
        else {
            bool foreign = false;
            for (size_t i = 0; i < lines.size(); i++) if (lines[i].compare(0, 5, "AJEN ") == 0) foreign = true;
            if (!foreign) reason = "la .w3db no trae el bloque AJEN";
        }
    }
    // 3) abrir el binario y volver a texto: el MISMO .w3dm
    Mesh* mb = NULL;
    if (reason.empty()) {
        OpenProjectNow(rB);
        Object* o = SceneCollection ? FindObjectByName(SceneCollection, "Futuro") : NULL;
        mb = (o && o->getType() == ObjectType::mesh) ? (Mesh*)o : NULL;
        // (la malla de un RECURSO abre con la edicion pendiente: los bloques ajenos de texto
        // viajan en la edicion, asi que se materializa como lo haria el editor al tocarla)
        if (mb && mb->mesh && !mb->pendingEdit.empty()) W3dMeshBinMaterializeEdit(mb);
        if (!mb) reason = "la malla no volvio del binario";
        else if (mb->w3dmForeign.blocks.size() != 2 || mb->w3dmForeign.requiresValue.size() != 4)
            reason = "el binario no devolvio los bloques ajenos / el requiere";
        else {
            g_w3dFormatForcedMeshes = W3D_MESHES_TEXT;
            if (!SaveW3D(rT2) || !PrEntryMesh(rT2, ".w3dm", e2)) reason = "no pude guardar el texto de vuelta";
            else if (e2 != e0) reason = "text -> binario -> texto no da el mismo .w3dm";
        }
    }
    printf("      [w3dbajenos] texto -> binario (AJEN, %d B) -> texto: %s (%d B)\n", (int)eb.size(),
           reason.empty() ? "el MISMO .w3dm" : reason.c_str(), (int)e2.size());
    // 4) el SELLO: sin una cara, el bloque de cara se descarta avisando; el global y el requiere siguen
    if (reason.empty()) {
        mb->faces3d.pop_back();
        W3dMeshBinIndices canon;
        W3dMeshCanonicalIndices(mb, canon);
        std::string re; std::vector<std::string> av;
        Mesh* c = PrLooseMesh(); W3dMeshInfo ic; W3dMeshBinOptions op;
        int avFold = 0;
        const bool wrote = W3dMeshBinWrite(mb, &canon, re, &av);
        for (size_t i = 0; i < av.size(); i++) if (av[i].find("X.PC2.PLIEGUE") != std::string::npos) avFold++;
        const bool read = wrote && W3dMeshBinRead((const unsigned char*)re.data(), re.size(), c, &ic, op);
        const bool remainsNote = read && c->w3dmForeign.blocks.size() == 1 && c->w3dmForeign.blocks[0].name == "X.PC2.NOTA" &&
                               c->w3dmForeign.requiresValue == mb->w3dmForeign.requiresValue;
        printf("      [w3dbajenos] sin una cara: escribio=%s aviso del bloque de cara=%d | vuelve solo el global + el requiere=%s\n",
               wrote ? "si" : "NO", avFold, remainsNote ? "si" : "NO");
        if (!wrote || avFold != 1 || !remainsNote) reason = "la regla del sello no se cumple en AJEN";
        delete c;
    }
    g_w3dFormatForcedMeshes = forcedPrev;
    extern void W3dCreateDefaultScene();
    RestartScene();
    W3dCreateDefaultScene();
    if (!reason.empty()) { err = "w3dbajenos: " + reason; return false; }
    return true;
}

// ============================================================================
//  EL DESPACHADOR (lo llama W3dRunCommand antes de su cadena de comandos)
// ============================================================================
bool W3dTestsMeshesCmd(const std::string& cmd, std::istringstream& ss, std::string& err, bool& handled);
bool W3dTestsOnlyGPUCmd(const std::string& cmd, std::istringstream& ss, std::string& err, bool& handled);
bool W3dTestsCallsCmd(const std::string& cmd, std::istringstream& ss, std::string& err, bool& handled);
bool W3dTestsResourcesCmd(const std::string& cmd, std::istringstream& ss,
                           std::string& err, bool& handled) {
    // los ANIMSETS (test/W3dTestsAnims.cpp) van primero: antes de CADA comando VERIFICAN
    // (sellos) que ningun clip compartido haya cambiado y, si cambio, el comando falla. No
    // copian ni reparan nada: el editor copia solo al escribir (no hay red por frame)
    { const bool r = W3dTestsAnimsCmd(cmd, ss, err, handled); if (handled) return r; }
    // las MALLAS COMO RECURSO tienen sus comandos en test/W3dTestsMeshes.cpp
    { bool mm = false; const bool r = W3dTestsMeshesCmd(cmd, ss, err, mm); if (mm) { handled = true; return r; } }
    // los del HITBOX viven en su propio archivo (test/W3dTestsHitbox.cpp)
    { const bool r = W3dTestsHitboxCmd(cmd, ss, err, handled); if (handled) return r; }
    // la prueba CRUZADA de las tres (test/W3dTestsIntegration.cpp)
    { const bool r = W3dTestsIntegrationCmd(cmd, ss, err, handled); if (handled) return r; }
    // el OUTLINER POR RECURSOS (vistas, carpetas, recurso activo: test/W3dTestsOutliner.cpp)
    { const bool r = W3dTestsOutlinerCmd(cmd, ss, err, handled); if (handled) return r; }
    // las CAPTURAS de los tutoriales (test/W3dCaptures.cpp, tools/capturas/)
    { const bool r = W3dCapturesCmd(cmd, ss, err, handled); if (handled) return r; }
    // las ESCENAS 3D y los PREFABS del proyecto (raices: test/W3dTestsScenes.cpp)
    { const bool r = W3dTestsScenesCmd(cmd, ss, err, handled); if (handled) return r; }
    // las INSTANCIAS DE PREFAB (test/W3dTestsPrefabs.cpp)
    { const bool r = W3dTestsPrefabsCmd(cmd, ss, err, handled); if (handled) return r; }
    // las LIBRERIAS EXTERNAS y los PROXIES W3D (test/W3dTestsLibs.cpp)
    { const bool r = W3dTestsLibsCmd(cmd, ss, err, handled); if (handled) return r; }
    // el STREAMING (test/W3dTestsStreaming.cpp)
    { const bool r = W3dTestsStreamingCmd(cmd, ss, err, handled); if (handled) return r; }
    // las MALLAS SOLO EN GPU (opcion de proyecto "mallasSoloGPU": test/W3dTestsGPUOnly.cpp)
    { const bool r = W3dTestsOnlyGPUCmd(cmd, ss, err, handled); if (handled) return r; }
    // MENOS LLAMADAS GL (la pila perezosa y el cache de punteros de gfx: test/W3dTestsCalls.cpp)
    { const bool r = W3dTestsCallsCmd(cmd, ss, err, handled); if (handled) return r; }
    handled = true;
    if (cmd == "loadbench")      return CmdLoadBench(ss, err);
    if (cmd == "scenesynthetic") return CmdSceneSynthetic(ss, err);
    if (cmd == "heapusage")         return CmdHeapUse(ss, err);
    if (cmd == "projectcaches")  return CmdCachesProject(ss, err);
    if (cmd == "scenedefault")   return CmdSceneDefault(ss, err);
    if (cmd == "nostackbuild")    return CmdNotStacksBuild(err);
    if (cmd == "nostack")         return CmdNotStacks(ss, err);
    if (cmd == "game2dmin")      return CmdGame2DMin(ss, err);
    if (cmd == "game2dpx")       return CmdGame2DPx(ss, err);
    if (cmd == "viewports")       return CmdViewports(ss, err);
    if (cmd == "layoutmaximize") return CmdLayoutMaximize(ss, err);
    if (cmd == "propsfocus")       return CmdPropsFocus(ss, err);
    if (cmd == "inputstate")      return CmdInputState(ss, err);
    if (cmd == "propsrename")     return CmdPropsRename(ss, err);
    if (cmd == "redopanel")       return CmdRedoPanel(ss, err);
    if (cmd == "propsarm2d")      return CmdPropsArm2D(ss, err);
    if (cmd == "iconspkg")       return CmdIconsPkg(err);
    if (cmd == "formatmeshes")   return CmdFormatMeshes(ss, err);
    if (cmd == "logloadmeshes")  return CmdLogLoadMeshes(ss, err);
    if (cmd == "w3dbinfo")        return CmdW3dbInfo(ss, err);
    if (cmd == "meshsnapshot")       return CmdMeshSnapshot(ss, err);
    if (cmd == "meshsnapshotequal")  return CmdMeshSnapshotEqual(ss, err);
    if (cmd == "w3dbrender")      return CmdW3dbRender(ss, err);
    if (cmd == "w3dbcorrupt")    return CmdW3dbCorrupt(ss, err);
    if (cmd == "meshrich")       return CmdMeshRich(ss, err);
    if (cmd == "w3dbbreak")      return CmdW3dbBreak(ss, err);
    if (cmd == "formatmeshesui") return CmdFormatMeshesUI(ss, err);
    if (cmd == "zipcrc")          return CmdZipCrc(err);
    if (cmd == "w3dbregistry")    return CmdW3dbRegistry(ss, err);
    if (cmd == "game3dbool")     return CmdGame3DBool(ss, err);
    if (cmd == "game3dboolpx")   return CmdGame3DBoolPx(ss, err);
    if (cmd == "indices16")       return CmdIndices16(ss, err);
    if (cmd == "w3dbgame")       return CmdW3dbGame(ss, err);
    if (cmd == "w3dbmarks")      return CmdW3dbMarks(ss, err);
    if (cmd == "w3dbforeign")      return CmdW3dbForeign(ss, err);
    handled = false;
    return false;
}
