/*
   +----------------------------------------------------------------------+
   | Zend Engine                                                          |
   +----------------------------------------------------------------------+
   | Copyright (c) Zend Technologies Ltd. (http://www.zend.com)           |
   +----------------------------------------------------------------------+
   | This source file is subject to version 2.00 of the Zend license,     |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.zend.com/license/2_00.txt.                                |
   | If you did not receive a copy of the Zend license and are unable to  |
   | obtain it through the world-wide-web, please send a note to          |
   | license@zend.com so we can mail you a copy immediately.              |
   +----------------------------------------------------------------------+
*/

#ifndef ZEND_COLLECTION_INFO_H
#define ZEND_COLLECTION_INFO_H

#include "zend_types.h"

/* Runtime element-type nodes for immutable collection values.
 *
 * A zend_collection_info is the *runtime* counterpart of the compile-time
 * zend_collection_type. The two are deliberately different C types:
 *
 *   zend_collection_type  compiler/opcache owned; arena, SHM or file-cache
 *                         backed; nested parameters stored inline.
 *   zend_collection_info  runtime owned; interned; nested parameters stored
 *                         *by reference* to other interned nodes.
 *
 * Keeping them distinct is what makes the ownership rule structural rather
 * than a convention: an arena pointer is not assignable to a field that is
 * declared to hold a zend_collection_info*, so a runtime value cannot store
 * borrowed compiler memory even by mistake.
 *
 * Ownership (see docs/first-class-collections/runtime-type-ownership.md):
 *
 *   INV-1  Every node is owned by exactly one intern tier, permanent or
 *          request. Nodes are not refcounted.
 *   INV-2  Values, run-time cache slots and nested slots hold *borrowed*
 *          pointers, valid for the whole lifetime of the owning tier.
 *   INV-3  A permanent node references only permanent entities; a request
 *          node may reference either. Never the reverse.
 *   INV-3a The permanent tier holds MINIT-time internal declarations only.
 *          Types originating in compiled or preloaded scripts are never
 *          promoted, because an opcache restart resets the SHM interned
 *          string buffer their class names would live in.
 *   INV-4  The request tier is destroyed at the end of
 *          zend_shutdown_executor_values(), after the object store. That
 *          function -- not shutdown_executor() -- is the hook point, because
 *          preload calls it directly and never calls shutdown_executor().
 *   INV-5  Constructing or destroying a value performs no refcount operation
 *          on its type node.
 */

BEGIN_EXTERN_C()

#define ZEND_COLLECTION_INFO_PERMANENT (1u << 0)

typedef struct _zend_collection_info {
	uint32_t   kind;       /* zend_collection_kind */
	uint32_t   num_types;
	uint32_t   flags;      /* ZEND_COLLECTION_INFO_* */
	uint32_t   fast_mask;  /* derived: builtin-only check, 0 when not applicable */
	zend_ulong hash;       /* derived: structural hash, cached for nesting */
	zend_type  types[1];   /* nested collection slots hold zend_collection_info* */
} zend_collection_info;

#define ZEND_COLLECTION_INFO_SIZE(num_types) \
	(sizeof(zend_collection_info) + ((num_types) - 1) * sizeof(zend_type))

#define ZEND_COLLECTION_INFO_IS_PERMANENT(info) \
	(((info)->flags & ZEND_COLLECTION_INFO_PERMANENT) != 0)

/* Promote a compiler- or extension-produced descriptor into an interned runtime
 * node. The returned pointer is borrowed and stable for the lifetime of its
 * tier; the caller must not free it. `type` is only read, never retained, so it
 * may be arena-backed, SHM-backed or caller-owned. Returns NULL if the type is
 * not a collection descriptor. */
ZEND_API const zend_collection_info *zend_collection_info_intern(zend_type type);

/* Structural equality over runtime nodes. Nested nodes compare by pointer,
 * which is valid only because interning has already made them canonical. */
ZEND_API bool zend_collection_info_equals(
	const zend_collection_info *a, const zend_collection_info *b);

/* "vec[int]" etc., for diagnostics. Caller owns the returned string. */
ZEND_API zend_string *zend_collection_info_to_string(const zend_collection_info *info);

void zend_collection_info_request_init(void);
void zend_collection_info_request_shutdown(void);
void zend_collection_info_permanent_shutdown(void);

END_EXTERN_C()

#endif /* ZEND_COLLECTION_INFO_H */
