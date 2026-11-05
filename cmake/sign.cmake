# Authenticode signing with signtool.exe, for the package step (cmake/package.cmake). Off unless
# CMake was configured with -DLEXIA_SIGN=ON; see docs/signing.md.
#
# The certificate is never in the repo or in the CMake cache in clear text:
#   LEXIA_SIGN_THUMBPRINT  (env or cache) SHA-1 thumbprint of a code-signing certificate in the
#                          user's certificate store (signtool /sha1; the thumbprint is not a secret)
#   LEXIA_SIGN_PFX         (env or cache) path to a .pfx file; its password is read ONLY from the
#                          environment variable LEXIA_SIGN_PFX_PASSWORD, at build time
# The thumbprint wins when both are set.
#
# Inputs from the caller (-D): SIGNTOOL, SIGN_TIMESTAMP_URL, optional SIGN_THUMBPRINT / SIGN_PFX
# (the cache values; the environment is used when they are empty).

# lexia_sign_files(<file>...): signs each file in place (SHA-256 file digest, RFC 3161 timestamp
# with a SHA-256 digest). Stops the build with a message when no certificate is configured or
# signtool fails. Sign only files Lexia built; third-party DLLs keep their vendors' signatures.
function(lexia_sign_files)
    set(thumbprint "${SIGN_THUMBPRINT}")
    if(NOT thumbprint)
        set(thumbprint "$ENV{LEXIA_SIGN_THUMBPRINT}")
    endif()
    set(pfx "${SIGN_PFX}")
    if(NOT pfx)
        set(pfx "$ENV{LEXIA_SIGN_PFX}")
    endif()

    if(thumbprint)
        set(cert_args /sha1 "${thumbprint}")
        set(cert_note "certificate ${thumbprint} from the certificate store")
    elseif(pfx)
        if(NOT EXISTS "${pfx}")
            message(FATAL_ERROR "Lexia signing: the .pfx file '${pfx}' does not exist.")
        endif()
        set(cert_args /f "${pfx}")
        if(DEFINED ENV{LEXIA_SIGN_PFX_PASSWORD} AND NOT "$ENV{LEXIA_SIGN_PFX_PASSWORD}" STREQUAL "")
            list(APPEND cert_args /p "$ENV{LEXIA_SIGN_PFX_PASSWORD}")
        endif()
        set(cert_note "certificate file ${pfx}")
    else()
        message(FATAL_ERROR
            "Lexia signing (LEXIA_SIGN=ON) has no certificate. Set one of:\n"
            "  LEXIA_SIGN_THUMBPRINT  thumbprint of a code-signing certificate in your certificate store\n"
            "  LEXIA_SIGN_PFX         path to a .pfx file (and its password in the environment variable LEXIA_SIGN_PFX_PASSWORD)\n"
            "as an environment variable, or -D on the cmake configure line. See docs/signing.md.")
    endif()

    foreach(f IN LISTS ARGN)
        message(STATUS "Lexia signing: ${f} with ${cert_note}")
        execute_process(
            COMMAND "${SIGNTOOL}" sign /fd sha256 /tr "${SIGN_TIMESTAMP_URL}" /td sha256
                    ${cert_args} "${f}"
            RESULT_VARIABLE sign_result
            OUTPUT_VARIABLE sign_out
            ERROR_VARIABLE sign_err)
        if(NOT sign_result EQUAL 0)
            # Never echo the command line: it holds the .pfx password.
            message(FATAL_ERROR "Lexia signing: signtool failed on ${f} (exit ${sign_result}):\n${sign_out}${sign_err}")
        endif()
    endforeach()
endfunction()
