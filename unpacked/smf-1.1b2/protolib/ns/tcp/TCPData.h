/*
 *  TcpData.h
 *
 *  Created by Ian Taylor on 28/12/2006.
 *
 */

// ADU for plain TCPBiDirectionalPipe, which is by default a bytesing of otcl script
// XXX Local to this file

#include "ns-process.h"

class TcpData : public AppData {
private:
	int size_;
	char* bytes_; 
public:
	TcpData() : AppData(TCPAPP_STRING), size_(0), bytes_(NULL) {}
	
	TcpData(TcpData& d);
	
	virtual ~TcpData() { 
		if (bytes_ != NULL) 
			delete []bytes_; 
	}

// this is the size of the TcpData object as a whole - not sure why this is calculated 
// this way - todo figure out if this is correct and if NS-2 cares about this setting - if not, 
// then we can change this to return just size_ like getDataSize below

	virtual int size() const { return AppData::size() + size_; }

	char* getBytes() { return bytes_; }

	void setBytes(const char* s, int size);

	int getDataSize() { return size_; }
	
	virtual AppData* copy() {
		return new TcpData(*this);
	}
};