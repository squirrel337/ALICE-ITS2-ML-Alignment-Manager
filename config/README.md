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
| `alignctl.sh keys` | List every key |
| `alignctl.sh ui` | Open the ROOT window |

## The window

`alignctl.sh ui` opens a ROOT GUI with four tabs — Data, Module, Schedule,
Environment — and a log pane. Every path field has a **Browse…** button that
opens a directory tree like `TBrowser`'s, so nothing has to be typed by hand.
On the Data tab, **List files** shows every file in the input directory
matching the pattern with a tick box, and the ticked set becomes `DATA_FILES`.

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
| `DATA_INPUT_DIR`, `DATA_FILES`, `DATA_FILES_PER_BATCH` | `RUN/MasterDataScript*/DataSetConfig.h`, included by `DataRandomMerge.C` | `generate`, and at the start of every run |
| `MODULE_EVENTS`, `MODULE_EPOCHS`, `MODULE_JPARALLEL`, `MODULE_CORES` | `YMLPParallel.h` inside each worker's unpacked module | After the module is unpacked, per worker |
| everything else | read directly by `runAll_alignment.sh` | — |

The module's job size is a set of `#define`s inside the frozen archive.
Overwriting `YMLPParallel.h` in the worker's unpacked copy is the only way to
change it without rebuilding the tarball, so that is what the driver does —
the archive in `MODULE/` is never modified.

Generated files are not committed; `generate` recreates them.

## What was left alone

`runAll_alignment_example.sh` is unchanged. It is the record of the original
run and the reference the `original-v20251024` tag points at.
`runAll_alignment.sh` is the configured equivalent.

`DataRandomMerge.C` carries the one edit this required: it includes
`DataSetConfig.h` instead of holding the directory and the file list inline.
The commented history of earlier campaigns is still there.

## Adding a setting

1. Add the key to `alignment.conf` with a comment saying what it is for.
2. Add it to `AC_KEYS` in `alignconf.sh` as `NAME:TYPE:GROUP`.
3. If it needs a range or a relationship to another value, add the check to
   `ac_validate`; if it must exist on disk, add it to `ac_doctor`.
4. Add a widget in `RUN/ConfigUI/ConfigUI.C` — one line in `LoadAll` to read
   it and one in `OnSave` to write it.
