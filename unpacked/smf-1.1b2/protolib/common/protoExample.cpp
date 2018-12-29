
#ifdef SIMULATE
#include "nsProtoSimAgent.h"
#else
#include "protoApp.h"
#include "protoRouteMgr.h"
#endif  // if/else SIMULATE

#include <stdlib.h>  // for atoi()
#include <stdio.h>   // for stdout/stderr printouts
#include <string.h>

class ProtoExample :
#ifdef SIMULATE
                     public NsProtoSimAgent
#else
                     public ProtoApp
#endif  // if/else SIMULATE
                     
{
    public:
        ProtoExample();
        ~ProtoExample();

        // Overrides from ProtoApp or NsProtoSimAgent base
        bool OnStartup(int argc, const char*const* argv);
        bool ProcessCommands(int argc, const char*const* argv);
        void OnShutdown();

    private:
        enum CmdType {CMD_INVALID, CMD_ARG, CMD_NOARG};
        static CmdType GetCmdType(const char* string);
        bool OnCommand(const char* cmd, const char* val);        
        void Usage();
        
        bool OnTxTimeout(ProtoTimer& theTimer);
        void OnUdpSocketEvent(ProtoSocket&       theSocket, 
                              ProtoSocket::Event theEvent);
        void OnClientSocketEvent(ProtoSocket&       theSocket, 
                                 ProtoSocket::Event theEvent);
        void OnServerSocketEvent(ProtoSocket&       theSocket, 
                                 ProtoSocket::Event theEvent);
        static const char* const CMD_LIST[];
        static void SignalHandler(int sigNum);
        
        // ProtoTimer/ UDP socket demo members
        ProtoTimer          tx_timer;
        ProtoAddress        dst_addr;
        
        ProtoSocket         udp_tx_socket;
        ProtoSocket         udp_rx_socket;
        
        // TCP socket demo members
        ProtoSocket         server_socket;
        ProtoSocket         client_socket;
        unsigned int        client_msg_count;
        ProtoSocket::List   connection_list;
}; // end class ProtoExample


// (TBD) Note this #if/else code could be replaced with something like
// a PROTO_INSTANTIATE(ProtoExample) macro defined differently
// in "protoApp.h" and "nsProtoSimAgent.h"
#ifdef SIMULATE
#ifdef NS2
static class NsProtoExampleClass : public TclClass
{
	public:
		NsProtoExampleClass() : TclClass("Agent/ProtoExample") {}
	 	TclObject *create(int argc, const char*const* argv) 
			{return (new ProtoExample());}
} class_proto_example;	
#endif // NS2


#else

// Our application instance 
PROTO_INSTANTIATE_APP(ProtoExample) 

#endif  // SIMULATE

ProtoExample::ProtoExample()
: udp_tx_socket(ProtoSocket::UDP),
  udp_rx_socket(ProtoSocket::UDP),
  server_socket(ProtoSocket::TCP), 
  client_socket(ProtoSocket::TCP), 
  client_msg_count(0)
{    
    tx_timer.SetListener(this, &ProtoExample::OnTxTimeout);
    udp_tx_socket.SetNotifier(&GetSocketNotifier());
    udp_tx_socket.SetListener(this, &ProtoExample::OnUdpSocketEvent);
    udp_rx_socket.SetNotifier(&GetSocketNotifier());
    udp_rx_socket.SetListener(this, &ProtoExample::OnUdpSocketEvent);
    client_socket.SetNotifier(&GetSocketNotifier());
    client_socket.SetListener(this, &ProtoExample::OnClientSocketEvent);
    server_socket.SetNotifier(&GetSocketNotifier());
    server_socket.SetListener(this, &ProtoExample::OnServerSocketEvent);
}

ProtoExample::~ProtoExample()
{
    
}

void ProtoExample::Usage()
{
    fprintf(stderr, "protoExample [send <host>/<port>] [recv [<group>/]<port>]\n"
                    "             connect <host>/<port>] [listen <port>]\n");
}  // end ProtoExample::Usage()


const char* const ProtoExample::CMD_LIST[] =
{
    "+send",       // Send UDP packets to destination host/port
    "+recv",       // Recv UDP packets on <port> (<group> addr optional)
    "+connect",    // TCP connect and send to destination host/port
    "+listen",     // Listen for TCP connections on <port>
    NULL
};

ProtoExample::CmdType ProtoExample::GetCmdType(const char* cmd)
{
    if (!cmd) return CMD_INVALID;
    unsigned int len = strlen(cmd);
    bool matched = false;
    CmdType type = CMD_INVALID;
    const char* const* nextCmd = CMD_LIST;
    while (*nextCmd)
    {
        if (!strncmp(cmd, *nextCmd+1, len))
        {
            if (matched)
            {
                // ambiguous command (command should match only once)
                return CMD_INVALID;
            }
            else
            {
                matched = true;   
                if ('+' == *nextCmd[0])
                    type = CMD_ARG;
                else
                    type = CMD_NOARG;
            }
        }
        nextCmd++;
    }
    return type; 
}  // end ProtoExample::GetCmdType()


bool ProtoExample::OnStartup(int argc, const char*const* argv)
{
#ifdef _WIN32_WCE
    if (!OnCommand("recv", "224.225.1.2/5002"))
    {
        TRACE("Error with \"recv\" command ...\n");
    }
    if (!OnCommand("send", "224.225.1.2  /5002"))
    {
        TRACE("Error with \"send\" command ...\n");
    }
#endif // _WIN32_WCE
    
    
    if (!ProcessCommands(argc, argv))
    {
        DMSG(0, "protoExample: Error! bad command line\n");
        return false;
    }  
    
#ifndef SIMULATE     
    // Here's some code to test the ProtoSocket routines for network interface info       
    ProtoAddress localAddress;

    char* macName = "00:c0:a8:7c:e8:83";
    char macAddr[6];
    char* ptr = macName;
    int i = 0;
    while (ptr && ('\0' != *ptr))
    {
        int val;
        if (1 == sscanf(ptr, "%x:", &val))
        {
            macAddr[i++] = (char)val;
        }
        else
        {
            TRACE("Error reading macAddr ...\n");
        }
        ptr = strchr(ptr, ':');
        if (ptr) ptr++;
    }
    localAddress.SetRawHostAddress(ProtoAddress::ETH, macAddr, 6);
    ProtoSocket dummySocket2(ProtoSocket::UDP);
    char nameBuffer[256];
    dummySocket2.GetInterfaceName(localAddress, nameBuffer, 256);
    TRACE("FOUND BY MAC ADDRESS IF NAME: %s\n", nameBuffer);

    if (localAddress.ResolveLocalAddress())
    {
        ProtoSocket dummySocket(ProtoSocket::UDP);
        unsigned int index = dummySocket.GetInterfaceIndex(localAddress.GetHostString());
        TRACE("protoExample: got interface index:%d for interface addr: %s\n", 
               index, localAddress.GetHostString());

        char buffer[256];
        buffer[255] = '\0';
        if (dummySocket.GetInterfaceName(index, buffer, 255))
        {
            TRACE("    (real interface name: \"%s\")\n", buffer);
        }

        index = dummySocket.GetInterfaceIndex(buffer);
        TRACE("protoExample: got interface index:%d for interface: %s\n", index, buffer);

        ProtoAddress ifAddr;
        if (dummySocket.GetInterfaceAddress(buffer, ProtoAddress::IPv4, ifAddr))
        {
            TRACE("protoExample: got IPv4 address: %s for interface: %s\n", ifAddr.GetHostString(), buffer);
        }
#ifdef HAVE_IPV6
        if (dummySocket.GetInterfaceAddress(buffer, ProtoAddress::IPv6, ifAddr))
        {
            TRACE("protoExample: got IPv6 address: %s for interface: %s\n", ifAddr.GetHostString(), buffer);
        }
#endif // HAVE_IPV6
        ProtoAddress ethAddr; 
        if (dummySocket.GetInterfaceAddress(buffer, ProtoAddress::ETH, ethAddr))
	    TRACE("protoExample: got ETH address: %s for interface: %s\n", ethAddr.GetHostString(), buffer);
        
    }
    
    //return false;
       
    /*// Here's some code to get the system routing table
    ProtoRouteTable routeTable;
    if (!routeTable.Init())
    {
        DMSG(0, "ProtoExample::OnStartup() error initing route table\n");
        return false;
    }
    ProtoRouteMgr* routeMgr = ProtoRouteMgr::Create();
    if (NULL == routeMgr)
    {
        DMSG(0, "ProtoExample::OnStartup() error creating route manager\n");
        return false;
    }
    if (!routeMgr->Open())
    {
        DMSG(0, "ProtoExample::OnStartup() error opening route manager\n");
        return false;    
    }
    if (!routeMgr->GetAllRoutes(ProtoAddress::IPv4, routeTable))
        DMSG(0, "ProtoExample::OnStartup() warning getting system routes\n");     
    routeMgr->Close();
    // Display whatever routes we got
    ProtoRouteTable::Iterator iterator(routeTable);
    ProtoRouteTable::Entry* entry;
    DMSG(0, "Routing Table:\n");
    DMSG(0, "%16s/Prefix %-12s ifIndex Metric\n", "Destination", "Gateway");
    while ((entry= iterator.GetNextEntry()))
    {
        DMSG(0, "%16s/%02u     ",
                entry->GetDestination().GetHostString(), entry->GetPrefixLength());
        const ProtoAddress& gw = entry->GetGateway();
        DMSG(0, "%-16s %u     %d\n",
                gw.IsValid() ? gw.GetHostString() : "0.0.0.0", 
                entry->GetInterfaceIndex(),
                entry->GetMetric());
    }
    */
    fprintf(stderr, "\nprotoExample: entering main loop (<CTRL-C> to exit)...\n");
#endif // !SIMULATE
    return true;
}  // end ProtoExample::OnStartup()

void ProtoExample::OnShutdown()
{
    if (tx_timer.IsActive()) tx_timer.Deactivate();
    if (udp_tx_socket.IsOpen()) udp_tx_socket.Close();
    if (udp_rx_socket.IsOpen()) udp_rx_socket.Close();
    if (server_socket.IsOpen()) server_socket.Close();
    if (client_socket.IsOpen()) client_socket.Close();
    connection_list.Destroy();   
#ifndef SIMULATE
    DMSG(0, "protoExample: Done.\n");
#endif // SIMULATE
    CloseDebugLog();
}  // end ProtoExample::OnShutdown()

bool ProtoExample::ProcessCommands(int argc, const char*const* argv)
{
    // Dispatch command-line commands to our OnCommand() method
    int i = 1;
    while ( i < argc)
    {
        // Is it a class ProtoExample command?
        switch (GetCmdType(argv[i]))
        {
            case CMD_INVALID:
            {
#ifndef SIMULATE
                DMSG(0, "ProtoExample::ProcessCommands() Invalid command:%s\n", 
                        argv[i]);
#endif // SIMULATE
                return false;
            }
            case CMD_NOARG:
                if (!OnCommand(argv[i], NULL))
                {
                    DMSG(0, "ProtoExample::ProcessCommands() ProcessCommand(%s) error\n", 
                            argv[i]);
                    return false;
                }
                i++;
                break;
            case CMD_ARG:
                if (!OnCommand(argv[i], argv[i+1]))
                {
                    DMSG(0, "ProtoExample::ProcessCommands() ProcessCommand(%s, %s) error\n", 
                            argv[i], argv[i+1]);
                    return false;
                }
                i += 2;
                break;
        }
    }
    return true;  
}  // end ProtoExample::ProcessCommands()

bool ProtoExample::OnCommand(const char* cmd, const char* val)
{
    // (TBD) move command processing into Mgen class ???
    CmdType type = GetCmdType(cmd);
    ASSERT(CMD_INVALID != type);
    unsigned int len = strlen(cmd);
    if ((CMD_ARG == type) && !val)
    {
        DMSG(0, "ProtoExample::ProcessCommand(%s) missing argument\n", cmd);
        return false;
    }
    else if (!strncmp("send", cmd, len))
    {
        TRACE("opening udp socket ...\n");
        if (!udp_tx_socket.Open())
        {
            DMSG(0, "ProtoExample::ProcessCommand(send) error opening udp_tx_socket\n");
            return false;    
        }    
        TRACE("udp tx socket opened ...\n");
        char string[256];
        strncpy(string, val, 256);
        string[255] ='\0';
        char* ptr = strchr(string, '/');
        if (ptr) 
        {
            *ptr++ = '\0';
            if (!dst_addr.ResolveFromString(string))
            {
                DMSG(0, "ProtoExample::ProcessCommand(send) error: invalid <host>\n");
                return false;
            }
            TRACE("Resolved dst_addr from %s\n", string);
            TRACE("dst_addr:\"%s\" type:%d\n", dst_addr.GetHostString(), dst_addr.GetType());

            dst_addr.SetPort(atoi(ptr));
            tx_timer.SetInterval(1.0);
            tx_timer.SetRepeat(-1);
            TRACE("calling OnTxTimeout() ...\n");
            OnTxTimeout(tx_timer);
            if (!tx_timer.IsActive()) ActivateTimer(tx_timer);
        }
        else
        {
            DMSG(0, "ProtoExample::ProcessCommand(send) error: <port> not specified\n");
            return false;   
        } 
    }
    else if (!strncmp("recv", cmd, len))
    {     
        char string[256];
        strncpy(string, val, 256);
        string[255] ='\0';
        char* portPtr = strchr(string, '/');
        ProtoAddress groupAddr;
        if (portPtr) 
        {
            *portPtr++ = '\0';
            if (!groupAddr.ResolveFromString(string))
            {
                DMSG(0, "ProtoExample::ProcessCommand(recv) error: invalid <groupAddr>\n");
                return false;
            }
        }
        else
        {
            portPtr = string; 
        } 
        UINT16 thePort;
        if (1 == sscanf(portPtr, "%hu", &thePort))
        {
            if (!udp_rx_socket.Open(thePort))
            {
                DMSG(0, "ProtoExample::ProcessCommand(recv) error opening udp_rx_socket\n");
                return false;   
            }
            if (groupAddr.IsValid() && groupAddr.IsMulticast())
            {
                if (!(udp_rx_socket.JoinGroup(groupAddr)))
                {
                    DMSG(0, "ProtoExample::ProcessCommand(recv) error joining group\n");
                    udp_rx_socket.Close();
                    return false;  
                }
            }            
        }
        else
        {
            DMSG(0, "ProtoExample::ProcessCommand(recv) error: <port> not specified\n");
            return false;   
        } 
    }
    else if (!strncmp("connect", cmd, len))
    {
        TRACE("connecting client socket:%p\n", &client_socket);
        char string[256];
        strncpy(string, val, 256);
        string[255] ='\0';
        char* ptr = strchr(string, '/');
        if (ptr) 
        {
            *ptr++ = '\0';
            ProtoAddress server;
            if (!server.ResolveFromString(string))
            {
                DMSG(0, "ProtoExample::ProcessCommand(connect) error: invalid <host>\n");
                return false;
            }
            server.SetPort(atoi(ptr));
            if (!client_socket.Connect(server))
            {
                DMSG(0, "ProtoExample::ProcessCommand(connect) error connecting\n");
                return false;
            }
            client_msg_count = 5;  // (Send 5 messages, then disconnect)
            client_socket.StartOutputNotification();
        }
        else
        {
            DMSG(0, "ProtoExample::ProcessCommand(connect) error: <port> not specified\n");
            return false;   
        } 
    }
    else if (!strncmp("listen", cmd, len))
    {
        TRACE("listening on server socket:%p\n", &server_socket);
        UINT16 thePort;
        if (1 == sscanf(val, "%hu", &thePort))
        {
            if (!server_socket.Listen(thePort))
            {
                DMSG(0, "ProtoExample::ProcessCommand(listen) error listening\n");
                return false;   
            }            
        }
        else
        {
            DMSG(0, "ProtoExample::ProcessCommand(connect) error: <port> not specified\n");
            return false;   
        } 
    }
    else if (!strncmp("background", cmd, len))
    {
        // do nothing (this command was scanned immediately at startup)
    }
    return true;
}  // end ProtoExample::OnCommand()

bool ProtoExample::OnTxTimeout(ProtoTimer& /*theTimer*/)
{
    const char* string = "Hello there UDP peer, how are you doing?";
    unsigned int len = strlen(string) + 1;
    unsigned int numBytes = len;
    
    //TRACE("ProtoExample::OnTxTimeout() ...\n");
    if (!udp_tx_socket.SendTo(string, numBytes, dst_addr))
    {
        DMSG(0, "ProtoExample::OnTxTimeout() error sending to %s/%hu ...\n",
                dst_addr.GetHostString(), dst_addr.GetPort());   
    }
    else if (len != numBytes)
    {
        DMSG(0, "ProtoExample::OnClientSocketEvent() incomplete SendTo()\n");                
        TRACE("   (only sent %lu of %lu bytes)\n", numBytes, len);
    }
    return true;
}  // end ProtoExample::OnTxTimeout()

void ProtoExample::OnUdpSocketEvent(ProtoSocket&       theSocket, 
                                    ProtoSocket::Event theEvent)
{
    if (&theSocket == &udp_tx_socket)
        TRACE("ProtoExample::OnUdpTxSocketEvent(");
    else
        TRACE("ProtoExample::OnUdpRxSocketEvent(");
    switch (theEvent)
    {
        case ProtoSocket::INVALID_EVENT:
            TRACE("ERROR) ...\n");
            break;
        case ProtoSocket::CONNECT:
            TRACE("CONNECT) ...\n");
            break;  
        case ProtoSocket::ACCEPT:
            TRACE("ACCEPT) ...\n");
            break; 
        case ProtoSocket::SEND:
        {
            TRACE("SEND) ...\n");
            break; 
        }
        case ProtoSocket::RECV:
        {
            static unsigned long count = 0;
            TRACE("RECV) ...\n");
            ProtoAddress srcAddr;
            char buffer[1024];
            unsigned int len = 1024;
            if (theSocket.RecvFrom(buffer, len, srcAddr))
            {
                buffer[len] = '\0';
                if (len)
                    TRACE("ProtoExample::OnUdpSocketEvent(%lu) received \"%s\" from %s\n", 
                           count++, buffer, srcAddr.GetHostString());
                else
                    TRACE("ProtoExample::OnUdpSocketEvent() received 0 bytes\n");
            }
            else
            {
                DMSG(0, "ProtoExample::OnUdpSocketEvent() error receiving\n");
            }
            break; 
        }
        case ProtoSocket::DISCONNECT:
            TRACE("DISCONNECT) ...\n");
            break;  
    }
}  // end ProtoExample::OnUdpSocketEvent()


void ProtoExample::OnClientSocketEvent(ProtoSocket&       theSocket, 
                                       ProtoSocket::Event theEvent)
{
    TRACE("ProtoExample::OnClientSocketEvent(");
    switch (theEvent)
    {
        case ProtoSocket::INVALID_EVENT:
            TRACE("ERROR) ...\n");
            break;
        case ProtoSocket::CONNECT:
            TRACE("CONNECT) ...\n");
            break;  
        case ProtoSocket::ACCEPT:
            TRACE("ACCEPT) ...\n");
            break; 
        case ProtoSocket::SEND:
        {
            TRACE("SEND) ...\n");
            const char* string = "Hello there ProtoServer, how are you doing?";
            unsigned int len = strlen(string) + 1;
            unsigned int numBytes = len;
            if (!client_socket.Send(string, numBytes))
            {
                DMSG(0, "ProtoExample::OnClientSocketEvent() error sending\n");   
            }
            else if (len != numBytes)
            {
                DMSG(0, "ProtoExample::OnClientSocketEvent() incomplete Send()\n");                
                TRACE("   (only sent %lu of %lu bytes)\n", numBytes, len);
            }   
            if (0 == --client_msg_count)
            {
                client_socket.Shutdown();
            }         
            break; 
        }
        case ProtoSocket::RECV:
        {
            TRACE("RECV) ...\n");
            char buffer[16];
            unsigned int len = 16;
            theSocket.Recv(buffer, len);
            break;  
        }
        case ProtoSocket::DISCONNECT:
            TRACE("DISCONNECT) ...\n");
            client_socket.Close();
            break;  
    }
}  // end ProtoExample::OnClientSocketEvent()
  
void ProtoExample::OnServerSocketEvent(ProtoSocket&       theSocket, 
                                       ProtoSocket::Event theEvent)
{
    TRACE("ProtoExample::OnServerSocketEvent()\n");
    switch (theEvent)
    {
        case ProtoSocket::INVALID_EVENT:
            TRACE("INVALID_EVENT) ...\n");
            break;
        case ProtoSocket::CONNECT:
            TRACE("CONNECT) ...\n");
            break;  
        case ProtoSocket::ACCEPT:
        {
            TRACE("ACCEPT) ...\n");
            ProtoSocket* connectedSocket = new ProtoSocket(ProtoSocket::TCP);
            if (!connectedSocket)
            {
                DMSG(0, "ProtoExample::OnServerSocketEvent(ACCEPT) new ProtoSocket error\n");
                break;
            }
            if (!server_socket.Accept(connectedSocket))
            {
                DMSG(0, "ProtoExample::OnServerSocketEvent(ACCEPT) error accepting connection\n");
                delete connectedSocket;
                break;
            }
            //clientSocket->SetListener(static_cast<ProtoSocket::Listener*>(this),
            //                          static_cast<EventHandler>(&ProtoExample::OnClientSocketEvent));
            if (!connection_list.AddSocket(*connectedSocket))
            {
                DMSG(0, "ProtoExample::OnServerSocketEvent(ACCEPT) error adding client to list\n");
                delete connectedSocket;
            }
            DMSG(0, "protoExample: accepted connection from %s\n",
                    connectedSocket->GetDestination().GetHostString());
            break; 
        }
        case ProtoSocket::SEND:
            TRACE("SEND) ...\n");
            break; 
        case ProtoSocket::RECV:
        {
            TRACE("RECV) ...\n");
            char buffer[1024];
            unsigned int len = 1024;
            if (theSocket.Recv(buffer, len))
            {
                if (len)
                {
                    buffer[len-1] = '\0';
                    TRACE("ProtoExample::OnServerSocketEvent() received %u bytes \"%s\" from %s\n", 
                           len, buffer, theSocket.GetDestination().GetHostString());
                }
                else
                {
                    TRACE("ProtoExample::OnServerSocketEvent() received 0 bytes\n");
                }
            }
            else
            {
                DMSG(0, "ProtoExample::OnServerSocketEvent() error receiving\n");
            }
            break; 
        }
        case ProtoSocket::DISCONNECT:
            TRACE("DISCONNECT) ...\n");
            if(&theSocket == &server_socket)
            {
                TRACE("server_socket disconnected!\n");
            }
            theSocket.Close();
            connection_list.RemoveSocket(theSocket);
            break;
    }
}  // end ProtoExample::OnServerSocketEvent() 
       

