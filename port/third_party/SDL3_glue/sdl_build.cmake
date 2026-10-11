# SDL3 static, gamepad only, for the controller backend (run hunt8, lanes SDLPAD1 / SDLFIX1).
# Included by port/CMakeLists.txt (end block) and by the shim-regeneration probe project
# (probe/CMakeLists.txt in the lane record), so both build the exact same SDL3-static.
# Expects SDL_ROOT (the vendored tree) and the glue dir next to this file.
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
foreach(_sdlpad1_off IN ITEMS SDL_AUDIO SDL_VIDEO SDL_GPU SDL_RENDER SDL_CAMERA
        SDL_HAPTIC SDL_POWER SDL_DIALOG SDL_TRAY SDL_TESTS SDL_TEST_LIBRARY
        SDL_INSTALL SDL_UNINSTALL SDL_EXAMPLES SDL_DIRECTX SDL_VULKAN SDL_OPENGL
        SDL_OPENGLES SDL_OFFSCREEN SDL_DUMMYVIDEO SDL_DUMMYAUDIO SDL_DISKAUDIO)
    set(${_sdlpad1_off} OFF CACHE BOOL "" FORCE)
endforeach()
add_subdirectory("${SDL_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/sdl3" EXCLUDE_FROM_ALL)
# SDL names the SDK import libraries as its link interface; the shim replaces them, so the
# exe links only the static data libraries SDL's GUIDs live in.
set_property(TARGET SDL3-static PROPERTY INTERFACE_LINK_LIBRARIES "")
# SDLFIX1 / F2: drop SDL's process spawning (CreateProcessW, pipes), open-URL (ShellExecuteW)
# and Steam user storage (steam_api loader). SDL's gamepad code does not call any of them;
# sdl_f2_stubs.c supplies failing versions of the entry points the rest of SDL still names.
get_target_property(_sdl_srcs SDL3-static SOURCES)
list(FILTER _sdl_srcs EXCLUDE REGEX "src/process/windows/|src/misc/windows/SDL_sysurl\.c|src/storage/steam/")
set_property(TARGET SDL3-static PROPERTY SOURCES ${_sdl_srcs})
target_sources(SDL3-static PRIVATE "${CMAKE_CURRENT_LIST_DIR}/sdl_f2_stubs.c")
