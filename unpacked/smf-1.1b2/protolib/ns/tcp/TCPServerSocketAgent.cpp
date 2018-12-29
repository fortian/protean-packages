/*
 *  TCPServerSocketAgent.cpp
 *
 *  Created by Ian Taylor on 01/03/2007.
 *
 */
 
#include "TCPServerSocketAgent.h"

static class TCPServerSocketAgentClass : public TclClass {
public:
	TCPServerSocketAgentClass() : TclClass("Agent/TCP/ServerSocketAgent") {}
	TclObject* create(int argc, const char*const *argv) {		
		cout << "Creating TCPSocketAgent " << argc << " arguments" << endl;
		
		if (argc == 6) { // with protocol included
			return (new TCPServerSocketAgent(argv[4], atoi(argv[5])));
		} else if (argc == 5) {
			TCPServerSocketAgent *tcpsock = new TCPServerSocketAgent(TCPSocketAgent::FULLTCP, atoi(argv[5]));
			return (tcpsock);
		} else if (argc == 4) {
			TCPServerSocketAgent *tcpsock = new TCPServerSocketAgent(TCPSocketAgent::FULLTCP, 0);
			return (tcpsock);
		} else
			return NULL;
	}
} class_tcp_server_socket_agent; 


void TCPServerSocketAgent::recv(Packet *pkt, Handler *handler) {
		int tiflags = hdr_tcp::access(pkt)->flags() ; 		 // tcp flags from packet
		hdr_ip *iph = hdr_ip::access(pkt);

		if (tiflags & TH_SYN) { // connection request
			synPacket_= pkt;
			// set the senders port and address
			dport() = iph->sport();
			daddr() = iph->saddr();  
			
			printf("TCPServerSocketAgent received a SYN - connection request\n");
			if (tcpListener_!=NULL)
				tcpListener_->tcpEventReceived(new TCPEvent(TCPEvent::ACCEPT, 0, 0));
			else
				accept(); // no listener, so accept by default.
		} else { // its data to go to a TCPSocketAgent
			TCPSocketAgent* socket = mySocketList->findSocket(iph->saddr(), iph->sport());
			if (socket==NULL) {
				cout << "TCPServerSocketAgent: No socket found for address " << iph->saddr() << " and port " << iph->sport() << " !!!" << endl;
				cout << "TCPServerSocketAgent: This should not happen - please report bug " << endl;
				exit(1);
			}
			else socket->getTCPAgent()->recv(pkt,handler);
			}
	}

 /**
  * Returns a bew TCPSocketAgent that will deal with the accepted connection
  */
TCPSocketAgent* TCPServerSocketAgent::accept() { 
	printf("TCPServerSocketAgent::Accept CALLED \n");
	
	Tcl& tcl = Tcl::instance();    

	tcl.evalf("eval new Agent/TCP/SocketAgent");
	
	printf("TCPServerSocketAgent::Created new TCPSocketAgent \n");

	const char *tcpVar = tcl.result();
 
	printf("TCPSocketAgent::Got result \n");
	
	TCPSocketAgent *socket = (TCPSocketAgent *)tcl.lookup(tcpVar);

	socket->createTCPAgent(theTCPProtocol_); // creates the underlying TCP protocol for the agent
		
	socket->setDestinationPort(dport());
	socket->setDestinationAddress(daddr());

 	socket->attachTCPAgentToNode(nodeNameInTCL_); // attaches the socket to this node
	
	socket->getTCPInterface()->listenOn(port());

	socket->setToCurrentTCPAgent();

	mySocketList->prepend(socket);

	socket->getTCPAgent()->recv(synPacket_, NULL); // send the SYN packet to the TCP protocol so it can respond
														// as if it received it in the first place ....	 	
	return socket;
	}
	
/**
 * Overridden to become a dummy TCPSocketAgent that muli-plexes messages to real socket Agents.
 */
bool TCPServerSocketAgent::attachTCPAgentToNode(const char *node) {
	if (nodeNameInTCL_!=NULL) delete nodeNameInTCL_;
	nodeNameInTCL_ = new char[strlen(node)+1];
	strcpy(nodeNameInTCL_,node);
    
	Tcl& tcl = Tcl::instance();	
    // find out the name of this node in tcl space
	
	// tcl.evalf("%s set node_", node->name());
    // const char* nodeName = tcl.result();

	// cout << "Node name for " << name() << " on node: " << nodeNameInTCL_ <<  endl;
    	
	cout << "Attaching agent = " << name() << " on node: " << nodeNameInTCL_ <<  endl;

	tcl.evalf("%s attach %s", nodeNameInTCL_, name());

	cout << "Result = " << tcl.result() << endl;
	
	return false; 
	}
		
int TCPServerSocketAgent::command(int argc, const char*const* argv) {

	cout << "Node = " << name() << ", command: " << argv[1] << "," ;

    for (int i =0; i<argc; ++i)
		cout << " arg[" << i << "] = " << argv[i] << ", ";

	cout << endl;

	if (argc==2) {
		if (strcmp(argv[1], "listen") == 0) {
			//
			return (TCL_OK);
			}
	} else if (argc==3) {
		if (strcmp(argv[1], "attach-to-node") == 0) {
			attachTCPAgentToNode(argv[2]); 
			return (TCL_OK);
		}
	}
	
	return Agent::command(argc, argv);
} 

SocketList::SocketList() : head(NULL) {
}

void SocketList::prepend(TCPSocketAgent *proxy) {
    proxy->setPrev(NULL);
    proxy->setNext(head);
    if (head) head->setPrev(proxy);
    head = proxy;
}

void SocketList::remove(TCPSocketAgent *proxy) {
    TCPSocketAgent* prev = proxy->getPrev();
    TCPSocketAgent* next = proxy->getNext();
    if (prev)
        prev->setNext(next);
    else
        head = next;
    if (next)
        next->setPrev(prev);
} 

TCPSocketAgent* SocketList::findSocket(nsaddr_t address, nsaddr_t port) {					
    TCPSocketAgent* next = head;
    while (next) {
//		cout << "SockeList: comparing " << next->getDestinationAddress() << " and port " << next->getDestinationPort() << endl;
        if ((next->getDestinationPort() == port) && (next->getDestinationAddress() == address))
            return next;
        else
            next = next->getNext();   
    }
    return NULL;
}

void SocketList::removeSockets() {					
    TCPSocketAgent* cur = head;
	TCPSocketAgent* next = cur->getNext();
    while (cur) {
		delete cur;
		cur = next; 
		next = cur->getNext();   
    }
}

