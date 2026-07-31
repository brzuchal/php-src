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

#include "zend.h"
#include "zend_vec.h"
#include "zend_variables.h"
#include "zend_API.h"
#include "zend_operators.h"
#include "zend_compile.h"
#include "zend_collection_info.h"

ZEND_API bool zend_vec_type_is_supported(zend_type type)
{
	/* The element subset a *value* may hold, which is narrower than the subset
	 * a type may name: canonicalization accepts any builtin mask, while a value
	 * still requires exactly one element kind.
	 *
	 * The arena clause is now unreachable for anything reaching a value. Node
	 * members have provenance stripped during promotion, so the former
	 * restriction -- that a declared vec[int] could not construct a value
	 * because every compiler descriptor is arena-marked -- is discharged. It is
	 * kept as a guard against a raw descriptor being passed in by mistake. */
	if (ZEND_TYPE_IS_TYPE_LIST(type)
	 || ZEND_TYPE_HAS_LITERAL_NAME(type)
	 || ZEND_TYPE_USES_ARENA(type)) {
		return false;
	}

	/* Leaf members only. A nested member is *not* decided here: promotion
	 * classifies the child once and records the verdict in its
	 * VALUE_CONSTRUCTIBLE bit, so nesting is answered by reading that bit
	 * rather than by walking the child again on every construction. */
	if (ZEND_TYPE_HAS_COLLECTION_DESCRIPTOR(type)) {
		return false;
	}

	if (ZEND_TYPE_HAS_NAME(type)) {
		/* A single class name, with no additional may-be bits: no union with a
		 * builtin, no nullable. The name is a refcounted zend_string, not a
		 * resolved class entry; class lookup, autoload and instanceof are
		 * future concerns. */
		return (ZEND_TYPE_FULL_MASK(type) & _ZEND_TYPE_MAY_BE_MASK) == 0;
	}

	/* A pure builtin mask that is exactly one supported element type. The bits
	 * are those the standard ZEND_TYPE_INIT_CODE macro produces (1 << IS_x;
	 * bool is the IS_FALSE/IS_TRUE pair). Matching an exact value deliberately
	 * rejects multi-type masks (int|string), nullable builtins, and the
	 * mask-encoded pseudo-types callable, iterable, static, void, never,
	 * object and mixed. */
	switch (ZEND_TYPE_FULL_MASK(type) & _ZEND_TYPE_MAY_BE_MASK) {
		case (1u << IS_LONG):
		case (1u << IS_DOUBLE):
		case (1u << IS_STRING):
		case ((1u << IS_FALSE) | (1u << IS_TRUE)):
		case (1u << IS_ARRAY):
			return true;
		default:
			return false;
	}
}

static zend_vec *zend_vec_alloc(uint32_t count, const zend_collection_info *type)
{
	/* safe_emalloc computes count * sizeof(zval) + header with overflow
	 * checking, so a large count cannot silently wrap the allocation size. */
	zend_vec *vec = safe_emalloc(count, sizeof(zval), ZEND_VEC_HEADER_SIZE);

	GC_SET_REFCOUNT(vec, 1);
	/* Collectable, uniformly and unconditionally, exactly like an array or an
	 * object. An element may be an object or an array and can therefore close a
	 * cycle back to this vec, so the collector must be able to reach it. The
	 * decision deliberately does not depend on the element type: a per-instance
	 * rule would have to recurse through nested descriptors, and getting it
	 * wrong in the permissive direction leaks while the strict direction frees
	 * live memory. */
	GC_TYPE_INFO(vec) = GC_VEC;
	/* Zero until an element is actually installed, so a failure part-way
	 * through construction never leaves destroy() reading uninitialised slots. */
	vec->count = 0;
	/* Borrowed: owned by the request intern tier, never released here. */
	vec->type = type;

	return vec;
}

/* Does `value` satisfy member `member_idx` of the type? Shallow: a matching
 * element may itself be a mutable array or object, or a nested collection value.
 *
 * The member index generalises the check for positional kinds: vec and set have
 * one member and always pass 0; tuple checks element i against member i. The
 * fast_mask shortcut applies only to a single-member all-builtin node, so it is
 * gated on member 0. */
static bool collection_member_matches(
		const zend_collection_info *info, uint32_t member_idx, zval *value)
{
	zend_type member_type;

	/* Cached: for a builtin member type the whole check is a mask test, and the
	 * member's zend_type is never read. This is the common case. */
	if (member_idx == 0 && info->fast_mask != 0) {
		return (info->fast_mask & (1u << Z_TYPE_P(value))) != 0;
	}

	member_type = info->types[member_idx];

	if (ZEND_TYPE_HAS_COLLECTION_DESCRIPTOR(member_type)) {
		/* Both sides are canonical, so a nested collection element check is a
		 * pointer comparison rather than a structural walk. */
		if (Z_TYPE_P(value) != IS_COLLECTION) {
			return false;
		}
		return Z_VEC_P(value)->type == ZEND_COLLECTION_INFO_CHILD(member_type);
	}

	if (ZEND_TYPE_HAS_NAME(member_type)) {
		zend_class_entry *ce;

		if (Z_TYPE_P(value) != IS_OBJECT) {
			return false;
		}
		ce = zend_lookup_class(ZEND_TYPE_NAME(member_type));
		return ce != NULL && instanceof_function(Z_OBJCE_P(value), ce);
	}

	return ZEND_TYPE_CONTAINS_CODE(member_type, Z_TYPE_P(value));
}

static bool zend_vec_append(zend_vec *vec, zval *value)
{
	ZVAL_DEREF(value);

	if (!collection_member_matches(vec->type, 0, value)) {
		return false;
	}
	/* Install first, then publish the slot by raising count. */
	ZVAL_COPY(&vec->elements[vec->count], value);
	vec->count++;
	return true;
}

ZEND_API zend_vec *zend_vec_create(
		const HashTable *values, const zend_collection_info *type, uint32_t *failed_index)
{
	ZEND_ASSERT(type != NULL && type->num_types >= 1);
	/* Cached at promotion; no recursive re-derivation per construction. */
	ZEND_ASSERT(ZEND_COLLECTION_INFO_IS_VALUE_CONSTRUCTIBLE(type));

	zend_vec *vec = zend_vec_alloc(zend_hash_num_elements(values), type);
	zval *entry;

	ZEND_HASH_FOREACH_VAL((HashTable *) values, entry) {
		if (!zend_vec_append(vec, entry)) {
			/* count is the number of elements already installed, so it is also
			 * the position of the one that was rejected. Read before the vec is
			 * destroyed. */
			if (failed_index != NULL) {
				*failed_index = vec->count;
			}
			/* Only the slots already installed are live, so this releases
			 * exactly those and the element type, and nothing else. */
			zend_vec_destroy(vec);
			return NULL;
		}
	} ZEND_HASH_FOREACH_END();

	return vec;
}

/* Direct-construction builder (Architecture B, spike). The VM allocates an exact-size,
 * empty (count == 0) vec payload, stores each *evaluated* literal element into it WITHOUT
 * type validation (the store + count++ happens in the ADD_COLLECTION_ELEMENT handler), and
 * validates every stored slot at FINISH_COLLECTION. During this window `count` is the
 * *initialized-slot* count, not a user-visible validated cardinality: the payload is an
 * owned VM TMP, never published, and destroy/GC are safe because stored slots hold valid
 * zvals regardless of whether their element type has been checked yet. */
ZEND_API zend_vec *zend_vec_builder_alloc(uint32_t count, const zend_collection_info *type)
{
	/* INIT_COLLECTION allocates the payload *before* the value-constructibility gate, which
	 * FINISH_COLLECTION applies only after every element expression has run -- so that a
	 * non-constructible type (e.g. vec[?int]) still evaluates its elements' side effects
	 * before "Cannot create a value of type ..." is raised, exactly as the array path does.
	 * Only the kind is asserted here; the payload holds valid zvals regardless, is never
	 * published, and is destroyed on the FINISH error path. Shared by vec (single member),
	 * tuple (positional, exact arity) and set. For set, `count` is the *evaluated-element*
	 * count (the allocated capacity); FINISH validates, then deduplicates in place and lowers
	 * `count` to the unique cardinality (see zend_set_builder_dedup) -- the unused tail is
	 * allocated slack, never published or observed. */
	ZEND_ASSERT(type != NULL
		&& (type->kind == ZEND_COLLECTION_TYPE_VEC
			|| type->kind == ZEND_COLLECTION_TYPE_TUPLE
			|| type->kind == ZEND_COLLECTION_TYPE_SET));
	return zend_vec_alloc(count, type);
}

/* Validate every initialized slot of a builder payload against the vec's single member
 * type, in source (slot) order, so the first offender reported matches the observable
 * left-to-right position. Elements were stored dereferenced by ADD, so no deref here.
 * Returns true when all slots pass; otherwise false with *failed_index set. Allocates
 * nothing and mutates nothing. */
ZEND_API bool zend_vec_builder_validate(const zend_vec *vec, uint32_t *failed_index)
{
	/* vec has one member type checked against every slot; tuple is positional, so slot i is
	 * checked against member i (its count equals the arity, compile-checked, which equals
	 * num_types). This mirrors the per-index rule in collection_member_matches() and in
	 * zend_collection_element_type_error_ex(), so the diagnostic names the same member. */
	bool positional = (vec->type->kind == ZEND_COLLECTION_TYPE_TUPLE);
	for (uint32_t i = 0; i < vec->count; i++) {
		if (!collection_member_matches(vec->type, positional ? i : 0, (zval *) &vec->elements[i])) {
			*failed_index = i;
			return false;
		}
	}
	return true;
}

/* Deduplicate a set builder payload in place, after every slot has been validated. On entry
 * `count` is the evaluated-element count (all slots initialized, in source order); on return
 * it is the unique cardinality and elements[0..count) hold the first occurrence of each
 * distinct value in first-occurrence order -- the same result the array path produced via
 * zend_set_create()/zend_set_contains(), using the same zend_is_identical() value identity
 * (order-insensitive, recursive for collections; NAN never equals NAN; objects by handle;
 * arrays by value). No hashing, no loose comparison.
 *
 * Two phases, and the split is the whole point of the design:
 *
 *   1. Partition (no user code, no allocation, no free). A stable read/write scan moves the
 *      unique values to the front and the discarded duplicates to the tail using pure zval
 *      *swaps*. A swap exchanges two owned slots, so every slot holds a distinct, valid,
 *      singly-owned zval at every step -- the payload never contains a stale alias, unlike a
 *      move-compaction. zend_is_identical() over the accepted prefix [0,write) allocates and
 *      frees nothing, so this phase cannot trigger GC or re-enter userland -- but a recursive
 *      strict *array* comparison can throw. If it does, we return at once with `count`
 *      unchanged: the swaps are pure exchanges, so ownership is still bijective and every
 *      initialized slot is owned exactly once, and the caller frees the whole payload on the
 *      exception path. When it ends normally, [0,write) are the uniques (source order) and
 *      [write,count) are the discards.
 *
 *   2. Release. `count` is lowered to `write` *before* any discard is destroyed, so the
 *      payload is already the consistent final set when destructors run. Freeing a discarded
 *      duplicate can decref/free arbitrary values and thereby trigger a GC scan of this
 *      still-owned TMP -- which now sees only the valid unique prefix (GC scans [0,count)),
 *      never the tail being torn down. Each of the original `count` refs is released exactly
 *      once: uniques when the set is finally destroyed, discards here. (In practice a set
 *      discard is value-equal to a kept element, so destructible content is shared and no
 *      userland __destruct runs; the protocol is nonetheless correct if one ever did.) */
ZEND_API void zend_set_builder_dedup(zend_vec *set)
{
	ZEND_ASSERT(set->type->kind == ZEND_COLLECTION_TYPE_SET);

	uint32_t evaluated = set->count;
	uint32_t write = 0;

	for (uint32_t read = 0; read < evaluated; read++) {
		bool duplicate = false;
		for (uint32_t j = 0; j < write; j++) {
			if (zend_is_identical(&set->elements[read], &set->elements[j])) {
				duplicate = true;
				break;
			}
			/* zend_is_identical() invokes no userland and frees nothing, but a recursive
			 * strict *array* comparison can throw ("Nesting level too deep - recursive
			 * dependency?"). Stop the moment it does, leaving `count` unchanged so it still
			 * covers every initialized slot: the swaps done so far are pure exchanges, so
			 * ownership stays bijective and the caller's normal TMP unwind (FREE_OP1) frees
			 * each of the `evaluated` slots exactly once. Do not partition or drop `count`
			 * further, and do not release any discard while an exception is pending. */
			if (UNEXPECTED(EG(exception))) {
				return;
			}
		}
		if (duplicate) {
			/* Leave it where it is; the scan moves later uniques over it, so it ends up in
			 * the [write,evaluated) discard region without a move of its own. */
			continue;
		}
		if (write != read) {
			zval tmp;
			ZVAL_COPY_VALUE(&tmp, &set->elements[write]);
			ZVAL_COPY_VALUE(&set->elements[write], &set->elements[read]);
			ZVAL_COPY_VALUE(&set->elements[read], &tmp);
		}
		write++;
	}

	/* Publish the consistent final cardinality before releasing any discard. */
	set->count = write;
	for (uint32_t k = write; k < evaluated; k++) {
		zval discard;
		ZVAL_COPY_VALUE(&discard, &set->elements[k]);
		ZVAL_UNDEF(&set->elements[k]);
		zval_ptr_dtor(&discard);
	}
}

/* Build a new vec that is `base` with `value` appended (prepend == false) or
 * prepended (prepend == true). The result carries base's EXACT descriptor
 * (base->type, borrowed), so append/prepend on a vec[int] yields a vec[int] with
 * the same canonical node -- never a re-derived one. base's own elements are
 * already valid for that type, so they are copied without re-checking; only
 * `value` is validated against the element type. base is never mutated. Returns
 * a fresh vec (refcount 1) on success, or NULL (nothing is allocated on the
 * failure path) when `value` does not satisfy the element type, so the caller
 * raises a TypeError. */
ZEND_API zend_vec *zend_vec_create_with(const zend_vec *base, zval *value, bool prepend)
{
	ZEND_ASSERT(base->type->kind == ZEND_COLLECTION_TYPE_VEC);
	ZVAL_DEREF(value);
	if (!collection_member_matches(base->type, 0, value)) {
		return NULL;
	}

	zend_vec *out = zend_vec_alloc(base->count + 1, base->type);
	uint32_t at = 0;

	if (prepend) {
		ZVAL_COPY(&out->elements[at++], value);
	}
	for (uint32_t i = 0; i < base->count; i++) {
		ZVAL_COPY(&out->elements[at++], &base->elements[i]);
	}
	if (!prepend) {
		ZVAL_COPY(&out->elements[at++], value);
	}
	/* Publish all slots at once: every slot above is now initialised, so a later
	 * destroy() reads only live zvals. */
	out->count = at;
	return out;
}

/* Build a new vec equal to `base` with the element at `index` replaced by
 * `value`. base's descriptor is preserved exactly (borrowed), base is never
 * mutated, and only `value` is validated -- base's other elements are already
 * valid for the type and are copied without re-checking. Both failure modes are
 * detected before anything is allocated: an out-of-range index (BAD_INDEX) and a
 * value that fails the element type (BAD_VALUE). */
ZEND_API zend_vec *zend_vec_with_at(
		const zend_vec *base, zend_long index, zval *value, zend_vec_with_status *status)
{
	ZEND_ASSERT(base->type->kind == ZEND_COLLECTION_TYPE_VEC);
	/* Wide compare: index is 64-bit and count is 32-bit, so an index at or above
	 * count -- including one beyond UINT32_MAX -- is rejected here rather than
	 * being truncated into range by the later cast. */
	if (index < 0 || index >= (zend_long) base->count) {
		*status = ZEND_VEC_WITH_BAD_INDEX;
		return NULL;
	}

	ZVAL_DEREF(value);
	if (!collection_member_matches(base->type, 0, value)) {
		*status = ZEND_VEC_WITH_BAD_VALUE;
		return NULL;
	}

	uint32_t at = (uint32_t) index;   /* in range: the narrowing is exact */
	zend_vec *out = zend_vec_alloc(base->count, base->type);
	for (uint32_t i = 0; i < base->count; i++) {
		if (i == at) {
			ZVAL_COPY(&out->elements[i], value);
		} else {
			ZVAL_COPY(&out->elements[i], &base->elements[i]);
		}
	}
	out->count = base->count;
	*status = ZEND_VEC_WITH_OK;
	return out;
}

/* Build a new vec equal to `base` with the element at `index` removed and the
 * following elements compacted down by one. base's descriptor is preserved
 * exactly and base is never mutated. Removing the only element yields an empty
 * vec (count 0) that still carries the descriptor; zend_vec_alloc(0, ...) is a
 * header-only allocation, so the empty result has no element storage to read.
 * The only failure is an out-of-range index (BAD_INDEX); there is no value to
 * type-check. */
ZEND_API zend_vec *zend_vec_without_at(
		const zend_vec *base, zend_long index, zend_vec_with_status *status)
{
	ZEND_ASSERT(base->type->kind == ZEND_COLLECTION_TYPE_VEC);
	if (index < 0 || index >= (zend_long) base->count) {
		*status = ZEND_VEC_WITH_BAD_INDEX;
		return NULL;
	}

	uint32_t skip = (uint32_t) index;
	zend_vec *out = zend_vec_alloc(base->count - 1, base->type);
	uint32_t at = 0;
	for (uint32_t i = 0; i < base->count; i++) {
		if (i == skip) {
			continue;
		}
		ZVAL_COPY(&out->elements[at++], &base->elements[i]);
	}
	/* Publish all installed slots at once; at == base->count - 1. */
	out->count = at;
	*status = ZEND_VEC_WITH_OK;
	return out;
}

/* Build a new tuple equal to `base` with the element at `index` replaced by
 * `value`. Unlike a vec, a tuple is positional and fixed-arity: the arity is
 * unchanged and the replacement is validated against the descriptor member for
 * `index` (member `index`, not member 0), so each slot keeps its own declared
 * type. base's descriptor is preserved exactly (borrowed) and base is never
 * mutated; the unchanged elements are copied without re-checking. Both failures
 * are detected before anything is allocated. The range check runs first so that
 * `index` is a valid member index before it selects the position's type. */
ZEND_API zend_vec *zend_tuple_with_at(
		const zend_vec *base, zend_long index, zval *value, zend_tuple_with_status *status)
{
	ZEND_ASSERT(base->type->kind == ZEND_COLLECTION_TYPE_TUPLE);

	/* Wide compare against the arity (== count == num_types), so an index at or
	 * above it -- including one beyond UINT32_MAX -- is rejected here rather than
	 * truncated into range by the later cast. */
	if (index < 0 || index >= (zend_long) base->count) {
		*status = ZEND_TUPLE_WITH_BAD_INDEX;
		return NULL;
	}

	uint32_t at = (uint32_t) index;   /* in range: the narrowing is exact */

	ZVAL_DEREF(value);
	/* Positional: check the replacement against member `at`, the type declared
	 * for exactly this slot -- not member 0. */
	if (!collection_member_matches(base->type, at, value)) {
		*status = ZEND_TUPLE_WITH_BAD_VALUE;
		return NULL;
	}

	zend_vec *out = zend_vec_alloc(base->count, base->type);
	for (uint32_t i = 0; i < base->count; i++) {
		if (i == at) {
			ZVAL_COPY(&out->elements[i], value);
		} else {
			ZVAL_COPY(&out->elements[i], &base->elements[i]);
		}
	}
	out->count = base->count;   /* arity is invariant */
	*status = ZEND_TUPLE_WITH_OK;
	return out;
}

/* Build a tuple: a fixed-arity, positional collection. Storage is the same
 * packed layout as a vec -- header plus contiguous zvals -- so destruction and
 * GC traversal are shared; only the element check differs. Element i is checked
 * against member i, and the element count equals the arity, which the compiler
 * has already enforced against the descriptor. */
static zend_vec *zend_tuple_create(
		const HashTable *values, const zend_collection_info *type, uint32_t *failed_index)
{
	ZEND_ASSERT(type->kind == ZEND_COLLECTION_TYPE_TUPLE);
	ZEND_ASSERT(ZEND_COLLECTION_INFO_IS_VALUE_CONSTRUCTIBLE(type));

	/* The element count must equal the arity: element i is checked against
	 * member i, so a longer list would index a member -- and a tuple slot --
	 * out of range. The literal compiler enforces this, but unserialize()
	 * reconstructs from untrusted input, so this is a runtime check, not an
	 * assert: a mismatch fails construction rather than overflowing. */
	if (zend_hash_num_elements(values) != type->num_types) {
		if (failed_index != NULL) {
			*failed_index = type->num_types;
		}
		return NULL;
	}

	zend_vec *tuple = zend_vec_alloc(type->num_types, type);
	zval *entry;
	uint32_t i = 0;

	ZEND_HASH_FOREACH_VAL((HashTable *) values, entry) {
		zval *value = entry;

		ZVAL_DEREF(value);
		if (!collection_member_matches(type, i, value)) {
			if (failed_index != NULL) {
				*failed_index = i;
			}
			/* count == i, so only the i already installed are released. */
			zend_vec_destroy(tuple);
			return NULL;
		}
		/* Install first, then publish the slot by raising count. */
		ZVAL_COPY(&tuple->elements[i], value);
		tuple->count++;
		i++;
	} ZEND_HASH_FOREACH_END();

	return tuple;
}

/* Is `value` already present in the elements installed so far? Equality is
 * ===: zend_is_identical() compares scalars and arrays by value and objects by
 * identity, which is the deduplication rule a set uses. The scan is over the
 * kept elements only; for the leaf element types a set admits this is a plain
 * value/identity test with no recursion into collections. */
static bool zend_set_contains(const zend_vec *set, const zval *value)
{
	for (uint32_t i = 0; i < set->count; i++) {
		if (zend_is_identical(&set->elements[i], value)) {
			return true;
		}
	}
	return false;
}

/* Recursive strict value identity for two collection values, behind the IS_COLLECTION
 * arm of zend_is_identical() (===/!==). Immutable collections are value types: two are
 * strictly identical iff they share the exact canonical descriptor and their elements
 * are strictly identical.
 *
 *   descriptor  a->type == b->type -- the request-local intern tier gives one node per
 *               (kind, element types), so pointer equality *is* semantic descriptor
 *               equality and already subsumes "same kind". A different descriptor
 *               (vec[int] vs vec[float], vec vs tuple, vec vs set) is never identical,
 *               and equality can therefore never bypass a set's declared element type.
 *   vec / tuple positional, order significant, short-circuit on the first mismatch.
 *   set         order-insensitive. Both operands are unique by invariant with equal
 *               counts, so "every element of `a` has an identical element in `b`" is a
 *               bijection (two `a` elements matching one `b` element would be identical
 *               to each other, impossible in a set) -- i.e. exactly set equality. It
 *               reuses zend_set_contains(), the single membership predicate every set
 *               operation shares, so === and set dedup can never diverge.
 *
 * Elements are compared with zend_is_identical(): scalars/objects/arrays by the existing
 * strict rules, nested collections recurse back here. Collection elements are stored
 * dereferenced at construction, so an element is never IS_REFERENCE (no writable alias is
 * possible and no deref is required, exactly as zend_set_contains() already assumes).
 *
 * The comparator borrows element pointers only: it allocates nothing, mutates neither
 * operand nor any descriptor, exposes no internal pointer to userland, holds no reference
 * to balance on any path, and invokes no userland (strict identity never calls magic), so
 * it is fiber-safe. Recursion is bounded: a collection cannot acquire a self-reference
 * (immutable, built bottom-up from snapshots), and any cycle must pass through a mutable
 * array -- guarded by zend_hash_compare(), which throws "Nesting level too deep" on
 * re-entry -- or an object, which is compared by handle with no descent. */
ZEND_API bool zend_collection_is_identical(const zval *op1, const zval *op2)
{
	const zend_vec *a = Z_VEC_P(op1);
	const zend_vec *b = Z_VEC_P(op2);

	/* Reflexive fast path ($x === $x); mirrors IS_ARRAY's Z_ARRVAL == Z_ARRVAL. */
	if (a == b) {
		return true;
	}
	if (a->type != b->type || a->count != b->count) {
		return false;
	}

	if (a->type->kind == ZEND_COLLECTION_TYPE_SET) {
		for (uint32_t i = 0; i < a->count; i++) {
			if (!zend_set_contains(b, &a->elements[i])) {
				return false;
			}
		}
		return true;
	}

	/* vec and tuple: both are positional and share this loop. */
	for (uint32_t i = 0; i < a->count; i++) {
		if (!zend_is_identical(&a->elements[i], &b->elements[i])) {
			return false;
		}
	}
	return true;
}

/* Build a set: a single-member collection whose elements are unique. Storage is
 * the shared packed layout; duplicates are silently dropped, keeping the first
 * occurrence, so the value holds at most one of each element and preserves
 * source order among the kept ones. Capacity is the input length, an upper
 * bound; the unused tail past count is never read, exactly as for a vec whose
 * construction stopped early. */
static zend_vec *zend_set_create(
		const HashTable *values, const zend_collection_info *type, uint32_t *failed_index)
{
	ZEND_ASSERT(type->kind == ZEND_COLLECTION_TYPE_SET);
	ZEND_ASSERT(ZEND_COLLECTION_INFO_IS_VALUE_CONSTRUCTIBLE(type));

	zend_vec *set = zend_vec_alloc(zend_hash_num_elements(values), type);
	zval *entry;
	uint32_t input_idx = 0;

	ZEND_HASH_FOREACH_VAL((HashTable *) values, entry) {
		zval *value = entry;

		ZVAL_DEREF(value);
		if (!collection_member_matches(type, 0, value)) {
			/* The reported position is the element's place in the source, not
			 * its place among the kept elements, which dedup would skew. */
			if (failed_index != NULL) {
				*failed_index = input_idx;
			}
			zend_vec_destroy(set);
			return NULL;
		}
		/* Silent dedup: a repeat is not an error, it is set semantics. */
		if (!zend_set_contains(set, value)) {
			ZVAL_COPY(&set->elements[set->count], value);
			set->count++;
		}
		input_idx++;
	} ZEND_HASH_FOREACH_END();

	return set;
}

/* Build the set that is `base` with `value` added. Nothing is allocated until the
 * operation is known to change the set: the value is type-checked, then tested for
 * membership with the same strict-identity predicate construction uses, and only an
 * absent value allocates. A present value is a no-op that returns `base` itself as
 * an owned reference (its refcount is raised so the caller's returned value keeps
 * the receiver alive independently of the frame). base's descriptor is preserved
 * and base is never mutated. */
ZEND_API zend_vec *zend_set_with(
		const zend_vec *base, zval *value, zend_set_with_status *status)
{
	ZEND_ASSERT(base->type->kind == ZEND_COLLECTION_TYPE_SET);

	ZVAL_DEREF(value);
	/* Validate before any membership work: a wrong-typed value can never be a
	 * member, but it is still a TypeError, not a silent no-op. */
	if (!collection_member_matches(base->type, 0, value)) {
		*status = ZEND_SET_WITH_BAD_VALUE;
		return NULL;
	}

	/* Already a member: no-op. Return the receiver as an owned reference and
	 * allocate nothing. */
	if (zend_set_contains(base, value)) {
		GC_ADDREF((zend_vec *) base);
		*status = ZEND_SET_WITH_UNCHANGED;
		return (zend_vec *) base;
	}

	/* Absent: the new set is the existing members in order plus `value` at the end. */
	zend_vec *out = zend_vec_alloc(base->count + 1, base->type);
	for (uint32_t i = 0; i < base->count; i++) {
		ZVAL_COPY(&out->elements[i], &base->elements[i]);
	}
	ZVAL_COPY(&out->elements[base->count], value);
	out->count = base->count + 1;
	*status = ZEND_SET_WITH_CHANGED;
	return out;
}

/* Build the set that is `base` with `value` removed. Like with(), nothing is
 * allocated until a change is known: the value is type-checked, then a single scan
 * with the same strict-identity predicate locates the first (and, since sets are
 * unique, only) matching member. An absent value is a no-op that returns `base` as
 * an owned reference; a present value allocates one compacted set with that member
 * dropped and the order of the rest preserved. Removing the only member yields an
 * empty set (zend_vec_alloc(0, ...) is header-only) carrying the same descriptor.
 * base's descriptor is preserved and base is never mutated. */
ZEND_API zend_vec *zend_set_without(
		const zend_vec *base, zval *value, zend_set_with_status *status)
{
	ZEND_ASSERT(base->type->kind == ZEND_COLLECTION_TYPE_SET);

	ZVAL_DEREF(value);
	/* Validate first: a wrong-typed value cannot be present, but it is still a
	 * TypeError rather than a silent "absent -> no-op". */
	if (!collection_member_matches(base->type, 0, value)) {
		*status = ZEND_SET_WITH_BAD_VALUE;
		return NULL;
	}

	/* Locate the member to drop with the same predicate as zend_set_contains; a
	 * set holds at most one identical value, so the first match is the only one. */
	uint32_t at = base->count;
	for (uint32_t i = 0; i < base->count; i++) {
		if (zend_is_identical(&base->elements[i], value)) {
			at = i;
			break;
		}
	}
	if (at == base->count) {
		/* Absent: no-op. Return the receiver as an owned reference. */
		GC_ADDREF((zend_vec *) base);
		*status = ZEND_SET_WITH_UNCHANGED;
		return (zend_vec *) base;
	}

	/* Present: a new set with member `at` removed and the rest compacted in order.
	 * count - 1 may be 0 when the only member is removed. */
	zend_vec *out = zend_vec_alloc(base->count - 1, base->type);
	uint32_t w = 0;
	for (uint32_t i = 0; i < base->count; i++) {
		if (i == at) {
			continue;
		}
		ZVAL_COPY(&out->elements[w++], &base->elements[i]);
	}
	out->count = w;   /* == base->count - 1 */
	*status = ZEND_SET_WITH_CHANGED;
	return out;
}

/* The three binary set operations share a deliberate two-pass shape:
 *   pass 1 walks the operands with the strict-identity membership test to determine the
 *          result size AND whether the operation changes anything (no-op detection);
 *   pass 2 writes the result.
 * This repeats the O(count(base)*count(other)) membership work rather than recording
 * pass 1's per-element decisions in a scratch buffer. The trade is intentional: it buys
 * exactly ONE result allocation on a change and ZERO allocation on a no-op (INV-34
 * reuse), which matters more than avoiding a second linear scan for the small, immutable
 * sets these operate on. This is not a hash set and no hash-set complexity is claimed;
 * do not introduce scratch storage or redesign the representation here. */

/* The receiver returned unchanged as an owned reference: the empty-effect no-op path
 * shared by union/intersect/diff (INV-34). Raises the refcount and allocates nothing. */
static zend_vec *zend_set_binary_noop(const zend_vec *base)
{
	GC_ADDREF((zend_vec *) base);
	return (zend_vec *) base;
}

/* base ∪ other: base's members in base order, then other's members that are not already
 * in base, in other order. Both are sets (unique), so the appended members are pairwise
 * distinct and distinct from base -- no dedup pass is needed beyond the membership test.
 * No-op (reuse) when every member of other is already in base. */
ZEND_API zend_vec *zend_set_union(
		const zend_vec *base, const zend_vec *other)
{
	ZEND_ASSERT(base->type->kind == ZEND_COLLECTION_TYPE_SET);
	ZEND_ASSERT(base->type == other->type);

	uint32_t added = 0;
	for (uint32_t j = 0; j < other->count; j++) {
		if (!zend_set_contains(base, &other->elements[j])) {
			added++;
		}
	}
	if (added == 0) {
		return zend_set_binary_noop(base);
	}

	zend_vec *out = zend_vec_alloc(base->count + added, base->type);
	uint32_t w = 0;
	for (uint32_t i = 0; i < base->count; i++) {
		ZVAL_COPY(&out->elements[w++], &base->elements[i]);
	}
	for (uint32_t j = 0; j < other->count; j++) {
		if (!zend_set_contains(base, &other->elements[j])) {
			ZVAL_COPY(&out->elements[w++], &other->elements[j]);
		}
	}
	out->count = w;   /* == base->count + added */
	return out;
}

/* base ∩ other: base's members that are also in other, kept in base order. No-op
 * (reuse) when every base member is in other. An empty intersection is a fresh empty
 * set of the same descriptor. */
ZEND_API zend_vec *zend_set_intersect(
		const zend_vec *base, const zend_vec *other)
{
	ZEND_ASSERT(base->type->kind == ZEND_COLLECTION_TYPE_SET);
	ZEND_ASSERT(base->type == other->type);

	uint32_t kept = 0;
	for (uint32_t i = 0; i < base->count; i++) {
		if (zend_set_contains(other, &base->elements[i])) {
			kept++;
		}
	}
	if (kept == base->count) {
		return zend_set_binary_noop(base);
	}

	zend_vec *out = zend_vec_alloc(kept, base->type);   /* kept may be 0 */
	uint32_t w = 0;
	for (uint32_t i = 0; i < base->count; i++) {
		if (zend_set_contains(other, &base->elements[i])) {
			ZVAL_COPY(&out->elements[w++], &base->elements[i]);
		}
	}
	out->count = w;   /* == kept */
	return out;
}

/* base − other: base's members that are not in other, kept in base order. No-op (reuse)
 * when base and other are disjoint. Removing everything yields a fresh empty set of the
 * same descriptor. */
ZEND_API zend_vec *zend_set_diff(
		const zend_vec *base, const zend_vec *other)
{
	ZEND_ASSERT(base->type->kind == ZEND_COLLECTION_TYPE_SET);
	ZEND_ASSERT(base->type == other->type);

	uint32_t kept = 0;
	for (uint32_t i = 0; i < base->count; i++) {
		if (!zend_set_contains(other, &base->elements[i])) {
			kept++;
		}
	}
	if (kept == base->count) {
		return zend_set_binary_noop(base);
	}

	zend_vec *out = zend_vec_alloc(kept, base->type);   /* kept may be 0 */
	uint32_t w = 0;
	for (uint32_t i = 0; i < base->count; i++) {
		if (!zend_set_contains(other, &base->elements[i])) {
			ZVAL_COPY(&out->elements[w++], &base->elements[i]);
		}
	}
	out->count = w;   /* == kept */
	return out;
}

/* The single construction entry point for the VM: dispatch on the resolved
 * node's kind. Every kind that reaches here is value-constructible -- the
 * handler checks that first -- so a kind with no case is a contradiction, not a
 * runtime possibility. `values` holds the already-evaluated elements. */
ZEND_API zend_vec *zend_collection_construct(
		const HashTable *values, const zend_collection_info *type, uint32_t *failed_index)
{
	switch (type->kind) {
		case ZEND_COLLECTION_TYPE_VEC:
			return zend_vec_create(values, type, failed_index);
		case ZEND_COLLECTION_TYPE_TUPLE:
			return zend_tuple_create(values, type, failed_index);
		case ZEND_COLLECTION_TYPE_SET:
			return zend_set_create(values, type, failed_index);
		default:
			ZEND_ASSERT(0 && "construct reached for a non-constructible kind");
			return NULL;
	}
}

ZEND_API void ZEND_FASTCALL zend_vec_destroy(zend_vec *vec)
{
	zval *p = vec->elements, *end = p + vec->count;

	/* A vec is collectable, so it may be sitting in the GC root buffer. Drop it
	 * before the allocation goes away, or the collector is left holding a
	 * dangling root. Arrays do the same in zend_array_destroy(). */
	GC_REMOVE_FROM_BUFFER(vec);

	while (p != end) {
		i_zval_ptr_dtor(p);
		p++;
	}
	efree(vec);
}

ZEND_API uint32_t zend_vec_lifecycle_selftest(void)
{
	zend_type str_type = ZEND_TYPE_INIT_CODE(IS_STRING, 0, 0);
	union {
		zend_collection_type desc;
		char buf[ZEND_TYPE_COLLECTION_SIZE(1)];
	} probe;
	zend_type probe_type = ZEND_TYPE_INIT_NONE(0);
	const zend_collection_info *vec_of_string;
	zend_string *a = zend_string_init("a", 1, 0);
	zend_string *b = zend_string_init("b", 1, 0);
	zend_string *spare = zend_string_init("spare", 5, 0);
	uint32_t result = 0, installed, rc_spare;
	zend_vec *vec;
	zval tmp;

	/* Reserve four slots but install only two, so the vec spends the rest of
	 * this function in the partially-populated state. */
	/* Promote vec[string] once; the node is borrowed for the rest of this
	 * function and owned by the request tier. */
	probe.desc.kind = ZEND_COLLECTION_TYPE_VEC;
	probe.desc.num_types = 1;
	probe.desc.types[0] = str_type;
	ZEND_TYPE_SET_COLLECTION(probe_type, &probe.desc);
	vec_of_string = zend_collection_info_intern(probe_type);
	ZEND_ASSERT(vec_of_string != NULL);

	vec = zend_vec_alloc(4, vec_of_string);
	if (vec->count == 0) {
		result |= ZEND_VEC_SELFTEST_ALLOC_EMPTY;
	}

	ZVAL_STR_COPY(&tmp, a);
	zend_vec_append(vec, &tmp);
	zval_ptr_dtor(&tmp);
	ZVAL_STR_COPY(&tmp, b);
	zend_vec_append(vec, &tmp);
	zval_ptr_dtor(&tmp);
	installed = vec->count;

	/* A rejected value must leave both the count and the target slot alone.
	 * The slot is poisoned first so a stray write would be visible. */
	ZVAL_UNDEF(&vec->elements[installed]);
	ZVAL_LONG(&tmp, 42);
	if (!zend_vec_append(vec, &tmp)
	 && vec->count == installed
	 && Z_TYPE(vec->elements[installed]) == IS_UNDEF) {
		result |= ZEND_VEC_SELFTEST_FAILED_APPEND_INERT;
	}

	/* Put a live value in a slot beyond count. Destruction must not touch it,
	 * because count, not capacity, bounds what was ever initialised. */
	ZVAL_STR_COPY(&vec->elements[installed], spare);
	rc_spare = GC_REFCOUNT(spare);

	zend_vec_destroy(vec);

	if (GC_REFCOUNT(spare) == rc_spare) {
		result |= ZEND_VEC_SELFTEST_ONLY_INSTALLED;
	}
	/* Each installed element was released exactly once: back to the single
	 * reference this function still holds, neither leaked nor double-released. */
	if (GC_REFCOUNT(a) == 1 && GC_REFCOUNT(b) == 1) {
		result |= ZEND_VEC_SELFTEST_DTOR_EXACTLY_ONCE;
	}

	zend_string_release(a);
	zend_string_release(b);
	zend_string_release(spare);   /* the copy in the untouched slot */
	zend_string_release(spare);
	return result;
}
