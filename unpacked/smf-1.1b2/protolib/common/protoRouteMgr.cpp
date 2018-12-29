#include "protoRouteMgr.h"
#include "protoDebug.h"

bool ProtoRouteMgr::DeleteAllRoutes(ProtoAddress::Type addrType, unsigned int maxIterations)
{
    ProtoRouteTable rt;
    if (!rt.Init())
    {
        DMSG(0, "ProtoRouteMgr::DeleteAllRoutes() error initing table\n");
        return false; 
    }
    // Make multiple passes to get rid of possible
    // multiple routes to destinations
    // (TBD) extend ProtoRouteTable to support multiple routes per destination
    while (maxIterations-- > 0)
    {
        if (!GetAllRoutes(addrType, rt))
        {
            DMSG(0, "ProtoRouteMgr::DeleteAllRoutes() error getting routes\n");
            return false;   
        }
        if (rt.IsEmpty()) break;
        if (!DeleteRoutes(rt))
        {
            DMSG(0, "ProtoRouteMgr::DeleteAllRoutes() error deleting routes\n");
            return false;
        }
        rt.Clear();
    }

    if (0 == maxIterations)
    {
        DMSG(0, "ProtoRouteMgr::DeleteAllRoutes() couldn't seem to delete everything!\n");
        return false;
    }
    else
    {
        return true;
    }
}  // end ProtoRouteMgr::DeleteAllRoutes()

bool ProtoRouteMgr::SetRoutes(const ProtoRouteTable& routeTable)
{
    bool result = true;
    ProtoRouteTable::Iterator iterator(routeTable);
    ProtoRouteTable::Entry* entry;
    // First, set direct (interface) routes 
    while ((entry = iterator.GetNextEntry()))
    {
        if (entry->IsDirectRoute())
        {
            if (!SetRoute(entry->GetDestination(),
                          entry->GetPrefixLength(),
                          entry->GetGateway(),
                          entry->GetInterfaceIndex(),
                          entry->GetMetric()))
            {
                DMSG(0, "ProtoRouteMgr::SetAllRoutes() failed to set direct route to: %s\n",
                        entry->GetDestination().GetHostString());
                result = false;   
            }
        }
    }
    // Second, set gateway routes 
    iterator.Reset();
    while ((entry = iterator.GetNextEntry()))
    {
        if (entry->IsGatewayRoute())
        {
            if (!SetRoute(entry->GetDestination(),
                          entry->GetPrefixLength(),
                          entry->GetGateway(),
                          entry->GetInterfaceIndex(),
                          entry->GetMetric()))
            {
                DMSG(0, "ProtoRouteMgr::SetAllRoutes() failed to set gateway route to: %s\n",
                        entry->GetDestination().GetHostString());
                result = false;   
            }
        }
    }
    
    return result;
}  // end ProtoRouteMgr::SetRoutes()

bool ProtoRouteMgr::DeleteRoutes(const ProtoRouteTable& routeTable)
{
    bool result = true;
    ProtoRouteTable::Iterator iterator(routeTable);
    const ProtoRouteTable::Entry* entry;
    // First, delete gateway routes 
    while ((entry = iterator.GetNextEntry()))
    {
        if (entry->IsGatewayRoute())
        {
            if (!DeleteRoute(entry->GetDestination(),
			                 entry->GetPrefixLength(),
                             entry->GetGateway(),
                             entry->GetInterfaceIndex()))
            {
	            DMSG(0, "ProtoRouteMgr::DeleteAllRoutes() failed to delete gateway route to: %s\n",
	                    entry->GetDestination().GetHostString());
	            result = false;   
            }
        }
    }
    // Second, delete direct (interface) routes
    iterator.Reset(); 
    while ((entry = iterator.GetNextEntry()))
    {
        if (entry->IsDirectRoute())
        {
            if (!DeleteRoute(entry->GetDestination(),
			                 entry->GetPrefixLength(),
                             entry->GetGateway(),
                             entry->GetInterfaceIndex()))
            {
	            DMSG(0, "ProtoRouteMgr::DeleteAllRoutes() failed to delete direct route to: %s\n",
	                    entry->GetDestination().GetHostString());
	            result = false;   
            }
        }
    }
    // If there's a default entry delete it, too
    entry = routeTable.GetDefaultEntry();
    if (entry)
    {
        if (!DeleteRoute(entry->GetDestination(),
			             entry->GetPrefixLength(),
                         entry->GetGateway(),
                         entry->GetInterfaceIndex()))
        {
	        DMSG(0, "ProtoRouteMgr::DeleteAllRoutes() failed to delete default route\n");
	        result = false;   
        }
    }
    return result;
}  // end ProtoRouteMgr::DeleteRoutes()
