#!/bin/bash
# ==========================================================================
#  alignconf.sh -- load, validate and expand the central configuration
# ==========================================================================
#  Sourced by alignctl.sh and by runAll_alignment.sh. Not meant to be run
#  directly.
#
#  Targets bash 4.2 (CentOS 7): no namerefs, no ${var@Q}.
# ==========================================================================

# Every key the configuration understands, in display order, as
# NAME:TYPE:GROUP. TYPE is one of dir, path, name, int, text, list.
AC_KEYS="
DATA_INPUT_DIR:dir:data
DATA_FILE_PATTERN:name:data
DATA_FILES:list:data
DATA_FILES_PER_BATCH:int:data
DATA_MERGE_MAX_FILES:int:data
MODULE_NAME:name:module
MODULE_EVENTS:int:module
MODULE_EPOCHS:int:module
MODULE_JPARALLEL:int:module
MODULE_CORES:int:module
BASE_STEP:int:schedule
STEPS_PER_BATCH:int:schedule
N_BATCHES:int:schedule
N_WORKERS:int:schedule
BATCH_ID:int:schedule
O2_DIR:dir:env
MASTER_DATA_SCRIPT_DIR:dir:env
WORKER_LAUNCH_STAGGER:int:env
RUN_TAG:name:env
"

ac_keys() { echo "$AC_KEYS" | awk -F: 'NF{print $1}'; }
ac_type() { echo "$AC_KEYS" | awk -F: -v k="$1" '$1==k{print $2}'; }

ac_die()  { echo "alignconf: $*" >&2; return 1; }

# --- location -------------------------------------------------------------

# Repository root, derived from this file's own location so the scripts work
# from any working directory.
ac_root() {
  local here
  here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd) || return 1
  cd "$here/.." && pwd
}

ac_conf_file() { echo "${ALIGN_CONF:-$(ac_root)/config/alignment.conf}"; }

# --- load -----------------------------------------------------------------

# Sources the configuration after refusing anything that is not a plain
# assignment, so a stray command in the file cannot execute.
ac_load() {
  local f offenders
  f=$(ac_conf_file)
  [ -r "$f" ] || { ac_die "cannot read $f"; return 1; }

  offenders=$(grep -nvE '^[[:space:]]*(#|$)|^[A-Z_][A-Z0-9_]*=|^[[:space:]]+[^=]*$' "$f")
  if [ -n "$offenders" ]; then
    ac_die "refusing to source $f -- these lines are not assignments:"
    echo "$offenders" >&2
    return 1
  fi

  # shellcheck disable=SC1090
  . "$f" || { ac_die "failed to source $f"; return 1; }
  AC_CONF_LOADED=1
  ac_derive
}

# --- derived values -------------------------------------------------------

ac_derive() {
  AC_ROOT=$(ac_root)

  AC_TOTAL_STEPS=$(( N_BATCHES * STEPS_PER_BATCH ))
  AC_FINAL_STEP=$(( BASE_STEP + AC_TOTAL_STEPS ))
  AC_FIRST_STEP=$(( BASE_STEP + 1 ))

  AC_MODULE_TGZ="$AC_ROOT/MODULE/${MODULE_NAME}.tgz"
  AC_REFERENCE_TGZ="$AC_ROOT/PARAMS/MLPTrain_Step${BASE_STEP}.tgz"

  # The driver in the tree enters RUN/MasterDataScript; the directory that
  # exists is RUN/MasterDataScript.0. Accept either, preferring the
  # unsuffixed one, unless the configuration names a directory outright.
  if [ -n "$MASTER_DATA_SCRIPT_DIR" ]; then
    AC_MASTER_DIR="$MASTER_DATA_SCRIPT_DIR"
  elif [ -d "$AC_ROOT/RUN/MasterDataScript" ]; then
    AC_MASTER_DIR="$AC_ROOT/RUN/MasterDataScript"
  else
    AC_MASTER_DIR="$AC_ROOT/RUN/MasterDataScript.0"
  fi

  AC_MERGE_DIR="$AC_ROOT/RUN/MergeParamsScript"

  AC_DATA_FILE_COUNT=$(echo $DATA_FILES | wc -w)
  AC_DATA_FILE_COUNT=${AC_DATA_FILE_COUNT// /}
}

# --- validation -----------------------------------------------------------

# Structural checks only: types and relationships between values. Whether
# the paths exist on this machine is ac_doctor's job.
ac_validate() {
  local k t v bad=0

  for k in $(ac_keys); do
    t=$(ac_type "$k")
    eval "v=\$$k"
    case "$t" in
      int)
        if ! echo "$v" | grep -qE '^[0-9]+$'; then
          echo "  $k: expected a whole number, got '$v'" >&2; bad=1
        fi ;;
      dir|path|name)
        case "$v" in
          *'"'*) echo "  $k: must not contain a double quote" >&2; bad=1 ;;
          *'$('*|*'`'*) echo "  $k: must not contain command substitution" >&2; bad=1 ;;
        esac ;;
    esac
  done
  [ $bad -eq 0 ] || return 1

  [ "$N_WORKERS" -ge 1 ] || { echo "  N_WORKERS must be at least 1" >&2; bad=1; }
  [ "$N_WORKERS" -le 200 ] || {
    echo "  N_WORKERS is $N_WORKERS; WeightsMerge.C addresses at most 200 (nPARALLEL)" >&2; bad=1; }
  [ "$N_BATCHES" -ge 1 ] || { echo "  N_BATCHES must be at least 1" >&2; bad=1; }
  [ "$STEPS_PER_BATCH" -ge 1 ] || { echo "  STEPS_PER_BATCH must be at least 1" >&2; bad=1; }
  [ "$MODULE_EPOCHS" -ge 1 ] || { echo "  MODULE_EPOCHS must be at least 1" >&2; bad=1; }
  [ "$MODULE_EVENTS" -ge 1 ] || { echo "  MODULE_EVENTS must be at least 1" >&2; bad=1; }
  [ "$MODULE_CORES" -ge 1 ] || { echo "  MODULE_CORES must be at least 1" >&2; bad=1; }

  [ "$AC_DATA_FILE_COUNT" -ge 1 ] || {
    echo "  DATA_FILES is empty; nothing to train on" >&2; bad=1; }
  [ "$DATA_FILES_PER_BATCH" -ge 1 ] || {
    echo "  DATA_FILES_PER_BATCH must be at least 1" >&2; bad=1; }
  if [ "$AC_DATA_FILE_COUNT" -ge 1 ] && [ "$DATA_FILES_PER_BATCH" -gt "$AC_DATA_FILE_COUNT" ]; then
    echo "  DATA_FILES_PER_BATCH ($DATA_FILES_PER_BATCH) exceeds the $AC_DATA_FILE_COUNT selected files" >&2
    bad=1
  fi

  [ -z "$MODULE_NAME" ] && { echo "  MODULE_NAME is empty" >&2; bad=1; }

  return $bad
}

# --- generators -----------------------------------------------------------

_ac_banner() {
  echo "// Generated by config/alignctl.sh from config/alignment.conf."
  echo "// Edits here are overwritten on the next generate; change the"
  echo "// configuration instead."
}

# The module's job size lives in preprocessor macros inside the frozen
# archive, so it can only be set by overwriting this header in the worker's
# unpacked copy before ROOT reads it.
ac_gen_ymlpparallel() {
  local out="$1"
  { _ac_banner
    echo "#define jparallel $MODULE_JPARALLEL"
    echo "#define nCORE $MODULE_CORES"
    echo "#define nDATA $MODULE_EVENTS"
    echo "#define nEPOCH $MODULE_EPOCHS"
  } > "$out"
}

# The data location and the file selection, for DataRandomMerge.C.
ac_gen_datasetconfig() {
  local out="$1" f n=0
  { _ac_banner
    echo "#ifndef DATASETCONFIG_H"
    echo "#define DATASETCONFIG_H"
    echo
    echo "static const char* kDataSetDir = \"$DATA_INPUT_DIR\";"
    echo "static const int   kFilesPerBatch = $DATA_FILES_PER_BATCH;"
    echo "static const int   kNDataFiles = $AC_DATA_FILE_COUNT;"
    echo "static const char* kDataFiles[] = {"
    for f in $DATA_FILES; do
      echo "   \"$f\","
      n=$((n + 1))
    done
    # A zero-length array is not valid C++; keep one placeholder the count
    # already excludes.
    [ "$n" -eq 0 ] && echo "   \"\""
    echo "};"
    echo
    echo "#endif"
  } > "$out"
}

ac_generate() {
  ac_gen_datasetconfig "$AC_MASTER_DIR/DataSetConfig.h" || return 1
  echo "wrote $AC_MASTER_DIR/DataSetConfig.h"
  # YMLPParallel.h is written per worker at run time, once the module has
  # been unpacked; a copy here documents what the workers will receive.
  ac_gen_ymlpparallel "$AC_ROOT/config/YMLPParallel.h.generated" || return 1
  echo "wrote $AC_ROOT/config/YMLPParallel.h.generated"
}

# --- environment checks ---------------------------------------------------

_ac_ok()   { printf '  \033[32mok\033[0m    %s\n' "$1"; }
_ac_warn() { printf '  \033[33mwarn\033[0m  %s\n' "$1"; AC_DOCTOR_WARN=$((AC_DOCTOR_WARN+1)); }
_ac_bad()  { printf '  \033[31mFAIL\033[0m  %s\n' "$1"; AC_DOCTOR_FAIL=$((AC_DOCTOR_FAIL+1)); }

ac_doctor() {
  local f missing=0 present=0
  AC_DOCTOR_FAIL=0
  AC_DOCTOR_WARN=0

  echo "paths"
  [ -d "$DATA_INPUT_DIR" ] && _ac_ok "DATA_INPUT_DIR" || _ac_bad "DATA_INPUT_DIR does not exist: $DATA_INPUT_DIR"
  [ -d "$O2_DIR" ]         && _ac_ok "O2_DIR"         || _ac_bad "O2_DIR does not exist: $O2_DIR"
  [ -d "$AC_MASTER_DIR" ]  && _ac_ok "data-prep macros at $AC_MASTER_DIR" \
                           || _ac_bad "no data-prep macro directory at $AC_MASTER_DIR"
  [ -d "$AC_MERGE_DIR" ]   && _ac_ok "merge macros" || _ac_bad "no merge macro directory at $AC_MERGE_DIR"

  # DataRandomMerge.C stages its symlinks here and lists the directory to
  # build its file list; nothing in the tree creates it.
  if [ -d "$AC_MASTER_DIR/MasterData" ]; then
    _ac_ok "MasterData staging directory"
  else
    _ac_bad "missing $AC_MASTER_DIR/MasterData -- DataRandomMerge.C needs it (mkdir it)"
  fi

  # DataRandomMerge.C includes this; it is generated, not committed.
  if [ -f "$AC_MASTER_DIR/DataSetConfig.h" ]; then
    _ac_ok "DataSetConfig.h present"
  else
    _ac_bad "missing $AC_MASTER_DIR/DataSetConfig.h -- run 'alignctl.sh generate'"
  fi

  echo "archives"
  [ -f "$AC_MODULE_TGZ" ]    && _ac_ok "module archive" || _ac_bad "no module archive at $AC_MODULE_TGZ"
  [ -f "$AC_REFERENCE_TGZ" ] && _ac_ok "reference parameters for step $BASE_STEP" \
                             || _ac_bad "no reference archive at $AC_REFERENCE_TGZ"

  echo "data files"
  if [ -d "$DATA_INPUT_DIR" ]; then
    for f in $DATA_FILES; do
      if [ -f "$DATA_INPUT_DIR/$f" ]; then present=$((present+1)); else
        _ac_bad "selected file not found: $f"; missing=$((missing+1))
      fi
    done
    [ "$missing" -eq 0 ] && _ac_ok "all $present selected files present"
  else
    _ac_warn "skipped -- DATA_INPUT_DIR is not readable"
  fi

  echo "working directories"
  for d in ALIGN MODULE PARAMS RESULT; do
    [ -d "$AC_ROOT/$d" ] && _ac_ok "$d/" || _ac_bad "$d/ missing -- run ./run_dir_maker.sh"
  done

  echo "tools"
  command -v root >/dev/null 2>&1 && _ac_ok "root on PATH" \
    || _ac_warn "root not on PATH -- load the O2 environment before running"

  echo
  echo "$AC_DOCTOR_FAIL failed, $AC_DOCTOR_WARN warnings"
  [ "$AC_DOCTOR_FAIL" -eq 0 ]
}

# --- display --------------------------------------------------------------

ac_print() {
  local k t v group last=""
  for k in $(ac_keys); do
    t=$(ac_type "$k")
    group=$(echo "$AC_KEYS" | awk -F: -v key="$k" '$1==key{print $3}')
    if [ "$group" != "$last" ]; then printf '\n[%s]\n' "$group"; last=$group; fi
    eval "v=\$$k"
    if [ "$t" = list ]; then
      printf '  %-24s %s file(s)\n' "$k" "$(echo $v | wc -w)"
    else
      printf '  %-24s %s\n' "$k" "$v"
    fi
  done
  printf '\n[derived]\n'
  printf '  %-24s %s\n' "steps trained"  "$AC_FIRST_STEP .. $AC_FINAL_STEP ($AC_TOTAL_STEPS total)"
  printf '  %-24s %s\n' "module runs"    "$(( N_BATCHES * N_WORKERS ))"
  printf '  %-24s %s\n' "data-prep dir"  "$AC_MASTER_DIR"
  printf '  %-24s %s\n' "module archive" "$AC_MODULE_TGZ"
}

# --- mutation -------------------------------------------------------------

# Rewrites one key in place, preserving comments and layout. Values may span
# lines (DATA_FILES does), so the continuation is consumed too.
ac_set() {
  local key="$1" val="$2" f tmp t
  f=$(ac_conf_file)

  ac_type "$key" | grep -q . || { ac_die "unknown key: $key"; return 1; }
  case "$val" in *'"'*) ac_die "value must not contain a double quote"; return 1 ;; esac

  t=$(ac_type "$key")
  if [ "$t" = list ]; then
    # One entry per line keeps a long selection readable in the file.
    val=$(echo $val | tr ' ' '\n' | sed '2,$s/^/            /')
  fi

  tmp=$(mktemp "${TMPDIR:-/tmp}/alignconf.XXXXXX") || return 1
  awk -v key="$key" -v val="$val" '
    skipping { if ($0 ~ /"/) skipping = 0; next }
    index($0, key "=") == 1 {
      line = $0
      n = gsub(/"/, "", line)
      print key "=\"" val "\""
      if (n < 2) skipping = 1
      found = 1
      next
    }
    { print }
    END { if (!found) exit 3 }
  ' "$f" > "$tmp"

  case $? in
    0) ;;
    3) rm -f "$tmp"; ac_die "key $key not present in $f"; return 1 ;;
    *) rm -f "$tmp"; ac_die "rewrite failed"; return 1 ;;
  esac

  cat "$tmp" > "$f" && rm -f "$tmp"
}
