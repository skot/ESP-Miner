#include "job_pipeline_test_harness.h"
#include "asic_job_slot.h"
#include "asic_common.h"
#include "bm_job_packet.h"
#include "bzm/reactor.h"
#include "bzm/protocol.h"
#include "miner_job.h"
#include "unity.h"
#include "utils.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    bool bonanza;
    bzm_job_store_t bzm_store;
    bzm_reactor_t reactor;
    asic_job_store_t bitmain_store;
    asic_job_slot_entry_t entries[8];
    bzm_raw_result_t responses[JOB_PIPELINE_HARNESS_MAX_JOBS];
    uint16_t slots[JOB_PIPELINE_HARNESS_MAX_JOBS];
    size_t sent;
} invalidation_fixture_t;

static bool accept_work(void *context, const bzm_work_t *work)
{
    (void)context;
    (void)work;
    return true;
}

static void clear_work(void *context)
{
    invalidation_fixture_t *fixture = context;
    if (fixture->bonanza) {
        TEST_ASSERT_TRUE(bzm_reactor_invalidate_work(&fixture->reactor));
    } else {
        bm13xx_clear_work(&fixture->bitmain_store);
    }
}

static void send_work(void *context, const asic_job_t *job)
{
    invalidation_fixture_t *fixture = context;
    if (fixture->bonanza) {
        bzm_work_t work;
        TEST_ASSERT_EQUAL(BZM_ASSIGN_OK, bzm_reactor_assign(&fixture->reactor, job, &work));
        fixture->responses[fixture->sent] = (bzm_raw_result_t){
            .asic_id = BZM_FIRST_ASIC_ID, .engine_id = work.engine_id,
            .sequence_id = work.logical_sequence << 2, .time = 16, .status = 8,
        };
    } else {
        fixture->slots[fixture->sent] = asic_job_store_claim(&fixture->bitmain_store, job);
    }
    fixture->sent++;
}

static bool result_valid(invalidation_fixture_t *fixture, size_t index, asic_job_t *job)
{
    if (fixture->bonanza) {
        bzm_result_t result;
        bool valid = bzm_reactor_map_result(&fixture->reactor, &fixture->responses[index], &result);
        if (valid && job) *job = result.job;
        return valid;
    }
    return asic_job_store_lookup(&fixture->bitmain_store, fixture->slots[index], job);
}

static invalidation_fixture_t *fixture_init(bool bonanza)
{
    invalidation_fixture_t *fixture = calloc(1, sizeof(*fixture));
    TEST_ASSERT_NOT_NULL(fixture);
    fixture->bonanza = bonanza;
    if (bonanza) {
        TEST_ASSERT_TRUE(bzm_job_store_init(&fixture->bzm_store));
        bzm_reactor_config_t config = {.engine_count = 2, .timestamp_count = 16, .lead_zeros = 36};
        bzm_transport_ops_t ops = {.write_work = accept_work};
        TEST_ASSERT_TRUE(bzm_reactor_init(&fixture->reactor, &fixture->bzm_store, &config, &ops, NULL));
    } else {
        asic_job_store_init(&fixture->bitmain_store, fixture->entries, 8);
    }
    miner_job_pool_init();
    for (size_t slot = 0; slot < 3; ++slot) {
        miner_job_t *job = miner_job_get_slot(slot);
        job->type = JOB_TYPE_SV2_STANDARD;
        job->version = 0x20000004;
        job->version_mask = 0x00006000;
        job->ntime = 0x65010203;
        job->nbits = 0x1705dd01;
        job->coinbase_prefix_len = 0;
        job->extranonce2_len = 0;
        job->pool_id = slot ? 1 : 0;
        job->clean_jobs = slot < 2;
        strcpy(job->job_id, slot == 0 ? "old" : slot == 1 ? "clean" : "following");
        memset(job->prev_hash, 1, sizeof(job->prev_hash));
        memset(job->merkle_root, 2, sizeof(job->merkle_root));
        diff_to_target(slot ? 4096 : 1000, job->pool_target);
    }
    return fixture;
}

static job_pipeline_harness_config_t fixture_config(invalidation_fixture_t *fixture)
{
    return (job_pipeline_harness_config_t){
        .asic_initialized = true, .job_frequency_ms = 1, .software_midstates = 1,
        .on_send = send_work, .on_clear = clear_work, .context = fixture,
    };
}

static void fixture_destroy(invalidation_fixture_t *fixture)
{
    if (fixture->bonanza) bzm_job_store_destroy(&fixture->bzm_store);
    else asic_job_store_destroy(&fixture->bitmain_store);
    free(fixture);
}

TEST_CASE("clean notifications retire old BZM and Bitmain results before replacement",
          "[asic][job-task][invalidation]")
{
    for (unsigned backend = 0; backend < 2; ++backend) {
        invalidation_fixture_t *fixture = fixture_init(backend == 0);
        const job_pipeline_harness_event_t events[] = {
            {.type = JOB_PIPELINE_HARNESS_NOTIFY, .slot = 0},
            {.type = JOB_PIPELINE_HARNESS_NOTIFY, .slot = 1},
        };
        job_pipeline_harness_result_t result;
        job_pipeline_harness_run(fixture_config(fixture), events, 2, &result);
        TEST_ASSERT_EQUAL_UINT32(2, result.job_count);
        TEST_ASSERT_FALSE(result_valid(fixture, 0, NULL));
        asic_job_t job;
        TEST_ASSERT_TRUE(result_valid(fixture, 1, &job));
        TEST_ASSERT_EQUAL_STRING("clean", job.job_id);
        TEST_ASSERT_FLOAT_WITHIN(0.01, 4096, target_to_diff(job.pool_target));
        fixture_destroy(fixture);
    }
}

TEST_CASE("non-clean notifications preserve prior BZM and Bitmain work and targets",
          "[asic][job-task][invalidation]")
{
    for (unsigned backend = 0; backend < 2; ++backend) {
        invalidation_fixture_t *fixture = fixture_init(backend == 0);
        const job_pipeline_harness_event_t events[] = {
            {.type = JOB_PIPELINE_HARNESS_NOTIFY, .slot = 0},
            {.type = JOB_PIPELINE_HARNESS_NOTIFY, .slot = 2},
            {.type = JOB_PIPELINE_HARNESS_TIMEOUT},
        };
        job_pipeline_harness_result_t result;
        job_pipeline_harness_run(fixture_config(fixture), events, 3, &result);
        TEST_ASSERT_EQUAL_UINT32(2, result.job_count);
        TEST_ASSERT_EQUAL_UINT32(result.clears_before_send[0], result.clears_before_send[1]);
        asic_job_t job;
        TEST_ASSERT_TRUE(result_valid(fixture, 0, &job));
        TEST_ASSERT_EQUAL_STRING("old", job.job_id);
        TEST_ASSERT_FLOAT_WITHIN(0.01, 1000, target_to_diff(job.pool_target));
        TEST_ASSERT_TRUE(result_valid(fixture, 1, NULL));
        fixture_destroy(fixture);
    }
}

TEST_CASE("coalesced clean and non-clean notifications still retire old work promptly",
          "[asic][job-task][invalidation]")
{
    for (unsigned backend = 0; backend < 2; ++backend) {
        invalidation_fixture_t *fixture = fixture_init(backend == 0);
        const job_pipeline_harness_event_t events[] = {
            {.type = JOB_PIPELINE_HARNESS_NOTIFY, .slot = 0},
            {.type = JOB_PIPELINE_HARNESS_NOTIFY, .slot = 1, .coalesce_next = true},
            {.type = JOB_PIPELINE_HARNESS_NOTIFY, .slot = 2},
        };
        job_pipeline_harness_result_t result;
        job_pipeline_harness_run(fixture_config(fixture), events, 3, &result);
        TEST_ASSERT_EQUAL_UINT32(2, result.job_count);
        TEST_ASSERT_EQUAL_STRING("following", result.jobs[1]->job_id);
        TEST_ASSERT_FALSE(result_valid(fixture, 0, NULL));
        TEST_ASSERT_TRUE(result_valid(fixture, 1, NULL));
        fixture_destroy(fixture);
    }
}

TEST_CASE("pool reset stops timer redispatch and permits a coalesced fresh job",
          "[asic][job-task][invalidation]")
{
    for (unsigned backend = 0; backend < 2; ++backend) {
        for (unsigned fresh = 0; fresh < 2; ++fresh) {
            invalidation_fixture_t *fixture = fixture_init(backend == 0);
            const job_pipeline_harness_event_t events[] = {
                {.type = JOB_PIPELINE_HARNESS_NOTIFY, .slot = 0},
                {.type = JOB_PIPELINE_HARNESS_RESET, .coalesce_next = fresh},
                {.type = fresh ? JOB_PIPELINE_HARNESS_NOTIFY : JOB_PIPELINE_HARNESS_TIMEOUT, .slot = 2},
            };
            job_pipeline_harness_result_t result;
            job_pipeline_harness_run(fixture_config(fixture), events, 3, &result);
            TEST_ASSERT_EQUAL_UINT32(fresh ? 2 : 1, result.job_count);
            TEST_ASSERT_FALSE(result_valid(fixture, 0, NULL));
            if (fresh) {
                TEST_ASSERT_TRUE(result_valid(fixture, 1, NULL));
                TEST_ASSERT_EQUAL_UINT8(1, result.jobs[1]->pool_id);
            }
            fixture_destroy(fixture);
        }
    }
}

TEST_CASE("task boundary retires an old send crossing reset or coalesced clean receipt",
          "[asic][job-task][invalidation]")
{
    for (unsigned backend = 0; backend < 2; ++backend) {
        for (unsigned clean = 0; clean < 2; ++clean) {
            invalidation_fixture_t *fixture = fixture_init(backend == 0);
            job_pipeline_harness_config_t config = fixture_config(fixture);
            config.reset_during_send = clean ? 0 : 1;
            config.notify_during_send = clean ? 1 : 0;
            config.clean_slot = 1;
            config.following_slot = 2;
            const job_pipeline_harness_event_t events[] = {
                {.type = JOB_PIPELINE_HARNESS_NOTIFY, .slot = 0},
            };
            job_pipeline_harness_result_t result;
            job_pipeline_harness_run(config, events, 1, &result);
            TEST_ASSERT_EQUAL_UINT32(clean ? 2 : 1, result.job_count);
            TEST_ASSERT_FALSE(result_valid(fixture, 0, NULL));
            if (clean) TEST_ASSERT_TRUE(result_valid(fixture, 1, NULL));
            fixture_destroy(fixture);
        }
    }
}

TEST_CASE("invalidation consumes the latest notification after an older wait returned",
          "[asic][job-task][invalidation]")
{
    for (unsigned backend = 0; backend < 2; ++backend) {
        invalidation_fixture_t *fixture = fixture_init(backend == 0);
        job_pipeline_harness_config_t config = fixture_config(fixture);
        config.notify_after_wait = true;
        config.clean_slot = 1;
        config.following_slot = 2;
        const job_pipeline_harness_event_t events[] = {
            {.type = JOB_PIPELINE_HARNESS_NOTIFY, .slot = 0},
        };
        job_pipeline_harness_result_t result;
        job_pipeline_harness_run(config, events, 1, &result);
        TEST_ASSERT_EQUAL_UINT32(1, result.job_count);
        TEST_ASSERT_EQUAL_STRING("following", result.jobs[0]->job_id);
        TEST_ASSERT_TRUE(result_valid(fixture, 0, NULL));
        fixture_destroy(fixture);
    }
}
