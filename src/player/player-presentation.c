#include "player/player-presentation.h"
#include "data-structs/dynarr.h"
#include "defs.h"
#include "engine/fluids.h"
#include "engine/physics.h"
#include "engine/sprites.h"
#include "paths.h"
#include "player/player-core.h"
#include "services/csv.h"
#include "utils/parsing.h"
#include <SDL3/SDL.h>

#define HAIR_BASE_WIDTH 3

typedef enum {
    state_idle,
    state_moving,
    state_airborne,
    state_crouch_idle,
    state_crouch_moving,
    state_swimming,
} states_machine_state;

// Left and right pins of the cloak in pixels inside the sprite's cell, for a
// frame of the player's sprite. `mapped` is false for frames without any.
typedef struct {
    bool mapped;
    SDL_Point l, r;
} cloak_pins;

// Pin of the hair in pixels inside the spritesheet's cell, for a frame of the
// player's sprite. `mapped` is false for frames without one.
typedef struct {
    bool mapped;
    SDL_Point pin;
} hair_pin;

// The hair's and cloak's pins are mapped by the spritesheet cell of the
// sprite's current frame on `hair_pins` and `cloak_pins`. The hair has
// `hair_segments` segments, so one more point than that, and the cloak has
// `cloak_horiz_segments` by `cloak_vert_segments` segments, so one more column
// and one more row of points than that.
typedef struct {
    Uint8 coords_x, coords_y, coords_w, coords_h;
    Sint8 offset_x, offset_y;
    animation idle_anim;
    animation moving_anim;
    animation airborne_anim;
    animation crouch_idle_anim;
    animation crouch_moving_anim;
    animation swimming_anim;
    SDL_FColor hair_col;
    double hair_gravity;
    double hair_fluid_gravity;
    double hair_damping;
    double hair_fluid_damping;
    Uint8 hair_segments;
    double hair_segments_length;
    bool hair_segments_two_ways;
    hair_pin hair_pins[SPRITESHEET_X / SPRITESHEET_CELL_X]
                      [SPRITESHEET_Y / SPRITESHEET_CELL_Y];
    SDL_FColor cloak_in;
    SDL_FColor cloak_out;
    double cloak_gravity;
    double cloak_fluid_gravity;
    double cloak_damping;
    double cloak_fluid_damping;
    Uint8 cloak_horiz_segments;
    Uint8 cloak_vert_segments;
    double cloak_horiz_segment_length;
    double cloak_vert_segment_length;
    bool cloak_horiz_segment_two_ways;
    bool cloak_vert_segment_two_ways;
    cloak_pins cloak_pins[SPRITESHEET_X / SPRITESHEET_CELL_X]
                         [SPRITESHEET_Y / SPRITESHEET_CELL_Y];
} sprite_data;

// The cloak is a grid of prop points on `cloak`, each one connected to its
// neighbours to the right and below, with its top row pinned to the player.
// `cloak_deleted` has whether each point was already deleted, and once any of
// them is the cloak is `cloak_broken` and gets recreated on the next update.
// `cloak_under_fluid` has whether each point was in a fluid cell on the last
// update. `cloak_pins` are the pins of the last mapped frame the sprite was on,
// kept on frames that aren't mapped. The hair is a chain of prop points on
// `hair` from its pinned point, with the same kind of state as the cloak.
typedef struct {
    bool created;
    size_t sprite_id;
    states_machine_state state;
    size_t hair[PLAYER_HAIR_MAX_SEGMENTS + 1];
    bool hair_deleted[PLAYER_HAIR_MAX_SEGMENTS + 1];
    bool hair_under_fluid[PLAYER_HAIR_MAX_SEGMENTS + 1];
    bool hair_broken;
    hair_pin hair_pin;
    size_t cloak[PLAYER_CLOAK_MAX_VERT_SEGMENTS + 1]
                [PLAYER_CLOAK_MAX_HORIZ_SEGMENTS + 1];
    bool cloak_deleted[PLAYER_CLOAK_MAX_VERT_SEGMENTS + 1]
                      [PLAYER_CLOAK_MAX_HORIZ_SEGMENTS + 1];
    bool cloak_under_fluid[PLAYER_CLOAK_MAX_VERT_SEGMENTS + 1]
                          [PLAYER_CLOAK_MAX_HORIZ_SEGMENTS + 1];
    bool cloak_broken;
    cloak_pins cloak_pins;
} sprite_state;

static sprite_data data = (sprite_data){0};
static sprite_state state = (sprite_state){0};



static void state_changed(states_machine_state new_state) {
    switch (new_state) {
    case state_idle:
        SPRITES_play_anim(state.sprite_id, data.idle_anim);
        break;
    case state_moving:
        SPRITES_play_anim(state.sprite_id, data.moving_anim);
        break;
    case state_airborne:
        SPRITES_play_anim(state.sprite_id, data.airborne_anim);
        break;
    case state_crouch_idle:
        SPRITES_play_anim(state.sprite_id, data.crouch_idle_anim);
        break;
    case state_crouch_moving:
        SPRITES_play_anim(state.sprite_id, data.crouch_moving_anim);
        break;
    case state_swimming:
        SPRITES_play_anim(state.sprite_id, data.swimming_anim);
        break;
    }
}

static bool check_fluid(size_t point_id,
                        bool was_under,
                        double gravity,
                        double fluid_gravity,
                        double damping,
                        double fluid_damping) {
    prop_point p = (prop_point){0};
    PHYSICS_get_prop_point(point_id, &p);
    fluid_cell cell[1][1];
    FLUIDS_get_at(p.x / FLUIDS_CELL_W, p.y / FLUIDS_CELL_H, 1, 1, &cell);
    if (cell[0][0].solid) return false;

    double total = 0.0f;
    for (Uint8 t = 0; t < FLUIDS_TOTAL; t++)
        total += cell[0][0].volume[t];

    bool under = total >= FLUIDS_INTERACTION_VOL_THRESHOLD;
    if (under && !was_under) {
        PHYSICS_set_prop_point_acceleration(point_id, 0.0f, fluid_gravity);
        PHYSICS_set_prop_point_damping(point_id, fluid_damping);
    } else if (!under && was_under) {
        PHYSICS_set_prop_point_acceleration(point_id, 0.0f, gravity);
        PHYSICS_set_prop_point_damping(point_id, damping);
    }
    return under;
}

static SDL_FPoint get_hair_pin(Sint8 facing, body b) {
    sprite s = (sprite){0};
    SPRITES_get_sprite(state.sprite_id, &s);
    hair_pin pin = data.hair_pins[s.coords_x][s.coords_y];
    if (pin.mapped)
        state.hair_pin = pin;
    else
        pin = state.hair_pin;

    SDL_Point at = pin.pin;
    if (facing != 1) at.x = SPRITESHEET_CELL_X * data.coords_w - 1 - at.x;
    return (SDL_FPoint){
        SDL_roundf(b.x) + data.offset_x + at.x,
        SDL_roundf(b.y) + data.offset_y + at.y,
    };
}

static void on_hair_point_deleted(size_t point_id, size_t _arg) { // NOLINT
    for (Uint8 i = 0; i <= data.hair_segments; i++)
        if (!state.hair_deleted[i] && state.hair[i] == point_id)
            state.hair_deleted[i] = true;
    state.hair_broken = true;
}

static void create_hair(player_state player) {
    body b = (body){0};
    PHYSICS_get_body(player.body_id, &b);
    SDL_FPoint pin = get_hair_pin(player.facing, b);
    for (Uint8 i = 0; i <= data.hair_segments; i++) {
        prop_point p = (prop_point){
            .x = pin.x,
            .y = pin.y,
            .acc_y = data.hair_gravity,
            .damping = data.hair_damping,
            .pinned = i == 0,
            .ignore_bodies = true,
            .on_delete_func = on_hair_point_deleted,
        };
        PHYSICS_new_prop_point(p, &state.hair[i]);
        state.hair_deleted[i] = false;
        state.hair_under_fluid[i] = false;
        if (i > 0)
            PHYSICS_connect_prop_points(state.hair[i - 1],
                                        state.hair[i],
                                        0.0f,
                                        data.hair_segments_length,
                                        data.hair_segments_two_ways);
    }
    state.hair_broken = false;
}

static void delete_hair(void) {
    for (Uint8 i = 0; i <= data.hair_segments; i++)
        if (!state.hair_deleted[i]) PHYSICS_del_prop_point(state.hair[i]);
}

// The first point is pinned, so only the points after it check for fluids.
static void update_hair(player_state player, body b) {
    if (state.hair_broken) {
        delete_hair();
        create_hair(player);
    }
    for (Uint8 i = 1; i <= data.hair_segments; i++)
        state.hair_under_fluid[i] = check_fluid(state.hair[i],
                                                state.hair_under_fluid[i],
                                                data.hair_gravity,
                                                data.hair_fluid_gravity,
                                                data.hair_damping,
                                                data.hair_fluid_damping);
    prop_point p = (prop_point){0};
    PHYSICS_get_prop_point(state.hair[0], &p);
    SDL_FPoint pin = get_hair_pin(player.facing, b);
    p.x = pin.x;
    p.y = pin.y;
    PHYSICS_edit_prop_point(state.hair[0], p);
}

// The first and last pins on the cloak's top row are the current frame's left
// and right pins, positioned like the sprite on the player's rounded position
// so the cloak doesn't jitter around it, and the pins between them split the
// way from the first to the last in equal lengths. Facing left they're mirrored
// like the sprite, with the first pin being the mirrored right one so it's
// still the leftmost, otherwise turning around would flip the cloak's triangles
// and show its outer side.
static SDL_FPoint get_cloak_pin(Sint8 facing, body b, Uint8 col) {
    sprite s = (sprite){0};
    SPRITES_get_sprite(state.sprite_id, &s);
    cloak_pins pins = data.cloak_pins[s.coords_x][s.coords_y];
    if (pins.mapped)
        state.cloak_pins = pins;
    else
        pins = state.cloak_pins;

    SDL_Point first = pins.l;
    SDL_Point last = pins.r;
    if (facing != 1) {
        int w = SPRITESHEET_CELL_X * data.coords_w - 1;
        first = (SDL_Point){w - pins.r.x, pins.r.y};
        last = (SDL_Point){w - pins.l.x, pins.l.y};
    }
    float t = data.cloak_horiz_segments > 0
                  ? (float)col / data.cloak_horiz_segments
                  : 0.0f;
    return (SDL_FPoint){
        SDL_roundf(b.x) + data.offset_x + first.x + (last.x - first.x) * t,
        SDL_roundf(b.y) + data.offset_y + first.y + (last.y - first.y) * t,
    };
}

// Called by the physics engine when any point of the cloak gets deleted, we
// can't create points here so the cloak is recreated on the next update.
static void on_cloak_point_deleted(size_t point_id, size_t _arg) { // NOLINT
    for (Uint8 r = 0; r <= data.cloak_vert_segments; r++)
        for (Uint8 c = 0; c <= data.cloak_horiz_segments; c++)
            if (!state.cloak_deleted[r][c] && state.cloak[r][c] == point_id)
                state.cloak_deleted[r][c] = true;
    state.cloak_broken = true;
}

// Every point of a column starts at its pin, so the whole cloak is created on
// the player's empty tile and falls into shape from there.
static void create_cloak(player_state player) {
    body b = (body){0};
    PHYSICS_get_body(player.body_id, &b);
    for (Uint8 r = 0; r <= data.cloak_vert_segments; r++) {
        for (Uint8 c = 0; c <= data.cloak_horiz_segments; c++) {
            SDL_FPoint pin = get_cloak_pin(player.facing, b, c);
            prop_point p = (prop_point){
                .x = pin.x,
                .y = pin.y,
                .acc_y = data.cloak_gravity,
                .damping = data.cloak_damping,
                .pinned = r == 0,
                .ignore_bodies = true,
                .on_delete_func = on_cloak_point_deleted,
            };
            PHYSICS_new_prop_point(p, &state.cloak[r][c]);
            state.cloak_deleted[r][c] = false;
            state.cloak_under_fluid[r][c] = false;
            if (c > 0)
                PHYSICS_connect_prop_points(state.cloak[r][c - 1],
                                            state.cloak[r][c],
                                            0.0f,
                                            data.cloak_horiz_segment_length,
                                            data.cloak_horiz_segment_two_ways);
            if (r > 0)
                PHYSICS_connect_prop_points(state.cloak[r - 1][c],
                                            state.cloak[r][c],
                                            0.0f,
                                            data.cloak_vert_segment_length,
                                            data.cloak_vert_segment_two_ways);
        }
    }
    state.cloak_broken = false;
}

static void delete_cloak(void) {
    for (Uint8 r = 0; r <= data.cloak_vert_segments; r++)
        for (Uint8 c = 0; c <= data.cloak_horiz_segments; c++)
            if (!state.cloak_deleted[r][c])
                PHYSICS_del_prop_point(state.cloak[r][c]);
}

// The top row is pinned, so only the points below it check for fluids.
static void update_cloak(player_state player, body b) {
    if (state.cloak_broken) {
        delete_cloak();
        create_cloak(player);
    }
    for (Uint8 r = 1; r <= data.cloak_vert_segments; r++)
        for (Uint8 c = 0; c <= data.cloak_horiz_segments; c++)
            state.cloak_under_fluid[r][c] =
                check_fluid(state.cloak[r][c],
                            state.cloak_under_fluid[r][c],
                            data.cloak_gravity,
                            data.cloak_fluid_gravity,
                            data.cloak_damping,
                            data.cloak_fluid_damping);
    for (Uint8 c = 0; c <= data.cloak_horiz_segments; c++) {
        prop_point p = (prop_point){0};
        PHYSICS_get_prop_point(state.cloak[0][c], &p);
        SDL_FPoint pin = get_cloak_pin(player.facing, b, c);
        p.x = pin.x;
        p.y = pin.y;
        PHYSICS_edit_prop_point(state.cloak[0][c], p);
    }
}

static SDL_Point get_pixel(size_t point_id) {
    prop_point p = (prop_point){0};
    PHYSICS_get_prop_point(point_id, &p);
    return (SDL_Point){(int)SDL_round(p.x), (int)SDL_round(p.y)};
}

// Cross product of `b - a` and `c - a`, positive when `a,b,c` go clockwise on
// screen since y points down, negative when counter-clockwise and zero when
// they're on a line.
static int cross(SDL_Point a, SDL_Point b, SDL_Point c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

// Attaches the line from pixel `a` to pixel `b` to the player's sprite.
static void attach_edge(SDL_Point a, SDL_Point b, SDL_FColor col) {
    SPRITES_attach_line_to_sprite(state.sprite_id, a.x, a.y, b.x, b.y, col);
}

// Attaches the triangle with corners on pixels `a,b,c` to the player's sprite,
// filled by the GPU through the pixels' centers, then its edges as lines, so
// pixels on them are filled like pixels inside it and thin triangles almost
// edge on still show up as lines.
static void
attach_triangle(SDL_Point a, SDL_Point b, SDL_Point c, SDL_FColor col) {
    SPRITES_attach_triangle_to_sprite(state.sprite_id,
                                      a.x + 0.5f,
                                      a.y + 0.5f,
                                      b.x + 0.5f,
                                      b.y + 0.5f,
                                      c.x + 0.5f,
                                      c.y + 0.5f,
                                      col);
    attach_edge(a, b, col);
    attach_edge(b, c, col);
    attach_edge(c, a, col);
}

// Each square of the grid is split into two triangles going clockwise at rest,
// like a 3D model. A triangle still going clockwise shows the cloak's inner
// side, which faces the camera at rest, and one that got flipped to
// counter-clockwise by twisting shows its outer side.
static void attach_triangle_facing(SDL_Point a, SDL_Point b, SDL_Point c) {
    attach_triangle(
        a, b, c, cross(a, b, c) >= 0 ? data.cloak_in : data.cloak_out);
}

static void attach_cloak(void) {
    for (Uint8 r = 0; r < data.cloak_vert_segments; r++) {
        for (Uint8 c = 0; c < data.cloak_horiz_segments; c++) {
            SDL_Point tl = get_pixel(state.cloak[r][c]);
            SDL_Point tr = get_pixel(state.cloak[r][c + 1]);
            SDL_Point bl = get_pixel(state.cloak[r + 1][c]);
            SDL_Point br = get_pixel(state.cloak[r + 1][c + 1]);
            attach_triangle_facing(tl, tr, bl);
            attach_triangle_facing(tr, br, bl);
        }
    }
}

// Attaches the hair to the player's sprite as a thick line along its chain,
// `HAIR_BASE_WIDTH` pixels wide at its pin narrowing down to one pixel at its
// end. Each point gets a left and right side perpendicular to the chain's
// direction around it, and each segment is the quad between the sides of its
// two points, split into two triangles.
static void attach_hair(void) {
    Uint8 n = data.hair_segments + 1;
    SDL_FPoint at[PLAYER_HAIR_MAX_SEGMENTS + 1];
    for (Uint8 i = 0; i < n; i++) {
        prop_point p = (prop_point){0};
        PHYSICS_get_prop_point(state.hair[i], &p);
        at[i] = (SDL_FPoint){p.x, p.y};
    }

    SDL_Point l[PLAYER_HAIR_MAX_SEGMENTS + 1];
    SDL_Point r[PLAYER_HAIR_MAX_SEGMENTS + 1];
    for (Uint8 i = 0; i < n; i++) {
        SDL_FPoint from = at[i > 0 ? i - 1 : 0];
        SDL_FPoint to = at[i + 1 < n ? i + 1 : n - 1];
        float dx = to.x - from.x;
        float dy = to.y - from.y;
        float len = SDL_sqrtf(dx * dx + dy * dy);
        float px = len > EPSILON ? -dy / len : 1.0f;
        float py = len > EPSILON ? dx / len : 0.0f;
        float t = n > 1 ? (float)i / (n - 1) : 1.0f;
        float half = (HAIR_BASE_WIDTH - 1) / 2.0f * (1.0f - t);
        l[i] = (SDL_Point){(int)SDL_roundf(at[i].x + px * half),
                           (int)SDL_roundf(at[i].y + py * half)};
        r[i] = (SDL_Point){(int)SDL_roundf(at[i].x - px * half),
                           (int)SDL_roundf(at[i].y - py * half)};
    }
    for (Uint8 i = 0; i + 1 < n; i++) {
        attach_triangle(l[i], r[i], l[i + 1], data.hair_col);
        attach_triangle(r[i], r[i + 1], l[i + 1], data.hair_col);
    }
}

static void attach_props(void) {
    SPRITES_clear_attached_shapes(state.sprite_id);
    prop_point pin = (prop_point){0};
    PHYSICS_get_prop_point(state.hair[0], &pin);
    bool hair_up = true;
    for (Uint8 i = 1; i <= data.hair_segments; i++) {
        prop_point p = (prop_point){0};
        PHYSICS_get_prop_point(state.hair[i], &p);
        if (p.y >= pin.y) hair_up = false;
    }
    if (hair_up) {
        attach_cloak();
        attach_hair();
    } else {
        attach_hair();
        attach_cloak();
    }
}

static void update_states_machine(player_state player, body player_body) {
    states_machine_state new_state = state.state;
    bool is_on_floor = false;
    bool is_on_ceiling = false;
    PHYSICS_is_body_on_floor(player.body_id, &is_on_floor);
    PHYSICS_is_body_on_ceiling(player.body_id, &is_on_ceiling);

    if (player.swimming)
        new_state = state_swimming;
    else if (is_on_floor && !is_on_ceiling &&
             SDL_abs(player_body.vel_x) < EPSILON)
        new_state = state_idle;
    else if (is_on_floor && !is_on_ceiling)
        new_state = state_moving;
    else if (is_on_floor && is_on_ceiling &&
             SDL_abs(player_body.vel_x) < EPSILON)
        new_state = state_crouch_idle;
    else if (is_on_floor && is_on_ceiling)
        new_state = state_crouch_moving;
    else if (!is_on_floor)
        new_state = state_airborne;

    if (new_state != state.state) {
        state.state = new_state;
        state_changed(new_state);
    }
}



#define PINS_MAX                                                               \
    (SPRITESHEET_X / SPRITESHEET_CELL_X * SPRITESHEET_Y / SPRITESHEET_CELL_Y)

// Fields of the frames of `anim`, on the rows `<name>_anim_x`, `<name>_anim_y`
// and `<name>_anim_dur`, each column a frame.
#define ANIM_FIELDS(name, anim)                                                \
    {name "_anim_x",                                                           \
     parsing_uint8,                                                            \
     &(anim).frames[0].coords_x,                                               \
     .count = SPRITES_ANIM_MAX_KEYFRAMES,                                      \
     .stride = sizeof(keyframe)},                                              \
        {name "_anim_y",                                                       \
         parsing_uint8,                                                        \
         &(anim).frames[0].coords_y,                                           \
         .count = SPRITES_ANIM_MAX_KEYFRAMES,                                  \
         .stride = sizeof(keyframe)},                                          \
    {                                                                          \
        name "_anim_dur", parsing_double, &(anim).frames[0].duration,          \
            .count = SPRITES_ANIM_MAX_KEYFRAMES, .stride = sizeof(keyframe)    \
    }

// Field of the list on row `key`, written on `field` of each element of the
// array `arr`.
#define PINS_FIELD(key, type, arr, field)                                      \
    {key, type, &(arr)[0].field, .count = PINS_MAX, .stride = sizeof((arr)[0])}

// Each column of the hair's pin rows maps a frame of the sprite to its pin.
static void load_hair_pins(dynarr* parsed) {
    static struct {
        Uint8 anim_x, anim_y;
        SDL_Point pin;
    } pins[PINS_MAX];
    parsing_field fields[] = {
        PINS_FIELD("hair_anim_x", parsing_uint8, pins, anim_x),
        PINS_FIELD("hair_anim_y", parsing_uint8, pins, anim_y),
        PINS_FIELD("hair_pin_x", parsing_int, pins, pin.x),
        PINS_FIELD("hair_pin_y", parsing_int, pins, pin.y),
    };
    PARSING_read_keys(parsed, fields, SDL_arraysize(fields));
    size_t n = PARSING_count_values(parsed, "hair_anim_x");
    for (size_t i = 0; i < n; i++)
        data.hair_pins[pins[i].anim_x][pins[i].anim_y] =
            (hair_pin){.mapped = true, .pin = pins[i].pin};
}

// Each column of the cloak's pin rows maps a frame of the sprite to its pins.
static void load_cloak_pins(dynarr* parsed) {
    static struct {
        Uint8 anim_x, anim_y;
        SDL_Point l, r;
    } pins[PINS_MAX];
    parsing_field fields[] = {
        PINS_FIELD("cloak_anim_x", parsing_uint8, pins, anim_x),
        PINS_FIELD("cloak_anim_y", parsing_uint8, pins, anim_y),
        PINS_FIELD("cloak_pin_l_x", parsing_int, pins, l.x),
        PINS_FIELD("cloak_pin_l_y", parsing_int, pins, l.y),
        PINS_FIELD("cloak_pin_r_x", parsing_int, pins, r.x),
        PINS_FIELD("cloak_pin_r_y", parsing_int, pins, r.y),
    };
    PARSING_read_keys(parsed, fields, SDL_arraysize(fields));
    size_t n = PARSING_count_values(parsed, "cloak_anim_x");
    for (size_t i = 0; i < n; i++)
        data.cloak_pins[pins[i].anim_x][pins[i].anim_y] =
            (cloak_pins){.mapped = true, .l = pins[i].l, .r = pins[i].r};
}

void PLAYER_PRESENTATION_start(void) {
    char csv[] = {
#embed PLAYER_PRESENTATION_CSV_PATH suffix(, '\0')
    };

    sprite_data* d = &data;
    parsing_field fields[] = {
        {"coords", parsing_uint8, &d->coords_x},
        {"coords", parsing_uint8, &d->coords_y, .at = 1},
        {"coords", parsing_uint8, &d->coords_w, .at = 2},
        {"coords", parsing_uint8, &d->coords_h, .at = 3},
        {"offset", parsing_sint8, &d->offset_x},
        {"offset", parsing_sint8, &d->offset_y, .at = 1},
        {"hair_col", parsing_color, &d->hair_col},
        {"hair_gravity", parsing_double, &d->hair_gravity},
        {"hair_fluid_gravity", parsing_double, &d->hair_fluid_gravity},
        {"hair_damping", parsing_double, &d->hair_damping},
        {"hair_fluid_damping", parsing_double, &d->hair_fluid_damping},
        {"hair_segments", parsing_uint8, &d->hair_segments},
        {"hair_segments_length", parsing_double, &d->hair_segments_length},
        {"hair_segments_two_ways", parsing_bool, &d->hair_segments_two_ways},
        {"cloak_in_col", parsing_color, &d->cloak_in},
        {"cloak_out_col", parsing_color, &d->cloak_out},
        {"cloak_horiz_segments", parsing_uint8, &d->cloak_horiz_segments},
        {"cloak_vert_segments", parsing_uint8, &d->cloak_vert_segments},
        {"cloak_horiz_segment_length",
         parsing_double,
         &d->cloak_horiz_segment_length},
        {"cloak_vert_segment_length",
         parsing_double,
         &d->cloak_vert_segment_length},
        {"cloak_horiz_segment_two_ways",
         parsing_bool,
         &d->cloak_horiz_segment_two_ways},
        {"cloak_vert_segment_two_ways",
         parsing_bool,
         &d->cloak_vert_segment_two_ways},
        {"cloak_damping", parsing_double, &d->cloak_damping},
        {"cloak_gravity", parsing_double, &d->cloak_gravity},
        {"cloak_fluid_gravity", parsing_double, &d->cloak_fluid_gravity},
        {"cloak_fluid_damping", parsing_double, &d->cloak_fluid_damping},
        ANIM_FIELDS("idle", d->idle_anim),
        ANIM_FIELDS("moving", d->moving_anim),
        ANIM_FIELDS("airborne", d->airborne_anim),
        ANIM_FIELDS("crouch_idle", d->crouch_idle_anim),
        ANIM_FIELDS("crouch_moving", d->crouch_moving_anim),
        ANIM_FIELDS("swimming", d->swimming_anim),
    };
    dynarr parsed = CSV_parse(csv);
    PARSING_read_keys(&parsed, fields, SDL_arraysize(fields));
    load_hair_pins(&parsed);
    load_cloak_pins(&parsed);
    CSV_free(&parsed);

    d->idle_anim.looped = true;
    d->moving_anim.looped = true;
    d->airborne_anim.looped = true;
    d->crouch_idle_anim.looped = true;
    d->crouch_moving_anim.looped = true;
    d->swimming_anim.looped = true;

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Player sprite data loaded successfully.");
}

void PLAYER_PRESENTATION_create(player_state player) {
    SPRITES_new_sprite(
        (sprite){
            .coords_x = data.coords_x,
            .coords_y = data.coords_y,
            .coords_w = data.coords_w,
            .coords_h = data.coords_h,
            .tint_r = 1.0,
            .tint_g = 1.0,
            .tint_b = 1.0,
            .tint_a = 1.0,
        },
        &state.sprite_id);
    state.hair_pin = data.hair_pins[data.coords_x][data.coords_y];
    state.cloak_pins = data.cloak_pins[data.coords_x][data.coords_y];
    create_hair(player);
    create_cloak(player);
    state.created = true;
    state_changed(state.state);
}

void PLAYER_PRESENTATION_destroy(void) {
    SPRITES_del_sprite(state.sprite_id);
    delete_hair();
    delete_cloak();
    state.created = false;
}



void PLAYER_PRESENTATION_update(player_state player) {
    if (!state.created) return;

    sprite s = (sprite){0};
    body b = (body){0};
    PHYSICS_get_body(player.body_id, &b);
    SPRITES_get_sprite(state.sprite_id, &s);

    s.pos_x = (Uint32)SDL_roundf(b.x) + data.offset_x;
    s.pos_y = (Uint32)SDL_roundf(b.y) + data.offset_y;
    s.flip_mode = player.facing == 1 ? SDL_FLIP_NONE : SDL_FLIP_HORIZONTAL;

    SPRITES_edit_sprite(state.sprite_id, s);

    update_hair(player, b);
    update_cloak(player, b);
    attach_props();
    update_states_machine(player, b);
}
