/*
 *  TcpDataBuffer.cpp
 *
 *  Created by Ian Taylor on 28/12/2006.
 *
 */  
  
#include "TcpDataFIFO.h"

TcpDataFIFO::TcpDataFIFO(AppData *c, int nbytes)
{
	nbytes_ = nbytes;
	size_ = c->size();
  	if (size_ > 0) 
		data_ = c;
  	else 
  		data_ = NULL;
	next_ = NULL;
}
 
TcpDataFIFOList::~TcpDataFIFOList() 
{
	while (head_ != NULL) {
		tail_ = head_;
		head_ = head_->next_;
		delete tail_;
	}
}

void TcpDataFIFOList::insert(TcpDataFIFO *TcpDataFIFO) 
{
	if (tail_ == NULL) 
		head_ = tail_ = TcpDataFIFO;
	else {
		tail_->next_ = TcpDataFIFO;
		tail_ = TcpDataFIFO;
	}
#ifdef TcpAppInterface_DEBUG
	num_++;
#endif
}

TcpDataFIFO* TcpDataFIFOList::detach()
{
	if (head_ == NULL)
		return NULL;
	TcpDataFIFO *p = head_;
	if ((head_ = head_->next_) == NULL)
		tail_ = NULL;
#ifdef TcpAppInterface_DEBUG
	num_--;
#endif
	return p;
}
