#!/usr/bin/env bash
# Called after the native Linux build; no package installation or host changes.
set -euo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
ARCH=$(dpkg --print-architecture)
case "$ARCH" in amd64) AI_ARCH=x86_64;; i386) AI_ARCH=i686;; *) exit 1;; esac
VERSION=$(sed -n 's/^project(alenka-modern VERSION \([^ ]*\) .*/\1/p' CMakeLists.txt)
[[ $VERSION =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || exit 1
BUILD="$ROOT/build/linux-$ARCH"
TOOLS="$ROOT/build/appimage-tools/$ARCH"
mkdir -p "$ROOT/build/work" "$ROOT/artifacts/releases"
WORK=$(mktemp -d "$ROOT/build/work/appimage-$ARCH-XXXXXX")
trap 'rm -rf -- "$WORK"' EXIT
APPDIR="$WORK/Alenka.AppDir"
python3 tools/fetch_appimage_tools.py "$ARCH" "$TOOLS"
# Extract the build tools so packaging works inside unprivileged containers without FUSE.
for tool in linuxdeploy appimagetool; do
    mkdir "$WORK/$tool"
    (cd "$WORK/$tool" && "$TOOLS/$tool" --appimage-extract >/dev/null)
done
mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/share/doc/alenka-modern" "$APPDIR/usr/share/metainfo"
install -m 755 "$BUILD/alenka" "$APPDIR/usr/bin/alenka"
cp -a "$BUILD/data" "$APPDIR/usr/share/alenka-modern"
install -m 755 resources/AppRun "$APPDIR/AppRun"
install -m 644 resources/README-linux.txt "$APPDIR/usr/share/doc/alenka-modern/README.txt"
install -m 644 LICENSE "$APPDIR/usr/share/doc/alenka-modern/LICENSE"
install -m 644 resources/AppImage-runtime-LICENSE.txt "$APPDIR/usr/share/doc/alenka-modern/AppImage-runtime-LICENSE.txt"
install -m 644 resources/AppImage-runtime-SOURCE.txt "$APPDIR/usr/share/doc/alenka-modern/AppImage-runtime-SOURCE.txt"
install -m 644 resources/alenka.metainfo.xml "$APPDIR/usr/share/metainfo/io.github.RetrogamingNaBalkone.alenka-modern.metainfo.xml"
# SDL may dlopen these backends: they do not necessarily appear in the executable's ldd output.
# Do not bundle host GPU drivers, glibc or the ELF loader.
libraries=()
for soname in libstdc++.so.6 libasound.so.2 libpulse.so.0 libXcursor.so.1 libXinerama.so.1 libXi.so.6 \
    libXrandr.so.2 libXss.so.1 libXxf86vm.so.1 libwayland-client.so.0 \
    libwayland-cursor.so.0 libwayland-egl.so.1 libxkbcommon.so.0 libudev.so.1 libdbus-1.so.3; do
    path=$(ldconfig -p | awk -v name="$soname" '$1 == name && !found {print $NF; found=1}')
    if [[ -n $path ]]; then libraries+=(--library "$path"); fi
done
"$WORK/linuxdeploy/squashfs-root/AppRun" --appdir "$APPDIR" \
    --executable "$APPDIR/usr/bin/alenka" --desktop-file resources/alenka.desktop \
    --icon-file resources/alenka.png "${libraries[@]}"
# The custom AppRun must survive deployment (it selects the writable user directory).
cmp resources/AppRun "$APPDIR/AppRun"
python3 tools/appimage_licenses.py "$APPDIR"
desktop-file-validate "$APPDIR/alenka.desktop"
appstreamcli validate --no-net "$APPDIR/usr/share/metainfo/"*.metainfo.xml
python3 tools/verify_appimage.py --appdir "$APPDIR" --arch "$ARCH"
NAME="alenka-modern-$VERSION-$AI_ARCH.AppImage"
OUTPUT="$WORK/$NAME"
ARCH="$AI_ARCH" VERSION="$VERSION" "$WORK/appimagetool/squashfs-root/AppRun" \
    --runtime-file "$TOOLS/runtime" "$APPDIR" "$OUTPUT"
chmod 755 "$OUTPUT"
python3 tools/verify_appimage.py "$OUTPUT" --arch "$ARCH"
install -m 755 "$OUTPUT" "$ROOT/artifacts/releases/$NAME"
(cd "$ROOT/artifacts/releases" && sha256sum "$NAME" > "$NAME.sha256")
echo "AppImage: $ROOT/artifacts/releases/$NAME"
