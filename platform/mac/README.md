# Whisk3D en macOS

## Requisitos

Herramientas de línea de comandos de **Xcode** y **CMake**:

```bash
xcode-select --install
```

CMake: `brew install cmake` (o desde https://cmake.org/download/).

## Compilar

```bash
platform/mac/build_mac.sh
```

El script baja los submódulos, compila SDL2 estáticamente y genera la aplicación en `platform/mac/build/Whisk3D.app`.

## Correr

```bash
open platform/mac/build/Whisk3D.app
```

## Notas

- La config específica de macOS (bundle `.app`, `Info.plist`, `install`) vive en [`platform/mac/CMakeLists.txt`](CMakeLists.txt), que el CMake raíz incluye cuando compila en Apple.
- Podés cambiar el tipo de build con `BUILD_TYPE=Debug platform/mac/build_mac.sh`.
