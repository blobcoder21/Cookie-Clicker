#pragma once

#include "Types.h"
#include <string>

// Audio backend placeholder (SDL_mixer / miniaudio)
class Audio {
public:
    static void init();
    static void playSound(const std::string& url, float volume = 1.0f);
    static void setVolume(float v);
};
