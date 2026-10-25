#pragma once
// Deterministic, platform-independent RNG (SplitMix64).
// std::uniform_int_distribution is implementation-defined, so it is NOT used:
// a seed must produce identical behaviour on GCC/MinGW/Clang/MSVC and in the
// Python trace generator (python/trace_generator uses the same algorithm).

#include <cstdint>

namespace ftl {

class Rng {
 public:
  explicit Rng(std::uint64_t seed = 0) : state_(seed) {}

  std::uint64_t next() {
    std::uint64_t z = (state_ += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
  }

  // Uniform integer in [0, bound). bound == 0 returns 0.
  std::uint64_t uniform(std::uint64_t bound) { return bound == 0 ? 0 : next() % bound; }

  // Uniform double in [0, 1).
  double unit() { return static_cast<double>(next() >> 11) * (1.0 / 9007199254740992.0); }

 private:
  std::uint64_t state_;
};

}  // namespace ftl
