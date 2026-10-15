# PLAYINT1 result
Branch cloud/playint1 (base origin/port/l7-fix9a). Design: cloud_tasks/PLAYINT1_DESIGN.md.
New files only: port/hal/party_hits_core.h, party_hits.h, party_hits.cpp; port/tools/party_hits_probe.cpp, party_hits_proof.py.
Host proof: `python3 port/tools/party_hits_proof.py` -> 87 rows, 0 failed (each refusal paired with a must-fail control).
`g++ -std=c++17 -fsyntax-only -I port/hal port/hal/party_hits.cpp` clean. Not built with MSVC, not run on a device.
Local agent: apply the hook lines in section 6 of the design, build, run `--live-rows` L1..L11.
