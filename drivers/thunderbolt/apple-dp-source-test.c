// SPDX-License-Identifier: GPL-2.0

#include <kunit/test.h>

#include "apple-dp-source.h"

static const struct apple_dp_source_endpoint endpoint_a = {
	.in_route = 0,
	.out_route = 1,
	.in_port = 5,
	.out_port = 14,
};

static const struct apple_dp_source_endpoint endpoint_b = {
	.in_route = 0,
	.out_route = 2,
	.in_port = 6,
	.out_port = 15,
};

static void apple_dp_source_test_lifecycle(struct kunit *test)
{
	struct apple_dp_source_cookie cookie;
	struct apple_dp_source_state state;

	apple_dp_source_state_init(&state);
	KUNIT_EXPECT_EQ(test, apple_dp_source_prepare(&state, &endpoint_a, &cookie),
			-ESHUTDOWN);
	KUNIT_ASSERT_EQ(test, apple_dp_source_begin_session(&state), 0);
	KUNIT_ASSERT_EQ(test, apple_dp_source_prepare(&state, &endpoint_a, &cookie), 0);
	KUNIT_EXPECT_EQ(test, state.phase, APPLE_DP_SOURCE_PREPARED);
	KUNIT_ASSERT_EQ(test, apple_dp_source_enable(&state, &cookie), 0);
	KUNIT_EXPECT_EQ(test, state.phase, APPLE_DP_SOURCE_ENABLED);
	KUNIT_EXPECT_EQ(test, apple_dp_source_enable(&state, &cookie), 0);
	KUNIT_EXPECT_EQ(test, state.generation, 2ULL);
	KUNIT_EXPECT_TRUE(test, apple_dp_source_disable(&state, &cookie));
	KUNIT_EXPECT_EQ(test, state.phase, APPLE_DP_SOURCE_PREPARED);
	KUNIT_EXPECT_TRUE(test, apple_dp_source_unprepare(&state, &cookie));
	KUNIT_EXPECT_EQ(test, state.phase, APPLE_DP_SOURCE_IDLE);
	KUNIT_EXPECT_EQ(test, state.generation, 4ULL);
}

static void apple_dp_source_test_conflict_is_unchanged(struct kunit *test)
{
	struct apple_dp_source_cookie cookie_a, cookie_b;
	struct apple_dp_source_state state;
	u64 generation;

	apple_dp_source_state_init(&state);
	KUNIT_ASSERT_EQ(test, apple_dp_source_begin_session(&state), 0);
	KUNIT_ASSERT_EQ(test, apple_dp_source_prepare(&state, &endpoint_a, &cookie_a), 0);
	generation = state.generation;
	KUNIT_EXPECT_EQ(test, apple_dp_source_prepare(&state, &endpoint_b, &cookie_b),
			-EBUSY);
	KUNIT_EXPECT_EQ(test, apple_dp_source_prepare(&state, &endpoint_a, &cookie_b),
			-EBUSY);
	KUNIT_EXPECT_EQ(test, state.generation, generation);
	KUNIT_EXPECT_TRUE(test, apple_dp_source_cookie_equal(&state.cookie, &cookie_a));
}

static void apple_dp_source_test_stale_release_is_ignored(struct kunit *test)
{
	struct apple_dp_source_cookie old_cookie, new_cookie;
	struct apple_dp_source_state state;

	apple_dp_source_state_init(&state);
	KUNIT_ASSERT_EQ(test, apple_dp_source_begin_session(&state), 0);
	KUNIT_ASSERT_EQ(test,
			apple_dp_source_prepare(&state, &endpoint_a, &old_cookie), 0);
	KUNIT_ASSERT_TRUE(test, apple_dp_source_unprepare(&state, &old_cookie));
	KUNIT_ASSERT_EQ(test,
			apple_dp_source_prepare(&state, &endpoint_a, &new_cookie), 0);
	KUNIT_ASSERT_EQ(test, apple_dp_source_enable(&state, &new_cookie), 0);
	KUNIT_EXPECT_FALSE(test, apple_dp_source_disable(&state, &old_cookie));
	KUNIT_EXPECT_FALSE(test, apple_dp_source_unprepare(&state, &old_cookie));
	KUNIT_EXPECT_EQ(test, state.phase, APPLE_DP_SOURCE_ENABLED);
	KUNIT_EXPECT_TRUE(test, apple_dp_source_cookie_equal(&state.cookie, &new_cookie));
}

static void apple_dp_source_test_coalesces_to_idle(struct kunit *test)
{
	struct apple_dp_source_cookie cookie;
	struct apple_dp_source_state state;

	apple_dp_source_state_init(&state);
	KUNIT_ASSERT_EQ(test, apple_dp_source_begin_session(&state), 0);
	KUNIT_EXPECT_EQ(test, apple_dp_source_begin_session(&state), -EBUSY);
	KUNIT_ASSERT_EQ(test, apple_dp_source_prepare(&state, &endpoint_a, &cookie), 0);
	KUNIT_ASSERT_TRUE(test, apple_dp_source_unprepare(&state, &cookie));
	KUNIT_EXPECT_EQ(test, state.phase, APPLE_DP_SOURCE_IDLE);
	KUNIT_EXPECT_EQ(test, state.generation, 2ULL);
}

static void apple_dp_source_test_invalid_phase_and_direct_release(struct kunit *test)
{
	struct apple_dp_source_cookie cookie = {};
	struct apple_dp_source_state state;

	apple_dp_source_state_init(&state);
	KUNIT_ASSERT_EQ(test, apple_dp_source_begin_session(&state), 0);
	KUNIT_EXPECT_EQ(test, apple_dp_source_enable(&state, &cookie), -EINVAL);
	KUNIT_ASSERT_EQ(test, apple_dp_source_prepare(&state, &endpoint_a, &cookie), 0);
	KUNIT_ASSERT_EQ(test, apple_dp_source_enable(&state, &cookie), 0);
	KUNIT_EXPECT_TRUE(test, apple_dp_source_unprepare(&state, &cookie));
	KUNIT_EXPECT_EQ(test, state.phase, APPLE_DP_SOURCE_IDLE);
}

static void apple_dp_source_test_quiesce_invalidates_cookie(struct kunit *test)
{
	struct apple_dp_source_cookie cookie, ignored;
	struct apple_dp_source_state state;

	apple_dp_source_state_init(&state);
	KUNIT_ASSERT_EQ(test, apple_dp_source_begin_session(&state), 0);
	KUNIT_ASSERT_EQ(test, apple_dp_source_prepare(&state, &endpoint_a, &cookie), 0);
	KUNIT_EXPECT_TRUE(test, apple_dp_source_quiesce(&state));
	KUNIT_EXPECT_EQ(test, state.phase, APPLE_DP_SOURCE_IDLE);
	KUNIT_EXPECT_EQ(test, state.endpoint.in_port, 0);
	KUNIT_EXPECT_EQ(test, state.endpoint.out_port, 0);
	KUNIT_EXPECT_EQ(test, apple_dp_source_enable(&state, &cookie), -ESTALE);
	KUNIT_EXPECT_EQ(test, apple_dp_source_prepare(&state, &endpoint_a, &ignored),
			-ESHUTDOWN);
}

static void apple_dp_source_test_session_epoch(struct kunit *test)
{
	struct apple_dp_source_cookie old_cookie, new_cookie;
	struct apple_dp_source_state state;

	apple_dp_source_state_init(&state);
	KUNIT_ASSERT_EQ(test, apple_dp_source_begin_session(&state), 0);
	KUNIT_ASSERT_EQ(test,
			apple_dp_source_prepare(&state, &endpoint_a, &old_cookie), 0);
	apple_dp_source_quiesce(&state);
	KUNIT_ASSERT_EQ(test, apple_dp_source_begin_session(&state), 0);
	KUNIT_ASSERT_EQ(test,
			apple_dp_source_prepare(&state, &endpoint_a, &new_cookie), 0);
	KUNIT_EXPECT_NE(test, old_cookie.session, new_cookie.session);
	KUNIT_EXPECT_FALSE(test, apple_dp_source_unprepare(&state, &old_cookie));
}

static void apple_dp_source_test_generation_wrap(struct kunit *test)
{
	struct apple_dp_source_cookie cookie;
	struct apple_dp_source_state state;

	apple_dp_source_state_init(&state);
	KUNIT_ASSERT_EQ(test, apple_dp_source_begin_session(&state), 0);
	state.generation = ~0ULL;
	KUNIT_ASSERT_EQ(test, apple_dp_source_prepare(&state, &endpoint_a, &cookie), 0);
	KUNIT_EXPECT_EQ(test, state.generation, 0ULL);
}

static void apple_dp_source_test_identity_wrap(struct kunit *test)
{
	struct apple_dp_source_cookie cookie;
	struct apple_dp_source_state state;

	apple_dp_source_state_init(&state);
	state.session = ~0ULL;
	state.next_token = ~0ULL;
	KUNIT_ASSERT_EQ(test, apple_dp_source_begin_session(&state), 0);
	KUNIT_EXPECT_EQ(test, state.session, 1ULL);
	KUNIT_ASSERT_EQ(test, apple_dp_source_prepare(&state, &endpoint_a, &cookie), 0);
	KUNIT_EXPECT_EQ(test, cookie.token, ~0ULL);
	KUNIT_ASSERT_TRUE(test, apple_dp_source_unprepare(&state, &cookie));
	KUNIT_ASSERT_EQ(test, apple_dp_source_prepare(&state, &endpoint_a, &cookie), 0);
	KUNIT_EXPECT_EQ(test, cookie.token, 1ULL);
}

static struct kunit_case apple_dp_source_test_cases[] = {
	KUNIT_CASE(apple_dp_source_test_lifecycle),
	KUNIT_CASE(apple_dp_source_test_conflict_is_unchanged),
	KUNIT_CASE(apple_dp_source_test_stale_release_is_ignored),
	KUNIT_CASE(apple_dp_source_test_coalesces_to_idle),
	KUNIT_CASE(apple_dp_source_test_invalid_phase_and_direct_release),
	KUNIT_CASE(apple_dp_source_test_quiesce_invalidates_cookie),
	KUNIT_CASE(apple_dp_source_test_session_epoch),
	KUNIT_CASE(apple_dp_source_test_generation_wrap),
	KUNIT_CASE(apple_dp_source_test_identity_wrap),
	{}
};

static struct kunit_suite apple_dp_source_test_suite = {
	.name = "apple-dp-source",
	.test_cases = apple_dp_source_test_cases,
};

kunit_test_suite(apple_dp_source_test_suite);
