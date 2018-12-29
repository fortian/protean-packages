
/**
  TCPSocketApp: A class to use the ADU NS-2 functionality to implement
  a bi directional TCP pipe for transfering data over an NS-2 TCP connection.
  The connection is abstracted as a virtual pipe because rather than extending
  a TCP protocol to implement these extensions, we implement the sending of data
  as a separate entity which simply uses an external buffer at the application-level
  to buffer the data. TCPSocketApp then waits until NS-2 simulates the 
  transfer before retrieving the data from the pipe at thje receiver side of
  the connection.
  
  TCPSocketApp subclasses from Application as the pipe is modelled as a
  connection between two applications wishing to transfer data using a TCP
  protocol. We use the exisisting NS-2 mechanism for representing application
  data (ADU - Application Data Unit) and implement other necessities for 
  TCP real-world applications, ike the ability to send data ion both 
  directions. This class basically extends the existing tcpapp code in NS-2
  to enhance that functionality.    
  
  TCPSocketApp model the TCP connection as a FIFO byte stream. It shouldn't 
  be used if this assumption is violated.
*/
 
#ifndef ns_TCPSocketApp_h
#define ns_TCPSocketApp_h

#include <iostream>

#include "app.h"
#include "TcpData.h"
#include "TcpDataFIFO.h"

#include "TCPEvent.h"

#include <tcp-full.h>

class TCPSocketApp : public Application {
public:	
	TCPSocketApp(Agent *tcp);
	~TCPSocketApp();

	void setTCPAgent(Agent *tcp);
	
	void addTCPSocketAgentListener(TCPSocketAgentListener *listener) { tcpSocketListenerAgent = listener; }

	FullTcpAgent *getTCPAgent() { return (FullTcpAgent *)agent_; }
		
	virtual void recv(int nbytes);
	virtual void send(int nbytes, AppData *data);

	void connect(TCPSocketApp *dst) { // connect both ways for two way transfer
									  dst_ = dst; dst->dst_=this; }

	virtual void process_data(int size, AppData* data);
	virtual AppData* get_data(int&, AppData*) {
		// Not supported
		abort();
		return NULL;
	}

	// Do nothing when a connection is terminated
	virtual void resume();

protected:
	virtual int command(int argc, const char*const* argv);
	TcpDataFIFO* rcvr_retrieve_data() { return TcpDataFIFO_.detach(); }

	// We don't have start/stop
	virtual void start() { abort(); } 
	virtual void stop() { abort(); }

private:

	TCPSocketApp *dst_;
	TcpDataFIFOList TcpDataFIFO_;
	TcpDataFIFO *curdata_;
	int curbytes_;
	TCPSocketAgentListener *tcpSocketListenerAgent;
	
};

#endif // ns_TCPSocketApp_h
