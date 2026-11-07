# Run by CMake after every build (see CMakeLists.txt): collects everything Lexia needs
# to run on another PC into one folder, and zips it.
#
# Inputs (passed with -D):
#   EXE_FILE    the built lexia.exe
#   EXE_DIR     the folder it was built into (the DLLs vcpkg / ONNX Runtime copied are there)
#   DEST        the folder to create, e.g. <build>/package/Lexia
#   MODELS_SRC  the OCR models folder
#   ESPEAK_DATA_SRC  espeak-ng-data, the phonemiser data Piper needs
#   VOICES_SRC  the voices folder: voices.json plus the Voice files it lists
#   README_SRC  README.txt to include
#   CRT_DLLS    Visual C++ runtime DLLs, separated by "|" (may be empty)
#   CONFIG      the build configuration; only a Release build is packaged
#   SOURCE_DIR  the source tree: LICENSE and licenses\ (the third-party notices, see notices.cmake)
#   SIGN        ON to Authenticode-sign Lexia's own binaries (cmake/sign.cmake, which also uses
#               SIGNTOOL, SIGN_TIMESTAMP_URL, SIGN_THUMBPRINT, SIGN_PFX); empty or OFF otherwise
#   ORT_DIR, VCPKG_SHARE, PIPER_SRC, ESPEAK_SRC
#               where "@include" lines in licenses\components\*.txt find licence files
cmake_minimum_required(VERSION 3.21)
include("${CMAKE_CURRENT_LIST_DIR}/notices.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/sign.cmake")

if(NOT CONFIG STREQUAL "Release")
    return()
endif()

# Start clean every time, so the folder never holds stale files.
file(REMOVE_RECURSE "${DEST}")
file(MAKE_DIRECTORY "${DEST}")

file(COPY "${EXE_FILE}" DESTINATION "${DEST}")

file(GLOB DLLS "${EXE_DIR}/*.dll")
foreach(f IN LISTS DLLS)
    file(COPY "${f}" DESTINATION "${DEST}")
endforeach()

# The Visual C++ runtime, so the other PC doesn't need to install anything.
if(CRT_DLLS)
    string(REPLACE "|" ";" CRT_LIST "${CRT_DLLS}")
    foreach(f IN LISTS CRT_LIST)
        if(EXISTS "${f}")
            file(COPY "${f}" DESTINATION "${DEST}")
        endif()
    endforeach()
else()
    message(WARNING "Lexia package: Visual C++ runtime DLLs not found; the other PC may need "
                    "the Microsoft Visual C++ Redistributable (x64) installed.")
endif()

if(EXISTS "${MODELS_SRC}")
    # Only the models of the Languages Lexia offers (`offered` in src/language.cpp, ADR-0007): the
    # Cyrillic recogniser stays out until Bulgarian is offered again.
    file(COPY "${MODELS_SRC}" DESTINATION "${DEST}" PATTERN "*cyrillic*" EXCLUDE)
else()
    message(WARNING "Lexia package: models folder not found at ${MODELS_SRC}; the package will not work without it.")
endif()


# Speech: espeak-ng's data, and the Voices voices.json marks "ship": true. A Voice that is not
# cleared for redistribution (ship false) is never copied, even if it sits in voices\.
if(EXISTS "${ESPEAK_DATA_SRC}")
    file(COPY "${ESPEAK_DATA_SRC}" DESTINATION "${DEST}")
else()
    message(WARNING "Lexia package: espeak-ng-data not found at ${ESPEAK_DATA_SRC}; Lexia will not speak.")
endif()

if(EXISTS "${VOICES_SRC}/voices.json")
    file(READ "${VOICES_SRC}/voices.json" VOICES_JSON)
    file(MAKE_DIRECTORY "${DEST}/voices")
    file(COPY "${VOICES_SRC}/voices.json" DESTINATION "${DEST}/voices")
    string(JSON VOICE_COUNT LENGTH "${VOICES_JSON}" voices)
    math(EXPR VOICE_LAST "${VOICE_COUNT} - 1")
    set(SHIPPED_VOICES "")
    foreach(i RANGE ${VOICE_LAST})
        string(JSON VOICE_SHIP GET "${VOICES_JSON}" voices ${i} ship)
        string(JSON VOICE_NAME GET "${VOICES_JSON}" voices ${i} name)
        if(VOICE_SHIP)
            list(APPEND SHIPPED_VOICES "${VOICE_NAME}")
            foreach(ext ".onnx" ".onnx.json")
                if(EXISTS "${VOICES_SRC}/${VOICE_NAME}${ext}")
                    file(COPY "${VOICES_SRC}/${VOICE_NAME}${ext}" DESTINATION "${DEST}/voices")
                else()
                    message(WARNING "Lexia package: ${VOICE_NAME}${ext} not found in ${VOICES_SRC}; run scripts/fetch_deps.ps1.")
                endif()
            endforeach()
        endif()
    endforeach()
else()
    message(WARNING "Lexia package: ${VOICES_SRC}/voices.json not found; Lexia will not speak.")
endif()

if(EXISTS "${README_SRC}")
    file(COPY "${README_SRC}" DESTINATION "${DEST}")
endif()

# Licences: Lexia's own (GPL-3.0-or-later) and the third-party notices. This stops the build if a
# shipped Voice, .exe or .dll has no entry in licenses\components\.
file(COPY "${SOURCE_DIR}/LICENSE" DESTINATION "${DEST}")
lexia_generate_notices(
    SOURCE_DIR "${SOURCE_DIR}"
    DEST "${DEST}"
    OUT "${DEST}/THIRD-PARTY-NOTICES.txt"
    SHIPPED_VOICES ${SHIPPED_VOICES}
    TOKENS "ORT_DIR=${ORT_DIR}" "VCPKG_SHARE=${VCPKG_SHARE}" "PIPER_SRC=${PIPER_SRC}" "ESPEAK_SRC=${ESPEAK_SRC}")
# The same two files next to the dev exe (build\Release), so the About page's notices link works
# there too: App::OpenNotices opens THIRD-PARTY-NOTICES.txt beside lexia.exe.
file(COPY "${DEST}/LICENSE" "${DEST}/THIRD-PARTY-NOTICES.txt" DESTINATION "${EXE_DIR}")

# Signing (off by default, -DLEXIA_SIGN=ON): only what Lexia builds itself. Third-party DLLs (ONNX
# Runtime, OpenCV, the Visual C++ runtime ...) keep their vendors' signatures or stay as they are.
# H4 hook: this is the last step that changes the files. The zip below, and the installer (H4),
# must come after it and pack the files of this folder; H4 signs the installer .exe itself with
# lexia_sign_files() too.
if(SIGN)
    lexia_sign_files("${DEST}/lexia.exe" "${DEST}/piper.dll")
endif()

# A zip that is easy to send.
get_filename_component(PARENT "${DEST}" DIRECTORY)
get_filename_component(NAME "${DEST}" NAME)
file(REMOVE "${PARENT}/${NAME}.zip")
execute_process(
    COMMAND ${CMAKE_COMMAND} -E tar cf "${NAME}.zip" --format=zip "${NAME}"
    WORKING_DIRECTORY "${PARENT}"
    RESULT_VARIABLE ZIP_RESULT)
if(NOT ZIP_RESULT EQUAL 0)
    message(WARNING "Lexia package: could not create ${PARENT}/${NAME}.zip")
endif()

message(STATUS "Lexia package ready: ${DEST}")
message(STATUS "                     ${PARENT}/${NAME}.zip")
