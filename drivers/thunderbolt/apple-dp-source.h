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
