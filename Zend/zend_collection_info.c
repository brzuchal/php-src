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
