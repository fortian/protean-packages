/** This is an implementation of the Protolib class ProtoCap
 *  using "libpcap" for packet capture
 *
 */
 
#include "protoCap.h"  // for ProtoCap definition
#include "protoSocket.h"
#include "protoDebug.h"

#ifdef WIN32
//#include <winpcap.h>
#include <pcap.h>
#else
#include <pcap.h>    // for libpcap routines
#include <unistd.h>  // for write()
#endif  // end if/else WIN32/UNIX

class PcapCap : public ProtoCap
{
    public:
        PcapCap();
        ~PcapCap();
            
        bool Open(const char* interfaceName = NULL);
        bool IsOpen(){return (NULL != pcap_device);}
        void Close();
        bool Send(const char* buffer, unsigned int buflen);
        bool Forward(char* buffer, unsigned int buflen);
        bool Recv(char* buffer, unsigned int& numBytes, Direction* direction = NULL);
    
    private:
        pcap_t*         pcap_device; 
        char            eth_addr[6];    
};  // end class PcapCap


ProtoCap* ProtoCap::Create()
{
    return (static_cast<ProtoCap*>(new PcapCap));   
}

PcapCap::PcapCap()
 : pcap_device(NULL)
{
    StartInputNotification();  // enable input notification by default
}

PcapCap::~PcapCap()
{
    Close();   
}

bool PcapCap::Open(const char* interfaceName)
{
    int ifIndex;
    char buffer[256];
    if (NULL == interfaceName)
    {
        // Try to determine a "default" interface
        ProtoAddress localAddress;
        if (!localAddress.ResolveLocalAddress())
        {
            DMSG(0, "PcapCap::Open() error: couldn't auto determine local interface\n");
            return false;
        }
        if (!ProtoSocket::GetInterfaceName(localAddress, buffer, 256))
        {
             DMSG(0, "PcapCap::Open() error: couldn't determine local interface name\n");
            return false;
        }
        interfaceName = buffer;
        ifIndex = ProtoSocket::GetInterfaceIndex(interfaceName);
    } 
	else 
	{
		ifIndex = ProtoSocket::GetInterfaceIndex(interfaceName);
		if (ifIndex != 0) 
		{
			if(!ProtoSocket::GetInterfaceName(ifIndex ,buffer,256))
			{
				DMSG(0,"PcapCap::Open() error: couldn't get interface name of index %d\n",interfaceIndex);
				return false;
			}
			interfaceName = buffer;
		}
		else 
		{
			DMSG(0,"PcapCap::Open() error: coun't get interface index from interface name %s\n",interfaceName);
			return false;
		}
	}
    ProtoAddress macAddr;
    if (!ProtoSocket::GetInterfaceAddress(interfaceName, ProtoAddress::ETH, macAddr))
    {
        DMSG(0, "PcapCap::Open() error getting interface MAC address\n");
        return false;   
    }
    memcpy(eth_addr, macAddr.GetRawHostAddress(), 6);
    Close(); // just in case
    char errbuf[PCAP_ERRBUF_SIZE+1];
    errbuf[0] = '\0';
	TRACE("interfacename before pcap open live %s\n",interfaceName);
    pcap_device = pcap_open_live((char*)interfaceName, 65535, 1, 0, errbuf);
    if (NULL == pcap_device)
    {
        DMSG(0, "pcapExample: pcap_open_live() error: %s\n", errbuf);
        return false;   
    }
    // set non-blocking for async I/O
    if (-1 == pcap_setnonblock(pcap_device, 1, errbuf))
        DMSG(0, "pcapExample: pcap_setnonblock() warning: %s\n", errbuf);
#ifdef WIN32
    input_handle = pcap_getevent(pcap_device);
#else
    descriptor = pcap_get_selectable_fd(pcap_device);
#endif // if/else WIN32/UNIX
    
    if (!ProtoCap::Open(interfaceName))
    {
        DMSG(0, "PcapCap::Open() ProtoCap::Open() error\n");
        Close();
        return false;   
    }
    if_index = ifIndex;
    return true;
}  // end PcapCap::Open()

void PcapCap::Close()
{
    if (NULL != pcap_device)  
    {
        ProtoCap::Close();
        pcap_close(pcap_device);
        pcap_device = NULL;
#ifdef WIN32
        input_handle = INVALID_HANDLE_VALUE;
#else
        descriptor = -1;
#endif // if/else WIN32/UNIX
    } 
}  // end PcapCap::Close()

bool PcapCap::Recv(char* buffer, unsigned int& numBytes, Direction* direction)
{
    struct pcap_pkthdr* hdr;
    const u_char* data; 
    if (direction) *direction = UNSPECIFIED;  // (TBD) try to get a direction
    switch (pcap_next_ex(pcap_device, &hdr, &data))
    {
        case 1:     // pkt read
        {
            unsigned int copyLen = (numBytes > hdr->caplen) ? hdr->caplen : numBytes;
            memcpy(buffer, data, copyLen);
            numBytes = copyLen;
            return true;
            break;
        }
        case 0:     // no pkt ready?
        {
            numBytes = 0;
            return true;
        }
        default:    // error (or EOF for offline)
        {
            DMSG(0, "PcapCap::Recv() pcap_next_ex() error\n");
            numBytes = 0;
            return false;
        }
    } 
}  // end PcapCap::Recv()

bool PcapCap::Send(const char* buffer, unsigned int buflen)
{
    // Make sure packet is a type that is OK for us to send
    // (Some packets seem to cause a PF_PACKET socket trouble)
    UINT16 type;
    memcpy(&type, buffer+12, 2);
    type = ntohs(type);
	if (type <=  0x05dc) // assume it's 802.3 Length and ignore
    {
            DMSG(6, "PcapCap::Send() unsupported 802.3 frame (len = %04x)\n", type);
            return false;
    }

#ifdef WIN32
    int pcapreturn = pcap_sendpacket(pcap_device,(unsigned char*)buffer, buflen);
	TRACE("this is the pcapreturn value in Send %d\n",pcapreturn);
#else
    unsigned int put = 0;
    while (put < buflen)
    {
        ssize_t result = write(descriptor, buffer, buflen);
        if (result > 0)
        {
            put += result;
        }
        else
        {
            if (EINTR == errno)
            {
                continue;  // try again
            }
            else
            {
                DMSG(0, "PcapCap::Send() error: %s", GetErrorString());
                return false;
            }           
        }
    }
#endif
    return true;
}  // end PcapCap::Send()

bool PcapCap::Forward(char* buffer, unsigned int buflen)
{
    // Make sure packet is a type that is OK for us to send
    // (Some packets seem to cause a PF_PACKET socket trouble)
    UINT16 type;
    memcpy(&type, buffer+12, 2);
    type = ntohs(type);
    if (type <=  0x05dc) // assume it's 802.3 Length and ignore
    {
            DMSG(6, "PcapCap::Forward() unsupported 802.3 frame (len = %04x)\n", type);
            return false;
    }
    // Change the src MAC addr to our own
    // (TBD) allow caller to specify dst MAC addr ???
    memcpy(buffer+6, eth_addr, 6);
#ifdef WIN32
    int pcapreturn = pcap_sendpacket(pcap_device, (unsigned char*)buffer, buflen);
	TRACE("this is the pcapreturn value in Forward %d\n",pcapreturn);
#else
    unsigned int put = 0;
    while (put < buflen)
    {
        ssize_t result = write(descriptor, buffer, buflen);
        if (result > 0)
        {
            put += result;
        }
        else
        {
            if (EINTR == errno)
            {
                continue;  // try again
            }
            else
            {
                DMSG(0, "PcapCap::Forward() error: %s", GetErrorString());
                return false;
            }           
        }
    }
#endif
    return true;
}  // end PcapCap::Forward()
