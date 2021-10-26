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

void testFirst()
{
    using N = My;
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
    N::TreeType tr;
    TEST_ASSERT(tr.empty());
    for (std::uint8_t i = 1; i < 32; i++)
    {
        const auto pred = [&](const N& v) { return t[i].getValue() - v.getValue(); };
        TEST_ASSERT_NULL(tr.search(pred));
        TEST_ASSERT_NULL(static_cast<const N::TreeType&>(tr).search(pred));
        TEST_ASSERT_EQUAL(&t[i], tr.search(pred, [&]() { return &t[i]; }));
        TEST_ASSERT_EQUAL(&t[i], tr.search(pred));
        TEST_ASSERT_EQUAL(&t[i], static_cast<const N::TreeType&>(tr).search(pred));
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
    TEST_ASSERT_EQUAL(&t.at(1), static_cast<const N::TreeType&>(tr).min());
    TEST_ASSERT_EQUAL(&t.at(31), static_cast<const N::TreeType&>(tr).max());
}

}  // namespace

int main(const int argc, const char* const argv[])
{
    const auto seed = static_cast<unsigned>((argc > 1) ? std::atoll(argv[1]) : std::time(nullptr));  // NOLINT
    std::cout << "Randomness seed: " << seed << std::endl;
    std::srand(seed);
    UNITY_BEGIN();
    RUN_TEST(testFirst);
    return UNITY_END();
}
