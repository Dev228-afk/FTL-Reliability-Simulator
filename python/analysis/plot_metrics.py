#!/usr/bin/env python3
"""
Analysis and visualization tool for FTL simulation reports.
Plots Write Amplification Factor (WAF), GC cycles, and erase-count wear distributions.
"""

import argparse
import json
import sys


def print_report_summary(report_path: str):
    with open(report_path, "r", encoding="utf-8") as f:
        data = json.load(f)

    print("\n" + "=" * 60)
    print(f" FTL SIMULATION RUN: {data.get('run_id')} (Seed: {data.get('seed')})")
    print("=" * 60)
    print(f"  Mode:                {data.get('recovery_mode')}")
    print(f"  Total Ops:           {data['trace_stats']['total_ops']}")
    print(f"  Host Writes:         {data['trace_stats']['host_writes']}")
    print(f"  GC Cycles:           {data['gc_stats']['gc_cycles']}")
    print(f"  Pages Relocated:     {data['gc_stats']['gc_pages_relocated']}")
    print(f"  Write Amplification: {data['gc_stats']['write_amplification']:.2f}x")
    print("-" * 60)
    print(f"  Wear Distribution:")
    print(f"    Min Erase:         {data['wear']['min_erase']}")
    print(f"    Max Erase:         {data['wear']['max_erase']}")
    print(f"    Mean Erase:        {data['wear']['mean_erase']:.2f}")
    print(f"    StdDev Erase:      {data['wear']['stddev_erase']:.2f}")
    print("-" * 60)
    print(f"  Crash Injected:      {data['crash']['crashed']}")
    if data['crash']['crashed']:
        print(f"    Point:             {data['crash']['point']} (seq {data['crash']['seq']})")
        print(f"    Recovery Success:  {data['recovery']['success']}")
    print("-" * 60)
    inv = data["invariants"]
    print(f"  Structural Invariants: {'PASS' if inv['structural_passed'] else 'FAIL'}")
    print(f"  Durability Invariants: {'PASS' if inv['durability_passed'] else 'FAIL'}")
    if inv.get("violations"):
        print(f"  Violations ({len(inv['violations'])}):")
        for v in inv["violations"]:
            print(f"    * [{v['name']}] {v['details']}")
    print("=" * 60 + "\n")


def main():
    parser = argparse.ArgumentParser(description="Analyze FTL simulation reports")
    parser.add_argument("report_json", help="Path to simulation report JSON")
    args = parser.parse_args()

    print_report_summary(args.report_json)


if __name__ == "__main__":
    main()
