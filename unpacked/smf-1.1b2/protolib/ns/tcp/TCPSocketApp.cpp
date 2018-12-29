
//
// Tcp application interface: extension of the TCPApp code to do duplex connections to transmit 
// real application data and also allow multiple TCP connections
// 
// The single connections model underlying TCP connection as a 
// FIFO byte stream, use this to deliver user data
 
#include "agent.h"
#include "app.h"
#include "TCPSocketApp.h"

 
// TCPSocketApp that accept TCP agents as a parameter

static class TcpAppIntClass : public TclClass {
public:
	TcpAppIntClass() : TclClass("Application/TCPSocketApp") {}
	TclObject* create(int argc, const char*const* argv) {
		if (argc != 5)
			return NULL;
		Agent *tcp = (Agent *)TclObject::lookup(argv[4]);
		if (tcp == NULL) 
			return NULL;
		return (new TCPSocketApp(tcp));
	}
} class_TCPSocketApp;

// Socket Apps that don't have any underlying TCP protocol defined - these are used
// in the dynamic scenario, where we generate these.

TCPSocketApp::TCPSocketApp(Agent *tcp) : 
	Application(), curdata_(0), curbytes_(0)
{
	setTCPAgent(tcp);
}



TCPSocketApp::~TCPSocketApp()
{
	// XXX Before we quit, let our agent know what we no longer exist
	// so that it won't give us a call later...
	agent_->attachApp(NULL);
}

void TCPSocketApp::setTCPAgent(Agent *tcp) {
	cout << "TCPSocketApp: setting Agent to " << tcp->name() << " and attaching app " << endl;
	
	agent_ = tcp;
	agent_->attachApp(this);
}

/**   
 * Send with calls to add data to this pipe by adding to a FIFO buffer before allowing
 * the underlying TCP implementation to make a simulated transfer.
 */
void TCPSocketApp::send(int nbytes, AppData *cbk)
{
	TcpDataFIFO *p = new TcpDataFIFO(cbk, nbytes);
#ifdef TCPSocketApp_DEBUG
	p->time() = Scheduler::instance().clock();
#endif
	TcpDataFIFO_.insert(p);

// send number of bytes to the underlying protocol i.e. FullTCP or whatever

	Application::send(nbytes);

	// notify the TCP socket agent with a SEND event (i.e. buffer is ready to write to as its empty now)
	
	if (tcpSocketListenerAgent) tcpSocketListenerAgent->tcpEventReceived(new TCPEvent(TCPEvent::SEND, NULL, 0));
}
 
// All we need to know is that our sink has received one message
void TCPSocketApp::recv(int size) {		
	// If it's the start of a new transmission, grab info from dest, 
	// and execute callback
	if (curdata_ == 0)
		curdata_ = dst_->rcvr_retrieve_data();
	if (curdata_ == 0) {
		fprintf(stderr, "[%g] %s receives a packet but no callback!\n",
			Scheduler::instance().clock(), name_);
		return;
	}
	curbytes_ += size;
#ifdef TCPSocketApp_DEBUG
	fprintf(stderr, "[%g] %s gets data size %d, %s\n", 
		Scheduler::instance().clock(), name(), curbytes_, 
		curdata_->data());
#endif
	if (curbytes_ == curdata_->bytes()) {
		// We've got exactly the data we want
		// If we've received all data, execute the callback
		process_data(curdata_->size(), curdata_->data());
		// Then cleanup this data transmission
		delete curdata_;
		curdata_ = NULL;
		curbytes_ = 0;
	} else if (curbytes_ > curdata_->bytes()) {
		// We've got more than we expected. Must contain other data.
		// Continue process callbacks until the unfinished callback
		while (curbytes_ >= curdata_->bytes()) {
			process_data(curdata_->size(), curdata_->data());
			curbytes_ -= curdata_->bytes();
#ifdef TCPSocketApp_DEBUG
			fprintf(stderr, 
				"[%g] %s gets data size %d(left %d)\n", 
				Scheduler::instance().clock(), 
				name(),
				curdata_->bytes(), curbytes_);
				//curdata_->data());
#endif
			delete curdata_;
			curdata_ = dst_->rcvr_retrieve_data();
			if (curdata_ != 0)
				continue;
			if ((curdata_ == 0) && (curbytes_ > 0)) {
				fprintf(stderr, "[%g] %s gets extra data!\n",
					Scheduler::instance().clock(), name_);
				Tcl::instance().eval("[Simulator instance] flush-trace");
				abort();
			} else
				// Get out of the look without doing a check
				break;
		}
	}
}

void TCPSocketApp::resume()
{
	// Do nothing
}

int TCPSocketApp::command(int argc, const char*const* argv)
{
	Tcl& tcl = Tcl::instance();

	cout << "Command: " << argv[1] << endl;

	if (strcmp(argv[1], "connect") == 0) {
		dst_ = (TCPSocketApp *)TclObject::lookup(argv[2]);
		if (dst_ == NULL) {
			tcl.resultf("%s: connected to null object.", name_);
			return (TCL_ERROR);
		}
		dst_->connect(this);
		return (TCL_OK);
	} else if (strcmp(argv[1], "send") == 0) {
		
		const char *bytes = argv[3];
		int size = atoi(argv[2]);
		
		cout << "Sending " << bytes << ", size " << size << " from node " << getTCPAgent()->addr() << endl;
		
		if (argc == 3)
			send(size, NULL);
		else {
			TcpData *tmp = new TcpData();
			tmp->setBytes(bytes, size);
			send(size, tmp);
		}
		 
		return (TCL_OK);

	} else if (strcmp(argv[1], "dst") == 0) {
		tcl.resultf("%s", dst_->name());
		return TCL_OK;
	}
	return Application::command(argc, argv);
}
 
/**
 * Application Callback HERE:
 * 
 * process_data() is called when data is delivered from the TCP agent. This effectively is the callback 
 * an application gets from an agent when data arrives. Since TCPSocketApp uses the application
 * Ns2 interface, then this method will be called upon receipt of data the the Ns-2 node.   
 */
void TCPSocketApp::process_data(int size, AppData* data) 
{
	if (data == NULL)
		return;
		
	TcpData *tmp = (TcpData*)data;
    

	// XXX Default behavior:
	// If there isn't a target, use tcl to evaluate the data
	if (target())
		send_data(size, data);
	else if (tcpSocketListenerAgent) // pass it along
		tcpSocketListenerAgent->tcpEventReceived(new TCPEvent(TCPEvent::RECEIVE, tmp->getBytes(), tmp->getDataSize()));
	else {
		cout << "TCPSocketApp: Receiving " << tmp->getBytes() << " of size " << tmp->getDataSize()  << " at node " << getTCPAgent()->addr() << endl;
	}
}
