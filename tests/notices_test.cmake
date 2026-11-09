# ctest: the third-party notices step of the package (cmake/notices.cmake).
#   cmake -DSOURCE_DIR=.. -DWORK_DIR=.. -DORT_DIR=.. -DVCPKG_SHARE=.. -DPIPER_SRC=.. -DESPEAK_SRC=.. -P notices_test.cmake
# It runs itself once per case (a FATAL_ERROR ends a cmake -P run), so each case is a process.
cmake_minimum_required(VERSION 3.21)

if(NOT CASE)
    function(run_case case expect_ok expect_text)
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -DCASE=${case} "-DSOURCE_DIR=${SOURCE_DIR}" "-DWORK_DIR=${WORK_DIR}"
                "-DORT_DIR=${ORT_DIR}" "-DVCPKG_SHARE=${VCPKG_SHARE}" "-DPIPER_SRC=${PIPER_SRC}"
                "-DESPEAK_SRC=${ESPEAK_SRC}" -P "${CMAKE_CURRENT_LIST_FILE}"
            RESULT_VARIABLE result ERROR_VARIABLE err OUTPUT_VARIABLE out)
        if(expect_ok AND NOT result EQUAL 0)
            message(FATAL_ERROR "case ${case}: failed but should work:\n${err}${out}")
        elseif(NOT expect_ok AND result EQUAL 0)
            message(FATAL_ERROR "case ${case}: worked but should fail")
        endif()
        if(NOT expect_ok AND NOT err MATCHES "${expect_text}")
            message(FATAL_ERROR "case ${case}: failed, but not with '${expect_text}':\n${err}")
        endif()
    endfunction()
    run_case(ok TRUE "")
    run_case(voice-without-entry FALSE "no licence entry")
    run_case(unknown-dll FALSE "no licenses/components/")
    return()
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/notices.cmake")

set(dest "${WORK_DIR}/${CASE}")
file(REMOVE_RECURSE "${dest}")
file(MAKE_DIRECTORY "${dest}")
foreach(f lexia.exe onnxruntime.dll onnxruntime_providers_shared.dll piper.dll opencv_core4.dll
        opencv_imgproc4.dll z.dll msvcp140.dll vcruntime140_1.dll)
    file(WRITE "${dest}/${f}" "")
endforeach()
set(voices en_GB-cori-medium en_US-mike-medium)
if(CASE STREQUAL "voice-without-entry")
    list(APPEND voices xx_XX-nolicence-medium)
elseif(CASE STREQUAL "unknown-dll")
    file(WRITE "${dest}/mystery.dll" "")
endif()

lexia_generate_notices(
    SOURCE_DIR "${SOURCE_DIR}" DEST "${dest}" OUT "${dest}/THIRD-PARTY-NOTICES.txt"
    SHIPPED_VOICES ${voices}
    TOKENS "ORT_DIR=${ORT_DIR}" "VCPKG_SHARE=${VCPKG_SHARE}" "PIPER_SRC=${PIPER_SRC}" "ESPEAK_SRC=${ESPEAK_SRC}")

file(READ "${dest}/THIRD-PARTY-NOTICES.txt" notices)
foreach(expected "ONNX Runtime" "OpenCV" "JSON for Modern C++" "PaddleOCR" "Piper (libpiper" "eSpeak NG" "zlib"
        "Piper Voice en_GB-cori-medium" "Piper Voice en_US-mike-medium" "--- Apache-2.0 ---" "--- MIT ---" "Permission is hereby granted")
    string(FIND "${notices}" "${expected}" found_at)
    if(found_at EQUAL -1)
        message(FATAL_ERROR "the notices lack '${expected}'")
    endif()
endforeach()
# Each shared licence text once, not once per component.
string(REGEX MATCHALL "--- Apache-2.0 ---" apache_headings "${notices}")
list(LENGTH apache_headings apache_count)
if(NOT apache_count EQUAL 1)
    message(FATAL_ERROR "the Apache-2.0 text is printed ${apache_count} times, not once")
endif()
if(NOT EXISTS "${dest}/licenses/onnxruntime-ThirdPartyNotices.txt")
    message(FATAL_ERROR "ONNX Runtime's own third-party notices were not shipped")
endif()
