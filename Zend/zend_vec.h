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
