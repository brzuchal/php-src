/* Hybrid persistent vec spike — benchmark workloads + PHP entry points. */

#include "vec_spike.h"
#include <time.h>

void vec_spike_selftest_run(zval *return_value); /* vec_spike_test.c */

static zend_always_inline uint64_t spk_now(void)
{
#ifdef __APPLE__
	return clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
#else
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t) ts.tv_sec * 1000000000ull + (uint64_t) ts.tv_nsec;
#endif
}

static uint64_t bn_rng_state = 0x853c49e6748fea9bull;
static zend_always_inline uint64_t bn_rng(void)
{
	uint64_t x = bn_rng_state;
	x ^= x << 13; x ^= x >> 7; x ^= x << 17;
	return bn_rng_state = x;
}

/* ---------------- spec + result plumbing ---------------- */

#define SPK_MAX_SAMPLES 256

typedef struct {
	spk_repr repr;
	uint8_t  shift;
	spk_el   elem;
	uint32_t n, base_n, appended, blocks, forks, depth, per;
	uint32_t samples, warmup;
	uint64_t target_us, batch_cap, seed;
	int      retained;
	int      keep_all;
	const char *region;
	const char *lv_pos;
} spk_spec;

typedef struct {
	double   samples[SPK_MAX_SAMPLES];
	double   samples2[SPK_MAX_SAMPLES];
	uint32_t nsamples, nsamples2;
	uint64_t ops_per_sample;
	uint64_t batch;
	uint64_t checksum;
	spk_counters c_before, c_after; /* around last timed region */
	size_t   mem_build;             /* usage right after last timed build */
	const char *err;
	const char *extra_k[10];
	uint64_t    extra_v[10];
	int         extra_n;
} bench_out;

static void out_extra(bench_out *o, const char *k, uint64_t v)
{
	if (o->extra_n < 10) {
		o->extra_k[o->extra_n] = k;
		o->extra_v[o->extra_n] = v;
		o->extra_n++;
	}
}

static zend_long spec_long(HashTable *ht, const char *k, zend_long def)
{
	zval *z = zend_hash_str_find(ht, k, strlen(k));
	return z ? zval_get_long(z) : def;
}

static const char *spec_str(HashTable *ht, const char *k, const char *def)
{
	zval *z = zend_hash_str_find(ht, k, strlen(k));
	return (z && Z_TYPE_P(z) == IS_STRING) ? Z_STRVAL_P(z) : def;
}

static uint64_t calibrate_batch(uint64_t unit_ns, uint64_t target_us, uint64_t cap)
{
	if (unit_ns == 0) unit_ns = 1;
	uint64_t b = target_us * 1000 / unit_ns;
	if (b < 1) b = 1;
	if (cap && b > cap) b = cap;
	return b;
}

static void rec_sample(bench_out *o, const spk_spec *sp, uint32_t it, double v)
{
	if (it >= sp->warmup && o->nsamples < SPK_MAX_SAMPLES) {
		o->samples[o->nsamples++] = v;
	}
}

static void rec_sample2(bench_out *o, const spk_spec *sp, uint32_t it, double v)
{
	if (it >= sp->warmup && o->nsamples2 < SPK_MAX_SAMPLES) {
		o->samples2[o->nsamples2++] = v;
	}
}

/* ---------------- workload 1+2: linear append ---------------- */

static void wl_linear(const spk_spec *sp, bench_out *o)
{
	uint32_t n = sp->n;
	zval *pool = spk_pool_build(sp->elem, n);
	void **keep = sp->retained ? safe_emalloc(n, sizeof(void *), 0) : NULL;
	uint32_t iters = sp->warmup + sp->samples;

	for (uint32_t it = 0; it < iters; it++) {
		int last = (it == iters - 1);
		if (last) o->c_before = spk_cnt;
		uint64_t t0 = spk_now();
		void *h = spk_literal(sp->repr, sp->shift, NULL, 0);
		if (sp->retained) {
			for (uint32_t i = 0; i < n; i++) {
				void *pass = spk_dup(sp->repr, h);
				void *nh = spk_append(sp->repr, pass, &pool[i]);
				keep[i] = h;
				h = nh;
			}
		} else {
			for (uint32_t i = 0; i < n; i++) {
				h = spk_append(sp->repr, h, &pool[i]);
			}
		}
		uint64_t t1 = spk_now();
		if (last) {
			o->c_after = spk_cnt;
			o->mem_build = zend_memory_usage(0);
			o->checksum = spk_deep_checksum(sp->repr, h);
		}
		if (sp->retained) {
			for (uint32_t i = 0; i < n; i++) {
				spk_release(sp->repr, keep[i]);
			}
		}
		spk_release(sp->repr, h);
		rec_sample(o, sp, it, (double) (t1 - t0) / n);
	}
	o->ops_per_sample = n;
	o->batch = 1;
	if (keep) efree(keep);
	spk_pool_free(pool, n);
}

/* ---------------- workload 4: large base + append ---------------- */

static void wl_base_append(const spk_spec *sp, bench_out *o)
{
	uint32_t base = sp->base_n, k = sp->appended;
	zval *pool = spk_pool_build(sp->elem, base + k);
	void *lit = spk_literal(sp->repr, sp->shift, pool, base); /* retained */

	/* one unit = dup(lit) + k appends (intermediates dead) */
	uint64_t u0 = spk_now();
	void *uh = spk_dup(sp->repr, lit);
	for (uint32_t j = 0; j < k; j++) uh = spk_append(sp->repr, uh, &pool[base + j]);
	uint64_t unit_ns = spk_now() - u0;
	spk_release(sp->repr, uh);
	uint64_t batch = calibrate_batch(unit_ns, sp->target_us, sp->batch_cap);
	void **scratch = safe_emalloc(batch, sizeof(void *), 0);

	uint32_t iters = sp->warmup + sp->samples;
	for (uint32_t it = 0; it < iters; it++) {
		int last = (it == iters - 1);
		if (last) o->c_before = spk_cnt;
		uint64_t t0 = spk_now();
		for (uint64_t b = 0; b < batch; b++) {
			void *h = spk_dup(sp->repr, lit);
			for (uint32_t j = 0; j < k; j++) {
				h = spk_append(sp->repr, h, &pool[base + j]);
			}
			scratch[b] = h;
		}
		uint64_t t1 = spk_now();
		if (last) {
			o->c_after = spk_cnt;
			o->mem_build = zend_memory_usage(0);
			o->checksum = spk_deep_checksum(sp->repr, scratch[0]);
		}
		for (uint64_t b = 0; b < batch; b++) spk_release(sp->repr, scratch[b]);
		rec_sample(o, sp, it, (double) (t1 - t0) / ((double) batch * k));
	}
	o->ops_per_sample = batch * k;
	o->batch = batch;
	efree(scratch);
	spk_release(sp->repr, lit);
	spk_pool_free(pool, base + k);
}

/* ---------------- workload 3: branch from an older version ---------------- */

static void wl_branch(const spk_spec *sp, bench_out *o)
{
	uint32_t chunk = 1u << sp->shift;
	uint32_t base = sp->base_n;
	uint32_t lv_target = chunk / 2;
	if (!strcmp(sp->lv_pos, "worst")) lv_target = chunk - 1;
	else if (!strcmp(sp->lv_pos, "boundary")) lv_target = chunk;
	uint32_t p_app = 4 * chunk + lv_target;
	zval *pool = spk_pool_build(sp->elem, base + p_app + 4);

	void *S;
	if (sp->repr == REPR_C) {
		void *h = spk_literal(REPR_C, sp->shift, pool, base);
		for (uint32_t i = 0; i < p_app; i++) {
			h = spk_append(REPR_C, h, &pool[base + i]);
		}
		S = h;
		/* move the physical tip past S so appending to S must branch */
		void *t = spk_append(REPR_C, spk_dup(REPR_C, S), &pool[base + p_app]);
		spk_release(REPR_C, t);
	} else {
		S = spk_literal(sp->repr, sp->shift, pool, base + p_app);
	}

	uint64_t u0 = spk_now();
	void *uh = spk_append(sp->repr, spk_dup(sp->repr, S), &pool[base + p_app + 1]);
	uint64_t unit_ns = spk_now() - u0;
	spk_release(sp->repr, uh);
	uint64_t batch = calibrate_batch(unit_ns, sp->target_us, sp->batch_cap);
	void **scratch = safe_emalloc(batch, sizeof(void *), 0);

	uint32_t iters = sp->warmup + sp->samples;
	for (uint32_t it = 0; it < iters; it++) {
		int last = (it == iters - 1);
		if (last) o->c_before = spk_cnt;
		uint64_t t0 = spk_now();
		for (uint64_t b = 0; b < batch; b++) {
			scratch[b] = spk_append(sp->repr, spk_dup(sp->repr, S),
				&pool[base + p_app + 2]);
		}
		uint64_t t1 = spk_now();
		if (last) {
			o->c_after = spk_cnt;
			o->mem_build = zend_memory_usage(0);
			o->checksum = spk_deep_checksum(sp->repr, scratch[0]);
		}
		for (uint64_t b = 0; b < batch; b++) spk_release(sp->repr, scratch[b]);
		rec_sample(o, sp, it, (double) (t1 - t0) / batch);
	}
	o->ops_per_sample = batch;
	o->batch = batch;
	out_extra(o, "lv_at_branch", lv_target);
	efree(scratch);
	spk_release(sp->repr, S);
	spk_pool_free(pool, base + p_app + 4);
}

/* ---------------- workload 5: indexed read ---------------- */

#define NIDX 8192

static void wl_read(const spk_spec *sp, bench_out *o)
{
	uint32_t chunk = 1u << sp->shift;
	uint32_t base = sp->base_n, blocks = sp->blocks;
	uint32_t total = base + blocks * chunk;
	zval *pool = spk_pool_build(sp->elem, total);

	void *h;
	if (sp->repr == REPR_C) {
		h = spk_literal(REPR_C, sp->shift, pool, base);
		for (uint32_t i = 0; i < blocks * chunk; i++) {
			h = spk_append(REPR_C, h, &pool[base + i]);
		}
	} else {
		h = spk_literal(sp->repr, sp->shift, pool, total);
	}

	uint32_t lo = 0, hi = total;
	if (!strcmp(sp->region, "base")) { lo = 0; hi = base; }
	else if (!strcmp(sp->region, "full")) { lo = base; hi = base + (blocks - 1) * chunk; }
	else if (!strcmp(sp->region, "last")) { lo = total - chunk; hi = total; }
	uint32_t *idx = safe_emalloc(NIDX, sizeof(uint32_t), 0);
	bn_rng_state = sp->seed | 1;
	for (uint32_t i = 0; i < NIDX; i++) {
		idx[i] = lo + (uint32_t) (bn_rng() % (hi - lo));
	}

	uint64_t batch = calibrate_batch(4, sp->target_us, sp->batch_cap);
	uint32_t iters = sp->warmup + sp->samples;
	uint64_t sum = 0;
	for (uint32_t it = 0; it < iters; it++) {
		int last = (it == iters - 1);
		if (last) o->c_before = spk_cnt;
		uint64_t t0 = spk_now();
		switch (sp->repr) {
			case REPR_A: {
				spk_flat *f = h;
				for (uint64_t r = 0; r < batch; r++) {
					const zval *z = &f->slots[idx[r & (NIDX - 1)]];
					sum += Z_TYPE_P(z) == IS_LONG ? (uint64_t) Z_LVAL_P(z) : 1;
				}
				break;
			}
			case REPR_B: {
				spk_cap *c = h;
				for (uint64_t r = 0; r < batch; r++) {
					const zval *z = &c->slots[idx[r & (NIDX - 1)]];
					sum += Z_TYPE_P(z) == IS_LONG ? (uint64_t) Z_LVAL_P(z) : 1;
				}
				break;
			}
			default: {
				spk_hyb *v = h;
				uint32_t bc = v->base ? v->base->count : 0;
				uint8_t shift = v->shift;
				uint32_t mask = (1u << shift) - 1;
				for (uint64_t r = 0; r < batch; r++) {
					uint32_t i = idx[r & (NIDX - 1)];
					const zval *z;
					if (i < bc) {
						z = &v->base->slots[i];
					} else {
						uint32_t j = i - bc;
						z = &v->spine->ptrs[j >> shift]->slots[j & mask];
					}
					sum += Z_TYPE_P(z) == IS_LONG ? (uint64_t) Z_LVAL_P(z) : 1;
				}
				break;
			}
		}
		uint64_t t1 = spk_now();
		if (last) {
			o->c_after = spk_cnt;
			o->mem_build = zend_memory_usage(0);
		}
		rec_sample(o, sp, it, (double) (t1 - t0) / batch);
	}
	o->checksum = sum; /* DCE sink; equal across reprs for same seed+region */
	o->ops_per_sample = batch;
	o->batch = batch;
	efree(idx);
	spk_release(sp->repr, h);
	spk_pool_free(pool, total);
}

/* ---------------- workload 6: foreach ---------------- */

static void wl_foreach(const spk_spec *sp, bench_out *o)
{
	uint32_t chunk = 1u << sp->shift;
	uint32_t base = sp->base_n, blocks = sp->blocks;
	uint32_t total = base + blocks * chunk;
	zval *pool = spk_pool_build(sp->elem, total);

	void *h;
	if (sp->repr == REPR_C) {
		h = spk_literal(REPR_C, sp->shift, pool, base);
		for (uint32_t i = 0; i < blocks * chunk; i++) {
			h = spk_append(REPR_C, h, &pool[base + i]);
		}
	} else {
		h = spk_literal(sp->repr, sp->shift, pool, total);
	}

	uint64_t u0 = spk_now();
	uint64_t sum = spk_iter_sum(sp->repr, h);
	uint64_t unit_ns = spk_now() - u0;
	uint64_t batch = calibrate_batch(unit_ns, sp->target_us, sp->batch_cap);

	uint32_t iters = sp->warmup + sp->samples;
	for (uint32_t it = 0; it < iters; it++) {
		int last = (it == iters - 1);
		if (last) o->c_before = spk_cnt;
		uint64_t t0 = spk_now();
		for (uint64_t b = 0; b < batch; b++) {
			sum += spk_iter_sum(sp->repr, h);
		}
		uint64_t t1 = spk_now();
		if (last) o->c_after = spk_cnt;
		rec_sample(o, sp, it, (double) (t1 - t0) / ((double) batch * total));
	}
	/* DCE sink consumed via extra; checksum must be batch-independent so the
	 * driver can compare content across representations and chunk sizes */
	out_extra(o, "sink", sum);
	o->checksum = spk_iter_sum(sp->repr, h);
	o->ops_per_sample = (uint64_t) batch * total;
	o->batch = batch;
	spk_release(sp->repr, h);
	spk_pool_free(pool, total);
}

/* -------- workload 7: version creation at vb blocks (boundary branch) ------ */

static void wl_vcreate(const spk_spec *sp, bench_out *o)
{
	uint32_t chunk = 1u << sp->shift;
	uint32_t vb = sp->blocks;
	uint32_t base = sp->base_n;
	uint32_t p_app = vb * chunk; /* snapshot exactly at a block boundary */
	zval *pool = spk_pool_build(sp->elem, base + p_app + 4);

	void *h = spk_literal(REPR_C, sp->shift, pool, base);
	for (uint32_t i = 0; i < p_app; i++) {
		h = spk_append(REPR_C, h, &pool[base + i]);
	}
	void *S = h;
	void *t = spk_append(REPR_C, spk_dup(REPR_C, S), &pool[base + p_app]);
	spk_release(REPR_C, t); /* S is no longer at the physical tip */

	uint64_t u0 = spk_now();
	void *uh = spk_append(REPR_C, spk_dup(REPR_C, S), &pool[base + p_app + 1]);
	uint64_t unit_ns = spk_now() - u0;
	spk_release(REPR_C, uh);
	uint64_t batch = calibrate_batch(unit_ns * 2, sp->target_us, sp->batch_cap);
	void **scratch = safe_emalloc(batch, sizeof(void *), 0);

	uint32_t iters = sp->warmup + sp->samples;
	for (uint32_t it = 0; it < iters; it++) {
		int last = (it == iters - 1);
		if (last) o->c_before = spk_cnt;
		uint64_t t0 = spk_now();
		for (uint64_t b = 0; b < batch; b++) {
			scratch[b] = spk_append(REPR_C, spk_dup(REPR_C, S),
				&pool[base + p_app + 2]);
		}
		uint64_t t1 = spk_now();
		for (uint64_t b = 0; b < batch; b++) {
			spk_release(REPR_C, scratch[b]);
		}
		uint64_t t2 = spk_now();
		if (last) {
			o->c_after = spk_cnt;
			o->checksum = 0;
		}
		rec_sample(o, sp, it, (double) (t1 - t0) / batch);
		rec_sample2(o, sp, it, (double) (t2 - t1) / batch);
	}
	o->ops_per_sample = batch;
	o->batch = batch;
	efree(scratch);
	spk_release(REPR_C, S);
	spk_pool_free(pool, base + p_app + 4);
}

/* ---------------- workload 8: destruction ---------------- */

static void wl_destroy(const spk_spec *sp, bench_out *o)
{
	uint32_t n = sp->n;
	zval *pool = spk_pool_build(sp->elem, n);
	void **keep = sp->retained ? safe_emalloc(n, sizeof(void *), 0) : NULL;
	uint32_t iters = sp->warmup + sp->samples;

	for (uint32_t it = 0; it < iters; it++) {
		int last = (it == iters - 1);
		/* build (untimed) */
		void *h = spk_literal(sp->repr, sp->shift, NULL, 0);
		if (sp->retained) {
			for (uint32_t i = 0; i < n; i++) {
				void *pass = spk_dup(sp->repr, h);
				void *nh = spk_append(sp->repr, pass, &pool[i]);
				keep[i] = h;
				h = nh;
			}
		} else {
			for (uint32_t i = 0; i < n; i++) {
				h = spk_append(sp->repr, h, &pool[i]);
			}
		}
		if (last) {
			o->mem_build = zend_memory_usage(0);
			o->c_before = spk_cnt;
		}
		uint64_t t0 = spk_now();
		if (sp->retained) {
			for (uint32_t i = 0; i < n; i++) {
				spk_release(sp->repr, keep[i]);
			}
		}
		spk_release(sp->repr, h);
		uint64_t t1 = spk_now();
		if (last) o->c_after = spk_cnt;
		rec_sample(o, sp, it, (double) (t1 - t0) / n);
	}
	o->ops_per_sample = n;
	o->batch = 1;
	o->checksum = 0;
	if (keep) efree(keep);
	spk_pool_free(pool, n);
}

/* -------- workload 9: invisible retention (stats, no timing) -------- */

static void wl_invisible(const spk_spec *sp, bench_out *o)
{
	uint32_t chunk = 1u << sp->shift;
	uint32_t base = sp->base_n;
	uint32_t appended = sp->appended;   /* total appended at the tip */
	uint32_t snap_at = sp->n;           /* appended count at snapshot */
	zval *pool = spk_pool_build(sp->elem, base + appended);

	void *h = spk_literal(REPR_C, sp->shift, pool, base);
	void *snap = NULL;
	for (uint32_t i = 0; i < appended; i++) {
		if (i == snap_at) snap = spk_dup(REPR_C, h);
		h = spk_append(REPR_C, h, &pool[base + i]);
	}
	uint64_t live_slots_tip = spk_cnt.zval_copies - spk_cnt.zval_dtors;
	uint64_t live_bytes_tip = spk_cnt.bytes_alloc - spk_cnt.bytes_freed;
	spk_release(REPR_C, h); /* only the old snapshot survives */

	uint64_t live_slots = spk_cnt.zval_copies - spk_cnt.zval_dtors;
	uint64_t live_bytes = spk_cnt.bytes_alloc - spk_cnt.bytes_freed;
	uint64_t visible = snap ? spk_count(REPR_C, snap) : 0;
	uint64_t blocks_live = spk_cnt.block_allocs - spk_cnt.block_frees;
	uint64_t spine_arr_live = spk_cnt.tag_alloc_bytes[TAG_SPINEARR]
		- spk_cnt.tag_free_bytes[TAG_SPINEARR];

	out_extra(o, "live_slots_tip", live_slots_tip);
	out_extra(o, "live_bytes_tip", live_bytes_tip);
	out_extra(o, "live_slots_snap", live_slots);
	out_extra(o, "live_bytes_snap", live_bytes);
	out_extra(o, "visible_count", visible);
	out_extra(o, "invisible_slots", live_slots - visible);
	out_extra(o, "invisible_slot_bound", chunk);
	out_extra(o, "blocks_live_snap", blocks_live);
	out_extra(o, "spine_arr_bytes_snap", spine_arr_live);
	o->checksum = snap ? spk_deep_checksum(REPR_C, snap) : 0;
	if (snap) spk_release(REPR_C, snap);
	o->ops_per_sample = 1;
	o->batch = 1;
	spk_pool_free(pool, base + appended);
}

/* ---------------- workload 10a: many forks off one snapshot ---------------- */

static void wl_forks(const spk_spec *sp, bench_out *o)
{
	uint32_t chunk = 1u << sp->shift;
	uint32_t base = sp->base_n, F = sp->forks, per = sp->per ? sp->per : 8;
	zval *pool = spk_pool_build(sp->elem, base + chunk + F + per + 4);

	void *S;
	if (sp->repr == REPR_C) {
		S = spk_literal(REPR_C, sp->shift, pool, base);
		for (uint32_t i = 0; i < chunk / 2; i++) {
			S = spk_append(REPR_C, S, &pool[base + i]);
		}
		void *t = spk_append(REPR_C, spk_dup(REPR_C, S), &pool[base + chunk / 2]);
		spk_release(REPR_C, t); /* every fork off S is a branch */
	} else {
		S = spk_literal(sp->repr, sp->shift, pool, base + chunk / 2);
	}

	void **scratch = safe_emalloc(F, sizeof(void *), 0);
	uint32_t iters = sp->warmup + sp->samples;
	for (uint32_t it = 0; it < iters; it++) {
		int last = (it == iters - 1);
		if (last) o->c_before = spk_cnt;
		uint64_t t0 = spk_now();
		for (uint32_t f = 0; f < F; f++) {
			void *h = spk_append(sp->repr, spk_dup(sp->repr, S), &pool[base + chunk + f]);
			for (uint32_t j = 1; j < per; j++) {
				h = spk_append(sp->repr, h, &pool[base + chunk + F + j]);
			}
			scratch[f] = h;
		}
		uint64_t t1 = spk_now();
		if (last) {
			o->c_after = spk_cnt;
			o->mem_build = zend_memory_usage(0);
			out_extra(o, "live_bytes_all_forks",
				spk_cnt.bytes_alloc - spk_cnt.bytes_freed);
			o->checksum = spk_deep_checksum(sp->repr, scratch[F - 1]);
		}
		uint64_t t2 = spk_now();
		for (uint32_t f = 0; f < F; f++) spk_release(sp->repr, scratch[f]);
		uint64_t t3 = spk_now();
		rec_sample(o, sp, it, (double) (t1 - t0) / F);
		rec_sample2(o, sp, it, (double) (t3 - t2) / F);
	}
	o->ops_per_sample = F;
	o->batch = 1;
	efree(scratch);
	spk_release(sp->repr, S);
	spk_pool_free(pool, base + chunk + F + per + 4);
}

/* ------- workload 10b: fork depth — branch of branch of branch ... ------- */

static void wl_depth(const spk_spec *sp, bench_out *o)
{
	uint32_t chunk = 1u << sp->shift;
	uint32_t base = sp->base_n, D = sp->depth;
	zval *pool = spk_pool_build(sp->elem, base + chunk + 4);
	void **keep = sp->keep_all ? safe_emalloc(D, sizeof(void *), 0) : NULL;

	uint32_t iters = sp->warmup + sp->samples;
	for (uint32_t it = 0; it < iters; it++) {
		int last = (it == iters - 1);
		/* fresh chain each iteration (untimed setup) */
		void *v = spk_literal(REPR_C, sp->shift, pool, base);
		for (uint32_t i = 0; i < chunk / 2; i++) {
			v = spk_append(REPR_C, v, &pool[base + i]);
		}
		if (last) o->c_before = spk_cnt;
		uint64_t t0 = spk_now();
		for (uint32_t d = 0; d < D; d++) {
			/* extend the tip past v, then append to v: a true branch */
			void *w = spk_append(REPR_C, spk_dup(REPR_C, v), &pool[base + chunk]);
			void *b = spk_append(REPR_C, v, &pool[base + chunk + 1]);
			spk_release(REPR_C, w);
			if (keep) keep[d] = spk_dup(REPR_C, b);
			v = b;
		}
		uint64_t t1 = spk_now();
		if (last) {
			o->c_after = spk_cnt;
			o->mem_build = zend_memory_usage(0);
			out_extra(o, "live_bytes_depth",
				spk_cnt.bytes_alloc - spk_cnt.bytes_freed);
			o->checksum = spk_deep_checksum(REPR_C, v);
		}
		if (keep) {
			for (uint32_t d = 0; d < D; d++) spk_release(REPR_C, keep[d]);
		}
		spk_release(REPR_C, v);
		rec_sample(o, sp, it, (double) (t1 - t0) / D);
	}
	o->ops_per_sample = D;
	o->batch = 1;
	if (keep) efree(keep);
	spk_pool_free(pool, base + chunk + 4);
}

/* ---------------- PHP-visible functions ---------------- */

static void add_counter_delta(zval *arr, const spk_counters *a, const spk_counters *b)
{
#define D(field) add_assoc_double(arr, #field, (double) (b->field - a->field))
	D(allocs); D(frees); D(bytes_alloc); D(bytes_freed);
	D(zval_copies); D(zval_moves); D(zval_dtors);
	D(rc_addref); D(rc_delref);
	D(block_addref); D(block_delref); D(block_allocs); D(block_frees);
	D(spine_allocs); D(spine_grows); D(version_allocs); D(branches);
#undef D
}

static PHP_FUNCTION(zend_test_vec_spike_bench)
{
	HashTable *ht;
	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_ARRAY_HT(ht)
	ZEND_PARSE_PARAMETERS_END();

	spk_spec sp;
	memset(&sp, 0, sizeof(sp));
	const char *workload = spec_str(ht, "workload", "");
	const char *repr_s = spec_str(ht, "repr", "A");
	sp.repr = repr_s[0] == 'B' ? REPR_B : (repr_s[0] == 'C' ? REPR_C : REPR_A);
	sp.shift = (uint8_t) spec_long(ht, "shift", 6);
	int el = spk_el_parse(spec_str(ht, "elem", "int"));
	if (el < 0) {
		array_init(return_value);
		add_assoc_string(return_value, "error", "bad elem");
		return;
	}
	sp.elem = (spk_el) el;
	sp.n = (uint32_t) spec_long(ht, "n", 1024);
	sp.base_n = (uint32_t) spec_long(ht, "base", 0);
	sp.appended = (uint32_t) spec_long(ht, "appended", 1);
	sp.blocks = (uint32_t) spec_long(ht, "blocks", 1);
	sp.forks = (uint32_t) spec_long(ht, "forks", 1);
	sp.depth = (uint32_t) spec_long(ht, "depth", 1);
	sp.per = (uint32_t) spec_long(ht, "per", 8);
	sp.samples = (uint32_t) spec_long(ht, "samples", 15);
	sp.warmup = (uint32_t) spec_long(ht, "warmup", 3);
	sp.target_us = (uint64_t) spec_long(ht, "target_us", 300);
	sp.batch_cap = (uint64_t) spec_long(ht, "batch_cap", 4096);
	sp.seed = (uint64_t) spec_long(ht, "seed", 0x5eed);
	sp.retained = (int) spec_long(ht, "retained", 0);
	sp.keep_all = (int) spec_long(ht, "keep_all", 0);
	sp.region = spec_str(ht, "region", "mixed");
	sp.lv_pos = spec_str(ht, "lv_pos", "half");
	if (sp.samples > SPK_MAX_SAMPLES) sp.samples = SPK_MAX_SAMPLES;

	memset(&spk_cnt, 0, sizeof(spk_cnt));
	zend_memory_reset_peak_usage();
	size_t mem_start = zend_memory_usage(0);

	bench_out o;
	memset(&o, 0, sizeof(o));

	if (!strcmp(workload, "linear")) wl_linear(&sp, &o);
	else if (!strcmp(workload, "base_append")) wl_base_append(&sp, &o);
	else if (!strcmp(workload, "branch")) wl_branch(&sp, &o);
	else if (!strcmp(workload, "read")) wl_read(&sp, &o);
	else if (!strcmp(workload, "foreach")) wl_foreach(&sp, &o);
	else if (!strcmp(workload, "vcreate")) wl_vcreate(&sp, &o);
	else if (!strcmp(workload, "destroy")) wl_destroy(&sp, &o);
	else if (!strcmp(workload, "invisible")) wl_invisible(&sp, &o);
	else if (!strcmp(workload, "forks")) wl_forks(&sp, &o);
	else if (!strcmp(workload, "depth")) wl_depth(&sp, &o);
	else o.err = "unknown workload";

	size_t mem_end = zend_memory_usage(0);
	size_t mem_peak = zend_memory_peak_usage(0);

	array_init(return_value);
	if (o.err) {
		add_assoc_string(return_value, "error", o.err);
		return;
	}
	zval samples;
	array_init(&samples);
	for (uint32_t i = 0; i < o.nsamples; i++) {
		add_next_index_double(&samples, o.samples[i]);
	}
	add_assoc_zval(return_value, "samples_ns", &samples);
	if (o.nsamples2) {
		zval s2;
		array_init(&s2);
		for (uint32_t i = 0; i < o.nsamples2; i++) {
			add_next_index_double(&s2, o.samples2[i]);
		}
		add_assoc_zval(return_value, "samples2_ns", &s2);
	}
	add_assoc_double(return_value, "ops_per_sample", (double) o.ops_per_sample);
	add_assoc_double(return_value, "batch", (double) o.batch);
	char cs[24];
	snprintf(cs, sizeof(cs), "%016llx", (unsigned long long) o.checksum);
	add_assoc_string(return_value, "checksum", cs);

	zval cd;
	array_init(&cd);
	add_counter_delta(&cd, &o.c_before, &o.c_after);
	add_assoc_zval(return_value, "timed_counters", &cd);

	int balance_ok = spk_cnt.allocs == spk_cnt.frees
		&& spk_cnt.zval_copies == spk_cnt.zval_dtors
		&& spk_cnt.block_allocs == spk_cnt.block_frees
		&& spk_cnt.spine_allocs == spk_cnt.spine_frees
		&& spk_cnt.version_allocs == spk_cnt.version_frees;
	add_assoc_bool(return_value, "balance_ok", balance_ok);

	zval mem;
	array_init(&mem);
	add_assoc_double(&mem, "start", (double) mem_start);
	add_assoc_double(&mem, "build", (double) o.mem_build);
	add_assoc_double(&mem, "peak", (double) mem_peak);
	add_assoc_double(&mem, "end", (double) mem_end);
	add_assoc_zval(return_value, "mem", &mem);

	if (o.extra_n) {
		zval ex;
		array_init(&ex);
		for (int i = 0; i < o.extra_n; i++) {
			add_assoc_double(&ex, o.extra_k[i], (double) o.extra_v[i]);
		}
		add_assoc_zval(return_value, "extra", &ex);
	}
}

static PHP_FUNCTION(zend_test_vec_spike_selftest)
{
	if (zend_parse_parameters_none() == FAILURE) {
		RETURN_THROWS();
	}
	vec_spike_selftest_run(return_value);
}

/* Zend MM small size classes (from Zend/zend_alloc_sizes.h); allocations
 * above the last bin are "large" and rounded up to 4096-byte pages. */
static const uint32_t zmm_bins[] = {
	8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256,
	320, 384, 448, 512, 640, 768, 896, 1024, 1280, 1536, 1792, 2048, 2560, 3072
};

static uint64_t zmm_alloc_size(uint64_t want)
{
	for (size_t i = 0; i < sizeof(zmm_bins) / sizeof(zmm_bins[0]); i++) {
		if (want <= zmm_bins[i]) return zmm_bins[i];
	}
	return (want + 4095) & ~4095ull; /* large: page-rounded */
}

static void layout_row(zval *arr, const char *name, uint64_t sz)
{
	zval row;
	array_init(&row);
	add_assoc_long(&row, "sizeof", (zend_long) sz);
	uint64_t got = zmm_alloc_size(sz);
	add_assoc_long(&row, "zmm_alloc", (zend_long) got);
	add_assoc_double(&row, "waste_pct", sz ? 100.0 * (double) (got - sz) / (double) got : 0);
	add_assoc_zval(arr, (char *) name, &row);
}

static PHP_FUNCTION(zend_test_vec_spike_layout)
{
	if (zend_parse_parameters_none() == FAILURE) {
		RETURN_THROWS();
	}
	array_init(return_value);
	add_assoc_long(return_value, "sizeof_zval", sizeof(zval));
	add_assoc_long(return_value, "sizeof_spk_flat_hdr", sizeof(spk_flat));
	add_assoc_long(return_value, "sizeof_spk_cap_hdr", sizeof(spk_cap));
	add_assoc_long(return_value, "sizeof_spk_block_hdr", sizeof(spk_block));
	add_assoc_long(return_value, "sizeof_spk_spine", sizeof(spk_spine));
	add_assoc_long(return_value, "sizeof_spk_hyb", sizeof(spk_hyb));
	add_assoc_long(return_value, "offsetof_hyb_lv", offsetof(spk_hyb, lv));
	add_assoc_long(return_value, "zend_debug", ZEND_DEBUG);
	zval rows;
	array_init(&rows);
	layout_row(&rows, "version", sizeof(spk_hyb));
	layout_row(&rows, "spine_struct", sizeof(spk_spine));
	char name[64];
	for (int shift = 4; shift <= 7; shift++) {
		uint32_t chunk = 1u << shift;
		snprintf(name, sizeof(name), "block_chunk%u", chunk);
		layout_row(&rows, name, SPK_BLK_SZ(chunk));
		snprintf(name, sizeof(name), "block_chunk%u_minus1", chunk);
		layout_row(&rows, name, SPK_BLK_SZ(chunk - 1));
	}
	layout_row(&rows, "flat_100", SPK_FLAT_SZ(100));
	layout_row(&rows, "flat_100k", SPK_FLAT_SZ(100000));
	add_assoc_zval(return_value, "allocs", &rows);
}

ZEND_BEGIN_ARG_INFO_EX(ai_spike_none, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(ai_spike_bench, 0, 0, 1)
	ZEND_ARG_INFO(0, spec)
ZEND_END_ARG_INFO()

static const zend_function_entry vec_spike_functions[] = {
	PHP_FE(zend_test_vec_spike_layout, ai_spike_none)
	PHP_FE(zend_test_vec_spike_selftest, ai_spike_none)
	PHP_FE(zend_test_vec_spike_bench, ai_spike_bench)
	PHP_FE_END
};

void vec_spike_minit(void)
{
	zend_register_functions(NULL, vec_spike_functions, NULL, MODULE_PERSISTENT);
}
