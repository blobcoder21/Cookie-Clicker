#pragma once

#include "Types.h"
#include <string>
#include <map>

// Image/sound asset loading (placeholder; actual backend loads PNG/MP3)
class Assets {
public:
    static void init();
    static bool hasImage(const std::string& name);
};
