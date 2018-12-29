/*
 *  TCPProtoSocketAgent.h
 *
 *  Copyright 2007 __MyCompanyName__. All rights reserved.
 *
 */

#ifndef TCPProtoSocketAgent_h
#define TCPProtoSocketAgent_h

#include <agent.h>
#include <ip.h>

#include "./tcp/TCPSocketAgent.h"
#include "./tcp/TCPServerSocketAgent.h"
#include "nsProtoSimAgent.h"

class NsTCPProtoSocketAgent : public Agent, NsProtoSimAgent::NSSocketProxy, TCPSocketAgentListener {
		
	private:
		// either a server or a TCP pipe i.e. one to one connection. Once connected we deal with TCPSockets directly.
		// every socket becomes a TCPSocket after the first conneciton request. The server side doesn't care whether 
		// its a server or not, it just receives and writes to a TCP pipe.
		
		TCPServerSocketAgent *serverSocket;
		TCPSocketAgent *tcpPipe;

		// for clients, the state changes from NotInitialised to TcpSocket
		// for servers, the state changes from NotInitialised to TcpServerSocket, then finally to TcpSocket once a connection
		// has been made
		enum SocketType {TcpServerSocket, TcpSocket, NotInitialised};  
	
		SocketType socketType_;
		bool connected_;
		
	public:
		NsTCPProtoSocketAgent() : Agent(PT_TCP) { socketType_=NotInitialised; connected_=false; 	
			printf("TCPProtoSocketAgent: Created\n");
		}
                
		bool SendTo(const char* buffer, unsigned int& numBytes, const ProtoAddress& dstAddr);
		
// implemnted from NSSocketProxy for TCP 

		bool Connect(const ProtoAddress& theAddress);
		
		bool Accept(ProtoSocket* theSocket);

		bool Listen(UINT16 thePort);
		
		// What do we do here for groups?
		
		bool JoinGroup(const ProtoAddress& groupAddr) { return true; }
        bool LeaveGroup(const ProtoAddress& groupAddr) { return true; }

		void setTCPSocketAgent(TCPSocketAgent *tcpSocketAgent) { tcpPipe=tcpSocketAgent; }
	
		void attachTCPSocketAgent(); 
	
		// My callback from the TCPSocketAgents

	    bool tcpEventReceived(TCPEvent *event); // basic implementation here - override in your app specific one
                
};  // end class TCPProtoSocketAgent

#endif