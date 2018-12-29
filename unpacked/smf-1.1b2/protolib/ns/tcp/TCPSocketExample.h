/*
 *  TCPSocketExample.h
 *
 *  Created by Ian Taylor on 01/03/2007.
 *
 */

#include <agent.h>
#include <ip.h>

#include "TCPSocketAgent.h"
#include "TCPServerSocketAgent.h"

// C++ example ... work in progress
// need to solve locating the TCL reference for a node so we can bind to it 

class TCPSocketExample : public Agent {
	TCPSocketAgent *client;
	TCPServerSocketAgent *server;

	TCPSocketExample() : Agent(PT_UDP) {}
	
	~TCPSocketExample() {}
	
	void createClient() {
		// client = new TCPSocketAgent(
	}
	
};