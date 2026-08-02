/*
   +----------------------------------------------------------------------+
   | Zend Engine                                                          |
   +----------------------------------------------------------------------+
   | Copyright © Zend Technologies Ltd., a subsidiary company of          |
   |     Perforce Software, Inc., and Contributors.                       |
   +----------------------------------------------------------------------+
   | This source file is subject to the Modified BSD License that is      |
   | bundled with this package in the file LICENSE, and is available      |
   | through the World Wide Web at <https://www.php.net/license/>.        |
   |                                                                      |
   | SPDX-License-Identifier: BSD-3-Clause                                |
   +----------------------------------------------------------------------+
   | Authors: Michał Marcin Brzuchalski <brzuchal@php.net>                |
   +----------------------------------------------------------------------+
*/

#ifndef ZEND_VEC_H
#define ZEND_VEC_H

#include <stddef.h>

#include "zend_types.h"
#include "zend_collection_info.h"

BEGIN_EXTERN_C()

/* An immutable typed sequence. Elements are stored contiguously in the same
 * allocation as the header, so a vec is one allocation and is never resized.
 *
 * This is deliberately not a zend_array: generic array helpers allocate and
 * persist exactly sizeof(zend_array), so a vec must not be reachable through
 * them. Nothing here may be passed to zend_array_dup(), SEPARATE_ARRAY() or
 * the array persistence paths.
 *
 * The declared type is a *borrowed* canonical node owned by the request intern
 * tier (see zend_collection_info.h). A vec never owns, copies, addrefs or
 * releases it, so construction and destruction do no type-ownership work, and
 * an arena or SHM descriptor cannot be reached from a value: promotion strips
 * provenance and stores nested members as canonical child nodes. */
typedef struct _zend_vec {
	zend_refcounted_h gc;
	const zend_collection_info *type;   /* borrowed; the whole vec[T], not just T */
	uint32_t          count;
	zval              elements[1];
} zend_vec;

/* The declared element type of a vec: T in vec[T]. */
#define ZEND_VEC_ELEMENT_TYPE(vec) ((vec)->type->types[0])

/* offsetof is the only layout contract; do not assume a fixed header size. */
#define ZEND_VEC_HEADER_SIZE     offsetof(zend_vec, elements)

/* A collection zval: IS_COLLECTION is the runtime type, IS_VEC_GC is the
 * allocation kind. Every vec is collectable, exactly like an array or object. */
#define IS_COLLECTION_EX \
	(IS_COLLECTION | ((IS_TYPE_REFCOUNTED|IS_TYPE_COLLECTABLE) << Z_TYPE_FLAGS_SHIFT))

#define ZVAL_VEC(z, v) do {                     \
		zval *__z = (z);                        \
		Z_COUNTED_P(__z) = (zend_refcounted *) (v); \
		Z_TYPE_INFO_P(__z) = IS_COLLECTION_EX;  \
	} while (0)

#define Z_VEC(zval)              ((zend_vec *) Z_COUNTED(zval))
#define Z_VEC_P(zval_p)          Z_VEC(*(zval_p))
#define ZEND_VEC_COUNT(vec)      ((vec)->count)
#define ZEND_VEC_ELEMENTS(vec)   ((vec)->elements)

/* ---- Storage contract ------------------------------------------------------
 * A narrow internal contract so collection operations do not hard-code the
 * flat contiguous payload: every consumer goes through the representation
 * dispatch and the primitive vocabulary below (get / iterate / count / span
 * enumeration), never through raw `elements` arithmetic.
 *
 * With a single FLAT representation the tag is a COMPILE-TIME CONSTANT: every
 * non-flat switch arm is dead-code-eliminated and the hot get/count/iterate
 * collapse to the exact `elements[i]` / `count` loads they compiled to before
 * this contract existed, so the abstraction adds zero overhead on the flat
 * backend. A second representation replaces ZEND_STOR_REPR() with a real
 * per-value runtime tag; call sites and primitives stay identical. */
typedef enum _zend_stor_repr {
	ZEND_STOR_FLAT = 0,
} zend_stor_repr;

/* Single-representation constant; the argument is evaluated for
 * side-effect-freedom only. */
#define ZEND_STOR_REPR(c)        ((void) (c), ZEND_STOR_FLAT)

/* Hot path: logical element count. Identical for every representation. */
static zend_always_inline uint32_t zend_stor_count(const zend_vec *c)
{
	return c->count;
}

/* Hot path R1: O(1) random read of element `index` (0 <= index < count). Backs
 * the single scalar dim-read choke point (zend_collection_dim_lookup). */
static zend_always_inline zval *zend_stor_get(const zend_vec *c, uint32_t index)
{
	switch (ZEND_STOR_REPR(c)) {
		case ZEND_STOR_FLAT:
			return (zval *) &c->elements[index];
		default: ZEND_UNREACHABLE();
	}
}

/* Hot path R2: ordered read at logical position `pos`. The foreach cursor stays
 * a bare uint32_t index (the JIT/VM assume u2.fe_pos is a plain int and that a
 * collection owns no ht_iterators slot, so the cursor must stay index-shaped). */
static zend_always_inline zval *zend_stor_iter(const zend_vec *c, uint32_t pos)
{
	switch (ZEND_STOR_REPR(c)) {
		case ZEND_STOR_FLAT:
			return (zval *) &c->elements[pos];
		default: ZEND_UNREACHABLE();
	}
}

/* GC / serialization / bulk traversal: enumerate contiguous zval runs. FLAT is
 * exactly one span {elements, count}; hybrid/trie yield base + chunk/leaf spans.
 * GC keeps walking contiguous runs (no per-element call on the mark/scan path). */
typedef struct _zend_stor_span {
	zval    *base;
	uint32_t n;
} zend_stor_span;

static zend_always_inline uint32_t zend_stor_span_count(const zend_vec *c)
{
	switch (ZEND_STOR_REPR(c)) {
		case ZEND_STOR_FLAT:
			return 1;
		default: ZEND_UNREACHABLE();
	}
}

static zend_always_inline zend_stor_span zend_stor_span_get(const zend_vec *c, uint32_t s)
{
	ZEND_ASSERT(s < zend_stor_span_count(c));
	switch (ZEND_STOR_REPR(c)) {
		case ZEND_STOR_FLAT: {
			zend_stor_span sp;
			sp.base = (zval *) c->elements;
			sp.n    = c->count;
			return sp;
		}
		default: ZEND_UNREACHABLE();
	}
}

/* Encapsulated builder append: move `value` into the next builder slot and raise
 * count (install-then-publish). Replaces the raw elements[count]/count++ that the
 * ADD_COLLECTION_ELEMENT VM handler open-codes, so the payload layout stays
 * private. Ownership of `value` transfers into the slot (caller relinquishes). */
static zend_always_inline void zend_stor_builder_append(zend_vec *b, zval *value)
{
	switch (ZEND_STOR_REPR(b)) {
		case ZEND_STOR_FLAT:
			ZVAL_COPY_VALUE(&b->elements[b->count], value);
			b->count++;
			return;
		default: ZEND_UNREACHABLE();
	}
}

/* The element subset a value may hold, for a *leaf* member: a pure builtin mask
 * of exactly one element kind, or a single class-name zend_string with no extra
 * may-be bits. Narrower than what canonicalization accepts as a *type*.
 *
 * Nested members are deliberately rejected here rather than walked: promotion
 * classifies each node once and records the answer in its
 * ZEND_COLLECTION_INFO_VALUE_CONSTRUCTIBLE bit, which is what construction
 * reads. This function is the leaf policy that classification consults, not a
 * runtime check. */
ZEND_API bool zend_vec_type_is_supported(zend_type type);

/* Build a vec from a packed list of values. This is the only construction
 * entry point: reserving storage and installing elements are private to
 * zend_vec.c, so the capacity invariant they share cannot be violated from
 * outside. On any element failing validation the partially built vec is
 * destroyed, touching only the slots already installed, and NULL is returned.
 *
 * `failed_index`, when not NULL, receives the position of the element that was
 * rejected, so a caller can name it in a diagnostic without re-running the
 * element check -- which is private, and would have to be exported for the
 * caller to repeat it. It is written only when NULL is returned. */
ZEND_API zend_vec *zend_vec_create(
	const HashTable *values, const zend_collection_info *type, uint32_t *failed_index);

/* Build a new vec from an existing one plus one more value, appended (prepend ==
 * false) or prepended (prepend == true). The result carries `base`'s exact
 * borrowed descriptor; `base` is never mutated. Only the new value is validated
 * against the element type (base's elements are already valid). Returns a fresh
 * vec (refcount 1), or NULL when the value does not satisfy the element type
 * (caller raises a TypeError). This is the primitive behind vec::append/prepend. */
ZEND_API zend_vec *zend_vec_create_with(const zend_vec *base, zval *value, bool prepend);

/* Outcome of an index-addressed builder (with_at / without_at). Only OK yields a
 * result; the two failure codes tell the caller which diagnostic to raise, so the
 * distinct index-vs-value error surfaces stay in the handler, not the primitive. */
typedef enum _zend_vec_with_status {
	ZEND_VEC_WITH_OK = 0,
	ZEND_VEC_WITH_BAD_INDEX,   /* index outside 0..count-1 -> ValueError */
	ZEND_VEC_WITH_BAD_VALUE,   /* value fails the element type -> TypeError */
} zend_vec_with_status;

/* Build a new vec that is `base` with the element at `index` replaced by `value`.
 * `index` is a wide value compared against count without truncation, so an index
 * beyond UINT32_MAX is rejected, not wrapped. On success returns a fresh vec
 * (refcount 1) carrying base's exact descriptor with *status == OK; `base` is
 * never mutated. On failure returns NULL (nothing allocated) with *status set to
 * BAD_INDEX (out of range) or BAD_VALUE (value fails the element type). The
 * primitive behind vec::withAt. */
ZEND_API zend_vec *zend_vec_with_at(
	const zend_vec *base, zend_long index, zval *value, zend_vec_with_status *status);

/* Build a new vec that is `base` with the element at `index` removed and the
 * following elements compacted down. Removing the only element yields an empty
 * vec that still carries base's descriptor. On success returns a fresh vec
 * (refcount 1) with *status == OK; `base` is never mutated. On failure returns
 * NULL with *status == BAD_INDEX (the only possible failure: there is no value to
 * type-check). The primitive behind vec::withoutAt. */
ZEND_API zend_vec *zend_vec_without_at(
	const zend_vec *base, zend_long index, zend_vec_with_status *status);

/* Outcome of the tuple index-addressed builder (with_at). Mirrors the vec status,
 * but a tuple is positional: BAD_VALUE means the replacement fails the type of the
 * *selected position*, not a single uniform element type. */
typedef enum _zend_tuple_with_status {
	ZEND_TUPLE_WITH_OK = 0,
	ZEND_TUPLE_WITH_BAD_INDEX,   /* index outside 0..arity-1 -> ValueError */
	ZEND_TUPLE_WITH_BAD_VALUE,   /* value fails the selected position's type -> TypeError */
} zend_tuple_with_status;

/* Build a new tuple equal to `base` with the element at `index` replaced by
 * `value`. A tuple is fixed-arity and positional: the arity never changes, and
 * `value` is validated against the descriptor member for `index` (member `index`,
 * not member 0 as for a vec), so each position keeps its own declared type. base's
 * exact descriptor is preserved (borrowed) and base is never mutated; the other
 * elements are already valid and are copied without re-checking. On success returns
 * a fresh tuple (refcount 1) with *status == OK; on failure returns NULL (nothing
 * allocated) with *status BAD_INDEX (out of range) or BAD_VALUE (wrong type for that
 * position). The primitive behind tuple::withAt. */
ZEND_API zend_vec *zend_tuple_with_at(
	const zend_vec *base, zend_long index, zval *value, zend_tuple_with_status *status);

/* Outcome of the set builders (with / without). A set operation may legitimately
 * change nothing -- adding a present value or removing an absent one -- so UNCHANGED
 * is a first-class result, distinct from a type failure. */
typedef enum _zend_set_with_status {
	ZEND_SET_WITH_CHANGED = 0,   /* a new set was allocated */
	ZEND_SET_WITH_UNCHANGED,     /* no-op: value already present (with) / absent (without) */
	ZEND_SET_WITH_BAD_VALUE,     /* value fails the element type -> TypeError */
} zend_set_with_status;

/* Build the set that is `base` with `value` added. Membership is strict identity
 * (zend_is_identical, ===). `value` is validated against the element type first: a
 * wrong type is *status == BAD_VALUE (return NULL), even though it could never be a
 * member. If the value is already present nothing is allocated and the receiver is
 * returned as an *owned* reference (refcount raised) with *status == UNCHANGED;
 * otherwise a fresh set (refcount 1) is returned with the existing members in order
 * plus `value` appended, *status == CHANGED. base's exact descriptor is preserved
 * (borrowed) and base is never mutated. The primitive behind set::with. */
ZEND_API zend_vec *zend_set_with(
	const zend_vec *base, zval *value, zend_set_with_status *status);

/* Build the set that is `base` with `value` removed. `value` is validated against
 * the element type first (BAD_VALUE -> NULL), even though a wrong-typed value can
 * never be a member. Membership uses the same strict identity predicate as with()
 * and construction. If the value is absent nothing is allocated and the receiver is
 * returned as an *owned* reference with *status == UNCHANGED; otherwise a fresh set
 * with the first matching member removed and the rest compacted (order preserved) is
 * returned with *status == CHANGED. Removing the only member yields an empty set of
 * the same descriptor. base's exact descriptor is preserved and base is never
 * mutated. The primitive behind set::without. */
ZEND_API zend_vec *zend_set_without(
	const zend_vec *base, zval *value, zend_set_with_status *status);

/* The three binary set operations. Each requires `base` and `other` to carry the
 * *same* descriptor (the caller enforces this by pointer identity before calling;
 * asserted here) and returns a set of that descriptor. Membership is strict identity
 * (zend_is_identical). Order is receiver-driven (see each). Nothing is allocated for an
 * empty-effect result: `base` is returned as an owned reference (refcount raised),
 * exactly like the with/without no-op path (INV-34); a changed result is a fresh set
 * (refcount 1). `base` is never mutated. With packed storage and linear membership
 * these are O(count(base)*count(other)).
 *
 *   union     — base's members in base order, then other's members not in base, in
 *               other order. No-op when other is a subset of base.
 *   intersect — base's members that are also in other, in base order. No-op when every
 *               base member is in other.
 *   diff      — base's members that are not in other, in base order. No-op when base and
 *               other are disjoint. */
ZEND_API zend_vec *zend_set_union(const zend_vec *base, const zend_vec *other);
ZEND_API zend_vec *zend_set_intersect(const zend_vec *base, const zend_vec *other);
ZEND_API zend_vec *zend_set_diff(const zend_vec *base, const zend_vec *other);

/* Recursive strict value identity for two collection values, both already known to be
 * IS_COLLECTION. Backs the IS_COLLECTION arm of zend_is_identical() (===/!==): same
 * canonical descriptor (kind + element types, by pointer) and strictly identical
 * elements -- positional for vec/tuple, order-insensitive for set (via the shared
 * membership predicate). Allocates nothing, mutates nothing, invokes no userland. */
ZEND_API bool zend_collection_is_identical(const zval *op1, const zval *op2);

/* Construct a value of any packed collection kind (vec, tuple) from a list of
 * already-evaluated elements, dispatching on the resolved node's kind. This is
 * the entry point the construction opcode uses; direct per-kind creators stay
 * internal to zend_vec.c. `type` must be value-constructible. `failed_index`,
 * when not NULL, receives the position of a rejected element; written only when
 * NULL is returned. Storage is shared across kinds, so the result is destroyed
 * with zend_vec_destroy() regardless of kind. */
ZEND_API zend_vec *zend_collection_construct(
	const HashTable *values, const zend_collection_info *type, uint32_t *failed_index);

/* Direct-construction builder (Architecture B) for vec, tuple and set: allocate an
 * exact-size empty payload, store evaluated elements without validating, then validate all
 * slots at FINISH. `alloc` exposes the exact-size allocator; `validate` checks every
 * initialized slot against its member type (member 0 for vec/set, member i for tuple) and
 * reports the first offender's slot index. The element store itself is done inline by the
 * ADD_COLLECTION_ELEMENT VM handler. `dedup` is applied by FINISH for a set only, after
 * validation: it deduplicates the payload in place and lowers `count` to the unique
 * cardinality (the allocated tail becomes unused slack). See zend_vec.c. */
ZEND_API zend_vec *zend_vec_builder_alloc(uint32_t count, const zend_collection_info *type);
ZEND_API bool zend_vec_builder_validate(const zend_vec *vec, uint32_t *failed_index);
ZEND_API void zend_set_builder_dedup(zend_vec *set);

/* Exercise the private construction path's invariants from inside the engine
 * boundary, so they keep direct coverage without re-exporting the two-step
 * constructor. Returns a bitmask of the checks that passed. */
#define ZEND_VEC_SELFTEST_ALLOC_EMPTY          (1u << 0)
#define ZEND_VEC_SELFTEST_FAILED_APPEND_INERT  (1u << 1)
#define ZEND_VEC_SELFTEST_ONLY_INSTALLED       (1u << 2)
#define ZEND_VEC_SELFTEST_DTOR_EXACTLY_ONCE    (1u << 3)
#define ZEND_VEC_SELFTEST_ALL                  (0xfu)

ZEND_API uint32_t zend_vec_lifecycle_selftest(void);

/* Release the element type, the element zvals, and the allocation. Reached
 * through rc_dtor_func() when the refcount drops to zero. */
ZEND_API void ZEND_FASTCALL zend_vec_destroy(zend_vec *vec);

END_EXTERN_C()

#endif /* ZEND_VEC_H */
