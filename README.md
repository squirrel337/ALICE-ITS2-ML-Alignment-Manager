# ALICE ITS2 ML Alignment — Manager

Orchestration for a track-based alignment of the ALICE Inner Tracking System 2
in which the alignment parameters are learned rather than solved for.

The alignment itself is performed by a separate module: one small linear
network per sensor, trained simultaneously across all 24 120 ALPIDE chips
against a track-fit residual, with an analytic Jacobian re-expressing the
learned weights as six rigid-body parameters per sensor. **This repository is
the layer around that module** — it decides which data each training job sees,
runs many jobs in parallel, and reconciles their results into a single
alignment step.

The split matters for reading the code: the module knows nothing about
parallelism, and the manager knows nothing about the cost function.

---

## Repositories

| Repository | Contents | Enters a run as |
|---|---|---|
| **`ALICE-ITS2-ML-Alignment-Manager`** | Batch driver, data preparation, weight merging, configuration | Checked out and run |
| `ALICE-ITS2-ML-Alignment-2024` | Alignment module, vertex taken from reconstruction | `MODULE/<name>.tgz` |
| `ALICE-ITS2-ML-Alignment-2025` | Alignment module, vertex re-estimated from ITS tracks | `MODULE/<name>.tgz` |

The source trees are consumed as frozen tar archives, never edited in place.
Which one is in use is a single string in the configuration.

Both module repositories carry a full description of their internals in
`docs/workflow.html` — cost function, vertex constraint, optimisation loop,
and how weights become alignment parameters.

---

## What one run does

A run is `N_BATCHES` batches. Each batch advances the alignment by one step
and repeats four stages:

1. **Data preparation** — shuffle the available reconstructed track files,
   merge a random selection, and scatter the merged events uniformly into
   `N_WORKERS` disjoint slices.
2. **Parallel training** — launch one module copy per slice, each starting
   from the *same* previous alignment, and wait for all of them.
3. **Weight merge** — collect the last epoch's per-sensor weights from every
   worker and average them, unweighted, per sensor and per parameter.
4. **Parameter handoff** — pack the merged weights into the archive the next
   batch starts from.

Because the slices are uniform random partitions of one batch, the workers see
exchangeable samples and a plain mean is the natural combination. Statistics,
not model differences, is what separates them.

Default configuration as committed:

| | |
|---|---|
| Batches × workers | 10 × 8 = 80 module executions |
| Events per worker | 50 000 |
| Epochs per step | 5 |
| Steps | 901 → 910, continuing from the committed step-900 reference |
| Free parameters | 6 × 24 120 = 144 720 |

**Full pipeline description:** [`docs/workflow.html`](docs/workflow.html) —
stage by stage, with the file crossing every boundary and diagrams of the
fan-out and the parameter handoff.

---

## Requirements

- Linux; developed and run on **CentOS 7**
- **O2** with its ROOT 6 (`alienv load O2/latest`) — the committed default is
  `/home/alice/Software/v20230501`
- bash 4.2 or newer
- For the configuration window: ROOT with GUI support and an X display
  (`ssh -X` is sufficient). No other dependency.

Storage: nothing is deleted during a run. A full ten-batch run holds ten
batches of split ROOT files and eighty worker archives simultaneously.

---

## Quick start

```sh
git clone <this repository>
cd ALICE-ITS2-ML-Alignment-Manager

# Put the module archive in place
cp <somewhere>/v20250627+a_vtx....v20251024.tgz MODULE/

eval `alienv load -w /home/alice/Software/v20230501/sw O2/latest`

./config/alignctl.sh ui        # set the data path, module and schedule
./config/alignctl.sh doctor    # check this machine has what the run needs
./runAll_alignment.sh MYTAG
```

`doctor` reports missing input directories, absent archives, selected files
that are not on disk, and anything else that would stop the run — before it
starts rather than three batches in.

---

## Configuration

Every setting lives in [`config/alignment.conf`](config/alignment.conf):
the input directory and file selection, the module name, the job size, the
schedule, and the O2 path. See [`config/README.md`](config/README.md).

```sh
./config/alignctl.sh show               # everything, plus what follows from it
./config/alignctl.sh set N_WORKERS=16   # change and re-validate
./config/alignctl.sh ui                 # the same in a window
```

The window ([`RUN/ConfigUI/ConfigUI.C`](RUN/ConfigUI/ConfigUI.C)) browses
directories in a `TBrowser`-style tree and presents the input files as tick
boxes, so holding back a verification sample is a click rather than a
commented-out line. It writes nothing itself — Save shells out to
`alignctl.sh`, so the command line and the GUI cannot disagree.

Two settings cannot be read from a shell file by the code that needs them, so
they are generated:

| Setting | Reaches the code as |
|---|---|
| Input directory, file selection | `RUN/MasterDataScript/DataSetConfig.h`, included by `DataRandomMerge.C` |
| Events, epochs, cores, `jparallel` | `YMLPParallel.h`, overwritten in each worker's unpacked module |

The module's job size is a set of preprocessor defines inside the frozen
archive. Overwriting that header in the worker's copy after unpacking is the
only way to change it without repacking; the archive in `MODULE/` is never
modified. **Note that this covers job size only** — the module's physics
configuration (cut thresholds, learning rate, resolution coefficients) still
lives in preprocessor defines inside the source tree and requires a rebuilt
archive to change.

---

## Layout

```
ALIGN/    per-run scratch: worker directories and prepared slices
MODULE/   frozen module archives, one per version
PARAMS/   alignment parameter archives, including the step-900 reference
RESULT/   per-worker training output, retained
RUN/      MasterDataScript/  data preparation macros
          MergeParamsScript/ weight merging
          TrainScript/       distributed-training client (see below)
          ConfigUI/          the configuration window
config/   the configuration and its tooling
docs/     workflow.html
```

`runAll_alignment.sh` is the configured driver.
`runAll_alignment_example.sh` is the original, kept unchanged as the record of
the campaign the `v0.0.0-original-v20251024` baseline points at.

---

## For alignment groups

The method is a track-based alignment expressed as a learning problem. What is
conventional: a residual χ² in the sensor plane normalised by a resolution
model with intrinsic and multiple-scattering terms, rigid bodies with six
degrees of freedom, a vertex constraint from multi-prong events to fix the
coherent modes, and outlier rejection by hard cuts.

What differs from a Millepede-style global fit:

- **No global matrix.** Per-sensor gradients are accumulated and applied
  independently, so memory is O(N<sub>sensor</sub>) rather than
  O((6N<sub>sensor</sub>)²). Inter-sensor correlations are recovered by
  iterating steps rather than solved in one pass.
- **Iteration replaces inversion.** Convergence is empirical, monitored through
  the loss curve and residual distributions. **No parameter uncertainties or
  correlations are produced.**
- **Weak modes are attacked by the vertex constraint**, not by explicit
  constraint equations.
- **Robustness by rejection**, not by down-weighting: a track enters the cost
  in full or not at all.

In the 2025 module the vertex is re-derived from the same ITS tracks at the
current alignment, rather than taken from a reconstruction produced with the
geometry under correction. That removes a circularity at the price of a
fixed-point iteration whose convergence is empirical.

Section 14 of the module documentation states the position against both
Millepede-style and local iterative alignment in full, together with the
present version's limitations. **Readers evaluating the method for their own
detector should start there rather than here.**

---

## Reproducibility

The module archive name is the whole provenance record — nothing pins the
source repository to a commit. Tag the source trees with the version string
that appears in `MODULE_NAME` if a past run must be reconstructible.

Tag `v0.0.0-original-v20251024` marks the manager as it stood before the
documentation and configuration work, corresponding to module core version
`v20251024`.

---

## Status and caveats

This is working scientific code from an ongoing campaign, shared so the method
can be examined and reused. It is not a packaged product.

- Absolute paths to one workstation's storage and O2 build remain the first
  thing a new machine must change.
- `RUN/TrainScript/` holds an unrelated distributed-training client that pulls
  jobs from an HTTP service. It is **not** invoked by the driver and has known
  defects; see section 11 of `docs/workflow.html`.
- Worker failure is not fatal: the merge proceeds over whatever weights are
  present and reports the count in its log. Check that count.
- Section 11 of `docs/workflow.html` lists the remaining discrepancies between
  the scripts and the tree, with file and line.

Questions, and reports of anything that does not reproduce, are welcome
through the issue tracker.

---

## Credits

Method and module: **J. H. Kim**, Yonsei University.

Licensed under the Apache License 2.0 — see [`LICENSE`](LICENSE).
