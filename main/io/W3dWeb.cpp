// ============================================================================
//  W3dWeb.cpp - abrir con el selector del navegador y guardar = descargar (ver W3dWeb.h)
// ============================================================================
#include "W3dWeb.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <sys/stat.h>
#include "render/RenderOptions.h"   // g_redraw
#include "W3dLang.h"                // T(): los botones del selector en el idioma del editor

extern "C" void WebUnloadFile(const char* vfsPath, const char* suggestedName);   // main.cpp (EM_JS)

static void (*gWebCb)(const std::string&) = 0;

// lo elegido en el selector ya esta en /uploads: se lo pasa al que lo pidio
extern "C" EMSCRIPTEN_KEEPALIVE void W3dWebPickedFile(const char* path) {
    void (*cb)(const std::string&) = gWebCb;
    gWebCb = 0;
    if (cb && path && path[0]) cb(std::string(path));
    g_redraw = true;
}

// el selector del navegador, adentro de un cartel con un BOTON real: el input.click() tiene que salir de un gesto
// del usuario (iOS Safari no abre el selector si lo dispara el bucle de dibujo). multiple: un .obj viene con su .mtl
// y sus texturas, un .gltf con su .bin; todos se suben juntos y se avisa con el PRINCIPAL (el de la extension pedida).
EM_JS(void, W3dWebSelector, (const char* titlePtr, const char* acceptPtr, int multiple, const char* mainPtr,
                             const char* pickPtr, const char* cancelPtr), {
    var title = UTF8ToString(titlePtr), accept = UTF8ToString(acceptPtr), main = UTF8ToString(mainPtr).split(',');
    var prev = document.getElementById('w3d-pick'); if (prev && prev.parentNode) prev.parentNode.removeChild(prev);
    var ov = document.createElement('div');
    ov.id = 'w3d-pick';
    ov.style.cssText = 'position:fixed;left:0;top:0;right:0;bottom:0;z-index:99999;'
        + 'background:rgba(0,0,0,0.5);display:flex;align-items:flex-start;justify-content:center;';
    var card = document.createElement('div');
    card.style.cssText = 'margin-top:8vh;width:92%;max-width:440px;background:#2b2b2b;border:1px solid #555;'
        + 'border-radius:10px;padding:16px;box-shadow:0 6px 30px rgba(0,0,0,0.6);font-family:sans-serif;color:#ddd;';
    var lab = document.createElement('div');
    lab.textContent = title;
    lab.style.cssText = 'font-size:15px;margin-bottom:12px;color:#9ac9ff;';
    var input = document.createElement('input');
    input.type = 'file';
    if (accept) input.accept = accept;
    input.multiple = !!multiple;
    input.style.cssText = 'position:absolute;left:-9999px;width:1px;height:1px;opacity:0;';
    function close(){ if (ov.parentNode) ov.parentNode.removeChild(ov); }
    input.onchange = function(e) {
        var files = Array.prototype.slice.call(e.target.files);
        close();
        if (!files.length) return;
        try { FS.mkdir('/uploads'); } catch (_e) {}
        var main = null, pending = files.length;
        files.forEach(function(f) {
            var r = new FileReader();
            r.onload = function() {
                var p = '/uploads/' + f.name;
                FS.writeFile(p, new Uint8Array(r.result));
                var low = f.name.toLowerCase();
                for (var i = 0; i < main.length; i++)
                    if (main[i] && low.endsWith(main[i]) && !main) main = p;
                if (--pending === 0) ccall('W3dWebPickedFile', null, ['string'], [main || ('/uploads/' + files[0].name)]);
            };
            r.readAsArrayBuffer(f);
        });
    };
    var btnPick = document.createElement('button');
    btnPick.textContent = UTF8ToString(pickPtr);
    btnPick.style.cssText = 'width:100%;font-size:17px;padding:14px;border:none;border-radius:6px;'
        + 'background:#3a8f68;color:#fff;font-weight:bold;';
    var btnC = document.createElement('button');
    btnC.textContent = UTF8ToString(cancelPtr);
    btnC.style.cssText = 'width:100%;margin-top:10px;font-size:15px;padding:11px;border:none;border-radius:6px;background:#444;color:#ddd;';
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

static std::string Base(const std::string& r) {
    const size_t b = r.find_last_of("/\\");
    return (b == std::string::npos) ? r : r.substr(b + 1);
}

void W3dDownloadIfWeb(const std::string& vfsPath) {
    struct stat st;
    if (vfsPath.empty() || stat(vfsPath.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) return;   // (un export que fallo a medias)
    WebUnloadFile(vfsPath.c_str(), Base(vfsPath).c_str());
}

bool W3dWebOpenFile(const std::string& title, const std::string& filter, void (*cb)(const std::string&)) {
    if (!cb) return false;
    // ".obj .fbx .gltf" -> accept ".obj,.fbx,.gltf" y las extensiones PRINCIPALES (de cual se avisa)
    std::string accept, main;
    bool multiple = false;
    size_t i = 0;
    while (i < filter.size()) {
        while (i < filter.size() && filter[i] == ' ') i++;
        size_t j = i; while (j < filter.size() && filter[j] != ' ') j++;
        if (j > i) {
            const std::string e = filter.substr(i, j - i);
            if (!accept.empty()) { accept += ","; main += ","; }
            accept += e; main += e;
            if (e == ".obj" || e == ".gltf") multiple = true;   // vienen con companieros (.mtl/.bin/texturas)
        }
        i = j;
    }
    if (multiple) accept += ",.mtl,.bin,image/*,.png,.jpg,.jpeg,.bmp,.tga";
    gWebCb = cb;
    W3dWebSelector(title.c_str(), accept.c_str(), multiple ? 1 : 0, main.c_str(), T("Choose file"), T("Cancel"));
    return true;
}

std::string W3dWebFolderDownloads() {
    mkdir("/descargas", 0777);
    return "/descargas";
}

#else   // fuera del navegador: nada

void W3dDownloadIfWeb(const std::string&) {}
bool W3dWebOpenFile(const std::string&, const std::string&, void (*)(const std::string&)) { return false; }
std::string W3dWebFolderDownloads() { return std::string(); }

#endif
