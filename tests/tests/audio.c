#include "engine/audio.h"
#include "defs.h"
#include "tests.h"
#include "tests/audio.h"
#include <SDL3/SDL.h>
#include <stdio.h>

static bool is_near(float a, float b) {
    return SDL_fabsf(a - b) < EPSILON;
}

// Whether every bus has its own device, none of them shared.
static bool are_devices_unique(SDL_AudioDeviceID devices[AUDIO_BUSES_TOTAL]) {
    for (int a = 0; a < AUDIO_BUSES_TOTAL; a++) {
        if (!devices[a]) return false;
        for (int b = a + 1; b < AUDIO_BUSES_TOTAL; b++)
            if (devices[a] == devices[b]) return false;
    }
    return true;
}

void test_audio(void) {
    printf(YELLOW "\nTesting audio...\n" RESET);

    AUDIO_pause_bus(audio_bus_gameplay_sounds);
    expect("Paused a bus before starting audio, checking if it did nothing...",
           2,
           (bool[]){!AUDIO_is_bus_paused(audio_bus_gameplay_sounds),
                    AUDIO_get_bus_device(audio_bus_gameplay_sounds) == 0});

    // The dummy driver has no hardware behind it but consumes what's played
    // on it as a real device would.
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    AUDIO_start();
    SDL_AudioDeviceID devices[AUDIO_BUSES_TOTAL] = {0};
    bool unpaused = true;
    bool full_volume = true;
    for (audio_bus b = 0; b < AUDIO_BUSES_TOTAL; b++) {
        devices[b] = AUDIO_get_bus_device(b);
        unpaused = unpaused && !AUDIO_is_bus_paused(b);
        full_volume = full_volume && is_near(AUDIO_get_bus_volume(b), 1.0f) &&
                      is_near(SDL_GetAudioDeviceGain(devices[b]), 1.0f);
    }
    expect("Started audio on the dummy driver, checking if each bus got its "
           "own logical device, unpaused and at full volume...",
           3,
           (bool[]){are_devices_unique(devices), unpaused, full_volume});

    SDL_AudioDeviceID gameplay = devices[audio_bus_gameplay_sounds];
    SDL_AudioDeviceID gui = devices[audio_bus_gui_sounds];
    SDL_AudioDeviceID ambience = devices[audio_bus_ambience_tracks];
    SDL_AudioDeviceID music = devices[audio_bus_music_tracks];

    AUDIO_pause_bus(audio_bus_gameplay_sounds);
    expect("Paused the gameplay sounds bus, checking if only its device got "
           "paused...",
           5,
           (bool[]){AUDIO_is_bus_paused(audio_bus_gameplay_sounds),
                    SDL_AudioDevicePaused(gameplay),
                    !SDL_AudioDevicePaused(gui),
                    !SDL_AudioDevicePaused(ambience),
                    !SDL_AudioDevicePaused(music)});

    AUDIO_resume_bus(audio_bus_gameplay_sounds);
    expect("Resumed the gameplay sounds bus, checking if it's unpaused...",
           2,
           (bool[]){!AUDIO_is_bus_paused(audio_bus_gameplay_sounds),
                    !SDL_AudioDevicePaused(gameplay)});

    AUDIO_set_bus_volume(audio_bus_ambience_tracks, 0.5f);
    expect(
        "Set the ambience tracks bus's volume, checking if only its "
        "device's gain changed...",
        5,
        (bool[]){is_near(AUDIO_get_bus_volume(audio_bus_ambience_tracks), 0.5f),
                 is_near(SDL_GetAudioDeviceGain(ambience), 0.5f),
                 is_near(SDL_GetAudioDeviceGain(gameplay), 1.0f),
                 is_near(SDL_GetAudioDeviceGain(gui), 1.0f),
                 is_near(SDL_GetAudioDeviceGain(music), 1.0f)});

    AUDIO_set_bus_volume(audio_bus_music_tracks, -1.0f);
    expect("Set a negative volume on the music tracks bus, checking if it's "
           "taken as silence...",
           2,
           (bool[]){is_near(AUDIO_get_bus_volume(audio_bus_music_tracks), 0.0f),
                    is_near(SDL_GetAudioDeviceGain(music), 0.0f)});

    AUDIO_stop();
    bool closed = true;
    for (audio_bus b = 0; b < AUDIO_BUSES_TOTAL; b++)
        closed = closed && AUDIO_get_bus_device(b) == 0;
    expect(
        "Stopped audio, checking if the buses' devices were closed and "
        "their volumes kept...",
        3,
        (bool[]){closed,
                 is_near(AUDIO_get_bus_volume(audio_bus_ambience_tracks), 0.5f),
                 is_near(AUDIO_get_bus_volume(audio_bus_music_tracks), 0.0f)});

    AUDIO_start();
    expect("Started audio again, checking if the devices got the kept "
           "volumes...",
           2,
           (bool[]){is_near(SDL_GetAudioDeviceGain(AUDIO_get_bus_device(
                                audio_bus_ambience_tracks)),
                            0.5f),
                    is_near(SDL_GetAudioDeviceGain(
                                AUDIO_get_bus_device(audio_bus_music_tracks)),
                            0.0f)});
    AUDIO_set_bus_volume(audio_bus_ambience_tracks, 1.0f);
    AUDIO_set_bus_volume(audio_bus_music_tracks, 1.0f);
    AUDIO_stop();
}
