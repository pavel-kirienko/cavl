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
typedef int8_t (*CavlPredicate)(void* user_reference, const Cavl* node);

/// If provided, the factory is invoked if the searched node could not be found.
/// It is expected to return a new node that will be inserted immediately without the need to traverse the tree again.
/// If the factory returns NULL or is not provided, the tree is not modified.
typedef Cavl* (*CavlFactory)(void* user_reference);

/// Look for a node in the tree using the specified search predicate. Average/worst-case complexity is O(log n).
/// - If the node is found, return it.
/// - If the node is not found and the factory is NULL, return NULL.
/// - If the node is not found and the factory is not NULL, construct a new node using the factory, insert & return it;
///   if the factory returned NULL, behave as if factory was NULL.
/// The user_reference is passed into the functions unmodified.
/// If predicate is NULL, returns NULL.
static inline Cavl* cavlSearch(Cavl** const        root,
                               void* const         user_reference,
                               const CavlPredicate predicate,
                               const CavlFactory   factory);

#if 0
/// Remove the specified node from its tree in constant time. No search is necessary. The children will survive.
/// No effect if either of the pointers are NULL.
static inline void cavlRemove(Cavl** const root, Cavl* const node);
#endif

// ----------------------------------------     END OF PUBLIC API SECTION      ----------------------------------------
// ----------------------------------------      POLICE LINE DO NOT CROSS      ----------------------------------------

/// INTERNAL USE ONLY. Makes '!r' child of 'x' its parent; i.e., rotates toward 'r'.
static inline Cavl* _cavlRotate(Cavl* const x, const bool r)
{
    assert((x != NULL) && (x->lr[!r] != NULL));
    Cavl* const z = x->lr[!r];
    if (x->up != NULL)
    {
        x->up->lr[x->up->lr[1] == x] = z;
    }
    z->up     = x->up;
    x->up     = z;
    x->lr[!r] = z->lr[r];
    if (x->lr[!r] != NULL)
    {
        x->lr[!r]->up = x;
    }
    z->lr[r] = x;
    return z;
}

/// INTERNAL USE ONLY. Returns the new node to replace the old one if balancing took place, same node otherwise.
static inline Cavl* _cavlBalance(Cavl* const x)
{
    Cavl* out = x;
    if ((x != NULL) && ((x->bf < -1) || (x->bf > 1)))  // The AVL invariant is bf in {-1, 0, +1}.
    {
        const bool   r    = x->bf < 0;    // bf<0 if left-heavy --> right rotation is needed.
        const int8_t sign = r ? +1 : -1;  // Positive if we are rotating right.
        Cavl* const  z    = x->lr[!r];
        assert(z != NULL);        // Heavy side cannot be empty.
        if ((z->bf * sign) <= 0)  // Parent and child are heavy on the same side or the child is balanced.
        {
            out = _cavlRotate(x, r);
            if (z->bf == 0)
            {
                x->bf = (int8_t) (-sign);
                z->bf = (int8_t) (+sign);
            }
            else
            {
                x->bf = 0;
                z->bf = 0;
            }
        }
        else  // Otherwise, the child needs to be rotated in the opposite direction first.
        {
            Cavl* const y = z->lr[r];
            assert(y != NULL);  // Heavy side cannot be empty.
            (void) _cavlRotate(z, !r);
            out = _cavlRotate(x, r);
            if ((y->bf * sign) < 0)
            {
                x->bf = (int8_t) (+sign);
                y->bf = 0;
                z->bf = 0;
            }
            else if ((y->bf * sign) > 0)
            {
                x->bf = 0;
                y->bf = 0;
                z->bf = (int8_t) (-sign);
            }
            else
            {
                x->bf = 0;
                z->bf = 0;
            }
        }
    }
    return out;
}

/// INTERNAL USE ONLY.
/// Takes the culprit node (the one that is added/removed); returns NULL or the root of the tree (possibly new one).
/// When adding a new node, set its balance factor to zero and call this function to propagate the changes upward.
static inline Cavl* _cavlRetrace(Cavl* const start, const int8_t growth)
{
    assert((start != NULL) && ((growth == -1) || (growth == +1)));
    Cavl* c = start;      // Child
    Cavl* p = start->up;  // Parent
    while (p != NULL)
    {
        const bool r = p->lr[1] == c;  // c is the right child of parent
        assert(p->lr[r] == c);
        p->bf = (int8_t) (p->bf + (r ? +growth : -growth));
        p     = _cavlBalance(p);
        c     = p;
        p     = c->up;
        if (c->bf == 0)
        {           // The height change of the subtree made this parent perfectly balanced (as all things should be),
            break;  // hence, the height of the outer subtree is unchanged, so upper balance factors are unchanged.
        }
    }
    assert(c != NULL);
    return (p == NULL) ? c : NULL;  // New root or nothing.
}

static inline Cavl* cavlSearch(Cavl** const        root,
                               void* const         user_reference,
                               const CavlPredicate predicate,
                               const CavlFactory   factory)
{
    Cavl* out = NULL;
    if ((root != NULL) && (predicate != NULL))
    {
        Cavl*  up = *root;
        Cavl** n  = root;
        while (*n != NULL)
        {
            const int8_t cmp = predicate(user_reference, *n);
            if (cmp == 0)
            {
                out = *n;
                break;
            }
            up = *n;
            n  = &(*n)->lr[cmp > 0];
            assert((*n == NULL) || ((*n)->up == up));
        }
        if (out == NULL)
        {
            out = (factory == NULL) ? NULL : factory(user_reference);
            if (out != NULL)
            {
                *n             = out;  // Overwrite the pointer to the new node in the parent node.
                out->lr[0]     = NULL;
                out->lr[1]     = NULL;
                out->up        = up;
                out->bf        = 0;
                Cavl* const rt = _cavlRetrace(out, +1);
                if (rt != NULL)
                {
                    *root = rt;
                }
            }
        }
    }
    return out;
}

#ifdef __cplusplus
}
#endif
