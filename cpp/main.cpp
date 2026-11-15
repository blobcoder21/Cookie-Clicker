#include "Game.h"
#include "Render.h"
#include "UI.h"
#include "Audio.h"
#include "Assets.h"

#include <SDL2/SDL.h>
#include <chrono>
#include <iostream>
#include <memory>

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    // Desktop profile: 1280x720 resizable window. (UI_PROFILE CMake option,
    // wired in the next commit, selects the handheld DSi profile instead.)
    SDL_Window* window = SDL_CreateWindow(
        "Cookie Clicker (C++ port)", 100, 100, 1280, 720,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) {
        std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    Render render;
    render.init(window);
    Audio::init();
    Assets::init();

    Game game;
    game.launch();
    game.load(); // populate buildings, wrinklers, buff types

    // Default profile is desktop; the UI_PROFILE CMake option selects handheld.
    std::unique_ptr<UI> ui = CreateUI(UIProfile::Desktop, game, render);

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

        while (accumulator >= targetDt) {
            game.loop();
            ui->update(targetDt);
            accumulator -= targetDt;
        }

        render.newFrame();
        ui->draw();
        render.render();

        if (clickPending) {
            GameAction action = ui->on_pointer(clickX, clickY);
            switch (action.type) {
            case GameAction::Type::ClickCookie:
                game.clickCookie();
                break;
            case GameAction::Type::BuyBuilding:
                if (action.buildingIndex >= 0 &&
                    action.buildingIndex < static_cast<i32>(game.objects.size())) {
                    game.buyBuilding(game.objects[static_cast<size_t>(action.buildingIndex)].get());
                }
                break;
            case GameAction::Type::None:
                break;
            case GameAction::Type::ClickFortune:
                game.clickTickerFortune();
                break;
            case GameAction::Type::TogglePref:
                game.togglePref(action.prefName);
                break;
            case GameAction::Type::SaveGame:
                game.toSave = true;
                ui->notify("Game saved");
                break;
            case GameAction::Type::WipeSave:
                game.Reset(true);
                ui->notify("Save wiped");
                break;
            }
        }

        SDL_Delay(1);
    }

    render.shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
