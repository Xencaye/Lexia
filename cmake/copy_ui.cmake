# Run after every build of lexia.exe: puts the Settings window's pages next to it, as
# <exe folder>\ui, the way the package has them (see ui_files.cmake). So a build\Release\lexia.exe
# shows the same window as the packaged one.
#   -DSRC=<source ui folder>  -DDEST=<exe folder>/ui
cmake_minimum_required(VERSION 3.21)
include("${CMAKE_CURRENT_LIST_DIR}/ui_files.cmake")
lexia_copy_ui("${SRC}" "${DEST}")
