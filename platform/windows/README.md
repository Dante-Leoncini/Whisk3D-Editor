# Whisk3D en Windows

Se compila con **CMake + Visual Studio (MSVC)**. Nada de Git Bash ni MinGW.

## Requisitos

- **Git** — https://git-scm.com/download/win
- **CMake** — https://cmake.org/download/ (tildá "Add CMake to the PATH").
- **Visual Studio** con el workload **"Desarrollo para el escritorio con C++"** (trae el compilador MSVC y el Windows SDK). Las *Build Tools for Visual Studio* solas también sirven.

> Sirve cualquier Visual Studio moderno — CMake detecta solo el que tengas instalado (acá se usa VS 2026, pero anda igual con 2019/2022).

## Compilar (lo más fácil)

Doble click en `platform\windows\build_windows.bat`, o desde una consola:

```bat
platform\windows\build_windows.bat
```

Baja los submódulos, configura y compila en **Release**. El ejecutable queda en `platform\windows\build\Release\whisk3d.exe` (con la carpeta `res\` al lado). Para Debug: `build_windows.bat Debug`.

## A mano (es lo que hace el script)

Desde la **raíz del repo**:

```bat
git submodule update --init --recursive
cmake -S . -B platform\windows\build
cmake --build platform\windows\build --config Release
```

CMake usa el Visual Studio que tengas y compila **x64** por defecto. Para forzar arquitectura, agregá al *configure* `-A x64` o `-A Win32` (Windows 32 bits).

## Instalador (.exe)

Con [NSIS](https://nsis.sourceforge.io/Download) instalado, desde `platform\windows\build`:

```bat
cpack -G NSIS
```

Genera `Whisk3D-<fecha>-win64.exe` (accesos directos + asociación de `.w3d`). Sin NSIS, `cpack -G ZIP` hace un `.zip` portable.

## Correr

```bat
platform\windows\build\Release\whisk3d.exe
```

Busca `res\` **al lado del .exe** (el build ya la copia ahí).
