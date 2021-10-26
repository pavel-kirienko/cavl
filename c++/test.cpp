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
#include <sstream>

void setUp() {}

void tearDown() {}

namespace
{
/// These aliases are introduced to keep things nicely aligned in test cases.
constexpr auto Zzzzz  = nullptr;
constexpr auto Zzzzzz = nullptr;

/// Simple test -- fully static type with private inheritance.
class My : public cavl::Node<My>
{
public:
    explicit My(const std::uint16_t v) : value(v) {}
    using cavl::Node<My>::TreeType;
    using cavl::Node<My>::getChildNode;
    using cavl::Node<My>::getParentNode;
    using cavl::Node<My>::getBalanceFactor;

    [[nodiscard]] auto getValue() const -> std::uint16_t { return value; }

private:
    std::uint16_t value = 0;

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
static_assert(std::is_same_v<cavl::Node<My>, MyTree::NodeType>);

/// Ensure that polymorphic types can be used with the tree. The tree node type itself is not polymorphic!
class V : public cavl::Node<V>
{
public:
    using cavl::Node<V>::TreeType;
    using cavl::Node<V>::getChildNode;
    using cavl::Node<V>::getParentNode;
    using cavl::Node<V>::getBalanceFactor;

    V()          = default;
    virtual ~V() = default;
    V(const V&)  = delete;
    V(V&&)       = delete;
    V& operator=(const V&) = delete;
    V& operator=(V&&) = delete;

    [[nodiscard]] virtual auto getValue() const -> std::uint16_t = 0;

private:
    using E = struct
    {};
    [[maybe_unused]] E up;
    [[maybe_unused]] E lr;
    [[maybe_unused]] E bf;
};
using VTree = cavl::Tree<V>;
static_assert(std::is_same_v<V::TreeType, VTree>);
static_assert(std::is_same_v<cavl::Node<V>, VTree::NodeType>);

template <std::uint16_t Value>
class VValue : public VValue<Value - 1>
{
    [[nodiscard]] auto getValue() const -> std::uint16_t override { return VValue<Value - 1>::getValue() + 1; }
};
template <>
class VValue<0> : public V
{
    [[nodiscard]] auto getValue() const -> std::uint16_t override { return 0; }
};

template <typename T>
using N = typename cavl::Node<T>::DerivedType;
static_assert(std::is_same_v<My, N<My>>);

template <typename T>
[[nodiscard]] bool checkLinkage(const N<T>* const           self,
                                const N<T>* const           up,
                                const std::array<N<T>*, 2>& lr,
                                const std::int8_t           bf)
{
    return (self->getParentNode() == up) &&                                                      //
           (self->getChildNode(false) == lr.at(0)) && (self->getChildNode(true) == lr.at(1)) &&  //
           (self->getBalanceFactor() == bf) &&                                                   //
           ((up == nullptr) || (up->getChildNode(false) == self) || (up->getChildNode(true) == self)) &&
           ((lr.at(0) == nullptr) || (lr.at(0)->getParentNode() == self)) &&  //
           ((lr.at(1) == nullptr) || (lr.at(1)->getParentNode() == self));
}

template <typename T>
[[nodiscard]] auto getHeight(const N<T>* const n) -> std::int8_t
{
    return (n != nullptr) ? static_cast<std::int8_t>(1 + std::max(getHeight<T>(n->getChildNode(false)),  //
                                                                  getHeight<T>(n->getChildNode(true))))
                          : 0;
}

template <typename T>
[[nodiscard]] std::optional<std::size_t> checkOrdering(const N<T>* const root)
{
    const N<T>* prev  = nullptr;
    bool        valid = true;
    std::size_t size  = 0;
    cavl::Node<T>::traverse(root, [&](const N<T>& nd) {
        if (prev != nullptr)
        {
            valid = valid && (prev->getValue() < nd.getValue());
        }
        prev = &nd;
        size++;
    });
    return valid ? std::optional<std::size_t>(size) : std::optional<std::size_t>{};
}

template <typename T>
[[nodiscard]] const N<T>* findBrokenAncestry(const N<T>* const n, const N<T>* const parent = nullptr)
{
    if ((n != nullptr) && (n->getParentNode() == parent))
    {
        for (bool v : {true, false})
        {
            if (const N<T>* p = findBrokenAncestry<T>(n->getChildNode(v), n))
            {
                return p;
            }
        }
        return nullptr;
    }
    return n;
}

template <typename T>
[[nodiscard]] const N<T>* findBrokenBalanceFactor(const N<T>* const n)
{
    if (n != nullptr)
    {
        if (std::abs(n->getBalanceFactor()) > 1)
        {
            return n;
        }
        if (n->getBalanceFactor() != (getHeight<T>(n->getChildNode(true)) - getHeight<T>(n->getChildNode(false))))
        {
            return n;
        }
        for (bool v : {true, false})
        {
            if (auto* const ch = n->getChildNode(v))
            {
                if (auto* const p = findBrokenBalanceFactor<T>(ch))
                {
                    return p;
                }
            }
        }
    }
    return nullptr;
}

template <typename T>
[[nodiscard]] auto toGraphviz(const cavl::Tree<T>& tr) -> std::string
{
    std::ostringstream ss;
    ss << "// Feed the following text to Graphviz, or use an online UI like https://edotor.net/\n"
       << "digraph {\n"
       << "node[style=filled,shape=circle,fontcolor=white,penwidth=0,fontname=\"monospace\",fixedsize=1,fontsize=18];\n"
       << "edge[arrowhead=none,penwidth=2];\n"
       << "nodesep=0.0;ranksep=0.3;splines=false;\n";
    tr.traverse([&](const typename cavl::Tree<T>::DerivedType& x) {
        const char* const fill_color =
            (x.getBalanceFactor() == 0) ? "black" : ((x.getBalanceFactor() > 0) ? "orange" : "blue");
        ss << x.getValue() << "[fillcolor=" << fill_color << "];";
    });
    ss << "\n";
    tr.traverse([&](const typename cavl::Tree<T>::DerivedType& x) {
        if (const auto* const ch = x.getChildNode(false))
        {
            ss << x.getValue() << ":sw->" << ch->getValue() << ":n;";
        }
        if (const auto* const ch = x.getChildNode(true))
        {
            ss << x.getValue() << ":se->" << ch->getValue() << ":n;";
        }
    });
    ss << "\n}";
    return ss.str();
}

template <typename N>
void testManual()
{
    using TreeType = typename N::TreeType;
    std::vector<N> t;
    // Build a tree with 31 elements from 1 to 31 inclusive by adding new elements successively:
    //                               16
    //                       /               `
    //               8                              24
    //           /        `                      /       `
    //       4              12              20              28
    //     /    `         /    `          /    `          /    `
    //   2       6      10      14      18      22      26      30
    //  / `     / `     / `     / `     / `     / `     / `     / `
    // 1   3   5   7   9  11  13  15  17  19  21  23  25  27  29  31
    for (std::uint8_t i = 0; i < 32; i++)
    {
        t.emplace_back(i);
    }
    // Build the actual tree.
    TreeType tr;
    TEST_ASSERT(tr.empty());
    for (std::uint8_t i = 1; i < 32; i++)
    {
        const auto pred = [&](const N& v) { return t[i].getValue() - v.getValue(); };
        TEST_ASSERT_NULL(tr.search(pred));
        TEST_ASSERT_NULL(static_cast<const TreeType&>(tr).search(pred));
        TEST_ASSERT_EQUAL(&t[i], tr.search(pred, [&]() { return &t[i]; }));
        TEST_ASSERT_EQUAL(&t[i], tr.search(pred));
        TEST_ASSERT_EQUAL(&t[i], static_cast<const TreeType&>(tr).search(pred));
        // Validate the tree after every mutation.
        TEST_ASSERT(!tr.empty());
        TEST_ASSERT_EQUAL(i, tr.size());
        TEST_ASSERT_NULL(findBrokenBalanceFactor<N>(tr));
        TEST_ASSERT_NULL(findBrokenAncestry<N>(tr));
        TEST_ASSERT_EQUAL(i, checkOrdering<N>(tr));
    }
    std::cout << toGraphviz(tr) << std::endl;
    TEST_ASSERT_NULL(findBrokenBalanceFactor<N>(tr));
    TEST_ASSERT_NULL(findBrokenAncestry<N>(tr));
    TEST_ASSERT_EQUAL(31, checkOrdering<N>(tr));
    // Check composition -- ensure that every element is in the tree and it is there exactly once.
    {
        bool seen[32]{};
        tr.traverse([&](const N& n) {
            TEST_ASSERT_FALSE(seen[n.getValue()]);
            seen[n.getValue()] = true;
        });
        TEST_ASSERT(std::all_of(&seen[1], &seen[31], [](bool x) { return x; }));
    }
    TEST_ASSERT_EQUAL(&t.at(1), tr.min());
    TEST_ASSERT_EQUAL(&t.at(31), tr.max());
    TEST_ASSERT_EQUAL(&t.at(1), static_cast<const TreeType&>(tr).min());
    TEST_ASSERT_EQUAL(&t.at(31), static_cast<const TreeType&>(tr).max());

    // REMOVE 24
    //                               16
    //                       /               `
    //               8                              25
    //           /        `                      /       `
    //       4              12              20              28
    //     /    `         /    `          /    `          /    `
    //   2       6      10      14      18      22      26      30
    //  / `     / `     / `     / `     / `     / `       `     / `
    // 1   3   5   7   9  11  13  15  17  19  21  23      27  29  31
    std::puts("REMOVE 24");
    TEST_ASSERT(checkLinkage<N>(&t[24], &t[16], {&t[20], &t[28]}, 00));
    tr.remove(&t[24]);
    TEST_ASSERT_NULL(t[24].getParentNode());  // Ensure everything has been reset.
    TEST_ASSERT_NULL(t[24].getChildNode(false));
    TEST_ASSERT_NULL(t[24].getChildNode(true));
    TEST_ASSERT_EQUAL(0, t[24].getBalanceFactor());
    TEST_ASSERT_EQUAL(&t[16], static_cast<N*>(tr));
    TEST_ASSERT(checkLinkage<N>(&t[25], &t[16], {&t[20], &t[28]}, 00));
    TEST_ASSERT(checkLinkage<N>(&t[26], &t[28], {Zzzzzz, &t[27]}, +1));
    TEST_ASSERT_NULL(findBrokenBalanceFactor<N>(tr));
    TEST_ASSERT_NULL(findBrokenAncestry<N>(tr));
    TEST_ASSERT_EQUAL(30, checkOrdering<N>(tr));

    // REMOVE 25
    //                               16
    //                       /               `
    //               8                              26
    //           /        `                      /       `
    //       4              12              20              28
    //     /    `         /    `          /    `          /    `
    //   2       6      10      14      18      22      27      30
    //  / `     / `     / `     / `     / `     / `             / `
    // 1   3   5   7   9  11  13  15  17  19  21  23          29  31
    std::puts("REMOVE 25");
    TEST_ASSERT(checkLinkage<N>(&t[25], &t[16], {&t[20], &t[28]}, 00));
    tr.remove(&t[25]);
    TEST_ASSERT_EQUAL(&t[16], static_cast<N*>(tr));
    TEST_ASSERT(checkLinkage<N>(&t[26], &t[16], {&t[20], &t[28]}, 00));
    TEST_ASSERT(checkLinkage<N>(&t[28], &t[26], {&t[27], &t[30]}, +1));
    TEST_ASSERT_NULL(findBrokenBalanceFactor<N>(tr));
    TEST_ASSERT_NULL(findBrokenAncestry<N>(tr));
    TEST_ASSERT_EQUAL(29, checkOrdering<N>(tr));

    // REMOVE 26
    //                               16
    //                       /               `
    //               8                              27
    //           /        `                      /       `
    //       4              12              20              30
    //     /    `         /    `          /    `          /    `
    //   2       6      10      14      18      22      28      31
    //  / `     / `     / `     / `     / `     / `       `
    // 1   3   5   7   9  11  13  15  17  19  21  23      29
    std::puts("REMOVE 26");
    TEST_ASSERT(checkLinkage<N>(&t[26], &t[16], {&t[20], &t[28]}, 00));
    tr.remove(&t[26]);
    TEST_ASSERT_EQUAL(&t[16], static_cast<N*>(tr));
    TEST_ASSERT(checkLinkage<N>(&t[27], &t[16], {&t[20], &t[30]}, 00));
    TEST_ASSERT(checkLinkage<N>(&t[30], &t[27], {&t[28], &t[31]}, -1));
    TEST_ASSERT(checkLinkage<N>(&t[28], &t[30], {Zzzzzz, &t[29]}, +1));
    TEST_ASSERT_NULL(findBrokenBalanceFactor<N>(tr));
    TEST_ASSERT_NULL(findBrokenAncestry<N>(tr));
    TEST_ASSERT_EQUAL(28, checkOrdering<N>(tr));

    // REMOVE 20
    //                               16
    //                       /               `
    //               8                              27
    //           /        `                      /       `
    //       4              12              21              30
    //     /    `         /    `          /    `          /    `
    //   2       6      10      14      18      22      28      31
    //  / `     / `     / `     / `     / `       `       `
    // 1   3   5   7   9  11  13  15  17  19      23      29
    std::puts("REMOVE 20");
    TEST_ASSERT(checkLinkage<N>(&t[20], &t[27], {&t[18], &t[22]}, 00));
    tr.remove(&t[20]);
    TEST_ASSERT_EQUAL(&t[16], static_cast<N*>(tr));
    TEST_ASSERT(checkLinkage<N>(&t[21], &t[27], {&t[18], &t[22]}, 00));
    TEST_ASSERT(checkLinkage<N>(&t[22], &t[21], {Zzzzzz, &t[23]}, +1));
    TEST_ASSERT_NULL(findBrokenBalanceFactor<N>(tr));
    TEST_ASSERT_NULL(findBrokenAncestry<N>(tr));
    TEST_ASSERT_EQUAL(27, checkOrdering<N>(tr));

    // REMOVE 27
    //                               16
    //                       /               `
    //               8                              28
    //           /        `                      /       `
    //       4              12              21              30
    //     /    `         /    `          /    `          /    `
    //   2       6      10      14      18      22      29      31
    //  / `     / `     / `     / `     / `       `
    // 1   3   5   7   9  11  13  15  17  19      23
    std::puts("REMOVE 27");
    TEST_ASSERT(checkLinkage<N>(&t[27], &t[16], {&t[21], &t[30]}, 00));
    tr.remove(&t[27]);
    TEST_ASSERT_EQUAL(&t[16], static_cast<N*>(tr));
    TEST_ASSERT(checkLinkage<N>(&t[28], &t[16], {&t[21], &t[30]}, -1));
    TEST_ASSERT(checkLinkage<N>(&t[30], &t[28], {&t[29], &t[31]}, 00));
    TEST_ASSERT_NULL(findBrokenBalanceFactor<N>(tr));
    TEST_ASSERT_NULL(findBrokenAncestry<N>(tr));
    TEST_ASSERT_EQUAL(26, checkOrdering<N>(tr));

    // REMOVE 28
    //                               16
    //                       /               `
    //               8                              29
    //           /        `                      /       `
    //       4              12              21              30
    //     /    `         /    `          /    `               `
    //   2       6      10      14      18      22              31
    //  / `     / `     / `     / `     / `       `
    // 1   3   5   7   9  11  13  15  17  19      23
    std::puts("REMOVE 28");
    TEST_ASSERT(checkLinkage<N>(&t[28], &t[16], {&t[21], &t[30]}, -1));
    tr.remove(&t[28]);
    TEST_ASSERT_EQUAL(&t[16], static_cast<N*>(tr));
    TEST_ASSERT(checkLinkage<N>(&t[29], &t[16], {&t[21], &t[30]}, -1));
    TEST_ASSERT(checkLinkage<N>(&t[30], &t[29], {Zzzzzz, &t[31]}, +1));
    TEST_ASSERT_NULL(findBrokenBalanceFactor<N>(tr));
    TEST_ASSERT_NULL(findBrokenAncestry<N>(tr));
    TEST_ASSERT_EQUAL(25, checkOrdering<N>(tr));

    // REMOVE 29; UNBALANCED TREE BEFORE ROTATION:
    //                               16
    //                       /               `
    //               8                              30
    //           /        `                      /       `
    //       4              12              21              31
    //     /    `         /    `          /    `
    //   2       6      10      14      18      22
    //  / `     / `     / `     / `     / `       `
    // 1   3   5   7   9  11  13  15  17  19      23
    //
    // FINAL STATE AFTER ROTATION:
    //                               16
    //                       /               `
    //               8                              21
    //           /        `                      /       `
    //       4              12              18              30
    //     /    `         /    `          /    `          /    `
    //   2       6      10      14      17      19      22      31
    //  / `     / `     / `     / `                       `
    // 1   3   5   7   9  11  13  15                      23
    std::puts("REMOVE 29");
    TEST_ASSERT(checkLinkage<N>(&t[29], &t[16], {&t[21], &t[30]}, -1));
    tr.remove(&t[29]);
    TEST_ASSERT_EQUAL(&t[16], static_cast<N*>(tr));
    TEST_ASSERT(checkLinkage<N>(&t[21], &t[16], {&t[18], &t[30]}, +1));
    TEST_ASSERT(checkLinkage<N>(&t[18], &t[21], {&t[17], &t[19]}, 00));
    TEST_ASSERT(checkLinkage<N>(&t[30], &t[21], {&t[22], &t[31]}, -1));
    TEST_ASSERT(checkLinkage<N>(&t[22], &t[30], {Zzzzzz, &t[23]}, +1));
    TEST_ASSERT(checkLinkage<N>(&t[16], Zzzzzz, {&t[8], &t[21]}, 00));
    TEST_ASSERT_NULL(findBrokenBalanceFactor<N>(tr));
    TEST_ASSERT_NULL(findBrokenAncestry<N>(tr));
    TEST_ASSERT_EQUAL(24, checkOrdering<N>(tr));

    // REMOVE 8
    //                               16
    //                       /               `
    //               9                              21
    //           /        `                      /       `
    //       4              12              18              30
    //     /    `         /    `          /    `          /    `
    //   2       6      10      14      17      19      22      31
    //  / `     / `       `     / `                       `
    // 1   3   5   7      11  13  15                      23
    std::puts("REMOVE 8");
    TEST_ASSERT(checkLinkage<N>(&t[8], &t[16], {&t[4], &t[12]}, 00));
    tr.remove(&t[8]);
    TEST_ASSERT_EQUAL(&t[16], static_cast<N*>(tr));
    TEST_ASSERT(checkLinkage<N>(&t[9], &t[16], {&t[4], &t[12]}, 00));
    TEST_ASSERT(checkLinkage<N>(&t[10], &t[12], {Zzzzz, &t[11]}, +1));
    TEST_ASSERT_NULL(findBrokenBalanceFactor<N>(tr));
    TEST_ASSERT_NULL(findBrokenAncestry<N>(tr));
    TEST_ASSERT_EQUAL(23, checkOrdering<N>(tr));

    // REMOVE 9
    //                               16
    //                       /               `
    //               10                             21
    //           /        `                      /       `
    //       4              12              18              30
    //     /    `         /    `          /    `          /    `
    //   2       6      11      14      17      19      22      31
    //  / `     / `             / `                       `
    // 1   3   5   7          13  15                      23
    std::puts("REMOVE 9");
    TEST_ASSERT(checkLinkage<N>(&t[9], &t[16], {&t[4], &t[12]}, 00));
    tr.remove(&t[9]);
    TEST_ASSERT_EQUAL(&t[16], static_cast<N*>(tr));
    TEST_ASSERT(checkLinkage<N>(&t[10], &t[16], {&t[4], &t[12]}, 00));
    TEST_ASSERT(checkLinkage<N>(&t[12], &t[10], {&t[11], &t[14]}, +1));
    TEST_ASSERT_NULL(findBrokenBalanceFactor<N>(tr));
    TEST_ASSERT_NULL(findBrokenAncestry<N>(tr));
    TEST_ASSERT_EQUAL(22, checkOrdering<N>(tr));

    // REMOVE 1
    //                               16
    //                       /               `
    //               10                             21
    //           /        `                      /       `
    //       4              12              18              30
    //     /    `         /    `          /    `          /    `
    //   2       6      11      14      17      19      22      31
    //    `     / `             / `                       `
    //     3   5   7          13  15                      23
    std::puts("REMOVE 1");
    TEST_ASSERT(checkLinkage<N>(&t[1], &t[2], {Zzzzz, Zzzzz}, 00));
    tr.remove(&t[1]);
    TEST_ASSERT_EQUAL(&t[16], static_cast<N*>(tr));
    TEST_ASSERT(checkLinkage<N>(&t[2], &t[4], {Zzzzz, &t[3]}, +1));
    TEST_ASSERT_NULL(findBrokenBalanceFactor<N>(tr));
    TEST_ASSERT_NULL(findBrokenAncestry<N>(tr));
    TEST_ASSERT_EQUAL(21, checkOrdering<N>(tr));

    // Print the final state for manual inspection. Be sure to compare it against the above diagram for extra paranoia.
    std::cout << toGraphviz(tr) << std::endl;
    TEST_ASSERT(checkLinkage<N>(&t[16], Zzzzzz, {&t[10], &t[21]}, 00));
    TEST_ASSERT(checkLinkage<N>(&t[10], &t[16], {&t[+4], &t[12]}, 00));
    TEST_ASSERT(checkLinkage<N>(&t[21], &t[16], {&t[18], &t[30]}, +1));
    TEST_ASSERT(checkLinkage<N>(&t[+4], &t[10], {&t[+2], &t[+6]}, 00));
    TEST_ASSERT(checkLinkage<N>(&t[12], &t[10], {&t[11], &t[14]}, +1));
    TEST_ASSERT(checkLinkage<N>(&t[18], &t[21], {&t[17], &t[19]}, 00));
    TEST_ASSERT(checkLinkage<N>(&t[30], &t[21], {&t[22], &t[31]}, -1));
}

}  // namespace

int main(const int argc, const char* const argv[])
{
    const auto seed = static_cast<unsigned>((argc > 1) ? std::atoll(argv[1]) : std::time(nullptr));  // NOLINT
    std::cout << "Randomness seed: " << seed << std::endl;
    std::srand(seed);
    UNITY_BEGIN();
    RUN_TEST(testManual<My>);
    return UNITY_END();
}
