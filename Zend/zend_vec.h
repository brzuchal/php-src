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

/* Build a vec from a packed list of values. This is the only construction
 * entry point: reserving storage and installing elements are private to
 * zend_vec.c, so the capacity invariant they share cannot be violated from
 * outside. On any element failing validation the partially built vec is
 * destroyed, touching only the slots already installed, and NULL is returned. */
ZEND_API zend_vec *zend_vec_create(const HashTable *values, zend_type element_type);

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
