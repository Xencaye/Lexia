# ctest entry ui_bridge: runs the Settings window page's bridge tests (ui/dev/tests.js) in headless
# Microsoft Edge and fails unless the page says PASS in its title. Edge is needed: if it cannot be
# found the test FAILS with a message (it never passes by skipping). windows-latest has Edge.
#   cmake -DUI_DIR=<ui folder> -DWORK_DIR=<scratch folder> -P ui_bridge_test.cmake
# LEXIA_EDGE (an environment variable) names msedge.exe when it is somewhere else.
# --allow-file-access-from-files: the icons and pictures of the pages are CSS masks, and a file:// page may
# load a mask only with that switch (the tests do not look at them, but the page logs a CORS error without it).
cmake_minimum_required(VERSION 3.20)

if(NOT UI_DIR OR NOT EXISTS "${UI_DIR}/index.html")
    message(FATAL_ERROR "ui_bridge: UI_DIR is not the ui folder: '${UI_DIR}'")
endif()

# ---- find Edge ---
set(_edge "")
if(DEFINED ENV{LEXIA_EDGE} AND EXISTS "$ENV{LEXIA_EDGE}")
    set(_edge "$ENV{LEXIA_EDGE}")
endif()
if(NOT _edge)
    set(_dirs "")
    foreach(_var "ProgramFiles(x86)" "ProgramFiles" "LOCALAPPDATA")
        if(DEFINED ENV{${_var}})
            file(TO_CMAKE_PATH "$ENV{${_var}}" _base)
            list(APPEND _dirs "${_base}/Microsoft/Edge/Application")
        endif()
    endforeach()
    find_program(_found_edge NAMES msedge msedge.exe HINTS ${_dirs})
    if(_found_edge)
        set(_edge "${_found_edge}")
    endif()
endif()
if(NOT _edge)
    message(FATAL_ERROR "ui_bridge: Microsoft Edge (msedge.exe) was not found, so the page tests cannot run. "
                        "Install Edge or set LEXIA_EDGE to msedge.exe.")
endif()

# ---- run the page ---
string(RANDOM LENGTH 8 _id)
set(_profile "${WORK_DIR}/profile-${_id}")  # a fresh profile: a running Edge would take the request over
file(MAKE_DIRECTORY "${WORK_DIR}")
file(TO_NATIVE_PATH "${_profile}" _profile_native)
set(_url "file:///${UI_DIR}/index.html?test")
string(REPLACE "\\" "/" _url "${_url}")
string(REPLACE " " "%20" _url "${_url}")

execute_process(
    COMMAND "${_edge}" --headless --disable-gpu --allow-file-access-from-files "--user-data-dir=${_profile_native}"
            --virtual-time-budget=15000 --dump-dom "${_url}"
    OUTPUT_VARIABLE _dom
    ERROR_VARIABLE _err
    RESULT_VARIABLE _code
    TIMEOUT 180)
file(REMOVE_RECURSE "${_profile}")

if(NOT _code EQUAL 0 AND NOT _dom)
    message(FATAL_ERROR "ui_bridge: Edge failed (${_code}) and wrote no page.\n${_err}")
endif()

set(_list "")
if(_dom MATCHES "<pre id=\"test-result\">([^<]*)</pre>")
    set(_list "${CMAKE_MATCH_1}")
endif()
if(_dom MATCHES "<title>(PASS [0-9]+)</title>")
    message(STATUS "ui_bridge: ${CMAKE_MATCH_1}")
    message(STATUS "${_list}")
else()
    message(FATAL_ERROR "ui_bridge: the page tests did not pass.\n${_list}\n--- the page Edge dumped ---\n${_dom}")
endif()
