/* Hybrid persistent vec spike — representations + selftest. See vec_spike.h. */

#include "vec_spike.h"
#include "zend_smart_str.h"

spk_counters spk_cnt;

/* ================= element pools ================= */

zval *spk_pool_build(spk_el kind, uint32_t n)
{
	zval *p = safe_emalloc(n ? n : 1, sizeof(zval), 0);
	for (uint32_t i = 0; i < n; i++) {
		switch (kind) {
			case EL_INT:
				ZVAL_LONG(&p[i], (zend_long) (i * 2654435761u + 12345));
				break;
			case EL_STR: {
				/* runtime-created => not interned => refcounted */
				zend_string *s = zend_strpprintf(0, "e%08u", i);
				ZVAL_STR(&p[i], s);
				break;
			}
			case EL_OBJ:
				object_init(&p[i]);
				break;
			case EL_ARR: {
				zend_array *a = zend_new_array(2);
				zval v;
				ZVAL_STR(&v, zend_strpprintf(0, "a%08u", i));
				zend_hash_next_index_insert(a, &v);
				ZVAL_LONG(&v, i);
				zend_hash_next_index_insert(a, &v);
				ZVAL_ARR(&p[i], a);
				break;
			}
			case EL_NESTED: {
				zend_array *inner = zend_new_array(2);
				zval v;
				ZVAL_STR(&v, zend_strpprintf(0, "n%08u", i));
				zend_hash_next_index_insert(inner, &v);
				ZVAL_LONG(&v, i);
				zend_hash_next_index_insert(inner, &v);
				zend_array *outer = zend_new_array(2);
				ZVAL_ARR(&v, inner);
				zend_hash_next_index_insert(outer, &v);
				ZVAL_LONG(&v, i * 3);
				zend_hash_next_index_insert(outer, &v);
				ZVAL_ARR(&p[i], outer);
				break;
			}
			default:
				ZVAL_NULL(&p[i]);
		}
	}
	return p;
}

void spk_pool_free(zval *pool, uint32_t n)
{
	for (uint32_t i = 0; i < n; i++) {
		zval_ptr_dtor(&pool[i]);
	}
	efree(pool);
}

int spk_el_parse(const char *s)
{
	if (!strcmp(s, "int")) return EL_INT;
	if (!strcmp(s, "string")) return EL_STR;
	if (!strcmp(s, "object")) return EL_OBJ;
	if (!strcmp(s, "array")) return EL_ARR;
	if (!strcmp(s, "nested")) return EL_NESTED;
	return -1;
}

/* ================= A: flat-copy ================= */

static spk_flat *flat_new(uint32_t n, spk_tag tag)
{
	spk_flat *f = spk_alloc(SPK_FLAT_SZ(n), tag);
	f->rc = 1;
	f->count = n;
	return f;
}

static spk_flat *flat_literal(const zval *pool, uint32_t n)
{
	spk_flat *f = flat_new(n, TAG_FLAT);
	spk_copy_slots(f->slots, pool, n);
	return f;
}

static void flat_release_tag(spk_flat *f, spk_tag tag)
{
	if (--f->rc == 0) {
		for (uint32_t i = 0; i < f->count; i++) {
			spk_zval_dtor(&f->slots[i]);
		}
		spk_free(f, SPK_FLAT_SZ(f->count), tag);
	}
}

static spk_flat *flat_append(spk_flat *f, const zval *x)
{
	spk_flat *n = flat_new(f->count + 1, TAG_FLAT);
	spk_copy_slots(n->slots, f->slots, f->count);
	spk_zval_copy(&n->slots[f->count], x);
	flat_release_tag(f, TAG_FLAT);
	return n;
}

/* ================= B: flat-capacity/consume ================= */

static uint32_t pow2ceil32(uint32_t v)
{
	if (v < 4) return 4;
	v--;
	v |= v >> 1; v |= v >> 2; v |= v >> 4; v |= v >> 8; v |= v >> 16;
	return v + 1;
}

static spk_cap *cap_new(uint32_t count, uint32_t cap)
{
	spk_cap *c = spk_alloc(SPK_CAP_SZ(cap), TAG_CAP);
	c->rc = 1;
	c->count = count;
	c->cap = cap;
	return c;
}

static spk_cap *cap_literal(const zval *pool, uint32_t n)
{
	/* literal is exact-size: cap == count, no slack */
	spk_cap *c = cap_new(n, n);
	spk_copy_slots(c->slots, pool, n);
	return c;
}

static void cap_release(spk_cap *c)
{
	if (--c->rc == 0) {
		for (uint32_t i = 0; i < c->count; i++) {
			spk_zval_dtor(&c->slots[i]);
		}
		spk_free(c, SPK_CAP_SZ(c->cap), TAG_CAP);
	}
}

static spk_cap *cap_append(spk_cap *c, const zval *x)
{
	if (c->rc == 1) {
		if (c->count < c->cap) {
			spk_zval_copy(&c->slots[c->count], x);
			c->count++;
			return c;
		}
		/* exclusive but full: grow by move (alloc+memcpy+free, matches the
		 * production constraint that erealloc cannot move GC-visible slots) */
		uint32_t ncap = pow2ceil32(c->count + 1);
		spk_cap *n = cap_new(c->count, ncap);
		memcpy(n->slots, c->slots, (size_t) c->count * sizeof(zval));
		spk_cnt.zval_moves += c->count;
		spk_free(c, SPK_CAP_SZ(c->cap), TAG_CAP); /* moved: no slot dtor */
		spk_zval_copy(&n->slots[n->count], x);
		n->count++;
		return n;
	}
	/* shared: persistent copy-on-write append */
	uint32_t ncap = pow2ceil32(c->count + 1);
	spk_cap *n = cap_new(c->count, ncap);
	spk_copy_slots(n->slots, c->slots, c->count);
	spk_zval_copy(&n->slots[n->count], x);
	n->count++;
	cap_release(c);
	return n;
}

/* ================= C: hybrid ================= */

static spk_block *block_new(uint32_t chunk)
{
	spk_block *b = spk_alloc(SPK_BLK_SZ(chunk), TAG_BLOCK);
	b->rc = 1;
	b->phys_len = 0;
	spk_cnt.block_allocs++;
	return b;
}

static zend_always_inline void block_addref(spk_block *b)
{
	b->rc++;
	spk_cnt.block_addref++;
}

static void block_release(spk_block *b, uint32_t chunk)
{
	spk_cnt.block_delref++;
	if (--b->rc == 0) {
		for (uint32_t i = 0; i < b->phys_len; i++) {
			spk_zval_dtor(&b->slots[i]);
		}
		spk_free(b, SPK_BLK_SZ(chunk), TAG_BLOCK);
		spk_cnt.block_frees++;
	}
}

static spk_spine *spine_new(uint32_t cap)
{
	spk_spine *s = spk_alloc(sizeof(spk_spine), TAG_SPINE);
	s->rc = 1;
	s->nphys = 0;
	s->cap = cap;
	s->ptrs = spk_alloc((size_t) cap * sizeof(spk_block *), TAG_SPINEARR);
	spk_cnt.spine_allocs++;
	return s;
}

static void spine_release(spk_spine *s)
{
	if (--s->rc == 0) {
		/* blocks are owned by versions, not by the spine */
		spk_free(s->ptrs, (size_t) s->cap * sizeof(spk_block *), TAG_SPINEARR);
		spk_free(s, sizeof(spk_spine), TAG_SPINE);
		spk_cnt.spine_frees++;
	}
}

static void spine_push(spk_spine *s, spk_block *b)
{
	if (s->nphys == s->cap) {
		uint32_t ncap = s->cap * 2;
		spk_block **np = spk_alloc((size_t) ncap * sizeof(spk_block *), TAG_SPINEARR);
		memcpy(np, s->ptrs, (size_t) s->nphys * sizeof(spk_block *));
		spk_free(s->ptrs, (size_t) s->cap * sizeof(spk_block *), TAG_SPINEARR);
		s->ptrs = np;
		s->cap = ncap;
		spk_cnt.spine_grows++;
	}
	s->ptrs[s->nphys++] = b;
}

static spk_hyb *ver_new(void)
{
	spk_hyb *v = spk_alloc(sizeof(spk_hyb), TAG_VER);
	spk_cnt.version_allocs++;
	v->rc = 1;
	return v;
}

static spk_hyb *hyb_literal(const zval *pool, uint32_t n, uint8_t shift)
{
	spk_hyb *v = ver_new();
	v->count = n;
	v->base = NULL;
	v->spine = NULL;
	v->vb = 0;
	v->lv = 0;
	v->shift = shift;
	if (n > 0) {
		spk_flat *b = flat_new(n, TAG_BASE);
		spk_copy_slots(b->slots, pool, n);
		v->base = b;
	}
	return v;
}

static void hyb_release(spk_hyb *v)
{
	if (--v->rc) {
		return;
	}
	if (v->spine) {
		uint32_t chunk = 1u << v->shift;
		for (uint32_t k = 0; k < v->vb; k++) {
			block_release(v->spine->ptrs[k], chunk);
		}
		spine_release(v->spine);
	}
	if (v->base) {
		flat_release_tag(v->base, TAG_BASE);
	}
	spk_free(v, sizeof(spk_hyb), TAG_VER);
	spk_cnt.version_frees++;
}

/* rc>1 evolve helper: materialize a new version struct that owns fresh
 * references on base, spine and each of the OLD vb visible blocks. */
static spk_hyb *hyb_clone_refs(const spk_hyb *v)
{
	spk_hyb *n = ver_new();
	*n = *v;
	n->rc = 1;
	if (n->base) {
		n->base->rc++;
	}
	if (n->spine) {
		n->spine->rc++;
		for (uint32_t k = 0; k < v->vb; k++) {
			block_addref(v->spine->ptrs[k]);
		}
	}
	return n;
}

/* Branch append: receiver's visible prefix ends before the physical tip.
 * Shares the flat base and all fully-visible full blocks, copies only the
 * receiver-visible prefix of the last partial block. */
static spk_hyb *hyb_branch_append(spk_hyb *v, const zval *x)
{
	uint32_t chunk = 1u << v->shift;
	spk_spine *s = v->spine;
	uint32_t shared = (v->lv == chunk) ? v->vb : v->vb - 1;
	uint32_t plen = (v->lv == chunk) ? 0 : v->lv;

	spk_spine *ns = spine_new(pow2ceil32(shared + 1));
	memcpy(ns->ptrs, s->ptrs, (size_t) shared * sizeof(spk_block *));
	ns->nphys = shared;
	for (uint32_t k = 0; k < shared; k++) {
		block_addref(ns->ptrs[k]);
	}

	spk_block *nb = block_new(chunk);
	if (plen) {
		spk_copy_slots(nb->slots, s->ptrs[v->vb - 1]->slots, plen);
	}
	spk_zval_copy(&nb->slots[plen], x);
	nb->phys_len = (uint8_t) (plen + 1);
	spine_push(ns, nb);

	spk_hyb *n = ver_new();
	n->count = v->count + 1;
	n->base = v->base;
	if (n->base) {
		n->base->rc++;
	}
	n->spine = ns;
	n->vb = shared + 1;
	n->lv = (uint8_t) (plen + 1);
	n->shift = v->shift;
	spk_cnt.branches++;
	hyb_release(v);
	return n;
}

/* Append; consumes the caller's reference on v, returns the new version.
 * If v is exclusive (rc==1) the version struct and its container references
 * are reused in place — same observable semantics, no O(vb) re-addref. */
static spk_hyb *hyb_append(spk_hyb *v, const zval *x)
{
	uint32_t chunk = 1u << v->shift;

	if (!v->spine) {
		/* first append: keep the flat base immutable, start a spine */
		spk_spine *ns = spine_new(4);
		spk_block *b = block_new(chunk);
		spk_zval_copy(&b->slots[0], x);
		b->phys_len = 1;
		spine_push(ns, b);
		if (v->rc == 1) {
			v->spine = ns;
			v->vb = 1;
			v->lv = 1;
			v->count++;
			return v;
		}
		spk_hyb *n = ver_new();
		n->count = v->count + 1;
		n->base = v->base;
		if (n->base) {
			n->base->rc++;
		}
		n->spine = ns;
		n->vb = 1;
		n->lv = 1;
		n->shift = v->shift;
		v->rc--;
		return n;
	}

	spk_spine *s = v->spine;
	spk_block *last = s->ptrs[v->vb - 1];
	if (v->vb == s->nphys && v->lv == last->phys_len) {
		/* at the physical tip */
		if (v->lv < chunk) {
			/* extend the current last block at an invisible index */
			spk_zval_copy(&last->slots[last->phys_len], x);
			last->phys_len++;
			if (v->rc == 1) {
				v->lv++;
				v->count++;
				return v;
			}
			spk_hyb *n = hyb_clone_refs(v);
			n->lv++;
			n->count++;
			v->rc--;
			return n;
		}
		/* last block full: append a fresh block to the same spine */
		spk_block *b = block_new(chunk);
		spk_zval_copy(&b->slots[0], x);
		b->phys_len = 1;
		if (v->rc == 1) {
			spine_push(s, b);
			v->vb++;
			v->lv = 1;
			v->count++;
			return v;
		}
		spk_hyb *n = hyb_clone_refs(v); /* refs old vb blocks */
		spine_push(s, b);               /* b's rc=1 ref belongs to n */
		n->vb = v->vb + 1;
		n->lv = 1;
		n->count++;
		v->rc--;
		return n;
	}

	return hyb_branch_append(v, x);
}

static zend_always_inline zval *hyb_get(spk_hyb *v, uint32_t i)
{
	uint32_t bc = v->base ? v->base->count : 0;
	if (i < bc) {
		return &v->base->slots[i];
	}
	uint32_t j = i - bc;
	return &v->spine->ptrs[j >> v->shift]->slots[j & ((1u << v->shift) - 1)];
}

/* ================= generic dispatch ================= */

void *spk_literal(spk_repr r, uint8_t shift, const zval *pool, uint32_t n)
{
	switch (r) {
		case REPR_A: return flat_literal(pool, n);
		case REPR_B: return cap_literal(pool, n);
		default:     return hyb_literal(pool, n, shift);
	}
}

void *spk_append(spk_repr r, void *h, const zval *x)
{
	switch (r) {
		case REPR_A: return flat_append(h, x);
		case REPR_B: return cap_append(h, x);
		default:     return hyb_append(h, x);
	}
}

void *spk_dup(spk_repr r, void *h)
{
	switch (r) {
		case REPR_A: ((spk_flat *) h)->rc++; break;
		case REPR_B: ((spk_cap *) h)->rc++; break;
		default:     ((spk_hyb *) h)->rc++; break;
	}
	return h;
}

void spk_release(spk_repr r, void *h)
{
	switch (r) {
		case REPR_A: flat_release_tag(h, TAG_FLAT); break;
		case REPR_B: cap_release(h); break;
		default:     hyb_release(h); break;
	}
}

zval *spk_get(spk_repr r, void *h, uint32_t i)
{
	switch (r) {
		case REPR_A: return &((spk_flat *) h)->slots[i];
		case REPR_B: return &((spk_cap *) h)->slots[i];
		default:     return hyb_get(h, i);
	}
}

uint32_t spk_count(spk_repr r, void *h)
{
	switch (r) {
		case REPR_A: return ((spk_flat *) h)->count;
		case REPR_B: return ((spk_cap *) h)->count;
		default:     return ((spk_hyb *) h)->count;
	}
}

static zend_always_inline uint64_t sig_light(const zval *z)
{
	switch (Z_TYPE_P(z)) {
		case IS_LONG:   return (uint64_t) Z_LVAL_P(z);
		case IS_STRING: return Z_STRLEN_P(z);
		case IS_OBJECT: return Z_OBJ_HANDLE_P(z);
		case IS_ARRAY:  return zend_hash_num_elements(Z_ARRVAL_P(z));
		default:        return 7;
	}
}

uint64_t spk_iter_sum(spk_repr r, void *h)
{
	uint64_t sum = 0;
	if (r == REPR_A) {
		spk_flat *f = h;
		for (uint32_t i = 0; i < f->count; i++) {
			sum += sig_light(&f->slots[i]);
		}
		return sum;
	}
	if (r == REPR_B) {
		spk_cap *c = h;
		for (uint32_t i = 0; i < c->count; i++) {
			sum += sig_light(&c->slots[i]);
		}
		return sum;
	}
	spk_hyb *v = h;
	if (v->base) {
		const zval *s = v->base->slots;
		uint32_t n = v->base->count;
		for (uint32_t i = 0; i < n; i++) {
			sum += sig_light(&s[i]);
		}
	}
	if (v->spine) {
		uint32_t chunk = 1u << v->shift;
		for (uint32_t k = 0; k < v->vb; k++) {
			const zval *s = v->spine->ptrs[k]->slots;
			uint32_t lim = (k == v->vb - 1) ? v->lv : chunk;
			for (uint32_t i = 0; i < lim; i++) {
				sum += sig_light(&s[i]);
			}
		}
	}
	return sum;
}

/* deep content checksum for cross-representation validation (untimed) */
#define FNV_PRIME 0x100000001b3ull
static uint64_t fnv_bytes(uint64_t h, const void *p, size_t n)
{
	const unsigned char *c = p;
	for (size_t i = 0; i < n; i++) {
		h ^= c[i];
		h *= FNV_PRIME;
	}
	return h;
}

static uint64_t sig_deep(uint64_t h, const zval *z)
{
	switch (Z_TYPE_P(z)) {
		case IS_LONG: {
			zend_long l = Z_LVAL_P(z);
			h ^= 0x11;
			h *= FNV_PRIME;
			return fnv_bytes(h, &l, sizeof(l));
		}
		case IS_STRING:
			h ^= 0x22;
			h *= FNV_PRIME;
			return fnv_bytes(h, Z_STRVAL_P(z), Z_STRLEN_P(z));
		case IS_OBJECT:
			/* object identity is process-local; contribute only a marker so
			 * checksums stay comparable across processes */
			h ^= 0x33;
			h *= FNV_PRIME;
			return h;
		case IS_ARRAY: {
			h ^= 0x44;
			h *= FNV_PRIME;
			zval *e;
			ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(z), e) {
				h = sig_deep(h, e);
			} ZEND_HASH_FOREACH_END();
			return h;
		}
		default:
			h ^= 0x55;
			h *= FNV_PRIME;
			return h;
	}
}

uint64_t spk_deep_checksum(spk_repr r, void *h)
{
	uint64_t cs = 1469598103934665603ull;
	uint32_t n = spk_count(r, h);
	cs = fnv_bytes(cs, &n, sizeof(n));
	for (uint32_t i = 0; i < n; i++) {
		cs = sig_deep(cs, spk_get(r, h, i));
	}
	return cs;
}
