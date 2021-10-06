/// Copyright (c) 2021 Pavel Kirienko <pavel@uavcan.org>

#include "cavl.h"
#include <algorithm>
#include <cstdio>
#include <iostream>

// NOLINTNEXTLINE
#define ENFORCE(...)                                                                          \
    do                                                                                        \
    {                                                                                         \
        if (!bool(__VA_ARGS__))                                                               \
        {                                                                                     \
            std::cerr << "Assertion failure at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::exit(1);                                                                     \
        }                                                                                     \
    } while (false)

namespace
{
void print(const Cavl* const nd, const std::uint8_t depth = 0, const char marker = 'T')
{
    for (std::uint16_t i = 0U; i < depth; i++)
    {
        std::cout << "    ";
    }
    std::cout << marker << "=";
    if (nd == nullptr)
    {
        std::cout << "null\n";
    }
    else
    {
        std::cout << nd->value << " [" << int(nd->bf) << "]\n";
        print(nd->lr[0], depth + 1U, 'L');
        print(nd->lr[1], depth + 1U, 'R');
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

std::pair<bool, std::size_t> checkAscension(const Cavl* const root)
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
    return {valid, size};
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
    if (n != nullptr)
    {
        return std::uint8_t(1U + std::max(getHeight(n->lr[0]), getHeight(n->lr[1])));
    }
    return 0;
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
    ENFORCE(checkAscension(&t) == std::pair<bool, std::size_t>{true, 4});
    ENFORCE(getHeight(&t) == 3);
    // Break the arrangement and make sure the breakage is detected.
    t.lr[1] = &l;
    t.lr[0] = &r;
    ENFORCE(checkAscension(&t) == std::pair<bool, std::size_t>{false, 4});
    ENFORCE(getHeight(&t) == 3);
    ENFORCE(&t == findBrokenBalanceFactor(&t));  // All zeros, incorrect.
    r.lr[1] = nullptr;
    ENFORCE(getHeight(&t) == 2);
    ENFORCE(nullptr == findBrokenBalanceFactor(&t));  // Balanced now as we removed one node.
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
    auto Zz = nullptr;
    Cavl c{reinterpret_cast<void*>(3), Zz, {Zz, Zz}, 0};
    Cavl b{reinterpret_cast<void*>(2), Zz, {Zz, Zz}, 0};
    Cavl a{reinterpret_cast<void*>(1), Zz, {Zz, Zz}, 0};
    Cavl z{reinterpret_cast<void*>(8), Zz, {&b, &c}, 0};
    Cavl x{reinterpret_cast<void*>(9), Zz, {&a, &z}, 1};
    z.up = &x;
    c.up = &z;
    b.up = &z;
    a.up = &x;

    std::cout << "Before rotation:\n";
    ENFORCE(nullptr == findBrokenAncestry(&x));
    ENFORCE(nullptr == findBrokenBalanceFactor(&x));
    print(&x);

    std::cout << "After left rotation:\n";
    ENFORCE(&z == _cavlRotate(&x, false));
    ENFORCE(nullptr == findBrokenAncestry(&z));
    ENFORCE(nullptr == findBrokenBalanceFactor(&z));
    print(&z);
    ENFORCE(x.lr[0] == &a);
    ENFORCE(x.lr[1] == &b);
    ENFORCE(z.lr[0] == &x);
    ENFORCE(z.lr[1] == &c);

    std::cout << "After right rotation, back into the original configuration:\n";
    ENFORCE(&x == _cavlRotate(&z, true));
    ENFORCE(nullptr == findBrokenAncestry(&x));
    ENFORCE(nullptr == findBrokenBalanceFactor(&x));
    print(&x);
    ENFORCE(x.lr[0] == &a);
    ENFORCE(x.lr[1] == &z);
    ENFORCE(z.lr[0] == &b);
    ENFORCE(z.lr[1] == &c);
}

void testBalancing()
{
    auto Zz = nullptr;
    //     A             A           E
    //    / `           / `        /   `
    //   B   C?  =>    E   C? =>  B     A
    //  / `           / `        / `   / `
    // D?  E         B   G?     D?  F?G?  C?
    //    / `       / `
    //   F?  G?    D?  F?
    Cavl a{reinterpret_cast<void*>(1), Zz, {Zz, Zz}, -2};
    Cavl b{reinterpret_cast<void*>(2), &a, {Zz, Zz}, +1};
    Cavl c{reinterpret_cast<void*>(3), &a, {Zz, Zz}, 0};
    Cavl d{reinterpret_cast<void*>(4), &b, {Zz, Zz}, 0};
    Cavl e{reinterpret_cast<void*>(5), &b, {Zz, Zz}, 0};
    Cavl f{reinterpret_cast<void*>(6), &e, {Zz, Zz}, 0};
    Cavl g{reinterpret_cast<void*>(7), &e, {Zz, Zz}, 0};
    a.lr[0] = &b;
    a.lr[1] = &c;
    b.lr[0] = &d;
    b.lr[1] = &e;
    e.lr[0] = &f;
    e.lr[1] = &g;
    std::cout << "Before balancing:" << std::endl;
    print(&a);
    ENFORCE(nullptr == findBrokenBalanceFactor(&a));
    ENFORCE(nullptr == findBrokenAncestry(&a));
    std::cout << "After balancing:" << std::endl;
    ENFORCE(&e == _cavlBalance(&a));
    print(&e);
    ENFORCE(nullptr == findBrokenBalanceFactor(&e));
    ENFORCE(nullptr == findBrokenAncestry(&e));
    ENFORCE(e.lr[0] == &b);
    ENFORCE(e.lr[1] == &a);
    ENFORCE(b.lr[0] == &d);
    ENFORCE(b.lr[1] == &f);
    ENFORCE(a.lr[0] == &g);
    ENFORCE(a.lr[1] == &c);
    ENFORCE(d.lr[0] == nullptr);
    ENFORCE(d.lr[1] == nullptr);
    ENFORCE(f.lr[0] == nullptr);
    ENFORCE(f.lr[1] == nullptr);
    ENFORCE(g.lr[0] == nullptr);
    ENFORCE(g.lr[1] == nullptr);
    ENFORCE(c.lr[0] == nullptr);
    ENFORCE(c.lr[1] == nullptr);

    //       A              B
    //      / `           /   `
    //     B   C?  =>    D     A
    //    / `           / `   / `
    //   D   E?        F?  G?E?  C?
    //  / `
    // F?  G?
    a = {a.value, Zz, {&b, &c}, -2};
    b = {b.value, &a, {&d, &e}, -1};
    c = {c.value, &a, {Zz, Zz}, 0};
    d = {d.value, &b, {&f, &g}, 0};
    e = {e.value, &b, {Zz, Zz}, 0};
    f = {f.value, &d, {Zz, Zz}, 0};
    g = {g.value, &d, {Zz, Zz}, 0};
    std::cout << "Before balancing:" << std::endl;
    print(&a);
    ENFORCE(nullptr == findBrokenBalanceFactor(&a));
    ENFORCE(nullptr == findBrokenAncestry(&a));
    std::cout << "After balancing:" << std::endl;
    ENFORCE(&b == _cavlBalance(&a));
    print(&b);
    ENFORCE(nullptr == findBrokenBalanceFactor(&b));
    ENFORCE(nullptr == findBrokenAncestry(&b));
}

}  // namespace

int main()
{
    testCheckAscension();
    std::cout << "\n-----\n";
    testRotation();
    std::cout << "\n-----\n";
    testBalancing();
    return 0;
}
