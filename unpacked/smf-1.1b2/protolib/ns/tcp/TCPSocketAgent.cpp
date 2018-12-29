/*
 *  TCPSocketAgent.cpp
 *
 *  Created by Ian Taylor on 01/01/2007.
 *
 */
 
#include "TCPSocketAgent.h"

static TCPSocketAgent *currentAgent; // Used for temporarily setting the current TCPSocketAgent when
								// making a TCP connection 
  
// These are the  overloaded TCP classes that just detect the sequence of events when 
// connecting - more intelligent TCPFull classes. They are NOT instantiated directly for use
// with TCPSocketAgent. Use TCPSocketAgent or TCPProtoSocketAgent instead 
 
 
// NOTE: we same name as original TCP for FullTcp so we get initialised correctly. Otherwise
// we do not get the MANY variables deinfed in ./tcl/lib/ns-default.tcl initialised correctly
static class FullTcpWithEventsClass : public TclClass { 
public:
	FullTcpWithEventsClass() : TclClass("Agent/TCP/FullTcp/Events") {} 
	TclObject* create(int argc, const char*const* argv) {  
		if (argc != 4)
			return NULL;
		return (new TCPFullWithEvents());
	}
} class_full_tcp_with_events;

static class TCPSocketAgentClass : public TclClass {
public:
	TCPSocketAgentClass() : TclClass("Agent/TCP/SocketAgent") {}
	TclObject* create(int argc, const char*const *argv) {		
		cout << "Creating TCPSocketAgent " << argc << " arguments" << endl;
		
		if (argc == 6) { // with protocol included
			return (new TCPSocketAgent(argv[4], atoi(argv[5])));
		} else if (argc == 5) {
			TCPSocketAgent *tcpsock = new TCPSocketAgent(TCPSocketAgent::FULLTCP, atoi(argv[5]));
			return (tcpsock);
		} else if (argc == 4) {
			TCPSocketAgent *tcpsock = new TCPSocketAgent(TCPSocketAgent::FULLTCP, 0);
			return (tcpsock);
		} else
			return NULL;
	}  
} class_tcp_socket_agent; 


// Constructor Method Implementations

TCPSocketAgent::TCPSocketAgent(const char *theTCPProtocolString, nsaddr_t thePort)  : Agent(PT_TCP) {
	createTCPAgent(getTCPProtocolFromString(theTCPProtocolString));
	setPort(thePort);
}

TCPSocketAgent::TcpProtocol TCPSocketAgent::getTCPProtocolFromString(const char *theTCPProtocolString) { 			
	if (strcmp(theTCPProtocolString, "FULLTCP") == 0)
		return FULLTCP;
	else if (strcmp(theTCPProtocolString, "RENOFULLTCP") == 0)
		return RENOFULLTCP;
	else if (strcmp(theTCPProtocolString, "SACKFULLTCP") == 0)
		return SACKFULLTCP;
	else if (strcmp(theTCPProtocolString, "TAHOEFULLTCP") == 0)
		return TAHOEFULLTCP;
	else 
		return ERROR;
	}
  

bool TCPSocketAgent::connect(TCPSocketAgent *remoteTCPSockAgent) { 			
	FullTcpAgent *remoteFullTCPAgent = remoteTCPSockAgent->getTCPAgent();	
	connect(remoteFullTCPAgent->addr(), remoteFullTCPAgent->port());
	return true;
} 

/**
  Connect to remote location.  Note this just sets the dst port
*/
bool TCPSocketAgent::connect(nsaddr_t address, nsaddr_t port) {
	tcpAgentInterface->setDestinationPort(port);
	tcpAgentInterface->setDestinationAddress(address);  
	return true;
}	
	
/**
 * Send data to the TCP server through this socket. This uses the TCPSocket app class to actually
 * transfer the application data between the nodes.
 */	
void TCPSocketAgent::send(int nbytes, const char *data) {
	TcpData *tmp = new TcpData();
	tmp->setBytes(data, nbytes);
	tcpSocketApp->send(nbytes, tmp);
}

// TCPSocketAgentListener Interface implementation - used to receive events from TCP sockets

/**
 * Events from TCP Application received here - this can be reimplemented in a subclass in order to link into
 * whichever the software toolkit or application requires. Conenction events and receive etc defined in TCPEvent
 * are triggered by the underlying TCP agents and channeled through this method for conveneince. 
 */
bool TCPSocketAgent::tcpEventReceived(TCPEvent *event) {
/**	if (event->getType()==TCPEvent::ACCEPT) {
		currentAgent=this;
		Packet *pkt = (Packet *)event->getData(); // data contains the packet
		hdr_ip *iph = hdr_ip::access(pkt);
					 
		// set the desination to the sender of this packet i.e. the node trying to connect
		tcpAgentInterface->setDestinationPort(iph->sport());
		tcpAgentInterface->setDestinationAddress(iph->saddr());  
		}
	else */
	 
	if (event->getType()==TCPEvent::CONNECTED) {
		    printf("TCPSocketAgent CONNECTED Event on node %i\n", getTCPAgent()->addr());
	 		TCPSocketAgent* remoteTCPSocketAgent = currentAgent; // Just set when SYN was received by remote node
			TCPSocketApp *remoteTCPSocketApp = remoteTCPSocketAgent->getTCPApp();
			remoteTCPSocketApp->connect(tcpSocketApp); // connect the apps together 
		}
	else if (event->getType()==TCPEvent::SEND) {}
		// printf("TCPSocketAgent SEND Event on node %i\n", getTCPAgent()->addr());
	else if (event->getType()==TCPEvent::RECEIVE) {
		//printf("TCPSocketAgent RECEIVE Event on node %i\n", getTCPAgent()->addr());
		printf("TCPSocketAgent: Receiving %s of size %i at node %i\n", (char *)event->getData(), event->getDataSize(), getTCPAgent()->addr());
		}
	else if (event->getType()==TCPEvent::DISCONNECTED) {}
 		//printf("TCPSocketAgent DISCONNECT Event on node %i\n", getTCPAgent()->addr());
	else {
		printf("TCPSocketAgent UNKNOWN Event on node %i\n", getTCPAgent()->addr());
		return false;
		}
		
	// If a listener exists pass on the event
	if (tcpListener_) tcpListener_->tcpEventReceived(event);

	return true;
}
 
// Private Method Implementations
 
void TCPSocketAgent::setTCPParameter(const char *parName, const char *parValue) {
	Tcl& tcl = Tcl::instance();    
	
	cout << "Setting parameter " << parName << " to " << parValue << " on agent " << tcpAgent->name() << " on node: " << tcpAgent->addr() << endl;

	tcl.evalf("%s set %s %s", tcpAgent->name(), parName, parValue);
}
 
/**
 * Attaches the tcpAgent i.e. a FullTcp variant to this Ns-2 node.
 */
bool TCPSocketAgent::attachTCPAgentToNode(const char *node) {
	if (nodeNameInTCL_!=NULL) delete nodeNameInTCL_;
	nodeNameInTCL_ = new char[strlen(node)+1];
	strcpy(nodeNameInTCL_,node);
	Tcl& tcl = Tcl::instance();    

//	tcl.evalf("Simulator instance");
//	char simName[32];
//	strncpy(simName, tcl.result(), 32);

	cout << "Attaching agent = " << tcpAgent->name() << " on node: " << node <<  endl;

	tcl.evalf("%s attach %s", node, tcpAgent->name());

	cout << "Result = " << tcl.result() << endl;

//	cout << "Target is now = " << target() << " !! " <<  endl;

	return false; 
	}
 
bool TCPSocketAgent::createTCPAgent(TcpProtocol theTCPProtocol){
	Tcl& tcl = Tcl::instance();    
	
	theTCPProtocol_=theTCPProtocol;
	
	switch (theTCPProtocol) {
		case FULLTCP :  tcl.evalf("eval new Agent/TCP/FullTcp/Events");
			cout << "Created FULLTCP Agent" << endl;
			break;
		case TAHOEFULLTCP : tcl.evalf("eval new Agent/TCP/FullTcp/Tahoe");
			cout << "Created FULLTCP Tahoe Agent" << endl;
			break;
		case RENOFULLTCP : tcl.evalf("eval new Agent/TCP/FullTcp/Newreno");
			cout << "Created FULLTCP Newreno Agent" << endl;
			break;
		case SACKFULLTCP : tcl.evalf("eval new Agent/TCP/FullTcp/Sack");
			cout << "Created FULLTCP Sack Agent" << endl;
			break;
		default : protocolError("TCPSocketAgent: No TCP Identifier Specified");
	}
	
	const char *tcpVar = tcl.result();
	 
    tcpAgent = (FullTcpAgent *)tcl.lookup(tcpVar);

	cout << "tcpAgent Name " << tcpAgent->name() << endl;

    if (tcpAgent==NULL) {
		cerr << "TCPSocketAgent: Cannot instantiate TCP Agent " << tcpVar << endl;
		exit(1);
		}
	    	
	createTCPApplication(tcpAgent);
		
	// hook up the callback mechanism from the TCP agents to the Apps
	
	switch (theTCPProtocol) {
		case FULLTCP : ((TCPFullWithEvents *)tcpAgent)->setTCPSocketAgent(this);
			tcpAgentInterface = (TCPAgentInterface *)((TCPFullWithEvents *)tcpAgent);
	 		break;
		case TAHOEFULLTCP : 
			break;
		case RENOFULLTCP : 
			break;
		case SACKFULLTCP : 
			break;
		case ERROR : 
			return false;
	}
	
	return true;
	}

void TCPSocketAgent::protocolError(const char *theTCPProtocolString) {
	cerr << "TCPSocketAgent: Unable to identify TCP Protocol String" << theTCPProtocolString << endl;
	cerr << "TCPSocketAgent: Allowed Types are " << endl;
	cerr << "FULLTCP, RENOFULLTCP, SACKFULLTCP, TAHOEFULLTCP" << endl;
	exit(1);
}

/**
 * Creates the TCP application for sending data between TCP nodes. 
 */
bool TCPSocketAgent::createTCPApplication(FullTcpAgent *theTCPAgent) {
	
	Tcl& tcl = Tcl::instance();    

	tcl.evalf("eval new Application/TCPSocketApp %s", theTCPAgent->name());
 	
	const char *tcpVar = tcl.result();
	
    tcpSocketApp = (TCPSocketApp *)tcl.lookup(tcpVar);

	// Create Socket app and binds the TCPSocketApp with the underlying TCP protocol

	cout << "Created Socket App " << tcpSocketApp->name() << " on node " << theTCPAgent->addr() << endl;
		
	tcpSocketApp->addTCPSocketAgentListener(this); // add ourselves asas a listener for events - i.e. we get notified
 
	return true;
    }
	
void TCPSocketAgent::setToCurrentTCPAgent() { 
	currentAgent=this; 
	} // make the connecting TCPSocketAgent point to this socket for connection
	
int TCPSocketAgent::command(int argc, const char*const* argv) {
	Tcl& tcl = Tcl::instance();    
	cout << "Node = " << name() << ", command: " << argv[1] << "," ;

    for (int i =0; i<argc; ++i)
		cout << " arg[" << i << "] = " << argv[i] << ", ";

	cout << endl;

	if (argc==2) {
		if (strcmp(argv[1], "listen") == 0) {
			listen(port());
			return (TCL_OK);
			}
	} else if (argc==3) {
		if (strcmp(argv[1], "attach-to-node") == 0) {
			attachTCPAgentToNode(argv[2]); 
			return (TCL_OK);
		} else if (strcmp(argv[1], "tcp-connect") == 0) {
			TCPSocketAgent *tcpSocket = (TCPSocketAgent *)TclObject::lookup(argv[2]);
			if (tcpSocket == NULL) {
				tcl.resultf("%s: connected to null object.", name_);
				return (TCL_ERROR);
			}
			connect(tcpSocket);
 		
			return (TCL_OK);
		}
	} else if (argc==4) {
		// Pass all the set commands to the TCL agent e.g. $tcp1 set window_ 100
		if (strcmp(argv[1], "set-var") == 0) { 
			setTCPParameter(argv[2], argv[3]);
		
			return (TCL_OK);
		} else if (strcmp(argv[1], "tcp-connect") == 0) {
			cout << "Trying to connect " << argv[2] << " port " << argv[2] << endl;
			nsaddr_t address = atoi(argv[2]);
			nsaddr_t port = atoi(argv[3]);
			connect(address,port);
			return (TCL_OK);
		} else if (strcmp(argv[1], "send") == 0) {
			const char *bytes = argv[3];
			int size = atoi(argv[2]);
		
			cout << "Sending " << bytes << ", size " << size << " from node " << getTCPAgent()->addr() << endl;
		
			send(size,bytes); // the socketapp actually sends the num bytes to the protocol through our previous app 
							  // attachment layer. We just have to pass the data across.
		
			return (TCL_OK);
		}
	}
	
	return Agent::command(argc, argv);
}    

