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

## Instaladores (.deb / .rpm / AppImage)

Desde `platform/linux/build` (¡gracias [**Zariep**](https://github.com/ItsZariep) por este aporte!):

```bash
cpack
```

### Para el AppImage

El CMake que viene en Ubuntu no alcanza; actualizalo:

```bash
pip install cmake --upgrade
```

Y necesitás `patchelf` y `appimagetool`:

```bash
sudo apt install patchelf
wget https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage
chmod +x appimagetool-x86_64.AppImage
```

Ahora `cpack` genera también el AppImage.
