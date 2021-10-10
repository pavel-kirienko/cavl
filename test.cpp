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
constexpr auto Zz       = nullptr;
constexpr auto Zzzzz    = nullptr;
constexpr auto Zzzzzzzz = nullptr;

void print(const Cavl* const nd, const std::uint8_t depth = 0, const char marker = 'T')
{
    if (nd != nullptr)
    {
        print(nd->lr[0], static_cast<std::uint8_t>(depth + 1U), 'L');
        for (std::uint16_t i = 1U; i < depth; i++)
        {
            std::printf("                ");
        }
        if (marker == 'L')
        {
            std::printf(" ...............");
        }
        else if (marker == 'R')
        {
            std::printf(" ```````````````");
        }
        else
        {
            //
        }
        std::printf("%c=%p [%d]\n", marker, nd->value, nd->bf);
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
    Cavl t[256]{};
    //        0x50                 0x30
    //       /   `                /   `
    //     0x30   0x60?   =>    0x20   0x50
    //     /  `                 /      /  `
    //   0x20 0x40?           0x10   0x40? 0x60?
    //   /
    // 0x10
    t[0x50] = {reinterpret_cast<void*>(0x50), Zzzzzzzz, {&t[0x30], &t[0x60]}, -1};
    t[0x30] = {reinterpret_cast<void*>(0x30), &t[0x50], {&t[0x20], &t[0x40]}, 0};
    t[0x60] = {reinterpret_cast<void*>(0x60), &t[0x50], {Zzzzzzzz, Zzzzzzzz}, 0};
    t[0x20] = {reinterpret_cast<void*>(0x20), &t[0x30], {&t[0x10], Zzzzzzzz}, 0};
    t[0x40] = {reinterpret_cast<void*>(0x40), &t[0x30], {Zzzzzzzz, Zzzzzzzz}, 0};
    t[0x10] = {reinterpret_cast<void*>(0x10), &t[0x20], {Zzzzzzzz, Zzzzzzzz}, 0};
    print(&t[0x50]);  // The tree is imbalanced because we just added 1 and are about to retrace it.
    TEST_ASSERT_NULL(findBrokenAncestry(&t[0x50]));
    TEST_ASSERT_EQUAL(6, checkAscension(&t[0x50]));
    TEST_ASSERT_EQUAL(&t[0x30], _cavlRetraceOnGrowth(&t[0x10]));
    std::puts("ADD 0x10:");
    print(&t[0x30]);  // This is the new root.
    TEST_ASSERT_EQUAL(&t[0x20], t[0x30].lr[0]);
    TEST_ASSERT_EQUAL(&t[0x50], t[0x30].lr[1]);
    TEST_ASSERT_EQUAL(&t[0x10], t[0x20].lr[0]);
    TEST_ASSERT_EQUAL(Zzzzzzzz, t[0x20].lr[1]);
    TEST_ASSERT_EQUAL(&t[0x40], t[0x50].lr[0]);
    TEST_ASSERT_EQUAL(&t[0x60], t[0x50].lr[1]);
    TEST_ASSERT_EQUAL(Zzzzzzzz, t[0x10].lr[0]);
    TEST_ASSERT_EQUAL(Zzzzzzzz, t[0x10].lr[1]);
    TEST_ASSERT_EQUAL(Zzzzzzzz, t[0x40].lr[0]);
    TEST_ASSERT_EQUAL(Zzzzzzzz, t[0x40].lr[1]);
    TEST_ASSERT_EQUAL(Zzzzzzzz, t[0x60].lr[0]);
    TEST_ASSERT_EQUAL(Zzzzzzzz, t[0x60].lr[1]);
    TEST_ASSERT_EQUAL(-1, t[0x20].bf);
    TEST_ASSERT_EQUAL(+0, t[0x30].bf);
    TEST_ASSERT_NULL(findBrokenAncestry(&t[0x30]));
    TEST_ASSERT_NULL(findBrokenBalanceFactor(&t[0x30]));
    TEST_ASSERT_EQUAL(6, checkAscension(&t[0x30]));
    // Add a new child under 0x20 and ensure that retracing stops at 0x20 because it becomes perfectly balanced:
    //
    //           0x30
    //         /      `
    //       0x20      0x50
    //       /  `      /  `
    //     0x10 0x21 0x40 0x60
    TEST_ASSERT_NULL(findBrokenAncestry(&t[0x30]));
    TEST_ASSERT_NULL(findBrokenBalanceFactor(&t[0x30]));
    t[0x21]       = {reinterpret_cast<void*>(0x21), &t[0x20], {Zzzzzzzz, Zzzzzzzz}, 0};
    t[0x20].lr[1] = &t[0x21];
    TEST_ASSERT_NULL(_cavlRetraceOnGrowth(&t[0x21]));  // Root not reached, NULL returned.
    std::puts("ADD 0x21:");
    print(&t[0x30]);
    TEST_ASSERT_EQUAL(0, t[0x20].bf);
    TEST_ASSERT_EQUAL(0, t[0x30].bf);
    TEST_ASSERT_NULL(findBrokenAncestry(&t[0x30]));
    TEST_ASSERT_NULL(findBrokenBalanceFactor(&t[0x30]));
    TEST_ASSERT_EQUAL(7, checkAscension(&t[0x30]));
    //           0x30
    //          /    `
    //       0x20     0x50
    //       / `      /  `
    //    0x10 0x21 0x40 0x60
    //       `
    //       0x15        <== first we add this, no balancing needed
    //         `
    //         0x17      <== then we add this, forcing left rotation at 0x10
    //
    // After the left rotation of 0x10, we get:
    //
    //           0x30
    //          /    `
    //       0x20     0x50
    //       / `      /  `
    //    0x15 0x21 0x40 0x60
    //     / `
    //   0x10 0x17
    //
    // When we add one extra item after 0x17, we force a double rotation (0x15 left, 0x20 right). Before the rotation:
    //
    //           0x30
    //          /    `
    //       0x20     0x50
    //       / `      /  `
    //    0x15 0x21 0x40 0x60
    //     / `
    //   0x10 0x17
    //          `
    //          0x18    <== new item causes imbalance
    //
    // After left rotation of 0x15:
    //
    //           0x30
    //         /       `
    //       0x20      0x50
    //       / `       /   `
    //     0x17 0x21 0x40 0x60
    //     / `
    //   0x15 0x18
    //   /
    // 0x10
    //
    // After right rotation of 0x20, this is the final state:
    //
    //           0x30
    //         /       `
    //       0x17       0x50
    //       /  `       /   `
    //    0x15  0x20  0x40 0x60
    //    /     / `
    //  0x10  0x18 0x21
    std::puts("ADD 0x15:");
    TEST_ASSERT_NULL(findBrokenAncestry(&t[0x30]));
    TEST_ASSERT_NULL(findBrokenBalanceFactor(&t[0x30]));
    TEST_ASSERT_EQUAL(7, checkAscension(&t[0x30]));
    t[0x15]       = {reinterpret_cast<void*>(0x15), &t[0x10], {Zzzzzzzz, Zzzzzzzz}, 0};
    t[0x10].lr[1] = &t[0x15];
    TEST_ASSERT_EQUAL(&t[0x30], _cavlRetraceOnGrowth(&t[0x15]));  // Same root, its balance becomes -1.
    print(&t[0x30]);
    TEST_ASSERT_EQUAL(+1, t[0x10].bf);
    TEST_ASSERT_EQUAL(-1, t[0x20].bf);
    TEST_ASSERT_EQUAL(-1, t[0x30].bf);
    TEST_ASSERT_NULL(findBrokenAncestry(&t[0x30]));
    TEST_ASSERT_NULL(findBrokenBalanceFactor(&t[0x30]));
    TEST_ASSERT_EQUAL(8, checkAscension(&t[0x30]));

    std::puts("ADD 0x17:");
    t[0x17]       = {reinterpret_cast<void*>(0x17), &t[0x15], {Zzzzzzzz, Zzzzzzzz}, 0};
    t[0x15].lr[1] = &t[0x17];
    TEST_ASSERT_EQUAL(nullptr, _cavlRetraceOnGrowth(&t[0x17]));  // Same root, same balance, 0x10 rotated left.
    print(&t[0x30]);
    // Check 0x10
    TEST_ASSERT_EQUAL(&t[0x15], t[0x10].up);
    TEST_ASSERT_EQUAL(0, t[0x10].bf);
    TEST_ASSERT_EQUAL(nullptr, t[0x10].lr[0]);
    TEST_ASSERT_EQUAL(nullptr, t[0x10].lr[1]);
    // Check 0x17
    TEST_ASSERT_EQUAL(&t[0x15], t[0x17].up);
    TEST_ASSERT_EQUAL(0, t[0x17].bf);
    TEST_ASSERT_EQUAL(nullptr, t[0x17].lr[0]);
    TEST_ASSERT_EQUAL(nullptr, t[0x17].lr[1]);
    // Check 0x15
    TEST_ASSERT_EQUAL(&t[0x20], t[0x15].up);
    TEST_ASSERT_EQUAL(0, t[0x15].bf);
    TEST_ASSERT_EQUAL(&t[0x10], t[0x15].lr[0]);
    TEST_ASSERT_EQUAL(&t[0x17], t[0x15].lr[1]);
    // Check 0x20 -- leaning left
    TEST_ASSERT_EQUAL(&t[0x30], t[0x20].up);
    TEST_ASSERT_EQUAL(-1, t[0x20].bf);
    TEST_ASSERT_EQUAL(&t[0x15], t[0x20].lr[0]);
    TEST_ASSERT_EQUAL(&t[0x21], t[0x20].lr[1]);
    // Check the root -- still leaning left by one.
    TEST_ASSERT_EQUAL(nullptr, t[0x30].up);
    TEST_ASSERT_EQUAL(-1, t[0x30].bf);
    TEST_ASSERT_EQUAL(&t[0x20], t[0x30].lr[0]);
    TEST_ASSERT_EQUAL(&t[0x50], t[0x30].lr[1]);
    // Check hard invariants.
    TEST_ASSERT_NULL(findBrokenAncestry(&t[0x30]));
    TEST_ASSERT_NULL(findBrokenBalanceFactor(&t[0x30]));
    TEST_ASSERT_EQUAL(9, checkAscension(&t[0x30]));

    std::puts("ADD 0x18:");
    t[0x18]       = {reinterpret_cast<void*>(0x18), &t[0x17], {Zzzzzzzz, Zzzzzzzz}, 0};
    t[0x17].lr[1] = &t[0x18];
    TEST_ASSERT_EQUAL(nullptr, _cavlRetraceOnGrowth(&t[0x18]));  // Same root, 0x15 went left, 0x20 went right.
    print(&t[0x30]);
    // Check 0x17
    TEST_ASSERT_EQUAL(&t[0x30], t[0x17].up);
    TEST_ASSERT_EQUAL(0, t[0x17].bf);
    TEST_ASSERT_EQUAL(&t[0x15], t[0x17].lr[0]);
    TEST_ASSERT_EQUAL(&t[0x20], t[0x17].lr[1]);
    // Check 0x15
    TEST_ASSERT_EQUAL(&t[0x17], t[0x15].up);
    TEST_ASSERT_EQUAL(-1, t[0x15].bf);
    TEST_ASSERT_EQUAL(&t[0x10], t[0x15].lr[0]);
    TEST_ASSERT_EQUAL(nullptr, t[0x15].lr[1]);
    // Check 0x20
    TEST_ASSERT_EQUAL(&t[0x17], t[0x20].up);
    TEST_ASSERT_EQUAL(0, t[0x20].bf);
    TEST_ASSERT_EQUAL(&t[0x18], t[0x20].lr[0]);
    TEST_ASSERT_EQUAL(&t[0x21], t[0x20].lr[1]);
    // Check 0x10
    TEST_ASSERT_EQUAL(&t[0x15], t[0x10].up);
    TEST_ASSERT_EQUAL(0, t[0x10].bf);
    TEST_ASSERT_EQUAL(nullptr, t[0x10].lr[0]);
    TEST_ASSERT_EQUAL(nullptr, t[0x10].lr[1]);
    // Check 0x18
    TEST_ASSERT_EQUAL(&t[0x20], t[0x18].up);
    TEST_ASSERT_EQUAL(0, t[0x18].bf);
    TEST_ASSERT_EQUAL(nullptr, t[0x18].lr[0]);
    TEST_ASSERT_EQUAL(nullptr, t[0x18].lr[1]);
    // Check 0x21
    TEST_ASSERT_EQUAL(&t[0x20], t[0x21].up);
    TEST_ASSERT_EQUAL(0, t[0x21].bf);
    TEST_ASSERT_EQUAL(nullptr, t[0x21].lr[0]);
    TEST_ASSERT_EQUAL(nullptr, t[0x21].lr[1]);
    // Check hard invariants.
    TEST_ASSERT_NULL(findBrokenAncestry(&t[0x30]));
    TEST_ASSERT_NULL(findBrokenBalanceFactor(&t[0x30]));
    TEST_ASSERT_EQUAL(10, checkAscension(&t[0x30]));
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
