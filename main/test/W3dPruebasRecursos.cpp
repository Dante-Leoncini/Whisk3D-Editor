// ============================================================================
//  W3dPruebasRecursos.cpp — comandos de harness de RECURSOS, CARGA y FORMATOS.
//  Ver W3dPruebasRecursos.h: W3dRunCommand (test/W3dScript.cpp) llama aca ANTES
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
//        la ultima; picoKB el pico (VmHWM) mas alto visto DURANTE una apertura.
//        heapKB = heap EN USO tras la primera apertura (glibc mallinfo2: sin lo que el
//        allocator retiene); fugaKB = heap que sobrevive a cada ciclo cerrar+abrir (el
//        RSS solo no alcanza para decir "fuga": glibc no devuelve todo al sistema).
//        Asserts opcionales (sobre la ULTIMA apertura), en cualquier orden:
//          mallas N | armatures N | clips N | objetos N | texturas N  (cantidades exactas;
//          texturas = las de CONTENIDO, sin las fijadas de la UI, como texinfo)
//          mallasbin N | derivadas N  (mallas que salieron de un .w3db sin derivar nada /
//          mallas que pagaron el cierre clasico al abrir: Reagrupar, Forsyth, CalcularBordes...)
//          maxms X                                                    (cota del total promedio)
//          fugakb X      (cota de fugaKB; pide n >= 3; fuera de glibc no se asserta)
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
//        primer uso). Tras cerrar el proyecto (ReiniciarEscena) tienen que ser 0.
//    juego2dmin <carpeta> / juego2dpx <carpeta> <nombre>
//        el juego 2D PURO minimo (HUD + script, nada 3D) y el chequeo por pixel del
//        binario compilado: "Compilar juego" lo arma solo con kFuentesBase.
//    escenadefecto
//        ReiniciarEscena + la escena de arrancar sin archivo (la del constructor).
//    viewports [hojas N]
//        viewports VIVOS en el proceso contra los nodos del arbol actual (el completo:
//        con uno maximizado, el guardado). Tienen que ser iguales: abrir un proyecto
//        libera el layout anterior. Ademas viewPortActive tiene que ser una hoja del
//        arbol, y Viewport3DActive/PropsActivo NULL o una hoja (nunca uno muerto).
//    layoutmaximizar <kind>
//        maximiza (LayoutMaximizar) la primera hoja de ese ViewportKind (1=3D 3=props).
//    propsfoco [nulo]
//        enfoca el campo Nombre del primer panel de propiedades (g_textFieldActivo);
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
//        los iconos del enum (IconoNombre) contra el skin y los .pkg de Symbian: cada uno
//        tiene su res/Skins/Whisk3D/atlas/iconos/<nombre>.png de 10x10 y una linea en los
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
//        + MaterializarEdicion) y por W3dMallaBinLeerRender da lo mismo; y el modo noEditable
//        contrario al guardado da lo mismo que la carga clasica.
//    w3dbcorrupto <ruta.w3d>
//        robustez del lector: cortes, bytes al azar, version nueva, bloque desconocido
//        (se preserva) y requerido (solo lectura + freno al guardar).
//    w3dbromper <origen.w3d> <destino.w3d> corte|version
//        copia el contenedor con su primera .w3db rota: al abrirlo esa malla no carga
//        y guardar encima se frena (noCargo), como con un .w3dm ilegible.
//    mallarica [grande]
//        escena NUEVA con una malla de cada caso del formato (capas, parts, marcas, grupos,
//        forma, UV group, sueltos, noEditable, rig con clip, vertex anim, Mirror, flipbook);
//        'grande' agrega una esfera de mas de 65535 render-verts.
//    formatomallasui [clic] [tilde 0|1]...
//        el tilde "Meshes as text" de la tarjeta Archivo: 'clic' lo toca como un
//        click (EditPropertie) y 'tilde' asserta lo que muestra (espejo de la opcion del proyecto).
//    zipcrc
//        el CRC32 del contenedor (de a 8 bytes) contra los vectores de control y contra la
//        definicion bit a bit en todos los largos/alineaciones; informa MB/s.
//    w3dbregistro [contenedor.w3d]
//        los fourcc que entiende el codigo = las filas "w3db:" vivas de formato/bloques.tsv, y
//        cada uno documentado en formato/w3db.md; con contenedor, todo bloque de sus .w3db esta
//        registrado. Sin formato/ (gitignoreado) se saltea con "SIN CORPUS:".
//    juego3dbool <carpeta> / juego3dboolpx <carpeta> <nombre>
//        el juego 3D minimo con un Boolean DIFFERENCE cuyo target (oculto) solo tiene la
//        edicion PENDIENTE en el runtime (se materializa por ConstruirPolyMesh), y el chequeo
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
//        Con ruta la guarda (GuardarW3D). Es la escena de "mil arboles iguales"
//        contra la que se mide la comparticion de mallas.
// ============================================================================
#include "test/W3dPruebasRecursos.h"
#include "test/W3dPruebasAnims.h"     // los comandos de los ANIMSETS (clips de esqueleto .w3da)
#include "test/W3dPruebasHitbox.h"      // los comandos del HITBOX (se despachan antes que los de aca, ver abajo)
#include "test/W3dPruebasIntegracion.h" // la prueba CRUZADA: mallas + animsets + hitbox en la misma escena
#include "test/W3dPruebasOutliner.h"   // el OUTLINER POR RECURSOS: vistas, carpetas, en uso / huerfano
#include "test/W3dPruebasEscenas.h"    // escenas 3D y prefabs (las raices del proyecto)
#include "test/W3dPruebasPrefabs.h"    // las INSTANCIAS de prefab (generar, scope, overrides, instanciar/destruir)
#include "test/W3dPruebasLibrerias.h"  // las LIBRERIAS externas y los PROXIES W3D
#include "test/W3dPruebasStreaming.h"  // el STREAMING de las instancias y proxies diferidos
#include "importers/import_w3d.h"      // g_w3dCargaFases / g_w3dCargaReloj: el reparto por fase
#include "importers/import_obj.h"      // CargarTodasTexturasPendientes
#include "config/W3dProfile.h"         // W3dNowMs: reloj de PARED (SDL, alta resolucion)
#include "objects/Objects.h"
#include "objects/Mesh.h"
#include "objects/Armature.h"
#include "objects/Materials.h"
#include "objects/Textures.h"
#include "objects/Camera.h"
#include "objects/Light.h"
#include "objects/Collection.h"
#include "io/W3dRecursos.h"
#include "io/Textura2D.h"             // Textura2DListar: el cache de imagenes 2D del proyecto
#include "w3dFilesystem.h"
#include "gfx/w3dGraphics.h"           // Finish: drenar la GPU (la subida de VBOs es perezosa)
#include "w3dTexture.h"                // juego2dpx: DecodeImage de la captura del binario
#include "ViewPorts/ViewPorts.h"       // rootViewport
#include "ViewPorts/LayoutArbol.h"     // viewports: ViewportsVivos / LayoutRaizCompleta
#include "ViewPorts/LayoutInput.h"     // layoutmaximizar: LayoutMaximizar / LayoutEstaMaximizado
#include "ViewPorts/ViewPort3D.h"      // viewports: Viewport3DActive
#include "ViewPorts/Properties.h"      // viewports/propsfoco: PropsActivo, el campo Nombre del proyecto
#include "ViewPorts/UVEditor.h"        // propsarm2d: el modo del editor UV (Edit Bones / Pose)
#include "WhiskUI/draw/icons.h"        // iconospkg: IconoNombre / ICON_TOTAL
#include "ViewPorts/PopUp/RedoMeshPanel.h" // redopanel: AbrirRedoMeshPanel / RedoMeshPanelActivo
#include "ViewPorts/PopUp/ProgressPopup.h" // LayoutSwapBuffers: la barra de progreso hace swaps
#include <SDL2/SDL.h>                  // SDL_GL_SetSwapInterval: vsync apagado mientras se mide
#include "objects/UI.h"                // juego2dmin: la UI raiz (lo que compila "Compilar juego")
#include "objects/Rect2D.h"            // juego2dmin: la franja que prende el script
#include "script/W3dScript.h"          // juego2dmin: W3dScriptDatos / W3dScriptEntrada
#include "io/W3dMalla.h"               // .w3dm: el escritor de texto (w3dbcorrupto: lo que el texto no puede llevar)
#include "io/W3dMallaBin.h"            // .w3db: la malla binaria (formato, equivalencia, robustez)
#include "io/W3dZip.h"                 // W3dZipLeer: las entradas de mallas de un .w3d
#include "io/GuardarW3D.h"             // indices16: g_w3dIndices16Simulado (el guardado como en el N95)
#include "io/MallasProyecto.h"         // escenasintetica: los N arboles son UNA malla (un recurso)
#include "objects/MallaRecurso.h"
#include "w3dlog.h"                    // logcargamallas: el ring del log (lo que va a whisk3d.log)
#include "edit/MeshEdit.h"             // mallarica: GenerarRender / Reagrupar / CalcularBordes
#include "edit/Modifier.h"             // mallarica: Armature + Mirror
#include "edit/PolyMesh.h"             // w3dbrender: ConstruirPolyMesh (la puerta que materializa la edicion)
#include "edit/WeightPaint.h"          // mallarica: WeightPaintAsegurarMapa
#include "animation/Animation.h"       // CurrentFrame / SetKeyCurva
#include "animation/SkeletalAnimation.h" // SkinearMesh / CrearAnimacion / PrepararSkinAutorado
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

extern void ReiniciarEscena();
extern void AbrirProyectoAhora(const std::string& ruta);
extern bool GuardarW3D(const std::string& ruta);

// ============================================================================
//  Helpers
// ============================================================================

// ruta como argumento: un token, o "entre comillas" si tiene espacios
static std::string PrLeerRuta(std::istringstream& ss) {
    std::string t;
    if (!(ss >> t)) return std::string();
    if (t.empty() || t[0] != '"') return t;
    std::string r = t.substr(1);
    while (r.empty() || r[r.size() - 1] != '"') {
        std::string mas;
        if (!(ss >> mas)) break;
        r += " " + mas;
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
    const long pag = sysconf(_SC_PAGESIZE);
    return (long)(resid * (unsigned long)(pag > 0 ? pag : 4096) / 1024u);
#else
    return -1;
#endif
}

// PICO de memoria residente (VmHWM) en KB. -1 = no se puede medir aca.
static long PrPicoKb() {
#if defined(__linux__)
    FILE* f = fopen("/proc/self/status", "r");
    if (!f) return -1;
    char linea[256];
    long kb = -1;
    while (fgets(linea, sizeof(linea), f))
        if (strncmp(linea, "VmHWM:", 6) == 0) { kb = atol(linea + 6); break; }
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
static void PrPicoReset() {
#if defined(__linux__)
    FILE* f = fopen("/proc/self/clear_refs", "w");
    if (!f) return;
    fputs("5", f);
    fclose(f);
#endif
}

// cantidades del arbol: objetos (sin la raiz), mallas, armatures y clips de esqueleto
struct PrConteo { int objetos, mallas, armatures, clips; };
static void PrContar(Object* o, PrConteo& c, bool esRaiz) {
    if (!o) return;
    if (!esRaiz) {
        c.objetos++;
        if (o->getType() == ObjectType::mesh) c.mallas++;
        else if (o->getType() == ObjectType::armature) {
            c.armatures++;
            c.clips += (int)((Armature*)o)->animations.size();
        }
    }
    for (size_t i = 0; i < o->Childrens.size(); i++) PrContar(o->Childrens[i], c, false);
}

// una apertura medida (todo en ms de pared)
struct PrMedida {
    double reinicio;   // cerrar el proyecto anterior (fuera del total)
    double abrir;      // AbrirProyectoAhora entero
    double pendientes; // texturas que quedaron en la cola diferida
    double render;     // PRIMER frame del layout + glFinish (sube VBOs/texturas perezosas)
    double frame2;     // el SEGUNDO frame (ya sin subidas): render - frame2 ~ costo de subida
    W3dCargaFases f;   // el reparto interno del lector
    long rssKB, picoKB;
    long heapKB;       // heap en uso con el proyecto abierto (tras el segundo frame)
    long heapCerradoKB;// heap en uso con el proyecto ANTERIOR ya cerrado (antes de abrir)
    PrMedida() : reinicio(0), abrir(0), pendientes(0), render(0), frame2(0), rssKB(-1), picoKB(-1),
                 heapKB(-1), heapCerradoKB(-1) {}
};

// LA LINEA PARSEABLE de cargabench: "[cargabench] total=<n> ms json=<n> mallas=<n> clips=<n>
// tex=<n> render=<n> rssKB=<n>" al PRINCIPIO y en ESE orden (lo que sigue es libre). La leen
// herramientas de afuera (la linea base de medidas, las comparaciones entre fases): cambiarla
// sin querer las rompe en silencio, asi que cargabench la valida antes de dar OK.
static bool PrLineaParseable(const std::string& l, std::string& motivo) {
    static const char* claves[7] = { "total=", "json=", "mallas=", "clips=", "tex=", "render=", "rssKB=" };
    const std::string pre = "[cargabench] ";
    if (l.compare(0, pre.size(), pre) != 0) { motivo = "no empieza con '[cargabench] '"; return false; }
    size_t pos = pre.size();
    for (int i = 0; i < 7; i++) {
        const size_t lc = strlen(claves[i]);
        if (l.compare(pos, lc, claves[i]) != 0) { motivo = std::string("falta '") + claves[i] + "' en su lugar"; return false; }
        pos += lc;
        const char* ini = l.c_str() + pos;
        char* fin = 0;
        strtod(ini, &fin);
        if (!fin || fin == ini) { motivo = std::string("'") + claves[i] + "' sin numero"; return false; }
        pos += (size_t)(fin - ini);
        if (i == 0) {
            if (l.compare(pos, 3, " ms") != 0) { motivo = "falta ' ms' despues de total"; return false; }
            pos += 3;
        }
        // separador: un espacio (o el final, despues de la ultima clave)
        if (pos < l.size()) {
            if (l[pos] != ' ') { motivo = std::string("basura despues de '") + claves[i] + "'"; return false; }
            pos++;
        } else if (i < 6) { motivo = "la linea termina antes de tiempo"; return false; }
    }
    return true;
}

// ============================================================================
//  cargabench
// ============================================================================
static bool CmdCargaBench(std::istringstream& ss, std::string& err) {
    const std::string ruta = PrLeerRuta(ss);
    if (ruta.empty()) { err = "cargabench: uso: cargabench <ruta.w3d> [n] [clave valor]..."; return false; }
    int n = 1;
    // asserts opcionales (clave valor); el primer token numerico suelto es n
    std::vector<std::string> claves; std::vector<double> valores;
    {
        std::string t;
        bool primero = true;
        while (ss >> t) {
            char* fin = 0;
            const double v = strtod(t.c_str(), &fin);
            if (primero && fin && *fin == '\0') { n = (int)v; primero = false; continue; }
            primero = false;
            std::string val;
            if (!(ss >> val)) { err = "cargabench: falta el valor de '" + t + "'"; return false; }
            fin = 0;
            const double vv = strtod(val.c_str(), &fin);
            if (!fin || *fin != '\0') { err = "cargabench: '" + val + "' no es un numero"; return false; }
            claves.push_back(t); valores.push_back(vv);
        }
    }
    if (n < 1) n = 1;
    if (!w3dFileSystem::FileExists(ruta)) { err = "cargabench: no existe " + ruta; return false; }
    // 'sinreloj 1' NO es un assert: abre sin el reloj del lector (el reparto no mide nada).
    // Existe para probar con 'fail' que el guardia de abajo lo detecta.
    bool sinReloj = false;
    for (size_t i = 0; i < claves.size(); ) {
        if (claves[i] == "sinreloj") {
            sinReloj = (valores[i] != 0.0);
            claves.erase(claves.begin() + i); valores.erase(valores.begin() + i);
        } else i++;
    }

    // ---- sin vsync y sin barra de progreso: la espera del vblank se comia ~80% ----
    //      de la apertura de un archivo chico (cada swap de la barra = 1 vblank)
    void (*swapPrev)() = LayoutSwapBuffers;
    LayoutSwapBuffers = NULL;
    SDL_GL_SetSwapInterval(0);
    g_w3dCargaReloj = sinReloj ? NULL : W3dNowMs;

    std::vector<PrMedida> med;
    PrConteo cnt; cnt.objetos = cnt.mallas = cnt.armatures = cnt.clips = 0;
    long rssBase = -1;
    for (int it = 0; it < n; it++) {
        PrMedida m;
        double t0 = W3dNowMs();
        ReiniciarEscena();                           // cerrar el anterior: se mide APARTE
        m.reinicio = W3dNowMs() - t0;
        m.heapCerradoKB = PrHeapKb();                // lo que sobrevive al cierre
        if (it == 0) rssBase = PrRssKb();            // el proceso sin proyecto abierto
        PrPicoReset();
        g_w3dCargaFases.Reset();
        W3dRecursosStatsReset(-1);
        t0 = W3dNowMs();
        AbrirProyectoAhora(ruta);
        m.abrir = W3dNowMs() - t0;
        t0 = W3dNowMs();
        CargarTodasTexturasPendientes();
        m.pendientes = W3dNowMs() - t0;
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
        m.f = g_w3dCargaFases;
        m.rssKB = PrRssKb();
        m.picoKB = PrPicoKb();
        m.heapKB = PrHeapKb();
        med.push_back(m);
        const double total = m.abrir + m.pendientes + m.render;
        printf("      [cargabench #%d] total=%.1f ms abrir=%.1f montaje=%.1f json=%.1f objetos=%.1f "
               "mallas=%.1f clips=%.1f animEscena=%.1f precarga=%.1f pendientes=%.1f render=%.1f "
               "frame2=%.1f reinicio=%.1f rssKB=%ld picoKB=%ld heapKB=%ld heapCerradoKB=%ld\n",
               it + 1, total, m.abrir, m.f.montajeMs, m.f.jsonMs, m.f.objetosMs, m.f.mallasMs,
               m.f.clipsMs, m.f.animEscenaMs, m.f.precargaMs, m.pendientes, m.render, m.frame2,
               m.reinicio, m.rssKB, m.picoKB, m.heapKB, m.heapCerradoKB);
    }

    g_w3dCargaReloj = NULL;
    SDL_GL_SetSwapInterval(1);                       // como arranca la app
    LayoutSwapBuffers = swapPrev;

    // ---- promedio de las n aperturas (la primera paga el cache frio del disco) ----
    PrMedida p;   // acumulador (arranca en cero)
    double minTotal = 1e30;
    long picoMax = -1;
    for (size_t i = 0; i < med.size(); i++) {
        const PrMedida& m = med[i];
        p.reinicio += m.reinicio; p.abrir += m.abrir; p.pendientes += m.pendientes;
        p.render += m.render; p.frame2 += m.frame2;
        p.f.montajeMs += m.f.montajeMs; p.f.jsonMs += m.f.jsonMs; p.f.objetosMs += m.f.objetosMs;
        p.f.mallasMs += m.f.mallasMs; p.f.clipsMs += m.f.clipsMs;
        p.f.mallaLeerMs += m.f.mallaLeerMs; p.f.mallaParseMs += m.f.mallaParseMs;
        p.f.mallaDerivadosMs += m.f.mallaDerivadosMs;
        p.f.animEscenaMs += m.f.animEscenaMs; p.f.precargaMs += m.f.precargaMs;
        const double total = m.abrir + m.pendientes + m.render;
        if (total < minTotal) minTotal = total;
        if (m.picoKB > picoMax) picoMax = m.picoKB;
    }
    const double k = 1.0 / (double)med.size();
    const PrMedida& ult = med.back();
    const double total = (p.abrir + p.pendientes + p.render) * k;
    // lo que el reparto no ve: reparacion de nombres, targets/constraints, layout, sesion...
    const double resto = (p.abrir - p.f.montajeMs - p.f.jsonMs - p.f.objetosMs - p.f.animEscenaMs
                          - p.f.precargaMs) * k;
    const double tex = (p.f.precargaMs + p.pendientes) * k;
    // FUGA POR REAPERTURA: cuanto heap sobrevive a cada ciclo cerrar+abrir del MISMO proyecto
    // (heap en uso tras cerrar, apertura a apertura). La primera vuelta se descarta si hay con
    // que (n >= 3): paga las inicializaciones perezosas de una sola vez (shaders, fuentes...).
    // -1 = no se puede medir (n < 2 o fuera de glibc).
    long fugaKB = -1;
    if (med.size() >= 2 && med[0].heapCerradoKB >= 0) {
        const size_t d = (med.size() >= 3) ? 1 : 0;
        fugaKB = (ult.heapCerradoKB - med[d].heapCerradoKB) / (long)(med.size() - 1 - d);
    }
    // la linea PARSEABLE: se arma, se valida su formato y recien ahi se imprime
    char linea[1024];
    snprintf(linea, sizeof(linea),
             "[cargabench] total=%.1f ms json=%.1f mallas=%.1f clips=%.1f tex=%.1f render=%.1f "
             "rssKB=%ld montaje=%.1f objetos=%.1f animEscena=%.1f precarga=%.1f pendientes=%.1f "
             "resto=%.1f frame2=%.1f reinicio=%.1f picoKB=%ld rssBaseKB=%ld rssUltKB=%ld "
             "heapKB=%ld fugaKB=%ld minTotal=%.1f n=%d",
             total, p.f.jsonMs * k, p.f.mallasMs * k, p.f.clipsMs * k, tex, p.render * k,
             med[0].rssKB, p.f.montajeMs * k, p.f.objetosMs * k, p.f.animEscenaMs * k,
             p.f.precargaMs * k, p.pendientes * k, resto, p.frame2 * k, p.reinicio * k,
             picoMax, rssBase, ult.rssKB, med[0].heapKB, fugaKB, minTotal, (int)med.size());
    printf("      %s\n", linea);
    { std::string motivo;
      if (!PrLineaParseable(linea, motivo)) { err = "cargabench: la linea parseable se rompio: " + motivo; return false; } }
    // de que esta hecha la fase de mallas: leer la entrada, parsear el texto, DERIVAR (index
    // buffer, bordes, capas, mapa de puntos) y el resto (transform, anims, modificadores...)
    printf("      [cargabench] desglose mallas: leer=%.1f parse=%.1f derivados=%.1f otros=%.1f ms | "
           "binarias=%d derivadas=%d (de %d)\n",
           p.f.mallaLeerMs * k, p.f.mallaParseMs * k, p.f.mallaDerivadosMs * k,
           (p.f.mallasMs - p.f.mallaLeerMs - p.f.mallaParseMs - p.f.mallaDerivadosMs) * k,
           ult.f.mallasBin, ult.f.mallasDerivadas, ult.f.mallas);

    // ---- cantidad de recursos (la ultima apertura) ----
    PrContar(SceneCollection, cnt, true);
    // texturas DE CONTENIDO: las vivas menos las fijadas de la UI (mismo criterio que texinfo)
    const int texturas = TexturasVivas() - TexturasBase();
    printf("      [cargabench] recursos objetos=%d mallas=%d armatures=%d clips=%d materiales=%d "
           "texturas=%d texturasUI=%d almacen.tex=%d almacen.malla=%d almacen.anim=%d almacen.vis=%d "
           "lector.mallas=%d lector.armatures=%d lector.clips=%d\n",
           cnt.objetos, cnt.mallas, cnt.armatures, cnt.clips, (int)Materials.size(), texturas,
           TexturasBase(),
           W3dRecursosVivos(W3DREC_TEXTURA), W3dRecursosVivos(W3DREC_MALLA),
           W3dRecursosVivos(W3DREC_ANIM), W3dRecursosVivos(W3DREC_LISTA_VIS),
           ult.f.mallas, ult.f.armatures, ult.f.clips);

    // ---- asserts ----
    for (size_t i = 0; i < claves.size(); i++) {
        const std::string& c = claves[i];
        const double v = valores[i];
        double tiene = 0.0;
        if      (c == "mallas")    tiene = cnt.mallas;
        else if (c == "armatures") tiene = cnt.armatures;
        else if (c == "clips")     tiene = cnt.clips;
        else if (c == "objetos")   tiene = cnt.objetos;
        else if (c == "texturas")  tiene = texturas;
        // de las mallas de la ULTIMA apertura: cuantas salieron de un .w3db (sin derivar nada)
        // y cuantas pagaron el cierre clasico (Reagrupar/Forsyth/CalcularBordes/capas)
        else if (c == "mallasbin") tiene = ult.f.mallasBin;
        else if (c == "derivadas") tiene = ult.f.mallasDerivadas;
        else if (c == "maxms") {
            if (total > v) {
                char b[160]; sprintf(b, "cargabench: total %.1f ms supera la cota %.1f ms", total, v);
                err = b; return false;
            }
            continue;
        }
        else if (c == "fugakb") {
            // sin medida (fuera de glibc) no se asserta: no es una falla del proyecto
            if (fugaKB < 0 && PrHeapKb() < 0) continue;
            if (med.size() < 3) { err = "cargabench: fugakb necesita n >= 3 (la primera vuelta se descarta)"; return false; }
            if ((double)fugaKB > v) {
                char b[200];
                sprintf(b, "cargabench: cada reapertura deja %ld KB de heap vivo (cota %.0f KB): algo del "
                           "proyecto no se libera al cerrarlo", fugaKB, v);
                err = b; return false;
            }
            continue;
        }
        else { err = "cargabench: assert desconocido '" + c + "'"; return false; }
        if ((long)tiene != (long)v) {
            char b[160]; sprintf(b, "cargabench: %s=%ld y se esperaba %ld", c.c_str(), (long)tiene, (long)v);
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
        if (!(ult.f.jsonMs > 0.0)) {
            sprintf(b, "cargabench: el reparto no midio la fase json (%.3f ms): el reloj del lector no "
                       "esta instalado o el lector dejo de acumular", ult.f.jsonMs);
            err = b; return false;
        }
        if (cnt.mallas > 0 && !(ult.f.mallasMs > 0.0)) {
            sprintf(b, "cargabench: el arbol tiene %d mallas y la fase mallas midio %.3f ms", cnt.mallas, ult.f.mallasMs);
            err = b; return false;
        }
        if (cnt.armatures > 0 && !(ult.f.clipsMs > 0.0)) {
            sprintf(b, "cargabench: el arbol tiene %d armatures y la fase clips midio %.3f ms", cnt.armatures, ult.f.clipsMs);
            err = b; return false;
        }
        if (ult.f.mallas != cnt.mallas || ult.f.armatures != cnt.armatures || ult.f.clips != cnt.clips) {
            sprintf(b, "cargabench: el reparto conto %d mallas/%d armatures/%d clips y el arbol tiene %d/%d/%d",
                    ult.f.mallas, ult.f.armatures, ult.f.clips, cnt.mallas, cnt.armatures, cnt.clips);
            err = b; return false;
        }
    }
    return true;
}

// ============================================================================
//  escenasintetica
// ============================================================================
static bool CmdEscenaSintetica(std::istringstream& ss, std::string& err) {
    int n = 0; ss >> n;
    if (n < 1) { err = "escenasintetica: uso: escenasintetica <n> [ruta.w3d] [segmentos anillos]"; return false; }
    const std::string ruta = PrLeerRuta(ss);
    int seg = 32, ani = 16;
    { int a = 0, b = 0; if (ss >> a >> b) { if (a >= 3) seg = a; if (b >= 3) ani = b; } }

    ReiniciarEscena();
    Collection* col = new Collection(SceneCollection);
    col->SetNameObj("Bosque");
    CollectionActive = col;
    const int lado = (int)ceil(sqrt((double)n));
    const float sep = 3.0f;                         // la esfera tiene radio 1: 1 m de aire entre copas
    const float ext = (lado - 1) * sep;
    // camara de la escena mirando la grilla en diagonal desde arriba (como la default del editor)
    new Camera(col, Vector3(-ext * 0.5f - 6.0f, ext * 0.4f + 5.0f, ext * 0.5f + 6.0f),
               Vector3(-35.0f, -45.0f, 0.0f));
    Light* L = Light::Create(col, 1.0f, ext * 0.5f + 4.0f, 2.25f);
    L->SetDiffuse(1, 1, 1);
    // LOS N ARBOLES SON LA MISMA MALLA (un recurso, como N duplicados vinculados): el primero la
    // crea y los demas se vinculan a ella. Antes eran N mallas sueltas iguales que el guardado
    // juntaba por contenido; eso ahora es solo para las sueltas de un ARCHIVO viejo (dos mallas
    // nuevas iguales no se vinculan solas: Mesh::dedupPorContenido).
    MallaRecurso* rec = NULL;
    for (int i = 0; i < n; i++) {
        Mesh* m = (Mesh*)NewMesh(MeshType::UVsphere, col, false);
        if (!m) { err = "escenasintetica: NewMesh devolvio NULL"; return false; }
        if (!rec) {
            m->meshVerts = seg;
            m->meshVerts2 = ani;
            m->Regenerar();
        }
        m->SetNameObj("Arbol");
        m->pos = Vector3((i % lado) * sep - ext * 0.5f, 1.0f, (i / lado) * sep - ext * 0.5f);
        if (!rec) {
            rec = W3dMallaCrearRecurso(m, "Arbol");
            if (!rec) { err = "escenasintetica: no se pudo crear la malla 'Arbol'"; return false; }
        } else if (!W3dMallaVincular(m, rec)) { err = "escenasintetica: no se pudo vincular a la malla 'Arbol'"; return false; }
    }
    DeseleccionarTodo();
    ObjActivo = NULL;
    Mesh* m0 = NULL;
    for (size_t i = 0; i < col->Childrens.size() && !m0; i++)
        if (col->Childrens[i]->getType() == ObjectType::mesh) m0 = (Mesh*)col->Childrens[i];
    printf("      [escenasintetica] %d mallas iguales (esfera %dx%d: %d render-verts, %d tris cada una)\n",
           n, seg, ani, m0 ? m0->vertexSize : 0, m0 ? m0->facesSize / 3 : 0);
    if (!ruta.empty()) {
        const bool ok = GuardarW3D(ruta);
        printf("      [escenasintetica] guardada en %s -> %s\n", ruta.c_str(), ok ? "OK" : "FALLO");
        if (!ok) { err = "escenasintetica: no pude guardar " + ruta; return false; }
    }
    return true;
}

// ============================================================================
//  heapuso
// ============================================================================
// heap EN USO y cuanto cambio desde el heapuso anterior del mismo .w3s: la forma de
// asertar "hacer y deshacer N veces no deja memoria viva" (crear/borrar objetos,
// instanciar/destruir, cargar/descargar). 'max X' falla si el delta supera X KB.
static long gHeapUsoPrev = -1;
static bool CmdHeapUso(std::istringstream& ss, std::string& err) {
    const long kb = PrHeapKb();
    const long delta = (gHeapUsoPrev >= 0 && kb >= 0) ? kb - gHeapUsoPrev : 0;
    printf("      [heapuso] heap en uso=%ld KB delta=%+ld KB%s\n", kb, delta,
           kb < 0 ? " (no se puede medir en esta plataforma)" : (gHeapUsoPrev < 0 ? " (primera medida)" : ""));
    gHeapUsoPrev = kb;
    std::string clave;
    while (ss >> clave) {
        double v = 0.0;
        if (!(ss >> v)) { err = "heapuso: falta el valor de '" + clave + "'"; return false; }
        if (clave != "max") { err = "heapuso: assert desconocido '" + clave + "' (uso: heapuso [max KB])"; return false; }
        if (kb >= 0 && (double)delta > v) {
            char b[160]; sprintf(b, "heapuso: el heap crecio %ld KB desde la medida anterior (cota %.0f KB)", delta, v);
            err = b; return false;
        }
    }
    return true;
}

// ============================================================================
//  cachesproyecto
// ============================================================================
// lo que el proyecto ABIERTO dejo en los caches del lado del juego (se llenan en la
// precarga de AbrirW3D y en el primer uso). Al cerrar el proyecto (ReiniciarEscena)
// tienen que quedar en 0: si no, el audio/las imagenes del cerrado siguen vivos y otro
// proyecto con una entrada del mismo nombre usaria lo del anterior.
static bool CmdCachesProyecto(std::istringstream& ss, std::string& err) {
    extern int W3dSonidosCacheados();
    std::vector<std::string> tex2d;
    Textura2DListar(tex2d);
    const int nSon = W3dSonidosCacheados(), nTex = (int)tex2d.size();
    printf("      [cachesproyecto] sonidos=%d tex2d=%d\n", nSon, nTex);
    std::string clave;
    while (ss >> clave) {
        std::string op; int v = 0;
        if (!(ss >> op)) { err = "cachesproyecto: falta el valor de '" + clave + "'"; return false; }
        // "N" exacto o ">=N" (cuantos carga la precarga depende del mixer: sin audio cachea NULL igual)
        const bool alMenos = (op.size() > 2 && op[0] == '>' && op[1] == '=');
        v = atoi(alMenos ? op.c_str() + 2 : op.c_str());
        int tiene = 0;
        if (clave == "sonidos") tiene = nSon;
        else if (clave == "tex2d") tiene = nTex;
        else { err = "cachesproyecto: assert desconocido '" + clave + "' (sonidos|tex2d)"; return false; }
        if (alMenos ? (tiene < v) : (tiene != v)) {
            char b[160]; sprintf(b, "cachesproyecto: %s=%d y se esperaba %s", clave.c_str(), tiene, op.c_str());
            err = b; return false;
        }
    }
    return true;
}

// ============================================================================
//  juego2dmin / juego2dpx
// ============================================================================
// el JUEGO 2D PURO minimo (sin un solo objeto 3D): "Compilar juego" lo arma con
// kFuentesBase SOLO (sin kFuentes3D), que es justo lo que no probaba nadie: el
// juego 3D (prueba_juego3d) linkea con import_w3d.cpp adentro y tapa cualquier
// simbolo de ahi que el codigo compartido 2D nombre (BindsJuego -> importarW3D).
// La franja nace OCULTA y la prende el .lua solo si los binds 2D existen en el
// binario (importarW3D incluido): juego2dpx la busca por pixel en la captura.
static bool PrCrearCarpeta(const std::string& dir) {
#ifdef _WIN32
    _mkdir(dir.c_str());
#else
    mkdir(dir.c_str(), 0755);
#endif
    struct stat st;
    return stat(dir.c_str(), &st) == 0;
}

static bool CmdJuego2DMin(std::istringstream& ss, std::string& err) {
    const std::string dir = PrLeerRuta(ss);
    if (dir.empty()) { err = "juego2dmin: uso: juego2dmin <carpeta>"; return false; }
    if (!PrCrearCarpeta(dir)) { err = "juego2dmin: no pude crear " + dir; return false; }
    { FILE* f = fopen((dir + "/juego2dmin.lua").c_str(), "w");
      if (!f) { err = "juego2dmin: no pude escribir el .lua"; return false; }
      fputs("-- juego 2D minimo (prueba del runtime compilado SIN 3D)\n"
            "function inicio() end\n"
            "function actualizar(dt)\n"
            "  local w, h = pantalla()                 -- set 2D compartido\n"
            "  local s = sonido(\"no-existe.wav\")       -- nil: no hay wav, pero el bind existe\n"
            "  local imp = (type(importarW3D) == \"function\") -- vive en el lector 3D\n"
            "  if w and w > 0 and imp then mostrar(objeto(\"franja\"), true) end\n"
            "end\n", f);
      fclose(f); }
    ReiniciarEscena();
    UI* ui = new UI(NULL, Vector3(0, 0, 0));
    ui->name = "HUD";
    ui->igualQueRender = true;
    if (!ui->scriptDatos) ui->scriptDatos = new W3dScriptDatos();
    { W3dScriptEntrada e; e.ruta = "juego2dmin.lua";
      e.refs.push_back(std::make_pair(std::string("franja"), std::string("Franja")));
      ui->scriptDatos->scripts.push_back(e); }
    Rect2D* franja = new Rect2D(ui, Vector3(0.0f, 0.0f, 0.0f));   // centrada
    franja->name = "Franja";
    franja->ancho = 200.0f; franja->alto = 60.0f;
    franja->color[0] = 0.0f; franja->color[1] = 1.0f; franja->color[2] = 0.0f; franja->color[3] = 1.0f;
    franja->visible = false;   // la prende el script (solo si los binds existen en el binario)
    DeseleccionarTodo(); ObjActivo = NULL;
    printf("      [juego2dmin] HUD con script + franja oculta (sin 3D) en '%s'\n", dir.c_str());
    return true;
}

static bool CmdJuego2DPx(std::istringstream& ss, std::string& err) {
    std::string dir, nombre; ss >> dir >> nombre;
    if (dir.empty() || nombre.empty()) { err = "juego2dpx: uso: juego2dpx <carpeta> <nombre>"; return false; }
    const std::string bin = dir + "/build/linux/" + nombre;
    if (!w3dFileSystem::FileExists(bin)) { err = "juego2dpx: no existe el binario compilado '" + bin + "'"; return false; }
    const std::string png = dir + "/captura.png";
    remove(png.c_str());
    char cmdRun[2200];
    snprintf(cmdRun, sizeof(cmdRun),
             "cd \"%s/build/linux\" && W3D_CAPTURA=\"%s\" W3D_CAPTURA_FRAME=20 timeout 120 ./%s > /dev/null 2>&1",
             dir.c_str(), png.c_str(), nombre.c_str());
    if (system(cmdRun) != 0) { err = "juego2dpx: el juego no corrio"; return false; }
    unsigned char* px = 0; int iw = 0, ih = 0;
    if (!w3dEngine::DecodeImage(png.c_str(), &px, &iw, &ih) || !px || iw < 8 || ih < 8) {
        err = "juego2dpx: el juego no dejo la captura (no dibujo ningun frame)"; return false;
    }
    const unsigned char* c = px + ((size_t)(ih / 2) * iw + (size_t)(iw / 2)) * 4;   // top-left, sin flip
    const bool okHud = (c[1] > 150 && c[0] < 120 && c[2] < 120);
    printf("      [juego2dpx] %dx%d | centro rgb=%d,%d,%d %s\n", iw, ih, c[0], c[1], c[2],
           okHud ? "OK" : "<-- MAL (la franja no aparecio: al binario le falta un bind 2D)");
    w3dEngine::FreeImage(px);
    if (!okHud) { err = "juego2dpx: el juego 2D compilado no corre su script (ver " + png + ")"; return false; }
    return true;
}

// ============================================================================
//  escenadefecto
// ============================================================================
// cierra lo que haya y arma la escena de ARRANCAR SIN ARCHIVO (la misma funcion que
// usa el arranque). Los tests que trabajan "sobre la escena del usuario" (la coleccion
// activa no es la raiz: w3dmf2, w3dmf10, *marcas) la necesitan para poder correr en un
// mismo proceso despues de otros tests.
static bool CmdEscenaDefecto(std::string& err) {
    extern void W3dCrearEscenaPorDefecto();
    ReiniciarEscena();
    W3dCrearEscenaPorDefecto();
    if (CollectionActive == SceneCollection) { err = "escenadefecto: la coleccion activa quedo en la raiz"; return false; }
    printf("      [escenadefecto] coleccion del usuario + camara + luz + cubo\n");
    return true;
}

// ============================================================================
//  viewports / layoutmaximizar / propsfoco / redopanel
// ============================================================================
// abrir un proyecto LIBERA el layout anterior (antes quedaba huerfano: ~0,2 MB de heap por
// apertura). Estos comandos prueban las dos mitades: que no quede ningun viewport vivo fuera
// del arbol, y que ningun puntero global quede apuntando a uno liberado.
static void PrNodos(ViewportBase* n, std::vector<ViewportBase*>& todos, std::vector<ViewportBase*>& hojas) {
    if (!n) return;
    todos.push_back(n);
    if (n->isLeaf()) { hojas.push_back(n); return; }
    if (n->ContainerKind() == 1) {
        PrNodos(((ViewportRow*)n)->childA, todos, hojas); PrNodos(((ViewportRow*)n)->childB, todos, hojas);
    } else {
        PrNodos(((ViewportColumn*)n)->childA, todos, hojas); PrNodos(((ViewportColumn*)n)->childB, todos, hojas);
    }
}
static bool PrEsHoja(const std::vector<ViewportBase*>& hojas, const ViewportBase* v) {
    for (size_t i = 0; i < hojas.size(); i++) if (hojas[i] == v) return true;
    return false;
}
static ViewportBase* PrPrimeraHoja(int kind) {
    std::vector<ViewportBase*> todos, hojas;
    PrNodos(rootViewport, todos, hojas);
    for (size_t i = 0; i < hojas.size(); i++) if (hojas[i]->ViewportKind() == kind) return hojas[i];
    return NULL;
}

static bool CmdViewports(std::istringstream& ss, std::string& err) {
    std::vector<ViewportBase*> todos, hojas, todosVis, hojasVis;
    PrNodos(LayoutRaizCompleta(), todos, hojas);   // el arbol COMPLETO (con el maximizado adentro)
    PrNodos(rootViewport, todosVis, hojasVis);     // lo que se ve (con uno maximizado, esa hoja)
    const int vivos = ViewportsVivos();
    const bool actOk = viewPortActive && PrEsHoja(hojas, viewPortActive);
    const bool v3dOk = !Viewport3DActive || PrEsHoja(hojas, (ViewportBase*)Viewport3DActive);
    const bool propsOk = !PropsActivo || PrEsHoja(hojas, (ViewportBase*)PropsActivo);
    printf("      [viewports] vivos=%d arbol=%d hojas=%d (visibles %d) maximizado=%s | activo=%s 3d=%s props=%s\n",
           vivos, (int)todos.size(), (int)hojas.size(), (int)hojasVis.size(), LayoutEstaMaximizado() ? "si" : "no",
           actOk ? "hoja" : (viewPortActive ? "AJENO" : "NULL"),
           Viewport3DActive ? (v3dOk ? "hoja" : "AJENO") : "NULL",
           PropsActivo ? (propsOk ? "hoja" : "AJENO") : "NULL");
    if (vivos != (int)todos.size()) {
        char b[200]; sprintf(b, "viewports: hay %d viewports vivos y el arbol tiene %d (un layout quedo huerfano)",
                             vivos, (int)todos.size());
        err = b; return false;
    }
    if (!actOk)   { err = "viewports: viewPortActive no es una hoja del layout actual"; return false; }
    if (!v3dOk)   { err = "viewports: Viewport3DActive apunta a un viewport que no es del layout"; return false; }
    if (!propsOk) { err = "viewports: PropsActivo apunta a un panel que no es del layout"; return false; }
    std::string clave;
    while (ss >> clave) {
        int v = 0;
        if (!(ss >> v)) { err = "viewports: falta el valor de '" + clave + "'"; return false; }
        if (clave != "hojas") { err = "viewports: assert desconocido '" + clave + "' (uso: viewports [hojas N])"; return false; }
        if ((int)hojas.size() != v) {
            char b[120]; sprintf(b, "viewports: hojas=%d y se esperaba %d", (int)hojas.size(), v);
            err = b; return false;
        }
    }
    return true;
}

static bool CmdLayoutMaximizar(std::istringstream& ss, std::string& err) {
    int kind = 0; ss >> kind;
    ViewportBase* v = PrPrimeraHoja(kind);
    if (!v) { err = "layoutmaximizar: no hay una hoja de ese tipo en el layout"; return false; }
    if (LayoutEstaMaximizado()) { err = "layoutmaximizar: ya hay un viewport maximizado"; return false; }
    viewPortActive = v;
    LayoutMaximizar();
    if (!LayoutEstaMaximizado() || rootViewport != v) { err = "layoutmaximizar: no quedo maximizado"; return false; }
    printf("      [layoutmaximizar] hoja kind %d a pantalla completa\n", kind);
    return true;
}

static bool CmdPropsFoco(std::istringstream& ss, std::string& err) {
    std::string arg; ss >> arg;
    if (arg == "nulo") {
        printf("      [propsfoco] campo enfocado=%s\n", g_textFieldActivo ? "SI" : "ninguno");
        if (g_textFieldActivo) { err = "propsfoco: sigue enfocado un campo (el panel que lo tenia se libero?)"; return false; }
        return true;
    }
    if (!arg.empty()) { err = "propsfoco: uso: propsfoco [nulo]"; return false; }
    Properties* pr = (Properties*)PrPrimeraHoja(3);
    if (!pr || !pr->propProyNombre) { err = "propsfoco: no hay panel de propiedades en el layout"; return false; }
    g_textFieldActivo = &pr->propProyNombre->field;
    printf("      [propsfoco] enfocado el campo Nombre del proyecto\n");
    return true;
}

static bool CmdPropsRename(std::istringstream& ss, std::string& err) {
    extern bool RenameActivo();
    std::string arg; ss >> arg;
    if (arg == "nulo") {
        printf("      [propsrename] rename en curso=%s\n", RenameActivo() ? "SI" : "no");
        if (RenameActivo()) { err = "propsrename: sigue un rename en curso (su boton era de un panel liberado)"; return false; }
        return true;
    }
    if (!arg.empty()) { err = "propsrename: uso: propsrename [nulo]"; return false; }
    Properties* pr = (Properties*)PrPrimeraHoja(3);
    if (!pr || !pr->propBtnRenameMat || !pr->propBtnRenameMat->action) { err = "propsrename: no hay panel de propiedades"; return false; }
    PropsActivo = pr;                      // las acciones de la tarjeta operan sobre el panel activo
    pr->propBtnRenameMat->action();
    if (!RenameActivo()) { err = "propsrename: no arranco (el objeto activo tiene un material propio?)"; return false; }
    printf("      [propsrename] rename del material en curso\n");
    return true;
}

static bool CmdRedoPanel(std::istringstream& ss, std::string& err) {
    int esperado = -1;
    if (ss >> esperado) {
        const bool activo = RedoMeshPanelActivo();
        printf("      [redopanel] abierto=%s\n", activo ? "si" : "no");
        if ((esperado != 0) != activo) { err = activo ? "redopanel: el panel sigue abierto" : "redopanel: el panel no esta abierto"; return false; }
        return true;
    }
    ViewportBase* v = PrPrimeraHoja(1);
    if (!v) { err = "redopanel: no hay viewport 3D en el layout"; return false; }
    Viewport3DActive = (Viewport3D*)v;   // el panel recuerda el 3D ACTIVO como su creador
    Mesh* m = (Mesh*)NewMesh(MeshType::cube, NULL, false);
    if (!m) { err = "redopanel: NewMesh devolvio NULL"; return false; }
    AbrirRedoMeshPanel(m);
    if (!RedoMeshPanelActivo()) { err = "redopanel: el panel no se abrio"; return false; }
    printf("      [redopanel] cubo nuevo + panel Add abierto sobre el primer 3D\n");
    return true;
}

// ============================================================================
//  propsarm2d
// ============================================================================
// Las filas Pos/Rotation/Scale de la tarjeta Armature 2D eran punteros ESTATICOS a las filas
// del ULTIMO panel construido: al liberarse ese panel (cambiarle el tipo, Expand sobre el otro,
// cualquier panel temporal del harness) quedaban colgando y ActualizarPestanias de OTRO panel
// escribia sobre filas liberadas (corrupcion del heap en Release), mientras sus propias filas
// nunca se refrescaban. Aca un panel temporal nace y muere ANTES de mirar, y cada panel del
// layout tiene que mostrar/ocultar SUS filas segun el modo.
static bool CmdPropsArm2D(std::istringstream& ss, std::string& err) {
    std::string modo; ss >> modo;
    if (modo != "huesos" && modo != "pose") { err = "propsarm2d: uso: propsarm2d huesos|pose [paneles N]"; return false; }
    const bool pose = (modo == "pose");
    int panelesEsperados = -1;
    { std::string clave;
      while (ss >> clave) {
          int v = 0;
          if (!(ss >> v)) { err = "propsarm2d: falta el valor de '" + clave + "'"; return false; }
          if (clave != "paneles") { err = "propsarm2d: assert desconocido '" + clave + "' (uso: propsarm2d huesos|pose [paneles N])"; return false; }
          panelesEsperados = v;
      } }
    UVEditor* uv = (UVEditor*)PrPrimeraHoja(4);
    if (!uv) { err = "propsarm2d: no hay editor UV en el layout"; return false; }
    if (!ObjActivo || ObjActivo->getType() != ObjectType::mesh || ObjActivo != g_editMesh ||
        !((Mesh*)ObjActivo)->TieneArm2D() || ((Mesh*)ObjActivo)->Arm2DHuesos().empty()) {
        err = "propsarm2d: hace falta la malla activa en Edit Mode con un armature 2D y algun hueso (bone2dnew)";
        return false;
    }
    Mesh* m = (Mesh*)ObjActivo;
    if (m->Arm2DBoneActivo() < 0 || m->Arm2DBoneActivo() >= (int)m->Arm2DHuesos().size())
        m->Arm2DBoneActivo() = 0;                   // las filas del hueso piden un hueso ACTIVO
    uv->uvModo = pose ? UVModoPose : UVModoHuesos;  // queda asi (el script sigue en ese modo)
    // el panel temporal: pasa a ser el ULTIMO construido y se libera enseguida
    { Properties* tmp = new Properties(); delete tmp; }

    std::vector<ViewportBase*> todos, hojas;
    PrNodos(LayoutRaizCompleta(), todos, hojas);
    int paneles = 0;
    bool ok = true;
    for (size_t i = 0; i < hojas.size(); i++) {
        if (hojas[i]->ViewportKind() != 3) continue;
        Properties* pr = (Properties*)hojas[i];
        paneles++;
        pr->pestaniaActiva = 6;                     // la pestania Armature 2D
        pr->ActualizarPestanias();
        const bool tarjeta = pr->propBones2D && pr->propBones2D->visible && pr->pestaniaActiva == 6;
        const bool pos = pr->propBone2DPosX && pr->propBone2DPosX->value && pr->propBone2DPosY && pr->propBone2DPosY->value;
        const bool rot = pr->propBone2DRot && pr->propBone2DRot->value;
        const bool scl = pr->propBone2DSclX && pr->propBone2DSclX->value && pr->propBone2DSclY && pr->propBone2DSclY->value;
        const bool bien = tarjeta && pos && rot == pose && scl == pose;
        printf("      [propsarm2d] panel %d (%s): tarjeta=%s Pos=%s Rotation=%s Scale=%s -> %s\n",
               paneles, modo.c_str(), tarjeta ? "si" : "NO", pos ? "visible" : "oculta",
               rot ? "visible" : "oculta", scl ? "visible" : "oculta", bien ? "OK" : "MAL");
        if (!bien) ok = false;
    }
    if (paneles == 0) { err = "propsarm2d: no hay paneles de propiedades en el layout"; return false; }
    if (panelesEsperados >= 0 && paneles != panelesEsperados) {
        char b[120]; sprintf(b, "propsarm2d: hay %d paneles y se esperaban %d", paneles, panelesEsperados);
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
// Un icono nuevo va en cuatro lugares (enum + KNombres + png del skin + los DOS .pkg). Los
// .pkg se mantienen a mano y nadie los compila en PC: un icono que se olvida ahi no se instala
// en el celular y el atlas cae al arte legacy de otro icono, sin ningun error. Esto cruza las
// cuatro listas desde la PC.
static bool CmdIconosPkg(std::string& err) {
    const char* bases[5] = { "", "../", "../../", "../../../", "../../../../" };
    std::string raiz; bool hay = false;
    for (int i = 0; i < 5 && !hay; i++)
        if (w3dFileSystem::FileExists(std::string(bases[i]) + "platform/symbian/sis/Whisk3D_armv5.pkg")) {
            raiz = bases[i]; hay = true;
        }
    if (!hay) { err = "iconospkg: no encuentro platform/symbian/sis (correr desde tools/pruebas)"; return false; }
    const char* pkgs[2] = { "Whisk3D_armv5.pkg", "Whisk3D_gcce.pkg" };
    std::string texto[2];
    for (int k = 0; k < 2; k++) {
        std::vector<unsigned char> b;
        if (!w3dFileSystem::ReadFileBytes(raiz + "platform/symbian/sis/" + pkgs[k], b) || b.empty()) {
            err = std::string("iconospkg: no pude leer ") + pkgs[k]; return false;
        }
        texto[k].assign((const char*)&b[0], b.size());
    }
    const std::string dirIconos = raiz + "res/Skins/Whisk3D/atlas/iconos/";
    int faltaPng = 0, malTam = 0, faltaPkg = 0, sobraPkg = 0;
    for (int i = 0; i < (int)ICON_TOTAL; i++) {
        const std::string n = IconoNombre(i);
        if (n.empty()) { char b[80]; sprintf(b, "iconospkg: el icono %d no tiene nombre (KNombres)", i); err = b; return false; }
        const std::string png = dirIconos + n + ".png";
        if (!w3dFileSystem::FileExists(png)) {
            printf("      [iconospkg] '%s': falta %s\n", n.c_str(), png.c_str()); faltaPng++;
        } else {
            unsigned char* px = 0; int w = 0, h = 0;
            const bool dec = w3dEngine::DecodeImage(png.c_str(), &px, &w, &h) && px;
            if (px) w3dEngine::FreeImage(px);
            if (!dec || w != 10 || h != 10) {
                printf("      [iconospkg] '%s': el png no es de 10x10 (%dx%d)\n", n.c_str(), w, h); malTam++;
            }
        }
        // cada .pkg lo nombra DOS veces: el origen (res\...) y el destino (!:\private\...)
        const std::string ref = "\\atlas\\iconos\\" + n + ".png\"";
        for (int k = 0; k < 2; k++) {
            int veces = 0;
            for (size_t p = texto[k].find(ref); p != std::string::npos; p = texto[k].find(ref, p + 1)) veces++;
            if (veces < 2) { printf("      [iconospkg] '%s': falta en %s\n", n.c_str(), pkgs[k]); faltaPkg++; }
        }
    }
    // al reves: todo png del skin que instala un .pkg existe (sino makesis corta el empaquetado)
    const std::string marca = "res\\Skins\\Whisk3D\\atlas\\iconos\\";
    int instalados[2] = { 0, 0 };
    for (int k = 0; k < 2; k++) {
        for (size_t p = texto[k].find(marca); p != std::string::npos; p = texto[k].find(marca, p + 1)) {
            const size_t a = p + marca.size();
            const size_t e = texto[k].find('"', a);
            if (e == std::string::npos) break;
            const std::string archivo = texto[k].substr(a, e - a);
            instalados[k]++;
            if (!w3dFileSystem::FileExists(dirIconos + archivo)) {
                printf("      [iconospkg] %s instala '%s' y no existe en el skin\n", pkgs[k], archivo.c_str()); sobraPkg++;
            }
        }
    }
    printf("      [iconospkg] ICON_TOTAL=%d | %s instala %d, %s instala %d | faltan png=%d, png que no son 10x10=%d, "
           "faltan en los .pkg=%d, .pkg que instalan un png inexistente=%d\n",
           (int)ICON_TOTAL, pkgs[0], instalados[0], pkgs[1], instalados[1], faltaPng, malTam, faltaPkg, sobraPkg);
    if (faltaPng || malTam || faltaPkg || sobraPkg) {
        err = "iconospkg: los iconos del enum, el skin y los .pkg de Symbian no coinciden (ver arriba)";
        return false;
    }
    return true;
}

// ============================================================================
//  LA MALLA BINARIA (.w3db, io/W3dMallaBin.h): formato, equivalencia y robustez
// ============================================================================

// ---- formatomallas [texto|binario] | proyecto [texto|binario] | es texto|binario ----
static bool CmdFormatoMallas(std::istringstream& ss, std::string& err) {
    std::string a, b; ss >> a >> b;
    if (a == "texto")        g_w3dFormatoMallasForzado = W3D_MALLAS_TEXTO;
    else if (a == "binario") g_w3dFormatoMallasForzado = W3D_MALLAS_BINARIO;
    else if (a == "proyecto") {
        g_w3dFormatoMallasForzado = -1;
        if (b == "texto")        g_w3dFormatoMallasProyecto = W3D_MALLAS_TEXTO;
        else if (b == "binario") g_w3dFormatoMallasProyecto = W3D_MALLAS_BINARIO;
        else if (!b.empty()) { err = "formatomallas: 'proyecto' acepta texto|binario"; return false; }
    } else if (a == "es") {
        const int quiero = (b == "texto") ? W3D_MALLAS_TEXTO : W3D_MALLAS_BINARIO;
        if (b != "texto" && b != "binario") { err = "formatomallas: uso: formatomallas es texto|binario"; return false; }
        if (W3dFormatoMallas() != quiero) {
            err = std::string("formatomallas: el formato vigente es ") + (W3dFormatoMallas() == W3D_MALLAS_TEXTO ? "texto" : "binario");
            return false;
        }
    } else if (!a.empty()) { err = "formatomallas: uso: formatomallas [texto|binario|proyecto [texto|binario]|es texto|binario]"; return false; }
    printf("      [formatomallas] vigente=%s (proyecto=%s, forzado=%s)\n",
           W3dFormatoMallas() == W3D_MALLAS_TEXTO ? "texto" : "binario",
           g_w3dFormatoMallasProyecto == W3D_MALLAS_TEXTO ? "texto" : "binario",
           g_w3dFormatoMallasForzado < 0 ? "no" : (g_w3dFormatoMallasForzado == W3D_MALLAS_TEXTO ? "texto" : "binario"));
    return true;
}

// ---- logcargamallas <w3db> <w3dm> <rechazadas> ----
// La ULTIMA linea "[CARGA] mallas .w3db=" del log (la escribe AbrirW3D en cada apertura, con o sin
// reloj de medicion) tiene que decir exactamente esas cuentas. Es lo que se lee en el log del
// telefono (no tiene harness) para saber que las .w3db abrieron listas, sin derivar nada.
static bool CmdLogCargaMallas(std::istringstream& ss, std::string& err) {
    int bin = -1, tex = -1, rech = -1;
    if (!(ss >> bin >> tex >> rech)) { err = "logcargamallas: uso: logcargamallas <w3db> <w3dm> <rechazadas>"; return false; }
    const char* pre = "[CARGA] mallas .w3db=";
    for (int i = w3dLogRingCount() - 1; i >= 0; i--) {
        const char* l = strstr(w3dLogRingLinea(i), pre);
        if (!l) continue;
        int b = -1, t = -1, r = -1;
        if (sscanf(l, "[CARGA] mallas .w3db=%d (listas, sin derivar)  .w3dm=%d (derivadas al abrir)  .w3db rechazadas=%d", &b, &t, &r) != 3) {
            err = std::string("logcargamallas: la linea no tiene el formato esperado: ") + l; return false;
        }
        printf("      [logcargamallas] .w3db=%d .w3dm=%d rechazadas=%d\n", b, t, r);
        if (b != bin || t != tex || r != rech) {
            char x[160]; sprintf(x, "logcargamallas: el log dice .w3db=%d .w3dm=%d rechazadas=%d y se esperaba %d %d %d", b, t, r, bin, tex, rech);
            err = x; return false;
        }
        return true;
    }
    err = "logcargamallas: no hay ninguna linea \"[CARGA] mallas .w3db=\" en el log"; return false;
}

// ---- las entradas de mallas de un .w3d (nombre + bytes) ----
static bool PrEntradasMallas(const std::string& ruta, std::vector<W3dZipEntrada>& out, int& nBin, int& nTxt) {
    out.clear(); nBin = nTxt = 0;
    std::vector<W3dZipEntrada> ents;
    if (!W3dZipLeer(ruta, &ents)) return false;
    for (size_t i = 0; i < ents.size(); i++) {
        const std::string& n = ents[i].nombre;
        if (n.compare(0, 7, "mallas/") != 0) continue;
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
    const std::string ruta = PrLeerRuta(ss);
    if (ruta.empty()) { err = "w3dbinfo: uso: w3dbinfo <ruta.w3d> [detalle] [w3db N] [w3dm N] [tiene XXXX] [notiene XXXX]"; return false; }
    bool detalle = false;
    std::vector<std::string> claves, valores;
    std::string t;
    while (ss >> t) {
        if (t == "detalle") { detalle = true; continue; }
        std::string v;
        if (!(ss >> v)) { err = "w3dbinfo: falta el valor de '" + t + "'"; return false; }
        while (v.size() < 4 && (t == "tiene" || t == "notiene")) v += ' ';
        claves.push_back(t); valores.push_back(v);
    }
    std::vector<W3dZipEntrada> bins; int nBin = 0, nTxt = 0;
    if (!PrEntradasMallas(ruta, bins, nBin, nTxt)) { err = "w3dbinfo: no pude leer " + ruta; return false; }
    int rotas = 0;
    long bytesBin = 0;
    std::map<std::string, int> conBloque;   // fourcc -> en cuantas .w3db esta
    for (size_t i = 0; i < bins.size(); i++) {
        const std::vector<unsigned char>& d = bins[i].datos;
        bytesBin += (long)d.size();
        std::vector<std::string> lineas;
        const bool ok = !d.empty() && W3dMallaBinListar(&d[0], d.size(), lineas);
        W3dMallaRender r; W3dMallaInfo info;
        const bool leeRender = ok && W3dMallaBinLeerRender(&d[0], d.size(), r, &info);
        if (!leeRender) rotas++;
        if (detalle || i == 0) {
            printf("      [w3dbinfo] %s (%d B)%s\n", bins[i].nombre.c_str(), (int)d.size(), leeRender ? "" : " <-- NO SE LEE");
            for (size_t k = 0; k < lineas.size(); k++) printf("      [w3dbinfo]    %s\n", lineas[k].c_str());
        }
        for (size_t k = 0; k < lineas.size(); k++) conBloque[lineas[k].substr(0, 4)]++;
    }
    printf("      [w3dbinfo] %s: mallas binarias=%d (%ld B) de texto=%d | no se leen=%d\n",
           ruta.c_str(), nBin, bytesBin, nTxt, rotas);
    if (rotas > 0) { err = "w3dbinfo: hay .w3db que el lector de render no acepta"; return false; }
    for (size_t i = 0; i < claves.size(); i++) {
        const std::string& c = claves[i]; const std::string& v = valores[i];
        char b[200];
        if (c == "w3db" || c == "w3dm") {
            const int esp = atoi(v.c_str()), hay = (c == "w3db") ? nBin : nTxt;
            if (hay != esp) { sprintf(b, "w3dbinfo: %s=%d y se esperaba %d", c.c_str(), hay, esp); err = b; return false; }
        } else if (c == "tiene" || c == "notiene") {
            const int n = conBloque.count(v) ? conBloque[v] : 0;
            if (c == "tiene" && n != nBin) { sprintf(b, "w3dbinfo: el bloque '%s' esta en %d de %d .w3db", v.c_str(), n, nBin); err = b; return false; }
            if (c == "notiene" && n != 0)  { sprintf(b, "w3dbinfo: el bloque '%s' aparece en %d .w3db", v.c_str(), n); err = b; return false; }
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
    bool Igual(const PrHash& o) const { return a == o.a && b == o.b && n == o.n; }
};
typedef std::map<std::string, PrHash> PrCampos;
typedef std::map<std::string, PrCampos> PrFoto;     // ruta del objeto -> campos
static std::map<std::string, PrFoto> gPrFotos;

static std::string PrRutaObj(Object* o) {
    std::string r = o->name;
    for (Object* p = o->Parent; p && p != SceneCollection; p = p->Parent) r = p->name + "/" + r;
    return r;
}
static void PrJuntarMallas(Object* o, std::vector<Mesh*>& out) {
    if (!o) return;
    if (o != SceneCollection && o->getType() == ObjectType::mesh) out.push_back((Mesh*)o);
    for (size_t i = 0; i < o->Childrens.size(); i++) PrJuntarMallas(o->Childrens[i], out);
}

// los campos de UNA malla (render siempre; edicion salvo 'sinEdicion')
static void PrFotoCampos(Mesh* m, PrCampos& c, bool sinEdicion) {
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
          h.Str(vg->nombre); h.Int((int)vg->verts.size());
          if (!vg->verts.empty()) h.Bytes(&vg->verts[0], vg->verts.size() * 4);
          if (!vg->pesos.empty()) h.Bytes(&vg->pesos[0], vg->pesos.size() * 4);
      }
      h.Int(m->grupoActivo); c["vertexGroups"] = h; }
    { PrHash h;
      for (size_t k = 0; k < m->shapeKeys.size(); k++) {
          const W3dShapeKey& sk = m->shapeKeys[k];
          h.Str(sk.nombre); h.Int((int)sk.idx.size());
          if (!sk.idx.empty()) h.Bytes(&sk.idx[0], sk.idx.size() * 4);
          if (!sk.d.empty()) h.Bytes(&sk.d[0], sk.d.size() * 4);
      }
      h.Int((int)m->shapePesos.size()); c["shapeKeys"] = h; }
    { PrHash h;
      for (size_t g = 0; g < m->uvGroups.size(); g++) {
          const UVGroup* ug = m->uvGroups[g]; if (!ug) { h.Int(-1); continue; }
          h.Str(ug->nombre); h.Int((int)ug->verts.size());
          if (!ug->verts.empty()) h.Bytes(&ug->verts[0], ug->verts.size() * 4);
          if (!ug->pesos.empty()) h.Bytes(&ug->pesos[0], ug->pesos.size() * 4);
      }
      h.Int(m->uvGrupoActivo); c["uvGroups"] = h; }
    { PrHash h; h.Int((int)m->uv2dRest.size()); if (!m->uv2dRest.empty()) h.Bytes(&m->uv2dRest[0], m->uv2dRest.size() * 4); c["uv2dRest"] = h; }
    { PrHash h; h.Int(m->aabbOk ? 1 : 0);
      if (m->aabbOk) { h.Float(m->aabbMin.x); h.Float(m->aabbMin.y); h.Float(m->aabbMin.z);
                       h.Float(m->aabbMax.x); h.Float(m->aabbMax.y); h.Float(m->aabbMax.z); }
      c["aabb"] = h; }
    { PrHash h; h.Float(m->centroGeom.x); h.Float(m->centroGeom.y); h.Float(m->centroGeom.z); h.Float(m->radioGeom); c["centroRadio"] = h; }
    { PrHash h; h.Int(m->meshSmooth ? 1 : 0); h.Int(m->meshTipo);
      if (m->meshTipo >= 0) { h.Float(m->meshSize); h.Float(m->meshSize2); h.Float(m->meshDepth); h.Int(m->meshVerts); h.Int(m->meshVerts2); }
      h.Int(m->noEditable ? 1 : 0); c["cabecera"] = h; }
    // la malla GENERADA por el stack de modificadores (si tiene): se regenera al abrir
    { PrHash h; h.Int(m->genValido ? 1 : 0);
      if (m->genValido) {
          h.Int(m->genVertexSize); h.Int(m->genFacesSize);
          if (m->genVertex && m->genVertexSize > 0) h.Bytes(m->genVertex, (size_t)m->genVertexSize * 12);
          if (m->genFaces && m->genFacesSize > 0) h.Bytes(m->genFaces, (size_t)m->genFacesSize * sizeof(MeshIndex));
      }
      c["gen"] = h; }
    if (sinEdicion) return;
    { PrHash h;
      for (size_t f = 0; f < m->faces3d.size(); f++) {
          const MeshFace& F = m->faces3d[f];
          h.Int((int)F.idx.size()); if (!F.idx.empty()) h.Bytes(&F.idx[0], F.idx.size() * 4);
          h.Int(F.mat); h.Int(F.smooth);
      }
      c["faces3d"] = h; }
    { PrHash h; h.Int((int)m->looseEdges.size()); if (!m->looseEdges.empty()) h.Bytes(&m->looseEdges[0], m->looseEdges.size() * 4);
      h.Int((int)m->looseVerts.size()); if (!m->looseVerts.empty()) h.Bytes(&m->looseVerts[0], m->looseVerts.size() * 4);
      c["sueltos"] = h; }
    { PrHash h; h.Int((int)m->cornerNormal.size()); if (!m->cornerNormal.empty()) h.Bytes(&m->cornerNormal[0], m->cornerNormal.size()); c["cornerNormal"] = h; }
    { PrHash h;
      for (size_t l = 0; l < m->uvMaps.size(); l++) { h.Str(m->uvMaps[l]->nombre); h.Int((int)m->uvMaps[l]->uv.size());
          if (!m->uvMaps[l]->uv.empty()) h.Bytes(&m->uvMaps[l]->uv[0], m->uvMaps[l]->uv.size() * 4); }
      h.Int(m->uvMapActivo); c["uvMaps"] = h; }
    { PrHash h;
      for (size_t l = 0; l < m->colorLayers.size(); l++) { h.Str(m->colorLayers[l]->nombre); h.Int(m->colorLayers[l]->porVertice ? 1 : 0);
          h.Int((int)m->colorLayers[l]->color.size());
          if (!m->colorLayers[l]->color.empty()) h.Bytes(&m->colorLayers[l]->color[0], m->colorLayers[l]->color.size()); }
      h.Int(m->colorActivo); c["colorLayers"] = h; }
    { PrHash h;
      for (std::set<std::string>::const_iterator it = m->sharpEdges.begin(); it != m->sharpEdges.end(); ++it) h.Str(*it);
      h.Int(-7);
      for (std::set<std::string>::const_iterator it = m->seamEdges.begin(); it != m->seamEdges.end(); ++it) h.Str(*it);
      c["marcas"] = h; }
    { PrHash h; h.Int((int)m->edges.size()); if (!m->edges.empty()) h.Bytes(&m->edges[0], m->edges.size() * 4);
      h.Int((int)m->bordesBuf.size()); if (!m->bordesBuf.empty()) h.Bytes(&m->bordesBuf[0], m->bordesBuf.size() * 4);
      c["aristas"] = h; }
    { PrHash h; h.Int((int)m->posRep.size()); if (!m->posRep.empty()) h.Bytes(&m->posRep[0], m->posRep.size() * 4); h.Int(m->vertsAgrupados); c["posRep"] = h; }
    { PrHash h;
      for (size_t i = 0; i < m->w3dmAjenos.requiere.size(); i++) h.Str(m->w3dmAjenos.requiere[i]);
      for (size_t i = 0; i < m->w3dmAjenos.bloques.size(); i++) { h.Str(m->w3dmAjenos.bloques[i].nombre); h.Str(m->w3dmAjenos.bloques[i].texto); }
      for (size_t i = 0; i < m->w3dmAjenos.bloquesBin.size(); i++) { h.Str(m->w3dmAjenos.bloquesBin[i].nombre); h.Str(m->w3dmAjenos.bloquesBin[i].texto); }
      c["preservacion"] = h; }
}

// lo DINAMICO en el cuadro f: skinning, vertex anims y shape keys (se evalua y se restaura)
static void PrFotoCuadro(Mesh* m, int f, PrCampos& c) {
    char k[48];
    const int nV = m->vertex ? m->vertexSize : 0;
    if (m->skinArmature && nV > 0) {
        const int antes = CurrentFrame;
        CurrentFrame = f;
        m->skinArmature->lastPoseFrame = -999999;
        m->lastSkinFrame = -999999;
        SkinearMesh(m);
        PrHash h;
        if (m->skinVertex) h.Bytes(m->skinVertex, (size_t)nV * 12);
        if (m->skinNormals) h.Bytes(m->skinNormals, (size_t)nV * 3);
        sprintf(k, "skin@%d", f); c[k] = h;
        CurrentFrame = antes;
    }
    for (size_t a = 0; a < m->animations.size() && nV > 0; a++) {
        VertexAnimation* an = m->animations[a];
        if (!an) continue;
        std::vector<GLfloat> pv(m->vertex, m->vertex + (size_t)nV * 3), uv;
        std::vector<GLbyte> nv;
        if (m->normals) nv.assign(m->normals, m->normals + (size_t)nV * 3);
        if (m->uv) uv.assign(m->uv, m->uv + (size_t)nV * 2);
        const bool posada = m->posadaPorAnim;
        EvalVertexAnim(*an, m, (float)f);
        PrHash h;
        h.Bytes(m->vertex, (size_t)nV * 12);
        if (m->normals) h.Bytes(m->normals, (size_t)nV * 3);
        if (m->uv) h.Bytes(m->uv, (size_t)nV * 8);
        sprintf(k, "va%d@%d", (int)a, f); c[k] = h;
        memcpy(m->vertex, &pv[0], pv.size() * 4);
        if (m->normals && !nv.empty()) memcpy(m->normals, &nv[0], nv.size());
        if (m->uv && !uv.empty()) memcpy(m->uv, &uv[0], uv.size() * 4);
        m->posadaPorAnim = posada;
    }
}
static void PrFotoFormas(Mesh* m, PrCampos& c) {
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

// ---- mallafoto <nombre> [frame N]... [sinedicion] ----
static bool CmdMallaFoto(std::istringstream& ss, std::string& err) {
    std::string nombre; ss >> nombre;
    if (nombre.empty()) { err = "mallafoto: uso: mallafoto <nombre> [frame N]... [sinedicion]"; return false; }
    std::vector<int> frames; bool sinEdicion = false;
    std::string t;
    while (ss >> t) {
        if (t == "sinedicion") { sinEdicion = true; continue; }
        if (t == "frame") { int f = 0; if (!(ss >> f)) { err = "mallafoto: 'frame' sin numero"; return false; } frames.push_back(f); continue; }
        err = "mallafoto: argumento desconocido '" + t + "'"; return false;
    }
    std::vector<Mesh*> ms;
    PrJuntarMallas(SceneCollection, ms);
    PrFoto foto;
    long campos = 0;
    for (size_t i = 0; i < ms.size(); i++) {
        // la edicion de una malla de un RECURSO compartido queda pendiente al abrir (se lee al
        // pedirla): la foto con edicion compara LA MALLA, asi que primero se materializa
        if (!sinEdicion && ms[i]->malla && !ms[i]->edicionPendiente.empty()) W3dMallaBinMaterializarEdicion(ms[i]);
        PrCampos& c = foto[PrRutaObj(ms[i])];
        PrFotoCampos(ms[i], c, sinEdicion);
        PrFotoFormas(ms[i], c);
        for (size_t f = 0; f < frames.size(); f++) PrFotoCuadro(ms[i], frames[f], c);
        campos += (long)c.size();
    }
    gPrFotos[nombre] = foto;
    printf("      [mallafoto] '%s': %d mallas, %ld campos%s\n", nombre.c_str(), (int)ms.size(), campos,
           sinEdicion ? " (sin los de edicion)" : "");
    if (ms.empty()) { err = "mallafoto: no hay ninguna malla en la escena"; return false; }
    return true;
}

// ---- mallafotoigual <a> <b> [sinedicion]: las dos fotos tienen las MISMAS mallas con
//      los MISMOS bytes en cada campo (con 'sinedicion' se ignoran los de edicion) ----
static bool CmdMallaFotoIgual(std::istringstream& ss, std::string& err) {
    std::string a, b, op; ss >> a >> b >> op;
    if (!gPrFotos.count(a) || !gPrFotos.count(b)) { err = "mallafotoigual: no existe la foto '" + (gPrFotos.count(a) ? b : a) + "'"; return false; }
    const PrFoto& A = gPrFotos[a];
    const PrFoto& B = gPrFotos[b];
    static const char* kEdicion[] = { "faces3d", "sueltos", "cornerNormal", "uvMaps", "colorLayers", "marcas", "aristas", "posRep", "preservacion", 0 };
    const bool sinEdicion = (op == "sinedicion");
    std::vector<std::string> dif;
    for (PrFoto::const_iterator it = A.begin(); it != A.end(); ++it) {
        PrFoto::const_iterator o = B.find(it->first);
        if (o == B.end()) { dif.push_back("'" + it->first + "' falta en " + b); continue; }
        for (PrCampos::const_iterator c = it->second.begin(); c != it->second.end(); ++c) {
            bool esEd = false;
            for (int k = 0; kEdicion[k]; k++) if (c->first == kEdicion[k]) esEd = true;
            if (sinEdicion && esEd) continue;
            PrCampos::const_iterator d = o->second.find(c->first);
            if (d == o->second.end()) { dif.push_back("'" + it->first + "'." + c->first + " falta en " + b); continue; }
            if (!c->second.Igual(d->second)) {
                char buf[64]; sprintf(buf, " (%lu B contra %lu B)", c->second.n, d->second.n);
                dif.push_back("'" + it->first + "'." + c->first + buf);
            }
        }
        for (PrCampos::const_iterator c = o->second.begin(); c != o->second.end(); ++c) {
            bool esEd = false;
            for (int k = 0; kEdicion[k]; k++) if (c->first == kEdicion[k]) esEd = true;
            if (sinEdicion && esEd) continue;
            if (!it->second.count(c->first)) dif.push_back("'" + it->first + "'." + c->first + " sobra en " + b);
        }
    }
    for (PrFoto::const_iterator it = B.begin(); it != B.end(); ++it)
        if (!A.count(it->first)) dif.push_back("'" + it->first + "' sobra en " + b);
    printf("      [mallafotoigual] %s vs %s: %d mallas, %d diferencia(s)\n", a.c_str(), b.c_str(), (int)A.size(), (int)dif.size());
    for (size_t i = 0; i < dif.size() && i < 16; i++) printf("      [mallafotoigual]   %s\n", dif[i].c_str());
    if (!dif.empty()) { err = "mallafotoigual: las mallas NO quedaron iguales: " + dif[0]; return false; }
    return true;
}

// ---- una malla suelta (fuera del arbol) para leer entradas en el harness ----
static Mesh* PrMallaSuelta() {
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
//      (W3dMallaBinLeerRender, la base del recurso compartido). Ademas el modo "contrario"
//      (noEditable al reves de como se guardo) tiene que dar lo que da la carga clasica. ----
static bool CmdW3dbRender(std::istringstream& ss, std::string& err) {
    const std::string ruta = PrLeerRuta(ss);
    if (ruta.empty()) { err = "w3dbrender: uso: w3dbrender <ruta.w3d> (con ese proyecto abierto)"; return false; }
    std::vector<W3dZipEntrada> bins; int nBin = 0, nTxt = 0;
    if (!PrEntradasMallas(ruta, bins, nBin, nTxt)) { err = "w3dbrender: no pude leer " + ruta; return false; }
    if (bins.empty()) { err = "w3dbrender: " + ruta + " no tiene mallas .w3db"; return false; }
    int ok = 0, contrario = 0;
    for (size_t i = 0; i < bins.size(); i++) {
        const std::vector<unsigned char>& d = bins[i].datos;
        const std::string& nom = bins[i].nombre;
        { std::vector<unsigned char> montada;
          if (!w3dFileSystem::ReadFileBytes(nom, montada) || montada != d) { err = "w3dbrender: la entrada " + nom + " no es la del contenedor montado (abri " + ruta + " antes)"; return false; } }
        W3dMallaRender R; W3dMallaInfo iR;
        if (!W3dMallaBinLeerRender(&d[0], d.size(), R, &iR)) { err = "w3dbrender: LeerRender rechazo " + nom; return false; }
        W3dMallaBinOpciones op; op.noEditable = R.statsNoEditable;
        Mesh* A = PrMallaSuelta(); W3dMallaInfo iA;
        Mesh* B = PrMallaSuelta(); W3dMallaInfo iB;
        bool bien = W3dMallaBinLeer(&d[0], d.size(), A, &iA, op);
        A->noEditable = op.noEditable;   // (el flag lo pone el que carga, desde el proyecto)
        W3dMallaBinOpciones opJ = op; opJ.edicion = false;
        bien = bien && W3dMallaBinLeer(&d[0], d.size(), B, &iB, opJ);
        std::string motivo;
        if (!bien) motivo = "la lectura fallo";
        // el juego: sin edicion en memoria, y se materializa desde el contenedor
        if (motivo.empty() && (!B->faces3d.empty() || !B->uvMaps.empty() || !B->edges.empty())) motivo = "el camino del juego cargo datos de edicion";
        B->noEditable = op.noEditable;
        B->edicionPendiente = nom;
        // por la PUERTA REAL: el stack de modificadores (y el Boolean) arman sus poligonos con
        // ConstruirPolyMesh, que materializa lo pendiente la primera vez
        if (motivo.empty()) {
            PolyMesh W;
            ConstruirPolyMesh(B, W);
            if (!B->edicionPendiente.empty()) motivo = "ConstruirPolyMesh no materializo la edicion pendiente";
            else if (B->faces3d.size() != A->faces3d.size()) motivo = "MaterializarEdicion no trajo las caras";
            else if (W3dMallaBinMaterializarEdicion(B)) motivo = "MaterializarEdicion no es idempotente (volvio a leer)";
        }
        if (motivo.empty()) {
            PrCampos cA, cB; PrFotoCampos(A, cA, false); PrFotoCampos(B, cB, false);
            for (PrCampos::iterator it = cA.begin(); it != cA.end() && motivo.empty(); ++it)
                if (!cB.count(it->first) || !it->second.Igual(cB[it->first])) motivo = "juego+materializar difiere en " + it->first;
        }
        // la malla de render sin Mesh contra la del editor
        if (motivo.empty()) {
            const int nV = A->vertex ? A->vertexSize : 0;
            if (R.nRV != nV) motivo = "LeerRender: otra cantidad de render-verts";
            else if (nV && (memcmp(&R.pos[0], A->vertex, (size_t)nV * 12) || memcmp(&R.nrm[0], A->normals, (size_t)nV * 3) ||
                            memcmp(&R.uv[0], A->uv, (size_t)nV * 8) || memcmp(&R.col[0], A->vertexColor, (size_t)nV * 4) ||
                            memcmp(&R.ctrl[0], &A->vertCtrlPoint[0], (size_t)nV * 4))) motivo = "LeerRender: los arrays de render difieren";
            else if ((int)R.idx.size() != A->facesSize || (A->facesSize && memcmp(&R.idx[0], A->faces, (size_t)A->facesSize * sizeof(MeshIndex)))) motivo = "LeerRender: el index buffer difiere";
            else if (R.partes.size() != A->materialsGroup.size()) motivo = "LeerRender: otras mesh parts";
            else if (R.grupos.size() != A->vertexGroups.size() || R.formas.size() != A->shapeKeys.size() || R.uvGrupos.size() != A->uvGroups.size()) motivo = "LeerRender: otros grupos/formas";
            else if (R.Bytes() <= 0 && nV > 0) motivo = "LeerRender: Bytes() no cuenta nada";
        }
        // el modo CONTRARIO: tiene que dar lo mismo que la carga clasica (CalcularAABBSolo o
        // CalcularBordes(true,false) con los puntos del archivo)
        if (motivo.empty() && A->vertexSize > 0) {
            Mesh* C = PrMallaSuelta(); W3dMallaInfo iC;
            W3dMallaBinOpciones opC = op; opC.noEditable = !op.noEditable;
            if (!W3dMallaBinLeer(&d[0], d.size(), C, &iC, opC)) motivo = "la lectura en modo contrario fallo";
            else {
                A->noEditable = opC.noEditable;
                if (opC.noEditable) A->CalcularAABBSolo();
                else { A->posRep = A->vertCtrlPoint; A->CalcularBordes(true, false); }
                PrCampos cA, cC; PrFotoCampos(A, cA, false); PrFotoCampos(C, cC, false);
                static const char* kMirar[] = { "aabb", "centroRadio", "aristas", "posRep", 0 };
                for (int k = 0; kMirar[k] && motivo.empty(); k++)
                    if (!cA[kMirar[k]].Igual(cC[kMirar[k]])) motivo = std::string("modo contrario difiere en ") + kMirar[k];
                contrario++;
            }
            delete C;
        }
        delete A; delete B;
        if (!motivo.empty()) { err = "w3dbrender: " + nom + ": " + motivo; return false; }
        ok++;
    }
    printf("      [w3dbrender] %d .w3db: editor == juego+materializar == render sin Mesh; modo contrario == carga clasica en %d\n", ok, contrario);
    return true;
}

// ---- un .w3db armado a mano: tabla de bloques (parse / re-emision) ----
struct PrBloque { std::string id; unsigned dom, cant; std::string datos; };
static unsigned PrU32(const unsigned char* p) { return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24); }
static void PrPonU32(std::string& s, unsigned v) { char b[4] = { (char)(v & 255u), (char)((v >> 8) & 255u), (char)((v >> 16) & 255u), (char)((v >> 24) & 255u) }; s.append(b, 4); }
static bool PrPartirW3db(const std::vector<unsigned char>& d, std::vector<PrBloque>& out) {
    out.clear();
    if (d.size() < 20) return false;
    const unsigned nb = PrU32(&d[12]);
    size_t off = 20;
    for (unsigned i = 0; i < nb; i++) {
        if (d.size() - off < 16) return false;
        PrBloque b;
        b.id.assign((const char*)&d[off], 4);
        b.dom = PrU32(&d[off + 4]); b.cant = PrU32(&d[off + 8]);
        const unsigned by = PrU32(&d[off + 12]);
        if (by > d.size() - off - 16) return false;
        b.datos.assign((const char*)&d[off + 16], by);
        out.push_back(b);
        off += 16 + ((by + 3u) & ~3u);
    }
    return true;
}
static std::vector<unsigned char> PrArmarW3db(const std::vector<PrBloque>& bl, unsigned version) {
    std::string s = "W3DB";
    PrPonU32(s, version); PrPonU32(s, 0); PrPonU32(s, (unsigned)bl.size()); PrPonU32(s, 0);
    for (size_t i = 0; i < bl.size(); i++) {
        s += bl[i].id; PrPonU32(s, bl[i].dom); PrPonU32(s, bl[i].cant); PrPonU32(s, (unsigned)bl[i].datos.size());
        s += bl[i].datos;
        while (s.size() % 4) s += '\0';
    }
    return std::vector<unsigned char>(s.begin(), s.end());
}

// ---- w3dbcorrupto <ruta.w3d>: la ROBUSTEZ del lector sobre las .w3db del contenedor:
//      cortado en cada borde de bloque y en cientos de lugares, bytes cambiados al azar
//      (determinista), version mas nueva, un bloque que no conoce (se PRESERVA al
//      reescribir) y uno que no conoce pero esta en REQ (abre, y guardar encima se frena).
//      Un archivo roto NUNCA crashea ni deja la malla a medio llenar. ----
static bool CmdW3dbCorrupto(std::istringstream& ss, std::string& err) {
    const std::string ruta = PrLeerRuta(ss);
    if (ruta.empty()) { err = "w3dbcorrupto: uso: w3dbcorrupto <ruta.w3d>"; return false; }
    std::vector<W3dZipEntrada> bins; int nBin = 0, nTxt = 0;
    if (!PrEntradasMallas(ruta, bins, nBin, nTxt) || bins.empty()) { err = "w3dbcorrupto: " + ruta + " no tiene mallas .w3db"; return false; }
    // la malla testigo: tiene que quedar INTACTA cada vez que el lector rechaza un archivo
    Mesh* testigo = PrMallaSuelta();
    { W3dMallaInfo i0; W3dMallaBinOpciones op;
      if (!W3dMallaBinLeer(&bins[0].datos[0], bins[0].datos.size(), testigo, &i0, op)) { delete testigo; err = "w3dbcorrupto: la entrada sana no se lee"; return false; } }
    PrCampos antes; PrFotoCampos(testigo, antes, false);
    int probadas = 0, rechazadas = 0, aceptadas = 0, tocadas = 0;
    unsigned semilla = 12345u;
    for (size_t e = 0; e < bins.size() && e < 6; e++) {
        const std::vector<unsigned char>& d = bins[e].datos;
        std::vector<size_t> cortes;
        for (size_t c = 0; c < d.size(); c += (d.size() > 4000 ? d.size() / 400 : 4)) cortes.push_back(c);
        for (size_t k = 0; k < cortes.size(); k++) {
            std::vector<unsigned char> x(d.begin(), d.begin() + (long)cortes[k]);
            W3dMallaInfo inf; W3dMallaBinOpciones op;
            const bool leyo = !x.empty() && W3dMallaBinLeer(&x[0], x.size(), testigo, &inf, op);
            probadas++;
            if (leyo) { aceptadas++; W3dMallaBinLeer(&bins[0].datos[0], bins[0].datos.size(), testigo, &inf, op); }
            else {
                rechazadas++;
                PrCampos ahora; PrFotoCampos(testigo, ahora, false);
                for (PrCampos::iterator it = antes.begin(); it != antes.end(); ++it) if (!it->second.Igual(ahora[it->first])) { tocadas++; break; }
            }
        }
        for (int k = 0; k < 400; k++) {
            std::vector<unsigned char> x = d;
            for (int q = 0; q < 4; q++) {
                semilla = semilla * 1103515245u + 12345u;
                const size_t pos = (size_t)((semilla >> 8) % (unsigned)x.size());
                semilla = semilla * 1103515245u + 12345u;
                x[pos] = (unsigned char)(semilla >> 16);
            }
            W3dMallaInfo inf; W3dMallaBinOpciones op;
            op.edicion = (k % 2) == 0;
            probadas++;
            if (W3dMallaBinLeer(&x[0], x.size(), testigo, &inf, op)) {
                aceptadas++;
                W3dMallaBinOpciones sano; W3dMallaBinLeer(&bins[0].datos[0], bins[0].datos.size(), testigo, &inf, sano);
            } else {
                rechazadas++;
                PrCampos ahora; PrFotoCampos(testigo, ahora, false);
                for (PrCampos::iterator it = antes.begin(); it != antes.end(); ++it) if (!it->second.Igual(ahora[it->first])) { tocadas++; break; }
            }
        }
    }
    bool ok = (tocadas == 0);
    printf("      [w3dbcorrupto] %d archivos rotos: rechazados=%d aceptados=%d | malla tocada al rechazar=%d\n",
           probadas, rechazadas, aceptadas, tocadas);
    // ---- version mas nueva: se niega y lo dice ----
    const std::vector<unsigned char>& d0 = bins[0].datos;
    std::vector<PrBloque> bl;
    if (!PrPartirW3db(d0, bl)) { delete testigo; err = "w3dbcorrupto: no pude partir la entrada en bloques"; return false; }
    { std::vector<unsigned char> x = PrArmarW3db(bl, 2);
      W3dMallaInfo inf; W3dMallaBinOpciones op;
      const bool leyo = W3dMallaBinLeer(&x[0], x.size(), testigo, &inf, op);
      bool dice = false; for (size_t i = 0; i < inf.avisos.size(); i++) if (inf.avisos[i].find("mas nueva") != std::string::npos) dice = true;
      printf("      [w3dbcorrupto] version 2: %s, aviso 'mas nueva'=%s\n", leyo ? "LA LEYO <-- MAL" : "rechazada", dice ? "si" : "NO");
      if (leyo || !dice) ok = false; }
    // ---- bloque desconocido NO requerido: se preserva al reescribir (y otra vez al releer) ----
    { std::vector<PrBloque> b2 = bl;
      PrBloque x; x.id = "XPRB"; x.dom = W3DB_GLOBAL; x.cant = 3; x.datos = "dato del futuro"; b2.push_back(x);
      std::vector<unsigned char> y = PrArmarW3db(b2, 1);
      Mesh* m = PrMallaSuelta(); W3dMallaInfo inf; W3dMallaBinOpciones op;
      const bool leyo = W3dMallaBinLeer(&y[0], y.size(), m, &inf, op);
      std::string re; std::vector<std::string> av;
      const bool escribio = leyo && W3dMallaBinEscribir(m, NULL, re, &av);
      Mesh* m2 = PrMallaSuelta(); W3dMallaInfo inf2;
      const bool releyo = escribio && W3dMallaBinLeer((const unsigned char*)re.data(), re.size(), m2, &inf2, op);
      const bool viaja = releyo && m2->w3dmAjenos.bloquesBin.size() == 1 && m2->w3dmAjenos.bloquesBin[0].nombre == "XPRB" &&
                         m2->w3dmAjenos.bloquesBin[0].texto == "dato del futuro" && m2->w3dmAjenos.bloquesBin[0].cantidad == 3;
      printf("      [w3dbcorrupto] bloque desconocido XPRB: leyo=%s preservado al reescribir=%s\n", leyo ? "si" : "NO", viaja ? "si" : "NO");
      if (!leyo || !viaja) ok = false;
      // ...y el TEXTO no lo puede llevar: se pierde, pero avisando
      std::string txt; std::vector<std::string> avT;
      W3dMallaEscribir(m2, txt, &avT);
      bool avisa = false; for (size_t i = 0; i < avT.size(); i++) if (avT[i].find("XPRB") != std::string::npos) avisa = true;
      printf("      [w3dbcorrupto] guardarlo en TEXTO avisa que XPRB se pierde: %s\n", avisa ? "si" : "NO");
      if (!avisa) ok = false;
      delete m; delete m2; }
    // ---- bloque desconocido REQUERIDO: abre en solo lectura y guardar encima se FRENA ----
    { std::vector<PrBloque> b2 = bl;
      for (size_t i = 0; i < b2.size(); i++) if (b2[i].id == "REQ ") { b2[i].datos += "XREQ"; b2[i].cant++; }
      PrBloque x; x.id = "XREQ"; x.dom = W3DB_GLOBAL; x.cant = 1; x.datos = "obligatorio"; b2.push_back(x);
      std::vector<unsigned char> y = PrArmarW3db(b2, 1);
      Mesh* m = PrMallaSuelta(); W3dMallaInfo inf; W3dMallaBinOpciones op;
      const bool leyo = W3dMallaBinLeer(&y[0], y.size(), m, &inf, op);
      const std::string falta = W3dMallaBloqueQueFalta(m);
      std::string re; std::vector<std::string> av;
      const bool escribio = W3dMallaBinEscribir(m, NULL, re, &av);
      std::string txt; const bool escribioTxt = W3dMallaEscribir(m, txt, &av);
      printf("      [w3dbcorrupto] REQ con XREQ: leyo=%s soloLectura=%s falta='%s' guardar binario=%s texto=%s\n",
             leyo ? "si" : "NO", inf.soloLectura ? "si" : "NO", falta.c_str(), escribio ? "SE ESCRIBIO <-- MAL" : "frenado",
             escribioTxt ? "SE ESCRIBIO <-- MAL" : "frenado");
      if (!leyo || !inf.soloLectura || falta != "XREQ" || escribio || escribioTxt) ok = false;
      delete m; }
    delete testigo;
    if (!ok) { err = "w3dbcorrupto: el lector no se banca un archivo roto como deberia (ver arriba)"; return false; }
    return true;
}

// ---- w3dbromper <origen.w3d> <destino.w3d> corte|version: copia el contenedor con
//      la PRIMERA .w3db rota (cortada a la mitad, o de una version del formato mas
//      nueva). Al abrirlo esa malla no carga y guardar encima se tiene que FRENAR
//      (W3dAjenos::noCargo), igual que con un .w3dm que no se pudo leer. ----
static bool CmdW3dbRomper(std::istringstream& ss, std::string& err) {
    const std::string orig = PrLeerRuta(ss), dest = PrLeerRuta(ss);
    std::string modo; ss >> modo;
    if (orig.empty() || dest.empty() || (modo != "corte" && modo != "version")) {
        err = "w3dbromper: uso: w3dbromper <origen.w3d> <destino.w3d> corte|version"; return false;
    }
    std::vector<W3dZipEntrada> ents;
    if (!W3dZipLeer(orig, &ents)) { err = "w3dbromper: no pude leer " + orig; return false; }
    W3dZipWriter w;
    for (size_t i = 0; i < ents.size(); i++)   // mimetype PRIMERO (estilo ODF)
        if (ents[i].nombre == "mimetype") w.Agregar(ents[i].nombre, ents[i].datos);
    std::string rota;
    for (size_t i = 0; i < ents.size(); i++) {
        const std::string& n = ents[i].nombre;
        if (n == "mimetype") continue;
        std::vector<unsigned char> d = ents[i].datos;
        const bool esBin = n.size() > 5 && n.compare(n.size() - 5, 5, ".w3db") == 0;
        if (esBin && rota.empty() && d.size() > 24) {
            if (modo == "corte") d.resize(d.size() / 2);
            else { d[4] = 2; d[5] = d[6] = d[7] = 0; }
            rota = n;
        }
        w.Agregar(n, d);
    }
    if (rota.empty()) { err = "w3dbromper: " + orig + " no tiene ninguna .w3db"; return false; }
    if (!w.Guardar(dest)) { err = "w3dbromper: no pude escribir " + dest; return false; }
    printf("      [w3dbromper] %s -> %s: '%s' %s\n", orig.c_str(), dest.c_str(), rota.c_str(),
           modo == "corte" ? "cortada a la mitad" : "con version 2 (del futuro)");
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
static Mesh* PrPrimitiva(Object* padre, MeshType::Enum tipo, const char* nombre, int segs, int anillos) {
    Mesh* m = (Mesh*)NewMesh(MeshType(tipo), padre, false);
    if (segs > 0) { m->meshVerts = segs; m->meshVerts2 = anillos; m->Regenerar(); }
    m->SetNameObj(nombre);
    m->GenerarRender();   // el orden de render-verts CANONICO (el mismo merge que el lector de texto)
    return m;
}
static bool CmdMallaRica(std::istringstream& ss, std::string& err) {
    std::string op; ss >> op;
    ReiniciarEscena();
    PlayAnimation = false;
    Collection* col = new Collection(SceneCollection);
    col->SetNameObj("Rica");
    CollectionActive = col;
    Material* matA = new Material("MatRicaA");
    Material* matB = new Material("MatRicaB");
    matB->diffuse[0] = 0.2f;

    // ---- 1) la malla con TODO ----
    Mesh* m = PrPrimitiva(col, MeshType::UVsphere, "Rica", 12, 6);
    m->PoblarCapas();
    const int nC = m->ContarCorners();
    { UVMap* u2 = new UVMap("UV2"); u2->uv = m->uvMaps[0]->uv;
      for (size_t i = 0; i < u2->uv.size(); i++) u2->uv[i] = u2->uv[i] * 0.5f + 0.25f;
      m->uvMaps.push_back(u2); }
    { ColorLayer* c = new ColorLayer("Mascara"); c->porVertice = true; c->color.resize((size_t)nC * 4);
      for (int L = 0; L < nC; L++) { c->color[(size_t)L*4] = (GLubyte)(L * 7); c->color[(size_t)L*4+1] = (GLubyte)(255 - L * 3);
                                     c->color[(size_t)L*4+2] = (GLubyte)(L * 13); c->color[(size_t)L*4+3] = 255; }
      m->colorLayers.push_back(c); m->colorActivo = (int)m->colorLayers.size() - 1; }
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
    WeightPaintAsegurarMapa(m);
    { VertexGroup* a = new VertexGroup("Hueso1"); VertexGroup* b = new VertexGroup("Hueso2");
      W3dShapeKey sk; sk.nombre = "Boca";
      std::vector<char> hecho(m->vertexSize, 0);
      for (int i = 0; i < m->vertexSize; i++) {
          const int cp = m->vertCtrlPoint[i];
          if (hecho[cp]) continue;
          hecho[cp] = 1;
          if (m->vertex[i*3+1] > 0.0f) { a->verts.push_back(cp); a->pesos.push_back(0.25f + 0.5f * m->vertex[i*3+1]); }
          else { b->verts.push_back(cp); b->pesos.push_back(0.8f); }
          if (m->vertex[i*3] > 0.3f) { sk.idx.push_back(cp); sk.d.push_back(0.1f); sk.d.push_back(0.0f); sk.d.push_back(-0.05f * m->vertex[i*3+2]); }
      }
      m->vertexGroups.push_back(a); m->vertexGroups.push_back(b); m->grupoActivo = 1;
      m->shapeKeys.push_back(sk); m->shapePesos.assign(1, 0.0f); }
    { UVGroup* ug = new UVGroup("Grupo2D");
      for (int i = 0; i < m->vertexSize; i += 3) { ug->verts.push_back(i); ug->pesos.push_back(0.5f); }
      m->uvGroups.push_back(ug); m->uvGrupoActivo = 0;
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
    m->ReagruparMeshParts();
    m->posRep = m->vertCtrlPoint;
    m->CalcularBordes(true, false);
    m->AplicarCapasAlRender();

    // ---- 2) escenario cerrado a edicion ----
    { Mesh* e = PrPrimitiva(col, MeshType::cube, "Escenario", 0, 0);
      e->pos = Vector3(4, 0, 0); e->noEditable = true; e->CalcularAABBSolo(); }

    // ---- 3) un rig con clip + su malla skinneada ----
    { Armature* a = new Armature(col, Vector3(0, 0, 3)); a->name = "RigRico";
      { W3dBone r; r.name = "root";  r.parent = -1; r.head = Vector3(0,0,0); r.tail = Vector3(0,1,0); a->bones.push_back(r); }
      { W3dBone c; c.name = "child"; c.parent = 0;  c.head = Vector3(0,1,0); c.tail = Vector3(0,2,0); a->bones.push_back(c); }
      CrearAnimacion(a);
      SkeletalAnimation* an = a->animations[a->animActiva];
      an->name = "Saludo"; an->FrameRate = 24; an->startFrame = 1; an->endFrame = 20;
      { BoneTrack& tr = an->TrackDe(1);
        AnimProperty& ap = tr.PropertyDe(AnimRotation, AnimZ); SetKeyCurva(ap, 1, 0.0f); SetKeyCurva(ap, 10, 50.0f); SetKeyCurva(ap, 20, 0.0f); }
      PrepararSkinAutorado(a);
      Mesh* s = PrPrimitiva(col, MeshType::cylinder, "Cuerpo", 0, 0);
      s->pos = Vector3(0, 0, 3);
      WeightPaintAsegurarMapa(s);
      VertexGroup* vr = new VertexGroup("root"); VertexGroup* vc = new VertexGroup("child");
      std::vector<char> hecho(s->vertexSize, 0);
      for (int i = 0; i < s->vertexSize; i++) {
          const int cp = s->vertCtrlPoint[i];
          if (hecho[cp]) continue;
          hecho[cp] = 1;
          if (s->vertex[i*3+1] > 0.0f) { vc->verts.push_back(cp); vc->pesos.push_back(1.0f); }
          else { vr->verts.push_back(cp); vr->pesos.push_back(0.7f); }
      }
      s->vertexGroups.push_back(vr); s->vertexGroups.push_back(vc);
      Modifier* md = new Modifier(ModifierType::Armature, NombreTipoModificador(ModifierType::Armature));
      md->target = (Object*)a; s->modificadores.push_back(md);
      s->skinArmature = a; }

    // ---- 4) una vertex anim de dos keyframes (posiciones + normales) ----
    { Mesh* v = PrPrimitiva(col, MeshType::UVsphere, "Onda", 8, 4);
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
    { Mesh* e = PrPrimitiva(col, MeshType::cone, "Espejo", 0, 0);
      e->pos = Vector3(0, 0, -4);
      Modifier* md = new Modifier(ModifierType::Mirror, NombreTipoModificador(ModifierType::Mirror));
      e->modificadores.push_back(md); e->modificadorActivo = 0;
      e->GenerarMallaModificada(); }

    // ---- 6) una tira de flipbook (el guardado deja las UV en su base) ----
    { Mesh* t = PrPrimitiva(col, MeshType::plane, "Tira", 0, 0);
      t->pos = Vector3(0, 3, 0);
      t->SetUVAnimTira(4, 8.0f, 0, 0); }

    // ---- 7) con 'grande': mas de 65535 render-verts (indices de 32 bits) ----
    if (op == "grande") {
        Mesh* g = PrPrimitiva(col, MeshType::UVsphere, "Grande", 300, 222);
        g->pos = Vector3(0, -6, 0);
        printf("      [mallarica] 'Grande': %d render-verts\n", g->vertexSize);
        if (g->vertexSize <= 65535) { err = "mallarica: la malla grande no pasa los 65535 render-verts"; return false; }
    }
    DeseleccionarTodo(); ObjActivo = NULL;
    std::vector<Mesh*> ms; PrJuntarMallas(SceneCollection, ms);
    printf("      [mallarica] %d mallas en 'Rica' ('Rica': %d render-verts, %d caras, %d capas UV, %d de color, "
           "%d grupos, %d formas, %d sueltas)\n", (int)ms.size(), m->vertexSize, (int)m->faces3d.size(),
           (int)m->uvMaps.size(), (int)m->colorLayers.size(), (int)m->vertexGroups.size(), (int)m->shapeKeys.size(),
           (int)(m->looseEdges.size() / 2 + m->looseVerts.size()));
    return true;
}

// ============================================================================
//  formatomallasui: el TILDE de la tarjeta Archivo ("Meshes as text")
// ============================================================================
// La opcion del proyecto "formatoMallas" tiene su control en la UI: un PropBool en la
// tarjeta Archivo que es ESPEJO de g_w3dFormatoMallasProyecto. 'clic' lo toca por la
// MISMA puerta que un click (EditPropertie: da vuelta el bool y llama al onChange);
// 'tilde 0|1' asserta lo que muestra, despues de refrescar como en cada frame
// (Properties::Render -> ActualizarPestanias), y que coincide con el formato vigente.
static bool CmdFormatoMallasUI(std::istringstream& ss, std::string& err) {
    Properties* P = PropsActivo;
    if (!P) {   // el primer panel de propiedades del layout (el activo puede ser otro viewport)
        ViewportBase* h = PrPrimeraHoja(3);
        P = h ? (Properties*)h : NULL;
    }
    if (!P || !P->propProyMallasTexto || !P->propProyMallasTexto->value) {
        err = "formatomallasui: no hay panel de propiedades con el tilde del formato de las mallas"; return false;
    }
    PropBool* pb = P->propProyMallasTexto;
    std::string t;
    while (ss >> t) {
        if (t == "clic") {
            P->ActualizarPestanias();   // lo que la UI mostraba justo antes del click
            pb->EditPropertie();
            printf("      [formatomallasui] clic -> tilde=%d proyecto=%s\n", *pb->value ? 1 : 0,
                   g_w3dFormatoMallasProyecto == W3D_MALLAS_TEXTO ? "texto" : "binario");
        } else if (t == "tilde") {
            int quiero = -1; ss >> quiero;
            P->ActualizarPestanias();
            const int hay = *pb->value ? 1 : 0;
            const int proy = (g_w3dFormatoMallasProyecto == W3D_MALLAS_TEXTO) ? 1 : 0;
            printf("      [formatomallasui] tilde=%d proyecto=%s ('%s')\n", hay, proy ? "texto" : "binario", pb->name.c_str());
            if (hay != proy) { err = "formatomallasui: el tilde no refleja la opcion del proyecto"; return false; }
            if (quiero != hay) { char b[120]; sprintf(b, "formatomallasui: tilde=%d y se esperaba %d", hay, quiero); err = b; return false; }
        } else { err = "formatomallasui: uso: formatomallasui [clic] [tilde 0|1]..."; return false; }
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
    int malos = 0;
    for (int i = 0; kVec[i].s; i++) {
        const unsigned c = W3dZipCrc32((const unsigned char*)kVec[i].s, strlen(kVec[i].s));
        if (c != kVec[i].crc) { printf("      [zipcrc] '%s' -> %08X (se esperaba %08X)\n", kVec[i].s, c, kVec[i].crc); malos++; }
    }
    // datos pseudo-aleatorios deterministas (LCG): offsets 0..8 x largos 0..300, y 1 MB + 7
    std::vector<unsigned char> d((size_t)(1u << 20) + 7u);
    unsigned x = 12345u;
    for (size_t i = 0; i < d.size(); i++) { x = x * 1103515245u + 12345u; d[i] = (unsigned char)(x >> 16); }
    for (size_t off = 0; off <= 8; off++)
        for (size_t n = 0; n <= 300; n++)
            if (W3dZipCrc32(&d[off], n) != PrCrcBitABit(&d[off], n)) malos++;
    if (W3dZipCrc32(&d[3], d.size() - 3) != PrCrcBitABit(&d[3], d.size() - 3)) malos++;
    // velocidad (informativo): 16 pasadas de 1 MB
    const double t0 = W3dNowMs();
    unsigned acc = 0;
    for (int k = 0; k < 16; k++) acc = acc * 31u + W3dZipCrc32(&d[0], d.size());   // (que el compilador no la saltee)
    const double ms = W3dNowMs() - t0;
    printf("      [zipcrc] vectores de control + %d largos/alineaciones contra la definicion: %s | "
           "%.0f MB/s (acc %08X)\n", 9 * 301 + 1, malos ? "MAL" : "OK", ms > 0.0 ? 16.0 / (ms / 1000.0) : 0.0, acc);
    if (malos) { char b[80]; sprintf(b, "zipcrc: %d CRC distintos de la definicion", malos); err = b; return false; }
    return true;
}

// ============================================================================
//  w3dbregistro [contenedor.w3d]: el codigo, el registro y la spec dicen lo mismo
// ============================================================================
// Los fourcc que esta version entiende (W3dMallaBinBloquesConocidos) tienen que ser
// EXACTAMENTE las filas "w3db:" vivas de formato/bloques.tsv, y cada uno tiene que estar
// documentado en formato/w3db.md. Con un contenedor, ademas, todo bloque de todas sus
// .w3db tiene que estar registrado. formato/ es gitignoreado: sin la carpeta se saltea
// avisando "SIN CORPUS:" (como las partes del corpus de oro; W3D_CORPUS_OBLIGATORIO=1
// la exige).
static bool PrLeerTexto(const std::string& ruta, std::string& out) {
    std::vector<unsigned char> b;
    if (!w3dFileSystem::ReadFileBytes(ruta, b)) return false;
    out.assign(b.empty() ? "" : (const char*)&b[0], b.size());
    return true;
}
static bool CmdW3dbRegistro(std::istringstream& ss, std::string& err) {
    const std::string cont = PrLeerRuta(ss);
    static const char* kBases[] = { "formato", "../formato", "../../formato", "../../../formato", "../../../../formato", 0 };
    std::string base;
    for (int i = 0; kBases[i] && base.empty(); i++)
        if (w3dFileSystem::FileExists(std::string(kBases[i]) + "/bloques.tsv")) base = kBases[i];
    if (base.empty()) {
        const char* obl = getenv("W3D_CORPUS_OBLIGATORIO");
        if (obl && *obl && strcmp(obl, "0") != 0) { err = "w3dbregistro: no encuentro formato/bloques.tsv"; return false; }
        printf("      [w3dbregistro] SIN CORPUS: salteo el cruce con formato/bloques.tsv y formato/w3db.md "
               "(formato/ no esta: es gitignoreado; W3D_CORPUS_OBLIGATORIO=1 lo exige)\n");
        return true;
    }
    std::string tsv, md;
    if (!PrLeerTexto(base + "/bloques.tsv", tsv)) { err = "w3dbregistro: no pude leer " + base + "/bloques.tsv"; return false; }
    if (!PrLeerTexto(base + "/w3db.md", md))      { err = "w3dbregistro: no pude leer " + base + "/w3db.md"; return false; }
    // las filas w3db: (el '_' final es el espacio de un fourcc de 3 letras)
    std::set<std::string> registrados, obsoletos;
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
          if (f.size() != 4 || col.size() < 6) { printf("      [w3dbregistro] fila rota: '%s'\n", ln.c_str()); err = "w3dbregistro: una fila w3db: no es <fourcc> + 5 columnas"; return false; }
          if (col[4] == "obsoleto") obsoletos.insert(f); else registrados.insert(f);
      } }
    std::vector<std::string> conocidos, requeridos;
    W3dMallaBinBloquesConocidos(conocidos, &requeridos);
    int malos = 0;
    for (size_t i = 0; i < conocidos.size(); i++) {
        const std::string& f = conocidos[i];
        if (!registrados.count(f)) { printf("      [w3dbregistro] '%s' lo entiende el codigo y NO esta en bloques.tsv\n", f.c_str()); malos++; }
        if (md.find("`" + f + "`") == std::string::npos) { printf("      [w3dbregistro] '%s' no esta documentado en w3db.md\n", f.c_str()); malos++; }
    }
    for (std::set<std::string>::const_iterator it = registrados.begin(); it != registrados.end(); ++it)
        if (!W3dMallaBinBloqueConocido(*it)) { printf("      [w3dbregistro] '%s' esta registrado vivo y el codigo no lo entiende\n", it->c_str()); malos++; }
    for (size_t i = 0; i < requeridos.size(); i++)
        if (!W3dMallaBinBloqueConocido(requeridos[i])) { printf("      [w3dbregistro] el requerido '%s' no es un bloque conocido\n", requeridos[i].c_str()); malos++; }
    // con un contenedor: todo bloque de sus .w3db esta registrado
    int nBin = 0, nTxt = 0, bloques = 0;
    if (!cont.empty()) {
        std::vector<W3dZipEntrada> bins;
        if (!PrEntradasMallas(cont, bins, nBin, nTxt)) { err = "w3dbregistro: no pude leer " + cont; return false; }
        for (size_t i = 0; i < bins.size(); i++) {
            std::vector<std::string> lineas;
            if (bins[i].datos.empty() || !W3dMallaBinListar(&bins[i].datos[0], bins[i].datos.size(), lineas)) continue;
            for (size_t k = 0; k < lineas.size(); k++) {
                bloques++;
                const std::string f = lineas[k].substr(0, 4);
                if (!registrados.count(f)) { printf("      [w3dbregistro] %s escribe '%s', que no esta registrado\n", bins[i].nombre.c_str(), f.c_str()); malos++; }
            }
        }
    }
    printf("      [w3dbregistro] codigo=%d registrados=%d (obsoletos %d) requeridos=%d -> %s\n",
           (int)conocidos.size(), (int)registrados.size(), (int)obsoletos.size(), (int)requeridos.size(),
           malos ? "MAL" : "OK");
    if (!cont.empty()) printf("      [w3dbregistro] %s: %d .w3db, %d bloques mirados\n", cont.c_str(), nBin, bloques);
    if (!cont.empty() && nBin == 0) { err = "w3dbregistro: el contenedor no tiene ninguna .w3db para mirar"; return false; }
    if (malos) { err = "w3dbregistro: el codigo, formato/bloques.tsv y formato/w3db.md no coinciden (ver arriba)"; return false; }
    return true;
}

// ============================================================================
//  juego3dbool / juego3dboolpx: el TARGET de un Boolean en el juego compilado
// ============================================================================
// El runtime lee cada .w3db SOLO con los bloques de render. Una malla cuyo stack no genera
// malla queda con la edicion PENDIENTE (Mesh::edicionPendiente) y ConstruirPolyMesh la
// materializa recien cuando alguien pide sus poligonos: el target de un Boolean. Si esa
// materializacion se rompe, el Boolean se queda sin poligonos del target y NO CORTA
// NADA, en silencio: el juego se ve "casi" bien. juego3dbool arma, sobre el juego 3D
// minimo (juego3dmin), un Boolean DIFFERENCE en el cubo con un target OCULTO que lo
// perfora de adelante hacia atras (arriba del centro, lejos de la franja del HUD), y
// verifica el agujero en el editor. juego3dboolpx corre el binario compilado y mira por
// el agujero: tiene que verse el FONDO y no la cara del cubo.
static bool CmdJuego3DBool(std::istringstream& ss, std::string& err) {
    const std::string dir = PrLeerRuta(ss);
    if (dir.empty()) { err = "juego3dbool: uso: juego3dbool <carpeta>"; return false; }
    std::string e2;
    if (!W3dRunCommand("juego3dmin " + dir, e2)) { err = "juego3dbool: " + e2; return false; }
    Object* oc = SceneCollection ? FindObjectByName(SceneCollection, "Cubo") : NULL;
    if (!oc || oc->getType() != ObjectType::mesh) { err = "juego3dbool: juego3dmin no dejo el 'Cubo'"; return false; }
    Mesh* cubo = (Mesh*)oc;
    // el TARGET: una caja que atraviesa el cubo (lado 4, centrado) de punta a punta en Z,
    // de 1,6 x 1,6 y corrida +1 en Y. OCULTA: no se dibuja, solo corta.
    Mesh* hueco = (Mesh*)NewMesh(MeshType(MeshType::cube), cubo->Parent, false);
    if (!hueco) { err = "juego3dbool: NewMesh fallo"; return false; }
    hueco->SetNameObj("Hueco");
    hueco->pos = Vector3(0.0f, 1.0f, 0.0f);
    hueco->scale = Vector3(0.8f, 0.8f, 4.0f);
    hueco->visible = false;
    { extern bool g_objetosMovidos; g_objetosMovidos = true; }
    // LOS TRES CUBOS SON LA MISMA MALLA del proyecto (un recurso, como duplicados vinculados):
    // en el juego el cubo que corta la materializa al abrir (su stack genera malla) y el target,
    // sin modificadores, la deja PENDIENTE hasta que el Boolean pide sus poligonos. (Tres mallas
    // nuevas iguales ya no se juntan solas al guardar: hay que vincularlas.)
    MallaRecurso* rec = W3dMallaCrearRecurso(cubo, "Cubo");
    if (!rec || !W3dMallaVincular(hueco, rec)) { err = "juego3dbool: no pude vincular el target a la malla del cubo"; return false; }
    Object* afuera = FindObjectByName(SceneCollection, "CuboAfuera");
    if (afuera && afuera->getType() == ObjectType::mesh && !W3dMallaVincular((Mesh*)afuera, rec)) {
        err = "juego3dbool: no pude vincular el testigo a la malla del cubo"; return false;
    }
    Modifier* md = new Modifier(ModifierType::Boolean, NombreTipoModificador(ModifierType::Boolean));
    md->boolOp = 2;   // Difference
    md->target = hueco;
    cubo->modificadores.push_back(md);
    cubo->modificadorActivo = (int)cubo->modificadores.size() - 1;
    cubo->GenerarMallaModificada();
    // en el EDITOR ya hay agujero: la malla generada tiene vertices en su borde (espacio
    // LOCAL del cubo, escala 2: x = +-0,4 e y = 0,1 .. 0,9 en la cara de adelante z = 1)
    int bordes = 0;
    if (cubo->genValido && cubo->genVertex)
        for (int i = 0; i < cubo->genVertexSize; i++) {
            const float x = cubo->genVertex[i * 3], y = cubo->genVertex[i * 3 + 1], z = cubo->genVertex[i * 3 + 2];
            if (fabsf(fabsf(x) - 0.4f) < 1e-3f && (fabsf(y - 0.1f) < 1e-3f || fabsf(y - 0.9f) < 1e-3f) && fabsf(z - 1.0f) < 1e-3f) bordes++;
        }
    printf("      [juego3dbool] Boolean DIFFERENCE en 'Cubo' con target oculto 'Hueco': malla generada %s, "
           "%d render-verts, %d esquinas del agujero en la cara de adelante\n",
           cubo->genValido ? "OK" : "NO", cubo->genValido ? cubo->genVertexSize : 0, bordes);
    if (!cubo->genValido || bordes < 4) { err = "juego3dbool: el Boolean no perforo el cubo en el editor"; return false; }
    DeseleccionarTodo(); ObjActivo = NULL;
    return true;
}

static bool CmdJuego3DBoolPx(std::istringstream& ss, std::string& err) {
    std::string dir, nombre; ss >> dir >> nombre;
    if (dir.empty() || nombre.empty()) { err = "juego3dboolpx: uso: juego3dboolpx <carpeta> <nombre>"; return false; }
    const std::string bin = dir + "/build/linux/" + nombre;
    if (!w3dFileSystem::FileExists(bin)) { err = "juego3dboolpx: no existe el binario compilado '" + bin + "'"; return false; }
    const std::string png = dir + "/captura.png";
    remove(png.c_str());
    char cmdRun[2200];
    snprintf(cmdRun, sizeof(cmdRun),
             "cd \"%s/build/linux\" && W3D_CAPTURA=\"%s\" W3D_CAPTURA_FRAME=20 timeout 120 ./%s > /dev/null 2>&1",
             dir.c_str(), png.c_str(), nombre.c_str());
    if (system(cmdRun) != 0) { err = "juego3dboolpx: el juego no corrio"; return false; }
    unsigned char* px = 0; int iw = 0, ih = 0;
    if (!w3dEngine::DecodeImage(png.c_str(), &px, &iw, &ih) || !px || iw < 8 || ih < 8) {
        err = "juego3dboolpx: el juego no dejo la captura (no dibujo ningun frame)"; return false;
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
    int fondo[3], hueco[3], cara[3];
    LP::Get(px, iw, ih, 0.50f, 0.04f, fondo);   // adentro del encuadre, arriba del cubo
    LP::Get(px, iw, ih, 0.50f, 0.30f, hueco);   // mirando por el agujero (se ve de punta a punta)
    LP::Get(px, iw, ih, 0.35f, 0.30f, cara);    // la misma altura, fuera del agujero: la cara del cubo
    w3dEngine::FreeImage(px);
    const bool okCara  = LP::Dif(cara, fondo) >= 40;    // el cubo se dibuja y se distingue del fondo
    const bool okHueco = LP::Dif(hueco, fondo) <= 24;   // por el agujero se ve el fondo
    printf("      [juego3dboolpx] %dx%d | fondo rgb=%d,%d,%d | agujero rgb=%d,%d,%d %s | cara rgb=%d,%d,%d %s\n",
           iw, ih, fondo[0], fondo[1], fondo[2],
           hueco[0], hueco[1], hueco[2], okHueco ? "OK" : "<-- MAL (el Boolean no corto: el target no tenia poligonos)",
           cara[0], cara[1], cara[2], okCara ? "OK" : "<-- MAL (el cubo no se distingue del fondo)");
    if (!okCara || !okHueco) {
        err = "juego3dboolpx: el juego compilado no dibuja el agujero del Boolean (ver " + png + ")"; return false;
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
//      memoria y no entra en el .w3db (W3dMallaBinIndicesAlcanzan): sale en texto, que guarda
//      las caras con int. En el PC no hay nada truncado; lo que se prueba es la puerta del
//      guardado (EscribirMallaW3dm). La negativa del escritor binario se prueba compilando
//      el Core con W3D_SYMBIAN. ----
static bool CmdIndices16(std::istringstream& ss, std::string& err) {
    int on = -1; ss >> on;
    if (on != 0 && on != 1) { err = "indices16: uso: indices16 0|1"; return false; }
    g_w3dIndices16Simulado = (on == 1);
    printf("      [indices16] guardado de mallas %s (MeshIndex de este build: %d bits)\n",
           on ? "COMO una plataforma de indices de 16 bits" : "normal", (int)sizeof(MeshIndex) * 8);
    return true;
}

// ---- w3dbjuego <ruta.w3d>: la EDICION de las .w3db en el JUEGO COMPILADO ----
//      El juego lee cada .w3db SOLO con sus bloques de render. Los de edicion (caras, capas,
//      normales por esquina, marcas, aristas, posRep) quedan PENDIENTES y se leen recien si
//      el stack de la malla GENERA malla (al cargar, de los bytes que ya estan en memoria) o
//      si alguien pide sus poligonos (ConstruirPolyMesh: el Boolean que la usa de target).
//      Un stack que no genera nada (solo el PVS/Oclusion, o un modificador apagado en el
//      viewport) NO los carga: la malla mas grande de un nivel con PVS no puede entrar al
//      juego con toda su edicion en memoria sin que nadie la use.
//      Escena NUEVA con cuatro cubos (sin modificadores, solo Oclusion, un Mirror apagado y
//      un Mirror encendido), guardada en binario y abierta COMO EL JUEGO
//      (g_w3dMallasComoJuego): los tres primeros quedan pendientes y el cuarto trae su
//      edicion y su malla generada. Despues, lo pendiente se materializa por ConstruirPolyMesh
//      y guardar lo abierto asi materializa lo que falte: las mallas salen IDENTICAS
//      (<ruta>_2.w3d). Deja la escena por defecto (la abierta como el juego no es editable).
struct PrCasoJuego { const char* nombre; int tipo; bool viewport; bool pendiente; };
static bool PrSinEdicion(const Mesh* m) {
    return m->faces3d.empty() && m->looseEdges.empty() && m->cornerNormal.empty() && m->uvMaps.empty() &&
           m->colorLayers.empty() && m->sharpEdges.empty() && m->seamEdges.empty() && m->edges.empty() &&
           m->bordesBuf.empty() && m->posRep.empty();
}
static bool CmdW3dbJuego(std::istringstream& ss, std::string& err) {
    const std::string ruta = PrLeerRuta(ss);
    if (ruta.size() < 5 || ruta.compare(ruta.size() - 4, 4, ".w3d") != 0) { err = "w3dbjuego: uso: w3dbjuego <ruta.w3d>"; return false; }
    const std::string ruta2 = ruta.substr(0, ruta.size() - 4) + "_2.w3d";
    static const PrCasoJuego kCasos[] = {
        { "Comun",         -1,                            true,  true  },
        { "SoloOclusion",  (int)ModifierType::CullingTri, true,  true  },
        { "EspejoApagado", (int)ModifierType::Mirror,     false, true  },
        { "Espejo",        (int)ModifierType::Mirror,     true,  false },
    };
    const int nCasos = (int)(sizeof(kCasos) / sizeof(kCasos[0]));
    ReiniciarEscena();
    PlayAnimation = false;
    Collection* col = new Collection(SceneCollection);
    col->SetNameObj("Juego");
    CollectionActive = col;
    for (int i = 0; i < nCasos; i++) {
        Mesh* m = PrPrimitiva(col, MeshType::cube, kCasos[i].nombre, 0, 0);
        m->pos = Vector3(3.0f * (float)i, 0.0f, 0.0f);
        if (kCasos[i].tipo < 0) continue;
        Modifier* md = new Modifier(kCasos[i].tipo, NombreTipoModificador(kCasos[i].tipo));
        md->mostrarViewport = kCasos[i].viewport;
        m->modificadores.push_back(md);
        m->modificadorActivo = 0;
        m->GenerarMallaModificada();
    }
    DeseleccionarTodo(); ObjActivo = NULL;
    const int forzadoAntes = g_w3dFormatoMallasForzado;
    g_w3dFormatoMallasForzado = W3D_MALLAS_BINARIO;
    const bool guardo = GuardarW3D(ruta);
    g_w3dFormatoMallasForzado = forzadoAntes;
    if (!guardo) { err = "w3dbjuego: no pude guardar " + ruta; return false; }

    // ---- ABRIR COMO EL JUEGO ----
    g_w3dMallasComoJuego = true;
    AbrirProyectoAhora(ruta);
    g_w3dMallasComoJuego = false;
    std::string motivo;
    for (int i = 0; i < nCasos && motivo.empty(); i++) {
        Object* o = SceneCollection ? FindObjectByName(SceneCollection, kCasos[i].nombre) : NULL;
        if (!o || o->getType() != ObjectType::mesh) { motivo = std::string("'") + kCasos[i].nombre + "' no volvio"; break; }
        Mesh* m = (Mesh*)o;
        const bool pend = !m->edicionPendiente.empty();
        const char* stack = kCasos[i].tipo < 0 ? "sin modificadores" :
                            kCasos[i].tipo == (int)ModifierType::CullingTri ? "solo Oclusion" :
                            kCasos[i].viewport ? "Mirror" : "Mirror apagado";
        printf("      [w3dbjuego] '%s' (%s): edicion %s | caras=%d capasUV=%d aristas=%d posRep=%d | "
               "render %d verts %d tris | generada=%s\n", kCasos[i].nombre, stack, pend ? "PENDIENTE" : "en memoria",
               (int)m->faces3d.size(), (int)m->uvMaps.size(), (int)(m->edges.size() / 2), (int)m->posRep.size(),
               m->vertexSize, m->facesSize / 3, m->genValido ? "si" : "no");
        const std::string q = std::string("'") + kCasos[i].nombre + "' (" + stack + "): ";
        if (m->vertexSize <= 0 || m->facesSize <= 0) motivo = q + "no trajo su malla de render";
        else if (kCasos[i].pendiente && (!pend || !PrSinEdicion(m)))
            motivo = q + "cargo sus datos de edicion y su stack no genera malla (en el juego nadie los usa)";
        else if (!kCasos[i].pendiente && (pend || m->faces3d.size() != 6 || m->edges.empty() || (int)m->posRep.size() != m->vertexSize))
            motivo = q + "su stack genera malla y la edicion no quedo en memoria";
        else if (!kCasos[i].pendiente && !m->genValido) motivo = q + "la malla generada del stack no se armo";
        else if (kCasos[i].pendiente && m->genValido) motivo = q + "armo una malla generada con un stack que no genera";
    }
    // lo pendiente sigue andando: el primer ConstruirPolyMesh (el Boolean que la usa de target) lo trae
    if (motivo.empty()) {
        Mesh* m = (Mesh*)FindObjectByName(SceneCollection, "SoloOclusion");
        PolyMesh W;
        ConstruirPolyMesh(m, W);
        printf("      [w3dbjuego] 'SoloOclusion' tras ConstruirPolyMesh: caras=%d poligonos=%d pendiente=%s\n",
               (int)m->faces3d.size(), (int)W.F.size(), m->edicionPendiente.empty() ? "no" : "SI");
        if (!m->edicionPendiente.empty() || m->faces3d.size() != 6 || W.F.size() != 6)
            motivo = "'SoloOclusion': ConstruirPolyMesh no materializo la edicion pendiente";
    }
    // guardar lo abierto como el juego: lo pendiente se materializa ANTES de escribir (sin
    // eso las mallas saldrian sin caras) y las mallas del contenedor salen identicas
    if (motivo.empty()) {
        g_w3dFormatoMallasForzado = W3D_MALLAS_BINARIO;
        const bool g2 = GuardarW3D(ruta2);
        g_w3dFormatoMallasForzado = forzadoAntes;
        std::string e2;
        if (!g2) motivo = "no pude guardar la escena abierta como el juego";
        else if (!W3dRunCommand("w3diguales " + ruta + " " + ruta2 + " mallas/", e2))
            motivo = "guardar lo abierto como el juego no da las mismas mallas: " + e2;
    }
    // la escena abierta como el juego no es para editar: se cierra
    extern void W3dCrearEscenaPorDefecto();
    ReiniciarEscena();
    W3dCrearEscenaPorDefecto();
    if (!motivo.empty()) { err = "w3dbjuego: " + motivo; return false; }
    printf("      [w3dbjuego] solo el stack que genera malla carga la edicion; lo pendiente se materializa al pedirlo y al guardar\n");
    return true;
}

// ---- w3dbmarcas <ruta.w3d>: las CLAVES de SHRP/SEAM de cada .w3db del contenedor ----
//      Cada clave son las dos posiciones de la arista, 3 f32 LITTLE-ENDIAN cada una, la
//      MENOR primero comparando esos bytes: el archivo es el mismo en cualquier host. En
//      memoria la clave (Mesh::SharpEdgeKey) ordena las mitades comparando los bytes DEL
//      HOST, que en uno big-endian dan otro orden: por eso el lector la rearma desde los
//      floats y no depende del orden de las mitades en el archivo. Una .w3db con TODAS las
//      mitades dadas vuelta (la que escribiria un host big-endian copiando su clave de
//      memoria) abre con las MISMAS marcas y se reescribe con los mismos bytes. ----
static bool CmdW3dbMarcas(std::istringstream& ss, std::string& err) {
    const std::string ruta = PrLeerRuta(ss);
    if (ruta.empty()) { err = "w3dbmarcas: uso: w3dbmarcas <ruta.w3d>"; return false; }
    std::vector<W3dZipEntrada> bins; int nBin = 0, nTxt = 0;
    if (!PrEntradasMallas(ruta, bins, nBin, nTxt) || bins.empty()) { err = "w3dbmarcas: " + ruta + " no tiene mallas .w3db"; return false; }
    int conMarcas = 0, claves = 0;
    for (size_t e = 0; e < bins.size(); e++) {
        const std::vector<unsigned char>& d = bins[e].datos;
        const std::string& nom = bins[e].nombre;
        std::vector<PrBloque> bl;
        if (!PrPartirW3db(d, bl)) { err = "w3dbmarcas: no pude partir " + nom; return false; }
        bool hay = false;
        for (size_t k = 0; k < bl.size(); k++) if (bl[k].id == "SHRP" || bl[k].id == "SEAM") hay = true;
        if (!hay) continue;
        conMarcas++;
        std::string motivo;
        Mesh* A = PrMallaSuelta(); W3dMallaInfo iA;
        Mesh* B = PrMallaSuelta(); W3dMallaInfo iB;
        W3dMallaBinOpciones op;
        if (!W3dMallaBinLeer(&d[0], d.size(), A, &iA, op)) motivo = "no se lee";
        // las posiciones de la malla en little-endian: cada mitad tiene que ser una de estas
        std::set<std::string> posLE;
        for (int i = 0; motivo.empty() && i < A->vertexSize; i++) {
            std::string k;
            for (int q = 0; q < 3; q++) { unsigned u; memcpy(&u, &A->vertex[i * 3 + q], 4); PrPonU32(k, u); }
            posLE.insert(k);
        }
        std::vector<PrBloque> vuelta = bl;
        int distintas = 0;
        for (size_t k = 0; k < vuelta.size() && motivo.empty(); k++) {
            PrBloque& b = vuelta[k];
            if (b.id != "SHRP" && b.id != "SEAM") continue;
            if (b.datos.size() != (size_t)b.cant * 24) { motivo = b.id + " no mide 24 bytes por clave"; break; }
            for (unsigned c = 0; c < b.cant; c++) {
                const std::string h1 = b.datos.substr((size_t)c * 24, 12), h2 = b.datos.substr((size_t)c * 24 + 12, 12);
                if (memcmp(h1.data(), h2.data(), 12) > 0) { motivo = b.id + ": una clave trae la mitad MAYOR primero"; break; }
                if (!posLE.count(h1) || !posLE.count(h2)) { motivo = b.id + ": una mitad no es la posicion (little-endian) de ningun vertice"; break; }
                if (h1 != h2) distintas++;
                b.datos.replace((size_t)c * 24, 24, h2 + h1);   // las mitades dadas vuelta
                claves++;
            }
        }
        if (motivo.empty() && distintas == 0) motivo = "ninguna clave tiene dos mitades distintas para dar vuelta";
        // la version con las mitades dadas vuelta: las MISMAS marcas y los MISMOS bytes al reescribir
        if (motivo.empty()) {
            const std::vector<unsigned char> d2 = PrArmarW3db(vuelta, W3DB_VERSION);
            std::string reA, reB;
            if (!W3dMallaBinLeer(&d2[0], d2.size(), B, &iB, op)) motivo = "con las mitades dadas vuelta no se lee";
            else if (B->sharpEdges != A->sharpEdges || B->seamEdges != A->seamEdges)
                motivo = "el lector depende del orden de las mitades en el archivo (las claves no son las del host)";
            else if (!W3dMallaBinEscribir(A, NULL, reA) || !W3dMallaBinEscribir(B, NULL, reB)) motivo = "no se reescribe";
            else if (reA != reB) motivo = "reescrita desde las mitades dadas vuelta no da los mismos bytes";
            else {
                // y el reescrito trae las claves ORIGINALES (el escritor pone la menor primero)
                std::vector<unsigned char> r(reA.begin(), reA.end());
                std::vector<PrBloque> blR;
                if (!PrPartirW3db(r, blR)) motivo = "no pude partir la reescrita";
                for (size_t k = 0; k < bl.size() && motivo.empty(); k++) {
                    if (bl[k].id != "SHRP" && bl[k].id != "SEAM") continue;
                    bool igual = false;
                    for (size_t q = 0; q < blR.size(); q++) if (blR[q].id == bl[k].id) igual = (blR[q].datos == bl[k].datos);
                    if (!igual) motivo = bl[k].id + " reescrito no trae las mismas claves";
                }
            }
        }
        printf("      [w3dbmarcas] %s: %d sharp + %d seam | mitades en orden y en little-endian, lector y escritor "
               "independientes del orden: %s\n", nom.c_str(), (int)A->sharpEdges.size(), (int)A->seamEdges.size(),
               motivo.empty() ? "OK" : motivo.c_str());
        delete A; delete B;
        if (!motivo.empty()) { err = "w3dbmarcas: " + nom + ": " + motivo; return false; }
    }
    if (conMarcas == 0) { err = "w3dbmarcas: ninguna .w3db de " + ruta + " tiene SHRP/SEAM para mirar"; return false; }
    printf("      [w3dbmarcas] %d .w3db con marcas, %d claves\n", conMarcas, claves);
    return true;
}

// ---- w3dbajenos <prefijo>: la PRESERVACION del .w3dm de texto a traves del binario (AJEN) ----
//      Un .w3dm escrito por un Whisk3D mas nuevo trae bloques que esta version no entiende
//      (uno de cara y uno global) y un "requiere" que no es el default. Guardado en binario
//      (el default) viajan en el bloque AJEN; abierto y vuelto a guardar en texto, el .w3dm
//      sale IDENTICO al original. Y la regla del SELLO: si la topologia cambio, el bloque de
//      cara se DESCARTA avisando y el global y el "requiere" siguen. Escribe <prefijo>_t.w3d
//      (texto), <prefijo>_b.w3d (binario) y <prefijo>_t2.w3d (texto de nuevo). Deja la
//      escena por defecto. ----
static bool PrEntradaMalla(const std::string& ruta, const std::string& ext, std::string& datos) {
    std::vector<W3dZipEntrada> ents;
    if (!W3dZipLeer(ruta, &ents)) return false;
    for (size_t i = 0; i < ents.size(); i++) {
        const std::string& n = ents[i].nombre;
        if (n.compare(0, 7, "mallas/") != 0 || n.size() < ext.size() || n.compare(n.size() - ext.size(), ext.size(), ext) != 0) continue;
        datos.assign(ents[i].datos.begin(), ents[i].datos.end());
        return true;
    }
    return false;
}
static bool CmdW3dbAjenos(std::istringstream& ss, std::string& err) {
    const std::string pre = PrLeerRuta(ss);
    if (pre.empty()) { err = "w3dbajenos: uso: w3dbajenos <prefijo>"; return false; }
    const std::string rT = pre + "_t.w3d", rB = pre + "_b.w3d", rT2 = pre + "_t2.w3d";
    const int forzadoAntes = g_w3dFormatoMallasForzado;
    ReiniciarEscena();
    PlayAnimation = false;
    Collection* col = new Collection(SceneCollection);
    col->SetNameObj("Ajenos");
    CollectionActive = col;
    // el .w3dm "del futuro": el de un cubo, con un "requiere" de bloques conocidos que no es el
    // default y dos bloques que esta version no conoce (uno de cara, uno global)
    Mesh* m = PrPrimitiva(col, MeshType::cube, "Futuro", 0, 0);
    for (size_t g = 0; g < m->materialsGroup.size(); g++) m->materialsGroup[g].material = NULL;
    std::string txt;
    W3dMallaEscribir(m, txt, 0);
    const std::string reqDef = "requiere V F\n";
    const size_t pr = txt.find(reqDef);
    if (pr == std::string::npos) { err = "w3dbajenos: el .w3dm del cubo no trae 'requiere V F'"; return false; }
    txt.replace(pr, reqDef.size(), "requiere V F NRM UV\n");
    txt += "\n# bloques de una version mas nueva de Whisk3D\n";
    txt += "X.PC2.PLIEGUE 2 cara peso=alto\n 0 0.25\n 1 0.75\nEND\n";
    txt += "X.PC2.NOTA 1 global\n hecho en 2029\nEND\n";
    { W3dMallaInfo inf;
      if (!W3dMallaLeer(txt.data(), txt.size(), m, &inf) || inf.soloLectura) { err = "w3dbajenos: no pude leer el .w3dm del futuro"; return false; }
      // el cierre de la carga clasica (lo que hace AbrirEscenaJson con un .w3dm)
      m->ReagruparMeshParts();
      m->CalcularBordes(true, false);
      m->AplicarCapasAlRender();
      WeightPaintAsegurarMapa(m); }
    if (m->w3dmAjenos.bloques.size() != 2 || m->w3dmAjenos.requiere.size() != 4) {
        err = "w3dbajenos: el lector de texto no preservo los bloques ajenos / el requiere"; return false;
    }
    DeseleccionarTodo(); ObjActivo = NULL;
    std::string motivo, e0, eb, e2;
    // 1) texto: la referencia
    g_w3dFormatoMallasForzado = W3D_MALLAS_TEXTO;
    if (!GuardarW3D(rT) || !PrEntradaMalla(rT, ".w3dm", e0)) motivo = "no pude guardar el texto de referencia";
    else if (e0.find("requiere V F NRM UV\n") == std::string::npos || e0.find("X.PC2.PLIEGUE") == std::string::npos ||
             e0.find("X.PC2.NOTA") == std::string::npos) motivo = "el .w3dm de referencia no trae el requiere o los bloques ajenos";
    // 2) binario: van en AJEN
    if (motivo.empty()) {
        g_w3dFormatoMallasForzado = W3D_MALLAS_BINARIO;
        std::vector<std::string> lineas;
        if (!GuardarW3D(rB) || !PrEntradaMalla(rB, ".w3db", eb)) motivo = "no pude guardar el binario";
        else if (!W3dMallaBinListar((const unsigned char*)eb.data(), eb.size(), lineas)) motivo = "la .w3db no se lista";
        else {
            bool ajen = false;
            for (size_t i = 0; i < lineas.size(); i++) if (lineas[i].compare(0, 5, "AJEN ") == 0) ajen = true;
            if (!ajen) motivo = "la .w3db no trae el bloque AJEN";
        }
    }
    // 3) abrir el binario y volver a texto: el MISMO .w3dm
    Mesh* mb = NULL;
    if (motivo.empty()) {
        AbrirProyectoAhora(rB);
        Object* o = SceneCollection ? FindObjectByName(SceneCollection, "Futuro") : NULL;
        mb = (o && o->getType() == ObjectType::mesh) ? (Mesh*)o : NULL;
        // (la malla de un RECURSO abre con la edicion pendiente: los bloques ajenos de texto
        // viajan en la edicion, asi que se materializa como lo haria el editor al tocarla)
        if (mb && mb->malla && !mb->edicionPendiente.empty()) W3dMallaBinMaterializarEdicion(mb);
        if (!mb) motivo = "la malla no volvio del binario";
        else if (mb->w3dmAjenos.bloques.size() != 2 || mb->w3dmAjenos.requiere.size() != 4)
            motivo = "el binario no devolvio los bloques ajenos / el requiere";
        else {
            g_w3dFormatoMallasForzado = W3D_MALLAS_TEXTO;
            if (!GuardarW3D(rT2) || !PrEntradaMalla(rT2, ".w3dm", e2)) motivo = "no pude guardar el texto de vuelta";
            else if (e2 != e0) motivo = "texto -> binario -> texto no da el mismo .w3dm";
        }
    }
    printf("      [w3dbajenos] texto -> binario (AJEN, %d B) -> texto: %s (%d B)\n", (int)eb.size(),
           motivo.empty() ? "el MISMO .w3dm" : motivo.c_str(), (int)e2.size());
    // 4) el SELLO: sin una cara, el bloque de cara se descarta avisando; el global y el requiere siguen
    if (motivo.empty()) {
        mb->faces3d.pop_back();
        W3dMallaBinIndices canon;
        W3dMallaIndicesCanonicos(mb, canon);
        std::string re; std::vector<std::string> av;
        Mesh* c = PrMallaSuelta(); W3dMallaInfo ic; W3dMallaBinOpciones op;
        int avPliegue = 0;
        const bool escribio = W3dMallaBinEscribir(mb, &canon, re, &av);
        for (size_t i = 0; i < av.size(); i++) if (av[i].find("X.PC2.PLIEGUE") != std::string::npos) avPliegue++;
        const bool leyo = escribio && W3dMallaBinLeer((const unsigned char*)re.data(), re.size(), c, &ic, op);
        const bool quedaNota = leyo && c->w3dmAjenos.bloques.size() == 1 && c->w3dmAjenos.bloques[0].nombre == "X.PC2.NOTA" &&
                               c->w3dmAjenos.requiere == mb->w3dmAjenos.requiere;
        printf("      [w3dbajenos] sin una cara: escribio=%s aviso del bloque de cara=%d | vuelve solo el global + el requiere=%s\n",
               escribio ? "si" : "NO", avPliegue, quedaNota ? "si" : "NO");
        if (!escribio || avPliegue != 1 || !quedaNota) motivo = "la regla del sello no se cumple en AJEN";
        delete c;
    }
    g_w3dFormatoMallasForzado = forzadoAntes;
    extern void W3dCrearEscenaPorDefecto();
    ReiniciarEscena();
    W3dCrearEscenaPorDefecto();
    if (!motivo.empty()) { err = "w3dbajenos: " + motivo; return false; }
    return true;
}

// ============================================================================
//  EL DESPACHADOR (lo llama W3dRunCommand antes de su cadena de comandos)
// ============================================================================
bool W3dPruebasMallasCmd(const std::string& cmd, std::istringstream& ss, std::string& err, bool& manejado);
bool W3dPruebasRecursosCmd(const std::string& cmd, std::istringstream& ss,
                           std::string& err, bool& manejado) {
    // los ANIMSETS (test/W3dPruebasAnims.cpp) van primero: antes de CADA comando VERIFICAN
    // (sellos) que ningun clip compartido haya cambiado y, si cambio, el comando falla. No
    // copian ni reparan nada: el editor copia solo al escribir (no hay red por frame)
    { const bool r = W3dPruebasAnimsCmd(cmd, ss, err, manejado); if (manejado) return r; }
    // las MALLAS COMO RECURSO tienen sus comandos en test/W3dPruebasMallas.cpp
    { bool mm = false; const bool r = W3dPruebasMallasCmd(cmd, ss, err, mm); if (mm) { manejado = true; return r; } }
    // los del HITBOX viven en su propio archivo (test/W3dPruebasHitbox.cpp)
    { const bool r = W3dPruebasHitboxCmd(cmd, ss, err, manejado); if (manejado) return r; }
    // la prueba CRUZADA de las tres (test/W3dPruebasIntegracion.cpp)
    { const bool r = W3dPruebasIntegracionCmd(cmd, ss, err, manejado); if (manejado) return r; }
    // el OUTLINER POR RECURSOS (vistas, carpetas, recurso activo: test/W3dPruebasOutliner.cpp)
    { const bool r = W3dPruebasOutlinerCmd(cmd, ss, err, manejado); if (manejado) return r; }
    // las ESCENAS 3D y los PREFABS del proyecto (raices: test/W3dPruebasEscenas.cpp)
    { const bool r = W3dPruebasEscenasCmd(cmd, ss, err, manejado); if (manejado) return r; }
    // las INSTANCIAS DE PREFAB (test/W3dPruebasPrefabs.cpp)
    { const bool r = W3dPruebasPrefabsCmd(cmd, ss, err, manejado); if (manejado) return r; }
    // las LIBRERIAS EXTERNAS y los PROXIES W3D (test/W3dPruebasLibrerias.cpp)
    { const bool r = W3dPruebasLibreriasCmd(cmd, ss, err, manejado); if (manejado) return r; }
    // el STREAMING (test/W3dPruebasStreaming.cpp)
    { const bool r = W3dPruebasStreamingCmd(cmd, ss, err, manejado); if (manejado) return r; }
    manejado = true;
    if (cmd == "cargabench")      return CmdCargaBench(ss, err);
    if (cmd == "escenasintetica") return CmdEscenaSintetica(ss, err);
    if (cmd == "heapuso")         return CmdHeapUso(ss, err);
    if (cmd == "cachesproyecto")  return CmdCachesProyecto(ss, err);
    if (cmd == "escenadefecto")   return CmdEscenaDefecto(err);
    if (cmd == "juego2dmin")      return CmdJuego2DMin(ss, err);
    if (cmd == "juego2dpx")       return CmdJuego2DPx(ss, err);
    if (cmd == "viewports")       return CmdViewports(ss, err);
    if (cmd == "layoutmaximizar") return CmdLayoutMaximizar(ss, err);
    if (cmd == "propsfoco")       return CmdPropsFoco(ss, err);
    if (cmd == "propsrename")     return CmdPropsRename(ss, err);
    if (cmd == "redopanel")       return CmdRedoPanel(ss, err);
    if (cmd == "propsarm2d")      return CmdPropsArm2D(ss, err);
    if (cmd == "iconospkg")       return CmdIconosPkg(err);
    if (cmd == "formatomallas")   return CmdFormatoMallas(ss, err);
    if (cmd == "logcargamallas")  return CmdLogCargaMallas(ss, err);
    if (cmd == "w3dbinfo")        return CmdW3dbInfo(ss, err);
    if (cmd == "mallafoto")       return CmdMallaFoto(ss, err);
    if (cmd == "mallafotoigual")  return CmdMallaFotoIgual(ss, err);
    if (cmd == "w3dbrender")      return CmdW3dbRender(ss, err);
    if (cmd == "w3dbcorrupto")    return CmdW3dbCorrupto(ss, err);
    if (cmd == "mallarica")       return CmdMallaRica(ss, err);
    if (cmd == "w3dbromper")      return CmdW3dbRomper(ss, err);
    if (cmd == "formatomallasui") return CmdFormatoMallasUI(ss, err);
    if (cmd == "zipcrc")          return CmdZipCrc(err);
    if (cmd == "w3dbregistro")    return CmdW3dbRegistro(ss, err);
    if (cmd == "juego3dbool")     return CmdJuego3DBool(ss, err);
    if (cmd == "juego3dboolpx")   return CmdJuego3DBoolPx(ss, err);
    if (cmd == "indices16")       return CmdIndices16(ss, err);
    if (cmd == "w3dbjuego")       return CmdW3dbJuego(ss, err);
    if (cmd == "w3dbmarcas")      return CmdW3dbMarcas(ss, err);
    if (cmd == "w3dbajenos")      return CmdW3dbAjenos(ss, err);
    manejado = false;
    return false;
}
