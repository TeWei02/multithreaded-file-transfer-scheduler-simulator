#!/usr/bin/env python3
"""Bundle the C++ reference outputs into docs/oracle_bundle.js.

The browser page docs/verify.html replays every scenario listed here with the
JavaScript port of the simulation core (docs/engine.js) and compares the result
cell by cell against these reference CSVs.

Usage:
    python3 tools/build_oracle_bundle.py [--oracle oracle] [--out docs/oracle_bundle.js]

Prerequisite: the reference CSVs must already exist (see tools/run_oracle.sh).
"""

import argparse
import json
import os
import sys

SCHEDULERS = ["fifo", "sjf", "priority", "rr", "wfq"]

# (scenario name, input path relative to the repository, threads, quantum, overhead)
SCENARIOS = [
    ("s1_sample_t4_q1.0_o0.05", "data/sample_requests.csv", 4, 1.0, 0.05),
    ("s2_n50_t3_q0.5_o0.0", "data/generated/n50_seed7.csv", 3, 0.5, 0.0),
    ("s3_n120_t8_q2.5_o0.2", "data/generated/n120_seed42.csv", 8, 2.5, 0.2),
    ("s4_n37_t1_q1.0_o0.05", "data/generated/n37_seed2026.csv", 1, 1.0, 0.05),
    ("s5_n15_t5_q3.0_o0.1", "data/generated/n15_seed99.csv", 5, 3.0, 0.1),
    ("s6_edge_t2_q0.25_o0.0", "data/edge_cases.csv", 2, 0.25, 0.0),
    ("s7_edge_t6_q1.0_o0.05", "data/edge_cases.csv", 6, 1.0, 0.05),
    ("s8_sample_t1_q1.0_o0.05", "data/sample_requests.csv", 1, 1.0, 0.05),
    ("s9_sample_t8_q0.1_o0.5", "data/sample_requests.csv", 8, 0.1, 0.5),
    ("s10_edge_t3_q1.0_o0.02", "data/edge_cases.csv", 3, 1.0, 0.02),
    ("s11_n120_t4_q0.75_o0.0", "data/generated/n120_seed42.csv", 4, 0.75, 0.0),
    ("s12_n50_t16_q5.0_o0.25", "data/generated/n50_seed7.csv", 16, 5.0, 0.25),
]


def rows_from(path):
    """Parse a CSV file into a list of dicts (no external dependency)."""
    with open(path, "r", encoding="utf-8") as handle:
        lines = [line.strip() for line in handle if line.strip()]
    if not lines:
        raise ValueError("empty CSV: %s" % path)
    header = lines[0].split(",")
    return [dict(zip(header, line.split(","))) for line in lines[1:]]


def build(repo_root, oracle_dir):
    bundle = {"scenarios": []}

    for name, rel_input, threads, quantum, overhead in SCENARIOS:
        input_path = os.path.join(repo_root, rel_input)
        scenario_dir = os.path.join(oracle_dir, name)

        with open(input_path, "r", encoding="utf-8") as handle:
            csv_text = handle.read()

        for sched in SCHEDULERS:
            result_path = os.path.join(scenario_dir, "result_%s.csv" % sched)
            if not os.path.isfile(result_path):
                sys.exit("missing reference output: %s (run tools/run_oracle.sh first)" % result_path)

        summaries = rows_from(os.path.join(scenario_dir, "summary.csv"))
        expected_rows = {
            sched: rows_from(os.path.join(scenario_dir, "result_%s.csv" % sched))
            for sched in SCHEDULERS
        }
        expected_summary = {
            sched: next(row for row in summaries if row["scheduler"] == sched)
            for sched in SCHEDULERS
        }

        bundle["scenarios"].append({
            "name": name,
            "inputCsv": csv_text,
            "inputLabel": rel_input,
            "config": {"threads": threads, "quantumSec": quantum, "overheadSec": overhead},
            "expectedRows": expected_rows,
            "expectedSummary": expected_summary,
        })

    return bundle


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", default=os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    parser.add_argument("--oracle", default=None, help="reference output directory (default: <repo>/oracle)")
    parser.add_argument("--out", default=None, help="bundle path (default: <repo>/docs/oracle_bundle.js)")
    args = parser.parse_args()

    repo_root = os.path.abspath(args.repo)
    oracle_dir = args.oracle or os.path.join(repo_root, "oracle")
    out_path = args.out or os.path.join(repo_root, "docs", "oracle_bundle.js")

    bundle = build(repo_root, oracle_dir)

    with open(out_path, "w", encoding="utf-8") as handle:
        handle.write("window.ORACLE_BUNDLE = ")
        json.dump(bundle, handle)
        handle.write(";\n")

    print("wrote %s (%d bytes, %d scenarios)" % (out_path, os.path.getsize(out_path), len(bundle["scenarios"])))


if __name__ == "__main__":
    main()
