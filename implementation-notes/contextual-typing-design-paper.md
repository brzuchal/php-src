# Contextual typing for collection literals — a semantic analysis

A language-design paper. Semantics first; syntax only at the end.

---

## 1. What kind of term is `vec{1,2,3}`?

The premise — that a literal may be incomplete until context supplies its type —
is not a small ergonomic concession. It changes the *kind* of thing an expression
is, and it introduces a mechanism PHP does not currently have.

A type system assigns terms to one of two modes:

- **Synthesis** (`Γ ⊢ e ⇒ T`) — the term determines its own type, bottom-up.
  Every expression in PHP today is of this kind. `[1,2,3]` is `array`. `"x"` is
  `string`. Nothing outside the expression is consulted.
- **Checking** (`Γ ⊢ e ⇐ T`) — the term is *checked against* a type supplied from
  outside. The term alone does not denote a type.

`vec{1,2,3}` under the stated premise is a **checking-mode term**. It is not an
expression with an unknown type; it is an expression with **no type of its own**,
which becomes a value only when a type is pushed into it.

This is bidirectional type checking. PHP would be acquiring its first
checking-mode term. That is the central fact of this design, and everything below
follows from it.

**Precedent elsewhere.** Swift array literals (`let s: Set<Int> = [1,2,3]`),
C# 12 collection expressions (`List<int> x = [1,2,3]`), Java's diamond
(`new ArrayList<>()`), Scala and C# lambdas, TypeScript's contextual typing of
object literals. All are target-typed; none carry a type independently. So the
model is well-trodden — but in each of those languages it arrived alongside a
type system already designed bidirectionally. PHP's is not.

---

## 2. Taxonomy: what is `(Users) vec{…}`, precisely?

| Concept | Definition | Operand has a type? | Changes the value? | Can fail? |
|---|---|---|---|---|
| **Cast** | Takes a value of type S, yields a value of type T, possibly re-representing it | **yes** | **often** | in PHP, rarely — it coerces |
| **Conversion** | Same, generalised | yes | often | varies |
| **Type ascription** | Asserts/【in bidirectional systems】supplies the type a term is checked against. The value is unchanged | **no** (in checking mode) | **never** | **yes — that is its purpose** |
| **Contextual typing** | The *mechanism* by which an expected type flows inward. Not itself a syntax | n/a | n/a | n/a |
| **Type witness** | A runtime value carrying evidence of a type (a token, a proxy) | n/a | n/a | n/a |

Against that table:

- It is **not a cast or conversion.** A cast requires an operand that already has
  a type. `vec{1,2,3}` has none. There is nothing to convert *from*.
- It is **not a type witness.** Nothing is passed at runtime; the type is
  resolved statically (and, in this implementation, promoted to a canonical node
  at first execution). No evidence value exists.
- It is **not contextual typing** as such — contextual typing is the mechanism.
  This is one *source* feeding that mechanism.

> **`(Users) vec{…}` is type ascription: an explicit, inline source of the
> expected type for a checking-mode term.**

It stands in the same relation to the type system as a parameter declaration
does. The difference is only *where the type is written*.

---

## 3. Are `foo(vec{1,2,3})` and `$x = (Users) vec{1,2,3}` the same operation?

**In the type system: yes.** Both supply an expected type to a checking-mode
term; the checking rule invoked is identical. Where the type came from — a
declaration elsewhere, or an annotation here — is a fact about the *program
text*, not about the operation.

Two differences exist, and neither is essential:

1. **Blame assignment.** On failure, `foo(vec{…})` reports an argument mismatch;
   the ascribed form reports that this literal cannot be a `Users`. Diagnostics
   differ; the check does not.
2. **Availability.** This is the real asymmetry, and it is the entire
   justification for having an explicit form: **PHP has positions that supply no
   expected type.** Local variables are untyped. So

   ```php
   $x = vec{1,2,3};        // no context exists — unresolvable
   ```

   is not a stylistic choice; it is a term the type system cannot complete.
   Ascription exists to *create* a context where the language provides none.

So: not redundant with parameter/return/property context — **complementary to
it**, covering exactly the positions those cannot reach.

---

## 4. Alias models

Given `type Users = vec[User]`.

### Model 1 — Transparent aliases

`Users` is erased after resolution; it is pure abbreviation, indistinguishable
from `vec[User]` in identity, reflection and diagnostics.

**Does `(Users) vec{…}` make sense?** **Yes**, trivially — it is shorthand for
`(vec[User]) vec{…}`. Fully coherent, and fully *optional*: anything expressible
with the alias is expressible without it.

**Consequence:** under Model 1 the case for alias-specific construction is at its
weakest. The alias adds no information the expanded type lacks.

### Model 2 — Aliases preserved for diagnostics and reflection

Semantically identical to Model 1; the name is retained as metadata.

**Does it make sense?** **Yes**, and it acquires a second, genuine function:
ascription now *selects which name is displayed* in errors and
`ReflectionType::__toString()`. `(Users) vec{…}` documents intent and improves
diagnostics.

One question this model must answer: if two aliases denote the same structural
type, which name survives? The ascribed one is the natural rule, but it must be
stated — otherwise diagnostics become order-dependent.

### Model 3 — Nominal aliases

`Users` is a **distinct type** from `vec[User]`; they are not interchangeable
(a newtype).

**Does it make sense?** **Yes — and here it becomes necessary rather than
convenient.** Under nominality, structure no longer determines identity: if
`Users` and `Admins` are both nominal aliases of `vec[User]`, a literal cannot
determine which it is. Only an expected type can. Ascription becomes the sole way
to construct a nominal alias where no declared context exists.

**Note what does *not* change.** Even here, `(Users)` is not a conversion. The
operand never had a type; nothing is being converted. It is still ascription —
now selecting among distinct types rather than merely naming one.

### Result

**Ascription is coherent under all three models.** The alias model determines how
much *value* it carries — negligible under Model 1, real under Model 2,
load-bearing under Model 3 — not what it *is*.

This is a useful decision rule: **if aliases are transparent, an explicit
ascription form is close to unnecessary; if aliases are nominal, it is
unavoidable.** The alias model should therefore be decided *before* the syntax
question, not after.

---

## 5. Is cast syntax appropriate?

The semantics are sound. The surface is not. Three arguments, the third decisive.

**1. Opposite failure semantics.** PHP casts coerce and almost never fail:
`(int)"abc"` is `0`, `(array)$obj` always succeeds. Ascription must **fail** on
mismatch — a failing check is its purpose. Using the language's most permissive
syntax for its strictest operation inverts the reader's expectation.

**2. Inverted operand model.** A cast reads as "take this value and make it a
T". Ascription is "this term has no type; let it be a T". The cast spelling
implies an operand that already denotes something.

**3. It would create PHP's first user-type cast — decisive.** PHP's cast syntax
is a **closed set of built-in types**: `(int) (float) (string) (bool) (array)
(object)`. There is deliberately no `(Foo)` cast for user types. Writing
`(Users)` introduces user-defined names into that syntactic slot, and readers
will immediately and reasonably infer that `(SomeClass) $obj` also works. It does
not, and we are not proposing that it should. **The syntax would make a promise
about the language that the language does not keep.**

That is not a matter of taste; it is a false signal in the surface syntax, and it
would be raised on the first review.

### What follows

Rejecting cast *syntax* does not reject the *semantics*. The ascription model
stands. What it needs is a spelling that reads as "check this against T" rather
than "convert this to T" — and, per §7, it may not need a spelling at all yet.

Worth noting: **`Users::from(vec{1,2,3})` already is contextual typing**, using
only existing machinery. If `from` is declared `static function from(Users $v)`,
the parameter supplies the expected type and the literal is checked against it —
the identical operation, with zero new syntax and correct failure semantics. For
aliases specifically, this covers the case.

---

## 6. The hidden cost: context propagation must be specified

This is the part most likely to be underestimated, and it is larger than the
parser question that has occupied the previous rounds.

Once one term is checking-mode, **every** expression form must state whether and
how an expected type flows through it:

```php
$a ? vec{1} : vec{2}        // does context reach both branches?
vec{ vec{1,2} }             // inner literal — from the outer element type?
foo(...$args)               // spread: what context does each element get?
match ($x) { 1 => vec{…} }  // do match arms inherit the match's target?
vec{…} ?? $fallback         // does ?? propagate, and to which side?
[vec{…}]                    // an array element has no declared type — error?
$f = fn() => vec{…};        // arrow-function body: from the return type, if any
```

Each has a defensible answer; none has a *default* answer. PHP has never
specified rules of this kind because it has never had a checking-mode term.

Two constraints make this tractable, and both should be adopted explicitly:

- **Locality.** PHP compiles per file with no whole-program inference. The
  expected type must be syntactically apparent at the use site. Therefore
  `$x = vec{1}; foo($x);` can **never** work — inference does not cross
  statements.
- **Fail-closed.** A checking-mode term in a position that supplies no expected
  type is a **compile error** ("cannot infer collection type"), not a
  best-effort inference from the elements. Guessing `vec{1,2}` to be `vec[int]`
  would make the type of a literal depend on its contents, which then silently
  changes when a `2.0` is added.

---

## 7. Should explicit contextual typing become a general language concept?

I think the honest answer reframes the question.

If `vec{…}` needs an expected type, and PHP's declared-type positions are
parameters, returns, properties and class constants, then the gap is precisely:
**local variables have no declared type.** That is not a collections problem. It
is a general hole in PHP's type system that collections happen to expose.

Two ways to fill it:

- **A collection-specific ascription operator.** Solves the symptom. Applies to
  one feature. If typed locals ever arrive, it becomes redundant — a second way
  to say the same thing, permanently.
- **Typed local variables** (`vec[int] $x = vec{1,2,3};`). Solves the cause,
  generalises to every future checking-mode term (lambdas with inferred
  parameters, future literal forms), and matches how properties already work.

**Recommendation: do not generalise ascription now, and do not introduce a
collection-specific form either.** Ship contextual typing as a *checking rule*
that consumes the four type sources PHP already has. If the untyped-local gap
proves painful in practice, the principled fix is typed locals — a general
feature, decided on its own merits.

This is the long-term coherence argument: introducing feature-specific ascription
first means solving a general problem with a special mechanism, and then living
with both.

---

## 8. Recommendations

**Semantic model (adopt):**

1. Collection literals are **checking-mode terms** — target-typed, with no type
   of their own.
2. Checking is **strictly local**: the expected type must be syntactically
   apparent at the use site. No cross-statement inference.
3. Absence of an expected type is a **compile error**, never element-based
   inference.
4. Context propagation rules must be **enumerated explicitly** for every
   expression form (§6). This is a required part of the RFC, not an
   implementation detail.

**Syntax (defer):**

5. **Reject cast syntax** `(Users) vec{…}` — not because the semantics are
   wrong, but because the spelling asserts PHP has user-type casts. It does not.
6. **Ship no explicit ascription form in RFC 1.** The four existing type sources
   cover the common cases; `Users::from(…)` covers aliases through ordinary
   parameter typing, with correct semantics and no new syntax.

**Sequencing (the finding that matters most):**

7. **Decide the alias model before the syntax.** §4 shows the value of an
   explicit form is near-zero under transparent aliases and unavoidable under
   nominal ones. Choosing syntax first would settle the type-system question by
   accident.

**A correction to my earlier review.** I previously dismissed `(Users) vec{…}` as
"reads as a cast, which it is not." The dismissal reached the right conclusion
about the *surface*, but analysed the wrong *semantics*: I treated it as a
conversion. It is ascription, and ascription is a legitimate and useful concept
that this design may eventually need. What should be rejected is the cast
spelling — not the idea.

**A question the premise raises that should be answered before either.**
Contextual typing is being assumed, not decided. The alternative — requiring
`vec[int]{1,2,3}` always — removes bidirectional typing, §6's propagation rules,
the ascription question and the alias-construction question *entirely*, at the
cost of verbosity in declared-type positions. Given a "smallest coherent
language" objective, that alternative deserves to be stated and rejected on the
record rather than assumed away.
