# Run by CMake after the package step when LEXIA_INSTALLER is ON (see CMakeLists.txt): builds the Inno
# Setup installer from the package folder (task H4, installer/lexia.iss).
#
# Inputs (passed with -D):
#   CONFIG       the build configuration; only a Release build has a package, so only it gets an installer
#   ISCC         the Inno Setup compiler (ISCC.exe)
#   ISS          installer/lexia.iss
#   VERSION      Lexia's version (from vcpkg.json)
#   PACKAGE_DIR  the package folder the installer installs (<build>/package/Lexia)
#   OUTPUT_DIR   where Lexia-<version>-setup.exe goes (<build>/package)
cmake_minimum_required(VERSION 3.21)

if(NOT CONFIG STREQUAL "Release")
    return()
endif()
if(NOT EXISTS "${PACKAGE_DIR}/lexia.exe")
    message(FATAL_ERROR "The installer needs the package folder first: ${PACKAGE_DIR} has no lexia.exe.")
endif()

# Inno Setup wants Windows paths in its /D defines.
file(TO_NATIVE_PATH "${PACKAGE_DIR}" source_native)
file(TO_NATIVE_PATH "${OUTPUT_DIR}" output_native)
execute_process(
    COMMAND "${ISCC}" /Q "/DAppVersion=${VERSION}" "/DSourceDir=${source_native}" "/DOutputDir=${output_native}" "${ISS}"
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "Inno Setup could not build the installer (exit ${rc}):\n${out}${err}")
endif()
message(STATUS "Lexia installer ready: ${OUTPUT_DIR}/Lexia-${VERSION}-setup.exe")
