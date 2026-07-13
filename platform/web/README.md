# Whisk3D en la web (WebGL / Emscripten)

Compila el editor a WebGL con Emscripten. El backend de gráficos es `gles2/w3dGraphicsGLES2.cpp` (ES2/WebGL), no el de escritorio.

## Requisitos

**Emscripten (emsdk)** activado, con `emcc` en el PATH:

```bash
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk && ./emsdk install latest && ./emsdk activate latest
source ./emsdk_env.sh
```

## Compilar

Desde cualquier lado (el script se ubica solo en la raíz del repo):

```bash
platform/web/build_web.sh
```

Genera `platform/web/build/whisk3d.html` + `.js` + `.wasm` + `.data` (con `res/` empaquetado adentro).

> Notas del build: memoria fija de 128 MB (**sin** `-sALLOW_MEMORY_GROWTH`: con growth los navegadores nuevos rechazan `TextDecoder` sobre el buffer resizable → pantalla negra). Es 128 y no 256 porque iOS Safari fallaba al alocar el bloque fijo de 256 MB (pantalla en blanco). El shell HTML propio está en `platform/web/shell.html`.

## Probar

El `.wasm` no carga por `file://`, hay que servirlo por HTTP. Usá el server sin cache (así cada recarga trae el build fresco):

```bash
python platform/web/serve_nocache.py
```

Y abrí **http://localhost:8000/whisk3d.html**. El server escucha en `0.0.0.0`, así que también podés probar desde el celu con `http://IP-DE-TU-PC:8000/whisk3d.html` (misma red).

Alternativa: `emrun platform/web/build/whisk3d.html`.
