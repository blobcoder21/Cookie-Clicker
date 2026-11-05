#pragma once

#include "Minigame.h"

class Pantheon : public Minigame {
public:
    explicit Pantheon(Building* parent);
    void init() override;
};
