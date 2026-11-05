# The Settings window's pages (ui/) as the app loads them: everything except ui/dev/ (the browser
# preview and the tests) and ui/README.md, and index.html without its DEV ONLY block (the one
# <script src="dev/dev_host.js"> line, see ui/README.md). The source keeps the block, so the browser
# preview and the ui_bridge test still work.
#
#   lexia_copy_ui(<source ui folder> <destination folder>)
# The destination is deleted first, so it never holds stale files. Stops with an error if
# index.html has no marked block, or still names dev_host afterwards.
function(lexia_copy_ui SRC DEST)
    file(REMOVE_RECURSE "${DEST}")
    file(MAKE_DIRECTORY "${DEST}")
    file(GLOB children RELATIVE "${SRC}" "${SRC}/*")
    foreach(child IN LISTS children)
        if(child STREQUAL "dev" OR child STREQUAL "README.md")
            continue()
        endif()
        file(COPY "${SRC}/${child}" DESTINATION "${DEST}")
    endforeach()

    set(index "${DEST}/index.html")
    file(READ "${index}" html)
    string(FIND "${html}" "<!-- DEV ONLY" begin)
    string(FIND "${html}" "<!-- END DEV ONLY -->" end)
    if(begin EQUAL -1 OR end EQUAL -1 OR end LESS begin)
        message(FATAL_ERROR "ui/index.html has no <!-- DEV ONLY ... <!-- END DEV ONLY --> block to strip.")
    endif()
    string(LENGTH "<!-- END DEV ONLY -->" markerLength)
    math(EXPR afterEnd "${end} + ${markerLength}")
    string(SUBSTRING "${html}" 0 ${begin} head)
    string(SUBSTRING "${html}" ${afterEnd} -1 tail)
    string(REGEX REPLACE "^\r?\n" "" tail "${tail}")
    file(WRITE "${index}" "${head}${tail}")

    file(READ "${index}" check)
    if(check MATCHES "dev_host" OR check MATCHES "DEV ONLY")
        message(FATAL_ERROR "The packaged index.html still refers to dev/.")
    endif()
endfunction()
