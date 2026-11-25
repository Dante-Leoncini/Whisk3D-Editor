#!/bin/bash
# ---------------------------------------------------------------------------------------------------------------
#  preparar_android_viejo.sh -- lo que hace falta para compilar Whisk3D para Android 2.3, fuera del proyecto: en
#  ~/android-viejo (o en W3D_ANDROID_OLD). Sin root. Si algo ya esta, lo saltea.
#
#  1. el NDK r15c: el ultimo que compila para armeabi (ARMv5/v6) y para android-9
#  2. las fuentes de SDL 2.0.7: el ultimo SDL que anda en Android 2.3 (desde 2.0.8 piden la API 14)
#  3. libncurses.so.5 y libtinfo.so.5 para el clang del NDK: los Linux de hoy traen las 6, que le alcanzan
#
#  Del sistema hacen falta ademas: el SDK de Android (build-tools y cualquier plataforma) en ~/Android/Sdk o en
#  ANDROID_HOME, un JDK (javac y keytool) e ImageMagick (convert). Para instalar en el telefono, adb.
# ---------------------------------------------------------------------------------------------------------------
set -e
VIEJO="${W3D_ANDROID_OLD:-$HOME/android-viejo}"
mkdir -p "$VIEJO/fuentes" "$VIEJO/host/lib"
cd "$VIEJO"

if [ ! -x android-ndk-r15c/ndk-build ]; then
    echo "== NDK r15c (unos 1000 MB)"
    if [ ! -f ndk-r15c.zip ]; then
        curl -L --fail -o ndk-r15c.zip.parcial https://dl.google.com/android/repository/android-ndk-r15c-linux-x86_64.zip
        mv ndk-r15c.zip.parcial ndk-r15c.zip
    fi
    unzip -q ndk-r15c.zip
    rm ndk-r15c.zip
fi

if [ ! -f fuentes/SDL-release-2.0.7/Android.mk ]; then
    echo "== SDL 2.0.7"
    curl -L --fail -o fuentes/sdl207.tar.gz https://github.com/libsdl-org/SDL/archive/refs/tags/release-2.0.7.tar.gz
    tar -xzf fuentes/sdl207.tar.gz -C fuentes
fi

for lib in ncurses tinfo; do
    if [ ! -e "host/lib/lib$lib.so.5" ]; then
        seis="$(ldconfig -p | grep "lib$lib.so.6 (libc6,x86-64)" | head -1 | sed 's/.*=> //')"
        [ -n "$seis" ] || { echo "falta lib$lib.so.6 en el sistema (paquete lib${lib}6)"; exit 1; }
        ln -s "$seis" "host/lib/lib$lib.so.5"
    fi
done

SDK="${ANDROID_HOME:-$HOME/Android/Sdk}"
ls -d "$SDK"/build-tools/* > /dev/null 2>&1 || echo "AVISO: no hay build-tools en $SDK (instalarlas con el sdkmanager o Android Studio)"
ls "$SDK"/platforms/android-*/android.jar > /dev/null 2>&1 || echo "AVISO: no hay ninguna plataforma (android.jar) en $SDK"
command -v javac > /dev/null || echo "AVISO: falta un JDK (javac)"
command -v convert > /dev/null || echo "AVISO: falta ImageMagick (convert)"
echo "listo: $VIEJO"
