#!/bin/bash
# ==========================================================================
#  tests/run_tests.sh -- exercise the configuration tooling and the driver's
#  pre-flight against real module archives, without O2 and without training
# ==========================================================================
#  Usage:  tests/run_tests.sh ARCHIVE.tgz [ARCHIVE.tgz ...]
#
#  Works on a scratch copy of this repository, so nothing here is touched.
#  For every archive: validate, inspect, generate, doctor, then the driver
#  with a stub `alienv` and a stub `root` -- the driver runs its checks,
#  unpacks and patches the workers, and stops at the first ROOT macro. The
#  worker tree is then compared with the archive: with every module knob at
#  keep only YMLPParallel.h may differ, plus the geometry guard on a tree
#  that carries both backends. Then a few configurations that must be
#  refused. Prints PASS/FAIL per check; exits non-zero if any failed.
# ==========================================================================
set -u
here=$(cd "$(dirname "$0")/.." && pwd)
[ $# -ge 1 ] || { echo "usage: $0 ARCHIVE.tgz [ARCHIVE.tgz ...]" >&2; exit 2; }

npass=0; nfail=0
check() { if [ "$1" -eq 0 ]; then echo "  PASS  $2"; npass=$((npass+1)); else echo "  FAIL  $2"; nfail=$((nfail+1)); fi; }
strip() { sed 's/\x1b\[[0-9;]*m//g'; }

work=$(mktemp -d "${TMPDIR:-/tmp}/aligntests.XXXXXX") || exit 1
trap 'rm -rf "$work"' EXIT
mgr="$work/mgr"; mkdir -p "$mgr" "$work/data" "$work/stub"
(cd "$here" && tar --exclude=.git --exclude=ALIGN --exclude=RESULT -cf - .) | tar -xf - -C "$mgr"
mkdir -p "$mgr/ALIGN" "$mgr/RESULT" "$mgr/MODULE" "$mgr/PARAMS"
# A seed archive with the two files every step reads.
mkdir -p "$work/seed/MLPTrain_Step900/weights"
echo "#seed" > "$work/seed/MLPTrain_Step900/weights/weights.txt"
echo "#usl"  > "$work/seed/MLPTrain_Step900/UpdateSensorsList.txt"
(cd "$work/seed" && tar -czf "$mgr/PARAMS/MLPTrain_Step900.tgz" MLPTrain_Step900)
# Stubs: alienv "loads" nothing, root refuses, so the driver stops at stage 1.
printf '#!/bin/bash\necho "export ALIGN_TEST_STUB=1"\n' > "$work/stub/alienv"
printf '#!/bin/bash\necho "stub root: refusing $*" >&2; exit 1\n' > "$work/stub/root"
chmod +x "$work/stub/alienv" "$work/stub/root"
for i in 1 2 3; do : > "$work/data/alignment-input-data_test_$i.root"; done

ctl="$mgr/config/alignctl.sh"
"$ctl" set DATA_INPUT_DIR="$work/data" "DATA_FILES=alignment-input-data_test_1.root alignment-input-data_test_2.root alignment-input-data_test_3.root" \
       DATA_FILES_PER_BATCH=2 O2_DIR=/ N_WORKERS=2 WORKER_LAUNCH_STAGGER=0 BASE_STEP=900 GEOM_BACKEND=o2 TRACK_SCHEMA=auto >/dev/null \
  || { echo "could not configure the scratch copy" >&2; exit 1; }
for k in $("$ctl" keys | grep -E '^MODULE_(LEARNING_METHOD|DULEVEL|LAYERS|DET_MAG|NTRACKMAX|PT_|CHI_|TRACK_REJECT|IP_RANGE|MIN_CLUSTER|VERTEX_DERIVATIVES|ETA_|VALID_WINDOW|QUALITY_|MAX_BAD)'); do
  "$ctl" set "$k=keep" >/dev/null || exit 1
done

for archive in "$@"; do
  [ -f "$archive" ] || { echo "no such archive: $archive" >&2; exit 2; }
  name=$(basename "$archive" .tgz)
  cp "$archive" "$mgr/MODULE/$name.tgz"
  echo "== $name"
  "$ctl" set MODULE_NAME="$name" >/dev/null; check $? "set MODULE_NAME"
  "$ctl" validate >/dev/null 2>&1; check $? "validate"
  report=$("$ctl" inspect 2>&1); check $? "inspect"
  gen=$(echo "$report" | sed -n 's/^MP_GENERATION=//p'); cache=$(echo "$report" | sed -n 's/^MP_CACHE_CAPABLE=//p')
  echo "$report" | grep -q '^MP_VALID=1$'; check $? "inspect: MP_VALID=1 (generation ${gen:-?}, cache-capable ${cache:-?})"
  "$ctl" generate >/dev/null 2>&1; check $? "generate"
  out=$("$ctl" doctor 2>&1 | strip); echo "$out" | grep -q '^0 failed'; check $? "doctor: $(echo "$out" | tail -1)"
  echo "$out" | grep -q 'track schema auto ->'; check $? "doctor resolved the schema from the archive"

  tag="t_$name"
  PATH="$work/stub:$PATH" "$mgr/runAll_alignment.sh" "$tag" >"$work/driver.log" 2>&1
  grep -q 'ERROR: DataRandomMerge failed' "$work/driver.log"; check $? "driver ran its checks, unpacked and patched, and stopped at stage 1"
  rm -rf "$work/ref"; mkdir -p "$work/ref"; tar -xzf "$archive" -C "$work/ref"
  wt="$mgr/ALIGN/$tag/align_worker_0/$name"
  diffs=$(diff -rq "$work/ref/$name" "$wt" 2>/dev/null | grep -v '^Only in' | sed 's/^Files [^ ]* and //; s/ differ$//')
  n=$(echo "$diffs" | grep -c .)
  if [ "$cache" = 1 ]; then want=2; else want=1; fi
  [ "$n" -eq "$want" ] && echo "$diffs" | grep -q 'YMLPParallel.h'; check $? "worker tree differs from the archive only in YMLPParallel.h$([ "$cache" = 1 ] && echo ' and the geometry guard') ($n file(s))"
  [ -f "$mgr/ALIGN/$tag/align_worker_0/module_patch_manifest.txt" ]; check $? "manifest written beside the worker's archive copy"
  PATH="$work/stub:$PATH" "$mgr/runAll_alignment.sh" "$tag" >/dev/null 2>&1
  diffs2=$(diff -rq "$work/ref/$name" "$wt" 2>/dev/null | grep -v '^Only in' | grep -c .)
  [ "$diffs2" -eq "$n" ]; check $? "second run of the same tag: worker tree unpacked afresh, same result"

  echo "-- refusals ($name)"
  "$ctl" set MODULE_LEARNING_METHOD=kBFGS >/dev/null
  PATH="$work/stub:$PATH" "$mgr/runAll_alignment.sh" "$tag" 2>&1 | grep -q 'not implemented by this module'; check $? "an unimplemented learning method is refused before unpacking"
  "$ctl" set MODULE_LEARNING_METHOD=keep MODULE_PT_MIN=1000 >/dev/null
  PATH="$work/stub:$PATH" "$mgr/runAll_alignment.sh" "$tag" 2>&1 | grep -q 'pT window'; check $? "a pT window emptied against the archive's own value is refused"
  "$ctl" set MODULE_PT_MIN=keep >/dev/null
  if [ "$cache" != 1 ]; then
    "$ctl" set GEOM_BACKEND=cache >/dev/null
    PATH="$work/stub:$PATH" "$mgr/runAll_alignment.sh" "$tag" 2>&1 | grep -q 'only has the O2 backend'; check $? "cache mode on an O2-only tree is refused"
    "$ctl" set GEOM_BACKEND=o2 MODULE_LAYERS=3,4 >/dev/null
    PATH="$work/stub:$PATH" "$mgr/runAll_alignment.sh" "$tag" 2>&1 | grep -q 'no ALIGN_LAYER_MASK'; check $? "a layer selection on a tree without one is refused"
    "$ctl" set MODULE_LAYERS=keep >/dev/null
  else
    "$ctl" set MODULE_LAYERS=5,6 MODULE_DULEVEL=4 >/dev/null
    PATH="$work/stub:$PATH" "$mgr/runAll_alignment.sh" "$tag" >/dev/null 2>&1
    grep -q 'ALIGN_LAYER_MASK 0x60' "$wt/Ymlp/inc/DetectorConstant.h" && grep -q 'DULEVEL 4' "$wt/Ymlp/inc/DetectorConstant.h"; check $? "layers 5,6 and DULEVEL 4 patched into the worker"
    "$ctl" set MODULE_LAYERS=keep MODULE_DULEVEL=keep >/dev/null
    PATH="$work/stub:$PATH" "$mgr/runAll_alignment.sh" "$tag" >/dev/null 2>&1
    grep -q 'ALIGN_LAYER_MASK 0x60' "$wt/Ymlp/inc/DetectorConstant.h"; [ $? -ne 0 ]; check $? "back to keep: the worker holds the archive's own values again"
  fi
  "$ctl" set TRACK_SCHEMA=2024 >/dev/null
  if [ "$gen" != 2024 ]; then
    PATH="$work/stub:$PATH" "$mgr/runAll_alignment.sh" "$tag" 2>&1 | grep -q 'resolves to 2024'; check $? "a wrong TRACK_SCHEMA is refused"
  fi
  "$ctl" set TRACK_SCHEMA=auto >/dev/null
  rm -rf "$mgr/ALIGN/$tag" "$mgr/RESULT/$tag" "$mgr/PARAMS/$tag"
done

echo
echo "$npass passed, $nfail failed"
[ "$nfail" -eq 0 ]
