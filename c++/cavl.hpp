/// Source: https://github.com/pavel-kirienko/cavl
///
/// Cavl is a single-header C library providing an implementation of AVL tree suitable for deeply embedded systems.
/// To integrate it into your project, simply copy this file into your source tree. Read the API docs below.
///
/// See also O1Heap <https://github.com/pavel-kirienko/o1heap> -- a deterministic memory manager for hard-real-time
/// high-integrity embedded systems.
///
/// Copyright (c) 2021 Pavel Kirienko <pavel@uavcan.org>
///
/// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
/// documentation files (the "Software"), to deal in the Software without restriction, including without limitation
/// the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
/// and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
///
/// The above copyright notice and this permission notice shall be included in all copies or substantial portions of
/// the Software.
///
/// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
/// WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS
/// OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
/// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#pragma once

#include <cassert>
#include <cstdint>

namespace cavl
{
template <typename T>
class Node
{
public:
    using Self = T;

    // Tree nodes cannot be copied, but they can be moved.
    Node(const Node&) = delete;
    auto operator=(const Node&) -> Node& = delete;

    template <typename Predicate, typename Factory>
    static auto search(T*& root, const Predicate& predicate, const Factory& factory) -> T*
    {
        //
    }

    template <typename Predicate>
    static auto search(T*& root, const Predicate& predicate) noexcept -> T*
    {
        return search<T, Predicate>(root, predicate, []() -> T* { return nullptr; });
    }

    template <typename Predicate>
    static auto search(const T* const root, const Predicate& predicate) noexcept -> const T*
    {
        const T* out = nullptr;
        const T* n   = root;
        while (n != nullptr)
        {
            const auto cmp = predicate(*n);
            if (cmp == 0)
            {
                out = n;
                break;
            }
            n = n->cavl_lr_[cmp > 0];
        }
        return out;
    }

    static auto remove(T*& root, const T* const node) noexcept
    {
        //
    }
    static auto remove(T*& root, T* const node) noexcept
    {
        remove(node);
        node->cavl_ = Cavl{};
    }

    static auto findExtremum(T* const root, const bool maximum) noexcept -> T*
    {
        T* result = nullptr;
        T* c      = root;
        while (c != nullptr)
        {
            result = c;
            c      = c->cavl_lr_[maximum];
        }
        return result;
    }
    static auto findExtremum(const T* const root, const bool maximum) noexcept -> const T*
    {
        const T* result = nullptr;
        const T* c      = root;
        while (c != nullptr)
        {
            result = c;
            c      = c->cavl_lr_[maximum];
        }
        return result;
    }

protected:
    Node()  = default;
    ~Node() = default;

    Node(Node&& other) noexcept
    {
        cavl_.up    = other.cavl_.up;
        cavl_.lr[0] = other.cavl_.lr[0];
        cavl_.lr[1] = other.cavl_.lr[1];
        cavl_.bf    = other.cavl_.bf;
        other.cavl_ = {};
        // TODO relink
    }

    auto operator=(Node&& other) noexcept -> Node&
    {
        (void) other;
        // TODO
        return *this;
    }

private:
    void rotate(const bool r) noexcept
    {
        //
    }

    auto adjustBalance(const bool increment) noexcept -> T*
    {
        //
    }

    auto retraceOnGrowth() noexcept -> T*
    {
        //
    }

    struct Cavl final
    {
        T*          up = nullptr;
        T*          lr[2]{};
        std::int8_t bf = 0;
    } cavl_;
};

}  // namespace cavl
