#ifndef _PROTO_ADDRESS
#define _PROTO_ADDRESS
// This is an attempt at some generic network address generalization
// along with some methods for standard address lookup/ manipulation.
// This will evolve as we actually support address types besides IPv4

#include "protoDefs.h"
#include "protoTree.h"

#ifdef UNIX
#include <sys/types.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>    
#ifndef INADDR_NONE
const unsigned long INADDR_NONE = 0xffffffff;
#endif // !INADDR_NONE
#endif // UNIX

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef HAVE_IPV6
#include "tpipv6.h"  // not in Platform SDK
#endif // HAVE_IPV6
#endif // WIN32

#ifdef SIMULATE

#ifdef NS2
#include "config.h"  // for nsaddr_t which is a 32 bit int?
typedef nsaddr_t SIMADDR;
#endif // NS2

#ifdef OPNET
#include "opnet.h"
#include "ip_addr_v4.h"
typedef IpT_Address SIMADDR;
extern IpT_Address IPI_BroadcastAddr;
#endif // OPNET

#ifndef _SOCKADDRSIM
#define _SOCKADDRSIM
struct sockaddr_sim
{
    SIMADDR         addr;
    unsigned short  port;  
};
#endif  // ! _SOCKADDRSIM

#endif // SIMULATE

#ifdef HAVE_IPV6
// returns IPv4 portion of mapped address in network byte order
inline unsigned long IN6_V4MAPPED_ADDR(struct in6_addr* a) {return (((UINT32*)a)[3]);}
#endif // HAVE_IPV6

/*!
Network address container class with support
for IPv4, IPv6, and "SIM" address types.  Also
includes functions for name/address
resolution.
*/

class ProtoAddress
{
    public:            
        enum Type 
        {
            INVALID, 
            IPv4, 
            IPv6,
            ETH,
            SIM
        };
        // Construction/initialization
        ProtoAddress();
	    ~ProtoAddress();
        bool IsValid() const {return (INVALID != type);}
        void Invalidate()
        {
            type = INVALID;
            length = 0;
        }
        void Reset(ProtoAddress::Type theType, bool zero = true);
        
        
        static UINT8 GetLength(Type type)
        {
            switch (type)
            {
                case IPv4:
                    return 4;
                case IPv6:
                    return 16;
                case ETH:
                    return 6;
#ifdef SIMULATE
                case SIM:
                     return sizeof(SIMADDR);
#endif // SIMULATE
                default:
                    return 0;
            }   
        }
        
        // Address info
        ProtoAddress::Type GetType() const {return type;}
        UINT8 GetLength() const {return length;}
        bool IsBroadcast() const;
        bool IsMulticast() const;
        bool IsLoopback() const;
        bool IsSiteLocal() const;
        bool IsLinkLocal() const;
        bool IsUnspecified() const;
        const char* GetHostString(char*         buffer = NULL, 
                                  unsigned int  buflen = 0) const;
        
        // Host address/port get/set
        const char* GetRawHostAddress() const;
        bool SetRawHostAddress(ProtoAddress::Type   theType,
                               const char*          buffer,
                               UINT8                bufferLen);
        const struct sockaddr& GetSockAddr() const 
            {return ((const struct sockaddr&)addr);}
#ifdef HAVE_IPV6
        const struct sockaddr_storage& GetSockAddrStorage() const
            {return addr;}        
#endif // HAVE_IPV6        
        bool SetSockAddr(const struct sockaddr& theAddr);
        struct sockaddr& AccessSockAddr() 
            {return ((struct sockaddr&)addr);}
        UINT16 GetPort() const; 
        void SetPort(UINT16 thePort);
        void PortSet(UINT16 thePort);
        
        void SetUserData(const void* userData) 
            {user_data = userData;}
        const void* GetUserData() const
            {return user_data;}
        
        // Address comparison
        bool IsEqual(const ProtoAddress& theAddr) const
            {return (HostIsEqual(theAddr) && (GetPort() == theAddr.GetPort()));}
        bool HostIsEqual(const ProtoAddress& theAddr) const;
        int CompareHostAddr(const ProtoAddress& theAddr) const;
        
        
        UINT8 GetPrefixLength() const;
        void ApplyPrefixMask(UINT8 prefixLen);        
        void GetSubnetAddress(UINT8         prefixLen,  
                              ProtoAddress& subnetAddr) const;
        void GetBroadcastAddress(UINT8          prefixLen, 
                                 ProtoAddress&  broadcastAddr) const;
        
        // Name/address resolution
        bool ResolveFromString(const char* string);
        bool ResolveToName(char* nameBuffer, unsigned int buflen) const;
        bool ResolveLocalAddress(char* nameBuffer = NULL, unsigned int buflen = 0);
        
        // Miscellaneous
        // This function returns a 32-bit number which might _sometimes_
        // be useful as an address-based end system identifier.
        // (For IPv4 it's the address in host byte order)
        UINT32 EndIdentifier() const; 
        // Return IPv4 address in host byte order
        UINT32 IPv4GetAddress() const 
            {return (IPv4 == type) ? EndIdentifier() : 0;}
#ifdef SIMULATE
        SIMADDR SimGetAddress() const
        {
            return ((SIM == type) ? ((struct sockaddr_sim *)&addr)->addr : 
                                    0);
        }
        void SimSetAddress(SIMADDR theAddr)
        {
            type = SIM;
            length = sizeof(SIMADDR); // NSv2 addresses are 4 bytes? (32 bits)
            ((struct sockaddr_sim*)&addr)->addr = theAddr;
        }
#endif // SIMULATE
#ifdef WIN32
            static bool Win32Startup() 
            {
                WSADATA wsaData;
                WORD wVersionRequested = MAKEWORD(2, 2);
                return (0 == WSAStartup(wVersionRequested, &wsaData));
            }
            static void Win32Cleanup() {WSACleanup();}
#endif  // WIN32
            
        // This "ProtoAddress::List" class uses a Patrica tree (ProtoTree)
        // to keep a listing of addresses.  An "Iterator" is also provided
        // to go through the list when needed.
        // Note that only the _host_ address portion is kept!
        class List
        {
            public:
                List(unsigned int addrBitsMin = 32, unsigned int addrBitsMax = 128);
                ~List();
                void Destroy();
                bool Insert(const ProtoAddress& addr, const void* userData = NULL);
                bool Contains(const ProtoAddress& addr) const
                    {return (NULL != addr_tree.Find(addr.GetRawHostAddress(), addr.GetLength() << 3));}
                const void* GetUserData(const ProtoAddress& addr) const
                {
                    ProtoTree::Item* item = addr_tree.Find(addr.GetRawHostAddress(), addr.GetLength() << 3);
                    ProtoAddress* storedAddr = (NULL != item) ?  (ProtoAddress*)item->GetValue() : NULL;
                    return ((NULL != storedAddr) ? storedAddr->GetUserData() : NULL);
                }
                void Remove(const ProtoAddress& addr);
                unsigned int GetCount() const {return addr_count;}
                
                // Returns first address added to tree (subroot of ProtoTree) for given addrType
                bool GetFirstAddress(ProtoAddress::Type addrType, ProtoAddress& firstAddr) const;
                class Iterator;
                friend class Iterator;
                class Iterator
                {
                    public:
                        Iterator(const ProtoAddress::List& addrList);
                        ~Iterator();
                        
                        void Reset() {ptree_iterator.Reset();}
                        bool GetNextAddress(ProtoAddress& nextAddr);
                        
                    private:
                        ProtoTree::Iterator ptree_iterator;
                };  // end class ProtoAddress::List::Iterator
                
            private:
                unsigned int        addr_bits_min;
                unsigned int        addr_bits_max;
                ProtoTree           addr_tree;
                unsigned int        addr_count;
        };  // end class ProtoAddress::List

    private:
        Type                    type; 
        UINT8                   length;
#ifdef HAVE_IPV6
        struct sockaddr_storage addr;
#else       
#ifdef _WIN32_WCE
        struct sockaddr_in      addr;  // WINCE doesn't like anything else???
#else
// JPH 11/2/2005
#ifdef OPNET
	    struct sockaddr_sim	    addr;  // JPH	
#else
// end JPH 11/2/2005
        struct sockaddr         addr;
#endif // OPNET - JPH 11/2/2005
#endif // if/else WIN32_WCE
#endif // if/else (HAVE_IPV6 || WIN32)
        const void*             user_data;
};  // end class ProtoAddress

extern const ProtoAddress PROTO_ADDR_NONE;

#endif // _PROTO_ADDRESS

