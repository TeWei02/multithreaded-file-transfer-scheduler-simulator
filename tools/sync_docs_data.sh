#!/usr/bin/env bash
#
# Copy the canonical workload files into docs/data/ so that the static demo in
# docs/ is self-contained (GitHub Pages serves only the docs/ folder).
#
# The site cannot fetch from ../data, therefore the two copies must stay
# byte-identical; this script is the single supported way to refresh them.
#
# Usage:
#   bash tools/sync_docs_data.sh           # copy data/ -> docs/data/
#   bash tools/sync_docs_data.sh --check   # verify both copies are identical (CI)
#
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

MODE="sync"
if [ "${1:-}" = "--check" ]; then
  MODE="check"
elif [ -n "${1:-}" ]; then
  echo "usage: $0 [--check]" >&2
  exit 2
fi

sha256() {
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$1" | cut -d' ' -f1
  else
    shasum -a 256 "$1" | cut -d' ' -f1
  fi
}

# canonical file                     -> file name used by the web demo
PAIRS=(
  "data/sample_requests.csv:docs/data/sample_requests.csv"
  "data/generated/n15_seed99.csv:docs/data/n15_seed99.csv"
  "data/generated/n37_seed2026.csv:docs/data/n37_seed2026.csv"
  "data/generated/n50_seed7.csv:docs/data/n50_seed7.csv"
  "data/generated/n120_seed42.csv:docs/data/n120_seed42.csv"
  "data/edge_cases.csv:docs/data/boundary_workload.csv"
)

status=0
for pair in "${PAIRS[@]}"; do
  src="${pair%%:*}"
  dst="${pair##*:}"

  if [ "$MODE" = "check" ]; then
    if [ ! -f "$dst" ]; then
      echo "MISSING  $dst (run tools/sync_docs_data.sh)" >&2
      status=1
      continue
    fi
    src_hash="$(sha256 "$src")"
    dst_hash="$(sha256 "$dst")"
    if [ "$src_hash" = "$dst_hash" ]; then
      echo "OK       $src == $dst  ($src_hash)"
    else
      echo "DRIFT    $src != $dst (run tools/sync_docs_data.sh)" >&2
      status=1
    fi
    continue
  fi

  mkdir -p "$(dirname "$dst")"
  cp "$src" "$dst"
  echo "$(sha256 "$src")  $src -> $dst"
done

if [ "$MODE" = "check" ]; then
  if [ "$status" -eq 0 ]; then
    echo
    echo "docs/data is in sync with data/ (byte-identical, SHA-256 above)."
  fi
  exit "$status"
fi

echo
echo "docs/data refreshed. Both copies are byte-identical (SHA-256 above)."
