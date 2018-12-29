/*
 *  TcpData.cpp
 *
 *  Created by Ian Taylor on 28/12/2006.
 *
 */  
 
#include "TcpData.h"
#include <iostream>

TcpData::TcpData(TcpData& d) : AppData(d) {
		size_ = d.size_;
		if (size_ > 0) {
			bytes_ = new char[size_];
			memcpy(bytes_, d.bytes_, size_);
		} else
			bytes_ = NULL;
	} 
	 
void TcpData::setBytes(const char* b, int size) {
		if ((b == NULL) || (*b == 0)) 
			bytes_ = NULL, size_ = 0;
		else {
			size_ = size;
			bytes_ = new char[size_];
			assert(bytes_ != NULL);
			memcpy(bytes_, b, size_);
			// cout << "SETTING BYTES " << bytes_ << ", size " << size_ << endl;
		}
	}