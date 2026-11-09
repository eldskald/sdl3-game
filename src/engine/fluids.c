#include "engine/fluids.h"
#include "data-structs/dynarr.h"
#include "data-structs/hashmap.h"
#include "defs.h"
#include "engine/audio.h"
#include "engine/bgs.h"
#include "engine/camera.h"
#include "engine/game-manager.h"
#include "engine/sfx.h"
#include "engine/tilemap.h"
#include "paths.h"
#include "services/csv.h"
#include "utils/logger.h"
#include "utils/parsing.h"
#include <SDL3/SDL.h>

#define FLUID_SFX_MAX 5
#define FLUID_SFX_VOICES_TOTAL 4



typedef struct {
    double density;
    double move_down_time;
    double move_diag_time;
    double move_side_time;
    double min_spread;
    double particle_vol;
    Uint8 col_r, col_g, col_b, col_a;
} fluid_data;

typedef struct {
    double volume[FLUIDS_TOTAL];
} fluids;

typedef struct {
    double x, y;
    double vel_x, vel_y;
    fluid_type type;
} fluid_particle;

typedef struct {
    double volume;
    bool solid;
} neighbor;

typedef struct {
    Uint32 x, y, w, h;
    fluid_type type;
    double volume;
    bool clear;
} mod;

typedef struct {
    double gravity;
    double size_threshold;
    double splash_fan_angle;
    double splash_angle_jitter;
    double splash_particle_max_speed;
    double splash_volume;
    double splash_body_max_speed;
} fluid_particles_data;

typedef struct {
    Uint32 x, y;
    Uint32 w;
    double speed;
} splash;

typedef struct {
    Uint32 x, y;
    Uint32 size;
    double strength;
} ripple;

typedef struct {
    long x, y;
    long dir;
    Uint32 len;
    double fills[SIMULATION_SPACE_X * FLUIDS_SPLIT_X];
    double deltas[SIMULATION_SPACE_X * FLUIDS_SPLIT_X];
} stretch;



static const char fluids_csv[] = {
#embed FLUIDS_CSV_PATH suffix(, '\0')
};
static const char fluid_particles_csv[] = {
#embed FLUIDS_PARTICLES_CSV_PATH suffix(, '\0')
};
static const char ripples_csv[] = {
#embed FLUIDS_RIPPLES_CSV_PATH suffix(, '\0')
};
static const char audio_csv[] = {
#embed FLUIDS_AUDIO_CSV_PATH suffix(, '\0')
};

// A sound embedded on the executable that plays on `type`. A `NULL` `mem` with
// a `0` `size` is for no sound, so `type` plays nothing on its bank.
typedef struct {
    fluid_type type;
    const Uint8* mem;
    size_t size;
} fluid_sfx_file;

// A set of sounds per fluid type and the voices they play on, with a
// `cooldown` between any two of them loaded from `FLUIDS_AUDIO_CSV_PATH`.
// Voices are created on the first sound played instead of on `FLUIDS_start()`,
// so fluids don't need `SFX_start()` before them. Sounds are played from more
// than one thread, so each bank has its own mutex.
typedef struct {
    const char* name;
    double cooldown;
    const fluid_sfx_file* files;
    size_t files_total;
    audio_wav* wavs[FLUIDS_TOTAL][FLUID_SFX_MAX];
    Uint8 counts[FLUIDS_TOTAL];
    size_t voices[FLUID_SFX_VOICES_TOTAL];
    bool voices_created;
    Uint8 next_voice;
    Uint64 last_ns;
    bool played;
    SDL_Mutex* mutex;
} fluid_sfx_bank;

static const Uint8 water_slosh_1_wav[] = {
#embed FLUIDS_WATER_SLOSH_1_SFX_PATH
};
static const Uint8 water_slosh_2_wav[] = {
#embed FLUIDS_WATER_SLOSH_2_SFX_PATH
};
static const Uint8 water_slosh_3_wav[] = {
#embed FLUIDS_WATER_SLOSH_3_SFX_PATH
};
static const Uint8 water_slosh_4_wav[] = {
#embed FLUIDS_WATER_SLOSH_4_SFX_PATH
};
static const Uint8 water_slosh_5_wav[] = {
#embed FLUIDS_WATER_SLOSH_5_SFX_PATH
};

static const Uint8 water_splash_1_wav[] = {
#embed FLUIDS_WATER_SPLASH_1_SFX_PATH
};
static const Uint8 water_splash_2_wav[] = {
#embed FLUIDS_WATER_SPLASH_2_SFX_PATH
};
static const Uint8 water_splash_3_wav[] = {
#embed FLUIDS_WATER_SPLASH_3_SFX_PATH
};
static const Uint8 lava_splash_1_wav[] = {
#embed FLUIDS_LAVA_SPLASH_1_SFX_PATH
};


static const fluid_sfx_file slosh_sfx_files[] = {
    {fluid_water, water_slosh_1_wav, sizeof(water_slosh_1_wav)},
    {fluid_water, water_slosh_2_wav, sizeof(water_slosh_2_wav)},
    {fluid_water, water_slosh_3_wav, sizeof(water_slosh_3_wav)},
    {fluid_water, water_slosh_4_wav, sizeof(water_slosh_4_wav)},
    {fluid_water, water_slosh_5_wav, sizeof(water_slosh_5_wav)},
    {fluid_lava, NULL, 0},
};
static const fluid_sfx_file splash_sfx_files[] = {
    {fluid_water, water_splash_1_wav, sizeof(water_splash_1_wav)},
    {fluid_water, water_splash_2_wav, sizeof(water_splash_2_wav)},
    {fluid_water, water_splash_3_wav, sizeof(water_splash_3_wav)},
    {fluid_lava, lava_splash_1_wav, sizeof(lava_splash_1_wav)},
};

static fluid_sfx_bank slosh_sfx = {
    .name = "slosh",
    .files = slosh_sfx_files,
    .files_total = SDL_arraysize(slosh_sfx_files),
};
static fluid_sfx_bank splash_sfx = {
    .name = "splash",
    .files = splash_sfx_files,
    .files_total = SDL_arraysize(splash_sfx_files),
};

// Only the fluids thread writes on `grid`, locking `grid_mutex` while it does,
// so it reads it without locking and other threads lock to read it. Writes
// are kept to quick bulk copies and the queues drained at the end of each
// update, so other threads don't wait long.
static fluids grid[TILEMAP_SIZE_X * FLUIDS_SPLIT_X]
                  [TILEMAP_SIZE_Y * FLUIDS_SPLIT_Y] = {0};
static SDL_Mutex* grid_mutex = NULL;
static fluids buffer[SIMULATION_SPACE_X * FLUIDS_SPLIT_X]
                    [SIMULATION_SPACE_Y * FLUIDS_SPLIT_Y] = {0};

static long sim_x = 0;
static long sim_y = 0;
static fluid_data data[FLUIDS_TOTAL] = {0};
static fluid_particles_data particles_data = {0};

static dynarr mods = (dynarr){0};
static SDL_Mutex* mods_mutex = NULL;
static dynarr queued_particles = (dynarr){0};
static SDL_Mutex* queued_particles_mutex = NULL;
static dynarr splashes = (dynarr){0};
static SDL_Mutex* splashes_mutex = NULL;

static dynarr ripples = (dynarr){0};
static SDL_Mutex* ripples_mutex = NULL;
static Uint8 ripples_idx = 0;
static float ripples_x = 0.0f;
static float ripples_y = 0.0f;
static SDL_AtomicInt ripples_ticks = {0};
// The camera when `FLUIDS_prepare()` was called, so `FLUIDS_draw()` draws what
// it prepared even if the camera moved since.
static SDL_FRect prepared_cam = {0};
static float ripples_damping = 0.0f;
static float ripples_refraction = 0.0f;
static fluid_type ordered[FLUIDS_TOTAL] = {0};

static dynarr particles = (dynarr){0}; // fluid_particle dynarr
static SDL_Mutex* particles_mutex = NULL;

// Down, diagonal and sideways move timers, in that order.
static double move_timers[3][FLUIDS_TOTAL] = {0};

// Like a `pool` of `fluid_body`, but cleared by setting `len` to `0`, keeping
// every slot and the memory of their `cells`, so finding bodies only allocates
// when it finds more or bigger ones than ever before. Slots from `len` on are
// left over from earlier calls, ignored even if `used`. Slots below `inited`
// have their `cells` initialized, so they're reused instead of initialized
// again and deinitialized when quitting. Removed ids are pushed to `free_ids`,
// a `size_t` dynarr, to be reused by the next adds before `len` grows.
typedef struct {
    fluid_body* items;
    bool* used;
    size_t len;
    size_t inited;
    size_t cap;
    dynarr free_ids;
} body_pool;

// Bodies of fluid found by `FLUIDS_play_bgs()`, on a copy of the simulated
// rect of their own, since they're found off the fluids thread. `body_ids` maps
// each cell of `bodies_buffer` to the id + 1 of the body of each fluid type
// it's on, `0` being no body. Once the bodies are compacted, only whether it's
// `0` still holds. They aren't locked, so they're only used by the thread
// finding them.
static fluids bodies_buffer[SIMULATION_SPACE_X * FLUIDS_SPLIT_X]
                           [SIMULATION_SPACE_Y * FLUIDS_SPLIT_Y] = {0};
static size_t body_ids[FLUIDS_TOTAL][SIMULATION_SPACE_X * FLUIDS_SPLIT_X]
                      [SIMULATION_SPACE_Y * FLUIDS_SPLIT_Y] = {0};
static body_pool bodies = (body_pool){0};

// The tracks each fluid type's bodies play, `bgs_track_none` for no track.
typedef struct {
    bgs_track static_bgs;
    bgs_track running_bgs;
} fluid_bgs;
static const fluid_bgs body_bgs[FLUIDS_TOTAL] = {
    [fluid_water] =
        {
            .static_bgs = bgs_track_none,
            .running_bgs = bgs_track_water_streaming,
        },
    [fluid_lava] =
        {
            .static_bgs = bgs_track_lava_bubbling,
            .running_bgs = bgs_track_none,
        },
};

// Each fluid type's volumes, from `FLUIDS_AUDIO_CSV_PATH`. Its static track
// plays at up to `static_max_vol`, reached when a body's total volume is at
// `static_max_vol_at`, and its running track at up to `running_max_vol`,
// reached when a body's moving cells are at `running_max_vol_at`. A `0` on
// either `_at` means its track doesn't scale with it. Splashes play at
// `splash_vol` and ripples at up to `ripple_vol`, by their strength. A `0`
// volume is for a sound it doesn't have.
typedef struct {
    float static_max_vol;
    double static_max_vol_at;
    float running_max_vol;
    double running_max_vol_at;
    float splash_vol;
    float ripple_vol;
} fluid_audio;
static fluid_audio audio[FLUIDS_TOTAL] = {0};

// Pixels off the camera at which every fluid track and sound goes silent, from
// `FLUIDS_AUDIO_CSV_PATH`.
static double falloff_range = 0.0;

// We treat these differently from sprites and bodies because these particles
// are not exposed, their indexes are not IDs that other code should keep track
// of so we can move them around the array safely.
static void remove_fluid_particle(size_t index) {
    fluid_particle* last = dynarr_at(particles.len - 1, &particles);
    *(fluid_particle*)dynarr_at(index, &particles) = *last;
    remove_at_from_dynarr(particles.len - 1, &particles);
}

static void add_fluid_particle(fluid_particle particle) {
    push_to_dynarr(&particle, &particles);
}

static void queue_item(dynarr* queue, SDL_Mutex* mutex, const void* item) {
    SDL_LockMutex(mutex);
    push_to_dynarr(item, queue);
    SDL_UnlockMutex(mutex);
}

static void drain_queue(dynarr* queue, SDL_Mutex* mutex, void (*apply)(void*)) {
    SDL_LockMutex(mutex);
    if (apply)
        for (size_t i = 0; i < queue->len; i++)
            apply(dynarr_at(i, queue));
    clear_dynarr(queue);
    SDL_UnlockMutex(mutex);
}

static void clear_queue(dynarr* queue, SDL_Mutex** mutex) {
    deinit_dynarr(queue);
    SDL_DestroyMutex(*mutex);
    *mutex = NULL;
}



static void load_fluid_data(const char* csv) {
    // Rows can be in any order, so they're read here and then placed on `data`
    // by their fluid.
    struct {
        fluid_type type;
        fluid_data data;
    } rows[FLUIDS_TOTAL] = {0};
    fluid_data* d = &rows[0].data;
    parsing_field fields[] = {
        {"fluid", parsing_fluid, &rows[0].type},
        {"density", parsing_double, &d->density},
        {"move_down_time", parsing_double, &d->move_down_time},
        {"move_diag_time", parsing_double, &d->move_diag_time},
        {"move_side_time", parsing_double, &d->move_side_time},
        {"min_spread", parsing_double, &d->min_spread},
        {"particle_vol", parsing_double, &d->particle_vol},
        {"col_r", parsing_uint8, &d->col_r},
        {"col_g", parsing_uint8, &d->col_g},
        {"col_b", parsing_uint8, &d->col_b},
        {"col_a", parsing_uint8, &d->col_a},
    };
    dynarr parsed = CSV_parse(csv);
    size_t n =
        read_csv_table(&parsed, fields, SDL_arraysize(fields), sizeof(rows[0]));
    for (size_t i = 0; i < n; i++)
        data[rows[i].type] = rows[i].data;
    CSV_free(&parsed);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Fluid data loaded successfully.");
}

static void order_fluids_by_density(void) {
    // Initiate ordered_fluids
    for (int i = 0; i < FLUIDS_TOTAL; i++) {
        ordered[i] = i;
    }

    // Bubble sort order it
    bool found = true;
    while (found) {
        found = false;
        for (int i = 1; i < FLUIDS_TOTAL; i++) {
            if (data[ordered[i]].density > data[ordered[i - 1]].density) {
                int mem = ordered[i];
                ordered[i] = ordered[i - 1];
                ordered[i - 1] = mem;
                found = true;
            }
        }
    }
}

static double get_move_time(fluid_type type, Uint8 move) {
    double times[3] = {data[type].move_down_time,
                       data[type].move_diag_time,
                       data[type].move_side_time};
    return times[move];
}

// Starts each move timer at a different point of its period so the moves, each
// a full pass over the grid, land on different ticks as much as possible. The
// most frequent go first, each starting where the busiest tick it would land on
// is the least busy.
static void stagger_move_timers(void) {
    double tick = 1.0f / FLUIDS_T_TARGET_FPS;
    Uint32 ticks = FLUIDS_STAGGER_SECONDS * FLUIDS_T_TARGET_FPS;
    Uint32 load[FLUIDS_STAGGER_SECONDS * FLUIDS_T_TARGET_FPS] = {0};
    bool placed[3][FLUIDS_TOTAL] = {0};
    for (Uint8 n = 0; n < 3 * FLUIDS_TOTAL; n++) {
        Uint8 move = 0;
        fluid_type type = 0;
        Uint32 period = 0;
        for (Uint8 m = 0; m < 3; m++) {
            for (Uint8 t = 0; t < FLUIDS_TOTAL; t++) {
                Uint32 p = SDL_max(1, SDL_round(get_move_time(t, m) / tick));
                if (placed[m][t] || (period > 0 && p >= period)) continue;
                move = m;
                type = t;
                period = p;
            }
        }
        placed[move][type] = true;

        Uint32 best = 0;
        Uint32 best_peak = SDL_MAX_UINT32;
        Uint32 best_sum = SDL_MAX_UINT32;
        for (Uint32 o = 0; o < SDL_min(period, ticks); o++) {
            Uint32 peak = 0;
            Uint32 sum = 0;
            for (Uint32 t = o; t < ticks; t += period) {
                peak = SDL_max(peak, load[t]);
                sum += load[t];
            }
            if (peak < best_peak || (peak == best_peak && sum < best_sum)) {
                best = o;
                best_peak = peak;
                best_sum = sum;
            }
        }
        for (Uint32 t = best; t < ticks; t += period)
            load[t]++;

        // Half a tick early, so it isn't right at the edge of rounding.
        move_timers[move][type] =
            SDL_max(get_move_time(type, move) - (best + 0.5f) * tick, 0.0f);
    }
}

static void load_fluid_particles_data(const char* csv) {
    fluid_particles_data* p = &particles_data;
    parsing_field fields[] = {
        {"gravity", parsing_double, &p->gravity},
        {"size_threshold", parsing_double, &p->size_threshold},
        {"fan_angle", parsing_double, &p->splash_fan_angle},
        {"angle_jitter", parsing_double, &p->splash_angle_jitter},
        {"particle_max_speed", parsing_double, &p->splash_particle_max_speed},
        {"volume", parsing_double, &p->splash_volume},
        {"body_max_speed", parsing_double, &p->splash_body_max_speed},
    };
    dynarr parsed = CSV_parse(csv);
    read_csv_keys(&parsed, fields, SDL_arraysize(fields));
    CSV_free(&parsed);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Fluid particles data loaded successfully.");
}

static void load_ripples_data(const char* csv) {
    parsing_field fields[] = {
        {"damping", parsing_float, &ripples_damping},
        {"refraction", parsing_float, &ripples_refraction},
    };
    dynarr parsed = CSV_parse(csv);
    read_csv_keys(&parsed, fields, SDL_arraysize(fields));
    CSV_free(&parsed);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Ripples data loaded successfully.");
}

// Its first section has the banks' cooldowns and the falloff range shared by
// every track and sound, and its second the table of each fluid's volumes.
static void load_audio_data(const char* csv) {
    parsing_field keys[] = {
        {"splash_cooldown", parsing_double, &splash_sfx.cooldown},
        {"ripple_cooldown", parsing_double, &slosh_sfx.cooldown},
        {"falloff_range", parsing_double, &falloff_range},
    };
    dynarr parsed = CSV_parse_section(csv, 0);
    read_csv_keys(&parsed, keys, SDL_arraysize(keys));
    CSV_free(&parsed);

    // Rows can be in any order, so they're read here and then placed on
    // `audio` by their fluid.
    struct {
        fluid_type type;
        fluid_audio audio;
    } rows[FLUIDS_TOTAL] = {0};
    fluid_audio* a = &rows[0].audio;
    parsing_field fields[] = {
        {"fluid", parsing_fluid, &rows[0].type},
        {"static_max_vol", parsing_float, &a->static_max_vol},
        {"static_max_vol_at", parsing_double, &a->static_max_vol_at},
        {"running_max_vol", parsing_float, &a->running_max_vol},
        {"running_max_vol_at", parsing_double, &a->running_max_vol_at},
        {"splash_vol", parsing_float, &a->splash_vol},
        {"ripple_vol", parsing_float, &a->ripple_vol},
    };
    parsed = CSV_parse_section(csv, 1);
    size_t n =
        read_csv_table(&parsed, fields, SDL_arraysize(fields), sizeof(rows[0]));
    for (size_t i = 0; i < n; i++)
        audio[rows[i].type] = rows[i].audio;
    CSV_free(&parsed);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Fluid audio data loaded successfully.");
}

// Files without a sound are skipped.
static void load_sfx_bank(fluid_sfx_bank* bank) {
    for (size_t i = 0; i < bank->files_total; i++) {
        fluid_sfx_file f = bank->files[i];
        if (!f.mem) continue;
        Uint8* count = &bank->counts[f.type];
        if (*count >= FLUID_SFX_MAX) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Fluid type %d has more than %d %s SFX, ignoring "
                        "the rest",
                        f.type,
                        FLUID_SFX_MAX,
                        bank->name);
            continue;
        }
        audio_wav* wav = AUDIO_create_wav(
            bank->name, SDL_IOFromConstMem(f.mem, f.size), true);
        if (wav) bank->wavs[f.type][(*count)++] = wav;
    }
    bank->mutex = SDL_CreateMutex();
    log_creation(
        SDL_LOG_CATEGORY_APPLICATION, bank->mutex, "fluids' SFX bank mutex");
}

static void unload_sfx_bank(fluid_sfx_bank* bank) {
    for (int t = 0; t < FLUIDS_TOTAL; t++) {
        for (int i = 0; i < bank->counts[t]; i++)
            AUDIO_destroy_wav(bank->wavs[t][i]);
        bank->counts[t] = 0;
    }
    if (bank->voices_created)
        for (int i = 0; i < FLUID_SFX_VOICES_TOTAL; i++)
            SFX_del_voice(bank->voices[i]);
    bank->voices_created = false;
    bank->next_voice = 0;
    bank->played = false;
    SDL_DestroyMutex(bank->mutex);
    bank->mutex = NULL;
}

static void init_bodies(void) {
    bodies = (body_pool){.cap = FLUIDS_STARTING_BODIES_POOL_CAP};
    bodies.items = SDL_malloc(bodies.cap * sizeof(fluid_body));
    bodies.used = SDL_calloc(bodies.cap, sizeof(bool));
    init_dynarr(&bodies.free_ids, sizeof(size_t));
}

static void deinit_bodies(void) {
    for (size_t id = 0; id < bodies.inited; id++)
        deinit_dynarr(&bodies.items[id].cells);
    SDL_free(bodies.items);
    SDL_free(bodies.used);
    deinit_dynarr(&bodies.free_ids);
    bodies = (body_pool){0};
}

static void clear_bodies(void) {
    bodies.len = 0;
    clear_dynarr(&bodies.free_ids);
}

// Pointer to the body of id `id`, or `NULL` if there's none. Adding might move
// the bodies, so don't hold on to it across adds.
static fluid_body* get_body(size_t id) {
    return id < bodies.len && bodies.used[id] ? &bodies.items[id] : NULL;
}

// Takes a removed id if there's any, or the slot at `len` otherwise, doubling
// the cap when it's full. Returns its id, with an empty body of `type` on it.
static size_t add_body(fluid_type type) {
    size_t id;
    if (bodies.free_ids.len > 0) {
        size_t top = bodies.free_ids.len - 1;
        id = *(size_t*)dynarr_at(top, &bodies.free_ids);
        remove_at_from_dynarr(top, &bodies.free_ids);
    } else {
        if (bodies.len == bodies.cap) {
            bodies.cap *= 2;
            bodies.items =
                SDL_realloc(bodies.items, bodies.cap * sizeof(fluid_body));
            bodies.used = SDL_realloc(bodies.used, bodies.cap * sizeof(bool));
        }
        id = bodies.len++;
    }
    // Slots from `inited` on were never written, so they're not read
    dynarr cells;
    if (id == bodies.inited) {
        init_dynarr(&cells, sizeof(fluid_coords));
        bodies.inited++;
    } else {
        cells = bodies.items[id].cells;
        clear_dynarr(&cells);
    }
    bodies.items[id] = (fluid_body){.type = type, .cells = cells};
    bodies.used[id] = true;
    return id;
}

static void remove_body(size_t id) {
    bodies.used[id] = false;
    push_to_dynarr(&id, &bodies.free_ids);
}

// Fills every removed id below `len` with the body on the last slot, so the
// bodies are all on `0` to `len` with no gaps. Bodies are swapped instead of
// copied, so the removed ones keep owning their `cells` past `len`.
static void compact_bodies(void) {
    while (bodies.len > 0 && !bodies.used[bodies.len - 1])
        bodies.len--;
    for (size_t i = 0; i < bodies.free_ids.len; i++) {
        size_t hole = *(size_t*)dynarr_at(i, &bodies.free_ids);
        if (hole >= bodies.len) continue;
        size_t last = bodies.len - 1;
        fluid_body removed = bodies.items[hole];
        bodies.items[hole] = bodies.items[last];
        bodies.items[last] = removed;
        bodies.used[hole] = true;
        bodies.used[last] = false;
        while (bodies.len > 0 && !bodies.used[bodies.len - 1])
            bodies.len--;
    }
    clear_dynarr(&bodies.free_ids);
}

void FLUIDS_start(void) {
    load_fluid_data(fluids_csv);
    load_fluid_particles_data(fluid_particles_csv);
    load_ripples_data(ripples_csv);
    load_audio_data(audio_csv);
    load_sfx_bank(&slosh_sfx);
    load_sfx_bank(&splash_sfx);
    order_fluids_by_density();
    stagger_move_timers();
    init_dynarr(&mods, sizeof(mod));
    mods_mutex = SDL_CreateMutex();
    log_creation(
        SDL_LOG_CATEGORY_APPLICATION, mods_mutex, "fluids' mods mutex");
    init_dynarr(&queued_particles, sizeof(fluid_particle));
    queued_particles_mutex = SDL_CreateMutex();
    log_creation(SDL_LOG_CATEGORY_APPLICATION,
                 queued_particles_mutex,
                 "fluids' queued particles mutex");
    init_dynarr(&splashes, sizeof(splash));
    splashes_mutex = SDL_CreateMutex();
    log_creation(
        SDL_LOG_CATEGORY_APPLICATION, splashes_mutex, "fluids' splashes mutex");
    init_dynarr(&ripples, sizeof(ripple));
    ripples_mutex = SDL_CreateMutex();
    log_creation(
        SDL_LOG_CATEGORY_APPLICATION, ripples_mutex, "fluids' ripples mutex");
    ripples_idx = 0;
    ripples_x = 0.0f;
    ripples_y = 0.0f;
    SDL_SetAtomicInt(&ripples_ticks, 0);
    init_dynarr_with(&particles,
                     sizeof(fluid_particle),
                     FLUIDS_STARTING_PARTICLES_DYNARR_CAP);
    particles_mutex = SDL_CreateMutex();
    log_creation(SDL_LOG_CATEGORY_APPLICATION,
                 particles_mutex,
                 "fluids' particles mutex");
    init_bodies();
    grid_mutex = SDL_CreateMutex();
    log_creation(
        SDL_LOG_CATEGORY_APPLICATION, grid_mutex, "fluids' grid mutex");
}

void FLUIDS_stop(void) {
    unload_sfx_bank(&slosh_sfx);
    unload_sfx_bank(&splash_sfx);
    clear_queue(&mods, &mods_mutex);
    clear_queue(&queued_particles, &queued_particles_mutex);
    clear_queue(&splashes, &splashes_mutex);
    clear_queue(&ripples, &ripples_mutex);
    SDL_LockMutex(grid_mutex);
    SDL_memset(grid, 0, sizeof(grid));
    SDL_UnlockMutex(grid_mutex);
    SDL_memset(move_timers, 0, sizeof(move_timers));
    deinit_dynarr(&particles);
    SDL_DestroyMutex(particles_mutex);
    particles_mutex = NULL;
    deinit_bodies();
    SDL_memset(body_ids, 0, sizeof(body_ids));
    SDL_DestroyMutex(grid_mutex);
    grid_mutex = NULL;
}



static bool is_solid(Uint32 x, Uint32 y) {
    tile data = (tile){0};
    TILEMAP_get_at(x / FLUIDS_SPLIT_X, y / FLUIDS_SPLIT_Y, &data);
    return data.type && !data.platform;
}

#define TOP [1][0]
#define BOT [1][2]
#define LEFT [0][1]
#define RIGHT [2][1]
#define TLEFT [0][0]
#define TRIGHT [2][0]
#define BLEFT [0][2]
#define BRIGHT [2][2]

#define SELF [1][1]

// `buffer` only holds the simulated rect, so it's indexed relative to it.
#define BUFFER(x, y) buffer[(x) - sim_x][(y) - sim_y]

static void initiate_buffer(void) {
    for (Uint32 x = 0; x < SIMULATION_SPACE_X * FLUIDS_SPLIT_X; x++)
        SDL_memcpy(buffer[x], &grid[sim_x + x][sim_y], sizeof(buffer[x]));
}

static void transfer_buffer(void) {
    SDL_LockMutex(grid_mutex);
    for (Uint32 x = 0; x < SIMULATION_SPACE_X * FLUIDS_SPLIT_X; x++)
        SDL_memcpy(&grid[sim_x + x][sim_y], buffer[x], sizeof(buffer[x]));
    SDL_UnlockMutex(grid_mutex);
}

// Puts the simulated rect on the simulation space, in fluid cells.
static void update_simulated_rect(void) {
    SDL_Rect space = CAMERA_get_simulation_rect();
    sim_x = space.x * FLUIDS_SPLIT_X;
    sim_y = space.y * FLUIDS_SPLIT_Y;
}



static bool is_outside(long x, long y) {
    return x < sim_x || y < sim_y ||
           x >= sim_x + SIMULATION_SPACE_X * FLUIDS_SPLIT_X ||
           y >= sim_y + SIMULATION_SPACE_Y * FLUIDS_SPLIT_Y;
}

static bool is_wall(long x, long y) {
    return is_outside(x, y) || is_solid(x, y);
}

static double get_fill(fluids cell, Uint8 ordered_type) {
    double filled = 0.0f;
    for (Uint8 t = 0; t <= ordered_type; t++)
        filled += cell.volume[ordered[t]];
    return filled;
}

static void
get_neighbors(Uint32 x, Uint32 y, Uint8 ordered_type, neighbor (*nbs)[3][3]) {
    for (long i = -1; i <= 1; i++) {
        for (long j = -1; j <= 1; j++) {
            long nx = (long)x + i;
            long ny = (long)y + j;
            neighbor nb = (neighbor){.solid = is_wall(nx, ny)};
            if (!nb.solid) nb.volume = get_fill(grid[nx][ny], ordered_type);
            (*nbs)[i + 1][j + 1] = nb;
        }
    }
    // Blocking off diagonals if both up/down left/right neighbors are blocked
    if ((*nbs)TOP.solid && (*nbs)LEFT.solid) (*nbs) TLEFT.solid = true;
    if ((*nbs)TOP.solid && (*nbs)RIGHT.solid) (*nbs) TRIGHT.solid = true;
    if ((*nbs)BOT.solid && (*nbs)RIGHT.solid) (*nbs) BRIGHT.solid = true;
    if ((*nbs)BOT.solid && (*nbs)LEFT.solid) (*nbs) BLEFT.solid = true;
}

static double get_space(neighbor nb) {
    if (nb.solid || nb.volume > 1.0f - EPSILON) return 0.0f;
    return 1.0f - nb.volume;
}

// Checks the volume before the tiles, since most cells are empty.
static bool has_fluid(long x, long y, fluid_type type) {
    return !is_outside(x, y) && grid[x][y].volume[type] > EPSILON &&
           !is_solid(x, y);
}

static bool is_surface(long x, long y, fluid_type type) {
    return has_fluid(x, y, type) && !has_fluid(x, y - 1, type);
}

static bool has_any_fluid(long x, long y) {
    return !is_outside(x, y) &&
           get_fill(grid[x][y], FLUIDS_TOTAL - 1) > EPSILON && !is_solid(x, y);
}

// Like `is_surface()`, but for all fluid types together.
static bool is_any_surface(long x, long y) {
    return has_any_fluid(x, y) && !has_any_fluid(x, y - 1);
}

static double
level_fills(const double* base, bool* active, Uint32 len, double total) {
    for (Uint32 i = 0; i < len; i++)
        active[i] = true;
    double level = 0.0f;
    bool dropped = true;
    while (dropped) {
        double sum = total;
        Uint32 count = 0;
        for (Uint32 i = 0; i < len; i++) {
            if (!active[i]) continue;
            sum += base[i];
            count++;
        }
        level = sum / count;

        dropped = false;
        for (Uint32 i = 0; i < len; i++) {
            if (active[i] && base[i] > level) {
                active[i] = false;
                dropped = true;
            }
        }
    }
    return level;
}

static bool is_under_min_spread(double vol, fluid_type type) {
    return vol > EPSILON && vol < data[type].min_spread;
}

static void split_diagonally(
    double vol, double left, double right, double* l_share, double* r_share) {
    bool l = left > EPSILON;
    bool r = right > EPSILON;
    double share = l && r ? vol / 2.0f : vol;
    *l_share = l ? share : 0.0f;
    *r_share = r ? share : 0.0f;
    if (l && left < share) {
        *l_share = left;
        *r_share = r ? SDL_min(vol - left, right) : 0.0f;
    } else if (r && right < share) {
        *r_share = right;
        *l_share = l ? SDL_min(vol - right, left) : 0.0f;
    }
}

// Gets the surface stretch on row `y` starting at `x` and going in `dir`, so
// fluid poured into `(x,y)` is levelled over all of it. A cell that isn't a
// surface is a stretch of itself alone, and a blocked one is an empty stretch.
static void get_stretch(
    long x, long y, long dir, bool blocked, Uint8 ordered_type, stretch* s) {
    fluid_type type = ordered[ordered_type];
    s->x = x;
    s->y = y;
    s->dir = dir;
    s->len = 0;
    if (blocked || is_wall(x, y)) return;
    s->fills[s->len++] = get_fill(grid[x][y], ordered_type);
    if (!is_surface(x, y, type)) return;
    for (x += dir; is_surface(x, y, type); x += dir)
        s->fills[s->len++] = get_fill(grid[x][y], ordered_type);
}

// Summed before checking against `EPSILON`, so many nearly full cells still
// add up to some space.
static double get_stretch_space(const stretch* s) {
    double space = 0.0f;
    for (Uint32 i = 0; i < s->len; i++)
        space += SDL_max(1.0f - s->fills[i], 0.0f);
    return space < EPSILON ? 0.0f : space;
}

// Levels `vol` over the stretch into its `deltas`. Returns whether that would
// leave any of its cells under `min_spread`.
static bool level_stretch(stretch* s, double vol, fluid_type type) {
    bool active[SIMULATION_SPACE_X * FLUIDS_SPLIT_X] = {0};
    double level = level_fills(s->fills, active, s->len, vol);
    bool under = false;
    for (Uint32 i = 0; i < s->len; i++) {
        s->deltas[i] = active[i] ? level - s->fills[i] : 0.0f;
        double own = grid[s->x + s->dir * (long)i][s->y].volume[type];
        if (s->deltas[i] > 0.0f &&
            is_under_min_spread(own + s->deltas[i], type))
            under = true;
    }
    return under;
}

static void pour_into_stretch(const stretch* s, fluid_type type) {
    for (Uint32 i = 0; i < s->len; i++)
        BUFFER(s->x + s->dir * (long)i, s->y).volume[type] += s->deltas[i];
}


static void step_move_down(Uint8 ordered_type) {
    fluid_type type = ordered[ordered_type];
    initiate_buffer();
    for (Uint32 x = sim_x; x < sim_x + SIMULATION_SPACE_X * FLUIDS_SPLIT_X;
         x++) {
        for (Uint32 y = sim_y + SIMULATION_SPACE_Y * FLUIDS_SPLIT_Y;
             y-- > sim_y;) {
            double vol = BUFFER(x, y).volume[type];
            if (vol < EPSILON || is_wall(x, y) || is_wall(x, y + 1)) continue;

            double space = 1.0f - get_fill(BUFFER(x, y + 1), ordered_type);
            if (space < EPSILON) continue;

            double qty = SDL_min(vol, space);
            BUFFER(x, y).volume[type] -= qty;
            BUFFER(x, y + 1).volume[type] += qty;
        }
    }
    transfer_buffer();
}

static void step_move_diagonally(Uint8 ordered_type) {
    fluid_type type = ordered[ordered_type];
    initiate_buffer();
    for (Uint32 x = sim_x; x < sim_x + SIMULATION_SPACE_X * FLUIDS_SPLIT_X;
         x++) {
        for (Uint32 y = sim_y + SIMULATION_SPACE_Y * FLUIDS_SPLIT_Y;
             y-- > sim_y;) {
            double vol = grid[x][y].volume[type];
            if (vol < EPSILON || is_wall(x, y)) continue;

            neighbor nbs[3][3] = {0};
            get_neighbors(x, y, ordered_type, &nbs);
            if (get_space(nbs BOT) > EPSILON) continue;

            stretch left;
            stretch right;
            get_stretch(x - 1, y + 1, -1, nbs BLEFT.solid, ordered_type, &left);
            get_stretch(
                x + 1, y + 1, 1, nbs BRIGHT.solid, ordered_type, &right);
            double l_share = 0.0f;
            double r_share = 0.0f;
            split_diagonally(vol,
                             get_stretch_space(&left),
                             get_stretch_space(&right),
                             &l_share,
                             &r_share);
            if (is_under_min_spread(vol - l_share - r_share, type)) continue;
            if (l_share > 0.0f && level_stretch(&left, l_share, type)) continue;
            if (r_share > 0.0f && level_stretch(&right, r_share, type))
                continue;

            BUFFER(x, y).volume[type] -= l_share + r_share;
            if (l_share > 0.0f) pour_into_stretch(&left, type);
            if (r_share > 0.0f) pour_into_stretch(&right, type);
        }
    }
    transfer_buffer();
}

static void step_move_sideways(Uint8 ordered_type) {
    fluid_type type = ordered[ordered_type];
    initiate_buffer();
    for (Uint32 x = sim_x; x < sim_x + SIMULATION_SPACE_X * FLUIDS_SPLIT_X;
         x++) {
        for (Uint32 y = sim_y + SIMULATION_SPACE_Y * FLUIDS_SPLIT_Y;
             y-- > sim_y;) {
            double vol = grid[x][y].volume[type];
            if (vol < EPSILON || is_wall(x, y)) continue;

            neighbor nbs[3][3] = {0};
            get_neighbors(x, y, ordered_type, &nbs);
            if (get_space(nbs BOT) > EPSILON ||
                get_space(nbs BLEFT) > EPSILON ||
                get_space(nbs BRIGHT) > EPSILON)
                continue;

            // The cell only gives away its own fluid, so it starts levelling
            // from its fill without it.
            Uint32 xs[3] = {x};
            double base[3] = {nbs SELF.volume - vol};
            Uint32 len = 1;
            if (!nbs LEFT.solid) {
                xs[len] = x - 1;
                base[len++] = nbs LEFT.volume;
            }
            if (!nbs RIGHT.solid) {
                xs[len] = x + 1;
                base[len++] = nbs RIGHT.volume;
            }
            if (len == 1) continue;

            bool active[3] = {0};
            double level = level_fills(base, active, len, vol);
            double given = 0.0f;
            bool too_little = false;
            for (Uint32 i = 1; i < len; i++) {
                if (!active[i]) continue;
                given += level - base[i];
                if (is_under_min_spread(
                        grid[xs[i]][y].volume[type] + level - base[i], type))
                    too_little = true;
            }
            if (too_little || is_under_min_spread(vol - given, type)) continue;

            for (Uint32 i = 1; i < len; i++) {
                if (!active[i]) continue;
                BUFFER(x, y).volume[type] -= level - base[i];
                BUFFER(xs[i], y).volume[type] += level - base[i];
            }
        }
    }
    transfer_buffer();
}

static void step_stabilize_surfaces(Uint8 ordered_type) {
    fluid_type type = ordered[ordered_type];
    initiate_buffer();
    for (Uint32 y = sim_y; y < sim_y + SIMULATION_SPACE_Y * FLUIDS_SPLIT_Y;
         y++) {
        Uint32 x = sim_x;
        while (x < sim_x + SIMULATION_SPACE_X * FLUIDS_SPLIT_X) {
            if (grid[x][y].volume[type] <= EPSILON || !is_surface(x, y, type)) {
                x++;
                continue;
            }

            Uint32 start = x;
            double total = 0.0f;
            double base[SIMULATION_SPACE_X * FLUIDS_SPLIT_X] = {0};
            for (; is_surface(x, y, type); x++) {
                double vol = grid[x][y].volume[type];
                total += vol;
                base[x - start] = get_fill(grid[x][y], ordered_type) - vol;
            }

            Uint32 len = x - start;
            if (len < 2) continue;

            double leak = 2.0f * data[type].min_spread;
            neighbor first[3][3] = {0};
            neighbor last[3][3] = {0};
            get_neighbors(start, y, ordered_type, &first);
            get_neighbors(x - 1, y, ordered_type, &last);
            stretch left;
            stretch right;
            left.len = 0;
            right.len = 0;
            if (get_space(first BOT) < EPSILON &&
                grid[start][y].volume[type] < leak)
                get_stretch(start - 1,
                            y + 1,
                            -1,
                            first BLEFT.solid,
                            ordered_type,
                            &left);
            if (get_space(last BOT) < EPSILON &&
                grid[x - 1][y].volume[type] < leak)
                get_stretch(
                    x, y + 1, 1, last BRIGHT.solid, ordered_type, &right);

            double l_share = 0.0f;
            double r_share = 0.0f;
            split_diagonally(total,
                             get_stretch_space(&left),
                             get_stretch_space(&right),
                             &l_share,
                             &r_share);
            if (l_share > 0.0f) {
                level_stretch(&left, l_share, type);
                pour_into_stretch(&left, type);
            }
            if (r_share > 0.0f) {
                level_stretch(&right, r_share, type);
                pour_into_stretch(&right, type);
            }
            total -= l_share + r_share;

            bool active[SIMULATION_SPACE_X * FLUIDS_SPLIT_X] = {0};
            double level = level_fills(base, active, len, total);
            for (Uint32 i = 0; i < len; i++)
                BUFFER(start + i, y).volume[type] +=
                    (active[i] ? level - base[i] : 0.0f) -
                    grid[start + i][y].volume[type];
        }
    }
    transfer_buffer();
}

static void step_depressurize(Uint8 ordered_type) {
    fluid_type type = ordered[ordered_type];
    initiate_buffer();
    for (Uint32 x = sim_x; x < sim_x + SIMULATION_SPACE_X * FLUIDS_SPLIT_X;
         x++) {
        for (Uint32 y = sim_y; y < sim_y + SIMULATION_SPACE_Y * FLUIDS_SPLIT_Y;
             y++) {
            double vol = grid[x][y].volume[type];
            if (vol < EPSILON || is_wall(x, y)) continue;

            neighbor nbs[3][3] = {0};
            get_neighbors(x, y, ordered_type, &nbs);
            if (nbs SELF.volume < 1.0f + EPSILON) continue;
            double excess = SDL_min(nbs SELF.volume - 1.0f, vol);

            // Fills the sides first, so excess on a surface goes back into it
            // instead of up.
            double l_share = 0.0f;
            double r_share = 0.0f;
            split_diagonally(excess,
                             get_space(nbs LEFT),
                             get_space(nbs RIGHT),
                             &l_share,
                             &r_share);
            BUFFER(x, y).volume[type] -= l_share + r_share;
            if (l_share > 0.0f) BUFFER(x - 1, y).volume[type] += l_share;
            if (r_share > 0.0f) BUFFER(x + 1, y).volume[type] += r_share;
            excess -= l_share + r_share;
            if (excess < EPSILON) continue;

            // Fills empty neighbors or bubbles excess up if no empty neighbors
            // exist
            Uint32 free_x[8] = {0};
            Uint32 free_y[8] = {0};
            Uint8 len = 0;
            for (Uint8 i = 0; i < 3; i++) {
                for (Uint8 j = 0; j < 3; j++) {
                    if (i == 1 && j == 1) continue;
                    if (nbs[i][j].solid || nbs[i][j].volume > EPSILON) continue;
                    free_x[len] = x + i - 1;
                    free_y[len] = y + j - 1;
                    len++;
                }
            }
            if (len > 0) {
                BUFFER(x, y).volume[type] -= excess;
                for (Uint8 n = 0; n < len; n++)
                    BUFFER(free_x[n], free_y[n]).volume[type] += excess / len;
            } else if (!nbs TOP.solid) {
                BUFFER(x, y).volume[type] -= excess;
                BUFFER(x, y - 1).volume[type] += excess;
            }
        }
    }
    transfer_buffer();
}



static void add_queued_particle(void* item) {
    add_fluid_particle(*(fluid_particle*)item);
}

static void simulate_fluid_particles(void) {
    double dt = GAME_MANAGER_get_current_dt();
    size_t i = 0;
    while (i < particles.len) {
        fluid_particle* p = dynarr_at(i, &particles);
        p->vel_y += particles_data.gravity * dt;
        double dx = p->vel_x * dt;
        double dy = p->vel_y * dt;
        Uint32 steps = SDL_ceil(SDL_max(SDL_fabs(dx) / FLUIDS_CELL_W,
                                        SDL_fabs(dy) / FLUIDS_CELL_H));
        if (steps == 0) steps = 1;

        bool landed = false;
        for (Uint32 s = 0; s < steps && !landed; s++) {
            long x = SDL_floor(p->x / FLUIDS_CELL_W);
            long y = SDL_floor(p->y / FLUIDS_CELL_H);
            double next_x = p->x + dx / steps;
            double next_y = p->y + dy / steps;
            long to_x = SDL_floor(next_x / FLUIDS_CELL_W);
            long to_y = SDL_floor(next_y / FLUIDS_CELL_H);

            if (to_x != x || to_y != y) {
                if (is_wall(to_x, to_y)) {
                    if (!is_solid(x, y))
                        grid[x][y].volume[p->type] +=
                            data[p->type].particle_vol;
                    landed = true;
                } else if (has_any_fluid(to_x, to_y)) {
                    grid[to_x][to_y].volume[p->type] +=
                        data[p->type].particle_vol;
                    landed = true;
                }
            }
            if (!landed) {
                p->x = next_x;
                p->y = next_y;
            }
        }

        if (landed)
            remove_fluid_particle(i);
        else
            i++;
    }
}



static bool find_surface_near(long x, long y, long* surface_y) {
    for (long d = 0; d <= FLUIDS_SPLASH_SURFACE_RANGE; d++) {
        if (is_any_surface(x, y - d)) {
            *surface_y = y - d;
            return true;
        }
        if (d > 0 && is_any_surface(x, y + d)) {
            *surface_y = y + d;
            return true;
        }
    }
    return false;
}

static double take_from_cell(long x, long y, double wanted, double* taken) {
    if (is_wall(x, y)) return wanted;
    for (Uint8 t = FLUIDS_TOTAL; t-- > 0;) {
        fluid_type type = ordered[t];
        double qty = SDL_min(grid[x][y].volume[type], wanted);
        grid[x][y].volume[type] -= qty;
        taken[type] += qty;
        wanted -= qty;
    }
    return wanted;
}

// Must be called with `particles_mutex` locked, since it adds particles
// straight into the simulation.
static void apply_splash(void* item) {
    splash sp = *(splash*)item;
    double body_max_speed = particles_data.splash_body_max_speed;
    double strength = SDL_min(sp.speed, body_max_speed) / body_max_speed;
    if (strength < EPSILON || sp.w == 0) return;

    double fan = particles_data.splash_fan_angle * SDL_PI_D / 180.0f;
    double jitter = particles_data.splash_angle_jitter * SDL_PI_D / 180.0f;
    double slice = fan / sp.w;

    for (Uint32 i = 0; i < sp.w; i++) {
        long x = (long)sp.x + i;
        long y = 0;
        // Particles spawn on the cell above the surface, so it can't be a wall
        // or they would be destroyed.
        if (!find_surface_near(x, sp.y, &y) || is_wall(x, y - 1)) continue;

        double taken[FLUIDS_TOTAL] = {0};
        double wanted = take_from_cell(
            x, y, strength * particles_data.splash_volume, taken);
        if (wanted > EPSILON) take_from_cell(x, y + 1, wanted, taken);

        // This column's slice of the fan, pointing up, the leftmost column
        // getting the leftmost slice.
        double from = slice * i - fan / 2;
        for (Uint8 type = 0; type < FLUIDS_TOTAL; type++) {
            double total = taken[type];
            double vol = data[type].particle_vol;
            Uint32 count = vol > EPSILON ? (Uint32)SDL_floor(total / vol) : 0;
            grid[x][y].volume[type] += SDL_max(total - count * vol, 0.0f);

            // Spreads the particles evenly over the slice, each with a random
            // nudge in its direction and a random speed.
            for (Uint32 n = 0; n < count; n++) {
                double angle = from + slice * (n + 0.5f) / count;
                angle += (SDL_randf() * 2.0f - 1.0f) * jitter;
                double speed = SDL_randf() * strength *
                               particles_data.splash_particle_max_speed;
                add_fluid_particle((fluid_particle){
                    .x = (x + 0.5f) * FLUIDS_CELL_W,
                    .y = y * FLUIDS_CELL_H - 1.0f,
                    .vel_x = speed * SDL_sin(angle),
                    .vel_y = -speed * SDL_cos(angle),
                    .type = type,
                });
            }
        }
    }
}

static Uint32 count_due_steps(double* timer, double time, double dt) {
    if (time < EPSILON) return 1;
    *timer += dt;
    Uint32 count = (Uint32)(*timer / time);
    *timer -= count * time;
    return count;
}

static void apply_mod(void* item) {
    mod* m = (mod*)item;
    for (Uint32 x = m->x; x < m->x + m->w; x++) {
        for (Uint32 y = m->y; y < m->y + m->h; y++) {
            if (m->clear)
                grid[x][y].volume[m->type] = 0.0f;
            else
                grid[x][y].volume[m->type] += m->volume;
        }
    }
}

void FLUIDS_update(void) {
    double dt = GAME_MANAGER_get_current_dt();
    update_simulated_rect();
    void (*const moves[3])(Uint8) = {
        step_move_down, step_move_diagonally, step_move_sideways};

    // Denser fluids go first so lighter ones can depressurize from them in the
    // same tick.
    for (Uint8 t = 0; t < FLUIDS_TOTAL; t++) {
        fluid_type type = ordered[t];
        for (Uint8 m = 0; m < 3; m++) {
            Uint32 n = count_due_steps(
                &move_timers[m][type], get_move_time(type, m), dt);
            for (; n > 0; n--)
                moves[m](t);
        }
        step_depressurize(t);
        step_stabilize_surfaces(t);
    }

    // Mods, splashes and landing particles all write on the grid
    SDL_LockMutex(grid_mutex);
    drain_queue(&mods, mods_mutex, apply_mod);

    // Locked so `FLUIDS_draw()` doesn't read the array while it is reallocated.
    SDL_LockMutex(particles_mutex);
    drain_queue(&splashes, splashes_mutex, apply_splash);
    drain_queue(&queued_particles, queued_particles_mutex, add_queued_particle);
    simulate_fluid_particles();
    SDL_UnlockMutex(particles_mutex);
    SDL_UnlockMutex(grid_mutex);

    SDL_AddAtomicInt(&ripples_ticks, 1);
}



void FLUIDS_set_at(
    Uint32 x, Uint32 y, Uint32 w, Uint32 h, fluid_type type, double volume) {
    mod m = {.x = x, .y = y, .w = w, .h = h, .type = type, .volume = volume};
    if (GAME_MANAGER_is_fluids_thread()) {
        SDL_LockMutex(grid_mutex);
        apply_mod(&m);
        SDL_UnlockMutex(grid_mutex);
    } else
        queue_item(&mods, mods_mutex, &m);
}

void FLUIDS_clear_at(Uint32 x, Uint32 y, Uint32 w, Uint32 h, fluid_type type) {
    mod m = {.x = x, .y = y, .w = w, .h = h, .type = type, .clear = true};
    if (GAME_MANAGER_is_fluids_thread()) {
        SDL_LockMutex(grid_mutex);
        apply_mod(&m);
        SDL_UnlockMutex(grid_mutex);
    } else
        queue_item(&mods, mods_mutex, &m);
}

// The volumes are copied with the grid locked, and the tiles are checked only
// after, so it's locked for as little as possible.
void FLUIDS_get_at(
    Uint32 x, Uint32 y, Uint32 w, Uint32 h, fluid_cell (*cells)[w][h]) {
    SDL_LockMutex(grid_mutex);
    for (long i = x; i < x + w; i++) {
        for (long j = y; j < y + h; j++) {
            fluid_cell cell = (fluid_cell){0};
            if (i < TILEMAP_SIZE_X * FLUIDS_SPLIT_X &&
                j < TILEMAP_SIZE_Y * FLUIDS_SPLIT_Y)
                for (Uint8 type = 0; type < FLUIDS_TOTAL; type++)
                    cell.volume[type] = grid[i][j].volume[type];
            (*cells)[i - x][j - y] = cell;
        }
    }
    SDL_UnlockMutex(grid_mutex);
    for (long i = x; i < x + w; i++)
        for (long j = y; j < y + h; j++)
            (*cells)[i - x][j - y].solid =
                i >= TILEMAP_SIZE_X * FLUIDS_SPLIT_X ||
                j >= TILEMAP_SIZE_Y * FLUIDS_SPLIT_Y || is_solid(i, j);
}



static double quarter_px(double vol) {
    return 1.0f + (vol > 0.25f) + (vol > 0.5f) + (vol > 0.75f);
}

// Fluid cells aren't drawn in their colors. Instead, they are drawn into
// `fluids_texture`, owned by `renderer.c`, with colors that encode what each
// pixel is. It covers the ripples area, see `get_ripples_area()`, bigger than
// the screen and centered on it. The part of it on screen is then drawn on
// `base_screen` with the fluids shader, `src/gpu/fluids.frag.hlsl`, which reads
// those codes and decides the actual colors. This lets the shader look at the
// pixels around each one to smooth out edges and add effects, which it couldn't
// do if all it had were colors, especially once they get blended.
//
// Each pixel of `fluids_texture` is encoded as follows:
//
// - Red is the fluid type, `FLUIDS_SHADER_IDX_STEP * (type + 1)`. Air is `0`
//   and walls are `255`, so the shader samples air as `0.0`, walls as `1.0` and
//   fluids as `(type + 1) / 32.0`, which leaves room for 31 types. Its `IDX_*`
//   defines on `src/gpu/includes/fluids-common.hlsli` must match `fluid_type`
//   plus one.
// - Green is the density rank, `FLUIDS_SHADER_IDX_STEP * (FLUIDS_TOTAL - t)`
//   with `t` being the fluid's position on `ordered`, so denser fluids are
//   higher. Air is `0`, the lightest. Walls are `0` as well, but the shader
//   checks for them on red.
// - Blue is unused, always `0`. Alpha is always `255`.
//
// The texture is cleared to transparent black every frame, so anything not
// drawn on is air. Walls are drawn so the shader can tell them apart from air,
// otherwise fluids against walls would be contoured and have their corners cut.
// It uses nearest scaling so the shader samples the exact values drawn.
//
// For each pixel, the shader samples it and its 4 neighbors, then:
//
// 1. If two adjacent neighbors are the same material and lighter than the
//    pixel, while the other two are fluids, the pixel is a corner poking into
//    that material and is painted as it. Corners poking into air are painted as
//    air. Conversely, if two adjacent neighbors are the
//    same fluid and denser than the pixel, while the other two are the same
//    material as the pixel, it's an inner corner of that fluid and is painted
//    as it. Inner corners of air get contoured on step 4.
// 2. Pixels without fluid are painted with `_PreFluids`, everything the
//    renderer drew before the fluids, as is, so the shader covers the whole
//    render target.
// 3. Fluid pixels sample `_PreFluids` offset by the normal of the ripples'
//    height at the pixel, from `_Ripples`, the latest of
//    `fluids_canvas.ripples`, so ripples refract what's under the fluid. See
//    `step_ripples()`.
// 4. It calls the function of the pixel's fluid, like `water()` on
//    `src/gpu/includes/water.hlsli`, with the pixel, the refracted sample, the
//    ripples' normal and its neighbors, which returns its final color. Each
//    fluid gets its base color from the `_Colors` uniform, which
//    `set_shader_uniforms()` fills from the fluid data every frame, blends it
//    with the refracted sample, lights the ripples with their normal and
//    brightens pixels next to air to draw a contour.
//
// Adding a fluid requires a new `IDX_*` define, a new include with its
// function, a call to it on `main()` and increasing the size of `_Colors`.
//
// Particles aren't drawn into `fluids_texture`, they are drawn directly with
// their fluid's color.
//
// Only the cells on `area`, the ripples area, are drawn, relative to its top
// left.
static void draw_grid(SDL_FRect area) {
    Uint32 from_x = SDL_max(SDL_floor(area.x / FLUIDS_CELL_W), 0.0f);
    Uint32 from_y = SDL_max(SDL_floor(area.y / FLUIDS_CELL_H), 0.0f);
    Uint32 to_x = SDL_min(SDL_ceil((area.x + area.w) / FLUIDS_CELL_W),
                          TILEMAP_SIZE_X * FLUIDS_SPLIT_X);
    Uint32 to_y = SDL_min(SDL_ceil((area.y + area.h) / FLUIDS_CELL_H),
                          TILEMAP_SIZE_Y * FLUIDS_SPLIT_Y);
    for (Uint32 x = from_x; x < to_x; x++) {
        for (Uint32 y = from_y; y < to_y; y++) {
            if (is_solid(x, y)) {
                SDL_FRect rect = {x * FLUIDS_CELL_W - area.x,
                                  y * FLUIDS_CELL_H - area.y,
                                  FLUIDS_CELL_W,
                                  FLUIDS_CELL_H};
                GRAPHICS_fill_rect(
                    rect, GRAPHICS_fcolor((SDL_Color){FLUIDS_WALL_COLOR}));
                continue;
            }
            bool empty = !has_any_fluid(x, y);
            double below = 0.0f;
            for (Uint8 t = 0; t < FLUIDS_TOTAL; t++) {
                fluid_type type = ordered[t];
                double vol = grid[x][y].volume[type];
                if (vol < EPSILON && !empty) continue;
                bool top = has_fluid(x, (long)y - 1, type);
                bool bot = has_fluid(x, y + 1, type);

                // Empty cells between two of the same fluid are painted whole,
                // so falls look continuous. Denser fluids take precedence.
                bool gap = vol < EPSILON;
                if (gap && !(top && bot)) continue;
                empty = false;

                // Fluid with more of it on top or falling into nothing fills
                // the cell, otherwise it sits on top of what's below it.
                SDL_FRect rect = {x * FLUIDS_CELL_W - area.x,
                                  y * FLUIDS_CELL_H - area.y,
                                  FLUIDS_CELL_W,
                                  FLUIDS_CELL_H};
                if (!gap &&
                    (below > EPSILON || (!top && (bot || is_wall(x, y + 1))))) {
                    rect.h = quarter_px(vol);
                    rect.y += FLUIDS_CELL_H - rect.h -
                              (below > EPSILON ? quarter_px(below) : 0.0f);
                }
                below += vol;
                SDL_Color col = {FLUIDS_SHADER_IDX_STEP * (type + 1),
                                 FLUIDS_SHADER_IDX_STEP * (FLUIDS_TOTAL - t),
                                 0,
                                 255};
                GRAPHICS_fill_rect(rect, GRAPHICS_fcolor(col));
            }
        }
    }
}

static void draw_particles(SDL_FRect cam) {
    SDL_LockMutex(particles_mutex);
    for (size_t i = 0; i < particles.len; i++) {
        fluid_particle p = *(fluid_particle*)dynarr_at(i, &particles);
        float x = SDL_floor(p.x) - cam.x;
        float y = SDL_floor(p.y) - cam.y;
        if (x + 1.0f < 0.0f || y + 1.0f < 0.0f || x >= cam.w || y >= cam.h)
            continue;
        int n =
            data[p.type].particle_vol < particles_data.size_threshold ? 1 : 4;
        SDL_FPoint points[4] = {
            {x, y}, {x + 1.0f, y}, {x, y + 1.0f}, {x + 1.0f, y + 1.0f}};
        SDL_FColor col = GRAPHICS_fcolor((SDL_Color){data[p.type].col_r,
                                                     data[p.type].col_g,
                                                     data[p.type].col_b,
                                                     data[p.type].col_a});
        for (int j = 0; j < n; j++)
            GRAPHICS_draw_point(points[j].x, points[j].y, col);
    }
    SDL_UnlockMutex(particles_mutex);
}

// Matches the fluids shader's uniforms on
// `src/gpu/includes/fluids-uniforms.hlsli`, padded to a multiple of 16 bytes.
typedef struct {
    // Each fluid's color indexed by type.
    float colors[FLUIDS_TOTAL][4];
    // Camera position in uv units, so world pixels over the screen's size.
    float cam_pos[2];
    // Seconds since the game started.
    float time;
    // How far, in pixels, a fully tilted normal offsets what's seen through
    // the fluid.
    float refraction;
} shader_uniforms;

// Matches the uniforms on `src/gpu/ripples.frag.hlsl`, padded to 16 bytes.
typedef struct {
    // How much of their height ripples keep per tick.
    float damping;
    int pad0;
    int pad1;
    int pad2;
} ripples_uniforms;

static shader_uniforms get_shader_uniforms(SDL_FRect cam) {
    shader_uniforms uniforms = {.cam_pos = {cam.x / cam.w, cam.y / cam.h},
                                .time = SDL_GetTicks() / 1000.0f,
                                .refraction = ripples_refraction};
    for (Uint8 t = 0; t < FLUIDS_TOTAL; t++) {
        uniforms.colors[t][0] = data[t].col_r / 255.0f;
        uniforms.colors[t][1] = data[t].col_g / 255.0f;
        uniforms.colors[t][2] = data[t].col_b / 255.0f;
        uniforms.colors[t][3] = data[t].col_a / 255.0f;
    }
    return uniforms;
}

// Ripples are simulated on two float textures the size of `fluids_texture`,
// `fluids_canvas.ripples`, holding each pixel's current height on red and its
// previous height on green. On the first tick of a draw, the queued ripples
// are added to the current height of the latest one, a square of `size`
// pixels each, so they build on the ripples already there instead of
// flattening them. Then, each tick, the latest is drawn into the other
// with the ripples shader, `src/gpu/ripples.frag.hlsl`, which ticks them and
// makes that one the latest. The shader samples `fluids_texture`, drawn just
// before, so ripples only spread through pixels of the same fluid type,
// bouncing off anything else, and pixels that aren't fluid are flattened.
// Ripples reaching the textures' edges, off screen, are absorbed instead.
//
// Since rendering must happen on the main thread, ripples aren't ticked on
// `FLUIDS_update()`. It counts ticks on `ripples_ticks` instead, which are all
// run here, so ripples run at the fluids thread's rate. When the main thread
// falls behind, ticks over `FLUIDS_RIPPLES_MAX_TICKS` are dropped.
//
// Queued ripples are in world pixels, so they're drawn relative to the top left
// of `area`, the ripples area.
static void step_ripples(fluids_canvas* canvas, SDL_FRect area) {
    int ticks = SDL_SetAtomicInt(&ripples_ticks, 0);
    if (ticks <= 0) return;
    ticks = SDL_min(ticks, FLUIDS_RIPPLES_MAX_TICKS);

    GRAPHICS_set_target(canvas->ripples[ripples_idx]);
    GRAPHICS_set_state(&(graphics_state){.blend = graphics_blend_add});

    // SDL mutexes are recursive, so it stays locked while draining.
    SDL_LockMutex(ripples_mutex);
    for (size_t i = 0; i < ripples.len; i++) {
        ripple* r = dynarr_at(i, &ripples);
        SDL_FRect rect = {r->x - area.x - r->size / 2.0f,
                          r->y - area.y - r->size / 2.0f,
                          r->size,
                          r->size};
        GRAPHICS_fill_rect(rect, (SDL_FColor){r->strength, 0.0f, 0.0f, 1.0f});
    }
    drain_queue(&ripples, ripples_mutex, NULL);
    SDL_UnlockMutex(ripples_mutex);

    GRAPHICS_set_state(&(graphics_state){
        .fragment = canvas->ripples_shader,
        .samplers = {GRAPHICS_binding(canvas->texture)},
        .num_samplers = 1,
        .fragment_uniforms = &(ripples_uniforms){.damping = ripples_damping},
        .fragment_uniforms_size = sizeof(ripples_uniforms),
    });
    for (int i = 0; i < ticks; i++) {
        graphics_texture* from = canvas->ripples[ripples_idx];
        GRAPHICS_set_target(canvas->ripples[1 - ripples_idx]);
        GRAPHICS_draw_texture(
            from, NULL, NULL, SDL_FLIP_NONE, (SDL_FColor){1, 1, 1, 1});
        ripples_idx = 1 - ripples_idx;
    }
    GRAPHICS_set_state(NULL);
}

// Ripples are kept on textures covering `area`, the ripples area, so when it
// moves the latest is drawn into the other shifted the opposite way, making
// that one the latest, so they stay in place in the world. Ripples shifted in
// from outside it start flat.
static void shift_ripples(fluids_canvas* canvas, SDL_FRect area) {
    float dx = area.x - ripples_x;
    float dy = area.y - ripples_y;
    if (dx == 0.0f && dy == 0.0f) return;
    ripples_x = area.x;
    ripples_y = area.y;

    GRAPHICS_set_target(canvas->ripples[1 - ripples_idx]);
    GRAPHICS_clear((SDL_FColor){0, 0, 0, 0});
    SDL_FRect dst = {-dx, -dy, area.w, area.h};
    GRAPHICS_draw_texture(canvas->ripples[ripples_idx],
                          NULL,
                          &dst,
                          SDL_FLIP_NONE,
                          (SDL_FColor){1, 1, 1, 1});
    ripples_idx = 1 - ripples_idx;
}

// Area covered by `fluids_canvas.texture` and `fluids_canvas.ripples`, in world
// pixels, `FLUIDS_RIPPLES_TEXTURE_RES_FACTOR` times the size of the camera at
// `cam` and centered on it. Ripples are simulated on all of it, so they
// aren't cut off at the screen's edges as the camera moves.
static SDL_FRect get_ripples_area(SDL_FRect cam) {
    return (SDL_FRect){
        .x = cam.x - (FLUIDS_RIPPLES_TEXTURE_RES_FACTOR - 1) * cam.w / 2,
        .y = cam.y - (FLUIDS_RIPPLES_TEXTURE_RES_FACTOR - 1) * cam.h / 2,
        .w = FLUIDS_RIPPLES_TEXTURE_RES_FACTOR * cam.w,
        .h = FLUIDS_RIPPLES_TEXTURE_RES_FACTOR * cam.h,
    };
}

void FLUIDS_prepare(fluids_canvas* canvas) {
    prepared_cam = CAMERA_get_rect();
    SDL_FRect area = get_ripples_area(prepared_cam);

    GRAPHICS_set_target(canvas->texture);
    GRAPHICS_clear((SDL_FColor){0, 0, 0, 0});
    draw_grid(area);
    shift_ripples(canvas, area);
    step_ripples(canvas, area);
}

// The fluids shader is opaque and covers the whole target, so it isn't
// blended.
void FLUIDS_draw(fluids_canvas* canvas) {
    SDL_FRect cam = prepared_cam;
    SDL_FRect area = get_ripples_area(cam);

    shader_uniforms uniforms = get_shader_uniforms(cam);
    GRAPHICS_set_state(&(graphics_state){
        .fragment = canvas->shader,
        .samplers = {GRAPHICS_binding(canvas->ripples[ripples_idx]),
                     GRAPHICS_binding(canvas->pre_fluids)},
        .num_samplers = 2,
        .fragment_uniforms = &uniforms,
        .fragment_uniforms_size = sizeof(uniforms),
    });
    SDL_FRect screen = {cam.x - area.x, cam.y - area.y, cam.w, cam.h};
    GRAPHICS_draw_texture(canvas->texture,
                          &screen,
                          NULL,
                          SDL_FLIP_NONE,
                          (SDL_FColor){1, 1, 1, 1});
    GRAPHICS_set_state(NULL);

    draw_particles(cam);
}

// The simulated rect is read once, since the fluids thread may move it midway.
SDL_Rect FLUIDS_get_codes(Uint8* codes) {
    SDL_Rect rect = FLUIDS_get_simulated_rect();
    Uint8 wall = ((SDL_Color){FLUIDS_WALL_COLOR}).r;
    for (int y = 0; y < rect.h; y++) {
        for (int x = 0; x < rect.w; x++) {
            Uint32 cx = rect.x + x, cy = rect.y + y;
            Uint8* code = &codes[y * rect.w + x];
            if (is_solid(cx, cy)) {
                *code = wall;
                continue;
            }
            *code = 0;
            if (get_fill(grid[cx][cy], FLUIDS_TOTAL - 1) <
                FLUIDS_INTERACTION_VOL_THRESHOLD)
                continue;
            for (int t = 0; t < FLUIDS_TOTAL; t++) {
                if (grid[cx][cy].volume[ordered[t]] > EPSILON) {
                    *code = FLUIDS_SHADER_IDX_STEP * (ordered[t] + 1);
                    break;
                }
            }
        }
    }
    return rect;
}

SDL_Rect FLUIDS_get_simulated_rect(void) {
    return (SDL_Rect){
        .x = sim_x,
        .y = sim_y,
        .w = SIMULATION_SPACE_X * FLUIDS_SPLIT_X,
        .h = SIMULATION_SPACE_Y * FLUIDS_SPLIT_Y,
    };
}



// Puts the cell at `(x,y)` of `bodies_buffer` on the body of id `id`, `(x0,y0)`
// being where the buffer is on the grid.
static void
add_cell_to_body(size_t id, Uint32 x, Uint32 y, Uint32 x0, Uint32 y0) {
    fluid_body* b = get_body(id);
    body_ids[b->type][x][y] = id + 1;
    push_to_dynarr(&(fluid_coords){x0 + x, y0 + y}, &b->cells);
}

// Moves every cell of the body of id `from` to the one of id `to`, removing
// `from`, both of the same fluid type. Merge the smaller body into the bigger
// one, so each cell is moved only when its body at least doubles. `(x0,y0)` is
// where `bodies_buffer` is on the grid.
static void merge_bodies(size_t to, size_t from, Uint32 x0, Uint32 y0) {
    fluid_body* src = get_body(from);
    fluid_body* dst = get_body(to);
    for (size_t i = 0; i < src->cells.len; i++) {
        fluid_coords c = *(fluid_coords*)dynarr_at(i, &src->cells);
        body_ids[dst->type][c.x - x0][c.y - y0] = to + 1;
        push_to_dynarr(&c, &dst->cells);
    }
    remove_body(from);
}

// Whether `(x,y)` of `bodies_buffer` is a wall, being solid or outside it, with
// `(x0,y0)` where the buffer is on the grid.
static bool is_body_wall(long x, long y, Uint32 x0, Uint32 y0) {
    return x < 0 || y < 0 || x >= SIMULATION_SPACE_X * FLUIDS_SPLIT_X ||
           y >= SIMULATION_SPACE_Y * FLUIDS_SPLIT_Y || is_solid(x0 + x, y0 + y);
}

// Whether fluid of `ordered_type` has room to move into `(x,y)` of
// `bodies_buffer`, counting only the fluids as dense as it or denser, like the
// moves do.
static bool
has_body_room(long x, long y, Uint8 ordered_type, Uint32 x0, Uint32 y0) {
    return !is_body_wall(x, y, x0, y0) &&
           1.0f - get_fill(bodies_buffer[x][y], ordered_type) >= EPSILON;
}

// The index of `type` on `ordered`, which the moves take.
static Uint8 get_ordered_type(fluid_type type) {
    Uint8 t = 0;
    while (ordered[t] != type)
        t++;
    return t;
}

// Sums up the volumes and moving cells of the body of id `id` and finds the
// rect around its cells, counting only its own fluid type, `(x0,y0)` being
// where `bodies_buffer` is on the grid. A cell moving down also counts every
// cell of the body right over it, since they fall along with it. Those are the
// cells of its type right over it, so it works on compacted bodies, whose ids
// no longer match `body_ids`. A cell that can't move down moves diagonally if
// the cell down left or down right has room, unless it's blocked off by walls
// on both its sides, like the diagonal move does. That's a rougher check than
// the move's, which also pours into the surfaces from there and holds back
// fluid too thin to spread. `moving_cells` stops at its max instead of wrapping
// around.
static void measure_body(size_t id, Uint32 x0, Uint32 y0) {
    fluid_body* b = get_body(id);
    Uint8 ordered_type = get_ordered_type(b->type);
    fluid_coords first = *(fluid_coords*)dynarr_at(0, &b->cells);
    Uint32 min_x = first.x;
    Uint32 min_y = first.y;
    Uint32 max_x = first.x;
    Uint32 max_y = first.y;
    Uint32 moving = 0;
    for (size_t i = 0; i < b->cells.len; i++) {
        fluid_coords c = *(fluid_coords*)dynarr_at(i, &b->cells);
        Uint32 x = c.x - x0;
        Uint32 y = c.y - y0;
        b->total_volume += bodies_buffer[x][y].volume[b->type];
        min_x = SDL_min(min_x, c.x);
        min_y = SDL_min(min_y, c.y);
        max_x = SDL_max(max_x, c.x);
        max_y = SDL_max(max_y, c.y);
        if (has_body_room(x, y + 1, ordered_type, x0, y0)) {
            moving++;
            for (Uint32 above = y; above-- > 0 && body_ids[b->type][x][above];)
                moving++;
            continue;
        }
        bool bot_wall = is_body_wall(x, y + 1, x0, y0);
        bool left = has_body_room(x - 1, y + 1, ordered_type, x0, y0) &&
                    !(bot_wall && is_body_wall(x - 1, y, x0, y0));
        bool right = has_body_room(x + 1, y + 1, ordered_type, x0, y0) &&
                     !(bot_wall && is_body_wall(x + 1, y, x0, y0));
        if (left || right) moving++;
    }
    b->rect = (SDL_Rect){
        .x = min_x,
        .y = min_y,
        .w = max_x - min_x + 1,
        .h = max_y - min_y + 1,
    };
    b->moving_cells = SDL_min(moving, SDL_MAX_UINT16);
}

// How loud the tracks of `b` should be from where it is. It's at full volume
// while its rect intersects `cam`, going down linearly with the distance
// between them and reaching silence `falloff_range` pixels away.
static double get_body_distance_volume(const fluid_body* b, SDL_FRect cam) {
    double l = (double)b->rect.x * FLUIDS_CELL_W;
    double t = (double)b->rect.y * FLUIDS_CELL_H;
    double r = l + (double)b->rect.w * FLUIDS_CELL_W;
    double bt = t + (double)b->rect.h * FLUIDS_CELL_H;
    double dx = SDL_max(SDL_max(cam.x - r, l - (cam.x + cam.w)), 0.0);
    double dy = SDL_max(SDL_max(cam.y - bt, t - (cam.y + cam.h)), 0.0);
    return SDL_max(1.0 - SDL_sqrt(dx * dx + dy * dy) / falloff_range, 0.0);
}

// How much of `full` `amount` is, capped at `1`, or `1` if `full` is `0`.
static double get_body_scale(double amount, double full) {
    return full > 0.0 ? SDL_min(amount / full, 1.0) : 1.0;
}

// Fades `track` to `volume` times `max_vol`, so jumps in it are smoothed out,
// playing it from silence if it isn't playing and it isn't silent. It fades at
// `FLUIDS_BODIES_BGS_FADE_RATE` times `max_vol` too, so going from silence to
// `max_vol` takes the same time whatever `max_vol` is. Whether it stops once
// silent is up to the track. Does nothing for `bgs_track_none`.
static void play_body_track(bgs_track track, float volume, float max_vol) {
    if (track == bgs_track_none) return;
    volume *= max_vol;
    if (volume > 0.0f && !BGS_is_track_playing(track)) {
        BGS_set_track_volume(track, 0.0f);
        BGS_play_track(track);
    }
    BGS_fade_track(track, volume, FLUIDS_BODIES_BGS_FADE_RATE * max_vol);
}

// Plays each fluid type's tracks at the volume of its loudest body for each of
// them, scaled by distance and then by total volume for the static track and
// by moving cells for the running one.
static void play_bodies_sfx(void) {
    SDL_FRect cam = CAMERA_get_rect();
    float static_volumes[FLUIDS_TOTAL] = {0};
    float running_volumes[FLUIDS_TOTAL] = {0};
    for (size_t id = 0; id < bodies.len; id++) {
        fluid_body* b = &bodies.items[id];
        const fluid_audio* a = &audio[b->type];
        double dist = get_body_distance_volume(b, cam);
        float stat = (float)(dist * get_body_scale(b->total_volume,
                                                   a->static_max_vol_at));
        float run = (float)(dist * get_body_scale(b->moving_cells,
                                                  a->running_max_vol_at));
        static_volumes[b->type] = SDL_max(static_volumes[b->type], stat);
        running_volumes[b->type] = SDL_max(running_volumes[b->type], run);
    }
    for (fluid_type type = 0; type < FLUIDS_TOTAL; type++) {
        play_body_track(body_bgs[type].static_bgs,
                        static_volumes[type],
                        audio[type].static_max_vol);
        play_body_track(body_bgs[type].running_bgs,
                        running_volumes[type],
                        audio[type].running_max_vol);
    }
}

// Finds the bodies of fluid on the simulated rect, replacing the ones found
// last time, measures them and plays their tracks. Cells with the same fluid
// type connected by their sides are on the same body. The bodies aren't locked,
// so it's only called from one thread. The simulated rect is read once, since
// the fluids thread may move it midway. Each fluid type has its own bodies, so
// a cell with more than one type is on one body of each. Cells are checked top
// to bottom, left to right, so their left and top neighbors were already
// checked: a cell joins their body of the same type, merging the smaller one
// into the bigger if they're on different ones, or starts a new body if neither
// has one. The bodies are compacted once they're all found, so measuring them
// and playing their tracks don't skip over removed ones.
void FLUIDS_play_bgs(void) {
    SDL_LockMutex(grid_mutex);
    Uint32 x0 = sim_x;
    Uint32 y0 = sim_y;
    for (Uint32 x = 0; x < SIMULATION_SPACE_X * FLUIDS_SPLIT_X; x++)
        SDL_memcpy(
            bodies_buffer[x], &grid[x0 + x][y0], sizeof(bodies_buffer[x]));
    SDL_UnlockMutex(grid_mutex);
    SDL_memset(body_ids, 0, sizeof(body_ids));
    clear_bodies();

    for (Uint32 y = 0; y < SIMULATION_SPACE_Y * FLUIDS_SPLIT_Y; y++) {
        for (Uint32 x = 0; x < SIMULATION_SPACE_X * FLUIDS_SPLIT_X; x++) {
            if (is_solid(x0 + x, y0 + y)) continue;
            for (fluid_type type = 0; type < FLUIDS_TOTAL; type++) {
                if (bodies_buffer[x][y].volume[type] <= EPSILON) continue;
                size_t left = x > 0 ? body_ids[type][x - 1][y] : 0;
                size_t top = y > 0 ? body_ids[type][x][y - 1] : 0;
                size_t id;
                if (!left && !top) {
                    id = add_body(type);
                } else if (left && top && left != top) {
                    size_t a = left - 1;
                    size_t b = top - 1;
                    if (get_body(a)->cells.len < get_body(b)->cells.len) {
                        a = top - 1;
                        b = left - 1;
                    }
                    merge_bodies(a, b, x0, y0);
                    id = a;
                } else {
                    id = (left ? left : top) - 1;
                }
                add_cell_to_body(id, x, y, x0, y0);
            }
        }
    }
    compact_bodies();
    for (size_t id = 0; id < bodies.len; id++)
        measure_body(id, x0, y0);
    play_bodies_sfx();
}



int FLUIDS_add_particle(
    fluid_type type, double x, double y, double vel_x, double vel_y) {
    long cell_x = SDL_floor(x / FLUIDS_CELL_W);
    long cell_y = SDL_floor(y / FLUIDS_CELL_H);
    if (is_outside(cell_x, cell_y)) return 2;
    if (is_solid(cell_x, cell_y)) return 1;
    fluid_particle p = {
        .x = x, .y = y, .vel_x = vel_x, .vel_y = vel_y, .type = type};
    queue_item(&queued_particles, queued_particles_mutex, &p);
    return 0;
}

static void play_sfx_bank(fluid_sfx_bank* bank,
                          fluid_type type,
                          double x,
                          double y,
                          float max_volume) {
    if (!bank->counts[type]) return;
    SDL_LockMutex(bank->mutex);
    Uint64 now = SDL_GetTicksNS();
    if (bank->played &&
        now - bank->last_ns < (Uint64)(bank->cooldown * SDL_NS_PER_SECOND)) {
        SDL_UnlockMutex(bank->mutex);
        return;
    }
    if (!bank->voices_created) {
        // Without SFX started, there's nothing to play them on
        for (int i = 0; i < FLUID_SFX_VOICES_TOTAL; i++) {
            if (SFX_new_voice(audio_bus_gameplay_sounds, &bank->voices[i])) {
                SDL_UnlockMutex(bank->mutex);
                return;
            }
        }
        bank->voices_created = true;
    }
    SFX_play_voice_2d(bank->voices[bank->next_voice],
                      bank->wavs[type][SDL_rand(bank->counts[type])],
                      x,
                      y,
                      max_volume,
                      falloff_range);
    bank->next_voice = (bank->next_voice + 1) % FLUID_SFX_VOICES_TOTAL;
    bank->last_ns = now;
    bank->played = true;
    SDL_UnlockMutex(bank->mutex);
}

// Fills `type` with the fluid with the most volume on the deep columns of the
// `w` from `(x,y)` to its right, returning whether there are any. A column is
// deep when it has at least a tile's worth of fluid, `FLUIDS_SPLIT_Y` cells in
// a row with `FLUIDS_INTERACTION_VOL_THRESHOLD` volume or more each, starting
// within `FLUIDS_SPLASH_SURFACE_RANGE` cells under `y`. Cells under the
// threshold over it are skipped, since a surface cell may be only a thin film,
// but not under it, so thin puddles and blobs of fluid falling through the air
// don't count however tall they are.
static bool get_splash_fluid(Uint32 x, Uint32 y, Uint32 w, fluid_type* type) {
    if (w == 0) return false;
    const Uint32 h = FLUIDS_SPLASH_SURFACE_RANGE + FLUIDS_SPLIT_Y;
    fluid_cell cells[w][h];
    FLUIDS_get_at(x, y, w, h, &cells);
    double volumes[FLUIDS_TOTAL] = {0};
    bool deep = false;
    for (Uint32 i = 0; i < w; i++) {
        double column[FLUIDS_TOTAL] = {0};
        Uint32 run = 0;
        for (Uint32 j = 0; j < h && run < FLUIDS_SPLIT_Y; j++) {
            if (cells[i][j].solid) break;
            double total = 0.0;
            for (int t = 0; t < FLUIDS_TOTAL; t++)
                total += cells[i][j].volume[t];
            if (total < FLUIDS_INTERACTION_VOL_THRESHOLD) {
                if (run > 0) break;
                continue;
            }
            run++;
            for (int t = 0; t < FLUIDS_TOTAL; t++)
                column[t] += cells[i][j].volume[t];
        }
        if (run < FLUIDS_SPLIT_Y) continue;
        deep = true;
        for (int t = 0; t < FLUIDS_TOTAL; t++)
            volumes[t] += column[t];
    }
    if (!deep) return false;
    *type = 0;
    for (int t = 1; t < FLUIDS_TOTAL; t++)
        if (volumes[t] > volumes[*type]) *type = t;
    return true;
}

void FLUIDS_splash_at(Uint32 x, Uint32 y, Uint32 w, double speed) {
    splash sp = {.x = x, .y = y, .w = w, .speed = speed};
    queue_item(&splashes, splashes_mutex, &sp);
    fluid_type type;
    if (get_splash_fluid(x, y, w, &type))
        play_sfx_bank(&splash_sfx,
                      type,
                      (x + w / 2.0) * FLUIDS_CELL_W,
                      (double)y * FLUIDS_CELL_H,
                      audio[type].splash_vol);
}

static bool
get_ripple_fluid(Uint32 x, Uint32 y, Uint32 size, fluid_type* type) {
    long half = size / 2;
    long l = SDL_max((long)x - half, 0) / FLUIDS_CELL_W;
    long t = SDL_max((long)y - half, 0) / FLUIDS_CELL_H;
    long r = ((long)x - half + (long)SDL_max(size, 1) - 1) / FLUIDS_CELL_W;
    long b = ((long)y - half + (long)SDL_max(size, 1) - 1) / FLUIDS_CELL_H;
    long w = SDL_max(r - l + 1, 1);
    long h = SDL_max(b - t + 1, 1);
    fluid_cell cells[w][h];
    FLUIDS_get_at(l, t, w, h, &cells);
    double volumes[FLUIDS_TOTAL] = {0};
    double total = 0.0;
    for (long i = 0; i < w; i++) {
        for (long j = 0; j < h; j++) {
            if (cells[i][j].solid) continue;
            for (int f = 0; f < FLUIDS_TOTAL; f++) {
                volumes[f] += cells[i][j].volume[f];
                total += cells[i][j].volume[f];
            }
        }
    }
    if (total <= EPSILON) return false;
    *type = 0;
    for (int f = 1; f < FLUIDS_TOTAL; f++)
        if (volumes[f] > volumes[*type]) *type = f;
    return true;
}

static bool is_ripple_deep(Uint32 x, Uint32 y) {
    fluid_cell cells[1][2];
    FLUIDS_get_at(x / FLUIDS_CELL_W, y / FLUIDS_CELL_H, 1, 2, &cells);
    double total = 0.0;
    for (int j = 0; j < 2; j++) {
        if (cells[0][j].solid) continue;
        for (int t = 0; t < FLUIDS_TOTAL; t++)
            total += cells[0][j].volume[t];
    }
    return total >= FLUIDS_RIPPLE_SFX_VOL_THRESHOLD;
}

void FLUIDS_ripple_at(Uint32 x, Uint32 y, Uint32 size, double strength) {
    ripple r = {.x = x, .y = y, .size = size, .strength = strength};
    queue_item(&ripples, ripples_mutex, &r);
    fluid_type type;
    if (strength > 0.0 && is_ripple_deep(x, y) &&
        get_ripple_fluid(x, y, size, &type))
        play_sfx_bank(
            &slosh_sfx, type, x, y, (float)strength * audio[type].ripple_vol);
}



hashmap FLUIDS_get_string_fluid_types_map(void) {
    hashmap map;
    init_hashmap(&map, sizeof(fluid_type));
    set_on_hashmap(&(fluid_type){fluid_water}, "water", &map);
    set_on_hashmap(&(fluid_type){fluid_lava}, "lava", &map);
    return map;
}
