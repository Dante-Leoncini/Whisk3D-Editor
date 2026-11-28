#ifndef _WIN32
#include <sys/stat.h>   // mkdir (la carpeta temporal del usuario en --script)
#endif
#include "app/W3dKeyboard.h"   // teclado en pantalla de Android + pantalla completa automatica
#include "audio/W3dAudio.h"    // pausa del mixer con el juego en pausa
#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "render/RenderOptions.h"
#ifdef W3D_MULTIBACKEND
#include "w3dBackend.h"   // el backend grafico se elige al arrancar
#endif
#include "WhiskUI/widgets/TextField.h"   // TextFieldBlinkTick (el cursor que parpadea)   // RenderType / g_redraw: son del editor
#include "W3dLang.h"   // config.start: "idioma = es" fuerza el idioma por encima del SO
#include "test/W3dScript.h" // modo test: whisk3d --script <ruta>
#include "test/W3dMcp.h"
#ifdef W3D_MCP
#include "test/W3dHistory.h"   // la grabadora de pasos (W3D_HISTORY=<ruta>)
#endif    // whisk3d --mcp: el editor atiende a una IA (Model Context Protocol, solo en la PC)
#include "io/ProjectChanges.h" // lo no guardado: el cartel al cerrar y el '*' del titulo

#ifdef __EMSCRIPTEN__       // WebGL: el browser es 1 hilo -> el loop es emscripten_set_main_loop
#include <emscripten.h>
#include <emscripten/html5.h> // tamano del canvas (resize) + cancelar el loop al salir
#endif
#ifdef __EMSCRIPTEN__
// devicePixelRatio de la pagina (1 en un monitor comun, 2 en una Mac retina, 3 en un iPhone). Acotado: un valor raro no
// puede dejar el canvas en cero ni pedir uno gigante
static double W3dWebDpr() {
    double d = emscripten_get_device_pixel_ratio();
    if (!(d >= 1.0)) d = 1.0;
    if (d > 4.0) d = 4.0;
    return d;
}
// la escala de la UI de fabrica en la web (ver la carga del config): con el dedo, la de Android; con mouse, la del config
// por el devicePixelRatio
// la pantalla se usa con el DEDO (telefono, tablet): el puntero principal es "grueso"
static bool W3dWebFinger() {
    return EM_ASM_INT({ return (window.matchMedia && window.matchMedia('(pointer: coarse)').matches) ? 1 : 0; }) != 0;
}
static int W3dWebScaleUI(int scaleConfig) {
    const int finger = W3dWebFinger() ? 1 : 0;
    if (finger) return 4;   // (en un iPhone 13, con 3 se ve bien pero las opciones quedan chicas para el dedo)
    const int base = scaleConfig > 0 ? scaleConfig : 2;
    int e = (int)floor((double)base * W3dWebDpr() + 0.5);
    if (e < 1) e = 1;
    if (e > 8) e = 8;
    return e;
}
#endif
#include "ViewPorts/LayoutInput.h" // ruteo compartido + overlay del menu
#if !defined(W3D_SYMBIAN) // Desktop (Win/Linux) + Web + ANDROID: comparten el file browser interno + la
                        // carga por w3dEngine (import/export, browser, texturas, warp, swap). Symbian tiene lo suyo.
    #define NOMINMAX

#include "lectura-escritura.h"      // open(): dialogo + ImportOBJ
#include "ViewPorts/PopUp/FileBrowser.h" // explorador de archivos COMPARTIDO
#include "ViewPorts/PopUp/ProgressPopup.h" // barra de progreso + hook LayoutSwapBuffers
#include "ViewPorts/PopUp/ConfirmPopup.h" // Android: "Atras" pregunta si cerrar Whisk3D
#include "W3dDock.h"                       // barra de progreso en el icono del dock (Linux; no-op en el resto)
#include "io/CompileGame.h"              // build de juegos en 2do plano (Tick + barra overlay)
#include "io/W3dContainer.h"             // W3dImportAsset: importar = copiar adentro del .w3d
#include "importers/import_obj.h"        // ImportOBJ (el importador real)
#include "importers/import_fbx.h"        // ImportFBX
#include "importers/import_gltf.h"       // ImportGLTF (.gltf/.glb)
#include "ViewPorts/ViewPort3D.h"        // Viewport3DActive->FocusObject() para --framesel
#include "W3dProfile.h"                  // profiler del frame (ms por categoria)
#include "w3dVersion.h"                   // W3dVersion() para el titulo de ventana

// el callback del menu "Add > Import OBJ": abre el explorador filtrado a .obj
static void PCImportObj() {
    OpenFileBrowser(T("Import OBJ"), T("Import OBJ"), ".obj", ImportModelCallback);
}
// "Add > Imports > FBX/glTF/GLB": mismo explorador; ImportModelByExtension rutea por extension al importador correcto.
static void PCImportFbx()  { OpenFileBrowser(T("Import FBX"),  T("Import FBX"),  ".fbx",  ImportModelCallback); }
static void PCImportGltf() { OpenFileBrowser(T("Import glTF"), T("Import glTF"), ".gltf", ImportModelCallback); }
static void PCImportGlb()  { OpenFileBrowser(T("Import GLB"),  T("Import GLB"),  ".glb",  ImportModelCallback); }
static void PCImportStl()  { OpenFileBrowser(T("Import STL"),  T("Import STL"),  ".stl",  ImportModelCallback); }

// EL CUADRO A LA VENTANA y el vsync: con el backend Vulkan no hay contexto de GL, los hace el (graphics/vulkan/w3dVk.h)
#ifdef W3D_VULKAN
#include "graphics/vulkan/w3dVk.h"
#endif
#ifdef W3D_PSP
#include "graphics/gu/w3dGu.h"   // la PSP: dibuja el GE con sceGu (sin OpenGL)
#endif
static void W3dPresent(SDL_Window* w) {
#ifdef W3D_PSP
    (void)w;
    w3dEngine::GuFrameFinish();   // (la PSP: espera al GE, el vsync y da vuelta los buffers)
    return;
#endif
#ifdef W3D_VULKAN
    if (w3dEngine::ActiveBackend() == w3dEngine::BackendVulkan) { VkGLPresent(); return; }
#endif
    SDL_GL_SwapWindow(w);
}
static void W3dVsync(int interval) {
#ifdef W3D_VULKAN
    if (w3dEngine::ActiveBackend() == w3dEngine::BackendVulkan) { VkGLVsync(interval); return; }
#endif
    SDL_GL_SetSwapInterval(interval);
}
// hook de swap para la barra de progreso (se redibuja DENTRO del export/import bloqueante)
static SDL_Window* g_swapWindow = NULL;
static void PCSwapBuffers() { w3dEngine::DrawPending(); if (g_swapWindow) W3dPresent(g_swapWindow); }
// mientras un render largo bloquea (la barra de progreso): Escape o el boton "Cancelar" lo cortan; cerrar la ventana
// tambien (y el cierre vuelve a la cola, para que el editor pregunte al terminar). El resto de la entrada se descarta
static void PCProgressPoll() {
    extern void ProgressCancel(); extern bool ProgressButtonCancelIn(int, int);
    SDL_Event e;
    bool exit = false;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) { exit = true; ProgressCancel(); }
        else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) ProgressCancel();
        else if (e.type == SDL_MOUSEBUTTONDOWN && ProgressButtonCancelIn(e.button.x, e.button.y)) ProgressCancel();
        else if (e.type == SDL_FINGERDOWN && g_swapWindow) {
            int ww = 0, hh = 0; SDL_GetWindowSize(g_swapWindow, &ww, &hh);
            if (ProgressButtonCancelIn((int)(e.tfinger.x * ww), (int)(e.tfinger.y * hh))) ProgressCancel();
        }
    }
    if (exit) { SDL_Event q; memset(&q, 0, sizeof(q)); q.type = SDL_QUIT; SDL_PushEvent(&q); }
}

#include "ViewPorts/Properties.h" // DialogLoadTexture
#include "objects/Textures.h"
#include "objects/Materials.h"    // Material (asignar la textura cargada)

// warp del mouse real (los drags que no deben salirse de un rect)
static void PCWarpMouse(int x, int y) {
    SDL_WarpMouseInWindow(window, x, y);
    lastMouseX = x;
    lastMouseY = y;
}

#ifdef __EMSCRIPTEN__
// MOUSE CAPTURADO EN EL NAVEGADOR (juego estilo FPS): la pagina no puede mover el cursor, asi que el truco de PC
// (teletransportarlo al centro cada cuadro) no existe. Se usa el POINTER LOCK del navegador (SDL_SetRelativeMouseMode):
// el cursor desaparece y el movimiento llega como relativo. Lo maneja la captura de MainLoopFrame; ProcessEvent junta
// el movimiento y clava la posicion del cursor al centro del juego (como en PC: un click no cae en otro panel).
static bool  g_webPointerRequest = false;         // SDL_SetRelativeMouseMode(SDL_TRUE) pedido para el juego
static float g_webMouseDX = 0, g_webMouseDY = 0;  // movimiento con el puntero bloqueado desde el ultimo cuadro
static int   g_webCenterX = 0, g_webCenterY = 0;  // el centro del viewport del juego
static bool WebLockedPointer() {
    EmscriptenPointerlockChangeEvent pl;
    return emscripten_get_pointerlock_status(&pl) == EMSCRIPTEN_RESULT_SUCCESS && pl.isActive;
}
#endif

// "Load Texture": el browser COMPARTIDO elige la imagen (png/jpg/bmp...) y la
// carga al material que la pidio (async: el browser es modal)
extern void RebindMaterialMeshPart(); // Properties.cpp
static Material* gTexMat = NULL;
extern bool gLoadTextureAsNormal; // Properties.cpp: el "Load Texture" del normal map lo prende (compartido 4 OS)
extern bool gLoadTextureAsB;      // Properties.cpp: idem el de la TEXTURA B (mezcla de texturas)
extern int  gLoadTextureLayer;    // Properties.cpp: >= 0 = el "Load Texture" de ESA capa de textura del material
extern int  gLoadTextureMap;      // Properties.cpp: >= 0 = el "Load Texture" de ESE mapa del material (W3dMatMap)
static void PickedTexture(const std::string& pickedPath) {
    if (!gTexMat) return;
    // CARGAR = QUEDA AFUERA: la textura elegida del disco se referencia como EXTERNA (W3dLoadExternal, ver
    // io/W3dContainer.h); a meterla adentro del .w3d se la pasa desde la Biblioteca
    const std::string path = W3dLoadExternal(pickedPath);
    // por el CACHE por ruta: elegir dos veces la misma imagen (para dos
    // materiales) NO la sube dos veces a la GPU. Y la anterior se suelta, asi
    // el refcount baja y esa textura se puede liberar de verdad.
    Texture* t = TextureTake(path);
    if (t) {
        if (gLoadTextureMap >= W3D_MAP_EMISSION && gLoadTextureMap < W3D_MAPS) {   // a un MAPA (emision, rugosidad...)
            extern void W3dMaterialSetMap(Material* mat, Texture* tex, int map);   // (con undo; la ranura toma la suya)
            W3dMaterialSetMap(gTexMat, t, gLoadTextureMap);
            TextureRelease(t);   // (la del cache: la ranura ya retuvo la suya)
        } else if (gLoadTextureLayer >= 0) {   // a una CAPA de textura (se crea si es la fila "Add Layer")
            extern void UndoMaterialModStart(Material*); extern void UndoMaterialModCommit();
            UndoMaterialModStart(gTexMat);   // (Ctrl+Z la saca: el paso guarda la lista de capas)
            const size_t n = (size_t)gLoadTextureLayer;
            if (n >= gTexMat->layers.size()) gTexMat->layers.resize(n + 1);
            Texture* prev = gTexMat->layers[n].tex;
            gTexMat->layers[n].tex = t;
            if (prev && prev != t) TextureRelease(prev);
            UndoMaterialModCommit();
        } else if (gLoadTextureAsB) {   // a la TEXTURA B (mezcla de texturas en una pasada)
            Texture* prev     = gTexMat->textureB;
            gTexMat->textureB = t;
            if (prev && prev != t) TextureRelease(prev);
        } else {
            Texture* prev = gLoadTextureAsNormal ? gTexMat->normalTexture : gTexMat->texture;
            if (gLoadTextureAsNormal) { gTexMat->normalTexture = t; gTexMat->normalMap = true; }
            else { gTexMat->texture = t; gTexMat->textureOn = true; }
            if (prev && prev != t) TextureRelease(prev);
            if (!gLoadTextureAsNormal) LayoutReferenceTexture(gTexMat, pickedPath, t);   // (Add > Reference: su proporcion)
        }
        RebindMaterialMeshPart();
    }
    gTexMat = NULL; gLoadTextureAsNormal = false; gLoadTextureAsB = false; gLoadTextureLayer = -1; gLoadTextureMap = -1;
}
// "Load Texture" (base Y normal map): el MISMO browser. Quien lo llama ya dejo gLoadTextureAsNormal en el
// valor correcto (false=textura, true=normal); el callback de arriba decide el destino.
static void PCLoadTextureIn(Material* mat) {
    gTexMat = mat;
    OpenFileBrowser(gLoadTextureAsNormal ? T("Load normal map") : T("Load Texture"),
                    gLoadTextureAsNormal ? T("Open normal map") : T("Open"),
                    ".png .jpg .jpeg .bmp .tga", PickedTexture);
}

#ifdef __EMSCRIPTEN__
// ============================================================================
//  WEB: cargar texturas / OBJ con el SELECTOR DE ARCHIVOS DEL NAVEGADOR
// ----------------------------------------------------------------------------
//  El file browser interno navega el FS virtual de emscripten (no ve los archivos
//  reales del usuario). En web abrimos un <input type=file> nativo: el navegador
//  deja elegir del disco, leemos el archivo, lo escribimos en /uploads del FS de
//  emscripten y llamamos a la MISMA logica de carga (PickedTexture / ImportOBJ).
//  Para el OBJ el <input multiple> deja elegir el .obj + su .mtl + las texturas
//  juntos: quedan en la misma carpeta (/uploads) y el importador los resuelve.
// ============================================================================
extern "C" {
// las llama el JS del picker cuando el archivo ya esta escrito en el FS de emscripten
EMSCRIPTEN_KEEPALIVE void WebLoadedTexture(const char* path) { PickedTexture(std::string(path)); }
// POR LA PUERTA UNICA (ImportModelByExtension), no por el importador directo: sino la malla
// importada en la web quedaba SIN Mesh::origen (ni "reimportar del original" ni EXTERNALS.txt).
EMSCRIPTEN_KEEPALIVE void LoadedWebObj(const char* path)     { ImportModelByExtension(std::string(path)); }
EMSCRIPTEN_KEEPALIVE void LoadedWebFbx(const char* path)     { ImportModelByExtension(std::string(path)); }
}

// overlay con un BOTON real: el <input type=file>.click() se dispara DENTRO del tap del boton (gesto DOM
// genuino). Necesario para iOS Safari: ahi input.click() llamado desde el RAF (fuera de un gesto de usuario)
// NO abre el selector -> "al hacerle click no ocurre nada". El input va en el DOM (otro requisito de iOS).
// tipo: 0 = textura (1 imagen), 1 = OBJ (multi: obj+mtl+texturas).
EM_JS(void, WebOpenPicker, (int type), {
    var prev = document.getElementById('w3d-pick'); if (prev && prev.parentNode) prev.parentNode.removeChild(prev);
    var ov = document.createElement('div');
    ov.id = 'w3d-pick';
    ov.style.cssText = 'position:fixed;left:0;top:0;right:0;bottom:0;z-index:99999;'
        + 'background:rgba(0,0,0,0.5);display:flex;align-items:flex-start;justify-content:center;';
    var card = document.createElement('div');
    card.style.cssText = 'margin-top:8vh;width:92%;max-width:440px;background:#2b2b2b;border:1px solid #555;'
        + 'border-radius:10px;padding:16px;box-shadow:0 6px 30px rgba(0,0,0,0.6);font-family:sans-serif;color:#ddd;';
    var lab = document.createElement('div');
    lab.textContent = (type == 2) ? 'Importar FBX (.fbx)' : type ? 'Importar OBJ (.obj + .mtl + texturas)' : 'Cargar textura (imagen)';
    lab.style.cssText = 'font-size:15px;margin-bottom:12px;color:#9ac9ff;';
    var input = document.createElement('input');
    input.type = 'file';
    if (type == 2)   { input.accept = '.fbx'; }                                                  // FBX: un solo archivo binario
    else if (type)   { input.multiple = true; input.accept = '.obj,.mtl,image/*,.png,.jpg,.jpeg,.bmp,.tga'; }
    else             { input.accept = 'image/png,image/jpeg,image/bmp,.png,.jpg,.jpeg,.bmp,.tga'; }
    input.style.cssText = 'position:absolute;left:-9999px;width:1px;height:1px;opacity:0;'; // en el DOM pero oculto
    input.onchange = function(e) {
        var files = Array.prototype.slice.call(e.target.files);
        if (!files.length) { close(); return; }
        try { FS.mkdir('/uploads'); } catch (_e) {}
        if (type == 2) {                       // FBX: un solo archivo binario -> LoadedWebFbx
            var f = files[0], r = new FileReader();
            r.onload = function() {
                var p = '/uploads/' + f.name;
                FS.writeFile(p, new Uint8Array(r.result));
                ccall('LoadedWebFbx', null, ['string'], [p]);
            };
            r.readAsArrayBuffer(f); close();
        } else if (!type) {
            var f = files[0], r = new FileReader();
            r.onload = function() {
                var p = '/uploads/' + f.name;
                FS.writeFile(p, new Uint8Array(r.result));
                ccall('WebLoadedTexture', null, ['string'], [p]);
            };
            r.readAsArrayBuffer(f); close();
        } else {
            var objPath = null, pending = files.length;
            files.forEach(function(f) {
                var r = new FileReader();
                r.onload = function() {
                    var p = '/uploads/' + f.name;
                    FS.writeFile(p, new Uint8Array(r.result));
                    if (f.name.toLowerCase().endsWith('.obj')) objPath = p;
                    if (--pending === 0 && objPath) ccall('LoadedWebObj', null, ['string'], [objPath]);
                };
                r.readAsArrayBuffer(f);
            });
            close();
        }
    };
    var btnPick = document.createElement('button');
    btnPick.textContent = 'Elegir archivo';
    btnPick.style.cssText = 'width:100%;font-size:17px;padding:14px;border:none;border-radius:6px;'
        + 'background:#3a8f68;color:#fff;font-weight:bold;';
    var btnC = document.createElement('button');
    btnC.textContent = 'Cancel';
    btnC.style.cssText = 'width:100%;margin-top:10px;font-size:15px;padding:11px;border:none;border-radius:6px;background:#444;color:#ddd;';
    function close(){ if (ov.parentNode) ov.parentNode.removeChild(ov); }
    // el click del boton ES el gesto de usuario -> input.click() adentro SI abre el selector en iOS
    btnPick.addEventListener('click', function(){ input.click(); });
    btnC.addEventListener('click', function(){ close(); });
    ov.addEventListener('mousedown', function(e){ if (e.target === ov) close(); });
    ov.addEventListener('touchend', function(e){ if (e.target === ov) close(); });
    ['mousedown','mouseup','touchstart','touchmove','touchend','pointerdown','pointerup'].forEach(function(ev){
        card.addEventListener(ev, function(e){ e.stopPropagation(); });
    });
    card.appendChild(lab); card.appendChild(input); card.appendChild(btnPick); card.appendChild(btnC);
    ov.appendChild(card); document.body.appendChild(ov);
});
static void WebOpenPickerTexture(){ WebOpenPicker(0); }
static void WebOpenPickerObj(){ WebOpenPicker(1); }
static void WebOpenPickerFbx(){ WebOpenPicker(2); }

static void WebLoadTextureIn(Material* mat) { gTexMat = mat; WebOpenPickerTexture(); }
static void WebImportObj() { WebOpenPickerObj(); }
static void WebImportFbx() { WebOpenPickerFbx(); } // web: FBX por el selector NATIVO del navegador (como OBJ/texturas)

// descarga un archivo del FS de emscripten al DISCO del usuario (export OBJ/mtl y renders PNG).
// La llaman Properties.cpp (export OBJ) y ViewPort3D.cpp (render) via forward-declaration.
EM_JS(void, WebUnloadFile, (const char* pathPtr, const char* namePtr), {
    var path = UTF8ToString(pathPtr), name = UTF8ToString(namePtr);
    // COLA espaciada: Safari (sobre todo iOS) ignora descargas programaticas disparadas juntas -> de un
    // export OBJ+MTL o de varios renders solo bajaba UNO. Las encolamos y disparamos de a una con un gap.
    if (!window.__w3dDlQueue) window.__w3dDlQueue = [];
    window.__w3dDlQueue.push({ path: path, name: name });
    if (window.__w3dDlBusy) return;
    window.__w3dDlBusy = true;
    function next(){
        if (!window.__w3dDlQueue.length) { window.__w3dDlBusy = false; return; }
        var it = window.__w3dDlQueue.shift();
        try {
            var data = FS.readFile(it.path); // Uint8Array del FS virtual
            var blob = new Blob([data], { type: 'application/octet-stream' });
            var url = URL.createObjectURL(blob);
            var a = document.createElement('a');
            a.href = url; a.download = it.name; a.rel = 'noopener';
            document.body.appendChild(a); a.click(); document.body.removeChild(a);
            setTimeout(function () { URL.revokeObjectURL(url); }, 4000);
        } catch (e) { console.error('descarga fallo:', it.path, e); }
        setTimeout(next, 700); // gap entre descargas (Safari necesita respiro entre a.click())
    }
    next();
});

#endif // __EMSCRIPTEN__
#include "WhiskUI/widgets/PopupMenu.h" // MenuScreenW/H (desplegables)
#include "WhiskUI/draw/glesdraw.h"  // W3dScreenHeight (flip de Y)
#ifdef _WIN32
    #include <windows.h> // SOLO Windows (el resto del bloque es escritorio compartido Win+Linux)
#endif
#endif

#if SDL_MAJOR_VERSION == 2
    #ifndef __ANDROID__
        #define SDL_MAIN_HANDLED
    #endif
    #include <SDL2/SDL.h>
    #include "sdl_key_compat.h"
#elif SDL_MAJOR_VERSION == 3
    #include <SDL3/SDL.h>
    //para las texturas
    #define STB_IMAGE_IMPLEMENTATION
    #include "stb/stb_image.h"
#endif

#ifdef __ANDROID__
    #include <android/log.h>
    #include <GLES/gl.h>    // OpenGL ES 1.1
    #include <GLES/glext.h>
    // para pasarle el AAssetManager del APK al Core (que lee texturas sin SDL)
    #include <jni.h>
    #include <android/asset_manager.h>
    #include <android/asset_manager_jni.h>
#else
    #include <GL/gl.h>      // OpenGL Desktop
    #include <GL/glu.h>
    #ifndef _WIN32
    #include <GL/glext.h>
    #endif
#endif

//esto es solo para linux/android
#include <functional>
#include <cmath>
#include <cfloat>
#include <map>
#include <algorithm>
#include <cstring>
#include <unordered_set>


#include <filesystem>
#include <vector>
#include <fstream>
#include <unordered_map>
#include <string>
#include <sstream>
#include <iostream>
#include "base/w3dlog.h"   // w3dLogf: para redirigir cout/cerr al log propio (no la consola blanca de Windows)
// LA CAPTURA DE PRUEBA (W3D_CAPTURE=n): el cuadro n dibujado queda en un .ppm (W3D_CAPTURE_FILE, o en Android
// /sdcard/w3d_captura.ppm), antes de mostrarlo. Es para ver la pantalla donde no se puede sacar una captura (Android 2.3
// sin root); sin la variable no hace nada. W3D_CAPTURE=n:k guarda k cuadros seguidos desde el n (archivo.0, archivo.1...)
static void W3dCaptureFrame(SDL_Window* w) {
#if defined(W3D_PSP) || defined(W3D_3DS)
    (void)w;
#else
    static int target = -2, count = 1, frame = 0;
    if (target == -2) {
        const char* e = getenv("W3D_CAPTURE");
        target = (e && *e) ? atoi(e) : -1;
        const char* c = e ? strchr(e, ':') : NULL;
        if (c && atoi(c + 1) > 1) count = atoi(c + 1);
    }
    ++frame;
    if (target < 0 || frame < target || frame >= target + count) return;
    int pw = 0, ph = 0;
    SDL_GL_GetDrawableSize(w, &pw, &ph);
    if (pw <= 0 || ph <= 0) return;
    std::vector<unsigned char> px((size_t)pw * ph * 4);
    w3dEngine::ReadPixelsRGBA(0, 0, pw, ph, &px[0]);
    const char* f = getenv("W3D_CAPTURE_FILE");
#ifdef __ANDROID__
    std::string path = (f && *f) ? f : "/sdcard/w3d_captura.ppm";
#else
    std::string path = (f && *f) ? f : "w3d_captura.ppm";
#endif
    if (count > 1) { char n[16]; snprintf(n, sizeof(n), ".%d", frame - target); path += n; }
    FILE* o = fopen(path.c_str(), "wb");
    if (!o) { w3dLogfE("[captura] no se pudo escribir %s", path.c_str()); return; }
    fprintf(o, "P6\n%d %d\n255\n", pw, ph);
    for (int y = ph - 1; y >= 0; y--)   // (GL lee de abajo para arriba)
        for (int x = 0; x < pw; x++) fwrite(&px[((size_t)y * pw + x) * 4], 1, 3, o);
    fclose(o);
    w3dLogf("[captura] cuadro %d (%dx%d) -> %s", frame, pw, ph, path.c_str());
#endif
}
#include <iomanip>


#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
    #include <limits.h>
#endif

#include "w3dFilesystem.h"
#include "w3dTexture.h"   // w3dEngine::LoadTexture (carga de texturas del Core, lee via w3dFileSystem)

//Whisk3D imports
#include "variables.h"
#include "WhiskUI/theme/colors.h"
#include "ui/W3dColors.h" // W3dColors(Ubyte) + carga de colores del editor + Sincronizar
#include "objects/RenderColors.h" // paleta de render del core (Fase D: el sync de loadColors la llena)
#include "objects/Textures.h"
#include "objects/Materials.h"
#include "animation/Animation.h"
#include "animation/VertexAnimation.h"
#include "animation/Armature2DAnimation.h" // kind 4: evaluar el clip del armature 2D en el playhead
#include "ViewPorts/ViewPorts.h"
#include "ViewPorts/PopUp/PopUpBase.h"
#include "ViewPorts/PopUp/ColorPicker.h"
#include "controls.h"
#include "constructor.h"

// Reloj de ms: el Core ya trae uno portable; aca le damos el de SDL, que mide tiempo de PARED
// (el respaldo del Core usa clock(), que mide CPU). Se registra en el arranque, ver W3dSetClock.
static unsigned int ClockSDL() { return SDL_GetTicks(); }

// ---------------------------------------------------------------------------------------------------------------
//  VENTANA + CONTEXTO GL, CON INTENTOS DE RESPALDO.
//  No todos los drivers tienen una ventana con multisampling o con estencil: maquinas virtuales, GPUs viejas,
//  algunos XWayland, Mesa por software... Pedir una sola combinacion fija y salir si no estaba dejaba al editor sin
//  abrir ("Error SDL_CreateWindow: Couldn't find matching GLX visual", reporte de un usuario del AppImage). Ahora se
//  prueba de lo mas lindo a lo mas basico y se queda con lo primero que abra; lo que se resigna va al log.
//  W3D_GL_TEST_FALLBACK=1 hace fallar el primer intento a proposito (para probar el respaldo sin ese driver).
// ---------------------------------------------------------------------------------------------------------------
#ifdef W3D_3DS
#include "graphics/citro3d/w3dC3D.h"   // la 3DS: dibuja citro3d (sin OpenGL)
#endif
struct W3dAttemptGL { int msaa, stencil, z; const char* that; };
static bool g_glNoMsaa = false;   // se pidio antialiasing y el driver no lo dio (se avisa al abrir)
static bool g_windowHidden = false;   // --script en Windows: la ventana se crea oculta (ver el arranque de main)
static SDL_Window* CreateWindowGL(const char* title, int w, int h, Uint32 flags, bool antialias, SDL_GLContext* ctxOut) {
#ifdef W3D_3DS
    // la 3DS no tiene OpenGL: la ventana es la pantalla de abajo (la tactil, donde SDL entrega los toques) y dibuja
    // citro3d (graphics/citro3d/w3dGraphicsC3D.cpp), que se inicia aca: SDL ya inicio las pantallas (gfxInit)
    (void)antialias;
    *ctxOut = NULL;
    SDL_Window* win3ds = SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED_DISPLAY(1), SDL_WINDOWPOS_UNDEFINED_DISPLAY(1), w, h, flags);
    if (win3ds && !w3dEngine::C3DInit()) { w3dLogfE("citro3d no inicio"); SDL_DestroyWindow(win3ds); return NULL; }
    return win3ds;
#endif
#ifdef W3D_PSP
    // la PSP no tiene OpenGL: la ventana es la pantalla (SDL entrega ahi los eventos) y dibuja el GE con sceGu
    // (graphics/gu/w3dGraphicsGu.cpp), que se inicia aca
    (void)antialias;
    *ctxOut = NULL;
    SDL_Window* winPsp = SDL_CreateWindow(title, 0, 0, w, h, flags);
    if (winPsp && !w3dEngine::GuStart()) { w3dLogfE("el GE no inicio"); SDL_DestroyWindow(winPsp); return NULL; }
    return winPsp;
#endif
    static const W3dAttemptGL kAttempts[] = {
        { 4, 8, 24, "antialiasing 4x, estencil 8, profundidad 24" },
        { 2, 8, 24, "antialiasing 2x, estencil 8, profundidad 24" },
        { 0, 8, 24, "sin antialiasing, estencil 8, profundidad 24" },
        { 0, 0, 24, "sin antialiasing ni estencil, profundidad 24" },
        { 0, 0, 16, "sin antialiasing ni estencil, profundidad 16" },
    };
    const int n = (int)(sizeof(kAttempts) / sizeof(kAttempts[0]));
    const bool forceFailed = std::getenv("W3D_GL_TEST_FALLBACK") != NULL;
    for (int i = antialias ? 0 : 2; i < n; i++) {
        const W3dAttemptGL& t = kAttempts[i];
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, t.msaa ? 1 : 0);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, t.msaa);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, t.stencil);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, t.z);
        if (forceFailed && i == (antialias ? 0 : 2)) { w3dLogfW("Ventana GL (%s): fallo forzado por W3D_GL_TEST_FALLBACK", t.that); continue; }
        SDL_Window* win = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h, flags);
        if (!win) { w3dLogfW("Ventana GL (%s): %s", t.that, SDL_GetError()); continue; }
        SDL_GLContext ctx = SDL_GL_CreateContext(win);
        if (!ctx) { w3dLogfW("Contexto GL (%s): %s", t.that, SDL_GetError()); SDL_DestroyWindow(win); continue; }
        if (antialias && t.msaa == 0) g_glNoMsaa = true;
        if (i > (antialias ? 0 : 2)) w3dLogf("Ventana GL abierta con respaldo: %s", t.that);
        *ctxOut = ctx;
        return win;
    }
    *ctxOut = NULL;
    return NULL;
}
#ifdef W3D_VULKAN
// LA VENTANA DE VULKAN: la abre SDL con el cargador del sistema (vulkan-1.dll / libvulkan.so.1) y el backend arranca
// sobre ella (graphics/vulkan/w3dVk.h). NULL = no anduvo (ya quedo en el log por que)
#include <SDL2/SDL_vulkan.h>
static bool VkSurfaceSDL(void* user, void* instance, void* out) {
    return SDL_Vulkan_CreateSurface((SDL_Window*)user, (VkInstance)instance, (VkSurfaceKHR*)out) == SDL_TRUE;
}
static void VkSizeSDL(void* user, int* w, int* h) { SDL_Vulkan_GetDrawableSize((SDL_Window*)user, w, h); }
static SDL_Window* CreateWindowVulkan(const char* title, int w, int h, Uint32 flags) {
    if (SDL_Vulkan_LoadLibrary(NULL) != 0) { w3dLogfW("[vk] sin el cargador de Vulkan: %s", SDL_GetError()); return NULL; }
    flags = (flags & ~SDL_WINDOW_OPENGL) | SDL_WINDOW_VULKAN;
    SDL_Window* win = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h, flags);
    if (!win) { w3dLogfW("[vk] ventana: %s", SDL_GetError()); SDL_Vulkan_UnloadLibrary(); return NULL; }
    unsigned n = 0;
    SDL_Vulkan_GetInstanceExtensions(win, &n, NULL);
    std::vector<const char*> ext(n);
    if (n) SDL_Vulkan_GetInstanceExtensions(win, &n, &ext[0]);
    W3dVkWindow v;
    v.getInstanceProcAddr = SDL_Vulkan_GetVkGetInstanceProcAddr();
    v.extensions = n ? &ext[0] : NULL; v.nExtensions = n;
    v.createSurface = VkSurfaceSDL; v.size = VkSizeSDL; v.user = win;
    std::string err;
    if (!VkGLStart(v, &err)) {
        w3dLogfW("[vk] no arranco: %s", err.c_str());
        SDL_DestroyWindow(win);
        SDL_Vulkan_UnloadLibrary();
        return NULL;
    }
    return win;
}
#endif

// Función simple para leer el ini
#ifdef __ANDROID__
// el archivo ENTERO de un SDL_RWops, terminado en 0 (y lo cierra). Sin SDL_RWsize: en el SDL 2.0.7 del APK de Android 2.3
// da -1 con un archivo comun (el config.start del usuario, no los del APK): el buffer quedaba vacio, SDL_RWread copiaba el
// archivo a un puntero nulo y el editor se cerraba al arrancar (el Galaxy Ace, apenas se guardaba algo en Ajustes)
static std::vector<char> W3dReadRW(SDL_RWops* rw) {
    std::vector<char> b;
    char chunk[4096];
    size_t n;
    while ((n = SDL_RWread(rw, chunk, 1, sizeof(chunk))) > 0) b.insert(b.end(), chunk, chunk + n);
    SDL_RWclose(rw);
    b.push_back(0);
    return b;
}
#endif
Config loadConfig(const std::string& filename) {
    Config cfg;

    std::string configPath = filename;

    #ifdef WHISK3D_LINUX
        // 1. Intentar primero desde config de usuario
        std::string userConfigDir = GetResDir();
        if (!userConfigDir.empty()) {
            std::string userConfigPath = userConfigDir + "/config.ini";
            if (std::filesystem::exists(userConfigPath)) {
                configPath = userConfigPath;
                std::cout << "Usando config de usuario: " << configPath << "\n";
            } else {
                std::cout << "No se encontró config en directorio de usuario, usando sistema: " << filename << "\n";
            }
        }
    #endif

    #ifdef __ANDROID__
        SDL_RWops* rw = SDL_RWFromFile(configPath.c_str(), "rb");
        if (!rw) {
            __android_log_print(ANDROID_LOG_ERROR, "SDL_MAIN", "No se pudo abrir %s", configPath.c_str());
            return cfg;
        }

        std::vector<char> buffer = W3dReadRW(rw);

        std::istringstream iss(buffer.data());
    #else
        std::ifstream file(configPath);
        if (!file.is_open()) {
            std::cerr << "No se pudo abrir " << configPath << ", usando valores por defecto.\n";
            return cfg;
        }

        // (por el operator<< del stream y no con istreambuf_iterator: si el read falla, el iterador tira una excepcion
        // que nadie atrapa y la app se cae; asi solo queda el texto que se pudo leer)
        std::ostringstream read;
        read << file.rdbuf();
        std::istringstream iss(read.str());
    #endif

    std::string line;
    while (std::getline(iss, line)) {
        std::istringstream lineStream(line);
        std::string key, eq, value;
        if (lineStream >> key >> eq >> value && eq == "=") {
            if (key == "fullscreen") cfg.fullscreen = (value == "true");
            else if (key == "enableAntialiasing") cfg.enableAntialiasing = (value == "true");
            else if (key == "mipmaps") { cfg.mipmaps = (value == "true" || value == "1");
                                         w3dEngine::SetMipmapsGlobal(cfg.mipmaps); }
            else if (key == "width") cfg.width = std::stoi(value);
            else if (key == "height") cfg.height = std::stoi(value);
            else if (key == "displayIndex") cfg.displayIndex = std::stoi(value);
            else if (key == "scale") cfg.scale = std::stoi(value);
            else if (key == "newUser") cfg.newUser = (value == "true" || value == "1"); // barra de herramientas
            else if (key == "autoKey") cfg.autoKey = (value == "true" || value == "1");      // boton AUTO KEY del timeline
            else if (key == "SkinName") cfg.SkinName = value;
            else if (key == "graphicsAPI") cfg.graphicsAPI = value;
            else if (key == "repoPath") cfg.repoPath = value;   // raiz del repo para Compilar (editor instalado)
            else if (key == "logs") cfg.logs = std::stoi(value);   // nivel de los logs del editor (Ajustes > Logs)
            else if (key == "systemKeyboard") cfg.keyboardSystem = (value == "true" || value == "1");   // Ajustes > teclado
            else if (key == "language") {
                // "auto" (o ausente) = seguir al sistema. Cualquier otra cosa FUERZA ese idioma.
                if (value != "auto" && !value.empty()) { g_language = W3dLanguageOf(value.c_str()); g_languageForced = true; }
            }
        }
    }

    return cfg;
}

bool loadColors(const std::string& filename) {
    static std::unordered_map<std::string, ColorID> colorMap = {
        {"background", ColorID::background},
        {"white", ColorID::white},
        {"accent", ColorID::accent},
        {"accentDark", ColorID::accentDark},
        {"black", ColorID::black},
        {"gray", ColorID::gray},
        {"headerColor", ColorID::headerColor},
        {"blackTransparent", ColorID::blackTransparent},
        {"grayUI", ColorID::grayUI},
        {"grayLine", ColorID::grayLine}
    };

    std::string skinPath = filename;

    #ifdef WHISK3D_LINUX
        // 1. Intentar primero desde config de usuario
        std::string userConfigDir = GetResDir();
        if (!userConfigDir.empty()) {
            // Extraer nombre del skin del path original
            size_t skinsPos = filename.find("/Skins/");
            if (skinsPos != std::string::npos) {
                std::string skinRelPath = filename.substr(skinsPos);
                std::string userSkinPath = userConfigDir + skinRelPath;

                if (std::filesystem::exists(userSkinPath)) {
                    skinPath = userSkinPath;
                    std::cout << "Usando skin de usuario: " << skinPath << "\n";
                } else {
                    std::cout << "No se encontró skin en directorio de usuario, usando sistema: " << filename << "\n";
                }
            }
        }
    #endif

    std::istringstream fileStream;

    #ifdef __ANDROID__
        SDL_RWops* rw = SDL_RWFromFile(skinPath.c_str(), "rb");
        if (!rw) {
            __android_log_print(ANDROID_LOG_ERROR, "SDL_MAIN", "No se pudo abrir %s", skinPath.c_str());
            return false;
        }

        std::vector<char> buffer = W3dReadRW(rw);

        fileStream.str(buffer.data());
    #else
        std::ifstream file(skinPath);
        if (!file.is_open()) {
            std::cerr << "No se pudo abrir " << skinPath << ", usando colores por defecto.\n";
            return false;
        }

        std::ostringstream read;   // (no istreambuf_iterator: ver loadConfig)
        read << file.rdbuf();
        fileStream.str(read.str());
    #endif

    std::string line;
    while (std::getline(fileStream, line)) {
        std::istringstream iss(line);
        std::string name;
        float r, g, b, a;
        if (!(iss >> name >> r >> g >> b >> a)) {
            std::cerr << "Formato incorrecto en colors.skin: " << line << "\n";
            continue;
        }

        auto it = colorMap.find(name);
        if (it != colorMap.end()) {
            int idx = static_cast<int>(it->second);
            ColorList[idx][0] = r;
            ColorList[idx][1] = g;
            ColorList[idx][2] = b;
            ColorList[idx][3] = a;

            ColorListUbyte[idx][0] = (GLubyte)(r*255);
            ColorListUbyte[idx][1] = (GLubyte)(g*255);
            ColorListUbyte[idx][2] = (GLubyte)(b*255);
            ColorListUbyte[idx][3] = (GLubyte)(a*255);

            #ifdef __ANDROID__
                __android_log_print(ANDROID_LOG_VERBOSE, "SDL_MAIN",
                    "Color %s cargado: R=%.3f G=%.3f B=%.3f A=%.3f",
                    name.c_str(), r, g, b, a);
            #else
                //std::cout << "Color " << name << " cargado: R=" << r << " G=" << g << " B=" << b << " A=" << a << "\n";
            #endif
        } else {
            int eidx = editorColorIdByName(name.c_str());
            if (eidx >= 0) {
                W3dColors[eidx][0] = r;
                W3dColors[eidx][1] = g;
                W3dColors[eidx][2] = b;
                W3dColors[eidx][3] = a;
                W3dColorsUbyte[eidx][0] = (GLubyte)(r*255);
                W3dColorsUbyte[eidx][1] = (GLubyte)(g*255);
                W3dColorsUbyte[eidx][2] = (GLubyte)(b*255);
                W3dColorsUbyte[eidx][3] = (GLubyte)(a*255);
            } else {
                std::cerr << "Color desconocido en colors.skin: " << name << "\n";
            }
        }
    }

    // Fase D: copiar la paleta de la UI a la de RENDER del core. Ahora COMPARTIDO
    // (colors.cpp), asi Symbian -que carga por loadColorsW3d- tambien lo hace.
    SyncRenderColors();

    return true;
}

void initGL(int width, int height) {
    #ifdef __ANDROID__
        w3dEngine::Viewport(0, 0, width, height);

        // Proyección simple (similar a gluPerspective)
        w3dEngine::MatrixMode(w3dEngine::Projection);
        w3dEngine::LoadIdentity();
        GLfloat ratio = (GLfloat)width / (GLfloat)height;
        GLfloat fov = 60.0f * 3.14159265f / 180.0f;
        GLfloat nearClip = 0.1f;
        GLfloat farClip = 100.0f;
        GLfloat top = tan(fov / 2) * nearClip;
        GLfloat bottom = -top;
        GLfloat left = bottom * ratio;
        GLfloat right = top * ratio;
        w3dEngine::Frustum(left, right, bottom, top, nearClip, farClip);

        w3dEngine::MatrixMode(w3dEngine::ModelView);
        w3dEngine::LoadIdentity();
    #else
        w3dEngine::Viewport(0, 0, width, height);
        w3dEngine::MatrixMode(w3dEngine::Projection);
        w3dEngine::LoadIdentity();
        w3dEngine::Perspective(60.0f, (float)width / height, 0.1f, 100.0f);
        w3dEngine::MatrixMode(w3dEngine::ModelView);
        w3dEngine::LoadIdentity();
    #endif

    w3dEngine::Enable(w3dEngine::DepthTest);
    w3dEngine::Enable(w3dEngine::CullFace);
    w3dEngine::ClearColor(0.2f, 0.2f, 0.25f, 1.0f);
}

// UN FRAME del editor: procesa eventos y, si algo cambio, redibuja. En escritorio la corre el
// while(running); en WebGL la llama el browser (emscripten_set_main_loop). Todo lo que usa es
// global (window, winW/winH, rootViewport, running, g_redraw, last*Time...) salvo el SDL_Event local.
bool g_didRender = false; // el ultimo MainLoopFrame DIBUJO? -> el loop de escritorio pacea a 60fps si dibujo, o bloquea en eventos si no

// procesa UN evento SDL. COMPARTIDO: lo llaman el poll del frame Y la espera de reposo del loop (asi el manejo de
// input no esta duplicado). Cualquier evento marca g_redraw (hay que redibujar).
// LO NO GUARDADO (io/ProjectChanges.h): cerrar la ventana con algo sin guardar abre el cartel "Se perderan
// los cambios en:" en vez de salir; el cartel sale (o guarda y sale) con estos ganchos
static void ChangesExit() { running = false; }
static void ChangesOpen(const std::string& path) { extern std::string g_projOpenPending; g_projOpenPending = path; g_redraw = true; }
#ifdef __ANDROID__
// "Cerrar" en la pregunta de ATRAS (sin nada sin guardar): se cierra
static void BackCloseConfirmed() { running = false; }
#endif
static bool ChangesSave() {
    extern std::string w3dPath;
    extern bool SaveW3D(const std::string&);
    extern void SaveProjectAs();
    if (w3dPath.empty()) { SaveProjectAs(); return false; }   // (sin archivo: primero "Guardar como")
    return SaveW3D(w3dPath);
}
// MIENTRAS SE COMPILA UN JUEGO ("Compilar juego") el editor queda bloqueado: no se usa ni se guarda el proyecto hasta que
// termina; se ve la barra de progreso sobre el editor oscurecido (ProgressOverlayRender). Lo que hace el usuario se
// descarta, salvo SOLTAR (el mouse, un dedo, una tecla): terminan lo que se apreto antes (el click que lanzo la
// compilacion) y no dejan nada trabado. Los eventos de la ventana (el tamanio) y de los mandos que se conectan siguen
static bool LockedEventOnCompile(const SDL_Event& e) {
    switch (e.type) {
        case SDL_QUIT: case SDL_KEYDOWN: case SDL_TEXTEDITING: case SDL_TEXTINPUT:
        case SDL_MOUSEMOTION: case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEWHEEL:
        case SDL_FINGERDOWN: case SDL_FINGERMOTION: case SDL_MULTIGESTURE: case SDL_DOLLARGESTURE:
        case SDL_JOYAXISMOTION: case SDL_JOYBALLMOTION: case SDL_JOYHATMOTION: case SDL_JOYBUTTONDOWN:
        case SDL_CONTROLLERAXISMOTION: case SDL_CONTROLLERBUTTONDOWN:
        case SDL_DROPFILE: case SDL_DROPTEXT: case SDL_DROPBEGIN: case SDL_DROPCOMPLETE:
            return true;
        default: return false;
    }
}
// LA VENTANA REPARTIDA: el layout y el area de los popups (MenuPantalla*) van en lo que NO tapa un teclado. El del
// sistema achica la ventana (Android con adjustResize) o la tapa sin achicarla (en pantalla completa: lo avisa Java,
// W3dKeyboardCovered); el de Whisk3D abierto para el IDE se queda abajo y el IDE maximizado arriba. W3dScreenHeight
// sigue siendo el alto de la ventana (las cuentas de GL)
static void WindowRearrange() {
    const int covered = W3dKeyboardCovered();
    const int hUseful = winH - covered;
    W3dKeyboardResize(winW, winH, covered);   // un teclado del sistema abierto -> maximizar el viewport que lo pidio
    W3dScreenHeight = winH;
    MenuScreenW = winW;
    MenuScreenH = hUseful;
    if (rootViewport) rootViewport->Resize(winW, hUseful - W3dKeyboardIDECurrentHeight());
    // el TELEFONO girado: el layout de la otra orientacion (cada una tiene el suyo: LayoutInput.cpp). Con un teclado
    // abierto no: la parte de arriba de un telefono parado puede ser mas ancha que alta y no se giro nada
    if (!W3dKeyboardVisible() && !W3dKeyboardIDECurrentHeight()) { extern void LayoutScreenSize(int, int); LayoutScreenSize(winW, winH); }
}

// LA WEB RENDERIZANDO UN VIDEO (Render Animation a MP4, Properties.cpp): va de a unos cuadros por vuelta del bucle del
// navegador y el editor queda bloqueado como al compilar un juego
#ifdef __EMSCRIPTEN__
extern bool W3dWebRenderInProgress();
extern bool W3dWebRenderTick();
#else
static bool W3dWebRenderInProgress() { return false; }
#endif
static void ProcessEvent(SDL_Event& e) {
    // lo que tapa el teclado del sistema cambio (Android en pantalla completa: ver W3dKeyboardCovered)
    if (W3dKeyboardIsEvent(e)) { WindowRearrange(); g_redraw = true; return; }
    {   extern bool CompileGameInProgress();
        const bool compiling = CompileGameInProgress();
        if ((compiling || W3dWebRenderInProgress()) && LockedEventOnCompile(e)) {
            if (e.type == SDL_QUIT && compiling) { extern void Notify(const std::string&, bool); Notify(T("Wait for the build to finish"), false); }
            if (e.type == SDL_DROPFILE && e.drop.file) SDL_free(e.drop.file);
            if (e.type == SDL_DROPTEXT && e.drop.file) SDL_free(e.drop.file);
            return;
        }
    }
#ifdef W3D_VITA
    { extern bool W3dVitaValidEvent(const SDL_Event&); if (!W3dVitaValidEvent(e)) return; }   // (el panel de atras)
#endif
    g_redraw = true;
#ifdef W3D_DREAMCAST
    if (e.type == SDL_MOUSEMOTION && e.motion.which != SDL_TOUCH_MOUSEID) { extern void W3dMovedDcMouse(); W3dMovedDcMouse(); }
#endif
#ifdef __EMSCRIPTEN__
    // puntero bloqueado para el juego: el movimiento va a ratonRel() y, para el resto del editor, el cursor queda en el
    // centro del juego (el movimiento relativo ya no lo mueve)
    if (g_webPointerRequest && (e.type == SDL_MOUSEMOTION || e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) &&
        WebLockedPointer()) {
        if (e.type == SDL_MOUSEMOTION) {
            g_webMouseDX += (float)e.motion.xrel; g_webMouseDY += (float)e.motion.yrel;
            e.motion.x = g_webCenterX; e.motion.y = g_webCenterY; e.motion.xrel = 0; e.motion.yrel = 0;
        } else { e.button.x = g_webCenterX; e.button.y = g_webCenterY; }
    }
#endif
    // el SISTEMA cierra la app (Android: SDLActivity.onDestroy inyecta SDL_QUIT + SDL_APP_TERMINATING y bloquea su
    // hilo de UI hasta que main() vuelva): no hay a quien preguntarle; se sale siempre
    if (e.type == SDL_APP_TERMINATING) { running = false; return; }
    if (e.type == SDL_QUIT) {
#ifdef __ANDROID__
        // en Android el SDL_QUIT no lo pide el usuario (Atras no llega aca: es una tecla): lo manda el sistema al
        // destruir la actividad. Abrir el cartel dejaba el loop dibujando sobre una superficie destruida y la app
        // colgada hasta el ANR (y lo no guardado se perdia igual)
        running = false;
#else
        if (!W3dChangesAsk(W3D_CHANGES_EXIT, std::string())) running = false;
#endif
        return;
    }
#ifdef __ANDROID__
    // ATRAS (la tecla o el gesto de Android; SDL la atrapa: SDL_HINT_ANDROID_TRAP_BACK_BUTTON). Con algo abierto (un
    // menu, un cartel, la pantalla de inicio) lo cierra, como Esc. Si no, es CERRAR LA VENTANA de la PC: con algo sin
    // guardar, el cartel de lo no guardado (guardar y salir / salir sin guardar / cancelar); sin nada, pregunta si cerrar
    // Whisk3D. Aca SI se puede preguntar: lo pide el usuario desde el editor (el que no espera es el cierre del SISTEMA,
    // el SDL_QUIT de arriba). (Antes Android mandaba la app al fondo y no se cerraba nunca)
    if ((e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) && e.key.keysym.sym == SDLK_AC_BACK) {
        if (e.type == SDL_KEYUP || e.key.repeat) return;
        if (PopUpActive) PopUpActive->Key(LayoutKey::Cancel);
        else if (!W3dChangesAsk(W3D_CHANGES_EXIT, std::string())) {
            if (!confirmPopup) confirmPopup = new ConfirmPopup();
            confirmPopup->Ask(T("Close Whisk3D?"), T("Close"), T("Cancel"), BackCloseConfirmed, NULL);
        }
        g_redraw = true;
        return;
    }
#endif
#if !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
    // un ARCHIVO soltado desde el explorador del sistema (Windows / Linux / Mac): al viewport que queda bajo el puntero
    // (el 3D importa el modelo o le pone la imagen a la malla; la biblioteca lo guarda en el proyecto: ver
    // UI/ViewPorts/DropResource.cpp). Durante el arrastre del sistema la ventana no recibe el movimiento: la posicion
    // sale del mouse global menos la ventana
    if (e.type == SDL_DROPFILE) {
        if (e.drop.file) {
            int gx = 0, gy = 0, wx = 0, wy = 0;
            SDL_GetGlobalMouseState(&gx, &gy);
            SDL_GetWindowPosition(window, &wx, &wy);
            extern bool W3dReleaseFile(const std::string& path, int mx, int my, std::string* that);
            extern void Notify(const std::string& msg, bool error);
            std::string that;
            const bool ok = W3dReleaseFile(e.drop.file, gx - wx, gy - wy, &that);
            if (!that.empty()) Notify(that, !ok);
            SDL_free(e.drop.file);
        }
        return;
    }
#endif
    if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESIZED) {
#ifdef __EMSCRIPTEN__
        // WEB: el SDL avisa el tamano CSS (sin el devicePixelRatio). El tamano de verdad lo pone MainLoopFrame en el
        // proximo cuadro (ve que el canvas cambio): tomar este aca achicaba el layout un cuadro y lo volvia a agrandar
        return;
#endif
        winW = e.window.data1;
        winH = e.window.data2;
        WindowRearrange();
    } else {
        InputUserSDL3(e);
    }
}

// "--render-image" / "--render-animation" (main, abajo): 1 = una imagen, 2 = la animacion. Se lanza desde el bucle,
// con el proyecto ya dibujado una vez (la vista del render, el contexto y el tamanio de la ventana ya estan)
static int g_renderAtStart = 0, g_renderAtStartFrames = 0;
static void MainLoopFrame() {
#ifdef W3D_MCP
    W3dHistoryFrame();   // la grabadora de pasos: lo que cambio (sin W3D_HISTORY no hace nada)
    // el MCP (--mcp): los pedidos de la IA se atienden aca, entre cuadros. Si el cliente se va, el editor se cierra
    // por la misma puerta que la cruz de la ventana (con el cartel si hay cambios sin guardar)
    if (W3dMcpActive()) {
        static bool clientGone = false;
        if (!W3dMcpHandle() && !clientGone) {
            clientGone = true;
            SDL_Event q; SDL_zero(q); q.type = SDL_QUIT; SDL_PushEvent(&q);
        }
    }
#endif
#ifdef __EMSCRIPTEN__
    // EL VIDEO DE LA WEB en curso: esta vuelta es suya (renderiza unos cuadros, el navegador los codifica y se ve la
    // barra de progreso). El editor no se dibuja; la entrada la atiende la barra como en el render de la PC
    // (PCProgressPoll: Escape o su boton Cancelar lo cortan, el resto se descarta)
    if (W3dWebRenderInProgress()) {
        PCProgressPoll();
        W3dWebRenderTick();
        return;
    }
#endif
    // apertura de proyecto DIFERIDA (tarjeta Archivo): se procesa aca, con el
    // stack limpio, porque el boton que la pidio vive en el layout a destruir
    // (y el NUEVO PROYECTO de la tarjeta Archivo / la pantalla de inicio, igual: import_w3d.cpp)
    { extern void ProcessPendingProject(); ProcessPendingProject(); }
    Counters();
    // EL TITULO de la ventana dice el proyecto y un '*' si hay algo sin guardar (cada medio segundo: listar lo
    // sucio recorre la biblioteca)
    {
        static Uint32 lastTitle = 0;
        const Uint32 nowMs = SDL_GetTicks();
        if (window && (lastTitle == 0 || nowMs - lastTitle > 500)) {
            lastTitle = nowMs;
            extern std::string w3dPath;
            std::string name = w3dPath;
            const size_t b = name.find_last_of("/\\");
            if (b != std::string::npos) name = name.substr(b + 1);
            char title[512];
            snprintf(title, sizeof(title), "Whisk3D Pre-Alpha %s%s%s%s", W3dVersion(), name.empty() ? "" : " - ", name.c_str(),
                     W3dChangesHasCache() ? " *" : "");
            static std::string last;
            if (last != title) { last = title; SDL_SetWindowTitle(window, title); }
        }
    }
    W3dProfBegin(); // profiler: reset del frame
    double _profFrame0 = W3dNowMs();

    // CAPTURA + RESET de g_redraw al INICIO del frame: los subsistemas (anim de materiales, eventos, notificaciones,
    // carga diferida de texturas) lo vuelven a PRENDER si generan cambios este frame -> el idle-block de abajo lo ve el
    // frame SIGUIENTE (toasts/import fluidos) y en reposo REAL (nada prende g_redraw) bloquea en eventos (0% CPU).
    bool _needRender = g_redraw; g_redraw = false;

    // dt REAL del frame (el gameplay y las animaciones son proporcionales al
    // tiempo, NO a los fps: a 10 fps en el Nokia se recorre lo mismo por segundo)
    static double g_dtPrevMs = W3dNowMs();
    double g_dtNowMs = W3dNowMs();
    float dtFrame = (float)((g_dtNowMs - g_dtPrevMs) / 1000.0);
    g_dtPrevMs = g_dtNowMs;
    if (dtFrame < 0.0f) dtFrame = 0.0f;
    if (dtFrame > 0.1f) dtFrame = 0.1f;   // pico (debugger/carga): no dar saltos gigantes

    // VERTEX ANIM EN EL TIMELINE (kind 3): la malla sigue al PLAYHEAD. OJO: se
    // aplica SOLO cuando cambia el frame (o la anim): aplicar cada frame de
    // render PISABA los vertices mientras el usuario editaba en Edit Mode (el
    // auto key "no funcionaba": la malla volvia sola al frame del playhead).
    if (ActiveAnimKind == 3 && ActiveAnimMesh) {
        VertexAnimationActive* va = FindTargetAnim(ActiveAnimMesh);
        int ai = va ? va->currentAnim : -1;
        if (ai >= 0 && ai < (int)ActiveAnimMesh->animations.size() && ActiveAnimMesh->animations[ai] &&
            !ActiveAnimMesh->animations[ai]->frames.empty()) {
            // "es la MISMA malla que la ultima vez?" va por SERIAL (Object::serial, Objects.h) y
            // NO por puntero: borrar la malla y crear otra recicla la direccion, y el cache daba
            // "no cambio nada" para una malla DISTINTA -> su vertex anim no se evaluaba (la malla
            // nueva se quedaba en la pose del bind hasta mover el playhead).
            static unsigned int lastSerial = 0, lastGen = 0; static int lastAnim = -1, lastFrame = -999999;
            if (ActiveAnimMesh->serial != lastSerial || ai != lastAnim || CurrentFrame != lastFrame || lastGen != g_animCurvesGen) {
                lastSerial = ActiveAnimMesh->serial; lastAnim = ai; lastFrame = CurrentFrame; lastGen = g_animCurvesGen;   // (o editaron la curva)
                VertexAnimation* an = ActiveAnimMesh->animations[ai];
                an->target = ActiveAnimMesh;
                // los frames son KEYFRAMES en cuadros arbitrarios: se evalua por
                // POSICION del playhead (interpola entre los que lo rodean), no por indice.
                // Al escribir vertex[] bumpea skinGeomVersion -> el overlay de Edit Mode se
                // re-sincroniza solo (Mesh::RenderEditOverlay compara la version cacheada).
                EvalVertexAnim(*an, ActiveAnimMesh, (float)CurrentFrame);
            }
        }
    }

    // MODO JUEGO en pausa (o parado): TODO congelado, tambien las vertex anims
    // y las texturas animadas (el mundo del juego no respira si el juego no corre)
    // ... y con una VERTEX ANIM en el timeline (kind 3) el RESTO del mundo queda
    // QUIETO: solo se ve la animacion del objeto que estas editando (las demas
    // vertex anims y las texturas animadas no corren).
    if (TextFieldBlinkTick()) g_redraw = true;   // el cursor de la caja de texto enfocada parpadea
    if (!(AnimIsGame && !PlayAnimation) && ActiveAnimKind != 3) {
        UpdateAnimations(dtFrame);
        UpdateAnimatedMaterials(dtFrame);
        // ANIMACIONES UV "tira de atlas" (Core, autoplay): mismas reglas de pausa
        // que las texturas animadas. true = alguna malla cambio su uv -> redibujar.
        if (UpdateUVAnims(dtFrame)) g_redraw = true;
        // FLIPBOOKS (Image2D/particulas, registro unificado del Core): el motor avanza la UV.
        { extern bool UpdateFlipbooks(float); if (UpdateFlipbooks(dtFrame)) g_redraw = true; }
        // VISIBILIDAD POR CELDA (VisZone): evalua el modo (grilla/volumenes/curva)
        // y aplica la celda a su malla objetivo. Un paso por frame, como las particulas.
        { extern void W3dVisZonesTick(); W3dVisZonesTick(); }
        // OCLUSION POR NODOS DE RIEL (.w3dnodos): en PLAY el nodo sigue a la camara activa.
        { extern bool W3dOcclusionTick(); if (W3dOcclusionTick()) g_redraw = true; }
    }
    // PARTICULAS: la SIMULACION de lo vivo (avanzar + MATAR por vida) corre SIEMPRE que el mundo respira, TAMBIEN
    // con una vertex anim activa en el timeline (kind 3) -> sino, seleccionar/editar una vertex-anim CONGELABA las
    // particulas ya emitidas y no morian. La EMISION la controla el flag 'visible' por objeto (W3dParticlesTick).
    if (!(AnimIsGame && !PlayAnimation)) {
        extern void W3dParticlesTick(float); W3dParticlesTick(dtFrame);
    }

    // CLIP DEL ARMATURE 2D en el timeline (kind 4): el rig 2D de la malla sigue al PLAYHEAD. Se evalua
    // DESPUES de UpdateAnimations a proposito: es la REGLA DE PRECEDENCIA del uv. Si la misma malla
    // tiene ademas una vertex anim con capa uv horneada (proyectos viejos), las dos escriben mesh->uv y
    // se pisarian segun el orden -> con el clip 2D activo manda el SKINNING (corre ultimo) y la capa uv
    // queda ignorada para esa malla. Sin anims2d nada de esto corre y el camino viejo queda intacto.
    // A diferencia del kind 3, el kind 4 NO congela el resto de la escena (arriba no lo excluye).
    // MODO JUEGO (kind 2): no hay una malla "activa" del timeline -> se evalua TODA la escena, sino
    // los clips 2D no se reproducian con el Play ni en el juego compilado (el camino viejo, con la
    // pose horneada en la capa uv de la vertex anim, SI corria: sin esto era una regresion).
    if (ActiveAnimKind == 4 && ActiveAnimMesh && !ActiveAnimMesh->Arm2DAnims().empty())
        Armature2DEvaluate(ActiveAnimMesh, CurrentFrame);
    else if (ActiveAnimKind == 2 && !(AnimIsGame && !PlayAnimation))
        Armature2DEvaluateScene(CurrentFrame);

#ifdef __EMSCRIPTEN__
    // el canvas puede cambiar de tamano (ventana del browser). SDL2/Emscripten no siempre emite el
    // WINDOWEVENT_RESIZED, asi que chequeamos el tamano del canvas cada frame y resincronizamos.
    // PIXEL PERFECT: el canvas va a la resolucion REAL de la pantalla = tamano CSS x devicePixelRatio. Antes iba al
    // tamano CSS y en un iPhone (devicePixelRatio 3) el navegador estiraba la imagen x3: todo borroso. El mouse de
    // SDL se escala solo (ancho de la ventana / ancho CSS) y el dedo llega normalizado (0..1), asi que el resto del
    // editor trabaja en pixeles del dispositivo sin enterarse. El tamano del ELEMENTO tambien se mira: el manejador
    // de resize del propio SDL lo vuelve a poner en el tamano CSS cuando rota el telefono.
    {
        double cw = 0, ch = 0;
        emscripten_get_element_css_size("#canvas", &cw, &ch);
        const double dpr = W3dWebDpr();
        int nw = (int)floor(cw * dpr + 0.5), nh = (int)floor(ch * dpr + 0.5);
        int ew = 0, eh = 0;
        emscripten_get_canvas_element_size("#canvas", &ew, &eh);
        if (nw > 0 && nh > 0 && (nw != winW || nh != winH || ew != nw || eh != nh)) {
            emscripten_set_canvas_element_size("#canvas", nw, nh); // resolucion = display (nitido)
            SDL_SetWindowSize(window, nw, nh);
            winW = nw; winH = nh;
            W3dScreenHeight = winH; MenuScreenW = winW; MenuScreenH = winH;
            rootViewport->Resize(winW, winH);
            g_redraw = true;
        }
    }
#endif

    SDL_Event e;
#ifndef __EMSCRIPTEN__
    // REPOSO: si no hay animacion NI nada pendiente para redibujar, BLOQUEAR esperando el 1er evento
    // Esto reemplaza el busy-spin (que corria todo esto en loop cerrado). En emscripten NO se bloquea (el
    // requestAnimationFrame del browser no se puede frenar). El paceo cuando SI animamos lo hace el while de abajo.
    { extern bool W3dParticlesAnimating();
      // las particulas solo mantienen vivo el loop si su tick CORRE (mismas reglas de
      // pausa que el bloque de UpdateAnimatedMaterials: congeladas no redibujan nada)
      bool _partAnim = !(AnimIsGame && !PlayAnimation) && ActiveAnimKind != 3 &&
                       W3dParticlesAnimating();
      extern bool W3dInertiaActive();   // un panel que sigue deslizando solo despues de soltarlo (controls.cpp)
      bool _animPre = HasAnimationActive() || !VertexAnimationActives.empty() || PlayAnimation || W3dInertiaActive() ||
                      _partAnim;
      if (!_animPre && !_needRender && !g_redraw && !W3dProfLogOn() && SDL_WaitEventTimeout(&e, 200)) ProcessEvent(e); }
#endif
#ifdef W3D_DREAMCAST
    {   // el control de la consola como mouse (fuera del modo juego; con el juego corriendo el control es del juego)
        extern bool SimActive(); extern void W3dDcControlAsMouse(bool);
        W3dDcControlAsMouse(AnimIsGame && PlayAnimation && SimActive());
    }
#endif
#ifdef W3D_3DS
    {   // los botones y los sticks de la consola en el editor (fuera del modo juego, como en la Dreamcast)
        extern bool SimActive(); extern void W3d3dsControls(bool);
        W3d3dsControls(AnimIsGame && PlayAnimation && SimActive());
    }
#endif
#ifdef W3D_VITA
    {   // los botones y los sticks de la consola en el editor (fuera del modo juego, como en la 3DS)
        extern bool SimActive(); extern void W3dVitaControls(bool);
        W3dVitaControls(AnimIsGame && PlayAnimation && SimActive());
    }
#endif
#ifdef W3D_PSP
    {   // el control como mouse (sin pantalla tactil; fuera del modo juego, como en la Dreamcast)
        extern bool SimActive(); extern void W3dPspControls(bool);
        W3dPspControls(AnimIsGame && PlayAnimation && SimActive());
    }
#endif
    while (SDL_PollEvent(&e)) ProcessEvent(e); // manejo compartido (mismo que la espera de reposo)
    // la PULSACION LARGA tactil (agarrar una fila del outliner): se mide con el reloj, no con eventos
    { extern void ControlsTick(); ControlsTick(); }
#ifdef __EMSCRIPTEN__
    if (W3dWebRenderInProgress()) return;   // (una tecla o un boton recien empezo el video de la web: la barra ya esta dibujada)
#endif

#ifdef __EMSCRIPTEN__
    if (!running) { emscripten_cancel_main_loop(); return; } // SDL_QUIT -> salir del loop del browser
#else
    // se pidio salir (SDL_QUIT): este cuadro ya no se dibuja. En la 3DS el sistema suspende la GPU para cerrar la app
    // (cerrar desde HOME) y empezar un cuadro de citro3d esperaria a la GPU para siempre: la consola quedaba trabada
    if (!running) return;
#endif

    // CAPTURA DEL MOUSE (juego estilo FPS, PC): mientras el juego CORRE, la ventana tiene el foco y un script pidio
    // capturarRaton(true), el cursor se esconde y cada frame se lee cuanto se movio desde el CENTRO del viewport del
    // juego y se lo TELETRANSPORTA de vuelta al centro (SDL_WarpMouseInWindow). Asi nunca se sale del viewport ni de
    // la ventana (con el modo relativo de SDL el cursor se escapaba y dejaba de girar la vista). El delta llega a
    // ratonRel() de lua. ESC pausa el juego (PlayAnimation = false, controls.cpp) -> se suelta el mouse; Play /
    // espacio lo vuelve a capturar. Sin foco (alt-tab) no se toca el mouse del usuario.
    {
        extern bool SimActive(); extern bool W3dScriptMouseCapture(); extern void W3dScriptMouseRel(float, float);
        extern void W3dScriptCapturedMouseSet(bool); extern bool GameViewportRect(int*, int*, int*, int*);
        static bool mouseCaptured = false;
        int vx = 0, vy = 0, vw = 0, vh = 0;
        const Uint32 wf  = window ? SDL_GetWindowFlags(window) : 0;
        const bool focus = (wf & SDL_WINDOW_INPUT_FOCUS) != 0 && (wf & SDL_WINDOW_MINIMIZED) == 0;
        const bool wants = AnimIsGame && PlayAnimation && SimActive() && W3dScriptMouseCapture() && focus &&
                            GameViewportRect(&vx, &vy, &vw, &vh);
#ifdef __EMSCRIPTEN__
        // NAVEGADOR: POINTER LOCK. El navegador lo concede recien con un click o una tecla (el pedido queda armado
        // hasta entonces: el aviso lo dice). Si lo suelta por su cuenta (Esc, otra pestania) es el Esc de PC: pausa.
        static bool lockedPrev = false;
        const bool locked = g_webPointerRequest && WebLockedPointer();
        if (wants) {
            g_webCenterX = vx + vw / 2; g_webCenterY = vy + vh / 2;
            // (SDL lo arma solo con el puntero sobre el canvas: si no, se reintenta el cuadro siguiente)
            if (!g_webPointerRequest && SDL_SetRelativeMouseMode(SDL_TRUE) == 0) {
                g_webPointerRequest = true;
                if (!WebLockedPointer()) Notify(T("Click the game to capture the mouse (Esc releases it)"), false);
            }
            if (locked) {
                if (g_webMouseDX != 0.0f || g_webMouseDY != 0.0f) { W3dScriptMouseRel(g_webMouseDX, g_webMouseDY); g_redraw = true; }
            } else if (lockedPrev) {
                PlayAnimation = false;   // el navegador solto el mouse (Esc): pausa, como el Esc de PC
                g_redraw = true;
            }
        } else if (g_webPointerRequest) {
            SDL_SetRelativeMouseMode(SDL_FALSE);
            g_webPointerRequest = false;
        }
        g_webMouseDX = 0.0f; g_webMouseDY = 0.0f;
        lockedPrev    = wants && locked;
        mouseCaptured = wants && locked;
#else
        if (wants) {
            const int cx = vx + vw / 2, cy = vy + vh / 2;
            int mx = cx, my = cy; SDL_GetMouseState(&mx, &my);
            if (!mouseCaptured) {
                SDL_ShowCursor(SDL_DISABLE);
                PCWarpMouse(cx, cy);   // el primer salto al centro no cuenta como movimiento
                mouseCaptured = true;
            } else {
                const int dx = mx - cx, dy = my - cy;
                if (dx || dy) { W3dScriptMouseRel((float)dx, (float)dy); PCWarpMouse(cx, cy); g_redraw = true; }
            }
        } else if (mouseCaptured) {
            SDL_ShowCursor(SDL_ENABLE);
            mouseCaptured = false;
        }
#endif
        W3dScriptCapturedMouseSet(mouseCaptured);
    }
    // Animación
    Uint32 now = SDL_GetTicks();
    // el avance de frames va al ritmo de AnimFPS (default 30), independiente de los fps de la UI (que puede ir a 60).
    // Asi la animacion NO va "muy rapido": un frame de animacion dura 1000/AnimFPS ms aunque se dibuje 2 veces.
    unsigned int animMs = (AnimFPS > 0) ? (unsigned int)(1000 / AnimFPS) : 33;
    // el MODO JUEGO va a 60 fps fijo (la fisica de los scripts se siente fluida);
    // el timeline de animacion normal conserva su AnimFPS
    if (AnimIsGame && PlayAnimation) animMs = 1000 / 60;
    if (now - lastAnimTime >= animMs) {
        lastAnimTime = now;
        // juego en PAUSA (partida cargada sin Play): el mixer se congela (la musica no sigue sonando de fondo);
        // SimTickPlay lo despausa al reanudar y SimStop corta todo
        { extern bool SimActive(); if (AnimIsGame && !PlayAnimation && SimActive() && !w3dEngine::W3dAudioPaused()) w3dEngine::W3dAudioPause(true); }
        // Timeline: si esta en PLAY, avanzar CurrentFrame (loop Start..End) y forzar redibujo
        if (PlayAnimation) {
            extern void SimTickPlay(float); extern bool SimHasScripts(); extern bool SimActive();
            bool game = AnimIsGame && (SimActive() || SimHasScripts());
            // el dt de la SIMULACION tambien es TIEMPO REAL entre ticks (el fisico
            // del juego rinde lo mismo a 30, 60 o 10 fps)
            static Uint32 lastSimMs = 0;
            float dtSim;
            if (lastSimMs == 0 || now < lastSimMs) dtSim = game ? (1.0f / 60.0f)
                                : 1.0f / (AnimFPS > 0 ? (float)AnimFPS : 30.0f);
            else {
                dtSim = (float)(now - lastSimMs) / 1000.0f;
                if (dtSim > 0.1f) dtSim = 0.1f;
                if (dtSim < 0.001f) dtSim = 0.001f;
            }
            lastSimMs = now;
            if (game && AnimPlayDir < 0) {
                // PLAY EN REVERSA en modo juego: el juego no graba su pasado, no hay nada que rebobinar -> pausa
                PlayAnimation = false;
            } else if (game) {
                // MODO JUEGO hacia adelante: el playhead lo lleva la simulacion
                // (sin AnimTick: no hay Fin ni loop)
                SimTickPlay(dtSim);
            } else {
                // animacion NORMAL: reproduce con su loop de siempre, sin tocar el juego
                AnimTick();
            }
            g_redraw = true;
        }
        // Actualizar frame
    }
    // LOS SONIDOS de la escena (objetos Sonido): suenan con el Play del timeline, en segundos (libs/Whisk3DCore/objects/Sound.cpp)
    { extern void W3dSoundsTickEditor(); W3dSoundsTickEditor(); }
    // ANIMACION DE OBJETOS (transform pos/rot/escala): aplica los keyframes al frame actual (play o scrub del timeline).
    // Guard interno: solo trabaja al CAMBIAR de frame -> editar/keyframear un objeto en un frame fijo no lo resnapea.
    { extern void ApplyAnimationObjects(); ApplyAnimationObjects(); }

    // notificaciones (toasts): corre el timer (las de exito se cierran solas) y
    // mantiene vivo el render mientras haya alguna de exito activa
    static Uint32 lastNotif = now;
    NotificationsTick((float)(now - lastNotif) / 1000.0f);
    lastNotif = now;

    // build del juego en 2do plano ("Compilar juego"): mientras corre mantiene vivo el
    // render (la barra avanza) y al terminar el worker hace join + Notificar exito/error.
    CompileGameTick();

    // RED DE SEGURIDAD de la barra del dock: si no hay NINGUNA operacion con progreso
    // (ni build, ni import/export/render con la barra modal) el icono no puede quedar
    // con una barra a medias. Cubre cualquier camino de error que se haya olvidado de
    // llamar a ProgressEnd(). Es no-op si ya estaba apagada.
    if (!CompileGameInProgress() && !ProgressActive()) W3dDockProgressEnd();

    // CARGA DIFERIDA de texturas del import: 1 por frame (decode+upload). Prende g_redraw al cargar una -> el
    // modelo aparece enseguida y las texturas entran solas. No-op si no hay pendientes.
    { extern void LoadPendingTextures(); LoadPendingTextures(); }
    // la BOMBA del almacen de recursos: UNA carga async por frame (streaming).
    // Cola vacia = no-op: en el editor no cuesta nada.
    { extern void W3dResourcesPump(); W3dResourcesPump(); }
    // la VISTA PREVIA del STREAMING (io/Streaming.h): sin jugar y con la opcion del proyecto prendida, las instancias
    // diferidas cargan y descargan segun la vista del viewport (jugando lo hace el tick de la partida)
    { extern void W3dStreamingTickEditor(); W3dStreamingTickEditor(); }

    // Render EVENT-DRIVEN: solo si algo cambio (g_redraw) o hay una animacion EN
    // PLAY (vertex-anim o materiales animados activos). Sino no se dibuja nada ->
    // CPU casi 0 en reposo (como Blender), en vez de renderizar 60 veces/seg al pedo.
    extern bool W3dParticlesAnimating();
    // un emisor vivo mantiene el redibujado, pero SOLO si su tick corre (ver _partAnim arriba)
    bool animating = HasAnimationActive() || !VertexAnimationActives.empty() || PlayAnimation ||
                    (!(AnimIsGame && !PlayAnimation) && ActiveAnimKind != 3 && W3dParticlesAnimating());
    double _renderNowMs = W3dNowMs(); // reloj de alta resolucion (para el profiler)
#ifdef __EMSCRIPTEN__
    // WebGL: renderizar SIEMPRE (no event-driven). Motivos: 1) el canvas WebGL usa
    // preserveDrawingBuffer=false -> en un frame sin dibujar el browser BORRA el canvas a negro
    // (parpadeos/negro al quedar quieto); 2) el color-ID pick (hover/loop cut) limpia el framebuffer
    // visible y hay que taparlo cada frame. El RAF ya limita a la tasa del monitor y se PAUSA en
    // tabs ocultas, asi que no quema GPU de fondo. (En escritorio se mantiene el event-driven.)
    bool doRender = true; (void)animating; (void)_renderNowMs;
#else
    // (W3D_PROF_ORBIT=1 con el log del perfil: la vista 3D activa gira sola, para medir la navegacion donde no se puede
    // arrastrar a mano: el telefono por adb)
    // (W3D_PROF_MOVE=1: el objeto activo va y viene, como arrastrarlo con el gizmo)
    if (W3dProfLogOn()) {
        static int orbit = -1, move = -1, step = 0;
        if (orbit < 0) { const char* e = getenv("W3D_PROF_ORBIT"); orbit = (e && *e == '1') ? 1 : 0; }
        if (move < 0) { const char* e = getenv("W3D_PROF_MOVE"); move = (e && *e == '1') ? 1 : 0; }
        extern Viewport3D* Viewport3DActive;
        if (orbit && Viewport3DActive) Viewport3DActive->OrbitArrow(2, 0);
        if (move && ActiveObj) {
            ActiveObj->pos.x += ((step++ / 30) & 1) ? -0.03f : 0.03f;
            extern bool g_objectsMoved; g_objectsMoved = true;
        }
    }
    bool doRender = (_needRender || g_redraw || animating || W3dProfLogOn()); // _needRender = del frame previo; g_redraw = eventos/subsistemas de este.
    // (con el log del perfil se dibujan todos los cuadros: son los que se miden)
    // El CAP a 60fps lo hace el LOOP (paceo por sleep), no aca -> sin busy-spin.
#endif
    g_didRender = doRender; // el loop de escritorio lo lee: si dibujo -> pacea 60fps; si no -> bloquea en eventos (0% CPU)
    g_prof.logic = _renderNowMs - _profFrame0; // profiler: input + anim + notificaciones + carga diferida de texturas
    if (doRender) {
        lastRenderTime = now;
        double _profRender0 = W3dNowMs();
#ifdef W3D_3DS
        w3dEngine::C3DFrameBegin();                     // (la 3DS: el cuadro de citro3d, el editor en la de abajo)
        { extern void W3d3dsScreenUp(); W3d3dsScreenUp(); }   // (la vista grande; deja elegida la de abajo)
#endif
#ifdef W3D_PSP
        w3dEngine::GuFrameBegin();                      // (la PSP: la lista del GE de este cuadro)
#endif
        { extern void KeyboardsWatch(); KeyboardsWatch(); }   // (el teclado de Whisk3D del IDE se fue: el IDE vuelve)
        if (rootViewport){
            // (navegando una vista 3D: los paneles salen de la imagen guardada y se dibujan solo las vistas 3D, ver
            // W3dViewCacheBegin en ViewPorts.h)
            const bool only3D = W3dViewCacheBegin(rootViewport, winW, winH);
            if (only3D) W3dViewCacheRender3D(rootViewport);
            else rootViewport->Render();
            W3dViewCacheEnd(winW, winH, only3D);
        }
        // popups y desplegables (los dibuja el modulo compartido)
        LayoutRenderMenu(winW, winH);
        { extern void KeyboardSystemFieldRender(); KeyboardSystemFieldRender(); }   // el campo, arriba del teclado del sistema
        { extern void LoadFingerRender(int, int); LoadFingerRender(winW, winH); }   // el circulo de carga de la pulsacion larga
        NotificationsRender(winW, winH); // toasts ENCIMA de todo
        // barra del build de "Compilar juego": overlay NO modal (el MISMO widget que la
        // barra del export de modelos) anclado abajo; muestra la etapa + el porcentaje
        // que publica el worker. El editor sigue usable: la barra solo se dibuja, no
        // captura input ni bloquea el loop.
        if (CompileGameInProgress()) {
            std::string stageBld;
            int pctBld = CompileGameProgress(&stageBld);
            ProgressOverlayRender(stageBld, (float)pctBld / 100.0f);
        }
        LayoutTickFPS(now); // overlay de fps (reloj de PC: SDL_GetTicks)
        g_prof.render = W3dNowMs() - _profRender0; // profiler: TODO el render (viewports 3D + paneles UI + menus)
        double _profSwap0 = W3dNowMs();
#ifdef W3D_DREAMCAST
        { extern void W3dDcDrawCursor(int, int); W3dDcDrawCursor(winW, winH); }   // (la consola no tiene cursor propio)
#endif
#ifdef W3D_PSP
        { extern void W3dPspDrawCursor(int, int); W3dPspDrawCursor(winW, winH); }   // (el cursor que mueve el control)
#endif
#ifdef W3D_3DS
        w3dEngine::C3DFrameFinish();
        { extern void W3d3dsDrawnFrame(); W3d3dsDrawnFrame(); }   // (la captura de prueba en la SD)
#else
        w3dEngine::DrawPending();
#ifdef W3D_VITA
        { extern void W3dVitaDrawnFrame(); W3dVitaDrawnFrame(); }   // (la captura de prueba, antes de mostrarlo)
#endif
        W3dCaptureFrame(window);
        W3dPresent(window);
#endif
#ifdef W3D_DREAMCAST
        { extern void W3dDcDrawnFrame(); W3dDcDrawnFrame(); }   // (la captura de prueba por la consola serie)
#endif
#ifdef W3D_PSP
        { extern void W3dPspDrawnFrame(); W3dPspDrawnFrame(); }   // (la captura de prueba en el Memory Stick)
#endif
        g_prof.swap = W3dNowMs() - _profSwap0; // profiler: swap (con vsync ON = espera al vblank)
        W3dProfEnd(); // profiler: suaviza g_profShow hacia g_prof (numeros estables). NO se resetea g_redraw aca: se
        // capturo+reseteo al inicio del frame; lo que los subsistemas (notif/texturas) prendieron queda para el proximo.

#ifdef __EMSCRIPTEN__
        // apenas dibujamos el PRIMER frame, ocultamos la pantalla de carga del shell. Es lo mas
        // robusto: no depende de setStatus('')/onRuntimeInitialized (que con emscripten_set_main_loop
        // pueden no dispararse porque main() no retorna). Se hace una sola vez.
        static bool hiddenLoader = false;
        if (!hiddenLoader) {
            hiddenLoader = true;
            EM_ASM({ var l = document.getElementById('loader'); if (l) l.classList.add('hidden'); });
        }
#endif
    }

    // --render-image / --render-animation: al tercer cuadro dibujado (el render bloquea con su barra hasta terminar)
    if (g_renderAtStart && g_didRender && ++g_renderAtStartFrames >= 3) {
        const bool anim = g_renderAtStart == 2; g_renderAtStart = 0;
        extern void W3dRenderAtStart(bool); W3dRenderAtStart(anim);
        g_redraw = true;
    }

    // (el Delay(8) fijo que vivia aca era un vestigio del busy-spin viejo: sumado al
    // vsync del swap clavaba el editor en ~30 fps. El paceo real lo hace el loop de
    // escritorio: duerme hasta el proximo frame de 60 y en reposo bloquea en eventos.)
}

namespace w3dEngine { bool W3dAudioInit(int); }  // audio/W3dAudio.h (evita meter el dir al include path)
#ifdef __ANDROID__
// las franjas del render a archivo en la imagen fuera de pantalla del backend (ver main, despues de GLES2Init)
static bool W3dOutsideScreenFbo(int w, int h) { return w3dEngine::CaptureBegin(w, h); }
static void W3dOutsideScreenFboEnd() { w3dEngine::CaptureFinish(); }
#endif

int main(int argc, char* argv[]) {
#ifdef W3D_MCP
    // --mcp: antes que nada imprima, stdin y stdout pasan a ser del protocolo (ver test/W3dMcp.cpp)
    for (int ai = 1; ai < argc; ai++)
        if (std::string(argv[ai]) == "--mcp" && !W3dMcpStart()) { fprintf(stderr, "whisk3d --mcp: no se pudo iniciar\n"); return 1; }
#endif
#ifndef __EMSCRIPTEN__
    w3dRedirectCoutToLog();   // cout/cerr -> whisk3d.log (no la consola blanca de Windows). Impl comun en w3dlog.cpp.
#endif
#ifdef W3D_MCP
    W3dHistoryStart();     // W3D_HISTORY=<ruta.txt>: anota los pasos de la persona (para armar tutoriales)
#endif
#ifdef __EMSCRIPTEN__
    SDL_SetMainReady(); // web: no arrancamos por el main de SDL
    // touch -> mouse SIEMPRE sintetizado por SDL (which = SDL_TOUCH_MOUSEID): el ruteo tactil de
    // controls.cpp distingue por ese which los eventos del dedo de los mouse FANTASMA del browser.
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "1");
    // en web solo VIDEO: el subsistema de gamepad puede no estar y haria fallar todo el SDL_Init
    Uint32 initFlags = SDL_INIT_VIDEO;
#else
    Uint32 initFlags = SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER;
#endif
#ifdef __ANDROID__
    // MEDIR EN EL TELEFONO sin recompilar: las variables de /data/local/tmp/w3d_env.txt (NOMBRE=valor por linea, con adb
    // push), como en los juegos compilados en modo debug: W3D_PROF_LOG=1 (el perfil cada 2 s en el log), W3D_CAPTURE=n
    // (la pantalla del cuadro n en /sdcard/w3d_captura.ppm)
    { FILE* fe = fopen("/data/local/tmp/w3d_env.txt", "r");
      if (fe) {
          char l[512];
          while (fgets(l, sizeof(l), fe)) {
              l[strcspn(l, "\r\n")] = 0;
              char* eq = strchr(l, '=');
              if (eq && eq != l) { *eq = 0; setenv(l, eq + 1, 1); }
          }
          fclose(fe);
      } }
    // Al minimizar: SDL BLOQUEA el loop de main (NO mata el hilo nativo) y conserva el contexto GL. Sin esto, al volver
    // a abrir se relanzaba main() con los globals C++ estaticos vivos -> ConstructUniversal duplicaba la escena (otro
    // cubo/camara/luz) y las texturas quedaban negras (contexto GL nuevo). Junto con launchMode=singleTask del manifest.
    SDL_SetHint(SDL_HINT_ANDROID_BLOCK_ON_PAUSE, "1");
    // ATRAS llega como tecla (SDLK_AC_BACK): la maneja ProcessEvent (pregunta si cerrar). Sin esto Android la usaba
    // para mandar la app al fondo
    SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
#endif
#if defined(__linux__) && !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
    // ICONO EN EL DOCK (Linux): GNOME matchea el app-id de la ventana contra el
    // nombre del .desktop instalado (whisk3d.desktop). Este SDL2 no tiene
    // SDL_HINT_APP_ID (es de SDL3): el backend Wayland toma el app_id xdg del
    // env SDL_VIDEO_WAYLAND_WMCLASS y el de X11 toma el WM_CLASS del env
    // SDL_VIDEO_X11_WMCLASS. Se setean ANTES de SDL_Init (el video los lee al
    // inicializar). El SDL_SetWindowIcon de constructor.cpp sigue valiendo para
    // X11/Windows; en Wayland el dock resuelve el icono por este app-id.
    SDL_setenv("SDL_VIDEO_WAYLAND_WMCLASS", "whisk3d", 1);
    SDL_setenv("SDL_VIDEO_X11_WMCLASS", "whisk3d", 1);
    // MODO TEST SIN VENTANAS NI SONIDO: "whisk3d --script" dibuja en el driver 'offscreen' de SDL (GL por EGL, sin
    // abrir nada en el escritorio: las suites no llenan la pantalla de ventanas). Para ver la ventana:
    // W3D_SHOW_WINDOW=1. Un SDL_VIDEODRIVER explicito siempre manda.
    for (int ai = 1; ai < argc; ai++)
        if (std::string(argv[ai]) == "--script" && !SDL_getenv("W3D_SHOW_WINDOW") && !SDL_getenv("SDL_VIDEODRIVER")) {
            SDL_setenv("SDL_VIDEODRIVER", "offscreen", 1);
            if (!SDL_getenv("SDL_AUDIODRIVER")) SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);   // y sin sonido
            break;
        }
#endif
#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
    // VULKAN SIN VENTANA en Linux: el driver 'offscreen' de SDL no da superficie de Vulkan, asi que una prueba con Vulkan
    // va por X11 (SDL_VIDEODRIVER=x11). Con W3D_WINDOW_HIDDEN=1 esa ventana se crea OCULTA: no aparece en el escritorio
    if (SDL_getenv("W3D_WINDOW_HIDDEN")) g_windowHidden = true;
#endif
#ifdef _WIN32
    // lo mismo en Windows: el driver 'offscreen' de SDL necesita EGL (no hay), asi que la ventana se crea OCULTA
    // (SDL_WINDOW_HIDDEN: contexto WGL real, no aparece en el escritorio ni en la barra de tareas). W3D_SHOW_WINDOW=1
    // la muestra.
    for (int ai = 1; ai < argc; ai++)
        if (std::string(argv[ai]) == "--script" && !SDL_getenv("W3D_SHOW_WINDOW")) {
            g_windowHidden = true;
            if (!SDL_getenv("SDL_AUDIODRIVER")) SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);   // y sin sonido
            break;
        }
#endif
#ifdef W3D_DREAMCAST
    // la Dreamcast: el driver de video de SDL engancha GLdc (OpenGL 1.x de la PVR) solo en este modo; sin el hint
    // abre un framebuffer de software y no hay contexto GL
    SDL_SetHint(SDL_HINT_DC_VIDEO_MODE, "SDL_DC_OPENGL_VIDEO");
#endif
#ifdef W3D_VITA
    // la Vita: el click del mouse sale del toque (como en Android) solo si se dice de que panel. Sin el hint, SDL no lo
    // arma para ninguno y el editor no ve los toques (los gestos de dos dedos si, pero no un toque solo)
    SDL_SetHint(SDL_HINT_VITA_TOUCH_MOUSE_DEVICE, "0");   // (la pantalla; el panel de atras no)
#endif
    if (SDL_Init(initFlags) != 0) {
        std::cerr << "Error SDL_Init: " << SDL_GetError() << std::endl;
        return -1;
    }
#ifdef W3D_VITA
    // la Vita: sin el compilador de shaders del sistema vitaGL no puede dibujar (se avisa y se sale, ver w3d_vita.cpp)
    { extern bool W3dVitaCanDraw(); if (!W3dVitaCanDraw()) { SDL_Quit(); return 1; } }
#endif

    W3dSetClock(ClockSDL);   // el motor usa el reloj de PARED de SDL en vez de su respaldo
    w3dFileSystem::Init();
    #if !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
    // SIN res/ el editor abre igual pero sin texturas ni iconos (cuadrados blancos) y sin decir por que. Se avisa
    // claro: en el log, en la consola y con un cartel del sistema (en --script solo el log: no hay nadie mirando).
    if (!w3dFileSystem::IsDir(w3dFileSystem::GetResDir() + "/Skins")) {
        const std::string msg = "No se encontro la carpeta de recursos de Whisk3D (res/).\n"
                                "Se busco en: " + w3dFileSystem::GetResDir() + "\n"
                                "Instala el paquete (.deb) o deja la carpeta res/ al lado del ejecutable.";
        w3dLogfE("%s", msg.c_str());
        std::cerr << msg << std::endl;
        bool inScript = false;
        for (int ai = 1; ai < argc; ai++) if (std::string(argv[ai]) == "--script") inScript = true;
        if (!inScript) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Whisk3D", msg.c_str(), NULL);
    }
    #endif
    // AUDIO: arranca el mixer (efectos de los juegos: beep() de los scripts lua). Si el device no
    // abre, la app sigue MUDA (W3dAudioInit devuelve false y todo queda no-op). 44100 en desktop/web.
    w3dEngine::W3dAudioInit(44100);

    // Carpeta ESCRIBIBLE por-app para config/bookmarks. SDL_GetPrefPath la crea si no
    // existe (Win: AppData, Linux: ~/.local/share, Android: internal storage). En Android
    // el GetResDir es el APK (solo lectura) -> sin esto los bookmarks no se guardaban.
    {
    #ifdef W3D_DREAMCAST
        w3dFileSystem::SetUserDataDir("/ram");   // la Dreamcast: lo unico escribible es el disco en RAM
    #elif defined(W3D_PSP)
        // la PSP: la carpeta del EBOOT.PBP (la carpeta actual al arrancar). El SDL_GetPrefPath de la PSP arma dos
        // carpetas una adentro de la otra y crea solo la ultima: no existia
        { char cwd[512]; if (getcwd(cwd, sizeof(cwd))) w3dFileSystem::SetUserDataDir(cwd); }
    #else
        char* pref = SDL_GetPrefPath("Whisk3D", "Whisk3D");
        if (pref) { w3dFileSystem::SetUserDataDir(pref); SDL_free(pref); }
    #endif
    }

    // Android: los recursos van dentro del APK (assets). El Core los lee con el
    // AAssetManager del NDK (NO SDL); aca -en la capa de plataforma, que si usa
    // SDL- lo sacamos del Activity via JNI y se lo pasamos una sola vez al Core.
    #ifdef __ANDROID__
    {
        JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
        jobject activity = (jobject)SDL_AndroidGetActivity();
        if (env && activity) {
            jclass clazz = env->GetObjectClass(activity);
            jmethodID mid = env->GetMethodID(clazz, "getAssets", "()Landroid/content/res/AssetManager;");
            jobject amJava = env->CallObjectMethod(activity, mid);
            AAssetManager* am = AAssetManager_fromJava(env, amJava);
            w3dFileSystem::SetAssetManager(am);
            env->DeleteLocalRef(amJava);
            env->DeleteLocalRef(clazz);
            env->DeleteLocalRef(activity);
        }
    }
    // PERMISOS por NIVEL DE API. OJO: SDL_AndroidRequestPermission BLOQUEA el arranque esperando el callback del
    // sistema; pedir un permiso NO declarado para esa API (ej. WRITE_EXTERNAL_STORAGE en API>=30, maxSdkVersion=29)
    // es inutil (scoped storage: no se puede otorgar) y RIESGOSO (puede no llegar el callback y colgar el inicio ->
    // "no anda en Android 11+"). Por eso se piden SOLO los que corresponden.
    int androidApi = SDL_GetAndroidSDKVersion();
    if (androidApi <= 29) {
        // Android <=10 (legacy storage): permisos clasicos. Con requestLegacyExternalStorage, el fopen a /sdcard anda.
        SDL_AndroidRequestPermission("android.permission.READ_EXTERNAL_STORAGE");
        SDL_AndroidRequestPermission("android.permission.WRITE_EXTERNAL_STORAGE");
    }
    // Android 11+ (API>=30): "All files access" (MANAGE_EXTERNAL_STORAGE) para leer/escribir ARCHIVOS arbitrarios
    // (modelos/texturas) con el file browser interno; lo pide el Activity (Whisk3DActivity.onCreate abre Settings).
    // Los permisos de media NO sirven (solo imagenes/videos). Con "All files access" otorgado, Descargas vuelve a ser
    // escribible por fopen -> la salida por defecto sigue siendo Descargas (ver GetDefaultOutputDir), como en Android<=10.
    #endif

    // Configuración OpenGL
    #ifdef __ANDROID__
        // Android usa el backend ES2 (GLES2Init mas abajo)
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    #elif defined(__EMSCRIPTEN__)
        // WebGL: Emscripten crea el contexto WebGL (ES2) por su cuenta; no hay que forzar
        // version ni profile (igual que en los ejemplos). El backend ES2 se prende con GLES2Init.
    #else
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    #endif
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    // ESTENCIL: lo pide el recorte por FORMA del reflejo del espejo (la lamina de
    // agua marcada con z-test, para que lo que pase por delante del estanque no
    // reciba reflejo). 8 bits es lo que da cualquier driver junto al z de 24
    // (D24S8); si el driver lo niega, w3dEngine::StencilBits() devuelve 0 y el
    // espejo se recorta igual por planos (solo pierde la oclusion).
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    // OJO: asignar el cfg GLOBAL (no declarar uno local que lo tape) -> asi ConstructUniversal
    // y todo lo demas ven los valores cargados (ej. scale para SetGlobalScale).
    // El config del USUARIO vive en la carpeta escribible (SDL_GetPrefPath): res/ puede ser de solo lectura (el
    // AppImage, el .deb instalado en /usr/share, el APK). Si todavia no guardo nada, vale el de fabrica de res/.
    {
        const std::string cfgUser = w3dFileSystem::GetUserDataDir() + "/config.ini";
        FILE* fu = fopen(cfgUser.c_str(), "rb");
        const bool hasUser = (fu != NULL);
        if (fu) fclose(fu);
    #ifdef __ANDROID__
        cfg = loadConfig(hasUser ? cfgUser : std::string("res/config.ini"));
        // Android es de alta densidad y se usa con el dedo -> escala 3 de fabrica (el config compartido viene en 2
        // para PC). Si el usuario ya eligio una escala en Ajustes, se respeta la suya.
    #ifdef W3D_ANDROID_OLD
        // (los de Android 2.3 tienen pantallas de 320x480 o menos, con la densidad del N95: escala 1)
        if (!hasUser) cfg.scale = 1;
    #else
        if (!hasUser) cfg.scale = 3;
    #endif
    #else
        cfg = loadConfig(hasUser ? cfgUser : w3dFileSystem::GetResDir() + "/config.ini");
    #endif
    #ifdef __EMSCRIPTEN__
        // WEB: el canvas va a la resolucion REAL de la pantalla (CSS x devicePixelRatio, ver MainLoopFrame), asi que
        // la escala de la UI se cuenta en pixeles del DISPOSITIVO. Con el dedo (iPhone, Android, iPad) 4; con mouse,
        // la del config multiplicada por el devicePixelRatio (una Mac retina: 2 x 2 = 4), asi la interfaz mide lo
        // mismo que antes pero nitida. Si el usuario eligio una escala en Ajustes, manda la suya.
        if (!hasUser) cfg.scale = W3dWebScaleUI(cfg.scale);
        { extern bool g_w3dPhone; g_w3dPhone = W3dWebFinger(); }   // el layout de fabrica del telefono (LayoutDefault)
    #endif
    }
    winW = cfg.width;  winH = cfg.height;  // aplicar el tamano del config a la ventana
                                        // (antes quedaba el default 640x480: el config no se usaba)
#ifdef W3D_DREAMCAST
    // la Dreamcast: la salida de video es 640x480 (VGA o TV), la UI a escala 1 y sin antialiasing (la PVR no lo hace)
    winW = 640; winH = 480; cfg.fullscreen = true; cfg.scale = 1; cfg.enableAntialiasing = false;
#endif
#ifdef W3D_3DS
    // la 3DS: el editor en la pantalla de abajo (la tactil, 320x240), la UI a escala 1 y sin antialiasing
    winW = 320; winH = 240; cfg.fullscreen = true; cfg.scale = 1; cfg.enableAntialiasing = false;
#endif
#ifdef W3D_PSP
    // la PSP: la pantalla de 480x272, la UI a escala 1 (como la 3DS) y sin antialiasing
    winW = 480; winH = 272; cfg.fullscreen = true; cfg.scale = 1; cfg.enableAntialiasing = false;
#endif
#ifdef W3D_VITA
    // la Vita: la pantalla de 960x544 (unos 220 puntos por pulgada, como un telefono: la UI a la escala 2 de fabrica
    // del config, o la que eligio el usuario) y sin antialiasing
    winW = 960; winH = 544; cfg.fullscreen = true; cfg.enableAntialiasing = false;
#endif
#ifdef W3D_ANDROID_OLD
    // Android 2.3: sin antialiasing. La VideoCore IV lo da (4x), pero con tiles cuatro veces mas chicos el cuadro tardaba
    // ~300 ms en la GPU
    cfg.enableAntialiasing = false;
#endif
    // AUTO KEY: arranca SIEMPRE apagado (pedido del dueno); el autoKey del config.start ya no se aplica.
    // LOGS: el nivel del config (Ajustes > Logs)
    g_w3dLogLevel = cfg.logs;

#ifdef W3D_MULTIBACKEND
    // EL BACKEND GRAFICO se elige ACA, antes de crear la ventana (graphics/w3dBackend.h): Ajustes > Graphics
    // (cfg.graphicsAPI), o "--backend <OpenGL|es2>" en la linea de comandos (el harness corre las pruebas con cada uno)
    {
        std::string request = cfg.graphicsAPI;
        for (int k = 1; k + 1 < argc; k++) if (std::string(argv[k]) == "--backend") request = argv[k + 1];
        if (request == "es2" || request == "gles2") request = "OpenGL ES 2.0";
        const int b = w3dEngine::BackendFromName(request.c_str());
        if (b >= 0 && !w3dEngine::BackendUse(b))
            w3dLogfW("[graficos] '%s' no esta en este ejecutable: se usa OpenGL", request.c_str());
        w3dLogf("[graficos] backend: %s", w3dEngine::BackendName(w3dEngine::ActiveBackend()));
    }
#endif
    //TODO: Esto muy probablemente de problemas en gama baja? no se si deba configurable
    // (multisample / estencil / profundidad: los prueba CreateWindowGL, de lo mas completo a lo mas basico)
    #ifdef __ANDROID__
        window = CreateWindowGL("Whisk3D Pre-Alpha", winW, winH,
                                SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE,
                                cfg.enableAntialiasing, &glContext);
        if (window) W3dFullscreenAuto(window);   // sin notch: pantalla completa (la barra de arriba ocupa espacio valioso)
    #else
        // fullscreen -> maximizada; sino respeta width/height del config (util para
        // probar tamanos chicos, ej. 240x320 estilo N95 con scale=1)
    #ifdef W3D_DREAMCAST
        Uint32 windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN;   // (la consola es una sola pantalla)
    #elif defined(W3D_3DS)
        Uint32 windowFlags = SDL_WINDOW_FULLSCREEN;   // (sin OpenGL: ver CreateWindowGL)
    #elif defined(W3D_VITA)
        Uint32 windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN;   // (la consola es una sola pantalla)
    #elif defined(W3D_PSP)
        Uint32 windowFlags = SDL_WINDOW_FULLSCREEN;   // (sin OpenGL: ver CreateWindowGL)
    #else
        Uint32 windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
        if (cfg.fullscreen) windowFlags |= SDL_WINDOW_MAXIMIZED;
        if (g_windowHidden) windowFlags = (windowFlags & ~SDL_WINDOW_MAXIMIZED) | SDL_WINDOW_HIDDEN;   // --script en Windows
    #endif
#ifdef W3D_VULKAN
        // VULKAN: su ventana (sin contexto de GL). Si no arranca en esta maquina (sin driver, sin GPU, sin shaderc) se sigue
        // con OpenGL, como si se hubiera elegido ese
        if (w3dEngine::ActiveBackend() == w3dEngine::BackendVulkan) {
            window = CreateWindowVulkan("Whisk3D Pre-Alpha", winW, winH, windowFlags);
            if (!window) { w3dLogfW("[graficos] Vulkan no arranco en esta maquina: se usa OpenGL"); w3dEngine::BackendUse(w3dEngine::BackendOpenGL); }
        }
        if (!window)
#endif
        window = CreateWindowGL("Whisk3D Pre-Alpha", winW, winH, windowFlags, cfg.enableAntialiasing, &glContext);
        #if !defined(__EMSCRIPTEN__) && !defined(_WIN32)
        // ULTIMO RESPALDO: en una sesion Wayland, SDL abre por X11 (XWayland) y ahi puede no haber GLX usable.
        // Si nada abrio, se reintenta con el driver Wayland, que crea el contexto por EGL y no necesita GLX.
        if (!window && std::getenv("WAYLAND_DISPLAY")) {
            const char* drv = SDL_GetCurrentVideoDriver();
            if (drv && std::string(drv) != "wayland") {
                w3dLogfW("Sin ventana GL por %s: se prueba con Wayland (EGL)", drv);
                SDL_QuitSubSystem(SDL_INIT_VIDEO);
                SDL_setenv("SDL_VIDEODRIVER", "wayland", 1);
                if (SDL_InitSubSystem(SDL_INIT_VIDEO) == 0)
                    window = CreateWindowGL("Whisk3D Pre-Alpha", winW, winH, windowFlags, cfg.enableAntialiasing, &glContext);
                else
                    w3dLogfW("Wayland no inicio: %s", SDL_GetError());
            }
        }
        #endif
    #endif

    if (window) { // titulo con la VERSION (fecha de build): asi una captura de pantalla dice de que version es
        char title[64];
        snprintf(title, sizeof(title), "Whisk3D Pre-Alpha %s", W3dVersion());
        SDL_SetWindowTitle(window, title);
#ifdef W3D_DREAMCAST
        SDL_StartTextInput();   // (el SDL de la PC arranca con la entrada de texto activa; el de la Dreamcast, apagada)
#endif
    }

    if (!window) {
        std::cerr << "Error: no se pudo abrir una ventana OpenGL con ninguna configuracion (ver whisk3d.log)." << std::endl;
        w3dLogfE("Error SDL_CreateWindow / SDL_GL_CreateContext: ningun intento anduvo (ultimo: %s)", SDL_GetError());
        SDL_Quit();
        return -1;
    }
    if (g_glNoMsaa) w3dLogfW("Antialiasing pedido pero el driver no lo ofrece: se abre sin multisampling");

    SDL_GetWindowSize(window, &winW, &winH);

#if defined(W3D_WEBGL) || defined(W3D_MULTIBACKEND)
    // (con varios backends: solo si el elegido es el de shaders; el despacho lo ignora en el resto)
    // backend ES2/WebGL: carga GL2.0 + compila los shaders. TIENE que ir antes de initGL
    // (initGL ya dibuja por la abstraccion); sin esto el canvas queda en negro.
    w3dEngine::GLES2Init((void* (*)(const char*))SDL_GL_GetProcAddress);
#endif
#ifdef __ANDROID__
    w3dEngine::GLES2Init(nullptr);
    // EL RENDER A ARCHIVO, FUERA DE LA VENTANA si la ventana tiene menos de 8 bits por canal: en Android 2.3 la
    // superficie de SDL 2.0.7 es RGB_565 y las franjas del render (que se leen de donde se dibujan) salian con el grano
    // del tramado de 16 bits -el MP4 de 720p del Galaxy Ace-. Van a la imagen fuera de pantalla del backend (un FBO
    // RGBA de 8 bits por canal). Con una ventana de 8 bits no hace falta (y asi la sonda de reflejo en tiempo real
    // sigue saliendo en el render: dentro de la captura no se puede capturar otra)
    {
        int r = 8, g = 8, b = 8;
        SDL_GL_GetAttribute(SDL_GL_RED_SIZE, &r); SDL_GL_GetAttribute(SDL_GL_GREEN_SIZE, &g); SDL_GL_GetAttribute(SDL_GL_BLUE_SIZE, &b);
        w3dLogf("[gl] la ventana: R%d G%d B%d", r, g, b);
        if (r < 8 || g < 8 || b < 8) { W3dOutsideScreenBegin = W3dOutsideScreenFbo; W3dOutsideScreenFinish = W3dOutsideScreenFboEnd; }
    }
#endif

    //hay que ver que onda estos dos
    W3dVsync(1);
    initGL(winW, winH);

    // Cargar skin
    #ifdef __ANDROID__
        std::string skinPath = "res/Skins/" + cfg.SkinName + "/skin.ini";
    #else
        std::string skinPath = w3dFileSystem::GetResDir() + "/Skins/" + cfg.SkinName + "/skin.ini";
    #endif
    loadColors(skinPath);

    // Constructor de Whisk3D universal (Android, PC)
    // LAS PRUEBAS arrancan con la escena de siempre (el cubo en el origen, sin piso): ver W3dCreateDefaultScene
    { extern bool g_w3dTestScene; for (int ai = 1; ai < argc; ai++) if (std::string(argv[ai]) == "--script") g_w3dTestScene = true; }
    ConstructUniversal(argc, argv);

    // "Add > import Wavefront" del menu + Load Texture + warp/swap: hooks de ESCRITORIO (Windows + Linux). En Android
    // no existen esas funciones (el bloque de PC no se compila); los hooks quedan NULL (se chequean antes de usar).
#if !defined(__ANDROID__)
    LayoutImportObj = PCImportObj;
    LayoutImportFbx = PCImportFbx;
    LayoutImportGltf = PCImportGltf;
    LayoutImportGlb = PCImportGlb;
    LayoutImportStl = PCImportStl;
    DialogLoadTexture = PCLoadTextureIn; // "Load Texture" (base Y normal map) -> browser compartido
    LayoutWarpMouse = PCWarpMouse;
    g_swapWindow = window; LayoutSwapBuffers = PCSwapBuffers; // barra de progreso (export/import)
    { extern void (*W3dProgressPoll)(); W3dProgressPoll = PCProgressPoll; }   // (Cancelar en la barra)
#endif
#ifdef __ANDROID__
    // Android: import OBJ + "Load Texture" usan el MISMO file browser interno que PC
    // (navega el almacenamiento del telefono; el permiso se pide arriba). La lectura
    // la hace la abstraccion del Core y la carga de textura w3dEngine::LoadTexture.
    LayoutImportObj = PCImportObj;
    LayoutImportFbx = PCImportFbx;
    LayoutImportGltf = PCImportGltf;
    LayoutImportGlb = PCImportGlb;
    LayoutImportStl = PCImportStl;
    DialogLoadTexture = PCLoadTextureIn;
    g_swapWindow = window; LayoutSwapBuffers = PCSwapBuffers;
    { extern void (*W3dProgressPoll)(); W3dProgressPoll = PCProgressPoll; }   // (Cancelar en la barra)
#endif
#ifdef __EMSCRIPTEN__
    // en web pisamos import/textura con el SELECTOR DEL NAVEGADOR (el browser interno solo ve el FS
    // virtual de emscripten, no los archivos del usuario). El resto de hooks (warp/swap) sirven igual.
    LayoutImportObj = WebImportObj;
    LayoutImportFbx = WebImportFbx; // web: FBX tambien por el selector nativo (antes caia al explorador interno)
    DialogLoadTexture = WebLoadTextureIn;
#endif

    // LOS MANDOS QUE YA ESTAN ENCHUFADOS. Los que se enchufen DESPUES entran
    // solos por los eventos SDL_CONTROLLERDEVICEADDED/REMOVED (hotplug), que
    // atiende InputUserSDL3 (main/app/controls.cpp): ya no hay que cerrar el
    // editor, enchufar el control y volver a abrir.
    { extern void W3dControlsOpenThatHas(); W3dControlsOpenThatHas(); }

    running = true;
    // arbol en coordenadas ARRIBA-izquierda (unificado con Symbian);
    // el flip a GL pasa solo en glViewport/glScissor con este alto
    W3dScreenHeight = winH;
    MenuScreenW = winW;
    MenuScreenH = winH;
    rootViewport->Resize(winW, winH);
    { extern void LayoutScreenSize(int, int); LayoutScreenSize(winW, winH); }   // (la orientacion con la que arranca)

    // los ganchos del cartel de lo no guardado (salir, abrir otro proyecto, guardar)
    W3dChangesExitHook = ChangesExit;
    g_windowRearrangeHook = WindowRearrange;   // (los teclados de Whisk3D: el del IDE, ver NumPad.cpp)
    W3dChangesOpenHook = ChangesOpen;
    W3dChangesSaveHook = ChangesSave;
    W3dChangesSnapshot();   // el proyecto de arranque esta "guardado" (no hay nada que perder todavia)

    // MODO TEST: "whisk3d --script <ruta>" corre un script de comandos (sin GUI) y
    // sale (0 = todo OK, 1 = fallo). Ver main/test/W3dScript.cpp.
    for (int ai = 1; ai < argc; ai++) {
        if (std::string(argv[ai]) == "--script" && ai + 1 < argc) {
            g_w3dChangesNoDialog = true;   // (el harness no muestra el cartel: el comando 'cambios' lo consulta)
            { extern bool g_transformRedoAuto; g_transformRedoAuto = false; }   // (el panel de la ultima operacion: solo si la prueba lo pide)
            #ifndef _WIN32
            { const char* tmp = "/tmp/w3d_prueba_usuario"; mkdir(tmp, 0755); w3dFileSystem::SetUserDataDir(tmp); }   // (config.start y demas del usuario: no se tocan)
            #endif
            { extern void W3dShortcutsPathSet(const std::string&); W3dShortcutsPathSet("/tmp/w3d_prueba_atajos.ini"); }   // (los atajos de fabrica; el shortcuts.start del usuario no se toca)
            { extern void W3dLayoutsPathSet(const std::string&); remove("/tmp/w3d_prueba_layouts.txt");   // (los layouts guardados
              W3dLayoutsPathSet("/tmp/w3d_prueba_layouts.txt"); }                                        //  del usuario no se tocan)
            { extern void W3dRecentPathSet(const std::string&); remove("/tmp/w3d_prueba_recientes.txt");   // (los archivos recientes
              W3dRecentPathSet("/tmp/w3d_prueba_recientes.txt"); }                                         //  del usuario no se tocan; arranca vacia)
            { extern void W3dScriptFixedSeed(bool); W3dScriptFixedSeed(true); }   // (el mismo script juega siempre igual: las capturas se comparan)
            bool ok = W3dRunScript(argv[ai + 1]);
            SDL_Quit();
            return ok ? 0 : 1;
        }
    }

    // "whisk3d --open <archivo>" IMPORTA un modelo al arrancar (util para probar/depurar render + skinning sin el
    // file browser). Rutea por extension: .fbx -> ImportFBX; el resto -> ImportOBJ.
    for (int ai = 1; ai < argc; ai++) {
        if (std::string(argv[ai]) == "--open" && ai + 1 < argc) {
            std::string path = argv[ai + 1];
            // .w3d = proyecto entero (zip+json): va por el abridor de proyectos, no por el
            // importador de modelos (que solo entiende .obj/.fbx). Asi se puede arrancar el
            // editor con una escena guardada (util para depurar animaciones desde consola).
            size_t point = path.find_last_of('.');
            std::string ext = (point != std::string::npos) ? path.substr(point) : "";
            for (size_t k = 0; k < ext.size(); k++) ext[k] = (char)tolower(ext[k]);
            if (ext == ".w3d") {
                extern void OpenProjectNow(const std::string&);
                OpenProjectNow(path);
            } else {
                ImportModelByExtension(path);
            }
            g_redraw = true;
        }
    }
    // "whisk3d --frame N" fija CurrentFrame al arrancar (para ver/depurar una pose ANIMADA en un screenshot, no solo el bind)
    for (int ai = 1; ai < argc; ai++) {
        if (std::string(argv[ai]) == "--frame" && ai + 1 < argc) {
            extern int CurrentFrame; CurrentFrame = atoi(argv[ai + 1]); g_redraw = true;
        }
    }
    // "whisk3d --framesel" encuadra el objeto seleccionado (Frame Selected) al arrancar (depuracion: ver el modelo importado)
    for (int ai = 1; ai < argc; ai++) {
        if (std::string(argv[ai]) == "--framesel") {
            extern Viewport3D* Viewport3DActive;
            if (Viewport3DActive) Viewport3DActive->FocusObject();
            g_redraw = true;
        }
    }
    // "whisk3d --stats" prende el overlay Statistics (con el profiler de ms); "--play" arranca la animacion en PLAY
    for (int ai = 1; ai < argc; ai++) {
        if (std::string(argv[ai]) == "--stats") { // los 6 flags de stats son AHORA miembros per-viewport: prende los del 3D activo
            extern Viewport3D* Viewport3DActive;
            if (Viewport3DActive) {
                Viewport3DActive->OverlayStatVertices = Viewport3DActive->OverlayStatFaces =
                Viewport3DActive->OverlayStatModgen  = Viewport3DActive->OverlayStatTimes =
                Viewport3DActive->OverlayFps         = Viewport3DActive->OverlayStatGL = true;
                Viewport3DActive->showOverlays = true;   // el overlay de stats esta gateado por "Show Overlays"
            }
            g_redraw = true;
        }
        if (std::string(argv[ai]) == "--play")  { extern bool PlayAnimation;     PlayAnimation = true;     g_redraw = true; }
        // "--render-image" / "--render-animation": renderiza al abrir con la tarjeta Render del proyecto (y sigue abierto)
        if (std::string(argv[ai]) == "--render-image")     { g_renderAtStart = 1; g_redraw = true; }
        if (std::string(argv[ai]) == "--render-animation") { g_renderAtStart = 2; g_redraw = true; }
        // "whisk3d proyecto.w3d --juego": abre DIRECTO jugando (el selector del
        // timeline en "Juego" + play): para probar un juego sin tocar nada
        if (std::string(argv[ai]) == "--juego") {
            extern int ActiveAnimKind; extern bool AnimIsGame; extern bool PlayAnimation;
            // misma puerta que el boton Play: nada de arrancar con la cola de
            // texturas a medias (el primer segundo de juego salia gris)
            extern bool GameWaitTextures(); GameWaitTextures();
            ActiveAnimKind = 2; AnimIsGame = true; StartFrame = 1;
            PlayAnimation = true; g_redraw = true;
        }
    }

    // LA PANTALLA DE INICIO (ui/ViewPorts/PopUp/HomePopup.h): arriba, con el engranaje girando, Nuevo archivo, los
    // archivos recientes y Abrir... Solo si el editor arranco SIN un archivo (ni un .w3d por doble click o --open, ni
    // un modelo, ni el modo juego): el que abrio algo ya sabe que quiere hacer
    {
        bool withFile = (argc > 1 && argv[1][0] != '-');
        for (int ai = 1; ai < argc; ai++) {
            const std::string a = argv[ai];
            if (a == "--open" || a == "--juego") withFile = true;
        }
        extern std::string w3dPath;
        extern bool g_gameMode;
        if (!withFile && w3dPath.empty() && !g_gameMode) { extern void HomePopupOpen(); HomePopupOpen(); }
    }

#ifdef __EMSCRIPTEN__
    // WebGL: el browser es de 1 solo hilo, no podemos bloquear con un while(). Le pasamos el
    // frame y el browser lo llama (~60 veces/seg via requestAnimationFrame). El 3er arg = 1
    // "simula" un loop infinito (no se ejecuta lo de abajo). El cleanup queda solo para escritorio.
    { extern void W3dWebVideoTest(); W3dWebVideoTest(); }   // (el navegador codifica H.264? Render a MP4: io/W3dWebVideo.h)
    emscripten_set_main_loop(MainLoopFrame, 0, 1);
#else
    // Loop de escritorio PACEADO (reemplaza el busy-spin viejo): MainLoopFrame ya bloquea en REPOSO (0% CPU). Cuando
    // SI dibuja (animando/interactuando), dormimos hasta el proximo frame -> fps parejos y CPU baja. Acumulador
    // (nextFrame) para no driftear; si nos atrasamos mucho no acumulamos deuda.
    // TOPE DE FRAMERATE por proyecto: g_fpsCap (import_w3d) -> default 60, el .w3d puede subirlo hasta 120 en PC.
    // Se recomputa por iteracion para reaccionar a un .w3d abierto en caliente. VSYNC adaptativo: con cap>60 hay
    // que apagar el vsync (SwapInterval 0) para pasar de la tasa del monitor; con cap<=60 se deja (sin tearing).
    extern int g_fpsCap;
    int capVsync = -1;   // ultimo estado de vsync aplicado (para no re-llamar SwapInterval cada frame)
    double nextFrame = W3dNowMs();
    while (running) {
        int cap = (g_fpsCap > 0) ? g_fpsCap : 60;
        const double frameMs = 1000.0 / (double)cap;
        int wantsVsync = (cap > 60) ? 0 : 1;
        if (wantsVsync != capVsync) { W3dVsync(wantsVsync); capVsync = wantsVsync; }
        MainLoopFrame();
        if (g_didRender) {
            nextFrame += frameMs;
            double now = W3dNowMs();
            if (nextFrame < now - frameMs) nextFrame = now; // atraso grande (frame lento): resincronizar
            double wait = nextFrame - now;
            if (wait > 1.0) SDL_Delay((Uint32)wait); // duerme el resto del frame (yield de CPU)
        } else {
            nextFrame = W3dNowMs(); // estuvimos en reposo (MainLoopFrame bloqueo) -> resincronizar al reanudar
        }
    }
#endif

    W3dDockProgressEnd(); // ultimo seguro: cerrar el editor no puede dejar la barra prendida en el dock
    if (controller) SDL_GameControllerClose(controller);
#ifdef W3D_3DS
    // citro3d se cierra ANTES que la ventana: SDL_Quit apaga las pantallas (gfxExit) y citro3d tiene enganchados al
    // sistema su hook de APT y su interrupcion del vblank. Cerrar la app con eso puesto trababa la consola
    w3dEngine::C3DEnd();
#endif
#ifdef W3D_VULKAN
    VkGLFinish();
#endif
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    w3dLogf("[editor] cerrado");   // (la prueba de cierre de las consolas espera esta linea: no se trabo en el camino)
    return 0;
}