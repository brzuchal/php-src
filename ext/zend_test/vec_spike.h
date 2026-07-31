/*
 * Hybrid persistent vec — feasibility spike (DISPOSABLE, not for merge).
 *
 * Three isolated benchmark representations over real zvals + Zend MM:
 *   A. flat-copy:        every append allocates count+1 and copies everything
 *   B. flat-capacity:    exclusive (rc==1) value with free capacity appends in
 *                        place, otherwise copies the whole payload
 *   C. hybrid:           immutable flat base + shared append-only block spine +
 *                        per-version visible prefix + copy-on-branch last block
 *
 * None of this touches the production zend_vec layout.
 */

#ifndef VEC_SPIKE_H
#define VEC_SPIKE_H

#include "php.h"
#include <stdint.h>

/* ---------------- instrumentation ---------------- */

typedef enum {
	TAG_FLAT = 0,   /* A containers */
	TAG_CAP,        /* B containers */
	TAG_BASE,       /* C flat base payloads */
	TAG_BLOCK,      /* C blocks */
	TAG_SPINE,      /* C spine structs */
	TAG_SPINEARR,   /* C spine pointer arrays */
	TAG_VER,        /* C version structs */
	TAG_MISC,
	TAG__N
} spk_tag;

typedef struct {
	uint64_t allocs, frees, bytes_alloc, bytes_freed;
	uint64_t tag_alloc_bytes[TAG__N], tag_free_bytes[TAG__N];
	uint64_t tag_allocs[TAG__N], tag_frees[TAG__N];
	uint64_t zval_copies;    /* container-owned zval copies (addref-copy) */
	uint64_t zval_moves;     /* ownership moves (B growth memcpy, no addref) */
	uint64_t zval_dtors;     /* container-owned zval releases */
	uint64_t rc_addref, rc_delref;       /* element-level refcount ops */
	uint64_t block_addref, block_delref; /* C block container refcount ops */
	uint64_t block_allocs, block_frees;
	uint64_t spine_allocs, spine_frees, spine_grows;
	uint64_t version_allocs, version_frees;
	uint64_t branches;       /* C branch appends taken */
} spk_counters;

extern spk_counters spk_cnt;

static zend_always_inline void *spk_alloc(size_t n, spk_tag tag) {
	spk_cnt.allocs++;
	spk_cnt.bytes_alloc += n;
	spk_cnt.tag_allocs[tag]++;
	spk_cnt.tag_alloc_bytes[tag] += n;
	return emalloc(n);
}

static zend_always_inline void spk_free(void *p, size_t n, spk_tag tag) {
	spk_cnt.frees++;
	spk_cnt.bytes_freed += n;
	spk_cnt.tag_frees[tag]++;
	spk_cnt.tag_free_bytes[tag] += n;
	efree(p);
}

static zend_always_inline void spk_zval_copy(zval *dst, const zval *src) {
	ZVAL_COPY(dst, (zval *) src);
	spk_cnt.zval_copies++;
	if (Z_REFCOUNTED_P(src)) {
		spk_cnt.rc_addref++;
	}
}

static zend_always_inline void spk_zval_dtor(zval *z) {
	spk_cnt.zval_dtors++;
	if (Z_REFCOUNTED_P(z)) {
		spk_cnt.rc_delref++;
	}
	zval_ptr_dtor(z);
}

/* memcpy + addref pass; models the production copy loop */
static zend_always_inline void spk_copy_slots(zval *dst, const zval *src, uint32_t n) {
	memcpy(dst, src, (size_t) n * sizeof(zval));
	for (uint32_t i = 0; i < n; i++) {
		if (Z_REFCOUNTED(dst[i])) {
			Z_ADDREF(dst[i]);
			spk_cnt.rc_addref++;
		}
	}
	spk_cnt.zval_copies += n;
}

/* ---------------- A: flat-copy ---------------- */

typedef struct {
	uint32_t rc;
	uint32_t count;
	zval slots[];
} spk_flat;

#define SPK_FLAT_SZ(n) (sizeof(spk_flat) + (size_t)(n) * sizeof(zval))

/* ---------------- B: flat-capacity/consume ---------------- */

typedef struct {
	uint32_t rc;
	uint32_t count;
	uint32_t cap;
	zval slots[];
} spk_cap;

#define SPK_CAP_SZ(cap) (sizeof(spk_cap) + (size_t)(cap) * sizeof(zval))

/* ---------------- C: hybrid ---------------- */

/*
 * Block: fixed-capacity zval storage, append-only. phys_len counts the
 * initialized slots; slots beyond phys_len are uninitialized and never read.
 * rc counts the VERSIONS whose visible prefix includes this block (the spine
 * holds borrowed pointers and owns no block references).
 */
typedef struct {
	uint32_t rc;
	uint8_t  phys_len;      /* valid because chunk <= 128 < 256 */
	uint8_t  _pad[3];
	zval     slots[];
} spk_block;

#define SPK_BLK_SZ(chunk) (sizeof(spk_block) + (size_t)(chunk) * sizeof(zval))

/*
 * Spine: shared append-only physical block sequence. rc counts referencing
 * versions. ptrs is exclusively owned by the spine and may be erealloc'd on
 * growth; versions never cache ptrs. Entries at index >= every live version's
 * vb may be stale (block freed when its last viewer died) and are never
 * dereferenced: the tip check only reads ptrs[vb-1] of the appending version,
 * and the branch path dereferences only ptrs[0 .. vb-1].
 */
typedef struct {
	uint32_t   rc;
	uint32_t   nphys;       /* physical blocks appended so far */
	uint32_t   cap;         /* ptrs array capacity */
	spk_block **ptrs;
} spk_spine;

/*
 * Version: {base, spine, visible block count, visible length of last visible
 * block, total logical count}. Invariants:
 *   - spine == NULL  <=>  vb == 0 (literal, flat payload only)
 *   - spine != NULL  =>   vb >= 1 and count == base_count + (vb-1)*chunk + lv
 *   - blocks 0..vb-2 are full (phys_len == chunk) and fully visible
 *   - lv <= ptrs[vb-1]->phys_len  (visible prefix of the last visible block)
 *   - version owns: 1 ref on base, 1 ref on spine, 1 ref on EACH visible block
 */
typedef struct {
	uint32_t   rc;
	uint32_t   count;
	spk_flat  *base;        /* NULL for empty literal; tagged TAG_BASE */
	spk_spine *spine;       /* NULL until first append */
	uint32_t   vb;
	uint8_t    lv;          /* valid because chunk <= 128 < 256 */
	uint8_t    shift;       /* chunk == 1u << shift */
	uint8_t    _pad[2];
} spk_hyb;

/* ---------------- element pools ---------------- */

typedef enum { EL_INT = 0, EL_STR, EL_OBJ, EL_ARR, EL_NESTED, EL__N } spk_el;

zval *spk_pool_build(spk_el kind, uint32_t n);
void  spk_pool_free(zval *pool, uint32_t n);
int   spk_el_parse(const char *s);

/* ---------------- generic handle ops (dispatch on repr) ---------------- */

typedef enum { REPR_A = 0, REPR_B, REPR_C } spk_repr;

void    *spk_literal(spk_repr r, uint8_t shift, const zval *pool, uint32_t n);
void    *spk_append(spk_repr r, void *h, const zval *x);
void    *spk_dup(spk_repr r, void *h);
void     spk_release(spk_repr r, void *h);
zval    *spk_get(spk_repr r, void *h, uint32_t i);
uint32_t spk_count(spk_repr r, void *h);
uint64_t spk_iter_sum(spk_repr r, void *h);
uint64_t spk_deep_checksum(spk_repr r, void *h);

/* selftest + registration */
void vec_spike_minit(void);

#endif /* VEC_SPIKE_H */
