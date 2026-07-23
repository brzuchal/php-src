# Where must a collection descriptor originate?

A compiler architecture paper. Four categories are kept separate throughout:
**(1) compiler limitations, (2) VM/runtime limitations, (3) type-system
semantics, (4) ergonomics.** Only (1) and (2) can make a model impossible; (3)
can make it incompatible with decisions already taken; (4) is excluded from the
verdict.

---

## 0. Correction to the previous paper

The previous paper rejected runtime synthesis with this claim:

> *"the value's type becomes data-dependent, which destroys canonicalisation
> determinism"*

**That claim was wrong, and the challenge to it is correct.**

Canonicalisation is a hash table keyed by structure. It is deterministic *given a
descriptor*, and it is provenance-blind by explicit design — Finding A of the
original ownership audit established that the runtime **cannot** determine where
a descriptor came from, because opcache clears `_ZEND_TYPE_ARENA_BIT` and sets
nothing in its place. Two descriptors with identical structure intern to the same
node whether they were built by the compiler, by an extension, or by a VM
handler. `vec{1}` producing `vec[int]` and `vec{"x"}` producing `vec[string]` is
exactly as well-behaved as two parameters declaring those types: two structures,
two nodes, both canonical.

Nor is "data-dependent type" an invariant violation. It is ordinary dynamic
typing, which PHP does everywhere.

I conflated *canonicalisation determinism* (a property of the interning
mechanism) with *type determinism* (a property of the language). Only the second
was ever at issue. The rest of this paper analyses the question again without
that error.

---

## 1. Is a descriptor a compile-time or runtime object?

**Both, and this is already true in the implementation — not a proposal.**

| | Built by | Lives in |
|---|---|---|
| Declaration descriptors | compiler | arena → SHM |
| Extension descriptors | `ext/zend_test`, at runtime, on the C stack | caller memory |

`zend_collection_info_intern()` already accepts a stack-allocated descriptor
built moments earlier by a runtime caller, and **INV-13** states its contract
explicitly: *"promotion never retains its input"*. The test factory
(`zend_test_make_vec`) does exactly what Model B proposes — assembles a
descriptor at runtime, interns it, constructs a value — and has done so through
every ASAN run in this series.

So the architectural answer is unambiguous: **a descriptor is a transient
structural description that may originate anywhere.** The runtime already
depends on this being true.

---

## 2. Do compiler or VM limitations prohibit Model B?

### 2.1 Compiler limitations — **none**

The compiler emits a construction opcode. Under Model B it needs to know only the
kind and the element count, both syntactically available. It needs no type
information, no callee resolution, no expected-type source. Model B asks *less*
of the compiler than Model A.

### 2.2 VM/runtime limitations — **none that prohibit**

The required steps are each already implemented:

1. inspect element types — `Z_TYPE_P`;
2. build a descriptor — a plain struct; arity is known at compile time, so it can
   be stack-allocated;
3. intern it — `zend_collection_info_intern()`, which is provenance-blind and
   does not retain its input;
4. construct — `zend_vec_create()` already takes a node.

**One real cost, which is a cost and not a prohibition.** Model A resolves a
declared type once per site per request and caches the node in a run-time cache
slot. Model B must inspect N elements and hash a fresh descriptor **per
constructed value** — the PROBE path, which walks the descriptor. Construction
becomes O(N) inspection + O(N) hashing + a lookup, instead of a cached pointer
read.

This is mitigable by the standard technique: cache the last element-type
signature and its resulting node per site, and reuse on match — a monomorphic
inline cache. So the cost is a constant factor on a warm path, not an
architectural barrier.

**Verdict on (1) and (2): nothing in PHP's compiler or runtime makes Model B
impossible, or even significantly more complex to implement.** My previous
rejection did not rest on a runtime limitation, and I should not have implied it
did.

---

## 3. What actually constrains Model B: the join

The constraint is in category (3), and it is specific.

A descriptor's shape is fixed per kind. To synthesise one from N runtime values,
the element types must be folded into the descriptor's member slots:

| Kind | Members | Fold required |
|---|---|---|
| `tuple[A,B,C]` | **N**, positional | **none** — element *i* becomes member *i* |
| `vec[T]` | **1** | **join of N element types → 1 type** |
| `set[T]` | 1 | join |
| `map[K,V]` | 2 | two joins |

For `vec{1, "x"}` the join of `int` and `string` is either:

- **a union** — but union members are **outside the supported canonicalisation
  boundary** (D-7). They are rejected before hashing precisely because they have
  no canonical member order, and hashing one positionally would key it by source
  order and let structurally equal types intern as two nodes. Model B for `vec`
  therefore **pulls the deferred union-ordering work onto the critical path** —
  it cannot be built on today's canonicalisation;
- **`mixed`** — accepted today, but it discards the information the feature
  exists to carry, and `vec[mixed]` is a distinct type from `vec[int]` under
  invariance;
- **an error** — "heterogeneous literal requires an explicit type", which is a
  runtime error whose occurrence depends on input data.

**This is the first genuine architectural coupling**, and it is asymmetric across
kinds — which is itself a notable result.

### 3.1 The tuple asymmetry

**Model B is architecturally natural for `tuple` and awkward for `vec`/`map`/`set`.**

`tuple{1, "a"}` synthesises `tuple[int, string]` exactly: one member per element,
positional, no join, no union, no precision loss, no interaction with D-7. The
descriptor shape and the literal shape are isomorphic.

`vec{1, "a"}` has no such correspondence, because a vec's descriptor has one
member and the literal has many.

This asymmetry does not come from the language design. It falls directly out of
the descriptor shapes chosen for each kind, and it would be visible to anyone
implementing the opcode.

---

## 4. What Model B does to contextual typing

This is the decisive point in category (3), and it is not about implementation
cost at all.

Collection compatibility was chosen — and is implemented — as **invariant and
fail-closed**. Under invariance, a value of type `vec[int]` satisfies a parameter
of type `vec[float]`, `vec[int|null]` or `vec[mixed]` **in none** of those cases.

So under **pure** Model B:

```php
function foo(vec[float] $x) {}
foo(vec{1, 2, 3});     // synthesises vec[int] → invariant mismatch → TypeError
```

The literal must synthesise *exactly* the expected type, which it can do only by
coincidence. **Contextual typing becomes vacuous**: the context no longer
influences the literal; it only checks it. What the design set out to provide is
not provided.

To recover it, one of two things already rejected would have to return:

- **variance or subtyping between collection types** — explicitly rejected in
  favour of invariant compatibility, and reversing it invalidates the
  pointer-identity equality the runtime now depends on (INV-17); or
- **element-level coercion at the boundary** — which means a collection value's
  elements are re-checked and possibly converted on every parameter pass,
  contradicting the immutable-shared-value model.

**This is the real reason to reject pure Model B**, and it is a type-system
incompatibility with a decision already taken and implemented — not a runtime
limitation and not a preference.

### 4.1 The empty literal forces a hybrid regardless

`vec{}` has no elements and therefore no synthesisable type. Model B cannot type
it at all. Any design admitting `vec{}` must retain a context-driven path, so
**pure Model B is not self-sufficient** for reasons independent of §4.

---

## 5. Opcode design

| | Model A | Model B | Hybrid |
|---|---|---|---|
| Compile-time operands | kind, arity, **expected-type source** | kind, arity | kind, arity, *optional* source |
| Run-time cache | node for the declared type (once per request) | last element-signature → node (per site) | either path |
| Per-construction work | cached pointer read | inspect N + hash N + lookup | depends on path |
| Needs D-7 union work | no | **yes**, for vec/map/set | only on the synthesis path |

All three fit the VM equally well; none requires a new value kind, and none
requires an "incomplete value" — the user's observation on that point is correct
and applies to all three.

**The hybrid is the only one that is both complete and consistent with
invariance:** use the expected type when a source exists (so context genuinely
shapes the literal, per §4), and synthesise only where no source exists — the
positions the previous paper proposed to reject at compile time.

Its cost is precisely §3: the synthesis path needs a join, therefore union
canonicalisation, therefore D-7. Which yields a clean staging rule:

> A hybrid can ship with the synthesis path **restricted to `tuple`** (no join
> required) and to homogeneous literals for other kinds, with union-based
> synthesis deferred until D-7 is resolved.

---

## 6. Verdict

**Model B is technically viable.** No compiler limitation and no VM limitation
prohibits it. The runtime already builds, interns and constructs from
runtime-assembled descriptors on every test run, and canonicalisation is
provenance-blind by design.

What rules out **pure** Model B is neither of those:

1. **Type-system incompatibility (§4).** With invariant collection
   compatibility, a self-typed literal must match the expected type exactly, so
   contextual typing stops functioning. Restoring it requires variance or
   element coercion, both previously rejected, and variance additionally
   invalidates pointer-identity equality.
2. **Incompleteness (§4.1).** `vec{}` is untypeable under synthesis.
3. **Coupling to deferred work (§3).** Heterogeneous `vec`/`map`/`set` literals
   require a join, hence unions, hence canonical member ordering — the D-7 work
   deliberately deferred.

None of these is "less desirable". (1) contradicts an implemented invariant, (2)
is a gap in coverage, (3) is a dependency on unbuilt infrastructure.

### Remaining trade-offs if a hybrid is adopted

- **Runtime errors become input-dependent.** `$x = vec{$a, $b}` succeeds or fails
  per execution depending on whether the element types join cleanly. A construct
  that looks total is partial.
- **Two ways to type one literal.** The same source text acquires its type by
  different mechanisms in different positions, so the rule "what type does this
  literal have?" is no longer answerable locally.
- **Per-construction cost** on the synthesis path (§2.2), mitigable by a per-site
  inline cache.
- **Kind asymmetry becomes user-visible** (§3.1) if synthesis is enabled for
  `tuple` before the others.

### What the previous paper got right and wrong

Right: pure Model A is coherent, needs no new value kind, and the expected type
*is* obtainable at runtime from the resolved callee.

Wrong: the canonicalisation argument (§0), and the implication that runtime
synthesis is architecturally blocked. It is not. The correct objection is
narrower and stronger — invariance makes self-typed literals and contextual
typing mutually exclusive, so the choice between Model A and Model B is a choice
about **whether context shapes literals or merely checks them**, and that
question was already settled when invariant compatibility was chosen.
