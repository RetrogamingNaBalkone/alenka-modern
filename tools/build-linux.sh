#!/usr/bin/env bash
# Native Linux build. Install dependencies separately; never modifies the host.
set -euo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
[[ $(uname -s) == Linux ]] || { echo 'Run this script on Linux.' >&2; exit 1; }
ARCH=$(dpkg --print-architecture)
case "$ARCH" in amd64|i386) ;; *) echo "Unsupported architecture: $ARCH" >&2; exit 1;; esac
BUILD="$ROOT/build/linux-$ARCH"
OUTPUT="$ROOT/artifacts/releases"
mkdir -p "$BUILD" "$OUTPUT" "$ROOT/build/work"
WORK=$(mktemp -d "$ROOT/build/work/linux-$ARCH-XXXXXX")
trap 'rm -rf -- "$WORK"' EXIT

# -H/-B and the generated install script also work with Bionic's CMake 3.10.
cmake -H"$ROOT" -B"$BUILD" -DCMAKE_BUILD_TYPE=Release
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
    "$STAGE/usr/bin" "$STAGE/usr/share/applications" "$STAGE/usr/share/doc/alenka-modern"
cp -a "$PORTABLE/." "$STAGE/usr/lib/alenka-modern/"
install -m 755 resources/alenka-launcher.sh "$STAGE/usr/bin/alenka"
install -m 644 resources/alenka.desktop "$STAGE/usr/share/applications/alenka.desktop"
install -m 644 LICENSE "$STAGE/usr/share/doc/alenka-modern/copyright"
# Derive runtime requirements from ELF symbols, including libgcc/libstdc++.
mkdir -p "$WORK/debian"
printf 'Source: alenka-modern\n\nPackage: alenka-modern\nArchitecture: any\n' > "$WORK/debian/control"
DEPENDS=$(cd "$WORK" && dpkg-shlibdeps -O -e"$PORTABLE/alenka")
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
python3 tools/verify_linux_release.py "$WORK/extracted/usr/lib/alenka-modern" --archive "$WORK/$NAME.tar.gz" --arch "$ARCH"
cp "$WORK/$NAME.deb" "$WORK/$NAME.tar.gz" "$OUTPUT/"
(cd "$OUTPUT" && sha256sum "$NAME.deb" "$NAME.tar.gz" > "$NAME.sha256")
echo "Linux $ARCH release: $OUTPUT/$NAME.{deb,tar.gz,sha256}"
