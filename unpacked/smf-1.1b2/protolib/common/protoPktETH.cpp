#include "protoPktETH.h"

ProtoPktETH::ProtoPktETH(UINT32* bufferPtr, unsigned int numBytes, bool freeOnDestruct)
 : ProtoPkt(bufferPtr, numBytes, freeOnDestruct)
{    
}

ProtoPktETH::~ProtoPktETH()
{
}
