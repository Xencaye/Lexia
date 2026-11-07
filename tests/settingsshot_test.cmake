# ctest settingsshot_smoke: lexia_settingsshot draws the gallery page, light and dark, off-screen (WARP is
# enough, no GPU or window needed), and each PNG must exist, be a real picture and not one plain colour.
#   cmake -DSHOT_EXE=<lexia_settingsshot.exe> -DWORK_DIR=<folder> -P settingsshot_test.cmake
# "Not plain" is checked two ways: the tool counts the distinct colours of the pixels it drew and prints
# `colours=N`, and the PNG must be larger than a plain 800 x 600 picture would compress to (a few KB).
# The light and the dark picture must also differ, so a theme switch that does nothing fails.
cmake_minimum_required(VERSION 3.21)

if(NOT EXISTS "${SHOT_EXE}")
    message(FATAL_ERROR "lexia_settingsshot is not built: ${SHOT_EXE}")
endif()
file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

foreach(theme IN ITEMS light dark)
    set(png "${WORK_DIR}/gallery-${theme}.png")
    execute_process(
        COMMAND "${SHOT_EXE}" --page gallery --theme ${theme} --dpi 96 "${png}"
        RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "${theme}: lexia_settingsshot failed (${result}):\n${out}${err}")
    endif()
    if(NOT EXISTS "${png}")
        message(FATAL_ERROR "${theme}: no PNG was written to ${png}")
    endif()
    file(SIZE "${png}" bytes)
    if(bytes LESS 15000)
        message(FATAL_ERROR "${theme}: the PNG is only ${bytes} bytes, so it is (nearly) one plain colour")
    endif()
    if(NOT out MATCHES "colours=([0-9]+)")
        message(FATAL_ERROR "${theme}: the tool did not report its colours:\n${out}")
    endif()
    if(CMAKE_MATCH_1 LESS 16)
        message(FATAL_ERROR "${theme}: only ${CMAKE_MATCH_1} distinct colours, the picture is plain")
    endif()
    # The PNG signature, so a text file with the right name does not pass.
    file(READ "${png}" head LIMIT 8 HEX)
    if(NOT head STREQUAL "89504e470d0a1a0a")
        message(FATAL_ERROR "${theme}: ${png} is not a PNG file")
    endif()
    file(SHA256 "${png}" hash-${theme})
endforeach()

if(hash-light STREQUAL hash-dark)
    message(FATAL_ERROR "the light and the dark picture are identical")
endif()
message(STATUS "settingsshot_smoke: ok")
