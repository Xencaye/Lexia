cmake_minimum_required(VERSION 3.21)

# The version: "version" in vcpkg.json is the one source of truth. It becomes the project version
# (Lexia_VERSION), then src/version.h (LEXIA_VERSION), the exe's version resource and the
# package README. Read before project() so project() can take it.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg.json" LEXIA_VCPKG_JSON)
string(JSON LEXIA_VERSION ERROR_VARIABLE LEXIA_VERSION_ERROR GET "${LEXIA_VCPKG_JSON}" version)
if(LEXIA_VERSION_ERROR OR NOT LEXIA_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "vcpkg.json needs a \"version\" of the form major.minor.patch (got '${LEXIA_VERSION}').")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg.json")

project(Lexia VERSION ${LEXIA_VERSION} LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(UNICODE _UNICODE)
# /utf-8: sources are UTF-8 whatever the system code page (cp1251 on a Bulgarian Windows).
# /W4 /permissive-: Lexia's own code builds with zero warnings at this level.
add_compile_options(/utf-8 /W4 /permissive-)

find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(SoundTouch CONFIG REQUIRED)
find_package(miniz CONFIG REQUIRED)
find_package(doctest CONFIG REQUIRED)
find_package(unofficial-webview2 CONFIG REQUIRED)  # the Settings window (D3); only lexia.exe links it

# ONNX Runtime: the prebuilt Windows zip, unpacked to <project>/onnxruntime
set(ORT_DIR "${CMAKE_SOURCE_DIR}/onnxruntime" CACHE PATH "Folder containing ONNX Runtime's include/ and lib/")
if(NOT EXISTS "${ORT_DIR}/include/onnxruntime_cxx_api.h")
    message(FATAL_ERROR "ONNX Runtime not found in ${ORT_DIR}. Run scripts\\fetch_deps.ps1 first.")
endif()

# onnxruntime.dll loads providers_shared only for non-CPU execution providers; it is linked
# here so $<TARGET_RUNTIME_DLLS> ships it next to the exe as before.
add_library(onnxruntime::providers_shared SHARED IMPORTED)
set_target_properties(onnxruntime::providers_shared PROPERTIES
    IMPORTED_LOCATION "${ORT_DIR}/lib/onnxruntime_providers_shared.dll"
    IMPORTED_IMPLIB "${ORT_DIR}/lib/onnxruntime_providers_shared.lib")

add_library(onnxruntime::onnxruntime SHARED IMPORTED)
set_target_properties(onnxruntime::onnxruntime PROPERTIES
    IMPORTED_LOCATION "${ORT_DIR}/lib/onnxruntime.dll"
    IMPORTED_IMPLIB "${ORT_DIR}/lib/onnxruntime.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${ORT_DIR}/include"
    INTERFACE_LINK_LIBRARIES onnxruntime::providers_shared)

# Piper (libpiper, GPL-3.0-or-later) with espeak-ng, built from the pinned commit below by
# piper1-gpl's own CMake against Lexia's ONNX Runtime. It clones espeak-ng at the commit pinned
# in libpiper's CMakeLists, so the first build needs the network. Result: piper.dll + piper.lib +
# piper.h and espeak-ng-data under <build>/lp_install. Always built as Release (a DLL with a
# C API; no CRT objects cross it). Short folder names: espeak-ng's build nests deeply and a
# Windows path over 260 characters fails in MSBuild.
include(ExternalProject)
set(LIBPIPER_INSTALL "${CMAKE_BINARY_DIR}/lp_install")
# The .pfx password (docs/signing.md) is for the package step only: the configure, build and
# install of piper1-gpl and espeak-ng, third-party code, run without it in their environment.
# Needs CMake 4.2; an older CMake passes it on.
set(LIBPIPER_ENV_ARGS "")
if(CMAKE_VERSION VERSION_GREATER_EQUAL 4.2)
    foreach(step IN ITEMS CONFIGURE BUILD INSTALL)
        list(APPEND LIBPIPER_ENV_ARGS ${step}_ENVIRONMENT_MODIFICATION LEXIA_SIGN_PFX_PASSWORD=unset:)
    endforeach()
endif()
ExternalProject_Add(libpiper_external
    GIT_REPOSITORY https://github.com/OHF-Voice/piper1-gpl.git
    GIT_TAG 639388b6317fc4731e91d53da42aea68fd4166ff  # v1.8.0; re-pin deliberately on update
    UPDATE_DISCONNECTED TRUE
    SOURCE_DIR "${CMAKE_BINARY_DIR}/lp_src"
    SOURCE_SUBDIR libpiper
    BINARY_DIR "${CMAKE_BINARY_DIR}/lp"
    INSTALL_DIR "${LIBPIPER_INSTALL}"
    PATCH_COMMAND ${CMAKE_COMMAND} "-DSRC=${CMAKE_BINARY_DIR}/lp_src" -P "${CMAKE_SOURCE_DIR}/cmake/patch_libpiper.cmake"
    CMAKE_ARGS
        "-DONNXRUNTIME_DIR=${ORT_DIR}"
        "-DCMAKE_INSTALL_PREFIX=${LIBPIPER_INSTALL}"
    BUILD_COMMAND ${CMAKE_COMMAND} --build "${CMAKE_BINARY_DIR}/lp" --config Release
    INSTALL_COMMAND ${CMAKE_COMMAND} --install "${CMAKE_BINARY_DIR}/lp" --config Release
    ${LIBPIPER_ENV_ARGS}
    BUILD_BYPRODUCTS "${LIBPIPER_INSTALL}/lib/piper.dll" "${LIBPIPER_INSTALL}/lib/piper.lib")

file(MAKE_DIRECTORY "${LIBPIPER_INSTALL}/include")  # an imported target's include dir must exist
add_library(piper::piper SHARED IMPORTED)
set_target_properties(piper::piper PROPERTIES
    IMPORTED_LOCATION "${LIBPIPER_INSTALL}/lib/piper.dll"
    IMPORTED_IMPLIB "${LIBPIPER_INSTALL}/lib/piper.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${LIBPIPER_INSTALL}/include")
set(ESPEAK_DATA_DIR "${LIBPIPER_INSTALL}/share/espeak-ng-data")
# Where the package step finds licence files for the third-party notices (cmake/notices.cmake)
set(LEXIA_VCPKG_SHARE "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share")
set(LEXIA_PIPER_SRC "${CMAKE_BINARY_DIR}/lp_src")
set(LEXIA_ESPEAK_SRC "${CMAKE_BINARY_DIR}/lp/espeak_ng/src/espeak_ng_external")  # cloned by libpiper

# lexia_core: everything that is not UI, so the exe, the tests and the CLI link the same code.
add_library(lexia_core STATIC
    src/cli.cpp
    src/command_line.cpp
    src/codepoint.cpp
    src/common.cpp
    src/control_bar_layout.cpp
    src/jobqueue.cpp
    src/key_edit.cpp
    src/ctc.cpp
    src/language.cpp
    src/layout.cpp
    src/log.cpp
    src/lookalike.cpp
    src/mouse_role.cpp
    src/native_ui/palette.cpp
    src/ocr.cpp
    src/recognize.cpp
    src/score.cpp
    src/startup.cpp
    src/stretch.cpp
    src/svg_recolour.cpp
    src/ui_host.cpp
    src/settings.cpp
    src/tray_menu.cpp
    src/tts/md5.cpp
    src/tts/speech_chunk.cpp
    src/tts/piper_voice.cpp
    src/tts/pitch_shift.cpp
    src/tts/preset_file.cpp
    src/tts/reading_queue.cpp
    src/tts/speech_key.cpp
    src/tts/stretch_speech.cpp
    src/tts/voice_controls.cpp
    src/tts/voice_catalogue.cpp
    src/tts/voice_download.cpp
    src/tts/voice_files.cpp
    src/tts/voice_library.cpp
    src/tts/voices.cpp
    src/tts/wav.cpp
    src/strings/strings.cpp
    src/strings/table_bg.cpp
    src/strings/table_en.cpp)

# version.h and lexia.rc are generated into the build folder (see the version note at the top).
set(LEXIA_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
configure_file(src/version.h.in "${LEXIA_GENERATED_DIR}/version.h" @ONLY)
configure_file(src/lexia.rc.in "${LEXIA_GENERATED_DIR}/lexia.rc" @ONLY)
configure_file(package/README.txt "${LEXIA_GENERATED_DIR}/README.txt" @ONLY)

target_include_directories(lexia_core PUBLIC src "${LEXIA_GENERATED_DIR}")
add_dependencies(lexia_core libpiper_external)  # piper.h exists only once libpiper is built

# opencv_core is public because ocr.h / recognize.h take a cv::Mat; ONNX Runtime, Piper and JSON
# stay private. Static-library link dependencies still reach the exe, so
# $<TARGET_RUNTIME_DLLS:lexia> below sees every DLL.
target_link_libraries(lexia_core
    PUBLIC opencv_core
    PRIVATE opencv_imgproc nlohmann_json::nlohmann_json SoundTouch::SoundTouch miniz::miniz onnxruntime::onnxruntime piper::piper)

# In a Debug build the OCR models are read from <project>/models. Release only looks in a
# "models" folder next to lexia.exe (see ResolveModelsDir in ocr.cpp), so the build
# machine's path is never baked into a shipped exe.
target_compile_definitions(lexia_core
    PUBLIC NOMINMAX
    PRIVATE $<$<CONFIG:Debug>:MODELS_DIR="${CMAKE_SOURCE_DIR}/models">)

# lexia_net: the network, which is Win32 (WinHTTP), so it is not in lexia_core. Only the Voice library
# uses it, and only when the user asks (ADR-0005); lexia_cli runs it today, the Voice tab (D5) will.
add_library(lexia_net STATIC src/net/winhttp_fetch.cpp)
target_link_libraries(lexia_net PUBLIC lexia_core PRIVATE winhttp)

# The app: overlay, screen capture, speech and the hotkey loop.
add_executable(lexia
    src/main.cpp
    src/capture.cpp
    src/overlay.cpp
    src/controlbar.cpp
    src/svg_icon.cpp
    src/speech.cpp
    src/app.cpp
    src/settings_watch.cpp
    src/settings_window.cpp
    src/win32_theme.cpp
    src/autostart.cpp
    src/elevation.cpp
    src/instance.cpp
    src/tray.cpp
    src/lexia.manifest
    src/voice_tab.cpp
    src/tts/waveout_sink.cpp
    "${LEXIA_GENERATED_DIR}/lexia.rc")

# A Windows (GUI) program: no console of its own (wWinMain). `lexia.exe --console` opens one for development.
set_target_properties(lexia PROPERTIES WIN32_EXECUTABLE TRUE)

# Only the OpenCV modules Lexia uses: $<TARGET_RUNTIME_DLLS> ships every linked module's DLL.
target_link_libraries(lexia PRIVATE
    lexia_core lexia_net
    opencv_core opencv_imgproc
    unofficial::webview2::webview2
    user32 gdi32 gdiplus ole32 uuid winmm dwmapi shcore advapi32 shell32 d2d1 d3d11 dxgi shlwapi)

# Speech data next to the dev exe too (the package step copies it again, selectively)
add_custom_command(TARGET lexia POST_BUILD
    COMMAND ${CMAKE_COMMAND} "-DDEST=$<TARGET_FILE_DIR:lexia>" "-DESPEAK_DATA_SRC=${ESPEAK_DATA_DIR}"
        "-DVOICES_SRC=${CMAKE_SOURCE_DIR}/voices" -P "${CMAKE_SOURCE_DIR}/cmake/copy_speech_data.cmake"
    VERBATIM)

# The Settings window's pages next to the dev exe too, as the package has them (cmake/ui_files.cmake)
add_custom_command(TARGET lexia POST_BUILD
    COMMAND ${CMAKE_COMMAND} "-DSRC=${CMAKE_SOURCE_DIR}/ui" "-DDEST=$<TARGET_FILE_DIR:lexia>/ui"
        -P "${CMAKE_SOURCE_DIR}/cmake/copy_ui.cmake"
    VERBATIM)

# Put ONNX Runtime's DLLs next to the exe (avoids picking up an older system copy)
add_custom_command(TARGET lexia POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:lexia> "$<TARGET_FILE_DIR:lexia>"
    COMMAND_EXPAND_LISTS)

# ----------------------------------------------------------------------- cli ---
# lexia_cli: OCR + layout on a saved Shot (see src/cli_main.cpp). A developer tool, so it is not
# packaged: it gets its own output folder, because the package step copies every DLL that sits
# next to lexia.exe and imgcodecs / libpng must not leak into the zip.
#   build/cli/<Config>/lexia_cli.exe
add_executable(lexia_cli src/cli_main.cpp)
target_link_libraries(lexia_cli PRIVATE lexia_core lexia_net opencv_core opencv_imgproc opencv_imgcodecs)
set_target_properties(lexia_cli PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/cli"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/cli/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/cli/Release"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/cli/RelWithDebInfo"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/cli/MinSizeRel")
# Models and Voices: beside the exe like the app, else these folders. Only the CLI knows the paths (R30: it
# is never in lexia_core or lexia.exe).
target_compile_definitions(lexia_cli PRIVATE LEXIA_DEV_MODELS_DIR="${CMAKE_SOURCE_DIR}/models"
    LEXIA_DEV_VOICES_DIR="${CMAKE_SOURCE_DIR}/voices" LEXIA_DEV_ESPEAK_DATA_DIR="${ESPEAK_DATA_DIR}")
add_custom_command(TARGET lexia_cli POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:lexia_cli> "$<TARGET_FILE_DIR:lexia_cli>"
    COMMAND_EXPAND_LISTS)

# lexia_barshot: a developer tool, not packaged. It makes a Control bar window, draws it in a state the
# command line names and writes the picture the bar draws (shadows and all) to a PNG, so the look can
# be compared with wireframe/control_bar_*.png without a Session. Own output folder, like lexia_cli.
#   build/barshot/<Config>/lexia_barshot.exe --size large|small --state idle|speaking|paused|analysing
#       [--hover pause|skip|stop|help|size] [--theme light|dark] [--dpi 96] out.png
add_executable(lexia_barshot src/barshot_main.cpp src/controlbar.cpp src/svg_icon.cpp src/win32_theme.cpp)
target_link_libraries(lexia_barshot PRIVATE lexia_core
    user32 gdi32 gdiplus ole32 uuid advapi32 shell32 d2d1 d3d11 dxgi shlwapi)
set_target_properties(lexia_barshot PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/barshot"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/barshot/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/barshot/Release"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/barshot/RelWithDebInfo"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/barshot/MinSizeRel")
add_custom_command(TARGET lexia_barshot POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:lexia_barshot> "$<TARGET_FILE_DIR:lexia_barshot>"
    COMMAND_EXPAND_LISTS)

# ------------------------------------------------------------------- tests ---
# Unit tests (doctest) for the pure logic in lexia_core. Run with
#   ctest --test-dir build -C Release --output-on-failure
enable_testing()
add_executable(lexia_tests
    tests/test_main.cpp
    tests/announcement_test.cpp
    tests/cli_test.cpp
    tests/codepoint_test.cpp
    tests/command_line_test.cpp
    tests/control_bar_layout_test.cpp
    tests/hotkeys_test.cpp
    tests/jobqueue_test.cpp
    tests/key_edit_test.cpp
    tests/ctc_test.cpp
    tests/language_test.cpp
    tests/log_test.cpp
    tests/layout_test.cpp
    tests/lookalike_test.cpp
    tests/md5_test.cpp
    tests/mouse_role_test.cpp
    tests/palette_test.cpp
    tests/preset_file_test.cpp
    tests/score_test.cpp
    tests/startup_test.cpp
    tests/stretch_speech_test.cpp
    tests/stretch_test.cpp
    tests/settings_test.cpp
    tests/reading_queue_test.cpp
    tests/speech_key_test.cpp
    tests/tray_menu_test.cpp
    tests/tts_test.cpp
    tests/strings_test.cpp
    tests/svg_recolour_test.cpp
    tests/ui_host_test.cpp
    tests/voice_catalogue_test.cpp
    tests/voice_download_test.cpp
    tests/voice_library_test.cpp
    tests/voice_controls_test.cpp)
target_link_libraries(lexia_tests PRIVATE lexia_core nlohmann_json::nlohmann_json miniz::miniz doctest::doctest)

# lexia_core pulls in the OpenCV / ONNX Runtime DLLs; the test exe needs them next to it.
# Declared before doctest_discover_tests, whose POST_BUILD step runs the exe to list the cases.
add_custom_command(TARGET lexia_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:lexia_tests> "$<TARGET_FILE_DIR:lexia_tests>"
    COMMAND_EXPAND_LISTS)

include(doctest)
doctest_discover_tests(lexia_tests)

# The package's licence notices: a shipped Voice or DLL with no entry must fail (cmake/notices.cmake).
add_test(NAME package_notices
    COMMAND ${CMAKE_COMMAND} "-DSOURCE_DIR=${CMAKE_SOURCE_DIR}" "-DWORK_DIR=${CMAKE_BINARY_DIR}/notices_test"
        "-DORT_DIR=${ORT_DIR}" "-DVCPKG_SHARE=${LEXIA_VCPKG_SHARE}"
        "-DPIPER_SRC=${LEXIA_PIPER_SRC}"
        "-DESPEAK_SRC=${LEXIA_ESPEAK_SRC}"
        -P "${CMAKE_SOURCE_DIR}/tests/notices_test.cmake")

# The Settings window page (ui/): its bridge tests, in headless Edge (tests/ui_bridge_test.cmake).
# It fails, not skips, when Edge is missing; CI's windows-latest has Edge.
add_test(NAME ui_bridge
    COMMAND ${CMAKE_COMMAND} "-DUI_DIR=${CMAKE_SOURCE_DIR}/ui" "-DWORK_DIR=${CMAKE_BINARY_DIR}/ui_bridge_test"
        -P "${CMAKE_SOURCE_DIR}/tests/ui_bridge_test.cmake")

# --------------------------------------------------------- warnings as errors ---
# CI turns this ON, so a warning fails the build (the zero-warning rule at /W4). It is OFF by
# default so a newer compiler's new warning does not stop a developer's local build. Only
# Lexia's own targets get /WX, not the vcpkg ports.
option(LEXIA_WARNINGS_AS_ERRORS "Treat compiler warnings in Lexia's own targets as errors" OFF)
if(LEXIA_WARNINGS_AS_ERRORS)
    foreach(target lexia_core lexia_net lexia lexia_cli lexia_tests)
        target_compile_options(${target} PRIVATE /WX)
    endforeach()
    # Linker warnings too (e.g. LNK4098 conflicting default libs). lexia_core is a static
    # library, so its option goes to the librarian.
    foreach(target lexia lexia_cli lexia_tests)
        target_link_options(${target} PRIVATE /WX)
    endforeach()
    set_target_properties(lexia_core lexia_net PROPERTIES STATIC_LIBRARY_OPTIONS /WX)
endif()

# ------------------------------------------------------------------ package ---
# After every Release build, assemble a folder (and a .zip of it) that runs on another PC:
#   <build>/package/Lexia/        lexia.exe + all DLLs + models/ + README.txt
#   <build>/package/Lexia.zip
# Turn it off with  -DLEXIA_PACKAGE=OFF.
# -DLEXIA_SIGN=ON also Authenticode-signs lexia.exe and piper.dll in the package folder (see
# docs/signing.md and cmake/sign.cmake); it is OFF by default, so the default build and CI never sign.
# The OCR models folder the package copies; a git worktree can point it at the real folder
# (its own models/ may be a junction, which the copy refuses).
set(LEXIA_MODELS_DIR "${CMAKE_SOURCE_DIR}/models" CACHE PATH "Folder with the OCR models to package")
option(LEXIA_PACKAGE "Create a ready-to-send package/Lexia folder after each Release build" ON)
option(LEXIA_SIGN "Authenticode-sign lexia.exe and Lexia's own DLLs in the package (needs signtool and a certificate, see docs/signing.md)" OFF)
set(LEXIA_SIGN_TIMESTAMP_URL "http://timestamp.digicert.com" CACHE STRING "RFC 3161 timestamp server used when LEXIA_SIGN is ON")
# The certificate is never stored in the CMake cache in clear text. A thumbprint (not a secret) or a
# .pfx path may be given here or as environment variables of the same name; the .pfx password only as
# the environment variable LEXIA_SIGN_PFX_PASSWORD, read at build time.
set(LEXIA_SIGN_THUMBPRINT "" CACHE STRING "SHA-1 thumbprint of the code-signing certificate in your certificate store (or env LEXIA_SIGN_THUMBPRINT)")
set(LEXIA_SIGN_PFX "" CACHE FILEPATH "Path to a .pfx code-signing certificate (or env LEXIA_SIGN_PFX); the password is env LEXIA_SIGN_PFX_PASSWORD")
set(LEXIA_SIGNTOOL "")
if(LEXIA_SIGN)
    if(NOT LEXIA_PACKAGE)
        message(FATAL_ERROR "LEXIA_SIGN=ON signs the package folder, so it needs LEXIA_PACKAGE=ON.")
    endif()
    # signtool.exe comes with the Windows SDK: next to the resource compiler, or in the newest SDK bin folder.
    get_filename_component(LEXIA_RC_DIR "${CMAKE_RC_COMPILER}" DIRECTORY)
    set(LEXIA_PF86 "$ENV{ProgramFiles\(x86\)}")
    file(GLOB LEXIA_SDK_BIN_DIRS "${LEXIA_PF86}/Windows Kits/10/bin/10.*/x64")
    list(SORT LEXIA_SDK_BIN_DIRS COMPARE NATURAL ORDER DESCENDING)
    find_program(LEXIA_SIGNTOOL_EXE NAMES signtool.exe
        HINTS "${LEXIA_RC_DIR}" ${LEXIA_SDK_BIN_DIRS} "${LEXIA_PF86}/Windows Kits/10/bin/x64")
    if(NOT LEXIA_SIGNTOOL_EXE)
        message(FATAL_ERROR "LEXIA_SIGN=ON but signtool.exe was not found. Install the Windows SDK "
                            "(\"Windows SDK Signing Tools for Desktop Apps\" in the Visual Studio installer), "
                            "or set LEXIA_SIGNTOOL_EXE to its full path.")
    endif()
    set(LEXIA_SIGNTOOL "${LEXIA_SIGNTOOL_EXE}")
    message(STATUS "Lexia signing is ON: ${LEXIA_SIGNTOOL}, timestamp ${LEXIA_SIGN_TIMESTAMP_URL}")
endif()
if(LEXIA_PACKAGE)
    include(InstallRequiredSystemLibraries)  # locates the Visual C++ runtime DLLs
    string(REPLACE ";" "|" LEXIA_CRT_DLLS "${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS}")

    add_custom_command(TARGET lexia POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            "-DCONFIG=$<CONFIG>"
            "-DEXE_FILE=$<TARGET_FILE:lexia>"
            "-DEXE_DIR=$<TARGET_FILE_DIR:lexia>"
            "-DDEST=${CMAKE_BINARY_DIR}/package/Lexia"
            "-DMODELS_SRC=${LEXIA_MODELS_DIR}"
            "-DESPEAK_DATA_SRC=${ESPEAK_DATA_DIR}"
            "-DVOICES_SRC=${CMAKE_SOURCE_DIR}/voices"
            "-DREADME_SRC=${LEXIA_GENERATED_DIR}/README.txt"
            "-DCRT_DLLS=${LEXIA_CRT_DLLS}"
            "-DSOURCE_DIR=${CMAKE_SOURCE_DIR}"
            "-DORT_DIR=${ORT_DIR}"
            "-DVCPKG_SHARE=${LEXIA_VCPKG_SHARE}"
            "-DPIPER_SRC=${LEXIA_PIPER_SRC}"
            "-DESPEAK_SRC=${LEXIA_ESPEAK_SRC}"
            "-DSIGN=${LEXIA_SIGN}"
            "-DSIGNTOOL=${LEXIA_SIGNTOOL}"
            "-DSIGN_TIMESTAMP_URL=${LEXIA_SIGN_TIMESTAMP_URL}"
            "-DSIGN_THUMBPRINT=${LEXIA_SIGN_THUMBPRINT}"
            "-DSIGN_PFX=${LEXIA_SIGN_PFX}"
            -P "${CMAKE_SOURCE_DIR}/cmake/package.cmake"
        VERBATIM)
endif()

# ---------------------------------------------------------------- installer ---
# -DLEXIA_INSTALLER=ON also builds <build>/package/Lexia-<version>-setup.exe from the package folder,
# with Inno Setup 6 (task H4, installer/lexia.iss, cmake/installer.cmake). OFF by default, so a build
# without Inno Setup (and CI) works as before.
option(LEXIA_INSTALLER "Build the Inno Setup installer after the package (needs Inno Setup 6)" OFF)
if(LEXIA_INSTALLER)
    if(NOT LEXIA_PACKAGE)
        message(FATAL_ERROR "LEXIA_INSTALLER=ON installs the package folder, so it needs LEXIA_PACKAGE=ON.")
    endif()
    set(LEXIA_PF86 "$ENV{ProgramFiles\(x86\)}")
    find_program(LEXIA_ISCC NAMES ISCC.exe
        HINTS "${LEXIA_PF86}/Inno Setup 6" "$ENV{ProgramFiles}/Inno Setup 6" "$ENV{LOCALAPPDATA}/Programs/Inno Setup 6")
    if(NOT LEXIA_ISCC)
        message(FATAL_ERROR "LEXIA_INSTALLER=ON but ISCC.exe (Inno Setup 6) was not found. Install it "
                            "(winget install JRSoftware.InnoSetup), or set LEXIA_ISCC to its full path.")
    endif()
    message(STATUS "Lexia installer is ON: ${LEXIA_ISCC}")
    # A second POST_BUILD command on lexia: it runs after the package command above, in this order.
    add_custom_command(TARGET lexia POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            "-DCONFIG=$<CONFIG>"
            "-DISCC=${LEXIA_ISCC}"
            "-DISS=${CMAKE_SOURCE_DIR}/installer/lexia.iss"
            "-DVERSION=${LEXIA_VERSION}"
            "-DPACKAGE_DIR=${CMAKE_BINARY_DIR}/package/Lexia"
            "-DOUTPUT_DIR=${CMAKE_BINARY_DIR}/package"
            -P "${CMAKE_SOURCE_DIR}/cmake/installer.cmake"
        VERBATIM)
endif()
