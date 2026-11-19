#include "props/flames.h"
#include "data-structs/pool.h"
#include "defs.h"
#include "engine/camera.h"
#include "engine/game-manager.h"
#include "engine/particles.h"
#include "paths.h"
#include "services/csv.h"
#include "utils/math.h"
#include "utils/parsing.h"
#include <SDL3/SDL.h>

typedef struct {
    flame f;
    double base_time;
    double spark_time;
} flame_wrapper;

typedef struct {
    double emit_w, emit_h;
    double base_time;
    double spark_min_time, spark_max_time;
    double emit_base_speed;
    double emit_spark_speed;
    double spread;
    double speed_jitter;
} flames_data;

static pool flames = (pool){0};
static flames_data data = (flames_data){0};

static const char flames_csv[] = {
#embed PROPS_FLAMES_CSV_PATH suffix(, '\0')
};



static flame_wrapper* get_flame(size_t id) {
    return pool_at(id, &flames);
}

static int check_id(size_t flame_id, const char* to_do) {
    if (flame_id >= flames.cap) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to %s flame_id=%lu but this is outside the buffer "
                    "range",
                    to_do,
                    flame_id);
        return 3;
    }
    if (!get_flame(flame_id)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to %s inexistent flame at flame_id=%lu",
                    to_do,
                    flame_id);
        return 1;
    }
    return 0;
}



void FLAMES_start(void) {
    init_pool_with(
        &flames, sizeof(flame_wrapper), PROPS_FLAMES_STARTING_POOL_CAP);
    parsing_field fields[] = {
        {"emit_w", parsing_double, &data.emit_w},
        {"emit_h", parsing_double, &data.emit_h},
        {"base_time", parsing_double, &data.base_time},
        {"spark_min_time", parsing_double, &data.spark_min_time},
        {"spark_max_time", parsing_double, &data.spark_max_time},
        {"emit_base_speed", parsing_double, &data.emit_base_speed},
        {"emit_spark_speed", parsing_double, &data.emit_spark_speed},
        {"spread", parsing_double, &data.spread},
        {"speed_jitter", parsing_double, &data.speed_jitter},
    };
    dynarr parsed = CSV_parse(flames_csv);
    read_csv_keys(&parsed, fields, SDL_arraysize(fields));
    CSV_free(&parsed);
}

void FLAMES_stop(void) {
    deinit_pool(&flames);
}

void FLAMES_update(void) {
    for (size_t i = 0; i < flames.cap; i++) {
        if (!flames.used[i]) continue;
        flame_wrapper* d = pool_at(i, &flames);
        SDL_FRect cam = CAMERA_get_rect();
        cam.x -= SPRITESHEET_CELL_X;
        cam.y -= SPRITESHEET_CELL_Y;
        cam.w += 2 * SPRITESHEET_CELL_X;
        cam.h += 2 * SPRITESHEET_CELL_Y;
        if (!is_in_rect(cam, d->f.x, d->f.y)) continue;
        d->base_time += GAME_MANAGER_get_current_dt();
        d->spark_time -= GAME_MANAGER_get_current_dt();
        if (d->base_time >= data.base_time) {
            d->base_time -= data.base_time;
            PARTICLES_emit(
                particle_flames_base,
                (particles_emit_settings){
                    .shape = particles_emit_rect,
                    .rect = {.x = d->f.x,
                             .y = d->f.y,
                             .w = data.emit_w,
                             .h = data.emit_h},
                    .vel = {.x = 0.0f, .y = PROPS_FLAMES_EMIT_BASE_SPEED},
                    .count = 1,
                });
        }
        if (d->spark_time <= 0.0f) {
            d->spark_time = lerp(PROPS_FLAMES_SPARK_MIN_TIME,
                                 PROPS_FLAMES_SPARK_MAX_TIME,
                                 SDL_randf());
            PARTICLES_emit(particle_flames_spark,
                           (particles_emit_settings){
                               .shape = particles_emit_rect,
                               .rect = {.x = d->f.x,
                                        .y = d->f.y,
                                        .w = data.emit_w,
                                        .h = data.emit_h},
                               .vel = {.x = data.emit_spark_speed * ISQRT2,
                                       .y = data.emit_spark_speed * ISQRT2},
                               .spread = data.spread,
                               .speed_jitter = data.speed_jitter,
                               .count = 1,
                           });
        }
    }
}



int FLAMES_new_flames(flame data, size_t* flame_id) {
    *flame_id = add_to_pool(
        &(flame_wrapper){
            .f = data,
            .base_time = 0.0f,
            .spark_time = 0.0f,
        },
        &flames);
    return 0;
}

int FLAMES_get_flames(size_t flame_id, flame* data) {
    int err = check_id(flame_id, "get");
    if (err) return err;
    *data = get_flame(flame_id)->f;
    return 0;
}

int FLAMES_edit_flames(size_t flame_id, flame data) {
    int err = check_id(flame_id, "edit");
    if (err) return err;
    flame_wrapper* fdata = get_flame(flame_id);
    fdata->f = data;
    return 0;
}

int FLAMES_del_flames(size_t flame_id) {
    int err = check_id(flame_id, "delete");
    if (err) return err;
    remove_from_pool(flame_id, &flames);
    return 0;
}
