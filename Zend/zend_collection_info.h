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

/* Canonicalization keys come in three distinct forms. They are kept separate on
 * purpose, because collapsing them would make every intern-table lookup walk a
 * raw zend_type tree forever:
 *
 *   (1) PROBE -- an incoming compiler- or extension-produced descriptor, whose
 *       key must be computed by walking the raw zend_type tree. This is the
 *       only tree-walking path, and it runs at most once per lookup.
 *
 *   (2) CANONICAL KEY -- the key of an already-interned node, cached in its
 *       `hash` field at construction. Read via ZEND_COLLECTION_INFO_KEY(); it
 *       is never recomputed, and no caller should ever re-derive it from the
 *       node's members.
 *
 *   (3) CANONICAL EQUALITY -- equality between two already-interned nodes.
 *       Because interning makes nodes canonical, this is pointer identity for
 *       nested members rather than a structural walk.
 *
 * The first implementation shares the recursion between (1) and the collision
 * check, but the boundary above is the contract: anything holding a
 * zend_collection_info* is in world (2)/(3) and must not fall back to (1).
 */

/* (1) PROBE. Whether a descriptor is within the supported input boundary, and
 * its structural key. `type` is only read, never retained, so it may be
 * arena-backed, SHM-backed or caller-owned.
 *
 * Contract, mirroring zend_type_structurally_equals():
 *     zend_type_structurally_equals(a, b)  =>  hash(a) == hash(b)
 * The converse is not claimed; collisions are resolved by full comparison.
 *
 * zend_collection_key_hash_type() asserts on unsupported input: an unsupported
 * form has no stable key, and assigning it one silently would break
 * canonicalization rather than fail. Callers must test support first. */
ZEND_API bool zend_collection_key_is_supported(zend_type type);
ZEND_API zend_ulong zend_collection_key_hash_type(zend_type type);

/* (2) CANONICAL KEY. Cached at construction, never recomputed. */
#define ZEND_COLLECTION_INFO_KEY(info) ((info)->hash)

/* (3) CANONICAL EQUALITY, promotion, tier lifecycle and diagnostics arrive with
 * the intern table; they are deliberately absent here because nothing can
 * construct a node yet, and declaring an API before it can be exercised invites
 * untested code. */

END_EXTERN_C()

#endif /* ZEND_COLLECTION_INFO_H */
