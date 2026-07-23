# Where does a contextual argument literal get constructed? — a VM architecture study

A runtime-architecture paper, not a bug fix. It asks one question — *where in the
Zend VM does the construction of a target-typed argument literal* `foo(vec{…})`
*belong* — and evaluates the design space against what an RFC for PHP 9 would
require. No implementation code.

The **return** source (`return vec{…}` in a typed function) is already correct
and is out of scope except as a contrast: it reads `EX(func)`, the executing
function, which is a *stable, always-present* context that no optimizer pass can
remove. This paper is about the **argument** source, whose type lives on a
callee that is only known at runtime and only *while the call is pending*.

---

## 1. The fact that forces the whole design

The expected type of `foo(vec{…})` is `foo`'s parameter type. Three properties of
that fact are non-negotiable and drive everything:

1. **Late-bound.** `foo` is resolved at `INIT_FCALL` (dynamic dispatch,
   autoloading, conditional definition, opcache reuse). The type is *not* soundly
   knowable at compile time.
2. **Call-scoped.** The only place the type exists is on the *pending call*:
   `EX(call)->func->arg_info[N]`. `EX(call)` is created by `INIT_FCALL` and
   consumed by `DO_FCALL`. Outside that window it does not exist.
3. **Not an IR value.** `EX(call)` is a transient control object of the VM. It is
   **not** represented anywhere in the opline stream. Optimizer passes reason about
   opline data-flow; they cannot see a handler's reach into `EX(call)`.

The current implementation constructs the value *before* the call, in a free
`CONSTRUCT_COLLECTION` opcode whose handler reaches into `EX(call)`. Property (3)
is why that is fragile, and the crash is the proof: opcache's call optimizer, which
models a call as a `zend_call_info` (`caller_init_opline … caller_call_opline`),
legitimately elides a provably side-effect-free call and its `SEND`s, leaving the
free `CONSTRUCT_COLLECTION` behind to run with `EX(call) == NULL`.

> **The question is not "how do we stop the optimizer moving it" but "which IR
> construct is the type-source naturally attached to."**

---

## 2. The call lifecycle and the four candidate construction points

```
   caller frame (EX)                         callee frame (EX(call) → becomes EX)
   ────────────────                          ─────────────────────────────────
 ① before the call
   INIT_FCALL foo ──────────────► EX(call) allocated, func resolved
      │                                    (arg_info[N] now reachable)
 ② during argument evaluation
      │  INIT_ARRAY {…}          ← free expression zone
      │  CONSTRUCT_COLLECTION    ← current design lives here  ✗ (IR-invisible dep)
      │
 ③ during SEND
      │  SEND_* value, arg N ───► writes EX(call)->arg[N]     ← call-bound opcode
      │
   DO_FCALL ─────────────────────► EX(call) activated as EX
 ④ during RECV (callee side)
                                    RECV_INIT / RECV param N
                                    checks arg[N] against arg_info[N].type
                                    (value already in the slot)
```

Four places the collection *could* be built: ① before `INIT_FCALL` (type unknown —
excluded immediately), ② in the free-expression zone (current), ③ at `SEND`,
④ at `RECV`. The type is available from `INIT_FCALL` onward, so ②/③/④ are all
*type-feasible*. The architecture is decided by IR-visibility and by what value
crosses the `SEND`→`RECV` boundary.

---

## 3. Why ② (before the call) and ④ (at RECV) are both wrong

**② free-expression construction is IR-invisible.** The opcode's only IR inputs
are the element array (op1) and its result TMP; its real input — the pending
callee — is read behind the optimizer's back. Every pass that reasons about
data-flow (DFA, SCCP, DCE, call elision, call rewriting, and *any future pass*)
is entitled to move, fold, hoist or orphan it. Making it correct means teaching
*each* pass a special rule — the dependency stays invisible and the correctness
burden is unbounded in time. This is alternative **A**, evaluated below and
rejected.

**④ RECV construction requires an observable incomplete value.** For `RECV` to
build the collection it must receive the *elements* (an array, or a marked
"pending collection") in the argument slot and complete them against
`arg_info[N]`. But in the Zend VM an argument slot is **observable before `RECV`
runs**:

- `func_get_args()` / `debug_backtrace()` read the raw slots;
- a variadic parameter *packs* surplus slots into a plain array with no per-slot
  `RECV`;
- a by-reference parameter aliases the slot;
- named arguments bind slots to parameters late, at `DO_FCALL`.

So a half-formed value placed in a slot **leaks**: it can be seen, packed, or
aliased as an array before any `RECV` could complete it, and for an *untyped*
parameter no `RECV` completion happens at all — the array would silently pass
through, defeating fail-closed. This is exactly the pipeline paper's "no incomplete
value" rule, now grounded in the specific VM paths that expose the slot. Placing a
*new value kind* (a pending collection) into the argument-passing protocol would
force by-ref, variadic, named-arg, `func_get_args`, generator and fiber paths to
all understand it. That is a runtime-wide tax for one literal form.

**Consequence.** The value must be a *complete* `IS_COLLECTION` before it enters
the argument slot, and its construction must be *inside the call region so the
optimizer cannot detach it*. The only point that is simultaneously (a) call-region
bound and (b) before the slot is observable is **③ SEND**.

---

## 4. The pivot: SEND is where caller-side construction and callee-side type meet

`SEND_*` is the IR's representation of "argument N of *this* call." It is a member
of the `zend_call_info`; the optimizer treats `INIT_FCALL … SEND … DO_FCALL` as one
region and never separates a `SEND` from its `INIT_FCALL`. At `SEND` time
`EX(call)` is definitionally valid — existing `SEND_VAL_EX`/`SEND_FUNC_ARG` already
read `EX(call)->func` for by-ref decisions. So a construction performed *at* `SEND`:

- reads the callee's `arg_info[N]` from a pointer that is *guaranteed live* (the
  call region is intact by construction);
- produces a *complete* collection that is written straight into the slot, so no
  incomplete value ever exists;
- is may-throw, so the call region cannot be elided (fixing the crash class at its
  root, not by special-casing a pass);
- keeps fail-closed local: the send sees the parameter type; a non-collection or
  absent type throws there.

This is the hybrid the earlier carrier analysis pointed to: **type-source is the
callee (④'s correctness), construction is caller-side at SEND (②'s completeness,
without ②'s invisibility).**

```
 element expressions ──► INIT_ARRAY (TMP array, may be const-folded)
                              │  (ordinary IR data edge, optimizer-visible)
                              ▼
                        SEND_COLLECTION  op1=array, op2=arg N, ext=written kind
                              │  reads EX(call)->func->arg_info[N]  (live: in-region)
                              ▼
                        EX(call)->arg[N] = complete IS_COLLECTION value
                              │
                           DO_FCALL ─► RECV checks arg[N] (matches by construction, R1)
```

---

## 5. The alternatives, evaluated

Legend for the axes: **VM-fit / IR-visible / Optimizer / JIT / Opcode cost /
Compiler cost / Runtime correctness / Extensible / Zend-philosophy.**

### A — keep `CONSTRUCT_COLLECTION` before SEND, teach the optimizer

- **VM fit:** unchanged opcode, but its correctness now depends on optimizer
  restraint. **IR visibility:** none — the `EX(call)` edge stays invisible;
  correctness is a side-agreement with each pass. **Optimizer:** DFA/SCCP/DCE/call
  elision/call rewriting must *each* learn "do not move/fold/detach a contextual
  `CONSTRUCT_COLLECTION` out of its call region"; a future pass silently
  re-breaks it. **JIT:** the tracing/function JIT needs the same special rule.
  **Opcode cost:** zero new opcodes. **Compiler cost:** none. **Runtime:** correct
  only if every pass is taught. **Extensible:** the special rule must be
  re-asserted for map/shape and any future target-typed opcode. **Philosophy:**
  against it — Zend opcodes are meant to be reasoned about from the opline stream.
- **Verdict:** fixes the symptom, entrenches the disease. Fragile-by-design.
  Fallback only.

### B — a dedicated `SEND_COLLECTION` send opcode  *(recommended)*

- **VM fit:** excellent — a new member of the `SEND_*` family, the established way
  to add an argument-passing mode (`SEND_UNPACK`, `SEND_ARRAY`, `SEND_USER` are
  precedents). **IR visibility:** total — it *is* a send; the array is an ordinary
  data edge; the callee edge is legal because the opcode lives in the call region.
  **Optimizer:** nothing to teach — call elision/rewriting already keep sends with
  their call; may-throw keeps the region alive; DCE/SCCP see a normal
  array→send edge. **JIT:** one new handler (or bail to the VM handler initially —
  consistent with how rare opcodes are handled). **Opcode cost:** +1 opcode;
  operands reuse the send shape (op1 = element array `CONST|TMP`, op2 = arg-num,
  extended_value = written kind for the diagnostic). **Compiler cost:** localized —
  `zend_compile_args()` emits `INIT_ARRAY` + `SEND_COLLECTION` instead of
  `INIT_ARRAY` + `CONSTRUCT_COLLECTION` + `SEND_VAL`; nothing else moves.
  **Runtime:** fail-closed, exceptions (arg-slot/frame unwind is the existing send
  throw path), by-ref (value → by-ref param errors exactly as `SEND_VAL` does),
  variadics (arg_info clamp as today), dynamic calls (callee resolved at
  `INIT_*`), generators/fibers (suspension never occurs mid-argument-passing) —
  all handled by being an ordinary send. Named arguments bind late and are the one
  genuinely harder case (see §7). **Extensible:** map/shape reuse the same opcode
  (the descriptor carries the kind); a future general target-typed argument could
  generalize the same slot. **Philosophy:** strongly consistent.
- **Verdict:** the architecture that fits the VM and the optimizer with no special
  knowledge.

### C — extend the existing `SEND_*` family with a flag

- Same call-region benefits as B, but the construction branch lives *inside*
  `SEND_VAL`/`SEND_VAL_EX`. **Cost:** a per-send test on the hottest arg-passing
  opcodes for a case that is rare; muddies the specialization that keeps `SEND_VAL`
  fast; the JIT's `SEND_VAL` fast paths gain a branch. **Verdict:** the right
  *region*, the wrong *granularity* — pay a hot-path tax to save one opcode slot.
  B is cleaner. (C is worth keeping in mind only if opcode-count pressure ever
  becomes decisive.)

### D — split `CONSTRUCT_COLLECTION` into standalone vs call-context forms

- Not a mechanism but the **correct semantic partition**, and it is the framing
  behind the recommendation:
  - *standalone construction* — explicit `vec[int]{…}` and `return vec{…}` —
    keeps `CONSTRUCT_COLLECTION`, whose type source (op_array side table, or the
    stable `EX(func)` return type) is always present, so it is safe as a free
    expression;
  - *call-context construction* — `foo(vec{…})` — must be call-bound, i.e. a
    send (B).
- Read this way, "split into two forms" **is** "keep `CONSTRUCT_COLLECTION` for the
  standalone half and introduce `SEND_COLLECTION` for the call half." A literal
  second *free-standing* opcode that still read `EX(call)` would inherit A's
  invisibility, so the call-context half has to be a send regardless.
- **Verdict:** adopt D as the conceptual model; realize its call half as B.

### E1 — construct at RECV (callee side)

- Rejected in §3: needs an observable incomplete value crossing SEND→RECV, breaks
  fail-closed for untyped parameters, and taxes by-ref/variadic/named/`func_get_args`
  /generator/fiber paths with a new value kind. **Verdict:** where types *belong*
  conceptually, but defeated by the VM's slot observability.

### E2 — attach resolved parameter descriptors to the call frame as data

- Have `INIT_FCALL` deposit the callee's collection descriptors into the call
  frame and have construction read them as *data*. **Assessment:** the call frame
  is still not an opline operand, so "read as data" is still an in-region reach;
  it only works if the reader is itself in the call region — i.e. it collapses back
  to a send (B). It also front-loads work onto every `INIT_FCALL`. **Verdict:**
  subsumed by B without B's simplicity.

### E3 — do not support contextual literals in argument position

- The honest minimal-language option: ship contextual **return** only, and require
  explicit `vec[int]{…}` as an argument. **Assessment:** removes `SEND_COLLECTION`,
  the named-arg question, and all of §7. But arguments are the single most common
  place collections are produced in typed code, so this discards most of the
  feature's ergonomic value; and once map/shape arrive the asymmetry ("contextual
  in return, never in a call") is a language wart. **Verdict:** a legitimate
  fallback if the VM cost is judged too high, but strictly weaker than B, which
  carries a *low* and *localized* VM cost.

---

## 6. Trade-off summary

| | A optimizer-taught | **B SEND_COLLECTION** | C SEND flag | E1 RECV | E3 return-only |
|---|---|---|---|---|---|
| Dependency visible in IR | ✗ | **✓** | ✓ | partial | ✓ (n/a) |
| Optimizer needs special knowledge | **yes, forever** | **no** | no | yes (incomplete value) | no |
| New value kind in VM | no | **no** | no | **yes** | no |
| Fail-closed natural | needs care | **✓** | ✓ | ✗ (untyped param) | ✓ |
| Hot-path (`SEND_VAL`) untouched | ✓ | **✓** | ✗ | ✓ | ✓ |
| New opcodes | 0 | **1** | 0 | 0–1 | 0 |
| Future-pass safe | ✗ | **✓** | ✓ | ✗ | ✓ |
| Ergonomics delivered | full | **full** | full | full | return-only |
| RFC-acceptability | low | **high** | medium | low | medium |

---

## 7. Open questions that B still has to answer (not blockers)

1. **Named arguments.** `foo(v: vec{…})` binds slot→parameter at `DO_FCALL`, so the
   parameter index is not known at `SEND`. Options: (a) resolve the name→index at
   send for the statically-known-callee case and fail-closed otherwise; (b) keep
   the current deliberate deferral (a compile error directing to `vec[int]{…}`) for
   the first release. Recommendation: (b) now, (a) later — it does not affect the
   core architecture.
2. **JIT handler.** Ship with the tracing/function JIT bailing to the VM handler
   for `SEND_COLLECTION` (the opcode is rare), add a native handler later. This is
   the normal staging for a new opcode and costs nothing in correctness.
3. **Exception unwinding.** Construction throwing at `SEND` must free the element
   array and abandon the half-built call frame — this is exactly the existing
   "argument send throws" unwinding path; `SEND_COLLECTION` must route through it
   rather than inventing its own.
4. **`map`/`shape`.** Both reuse `SEND_COLLECTION` unchanged (the resolved
   descriptor selects the kind and the pair/field grammar lives inside the element
   builder), so the opcode is introduced once for the whole family.

---

## 8. Recommendation

**Redesign the argument path before any further implementation.** Adopt **D as the
model and B as the mechanism**:

- explicit and **return** literals keep `CONSTRUCT_COLLECTION` (standalone
  construction; type source always present) — no change;
- **argument** literals are built by a new **`SEND_COLLECTION`** send opcode that
  constructs the collection from the pending callee's parameter type *at send
  time*, inside the call region the optimizer already models.

This removes the `EX(call)` dependency from the free-expression zone entirely; the
opcode's every dependency is visible in the opline stream; no optimizer or JIT pass
needs special knowledge, now or in the future; no incomplete value ever enters the
argument protocol; and fail-closed, by-ref, variadic, dynamic-call, generator and
fiber behaviour all fall out of "it is an ordinary send." It is also the shape most
likely to survive RFC review, because it adds one opcode to an existing family
rather than a cross-cutting rule to the optimizer or a new value kind to the VM.

The current `CONSTRUCT_COLLECTION`-before-`SEND` argument implementation should be
treated as a **prototype to be replaced**, not extended. The parser/scanner work
(the `T_COLLECTION_HEAD` design, §8a–§8c of the implementation note) and the return
source stand and are unaffected.

### Diagram — target architecture

```
 EXPLICIT   vec[int]{…}     ─► INIT_ARRAY ─► CONSTRUCT_COLLECTION(idx)     (standalone)
 RETURN     return vec{…}   ─► INIT_ARRAY ─► CONSTRUCT_COLLECTION(RETURN)  (standalone; EX(func) stable)
 ARGUMENT   foo(vec{…})     ─► INIT_ARRAY ─► SEND_COLLECTION(arg N, kind)  (call-bound; new)
                                                └─ reads EX(call)->func->arg_info[N], in-region
```
