/*********************************************************************
 *
 * AUTHORIZATION TO USE AND DISTRIBUTE
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: 
 *
 * (1) source code distributions retain this paragraph in its entirety, 
 *  
 * (2) distributions including binary code include this paragraph in
 *     its entirety in the documentation or other materials provided 
 *     with the distribution, and 
 *
 * (3) all advertising materials mentioning features or use of this 
 *     software display the following acknowledgment:
 * 
 *      "This product includes software written and developed 
 *       by Brian Adamson of the Naval Research Laboratory (NRL)." 
 *         
 *  The name of NRL, the name(s) of NRL  employee(s), or any entity
 *  of the United States Government may not be used to endorse or
 *  promote  products derived from this software, nor does the 
 *  inclusion of the NRL written and developed software  directly or
 *  indirectly suggest NRL or United States  Government endorsement
 *  of this product.
 * 
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 ********************************************************************/
 
#include "protoTree.h"
#include "protoDebug.h"  // for DMSG()

#include <string.h>
#include <stdlib.h>  // for labs()

ProtoTree::Item::Item(const char* theKey, unsigned int theKeysize, void* theValue)
 : key(theKey), keysize(theKeysize), value(theValue), bit(0),
   parent((Item*)NULL), left((Item*)NULL), right((Item*)NULL)
{
       
}

ProtoTree::Item::~Item()
{
    
}

unsigned int ProtoTree::Item::GetDepth() const 
{
    unsigned int depth = 0;
    const Item* p = this;
    while ((p = p->parent)) depth++;
    return depth;
}  // end ProtoTree::Item::GetDepth()

ProtoTree::Item::Pool::Pool()
 : head(NULL)
{
}

ProtoTree::Item::Pool::~Pool()
{
    Destroy();
}

void ProtoTree::Item::Pool::Destroy()
{
    Item* item;
    while ((item = Get()))
        delete item;
}  // end ProtoTree::Item::Pool::Destroy()

ProtoTree::ProtoTree()
 : keysize_min(0), keysize_max(0), roots((Item**)NULL)
{
}

ProtoTree::~ProtoTree()
{
    Destroy();
}

bool ProtoTree::Init(unsigned int keysizeMin, unsigned int keysizeMax)
{
    Destroy();
    if (keysizeMin < 1) keysizeMin = 1;
    if (keysizeMax < 1) keysizeMax = 1;
    unsigned int numRoots = keysizeMax - keysizeMin + 1;
    if (!(roots = new Item*[numRoots]))
    {
        DMSG(0, "ProtoTree:Init() error allocating roots: %s\n", GetErrorString());
        return false;   
    }
    memset(roots, 0, numRoots*sizeof(Item**));
    keysize_min = keysizeMin;
    keysize_max = keysizeMax;
    return true;
}  // end ProtoTree::Init()

bool ProtoTree::IsEmpty() const
{
    unsigned int numRoots = keysize_max - keysize_min + 1;
    for (unsigned int i = 0 ; i < numRoots; i++)
    {
        if (NULL != roots[i]) return false;
    }
    return true;
}  // end ProtoTree::IsEmpty()

void ProtoTree::Destroy()
{
    if (roots)
    {
        Empty();
        delete[] roots;
        roots = (Item**)NULL;
        keysize_min = keysize_max = 0;
    }
}  // end ProtoTree:Destroy()


void ProtoTree::Empty()
{
    if (roots)
    {
        for (unsigned int i = keysize_min; i <= keysize_max; i++)
        {
            Item* next;
            while (NULL != (next = roots[i - keysize_min]))
            {
                Remove(*next);
                delete next;   
            }
        }
    }
}  // end ProtoTree::Empty()

ProtoTree::Item* ProtoTree::GetRoot(unsigned int keysize) const
{
    if (roots)
    {
        if (0 == keysize)
        {
            for (unsigned int i = keysize_min; i <= keysize_max; i++)
            {
                Item* nextRoot = roots[i - keysize_min];
                if (NULL != nextRoot) return nextRoot;
            }
        }
        else
        {
            if ((keysize >= keysize_min) && (keysize <= keysize_max))
                return (roots[keysize - keysize_min]);
            else
                return NULL;
        }
    }
    return NULL;
}  // end ProtoTree::GetRoot()

ProtoTree::Item* ProtoTree::GetFirstItem(unsigned int keysize) const
{
    if (roots)
    {
        // 1) Find the non-NULL root of shortest keysize
        //    (unless a specific (non-zero) "keysize" is given
        Item* root = NULL;
        if (0 == keysize)
        {
            for (unsigned int i = keysize_min; i <= keysize_max; i++)
            {
                root = roots[i - keysize_min];
                if (NULL != root) break;
            }
        }
        else
        {
            if ((keysize >= keysize_min) && (keysize <= keysize_max))
            {
                root = roots[keysize - keysize_min];
            }
        }
        if (NULL != root)
        {
            if (root->left == root->right)
            {
                // 2-A) Only one node in this tree
                return root;
            }            
            else
            {
                // 2-B) Return left most node in this tree
                Item* x = (root->left == root) ? root->right : root;   
                while (x->left->parent == x) x = x->left;
                return (x->left);
            }
        }
    }
    return NULL;
}  // end ProtoTree::GetFirstItem()

ProtoTree::Item* ProtoTree::GetLastItem(unsigned int keysize) const
{
    if (roots)
    {
        // 1) Find the non-NULL root of shortest keysize
        //    (unless a specific (non-zero) "keysize" is given
        Item* root = NULL;
        if (0 == keysize)
        {
            for (unsigned int i = keysize_max; i >= keysize_min; i--)
            {
                root = roots[i - keysize_min];
                if (NULL != root) break;
            }
        }
        else
        {
            if ((keysize >= keysize_min) && (keysize <= keysize_max))
            {
                root = roots[keysize - keysize_min];
            }
        }
        if (NULL != root)
        {
            // 2) Follow root or left of root all the way to right to find
            //    the last item (lexically) in the tree
            Item* x = (root->right == root) ? root->left : root;
            Item* p;
            do
            {
                p = x;
                x = x->right;
            } while (p == x->parent);
            return x;
        }
    }
    return NULL;
}  // end ProtoTree::GetLastItem()

typedef signed char ItemType;
#define VALUE(X) ( X ? (*((ItemType*)X->GetValue())) : 0)

#define KVALUE(X) (X ? *((ItemType*)X->GetKey()) : 0)


bool ProtoTree::Insert(ProtoTree::Item& item)
        
{
    if (!roots)
    {
        DMSG(0, "ProtoTree::Insert() error: uninitialized tree\n");
        return false;  
    }
    unsigned int keysize = item.keysize;
    Item* subroot = roots[keysize - keysize_min];
    if (subroot)
    {
        // 1) Find closest match to "item"
        const char* key = item.key;
        Item* x = subroot;
        Item* p;
        do
        {
            p = x;
            x = Bit(key, x->bit) ? x->right : x->left;
        } while (p == x->parent);
        
        // 2) Find index of first differing bit ("dBit")
        //    (also look out for exact match!)
        unsigned int dBit = 0;
        // A) Do byte-wise comparison as possible
        const char* ptr1 = key;
        const char* ptr2 = x->key;
        while ((dBit < keysize) && (*ptr1 == *ptr2))
        {
            ptr1++;
            ptr2++; 
            dBit += 8;  
        }   
        if (dBit < keysize)
        {
            // B) Do bit-wise comparison as needed
            unsigned char delta = *ptr1 ^ *ptr2;
            ASSERT(0 != delta);
            while ((dBit < keysize) && (delta < 0x80))
            {
                delta <<= 1;
                dBit++;
            }
        } 
        if (dBit >= keysize)
        {
            DMSG(0, "ProtoTree::Insert() Equivalent item already in tree!\n");
            return false;
        }
        item.bit = dBit;

        // 3) Find "item" insertion point
        x = subroot;
        do
        {
            p = x;
            x = Bit(key, x->bit) ? x->right : x->left;
        } while ((x->bit < dBit) && (p == x->parent)); 


        // 4) Insert "item" into tree
        if (Bit(key, dBit))
        {
            ASSERT(x);
            item.left = x;
            item.right = &item;
        }
        else
        {
            item.left = &item;
            item.right = x;                 
        }
        item.parent = p;
        if (Bit(key, p->bit))
            p->right = &item;
        else 
            p->left = &item;
        if (p == x->parent) 
            x->parent = &item;
        return true; 
    }
    else
    {
        // Subtree is empty, so make "item" the subtree root
        roots[keysize - keysize_min] = &item;
        item.parent = (Item*)NULL;
        item.left = item.right = &item;
        item.bit = 0;
        return true;
    }
}  // end ProtoTree::Insert()

// Find node with backpointer to "item"
ProtoTree::Item* ProtoTree::FindPredecessor(ProtoTree::Item& item) const
{
    // Find terminal "q"  with backpointer to "item"   
    Item* x = &item;
    Item* q;
    do
    {
        q = x;
        if (Bit(item.key, x->bit))
            x = x->right;
        else
            x = x->left;
    } while (x != &item);
    return q;
}  // end ProtoTree::FindPredecessor()

void ProtoTree::Remove(ProtoTree::Item& item)
{
    if (((&item == item.left) || (&item == item.right)) && (NULL != item.parent))
    {
        // non-root "item" that has at least one self-pointer  
        // (a.k.a an "external entry"?)
        Item* orphan = (&item == item.left) ? item.right : item.left;
        if (item.parent->left == &item)
            item.parent->left = orphan;
        else 
            item.parent->right = orphan;
        if (orphan->bit > item.parent->bit)
            orphan->parent = item.parent;
    }
    else
    {
        // Root or "item" with no self-pointers 
        // (a.k.a an "internal entry"?)
        // 1) Find terminal "q"  with backpointer to "item"  
        Item* x = &item;
        Item* q;
        q = x;
        do                      
        {                       
            q = x;              
            if (Bit(item.key, x->bit))
                x = x->right;
            else                
                x = x->left;    
        } while (x != &item);
        
        if (NULL != q->parent)
        {
            // Non-root "q", so "q" is moved into the place of "item"
            Item* s = NULL;
            if (NULL == item.parent)
            {
                // There are always two nodes backpointing to "root"
                // (unless root is the only node in the tree)
                // "s" is the other of these besides "q"
                // (We need to find "s" before we mess with the tree)
                x = Bit(item.key, item.bit) ? item.left : item.right;
                do
                {
                    s = x;
                    if (Bit(item.key, x->bit))
                        x = x->right;
                    else
                        x = x->left;
                } while (x != &item);
            }
            
            // A) Set bit index of "q" to that of "item"
            q->bit = item.bit;
            
            // B) Fix the parent, left, and right node pointers to "q"
            //    (removes "q" from its current place in the tree)
            Item* parent = q->parent;
            Item* child = (q->left == &item) ? q->right : q->left;
            ASSERT(NULL != child);
            if (parent->left == q)
                parent->left = child;
            else
                parent->right = child;
            if (child->bit > parent->bit)
                child->parent = parent;
            
            // C) Fix the item's left->parent and right->parent node pointers to "item"
            //    (places "q" into the current place of the "item" in the tree)
            ASSERT(q != NULL);
            if (item.left->parent == &item)
                item.left->parent = q;
            if (item.right->parent == &item)
                item.right->parent = q;
            if (NULL != item.parent)
            {
                if (item.parent->left == &item)
                    item.parent->left = q;
                else 
                    item.parent->right = q;
            }
            else
            {
                // "item" was root node, so update the "s" node 
                //  backpointer to "q" instead of "item"
                ASSERT(s != NULL);
                ASSERT(s != &item);
                if (s->left == &item)
                    s->left = q;
                else
                    s->right = q;
                roots[item.keysize - keysize_min] = q;
            }
              
            // E) Finally, "q" gets the pointers of the "item" being removed
            //    (which now _may_ include a pointer to itself)
            if (NULL != item.parent)
                ASSERT((&item != item.left) && (&item != item.right));
            q->parent = item.parent;
            q->left = (item.left == &item) ? q : item.left;
            q->right = (item.right == &item) ? q : item.right;
        }
        else
        {
            ASSERT(q == &item);
            Item* orphan = (q == q->left) ? q->right : q->left;
            if (q == orphan)
            {
                roots[item.keysize - keysize_min] = (Item*)NULL; 
            }
            else
            {
                roots[item.keysize - keysize_min] = orphan; 
                orphan->parent = NULL;
                if (orphan->left == q) 
                    orphan->left = orphan;
                else 
                    orphan->right = orphan;
                orphan->bit = 0;
            }
        }
    }   
    item.parent = item.left = item.right = (Item*)NULL;
}  // end ProtoTree::Remove()


// Find item with exact match to key and keysize
ProtoTree::Item* ProtoTree::Find(const char*  key, 
                                 unsigned int keysize) const
{
    ASSERT(keysize >= keysize_min);
    ASSERT(keysize <= keysize_max);
    Item* x = roots[keysize - keysize_min];
    if (x)
    {
        Item* p;
        do 
        { 
            p = x;
            x = Bit(key, x->bit) ? x->right : x->left;   
        } while (x->parent == p);
        return (x->IsEqual(key, keysize) ? x : NULL);   
    }    
    else
    {
        return (Item*)NULL;
    }
}  // end ProtoTree::Find()

// Find item with "closest" match to key and keysize (biggest prefix match?)
ProtoTree::Item* ProtoTree::FindClosestMatch(const char*  key, 
                                             unsigned int keysize) const
{
    ASSERT(keysize >= keysize_min);
    ASSERT(keysize <= keysize_max);
    Item* x = roots[keysize - keysize_min];
    if (x)
    {
        Item* p;
        do 
        { 
            p = x;
            x = Bit(key, x->bit) ? x->right : x->left;   
        } while (x->parent == p);
        return x;   
    }    
    else
    {
        return (Item*)NULL;
    }
}  // end ProtoTree::FindClosestMatch()

// Finds longest matching entry that is a prefix to "key"
ProtoTree::Item* ProtoTree::FindPrefix(const char*  key, 
                                       unsigned int keysize) const
{
    ASSERT(keysize >= keysize_min);
    ASSERT(keysize <= keysize_max);
    Item* x = NULL;
    while (keysize >= keysize_min)
    {
        x = roots[keysize - keysize_min];
        if (x)
        {
            Item* p;
            do 
            { 
                p = x;
                x = Bit(key, x->bit) ? x->right : x->left; 
            } while (x->parent == p);
            if (x->IsEqual(key, keysize)) return x;
        }
        keysize--; 
    }
    return NULL;
}  // end ProtoTree::FindPrefix()

ProtoTree::Item* ProtoTree::FindPrefixSubtree(const char*  prefix, 
                                              unsigned int prefixSize,
                                              unsigned int keysize,
                                              bool         minMatch) const
{
    unsigned int keysizeEnd;
    if (0 == keysize)
    {
        if (prefixSize > keysize_max) return NULL;
        if (minMatch)
        {
            // We're trying to find the shortest key that matches prefix
            keysize = (prefixSize > keysize_min) ? prefixSize : keysize_min;
            keysizeEnd = keysize_max + 1;
        }
        else
        {
            // We're trying to find the longest key that matches prefix
            keysize = keysize_max;
            keysizeEnd = keysize_min - 1;
        }
    }
    else
    {
        if ((prefixSize > keysize) ||
            (keysize < keysize_min) || 
            (keysize > keysize_max))
        {
            return NULL;
        }
        if (minMatch)
            keysizeEnd = keysize + 1;
        else
            keysizeEnd = keysize - 1;
    }
    while (keysize != keysizeEnd)
    {
        Item* x = roots[keysize - keysize_min];
        if (x)
        {
            Item* p;
            do 
            { 
                p = x;
                x = Bit(prefix, x->bit) ? x->right : x->left;  
            } while ((x->parent == p) && (x->bit < prefixSize));
            if (x->PrefixIsEqual(prefix, prefixSize)) return x;
        }
        if (minMatch)
            keysize++;
        else
            keysize--;
    }
    return (Item*)NULL;
}  // end ProtoTree::FindPrefixSubtree()
                

ProtoTree::Iterator::Iterator(const ProtoTree& theTree, bool reverse, ProtoTree::Item* cursor)
 : tree(theTree), keysize_set(0), prefix_size(0), prefix_item(NULL)
{
    if (NULL != cursor)
    {
        reversed = reverse;
        SetCursor(*cursor);
    }
    else
    {
        Reset(reverse);  // Reset() sets all to defaults
    }
}  

ProtoTree::Iterator::~Iterator()
{
}

void ProtoTree::Iterator::Reset(bool                reverse,
                                unsigned int        theKeysize,
                                const char*         prefix,
                                unsigned int        prefixSize)
{
    prefix_size = prefixSize;
    prefix_item = prev = next = curr_hop = (Item*)NULL;
    if ((0 == theKeysize) || ((theKeysize >= tree.keysize_min) && (theKeysize <= tree.keysize_max)))
        keysize_set = theKeysize;
    else
        return;
    if (!tree.roots) return;
    
    if (0 != prefixSize)
    {
        if (NULL == prefix) return;
        // Find shortest item with matching prefix.
        ProtoTree::Item* prefixItem = tree.FindPrefixSubtree(prefix, prefixSize, theKeysize, ~reverse);
        if (NULL == prefixItem) return;
        // Temporarily "Reset()" the iterator and "SetCursor()" to find
        //Reset(~reverse, theKeysize);
        reversed = ~reverse;
        keysize_set = theKeysize;
        SetCursor(*prefixItem);
        if (reverse)
        {
            // Find the maximum value with matching prefix.
            ProtoTree::Item* lastItem;
            while (NULL != (lastItem = GetNextItem()))
            {   
                if (!lastItem->PrefixIsEqual(prefix, prefixSize))
                    break;  // The "cursor" is set to after the last matching item
            }
            if (NULL == lastItem) Reset(reverse, theKeysize);
        }
        else
        {
            // Find the minimum value with matching prefix.
            ProtoTree::Item* firstItem;
            while (NULL != (firstItem = GetPrevItem()))
            {   
                if (!firstItem->PrefixIsEqual(prefix, prefixSize))
                    break;  // The "cursor" is set to before the first matching item
            }
            if (NULL == firstItem) Reset(reverse, theKeysize);
        }
        prefix_size = prefixSize;
        prefix_item = prefixItem;
        return;
    }
    
    if (reverse)
    {
        // This code is basically the same as ProtoTree::GetLastItem()
        unsigned int size = (0 != theKeysize) ? theKeysize : tree.keysize_max;
        unsigned int sizeMin = (0 != theKeysize) ? theKeysize : tree.keysize_min;
        while (size >= sizeMin)
        {
              Item* root = tree.roots[size - tree.keysize_min];
              if (NULL != root)
              {
                  // Follow left of root all the way to right to find
                  // the very last item (lexically) in the tree
                  Item* x = (root->right == root) ? root->left : root;
                  Item* p;
                  do
                  {
                       p = x;
                       x = x->right;
                  } while (p == x->parent);
                  prev = x;
                  break;
              }
              else
              {
                  size--;
              }
        }
        reversed = true;
    }    
    else
    {
        // This code is basically the same as ProtoTree::GetFirstItem()
        // (although the code to find the "curr_hop" for iteration is different)
        unsigned int size = (0 != theKeysize) ? theKeysize : tree.keysize_min;
        unsigned int sizeMax = (0 != theKeysize) ? theKeysize : tree.keysize_max;
        while (size <= sizeMax)
        {
            Item* root = tree.roots[size - tree.keysize_min];
            if (NULL != root)
            {
                if (root->left == root->right)
                {
                    // Only one entry in the tree
                    next = root;
                    curr_hop = NULL;
                }
                else
                {
                    // If root has a left side, go as far left as possible
                    // to find the very first item (lexically) in the tree
                    Item* x = (root->left == root) ? root->right : root;   
                    while (x->left->parent == x) x = x->left;
                    next = x->left;
                    if (x->right->parent == x)
                    {
                        // Branch right and go as far left as possible
                        x = x->right;
                        while (x->left->parent == x) x = x->left;
                    }
                    curr_hop = x;
                }
                break;
            }
            else
            {
                size++;    
            }        
        }
        reversed = false;
    }
}  // end ProtoTree::Iterator::Reset()

void ProtoTree::Iterator::SetCursor(ProtoTree::Item& item)
{
    if ((0 != keysize_set) && (item.keysize != keysize_set))
    {
        prev = next = curr_hop = NULL;
        return;
    }
    // Save prefix subtree info
    unsigned int prefixSize = prefix_size;
    ProtoTree::Item* prefixItem = prefix_item;
    prefix_size = 0;
    prefix_item = NULL;
    
    Item* root = tree.roots[item.keysize - tree.keysize_min];
    if (NULL == root)
    {
        prev = next = curr_hop = NULL;
    }
    else if (root->left == root->right)
    {
        ASSERT(&item == root);
        curr_hop = NULL;
        if (reversed)
        {
            prev = NULL;
            next = root;
        }
        else
        {
            prev = root;
            next = NULL;
        }
    }
    else if (reversed)
    {
        // Setting "cursor" for "reversed" iteration is easy.
        curr_hop = NULL;
        prev = &item;   
        GetPrevItem();  // note this sets "next"
    }
    else
    {
        // Setting "cursor" for forward iteration is a little more complicated.
        // Given an "item", we can find the "curr_hop" for the tree 
        // entry that lexically precedes the "item"
        // (We do a reverse iteration to find that preceding entry)
        reversed = true;
        prev = &item;
        GetPrevItem();  // note "GetPrevItem()" also sets "next" as needed
        if (NULL == GetPrevItem())
        {
            Reset(false, keysize_set);
            // Move forward one place so "cursor" is correct position
            GetNextItem();
        }
        else
        {
            // This finds the proper "curr_hop" that goes with
            // the entry previous of "item" ...
            if ((&item != root) || (item.right != &item))
            {
                // Find the node's "predecessor" 
                // (has backpointer to "item"
                curr_hop = tree.FindPredecessor(item);
            }
            else
            {
                // Instead, use the other node with backpointer to root "item"
                ASSERT(&item == root);
                Item* s;
                Item* x = Bit(item.key, item.bit) ? item.left : item.right;
                ASSERT((x == item.left) || (x == item.right));
                do
                {
                    s = x;
                    if (Bit(item.key, x->bit))
                        x = x->right;
                    else
                        x = x->left;
                } while (x != &item);  
                curr_hop = s;       
            }
            // Move forward two places so "cursor" is correct position
            reversed = false;
            GetNextItem();
            GetNextItem();
        }
    }
    // Restore prefix subtree info
    if (0 != prefixSize)
    {
        prefix_item = prefixItem;
        prefix_size = prefixSize;
    }
}  // end ProtoTree::Iterator::SetCursor()

ProtoTree::Item* ProtoTree::Iterator::GetPrevItem()
{
    if (NULL != prev)
    {
        if (!reversed)
        {
            // This iterator has been moving forward
            // so we need to turn it around.
            reversed = true;
            GetPrevItem();
        }
        Item* item = prev;
        
        if (0 != prefix_size)
        {
            // Test "item" against our reference "prefix_item"
            if (!item->PrefixIsEqual(prefix_item->key, prefix_size))
            {
                prev = NULL;
                return NULL;
            }
        }
        
        Item* x;
        // Find node "q" with backpointer to "item"
        if ((NULL == item->parent) && (item->right == item))
            x = item->left;
        else
            x = item;
        Item* q;
        do                      
        {                       
            q = x;              
            if (Bit(item->key, x->bit))
                x = x->right;
            else                
                x = x->left;    
        } while (x != prev); 
        
        if (q->right != item)
        {
            // Go up the tree
            do
            {
                x = q;
                q = q->parent;
            } while ((NULL != q) && (x == q->left));

            if ((NULL == q) || (NULL == q->parent))
            {
                if ((NULL == q) || (q->left == q))
                {
                    // We've bubbled completely up or
                    // root has no left side, so we're done
                    prev = NULL;
                    if (0 != keysize_set)
                    {
                        // Go to next smallest keysize
                        unsigned int keysize = item->keysize - 1;
                        while ((NULL == prev) && (keysize > tree.keysize_min) && (keysize > prefix_size))
                        {
                            prev = tree.GetLastItem(keysize);
                            keysize--;
                        } 
                    }
                }
                else 
                {
                    // We've iterated to root from the right side
                    // and root has a left side we should check out
                    // So, find the left-side predecessor to root "q"
                    Item* root = q;
                    x = q->left;
                    do                      
                    {                       
                        q = x;              
                        if (Bit(root->key, x->bit))
                            x = x->right;
                        else                
                            x = x->left;    
                    } while (x != root); 
                    if (q->left != q)
                    {
                        // Go as far right of "q->left" as possible
                        q = q->left;
                        do
                        {
                            x = q;
                            q = q->right; 
                        } while (x == q->parent);
                    }
                    prev = q;
                }
                next = item;
                return item;
            }
        }  // end if (q->right != prev)
        
        if (q->left->parent != q)
        {
            if ((NULL == q->left->parent) &&
                (q->left->left != q->left) &&
                Bit(q->key, 0))
            {
                // We've come from the right and there is a left of root
                // So, go as far right of the left of root "q->left" as possible
                x = q->left->left;
                do
                {
                    q = x;
                    x = x->right; 
                } while (q == x->parent);
                prev = x;
            }
            else
            {
                // Otherwise, this is the appropriate iterate
                prev = q->left;
            }
        }
        else
        {
            // Go as far right of "q->left" as possible
            x = q->left;
            do
            {
                q = x;
                x = x->right; 
            } while (q == x->parent);
            prev = x;
        }
        next = item;
        return item;
    }
    else
    {
        return NULL;
    } 
}  // end ProtoTree::Iterator::GetPrevItem()

ProtoTree::Item* ProtoTree::Iterator::GetNextItem()
{
    if (NULL != next)
    {
        if (reversed)
        {
            // This iterator has been going backwards
            // so we need to turn it around
            reversed = false;
            SetCursor(*next);
        }
        Item* item = next;
        if (NULL == curr_hop)
        {
            // For whole tree iterations, look to next subroot (if there is one)
            if (0 == keysize_set)
            {
                unsigned int keysize = next->keysize;
                next = NULL;
                while (keysize < tree.keysize_max)
                {

                    keysize++;
                    Item* root = tree.roots[keysize - tree.keysize_min];
                    if (root)
                    {
                        Item* x = root;
                        while (x->left->parent == x)
                            x = x->left;
                        curr_hop = x;
                        next = x->left;
                        break;
                    }
                }
            }
            else
            {
                next = NULL;
            }
        }
        else
        {
            Item* x = curr_hop;
            if (((x->left != next) && (x->left->parent != x)) ||
                (x->right->parent == x))
            {
                next = x->left;
                // Now, update "curr_hop" if applicable
                // First, check for root node visit
                if ((NULL == next->parent) &&
                    (Bit(next->key, 0) != Bit(x->key, 0)))
                {
                    if (x->right == x)
                    {
                        next = x;
                        curr_hop = NULL; 
                    }
                    else
                    {
                        // Branch right and go as far left as possible
                        x = x->right;
                        while (x->left->parent == x) x = x->left;
                        next = x->left;
                        if (x->right->parent == x)
                        {
                            // Branch right and go as far left as possible
                            x = x->right;
                            while (x->left->parent == x) x = x->left;
                        }
                        curr_hop = x;
                    }
                }
                else if (x->right->parent == x)
                {
                    // Branch right and go as far left as possible
                    x = x->right;
                    while (x->left->parent == x) x = x->left;
                    curr_hop = x;
                }
                // Otherwise, there was no change to "curr_hop" for now
            }
            else // if (curr_hop->right->parent != curr_hop)
            {
                // Right item is next in iteration
                next = x->right;
                // Now, update "curr_hop" if applicable
                // First, check for root node visit
                if ((NULL == next->parent) &&
                    (next->right != next)  &&
                    (Bit(x->key, 0) != Bit(next->key, 0)))   
                {
                    // Branch right and go as far left as possible
                    x = next->right;
                    while (x->left->parent == x) x = x->left;
                    next = x->left;
                    if (x->right->parent == x)
                    {
                        // Again, branch right and go as far left as possible
                        x = x->right;
                        while (x->left->parent == x) x = x->left;
                    }
                    curr_hop = x;
                }
                else
                {
                    // Go back up the tree
                    Item* p = x->parent;
                    while ((p != NULL) && (p->right == x))
                    {
                        x = p;
                        p = x->parent;
                    }
                    if (NULL != p)
                    {
                        if ((NULL == p->parent) && (p->right == p))
                        {
                            p = NULL;
                        }
                        else if (p->right->parent == p)
                        {
                            // Branch right and go as far left as possible
                            Item* x = p->right;
                            while (x->left->parent == x) x = x->left;
                            p = x;
                        }
                    }
                    curr_hop = p;
                }
            }
        }  // end if/else (NULL == curr_hop)
        if (0 != prefix_size)
        {
            // Test "item" against prefix of item last returned
            if (!item->PrefixIsEqual(prefix_item->key, prefix_size))
                return NULL;
        }
        prev = item;
        return item;
    }
    else
    {
        return NULL;
    }  // end if/else(NULL != next)
}  // end ProtoTree::Item* ProtoTree::Iterator::GetNextItem()


ProtoSortedTree::ProtoSortedTree()
 : keysize(0), use_sign_bit(false), complement_2(false),
   positive_min(NULL), head(NULL), tail(NULL)
{
}

ProtoSortedTree::~ProtoSortedTree()
{
}

bool ProtoSortedTree::Init(unsigned int keyBytes, bool useSignBit, bool twosComplement)
{
    // (TBD) Should we empty/destroy if non-empty tree/list???
    keysize = keyBytes << 3;
    use_sign_bit = useSignBit;      // use first bit of key as a "sign" bit for sort
    complement_2 = twosComplement;  // use two's complement lexical sorting rule
    positive_min = head = tail = NULL;
    
    if (!item_tree.Init(keysize, keysize))
    {
        DMSG(0, "ProtoSortedTree::Init() error: %s\n", GetErrorString());
        keysize = 0;
        return false;
    }
    return true;
}


bool ProtoSortedTree::Insert(Item& item)
{
    // Is an item with this key already in tree?
    Item* match = Find(item.GetKey());
    if (NULL == match)
    {
        // Insert the item into our "item_tree"
        item_tree.Insert(item);
        // Now find the place to thread this new item into our linked list
        ProtoTree::Iterator iterator(item_tree, true, &item);
        match = static_cast<Item*>(iterator.PeekPrevItem());
        if (NULL == match)
        {
            if (NULL == head)
            {
                // nothing in the list yet
                item.Prepend(NULL);
                item.Append(NULL);
                head = tail = &item;
                if (use_sign_bit)
                {
                    bool itemSign = (0 != (0x80 & *(item.GetKey())));
                    if (!itemSign) positive_min = &item;
                }
            }
            else
            {
                // this item lexically precedes the anything in the tree
                if (use_sign_bit)
                {
                    bool itemSign = (0 != (0x80 & *(item.GetKey())));
                    if (itemSign)
                    {
                        // A signed "item" with no lexical predecessor
                        if (complement_2)
                        {
                            // is smallest negative number in a tree that
                            // has no positive numbers yet.
                            // So this goes to the "head" automatically!
                            item.Prepend(NULL);
                            item.Append(head);
                            head->Prepend(&item);
                            head = &item;
                        }
                        else
                        {
                            // is biggest negative number in a tree that
                            // has no positive numbers yet.
                            // So this goes to the "tail" automatically!
                            item.Append(NULL);
                            item.Prepend(tail);
                            tail->Append(&item);
                            tail = &item;
                        }
                        // Note: (itemSign && !headSign) is impossible here
                    }
                    else 
                    {
                        bool headSign = (0 != (0x80 & *(head->GetKey())));
                        if (headSign)
                        {
                            // (!itemSign && headSign)
                            // An unsigned (positive value) "item" with no lexical predecessor
                            // is the smallest _positive_ number in the tree
                            // so insert this before our prior "positive_min" or at tail
                            if (NULL != positive_min)
                            {
                                Item* prev = positive_min->GetPrev();
                                ASSERT(NULL != prev);
                                prev->Append(&item);
                                item.Prepend(prev);
                                item.Append(positive_min);
                                positive_min->Prepend(&item);
                            }
                            else
                            {
                                // first positive value, put at tail
                                item.Append(NULL);
                                item.Prepend(tail);
                                tail->Append(&item);
                                tail = &item;
                            }
                            positive_min = &item;
                        }
                        else
                        {
                            // (!itemSign && !headSign)
                            // Both positive, and "item" < "head"
                            // (no negative numbers in the tree?)
                            // So, insert "item" before "head"
                            item.Prepend(NULL);
                            item.Append(head);
                            head->Prepend(&item);
                            head = positive_min = &item;
                        }
                    }
                }
                else
                {
                    // this item lexically precedes the "head"
                    item.Prepend(NULL);
                    item.Append(head);
                    head->Prepend(&item);
                    head = &item;
                }  // end if/else (use_sign_bit)
            }
        }
        else
        {
            // this "item" lexically succeeds the "match"
            if (use_sign_bit)
            {
                bool itemSign = (0 != (0x80 & *(item.GetKey())));
                if (!itemSign)
                {
                    // (!itemSign && !matchSign)
                    // Both positive, match < item
                    // Insert "item" just after "match"
                    Item* next = match->GetNext();
                    if (NULL != next)
                        next->Prepend(&item);
                    else
                        tail = &item;
                    item.Append(next);
                    item.Prepend(match);
                    match->Append(&item);
                    ASSERT(0 == (0x80 & *(match->GetKey())));
                    // Note (!itemSign && matchSign) can't happen
                    // here since signed "match" (negative value) 
                    // _must_ lexically succeed unsigned "item"
                }
                else 
                {
                    bool matchSign = (0 != (0x80 & *(match->GetKey())));
                    if (matchSign)
                    {
                        // (itemSign && matchSign)
                        if (complement_2)
                        {
                            // Both negative, "match" < "item"
                            // Insert "item" just after "match"
                            Item* next = match->GetNext();
                            if (NULL != next)
                                next->Prepend(&item);
                            else
                                tail = &item;
                            item.Append(next);
                            item.Prepend(match);
                            match->Append(&item);
                        }
                        else
                        {
                            // Both negative, "item" < "match"
                            // Insert "item" before first equivalent "match"
                            Item* prev = match->GetPrev();
                            while ((NULL != prev) && !prev->IsInTree())
                            {
                                match = prev;
                                prev = prev->GetPrev();
                            }
                            if (NULL != prev)
                                prev->Append(&item);
                            else
                                head = &item;
                            item.Prepend(prev);
                            item.Append(match);
                            match->Prepend(&item);
                        }
                    }
                    else
                    {
                        // (itemSign && !matchSign)
                        if (complement_2)
                        {
                            // Smallest negative number, put at list head
                            item.Prepend(NULL);
                            item.Append(head);
                            head->Prepend(&item);
                            head = &item;
                        }
                        else
                        {
                            // Greatest negative number, put before "positive_min"
                            ASSERT(NULL != positive_min);
                            Item* prev = positive_min->GetPrev();
                            if (NULL != prev)
                                prev->Append(&item);
                            else
                                head = &item;
                            item.Prepend(prev);
                            item.Append(positive_min);
                            positive_min->Prepend(&item);
                        }
                    }
                }
            }
            else
            {
                // Insert the item just after the close "match" from "item_tree"
                Item* next = match->GetNext();
                if (NULL != next)
                    next->Prepend(&item);
                else
                    tail = &item;
                item.Append(next);
                match->Append(&item);
                item.Prepend(match);
            }
        }
    }
    else
    {
        if (match == &item)
        {
            DMSG(0, "ProtoSortedTree::Insert() warning: item already in tree!\n");
            return false;
        }
        // Insert the item just before the exact "match" from "item_tree"
        Item* prev = match->GetPrev();
        if (NULL != prev)
            prev->Append(&item);
        else
            head = &item;
        item.Prepend(prev);
        match->Prepend(&item);
        item.Append(match);
        if (use_sign_bit && (match == positive_min))
            positive_min = &item;
    }
    return true;
}  // end  ProtoSortedTree::Insert()

void ProtoSortedTree::Remove(Item& item)
{
    // 1) Remove from linked list
    Item* prev = item.GetPrev();
    Item* next = item.GetNext();
    if (NULL != prev)
        prev->Append(next);
    else
        head = next;
    if (NULL != next)
        next->Prepend(prev);
    else
        tail = prev;
    if (&item == positive_min) 
        positive_min = item.GetNext();
    // 2) Remove from ProtoTree, if applicable
    if (item.IsInTree())
    {
        // Remove "item" from the tree
        item_tree.Remove(item);
        // Does "prev" needs to take the place of "item" in the ProtoTree?
        if ((NULL != prev) && !prev->IsInTree())
            item_tree.Insert(*prev);
    }
}  // end ProtoSortedTree::Remove()

ProtoSortedTree::Item::Item(const char* theKey, unsigned int keyBytes, void* theValue)
 : ProtoTree::Item(theKey, keyBytes << 3, theValue), prev(NULL), next(NULL)
{
}

ProtoSortedTree::Item::~Item()
{
}

ProtoSortedTree::Iterator::Iterator(ProtoSortedTree& theTree)
 : tree(theTree), next(theTree.GetHead())
{
}

ProtoSortedTree::Iterator::~Iterator()
{
}

void ProtoSortedTree::Iterator::Reset(const char* keyMin)
{
    if (NULL != keyMin)
    {
        unsigned int keysize = tree.GetKeysize();
        Item* match = static_cast<Item*>(tree.GetItemTree().Find(keyMin, keysize));
        if (NULL == match)
        {
            Item tmpItem(keyMin, keysize >> 3);
            tree.GetItemTree().Insert(tmpItem);
            ProtoTree::Iterator iterator(tree.item_tree, false, &tmpItem);
            match = static_cast<Item*>(iterator.PeekNextItem());
        }
        if (NULL != match)
        {
            // Rewind to first matching item in linked list
            Item* prevItem = match->GetPrev();
            while ((NULL != prevItem) && (!prevItem->IsInTree()))
            {
                match = prevItem;
                prevItem = prevItem->GetPrev();
            }
        }
        next = match;
    }
    else
    {
        next = tree.GetHead();
    }
}  // end ProtoSortedTree::Iterator::Reset()

