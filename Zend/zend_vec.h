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

BEGIN_EXTERN_C()

/* An immutable typed sequence. Elements are stored contiguously in the same
 * allocation as the header, so a vec is one allocation and is never resized.
 *
 * This is deliberately not a zend_array: generic array helpers allocate and
 * persist exactly sizeof(zend_array), so a vec must not be reachable through
 * them. Nothing here may be passed to zend_array_dup(), SEPARATE_ARRAY() or
 * the array persistence paths.
 *
 * The declared element type is an embedded zend_type, restricted to the subset
 * a runtime value can own safely (see zend_vec_type_is_supported): a pure
 * builtin mask, or a single refcounted class-name zend_string. Type lists,
 * literal names and arena-backed types are not supported yet. */
typedef struct _zend_vec {
	zend_refcounted_h gc;
	zend_type         element_type;
	uint32_t          count;
	zval              elements[1];
} zend_vec;

/* offsetof is the only layout contract; do not assume a fixed header size. */
#define ZEND_VEC_HEADER_SIZE     offsetof(zend_vec, elements)

#define Z_VEC(zval)              ((zend_vec *) Z_COUNTED(zval))
#define Z_VEC_P(zval_p)          Z_VEC(*(zval_p))
#define ZEND_VEC_COUNT(vec)      ((vec)->count)
#define ZEND_VEC_ELEMENTS(vec)   ((vec)->elements)

/* The single definition of the element-type subset a vec may own. True for a
 * pure builtin mask, or a single class-name zend_string with no additional
 * may-be bits (i.e. no union, no nullable). False for everything else,
 * including type lists, literal names and arena-backed types. A HAS_NAME type
 * carries a refcounted class name only; it is not a resolved zend_class_entry,
 * and class lookup / autoload / instanceof are future concerns. */
ZEND_API bool zend_vec_type_is_supported(zend_type type);

/* Copy a supported element type into *dst by value, taking ownership of a
 * class-name zend_string via addref. Asserts the type is supported. */
ZEND_API void zend_vec_type_copy(zend_type *dst, zend_type src);

/* Drop ownership of a supported element type: release a class-name
 * zend_string. A no-op for pure masks. */
ZEND_API void zend_vec_type_dtor(zend_type type);

/* Allocate an uninitialised vec of `count` elements with the given element
 * type. The type must be supported; the vec takes its own reference to any
 * class-name metadata. Every element must be filled before the value becomes
 * reachable; the elements are uninitialised memory on return, not IS_UNDEF. */
ZEND_API zend_vec *zend_vec_alloc(uint32_t count, zend_type element_type);

/* Release the element type, the element zvals, and the allocation. Reached
 * through rc_dtor_func() when the refcount drops to zero. */
ZEND_API void ZEND_FASTCALL zend_vec_destroy(zend_vec *vec);

END_EXTERN_C()

#endif /* ZEND_VEC_H */
