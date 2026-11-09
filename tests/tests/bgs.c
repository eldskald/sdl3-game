#include "engine/bgs.h"
#include "data-structs/dynarr.h"
#include "data-structs/hashmap.h"
#include "defs.h"
#include "engine/audio.h"
#include "tests.h"
#include "tests/bgs.h"
#include <SDL3/SDL.h>
#include <stdio.h>

// On tests, every track is the mock on `tests/paths.h`, 1s long. Per the mock
// CSV, `bgs_track_cave` loops from 50ms to 100ms and stops when silent, and
// `bgs_track_water_streaming` loops its whole file and doesn't.
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
    AUDIO_start();
    BGS_start();
    SDL_AudioDeviceID ambience =
        AUDIO_get_bus_device(audio_bus_ambience_tracks);
    SDL_AudioDeviceID gameplay =
        AUDIO_get_bus_device(audio_bus_gameplay_sounds);
    SDL_AudioStream* cave = BGS_get_track_stream(bgs_track_cave);
    SDL_AudioStream* water = BGS_get_track_stream(bgs_track_water_streaming);
    SDL_AudioStream* lava = BGS_get_track_stream(bgs_track_lava_bubbling);
    expect("Started audio and BGS on the dummy driver, checking if the cave "
           "track's stream is on the ambience tracks bus and the fluids' ones "
           "on the gameplay sounds bus...",
           5,
           (bool[]){ambience != 0,
                    gameplay != 0,
                    cave && SDL_GetAudioStreamDevice(cave) == ambience,
                    water && SDL_GetAudioStreamDevice(water) == gameplay,
                    lava && SDL_GetAudioStreamDevice(lava) == gameplay});

    Uint32 loop_start = 0;
    Uint32 loop_end = 0;
    BGS_get_track_loop(bgs_track_cave, &loop_start, &loop_end);
    hashmap names = BGS_get_string_tracks_map();
    bgs_track* named_cave = get_from_hashmap("cave", &names);
    bgs_track* named_bad = get_from_hashmap("not a track", &names);
    expect("Checking if the cave track's loop points and stopping when silent "
           "were loaded from its CSV and its name maps to it...",
           7,
           (bool[]){loop_start == CAVE_LOOP_START,
                    loop_end == CAVE_LOOP_END,
                    BGS_get_track_stops_when_silent(bgs_track_cave),
                    !BGS_get_track_stops_when_silent(bgs_track_water_streaming),
                    named_cave && *named_cave == bgs_track_cave,
                    named_bad == NULL,
                    names.len == BGS_TRACKS_TOTAL - 1});
    deinit_hashmap(&names);



    const int buffer = BGS_BUFFER_MS * 44100 / 1000;
    const int max_fill = BGS_MAX_FILL_MS * 44100 / 1000;

    // With the bus paused nothing is heard, so what's queued is exactly what
    // was decoded.
    AUDIO_pause_bus(audio_bus_ambience_tracks);
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
    AUDIO_resume_bus(audio_bus_ambience_tracks);



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
    BGS_fade_track(bgs_track_none, 0.5f, 1.0f);
    expect("Called every track function with no track, checking if they did "
           "nothing...",
           3,
           (bool[]){!BGS_is_track_playing(bgs_track_none),
                    is_near(BGS_get_track_volume(bgs_track_none), 0.0f),
                    are_playing(0, NULL)});



    AUDIO_pause_bus(audio_bus_ambience_tracks);
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
    BGS_fade_track(bgs_track_cave, 0.0f, 2.0f);
    update_times(15);
    float halfway = BGS_get_track_volume(bgs_track_cave);
    bool halfway_playing = BGS_is_track_playing(bgs_track_cave);
    update_times(16);
    expect("Faded the playing track out at 2 per second, checking its volume "
           "halfway through and if it stopped once silent, since it's set "
           "to...",
           5,
           (bool[]){halfway_playing,
                    is_near(halfway, 0.5f),
                    is_near(BGS_get_track_volume(bgs_track_cave), 0.0f),
                    is_near(SDL_GetAudioStreamGain(cave), 0.0f),
                    !BGS_is_track_playing(bgs_track_cave)});

    BGS_play_track(bgs_track_cave);
    BGS_fade_track(bgs_track_cave, 0.75f, 0.0f);
    BGS_update();
    expect("Faded the track in at a rate of 0, checking if it got there on the "
           "next update and kept playing...",
           2,
           (bool[]){is_near(BGS_get_track_volume(bgs_track_cave), 0.75f),
                    BGS_is_track_playing(bgs_track_cave)});

    BGS_fade_track(bgs_track_cave, 0.0f, 1.5f);
    update_times(15);
    BGS_fade_track(bgs_track_cave, 1.0f, 1.5f);
    update_times(10);
    float turned = BGS_get_track_volume(bgs_track_cave);
    update_times(60);
    expect("Faded the track out and retargeted it to full volume midway, "
           "checking if it turned around from where it was at the same rate "
           "and stopped at its target...",
           3,
           (bool[]){is_near(turned, 0.625f),
                    is_near(BGS_get_track_volume(bgs_track_cave), 1.0f),
                    BGS_is_track_playing(bgs_track_cave)});

    BGS_fade_track(bgs_track_cave, 0.0f, 1.0f);
    update_times(10);
    BGS_set_track_volume(bgs_track_cave, 0.5f);
    update_times(60);
    expect("Set the track's volume mid fade out, checking if it cancelled the "
           "fade...",
           2,
           (bool[]){is_near(BGS_get_track_volume(bgs_track_cave), 0.5f),
                    BGS_is_track_playing(bgs_track_cave)});

    BGS_set_track_volume(bgs_track_cave, 0.0f);
    expect("Set the track's volume to 0, checking if it stopped, since it's "
           "set to...",
           1,
           (bool[]){!BGS_is_track_playing(bgs_track_cave)});
    BGS_set_track_volume(bgs_track_cave, 0.5f);

    BGS_set_track_volume(bgs_track_water_streaming, 1.0f);
    BGS_play_track(bgs_track_water_streaming);
    BGS_fade_track(bgs_track_water_streaming, 0.0f, 4.0f);
    update_times(30);
    bool silent_playing = BGS_is_track_playing(bgs_track_water_streaming);
    BGS_set_track_volume(bgs_track_water_streaming, 0.0f);
    expect(
        "Faded and set to 0 the water streaming track, checking if it kept "
        "playing silently, since it's not set to stop...",
        3,
        (bool[]){silent_playing,
                 is_near(BGS_get_track_volume(bgs_track_water_streaming), 0.0f),
                 BGS_is_track_playing(bgs_track_water_streaming)});
    BGS_stop_track(bgs_track_water_streaming);
    BGS_stop_track(bgs_track_cave);
    AUDIO_resume_bus(audio_bus_ambience_tracks);



    AUDIO_set_bus_volume(audio_bus_ambience_tracks, 0.5f);
    expect("Set the ambience tracks bus's volume, checking if the cave "
           "track's stream kept its own...",
           2,
           (bool[]){is_near(SDL_GetAudioDeviceGain(ambience), 0.5f),
                    is_near(SDL_GetAudioStreamGain(cave), 0.5f)});
    AUDIO_set_bus_volume(audio_bus_ambience_tracks, 1.0f);

    BGS_stop();
    expect("Stopped BGS, checking if the tracks' streams were destroyed...",
           3,
           (bool[]){BGS_get_track_stream(bgs_track_cave) == NULL,
                    BGS_get_track_stream(bgs_track_water_streaming) == NULL,
                    BGS_get_track_stream(bgs_track_lava_bubbling) == NULL});
    AUDIO_stop();
}
