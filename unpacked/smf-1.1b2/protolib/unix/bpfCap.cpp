/** This is an implementation of the Protolib class ProtoCap
 *  using the Berkeley Packet Filter (bpf) for packet capture
 *
 */
 
#include "protoCap.h"  // for ProtoCap definition
#include "protoSocket.h"
#include "protoDebug.h"


#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <net/bpf.h>
#include <sys/socket.h>
#include <net/if.h>
#include <stdio.h>  // for snprintf()
#include <fcntl.h>   // for open()
#include <unistd.h>  // for close()

class BpfCap : public ProtoCap
{
    public:
        BpfCap();
        ~BpfCap();
            
        bool Open(const char* interfaceName = NULL);
        void Close();
        bool Send(const char* buffer, unsigned int buflen);
        bool Forward(char* buffer, unsigned int buflen);
        bool Recv(char* buffer, unsigned int& numBytes, Direction* direction = NULL);
    
    private:
        char*           bpf_buffer;
        unsigned int    bpf_buflen;
        unsigned int    bpf_captured;
        unsigned int    bpf_index;
        char            eth_addr[6];    
};  // end class BpfCap


ProtoCap* ProtoCap::Create()
{
    return (static_cast<ProtoCap*>(new BpfCap));   
}

BpfCap::BpfCap()
 : bpf_buffer(NULL), bpf_buflen(0), bpf_captured(0), bpf_index(0)
{
    StartInputNotification();  // enable input notification by default
}

BpfCap::~BpfCap()
{
    Close();   
}

bool BpfCap::Open(const char* interfaceName)
{
    char buffer[256];
    if (NULL == interfaceName)
    {
        // Try to determine a "default" interface
        ProtoAddress localAddress;
        if (!localAddress.ResolveLocalAddress())
        {
            DMSG(0, "BpfCap::Open() error: couldn't auto determine local interface\n");
            return false;
        }
        if (!ProtoSocket::GetInterfaceName(localAddress, buffer, 256))
        {
             DMSG(0, "BpfCap::Open() error: couldn't determine local interface name\n");
            return false;
        }
        interfaceName = buffer;
    }
    
    ProtoAddress macAddr;
    if (!ProtoSocket::GetInterfaceAddress(interfaceName, ProtoAddress::ETH, macAddr))
    {
        DMSG(0, "BpfCap::Open() error getting interface MAC address\n");
        return false;   
    }
    int ifIndex = ProtoSocket::GetInterfaceIndex(interfaceName);
    
    int i = 0;
    int fd = -1;
    do
    {
        char bpfName[256];
        bpfName[255] = '\0';
        snprintf(bpfName, 255, "/dev/bpf%d", i++);
        fd = open(bpfName, O_RDWR);
    } while ((fd < 0) && (EBUSY == errno));
    
    if (fd < 0)
    {
        DMSG(0, "BpfCap::Open() all bpf devices busy\n");
        return false;   
    }    
    
    // Check the BPF version
    struct bpf_version bpfVersion;
    if (ioctl(fd, BIOCVERSION, (caddr_t)&bpfVersion) < 0) 
    {
		DMSG(0, "BpfCap::Open() ioctl(BIOCVERSION) error: %s\n",
                GetErrorString());
		close(fd);
		return false;
	}
	if (bpfVersion.bv_major != BPF_MAJOR_VERSION ||
	    bpfVersion.bv_minor < BPF_MINOR_VERSION) 
    {
		DMSG(0, "BpfCap::Open() kernel bpf version out of date\n");
        close(fd);
		return false;
	}
    // Set which interface we interested in, and buffer size
    unsigned int buflen;
    if ((ioctl(fd, BIOCGBLEN, (caddr_t)&buflen) < 0) || buflen < 32768)
		buflen = 32768;	
    for ( ; buflen != 0; buflen >>= 1) 
    {
        ioctl(fd, BIOCSBLEN, (caddr_t)&buflen);
        struct ifreq ifr;
        strncpy(ifr.ifr_name, interfaceName, sizeof(ifr.ifr_name));
		if (ioctl(fd, BIOCSETIF, (caddr_t)&ifr) >= 0)
			break;	
		if (errno != ENOBUFS) 
        {
            DMSG(0, "BpfCap::Open() ioctl(BIOCSETIF) error: %s\n",
                   GetErrorString());
            close(fd);
            return false;
		}
	}
    if (0 == buflen)
    {
        DMSG(0, "BpfCap::Open() unable to set bpf buffer\n");
        close(fd);
        return false;
    }    
    // Set interface to promiscuous mode
    // (TBD) Allow it to open in non-promiscuous mode???
    if (ioctl(fd, BIOCPROMISC, NULL) < 0) 
    {
        DMSG(0, "BpfCap::Open() ioctl(BIOCPROMISC) error: %s\n",
                GetErrorString());
        close(fd);
        return false;
    }
    
    // For protolib purposes we generally want immediate
    // packet-by-packet notification
    unsigned int enable = 1;
	if (ioctl(fd, BIOCIMMEDIATE, &enable) < 0) 
    {
		DMSG(0, "BpfCap::Open() ioctl(BIOCIMMEDIATE) error: %s\n",
                GetErrorString());
        close(fd);
        return false;
	}    
    
    // Set it to non-blocking, too
    int flags = fcntl(fd, F_GETFL, 0);
    if (-1 == flags)
    {
        DMSG(0, "BpfCap::Open() fcnt(F_GETFL) error: %s\n",
                GetErrorString());
        close(fd);
        return false;   
    }
    flags |= O_NONBLOCK;
    if (-1 == fcntl(fd, F_SETFL, flags))
    {
        DMSG(0, "BpfCap::Open() fcnt(F_SETFL O_NONBLOCK) error: %s\n",
                GetErrorString());
        close(fd);
        return false;  
    }
    
    if (ioctl(fd, BIOCGBLEN, (caddr_t)&buflen) < 0) 
    {
		DMSG(0, "BpfCap::Open() ioctl(BIOCGBLEN) error: %s\n",
                GetErrorString());
        close(fd);
        return false;
	}
    
    Close(); // just in case
    memcpy(eth_addr, macAddr.GetRawHostAddress(), 6);   
    
    // Allocate buffer for reading available packets
    if (NULL == (bpf_buffer = new char[buflen]))
    {
        DMSG(0, "BpfCap::Open() new bp_buffer error: %s\n",
                GetErrorString());
        Close();
        return false;   
    }
    bpf_buflen = buflen;
    descriptor = fd;
    
    if (!ProtoCap::Open(interfaceName))
    {
        DMSG(0, "BpfCap::Open() ProtoCap::Open() error\n");
        Close();
        return false;   
    }
    if_index = ifIndex;
    return true;
}  // end BpfCap::Open()

void BpfCap::Close()
{
    if (NULL != bpf_buffer)
    {
        delete[] bpf_buffer;
        bpf_buffer = NULL;   
        bpf_buflen = 0; 
    }    
    if (descriptor >= 0)
    {
        ProtoCap::Close();
        close(descriptor);
        descriptor = -1;   
    }
}  // end BpfCap::Close()

bool BpfCap::Recv(char* buffer, unsigned int& numBytes, Direction* direction)
{
    if (direction) *direction = UNSPECIFIED;
    if (bpf_index >= bpf_captured)
    {
        while (1)
        {
            ssize_t result = read(descriptor, bpf_buffer, bpf_buflen);
            if (result < 0)
            {
                switch (errno)
                {
                    case EINTR:
                        continue;  // try again
                    case EWOULDBLOCK:
                        numBytes = 0;
                        return true;
                    default:
                        DMSG(0, "BpfCap::Recv() read() error: %s\n",
                            GetErrorString());
                        numBytes = 0;
                        return false;   
                }
            }
            else
            {
                bpf_captured = result;
                bpf_index = 0;
                break;
            }
        }
    }
    
    // Determine next packet (if applicable)
    if (bpf_captured > bpf_index)
    {
        struct bpf_hdr* bpfHdr = (struct bpf_hdr*)(bpf_buffer + bpf_index);
        if (numBytes >= bpfHdr->bh_caplen)
        {
            memcpy(buffer, bpf_buffer+bpf_index+bpfHdr->bh_hdrlen, bpfHdr->bh_caplen);
            numBytes = bpfHdr->bh_caplen;
            bpf_index += BPF_WORDALIGN(bpfHdr->bh_caplen + bpfHdr->bh_hdrlen);
        }
        else
        {
            DMSG(0, "BpfCap::Recv() error packet too big for buffer\n");
            return false;
        }
    }
    else
    {
        numBytes = 0;
    }
    return true;
}  // end BpfCap::Recv()

bool BpfCap::Send(const char* buffer, unsigned int buflen)
{
    // Make sure packet is a type that is OK for us to send
    // (Some packets seem to cause a PF_PACKET socket trouble)
    UINT16 type;
    memcpy(&type, buffer+12, 2);
    type = ntohs(type);
    if (type <=  0x05dc) // assume it's 802.3 Length and ignore
    {
            DMSG(6, "BpfCap::Send() unsupported 802.3 frame (len = %04x)\n", type);
            return false;
    }
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
                DMSG(0, "BpfCap::Send() error: %s", GetErrorString());
                return false;
            }           
        }
    }
    return true;
}  // end BpfCap::Send()

bool BpfCap::Forward(char* buffer, unsigned int buflen)
{
    // Make sure packet is a type that is OK for us to send
    // (Some packets seem to cause a PF_PACKET socket trouble)
    UINT16 type;
    memcpy(&type, buffer+12, 2);
    type = ntohs(type);
    if (type <=  0x05dc) // assume it's 802.3 Length and ignore
    {
            DMSG(6, "BpfCap::Forward() unsupported 802.3 frame (len = %04x)\n", type);
            return false;
    }
    // Change the src MAC addr to our own
    // (TBD) allow caller to specify dst MAC addr ???
    memcpy(buffer+6, eth_addr, 6);
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
                DMSG(0, "BpfCap::Forward() error: %s", GetErrorString());
                return false;
            }           
        }
    }
    return true;
}  // end BpfCap::Forward()
