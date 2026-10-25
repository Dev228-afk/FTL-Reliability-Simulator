# FTL Reliability Simulator: Evidence & Verification Report

## 1. Test Suite Summary
- **Target Architecture**: C++17, GoogleTest suite (`ftl_tests`), `ctest` harness
- **Platform**: Ubuntu 24.04 LTS (x86_64), GCC 13.3.0
- **Total Tests**: 24
- **Passed Tests**: 24 (100% passing)
- **Failed Tests**: 0

| Test Name | Domain | Execution Time | Status |
|:---|:---|:---:|:---:|
| `ChecksumTest.Crc32Empty` | Checksum / CRC | 0.01s | PASS |
| `ChecksumTest.Crc32StandardVectors` | Checksum / CRC | 0.02s | PASS |
| `ChecksumTest.Crc32SeedChaining` | Checksum / CRC | 0.01s | PASS |
| `ConfigTest.LogicalPagesAndOverprovisioning` | Config / Geometry | 0.01s | PASS |
| `ConfigTest.JsonRoundTrip` | Config / Serialization | 0.01s | PASS |
| `ConfigTest.SimpleYamlParsing` | Config / Parser | 0.01s | PASS |
| `ConfigTest.ValidationRejectsInvalidValues` | Config / Validation | 0.01s | PASS |
| `NandTest.EraseBeforeWriteConstraint` | NAND Flash Model | 0.01s | PASS |
| `NandTest.ReadProgramDataIntegrity` | NAND Flash Model | 0.01s | PASS |
| `NandTest.WearDistribution` | NAND Flash Model | 0.01s | PASS |
| `WalTest.AppendAndReadRecords` | WAL / Durability | 0.04s | PASS |
| `WalTest.TornWriteDetection` | WAL / Crash Safety | 0.04s | PASS |
| `CheckpointTest.SaveAndLoadIntegrity` | Checkpoint / Persistence | 0.03s | PASS |
| `CheckpointTest.CorruptedCheckpointRejection` | Checkpoint / Integrity | 0.03s | PASS |
| `TraceTest.SaveAndLoadTrace` | Workload / Traces | 0.03s | PASS |
| `MappingTest.BasicWriteReadTrim` | FTL Core / Mapping | 0.03s | PASS |
| `MappingTest.OverwriteInvalidatesOldPage` | FTL Core / Allocation | 0.04s | PASS |
| `GcIntegrationTest.OutOfPlaceWritesTriggerGc` | GC / Relocation | 4.46s | PASS |
| `WearLevelingTest.WearDistributionRemainsEven` | Wear Leveling | 6.79s | PASS |
| `RestartTest.CleanCheckpointRestartRestoresAllMappings` | Clean Restart | 0.71s | PASS |
| `CrashTest.InjectedCrashPointsThrowException` | Fault Injection | 0.07s | PASS |
| `VulnerableDefectTest.VulnerableModeLosesAcknowledgedWritesAfterCrash` | Defect Reproduction | 0.21s | PASS |
| `VulnerableDefectTest.WalModeSurvivesIdenticalCrashWithZeroViolations` | Defect Mitigation | 0.22s | PASS |
| `SeedRegressionTest.ThirtyFaultSeedsComparison` | 30 Seeds Comparison | 30.29s | PASS |

---

## 2. 30 Fault Seeds Empirical Regression Results

The core research finding compares **Vulnerable Mode** (delayed in-memory checkpoints) vs **Fixed WAL Mode** (per-record CRC32 write-ahead logging) across 30 pseudorandom SplitMix64 seeds with power-loss crash injection at `CrashPoint::AfterAck`:

```text
================================================================================
Mode         | Runs | Crashes Injected | Corrupted Runs | Corruption Rate
================================================================================
Vulnerable   |  30  |        30        |       30       |     100.0%
WAL (Fixed)  |  30  |        30        |        0       |       0.0%
================================================================================
```

### Invariant Violations Caught
1. **`AcknowledgedWritesSurvive` (Durability)**: Caught in 30/30 vulnerable runs where the host received write completion confirmation prior to crash, but unpersisted volatile L2P table updates caused the recovered controller to revert to pre-crash checkpoint state.
2. **Zero Failures under WAL**: In 30/30 runs under WAL mode, all acknowledged writes survived reboot with matching 4096-byte payloads and CRC32 verification.
