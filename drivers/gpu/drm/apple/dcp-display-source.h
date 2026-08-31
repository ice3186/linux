/* SPDX-License-Identifier: GPL-2.0-only OR MIT */
#ifndef __APPLE_DCP_DISPLAY_SOURCE_H__
#define __APPLE_DCP_DISPLAY_SOURCE_H__

#include <linux/errno.h>
#include <linux/types.h>

#include "dptx-transport.h"

enum apple_dcp_display_phase {
	APPLE_DCP_DISPLAY_IDLE,
	APPLE_DCP_DISPLAY_PREPARED,
	APPLE_DCP_DISPLAY_ENABLED,
};

struct apple_dcp_display_route {
	u8 die;
	u8 atc;
	u8 dpin;
	u8 core;
};

struct apple_dcp_display_cookie {
	u64 session;
	u64 token;
};

struct apple_dcp_display_lease {
	struct apple_dcp_display_route route;
	struct apple_dcp_display_cookie cookie;
	enum apple_dcp_display_phase phase;
};

struct apple_dcp_display_pool {
	struct apple_dcp_display_lease *leases;
	unsigned int n_engines;
	u64 session;
	u64 next_token;
	bool quiescing;
};

/* All pool operations require serialization by the eventual owner. */

static inline bool
apple_dcp_display_cookie_equal(const struct apple_dcp_display_cookie *a,
			       const struct apple_dcp_display_cookie *b)
{
	return a->session == b->session && a->token == b->token;
}

static inline bool
apple_dcp_display_sink_equal(const struct apple_dcp_display_route *a,
			     const struct apple_dcp_display_route *b)
{
	/* A physical DP-IN mux control can select only one source/core. */
	return a->die == b->die && a->atc == b->atc && a->dpin == b->dpin;
}

static inline int
apple_dcp_display_route_to_target(const struct apple_dcp_display_route *route,
				    struct apple_dptx_target *target)
{
	u32 ignored;
	int ret;

	if (!target)
		return -EINVAL;
	*target = (struct apple_dptx_target) {};
	if (!route)
		return -EINVAL;
	if (route->dpin > 1)
		return -ERANGE;

	*target = (struct apple_dptx_target) {
		.core = route->core,
		.atc = route->atc,
		.die = route->die,
	};
	ret = apple_dptx_target_encode(target, &ignored);
	if (ret)
		*target = (struct apple_dptx_target) {};

	return ret;
}

static inline void
apple_dcp_display_pool_init(struct apple_dcp_display_pool *pool,
			    struct apple_dcp_display_lease *leases,
			    unsigned int n_engines)
{
	unsigned int i;

	/* The owner supplies non-NULL storage for exactly n_engines leases. */
	*pool = (struct apple_dcp_display_pool) {
		.leases = leases,
		.n_engines = n_engines,
		.next_token = 1,
		.quiescing = true,
	};
	for (i = 0; i < n_engines; i++)
		leases[i] = (struct apple_dcp_display_lease) {};
}

static inline int
apple_dcp_display_begin_session(struct apple_dcp_display_pool *pool)
{
	unsigned int i;

	if (!pool || !pool->leases || !pool->n_engines)
		return -EINVAL;
	if (!pool->quiescing)
		return -EBUSY;
	for (i = 0; i < pool->n_engines; i++) {
		if (pool->leases[i].phase != APPLE_DCP_DISPLAY_IDLE)
			return -EBUSY;
	}
	if (pool->session == ~0ULL)
		return -EOVERFLOW;

	pool->session++;
	pool->next_token = 1;
	pool->quiescing = false;

	return 0;
}

static inline bool
apple_dcp_display_quiesce(struct apple_dcp_display_pool *pool)
{
	bool changed;

	if (!pool)
		return false;

	changed = !pool->quiescing;
	pool->quiescing = true;
	return changed;
}

static inline int
apple_dcp_display_prepare(struct apple_dcp_display_pool *pool,
			  unsigned int engine,
			  const struct apple_dcp_display_route *route,
			  struct apple_dcp_display_cookie *cookie)
{
	struct apple_dcp_display_lease *lease;
	struct apple_dptx_target target;
	unsigned int i;
	int ret;

	if (!pool || !pool->leases || engine >= pool->n_engines || !cookie)
		return -EINVAL;
	if (pool->quiescing)
		return -ESHUTDOWN;

	ret = apple_dcp_display_route_to_target(route, &target);
	if (ret)
		return ret;

	lease = &pool->leases[engine];
	if (lease->phase != APPLE_DCP_DISPLAY_IDLE)
		return -EBUSY;

	for (i = 0; i < pool->n_engines; i++) {
		if (i != engine &&
		    pool->leases[i].phase != APPLE_DCP_DISPLAY_IDLE &&
		    apple_dcp_display_sink_equal(&pool->leases[i].route, route))
			return -EBUSY;
	}
	if (!pool->next_token)
		return -EOVERFLOW;

	lease->route = *route;
	lease->cookie.session = pool->session;
	lease->cookie.token = pool->next_token++;
	lease->phase = APPLE_DCP_DISPLAY_PREPARED;
	*cookie = lease->cookie;

	return 0;
}

static inline int
apple_dcp_display_enable(struct apple_dcp_display_pool *pool,
			 unsigned int engine,
			 const struct apple_dcp_display_cookie *cookie)
{
	struct apple_dcp_display_lease *lease;

	if (!pool || !pool->leases || engine >= pool->n_engines || !cookie)
		return -EINVAL;
	lease = &pool->leases[engine];
	if (!apple_dcp_display_cookie_equal(&lease->cookie, cookie))
		return -ESTALE;
	if (lease->phase == APPLE_DCP_DISPLAY_ENABLED)
		return 0;
	if (pool->quiescing)
		return -ESHUTDOWN;
	if (lease->phase != APPLE_DCP_DISPLAY_PREPARED)
		return -EINVAL;

	lease->phase = APPLE_DCP_DISPLAY_ENABLED;
	return 0;
}

static inline int
apple_dcp_display_disable(struct apple_dcp_display_pool *pool,
			  unsigned int engine,
			  const struct apple_dcp_display_cookie *cookie)
{
	struct apple_dcp_display_lease *lease;

	if (!pool || !pool->leases || engine >= pool->n_engines || !cookie)
		return -EINVAL;
	lease = &pool->leases[engine];
	if (!apple_dcp_display_cookie_equal(&lease->cookie, cookie))
		return -ESTALE;
	if (lease->phase == APPLE_DCP_DISPLAY_PREPARED)
		return 0;
	if (lease->phase != APPLE_DCP_DISPLAY_ENABLED)
		return -EINVAL;

	lease->phase = APPLE_DCP_DISPLAY_PREPARED;
	return 0;
}

static inline int
apple_dcp_display_release(struct apple_dcp_display_pool *pool,
			  unsigned int engine,
			  const struct apple_dcp_display_cookie *cookie)
{
	struct apple_dcp_display_lease *lease;

	if (!pool || !pool->leases || engine >= pool->n_engines || !cookie)
		return -EINVAL;
	lease = &pool->leases[engine];
	if (!apple_dcp_display_cookie_equal(&lease->cookie, cookie))
		return -ESTALE;
	if (lease->phase == APPLE_DCP_DISPLAY_ENABLED)
		return -EBUSY;
	if (lease->phase != APPLE_DCP_DISPLAY_PREPARED)
		return -EINVAL;

	*lease = (struct apple_dcp_display_lease) {};
	return 0;
}

#endif /* __APPLE_DCP_DISPLAY_SOURCE_H__ */
