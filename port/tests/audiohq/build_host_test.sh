#!/bin/sh
# Build and link the host audio test on Linux (g++ or clang++). Usage:
#   sh build_host_test.sh [outdir]      (default ./build)
set -e
here=$(cd "$(dirname "$0")" && pwd)
root="$here/../../hal/sdat"
out=${1:-"$here/build"}
mkdir -p "$out"
CXX=${CXX:-g++}
$CXX -std=c++14 -O2 -I"$root" \
  -c "$root/vorbis_impl.cpp" -w -o "$out/vorbis_impl.o"
$CXX -std=c++14 -O2 -Wall -Wno-unused-function -I"$here/shim" -I"$root" \
  "$here/host_audio_test.cpp" "$root/sdat.cpp" "$root/mixer.cpp" "$root/sseq.cpp" \
  "$root/sd_audio.cpp" "$out/vorbis_impl.o" -o "$out/host_audio_test" -lm
echo "built $out/host_audio_test"
