#!/usr/bin/env python3
"""
Deterministic workload trace generator for the FTL Reliability Simulator.
Produces JSONL traces with IEEE 802.3 CRC32 checksums matching the C++ core.
"""

import argparse
import json
import random
import zlib


def generate_trace(workload_type: str, num_ops: int, lba_count: int, seed: int, output_path: str):
    rng = random.Random(seed)
    records = []

    for seq in range(1, num_ops + 1):
        if workload_type == "seq_write":
            op = "WRITE"
            lba = (seq - 1) % lba_count
        elif workload_type == "rand_write":
            op = "WRITE"
            lba = rng.randint(0, lba_count - 1)
        elif workload_type == "churn":
            op = "WRITE"
            hot_range = max(10, lba_count // 8)
            lba = rng.randint(0, hot_range - 1)
        elif workload_type == "trim_heavy":
            roll = rng.random()
            if roll < 0.40:
                op = "WRITE"
            elif roll < 0.70:
                op = "TRIM"
            else:
                op = "READ"
            lba = rng.randint(0, lba_count - 1)
        else:  # mixed_70_30
            roll = rng.random()
            if roll < 0.70:
                op = "WRITE"
            elif roll < 0.95:
                op = "READ"
            else:
                op = "TRIM"
            lba = rng.randint(0, lba_count - 1)

        rec = {
            "seq": seq,
            "op": op,
            "lba": lba
        }

        if op == "WRITE":
            pattern_byte = (lba + seq) & 0xFF
            payload = bytes([pattern_byte] * 4096)
            crc = zlib.crc32(payload) & 0xFFFFFFFF
            rec["size"] = 4096
            rec["data_pattern"] = pattern_byte
            rec["crc"] = f"{crc:08x}"

        records.append(rec)

    with open(output_path, "w", encoding="utf-8") as f:
        for r in records:
            f.write(json.dumps(r) + "\n")

    print(f"Generated {len(records)} trace records ({workload_type}) to {output_path}")


def main():
    parser = argparse.ArgumentParser(description="Generate deterministic FTL workload trace")
    parser.add_argument("--type", choices=["seq_write", "rand_write", "mixed_70_30", "churn", "trim_heavy"], default="mixed_70_30")
    parser.add_argument("--ops", type=int, default=5000)
    parser.add_argument("--lbas", type=int, default=1024)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--output", type=str, default="trace.jsonl")
    args = parser.parse_args()

    generate_trace(args.type, args.ops, args.lbas, args.seed, args.output)


if __name__ == "__main__":
    main()
