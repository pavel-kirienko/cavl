/// Source: https://github.com/pavel-kirienko/cavl
/// Cavl is a single-header C library providing an implementation of AVL tree suitable for deeply embedded systems.
/// To integrate it into your project, simply copy this file into your source tree.
///
/// Cavl is best used with O1Heap <https://github.com/pavel-kirienko/o1heap> -- a deterministic memory manager for
/// hard-real-time high-integrity embedded systems.
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

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ----------------------------------------         PUBLIC API SECTION         ----------------------------------------

/// The tree node/root. The user data is represented by a single untyped pointer.
/// Per standard convention, nodes that compare smaLLer are put on the Left; those that are laRgeR are on the Right.
typedef struct Cavl Cavl;
struct Cavl
{
    void*  value;  ///< The user data stored in this node.
    Cavl*  up;     ///< Parent node, NULL in the root.
    Cavl*  lr[2];  ///< Left child (lesser), right child (greater).
    int8_t bf;     ///< Balance factor is positive when right-heavy.
};

/// Returns positive if the search target is greater than the node value, negative if smaller, zero on match (found).
typedef int8_t CavlPredicate(void* user_reference, const Cavl* node);

/// If provided, the factory is invoked if the searched node could not be found.
/// It is expected to return a new node that will be inserted immediately without the need to traverse the tree again.
/// If the factory returns NULL or is not provided, the tree is not modified.
typedef Cavl* CavlFactory(void* user_reference);

/// Look for a node in the tree using the specified search predicate. Average/worst-case complexity is O(log n).
/// - If the node is found, return it.
/// - If the node is not found and the factory is NULL, return NULL.
/// - If the node is not found and the factory is not NULL, construct a new node using the factory, insert & return it;
///   if the factory returned NULL, behave as if factory was NULL.
/// The user_reference is passed into the functions unmodified.
/// If predicate is NULL, returns NULL.
static inline Cavl* cavlSearch(Cavl** const         root,
                               void* const          user_reference,
                               CavlPredicate* const predicate,
                               CavlFactory* const   factory);

#if 0
/// Remove the specified node from its tree in constant time. No search is necessary. The children will survive.
/// No effect if either of the pointers are NULL.
static inline void cavlRemove(Cavl** const root, Cavl* const node);
#endif
// ----------------------------------------      POLICE LINE DO NOT CROSS      ----------------------------------------

static inline Cavl* _cavlRotate(Cavl* const n, const bool r)
{
    assert((n != NULL) && (n->lr[!r] != NULL));
    Cavl* const x = n->lr[!r];
    if (n->up != NULL)
    {
        if (n->up->lr[!r] == n)
        {
            n->up->lr[!r] = x;
        }
        else
        {
            assert(n->up->lr[r] == n);
            n->up->lr[r] = x;
        }
    }
    x->up     = n->up;
    n->up     = x;
    n->lr[!r] = x->lr[r];
    if (n->lr[!r] != NULL)
    {
        n->lr[!r]->up = n;
    }
    x->lr[r]           = n;
    const int8_t delta = r ? +1 : -1;
    n->bf              = (int8_t) (n->bf + delta);
    x->bf              = (int8_t) (x->bf + delta);
    return x;
}

static inline Cavl* _cavlBalance(Cavl* const n)
{
    Cavl* out = n;
    if ((n->bf < -1) || (n->bf > 1))  // The AVL invariant is bf in {-1, 0, +1}.
    {
        const bool right = n->bf < 0;                // bf<0 if left-heavy --> right rotation is needed.
        assert(n->lr[!right] != NULL);               // Heavy side cannot be empty.
        if ((n->lr[!right]->bf > 0) == (n->bf > 0))  // Parent and child are heavy on the same side.
        {
            out = _cavlRotate(n, right);
        }
        else  // Otherwise, the child needs to be rotated in the opposite direction first.
        {
            (void) _cavlRotate(n->lr[!right], !right);
            out = _cavlRotate(n, right);
        }
        n->bf = (int8_t) (n->bf + (right ? +1 : -1));
    }
    return out;
}

static inline Cavl* cavlSearch(Cavl** const         root,
                               void* const          user_reference,
                               CavlPredicate* const predicate,
                               CavlFactory* const   factory)
{
    Cavl* out = NULL;
    if ((root != NULL) && (predicate != NULL))
    {
        Cavl*  up = *root;
        Cavl** n  = root;
        bool   r  = false;  // Left/right side selector.
        while (*n != NULL)
        {
            const int8_t cmp = predicate(user_reference, *n);
            if (cmp == 0)
            {
                out = *n;
                break;
            }
            r  = cmp > 0;
            up = *n;
            n  = &(*n)->lr[r];
            assert((*n == NULL) || ((*n)->up == up));
        }
        if (out == NULL)
        {
            out = (factory == NULL) ? NULL : factory(user_reference);
            if (out != NULL)
            {
                *n         = out;  // Overwrite the pointer to the new node in the parent node.
                out->lr[0] = NULL;
                out->lr[1] = NULL;
                out->up    = up;
                out->bf    = 0;
                if (up != NULL)
                {
                    assert(up->lr[r] == out);
                    up->bf = (int8_t) (up->bf + (r ? +1 : -1));  // New balance of the parent node.
                    assert((up->bf == 0) || (up->bf == -2) || (up->bf == +2));
                }
                while ((up != NULL) && (up->bf != 0))
                {
                    const bool is_root = up == *root;
                    up                 = _cavlBalance(up);
                    if (is_root)
                    {
                        assert(up->up == NULL);  // Ensure the new root is indeed root.
                        *root = up;
                    }
                    up = up->up;
                }
            }
        }
    }
    return out;
}

#ifdef __cplusplus
}
#endif
