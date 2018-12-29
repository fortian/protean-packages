For the TCP implementation, you just need to add the following files to the existing protolib definitions in Makefile.in:

	$(PROTOLIB)/ns/tcp/TCPData.o \
	$(PROTOLIB)/ns/tcp/TCPDataFIFO.o \
	$(PROTOLIB)/ns/tcp/TCPSocketApp.o \
	$(PROTOLIB)/ns/tcp/TCPEvent.o \
	$(PROTOLIB)/ns/tcp/TCPSocketAgent.o \
	$(PROTOLIB)/ns/tcp/TCPServerSocketAgent.o \
	$(PROTOLIB)/ns/tcp/TCPSocketExample.o \
	$(PROTOLIB)/ns/nsTCPProtoSocketAgent.o 
