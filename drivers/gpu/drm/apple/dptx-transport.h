/* SPDX-License-Identifier: GPL-2.0-only OR MIT */
#ifndef __APPLE_DCP_DPTX_TRANSPORT_H__
#define __APPLE_DCP_DPTX_TRANSPORT_H__

#include <linux/bitfield.h>
#include <linux/types.h>

#define DCPDPTX_REMOTE_PORT_CORE GENMASK(3, 0)
#define DCPDPTX_REMOTE_PORT_ATC GENMASK(7, 4)
#define DCPDPTX_REMOTE_PORT_DIE GENMASK(11, 8)
#define DCPDPTX_REMOTE_PORT_CONNECTED BIT(15)

enum dptx_link_rate {
	LINK_RATE_RBR = 0x06,
	LINK_RATE_HBR = 0x0a,
	LINK_RATE_HBR2 = 0x14,
	LINK_RATE_HBR3 = 0x1e,
};

enum apple_dptx_transport_kind {
	APPLE_DPTX_TRANSPORT_PHYSICAL,
	APPLE_DPTX_TRANSPORT_USB4,
};

struct apple_dptx_target {
	u8 core;
	u8 atc;
	u8 die;
};

struct apple_dptx_link_caps {
	u32 max_lanes;
	u32 max_link_rate;
};

struct apple_dptx_transport_ops;

struct apple_dptx_transport {
	const struct apple_dptx_transport_ops *ops;
	struct apple_dptx_target target;
	struct apple_dptx_link_caps caps;
	enum apple_dptx_transport_kind kind;
	bool active;
};

struct apple_dcp;
struct dptx_port;

struct apple_dptx_transport_ops {
	int (*get_max_lane_count)(struct dptx_port *dptx, struct apple_dcp *dcp,
				  u32 *lane_count);
	int (*set_active_lane_count)(struct dptx_port *dptx,
				     struct apple_dcp *dcp, u32 lane_count);
	int (*get_max_link_rate)(struct dptx_port *dptx, struct apple_dcp *dcp,
				 u32 *link_rate);
	int (*set_link_rate)(struct dptx_port *dptx, struct apple_dcp *dcp,
			     u32 link_rate);
	int (*activate)(struct dptx_port *dptx, struct apple_dcp *dcp);
	int (*deactivate)(struct dptx_port *dptx, struct apple_dcp *dcp);
};

extern const struct apple_dptx_transport_ops apple_dptx_physical_ops;
extern const struct apple_dptx_transport_ops apple_dptx_usb4_ops;

int apple_dptx_target_encode(const struct apple_dptx_target *target,
			     u32 *value);
int apple_dptx_target_decode(u32 value, struct apple_dptx_target *target);
int apple_dptx_rate_to_mbps(u32 link_rate, u32 *mbps);
void apple_dptx_transport_init_physical(struct dptx_port *dptx);
int apple_dptx_transport_init_usb4(struct dptx_port *dptx,
				   const struct apple_dptx_target *target,
				   const struct apple_dptx_link_caps *caps);

#endif
