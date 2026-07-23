# The semantic pipeline for aliases and contextual typing

A compiler architecture paper. Syntax is discussed only in §9, and only as a
consequence.

---

## 1. What the existing pipeline already decides

Two facts about the implemented architecture constrain everything below, before
any language question is asked.

**There are already two type representations, not one:**

| | `zend_collection_type` | `zend_collection_info` |
|---|---|---|
| Built by | the compiler, from a type AST | promotion, at runtime |
| Lives in | arena, opcache SHM, file cache | request-local intern tier |
| Identity | per declaration site | **canonical** — one node per distinct type |
| Persisted | yes | never |

**And the compiler already resolves type *names* — but not class *entities*.**
`vec[Foo]` compiles to a descriptor holding an interned class-name string.
`Foo` is not resolved to a `zend_class_entry` until runtime, lazily.

That distinction is the template for aliases. A class name is a *runtime entity*
whose resolution must be deferred (autoloading, conditional declaration). An
alias is a *compile-time abbreviation*. They are not the same kind of thing and
should not be handled the same way.

---

## 2. Full trace: `foo(map{"john" => $john})`

Given `type Users = map[string, User]` and `function foo(Users $users)`.

### 2.1 Compiling the declaration — where the alias dies

```
type AST: name "Users"
   ↓  alias lookup (compile time, lexical scope)
   →  map[string, User]
   ↓  zend_compile_collection_typename
   →  zend_collection_type{ kind=map, types=[ mask(string), name("User") ] }
   ↓  stored in arg_info[0].type   (+ source name "Users" recorded for diagnostics, §5)
   ↓  opcache: persisted to SHM, arena bit cleared
```

**After this stage the alias does not exist.** No later phase sees `Users`.

### 2.2 Compiling the literal — independently

```
AST: COLLECTION_LITERAL(kind=map, pairs=[("john", $john)])
   ↓  the literal carries NO type node
   ↓  compiler validates only: is this position allowed to supply an expected type? (§3.3)
   ↓  emits: element construction + a construction opcode carrying
             an *expected-type source*, not an expected type
```

The literal is compiled with no knowledge of `foo`, `Users`, or `map[string,User]`.

### 2.3 Runtime

```
INIT_FCALL foo            → resolves zend_function*, caches it (existing mechanism)
   ↓  arguments evaluated
CONSTRUCT_COLLECTION      → reads the expected type from its declared source:
                            EX(call)->func->arg_info[0].type   (a descriptor)
   ↓  zend_collection_info_resolve(descriptor)   (existing, cached per request)
   →  canonical node for map[string, User]
   ↓  literal checking: each element against the node's members
   →  immutable value, holding a borrowed canonical node
```

### 2.4 The resulting pipeline

```
        declaration                              literal
        ───────────                              ───────
        type AST                                 literal AST
            ↓ alias expansion  ◄── ALIASES END HERE
        descriptor
            ↓ persist (SHM)
            └──────────────► expected-type source ──► descriptor
                                                          ↓ promotion (cached)
                                                     canonical node
                                                          ↓
                                                     literal checking
                                                          ↓
                                                     immutable value
```

---

## 3. Where does the expected type actually come from?

This is the question the pipeline diagram hides, and it is the hardest part of
the design.

### 3.1 The expected type is *not* uniformly available at compile time

| Position | Expected type known when? |
|---|---|
| `return vec{…}` in a function with a declared return type | **compile time** — the enclosing signature is being compiled |
| typed property default, typed class constant | **compile time** |
| `foo(vec{…})` — argument | **runtime only** |
| `$obj->prop = vec{…}` | **runtime only** |
| `$x = vec{…}` — untyped local | **never** |

The argument case — the motivating example — is the hard one. **PHP cannot know
the callee at compile time**: functions may be conditionally declared, declared
later, autoloaded, or called dynamically. A compile-time lookup would also be
unsound under opcache, where a script compiled with one `foo` visible is reused
when a different `foo` is in scope.

So contextual typing cannot be a purely static analysis. Any design that assumes
it is will fail on its own primary example.

### 3.2 The resolution: the callee *is* known at runtime, before arguments

PHP evaluates arguments **after** `INIT_FCALL`, which already resolves the
function and caches it in a run-time cache slot. Therefore at the moment the
literal is constructed, `EX(call)->func` is available, and its `arg_info` carries
the parameter descriptor.

This means **no new value kind is required.** There is no need for an
"incomplete collection value" that floats through the VM waiting to be completed
— an idea that would otherwise force every argument-passing, coercion, union-type
and by-reference path to understand a half-formed value, and would leak whenever
the receiving parameter turned out to be untyped.

Instead the construction opcode carries, from compile time, a small
**expected-type source** descriptor:

- *from the enclosing return type* (resolved statically),
- *from parameter N of the pending call* (read at runtime from the resolved
  callee),
- *from the property being assigned* (read at runtime from `zend_property_info`).

The type itself is never baked in; only *where to find it*. That composes with
the existing machinery exactly: descriptors are already reachable from
`arg_info` / `property_info`, and `zend_collection_info_resolve()` already
caches descriptor → canonical node per request.

### 3.3 The compiler's remaining job: reject positions with no source

The compiler cannot know the *type*, but it can always determine *whether a
source exists*, syntactically. `$x = vec{…}`, `echo vec{…}`, an untyped array
element — these have no expected-type source and are **compile errors**.

One hole remains and must be specified: `foo(vec{…})` where `foo`'s parameter has
no collection type. The position looks context-supplying at compile time; at
runtime no type arrives. This must be a `TypeError`, and it is the one case that
compiles but fails at runtime. It should be stated in the RFC rather than
discovered.

---

## 4. At what phase do aliases disappear? — **Model A**

**Aliases are expanded when the declaration that mentions them is compiled, and
never exist afterwards.** The literal never sees an alias; it never sees a source
type name at all.

Three independent reasons, each sufficient:

1. **The literal's expected type arrives from `arg_info` / `property_info` /
   return type — structures that store descriptors, not source text.** By the
   time any literal can consult them, expansion has already happened. Model B
   ("the literal sees aliases") would require those structures to retain source
   names *and* the literal to perform alias lookup at runtime — making aliases
   runtime entities with their own resolution, ordering and autoloading
   semantics.

2. **Canonicalisation demands it.** The runtime interns descriptors into
   canonical nodes keyed by structure, and equality is pointer identity
   (INV-17). `Users` and `map[string, User]` denote one type and *must* intern
   to one node. If an alias survived into the descriptor, either the key would
   have to ignore it — in which case it carries no information — or it would
   split one type into two nodes and break every pointer comparison built on
   canonicalisation.

3. **Descriptors are persisted to shared memory.** An unexpanded alias in SHM
   would need to be resolvable in every future request, in any scope, which means
   a global alias registry with autoloading. That is a large, separate feature,
   and it would put a name-resolution dependency inside opcache's read-only data.

**So collection literals operate on canonical descriptors, never on source type
names.** The layer that owns alias expansion is **the type-declaration compiler**
— the same layer that already turns `vec[Foo]` into a descriptor holding an
interned name.

The precedent is exact: PHP already has transparent, compile-time-only,
file-scoped name aliases in `use Foo\Bar as Baz;`. They never become runtime
entities. A `type` alias resolved the same way — lexically, per compilation
unit, with `use type …` for cross-file visibility — fits the existing pipeline
with no new machinery.

---

## 5. Diagnostics without nominality

**Yes — and the mechanism is already implied by §1's two-representation split.**

Record the source name at the **declaration**, not in the type:

- `arg_info` / `property_info` / return type gain an optional "as written"
  string (`"Users"`), captured at compile time next to the descriptor.
- The **canonical node keeps no alias information at all** — it remains purely
  structural, so interning and pointer equality are untouched.
- Diagnostics render from the declaration's recorded name when reporting *about
  that declaration* ("Argument #1 ($users) must be of type Users, vec[int]
  given"), and from the node when reporting about a *value*.
- Reflection exposes both: the written form and the resolved structural form —
  exactly as it could already distinguish `?int` from `int|null`.

Cost: one pointer per *declaration*, not per type. Aliases remain fully
transparent to the type system while remaining fully visible to humans.

---

## 6. The three alias models against the pipeline

**Model 1 — Transparent.** Falls out of §4 with no additional mechanism. The
alias is a compile-time abbreviation; nothing downstream is aware of it.
Diagnostics show the expanded form.

**Model 2 — Transparent with preserved source metadata.** Model 1 plus §5. The
type system is unchanged; only declarations carry an extra name. **This is the
natural fit** — it costs one field, preserves canonicalisation exactly, and
recovers the entire diagnostic benefit people actually want from aliases.

**Model 3 — Nominal.** Architecturally expensive *in this pipeline
specifically*. A nominal alias must survive into the descriptor and into
canonicalisation, because identity now depends on it. Consequences:

- the intern key must include alias identity, so `Users` and `map[string,User]`
  become **distinct nodes** — structural interning is no longer structural;
- every pointer-identity check must decide whether nominal difference matters,
  and the "canonical equality is pointer identity" invariant needs restating;
- the alias becomes a runtime entity (it participates in type identity), so it
  needs registration, ordering and autoloading semantics;
- opcache must persist alias identity and keep it stable across requests.

None of this is impossible, but it converts aliases from a compile-time
convenience into a type-system feature with runtime presence — a different and
much larger proposal.

---

## 7. The hypothesis: confirmed

> *"The correct alias semantics may naturally fall out of the compiler pipeline
> rather than the other way around."*

**Confirmed, and more strongly than stated.** The pipeline does not merely
suggest an alias model — it *excludes* one. Structural canonicalisation with
pointer-identity equality, plus persistence of descriptors into shared memory,
makes transparent aliases nearly free and nominal aliases a rework of the runtime
type layer.

The reasoning runs one way only: the pipeline was designed for structural
identity long before aliases were discussed, and that decision has already
answered the alias question. Choosing nominal aliases now would mean revisiting
canonicalisation, not merely adding a feature.

---

## 8. Checking-only, or checking *and* synthesis?

### 8.1 What bidirectional systems normally do

Supporting both modes for one term is **entirely standard**. The usual shape:

- *introduction forms* (lambdas, literals, constructors) are checked;
- *elimination forms* (application, projection) synthesise;
- a **subsumption rule** lets a synthesising term be used where checking is
  expected (`e ⇒ T`, `T <: S` ⊢ `e ⇐ S`);
- the reverse direction requires an **annotation** — which is precisely why
  ascription exists in such systems.

Terms that support both are common. Swift array literals are the closest
analogue: `let s: Set<Int> = [1,2,3]` checks, while `let x = [1,2,3]`
synthesises via a *default literal type*. That is exactly Model B, and it works
in Swift.

**So my earlier paper overstated the case in claiming checking-only is inherent
to bidirectional typing. It is not.**

### 8.2 Why Model B nevertheless fails *in PHP*

Synthesis from contents requires the compiler to know the *types of the
elements*. PHP does not have them:

```php
vec{1, 2, 3}      // literal constants — types statically known
vec{$a, $b}       // types unknown at compile time
vec{f()}          // unknown
```

That leaves two possibilities, both unacceptable:

- **Synthesise only when all elements are compile-time constants.** Then
  `vec{1,2,3}` compiles and `vec{$x}` does not. The rule depends on the
  *syntactic form of the elements*, not on types — the kind of distinction users
  cannot predict and tools cannot explain.
- **Synthesise at runtime from actual element values.** Then `vec{1, 2.0}`
  produces `vec[int|float]` on one execution and something else on another. The
  value's type becomes **data-dependent**, which destroys canonicalisation
  determinism, makes `vec[int]` parameters fail unpredictably, and means a
  program's type errors depend on its inputs.

Swift can do this because it has static types for all expressions. PHP does not,
and no amount of design work in this feature creates them.

**Conclusion: Model A is forced, not preferred.** The reason is not type theory
— it is PHP's absence of static expression types. This is worth stating in the
RFC precisely because the theory *would* permit Model B in a different language,
and reviewers familiar with Swift or C# will ask.

A corollary: `vec{}` (empty) is not a special case needing extra rules. Under
Model A *no* literal synthesises, so the empty literal is uniform with the rest.

---

## 9. Is explicit syntax still necessary?

Only now can this be answered, and the pipeline answers it.

**The gap is exactly the set of positions with no expected-type source** (§3.3):
untyped local assignment, untyped array elements, `echo`, and arguments to
untyped parameters.

Two observations:

1. **The set is smaller than it looks.** Return, property, class-constant and
   *typed*-parameter positions are all covered by §3.2 with existing machinery.
   These are where collection values are actually created in typed code.
2. **Filling the gap with feature-specific syntax is the wrong layer.** The
   positions that lack an expected type lack one for a *general* reason: PHP has
   no typed local variables. That is not a collections problem, and a
   collections-specific annotation would become a permanent second way of saying
   what typed locals would say better.

**Recommendation: ship no explicit annotation.** A literal in a
no-source position is a compile error whose message names the fix — write the
element type on the literal (`vec[int]{…}`), which is always available and always
unambiguous.

That deliberately leaves `vec[int]{1,2,3}` as the fully-explicit form and
`vec{1,2,3}` as the contextual form, with no third spelling. If the ergonomics
prove insufficient, the principled next step is **typed local variables**, which
would close the gap for this feature and every future checking-mode term at once.

The cast-shaped form is not re-evaluated here: §4 already removed its
justification. Its purpose was to supply a type the literal could not otherwise
obtain — but the pipeline shows the type is obtainable from the declaration in
every position where a collection value is meaningfully created, and where it is
not, the explicit literal form already exists.

---

## 10. Summary of decisions

| Question | Answer |
|---|---|
| Where do aliases disappear? | At declaration compilation — **Model A** |
| What do literals operate on? | **Canonical descriptors / nodes**, never source names |
| Who owns alias expansion? | The type-declaration compiler |
| Can diagnostics keep `Users`? | Yes — per-declaration metadata, node stays structural |
| Which alias model? | **Model 2** (transparent + source metadata) |
| Is Model 3 viable? | Possible, but it reworks canonicalisation — a separate proposal |
| Does the pipeline determine alias semantics? | **Yes — hypothesis confirmed** |
| Checking-only or both? | **Checking-only — forced by PHP's lack of static expression types**, not by type theory |
| Where does the expected type come from? | A compile-time-encoded *source*, read at runtime from the resolved callee / property / enclosing signature |
| Is a new runtime value kind needed? | **No** |
| Is explicit annotation syntax needed? | **No** — `vec[int]{…}` already is it |
