# Run by CMake after building lexia.exe (see CMakeLists.txt): puts espeak-ng-data and the Voices
# next to the dev exe, as in the package, so build/<Config>/lexia.exe can speak. file(COPY)
# skips files that are already there with the same timestamp, so repeat builds are quick.
#   -DDEST=<folder of lexia.exe>  -DESPEAK_DATA_SRC=<espeak-ng-data>  -DVOICES_SRC=<voices folder>
file(COPY "${ESPEAK_DATA_SRC}" DESTINATION "${DEST}")
if(EXISTS "${VOICES_SRC}/voices.json")
    file(COPY "${VOICES_SRC}/" DESTINATION "${DEST}/voices")
endif()
