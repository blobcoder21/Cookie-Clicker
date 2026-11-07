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
    game.load(); // populate buildings, wrinklers, buff types

    bool running = true;
    SDL_Event event;
    auto last = std::chrono::steady_clock::now();
    const f64 targetDt = 1.0 / static_cast<f64>(GameConst::FPS);
    f64 accumulator = 0.0;

    i32 clickX = 0, clickY = 0;
    bool clickPending = false;

    while (running) {
        clickPending = false;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_MOUSEBUTTONDOWN &&
                       event.button.button == SDL_BUTTON_LEFT) {
                clickX = event.button.x;
                clickY = event.button.y;
                clickPending = true;
            }
        }

        auto now = std::chrono::steady_clock::now();
        f64 frameDt = std::chrono::duration<f64>(now - last).count();
        last = now;
        accumulator += frameDt;

        // fixed-timestep logic, mirroring the JS setTimeout loop (~30 FPS)
        while (accumulator >= targetDt) {
            game.loop();            // logic only; draw happens once per frame below
            accumulator -= targetDt;
        }

        Render::newFrame();
        game.draw();                // draw once per rendered frame
        Render::render();

        // handle input against this frame's clickable areas (cookie / buy buttons)
        if (clickPending) {
            i32 hit = Render::hitTest(clickX, clickY);
            if (hit == Render::HIT_COOKIE) {
                game.clickCookie();
            } else if (hit >= Render::HIT_BUILDING_BASE) {
                i32 idx = hit - Render::HIT_BUILDING_BASE;
                if (idx >= 0 && idx < static_cast<i32>(game.objects.size())) {
                    game.buyBuilding(game.objects[static_cast<size_t>(idx)].get());
                }
            }
        }

        SDL_Delay(1);
    }

    Render::shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
