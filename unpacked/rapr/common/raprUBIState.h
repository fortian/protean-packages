#ifndef _RAPR_UBI_STATE
#define _RAPR_UBI_STATE
#include "deque"

class RaprUBIStateStore {
	public:
		RaprUBIStateStore();
		~RaprUBIStateStore();
		unsigned int GetUBI() {return ubi;}
		void SetUBI(unsigned int inUBI) {ubi = inUBI;}
		int GetState() {return state;}
		void SetState(int inState) {state = inState;}
	private:
	   unsigned int ubi;
		int state;
};

class RaprUBIState {
	public:
		RaprUBIState();
		~RaprUBIState();
	   static const int MAXSIZE = 10000;
		typedef _STL::deque<RaprUBIStateStore *> UBIStateStorage;
	   int GetState(unsigned int inUBI);
		void SetState(unsigned int inUBI,int inState);
	private:
	   UBIStateStorage::iterator find(unsigned int inUBI);
	   UBIStateStorage storage;
};

#endif //_RAPR_UBI_STATE

