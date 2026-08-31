// SPDX-License-Identifier: GPL-2.0-only OR MIT

#include <kunit/test.h>

#include "dcp-display-source.h"

#define TEST_ENGINES 2

struct apple_dcp_display_test_ctx {
	struct apple_dcp_display_lease leases[TEST_ENGINES];
	struct apple_dcp_display_pool pool;
};

static const struct apple_dcp_display_route route_atc2_dpin0 = {
	.die = 0,
	.atc = 2,
	.dpin = 0,
	.core = 0,
};

static const struct apple_dcp_display_sink sink_atc2_dpin0 = {
	.die = 0,
	.atc = 2,
	.dpin = 0,
};

static int apple_dcp_display_test_init(struct kunit *test)
{
	struct apple_dcp_display_test_ctx *ctx;

	ctx = kunit_kzalloc(test, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;
	apple_dcp_display_pool_init(&ctx->pool, ctx->leases, TEST_ENGINES);
	test->priv = ctx;

	return apple_dcp_display_begin_session(&ctx->pool);
}

static void apple_dcp_display_lifecycle_test(struct kunit *test)
{
	struct apple_dcp_display_test_ctx *ctx = test->priv;
	struct apple_dcp_display_cookie cookie;

	KUNIT_ASSERT_EQ(test,
		apple_dcp_display_prepare(&ctx->pool, 0, &route_atc2_dpin0,
					  &cookie), 0);
	KUNIT_ASSERT_EQ(test, apple_dcp_display_enable(&ctx->pool, 0, &cookie), 0);
	KUNIT_EXPECT_EQ(test, apple_dcp_display_enable(&ctx->pool, 0, &cookie), 0);
	KUNIT_EXPECT_EQ(test, apple_dcp_display_release(&ctx->pool, 0, &cookie),
			-EBUSY);
	KUNIT_ASSERT_EQ(test, apple_dcp_display_disable(&ctx->pool, 0, &cookie), 0);
	KUNIT_EXPECT_EQ(test, apple_dcp_display_disable(&ctx->pool, 0, &cookie), 0);
	KUNIT_ASSERT_EQ(test, apple_dcp_display_release(&ctx->pool, 0, &cookie), 0);
	KUNIT_EXPECT_EQ(test, ctx->leases[0].phase, APPLE_DCP_DISPLAY_IDLE);
}

static void apple_dcp_display_engine_conflict_test(struct kunit *test)
{
	struct apple_dcp_display_test_ctx *ctx = test->priv;
	struct apple_dcp_display_cookie first, untouched = { .token = 42 };

	KUNIT_ASSERT_EQ(test,
		apple_dcp_display_prepare(&ctx->pool, 0, &route_atc2_dpin0,
					  &first), 0);
	KUNIT_EXPECT_EQ(test,
		apple_dcp_display_prepare(&ctx->pool, 0, &route_atc2_dpin0,
					  &untouched), -EBUSY);
	KUNIT_EXPECT_EQ(test, untouched.token, 42ULL);
	KUNIT_EXPECT_TRUE(test,
		apple_dcp_display_cookie_equal(&ctx->leases[0].cookie, &first));
}

static void apple_dcp_display_sink_conflict_test(struct kunit *test)
{
	struct apple_dcp_display_test_ctx *ctx = test->priv;
	struct apple_dcp_display_route same_sink = route_atc2_dpin0;
	struct apple_dcp_display_cookie first, second;

	same_sink.core = 1;
	KUNIT_ASSERT_EQ(test,
		apple_dcp_display_prepare(&ctx->pool, 0, &route_atc2_dpin0,
					  &first), 0);
	KUNIT_ASSERT_EQ(test, apple_dcp_display_enable(&ctx->pool, 0, &first), 0);
	KUNIT_EXPECT_EQ(test,
		apple_dcp_display_prepare(&ctx->pool, 1, &same_sink, &second),
		-EBUSY);
}

static void apple_dcp_display_independent_dpin_test(struct kunit *test)
{
	struct apple_dcp_display_test_ctx *ctx = test->priv;
	struct apple_dcp_display_route dpin1 = route_atc2_dpin0;
	struct apple_dcp_display_cookie first, second;

	dpin1.dpin = 1;
	KUNIT_ASSERT_EQ(test,
		apple_dcp_display_prepare(&ctx->pool, 0, &route_atc2_dpin0,
					  &first), 0);
	KUNIT_EXPECT_EQ(test,
		apple_dcp_display_prepare(&ctx->pool, 1, &dpin1, &second), 0);
}

static void apple_dcp_display_independent_atc_test(struct kunit *test)
{
	struct apple_dcp_display_test_ctx *ctx = test->priv;
	struct apple_dcp_display_route other_atc = route_atc2_dpin0;
	struct apple_dcp_display_cookie first, second;

	other_atc.atc = 1;
	KUNIT_ASSERT_EQ(test,
		apple_dcp_display_prepare(&ctx->pool, 0, &route_atc2_dpin0,
					  &first), 0);
	KUNIT_EXPECT_EQ(test,
		apple_dcp_display_prepare(&ctx->pool, 1, &other_atc, &second), 0);
}

static void apple_dcp_display_independent_die_test(struct kunit *test)
{
	struct apple_dcp_display_test_ctx *ctx = test->priv;
	struct apple_dcp_display_route other_die = route_atc2_dpin0;
	struct apple_dcp_display_cookie first, second;

	other_die.die = 1;
	KUNIT_ASSERT_EQ(test,
		apple_dcp_display_prepare(&ctx->pool, 0, &route_atc2_dpin0,
					  &first), 0);
	KUNIT_EXPECT_EQ(test,
		apple_dcp_display_prepare(&ctx->pool, 1, &other_die, &second), 0);
}

static void apple_dcp_display_stale_cookie_test(struct kunit *test)
{
	struct apple_dcp_display_test_ctx *ctx = test->priv;
	struct apple_dcp_display_cookie first, second;

	KUNIT_ASSERT_EQ(test,
		apple_dcp_display_prepare(&ctx->pool, 0, &route_atc2_dpin0,
					  &first), 0);
	KUNIT_ASSERT_EQ(test, apple_dcp_display_release(&ctx->pool, 0, &first), 0);
	KUNIT_ASSERT_EQ(test,
		apple_dcp_display_prepare(&ctx->pool, 0, &route_atc2_dpin0,
					  &second), 0);
	KUNIT_EXPECT_EQ(test, apple_dcp_display_enable(&ctx->pool, 0, &first),
			-ESTALE);
	KUNIT_EXPECT_EQ(test, apple_dcp_display_disable(&ctx->pool, 0, &first),
			-ESTALE);
	KUNIT_EXPECT_EQ(test, apple_dcp_display_release(&ctx->pool, 0, &first),
			-ESTALE);
	KUNIT_EXPECT_EQ(test, apple_dcp_display_enable(&ctx->pool, 1, &second),
			-ESTALE);
	KUNIT_EXPECT_TRUE(test,
		apple_dcp_display_cookie_equal(&ctx->leases[0].cookie, &second));
}

static void apple_dcp_display_session_stale_test(struct kunit *test)
{
	struct apple_dcp_display_test_ctx *ctx = test->priv;
	struct apple_dcp_display_cookie first, second;

	KUNIT_ASSERT_EQ(test,
		apple_dcp_display_prepare(&ctx->pool, 0, &route_atc2_dpin0,
					  &first), 0);
	KUNIT_ASSERT_EQ(test, apple_dcp_display_release(&ctx->pool, 0, &first), 0);
	KUNIT_ASSERT_TRUE(test, apple_dcp_display_quiesce(&ctx->pool));
	KUNIT_ASSERT_EQ(test, apple_dcp_display_begin_session(&ctx->pool), 0);
	KUNIT_ASSERT_EQ(test,
		apple_dcp_display_prepare(&ctx->pool, 0, &route_atc2_dpin0,
					  &second), 0);
	KUNIT_EXPECT_EQ(test, first.token, second.token);
	KUNIT_EXPECT_NE(test, first.session, second.session);
	KUNIT_EXPECT_EQ(test, apple_dcp_display_release(&ctx->pool, 0, &first),
			-ESTALE);
}

static void apple_dcp_display_quiesce_drain_test(struct kunit *test)
{
	struct apple_dcp_display_test_ctx *ctx = test->priv;
	struct apple_dcp_display_cookie cookie, ignored;

	KUNIT_ASSERT_EQ(test,
		apple_dcp_display_prepare(&ctx->pool, 0, &route_atc2_dpin0,
					  &cookie), 0);
	KUNIT_EXPECT_TRUE(test, apple_dcp_display_quiesce(&ctx->pool));
	KUNIT_EXPECT_FALSE(test, apple_dcp_display_quiesce(&ctx->pool));
	KUNIT_EXPECT_EQ(test,
		apple_dcp_display_prepare(&ctx->pool, 1, &route_atc2_dpin0,
					  &ignored), -ESHUTDOWN);
	KUNIT_EXPECT_EQ(test, apple_dcp_display_enable(&ctx->pool, 0, &cookie),
			-ESHUTDOWN);
	KUNIT_EXPECT_EQ(test, apple_dcp_display_begin_session(&ctx->pool), -EBUSY);
	KUNIT_ASSERT_EQ(test, apple_dcp_display_disable(&ctx->pool, 0, &cookie), 0);
	KUNIT_ASSERT_EQ(test, apple_dcp_display_release(&ctx->pool, 0, &cookie), 0);
	KUNIT_EXPECT_EQ(test, apple_dcp_display_begin_session(&ctx->pool), 0);
}

static void apple_dcp_display_overflow_test(struct kunit *test)
{
	struct apple_dcp_display_test_ctx *ctx = test->priv;
	struct apple_dcp_display_cookie first, second;

	KUNIT_ASSERT_TRUE(test, apple_dcp_display_quiesce(&ctx->pool));
	ctx->pool.session = ~0ULL;
	KUNIT_EXPECT_EQ(test, apple_dcp_display_begin_session(&ctx->pool),
			-EOVERFLOW);
	ctx->pool.session = 1;
	KUNIT_ASSERT_EQ(test, apple_dcp_display_begin_session(&ctx->pool), 0);
	ctx->pool.next_token = ~0ULL;
	KUNIT_ASSERT_EQ(test,
		apple_dcp_display_prepare(&ctx->pool, 0, &route_atc2_dpin0,
					  &first), 0);
	KUNIT_EXPECT_EQ(test, first.token, ~0ULL);
	KUNIT_ASSERT_EQ(test, apple_dcp_display_release(&ctx->pool, 0, &first), 0);
	KUNIT_EXPECT_EQ(test,
		apple_dcp_display_prepare(&ctx->pool, 0, &route_atc2_dpin0,
					  &second), -EOVERFLOW);
}

static void apple_dcp_display_acquire_order_test(struct kunit *test)
{
	struct apple_dcp_display_test_ctx *ctx = test->priv;
	const struct apple_dcp_display_candidate candidates[] = {
		{ .engine = 1, .core = 3 },
		{ .engine = 0, .core = 2 },
	};
	struct apple_dcp_display_allocation allocation;

	KUNIT_ASSERT_EQ(test,
		apple_dcp_display_acquire(&ctx->pool, &sink_atc2_dpin0,
					  candidates, ARRAY_SIZE(candidates),
					  &allocation), 0);
	KUNIT_EXPECT_EQ(test, allocation.engine, 1U);
	KUNIT_EXPECT_EQ(test, allocation.route.die, sink_atc2_dpin0.die);
	KUNIT_EXPECT_EQ(test, allocation.route.atc, sink_atc2_dpin0.atc);
	KUNIT_EXPECT_EQ(test, allocation.route.dpin, sink_atc2_dpin0.dpin);
	KUNIT_EXPECT_EQ(test, allocation.route.core, 3);
	KUNIT_EXPECT_TRUE(test,
		apple_dcp_display_cookie_equal(&allocation.cookie,
					       &ctx->leases[1].cookie));
	KUNIT_EXPECT_EQ(test, ctx->leases[1].phase,
			APPLE_DCP_DISPLAY_PREPARED);
}

static void apple_dcp_display_acquire_fallback_test(struct kunit *test)
{
	struct apple_dcp_display_test_ctx *ctx = test->priv;
	const struct apple_dcp_display_candidate candidates[] = {
		{ .engine = 0, .core = 0 },
		{ .engine = 1, .core = 1 },
	};
	struct apple_dcp_display_route other_sink = route_atc2_dpin0;
	struct apple_dcp_display_allocation allocation;
	struct apple_dcp_display_cookie busy;

	other_sink.atc = 1;
	KUNIT_ASSERT_EQ(test,
		apple_dcp_display_prepare(&ctx->pool, 0, &other_sink, &busy), 0);
	KUNIT_ASSERT_EQ(test,
		apple_dcp_display_acquire(&ctx->pool, &sink_atc2_dpin0,
					  candidates, ARRAY_SIZE(candidates),
					  &allocation), 0);
	KUNIT_EXPECT_EQ(test, allocation.engine, 1U);
	KUNIT_EXPECT_EQ(test, allocation.route.core, 1);
}

static void apple_dcp_display_acquire_atomic_validation_test(struct kunit *test)
{
	struct apple_dcp_display_test_ctx *ctx = test->priv;
	const struct apple_dcp_display_candidate candidates[] = {
		{ .engine = 0, .core = 0 },
		{ .engine = 1, .core = 16 },
	};
	struct apple_dcp_display_allocation allocation = {
		.engine = 7,
		.route = { .die = 7, .atc = 7, .dpin = 7, .core = 7 },
		.cookie = { .session = 7, .token = 7 },
	};
	u64 next_token = ctx->pool.next_token;

	KUNIT_EXPECT_EQ(test,
		apple_dcp_display_acquire(&ctx->pool, &sink_atc2_dpin0,
					  candidates, ARRAY_SIZE(candidates),
					  &allocation), -ERANGE);
	KUNIT_EXPECT_EQ(test, allocation.engine, 0U);
	KUNIT_EXPECT_EQ(test, allocation.route.core, 0);
	KUNIT_EXPECT_EQ(test, allocation.cookie.session, 0ULL);
	KUNIT_EXPECT_EQ(test, allocation.cookie.token, 0ULL);
	KUNIT_EXPECT_EQ(test, ctx->pool.next_token, next_token);
	KUNIT_EXPECT_EQ(test, ctx->leases[0].phase, APPLE_DCP_DISPLAY_IDLE);
	KUNIT_EXPECT_EQ(test, ctx->leases[1].phase, APPLE_DCP_DISPLAY_IDLE);
}

static void apple_dcp_display_acquire_input_test(struct kunit *test)
{
	struct apple_dcp_display_test_ctx *ctx = test->priv;
	const struct apple_dcp_display_candidate duplicate[] = {
		{ .engine = 0, .core = 4 },
		{ .engine = 0, .core = 5 },
	};
	const struct apple_dcp_display_candidate invalid_engine = {
		.engine = TEST_ENGINES,
	};
	struct apple_dcp_display_sink opaque_dpin = sink_atc2_dpin0;
	struct apple_dcp_display_allocation allocation;

	KUNIT_EXPECT_EQ(test,
		apple_dcp_display_acquire(&ctx->pool, &sink_atc2_dpin0,
					  duplicate, 0, &allocation), -EINVAL);
	KUNIT_EXPECT_EQ(test,
		apple_dcp_display_acquire(&ctx->pool, &sink_atc2_dpin0,
					  NULL, 1, &allocation), -EINVAL);
	KUNIT_EXPECT_EQ(test,
		apple_dcp_display_acquire(&ctx->pool, NULL, duplicate,
					  ARRAY_SIZE(duplicate), &allocation),
		-EINVAL);
	KUNIT_EXPECT_EQ(test,
		apple_dcp_display_acquire(&ctx->pool, &sink_atc2_dpin0,
					  &invalid_engine, 1, &allocation),
		-EINVAL);
	KUNIT_EXPECT_EQ(test,
		apple_dcp_display_acquire(&ctx->pool, &sink_atc2_dpin0,
					  duplicate, ARRAY_SIZE(duplicate), NULL),
		-EINVAL);

	KUNIT_ASSERT_EQ(test,
		apple_dcp_display_acquire(&ctx->pool, &sink_atc2_dpin0,
					  duplicate, ARRAY_SIZE(duplicate),
					  &allocation), 0);
	KUNIT_EXPECT_EQ(test, allocation.engine, 0U);
	KUNIT_EXPECT_EQ(test, allocation.route.core, 4);
	KUNIT_ASSERT_EQ(test,
		apple_dcp_display_release(&ctx->pool, allocation.engine,
					  &allocation.cookie), 0);

	/* DP-IN is opaque identity here; the topology provider owns its range. */
	opaque_dpin.dpin = 42;
	KUNIT_ASSERT_EQ(test,
		apple_dcp_display_acquire(&ctx->pool, &opaque_dpin, duplicate,
					  ARRAY_SIZE(duplicate), &allocation), 0);
	KUNIT_EXPECT_EQ(test, allocation.route.dpin, 42);
}

static void apple_dcp_display_acquire_busy_test(struct kunit *test)
{
	struct apple_dcp_display_test_ctx *ctx = test->priv;
	const struct apple_dcp_display_candidate candidates[] = {
		{ .engine = 0, .core = 0 },
		{ .engine = 1, .core = 1 },
	};
	struct apple_dcp_display_route first_route = route_atc2_dpin0;
	struct apple_dcp_display_route second_route = route_atc2_dpin0;
	struct apple_dcp_display_allocation allocation = { .engine = 7 };
	struct apple_dcp_display_cookie first, second;

	first_route.atc = 0;
	second_route.atc = 1;
	KUNIT_ASSERT_EQ(test,
		apple_dcp_display_prepare(&ctx->pool, 0, &first_route, &first), 0);
	KUNIT_ASSERT_EQ(test,
		apple_dcp_display_prepare(&ctx->pool, 1, &second_route, &second), 0);
	KUNIT_EXPECT_EQ(test,
		apple_dcp_display_acquire(&ctx->pool, &sink_atc2_dpin0,
					  candidates, ARRAY_SIZE(candidates),
					  &allocation), -EBUSY);
	KUNIT_EXPECT_EQ(test, allocation.engine, 0U);
}

static void apple_dcp_display_acquire_quarantine_fallback_test(struct kunit *test)
{
	struct apple_dcp_display_test_ctx *ctx = test->priv;
	const struct apple_dcp_display_candidate candidates[] = {
		{ .engine = 0, .core = 0 },
		{ .engine = 1, .core = 1 },
	};
	struct apple_dcp_display_route poisoned_sink = route_atc2_dpin0;
	struct apple_dcp_display_sink independent_sink = sink_atc2_dpin0;
	struct apple_dcp_display_allocation allocation;
	struct apple_dcp_display_cookie cookie;

	poisoned_sink.atc = 0;
	independent_sink.atc = 1;
	KUNIT_ASSERT_EQ(test,
		apple_dcp_display_prepare(&ctx->pool, 0, &poisoned_sink,
					  &cookie), 0);
	KUNIT_ASSERT_EQ(test,
		apple_dcp_display_quarantine(&ctx->pool, 0, &cookie), 0);
	KUNIT_ASSERT_EQ(test,
		apple_dcp_display_acquire(&ctx->pool, &independent_sink,
					  candidates, ARRAY_SIZE(candidates),
					  &allocation), 0);
	KUNIT_EXPECT_EQ(test, allocation.engine, 1U);
	KUNIT_EXPECT_EQ(test, allocation.route.core, 1);
}

static void apple_dcp_display_acquire_shutdown_overflow_test(struct kunit *test)
{
	struct apple_dcp_display_test_ctx *ctx = test->priv;
	const struct apple_dcp_display_candidate candidate = {
		.engine = 0,
		.core = 0,
	};
	struct apple_dcp_display_allocation allocation = { .engine = 7 };

	KUNIT_ASSERT_TRUE(test, apple_dcp_display_quiesce(&ctx->pool));
	KUNIT_EXPECT_EQ(test,
		apple_dcp_display_acquire(&ctx->pool, &sink_atc2_dpin0,
					  &candidate, 1, &allocation),
		-ESHUTDOWN);
	KUNIT_EXPECT_EQ(test, allocation.engine, 0U);
	KUNIT_ASSERT_EQ(test, apple_dcp_display_begin_session(&ctx->pool), 0);
	ctx->pool.next_token = 0;
	allocation.engine = 7;
	KUNIT_EXPECT_EQ(test,
		apple_dcp_display_acquire(&ctx->pool, &sink_atc2_dpin0,
					  &candidate, 1, &allocation),
		-EOVERFLOW);
	KUNIT_EXPECT_EQ(test, allocation.engine, 0U);
}

static void apple_dcp_display_quarantine_lifecycle_test(struct kunit *test)
{
	struct apple_dcp_display_test_ctx *ctx = test->priv;
	struct apple_dcp_display_cookie cookie, stale;
	struct apple_dcp_display_lease before;

	KUNIT_ASSERT_EQ(test,
		apple_dcp_display_prepare(&ctx->pool, 0, &route_atc2_dpin0,
					  &cookie), 0);
	before = ctx->leases[0];
	stale = cookie;
	stale.token++;
	KUNIT_EXPECT_EQ(test,
		apple_dcp_display_quarantine(&ctx->pool, 0, &stale), -ESTALE);
	KUNIT_EXPECT_EQ(test, ctx->leases[0].phase,
			APPLE_DCP_DISPLAY_PREPARED);
	KUNIT_ASSERT_EQ(test,
		apple_dcp_display_quarantine(&ctx->pool, 0, &cookie), 0);
	KUNIT_EXPECT_EQ(test, ctx->leases[0].route.atc, before.route.atc);
	KUNIT_EXPECT_TRUE(test,
		apple_dcp_display_cookie_equal(&ctx->leases[0].cookie,
					       &before.cookie));
	KUNIT_EXPECT_EQ(test,
		apple_dcp_display_quarantine(&ctx->pool, 0, &cookie), 0);
	KUNIT_EXPECT_EQ(test, apple_dcp_display_enable(&ctx->pool, 0, &cookie),
			-EIO);
	KUNIT_EXPECT_EQ(test, apple_dcp_display_disable(&ctx->pool, 0, &cookie),
			-EIO);
	KUNIT_EXPECT_EQ(test, apple_dcp_display_release(&ctx->pool, 0, &cookie),
			-EIO);
	KUNIT_EXPECT_EQ(test, apple_dcp_display_enable(&ctx->pool, 0, &stale),
			-ESTALE);
	KUNIT_EXPECT_EQ(test,
		apple_dcp_display_quarantine(&ctx->pool, 1, &cookie), -ESTALE);
	KUNIT_EXPECT_EQ(test,
		apple_dcp_display_quarantine(&ctx->pool, 1, NULL), -EINVAL);
}

static void apple_dcp_display_quarantine_enabled_test(struct kunit *test)
{
	struct apple_dcp_display_test_ctx *ctx = test->priv;
	struct apple_dcp_display_cookie cookie;

	KUNIT_ASSERT_EQ(test,
		apple_dcp_display_prepare(&ctx->pool, 0, &route_atc2_dpin0,
					  &cookie), 0);
	KUNIT_ASSERT_EQ(test, apple_dcp_display_enable(&ctx->pool, 0, &cookie), 0);
	KUNIT_EXPECT_EQ(test,
		apple_dcp_display_quarantine(&ctx->pool, 0, &cookie), 0);
	KUNIT_EXPECT_EQ(test, ctx->leases[0].phase,
			APPLE_DCP_DISPLAY_QUARANTINED);
}

static void apple_dcp_display_quarantine_conflict_test(struct kunit *test)
{
	struct apple_dcp_display_test_ctx *ctx = test->priv;
	const struct apple_dcp_display_candidate candidate = {
		.engine = 1,
		.core = 1,
	};
	struct apple_dcp_display_allocation allocation;
	struct apple_dcp_display_cookie cookie;
	struct apple_dcp_display_route other_sink = route_atc2_dpin0;
	struct apple_dcp_display_cookie ignored;

	KUNIT_ASSERT_EQ(test,
		apple_dcp_display_prepare(&ctx->pool, 0, &route_atc2_dpin0,
					  &cookie), 0);
	KUNIT_ASSERT_EQ(test,
		apple_dcp_display_quarantine(&ctx->pool, 0, &cookie), 0);
	KUNIT_EXPECT_EQ(test,
		apple_dcp_display_acquire(&ctx->pool, &sink_atc2_dpin0,
					  &candidate, 1, &allocation), -EIO);
	KUNIT_EXPECT_EQ(test,
		apple_dcp_display_prepare(&ctx->pool, 1, &route_atc2_dpin0,
					  &ignored), -EIO);
	other_sink.atc = 1;
	KUNIT_EXPECT_EQ(test,
		apple_dcp_display_prepare(&ctx->pool, 0, &other_sink, &ignored),
		-EIO);
}

static void apple_dcp_display_all_quarantined_test(struct kunit *test)
{
	struct apple_dcp_display_test_ctx *ctx = test->priv;
	const struct apple_dcp_display_candidate candidates[] = {
		{ .engine = 0, .core = 0 },
		{ .engine = 1, .core = 1 },
	};
	struct apple_dcp_display_route first_route = route_atc2_dpin0;
	struct apple_dcp_display_route second_route = route_atc2_dpin0;
	struct apple_dcp_display_sink third_sink = sink_atc2_dpin0;
	struct apple_dcp_display_allocation allocation;
	struct apple_dcp_display_cookie first, second;

	first_route.atc = 0;
	second_route.atc = 1;
	third_sink.atc = 3;
	KUNIT_ASSERT_EQ(test,
		apple_dcp_display_prepare(&ctx->pool, 0, &first_route, &first), 0);
	KUNIT_ASSERT_EQ(test,
		apple_dcp_display_prepare(&ctx->pool, 1, &second_route, &second), 0);
	KUNIT_ASSERT_EQ(test,
		apple_dcp_display_quarantine(&ctx->pool, 0, &first), 0);
	KUNIT_ASSERT_EQ(test,
		apple_dcp_display_quarantine(&ctx->pool, 1, &second), 0);
	KUNIT_EXPECT_EQ(test,
		apple_dcp_display_acquire(&ctx->pool, &third_sink, candidates,
					  ARRAY_SIZE(candidates), &allocation), -EIO);
	KUNIT_ASSERT_TRUE(test, apple_dcp_display_quiesce(&ctx->pool));
	KUNIT_EXPECT_EQ(test, apple_dcp_display_begin_session(&ctx->pool), -EIO);
}

static void apple_dcp_display_mixed_quarantine_test(struct kunit *test)
{
	struct apple_dcp_display_test_ctx *ctx = test->priv;
	const struct apple_dcp_display_candidate candidates[] = {
		{ .engine = 0, .core = 0 },
		{ .engine = 1, .core = 1 },
	};
	struct apple_dcp_display_route first_route = route_atc2_dpin0;
	struct apple_dcp_display_route second_route = route_atc2_dpin0;
	struct apple_dcp_display_sink third_sink = sink_atc2_dpin0;
	struct apple_dcp_display_allocation allocation;
	struct apple_dcp_display_cookie first, second;

	first_route.atc = 0;
	second_route.atc = 1;
	third_sink.atc = 3;
	KUNIT_ASSERT_EQ(test,
		apple_dcp_display_prepare(&ctx->pool, 0, &first_route, &first), 0);
	KUNIT_ASSERT_EQ(test,
		apple_dcp_display_prepare(&ctx->pool, 1, &second_route, &second), 0);
	KUNIT_ASSERT_EQ(test,
		apple_dcp_display_quarantine(&ctx->pool, 0, &first), 0);
	KUNIT_EXPECT_EQ(test,
		apple_dcp_display_acquire(&ctx->pool, &third_sink, candidates,
					  ARRAY_SIZE(candidates), &allocation),
		-EBUSY);
	KUNIT_ASSERT_TRUE(test, apple_dcp_display_quiesce(&ctx->pool));
	/* A terminal quarantine takes precedence over a drainable lease. */
	KUNIT_EXPECT_EQ(test, apple_dcp_display_begin_session(&ctx->pool), -EIO);
}

static void apple_dcp_display_restart_epoch_test(struct kunit *test)
{
	struct apple_dcp_display_test_ctx *ctx = test->priv;
	struct apple_dcp_display_cookie old, fresh;
	struct apple_dcp_display_restart_ticket ticket = {};
	u64 session;

	KUNIT_ASSERT_EQ(test,
		apple_dcp_display_prepare(&ctx->pool, 0, &route_atc2_dpin0, &old), 0);
	KUNIT_ASSERT_EQ(test,
		apple_dcp_display_quarantine(&ctx->pool, 0, &old), 0);
	KUNIT_EXPECT_EQ(test,
		apple_dcp_display_endpoint_restarted(&ctx->pool, &ticket),
		-EBUSY);
	KUNIT_ASSERT_EQ(test,
		apple_dcp_display_quiesce_for_restart(&ctx->pool, &ticket), 0);
	session = ctx->pool.session;
	KUNIT_ASSERT_EQ(test,
		apple_dcp_display_endpoint_restarted(&ctx->pool, &ticket), 0);
	KUNIT_EXPECT_EQ(test, ctx->pool.session, session);
	KUNIT_EXPECT_TRUE(test, ctx->pool.quiescing);
	KUNIT_EXPECT_EQ(test, ctx->leases[0].phase, APPLE_DCP_DISPLAY_IDLE);
	KUNIT_ASSERT_EQ(test, apple_dcp_display_begin_session(&ctx->pool), 0);
	KUNIT_EXPECT_EQ(test, ctx->pool.session, session + 1);
	KUNIT_ASSERT_EQ(test,
		apple_dcp_display_prepare(&ctx->pool, 0, &route_atc2_dpin0,
					  &fresh), 0);
	KUNIT_EXPECT_EQ(test, old.token, fresh.token);
	KUNIT_EXPECT_NE(test, old.session, fresh.session);
	KUNIT_EXPECT_EQ(test, apple_dcp_display_release(&ctx->pool, 0, &old),
			-ESTALE);
}

static void apple_dcp_display_restart_exhaustion_test(struct kunit *test)
{
	struct apple_dcp_display_test_ctx *ctx = test->priv;
	struct apple_dcp_display_restart_ticket ticket;

	ctx->pool.session = ~0ULL;
	KUNIT_ASSERT_EQ(test,
		apple_dcp_display_quiesce_for_restart(&ctx->pool, &ticket), 0);
	KUNIT_ASSERT_EQ(test,
		apple_dcp_display_endpoint_restarted(&ctx->pool, &ticket), 0);
	KUNIT_EXPECT_EQ(test, apple_dcp_display_begin_session(&ctx->pool),
			-EOVERFLOW);
}

static void apple_dcp_display_restart_active_rejected_test(struct kunit *test)
{
	struct apple_dcp_display_test_ctx *ctx = test->priv;
	struct apple_dcp_display_route other_route = route_atc2_dpin0;
	struct apple_dcp_display_restart_ticket ticket;
	struct apple_dcp_display_cookie prepared, enabled;

	other_route.atc = 1;
	KUNIT_ASSERT_EQ(test,
		apple_dcp_display_prepare(&ctx->pool, 0, &route_atc2_dpin0,
					  &prepared), 0);
	KUNIT_ASSERT_EQ(test,
		apple_dcp_display_prepare(&ctx->pool, 1, &other_route, &enabled), 0);
	KUNIT_ASSERT_EQ(test,
		apple_dcp_display_enable(&ctx->pool, 1, &enabled), 0);
	KUNIT_ASSERT_EQ(test,
		apple_dcp_display_quiesce_for_restart(&ctx->pool, &ticket), 0);
	KUNIT_EXPECT_EQ(test,
		apple_dcp_display_endpoint_restarted(&ctx->pool, &ticket),
		-EBUSY);
	KUNIT_EXPECT_EQ(test, ctx->leases[0].phase,
			APPLE_DCP_DISPLAY_PREPARED);
	KUNIT_EXPECT_EQ(test, ctx->leases[1].phase,
			APPLE_DCP_DISPLAY_ENABLED);
	KUNIT_EXPECT_TRUE(test, ctx->pool.restart_armed);
}

static void apple_dcp_display_restart_ticket_stale_test(struct kunit *test)
{
	struct apple_dcp_display_test_ctx *ctx = test->priv;
	struct apple_dcp_display_restart_ticket old, fresh;

	KUNIT_ASSERT_EQ(test,
		apple_dcp_display_quiesce_for_restart(&ctx->pool, &old), 0);
	KUNIT_ASSERT_EQ(test,
		apple_dcp_display_endpoint_restarted(&ctx->pool, &old), 0);
	KUNIT_EXPECT_EQ(test,
		apple_dcp_display_endpoint_restarted(&ctx->pool, &old), -ESTALE);
	KUNIT_ASSERT_EQ(test, apple_dcp_display_begin_session(&ctx->pool), 0);
	KUNIT_ASSERT_EQ(test,
		apple_dcp_display_quiesce_for_restart(&ctx->pool, &fresh), 0);
	KUNIT_EXPECT_EQ(test,
		apple_dcp_display_endpoint_restarted(&ctx->pool, &old), -ESTALE);
	KUNIT_EXPECT_TRUE(test, ctx->pool.restart_armed);
	KUNIT_ASSERT_EQ(test,
		apple_dcp_display_endpoint_restarted(&ctx->pool, &fresh), 0);
}

static void apple_dcp_display_route_validation_test(struct kunit *test)
{
	struct apple_dcp_display_test_ctx *ctx = test->priv;
	struct apple_dcp_display_route invalid = route_atc2_dpin0;
	struct apple_dcp_display_cookie cookie;
	struct apple_dptx_target target;
	u32 encoded;

	invalid.core = 16;
	target = (struct apple_dptx_target) { .core = 1 };
	KUNIT_EXPECT_EQ(test,
		apple_dcp_display_route_to_target(&invalid, &target), -ERANGE);
	KUNIT_EXPECT_EQ(test, target.core, 0);
	KUNIT_EXPECT_EQ(test,
		apple_dcp_display_prepare(&ctx->pool, 0, &invalid, &cookie),
		-ERANGE);
	invalid = route_atc2_dpin0;
	invalid.dpin = 42;
	target = (struct apple_dptx_target) { .core = 1 };
	KUNIT_EXPECT_EQ(test,
		apple_dcp_display_route_to_target(&invalid, &target), 0);
	KUNIT_EXPECT_EQ(test, target.core, invalid.core);
	target = (struct apple_dptx_target) { .core = 1 };
	KUNIT_EXPECT_EQ(test, apple_dcp_display_route_to_target(NULL, &target),
			-EINVAL);
	KUNIT_EXPECT_EQ(test, target.core, 0);
	KUNIT_EXPECT_EQ(test,
		apple_dcp_display_prepare(&ctx->pool, TEST_ENGINES,
					  &route_atc2_dpin0, &cookie),
		-EINVAL);
	KUNIT_EXPECT_EQ(test,
		apple_dcp_display_enable(&ctx->pool, TEST_ENGINES, &cookie),
		-EINVAL);
	KUNIT_EXPECT_EQ(test,
		apple_dcp_display_disable(&ctx->pool, TEST_ENGINES, &cookie),
		-EINVAL);
	KUNIT_EXPECT_EQ(test,
		apple_dcp_display_release(&ctx->pool, TEST_ENGINES, &cookie),
		-EINVAL);

	/* Public traces observed this tuple; it is not J414c route policy. */
	KUNIT_ASSERT_EQ(test,
		apple_dcp_display_route_to_target(&route_atc2_dpin0, &target), 0);
	KUNIT_ASSERT_EQ(test, apple_dptx_target_encode(&target, &encoded), 0);
	KUNIT_EXPECT_EQ(test, target.die, route_atc2_dpin0.die);
	KUNIT_EXPECT_EQ(test, target.atc, route_atc2_dpin0.atc);
	KUNIT_EXPECT_EQ(test, target.core, route_atc2_dpin0.core);
}

static struct kunit_case apple_dcp_display_test_cases[] = {
	KUNIT_CASE(apple_dcp_display_lifecycle_test),
	KUNIT_CASE(apple_dcp_display_engine_conflict_test),
	KUNIT_CASE(apple_dcp_display_sink_conflict_test),
	KUNIT_CASE(apple_dcp_display_independent_dpin_test),
	KUNIT_CASE(apple_dcp_display_independent_atc_test),
	KUNIT_CASE(apple_dcp_display_independent_die_test),
	KUNIT_CASE(apple_dcp_display_stale_cookie_test),
	KUNIT_CASE(apple_dcp_display_session_stale_test),
	KUNIT_CASE(apple_dcp_display_quiesce_drain_test),
	KUNIT_CASE(apple_dcp_display_overflow_test),
	KUNIT_CASE(apple_dcp_display_acquire_order_test),
	KUNIT_CASE(apple_dcp_display_acquire_fallback_test),
	KUNIT_CASE(apple_dcp_display_acquire_atomic_validation_test),
	KUNIT_CASE(apple_dcp_display_acquire_input_test),
	KUNIT_CASE(apple_dcp_display_acquire_busy_test),
	KUNIT_CASE(apple_dcp_display_acquire_quarantine_fallback_test),
	KUNIT_CASE(apple_dcp_display_acquire_shutdown_overflow_test),
	KUNIT_CASE(apple_dcp_display_quarantine_lifecycle_test),
	KUNIT_CASE(apple_dcp_display_quarantine_enabled_test),
	KUNIT_CASE(apple_dcp_display_quarantine_conflict_test),
	KUNIT_CASE(apple_dcp_display_all_quarantined_test),
	KUNIT_CASE(apple_dcp_display_mixed_quarantine_test),
	KUNIT_CASE(apple_dcp_display_restart_epoch_test),
	KUNIT_CASE(apple_dcp_display_restart_exhaustion_test),
	KUNIT_CASE(apple_dcp_display_restart_active_rejected_test),
	KUNIT_CASE(apple_dcp_display_restart_ticket_stale_test),
	KUNIT_CASE(apple_dcp_display_route_validation_test),
	{}
};

static struct kunit_suite apple_dcp_display_test_suite = {
	.name = "apple-dcp-display-source",
	.init = apple_dcp_display_test_init,
	.test_cases = apple_dcp_display_test_cases,
};

kunit_test_suite(apple_dcp_display_test_suite);
