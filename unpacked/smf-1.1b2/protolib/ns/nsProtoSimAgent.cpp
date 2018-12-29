#include "nsProtoSimAgent.h"
#include "ip.h"  	// for ns-2 hdr_ip def
#include "flags.h"  // for ns-2 hdr_flags def

// (TBD) should "NsProtoSimAgent" specify a different packet type?
NsProtoSimAgent::NsProtoSimAgent()
 : Agent(PT_UDP), scheduler(NULL)
{
    scheduler = &Scheduler::instance();
}

NsProtoSimAgent::~NsProtoSimAgent()
{
   
}

int NsProtoSimAgent::command(int argc, const char*const* argv)
{
   if (argc >= 2)
   {
        if (!strcmp("startup", argv[1]))
        {
            if (OnStartup(argc-1, argv+1))
            {
                return TCL_OK;
            }
            else
            {
                fprintf(stderr, "NsProtoSimAgent::command() bad command\n");
                return TCL_ERROR;       
            }   
        }
        else if (!strcmp("shutdown", argv[1]))
        {
            OnShutdown();
            return TCL_OK;    
        }  
        else if (ProcessCommands(argc, argv))
        {
            return TCL_OK;   
        } 
   }
   return Agent::command(argc, argv);
}  // end NsProtoSimAgent::command()

bool NsProtoSimAgent::InvokeSystemCommand(const char* cmd)
{
    tcl = &Tcl::instance();     /* This is the tcl instance for the ns2 node.
                                 * Any "operating system" command for the node
                                 * can be invoked using this pointer for the
                                 * tcl.evalf(const char*) method.
                                 */

    fprintf(stderr,"NsProtoSimAgent: invoking command \"%s\"\n", cmd);
    fprintf(stderr,"NsProtoSimAgent: Tcl == %p\n", tcl);

    Tcl& tcl = Tcl::instance();	
    Agent* agent = dynamic_cast<Agent*>(this);
	tcl.evalf("%s set node_", agent->name());
    const char* nodeName = tcl.result();
    
    tcl.evalf("%s %s", nodeName, cmd);

    return true;
}

ProtoSimAgent::SocketProxy* NsProtoSimAgent::OpenSocket(ProtoSocket& theSocket)
{

//	printf("NsProtoSimAgent: Opening Socket\n");

    // Create an appropriate ns-2 transport agent
    Tcl& tcl = Tcl::instance();
    if (ProtoSocket::UDP == theSocket.GetProtocol()) 
        tcl.evalf("eval new Agent/ProtoUdpSocket");
    else
        tcl.evalf("eval new Agent/ProtoSocket/TCP");
    
//	printf("NsProtoSimAgent: Opened Socket\n");

    NSSocketProxy* socketAgent = 
        dynamic_cast<NSSocketProxy*>(tcl.lookup(tcl.result()));
    
    if (socketAgent)
    {
        socketAgent->AttachSocket(theSocket);
        socket_proxy_list.Prepend(*socketAgent);
        return socketAgent;   
    }    
    else
    {
        fprintf(stderr, "NsProtoSimAgent::OpenSocket() error creating socket agent\n");
        return NULL;
    }
}  // end NSProtoSimAgent::OpenSocket()

void NsProtoSimAgent::CloseSocket(ProtoSocket& theSocket)
{
    ASSERT(theSocket.IsOpen());
    Tcl& tcl = Tcl::instance();	
    tcl.evalf("Simulator instance");
    char simName[32];
    strncpy(simName, tcl.result(), 32);
    tcl.evalf("%s set node_", this->name());
    const char* nodeName = tcl.result();
    NSSocketProxy* socketAgent = static_cast<NSSocketProxy*>(theSocket.GetHandle());
    tcl.evalf("%s detach-agent %s %s", simName, nodeName, 
              dynamic_cast<Agent*>(socketAgent)->name());
    socket_proxy_list.Remove(*socketAgent);
    delete socketAgent;
}  // end NSProtoSimAgent::OpenSocket()

NsProtoSimAgent::NSSocketProxy::NSSocketProxy()
 : mcast_ttl(255), mcast_loopback(false), 
   recv_data(NULL), recv_data_len(0)
{    
}

NsProtoSimAgent::NSSocketProxy::~NSSocketProxy()
{
}

bool NsProtoSimAgent::NSSocketProxy::Bind(UINT16& thePort)
{
    ASSERT(proto_socket && proto_socket->IsOpen());
    Agent* simAgent = dynamic_cast<Agent*>(proto_socket->GetNotifier());
    Tcl& tcl = Tcl::instance();	
    tcl.evalf("%s set node_", simAgent->name());
    const char* nodeName = tcl.result();
    
    Agent* socketProxyAgent = dynamic_cast<Agent*>(this);
    
    if (thePort > 0)
        tcl.evalf("%s attach %s %hu", nodeName, socketProxyAgent->name(), thePort);
    else
        tcl.evalf("%s attach %s", nodeName, socketProxyAgent->name());
    
    thePort = (UINT16)socketProxyAgent->port();
    
    return true;       
}  // end NsProtoSimAgent::NSSocketProxy::Bind()

bool NsProtoSimAgent::NSSocketProxy::RecvFrom(char*          buffer, 
                                            unsigned int& numBytes, 
                                            ProtoAddress& srcAddr)
{
    if (recv_data)
    {
        if (numBytes >= recv_data_len)
        {
            numBytes = recv_data_len;
            memcpy(buffer, recv_data, numBytes);
            srcAddr = src_addr;
            recv_data = NULL;
            recv_data_len = 0;
            return true;
        }
        else
        {
            fprintf(stderr, "NsProtoSimAgent::NSSocketProxy::RecvFrom() buffer tool small\n");
            numBytes = 0;
            return false;    
        }        
    }
    else
    {
        //if (GetDebugLevel() > 4)
        //    fprintf(stderr, "NsProtoSimAgent::NSSocketProxy::RecvFrom() no data ready\n");
        numBytes = 0;
        return false;       
    }   
}  // end NsProtoSimAgent::NSSocketProxy::RecvFrom()

bool NsProtoSimAgent::NSSocketProxy::JoinGroup(const ProtoAddress& groupAddr)
{
    if (groupAddr.IsBroadcast()) return true; // bcast membership implicit
    Agent* socketAgent = dynamic_cast<Agent*>(this);
    ASSERT(socketAgent);
    
    ProtoSocket* s = GetSocket();
    if (!s->IsBound()) s->Bind(0);  // can only join on bound sockets (TBD) return false instead???
    
    Tcl& tcl = Tcl::instance();	
    tcl.evalf("%s set node_", socketAgent->name());
	const char* nodeName = tcl.result();
    tcl.evalf("%s join-group %s %d", nodeName, 
              socketAgent->name(), groupAddr.SimGetAddress());	
    return true;
}  // end NsProtoSimAgent::NSSocketProxy::JoinGroup()

bool NsProtoSimAgent::NSSocketProxy::LeaveGroup(const ProtoAddress& groupAddr)
{
    if (groupAddr.IsBroadcast()) return true; // bcast membership implicit
    Tcl& tcl = Tcl::instance();	
    Agent* socketAgent = dynamic_cast<Agent*>(this);
    ASSERT(socketAgent);
	tcl.evalf("%s set node_", socketAgent->name());
	const char* nodeName = tcl.result();
    tcl.evalf("%s leave-group %s %d", nodeName, 
              socketAgent->name(), groupAddr.SimGetAddress());	
	return true;
}  // end NsProtoSimAgent::NSSocketProxy::LeaveGroup(()

/////////////////////////////////////////////////////////////////////////
// NsProtoSimAgent::UdpSocketAgent implementation
//
static class ProtoSocketUdpAgentClass : public TclClass {
public:
	ProtoSocketUdpAgentClass() : TclClass("Agent/ProtoUdpSocket") {}
	TclObject* create(int, const char*const*) {
		return (new NsProtoSimAgent::UdpSocketAgent());
	}
} class_proto_socket_udp_agent;

NsProtoSimAgent::UdpSocketAgent::UdpSocketAgent()
 : Agent(PT_UDP)
{
}

bool NsProtoSimAgent::UdpSocketAgent::SendTo(const char*         buffer, 
                                             unsigned int&       buflen,
                                             const ProtoAddress& dstAddr)
{
    // Allocate _and_ init packet
    Packet* p = allocpkt(buflen);
    if (p)
    {
        // Set packet destination addr/port
        hdr_ip* iph = hdr_ip::access(p);
        hdr_flags::access(p)->ect() = 1;  // ecn enabled 
        iph->daddr() = dstAddr.SimGetAddress();
        iph->dport() = dstAddr.GetPort();   
        if (dstAddr.IsMulticast())
            iph->ttl() = mcast_ttl;
        else
            iph->ttl() = 255;
        hdr_cmn* ch = hdr_cmn::access(p);  
        // We could "fake" other packet types here if we liked
        ch->ptype() = PT_UDP;
        // Copy data to be sent into packet
        memcpy(p->accessdata(), buffer, buflen);
        // Set packet length field to UDP payload size for now
        // (TBD) Should we add UDP/IP header overhead?
        ch->size() = buflen;
        send(p, 0);
        return true;
    }
    else
    {
       perror("NsProtoSimAgent::UdpSocketAgent::SendTo() new Packet error");
       return false;
    }
}  // end NsProtoSimAgent::UdpSocketAgent::SendTo()


void NsProtoSimAgent::UdpSocketAgent::recv(Packet* pkt, Handler* /*h*/)
{
    
    if (pkt->userdata() && (PACKET_DATA == pkt->userdata()->type()))
    {
        PacketData* pktData = (PacketData*)pkt->userdata();
        char* recvData = (char*)pktData->data();
        unsigned int recvLen = pktData->size();
        ns_addr_t srcAddr = HDR_IP(pkt)->src(); 
        ns_addr_t dstAddr = HDR_IP(pkt)->dst();   
        bool isUnicast = (0 == (dstAddr.addr_ & 0x80000000));
        if ((addr() != srcAddr.addr_) || 
            (port() != srcAddr.port_) ||
            isUnicast || mcast_loopback)
        {
            recv_data = recvData;
            recv_data_len = recvLen;
            src_addr.SimSetAddress(srcAddr.addr_);
            src_addr.SetPort(srcAddr.port_);
            if (proto_socket)
                proto_socket->OnNotify(ProtoSocket::NOTIFY_INPUT);  
        }
    }
    else
    {
        fprintf(stderr, "NsProtoSimAgent::UdpSocketAgent::recv() warning: "
                        "received packet with no payload\n");   
    }
    Packet::free(pkt);
}  // end NsProtoSimAgent::UdpSocketAgent::recv()

