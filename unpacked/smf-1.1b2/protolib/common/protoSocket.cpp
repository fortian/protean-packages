#ifdef UNIX
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>  // for atoi()
#ifdef HAVE_IPV6
#ifdef MACOSX
#include <arpa/nameser.h>
#endif // MACOSX
#include <resolv.h>
#endif  // HAVE_IPV6
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <errno.h>
#include <fcntl.h>

#ifndef SIOCGIFHWADDR
#if defined(SOLARIS) || defined(IRIX)
#include <sys/sockio.h> // for SIOCGIFADDR ioctl
#include <netdb.h>      // for rest_init()
#include <sys/dlpi.h>
#include <stropts.h>
#else
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <ifaddrs.h> 
#endif  // if/else SOLARIS
#endif  // !SIOCGIFHWADDR
#endif  // UNIX

#ifdef WIN32
#include <winsock2.h>
#include <WS2tcpip.h>  // for extra socket options
#include <Iphlpapi.h>
#include <Iptypes.h>
#endif  // WIN32


// NOTE: This value should be confirmed.  It's the
//       apparently Linux-only approach we currently
//       use to set the IPv6 flow info header fields
//       Other approaches are under investigation
//       (We seem to be only able to affect
//        the "traffic class" bits at this time)
#ifndef IPV6_FLOWINFO_SEND
#define IPV6_FLOWINFO_SEND 33 // from linux/in6.h
#endif // !IPV6_FLOWINFO_SEND

#include "protoSocket.h"
#include "protoDebug.h"

#include <stdio.h>
#include <string.h>

// Hack for using with NRL IPSEC implementation
#ifdef HAVE_NETSEC
#include <net/security.h>
extern void* netsec_request;
extern int netsec_requestlen;
#endif // HAVE_NETSEC

// Use this macro (-DSOCKLEN_T=xxx) in your
// system's Makefile if the type socklen_t is 
// not defined for your system.
// (use "man getsockname" to see what type 
// is required for the last argument)

#ifdef SOCKLEN_T
#define socklen_t SOCKLEN_T
#else
#ifdef WIN32
//#define socklen_t int
#endif // WIN32
#endif // if/else SOCKLEN_T

#ifdef WIN32
const ProtoSocket::Handle ProtoSocket::INVALID_HANDLE = INVALID_SOCKET;
#else
const ProtoSocket::Handle ProtoSocket::INVALID_HANDLE = -1;
#endif  // if/else WIN32

ProtoSocket::ProtoSocket(ProtoSocket::Protocol theProtocol)
    : domain(IPv4), protocol(theProtocol), state(CLOSED), 
      handle(INVALID_HANDLE), port(-1), 
#ifdef HAVE_IPV6
      flow_label(0),
#endif // HAVE_IPV6
      notifier(NULL), notify_output(false), 
      notify_input(true), 
      notify_exception(false),
#ifdef WIN32
      input_event_handle(NULL), output_event_handle(NULL), 
      output_ready(false), input_ready(false),
#endif // WIN32
      listener(NULL), user_data(NULL)
{
   
}

ProtoSocket::~ProtoSocket()
{
    Close();
    if (listener)
    {
        delete listener;
        listener = NULL; 
    }
}


bool ProtoSocket::SetBlocking(bool blocking)
{
#ifdef UNIX
    if (blocking)
    {
        if(-1 == fcntl(handle, F_SETFL, fcntl(handle, F_GETFL, 0) & ~O_NONBLOCK))
        {
            DMSG(0, "ProtoSocket::SetBlocking() fcntl(F_SETFL(~O_NONBLOCK)) error: %s\n", GetErrorString());
            return false;
        }
    }
    else
    {
        if(-1 == fcntl(handle, F_SETFL, fcntl(handle, F_GETFL, 0) | O_NONBLOCK))
        {
            DMSG(0, "ProtoSocket::SetBlocking() fcntl(F_SETFL(O_NONBLOCK)) error: %s\n", GetErrorString());
            return false;
        }
    }
#endif // UNIX
    return true;  //Note: taken care automatically under Win32 by WSAAsyncSelect(), etc
}  // end ProtoSocket::SetBlocking(bool blocking)

bool ProtoSocket::SetNotifier(ProtoSocket::Notifier* theNotifier)
{
    if (notifier != theNotifier)
    {
        if (IsOpen())
        {
            // 1) Detach old notifier, if any
            if (notifier)
            {
                notifier->UpdateSocketNotification(*this, 0);
                if (!theNotifier)
                {
                    // Reset socket to "blocking"
                    if(!SetBlocking(true))
                        DMSG(0, "ProtoSocket::SetNotifier() SetBlocking(true) error\n", GetErrorString());
                }
            }
            else
            {
                // Set socket to "non-blocking"
	      if(!SetBlocking(false))
                {
                    DMSG(0, "ProtoSocket::SetNotifier() SetBlocking(false) error\n", GetErrorString());
                    return false;
                }
            }   
            // 2) Set and update new notifier (if applicable)
            notifier = theNotifier;
            if (!UpdateNotification())
            {
                notifier = NULL;  
                return false;
            } 
        }
        else
        {
            notifier = theNotifier;
        }
    }
    return true;
}  // end ProtoSocket::SetNotifier()


ProtoAddress::Type ProtoSocket::GetAddressType()
{
    switch (domain)
    {
        case LOCAL:
            return ProtoAddress::INVALID;  
        case IPv4:
            return ProtoAddress::IPv4;
#ifdef HAVE_IPV6
        case IPv6:
            return ProtoAddress::IPv6; 
#endif // HAVE_IPV6
#ifdef SIMULATE
        case SIM:
            return ProtoAddress::SIM;
#endif // SIMULATE
        default:
            return ProtoAddress::INVALID; 
    }  
}  // end ProtoSocket::GetAddressType()

// WIN32 needs the address type determine IPv6 _or_ IPv4 socket domain
// Note: WIN32 can't do IPv4 on an IPV6 socket!
bool ProtoSocket::Open(UINT16               thePort, 
                       ProtoAddress::Type   addrType,
                       bool                 bindOnOpen)
{
    if (IsOpen()) Close();
#ifdef HAVE_IPV6
    if(addrType == ProtoAddress::IPv6)
    {
        if (!HostIsIPv6Capable())
        {
            if (!SetHostIPv6Capable())
            {
                DMSG(0, "ProtoSocket::Open() system not IPv6 capable?!\n"); 
                return false;  
            }   
        }
        domain = IPv6;
    }
    else
#endif // HAVE_IPV6
    {
        domain = IPv4;
    }    
    
    int socketType = 0;
    switch (protocol)
    {
        case UDP:
            socketType = SOCK_DGRAM;
            break;
        case TCP:
            socketType = SOCK_STREAM;
            break;
        case RAW:  
            socketType = SOCK_RAW;
            break;
        default:
	        DMSG(0,"ProtoSocket::Open Error: Unsupported protocol\n");
	        return false;
    }
    
#ifdef WIN32
#ifdef HAVE_IPV6
    int family = (IPv6 == domain) ? AF_INET6: AF_INET;
#else
    int family = AF_INET;
#endif // if/else HAVE_IPV6
    // Startup WinSock
    if (!ProtoAddress::Win32Startup())
    {
        DMSG(0, "ProtoSocket::Open() WSAStartup() error: %s\n", GetErrorString());
        return false;
    }
    // Since we're might want QoS, we need find a QoS-capable UDP service provider
    WSAPROTOCOL_INFO* infoPtr = NULL;
    WSAPROTOCOL_INFO* protocolInfo = NULL;
    DWORD buflen = 0;
    // Query to properly size protocolInfo buffer
    WSAEnumProtocols(NULL, protocolInfo, &buflen);
    if (buflen)
    {
        int protocolType;
        switch (protocol)
        {
            case UDP:
                protocolType = IPPROTO_UDP;
                break;
            case TCP:
                protocolType = IPPROTO_TCP;
                break;
            case RAW:  
                protocolType = IPPROTO_RAW;
                break; 
        }
        
        // Enumerate, try to find multipoint _AND_ QOS-capable UDP, and create a socket
        if ((protocolInfo = (WSAPROTOCOL_INFO*) new char[buflen]))
        {
            int count = WSAEnumProtocols(NULL, protocolInfo, &buflen);
            if (SOCKET_ERROR != count)
            {
                for (int i = 0; i < count; i++)
                {
                    switch (protocol)
                    {
                        // This code tries to find a multicast-capable UDP/TCP socket
                        // providers _without_ QoS support, if possible (but note will 
                        // use one with QoS support if this is not possible.  The reason
                        // for this is that, ironically, it appears that a socket _without_ 
                        // QoS support allows for more flexible control of IP TOS setting 
                        // by the app?!  (Note that this may be revisited if we someday 
                        // again dabble with Win32 RSVP support (if it still exists) in future
                        // Protolib apps)
                        case UDP:
                            if ((IPPROTO_UDP == protocolInfo[i].iProtocol) &&
                                (0 != (XP1_SUPPORT_MULTIPOINT & protocolInfo[i].dwServiceFlags1)))
                            {
                                
                                if ((NULL == infoPtr) ||
                                    (0 == (XP1_QOS_SUPPORTED & protocolInfo[i].dwServiceFlags1)))
                                {
                                    infoPtr = protocolInfo + i;
                                }
                            }
                            break;
                        case TCP:
                            if (IPPROTO_TCP == protocolInfo[i].iProtocol)
                            {
                                if ((NULL == infoPtr) ||
                                    (0 == (XP1_QOS_SUPPORTED & protocolInfo[i].dwServiceFlags1)))
                                {
                                    infoPtr = protocolInfo + i;
                                }
                            }
                            break;
                        default:
                            break;
                    }
                }  // end for (i=0..count)
            }
            else
            {
                DMSG(0, "ProtoSocket: WSAEnumProtocols() error2!\n");
                delete[] protocolInfo;
                ProtoAddress::Win32Cleanup();
                return false;
            }
        }
        else
        {
            DMSG(0, "ProtoSocket: Error allocating memory!\n");
            ProtoAddress::Win32Cleanup();
            return false;
        }        
    }
    else
    {
        DMSG(0, "ProtoSocket::Open() WSAEnumProtocols() error1!\n");            
    }

    // Use WSASocket() to open right kind of socket
    // (Just a regular socket if infoPtr == NULL
    DWORD flags = WSA_FLAG_OVERLAPPED;
#ifndef _WIN32_WCE
    if (UDP == protocol) flags |= (WSA_FLAG_MULTIPOINT_C_LEAF | WSA_FLAG_MULTIPOINT_D_LEAF);
#endif // !_WIN32_WCE
    handle = WSASocket(family, socketType, 0, infoPtr, 0, flags);
    if (protocolInfo) delete[] protocolInfo;
    if (INVALID_HANDLE == handle)
    {
        DMSG(0, "ProtoSocket::WSASocket() error: %s\n", GetErrorString());
        return false;
    }
    if (NULL == (input_event_handle = WSACreateEvent()))
    {
        DMSG(0, "ProtoSocket::Open() WSACreateEvent() error: %s\n", GetErrorString());
        Close();
        return false;
    }
#else
    int family = (IPv6 == domain) ? PF_INET6: PF_INET;
    int socketProtocol = (SOCK_RAW == socketType) ? IPPROTO_RAW : 0;
    if (INVALID_HANDLE == (handle = socket(family, socketType, socketProtocol)))
    {
       DMSG(0, "ProtoSocket: socket() error: %s\n", GetErrorString());
       return false;
    }
    // (TBD) set IP_HDRINCL option for raw socket
#endif // if/else WIN32
    state = IDLE;
#ifdef NETSEC
    if (net_security_setrequest(handle, 0, netsec_request, netsec_requestlen))
    {
        DMSG(0, "ProtoSocket: net_security_setrequest() error: %s\n", 
                GetErrorString());
        Close();
        return false;
    }
#endif // NETSEC

#ifdef UNIX
    // Don't pass descriptor to child processes
    if(-1 == fcntl(handle, F_SETFD, FD_CLOEXEC))
        DMSG(0, "ProtoSocket::Open() fcntl(FD_CLOEXEC) warning: %s\n", GetErrorString());
    // Make the socket non-blocking
    if (notifier)
    {
        if(-1 == fcntl(handle, F_SETFL, fcntl(handle, F_GETFL, 0) | O_NONBLOCK))
        {
            DMSG(0, "ProtoSocket::Open() fcntl(F_SETFL(O_NONBLOCK)) error: %s\n",
                    GetErrorString());
            Close();
            return false;
        }
    }
#endif // UNIX
    if (bindOnOpen)
    {
        if (!Bind(thePort))
		{
			Close();
			return false;
		}
    }
    else
    {
        port = -1;
        if (!UpdateNotification())
        {
            DMSG(0, "ProtoSocket::Open() error installing async notification\n");
            Close();
            return false;
        }
    } 

    return true;
}  // end ProtoSocket::Open()

bool ProtoSocket::UpdateNotification()
{    
    if (notifier)
    {
        if (IsOpen() && !SetBlocking(false))
        {
            DMSG(0, "ProtoSocket::UpdateNotification() SetBlocking() error\n");
            return false;   
        }
        int notifyFlags = NOTIFY_NONE;
        if (listener)
        {
            switch (protocol)
            {
                case UDP:
                case RAW:
                    if (notify_input && IsBound())
                        notifyFlags = NOTIFY_INPUT;
                    else
                        notifyFlags = NOTIFY_NONE;
		    if (notify_output) 
		      notifyFlags |= NOTIFY_OUTPUT;

                    if (IsOpen() && notify_exception)
                        notifyFlags |= NOTIFY_EXCEPTION;
                    break;

                case TCP:
                    switch(state)
                    {
                        case CLOSED:
                        case IDLE:
                            notifyFlags = NOTIFY_NONE;
                            break;
                        case CONNECTING:
                            notifyFlags = NOTIFY_OUTPUT;
                            break;
                        case LISTENING:
                            notifyFlags = NOTIFY_INPUT;
                            break;
                        case CONNECTED:
                            notifyFlags = NOTIFY_INPUT;
                            if (notify_output) 
                                notifyFlags |= NOTIFY_OUTPUT;
                            break;  
                    }  // end switch(state)
                    break; 
	            default:
                    DMSG(0,"ProtoSocket::UpdateNotification Error: Unsupported protocol.\n");
	                break;
            }  // end switch(protocol)
        }  // end if(listener)
        return notifier->UpdateSocketNotification(*this, notifyFlags);
    }  
    else
    {
        return true;   
    }
}  // end ProtoSocket::UpdateNotification()

void ProtoSocket::OnNotify(ProtoSocket::Flag theFlag)
{
    ProtoSocket::Event event = INVALID_EVENT;
    if (NOTIFY_INPUT == theFlag)
    {
        switch (state)
        {
            case CLOSED:
                break;
            case IDLE:
                event = RECV;
                break;
            case CONNECTING:
                break;
            case LISTENING:
                // (TBD) check for error
                event = ACCEPT;
                break; 
            case CONNECTED:
                event = RECV;
                break;
        }        
    }
    else if (NOTIFY_OUTPUT == theFlag)
    {
        switch (state)
        {
            case CLOSED:
                break;
            case IDLE:
                event = SEND;
                break;
            case CONNECTING:
            {
#ifdef WIN32
                event = CONNECT;
                state = CONNECTED;
                UpdateNotification();
#else
                int err;
                socklen_t errsize = sizeof(err);
                if (getsockopt(handle, SOL_SOCKET, SO_ERROR, (char*)&err, &errsize))
                {
                    DMSG(0, "ProtoSocket::OnNotify() getsockopt() error: %s\n", GetErrorString());
                    
                } 
                else if (err)
                {
		        DMSG(1, "ProtoSocket::OnNotify() getsockopt() error: %s\n", GetErrorString()); 

                    Disconnect();
		            event = ERROR_;
                }
                else
                {
                    event = CONNECT;
                    state = CONNECTED;
                    UpdateNotification();
                }
#endif  // if/else WIN32
                break;
            }
            case LISTENING: 
                break;
            case CONNECTED:
                event = SEND;
                break;
        }    
    }
    else if (NOTIFY_EXCEPTION == theFlag)
    {
        event = EXCEPTION;
    }
    else if (NOTIFY_ERROR == theFlag)
    {
        switch(state)
    	{
	        case CONNECTING:
	        case CONNECTED:
	            Disconnect();
            default:
                event = ProtoSocket::ERROR_;
	            break;
	    }
    }
    else  // NOTIFY_NONE  (connection was purposefully ended)
    {
        switch(state)
        {
            case CONNECTING:
                //DMSG(0, "ProtoSocket::OnNotify() Connect() error: %s\n", GetErrorString());
            case CONNECTED:
                Disconnect();
                event = DISCONNECT;
                break;
            default:
                break;
        }
    }
    if (listener) listener->on_event(*this, event);
}  // end ProtoSocket::OnNotify()

bool ProtoSocket::Bind(UINT16 thePort, const ProtoAddress* localAddress)
{
    if (IsBound()) 
        Close();
    if (IsOpen() && 
        (NULL != localAddress) &&
        (GetAddressType() != localAddress->GetType()))
    {
        Close();
    }
    if (!IsOpen()) 
    {
        ProtoAddress::Type addrType = localAddress ? localAddress->GetType() : ProtoAddress::IPv4;
		if(!Open(thePort, addrType, false))
        {
			DMSG(0,"ProtoSocket::Bind() error opening socket on port %d\n",thePort);
			return false;
		}
	}
#ifdef HAVE_IPV6
    struct sockaddr_storage socketAddr;
    socklen_t addrSize;
    if (IPv6 == domain)
    {
	    addrSize = sizeof(struct sockaddr_in6);
        memset((char*)&socketAddr, 0, sizeof(struct sockaddr_in6));    
        ((struct sockaddr_in6*)&socketAddr)->sin6_family = AF_INET6;
        ((struct sockaddr_in6*)&socketAddr)->sin6_port = htons(thePort);
        ((struct sockaddr_in6*)&socketAddr)->sin6_flowinfo = 0;
        if (localAddress)
        {
            ((struct sockaddr_in6*)&socketAddr)->sin6_addr = 
                ((const struct sockaddr_in6*)(&localAddress->GetSockAddrStorage()))->sin6_addr;
        }
        else
        {
            ((struct sockaddr_in6*)&socketAddr)->sin6_addr = in6addr_any;
        }
    }
    else
    {
        addrSize = sizeof(struct sockaddr_in);
        memset((char*)&socketAddr, 0, sizeof(struct sockaddr_in));    
        ((struct sockaddr_in*)&socketAddr)->sin_family = AF_INET;
        ((struct sockaddr_in*)&socketAddr)->sin_port = htons(thePort);
        if (localAddress)
        {
            ((struct sockaddr_in*)&socketAddr)->sin_addr = 
                ((const struct sockaddr_in*)(&localAddress->GetSockAddr()))->sin_addr;
        }
	    else
        {
            struct in_addr inAddr;
            inAddr.s_addr = htonl(INADDR_ANY);
            ((struct sockaddr_in*)&socketAddr)->sin_addr = inAddr;
        }
    }
#else
    struct sockaddr socketAddr;
    socklen_t addrSize = sizeof(struct sockaddr_in);
    memset((char*)&socketAddr, 0, sizeof(struct sockaddr_in));    
    ((struct sockaddr_in*)&socketAddr)->sin_family = AF_INET;
    ((struct sockaddr_in*)&socketAddr)->sin_port = htons(thePort);
    if (localAddress)
    {
        ((struct sockaddr_in*)&socketAddr)->sin_addr = 
            ((struct sockaddr_in*)(&localAddress->GetSockAddr()))->sin_addr;
    }
	else
    {
        struct in_addr inAddr;
        inAddr.s_addr = htonl(INADDR_ANY);
        ((struct sockaddr_in*)&socketAddr)->sin_addr = inAddr;
    }
#endif  //  if/else HAVE_IPV6
    
#ifdef UNIX
#ifdef HAVE_IPV6
    if ((IPv6 == domain) && (0 != flow_label))
        ((struct sockaddr_in6*)&socketAddr)->sin6_flowinfo = flow_label;
#endif // HAVE_IPV6
#endif // UNIX
    // Bind the socket to the given port     
    if (bind(handle, (struct sockaddr*)&socketAddr, addrSize) < 0)
    {
       DMSG(0, "ProtoSocket: bind() error: %s\n", GetErrorString());
       return false;
    }

    // Get socket name so we know our port number  
    socklen_t sockLen = addrSize;
    if (getsockname(handle, (struct sockaddr*)&socketAddr, &sockLen) < 0) 
    {    
        DMSG(0, "ProtoSocket: getsockname() error: %s\n", GetErrorString());
        return false;
    }

    switch(((struct sockaddr*)&socketAddr)->sa_family)
    {
        case AF_INET:    
            port = ntohs(((struct sockaddr_in*)&socketAddr)->sin_port);
            break;
#ifdef HAVE_IPV6        
        case AF_INET6:
            port = ntohs(((struct sockaddr_in6*)&socketAddr)->sin6_port);
            break;
#endif // HAVE_IPV6        
        default:
            DMSG(0, "ProtoSocket: getsockname() returned unknown address type: %s\n",
                    GetErrorString());
            return false;
    }
    return UpdateNotification();
}  // end ProtoSocket::Bind()

bool ProtoSocket::Shutdown()
{
    if (IsConnected() && (TCP == protocol))
    {
        bool notifyOutput = notify_output;
        if (notifyOutput)
        {
            notify_output = false;
            UpdateNotification();
        }
#ifdef WIN32
        if (SOCKET_ERROR == shutdown(handle, SD_SEND))
#else
        if (0 != shutdown(handle, 1))
#endif // if/else WIN32/UNIX
        {
            if (notifyOutput)
            {
                notify_output= true;
                UpdateNotification();
            }
            DMSG(0, "ProtoSocket::Shutdown() error: %s\n", GetErrorString());
            return false;
        }
        else
        {
            return true;
        }
    }
    else
    {
        DMSG(0, "ProtoSocket::Shutdown() error: socket not connected\n");
        return false;
    }
}  // end ProtoSocket::Shutdown()

void ProtoSocket::Close()
{

    if (IsOpen()) 
    {
      if (IsConnected()) {Disconnect();};
        state = CLOSED;
        UpdateNotification();   
#ifdef WIN32
        if (NULL != input_event_handle)
        {
            if (LOCAL == protocol)
                CloseHandle(input_event_handle);
            else
                WSACloseEvent(input_event_handle);
            input_event_handle = NULL;
        }
#endif // if WIN32
        if (INVALID_HANDLE != handle)
        {
#ifdef WIN32
            if (SOCKET_ERROR == closesocket(handle))
                DMSG(0, "ProtoSocket::Close() warning: closesocket() error: %s\n", GetErrorString());
            ProtoAddress::Win32Cleanup();
            output_ready = false;
#else
            close(handle);
#endif // if/else WIN32/UNIX
        handle = INVALID_HANDLE;
        }
        port = -1;
    }

}  // end Close() 


bool ProtoSocket::Connect(const ProtoAddress& theAddress)
{
    if (IsConnected()) Disconnect();
    if (!IsOpen() && !Open(0, theAddress.GetType()))
    {
        DMSG(0, "ProtoSocket::Connect() error opening socket!\n");
        return false;
    }
#ifdef HAVE_IPV6
    socklen_t addrSize = (IPv6 == domain) ? sizeof(sockaddr_storage) : 
                                            sizeof(struct sockaddr_in);
#else
    socklen_t addrSize = sizeof(struct sockaddr_in);
#endif // if/else HAVE_IPV6
    state = CONNECTING;
    if (!UpdateNotification())
    {   
        DMSG(0, "ProtoSocket::Connect() error updating notification\n");
        state = IDLE;
        return false;
    }
#ifdef WIN32
    int result = WSAConnect(handle, &theAddress.GetSockAddr(), addrSize,
                            NULL, NULL, NULL, NULL);
    if (SOCKET_ERROR == result)
    {
        if (WSAEWOULDBLOCK != WSAGetLastError())
        {
            DMSG(0, "ProtoSocket::Connect() WSAConnect() error: (%s)\n", GetErrorString());
            state = IDLE;
            UpdateNotification();
            return false;
        }
    }
#else
#ifdef HAVE_IPV6
    if (flow_label && (ProtoAddress::IPv6 == theAddress.GetType()))
            ((struct sockaddr_in6*)(&theAddress.GetSockAddrStorage()))->sin6_flowinfo = flow_label;
#endif // HAVE_IPV6
    int result = connect(handle, &theAddress.GetSockAddr(), addrSize);
    if (0 != result)
    {
        if (EINPROGRESS != errno)
        {
            DMSG(0, "ProtoSocket::Connect() connect() error: %s\n", GetErrorString());
            state = IDLE;
            UpdateNotification();
            return false;
        }
    }
#endif // if/else WIN32
    else
    {
        state = CONNECTED;
        if (!UpdateNotification())
        {
            DMSG(0, "ProtoSocket::Connect() error updating notification\n");
            state = IDLE;
            return false;
        }
    }
    destination = theAddress;
    return true;
}  // end bool ProtoSocket::Connect()

void ProtoSocket::Disconnect()
{
    if (IsConnected() || IsConnecting())
    {
        state = IDLE;
        //destination.Invalidate();
        UpdateNotification();
        struct sockaddr nullAddr;
        memset(&nullAddr, 0 , sizeof(struct sockaddr));
#ifdef UNIX
        nullAddr.sa_family = AF_UNSPEC;
        if (connect(handle, &nullAddr, sizeof(struct sockaddr)))
        {
            DMSG(12, "ProtoSocket::Disconnect() connect() error: %s)\n", GetErrorString());
            // (TBD) should we Close() and re-Open() the socket here?
        }
#else
        if (SOCKET_ERROR == WSAConnect(handle, &nullAddr, sizeof(struct sockaddr),
                                       NULL, NULL, NULL, NULL))
        {
            DMSG(12, "ProtoSocket::Disconnect() WSAConnect() error: (%s)\n", GetErrorString());
        }
        output_ready = false;
#endif  // WIN32
    }
}  // end ProtoSocket::Disconnect()

bool ProtoSocket::Listen(UINT16 thePort)
{
    if (IsBound())
    {
        if ((0 != thePort) && (thePort != port))
        {
            DMSG(0, "ProtoSocket::Listen() error: socket bound to different port.\n");
            return false;
        }
    }
    else
    {
        if (!Bind(thePort))
        {
            DMSG(0, "ProtoSocket::Listen() error binding socket.\n");
            return false; 
        } 
    } 
    if (UDP == protocol)
        state = CONNECTED;
    else
        state = LISTENING;
    if (!UpdateNotification())
    {
        state = IDLE;
        DMSG(0, "ProtoSocket::Listen() error updating notification\n");
        return false;
    }
    if (UDP == protocol) return true;
#ifdef WIN32
    if (SOCKET_ERROR == listen(handle, 5))
#else
    if (listen(handle, 5) < 0)
#endif // if/else WIN32/UNIX
    {
        DMSG(0, "ProtoSocket: listen() error: %s\n", GetErrorString());
        return false;
    }
    return true;
}  // end ProtoSocket::Listen()

bool ProtoSocket::Accept(ProtoSocket* newSocket)
{
    ProtoSocket& theSocket = newSocket ? *newSocket : *this;
    // Clone server socket
    if (this != &theSocket) theSocket = *this;
#ifdef HAVE_IPV6
	struct sockaddr_in6 socketAddr;
#else
    struct sockaddr socketAddr;
#endif // if/else HAVE_IPV6
    socklen_t addrLen = sizeof(socketAddr);
#ifdef WIN32
    if (this != &theSocket)
    {
        if (!ProtoAddress::Win32Startup())
        {
            DMSG(0, "ProtoSocket::Accept() WSAStartup() error: %s\n", GetErrorString());
            return false;
        }
    }
    Handle theHandle = WSAAccept(handle, (struct sockaddr*)&socketAddr, &addrLen, NULL, NULL);
#else
    Handle theHandle = accept(handle, (struct sockaddr*)&socketAddr, &addrLen);
#endif // if/else WIN32/UNIX
    if (INVALID_HANDLE == theHandle)
    {
        DMSG(0, "ProtoSocket::Accept() accept() error: %s\n", GetErrorString());
        if (this != &theSocket)
        {
#ifdef WIN32
            closesocket(theHandle);
            ProtoAddress::Win32Cleanup();
#endif // WIN32
            theSocket.handle = INVALID_HANDLE;
            theSocket.state = CLOSED;
        }
        return false;
    }
    if (LOCAL != domain)
        theSocket.destination.SetSockAddr((struct sockaddr&)socketAddr);
    // Get the socket name so we know our port number
    addrLen = sizeof(socketAddr);
    if (getsockname(theSocket.handle, (struct sockaddr*)&socketAddr, &addrLen) < 0) 
    {    
        DMSG(0, "ProtoSocket::Accept(): getsockname() error\n");
        if (this != &theSocket)
        {
 #ifdef WIN32
            closesocket(theHandle);
            ProtoAddress::Win32Cleanup();
#endif // WIN32
            theSocket.handle = INVALID_HANDLE;
            theSocket.state = CLOSED;
        }
	    return false;
    }
    switch(((struct sockaddr*)&socketAddr)->sa_family)
    {
	    case AF_INET:    
	        theSocket.port = ntohs(((struct sockaddr_in*)&socketAddr)->sin_port);
	        break;
#ifdef HAVE_IPV6	    
	    case AF_INET6:
	        theSocket.port = ntohs(((struct sockaddr_in6*)&socketAddr)->sin6_port);
	        break;
#endif // HAVE_IPV6	   
#ifndef WIN32
        case AF_UNIX:
            theSocket.port = -1;
            break;
#endif // !WIN32 
	    default:
	        DMSG(0, "ProtoSocket: getsockname() returned unknown address type");
            if (this != &theSocket)
            {
 #ifdef WIN32
                closesocket(theHandle);
                ProtoAddress::Win32Cleanup();
#endif // WIN32
                theSocket.handle = INVALID_HANDLE;
                theSocket.state = CLOSED;
            }
	        return false;;
    }  // end switch()
    if (this == &theSocket)
    {  
        state = CLOSED;
        UpdateNotification();
#ifdef WIN32
        closesocket(theSocket.handle);
#else
        close(theSocket.handle);
#endif // if/else WIN32/UNIX
    }
    else
    {   
#ifdef WIN32
        theSocket.input_event_handle = NULL;
        if (NULL == (theSocket.input_event_handle = WSACreateEvent()))
        {
            DMSG(0, "ProtoSocket::Accept() WSACreateEvent error: %s\n", GetErrorString());
            theSocket.Close();
            return false;
        }
#endif // WIN32
        if (listener)
        {
            if (!(theSocket.listener = listener->duplicate()))
            {
                DMSG(0, "ProtoSocket::Accept() listener duplication error: %s\n", ::GetErrorString());
                theSocket.Close();
                return false;
            }   
        }
        if (notifier)
        {
            theSocket.handle = theHandle;
            if(!theSocket.SetBlocking(false))
            {
                DMSG(0, "ProtoSocket::Accept() SetBlocking(false) error\n");
                theSocket.Close();
                return false;
	    }
        }
    }  // end if/else (this == &theSocket)
    theSocket.handle = theHandle;
    theSocket.state = CONNECTED;
    theSocket.UpdateNotification();
    return true;
}  // end ProtoSocket::Accept()

bool ProtoSocket::Send(const char*         buffer, 
                       unsigned int&       numBytes)
{
    if (IsConnected())
    {
        
#ifdef WIN32
        WSABUF sendBuf;
        sendBuf.buf = (char*)buffer;
        sendBuf.len = numBytes;
        DWORD bytesSent;
        if (SOCKET_ERROR == WSASend(handle, &sendBuf, 1, &bytesSent, 0, NULL, NULL))
        {
            numBytes = bytesSent;
            switch (WSAGetLastError())
            {
                case WSAEINTR:
                    return true;
                case WSAEWOULDBLOCK:
                    output_ready = false;
                    return true;
                case WSAENETRESET:
                case WSAECONNABORTED:
                case WSAECONNRESET:
                case WSAESHUTDOWN:
                case WSAENOTCONN:
                    output_ready = false;
                    OnNotify(NOTIFY_NONE);
                    break;
                default:
                    DMSG(0, "ProtoSocket::Send() WSASend() error: %s\n", GetErrorString());
                    break;
            }
            return false;
        }
        else
        {
            output_ready= true;
            numBytes = bytesSent;
            return true;
        }
#else
        int result = send(handle, buffer, (size_t)numBytes, 0);
        if (result < 0)
	  {

            numBytes = 0;
	    switch (errno)
            {
                case EINTR:
                case EAGAIN:
                    return true;
                case ENETRESET:
                case ECONNABORTED:
                case ECONNRESET:
                case ESHUTDOWN:
                case ENOTCONN:
                    OnNotify(NOTIFY_NONE);
                    break;
                default:
                    DMSG(0, "ProtoSocket::Send() send() error: %s\n", GetErrorString());
                    break;
            }
            return false;  
        }
        else
        {
            numBytes = result;
            return true;
        }
#endif // if/else WIN32/UNIX
    }
    else
    {
        DMSG(0, "ProtoSocket::Send() error unconnected socket\n");
        return false;   
    }
}  // end ProtoSocket::Send()

bool ProtoSocket::Recv(char*            buffer, 
                       unsigned int&    numBytes)
{
#ifdef WIN32
    WSABUF recvBuf;
    recvBuf.buf = buffer;
    recvBuf.len = numBytes;
    DWORD bytesReceived;
    DWORD flags = 0;
    if (SOCKET_ERROR == WSARecv(handle, &recvBuf, 1, &bytesReceived, &flags, NULL, NULL))
    {
        numBytes = bytesReceived;
        switch (WSAGetLastError())
        {
            case WSAEINTR:
            case WSAEWOULDBLOCK:
                break;
            case WSAENETRESET:
            case WSAECONNABORTED:
            case WSAECONNRESET:
            case WSAESHUTDOWN:
            case WSAENOTCONN:
                DMSG(0, "ProtoSocket::Recv() recv() error: %s\n", GetErrorString());
                OnNotify(NOTIFY_ERROR);
                break;
            default:
                DMSG(0, "ProtoSocket::Recv() recv() error: %s\n", GetErrorString());
                break;
        }
        return false;
    }
    else
    {
        numBytes = bytesReceived;
        return true;
    }
#else
    int result = recv(handle, buffer, numBytes, 0);
    if (result < 0)
    {
        numBytes = 0;
        switch (errno)
        {
            case EINTR:
            case EAGAIN:
                DMSG(4, "ProtoSocket::Recv() recv() error: %s\n", GetErrorString());               
                break;
            case ENETRESET:
            case ECONNABORTED:
            case ECONNRESET:
            case ESHUTDOWN:
            case ENOTCONN:
                OnNotify(NOTIFY_ERROR);
                break;
            default:
                DMSG(0, "ProtoSocket::Recv() recv() error: %s\n", GetErrorString());
                break;
        }
        return false;
    }
    else
    {
        numBytes = result;
        if (0 == result) OnNotify(NOTIFY_NONE);
        return true;
    }
#endif // if/else WIN32/UNIX
}  // end ProtoSocket::Recv()

bool ProtoSocket::SendTo(const char*         buffer, 
                         unsigned int        buflen,
                         const ProtoAddress& dstAddr)
{
    if (!IsOpen())
    {
        if (!Open(0, dstAddr.GetType()))
        {
            DMSG(0, "ProtoSocket::SendTo() error: socket not open\n");
            return false;
        }
    }
    if (IsConnected())
    {
        unsigned int numBytes = buflen;
        if (!Send(buffer, numBytes))
        {

	        DMSG(4, "ProtoSocket::SendTo() error: Send() error\n");
            return false;
        }
        else if (numBytes != buflen)
        {
            DMSG(0, "ProtoSocket::SendTo() error: Send() incomplete\n");
            return false;
        }
        else
        {
            return true;
        }
    }
    else
    {
        socklen_t addrSize;
#ifdef HAVE_IPV6
        if (flow_label && (ProtoAddress::IPv6 == dstAddr.GetType()))
            ((struct sockaddr_in6*)(&dstAddr.GetSockAddrStorage()))->sin6_flowinfo = flow_label;
        if (ProtoAddress::IPv6 == dstAddr.GetType())
            addrSize = sizeof(struct sockaddr_in6);
        else
#endif //HAVE_IPV6
            addrSize = sizeof(struct sockaddr_in);
#ifdef WIN32
        WSABUF wsaBuf;
        wsaBuf.len = buflen;
        wsaBuf.buf = (char*)buffer;
        DWORD numBytes;
        if (SOCKET_ERROR == WSASendTo(handle, &wsaBuf, 1, &numBytes, 0, 
            &dstAddr.GetSockAddr(), addrSize, NULL, NULL))
#else
        int result = sendto(handle, buffer, (size_t)buflen, 0, 
                            &dstAddr.GetSockAddr(), addrSize);
        if (result < 0)
#endif // if/else WIN32/UNIX
        {
	  DMSG(4, "ProtoSocket::SendTo() sendto() error: %s\n", GetErrorString());
            return false;
        }
        else
        {   
            return true;
        }
    }
}  // end ProtoSocket::SendTo()


bool ProtoSocket::RecvFrom(char*            buffer, 
                           unsigned int&    numBytes, 
                           ProtoAddress&    sourceAddr)
{
    if (!IsBound())
    {
        DMSG(0, "ProtoSocket::RecvFrom() error: socket not bound\n");
        numBytes = 0;    
    }
#ifdef HAVE_IPV6    
    struct sockaddr_storage sockAddr;
#else
    struct sockaddr sockAddr;
#endif  // if/else HAVE_IPV6
    socklen_t addrLen = sizeof(sockAddr);
    int result = recvfrom(handle, buffer, (size_t)numBytes, 0, (struct sockaddr*)&sockAddr, &addrLen);
    if (result < 0)
    {
#ifdef WIN32
        DMSG(14, "ProtoSocket::RecvFrom() recvfrom() error: %s\n", GetErrorString());
#else
        if ((errno != EINTR) && (errno != EAGAIN))
            DMSG(4, "ProtoSocket::RecvFrom() recvfrom() error: %s\n", GetErrorString());
#endif // UNIX
        numBytes = 0;
        return false;
    }
    else
    {
        numBytes = result;
        sourceAddr.SetSockAddr(*((struct sockaddr*)&sockAddr));
        if (!sourceAddr.IsValid())
        {
            DMSG(0, "ProtoSocket::RecvFrom() Unsupported address type!\n");
            return false;
        }
        return true;
    }
}  // end ProtoSocket::RecvFrom()

#ifdef HAVE_IPV6
#ifndef IPV6_ADD_MEMBERSHIP
#define IPV6_ADD_MEMBERSHIP IPV6_JOIN_GROUP
#define IPV6_DROP_MEMBERSHIP IPV6_LEAVE_GROUP
#endif // !IPV6_ADD_MEMBERSHIP
#endif // HAVE_IPV6 


bool ProtoSocket::JoinGroup(const ProtoAddress& groupAddress, 
                            const char*         interfaceName)
{
    if (!IsOpen() && !Open(0, groupAddress.GetType(), false))
    {
        DMSG(0, "ProtoSocket::JoinGroup() error: socket not open\n");
        return false;
    }    
#ifdef WIN32
    // on WinNT 4.0 (or earlier?), we seem to need WSAJoinLeaf() for multicast to work
    // Thus NT 4.0 probably doesn't support IPv6 multicast???
    // So, here we use WSAJoinLeaf() iff the OS is NT and version 4.0 or earlier.
    bool useJoinLeaf = false;
    OSVERSIONINFO vinfo;
    vinfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&vinfo);
    if ((VER_PLATFORM_WIN32_NT == vinfo.dwPlatformId) &&
        ((vinfo.dwMajorVersion < 4) ||
            ((vinfo.dwMajorVersion == 4) && (vinfo.dwMinorVersion == 0))))
                useJoinLeaf = true;
    if (useJoinLeaf)
    {
        if (interfaceName && !SetMulticastInterface(interfaceName))
            DMSG(0, "ProtoSocket::JoinGroup() warning: error setting socket multicast interface\n");
        SOCKET result = WSAJoinLeaf(handle, &groupAddress.GetSockAddr(), sizeof(struct sockaddr), 
                                    NULL, NULL, NULL, NULL, JL_BOTH);
        if (INVALID_SOCKET == result)
        {
            DMSG(0, "WSAJoinLeaf() error: %d\n", WSAGetLastError());
            return false;
        }
        else
        {
            return true;
        }
    }  // end if (useJoinLeaf)
#endif // WIN32
    int result;
#ifdef HAVE_IPV6
    if  (ProtoAddress::IPv6 == groupAddress.GetType())
    {
        if (IN6_IS_ADDR_V4MAPPED(&((struct sockaddr_in6*)&groupAddress.GetSockAddrStorage())->sin6_addr))
        {
            struct ip_mreq mreq;
            mreq.imr_multiaddr.s_addr = 
                IN6_V4MAPPED_ADDR(&(((struct sockaddr_in6*)&groupAddress.GetSockAddrStorage())->sin6_addr));
            if (interfaceName)
            {
                ProtoAddress interfaceAddress;
#if defined(WIN32) && (WINVER < 0x0500)
                if (interfaceAddress.ResolveFromString(interfaceName))
                {
                    mreq.imr_interface.s_addr = htonl(interfaceAddress.IPv4GetAddress());
                }
                else if (GetInterfaceAddress(interfaceName, ProtoAddress::IPv4, interfaceAddress))
#else
                if (GetInterfaceAddress(interfaceName, ProtoAddress::IPv4, interfaceAddress))
#endif // if/else defined(WIN32) && (WINVER < 0x0500)
                {
                    mreq.imr_interface.s_addr = htonl(interfaceAddress.IPv4GetAddress());
                }
                else
                {
                    DMSG(0, "ProtoSocket::JoinGroup() invalid interface name\n");
                    return false;
                }
            }
            else
            {
                mreq.imr_interface.s_addr = INADDR_ANY;  
            } 
            result = setsockopt(handle, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq));
        }
        else
        {
            struct ipv6_mreq mreq;
            mreq.ipv6mr_multiaddr = ((struct sockaddr_in6*)&groupAddress.GetSockAddrStorage())->sin6_addr;
            if (interfaceName)
                mreq.ipv6mr_interface = GetInterfaceIndex(interfaceName);               
            else
                mreq.ipv6mr_interface = 0;
            result = setsockopt(handle, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq));
        }
    }
    else
#endif // HAVE_IPV6
    {
        struct ip_mreq mreq;
#ifdef HAVE_IPV6
        mreq.imr_multiaddr = ((struct sockaddr_in*)&groupAddress.GetSockAddrStorage())->sin_addr;
#else
        mreq.imr_multiaddr = ((struct sockaddr_in*)&groupAddress.GetSockAddr())->sin_addr;
#endif  // end if/else HAVE_IPV6
        if (interfaceName)
        {
            ProtoAddress interfaceAddress;
#if defined(WIN32) && (WINVER < 0x0500)
            if (interfaceAddress.ResolveFromString(interfaceName))
            {
                mreq.imr_interface.s_addr = htonl(interfaceAddress.IPv4GetAddress());
            }
            else if (GetInterfaceAddress(interfaceName, ProtoAddress::IPv4, interfaceAddress))
#else
            if (GetInterfaceAddress(interfaceName, ProtoAddress::IPv4, interfaceAddress))
#endif // if/else defined(WIN32) && (WINVER < 0x0500)
            {
                mreq.imr_interface.s_addr = htonl(interfaceAddress.IPv4GetAddress());
            }
            else
            {
                DMSG(0, "ProtoSocket::JoinGroup() invalid interface name\n");
                return false;
            }
        }
        else
        {
            mreq.imr_interface.s_addr = INADDR_ANY;  
        }
        result = setsockopt(handle, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq));
    }
    if (result < 0)
    { 
        DMSG(0, "ProtoSocket: Error joining multicast group: %s\n", GetErrorString());
        return false;
    }  
    return true;
}  // end ProtoSocket::JoinGroup() 

#ifdef WIN32

bool ProtoSocket::GetInterfaceAddressList(const char*       interfaceName,
                                      ProtoAddress::Type    addressType,
                                      ProtoAddress::List&   addrList,
                                      unsigned int*         interfaceIndex)  // optional to fill in (saves lines of code)
{
    ProtoAddress::List localAddrList; // used to cache link local addrs
    if (!strcmp(interfaceName, "lo"))  // loopback interface
    {
        // (TBD) should we also test for interfaceName == loopback address string?
        ProtoAddress loopbackAddr;
        if ((ProtoAddress::IPv4 == addressType) || (ProtoAddress::INVALID == addressType))
        {
            loopbackAddr.ResolveFromString("127.0.0.1");
        }
#ifdef HAVE_IPV6
        else if (ProtoAddress::IPv6 == addressType)
        {
            loopbackAddr.ResolveFromString("::1");
        }
#endif // HAVE_IPV6
        else
        {
            DMSG(0, "ProtoSocket::GetInterfaceAddressList() error: unsupported addressType\n");
            return false;
        }
        if (NULL != interfaceIndex) *interfaceIndex = 1; /// (TBD) what should we really set for interfaceIndex ???
        if (addrList.Insert(loopbackAddr))
        {
            return true;
        }
        else
        {
            DMSG(0, "ProtoSocket::GetInterfaceAddressList() error: unable to add loopback addr to list\n");
            return false;
        }
    }
    // Two different approaches are given here:
    // 1) Supports IPv4 and IPv6 on newer Windows Operating systems (WinXP and Win2003)
    //    using the "GetAdaptersAddresses()" call, and
    // 2) Supports IPv4 only on older operating systems using "GetIPaddrTable()" 
    // Then, try the "GetAdaptersAddresses()" approach first
    bool foundAddr = false;
    ULONG afFamily = AF_UNSPEC;
    switch (addressType)
    {
        case ProtoAddress::IPv4:
            afFamily = AF_INET;
            break;
#ifdef HAVE_IPV6
        case ProtoAddress::IPv6:
            afFamily = AF_INET6;
            break;
#endif // HAVE_IPV6
        default:
            break;
    }
    ULONG bufferLength = 0;

#if (WINVER >= 0x0501)
    // On NT4, Win2K and earlier, GetAdaptersAddresses() isn't to be found
    // in the iphlpapi.dll ...
    DWORD addrFlags = GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST;
    ULONG bufferSize = 0;
    DWORD result = GetAdaptersAddresses(afFamily, addrFlags, NULL, NULL, &bufferSize);
    if ((ERROR_BUFFER_OVERFLOW == result) ||
        (ERROR_NO_DATA == result))
    {
        if (ERROR_NO_DATA == result)
        {   
            DMSG(2, "ProtoSocket::GetInterfaceAddressList(%s) error: no matching interface adapters found.\n", interfaceName);
            return false;
        }
        char* addrBuffer = new char[bufferSize];
        if (NULL == addrBuffer)
        {
            DMSG(0, "ProtoSocket::GetInterfaceAddressList() new addrBuffer error: %s\n", ::GetErrorString());
            return false;
        }
        IP_ADAPTER_ADDRESSES* addrEntry = (IP_ADAPTER_ADDRESSES*)addrBuffer;
        if (ERROR_SUCCESS != GetAdaptersAddresses(afFamily, addrFlags, NULL, addrEntry, &bufferSize))
        {
            DMSG(0, "ProtoSocket::GetInterfaceAddressList() GetAdaptersAddresses() error: %s\n", ::GetErrorString());
            delete[] addrBuffer;
            return false;
        }
        while (addrEntry)
        {
            if (0 == strncmp(interfaceName, addrEntry->AdapterName, MAX_INTERFACE_NAME_LEN))
            {
                // A match was found!
                if (ProtoAddress::ETH == addressType)
                {
                    if (6 == addrEntry->PhysicalAddressLength)
                    {
                        ProtoAddress ethAddr;
                        ethAddr.SetRawHostAddress(ProtoAddress::ETH, (char*)&addrEntry->PhysicalAddress, 6);
                        if (NULL != interfaceIndex)
                        {
                            if (0 != addrEntry->IfIndex)
                                *interfaceIndex = addrEntry->IfIndex;
                            else
                                *interfaceIndex = addrEntry->Ipv6IfIndex;
                        }
                        delete[] addrBuffer;
                        if (addrList.Insert(ethAddr))
                        {
                            return true;
                        }
                        else
                        {
                            DMSG(0, "ProtoSocket::GetInterfaceAddressList() error: unable to add ETH addr to list\n");
                            return false;
                        }
                    }
                }
                else
                {
                    IP_ADAPTER_UNICAST_ADDRESS* ipAddr = addrEntry->FirstUnicastAddress;
                    while(NULL != ipAddr)
                    {
                        if ((afFamily == AF_UNSPEC) ||
                            (afFamily == ipAddr->Address.lpSockaddr->sa_family))
                        {
                            if (NULL != interfaceIndex)
                            {
                                if (0 != addrEntry->IfIndex)
                                    *interfaceIndex = addrEntry->IfIndex;
                                else
                                    *interfaceIndex = addrEntry->Ipv6IfIndex;
                            }
                            ProtoAddress ifAddr;
                            ifAddr.SetSockAddr(*(ipAddr->Address.lpSockaddr));
                            // Defer link local address to last
                            if (ifAddr.IsLinkLocal())
                            {
                                if (localAddrList.Insert(ifAddr))
                                    foundAddr = true;
                                else
                                    DMSG(0, "ProtoSocket::GetInterfaceAddressList() error: unable to add addr to local list\n");
                            }
                            else
                            {
                                if (addrList.Insert(ifAddr))
                                {
                                    foundAddr = true;
                                }  
                                else
                                {
                                    DMSG(0, "ProtoSocket::GetInterfaceAddressList() error: unable to add addr to list\n");
                                    delete[] addrBuffer;
                                    return false;
                                }
                            }
                        }
                        ipAddr = ipAddr->Next;
                    }
                }
            }
            addrEntry = addrEntry->Next;
        }
        delete[] addrBuffer;
        if (!foundAddr)
            DMSG(4, "ProtoSocket::GetInterfaceAddressList(%s) warning: no matching interface found\n", interfaceName);
    }
    else 
#endif // if (WINVER >= 0x0501)
    if (ERROR_BUFFER_OVERFLOW == GetAdaptersInfo(NULL, &bufferLength))
    {
        char* infoBuffer = new char[bufferLength];
        if (NULL == infoBuffer)
        {
            DMSG(0, "ProtoSocket::GetInterfaceAddressList(%s) new infoBuffer error: %s\n", interfaceName, ::GetErrorString());
            return false;
        }
        IP_ADAPTER_INFO* adapterInfo = (IP_ADAPTER_INFO*)infoBuffer; 
        if (NO_ERROR != GetAdaptersInfo(adapterInfo, &bufferLength))
        {       
            DMSG(0, "ProtoSocket::GetInterfaceAddressList(%s) new infoBuffer error: %s\n", interfaceName, ::GetErrorString());
            delete[] infoBuffer;
            return false;
        }
        while (NULL != adapterInfo)
        {
            if (0 == strncmp(interfaceName, adapterInfo->AdapterName, MAX_ADAPTER_NAME_LENGTH+4))
            {
                if (interfaceIndex) *interfaceIndex = adapterInfo->Index;
                if (ProtoAddress::ETH == addressType)
                {
                    if (6 == adapterInfo->AddressLength)
                    {
                        ProtoAddress ethAddr;
                        ethAddr.SetRawHostAddress(ProtoAddress::ETH, (char*)adapterInfo->Address, 6);
                        if (addrList.Insert(ethAddr))
                        {
                            delete[] infoBuffer;
                            return true;
                        }
                        else
                        {
                            DMSG(0, "ProtoSocket::GetInterfaceAddressList() error: unable to add ETH addr to list\n");
                            delete[] infoBuffer;
                            return false;
                        }
                    }
                    else
                    {
                        DMSG(0, "ProtoSocket::GetInterfaceAddressList(%s) error: non-Ethernet interface\n", interfaceName);
                        delete[] infoBuffer;
                        return false;
                    }
                }
                else if (ProtoAddress::IPv6 == addressType)
                {
                    DMSG(0, "ProtoSocket::GetInterfaceAddressList(%s) error: IPv6 not supported\n", interfaceName);
                    delete[] infoBuffer;
                    return false;
                }
                else // ProtoAddress::IPv4 == addressType
                {
                    ProtoAddress ifAddr;
                    if (ifAddr.ResolveFromString(adapterInfo->IpAddressList.IpAddress.String))
                    {
                        // (TBD) Do we need to check for loopback or link local here???
                        if (addrList.Insert(ifAddr))
                        {
                            foundAddr = true;
                        }
                        else
                        {
                            DMSG(0, "ProtoSocket::GetInterfaceAddressList() error: unable to add  addr to list\n"); 
                            delete[] infoBuffer;
                            return false;
                        }
                    }
                }
            }
            adapterInfo = adapterInfo->Next;
        }
        delete[] infoBuffer;
    }
    else if (ProtoAddress::ETH == addressType)
    {
        // Since "GetAdaptersInfo() didn't work (probably NT4), try this as a backup
        DWORD ifCount;
        if (NO_ERROR != GetNumberOfInterfaces(&ifCount))
        {
            DMSG(0, "ProtoSocket::GetInterfaceAddressList() GetNumberOfInterfaces() error: %s\n", ::GetErrorString());
            return false;
        }
        for (DWORD i = 1; i <= ifCount; i++)
        {
            MIB_IFROW ifRow;
            ifRow.dwIndex = i;
            if (NO_ERROR != GetIfEntry(&ifRow))
            {
                DMSG(4, "ProtoSocket::GetInterfaceAddressList() GetIfEntry(%d) error: %s\n", i, ::GetErrorString());
                continue;
            }
            // We use "bDescr" field because "wszName" doesn't seem to work (on non-Unicode)?
#ifdef _UNICODE
            // First, we need to convert the "interfaceName" to wchar_t
            wchar_t wideIfName[MAX_INTERFACE_NAME_LEN];
            mbstowcs(wideIfName, interfaceName, MAX_INTERFACE_NAME_LEN);
            if (0 == wcsncmp(ifRow.wszName, wideIfName, MAX_INTERFACE_NAME_LEN))
#else
            if (0 == strncmp((char*)ifRow.bDescr, interfaceName, ifRow.dwDescrLen))
#endif // if/else _UNICODE
            {
                if (6 == ifRow.dwPhysAddrLen)
                {
                    ProtoAddress ethAddr;
                    ethAddr.SetRawHostAddress(ProtoAddress::ETH, (char*)ifRow.bPhysAddr, 6);
                    if (NULL != interfaceIndex) *interfaceIndex = i;
                    if (addrList.Insert(ethAddr))
                    {
                        return true;
                    }
                    else
                    {
                        DMSG(0, "ProtoSocket::GetInterfaceAddressList() error: unable to add ETH addr to list\n"); 
                        return false;
                    }
                }
            }
        }
        DMSG(4, "ProtoSocket::GetInterfaceAddressList(%s) GetIfEntry() warning: no matching ETH interface found\n", 
                interfaceName);
    }
    else if ((ProtoAddress::IPv4 == addressType) || (ProtoAddress::INVALID == addressType))
    {
        // Since GetAdaptersAddresses() failed, try the other approach iff IPv4 == addressType
        DWORD ifCount;
        if (NO_ERROR != GetNumberOfInterfaces(&ifCount))
        {
            DMSG(0, "ProtoSocket::GetInterfaceAddressList() GetNumberOfInterfaces() error: %s\n", ::GetErrorString());
            return false;
        }
        // Second, iterate through addresses looking for a name match
        bool foundMatch = false;
        MIB_IFROW ifEntry;    
        for (DWORD i = 1; i <= ifCount; i++)
        {
            ifEntry.dwIndex = i;
            if (NO_ERROR != GetIfEntry(&ifEntry))
            {   
                DMSG(0, "ProtoSocket::GetInterfaceAddressList() GetIfEntry(%d) error: %s\n", i, ::GetErrorString());
                continue;
            }
            // We use the "bDescr" field because the "wszName" field doesn't seem to work
#ifdef _UNICODE
            wchar_t wideIfName[MAX_INTERFACE_NAME_LEN];
            mbstowcs(wideIfName, interfaceName, MAX_INTERFACE_NAME_LEN);
            if (0 == wcsncmp(wideIfName, ifEntry.wszName, MAX_INTERFACE_NAME_LEN))
#else
            if (0 == strncmp(interfaceName, (char*)ifEntry.bDescr, ifEntry.dwDescrLen))
#endif // if/else _UNICODE
            {
                foundMatch = true;
                break;
            }
        }
        if (foundMatch)
        {
            DWORD ifIndex = ifEntry.dwIndex;
            ULONG bufferSize = 0;
            if (ERROR_INSUFFICIENT_BUFFER == GetIpAddrTable(NULL, &bufferSize, FALSE))
            {
                char* tableBuffer = new char[bufferSize];
                if (NULL == tableBuffer)
                {   
                    DMSG(0, "ProtoSocket::GetInterfaceAddressList() new tableBuffer error: %s\n", ::GetErrorString());
                    return false;
                }
                MIB_IPADDRTABLE* addrTable = (MIB_IPADDRTABLE*)tableBuffer;
                if (ERROR_SUCCESS == GetIpAddrTable(addrTable, &bufferSize, FALSE))
                {
                    for (DWORD i = 0; i < addrTable->dwNumEntries; i++)
                    {
                        MIB_IPADDRROW* entry = &(addrTable->table[i]);
                        if (ifIndex == entry->dwIndex)
                        {
                            ProtoAddress ifAddr;
                            ifAddr.SetRawHostAddress(ProtoAddress::IPv4, (char*)&entry->dwAddr, 4);
                            if (NULL != interfaceIndex) *interfaceIndex = ifIndex;
                            foundAddr = true;
                            if (!addrList.Insert(ifAddr))
                            {
                                DMSG(0, "ProtoSocket::GetInterfaceAddressList() error: unable to add addr to list\n"); 
                                delete[] tableBuffer;
                                return false;
                            }
                        }
                    }
                }
                else
                {
                    DMSG(0, "ProtoSocket::GetInterfaceAddressList() warning GetIpAddrTable() error: %s\n", ::GetErrorString());
                }
                delete[] tableBuffer;
            }
            else
            {
                DMSG(0, "ProtoSocket::GetInterfaceAddressList() warning GetIpAddrTable() error 1: %s\n", ::GetErrorString());
            }
        }
        else
        {
            DMSG(4, "ProtoSocket::GetInterfaceAddressList(%s) warning: no matching IPv4 interface found\n",
                    interfaceName);
        }  // end if/else (foundMatch)
    }
    else
    {
        DMSG(4, "ProtoSocket::GetInterfaceAddressList() warning: GetAdaptersAddresses() error: %s\n", ::GetErrorString());
    }
    // Add any link local addrs found to addrList
    ProtoAddress::List::Iterator iterator(localAddrList);
    ProtoAddress localAddr;
    while (iterator.GetNextAddress(localAddr))
    {
        if (addrList.Insert(localAddr))
        {
            foundAddr = true;
        }
        else
        {
            DMSG(0, "ProtoSocket::GetInterfaceAddressList() warning: unable to add local addr to list\n"); 
            break;
        }   
    }
    if (foundAddr) return true;
    // As a last resort, check if the "interfaceName" is actually an address string
    ProtoAddress ifAddr;
    if (ifAddr.ResolveFromString(interfaceName))
    {
        char ifName[256];
        if (!GetInterfaceName(ifAddr, ifName, 256))
        {
            DMSG(0, "ProtoSocket::GetInterfaceAddressList(%s) error: no matching interface name found\n", interfaceName);
            return false;
        }
        bool result = GetInterfaceAddressList(ifName, addressType, addrList, interfaceIndex);
        return result;
    }
    else
    {
        char* typeString = NULL;
        switch (addressType)
        {   
            case ProtoAddress::IPv4:
                typeString = "IPv4";
                break;
#ifdef HAVE_IPV6
            case ProtoAddress::IPv6:
                typeString = "IPv6";
                break;
#endif // HAVE_IPV6
            case ProtoAddress::ETH:
                typeString = "Ethernet";
                break;
            default:
                typeString = "UNSPECIFIED";
                break;
        }
        DMSG(4, "ProtoSocket::GetInterfaceAddressList(%s) error: no matching %s interface found\n", interfaceName, typeString);
        return false;
    }
}  // end ProtoSocket::GetInterfaceAddressList()

int ProtoSocket::GetInterfaceIndices(int* indexArray, unsigned int indexArraySize)
{
    int indexCount = 0;
    ULONG bufferLength = 0;
    if (ERROR_BUFFER_OVERFLOW == GetAdaptersInfo(NULL, &bufferLength))
    {
        char* infoBuffer = new char[bufferLength];
        if (NULL == infoBuffer)
        {
            DMSG(0, "ProtoSocket::GetInterfaceIndices() new infoBuffer error: %s\n", ::GetErrorString());
            return -1;
        }
        IP_ADAPTER_INFO* adapterInfo = (IP_ADAPTER_INFO*)infoBuffer; 
        if (NO_ERROR != GetAdaptersInfo(adapterInfo, &bufferLength))
        {       
            DMSG(0, "ProtoSocket::GetInterfaceIndices() GetAdaptersInfo() error: %s\n", ::GetErrorString());
            delete[] infoBuffer;
            return -1;
        }
        while (NULL != adapterInfo)
        {
            if ((unsigned int)indexCount < indexArraySize)
                indexArray[indexCount] = adapterInfo->Index;
            indexCount++;
            adapterInfo = adapterInfo->Next;
        }
        delete[] infoBuffer;
        return indexCount;
    }
    else 
    {
        DWORD ifCount = 0;
        if (NO_ERROR != GetNumberOfInterfaces(&ifCount))
        {
            DMSG(0, "ProtoSocket::GetInterfaceIndices() GetNumberOfInterfaces() error: %s\n", ::GetErrorString());
            return false;
        }
        // Second, iterate through addresses looking for a name match
        MIB_IFROW ifEntry;    
        for (DWORD i = 1; i <= ifCount; i++)
        {
            ifEntry.dwIndex = i;
            if (NO_ERROR != GetIfEntry(&ifEntry))
            {   
                DMSG(0, "ProtoSocket::GetInterfaceAddressList() GetIfEntry(%d) error: %s\n", i, ::GetErrorString());
                continue;
            }
            if ((unsigned int)indexCount < indexArraySize)
                indexArray[indexCount] = i;
            indexCount++;
        }
        return indexCount;
    }
}  // end ProtoSocket::GetInterfaceIndices()

bool ProtoSocket::FindLocalAddress(ProtoAddress::Type addrType, ProtoAddress& theAddress)
{
    ULONG bufferLength = 0;
    if (ERROR_BUFFER_OVERFLOW == GetAdaptersInfo(NULL, &bufferLength))
    {
        char* infoBuffer = new char[bufferLength];
        if (NULL == infoBuffer)
        {
            DMSG(0, "ProtoSocket::FindLocalAddress() new infoBuffer error: %s\n", ::GetErrorString());
            return false;
        }
        IP_ADAPTER_INFO* adapterInfo = (IP_ADAPTER_INFO*)infoBuffer; 
        if (NO_ERROR != GetAdaptersInfo(adapterInfo, &bufferLength))
        {       
            DMSG(0, "ProtoSocket::FindLocalAddress() GetAdaptersInfo() error: %s\n", ::GetErrorString());
            delete[] infoBuffer;
            return false;
        }
        while (NULL != adapterInfo)
        {
            char ifName[MAX_ADAPTER_NAME_LENGTH + 4];
            if (GetInterfaceName(adapterInfo->Index, ifName, MAX_ADAPTER_NAME_LENGTH + 4))
            {
                if (GetInterfaceAddress(ifName, addrType, theAddress))
                {
                    if (!theAddress.IsLoopback())
                    {
                        delete[] infoBuffer;
                        return true;
                    }
                }   
            }
            adapterInfo = adapterInfo->Next;
        }
        delete[] infoBuffer;
        DMSG(0, "ProtoSocket::FindLocalAddress() no IPv%d address assigned\n",
                (addrType == ProtoAddress::IPv6) ? 6 : 4);
        return false;
    }
    else if (ProtoAddress::IPv4 == addrType)
    {
        DWORD ifCount = 0;
        if (NO_ERROR != GetNumberOfInterfaces(&ifCount))
        {
            DMSG(0, "ProtoSocket::FindLocalAddress() GetNumberOfInterfaces() error: %s\n", ::GetErrorString());
            return false;
        }
        // Second, iterate through addresses looking for a name match
        MIB_IFROW ifEntry;    
        for (DWORD i = 1; i <= ifCount; i++)
        {
            ifEntry.dwIndex = i;
            if (NO_ERROR != GetIfEntry(&ifEntry))
            {   
                DMSG(0, "ProtoSocket::FindLocalAddress() GetIfEntry(%d) error: %s\n", i, ::GetErrorString());
                continue;
            }
            char ifName[MAX_INTERFACE_NAME_LEN];
            if (GetInterfaceName(i, ifName, MAX_INTERFACE_NAME_LEN))
            {
                if (GetInterfaceAddress(ifName, addrType, theAddress))
                {
                    if (!theAddress.IsLoopback())
                    {
                        return true;
                    }
                }   
            }
        }
        // (TBD) should we set loopback addr if nothing else?
        DMSG(0, "ProtoSocket::FindLocalAddress() no IPv4 address assigned\n");
        return false;
    }
    else
    {
        // (TBD) should we set loopback addr if nothing else?
        DMSG(0, "ProtoSocket::FindLocalAddress() IPv6 not supported for this \n");
        return false;    
    }    
}  // end ProtoSocket::FindLocalAddress()

unsigned int ProtoSocket::GetInterfaceIndex(const char* interfaceName)
{
    ProtoAddress theAddress;
    unsigned int theIndex;
    if (GetInterfaceAddress(interfaceName, theAddress.GetType(), theAddress, &theIndex))
    {
        return theIndex;
    }
    else
    {
        DMSG(0, "ProtoSocket::GetInterfaceIndex(%s) error: no matching interface found.\n", interfaceName);
        return 0;
    }
}  // end ProtoSocket::GetInterfaceIndex()

bool ProtoSocket::GetInterfaceName(unsigned int index, char* buffer, unsigned int buflen)
{
    ULONG bufferLength = 0;
    if (ERROR_BUFFER_OVERFLOW == GetAdaptersInfo(NULL, &bufferLength))  
    {
        char* infoBuffer = new char[bufferLength];
        if (NULL == infoBuffer)
        {
            DMSG(0, "ProtoSocket::GetInterfaceName(by index) new infoBuffer error: %s\n", ::GetErrorString());
            return false;
        }
        IP_ADAPTER_INFO* adapterInfo = (IP_ADAPTER_INFO*)infoBuffer; 
        if (NO_ERROR != GetAdaptersInfo(adapterInfo, &bufferLength))
        {       
            DMSG(0, "ProtoSocket::GetInterfaceName(by index) GetAdaptersInfo() error: %s\n", ::GetErrorString());
            delete[] infoBuffer;
            return false;
        }
        while (NULL != adapterInfo)
        {
            if (index == adapterInfo->Index)
            {
                buflen = (buflen < MAX_ADAPTER_NAME_LENGTH + 4) ? buflen : (MAX_ADAPTER_NAME_LENGTH + 4);
                strncpy(buffer, adapterInfo->AdapterName, buflen);
                delete[] infoBuffer;
                return true;
            }
            adapterInfo = adapterInfo->Next;
        }
        // Assume index==1 is loopback?
        if (1 == index)
        {
            strncpy(buffer, "lo", buflen);
            return true;
        }
        else
        {
            DMSG(0, "ProtoSocket::GetInterfaceName(by index) no matching interface found!\n");
            return false;
        }
    }
    else if (0 != index)
    {
        // This should work on any Win32 that doesn't support GetAdaptersAddresses() ?? 
        MIB_IFROW ifRow;
        ifRow.dwIndex = index;
        if (NO_ERROR == GetIfEntry(&ifRow))
        {
            ProtoAddress temp;
            temp.SetRawHostAddress(ProtoAddress::ETH, (char*)ifRow.bPhysAddr, 6);
            // We use the "bDescr" field because the "wszName" field doesn't seem to work
#ifdef _UNICODE
            buflen = buflen < MAX_INTERFACE_NAME_LEN ? buflen : MAX_INTERFACE_NAME_LEN;
            wcstombs(buffer, ifRow.wszName, buflen);
#else
            buflen = buflen < ifRow.dwDescrLen ? buflen : ifRow.dwDescrLen;
            strncpy(buffer, (char*)ifRow.bDescr, buflen);
#endif // if/else _UNICODE
            return true;
        }
        else
        {
            DMSG(0, "ProtoSocket::GetInterfaceName(by index) GetIfEntry(%d) error: %s\n", index, ::GetErrorString());
            return false;
        }
    }
    else
    {
        DMSG(0, "ProtoSocket::GetInterfaceName(%d) error: invalid index\n", index);
        return false;
    }
}  // end ProtoSocket::GetInterfaceName(by index)

bool ProtoSocket::GetInterfaceName(const ProtoAddress& ifAddr, char* buffer, unsigned int buflen)
{
    /* (TBD) Do the approaches below provide the loopback address?
    if (ifAddr.IsLoopback())
    {
        strncpy(buffer, "lo", buflen);
        return true;
    }*/
   
    // Two different approaches are given here:
    // 1) Supports IPv4 and IPv6 on newer Windows Operating systems (WinXP and Win2003)
    //    using the "GetAdaptersAddresses()" call, and
    // 2) Supports IPv4 only on older operating systems using "GetIPaddrTable()" 
    
    // Try the "GetAdaptersAddresses()" approach first
    ULONG afFamily = AF_UNSPEC;
    switch (ifAddr.GetType())
    {
        case ProtoAddress::IPv4:
            afFamily = AF_INET;
            break;
#ifdef HAVE_IPV6
        case ProtoAddress::IPv6:
            afFamily = AF_INET6;
            break;
#endif // HAVE_IPV6
        default:
            break;
    }
    ULONG bufferLength = 0;
#if (WINVER >= 0x0501)
    // On NT4 and earlier, GetAdaptersAddresses() isn't to be found
    // in the iphlpapi.dll ...
    DWORD addrFlags = GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST;
    ULONG bufferSize = 0;
    DWORD result = GetAdaptersAddresses(afFamily, addrFlags, NULL, NULL, &bufferSize);
    if ((ERROR_BUFFER_OVERFLOW == result) ||
        (ERROR_NO_DATA == result))
    {
        if (ERROR_NO_DATA == result)
        {
            DMSG(2, "ProtoSocket::GetInterfaceName(%s) error: no matching network adapters found.\n", ifAddr.GetHostString());
            return false;
        }
        char* addrBuffer = new char[bufferSize];
        if (NULL == addrBuffer)
        {
            DMSG(0, "ProtoSocket::GetInterfaceName() new addrBuffer error: %s\n", ::GetErrorString());
            return false;
        }
        IP_ADAPTER_ADDRESSES* addrEntry = (IP_ADAPTER_ADDRESSES*)addrBuffer;
        if (ERROR_SUCCESS != GetAdaptersAddresses(afFamily, addrFlags, NULL, addrEntry, &bufferSize))
        {
            DMSG(0, "ProtoSocket::GetInterfaceName() GetAdaptersAddresses() error: %s\n", ::GetErrorString());
            delete[] addrBuffer;
            return false;
        }
        while (NULL != addrEntry)
        {
            if (ProtoAddress::ETH == ifAddr.GetType())
            {
                if (6 == addrEntry->PhysicalAddressLength)
                {
                    ProtoAddress tempAddress;
                    tempAddress.SetRawHostAddress(ProtoAddress::ETH, (char*)&addrEntry->PhysicalAddress, 6);
                    if (tempAddress.HostIsEqual(ifAddr))
                    {
                        // Copy the interface name
                        buflen = buflen < MAX_INTERFACE_NAME_LEN ? buflen : MAX_INTERFACE_NAME_LEN;
                        strncpy(buffer, addrEntry->AdapterName, buflen);
                        delete[] addrBuffer;
                        return true;
                    }
                }
            }
            else
            {
                IP_ADAPTER_UNICAST_ADDRESS* ipAddr = addrEntry->FirstUnicastAddress;
                while(NULL != ipAddr)
                {
                    if (afFamily == ipAddr->Address.lpSockaddr->sa_family)
                    {
                        ProtoAddress tempAddress;
                        tempAddress.SetSockAddr(*(ipAddr->Address.lpSockaddr));
                        if (tempAddress.HostIsEqual(ifAddr))
                        {
                            buflen = buflen < MAX_INTERFACE_NAME_LEN ? buflen : MAX_INTERFACE_NAME_LEN;
                            strncpy(buffer, addrEntry->AdapterName, buflen);\
                            delete[] addrBuffer;
                            return true;
                        }
                    }
                    ipAddr = ipAddr->Next;
                }
                if (NULL != ipAddr) break;
            }
            addrEntry = addrEntry->Next;
        }  // end while(addrEntry)
        delete[] addrBuffer;
        DMSG(4, "ProtoSocket::GetInterfaceName(%s) warning: no matching interface found\n", ifAddr.GetHostString());
    }
    else 
#endif // if (WINVER >= 0x0501)
    if (ERROR_BUFFER_OVERFLOW == GetAdaptersInfo(NULL, &bufferLength))
    {
        char* infoBuffer = new char[bufferLength];
        if (NULL == infoBuffer)
        {
            DMSG(0, "ProtoSocket::GetInterfaceName(%s) new infoBuffer error: %s\n", ifAddr.GetHostString(), ::GetErrorString());
            return false;
        }
        IP_ADAPTER_INFO* adapterInfo = (IP_ADAPTER_INFO*)infoBuffer; 
        if (NO_ERROR != GetAdaptersInfo(adapterInfo, &bufferLength))
        {       
            DMSG(0, "ProtoSocket::GetInterfaceName(%s) GetAdaptersInfo() error: %s\n", ifAddr.GetHostString(), ::GetErrorString());
            delete[] infoBuffer;
            return false;
        }
        while (NULL != adapterInfo)
        {
            ProtoAddress tempAddr;
            tempAddr.Invalidate();
            if (ProtoAddress::ETH == ifAddr.GetType())
            {
                if (6 == adapterInfo->AddressLength)
                    tempAddr.SetRawHostAddress(ProtoAddress::ETH, (char*)adapterInfo->Address, 6);
            }
            else if (ProtoAddress::IPv4 == ifAddr.GetType())
            {
                tempAddr.ResolveFromString(adapterInfo->IpAddressList.IpAddress.String);
            }
            if (tempAddr.IsValid() && tempAddr.HostIsEqual(ifAddr))
            {
                buflen = buflen > (MAX_ADAPTER_NAME_LENGTH+4) ? (MAX_ADAPTER_NAME_LENGTH+4) : buflen;
                strncpy(buffer, adapterInfo->AdapterName, buflen);
                delete[] infoBuffer;
                return true;
            }
            adapterInfo = adapterInfo->Next;
        }
        delete[] infoBuffer;
        DMSG(0, "ProtoSocket::GetInterfaceName(%s) error: no matching interface found\n", ifAddr.GetHostString());
    }
    else if (ProtoAddress::ETH == ifAddr.GetType())
    {
        DWORD ifCount;
        if (NO_ERROR != GetNumberOfInterfaces(&ifCount))
        {
            DMSG(0, "ProtoSocket::GetInterfaceName() GetNumberOfInterfaces() error: %s\n", ::GetErrorString());
            return false;
        }
        for (DWORD i = 1; i <= ifCount; i++)
        {
            MIB_IFROW ifRow;
            ifRow.dwIndex = i;
            if (NO_ERROR != GetIfEntry(&ifRow))
            {
                DMSG(4, "ProtoSocket::GetInterfaceName() GetIfEntry(%d) error: %s\n", i, ::GetErrorString());
                continue;
            }
            if (6 == ifRow.dwPhysAddrLen)
            {
                ProtoAddress tempAddress;
                tempAddress.SetRawHostAddress(ProtoAddress::ETH, (char*)ifRow.bPhysAddr, 6);
                if (tempAddress.HostIsEqual(ifAddr))
                {
                    // We use the "bDescr" field because the "wszName" field doesn't seem to work
#ifdef _UNICODE
                    buflen = buflen < MAX_INTERFACE_NAME_LEN ? buflen : MAX_INTERFACE_NAME_LEN;
                    wcstombs(buffer, ifRow.wszName, buflen);
#else
                    buflen = buflen < ifRow.dwDescrLen ? buflen : ifRow.dwDescrLen;
                    strncpy(buffer, (char*)ifRow.bDescr, buflen);
#endif // if/else _UNICODE
                    return true;
                }
            }
        }
        DMSG(4, "ProtoSocket::GetInterfaceName(%s) GetIfEntry() error: no matching Ethernet interface found\n", 
                ifAddr.GetHostString());
    }
    else if (ProtoAddress::IPv4 == ifAddr.GetType())
    {
        DMSG(4, "ProtoSocket::GetInterfaceName() warning GetAdaptersAddresses() error: %s\n", ::GetErrorString());
        // Since GetAdaptersAddresses() failed, try the other approach iff IPv4 == addressType
        // Iterate through addresses looking for an address match
        ULONG bufferSize = 0;
        if (ERROR_INSUFFICIENT_BUFFER == GetIpAddrTable(NULL, &bufferSize, FALSE))
        {
            char* tableBuffer = new char[bufferSize];
            if (NULL == tableBuffer)
            {   
                DMSG(0, "ProtoSocket::GetInterfaceName() new tableBuffer error: %s\n", ::GetErrorString());
                return false;
            }
            MIB_IPADDRTABLE* addrTable = (MIB_IPADDRTABLE*)tableBuffer;
            if (ERROR_SUCCESS == GetIpAddrTable(addrTable, &bufferSize, FALSE))
            {
                for (DWORD i = 0; i < addrTable->dwNumEntries; i++)
                {
                    MIB_IPADDRROW* entry = &(addrTable->table[i]);
                    ProtoAddress tempAddress;
                    tempAddress.SetRawHostAddress(ProtoAddress::IPv4, (char*)&entry->dwAddr, 4);
                    if (tempAddress.HostIsEqual(ifAddr))
                    {
                        MIB_IFROW ifEntry;  
                        ifEntry.dwIndex = entry->dwIndex;
                        if (NO_ERROR != GetIfEntry(&ifEntry))
                        {   
                            DMSG(0, "ProtoSocket::GetInterfaceName() GetIfEntry(%d) error: %s\n", i, ::GetErrorString());
                            return false;
                        }
                        // We use the "bDescr" field because the "wszName" field doesn't seem to work
#ifdef _UNICODE
                        buflen = buflen < MAX_INTERFACE_NAME_LEN ? buflen : MAX_INTERFACE_NAME_LEN;
                        wcstombs(buffer, ifEntry.wszName, buflen);
#else
                        buflen = buflen < ifEntry.dwDescrLen ? buflen : ifEntry.dwDescrLen;
                        strncpy(buffer, (char*)ifEntry.bDescr, buflen);
#endif // if/else _UNICODE
                        delete[] tableBuffer;
                        return true;
                    }
                }
            }
            else
            {
                DMSG(4, "ProtoSocket::GetInterfaceName(%s) warning GetIpAddrTable() error: %s\n", ifAddr.GetHostString(), ::GetErrorString());
            }
            delete[] tableBuffer;
        }
        else
        {
            DMSG(0, "ProtoSocket::GetInterfaceName(%s) warning GetIpAddrTable() error 2: %s\n", ifAddr.GetHostString(), ::GetErrorString());            
        }
        DMSG(4, "ProtoSocket::GetInterfaceName(%s) warning: no matching IPv4 interface found\n",
                ifAddr.GetHostString());
    }
    else
    {
        DMSG(4, "ProtoSocket::GetInterfaceName() warning GetAdaptersAddresses() error: %s\n", ::GetErrorString());
    }
    return false;
}  // end ProtoSocket::GetInterfaceName(by addr)


#else  // UNIX

// Helper functions for group joins & leaves
// NOTE: On Linux, this works only for IPv4 addresses (TBD) Fix this!
bool ProtoSocket::GetInterfaceAddressList(const char*         interfaceName,
                                          ProtoAddress::Type  addressType,
                                          ProtoAddress::List& addrList,
                                          unsigned int*       interfaceIndex)
{
    struct ifreq req;
    memset(&req, 0, sizeof(struct ifreq));
    strncpy(req.ifr_name, interfaceName, IFNAMSIZ);
    int socketFd = -1;
    switch (addressType)
    {
        case ProtoAddress::IPv4:
            req.ifr_addr.sa_family = AF_INET;
            socketFd = socket(PF_INET, SOCK_DGRAM, 0);
            break;
#ifdef HAVE_IPV6
        case ProtoAddress::IPv6:
            req.ifr_addr.sa_family = AF_INET6;
            socketFd = socket(PF_INET6, SOCK_DGRAM, 0);
            break;
#endif // HAVE_IPV6
        default:
            req.ifr_addr.sa_family = AF_UNSPEC;
            socketFd = socket(PF_INET, SOCK_DGRAM, 0);
            break;
    }
    
    if (socketFd < 0)
    {
        DMSG(0, "ProtoSocket::GetInterfaceAddressList() socket() error: %s\n", GetErrorString()); 
        return false;
    }   
    
    if (ProtoAddress::ETH == addressType)
    {
#ifdef SIOCGIFHWADDR
        // Probably Linux
        // Get hardware (MAC) address instead of IP address
        if (ioctl(socketFd, SIOCGIFHWADDR, &req) < 0)
        {
            DMSG(0, "ProtoSocket::GetInterfaceAddressList() ioctl(SIOCGIFHWADDR) error: %s\n",
                    GetErrorString());
            close(socketFd);
            return false;   
        }  
        else
        {
            close(socketFd);
            if (NULL != interfaceIndex) *interfaceIndex = req.ifr_ifindex;
            ProtoAddress ethAddr;
            if (!ethAddr.SetRawHostAddress(ProtoAddress::ETH,
                                           (const char*)&req.ifr_hwaddr.sa_data,
                                           IFHWADDRLEN))
            {
                DMSG(0, "ProtoSocket::GetInterfaceAddressList() error: invalid ETH addr?\n");
                return false;
            }   
            if (!addrList.Insert(ethAddr))
            {
                DMSG(0, "ProtoSocket::GetInterfaceAddressList() error: unable to add ETH addr to list.\n");
                return false;
            }
            return true;            
        }
#else
#if defined(SOLARIS) || defined(IRIX)
        // Use DLPI instead
        close(socketFd);
        char deviceName[32];
        snprintf(deviceName, 32, "/dev/%s", interfaceName);
        char* ptr = strpbrk(deviceName, "0123456789");
        if (NULL == ptr)
        {
            DMSG(0, "ProtoSocket::GetInterfaceAddressList() invalid interface\n");
            return false;
        }
        int ifNumber = atoi(ptr);
        *ptr = '\0';    
        if ((socketFd = open(deviceName, O_RDWR)) < 0)
        {
            DMSG(0, "ProtoSocket::GetInterfaceAddressList() dlpi open() error: %s\n",
                    GetErrorString());
            return false;
        }
        // dlp device opened, now bind to specific interface
        UINT32 buffer[8192];
        union DL_primitives* dlp = (union DL_primitives*)buffer;
        dlp->info_req.dl_primitive = DL_INFO_REQ;
        struct strbuf msg;
        msg.maxlen = 0;
        msg.len = DL_INFO_REQ_SIZE;
        msg.buf = (caddr_t)dlp;
        if (putmsg(socketFd, &msg, NULL, RS_HIPRI) < 0)
        {
            DMSG(0, "ProtoSocket::GetInterfaceAddressList() dlpi putmsg(1) error: %s\n",
                    GetErrorString());
            close(socketFd);
            return false;
        }
        msg.maxlen = 8192;
        msg.len = 0;
        int flags = 0;
        if (getmsg(socketFd, &msg, NULL, &flags) < 0)
        {
            DMSG(0, "ProtoSocket::GetInterfaceAddressList() dlpi getmsg(1) error: %s\n",
                    GetErrorString());
            close(socketFd);
            return false;
        }
        if ((DL_INFO_ACK != dlp->dl_primitive) ||
            (msg.len <  (int)DL_INFO_ACK_SIZE))
        {
            DMSG(0, "ProtoSocket::GetInterfaceAddressList() dlpi getmsg(1) error: unexpected response\n");
            close(socketFd);
            return false;
        }
        if (DL_STYLE2 == dlp->info_ack.dl_provider_style)
        {
            dlp->attach_req.dl_primitive = DL_ATTACH_REQ;
            dlp->attach_req.dl_ppa = ifNumber;
            msg.maxlen = 0;
            msg.len = DL_ATTACH_REQ_SIZE;
            msg.buf = (caddr_t)dlp;
            if (putmsg(socketFd, &msg, NULL, RS_HIPRI) < 0)
            {
                DMSG(0, "ProtoSocket::GetInterfaceAddressList() dlpi putmsg(DL_ATTACH_REQ) error: %s\n",
                        GetErrorString());
                close(socketFd);
                return false;
            }
            msg.maxlen = 8192;
            msg.len = 0;
            flags = 0;
            if (getmsg(socketFd, &msg, NULL, &flags) < 0)
            {
                DMSG(0, "ProtoSocket::GetInterfaceAddressList() dlpi getmsg(DL_OK_ACK) error: %s\n",
                        GetErrorString());
                close(socketFd);
                return false;
            }
            if ((DL_OK_ACK != dlp->dl_primitive) ||
                (msg.len <  (int)DL_OK_ACK_SIZE))
            {
                DMSG(0, "ProtoSocket::GetInterfaceAddressList() dlpi getmsg(DL_OK_ACK) error: unexpected response\n");
                close(socketFd);
                return false;
            }
        }
        memset(&dlp->bind_req, 0, DL_BIND_REQ_SIZE);
        dlp->bind_req.dl_primitive = DL_BIND_REQ;
#ifdef DL_HP_RAWDLS
	    dlp->bind_req.dl_sap = 24;	
	    dlp->bind_req.dl_service_mode = DL_HP_RAWDLS;
#else
	    dlp->bind_req.dl_sap = DL_ETHER;
	    dlp->bind_req.dl_service_mode = DL_CLDLS;
#endif
        msg.maxlen = 0;
        msg.len = DL_BIND_REQ_SIZE;
        msg.buf = (caddr_t)dlp;
        if (putmsg(socketFd, &msg, NULL, RS_HIPRI) < 0)
        {
            DMSG(0, "ProtoSocket::GetInterfaceAddressList() dlpi putmsg(DL_BIND_REQ) error: %s\n",
                    GetErrorString());
            close(socketFd);
            return false;     
        }
        msg.maxlen = 8192;
        msg.len = 0;
        flags = 0;
        if (getmsg(socketFd, &msg, NULL, &flags) < 0)
        {
            DMSG(0, "ProtoSocket::GetInterfaceAddressList() dlpi getmsg(DL_BIND_ACK) error: %s\n",
                    GetErrorString());
            close(socketFd);
            return false;
        }
        if ((DL_BIND_ACK != dlp->dl_primitive) ||
            (msg.len <  (int)DL_BIND_ACK_SIZE))
        {
            DMSG(0, "ProtoSocket::GetInterfaceAddressList() dlpi getmsg(DL_BIND_ACK) error: unexpected response\n");
            close(socketFd);
            return false;
        }
        // We're bound to the interface, now request interface address
        dlp->info_req.dl_primitive = DL_INFO_REQ;
        msg.maxlen = 0;
        msg.len = DL_INFO_REQ_SIZE;
        msg.buf = (caddr_t)dlp;
        if (putmsg(socketFd, &msg, NULL, RS_HIPRI) < 0)
        {
            DMSG(0, "ProtoSocket::GetInterfaceAddressList() dlpi putmsg() error: %s\n",
                    GetErrorString());
            close(socketFd);
            return false;     
        }
        msg.maxlen = 8192;
        msg.len = 0;
        flags = 0;
        if (getmsg(socketFd, &msg, NULL, &flags) < 0)
        {
            DMSG(0, "ProtoSocket::GetInterfaceAddressList() dlpi getmsg() error: %s\n",
                    GetErrorString());
            close(socketFd);
            return false;
        }
        if ((DL_INFO_ACK != dlp->dl_primitive) || (msg.len <  (int)DL_INFO_ACK_SIZE))
        {
            DMSG(0, "ProtoSocket::GetInterfaceAddressList() dlpi getmsg() error: unexpected response\n");
            close(socketFd);
            return false;
        }
        ProtoAddress macAddr;
        macAddr.SetRawHostAddress(addressType, (char*)(buffer + dlp->physaddr_ack.dl_addr_offset), 6);
        if (NULL != interfaceIndex) *interfaceIndex = ifNumber;
        close(socketFd);
        if (!addrList.Insert(macAddr))
        {
            DMSG(0, "ProtoSocket::GetInterfaceAddressList() error: unable to add ETH addr to list.\n");
            return false; 
        }
        return true;
#else
        // For now, assume we're BSD and use getifaddrs()
        close(socketFd);  // don't need the socket
        struct ifaddrs* ifap;
        if (0 == getifaddrs(&ifap))
        {
            // TBD - Look for AF_LINK address for given "interfaceName"
            struct ifaddrs* ptr = ifap;
            while (ptr)
            {
                if (ptr->ifa_addr && (AF_LINK == ptr->ifa_addr->sa_family))
                {
                    if (!strcmp(interfaceName, ptr->ifa_name))
                    {
                        // (TBD) should we confirm sdl->sdl_type == IFT_ETHER?
                        struct sockaddr_dl* sdl = (struct sockaddr_dl*)ptr->ifa_addr;
                        if (IFT_ETHER != sdl->sdl_type)
                        {
                            freeifaddrs(ifap);
                            DMSG(0, "ProtoSocket::GetInterfaceAddressList() error: non-Ethertype iface\n");
                            return false;
                        }
                        ProtoAddress macAddr;
                        macAddr.SetRawHostAddress(addressType, 
                                                  sdl->sdl_data + sdl->sdl_nlen,
                                                  sdl->sdl_alen);
                        if (NULL != interfaceIndex) 
                            *interfaceIndex = sdl->sdl_index;
                        freeifaddrs(ifap);
                        if (!addrList.Insert(macAddr))
                        {
                            DMSG(0, "ProtoSocket::GetInterfaceAddressList() error: unable to add ETH addr to list.\n");
                            return false; 
                        }
                        return true;
                    }
                }   
                ptr = ptr->ifa_next;
            }
            freeifaddrs(ifap);
            DMSG(0, "ProtoSocket::GetInterfaceAddressList() unknown interface name\n");
            return false; // change to true when implemented
        }
        else
        {
            DMSG(0, "ProtoSocket::GetInterfaceAddressList() getifaddrs() error: %s\n",
                    GetErrorString());
            return false;  
        }
#endif // if/else SOLARIS
#endif // if/else SIOCGIFHWADDR
    }  // end if (ETH == addrType)
    if (ioctl(socketFd, SIOCGIFADDR, &req) < 0)
    {
        close(socketFd);
        // Perhaps "interfaceName" is an address string?
        ProtoAddress ifAddr;
        if (ifAddr.ResolveFromString(interfaceName))
        {
            char nameBuffer[IFNAMSIZ+1];
            if (GetInterfaceName(ifAddr, nameBuffer, IFNAMSIZ+1))
            {
                return GetInterfaceAddressList(nameBuffer, addressType, addrList, interfaceIndex);
            }
            else
            {
                DMSG(0, "ProtoSocket::GetInterfaceAddressList() error: unknown interface address\n");
                return false;
            }
        }
        DMSG(0, "ProtoSocket::GetInterfaceAddressList() ioctl(SIOCGIFADDR) error: %s\n",
                GetErrorString()); 
        return false; 
    }
    close(socketFd);
    if (NULL != interfaceIndex) 
        *interfaceIndex = GetInterfaceIndex(req.ifr_name);
    ProtoAddress ifAddr;
#ifdef MACOSX
    // (TBD) make this more general somehow???
    if (0 == req.ifr_addr.sa_len)
    {
        DMSG(0, "ProtoSocket::GetInterfaceAddressList() warning: no addresses for given family?\n");
        return false;
    }
    else 
#endif // MACOSX
    if (ifAddr.SetSockAddr(req.ifr_addr))
    {
        if (addrList.Insert(ifAddr))
        {
            return true;
        }
        else
        {
            DMSG(0, "ProtoSocket::GetInterfaceAddressList() error: unable to add ifAddr to list\n");
            return false;
        }
    }
    else
    {
        DMSG(0, "ProtoSocket::GetInterfaceAddressList() error: invalid address family\n");
        return false;
    }
}  // end ProtoSocket::GetInterfaceAddressList()

unsigned int ProtoSocket::GetInterfaceIndex(const char* interfaceName)
{
    struct if_nameindex* ifdx = if_nameindex();
    unsigned int index = 0;
    struct if_nameindex* ptr = ifdx;
    if (ptr)
    {
        while (0 != ptr->if_index)
        {
            if (!strncmp(ptr->if_name, interfaceName, IFNAMSIZ))
            {
                index = ptr->if_index;
                break;  
            }
            ptr++;
        } 
    }
    if_freenameindex(ifdx);
    if (0 == index)
    {
        // Perhaps "interfaceName" was an address string;
        ProtoAddress ifAddr;
        if (ifAddr.ResolveFromString(interfaceName))
        {
            char nameBuffer[IFNAMSIZ+1]; 
            if (GetInterfaceName(ifAddr, nameBuffer, IFNAMSIZ+1))
                return GetInterfaceIndex(nameBuffer);
        }
    }
    return index;
}  // end ProtoSocket::GetInterfaceIndex()

int ProtoSocket::GetInterfaceIndices(int* indexArray, unsigned int indexArraySize)
{
    int indexCount = 0;
    struct if_nameindex* ifdx = if_nameindex();
    struct if_nameindex* ptr = ifdx;
    if (ptr)
    {
        while (0 != ptr->if_index)
        {
            if ((unsigned int)indexCount < indexArraySize)
                indexArray[indexCount] = ptr->if_index;
            indexCount++;         
            ptr++;
        } 
        if_freenameindex(ifdx);
    }
    return indexCount;
}  // end ProtoSocket::GetInterfaceIndices()

bool ProtoSocket::FindLocalAddress(ProtoAddress::Type addrType, ProtoAddress& theAddress)
{
    struct if_nameindex* ifdx = if_nameindex();
    struct if_nameindex* ptr = ifdx;
    if (ptr)
    {
        while (0 != ptr->if_index)
        {
            if (GetInterfaceAddress(ptr->if_name, addrType, theAddress))
            {
                if (!theAddress.IsLoopback())
                {
                    if_freenameindex(ifdx);
                    return true;      
                }
            }            
            ptr++;
        } 
        if_freenameindex(ifdx);
    }
    // (TBD) set loopback addr if nothing else?
    DMSG(0, "ProtoSocket::FindLocalAddress() no %s address assigned\n", 
            (ProtoAddress::IPv6 == addrType) ? "IPv6" : "IPv4");
    return false;
}  // end ProtoSocket::FindLocalAddress()

bool ProtoSocket::GetInterfaceName(unsigned int index, char* buffer, unsigned int buflen)
{
    bool result = false;
    struct if_nameindex* ifdx = if_nameindex();
    struct if_nameindex* ptr = ifdx;
    if (ptr)
    {
        while (0 != ptr->if_index)
        {
            if (index == ptr->if_index)
            {
                strncpy(buffer, ptr->if_name, buflen);
                result = true;
                break;
            }
            ptr++;
        } 
        if_freenameindex(ifdx);
    }
    return result;
}  // end ProtoSocket::GetInterfaceName(by index)

bool ProtoSocket::GetInterfaceName(const ProtoAddress& ifAddr, char* buffer, unsigned int buflen)
{   
    // Go through list, looking for matching address
    bool result = false;
    struct if_nameindex* ifdx = if_nameindex();
    struct if_nameindex* ptr = ifdx;
    while (0 != ptr->if_index)
    {
        if (0 != ptr->if_index)
        {
            ProtoAddress theAddress;
            if (GetInterfaceAddress(ptr->if_name, ifAddr.GetType(), theAddress))
            {
                if (ifAddr.HostIsEqual(theAddress))
                {
                    strncpy(buffer, ptr->if_name, buflen);
                    result = true;
                    break;
                }   
            }    
        }
        ptr++;
    }
    if_freenameindex(ifdx);
    return result;
}  // end  ProtoSocket::GetInterfaceName(by address)

#endif  // if/else WIN32/UNIX


bool ProtoSocket::LeaveGroup(const ProtoAddress& groupAddress,
                             const char*         interfaceName)
{    
    if (!IsOpen()) return true;
    int result;
#ifdef HAVE_IPV6
    if (ProtoAddress::IPv6 == groupAddress.GetType())
    {
        if (IN6_IS_ADDR_V4MAPPED(&((struct sockaddr_in6*)&groupAddress.GetSockAddrStorage())->sin6_addr))
        {
            struct ip_mreq mreq;
            mreq.imr_multiaddr.s_addr = 
            IN6_V4MAPPED_ADDR(&(((struct sockaddr_in6*)&groupAddress.GetSockAddrStorage())->sin6_addr));
            if (interfaceName)
            {
                ProtoAddress interfaceAddress;
                if (GetInterfaceAddress(interfaceName, ProtoAddress::IPv4, interfaceAddress))
                {
                    mreq.imr_interface.s_addr = htonl(interfaceAddress.IPv4GetAddress());
                }
                else
                {
                    DMSG(0, "ProtoSocket::LeaveGroup() invalid interface name\n");
                    return false;
                }
            }
            else
            {
                mreq.imr_interface.s_addr = INADDR_ANY; 
            }
            result = setsockopt(handle, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char*)&mreq, sizeof(mreq));
        }
        else
        {
            struct ipv6_mreq mreq;
            mreq.ipv6mr_multiaddr = ((struct sockaddr_in6*)&groupAddress.GetSockAddrStorage())->sin6_addr;
            if (interfaceName)
                mreq.ipv6mr_interface = GetInterfaceIndex(interfaceName);
            else
                mreq.ipv6mr_interface = 0;
            result = setsockopt(handle, IPPROTO_IPV6, IPV6_DROP_MEMBERSHIP, (char*)&mreq, sizeof(mreq));
        }
    }
    else
#endif //HAVE_IPV6
    {
        struct ip_mreq mreq;
#ifdef HAVE_IPV6
        mreq.imr_multiaddr = ((struct sockaddr_in*)&groupAddress.GetSockAddrStorage())->sin_addr;
#else
        mreq.imr_multiaddr = ((struct sockaddr_in*)&groupAddress.GetSockAddr())->sin_addr;
#endif  // end if/else HAVE_IPV6
        if (interfaceName)
        {
            ProtoAddress interfaceAddress;
            if (GetInterfaceAddress(interfaceName, ProtoAddress::IPv4, 
                                    interfaceAddress))
            {
                mreq.imr_interface.s_addr = htonl(interfaceAddress.IPv4GetAddress());
            }
            else
            {
                DMSG(0, "ProtoSocket::LeaveGroup() invalid interface name\n");
                return false;
            }
        }
        else
        {
                mreq.imr_interface.s_addr = INADDR_ANY; 
        }
        result = setsockopt(handle, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char*)&mreq, sizeof(mreq));
    }
    if (result < 0)
    {
        DMSG(0, "ProtoSocket::LeaveGroup() error leaving multicast group: %s\n", GetErrorString());
        return false;
    }
    else
    {
        return true;
    }
}  // end ProtoSocket::LeaveGroup() 

bool ProtoSocket::SetTTL(unsigned char ttl)
{   
#if defined(WIN32) && !defined(_WIN32_WCE)
    DWORD dwTTL = (DWORD)ttl; 
    DWORD dwBytesXfer;
    if (WSAIoctl(handle, SIO_MULTICAST_SCOPE, &dwTTL, sizeof(dwTTL),
                 NULL, 0, &dwBytesXfer, NULL, NULL))
#else
    int result;
#ifdef HAVE_IPV6
    if (IPv6 == domain)
    {
        unsigned int hops = (unsigned int) ttl;
        result = setsockopt(handle, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, 
                            &hops, sizeof(hops));
    }
    else
#endif // HAVE_IPV6
    {
#ifdef _WIN32_WCE
        int hops = (int)ttl;
#else
        char hops = (char)ttl;
#endif // if/else _WIN32_WCE/UNIX
        result = setsockopt(handle, IPPROTO_IP, IP_MULTICAST_TTL, 
                            (char*)&hops, sizeof(hops));
    }
    if (result < 0) 
#endif // if/else WIN32/UNIX | _WIN32_WCE
    { 
        DMSG(0, "ProtoSocket: setsockopt(IP_MULTICAST_TTL) error: %s\n", GetErrorString()); 
        return false;
    }
    else
    {   
        return true;
    }
}  // end ProtoSocket::SetTTL()

bool ProtoSocket::SetTOS(unsigned char tos)
{ 
#ifdef NEVER_EVER // (for older LINUX?)                  
   int precedence = IPTOS_PREC(tos);
   if (setsockopt(handle, SOL_SOCKET, SO_PRIORITY, (char*)&precedence, sizeof(precedence)) < 0)           
   {     
       DMSG(0, "ProtoSocket: setsockopt(SO_PRIORITY) error: %s\n", GetErrorString()); 
       return false;
    }                          
   int tos_bits = IPTOS_TOS(tos);
   if (setsockopt(handle, SOL_IP, IP_TOS, (char*)&tos_bits, sizeof(tos_bits)) < 0) 
#else
    int theTOS = tos;
    int result;
#ifdef HAVE_IPV6
    if (IPv6 == domain)
        result = setsockopt(handle, IPPROTO_IPV6, IP_TOS, (char*)&theTOS, sizeof(theTOS)); 
    else 
#endif // HAVE_IPV6  
        result =  setsockopt(handle, IPPROTO_IP, IP_TOS, (char*)&theTOS, sizeof(theTOS));    
#endif  // if/else NEVER_EVER
    if (result < 0)
    {               
        DMSG(0, "ProtoSocket: setsockopt(IP_TOS) error\n");
        return false;
    }
    return true; 
}  // end ProtoSocket::SetTOS()

bool ProtoSocket::SetBroadcast(bool broadcast)
{
#ifdef SO_BROADCAST
#ifdef WIN32
    BOOL state = broadcast ? TRUE : FALSE;
#else
    int state = broadcast ? 1 : 0;
#endif // if/else WIN32
    if (setsockopt(handle, SOL_SOCKET, SO_BROADCAST, (char*)&state, sizeof(state)) < 0)
    {
        DMSG(0, "ProtoSocket::SetBroadcast(): setsockopt(SO_BROADCAST) error: %s\n",
                GetErrorString());
        return false;
    }
#endif // SO_BROADCAST
    return true;
}  // end ProtoSocket::SetBroadcast()


bool ProtoSocket::SetLoopback(bool loopback)
{
    ASSERT(IsOpen());
#ifdef WIN32
    DWORD dwLoop = loopback ? TRUE: FALSE;
    DWORD dwBytesXfer;
    if (WSAIoctl(handle, SIO_MULTIPOINT_LOOPBACK , &dwLoop, sizeof(dwLoop),
                 NULL, 0, &dwBytesXfer, NULL, NULL))
#else 
    int result;
    char loop = loopback ? 1 : 0;
#ifdef HAVE_IPV6
    unsigned int loop6 = loopback ? 1 : 0;  // this is needed at least for FreeBSD
    if (IPv6 == domain)
        result = setsockopt(handle, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, 
                            (char*)&loop6, sizeof(loop6));
    else
#endif // HAVE_IPV6
    result = setsockopt(handle, IPPROTO_IP, IP_MULTICAST_LOOP, 
                        (char*)&loop, sizeof(loop));
    if (result < 0)
#endif // if/else WIN32
    {
#ifdef WIN32
        // On NT, this the SIO_MULTIPOINT_LOOPBACK always seems to return an error
        // so we'll ignore the error and return success on NT4 and earlier
        OSVERSIONINFO vinfo;
        vinfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
        GetVersionEx(&vinfo);
        if ((VER_PLATFORM_WIN32_NT == vinfo.dwPlatformId) &&
            ((vinfo.dwMajorVersion < 4) ||
                ((vinfo.dwMajorVersion == 4) && (vinfo.dwMinorVersion == 0))))
                    return true;
#endif // WIN32
        DMSG(0, "ProtoSocket: setsockopt(IP_MULTICAST_LOOP) error: %s\n", GetErrorString());
        return false;
    } 
    else
    {
        return true;
    }
}  // end ProtoSocket::SetLoopback() 

bool ProtoSocket::SetMulticastInterface(const char* interfaceName)
{    
    if (interfaceName)
    {
        int result;
#ifdef HAVE_IPV6
        if (IPv6 == domain)  
        {
#ifdef WIN32
            // (TBD) figure out Win32 IPv6 multicast interface
            DMSG(0, "ProtoSocket::SetMulticastInterface() not yet supported for IPv6 on WIN32?!\n");
            return false;
#else
            unsigned int interfaceIndex = GetInterfaceIndex(interfaceName);
            result = setsockopt(handle, IPPROTO_IPV6, IPV6_MULTICAST_IF,
                                (char*)&interfaceIndex, sizeof(interfaceIndex));
#endif // if/else WIN32
        }
        else 
#endif // HAVE_IPV6 
        {  
            struct in_addr localAddr;
            ProtoAddress interfaceAddress;
#if defined(WIN32) && (WINVER < 0x0500)
            // First check to see if "interfaceName" is the IP address on older Win32 versions
            // since there seem to be issues with iphlpapi.lib (and hence GetInterfaceAddress()) on those platforms
            if (interfaceAddress.ResolveFromString(interfaceName))
            {
                localAddr.s_addr = htonl(interfaceAddress.IPv4GetAddress());
            }
            else if (GetInterfaceAddress(interfaceName, ProtoAddress::IPv4, interfaceAddress))
#else
            if (GetInterfaceAddress(interfaceName, ProtoAddress::IPv4, interfaceAddress))
#endif // if/else WIN32 &&  (WINVER < 0x0500)
			{
                localAddr.s_addr = htonl(interfaceAddress.IPv4GetAddress());
            }
            else
            {
                DMSG(0, "ProtoSocket::SetMulticastInterface() invalid interface name\n");
                return false;
            }
            result = setsockopt(handle, IPPROTO_IP, IP_MULTICAST_IF, (char*)&localAddr, 
                                sizeof(localAddr));
        }
        if (result < 0)
        { 
            DMSG(0, "ProtoSocket: setsockopt(IP_MULTICAST_IF) error: %s\n", 
                     GetErrorString());
            return false;
        }         
    }     
    return true;
}  // end ProtoSocket::SetMulticastInterface()

bool ProtoSocket::SetReuse(bool state)
{
#if defined(SO_REUSEADDR) || defined(SO_REUSEPORT)
    bool success = true;
#else
    return false;
#endif
    int reuse = state ? 1 : 0;
#ifdef SO_REUSEADDR
#ifdef WIN32
    BOOL breuse = (BOOL)reuse;
    if (setsockopt(handle, SOL_SOCKET, SO_REUSEADDR, (char*)&breuse, sizeof(breuse)) < 0)
#else
    if (setsockopt(handle, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse)) < 0)
#endif // if/else WIN32
    {
        DMSG(0, "ProtoSocket: setsockopt(REUSE_ADDR) error: %s\n", GetErrorString());
        success = false;
    }
#endif // SO_REUSEADDR            
            
#ifdef SO_REUSEPORT  // not defined on Linux for some reason?
#ifdef WIN32
    BOOL breuse = (BOOL)reuse;
    if (setsockopt(handle, SOL_SOCKET, SO_REUSEPORT, (char*)&breuse, sizeof(i_reuse)) < 0)
#else
    if (setsockopt(handle, SOL_SOCKET, SO_REUSEPORT, (char*)&reuse, sizeof(reuse)) < 0)
#endif // if/else WIN32
    {
        DMSG(0, "ProtoSocket: setsockopt(SO_REUSEPORT) error: %s\n", GetErrorString());
        success = false;
    }
#endif // SO_REUSEPORT
    return success;
}  // end ProtoSocketError::SetReuse()

#ifdef HAVE_IPV6
bool ProtoSocket::HostIsIPv6Capable()
{
#ifdef WIN32
    if (!ProtoAddress::Win32Startup())
    {
        DMSG(0, "ProtoSocket::HostIsIPv6Capable() WSAStartup() error: %s\n", GetErrorString());
        return false;
    }
    SOCKET handle = socket(AF_INET6, SOCK_DGRAM, 0);
    closesocket(handle);
    ProtoAddress::Win32Cleanup();
    if(INVALID_SOCKET == handle)
        return false;
    else
        return true;
#else
#ifdef RES_USE_INET6
    if (0 == (_res.options & RES_INIT)) res_init();
    if (0 == (_res.options & RES_USE_INET6))
        return false;
    else
#endif // RES_USE_INET6
        return true;
#endif  // if/else WIN32
}  // end ProtoSocket::HostIsIPv6Capable()

bool ProtoSocket::SetHostIPv6Capable()
{
#ifdef WIN32
    if (!ProtoAddress::Win32Startup())
    {
        DMSG(0, "ProtoSocket::SetHostIPv6Capable() WSAStartup() error: %s\n", GetErrorString());
        return false;
    }
    SOCKET handle = socket(AF_INET6, SOCK_DGRAM, 0);
    closesocket(handle);
    ProtoAddress::Win32Cleanup();
    if(INVALID_SOCKET == handle)
        return false;
    else
        return true;
#else
#ifdef RES_USE_INET6
    if (0 == (_res.options & RES_INIT)) 
        res_init();
    if (0 == (_res.options & RES_USE_INET6)) 
        _res.options |= RES_USE_INET6; 
    if (0 == (_res.options & RES_USE_INET6))
        return false;
    else
#endif // RES_USE_INET6
        return true;
#endif  // if/else WIN32
}  // end ProtoSocket::SetIPv6Capable()
#endif // HAVE_IPV6


bool ProtoSocket::SetTxBufferSize(unsigned int bufferSize)
{
   if (!IsOpen())
   {
        DMSG(0, "ProtoSocket::SetTxBufferSize() error: socket closed\n");     
        return false;
   }
   unsigned int oldBufferSize = GetTxBufferSize();
   if (setsockopt(handle, SOL_SOCKET, SO_SNDBUF, (char*)&bufferSize, sizeof(bufferSize)) < 0) 
   {
        setsockopt(handle, SOL_SOCKET, SO_SNDBUF, (char*)&oldBufferSize, sizeof(oldBufferSize));
        DMSG(0, "ProtoSocket::SetTxBufferSize() setsockopt(SO_SNDBUF) error: %s\n",
                GetErrorString());
        return false;
   }
   return true;
}  // end ProtoSocket::SetTxBufferSize()

unsigned int ProtoSocket::GetTxBufferSize()
{
    if (!IsOpen()) return 0;
    unsigned int txBufferSize = 0;
    socklen_t len = sizeof(txBufferSize);
    if (getsockopt(handle, SOL_SOCKET, SO_SNDBUF, (char*)&txBufferSize, &len) < 0) 
    {
        DMSG(0, "ProtoSocket::GetTxBufferSize() getsockopt(SO_SNDBUF) error: %s\n",
                 GetErrorString());
	    return 0; 
    }
    return ((unsigned int)txBufferSize);
}  // end ProtoSocket::GetTxBufferSize()

bool ProtoSocket::SetRxBufferSize(unsigned int bufferSize)
{
   if (!IsOpen())
   {
        DMSG(0, "ProtoSocket::SetRxBufferSize() error: socket closed\n");    
        return false;
   }
   unsigned int oldBufferSize = GetTxBufferSize();
   if (setsockopt(handle, SOL_SOCKET, SO_RCVBUF, (char*)&bufferSize, sizeof(bufferSize)) < 0) 
   {
        setsockopt(handle, SOL_SOCKET, SO_RCVBUF, (char*)&oldBufferSize, sizeof(oldBufferSize));
        DMSG(0, "ProtoSocket::SetRxBufferSize() setsockopt(SO_RCVBUF) error: %s\n",
                GetErrorString());
        return false;
   }
   return true;
}  // end ProtoSocket::SetRxBufferSize()

unsigned int ProtoSocket::GetRxBufferSize()
{
    if (!IsOpen()) return 0;
    unsigned int rxBufferSize = 0;
    socklen_t len = sizeof(rxBufferSize);
    if (getsockopt(handle, SOL_SOCKET, SO_RCVBUF, (char*)&rxBufferSize, &len) < 0) 
    {
        DMSG(0, "ProtoSocket::GetRxBufferSize() getsockopt(SO_RCVBUF) error: %s\n",
                GetErrorString());
	    return 0; 
    }
    return ((unsigned int)rxBufferSize);
}  // end ProtoSocket::GetRxBufferSize()

#ifdef HAVE_IPV6
bool ProtoSocket::SetFlowLabel(UINT32 label)  
{
    int result = 0; 
#ifdef SOL_IPV6
    if (label && !flow_label)
    {
       int on = 1;
       result = setsockopt(handle, SOL_IPV6, IPV6_FLOWINFO_SEND, (void*)&on, sizeof(on));
    }
    else if (!label && flow_label)
    {
        int off = 0;
        result = setsockopt(handle, SOL_IPV6, IPV6_FLOWINFO_SEND, (void*)&off, sizeof(off));
    }
#endif  //SOL_IPV6
    if (0 == result)
    {
        flow_label = htonl(label);
        return true;
    }
    else
    {
        DMSG(0, "ProtoSocket::SetFlowLabel() setsockopt(SOL_IPV6) error\n");
        return false;
    }    
}  // end ProtoSocket::SetFlowLabel()
#endif  //HAVE_IPV6





ProtoSocket::List::List()
 : head(NULL)
{
}

ProtoSocket::List::~List()
{
    Destroy();
}

void ProtoSocket::List::Destroy()
{
    Item* next = head;
    while (next)
    {
        Item* current = next;
        next = next->GetNext();
        delete current->GetSocket();
        delete current;
    }   
    head = NULL;
}  // end ProtoSocket::List::Destroy()

bool ProtoSocket::List::AddSocket(ProtoSocket& theSocket)
{
    Item* item = new Item(&theSocket);
    if (item)
    {
        item->SetPrev(NULL);
        item->SetNext(head);
        head = item;
        return true;
    }
    else
    {
        DMSG(0, "ProtoSocket::List::AddSocket() new Item error: %s\n", GetErrorString());
        return false;
    }
}  // end ProtoSocket::List::AddSocket()

void ProtoSocket::List::RemoveSocket(ProtoSocket& theSocket)
{
    Item* item = head;
    while (item)
    {
        if (&theSocket == item->GetSocket())
        {
            Item* prev = item->GetPrev();
            Item* next = item->GetNext();
            if (prev) 
                prev->SetNext(next);
            else
                head = next;
            if (next) next->SetPrev(prev);
            delete item;
            break;
        }
        item = item->GetNext();   
    }
}  // end ProtoSocket::List::AddSocket()

ProtoSocket::List::Item* ProtoSocket::List::FindItem(const ProtoSocket& theSocket) const
{
    Item* item = head;
    while (item)
    {
        if (&theSocket == item->GetSocket())
            return item;
        else
            item = item->GetNext(); 
    }  
    return NULL; 
}  // end ProtoSocket::List::FindItem()

ProtoSocket::List::Item::Item(ProtoSocket* theSocket)
 : socket(theSocket), prev(NULL), next(NULL)
{
}

ProtoSocket::List::Iterator::Iterator(const ProtoSocket::List& theList)
 : list(theList), next(theList.head)
{
}
