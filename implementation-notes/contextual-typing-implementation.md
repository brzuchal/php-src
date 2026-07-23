# Phase 2 — contextual collection literals: implementation note

> ## SCOPE DECISION (final) — DEFERRED, NOT SHIPPED
>
> **Contextual collection literals are deferred because argument-context typing
> requires VM architecture work outside the scope of the initial immutable
> collections RFC. Shipping return-only contextual literals would create an
> intentionally incomplete and asymmetric language feature.**
>
> The initial RFC ships **explicit literals only** (`vec[int]{…}`, `tuple[…]{…}`,
> `set[…]{…}`) — already committed, together with the A5 strict-comparison fix.
> The entire contextual implementation prototyped below (the `vec{…}` surface via
> `T_COLLECTION_HEAD`, the return + argument compiler paths, and the
> `CONSTRUCT_COLLECTION` expected-type source encoding) was **reverted from the
> shipping tree** and preserved as design record here plus a working-tree stash /
> `scratchpad/contextual-prototype` patch. It is *not* code to be merged; it is the
> substrate for a follow-up proposal.
>
> The follow-up proposal must bundle contextual typing (return **and** argument)
> with the VM evolution described in **`contextual-arg-vm-architecture.md`** — a
> call-bound `SEND_COLLECTION`-family construction — because the `EX(call)`-based
> argument carrier used in the prototype is architecturally invalid (§8d–§8e).
> What stands and is reusable for that proposal: the `T_COLLECTION_HEAD`
> parser/scanner design and its zero-conflict grammar (§8a–§8c), the grammar
> invariant that keeps the head out of `constant` (§8c), the contextual-typing
> semantic model (§2–§7), the finding that `EX(call)` is not a valid carrier, the
> `SEND`-based recommendation, and the deferred named-argument and JIT questions.

Design note, **written before implementation** per the workflow rule. This is
the roadmap's **Phase 2** (`implementation-notes/literals-roadmap.md §5`); the
semantic model is settled by `contextual-typing-design-paper.md`,
`contextual-typing-pipeline-paper.md` and `descriptor-origin-paper.md`. This note
reconciles those against the *current code* and fixes the exact implementation.
**The implementation described below was prototyped and then reverted; read it as
the design of record for the deferred follow-up, not as shipping code.**

## 1. Current behaviour

Only the fully-explicit literal exists: `vec[int]{1,2,3}`. Grammar
(`zend_language_parser.y:1434`) requires `T_VEC_LBRACKET collection_type_args ']'
'{' … '}'`; `T_VEC_LBRACKET` is the compound lexer token `vec[`
(`zend_language_scanner.l:1629`). `vec{…}` is a **syntax error** today (verified:
`const vec=5; vec{0}` and `$x=vec{1}` both fail to parse). `zend_compile_collection_literal`
(`zend_compile.c:11502`) builds a descriptor from `child[0]`, registers it in
`op_array->collection_types[]`, and emits `ZEND_CONSTRUCT_COLLECTION` with
`extended_value = index`; the VM (`zend_vm_def.h:6548`) reads
`op_array.collection_types[extended_value]` and `zend_collection_info_resolve()`s it.

## 2. Recovered semantics (from the papers, frozen)

- **Checking-mode / target-typed.** `vec{…}` has no type of its own; a type is
  pushed in from context. **Strictly local** — the source must be syntactically
  apparent at the use site; **no cross-statement inference**. (design-paper §6–8.)
- **The head names the KIND; context supplies the ELEMENT type(s).** `vec{…}` is
  always a vec; only its element type comes from context. So a written kind that
  disagrees with the expected type's kind is an error, not a silent reinterpret.
- **R1 (precedence).** If a usable expected type exists, checking wins; the value's
  node *is* the expected type's node, so the later param/return check is a
  pointer-identity match by construction (invariance never exercised).
- **R2 (coercion).** Element checks follow the construction site's `strict_types`,
  exactly as typed-property assignment does — **inherited, not added**: a
  contextual `vec{…}` targeting `vec[T]` runs the *same* `zend_collection_construct`
  as explicit `vec[T]{…}`, so its coercion/strictness is whatever explicit
  literals already do. This milestone adds no new coercion rule.
- **Model A only.** No synthesis from element values (Phase 5, deferred): PHP has
  no static element types, empty `vec{}` is untypeable by synthesis, and invariant
  compatibility makes self-typed literals + contextual typing mutually exclusive
  (`descriptor-origin-paper §4`). `vec{}` empty is uniform under Model A.
- **Fail-closed.** A checking-mode term with no usable source is an error, never a
  guess.

## 3. The current type system makes "usable expected type" trivial

The agent trace establishes a decisive simplification. A collection type inside a
`zend_type` is the `.ptr` with `_ZEND_TYPE_LIST_BIT` set and the union/intersection
bits clear. **Collections are forbidden in unions and intersections already**
(`zend_compile.c:7732,7822` — hard compile errors). Therefore for any declared
`arg_info[N].type` / return type:

> `ZEND_TYPE_HAS_COLLECTION_DESCRIPTOR(t) == true` already means *exactly one*
> (optionally nullable) collection type. `vec[int]|string` is unrepresentable.

So R1's "strip nullability, require exactly one collection type of the kind"
reduces to: `ZEND_TYPE_HAS_COLLECTION_DESCRIPTOR(t)` and
`ZEND_TYPE_COLLECTION(t)->kind == <literal kind>`. Nullable `?vec[int]` keeps the
descriptor and the predicate true (`MAY_BE_NULL` is an outer may-be bit), so it is
usable with no stripping needed. And `zend_collection_info_resolve(zend_type)`
takes the whole `zend_type` **by value** and self-guards, so a parameter/return
type is passed to it **directly**, no extraction — the runtime already does this
in `zend_check_collection_type` (`zend_execute.c:1115`).

## 4. Where the expected type comes from — the two sources in scope

`descriptor-origin-paper` and roadmap §5.1/§10 define an *expected-type source*
carried by the opcode, read at runtime. Both sources in this milestone read an
existing `zend_type` at runtime and hand it to `resolve()` — **no descriptor is
copied, the side table is not grown, and opcache is untouched**:

| Source | Read at runtime from | Validated |
|---|---|---|
| **RETURN** | `EX(func)->op_array.arg_info[-1].type` | at **compile time** (return type is known) |
| **PENDING-CALL ARG N** | `EX(call)->func->arg_info[N-1].type` | at **runtime** (callee unknown/opcache-unsound at compile time) |

Why arguments must be runtime (pipeline §3): the callee is not soundly known at
compile time (conditional declaration, autoload, dynamic dispatch, and opcache
reuse of a body compiled with a different `foo` visible). But arguments are
evaluated *after* `INIT_FCALL`, so at the construction opcode `EX(call)->func` is
resolved and `arg_info[N-1].type` is readable — **no incomplete value is needed**.
Nested/sub-call elements restore `EX(call)` before the construction opcode runs,
so `EX(call)` is always the immediate pending call at that point.

## 5. Context matrix (explicit include/defer/reject)

| # | Context | Decision | Why |
|---|---|---|---|
| 3 | `return vec{…}` in a typed fn | **REQUIRED** | RETURN source, compile-time validated |
| 7 | arrow-fn / closure return with return type | **INCIDENTAL** | same RETURN mechanism |
| 2 | call argument to a typed collection param (direct/method/static/dynamic/ctor, **positional**) | **REQUIRED** | PENDING-ARG source |
| 6 | constructor arguments `new C(vec{…})` | **INCIDENTAL** | PENDING-ARG via the ctor call |
| — | nullable param/return `?vec[int]` | **INCLUDED** | descriptor intact (§3) |
| 1 | untyped local `$x = vec{…}` | **REJECTED** | no source → compile error (no typed locals in PHP) |
| 4/5 | property / static-property **assignment** `$o->p = vec{…}` | **DEFERRED** | RHS evaluated before `ASSIGN_OBJ`; no pending-assignment frame (roadmap §5.2) |
| 12 | constant-expression positions (property/param defaults, class const) | **DEFERRED** | collection literals not allowed in const-expr yet (D-10) |
| 8 | ternary / match / coalesce branches | **DEFERRED** | requires forwarding a source through wrappers; only the *direct* operand is a source here |
| 9/10/11 | nested literals, tuple-slot, set-member propagation | **DEFERRED** | needs per-element expected-type propagation from the outer-literal compiler (a third interception point) |
| — | named-argument contextual literal `foo(x: vec{…})` | **DEFERRED** | position→index binds late; compile error naming the fix |
| — | union / intersection / `mixed` / `object` expected type | **REJECTED (fail-closed)** | not a single collection type → runtime `TypeError` (arg) / compile error (return) |

The deferrals are stated, not silent. The mechanism extends to the deferred
context-flow cases (ternary, nested) by adding more interception points later; it
does **not** need redesign.

## 6. Compiler strategy — targeted interception (Strategy A, minimal)

Rejected alternatives: **B** (push/pop ambient `CG()` expected-type stack) — the
user's caution against global mutable compile state applies, and it risks leaking
context into sub-expressions (`foo($x + vec{…})` must *not* see foo's arg type);
**C** (separate AST/type-resolution pass) — disproportionate, PHP has no such pass
for expressions; **D** as the *whole* design — only the arg case needs runtime
resolution, return is compile-time; **threading a parameter through
`zend_compile_expr`** — large blast radius for two positions.

**Chosen:** intercept at exactly the two positions that supply a source, and let
every other position fall through to a uniform rejection:

1. `zend_compile_collection_literal` learns the **contextual** shape
   (`ast->child[0] == NULL`, unambiguous — explicit `collection_type_args` is
   always ≥1 node). Reached via the generic `zend_compile_expr` path (i.e. *not*
   intercepted below) ⇒ **compile error**: *"Cannot infer the element type of
   `vec{…}` here; write `vec[int]{…}` to state it."*
2. `zend_compile_return`: if the (paren-stripped) operand is a contextual literal,
   compile it with **SOURCE_RETURN** after a compile-time check that the enclosing
   function has a return type whose descriptor kind equals the literal's kind
   (else a precise compile error; tuple arity checked here too).
3. `zend_compile_args`: if argument `i` is *directly* a contextual literal, compile
   it with **SOURCE_ARG(i+1)**. No compile-time callee assumptions (fail-closed at
   runtime). Named-arg / spread wrappers are not the direct form ⇒ deferred path.

No ambient state; context never leaks into sub-expressions because only a *direct*
contextual-literal operand is intercepted. `$x + vec{…}` compiles `vec{…}` through
the generic path ⇒ the intended compile error.

## 7. Opcode encoding (backward-compatible, no opcache change)

`ZEND_CONSTRUCT_COLLECTION` keeps op1 = element array and `result`. Today op2 is
`UNUSED` and `extended_value` = descriptor index. Extend:

- **`op2.num`** = source discriminator, `0 = EXPLICIT` (today's default, so existing
  emission is unchanged), `1 = RETURN`, `2 = ARG`, with the literal's **kind** folded
  in (`kind << 2`) for the contextual runtime kind-check.
- **`extended_value`** = EXPLICIT → descriptor index (unchanged); ARG → argument
  number N; RETURN → unused.

VM handler switch on `op2.num & 3`:
- EXPLICIT: `descriptor = op_array.collection_types[extended_value]` (unchanged path).
- RETURN: `descriptor = EX(func)->op_array.arg_info[-1].type`.
- ARG: `f = EX(call)->func; idx = min(N-1, num_args-1 if variadic else N-1);`
  `descriptor = (in range && collection) ? arg_info[idx].type : <none>`.

Then the **existing** tail runs unchanged: `info = zend_collection_info_resolve(descriptor)`
→ construct. Two contextual guards before construct: (a) `info == NULL` (no
collection at that position) ⇒ fail-closed `TypeError` naming the fix; (b)
`info->kind != <written kind from op2>` ⇒ `TypeError` "cannot use `set{}` where
`vec[int]` is expected".

**No per-opline cache slot.** `resolve()` already caches per request keyed by the
descriptor pointer (stable for both sources), so correctness needs no new cache;
the roadmap's optional node cache (§10) is a deferred micro-opt and is noted as
such. Consequently op2 stays `UNUSED`-typed (only `op2.num` is used, like SEND's
`opline_num`), `extended_value` stays a `NUM`, and **no opcache persist/copy code
changes** (run_time_cache and `collection_types[]` layouts are untouched).

## 8. Grammar / lexer / AST

- Add compound tokens `vec{` `set{` `tuple{` (and `map{` `shape{` for the five-kind
  symmetry, gated to the same "not implemented" compile error as their bracket
  forms) — `T_VEC_LBRACE` etc., emitted only when the head name is *immediately*
  followed by `{` in `ST_IN_SCRIPTING`, with the same `::`-guard and brace-nesting
  bookkeeping as the `vec[` tokens.

> **CORRECTION (found during implementation) — this is NOT BC-safe.** The claim
> "`name{` is a syntax error in PHP 8+" is false for the case that matters: `{`
> is the universal *body* delimiter that follows a *name*, so a compound
> `head{` token collides with class/interface/trait/enum bodies and their
> `extends`/`implements`/return-type/`use` clauses:
> `class shape{`, `class X extends Set{`, `class Y implements Map{`,
> `function f(): Set{`, `use Tuple{`. The scanner runs case-inverted, so
> `Set{`/`Map{`/`Vec{` match too — and `Set`/`Map` are common class names.
> `Zend/tests/get_class_methods/bug32296.phpt` (`class quad extends shape{`)
> fails. Unlike `vec[` (which the corpus scan showed never occurs, because
> `name[` in a class header is invalid), `name{` in a class header is ordinary,
> valid code. The compound-lexer-token technique that works for `vec[` therefore
> does **not** transfer to `vec{`.
>
> Root of the problem: the lexer is (almost) context-free, but the distinction is
> a *parser-context* one — `head{` is a collection literal only in expression
> position, and a body brace everywhere a name/type is expected.

### 8a. Grammar analysis of the three parser-contextual alternatives

**Exact scanner rule that caused the collision** (reverted):
`<ST_IN_SCRIPTING>"vec{" { yy_push_state(ST_IN_SCRIPTING); enter_nesting('{'); RETURN_TOKEN(T_VEC_LBRACE); }`
(and its four siblings). An *unconditional compound rule that consumes the `{`*,
forcing the head-vs-body decision in the scanner, where the context is unknown.
The fix is to leave `{` a separate token and let the parser, which knows the
context, decide.

**Alternative 1 — `T_STRING '{' collection_literal_elements '}'` in `expr`.**
Measured: **1 shift/reduce conflict on `{`**, against property-hook blocks
(`optional_property_hook_list: '{' property_hook_list '}'`) in property /
promoted-parameter default-value position. Bison's default resolution (shift)
makes `public int $x = FOO { set($v){…} }` parse `FOO{…}` as a collection literal
— and that is *valid baseline code* (a backed hooked property with a constant
default), so it is a real break. The conflict is on `T_STRING` (every
identifier), so a precedence fix would ripple across the grammar. **Impractical.**

**Alternative 2 — a dedicated head token, `{` left separate (RECOMMENDED).**
The scanner emits `T_COLLECTION_HEAD` for `vec|set|tuple|map|shape` **only when
immediately followed by `{`** (re2c *trailing context* `("vec"|…)/"{"`; the `{`
is not consumed), case-insensitively as today. The token is admitted into `name`
(so `extends Set{`, `implements Map{`, return types, and every ordinary
name/type context accept it — verified: bug32296's `extends shape{` parses),
plus the class/interface/trait/enum/namespace *declaration-name* positions that
use `T_STRING` directly, and `collection_literal: T_COLLECTION_HEAD '{' … '}'`.
Measured: **the same 1 shift/reduce conflict, but now on `T_COLLECTION_HEAD`**,
and reachable only by `= vec{ … }` — a constant *named* `vec`/…/`shape` used as a
property default with an *adjacent* hook brace (never-written; a space,
`= vec { … }`, keeps `vec` a `T_STRING`). It resolves to shift (the literal),
which respects what was typed. Because the conflict is confined to the dedicated
token, it is silenceable with a one-token precedence or a documented `%expect 1`
**without touching `T_STRING`**. **This is the way to keep `vec{…}`.**

**Alternative 3 — parser-driven scanner state.** Not needed, and it is the
ad-hoc "track class headers / extends / implements / return types / trait use"
machine to be avoided.

**token_get_all() implications (alt 2).** `vec` immediately followed by `{`
tokenises as `T_COLLECTION_HEAD`; `vec` anywhere else (`vec[`, `vec(`, `vec ` +
space, `$o->vec`, `Foo::vec`, `vec;`) stays `T_STRING`. So only the head-directly-
before-`{` position gains a new token — a far smaller surface than making the
five names keywords.

**Whitespace / comments between head and `{`.** The trailing context requires the
`{` *immediately* adjacent, exactly like the existing `vec[` rule: `vec{1}` is a
literal; `vec {1}` and `vec/*c*/{1}` are `T_STRING` then `{`. Adjacency is the
same contract already shipped for the bracket head.

**BC (alt 2), by context.** All preserved once `T_COLLECTION_HEAD` is admitted
into `name` + declaration-name positions: class/interface/trait/enum *names*
(`class Set {`, and `class Set{` once the decl-name positions take the token),
`extends`/`implements` (via `name`), return types (via `type`→`name`), trait
`use`, namespaces, and mixed casing (`Set{`/`VEC{` are the same token and are
accepted as names). The one residual is the never-written `= vec{ hooks }`.

### 8b. The conflict is *eliminated*, not suppressed — no `%expect 1`

The 1 s/r conflict of §8a exists **only because the head was admitted into
`name`**, and `constant: name` then makes the head a constant expression; in a
property/parameter default (`= expr` followed by the optional hook `{`) the
parser cannot tell `= HEAD·{…}` (collection literal) from `= HEAD` (constant
default) + `{…}` (hook block).

**Exact Bison counterexample** (name-based variant):

```
shift/reduce conflict on token '{'
  Example: … "variable" '=' "collection head" . '{' '}'
  Shift  (literal):  parameter → … '=' expr[ collection_literal
                       → "collection head" . '{' collection_literal_elements '}' ]
                       … optional_property_hook_list[ %empty ]
  Reduce (hook):     parameter → … '=' expr[ scalar → constant → name
                       → "collection head" . ]
                       … optional_property_hook_list[ '{' property_hook_list '}' ]
```

**Elimination.** The head is a *type/class-name* token, never a constant. Admit
`T_COLLECTION_HEAD` into `class_name`, `type_without_static`, and a small
`class_decl_name: T_STRING | T_COLLECTION_HEAD` (used by the
class/interface/trait/enum declaration names) — the paths that reach `extends`,
`implements`, return/property types, `new`, `instanceof`, `::`, and declarations —
but **not** into `name` (hence not into `constant`/`expr`). Now in expression
position the head has no reduce-to-constant alternative: `HEAD` must be followed
by `{` as a `collection_literal`. The conflict disappears — **0 shift/reduce,
0 reduce/reduce, `%expect 0` unchanged.**

**Why no future collection feature reintroduces it.** The head token is admitted
only in class/type-name and literal-head positions and is emitted by the scanner
*only when immediately followed by `{`*. It is never a constant, so it can never
be the tail of a value-expression that an optional `{`-block (the only such PHP
context is property hooks) follows. Future work does not add a path:
- **map / shape** reuse the *same* `T_COLLECTION_HEAD '{' … '}'` production (the
  pair grammar lives *inside* the braces); the head/`{` boundary is unchanged.
- **append literals / methods** operate on a *value* via `->`/`[`/`(`, not via a
  bare head token, so they introduce no new `HEAD ·{` decision.
- Any new head name is one more alternative of the scanner rule and of
  `class_decl_name`; it changes no state adjacent to `{`.
The invariant is structural: *the head is not a constant, and property hooks are
the sole "expr then optional `{`-block" site*, so the only production that could
collide is excluded by construction.

**Status: implemented and verified.** re2c trailing-context head rule (`{` left
separate); `T_COLLECTION_HEAD` in `class_name` / `type_without_static` /
`class_decl_name`; case-insensitive head→kind via
`zend_ast_create_collection_literal()` feeding the existing
`attr = kind, child[0] = NULL` node (so §4–§7 are reused unchanged).

### 8c. The `name`-sharing invariant this fix depends on

The elimination in §8b is sound **iff** an identifier used as a *type/class name*
and an identifier used as a *constant name* are reachable through **disjoint
productions**. They share one nonterminal, `name`, which by grammar inspection
has exactly four RHS uses — an identifier is one lexical thing, and its role is
fixed by the enclosing production:

| Production | Role of `name` here | Head token admitted? |
|---|---|---|
| `class_name: T_STATIC \| name` | class reference — `extends`, `implements`, `new`, `instanceof`, `::`, `catch`, attributes, trait `use` | **yes**, as a sibling alternative to `name` |
| `type_without_static: … \| name \| …` | type — parameter / return / property, union & intersection members | **yes**, sibling alternative |
| `function_call: name argument_list` | function name | no — needs `(`; the head is emitted only before `{` |
| `constant: name` | constant expression (a value) | **no** — deliberately excluded |

> **INVARIANT (G1).** No type/class production derives `constant`, and `constant`
> derives no type/class production; so type positions and constant positions are
> disjoint even though both spell their identifier through `name`. The head token
> is emitted **only immediately before `{`**, so it meets the grammar only where
> `name` precedes `{`: the type/class-body positions (routed to the head) and the
> property-hook default-value position (a `constant`, from which the head is
> excluded — hence `= vec{…}` is a literal, never "constant default + hook").

Three independent, and deliberately different-in-kind, checks support G1:

1. **Grammar inspection establishes the structural separation.** The two
   directions were read directly from the grammar: no `type*` production contains
   `constant`, and `constant` contains no `type`/`type_expr`. This is the
   structural claim; it is an inspection of the current productions, not a proof
   about all reachable states.
2. **`%expect 0` mechanically guards parser ambiguity.** Bison rebuilds the LALR
   automaton on every change and fails the build if the conflict count moves off
   zero. So any edit that made a single production simultaneously a type and a
   constant adjacent to `{` — the only way to reintroduce the head-vs-hook
   ambiguity — cannot land silently; it turns into a build failure.
3. **`Zend/tests` provides regression evidence, not a completeness proof.** The
   full suite passes with zero failures, exercising class/interface/trait/enum,
   `extends`/`implements`, types, `new`, `::`, `instanceof`, namespaces, trait
   `use`, and property hooks. This is strong evidence that no *existing* construct
   is broken; it is not a formal proof that no construct could be.

**Future-work caveat.** G1 can be violated only by a feature that unifies a type
and a constant in one production next to `{` — concretely, a *constant position
that accepts a type* (`constant → type → head`), which would make the head a
constant again. Type aliases, generics and enum backing types put *types* (or
declaration names) in those slots, not constants, so they preserve G1. Any change
that did violate G1 is caught by check 2 (a new conflict), not silently
miscompiled. This paragraph is the standing warning for edits to `constant` /
`type`.

**Verification.** Full `Zend/tests` passes with 0 failures (no opcache); contextual
return/argument literals, every BC context, and the fail-closed diagnostics all
pass without opcache.

### 8d. BLOCKER — the ARG source is not opcache-safe (RETURN source is)

The landing matrix surfaced a real **segfault under opcache** in the *argument*
source. Root cause: the contextual-ARG `ZEND_CONSTRUCT_COLLECTION` reads
`EX(call)->func` (`zend_execute.c` `zend_collection_resolve_contextual`) to find
the pending call's parameter type, i.e. it has a hidden dependency on the *call
frame* that the optimizer does not model. When the callee is provably
side-effect-free (empty body **and** an untyped parameter, so no `RECV` type
check) and the element array const-folds, opcache rewrites the call into just its
argument evaluations plus the constant return value — dropping `INIT_FCALL` /
`DO_FCALL` but keeping the may-throw `CONSTRUCT_COLLECTION`, which then runs with
`EX(call) == NULL`. Backtrace: `EX(call)->func` at
`zend_collection_resolve_contextual`, `source = ARG`. Reproduces under
`-d opcache.enable_cli=1`; a typed parameter, a non-empty callee body, or a
non-constant element all avoid it (they keep the call), and the **RETURN source
is unaffected because it reads `EX(func)` — the running function, never NULL**.

This is the same *class* of issue A5 was about: an opcode with a dependency the
optimizer cannot see. The pipeline paper's assumption that `EX(call)` is stable
at construction is false under opcache's call optimization.

### 8e. Is `EX(call)` the right carrier? — carrier analysis

The crash is not fundamentally an opcache bug to patch; it is a sign that the
expected type is carried by the wrong thing. The right question is *what carries
the argument's expected type such that it survives optimization by being visible
to the IR*, not *how to stop the optimizer from moving an invisible dependency*.

Grounding facts (from the code): the optimizer models each call as a
`zend_call_info` spanning `caller_init_opline` (`INIT_FCALL`) …
`caller_call_opline` (`DO_FCALL`) with its `SEND`s. The contextual-ARG literal is
compiled as a **free expression** — `INIT_ARRAY` + `CONSTRUCT_COLLECTION` → a TMP
— that is then `SEND`'d. So `CONSTRUCT_COLLECTION` is *not* part of any
`zend_call_info`; its dependency on `EX(call)` exists only inside the VM handler,
nowhere in the IR. Eliminating the call removes the `SEND` but leaves the free
`CONSTRUCT_COLLECTION` → it runs with `EX(call) == NULL`.

Candidate carriers, judged by "does it survive optimization *naturally*":

| Carrier | Survives? | Why |
|---|---|---|
| **A. `EX(call)` at construct time** (current) | ✗ | transient control object, invisible to the IR; optimizer orphans the opcode |
| **B. compile-time bake from a statically-known callee** | ✗ | opcache-unsound (the callee can be redefined between compile and reuse) and no dynamic calls |
| **C. per-opline run-time cache on `CONSTRUCT_COLLECTION`** | ✗ | populating it still reads the transient callee — same invisibility |
| **D. bind construction to the argument `SEND`** | ✓ | `SEND` *is* the IR representation of "argument N of this call"; it lives inside the `zend_call_info` the optimizer already tracks, so it cannot be separated from its call, and being may-throw it keeps the call from being eliminated. The intermediate elements-array is never observable (built, immediately sent). Fail-closed falls out: the send sees the parameter type. |
| **E. incomplete value completed at `RECV`** | ✗ | breaks fail-closed for an untyped parameter (array would pass through untyped), forces every arg/coercion/by-ref path to understand a half-formed value, leaks — the pipeline paper's rejection stands |

**Conclusion.** `EX(call)` is **not** the correct carrier: it is a transient
control object outside the IR, so any opcode depending on it is at the mercy of
transformations that reason only about IR data-flow. The argument's expected type
is intrinsically *call-scoped*, so its construction belongs to the one IR
construct that is intrinsically call-scoped — the argument `SEND` (carrier **D**).
That is why the **RETURN** source is already correct: it reads `EX(func)`, the
stable, always-present executing function, which no transformation removes;
`EX(call)` has no such stability.

**Recommended direction:** implement the ARG path as a construction bound to
argument passing — a dedicated send (e.g. `ZEND_SEND_COLLECTION`) that takes the
elements array and builds the collection from the pending callee's parameter type
*at send time*, so the dependency is structural and optimizer-visible. Teaching
the optimizer to preserve an invisible `CONSTRUCT_COLLECTION`↔call edge (working
*against* the optimizer) is the fallback only if D proves infeasible. The RETURN
source needs no change.

**Not committing:** the landing matrix is not clean, so no commit is prepared.
- Grammar: `collection_literal` gains `T_VEC_LBRACE collection_literal_elements '}'`
  → `zend_ast_create_ex(ZEND_AST_COLLECTION, ZEND_COLLECTION_TYPE_VEC, NULL, $2)` —
  same node, `child[0] = NULL` marks contextual. Must keep `%expect 0` (no new
  conflicts).
- Token constants propagate to generated `zend_language_parser.h`,
  `ext/tokenizer/tokenizer_data.stub.php` (+ regenerated `_arginfo.h`,
  `tokenizer_data.c`); a tokenizer test asserts the new heads.

## 9. Diagnostics

- No source (generic path): *"Cannot infer the element type of `vec{…}` here; write
  `vec[int]{…}` to state it."* (compile error)
- Return kind/shape mismatch (compile time): *"`vec{…}` cannot be returned; the
  declared return type `int` is not a `vec` type"* / kind mismatch / tuple arity.
- Arg, no collection at position (runtime, fail-closed): *"Cannot infer collection
  type for argument #N of foo(); parameter is not a collection type. Write
  `vec[int]{…}`."*
- Arg/return kind mismatch (runtime): *"Cannot use `set{}` where `vec[int]` is
  expected."*
- Element type mismatch / tuple arity: the **existing** construction errors, unchanged.
- `map{…}` / `shape{…}`: the existing "not implemented yet" compile error.

## 10. What is provably unchanged

Runtime value is identical to the equivalent explicit literal (same descriptor via
`resolve()`, same `zend_collection_construct`, same canonical node, IS_COLLECTION,
identity, truthiness, comparison, serialization, reflection, GC). Optimizer/opcache/JIT:
the opcode still emits one `ZEND_CONSTRUCT_COLLECTION`; SSA sees no new
compile-time-known value (Model A, no synthesis); JIT bails to the same VM handler;
`collection_types[]` and run_time_cache layouts are unchanged, so all six opcache
persistence sites are untouched. The A5 `MAY_BE_COLLECTION` work is unaffected (no
new `ZEND_TYPE_CHECK` interaction).

## 11. Test matrix

**Positive** (explicit vs contextual compared directly): return + argument for
vec/set/tuple; empty `vec{}`; nullable `?vec[int]` param/return; method/static/
dynamic/constructor argument; variadic target; strict vs weak element handling
(inherited, both `strict_types` values); parenthesised operand; multiple contextual
literals in one call; identity — contextual value's node `===`/type-id equals the
explicit value's.

**Negative**: no source (`$x = vec{}`, `echo vec{}`, array element) → compile error;
non-collection / wrong-kind / union / intersection / `mixed` return → compile error;
untyped or union param arg → runtime `TypeError`; kind mismatch (`set{}`→`vec[int]`);
tuple arity; element type mismatch; contextual `map{}`/`shape{}` → not-implemented;
named-argument contextual literal → deferred compile error; const-expr position.

**Regression**: all existing explicit literals; ordinary arrays; `vec`/`set`/`tuple`
as function/const/class names; A5 strict null/bool; serialization; reflection; the
canonicalization/lifecycle tests (the three opcache-only failures are the separate
pre-existing issue — reproduced, not touched).

**Matrix**: release; opcache no-JIT; function-JIT; tracing-JIT; debug; debug+ASAN —
reported separately.
