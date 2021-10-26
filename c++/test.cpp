/// Copyright (c) 2021 Pavel Kirienko <pavel@uavcan.org>

#include "cavl.hpp"
#include <unity.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <optional>
#include <iostream>

void setUp() {}

void tearDown() {}

namespace
{
/// Simple test -- fully static type with private inheritance.
struct My : private cavl::Node<My>
{
    using cavl::Node<My>::TreeType;

    std::uint8_t value = 0;

private:
    // These dummy fields are needed to ensure the node class does not make incorrect references to the fields
    // defined in the derived class. That would trigger compilation error in this case, but may be deadly in the field.
    using E = struct
    {};
    [[maybe_unused]] E up;
    [[maybe_unused]] E lr;
    [[maybe_unused]] E bf;
};
using MyTree = cavl::Tree<My>;
static_assert(std::is_same_v<My::TreeType, MyTree>);

/// Ensure that polymorphic types can be used with the tree. The tree node type itself is not polymorphic!
struct V : public cavl::Node<V>
{
    V()          = default;
    virtual ~V() = default;
    V(const V&)  = delete;
    V(V&&)       = delete;
    V& operator=(const V&) = delete;
    V& operator=(V&&) = delete;

    [[nodiscard]] virtual auto getValue() const -> std::uint8_t = 0;

private:
    using E = struct
    {};
    [[maybe_unused]] E up;
    [[maybe_unused]] E lr;
    [[maybe_unused]] E bf;
};
using VTree = cavl::Tree<V>;
static_assert(std::is_same_v<V::TreeType, VTree>);

template <std::uint8_t Value>
struct VValue : public VValue<Value - 1>
{
    [[nodiscard]] auto getValue() const -> std::uint8_t override { return VValue<Value - 1>::getValue() + 1; }
};
template <>
struct VValue<0> : public V
{
    [[nodiscard]] auto getValue() const -> std::uint8_t override { return 0; }
};

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
