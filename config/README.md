# Configuration

Everything a run needs is in `alignment.conf`. Nothing else in the tree
should have to be edited to change where the data comes from, which module
is used, how big each training job is, or how many steps the run takes.

## Quick start

```sh
eval `alienv load -w /home/alice/Software/v20230501/sw O2/latest`

./config/alignctl.sh ui        # edit everything in one window
./config/alignctl.sh doctor    # check this machine has what the run needs
./runAll_alignment.sh MYTAG    # go
```

## Command line

| Command | Does |
|---|---|
| `alignctl.sh show` | Print every setting, plus what follows from it |
| `alignctl.sh get KEY` | Print one value |
| `alignctl.sh set KEY=VALUE ...` | Change settings, then re-validate |
| `alignctl.sh validate` | Check types and relationships between values |
| `alignctl.sh doctor` | Check paths, archives and data files on this machine |
| `alignctl.sh generate` | Write the generated headers |
| `alignctl.sh inspect [ARCHIVE]` | What a module archive has and holds, as `MP_NAME=value` lines |
| `alignctl.sh keys` | List every key |
| `alignctl.sh ui` | Open the ROOT window |

## The window

`alignctl.sh ui` opens a ROOT GUI with five tabs — Data, Module, Module
tuning, Schedule, Environment — and a log pane. Every path field has a
**Browse…** button that opens a directory tree like `TBrowser`'s, so nothing
has to be typed by hand. On the Data tab, **List files** shows every file in
the input directory matching the pattern with a tick box, and the ticked set
becomes `DATA_FILES`. Picking an archive on the Module tab reads it through
`alignctl.sh inspect`: the panel says what generation it is and what it can
do, and knobs the archive does not have are greyed out and saved as `keep`.

The window never writes the configuration file itself. **Save + Generate**
shells out to `alignctl.sh`, so the file format and the validation rules live
in one place and the GUI cannot drift away from the command line.

Requires ROOT with GUI support and an X display. Over ssh, `ssh -X` is enough.
Where there is no display, `alignctl.sh set` does the same job.

## How a setting reaches the code

Three of the settings do not live in shell variables, so they are pushed out
by generating the files that hold them:

| Setting | Lands in | Written when |
|---|---|---|
| `DATA_INPUT_DIR`, `DATA_FILES`, `DATA_FILES_PER_BATCH` | `RUN/MasterDataScript/DataSetConfig.h`, included by `DataRandomMerge.C` | `generate`, and at the start of every run |
| `TRACK_SCHEMA` | `RUN/MasterDataScript/DataSchema.h`, included by `DataInputStructure.h` | `generate`, and at the start of every run |
| `MODULE_EVENTS`, `MODULE_EPOCHS`, `MODULE_JPARALLEL`, `MODULE_CORES` | `YMLPParallel.h` inside each worker's unpacked module | After the module is unpacked, per worker |
| `GEOM_BACKEND`, `MODULE_LEARNING_METHOD`, `MODULE_DULEVEL`, `MODULE_LAYERS` and the `[tuning]` keys | Rewritten in place in the worker's unpacked `Ymlp/inc/DetectorConstant.h`, `Ymlp/src/YMultiLayerPerceptron.cxx`, `run_train_circle.C` and `Ymlp/inc/YDetectorGeometry.h` | After the module is unpacked, per worker |
| everything else | read directly by `runAll_alignment.sh` | — |

The module's job size is a set of `#define`s inside the frozen archive.
Overwriting `YMLPParallel.h` in the worker's unpacked copy is the only way to
change it without rebuilding the tarball, so that is what the driver does —
the archive in `MODULE/` is never modified.

Generated files are not committed; `generate` recreates them.

## Module knobs and generations

The rest of the module's configuration — learning method, detector-unit
level, layer mask, cut windows, learning-rate constants, vertex thresholds —
is a set of defines and file-scope constants in the source tree. The keys in
the `[module]` and `[tuning]` groups patch them the same way, in the worker's
copy, through `config/modulepatch.sh` (the same rewrites the 2026 tree's own
run console does). Every one of them defaults to `keep`, which leaves the
archive's value alone, so a configuration that sets none of them runs a 2024
or 2025 archive exactly as before.

What a given archive has is discovered by reading it, not assumed from its
name: `alignctl.sh inspect` prints the report, `doctor` checks every knob
that is set against it, and the driver checks again before unpacking. A knob
the module lacks is refused; `MODULE_DULEVEL=5` and `MODULE_LAYERS=3,4,5,6`
are accepted on older trees as what they do anyway. Learning methods are
checked against the module's `Train()` switch: every generation declares
eight, implements three or four, and an unimplemented one would train nothing.

| Knob | 2024 | 2025 | 2026 |
|---|---|---|---|
| input tree (`TRACK_SCHEMA`) | 2024 | 2025 | 2025 (`auto` reads it from the archive) |
| `MODULE_LEARNING_METHOD` | kStochastic, kBatch, kBatchDetectorUnitUser, kSteepestDescent | same | kStochastic, kBatch, kSteepestDescent |
| `MODULE_DULEVEL` | — (per chip; 5 accepted) | — | -1..5 |
| `MODULE_LAYERS` | — (outer barrel; 3,4,5,6 accepted) | — | any subset of 0..6 |
| `GEOM_BACKEND` | o2 only | o2 only | o2 or cache |
| adaptive vertex (`MODULE_QUALITY_*`, `MODULE_MAX_BAD_TRACKS`, `MODULE_VERTEX_DERIVATIVES`) | — | yes | yes |
| eta and cut windows | yes | yes | yes |

Two things about the 2026 tree are worth knowing. It ships with the cache
geometry backend selected, so under the default `GEOM_BACKEND=o2` the driver
writes the O2 guard into every worker's `YDetectorGeometry.h`. And `cache`
needs `geometry/its2_geom.root` inside the archive — the 2026 repository
ignores that directory in git, so an archive packed from a checkout has to be
built with `tools/export_geometry_cache.C` run first. O2 is loaded for the
run either way: the manager's own `DataSplit.C` and `WeightsMerge.C` need it.

The archive must unpack to a directory named like the archive
(`tar czf NAME.tgz NAME`); `doctor` and the driver check.

Each worker directory gets a `module_patch_manifest.txt` saying what the
archive is and what was patched into that copy.

## What was left alone

`runAll_alignment_example.sh` is unchanged. It is the record of the original
run and the reference the `original-v20251024` tag points at.
`runAll_alignment.sh` is the configured equivalent.

`DataRandomMerge.C` carries the one edit this required: it includes
`DataSetConfig.h` instead of holding the directory and the file list inline.
The commented history of earlier campaigns is still there.

`DataInputStructure.h` and `DataSplit.C` carry the per-track `charge` field
behind `#if ALIGN_TRACK_HAS_CHARGE`, which `DataSchema.h` sets from
`TRACK_SCHEMA`. The field is the only difference between the 2024 and the
2025 input tree (the 2026 module reads the 2025 tree), and choosing wrong is
not a compile error -- it writes a tree the module misreads -- so `doctor`
reads `Ymlp/inc/DataInputStructure.h` out of the selected module archive and
fails if the two disagree. `TRACK_SCHEMA=auto` takes the answer from the
archive instead of asking.

## Adding a setting

1. Add the key to `alignment.conf` with a comment saying what it is for.
2. Add it to `AC_KEYS` in `alignconf.sh` as `NAME:TYPE:GROUP`. A key that
   older configuration files may lack goes into `AC_OPTIONAL` with its
   default as well; `set` appends such a key when it is absent.
3. If it needs a range or a relationship to another value, add the check to
   `ac_validate`; if it must exist on disk, add it to `ac_doctor`. A knob
   that only some module generations have is gated in `mp_apply`
   (`modulepatch.sh`) on what `mp_inspect` found.
4. Add a widget in `RUN/ConfigUI/ConfigUI.C` — one line in `LoadAll` to read
   it and one in `OnSave` to write it.
