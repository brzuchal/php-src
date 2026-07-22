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

	/* A nested collection, e.g. the inner vec[int] of vec[vec[int]]. Supported
	 * so that runtime values never lag the types the compiler accepts.
	 *
	 * The pointer here is a canonical zend_collection_info, not a compiler
	 * zend_collection_type: this function inspects members of an already
	 * promoted node, and promotion replaces nested descriptors with child
	 * nodes. The two structs share kind and num_types but diverge after them,
	 * so reading this as a descriptor would walk the wrong offsets. */
	if (ZEND_TYPE_HAS_COLLECTION_DESCRIPTOR(type)) {
		const zend_collection_info *child =
			(const zend_collection_info *) ZEND_TYPE_COLLECTION(type);

		if (child->kind != ZEND_COLLECTION_TYPE_VEC || child->num_types != 1) {
			return false;
		}
		if ((ZEND_TYPE_FULL_MASK(type) & _ZEND_TYPE_MAY_BE_MASK) != 0) {
			return false;
		}
		return zend_vec_type_is_supported(child->types[0]);
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

/* Does `value` satisfy the declared element type? Shallow: a matching element
 * may itself be a mutable array or object, or a nested collection value. */
static bool zend_vec_element_matches(zend_type element_type, zval *value)
{
	if (ZEND_TYPE_HAS_COLLECTION_DESCRIPTOR(element_type)) {
		/* Both sides are canonical, so a nested collection element check is a
		 * pointer comparison rather than a structural walk. */
		if (Z_TYPE_P(value) != IS_COLLECTION) {
			return false;
		}
		return Z_VEC_P(value)->type
			== (const zend_collection_info *) ZEND_TYPE_COLLECTION(element_type);
	}

	if (ZEND_TYPE_HAS_NAME(element_type)) {
		zend_class_entry *ce;

		if (Z_TYPE_P(value) != IS_OBJECT) {
			return false;
		}
		ce = zend_lookup_class(ZEND_TYPE_NAME(element_type));
		return ce != NULL && instanceof_function(Z_OBJCE_P(value), ce);
	}

	return ZEND_TYPE_CONTAINS_CODE(element_type, Z_TYPE_P(value));
}

static bool zend_vec_append(zend_vec *vec, zval *value)
{
	ZVAL_DEREF(value);

	if (!zend_vec_element_matches(ZEND_VEC_ELEMENT_TYPE(vec), value)) {
		return false;
	}
	/* Install first, then publish the slot by raising count. */
	ZVAL_COPY(&vec->elements[vec->count], value);
	vec->count++;
	return true;
}

ZEND_API zend_vec *zend_vec_create(const HashTable *values, const zend_collection_info *type)
{
	ZEND_ASSERT(type != NULL && type->num_types >= 1);
	ZEND_ASSERT(zend_vec_type_is_supported(type->types[0]));

	zend_vec *vec = zend_vec_alloc(zend_hash_num_elements(values), type);
	zval *entry;

	ZEND_HASH_FOREACH_VAL((HashTable *) values, entry) {
		if (!zend_vec_append(vec, entry)) {
			/* Only the slots already installed are live, so this releases
			 * exactly those and the element type, and nothing else. */
			zend_vec_destroy(vec);
			return NULL;
		}
	} ZEND_HASH_FOREACH_END();

	return vec;
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
