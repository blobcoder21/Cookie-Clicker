#include "Game.h"
#include "Render.h"
#include "Audio.h"
#include "Assets.h"

#include <SDL2/SDL.h>
#include <chrono>
#include <iostream>

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Cookie Clicker (C++ port)", 100, 100, 900, 600,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) {
        std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    Render::init(window);
    Audio::init();
    Assets::init();

    Game game;
    game.launch();

    bool running = true;
    SDL_Event event;
    auto last = std::chrono::steady_clock::now();
    const f64 targetDt = 1.0 / static_cast<f64>(GameConst::FPS);
    f64 accumulator = 0.0;

    while (running) {
        while (SDL_PollEvent(&event)) {
            // TODO: ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) running = false;
        }

        auto now = std::chrono::steady_clock::now();
        f64 frameDt = std::chrono::duration<f64>(now - last).count();
        last = now;
        accumulator += frameDt;

        // fixed-timestep logic, mirroring the JS setTimeout loop
        while (accumulator >= targetDt) {
            game.loop();
            accumulator -= targetDt;
        }

        Render::newFrame();
        game.draw();
        Render::render();

        // simple frame cap
        SDL_Delay(1);
    }

    Render::shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
