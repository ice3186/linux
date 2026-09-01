/* SPDX-License-Identifier: GPL-2.0 */
#ifndef APPLE_DP_SOURCE_H_
#define APPLE_DP_SOURCE_H_

#include <linux/errno.h>
#include <linux/types.h>

enum apple_dp_source_phase {
	APPLE_DP_SOURCE_IDLE,
	APPLE_DP_SOURCE_PREPARED,
	APPLE_DP_SOURCE_ENABLED,
};

struct apple_dp_source_endpoint {
	u64 in_route;
	u64 out_route;
	u8 in_port;
	u8 out_port;
};

struct apple_dp_source_cookie {
	u64 session;
	u64 token;
};

struct apple_dp_source_state {
	struct apple_dp_source_endpoint endpoint;
	struct apple_dp_source_cookie cookie;
	enum apple_dp_source_phase phase;
	u64 generation;
	u64 session;
	u64 next_token;
	bool quiescing;
};

static inline bool
apple_dp_source_cookie_equal(const struct apple_dp_source_cookie *a,
			     const struct apple_dp_source_cookie *b)
{
	return a->session == b->session && a->token == b->token;
}

static inline bool
apple_dp_source_endpoint_equal(const struct apple_dp_source_endpoint *a,
			       const struct apple_dp_source_endpoint *b)
{
	return a->in_route == b->in_route && a->out_route == b->out_route &&
	       a->in_port == b->in_port && a->out_port == b->out_port;
}

static inline bool
apple_dp_source_state_equal(const struct apple_dp_source_state *a,
			    const struct apple_dp_source_state *b)
{
	return apple_dp_source_endpoint_equal(&a->endpoint, &b->endpoint) &&
	       apple_dp_source_cookie_equal(&a->cookie, &b->cookie) &&
	       a->phase == b->phase && a->generation == b->generation &&
	       a->session == b->session && a->next_token == b->next_token &&
	       a->quiescing == b->quiescing;
}

/*
 * Advance an applied state by one externally observable lifecycle edge.
 *
 * A future hardware provider must disable an enabled source before releasing
 * its reservation. Keeping that ordering in the trace-only reducer prevents
 * a coalesced ENABLED -> IDLE request from hiding either edge during bring-up.
 * Identity changes also drain the old source to IDLE before preparing the new
 * endpoint.
 */
static inline bool
apple_dp_source_step_toward(struct apple_dp_source_state *applied,
			    const struct apple_dp_source_state *desired)
{
	bool identity_changed =
		!apple_dp_source_endpoint_equal(&applied->endpoint,
					&desired->endpoint) ||
		!apple_dp_source_cookie_equal(&applied->cookie, &desired->cookie);

	if (applied->phase != APPLE_DP_SOURCE_IDLE && identity_changed) {
		if (applied->phase == APPLE_DP_SOURCE_ENABLED) {
			applied->phase = APPLE_DP_SOURCE_PREPARED;
		} else {
			*applied = *desired;
			applied->phase = APPLE_DP_SOURCE_IDLE;
			applied->endpoint = (struct apple_dp_source_endpoint) {};
			applied->cookie = (struct apple_dp_source_cookie) {};
		}
		return true;
	}

	if (applied->phase > desired->phase) {
		applied->phase--;
		if (applied->phase == APPLE_DP_SOURCE_IDLE)
			*applied = *desired;
		return true;
	}

	if (applied->phase < desired->phase) {
		if (applied->phase == APPLE_DP_SOURCE_IDLE) {
			*applied = *desired;
			applied->phase = APPLE_DP_SOURCE_PREPARED;
		} else {
			applied->phase = APPLE_DP_SOURCE_ENABLED;
		}
		return true;
	}

	if (apple_dp_source_state_equal(applied, desired))
		return false;

	*applied = *desired;
	return true;
}

static inline void apple_dp_source_state_init(struct apple_dp_source_state *state)
{
	*state = (struct apple_dp_source_state) {
		.next_token = 1,
		.quiescing = true,
	};
}

static inline int apple_dp_source_begin_session(struct apple_dp_source_state *state)
{
	if (!state->quiescing || state->phase != APPLE_DP_SOURCE_IDLE)
		return -EBUSY;

	state->session++;
	if (!state->session)
		state->session++;
	state->quiescing = false;
	return 0;
}

static inline int
apple_dp_source_prepare(struct apple_dp_source_state *state,
			const struct apple_dp_source_endpoint *endpoint,
			struct apple_dp_source_cookie *cookie)
{
	if (state->quiescing)
		return -ESHUTDOWN;
	if (state->phase != APPLE_DP_SOURCE_IDLE)
		return -EBUSY;

	state->endpoint = *endpoint;
	state->cookie.session = state->session;
	state->cookie.token = state->next_token++;
	if (!state->next_token)
		state->next_token++;
	state->phase = APPLE_DP_SOURCE_PREPARED;
	state->generation++;
	*cookie = state->cookie;

	return 0;
}

static inline int
apple_dp_source_enable(struct apple_dp_source_state *state,
		       const struct apple_dp_source_cookie *cookie)
{
	if (!apple_dp_source_cookie_equal(&state->cookie, cookie))
		return -ESTALE;
	if (state->phase == APPLE_DP_SOURCE_ENABLED)
		return 0;
	if (state->phase != APPLE_DP_SOURCE_PREPARED)
		return -EINVAL;

	state->phase = APPLE_DP_SOURCE_ENABLED;
	state->generation++;
	return 0;
}

static inline bool
apple_dp_source_disable(struct apple_dp_source_state *state,
			const struct apple_dp_source_cookie *cookie)
{
	if (!apple_dp_source_cookie_equal(&state->cookie, cookie) ||
	    state->phase != APPLE_DP_SOURCE_ENABLED)
		return false;

	state->phase = APPLE_DP_SOURCE_PREPARED;
	state->generation++;
	return true;
}

static inline bool
apple_dp_source_unprepare(struct apple_dp_source_state *state,
			  const struct apple_dp_source_cookie *cookie)
{
	if (!apple_dp_source_cookie_equal(&state->cookie, cookie) ||
	    state->phase == APPLE_DP_SOURCE_IDLE)
		return false;

	state->phase = APPLE_DP_SOURCE_IDLE;
	state->endpoint = (struct apple_dp_source_endpoint) {};
	state->cookie = (struct apple_dp_source_cookie) {};
	state->generation++;
	return true;
}

static inline bool apple_dp_source_quiesce(struct apple_dp_source_state *state)
{
	bool changed = state->phase != APPLE_DP_SOURCE_IDLE;

	state->quiescing = true;
	state->phase = APPLE_DP_SOURCE_IDLE;
	state->endpoint = (struct apple_dp_source_endpoint) {};
	state->cookie = (struct apple_dp_source_cookie) {};
	if (changed)
		state->generation++;

	return changed;
}

#endif
