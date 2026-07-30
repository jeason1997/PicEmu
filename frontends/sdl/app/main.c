#include "picemu/sim/circuit_config.h"
#include "picemu/firmware/hex_loader.h"
#include "circuit/sdl_circuit.h"

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
    SDL_atomic_t frequency_hz;
    float phase;
} AudioState;

static void audio_callback(void *userdata, Uint8 *stream, int length)
{
    AudioState *audio = userdata;
    float *samples = (float *)stream;
    int count = length / (int)sizeof(float);
    int i;
    int frequency_hz = SDL_AtomicGet(&audio->frequency_hz);
    float phase_step =
        6.28318530718f * (float)frequency_hz / 48000.0f;

    for (i = 0; i < count; ++i) {
        samples[i] = frequency_hz > 0
            ? (sinf(audio->phase) >= 0.0f ? 0.18f : -0.18f) : 0.0f;
        audio->phase += phase_step;
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
    double cycle_budget = 0.0;
    double cycles_per_second;
    const double frames_per_second = 60.0;
    float zoom = 1.0f;
    int pan_x = 0;
    int pan_y = 0;
    bool panning = false;
    int pan_start_x = 0;
    int pan_start_y = 0;
    int pan_origin_x = 0;
    int pan_origin_y = 0;

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
    if (!(argc == 3
              ? sdl_circuit_init_with_override(
                    &circuit, &config, &image, error, sizeof(error))
              : sdl_circuit_init(
                    &circuit, &config, &image, error, sizeof(error)))) {
        fprintf(stderr, "创建电路失败：%s\n", error);
        return EXIT_FAILURE;
    }
    cycles_per_second = circuit.board.mcu->cycles_per_second;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "SDL初始化失败：%s\n", SDL_GetError());
        sdl_circuit_destroy(&circuit);
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
        sdl_circuit_destroy(&circuit);
        return EXIT_FAILURE;
    }
    SDL_SetWindowMinimumSize(window, 480, 300);

    requested.freq = 48000;
    requested.format = AUDIO_F32SYS;
    requested.channels = 1;
    requested.samples = 512;
    requested.callback = audio_callback;
    requested.userdata = &audio;
    audio_device = SDL_OpenAudioDevice(NULL, 0, &requested, NULL, 0);
    if (audio_device != 0) SDL_PauseAudioDevice(audio_device, 0);

    counter_frequency = SDL_GetPerformanceFrequency();
    last_counter = SDL_GetPerformanceCounter();
    next_frame_counter = last_counter;
    while (!quit) {
        SDL_Event event;
        Uint64 now;
        double elapsed;

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
                else if (event.key.keysym.sym == SDLK_0) {
                    zoom = 1.0f;
                    pan_x = 0;
                    pan_y = 0;
                } else if (event.key.keysym.sym == SDLK_PLUS ||
                           event.key.keysym.sym == SDLK_EQUALS ||
                           event.key.keysym.sym == SDLK_KP_PLUS) {
                    zoom = fminf(3.0f, zoom * 1.2f);
                } else if (event.key.keysym.sym == SDLK_MINUS ||
                           event.key.keysym.sym == SDLK_KP_MINUS) {
                    zoom = fmaxf(0.2f, zoom / 1.2f);
                }
            } else if (event.type == SDL_MOUSEWHEEL) {
                int mouse_x, mouse_y;
                float old_zoom = zoom;
                float world_x;
                float world_y;
                SDL_GetMouseState(&mouse_x, &mouse_y);
                world_x = (mouse_x - pan_x) / old_zoom;
                world_y = (mouse_y - pan_y) / old_zoom;
                zoom = event.wheel.y > 0
                    ? fminf(3.0f, zoom * 1.12f)
                    : fmaxf(0.2f, zoom / 1.12f);
                /* 缩放后保持鼠标指向的世界坐标不动。 */
                pan_x = (int)(mouse_x - world_x * zoom);
                pan_y = (int)(mouse_y - world_y * zoom);
            } else if (event.type == SDL_MOUSEBUTTONDOWN &&
                       event.button.button == SDL_BUTTON_MIDDLE) {
                panning = true;
                pan_start_x = event.button.x;
                pan_start_y = event.button.y;
                pan_origin_x = pan_x;
                pan_origin_y = pan_y;
            } else if (event.type == SDL_MOUSEMOTION && panning) {
                pan_x = pan_origin_x +
                    event.motion.x - pan_start_x;
                pan_y = pan_origin_y +
                    event.motion.y - pan_start_y;
            } else if (event.type == SDL_MOUSEBUTTONUP &&
                       event.button.button == SDL_BUTTON_MIDDLE) {
                panning = false;
            } else if (event.type == SDL_MOUSEBUTTONDOWN &&
                       event.button.button == SDL_BUTTON_LEFT) {
                float x, y;
                SDL_RenderWindowToLogical(renderer, event.button.x,
                                          event.button.y, &x, &y);
                x = (x - pan_x) / zoom;
                y = (y - pan_y) / zoom;
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
            cycle_budget += elapsed * cycles_per_second;
            if (cycle_budget > 100000.0) {
                cycle_budget = 100000.0;
            }
        }
        while (cycle_budget >= 1.0 &&
               !sdl_circuit_all_stopped(&circuit)) {
            unsigned consumed = sdl_circuit_step(&circuit);
            if (consumed == 0) {
                cycle_budget = 0.0;
                break;
            }
            /*
             * 预算单位是PIC指令周期，而不是指令条数。GOTO、CALL以及真正
             * 发生跳过的指令消耗两个周期，必须按返回值扣除。
             */
            cycle_budget -= consumed;
        }

        SDL_AtomicSet(&audio.frequency_hz,
                      (int)(sdl_circuit_buzzer_frequency(&circuit) + 0.5));
        sdl_circuit_render(renderer, &circuit, running,
                           zoom, pan_x, pan_y);

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
    sdl_circuit_destroy(&circuit);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return EXIT_SUCCESS;
}
