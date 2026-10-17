# Third-party software

Fero Player is licensed under the GNU General Public License v3.0 or later (see `LICENSE`).
The Windows release bundles the components below. Their source code is available from the
listed projects; `tools/build-mpv.sh`, `tools/build-mpv.ps1` and `tools/build-qt-static.ps1`
download and build the exact versions that are shipped.

| Component | Version in the release | License | Source |
|---|---|---|---|
| mpv (libmpv) | git 3186d369f9 | GPL-2.0-or-later (built with GPL features) | https://github.com/mpv-player/mpv |
| FFmpeg | 9.0.2 | LGPL-2.1-or-later | https://ffmpeg.org |
| Qt (QtBase, QtSvg, statically linked) | 6.8.3 | LGPL-3.0 | https://download.qt.io/archive/qt/6.8/6.8.3/submodules/ |
| libplacebo | MSYS2 package at build time | LGPL-2.1-or-later | https://code.videolan.org/videolan/libplacebo |
| libass | MSYS2 package at build time | ISC | https://github.com/libass/libass |
| FreeType | MSYS2 package at build time | FreeType License (FTL) | https://freetype.org |
| HarfBuzz | MSYS2 package at build time | MIT | https://github.com/harfbuzz/harfbuzz |
| FriBidi | MSYS2 package at build time | LGPL-2.1-or-later | https://github.com/fribidi/fribidi |
| Little CMS | MSYS2 package at build time | MIT | https://github.com/mm2/Little-CMS |
| libjpeg-turbo | MSYS2 package at build time | IJG / BSD-3-Clause / Zlib | https://libjpeg-turbo.org |
| zimg | MSYS2 package at build time | WTFPL | https://github.com/sekrit-twc/zimg |
| uchardet | MSYS2 package at build time | MPL-1.1 / GPL-2.0 / LGPL-2.1 | https://www.freedesktop.org/wiki/Software/uchardet/ |
| dav1d | MSYS2 package at build time | BSD-2-Clause | https://code.videolan.org/videolan/dav1d |
| libopenmpt | MSYS2 package at build time | BSD-3-Clause | https://lib.openmpt.org |
| libjxl | MSYS2 package at build time | BSD-3-Clause | https://github.com/libjxl/libjxl |
| libwebp | MSYS2 package at build time | BSD-3-Clause | https://chromium.googlesource.com/webm/libwebp |
| libxml2 | MSYS2 package at build time | MIT | https://gitlab.gnome.org/GNOME/libxml2 |
| zvbi | MSYS2 package at build time | LGPL-2.0-or-later | https://github.com/zapping-vbi/zvbi |
| zlib, bzip2, xz, libiconv | MSYS2 packages at build time | Zlib, bzip2, 0BSD / public domain, LGPL-2.1-or-later | https://www.msys2.org |
| shaderc | MSYS2 package at build time | Apache-2.0 | https://github.com/google/shaderc |
| SPIRV-Cross | MSYS2 package at build time | Apache-2.0 | https://github.com/KhronosGroup/SPIRV-Cross |
| GCC runtime (libgcc, libstdc++) | GCC 16 (MSYS2) | GPL-3.0 with GCC Runtime Library Exception | https://gcc.gnu.org |
| MinGW-w64 runtime (winpthreads) | MSYS2 package at build time | MIT / ZPL-2.1 | https://www.mingw-w64.org |

Qt is linked statically under the LGPL-3.0. Because the complete source code of Fero Player
and the build scripts are published here, you can rebuild and relink the program with a
modified Qt.

The demo clips in `tools/demo` and `tools/fero-test.webm` were generated for this project
and are covered by the project license.
