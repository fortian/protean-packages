/**
 *  TCPServerSocketAgent.h
 *
 *  Created by Ian Taylor on 01/03/2007.
 *  
 * A TCPServerSocketAgent to provides the server capabilities for a TCP connection.
 * The server socket contains capabilities to host multiple connections simulataneously through the
 * use of a demultiplexor that route the messages to and from the various TCPSocket agents. 
 */

#include "TCPSocketAgent.h"

class SocketList {
public:
	SocketList();
	
	void prepend(TCPSocketAgent *broker);
	void remove(TCPSocketAgent *broker);
	TCPSocketAgent* findSocket(nsaddr_t address, nsaddr_t port);						
	void removeSockets();
	
private: 
	TCPSocketAgent* head;
}; 

class TCPServerSocketAgent: public Agent {		
public:

// Constructors

	TCPServerSocketAgent(TCPSocketAgent::TcpProtocol theTCPProtocol, nsaddr_t thePort) : Agent(PT_TCP) { 
		theTCPProtocol_=theTCPProtocol; 
		port()=thePort; 
		mySocketList=new SocketList();
		}

	TCPServerSocketAgent(const char *theTCPProtocolString, nsaddr_t thePort) : Agent(PT_TCP) { 
		theTCPProtocol_=TCPSocketAgent::getTCPProtocolFromString(theTCPProtocolString); 
		port()=thePort; 
		mySocketList=new SocketList();
		}

	~TCPServerSocketAgent() { mySocketList->removeSockets(); delete mySocketList; }
		

// Methods

	/**
	 * Attaches a listener to this socket - note that TCPSocketAgents only support ONE listener 
	 * for events from this socket.
	 */
	void attachListener(TCPSocketAgentListener *listener) { tcpListener_=listener; }

	bool attachTCPAgentToNode(const char *node);

	TCPSocketAgent* accept();
	
	void listenOn(int thePort) {} // nothing to do - this is set up by accept. Aserver socket is always listening

	void recv(Packet *pkt, Handler *handler);
	
	int command(int argc, const char*const* argv);
	
private:		

	Packet *synPacket_;
	TCPSocketAgentListener *tcpListener_;
	char *nodeNameInTCL_;
	TCPSocketAgent::TcpProtocol theTCPProtocol_;
	
	SocketList *mySocketList;
};

