#pragma once

#include "Minigame.h"

class Game;

class Garden : public Minigame {
public:
    explicit Garden(Building* parent);
    void init() override;
    void draw() override;
    void eachFrame() override;
    std::string save() override;
};
