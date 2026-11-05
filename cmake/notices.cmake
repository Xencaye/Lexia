# Builds THIRD-PARTY-NOTICES.txt for the package from the per-component files in licenses/.
# Used by package.cmake (and tested by tests/notices_test.cmake). How to add a component:
# docs/third-party.md.
#
#   licenses/header.txt           the top of the file
#   licenses/components/*.txt     one file per component, in file-name order; the first line
#                                 is its title (it goes in the contents list too)
#   licenses/texts/<id>.txt       a licence text that several components share
#
# A component file is plain text. A line that starts with "@" is a directive:
#   @include <file>          the file's text goes here as it is. {NAME} in the path is replaced by
#                            the TOKENS value NAME=... (e.g. {VCPKG_SHARE}, {ESPEAK_SRC}); {SOURCE_DIR}
#                            is always the source tree
#   @licence-text <id>       "the full text is under LICENCE TEXTS" here, and licenses/texts/<id>.txt
#                            is added to the end of the file once, however many components use it
#   @ship <file> [as <name>] the file is too big to repeat: it is copied to <package>/licenses/
#                            (as <name> if given) and this line says so
#   @covers <name> [<name>]  not printed: the exe / DLL file names (wildcards * ok) in the package
#                            that this component is the licence entry for
# A file named voice-<Voice name>.txt is the entry for that Piper Voice and is used only when
# voices.json ships the Voice ("ship": true). The build fails if a shipped Voice has no such file,
# or if an .exe / .dll in the package is covered by no component.

# lexia_generate_notices(
#     SOURCE_DIR <repo>  DEST <package folder>  OUT <notices file>
#     SHIPPED_VOICES <voice name>...  TOKENS <NAME=value>...)
function(lexia_generate_notices)
    cmake_parse_arguments(A "" "SOURCE_DIR;DEST;OUT" "SHIPPED_VOICES;TOKENS" ${ARGN})
    list(APPEND A_TOKENS "SOURCE_DIR=${A_SOURCE_DIR}")  # {SOURCE_DIR} in an @include path

    # A shipped Voice with no licence entry is an error, whatever else is in the folder.
    foreach(voice IN LISTS A_SHIPPED_VOICES)
        if(NOT EXISTS "${A_SOURCE_DIR}/licenses/components/voice-${voice}.txt")
            message(FATAL_ERROR "Lexia notices: the Voice ${voice} ships (voices.json) but has no licence "
                "entry. Add licenses/components/voice-${voice}.txt (see docs/third-party.md).")
        endif()
    endforeach()

    file(GLOB component_files "${A_SOURCE_DIR}/licenses/components/*.txt")
    set(toc "")
    set(body "")
    set(text_ids "")
    set(covers "")
    foreach(file IN LISTS component_files)
        get_filename_component(name "${file}" NAME_WLE)
        if(name MATCHES "^voice-(.+)$")
            if(NOT CMAKE_MATCH_1 IN_LIST A_SHIPPED_VOICES)
                continue()  # a Voice that does not ship needs no notice
            endif()
        endif()

        file(READ "${file}" rest)
        string(REPLACE "\r\n" "\n" rest "${rest}")
        string(REGEX MATCH "^[^\n]*" title "${rest}")
        string(APPEND toc "  - ${title}\n")

        set(section "")
        # Walk the directives: copy the text before each one, then act on it.
        while(rest MATCHES "(^|\n)@([a-z-]+)[ \t]+([^\n]*)")
            set(directive "${CMAKE_MATCH_2}")
            set(argument "${CMAKE_MATCH_3}")
            string(FIND "${rest}" "${CMAKE_MATCH_0}" at)
            string(LENGTH "${CMAKE_MATCH_0}" matched_length)
            string(SUBSTRING "${rest}" 0 ${at} before)
            math(EXPR after_at "${at} + ${matched_length}")
            string(SUBSTRING "${rest}" ${after_at} -1 rest)
            string(APPEND section "${before}")

            string(STRIP "${argument}" argument)
            if(directive STREQUAL "covers")
                string(REPLACE " " ";" names "${argument}")
                list(APPEND covers ${names})
            elseif(directive STREQUAL "licence-text")
                if(NOT EXISTS "${A_SOURCE_DIR}/licenses/texts/${argument}.txt")
                    message(FATAL_ERROR "Lexia notices: ${name}: no licenses/texts/${argument}.txt")
                endif()
                list(APPEND text_ids "${argument}")
                string(APPEND section "\nThe full text of the ${argument} licence is under LICENCE TEXTS below.")
            elseif(directive STREQUAL "include" OR directive STREQUAL "ship")
                set(path "${argument}")
                set(shipped_name "")
                if(directive STREQUAL "ship" AND path MATCHES "^(.*[^ ]) +as +([^ ]+)$")
                    set(path "${CMAKE_MATCH_1}")
                    set(shipped_name "${CMAKE_MATCH_2}")
                endif()
                foreach(token IN LISTS A_TOKENS)
                    string(FIND "${token}" "=" eq)
                    string(SUBSTRING "${token}" 0 ${eq} key)
                    math(EXPR value_at "${eq} + 1")
                    string(SUBSTRING "${token}" ${value_at} -1 value)
                    string(REPLACE "{${key}}" "${value}" path "${path}")
                endforeach()
                if(NOT EXISTS "${path}")
                    message(FATAL_ERROR "Lexia notices: ${name}: @${directive} ${argument}: ${path} does not exist")
                endif()
                if(directive STREQUAL "include")
                    file(READ "${path}" included)
                    string(REPLACE "\r\n" "\n" included "${included}")
                    string(APPEND section "\n${included}")
                else()
                    if(shipped_name STREQUAL "")
                        get_filename_component(shipped_name "${path}" NAME)
                    endif()
                    file(MAKE_DIRECTORY "${A_DEST}/licenses")
                    file(COPY_FILE "${path}" "${A_DEST}/licenses/${shipped_name}")
                    string(APPEND section "\nThe full text is in the file licenses\\${shipped_name}, next to this file.")
                endif()
            else()
                message(FATAL_ERROR "Lexia notices: ${name}: unknown directive @${directive}")
            endif()
        endwhile()
        string(APPEND section "${rest}")
        string(STRIP "${section}" section)
        string(APPEND body "\n${section}\n\n${LEXIA_NOTICES_RULE}\n")
    endforeach()

    # Every program file in the package must have an entry that covers it.
    file(GLOB shipped_files RELATIVE "${A_DEST}" "${A_DEST}/*.exe" "${A_DEST}/*.dll")
    foreach(shipped IN LISTS shipped_files)
        string(TOLOWER "${shipped}" shipped_lower)
        set(is_covered FALSE)
        foreach(pattern IN LISTS covers)
            string(TOLOWER "${pattern}" pattern)
            string(REPLACE "." "\\." pattern "${pattern}")
            string(REPLACE "*" ".*" pattern "${pattern}")
            if(shipped_lower MATCHES "^${pattern}$")
                set(is_covered TRUE)
                break()
            endif()
        endforeach()
        if(NOT is_covered)
            message(FATAL_ERROR "Lexia notices: ${shipped} is in the package but no licenses/components/ "
                "file covers it. Add the component (see docs/third-party.md), with an @covers line.")
        endif()
    endforeach()

    file(READ "${A_SOURCE_DIR}/licenses/header.txt" header)
    string(REPLACE "\r\n" "\n" header "${header}")
    set(out "${header}\nCONTENTS\n\n${toc}  - LICENCE TEXTS (the licences several components share)\n")
    string(APPEND out "\n${LEXIA_NOTICES_RULE}${body}\n")

    string(APPEND out "LICENCE TEXTS\n=============\n")
    list(REMOVE_DUPLICATES text_ids)
    list(SORT text_ids)
    foreach(id IN LISTS text_ids)
        file(READ "${A_SOURCE_DIR}/licenses/texts/${id}.txt" text)
        string(REPLACE "\r\n" "\n" text "${text}")
        string(STRIP "${text}" text)
        string(APPEND out "\n--- ${id} ---\n\n${text}\n")
    endforeach()
    file(WRITE "${A_OUT}" "${out}")
endfunction()

string(REPEAT "=" 78 LEXIA_NOTICES_RULE)
