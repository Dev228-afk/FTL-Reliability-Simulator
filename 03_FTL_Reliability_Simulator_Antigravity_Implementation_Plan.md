# FTL Reliability Simulator — AI-Agent Implementation Plan

## 1. Project Mission

Build the **FTL Reliability Simulator** as a deterministic C++17 flash translation layer and crash-recovery testbed.

This project is based on the resume claim:

> FTL Reliability Simulator – Trace-driven flash translation layer and recovery testbed (C++17, Python, CMake, GoogleTest)

Priority:

1. **Implement the FTL behaviors claimed on the resume.**
2. **Create deterministic crash/fault injection and invariants.**
3. **Reproduce an ordering/corruption bug in a deliberately vulnerable mode and fix it with write-ahead recovery.**
4. **Generate deep interview material around NAND, FTL, durability, GC, wear leveling, recovery, and systems debugging.**
5. **Provide a visualization/replay tool only after correctness is proven.**

This is not intended to emulate a commercial SSD controller.

It is an educational/research simulator that demonstrates storage-system reasoning.

---

# 2. Resume Claims This Project Must Support

Implementation must cover:

- logical-to-physical mapping
- page/block allocation
- invalidation
- garbage collection
- wear leveling
- over-provisioning
- configurable NAND program/read/erase latency
- checksummed write traces
- abrupt-reset injection
- persisted metadata checkpoints
- replay recovery
- invariants for:
  - mapping consistency
  - stale reads
  - duplicate ownership
  - acknowledged-write durability
- deterministic mixed workloads
- endurance simulation
- at least 30 fault seeds
- a reproducible metadata-ordering defect
- write-ahead recovery
- no observed mapping corruption in the post-fix regression suite

The project must reproduce these behaviors honestly.

Do not artificially make tests pass.

---

# 3. Core Architecture

```text
Workload Trace
      |
      v
FTL API
      |
      +-- Read(LBA)
      +-- Write(LBA, data)
      +-- Trim(LBA)
      +-- Flush()
      |
      v
Logical-to-Physical Map
      |
      v
Allocator
      |
      +-- free-page management
      +-- block state
      +-- invalidation
      |
      v
NAND Model
      |
      +-- program
      +-- read
      +-- erase
      +-- latency
      +-- wear count
      |
      v
Persistence Layer
      |
      +-- metadata checkpoint
      +-- write-ahead log
      +-- crash injection
```

---

# 4. Technology Stack

Required:

```text
C++17
CMake
GoogleTest
Python 3.11+
JSON/CSV
```

Recommended:

```text
clang-format
clang-tidy
AddressSanitizer
UndefinedBehaviorSanitizer
ThreadSanitizer only if concurrency added
gcov/lcov or llvm-cov
```

Do not add concurrency early.

Correctness and deterministic recovery matter more.

---

# 5. Repository Structure

```text
ftl-reliability-simulator/
├── README.md
├── PROJECT_CONSTITUTION.md
├── CMakeLists.txt
├── Makefile
├── .agents/
│   ├── rules/
│   ├── skills/
│   └── agents/
├── include/
│   ├── ftl/
│   ├── nand/
│   ├── recovery/
│   ├── workload/
│   └── invariants/
├── src/
│   ├── ftl/
│   ├── nand/
│   ├── recovery/
│   ├── workload/
│   └── main.cpp
├── tests/
│   ├── unit/
│   ├── integration/
│   ├── crash/
│   └── regression/
├── python/
│   ├── trace_generator/
│   ├── analysis/
│   └── visualization/
├── configs/
├── traces/
├── evidence/
├── experiments/
└── interview/
```

---

# 6. NAND Geometry Model

Configuration:

```yaml
channels: 1
dies_per_channel: 1
blocks_per_die: 128
pages_per_block: 256
page_size_bytes: 4096
overprovision_percent: 20
```

Keep the first version simple.

Later allow multiple channels/dies for architecture discussions.

Track physical page state:

```text
FREE
VALID
INVALID
```

Track block:

```text
erase_count
valid_pages
invalid_pages
free_pages
```

---

# 7. NAND Timing Model

Configurable:

```text
read latency
program latency
erase latency
```

Example values may be synthetic.

Never claim they match a specific NAND generation unless sourced.

The simulator may model latency as:

```text
fixed
+
optional jitter
+
optional wear penalty
```

Use deterministic RNG seed.

---

# 8. FTL Mapping

Start with page-level mapping:

```text
LBA -> PPA
```

Where PPA includes:

```text
channel
die
block
page
```

Required structures:

```text
forward map
reverse map
```

Reverse map is important for garbage collection and invariants.

---

# 9. Write Path

Basic write:

```text
receive LBA + data
      |
allocate new physical page
      |
program page
      |
update mapping
      |
invalidate old physical page
      |
return acknowledgement
```

The exact update ordering becomes important for crash-consistency experiments.

---

# 10. Read Path

Read:

```text
lookup LBA
      |
resolve PPA
      |
read page
      |
verify ownership
      |
return data
```

Detect:

```text
unmapped read
stale mapping
wrong physical owner
corrupt checksum
```

---

# 11. Trim / DSM

Implement logical trim:

```text
Trim(LBA)
```

Effects:

```text
remove logical mapping
invalidate current physical page
future reads return unmapped/zero according to simulator contract
```

Use this to discuss NVMe DSM/TRIM concepts in interviews.

---

# 12. Allocation

Begin with:

```text
sequential free-page allocator
```

Then add:

```text
greedy block selection
```

Do not spend early effort on sophisticated commercial allocation algorithms.

---

# 13. Garbage Collection

Trigger on:

```text
free-page watermark
```

Basic GC:

1. choose victim block
2. copy valid pages
3. update mappings
4. erase victim
5. return pages to free pool

Victim policies:

```text
greedy invalid-page count
cost-benefit optional
```

Metrics:

```text
GC cycles
pages moved
write amplification
erase count
GC latency
```

---

# 14. Wear Leveling

Implement a simple mechanism.

Track:

```text
erase_count per block
min
max
mean
standard deviation
```

Policy:

```text
avoid repeatedly using same low-numbered blocks
periodically relocate cold data if imbalance threshold exceeded
```

Keep it explainable.

Measure:

```text
max-min erase count
variance
lifetime proxy
```

---

# 15. Over-Provisioning

Configurable reserved physical capacity.

Experiment:

```text
0%
7%
20%
30%
```

Measure:

```text
GC frequency
write amplification
free-page pressure
latency
```

Do not claim a universal optimum.

---

# 16. Workload Trace Format

JSONL or binary-simple format.

Example:

```json
{"seq":1,"op":"WRITE","lba":100,"data":"..."}
{"seq":2,"op":"READ","lba":100}
{"seq":3,"op":"TRIM","lba":100}
{"seq":4,"op":"FLUSH"}
```

Every write has a checksum.

Generate traces with Python.

---

# 17. Workload Types

Required:

```text
sequential write
random write
sequential read
random read
70/30 mixed
hot/cold LBA
overwrite-heavy
trim-heavy
fill-and-churn
endurance loop
```

All generators accept a seed.

---

# 18. Data Integrity

Each logical write carries:

```text
sequence number
LBA
payload
checksum
```

Verification after every recovery may compare:

```text
expected acknowledged data
actual FTL read
```

---

# 19. Persistence Model

Persist simulator state into a durable directory.

Files may include:

```text
nand.bin
checkpoint.meta
journal.log
run.meta
```

The persistence system must simulate ordering explicitly.

Do not rely on host filesystem behavior as the only durability model.

Model durable writes within the simulator.

---

# 20. Metadata Checkpoints

Checkpoint stores:

```text
mapping
reverse mapping
block states
allocator state
erase counts
checkpoint sequence
checksum
```

Checkpoint must be checksummed.

Support:

```text
periodic checkpoint
explicit checkpoint
recovery from latest valid checkpoint
```

---

# 21. Baseline Vulnerable Recovery Mode

Create a deliberately vulnerable mode representing bad metadata ordering.

Important:

This must be clearly labeled:

```text
experimental vulnerable implementation
```

The goal is to reproduce the resume's metadata-ordering defect.

Example vulnerable sequence:

```text
program new data
acknowledge write
update in-memory mapping
crash before durable metadata update
```

or another realistic ordering flaw.

After restart, a previously acknowledged write can become:

```text
stale
missing
mapped inconsistently
```

The failure must be detected by invariants.

---

# 22. Crash Injection

Allow deterministic crash points.

Examples:

```text
AFTER_DATA_PROGRAM
BEFORE_MAPPING_UPDATE
AFTER_MAPPING_UPDATE
BEFORE_INVALIDATION
DURING_CHECKPOINT
AFTER_ACK
DURING_GC_COPY
BEFORE_BLOCK_ERASE
```

Injection:

```text
seed
operation index
crash point
```

Run must be exactly reproducible.

---

# 23. Recovery

Startup:

```text
load latest valid checkpoint
      |
      v
validate checksum
      |
      v
replay durable log
      |
      v
rebuild/verify reverse map
      |
      v
run invariants
```

Return:

```text
RECOVERY_OK
RECOVERY_FAILED
```

with evidence.

---

# 24. Write-Ahead Recovery

Implement fixed mode.

Before externally acknowledging a metadata-changing operation:

```text
append intent/update to WAL
make WAL durable in simulator model
apply mapping state
acknowledge
```

On recovery:

```text
checkpoint
+
WAL replay
```

must recover acknowledged operations.

Document ordering carefully.

---

# 25. Invariants

Required invariants:

## Mapping Consistency

```text
every mapped LBA points to a VALID page
```

## Reverse Mapping

```text
forward and reverse maps agree
```

## Unique Ownership

```text
no physical page belongs to multiple active LBAs
```

## No Stale Reads

```text
read(LBA) returns latest acknowledged value
```

## Durability

```text
all writes acknowledged before crash according to durability contract survive recovery
```

## Free-Page Consistency

```text
FREE page cannot be mapped
```

## Block Accounting

```text
free + valid + invalid == pages_per_block
```

Run invariants after:

```text
every operation in debug profile
every checkpoint
every recovery
every GC
```

---

# 26. 30 Fault Seeds

Command:

```bash
make fault-regression SEEDS=30
```

For every seed:

```text
generate deterministic workload
choose deterministic crash points
execute vulnerable mode
recover
run invariants
```

Then run fixed/WAL mode on identical seeds.

Evidence should show:

```text
vulnerable failures
fixed failures
```

The desired result is zero observed corruption in the fixed regression set.

Do not hard-code tests to pass.

If a fixed-mode failure occurs, preserve it and fix the implementation.

---

# 27. Endurance Simulation

Run many write/erase cycles.

Metrics:

```text
host writes
NAND writes
write amplification
erase counts
GC cycles
max erase count
min erase count
wear spread
simulated latency
```

No claim should be made about real SSD lifetime.

Call it a model.

---

# 28. Write Amplification

Calculate:

```text
NAND bytes programmed / host bytes written
```

Track separately:

```text
user writes
GC relocation writes
metadata writes
```

Use to compare:

```text
overprovisioning
GC policy
workload
```

---

# 29. Deterministic Replay

Every run writes:

```text
config
seed
trace
crash injection
build commit
result
```

Command:

```bash
ftl-sim replay evidence/failures/failure_017/run.json
```

must reproduce the same defect.

This is a core interview-strength feature.

---

# 30. Testing Strategy

## Unit Tests

```text
mapping
allocator
block accounting
checksum
WAL encoding
checkpoint encoding
trace parser
```

## Integration

```text
write/read
overwrite
trim
GC
checkpoint/restart
```

## Crash Tests

```text
every injection point
```

## Property/Invariants

Run randomized operation sequences with fixed seeds.

---

# 31. Sanitizers

CI must run:

```text
AddressSanitizer
UndefinedBehaviorSanitizer
```

Fix all sanitizer issues before project completion.

Optional:

```text
ThreadSanitizer
```

only if concurrency exists.

---

# 32. Performance Is Secondary

Do not over-optimize the simulator.

Measure:

```text
operations/sec
memory use
simulation runtime
```

but correctness and recovery behavior are more important.

---

# 33. Python Analysis Tools

Use Python for:

```text
trace generation
seed orchestration
experiment matrix
CSV analysis
plots
timeline/recovery visualization
```

Do not move FTL core logic into Python.

The resume says C++17; the simulator core must remain C++.

---

# 34. Visualization

Secondary reviewer surface.

Show:

```text
LBA -> PPA mapping
block occupancy
valid/invalid/free pages
GC event timeline
erase-count distribution
crash point
recovery replay
invariant result
```

Simple Plotly or static HTML is sufficient.

---

# 35. Cloud Use

Cloud is not required for the FTL simulator.

Optional:

```text
GitHub Actions / Jenkins
AWS CodeBuild or EC2
S3 experiment artifacts
```

Cloud does not improve the core storage-system story.

Do not spend time deploying this to Kubernetes.

---

# 36. Antigravity Agent Structure

Lead:

```text
ftl-lead
```

Subagents:

```text
nand-model-agent
mapping-gc-agent
recovery-agent
fault-testing-agent
analysis-agent
interview-verification-agent
```

The recovery agent and fault-testing agent should remain somewhat independent so the test agent actively challenges recovery assumptions.

---

# 37. Antigravity Rules

Create `.agents/rules/project-focus.md`.

```text
PRIMARY GOAL:
Build a deterministic FTL/recovery simulator matching the resume.

CORRECTNESS:
Never suppress an invariant failure.
Never hard-code tests to pass.
Every crash failure must be reproducible by seed.

SCOPE:
This is not a commercial SSD emulator.
Do not implement unnecessary NVMe host protocol layers.
Do not implement advanced multi-channel concurrency until single-thread correctness is complete.

PERSISTENCE:
Explicitly model durability and ordering.
Do not accidentally rely on host filesystem ordering semantics.

RESUME:
Reproduce the metadata-ordering defect in a clearly marked vulnerable mode.
Then fix it using write-ahead recovery.
Run identical fault seeds before and after.

INTERVIEW:
Document every major policy:
- mapping
- allocation
- GC
- wear leveling
- overprovisioning
- checkpointing
- WAL
- crash recovery
```

---

# 38. Master Antigravity Prompt

```text
/goal

Implement the FTL Reliability Simulator according to PROJECT_CONSTITUTION.md.

Primary objective:
Substantiate the resume's C++17 FTL, flash-management, crash-recovery, and reliability claims.

Start with:
write/read -> page mapping -> physical-page model -> invariants.

Then implement:
1. invalidation
2. allocator
3. garbage collection
4. overprovisioning
5. wear leveling
6. configurable NAND timing
7. trace generator
8. checksums
9. checkpoints
10. deterministic crash injection
11. vulnerable metadata-ordering mode
12. recovery
13. write-ahead log fix
14. 30-seed fault regression
15. endurance experiments
16. visualization
17. interview guide

Do not add unrelated distributed infrastructure.

Do not hide invariant failures.

The same workload seeds and crash points must be usable against vulnerable and fixed modes.

At completion generate interview/FTL_INTERVIEW_GUIDE.md strictly from the implemented behavior and evidence.
```

---

# 39. Interview Guide

Generate:

```text
interview/FTL_INTERVIEW_GUIDE.md
```

Minimum categories:

## NAND Fundamentals

- page
- block
- program
- read
- erase
- erase-before-write
- program/erase cycles
- ECC
- bad blocks
- endurance

## FTL

- logical vs physical
- mapping
- page-level mapping
- block-level tradeoffs
- invalidation
- GC
- overprovisioning
- wear leveling
- write amplification
- TRIM

## Reliability

- acknowledged writes
- crash consistency
- checkpoint
- journal/WAL
- replay
- ordering
- atomicity
- stale reads
- duplicate ownership
- metadata corruption

## Systems

- checksums
- invariants
- deterministic testing
- fault injection
- reproduction
- sanitizers
- C++ memory ownership

Minimum:

```text
55 primary questions
110 follow-ups
25 crash/debugging scenarios
5 design questions
```

---

# 40. Mandatory Interview Questions

The final agent must answer:

1. Why can't NAND overwrite a page in place?
2. Why does FTL need logical-to-physical mapping?
3. What causes write amplification?
4. How does overprovisioning affect GC?
5. What happens when a block has valid and invalid pages?
6. How do you select a GC victim?
7. Why can aggressive GC hurt latency?
8. Static vs dynamic wear leveling?
9. Why is a reverse mapping useful?
10. What is stale-read corruption?
11. How could one physical page become owned by two LBAs?
12. Why are acknowledged writes a special durability boundary?
13. What exactly failed in the metadata-ordering defect?
14. Why did write-ahead logging fix it?
15. What if the system crashes while writing the WAL?
16. Why checksum checkpoints?
17. How do you choose checkpoint frequency?
18. How do you recover after a crash during GC?
19. What are the limitations of this simulator versus a real SSD?
20. How did deterministic seeds make debugging easier?
21. What invariants caught the corruption?
22. How do TRIM commands affect mapping and GC?
23. How do you model NAND latency?
24. How would multiple channels change the design?
25. How would you extend this to power-loss-protection semantics?

---

# 41. Evidence Artifacts

Required:

```text
evidence/
├── vulnerable/
│   ├── seed-*/
├── fixed/
│   ├── seed-*/
├── endurance/
├── gc/
├── wear/
└── summary/
```

Summary must contain:

```text
seeds run
crashes injected
vulnerable invariant failures
fixed invariant failures
write amplification
GC metrics
wear distribution
```

---

# 42. Definition of Done

FTL simulator is complete only when:

- C++17 core exists
- CMake works
- GoogleTest suite exists
- page-level L2P mapping works
- reverse map works
- allocation works
- invalidation works
- GC works
- wear leveling works
- overprovisioning is configurable
- NAND latency is configurable
- checksummed traces exist
- crash injection exists
- checkpoint persistence exists
- replay recovery exists
- required invariants exist
- vulnerable ordering defect is reproducible
- WAL fix exists
- 30 deterministic seeds run
- fixed mode has no observed mapping corruption in those regression runs
- sanitizer CI passes
- evidence artifacts exist
- interview guide exists
