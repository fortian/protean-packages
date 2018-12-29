
#include "protoCap.h"
#include "protoDebug.h"
#include "protoSocket.h"

#include <unistd.h>
#include <sys/socket.h>
#include <features.h>    /* for the glibc version number */
#if __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 1
#include <netpacket/packet.h>
#include <net/ethernet.h>     /* the L2 protocols */
#else
#include <asm/types.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>   /* The L2 protocols */
#endif
#include <netinet/in.h>

/** This implementation of ProtoCap uses the
 *  PF_PACKET socket type available on Linux systems
 */

class ProtoLinuxCap : public ProtoCap
{
    public:
        ProtoLinuxCap();
        ~ProtoLinuxCap();
            
        bool Open(const char* interfaceName = NULL);
        void Close();
        bool Send(const char* buffer, unsigned int buflen);
        bool Forward(char* buffer, unsigned int buflen);
        bool Recv(char* buffer, unsigned int& numBytes, Direction* direction = NULL);
    
    private:
        struct sockaddr_ll  iface_addr;
};  // end class ProtoLinuxCap

ProtoCap* ProtoCap::Create()
{
    return static_cast<ProtoCap*>(new ProtoLinuxCap());   
}  // end ProtoCap::Create()

ProtoLinuxCap::ProtoLinuxCap()
{
}

ProtoLinuxCap::~ProtoLinuxCap()
{   
    Close();
}

bool ProtoLinuxCap::Open(const char* interfaceName)
{
    char buffer[256];
    if (NULL == interfaceName)
    {
        // Try to determine a "default" interface
        ProtoAddress localAddress;
        if (!localAddress.ResolveLocalAddress())
        {
            DMSG(0, "ProtoLinuxCap::Open() error: couldn't auto determine local interface\n");
            return false;
        }
        if (!ProtoSocket::GetInterfaceName(localAddress, buffer, 256))
        {
             DMSG(0, "ProtoLinuxCap::Open() error: couldn't determine local interface name\n");
            return false;
        }
        interfaceName = buffer;
    }
    
    if ((descriptor = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) < 0)
    {
        DMSG(0, "ProtoLinuxCap::Open() socket(PF_PACKET) error: %s\n", GetErrorString());
        return false;   
    }
    
    // try to turn on broadcast capability (why?)
    int enable = 1;
    if (setsockopt(descriptor, SOL_SOCKET, SO_BROADCAST, &enable, sizeof(enable)) < 0)
        DMSG(0, "ProtoLinuxCap::Open() setsockopt(SO_BROADCAST) warning: %s\n", 
                GetErrorString());
    
    int ifIndex = ProtoSocket::GetInterfaceIndex(interfaceName);
    if (0 == ifIndex)
    {
        DMSG(0, "ProtoLinuxCap::Open() error getting interface index\n");
        Close();
        return false;   
    }
    
    // Set interface to promiscuous mode
    // (TBD) add ProtoCap method to control interface promiscuity
    struct packet_mreq mreq;
    memset(&mreq, 0, sizeof(struct packet_mreq));
    mreq.mr_ifindex = ifIndex;
    mreq.mr_type = PACKET_MR_PROMISC;
    if (setsockopt(descriptor, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
        DMSG(0, "ProtoLinuxCap::Open() setsockopt(PACKET_MR_PROMISC) warning: %s\n", 
                GetErrorString());
    
    
    ProtoAddress ethAddr;
    if (!ProtoSocket::GetInterfaceAddress(interfaceName, ProtoAddress::ETH, ethAddr))
    {
        DMSG(0, "ProtoLinuxCap::Open() error getting interface MAC address\n");
        Close();
        return false;
    }
    
    // Init our interface address structure   
    memset((char*)&iface_addr, 0, sizeof(iface_addr));
    iface_addr.sll_protocol = htons(ETH_P_ALL);
    iface_addr.sll_ifindex = ifIndex;
    iface_addr.sll_family = AF_PACKET;
    memcpy(iface_addr.sll_addr, ethAddr.GetRawHostAddress(), 6);
    iface_addr.sll_halen = ethAddr.GetLength();
    
    // bind() the socket to the specified interface
    if (bind(descriptor, (struct sockaddr*)&iface_addr, sizeof(iface_addr)) < 0)
    {
        DMSG(0, "ProtoLinuxCap::Open() bind error: %s\n", GetErrorString());
        Close();
        return false;      
    }
    
    if (!ProtoCap::Open(interfaceName))
    {
        DMSG(0, "ProtoLinuxCap::Open() ProtoCap::Open() error\n");
        Close();
        return false;   
    }
    if_index = ifIndex;
    return true;
}  // end ProtoLinuxCap::Open()

void ProtoLinuxCap::Close()
{
    ProtoCap::Close();
    close(descriptor);
    descriptor = INVALID_HANDLE;   
}  // end ProtoLinuxCap::Close()

bool ProtoLinuxCap::Send(const char* buffer, unsigned int buflen)
{
    // Make sure packet is a type that is OK for us to send
    // (Some packets seem to cause a PF_PACKET socket trouble)
    UINT16 type;
    memcpy(&type, buffer+12, 2);
    type = ntohs(type);
    if (type <=  0x05dc) // assume it's 802.3 Length and ignore
    {
            DMSG(6, "LinuxCap::Send() unsupported 802.3 frame (len = %04x)\n", type);
            return false;
    }
    size_t result = write(descriptor, buffer, buflen);
    if (result != buflen)
    {
        DMSG(0, "LinuxCap::Send() write() error: %s\n", GetErrorString());
        return false;  
    }    
    return true;
}  // end ProtoLinuxCap::Send()

bool ProtoLinuxCap::Forward(char* buffer, unsigned int buflen)
{
    // Make sure packet is a type that is OK for us to send
    // (Some packets seem to cause PF_PACKET socket trouble)
    UINT16 type;
    memcpy(&type, buffer+12, 2);
    type = ntohs(type);
    if (type <=  0x05dc) // assume it's 802.3 Length and ignore
    {
        DMSG(6, "LinuxCap::Forward() unsupported 802.3 frame (len = %04x)\n", type);
        return false;
    }
    // Change the src MAC addr to our own
    // (TBD) allow caller to specify dst MAC addr ???
    memcpy(buffer+6, iface_addr.sll_addr, 6);
    size_t result = write(descriptor, buffer, buflen);                   
    if (result != buflen)
    {
        DMSG(0, "LinuxCap::Send() write() error: %s\n", GetErrorString());
        return false;  
    }    
    return true;
}  // end ProtoLinuxCap::Forward()

bool ProtoLinuxCap::Recv(char* buffer, unsigned int& numBytes, Direction* direction)
{
    struct sockaddr_ll pktAddr;
    socklen_t addrLen = sizeof(pktAddr);
    int result = recvfrom(descriptor, buffer, (size_t)numBytes, 0, 
                          (struct sockaddr*)&pktAddr, &addrLen);
    if (result < 0)
    {
        numBytes = 0;
        if ((EAGAIN != errno) && (EINTR != errno))
            DMSG(0, "ProtoLinuxCap::Recv() error: %s\n", GetErrorString());
        else
            return true;
        return false;
    }
    else
    {
        if (direction)
        {
            if (pktAddr.sll_pkttype == PACKET_OUTGOING)
                *direction = OUTBOUND;
            else 
                *direction = INBOUND;
        }
        numBytes = result; 
        return true;   
    }
}  // end ProtoLinuxCap::Recv()
