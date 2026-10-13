// The one translation unit that holds stb_vorbis's code (public domain / MIT,
// see port/third_party/stb_vorbis/LICENSE-NOTE.txt). Memory decoding only:
// no stdio, no push API. Used by sd_audio.cpp for sound-mod .ogg files.
#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#include "../../third_party/stb_vorbis/stb_vorbis.c"
