#ifndef TCPDataFifo_h
#define TCPDataFifo_h

/*
 *  TcpDataBuffer.h
 *
 *  Created by Ian Taylor on 28/12/2006.
 *
 */
#include "ns-process.h"

class TcpDataFIFO { 
public:
	TcpDataFIFO(AppData *c, int nbytes);

	~TcpDataFIFO() {
		if (data_ != NULL)
			delete data_;
	}
	AppData* data() { return data_; }
	int size() { return size_; }
	int bytes() { return nbytes_; }

#ifdef TcpAppInterface_DEBUG	
	// debug only
	double& time() { return time_; }
#endif
 
protected:
	friend class TcpDataFIFOList;
	AppData *data_;
	int size_;
	int nbytes_; 	// Total length of this transmission
	TcpDataFIFO *next_;

#ifdef TcpAppInterface_DEBUG
	// for debug only 
	double time_; 
#endif
};

// A FIFO queue
class TcpDataFIFOList {
public:	
#ifdef TcpAppInterface_DEBUG 
	TcpDataFIFOList() : head_(NULL), tail_(NULL), num_(0) {}
#else
	TcpDataFIFOList() : head_(NULL), tail_(NULL) {}
#endif
	~TcpDataFIFOList();

	void insert(TcpDataFIFO *TcpDataFIFO);
	TcpDataFIFO* detach();

protected:
	TcpDataFIFO *head_;
	TcpDataFIFO *tail_;
#ifdef TcpAppInterface_DEBUG
	int num_;
#endif
};

#endif