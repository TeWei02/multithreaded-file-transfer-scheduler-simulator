#!/usr/bin/env bash
#
# Regenerate the reference ("oracle") outputs that back docs/verify.html.
#
# The reference files are produced by the real C++ engine, never by the browser
# port: every scenario below runs ./simulator --benchmark and the resulting CSVs
# are bundled into docs/oracle_bundle.js by tools/build_oracle_bundle.py.
#
# The digest of every generated CSV is recorded in data/reference/oracle_sha256.txt
# so the reference set can be audited without committing the CSV payloads:
#
#   bash tools/run_oracle.sh            # recompute + refresh the digest manifest
#   bash tools/run_oracle.sh --check    # recompute + fail if any digest moved (CI)
#
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

MODE="write"
ORACLE=""
for arg in "$@"; do
  case "$arg" in
    --check) MODE="check" ;;
    -h|--help)
      echo "usage: $0 [--check] [oracle_dir]"
      exit 0
      ;;
    *) ORACLE="$arg" ;;
  esac
done
ORACLE="${ORACLE:-$REPO_ROOT/oracle}"
MANIFEST="$REPO_ROOT/data/reference/oracle_sha256.txt"

sha256() {
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$1" | cut -d' ' -f1
  else
    shasum -a 256 "$1" | cut -d' ' -f1
  fi
}

if [ ! -x ./simulator ]; then
  echo "== building simulator =="
  make
fi

status=0

echo "== regenerating the deterministic workloads in data/generated =="
./simulator --generate-sample data/generated/n15_seed99.csv   --n 15  --seed 99
./simulator --generate-sample data/generated/n37_seed2026.csv --n 37  --seed 2026
./simulator --generate-sample data/generated/n50_seed7.csv    --n 50  --seed 7
./simulator --generate-sample data/generated/n120_seed42.csv  --n 120 --seed 42

if command -v git >/dev/null 2>&1; then
  if git diff --quiet -- data/generated; then
    echo "== data/generated is unchanged: the generator is deterministic =="
  else
    echo "!! data/generated differs from the committed files (git diff data/generated)" >&2
    status=1
  fi
fi

mkdir -p "$ORACLE"

run () { # name  input  threads  quantum  overhead
  local name="$1" input="$2" threads="$3" quantum="$4" overhead="$5"
  mkdir -p "$ORACLE/$name"
  ./simulator --input "$input" --benchmark --threads "$threads" \
      --quantum "$quantum" --overhead "$overhead" \
      --output-dir "$ORACLE/$name" > "$ORACLE/$name/console.txt"
  echo "  $name"
}

echo "== running the reference scenarios =="
run s1_sample_t4_q1.0_o0.05   data/sample_requests.csv     4 1.0 0.05
run s2_n50_t3_q0.5_o0.0       data/generated/n50_seed7.csv    3 0.5 0.0
run s3_n120_t8_q2.5_o0.2      data/generated/n120_seed42.csv  8 2.5 0.2
run s4_n37_t1_q1.0_o0.05      data/generated/n37_seed2026.csv 1 1.0 0.05
run s5_n15_t5_q3.0_o0.1       data/generated/n15_seed99.csv   5 3.0 0.1
run s6_edge_t2_q0.25_o0.0     data/edge_cases.csv          2 0.25 0.0
run s7_edge_t6_q1.0_o0.05     data/edge_cases.csv          6 1.0 0.05
run s8_sample_t1_q1.0_o0.05   data/sample_requests.csv     1 1.0 0.05
run s9_sample_t8_q0.1_o0.5    data/sample_requests.csv     8 0.1 0.5
run s10_edge_t3_q1.0_o0.02    data/edge_cases.csv          3 1.0 0.02
run s11_n120_t4_q0.75_o0.0    data/generated/n120_seed42.csv  4 0.75 0.0
run s12_n50_t16_q5.0_o0.25    data/generated/n50_seed7.csv   16 5.0 0.25

echo "== digesting the generated reference CSVs =="
generated="$(cd "$ORACLE" && find . -name '*.csv' | LC_ALL=C sort)"
count=0
mkdir -p "$(dirname "$MANIFEST")"
: > "$MANIFEST.tmp"
for rel in $generated; do
  clean="${rel#./}"
  echo "$(sha256 "$ORACLE/$clean")  oracle/$clean" >> "$MANIFEST.tmp"
  count=$((count + 1))
done

if [ "$MODE" = "check" ]; then
  if [ ! -f "$MANIFEST" ]; then
    echo "!! missing manifest $MANIFEST" >&2
    rm -f "$MANIFEST.tmp"
    exit 1
  fi
  if diff -u "$MANIFEST" "$MANIFEST.tmp"; then
    echo "== $count reference CSVs match data/reference/oracle_sha256.txt =="
  else
    echo "!! the C++ reference outputs changed (see diff above)" >&2
    status=1
  fi
  rm -f "$MANIFEST.tmp"
else
  mkdir -p "$(dirname "$MANIFEST")"
  mv "$MANIFEST.tmp" "$MANIFEST"
  echo "== $count reference CSVs digested into ${MANIFEST#$REPO_ROOT/} =="
fi

echo "== oracle outputs written to $ORACLE =="
echo "   next: python3 tools/build_oracle_bundle.py --oracle \"$ORACLE\""
exit "$status"
