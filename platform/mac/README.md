# Whisk3D en macOS

## Requisitos

Herramientas de línea de comandos de **Xcode** y **CMake**:

```bash
xcode-select --install
```

CMake: `brew install cmake` (o desde https://cmake.org/download/).

## Compilar

El script detecta la arquitectura nativa del Mac y mantiene caches separadas para Intel y Apple Silicon:

```bash
platform/mac/build_mac.sh
```

También se puede seleccionar la arquitectura de forma explícita:

```bash
ARCH=x86_64 platform/mac/build_mac.sh
ARCH=arm64 platform/mac/build_mac.sh
```

La aplicación se genera en `platform/mac/build-<arquitectura>/Whisk3D.app`, por ejemplo
`platform/mac/build-x86_64/Whisk3D.app` o `platform/mac/build-arm64/Whisk3D.app`.

## Correr

En el Mac de la misma arquitectura del build:

```bash
# Intel
open platform/mac/build-x86_64/Whisk3D.app

# Apple Silicon
open platform/mac/build-arm64/Whisk3D.app
```

## Notas

- La config específica de macOS (bundle `.app`, `Info.plist`, OpenGL framework e incluye de compatibilidad `GL/*`) vive bajo `platform/mac/`; el CMake raíz solo la incluye cuando compila en Apple.
- Intel (`x86_64`) y Apple Silicon (`arm64`) se configuran y compilan por separado porque SDL2 y CMake detectan capacidades de CPU distintas.
- Podés cambiar el tipo de build con `BUILD_TYPE=Debug platform/mac/build_mac.sh`.
- `BUILD_DIR=/ruta/personalizada` sigue disponible para elegir manualmente el directorio de build.
