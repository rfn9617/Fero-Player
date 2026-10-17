#!/usr/bin/env bash
# Runs inside MSYS2 UCRT64. Usage: build-mpv.sh <output-dir>
set -euo pipefail
OUT="$1"
WORK=/c/lq/mpv
PREFIX="$WORK/prefix"
FFMPEG_VERSION=${FFMPEG_VERSION:-9.0.2}
MPV_VERSION=${MPV_VERSION:-3186d369f9}
JOBS=$(nproc)
mkdir -p "$WORK" "$PREFIX"
cd "$WORK"

P=mingw-w64-ucrt-x86_64
echo "==> Packages"
pacman -S --needed --noconfirm git make diffutils tar xz curl \
  $P-toolchain $P-meson $P-ninja $P-pkgconf $P-nasm $P-cmake $P-ntldd \
  $P-libplacebo $P-shaderc $P-spirv-cross $P-vulkan-headers \
  $P-libass $P-freetype $P-harfbuzz $P-fribidi $P-lcms2 $P-libjpeg-turbo $P-zimg $P-uchardet \
  $P-dav1d $P-zvbi $P-libopenmpt $P-libjxl $P-libwebp $P-libxml2 \
  $P-zlib $P-bzip2 $P-xz $P-libiconv

export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
FLAGS="-O3 -march=x86-64-v3 -mtune=generic -pipe"

if [ ! -f "$PREFIX/lib/libavcodec.a" ]; then
  echo "==> FFmpeg $FFMPEG_VERSION"
  # Sources are downloaded by build-mpv.ps1 (Windows certificate store).
  rm -rf ffmpeg && mkdir ffmpeg && tar -xf "ffmpeg-$FFMPEG_VERSION.tar.xz" -C ffmpeg --strip-components=1
  cd ffmpeg
  # All decoders, demuxers, parsers, bitstream and audio/video filters stay.
  # Encoders/muxers only for screenshots, previews and S/PDIF passthrough.
  ./configure --prefix="$PREFIX" --target-os=mingw32 --arch=x86_64 \
    --enable-static --disable-shared --disable-programs --disable-doc --disable-debug \
    --disable-network --disable-devices --disable-vulkan \
    --disable-encoders --enable-encoder=png,mjpeg,bmp,ac3,eac3 \
    --disable-muxers --enable-muxer=image2,image2pipe,spdif,null \
    --enable-libdav1d --enable-libzvbi --enable-libopenmpt --enable-libjxl --enable-libwebp \
    --enable-libxml2 --enable-zlib --enable-bzlib --enable-lzma --enable-iconv \
    --enable-d3d11va --enable-dxva2 \
    --pkg-config-flags=--static --extra-cflags="$FLAGS" --extra-libs="-lstdc++"
  make -j"$JOBS"
  make install
  cd ..
fi

echo "==> mpv $MPV_VERSION"
# shaderc, spirv-cross and the Vulkan loader exist only as DLLs in MSYS2;
# everything else links statically (prefer_static) and they ship beside libmpv.
rm -rf mpv && mkdir mpv && tar -xf "mpv-$MPV_VERSION.tar.gz" -C mpv --strip-components=1
cd mpv
# Only pass options this mpv revision knows (names change between versions).
OPTFILE=meson.options; [ -f "$OPTFILE" ] || OPTFILE=meson_options.txt
WANT="lua=disabled javascript=disabled libarchive=disabled libbluray=disabled dvdnav=disabled
cdda=disabled rubberband=disabled vapoursynth=disabled sdl2-audio=disabled sdl2-video=disabled
sdl2-gamepad=disabled openal=disabled vulkan=disabled gl=disabled egl-angle=disabled
d3d11=enabled shaderc=enabled spirv-cross=enabled jpeg=enabled lcms2=enabled zimg=enabled
uchardet=enabled libcurl=disabled cplayer=false libmpv=true"
OPTS=()
for o in $WANT; do
  if grep -q "option('${o%%=*}'" "$OPTFILE"; then OPTS+=("-D$o"); else echo "  (option ${o%%=*} not in this mpv)"; fi
done
CFLAGS="$FLAGS" LDFLAGS="-static-libgcc -static-libstdc++" \
meson setup build --buildtype=release --wrap-mode=nodownload -Ddefault_library=shared -Dprefer_static=true "${OPTS[@]}"
meson compile -C build
cd ..

echo "==> Collect"
rm -rf "$OUT" && mkdir -p "$OUT/include/mpv"
DLL=$(find mpv/build -maxdepth 1 -name 'libmpv-2.dll' | head -n1)
cp "$DLL" "$OUT/libmpv-2.dll"
cp mpv/include/mpv/*.h "$OUT/include/mpv/"
strip --strip-unneeded "$OUT/libmpv-2.dll"
# Any library that could not be linked statically is shipped next to it.
ntldd -R "$OUT/libmpv-2.dll" | grep -i 'ucrt64' | awk '{print $3}' | sed 's#\\#/#g' | while read -r dep; do
  [ -n "$dep" ] && cp "$(cygpath -u "$dep")" "$OUT/" && echo "  dependency: $(basename "$dep")"
done
ls -la "$OUT"
echo "ffmpeg $FFMPEG_VERSION, mpv $MPV_VERSION" > "$OUT/SOURCE.txt"
