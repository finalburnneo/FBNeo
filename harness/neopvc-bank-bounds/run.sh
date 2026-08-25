#!/usr/bin/env bash
#
# Build and run the 68K program-ROM bank-bounds harness twice:
#
#   baseline -- functions extracted from the pristine base commit
#               (git show $BASE:src/burn/drv/neogeo/...)
#   patched  -- functions extracted from the working tree
#
# then enforce the acceptance criteria from SPEC.md §6.  Exits non-zero on any
# failure, so this is usable as a gate and not just as a report.
#
# The functions under test are never copied by hand: extract.py pulls them
# verbatim from the real sources for each variant, which is what makes the
# comparison meaningful.
#
# No ROM or BIOS data is used anywhere.  All inputs are synthetic.
#
# Usage:  ./run.sh [base-rev]        (default base-rev: 68fc1af)

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(git -C "$HERE" rev-parse --show-toplevel)"
BASE="${1:-68fc1af}"

D_NEOGEO="src/burn/drv/neogeo/d_neogeo.cpp"
NEO_RUN="src/burn/drv/neogeo/neo_run.cpp"

D_FUNCS=(NeoPVCBankswitch NeoPVCMapBank mslugxMapBank mslugxBankswitch
         ms5plusWriteWordBankSwitch kf2k3blaWriteWordBankswitch)
R_FUNCS=(Bankswitch)
R_OPT=(NeoClampBank)
# object-like macros the patched d_neogeo.cpp introduces; absent from baseline
D_DEFS=(PVC_BANK_WINDOW MSLUGX_BANK_WINDOW)

CXX=${CXX:-c++}
CXXFLAGS=${CXXFLAGS:--O2 -std=c++17 -Wall -Wextra -Wno-unused-parameter}

BUILD="$HERE/build"
rm -rf "$BUILD"
mkdir -p "$BUILD/baseline/gen" "$BUILD/patched/gen"

emit_gen() {   # $1 = variant dir, $2 = "base"|"tree"
	local out="$1" mode="$2"
	local req_d=() req_r=() opt_r=() def_d=()
	for f in "${D_FUNCS[@]}"; do req_d+=(--require "$f"); done
	for f in "${R_FUNCS[@]}"; do req_r+=(--require "$f"); done
	for f in "${R_OPT[@]}";   do opt_r+=(--optional "$f"); done
	for f in "${D_DEFS[@]}";  do def_d+=(--define  "$f"); done

	if [ "$mode" = base ]; then
		git -C "$REPO" show "$BASE:$NEO_RUN" \
			| python3 "$HERE/extract.py" --source - --label "$BASE:$NEO_RUN" \
			  "${req_r[@]}" "${opt_r[@]}" > "$out/gen/neo_run_funcs.inc"
		git -C "$REPO" show "$BASE:$D_NEOGEO" \
			| python3 "$HERE/extract.py" --source - --label "$BASE:$D_NEOGEO" \
			  "${def_d[@]}" "${req_d[@]}" > "$out/gen/d_neogeo_funcs.inc"
	else
		python3 "$HERE/extract.py" --source "$REPO/$NEO_RUN" --label "tree:$NEO_RUN" \
			"${req_r[@]}" "${opt_r[@]}" > "$out/gen/neo_run_funcs.inc"
		python3 "$HERE/extract.py" --source "$REPO/$D_NEOGEO" --label "tree:$D_NEOGEO" \
			"${def_d[@]}" "${req_d[@]}" > "$out/gen/d_neogeo_funcs.inc"
	fi
}

build_variant() {   # $1 = label, $2 = "base"|"tree"
	local label="$1" mode="$2"
	local out="$BUILD/$label"
	echo "=== extracting [$label] ($mode) ==="
	emit_gen "$out" "$mode"
	grep -h '^/\* --- ' "$out/gen/"*.inc | sed 's/^/    /'
	echo "=== building [$label] ==="
	$CXX $CXXFLAGS -I"$HERE/shim" -I"$out" \
		-c "$HERE/shim/fbneo_bank_shim.cpp" -o "$out/shim.o"
	$CXX $CXXFLAGS -I"$HERE/shim" -I"$out" \
		-c "$HERE/bank_bounds_test.cpp" -o "$out/test.o"
	$CXX $CXXFLAGS "$out/shim.o" "$out/test.o" -o "$out/bank_bounds_test"
}

build_variant baseline base
build_variant patched  tree

echo "=== running ==="
"$BUILD/baseline/bank_bounds_test" > "$BUILD/baseline.out"
"$BUILD/patched/bank_bounds_test"  > "$BUILD/patched.out"

grep '^IN '  "$BUILD/baseline.out" > "$BUILD/baseline.in"
grep '^IN '  "$BUILD/patched.out"  > "$BUILD/patched.in"
grep '^OOB ' "$BUILD/baseline.out" > "$BUILD/baseline.oob"
grep '^OOB ' "$BUILD/patched.out"  > "$BUILD/patched.oob"

fail=0
pass() { echo "  PASS  $1"; }
bad()  { echo "  FAIL  $1"; fail=1; }

echo
echo "=== acceptance criteria (SPEC.md §6) ==="

# --- AC-1: the baseline reproduces the defect, for every unbounded setter ----
for tag in PVC/f0 PVC/f1 MS5PLUS KF2K3BLA MSLUGX; do
	n=$(awk -v t="$tag" '$3==t {sub(/^violations=/,"",$7); s+=$7} END{print s+0}' \
	    "$BUILD/baseline.oob")
	if [ "$n" -gt 0 ]; then
		pass "AC-1 baseline overshoots on $tag ($n out-of-range mappings)"
	else
		bad  "AC-1 baseline shows no overshoot on $tag -- corpus does not reach it"
	fi
done

# --- AC-2: the measured on-device case is reproduced exactly -----------------
m=$(grep '^RPT  measured' "$BUILD/baseline.out")
echo "        baseline: $m"
if echo "$m" | grep -q 'bank=0x010ffffe' \
   && echo "$m" | grep -q 'end=0x011fdffe' \
   && echo "$m" | grep -q 'over=9428990'; then
	pass "AC-2 baseline reproduces bank=0x010ffffe end=0x011fdffe over=9428990"
else
	bad  "AC-2 baseline does not match the on-device measurement"
fi
mp=$(grep '^RPT  measured' "$BUILD/patched.out")
echo "        patched : $mp"
if echo "$mp" | grep -q 'contained=1'; then
	pass "AC-2 patched contains the measured case"
else
	bad  "AC-2 patched does NOT contain the measured case"
fi

# --- AC-3 / AC-6: in-range results are byte-identical ------------------------
if diff -u "$BUILD/baseline.in" "$BUILD/patched.in" > "$BUILD/inrange.diff"; then
	lines=$(wc -l < "$BUILD/baseline.in" | tr -d ' ')
	pass "AC-3/AC-6 in-range output identical ($lines lines, incl. exhaustive hashes)"
else
	bad  "AC-3/AC-6 in-range output DIFFERS -- see $BUILD/inrange.diff"
	head -40 "$BUILD/inrange.diff"
fi

# in-range self-check, independent of the baseline
w=$(awk '{for(i=1;i<=NF;i++) if($i ~ /^wrong_inrange=/){sub(/^wrong_inrange=/,"",$i); s+=$i}} END{print s+0}' \
    "$BUILD/patched.in")
if [ "$w" -eq 0 ]; then
	pass "AC-3 patched: every in-range input is the identity (wrong_inrange=0)"
else
	bad  "AC-3 patched: $w in-range inputs did not map identically"
fi
v=$(awk '{for(i=1;i<=NF;i++) if($i ~ /^violations=/){sub(/^violations=/,"",$i); s+=$i}} END{print s+0}' \
    "$BUILD/patched.in")
[ "$v" -eq 0 ] && pass "AC-3 in-range corpus itself never leaves the allocation" \
               || bad  "AC-3 in-range corpus produced $v containment violations"

# --- AC-4: out-of-range containment -----------------------------------------
v=$(awk '{for(i=1;i<=NF;i++) if($i ~ /^violations=/){sub(/^violations=/,"",$i); s+=$i}} END{print s+0}' \
    "$BUILD/patched.oob")
tot=$(awk '{for(i=1;i<=NF;i++) if($i ~ /^cases=/){sub(/^cases=/,"",$i); s+=$i}} END{print s+0}' \
    "$BUILD/patched.oob")
bv=$(awk '{for(i=1;i<=NF;i++) if($i ~ /^violations=/){sub(/^violations=/,"",$i); s+=$i}} END{print s+0}' \
    "$BUILD/baseline.oob")
echo "        out-of-range cases: $tot   baseline violations: $bv   patched violations: $v"
if [ "$v" -eq 0 ]; then
	pass "AC-4 every out-of-range bank stays inside the allocation"
else
	bad  "AC-4 $v out-of-range mappings still leave the allocation"
fi

# --- AC-5: the fallback is the driver's own bank 0 ---------------------------
wf=$(awk '{for(i=1;i<=NF;i++) if($i ~ /^wrong_fallback=/){sub(/^wrong_fallback=/,"",$i); s+=$i}} END{print s+0}' \
    "$BUILD/patched.oob")
if [ "$wf" -eq 0 ]; then
	pass "AC-5 every clamped bank lands on the driver's own bank 0"
else
	bad  "AC-5 $wf clamped banks landed somewhere other than bank 0"
fi

# --- savestate restatement paths --------------------------------------------
grep '^RPT  restate' "$BUILD/baseline.out" | sed 's/^/        baseline: /'
grep '^RPT  restate' "$BUILD/patched.out"  | sed 's/^/        patched : /'
n=$(grep -c '^RPT  restate.*contained=1' "$BUILD/patched.out" || true)
if [ "$n" -eq 2 ]; then
	pass "restatement paths contain an out-of-range bank restored from a savestate"
else
	bad  "restatement paths: only $n/2 contained"
fi

# --- AC-7: hot-path cost -----------------------------------------------------
bt=$(awk '/^TIM /{for(i=1;i<=NF;i++) if($i ~ /^best_ns_per_call=/){sub(/^best_ns_per_call=/,"",$i); print $i}}' "$BUILD/baseline.out")
pt=$(awk '/^TIM /{for(i=1;i<=NF;i++) if($i ~ /^best_ns_per_call=/){sub(/^best_ns_per_call=/,"",$i); print $i}}' "$BUILD/patched.out")
echo "        NeoPVCBankswitch ns/call -- baseline: $bt   patched: $pt"
echo "        (harness-loop figure: includes the harness's own per-call setup;"
echo "         see RESULTS.md for the instruction-level comparison)"

echo
if [ "$fail" -eq 0 ]; then
	echo "ALL CHECKS PASSED"
else
	echo "SOME CHECKS FAILED"
fi
exit "$fail"
