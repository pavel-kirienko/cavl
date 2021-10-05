/// Cavl is a generic C implementation of AVL tree suitable for deeply embedded systems distributed as a single header.
/// To integrate it into your project, simply copy the only header into your source tree.
///
/// Cavl is best used with O1Heap <https://github.com/pavel-kirienko/o1heap> -- a deterministic memory manager for
/// hard-real-time high-integrity embedded systems.
///
/// Copyright (c) 2021 Pavel Kirienko
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
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ----------------------------------------         PUBLIC API SECTION         ----------------------------------------

/// The tree node/root object. User types should put this item as the very first field or (in case of C++) inherit.
typedef struct Cavl Cavl;
struct Cavl
{
    Cavl*   parent;
    Cavl*   left;
    Cavl*   right;
    uint8_t height;  ///< Limits the capacity at 2**255 nodes (over 10**76, an astronomical number).
};

/// Returns positive if the search target is greater than the node value, negative if smaller, zero on match (found).
typedef int8_t CavlPredicate(void* user_reference, const Cavl* node);

/// If provided, the factory is invoked if the searched node could not be found.
/// It is expected to return a new node that will be inserted immediately without the need to traverse the tree again.
/// If the factory returns NULL or is not provided, the tree is not modified.
typedef Cavl* CavlFactory(void* user_reference);

/// See cavlTraverse().
typedef void CavlVisitor(void* user_reference, Cavl* node);

/// Look for a node in the tree using the specified search predicate. Worst-case complexity is O(log n).
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

/// Remove the specified node from its tree in constant time. No search is necessary.
static inline void cavlRemove(Cavl* const node);

/// Calls the visitor with each node in the specified order: ascending (in-order) or descending (reverse in-order).
/// The node pointer passed to the visitor is guaranteed to be valid.
/// The user_reference is passed into the visitor unmodified.
/// No action is taken if root or visitor are NULL.
static inline void cavlTraverse(Cavl* const        root,
                                const bool         ascending,
                                void* const        user_reference,
                                CavlVisitor* const visitor)
{
    if ((root != NULL) && (visitor != NULL))
    {
        cavlTraverse(ascending ? root->left : root->right, ascending, user_reference, visitor);
        visitor(user_reference, root);
        cavlTraverse(ascending ? root->right : root->left, ascending, user_reference, visitor);
    }
}

// ----------------------------------------      POLICE LINE DO NOT CROSS      ----------------------------------------

static inline uint8_t _cavlGetHeight(const Cavl* const nd)
{
    return (nd != NULL) ? nd->height : 0;
}

static inline void _cavlAdjustHeight(Cavl* const nd)
{
    const uint8_t lf = _cavlGetHeight(nd->left);
    const uint8_t rt = _cavlGetHeight(nd->right);
    nd->height       = (uint8_t) (1U + ((lf > rt) ? lf : rt));
}

static inline int8_t _cavlGetBalance(const Cavl* const nd)
{
    int16_t bal = 0;
    if (nd != NULL)
    {
        bal = (int16_t) ((int16_t) (_cavlGetHeight(nd->right)) - (int16_t) (_cavlGetHeight(nd->left)));
        assert((bal >= -2) && (bal <= 2));
    }
    return (int8_t) bal;
}

static inline Cavl* _cavlRotateRight(Cavl* const nd)
{
    assert((nd != NULL) && (nd->left != NULL));
    Cavl* const new_nd = nd->left;
    if (nd->parent != NULL)
    {
        if (nd->parent->left == nd)
        {
            nd->parent->left = new_nd;
        }
        else
        {
            assert(nd->parent->right == nd);
            nd->parent->right = new_nd;
        }
    }
    new_nd->parent = nd->parent;
    nd->parent     = new_nd;
    nd->left       = new_nd->right;
    if (nd->left != NULL)
    {
        nd->left->parent = nd;
    }
    new_nd->right = nd;
    _cavlAdjustHeight(nd);
    _cavlAdjustHeight(new_nd);
    return new_nd;
}

static inline Cavl* _cavlRotateLeft(Cavl* const nd)
{
    assert((nd != NULL) && (nd->right != NULL));
    Cavl* const new_nd = nd->right;
    if (nd->parent != NULL)
    {
        if (nd->parent->right == nd)
        {
            nd->parent->right = new_nd;
        }
        else
        {
            assert(nd->parent->left == nd);
            nd->parent->left = new_nd;
        }
    }
    new_nd->parent = nd->parent;
    nd->parent     = new_nd;
    nd->right      = new_nd->left;
    if (nd->right != NULL)
    {
        nd->right->parent = nd;
    }
    new_nd->left = nd;
    _cavlAdjustHeight(nd);
    _cavlAdjustHeight(new_nd);
    return new_nd;
}

static inline Cavl* _cavlBalance(Cavl* const nd)
{
    Cavl* out = nd;
    if (_cavlGetBalance(nd) < -1)
    {
        if (_cavlGetBalance(nd->left) < 0)
        {
            out = _cavlRotateRight(nd);
        }
        else
        {
            (void) _cavlRotateLeft(nd->left);
            out = _cavlRotateRight(nd);
        }
    }
    else if (_cavlGetBalance(nd) > 1)
    {
        if (_cavlGetBalance(nd->right) > 0)
        {
            out = _cavlRotateLeft(nd);
        }
        else
        {
            (void) _cavlRotateRight(nd->right);
            out = _cavlRotateLeft(nd);
        }
    }
    else
    {
        (void) 0;  // Already balanced or empty.
    }
    return out;
}

#ifdef __cplusplus
}
#endif
