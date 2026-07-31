# Hybrid persistent vec — spike results & recommendation

**TL;DR: all eight acceptance criteria pass.** One append onto a retained
100k-element vec goes from ~115 µs / 1.5 MB (flat-copy) to ~11 ns / ~1 KB
(hybrid) — four orders of magnitude — while indexed reads in the base region
stay within ~10–20% of flat and foreach within ~0–14%. Branch cost and
invisible retention are provably bounded by one block. Recommendation:
**continue with a production architecture study** (details at the end).

Environment: Apple M3 Pro (12 cores), 36 GB, macOS 14 (Darwin 23.5), release
build (`-O2`, ZEND_DEBUG=0), base commit `5b98890223c` + spike patch. 603
benchmark cases, fresh process each, randomized order, GC disabled, medians
over ≥5–15 samples with warmup; every case passed counter-balance validation
and cross-representation checksum comparison (0 failures). Raw data:
`results/raw.csv`, `results/env.json`, full pivots in `results/analysis.md`.

Representations: **A** flat-copy (copy on every append), **B**
flat-capacity/consume (in-place when exclusive, full copy when shared), **C**
hybrid (immutable flat base + shared append-only block spine + per-version
visible prefix). `cN` = hybrid with chunk N.

## Answers to the eight questions

### 1. One append onto a 100k flat base (original retained)

| elem | repr | median/append | zval copies | allocs | bytes |
|---|---|---|---|---|---|
| int | A | 115.7 µs | 100,001 | 1 | 1.5 MB |
| int | B | 115.1 µs | 100,001 | 1 | 2.0 MB |
| int | C c16 | **11.4 ns** | 1 | 4 | 352 B |
| int | C c64 | **10.6 ns** | 1 | 4 | 1.1 KB |
| int | C c128 | **10.4 ns** | 1 | 4 | 2.1 KB |
| string | A | 90.3 µs | 100,001 | 1 | 1.5 MB |
| string | C c64 | 11.1 ns | 1 | 4 | 1.1 KB |

A/B do O(n) work (memcpy + an addref pass that dirties every refcounted
element header); C does O(1): one zval copy plus four small allocations
(spine, pointer array, block, version). The speedup is ~10,900× at 100k and
already 45× at a 1k base; C stays ~11 ns at every base size (100 → 100k).
A/B big-copy medians vary ±25% between processes (page-fault noise on MB-size
allocations); it does not affect any conclusion.

Amortization (base=100k, int, ns/append): A stays ~146 µs regardless of how
many values are appended; B amortizes its single big copy (29.2 µs at 4
appends → 115 ns at 1024); C is 2.5–4.4 ns from the second append on.

### 2. Bytes copied on a branch (chunk 16/32/64/128)

Measured zval copies on branch-append are exactly `lv + 1` (receiver-visible
prefix of the last partial block + the appended value), as designed:

| chunk | worst case (lv = chunk−1) | median | mid-block (lv = chunk/2) | block boundary |
|---|---|---|---|---|
| 16 | 256 B | 23.6 ns | 144 B / 21.5 ns | 16 B / 15.6 ns |
| 32 | 512 B | 31.9 ns | 272 B / 23.8 ns | 16 B / 16.0 ns |
| 64 | 1.0 KB | 41.6 ns | 528 B / 29.5 ns | 16 B / 15.5 ns |
| 128 | 2.0 KB | 84.1 ns | 1.0 KB / 61.3 ns | 16 B / 15.8 ns |

For comparison, the flat models pay a full copy to "branch" a retained value:
452 ns–1.4 µs already at ~1.1–1.6k elements (and 115 µs at 100k). Branch cost
in C is independent of total size and bounded by one block.

### 3. Read overhead in the flat base region

Random indexed reads, base 10000 + 8 blocks, int, ns/read (throughput-mode,
pre-generated indices):

| repr | base region | full-block region | last block | mixed |
|---|---|---|---|---|
| A / B (flat) | 0.49–0.51 | — | — | 0.47–0.51 |
| C c16–c128 | 0.56–0.61 | 0.65 | 0.61–0.65 | 0.62–0.72 |

Base region: **+10–20%** (one `i < base_count` compare+branch). Block
regions: +25–40% (extra pointer chase through the spine). Absolute costs stay
sub-nanosecond; the base region — where nearly all data lives in the
motivating workload — remains close to flat.

### 4. foreach overhead (base + 1 / 8 / 64 / 1024 blocks)

ns/element, base 10000, int, vs a flat vec of the same total length:

| blocks | C c64 | flat same total | overhead |
|---|---|---|---|
| 0 | 0.495 | 0.494 | ~0% |
| 1 | 0.496 | 0.494 | ~0% |
| 8 | 0.500 | 0.494 | +1% |
| 64 | 0.559 | 0.530 | +5% |
| 1024 | 0.605 | 0.529 | +14% |

(Chunk 16–128 all fall in the same band; flat baselines themselves jitter
0.494–0.530 with cache footprint.) Small block counts are free; even 1024
blocks (64k appended elements) cost +14%.

### 5. How much invisible memory can an old snapshot retain?

Measured (string elements, tip at 40 blocks, then tip dropped, snapshot only
survivor): invisible slots = `phys_len(last visible block) − lv` — worst case
measured **chunk − 1 slots** (e.g. 127 slots / ~2 KB payload at c128), i.e.
bounded by one block, exactly per design. All blocks beyond the snapshot's
visible prefix were freed (blocks live == vb == 11 in every scenario).
Caveat: the shared spine *pointer array* is retained at its physical size —
512 B here (64-pointer capacity), 8 B per physical block generally. That is
0.4–3% of block payload and the one place retention is not strictly
one-block-bounded; a production design can epoch-trim or cap it.

### 6. When does a plain array of block pointers become too expensive?

The O(vb) costs are: materializing a version with a shared receiver
(vb block addrefs), branching (vb pointer copies + addrefs), destroying a
version (vb delrefs). Measured slope ≈ 0.5–2 ns per block:

| visible blocks | create (c16 → c128) | destroy |
|---|---|---|
| 1 | 13.5–14.9 ns | 12.5–14.3 ns |
| 8 | ~18 ns | ~16 ns |
| 64 | 42–48 ns | 50–53 ns |
| 1024 | 0.57–2.3 µs | 0.7–1.2 µs |

Up to ~64 blocks the spine is effectively free (≤ ~50 ns, comparable to one
block copy). It becomes the dominant per-append cost in the all-versions-
retained workload beyond a few hundred blocks: retained linear append at
n=65536/c16 (up to 4096 blocks) reaches 1.2 µs/append — still ~10× faster
than flat-copy would be, but clearly linear. Practical threshold: **plain
array is fine below ~256 blocks; beyond ~1k blocks (≈64k appended elements
at c64) the linear spine dominates.**

### 7. Does hybrid still win if all intermediates die immediately?

No — flat-capacity/consume is the better transient engine, by 10–40%:

| elem, n=4096 dead | A | B | C c64 |
|---|---|---|---|
| int | 2.9 µs | **2.1 ns** | 2.4 ns |
| string | 4.0 µs | **2.4 ns** | 3.1 ns |
| nested | 6.9 µs | **2.9 ns** | 3.2 ns |

Both are O(1)/append (B: in-place slot write; C: block write + occasional
block alloc); B additionally keeps memory contiguous. The hybrid's value is
entirely in sharing: the moment any old version is retained, B degrades to a
full copy (W2: 801 ns–4.2 µs/append, 135–186 MB retained at n=4096) while C
stays 6–49 ns/append with 306 KB retained — a **440× memory** and 60–350×
time advantage. The two designs compose rather than compete (see
recommendation).

### 8. Is a tree needed, and at what block count?

Not for the target workloads. All costs except the O(vb) spine effects are
O(1) or O(chunk)-bounded. From the measured slopes, a tree (or any extra
indirection level) pays for itself only when *shared* versions are
materialized/branched/destroyed at **vb ≳ 256–1024** (tens of thousands of
appended elements past the literal). Below that, the plain pointer array is
faster and vastly simpler. A production design can keep the plain spine and
either cap it (flatten into a new base when the appended tail exceeds ~25% of
the base, amortized O(1)) or add one indirection level lazily beyond ~1k
blocks. A general tree is not justified by the data.

## Other measured properties

- **Retained linear append** (every version kept, ns/append, int):
  A/B grow linearly (631 ns → 3.4 µs at 1k→4k, then capped for time);
  C: c16 15→49→163 ns and c128 6.3→12→40 ns at n=1k→4k→16k. Retained memory
  at n=4096: A 135.7 MB, B 186.3 MB, C ~306 KB (all chunk sizes) —
  ~75 B/version amortized.
- **Destruction**: single-version teardown is identical across
  representations (1.0–2.2 ns/element, element-dtor-bound). Retained chains:
  A/B ~1.1–1.3 µs per version (each version owns a full payload), C 41 ns
  (c128) / 289 ns (c16) per version.
- **Forks**: creating a fork off one snapshot costs ~45 ns regardless of fork
  count (1→256, +~1.1 KB live memory each, base shared); flat-copy pays
  110 µs and 160 KB per fork. Fork-depth chains (branch-of-branch, 1024 deep)
  cost ~95–107 ns/level with no depth blowup; branch histories verified
  independent.
- **Element kinds**: hybrid append cost is element-kind-independent (1 zval
  copy); flat models scale their copy/addref pass with n and pay 30–110% more
  for refcounted payloads (object n=4096 dead: 6.2 µs vs int 2.9 µs).
- **Struct/allocator reality** (measured, arm64): version = 32 B and spine
  struct = 24 B (both bin-exact; `uint8_t` lv/shift is what keeps the version
  out of the 40 B bin). Power-of-two blocks waste 17.5–19.7% in Zend MM bins
  (e.g. c64: 1032 B → 1280 B bin); `2^k−1` slot counts waste 0.4–3.1%
  (63 slots: 1016 B → 1024 B bin). End-to-end, dead-build memory for C c16 at
  n=65536 is ~1.31 MB vs 1.0 MB flat payload (+31%, = bin waste + 8 B
  headers + spine). A production chunk should be 63 or 127 slots with
  compile-time constant division.

## Acceptance checklist

| criterion | verdict |
|---|---|
| Large-base persistent append improves by multiple times | ✅ 45× at 1k base, ~10,900× at 100k |
| Fork cost bounded by one partial block | ✅ measured copies = lv+1 ≤ chunk (16 B–2 KB, 16–84 ns) |
| Invisible retention bounded by one block | ✅ ≤ chunk−1 slots measured; caveat: shared spine ptr array (8 B/phys block) |
| Base-region indexed reads close to flat | ✅ +10–20% (0.51 → 0.56–0.61 ns) |
| foreach with few blocks has modest overhead | ✅ ≤ +5% up to 64 blocks, +14% at 1024 |
| Ownership/GC statable as strict invariants | ✅ stated in design.md; oracle tests + zero debug-allocator leaks + per-case counter balance |
| No production zend_vec change required | ✅ everything behind ext/zend_test |

## Threats to validity

C-level microbenchmarks (no VM dispatch/opcode overhead — relative gaps at
the ns scale will compress once VM cost is added); single machine (M3 Pro,
41.7 ns timer forcing batched sampling); GC disabled (containers are not
GC-tracked in the spike; production needs `get_gc` over initialized ranges
and cycle handling for object elements); ±25% run variance on MB-scale
copy cases; element-kind matrix trimmed for read/foreach (read path is
element-independent).

## Recommendation

**Continue with a production architecture study.** The layout is technically
feasible and the performance case is decisive for the persistent scenarios
this design exists for; nothing found is a blocker. The spike also shows
plain flat-capacity/consume (B) is the better *pure-transient* engine, so the
study should treat the two as one design with two states rather than
alternatives:

1. Exclusive values stay flat-with-capacity (B semantics; already validated
   as "feasible now" in the destructive-update notes). On the first append to
   a *shared* value, freeze the flat payload as the immutable base and switch
   to the hybrid spine — the 100k-base append then costs ~11 ns instead of
   ~115 µs, and old snapshots retain at most one partial block.
2. Use `2^k−1`-slot blocks (63 or 127) for bin-exact allocation; chunk ~64
   balances branch cost (≤ ~42 ns) against spine length.
3. Resolve in the study: spine pointer-array retention (epoch-trim or cap),
   flatten-on-threshold vs one lazy indirection level beyond ~1k blocks, GC
   integration (`get_gc` over phys_len-bounded ranges is straightforward;
   buffer-root interaction with the consume path is the open question), and
   VM-level cost of the two-state dispatch.

A builder/transient-only implementation (the third option) would capture only
the Q7 numbers — that value is already available from plain capacity/consume
without any hybrid machinery, so it is not worth building the block layout
for. Reject is not supported by the data.

## Reproduction

```sh
# from the worktree root (5b98890223c + spike patch)
sh spike-hybrid-vec/run.sh
```

Individual pieces: `./buildconf --force && ./configure --disable-all
--enable-zend-test [--enable-debug] && make -j$(sysctl -n hw.ncpu)`;
correctness: `./sapi/cli/php -n -r 'var_dump(zend_test_vec_spike_selftest());'`
(stderr must stay empty in the debug build); benchmarks:
`./sapi/cli/php -n spike-hybrid-vec/driver.php` (≈50 s); pivots:
`./sapi/cli/php -n spike-hybrid-vec/analyze.php`.
