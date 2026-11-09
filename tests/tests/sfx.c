#include "engine/sfx.h"
#include "defs.h"
#include "engine/audio.h"
#include "engine/camera.h"
#include "tests.h"
#include "tests/sfx.h"
#include <SDL3/SDL.h>
#include <stdio.h>

// 50ms of 16-bit stereo at 44100Hz, short enough to wait for it to play. It
// replaces no game file, so it's not on `tests/paths.h`.
#define MOCK_WAV_PATH "tests/mocks/audio/short.wav"

// The dummy driver plays in real time, so sounds take as long as they are.
#define WAIT_TIMEOUT_MS 1000
#define WAIT_STEP_MS 5
#define FALLOFF 160.0 // Of the 2D sounds, in pixels

static const Uint8 wav_bytes[] = {
#embed MOCK_WAV_PATH
};

// Waits for the voice to finish playing, returning whether it did before
// `WAIT_TIMEOUT_MS`.
static bool wait_for_voice(size_t voice_id) {
    Uint64 end = SDL_GetTicks() + WAIT_TIMEOUT_MS;
    bool playing = true;
    while (SDL_GetTicks() < end) {
        SFX_is_voice_playing(voice_id, &playing);
        if (!playing) return true;
        SDL_Delay(WAIT_STEP_MS);
    }
    return false;
}

static bool is_near(float a, float b) {
    return SDL_fabsf(a - b) < EPSILON;
}

void test_sfx(void) {
    printf(YELLOW "\nTesting SFX...\n" RESET);

    size_t early_id = 0;
    int early = SFX_new_voice(audio_bus_gameplay_sounds, &early_id);
    expect("Created a voice before starting SFX, checking the error...",
           1,
           (bool[]){early == 2});

    // The dummy driver has no hardware behind it but consumes what's played
    // on it as a real device would.
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    AUDIO_start();
    SFX_start();
    SDL_AudioDeviceID gameplay =
        AUDIO_get_bus_device(audio_bus_gameplay_sounds);
    SDL_AudioDeviceID gui = AUDIO_get_bus_device(audio_bus_gui_sounds);
    expect("Started audio and SFX on the dummy driver, checking if the "
           "gameplay and GUI sounds buses got their devices...",
           3,
           (bool[]){gameplay != 0, gui != 0, gameplay != gui});

    audio_wav* wav = AUDIO_create_wav(
        "test", SDL_IOFromConstMem(wav_bytes, sizeof(wav_bytes)), true);
    expect("Loaded a WAV of 50ms, checking its format and length...",
           5,
           (bool[]){wav != NULL,
                    wav && wav->spec.format == SDL_AUDIO_S16,
                    wav && wav->spec.channels == 2,
                    wav && wav->spec.freq == 44100,
                    wav && wav->len == 2205 * 4});
    if (!wav) {
        SFX_stop();
        AUDIO_stop();
        return;
    }

    size_t a = 0;
    size_t b = 0;
    int new_a = SFX_new_voice(audio_bus_gameplay_sounds, &a);
    int new_b = SFX_new_voice(audio_bus_gui_sounds, &b);
    SDL_AudioStream* stream_a = SFX_get_voice_stream(a);
    SDL_AudioStream* stream_b = SFX_get_voice_stream(b);
    expect("Created a voice on each bus, checking if their streams are bound "
           "to their bus's device...",
           7,
           (bool[]){new_a == 0,
                    new_b == 0,
                    a != b,
                    stream_a != NULL,
                    stream_b != NULL,
                    SDL_GetAudioStreamDevice(stream_a) == gameplay,
                    SDL_GetAudioStreamDevice(stream_b) == gui});

    bool playing = false;
    SFX_is_voice_playing(a, &playing);
    int play_a = SFX_play_voice(a, wav);
    bool playing_a = false;
    SFX_is_voice_playing(a, &playing_a);
    bool finished_a = wait_for_voice(a);
    expect("Played the WAV on the gameplay voice, checking if it went from "
           "not playing to playing and then finished in time...",
           4,
           (bool[]){!playing, play_a == 0, playing_a, finished_a});



    AUDIO_pause_bus(audio_bus_gameplay_sounds);
    SFX_play_voice(a, wav);
    SFX_play_voice(b, wav);
    expect("Paused the gameplay bus and played the WAV on both voices, "
           "checking if only the gameplay device got paused and the gameplay "
           "voice got the whole WAV queued...",
           5,
           (bool[]){AUDIO_is_bus_paused(audio_bus_gameplay_sounds),
                    !AUDIO_is_bus_paused(audio_bus_gui_sounds),
                    SDL_AudioDevicePaused(gameplay),
                    !SDL_AudioDevicePaused(gui),
                    SDL_GetAudioStreamQueued(stream_a) == (int)wav->len});

    bool finished_b = wait_for_voice(b);
    SFX_is_voice_playing(a, &playing_a);
    expect("Waited for the GUI voice to finish, checking if the gameplay one "
           "is still waiting with all of it queued...",
           3,
           (bool[]){finished_b,
                    playing_a,
                    SDL_GetAudioStreamQueued(stream_a) == (int)wav->len});

    AUDIO_resume_bus(audio_bus_gameplay_sounds);
    finished_a = wait_for_voice(a);
    expect("Resumed the gameplay bus, checking if it's unpaused and its voice "
           "finished playing...",
           3,
           (bool[]){!AUDIO_is_bus_paused(audio_bus_gameplay_sounds),
                    !SDL_AudioDevicePaused(gameplay),
                    finished_a});

    AUDIO_pause_bus(audio_bus_gameplay_sounds);
    SFX_play_voice(a, wav);
    int stop_a = SFX_stop_voice(a);
    SFX_is_voice_playing(a, &playing_a);
    AUDIO_resume_bus(audio_bus_gameplay_sounds);
    expect("Played the WAV on a paused bus and stopped it, checking if it's "
           "not playing with nothing queued...",
           3,
           (bool[]){stop_a == 0,
                    !playing_a,
                    SDL_GetAudioStreamQueued(stream_a) == 0});



    float vol_a = 0.0f;
    float vol_b = 0.0f;
    SFX_get_voice_volume(a, &vol_a);
    expect("Checking if voices start at full volume...",
           2,
           (bool[]){is_near(vol_a, 1.0f),
                    is_near(SDL_GetAudioStreamGain(stream_a), 1.0f)});

    int set_a = SFX_set_voice_volume(a, 0.25f);
    int get_a = SFX_get_voice_volume(a, &vol_a);
    SFX_get_voice_volume(b, &vol_b);
    expect("Set the gameplay voice's volume, checking if only its stream's "
           "gain changed...",
           6,
           (bool[]){set_a == 0,
                    get_a == 0,
                    is_near(vol_a, 0.25f),
                    is_near(SDL_GetAudioStreamGain(stream_a), 0.25f),
                    is_near(vol_b, 1.0f),
                    is_near(SDL_GetAudioStreamGain(stream_b), 1.0f)});

    SFX_play_voice(a, wav);
    SFX_stop_voice(a);
    expect("Played the gameplay voice again, checking if it kept its "
           "volume...",
           1,
           (bool[]){is_near(SDL_GetAudioStreamGain(stream_a), 0.25f)});

    SFX_set_voice_volume(b, -2.0f);
    SFX_get_voice_volume(b, &vol_b);
    expect("Set a negative volume on the GUI voice, checking if it's taken as "
           "silence...",
           2,
           (bool[]){is_near(vol_b, 0.0f),
                    is_near(SDL_GetAudioStreamGain(stream_b), 0.0f)});



    AUDIO_set_bus_volume(audio_bus_gameplay_sounds, 0.5f);
    expect("Set the gameplay sounds bus's volume, checking if only its device "
           "got it while the voices' streams kept theirs...",
           4,
           (bool[]){is_near(SDL_GetAudioDeviceGain(gameplay), 0.5f),
                    is_near(SDL_GetAudioDeviceGain(gui), 1.0f),
                    is_near(SDL_GetAudioStreamGain(stream_a), 0.25f),
                    is_near(SDL_GetAudioStreamGain(stream_b), 0.0f)});
    AUDIO_set_bus_volume(audio_bus_gameplay_sounds, 1.0f);



    int del_b = SFX_del_voice(b);
    bool unchanged = true;
    float unchanged_vol = 0.0f;
    size_t out = 9999;
    expect("Deleted the GUI voice, checking the errors of every voice "
           "function on it and on an id out of range...",
           13,
           (bool[]){del_b == 0,
                    SFX_get_voice_stream(b) == NULL,
                    SFX_del_voice(b) == 1,
                    SFX_play_voice(b, wav) == 1,
                    SFX_stop_voice(b) == 1,
                    SFX_is_voice_playing(b, &unchanged) == 1,
                    SFX_set_voice_volume(b, 1.0f) == 1,
                    SFX_get_voice_volume(b, &unchanged_vol) == 1,
                    SFX_del_voice(out) == 3,
                    SFX_play_voice(out, wav) == 3,
                    SFX_is_voice_playing(out, &unchanged) == 3,
                    SFX_set_voice_volume(out, 1.0f) == 3,
                    unchanged && is_near(unchanged_vol, 0.0f)});

    size_t d = 0;
    SFX_new_voice(audio_bus_gameplay_sounds, &d);
    CAMERA_center_at(WINDOW_SIZE_X, WINDOW_SIZE_Y);
    SDL_FRect cam = CAMERA_get_rect();
    float vol_inside = 0.0f;
    float vol_edge = 0.0f;
    float vol_half = 0.0f;
    float vol_corner = 0.0f;
    float vol_far = 0.0f;
    bool playing_far = true;
    int inside = SFX_play_voice_2d(
        d, wav, cam.x + cam.w / 2, cam.y + cam.h / 2, 0.5f, FALLOFF);
    SFX_get_voice_volume(d, &vol_inside);
    SFX_play_voice_2d(d, wav, cam.x + cam.w, cam.y, 0.5f, FALLOFF);
    SFX_get_voice_volume(d, &vol_edge);
    SFX_play_voice_2d(
        d, wav, cam.x - FALLOFF / 2, cam.y + cam.h / 2, 0.5f, FALLOFF);
    SFX_get_voice_volume(d, &vol_half);
    // 3-4-5 triangle off the bottom right corner, 5 tenths of the falloff away
    SFX_play_voice_2d(d,
                      wav,
                      cam.x + cam.w + FALLOFF * 0.3,
                      cam.y + cam.h + FALLOFF * 0.4,
                      1.0f,
                      FALLOFF);
    SFX_get_voice_volume(d, &vol_corner);
    SFX_stop_voice(d);
    int far = SFX_play_voice_2d(
        d, wav, cam.x + cam.w / 2, cam.y - FALLOFF * 2, 1.0f, FALLOFF);
    SFX_get_voice_volume(d, &vol_far);
    SFX_is_voice_playing(d, &playing_far);
    expect("Played 2D sounds inside, on the edge, outside and far from the "
           "camera, checking their volumes and if the far one was skipped...",
           9,
           (bool[]){inside == 0,
                    is_near(vol_inside, 0.5f),
                    is_near(vol_edge, 0.5f),
                    is_near(vol_half, 0.25f),
                    is_near(vol_corner, 0.5f),
                    far == 0,
                    is_near(vol_far, 0.0f),
                    !playing_far,
                    SFX_play_voice_2d(out, wav, 0.0, 0.0, 1.0f, FALLOFF) == 3});
    SFX_del_voice(d);

    size_t c = 0;
    SFX_new_voice(audio_bus_gameplay_sounds, &c);
    expect("Created a voice after deleting one, checking if it reused the "
           "freed slot on the gameplay bus...",
           2,
           (bool[]){c == b,
                    SDL_GetAudioStreamDevice(SFX_get_voice_stream(c)) ==
                        gameplay});

    SFX_stop();
    size_t late_id = 0;
    int late = SFX_new_voice(audio_bus_gameplay_sounds, &late_id);
    expect("Stopped SFX, checking if its voices are gone and new ones can't "
           "be created...",
           3,
           (bool[]){SFX_get_voice_stream(a) == NULL,
                    SFX_get_voice_stream(c) == NULL,
                    late == 2});
    AUDIO_stop();
    AUDIO_destroy_wav(wav);
}
