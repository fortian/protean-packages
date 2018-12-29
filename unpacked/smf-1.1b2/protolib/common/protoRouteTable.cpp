
#include "protoRouteTable.h"
#include "protoDebug.h"

ProtoRouteTable::ProtoRouteTable()
{   
}

ProtoRouteTable::~ProtoRouteTable()
{   
    Destroy();
}

void ProtoRouteTable::Clear()
{   
    // First, get rid of entries contained in tree.
    Entry* next;
    while (NULL != (next = static_cast<Entry*>(tree.GetRoot())))
    {
        tree.Remove(*next);
        delete next;
    }
    // Second, get rid of default_entry
    if (default_entry.IsValid()) default_entry.Clear();
}  // end ProtoRouteTable::Clear()

void ProtoRouteTable::Destroy()
{   
    // First, clear the tree (and default_entry)
    Clear();
    // Second, destroy the tree
    tree.Destroy();
}  // end ProtoRouteTable::Destroy()

bool ProtoRouteTable::GetRoute(const ProtoAddress&  dst, 
                               unsigned int         prefixSize,
                               ProtoAddress&        gw,
                               unsigned int&        ifIndex,
                               int&                 metric)
{
    if (0 == prefixSize) 
    {
        gw = default_entry.gateway;
        ifIndex = default_entry.iface_index;
        metric = default_entry.metric;
        return true;   
    }
    Entry* entry = GetEntry(dst, prefixSize);
    if (entry)
    {
        gw = entry->gateway;
        ifIndex = entry->iface_index;
        metric = entry->metric;
        return true;
    }
    else
    {
        return false;   
    }
}  // end ProtoRouteTable::GetRoute()

bool ProtoRouteTable::SetRoute(const ProtoAddress&  dst,
                               unsigned int         prefixSize,
                               const ProtoAddress&  gw,
                               unsigned int         ifIndex,
                               int                  metric)
{
    if (0 == prefixSize)
    {
        
        default_entry.destination = dst;
        default_entry.gateway = gw;
        default_entry.iface_index = ifIndex;
        default_entry.metric = metric;
        return true;
    }
    Entry* entry = GetEntry(dst, prefixSize);
    // (TBD) allow for multiple routes to same dst/mask ???
    if (NULL == entry) 
        entry = CreateEntry(dst, prefixSize);
    if (entry)
    {
        entry->gateway = gw;
        entry->iface_index = ifIndex;
        entry->metric = metric;
        return true;
    }
    else
    {
        DMSG(0, "ProtoRouteTable::SetRoute() error creating entry\n");
        return false;   
    }
}  // end ProtoRouteTable::SetRoute()

bool ProtoRouteTable::FindRoute(const ProtoAddress& dst,
                                unsigned int        prefixSize,
                                ProtoAddress&       gw,
                                unsigned int&       ifIndex,
                                int&                metric)
{
    Entry* entry = FindRouteEntry(dst, prefixSize);
    if (entry)
    {
        gw = entry->gateway;
        ifIndex = entry->iface_index;
        metric = entry->metric;
        return true;
    }
    else
    {
        return false;
    }
}  // end ProtoRouteTable::FindRoute()

bool ProtoRouteTable::DeleteRoute(const ProtoAddress& dst, 
                                  unsigned int        maskLen,
                                  const ProtoAddress* gw,
                                  unsigned int        index)
{
    Entry* entry = GetEntry(dst, maskLen);
    if (entry)
      {
	if(gw) {
	  if(gw->IsValid() || entry->gateway.IsValid()){ //make sure at least one of this is valid
	    if (!entry->gateway.HostIsEqual(*gw)) 
	      {
		DMSG(0, "ProtoRouteTable::DeleteRoute() non-matching gateway addr\n");
		return false;
	      }
	  }
	}
	if ((0 != index) && (index != entry->iface_index))
	  {
	    DMSG(0, "ProtoRouteTable::DeleteRoute() non-matching interface index\n");
	    return false;
	  }
	DeleteEntry(entry);
	return true;
      }
    else
      {
        return false;   
      }
}  // end ProtoRouteTable::DeleteRoute()

ProtoRouteTable::Entry* ProtoRouteTable::CreateEntry(const ProtoAddress& dstAddr, 
                                                     unsigned int        prefixSize)
{
    if (!dstAddr.IsValid())
    {
        DMSG(0, "ProtoRouteTable::CreateEntry() invalid destination addr\n");
        return NULL;
    }
    Entry* entry = new Entry(dstAddr, prefixSize);
    if (NULL == entry)
    {
        DMSG(0, "ProtoRouteTable::CreateEntry() memory allocation error: %s\n",
                GetErrorString());
        delete entry;
        return NULL;   
    }
    // Bind the item and the entry
    if (tree.Insert(*entry))
    {
        return entry;
    }    
    else
    {
        DMSG(0, "ProtoRouteTable::CreateEntry() equivalent entry already exists?\n");
        delete entry;
        return NULL;
    }
}  // end ProtoRouteTable::CreateEntry()
        
ProtoRouteTable::Entry* ProtoRouteTable::GetEntry(const ProtoAddress& dstAddr, 
                                                  unsigned int        prefixSize) const
{
    if (0 == prefixSize)
    {
        if (default_entry.IsValid()) 
            return (Entry*)&default_entry;    
        else
            return NULL;
    }    
    return (static_cast<Entry*>(tree.Find(dstAddr.GetRawHostAddress(), prefixSize)));
}  // end ProtoRouteTable::GetEntry()

ProtoRouteTable::Entry* ProtoRouteTable::FindRouteEntry(const ProtoAddress& dstAddr, 
                                                        unsigned int        prefixSize) const
{
    if (0 == prefixSize) return GetDefaultEntry();
    Entry* entry = static_cast<Entry*>(tree.FindPrefix(dstAddr.GetRawHostAddress(), prefixSize));
    return (NULL != entry) ? entry : GetDefaultEntry();
}  // end ProtoRouteTable::FindRouteEntry()

void ProtoRouteTable::DeleteEntry(ProtoRouteTable::Entry* entry)
{
    if (NULL == entry) return;
    if (&default_entry == entry) 
    {
        default_entry.Clear();
        return;
    }
    Entry* entryFound = static_cast<Entry*>(tree.Find(entry->GetDestination().GetRawHostAddress(), entry->GetPrefixLength()));
    if (entryFound == entry)
    {
        tree.Remove(*entry);
        delete entry;
    }
    else
    {
        DMSG(0, "ProtoRouteTable::DeleteEntry() invalid entry\n");
    }
}  // end ProtoRouteTable::DeleteEntry()

ProtoRouteTable::Entry::Entry()
    : iface_index(0), metric(-1)
{
    destination.Invalidate();
    gateway.Invalidate();
}

ProtoRouteTable::Entry::Entry(const ProtoAddress& dstAddr, unsigned int prefixSize)
 : ProtoTree::Item(dstAddr.GetRawHostAddress(), prefixSize), iface_index(0)
{
    destination = dstAddr;
    gateway.Invalidate();
}

void ProtoRouteTable::Entry::Init(const ProtoAddress& dstAddr, unsigned int prefixSize)
{
    ProtoTree::Item::Init(dstAddr.GetRawHostAddress(), prefixSize);
    destination = dstAddr;
    gateway.Invalidate();
    iface_index = 0;
    metric = -1;
}  // end ProtoRouteTable::Entry::Init()

ProtoRouteTable::Iterator::Iterator(const ProtoRouteTable& theTable)
 : table(theTable), iterator(theTable.tree), default_pending(true)
{
    
}
ProtoRouteTable::Entry* ProtoRouteTable::Iterator::GetNextEntry()
{
    if (default_pending)
    {
        default_pending = false;
        Entry* entry = (Entry*)table.GetDefaultEntry();
        if (entry) return entry;
    }
    return (static_cast<Entry*>(iterator.GetNextItem()));
}  // end ProtoRouteTable::Iterator::GetNextEntry()
