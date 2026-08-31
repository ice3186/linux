// SPDX-License-Identifier: GPL-2.0-only OR MIT

#include <kunit/test.h>

#include "dptxep.h"

static void apple_dptx_target_round_trip_test(struct kunit *test)
{
	const struct apple_dptx_target fixtures[] = {
		{ .core = 0, .atc = 0, .die = 0 },
		{ .core = 0, .atc = 1, .die = 0 },
		/* Public traces observed this tuple; it is not J414c policy. */
		{ .core = 0, .atc = 2, .die = 0 },
		{ .core = 15, .atc = 15, .die = 15 },
	};
	struct apple_dptx_target decoded;
	u32 encoded;
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(fixtures); i++) {
		ret = apple_dptx_target_encode(&fixtures[i], &encoded);
		KUNIT_ASSERT_EQ(test, ret, 0);
		ret = apple_dptx_target_decode(encoded, &decoded);
		KUNIT_ASSERT_EQ(test, ret, 0);
		KUNIT_EXPECT_EQ(test, decoded.core, fixtures[i].core);
		KUNIT_EXPECT_EQ(test, decoded.atc, fixtures[i].atc);
		KUNIT_EXPECT_EQ(test, decoded.die, fixtures[i].die);
	}
}

static void apple_dptx_target_rejects_invalid_test(struct kunit *test)
{
	struct apple_dptx_target target = { .core = 16 };
	u32 invalid = BIT(14) | DCPDPTX_REMOTE_PORT_CONNECTED;
	u32 encoded;
	int ret;

	ret = apple_dptx_target_encode(&target, &encoded);
	KUNIT_EXPECT_EQ(test, ret, -ERANGE);
	ret = apple_dptx_target_decode(invalid, &target);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
	KUNIT_EXPECT_EQ(test, apple_dptx_target_decode(0, &target), -EINVAL);
}

static void apple_dptx_rate_mapping_test(struct kunit *test)
{
	static const struct {
		u32 rate;
		u32 mbps;
	} fixtures[] = {
		{ 0, 0 },
		{ LINK_RATE_RBR, 1620 },
		{ LINK_RATE_HBR, 2700 },
		{ LINK_RATE_HBR2, 5400 },
		{ LINK_RATE_HBR3, 8100 },
	};
	u32 mbps;
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(fixtures); i++) {
		ret = apple_dptx_rate_to_mbps(fixtures[i].rate, &mbps);
		KUNIT_ASSERT_EQ(test, ret, 0);
		KUNIT_EXPECT_EQ(test, mbps, fixtures[i].mbps);
	}
	KUNIT_EXPECT_EQ(test, apple_dptx_rate_to_mbps(0xff, &mbps), -EINVAL);
}

static void apple_dptx_usb4_transport_test(struct kunit *test)
{
	const struct apple_dptx_target target = { .core = 0, .atc = 2 };
	const struct apple_dptx_link_caps caps = {
		.max_lanes = 4,
		.max_link_rate = LINK_RATE_HBR3,
	};
	struct dptx_port dptx = {};
	u32 value;
	int ret;

	ret = apple_dptx_transport_init_usb4(&dptx, &target, &caps);
	KUNIT_ASSERT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, dptx.transport.kind, APPLE_DPTX_TRANSPORT_USB4);
	ret = dptx.transport.ops->get_max_lane_count(&dptx, NULL, &value);
	KUNIT_ASSERT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, value, 4U);
	ret = dptx.transport.ops->get_max_link_rate(&dptx, NULL, &value);
	KUNIT_ASSERT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, value, (u32)LINK_RATE_HBR3);

	ret = dptx.transport.ops->set_active_lane_count(&dptx, NULL, 2);
	KUNIT_ASSERT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, dptx.lane_count, 2U);
	ret = dptx.transport.ops->set_active_lane_count(&dptx, NULL, 3);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
	KUNIT_EXPECT_EQ(test, dptx.lane_count, 2U);
	ret = dptx.transport.ops->set_active_lane_count(&dptx, NULL, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, dptx.lane_count, 0U);

	ret = dptx.transport.ops->set_link_rate(&dptx, NULL, LINK_RATE_HBR2);
	KUNIT_ASSERT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, dptx.link_rate, (u32)LINK_RATE_HBR2);
	ret = dptx.transport.ops->set_link_rate(&dptx, NULL, 0xff);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
	KUNIT_EXPECT_EQ(test, dptx.link_rate, (u32)LINK_RATE_HBR2);
	ret = dptx.transport.ops->set_link_rate(&dptx, NULL, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, dptx.link_rate, 0U);

	KUNIT_ASSERT_EQ(test, dptx.transport.ops->activate(&dptx, NULL), 0);
	KUNIT_EXPECT_TRUE(test, dptx.transport.active);
	KUNIT_ASSERT_EQ(test, dptx.transport.ops->deactivate(&dptx, NULL), 0);
	KUNIT_EXPECT_FALSE(test, dptx.transport.active);
}

static void apple_dptx_usb4_caps_validation_test(struct kunit *test)
{
	const struct apple_dptx_target target = {};
	struct apple_dptx_link_caps caps = {
		.max_lanes = 3,
		.max_link_rate = LINK_RATE_HBR3,
	};
	struct dptx_port dptx = {};
	int ret;

	apple_dptx_transport_init_physical(&dptx);
	ret = apple_dptx_transport_init_usb4(&dptx, &target, &caps);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
	KUNIT_EXPECT_PTR_EQ(test, dptx.transport.ops,
			    &apple_dptx_physical_ops);
	KUNIT_EXPECT_EQ(test, dptx.transport.kind,
			APPLE_DPTX_TRANSPORT_PHYSICAL);
	caps.max_lanes = 4;
	caps.max_link_rate = 0xff;
	ret = apple_dptx_transport_init_usb4(&dptx, &target, &caps);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
}

static void apple_dptx_usb4_over_caps_test(struct kunit *test)
{
	const struct apple_dptx_target target = {};
	const struct apple_dptx_link_caps caps = {
		.max_lanes = 2,
		.max_link_rate = LINK_RATE_HBR,
	};
	struct dptx_port dptx = {};
	int ret;

	ret = apple_dptx_transport_init_usb4(&dptx, &target, &caps);
	KUNIT_ASSERT_EQ(test, ret, 0);
	ret = dptx.transport.ops->set_active_lane_count(&dptx, NULL, 4);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
	KUNIT_EXPECT_EQ(test, dptx.lane_count, 0U);
	ret = dptx.transport.ops->set_link_rate(&dptx, NULL, LINK_RATE_HBR3);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
	KUNIT_EXPECT_EQ(test, dptx.link_rate, 0U);
}

static void apple_dptx_physical_is_default_test(struct kunit *test)
{
	struct dptx_port dptx = {};

	apple_dptx_transport_init_physical(&dptx);
	KUNIT_EXPECT_PTR_EQ(test, dptx.transport.ops, &apple_dptx_physical_ops);
	KUNIT_EXPECT_EQ(test, dptx.transport.kind,
			APPLE_DPTX_TRANSPORT_PHYSICAL);
}

static struct kunit_case apple_dptx_transport_test_cases[] = {
	KUNIT_CASE(apple_dptx_target_round_trip_test),
	KUNIT_CASE(apple_dptx_target_rejects_invalid_test),
	KUNIT_CASE(apple_dptx_rate_mapping_test),
	KUNIT_CASE(apple_dptx_usb4_transport_test),
	KUNIT_CASE(apple_dptx_usb4_caps_validation_test),
	KUNIT_CASE(apple_dptx_usb4_over_caps_test),
	KUNIT_CASE(apple_dptx_physical_is_default_test),
	{}
};

static struct kunit_suite apple_dptx_transport_test_suite = {
	.name = "apple-dptx-transport",
	.test_cases = apple_dptx_transport_test_cases,
};

kunit_test_suite(apple_dptx_transport_test_suite);
