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

#include "zend.h"
#include "zend_collection_info.h"
#include "zend_types.h"
#include "zend_smart_str.h"
#include "zend_compile.h"
#include "zend_vec.h"

/* Structural hashing of collection descriptors -- the *probe* side of the
 * canonicalization key. See zend_collection_info.h for the separation between
 * this, the key cached in a canonical node, and equality between two canonical
 * nodes.
 *
 * The hash exists to serve exactly one equality relation:
 * zend_type_structurally_equals() (zend_opcode.c). Every decision here mirrors
 * a decision made there, and the two must be changed together:
 *
 *   - only ZEND_TYPE_PURE_MASK participates, so allocation/provenance bits such
 *     as _ZEND_TYPE_ARENA_BIT can never influence the key;
 *   - descriptor members are folded positionally, because member order is
 *     semantic for the parameterized kinds (map[K,V], tuple[A,B,C]);
 *   - class names are folded case-insensitively, because that function compares
 *     them with zend_string_equals_ci().
 *
 * The contract this file must satisfy is one-directional:
 *
 *     zend_type_structurally_equals(a, b)  =>  hash(a) == hash(b)
 *
 * The converse is not claimed: colliding keys are expected and are resolved by
 * a full structural comparison at lookup time, never by the hash alone.
 */

/* FNV-1a, sized to zend_ulong so the key is a native word on both 32- and
 * 64-bit builds. The specific function is not load-bearing: the intern table
 * resolves collisions by full structural comparison, so only the "structurally
 * equal implies equal key" direction matters. */
#if SIZEOF_ZEND_LONG == 8
# define COLLECTION_KEY_SEED  Z_UL(0xcbf29ce484222325)
# define COLLECTION_KEY_PRIME Z_UL(0x100000001b3)
#else
# define COLLECTION_KEY_SEED  Z_UL(0x811c9dc5)
# define COLLECTION_KEY_PRIME Z_UL(0x01000193)
#endif

static zend_always_inline zend_ulong collection_key_mix(zend_ulong key, zend_ulong value)
{
	return (key ^ value) * COLLECTION_KEY_PRIME;
}

/* Case-insensitive, allocation-free. Mirrors zend_string_equals_ci(), which
 * lowers with the ASCII table, so this must lower the same way. */
static zend_ulong collection_key_mix_name_ci(zend_ulong key, const zend_string *name)
{
	const char *p = ZSTR_VAL(name);
	const char *end = p + ZSTR_LEN(name);

	while (p < end) {
		key = collection_key_mix(key, (zend_ulong)(unsigned char)zend_tolower_ascii(*p));
		p++;
	}
	return collection_key_mix(key, (zend_ulong)ZSTR_LEN(name));
}

/* The supported-input boundary of the first implementation.
 *
 * This is a deliberate contract, not a reflection of what the compiler happens
 * to reject today. A descriptor is hashable iff every parameter, recursively,
 * is one of:
 *
 *   - a pure builtin mask, including its nullable form (the null bit is part of
 *     the mask, so `?int` needs no special case);
 *   - a single class name;
 *   - a nested collection descriptor that is itself supported.
 *
 * Union and intersection members are outside the boundary. They have no
 * canonical member order yet -- the design requires sorting them before they
 * can produce a stable key -- so hashing one positionally would assign it a key
 * that depends on source order. Two structurally equal types could then hash
 * differently and intern as two nodes, silently breaking canonicalization. They
 * are therefore rejected up front rather than given an unstable key.
 */
static bool collection_key_type_is_supported(zend_type type, bool is_root)
{
	if (ZEND_TYPE_HAS_COLLECTION_DESCRIPTOR(type)) {
		const zend_collection_type *desc = ZEND_TYPE_COLLECTION(type);

		if (desc->num_types == 0) {
			return false;
		}
		for (uint32_t i = 0; i < desc->num_types; i++) {
			if (!collection_key_type_is_supported(desc->types[i], /* is_root */ false)) {
				return false;
			}
		}
		return true;
	}

	/* Only a collection descriptor may appear at the root. */
	if (is_root) {
		return false;
	}

	/* Union and intersection lists: outside the boundary, see above. */
	if (ZEND_TYPE_IS_TYPE_LIST(type)) {
		return false;
	}

	/* A class name, or a pure builtin mask. Both are supported. */
	return true;
}

ZEND_API bool zend_collection_key_is_supported(zend_type type)
{
	return collection_key_type_is_supported(type, /* is_root */ true);
}

static zend_ulong collection_key_hash_type(zend_ulong key, zend_type type)
{
	/* Provenance and allocation bits are excluded by construction: only the
	 * pure may-be mask is folded in. */
	key = collection_key_mix(key, (zend_ulong)ZEND_TYPE_PURE_MASK(type));

	if (ZEND_TYPE_HAS_COLLECTION_DESCRIPTOR(type)) {
		const zend_collection_type *desc = ZEND_TYPE_COLLECTION(type);

		key = collection_key_mix(key, (zend_ulong)desc->kind);
		key = collection_key_mix(key, (zend_ulong)desc->num_types);
		/* Positional: member order is semantic for map/tuple, and differing
		 * nesting depth changes the fold sequence. */
		for (uint32_t i = 0; i < desc->num_types; i++) {
			key = collection_key_hash_type(key, desc->types[i]);
		}
		return key;
	}

	if (ZEND_TYPE_HAS_NAME(type)) {
		return collection_key_mix_name_ci(key, ZEND_TYPE_NAME(type));
	}

	return key;
}

ZEND_API zend_ulong zend_collection_key_hash_type(zend_type type)
{
	/* Callers must have accepted the input first. Hashing an unsupported form
	 * would produce a key that is stable for this call but not across
	 * structurally equal inputs, which is precisely the failure the intern
	 * table cannot tolerate. */
	ZEND_ASSERT(zend_collection_key_is_supported(type)
		&& "collection key: unsupported descriptor form");

	return collection_key_hash_type(COLLECTION_KEY_SEED, type);
}

/* ---------------------------------------------------------------------------
 * Canonicalization: request-local interning of collection types.
 *
 * Ownership (docs/first-class-collections/runtime-type-ownership.md):
 *
 *   Who owns nodes        The request tier, EG(collection_types), owns every
 *                         node it holds. Nothing else does.
 *   What values hold      A borrowed zend_collection_info*. Values never own,
 *                         addref or release a node (INV-5), so constructing
 *                         and destroying a vec costs no type-ownership work.
 *   When nodes die        All at once, at the end of
 *                         zend_shutdown_executor_values(), after the object
 *                         store has been freed. That function is the hook
 *                         point rather than shutdown_executor(), because
 *                         preload calls it directly and never calls the other.
 *   Request shutdown      Bulk teardown; each node releases only its own
 *                         class-name strings. Nested nodes are borrowed and
 *                         are freed by the same sweep, so no recursion.
 *   Collisions            Nodes sharing a key are chained through ->next and
 *                         separated by full structural comparison. A shared
 *                         key never merges two types; it only lengthens a
 *                         chain.
 *
 * A node never embeds a compiler descriptor: nested members point at other
 * canonical nodes, and the arena bit is stripped from every member, so no
 * arena or SHM pointer can be reached from a runtime value.
 * ------------------------------------------------------------------------- */

#if ZEND_DEBUG
static uint64_t collection_info_descents = 0;

ZEND_API uint64_t zend_collection_info_descent_count(void)
{
	return collection_info_descents;
}
#endif

static bool collection_info_matches_member(zend_type node_slot, zend_type desc_slot)
{
	if (ZEND_TYPE_PURE_MASK(node_slot) != ZEND_TYPE_PURE_MASK(desc_slot)) {
		return false;
	}
	if (ZEND_TYPE_HAS_COLLECTION_DESCRIPTOR(node_slot)
	 || ZEND_TYPE_HAS_COLLECTION_DESCRIPTOR(desc_slot)) {
		if (!ZEND_TYPE_HAS_COLLECTION_DESCRIPTOR(node_slot)
		 || !ZEND_TYPE_HAS_COLLECTION_DESCRIPTOR(desc_slot)) {
			return false;
		}
#if ZEND_DEBUG
		collection_info_descents++;
#endif
		/* The node side is a canonical child; the descriptor side is still a
		 * compiler descriptor, so this must descend. Only reachable for a node
		 * that classification marked as nested -- see the fast path in
		 * zend_collection_info_matches_type(). */
		return zend_collection_info_matches_type(
			(const zend_collection_info *) ZEND_TYPE_COLLECTION(node_slot), desc_slot);
	}
	if (ZEND_TYPE_HAS_NAME(node_slot) || ZEND_TYPE_HAS_NAME(desc_slot)) {
		if (!ZEND_TYPE_HAS_NAME(node_slot) || !ZEND_TYPE_HAS_NAME(desc_slot)) {
			return false;
		}
		/* Case-insensitive, matching zend_type_structurally_equals() and the
		 * key's own folding. */
		return zend_string_equals_ci(ZEND_TYPE_NAME(node_slot), ZEND_TYPE_NAME(desc_slot));
	}
	return true;
}

ZEND_API bool zend_collection_info_matches_type(const zend_collection_info *info, zend_type type)
{
	if (!ZEND_TYPE_HAS_COLLECTION_DESCRIPTOR(type)) {
		return false;
	}

	const zend_collection_type *desc = ZEND_TYPE_COLLECTION(type);

	if (info->kind != desc->kind || info->num_types != desc->num_types) {
		return false;
	}

	/* Cached classification answers the common case outright: when no member is
	 * a class name or a nested collection, the comparison is masks only, so
	 * neither side is inspected as a structured type and nothing descends. */
	if (ZEND_COLLECTION_INFO_HAS_FLAG(info, ZEND_COLLECTION_INFO_ALL_MASK_MEMBERS)) {
		for (uint32_t i = 0; i < info->num_types; i++) {
			zend_type desc_slot = desc->types[i];

			if (ZEND_TYPE_HAS_COLLECTION_DESCRIPTOR(desc_slot)
			 || ZEND_TYPE_HAS_NAME(desc_slot)
			 || ZEND_TYPE_PURE_MASK(info->types[i]) != ZEND_TYPE_PURE_MASK(desc_slot)) {
				return false;
			}
		}
		return true;
	}

	for (uint32_t i = 0; i < info->num_types; i++) {
		if (!collection_info_matches_member(info->types[i], desc->types[i])) {
			return false;
		}
	}
	return true;
}

ZEND_API bool zend_collection_info_equals(
	const zend_collection_info *a, const zend_collection_info *b)
{
	/* Both sides are canonical, so identity is the whole answer. */
	return a == b;
}

/* Classify a node whose members are already filled and already canonical.
 *
 * This is the single place any classification field is written. It reads each
 * child's *cached* metadata rather than descending, which is what keeps the
 * cost O(num_types) instead of O(tree): by the time a parent is classified,
 * promotion has already classified every child.
 *
 * All fields below are immutable afterwards. Nothing at runtime recomputes
 * them, and nothing writes to a node once it is in the table. */
static void collection_info_classify(zend_collection_info *info)
{
	uint32_t flags = 0;
	uint32_t depth = 1;
	bool all_mask = true;
	bool constructible;

	for (uint32_t i = 0; i < info->num_types; i++) {
		zend_type member = info->types[i];

		if (ZEND_TYPE_HAS_COLLECTION_DESCRIPTOR(member)) {
			const zend_collection_info *child =
				(const zend_collection_info *) ZEND_TYPE_COLLECTION(member);

			flags |= ZEND_COLLECTION_INFO_HAS_NESTED;
			all_mask = false;

			/* Cached, not recomputed: the child already knows its own depth and
			 * whether it contains a class name. */
			if (child->depth + 1 > depth) {
				depth = child->depth + 1;
			}
			if (ZEND_COLLECTION_INFO_HAS_FLAG(child, ZEND_COLLECTION_INFO_HAS_CLASS_NAME)) {
				flags |= ZEND_COLLECTION_INFO_HAS_CLASS_NAME;
			}
			continue;
		}

		if (ZEND_TYPE_HAS_NAME(member)) {
			flags |= ZEND_COLLECTION_INFO_HAS_CLASS_NAME;
			all_mask = false;
		}
	}

	if (all_mask) {
		flags |= ZEND_COLLECTION_INFO_ALL_MASK_MEMBERS;
	}

	/* Whether a *value* of this type can be built. The element subset a value
	 * may hold is narrower than the subset a type may name, and the policy is
	 * per kind. Applying it here means construction never re-derives it. */
	constructible = false;
	if (info->kind == ZEND_COLLECTION_TYPE_VEC && info->num_types == 1) {
		zend_type member = info->types[0];

		if (ZEND_TYPE_HAS_COLLECTION_DESCRIPTOR(member)) {
			const zend_collection_info *child =
				(const zend_collection_info *) ZEND_TYPE_COLLECTION(member);

			/* The child's own cached verdict; no descent. */
			constructible =
				ZEND_COLLECTION_INFO_IS_VALUE_CONSTRUCTIBLE(child)
				&& (ZEND_TYPE_FULL_MASK(member) & _ZEND_TYPE_MAY_BE_MASK) == 0;
		} else {
			constructible = zend_vec_type_is_supported(member);
		}
	}
	if (constructible) {
		flags |= ZEND_COLLECTION_INFO_VALUE_CONSTRUCTIBLE;
	}

	info->flags = flags;
	info->depth = depth;

	/* The builtin-only element check, or 0 when a mask test is not enough. */
	info->fast_mask =
		(info->num_types == 1 && all_mask) ? ZEND_TYPE_PURE_MASK(info->types[0]) : 0;
}

/* Walk a key's chain, separating members by full structural comparison. A
 * shared key never merges two types; it only makes this walk longer. */
static const zend_collection_info *collection_info_find_in_chain(
	const zend_collection_info *head, zend_type type)
{
	for (const zend_collection_info *cur = head; cur; cur = cur->next) {
		if (zend_collection_info_matches_type(cur, type)) {
			return cur;
		}
	}
	return NULL;
}

ZEND_API const zend_collection_info *zend_collection_info_intern(zend_type type)
{
	if (!zend_collection_key_is_supported(type)) {
		return NULL;
	}

	zend_ulong key = zend_collection_key_hash_type(type);
	zend_collection_info *node =
		zend_hash_index_find_ptr(&EG(collection_types), key);
	const zend_collection_info *hit = collection_info_find_in_chain(node, type);

	if (hit) {
		return hit;
	}

	const zend_collection_type *desc = ZEND_TYPE_COLLECTION(type);
	zend_collection_info *created = emalloc(ZEND_COLLECTION_INFO_SIZE(desc->num_types));

	created->kind = desc->kind;
	created->num_types = desc->num_types;
	created->hash = key;

	for (uint32_t i = 0; i < desc->num_types; i++) {
		zend_type member = desc->types[i];

		if (ZEND_TYPE_HAS_COLLECTION_DESCRIPTOR(member)) {
			/* Bottom-up: the child is canonicalized first, and the node stores
			 * a borrowed pointer to it rather than a copy of the descriptor. */
			const zend_collection_info *child = zend_collection_info_intern(member);

			/* Support was validated for the whole tree above, so a supported
			 * parent cannot contain an unsupported child. */
			ZEND_ASSERT(child != NULL);

			zend_type slot = ZEND_TYPE_INIT_NONE(0);
			ZEND_TYPE_FULL_MASK(slot) = ZEND_TYPE_PURE_MASK(member);
			ZEND_TYPE_SET_COLLECTION(slot, (void *) child);
			created->types[i] = slot;
			continue;
		}

		created->types[i] = member;
		/* Provenance never survives promotion: a node member is request-owned
		 * or borrowed from another node, never arena- or SHM-backed. */
		ZEND_TYPE_FULL_MASK(created->types[i]) &= ~_ZEND_TYPE_ARENA_BIT;

		if (ZEND_TYPE_HAS_NAME(created->types[i])) {
			/* A no-op for the interned names the compiler produces, and a real
			 * reference for extension-supplied ones. */
			zend_string_addref(ZEND_TYPE_NAME(created->types[i]));
		}
	}

	collection_info_classify(created);

	/* Re-read the chain head: canonicalizing children above may have inserted
	 * into this same bucket. */
	created->next = zend_hash_index_find_ptr(&EG(collection_types), key);
	zend_hash_index_update_ptr(&EG(collection_types), key, created);

	return created;
}

static void collection_info_stringify(smart_str *str, const zend_collection_info *info)
{
	smart_str_appends(str, zend_collection_type_kind_name(info->kind));
	smart_str_appendc(str, '[');

	for (uint32_t i = 0; i < info->num_types; i++) {
		if (i != 0) {
			smart_str_appends(str, ", ");
		}
		zend_type member = info->types[i];

		if (ZEND_TYPE_HAS_COLLECTION_DESCRIPTOR(member)) {
			if (ZEND_TYPE_ALLOW_NULL(member)) {
				smart_str_appendc(str, '?');
			}
			collection_info_stringify(str,
				(const zend_collection_info *) ZEND_TYPE_COLLECTION(member));
			continue;
		}

		zend_string *rendered = zend_type_to_string(member);
		smart_str_append(str, rendered);
		zend_string_release(rendered);
	}

	smart_str_appendc(str, ']');
}

ZEND_API zend_string *zend_collection_info_to_string(const zend_collection_info *info)
{
	smart_str str = {0};

	collection_info_stringify(&str, info);
	smart_str_0(&str);

	return str.s;
}

void zend_collection_info_request_init(void)
{
	zend_hash_init(&EG(collection_types), 8, NULL, NULL, 0);
}

void zend_collection_info_request_shutdown(void)
{
	zend_collection_info *head;

	ZEND_HASH_FOREACH_PTR(&EG(collection_types), head) {
		zend_collection_info *cur = head;

		while (cur) {
			zend_collection_info *next = cur->next;

			/* Only this node's own strings. Nested nodes are borrowed and are
			 * freed by this same sweep. */
			for (uint32_t i = 0; i < cur->num_types; i++) {
				if (ZEND_TYPE_HAS_NAME(cur->types[i])) {
					zend_string_release(ZEND_TYPE_NAME(cur->types[i]));
				}
			}
			efree(cur);
			cur = next;
		}
	} ZEND_HASH_FOREACH_END();

	zend_hash_destroy(&EG(collection_types));
	/* Leave a valid empty table: preload runs this teardown and then keeps
	 * using the executor globals. */
	zend_hash_init(&EG(collection_types), 8, NULL, NULL, 0);
}

/* Collisions cannot be produced on demand from PHP -- they need two structurally
 * different types whose FNV keys agree. So the *behaviour* is exercised instead:
 * two real canonical nodes are chained as if they shared a key, and the chain
 * walk must still return each one only for its own descriptor. */
ZEND_API bool zend_collection_info_collision_selftest(void)
{
	union {
		zend_collection_type desc;
		char buf[ZEND_TYPE_COLLECTION_SIZE(1)];
	} a, b;
	zend_type ta = ZEND_TYPE_INIT_NONE(0);
	zend_type tb = ZEND_TYPE_INIT_NONE(0);
	zend_type m_long = ZEND_TYPE_INIT_MASK(1u << IS_LONG);
	zend_type m_string = ZEND_TYPE_INIT_MASK(1u << IS_STRING);
	const zend_collection_info *node_a, *node_b;
	zend_collection_info *mutable_b;
	zend_collection_info *saved_next;
	bool ok;

	a.desc.kind = ZEND_COLLECTION_TYPE_VEC;
	a.desc.num_types = 1;
	a.desc.types[0] = m_long;
	ZEND_TYPE_SET_COLLECTION(ta, &a.desc);

	b.desc.kind = ZEND_COLLECTION_TYPE_VEC;
	b.desc.num_types = 1;
	b.desc.types[0] = m_string;
	ZEND_TYPE_SET_COLLECTION(tb, &b.desc);

	node_a = zend_collection_info_intern(ta);
	node_b = zend_collection_info_intern(tb);
	if (!node_a || !node_b || node_a == node_b) {
		return false;
	}

	/* Splice A onto B's chain, so a walk from B sees both. Restored below. */
	mutable_b = (zend_collection_info *) node_b;
	saved_next = mutable_b->next;
	mutable_b->next = (zend_collection_info *) node_a;

	ok = collection_info_find_in_chain(node_b, tb) == node_b
	  && collection_info_find_in_chain(node_b, ta) == node_a;

	mutable_b->next = saved_next;

	return ok;
}
