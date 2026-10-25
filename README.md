# FTL Reliability Simulator

A deterministic Flash Translation Layer (FTL) and NAND Reliability Simulator built in modern C++20 for evaluating power-loss recovery, write-ahead logging (WAL), wear leveling, and invariant verification under simulated hardware crashes.

---

## Key Features

- **Page-Level Flash Translation Layer (FTL)**: Logical-to-Physical block mapping, out-of-place updates, and garbage collection.
- **Power-Loss & Crash Recovery**: Write-Ahead Logging (WAL) and checkpointing mechanisms designed to prevent data corruption during sudden power failures.
- **Deterministic Crash Injection**: Systematically inject power-loss faults at precise pipeline stages (pre-write, mid-write, post-write, checkpoint creation).
- **Invariant Verification Engine**: Runtime verification of critical storage invariants (LBA uniqueness, mapping table consistency, wear-level deviation thresholds).
- **Telemetry & Evidence Reporting**: Structured JSON and Markdown summary generation with Python visualization tools.

---

## Directory Structure

```
├── CMakeLists.txt              # CMake build configuration (C++20)
├── Makefile                    # Convenience commands for build and test
├── configs/                    # Simulation and crash scenarios (default, vulnerable, wal)
├── evidence/                   # Test evidence reports and validation artifacts
├── include/                    # Header files
│   ├── ftl/                    # Core FTL structures, checksums, and configurations
│   ├── invariants/             # Structural and state consistency invariants
│   ├── nand/                   # NAND flash memory and block/page models
│   ├── recovery/               # WAL, checkpointing, and crash recovery logic
│   └── workload/               # Trace runner and performance reporting
├── python/                     # Trace generation and metrics plotting scripts
├── src/                        # Implementation files
├── tests/                      # Unit, integration, crash, and regression suites
└── third_party/                # Header-only dependencies (nlohmann/json)
```

---

## Getting Started

### Prerequisites

- **C++ Compiler**: GCC 11+, Clang 13+, or MSVC with C++20 support
- **CMake**: Version 3.20 or newer
- **Make** / **Ninja** (optional)
- **Python**: 3.9+ (for analysis & trace generation)

### Build Instructions

```bash
# Generate build files
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Compile the simulator and test suites
cmake --build build --config Release
```

### Running Tests

```bash
# Run all tests using CTest
ctest --test-dir build --output-on-failure
```

---

## License

MIT License.
