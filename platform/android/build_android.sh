#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/../.." # el script vive en platform/android/ -> raiz del repo 2 niveles arriba

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

# -j ACOTADO, NUNCA $(nproc) pelado: son ~200 unidades de compilacion POR ABI y con
# paralelismo ilimitado el OOM killer cierra la sesion entera (mismo criterio que
# W3dCompilarJobs, main/io/CompilarJuego.h). Se puede pisar con W3D_JOBS=n.
if [[ -z "${W3D_JOBS:-}" ]]; then
	W3D_NPROC="$(nproc 2>/dev/null || echo 2)"
	W3D_JOBS=$(( W3D_NPROC / 2 ))
	if (( W3D_JOBS < 1 )); then W3D_JOBS=1; fi
	if (( W3D_JOBS > 8 )); then W3D_JOBS=8; fi
fi
echo "Compilando con -j${W3D_JOBS} (W3D_JOBS para cambiarlo)."

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

# Recursos del editor -> assets del APK. Se REGENERA en cada build desde el res/
# canonico (esta gitignoreado y NO se commitea, para no duplicar/desactualizar).
echo "Copiando res/ al APK (assets)..."
mkdir -p "$APROJECT_PATH/app/src/main/assets"   # assets/ esta gitignoreado (no se commitea) -> crearlo si no existe
rm -rf "$APROJECT_PATH/app/src/main/assets/res"
cp -r res "$APROJECT_PATH/app/src/main/assets/res"

build_one() {
	local abi="$1"

	echo "[BUILD] $abi"

	# jni/ vive en platform/android -> NDK_PROJECT_PATH ahi (libs/ y obj/ salen adentro
	# de platform/android, no en la raiz del repo al lado de los submodulos).
	"$NDK_BUILD" \
		-j"${W3D_JOBS}" \
		-s \
		APP_ABI="$abi" \
		NDK_PROJECT_PATH="$APROJECT_PATH" \
		APP_BUILD_SCRIPT="$APROJECT_PATH/jni/Android.mk" \
		NDK_APPLICATION_MK="$APROJECT_PATH/jni/Application.mk" \
		>/dev/null

	mkdir -p "$AAPK_PATH/$abi"
	cp "$APROJECT_PATH/libs/$abi"/*.so "$AAPK_PATH/$abi"/

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
echo "Aplicación empaquetada con gradlew (firmada con el keystore de debug)."
# El nombre del APK lo arma gradle (app/build.gradle: whisk3d-editor-<fecha>-<abis>.apk) y cambia con la
# fecha y con las ABIs que se hayan compilado -> NO clavar un patron aca. Se toma el .apk mas NUEVO de la
# carpeta de salida. El '|| true' es por el 'set -euo pipefail': si el glob no encuentra nada el pipe falla
# y tumbaba el script con exit 2 aunque el APK estuviera bien empaquetado (este bloque es solo el reporte).
OUTDIR="$APROJECT_PATH/app/distribution/android/app/outputs/apk/debug"
APK=$(find "$OUTDIR" -maxdepth 1 -name '*.apk' -printf '%T@ %p\n' 2>/dev/null | sort -nr | head -1 | cut -d' ' -f2-) || true
if [[ -n "${APK:-}" ]]; then
	echo "output: $APK"
else
	echo "ERROR: gradlew termino pero no hay ningun .apk en $OUTDIR" >&2
	exit 1
fi