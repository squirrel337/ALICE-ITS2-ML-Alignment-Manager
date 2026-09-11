#!/bin/bash
# ==========================================================================
#  runAll_alignment.sh -- the batch driver, reading config/alignment.conf
# ==========================================================================
#  Same pipeline as runAll_alignment_example.sh, with every setting taken
#  from the configuration instead of being written inline. The example
#  driver is kept unchanged as the record of the original run.
#
#  Usage:  ./runAll_alignment.sh <TAG>
#          ./runAll_alignment.sh            (uses RUN_TAG from the config)
#
#  Check the machine first:  ./config/alignctl.sh doctor
# ==========================================================================
set -u

homedir=$(cd "$(dirname "$0")" && pwd)
. "$homedir/config/alignconf.sh"

ac_load    || exit 1
ac_validate || { echo "configuration is not consistent; fix it first" >&2; exit 1; }

TAG=${1:-$RUN_TAG}
if [ -z "$TAG" ]; then
  echo "usage: $0 <TAG>    (or set RUN_TAG in config/alignment.conf)" >&2
  exit 2
fi

aligndir="${homedir}/ALIGN"
alignworker="${aligndir}/${TAG}"
paramsdir="${homedir}/PARAMS"
moduledir="${homedir}/MODULE"
resultdir="${homedir}/RESULT"
resultcontainer="${resultdir}/${TAG}"
modulename="${MODULE_NAME}"

say() { echo "[$(date '+%H:%M:%S')] $*"; }
die() { echo "[$(date '+%H:%M:%S')] ERROR: $*" >&2; exit 1; }

say "tag          : ${TAG}"
say "homedir      : ${homedir}"
say "module       : ${modulename}"
say "workers      : ${N_WORKERS}"
say "batches      : ${N_BATCHES} x ${STEPS_PER_BATCH} step(s)"
say "steps        : ${AC_FIRST_STEP} .. ${AC_FINAL_STEP}"
say "data-prep in : ${AC_MASTER_DIR}"

[ -f "$AC_MODULE_TGZ" ]    || die "no module archive at $AC_MODULE_TGZ"
[ -f "$AC_REFERENCE_TGZ" ] || die "no reference archive at $AC_REFERENCE_TGZ"
[ -d "$AC_MASTER_DIR" ]    || die "no data-prep macro directory at $AC_MASTER_DIR"

# --- what the archive is, and whether every module knob applies to it ------

# The headers are read out of the archive into a scratch copy and the knobs
# tried on it exactly as they will be applied to each worker, so an archive
# packed wrongly, a schema mismatch or a knob this module does not have
# stops the run here rather than after the data preparation.
ac_resolve_schema || die "could not resolve the track schema"
probe=$(mktemp -d "${TMPDIR:-/tmp}/alignprobe.XXXXXX") || die "cannot create a scratch directory"
# Every refusal below exits through die; the scratch copy goes with it.
trap 'rm -rf "$probe"' EXIT
ac_probe "$AC_MODULE_TGZ" "$probe" || die "could not read the module headers out of $AC_MODULE_TGZ"
proberoot=$AC_PROBE_ROOT
archivetop=$(mp_archive_top "$AC_MODULE_TGZ")
[ "$MP_VALID" -eq 1 ] \
  || die "$AC_MODULE_TGZ does not look like an alignment module (no YMLPParallel.h, Ymlp/inc or run_train_circle.C under ${archivetop}/)"
[ "$archivetop" = "$modulename" ] \
  || die "the archive unpacks to '${archivetop}/' but MODULE_NAME is '${modulename}'; repack it as: tar czf ${modulename}.tgz ${modulename}"
[ "$AC_TRACK_SCHEMA" = "$MP_SCHEMA" ] \
  || die "TRACK_SCHEMA=${TRACK_SCHEMA} resolves to ${AC_TRACK_SCHEMA}, but the module reads the ${MP_SCHEMA} input tree"
if [ "$GEOM_BACKEND" = cache ]; then
  [ "$MP_CACHE_CAPABLE" -eq 1 ] \
    || die "GEOM_BACKEND is cache, but this module only has the O2 backend; use o2"
  mp_archive_member "$AC_MODULE_TGZ" "$archivetop" "$MP_GEOMCACHE" >/dev/null \
    || die "GEOM_BACKEND is cache, but the archive holds no ${archivetop}/${MP_GEOMCACHE}; build it with the module's tools/export_geometry_cache.C and repack, or set GEOM_BACKEND=o2"
fi
mp_apply "$proberoot" || die "the module knobs in the configuration do not apply to this archive (see above)"
# A configured value against the archive's own on the other side of a
# relation: an empty pT window, a training cut looser than the cost cut, a
# learning rate of zero. doctor reports these; the run must not start on them.
relation_errors=$(ac_relation_errors)
if [ -n "$relation_errors" ]; then
  echo "$relation_errors" >&2
  die "the module knobs conflict with the archive's own values (see above)"
fi
say "module       : generation ${MP_GENERATION}, input schema ${MP_SCHEMA}, backend ${GEOM_BACKEND}"
if [ -n "$MP_APPLIED" ]; then
  say "module knobs :"
  printf '%s' "$MP_APPLIED" | sed 's/^/                 /'
else
  say "module knobs : none -- the archive trains as shipped"
fi
rm -rf "$probe"
trap - EXIT

# Push the settings that live inside ROOT macros before anything reads them.
ac_gen_datasetconfig "$AC_MASTER_DIR/DataSetConfig.h" || die "could not write DataSetConfig.h"
ac_gen_dataschema   "$AC_MASTER_DIR/DataSchema.h"   || die "could not write DataSchema.h"
say "wrote DataSetConfig.h and DataSchema.h (track schema ${AC_TRACK_SCHEMA}, from TRACK_SCHEMA=${TRACK_SCHEMA})"

# DataRandomMerge.C stages its symlinks here and lists the directory to build
# its file list; nothing else creates it.
mkdir -p "$AC_MASTER_DIR/MasterData" || die "could not create MasterData staging directory"

mkdir -p "$alignworker" "$resultcontainer" "$paramsdir/$TAG" || die "could not create run directories"

# alienv prints the environment on stdout; an empty result means it failed,
# which `eval` on its own would not report.
o2env=$(alienv load -w "${O2_DIR}/sw" O2/latest) \
  || die "alienv failed for ${O2_DIR}/sw"
[ -n "$o2env" ] || die "alienv produced no environment for ${O2_DIR}/sw"
eval "$o2env"
command -v root >/dev/null 2>&1 || die "root is still not on PATH after loading O2"

# --- one unpacked module per worker ---------------------------------------

say "unpacking the module into ${N_WORKERS} worker directories"
for (( ns = 0; ns < N_WORKERS; ns++ )); do
  worker="${alignworker}/align_worker_${ns}"
  mkdir -p "$worker" || die "could not create $worker"
  # A fresh copy every run: the previous run's logs and outputs go, and a knob
  # at keep is the archive's own value even after a run that patched it. (An
  # earlier "only if the archive is newer" shortcut never fired, because the
  # unpacked directory carries the archive's older timestamp.)
  rm -rf "$worker/$modulename"
  cp "$AC_MODULE_TGZ" "$worker/${modulename}.tgz" || die "could not copy the module archive"
  ( cd "$worker" && tar -zxf "${modulename}.tgz" ) || die "could not unpack the module in $worker"
  # The module's job size is a set of #defines; overwriting the header in the
  # unpacked copy is the only way to set it without rebuilding the archive.
  ac_gen_ymlpparallel "$worker/$modulename/YMLPParallel.h" \
    || die "could not write YMLPParallel.h in $worker"
  # The other knobs are patched into the unpacked copy the same way, and what
  # was done is recorded beside the worker's copy of the archive.
  mp_apply "$worker/$modulename" || die "could not patch the module in $worker"
  mp_manifest "$worker/module_patch_manifest.txt" "$AC_MODULE_TGZ" "$worker/$modulename"
done
say "each worker has nDATA=${MODULE_EVENTS} nEPOCH=${MODULE_EPOCHS} nCORE=${MODULE_CORES}"

# --- batches ---------------------------------------------------------------

for (( bcnt = 1; bcnt <= N_BATCHES; bcnt++ )); do

  input_step=$(( BASE_STEP + (bcnt - 1) * STEPS_PER_BATCH ))
  start_step=$(( input_step + 1 ))
  end_step=$(( BASE_STEP + bcnt * STEPS_PER_BATCH ))

  say "=== batch ${bcnt}/${N_BATCHES}: steps ${start_step}..${end_step} from ${input_step} ==="
  # Emptied, not just created: an archive left by an earlier attempt at this
  # batch would otherwise be unpacked and merged as if it were from this one.
  rm -rf "${resultcontainer}/result_step_${bcnt}"
  mkdir -p "${resultcontainer}/result_step_${bcnt}"

  # ---- stage 1: data preparation ----
  say "[1/4] preparing data"
  cd "$AC_MASTER_DIR" || die "cannot enter $AC_MASTER_DIR"

  # Anything left here by an aborted run belongs to that run, not this one.
  rm -f "alignment-input-data_${bcnt}.root" "MasterData_${bcnt}.lst"
  rm -rf "step${bcnt}"

  root -l -b -q "DataRandomMerge.C(${bcnt},${DATA_MERGE_MAX_FILES})" \
       &> "alignment-input-data-merge.log.${bcnt}" || die "DataRandomMerge failed (batch ${bcnt})"
  [ -s "alignment-input-data_${bcnt}.root" ] \
    || die "batch ${bcnt}: the merge produced no alignment-input-data_${bcnt}.root (see alignment-input-data-merge.log.${bcnt})"

  root -l -b -q "DataSplit.C(${bcnt},${N_WORKERS},${BATCH_ID})" \
       &> "alignment-input-data-split.log.${bcnt}" || die "DataSplit failed (batch ${bcnt})"

  [ -d "step${bcnt}" ] || die "DataSplit produced no step${bcnt} directory"
  mv "alignment-input-data-merge.log.${bcnt}" "step${bcnt}/"
  mv "alignment-input-data-split.log.${bcnt}" "step${bcnt}/"
  mv "MasterData_${bcnt}.lst"                 "step${bcnt}/"
  mv "alignment-input-data_${bcnt}.root"      "step${bcnt}/"
  rm -rf "${alignworker}/step${bcnt}"
  mv "step${bcnt}" "${alignworker}/step${bcnt}"

  # ---- stage 2: parallel training ----
  say "[2/4] launching ${N_WORKERS} workers"
  worker_pid=()
  for (( ns = 0; ns < N_WORKERS; ns++ )); do
    workdir="${alignworker}/align_worker_${ns}/${modulename}"
    slice="${alignworker}/step${bcnt}/alignment-input-data-split${ns}.root"
    [ -f "$slice" ] || die "worker ${ns} has no slice at $slice"

    cd "$workdir" || die "cannot enter $workdir"
    rm -f XXXXinput.root
    ln -s "$slice" XXXXinput.root

    if [ "$bcnt" -eq 1 ]; then
      src="${paramsdir}/MLPTrain_Step${input_step}.tgz"
    else
      src="${paramsdir}/${TAG}/MLPTrain_Step${input_step}.tgz"
    fi
    [ -f "$src" ] || die "no starting parameters at $src"
    cp "$src" "MLPTrain_Step${input_step}.tgz"
    tar -zxf "MLPTrain_Step${input_step}.tgz" || die "could not unpack $src in worker ${ns}"

    ./process_all_master.sh "${TAG}" "${start_step}" "${end_step}" \
        > "${alignworker}/step${bcnt}/worker_${ns}.log" 2>&1 &
    worker_pid[$ns]=$!
    sleep "${WORKER_LAUNCH_STAGGER}"
  done

  # A bare `wait` discards every exit status, so a run that lost workers used
  # to finish and report success. Wait on each pid and say which ones failed.
  say "[2/4] waiting for the workers"
  worker_failed=0
  for (( ns = 0; ns < N_WORKERS; ns++ )); do
    if ! wait "${worker_pid[$ns]}"; then
      echo "  warning: worker ${ns} exited non-zero (see step${bcnt}/worker_${ns}.log)" >&2
      worker_failed=$((worker_failed + 1))
    fi
  done
  if [ "$worker_failed" -gt 0 ]; then
    say "[2/4] ${worker_failed} of ${N_WORKERS} workers failed; the merge will use the rest"
  fi

  # ---- collect ----
  for (( ns = 0; ns < N_WORKERS; ns++ )); do
    workdir="${alignworker}/align_worker_${ns}/${modulename}"
    cd "$workdir" || die "cannot enter $workdir"
    mkdir -p "../buffer_step${bcnt}"
    for (( nStep = start_step; nStep <= end_step; nStep++ )); do
      if [ -f "MLPTrain_Step${nStep}.tgz" ]; then
        # Move this archive by name; a glob here would also sweep up the
        # archives the later iterations still need.
        mv "MLPTrain_Step${nStep}.tgz" \
           "${resultcontainer}/result_step_${bcnt}/MLPTrain_Step${nStep}-aw${ns}.tgz"
      else
        echo "  warning: worker ${ns} produced no MLPTrain_Step${nStep}.tgz" >&2
      fi
      [ -d "MLPTrain_Step${nStep}" ] && mv "MLPTrain_Step${nStep}" "../buffer_step${bcnt}/"
    done
    rm -f "MLPTrain_Step${input_step}.tgz"
    [ -d "MLPTrain_Step${input_step}" ] && mv "MLPTrain_Step${input_step}" "../buffer_step${bcnt}/"
  done

  # ---- stage 3: merge ----
  say "[3/4] merging worker weights"
  # WeightsMerge.C merges whatever this directory contains, so a file left by a
  # previous run -- or by a run with more workers -- would be averaged in.
  rm -rf "${AC_MERGE_DIR}/weights_step${bcnt}"
  # ...and the macro's own outputs, so the check below cannot pass on a file
  # an interrupted earlier attempt at this batch left behind.
  rm -f "${AC_MERGE_DIR}/weights_merge_step${bcnt}.txt" \
        "${AC_MERGE_DIR}/weights_merge_${bcnt}.lst" \
        "${AC_MERGE_DIR}/Monitor_MergeWeights_step${bcnt}.root"
  mkdir -p "${AC_MERGE_DIR}/weights_step${bcnt}"

  found=0
  for (( ns = 0; ns < N_WORKERS; ns++ )); do
    archive="${resultcontainer}/result_step_${bcnt}/MLPTrain_Step${end_step}-aw${ns}.tgz"
    [ -f "$archive" ] || { echo "  warning: worker ${ns} has no archive to merge" >&2; continue; }

    cd "${resultcontainer}/result_step_${bcnt}" || die "cannot enter the result container"
    rm -rf "MLPTrain_Step${end_step}" "MLPTrain_Step${end_step}-aw${ns}"
    # A truncated archive used to unpack partially and be merged as zeros.
    if ! tar -zxf "$archive"; then
      echo "  warning: worker ${ns} archive is unreadable or truncated; left out of the merge" >&2
      continue
    fi
    [ -d "MLPTrain_Step${end_step}" ] || {
      echo "  warning: worker ${ns} archive holds no MLPTrain_Step${end_step}; left out of the merge" >&2
      continue
    }
    mv "MLPTrain_Step${end_step}" "MLPTrain_Step${end_step}-aw${ns}"

    # Newest weights_Epoch file: the last epoch the worker finished.
    weightfile=$(ls -1t "MLPTrain_Step${end_step}-aw${ns}/weights/" 2>/dev/null \
                 | grep '^weights_Epoch' | head -n1)
    if [ -z "$weightfile" ]; then
      echo "  warning: worker ${ns} wrote no weights_Epoch file" >&2
      continue
    fi
    splitindex=$(printf '%.3d' "$ns")
    cp "MLPTrain_Step${end_step}-aw${ns}/weights/${weightfile}" \
       "${AC_MERGE_DIR}/weights_step${bcnt}/weights_n${splitindex}.txt" \
       || die "could not collect weights from worker ${ns}"
    found=$((found + 1))
  done

  [ "$found" -gt 0 ] || die "batch ${bcnt}: no worker produced weights"
  say "[3/4] merging ${found}/${N_WORKERS} worker weight sets"

  cd "$AC_MERGE_DIR" || die "cannot enter $AC_MERGE_DIR"
  root -l -b -q "WeightsMerge.C(${bcnt})" &> "alignment-params-merge.log.${bcnt}" \
    || die "WeightsMerge failed (batch ${bcnt})"
  [ -s "weights_merge_step${bcnt}.txt" ] || die "WeightsMerge produced no usable weights_merge_step${bcnt}.txt"

  mv "alignment-params-merge.log.${bcnt}"      "${alignworker}/step${bcnt}/"
  mv "Monitor_MergeWeights_step${bcnt}.root"   "${alignworker}/step${bcnt}/" 2>/dev/null
  mv "weights_merge_${bcnt}.lst"               "${alignworker}/step${bcnt}/" 2>/dev/null
  mv "weights_merge_step${bcnt}.txt"           "${alignworker}/step${bcnt}/"
  rm -rf "${alignworker}/step${bcnt}/weights_step${bcnt}"
  mv "weights_step${bcnt}"                     "${alignworker}/step${bcnt}/"

  # ---- stage 3-1: hand the parameters to the next batch ----
  say "[4/4] packing parameters for step ${end_step}"
  cd "${paramsdir}/${TAG}" || die "cannot enter ${paramsdir}/${TAG}"
  rm -rf "MLPTrain_Step${end_step}"
  cp "$AC_REFERENCE_TGZ" "MLPTrain_Step${BASE_STEP}.tgz"
  tar -zxf "MLPTrain_Step${BASE_STEP}.tgz"
  mv "MLPTrain_Step${BASE_STEP}" "MLPTrain_Step${end_step}"
  cp "${alignworker}/step${bcnt}/weights_merge_step${bcnt}.txt" \
     "MLPTrain_Step${end_step}/weights/weights.txt" \
     || die "could not install the merged weights"
  tar -zcf "MLPTrain_Step${end_step}.tgz" "MLPTrain_Step${end_step}" \
    || die "could not pack MLPTrain_Step${end_step}.tgz"
  rm -rf "MLPTrain_Step${end_step}" "MLPTrain_Step${BASE_STEP}.tgz"

  say "batch ${bcnt} done -> ${paramsdir}/${TAG}/MLPTrain_Step${end_step}.tgz"
  cd "$homedir"
done

say "run complete: steps ${AC_FIRST_STEP}..${AC_FINAL_STEP}"
say "final parameters: ${paramsdir}/${TAG}/MLPTrain_Step${AC_FINAL_STEP}.tgz"
