# Whisk3D en GNU/Linux (Ubuntu)

## Dependencias

```bash
sudo apt update
sudo apt install git build-essential cmake \
  libgl1-mesa-dev \
  libglu1-mesa-dev \
  mesa-common-dev \
  libsdl2-dev
```

## Compilar (lo más fácil)

Con las dependencias instaladas:

```bash
platform/linux/build_linux.sh
```

Baja los submódulos, configura y compila en Release → `platform/linux/build/whisk3d`. Para Debug: `BUILD_TYPE=Debug platform/linux/build_linux.sh`.

## A mano (es lo que hace el script)

Desde la raíz del repo:

```bash
git submodule update --init --recursive
cmake -B platform/linux/build -DCMAKE_BUILD_TYPE=Release
cmake --build platform/linux/build -- -j8
```

El ejecutable queda en `platform/linux/build/whisk3d`.

## Debug

```bash
cmake --build platform/linux/build --config Debug -- -j8
gdb ./platform/linux/build/whisk3d
run
```

Si crashea, para ver en detalle dónde:

```bash
bt full
```

## Instalador `.deb` (Debian / Ubuntu)

Desde `platform/linux/build` (¡gracias [**Zariep**](https://github.com/ItsZariep) por este aporte!):

```bash
cpack
```

Genera `whisk3d-<ver>-Linux.deb`. (RPM: agregá `RPM` a `CPACK_GENERATOR` en `CMakeLists.txt` si tenés `rpmbuild`.)

## AppImage (portable)

El cmake estándar **no** trae generador CPack de AppImage (`cpack` aborta con *"Could not create CPack generator: AppImage"*). Se arma con un script propio que usa `linuxdeploy` + `appimagetool`:

```bash
platform/linux/build_appimage.sh
```

Genera `platform/linux/build/Whisk3D-<ver>-x86_64.AppImage`. Descarga `linuxdeploy`/`appimagetool` a `~/.cache/whisk3d-appimage` la primera vez (o los toma del PATH si ya están). Requiere haber compilado antes (`build_linux.sh`).
