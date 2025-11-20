/// Copyright (c) 2021 Pavel Kirienko <pavel@uavcan.org>

#include "cavl2.h"
#include <unity.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <optional>
#include <numeric>

void setUp() {}

void tearDown() {}

namespace {
/// These aliases are introduced to keep things nicely aligned in test cases.
constexpr auto Zz     = nullptr;
constexpr auto Zzzzz  = nullptr;
constexpr auto Zzzzzz = nullptr;

template<typename T>
struct Node final : cavl2_t
{
    explicit Node(const T val)
      : cavl2_t{ cavl2_t{} }
      , value(val)
    {
    }
    Node(const cavl2_t& cv, const T val)
      : cavl2_t{ cv }
      , value(val)
    {
    }
    Node()
      : cavl2_t{ cavl2_t{} }
    {
    }

    T value{};

    bool check_linkage_up_left_right_bf(const cavl2_t* const check_up,
                                        const cavl2_t* const check_le,
                                        const cavl2_t* const check_ri,
                                        const std::int8_t    check_bf) const
    {
        return (up == check_up) &&                                                                  //
               (lr[0] == check_le) && (lr[1] == check_ri) &&                                        //
               (bf == check_bf) &&                                                                  //
               ((check_up == nullptr) || (check_up->lr[0] == this) || (check_up->lr[1] == this)) && //
               ((check_le == nullptr) || (check_le->up == this)) &&                                 //
               ((check_ri == nullptr) || (check_ri->up == this));
    }

    Node* min() { return reinterpret_cast<Node*>(cavl2_min(this)); }
    Node* max() { return reinterpret_cast<Node*>(cavl2_max(this)); }

    Node& operator=(const cavl2_t& cv)
    {
        static_cast<cavl2_t&>(*this) = cv;
        return *this;
    }
};

/// Wrapper over cavl2_find_or_insert() that supports closures.
template<typename T, typename Comparator, typename Factory>
Node<T>* find_or_insert(Node<T>** const root, const Comparator& comparator, const Factory& factory)
{
    struct Refs
    {
        Comparator comparator;
        Factory    factory;

        static std::ptrdiff_t call_comparator(const void* const user, const cavl2_t* const node)
        {
            const auto ret = static_cast<const Refs*>(user)->comparator(reinterpret_cast<const Node<T>&>(*node));
            if (ret > 0) {
                return 1;
            }
            if (ret < 0) {
                return -1;
            }
            return 0;
        }

        static cavl2_t* call_factory(void* const user) { return static_cast<Refs*>(user)->factory(); }
    } refs{ comparator, factory };
    cavl2_t* const out = cavl2_find_or_insert(reinterpret_cast<cavl2_t**>(root), //
                                              &refs,
                                              &Refs::call_comparator,
                                              &refs,
                                              &Refs::call_factory);
    return reinterpret_cast<Node<T>*>(out);
}
template<typename T, typename Comparator>
Node<T>* find(Node<T>* const root, const Comparator& comparator)
{
    struct Refs
    {
        Comparator comparator;

        static std::ptrdiff_t call_comparator(const void* const user, const cavl2_t* const node)
        {
            const auto ret = static_cast<const Refs*>(user)->comparator(reinterpret_cast<const Node<T>&>(*node));
            if (ret > 0) {
                return 1;
            }
            if (ret < 0) {
                return -1;
            }
            return 0;
        }
    } refs{ comparator };
    cavl2_t* const out = cavl2_find(reinterpret_cast<cavl2_t*>(root), &refs, &Refs::call_comparator);
    return reinterpret_cast<Node<T>*>(out);
}

/// Wrapper over cavl2_remove().
template<typename T>
void remove(Node<T>** const root, const Node<T>* const n)
{
    cavl2_remove(reinterpret_cast<cavl2_t**>(root), n);
}

template<typename T>
std::uint8_t get_height(const Node<T>* const n)
{
    return (n != nullptr) ? static_cast<std::uint8_t>(1U + std::max(get_height(reinterpret_cast<Node<T>*>(n->lr[0])),
                                                                    get_height(reinterpret_cast<Node<T>*>(n->lr[1]))))
                          : 0;
}

template<typename T>
void print(const Node<T>* const nd, const std::uint8_t depth = 0, const char marker = 'T')
{
    TEST_ASSERT_LESS_THAN(10, get_height(nd)); // Fail early for malformed cyclic trees, do not overwhelm stdout.
    if (nd != nullptr) {
        print<T>(reinterpret_cast<const Node<T>*>(nd->lr[0]), static_cast<std::uint8_t>(depth + 1U), 'L');
        for (std::uint16_t i = 1U; i < depth; i++) {
            std::printf("              ");
        }
        if (marker == 'L') {
            std::printf(" .............");
        } else if (marker == 'R') {
            std::printf(" `````````````");
        } else {
            (void)0;
        }
        std::printf("%c=%lld [%d]\n", marker, static_cast<long long>(nd->value), nd->bf);
        print<T>(reinterpret_cast<const Node<T>*>(nd->lr[1]), static_cast<std::uint8_t>(depth + 1U), 'R');
    }
}

template<bool Ascending, typename Node, typename Visitor>
void traverse(Node* const root, const Visitor& visitor)
{
    if (root != nullptr) {
        traverse<Ascending, Node, Visitor>(reinterpret_cast<Node*>(root->lr[!Ascending]), visitor);
        visitor(root);
        traverse<Ascending, Node, Visitor>(reinterpret_cast<Node*>(root->lr[Ascending]), visitor);
    }
}

template<typename T>
void print_graphviz(const Node<T>* const nd)
{
    TEST_ASSERT_LESS_THAN(12, get_height(nd)); // Fail early for malformed cyclic trees, do not overwhelm stdout.
    std::puts("// Feed the following text to Graphviz, or use an online UI like https://edotor.net/");
    std::puts("digraph {");
    std::puts(
      "node [style=filled,shape=circle,fontcolor=white,penwidth=0,fontname=\"monospace\",fixedsize=1,fontsize=18];");
    std::puts("edge [arrowhead=none,penwidth=2];");
    std::puts("nodesep=0.0;ranksep=0.3;splines=false;");
    traverse<true>(nd, [](const Node<T>* const x) {
        // NOLINTNEXTLINE(*-avoid-nested-conditional-operator)
        const char* const fill_color = (x->bf == 0) ? "black" : ((x->bf > 0) ? "orange" : "blue");
        std::printf("%u[fillcolor=%s];", static_cast<unsigned>(x->value), fill_color);
    });
    std::puts("");
    traverse<true>(nd, [](const Node<T>* const x) {
        if (x->lr[0] != nullptr) {
            std::printf("%u:sw->%u:n;",
                        static_cast<unsigned>(x->value),
                        static_cast<unsigned>(reinterpret_cast<Node<T>*>(x->lr[0])->value));
        }
        if (x->lr[1] != nullptr) {
            std::printf("%u:se->%u:n;",
                        static_cast<unsigned>(x->value),
                        static_cast<unsigned>(reinterpret_cast<Node<T>*>(x->lr[1])->value));
        }
    });
    std::puts("\n}");
}

template<typename T>
std::optional<std::size_t> check_ascension(const Node<T>* const root)
{
    const Node<T>* prev  = nullptr;
    bool           valid = true;
    std::size_t    size  = 0;
    traverse<true, const Node<T>>(root, [&](const Node<T>* const nd) {
        if (prev != nullptr) {
            valid = valid && (prev->value < nd->value);
        }
        prev = nd;
        size++;
    });
    return valid ? std::optional<std::size_t>(size) : std::optional<std::size_t>{};
}

template<typename T>
const Node<T>* find_broken_ancestry(const Node<T>* const n, const cavl2_t* const parent = nullptr)
{
    if ((n != nullptr) && (n->up == parent)) {
        for (auto* ch : n->lr) {
            if (const Node<T>* p = find_broken_ancestry(reinterpret_cast<Node<T>*>(ch), n)) {
                return p;
            }
        }
        return nullptr;
    }
    return n;
}

template<typename T>
const cavl2_t* find_broken_bf(const Node<T>* const n)
{
    if (n != nullptr) {
        if (std::abs(n->bf) > 1) {
            return n;
        }
        const std::int16_t hl = get_height(reinterpret_cast<Node<T>*>(n->lr[0]));
        const std::int16_t hr = get_height(reinterpret_cast<Node<T>*>(n->lr[1]));
        if (n->bf != (hr - hl)) {
            return n;
        }
        for (auto* ch : n->lr) {
            if (const cavl2_t* p = find_broken_bf(reinterpret_cast<Node<T>*>(ch))) {
                return p;
            }
        }
    }
    return nullptr;
}

void test_check_ascension()
{
    using N = Node<std::uint8_t>;
    N t{ 2 };
    N l{ 1 };
    N r{ 3 };
    N rr{ 4 };
    // Correctly arranged tree -- smaller items on the left.
    t.lr[0] = &l;
    t.lr[1] = &r;
    r.lr[1] = &rr;
    TEST_ASSERT_EQUAL(4, check_ascension(&t));
    TEST_ASSERT_EQUAL(3, get_height(&t));
    // Break the arrangement and make sure the breakage is detected.
    t.lr[1] = &l;
    t.lr[0] = &r;
    TEST_ASSERT_NOT_EQUAL(4, check_ascension(&t));
    TEST_ASSERT_EQUAL(3, get_height(&t));
    TEST_ASSERT_EQUAL(&t, find_broken_bf(&t)); // All zeros, incorrect.
    r.lr[1] = nullptr;
    TEST_ASSERT_EQUAL(2, get_height(&t));
    TEST_ASSERT_NULL(find_broken_bf(&t)); // Balanced now as we removed one node.
}

void test_rotation()
{
    using N = Node<std::uint8_t>;
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
    N c{ { Zz, { Zz, Zz }, 0 }, 3 };
    N b{ { Zz, { Zz, Zz }, 0 }, 2 };
    N a{ { Zz, { Zz, Zz }, 0 }, 1 };
    N z{ { Zz, { &b, &c }, 0 }, 8 };
    N x{ { Zz, { &a, &z }, 1 }, 9 };
    z.up = &x;
    c.up = &z;
    b.up = &z;
    a.up = &x;

    std::printf("Before rotation:\n");
    TEST_ASSERT_NULL(find_broken_ancestry(&x));
    print(&x);

    std::printf("After left rotation:\n");
    _cavl2_rotate(&x, false); // z is now the root
    TEST_ASSERT_NULL(find_broken_ancestry(&z));
    print(&z);
    TEST_ASSERT_EQUAL(&a, x.lr[0]);
    TEST_ASSERT_EQUAL(&b, x.lr[1]);
    TEST_ASSERT_EQUAL(&x, z.lr[0]);
    TEST_ASSERT_EQUAL(&c, z.lr[1]);

    std::printf("After right rotation, back into the original configuration:\n");
    _cavl2_rotate(&z, true); // x is now the root
    TEST_ASSERT_NULL(find_broken_ancestry(&x));
    print(&x);
    TEST_ASSERT_EQUAL(&a, x.lr[0]);
    TEST_ASSERT_EQUAL(&z, x.lr[1]);
    TEST_ASSERT_EQUAL(&b, z.lr[0]);
    TEST_ASSERT_EQUAL(&c, z.lr[1]);
}

void test_balancing_a()
{
    using N = Node<std::uint8_t>;
    // Double left-right rotation.
    //     X             X           Y
    //    / `           / `        /   `
    //   Z   C   =>    Y   C  =>  Z     X
    //  / `           / `        / `   / `
    // D   Y         Z   G      D   F G   C
    //    / `       / `
    //   F   G     D   F
    N x{ { Zz, { Zz, Zz }, 0 }, 1 }; // bf = -2
    N z{ { &x, { Zz, Zz }, 0 }, 2 }; // bf = +1
    N c{ { &x, { Zz, Zz }, 0 }, 3 };
    N d{ { &z, { Zz, Zz }, 0 }, 4 };
    N y{ { &z, { Zz, Zz }, 0 }, 5 };
    N f{ { &y, { Zz, Zz }, 0 }, 6 };
    N g{ { &y, { Zz, Zz }, 0 }, 7 };
    x.lr[0] = &z;
    x.lr[1] = &c;
    z.lr[0] = &d;
    z.lr[1] = &y;
    y.lr[0] = &f;
    y.lr[1] = &g;
    print(&x);
    TEST_ASSERT_NULL(find_broken_ancestry(&x));
    TEST_ASSERT_EQUAL(&x, _cavl2_adjust_balance(&x, false)); // bf = -1, same topology
    TEST_ASSERT_EQUAL(-1, x.bf);
    TEST_ASSERT_EQUAL(&z, _cavl2_adjust_balance(&z, true)); // bf = +1, same topology
    TEST_ASSERT_EQUAL(+1, z.bf);
    TEST_ASSERT_EQUAL(&y, _cavl2_adjust_balance(&x, false)); // bf = -2, rotation needed
    print(&y);
    TEST_ASSERT_NULL(find_broken_bf(&y)); // Should be balanced now.
    TEST_ASSERT_NULL(find_broken_ancestry(&y));
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

void test_balancing_b()
{
    using N = Node<std::uint8_t>;
    // Without F the handling of Z and Y is more complex; Z flips the sign of its balance factor:
    //     X             X           Y
    //    / `           / `        /   `
    //   Z   C   =>    Y   C  =>  Z     X
    //  / `           / `        /     / `
    // D   Y         Z   G      D     G   C
    //      `       /
    //       G     D
    N x{};
    N z{};
    N c{};
    N d{};
    N y{};
    N g{};
    x = { { Zz, { &z, &c }, 0 }, 1 }; // bf = -2
    z = { { &x, { &d, &y }, 0 }, 2 }; // bf = +1
    c = { { &x, { Zz, Zz }, 0 }, 3 };
    d = { { &z, { Zz, Zz }, 0 }, 4 };
    y = { { &z, { Zz, &g }, 0 }, 5 }; // bf = +1
    g = { { &y, { Zz, Zz }, 0 }, 7 };
    print(&x);
    TEST_ASSERT_NULL(find_broken_ancestry(&x));
    TEST_ASSERT_EQUAL(&x, _cavl2_adjust_balance(&x, false)); // bf = -1, same topology
    TEST_ASSERT_EQUAL(-1, x.bf);
    TEST_ASSERT_EQUAL(&z, _cavl2_adjust_balance(&z, true)); // bf = +1, same topology
    TEST_ASSERT_EQUAL(+1, z.bf);
    TEST_ASSERT_EQUAL(&y, _cavl2_adjust_balance(&y, true)); // bf = +1, same topology
    TEST_ASSERT_EQUAL(+1, y.bf);
    TEST_ASSERT_EQUAL(&y, _cavl2_adjust_balance(&x, false)); // bf = -2, rotation needed
    print(&y);
    TEST_ASSERT_NULL(find_broken_bf(&y)); // Should be balanced now.
    TEST_ASSERT_NULL(find_broken_ancestry(&y));
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

void test_balancing_c()
{
    using N = Node<std::uint8_t>;
    // Both X and Z are heavy on the same side.
    //       X              Z
    //      / `           /   `
    //     Z   C   =>    D     X
    //    / `           / `   / `
    //   D   Y         F   G Y   C
    //  / `
    // F   G
    N x{};
    N z{};
    N c{};
    N d{};
    N y{};
    N f{};
    N g{};
    x = { { Zz, { &z, &c }, 0 }, 1 }; // bf = -2
    z = { { &x, { &d, &y }, 0 }, 2 }; // bf = -1
    c = { { &x, { Zz, Zz }, 0 }, 3 };
    d = { { &z, { &f, &g }, 0 }, 4 };
    y = { { &z, { Zz, Zz }, 0 }, 5 };
    f = { { &d, { Zz, Zz }, 0 }, 6 };
    g = { { &d, { Zz, Zz }, 0 }, 7 };
    print(&x);
    TEST_ASSERT_NULL(find_broken_ancestry(&x));
    TEST_ASSERT_EQUAL(&x, _cavl2_adjust_balance(&x, false)); // bf = -1, same topology
    TEST_ASSERT_EQUAL(-1, x.bf);
    TEST_ASSERT_EQUAL(&z, _cavl2_adjust_balance(&z, false)); // bf = -1, same topology
    TEST_ASSERT_EQUAL(-1, z.bf);
    TEST_ASSERT_EQUAL(&z, _cavl2_adjust_balance(&x, false));
    print(&z);
    TEST_ASSERT_NULL(find_broken_bf(&z));
    TEST_ASSERT_NULL(find_broken_ancestry(&z));
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

void test_retracing_on_growth()
{
    using N = Node<std::uint8_t>;
    N t[100]{};
    for (std::uint8_t i = 0; i < 100; i++) {
        t[i].value = i;
    }
    //        50              30
    //      /   `            /   `
    //     30   60?   =>    20   50
    //    / `              /    /  `
    //   20 40?           10   40? 60?
    //  /
    // 10
    t[50] = { Zzzzzz, { &t[30], &t[60] }, -1 };
    t[30] = { &t[50], { &t[20], &t[40] }, 00 };
    t[60] = { &t[50], { Zzzzzz, Zzzzzz }, 00 };
    t[20] = { &t[30], { &t[10], Zzzzzz }, 00 };
    t[40] = { &t[30], { Zzzzzz, Zzzzzz }, 00 };
    t[10] = { &t[20], { Zzzzzz, Zzzzzz }, 00 };
    print(&t[50]); // The tree is imbalanced because we just added 1 and are about to retrace it.
    TEST_ASSERT_NULL(find_broken_ancestry(&t[50]));
    TEST_ASSERT_EQUAL(6, check_ascension(&t[50]));
    TEST_ASSERT_EQUAL(&t[30], _cavl2_retrace_on_growth(&t[10]));
    std::puts("ADD 10:");
    print(&t[30]); // This is the new root.
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
    TEST_ASSERT_NULL(find_broken_ancestry(&t[30]));
    TEST_ASSERT_NULL(find_broken_bf(&t[30]));
    TEST_ASSERT_EQUAL(6, check_ascension(&t[30]));
    // Add a new child under 20 and ensure that retracing stops at 20 because it becomes perfectly balanced:
    //          30
    //         /   `
    //       20    50
    //      /  `  /  `
    //     10 21 40 60
    TEST_ASSERT_NULL(find_broken_ancestry(&t[30]));
    TEST_ASSERT_NULL(find_broken_bf(&t[30]));
    t[21]       = { &t[20], { Zzzzzz, Zzzzzz }, 0 };
    t[20].lr[1] = &t[21];
    TEST_ASSERT_NULL(_cavl2_retrace_on_growth(&t[21])); // Root not reached, NULL returned.
    std::puts("ADD 21:");
    print(&t[30]);
    TEST_ASSERT_EQUAL(0, t[20].bf);
    TEST_ASSERT_EQUAL(0, t[30].bf);
    TEST_ASSERT_NULL(find_broken_ancestry(&t[30]));
    TEST_ASSERT_NULL(find_broken_bf(&t[30]));
    TEST_ASSERT_EQUAL(7, check_ascension(&t[30]));
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
    TEST_ASSERT_NULL(find_broken_ancestry(&t[30]));
    TEST_ASSERT_NULL(find_broken_bf(&t[30]));
    TEST_ASSERT_EQUAL(7, check_ascension(&t[30]));
    t[15]       = { &t[10], { Zzzzzz, Zzzzzz }, 0 };
    t[10].lr[1] = &t[15];
    TEST_ASSERT_EQUAL(&t[30], _cavl2_retrace_on_growth(&t[15])); // Same root, its balance becomes -1.
    print(&t[30]);
    TEST_ASSERT_EQUAL(+1, t[10].bf);
    TEST_ASSERT_EQUAL(-1, t[20].bf);
    TEST_ASSERT_EQUAL(-1, t[30].bf);
    TEST_ASSERT_NULL(find_broken_ancestry(&t[30]));
    TEST_ASSERT_NULL(find_broken_bf(&t[30]));
    TEST_ASSERT_EQUAL(8, check_ascension(&t[30]));

    std::puts("ADD 17:");
    t[17]       = { &t[15], { Zzzzzz, Zzzzzz }, 0 };
    t[15].lr[1] = &t[17];
    TEST_ASSERT_EQUAL(nullptr, _cavl2_retrace_on_growth(&t[17])); // Same root, same balance, 10 rotated left.
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
    TEST_ASSERT_NULL(find_broken_ancestry(&t[30]));
    TEST_ASSERT_NULL(find_broken_bf(&t[30]));
    TEST_ASSERT_EQUAL(9, check_ascension(&t[30]));

    std::puts("ADD 18:");
    t[18]       = { &t[17], { Zzzzzz, Zzzzzz }, 0 };
    t[17].lr[1] = &t[18];
    TEST_ASSERT_EQUAL(nullptr, _cavl2_retrace_on_growth(&t[18])); // Same root, 15 went left, 20 went right.
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
    TEST_ASSERT_NULL(find_broken_ancestry(&t[30]));
    TEST_ASSERT_NULL(find_broken_bf(&t[30]));
    TEST_ASSERT_EQUAL(10, check_ascension(&t[30]));
}

void test_find_trivial()
{
    using N = Node<std::uint8_t>;
    //      A
    //    B   C
    //   D E F G
    N a{ 4 };
    N b{ 2 };
    N c{ 6 };
    N d{ 1 };
    N e{ 3 };
    N f{ 5 };
    N g{ 7 };
    N q{ 9 };
    a = { Zz, { &b, &c }, 0 };
    b = { &a, { &d, &e }, 0 };
    c = { &a, { &f, &g }, 0 };
    d = { &b, { Zz, Zz }, 0 };
    e = { &b, { Zz, Zz }, 0 };
    f = { &c, { Zz, Zz }, 0 };
    g = { &c, { Zz, Zz }, 0 };
    q = { Zz, { Zz, Zz }, 0 };
    TEST_ASSERT_NULL(find_broken_bf(&a));
    TEST_ASSERT_NULL(find_broken_ancestry(&a));
    TEST_ASSERT_EQUAL(7, check_ascension(&a));
    N* root = &a;
    // Bad arguments:
    TEST_ASSERT_NULL(cavl2_find_or_insert(reinterpret_cast<cavl2_t**>(&root), nullptr, nullptr, nullptr, nullptr));
    TEST_ASSERT_EQUAL(&a, root);
    TEST_ASSERT_NULL(find(root, [&](const N& v) { return q.value - v.value; }));
    TEST_ASSERT_EQUAL(&a, root);
    TEST_ASSERT_EQUAL(&e, find(root, [&](const N& v) { return e.value - v.value; }));
    TEST_ASSERT_EQUAL(&b, find(root, [&](const N& v) { return b.value - v.value; }));
    TEST_ASSERT_EQUAL(&a, root);
    print(&a);
    TEST_ASSERT_EQUAL(nullptr, cavl2_extremum(nullptr, true));
    TEST_ASSERT_EQUAL(nullptr, cavl2_extremum(nullptr, false));
    TEST_ASSERT_EQUAL(&g, a.max());
    TEST_ASSERT_EQUAL(&d, a.min());
    TEST_ASSERT_EQUAL(&g, g.max());
    TEST_ASSERT_EQUAL(&g, g.min());
    TEST_ASSERT_EQUAL(&d, d.max());
    TEST_ASSERT_EQUAL(&d, d.min());
}

void test_removal_a()
{
    using N = Node<std::uint8_t>;
    //        4
    //      /   `
    //    2       6
    //   / `     / `
    //  1   3   5   8
    //             / `
    //            7   9
    N t[10]{};
    for (std::uint8_t i = 0; i < 10; i++) {
        t[i].value = i;
    }
    t[1]    = { &t[2], { Zzzzz, Zzzzz }, 00 };
    t[2]    = { &t[4], { &t[1], &t[3] }, 00 };
    t[3]    = { &t[2], { Zzzzz, Zzzzz }, 00 };
    t[4]    = { Zzzzz, { &t[2], &t[6] }, +1 };
    t[5]    = { &t[6], { Zzzzz, Zzzzz }, 00 };
    t[6]    = { &t[4], { &t[5], &t[8] }, +1 };
    t[7]    = { &t[8], { Zzzzz, Zzzzz }, 00 };
    t[8]    = { &t[6], { &t[7], &t[9] }, 00 };
    t[9]    = { &t[8], { Zzzzz, Zzzzz }, 00 };
    N* root = &t[4];
    print(root);
    TEST_ASSERT_NULL(find_broken_bf(root));
    TEST_ASSERT_NULL(find_broken_ancestry(root));
    TEST_ASSERT_EQUAL(9, check_ascension(root));

    // Remove 9, the easiest case. The rest of the tree remains unchanged.
    //        4
    //      /   `
    //    2       6
    //   / `     / `
    //  1   3   5   8
    //             /
    //            7
    std::puts("REMOVE 9:");
    remove(&root, &t[9]);
    TEST_ASSERT_EQUAL(&t[4], root);
    print(root);
    TEST_ASSERT_NULL(find_broken_bf(root));
    TEST_ASSERT_NULL(find_broken_ancestry(root));
    TEST_ASSERT_EQUAL(8, check_ascension(root));
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
    TEST_ASSERT_EQUAL(Zzzzz, t[4].up); // Nihil Supernum
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
    remove(&root, &t[8]);
    TEST_ASSERT_EQUAL(&t[4], root);
    print(root);
    TEST_ASSERT_NULL(find_broken_bf(root));
    TEST_ASSERT_NULL(find_broken_ancestry(root));
    TEST_ASSERT_EQUAL(7, check_ascension(root));
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
    TEST_ASSERT_EQUAL(Zzzzz, t[4].up); // Nihil Supernum
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
    remove(&root, &t[4]);
    print(root);
    TEST_ASSERT_EQUAL(&t[5], root);
    TEST_ASSERT_NULL(find_broken_bf(root));
    TEST_ASSERT_NULL(find_broken_ancestry(root));
    TEST_ASSERT_EQUAL(6, check_ascension(root));
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
    TEST_ASSERT_EQUAL(Zzzzz, t[5].up); // Nihil Supernum
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
    remove(&root, &t[5]);
    TEST_ASSERT_EQUAL(&t[6], root);
    print(root);
    TEST_ASSERT_NULL(find_broken_bf(root));
    TEST_ASSERT_NULL(find_broken_ancestry(root));
    TEST_ASSERT_EQUAL(5, check_ascension(root));
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
    TEST_ASSERT_EQUAL(Zzzzz, t[6].up); // Nihil Supernum
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
    remove(&root, &t[6]);
    TEST_ASSERT_EQUAL(&t[2], root);
    print(root);
    TEST_ASSERT_NULL(find_broken_bf(root));
    TEST_ASSERT_NULL(find_broken_ancestry(root));
    TEST_ASSERT_EQUAL(4, check_ascension(root));
    // 1
    TEST_ASSERT_EQUAL(&t[2], t[1].up);
    TEST_ASSERT_EQUAL(Zzzzz, t[1].lr[0]);
    TEST_ASSERT_EQUAL(Zzzzz, t[1].lr[1]);
    TEST_ASSERT_EQUAL(00, t[1].bf);
    // 2
    TEST_ASSERT_EQUAL(Zzzzz, t[2].up); // Nihil Supernum
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
    remove(&root, &t[1]);
    TEST_ASSERT_EQUAL(&t[3], root);
    print(root);
    TEST_ASSERT_NULL(find_broken_bf(root));
    TEST_ASSERT_NULL(find_broken_ancestry(root));
    TEST_ASSERT_EQUAL(3, check_ascension(root));
    // 2
    TEST_ASSERT_EQUAL(&t[3], t[2].up);
    TEST_ASSERT_EQUAL(Zzzzz, t[2].lr[0]);
    TEST_ASSERT_EQUAL(Zzzzz, t[2].lr[1]);
    TEST_ASSERT_EQUAL(0, t[2].bf);
    // 3
    TEST_ASSERT_EQUAL(Zzzzz, t[3].up); // Nihil Supernum
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
    remove(&root, &t[7]);
    TEST_ASSERT_EQUAL(&t[3], root);
    print(root);
    TEST_ASSERT_NULL(find_broken_bf(root));
    TEST_ASSERT_NULL(find_broken_ancestry(root));
    TEST_ASSERT_EQUAL(2, check_ascension(root));
    // 2
    TEST_ASSERT_EQUAL(&t[3], t[2].up);
    TEST_ASSERT_EQUAL(Zzzzz, t[2].lr[0]);
    TEST_ASSERT_EQUAL(Zzzzz, t[2].lr[1]);
    TEST_ASSERT_EQUAL(0, t[2].bf);
    // 3
    TEST_ASSERT_EQUAL(Zzzzz, t[3].up); // Nihil Supernum
    TEST_ASSERT_EQUAL(&t[2], t[3].lr[0]);
    TEST_ASSERT_EQUAL(Zzzzz, t[3].lr[1]);
    TEST_ASSERT_EQUAL(-1, t[3].bf);

    // Remove 3. Only 2 is left, which is now obviously the root.
    std::puts("REMOVE 3:");
    remove(&root, &t[3]);
    TEST_ASSERT_EQUAL(&t[2], root);
    print(root);
    TEST_ASSERT_NULL(find_broken_bf(root));
    TEST_ASSERT_NULL(find_broken_ancestry(root));
    TEST_ASSERT_EQUAL(1, check_ascension(root));
    // 2
    TEST_ASSERT_EQUAL(Zzzzz, t[2].up);
    TEST_ASSERT_EQUAL(Zzzzz, t[2].lr[0]);
    TEST_ASSERT_EQUAL(Zzzzz, t[2].lr[1]);
    TEST_ASSERT_EQUAL(0, t[2].bf);

    // Remove 2. The tree is now empty, make sure the root pointer is updated accordingly.
    std::puts("REMOVE 2:");
    remove(&root, &t[2]);
    TEST_ASSERT_EQUAL(nullptr, root);
}

void test_mutation_manual()
{
    using N = Node<std::uint8_t>;
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
    N t[32]{};
    for (std::uint8_t i = 0; i < 32; i++) {
        t[i].value = i;
    }
    // Build the actual tree.
    N* root = nullptr;
    for (std::uint8_t i = 1; i < 32; i++) {
        const auto pred = [&](const N& v) { return t[i].value - v.value; };
        TEST_ASSERT_NULL(find(root, pred));
        TEST_ASSERT_EQUAL(&t[i], find_or_insert(&root, pred, [&]() { return &t[i]; }));
        TEST_ASSERT_EQUAL(&t[i], find(root, pred));
        // Validate the tree after every mutation.
        TEST_ASSERT_NOT_NULL(root);
        TEST_ASSERT_NULL(find_broken_bf(root));
        TEST_ASSERT_NULL(find_broken_ancestry(root));
        TEST_ASSERT_EQUAL(i, check_ascension(root));
    }
    print(root);
    TEST_ASSERT_NULL(find_broken_bf(root));
    TEST_ASSERT_NULL(find_broken_ancestry(root));
    TEST_ASSERT_EQUAL(31, check_ascension(root));
    // Check composition -- ensure that every element is in the tree and it is there exactly once.
    {
        bool seen[32]{};
        traverse<true>(root, [&](const N* const n) {
            TEST_ASSERT_FALSE(seen[n->value]);
            seen[n->value] = true;
        });
        TEST_ASSERT(std::all_of(&seen[1], &seen[31], [](const bool x) { return x; }));
    }

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
    std::puts("REMOVE 24:");
    TEST_ASSERT(t[24].check_linkage_up_left_right_bf(&t[16], &t[20], &t[28], 00));
    remove(&root, &t[24]);
    TEST_ASSERT_EQUAL(&t[16], root);
    print(root);
    TEST_ASSERT(t[25].check_linkage_up_left_right_bf(&t[16], &t[20], &t[28], 00));
    TEST_ASSERT(t[26].check_linkage_up_left_right_bf(&t[28], Zzzzzz, &t[27], +1));
    TEST_ASSERT_NULL(find_broken_bf(root));
    TEST_ASSERT_NULL(find_broken_ancestry(root));
    TEST_ASSERT_EQUAL(30, check_ascension(root));

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
    std::puts("REMOVE 25:");
    TEST_ASSERT(t[25].check_linkage_up_left_right_bf(&t[16], &t[20], &t[28], 00));
    remove(&root, &t[25]);
    TEST_ASSERT_EQUAL(&t[16], root);
    print(root);
    TEST_ASSERT(t[26].check_linkage_up_left_right_bf(&t[16], &t[20], &t[28], 00));
    TEST_ASSERT(t[28].check_linkage_up_left_right_bf(&t[26], &t[27], &t[30], +1));
    TEST_ASSERT_NULL(find_broken_bf(root));
    TEST_ASSERT_NULL(find_broken_ancestry(root));
    TEST_ASSERT_EQUAL(29, check_ascension(root));

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
    std::puts("REMOVE 26:");
    TEST_ASSERT(t[26].check_linkage_up_left_right_bf(&t[16], &t[20], &t[28], 00));
    remove(&root, &t[26]);
    TEST_ASSERT_EQUAL(&t[16], root);
    print(root);
    TEST_ASSERT(t[27].check_linkage_up_left_right_bf(&t[16], &t[20], &t[30], 00));
    TEST_ASSERT(t[30].check_linkage_up_left_right_bf(&t[27], &t[28], &t[31], -1));
    TEST_ASSERT(t[28].check_linkage_up_left_right_bf(&t[30], Zzzzzz, &t[29], +1));
    TEST_ASSERT_NULL(find_broken_bf(root));
    TEST_ASSERT_NULL(find_broken_ancestry(root));
    TEST_ASSERT_EQUAL(28, check_ascension(root));

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
    std::puts("REMOVE 20:");
    TEST_ASSERT(t[20].check_linkage_up_left_right_bf(&t[27], &t[18], &t[22], 00));
    remove(&root, &t[20]);
    TEST_ASSERT_EQUAL(&t[16], root);
    print(root);
    TEST_ASSERT(t[21].check_linkage_up_left_right_bf(&t[27], &t[18], &t[22], 00));
    TEST_ASSERT(t[22].check_linkage_up_left_right_bf(&t[21], Zzzzzz, &t[23], +1));
    TEST_ASSERT_NULL(find_broken_bf(root));
    TEST_ASSERT_NULL(find_broken_ancestry(root));
    TEST_ASSERT_EQUAL(27, check_ascension(root));

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
    std::puts("REMOVE 27:");
    TEST_ASSERT(t[27].check_linkage_up_left_right_bf(&t[16], &t[21], &t[30], 00));
    remove(&root, &t[27]);
    TEST_ASSERT_EQUAL(&t[16], root);
    print(root);
    TEST_ASSERT(t[28].check_linkage_up_left_right_bf(&t[16], &t[21], &t[30], -1));
    TEST_ASSERT(t[30].check_linkage_up_left_right_bf(&t[28], &t[29], &t[31], 00));
    TEST_ASSERT_NULL(find_broken_bf(root));
    TEST_ASSERT_NULL(find_broken_ancestry(root));
    TEST_ASSERT_EQUAL(26, check_ascension(root));

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
    std::puts("REMOVE 28:");
    TEST_ASSERT(t[28].check_linkage_up_left_right_bf(&t[16], &t[21], &t[30], -1));
    remove(&root, &t[28]);
    TEST_ASSERT_EQUAL(&t[16], root);
    print(root);
    TEST_ASSERT(t[29].check_linkage_up_left_right_bf(&t[16], &t[21], &t[30], -1));
    TEST_ASSERT(t[30].check_linkage_up_left_right_bf(&t[29], Zzzzzz, &t[31], +1));
    TEST_ASSERT_NULL(find_broken_bf(root));
    TEST_ASSERT_NULL(find_broken_ancestry(root));
    TEST_ASSERT_EQUAL(25, check_ascension(root));

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
    std::puts("REMOVE 29:");
    TEST_ASSERT(t[29].check_linkage_up_left_right_bf(&t[16], &t[21], &t[30], -1));
    remove(&root, &t[29]);
    TEST_ASSERT_EQUAL(&t[16], root);
    print(root);
    TEST_ASSERT(t[21].check_linkage_up_left_right_bf(&t[16], &t[18], &t[30], +1));
    TEST_ASSERT(t[18].check_linkage_up_left_right_bf(&t[21], &t[17], &t[19], 00));
    TEST_ASSERT(t[30].check_linkage_up_left_right_bf(&t[21], &t[22], &t[31], -1));
    TEST_ASSERT(t[22].check_linkage_up_left_right_bf(&t[30], Zzzzzz, &t[23], +1));
    TEST_ASSERT(t[16].check_linkage_up_left_right_bf(Zzzzzz, &t[8], &t[21], 00));
    TEST_ASSERT_NULL(find_broken_bf(root));
    TEST_ASSERT_NULL(find_broken_ancestry(root));
    TEST_ASSERT_EQUAL(24, check_ascension(root));

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
    std::puts("REMOVE 8:");
    TEST_ASSERT(t[8].check_linkage_up_left_right_bf(&t[16], &t[4], &t[12], 00));
    remove(&root, &t[8]);
    TEST_ASSERT_EQUAL(&t[16], root);
    print(root);
    TEST_ASSERT(t[9].check_linkage_up_left_right_bf(&t[16], &t[4], &t[12], 00));
    TEST_ASSERT(t[10].check_linkage_up_left_right_bf(&t[12], Zzzzz, &t[11], +1));
    TEST_ASSERT_NULL(find_broken_bf(root));
    TEST_ASSERT_NULL(find_broken_ancestry(root));
    TEST_ASSERT_EQUAL(23, check_ascension(root));

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
    std::puts("REMOVE 9:");
    TEST_ASSERT(t[9].check_linkage_up_left_right_bf(&t[16], &t[4], &t[12], 00));
    remove(&root, &t[9]);
    TEST_ASSERT_EQUAL(&t[16], root);
    print(root);
    TEST_ASSERT(t[10].check_linkage_up_left_right_bf(&t[16], &t[4], &t[12], 00));
    TEST_ASSERT(t[12].check_linkage_up_left_right_bf(&t[10], &t[11], &t[14], +1));
    TEST_ASSERT_NULL(find_broken_bf(root));
    TEST_ASSERT_NULL(find_broken_ancestry(root));
    TEST_ASSERT_EQUAL(22, check_ascension(root));

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
    std::puts("REMOVE 1:");
    TEST_ASSERT(t[1].check_linkage_up_left_right_bf(&t[2], Zzzzz, Zzzzz, 00));
    remove(&root, &t[1]);
    TEST_ASSERT_EQUAL(&t[16], root);
    print(root);
    TEST_ASSERT(t[2].check_linkage_up_left_right_bf(&t[4], Zzzzz, &t[3], +1));
    TEST_ASSERT_NULL(find_broken_bf(root));
    TEST_ASSERT_NULL(find_broken_ancestry(root));
    TEST_ASSERT_EQUAL(21, check_ascension(root));
}

std::uint8_t get_random_byte()
{
    return static_cast<std::uint8_t>((0xFFLL * std::rand()) / RAND_MAX);
}

void test_mutation_randomized()
{
    using N = Node<std::uint8_t>;
    std::array<N, 256> t{};
    for (auto i = 0U; i < 256U; i++) {
        t.at(i).value = static_cast<std::uint8_t>(i);
    }
    std::array<bool, 256> mask{};
    std::size_t           size = 0;
    N*                    root = nullptr;

    std::uint64_t cnt_addition = 0;
    std::uint64_t cnt_removal  = 0;

    const auto validate = [&] {
        TEST_ASSERT_EQUAL(size,
                          std::accumulate(mask.begin(), mask.end(), 0U, [](const std::size_t a, const std::size_t b) {
                              return a + b;
                          }));
        TEST_ASSERT_NULL(find_broken_bf(root));
        TEST_ASSERT_NULL(find_broken_ancestry(root));
        TEST_ASSERT_EQUAL(size, check_ascension(root));
        std::array<bool, 256> new_mask{};
        traverse<true>(root, [&](const N* node) { new_mask.at(node->value) = true; });
        TEST_ASSERT_EQUAL(mask, new_mask); // Otherwise, the contents of the tree does not match our expectations.
    };
    validate();

    const auto add = [&](const std::uint8_t x) {
        const auto comparator = [&](const N& v) { return x - v.value; };
        if (const N* const existing = find(root, comparator)) {
            TEST_ASSERT_TRUE(mask.at(x));
            TEST_ASSERT_EQUAL(x, existing->value);
            TEST_ASSERT_EQUAL(x, find_or_insert(&root, comparator, []() -> N* {
                                     TEST_FAIL_MESSAGE("Attempted to create a new node when there is one already");
                                     return nullptr;
                                 })->value);
        } else {
            TEST_ASSERT_FALSE(mask.at(x));
            bool factory_called = false;
            TEST_ASSERT_EQUAL(x, find_or_insert(&root, comparator, [&]() -> N* {
                                     factory_called = true;
                                     return &t.at(x);
                                 })->value);
            TEST_ASSERT(factory_called);
            size++;
            cnt_addition++;
            mask.at(x) = true;
        }
    };

    const auto drop = [&](const std::uint8_t x) {
        const auto comparator = [&](const N& v) { return x - v.value; };
        if (const N* const existing = find(root, comparator)) {
            TEST_ASSERT_TRUE(mask.at(x));
            TEST_ASSERT_EQUAL(x, existing->value);
            remove(&root, existing);
            size--;
            cnt_removal++;
            mask.at(x) = false;
            TEST_ASSERT_NULL(find(root, comparator));
        } else {
            TEST_ASSERT_FALSE(mask.at(x));
        }
    };

    std::puts("Running the randomized test...");
    for (std::uint32_t iteration = 0U; iteration < 100'000U; iteration++) {
        if ((get_random_byte() % 2U) != 0) {
            add(get_random_byte());
        } else {
            drop(get_random_byte());
        }
        validate();
    }

    std::puts("Randomized test finished. Final state:");
    std::printf("\tsize:         %u\n", static_cast<unsigned>(size));
    std::printf("\tcnt_addition: %u\n", static_cast<unsigned>(cnt_addition));
    std::printf("\tcnt_removal:  %u\n", static_cast<unsigned>(cnt_removal));
    if (root != nullptr) {
        std::printf("\tmin/max:      %u/%u\n",
                    static_cast<unsigned>(root->min()->value),
                    static_cast<unsigned>(root->max()->value));
    }
    print_graphviz(root);
    validate();

    std::puts("Running the traversal test...");
    std::int16_t last_value = -1;
    for (N* p = root->min(); p != nullptr; p = reinterpret_cast<N*>(cavl2_next_greater(p))) {
        TEST_ASSERT_GREATER_THAN(last_value, static_cast<std::int16_t>(p->value));
        last_value = p->value;
        std::printf("%u ", static_cast<unsigned>(p->value));
    }
    std::puts("");
}

void test_traversal_full()
{
    using N = Node<std::uint8_t>;
    std::array<N, 256> t{};
    for (std::size_t i = 0U; i < 256U; i++) {
        t.at(i).value = static_cast<std::uint8_t>(i);
    }
    N* root = nullptr;

    // Add all nodes into the tree in order.
    for (std::size_t i = 0; i < 256U; i++) {
        const auto pred = [&](const N& v) { return t[i].value - v.value; };
        TEST_ASSERT_NULL(find(root, pred));
        TEST_ASSERT_EQUAL(&t[i], find_or_insert(&root, pred, [&] { return &t[i]; }));
        TEST_ASSERT_EQUAL(&t[i], find(root, pred));
    }

    // Traverse the tree and ensure we get each node in order.
    std::int16_t last_value = -1;
    N*           node       = root->min();
    TEST_ASSERT_EQUAL(0, node->value);
    while (node != nullptr) {
        TEST_ASSERT_EQUAL(last_value + 1, static_cast<std::int16_t>(node->value));
        last_value = node->value;
        node       = reinterpret_cast<N*>(cavl2_next_greater(node));
    }
    TEST_ASSERT_EQUAL(255, last_value);
}

void test_trivial_factory()
{
    Node<std::uint8_t> node{};
    TEST_ASSERT_EQUAL_PTR(&node, cavl2_trivial_factory(&node));
    TEST_ASSERT_EQUAL_PTR(NULL, cavl2_trivial_factory(nullptr));
}

void test_to_owner()
{
    struct foo
    {
        cavl2_t tree_a;
        int     a;
        cavl2_t tree_b;
        char    b;
        cavl2_t tree_c;
    };
    foo f{};
    TEST_ASSERT_EQUAL_PTR(&f, CAVL2_TO_OWNER(&f.tree_a, foo, tree_a));
    TEST_ASSERT_EQUAL_PTR(&f, CAVL2_TO_OWNER(&f.tree_b, foo, tree_b));
    TEST_ASSERT_EQUAL_PTR(&f, CAVL2_TO_OWNER(&f.tree_c, foo, tree_c));

    // Test that the macro doesn't evaluate the pointer expression twice.
    // If it did, call_count would be 2 instead of 1.
    int  call_count    = 0;
    auto get_tree_node = [&call_count, &f]() -> cavl2_t* {
        call_count++;
        return &f.tree_b;
    };
    TEST_ASSERT_EQUAL_PTR(&f, CAVL2_TO_OWNER(get_tree_node(), foo, tree_b));
    TEST_ASSERT_EQUAL_INT(1, call_count);

    // Test with NULL return
    call_count         = 0;
    auto get_null_node = [&call_count]() -> cavl2_t* {
        call_count++;
        return nullptr;
    };
    TEST_ASSERT_EQUAL_PTR(NULL, CAVL2_TO_OWNER(get_null_node(), foo, tree_b));
    TEST_ASSERT_EQUAL_INT(1, call_count);
}

} // namespace

int main(const int argc, const char* const argv[])
{
    const auto seed = static_cast<unsigned>((argc > 1) ? std::atoll(argv[1]) : std::time(nullptr)); // NOLINT
    std::printf("Randomness seed: %u\n", seed);
    std::srand(seed);
    // NOLINTBEGIN(misc-include-cleaner)
    UNITY_BEGIN();
    RUN_TEST(test_check_ascension);
    RUN_TEST(test_rotation);
    RUN_TEST(test_balancing_a);
    RUN_TEST(test_balancing_b);
    RUN_TEST(test_balancing_c);
    RUN_TEST(test_retracing_on_growth);
    RUN_TEST(test_find_trivial);
    RUN_TEST(test_removal_a);
    RUN_TEST(test_mutation_manual);
    RUN_TEST(test_mutation_randomized);
    RUN_TEST(test_traversal_full);
    RUN_TEST(test_trivial_factory);
    RUN_TEST(test_to_owner);
    return UNITY_END();
    // NOLINTEND(misc-include-cleaner)
}
