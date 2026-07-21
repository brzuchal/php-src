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

ZEND_API bool zend_vec_type_is_supported(zend_type type)
{
	/* Type lists (unions/intersections), literal names and arena-backed types
	 * cannot be owned safely by a value and are out of scope for now. */
	if (ZEND_TYPE_HAS_LIST(type)
	 || ZEND_TYPE_HAS_LITERAL_NAME(type)
	 || ZEND_TYPE_USES_ARENA(type)) {
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

ZEND_API void zend_vec_type_copy(zend_type *dst, zend_type src)
{
	ZEND_ASSERT(zend_vec_type_is_supported(src));

	if (ZEND_TYPE_HAS_NAME(src)) {
		zend_string_addref(ZEND_TYPE_NAME(src));
	}
	*dst = src;
}

ZEND_API void zend_vec_type_dtor(zend_type type)
{
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
	/* Not collectable yet: a vec can take part in a cycle through an element,
	 * but the collector has no arm for this type. Marking it uncollectable
	 * leaks such cycles rather than letting the collector misread the
	 * allocation. This is a temporary prototype limitation. */
	GC_TYPE_INFO(vec) = GC_VEC | (GC_NOT_COLLECTABLE << GC_FLAGS_SHIFT);
	vec->count = count;
	zend_vec_type_copy(&vec->element_type, element_type);

	return vec;
}

ZEND_API void ZEND_FASTCALL zend_vec_destroy(zend_vec *vec)
{
	zval *p = vec->elements, *end = p + vec->count;

	while (p != end) {
		i_zval_ptr_dtor(p);
		p++;
	}
	zend_vec_type_dtor(vec->element_type);
	efree(vec);
}
