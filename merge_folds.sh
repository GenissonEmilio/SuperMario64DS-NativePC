#!/bin/bash
# Batch 4 merge loop: --no-ff each fold at pinned head, stop on first conflict.
cd "$(dirname "$0")"
START=${1:-0}
FOLDS=(
"c4051f5da78 ov100-fish"
"6668fbb7f2f ov096-tor"
"33b1789fcdf ov064-bbl"
"cf9dce882de ov022-london"
"a396048f3bb ov060-kpa3bg"
"92b495afb5a ov098-wbm"
"8abf52b8f67 ov045-agar"
"868e146dc92 ov036-tikuwa"
"0947ec44b4c ov022-seesaw"
"a06e011e42e ov029-wcobj02"
"44489a5acf3 ov025-dgr"
"e90132e1dca ov033-ttfuta"
"2ab6ea2a663 ov047-kaitendai"
"db0de417baa ov026-polelift"
"740224ea1aa ov029-wcobj05"
"31f646aedef ov047-kuruma"
"1b45f43c978 ov025-brock"
"94cb171695e ov026-suikomi"
"4ecac5485be ov032-tdfuta"
"7649cfe964a ov033-ttwater"
"fc1df0e6907 ov045-gura"
"b95884b539a ov029-wcobj04"
"df3ed15d4da ov036-hane"
"29a071c45dd ov032-tdwater"
"7751c52a269 ov035-mecha11"
"2249f1584bd ov081-iceblock"
"0d5e7c9fdcc ov036-buranko"
"ff5bea3beed ov064-hakidasi"
"9570ea88efe ov029-wcmizu"
"5fa7c58c957 ov017-kswater"
"c4027430a93 ov065-mecha03"
"1737fe7bc89 ov012-c0water"
"82bc9bf64cd ov043-ukishima"
"f09d957cf52 ov064-ring"
"45ba275b598 ov013-clockhuriko"
"bd31d9c4615 ov015-bklift"
"04200f70213 ov014-shutter"
"764911f5261 ov022-fallblock"
"ab0a57582f5 ov010-c1hikari"
"0fc873115e4 ov065-mecha09"
"f8610838ae5 ov015-botaosi"
"61e87d98033 ov009-mcmetalnet"
"f666f8eb1dd ov022-maruta"
"7f5a5fa4d2e ov009-mcflag"
"c0d92262a27 ov060-fring"
"2de67fd3082 ov091-linelift2"
"d786f17e9da ov100-pathlift"
"1e803c99ab9 ov015-rotebar"
"f7989683756 ov062-rflag"
"6ccb8589bc8 ov006-curling"
"48c8a1caeca ov045-nobiru"
"01cb2ca7736 ov065-mecha04"
"8a18cd437a1 ov071-eybm"
"05f77277e19 ov030-hmbskt"
"81ff8ebd6de ov073-ewbice"
"8e67284ba75 ov006-curling2"
"4aa94653684 ov047-kurumajiku"
"96687d2b2f6 ov010-c1peach"
"422637b9256 ov085-mipkey"
"a083bace7d5 ov070-krpa"
"24a50c91043 ov006-pachinko"
"da13a6d30b4 ov070-kpfr"
"93d2aa14630 ov006-pachinko2"
"7b8fb79d9a2 ov002-coin"
"5cfb9ee2a87 ov002-1uplogo"
"3a540d4e22e ov002-itemtag"
"91b5c1b7b7a ov006-espanimset"
"86cad1994ff ov006-memory"
)
for i in "${!FOLDS[@]}"; do
  if [ "$i" -lt "$START" ]; then continue; fi
  sha=$(echo "${FOLDS[$i]}" | cut -d' ' -f1)
  name=$(echo "${FOLDS[$i]}" | cut -d' ' -f2)
  echo "=== [$i] merging $name @ $sha"
  if git merge --no-ff -m "Merge branch 'sinit/$name' into batch/sinit-1010-4" "$sha"; then
    echo "    OK $name"
  else
    echo "    conflict in $name — trying auto-resolve"
    python resolve_baseline.py > /tmp/resolver_out.txt 2>&1
    cat /tmp/resolver_out.txt
    if grep -q "MANUAL" /tmp/resolver_out.txt; then
      echo "    MANUAL files at index $i ($name) — resolve then resume: bash merge_folds.sh $i"
      exit 1
    fi
    left=$(git diff --name-only --diff-filter=U)
    if [ -n "$left" ]; then
      echo "    STILL CONFLICTED: $left (index $i, $name)"
      exit 1
    fi
    git commit --no-edit -q && echo "    OK $name (auto-resolved)"
  fi
done
echo "=== ALL MERGES COMPLETE ==="
