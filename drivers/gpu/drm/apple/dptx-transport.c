// SPDX-License-Identifier: GPL-2.0-only OR MIT
/* Copyright 2026 Matthew Altman */

#include <linux/errno.h>

#include "dcp-internal.h"
#include "dptx-transport.h"

static bool apple_dptx_lane_count_valid(u32 lane_count)
{
	return lane_count == 1 || lane_count == 2 || lane_count == 4;
}

int apple_dptx_target_encode(const struct apple_dptx_target *target, u32 *value)
{
	if (target->core > FIELD_MAX(DCPDPTX_REMOTE_PORT_CORE) ||
	    target->atc > FIELD_MAX(DCPDPTX_REMOTE_PORT_ATC) ||
	    target->die > FIELD_MAX(DCPDPTX_REMOTE_PORT_DIE))
		return -ERANGE;

	*value = FIELD_PREP(DCPDPTX_REMOTE_PORT_CORE, target->core) |
		 FIELD_PREP(DCPDPTX_REMOTE_PORT_ATC, target->atc) |
		 FIELD_PREP(DCPDPTX_REMOTE_PORT_DIE, target->die) |
		 DCPDPTX_REMOTE_PORT_CONNECTED;

	return 0;
}

int apple_dptx_target_decode(u32 value, struct apple_dptx_target *target)
{
	u32 valid = DCPDPTX_REMOTE_PORT_CORE | DCPDPTX_REMOTE_PORT_ATC |
		    DCPDPTX_REMOTE_PORT_DIE | DCPDPTX_REMOTE_PORT_CONNECTED;

	if ((value & ~valid) || !(value & DCPDPTX_REMOTE_PORT_CONNECTED))
		return -EINVAL;

	target->core = FIELD_GET(DCPDPTX_REMOTE_PORT_CORE, value);
	target->atc = FIELD_GET(DCPDPTX_REMOTE_PORT_ATC, value);
	target->die = FIELD_GET(DCPDPTX_REMOTE_PORT_DIE, value);

	return 0;
}

int apple_dptx_rate_to_mbps(u32 link_rate, u32 *mbps)
{
	switch (link_rate) {
	case LINK_RATE_RBR:
		*mbps = 1620;
		break;
	case LINK_RATE_HBR:
		*mbps = 2700;
		break;
	case LINK_RATE_HBR2:
		*mbps = 5400;
		break;
	case LINK_RATE_HBR3:
		*mbps = 8100;
		break;
	case 0:
		*mbps = 0;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int apple_dptx_usb4_get_max_lane_count(struct dptx_port *dptx,
					      struct apple_dcp *dcp,
					      u32 *lane_count)
{
	*lane_count = dptx->transport.caps.max_lanes;
	return 0;
}

static int apple_dptx_usb4_set_active_lane_count(struct dptx_port *dptx,
						 struct apple_dcp *dcp,
						 u32 lane_count)
{
	if (lane_count && (!apple_dptx_lane_count_valid(lane_count) ||
			   lane_count > dptx->transport.caps.max_lanes))
		return -EINVAL;

	dptx->lane_count = lane_count;
	return 0;
}

static int apple_dptx_usb4_get_max_link_rate(struct dptx_port *dptx,
					     struct apple_dcp *dcp,
					     u32 *link_rate)
{
	*link_rate = dptx->transport.caps.max_link_rate;
	return 0;
}

static int apple_dptx_usb4_set_link_rate(struct dptx_port *dptx,
					 struct apple_dcp *dcp, u32 link_rate)
{
	u32 ignored;

	if (apple_dptx_rate_to_mbps(link_rate, &ignored) ||
	    link_rate > dptx->transport.caps.max_link_rate)
		return -EINVAL;

	dptx->link_rate = link_rate;
	dptx->pending_link_rate = link_rate;
	return 0;
}

static int apple_dptx_usb4_activate(struct dptx_port *dptx,
				    struct apple_dcp *dcp)
{
	dptx->transport.active = true;
	return 0;
}

static int apple_dptx_usb4_deactivate(struct dptx_port *dptx,
				      struct apple_dcp *dcp)
{
	dptx->transport.active = false;
	return 0;
}

/*
 * This backend is intentionally unreachable from production code. It only
 * models DCP firmware's logical link requests without touching a physical PHY.
 */
const struct apple_dptx_transport_ops apple_dptx_usb4_ops = {
	.get_max_lane_count = apple_dptx_usb4_get_max_lane_count,
	.set_active_lane_count = apple_dptx_usb4_set_active_lane_count,
	.get_max_link_rate = apple_dptx_usb4_get_max_link_rate,
	.set_link_rate = apple_dptx_usb4_set_link_rate,
	.activate = apple_dptx_usb4_activate,
	.deactivate = apple_dptx_usb4_deactivate,
};

void apple_dptx_transport_init_physical(struct dptx_port *dptx)
{
	dptx->transport = (struct apple_dptx_transport){
		.ops = &apple_dptx_physical_ops,
		.kind = APPLE_DPTX_TRANSPORT_PHYSICAL,
	};
}

int apple_dptx_transport_init_usb4(struct dptx_port *dptx,
				   const struct apple_dptx_target *target,
				   const struct apple_dptx_link_caps *caps)
{
	u32 ignored;

	if (!apple_dptx_lane_count_valid(caps->max_lanes) ||
	    apple_dptx_rate_to_mbps(caps->max_link_rate, &ignored) ||
	    apple_dptx_target_encode(target, &ignored))
		return -EINVAL;

	dptx->transport = (struct apple_dptx_transport){
		.ops = &apple_dptx_usb4_ops,
		.target = *target,
		.caps = *caps,
		.kind = APPLE_DPTX_TRANSPORT_USB4,
	};

	return 0;
}
