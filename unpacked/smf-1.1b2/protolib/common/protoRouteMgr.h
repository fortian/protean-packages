#ifndef _PROTO_ROUTE_MGR
#define _PROTO_ROUTE_MGR

#include "protoRouteTable.h"
#include "protoSocket.h"

/*!
Base class for providing  a consistent
interface to manage operating system (or
other) routing engines.
 
Note: Since ProtoRouteTree can handle only one route per destination
GetAllRoutes() may miss some routes.  (Our bsdRouteMgr, linuxRouteMgr
and win32RouteMgr code has been tuned to deal with this for the
moment).  We'll likely extend ProtoRouteTree soon.
*/


class ProtoRouteMgr
{
    public:
        static ProtoRouteMgr* Create();
        virtual ~ProtoRouteMgr() {}
        
        virtual bool Open(const void* userData = NULL) = 0;
        virtual bool IsOpen() const = 0;
        virtual void Close() = 0;
        
        virtual bool GetAllRoutes(ProtoAddress::Type addrType,
                                  ProtoRouteTable&   routeTable) = 0;
        bool DeleteAllRoutes(ProtoAddress::Type addrType, unsigned int maxIterations = 8);
        bool SetRoutes(const ProtoRouteTable& routeTable);        
	    bool DeleteRoutes(const ProtoRouteTable& routeTable);
        
        virtual bool GetRoute(const ProtoAddress& dst, 
                              unsigned int        prefixLen,
                              ProtoAddress&       gw,
                              unsigned int&       ifIndex,
                              int&                metric) = 0;
        
        virtual bool SetRoute(const ProtoAddress&   dst,
                              unsigned int          prefixLen,
                              const ProtoAddress&   gw,
                              unsigned int          ifIndex,
                              int                   metric) = 0;
        
        virtual bool DeleteRoute(const ProtoAddress&    dst,
                                 unsigned int           prefixLen,
                                 const ProtoAddress&    gw,
                                 unsigned int           ifIndex) = 0;     


        
        // Here are some "shortcut" set route methods
        bool SetHostRoute(const ProtoAddress& dst,
                          const ProtoAddress& gw,
                          unsigned int        ifIndex,
                          int                 metric)
        {
            return SetRoute(dst, (dst.GetLength() << 3), gw, ifIndex, metric);   
        }
        
        bool SetDirectHostRoute(const ProtoAddress& dst,
                                unsigned int        ifIndex,
                                int                 metric)
        {
            ProtoAddress gw;
            gw.Invalidate();
            return SetRoute(dst, (dst.GetLength() << 3), gw, ifIndex, metric);   
        } 
        
        bool SetNetRoute(const ProtoAddress& dst, 
                         unsigned int        prefixLen,
                         const ProtoAddress& gw,
                         unsigned int        ifIndex,
                         int                 metric)
        {
            return SetRoute(dst, prefixLen, gw, ifIndex, metric);   
        }
        
        bool SetDirectNetRoute(const ProtoAddress& dst, 
                               unsigned int        prefixLen,
                               unsigned int        ifIndex,
                               int                 metric)
        {
            ProtoAddress gw;
            gw.Invalidate();
            return SetRoute(dst, prefixLen, gw, ifIndex, metric);   
        } 

        // Turn IP forward on/off
        virtual bool SetForwarding(bool state) = 0;
        
        // Network interface helper functions
        bool GetInterfaceAddress(unsigned int       ifIndex, 
                                 ProtoAddress::Type addrType,
                                 ProtoAddress&      theAddress)
        {
            ProtoAddress::List addrList;
            GetInterfaceAddressList(ifIndex, addrType, addrList);
            return addrList.GetFirstAddress(addrType, theAddress);
        }
        virtual unsigned int GetInterfaceIndex(const char* interfaceName) = 0;
        virtual bool GetInterfaceName(unsigned int  interfaceIndex, 
                                      char*         buffer, 
                                      unsigned int  buflen) = 0;
        
        // Retrieve all addresses for a given interface
        // (the retrieved addresses are added to the "addrList" provided)
        virtual bool GetInterfaceAddressList(unsigned int         ifIndex,
                                             ProtoAddress::Type   addrType,
                                             ProtoAddress::List&  addrList) = 0;
};  // end class ProtoRouteMgr



#endif // _PROTO_ROUTE_MGR
