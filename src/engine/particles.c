#include "engine/particles.h"
#include "data-structs/dynarr.h"
#include "data-structs/hashmap.h"
#include "defs.h"
#include "engine/camera.h"
#include "engine/fluids.h"
#include "engine/game-manager.h"
#include "engine/graphics.h"
#include "paths.h"
#include "services/csv.h"
#include "utils/logger.h"
#include "utils/parsing.h"
#include <SDL3/SDL.h>

// Matches `ParticleType` on `src/gpu/includes/particles-common.hlsli`.
typedef struct {
    SDL_FColor col;
    SDL_FColor col_jitter;
    float src[4];
    float acc[2];
    int pad0[2];

    float lifetime;
    float lifetime_jitter;
    float fade_time;
    Uint32 ignore_fluids;

    Uint32 face_vel;
    float damping;
    float slither_freq;
    float slither_amp;

    Uint32 bounce_count;
    float bounce;
    float bounce_until;

    float col_str;

    Uint32 death_particles;
    Uint32 death_count;
    float death_speed;
    float death_spread;
    float death_speed_jitter;

    Uint32 trail_particles;
    float trail_freq;
    float trail_speed;
    float trail_spread;
    float trail_speed_jitter;

    Uint32 group;
    int pad1;
} gpu_type;

// Matches `Request` on `src/gpu/includes/particles-common.hlsli`.
typedef struct {
    float pos[2];
    float vel[2];
    float size[2];
    float speed_jitter;
    float spread;
    Uint32 type;
    Uint32 count;
    Uint32 seed;
    Uint32 shape;
} request;

// Matches `src/gpu/includes/particles-uniforms.hlsli`.
typedef struct {
    int fluid_sim_rect[4];
    float fluid_cell_size[2];
    int pad0[2];
    float dt;
    Uint32 particle_buffer_cap;
    Uint32 max_requests;
    int pad1;
} compute_uniforms;

// Matches the second uniform buffer of `src/gpu/particles.vert.hlsl`.
typedef struct {
    float cam_pos[2];
    float texture_size[2];
    Uint32 group;
    Uint32 particle_buffer_cap;
    int pad0;
    int pad1;
} vertex_uniforms;

// A row of `PARTICLES_CSV_PATH`. Sprites are in particle cells, spritesheet
// cells split by `PARTICLES_SPLIT_X` and `PARTICLES_SPLIT_Y`.
typedef struct {
    particle_type type;
    particle_shader shader;

    Uint8 sprite_x, sprite_y, sprite_w, sprite_h;

    float lifetime;
    float lifetime_jitter;
    float fade_time;
    bool ignore_fluids;

    bool face_vel;
    float acc_x, acc_y;
    float damping;
    float slither_amp;
    float slither_freq;

    Uint8 bounce_count;
    float bounce;
    float bounce_until;

    Uint8 col_r, col_g, col_b, col_a;
    float col_str;
    Uint8 col_jitter_r, col_jitter_g, col_jitter_b, col_jitter_a;

    particle_type death_particles;
    Uint32 death_count;
    float death_speed;
    float death_spread;
    float death_speed_jitter;

    particle_type trail_particles;
    float trail_freq;
    float trail_speed;
    float trail_spread;
    float trail_speed_jitter;
} type_data;

// Fluid cells on the simulated rect.
#define GRID_CELLS                                                             \
    (SIMULATION_SPACE_X * FLUIDS_SPLIT_X * SIMULATION_SPACE_Y * FLUIDS_SPLIT_Y)

// The grid is read as uints on the GPU, so its size is rounded up to them.
#define GRID_SIZE ((GRID_CELLS + 3) / 4 * 4)

static_assert((PARTICLES_CAPACITY & (PARTICLES_CAPACITY - 1)) == 0,
              "PARTICLES_CAPACITY must be a power of 2");

static const char particles_csv[] = {
#embed PARTICLES_CSV_PATH suffix(, '\0')
};
static const Uint8 simulate_spirv[] = {
#embed PARTICLES_SIMULATE_COMP_SPIRV_PATH
};
static const Uint8 simulate_dxil[] = {
#embed PARTICLES_SIMULATE_COMP_DXIL_PATH
};
static const Uint8 spawn_spirv[] = {
#embed PARTICLES_SPAWN_COMP_SPIRV_PATH
};
static const Uint8 spawn_dxil[] = {
#embed PARTICLES_SPAWN_COMP_DXIL_PATH
};
static const Uint8 compact_spirv[] = {
#embed PARTICLES_COMPACT_COMP_SPIRV_PATH
};
static const Uint8 compact_dxil[] = {
#embed PARTICLES_COMPACT_COMP_DXIL_PATH
};
static const Uint8 vert_spirv[] = {
#embed PARTICLES_VERT_SPIRV_PATH
};
static const Uint8 vert_dxil[] = {
#embed PARTICLES_VERT_DXIL_PATH
};
static const Uint8 frag_spirv[] = {
#embed PARTICLES_FRAG_SPIRV_PATH
};
static const Uint8 frag_dxil[] = {
#embed PARTICLES_FRAG_DXIL_PATH
};
static const Uint8 glow_shader_spirv[] = {
#embed PARTICLES_SHADER_GLOW_SPIRV_PATH
};
static const Uint8 glow_shader_dxil[] = {
#embed PARTICLES_SHADER_GLOW_DXIL_PATH
};

static gpu_type types[PARTICLES_TOTAL] = {0};
static Uint8 grid[GRID_SIZE] = {0};

static dynarr requests = (dynarr){0}; // request dynarr
static SDL_Mutex* requests_mutex = NULL;

static SDL_GPUBuffer* types_buffer = NULL;
static SDL_GPUBuffer* grid_buffer = NULL;
static SDL_GPUBuffer* particles_buffer = NULL;
static SDL_GPUBuffer* requests_buffer = NULL;
static SDL_GPUBuffer* counters_buffer = NULL;
static SDL_GPUBuffer* alive_buffer = NULL;
static SDL_GPUBuffer* draw_args_buffer = NULL;
static SDL_GPUComputePipeline* simulate_pipeline = NULL;
static SDL_GPUComputePipeline* spawn_pipeline = NULL;
static SDL_GPUComputePipeline* compact_pipeline = NULL;
static SDL_GPUShader* vert_shader = NULL;
static SDL_GPUShader* frag_shaders[particle_shaders_total] = {0};

// What each group's draw arguments are reset to every frame, before the
// compaction pass counts its instances: quads of 6 vertices, none of them yet.
static SDL_GPUIndirectDrawCommand draw_args[particle_shaders_total] = {0};



static void load_types(void) {
    dynarr parsed = CSV_parse(particles_csv);
    type_data* rows = SDL_calloc(count_csv_entries(&parsed), sizeof(type_data));
    parsing_field fields[] = {
        {"particle", parsing_particle, &rows->type},
        {"shader", parsing_particle_shader, &rows->shader},

        {"sprite_x", parsing_uint8, &rows->sprite_x},
        {"sprite_y", parsing_uint8, &rows->sprite_y},
        {"sprite_w", parsing_uint8, &rows->sprite_w},
        {"sprite_h", parsing_uint8, &rows->sprite_h},

        {"lifetime", parsing_float, &rows->lifetime},
        {"lifetime_jitter", parsing_float, &rows->lifetime_jitter},
        {"fade_time", parsing_float, &rows->fade_time},
        {"ignore_fluids", parsing_bool, &rows->ignore_fluids},

        {"face_vel", parsing_bool, &rows->face_vel},
        {"acc_x", parsing_float, &rows->acc_x},
        {"acc_y", parsing_float, &rows->acc_y},
        {"damping", parsing_float, &rows->damping},
        {"slither_amp", parsing_float, &rows->slither_amp},
        {"slither_freq", parsing_float, &rows->slither_freq},

        {"bounce_count", parsing_uint8, &rows->bounce_count},
        {"bounce", parsing_float, &rows->bounce},
        {"bounce_until", parsing_float, &rows->bounce_until},

        {"col_r", parsing_uint8, &rows->col_r},
        {"col_g", parsing_uint8, &rows->col_g},
        {"col_b", parsing_uint8, &rows->col_b},
        {"col_a", parsing_uint8, &rows->col_a},
        {"col_str", parsing_float, &rows->col_str},
        {"col_jitter_r", parsing_uint8, &rows->col_jitter_r},
        {"col_jitter_g", parsing_uint8, &rows->col_jitter_g},
        {"col_jitter_b", parsing_uint8, &rows->col_jitter_b},
        {"col_jitter_a", parsing_uint8, &rows->col_jitter_a},

        {"death_particles", parsing_particle, &rows->death_particles},
        {"death_count", parsing_uint32, &rows->death_count},
        {"death_speed", parsing_float, &rows->death_speed},
        {"death_spread", parsing_float, &rows->death_spread},
        {"death_speed_jitter", parsing_float, &rows->death_speed_jitter},

        {"trail_particles", parsing_particle, &rows->trail_particles},
        {"trail_freq", parsing_float, &rows->trail_freq},
        {"trail_speed", parsing_float, &rows->trail_speed},
        {"trail_spread", parsing_float, &rows->trail_spread},
        {"trail_speed_jitter", parsing_float, &rows->trail_speed_jitter},
    };
    size_t n = read_csv_table(
        &parsed, fields, SDL_arraysize(fields), sizeof(type_data));
    CSV_free(&parsed);

    for (size_t i = 0; i < n; i++) {
        type_data* d = &rows[i];
        types[d->type] = (gpu_type){
            .group = d->shader,
            .col = GRAPHICS_fcolor(
                (SDL_Color){d->col_r, d->col_g, d->col_b, d->col_a}),
            .col_str = d->col_str,
            .col_jitter = GRAPHICS_fcolor((SDL_Color){d->col_jitter_r,
                                                      d->col_jitter_g,
                                                      d->col_jitter_b,
                                                      d->col_jitter_a}),


            .src = {d->sprite_x * PARTICLES_CELL_W,
                    d->sprite_y * PARTICLES_CELL_H,
                    d->sprite_w * PARTICLES_CELL_W,
                    d->sprite_h * PARTICLES_CELL_H},
            .acc = {d->acc_x, d->acc_y},
            .damping = d->damping,
            .face_vel = d->face_vel,
            .slither_freq = d->slither_freq,
            .slither_amp = d->slither_amp,
            .bounce_count = d->bounce_count,
            .bounce = d->bounce,
            .bounce_until = d->bounce_until,

            .lifetime = d->lifetime,
            .lifetime_jitter = d->lifetime_jitter,
            .fade_time = d->fade_time,
            .ignore_fluids = d->ignore_fluids,

            .death_particles = d->death_particles,
            .death_count = d->death_count,
            .death_speed = d->death_speed,
            .death_spread = d->death_spread,
            .death_speed_jitter = d->death_speed_jitter,

            .trail_particles = d->trail_particles,
            .trail_freq = d->trail_freq,
            .trail_speed = d->trail_speed,
            .trail_spread = d->trail_spread,
            .trail_speed_jitter = d->trail_speed_jitter,

        };
    }
    SDL_free(rows);
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Particles data loaded successfully.");
}

// The types are read by both the compute and vertex shaders. The grid is only
// read by the simulation, and the requests and counters are written by it, so
// they're read-write on the compute shaders even where they're only read. The
// compaction pass writes the alive indices and draw arguments, which the draws
// then read. Each group has `PARTICLES_CAPACITY` alive indices, so any of them
// can have every particle.
static int create_buffers(void) {
    for (int i = 0; i < particle_shaders_total; i++)
        draw_args[i] = (SDL_GPUIndirectDrawCommand){.num_vertices = 6};
    types_buffer =
        GRAPHICS_create_buffer("particle_types",
                               SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ |
                                   SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
                               sizeof(types));
    grid_buffer = GRAPHICS_create_buffer(
        "particles_grid", SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ, GRID_SIZE);
    particles_buffer =
        GRAPHICS_create_buffer("particles",
                               SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ |
                                   SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE |
                                   SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
                               PARTICLES_CAPACITY * PARTICLE_SIZE);
    requests_buffer =
        GRAPHICS_create_buffer("particle_requests",
                               SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ |
                                   SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,
                               PARTICLES_MAX_REQUESTS * sizeof(request));
    counters_buffer =
        GRAPHICS_create_buffer("particle_counters",
                               SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ |
                                   SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,
                               2 * sizeof(Uint32));
    alive_buffer = GRAPHICS_create_buffer(
        "particles_alive",
        SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ |
            SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE |
            SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
        particle_shaders_total * PARTICLES_CAPACITY * sizeof(Uint32));
    draw_args_buffer =
        GRAPHICS_create_buffer("particles_draw_args",
                               SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ |
                                   SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE |
                                   SDL_GPU_BUFFERUSAGE_INDIRECT,
                               sizeof(draw_args));
    if (!types_buffer || !grid_buffer || !particles_buffer ||
        !requests_buffer || !counters_buffer || !alive_buffer ||
        !draw_args_buffer)
        return 1;
    GRAPHICS_upload_buffer(types_buffer, 0, types, sizeof(types));
    return 0;
}

static int load_shaders(void) {
    simulate_pipeline = GRAPHICS_load_compute_pipeline(
        "particles_simulate",
        GRAPHICS_SHADER_CODE(simulate_spirv, simulate_dxil),
        2,
        3,
        1,
        PARTICLES_THREADS);
    if (!simulate_pipeline) return 1;

    spawn_pipeline = GRAPHICS_load_compute_pipeline(
        "particles_spawn",
        GRAPHICS_SHADER_CODE(spawn_spirv, spawn_dxil),
        1,
        3,
        1,
        PARTICLES_THREADS);
    if (!spawn_pipeline) return 1;

    compact_pipeline = GRAPHICS_load_compute_pipeline(
        "particles_compact",
        GRAPHICS_SHADER_CODE(compact_spirv, compact_dxil),
        2,
        2,
        1,
        PARTICLES_THREADS);
    if (!compact_pipeline) return 1;

    vert_shader = GRAPHICS_load_vertex_shader(
        "particles_vert", GRAPHICS_SHADER_CODE(vert_spirv, vert_dxil), 3, 2);
    if (!vert_shader) return 1;

    frag_shaders[particle_shader_base] = GRAPHICS_load_fragment_shader(
        "particles_base_frag",
        GRAPHICS_SHADER_CODE(frag_spirv, frag_dxil),
        1,
        0,
        0);

    frag_shaders[particle_shader_glow] = GRAPHICS_load_fragment_shader(
        "particles_shader_glow",
        GRAPHICS_SHADER_CODE(glow_shader_spirv, glow_shader_dxil),
        1,
        0,
        0);

    for (int i = 0; i < particle_shaders_total; i++)
        if (!frag_shaders[i]) return 1;
    return 0;
}

int PARTICLES_start(void) {
    init_dynarr_with(
        &requests, sizeof(request), PARTICLES_STARTING_REQUESTS_DYNARR_CAP);
    requests_mutex = SDL_CreateMutex();
    if (!log_creation(SDL_LOG_CATEGORY_APPLICATION,
                      requests_mutex,
                      "particles' requests mutex"))
        return 1;
    load_types();
    if (create_buffers()) return 1;
    if (load_shaders()) return 1;
    return 0;
}

void PARTICLES_stop(void) {
    GRAPHICS_release_shader(vert_shader);
    for (int i = 0; i < particle_shaders_total; i++) {
        GRAPHICS_release_shader(frag_shaders[i]);
        frag_shaders[i] = NULL;
    }
    GRAPHICS_release_compute_pipeline(compact_pipeline);
    GRAPHICS_release_compute_pipeline(spawn_pipeline);
    GRAPHICS_release_compute_pipeline(simulate_pipeline);
    GRAPHICS_destroy_buffer(draw_args_buffer);
    GRAPHICS_destroy_buffer(alive_buffer);
    GRAPHICS_destroy_buffer(counters_buffer);
    GRAPHICS_destroy_buffer(requests_buffer);
    GRAPHICS_destroy_buffer(particles_buffer);
    GRAPHICS_destroy_buffer(grid_buffer);
    GRAPHICS_destroy_buffer(types_buffer);
    vert_shader = NULL;
    compact_pipeline = spawn_pipeline = simulate_pipeline = NULL;
    draw_args_buffer = alive_buffer = NULL;
    counters_buffer = requests_buffer = particles_buffer = NULL;
    grid_buffer = types_buffer = NULL;

    deinit_dynarr(&requests);
    SDL_DestroyMutex(requests_mutex);
    requests_mutex = NULL;
    SDL_memset(types, 0, sizeof(types));
}



void PARTICLES_emit(particle_type type, particles_emit_settings settings) {
    request r = {
        .vel = {settings.vel.x, settings.vel.y},
        .speed_jitter = settings.speed_jitter,
        .spread = settings.spread,
        .type = type,
        .count = settings.count,
        .seed = SDL_rand_bits(),
        .shape = settings.shape,
    };
    switch (settings.shape) {
    case particles_emit_point:
        r.pos[0] = settings.pos.x;
        r.pos[1] = settings.pos.y;
        break;
    case particles_emit_rect:
        r.pos[0] = settings.rect.x;
        r.pos[1] = settings.rect.y;
        r.size[0] = settings.rect.w;
        r.size[1] = settings.rect.h;
        break;
    }
    SDL_LockMutex(requests_mutex);
    push_to_dynarr(&r, &requests);
    SDL_UnlockMutex(requests_mutex);
}

SDL_GPUBuffer* PARTICLES_get_requests_buffer(void) {
    return requests_buffer;
}

SDL_GPUBuffer* PARTICLES_get_counters_buffer(void) {
    return counters_buffer;
}

// Uploads the queued requests and sets the request counter to how many there
// are, which also discards the ones the simulation appended last frame.
static void upload_requests(void) {
    SDL_LockMutex(requests_mutex);
    Uint32 n = SDL_min(requests.len, PARTICLES_MAX_REQUESTS);
    if (requests.len > PARTICLES_MAX_REQUESTS)
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Dropped %zu particle requests, increase "
                    "PARTICLES_MAX_REQUESTS.",
                    requests.len - PARTICLES_MAX_REQUESTS);
    if (n)
        GRAPHICS_upload_buffer(
            requests_buffer, 0, requests.data, n * sizeof(request));
    clear_dynarr(&requests);
    SDL_UnlockMutex(requests_mutex);

    GRAPHICS_upload_buffer(counters_buffer, 0, &n, sizeof(n));
}

void PARTICLES_draw(graphics_texture* spritesheet) {
    if (!particles_buffer) return;

    upload_requests();
    GRAPHICS_upload_buffer(draw_args_buffer, 0, draw_args, sizeof(draw_args));
    SDL_Rect rect = FLUIDS_get_codes(grid);
    GRAPHICS_upload_buffer(grid_buffer, 0, grid, GRID_SIZE);

    compute_uniforms uniforms = {
        .dt = GAME_MANAGER_get_main_dt(),
        .max_requests = PARTICLES_MAX_REQUESTS,
        .particle_buffer_cap = PARTICLES_CAPACITY,
        .fluid_sim_rect = {rect.x, rect.y, rect.w, rect.h},
        .fluid_cell_size = {FLUIDS_CELL_W, FLUIDS_CELL_H},
    };
    GRAPHICS_dispatch(&(graphics_dispatch){
        .pipeline = simulate_pipeline,
        .readonly_buffers = {types_buffer, grid_buffer},
        .num_readonly_buffers = 2,
        .readwrite_buffers = {particles_buffer,
                              requests_buffer,
                              counters_buffer},
        .num_readwrite_buffers = 3,
        .uniforms = &uniforms,
        .uniforms_size = sizeof(uniforms),
        .groups = PARTICLES_CAPACITY / PARTICLES_THREADS,
    });
    GRAPHICS_dispatch(&(graphics_dispatch){
        .pipeline = spawn_pipeline,
        .readonly_buffers = {types_buffer},
        .num_readonly_buffers = 1,
        .readwrite_buffers = {particles_buffer,
                              requests_buffer,
                              counters_buffer},
        .num_readwrite_buffers = 3,
        .uniforms = &uniforms,
        .uniforms_size = sizeof(uniforms),
        .groups = (PARTICLES_MAX_REQUESTS + PARTICLES_THREADS - 1) /
                  PARTICLES_THREADS,
    });
    GRAPHICS_dispatch(&(graphics_dispatch){
        .pipeline = compact_pipeline,
        .readonly_buffers = {types_buffer, particles_buffer},
        .num_readonly_buffers = 2,
        .readwrite_buffers = {alive_buffer, draw_args_buffer},
        .num_readwrite_buffers = 2,
        .uniforms = &uniforms,
        .uniforms_size = sizeof(uniforms),
        .groups = PARTICLES_CAPACITY / PARTICLES_THREADS,
    });

    // One draw per group, each drawing only its live particles, as many as the
    // compaction pass counted on its draw arguments.
    SDL_FRect cam = CAMERA_get_rect();
    for (Uint32 g = 0; g < particle_shaders_total; g++) {
        vertex_uniforms v_uniforms = {
            .cam_pos = {cam.x, cam.y},
            .texture_size = {spritesheet->w, spritesheet->h},
            .group = g,
            .particle_buffer_cap = PARTICLES_CAPACITY,
        };
        GRAPHICS_set_state(&(graphics_state){
            .vertex = vert_shader,
            .vertex_storage_buffers = {particles_buffer,
                                       types_buffer,
                                       alive_buffer},
            .num_vertex_storage_buffers = 3,
            .vertex_uniforms = &v_uniforms,
            .vertex_uniforms_size = sizeof(v_uniforms),
            .fragment = frag_shaders[g],
            .blend = graphics_blend_blend,
        });
        GRAPHICS_draw_instances(
            spritesheet,
            &(graphics_instances){
                .indirect_buffer = draw_args_buffer,
                .indirect_offset = g * sizeof(SDL_GPUIndirectDrawCommand),
            });
    }
    GRAPHICS_set_state(NULL);
}



hashmap PARTICLES_get_string_particle_types_map(void) {
    hashmap map;
    init_hashmap(&map, sizeof(particle_type));
    set_on_hashmap(&(particle_type){particle_tile_dirt}, "dirt", &map);
    set_on_hashmap(&(particle_type){particle_tile_stone}, "stone", &map);
    set_on_hashmap(&(particle_type){particle_tile_wood}, "wood", &map);
    set_on_hashmap(&(particle_type){particle_tile_metal}, "metal", &map);
    set_on_hashmap(
        &(particle_type){particle_fluid_acid_bubble}, "acid_bubble", &map);
    set_on_hashmap(
        &(particle_type){particle_fluid_sludge_bubble}, "sludge_bubble", &map);
    set_on_hashmap(
        &(particle_type){particle_fluid_sludge_pop}, "sludge_pop", &map);
    return map;
}

hashmap PARTICLES_get_string_particle_shaders_map(void) {
    hashmap map;
    init_hashmap(&map, sizeof(particle_shader));
    set_on_hashmap(&(particle_shader){particle_shader_base}, "base", &map);
    set_on_hashmap(&(particle_shader){particle_shader_glow}, "glow", &map);
    return map;
}
