/// Copyright (c) 2021 Pavel Kirienko <pavel@uavcan.org>

#include "cavl.h"
#include <unity.h>
#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <optional>

void setUp() {}

void tearDown() {}

namespace
{
/// These aliases are introduced to keep things nicely aligned in test cases.
constexpr auto Zz     = nullptr;
constexpr auto Zzzzz  = nullptr;
constexpr auto Zzzzzz = nullptr;

void print(const Cavl* const nd, const std::uint8_t depth = 0, const char marker = 'T')
{
    if (nd != nullptr)
    {
        print(nd->lr[0], static_cast<std::uint8_t>(depth + 1U), 'L');
        for (std::uint16_t i = 1U; i < depth; i++)
        {
            std::printf("              ");
        }
        if (marker == 'L')
        {
            std::printf(" .............");
        }
        else if (marker == 'R')
        {
            std::printf(" `````````````");
        }
        else
        {
            (void) 0;
        }
        std::printf("%c=%llu [%d]\n", marker, reinterpret_cast<unsigned long long>(nd->value), nd->bf);
        print(nd->lr[1], static_cast<std::uint8_t>(depth + 1U), 'R');
    }
}

template <bool Ascending, typename Node, typename Visitor>
inline void traverse(Node* const root, const Visitor& visitor)
{
    if (root != nullptr)
    {
        traverse<Ascending, Node, Visitor>(root->lr[!Ascending], visitor);
        visitor(root);
        traverse<Ascending, Node, Visitor>(root->lr[Ascending], visitor);
    }
}

std::optional<std::size_t> checkAscension(const Cavl* const root)
{
    const Cavl* prev  = nullptr;
    bool        valid = true;
    std::size_t size  = 0;
    traverse<true, const Cavl>(root, [&](const Cavl* const nd) {
        if (prev != nullptr)
        {
            valid = valid && (prev->value < nd->value);
        }
        prev = nd;
        size++;
    });
    return valid ? std::optional<std::size_t>(size) : std::optional<std::size_t>{};
}

const Cavl* findBrokenAncestry(const Cavl* const n, const Cavl* const parent = nullptr)
{
    if ((n != nullptr) && (n->up == parent))
    {
        for (auto* ch : n->lr)
        {
            if (const Cavl* p = findBrokenAncestry(ch, n))
            {
                return p;
            }
        }
        return nullptr;
    }
    return n;
}

std::uint8_t getHeight(const Cavl* const n)
{
    return (n != nullptr) ? std::uint8_t(1U + std::max(getHeight(n->lr[0]), getHeight(n->lr[1]))) : 0;
}

const Cavl* findBrokenBalanceFactor(const Cavl* const n)
{
    if (n != nullptr)
    {
        const std::int16_t hl = getHeight(n->lr[0]);
        const std::int16_t hr = getHeight(n->lr[1]);
        if (n->bf != (hr - hl))
        {
            return n;
        }
        for (auto* ch : n->lr)
        {
            if (const Cavl* p = findBrokenBalanceFactor(ch))
            {
                return p;
            }
        }
    }
    return nullptr;
}

void testCheckAscension()
{
    Cavl t{};
    Cavl l{};
    Cavl r{};
    Cavl rr{};
    t.value  = reinterpret_cast<void*>(2);
    l.value  = reinterpret_cast<void*>(1);
    r.value  = reinterpret_cast<void*>(3);
    rr.value = reinterpret_cast<void*>(4);
    // Correctly arranged tree -- smaller items on the left.
    t.lr[0] = &l;
    t.lr[1] = &r;
    r.lr[1] = &rr;
    TEST_ASSERT_EQUAL(4, checkAscension(&t));
    TEST_ASSERT_EQUAL(3, getHeight(&t));
    // Break the arrangement and make sure the breakage is detected.
    t.lr[1] = &l;
    t.lr[0] = &r;
    TEST_ASSERT_NOT_EQUAL(4, checkAscension(&t));
    TEST_ASSERT_EQUAL(3, getHeight(&t));
    TEST_ASSERT_EQUAL(&t, findBrokenBalanceFactor(&t));  // All zeros, incorrect.
    r.lr[1] = nullptr;
    TEST_ASSERT_EQUAL(2, getHeight(&t));
    TEST_ASSERT_NULL(findBrokenBalanceFactor(&t));  // Balanced now as we removed one node.
}

void testRotation()
{
    // Original state:
    //      x.left  = a
    //      x.right = z
    //      z.left  = b
    //      z.right = c
    // After left rotation of X:
    //      x.left  = a
    //      x.right = b
    //      z.left  = x
    //      z.right = c
    Cavl c{reinterpret_cast<void*>(3), Zz, {Zz, Zz}, 0};
    Cavl b{reinterpret_cast<void*>(2), Zz, {Zz, Zz}, 0};
    Cavl a{reinterpret_cast<void*>(1), Zz, {Zz, Zz}, 0};
    Cavl z{reinterpret_cast<void*>(8), Zz, {&b, &c}, 0};
    Cavl x{reinterpret_cast<void*>(9), Zz, {&a, &z}, 1};
    z.up = &x;
    c.up = &z;
    b.up = &z;
    a.up = &x;

    std::printf("Before rotation:\n");
    TEST_ASSERT_NULL(findBrokenAncestry(&x));
    print(&x);

    std::printf("After left rotation:\n");
    TEST_ASSERT_EQUAL(&z, _cavlRotate(&x, false));
    TEST_ASSERT_NULL(findBrokenAncestry(&z));
    print(&z);
    TEST_ASSERT_EQUAL(&a, x.lr[0]);
    TEST_ASSERT_EQUAL(&b, x.lr[1]);
    TEST_ASSERT_EQUAL(&x, z.lr[0]);
    TEST_ASSERT_EQUAL(&c, z.lr[1]);

    std::printf("After right rotation, back into the original configuration:\n");
    TEST_ASSERT_EQUAL(&x, _cavlRotate(&z, true));
    TEST_ASSERT_NULL(findBrokenAncestry(&x));
    print(&x);
    TEST_ASSERT_EQUAL(&a, x.lr[0]);
    TEST_ASSERT_EQUAL(&z, x.lr[1]);
    TEST_ASSERT_EQUAL(&b, z.lr[0]);
    TEST_ASSERT_EQUAL(&c, z.lr[1]);
}

void testBalancingA()
{
    // Double left-right rotation.
    //     X             X           Y
    //    / `           / `        /   `
    //   Z   C   =>    Y   C  =>  Z     X
    //  / `           / `        / `   / `
    // D   Y         Z   G      D   F G   C
    //    / `       / `
    //   F   G     D   F
    Cavl x{reinterpret_cast<void*>(1), Zz, {Zz, Zz}, 0};  // bf = -2
    Cavl z{reinterpret_cast<void*>(2), &x, {Zz, Zz}, 0};  // bf = +1
    Cavl c{reinterpret_cast<void*>(3), &x, {Zz, Zz}, 0};
    Cavl d{reinterpret_cast<void*>(4), &z, {Zz, Zz}, 0};
    Cavl y{reinterpret_cast<void*>(5), &z, {Zz, Zz}, 0};
    Cavl f{reinterpret_cast<void*>(6), &y, {Zz, Zz}, 0};
    Cavl g{reinterpret_cast<void*>(7), &y, {Zz, Zz}, 0};
    x.lr[0] = &z;
    x.lr[1] = &c;
    z.lr[0] = &d;
    z.lr[1] = &y;
    y.lr[0] = &f;
    y.lr[1] = &g;
    print(&x);
    TEST_ASSERT_NULL(findBrokenAncestry(&x));
    TEST_ASSERT_EQUAL(&x, _cavlAdjustBalance(&x, false));  // bf = -1, same topology
    TEST_ASSERT_EQUAL(-1, x.bf);
    TEST_ASSERT_EQUAL(&z, _cavlAdjustBalance(&z, true));  // bf = +1, same topology
    TEST_ASSERT_EQUAL(+1, z.bf);
    TEST_ASSERT_EQUAL(&y, _cavlAdjustBalance(&x, false));  // bf = -2, rotation needed
    print(&y);
    TEST_ASSERT_NULL(findBrokenBalanceFactor(&y));  // Should be balanced now.
    TEST_ASSERT_NULL(findBrokenAncestry(&y));
    TEST_ASSERT_EQUAL(&z, y.lr[0]);
    TEST_ASSERT_EQUAL(&x, y.lr[1]);
    TEST_ASSERT_EQUAL(&d, z.lr[0]);
    TEST_ASSERT_EQUAL(&f, z.lr[1]);
    TEST_ASSERT_EQUAL(&g, x.lr[0]);
    TEST_ASSERT_EQUAL(&c, x.lr[1]);
    TEST_ASSERT_EQUAL(Zz, d.lr[0]);
    TEST_ASSERT_EQUAL(Zz, d.lr[1]);
    TEST_ASSERT_EQUAL(Zz, f.lr[0]);
    TEST_ASSERT_EQUAL(Zz, f.lr[1]);
    TEST_ASSERT_EQUAL(Zz, g.lr[0]);
    TEST_ASSERT_EQUAL(Zz, g.lr[1]);
    TEST_ASSERT_EQUAL(Zz, c.lr[0]);
    TEST_ASSERT_EQUAL(Zz, c.lr[1]);
}

void testBalancingB()
{
    // Without F the handling of Z and Y is more complex; Z flips the sign of its balance factor:
    //     X             X           Y
    //    / `           / `        /   `
    //   Z   C   =>    Y   C  =>  Z     X
    //  / `           / `        /     / `
    // D   Y         Z   G      D     G   C
    //      `       /
    //       G     D
    Cavl x{};
    Cavl z{};
    Cavl c{};
    Cavl d{};
    Cavl y{};
    Cavl g{};
    x = {reinterpret_cast<void*>(1), Zz, {&z, &c}, 0};  // bf = -2
    z = {reinterpret_cast<void*>(2), &x, {&d, &y}, 0};  // bf = +1
    c = {reinterpret_cast<void*>(3), &x, {Zz, Zz}, 0};
    d = {reinterpret_cast<void*>(4), &z, {Zz, Zz}, 0};
    y = {reinterpret_cast<void*>(5), &z, {Zz, &g}, 0};  // bf = +1
    g = {reinterpret_cast<void*>(7), &y, {Zz, Zz}, 0};
    print(&x);
    TEST_ASSERT_NULL(findBrokenAncestry(&x));
    TEST_ASSERT_EQUAL(&x, _cavlAdjustBalance(&x, false));  // bf = -1, same topology
    TEST_ASSERT_EQUAL(-1, x.bf);
    TEST_ASSERT_EQUAL(&z, _cavlAdjustBalance(&z, true));  // bf = +1, same topology
    TEST_ASSERT_EQUAL(+1, z.bf);
    TEST_ASSERT_EQUAL(&y, _cavlAdjustBalance(&y, true));  // bf = +1, same topology
    TEST_ASSERT_EQUAL(+1, y.bf);
    TEST_ASSERT_EQUAL(&y, _cavlAdjustBalance(&x, false));  // bf = -2, rotation needed
    print(&y);
    TEST_ASSERT_NULL(findBrokenBalanceFactor(&y));  // Should be balanced now.
    TEST_ASSERT_NULL(findBrokenAncestry(&y));
    TEST_ASSERT_EQUAL(&z, y.lr[0]);
    TEST_ASSERT_EQUAL(&x, y.lr[1]);
    TEST_ASSERT_EQUAL(&d, z.lr[0]);
    TEST_ASSERT_EQUAL(Zz, z.lr[1]);
    TEST_ASSERT_EQUAL(&g, x.lr[0]);
    TEST_ASSERT_EQUAL(&c, x.lr[1]);
    TEST_ASSERT_EQUAL(Zz, d.lr[0]);
    TEST_ASSERT_EQUAL(Zz, d.lr[1]);
    TEST_ASSERT_EQUAL(Zz, g.lr[0]);
    TEST_ASSERT_EQUAL(Zz, g.lr[1]);
    TEST_ASSERT_EQUAL(Zz, c.lr[0]);
    TEST_ASSERT_EQUAL(Zz, c.lr[1]);
}

void testBalancingC()
{
    // Both X and Z are heavy on the same side.
    //       X              Z
    //      / `           /   `
    //     Z   C   =>    D     X
    //    / `           / `   / `
    //   D   Y         F   G Y   C
    //  / `
    // F   G
    Cavl x{};
    Cavl z{};
    Cavl c{};
    Cavl d{};
    Cavl y{};
    Cavl f{};
    Cavl g{};
    x = {reinterpret_cast<void*>(1), Zz, {&z, &c}, 0};  // bf = -2
    z = {reinterpret_cast<void*>(2), &x, {&d, &y}, 0};  // bf = -1
    c = {reinterpret_cast<void*>(3), &x, {Zz, Zz}, 0};
    d = {reinterpret_cast<void*>(4), &z, {&f, &g}, 0};
    y = {reinterpret_cast<void*>(5), &z, {Zz, Zz}, 0};
    f = {reinterpret_cast<void*>(6), &d, {Zz, Zz}, 0};
    g = {reinterpret_cast<void*>(7), &d, {Zz, Zz}, 0};
    print(&x);
    TEST_ASSERT_NULL(findBrokenAncestry(&x));
    TEST_ASSERT_EQUAL(&x, _cavlAdjustBalance(&x, false));  // bf = -1, same topology
    TEST_ASSERT_EQUAL(-1, x.bf);
    TEST_ASSERT_EQUAL(&z, _cavlAdjustBalance(&z, false));  // bf = -1, same topology
    TEST_ASSERT_EQUAL(-1, z.bf);
    TEST_ASSERT_EQUAL(&z, _cavlAdjustBalance(&x, false));
    print(&z);
    TEST_ASSERT_NULL(findBrokenBalanceFactor(&z));
    TEST_ASSERT_NULL(findBrokenAncestry(&z));
    TEST_ASSERT_EQUAL(&d, z.lr[0]);
    TEST_ASSERT_EQUAL(&x, z.lr[1]);
    TEST_ASSERT_EQUAL(&f, d.lr[0]);
    TEST_ASSERT_EQUAL(&g, d.lr[1]);
    TEST_ASSERT_EQUAL(&y, x.lr[0]);
    TEST_ASSERT_EQUAL(&c, x.lr[1]);
    TEST_ASSERT_EQUAL(Zz, f.lr[0]);
    TEST_ASSERT_EQUAL(Zz, f.lr[1]);
    TEST_ASSERT_EQUAL(Zz, g.lr[0]);
    TEST_ASSERT_EQUAL(Zz, g.lr[1]);
    TEST_ASSERT_EQUAL(Zz, y.lr[0]);
    TEST_ASSERT_EQUAL(Zz, y.lr[1]);
    TEST_ASSERT_EQUAL(Zz, c.lr[0]);
    TEST_ASSERT_EQUAL(Zz, c.lr[1]);
}

void testRetracingOnGrowth()
{
    Cavl t[100]{};
    //        50              30
    //      /   `            /   `
    //     30   60?   =>    20   50
    //    / `              /    /  `
    //   20 40?           10   40? 60?
    //  /
    // 10
    t[50] = {reinterpret_cast<void*>(50), Zzzzzz, {&t[30], &t[60]}, -1};
    t[30] = {reinterpret_cast<void*>(30), &t[50], {&t[20], &t[40]}, 0};
    t[60] = {reinterpret_cast<void*>(60), &t[50], {Zzzzzz, Zzzzzz}, 0};
    t[20] = {reinterpret_cast<void*>(20), &t[30], {&t[10], Zzzzzz}, 0};
    t[40] = {reinterpret_cast<void*>(40), &t[30], {Zzzzzz, Zzzzzz}, 0};
    t[10] = {reinterpret_cast<void*>(10), &t[20], {Zzzzzz, Zzzzzz}, 0};
    print(&t[50]);  // The tree is imbalanced because we just added 1 and are about to retrace it.
    TEST_ASSERT_NULL(findBrokenAncestry(&t[50]));
    TEST_ASSERT_EQUAL(6, checkAscension(&t[50]));
    TEST_ASSERT_EQUAL(&t[30], _cavlRetraceOnGrowth(&t[10]));
    std::puts("ADD 10:");
    print(&t[30]);  // This is the new root.
    TEST_ASSERT_EQUAL(&t[20], t[30].lr[0]);
    TEST_ASSERT_EQUAL(&t[50], t[30].lr[1]);
    TEST_ASSERT_EQUAL(&t[10], t[20].lr[0]);
    TEST_ASSERT_EQUAL(Zzzzzz, t[20].lr[1]);
    TEST_ASSERT_EQUAL(&t[40], t[50].lr[0]);
    TEST_ASSERT_EQUAL(&t[60], t[50].lr[1]);
    TEST_ASSERT_EQUAL(Zzzzzz, t[10].lr[0]);
    TEST_ASSERT_EQUAL(Zzzzzz, t[10].lr[1]);
    TEST_ASSERT_EQUAL(Zzzzzz, t[40].lr[0]);
    TEST_ASSERT_EQUAL(Zzzzzz, t[40].lr[1]);
    TEST_ASSERT_EQUAL(Zzzzzz, t[60].lr[0]);
    TEST_ASSERT_EQUAL(Zzzzzz, t[60].lr[1]);
    TEST_ASSERT_EQUAL(-1, t[20].bf);
    TEST_ASSERT_EQUAL(+0, t[30].bf);
    TEST_ASSERT_NULL(findBrokenAncestry(&t[30]));
    TEST_ASSERT_NULL(findBrokenBalanceFactor(&t[30]));
    TEST_ASSERT_EQUAL(6, checkAscension(&t[30]));
    // Add a new child under 20 and ensure that retracing stops at 20 because it becomes perfectly balanced:
    //
    //          30
    //         /   `
    //       20    50
    //      /  `  /  `
    //     10 21 40 60
    TEST_ASSERT_NULL(findBrokenAncestry(&t[30]));
    TEST_ASSERT_NULL(findBrokenBalanceFactor(&t[30]));
    t[21]       = {reinterpret_cast<void*>(21), &t[20], {Zzzzzz, Zzzzzz}, 0};
    t[20].lr[1] = &t[21];
    TEST_ASSERT_NULL(_cavlRetraceOnGrowth(&t[21]));  // Root not reached, NULL returned.
    std::puts("ADD 21:");
    print(&t[30]);
    TEST_ASSERT_EQUAL(0, t[20].bf);
    TEST_ASSERT_EQUAL(0, t[30].bf);
    TEST_ASSERT_NULL(findBrokenAncestry(&t[30]));
    TEST_ASSERT_NULL(findBrokenBalanceFactor(&t[30]));
    TEST_ASSERT_EQUAL(7, checkAscension(&t[30]));
    //         30
    //       /    `
    //      20     50
    //     / `    /  `
    //    10 21  40  60
    //     `
    //      15        <== first we add this, no balancing needed
    //        `
    //        17      <== then we add this, forcing left rotation at 10
    //
    // After the left rotation of 10, we get:
    //
    //         30
    //       /    `
    //      20     50
    //     / `    /  `
    //    15 21  40  60
    //   / `
    //  10 17
    //
    // When we add one extra item after 17, we force a double rotation (15 left, 20 right). Before the rotation:
    //
    //         30
    //       /    `
    //     20     50
    //    / `    /  `
    //   15 21  40 60
    //  / `
    // 10 17
    //      `
    //       18    <== new item causes imbalance
    //
    // After left rotation of 15:
    //
    //          30
    //        /    `
    //       20     50
    //      / `    / `
    //     17 21  40 60
    //    / `
    //   15 18
    //  /
    // 10
    //
    // After right rotation of 20, this is the final state:
    //
    //          30
    //        /    `
    //       17     50
    //      / `    /  `
    //    15  20  40  60
    //   /   / `
    //  10  18 21
    std::puts("ADD 15:");
    TEST_ASSERT_NULL(findBrokenAncestry(&t[30]));
    TEST_ASSERT_NULL(findBrokenBalanceFactor(&t[30]));
    TEST_ASSERT_EQUAL(7, checkAscension(&t[30]));
    t[15]       = {reinterpret_cast<void*>(15), &t[10], {Zzzzzz, Zzzzzz}, 0};
    t[10].lr[1] = &t[15];
    TEST_ASSERT_EQUAL(&t[30], _cavlRetraceOnGrowth(&t[15]));  // Same root, its balance becomes -1.
    print(&t[30]);
    TEST_ASSERT_EQUAL(+1, t[10].bf);
    TEST_ASSERT_EQUAL(-1, t[20].bf);
    TEST_ASSERT_EQUAL(-1, t[30].bf);
    TEST_ASSERT_NULL(findBrokenAncestry(&t[30]));
    TEST_ASSERT_NULL(findBrokenBalanceFactor(&t[30]));
    TEST_ASSERT_EQUAL(8, checkAscension(&t[30]));

    std::puts("ADD 17:");
    t[17]       = {reinterpret_cast<void*>(17), &t[15], {Zzzzzz, Zzzzzz}, 0};
    t[15].lr[1] = &t[17];
    TEST_ASSERT_EQUAL(nullptr, _cavlRetraceOnGrowth(&t[17]));  // Same root, same balance, 10 rotated left.
    print(&t[30]);
    // Check 10
    TEST_ASSERT_EQUAL(&t[15], t[10].up);
    TEST_ASSERT_EQUAL(0, t[10].bf);
    TEST_ASSERT_EQUAL(nullptr, t[10].lr[0]);
    TEST_ASSERT_EQUAL(nullptr, t[10].lr[1]);
    // Check 17
    TEST_ASSERT_EQUAL(&t[15], t[17].up);
    TEST_ASSERT_EQUAL(0, t[17].bf);
    TEST_ASSERT_EQUAL(nullptr, t[17].lr[0]);
    TEST_ASSERT_EQUAL(nullptr, t[17].lr[1]);
    // Check 15
    TEST_ASSERT_EQUAL(&t[20], t[15].up);
    TEST_ASSERT_EQUAL(0, t[15].bf);
    TEST_ASSERT_EQUAL(&t[10], t[15].lr[0]);
    TEST_ASSERT_EQUAL(&t[17], t[15].lr[1]);
    // Check 20 -- leaning left
    TEST_ASSERT_EQUAL(&t[30], t[20].up);
    TEST_ASSERT_EQUAL(-1, t[20].bf);
    TEST_ASSERT_EQUAL(&t[15], t[20].lr[0]);
    TEST_ASSERT_EQUAL(&t[21], t[20].lr[1]);
    // Check the root -- still leaning left by one.
    TEST_ASSERT_EQUAL(nullptr, t[30].up);
    TEST_ASSERT_EQUAL(-1, t[30].bf);
    TEST_ASSERT_EQUAL(&t[20], t[30].lr[0]);
    TEST_ASSERT_EQUAL(&t[50], t[30].lr[1]);
    // Check hard invariants.
    TEST_ASSERT_NULL(findBrokenAncestry(&t[30]));
    TEST_ASSERT_NULL(findBrokenBalanceFactor(&t[30]));
    TEST_ASSERT_EQUAL(9, checkAscension(&t[30]));

    std::puts("ADD 18:");
    t[18]       = {reinterpret_cast<void*>(18), &t[17], {Zzzzzz, Zzzzzz}, 0};
    t[17].lr[1] = &t[18];
    TEST_ASSERT_EQUAL(nullptr, _cavlRetraceOnGrowth(&t[18]));  // Same root, 15 went left, 20 went right.
    print(&t[30]);
    // Check 17
    TEST_ASSERT_EQUAL(&t[30], t[17].up);
    TEST_ASSERT_EQUAL(0, t[17].bf);
    TEST_ASSERT_EQUAL(&t[15], t[17].lr[0]);
    TEST_ASSERT_EQUAL(&t[20], t[17].lr[1]);
    // Check 15
    TEST_ASSERT_EQUAL(&t[17], t[15].up);
    TEST_ASSERT_EQUAL(-1, t[15].bf);
    TEST_ASSERT_EQUAL(&t[10], t[15].lr[0]);
    TEST_ASSERT_EQUAL(nullptr, t[15].lr[1]);
    // Check 20
    TEST_ASSERT_EQUAL(&t[17], t[20].up);
    TEST_ASSERT_EQUAL(0, t[20].bf);
    TEST_ASSERT_EQUAL(&t[18], t[20].lr[0]);
    TEST_ASSERT_EQUAL(&t[21], t[20].lr[1]);
    // Check 10
    TEST_ASSERT_EQUAL(&t[15], t[10].up);
    TEST_ASSERT_EQUAL(0, t[10].bf);
    TEST_ASSERT_EQUAL(nullptr, t[10].lr[0]);
    TEST_ASSERT_EQUAL(nullptr, t[10].lr[1]);
    // Check 18
    TEST_ASSERT_EQUAL(&t[20], t[18].up);
    TEST_ASSERT_EQUAL(0, t[18].bf);
    TEST_ASSERT_EQUAL(nullptr, t[18].lr[0]);
    TEST_ASSERT_EQUAL(nullptr, t[18].lr[1]);
    // Check 21
    TEST_ASSERT_EQUAL(&t[20], t[21].up);
    TEST_ASSERT_EQUAL(0, t[21].bf);
    TEST_ASSERT_EQUAL(nullptr, t[21].lr[0]);
    TEST_ASSERT_EQUAL(nullptr, t[21].lr[1]);
    // Check hard invariants.
    TEST_ASSERT_NULL(findBrokenAncestry(&t[30]));
    TEST_ASSERT_NULL(findBrokenBalanceFactor(&t[30]));
    TEST_ASSERT_EQUAL(10, checkAscension(&t[30]));
}

int8_t predicate(void* const value, const Cavl* const node)
{
    if (value > node->value)
    {
        return +1;
    }
    if (value < node->value)
    {
        return -1;
    }
    return 0;
}

void testSearchTrivial()
{
    //      A
    //    B   C
    //   D E F G
    Cavl a{};
    Cavl b{};
    Cavl c{};
    Cavl d{};
    Cavl e{};
    Cavl f{};
    Cavl g{};
    a = {reinterpret_cast<void*>(4), Zz, {&b, &c}, 0};
    b = {reinterpret_cast<void*>(2), &a, {&d, &e}, 0};
    c = {reinterpret_cast<void*>(6), &a, {&f, &g}, 0};
    d = {reinterpret_cast<void*>(1), &b, {Zz, Zz}, 0};
    e = {reinterpret_cast<void*>(3), &b, {Zz, Zz}, 0};
    f = {reinterpret_cast<void*>(5), &c, {Zz, Zz}, 0};
    g = {reinterpret_cast<void*>(7), &c, {Zz, Zz}, 0};
    TEST_ASSERT_NULL(findBrokenBalanceFactor(&a));
    TEST_ASSERT_NULL(findBrokenAncestry(&a));
    TEST_ASSERT_EQUAL(7, checkAscension(&a));
    Cavl* root = &a;
    TEST_ASSERT_NULL(cavlSearch(&root, nullptr, nullptr, nullptr));         // Bad arguments.
    TEST_ASSERT_EQUAL(&a, root);                                            // Root shall not be altered.
    TEST_ASSERT_NULL(cavlSearch(&root, nullptr, predicate, nullptr));       // Item not found.
    TEST_ASSERT_EQUAL(&a, root);                                            // Root shall not be altered.
    TEST_ASSERT_EQUAL(&e, cavlSearch(&root, e.value, predicate, nullptr));  // Item found.
    TEST_ASSERT_EQUAL(&a, root);                                            // Root shall not be altered.
    print(&a);
    TEST_ASSERT_EQUAL(nullptr, cavlFindExtremum(nullptr, true));
    TEST_ASSERT_EQUAL(nullptr, cavlFindExtremum(nullptr, false));
    TEST_ASSERT_EQUAL(&g, cavlFindExtremum(&a, true));
    TEST_ASSERT_EQUAL(&d, cavlFindExtremum(&a, false));
    TEST_ASSERT_EQUAL(&g, cavlFindExtremum(&g, true));
    TEST_ASSERT_EQUAL(&g, cavlFindExtremum(&g, false));
    TEST_ASSERT_EQUAL(&d, cavlFindExtremum(&d, true));
    TEST_ASSERT_EQUAL(&d, cavlFindExtremum(&d, false));
}

void testRemovalA()
{
    //        4
    //      /   `
    //    2       6
    //   / `     / `
    //  1   3   5   8
    //             / `
    //            7   9
    Cavl t[10]{};
    t[1]       = {reinterpret_cast<void*>(1), &t[2], {Zzzzz, Zzzzz}, 00};
    t[2]       = {reinterpret_cast<void*>(2), &t[4], {&t[1], &t[3]}, 00};
    t[3]       = {reinterpret_cast<void*>(3), &t[2], {Zzzzz, Zzzzz}, 00};
    t[4]       = {reinterpret_cast<void*>(4), Zzzzz, {&t[2], &t[6]}, +1};
    t[5]       = {reinterpret_cast<void*>(5), &t[6], {Zzzzz, Zzzzz}, 00};
    t[6]       = {reinterpret_cast<void*>(6), &t[4], {&t[5], &t[8]}, +1};
    t[7]       = {reinterpret_cast<void*>(7), &t[8], {Zzzzz, Zzzzz}, 00};
    t[8]       = {reinterpret_cast<void*>(8), &t[6], {&t[7], &t[9]}, 00};
    t[9]       = {reinterpret_cast<void*>(9), &t[8], {Zzzzz, Zzzzz}, 00};
    Cavl* root = &t[4];
    print(root);
    TEST_ASSERT_NULL(findBrokenBalanceFactor(root));
    TEST_ASSERT_NULL(findBrokenAncestry(root));
    TEST_ASSERT_EQUAL(9, checkAscension(root));

    // Remove 9, the easiest case. The rest of the tree remains unchanged.
    //        4
    //      /   `
    //    2       6
    //   / `     / `
    //  1   3   5   8
    //             /
    //            7
    std::puts("REMOVE 9:");
    cavlRemove(&root, &t[9]);
    TEST_ASSERT_EQUAL(&t[4], root);
    print(root);
    TEST_ASSERT_NULL(findBrokenBalanceFactor(root));
    TEST_ASSERT_NULL(findBrokenAncestry(root));
    TEST_ASSERT_EQUAL(8, checkAscension(root));
    // 1
    TEST_ASSERT_EQUAL(&t[2], t[1].up);
    TEST_ASSERT_EQUAL(Zzzzz, t[1].lr[0]);
    TEST_ASSERT_EQUAL(Zzzzz, t[1].lr[1]);
    TEST_ASSERT_EQUAL(00, t[1].bf);
    // 2
    TEST_ASSERT_EQUAL(&t[4], t[2].up);
    TEST_ASSERT_EQUAL(&t[1], t[2].lr[0]);
    TEST_ASSERT_EQUAL(&t[3], t[2].lr[1]);
    TEST_ASSERT_EQUAL(00, t[2].bf);
    // 3
    TEST_ASSERT_EQUAL(&t[2], t[3].up);
    TEST_ASSERT_EQUAL(Zzzzz, t[3].lr[0]);
    TEST_ASSERT_EQUAL(Zzzzz, t[3].lr[1]);
    TEST_ASSERT_EQUAL(00, t[3].bf);
    // 4
    TEST_ASSERT_EQUAL(Zzzzz, t[4].up);  // Nihil Supernum
    TEST_ASSERT_EQUAL(&t[2], t[4].lr[0]);
    TEST_ASSERT_EQUAL(&t[6], t[4].lr[1]);
    TEST_ASSERT_EQUAL(+1, t[4].bf);
    // 5
    TEST_ASSERT_EQUAL(&t[6], t[5].up);
    TEST_ASSERT_EQUAL(Zzzzz, t[5].lr[0]);
    TEST_ASSERT_EQUAL(Zzzzz, t[5].lr[1]);
    TEST_ASSERT_EQUAL(00, t[5].bf);
    // 6
    TEST_ASSERT_EQUAL(&t[4], t[6].up);
    TEST_ASSERT_EQUAL(&t[5], t[6].lr[0]);
    TEST_ASSERT_EQUAL(&t[8], t[6].lr[1]);
    TEST_ASSERT_EQUAL(+1, t[6].bf);
    // 7
    TEST_ASSERT_EQUAL(&t[8], t[7].up);
    TEST_ASSERT_EQUAL(Zzzzz, t[7].lr[0]);
    TEST_ASSERT_EQUAL(Zzzzz, t[7].lr[1]);
    TEST_ASSERT_EQUAL(00, t[7].bf);
    // 8
    TEST_ASSERT_EQUAL(&t[6], t[8].up);
    TEST_ASSERT_EQUAL(&t[7], t[8].lr[0]);
    TEST_ASSERT_EQUAL(Zzzzz, t[8].lr[1]);
    TEST_ASSERT_EQUAL(-1, t[8].bf);

    // Remove 8, 7 takes its place (the one-child case). The rest of the tree remains unchanged.
    //        4
    //      /   `
    //    2       6
    //   / `     / `
    //  1   3   5   7
    std::puts("REMOVE 8:");
    cavlRemove(&root, &t[8]);
    TEST_ASSERT_EQUAL(&t[4], root);
    print(root);
    TEST_ASSERT_NULL(findBrokenBalanceFactor(root));
    TEST_ASSERT_NULL(findBrokenAncestry(root));
    TEST_ASSERT_EQUAL(7, checkAscension(root));
    // 1
    TEST_ASSERT_EQUAL(&t[2], t[1].up);
    TEST_ASSERT_EQUAL(Zzzzz, t[1].lr[0]);
    TEST_ASSERT_EQUAL(Zzzzz, t[1].lr[1]);
    TEST_ASSERT_EQUAL(00, t[1].bf);
    // 2
    TEST_ASSERT_EQUAL(&t[4], t[2].up);
    TEST_ASSERT_EQUAL(&t[1], t[2].lr[0]);
    TEST_ASSERT_EQUAL(&t[3], t[2].lr[1]);
    TEST_ASSERT_EQUAL(00, t[2].bf);
    // 3
    TEST_ASSERT_EQUAL(&t[2], t[3].up);
    TEST_ASSERT_EQUAL(Zzzzz, t[3].lr[0]);
    TEST_ASSERT_EQUAL(Zzzzz, t[3].lr[1]);
    TEST_ASSERT_EQUAL(00, t[3].bf);
    // 4
    TEST_ASSERT_EQUAL(Zzzzz, t[4].up);  // Nihil Supernum
    TEST_ASSERT_EQUAL(&t[2], t[4].lr[0]);
    TEST_ASSERT_EQUAL(&t[6], t[4].lr[1]);
    TEST_ASSERT_EQUAL(00, t[4].bf);
    // 5
    TEST_ASSERT_EQUAL(&t[6], t[5].up);
    TEST_ASSERT_EQUAL(Zzzzz, t[5].lr[0]);
    TEST_ASSERT_EQUAL(Zzzzz, t[5].lr[1]);
    TEST_ASSERT_EQUAL(00, t[5].bf);
    // 6
    TEST_ASSERT_EQUAL(&t[4], t[6].up);
    TEST_ASSERT_EQUAL(&t[5], t[6].lr[0]);
    TEST_ASSERT_EQUAL(&t[7], t[6].lr[1]);
    TEST_ASSERT_EQUAL(00, t[6].bf);
    // 7
    TEST_ASSERT_EQUAL(&t[6], t[7].up);
    TEST_ASSERT_EQUAL(Zzzzz, t[7].lr[0]);
    TEST_ASSERT_EQUAL(Zzzzz, t[7].lr[1]);
    TEST_ASSERT_EQUAL(00, t[7].bf);

    // Remove the root node 4, 5 takes its place. The overall structure remains unchanged except that 5 is now the root.
    //        5
    //      /   `
    //    2       6
    //   / `       `
    //  1   3       7
    std::puts("REMOVE 4:");
    cavlRemove(&root, &t[4]);
    TEST_ASSERT_EQUAL(&t[5], root);
    print(root);
    TEST_ASSERT_NULL(findBrokenBalanceFactor(root));
    TEST_ASSERT_NULL(findBrokenAncestry(root));
    TEST_ASSERT_EQUAL(6, checkAscension(root));
    // 1
    TEST_ASSERT_EQUAL(&t[2], t[1].up);
    TEST_ASSERT_EQUAL(Zzzzz, t[1].lr[0]);
    TEST_ASSERT_EQUAL(Zzzzz, t[1].lr[1]);
    TEST_ASSERT_EQUAL(00, t[1].bf);
    // 2
    TEST_ASSERT_EQUAL(&t[5], t[2].up);
    TEST_ASSERT_EQUAL(&t[1], t[2].lr[0]);
    TEST_ASSERT_EQUAL(&t[3], t[2].lr[1]);
    TEST_ASSERT_EQUAL(00, t[2].bf);
    // 3
    TEST_ASSERT_EQUAL(&t[2], t[3].up);
    TEST_ASSERT_EQUAL(Zzzzz, t[3].lr[0]);
    TEST_ASSERT_EQUAL(Zzzzz, t[3].lr[1]);
    TEST_ASSERT_EQUAL(00, t[3].bf);
    // 5
    TEST_ASSERT_EQUAL(Zzzzz, t[5].up);  // Nihil Supernum
    TEST_ASSERT_EQUAL(&t[2], t[5].lr[0]);
    TEST_ASSERT_EQUAL(&t[6], t[5].lr[1]);
    TEST_ASSERT_EQUAL(00, t[5].bf);
    // 6
    TEST_ASSERT_EQUAL(&t[5], t[6].up);
    TEST_ASSERT_EQUAL(Zzzzz, t[6].lr[0]);
    TEST_ASSERT_EQUAL(&t[7], t[6].lr[1]);
    TEST_ASSERT_EQUAL(+1, t[6].bf);
    // 7
    TEST_ASSERT_EQUAL(&t[6], t[7].up);
    TEST_ASSERT_EQUAL(Zzzzz, t[7].lr[0]);
    TEST_ASSERT_EQUAL(Zzzzz, t[7].lr[1]);
    TEST_ASSERT_EQUAL(00, t[7].bf);

    // Remove the root node 5, 6 takes its place.
    //        6
    //      /   `
    //    2       7
    //   / `
    //  1   3
    std::puts("REMOVE 5:");
    cavlRemove(&root, &t[5]);
    TEST_ASSERT_EQUAL(&t[6], root);
    print(root);
    TEST_ASSERT_NULL(findBrokenBalanceFactor(root));
    TEST_ASSERT_NULL(findBrokenAncestry(root));
    TEST_ASSERT_EQUAL(5, checkAscension(root));
    // 1
    TEST_ASSERT_EQUAL(&t[2], t[1].up);
    TEST_ASSERT_EQUAL(Zzzzz, t[1].lr[0]);
    TEST_ASSERT_EQUAL(Zzzzz, t[1].lr[1]);
    TEST_ASSERT_EQUAL(00, t[1].bf);
    // 2
    TEST_ASSERT_EQUAL(&t[6], t[2].up);
    TEST_ASSERT_EQUAL(&t[1], t[2].lr[0]);
    TEST_ASSERT_EQUAL(&t[3], t[2].lr[1]);
    TEST_ASSERT_EQUAL(00, t[2].bf);
    // 3
    TEST_ASSERT_EQUAL(&t[2], t[3].up);
    TEST_ASSERT_EQUAL(Zzzzz, t[3].lr[0]);
    TEST_ASSERT_EQUAL(Zzzzz, t[3].lr[1]);
    TEST_ASSERT_EQUAL(00, t[3].bf);
    // 6
    TEST_ASSERT_EQUAL(Zzzzz, t[6].up);  // Nihil Supernum
    TEST_ASSERT_EQUAL(&t[2], t[6].lr[0]);
    TEST_ASSERT_EQUAL(&t[7], t[6].lr[1]);
    TEST_ASSERT_EQUAL(-1, t[6].bf);
    // 7
    TEST_ASSERT_EQUAL(&t[6], t[7].up);
    TEST_ASSERT_EQUAL(Zzzzz, t[7].lr[0]);
    TEST_ASSERT_EQUAL(Zzzzz, t[7].lr[1]);
    TEST_ASSERT_EQUAL(00, t[7].bf);

    // Remove the root node 6, 7 takes its place, then right rotation is done to restore balance, 2 is the new root.
    //          2
    //        /   `
    //       1     7
    //            /
    //           3
    std::puts("REMOVE 6:");
    cavlRemove(&root, &t[6]);
    TEST_ASSERT_EQUAL(&t[2], root);
    print(root);
    TEST_ASSERT_NULL(findBrokenBalanceFactor(root));
    TEST_ASSERT_NULL(findBrokenAncestry(root));
    TEST_ASSERT_EQUAL(4, checkAscension(root));
    // 1
    TEST_ASSERT_EQUAL(&t[2], t[1].up);
    TEST_ASSERT_EQUAL(Zzzzz, t[1].lr[0]);
    TEST_ASSERT_EQUAL(Zzzzz, t[1].lr[1]);
    TEST_ASSERT_EQUAL(00, t[1].bf);
    // 2
    TEST_ASSERT_EQUAL(Zzzzz, t[2].up);  // Nihil Supernum
    TEST_ASSERT_EQUAL(&t[1], t[2].lr[0]);
    TEST_ASSERT_EQUAL(&t[7], t[2].lr[1]);
    TEST_ASSERT_EQUAL(+1, t[2].bf);
    // 3
    TEST_ASSERT_EQUAL(&t[7], t[3].up);
    TEST_ASSERT_EQUAL(Zzzzz, t[3].lr[0]);
    TEST_ASSERT_EQUAL(Zzzzz, t[3].lr[1]);
    TEST_ASSERT_EQUAL(00, t[3].bf);
    // 7
    TEST_ASSERT_EQUAL(&t[2], t[7].up);
    TEST_ASSERT_EQUAL(&t[3], t[7].lr[0]);
    TEST_ASSERT_EQUAL(Zzzzz, t[7].lr[1]);
    TEST_ASSERT_EQUAL(-1, t[7].bf);

    // Remove 1, then balancing makes 3 the new root node.
    //          3
    //        /   `
    //       2     7
    std::puts("REMOVE 1:");
    cavlRemove(&root, &t[1]);
    TEST_ASSERT_EQUAL(&t[3], root);
    print(root);
    TEST_ASSERT_NULL(findBrokenBalanceFactor(root));
    TEST_ASSERT_NULL(findBrokenAncestry(root));
    TEST_ASSERT_EQUAL(3, checkAscension(root));
    // 2
    TEST_ASSERT_EQUAL(&t[3], t[2].up);
    TEST_ASSERT_EQUAL(Zzzzz, t[2].lr[0]);
    TEST_ASSERT_EQUAL(Zzzzz, t[2].lr[1]);
    TEST_ASSERT_EQUAL(0, t[2].bf);
    // 3
    TEST_ASSERT_EQUAL(Zzzzz, t[3].up);  // Nihil Supernum
    TEST_ASSERT_EQUAL(&t[2], t[3].lr[0]);
    TEST_ASSERT_EQUAL(&t[7], t[3].lr[1]);
    TEST_ASSERT_EQUAL(00, t[3].bf);
    // 7
    TEST_ASSERT_EQUAL(&t[3], t[7].up);
    TEST_ASSERT_EQUAL(Zzzzz, t[7].lr[0]);
    TEST_ASSERT_EQUAL(Zzzzz, t[7].lr[1]);
    TEST_ASSERT_EQUAL(00, t[7].bf);

    // Remove 7.
    //          3
    //        /
    //       2
    std::puts("REMOVE 7:");
    cavlRemove(&root, &t[7]);
    TEST_ASSERT_EQUAL(&t[3], root);
    print(root);
    TEST_ASSERT_NULL(findBrokenBalanceFactor(root));
    TEST_ASSERT_NULL(findBrokenAncestry(root));
    TEST_ASSERT_EQUAL(2, checkAscension(root));
    // 2
    TEST_ASSERT_EQUAL(&t[3], t[2].up);
    TEST_ASSERT_EQUAL(Zzzzz, t[2].lr[0]);
    TEST_ASSERT_EQUAL(Zzzzz, t[2].lr[1]);
    TEST_ASSERT_EQUAL(0, t[2].bf);
    // 3
    TEST_ASSERT_EQUAL(Zzzzz, t[3].up);  // Nihil Supernum
    TEST_ASSERT_EQUAL(&t[2], t[3].lr[0]);
    TEST_ASSERT_EQUAL(Zzzzz, t[3].lr[1]);
    TEST_ASSERT_EQUAL(-1, t[3].bf);

    // Remove 3. Only 2 is left, which is now obviously the root.
    std::puts("REMOVE 3:");
    cavlRemove(&root, &t[3]);
    TEST_ASSERT_EQUAL(&t[2], root);
    print(root);
    TEST_ASSERT_NULL(findBrokenBalanceFactor(root));
    TEST_ASSERT_NULL(findBrokenAncestry(root));
    TEST_ASSERT_EQUAL(1, checkAscension(root));
    // 2
    TEST_ASSERT_EQUAL(Zzzzz, t[2].up);
    TEST_ASSERT_EQUAL(Zzzzz, t[2].lr[0]);
    TEST_ASSERT_EQUAL(Zzzzz, t[2].lr[1]);
    TEST_ASSERT_EQUAL(0, t[2].bf);

    // Remove 2. The tree is now empty, make sure the root pointer is updated accordingly.
    std::puts("REMOVE 2:");
    cavlRemove(&root, &t[2]);
    TEST_ASSERT_EQUAL(nullptr, root);
}

}  // namespace

int main()
{
    UNITY_BEGIN();
    RUN_TEST(testCheckAscension);
    RUN_TEST(testRotation);
    RUN_TEST(testBalancingA);
    RUN_TEST(testBalancingB);
    RUN_TEST(testBalancingC);
    RUN_TEST(testRetracingOnGrowth);
    RUN_TEST(testSearchTrivial);
    RUN_TEST(testRemovalA);
    return UNITY_END();
}
