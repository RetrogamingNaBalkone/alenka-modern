#!/usr/bin/env bash
# Linux build (native or amd64 -> i386). Install dependencies separately.
set -euo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
[[ $(uname -s) == Linux ]] || { echo 'Run this script on Linux.' >&2; exit 1; }
HOST_ARCH=$(dpkg --print-architecture)
[[ $# -le 1 ]] || { echo 'Usage: build-linux.sh [amd64|i386]' >&2; exit 1; }
ARCH=${1:-$HOST_ARCH}
case "$ARCH" in amd64|i386) ;; *) echo "Unsupported architecture: $ARCH" >&2; exit 1;; esac
BUILD="$ROOT/build/linux-$ARCH"
CMAKE_ARGS=()
if [[ $ARCH != "$HOST_ARCH" ]]; then
    [[ $HOST_ARCH == amd64 && $ARCH == i386 ]] || { echo "Unsupported cross build: $HOST_ARCH -> $ARCH" >&2; exit 1; }
    [[ $(dpkg-query -W -f '${db:Status-Status}' libstdc++6:i386 2>/dev/null) == installed ]] || {
        echo 'Install libstdc++6:i386 (in addition to g++-multilib and libsdl2-dev:i386); see docs/linux-build.md.' >&2
        exit 1
    }
    BUILD="$ROOT/build/linux-i386-cross"
    # Keep both SDL's CMake config and pkg-config from selecting host libraries.
    export PKG_CONFIG_LIBDIR=/usr/lib/i386-linux-gnu/pkgconfig:/usr/share/pkgconfig
    export PKG_CONFIG_PATH=
    CMAKE_ARGS=(-DCMAKE_CXX_FLAGS=-m32 -DCMAKE_DISABLE_FIND_PACKAGE_SDL2=TRUE)
fi
OUTPUT="$ROOT/artifacts/releases"
mkdir -p "$BUILD" "$OUTPUT" "$ROOT/build/work"
WORK=$(mktemp -d "$ROOT/build/work/linux-$ARCH-XXXXXX")
trap 'rm -rf -- "$WORK"' EXIT

# -H/-B and the generated install script also work with Bionic's CMake 3.10.
cmake -H"$ROOT" -B"$BUILD" -DCMAKE_BUILD_TYPE=Release "${CMAKE_ARGS[@]}"
cmake --build "$BUILD" -- -j"${JOBS:-2}"
(cd "$BUILD" && ctest --output-on-failure)
VERSION=$(sed -n 's/^project(alenka-modern VERSION \([^ ]*\) .*/\1/p' CMakeLists.txt)
[[ $VERSION =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo 'Cannot determine release version' >&2; exit 1; }
NAME="alenka-modern-${VERSION}-linux-${ARCH}"
PORTABLE="$WORK/alenka"
cmake -DCMAKE_INSTALL_PREFIX="$PORTABLE" -P "$BUILD/cmake_install.cmake"
python3 tools/verify_linux_release.py "$PORTABLE" --arch "$ARCH"

# Exercise the installed binary and its default resource lookup from another CWD.
# Xvfb is required so fullscreen/window/cursor checks use the X11 SDL backend.
(cd "$WORK" && SDL_AUDIODRIVER=dummy SDL_RENDER_DRIVER=software \
    xvfb-run -a "$PORTABLE/alenka" --smoke-test)
tar -czf "$WORK/$NAME.tar.gz" -C "$WORK" alenka

STAGE="$WORK/package"
mkdir -p "$STAGE/DEBIAN" "$STAGE/usr/lib/alenka-modern" \
    "$STAGE/usr/bin" "$STAGE/usr/share/alenka-modern" \
    "$STAGE/usr/share/applications" "$STAGE/usr/share/doc/alenka-modern"
install -m 755 "$PORTABLE/alenka" "$STAGE/usr/lib/alenka-modern/alenka"
cp -a "$PORTABLE/data/." "$STAGE/usr/share/alenka-modern/"
install -m 644 "$PORTABLE/README.txt" "$STAGE/usr/share/doc/alenka-modern/README.txt"
install -m 755 resources/alenka-launcher.sh "$STAGE/usr/bin/alenka"
install -m 644 resources/alenka.desktop "$STAGE/usr/share/applications/alenka.desktop"
install -D -m 644 resources/alenka.png "$STAGE/usr/share/icons/hicolor/256x256/apps/alenka.png"
install -m 644 LICENSE "$STAGE/usr/share/doc/alenka-modern/copyright"
# Derive runtime requirements from ELF symbols, including libgcc/libstdc++.
mkdir -p "$WORK/debian"
printf 'Source: alenka-modern\n\nPackage: alenka-modern\nArchitecture: any\n' > "$WORK/debian/control"
DEPENDS=$(cd "$WORK" && ALENKA_BINARY="$PORTABLE/alenka" dpkg-architecture -a"$ARCH" \
    -c 'dpkg-shlibdeps -O -l"/usr/lib/$DEB_HOST_MULTIARCH" -e"$ALENKA_BINARY"')
DEPENDS=${DEPENDS#shlibs:Depends=}
[[ -n $DEPENDS && $DEPENDS != *$'\n'* && $DEPENDS != shlibs:* ]] || { echo 'Invalid runtime dependencies' >&2; exit 1; }
cat > "$STAGE/DEBIAN/control" <<EOF
Package: alenka-modern
Version: ${VERSION}-1
Architecture: ${ARCH}
Maintainer: RetrogamingNaBalkone <noreply@github.com>
Depends: ${DEPENDS}
Section: education
Priority: optional
Installed-Size: $(du -sk "$STAGE/usr" | cut -f1)
Homepage: https://github.com/RetrogamingNaBalkone/alenka-modern
Description: Alenka keyboard trainer
 Native SDL2 port of the DOS keyboard trainer with Russian and English courses.
EOF
find "$STAGE" -type d -exec chmod 755 {} +
chmod 644 "$STAGE/DEBIAN/control"
fakeroot dpkg-deb --build "$STAGE" "$WORK/$NAME.deb"
dpkg-deb --info "$WORK/$NAME.deb"
# Extract and verify the actual .deb payload as well as the tarball bytes.
mkdir "$WORK/extracted"
dpkg-deb -x "$WORK/$NAME.deb" "$WORK/extracted"
python3 tools/verify_linux_release.py "$PORTABLE" --deb-root "$WORK/extracted" --archive "$WORK/$NAME.tar.gz" --arch "$ARCH"
# Resolve resources from /usr/share even before the package is installed.
(cd "$WORK" && SDL_AUDIODRIVER=dummy SDL_RENDER_DRIVER=software \
    xvfb-run -a "$WORK/extracted/usr/lib/alenka-modern/alenka" \
    --data "$WORK/extracted/usr/share/alenka-modern" --smoke-test)
cp "$WORK/$NAME.deb" "$WORK/$NAME.tar.gz" "$OUTPUT/"
(cd "$OUTPUT" && sha256sum "$NAME.deb" "$NAME.tar.gz" > "$NAME.sha256")
echo "Linux $ARCH release: $OUTPUT/$NAME.{deb,tar.gz,sha256}"
