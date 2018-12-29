/*
 *  TCPProtoSocketAgent.cpp
 *
 *  Copyright 2007 __MyCompanyName__. All rights reserved.
 *
 */
 
/**
 * This is the connection ID which is incremented every time there's a new connection
 * request by calling connect. This is used to "label" the connections so that
 * the sends can be de-multiplexed at the receiving TCP server and sent to the
 * correct TCP connection responsible for that connection. 
 */
static int protoSocketConnectionID=0;

#include "nsTCPProtoSocketAgent.h"

static class ProtoSocketTCPAgentClass : public TclClass {
public:
	ProtoSocketTCPAgentClass() : TclClass("Agent/ProtoSocket/TCP") {}
	TclObject* create(int, const char*const*) {
		return (new NsTCPProtoSocketAgent());
	}
} class_proto_socket_TCP_agent;

 
                
bool NsTCPProtoSocketAgent::SendTo(const char* buffer, unsigned int& numBytes, const ProtoAddress& dstAddr) {
	if (!connected_)
		Connect(dstAddr);
	// once connected, we have to be a pipe.	
	tcpPipe->send(numBytes,buffer);
	return true;
}	
	
bool NsTCPProtoSocketAgent::Connect(const ProtoAddress& theAddress) { 
	printf("TCPProtoSocketAgent: Connecting\n");
	
	if (socketType_==NotInitialised) { // must be a client socket, port is set from the NsProtoSimAgent bind operation		
		tcpPipe =new TCPSocketAgent(TCPSocketAgent::FULLTCP, port());
		attachTCPSocketAgent(); // attaches the underlying socket agent to this node
		// connect to server
		tcpPipe->connect(atoi(theAddress.GetRawHostAddress()), theAddress.GetPort());
		connected_=true;
		}
	else {
		return false;
	}
			
	return true; 
	} 
 
void NsTCPProtoSocketAgent::attachTCPSocketAgent() { 
		tcpPipe->attachListener(this);
		
		Agent* simAgent = dynamic_cast<Agent*>(proto_socket->GetNotifier());
		Tcl& tcl = Tcl::instance();	
		tcl.evalf("%s set node_", simAgent->name());
		const char* nodeName = tcl.result();

		tcpPipe->attachTCPAgentToNode(nodeName);
		socketType_=TcpSocket;
		}
		
 /**
  * Here, we create a new protosocket to
  * deal with the new connection and tell this TCPProtoSocketAgent to reply to the requesting
  * client. This allows us to spwan off new agents to deal with multiple clients.
  * NOTE that this assumes that the user provides a socket to deal with the connection !
  */
bool NsTCPProtoSocketAgent::Accept(ProtoSocket* theSocket) { 
	printf("TCPProtoSocketAgent::Accept CALLED \n");
	
	NsTCPProtoSocketAgent *theProtoSocket = (NsTCPProtoSocketAgent *)theSocket;
	
	TCPSocketAgent* socket = serverSocket->accept(); // create new socket agent for this connection
	
	theProtoSocket->setTCPSocketAgent(socket);  // set the socket agent in the new TCPProtoSocketAgent to be 
	theProtoSocket->attachTCPSocketAgent(); // attaches new socket to the node and changes the state
	
	return false;
	}
 
bool NsTCPProtoSocketAgent::Listen(UINT16 thePort) {
	if (socketType_==NotInitialised) { // must be a client socket, port is set from the NsProtoSimAgent bind operation
		printf("TCPProtoSocketAgent: Listening\n");
		serverSocket = new TCPServerSocketAgent(TCPSocketAgent::FULLTCP, thePort);
		serverSocket->attachListener(this);
		
		printf("TCPProtoSocketAgent: Getting node Reference\n");

		Agent* simAgent = dynamic_cast<Agent*>(proto_socket->GetNotifier());
		Tcl& tcl = Tcl::instance();	
		tcl.evalf("%s set node_", simAgent->name());
		const char* nodeName = tcl.result();

		printf("TCPProtoSocketAgent: Attaching server socket to node\n");
		serverSocket->attachTCPAgentToNode(nodeName);
		socketType_=TcpServerSocket;
		
		printf("TCPProtoSocketAgent: Calling listen on server socket\n");

		// start lisening for connections
		serverSocket->listenOn(thePort);
		}
	else {
		return false;
	}

	return true;
	}
 
/**
 * Catches events thrown from the underlying TCP agents. 
 */
bool NsTCPProtoSocketAgent::tcpEventReceived(TCPEvent *event) {

	// check how these map ...
	
	if (event->getType()==TCPEvent::ACCEPT) {
		if (proto_socket)
			proto_socket->OnNotify(ProtoSocket::NOTIFY_INPUT);  
	} else if (event->getType()==TCPEvent::CONNECTED) {
		if (proto_socket)
			proto_socket->OnNotify(ProtoSocket::NOTIFY_INPUT);  
	} else if (event->getType()==TCPEvent::SEND) {
		if (proto_socket)
			proto_socket->OnNotify(ProtoSocket::NOTIFY_OUTPUT);  
	} else if (event->getType()==TCPEvent::RECEIVE) {
		recv_data = (char *)event->getData();
		recv_data_len = event->getDataSize();
		if (proto_socket)
			proto_socket->OnNotify(ProtoSocket::NOTIFY_INPUT);  
		// or	proto_socket->OnNotify(ProtoSocket::RECV); ???  
	} else if (event->getType()==TCPEvent::DISCONNECTED) {
		if (proto_socket)
			proto_socket->OnNotify(ProtoSocket::NOTIFY_OUTPUT);  
	} else {
		printf("TCPProtoSocketAgent UNKNOWN Event on node %i\n", addr());
		return false;
		}
	return true;
}
