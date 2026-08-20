#!/bin/bash
# ==========================================================================
#  alignctl.sh -- read, change and check the alignment configuration
# ==========================================================================
#  Usage:
#    alignctl.sh show                 print every setting and what follows
#    alignctl.sh get KEY              print one value
#    alignctl.sh set KEY=VALUE ...    change settings in place
#    alignctl.sh validate             check types and relationships
#    alignctl.sh doctor               check this machine has what the run needs
#    alignctl.sh generate             write the generated headers
#    alignctl.sh keys                 list every key
#    alignctl.sh ui                   open the ROOT configuration window
# ==========================================================================
set -u

_here=$(cd "$(dirname "$0")" && pwd)
. "$_here/alignconf.sh"

usage() { sed -n '3,17p' "$0" | sed 's/^#[ ]\{0,2\}//'; }

cmd=${1:-show}
[ $# -gt 0 ] && shift

case "$cmd" in

  show)
    ac_load || exit 1
    ac_print
    ;;

  get)
    [ $# -eq 1 ] || { echo "usage: alignctl.sh get KEY" >&2; exit 2; }
    ac_load || exit 1
    ac_type "$1" | grep -q . || { echo "unknown key: $1" >&2; exit 2; }
    eval "printf '%s\n' \"\$$1\""
    ;;

  set)
    [ $# -ge 1 ] || { echo "usage: alignctl.sh set KEY=VALUE [KEY=VALUE ...]" >&2; exit 2; }
    for pair in "$@"; do
      case "$pair" in
        *=*) ;;
        *) echo "not a KEY=VALUE pair: $pair" >&2; exit 2 ;;
      esac
    done

    # Every pair is applied to a scratch copy and the result validated there
    # before anything is written back. Applying them straight to the file meant
    # a bad third value left the first two committed and skipped validation
    # altogether, so the file could end up in a state the tool would refuse to
    # produce.
    _conf=$(ac_conf_file)
    _scratch=$(mktemp "${TMPDIR:-/tmp}/alignctl.XXXXXX") || exit 1
    trap 'rm -f "$_scratch"' EXIT
    cat "$_conf" > "$_scratch" || exit 1

    _saved_conf=${ALIGN_CONF:-}
    ALIGN_CONF="$_scratch"
    for pair in "$@"; do
      ac_set "${pair%%=*}" "${pair#*=}" || exit 1
    done

    if ! ( ac_load && ac_validate ); then
      echo "nothing written: the result would be invalid" >&2
      exit 1
    fi
    ALIGN_CONF=$_saved_conf

    cat "$_scratch" > "$_conf" || exit 1
    for pair in "$@"; do echo "set ${pair%%=*}"; done
    ;;

  validate)
    ac_load || exit 1
    if ac_validate; then echo "configuration is consistent"; else exit 1; fi
    ;;

  doctor)
    ac_load || exit 1
    ac_validate || { echo "fix the configuration first" >&2; exit 1; }
    ac_doctor
    ;;

  generate)
    ac_load || exit 1
    ac_validate || { echo "refusing to generate from an invalid configuration" >&2; exit 1; }
    ac_generate
    ;;

  keys)
    ac_keys
    ;;

  ui)
    ac_load || exit 1
    if ! command -v root >/dev/null 2>&1; then
      echo "root is not on PATH. Load the O2 environment first, for example:" >&2
      echo "  eval \`alienv load -w \$O2_DIR/sw O2/latest\`" >&2
      exit 1
    fi
    root -l "$AC_ROOT/RUN/ConfigUI/ConfigUI.C(\"$(ac_conf_file)\")"
    ;;

  -h|--help|help)
    usage
    ;;

  *)
    echo "unknown command: $cmd" >&2
    usage >&2
    exit 2
    ;;
esac
