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
	uint32_t          count;            /* logical size: iteration/GC/serialize bound  */
	uint32_t          capacity;         /* allocated slots; count <= capacity          */
	zval              elements[1];
} zend_vec;

/* The declared element type of a vec: T in vec[T]. */
#define ZEND_VEC_ELEMENT_TYPE(vec) ((vec)->type->types[0])

/* offsetof is the only layout contract; do not assume a fixed header size. */
#define ZEND_VEC_HEADER_SIZE     offsetof(zend_vec, elements)

/* Whether the HYBRID representation is *used* on this target. This is a
 * deliberately conservative VALIDATION gate, not a correctness gate: the hybrid
 * layout invariants (asserted unconditionally below) are ABI-independent and hold
 * on ILP32 exactly as on LP64, so hybrid is *correct* anywhere those asserts pass.
 * What this macro tracks is which ABIs have actually had the full collection
 * corpus run against them:
 *
 *   - LP64 and LLP64/Windows x64 (SIZEOF_SIZE_T == 8): validated. `capacity`
 *     fills the padding that already sat between `count` and the 8-aligned
 *     `elements[]`, so it costs zero bytes.
 *   - x32, i.e. ILP32 on the x86-64 backend (SIZEOF_SIZE_T == 4 && __x86_64__):
 *     validated via the LINUX_X32 collections CI job. Its `type` pointer is
 *     4 bytes, so `capacity` occupies its own header word rather than free
 *     padding; the base/tail overlay and the bit-31 tag are unaffected (see the
 *     asserts below).
 *
 * Every other ILP32 ABI (classical i386, ARM32, ...) falls back to the always-
 * correct FLAT representation -- identical semantics, only without the retained-
 * append base-sharing win -- until it too passes the corpus. Widen this gate as
 * ABIs are validated; never widen it ahead of a green pipeline. */
#if SIZEOF_SIZE_T == 8
# define ZEND_VEC_HYBRID_SUPPORTED 1          /* LP64 and LLP64/Windows x64        */
#elif SIZEOF_SIZE_T == 4 && defined(__x86_64__)
# define ZEND_VEC_HYBRID_SUPPORTED 1          /* x32 (ILP32 on x86-64): CI-validated */
#else
# define ZEND_VEC_HYBRID_SUPPORTED 0          /* i386, ARM32, ...: flat until validated */
#endif

/* ---- Hybrid layout invariants (ABI-independent; asserted on every target) ----
 * These express what the HYBRID overlay and the bit-31 capacity tag actually
 * require. They hold on LP64, LLP64 and every ILP32 ABI alike, so a build fails
 * only when an invariant is *truly* violated -- never merely because the target
 * is 32-bit. (Whether hybrid is *used* on a target is the separate, conservative
 * decision above.) We assert containment, and never that `capacity` is "free":
 * on LP64 it fills the padding between `count` and the 8-aligned `elements[]`
 * (0 added bytes), on ILP32 it occupies its own word (4 bytes on i386, 8 with the
 * alignment pad on x32/ARM32) -- both correct. The old
 * `offsetof(elements) == offsetof(count) + 8` shortcut held on LP64 (and on i386
 * only by coincidence) but is false on x32/ARM32, so it is deliberately gone. */

/* `count` then `capacity` both lie strictly within the header, ahead of the
 * elements payload: a builder writing elements never clobbers the tag or count,
 * and vice versa. */
ZEND_STATIC_ASSERT(offsetof(zend_vec, capacity) >= offsetof(zend_vec, count) + sizeof(uint32_t),
	"count must precede capacity within the vec header");
ZEND_STATIC_ASSERT(offsetof(zend_vec, elements) >= offsetof(zend_vec, capacity) + sizeof(uint32_t),
	"capacity must lie within the vec header, before the elements payload");

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

/* ---- Hybrid representation tag ---------------------------------------------
 * A HYBRID vec is an immutable flat `base` plus one bounded flat `tail`, so a
 * retained value can be extended without copying the base. The high bit of
 * `capacity` tags a HYBRID root; the low 31 bits then cache the base child's
 * element count (a hybrid has no flat capacity of its own). A FLAT vec never
 * sets the bit: its capacity is a real slot count that the allocation and
 * growth paths cap below 2^31. No generic code may read `->capacity` raw --
 * the tag must be impossible to mistake for a slot count, so every read goes
 * through a masked accessor. `capacity` is an exact-width uint32_t, so the
 * tag's position and the mask are identical on every ABI -- LP64, LLP64/Windows
 * x64 and ILP32 alike; the layout static-asserts above hold on all of them (they
 * check header containment and element alignment, not a fixed 64-bit shape). */
#define ZEND_VEC_HYBRID_FLAG   (UINT32_C(1) << 31)
#define ZEND_VEC_CAP_MASK      (~ZEND_VEC_HYBRID_FLAG)          /* 0x7fffffff */
#if ZEND_VEC_HYBRID_SUPPORTED
# define ZEND_VEC_IS_HYBRID(v)  (((v)->capacity & ZEND_VEC_HYBRID_FLAG) != 0)
#else
/* No hybrid on this target: the tag is never set (creation is compiled out), so
 * every vec reads as flat and the hybrid branches in read/GC/destroy fold away. */
# define ZEND_VEC_IS_HYBRID(v)  ((void) (v), 0)
#endif

ZEND_STATIC_ASSERT(sizeof(((zend_vec *) 0)->capacity) * 8 == 32,
	"the representation tag lives in bit 31 of an exact 32-bit capacity");

/* Flat slot capacity. Meaningful only for a FLAT vec; the mask is defensive (a
 * flat vec never has the tag bit set) and documents that a raw read is a bug. */
#define ZEND_VEC_CAPACITY(v)   ((v)->capacity & ZEND_VEC_CAP_MASK)

/* Maximum representable slot count of a flat vec: the tag bit is reserved, so
 * allocation and growth must never produce a capacity above this. */
#define ZEND_VEC_MAX_CAPACITY  ZEND_VEC_CAP_MASK

/* HYBRID child overlay: the two owned child collection zvals live in the first
 * two element slots, so the GC walker enumerates them as an ordinary 2-zval run
 * and a hybrid root needs no bespoke struct. elements[0] = the immutable FLAT
 * base (owned ref, shared by every branch); elements[1] = the bounded FLAT tail
 * (owned ref). A hybrid allocation is ZEND_VEC_HEADER_SIZE + 2*sizeof(zval). */
#define ZEND_VEC_HYBRID_BASE(v)        (&(v)->elements[0])
#define ZEND_VEC_HYBRID_TAIL(v)        (&(v)->elements[1])
#define ZEND_VEC_HYBRID_BASE_VEC(v)    Z_VEC_P(ZEND_VEC_HYBRID_BASE(v))
#define ZEND_VEC_HYBRID_TAIL_VEC(v)    Z_VEC_P(ZEND_VEC_HYBRID_TAIL(v))
/* Cached base element count (the low 31 bits of the tagged capacity). base is
 * immutable, so this never changes across a hybrid root's lifetime. */
#define ZEND_VEC_HYBRID_BASE_COUNT(v)  ((v)->capacity & ZEND_VEC_CAP_MASK)

/* The overlay assumes the element slots are zvals; a layout change here would
 * silently desync the GC walker and the accessors above. */
ZEND_STATIC_ASSERT(sizeof(((zend_vec *) 0)->elements[0]) == sizeof(zval),
	"hybrid base/tail overlay requires elements[] to be zvals");

/* ...and that the element base is correctly zval-aligned, so &elements[0] and
 * &elements[1] are valid zval storage on every ABI (i386's 4-aligned double
 * included). This is asserted, never left to accidental padding or alignment.
 * The probe struct yields a portable alignof(zval): the offset a zval receives
 * when placed after a single char equals its alignment (no _Alignof/__alignof__
 * dependency, so it holds on every compiler PHP targets). */
struct zend_vec_zval_align_probe { char zvap_c; zval zvap_z; };
ZEND_STATIC_ASSERT(
	offsetof(zend_vec, elements) % offsetof(struct zend_vec_zval_align_probe, zvap_z) == 0,
	"elements[] must be zval-aligned for the hybrid base/tail overlay");

/* ---- Storage contract ------------------------------------------------------
 * A narrow internal contract so collection operations do not hard-code the
 * flat contiguous payload: every consumer goes through the representation
 * dispatch and the primitive vocabulary below (get / iterate / count / span
 * enumeration), never through raw `elements` arithmetic. The tag costs one
 * predictable bit test; FLAT stays the predicted-taken branch on every hot
 * path (the contract was proven zero-overhead with the tag pinned to a
 * compile-time constant before the hybrid representation existed). */
typedef enum _zend_stor_repr {
	ZEND_STOR_FLAT   = 0,
	ZEND_STOR_HYBRID = 1,   /* immutable flat base + one bounded flat tail */
} zend_stor_repr;

/* A real per-value runtime tag, read from the high capacity bit. */
#define ZEND_STOR_REPR(c) \
	(ZEND_VEC_IS_HYBRID(c) ? ZEND_STOR_HYBRID : ZEND_STOR_FLAT)

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
		case ZEND_STOR_HYBRID: {
			/* O(1): one repr test (above) + one base-boundary test. base_count is
			 * cached in the tagged capacity, so the base pointer is dereferenced only
			 * on the branch actually taken. index < count is guaranteed by the caller,
			 * so index - base_count is in range for the tail (all uint32, no overflow). */
			uint32_t base_count = ZEND_VEC_HYBRID_BASE_COUNT(c);
			if (index < base_count) {
				return &ZEND_VEC_HYBRID_BASE_VEC(c)->elements[index];
			}
			return &ZEND_VEC_HYBRID_TAIL_VEC(c)->elements[index - base_count];
		}
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
		case ZEND_STOR_HYBRID: {
			/* Ordered read: the base elements in order, then the tail elements. Same
			 * O(1) mapping as zend_stor_get; the foreach cursor stays a bare uint32_t
			 * position that spans base then tail. */
			uint32_t base_count = ZEND_VEC_HYBRID_BASE_COUNT(c);
			if (pos < base_count) {
				return &ZEND_VEC_HYBRID_BASE_VEC(c)->elements[pos];
			}
			return &ZEND_VEC_HYBRID_TAIL_VEC(c)->elements[pos - base_count];
		}
		default: ZEND_UNREACHABLE();
	}
}

/* GC-children traversal: enumerate the contiguous zval runs whose refcounted
 * members the collector must reach. This is the *GC child* view, NOT logical
 * element iteration (serialize/foreach use zend_stor_get/iter by position):
 *   FLAT   -> one run {elements, count}: a flat vec's GC children ARE its logical
 *             elements.
 *   HYBRID -> one run {elements, 2}: a hybrid root's GC children are exactly its
 *             two owned child collections (base, tail). Their element payloads are
 *             reached when each child is itself visited as a node -- so a shared
 *             base is scanned once, not once per branch, and no base/tail element
 *             is scanned from here. GC walks contiguous runs (no per-element call
 *             on the mark/scan path). */
typedef struct _zend_stor_span {
	zval    *base;
	uint32_t n;
} zend_stor_span;

static zend_always_inline uint32_t zend_stor_span_count(const zend_vec *c)
{
	switch (ZEND_STOR_REPR(c)) {
		case ZEND_STOR_FLAT:
			return 1;   /* the element run */
		case ZEND_STOR_HYBRID:
			return 1;   /* the {base, tail} children run */
		default: ZEND_UNREACHABLE();
	}
}

static zend_always_inline zend_stor_span zend_stor_span_get(const zend_vec *c, uint32_t s)
{
	zend_stor_span sp;
	ZEND_ASSERT(s < zend_stor_span_count(c));
	switch (ZEND_STOR_REPR(c)) {
		case ZEND_STOR_FLAT:
			sp.base = (zval *) c->elements;
			sp.n    = c->count;
			return sp;
		case ZEND_STOR_HYBRID:
			/* Exactly the two child collection zvals overlaid on elements[0..1].
			 * Debug-assert the published-hybrid invariants right at the GC exposure
			 * point: both children initialised collections of the same descriptor,
			 * and the logical count is the sum of the children's counts (so no spare
			 * tail capacity can be mistaken for an initialised element). */
			ZEND_ASSERT(Z_TYPE_P(ZEND_VEC_HYBRID_BASE(c)) == IS_COLLECTION);
			ZEND_ASSERT(Z_TYPE_P(ZEND_VEC_HYBRID_TAIL(c)) == IS_COLLECTION);
			ZEND_ASSERT(!ZEND_VEC_IS_HYBRID(ZEND_VEC_HYBRID_BASE_VEC(c)));
			ZEND_ASSERT(!ZEND_VEC_IS_HYBRID(ZEND_VEC_HYBRID_TAIL_VEC(c)));
			ZEND_ASSERT(ZEND_VEC_HYBRID_BASE_VEC(c)->type == c->type);
			ZEND_ASSERT(ZEND_VEC_HYBRID_TAIL_VEC(c)->type == c->type);
			ZEND_ASSERT(c->count == ZEND_VEC_HYBRID_BASE_VEC(c)->count
			                      + ZEND_VEC_HYBRID_TAIL_VEC(c)->count);
			sp.base = (zval *) c->elements;   /* &elements[0]: the base child zval */
			sp.n    = 2;                       /* base, tail */
			return sp;
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
ZEND_API zend_vec *zend_vec_create_with(zend_vec *base, zval *value, bool prepend, bool exclusive);

/* Append dispatcher (the primitive behind vec::append). Chooses the
 * representation for the result:
 *   - a FLAT base appended non-exclusively (retained/shared receiver) shares the
 *     base and puts the new value in a fresh one-element tail, yielding a HYBRID
 *     root -- the base is never copied (the C1 primary target);
 *   - a FLAT base appended exclusively (a consumable temporary) mutates/grows in
 *     place, returning the base itself;
 *   - a HYBRID base runs the root/tail exclusivity matrix.
 * `value` is validated against the element type first; NULL is returned on a type
 * failure with nothing published (the caller raises a TypeError). `exclusive` is
 * the centralized frame-ownership verdict for the receiver. prepend has no hybrid
 * form in C1 and continues through zend_vec_create_with. */
ZEND_API zend_vec *zend_vec_append_value(zend_vec *base, zval *value, bool exclusive);

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
	zend_vec *base, zend_long index, zval *value, zend_vec_with_status *status, bool exclusive);

/* Build a new vec that is `base` with the element at `index` removed and the
 * following elements compacted down. Removing the only element yields an empty
 * vec that still carries base's descriptor. On success returns a fresh vec
 * (refcount 1) with *status == OK; `base` is never mutated. On failure returns
 * NULL with *status == BAD_INDEX (the only possible failure: there is no value to
 * type-check). The primitive behind vec::withoutAt. */
ZEND_API zend_vec *zend_vec_without_at(
	zend_vec *base, zend_long index, zend_vec_with_status *status, bool exclusive);

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

/* Hybrid ownership selftest. Builds a HYBRID root over a shared flat base and
 * a fresh tail entirely in C, then asserts the ownership invariants the
 * representation promises:
 *  - the root tags HYBRID, count == base_count + tail_count, descriptors match;
 *  - creating the root raises base's refcount by exactly one (base is shared,
 *    never copied) and takes the tail's sole reference;
 *  - a second branch over the same base shares it (refcount 2), and destroying
 *    one branch leaves the base and the other branch intact (independent
 *    lifetimes through refcounting);
 *  - destroying the final root drops base to its original refcount and releases
 *    the tail exactly once (no leak, no double free). */
#define ZEND_HYBRID_SELFTEST_TAGGED_HYBRID     (1u << 0)  /* repr/count/descriptor invariants */
#define ZEND_HYBRID_SELFTEST_BASE_SHARED       (1u << 1)  /* create addrefs base by exactly 1  */
#define ZEND_HYBRID_SELFTEST_TAIL_OWNED        (1u << 2)  /* root takes the tail's sole ref    */
#define ZEND_HYBRID_SELFTEST_BRANCH_INDEP      (1u << 3)  /* destroy one branch, base+other live*/
#define ZEND_HYBRID_SELFTEST_DTOR_BALANCED     (1u << 4)  /* final dtor: base restored, tail freed*/
#define ZEND_HYBRID_SELFTEST_ALL               (0x1fu)

ZEND_API uint32_t zend_hybrid_lifecycle_selftest(void);

/* Flatten-policy selftest: the dispatcher's published representations respect
 * the policy bounds. EMPTY_BASE_FLAT: a retained append to an empty vec stays
 * flat (R forbids tail > R*base at base_count == 0). RETAINED_HYBRID: the
 * positive control -- the same append to a non-empty vec publishes a hybrid
 * sharing the receiver as base with a one-element tail. */
#define ZEND_HYBRID_POLICY_SELFTEST_EMPTY_BASE_FLAT  (1u << 0)
#define ZEND_HYBRID_POLICY_SELFTEST_RETAINED_HYBRID  (1u << 1)
#define ZEND_HYBRID_POLICY_SELFTEST_ALL              (0x3u)

ZEND_API uint32_t zend_hybrid_policy_selftest(void);

/* Release the element type, the element zvals, and the allocation. Reached
 * through rc_dtor_func() when the refcount drops to zero. */
ZEND_API void ZEND_FASTCALL zend_vec_destroy(zend_vec *vec);

END_EXTERN_C()

#endif /* ZEND_VEC_H */
