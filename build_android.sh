#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

ANDROID_NDK="${ANDROID_NDK:-/opt/android-ndk}"
NDK_BUILD="$ANDROID_NDK/ndk-build"
AAPK_PATH="${AAPK_PATH:-$(pwd)/platform/android/app/src/main/jniLibs}"
APROJECT_PATH="${APROJECT_PATH:-$(pwd)/platform/android}"

if [[ ! -x "$NDK_BUILD" ]]; then
	echo "ERROR: ndk-build no encontrado."
	exit 1
fi

if [[ ! -d thirdparty/SDL2 ]]; then
	echo "ERROR: ejecuta el script desde la raíz del repo."
	exit 1
fi

# Si no se especifica ninguna ABI, compilar todas.
if [[ $# -eq 0 ]]; then
	set -- arm64 amd64 arm32
fi

abis=()

for abi in "$@"; do
	case "$abi" in
		arm64-v8a|aarch64|arm64)
			abis+=(arm64-v8a)
			;;
		x86_64|amd64)
			abis+=(x86_64)
			;;
		armeabi-v7a|arm|arm32)
			abis+=(armeabi-v7a)
			;;
		*)
			echo "ABI desconocida: $abi"
			exit 1
			;;
	esac
done

mkdir -p "$AAPK_PATH"

build_one() {
	local abi="$1"

	echo "[BUILD] $abi"

	"$NDK_BUILD" \
		-j"$(nproc)" \
		-s \
		APP_ABI="$abi" \
		NDK_PROJECT_PATH=. \
		APP_BUILD_SCRIPT=./jni/Android.mk \
		NDK_APPLICATION_MK=./jni/Application.mk \
		>/dev/null

	mkdir -p "$AAPK_PATH/$abi"
	cp libs/"$abi"/*.so "$AAPK_PATH/$abi"/

	echo "[ OK ] $abi"
}

pids=()

for abi in "${abis[@]}"; do
	build_one "$abi" &
	pids+=($!)
done

failed=0

for pid in "${pids[@]}"; do
	wait "$pid" || failed=1
done

if [[ $failed -ne 0 ]]; then
	echo
	echo "Alguna compilación falló."
	exit 1
fi

echo
echo "Todo listo."
echo "Bibliotecas copiadas a:"
echo "  $AAPK_PATH"

echo "Empaquetando con gradlew..."
cd $APROJECT_PATH
./gradlew assembleDebug
echo "Aplicación empaquetada con gradlew."
echo "output: $APROJECT_PATH/app/distribution/android/app/outputs/apk/debug/app-debug.apk"