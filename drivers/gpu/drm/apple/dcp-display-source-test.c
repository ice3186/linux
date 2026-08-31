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

static void apple_dcp_display_route_validation_test(struct kunit *test)
{
	struct apple_dcp_display_test_ctx *ctx = test->priv;
	struct apple_dcp_display_route invalid = route_atc2_dpin0;
	struct apple_dcp_display_cookie cookie;
	struct apple_dptx_target target;
	u32 encoded;

	invalid.dpin = 2;
	target = (struct apple_dptx_target) { .core = 1 };
	KUNIT_EXPECT_EQ(test,
		apple_dcp_display_route_to_target(&invalid, &target), -ERANGE);
	KUNIT_EXPECT_EQ(test, target.core, 0);
	KUNIT_EXPECT_EQ(test,
		apple_dcp_display_prepare(&ctx->pool, 0, &invalid, &cookie),
		-ERANGE);
	invalid = route_atc2_dpin0;
	invalid.core = 16;
	target = (struct apple_dptx_target) { .core = 1 };
	KUNIT_EXPECT_EQ(test,
		apple_dcp_display_route_to_target(&invalid, &target), -ERANGE);
	KUNIT_EXPECT_EQ(test, target.core, 0);
	KUNIT_EXPECT_EQ(test,
		apple_dcp_display_prepare(&ctx->pool, 0, &invalid, &cookie),
		-ERANGE);
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
	KUNIT_CASE(apple_dcp_display_route_validation_test),
	{}
};

static struct kunit_suite apple_dcp_display_test_suite = {
	.name = "apple-dcp-display-source",
	.init = apple_dcp_display_test_init,
	.test_cases = apple_dcp_display_test_cases,
};

kunit_test_suite(apple_dcp_display_test_suite);
