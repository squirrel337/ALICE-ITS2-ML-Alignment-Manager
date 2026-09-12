# Tests

Both tests work on a scratch copy and leave the repository alone. They need
real module archives, which are not committed; `tests/run_tests.sh` builds
everything else it needs.

| Test | Needs | Checks |
|---|---|---|
| `tests/run_tests.sh ARCHIVE.tgz ...` | bash, tar | For each archive: validate, inspect, generate, doctor; the driver's pre-flight, unpack and patch stage (with a stub `alienv` and `root`, so it stops before the first macro); that a worker tree differs from the archive only in `YMLPParallel.h` (and the geometry guard on a two-backend tree) when every knob is `keep`; that unsupported knobs, an emptied pT window, cache mode on an O2-only tree and a wrong `TRACK_SCHEMA` are refused before anything is unpacked. |
| `tests/gui_test.C` | ROOT with GUI support and a display (`xvfb-run` does) | Opens the real configuration window, reads a 2026-generation and a 2024-generation archive through it, sets knobs, saves, reads the file back through `alignctl.sh`, reloads, and checks that knobs the archive lacks are greyed out and saved as `keep`. |

```sh
tests/run_tests.sh MODULE/mod2024.tgz MODULE/mod2025.tgz MODULE/mod2026.tgz

cp -r . /tmp/mgr && cd /tmp/mgr          # the GUI test rewrites the configuration it is given
xvfb-run -a root -l -q 'tests/gui_test.C("config/alignment.conf","config/alignctl.sh","mod2026","mod2024")'
```

Neither test trains anything or needs O2. What they cannot cover is the
part that does: the data split, the training step and the merge, which run
only with O2 loaded on the machine the run is meant for.
