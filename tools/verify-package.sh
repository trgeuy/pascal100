#!/bin/sh
#
# PROVE THE MANUAL IS TRUE AGAINST THE ACTUAL SHIPPED PACKAGE.
#
# tools/build-package.sh assembles the archive we hand people and STOPS -- it never runs the
# manual's own commands against it, so a chapter can tell a reader to type something that does
# not work in the zip and nothing catches it. That is not hypothetical: the manual promised
# "CP/M in one command" while a published archive carried no media at all (docs/package.map
# and issue #308 record it). This script is the missing half. It takes the finished archive,
# extracts it somewhere `git rev-parse` FAILS -- no repository in sight, exactly the layout a
# user has -- and types the manual's own fenced `$ pascal100 ...` commands at the binary that
# is actually in the package, asserting each reaches the prompt the manual says it will.
#
# WHY A SEPARATE SCRIPT, and not a step inside build-package.sh or a ctest case:
#   * build-package.sh's job is to ASSEMBLE AND STOP (like build-checksums.sh). Verifying a
#     finished artifact is a different job, and must be re-runnable against an already-built
#     archive without a rebuild -- which folding it into the packager would forbid.
#   * ctest runs BEFORE packaging (DISTRIBUTION.md 4.2 step 4) and proves the BINARY, not the
#     archive. The archive only exists at release time, and only then do the PDFs (docs.yml)
#     and the windowed-SDL binary that build-package.sh's own gates require exist. So this is
#     a release-time check, wired into DISTRIBUTION.md 4.2 after build-package.sh. In altairsim,
#     CI also ran it (package.yml, issue #813); pascal100 removed that workflow.
#
# WHERE THE COMMANDS AND THE PROMPTS COME FROM. The commands are read from the manual SOURCE
# (docs/manual/*.md in this checkout -- the package ships only the rendered PDF), token-expanded
# from docs/package.map with the SAME substitution build-package.sh uses, so the two can never
# disagree about what {{MACHINE_CPM}} means. Extraction is open-ended (any fenced one-liner is a
# candidate); DISPATCH is a small closed table, and a documented example this table does not
# know is a LOUD FAILURE ("add it here"), never a silent skip. That asymmetry is the point: a
# new `$ pascal100 {{MACHINE_NEWTHING}}` in a chapter cannot slip past unverified.
#
# A COMMAND WITH OPTIONS after the machine file (`$ pascal100 {{MACHINE_CPM}} --mcp`) is not a
# boot-to-prompt walkthrough -- it starts an MCP server, or runs `-x` commands on a disk the reader
# made earlier -- so no driver runs it. It still gets the check that catches the #308 fault: the
# machine file it names must be in the package, and it must be one the dispatch table knows. It
# is tallied apart from the boot checks and named in a closing NOTE, so the PASS line's claim
# ("reached their documented prompt") stays true and an unrun option never looks verified.
#
# ONE DRIVER, PIPED. pascal100 ships one booting example, CompuPro CP/M 2.2K (examples/cpm22).
# The keystrokes go in on a pipe and the sign-on and a DIR line must come back -- the same
# strings tests/acceptance/examples.cmake checks. altairsim's pty drivers (4K BASIC, BASIC 1.0,
# disk BASIC, the Sol-20, HDSK CP/M) went with those machines in standalone step 2.
#
# NOT `set -e`. This harness RUNS commands that are expected to fail when the package is wrong
# -- a failed boot is a finding, not a script error -- so failures are handled explicitly and
# tallied, and the script's own exit status is the count of findings.
#
#   usage: tools/verify-package.sh [--keep] [--require-notarized] <archive>
#
#     --keep     Leave the scratch extraction directory instead of deleting it, to debug a
#                failure by hand (the message names the directory).
#     --require-notarized
#                macOS: the program must be signed with a Developer ID, Apple must have
#                notarized it, and it must start with the quarantine mark a browser adds.
#                tools/release-worker.sh passes it; the CI leg, which signs ad hoc, does not.
#                Without it, a macOS package still gets `codesign --verify --strict`.
#     <archive>  dist/pascal100-X.Y.Z-<target>.tar.gz or .zip -- what build-package.sh wrote.

set -u

root=$(cd "$(dirname "$0")/.." && pwd)
map=$root/docs/package.map
manual=$root/docs/manual

keep=0
notarized=0
archive=""
while [ $# -gt 0 ]; do
  case $1 in
    --keep) keep=1; shift ;;
    --require-notarized) notarized=1; shift ;;
    -h|--help) echo "usage: tools/verify-package.sh [--keep] [--require-notarized] <archive>"; exit 0 ;;
    -*) echo "verify-package: unknown option $1" >&2; exit 1 ;;
    *)  [ -z "$archive" ] || { echo "verify-package: only one archive, got '$archive' and '$1'" >&2; exit 1; }
        archive=$1; shift ;;
  esac
done

[ -n "$archive" ] || { echo "verify-package: no archive given. usage: tools/verify-package.sh [--keep] <archive>" >&2; exit 1; }
[ -f "$archive" ] || { echo "verify-package: $archive does not exist" >&2; exit 1; }
[ -f "$map" ]     || { echo "verify-package: $map is missing -- run from a checkout of the repo" >&2; exit 1; }
case $notarized:$archive in
  1:*-macos-*.tar.gz|0:*) ;;
  *) echo "verify-package: --require-notarized is for a macOS package, not $archive" >&2; exit 1 ;;
esac


archive=$(cd "$(dirname "$archive")" && pwd)/$(basename "$archive")

# ---------------------------------------------------------------------------
# 1. EXTRACT, OUTSIDE THE REPOSITORY. mktemp under TMPDIR, never under $root, so the extracted
#    tree is a place `git rev-parse` fails -- the same "no repo in sight" property the examples
#    acceptance test relies on, now proven against the REAL archive rather than a copy. Running
#    the PACKAGED binary (not build/pascal100) also proves the copy kept the exec bit and did
#    not corrupt it.
# ---------------------------------------------------------------------------
scratch=$(mktemp -d "${TMPDIR:-/tmp}/verify-package.XXXXXX") || {
  echo "verify-package: could not make a scratch directory" >&2; exit 1; }

cleanup() {
  if [ "$keep" -eq 1 ]; then echo "verify-package: kept scratch dir $scratch" >&2
  else rm -rf "$scratch"; fi
}
trap cleanup EXIT INT TERM

extract_ok=1
case $archive in
  *.tar.gz|*.tgz) tar xzf "$archive" -C "$scratch" || extract_ok=0 ;;
  *.zip)
    # unzip first; fall back to a REAL bsdtar (libarchive), mirroring build-package.sh's
    # zip-CREATION asymmetry on the extraction side. Git Bash's GNU tar cannot unzip, so on
    # Windows reach System32\tar.exe by absolute path exactly as the packager does.
    if command -v unzip > /dev/null 2>&1; then
      unzip -q "$archive" -d "$scratch" || extract_ok=0
    else
      bsdtar=""
      if tar --version 2>/dev/null | grep -qi bsdtar; then
        bsdtar=tar
      else
        for t in "$(cygpath -u "${SYSTEMROOT:-}" 2>/dev/null)/System32/tar.exe" \
                 /c/Windows/System32/tar.exe; do
          if [ -x "$t" ] && "$t" --version 2>/dev/null | grep -qi bsdtar; then bsdtar=$t; break; fi
        done
      fi
      [ -n "$bsdtar" ] || { echo "verify-package: no unzip and no bsdtar -- cannot extract $archive" >&2; exit 1; }
      ( cd "$scratch" && "$bsdtar" -xf "$archive" ) || extract_ok=0
    fi ;;
  *) echo "verify-package: unknown archive extension: $archive (want .tar.gz or .zip)" >&2; exit 1 ;;
esac
[ "$extract_ok" -eq 1 ] || { echo "verify-package: extracting $archive failed" >&2; exit 1; }

# The archive holds exactly one top-level directory, pascal100-X.Y.Z-<target>/.
base=""
for d in "$scratch"/*/; do
  [ -d "$d" ] || continue
  [ -z "$base" ] || { echo "verify-package: archive has more than one top-level directory -- $archive" >&2; exit 1; }
  base=${d%/}
done
[ -n "$base" ] || { echo "verify-package: archive extracted to nothing -- $archive" >&2; exit 1; }

if   [ -x "$base/pascal100" ];     then bin=$base/pascal100
elif [ -x "$base/pascal100.exe" ]; then bin=$base/pascal100.exe
elif [ -f "$base/pascal100.exe" ]; then bin=$base/pascal100.exe   # Windows: no exec bit to check
else echo "verify-package: no pascal100 binary in the package ($base)" >&2; exit 1
fi

# ---------------------------------------------------------------------------
# 2. THE TOKEN TABLE, built from docs/package.map into a sed script of `s|{{TOKEN}}|value|g`
#    lines -- the SAME token regex build-package.sh's expand() uses (tools/build-package.sh
#    ~line 309), so a path the manual writes as a token expands here to the path that is
#    actually in the zip, and the two scripts cannot drift on it.
# ---------------------------------------------------------------------------
sedscript=$scratch/.tokens.sed
sed -n 's/^\([A-Z_][A-Z0-9_]*\)[[:blank:]]*=[[:blank:]]*\(.*[^[:blank:]]\)[[:blank:]]*$/s|{{\1}}|\2|g/p' "$map" > "$sedscript"
expand() { printf '%s' "$1" | sed -f "$sedscript"; }

# ---------------------------------------------------------------------------
# 3. EXTRACT CANDIDATE COMMANDS from the manual's fenced blocks. A block is a runnable
#    candidate IFF it holds exactly one `$ pascal100 <arg>` line whose arg does not start with
#    `-`, optionally preceded in the same block by ONE `$ cp -R`, `$ cd`, or `$ ls` setup line
#    and no other `$` command. That keeps the walkthrough one-liners (and their `cp -R` / `cd`
#    pairs) and drops the machines/running/package comparison TABLES, which always stack two or
#    three `$ pascal100` lines with trailing prose in one block. awk emits, per surviving block,
#    a tab-separated record:  FILE <tab> LINE <tab> SETUP-LINE-or-empty <tab> PASCAL100-LINE
#    The PASCAL100-LINE may carry options after the machine file; route() below decides what
#    such a line is checked for.
# ---------------------------------------------------------------------------
records=$scratch/.records
awk '
  FNR==1 { inblock=0 }
  /^```/ {
    if (inblock) {
      ok=1; ac=0; setupn=0; acmd=""; setup=""
      for (i=1; i<=n; i++) {
        l=cmd[i]
        if (l ~ /^\$ pascal100 /)                                    { ac++; acmd=l }
        else if (l ~ /^\$ cp -R / || l ~ /^\$ cd / || l ~ /^\$ ls /) { setupn++; setup=l }
        else                                                         { ok=0 }
      }
      if (ok==1 && ac==1 && setupn<=1) {
        arg=acmd; sub(/^\$ pascal100 */,"",arg)
        if (arg !~ /^-/) print FILENAME "\t" bstart "\t" setup "\t" acmd
      }
      inblock=0
    } else { inblock=1; bstart=FNR; n=0 }
    next
  }
  inblock && /^\$ / { cmd[++n]=$0 }
' "$manual"/*.md > "$records"

# ---------------------------------------------------------------------------
# 4/5. RESOLVE, DISPATCH, RUN, ASSERT.
# ---------------------------------------------------------------------------
seen=$scratch/.seen
: > "$seen"
total=0
passed=0
failed=0
optchecked=0

# assert_piped <label> <captured-output>  (required strings: newline-list on stdin)
assert_piped() {
  _label=$1; _out=$2; _missing=""
  while IFS= read -r want; do
    [ -n "$want" ] || continue
    if ! printf '%s' "$_out" | grep -F -q "$want"; then _missing="$_missing
    missing: $want"; fi
  done
  if [ -n "$_missing" ]; then
    echo "verify-package: FAIL -- $_label did not reach its documented prompt:" >&2
    printf '%s\n' "$_missing" >&2
    return 1
  fi
  return 0
}

# run_piped <rundir> <tomlarg> <keysfile>  -> PIPED_OUT. Boot flat-out, feed keys, watchdog-
# kill: the guest does not quit on stdin EOF, but the banner and first DIR line land in the
# first second at the free-running default.
run_piped() {
  _dir=$1; _toml=$2; _keys=$3
  _outf=$scratch/.out
  ( cd "$_dir" && "$bin" "$_toml" < "$_keys" > "$_outf" 2>&1 ) &
  _pid=$!
  ( sleep 30; kill "$_pid" 2>/dev/null ) &
  _wd=$!
  wait "$_pid" 2>/dev/null
  kill "$_wd" 2>/dev/null
  PIPED_OUT=$(cat "$_outf" 2>/dev/null)
  rm -f "$_outf"
}

# dispatch <driver> <target> <detail> <human-cmd>
#   target=rundir, detail=toml arg relative to it
dispatch() {
  _key=$1; _target=$2; _detail=$3; _cmd=$4
  _dk="$_key|$_target|$_detail"
  if grep -Fxq "$_dk" "$seen" 2>/dev/null; then echo "  (already handled: $_cmd)"; return 0; fi
  echo "$_dk" >> "$seen"
  total=$((total + 1))
  echo "  RUN  $_cmd"
  _ok=0
  case $_key in
    CPM)
      run_piped "$_target" "$_detail" "$root/tests/acceptance/cpm-dir.keys"
      if assert_piped "CP/M ($_cmd)" "$PIPED_OUT" <<EOF
CompuPro Systems
64K CP/M 2.2K
A>
A: MAC      COM
EOF
      then _ok=1; fi ;;
    PASCAL)
      # pascal.toml opens a VT52 window, which a pipe cannot drive. Its twin pascal-console.toml
      # is the same machine and disk with the console on stdio: boot that, and expect the UCSD
      # banner. Nothing is typed; the system starts by itself. Its boot takes seconds, and the
      # program stops when its input ends ("input ended"), so stdin is a pipe held open by a
      # sleep rather than a keys file. The subshell execs the program, so $! is the program.
      _outf=$scratch/.out
      sleep 25 | ( cd "$_target" && exec "$bin" pascal-console.toml ) > "$_outf" 2>&1 &
      _pid=$!
      ( sleep 20; kill "$_pid" 2>/dev/null ) &
      _wd=$!
      wait "$_pid" 2>/dev/null
      kill "$_wd" 2>/dev/null
      PIPED_OUT=$(cat "$_outf" 2>/dev/null)
      rm -f "$_outf"
      if assert_piped "UCSD Pascal ($_cmd)" "$PIPED_OUT" <<EOF
U.C.S.D.  Pascal  System  III.0
EOF
      then _ok=1; fi ;;
  esac
  if [ "$_ok" -eq 1 ]; then passed=$((passed + 1)); echo "  PASS $_cmd"
  else failed=$((failed + 1)); echo "  ---- FAIL $_cmd"; fi
}

# route <driver> <target> <detail> <human-cmd>: a bare machine file goes to its boot driver; one
# with options only has to be in the package (see the header).
route() {
  if [ -z "$mrest" ]; then dispatch "$@"; return; fi
  if [ -f "$rundir/$toml_run" ]; then
    optchecked=$((optchecked + 1))
    echo "  FILE $mpath is in the package (options not run: $mrest)"
  else
    echo "verify-package: FAIL -- $rel:$line names $mpath, which is not in the package." >&2
    failed=$((failed + 1))
  fi
}

echo "verify-package: running the manual's documented commands against"
echo "                $archive"
echo

# Iterate the work-list by INDEX, pulling each record with sed. A streaming `while read` loop
# shares its input descriptor with the children spawned in the body (the backgrounded simulator,
# expect), which disturb it and silently drop and reorder commands; reading each line
# independently shares nothing with any child. The list is tiny, so O(n^2) is free.
nrec=$(awk 'END{print NR}' "$records")
rec_i=0
while [ "$rec_i" -lt "$nrec" ]; do
  rec_i=$((rec_i + 1))
  record=$(sed -n "${rec_i}p" "$records")
  [ -n "$record" ] || continue
  file=$(printf  '%s' "$record" | cut -f1)
  line=$(printf  '%s' "$record" | cut -f2)
  setup=$(printf '%s' "$record" | cut -f3)
  acmd=$(printf  '%s' "$record" | cut -f4)
  [ -n "${acmd:-}" ] || continue
  rel=${file#"$root"/}
  arg=${acmd#\$ pascal100 }
  argx=$(expand "$arg")
  # The machine file is the first word; anything after it is options (see route()).
  mpath=${argx%% *}
  mrest=""
  case $argx in *' '*) mrest=${argx#* } ;; esac

  case $argx in
    *'{{'*'}}'*)
      echo "verify-package: FAIL -- $rel:$line has an UNEXPANDED TOKEN: $argx" >&2
      echo "  Add it to docs/package.map." >&2
      failed=$((failed + 1)); continue ;;
  esac

  # Only PATH-shaped args are packaged-media examples. A bare word ($ pascal100 basic4k) or an
  # empty arg ($ pascal100) is a built-in launch -- log and skip, do not dispatch.
  is_path=0
  case $mpath in */*|*.toml) is_path=1 ;; esac
  if [ "$is_path" -eq 0 ]; then
    echo "  SKIP (built-in, not packaged media): $rel:$line  \$ pascal100 ${arg:-<default>}"
    continue
  fi

  # Setup + where the command runs from:
  #   cp -R A B : copy inside the scratch tree (guarded against a second chapter re-copying),
  #               run from base with the copied path -- proves the "copy the folder" step and
  #               never touches the archive under dist/.
  #   cd D      : run from base/D with the bare filename.
  #   ls / none : run from base with the expanded path.
  rundir=$base
  toml_run=$mpath
  case $setup in
    '$ cp -R '*)
      _src=$(printf '%s' "$setup" | awk '{print $4}')
      _dst=$(printf '%s' "$setup" | awk '{print $5}')
      [ -e "$base/$_dst" ] || cp -R "$base/$_src" "$base/$_dst" ;;
    '$ cd '*)
      _cd=$(printf '%s' "$setup" | awk '{print $3}')
      rundir=$base/$_cd
      toml_run=$(basename "$mpath") ;;
  esac

  bn=$(basename "$toml_run")
  case $bn in
    cpm22-console.toml) route CPM   "$rundir" "$toml_run" "\$ pascal100 $arg" ;;
    pascal.toml|pascal-console.toml)
                         route PASCAL "$rundir" "$toml_run" "\$ pascal100 $arg" ;;
    *)
      echo "verify-package: FAIL -- $rel:$line runs a documented example this check does not know:" >&2
      echo "    \$ pascal100 $arg   ->   $argx" >&2
      echo "  Add a dispatch case for $bn to tools/verify-package.sh." >&2
      failed=$((failed + 1)) ;;
  esac
done

# THE USER TOOLS. The manual and tools/README.pdf tell a reader to run tools/mirror-watch.sh
# (or .ps1) and the LBR tools, so all must be in the archive, with their README and license.
# And where the archive carries modes -- a tarball -- every script must be executable, and the
# .sh must run: a script that
# refuses a bad port with its usage line has got as far as bash parsing all of it. Starting a
# mirror for it to watch is the acceptance suite's job (tests/acceptance/mirror-watch.sh).
total=$((total + 1))
_tools_ok=1
for _f in mirror-watch.sh mirror-watch.ps1 mcp-watch.sh README.pdf LICENSE-mirror-watch.txt \
          lbr-extract.py uncrunch.py unsqueeze.py; do
  if [ ! -f "$base/tools/$_f" ]; then
    echo "verify-package: FAIL -- tools/$_f is not in the package." >&2
    _tools_ok=0
  fi
done
case $archive in
  *.tar.gz)
    if [ "$_tools_ok" -eq 1 ]; then
      for _f in lbr-extract.py uncrunch.py unsqueeze.py mcp-watch.sh; do
        if [ ! -x "$base/tools/$_f" ]; then
          echo "verify-package: FAIL -- tools/$_f is not executable." >&2
          _tools_ok=0
        fi
      done
      if [ ! -x "$base/tools/mirror-watch.sh" ]; then
        echo "verify-package: FAIL -- tools/mirror-watch.sh is not executable." >&2
        _tools_ok=0
      else
        _usage=$("$base/tools/mirror-watch.sh" not-a-port 2>&1) && _rc=0 || _rc=$?
        case "$_rc:$_usage" in
          1:*'is not a port number'*) ;;
          *) echo "verify-package: FAIL -- tools/mirror-watch.sh not-a-port gave ($_rc): $_usage" >&2
             _tools_ok=0 ;;
        esac
      fi
    fi ;;
esac
if [ "$_tools_ok" -eq 1 ]; then passed=$((passed + 1)); echo "  PASS tools/ has the mirror-watch scripts and the LBR tools"
else failed=$((failed + 1)); echo "  ---- FAIL tools/ (the mirror-watch scripts and the LBR tools)"; fi

# THE macOS SIGNATURE (tools/macos-sign.sh). Every macOS package must carry a valid one; a
# release package must also be a Developer ID one that Apple notarized, and must START with the
# quarantine mark on it -- the state a browser download is in, which is what a user has.
case $archive in
  *-macos-*.tar.gz)
    total=$((total + 1))
    _sig_ok=1
    if ! _out=$(codesign --verify --strict --verbose=2 "$bin" 2>&1); then
      echo "verify-package: FAIL -- the program's signature is not valid:" >&2
      echo "$_out" | sed 's/^/    /' >&2
      _sig_ok=0
    elif [ "$notarized" -eq 1 ]; then
      _auth=$(codesign --display --verbose=2 "$bin" 2>&1 | sed -n 's/^Authority=//p' | head -n 1)
      case $_auth in
        "Developer ID Application:"*) ;;
        *) echo "verify-package: FAIL -- signed by '${_auth:-nobody: an ad hoc signature}', not a Developer ID Application identity" >&2
           _sig_ok=0 ;;
      esac
      # spctl asks Gatekeeper, which looks the notary ticket up online. `--type execute`
      # assesses only an app bundle and rejects a bare program as "not an app"; this form is
      # the one that judges a plain executable.
      _gk=$(spctl --assess --type open --context context:primary-signature --verbose=2 "$bin" 2>&1)
      case $_gk in
        *"source=Notarized Developer ID"*) ;;
        *) echo "verify-package: FAIL -- Gatekeeper does not accept the program as notarized:" >&2
           echo "$_gk" | sed 's/^/    /' >&2
           _sig_ok=0 ;;
      esac
      if [ "$_sig_ok" -eq 1 ]; then
        xattr -w com.apple.quarantine "0081;$(printf '%x' "$(date +%s)");Safari;" "$bin"
        if ! _out=$("$bin" --version 2>&1); then
          echo "verify-package: FAIL -- with the quarantine mark on it, the program did not start:" >&2
          echo "$_out" | sed 's/^/    /' >&2
          _sig_ok=0
        fi
        xattr -d com.apple.quarantine "$bin" 2>/dev/null || true
      fi
    fi
    if [ "$_sig_ok" -eq 1 ]; then
      passed=$((passed + 1))
      if [ "$notarized" -eq 1 ]; then echo "  PASS the program is notarized and starts with the quarantine mark"
      else echo "  PASS the program's signature is valid (notarization not required)"; fi
    else failed=$((failed + 1)); echo "  ---- FAIL the program's signature"; fi ;;
esac

echo
# A claimed test that runs nothing is worse than no test (issue #308's whole point), so an
# empty extraction is itself a failure -- the extractor or the manual's fence layout changed.
if [ "$((total + failed))" -eq 0 ]; then
  echo "verify-package: FAIL -- extracted NO runnable commands from docs/manual/*.md." >&2
  echo "  The fence layout or the extractor changed; this check is not proving anything." >&2
  exit 2
fi

if [ "$optchecked" -ne 0 ]; then
  echo "verify-package: NOTE -- $optchecked command(s) with options: machine file checked, options not run." >&2
fi

if [ "$failed" -ne 0 ]; then
  echo "verify-package: FAIL -- $failed of $((passed + failed)) manual commands did not reach their documented prompt." >&2
  exit 1
fi

echo "verify-package: PASS -- $passed/$passed manual commands reached their documented prompt."
exit 0
