/* test_async_queue.c - Unit tests for async event queue
 *
 * Tests queue operations, thread safety, and event handling.
 */

#include "test_framework.h"
#include "loki/async_queue.h"
#include <string.h>
#include <pthread.h>

/* Test: Queue initialization */
TEST(queue_init) {
    /* Cleanup any previous state */
    async_queue_cleanup();

    ASSERT_EQ(async_queue_init(), 0);
    ASSERT_NOT_NULL(async_queue_global());

    /* Second init should be no-op */
    ASSERT_EQ(async_queue_init(), 0);

    async_queue_cleanup();
    ASSERT_NULL(async_queue_global());
}

/* Test: Basic push and poll */
TEST(queue_push_poll) {
    async_queue_cleanup();
    ASSERT_EQ(async_queue_init(), 0);

    AsyncEventQueue *queue = async_queue_global();
    ASSERT_NOT_NULL(queue);

    /* Queue should be empty */
    ASSERT_TRUE(async_queue_is_empty(queue));
    ASSERT_EQ(async_queue_count(queue), 0);

    /* Push a language callback event */
    ASSERT_EQ(async_queue_push_lang_callback(queue, 42, 0), 0);

    /* Queue should have one event */
    ASSERT_FALSE(async_queue_is_empty(queue));
    ASSERT_EQ(async_queue_count(queue), 1);

    /* Poll the event */
    AsyncEvent event;
    ASSERT_EQ(async_queue_poll(queue, &event), 0);
    ASSERT_EQ(event.type, ASYNC_EVENT_LANG_CALLBACK);
    ASSERT_EQ(event.data.lang.slot_id, 42);
    ASSERT_EQ(event.data.lang.status, 0);

    /* Queue should be empty again */
    ASSERT_TRUE(async_queue_is_empty(queue));
    ASSERT_EQ(async_queue_count(queue), 0);

    async_queue_cleanup();
}

/* Test: Multiple event types */
TEST(queue_event_types) {
    async_queue_cleanup();
    ASSERT_EQ(async_queue_init(), 0);

    AsyncEventQueue *queue = async_queue_global();

    /* Push various event types */
    ASSERT_EQ(async_queue_push_link_peers(queue, 5), 0);
    ASSERT_EQ(async_queue_push_link_tempo(queue, 120.5), 0);
    ASSERT_EQ(async_queue_push_link_transport(queue, 1), 0);
    ASSERT_EQ(async_queue_push_beat(queue, 4.0, 4.0, 1), 0);
    ASSERT_EQ(async_queue_push_timer(queue, 100, NULL), 0);

    ASSERT_EQ(async_queue_count(queue), 5);

    /* Poll and verify each event */
    AsyncEvent event;

    ASSERT_EQ(async_queue_poll(queue, &event), 0);
    ASSERT_EQ(event.type, ASYNC_EVENT_LINK_PEERS);
    ASSERT_EQ(event.data.link_peers.peers, 5);

    ASSERT_EQ(async_queue_poll(queue, &event), 0);
    ASSERT_EQ(event.type, ASYNC_EVENT_LINK_TEMPO);
    ASSERT_TRUE(event.data.link_tempo.tempo > 120.4 && event.data.link_tempo.tempo < 120.6);

    ASSERT_EQ(async_queue_poll(queue, &event), 0);
    ASSERT_EQ(event.type, ASYNC_EVENT_LINK_TRANSPORT);
    ASSERT_EQ(event.data.link_transport.playing, 1);

    ASSERT_EQ(async_queue_poll(queue, &event), 0);
    ASSERT_EQ(event.type, ASYNC_EVENT_BEAT_BOUNDARY);
    ASSERT_EQ(event.data.beat.buffer_id, 1);

    ASSERT_EQ(async_queue_poll(queue, &event), 0);
    ASSERT_EQ(event.type, ASYNC_EVENT_TIMER);
    ASSERT_EQ(event.data.timer.timer_id, 100);

    ASSERT_TRUE(async_queue_is_empty(queue));

    async_queue_cleanup();
}

/* Test: Custom events with heap data */
TEST(queue_custom_events) {
    async_queue_cleanup();
    ASSERT_EQ(async_queue_init(), 0);

    AsyncEventQueue *queue = async_queue_global();

    /* Push custom event with data */
    const char *data = "hello world";
    ASSERT_EQ(async_queue_push_custom(queue, "test_tag", data, strlen(data) + 1), 0);

    AsyncEvent event;
    ASSERT_EQ(async_queue_poll(queue, &event), 0);
    ASSERT_EQ(event.type, ASYNC_EVENT_CUSTOM);
    ASSERT_STR_EQ(event.data.custom.tag, "test_tag");
    ASSERT_EQ(event.data.custom.len, strlen(data) + 1);
    ASSERT_NOT_NULL(event.data.custom.data);
    ASSERT_STR_EQ((const char *)event.data.custom.data, data);

    /* Clean up heap data */
    async_event_cleanup(&event);

    async_queue_cleanup();
}

/* Test: Peek without consuming */
TEST(queue_peek) {
    async_queue_cleanup();
    ASSERT_EQ(async_queue_init(), 0);

    AsyncEventQueue *queue = async_queue_global();

    ASSERT_EQ(async_queue_push_link_tempo(queue, 140.0), 0);

    AsyncEvent event;

    /* Peek should not consume */
    ASSERT_EQ(async_queue_peek(queue, &event), 0);
    ASSERT_EQ(event.type, ASYNC_EVENT_LINK_TEMPO);
    ASSERT_EQ(async_queue_count(queue), 1);

    /* Peek again - same event */
    ASSERT_EQ(async_queue_peek(queue, &event), 0);
    ASSERT_EQ(event.type, ASYNC_EVENT_LINK_TEMPO);
    ASSERT_EQ(async_queue_count(queue), 1);

    /* Pop to consume */
    async_queue_pop(queue);
    ASSERT_TRUE(async_queue_is_empty(queue));

    async_queue_cleanup();
}

/* Test: Queue full behavior */
TEST(queue_full) {
    async_queue_cleanup();
    ASSERT_EQ(async_queue_init(), 0);

    AsyncEventQueue *queue = async_queue_global();

    /* Fill the queue */
    for (int i = 0; i < ASYNC_QUEUE_SIZE - 1; i++) {
        ASSERT_EQ(async_queue_push_timer(queue, i, NULL), 0);
    }

    ASSERT_EQ(async_queue_count(queue), ASYNC_QUEUE_SIZE - 1);

    /* Queue should be full - next push should fail */
    ASSERT_EQ(async_queue_push_timer(queue, 999, NULL), -1);

    /* Drain the queue */
    AsyncEvent event;
    int count = 0;
    while (async_queue_poll(queue, &event) == 0) {
        count++;
    }
    ASSERT_EQ(count, ASYNC_QUEUE_SIZE - 1);

    async_queue_cleanup();
}

/* Test: Empty queue poll returns error */
TEST(queue_empty_poll) {
    async_queue_cleanup();
    ASSERT_EQ(async_queue_init(), 0);

    AsyncEventQueue *queue = async_queue_global();

    AsyncEvent event;
    ASSERT_EQ(async_queue_poll(queue, &event), 1);  /* 1 = empty */
    ASSERT_EQ(async_queue_peek(queue, &event), 1);

    async_queue_cleanup();
}

/* Test: NULL queue uses global */
TEST(queue_null_uses_global) {
    async_queue_cleanup();
    ASSERT_EQ(async_queue_init(), 0);

    /* Push with NULL queue should use global */
    ASSERT_EQ(async_queue_push_link_peers(NULL, 3), 0);

    /* Count with NULL queue should use global */
    ASSERT_EQ(async_queue_count(NULL), 1);

    /* Poll with NULL queue should use global */
    AsyncEvent event;
    ASSERT_EQ(async_queue_poll(NULL, &event), 0);
    ASSERT_EQ(event.type, ASYNC_EVENT_LINK_PEERS);

    async_queue_cleanup();
}

/* Test: Event type names */
TEST(event_type_names) {
    ASSERT_STR_EQ(async_event_type_name(ASYNC_EVENT_NONE), "NONE");
    ASSERT_STR_EQ(async_event_type_name(ASYNC_EVENT_LANG_CALLBACK), "LANG_CALLBACK");
    ASSERT_STR_EQ(async_event_type_name(ASYNC_EVENT_LINK_PEERS), "LINK_PEERS");
    ASSERT_STR_EQ(async_event_type_name(ASYNC_EVENT_LINK_TEMPO), "LINK_TEMPO");
    ASSERT_STR_EQ(async_event_type_name(ASYNC_EVENT_LINK_TRANSPORT), "LINK_TRANSPORT");
    ASSERT_STR_EQ(async_event_type_name(ASYNC_EVENT_TIMER), "TIMER");
    ASSERT_STR_EQ(async_event_type_name(ASYNC_EVENT_BEAT_BOUNDARY), "BEAT_BOUNDARY");
    ASSERT_STR_EQ(async_event_type_name(ASYNC_EVENT_CUSTOM), "CUSTOM");
}

/* Test: Handler registration */
static int g_handler_called = 0;
static AsyncEventType g_handler_event_type = ASYNC_EVENT_NONE;

static void test_handler(AsyncEvent *event, void *ctx) {
    (void)ctx;
    g_handler_called = 1;
    g_handler_event_type = event->type;
}

TEST(handler_registration) {
    async_queue_cleanup();
    ASSERT_EQ(async_queue_init(), 0);

    AsyncEventQueue *queue = async_queue_global();

    /* Register custom handler for timer events */
    async_queue_set_handler(queue, ASYNC_EVENT_TIMER, test_handler);
    ASSERT_EQ((void *)async_queue_get_handler(queue, ASYNC_EVENT_TIMER), (void *)test_handler);

    /* Push and dispatch */
    g_handler_called = 0;
    g_handler_event_type = ASYNC_EVENT_NONE;

    ASSERT_EQ(async_queue_push_timer(queue, 1, NULL), 0);
    async_queue_dispatch_all(queue, NULL);

    ASSERT_EQ(g_handler_called, 1);
    ASSERT_EQ(g_handler_event_type, ASYNC_EVENT_TIMER);

    /* Unregister handler */
    async_queue_set_handler(queue, ASYNC_EVENT_TIMER, NULL);
    ASSERT_NULL(async_queue_get_handler(queue, ASYNC_EVENT_TIMER));

    async_queue_cleanup();
}

/* Test: FIFO ordering */
TEST(queue_fifo_order) {
    async_queue_cleanup();
    ASSERT_EQ(async_queue_init(), 0);

    AsyncEventQueue *queue = async_queue_global();

    /* Push events in order */
    for (int i = 0; i < 10; i++) {
        ASSERT_EQ(async_queue_push_timer(queue, i, NULL), 0);
    }

    /* Poll should return in same order */
    AsyncEvent event;
    for (int i = 0; i < 10; i++) {
        ASSERT_EQ(async_queue_poll(queue, &event), 0);
        ASSERT_EQ(event.data.timer.timer_id, i);
    }

    async_queue_cleanup();
}

/* Test: Timestamp is set */
TEST(event_timestamp) {
    async_queue_cleanup();
    ASSERT_EQ(async_queue_init(), 0);

    AsyncEventQueue *queue = async_queue_global();

    ASSERT_EQ(async_queue_push_timer(queue, 1, NULL), 0);

    AsyncEvent event;
    ASSERT_EQ(async_queue_poll(queue, &event), 0);
    ASSERT_TRUE(event.timestamp > 0);

    async_queue_cleanup();
}

/* Thread function for concurrent push test */
static void *thread_push_func(void *arg) {
    int thread_id = *(int *)arg;
    AsyncEventQueue *queue = async_queue_global();

    for (int i = 0; i < 50; i++) {
        async_queue_push_timer(queue, thread_id * 100 + i, NULL);
    }

    return NULL;
}

/* Test: Concurrent pushes from multiple threads */
TEST(queue_concurrent_push) {
    async_queue_cleanup();
    ASSERT_EQ(async_queue_init(), 0);

    pthread_t threads[4];
    int thread_ids[4] = {0, 1, 2, 3};

    /* Start multiple threads pushing events */
    for (int i = 0; i < 4; i++) {
        pthread_create(&threads[i], NULL, thread_push_func, &thread_ids[i]);
    }

    /* Wait for threads to complete */
    for (int i = 0; i < 4; i++) {
        pthread_join(threads[i], NULL);
    }

    /* Verify all events were pushed (4 threads * 50 events = 200) */
    ASSERT_EQ(async_queue_count(NULL), 200);

    /* Drain the queue */
    AsyncEvent event;
    int count = 0;
    while (async_queue_poll(NULL, &event) == 0) {
        count++;
    }
    ASSERT_EQ(count, 200);

    async_queue_cleanup();
}

/* Test suite */
BEGIN_TEST_SUITE("Async Event Queue")
    RUN_TEST(queue_init);
    RUN_TEST(queue_push_poll);
    RUN_TEST(queue_event_types);
    RUN_TEST(queue_custom_events);
    RUN_TEST(queue_peek);
    RUN_TEST(queue_full);
    RUN_TEST(queue_empty_poll);
    RUN_TEST(queue_null_uses_global);
    RUN_TEST(event_type_names);
    RUN_TEST(handler_registration);
    RUN_TEST(queue_fifo_order);
    RUN_TEST(event_timestamp);
    RUN_TEST(queue_concurrent_push);
END_TEST_SUITE()
