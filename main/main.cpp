#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "test/W3dScript.h" // modo test: whisk3d --script <ruta>

#ifdef __EMSCRIPTEN__       // WebGL: el browser es 1 hilo -> el loop es emscripten_set_main_loop
#include <emscripten.h>
#include <emscripten/html5.h> // tamano del canvas (resize) + cancelar el loop al salir
#endif
#include "ViewPorts/LayoutInput.h" // ruteo compartido + overlay del menu
#if !defined(W3D_SYMBIAN) // Desktop (Win/Linux) + Web + ANDROID: comparten el file browser interno + la
                        // carga por w3dEngine (import/export, browser, texturas, warp, swap). Symbian tiene lo suyo.
    #define NOMINMAX

#include "lectura-escritura.h"      // abrir(): dialogo + ImportOBJ
#include "ViewPorts/PopUp/FileBrowser.h" // explorador de archivos COMPARTIDO
#include "ViewPorts/PopUp/ProgressPopup.h" // barra de progreso + hook LayoutSwapBuffers
#include "importers/import_obj.h"        // ImportOBJ (el importador real)
#include "importers/import_fbx.h"        // ImportFBX
#include "w3dVersion.h"                   // W3dVersion() para el titulo de ventana

// accion del File browser al elegir un modelo: por EXTENSION (.fbx -> FBX; resto -> OBJ)
static void ImportObjDesdeBrowser(const std::string& path) {
    size_t d = path.find_last_of('.');
    std::string ext = (d == std::string::npos) ? std::string() : path.substr(d);
    for (size_t i = 0; i < ext.size(); i++) if (ext[i] >= 'A' && ext[i] <= 'Z') ext[i] += 32;
    if (ext == ".fbx") ImportFBX(path);
    else               ImportOBJ(path, false);
}

// el callback del menu "Add > Import OBJ": abre el explorador filtrado a .obj
static void PCImportObj() {
    AbrirFileBrowser("Import OBJ", "Import OBJ", ".obj", ImportObjDesdeBrowser);
}
// "Add > Import FBX": mismo explorador, filtrado a .fbx (ImportObjDesdeBrowser rutea por extension -> ImportFBX)
static void PCImportFbx() {
    AbrirFileBrowser("Import FBX", "Import FBX", ".fbx", ImportObjDesdeBrowser);
}

// hook de swap para la barra de progreso (se redibuja DENTRO del export/import bloqueante)
static SDL_Window* g_swapWindow = NULL;
static void PCSwapBuffers() { if (g_swapWindow) SDL_GL_SwapWindow(g_swapWindow); }

#include "ViewPorts/Properties.h" // DialogoCargarTextura
#include "objects/Textures.h"
#include "objects/Materials.h"    // Material (asignar la textura cargada)

// warp del mouse real (los drags que no deben salirse de un rect)
static void PCWarpMouse(int x, int y) {
    SDL_WarpMouseInWindow(window, x, y);
    lastMouseX = x;
    lastMouseY = y;
}

// "Load Texture": el browser COMPARTIDO elige la imagen (png/jpg/bmp...) y la
// carga al material que la pidio (async: el browser es modal)
extern void RebindMaterialMeshPart(); // Properties.cpp
static Material* gTexMat = NULL;
extern bool gCargarTexturaComoNormal; // Properties.cpp: el "Load Texture" del normal map lo prende (compartido 4 OS)
static void TexturaElegida(const std::string& path) {
    if (!gTexMat) return;
    GLuint id = 0;
    if (LoadTexture(path.c_str(), id)) {
        Texture* t = new Texture(path);
        t->iID = id;
        Textures.push_back(t);
        if (gCargarTexturaComoNormal) { gTexMat->normalTexture = t; }
        else { gTexMat->texture = t; gTexMat->textureOn = true; }
        RebindMaterialMeshPart();
    }
    gTexMat = NULL; gCargarTexturaComoNormal = false;
}
// "Load Texture" (base Y normal map): el MISMO browser. Quien lo llama ya dejo gCargarTexturaComoNormal en el
// valor correcto (false=textura, true=normal); el callback de arriba decide el destino.
static void PCCargarTexturaEn(Material* mat) {
    gTexMat = mat;
    AbrirFileBrowser(gCargarTexturaComoNormal ? "Cargar normal map" : "Cargar textura",
                    gCargarTexturaComoNormal ? "Abrir normal map" : "Abrir textura",
                    ".png .jpg .jpeg .bmp .tga", TexturaElegida);
}

#ifdef __EMSCRIPTEN__
// ============================================================================
//  WEB: cargar texturas / OBJ con el SELECTOR DE ARCHIVOS DEL NAVEGADOR
// ----------------------------------------------------------------------------
//  El file browser interno navega el FS virtual de emscripten (no ve los archivos
//  reales del usuario). En web abrimos un <input type=file> nativo: el navegador
//  deja elegir del disco, leemos el archivo, lo escribimos en /uploads del FS de
//  emscripten y llamamos a la MISMA logica de carga (TexturaElegida / ImportOBJ).
//  Para el OBJ el <input multiple> deja elegir el .obj + su .mtl + las texturas
//  juntos: quedan en la misma carpeta (/uploads) y el importador los resuelve.
// ============================================================================
extern "C" {
// las llama el JS del picker cuando el archivo ya esta escrito en el FS de emscripten
EMSCRIPTEN_KEEPALIVE void WebTexturaCargada(const char* path) { TexturaElegida(std::string(path)); }
EMSCRIPTEN_KEEPALIVE void WebObjCargado(const char* path)     { ImportOBJ(std::string(path), false); }
EMSCRIPTEN_KEEPALIVE void WebFbxCargado(const char* path)     { ImportFBX(std::string(path)); }
}

// overlay con un BOTON real: el <input type=file>.click() se dispara DENTRO del tap del boton (gesto DOM
// genuino). Necesario para iOS Safari: ahi input.click() llamado desde el RAF (fuera de un gesto de usuario)
// NO abre el selector -> "al hacerle click no ocurre nada". El input va en el DOM (otro requisito de iOS).
// tipo: 0 = textura (1 imagen), 1 = OBJ (multi: obj+mtl+texturas).
EM_JS(void, WebAbrirPicker, (int tipo), {
    var prev = document.getElementById('w3d-pick'); if (prev && prev.parentNode) prev.parentNode.removeChild(prev);
    var ov = document.createElement('div');
    ov.id = 'w3d-pick';
    ov.style.cssText = 'position:fixed;left:0;top:0;right:0;bottom:0;z-index:99999;'
        + 'background:rgba(0,0,0,0.5);display:flex;align-items:flex-start;justify-content:center;';
    var card = document.createElement('div');
    card.style.cssText = 'margin-top:8vh;width:92%;max-width:440px;background:#2b2b2b;border:1px solid #555;'
        + 'border-radius:10px;padding:16px;box-shadow:0 6px 30px rgba(0,0,0,0.6);font-family:sans-serif;color:#ddd;';
    var lab = document.createElement('div');
    lab.textContent = (tipo == 2) ? 'Importar FBX (.fbx)' : tipo ? 'Importar OBJ (.obj + .mtl + texturas)' : 'Cargar textura (imagen)';
    lab.style.cssText = 'font-size:15px;margin-bottom:12px;color:#9ac9ff;';
    var input = document.createElement('input');
    input.type = 'file';
    if (tipo == 2)   { input.accept = '.fbx'; }                                                  // FBX: un solo archivo binario
    else if (tipo)   { input.multiple = true; input.accept = '.obj,.mtl,image/*,.png,.jpg,.jpeg,.bmp,.tga'; }
    else             { input.accept = 'image/png,image/jpeg,image/bmp,.png,.jpg,.jpeg,.bmp,.tga'; }
    input.style.cssText = 'position:absolute;left:-9999px;width:1px;height:1px;opacity:0;'; // en el DOM pero oculto
    input.onchange = function(e) {
        var files = Array.prototype.slice.call(e.target.files);
        if (!files.length) { cerrar(); return; }
        try { FS.mkdir('/uploads'); } catch (_e) {}
        if (tipo == 2) {                       // FBX: un solo archivo binario -> WebFbxCargado
            var f = files[0], r = new FileReader();
            r.onload = function() {
                var p = '/uploads/' + f.name;
                FS.writeFile(p, new Uint8Array(r.result));
                ccall('WebFbxCargado', null, ['string'], [p]);
            };
            r.readAsArrayBuffer(f); cerrar();
        } else if (!tipo) {
            var f = files[0], r = new FileReader();
            r.onload = function() {
                var p = '/uploads/' + f.name;
                FS.writeFile(p, new Uint8Array(r.result));
                ccall('WebTexturaCargada', null, ['string'], [p]);
            };
            r.readAsArrayBuffer(f); cerrar();
        } else {
            var objPath = null, pending = files.length;
            files.forEach(function(f) {
                var r = new FileReader();
                r.onload = function() {
                    var p = '/uploads/' + f.name;
                    FS.writeFile(p, new Uint8Array(r.result));
                    if (f.name.toLowerCase().endsWith('.obj')) objPath = p;
                    if (--pending === 0 && objPath) ccall('WebObjCargado', null, ['string'], [objPath]);
                };
                r.readAsArrayBuffer(f);
            });
            cerrar();
        }
    };
    var btnPick = document.createElement('button');
    btnPick.textContent = 'Elegir archivo';
    btnPick.style.cssText = 'width:100%;font-size:17px;padding:14px;border:none;border-radius:6px;'
        + 'background:#3a8f68;color:#fff;font-weight:bold;';
    var btnC = document.createElement('button');
    btnC.textContent = 'Cancelar';
    btnC.style.cssText = 'width:100%;margin-top:10px;font-size:15px;padding:11px;border:none;border-radius:6px;background:#444;color:#ddd;';
    function cerrar(){ if (ov.parentNode) ov.parentNode.removeChild(ov); }
    // el click del boton ES el gesto de usuario -> input.click() adentro SI abre el selector en iOS
    btnPick.addEventListener('click', function(){ input.click(); });
    btnC.addEventListener('click', function(){ cerrar(); });
    ov.addEventListener('mousedown', function(e){ if (e.target === ov) cerrar(); });
    ov.addEventListener('touchend', function(e){ if (e.target === ov) cerrar(); });
    ['mousedown','mouseup','touchstart','touchmove','touchend','pointerdown','pointerup'].forEach(function(ev){
        card.addEventListener(ev, function(e){ e.stopPropagation(); });
    });
    card.appendChild(lab); card.appendChild(input); card.appendChild(btnPick); card.appendChild(btnC);
    ov.appendChild(card); document.body.appendChild(ov);
});
static void WebAbrirPickerTextura(){ WebAbrirPicker(0); }
static void WebAbrirPickerObj(){ WebAbrirPicker(1); }
static void WebAbrirPickerFbx(){ WebAbrirPicker(2); }

static void WebCargarTexturaEn(Material* mat) { gTexMat = mat; WebAbrirPickerTextura(); }
static void WebImportObj() { WebAbrirPickerObj(); }
static void WebImportFbx() { WebAbrirPickerFbx(); } // web: FBX por el selector NATIVO del navegador (como OBJ/texturas)

// descarga un archivo del FS de emscripten al DISCO del usuario (export OBJ/mtl y renders PNG).
// La llaman Properties.cpp (export OBJ) y ViewPort3D.cpp (render) via forward-declaration.
EM_JS(void, WebDescargarArchivo, (const char* pathPtr, const char* namePtr), {
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
#include "WhiskUI/PopupMenu.h" // MenuPantallaW/H (desplegables)
#include "WhiskUI/glesdraw.h"  // W3dPantallaAlto (flip de Y)
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
#include "WhiskUI/colores.h"
#include "ui/W3dColors.h" // W3dColores(Ubyte) + carga de colores del editor + Sincronizar
#include "objects/RenderColors.h" // paleta de render del core (Fase D: el sync de loadColors la llena)
#include "objects/Textures.h"
#include "objects/Materials.h"
#include "animation/Animation.h"
#include "animation/VertexAnimation.h"
#include "ViewPorts/ViewPorts.h"
#include "ViewPorts/PopUp/PopUpBase.h"
#include "ViewPorts/PopUp/ColorPicker.h"
#include "controles.h"
#include "constructor.h"

// Reloj de ms para el core (Animation declara w3dGetTicks en Animation.h y NO incluye
// SDL). Aca el EDITOR/plataforma PC lo implementa con SDL. Symbian pondria el suyo.
unsigned int w3dGetTicks() { return SDL_GetTicks(); }

// Función simple para leer el ini
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

        Sint64 size = SDL_RWsize(rw);
        std::vector<char> buffer(size + 1, 0);
        SDL_RWread(rw, buffer.data(), 1, size);
        SDL_RWclose(rw);

        std::istringstream iss(buffer.data());
    #else
        std::ifstream file(configPath);
        if (!file.is_open()) {
            std::cerr << "No se pudo abrir " << configPath << ", usando valores por defecto.\n";
            return cfg;
        }

        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());

        std::istringstream iss(content);
    #endif

    std::string line;
    while (std::getline(iss, line)) {
        std::istringstream lineStream(line);
        std::string key, eq, value;
        if (lineStream >> key >> eq >> value && eq == "=") {
            if (key == "fullscreen") cfg.fullscreen = (value == "true");
            else if (key == "enableAntialiasing") cfg.enableAntialiasing = (value == "true");
            else if (key == "width") cfg.width = std::stoi(value);
            else if (key == "height") cfg.height = std::stoi(value);
            else if (key == "displayIndex") cfg.displayIndex = std::stoi(value);
            else if (key == "scale") cfg.scale = std::stoi(value);
            else if (key == "nuevoUsuario") cfg.nuevoUsuario = (value == "true" || value == "1"); // barra de herramientas
            else if (key == "SkinName") cfg.SkinName = value;
            else if (key == "graphicsAPI") cfg.graphicsAPI = value;
        }
    }

    return cfg;
}

bool loadColors(const std::string& filename) {
    static std::unordered_map<std::string, ColorID> colorMap = {
        {"background", ColorID::background},
        {"blanco", ColorID::blanco},
        {"accent", ColorID::accent},
        {"accentDark", ColorID::accentDark},
        {"negro", ColorID::negro},
        {"gris", ColorID::gris},
        {"headerColor", ColorID::headerColor},
        {"negroTransparente", ColorID::negroTransparente},
        {"grisUI", ColorID::grisUI}
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

        Sint64 size = SDL_RWsize(rw);
        std::vector<char> buffer(size + 1, 0);
        SDL_RWread(rw, buffer.data(), 1, size);
        SDL_RWclose(rw);

        fileStream.str(buffer.data());
    #else
        std::ifstream file(skinPath);
        if (!file.is_open()) {
            std::cerr << "No se pudo abrir " << skinPath << ", usando colores por defecto.\n";
            return false;
        }

        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        fileStream.str(content);
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
            ListaColores[idx][0] = r;
            ListaColores[idx][1] = g;
            ListaColores[idx][2] = b;
            ListaColores[idx][3] = a;

            ListaColoresUbyte[idx][0] = (GLubyte)(r*255);
            ListaColoresUbyte[idx][1] = (GLubyte)(g*255);
            ListaColoresUbyte[idx][2] = (GLubyte)(b*255);
            ListaColoresUbyte[idx][3] = (GLubyte)(a*255);

            #ifdef __ANDROID__
                __android_log_print(ANDROID_LOG_VERBOSE, "SDL_MAIN",
                    "Color %s cargado: R=%.3f G=%.3f B=%.3f A=%.3f",
                    name.c_str(), r, g, b, a);
            #else
                //std::cout << "Color " << name << " cargado: R=" << r << " G=" << g << " B=" << b << " A=" << a << "\n";
            #endif
        } else {
            int eidx = editorColorIdPorNombre(name.c_str());
            if (eidx >= 0) {
                W3dColores[eidx][0] = r;
                W3dColores[eidx][1] = g;
                W3dColores[eidx][2] = b;
                W3dColores[eidx][3] = a;
                W3dColoresUbyte[eidx][0] = (GLubyte)(r*255);
                W3dColoresUbyte[eidx][1] = (GLubyte)(g*255);
                W3dColoresUbyte[eidx][2] = (GLubyte)(b*255);
                W3dColoresUbyte[eidx][3] = (GLubyte)(a*255);
            } else {
                std::cerr << "Color desconocido en colors.skin: " << name << "\n";
            }
        }
    }

    // Fase D: copiar la paleta de la UI a la de RENDER del core. Ahora COMPARTIDO
    // (colores.cpp), asi Symbian -que carga por loadColorsW3d- tambien lo hace.
    SincronizarRenderColores();

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
static void MainLoopFrame() {
    Contadores();

    UpdateAnimations();
    UpdateAnimatedMaterials();

#ifdef __EMSCRIPTEN__
    // el canvas puede cambiar de tamano (ventana del browser). SDL2/Emscripten no siempre emite el
    // WINDOWEVENT_RESIZED, asi que chequeamos el tamano del canvas cada frame y resincronizamos.
    {
        double cw = 0, ch = 0;
        emscripten_get_element_css_size("#canvas", &cw, &ch);
        int nw = (int)cw, nh = (int)ch;
        if (nw > 0 && nh > 0 && (nw != winW || nh != winH)) {
            emscripten_set_canvas_element_size("#canvas", nw, nh); // resolucion = display (nitido)
            SDL_SetWindowSize(window, nw, nh);
            winW = nw; winH = nh;
            W3dPantallaAlto = winH; MenuPantallaW = winW; MenuPantallaH = winH;
            rootViewport->Resize(winW, winH);
            g_redraw = true;
        }
    }
#endif

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        g_redraw = true; // cualquier evento (tecla/mouse/resize) -> hay que redibujar
        if (e.type == SDL_QUIT) running = false;
        if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESIZED) {
            winW = e.window.data1;
            winH = e.window.data2;
            W3dPantallaAlto = winH;
            MenuPantallaW = winW;
            MenuPantallaH = winH;
            rootViewport->Resize(winW, winH);
        } else {
            InputUsuarioSDL3(e);
        }
    }

#ifdef __EMSCRIPTEN__
    if (!running) { emscripten_cancel_main_loop(); return; } // SDL_QUIT -> salir del loop del browser
#endif

    // Animación
    Uint32 now = SDL_GetTicks();
    // el avance de frames va al ritmo de AnimFPS (default 30), independiente de los fps de la UI (que puede ir a 60).
    // Asi la animacion NO va "muy rapido": un frame de animacion dura 1000/AnimFPS ms aunque se dibuje 2 veces.
    unsigned int animMs = (AnimFPS > 0) ? (unsigned int)(1000 / AnimFPS) : 33;
    if (now - lastAnimTime >= animMs) {
        lastAnimTime = now;
        // Timeline: si esta en PLAY, avanzar CurrentFrame (loop Start..End) y forzar redibujo
        if (PlayAnimation) { AnimTick(); g_redraw = true; }
        // Actualizar frame
        ReloadAnimation();
    }

    // notificaciones (toasts): corre el timer (las de exito se cierran solas) y
    // mantiene vivo el render mientras haya alguna de exito activa
    static Uint32 lastNotif = now;
    NotificacionesTick((float)(now - lastNotif) / 1000.0f);
    lastNotif = now;

    // CARGA DIFERIDA de texturas del import: 1 por frame (decode+upload). Prende g_redraw al cargar una -> el
    // modelo aparece enseguida y las texturas entran solas. No-op si no hay pendientes.
    { extern void CargarTexturasPendientes(); CargarTexturasPendientes(); }

    // Render EVENT-DRIVEN: solo si algo cambio (g_redraw) o hay una animacion EN
    // PLAY (vertex-anim o materiales animados activos). Sino no se dibuja nada ->
    // CPU casi 0 en reposo (como Blender), en vez de renderizar 60 veces/seg al pedo.
    bool animando = HayAnimacionActiva() || !VertexAnimationActives.empty() || PlayAnimation;
#ifdef __EMSCRIPTEN__
    // WebGL: renderizar SIEMPRE (no event-driven). Motivos: 1) el canvas WebGL usa
    // preserveDrawingBuffer=false -> en un frame sin dibujar el browser BORRA el canvas a negro
    // (parpadeos/negro al quedar quieto); 2) el color-ID pick (hover/loop cut) limpia el framebuffer
    // visible y hay que taparlo cada frame. El RAF ya limita a la tasa del monitor y se PAUSA en
    // tabs ocultas, asi que no quema GPU de fondo. (En escritorio se mantiene el event-driven.)
    bool doRender = true; (void)animando;
#else
    bool doRender = (g_redraw || animando) && (now - lastRenderTime >= 16); // ~60hz
#endif
    if (doRender) {
        lastRenderTime = now;
        if (rootViewport){
            rootViewport->Render();
        }
        // popups y desplegables (los dibuja el modulo compartido)
        LayoutRenderMenu(winW, winH);
        NotificacionesRender(winW, winH); // toasts ENCIMA de todo
        LayoutTickFPS(now); // overlay de fps (reloj de PC: SDL_GetTicks)
        SDL_GL_SwapWindow(window);
        g_redraw = false;

#ifdef __EMSCRIPTEN__
        // apenas dibujamos el PRIMER frame, ocultamos la pantalla de carga del shell. Es lo mas
        // robusto: no depende de setStatus('')/onRuntimeInitialized (que con emscripten_set_main_loop
        // pueden no dispararse porque main() no retorna). Se hace una sola vez.
        static bool loaderOculto = false;
        if (!loaderOculto) {
            loaderOculto = true;
            EM_ASM({ var l = document.getElementById('loader'); if (l) l.classList.add('hidden'); });
        }
#endif
    }

#ifndef __EMSCRIPTEN__
    SDL_Delay(8); // liberar CPU (en web el timing lo maneja el browser via requestAnimationFrame)
#endif
}

int main(int argc, char* argv[]) {
#ifdef __EMSCRIPTEN__
    SDL_SetMainReady(); // web: no arrancamos por el main de SDL
    // touch -> mouse SIEMPRE sintetizado por SDL (which = SDL_TOUCH_MOUSEID): el ruteo tactil de
    // controles.cpp distingue por ese which los eventos del dedo de los mouse FANTASMA del browser.
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "1");
    // en web solo VIDEO: el subsistema de gamepad puede no estar y haria fallar todo el SDL_Init
    Uint32 initFlags = SDL_INIT_VIDEO;
#else
    Uint32 initFlags = SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER;
#endif
#ifdef __ANDROID__
    // Al minimizar: SDL BLOQUEA el loop de main (NO mata el hilo nativo) y conserva el contexto GL. Sin esto, al volver
    // a abrir se relanzaba main() con los globals C++ estaticos vivos -> ConstructUniversal duplicaba la escena (otro
    // cubo/camara/luz) y las texturas quedaban negras (contexto GL nuevo). Junto con launchMode=singleTask del manifest.
    SDL_SetHint(SDL_HINT_ANDROID_BLOCK_ON_PAUSE, "1");
#endif
    if (SDL_Init(initFlags) != 0) {
        std::cerr << "Error SDL_Init: " << SDL_GetError() << std::endl;
        return -1;
    }

    w3dFileSystem::Init();

    // Carpeta ESCRIBIBLE por-app para config/bookmarks. SDL_GetPrefPath la crea si no
    // existe (Win: AppData, Linux: ~/.local/share, Android: internal storage). En Android
    // el GetResDir es el APK (solo lectura) -> sin esto los bookmarks no se guardaban.
    {
        char* pref = SDL_GetPrefPath("Whisk3D", "Whisk3D");
        if (pref) { w3dFileSystem::SetUserDataDir(pref); SDL_free(pref); }
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
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
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

    // OJO: asignar el cfg GLOBAL (no declarar uno local que lo tape) -> asi ConstructUniversal
    // y todo lo demas ven los valores cargados (ej. scale para SetGlobalScale).
    #ifdef __ANDROID__
        cfg = loadConfig("res/config.ini");
        // Android es de alta densidad y se usa con el dedo -> forzamos escala 3
        // (el config compartido viene en 2 para PC). Asi la UI queda comoda al tacto.
        cfg.scale = 3;
    #else
        cfg = loadConfig(w3dFileSystem::GetResDir() + "/config.ini");
    #endif
    winW = cfg.width;  winH = cfg.height;  // aplicar el tamano del config a la ventana
                                        // (antes quedaba el default 640x480: el config no se usaba)

    //TODO: Esto muy probablemente de problemas en gama baja? no se si deba configurable
    if (cfg.enableAntialiasing) {
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
    }

    #ifdef __ANDROID__
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

        window = SDL_CreateWindow("Whisk3D Pre-Alpha",
                                SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                winW, winH,
                                SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    #else
        // fullscreen -> maximizada; sino respeta width/height del config (util para
        // probar tamanos chicos, ej. 240x320 estilo N95 con scale=1)
        Uint32 windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
        if (cfg.fullscreen) windowFlags |= SDL_WINDOW_MAXIMIZED;
        window = SDL_CreateWindow("Whisk3D Pre-Alpha", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, winW, winH, windowFlags);
    #endif

    if (window) { // titulo con la VERSION (fecha de build): asi una captura de pantalla dice de que version es
        char titulo[64];
        snprintf(titulo, sizeof(titulo), "Whisk3D Pre-Alpha %s", W3dVersion());
        SDL_SetWindowTitle(window, titulo);
    }

    if (!window) {
        std::cerr << "Error SDL_CreateWindow: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return -1;
    }

    SDL_GetWindowSize(window, &winW, &winH);

    glContext = SDL_GL_CreateContext(window);
    if (!glContext) {
        std::cerr << "Error SDL_GL_CreateContext: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

#ifdef W3D_WEBGL
    // backend ES2/WebGL: carga GL2.0 + compila los shaders. TIENE que ir antes de initGL
    // (initGL ya dibuja por la abstraccion); sin esto el canvas queda en negro.
    w3dEngine::GLES2Init((void* (*)(const char*))SDL_GL_GetProcAddress);
#endif
#ifdef __ANDROID__
    w3dEngine::GLES2Init(nullptr);
#endif

    //hay que ver que onda estos dos
    SDL_GL_SetSwapInterval(1);
    initGL(winW, winH);

    // Cargar skin
    #ifdef __ANDROID__
        std::string skinPath = "res/Skins/" + cfg.SkinName + "/skin.ini";
    #else
        std::string skinPath = w3dFileSystem::GetResDir() + "/Skins/" + cfg.SkinName + "/skin.ini";
    #endif
    loadColors(skinPath);

    // Constructor de Whisk3D universal (Android, PC)
    ConstructUniversal(argc, argv);

    // "Add > import Wavefront" del menu + Load Texture + warp/swap: hooks de ESCRITORIO (Windows + Linux). En Android
    // no existen esas funciones (el bloque de PC no se compila); los hooks quedan NULL (se chequean antes de usar).
#if !defined(__ANDROID__)
    LayoutImportObj = PCImportObj;
    LayoutImportFbx = PCImportFbx;
    DialogoCargarTextura = PCCargarTexturaEn; // "Load Texture" (base Y normal map) -> browser compartido
    LayoutWarpMouse = PCWarpMouse;
    g_swapWindow = window; LayoutSwapBuffers = PCSwapBuffers; // barra de progreso (export/import)
#endif
#ifdef __ANDROID__
    // Android: import OBJ + "Load Texture" usan el MISMO file browser interno que PC
    // (navega el almacenamiento del telefono; el permiso se pide arriba). La lectura
    // la hace la abstraccion del Core y la carga de textura w3dEngine::LoadTexture.
    LayoutImportObj = PCImportObj;
    LayoutImportFbx = PCImportFbx;
    DialogoCargarTextura = PCCargarTexturaEn;
    g_swapWindow = window; LayoutSwapBuffers = PCSwapBuffers;
#endif
#ifdef __EMSCRIPTEN__
    // en web pisamos import/textura con el SELECTOR DEL NAVEGADOR (el browser interno solo ve el FS
    // virtual de emscripten, no los archivos del usuario). El resto de hooks (warp/swap) sirven igual.
    LayoutImportObj = WebImportObj;
    LayoutImportFbx = WebImportFbx; // web: FBX tambien por el selector nativo (antes caia al explorador interno)
    DialogoCargarTextura = WebCargarTexturaEn;
#endif

    // Detectar primer mando
    if (SDL_NumJoysticks() > 0 && SDL_IsGameController(0)) {
        controller = SDL_GameControllerOpen(0);
        if (controller)
            std::cout << "Control detectado: " << SDL_GameControllerName(controller) << std::endl;

        /*
        esto solo anda en versiones nuevas del SDL2 gamepad = SDL_OpenGamepad(0);
        if (gamepad) {
            // luz verde del mando de ps4
            SDL_GamepadSetLED(gamepad, 255, 0, 0);
        }*/
    }

    running = true;
    // arbol en coordenadas ARRIBA-izquierda (unificado con Symbian);
    // el flip a GL pasa solo en glViewport/glScissor con este alto
    W3dPantallaAlto = winH;
    MenuPantallaW = winW;
    MenuPantallaH = winH;
    rootViewport->Resize(winW, winH);

    // MODO TEST: "whisk3d --script <ruta>" corre un script de comandos (sin GUI) y
    // sale (0 = todo OK, 1 = fallo). Ver main/test/W3dScript.cpp.
    for (int ai = 1; ai < argc; ai++) {
        if (std::string(argv[ai]) == "--script" && ai + 1 < argc) {
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
            size_t dot = path.find_last_of('.');
            std::string ext = (dot != std::string::npos) ? path.substr(dot) : std::string();
            bool esFbx = (ext == ".fbx" || ext == ".FBX" || ext == ".Fbx");
            if (esFbx) ImportFBX(path); else ImportOBJ(path, false);
            g_redraw = true;
        }
    }

#ifdef __EMSCRIPTEN__
    // WebGL: el browser es de 1 solo hilo, no podemos bloquear con un while(). Le pasamos el
    // frame y el browser lo llama (~60 veces/seg via requestAnimationFrame). El 3er arg = 1
    // "simula" un loop infinito (no se ejecuta lo de abajo). El cleanup queda solo para escritorio.
    emscripten_set_main_loop(MainLoopFrame, 0, 1);
#else
    while (running) MainLoopFrame();
#endif

    if (controller) SDL_GameControllerClose(controller);
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}