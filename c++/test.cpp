/// Copyright (c) 2021 Pavel Kirienko <pavel@uavcan.org>

#include "cavl.hpp"
#include <unity.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <optional>
#include <numeric>
#include <iostream>

void setUp() {}

void tearDown() {}

namespace
{
void testA()
{
    //
}

}  // namespace

int main(const int argc, const char* const argv[])
{
    const auto seed = static_cast<unsigned>((argc > 1) ? std::atoll(argv[1]) : std::time(nullptr));  // NOLINT
    std::cout << "Randomness seed: " << seed << std::endl;
    std::srand(seed);
    UNITY_BEGIN();
    RUN_TEST(testA);
    return UNITY_END();
}
