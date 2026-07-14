#!/usr/bin/env bash
# ============================================================================
#  build_appimage.sh - genera el AppImage de Whisk3D (Linux).
#  El cmake estandar NO trae generador CPack de AppImage (cpack falla con
#  "Could not create CPack generator: AppImage"), asi que se arma a mano con
#  linuxdeploy + appimagetool (se descargan a una cache si faltan).
#
#  Requiere el binario ya compilado:  platform/linux/build_linux.sh
#  Genera:  platform/linux/build/Whisk3D-<ver>-x86_64.AppImage
#
#  Uso:  platform/linux/build_appimage.sh
#  Las tools: si linuxdeploy/appimagetool estan en el PATH se usan; sino se
#  bajan a $APPIMAGE_TOOLS (default ~/.cache/whisk3d-appimage).
# ============================================================================
set -euo pipefail
cd "$(dirname "$0")/../.." # el script vive en platform/linux/ -> raiz del repo 2 niveles arriba

BUILD=platform/linux/build
if [[ ! -x "$BUILD/whisk3d" ]]; then
    echo "ERROR: falta el binario ($BUILD/whisk3d). Compila primero:  platform/linux/build_linux.sh" >&2
    exit 1
fi

# version = la del .deb generado por cpack (si esta), sino la fecha de hoy YY.MM.DD (mismo criterio que el resto)
VER=$(ls "$BUILD"/whisk3d-*-Linux.deb 2>/dev/null | sed -E 's#.*/whisk3d-([0-9.]+)-Linux\.deb#\1#' | head -1)
[[ -n "$VER" ]] || VER=$(date +%y.%m.%d)

# --- herramientas (PATH -> cache -> descargar) ---
TOOLS="${APPIMAGE_TOOLS:-$HOME/.cache/whisk3d-appimage}"
mkdir -p "$TOOLS"
obtener() { # $1=comando $2=archivo $3=url  -> imprime la ruta ejecutable
    if command -v "$1" >/dev/null 2>&1; then command -v "$1"; return; fi
    if [[ ! -x "$TOOLS/$2" ]]; then echo "Descargando $2..." >&2; wget -qO "$TOOLS/$2" "$3"; chmod +x "$TOOLS/$2"; fi
    echo "$TOOLS/$2"
}
LD=$(obtener linuxdeploy  linuxdeploy-x86_64.AppImage  "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage")
AT=$(obtener appimagetool appimagetool-x86_64.AppImage "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage")
export PATH="$(dirname "$AT"):$PATH" # linuxdeploy invoca appimagetool desde el PATH
# sin FUSE (headless/CI) las AppImage de las tools no montan -> extraer y correr
export APPIMAGE_EXTRACT_AND_RUN=1

# --- AppDir (install del cmake) -> AppImage ---
WORK=$(mktemp -d); APPDIR="$WORK/AppDir"
DESTDIR="$APPDIR" cmake --install "$BUILD" --prefix /usr >/dev/null

export ARCH=x86_64 VERSION="$VER"
( cd "$WORK" && "$LD" --appdir AppDir \
    --executable AppDir/usr/bin/whisk3d \
    --desktop-file AppDir/usr/share/applications/whisk3d.desktop \
    --icon-file AppDir/usr/share/icons/hicolor/256x256/apps/whisk3d.png \
    --output appimage )

# linuxdeploy nombra segun el Name del .desktop ("Whisk3D pre-alpha") -> renombrar a la convencion del sitio
GEN=$(ls "$WORK"/*.AppImage | head -1)
OUT="$BUILD/Whisk3D-$VER-x86_64.AppImage"
mv "$GEN" "$OUT"
rm -rf "$WORK"
echo
echo "AppImage generado -> $OUT"
