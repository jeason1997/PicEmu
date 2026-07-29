#include "circuit_config.h"
#include "hex_loader.h"
#include "sdl_circuit.h"

#include <SDL2/SDL.h>

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

enum {
    WINDOW_WIDTH = 960,
    WINDOW_HEIGHT = 600
};

typedef struct {
    SDL_atomic_t enabled;
    float phase;
    float phase_step;
} AudioState;

static void audio_callback(void *userdata, Uint8 *stream, int length)
{
    AudioState *audio = userdata;
    float *samples = (float *)stream;
    int count = length / (int)sizeof(float);
    int i;
    bool enabled = SDL_AtomicGet(&audio->enabled) != 0;

    for (i = 0; i < count; ++i) {
        samples[i] = enabled
            ? (sinf(audio->phase) >= 0.0f ? 0.18f : -0.18f) : 0.0f;
        audio->phase += audio->phase_step;
        if (audio->phase >= 6.28318530718f) {
            audio->phase -= 6.28318530718f;
        }
    }
}

int main(int argc, char **argv)
{
    CircuitConfig config;
    HexImage image;
    SdlCircuit circuit;
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_AudioDeviceID audio_device;
    SDL_AudioSpec requested = {0};
    AudioState audio = {0};
    char error[256];
    const char *firmware;
    bool running = true;
    bool quit = false;
    Uint64 last_counter;
    Uint64 next_frame_counter;
    Uint64 counter_frequency;
    double cycle_fraction = 0.0;
    const double cycles_per_second = 1000000.0;
    const double frames_per_second = 60.0;

    if (argc < 2 || argc > 3) {
        fprintf(stderr, "用法：%s circuit.json [firmware.hex]\n", argv[0]);
        return EXIT_FAILURE;
    }
    if (!circuit_config_load(argv[1], &config, error, sizeof(error))) {
        fprintf(stderr, "加载电路失败：%s\n", error);
        return EXIT_FAILURE;
    }
    firmware = argc == 3 ? argv[2] : config.firmware;
    if (firmware[0] == '\0') {
        fprintf(stderr, "电路配置没有firmware，命令行也未指定HEX\n");
        return EXIT_FAILURE;
    }
    if (!hex_load_file(firmware, &image, error, sizeof(error))) {
        fprintf(stderr, "加载HEX失败：%s\n", error);
        return EXIT_FAILURE;
    }
    if (!sdl_circuit_init(&circuit, &config, &image,
                          error, sizeof(error))) {
        fprintf(stderr, "创建电路失败：%s\n", error);
        return EXIT_FAILURE;
    }
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "SDL初始化失败：%s\n", SDL_GetError());
        return EXIT_FAILURE;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    window = SDL_CreateWindow("PicEmu SDL - virtual circuit",
                              SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED,
                              WINDOW_WIDTH, WINDOW_HEIGHT,
                              SDL_WINDOW_SHOWN |
                              SDL_WINDOW_RESIZABLE |
                              SDL_WINDOW_ALLOW_HIGHDPI);
    renderer = window != NULL
        ? SDL_CreateRenderer(window, -1,
                            SDL_RENDERER_ACCELERATED |
                            SDL_RENDERER_PRESENTVSYNC)
        : NULL;
    if (window != NULL && renderer == NULL) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (window == NULL || renderer == NULL ||
        SDL_RenderSetLogicalSize(renderer, WINDOW_WIDTH, WINDOW_HEIGHT) != 0) {
        fprintf(stderr, "SDL窗口创建失败：%s\n", SDL_GetError());
        if (renderer != NULL) SDL_DestroyRenderer(renderer);
        if (window != NULL) SDL_DestroyWindow(window);
        SDL_Quit();
        return EXIT_FAILURE;
    }
    SDL_SetWindowMinimumSize(window, 480, 300);

    requested.freq = 48000;
    requested.format = AUDIO_F32SYS;
    requested.channels = 1;
    requested.samples = 512;
    requested.callback = audio_callback;
    requested.userdata = &audio;
    audio.phase_step = 6.28318530718f * 2000.0f / 48000.0f;
    audio_device = SDL_OpenAudioDevice(NULL, 0, &requested, NULL, 0);
    if (audio_device != 0) SDL_PauseAudioDevice(audio_device, 0);

    counter_frequency = SDL_GetPerformanceFrequency();
    last_counter = SDL_GetPerformanceCounter();
    next_frame_counter = last_counter;
    while (!quit) {
        SDL_Event event;
        Uint64 now;
        double elapsed;
        unsigned cycles_to_run = 0;
        unsigned i;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                quit = true;
            } else if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
                if (event.key.keysym.sym == SDLK_ESCAPE) quit = true;
                else if (event.key.keysym.sym == SDLK_SPACE)
                    running = !running;
                else if (event.key.keysym.sym == SDLK_n && !running)
                    sdl_circuit_step(&circuit);
                else if (event.key.keysym.sym == SDLK_r)
                    sdl_circuit_reset(&circuit);
            } else if (event.type == SDL_MOUSEBUTTONDOWN &&
                       event.button.button == SDL_BUTTON_LEFT) {
                float x, y;
                SDL_RenderWindowToLogical(renderer, event.button.x,
                                          event.button.y, &x, &y);
                sdl_circuit_mouse(&circuit, (int)x, (int)y, true);
            } else if (event.type == SDL_MOUSEBUTTONUP &&
                       event.button.button == SDL_BUTTON_LEFT) {
                sdl_circuit_mouse(&circuit, 0, 0, false);
            }
        }

        now = SDL_GetPerformanceCounter();
        elapsed = (double)(now - last_counter) / (double)counter_frequency;
        last_counter = now;
        if (running) {
            cycle_fraction += elapsed * cycles_per_second;
            cycles_to_run = (unsigned)cycle_fraction;
            cycle_fraction -= cycles_to_run;
            if (cycles_to_run > 100000) {
                cycles_to_run = 100000;
                cycle_fraction = 0;
            }
        }
        for (i = 0; i < cycles_to_run && !circuit.cpu.stopped; ++i) {
            sdl_circuit_step(&circuit);
        }

        SDL_AtomicSet(&audio.enabled,
                      sdl_circuit_buzzer_active(&circuit) ? 1 : 0);
        sdl_circuit_render(renderer, &circuit, running);

        next_frame_counter +=
            (Uint64)((double)counter_frequency / frames_per_second);
        now = SDL_GetPerformanceCounter();
        if (next_frame_counter > now) {
            Uint64 remaining = next_frame_counter - now;
            Uint32 delay_ms =
                (Uint32)(remaining * 1000U / counter_frequency);
            if (delay_ms > 0) SDL_Delay(delay_ms);
        } else if (now - next_frame_counter > counter_frequency / 4U) {
            next_frame_counter = now;
        }
    }

    if (audio_device != 0) SDL_CloseAudioDevice(audio_device);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return EXIT_SUCCESS;
}
