#include "Particle.h"

void Particle::tick(f64 dt) {
    x += vx * dt;
    y += vy * dt;
    age += dt;
    if (age >= duration) dead = true;
}
