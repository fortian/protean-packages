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

#ifndef _PROTO_TREE
#define _PROTO_TREE

/*******************************************************
 * protoTree.h - This is a general purpose prefix-based  
 *               C++ Patricia tree. The code also provides 
 *               an ability to iterate over items with a 
 *               common prefix of arbitrary bit length.
 *               Note this prefix iteration must currently
 *               be done separately for each keysize in the
 *               tree (This may be automated in the future)
 *
 *               The class ProtoTree provides a relatively
 *               lightweight but reasonable performance
 *               data storage and retrieval mechanism which
 *               might be useful in prototype protocol
 *               implementations.  Thus, its inclusion
 *               in the NRL Protolib.
 *
 *               Future Goals: I would like to expand this
 *               with either an alternative implementation
 *               or modify the current code to support indexing 
 *               of strings (probably NULL-terminated) of
 *               arbitrary length or (less-likely) modify
 *               the algorithms used so that a single root
 *               could manage items of mixed key sizes.
 *               (Probably some type of "NULL termination"
 *               or similar will be needed for a singly-
 *               rooted tree - otherwise a non-binary radix
 *               might be necessary.  The current tree uses 
 *               multiple roots; one for each key size, and
 *               I don't like this limitation although it's
 *               OK for lots of uses!).  If a string-based
 *               approach were used, then some encoding,
 *               like base64 or similar could be used to
 *               represent binary content of arbitrary length.
 *               More recent thoughts on this:  It _can_ already
 *               be used for arbitrary length strings as is
 *               already implemented! (I tested it.)  Just init
 *               with a single keysize big enough for your max
 *               string length _and_ include a "NULL termination"
 *               character as part of the string (then the code
 *               shouldn't index itself into the string any further
 *               than the NULL terminator since this is a prefix tree).
 *               Just some thoughts!
 ******************************************************/

#include <string.h>

class ProtoTree
{
    public:
        ProtoTree();
        ~ProtoTree();
        
        // IMPORTANT: "keysizeMin" and "keysizeMax" are in units of "bits"
        bool Init(unsigned int keysizeMin, unsigned int keysizeMax);
        bool IsReady() const {return (NULL != roots);}
        bool IsEmpty() const;
        unsigned long GetKeysizeMin() const {return keysize_min;}
        unsigned long GetKeysizeMax() const {return keysize_max;}
        // Caution: These two methods delete the ProtoTree::Item pointers
        // found in the tree. (and the destructor calls these!)
        void Empty();
        void Destroy();
        
        class Item;
        
        // Insert the "item" into the tree (will fail if item with equivalent key already in tree)
        bool Insert(Item& item);
        
        // Remove the "item" from the tree
        void Remove(Item& item); 
        
        // Find item with exact match to "key" and "keysize"
        ProtoTree::Item* Find(const char* key, unsigned int keysize) const;
        
        ProtoTree::Item* FindClosestMatch(const char* key, unsigned int keysize) const;
        
        // Find largest item which is a prefix of the "key"
        ProtoTree::Item* FindPrefix(const char* key, unsigned int keysize) const;
        
        // For these, unspecified (ZERO) keysize searches the roots for you 
        ProtoTree::Item* GetRoot(unsigned int keysize = 0) const;
        ProtoTree::Item* GetFirstItem(unsigned int keysize = 0) const;
        ProtoTree::Item* GetLastItem(unsigned int keysize = 0) const;
        
        // These old "Insert" and "Remove" methods (using Item pointers) have been deprecated!
        /*bool Insert(Item* item)
            {return ((NULL != item) ? Insert(*item) : false);}
        Item* Remove(Item* item)
        {
            if (NULL != item) Remove(*item);
            return item;
        }*/
        
        // class ProtoTree::Item serves as a "container" 
        // for items to be stored in the tree
        class Iterator;
        class Item
        {
            friend class ProtoTree;
            friend class Iterator;

            public: 
                Item(const char*    theKey = NULL, 
                     unsigned int   theKeysize = 0, 
                     void*          theValue = NULL);  
                virtual ~Item();
                void Init(const char*  theKey, 
                          unsigned int theKeysize,      // in "bits" !!!
                          void*        theValue = NULL)
                {
                    key = theKey;
                    keysize = theKeysize;
                    value = theValue;
                }
                
                void* GetValue() const {return value;}
                const char* GetKey() const {return key;}
                unsigned int GetKeysize() const {return keysize;}
                
                bool IsInTree() const
                    {return (NULL != left);}
                
                unsigned int GetDepth() const;
                
                // This is useful for managing a reserved "pool" of Item (containers)
                // (TBD) add some automated memory management features to this ???
                class Pool
                {
                    public:
                        Pool();
                        ~Pool();
                        void Destroy();
                        Item* Get()
                        {   
                            Item* item = head;
                            head = item ? item->right : NULL;
                            return item;
                        }
                        void Put(Item& item)
                        {
                            item.right = head;
                            head = &item;
                        }
                    private:
                        Item*   head;
                };  // end class ProtoTree::Item::Pool
                friend class Pool;
                
            protected:    
                Item* GetParent() const {return parent;}
                Item* GetLeft() const {return left;}
                Item* GetRight() const {return right;}
                unsigned int GetBit() {return bit;}
                
                // bitwise comparison of the two keys
                bool IsEqual(const char* theKey, unsigned int theKeysize) const
                {
                    unsigned int n  = theKeysize >> 3;
                    return ((keysize != theKeysize) ? false :
                            ((0 != memcmp(key, theKey, n)) ? false :
                              (0 == ((unsigned char)(0xff << (8 - (theKeysize & 0x07))) & 
                                    (key[n] ^ theKey[n])))));
                }
                bool PrefixIsEqual(const char* prefix, unsigned int prefixSize) const
                {
                    unsigned int n  = prefixSize >> 3;
                    return ((0 != memcmp(key, prefix, n)) ? false :
                             (0 == ((unsigned char)(0xff << (8 - (prefixSize & 0x07))) & 
                                   (key[n] ^ prefix[n]))));
                }
                               
                const char*         key;
                unsigned int        keysize; // this could possiblby be removed.
                void*               value;   // this could be removed! 
                
                unsigned int        bit;
                Item*               parent;
                Item*               left;
                Item*               right;
                
        };  // end class ProtoTree::Item

        // This can be used to iterate over the entire data set (default)
        // or limited to specific "keysize" entries and/or of certain prefix
        class Iterator
        {
            public:
                Iterator(const ProtoTree&   tree, 
                         bool               reverse = false,
                         Item*              cursor = NULL);
                ~Iterator();
                
                void Reset(bool         reverse = false,
                           unsigned int keysize = 0,
                           const char*  prefix = NULL,
                           unsigned int prefixSize = 0);
                
                void SetCursor(Item& item);
                
                Item* GetPrevItem();
                Item* PeekPrevItem() const
                    {return prev;}
                
                Item* GetNextItem();
                Item* PeekNextItem() const
                    {return next;}
                
            private:
                const ProtoTree&    tree;
                bool                reversed;    // true if currently iterating backwards
                unsigned int        keysize_set; // if non-zero, limits iteration to specfic keysize root
                unsigned int        prefix_size; // if non-zero, iterating over items of certain prefix 
                Item*               prefix_item; // reference item with matching prefix for subtree iteration
                Item*               prev;  
                Item*               next;
                Item*               curr_hop;
                       
        };  // end class ProtoTree::Iterator  
                
        friend class ProtoTree::Iterator;
            
    //protected:
        // This finds the closest matching item with backpointer to "item"
        ProtoTree::Item* FindPredecessor(ProtoTree::Item& item) const;
        // This finds the root of a subtree of Items matching the given prefix
        ProtoTree::Item* FindPrefixSubtree(const char*     prefix, 
                                           unsigned int    prefixLen, // in bits
                                           unsigned int    keysize = 0,
                                           bool            minMatch = true) const;
        
        static bool Bit(const char* mask, unsigned int index)
            {return (0 != (mask[index >> 3] & (0x80 >> (index & 0x07))));}
        
        unsigned int    keysize_min;
        unsigned int    keysize_max;
        Item**          roots;
        
};  // end class ProtoTree


// This class extends ProtoTree::Item to provide a "threaded" tree
// for rapid (linked-list) iteration.  Also note that entries with
// duplicate key values are allowed.  Items are sorted lexically
// by their key by default.  Optionally the tree may be configured
// to treat the first bit of the key as a "sign" bit and order the
// sorted list properly with a mix of positive and negative values
// using two's complement (e.g. "int") rules or just signed ordering
// (e.g. "double").  Note that the key must point to a Big Endian
// representation of any such keys.  Also note that keysize here 
// is in units of bytes!
class ProtoSortedTree
{
    public:
        ProtoSortedTree();
        ~ProtoSortedTree();
        
        bool Init(unsigned int  keyBytes, 
                  bool          useSignBit = false, 
                  bool          twosComplement = true);  
        
        unsigned int GetKeyBytes() const
            {return (keysize >> 3);}
        
        class Iterator;
        
        class Item : protected ProtoTree::Item
        {
            friend class Iterator;
            friend class ProtoSortedTree;
            
            public:
                Item(const char* theKey = NULL, unsigned int keyBytes = 0, void* theValue = NULL);  
                virtual ~Item();
                
                void Init(const char* theKey = NULL, unsigned int keyBytes = 0, void* theValue = NULL)
                    {ProtoTree::Item::Init(theKey, keyBytes << 3, theValue);}
                
                const char* GetKey() 
                    {return key;}
                
                void* GetValue() 
                    {return value;}
                
             //private:  
                // Linked list (threading) helper
                Item* GetPrev() const
                    {return prev;}
                Item* GetNext() const
                    {return next;}
                void Prepend(Item*  item)
                    {prev = item;}
                void Append(Item*  item)
                    {next = item;}
                
                Item*            prev;
                Item*            next;
        };  // end class ProtoSortedTree::Item
        
        bool Insert(Item& item);
        
        // Random access methods (use ProtoTree)
        Item* Find(const char* key) const
        {
            return (static_cast<Item*>(item_tree.Find(key, keysize)));
        }
            
        void Remove(Item& item);
        
        class Iterator
        {
            public:
                Iterator(ProtoSortedTree& tree);
                ~Iterator();
                
                Item* GetNextItem()
                {
                    Item* item = next;
                    next = (NULL != next) ? next->GetNext() : NULL;
                    return item;
                }
                
                void Reset(const char* keyMin = NULL);
                
            private:
                ProtoSortedTree&    tree;
                Item*               next;
                
        };  // end class ProtoSortedTree::Iterator
        
        friend class Iterator;
            
    //private:
        Item* GetHead() const
            {return head;}    
        unsigned int GetKeysize() const
            {return keysize;}
        ProtoTree& GetItemTree()
            {return item_tree;}
            
        unsigned int keysize;       // in bits (but always multiple of 8)
        bool         use_sign_bit;
        bool         complement_2;
        Item*        positive_min;  // Pointer to minimum non-negative entry
        ProtoTree    item_tree;
        Item*        head;
        Item*        tail;
};  // end class ProtoSortedTree

#endif // PROTO_TREE
