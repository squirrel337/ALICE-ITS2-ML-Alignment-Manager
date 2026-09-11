#!/bin/bash
# ==========================================================================
#  modulepatch.sh -- inspect a module tree and patch what its archive freezes
# ==========================================================================
#  Sourced by alignconf.sh. Not meant to be run directly.
#
#  The module's physics configuration is a set of #defines and file-scope
#  constants inside the frozen archive. This file knows where each one
#  lives, whether a given tree has it at all, and how to rewrite it in the
#  worker's unpacked copy -- the archive in MODULE/ is never modified.
#
#  Every patch is gated on what the tree actually contains, discovered by
#  reading it rather than assumed from its name, so one code path drives the
#  2024, 2025 and 2026 module generations. A knob left at `keep` is not
#  touched, and the four patchable files are restored from the archive
#  before patching, so `keep` always means the archive's own value even in
#  a worker directory an earlier run patched.
#
#  The patch primitives are the ones the 2026 tree's own run console uses
#  (ALICE-ITS2-ML-Alignment-2026, config/runconf.sh, rc_patch_*), carried
#  over with an mp_ prefix so both tools rewrite the same lines the same way.
#  Only the primitives are carried over: YMLPParallel.h keeps being written
#  by ac_gen_ymlpparallel, and FITMODEL/VERTEXFIT are never touched (the
#  2024 tree spells the latter kFALSE and passes it as a Bool_t).
#
#  Targets bash 4.2 (CentOS 7) and GNU tar 1.26: no namerefs, no ${var@Q}.
# ==========================================================================

MP_PARALLEL="YMLPParallel.h"
MP_DETCONST="Ymlp/inc/DetectorConstant.h"
MP_DATAHDR="Ymlp/inc/DataInputStructure.h"
MP_GEOMHDR="Ymlp/inc/YDetectorGeometry.h"
MP_GEOMSRC="Ymlp/src/YDetectorGeometry.cxx"
MP_MLPSRC="Ymlp/src/YMultiLayerPerceptron.cxx"
MP_ALIGNSRC="Ymlp/src/YAlignment.cxx"
MP_DRIVER="run_train_circle.C"
MP_GEOMCACHE="geometry/its2_geom.root"
MP_ALIGNFILE="ITSAlignment.root"
MP_FPTOOL="tools/align_fingerprint.C"

# Read by inspect. All eight exist in every supported generation; the probe
# requires the extraction to succeed and treats any tar failure as an
# unreadable archive.
MP_PROBE_FILES="$MP_PARALLEL $MP_DETCONST $MP_DATAHDR $MP_GEOMHDR $MP_GEOMSRC $MP_MLPSRC $MP_ALIGNSRC $MP_DRIVER"
# The four a patch may rewrite; restored from the archive before every pass.
MP_PATCH_FILES="$MP_DETCONST $MP_GEOMHDR $MP_MLPSRC $MP_DRIVER"

# Same marker the 2026 console writes, so a tree composed by either tool is
# cleaned up by the other instead of accumulating a second define.
MP_GEOM_MARK="// --- run console: geometry backend ---"

# The learning methods YMultiLayerPerceptron.h declares. Which of them a
# given tree actually implements is read from its Train() switch.
MP_METHODS="kStochastic kBatch kBatchDetectorUnitUser kSteepestDescent kRibierePolak kFletcherReeves kBFGS kOffsetTuneByMean"

# Chips per layer, for the layer-selection summary.
MP_LAYER_SENSORS="108 144 180 2688 3360 8232 9408"

# Configuration key -> #define in DetectorConstant.h, for the numeric knobs.
MP_DEFINE_KEYS="
MODULE_DET_MAG:DET_MAG
MODULE_NTRACKMAX:nTrackMax
MODULE_PT_MIN:Update_pTmin
MODULE_PT_MAX:Update_pTmax
MODULE_CHI_IB:RANGE_CHI_IB
MODULE_CHI_OB:RANGE_CHI_OB
MODULE_CHI_IB_TRAIN:RANGE_CHI_IB_TRAINING
MODULE_CHI_OB_TRAIN:RANGE_CHI_OB_TRAINING
MODULE_TRACK_REJECT:TrackRejection
MODULE_IP_RANGE_R:RANGE_IMPACTPARAMS_R
MODULE_IP_RANGE_Z:RANGE_IMPACTPARAMS_Z
MODULE_MIN_CLUSTER:Min_Cluster_by_Sensor
MODULE_VERTEX_DERIVATIVES:VERTEX_DERIVATIVES
"
# Configuration key -> file-scope double in YMultiLayerPerceptron.cxx.
MP_GLOBAL_KEYS="
MODULE_ETA_CONSTANT:UpdateConstant
MODULE_ETA_SCALE:UpdateScale
MODULE_ETA_DETRES:DETRES
MODULE_VALID_WINDOW:ValidWindow
MODULE_QUALITY_VERTEXING:QUALITY_VERTEXING
MODULE_QUALITY_TRACKVERTEX:QUALITY_TRACKVERTEX
"

mp_die() { echo "modulepatch: $*" >&2; return 1; }

# --- archive access --------------------------------------------------------

# The member list, read once per archive and reused: every question below
# (top directory, which files exist, is the geometry cache there) is answered
# from it, so an archive is decompressed once for the listing and once for
# the extraction, never per file.
mp_archive_list() {   # archive -> member names, one per line
  if [ "${MP_LIST_FOR:-}" != "$1" ]; then
    MP_LIST=$(tar -tzf "$1" 2>/dev/null) || { MP_LIST=""; MP_LIST_FOR=""; return 1; }
    MP_LIST_FOR="$1"
  fi
  printf '%s\n' "$MP_LIST"
}

# First path component of the first member, with any leading ./ dropped.
# The driver unpacks the archive and enters MODULE_NAME/, so this must equal
# MODULE_NAME; doctor checks it. Archives packed as `tar czf X.tgz ./X`
# store members as ./X/..., which tar unpacks to X/ exactly like X/...; the
# member names below are always taken from the listing, never composed.
mp_archive_top() {    # archive -> top directory name
  mp_archive_list "$1" | sed -n '1{s|^\./||;s|/.*||;p;}'
}

# The member name for one path inside the tree, exactly as the archive
# spells it, or nothing if it is absent.
mp_archive_member() { # archive top relpath -> member name
  # Fixed strings: module names carry '.' and '+', which a pattern would read.
  mp_archive_list "$1" | grep -F -x -m1 -e "$2/$3" -e "./$2/$3"
}

# Extracts the probe files into DESTDIR and prints the tree root
# (DESTDIR/<top>). One tar call naming exactly the members that are present,
# so tar's exit status means what it says: non-zero is an unreadable archive.
mp_probe_extract() {  # archive destdir -> tree root on stdout
  local a="$1" d="$2" top f m members=""
  top=$(mp_archive_top "$a")
  [ -n "$top" ] || { mp_die "cannot list $a"; return 1; }
  for f in $MP_PROBE_FILES; do
    m=$(mp_archive_member "$a" "$top" "$f") && members="$members $m"
  done
  [ -n "$members" ] || { mp_die "$a holds none of the module files (top directory $top)"; return 1; }
  mkdir -p "$d" || return 1
  # shellcheck disable=SC2086
  tar -xzf "$a" -C "$d" $members || { mp_die "could not extract the module headers from $a"; return 1; }
  echo "$d/$top"
}

# The cache-mode files, wanted only by doctor's staleness check: the cache
# itself, the alignment the o2 backend would apply, and the module's own
# fingerprint macro. Missing members are simply not extracted.
mp_probe_extract_cache() { # archive destdir
  local a="$1" d="$2" top f m members=""
  top=$(mp_archive_top "$a") || return 1
  for f in $MP_GEOMCACHE $MP_ALIGNFILE $MP_FPTOOL; do
    m=$(mp_archive_member "$a" "$top" "$f") && members="$members $m"
  done
  [ -n "$members" ] || return 0
  # shellcheck disable=SC2086
  tar -xzf "$a" -C "$d" $members
}

# Puts the archive's own copy of every patchable file back into an unpacked
# worker tree. PARENT is the directory the archive was unpacked in, so the
# members land exactly where the unpack put them.
mp_restore() {        # archive parentdir
  local a="$1" parent="$2" top f m members=""
  top=$(mp_archive_top "$a")
  [ -n "$top" ] || { mp_die "cannot list $a"; return 1; }
  for f in $MP_PATCH_FILES; do
    m=$(mp_archive_member "$a" "$top" "$f") && members="$members $m"
  done
  # shellcheck disable=SC2086
  [ -n "$members" ] && { tar -xzf "$a" -C "$parent" $members || { mp_die "could not restore module files from $a"; return 1; }; }
  return 0
}

# --- reading a tree ---------------------------------------------------------

# The first value of one #define in a header, or empty.
mp_read_define() {    # file name
  [ -r "$1" ] || return 0
  awk -v n="$2" '$1=="#define" && $2==n {print $3; exit}' "$1"
}
mp_has_define() {     # file name
  [ -n "$(mp_read_define "$1" "$2")" ]
}

# A file-scope `[const ]double NAME = value;`, or empty.
mp_read_global() {    # file name
  [ -r "$1" ] || return 0
  awk -v n="$2" '
    $0 ~ ("^(const[[:space:]]+)?double[[:space:]]+" n "[[:space:]]*=") {
      sub(/^[^=]*=[[:space:]]*/, ""); sub(/[[:space:]]*;.*$/, ""); print; exit }' "$1"
}

# The bad-prong gate is a literal in an if(), so read it where it is written.
mp_read_badtracks() { # file
  [ -r "$1" ] || return 0
  sed -n 's|.*if(Num_Of_Bad_Tracks>\([0-9][0-9]*\)).*|\1|p' "$1" | head -n1
}

# The learning methods this tree really trains with. Every generation
# declares all eight enumerators, but Train()'s switch implements only some:
# kRibierePolak, kFletcherReeves and kBFGS are empty cases everywhere, and
# the 2026 tree dropped kBatchDetectorUnitUser. A case counts as implemented
# when its body calls something. kOffsetTuneByMean takes a separate branch
# whose only update is compiled under MONITORSENSORUNITprofile.
mp_read_methods() {   # mlpsrc detconst -> implemented methods, space separated
  local impl
  [ -r "$1" ] || return 0
  impl=$(awk '
    function flush() { if (cur != "" && body) printf "%s ", cur; cur = ""; body = 0 }
    /switch[[:space:]]*\([[:space:]]*fLearningMethod[[:space:]]*\)/ { insw = 1; next }
    insw && /^[[:space:]]*case[[:space:]]+YMultiLayerPerceptron::k[A-Za-z]+[[:space:]]*:/ {
      flush(); match($0, /k[A-Za-z]+/); cur = substr($0, RSTART, RLENGTH); next }
    insw && /^[[:space:]]*default[[:space:]]*:/ { flush(); insw = 0; next }
    insw && cur != "" && $0 !~ /^[[:space:]]*\/\// && $0 ~ /[A-Za-z_][A-Za-z_0-9]*[[:space:]]*\(/ { body = 1 }
    END { flush() }' "$1")
  if grep -qE '^[[:space:]]*#define[[:space:]]+MONITORSENSORUNITprofile' "$2" 2>/dev/null \
     && grep -q 'kOffsetTuneByMean' "$1"; then
    impl="$impl kOffsetTuneByMean"
  fi
  echo $impl | tr ' ' '\n' | awk '!seen[$0]++' | tr '\n' ' ' | sed 's/ *$//'
}

# Every MP_* variable, at the value that means "nothing known". Called first
# by mp_inspect and on its own when the probe fails, so a caller under
# `set -u` can always read the flags.
mp_reset() {
  MP_VALID=0 MP_HAS_CHARGE=0 MP_SCHEMA="" MP_CACHE_CAPABLE=0 MP_O2_REQUIRED=0 MP_GEOM_SHIPPED=""
  MP_MOD_NDATA="" MP_MOD_NEPOCH="" MP_MOD_NTRACKMAX="" MP_MOD_DET_MAG=""
  MP_MOD_METHOD="" MP_HAS_METHOD_LINE=0 MP_METHODS_IMPL=""
  MP_MOD_DULEVEL="" MP_DETECTOR_UNIT=0 MP_MOD_LAYER_MASK="" MP_LAYER_SELECT=0
  MP_MOD_QUALITY_VERTEXING="" MP_MOD_QUALITY_TRACKVERTEX="" MP_MOD_MAX_BAD_TRACKS="" MP_ADAPTIVE_VERTEX=0
  MP_MOD_ETA_CONSTANT="" MP_MOD_ETA_SCALE="" MP_MOD_ETA_DETRES="" MP_MOD_VALID_WINDOW=""
  MP_MOD_PT_MIN="" MP_MOD_PT_MAX="" MP_MOD_CHI_IB="" MP_MOD_CHI_OB="" MP_MOD_CHI_IB_TRAIN="" MP_MOD_CHI_OB_TRAIN=""
  MP_MOD_TRACK_REJECT="" MP_MOD_IP_RANGE_R="" MP_MOD_IP_RANGE_Z="" MP_MOD_MIN_CLUSTER="" MP_MOD_VERTEX_DERIVATIVES=""
  MP_DU_PERSIST=0 MP_GENERATION=""
  MP_APPLIED=""
}

# What this tree offers, discovered rather than assumed. Sets MP_* for the
# callers: the driver gates patches on them, doctor and inspect report them.
mp_inspect() {        # treeroot
  local t="$1" src
  mp_reset

  [ -f "$t/$MP_PARALLEL" ] && [ -f "$t/$MP_DETCONST" ] && [ -f "$t/$MP_GEOMHDR" ] && [ -f "$t/$MP_DRIVER" ] && MP_VALID=1

  # The only on-disk difference between the 2024 and the 2025 input tree.
  grep -qE '^[[:space:]]*int[[:space:]]+charge[[:space:]]*;' "$t/$MP_DATAHDR" 2>/dev/null && MP_HAS_CHARGE=1
  if [ -f "$t/$MP_DATAHDR" ]; then
    MP_SCHEMA=2024; [ "$MP_HAS_CHARGE" -eq 1 ] && MP_SCHEMA=2025
  fi

  # A cache-backed geometry runs without O2 in the worker; an O2-only one
  # cannot. A cache-capable header compiles as whichever backend its guard
  # selects, and shipped without the define that is the cache backend.
  src=$(cat "$t/$MP_GEOMHDR" "$t/$MP_GEOMSRC" 2>/dev/null)
  case "$src" in *LoadCache*|*YGEOM_CACHE*) MP_CACHE_CAPABLE=1 ;; esac
  if [ "$MP_CACHE_CAPABLE" -eq 0 ]; then
    case "$src" in *ITSBase/GeometryTGeo.h*) MP_O2_REQUIRED=1 ;; esac
  else
    if grep -qE '^#define YGEOM_USE_O2' "$t/$MP_GEOMHDR" 2>/dev/null; then MP_GEOM_SHIPPED=o2; else MP_GEOM_SHIPPED=cache; fi
  fi

  MP_MOD_NDATA=$(mp_read_define "$t/$MP_PARALLEL" nDATA)
  MP_MOD_NEPOCH=$(mp_read_define "$t/$MP_PARALLEL" nEPOCH)
  MP_MOD_NTRACKMAX=$(mp_read_define "$t/$MP_DETCONST" nTrackMax)
  MP_MOD_DET_MAG=$(mp_read_define "$t/$MP_DETCONST" DET_MAG)
  MP_MOD_PT_MIN=$(mp_read_define "$t/$MP_DETCONST" Update_pTmin)
  MP_MOD_PT_MAX=$(mp_read_define "$t/$MP_DETCONST" Update_pTmax)
  MP_MOD_CHI_IB=$(mp_read_define "$t/$MP_DETCONST" RANGE_CHI_IB)
  MP_MOD_CHI_OB=$(mp_read_define "$t/$MP_DETCONST" RANGE_CHI_OB)
  MP_MOD_CHI_IB_TRAIN=$(mp_read_define "$t/$MP_DETCONST" RANGE_CHI_IB_TRAINING)
  MP_MOD_CHI_OB_TRAIN=$(mp_read_define "$t/$MP_DETCONST" RANGE_CHI_OB_TRAINING)
  MP_MOD_TRACK_REJECT=$(mp_read_define "$t/$MP_DETCONST" TrackRejection)
  MP_MOD_IP_RANGE_R=$(mp_read_define "$t/$MP_DETCONST" RANGE_IMPACTPARAMS_R)
  MP_MOD_IP_RANGE_Z=$(mp_read_define "$t/$MP_DETCONST" RANGE_IMPACTPARAMS_Z)
  MP_MOD_MIN_CLUSTER=$(mp_read_define "$t/$MP_DETCONST" Min_Cluster_by_Sensor)
  MP_MOD_VERTEX_DERIVATIVES=$(mp_read_define "$t/$MP_DETCONST" VERTEX_DERIVATIVES)

  # The learning method is an argument in the driver macro.
  MP_MOD_METHOD=$(sed -n 's/.*ELearningMethod method = YMultiLayerPerceptron::\(k[A-Za-z]*\).*/\1/p' "$t/$MP_DRIVER" 2>/dev/null | head -n1)
  [ -n "$MP_MOD_METHOD" ] && MP_HAS_METHOD_LINE=1
  MP_METHODS_IMPL=$(mp_read_methods "$t/$MP_MLPSRC" "$t/$MP_DETCONST")

  # Detector-unit alignment (2026): DULEVEL picks a level of the
  # HalfBarrel/Layer/HalfStave/Stave/Module/Chip tree.
  MP_MOD_DULEVEL=$(mp_read_define "$t/$MP_DETCONST" DULEVEL)
  [ -n "$MP_MOD_DULEVEL" ] && MP_DETECTOR_UNIT=1

  # Layer selection (2026). A tree without the define hard-codes the outer
  # barrel in its batch path, which is what mask 0x78 spells out.
  MP_MOD_LAYER_MASK=$(mp_read_define "$t/$MP_DETCONST" ALIGN_LAYER_MASK)
  [ -n "$MP_MOD_LAYER_MASK" ] && MP_LAYER_SELECT=1

  # Adaptive vertex estimation (2025 on); absent in 2024.
  MP_MOD_QUALITY_VERTEXING=$(mp_read_global "$t/$MP_MLPSRC" QUALITY_VERTEXING)
  MP_MOD_QUALITY_TRACKVERTEX=$(mp_read_global "$t/$MP_MLPSRC" QUALITY_TRACKVERTEX)
  MP_MOD_MAX_BAD_TRACKS=$(mp_read_badtracks "$t/$MP_MLPSRC")
  [ -n "$MP_MOD_QUALITY_VERTEXING" ] && MP_ADAPTIVE_VERTEX=1

  MP_MOD_ETA_CONSTANT=$(mp_read_global "$t/$MP_MLPSRC" UpdateConstant)
  MP_MOD_ETA_SCALE=$(mp_read_global "$t/$MP_MLPSRC" UpdateScale)
  MP_MOD_ETA_DETRES=$(mp_read_global "$t/$MP_MLPSRC" DETRES)
  MP_MOD_VALID_WINDOW=$(mp_read_global "$t/$MP_MLPSRC" ValidWindow)

  # Whether a step reads and writes weights/weightsDU.txt. The 2024 and 2025
  # trees do; the 2026 tree has both calls commented out, so the merge's
  # weights.txt-only hand-off loses nothing there.
  grep -qE '^[[:space:]]*fMLPNetwork->(SetPrevWeightDetectorUnit|DumpWeightsDetectorUnit)' "$t/$MP_ALIGNSRC" 2>/dev/null && MP_DU_PERSIST=1

  # A label for people; nothing is gated on it.
  if [ "$MP_DETECTOR_UNIT" -eq 1 ] || [ "$MP_LAYER_SELECT" -eq 1 ] || [ "$MP_CACHE_CAPABLE" -eq 1 ]; then
    MP_GENERATION=2026
  elif [ "$MP_HAS_CHARGE" -eq 1 ]; then
    MP_GENERATION=2025
  elif [ "$MP_VALID" -eq 1 ]; then
    MP_GENERATION=2024
  fi
}

# --- layer selection --------------------------------------------------------

mp_layers_mask() {    # "3,4,5,6" -> decimal mask, or empty if malformed
  echo "$1" | awk -F, '
    { m = 0
      for (i = 1; i <= NF; i++) {
        gsub(/[[:space:]]/, "", $i)
        if ($i !~ /^[0-6]$/) { print ""; exit }
        b = 1; for (k = 0; k < $i; k++) b *= 2
        if (int(m / b) % 2 == 1) { print ""; exit }   # repeated layer
        m += b
      }
      print m }'
}

mp_mask_layers() {    # decimal mask -> "3,4,5,6"
  awk -v m="$1" 'BEGIN{ out=""
    for (l = 0; l < 7; l++) { b=1; for(k=0;k<l;k++) b*=2
      if (int(m/b) % 2 == 1) out = (out=="" ? l : out "," l) }
    print out }'
}

mp_layers_preset() {  # decimal mask -> preset name
  case "$1" in
    127) echo "all" ;;
      7) echo "IB-only" ;;
     24) echo "L3L4-only" ;;
     96) echo "L5L6-only" ;;
    120) echo "OB-only" ;;
      *) echo "manual" ;;
  esac
}

mp_layers_sensors() { # decimal mask -> chips covered
  awk -v m="$1" -v n="$MP_LAYER_SENSORS" 'BEGIN{ split(n, a, " "); t=0
    for (l = 0; l < 7; l++) { b=1; for(k=0;k<l;k++) b*=2
      if (int(m/b) % 2 == 1) t += a[l+1] }
    print t }'
}

mp_dulevel_name() {
  case "$1" in
    -1) echo "whole detector" ;;
     0) echo "half-barrel" ;;
     1) echo "layer" ;;
     2) echo "half-stave" ;;
     3) echo "stave" ;;
     4) echo "module" ;;
     5) echo "chip" ;;
     *) echo "?" ;;
  esac
}

# --- runtime estimate -------------------------------------------------------

# Minutes for one training step of one worker, fitted on completed runs of
# the 2026 module; other generations are in the same range. Informational.
MP_COST_FIXED="4.8"
MP_COST_EVAL="0.00337"
MP_COST_EPOCH="0.01431"

mp_estimate() {       # ndata nepoch -> minutes per step
  awk -v n="$1" -v e="$2" -v a="$MP_COST_FIXED" -v b="$MP_COST_EVAL" -v c="$MP_COST_EPOCH" \
    'BEGIN{ if (n=="" || e=="") { print ""; exit }
            s = (e <= 0) ? 0.75 : 1.0;
            if (e < 0) e = 0;
            printf "%.0f", a + b*s*n + e*c*n }'
}

# --- patch primitives -------------------------------------------------------

# A value on the right-hand side of a sed s||| command: & and \ and the |
# delimiter would otherwise be interpreted.
_mp_sed_rhs() { printf '%s' "$1" | sed 's/[\\&|]/\\&/g'; }
# A value inside a grep -E pattern. '+' is the one metacharacter an accepted
# number can carry (1.0e+3), and the 2026 original forgot it.
_mp_re() { printf '%s' "$1" | sed 's|[.[\*^$/+?(){}|]|\\&|g'; }

# DetectorConstant.h carries far more than the knobs, so its values are
# rewritten in place. Leading spacing and any trailing comment are kept, so
# the diff against the module stays readable. Verified after: a define that
# did not take is an error, not a silent no-op.
mp_patch_define() {   # file name value
  local f="$1" n="$2" v="$3"
  [ -f "$f" ] || { mp_die "$f is missing"; return 1; }
  mp_has_define "$f" "$n" || { mp_die "$(basename "$f") has no #define $n"; return 1; }
  sed -i "s|^\([[:space:]]*#define[[:space:]][[:space:]]*$n[[:space:]][[:space:]]*\)[^[:space:]][^[:space:]]*|\1$(_mp_sed_rhs "$v")|" "$f"
  [ "$(mp_read_define "$f" "$n")" = "$v" ] || { mp_die "could not patch $n in $(basename "$f")"; return 1; }
}

# One file-scope `double NAME = value;`, with or without a leading `const`.
# Anchored at the start of the line so commented-out duplicates a few lines
# below (`//double UpdateConstant = 4.0;`) are left alone. The 2024 tree
# declares these as plain `double`, 2025 and 2026 as `const double`; the
# pattern takes both, and the check afterwards catches a tree with neither.
mp_patch_global() {   # file name value
  local f="$1" n="$2" v="$3"
  [ -f "$f" ] || { mp_die "$f is missing"; return 1; }
  [ -n "$(mp_read_global "$f" "$n")" ] || { mp_die "$(basename "$f") has no file-scope 'double $n = ...;'"; return 1; }
  sed -i "s|^\(\(const[[:space:]][[:space:]]*\)\?double[[:space:]][[:space:]]*$n[[:space:]]*=[[:space:]]*\)[^;]*;|\1$(_mp_sed_rhs "$v");|" "$f"
  grep -qE "^(const[[:space:]]+)?double[[:space:]]+$n[[:space:]]*=[[:space:]]*$(_mp_re "$v");" "$f" \
    || { mp_die "could not patch $n in $(basename "$f")"; return 1; }
}

# The adaptive vertex gate lives on `if(Num_Of_Bad_Tracks>N)`. Anchored on
# the variable name, so the digit is the only thing that moves.
mp_patch_badtracks() { # file value
  local f="$1" v="$2"
  [ -f "$f" ] || { mp_die "$f is missing"; return 1; }
  [ -n "$(mp_read_badtracks "$f")" ] || { mp_die "$(basename "$f") has no Num_Of_Bad_Tracks gate"; return 1; }
  sed -i "s|\(if(Num_Of_Bad_Tracks>\)[0-9][0-9]*\()\)|\1$v\2|" "$f"
  grep -q "if(Num_Of_Bad_Tracks>$v)" "$f" || { mp_die "could not patch the bad-track gate in $(basename "$f")"; return 1; }
}

# The learning method is an enumerator in the driver macro. Only that line:
# the 2026 console also rewrites SetSourceTreeName here, which the Manager
# leaves at the "DataInput" its data preparation writes.
mp_patch_driver() {   # file method
  local f="$1" m="$2"
  [ -f "$f" ] || { mp_die "$f is missing"; return 1; }
  sed -i "s|\(ELearningMethod method = YMultiLayerPerceptron::\)k[A-Za-z]*|\1$m|" "$f"
  grep -qE "ELearningMethod method = YMultiLayerPerceptron::$m([^A-Za-z0-9_]|$)" "$f" \
    || { mp_die "could not set the learning method in $(basename "$f")"; return 1; }
}

# Writes or removes the YGEOM_USE_O2 guard in the tree's own header. In both
# modes the marker line is written after the include guard; the define only
# for o2. Idempotent: any marker block a previous pass left is dropped first,
# so switching backends back and forth does not accumulate defines.
#
# The define goes immediately after the include guard, ahead of EVERY
# backend guard in the file -- the header also has an #ifndef YGEOM_USE_O2
# block, and that one comes first; anchoring on the #ifdef would leave it on
# the wrong side of the define. Verified after, unlike the 2026 original: a
# missed anchor under o2 would leave a cache build that Fatals at start-up.
mp_patch_geom() {     # file backend(o2|cache)
  local f="$1" want="$2" tmp
  [ -f "$f" ] || { mp_die "$f is missing"; return 1; }
  grep -q '^#define ROOT_YDetectorGeometry$' "$f" || { mp_die "$(basename "$f") has no ROOT_YDetectorGeometry include guard"; return 1; }
  tmp=$(mktemp "${TMPDIR:-/tmp}/modulepatch.XXXXXX") || return 1
  awk -v want="$want" -v mark="$MP_GEOM_MARK" '
    $0 == mark { drop = 1; next }
    drop == 1 && /^#define (YGEOM_USE_O2|YO2_LOCAL_CONSTANTS) 1$/ { next }
    drop == 1 { drop = 0 }
    { print }
    /^#define ROOT_YDetectorGeometry$/ && !done {
      print mark
      if (want == "o2") print "#define YGEOM_USE_O2 1"
      done = 1
    }
  ' "$f" > "$tmp" && cat "$tmp" > "$f"
  rm -f "$tmp"
  if [ "$want" = o2 ]; then
    grep -q '^#define YGEOM_USE_O2 1$' "$f" || { mp_die "could not select the O2 backend in $(basename "$f")"; return 1; }
  else
    grep -q '^#define YGEOM_USE_O2' "$f" && { mp_die "could not select the cache backend in $(basename "$f")"; return 1; }
  fi
  return 0
}

# --- applying the configuration ----------------------------------------------

# Rewrites every knob that is not `keep` in an unpacked tree, gated on what
# mp_inspect found there. Every change is recorded in MP_APPLIED for the
# manifest. Returns non-zero if a requested knob is unsupported by this tree
# or a patch did not take; the caller must not launch in that case.
mp_apply() {          # treeroot
  local t="$1" bad=0 pair k d v mask old
  _mp_note() { MP_APPLIED="${MP_APPLIED}$1
"; }

  mp_inspect "$t"
  MP_APPLIED=""
  [ "$MP_VALID" -eq 1 ] || { mp_die "$t does not look like an alignment module"; return 1; }

  # Learning method: an argument in the driver. A method the tree declares
  # but does not implement would train nothing while looking busy.
  if [ "$MODULE_LEARNING_METHOD" != keep ]; then
    if [ "$MP_HAS_METHOD_LINE" -ne 1 ]; then
      mp_die "MODULE_LEARNING_METHOD is set, but $MP_DRIVER has no 'ELearningMethod method = ...' line"; bad=1
    elif ! echo " $MP_METHODS_IMPL " | grep -q " $MODULE_LEARNING_METHOD "; then
      mp_die "MODULE_LEARNING_METHOD $MODULE_LEARNING_METHOD is not implemented by this module (its Train() has: ${MP_METHODS_IMPL:-nothing}); use keep or one of those"; bad=1
    else
      mp_patch_driver "$t/$MP_DRIVER" "$MODULE_LEARNING_METHOD" && _mp_note "$MP_DRIVER  method $MP_MOD_METHOD -> $MODULE_LEARNING_METHOD" || bad=1
    fi
  fi

  # Detector unit level: 2026 only. Level 5 (chip) is what an older tree
  # does anyway, so it is accepted there as a documented no-op, which lets
  # one configuration drive both generations.
  if [ "$MODULE_DULEVEL" != keep ]; then
    if [ "$MP_DETECTOR_UNIT" -eq 1 ]; then
      mp_patch_define "$t/$MP_DETCONST" DULEVEL "$MODULE_DULEVEL" && _mp_note "$MP_DETCONST  DULEVEL $MP_MOD_DULEVEL -> $MODULE_DULEVEL ($(mp_dulevel_name "$MODULE_DULEVEL"))" || bad=1
    elif [ "$MODULE_DULEVEL" = 5 ]; then
      _mp_note "DULEVEL 5 requested; this module has no DULEVEL and aligns per chip anyway (no change)"
    else
      mp_die "MODULE_DULEVEL is $MODULE_DULEVEL, but this module has no DULEVEL -- it aligns per chip only; use keep or 5"; bad=1
    fi
  fi

  # Layer selection: 2026 only. The module wants the mask, the configuration
  # holds the list people read. 3,4,5,6 is the outer barrel an older tree
  # hard-codes, accepted there as a no-op for the same reason.
  if [ "$MODULE_LAYERS" != keep ]; then
    mask=$(mp_layers_mask "$MODULE_LAYERS")
    if [ -z "$mask" ] || [ "$mask" -eq 0 ]; then
      mp_die "MODULE_LAYERS '$MODULE_LAYERS' is not a list of distinct layers 0..6"; bad=1
    elif [ "$MP_LAYER_SELECT" -eq 1 ]; then
      mp_patch_define "$t/$MP_DETCONST" ALIGN_LAYER_MASK "$(printf '0x%02X' "$mask")" \
        && _mp_note "$MP_DETCONST  ALIGN_LAYER_MASK $MP_MOD_LAYER_MASK -> $(printf '0x%02X' "$mask") (layers $MODULE_LAYERS, $(mp_layers_preset "$mask"))" || bad=1
    elif [ "$mask" -eq 120 ]; then
      _mp_note "layers 3,4,5,6 requested; this module has no ALIGN_LAYER_MASK and hard-codes the outer barrel anyway (no change)"
    else
      mp_die "MODULE_LAYERS is $MODULE_LAYERS, but this module has no ALIGN_LAYER_MASK -- it hard-codes the outer barrel; use keep or 3,4,5,6"; bad=1
    fi
  fi

  # Numeric defines in DetectorConstant.h.
  for pair in $MP_DEFINE_KEYS; do
    k=${pair%%:*}; d=${pair#*:}
    eval "v=\$$k"
    [ "$v" = keep ] && continue
    if mp_has_define "$t/$MP_DETCONST" "$d"; then
      old=$(mp_read_define "$t/$MP_DETCONST" "$d")
      mp_patch_define "$t/$MP_DETCONST" "$d" "$v" && _mp_note "$MP_DETCONST  $d $old -> $v" || bad=1
    else
      mp_die "$k is set, but this module has no #define $d; use keep"; bad=1
    fi
  done

  # File-scope constants in YMultiLayerPerceptron.cxx.
  for pair in $MP_GLOBAL_KEYS; do
    k=${pair%%:*}; d=${pair#*:}
    eval "v=\$$k"
    [ "$v" = keep ] && continue
    old=$(mp_read_global "$t/$MP_MLPSRC" "$d")
    if [ -n "$old" ]; then
      mp_patch_global "$t/$MP_MLPSRC" "$d" "$v" && _mp_note "$MP_MLPSRC  $d $old -> $v" || bad=1
    else
      mp_die "$k is set, but this module has no file-scope 'double $d' (no adaptive vertex in this tree?); use keep"; bad=1
    fi
  done
  if [ "$MODULE_MAX_BAD_TRACKS" != keep ]; then
    if [ -n "$MP_MOD_MAX_BAD_TRACKS" ]; then
      mp_patch_badtracks "$t/$MP_MLPSRC" "$MODULE_MAX_BAD_TRACKS" && _mp_note "$MP_MLPSRC  Num_Of_Bad_Tracks gate $MP_MOD_MAX_BAD_TRACKS -> $MODULE_MAX_BAD_TRACKS" || bad=1
    else
      mp_die "MODULE_MAX_BAD_TRACKS is set, but this module has no Num_Of_Bad_Tracks gate; use keep"; bad=1
    fi
  fi

  # Geometry backend. A tree with only the O2 backend is left exactly as it
  # is under o2, and cannot do cache. A cache-capable tree compiles as the
  # cache backend unless the guard is written, so o2 -- the default -- is an
  # action there, not a no-op.
  case "$GEOM_BACKEND" in
    o2)
      if [ "$MP_CACHE_CAPABLE" -eq 1 ]; then
        mp_patch_geom "$t/$MP_GEOMHDR" o2 && _mp_note "$MP_GEOMHDR  backend o2 (YGEOM_USE_O2 written; the tree ships as cache)" || bad=1
      fi ;;
    cache)
      if [ "$MP_CACHE_CAPABLE" -eq 1 ]; then
        mp_patch_geom "$t/$MP_GEOMHDR" cache && _mp_note "$MP_GEOMHDR  backend cache (worker reads $MP_GEOMCACHE; no O2 in the worker)" || bad=1
      else
        mp_die "GEOM_BACKEND is cache, but this module only has the O2 backend; use o2"; bad=1
      fi ;;
    *) mp_die "GEOM_BACKEND must be o2 or cache, got '$GEOM_BACKEND'"; bad=1 ;;
  esac

  unset -f _mp_note
  return $bad
}

# What was found and what was changed, so a run records exactly what it
# trained with. Written beside the worker's copy of the archive, outside the
# module tree, so the tree itself holds nothing the archive did not.
mp_manifest() {       # file archive treeroot
  {
    echo "module_patch_manifest  $(date '+%Y-%m-%d %H:%M:%S')"
    echo "archive     $2"
    echo "tree        $3"
    echo "generation  ${MP_GENERATION:-?} (schema ${MP_SCHEMA:-?}, charge=$MP_HAS_CHARGE, detector-unit=$MP_DETECTOR_UNIT, layer-select=$MP_LAYER_SELECT, adaptive-vertex=$MP_ADAPTIVE_VERTEX, cache-capable=$MP_CACHE_CAPABLE, o2-required=$MP_O2_REQUIRED, du-persist=$MP_DU_PERSIST)"
    echo "methods     ${MP_METHODS_IMPL:-?} (implemented by this tree; driver macro ships ${MP_MOD_METHOD:-?})"
    echo "job size    nDATA=$MODULE_EVENTS nEPOCH=$MODULE_EPOCHS nCORE=$MODULE_CORES jparallel=$MODULE_JPARALLEL (YMLPParallel.h regenerated)"
    if [ -n "$MP_APPLIED" ]; then
      echo "patched"
      printf '%s' "$MP_APPLIED" | sed 's/^/  /'
    else
      echo "patched     nothing -- every module knob is keep and the tree has only the O2 backend"
    fi
  } > "$1"
}

# The capability report as plain NAME=value lines, one per MP_* variable, so
# `alignctl.sh inspect` and the GUI read the same thing the driver gates on.
mp_report() {         # (after mp_inspect)
  local v
  for v in MP_VALID MP_GENERATION MP_SCHEMA MP_HAS_CHARGE MP_CACHE_CAPABLE MP_O2_REQUIRED MP_GEOM_SHIPPED \
           MP_HAS_METHOD_LINE MP_MOD_METHOD MP_METHODS_IMPL MP_DETECTOR_UNIT MP_MOD_DULEVEL \
           MP_LAYER_SELECT MP_MOD_LAYER_MASK MP_ADAPTIVE_VERTEX MP_DU_PERSIST \
           MP_MOD_NDATA MP_MOD_NEPOCH MP_MOD_NTRACKMAX MP_MOD_DET_MAG MP_MOD_PT_MIN MP_MOD_PT_MAX \
           MP_MOD_CHI_IB MP_MOD_CHI_OB MP_MOD_CHI_IB_TRAIN MP_MOD_CHI_OB_TRAIN MP_MOD_TRACK_REJECT \
           MP_MOD_IP_RANGE_R MP_MOD_IP_RANGE_Z MP_MOD_MIN_CLUSTER MP_MOD_VERTEX_DERIVATIVES \
           MP_MOD_ETA_CONSTANT MP_MOD_ETA_SCALE MP_MOD_ETA_DETRES MP_MOD_VALID_WINDOW \
           MP_MOD_QUALITY_VERTEXING MP_MOD_QUALITY_TRACKVERTEX MP_MOD_MAX_BAD_TRACKS; do
    eval "printf '%s=%s\n' \"$v\" \"\${$v:-}\""
  done
}
