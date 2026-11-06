#include "engine/bgs.h"
#include "data-structs/dynarr.h"
#include "data-structs/hashmap.h"
#include "defs.h"
#include "tests.h"
#include "tests/bgs.h"
#include <SDL3/SDL.h>
#include <stdio.h>

// On tests, `bgs_track_cave` is the mock on `tests/paths.h`, 1s long and
// looping from 50ms to 100ms.
#define CAVE_LOOP_START 2205
#define CAVE_LOOP_END 4410
#define TICK_MS 16

static bool is_near(float a, float b) {
    return SDL_fabsf(a - b) < EPSILON;
}

// Frames queued on the track's stream, at the mock's 2 channels of floats.
static int queued_frames(bgs_track track) {
    int bytes = SDL_GetAudioStreamQueued(BGS_get_track_stream(track));
    return bytes / (int)(2 * sizeof(float));
}

static void update_times(int times) {
    for (int i = 0; i < times; i++)
        BGS_update();
}

// Whether the tracks playing are exactly the `n` `expected` ones, in order.
static bool are_playing(size_t n, const bgs_track* expected) {
    dynarr playing = BGS_get_playing_tracks();
    bool same = playing.len == n;
    for (size_t i = 0; same && i < n; i++)
        same = *(bgs_track*)dynarr_at(i, &playing) == expected[i];
    deinit_dynarr(&playing);
    return same;
}

void test_bgs(void) {
    printf(YELLOW "\nTesting BGS...\n" RESET);

    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    BGS_start();
    SDL_AudioDeviceID music = BGS_get_bus_device(bgs_bus_music);
    SDL_AudioDeviceID ambience = BGS_get_bus_device(bgs_bus_ambience);
    SDL_AudioStream* cave = BGS_get_track_stream(bgs_track_cave);
    expect("Started BGS on the dummy driver, checking if each bus got its own "
           "logical device at full volume and the cave track's stream is on "
           "the ambience one...",
           7,
           (bool[]){music != 0,
                    ambience != 0,
                    music != ambience,
                    is_near(SDL_GetAudioDeviceGain(music), 1.0f),
                    is_near(SDL_GetAudioDeviceGain(ambience), 1.0f),
                    cave != NULL,
                    SDL_GetAudioStreamDevice(cave) == ambience});

    Uint32 loop_start = 0;
    Uint32 loop_end = 0;
    BGS_get_track_loop(bgs_track_cave, &loop_start, &loop_end);
    hashmap names = BGS_get_string_tracks_map();
    bgs_track* named_cave = get_from_hashmap("cave", &names);
    bgs_track* named_bad = get_from_hashmap("not a track", &names);
    expect("Checking if the cave track's loop points were loaded from its CSV "
           "and its name maps to it...",
           5,
           (bool[]){loop_start == CAVE_LOOP_START,
                    loop_end == CAVE_LOOP_END,
                    named_cave && *named_cave == bgs_track_cave,
                    named_bad == NULL,
                    names.len == BGS_TRACKS_TOTAL - 1});
    deinit_hashmap(&names);



    const int buffer = BGS_BUFFER_MS * 44100 / 1000;
    const int max_fill = BGS_MAX_FILL_MS * 44100 / 1000;

    // With the bus paused nothing is heard, so what's queued is exactly what
    // was decoded.
    BGS_pause_bus(bgs_bus_ambience);
    bool none_before = are_playing(0, NULL);
    BGS_play_track(bgs_track_cave);
    bool playing = BGS_is_track_playing(bgs_track_cave);
    bool listed = are_playing(1, (bgs_track[]){bgs_track_cave});
    int before = queued_frames(bgs_track_cave);
    BGS_update();
    int first = queued_frames(bgs_track_cave);
    BGS_update();
    int second = queued_frames(bgs_track_cave);
    expect("Played the cave track on its paused bus, checking if it's playing, "
           "listed as playing, and each update decoded at most the max "
           "fill...",
           6,
           (bool[]){none_before,
                    playing,
                    listed,
                    before == 0,
                    first == max_fill,
                    second == 2 * max_fill});

    update_times(buffer / max_fill + 2);
    int full = queued_frames(bgs_track_cave);
    update_times(10);
    int still_full = queued_frames(bgs_track_cave);
    expect(
        "Updated enough times to fill the buffer, checking if it filled it, "
        "looping since it's longer than the loop, and stopped there...",
        3,
        (bool[]){buffer > CAVE_LOOP_END, full == buffer, still_full == buffer});

    BGS_stop_track(bgs_track_cave);
    expect("Stopped the track, checking if it's not playing, not listed and "
           "has nothing queued...",
           3,
           (bool[]){!BGS_is_track_playing(bgs_track_cave),
                    are_playing(0, NULL),
                    queued_frames(bgs_track_cave) == 0});
    BGS_resume_bus(bgs_bus_ambience);



    BGS_play_track(bgs_track_cave);
    update_times(buffer / max_fill + 2);
    SDL_Delay(BGS_BUFFER_MS / 2);
    int left = queued_frames(bgs_track_cave);
    expect("Filled the buffer on the unpaused bus and stalled for half of it "
           "without updates, checking if the track kept playing from what was "
           "buffered...",
           3,
           (bool[]){
               BGS_is_track_playing(bgs_track_cave), left > 0, left < buffer});

    Uint64 start = SDL_GetTicks();
    while (SDL_GetTicks() - start < 1500) {
        BGS_update();
        SDL_Delay(TICK_MS);
    }
    expect("Kept updating in real time for longer than the whole file, "
           "checking if it's still playing on its loop...",
           1,
           (bool[]){BGS_is_track_playing(bgs_track_cave)});
    BGS_stop_track(bgs_track_cave);

    BGS_play_track(bgs_track_none);
    BGS_stop_track(bgs_track_none);
    BGS_set_track_volume(bgs_track_none, 0.5f);
    BGS_fade_track(bgs_track_none, 0.5f, 1.0, true);
    expect("Called every track function with no track, checking if they did "
           "nothing...",
           3,
           (bool[]){!BGS_is_track_playing(bgs_track_none),
                    is_near(BGS_get_track_volume(bgs_track_none), 0.0f),
                    are_playing(0, NULL)});



    BGS_pause_bus(bgs_bus_ambience);
    float initial = BGS_get_track_volume(bgs_track_cave);
    BGS_set_track_volume(bgs_track_cave, 0.25f);
    float set = BGS_get_track_volume(bgs_track_cave);
    float gain = SDL_GetAudioStreamGain(cave);
    BGS_set_track_volume(bgs_track_cave, -1.0f);
    expect("Set the cave track's volume, checking if it started at full "
           "volume, its stream's gain changed and negatives are silence...",
           4,
           (bool[]){is_near(initial, 1.0f),
                    is_near(set, 0.25f),
                    is_near(gain, 0.25f),
                    is_near(BGS_get_track_volume(bgs_track_cave), 0.0f)});

    // Tests update at 60 ticks per second.
    BGS_set_track_volume(bgs_track_cave, 1.0f);
    BGS_play_track(bgs_track_cave);
    BGS_fade_track(bgs_track_cave, 0.0f, 0.5, true);
    update_times(15);
    float halfway = BGS_get_track_volume(bgs_track_cave);
    bool halfway_playing = BGS_is_track_playing(bgs_track_cave);
    update_times(16);
    expect("Faded the playing track out over 0.5s, stopping at the end, "
           "checking its volume halfway through and if it stopped once "
           "silent...",
           5,
           (bool[]){halfway_playing,
                    is_near(halfway, 0.5f),
                    is_near(BGS_get_track_volume(bgs_track_cave), 0.0f),
                    is_near(SDL_GetAudioStreamGain(cave), 0.0f),
                    !BGS_is_track_playing(bgs_track_cave)});

    BGS_play_track(bgs_track_cave);
    BGS_fade_track(bgs_track_cave, 0.75f, 0.0, false);
    BGS_update();
    expect("Faded the track in over 0s without stopping, checking if it got "
           "there on the next update and kept playing...",
           2,
           (bool[]){is_near(BGS_get_track_volume(bgs_track_cave), 0.75f),
                    BGS_is_track_playing(bgs_track_cave)});

    BGS_fade_track(bgs_track_cave, 0.0f, 1.0, true);
    update_times(10);
    BGS_set_track_volume(bgs_track_cave, 0.5f);
    update_times(60);
    expect("Set the track's volume mid fade out, checking if it cancelled the "
           "fade and its stop...",
           2,
           (bool[]){is_near(BGS_get_track_volume(bgs_track_cave), 0.5f),
                    BGS_is_track_playing(bgs_track_cave)});
    BGS_stop_track(bgs_track_cave);
    BGS_resume_bus(bgs_bus_ambience);



    BGS_pause_bus(bgs_bus_music);
    expect("Paused the music bus, checking if only its device got paused...",
           4,
           (bool[]){BGS_is_bus_paused(bgs_bus_music),
                    !BGS_is_bus_paused(bgs_bus_ambience),
                    SDL_AudioDevicePaused(music),
                    !SDL_AudioDevicePaused(ambience)});
    BGS_resume_bus(bgs_bus_music);

    BGS_set_bus_volume(bgs_bus_ambience, 0.5f);
    expect("Set the ambience bus's volume, checking if only its device's gain "
           "changed and the track's stream kept its own...",
           5,
           (bool[]){is_near(BGS_get_bus_volume(bgs_bus_ambience), 0.5f),
                    is_near(BGS_get_bus_volume(bgs_bus_music), 1.0f),
                    is_near(SDL_GetAudioDeviceGain(ambience), 0.5f),
                    is_near(SDL_GetAudioDeviceGain(music), 1.0f),
                    is_near(SDL_GetAudioStreamGain(cave), 0.5f)});

    BGS_set_bus_volume(bgs_bus_music, -1.0f);
    expect("Set a negative volume on the music bus, checking if it's taken as "
           "silence...",
           2,
           (bool[]){is_near(BGS_get_bus_volume(bgs_bus_music), 0.0f),
                    is_near(SDL_GetAudioDeviceGain(music), 0.0f)});

    BGS_stop();
    expect("Stopped BGS, checking if the buses' devices were closed and the "
           "track's stream destroyed...",
           3,
           (bool[]){BGS_get_bus_device(bgs_bus_music) == 0,
                    BGS_get_bus_device(bgs_bus_ambience) == 0,
                    BGS_get_track_stream(bgs_track_cave) == NULL});
}
