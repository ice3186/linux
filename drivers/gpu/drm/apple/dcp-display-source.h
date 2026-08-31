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
	APPLE_DCP_DISPLAY_QUARANTINED,
};

struct apple_dcp_display_sink {
	u8 die;
	u8 atc;
	u8 dpin;
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

struct apple_dcp_display_candidate {
	unsigned int engine;
	u8 core;
};

struct apple_dcp_display_allocation {
	unsigned int engine;
	struct apple_dcp_display_route route;
	struct apple_dcp_display_cookie cookie;
};

struct apple_dcp_display_restart_ticket {
	u64 session;
	u64 generation;
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
	u64 restart_generation;
	bool quiescing;
	bool restart_armed;
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

	/*
	 * Construction/object-lifetime reset only. Runtime recovery must use
	 * endpoint_restarted() so stale cookies cannot alias a new session.
	 * The owner supplies non-NULL storage for exactly n_engines leases.
	 */
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
	bool busy = false;
	unsigned int i;

	if (!pool || !pool->leases || !pool->n_engines)
		return -EINVAL;
	if (!pool->quiescing)
		return -EBUSY;
	if (pool->restart_armed)
		return -EBUSY;
	for (i = 0; i < pool->n_engines; i++) {
		if (pool->leases[i].phase == APPLE_DCP_DISPLAY_QUARANTINED)
			return -EIO;
		if (pool->leases[i].phase != APPLE_DCP_DISPLAY_IDLE)
			busy = true;
	}
	if (busy)
		return -EBUSY;
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
apple_dcp_display_quiesce_for_restart(
	struct apple_dcp_display_pool *pool,
	struct apple_dcp_display_restart_ticket *ticket)
{
	if (!ticket)
		return -EINVAL;
	*ticket = (struct apple_dcp_display_restart_ticket) {};
	if (!pool || !pool->leases || !pool->n_engines)
		return -EINVAL;
	if (pool->restart_generation == ~0ULL)
		return -EOVERFLOW;

	pool->quiescing = true;
	pool->restart_generation++;
	pool->restart_armed = true;
	*ticket = (struct apple_dcp_display_restart_ticket) {
		.session = pool->session,
		.generation = pool->restart_generation,
	};

	return 0;
}

static inline int
apple_dcp_display_endpoint_restarted(
	struct apple_dcp_display_pool *pool,
	const struct apple_dcp_display_restart_ticket *ticket)
{
	unsigned int i;

	if (!pool || !pool->leases || !pool->n_engines || !ticket)
		return -EINVAL;
	if (!pool->quiescing)
		return -EBUSY;
	if (!pool->restart_armed || ticket->session != pool->session ||
	    ticket->generation != pool->restart_generation)
		return -ESTALE;
	for (i = 0; i < pool->n_engines; i++) {
		if (pool->leases[i].phase == APPLE_DCP_DISPLAY_PREPARED ||
		    pool->leases[i].phase == APPLE_DCP_DISPLAY_ENABLED)
			return -EBUSY;
	}

	/*
	 * The Apple display owner may attest this only after the DPTX endpoint
	 * has restarted and all callbacks and work using old allocations drained.
	 * Preserve session so begin_session() advances the epoch before reuse.
	 */
	for (i = 0; i < pool->n_engines; i++)
		pool->leases[i] = (struct apple_dcp_display_lease) {};
	pool->restart_armed = false;

	return 0;
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
	if (lease->phase == APPLE_DCP_DISPLAY_QUARANTINED)
		return -EIO;
	if (lease->phase != APPLE_DCP_DISPLAY_IDLE)
		return -EBUSY;

	for (i = 0; i < pool->n_engines; i++) {
		if (i != engine &&
		    pool->leases[i].phase != APPLE_DCP_DISPLAY_IDLE &&
		    apple_dcp_display_sink_equal(&pool->leases[i].route, route)) {
			if (pool->leases[i].phase ==
			    APPLE_DCP_DISPLAY_QUARANTINED)
				return -EIO;
			return -EBUSY;
		}
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

/*
 * Apple-provider-internal selection. The provider derives candidates from
 * topology; a Thunderbolt requester must not choose an engine or DPTX core.
 */
static inline int
apple_dcp_display_acquire(struct apple_dcp_display_pool *pool,
			  const struct apple_dcp_display_sink *sink,
			  const struct apple_dcp_display_candidate *candidates,
			  unsigned int n_candidates,
			  struct apple_dcp_display_allocation *allocation)
{
	struct apple_dcp_display_route route;
	struct apple_dcp_display_cookie cookie;
	struct apple_dptx_target target;
	bool all_quarantined = true;
	unsigned int i;
	int ret;

	if (!allocation)
		return -EINVAL;
	*allocation = (struct apple_dcp_display_allocation) {};
	if (!pool || !pool->leases || !pool->n_engines || !sink ||
	    !candidates || !n_candidates)
		return -EINVAL;

	/* Reject malformed topology atomically, including later candidates. */
	for (i = 0; i < n_candidates; i++) {
		if (candidates[i].engine >= pool->n_engines)
			return -EINVAL;
		route = (struct apple_dcp_display_route) {
			.die = sink->die,
			.atc = sink->atc,
			.dpin = sink->dpin,
			.core = candidates[i].core,
		};
		ret = apple_dcp_display_route_to_target(&route, &target);
		if (ret)
			return ret;
	}

	if (pool->quiescing)
		return -ESHUTDOWN;

	route = (struct apple_dcp_display_route) {
		.die = sink->die,
		.atc = sink->atc,
		.dpin = sink->dpin,
	};
	for (i = 0; i < pool->n_engines; i++) {
		if (pool->leases[i].phase == APPLE_DCP_DISPLAY_IDLE ||
		    !apple_dcp_display_sink_equal(&pool->leases[i].route, &route))
			continue;
		return pool->leases[i].phase == APPLE_DCP_DISPLAY_QUARANTINED ?
			-EIO : -EBUSY;
	}

	for (i = 0; i < n_candidates; i++) {
		if (pool->leases[candidates[i].engine].phase ==
		    APPLE_DCP_DISPLAY_IDLE) {
			route.core = candidates[i].core;
			ret = apple_dcp_display_prepare(pool, candidates[i].engine,
						&route, &cookie);
			if (ret)
				return ret;
			*allocation = (struct apple_dcp_display_allocation) {
				.engine = candidates[i].engine,
				.route = route,
				.cookie = cookie,
			};
			return 0;
		}
		if (pool->leases[candidates[i].engine].phase !=
		    APPLE_DCP_DISPLAY_QUARANTINED)
			all_quarantined = false;
	}

	return all_quarantined ? -EIO : -EBUSY;
}

static inline int
apple_dcp_display_quarantine(struct apple_dcp_display_pool *pool,
			     unsigned int engine,
			     const struct apple_dcp_display_cookie *cookie)
{
	struct apple_dcp_display_lease *lease;

	if (!pool || !pool->leases || engine >= pool->n_engines || !cookie)
		return -EINVAL;
	lease = &pool->leases[engine];
	if (!apple_dcp_display_cookie_equal(&lease->cookie, cookie))
		return -ESTALE;
	if (lease->phase == APPLE_DCP_DISPLAY_QUARANTINED)
		return 0;
	if (lease->phase != APPLE_DCP_DISPLAY_PREPARED &&
	    lease->phase != APPLE_DCP_DISPLAY_ENABLED)
		return -EINVAL;

	lease->phase = APPLE_DCP_DISPLAY_QUARANTINED;
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
	if (lease->phase == APPLE_DCP_DISPLAY_QUARANTINED)
		return -EIO;
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
	if (lease->phase == APPLE_DCP_DISPLAY_QUARANTINED)
		return -EIO;
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
	if (lease->phase == APPLE_DCP_DISPLAY_QUARANTINED)
		return -EIO;
	if (lease->phase == APPLE_DCP_DISPLAY_ENABLED)
		return -EBUSY;
	if (lease->phase != APPLE_DCP_DISPLAY_PREPARED)
		return -EINVAL;

	*lease = (struct apple_dcp_display_lease) {};
	return 0;
}

#endif /* __APPLE_DCP_DISPLAY_SOURCE_H__ */
