#!/bin/bash
# ---------------------------------------------------------------------------------------------------------------
#  build_android_viejo.sh -- Whisk3D para Android 2.3 (API 10) en ARMv6: build/whisk3d-android-viejo.apk
#  Antes, una vez: preparar_android_viejo.sh (el NDK r15c y SDL 2.0.7, en ~/android-viejo).
#  Con "instalar" al final, lo instala en el telefono conectado (adb) y lo abre.
#
#  1. lo nativo con ndk-build (NDK r15c): libSDL2.so y libmain.so (el editor) para armeabi
#  2. el Java (la actividad de SDL 2.0.7 + la nuestra) con javac y d8, para la API 10
#  3. el APK con aapt (manifiesto, icono y res/ del editor como assets), alineado y firmado con la llave de debug
# ---------------------------------------------------------------------------------------------------------------
set -e
cd "$(dirname "$0")"
AQUI="$PWD"
RAIZ="$(cd ../.. && pwd)"
VIEJO="${W3D_ANDROID_VIEJO:-$HOME/android-viejo}"
NDK="$VIEJO/android-ndk-r15c"
SDL="$VIEJO/fuentes/SDL-release-2.0.7"
SDK="${ANDROID_HOME:-$HOME/Android/Sdk}"
BT="$(ls -d "$SDK"/build-tools/* 2>/dev/null | sort -V | tail -1)"
JAR="$(ls -d "$SDK"/platforms/android-*/android.jar 2>/dev/null | sort -V | tail -1)"
ADB="$SDK/platform-tools/adb"
KS="${W3D_KEYSTORE:-$HOME/.android/debug.keystore}"
B="$AQUI/build"
[ -x "$NDK/ndk-build" ] && [ -f "$SDL/Android.mk" ] || { echo "falta el NDK r15c o SDL 2.0.7 en $VIEJO (correr preparar_android_viejo.sh)"; exit 1; }
# (el clang del NDK r15c pide libncurses.so.5 y libtinfo.so.5, que los Linux de hoy ya no traen: preparar_android_viejo.sh
# deja en host/lib unos enlaces a las versiones 6, que le alcanzan)
export LD_LIBRARY_PATH="$VIEJO/host/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
[ -x "$BT/aapt" ] && [ -f "$JAR" ] || { echo "faltan las build-tools o una plataforma (android.jar) en $SDK"; exit 1; }
[ -f "$KS" ] || keytool -genkeypair -keystore "$KS" -storepass android -alias androiddebugkey -keypass android \
                       -keyalg RSA -keysize 2048 -validity 10000 -dname "CN=Android Debug,O=Android,C=US"

echo "== nativo (ndk-build)"
# -j ACOTADO (como platform/android/build_android.sh): con $(nproc) pelado el OOM killer cierra la sesion entera
W3D_JOBS="${W3D_JOBS:-$(( $(nproc) / 2 ))}"; [ "$W3D_JOBS" -ge 1 ] 2>/dev/null || W3D_JOBS=1
"$NDK/ndk-build" -j"$W3D_JOBS" NDK_PROJECT_PATH="$AQUI" APP_BUILD_SCRIPT="$AQUI/jni/Android.mk" \
    NDK_APPLICATION_MK="$AQUI/jni/Application.mk" NDK_OUT="$B/obj" NDK_LIBS_OUT="$B/lib" \
    SDL_DIR="$SDL" W3D_RAIZ="$RAIZ" > "$B.ndk.log" 2>&1 || { grep -m20 -E "error|Error" "$B.ndk.log"; echo "(ver $B.ndk.log)"; exit 1; }

echo "== java"
rm -rf "$B/clases" "$B/dex" && mkdir -p "$B/clases" "$B/dex"
# (el SDK moderno para compilar: la actividad de SDL 2.0.7 usa lo nuevo detras de chequeos de version; d8 deja el
# bytecode para la API 10)
javac -nowarn -source 8 -target 8 -bootclasspath "$JAR" -classpath "$JAR" -d "$B/clases" \
    "$SDL"/android-project/src/org/libsdl/app/*.java "$AQUI"/java/com/whisk3d/viejo/*.java 2> "$B.javac.log" || { cat "$B.javac.log"; exit 1; }
"$BT/d8" --min-api 10 --lib "$JAR" --output "$B/dex" $(find "$B/clases" -name '*.class')

echo "== apk"
rm -rf "$B/res" "$B/assets" "$B/apk" && mkdir -p "$B/res/drawable" "$B/assets" "$B/apk/lib/armeabi"
convert "$RAIZ/res/Whisk3D.png" -resize 48x48 "$B/res/drawable/icono.png"
cp -r "$RAIZ/res" "$B/assets/res"
rm -rf "$B/assets/res/installer"
VERSION="$(date +%y.%m.%d)"
"$BT/aapt" package -f -M "$AQUI/AndroidManifest.xml" -S "$B/res" -A "$B/assets" -I "$JAR" \
    --version-name "$VERSION" -F "$B/sin_firmar.apk"
cp "$B/dex/classes.dex" "$B/apk/"
cp "$B"/lib/armeabi/*.so "$B/apk/lib/armeabi/"
(cd "$B/apk" && "$BT/aapt" add "$B/sin_firmar.apk" classes.dex lib/armeabi/*.so > /dev/null)
"$BT/zipalign" -f 4 "$B/sin_firmar.apk" "$B/alineado.apk"
"$BT/apksigner" sign --ks "$KS" --ks-pass pass:android --key-pass pass:android --min-sdk-version 10 \
    --out "$B/whisk3d-android-viejo.apk" "$B/alineado.apk"
rm -f "$B/sin_firmar.apk" "$B/alineado.apk"
echo "  $B/whisk3d-android-viejo.apk ($(du -h "$B/whisk3d-android-viejo.apk" | cut -f1))"

if [ "${1:-}" = "instalar" ]; then
    echo "== instalar en el telefono"
    "$ADB" install -r "$B/whisk3d-android-viejo.apk"
    "$ADB" shell am start -n com.whisk3d.viejo/.Whisk3DActivity
fi
