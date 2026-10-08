# The sample plugin. Build it 32-bit: the game is a 32-bit program.
#   cmake -S . -B build -A Win32            (Visual Studio generator)
#   cmake --build build --config Release
# or from an x86 developer prompt (vcvars32): cmake -S . -B build -G Ninja
cmake_minimum_required(VERSION 3.15)
project(sm64ds_sample_mod C)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "The game is 32-bit: configure with -A Win32 or an x86 toolchain.")
endif()
add_library(sample_mod SHARED sample_plugin.c)
# the plugin has no dependency on the game's runtime: link the CRT statically
set_property(TARGET sample_mod PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded)
target_compile_options(sample_mod PRIVATE /W3)
if(DEFINED SAMPLE_TEST_FAULT_FRAME)
    target_compile_definitions(sample_mod PRIVATE SAMPLE_TEST_FAULT_FRAME=${SAMPLE_TEST_FAULT_FRAME})
endif()
