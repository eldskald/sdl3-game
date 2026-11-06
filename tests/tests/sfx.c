#include "engine/sfx.h"
#include "defs.h"
#include "engine/audio.h"
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

    // The dummy driver has no hardware behind it but consumes what's played
    // on it as a real device would.
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    SFX_start();
    SDL_AudioDeviceID gameplay = SFX_get_bus_device(sfx_bus_gameplay);
    SDL_AudioDeviceID gui = SFX_get_bus_device(sfx_bus_gui);
    expect("Started SFX on the dummy driver, checking if each bus got its own "
           "logical device, unpaused and at full volume...",
           7,
           (bool[]){gameplay != 0,
                    gui != 0,
                    gameplay != gui,
                    !SFX_is_bus_paused(sfx_bus_gameplay),
                    !SFX_is_bus_paused(sfx_bus_gui),
                    is_near(SDL_GetAudioDeviceGain(gameplay), 1.0f),
                    is_near(SDL_GetAudioDeviceGain(gui), 1.0f)});

    static const Uint8 wav_bytes[] = {
#embed MOCK_WAV_PATH
    };
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
        return;
    }

    size_t a = 0;
    size_t b = 0;
    int new_a = SFX_new_voice(sfx_bus_gameplay, &a);
    int new_b = SFX_new_voice(sfx_bus_gui, &b);
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



    SFX_pause_bus(sfx_bus_gameplay);
    SFX_play_voice(a, wav);
    SFX_play_voice(b, wav);
    expect("Paused the gameplay bus and played the WAV on both voices, "
           "checking if only the gameplay device got paused and the gameplay "
           "voice got the whole WAV queued...",
           5,
           (bool[]){SFX_is_bus_paused(sfx_bus_gameplay),
                    !SFX_is_bus_paused(sfx_bus_gui),
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

    SFX_resume_bus(sfx_bus_gameplay);
    finished_a = wait_for_voice(a);
    expect("Resumed the gameplay bus, checking if it's unpaused and its voice "
           "finished playing...",
           3,
           (bool[]){!SFX_is_bus_paused(sfx_bus_gameplay),
                    !SDL_AudioDevicePaused(gameplay),
                    finished_a});

    SFX_pause_bus(sfx_bus_gameplay);
    SFX_play_voice(a, wav);
    int stop_a = SFX_stop_voice(a);
    SFX_is_voice_playing(a, &playing_a);
    SFX_resume_bus(sfx_bus_gameplay);
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



    SFX_set_volume(0.5f);
    expect("Set the global SFX volume, checking if every bus's device got it "
           "while the voices' streams kept theirs...",
           5,
           (bool[]){is_near(SFX_get_volume(), 0.5f),
                    is_near(SDL_GetAudioDeviceGain(gameplay), 0.5f),
                    is_near(SDL_GetAudioDeviceGain(gui), 0.5f),
                    is_near(SDL_GetAudioStreamGain(stream_a), 0.25f),
                    is_near(SDL_GetAudioStreamGain(stream_b), 0.0f)});

    SFX_set_volume(-1.0f);
    expect("Set a negative global SFX volume, checking if it's taken as "
           "silence...",
           3,
           (bool[]){is_near(SFX_get_volume(), 0.0f),
                    is_near(SDL_GetAudioDeviceGain(gameplay), 0.0f),
                    is_near(SDL_GetAudioDeviceGain(gui), 0.0f)});
    SFX_set_volume(1.0f);



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

    size_t c = 0;
    SFX_new_voice(sfx_bus_gameplay, &c);
    expect("Created a voice after deleting one, checking if it reused the "
           "freed slot on the gameplay bus...",
           2,
           (bool[]){c == b,
                    SDL_GetAudioStreamDevice(SFX_get_voice_stream(c)) ==
                        gameplay});

    SFX_stop();
    AUDIO_destroy_wav(wav);
    expect("Stopped SFX, checking if the buses' devices were closed...",
           2,
           (bool[]){SFX_get_bus_device(sfx_bus_gameplay) == 0,
                    SFX_get_bus_device(sfx_bus_gui) == 0});
}
