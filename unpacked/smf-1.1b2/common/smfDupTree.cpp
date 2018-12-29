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
 * Revision history
 * Date  Author Details
 * 01/07/05 William Chao init version 
 * 01/07/05 Justin Dean init version
*/

#include "smfDupTree.h"
#include <stdlib.h>  // for rand()

SmfDuplicateTree::SmfDuplicateTree()
{
}

SmfDuplicateTree::~SmfDuplicateTree ()
{
    Destroy();
}

void SmfDuplicateTree::Destroy()
{
    Flow* nextFlow;
    while (NULL != (nextFlow = flow_list.GetHead()))
    {
        flow_tree.Remove(*nextFlow);
        flow_list.Remove(*nextFlow);
        delete nextFlow;
    }
    flow_tree.Destroy();
}  // end SmfDuplicateTree::Destroy()

bool SmfDuplicateTree::Init(UINT32 windowSize,       // in packets
                            UINT32 windowPastMax)    // in packets
{
    // This tree init currently handles single IPv4 arc addrs through 
    // IPv6 src::dst::taggerID addr tuples as keys (IPv6 addr taggerID)
    if (!flow_tree.Init(32, 384))
    {
        DMSG(0, "SmfDuplicateTree::Init() flow_tree init error: %s\n",
                GetErrorString());
        return false;
    }
    
    if (windowPastMax < windowSize)
    {
        DMSG(0, "SmfDuplicateTree::Init() error: invalid windowPastMax value\n");
        flow_tree.Destroy();
        return false;
    }
    
    window_size = windowSize;
    window_past_max = windowPastMax;
    return true;
}  // end SmfDuplicateTree::Init()
    

bool SmfDuplicateTree::IsDuplicate(unsigned int   currentTime,
                                   UINT32         seqNum,
                                   unsigned int   seqNumSize,      // in bits
                                   const char*    seqContext,
                                   unsigned int   seqContextSize)  // in bits
{
    Flow* theFlow = static_cast<Flow*>(flow_tree.Find(seqContext, seqContextSize)); 
    if (NULL == theFlow) 
    {
        // (TBD) We should have a max number of entries in tree
        theFlow = new Flow();
        if (NULL == theFlow)
        {
            DMSG(0,"SmfDuplicateTree::IsDuplicate() new PFlow() error: %s\n",
                    GetErrorString());
            return true;  // returns true to be safe (but breaks forwarding)
        }
        // (TBD) set window_size_past properly
        if (!theFlow->Init(seqContext, 
                           seqContextSize, 
                           seqNumSize, 
                           window_size, 
                           window_past_max))
        {
            DMSG(0,"SmfDuplicateTree::IsDuplicate() SmfSlidingWindow::Init() error\n");
            delete theFlow;
            return true;  // returns true to be safe (but breaks forwarding)
        }
        // (TBD) we may want to set a cache limit
        // (max number of entries in flow_list/flow_tree)
        flow_tree.Insert(*theFlow);
        theFlow->IsDuplicate(seqNum);
        theFlow->SetUpdateTime(currentTime);
        flow_list.Prepend(*theFlow);
        return false;
    }  
    else
    {
        if (theFlow->IsDuplicate(seqNum))
        {
            return true;
        }
        else
        {
            // "Bubble up" fresh flow to head of list
            flow_list.Remove(*theFlow);
            theFlow->SetUpdateTime(currentTime);
            flow_list.Prepend(*theFlow);
            return false;    
        }    
    }
}  // end SmfDuplicateTree::IsDuplicate()

void SmfDuplicateTree::Prune(unsigned int currentTime, unsigned int ageMax)
{
    Flow* oldestFlow;
    while ((oldestFlow = flow_list.GetTail()))
    {
        if (oldestFlow->GetAge(currentTime) > ageMax) 
        {
            flow_list.Remove(*oldestFlow);
            flow_tree.Remove(*oldestFlow);
            delete oldestFlow;
        }
        else
        {
            break; 
        }
    }
}  // end SmfDuplicateTree::FlowList::Prune()

SmfDuplicateTree::Flow::Flow()
{
}

SmfDuplicateTree::Flow::~Flow()
{
    window.Destroy();
    if (NULL != key)
    {
        delete[] key;
        key = NULL;
    }
    keysize = 0;
}

bool SmfDuplicateTree::Flow::Init(const char*   theKey,
                                  unsigned int  theKeysize,     // in bits
                                  UINT8         seqNumSize,     // in bits
                                  UINT32        windowSize,     // in packets
                                  UINT32        windowPastMax)  // in packets
{
    unsigned int keyBytes = theKeysize >> 3;
    if (0 != (theKeysize & 0x07)) keyBytes++;
    char* keySpace = new char[keyBytes];
    if (NULL == keySpace)
    {
        DMSG(0, "SmfDuplicateTree::Flow::Init() new char[] error: %s\n", GetErrorString());
        return false;
    }
    memcpy(keySpace, theKey, keyBytes);
    ProtoTree::Item::Init(keySpace, theKeysize);
    if (!window.Init(seqNumSize, windowSize, windowPastMax))
    {
        delete[] key;
        key = NULL;
        keysize = 0;
        DMSG(0, "SmfDuplicateTree::Flow::Init() error: DPD window bitmask init failed\n");
        return false;
    }
}  // end  SmfDuplicateTree::Flow::Init()

SmfDuplicateTree::FlowList::FlowList()
 : count(0), head(NULL), tail(NULL)
{
}

SmfDuplicateTree::FlowList::~FlowList()
{
}


SmfSequenceMgr::SmfSequenceMgr()
 : seq_mask(0)
{
}

SmfSequenceMgr::~SmfSequenceMgr()
{
    Destroy();
}

bool SmfSequenceMgr::Init(UINT8 numSeqBits)
{
    // Init to handle single IPv4 addr thru Ipv6 src::dst addr concatenated key sizes
    if (!flow_tree.Init(32, 256))
    {
        DMSG(0, "SmfSequenceMgr::Init() ProtoTree::Init() error: %s\n", GetErrorString());
        return false;
    }   
    seq_mask = 0xffffffff;
    if (numSeqBits < 32)
        seq_mask >>= (32 - numSeqBits);   
    return true; 
}  // end SmfSequenceMgr::Init()

void SmfSequenceMgr::Destroy()
{
    Flow* nextFlow;
    while (NULL != (nextFlow = flow_list.GetHead()))
    {
        flow_tree.Remove(*nextFlow);
        flow_list.Remove(*nextFlow);
        delete nextFlow;
    }
    flow_tree.Destroy();
}  // end SmfSequenceMgr::Destroy()

UINT32 SmfSequenceMgr::IncrementSequence(unsigned int        updateTime,
                                         const ProtoAddress* dstAddr, 
                                         const ProtoAddress* srcAddr)
{
    char addrKey[32];  // big enough for up IPv6 src::dst concatenation
    unsigned int addrBits = 0;
    if (NULL != srcAddr)
    {
        addrBits = srcAddr->GetLength();
        memcpy(addrKey, srcAddr->GetRawHostAddress(), addrBits);
    }
    if (NULL != dstAddr)
    {
        unsigned int dstLen = dstAddr->GetLength();
        memcpy(addrKey+addrBits, dstAddr->GetRawHostAddress(), dstLen);
        addrBits += dstLen;
    }
    else
    {
        DMSG(0, "SmfSequenceMgr::IncrementSequence() warning: NULL dstAddr?!\n");
        ASSERT(0);
    }
    addrBits <<= 3;
    Flow* flow = static_cast<Flow*>(flow_tree.Find(addrKey, addrBits));
    if (NULL == flow)
    {
        if (NULL == (flow = new Flow()))
        {
            DMSG(0, "SmfSequenceMgr::IncrementSequence() new Item error: %s\n", GetErrorString());
            return  (seq_global++ & seq_mask);
        }
        flow->Init(addrKey, addrBits);
        flow->SetSequence((UINT32)rand() & seq_mask);
        flow->SetUpdateTime(updateTime);
        flow_list.Prepend(*flow);
        flow_tree.Insert(*flow);
    }
    else
    {
        flow_list.Remove(*flow);
        flow->SetUpdateTime(updateTime);
        flow_list.Prepend(*flow);
    }
    return flow->IncrementSequence(seq_mask);
}  // end SmfSequenceMgr::IncrementSequence()


void SmfSequenceMgr::Prune(unsigned int currentTime, unsigned int ageMax)
{
    Flow* oldestFlow;
    while (NULL != (oldestFlow = flow_list.GetTail()))
    {
        if (oldestFlow->GetAge(currentTime) > ageMax) 
        {
            flow_list.Remove(*oldestFlow);
            flow_tree.Remove(*oldestFlow);
            delete oldestFlow;
        }
        else
        {
            break; 
        }
    }
}  // end SmfSequenceMgr::FlowList::Prune()

SmfSequenceMgr::Flow::Flow()
{
}

SmfSequenceMgr::Flow::~Flow()
{
    if (NULL != key)
    {
        delete[] key;
        key = NULL;
    }
    keysize = 0;
}

bool SmfSequenceMgr::Flow::Init(const char*  theKey, 
                                unsigned int theKeysize)
{
    if (NULL != key) delete[] key;
    unsigned int keyBytes = theKeysize >> 3;
    if (0 != (theKeysize & 0x07)) keyBytes++;
    char* keySpace = new char[keyBytes];
    if (NULL == keySpace)
    {
        keysize = 0;
        DMSG(0, "smfSequenceMgr::Flow::Init() new char[] error: %s\n", GetErrorString());
        return false;
    }
    memcpy(keySpace, theKey, keyBytes);
    ProtoTree::Item::Init(keySpace, theKeysize);
    return true;
}  // end SmfSequenceMgr::Flow::Init()

SmfSequenceMgr::FlowList::FlowList()
 : count(0), head(NULL), tail(NULL)
{
}

SmfSequenceMgr::FlowList::~FlowList()
{
}
