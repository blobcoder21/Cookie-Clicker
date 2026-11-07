#pragma once

#include <cstdint>
#include <cmath>
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <random>

// ---------------------------------------------------------------------------
// Core numeric types
//
// JavaScript numbers are IEEE-754 doubles, so we use double everywhere to
// preserve behavior exactly (including loss of integer precision above 2^53,
// which the JS game already exhibits).
// ---------------------------------------------------------------------------
using f64 = double;
using f32 = float;
using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;
using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

namespace Math {
    inline f64 clamp(f64 v, f64 lo, f64 hi) { return v < lo ? lo : (v > hi ? hi : v); }
    inline f64 lerp(f64 a, f64 b, f64 t) { return a + (b - a) * t; }
    inline i32 round(i32) = delete; // avoid ambiguity
    inline f64 round_f64(f64 v) { return std::floor(v + 0.5); } // JS Math.round for positives
    inline i32 floor_i32(f64 v) { return static_cast<i32>(std::floor(v)); }
}

// ---------------------------------------------------------------------------
// Wall-clock time (for offline progress, matching Date.now())
// ---------------------------------------------------------------------------
inline i64 now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

inline f64 now_s() {
    return now_ms() / 1000.0;
}

// ---------------------------------------------------------------------------
// Seeded PRNG
//
// JavaScript uses David Bau's seedrandom (ARC4-based) when Game.seed is set.
// To keep deterministic behaviour (dungeons, plant mutations, sugar lumps)
// identical between the JS and C++ builds, port that algorithm here. Until
// then, std::mt19937 seeded from Game.seed is a reasonable substitute.
// ---------------------------------------------------------------------------
class Prng {
public:
    explicit Prng(u32 seed = std::random_device{}()) : rng_(seed) {}
    void seed(u32 s) { rng_.seed(s); }
    // [0, 1) matching Math.random()
    f64 random() { return dist_(rng_); }
    i32 randint(i32 n) { return static_cast<i32>(std::floor(random() * static_cast<f64>(n))); }

private:
    std::mt19937 rng_;
    std::uniform_real_distribution<f64> dist_{0.0, 1.0};
};

// ---------------------------------------------------------------------------
// Common game constants
// ---------------------------------------------------------------------------
namespace GameConst {
    constexpr i32 FPS = 30;
    constexpr f64 PRICE_INCREASE = 1.15;
}
