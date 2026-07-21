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

ZEND_API bool zend_vec_type_is_supported(zend_type type)
{
	/* Unions and intersections have no single element representation, and
	 * literal names and arena-backed types cannot be owned by a value. */
	if (ZEND_TYPE_IS_TYPE_LIST(type)
	 || ZEND_TYPE_HAS_LITERAL_NAME(type)
	 || ZEND_TYPE_USES_ARENA(type)) {
		return false;
	}

	/* A nested collection, e.g. the inner vec[int] of vec[vec[int]]. Supported
	 * so that runtime values never lag the types the compiler accepts. */
	if (ZEND_TYPE_HAS_COLLECTION_DESCRIPTOR(type)) {
		const zend_collection_type *desc = ZEND_TYPE_COLLECTION(type);

		if (desc->kind != ZEND_COLLECTION_TYPE_VEC || desc->num_types != 1) {
			return false;
		}
		if ((ZEND_TYPE_FULL_MASK(type) & _ZEND_TYPE_MAY_BE_MASK) != 0) {
			return false;
		}
		return zend_vec_type_is_supported(desc->types[0]);
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

ZEND_API void zend_vec_type_copy(zend_type *dst, zend_type src)
{
	ZEND_ASSERT(zend_vec_type_is_supported(src));

	if (ZEND_TYPE_HAS_COLLECTION_DESCRIPTOR(src)) {
		/* Deep-copy the descriptor so the value owns its element type outright.
		 * A compiler-owned descriptor lives in the arena and would otherwise
		 * dangle, and a caller-built one would be freed underneath us. */
		const zend_collection_type *sd = ZEND_TYPE_COLLECTION(src);
		zend_collection_type *dd = zend_type_collection_alloc(
			sd->kind, sd->num_types, /* persistent */ false);
		zend_type copy = ZEND_TYPE_INIT_NONE(0);

		for (uint32_t i = 0; i < sd->num_types; i++) {
			zend_vec_type_copy(&dd->types[i], sd->types[i]);
		}
		ZEND_TYPE_SET_COLLECTION(copy, dd);
		*dst = copy;
		return;
	}

	if (ZEND_TYPE_HAS_NAME(src)) {
		zend_string_addref(ZEND_TYPE_NAME(src));
	}
	*dst = src;
}

ZEND_API void zend_vec_type_dtor(zend_type type)
{
	if (ZEND_TYPE_HAS_COLLECTION_DESCRIPTOR(type)) {
		zend_collection_type *desc = ZEND_TYPE_COLLECTION(type);

		for (uint32_t i = 0; i < desc->num_types; i++) {
			zend_vec_type_dtor(desc->types[i]);
		}
		pefree(desc, /* persistent */ false);
		return;
	}

	if (ZEND_TYPE_HAS_NAME(type)) {
		zend_string_release(ZEND_TYPE_NAME(type));
	}
}

ZEND_API zend_vec *zend_vec_alloc(uint32_t count, zend_type element_type)
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
	zend_vec_type_copy(&vec->element_type, element_type);

	return vec;
}

/* Does `value` satisfy the declared element type? Shallow: a matching element
 * may itself be a mutable array or object, or a nested collection value. */
static bool zend_vec_element_matches(zend_type element_type, zval *value)
{
	if (ZEND_TYPE_HAS_COLLECTION_DESCRIPTOR(element_type)) {
		const zend_collection_type *desc = ZEND_TYPE_COLLECTION(element_type);

		if (Z_TYPE_P(value) != IS_COLLECTION) {
			return false;
		}
		ZEND_ASSERT(GC_TYPE(Z_COUNTED_P(value)) == IS_VEC_GC);
		if (desc->kind != ZEND_COLLECTION_TYPE_VEC || desc->num_types != 1) {
			return false;
		}
		return zend_type_structurally_equals(
			Z_VEC_P(value)->element_type, desc->types[0]);
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

ZEND_API bool zend_vec_append(zend_vec *vec, zval *value)
{
	ZVAL_DEREF(value);

	if (!zend_vec_element_matches(vec->element_type, value)) {
		return false;
	}
	/* Install first, then publish the slot by raising count. */
	ZVAL_COPY(&vec->elements[vec->count], value);
	vec->count++;
	return true;
}

ZEND_API zend_vec *zend_vec_create(const HashTable *values, zend_type element_type)
{
	zend_vec *vec = zend_vec_alloc(zend_hash_num_elements(values), element_type);
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
	zend_vec_type_dtor(vec->element_type);
	efree(vec);
}
