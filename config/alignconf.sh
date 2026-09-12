#!/bin/bash
# ==========================================================================
#  alignconf.sh -- load, validate and expand the central configuration
# ==========================================================================
#  Sourced by alignctl.sh and by runAll_alignment.sh. Not meant to be run
#  directly.
#
#  Targets bash 4.2 (CentOS 7): no namerefs, no ${var@Q}.
# ==========================================================================

# The module inspection and patch primitives live beside this file.
# shellcheck disable=SC1090
. "$(dirname "${BASH_SOURCE[0]}")/modulepatch.sh"

# Every key the configuration understands, in display order, as
# NAME:TYPE:GROUP. TYPE is one of dir, path, name, int, list, enum, numk
# (a number or keep), intk (a whole number or keep), layers (a comma list
# of layers or keep).
AC_KEYS="
DATA_INPUT_DIR:dir:data
DATA_FILE_PATTERN:name:data
DATA_FILES:list:data
DATA_FILES_PER_BATCH:int:data
DATA_MERGE_MAX_FILES:int:data
MODULE_NAME:name:module
TRACK_SCHEMA:enum:module
MODULE_EVENTS:int:module
MODULE_EPOCHS:int:module
MODULE_JPARALLEL:int:module
MODULE_CORES:int:module
GEOM_BACKEND:enum:module
MODULE_LEARNING_METHOD:enum:module
MODULE_DULEVEL:enum:module
MODULE_LAYERS:layers:module
MODULE_DET_MAG:numk:tuning
MODULE_NTRACKMAX:intk:tuning
MODULE_PT_MIN:numk:tuning
MODULE_PT_MAX:numk:tuning
MODULE_CHI_IB:numk:tuning
MODULE_CHI_OB:numk:tuning
MODULE_CHI_IB_TRAIN:numk:tuning
MODULE_CHI_OB_TRAIN:numk:tuning
MODULE_TRACK_REJECT:numk:tuning
MODULE_IP_RANGE_R:numk:tuning
MODULE_IP_RANGE_Z:numk:tuning
MODULE_MIN_CLUSTER:intk:tuning
MODULE_VERTEX_DERIVATIVES:enum:tuning
MODULE_ETA_CONSTANT:numk:tuning
MODULE_ETA_SCALE:numk:tuning
MODULE_ETA_DETRES:numk:tuning
MODULE_VALID_WINDOW:numk:tuning
MODULE_QUALITY_VERTEXING:numk:tuning
MODULE_QUALITY_TRACKVERTEX:numk:tuning
MODULE_MAX_BAD_TRACKS:intk:tuning
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

# The module knobs are optional in the file: a configuration written before
# they existed still loads, at these defaults. `keep` means the archive's
# own value is used untouched. GEOM_BACKEND has no keep: on a tree that
# carries both backends the choice has to be written into the header.
AC_OPTIONAL="
GEOM_BACKEND=o2
MODULE_LEARNING_METHOD=keep
MODULE_DULEVEL=keep
MODULE_LAYERS=keep
MODULE_DET_MAG=keep
MODULE_NTRACKMAX=keep
MODULE_PT_MIN=keep
MODULE_PT_MAX=keep
MODULE_CHI_IB=keep
MODULE_CHI_OB=keep
MODULE_CHI_IB_TRAIN=keep
MODULE_CHI_OB_TRAIN=keep
MODULE_TRACK_REJECT=keep
MODULE_IP_RANGE_R=keep
MODULE_IP_RANGE_Z=keep
MODULE_MIN_CLUSTER=keep
MODULE_VERTEX_DERIVATIVES=keep
MODULE_ETA_CONSTANT=keep
MODULE_ETA_SCALE=keep
MODULE_ETA_DETRES=keep
MODULE_VALID_WINDOW=keep
MODULE_QUALITY_VERTEXING=keep
MODULE_QUALITY_TRACKVERTEX=keep
MODULE_MAX_BAD_TRACKS=keep
"

ac_keys() { echo "$AC_KEYS" | awk -F: 'NF{print $1}'; }
ac_type() { echo "$AC_KEYS" | awk -F: -v k="$1" '$1==k{print $2}'; }
ac_optional_keys()    { echo "$AC_OPTIONAL" | awk -F= 'NF{print $1}'; }
ac_optional_default() { echo "$AC_OPTIONAL" | awk -F= -v k="$1" '$1==k{print $2}'; }
ac_is_optional()      { [ -n "$(ac_optional_default "$1")" ]; }

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
  local f offenders kv k v
  f=$(ac_conf_file)
  [ -r "$f" ] || { ac_die "cannot read $f"; return 1; }

  offenders=$(grep -nvE '^[[:space:]]*(#|$)|^[A-Z_][A-Z0-9_]*=|^[[:space:]]+[^=]*$' "$f")
  if [ -n "$offenders" ]; then
    ac_die "refusing to source $f -- these lines are not assignments:"
    echo "$offenders" >&2
    return 1
  fi

  # The optional keys get their defaults BEFORE the file is sourced, so the
  # file overrides them if it has them and a variable that happens to be
  # exported in the caller's environment can never stand in for a missing
  # key.
  for kv in $AC_OPTIONAL; do eval "${kv%%=*}=\"${kv#*=}\""; done

  # shellcheck disable=SC1090
  . "$f" || { ac_die "failed to source $f"; return 1; }

  # ac_derive does arithmetic on several of these. Under `set -u` a missing key
  # would abort the shell there with a bare "unbound variable" instead of a
  # message naming the file and the key, so check they are all present first.
  local missing=""
  for k in $(ac_keys); do
    ac_is_optional "$k" && continue
    eval "[ \"\${$k+set}\" = set ]" || missing="$missing $k"
  done
  if [ -n "$missing" ]; then
    ac_die "$f does not define:$missing"
    return 1
  fi

  # An empty module knob reads as its default: the GUI leaves an entry blank
  # for "do not touch", and a hand-written MODULE_X="" means the same thing.
  for k in $(ac_optional_keys); do
    eval "v=\$$k"
    [ -z "$v" ] && eval "$k=\"$(ac_optional_default "$k")\""
  done

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

  # Data-prep macros, overridable for a tree laid out somewhere else. A
  # relative override is resolved against the repository root, not against
  # whatever directory the driver happens to be in when it is used.
  if [ -n "$MASTER_DATA_SCRIPT_DIR" ]; then
    case "$MASTER_DATA_SCRIPT_DIR" in
      /*) AC_MASTER_DIR="$MASTER_DATA_SCRIPT_DIR" ;;
      *)  AC_MASTER_DIR="$AC_ROOT/$MASTER_DATA_SCRIPT_DIR" ;;
    esac
  else
    AC_MASTER_DIR="$AC_ROOT/RUN/MasterDataScript"
  fi

  AC_MERGE_DIR="$AC_ROOT/RUN/MergeParamsScript"

  AC_DATA_FILE_COUNT=$(echo $DATA_FILES | wc -w)
  AC_DATA_FILE_COUNT=${AC_DATA_FILE_COUNT// /}

  # The schema the generated header will carry. 2026 modules read the 2025
  # input tree, so the two resolve alike. `auto` stays empty here: reading
  # it means decompressing the archive, which ac_resolve_schema does for the
  # commands that need it (generate, doctor, show, the driver) and never for
  # set or get.
  case "$TRACK_SCHEMA" in
    2024|2025) AC_TRACK_SCHEMA=$TRACK_SCHEMA ;;
    2026)      AC_TRACK_SCHEMA=2025 ;;
    *)         AC_TRACK_SCHEMA="" ;;
  esac
  AC_SCHEMA_SOURCE=configured

  # Which module knobs the driver will patch, for show and the manifest.
  AC_PATCH_KEYS=""
  local k v
  for k in $(ac_optional_keys); do
    [ "$k" = GEOM_BACKEND ] && continue
    eval "v=\$$k"
    [ "$v" = keep ] || AC_PATCH_KEYS="$AC_PATCH_KEYS $k=$v"
  done
  AC_PATCH_KEYS=${AC_PATCH_KEYS# }
}

# Fills AC_TRACK_SCHEMA for TRACK_SCHEMA=auto from the archive's own
# DataInputStructure.h. The one function outside doctor and inspect that
# opens the archive.
ac_resolve_schema() {
  local hdr
  [ -n "$AC_TRACK_SCHEMA" ] && return 0
  [ -f "$AC_MODULE_TGZ" ] || { ac_die "TRACK_SCHEMA is auto, but there is no module archive to read it from at $AC_MODULE_TGZ"; return 1; }
  hdr=$(tar -xzOf "$AC_MODULE_TGZ" --wildcards '*/Ymlp/inc/DataInputStructure.h' 2>/dev/null)
  [ -n "$hdr" ] || { ac_die "TRACK_SCHEMA is auto, but Ymlp/inc/DataInputStructure.h could not be read from $AC_MODULE_TGZ"; return 1; }
  if echo "$hdr" | grep -qE '^[[:space:]]*int[[:space:]]+charge[[:space:]]*;'; then
    AC_TRACK_SCHEMA=2025
  else
    AC_TRACK_SCHEMA=2024
  fi
  AC_SCHEMA_SOURCE=archive
}

# --- validation -----------------------------------------------------------

_ac_isnum() { echo "$1" | grep -qE '^-?[0-9]+([.][0-9]*)?([eE][-+]?[0-9]+)?$'; }

# Structural checks only: types and relationships between values. Whether
# the paths exist on this machine, and whether the selected module has a
# given knob at all, is ac_doctor's job -- this runs on every `set` and must
# not open the archive.
ac_validate() {
  local k t v m bad=0

  for k in $(ac_keys); do
    t=$(ac_type "$k")
    eval "v=\$$k"
    case "$t" in
      int)
        if ! echo "$v" | grep -qE '^[0-9]+$'; then
          echo "  $k: expected a whole number, got '$v'" >&2; bad=1
        fi ;;
      intk)
        if [ "$v" != keep ] && ! echo "$v" | grep -qE '^[0-9]+$'; then
          echo "  $k: expected a whole number or keep, got '$v'" >&2; bad=1
        fi ;;
      numk)
        if [ "$v" != keep ] && ! _ac_isnum "$v"; then
          echo "  $k: expected a number or keep, got '$v'" >&2; bad=1
        fi ;;
      layers)
        if [ "$v" != keep ]; then
          m=$(mp_layers_mask "$v")
          if [ -z "$m" ]; then
            echo "  $k: '$v' is not a comma-separated list of distinct layers 0..6 (or keep)" >&2; bad=1
          elif [ "$m" -eq 0 ]; then
            echo "  $k: selects no layer -- the run would update nothing" >&2; bad=1
          fi
        fi ;;
      enum)
        case "$k" in
          TRACK_SCHEMA)
            case "$v" in
              auto|2024|2025|2026) ;;
              *) echo "  $k: expected auto, 2024, 2025 or 2026, got '$v'" >&2; bad=1 ;;
            esac ;;
          GEOM_BACKEND)
            case "$v" in
              o2|cache) ;;
              *) echo "  $k: expected o2 or cache, got '$v'" >&2; bad=1 ;;
            esac ;;
          MODULE_LEARNING_METHOD)
            case " keep $MP_METHODS " in
              *" $v "*) ;;
              *) echo "  $k: expected keep or one of: $MP_METHODS; got '$v'" >&2; bad=1 ;;
            esac ;;
          MODULE_DULEVEL)
            case "$v" in
              keep|-1|0|1|2|3|4|5) ;;
              *) echo "  $k: expected keep or -1..5 (-1 whole detector, 0 half-barrel, 1 layer, 2 half-stave, 3 stave, 4 module, 5 chip), got '$v'" >&2; bad=1 ;;
            esac ;;
          MODULE_VERTEX_DERIVATIVES)
            case "$v" in
              keep|TRUE|FALSE) ;;
              *) echo "  $k: expected keep, TRUE or FALSE, got '$v'" >&2; bad=1 ;;
            esac ;;
        esac ;;
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

  # The module knobs, where a value is given. Relations with a `keep` side
  # are checked by doctor against the archive's own value.
  if [ "$MODULE_NTRACKMAX" != keep ] && [ "$MODULE_NTRACKMAX" -lt 2 ]; then
    echo "  MODULE_NTRACKMAX must be at least 2 (a vertex needs two tracks)" >&2; bad=1
  fi
  if [ "$MODULE_DET_MAG" != keep ] && awk -v b="$MODULE_DET_MAG" 'BEGIN{exit !(b+0 == 0)}'; then
    echo "  MODULE_DET_MAG must not be zero -- it divides the momentum estimate" >&2; bad=1
  fi
  for k in MODULE_TRACK_REJECT MODULE_IP_RANGE_R MODULE_IP_RANGE_Z MODULE_CHI_IB MODULE_CHI_OB \
           MODULE_CHI_IB_TRAIN MODULE_CHI_OB_TRAIN MODULE_ETA_DETRES MODULE_VALID_WINDOW; do
    eval "v=\$$k"
    [ "$v" = keep ] && continue
    awk -v x="$v" 'BEGIN{exit !(x+0 > 0)}' || { echo "  $k must be above zero, got '$v'" >&2; bad=1; }
  done
  if [ "$MODULE_PT_MIN" != keep ] && [ "$MODULE_PT_MAX" != keep ]; then
    awk -v lo="$MODULE_PT_MIN" -v hi="$MODULE_PT_MAX" 'BEGIN{exit !(lo+0 < hi+0)}' || {
      echo "  MODULE_PT_MIN ($MODULE_PT_MIN) must be below MODULE_PT_MAX ($MODULE_PT_MAX)" >&2; bad=1; }
  fi
  if [ "$MODULE_CHI_IB_TRAIN" != keep ] && [ "$MODULE_CHI_IB" != keep ]; then
    awk -v a="$MODULE_CHI_IB_TRAIN" -v b="$MODULE_CHI_IB" 'BEGIN{exit !(a+0 > b+0)}' && {
      echo "  MODULE_CHI_IB_TRAIN ($MODULE_CHI_IB_TRAIN) is looser than MODULE_CHI_IB ($MODULE_CHI_IB); the update would accept hits the cost rejects" >&2; bad=1; }
  fi
  if [ "$MODULE_CHI_OB_TRAIN" != keep ] && [ "$MODULE_CHI_OB" != keep ]; then
    awk -v a="$MODULE_CHI_OB_TRAIN" -v b="$MODULE_CHI_OB" 'BEGIN{exit !(a+0 > b+0)}' && {
      echo "  MODULE_CHI_OB_TRAIN ($MODULE_CHI_OB_TRAIN) is looser than MODULE_CHI_OB ($MODULE_CHI_OB)" >&2; bad=1; }
  fi
  if [ "$MODULE_ETA_CONSTANT" != keep ] && [ "$MODULE_ETA_SCALE" != keep ] && [ "$MODULE_ETA_DETRES" != keep ]; then
    awk -v c="$MODULE_ETA_CONSTANT" -v s="$MODULE_ETA_SCALE" -v d="$MODULE_ETA_DETRES" \
        'BEGIN{exit !(c * s * (d*1e-4)^2 > 0)}' || {
      echo "  the eta constants give a learning rate of zero or below; it must be above zero" >&2; bad=1; }
  fi

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

# The track schema, for DataInputStructure.h and DataSplit.C. The 2025 input
# tree carries a per-track charge field; the 2024 tree does not. Getting this
# wrong is not a compile error -- it produces a tree the module misreads -- so
# it is a generated define rather than a hand-edited one. Written from the
# resolved schema: 2026 modules read the 2025 tree, and auto is whatever the
# archive says.
ac_gen_dataschema() {
   local out="$1" has_charge=0
   [ -n "$AC_TRACK_SCHEMA" ] || { ac_die "the track schema is unresolved (TRACK_SCHEMA=$TRACK_SCHEMA); the module archive must be readable to resolve it"; return 1; }
   [ "$AC_TRACK_SCHEMA" = "2025" ] && has_charge=1
   { _ac_banner
     echo "// TRACK_SCHEMA=$TRACK_SCHEMA ($AC_SCHEMA_SOURCE), resolved to $AC_TRACK_SCHEMA"
     echo "#ifndef DATASCHEMA_H"
     echo "#define DATASCHEMA_H"
     echo
     echo "#define ALIGN_TRACK_SCHEMA     $AC_TRACK_SCHEMA"
     echo "#define ALIGN_TRACK_HAS_CHARGE $has_charge"
     echo
     echo "#endif"
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
  ac_resolve_schema || return 1
  ac_gen_datasetconfig "$AC_MASTER_DIR/DataSetConfig.h" || return 1
  echo "wrote $AC_MASTER_DIR/DataSetConfig.h"
  ac_gen_dataschema "$AC_MASTER_DIR/DataSchema.h" || return 1
  echo "wrote $AC_MASTER_DIR/DataSchema.h (track schema $AC_TRACK_SCHEMA, from TRACK_SCHEMA=$TRACK_SCHEMA)"
  # YMLPParallel.h is written per worker at run time, once the module has
  # been unpacked; a copy here documents what the workers will receive.
  ac_gen_ymlpparallel "$AC_ROOT/config/YMLPParallel.h.generated" || return 1
  echo "wrote $AC_ROOT/config/YMLPParallel.h.generated"
}

# --- module inspection ----------------------------------------------------

# Reads the module headers out of an archive into a scratch directory and
# runs mp_inspect on them. Sets AC_PROBE_ROOT to the tree root; the caller
# removes the scratch directory. Every MP_* is assigned even when this
# fails. Not for use inside $(...): the flags must land in the caller.
ac_probe() {          # archive scratchdir
  mp_reset
  AC_PROBE_ROOT=""
  # Warm the member-list cache in this shell; the extraction below runs in
  # a subshell and would otherwise leave it to be read a second time.
  mp_archive_list "$1" >/dev/null || { mp_die "cannot list $1: ${MP_LIST_ERR:-not a readable gzip tar archive}"; return 1; }
  AC_PROBE_ROOT=$(mp_probe_extract "$1" "$2") || { AC_PROBE_ROOT=""; return 1; }
  mp_inspect "$AC_PROBE_ROOT"
}

# The knobs that have a relation to another value, checked with the
# archive's own value on any side left at keep. One message per line on
# stdout, nothing when all is well. Needs mp_inspect to have run; doctor
# reports these as failures and the driver refuses to start on them.
ac_relation_errors() {
  local ptmin ptmax a b c s d
  ptmin=$MODULE_PT_MIN; [ "$ptmin" = keep ] && ptmin=$MP_MOD_PT_MIN
  ptmax=$MODULE_PT_MAX; [ "$ptmax" = keep ] && ptmax=$MP_MOD_PT_MAX
  if [ -n "$ptmin" ] && [ -n "$ptmax" ] && ! awk -v lo="$ptmin" -v hi="$ptmax" 'BEGIN{exit !(lo+0 < hi+0)}'; then
    echo "pT window [$ptmin, $ptmax] is empty -- MODULE_PT_MIN/MODULE_PT_MAX against the archive's own values"
  fi
  a=$MODULE_CHI_IB_TRAIN; [ "$a" = keep ] && a=$MP_MOD_CHI_IB_TRAIN
  b=$MODULE_CHI_IB;       [ "$b" = keep ] && b=$MP_MOD_CHI_IB
  if [ -n "$a" ] && [ -n "$b" ] && awk -v a="$a" -v b="$b" 'BEGIN{exit !(a+0 > b+0)}'; then
    echo "RANGE_CHI_IB_TRAINING ($a) would be looser than RANGE_CHI_IB ($b); the update would accept hits the cost rejects"
  fi
  a=$MODULE_CHI_OB_TRAIN; [ "$a" = keep ] && a=$MP_MOD_CHI_OB_TRAIN
  b=$MODULE_CHI_OB;       [ "$b" = keep ] && b=$MP_MOD_CHI_OB
  if [ -n "$a" ] && [ -n "$b" ] && awk -v a="$a" -v b="$b" 'BEGIN{exit !(a+0 > b+0)}'; then
    echo "RANGE_CHI_OB_TRAINING ($a) would be looser than RANGE_CHI_OB ($b)"
  fi
  c=$MODULE_ETA_CONSTANT; [ "$c" = keep ] && c=$MP_MOD_ETA_CONSTANT
  s=$MODULE_ETA_SCALE;    [ "$s" = keep ] && s=$MP_MOD_ETA_SCALE
  d=$MODULE_ETA_DETRES;   [ "$d" = keep ] && d=$MP_MOD_ETA_DETRES
  if [ -n "$c" ] && [ -n "$s" ] && [ -n "$d" ] && ! awk -v c="$c" -v s="$s" -v d="$d" 'BEGIN{exit !(c * s * (d*1e-4)^2 > 0)}'; then
    echo "the effective eta is zero or below (constant $c, scale $s, DETRES $d); nothing would be learned"
  fi
}

# The capability report for `alignctl.sh inspect [ARCHIVE]`: NAME=value
# lines only, no colour, so the GUI can read it. Exit status 0 when the
# archive was read, 1 when it was not (the MP_* lines then say "nothing").
ac_inspect() {        # [archive]
  local a="${1:-$AC_MODULE_TGZ}" probe top want rc=0 err=""
  if [ ! -f "$a" ]; then
    mp_reset
    echo "MP_ARCHIVE=$a"
    echo "MP_ERROR=no archive at $a"
    echo "MP_TOP="; echo "MP_TOP_OK=0"; echo "MP_CACHE_FILE=0"
    mp_report
    return 1
  fi
  if ! probe=$(mktemp -d "${TMPDIR:-/tmp}/alignprobe.XXXXXX"); then
    mp_reset
    echo "MP_ARCHIVE=$a"
    echo "MP_ERROR=cannot create a scratch directory under ${TMPDIR:-/tmp}"
    echo "MP_TOP="; echo "MP_TOP_OK=0"; echo "MP_CACHE_FILE=0"
    mp_report
    return 1
  fi
  if ! ac_probe "$a" "$probe" 2>"$probe/probe.err"; then
    err=$(tr '\n' ' ' < "$probe/probe.err")
    rc=1
  fi
  top=$(mp_archive_top "$a")
  # For the configured archive the name the driver enters is MODULE_NAME;
  # for any other archive it is what MODULE_NAME would become.
  case "$a" in
    "$AC_MODULE_TGZ") want=$MODULE_NAME ;;
    *) want=$(basename "$a" .tgz) ;;
  esac
  echo "MP_ARCHIVE=$a"
  [ -n "$err" ] && echo "MP_ERROR=$err"
  echo "MP_TOP=$top"
  echo "MP_TOP_OK=$([ -n "$top" ] && [ "$top" = "$want" ] && echo 1 || echo 0)"
  echo "MP_CACHE_FILE=$([ -n "$top" ] && mp_archive_member "$a" "$top" "$MP_GEOMCACHE" >/dev/null && echo 1 || echo 0)"
  mp_report
  rm -rf "$probe"
  return $rc
}

# --- environment checks ---------------------------------------------------

# The counters default to zero so these can be used outside ac_doctor under
# `set -u` without aborting the shell.
_ac_ok()   { printf '  \033[32mok\033[0m    %s\n' "$1"; }
_ac_warn() { printf '  \033[33mwarn\033[0m  %s\n' "$1"; AC_DOCTOR_WARN=$(( ${AC_DOCTOR_WARN:-0} + 1 )); }
_ac_bad()  { printf '  \033[31mFAIL\033[0m  %s\n' "$1"; AC_DOCTOR_FAIL=$(( ${AC_DOCTOR_FAIL:-0} + 1 )); }

# The module section of doctor, once the archive has been read. Everything
# here is gated on MP_VALID by the caller.
_ac_doctor_module() { # treeroot scratchdir
  local root="$1" probe="$2" top line eff_method eff_dulevel eff_mask geom_ok=1 ptmin ptmax a b c s d
  top=$(mp_archive_top "$AC_MODULE_TGZ")

  # The driver unpacks the archive and enters MODULE_NAME/.
  if [ "$top" = "$MODULE_NAME" ]; then
    _ac_ok "archive unpacks to $top/"
  else
    _ac_bad "archive unpacks to '$top/' but MODULE_NAME is '$MODULE_NAME' -- the driver enters MODULE_NAME/ after unpacking; repack it as 'tar czf $MODULE_NAME.tgz $MODULE_NAME'"
  fi
  _ac_ok "generation $MP_GENERATION: input schema $MP_SCHEMA, detector-unit=$MP_DETECTOR_UNIT, layer-select=$MP_LAYER_SELECT, adaptive-vertex=$MP_ADAPTIVE_VERTEX, cache-capable=$MP_CACHE_CAPABLE"
  # The launch chain is process_all_master.sh -> process_all_train.sh ->
  # process.sh. The repositories track them without the execute bit; the
  # driver sets it after unpacking, so this is information, not a failure.
  local noexec=""
  for f in process_all_master.sh process_all_train.sh process.sh; do
    if ! tar -tzvf "$AC_MODULE_TGZ" 2>/dev/null | grep -E "[[:space:]](\./)?$(printf '%s' "$top" | sed 's|[.[\*^$/+?(){}|]|\\&|g')/$f\$" | grep -q '^-..x'; then
      noexec="$noexec $f"
    fi
  done
  if [ -n "$noexec" ]; then
    _ac_ok "launch scripts not executable in the archive:$noexec -- the driver sets the bit after unpacking"
  else
    _ac_ok "launch scripts executable in the archive"
  fi
  _ac_ok "learning methods this tree implements: ${MP_METHODS_IMPL:-none found}; its driver macro ships ${MP_MOD_METHOD:-?}"

  # The module and the split files must agree about the track schema. The
  # module carries its own copy of the structure, so ask the archive rather
  # than trusting the name. Compared on the resolved value: 2026 reads the
  # 2025 tree, and auto is by definition what the archive says.
  if ! ac_resolve_schema 2>/dev/null; then
    _ac_bad "TRACK_SCHEMA is auto but could not be read from the archive"
  elif [ "$TRACK_SCHEMA" = auto ]; then
    _ac_ok "track schema auto -> $AC_TRACK_SCHEMA (read from the archive)"
  elif [ "$AC_TRACK_SCHEMA" = "$MP_SCHEMA" ]; then
    _ac_ok "track schema $TRACK_SCHEMA (resolved $AC_TRACK_SCHEMA) matches the module archive"
    # 2025 and 2026 read the same tree, so the year cannot be checked through
    # the schema; say so when it disagrees with what the archive looks like.
    [ "$TRACK_SCHEMA" = "$MP_GENERATION" ] || \
      _ac_warn "TRACK_SCHEMA says $TRACK_SCHEMA but the archive looks like a $MP_GENERATION-generation module -- same input tree, so harmless; check MODULE_NAME is the archive you meant"
  else
    _ac_bad "TRACK_SCHEMA is $TRACK_SCHEMA (resolved $AC_TRACK_SCHEMA) but the module archive expects $MP_SCHEMA -- the split files would be misread"
  fi

  # Geometry backend. O2 is needed on this host in every mode: the Manager's
  # own DataSplit.C and WeightsMerge.C use it. `cache` only takes O2 out of
  # the worker's geometry, and needs the cache file to travel in the archive
  # so a run is reproducible from MODULE_NAME alone.
  case "$GEOM_BACKEND" in
    o2)
      if [ "$MP_CACHE_CAPABLE" -eq 1 ]; then
        _ac_ok "backend o2 -- this tree ships as the cache backend, so the O2 guard is written into every worker's YDetectorGeometry.h"
      else
        _ac_ok "backend o2 (the only backend this tree has; its header is left untouched)"
      fi ;;
    cache)
      # An O2-only tree is reported by the knob check below (mp_apply refuses
      # it); here only what that check cannot see.
      if [ "$MP_CACHE_CAPABLE" -ne 1 ]; then
        geom_ok=0
      elif ! mp_archive_member "$AC_MODULE_TGZ" "$top" "$MP_GEOMCACHE" >/dev/null; then
        geom_ok=0
        _ac_bad "no $top/$MP_GEOMCACHE in the archive -- geometry/ is gitignored in the 2026 tree; run tools/export_geometry_cache.C in the tree and repack, or set GEOM_BACKEND=o2"
      else
        _ac_ok "backend cache -- $MP_GEOMCACHE travels in the archive; workers read it instead of O2"
        # The cache bakes its alignment in and never re-reads it; a cache
        # exported from another ITSAlignment.root would silently train on a
        # different geometry than the same archive under o2.
        if command -v root >/dev/null 2>&1; then
          mp_probe_extract_cache "$AC_MODULE_TGZ" "$probe" 2>/dev/null
          local cfp="" ffp=""
          cfp=$(root -l -b -q -e "auto f=TFile::Open(\"$root/$MP_GEOMCACHE\"); auto o=f?(TNamed*)f->Get(\"alignfingerprint\"):0; printf(\"AC_FP %s\\n\", f?(o?o->GetTitle():\"none\"):\"unreadable\")" 2>/dev/null | sed -n 's/^AC_FP //p')
          ffp=$(mp_align_fingerprint "$root")
          if [ -z "$cfp" ] || [ "$cfp" = unreadable ]; then
            _ac_warn "root could not open the archive's $MP_GEOMCACHE; cache staleness unchecked"
          elif [ "$cfp" = none ]; then
            _ac_warn "the cache carries no alignment fingerprint -- rebuild it with tools/export_geometry_cache.C to enable the staleness check"
          elif [ -z "$ffp" ] || [ "$ffp" = unreadable ]; then
            _ac_warn "cannot fingerprint the archive's $MP_ALIGNFILE; cache staleness unchecked"
          elif [ "$cfp" = "$ffp" ]; then
            _ac_ok "cache matches the archive's $MP_ALIGNFILE ($cfp)"
          else
            _ac_bad "the geometry cache was built from a DIFFERENT alignment than the archive's $MP_ALIGNFILE (cache $cfp, file $ffp) -- rebuild it and repack"
          fi
        else
          _ac_warn "cache staleness unchecked (root not on PATH)"
        fi
      fi
      [ "$geom_ok" -eq 1 ] && _ac_ok "O2 is still loaded for the run: DataSplit.C and WeightsMerge.C need it whatever the worker's backend" ;;
  esac

  # Every knob that is not keep, tried on the scratch copy exactly as the
  # driver will apply it to each worker: an unsupported knob fails here
  # instead of after the data preparation.
  if mp_apply "$root" 2>"$probe/apply.err"; then
    if [ -n "$MP_APPLIED" ]; then
      while IFS= read -r line; do [ -n "$line" ] && _ac_ok "will patch each worker: $line"; done <<EOF
$MP_APPLIED
EOF
    else
      _ac_ok "no module knob set; every worker trains the archive as shipped"
    fi
  else
    while IFS= read -r line; do _ac_bad "${line#modulepatch: }"; done < "$probe/apply.err"
  fi

  # What the run will actually use, for the interaction checks: the
  # configured value where one is given, the archive's own otherwise. Both
  # knobs act in MLP_BatchArr, which kBatch and kSteepestDescent go through
  # and kStochastic does not.
  eff_method=$MODULE_LEARNING_METHOD; [ "$eff_method" = keep ] && eff_method=$MP_MOD_METHOD
  eff_dulevel=$MODULE_DULEVEL;        [ "$eff_dulevel" = keep ] && eff_dulevel=${MP_MOD_DULEVEL:-5}
  if [ "$MP_DETECTOR_UNIT" -eq 1 ]; then
    _ac_ok "DULEVEL $eff_dulevel ($(mp_dulevel_name "$eff_dulevel")) -- the unit one set of six parameters is pooled over"
    if ! mp_is_batch_method "$eff_method" && [ "$eff_dulevel" != 5 ]; then
      _ac_warn "DULEVEL $eff_dulevel has no effect under $eff_method -- pooling a unit needs the batch update (kBatch or kSteepestDescent)"
    fi
  fi
  if [ "$MP_LAYER_SELECT" -eq 1 ]; then
    if [ "$MODULE_LAYERS" != keep ]; then eff_mask=$(mp_layers_mask "$MODULE_LAYERS"); else eff_mask=$(( MP_MOD_LAYER_MASK )); fi
    _ac_ok "layers $(mp_mask_layers "$eff_mask") ($(mp_layers_preset "$eff_mask")) -- $(mp_layers_sensors "$eff_mask") of 24120 chips aligned, mask $(printf '0x%02X' "$eff_mask"); chips outside are averaged through the merge unchanged"
    if ! mp_is_batch_method "$eff_method"; then
      _ac_warn "layers have no effect under $eff_method -- the mask gates the batch update (kBatch, kSteepestDescent); kStochastic updates all seven layers"
    fi
  fi

  # Relations between a configured value and the archive's own; the driver
  # refuses to start on the same list.
  while IFS= read -r line; do [ -n "$line" ] && _ac_bad "$line"; done <<EOF
$(ac_relation_errors)
EOF
  ptmin=$MODULE_PT_MIN; [ "$ptmin" = keep ] && ptmin=$MP_MOD_PT_MIN
  ptmax=$MODULE_PT_MAX; [ "$ptmax" = keep ] && ptmax=$MP_MOD_PT_MAX
  [ -n "$ptmin" ] && [ -n "$ptmax" ] && awk -v lo="$ptmin" -v hi="$ptmax" 'BEGIN{exit !(lo+0 < hi+0)}' \
    && _ac_ok "pT window [$ptmin, $ptmax] GeV/c"
  c=$MODULE_ETA_CONSTANT; [ "$c" = keep ] && c=$MP_MOD_ETA_CONSTANT
  s=$MODULE_ETA_SCALE;    [ "$s" = keep ] && s=$MP_MOD_ETA_SCALE
  d=$MODULE_ETA_DETRES;   [ "$d" = keep ] && d=$MP_MOD_ETA_DETRES
  [ -n "$c" ] && [ -n "$s" ] && [ -n "$d" ] && awk -v c="$c" -v s="$s" -v d="$d" 'BEGIN{exit !(c * s * (d*1e-4)^2 > 0)}' \
    && _ac_ok "eta $(awk -v c="$c" -v s="$s" -v d="$d" 'BEGIN{printf "%.6g", c * s * (d*1e-4)^2}') (constant $c, scale $s, DETRES $d um)"
  a=$MODULE_DET_MAG; [ "$a" = keep ] && a=$MP_MOD_DET_MAG
  b=$MODULE_NTRACKMAX; [ "$b" = keep ] && b=$MP_MOD_NTRACKMAX
  [ -n "$a" ] && _ac_ok "DET_MAG $a T, nTrackMax $b"

  # Every step reseeds from the base archive with only weights.txt replaced,
  # so weightsDU.txt and UpdateSensorsList.txt are always the reference's.
  if [ "$MP_DU_PERSIST" -eq 1 ]; then
    if [ -f "$AC_REFERENCE_TGZ" ]; then
      if tar -tzf "$AC_REFERENCE_TGZ" 2>/dev/null | grep -q '/weights/weightsDU\.txt$'; then
        _ac_ok "this tree reloads weights/weightsDU.txt each step; the reference archive carries it (reseeded from there every batch)"
      else
        _ac_warn "this tree reloads weights/weightsDU.txt each step, but the reference archive has none -- the module reads it from a stream that failed to open (the 2026 console's doctor reports the cost then comes out -nan)"
      fi
    fi
  else
    _ac_ok "this tree neither reads nor writes weightsDU.txt; the weights.txt-only hand-off loses nothing"
  fi
}

ac_doctor() {
  local f missing=0 present=0 probe="" est avail need
  AC_DOCTOR_FAIL=0
  AC_DOCTOR_WARN=0
  mp_reset

  echo "paths"
  [ -d "$DATA_INPUT_DIR" ] && _ac_ok "DATA_INPUT_DIR" || _ac_bad "DATA_INPUT_DIR does not exist: $DATA_INPUT_DIR"
  [ -d "$O2_DIR" ]         && _ac_ok "O2_DIR"         || _ac_bad "O2_DIR does not exist: $O2_DIR (O2 is needed in every backend: the data split and the merge use it)"
  [ -d "$AC_MASTER_DIR" ]  && _ac_ok "data-prep macros at $AC_MASTER_DIR" \
                           || _ac_bad "no data-prep macro directory at $AC_MASTER_DIR"
  [ -d "$AC_MERGE_DIR" ]   && _ac_ok "merge macros" || _ac_bad "no merge macro directory at $AC_MERGE_DIR"

  # DataRandomMerge.C stages its symlinks here and lists the directory to
  # build its file list; nothing in the tree creates it.
  if [ -d "$AC_MASTER_DIR/MasterData" ]; then
    _ac_ok "MasterData staging directory"
  else
    _ac_bad "missing $AC_MASTER_DIR/MasterData -- it is tracked, so it was removed; restore it with 'git checkout -- $AC_MASTER_DIR/MasterData' or ./run_dir_maker.sh"
  fi

  # DataRandomMerge.C includes this; it is generated, not committed.
  if [ -f "$AC_MASTER_DIR/DataSetConfig.h" ]; then
    _ac_ok "DataSetConfig.h present"
  else
    _ac_bad "missing $AC_MASTER_DIR/DataSetConfig.h -- run 'alignctl.sh generate'"
  fi
  if [ -f "$AC_MASTER_DIR/DataSchema.h" ]; then
    _ac_ok "DataSchema.h present"
  else
    _ac_bad "missing $AC_MASTER_DIR/DataSchema.h -- run 'alignctl.sh generate'"
  fi

  echo "archives"
  [ -f "$AC_REFERENCE_TGZ" ] && _ac_ok "reference parameters for step $BASE_STEP" \
                             || _ac_bad "no reference archive at $AC_REFERENCE_TGZ"
  if [ -f "$AC_MODULE_TGZ" ]; then
    _ac_ok "module archive $MODULE_NAME.tgz"
    if ! probe=$(mktemp -d "${TMPDIR:-/tmp}/alignprobe.XXXXXX"); then
      _ac_bad "cannot create a scratch directory under ${TMPDIR:-/tmp}; the archive was not inspected"
    elif ac_probe "$AC_MODULE_TGZ" "$probe" 2>"$probe/probe.err" && [ "$MP_VALID" -eq 1 ]; then
      _ac_doctor_module "$AC_PROBE_ROOT" "$probe"
    else
      _ac_bad "could not read the module out of the archive: $(sed 's/^modulepatch: //' "$probe/probe.err" 2>/dev/null | tr '\n' ' ')"
    fi
    [ -n "$probe" ] && rm -rf "$probe"
  else
    _ac_bad "no module archive at $AC_MODULE_TGZ"
    [ "$TRACK_SCHEMA" = auto ] && _ac_bad "TRACK_SCHEMA is auto, which needs the archive to resolve; generate and the driver will refuse until it is there"
  fi

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

  echo "machine"
  command -v root >/dev/null 2>&1 && _ac_ok "root on PATH" \
    || _ac_warn "root not on PATH -- the driver loads the O2 environment itself; load it here to run the window or the fingerprint check"
  # A worker holds roughly 8 GB resident; the workers run at the same time.
  avail=$(awk '/MemAvailable/{printf "%.1f", $2/1048576}' /proc/meminfo 2>/dev/null)
  need=$(( N_WORKERS * 8 ))
  if [ -n "$avail" ]; then
    awk -v g="$avail" -v n="$need" 'BEGIN{exit !(g > n)}' \
      && _ac_ok "${avail} GB available; ${N_WORKERS} workers hold ~${need} GB resident" \
      || _ac_warn "${avail} GB available, but ${N_WORKERS} workers hold ~${need} GB resident (about 8 GB each); lower N_WORKERS or expect the OOM killer"
  fi

  echo
  # The runtime fit is the 2026 console's, for that module only.
  if [ "${MP_GENERATION:-}" = 2026 ]; then
    est=$(mp_estimate "$MODULE_EVENTS" "$MODULE_EPOCHS")
    [ -n "$est" ] && echo "estimated training  ~$(mp_minutes_text "$est") per step per worker, ~$(mp_minutes_text $(( est * STEPS_PER_BATCH * N_BATCHES ))) for the run (fit on the 2026 module; data preparation and merges extra)"
  fi
  echo "$AC_DOCTOR_FAIL failed, $AC_DOCTOR_WARN warnings"
  [ "$AC_DOCTOR_FAIL" -eq 0 ]
}

# --- display --------------------------------------------------------------

ac_print() {
  local k t v group last="" est
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
  if [ -n "$AC_TRACK_SCHEMA" ]; then
    printf '  %-24s %s\n' "track schema" "$AC_TRACK_SCHEMA (from TRACK_SCHEMA=$TRACK_SCHEMA)"
  else
    printf '  %-24s %s\n' "track schema" "auto -- read from the archive by generate, doctor and the driver"
  fi
  printf '  %-24s %s\n' "module patches"  "${AC_PATCH_KEYS:-none -- every knob is keep}"
  est=$(mp_estimate "$MODULE_EVENTS" "$MODULE_EPOCHS")
  [ -n "$est" ] && printf '  %-24s %s\n' "estimated training" "~$(mp_minutes_text "$est") per step per worker if the module is a 2026 tree (that is what the fit covers)"
}

# --- mutation -------------------------------------------------------------

# Rewrites one key in place, preserving comments and layout. Values may span
# lines (DATA_FILES does), so the continuation is consumed too. A module
# knob absent from an older file is appended; a mandatory key must be there.
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
  # The value goes through the environment, not -v: awk applies escape-sequence
  # processing to a -v assignment, so a value containing \t or \\ would arrive
  # changed.
  AC_SET_VALUE="$val" awk -v key="$key" '
    BEGIN { val = ENVIRON["AC_SET_VALUE"] }
    skipping { if ($0 ~ /"/) skipping = 0; next }
    index($0, key "=") == 1 {
      line = $0
      n = gsub(/"/, "", line)
      print key "=\"" val "\""
      # An odd number of quotes means the value is still open and continues on
      # the following lines, so they belong to it and are replaced too. An even
      # number -- including a value written without quotes at all -- means the
      # line stands alone, and skipping past it would eat the next setting.
      if (n % 2 == 1) skipping = 1
      found = 1
      next
    }
    { print }
    END { if (!found) exit 3 }
  ' "$f" > "$tmp"

  case $? in
    0) ;;
    3)
      rm -f "$tmp"
      ac_is_optional "$key" || { ac_die "key $key not present in $f"; return 1; }
      # A file without a final newline would otherwise glue the new key onto
      # its last line.
      [ -s "$f" ] && [ -n "$(tail -c1 "$f")" ] && echo >> "$f"
      grep -q '^# --- module knobs (added by alignctl.sh set) ---' "$f" \
        || printf '\n# --- module knobs (added by alignctl.sh set) ---\n' >> "$f"
      printf '%s="%s"\n' "$key" "$val" >> "$f"
      return 0 ;;
    *) rm -f "$tmp"; ac_die "rewrite failed"; return 1 ;;
  esac

  cat "$tmp" > "$f" && rm -f "$tmp"
}
