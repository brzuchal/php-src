# Hybrid persistent vec — spike design

Feasibility spike for a hybrid persistent `vec` representation. Disposable
code; nothing here touches the production `zend_vec` layout, parser, compiler,
opcodes, JIT, serialization, or PHP syntax. Base: `5b98890223c`
(first-class-collections). All spike code lives behind `ext/zend_test`
(`vec_spike.h`, `vec_spike.c`, `vec_spike_test.c`, `vec_spike_bench.c`) plus
this directory's driver scripts.

## Representations

All three store real zvals, allocate from Zend MM (`emalloc`) inside a normal
request, and own exactly one element reference per stored slot
(`ZVAL_COPY` on store, `zval_ptr_dtor` on container destruction).

### A. flat-copy (baseline, ~production semantics for shared vecs)

```
spk_flat { uint32_t rc; uint32_t count; zval slots[]; }   // header 8 B
```

Literal: exact-size. Append: always allocate `count+1`, memcpy the payload,
addref every refcounted element, release the old container. This is the
worst-case persistent append and doubles as the flat baseline for read/foreach
comparisons.

### B. flat-capacity/consume

```
spk_cap { uint32_t rc; uint32_t count; uint32_t cap; zval slots[]; }  // header 16 B
```

Literal: exact-size (`cap == count`). Append:

- `rc == 1 && count < cap`: write in place (consume).
- `rc == 1 && count == cap`: grow to `pow2ceil(count+1)` by alloc + memcpy
  **move** (no addref, old container freed without slot dtors) — deliberately
  models the production constraint that `erealloc` cannot move GC-visible
  buffers, so growth is alloc+copy+free.
- `rc > 1`: full addref-copy into a `pow2ceil(count+1)` container.

This is the "destructive update when exclusive" model from the
implementation-notes feasibility work.

### C. hybrid (the representation under evaluation)

Measured layouts (release build, arm64; `sizeof(zval) == 16`):

```
spk_hyb (version, 32 B — exactly the 32-byte ZMM bin):
    uint32_t   rc;        // version handle refcount
    uint32_t   count;     // total logical count
    spk_flat  *base;      // immutable exact-size literal payload, or NULL
    spk_spine *spine;     // NULL until first append
    uint32_t   vb;        // visible block count
    uint8_t    lv;        // visible length of last visible block
    uint8_t    shift;     // chunk == 1 << shift
    uint8_t    _pad[2];

spk_spine (24 B — exactly the 24-byte bin; ptr array is a separate alloc):
    uint32_t   rc;        // referencing versions
    uint32_t   nphys;     // physical blocks appended so far
    uint32_t   cap;       // ptrs capacity (grows ×2)
    spk_block **ptrs;

spk_block (header 8 B + chunk * 16 B):
    uint32_t rc;          // number of versions whose prefix includes it
    uint8_t  phys_len;    // initialized slots (append-only)
    uint8_t  _pad[3];
    zval     slots[];
```

`uint8_t` is valid for `phys_len`/`lv` because chunk ≤ 128 < 256. Note the
field-size reality check: with `uint32_t lv/shift` the version struct would be
40 B (next bin: 40); with `uint8_t` it is exactly 32 B. The block header is
8 B either way (zval alignment).

## Hybrid semantics

1. A literal is one exact-size flat payload (`base`), `spine == NULL`.
2. First append leaves `base` untouched forever and starts a spine with one
   block.
3. The "last block" is just the last not-full block — no separate type.
4. Many versions share one spine; each version stores only
   `(vb, lv, count)` — its visible prefix.
5. **Tip append** (`vb == nphys && lv == ptrs[vb-1]->phys_len`): write the
   zval at `slots[phys_len]` (an index invisible to every existing version),
   then `phys_len++`, then create the successor version. If the receiver
   handle is exclusive (`rc == 1`) the version struct and its container
   references are reused in place (consume); otherwise a new 32 B version is
   materialized which addrefs base, spine and each of its `vb` blocks.
6. **Branch append** (receiver not at the physical tip): new spine that
   shares the flat base and all fully-visible full blocks
   (`shared = lv == chunk ? vb : vb-1` pointer copies + block addrefs),
   plus one fresh block holding an addref-copy of the receiver-visible
   prefix of the old last block (`lv` zvals; zero when the receiver ends on
   a block boundary) and the appended value.
7. No slot at an index visible to any version is ever written. Full blocks
   are immutable. Blocks before the physical last block are always full.

## Ownership model (the load-bearing decision)

**Each version owns one reference on every block in its visible prefix; the
spine owns none** (its `ptrs` are borrowed). Version destruction delrefs its
`vb` blocks, then the spine, then the base. A block dies exactly when the
last version that can see it dies.

This is forced by the retention bound: if the spine owned the blocks, any old
snapshot holding the spine would retain every block appended after it —
unbounded invisible memory. With per-version ownership, an old snapshot
retains at most `chunk - lv` invisible slots of its own last block (≤ one
block) plus its share of the spine pointer array (8 B per physical block, see
limitations).

Consequences:

- Spine entries at indexes ≥ every live version's `vb` may dangle after the
  versions that saw those blocks died. They are never dereferenced: the tip
  check dereferences only `ptrs[vb-1]` of the appending version (alive by
  ownership), and the branch path only `ptrs[0..vb-1]`. Once a tip version
  dies, that spine tail is unreachable garbage-by-design (already freed) and
  the spine can only be branched from, never extended.
- Materializing a version with `rc > 1` at the tip costs O(vb) block addrefs;
  branching costs O(vb) pointer copies + addrefs. This is the linear-spine
  cost that a tree would remove; the benchmarks measure exactly this slope
  (workloads 2 and 7) to answer "when is a tree needed".
- The exclusive-receiver consume path (`rc == 1`) skips all of it: O(1)
  appends regardless of vb.

## Invariants (tested)

For every version V: `V.spine == NULL ⇔ V.vb == 0`, and with a spine
`V.count == base_count + (V.vb-1)*chunk + V.lv`, `1 ≤ V.lv ≤
ptrs[V.vb-1]->phys_len ≤ chunk`, blocks `0..vb-2` full. Every initialized
zval slot has exactly one owning container (flat payload or block); versions
own container references, never slots. Destruction releases every zval
exactly once; uninitialized slots (`≥ phys_len`) are never read or destroyed
— a production `get_gc` would walk `phys_len`-bounded ranges the same way.

Verification (all green, chunk sizes 2/4/16/128, release and debug builds):

- Differential oracle: every logical version maintained simultaneously as
  hybrid and as flat-copy; 12 randomized runs × 1500 ops (append to random
  live version / dup / drop / verify) compare deep checksums, point reads and
  structural invariants after every step. 1632 branch appends exercised.
- Deterministic scenarios: retention bound (old snapshot retains exactly one
  partial block, invisible slots ≤ chunk), branch copy counts (partial: lv+1
  zval copies; boundary: 1), probe-string refcount audit (rc returns to
  pool-only after teardown), B consume/copy semantics, empty literal, two
  independent spines off one shared base.
- Debug-allocator gate: the full selftest runs with zero Zend MM leak
  reports at shutdown.
- Every benchmark case (release) asserts counter balance at teardown:
  allocs == frees, zval copies == dtors, element addrefs == delrefs, block/
  spine/version allocs == frees. The driver additionally cross-checks deep
  content checksums between representations and chunk sizes within each
  logical-content group.

## Allocator size classes (measured via layout report)

| allocation           | sizeof  | ZMM class | waste |
|----------------------|---------|-----------|-------|
| version              | 32      | 32        | 0%    |
| spine struct         | 24      | 24        | 0%    |
| block chunk=16       | 264     | 320       | 17.5% |
| block chunk=32       | 520     | 640       | 18.8% |
| block chunk=64       | 1032    | 1280      | 19.4% |
| block chunk=128      | 2056    | 2560      | 19.7% |
| block 15 slots       | 248     | 256       | 3.1%  |
| block 31 slots       | 504     | 512       | 1.6%  |
| block 63 slots       | 1016    | 1024      | 0.8%  |
| block 127 slots      | 2040    | 2048      | 0.4%  |
| flat 100k elems      | 1.6 MB  | page-rounded | 0.1% |

Power-of-two chunks pay ~18–20% allocator waste per block because
header + `chunk*16` lands just above a bin. `chunk-1` slot counts fit bins
almost exactly. The spike benchmarks power-of-two chunks (shift/mask
indexing); a production design should either use 2^k−1 slots with
compile-time constant division (a multiply+shift, ~free) or put the 8 B
header elsewhere. This is a memory-density decision, not a feasibility
blocker.

## Benchmark methodology

- Fresh process per case (`php -n -d zend.enable_gc=0 -d memory_limit=2G`),
  603 cases, execution order shuffled with a fixed seed (20260731).
- GC disabled to keep root-buffer scans out of the timings (the containers
  are not GC-tracked; elements are acyclic).
- Timer: `clock_gettime_nsec_np(CLOCK_UPTIME_RAW)`; Apple Silicon counts at
  24 MHz (~41.7 ns granularity), so every workload batches operations to a
  ~300–500 µs target per sample before dividing. Warmup samples discarded;
  median/p95 over ≥5–15 samples reported.
- Element pools built untimed (ints; runtime-created non-interned strings;
  stdClass objects; small arrays; nested array-in-array payloads); appends
  copy out of the pool exactly like a VM append would (addref, no
  allocation of the element itself in the timed region).
- Counters captured around the last timed region give allocations, bytes,
  zval copies/moves, element addrefs, block addrefs and branch counts per
  operation. Memory via `zend_memory_usage`/`peak` with peak reset per case.
- DCE sinks: read/foreach loops accumulate into a checksum that is emitted.

## Out of scope (per task)

prepend, `withAt`/set, tuple integration, JIT, parser/syntax, public API,
GC cycle integration (elements here are acyclic; a production design adds
`get_gc` walking initialized ranges), trees/skip-lists over the spine
(analyzed from measured slopes instead).
