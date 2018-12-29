/*
 *  TCPSocketAgent.h
 *
 *  Created by Ian Taylor on 01/01/2007.
 *
 */

/**
	A TCPSocketAgent is an agent that is capable of creating Sockets on-the-fly for allowing multiple TCP connections.
*/

#ifndef TCPSocketAgent_h
#define TCPSocketAgent_h

#include <iostream>

#include <agent.h>
#include <ip.h>
#include <tcp-full.h>

#include <protoAddress.h>
#include <protoSocket.h>

#include "TCPSocketApp.h"
#include "TCPEvent.h"

class TCPAgentInterface  {
public:
	virtual nsaddr_t getPort() = 0;
	virtual nsaddr_t getAddress() = 0;
	virtual void setPort(nsaddr_t thePort) = 0;
	virtual void setAddress(nsaddr_t theAddress) = 0;
	virtual nsaddr_t getDestinationPort() = 0;
	virtual nsaddr_t getDestinationAddress() = 0;
	virtual void setDestinationPort(nsaddr_t destPort) = 0;
	virtual void setDestinationAddress(nsaddr_t destAddress) = 0;
	
	virtual void listenOn(nsaddr_t port) = 0;
	virtual void close() = 0;
	virtual ~TCPAgentInterface() {}
};

class TCPSocketAgent : public Agent, TCPSocketAgentListener {		
public:
	// Allowed TCP protocol types and constructors
	
	enum TcpProtocol {FULLTCP, RENOFULLTCP, SACKFULLTCP, TAHOEFULLTCP, ERROR};  

	TCPSocketAgent(TcpProtocol theTCPProtocol, nsaddr_t thePort) : Agent(PT_TCP) { createTCPAgent(theTCPProtocol); setPort(thePort); }

	TCPSocketAgent(const char *theTCPProtocolString, nsaddr_t thePort);

	TCPSocketAgent(FullTcpAgent *theTCPAgent, nsaddr_t thePort) : Agent(PT_TCP), tcpAgent(theTCPAgent) {setPort(thePort);}
		
	~TCPSocketAgent() { // send a DISCONNECTED event - this will be called when a close of something happens
		cout << "TCPSocketAgent destructor called " << endl; 
		tcpEventReceived(new TCPEvent(TCPEvent::DISCONNECTED, NULL, 0));}

	// others

	bool connect(TCPSocketAgent *tcpSocket);

	bool connect(nsaddr_t destAddress, nsaddr_t destPort);
	
	void send(int nbytes, const char *data);
	
	/**
	 * Attaches a listener to this socket - note that TCPSocketAgents only support ONE listener 
	 * for events from this socket.
	 */
	void attachListener(TCPSocketAgentListener *listener) { tcpListener_=listener; }
		
	int command(int argc, const char*const* argv);

	FullTcpAgent *getTCPAgent() { return tcpAgent; }
	TCPAgentInterface *getTCPInterface() { return tcpAgentInterface; }
	TCPSocketApp *getTCPApp() { return tcpSocketApp; }

    // Implementation of the TCPSocketAgentListener interface - callback - where events pass through

	virtual bool tcpEventReceived(TCPEvent *event); // basic implementation here - override in your app specific one

	// Proxy methods to the underlying protocol
	
	nsaddr_t getPort() { return tcpAgentInterface ? tcpAgentInterface->getPort() : -1; } 
	nsaddr_t getAddress() { return tcpAgentInterface ? tcpAgentInterface->getAddress(): -1; } 	
	void setPort(nsaddr_t thePort) { if (tcpAgentInterface) tcpAgentInterface->setPort(thePort); } 
	void setAddress(nsaddr_t theAddress) { if (tcpAgentInterface) tcpAgentInterface->setAddress(theAddress); } 	

	nsaddr_t getDestinationPort() { return tcpAgentInterface ? tcpAgentInterface->getDestinationPort() : -1; } 
	nsaddr_t getDestinationAddress() { return tcpAgentInterface ? tcpAgentInterface->getDestinationAddress(): -1; } 	
	void setDestinationPort(nsaddr_t thePort) { if (tcpAgentInterface) tcpAgentInterface->setDestinationPort(thePort); } 
	void setDestinationAddress(nsaddr_t theAddress) { if (tcpAgentInterface) tcpAgentInterface->setDestinationAddress(theAddress); } 	

	static TcpProtocol getTCPProtocolFromString(const char *theTCPProtocolString); 			
	bool createTCPAgent(TcpProtocol theTCPProtocol);
	bool attachTCPAgentToNode(const char *node);

	void setToCurrentTCPAgent(); // make the connecting TCPSocketAgent point to this socket for connection

	TCPSocketAgent* getPrev() {return prev;}
    void setPrev(TCPSocketAgent* broker) {prev = broker;}
    TCPSocketAgent* getNext() {return next;}
    void setNext(TCPSocketAgent* broker) {next = broker;}
	
protected:		
	TCPSocketAgent() : Agent(PT_TCP) {  }

	// The TCP Protocol Agent
	FullTcpAgent *tcpAgent;
	
	TCPAgentInterface *tcpAgentInterface;
	
	// The TCP Socket App
	TCPSocketApp *tcpSocketApp;
	
	// The TCP Socket agent this agent is connected to
	
	TCPSocketAgent *tcpSocketConnected;
	TcpProtocol theTCPProtocol_;
	char *nodeNameInTCL_;

private:
	TCPSocketAgent *prev;    
	TCPSocketAgent *next;    

	void listen(nsaddr_t port) { if(tcpAgentInterface) tcpAgentInterface->listenOn(port); }

	TCPSocketAgentListener *tcpListener_;
	 
	bool createTCPApplication(FullTcpAgent *theTCPAgent);
	void setTCPParameter(const char *parName, const char *parValue);
	
	void protocolError(const char *theTCPProtocolString);
};

class TCPFullWithEvents : public FullTcpAgent, TCPAgentInterface {

private:
	TCPSocketAgentListener *tcpsocket_;

public:

	TCPFullWithEvents() : FullTcpAgent() { cout << "TCP With Events !" << endl; }

	void listenOn(nsaddr_t onPort) { port()=onPort; 
										listen(); }
	void close() { close(); }

	void setDestinationPort(nsaddr_t destPort) { dport()=destPort; }
	void setDestinationAddress(nsaddr_t destAddress) { daddr()=destAddress; }

	void setPort(nsaddr_t thePort) { port()=thePort; }
	void setAddress(nsaddr_t theAddress) { addr()=theAddress; }

	nsaddr_t getPort() { return port(); }
	nsaddr_t getAddress() { return addr(); }

	nsaddr_t getDestinationPort() { return dport(); }
	nsaddr_t getDestinationAddress() { return daddr(); }

	void setTCPSocketAgent(TCPSocketAgentListener *tcpsocket) { tcpsocket_=tcpsocket; }

	~TCPFullWithEvents() { cancel_timers(); rq_.clear(); }
	
	/**
	 * Olverride the TCP full and add detection of Connect SYN and ACK packets so we can notify our 
	 * application of the progress of a connection.  
	 */
	void recv(Packet *pkt, Handler *handler) {
		int tiflags = hdr_tcp::access(pkt)->flags() ; 		 // tcp flags from packet

	//	if ((state_ == TCPS_LISTEN) && (tiflags & TH_SYN)) {
	//		tcpsocket_->tcpEventReceived(new TCPEvent(TCPEvent::ACCEPT, pkt, 0));
	//		}
	//	else 
		if ((state_ == TCPS_SYN_SENT) && (tiflags & TH_ACK)) {
			printf("TCPSocketAgent CONNECTED Event\n");
			tcpsocket_->tcpEventReceived(new TCPEvent(TCPEvent::CONNECTED, NULL, 0));
  		    }

		FullTcpAgent::recv(pkt, handler);
	}
};

#endif 
