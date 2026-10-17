cmake_minimum_required(VERSION 3.21)
project(FeroPlayer VERSION 1.0.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Svg)

# libmpv is loaded at run time (LoadLibrary) so the window appears before the
# 100 MB engine is mapped; only its public headers are needed here.
set(MPV_DIR "${CMAKE_SOURCE_DIR}/.deps/mpv" CACHE PATH "libmpv development package")
if(NOT EXISTS "${MPV_DIR}/include/mpv/client.h")
    message(FATAL_ERROR "libmpv headers not found in ${MPV_DIR}. Run Setup.cmd first.")
endif()

add_executable(FeroPlayer WIN32
    src/main.cpp
    src/theme.cpp src/theme.h
    src/icons.cpp src/icons.h
    src/widgets.cpp src/widgets.h
    src/medialist.cpp src/medialist.h
    src/catalog.cpp src/catalog.h
    src/mpvplayer.cpp src/mpvplayer.h
    src/playerwindow.cpp src/playerwindow.h
    src/playerwindow_chrome.cpp
    src/playerwindow_menus.cpp
    src/selftest.cpp src/selftest.h
    src/thumbnails.cpp src/thumbnails.h
    src/i18n.cpp src/i18n.h
    src/audiowarmup.cpp src/audiowarmup.h
    res/fero.qrc
    res/app.rc
)
target_include_directories(FeroPlayer PRIVATE "${MPV_DIR}/include" src res)
target_compile_definitions(FeroPlayer PRIVATE
    FERO_VERSION="${PROJECT_VERSION}" UNICODE _UNICODE NOMINMAX)
target_link_libraries(FeroPlayer PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Svg dwmapi psapi ole32 shell32 uuid)
option(FERO_STATIC "Link a static Qt and the MinGW runtime into FeroPlayer.exe" OFF)
if(FERO_STATIC)
    # Only the plug-ins Fero needs: Windows platform, JPEG/ICO/SVG images, SVG icons.
    qt_import_plugins(FeroPlayer
        INCLUDE_BY_TYPE platforms Qt6::QWindowsIntegrationPlugin
        INCLUDE_BY_TYPE imageformats Qt6::QJpegPlugin Qt6::QICOPlugin Qt6::QSvgPlugin
        INCLUDE_BY_TYPE iconengines Qt6::QSvgIconPlugin
        EXCLUDE_BY_TYPE styles
        EXCLUDE_BY_TYPE generic)
    target_link_options(FeroPlayer PRIVATE -static -static-libgcc -static-libstdc++)
endif()
if(MINGW)
    # Smaller, faster binary; Qt itself already ships the MinGW runtime DLLs.
    target_compile_options(FeroPlayer PRIVATE -O2 -Wall -Wextra -ffunction-sections -fdata-sections)
    target_link_options(FeroPlayer PRIVATE -s -Wl,--gc-sections)
endif()
