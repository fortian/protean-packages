
#include "manetMsg.h"

ManetTlv::ManetTlv()
{
}

ManetTlv::~ManetTlv()
{
}


bool ManetTlv::InitIntoBuffer(UINT8 type, UINT8 semantics, char* bufferPtr, unsigned numBytes)
{
    unsigned int minLength = GetMinLength(semantics);
    if (NULL != bufferPtr)
    {
        if (numBytes < minLength)
            return false;
        else
            AttachBuffer((UINT32*)bufferPtr, numBytes);
    }
    else if (buffer_bytes < minLength)
    {
        return false;
    }
    ((UINT8*)buffer_ptr)[OFFSET_TYPE] = (UINT8)type;
    ((UINT8*)buffer_ptr)[OFFSET_SEMANTICS] = (UINT8)semantics;  
    SetTlvLength(0);
    pkt_length = minLength;
    return true;
}  // end ManetTlv::InitIntoBuffer()


bool ManetTlv::SetValue(char* value, UINT16 valueLength, UINT8 index)
{
    ASSERT(!SemanticIsSet(NO_VALUE));
    ASSERT((0 == index) || SemanticIsSet(MULTIVALUE));
    ASSERT((index >= GetIndexStart()) && (index <= GetIndexStop()));
    index -= index ? GetIndexStart() : 0;  // adjust index (if non-zero)
    UINT16 tlvLength = index * valueLength;
    UINT16 offset = OffsetValue() + tlvLength;
    if (offset+valueLength > buffer_bytes)
        return false;
    else
        memcpy((char*)buffer_ptr + offset, value, valueLength);
    tlvLength += valueLength;
    if ((tlvLength > 255) && !SemanticIsSet(EXTENDED)) return false;
    if (tlvLength > GetTlvLength()) SetTlvLength(tlvLength);
    pkt_length += valueLength;
    return true;
}  // end ManetTlv::SetValue()

bool ManetTlv::InitFromBuffer(char* bufferPtr, unsigned numBytes)
{
    if (NULL != bufferPtr) AttachBuffer((UINT32*)bufferPtr, numBytes);
    UINT8 semantics = (buffer_bytes >= OFFSET_SEMANTICS) ? ((char*)buffer_ptr)[OFFSET_SEMANTICS] : 0;
    unsigned int minLength = GetMinLength(semantics);
    if (buffer_bytes < minLength)
    {
        pkt_length = 0;
        return false;
    }
    else
    {
        pkt_length =  minLength + GetTlvLength();
        return true;
    }
}  // end ManetTlv::InitFromBuffer()

ManetFragmentationTlv::ManetFragmentationTlv()
{
}

ManetFragmentationTlv::~ManetFragmentationTlv()
{
}


        
ManetTlvBlock::ManetTlvBlock()
 : tlv_pending(false)
{
}

ManetTlvBlock::~ManetTlvBlock()
{
}

bool ManetTlvBlock::InitIntoBuffer(char* bufferPtr, unsigned int numBytes)
{   
    tlv_pending = false;
    if (NULL != bufferPtr)
        AttachBuffer((UINT32*)bufferPtr, numBytes);
    if (buffer_bytes >= 2)
    {
        SetTlvBlockLength(0);
        pkt_length = 2;
        return true;
    }
    else
    {
        pkt_length = 0;
        return false;
    }
}  // end ManetTlvBlock::InitIntoBuffer()

ManetTlv* ManetTlvBlock::AppendTlv(UINT8 type, UINT8 semantics)
{
    if (tlv_pending) 
        pkt_length += tlv_temp.GetLength(); 
    tlv_pending = 
        tlv_temp.InitIntoBuffer(type, semantics, (char*)buffer_ptr + pkt_length, buffer_bytes - pkt_length);
    return tlv_pending ? &tlv_temp : NULL;
}  // end ManetTlvBlock::AppendTlv()

bool ManetTlvBlock::AppendTlv(ManetTlv& tlv)
{
    if (tlv_pending) 
    {
        pkt_length += tlv_temp.GetLength(); 
        tlv_pending = false;
    } 
    unsigned int tlvLength = tlv.GetLength();
    unsigned int newLength = pkt_length + tlvLength;
    if (buffer_bytes < newLength) return false;
    memcpy((char*)buffer_ptr+pkt_length, (char*)tlv.GetBuffer(), tlvLength);
    pkt_length += tlvLength;
    return true;
}  // end ManetTlvBlock::AppendTlv()

void ManetTlvBlock::Pack()
{
    ASSERT(pkt_length >= 2);
    if (tlv_pending)
    {
        pkt_length += tlv_temp.GetLength();
        tlv_pending = false;
    }
    SetTlvBlockLength(pkt_length - 2);
}  // end ManetTlvBlock::Pack()

bool ManetTlvBlock::InitFromBuffer(char* bufferPtr, unsigned numBytes)
{
    if (NULL != bufferPtr) AttachBuffer((UINT32*)bufferPtr, numBytes);
    if (buffer_bytes >= 2)
    {
        pkt_length = GetTlvBlockLength() + OFFSET_CONTENT;
        return true;
    }
    else
    {
        pkt_length = 0;
        return false;
    }
}  // end ManetTlvBlock::InitFromBuffer() 

bool ManetTlvBlock::GetNextTlv(ManetTlv& tlv) const
{
    char* currentBuffer = (char*)tlv.GetBuffer();
    unsigned int nextOffset;
    if (NULL == currentBuffer)
        nextOffset = OFFSET_CONTENT;
    else
        nextOffset = currentBuffer - (char*)buffer_ptr + tlv.GetLength();
    if (nextOffset < pkt_length)
        return tlv.InitFromBuffer((char*)buffer_ptr + nextOffset, pkt_length - nextOffset);
    else
        return false;
}  // end ManetTlvBlock::GetNextTlv()       

ManetTlvBlock::Iterator::Iterator(ManetTlvBlock& tlvBlock)
 : tlv_block(tlvBlock)
{
}

ManetTlvBlock::Iterator::~Iterator()
{
}


ManetAddrBlock::ManetAddrBlock()
 : addr_type(ProtoAddress::INVALID), tlv_block_pending(false)
{
}

ManetAddrBlock::ManetAddrBlock(char*        bufferPtr, 
                               unsigned int numBytes, 
                               bool         freeOnDestruct)
 : ProtoPkt((UINT32*)bufferPtr, numBytes, freeOnDestruct), 
   addr_type(ProtoAddress::INVALID), tlv_block_pending(false)
{
}

ManetAddrBlock::~ManetAddrBlock()
{
}


bool ManetAddrBlock::InitIntoBuffer(char* bufferPtr, unsigned int numBytes)
{
    // minLength = num-addr field (1) + head-octet field (1) + tlv-block length (2)
    unsigned int minLength = 2 + 2;
    if (NULL != bufferPtr)
        AttachBuffer((UINT32*)bufferPtr, numBytes);
    if (buffer_bytes < minLength) return false;
    addr_type = ProtoAddress::INVALID;
    SetAddressCount(0);
    ((UINT8*)buffer_ptr)[OFFSET_HEAD_OCTET] = (UINT8)FLAG_NO_TAIL;
    tlv_block.InitIntoBuffer(NULL, 0);
    tlv_block_pending = false;
    pkt_length = 2;  // don't include tlv-block part yet.
    return true;
}  // end ManetAddrBlock::InitIntoBuffer()

bool ManetAddrBlock::SetHead(const ProtoAddress& addr, UINT8 hlen)
{   
    // Re-init since head must be set first
    if (InitIntoBuffer((char*)buffer_ptr, buffer_bytes))
    {
        ASSERT(addr.IsValid());
        if ((hlen > 127) || (hlen > addr.GetLength())) return false;
        // minLength: 1 (num-addr) + 1 (head-octet) + hlen + 2 bytes tlv-block len
        unsigned int minLength = 2 + hlen + 2;
        if (hlen < addr.GetLength()) minLength += 1;  // at least tail-octet also required
        if (buffer_bytes < minLength) return false;
        // Save the addr_type, hlen, and head
        addr_type = addr.GetType();
        ((UINT8*)buffer_ptr)[OFFSET_HEAD_OCTET] = (UINT8)FLAG_NO_TAIL | hlen;
        memcpy(((char*)buffer_ptr)+OFFSET_HEAD, addr.GetRawHostAddress(), hlen);
        pkt_length = OFFSET_HEAD + hlen;  // don't include tlv-block part yet.
        return true;
    }
    else
    {
        return false;
    }     
}  // end ManetAddrBlock::SetHead()

bool ManetAddrBlock::SetTail(const ProtoAddress& addr, UINT8 tlen)
{
    ASSERT(addr.IsValid());
    if (tlen > addr.GetLength()) return false;
    unsigned int hlen = GetHeadLength();
    if (0 == hlen)
    {
        // In case SetTail() is called first
        if (!InitIntoBuffer((char*)buffer_ptr, buffer_bytes)) return false;
        addr_type = addr.GetType();
    }
    else if (addr_type != addr.GetType())
    {
        return false;
    }
    
    // minLength: 1 (num-addr) + 1 (head-octet) + hlen + 2 bytes tlv-block len
    unsigned int minLength = 2 + hlen + 2;
    
    if (tlen > 0)
    {
        // Increment minLength by tail-octet (1) + tlen
        minLength += 1 + tlen;
        if (minLength > buffer_bytes) return false;
        ((UINT8*)buffer_ptr)[OFFSET_HEAD_OCTET] &= (UINT8)(~FLAG_NO_TAIL);
        ((UINT8*)buffer_ptr)[OffsetTailLength()] = tlen;
        // Is it an all-zero tail?
        const char* tailPtr = addr.GetRawHostAddress() + addr.GetLength() - tlen;
        bool zeroTail = false;
        for (UINT8 i = 0; i < tlen; i++)
        {
            if (0 != tailPtr[i])
            {
                zeroTail = true;
                break;
            }
        }
        if (zeroTail) ((UINT8*)buffer_ptr)[OffsetTailLength()] |= (UINT8)FLAG_ZERO_TAIL;
        memcpy(((char*)buffer_ptr)+OffsetTail(), tailPtr, tlen); 
        pkt_length = OffsetTail() + tlen;  // don't include tlv-block part yet.
    }
    else
    {
        ((UINT8*)buffer_ptr)[OFFSET_HEAD_OCTET] |= FLAG_NO_TAIL;
        pkt_length = OFFSET_HEAD + hlen;  // don't include tlv-block part yet.
    }
    return true;
}  // end ManetAddrBlock::SetTail()

bool ManetAddrBlock::AppendAddress(const ProtoAddress& addr)
{
    // (TBD) Should we make sure this addr prefix matches our "head" _and_ "tail"
    ASSERT(addr.GetType() == addr_type);
    UINT8 numAddr = GetAddressCount();
    UINT8 mlen = GetMidLength();
    unsigned int offset = (0 == numAddr) ? OffsetMid() : pkt_length;
    unsigned int minLength = offset + mlen + 2;  // account for tlv-block min
    if (buffer_bytes < minLength) return false;
    memcpy(((char*)buffer_ptr)+offset, addr.GetRawHostAddress()+GetHeadLength(), mlen);
    SetAddressCount(numAddr + 1);
    pkt_length = offset + mlen;  // 2 bytes account for tlv_block minimum
    return true;
}  // end ManetAddrBlock::AppendAddress()

ManetTlv* ManetAddrBlock::AppendTlv(UINT8 type, UINT8 semantics)
{
   if (0 == GetAddressCount())
   {
       // Check for "head-only" or "tail-only" address block
       if ((0 == GetHeadLength()) && (0 == GetTailLength()))
           return false;
       else
           SetAddressCount(1);
   } 
   if (!tlv_block_pending) 
        tlv_block_pending = tlv_block.InitIntoBuffer((char*)buffer_ptr + pkt_length, buffer_bytes - pkt_length);
    return tlv_block_pending ? tlv_block.AppendTlv(type, semantics) : NULL;
}  // end ManetAddrBlock::AppendTlv()

bool ManetAddrBlock::AppendTlv(ManetTlv& tlv)
{
    if (0 == GetAddressCount())
    {
       // Check for "head-only" or "tail-only" address block
       if ((0 == GetHeadLength()) && (0 == GetTailLength()))
           return false;
       else
           SetAddressCount(1);
    } 
    if (!tlv_block_pending) 
        tlv_block_pending = tlv_block.InitIntoBuffer((char*)buffer_ptr + pkt_length, buffer_bytes - pkt_length);
    return (tlv_block_pending ? tlv_block.AppendTlv(tlv) : false);
}  // end ManetAddrBlock::AppendTlv()

void ManetAddrBlock::Pack()
{
    if (0 == GetAddressCount())
    {
       // Check for "head-only" or "tail-only" address block
       if ((0 == GetHeadLength()) && (0 == GetTailLength()))
       {
           pkt_length = 0;
           return;
       }
       else
       {
           SetAddressCount(1);
       }
    } 
    if (!tlv_block_pending)
        tlv_block.InitIntoBuffer((char*)buffer_ptr + pkt_length, buffer_bytes - pkt_length);   
    tlv_block.Pack();
    pkt_length += tlv_block.GetLength();
    tlv_block_pending = false;
}  // end ManetAddrBlock::Pack()

bool ManetAddrBlock::InitFromBuffer(ProtoAddress::Type addrType, char* bufferPtr, unsigned numBytes)
{
    if (NULL != bufferPtr) AttachBuffer((UINT32*)bufferPtr, numBytes);
    addr_type = addrType;
    pkt_length = 0;
    unsigned int minLength = 2 + GetHeadLength();
    if (buffer_bytes < minLength) return false;
    if (HasTail()) 
    {
        minLength += 1;
        if (buffer_bytes < minLength) return false;
        minLength += HasZeroTail() ? 0 : GetTailLength();
        if (buffer_bytes < minLength) return false;
    }
    // Make sure big enough for any listed address mid-sections
    minLength += GetAddressCount() * GetMidLength();
    if (buffer_bytes < minLength) return false;
    if (tlv_block.InitFromBuffer((char*)buffer_ptr+minLength, buffer_bytes - minLength))
    {
        pkt_length = minLength + tlv_block.GetLength();
        return true;
    }
    else
    {
        pkt_length = 0;
        return false;
    }
}  // end ManetAddrBlock::InitFromBuffer()

bool ManetAddrBlock::GetAddress(UINT8 index, ProtoAddress& theAddr) const
{
    ASSERT(index < GetAddressCount());
    char addrBuffer[16]; // supports up to IPv6
    // Copy head into addrBuffer
    UINT8 hlen = GetHeadLength();
    ASSERT(hlen <= GetAddressLength());
    if (hlen > 0)
        memcpy(addrBuffer, (char*)buffer_ptr+OFFSET_HEAD, hlen);
    // Copy mid into addrBuffer
    UINT8 mlen = GetMidLength();
    ASSERT(mlen <= GetAddressLength());
    if (mlen > 0)
        memcpy(addrBuffer+hlen, (char*)buffer_ptr + OffsetMid() + index*GetMidLength(), mlen);
    UINT8 tlen = GetTailLength();
    // Copy tail into addrBuffer
    if (tlen > 0)
    {
        if (HasZeroTail())
            memset(addrBuffer+hlen+mlen, 0, tlen);
        else
            memcpy(addrBuffer+hlen+mlen, (char*)buffer_ptr + OffsetTail(), tlen);
    }
    ASSERT((hlen + mlen + tlen) == GetAddressLength());
    theAddr.SetRawHostAddress(addr_type, addrBuffer, GetAddressLength());  
    return theAddr.IsValid();
}  // end ManetAddrBlock::GetAddress()

ManetAddrBlock::TlvIterator::TlvIterator(ManetAddrBlock& addrBlk)
 : ManetTlvBlock::Iterator(addrBlk.tlv_block)
{
}

ManetAddrBlock::TlvIterator::~TlvIterator()
{
}

ManetMsg::ManetMsg()
 : addr_type(ProtoAddress::INVALID)
{
}

ManetMsg::ManetMsg(UINT32*      bufferPtr, 
                   unsigned int numBytes, 
                   bool         freeOnDestruct)
 : ProtoPkt(bufferPtr, numBytes, freeOnDestruct), 
   addr_type(ProtoAddress::INVALID), 
   tlv_block_pending(false), addr_block_pending(false)
{
}

ManetMsg::~ManetMsg()
{
}

bool ManetMsg::InitIntoBuffer(UINT32* bufferPtr, unsigned int numBytes)
{
    addr_block_pending = false;
    if (NULL != bufferPtr)
        AttachBuffer(bufferPtr, numBytes);
    if (buffer_bytes >= 4)
    {
        pkt_length = 4;
        ClearAllSemantics();
        SetSemantic(NO_ORIG_SEQ);
        SetSemantic(NO_TTL);
        // Assume initial msg-tlv-block location
        tlv_block_pending = tlv_block.InitIntoBuffer((char*)buffer_ptr+4, buffer_bytes-4);
        return tlv_block_pending;
    }
    else
    {
        pkt_length = 0;
        return false;
    }
}  // end ManetMsg::InitIntoBuffer()

bool ManetMsg::SetOriginator(const ProtoAddress& addr)
{
    bool result = addr.IsValid();
    addr_type = (ProtoAddress::INVALID == addr_type) ? addr.GetType() : addr_type;
    ASSERT(addr_type == addr.GetType());  // if was already set
    unsigned int addrLen = ProtoAddress::GetLength(addr_type);
    if (result && SemanticIsSet(NO_ORIG_SEQ))
    {
        UINT16 newLength = pkt_length + addrLen + 2;
        if (newLength > buffer_bytes) return false;
        pkt_length = newLength;
        ClearSemantic(NO_ORIG_SEQ);
        // Move <msg-tlv-block> location
        result = tlv_block.InitIntoBuffer((char*)buffer_ptr+pkt_length, buffer_bytes-pkt_length);
    }
    memcpy((char*)(buffer_ptr+OFFSET_ORIGINATOR), addr.GetRawHostAddress(), addrLen);
    return result;
}  // end ManetMsg::SetOriginator()

bool ManetMsg::SetHopLimit(UINT8 hopLimit)
{
    bool result = true;
    if (SemanticIsSet(NO_TTL))
    {
        UINT16 newLength = pkt_length + 2;        // no padding when both set
        if (newLength > buffer_bytes) return false;
        pkt_length = newLength;
        ClearSemantic(NO_TTL);
        // Move <msg-tlv-block> location
        result = tlv_block.InitIntoBuffer((char*)buffer_ptr+pkt_length, buffer_bytes-pkt_length);
    }
    ((UINT8*)buffer_ptr)[OffsetHopLimit()] = hopLimit;
    return result;
}  // end ManetMsg::SetTTL()

bool ManetMsg::SetHopCount(UINT8 hopCount)
{
    bool result = true;
    if (SemanticIsSet(NO_TTL))
    {
        UINT16 newLength = pkt_length + 2;        // no padding when both set
        if (newLength > buffer_bytes) return false;
        pkt_length = newLength;
        ClearSemantic(NO_TTL);
        // Move <msg-tlv-block> location
        result = tlv_block.InitIntoBuffer((char*)buffer_ptr+pkt_length, buffer_bytes-pkt_length);
    }
    ((UINT8*)buffer_ptr)[OffsetHopCount()] = hopCount;
    return result;
}  // end ManetMsg::SetHopCount()

ManetAddrBlock* ManetMsg::AppendAddressBlock()
{
    if (tlv_block_pending)
    {
        tlv_block.Pack();
        pkt_length += tlv_block.GetLength();
        tlv_block_pending = false;
    }
    if (addr_block_pending) 
    {
        addr_block_temp.Pack();
        pkt_length += addr_block_temp.GetLength();
    }
    unsigned int bufferMax = (buffer_bytes > pkt_length) ? buffer_bytes - pkt_length : 0;
    char* bufferPtr = (char*)buffer_ptr + pkt_length;
    addr_block_pending = addr_block_temp.InitIntoBuffer(bufferPtr, bufferMax);
    return (addr_block_pending ? &addr_block_temp : NULL);
}  // end ManetMsg::AppendAddressBlock()

void ManetMsg::Pack() 
{
    if (tlv_block_pending)
    {
        tlv_block.Pack();
        pkt_length += tlv_block.GetLength();
        tlv_block_pending = false;
    }
    if (addr_block_pending) 
    {
        addr_block_temp.Pack();
        pkt_length += addr_block_temp.GetLength();
        addr_block_pending = false;
    }
    SetSize((UINT16)pkt_length);  // includes message header _and_ body
}  // end ManetMsg::Pack()

bool ManetMsg::InitFromBuffer(ProtoAddress::Type addrType, UINT32* bufferPtr, unsigned int numBytes)
{
    if (NULL != bufferPtr)
        AttachBuffer(bufferPtr, numBytes);
    addr_type = addrType;
    pkt_length = (buffer_bytes >= 4) ? GetSize() : 0;
    if (0 == pkt_length) return false;  
    UINT16 tlvBlockOffset = OffsetTlvBlock();
    char* tlvBuffer =  (char*)buffer_ptr + tlvBlockOffset;
    return tlv_block.InitFromBuffer(tlvBuffer, pkt_length - tlvBlockOffset);
}  // end ManetMsg::InitFromBuffer()

bool ManetMsg::GetOriginator(ProtoAddress& addr) const
{
    if (SemanticIsSet(NO_ORIG_SEQ))
        addr.Invalidate();
    else
        addr.SetRawHostAddress(addr_type, (char*)(buffer_ptr+OFFSET_ORIGINATOR), ProtoAddress::GetLength(addr_type));
    return addr.IsValid();
}  // ManetMsg::GetOriginator()
        
bool ManetMsg::GetNextAddressBlock(ManetAddrBlock& addrBlk) const
{
    char* nextBuffer = (char*)addrBlk.GetBuffer();
    if (NULL == nextBuffer)
    {
        // Get _first_ address block
        ManetTlvBlock tlvBlk;
        if (GetTlvBlock(tlvBlk)) 
            nextBuffer = (char*)tlvBlk.GetBuffer() + tlvBlk.GetLength();
        else
            return false; // there was no <msg-tlv-block>
    }
    else
    {
        nextBuffer += addrBlk.GetLength();
    }
    unsigned int offset = nextBuffer - (char*)buffer_ptr;
    if (offset < pkt_length)
        return addrBlk.InitFromBuffer(addr_type, nextBuffer, pkt_length - offset);
    else
        return false;
}  // end ManetMsg::GetNextAddressBlock()


ManetMsg::TlvIterator::TlvIterator(ManetMsg& msg)
 : ManetTlvBlock::Iterator(msg.tlv_block)
{
}

ManetMsg::TlvIterator::~TlvIterator()
{
}

ManetMsg::AddrBlockIterator::AddrBlockIterator(ManetMsg& theMsg)
 : msg(theMsg)
{
}

ManetMsg::AddrBlockIterator::~AddrBlockIterator()
{
}


ManetPkt::ManetPkt()
{
}

ManetPkt::ManetPkt(UINT32*      bufferPtr, 
                   unsigned int numBytes, 
                   bool         freeOnDestruct)
 : ProtoPkt(bufferPtr, numBytes, freeOnDestruct), 
   tlv_block_pending(false), msg_pending(false)
{
}

ManetPkt::~ManetPkt()
{
}

bool ManetPkt::InitIntoBuffer(UINT32* bufferPtr, unsigned int numBytes)
{
    if (NULL != bufferPtr)
        AttachBuffer(bufferPtr, numBytes);
    if (buffer_bytes < 2) return false;
    buffer_ptr[OFFSET_ZERO] = 0;
    ClearAllSemantics();
    SetSemantic(NO_SEQ_NUM);  // semantic is set if SetSequence() is called
    pkt_length = 2;
    tlv_block_pending = false;
    msg_pending = false;
    return true;
}  // end ManetPkt::InitIntoBuffer()

ManetTlv* ManetPkt::AppendTlv(UINT8 type, UINT8 semantics)
{
    if (msg_pending)
        return NULL; // Can't append Tlv after appending a message!
    if (!tlv_block_pending)
    {
        // Set semantic and init tlv_block
        SetSemantic(TLV_BLOCK);
        unsigned int offsetTlvBlock = OffsetTlvBlock();
        tlv_block_pending = 
            tlv_block.InitIntoBuffer((char*)buffer_ptr+offsetTlvBlock, buffer_bytes-offsetTlvBlock);
    }
    semantics |= ManetTlv::NO_INDEX;  // pkt-tlv's never have index!
    return (tlv_block_pending ? tlv_block.AppendTlv(type, semantics) : NULL);   
}  // end ManetPkt::AppendTlv()

bool ManetPkt::AppendTlv(ManetTlv& tlv)
{
    ASSERT(!tlv.SemanticIsSet(ManetTlv::NO_INDEX));  // msg-tlv's never have index!
    if (msg_pending)
        return false; // Can't append Tlv after appending a message!
    if (!tlv_block_pending)
    {
        // Set semantic and init tlv_block
        SetSemantic(TLV_BLOCK);
        unsigned int offsetTlvBlock = OffsetTlvBlock();
        tlv_block_pending = 
            tlv_block.InitIntoBuffer((char*)buffer_ptr+offsetTlvBlock, buffer_bytes-offsetTlvBlock);
    }
    return (tlv_block_pending ? tlv_block.AppendTlv(tlv) : false);
}  // end ManetPkt::AppendTlv()

ManetMsg* ManetPkt::AppendMessage()
{
    if (tlv_block_pending)
    {
        tlv_block.Pack();
        pkt_length += tlv_block.GetLength();
        tlv_block_pending = false;
    }
    if (msg_pending) 
    {
        msg_temp.Pack();
        pkt_length += msg_temp.GetLength();
    }
    unsigned int bufferMax = (buffer_bytes > pkt_length) ? buffer_bytes - pkt_length : 0;
    UINT32* bufferPtr = (UINT32*)((char*)buffer_ptr + pkt_length);
    msg_pending = msg_temp.InitIntoBuffer(bufferPtr, bufferMax);
    return msg_pending ? &msg_temp : NULL;
}  // end ManetPkt::AppendMessage()

void ManetPkt::Pack()
{
    if (tlv_block_pending)
    {
        TRACE("ManetPkt::Pack() packing pending tlv_block\n");
        tlv_block.Pack();
        pkt_length += tlv_block.GetLength();
        tlv_block_pending = false;
    }
    if (msg_pending)
    {
        TRACE("ManetPkt::Pack() packing pending msg_temp\n");
        msg_temp.Pack();
        pkt_length += msg_temp.GetLength();
        msg_pending = false; 
    }  
}  // end ManetPkt::Pack()

bool ManetPkt::InitFromBuffer(unsigned int pktLength, UINT32* bufferPtr, unsigned int numBytes)
{
    if (NULL != bufferPtr) 
        AttachBuffer(bufferPtr, numBytes);
    msg_pending = false;
    if ((pktLength < 2) || (buffer_bytes < pktLength))
        return false;
    if (pktLength < GetMinLength(((UINT8*)bufferPtr)[OFFSET_SEMANTICS]))
        return false;
    else 
        pkt_length = pktLength; 
            
    if (SemanticIsSet(TLV_BLOCK))
    {
        unsigned tlvBlockOffset = OffsetTlvBlock();
        char* tlvBuffer =  (char*)buffer_ptr + tlvBlockOffset;
        return tlv_block.InitFromBuffer(tlvBuffer, pkt_length - tlvBlockOffset);
    }
    else
    {
        tlv_block.InitFromBuffer(NULL, 0);
        return true;
    }   
}  // end ManetPkt::InitFromBuffer()

ManetPkt::TlvIterator::TlvIterator(ManetPkt& pkt)
 : ManetTlvBlock::Iterator(pkt.tlv_block)
{
}

ManetPkt::TlvIterator::~TlvIterator()
{
}

ManetPkt::MsgIterator::MsgIterator(ManetPkt& thePkt)
 : pkt(thePkt)
{
}

ManetPkt::MsgIterator::~MsgIterator()
{
}
