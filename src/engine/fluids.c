#include "engine/fluids.h"
#include "data-structs/dynarr.h"
#include "data-structs/hashmap.h"
#include "defs.h"
#include "engine/game-manager.h"
#include "engine/tilemap.h"
#include "services/csv.h"
#include <SDL3/SDL.h>

#ifdef TEST
#include "stubs.h"
#endif



typedef struct {
    double density;
    double move_down_time;
    double move_diag_time;
    double move_side_time;
    double min_spread;
    double particle_vol;
    Uint8 col_r;
    Uint8 col_g;
    Uint8 col_b;
    Uint8 col_a;
} fluid_data;

typedef struct {
    double volume[FLUIDS_TOTAL];
} fluids;

typedef struct {
    double x;
    double y;
    double vel_x;
    double vel_y;
    fluid_type type;
} fluid_particle;

typedef struct {
    double volume;
    bool solid;
} neighbor;

typedef struct {
    Uint32 x;
    Uint32 y;
    Uint32 w;
    Uint32 h;
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
    Uint32 x;
    Uint32 y;
    Uint32 w;
    double speed;
} splash;

typedef struct {
    Uint32 x;
    Uint32 y;
    double strength;
} ripple;

typedef struct {
    long x;
    long y;
    long dir;
    Uint32 len;
    double fills[FLUIDS_SIMULATE_X * FLUIDS_SPLIT_X];
    double deltas[FLUIDS_SIMULATE_X * FLUIDS_SPLIT_X];
} stretch;



static fluids grid[TILEMAP_SIZE_X * FLUIDS_SPLIT_X]
                  [TILEMAP_SIZE_Y * FLUIDS_SPLIT_Y] = {0};
static fluids buffer[FLUIDS_SIMULATE_X * FLUIDS_SPLIT_X]
                    [FLUIDS_SIMULATE_Y * FLUIDS_SPLIT_Y] = {0};
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
// Which of `fluids_canvas.ripples` has the latest ripples.
static Uint8 ripples_idx = 0;
// Ripples ticks due, added by `FLUIDS_step()` and run by `FLUIDS_draw()`.
static SDL_AtomicInt ripples_ticks = {0};
static fluid_type ordered[FLUIDS_TOTAL] = {0};

static fluid_particle* particles = NULL;
static size_t particles_cap = 0;
static size_t particles_len = 0;
static SDL_Mutex* particles_mutex = NULL;

// Down, diagonal and sideways move timers, in that order.
static double move_timers[3][FLUIDS_TOTAL] = {0};

static void double_particles_dynarr(void) {
    fluid_particle* new_particles =
        SDL_calloc(particles_cap << 1, sizeof(fluid_particle));
    SDL_memcpy(
        new_particles, particles, particles_cap * sizeof(fluid_particle));
    SDL_free(particles);
    particles_cap = particles_cap << 1;
    particles = new_particles;
}

// We treat these differently from sprites and bodies because these particles
// are not exposed, their indexes are not IDs that other code should keep track
// of so we can move them around the array safely.
static void remove_fluid_particle(size_t index) {
    particles_len--;
    particles[index] = particles[particles_len];
}

static void add_fluid_particle(fluid_particle particle) {
    if (particles_len >= particles_cap) double_particles_dynarr();
    particles[particles_len] = particle;
    particles_len++;
}

static void
queue_item(dynarr* queue, SDL_Mutex* mutex, const void* item, size_t size) {
    void* copy = SDL_malloc(size);
    SDL_memcpy(copy, item, size);
    SDL_LockMutex(mutex);
    push_to_dynarr(copy, queue);
    SDL_UnlockMutex(mutex);
}

// Applies the queued items in the order they were queued, if `apply` isn't
// `NULL`, and frees them.
static void drain_queue(dynarr* queue, SDL_Mutex* mutex, void (*apply)(void*)) {
    SDL_LockMutex(mutex);
    for (size_t i = 0; i < queue->len; i++) {
        if (apply) apply(queue->at[i]);
        SDL_free(queue->at[i]);
    }
    while (queue->len > 0)
        remove_at_from_dynarr(queue->len - 1, queue);
    SDL_UnlockMutex(mutex);
}

static void clear_queue(dynarr* queue, SDL_Mutex** mutex) {
    drain_queue(queue, *mutex, NULL);
    clear_dynarr(queue);
    SDL_DestroyMutex(*mutex);
    *mutex = NULL;
}



static double read_prop(dynarr* row, char* prop, hashmap* props) {
    return SDL_strtod((char*)row->at[(size_t)get_from_hashmap(prop, props)],
                      NULL);
}

static void load_fluid_data(void) {
#ifndef TEST
    char csv[] = {
#embed FLUIDS_CSV_PATH suffix(, '\0')
    };
#else
    char csv[] = {
#embed "tests/mocks/fluids.csv" suffix(, '\0')
    };
#endif

    dynarr parsed = CSV_parse(csv);

    // Get hashmap of property indexes
    hashmap props = (hashmap){0};
    int props_count = ((dynarr*)parsed.at[0])->len - 1;
    init_hashmap(&props);
    for (size_t i = 1; i <= props_count; i++)
        set_on_hashmap((void*)i, (char*)((dynarr*)parsed.at[0])->at[i], &props);

    // Get hashmap of fluid names
    hashmap names = (hashmap){0};
    init_hashmap(&names);
    set_on_hashmap((void*)fluid_water, "water", &names);
    set_on_hashmap((void*)fluid_lava, "lava", &names);

    // Sets fluids data
    for (size_t i = 1; i <= FLUIDS_TOTAL; i++) {
        dynarr* row = (dynarr*)parsed.at[i];
        char* name = (char*)row->at[0];
        data[(size_t)get_from_hashmap(name, &names)] = (fluid_data){
            .density = read_prop(row, "density", &props),
            .move_down_time = read_prop(row, "move_down_time", &props),
            .move_diag_time = read_prop(row, "move_diag_time", &props),
            .move_side_time = read_prop(row, "move_side_time", &props),
            .min_spread = read_prop(row, "min_spread", &props),
            .particle_vol = read_prop(row, "particle_vol", &props),
            .col_r = read_prop(row, "col_r", &props),
            .col_g = read_prop(row, "col_g", &props),
            .col_b = read_prop(row, "col_b", &props),
            .col_a = read_prop(row, "col_a", &props),
        };
    }

    clear_hashmap(&names);
    clear_hashmap(&props);
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

static void load_fluid_particles_data(void) {
#ifndef TEST
    char csv[] = {
#embed FLUID_PARTICLES_CSV_PATH suffix(, '\0')
    };
#else
    char csv[] = {
#embed "tests/mocks/fluid-particles.csv" suffix(, '\0')
    };
#endif

    dynarr parsed = CSV_parse(csv);

    for (int i = 0; i < parsed.len; i++) {
        if (((dynarr*)parsed.at[i])->len < 2) continue;
        char* key = (char*)((dynarr*)parsed.at[i])->at[0];
        char* val = (char*)((dynarr*)parsed.at[i])->at[1];
        if (SDL_strcmp(key, "gravity") == 0)
            particles_data.gravity = SDL_strtod(val, NULL);
        else if (SDL_strcmp(key, "size_threshold") == 0)
            particles_data.size_threshold = SDL_strtod(val, NULL);
        else if (SDL_strcmp(key, "fan_angle") == 0)
            particles_data.splash_fan_angle = SDL_strtod(val, NULL);
        else if (SDL_strcmp(key, "angle_jitter") == 0)
            particles_data.splash_angle_jitter = SDL_strtod(val, NULL);
        else if (SDL_strcmp(key, "particle_max_speed") == 0)
            particles_data.splash_particle_max_speed = SDL_strtod(val, NULL);
        else if (SDL_strcmp(key, "volume") == 0)
            particles_data.splash_volume = SDL_strtod(val, NULL);
        else if (SDL_strcmp(key, "body_max_speed") == 0)
            particles_data.splash_body_max_speed = SDL_strtod(val, NULL);
    }

    CSV_free(&parsed);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Fluid particles data loaded successfully.");
}

void FLUIDS_start(void) {
    load_fluid_data();
    load_fluid_particles_data();
    order_fluids_by_density();
    stagger_move_timers();
    init_dynarr(&mods);
    mods_mutex = SDL_CreateMutex();
    init_dynarr(&queued_particles);
    queued_particles_mutex = SDL_CreateMutex();
    init_dynarr(&splashes);
    splashes_mutex = SDL_CreateMutex();
    init_dynarr(&ripples);
    ripples_mutex = SDL_CreateMutex();
    ripples_idx = 0;
    SDL_SetAtomicInt(&ripples_ticks, 0);
    particles = SDL_calloc(STARTING_FLUIDS_PARTICLES_DYNARR_CAP,
                           sizeof(fluid_particle));
    particles_cap = STARTING_FLUIDS_PARTICLES_DYNARR_CAP;
    particles_len = 0;
    particles_mutex = SDL_CreateMutex();
}

void FLUIDS_stop(void) {
    clear_queue(&mods, &mods_mutex);
    clear_queue(&queued_particles, &queued_particles_mutex);
    clear_queue(&splashes, &splashes_mutex);
    clear_queue(&ripples, &ripples_mutex);
    SDL_memset(grid, 0, sizeof(grid));
    SDL_memset(move_timers, 0, sizeof(move_timers));
    SDL_free(particles);
    particles = NULL;
    particles_cap = 0;
    particles_len = 0;
    SDL_DestroyMutex(particles_mutex);
    particles_mutex = NULL;
}



static bool is_solid(Uint32 x, Uint32 y) {
    tile data = (tile){0};
    TILEMAP_get_at(x / FLUIDS_SPLIT_X, y / FLUIDS_SPLIT_Y, &data);
    return data.mat;
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

static void initiate_buffer(void) {
    for (Uint32 x = 0; x < FLUIDS_SIMULATE_X * FLUIDS_SPLIT_X; x++)
        SDL_memcpy(buffer[x], grid[x], sizeof(buffer[x]));
}

static void transfer_buffer(void) {
    for (Uint32 x = 0; x < FLUIDS_SIMULATE_X * FLUIDS_SPLIT_X; x++)
        SDL_memcpy(grid[x], buffer[x], sizeof(buffer[x]));
}



static bool is_outside(long x, long y) {
    return x < 0 || y < 0 || x >= FLUIDS_SIMULATE_X * FLUIDS_SPLIT_X ||
           y >= FLUIDS_SIMULATE_Y * FLUIDS_SPLIT_Y;
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
    bool active[FLUIDS_SIMULATE_X * FLUIDS_SPLIT_X] = {0};
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
        buffer[s->x + s->dir * (long)i][s->y].volume[type] += s->deltas[i];
}


static void step_move_down(Uint8 ordered_type) {
    fluid_type type = ordered[ordered_type];
    initiate_buffer();
    for (Uint32 x = 0; x < FLUIDS_SIMULATE_X * FLUIDS_SPLIT_X; x++) {
        for (Uint32 y = FLUIDS_SIMULATE_Y * FLUIDS_SPLIT_Y; y-- > 0;) {
            double vol = buffer[x][y].volume[type];
            if (vol < EPSILON || is_wall(x, y) || is_wall(x, y + 1)) continue;

            double space = 1.0f - get_fill(buffer[x][y + 1], ordered_type);
            if (space < EPSILON) continue;

            double qty = SDL_min(vol, space);
            buffer[x][y].volume[type] -= qty;
            buffer[x][y + 1].volume[type] += qty;
        }
    }
    transfer_buffer();
}

static void step_move_diagonally(Uint8 ordered_type) {
    fluid_type type = ordered[ordered_type];
    initiate_buffer();
    for (Uint32 x = 0; x < FLUIDS_SIMULATE_X * FLUIDS_SPLIT_X; x++) {
        for (Uint32 y = FLUIDS_SIMULATE_Y * FLUIDS_SPLIT_Y; y-- > 0;) {
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

            buffer[x][y].volume[type] -= l_share + r_share;
            if (l_share > 0.0f) pour_into_stretch(&left, type);
            if (r_share > 0.0f) pour_into_stretch(&right, type);
        }
    }
    transfer_buffer();
}

static void step_move_sideways(Uint8 ordered_type) {
    fluid_type type = ordered[ordered_type];
    initiate_buffer();
    for (Uint32 x = 0; x < FLUIDS_SIMULATE_X * FLUIDS_SPLIT_X; x++) {
        for (Uint32 y = FLUIDS_SIMULATE_Y * FLUIDS_SPLIT_Y; y-- > 0;) {
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
                buffer[x][y].volume[type] -= level - base[i];
                buffer[xs[i]][y].volume[type] += level - base[i];
            }
        }
    }
    transfer_buffer();
}

static void step_stabilize_surfaces(Uint8 ordered_type) {
    fluid_type type = ordered[ordered_type];
    initiate_buffer();
    for (Uint32 y = 0; y < FLUIDS_SIMULATE_Y * FLUIDS_SPLIT_Y; y++) {
        Uint32 x = 0;
        while (x < FLUIDS_SIMULATE_X * FLUIDS_SPLIT_X) {
            if (grid[x][y].volume[type] <= EPSILON || !is_surface(x, y, type)) {
                x++;
                continue;
            }

            Uint32 start = x;
            double total = 0.0f;
            double base[FLUIDS_SIMULATE_X * FLUIDS_SPLIT_X] = {0};
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

            bool active[FLUIDS_SIMULATE_X * FLUIDS_SPLIT_X] = {0};
            double level = level_fills(base, active, len, total);
            for (Uint32 i = 0; i < len; i++)
                buffer[start + i][y].volume[type] +=
                    (active[i] ? level - base[i] : 0.0f) -
                    grid[start + i][y].volume[type];
        }
    }
    transfer_buffer();
}

static void step_depressurize(Uint8 ordered_type) {
    fluid_type type = ordered[ordered_type];
    initiate_buffer();
    for (Uint32 x = 0; x < FLUIDS_SIMULATE_X * FLUIDS_SPLIT_X; x++) {
        for (Uint32 y = 0; y < FLUIDS_SIMULATE_Y * FLUIDS_SPLIT_Y; y++) {
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
            buffer[x][y].volume[type] -= l_share + r_share;
            if (l_share > 0.0f) buffer[x - 1][y].volume[type] += l_share;
            if (r_share > 0.0f) buffer[x + 1][y].volume[type] += r_share;
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
                buffer[x][y].volume[type] -= excess;
                for (Uint8 n = 0; n < len; n++)
                    buffer[free_x[n]][free_y[n]].volume[type] += excess / len;
            } else if (!nbs TOP.solid) {
                buffer[x][y].volume[type] -= excess;
                buffer[x][y - 1].volume[type] += excess;
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
    while (i < particles_len) {
        fluid_particle* p = &particles[i];
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
                    if (!is_wall(x, y))
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
    for (long d = 0; d <= FLUID_SPLASH_SURFACE_RANGE; d++) {
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

void FLUIDS_step(void) {
    double dt = GAME_MANAGER_get_current_dt();
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

    drain_queue(&mods, mods_mutex, apply_mod);

    // Locked so `FLUIDS_draw()` doesn't read the array while it is reallocated.
    SDL_LockMutex(particles_mutex);
    drain_queue(&splashes, splashes_mutex, apply_splash);
    drain_queue(&queued_particles, queued_particles_mutex, add_queued_particle);
    simulate_fluid_particles();
    SDL_UnlockMutex(particles_mutex);

    SDL_AddAtomicInt(&ripples_ticks, 1);
}



void FLUIDS_set_at(
    Uint32 x, Uint32 y, Uint32 w, Uint32 h, fluid_type type, double volume) {
    mod m = {.x = x, .y = y, .w = w, .h = h, .type = type, .volume = volume};
    queue_item(&mods, mods_mutex, &m, sizeof(mod));
}

void FLUIDS_clear_at(Uint32 x, Uint32 y, Uint32 w, Uint32 h, fluid_type type) {
    mod m = {.x = x, .y = y, .w = w, .h = h, .type = type, .clear = true};
    queue_item(&mods, mods_mutex, &m, sizeof(mod));
}

void FLUIDS_get_at(
    Uint32 x, Uint32 y, Uint32 w, Uint32 h, fluid_cell (*cells)[w][h]) {
    for (long i = x; i < x + w; i++) {
        for (long j = y; j < y + h; j++) {
            if (i >= TILEMAP_SIZE_X * FLUIDS_SPLIT_X ||
                j >= TILEMAP_SIZE_Y * FLUIDS_SPLIT_Y) {
                (*cells)[i - x][j - y] = (fluid_cell){.solid = true};
                continue;
            }
            fluid_cell cell = (fluid_cell){0};
            cell.solid = is_solid(i, j);
            for (Uint8 type = 0; type < FLUIDS_TOTAL; type++)
                cell.volume[type] = grid[i][j].volume[type];
            (*cells)[i - x][j - y] = cell;
        }
    }
}



static double quarter_px(double vol) {
    return 1.0f + (vol > 0.25f) + (vol > 0.5f) + (vol > 0.75f);
}

// Fluid cells aren't drawn in their colors. Instead, they are drawn into
// `fluids_texture`, owned by `renderer.c` and the size of `base_screen`, with
// colors that encode what each pixel is. That texture is then drawn on
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
static void draw_grid(SDL_Renderer* renderer) {
    for (Uint32 x = 0; x < FLUIDS_SIMULATE_X * FLUIDS_SPLIT_X; x++) {
        for (Uint32 y = 0; y < FLUIDS_SIMULATE_Y * FLUIDS_SPLIT_Y; y++) {
            if (is_solid(x, y)) {
                SDL_FRect rect = {x * FLUIDS_CELL_W,
                                  y * FLUIDS_CELL_H,
                                  FLUIDS_CELL_W,
                                  FLUIDS_CELL_H};
#ifndef TEST
                SDL_SetRenderDrawColor(renderer, FLUIDS_WALL_COLOR);
                SDL_RenderFillRect(renderer, &rect);
#else
                stubbed_SDL_RenderFillRect(&rect);
#endif
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
                SDL_FRect rect = {x * FLUIDS_CELL_W,
                                  y * FLUIDS_CELL_H,
                                  FLUIDS_CELL_W,
                                  FLUIDS_CELL_H};
                if (!gap &&
                    (below > EPSILON || (!top && (bot || is_wall(x, y + 1))))) {
                    rect.h = quarter_px(vol);
                    rect.y += FLUIDS_CELL_H - rect.h -
                              (below > EPSILON ? quarter_px(below) : 0.0f);
                }
                below += vol;
#ifndef TEST
                SDL_SetRenderDrawColor(renderer,
                                       FLUIDS_SHADER_IDX_STEP * (type + 1),
                                       FLUIDS_SHADER_IDX_STEP *
                                           (FLUIDS_TOTAL - t),
                                       0,
                                       255);
                SDL_RenderFillRect(renderer, &rect);
#else
                stubbed_SDL_RenderFillRect(&rect);
#endif
            }
        }
    }
}

static void draw_particles(SDL_Renderer* renderer) {
    SDL_LockMutex(particles_mutex);
    for (size_t i = 0; i < particles_len; i++) {
        fluid_particle p = particles[i];
        float x = SDL_floor(p.x);
        float y = SDL_floor(p.y);
        int n =
            data[p.type].particle_vol < particles_data.size_threshold ? 1 : 4;
        SDL_FPoint points[4] = {
            {x, y}, {x + 1.0f, y}, {x, y + 1.0f}, {x + 1.0f, y + 1.0f}};
#ifndef TEST
        SDL_SetRenderDrawColor(renderer,
                               data[p.type].col_r,
                               data[p.type].col_g,
                               data[p.type].col_b,
                               data[p.type].col_a);
        SDL_RenderPoints(renderer, points, n);
#else
        for (int j = 0; j < n; j++)
            stubbed_SDL_RenderPoint(points[j].x, points[j].y);
#endif
    }
    SDL_UnlockMutex(particles_mutex);
}

#ifndef TEST
// Matches the fluids shader's uniforms on
// `src/gpu/includes/fluids-uniforms.hlsli`, padded to a multiple of 16 bytes.
typedef struct {
    // Each fluid's color indexed by type.
    float colors[FLUIDS_TOTAL][4];
    // Camera position in uv units, so world pixels over the screen's size.
    float cam_pos[2];
    // Seconds since the game started.
    float time;
    float padding;
} shader_uniforms;

// TODO: set `cam_pos` from the camera once there is one.
static void set_shader_uniforms(SDL_GPURenderState* render_state) {
    shader_uniforms uniforms = {.cam_pos = {0.0f, 0.0f},
                                .time = SDL_GetTicks() / 1000.0f};
    for (Uint8 t = 0; t < FLUIDS_TOTAL; t++) {
        uniforms.colors[t][0] = data[t].col_r / 255.0f;
        uniforms.colors[t][1] = data[t].col_g / 255.0f;
        uniforms.colors[t][2] = data[t].col_b / 255.0f;
        uniforms.colors[t][3] = data[t].col_a / 255.0f;
    }
    SDL_SetGPURenderStateFragmentUniforms(
        render_state, 0, &uniforms, sizeof(uniforms));
}
#endif

// Ripples are simulated on two float textures the size of `fluids_texture`,
// `fluids_canvas.ripples`, holding each pixel's current height on red and its
// previous height on green. On the first tick of a draw, the queued ripples
// are added to the current height of the latest one, a `FLUIDS_RIPPLE_SIZE`
// square of pixels each, so they build on the ripples already there instead of
// flattening them. Then, each tick, the latest is drawn into the other
// with the ripples shader, `src/gpu/ripples.frag.hlsl`, which ticks them and
// makes that one the latest. The shader samples `fluids_texture`, drawn just
// before, so ripples only spread through pixels of the same fluid type,
// bouncing off anything else, and pixels that aren't fluid are flattened.
//
// Since rendering must happen on the main thread, ripples aren't ticked on
// `FLUIDS_step()`. It counts ticks on `ripples_ticks` instead, which are all
// run here, so ripples run at the fluids thread's rate. When the main thread
// falls behind, ticks over `FLUIDS_RIPPLES_MAX_TICKS` are dropped.
static void step_ripples(SDL_Renderer* renderer, fluids_canvas* canvas) {
    int ticks = SDL_SetAtomicInt(&ripples_ticks, 0);
    if (ticks <= 0) return;
    ticks = SDL_min(ticks, FLUIDS_RIPPLES_MAX_TICKS);

#ifndef TEST
    SDL_SetRenderTarget(renderer, canvas->ripples[ripples_idx]);
    SDL_BlendMode blend_mode = SDL_BLENDMODE_NONE;
    SDL_GetRenderDrawBlendMode(renderer, &blend_mode);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);

    // SDL mutexes are recursive, so it stays locked while draining.
    SDL_LockMutex(ripples_mutex);
    for (size_t i = 0; i < ripples.len; i++) {
        ripple* r = ripples.at[i];
        SDL_SetRenderDrawColorFloat(renderer, r->strength, 0.0f, 0.0f, 1.0f);
        SDL_FRect rect = {r->x - FLUIDS_RIPPLE_SIZE / 2.0f,
                          r->y - FLUIDS_RIPPLE_SIZE / 2.0f,
                          FLUIDS_RIPPLE_SIZE,
                          FLUIDS_RIPPLE_SIZE};
        SDL_RenderFillRect(renderer, &rect);
    }
    drain_queue(&ripples, ripples_mutex, NULL);
    SDL_UnlockMutex(ripples_mutex);
    SDL_SetRenderDrawBlendMode(renderer, blend_mode);

    SDL_SetGPURenderState(renderer, canvas->ripples_state);
    for (int i = 0; i < ticks; i++) {
        SDL_Texture* from = canvas->ripples[ripples_idx];
        SDL_SetRenderTarget(renderer, canvas->ripples[1 - ripples_idx]);
        SDL_RenderTexture(renderer, from, NULL, NULL);
        ripples_idx = 1 - ripples_idx;
    }
    SDL_SetGPURenderState(renderer, NULL);
#else
    drain_queue(&ripples, ripples_mutex, NULL);
#endif
}

void FLUIDS_draw(SDL_Renderer* renderer, fluids_canvas* canvas) {
#ifndef TEST
    SDL_Texture* target = SDL_GetRenderTarget(renderer);
    SDL_SetRenderTarget(renderer, canvas->texture);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);
#endif

    draw_grid(renderer);
    step_ripples(renderer, canvas);

#ifndef TEST
    SDL_GPURenderState* render_state = canvas->render_states[ripples_idx];
    SDL_SetRenderTarget(renderer, target);
    set_shader_uniforms(render_state);
    SDL_SetGPURenderState(renderer, render_state);
    SDL_RenderTexture(renderer, canvas->texture, NULL, NULL);
    SDL_SetGPURenderState(renderer, NULL);
#endif

    draw_particles(renderer);
}



int FLUIDS_add_particle(
    fluid_type type, double x, double y, double vel_x, double vel_y) {
    long cell_x = SDL_floor(x / FLUIDS_CELL_W);
    long cell_y = SDL_floor(y / FLUIDS_CELL_H);
    if (is_outside(cell_x, cell_y)) return 2;
    if (is_solid(cell_x, cell_y)) return 1;
    fluid_particle p = {
        .x = x, .y = y, .vel_x = vel_x, .vel_y = vel_y, .type = type};
    queue_item(&queued_particles, queued_particles_mutex, &p, sizeof(p));
    return 0;
}

void FLUIDS_splash_at(Uint32 x, Uint32 y, Uint32 w, double speed) {
    splash sp = {.x = x, .y = y, .w = w, .speed = speed};
    queue_item(&splashes, splashes_mutex, &sp, sizeof(sp));
}

void FLUIDS_ripple_at(Uint32 x, Uint32 y, double strength) {
    ripple r = {.x = x, .y = y, .strength = strength};
    queue_item(&ripples, ripples_mutex, &r, sizeof(r));
}
