# Run by CMake as the patch step of libpiper_external (see CMakeLists.txt).
#   -DSRC=<libpiper_external source dir>
#
# piper1-gpl v1.8.0 calls Ort::Session::GetOutputNames(), which ONNX Runtime added in 1.22.
# Lexia ships 1.20.1, so the call is replaced by GetOutputNameAllocated(), which 1.20 has.
# When Lexia moves to ONNX Runtime 1.22 or later, delete this patch.
file(READ "${SRC}/libpiper/src/piper.cpp" text)
string(FIND "${text}" "GetOutputNames()" at)
if(at EQUAL -1)
    string(FIND "${text}" "GetOutputNameAllocated" patched)
    if(patched EQUAL -1)
        message(FATAL_ERROR "patch_libpiper.cmake: piper.cpp has neither GetOutputNames() nor our patch; piper1-gpl changed?")
    endif()
    return()  # already patched
endif()
string(REPLACE
    "std::vector<std::string> output_names_strs = synth->session->GetOutputNames();"
    "std::vector<std::string> output_names_strs;
  {
    Ort::AllocatorWithDefaultOptions names_allocator;
    for (size_t i = 0; i < synth->session->GetOutputCount(); ++i) {
      output_names_strs.emplace_back(
          synth->session->GetOutputNameAllocated(i, names_allocator).get());
    }
  }"
    text "${text}")
string(FIND "${text}" "GetOutputNames()" still)
if(NOT still EQUAL -1)
    message(FATAL_ERROR "patch_libpiper.cmake: GetOutputNames() not replaced; piper1-gpl changed?")
endif()
file(WRITE "${SRC}/libpiper/src/piper.cpp" "${text}")
