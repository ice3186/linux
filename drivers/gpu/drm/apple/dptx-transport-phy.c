// SPDX-License-Identifier: GPL-2.0-only OR MIT
/* Copyright 2026 Matthew Altman */

#include <linux/phy/phy.h>

#include "dcp-internal.h"
#include "dptx-transport.h"

static int apple_dptx_physical_get_max_lane_count(struct dptx_port *dptx,
						  struct apple_dcp *dcp,
						  u32 *lane_count)
{
	union phy_configure_opts phy_ops;
	int ret;

	ret = phy_validate(dptx->atcphy, PHY_MODE_DP, 0, &phy_ops);
	if (ret < 0) {
		dev_err(dcp->dev, "phy_validate failed: %d\n", ret);
		return ret;
	}

	if (phy_ops.dp.lanes < 2) {
		/* phy_validate can report zero until the ATC PHY enters DP mode. */
		dev_dbg(dcp->dev, "get_max_lane_count: phy lanes: %d\n",
			phy_ops.dp.lanes);
		*lane_count = 4;
	} else {
		*lane_count = phy_ops.dp.lanes;
	}
	dptx->lane_count = *lane_count;

	return 0;
}

static int apple_dptx_physical_set_active_lane_count(struct dptx_port *dptx,
						     struct apple_dcp *dcp,
						     u32 lane_count)
{
	dptx->phy_ops.dp.lanes = lane_count;
	/* Standalone DPTX PHYs require explicit lane programming. */
	dptx->phy_ops.dp.set_lanes = dcp->dptx_phy > 3;
	if (!dptx->phy_ops.dp.set_lanes)
		return 0;

	if (dptx->atcphy) {
		int ret = phy_configure(dptx->atcphy, &dptx->phy_ops);

		if (ret)
			return ret;
	}
	dptx->phy_ops.dp.set_lanes = 0;
	dptx->lane_count = lane_count;

	return 0;
}

static int apple_dptx_physical_get_max_link_rate(struct dptx_port *dptx,
						 struct apple_dcp *dcp,
						 u32 *link_rate)
{
	*link_rate = LINK_RATE_HBR3;
	return 0;
}

static int apple_dptx_physical_set_link_rate(struct dptx_port *dptx,
					     struct apple_dcp *dcp,
					     u32 link_rate)
{
	u32 phy_link_rate;
	int ret;

	ret = apple_dptx_rate_to_mbps(link_rate, &phy_link_rate);
	if (ret) {
		dev_err(dcp->dev,
			"DPTXPort: Unsupported link rate 0x%x requested\n",
			link_rate);
		return ret;
	}

	dptx->phy_ops.dp.link_rate = phy_link_rate;
	dptx->phy_ops.dp.set_rate = 1;
	if (dptx->atcphy) {
		ret = phy_configure(dptx->atcphy, &dptx->phy_ops);
		if (ret)
			return ret;
	}
	dptx->link_rate = link_rate;
	dptx->pending_link_rate = link_rate;

	return 0;
}

static int apple_dptx_physical_activate(struct dptx_port *dptx,
					struct apple_dcp *dcp)
{
	/* Standalone DPTX PHYs use the mode argument to select the DCP input. */
	if (!dcp->typec_mux)
		phy_set_mode_ext(dptx->atcphy, PHY_MODE_DP, dcp->index);

	return 0;
}

static int apple_dptx_physical_deactivate(struct dptx_port *dptx,
					  struct apple_dcp *dcp)
{
	phy_set_mode_ext(dptx->atcphy, PHY_MODE_INVALID, 0);
	return 0;
}

const struct apple_dptx_transport_ops apple_dptx_physical_ops = {
	.get_max_lane_count = apple_dptx_physical_get_max_lane_count,
	.set_active_lane_count = apple_dptx_physical_set_active_lane_count,
	.get_max_link_rate = apple_dptx_physical_get_max_link_rate,
	.set_link_rate = apple_dptx_physical_set_link_rate,
	.activate = apple_dptx_physical_activate,
	.deactivate = apple_dptx_physical_deactivate,
};
