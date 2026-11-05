#pragma once

#include "Types.h"

// Rendering abstraction placeholder (OpenGL via ImGui backend)
class Render {
public:
    static void init(void* window);
    static void newFrame();
    static void render();
    static void shutdown();
};
