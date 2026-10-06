# Plan E: Flash Size, Build and CI (review 6.1–6.4, S1–S3, 3.5b)

- [x] S2 + S3 in `include/lv_conf.h`: 88.2% → 84.0% (−78KB). Device: 5 boots PASS; visual check pending.
- [x] `check_secrets.py`: pre-build credential scan; passes on the code, blocked a planted key.
- [x] 6.3: upstream OFL texts + README licence table (licences read from each library).
- [x] Patches (ask-first files): 0003 platformio.ini (S1/3.5b, debug env, pinning), 0004 ci.yml, 0005 tag-only releases.
- [x] Patch 0003 test-built in a throwaway worktree (83.2%) and flashed: 3 boots PASS, no `appid` in the log.
