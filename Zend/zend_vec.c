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
	/* A flat vec's capacity is a real slot count and must never reach the tag
	 * bit that marks a HYBRID root: reject the request before any allocation
	 * arithmetic, unconditionally (a capacity at or above 2^31 would be
	 * misread as the hybrid tag, which is memory corruption, not a limit). */
	if (UNEXPECTED(count > ZEND_VEC_MAX_CAPACITY)) {
		zend_error_noreturn(E_ERROR,
			"Possible integer overflow in collection allocation (%u elements)", count);
	}
	/* safe_emalloc computes count * sizeof(zval) + header with overflow
	 * checking, so a large count cannot silently wrap the allocation size.
	 * `count` here is the number of slots to reserve, i.e. the capacity; the
	 * logical count starts at 0 and grows as elements are installed. Callers
	 * that want an exact value pass the final element count (capacity == count);
	 * the append copy path passes a grown capacity for exclusive-consume spare. */
	zend_vec *vec = safe_emalloc(count, sizeof(zval), ZEND_VEC_HEADER_SIZE);

	GC_SET_REFCOUNT(vec, 1);
	vec->capacity = count;
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

/* Assemble a HYBRID root over an immutable flat `base` and a bounded
 * flat `tail`. Ownership convention -- the root takes ONE owned reference to each
 * child:
 *   base  is SHARED: addref'd here, so the caller keeps its own reference (a share
 *         never consumes the receiver). elements[0].
 *   tail  is TRANSFERRED: the caller's sole reference moves into the root (no
 *         addref), so a freshly built tail is handed straight in. elements[1].
 * Both children must be flat and carry the same borrowed descriptor as the root.
 *
 * safe_emalloc bails out on OOM rather than returning, and this is a single
 * allocation, so there is no partially built root to roll back: it either wholly
 * succeeds or unwinds through the engine bailout (which reclaims the request
 * arena). The one recoverable failure -- a value that fails the element type -- is
 * rejected by the caller BEFORE any allocation, so this primitive runs only once
 * the result is certain to be published. */
static zend_vec *zend_hybrid_alloc(zend_vec *base, zend_vec *tail)
{
	ZEND_ASSERT(!ZEND_VEC_IS_HYBRID(base) && "hybrid base must be flat (no nested hybrids)");
	ZEND_ASSERT(!ZEND_VEC_IS_HYBRID(tail) && "hybrid tail must be flat");
	ZEND_ASSERT(base->type == tail->type && "base and tail must share the descriptor");
	ZEND_ASSERT(base->count <= ZEND_VEC_CAP_MASK && "base count must fit the tagged capacity");

	zend_vec *root = safe_emalloc(2, sizeof(zval), ZEND_VEC_HEADER_SIZE);

	GC_SET_REFCOUNT(root, 1);
	GC_TYPE_INFO(root) = GC_VEC;
	root->type     = base->type;                         /* borrowed, shared with children */
	root->count    = base->count + tail->count;          /* logical total                  */
	root->capacity = ZEND_VEC_HYBRID_FLAG | base->count;  /* tag + cached base element count */

	/* elements[0] = base: share it (addref); the caller keeps its own reference. */
	ZVAL_VEC(ZEND_VEC_HYBRID_BASE(root), base);
	GC_ADDREF(base);
	/* elements[1] = tail: take over the caller's sole reference (no addref). */
	ZVAL_VEC(ZEND_VEC_HYBRID_TAIL(root), tail);

	ZEND_ASSERT(root->count == ZEND_VEC_HYBRID_BASE_COUNT(root) + tail->count);
	return root;
}

/* Defined below; the flat update primitives use it for their HYBRID fallback. */
static zend_vec *zend_hybrid_flatten(const zend_vec *h);

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
/* Growth for the append copy path when an exclusive value has outgrown its
 * spare capacity (a transient chain continuation): give headroom so subsequent
 * exclusive appends consume instead of reallocating. Small values get +4, larger
 * +50%. Overflow-safe (never returns < final). A copy off a *retained* base
 * (exclusive == false) allocates exact -- no waste on single updates. */
static zend_always_inline uint32_t zend_vec_grow_capacity(uint32_t final)
{
	uint32_t extra = final < 8 ? 4u : (final >> 1);
	/* Never cross the representation-tag boundary: a flat capacity at or above
	 * 2^31 would read as a HYBRID tag. Growth degrades to an exact allocation
	 * near the limit; zend_vec_alloc() hard-errors past it. */
	if (UNEXPECTED(extra > ZEND_VEC_MAX_CAPACITY - final)) {
		return final;
	}
	return final + extra;
}

ZEND_API zend_vec *zend_vec_create_with(zend_vec *base, zval *value, bool prepend, bool exclusive)
{
	ZEND_ASSERT(base->type->kind == ZEND_COLLECTION_TYPE_VEC);
	/* append routes hybrids through zend_vec_append_value; prepend has no hybrid
	 * form and reaches here directly. Fall back: flatten the hybrid to a fresh
	 * flat value and run the flat primitive on that (never consuming the temp, so it
	 * is always freed; NULL on a type error leaves nothing allocated). A native
	 * tail form for the update ops is deliberately deferred. */
	if (UNEXPECTED(ZEND_VEC_IS_HYBRID(base))) {
		zend_vec *flat = zend_hybrid_flatten(base);
		zend_vec *out  = zend_vec_create_with(flat, value, prepend, /* exclusive */ false);
		zend_vec_destroy(flat);
		return out;
	}
	ZVAL_DEREF(value);
	if (!collection_member_matches(base->type, 0, value)) {
		return NULL;
	}

	/* Exclusive-consume fast path (append only). The caller proved the receiver
	 * is observed by no other zval -- GC_REFCOUNT == 1, established by the frame
	 * addref + FREE_OP1 of a consumed TMP/VAR (frame addref plus FREE_OP1 of a consumed TMP/VAR).
	 * With a spare slot, install the value and publish by raising count: no
	 * allocation, no copy. Returns `base` itself so the handler transfers base's
	 * sole reference to the result. The value was validated above, so a type
	 * error never mutates the receiver. prepend has no tail slot, so it copies. */
	if (exclusive && base->count < ZEND_VEC_CAPACITY(base)) {
		if (prepend) {
			/* Shift the initialized prefix up by one into the spare tail slot, then
			 * place the value at index 0. memmove *moves* each zval (refcount
			 * preserved, no addref); no allocation. O(n) move, not O(1), but avoids
			 * the copy allocation -- benchmarked separately, no asymptotic claim. */
			memmove(&base->elements[1], &base->elements[0], base->count * sizeof(zval));
			ZVAL_COPY(&base->elements[0], value);         /* addref into slot 0        */
		} else {
			ZVAL_COPY(&base->elements[base->count], value); /* addref into spare slot  */
		}
		base->count++;                                    /* publish after the write   */
		return base;
	}

	uint32_t final = base->count + 1;
	uint32_t cap = exclusive ? zend_vec_grow_capacity(final) : final;
	zend_vec *out = zend_vec_alloc(cap, base->type);
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
	/* Publish all live slots at once (<= capacity); destroy() reads only [0,count). */
	out->count = at;
	return out;
}

/* Materialize a hybrid's logical elements -- the base elements in
 * order, then the tail elements -- into a fresh exact-size FLAT vec (FLAT_EXACT).
 * The children are never mutated; each element is copied (addref'd). Used as the
 * flatten primitive (Commits 4/7) and the interim hybrid-append fallback. */
static zend_vec *zend_hybrid_flatten(const zend_vec *h)
{
	ZEND_ASSERT(ZEND_VEC_IS_HYBRID(h));
	zend_vec *base = ZEND_VEC_HYBRID_BASE_VEC(h);
	zend_vec *tail = ZEND_VEC_HYBRID_TAIL_VEC(h);
	zend_vec *out  = zend_vec_alloc(h->count, h->type);   /* exact size */
	uint32_t at = 0;

	for (uint32_t i = 0; i < base->count; i++) {
		ZVAL_COPY(&out->elements[at++], &base->elements[i]);
	}
	for (uint32_t i = 0; i < tail->count; i++) {
		ZVAL_COPY(&out->elements[at++], &tail->elements[i]);
	}
	ZEND_ASSERT(at == h->count);
	out->count = at;
	return out;
}

/* Retained-append transition. `base` is a FLAT
 * vec observed elsewhere (not exclusively owned), so instead of copying base + 1
 * we SHARE base and place the new value in a fresh one-element tail -- a HYBRID
 * root. `value` is validated FIRST; on failure nothing is allocated and NULL is
 * returned. base is neither mutated nor copied. */
static zend_vec *zend_flat_append_to_hybrid(zend_vec *base, zval *value)
{
	ZEND_ASSERT(!ZEND_VEC_IS_HYBRID(base));
	/* Policy degenerate case (== zend_hybrid_should_flatten(1, base->count) at
	 * the frozen R = 1.0; keep in sync with the knobs below): a one-element
	 * tail over an EMPTY base would publish tail > R*base and shares nothing.
	 * Publish a FLAT copy instead -- the copy cost is the appended element
	 * alone, so no published hybrid ever crosses a flatten trigger. */
	if (UNEXPECTED(base->count == 0)) {
		return zend_vec_create_with(base, value, /* prepend */ false, /* exclusive */ false);
	}
	ZVAL_DEREF(value);
	if (!collection_member_matches(base->type, 0, value)) {
		return NULL;   /* type failure: nothing allocated, caller raises TypeError */
	}

	/* Fresh one-element tail (rc 1); its sole reference is consumed by the root. */
	zend_vec *tail = zend_vec_alloc(1, base->type);
	ZVAL_COPY(&tail->elements[0], value);   /* addref the value into the tail slot */
	tail->count = 1;

	/* Assemble the root: shares base (addref, base kept by the caller too) and
	 * takes the tail's sole reference. */
	return zend_hybrid_alloc(base, tail);
}

/* Flatten policy knobs. Two INDEPENDENT triggers bound the tail so a hybrid
 * always stays "small tail over a large shared base":
 *   T  -- the absolute maximum tail count. Caps read/GC/tail-copy cost and MM-bin
 *         footprint for a large base (where the ratio never fires).
 *   R  -- the tail/base ratio, as NUM/DEN. A tail that grows past R*base no longer
 *         benefits from base-sharing, so it flattens; this only ever LOWERS the
 *         bound below T, and only for small bases (for base >= T/R, T dominates).
 * T = 127 with R = 1.0 is the VALIDATED policy (storage benchmark, 2026-08-02:
 * retained append O(1); 116 B marginal retained memory per live branch at 1000
 * branches from a 100k base; CV-accumulation log-log slope 1.85 -> 1.20; flat
 * and hybrid read/foreach within 11% of the flat baseline). */
#define ZEND_HYBRID_TAIL_MAX 127u
#define ZEND_HYBRID_RATIO_NUM 1u
#define ZEND_HYBRID_RATIO_DEN 1u

/* True when publishing a tail of `new_tail` over a base of `base_count` would
 * cross either flatten trigger. 64-bit products avoid overflow at the T/ratio
 * scales. */
static zend_always_inline bool zend_hybrid_should_flatten(uint32_t new_tail, uint32_t base_count)
{
	return new_tail > ZEND_HYBRID_TAIL_MAX
	    || (uint64_t) new_tail * ZEND_HYBRID_RATIO_DEN
	         > (uint64_t) base_count * ZEND_HYBRID_RATIO_NUM;
}

/* Build a fresh tail = the old tail's elements followed by `value`, with spare
 * capacity for subsequent in-place appends. The old tail is not modified; `value`
 * is already validated and dereferenced. */
static zend_vec *zend_hybrid_grow_tail(
		const zend_vec *tail, zval *value, const zend_collection_info *type)
{
	uint32_t n = tail->count;
	zend_vec *nt = zend_vec_alloc(zend_vec_grow_capacity(n + 1), type);

	for (uint32_t i = 0; i < n; i++) {
		ZVAL_COPY(&nt->elements[i], &tail->elements[i]);
	}
	ZVAL_COPY(&nt->elements[n], value);
	nt->count = n + 1;
	return nt;
}

/* Hybrid append ownership matrix. `value` is already
 * validated and dereferenced. `root_exclusive` is the centralized frame-ownership
 * verdict for the root; tail exclusivity is checked INDEPENDENTLY via its own
 * refcount, never inferred from the root. Ownership transfer is annotated per arm. */
static zend_vec *zend_hybrid_append(zend_vec *root, zval *value, bool root_exclusive)
{
	zend_vec *base = ZEND_VEC_HYBRID_BASE_VEC(root);
	zend_vec *tail = ZEND_VEC_HYBRID_TAIL_VEC(root);

	/* Flatten before the tail would cross a trigger: produce a fresh flat value
	 * (base + tail + value). Never publishes a tail past T or R*base; the shared
	 * root/base are untouched, so this is a NEW value, never an in-place mutation of
	 * a shared one (a read-only op never reaches here). */
	if (UNEXPECTED(zend_hybrid_should_flatten(tail->count + 1, ZEND_VEC_HYBRID_BASE_COUNT(root)))) {
		zend_vec *flat = zend_hybrid_flatten(root);                        /* [base..,tail..] rc1 */
		zend_vec *out  = zend_vec_create_with(flat, value, false, true);   /* + value             */
		if (out != flat) {
			zend_vec_destroy(flat);
		}
		return out;
	}

	bool tail_exclusive = (GC_REFCOUNT(tail) == 1);
	bool tail_spare     = (tail->count < ZEND_VEC_CAPACITY(tail));

	if (root_exclusive) {
		if (tail_exclusive && tail_spare) {
			/* (1) Every mutable component is exclusively owned and the tail has a
			 * spare slot: install the value and publish by raising both counts.
			 * No allocation. The root's sole frame reference transfers to the result. */
			ZVAL_COPY(&tail->elements[tail->count], value);
			tail->count++;
			root->count++;
			return root;
		}
		/* (2) tail full, or (3) tail unexpectedly shared: the root is ours but the
		 * tail cannot be mutated, so install a fresh grown tail and release the old
		 * tail reference exactly once. base (and the cached base_count) are unchanged. */
		zend_vec *nt = zend_hybrid_grow_tail(tail, value, root->type);
		i_zval_ptr_dtor(ZEND_VEC_HYBRID_TAIL(root));   /* drop the old tail ref (free if last) */
		ZVAL_VEC(ZEND_VEC_HYBRID_TAIL(root), nt);       /* take the new tail's sole reference   */
		root->count++;
		return root;
	}

	/* (4) The root is shared: leave it wholly untouched and build a NEW root that
	 * shares the immutable base (addref, no copy) and owns an independent new tail. */
	{
		zend_vec *nt = zend_hybrid_grow_tail(tail, value, root->type);
		return zend_hybrid_alloc(base, nt);   /* addrefs base, consumes nt */
	}
}

ZEND_API zend_vec *zend_vec_append_value(zend_vec *base, zval *value, bool exclusive)
{
	ZEND_ASSERT(base->type->kind == ZEND_COLLECTION_TYPE_VEC);

	if (!ZEND_VEC_IS_HYBRID(base)) {
		if (exclusive) {
			/* Flat consumable temporary: mutate/grow in place. */
			return zend_vec_create_with(base, value, /* prepend */ false, /* exclusive */ true);
		}
		/* Retained/shared flat append: share base, one-element tail -> HYBRID. On a
		 * target without hybrid support the tag can never be set, so fall back to a
		 * plain flat copy (correct, only without the base-sharing win). The predicate
		 * is a compile-time constant, so exactly one arm is kept. */
		if (ZEND_VEC_HYBRID_SUPPORTED) {
			return zend_flat_append_to_hybrid(base, value);
		}
		return zend_vec_create_with(base, value, /* prepend */ false, /* exclusive */ false);
	}

	/* HYBRID receiver: validate the value, then run the root/tail exclusivity
	 * matrix. base->type is the shared descriptor for root/base/tail. */
	ZVAL_DEREF(value);
	if (!collection_member_matches(base->type, 0, value)) {
		return NULL;
	}
	return zend_hybrid_append(base, value, exclusive);
}

/* Build a new vec equal to `base` with the element at `index` replaced by
 * `value`. base's descriptor is preserved exactly (borrowed), base is never
 * mutated, and only `value` is validated -- base's other elements are already
 * valid for the type and are copied without re-checking. Both failure modes are
 * detected before anything is allocated: an out-of-range index (BAD_INDEX) and a
 * value that fails the element type (BAD_VALUE). */
ZEND_API zend_vec *zend_vec_with_at(
		zend_vec *base, zend_long index, zval *value, zend_vec_with_status *status, bool exclusive)
{
	ZEND_ASSERT(base->type->kind == ZEND_COLLECTION_TYPE_VEC);
	/* C1 fallback: a HYBRID base is flattened, then withAt runs on the flat value
	 * (the flatten policy and any native tail-region form come separately). base->count
	 * is the logical count for both representations, so the range check below is
	 * still correct pre-flatten. */
	if (UNEXPECTED(ZEND_VEC_IS_HYBRID(base))) {
		zend_vec *flat = zend_hybrid_flatten(base);
		zend_vec *out  = zend_vec_with_at(flat, index, value, status, /* exclusive */ false);
		zend_vec_destroy(flat);
		return out;
	}
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

	/* Exclusive-consume: replace one slot in place, O(1), no allocation. Both the
	 * index and the value are validated ABOVE, so the receiver is never mutated on
	 * a failure. The replaced element is destroyed exactly once; the receiver's
	 * sole reference is transferred to the result by the handler. */
	if (exclusive) {
		i_zval_ptr_dtor(&base->elements[at]);   /* destroy the old value once */
		ZVAL_COPY(&base->elements[at], value);  /* install the new value      */
		*status = ZEND_VEC_WITH_OK;
		return base;
	}

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
		zend_vec *base, zend_long index, zend_vec_with_status *status, bool exclusive)
{
	ZEND_ASSERT(base->type->kind == ZEND_COLLECTION_TYPE_VEC);
	/* Fallback: flatten a HYBRID base, then remove on the flat value. A native
	 * tail-region form for removal is deliberately deferred. */
	if (UNEXPECTED(ZEND_VEC_IS_HYBRID(base))) {
		zend_vec *flat = zend_hybrid_flatten(base);
		zend_vec *out  = zend_vec_without_at(flat, index, status, /* exclusive */ false);
		zend_vec_destroy(flat);
		return out;
	}
	if (index < 0 || index >= (zend_long) base->count) {
		*status = ZEND_VEC_WITH_BAD_INDEX;
		return NULL;
	}

	uint32_t skip = (uint32_t) index;

	/* Exclusive-consume: remove the slot in place. Destroy the removed value once,
	 * shift the suffix down (memmove *moves* each zval, refcount preserved), clear
	 * the now-duplicate final slot so it is never scanned, and lower count. O(n)
	 * move, no allocation. There is no value to validate, so nothing can fail after
	 * the destroy. */
	if (exclusive) {
		i_zval_ptr_dtor(&base->elements[skip]);
		uint32_t tail = base->count - 1u - skip;    /* elements after the removed one */
		if (tail) {
			memmove(&base->elements[skip], &base->elements[skip + 1], tail * sizeof(zval));
		}
		ZVAL_UNDEF(&base->elements[base->count - 1]);   /* clear the vacated tail slot */
		base->count--;
		*status = ZEND_VEC_WITH_OK;
		return base;
	}

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
		/* Sets are never hybrid (only vec append produces one), but iterate by
		 * logical position so this stays representation-independent regardless. */
		for (uint32_t i = 0; i < a->count; i++) {
			if (!zend_set_contains(b, zend_stor_iter(a, i))) {
				return false;
			}
		}
		return true;
	}

	/* vec and tuple: both are positional and share this loop. Iterate by logical
	 * position so a flat value and a hybrid value -- or two hybrids split
	 * differently between base and tail -- with the same elements compare identical,
	 * without flattening either side. */
	for (uint32_t i = 0; i < a->count; i++) {
		if (!zend_is_identical(zend_stor_iter(a, i), zend_stor_iter(b, i))) {
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
	/* A vec is collectable, so it may be sitting in the GC root buffer. Drop it
	 * before the allocation goes away, or the collector is left holding a
	 * dangling root. Arrays do the same in zend_array_destroy(). */
	GC_REMOVE_FROM_BUFFER(vec);

	if (UNEXPECTED(ZEND_VEC_IS_HYBRID(vec))) {
		/* A HYBRID root owns exactly two child references -- the shared
		 * flat base and the bounded flat tail, overlaid on elements[0..1]. Release
		 * each once; i_zval_ptr_dtor recursively destroys a child only when its own
		 * refcount reaches zero, so a base still shared by another branch survives.
		 * The element payloads belong to the children and are never walked here. */
		i_zval_ptr_dtor(ZEND_VEC_HYBRID_BASE(vec));
		i_zval_ptr_dtor(ZEND_VEC_HYBRID_TAIL(vec));
		efree(vec);
		return;
	}

	zval *p = vec->elements, *end = p + vec->count;
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

/* Exercise HYBRID ownership entirely in C, so the invariants hold before any PHP
 * surface builds a hybrid yet. Two branches share one flat base; refcounts prove
 * the base is shared (never copied), each child is released exactly once, and
 * branches have independent lifetimes. */
ZEND_API uint32_t zend_hybrid_lifecycle_selftest(void)
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
	zend_string *c = zend_string_init("c", 1, 0);
	zend_string *d = zend_string_init("d", 1, 0);
	zend_vec *base, *tail1, *tail2, *h1, *h2;
	uint32_t result = 0, base_rc0;
	zval tmp;

	probe.desc.kind = ZEND_COLLECTION_TYPE_VEC;
	probe.desc.num_types = 1;
	probe.desc.types[0] = str_type;
	ZEND_TYPE_SET_COLLECTION(probe_type, &probe.desc);
	vec_of_string = zend_collection_info_intern(probe_type);
	ZEND_ASSERT(vec_of_string != NULL);

	/* Immutable flat base ["a","b"]; base_rc0 is our sole reference. */
	base = zend_vec_alloc(2, vec_of_string);
	ZVAL_STR_COPY(&tmp, a); zend_vec_append(base, &tmp); zval_ptr_dtor(&tmp);
	ZVAL_STR_COPY(&tmp, b); zend_vec_append(base, &tmp); zval_ptr_dtor(&tmp);
	base_rc0 = GC_REFCOUNT(base);

	/* Fresh tail ["c"] (rc 1); its sole reference is consumed by the hybrid. */
	tail1 = zend_vec_alloc(1, vec_of_string);
	ZVAL_STR_COPY(&tmp, c); zend_vec_append(tail1, &tmp); zval_ptr_dtor(&tmp);
	h1 = zend_hybrid_alloc(base, tail1);

	/* (1) repr / count / descriptor invariants, and the base/tail overlay. */
	if (ZEND_VEC_IS_HYBRID(h1)
	 && h1->count == 3
	 && ZEND_VEC_HYBRID_BASE_COUNT(h1) == base->count
	 && h1->type == base->type
	 && ZEND_VEC_HYBRID_BASE_VEC(h1) == base
	 && ZEND_VEC_HYBRID_TAIL_VEC(h1) == tail1) {
		result |= ZEND_HYBRID_SELFTEST_TAGGED_HYBRID;
	}
	/* (2) base is shared: creating the root raised its refcount by exactly one. */
	if (GC_REFCOUNT(base) == base_rc0 + 1) {
		result |= ZEND_HYBRID_SELFTEST_BASE_SHARED;
	}
	/* (3) tail is owned: the root took our sole tail reference (still rc 1). */
	if (GC_REFCOUNT(tail1) == 1) {
		result |= ZEND_HYBRID_SELFTEST_TAIL_OWNED;
	}

	/* GC-children exposure: the span contract yields exactly the two
	 * child collection zvals {base, tail}; the debug asserts inside zend_stor_span_get
	 * fire here on a real hybrid. */
	{
		zend_stor_span sp = zend_stor_span_get(h1, 0);
		ZEND_ASSERT(zend_stor_span_count(h1) == 1);
		ZEND_ASSERT(sp.n == 2);
		ZEND_ASSERT(sp.base == ZEND_VEC_HYBRID_BASE(h1));
		ZEND_ASSERT(Z_VEC(sp.base[0]) == base);
		ZEND_ASSERT(Z_VEC(sp.base[1]) == tail1);
	}

	/* A second branch over the same base -- base now shared three ways. */
	tail2 = zend_vec_alloc(1, vec_of_string);
	ZVAL_STR_COPY(&tmp, d); zend_vec_append(tail2, &tmp); zval_ptr_dtor(&tmp);
	h2 = zend_hybrid_alloc(base, tail2);

	/* (4) destroy one branch; the base and the other branch survive independently,
	 * and the destroyed branch's tail element was released exactly once. */
	zend_vec_destroy(h1);
	if (GC_REFCOUNT(base) == base_rc0 + 1
	 && GC_REFCOUNT(c) == 1
	 && ZEND_VEC_IS_HYBRID(h2)
	 && h2->count == 3
	 && ZEND_VEC_HYBRID_BASE_VEC(h2) == base
	 && Z_TYPE(base->elements[0]) == IS_STRING) {
		result |= ZEND_HYBRID_SELFTEST_BRANCH_INDEP;
	}

	/* (5) destroy the final branch; base refcount is restored and tail2 freed once. */
	zend_vec_destroy(h2);
	if (GC_REFCOUNT(base) == base_rc0 && GC_REFCOUNT(d) == 1) {
		result |= ZEND_HYBRID_SELFTEST_DTOR_BALANCED;
	}

	zend_vec_destroy(base);
	/* Every base element was released exactly once, back to our held reference. */
	ZEND_ASSERT(GC_REFCOUNT(a) == 1 && GC_REFCOUNT(b) == 1);

	zend_string_release(a);
	zend_string_release(b);
	zend_string_release(c);
	zend_string_release(d);
	return result;
}

/* Prove the flatten-policy invariants at the dispatcher boundary, entirely in
 * C: no PHP surface can observe a value's representation, so the policy's
 * published-representation guarantees keep direct coverage here. */
ZEND_API uint32_t zend_hybrid_policy_selftest(void)
{
	zend_type str_type = ZEND_TYPE_INIT_CODE(IS_STRING, 0, 0);
	union {
		zend_collection_type desc;
		char buf[ZEND_TYPE_COLLECTION_SIZE(1)];
	} probe;
	zend_type probe_type = ZEND_TYPE_INIT_NONE(0);
	const zend_collection_info *vec_of_string;
	zend_string *a = zend_string_init("a", 1, 0);
	zend_string *v = zend_string_init("v", 1, 0);
	zend_vec *base, *out;
	uint32_t result = 0;
	zval tmp;

	probe.desc.kind = ZEND_COLLECTION_TYPE_VEC;
	probe.desc.num_types = 1;
	probe.desc.types[0] = str_type;
	ZEND_TYPE_SET_COLLECTION(probe_type, &probe.desc);
	vec_of_string = zend_collection_info_intern(probe_type);
	ZEND_ASSERT(vec_of_string != NULL);

	/* A retained (non-exclusive) append to an EMPTY vec must publish a FLAT
	 * value: a hybrid over base_count == 0 would violate the R bound and
	 * shares nothing. The receiver stays untouched. */
	base = zend_vec_alloc(0, vec_of_string);
	ZVAL_STR(&tmp, v);   /* borrowed; the append copies */
	out = zend_vec_append_value(base, &tmp, /* exclusive */ false);
	if (out != NULL && out != base
	 && !ZEND_VEC_IS_HYBRID(out)
	 && out->count == 1
	 && base->count == 0) {
		result |= ZEND_HYBRID_POLICY_SELFTEST_EMPTY_BASE_FLAT;
	}
	if (out != NULL && out != base) {
		zend_vec_destroy(out);
	}
	zend_vec_destroy(base);

	/* Positive control: the same retained append to a NON-empty vec publishes
	 * a HYBRID sharing the receiver as base, with a one-element tail (both
	 * policy bounds hold at tail == 1 <= base_count). */
	base = zend_vec_alloc(1, vec_of_string);
	ZVAL_STR_COPY(&tmp, a);
	zend_vec_append(base, &tmp);
	zval_ptr_dtor(&tmp);
	ZVAL_STR(&tmp, v);
	out = zend_vec_append_value(base, &tmp, /* exclusive */ false);
	if (out != NULL
	 && ZEND_VEC_IS_HYBRID(out)
	 && ZEND_VEC_HYBRID_BASE_VEC(out) == base
	 && ZEND_VEC_HYBRID_TAIL_VEC(out)->count == 1
	 && out->count == 2) {
		result |= ZEND_HYBRID_POLICY_SELFTEST_RETAINED_HYBRID;
	}
	if (out != NULL) {
		zend_vec_destroy(out);
	}
	zend_vec_destroy(base);

	zend_string_release(a);
	zend_string_release(v);
	return result;
}
