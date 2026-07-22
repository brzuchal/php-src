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

/* Classification bits. All are computed once, during promotion, from members
 * that are already canonical, and are immutable for the life of the node.
 *
 * The rule that makes classification cheap is that promotion is bottom-up: when
 * a parent is classified its children are already classified, so the parent
 * *reads* their cached bits instead of descending. Classifying a node is
 * therefore O(num_types), never O(tree), no matter how deeply it nests.
 *
 * Every bit below is kind-agnostic: it is defined in terms of "members", not of
 * vec's single element, so map[K,V], tuple[A,B,C] and shape[...] can reuse all
 * of them unchanged. Only ..._VALUE_CONSTRUCTIBLE consults a per-kind policy,
 * and that policy is applied at promotion, not at read time. */

/* Every member is a pure builtin mask: no class name, no nesting anywhere.
 * A node with this bit can be compared and checked without inspecting members
 * as structured types at all. */
#define ZEND_COLLECTION_INFO_ALL_MASK_MEMBERS    (1u << 0)
/* A runtime value of this type can be constructed: the members satisfy the
 * (narrower) element subset a value may hold. Replaces a recursive re-check at
 * every construction. */
#define ZEND_COLLECTION_INFO_VALUE_CONSTRUCTIBLE (1u << 1)

typedef struct _zend_collection_info {
	uint32_t   kind;       /* zend_collection_kind */
	uint32_t   num_types;
	/* Classification. Computed: by promotion, once, after members are filled.
	 * Valid: for the whole life of the node -- nodes are immutable.
	 * Readable by: anyone holding the node. Never recomputed, never written
	 * again. Reusable by every future kind. */
	uint32_t   flags;      /* ZEND_COLLECTION_INFO_* */
	/* The builtin-only element check for a single-member node, or 0 when the
	 * type needs more than a mask test. Lets the element check run without
	 * reading the member's zend_type. For kinds with more than one member this
	 * stays 0; a per-member equivalent can be added without disturbing it. */
	uint32_t   fast_mask;
	/* Structural key. Computed by promotion, cached; see the three key forms
	 * documented below. Never recomputed. */
	zend_ulong hash;
	/* Chain of nodes sharing this key. Owned by the request tier; written only
	 * by promotion while inserting. */
	struct _zend_collection_info *next;
	/* Members. A nested member's pointer is a canonical zend_collection_info
	 * child, never a compiler zend_collection_type. */
	zend_type  types[1];
} zend_collection_info;

#define ZEND_COLLECTION_INFO_HAS_FLAG(info, flag) \
	((((info)->flags) & (flag)) != 0)

#define ZEND_COLLECTION_INFO_IS_VALUE_CONSTRUCTIBLE(info) \
	ZEND_COLLECTION_INFO_HAS_FLAG(info, ZEND_COLLECTION_INFO_VALUE_CONSTRUCTIBLE)

/* Read a nested member as the canonical child node it is.
 *
 * Always use this rather than casting ZEND_TYPE_COLLECTION() by hand. The two
 * structs share kind and num_types and diverge after, so a hand-written cast
 * that picks the wrong one walks valid-looking but wrong offsets -- which is
 * exactly the defect that reached a release build once, because NDEBUG had
 * compiled out the assertion that would have caught it. */
#define ZEND_COLLECTION_INFO_CHILD(member) \
	((const zend_collection_info *) ZEND_TYPE_COLLECTION(member))

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

/* (2) CANONICAL KEY is `info->hash`, cached at construction and never
 * recomputed. (3) CANONICAL EQUALITY between two canonical nodes is pointer
 * identity -- there is deliberately no helper for it, because a function call
 * would only obscure that it is a plain comparison.
 *
 * The BRIDGE comparison (canonical node against a raw descriptor, used to
 * resolve collisions) is internal to zend_collection_info.c: everything outside
 * holds nodes and compares them by identity. Keeping it private is what stops
 * callers from reaching for a structural walk they do not need.
 */

/* PROMOTION. Canonicalize a compiler- or extension-produced descriptor and
 * return the borrowed node for it. Structurally identical descriptors always
 * return the same pointer, regardless of where they were allocated; descriptor
 * pointer identity is never consulted. Returns NULL for forms outside the
 * supported boundary. `type` is only read, never retained, so arena and SHM
 * memory neither escapes into the node nor is freed by it. */
ZEND_API const zend_collection_info *zend_collection_info_intern(zend_type type);

/* RESOLUTION CACHE. The runtime entry point for a *declaration*: returns the
 * canonical node for a compiler descriptor, promoting at most once per distinct
 * descriptor per request and answering from cache afterwards. Callers that hold
 * a declaration should use this rather than intern(), so a repeated check never
 * re-walks the descriptor. Returns NULL for unsupported forms, which are never
 * cached. See the definition for lifetime, invalidation and opcache notes. */
ZEND_API const zend_collection_info *zend_collection_info_resolve(zend_type type);

/* "vec[int]", "vec[vec[Foo]]" -- for diagnostics. Caller owns the result. */
ZEND_API zend_string *zend_collection_info_to_string(const zend_collection_info *info);

/* One member of a node -- "int", "vec[Foo]" -- for diagnostics that name the
 * expected type of a single member. Caller owns the result. Use this rather
 * than zend_type_to_string() on info->types[index]: a nested member points at a
 * zend_collection_info, which the generic stringifier would misread as a
 * zend_collection_type. */
ZEND_API zend_string *zend_collection_info_member_to_string(
		const zend_collection_info *info, uint32_t index);

/* Request tier lifecycle. Init runs in init_executor(); shutdown runs at the
 * end of zend_shutdown_executor_values() per INV-4. */
void zend_collection_info_request_init(void);
void zend_collection_info_request_shutdown(void);

/* Exercise chain separation from inside the engine boundary: a real hash
 * collision cannot be requested from PHP, so the behaviour is tested instead. */
ZEND_API bool zend_collection_info_collision_selftest(void);

#if ZEND_DEBUG
/* Debug-only instrumentation. Counts member comparisons that had to descend
 * into a nested node, so tests can demonstrate that cached classification --
 * not recursion -- answers the common checks. Not present in release builds. */
ZEND_API uint64_t zend_collection_info_descent_count(void);
/* Promotions actually performed, i.e. resolution-cache misses. */
ZEND_API uint64_t zend_collection_info_promotion_count(void);
/* Live canonical nodes in the request tier. */
ZEND_API uint32_t zend_collection_info_node_count(void);
#endif

END_EXTERN_C()

#endif /* ZEND_COLLECTION_INFO_H */
