/* SPDX-License-Identifier: GPL-2.0-only OR MIT */
#ifndef __APPLE_DCP_DPTX_ATTEMPT_H__
#define __APPLE_DCP_DPTX_ATTEMPT_H__

#include <linux/errno.h>
#include <linux/types.h>

enum apple_dptx_attempt_phase {
	APPLE_DPTX_ATTEMPT_IDLE,
	APPLE_DPTX_ATTEMPT_CONNECTING,
	APPLE_DPTX_ATTEMPT_LINK_CONFIGURED,
	APPLE_DPTX_ATTEMPT_CONNECTED,
	APPLE_DPTX_ATTEMPT_CANCELING,
	APPLE_DPTX_ATTEMPT_FAILED,
};

struct apple_dptx_attempt_ticket {
	u64 generation;
};

struct apple_dptx_attempt {
	enum apple_dptx_attempt_phase phase;
	u64 generation;
};

static inline void apple_dptx_attempt_init(struct apple_dptx_attempt *attempt)
{
	*attempt = (struct apple_dptx_attempt) {};
}

static inline int
apple_dptx_attempt_begin(struct apple_dptx_attempt *attempt,
			 struct apple_dptx_attempt_ticket *ticket)
{
	if (attempt->phase != APPLE_DPTX_ATTEMPT_IDLE)
		return -EBUSY;

	attempt->generation++;
	if (!attempt->generation)
		attempt->generation++;
	attempt->phase = APPLE_DPTX_ATTEMPT_CONNECTING;
	ticket->generation = attempt->generation;

	return 0;
}

static inline bool
apple_dptx_attempt_link_configured(struct apple_dptx_attempt *attempt)
{
	/*
	 * Firmware supplies no attempt token. Callers must quarantine every
	 * unsuccessful attempt until the DPTX endpoint restarts, so an old APCALL
	 * cannot complete a newer attempt.
	 */
	if (attempt->phase != APPLE_DPTX_ATTEMPT_CONNECTING)
		return false;

	attempt->phase = APPLE_DPTX_ATTEMPT_LINK_CONFIGURED;
	return true;
}

static inline bool apple_dptx_attempt_cancel(struct apple_dptx_attempt *attempt)
{
	switch (attempt->phase) {
	case APPLE_DPTX_ATTEMPT_CONNECTING:
	case APPLE_DPTX_ATTEMPT_LINK_CONFIGURED:
		attempt->phase = APPLE_DPTX_ATTEMPT_CANCELING;
		return true;
	case APPLE_DPTX_ATTEMPT_CONNECTED:
		attempt->phase = APPLE_DPTX_ATTEMPT_IDLE;
		return false;
	case APPLE_DPTX_ATTEMPT_IDLE:
	case APPLE_DPTX_ATTEMPT_CANCELING:
	case APPLE_DPTX_ATTEMPT_FAILED:
		return false;
	}

	return false;
}

static inline int
apple_dptx_attempt_finish_wait(struct apple_dptx_attempt *attempt,
			       const struct apple_dptx_attempt_ticket *ticket,
			       bool completed)
{
	if (ticket->generation != attempt->generation)
		return -ESTALE;
	if (attempt->phase == APPLE_DPTX_ATTEMPT_FAILED)
		return -EIO;
	if (attempt->phase == APPLE_DPTX_ATTEMPT_CANCELING)
		return -ECANCELED;
	if (!completed) {
		attempt->phase = APPLE_DPTX_ATTEMPT_CANCELING;
		return -ETIMEDOUT;
	}
	if (attempt->phase != APPLE_DPTX_ATTEMPT_LINK_CONFIGURED) {
		attempt->phase = APPLE_DPTX_ATTEMPT_CANCELING;
		return -EIO;
	}

	attempt->phase = APPLE_DPTX_ATTEMPT_CONNECTED;
	return 0;
}

static inline bool
apple_dptx_attempt_reset(struct apple_dptx_attempt *attempt,
			 const struct apple_dptx_attempt_ticket *ticket)
{
	if (ticket->generation != attempt->generation ||
	    attempt->phase == APPLE_DPTX_ATTEMPT_CONNECTED ||
	    attempt->phase == APPLE_DPTX_ATTEMPT_FAILED)
		return false;

	attempt->phase = APPLE_DPTX_ATTEMPT_IDLE;
	return true;
}

static inline bool
apple_dptx_attempt_fail(struct apple_dptx_attempt *attempt,
			const struct apple_dptx_attempt_ticket *ticket)
{
	if (ticket && ticket->generation != attempt->generation)
		return false;

	attempt->phase = APPLE_DPTX_ATTEMPT_FAILED;
	return true;
}

#endif
