#include "scheduler.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "builtins.h"

typedef struct {
    char* key;
    size_t value;
} CycleEntry;

typedef struct {
    char* key;
    double value;
    bool has_value;
} TriggerEntry;

struct BogStateManager {
    CycleEntry* cycles;
    size_t cycle_count;
    size_t cycle_capacity;
    TriggerEntry* triggers;
    size_t trigger_count;
    size_t trigger_capacity;
};

typedef struct {
    BogBeatCallback cb;
    void* userdata;
    int handle;
    bool active;
} BeatCallbackEntry;

struct BogScheduler {
    BogAudioCallbacks audio;
    const BogBuiltins* builtins;
    BogStateManager* state_manager;
    const BogProgram* program;
    double bpm;
    double swing;
    double lookahead_ms;
    double grid_beats;
    bool running;
    int current_beat;
    BeatCallbackEntry* callbacks;
    size_t callback_count;
    size_t callback_capacity;
    int next_callback_handle;
};

struct BogTransitionManager {
    BogScheduler* scheduler;
    double quantization;
    const BogProgram* pending_program;
    double pending_boundary;
    bool has_pending;
};

static char* dup_string(const char* src)
{
    if (!src)
        return NULL;
    size_t len = strlen(src) + 1;
    char* copy = (char*)malloc(len);
    if (copy)
        memcpy(copy, src, len);
    return copy;
}

static CycleEntry* find_cycle_entry(BogStateManager* mgr, const char* key)
{
    for (size_t i = 0; i < mgr->cycle_count; ++i) {
        if (strcmp(mgr->cycles[i].key, key) == 0)
            return &mgr->cycles[i];
    }
    return NULL;
}

static TriggerEntry* find_trigger_entry(BogStateManager* mgr,
                                        const char* key)
{
    for (size_t i = 0; i < mgr->trigger_count; ++i) {
        if (strcmp(mgr->triggers[i].key, key) == 0)
            return &mgr->triggers[i];
    }
    return NULL;
}

static void free_cycles(BogStateManager* mgr)
{
    for (size_t i = 0; i < mgr->cycle_count; ++i) {
        free(mgr->cycles[i].key);
    }
    free(mgr->cycles);
    mgr->cycles = NULL;
    mgr->cycle_count = 0;
    mgr->cycle_capacity = 0;
}

static void free_triggers(BogStateManager* mgr)
{
    for (size_t i = 0; i < mgr->trigger_count; ++i) {
        free(mgr->triggers[i].key);
    }
    free(mgr->triggers);
    mgr->triggers = NULL;
    mgr->trigger_count = 0;
    mgr->trigger_capacity = 0;
}

BogStateManager* bog_state_manager_create(void)
{
    BogStateManager* mgr = (BogStateManager*)calloc(
        1, sizeof(BogStateManager));
    return mgr;
}

void bog_state_manager_destroy(BogStateManager* manager)
{
    if (!manager)
        return;
    free_cycles(manager);
    free_triggers(manager);
    free(manager);
}

void bog_state_manager_reset(BogStateManager* manager)
{
    if (!manager)
        return;
    for (size_t i = 0; i < manager->cycle_count; ++i) {
        free(manager->cycles[i].key);
        manager->cycles[i].key = NULL;
        manager->cycles[i].value = 0;
    }
    manager->cycle_count = 0;
    for (size_t i = 0; i < manager->trigger_count; ++i) {
        free(manager->triggers[i].key);
        manager->triggers[i].key = NULL;
        manager->triggers[i].has_value = false;
        manager->triggers[i].value = 0.0;
    }
    manager->trigger_count = 0;
}

size_t bog_state_manager_get_cycle(const BogStateManager* manager,
                                       const char* key)
{
    if (!manager || !key)
        return 0;
    for (size_t i = 0; i < manager->cycle_count; ++i) {
        if (strcmp(manager->cycles[i].key, key) == 0)
            return manager->cycles[i].value;
    }
    return 0;
}

size_t bog_state_manager_increment_cycle(BogStateManager* manager,
                                             const char* key,
                                             size_t list_length)
{
    if (!manager || !key || list_length == 0)
        return 0;
    CycleEntry* entry = find_cycle_entry(manager, key);
    if (!entry) {
        if (manager->cycle_count == manager->cycle_capacity) {
            size_t new_cap = manager->cycle_capacity
                ? manager->cycle_capacity * 2
                : 8;
            CycleEntry* new_items = (CycleEntry*)realloc(
                manager->cycles, new_cap * sizeof(CycleEntry));
            if (!new_items)
                return 0;
            manager->cycles = new_items;
            manager->cycle_capacity = new_cap;
        }
        manager->cycles[manager->cycle_count].key = dup_string(key);
        manager->cycles[manager->cycle_count].value = 0;
        entry = &manager->cycles[manager->cycle_count++];
    }
    size_t current = entry->value;
    entry->value = (entry->value + 1) % list_length;
    return current;
}

double
bog_state_manager_get_last_trigger(const BogStateManager* manager,
                                       const char* key, bool* found)
{
    if (found)
        *found = false;
    if (!manager || !key)
        return 0.0;
    for (size_t i = 0; i < manager->trigger_count; ++i) {
        if (strcmp(manager->triggers[i].key, key) == 0
            && manager->triggers[i].has_value) {
            if (found)
                *found = true;
            return manager->triggers[i].value;
        }
    }
    return 0.0;
}

void bog_state_manager_set_last_trigger(BogStateManager* manager,
                                            const char* key, double time_value)
{
    if (!manager || !key)
        return;
    TriggerEntry* entry = find_trigger_entry(manager, key);
    if (!entry) {
        if (manager->trigger_count == manager->trigger_capacity) {
            size_t new_cap = manager->trigger_capacity
                ? manager->trigger_capacity * 2
                : 8;
            TriggerEntry* new_items = (TriggerEntry*)realloc(
                manager->triggers, new_cap * sizeof(TriggerEntry));
            if (!new_items)
                return;
            manager->triggers = new_items;
            manager->trigger_capacity = new_cap;
        }
        manager->triggers[manager->trigger_count].key = dup_string(key);
        manager->triggers[manager->trigger_count].value = 0.0;
        manager->triggers[manager->trigger_count].has_value = false;
        entry = &manager->triggers[manager->trigger_count++];
    }
    entry->value = time_value;
    entry->has_value = true;
}

bool bog_state_manager_can_trigger(const BogStateManager* manager,
                                       const char* key, double now, double gap)
{
    bool found = false;
    double last = bog_state_manager_get_last_trigger(manager, key, &found);
    if (!found)
        return true;
    return (now - last) >= gap;
}

/* Scheduler */

static double swing_adjust(double t, double bpm, double swing_amt)
{
    double eighth = (60.0 / bpm) / 2.0;
    double pos = t / eighth;
    long is_odd = ((long)floor(pos)) % 2;
    return (is_odd == 1) ? t + swing_amt * eighth : t;
}

static void trigger_voice(const BogScheduler* scheduler, const char* voice,
                          double time_value, double midi, double velocity)
{
    if (!voice)
        return;
    const BogAudioCallbacks* audio = &scheduler->audio;
    if (strcmp(voice, "kick") == 0 && audio->kick) {
        audio->kick(audio->userdata, time_value, velocity);
        return;
    }
    if (strcmp(voice, "snare") == 0 && audio->snare) {
        audio->snare(audio->userdata, time_value, velocity);
        return;
    }
    if (strcmp(voice, "hat") == 0 && audio->hat) {
        audio->hat(audio->userdata, time_value, velocity);
        return;
    }
    if (strcmp(voice, "clap") == 0 && audio->clap) {
        audio->clap(audio->userdata, time_value, velocity);
        return;
    }
    if (strcmp(voice, "sine") == 0 && audio->sine) {
        audio->sine(audio->userdata, time_value, midi, velocity);
        return;
    }
    if (strcmp(voice, "square") == 0 && audio->square) {
        audio->square(audio->userdata, time_value, midi, velocity);
        return;
    }
    if (strcmp(voice, "triangle") == 0 && audio->triangle) {
        audio->triangle(audio->userdata, time_value, midi, velocity);
        return;
    }
    if (strcmp(voice, "noise") == 0 && audio->noise) {
        audio->noise(audio->userdata, time_value, velocity);
        return;
    }
}

static void ensure_callback_capacity(BogScheduler* scheduler)
{
    if (scheduler->callback_count < scheduler->callback_capacity)
        return;
    size_t new_cap = scheduler->callback_capacity
        ? scheduler->callback_capacity * 2
        : 4;
    BeatCallbackEntry* entries = (BeatCallbackEntry*)realloc(
        scheduler->callbacks, new_cap * sizeof(BeatCallbackEntry));
    if (!entries)
        return;
    for (size_t i = scheduler->callback_capacity; i < new_cap; ++i) {
        entries[i].active = false;
        entries[i].cb = NULL;
        entries[i].userdata = NULL;
        entries[i].handle = -1;
    }
    scheduler->callbacks = entries;
    scheduler->callback_capacity = new_cap;
}

static void notify_beat_callbacks(BogScheduler* scheduler, int beat)
{
    for (size_t i = 0; i < scheduler->callback_capacity; ++i) {
        if (scheduler->callbacks[i].active && scheduler->callbacks[i].cb) {
            scheduler->callbacks[i].cb(beat, scheduler->callbacks[i].userdata);
        }
    }
}

static void scheduler_init(BogScheduler* scheduler,
                           const BogAudioCallbacks* audio,
                           const BogBuiltins* builtins,
                           BogStateManager* state_manager)
{
    if (!scheduler)
        return;
    memset(scheduler, 0, sizeof(*scheduler));
    if (audio)
        scheduler->audio = *audio;
    scheduler->builtins = builtins;
    scheduler->state_manager = state_manager;
    scheduler->bpm = 120.0;
    scheduler->swing = 0.0;
    scheduler->lookahead_ms = 80.0;
    scheduler->grid_beats = 0.25;
    scheduler->current_beat = 0;
    scheduler->next_callback_handle = 1;
}

BogScheduler* bog_scheduler_create(const BogAudioCallbacks* audio,
                                           const BogBuiltins* builtins,
                                           BogStateManager* state_manager)
{
    BogScheduler* scheduler = (BogScheduler*)calloc(
        1, sizeof(BogScheduler));
    if (!scheduler)
        return NULL;
    scheduler_init(scheduler, audio, builtins, state_manager);
    return scheduler;
}

void bog_scheduler_destroy(BogScheduler* scheduler)
{
    if (!scheduler)
        return;
    free(scheduler->callbacks);
    free(scheduler);
}

void bog_scheduler_set_program(BogScheduler* scheduler,
                                   const BogProgram* program)
{
    if (!scheduler)
        return;
    scheduler->program = program;
}

void bog_scheduler_configure(BogScheduler* scheduler, double bpm,
                                 double swing, double lookahead_ms,
                                 double grid_beats)
{
    if (!scheduler)
        return;
    if (bpm > 0.0)
        scheduler->bpm = bpm;
    scheduler->swing = swing;
    if (lookahead_ms > 0.0)
        scheduler->lookahead_ms = lookahead_ms;
    if (grid_beats > 0.0)
        scheduler->grid_beats = grid_beats;
}

void bog_scheduler_start(BogScheduler* scheduler)
{
    if (!scheduler)
        return;
    if (scheduler->audio.init)
        scheduler->audio.init(scheduler->audio.userdata);
    scheduler->running = true;
}

void bog_scheduler_stop(BogScheduler* scheduler)
{
    if (!scheduler)
        return;
    scheduler->running = false;
    scheduler->current_beat = 0;
    notify_beat_callbacks(scheduler, scheduler->current_beat);
}

double bog_scheduler_now(const BogScheduler* scheduler)
{
    if (!scheduler || !scheduler->audio.time)
        return 0.0;
    return scheduler->audio.time(scheduler->audio.userdata);
}

int bog_scheduler_add_beat_callback(BogScheduler* scheduler,
                                        BogBeatCallback cb, void* userdata)
{
    if (!scheduler || !cb)
        return -1;
    ensure_callback_capacity(scheduler);
    for (size_t i = 0; i < scheduler->callback_capacity; ++i) {
        if (!scheduler->callbacks[i].active) {
            scheduler->callbacks[i].active = true;
            scheduler->callbacks[i].cb = cb;
            scheduler->callbacks[i].userdata = userdata;
            scheduler->callbacks[i].handle = scheduler->next_callback_handle++;
            scheduler->callback_count++;
            return scheduler->callbacks[i].handle;
        }
    }
    return -1;
}

void bog_scheduler_remove_beat_callback(BogScheduler* scheduler,
                                            int handle)
{
    if (!scheduler || handle < 0)
        return;
    for (size_t i = 0; i < scheduler->callback_capacity; ++i) {
        if (scheduler->callbacks[i].active
            && scheduler->callbacks[i].handle == handle) {
            scheduler->callbacks[i].active = false;
            scheduler->callbacks[i].cb = NULL;
            scheduler->callbacks[i].userdata = NULL;
            scheduler->callbacks[i].handle = -1;
            if (scheduler->callback_count > 0)
                scheduler->callback_count--;
            break;
        }
    }
}

static void scheduler_query_and_schedule(BogScheduler* scheduler, double t)
{
    if (!scheduler->program || !scheduler->builtins)
        return;
    BogArena* arena = bog_arena_create();
    if (!arena)
        return;

    BogTerm voiceVar = { .type = CPROLOG_TERM_VAR, .value.atom = "Voice" };
    BogTerm pitchVar = { .type = CPROLOG_TERM_VAR, .value.atom = "Pitch" };
    BogTerm velVar = { .type = CPROLOG_TERM_VAR, .value.atom = "Vel" };
    BogTerm timeTerm = { .type = CPROLOG_TERM_NUM };
    timeTerm.value.number = t;
    BogTerm* args[4] = { &voiceVar, &pitchVar, &velVar, &timeTerm };
    BogTerm eventTerm;
    eventTerm.type = CPROLOG_TERM_COMPOUND;
    eventTerm.value.compound.functor = "event";
    eventTerm.value.compound.args = args;
    eventTerm.value.compound.arity = 4;

    BogGoal goal;
    goal.kind = CPROLOG_GOAL_TERM;
    goal.data.term = &eventTerm;
    BogGoalList goals;
    goals.count = 1;
    goals.items = &goal;

    BogEnv env;
    bog_env_init(&env);

    BogSolutions solutions = { 0 };
    BogContext ctx;
    ctx.bpm = scheduler->bpm;
    ctx.state_manager = scheduler->state_manager;
    bog_resolve(&goals, &env, scheduler->program, &ctx,
                    scheduler->builtins, &solutions, arena);
    bog_env_free(&env);

    for (size_t i = 0; i < solutions.count; ++i) {
        BogArena* subst_arena = bog_arena_create();
        if (!subst_arena)
            continue;
        BogTerm* voiceValue = bog_subst_term(
            &voiceVar, &solutions.envs[i], subst_arena);
        char* voiceStr = voiceValue
            ? bog_term_to_string(voiceValue, subst_arena)
            : NULL;

        BogTerm* pitchValue = bog_subst_term(
            &pitchVar, &solutions.envs[i], subst_arena);
        double midi = 48.0;
        if (pitchValue && pitchValue->type == CPROLOG_TERM_NUM) {
            midi = pitchValue->value.number;
        }

        BogTerm* velValue = bog_subst_term(&velVar, &solutions.envs[i],
                                                   subst_arena);
        double vel = 0.7;
        if (velValue && velValue->type == CPROLOG_TERM_NUM) {
            if (velValue->value.number < 0.0)
                vel = 0.0;
            else if (velValue->value.number > 1.0)
                vel = 1.0;
            else
                vel = velValue->value.number;
        }

        double scheduled_time = swing_adjust(t, scheduler->bpm,
                                             scheduler->swing);
        if (voiceStr) {
            trigger_voice(scheduler, voiceStr, scheduled_time, midi, vel);
        }
        free(voiceStr);
        bog_env_free(&solutions.envs[i]);
        bog_arena_destroy(subst_arena);
    }
    free(solutions.envs);
    bog_arena_destroy(arena);
}

void bog_scheduler_tick_at(BogScheduler* scheduler, double now_seconds)
{
    if (!scheduler || !scheduler->program)
        return;
    double ahead = scheduler->lookahead_ms / 1000.0;
    double beat_duration = 60.0 / scheduler->bpm;
    double step = beat_duration * scheduler->grid_beats;
    if (step <= 0.0)
        step = beat_duration * 0.25;
    double start_quantized = floor(now_seconds / step) * step;

    int new_beat = (int)floor(now_seconds / beat_duration);
    if (new_beat != scheduler->current_beat) {
        scheduler->current_beat = new_beat;
        notify_beat_callbacks(scheduler, scheduler->current_beat);
    }

    for (double t = start_quantized; t < now_seconds + ahead; t += step) {
        scheduler_query_and_schedule(scheduler, t + step);
    }
}

void bog_scheduler_tick(BogScheduler* scheduler)
{
    if (!scheduler || !scheduler->running)
        return;
    double now = bog_scheduler_now(scheduler);
    bog_scheduler_tick_at(scheduler, now);
}

/* Transition manager */

static void transition_manager_init(BogTransitionManager* manager,
                                    BogScheduler* scheduler,
                                    double quantization_beats)
{
    if (!manager)
        return;
    manager->scheduler = scheduler;
    manager->quantization = (quantization_beats > 0.0) ? quantization_beats
                                                       : 4.0;
    manager->pending_program = NULL;
    manager->pending_boundary = 0.0;
    manager->has_pending = false;
}

BogTransitionManager*
bog_transition_manager_create(BogScheduler* scheduler,
                                  double quantization_beats)
{
    BogTransitionManager* manager = (BogTransitionManager*)calloc(
        1, sizeof(BogTransitionManager));
    if (!manager)
        return NULL;
    transition_manager_init(manager, scheduler, quantization_beats);
    return manager;
}

void bog_transition_manager_destroy(BogTransitionManager* manager)
{
    if (!manager)
        return;
    free(manager);
}

void bog_transition_manager_schedule(BogTransitionManager* manager,
                                         const BogProgram* program)
{
    if (!manager || !program)
        return;
    double now = bog_scheduler_now(manager->scheduler);
    double bpm = manager->scheduler ? manager->scheduler->bpm : 120.0;
    double beat_duration = 60.0 / (bpm > 0.0 ? bpm : 120.0);
    double quant_duration = beat_duration * manager->quantization;
    double current_phase = fmod(now, quant_duration);
    double time_to_next = (current_phase == 0.0)
        ? 0.0
        : (quant_duration - current_phase);
    manager->pending_boundary = now + time_to_next;
    manager->pending_program = program;
    manager->has_pending = true;
}

void bog_transition_manager_cancel(BogTransitionManager* manager)
{
    if (!manager)
        return;
    manager->pending_program = NULL;
    manager->pending_boundary = 0.0;
    manager->has_pending = false;
}

bool bog_transition_manager_has_pending(
    const BogTransitionManager* manager)
{
    return manager && manager->has_pending;
}

void bog_transition_manager_process(BogTransitionManager* manager,
                                        double now_seconds)
{
    if (!manager || !manager->has_pending)
        return;
    if (now_seconds + 1e-9 >= manager->pending_boundary) {
        bog_scheduler_set_program(manager->scheduler,
                                      manager->pending_program);
        manager->pending_program = NULL;
        manager->has_pending = false;
        manager->pending_boundary = 0.0;
    }
}
