/* Hybrid persistent vec spike — correctness selftest.
 *
 * Strategy:
 *  1. Differential/oracle testing: every logical version is maintained twice,
 *     as a hybrid (C) and as a flat-copy (A, trivially correct). Random op
 *     sequences (append to random live version, dup, drop) are applied to
 *     both; contents are compared by deep checksum and point reads. This
 *     covers snapshot isolation, branch independence and "older snapshots
 *     cannot observe later values" by construction.
 *  2. Deterministic scenarios for ownership, retention bounds and branch copy
 *     costs, asserted against the instrumentation counters.
 *  3. Balance invariants after every scenario teardown: every container
 *     allocation freed exactly once, every container-owned zval released
 *     exactly once, element refcounts back to pool-only.
 */

#include "vec_spike.h"

typedef struct {
	int   ok;
	char  msg[512];
} st_result;

#define ST_FAIL(res, ...) do { \
	if ((res)->ok) { \
		(res)->ok = 0; \
		snprintf((res)->msg, sizeof((res)->msg), __VA_ARGS__); \
	} \
} while (0)

static uint64_t st_rng_state;
static uint64_t st_rng(void)
{
	uint64_t x = st_rng_state;
	x ^= x << 13; x ^= x >> 7; x ^= x << 17;
	return st_rng_state = x;
}

static void st_check_balance(st_result *r, const char *ctx)
{
	if (spk_cnt.allocs != spk_cnt.frees) {
		ST_FAIL(r, "%s: alloc/free imbalance %llu != %llu", ctx,
			(unsigned long long) spk_cnt.allocs, (unsigned long long) spk_cnt.frees);
	}
	if (spk_cnt.zval_copies != spk_cnt.zval_dtors) {
		ST_FAIL(r, "%s: zval copy/dtor imbalance %llu != %llu", ctx,
			(unsigned long long) spk_cnt.zval_copies, (unsigned long long) spk_cnt.zval_dtors);
	}
	if (spk_cnt.block_allocs != spk_cnt.block_frees) {
		ST_FAIL(r, "%s: block leak %llu != %llu", ctx,
			(unsigned long long) spk_cnt.block_allocs, (unsigned long long) spk_cnt.block_frees);
	}
	if (spk_cnt.spine_allocs != spk_cnt.spine_frees) {
		ST_FAIL(r, "%s: spine leak", ctx);
	}
	if (spk_cnt.version_allocs != spk_cnt.version_frees) {
		ST_FAIL(r, "%s: version leak", ctx);
	}
	if (spk_cnt.rc_addref != spk_cnt.rc_delref) {
		ST_FAIL(r, "%s: element rc imbalance %llu != %llu", ctx,
			(unsigned long long) spk_cnt.rc_addref, (unsigned long long) spk_cnt.rc_delref);
	}
}

/* --- 1. differential random-op test: hybrid vs flat oracle --- */

#define ST_MAX_LIVE 24

typedef struct {
	spk_hyb  *c;
	spk_flat *a;
	int active;
} st_pair;

static void st_pair_drop(st_pair *p)
{
	if (p->active) {
		spk_release(REPR_C, p->c);
		spk_release(REPR_A, p->a);
		p->active = 0;
	}
}

static void st_pair_verify(st_result *r, st_pair *p, const char *ctx)
{
	uint32_t nc = spk_count(REPR_C, p->c);
	uint32_t na = spk_count(REPR_A, p->a);
	if (nc != na) {
		ST_FAIL(r, "%s: count mismatch C=%u A=%u", ctx, nc, na);
		return;
	}
	if (spk_deep_checksum(REPR_C, p->c) != spk_deep_checksum(REPR_A, p->a)) {
		ST_FAIL(r, "%s: checksum mismatch at count=%u", ctx, nc);
		return;
	}
	/* invariant: count == base + (vb-1)*chunk + lv */
	spk_hyb *v = p->c;
	uint32_t bc = v->base ? v->base->count : 0;
	if (v->spine) {
		uint32_t chunk = 1u << v->shift;
		if (v->vb < 1 || v->count != bc + (v->vb - 1) * chunk + v->lv) {
			ST_FAIL(r, "%s: prefix invariant broken count=%u bc=%u vb=%u lv=%u",
				ctx, v->count, bc, v->vb, v->lv);
		}
		if (v->lv > v->spine->ptrs[v->vb - 1]->phys_len) {
			ST_FAIL(r, "%s: lv beyond phys_len", ctx);
		}
		for (uint32_t k = 0; k + 1 < v->vb; k++) {
			if (v->spine->ptrs[k]->phys_len != chunk) {
				ST_FAIL(r, "%s: non-last visible block not full", ctx);
			}
		}
	} else if (v->count != bc || v->vb != 0) {
		ST_FAIL(r, "%s: flat-version invariant broken", ctx);
	}
	/* random point reads */
	for (int t = 0; t < 8 && nc; t++) {
		uint32_t i = (uint32_t) (st_rng() % nc);
		zval *zc = spk_get(REPR_C, p->c, i);
		zval *za = spk_get(REPR_A, p->a, i);
		if (Z_TYPE_P(zc) != Z_TYPE_P(za) ||
			(Z_TYPE_P(zc) == IS_LONG && Z_LVAL_P(zc) != Z_LVAL_P(za)) ||
			(Z_TYPE_P(zc) == IS_STRING &&
			 !zend_string_equals(Z_STR_P(zc), Z_STR_P(za)))) {
			ST_FAIL(r, "%s: point read mismatch at %u", ctx, i);
		}
	}
}

static void st_random_ops(st_result *r, uint8_t shift, uint64_t seed, int steps)
{
	memset(&spk_cnt, 0, sizeof(spk_cnt));
	st_rng_state = seed | 1;
	st_pair live[ST_MAX_LIVE];
	memset(live, 0, sizeof(live));

	/* seed version: literal of random small size, string elements */
	uint32_t n0 = (uint32_t) (st_rng() % 7);
	zval *pool = spk_pool_build(EL_STR, n0 ? n0 : 1);
	live[0].c = spk_literal(REPR_C, shift, pool, n0);
	live[0].a = spk_literal(REPR_A, shift, pool, n0);
	live[0].active = 1;
	spk_pool_free(pool, n0 ? n0 : 1);

	uint32_t next_val = 1000;
	for (int s = 0; s < steps && r->ok; s++) {
		int slot = (int) (st_rng() % ST_MAX_LIVE);
		if (!live[slot].active) {
			/* clone some active version into this slot (handle dup) */
			int src = -1;
			for (int k = 0; k < ST_MAX_LIVE; k++) {
				if (live[(slot + k) % ST_MAX_LIVE].active) {
					src = (slot + k) % ST_MAX_LIVE;
					break;
				}
			}
			live[slot].c = spk_dup(REPR_C, live[src].c);
			live[slot].a = spk_dup(REPR_A, live[src].a);
			live[slot].active = 1;
			continue;
		}
		uint64_t op = st_rng() % 100;
		if (op < 70) {
			/* append: this hits tip-extend, new-block, first-append and
			 * branch paths depending on version topology and sharing */
			zval x;
			ZVAL_STR(&x, zend_strpprintf(0, "v%u", next_val++));
			live[slot].c = spk_append(REPR_C, live[slot].c, &x);
			live[slot].a = spk_append(REPR_A, live[slot].a, &x);
			zval_ptr_dtor(&x);
		} else if (op < 85) {
			int nactive = 0;
			for (int k = 0; k < ST_MAX_LIVE; k++) nactive += live[k].active;
			if (nactive > 1) {
				st_pair_drop(&live[slot]);
			}
		} else {
			st_pair_verify(r, &live[slot], "random-op");
		}
	}
	/* final: verify everything, then tear down in random order */
	for (int k = 0; k < ST_MAX_LIVE && r->ok; k++) {
		if (live[k].active) {
			st_pair_verify(r, &live[k], "final-verify");
		}
	}
	for (int k = ST_MAX_LIVE - 1; k >= 0; k--) {
		st_pair_drop(&live[k]);
	}
	st_check_balance(r, "random-ops");
}

/* --- 2. deterministic scenarios --- */

/* invisible retention bounded by one block */
static void st_retention(st_result *r)
{
	memset(&spk_cnt, 0, sizeof(spk_cnt));
	const uint8_t shift = 4; /* chunk 16 */
	zval *pool = spk_pool_build(EL_STR, 64);

	spk_hyb *snap = NULL;
	spk_hyb *h = spk_literal(REPR_C, shift, pool, 4);
	for (uint32_t i = 0; i < 40; i++) {
		if (spk_count(REPR_C, h) == 14 && !snap) {
			snap = spk_dup(REPR_C, h);
		}
		h = spk_append(REPR_C, h, &pool[4 + i]);
	}
	if (!snap) {
		ST_FAIL(r, "retention: snapshot not taken");
		spk_release(REPR_C, h);
		spk_pool_free(pool, 64);
		return;
	}
	/* tip now at count 44 (40 appended = 2 full blocks + lv 8 of third) */
	uint64_t blocks_live_before = spk_cnt.block_allocs - spk_cnt.block_frees;
	if (blocks_live_before != 3) {
		ST_FAIL(r, "retention: expected 3 live blocks at tip, got %llu",
			(unsigned long long) blocks_live_before);
	}
	spk_release(REPR_C, h); /* drop tip; snapshot (count 14, vb=1, lv=10) survives */
	uint64_t blocks_live = spk_cnt.block_allocs - spk_cnt.block_frees;
	if (blocks_live != 1) {
		ST_FAIL(r, "retention: old snapshot retains %llu blocks, want 1",
			(unsigned long long) blocks_live);
	}
	uint64_t live_slots = spk_cnt.zval_copies - spk_cnt.zval_dtors;
	/* base 4 + one block physically filled to 16 */
	if (live_slots != 4 + 16) {
		ST_FAIL(r, "retention: live slots %llu, want 20",
			(unsigned long long) live_slots);
	}
	uint64_t invisible = live_slots - spk_count(REPR_C, snap);
	if (invisible > (1u << shift)) {
		ST_FAIL(r, "retention: invisible %llu slots exceeds one block",
			(unsigned long long) invisible);
	}
	/* snapshot still reads its own values only */
	if (spk_count(REPR_C, snap) != 14) {
		ST_FAIL(r, "retention: snapshot count changed");
	}
	spk_release(REPR_C, snap);
	spk_pool_free(pool, 64);
	st_check_balance(r, "retention");
}

/* branch copy cost: partial-block branch copies lv+1 zvals; boundary branch
 * copies exactly 1 (the appended value) */
static void st_branch_cost(st_result *r)
{
	memset(&spk_cnt, 0, sizeof(spk_cnt));
	const uint8_t shift = 4;
	const uint32_t chunk = 16;
	zval *pool = spk_pool_build(EL_INT, 256);

	/* partial: snapshot at lv = 5 */
	spk_hyb *h = spk_literal(REPR_C, shift, pool, 8);
	for (uint32_t i = 0; i < chunk + 5; i++) {
		h = spk_append(REPR_C, h, &pool[8 + i]);
	}
	spk_hyb *snap = spk_dup(REPR_C, h);
	h = spk_append(REPR_C, h, &pool[100]); /* tip moves past snapshot */
	uint64_t copies0 = spk_cnt.zval_copies;
	uint64_t branches0 = spk_cnt.branches;
	spk_hyb *b = spk_append(REPR_C, snap, &pool[101]); /* consumes snap: branch */
	if (spk_cnt.branches != branches0 + 1) {
		ST_FAIL(r, "branch-cost: expected branch path");
	}
	if (spk_cnt.zval_copies - copies0 != 5 + 1) {
		ST_FAIL(r, "branch-cost: partial branch copied %llu zvals, want 6",
			(unsigned long long) (spk_cnt.zval_copies - copies0));
	}
	if (spk_count(REPR_C, b) != 8 + chunk + 5 + 1) {
		ST_FAIL(r, "branch-cost: branch count wrong");
	}
	/* tip unaffected */
	if (spk_count(REPR_C, h) != 8 + chunk + 5 + 1) {
		ST_FAIL(r, "branch-cost: tip count wrong");
	}
	zval *tip_last = spk_get(REPR_C, h, spk_count(REPR_C, h) - 1);
	zval *br_last = spk_get(REPR_C, b, spk_count(REPR_C, b) - 1);
	if (Z_LVAL_P(tip_last) != Z_LVAL_P(&pool[100]) ||
		Z_LVAL_P(br_last) != Z_LVAL_P(&pool[101])) {
		ST_FAIL(r, "branch-cost: divergent tails wrong");
	}
	spk_release(REPR_C, b);

	/* boundary: snapshot exactly at a full block edge shares all blocks */
	spk_hyb *edge = NULL;
	spk_release(REPR_C, h);
	h = spk_literal(REPR_C, shift, pool, 8);
	for (uint32_t i = 0; i < 2 * chunk; i++) {
		h = spk_append(REPR_C, h, &pool[8 + i]);
	}
	edge = spk_dup(REPR_C, h); /* lv == chunk exactly */
	h = spk_append(REPR_C, h, &pool[120]); /* new physical block */
	copies0 = spk_cnt.zval_copies;
	uint64_t blk0 = spk_cnt.block_allocs;
	spk_hyb *b2 = spk_append(REPR_C, edge, &pool[121]);
	if (spk_cnt.zval_copies - copies0 != 1) {
		ST_FAIL(r, "branch-cost: boundary branch copied %llu zvals, want 1",
			(unsigned long long) (spk_cnt.zval_copies - copies0));
	}
	if (spk_cnt.block_allocs - blk0 != 1) {
		ST_FAIL(r, "branch-cost: boundary branch allocated wrong block count");
	}
	spk_release(REPR_C, b2);
	spk_release(REPR_C, h);
	spk_pool_free(pool, 256);
	st_check_balance(r, "branch-cost");
}

/* element refcount audit: a specific string's refcount returns to pool-only */
static void st_rc_audit(st_result *r)
{
	memset(&spk_cnt, 0, sizeof(spk_cnt));
	zval probe;
	ZVAL_STR(&probe, zend_string_init("probe-string-xyz", 16, 0));
	uint32_t rc0 = GC_REFCOUNT(Z_COUNTED(probe)); /* == 1 */

	zval *pool = spk_pool_build(EL_STR, 40);
	spk_hyb *h = spk_literal(REPR_C, 2, pool, 4); /* chunk 4 */
	h = spk_append(REPR_C, h, &probe);            /* +1 ref */
	spk_hyb *snap = spk_dup(REPR_C, h);
	h = spk_append(REPR_C, h, &pool[10]);
	spk_hyb *branch = spk_append(REPR_C, snap, &probe); /* branch copies the
		probe slot (+1) and appends probe again (+1) */
	uint32_t rc_mid = GC_REFCOUNT(Z_COUNTED(probe));
	if (rc_mid != rc0 + 3) {
		ST_FAIL(r, "rc-audit: expected rc %u mid, got %u", rc0 + 3, rc_mid);
	}
	spk_release(REPR_C, h);
	spk_release(REPR_C, branch);
	uint32_t rc_end = GC_REFCOUNT(Z_COUNTED(probe));
	if (rc_end != rc0) {
		ST_FAIL(r, "rc-audit: expected rc back to %u, got %u", rc0, rc_end);
	}
	spk_pool_free(pool, 40);
	zval_ptr_dtor(&probe);
	st_check_balance(r, "rc-audit");
}

/* B semantics: in-place when exclusive with capacity, copy when shared */
static void st_capacity_model(st_result *r)
{
	memset(&spk_cnt, 0, sizeof(spk_cnt));
	zval *pool = spk_pool_build(EL_STR, 64);
	spk_cap *c = spk_literal(REPR_B, 0, pool, 4); /* cap == 4 exact */
	uint64_t a0 = spk_cnt.allocs;
	c = spk_append(REPR_B, c, &pool[10]); /* full: grow-move */
	if (spk_cnt.allocs - a0 != 1 || spk_cnt.zval_moves != 4) {
		ST_FAIL(r, "capacity: grow-move expected 1 alloc + 4 moves");
	}
	a0 = spk_cnt.allocs;
	uint64_t cp0 = spk_cnt.zval_copies;
	c = spk_append(REPR_B, c, &pool[11]); /* cap 8, rc1: in place */
	if (spk_cnt.allocs != a0 || spk_cnt.zval_copies - cp0 != 1) {
		ST_FAIL(r, "capacity: in-place append expected 0 allocs");
	}
	spk_cap *keep = spk_dup(REPR_B, c);
	cp0 = spk_cnt.zval_copies;
	c = spk_append(REPR_B, c, &pool[12]); /* shared: full copy */
	if (spk_cnt.zval_copies - cp0 != 6 + 1) {
		ST_FAIL(r, "capacity: shared append should copy all %u + 1", 6);
	}
	if (spk_count(REPR_B, keep) != 6 || spk_count(REPR_B, c) != 7) {
		ST_FAIL(r, "capacity: snapshot isolation broken");
	}
	spk_release(REPR_B, keep);
	spk_release(REPR_B, c);
	spk_pool_free(pool, 64);
	st_check_balance(r, "capacity");
}

/* empty literal, first append on shared literal, two independent spines */
static void st_first_append(st_result *r)
{
	memset(&spk_cnt, 0, sizeof(spk_cnt));
	zval *pool = spk_pool_build(EL_STR, 8);
	spk_hyb *e = spk_literal(REPR_C, 3, NULL, 0);
	if (spk_count(REPR_C, e) != 0 || e->base != NULL) {
		ST_FAIL(r, "first-append: empty literal malformed");
	}
	spk_hyb *lit = spk_literal(REPR_C, 3, pool, 3);
	spk_hyb *w = spk_append(REPR_C, spk_dup(REPR_C, lit), &pool[4]);
	spk_hyb *x = spk_append(REPR_C, spk_dup(REPR_C, lit), &pool[5]);
	/* two independent spines off the same shared base */
	if (w->spine == x->spine || w->base != x->base || lit->spine != NULL) {
		ST_FAIL(r, "first-append: spine sharing wrong");
	}
	if (lit->base->rc != 3) {
		ST_FAIL(r, "first-append: base rc %u, want 3", lit->base->rc);
	}
	zval *zw = spk_get(REPR_C, w, 3);
	zval *zx = spk_get(REPR_C, x, 3);
	if (zend_string_equals(Z_STR_P(zw), Z_STR_P(zx))) {
		ST_FAIL(r, "first-append: branches not independent");
	}
	e = spk_append(REPR_C, e, &pool[6]);
	if (spk_count(REPR_C, e) != 1 || e->base != NULL || e->vb != 1) {
		ST_FAIL(r, "first-append: append on empty literal wrong");
	}
	spk_release(REPR_C, e);
	spk_release(REPR_C, lit);
	spk_release(REPR_C, w);
	spk_release(REPR_C, x);
	spk_pool_free(pool, 8);
	st_check_balance(r, "first-append");
}

/* entry point used by the PHP-visible function */
void vec_spike_selftest_run(zval *return_value)
{
	array_init(return_value);
	zval fails;
	array_init(&fails);
	int all_ok = 1;
	uint64_t total_branches = 0;

	static const uint8_t shifts[] = {1, 2, 4, 7};
	for (size_t si = 0; si < sizeof(shifts); si++) {
		for (uint64_t seed = 1; seed <= 3; seed++) {
			st_result r = { .ok = 1 };
			st_random_ops(&r, shifts[si], seed * 0x9e3779b97f4a7c15ull, 1500);
			total_branches += spk_cnt.branches;
			if (!r.ok) {
				all_ok = 0;
				char buf[600];
				snprintf(buf, sizeof(buf), "random shift=%u seed=%llu: %s",
					shifts[si], (unsigned long long) seed, r.msg);
				add_next_index_string(&fails, buf);
			}
		}
	}

	struct { const char *name; void (*fn)(st_result *); } dets[] = {
		{ "retention", st_retention },
		{ "branch-cost", st_branch_cost },
		{ "rc-audit", st_rc_audit },
		{ "capacity-model", st_capacity_model },
		{ "first-append", st_first_append },
	};
	for (size_t i = 0; i < sizeof(dets) / sizeof(dets[0]); i++) {
		st_result r = { .ok = 1 };
		dets[i].fn(&r);
		if (!r.ok) {
			all_ok = 0;
			char buf[600];
			snprintf(buf, sizeof(buf), "%s: %s", dets[i].name, r.msg);
			add_next_index_string(&fails, buf);
		}
	}

	add_assoc_bool(return_value, "ok", all_ok);
	add_assoc_long(return_value, "random_branches_exercised", (zend_long) total_branches);
	add_assoc_zval(return_value, "failures", &fails);
}
