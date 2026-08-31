// SPDX-License-Identifier: GPL-2.0-only OR MIT

#include <kunit/test.h>

#include "dptx-attempt.h"

static void apple_dptx_attempt_success_test(struct kunit *test)
{
	struct apple_dptx_attempt_ticket ticket;
	struct apple_dptx_attempt attempt;

	apple_dptx_attempt_init(&attempt);
	KUNIT_ASSERT_EQ(test, apple_dptx_attempt_begin(&attempt, &ticket), 0);
	KUNIT_EXPECT_TRUE(test, apple_dptx_attempt_link_configured(&attempt));
	KUNIT_EXPECT_EQ(test,
			apple_dptx_attempt_finish_wait(&attempt, &ticket, true), 0);
	KUNIT_EXPECT_EQ(test, attempt.phase, APPLE_DPTX_ATTEMPT_CONNECTED);
	KUNIT_EXPECT_FALSE(test, apple_dptx_attempt_cancel(&attempt));
	KUNIT_EXPECT_EQ(test, attempt.phase, APPLE_DPTX_ATTEMPT_IDLE);
}

static void apple_dptx_attempt_timeout_test(struct kunit *test)
{
	struct apple_dptx_attempt_ticket ticket;
	struct apple_dptx_attempt attempt;

	apple_dptx_attempt_init(&attempt);
	KUNIT_ASSERT_EQ(test, apple_dptx_attempt_begin(&attempt, &ticket), 0);
	KUNIT_EXPECT_EQ(test,
			apple_dptx_attempt_finish_wait(&attempt, &ticket, false),
			-ETIMEDOUT);
	KUNIT_EXPECT_EQ(test, attempt.phase, APPLE_DPTX_ATTEMPT_CANCELING);
	KUNIT_EXPECT_TRUE(test, apple_dptx_attempt_reset(&attempt, &ticket));
	KUNIT_EXPECT_EQ(test, attempt.phase, APPLE_DPTX_ATTEMPT_IDLE);
}

static void apple_dptx_attempt_cancel_wakes_waiter_test(struct kunit *test)
{
	struct apple_dptx_attempt_ticket ticket;
	struct apple_dptx_attempt attempt;

	apple_dptx_attempt_init(&attempt);
	KUNIT_ASSERT_EQ(test, apple_dptx_attempt_begin(&attempt, &ticket), 0);
	KUNIT_EXPECT_TRUE(test, apple_dptx_attempt_cancel(&attempt));
	KUNIT_EXPECT_EQ(test,
			apple_dptx_attempt_finish_wait(&attempt, &ticket, true),
			-ECANCELED);
	KUNIT_EXPECT_TRUE(test, apple_dptx_attempt_reset(&attempt, &ticket));
}

static void apple_dptx_attempt_late_completion_test(struct kunit *test)
{
	struct apple_dptx_attempt_ticket ticket;
	struct apple_dptx_attempt attempt;

	apple_dptx_attempt_init(&attempt);
	KUNIT_ASSERT_EQ(test, apple_dptx_attempt_begin(&attempt, &ticket), 0);
	KUNIT_ASSERT_TRUE(test, apple_dptx_attempt_cancel(&attempt));
	KUNIT_EXPECT_FALSE(test, apple_dptx_attempt_link_configured(&attempt));
	KUNIT_EXPECT_EQ(test,
			apple_dptx_attempt_finish_wait(&attempt, &ticket, true),
			-ECANCELED);
}

static void apple_dptx_attempt_duplicate_and_busy_test(struct kunit *test)
{
	struct apple_dptx_attempt_ticket ticket, ignored;
	struct apple_dptx_attempt attempt;

	apple_dptx_attempt_init(&attempt);
	KUNIT_ASSERT_EQ(test, apple_dptx_attempt_begin(&attempt, &ticket), 0);
	KUNIT_EXPECT_EQ(test, apple_dptx_attempt_begin(&attempt, &ignored), -EBUSY);
	KUNIT_ASSERT_TRUE(test, apple_dptx_attempt_link_configured(&attempt));
	KUNIT_EXPECT_FALSE(test, apple_dptx_attempt_link_configured(&attempt));
	KUNIT_ASSERT_EQ(test,
			apple_dptx_attempt_finish_wait(&attempt, &ticket, true), 0);
	KUNIT_EXPECT_FALSE(test, apple_dptx_attempt_reset(&attempt, &ticket));
}

static void apple_dptx_attempt_stale_ticket_test(struct kunit *test)
{
	struct apple_dptx_attempt_ticket old_ticket, new_ticket;
	struct apple_dptx_attempt attempt;

	apple_dptx_attempt_init(&attempt);
	KUNIT_ASSERT_EQ(test, apple_dptx_attempt_begin(&attempt, &old_ticket), 0);
	KUNIT_ASSERT_TRUE(test, apple_dptx_attempt_cancel(&attempt));
	KUNIT_ASSERT_TRUE(test, apple_dptx_attempt_reset(&attempt, &old_ticket));
	KUNIT_ASSERT_EQ(test, apple_dptx_attempt_begin(&attempt, &new_ticket), 0);
	KUNIT_EXPECT_EQ(test,
			apple_dptx_attempt_finish_wait(&attempt, &old_ticket, true),
			-ESTALE);
	KUNIT_EXPECT_FALSE(test, apple_dptx_attempt_reset(&attempt, &old_ticket));
	KUNIT_EXPECT_NE(test, old_ticket.generation, new_ticket.generation);
}

static void apple_dptx_attempt_generation_wrap_test(struct kunit *test)
{
	struct apple_dptx_attempt_ticket ticket;
	struct apple_dptx_attempt attempt;

	apple_dptx_attempt_init(&attempt);
	attempt.generation = ~0ULL;
	KUNIT_ASSERT_EQ(test, apple_dptx_attempt_begin(&attempt, &ticket), 0);
	KUNIT_EXPECT_EQ(test, ticket.generation, 1ULL);
}

static void apple_dptx_attempt_completion_without_link_test(struct kunit *test)
{
	struct apple_dptx_attempt_ticket ticket;
	struct apple_dptx_attempt attempt;

	apple_dptx_attempt_init(&attempt);
	KUNIT_ASSERT_EQ(test, apple_dptx_attempt_begin(&attempt, &ticket), 0);
	KUNIT_EXPECT_EQ(test,
			apple_dptx_attempt_finish_wait(&attempt, &ticket, true),
			-EIO);
	KUNIT_EXPECT_EQ(test, attempt.phase, APPLE_DPTX_ATTEMPT_CANCELING);
}

static void apple_dptx_attempt_failure_quarantines_test(struct kunit *test)
{
	struct apple_dptx_attempt_ticket ticket, ignored;
	struct apple_dptx_attempt attempt;

	apple_dptx_attempt_init(&attempt);
	KUNIT_ASSERT_EQ(test, apple_dptx_attempt_begin(&attempt, &ticket), 0);
	KUNIT_ASSERT_TRUE(test, apple_dptx_attempt_fail(&attempt, &ticket));
	KUNIT_EXPECT_EQ(test, attempt.phase, APPLE_DPTX_ATTEMPT_FAILED);
	KUNIT_EXPECT_FALSE(test, apple_dptx_attempt_link_configured(&attempt));
	KUNIT_EXPECT_FALSE(test, apple_dptx_attempt_cancel(&attempt));
	KUNIT_EXPECT_FALSE(test, apple_dptx_attempt_reset(&attempt, &ticket));
	KUNIT_EXPECT_EQ(test, apple_dptx_attempt_begin(&attempt, &ignored), -EBUSY);
	KUNIT_EXPECT_EQ(test,
			apple_dptx_attempt_finish_wait(&attempt, &ticket, true),
			-EIO);
	KUNIT_EXPECT_EQ(test, attempt.phase, APPLE_DPTX_ATTEMPT_FAILED);
}

static void apple_dptx_attempt_forced_failure_test(struct kunit *test)
{
	struct apple_dptx_attempt_ticket ticket, ignored;
	struct apple_dptx_attempt attempt;

	apple_dptx_attempt_init(&attempt);
	KUNIT_ASSERT_EQ(test, apple_dptx_attempt_begin(&attempt, &ticket), 0);
	KUNIT_ASSERT_TRUE(test, apple_dptx_attempt_link_configured(&attempt));
	KUNIT_ASSERT_EQ(test,
			apple_dptx_attempt_finish_wait(&attempt, &ticket, true), 0);
	KUNIT_ASSERT_TRUE(test, apple_dptx_attempt_fail(&attempt, NULL));
	KUNIT_EXPECT_EQ(test, attempt.phase, APPLE_DPTX_ATTEMPT_FAILED);
	KUNIT_EXPECT_EQ(test, apple_dptx_attempt_begin(&attempt, &ignored), -EBUSY);
}

static struct kunit_case apple_dptx_attempt_test_cases[] = {
	KUNIT_CASE(apple_dptx_attempt_success_test),
	KUNIT_CASE(apple_dptx_attempt_timeout_test),
	KUNIT_CASE(apple_dptx_attempt_cancel_wakes_waiter_test),
	KUNIT_CASE(apple_dptx_attempt_late_completion_test),
	KUNIT_CASE(apple_dptx_attempt_duplicate_and_busy_test),
	KUNIT_CASE(apple_dptx_attempt_stale_ticket_test),
	KUNIT_CASE(apple_dptx_attempt_generation_wrap_test),
	KUNIT_CASE(apple_dptx_attempt_completion_without_link_test),
	KUNIT_CASE(apple_dptx_attempt_failure_quarantines_test),
	KUNIT_CASE(apple_dptx_attempt_forced_failure_test),
	{}
};

static struct kunit_suite apple_dptx_attempt_test_suite = {
	.name = "apple-dptx-attempt",
	.test_cases = apple_dptx_attempt_test_cases,
};

kunit_test_suite(apple_dptx_attempt_test_suite);
