#ifndef _MANET_MESSAGE
#define _MANET_MESSAGE

// This C++ class is an implementation of the packet and message formats given 
// in <http://www.ietf.org/internet-drafts/draft-ietf-manet-packetbb-01.txt>
// Methods are provided to construct and parse the Type-Length-Value (TLV)
// based scheme described in that document.

#include <protoPkt.h>
#include <protoDefs.h>
#include <protoDebug.h>
#include <protoAddress.h>

class ManetTlv : public ProtoPkt
{
    public:
        ManetTlv();
        ~ManetTlv();
        
        // (TBD) Will there be standard "namespace:ietf:manet:message:tlvTypes"
        //       that we should enumerate here?
        
        // Should this enum be moved to ManetMsg::TlvType???
        enum Type
        {
            // (TBD) Provide methods for dealing with fragmented TLVs
            FRAGMENTATION       = 0,    // value length = 3 bytes 
            CONTENT_SEQ_NUMBER  = 1     // value length
        };
            
        enum TypeFlag
        {
            TLV_USER     = 0X80,
            TLV_PROTOCOL = 0X40           
        };
        
        enum Semantic
        {
            SEMANTIC_DEFAULT    = 0x00, // there is an index range, single byte length, single value content
            NO_VALUE            = 0x01, // no value content and no value length
            EXTENDED            = 0x02, // Indicates 16-bit TLV length field
            NO_INDEX            = 0x04, // there is no index range given
            MULTIVALUE          = 0x08  // one value for each address in index range
        };
            
        // TLV building routines (these MUST be called in order given here)
        bool InitIntoBuffer(UINT8 type, UINT8 semantics, char* bufferPtr = NULL, unsigned numBytes = 0);
        
        // Defined to use with built-in enumerated TLV types (FRAGMENTATION, etc)
        bool InitIntoBuffer(Type type, UINT8 semantics, char* bufferPtr = NULL, unsigned numBytes = 0)
            {return InitIntoBuffer((UINT8)type, semantics, bufferPtr, numBytes);}
        
        void SetIndexRange(UINT8 start, UINT8 stop)
        {
            ASSERT(!SemanticIsSet(NO_INDEX));
            ((UINT8*)buffer_ptr)[OffsetIndexStart()] = start;
            ((UINT8*)buffer_ptr)[OffsetIndexStop()] = stop;
        }
        
        // Note "index" MUST be >= GetIndexStart() and <= GetIndexStop()
        bool SetValue(char* value, UINT16 valueLength, UINT8 index = 0);
        // Here are some "shortcut" versions of SetValue()
        bool SetValue(UINT8 value, UINT8 index = 0)
            {return (SetValue((char*)&value, sizeof(UINT8), index));}
        bool SetValue(UINT16 value, UINT8 index = 0)
            {value = htons(value); return (SetValue((char*)&value, sizeof(UINT16), index));}
        bool SetValue(UINT32 value, UINT8 index = 0)
            {value = htonl(value); return (SetValue((char*)&value, sizeof(UINT32), index));}
        
        // TLV parsing methods
        // Note:  This one is called by the ManetMsg and ManetAddrBlock "TlvIterator"
        //        classes so you shouldn't call this yourself, typically.
        bool InitFromBuffer(char* bufferPtr = NULL, unsigned int numBytes = 0);
        
        UINT8 GetType() const
            {return (((UINT8*)buffer_ptr)[OFFSET_TYPE]);}
        
        bool SemanticIsSet(Semantic semantic) const
            {return (0 != ((char)semantic & ((char*)buffer_ptr)[OFFSET_SEMANTICS]));}
        
        bool HasTlvLength() const
            {return (0 == SemanticIsSet(NO_VALUE));}
        // Returns total length of tlv <value> field
        UINT16 GetTlvLength() const
        {
            return (HasTlvLength() ? 
                        (SemanticIsSet(EXTENDED) ? 
                            GetUINT16((UINT16*)buffer_ptr+OFFSET_LENGTH) :
                            (UINT16)(((UINT8*)buffer_ptr)[OFFSET_LENGTH*2])) : 0);
        }
        
        bool HasIndex() const {return !SemanticIsSet(NO_INDEX);}
        
        UINT8 GetIndexStart() const
            {return (SemanticIsSet(NO_INDEX) ? 0 : ((char*)buffer_ptr)[OffsetIndexStart()]);}
        
        UINT8 GetIndexStop() const
            {return (SemanticIsSet(NO_INDEX) ? 0 : ((char*)buffer_ptr)[OffsetIndexStop()]);}
        
        bool HasValue() const {return (!SemanticIsSet(NO_VALUE));}
        bool IsMultiValue() const
            {return (HasValue() ? SemanticIsSet(MULTIVALUE) : false);}
        // Returns length of _single_ tlv value (in bytes)
        UINT16 GetValueLength() const
        {
            return (SemanticIsSet(NO_VALUE) ?
                        0 :
                        (SemanticIsSet(MULTIVALUE) ?
                            (GetTlvLength() / (GetIndexStop() - GetIndexStart() + 1)) : 
                            GetTlvLength()));  
        }
        
        void GetValue(char* value, UINT16 valueLength, UINT8 index = 0) const
        {
            ASSERT(!SemanticIsSet(NO_VALUE));
            ASSERT((0 == index) || SemanticIsSet(MULTIVALUE));
            ASSERT((index >= GetIndexStart()) && (index <= GetIndexStop()));
            ASSERT(valueLength == GetValueLength());
            index -= GetIndexStart();
            UINT16 offset = OffsetValue() + (index * valueLength); 
            ASSERT((offset+valueLength) <= pkt_length);
            memcpy(value, (char*)buffer_ptr + offset, valueLength);    
        }
        // Here are some "shortcut" methods for certain lengths
        void GetValue(UINT8& value, UINT8 index = 0) const
            {return GetValue((char*)&value, sizeof(UINT8), index);}
        void GetValue(UINT16& value, UINT8 index = 0) const
            {GetValue((char*)&value, sizeof(UINT16), index); value = ntohs(value);}
        void GetValue(UINT32& value, UINT8 index = 0) const
            {GetValue((char*)&value, sizeof(UINT32), index); value = ntohl(value);}
        
        // This returns a pointer to the value portion of the tlv buffer_ptr
        const char* GetValue(UINT8 index = 0) const
        {
            ASSERT((0 == index) || SemanticIsSet(MULTIVALUE));
            ASSERT((index >= GetIndexStart()) && (index <= GetIndexStop()));
            index -= GetIndexStart();
            UINT16 offset = OffsetValue() + (index * GetValueLength());
            return (SemanticIsSet(NO_VALUE) ? NULL : (char*)buffer_ptr+offset);
        }
            
    private:
        enum
        {
            OFFSET_TYPE         = 0,
            OFFSET_SEMANTICS    = OFFSET_TYPE + 1,   
            OFFSET_LENGTH       = (OFFSET_SEMANTICS + 1)/2
            // OffsetIndexStart()
            // OffsetIndexStop()
            // OffsetValue()
        };
            
        static unsigned int GetMinLength(UINT8 semantics)
        {
            unsigned int minLength = 2;
            minLength += 
                (0 == ((UINT8)NO_VALUE & semantics)) ?
                    ((0 == ((UINT8)EXTENDED & semantics)) ? 1 : 2) : 0;
            minLength += (0 == ((UINT8)NO_INDEX & semantics)) ? 2 : 0;
            return minLength;
        }
            
        void SetTlvLength(UINT16 tlvLength)
        {
            if (SemanticIsSet(EXTENDED))
                SetUINT16((UINT16*)buffer_ptr+OFFSET_LENGTH, tlvLength);
            else
                ((UINT8*)buffer_ptr)[OFFSET_LENGTH*2] = (UINT8)tlvLength;  
        }
            
        unsigned int OffsetIndexStart() const
        {
            unsigned int offset = OFFSET_LENGTH*2;
            offset += SemanticIsSet(NO_VALUE) ?
                        0 : (SemanticIsSet(EXTENDED) ? 2 : 1);
            return offset;   
        }
        unsigned int OffsetIndexStop() const
            {return (OffsetIndexStart() + 1);}
        unsigned int OffsetValue() const
        {
            unsigned int offset = OFFSET_LENGTH*2;
            offset += SemanticIsSet(EXTENDED) ? 2 : 1;
            offset += SemanticIsSet(NO_INDEX) ? 0 : 2;
            return offset;
        }
                   
};  // end class ManetTlv

class ManetFragmentationTlv : private ManetTlv
{
    public:
        ManetFragmentationTlv();
        ~ManetFragmentationTlv();
        
        enum FragmentationSemantic
        {
            NOT_SELF_CONTAINED = 0x01,
            TYPED_SEQ          = 0x02
        };
        
        bool InitIntoBuffer(UINT8 numFragments, 
                            UINT8 fragmentNumber,
                            UINT8 fragmentSemantics, 
                            char* bufferPtr = NULL, 
                            unsigned numBytes = 0)
        {
            bool result = ManetTlv::InitIntoBuffer(FRAGMENTATION, NO_INDEX, bufferPtr, numBytes);
            char values[3];
            values[OFFSET_NUM_FRAGMENTS] = numFragments;
            values[OFFSET_FRAGMENT_NUMBER] = fragmentNumber;
            values[OFFSET_SEMANTICS] = fragmentSemantics;
            return result ? SetValue(values, 3) : false;
        }
        
        bool InitFromBuffer(char* bufferPtr = NULL, unsigned int numBytes = 0)
            {return ManetTlv::InitFromBuffer(bufferPtr, numBytes);}
        
        UINT8 GetNumFragments() const
        {
            const UINT8* valuePtr = (const UINT8*)GetValue();
            return ((NULL != valuePtr) ? valuePtr[OFFSET_NUM_FRAGMENTS] : 0);
        }
        UINT8 GetFragmentNumber() const
        {
            const UINT8* valuePtr = (const UINT8*)GetValue();
            return ((NULL != valuePtr) ? valuePtr[OFFSET_FRAGMENT_NUMBER] : 0);
        }
        
        bool IsSelfContained() const
            {return !SemanticIsSet(NOT_SELF_CONTAINED);}
        
        bool SequenceIsTypeDependent() const
            {return SemanticIsSet(TYPED_SEQ);}
        
    
    private:
        bool SemanticIsSet(FragmentationSemantic semantic) const
        {
            const UINT8* valuePtr = (const UINT8*)GetValue();
            return ((NULL != valuePtr) ? 
                    (0 != (semantic & valuePtr[OFFSET_SEMANTICS])) : 
                    false);
        }
        // Offsets into "value" field for different sub-values
        enum
        {
            OFFSET_NUM_FRAGMENTS   = 0,
            OFFSET_FRAGMENT_NUMBER = 1,
            OFFSET_SEMANTICS       = 2
        };
    
};  // end class ManetFragmentationTlv

// Note: Typically, you should not have to use the ManetTlvBlock class directly yourself
class ManetTlvBlock: public ProtoPkt
{
    public:
        ManetTlvBlock();
        ~ManetTlvBlock();
        
        // TLV Block building methods
        // Note: This one is called for you by the ManetMsg and ManetAddrBlock
        //       "AppendTlv()", etc routines, so you shouldn't normally
        //       need to make calls related to ManetTlvBlock yourself.
        bool InitIntoBuffer(char* bufferPtr = NULL, unsigned int numBytes = 0);
        
        // This returns a ManetTlv pointer initialized into the
        // tlv block's buffer_ptr space
        ManetTlv* AppendTlv(UINT8 type, UINT8 semantics);
        
        // This copies an existing ManetTlv into the tlv block's
        // buffer_ptr space if there is room.
        bool AppendTlv(ManetTlv& tlv);
        
        void Pack();
        
        // TLV Block parsing methods
        bool InitFromBuffer(char* bufferPtr = NULL, unsigned int numBytes = 0);
        
        UINT16 GetTlvBlockLength()
            {return GetUINT16((UINT16*)buffer_ptr+OFFSET_LENGTH);}
        
        class Iterator
        {
            public:
                Iterator(ManetTlvBlock& tlvBlk);
                ~Iterator();
                
                void Reset() {tlv_temp.AttachBuffer(NULL, 0);}
                bool GetNextTlv(ManetTlv & tlv)
                {
                    bool result = tlv_block.GetNextTlv(tlv_temp);
                    tlv = tlv_temp;
                    return result;
                }
                        
                        
            private:
                ManetTlvBlock&    tlv_block;
                ManetTlv          tlv_temp;
        };  // end class ManetTlvBlock::Iterator
        
        
    private:
        bool GetNextTlv(ManetTlv& tlv) const;
        
        void SetTlvBlockLength(UINT16 tlvLength)
            {SetUINT16((UINT16*)buffer_ptr+OFFSET_LENGTH, tlvLength);}
    
        ManetTlv    tlv_temp;
        bool        tlv_pending;
            
        enum
        {
            OFFSET_LENGTH  = 0,  // 2-byte <tlv-length> field
            OFFSET_CONTENT = 2
        };
                 
};  // end class ManetTlvBlock
    
class ManetAddrBlock : public ProtoPkt
{
    public:
        ManetAddrBlock();
        ManetAddrBlock(char* bufferPtr, unsigned int numBytes, bool freeOnDestroy = false);
        ~ManetAddrBlock();
        
        // Addr block building methods (These MUST be called in order given)
        // Note:  This is called for you by ManetMsg::AppendAddressBlock() so
        //        typically you shouldn't call this one yourself.
        bool InitIntoBuffer(char* bufferPtr = NULL, unsigned int numBytes = 0);
        
        // if <head-length> == addr-length, this sets the entire addr-block content
        // Note: "hlen" is the prefix length in bytes
        bool SetHead(const ProtoAddress& addr, UINT8 hlen);
        
        bool SetTail(const ProtoAddress& addr, UINT8 tlen);
        
        bool AppendAddress(const ProtoAddress& addr);
        
        // This returns a ManetTlv pointer initialized into the
        // address block's buffer_ptr space
        ManetTlv* AppendTlv(UINT8 type, UINT8 semantics);
        
        // This copies an _existing_ ManetTlv into the address block's
        // buffer_ptr space, if there is room.
        // (This lets us build TLV's in a separate buffer_ptr if we choose to)
        bool AppendTlv(ManetTlv& tlv);
        
        // This finalizes the address block build
        // Note: This is taken care of for you by ManetMsg::Pack() which is called by
        //       ManetPkt::Pack() so you shouldn't call this yourself if you use
        //       those constructs to encapsulate your address block.
        void Pack();
        
        // Addr block parsing methods
        // Note:  This is called for you by ManetMsg::AddrBlockIterator() so typically
        //        you shouldn't have to call this yourself.
        bool InitFromBuffer(ProtoAddress::Type addrType, char* bufferPtr, unsigned int numBytes = 0);
        
       UINT8 GetAddressLength() const
            {return ProtoAddress::GetLength(addr_type);}
        UINT8 GetHeadLength() const
            {return (0x7f & ((UINT8*)buffer_ptr)[OFFSET_HEAD_LENGTH]);}
        bool HasTail() const
            {return (0 == (FLAG_NO_TAIL & ((UINT8*)buffer_ptr)[OFFSET_HEAD_OCTET]));}
        bool HasZeroTail() const
            {return (0 == (FLAG_ZERO_TAIL & ((UINT8*)buffer_ptr)[OffsetTailOctet()]));}
                
        UINT8 GetTailLength() const
            {return (HasTail() ? (0x7f & ((UINT8*)buffer_ptr)[OffsetTailLength()]) : 0);}
        
        UINT8 GetMidLength() const
            {return (GetAddressLength() - GetHeadLength() - GetTailLength());}
        
        UINT8 GetAddressCount() const
            {return (((UINT8*)buffer_ptr)[OFFSET_NUM_ADDR]);}
        
        bool GetAddress(UINT8 index, ProtoAddress& theAddr) const;
        
        // These methods are provided for more optimal (but stateful) 
        // parsing by application code using this class
        // (i.e, directly copy addresses into a buffer_ptr)
        const char* GetHead() const
            {return ((char*)buffer_ptr + OFFSET_HEAD);}
        const char* GetMid(UINT8 index) const
        {
            ASSERT(index < GetAddressCount());
            return ((char*)buffer_ptr + OffsetMid() + index*GetMidLength()); 
        }
        const char* GetTail(UINT8 index) const
            {return ((char*)buffer_ptr + OffsetTail());}
        
        class TlvIterator : public ManetTlvBlock::Iterator
        {
            public:
                TlvIterator(ManetAddrBlock& addrBlk);
                ~TlvIterator();  
                
                void Reset() {ManetTlvBlock::Iterator::Reset();}
                
                bool GetNextTlv(ManetTlv& tlv)
                    {return ManetTlvBlock::Iterator::GetNextTlv(tlv);} 
        }; //  end class ManetAddrBlock::TlvIterator
        
    private:
        ProtoAddress::Type  addr_type;
        ManetTlvBlock       tlv_block;
        bool                tlv_block_pending;
        
        enum
        {
            FLAG_NO_TAIL   = 0x80,
            FLAG_ZERO_TAIL = 0x80
        };
        
        enum
        {
            OFFSET_NUM_ADDR    = 0,
            OFFSET_HEAD_OCTET  = OFFSET_NUM_ADDR + 1,
            OFFSET_HEAD_LENGTH = OFFSET_HEAD_OCTET,
            OFFSET_HEAD        = OFFSET_HEAD_LENGTH + 1  
        };
            
        UINT16 OffsetTailOctet() const
            {return (OFFSET_HEAD + GetHeadLength());}
        UINT16 OffsetTailLength() const
            {return (OffsetTailOctet());}
        UINT16 OffsetTail() const
            {return (OffsetTailOctet() + (HasTail() ? 1 : 0));}
        UINT16 OffsetMid() const
        {
            UINT16 offset = OffsetTailOctet();
            offset += HasTail() ? (1 + GetTailLength()) : 0;
            return offset;
        }
        void SetAddressCount(UINT8 numAddrs) const
            {((UINT8*)buffer_ptr)[OFFSET_NUM_ADDR] = numAddrs;}
                       
};  // end class ManetAddrBlock

class ManetMsg : public ProtoPkt
{
    public:
        ManetMsg();
        ManetMsg(UINT32* bufferPtr, unsigned int numBytes, bool freeOnDestroy = false); 
        virtual ~ManetMsg();
        
        // (TBD) Will there be standard "namespace:ietf:manet:message:types"
        //       that we should enumerate here?
        
        enum SemanticFlag
        {
            NO_ORIG_SEQ = 0x01,
            NO_TTL      = 0x02,
            TYPED_SEQ   = 0x04,
            NO_FWD      = 0x08,
            RESERVED1   = 0x10,  // this must be cleared
            RESERVED2   = 0x20,  // this must be cleared
            RESERVED3   = 0x40,  // this must be cleared
            RESERVED4   = 0x80   // this must be cleared
        };
            
        ////////////////////////////////////////
        // Message Building (all header & info fields MUST be set 
        // _before_ tlv's, etc).  Also message header/info fields 
        // MUST be set in order given here!
        
        // Init into given buffer_ptr space
        // Note:: ManetPkt::AppendMessage() will call this one for you so
        //        typically you shouldn't need to call this
        bool InitIntoBuffer(UINT32* bufferPtr = NULL, unsigned int numBytes = 0);
        
        void SetType(UINT8 type)
            {((UINT8*)buffer_ptr)[OFFSET_TYPE] = type;}
        
        // <msg-semantics> flags are set as corresponding fields are set
        // <msg-size> is set when message is Pack()'d 
        
        bool SetOriginator(const ProtoAddress& addr);
        
        bool SetHopLimit(UINT8 hopLimit);
        
        bool SetHopCount(UINT8 hopCount);
        
        bool SetSequence(UINT16 seq)
        {
            ASSERT(!SemanticIsSet(NO_ORIG_SEQ));  // must set <originator-addr> first!
            SetUINT16((UINT16*)buffer_ptr + OffsetSequence(), seq);
            return true;
        }
        
        // First, append any message TLV's (and set their indexes, values, etc as needed)
        
        // Append a <msg-tlv>
        // This returns a ManetTlv pointer initialized into the ManetMsg buffer_ptr space
        ManetTlv* AppendTlv(UINT8 type, UINT8 semantics)
        {
            semantics |= ManetTlv::NO_INDEX;  // msg-tlv's never have index!
            return (tlv_block_pending ? tlv_block.AppendTlv(type, semantics) : NULL);   
        }
        
        // This copies an existing ManetTlv into the message's buffer_ptr space
        // if there is room.  (This lets us build TLV's in a separate 
        // buffer_ptr if we so choose)
        bool AppendTlv(ManetTlv& tlv)
        {
            ASSERT(!tlv.SemanticIsSet(ManetTlv::NO_INDEX));  // msg-tlv's never have index!
            return (tlv_block_pending ? tlv_block.AppendTlv(tlv) : false);
        }
        
        // Then, append any address block(s)
        // (This returns a pointer to an address-block initialized into the message's buffer_ptr 
        ManetAddrBlock* AppendAddressBlock();
                
       // Note:  ManetPkt::Pack() will call this for you, so you don't have to!
        void Pack();
        
        // Message parsing
        // Note:  InitFromBuffer() is called for you by the ManetPkt::Iterator()
        //        so you shouldn't need to call this (perhaps this should be private
        //        assuming the message is encapsulated in an ManetPkt?)
        
        bool InitFromBuffer(ProtoAddress::Type addrType, UINT32* bufferPtr, unsigned int numBytes = 0);
        
        UINT8 GetType() const
            {return (((UINT8*)buffer_ptr)[OFFSET_TYPE]);}
        
        UINT16 GetSize() const
            {return GetUINT16((UINT16*)buffer_ptr + OFFSET_SIZE);}
        
        bool HasOriginator() const
            {return (!SemanticIsSet(NO_ORIG_SEQ));}
        bool GetOriginator(ProtoAddress& addr) const;
              
        bool HasHopLimit() const
            {return (!SemanticIsSet(NO_TTL));}
        UINT8 GetHopLimit() const
            {return (((UINT8*)buffer_ptr)[OffsetHopLimit()]);}
        
        bool HasHopCount() const
            {return (!SemanticIsSet(NO_TTL));}
        UINT8 GetHopCount() const
            {return (((UINT8*)buffer_ptr)[OffsetHopCount()]);}
        
        bool HasSequence() const
            {return (!SemanticIsSet(NO_ORIG_SEQ));}
        UINT16 GetSequence() const
            {return GetUINT16((UINT16*)buffer_ptr + OffsetSequence());}
        
        class TlvIterator : public ManetTlvBlock::Iterator
        {
            public:
                TlvIterator(ManetMsg& msg);
                ~TlvIterator();   
                
                void Reset() {ManetTlvBlock::Iterator::Reset();}
                
                bool GetNextTlv(ManetTlv& tlv)
                    {return ManetTlvBlock::Iterator::GetNextTlv(tlv);}
        }; //  end class ManetAddrBlock::TlvIterator
        
        class AddrBlockIterator
        {
            public:
                AddrBlockIterator(ManetMsg& theMsg);
                ~AddrBlockIterator();
                
                void Reset() 
                    {addr_block_temp.AttachBuffer(NULL, 0);}
                
                bool GetNextAddressBlock(ManetAddrBlock& addrBlk)
                {
                    bool result = msg.GetNextAddressBlock(addr_block_temp);
                    addrBlk = addr_block_temp;
                    return result;
                }
            private:
                ManetMsg&         msg;
                ManetAddrBlock    addr_block_temp;
        };  // end class ManetMessage::AddrBlockIterator()
        
    protected:
        bool GetTlvBlock(ManetTlvBlock& tlvBlk) const
        {
            UINT16 tlvBlockOffset = OffsetTlvBlock();
            char* tlvBuffer =  (char*)buffer_ptr + tlvBlockOffset;
            return tlvBlk.InitFromBuffer(tlvBuffer, pkt_length - tlvBlockOffset); 
        }
        
        bool GetNextAddressBlock(ManetAddrBlock& addrBlk) const;
        
        bool SemanticIsSet(SemanticFlag flag) const
            {return (0 != (flag & ((UINT8*)buffer_ptr)[OFFSET_SEMANTICS]));}
        void SetSemantic(SemanticFlag flag)
            {((UINT8*)buffer_ptr)[OFFSET_SEMANTICS] |= (UINT8)flag;}
        void ClearSemantic(SemanticFlag flag)
            {((UINT8*)buffer_ptr)[OFFSET_SEMANTICS] &= (UINT8)(~flag);}
        void ClearAllSemantics()
            {((UINT8*)buffer_ptr)[OFFSET_SEMANTICS] = 0;}
        
        UINT8 GetAddressLength() const
            {return ProtoAddress::GetLength(addr_type);}
        
        UINT16 OffsetHopLimit() const
        {
            return SemanticIsSet(NO_ORIG_SEQ) ? 
                        (OFFSET_INFO) : 
                        (OFFSET_INFO + GetAddressLength());
        }
        UINT16 OffsetHopCount() const
        {
            return SemanticIsSet(NO_ORIG_SEQ) ? 
                        (OFFSET_INFO + 1) : 
                        (OFFSET_ORIGINATOR*4 + GetAddressLength() + 1);
        }
        UINT16 OffsetSequence() const
        {
            return SemanticIsSet(NO_TTL) ? 
                        (OFFSET_ORIGINATOR*4 + GetAddressLength())/2 : 
                        (OFFSET_ORIGINATOR*4 + GetAddressLength() + 2)/2;
        }
        UINT16 OffsetTlvBlock() const
        {
            UINT16 offset = OFFSET_INFO;
            offset += SemanticIsSet(NO_ORIG_SEQ) ? 0 : (GetAddressLength()  + 2);
            offset += SemanticIsSet(NO_TTL) ? 0 : 2;
            return (offset); 
        }
        
        void SetSize(UINT16 numBytes)
            {SetUINT16((UINT16*)buffer_ptr+OFFSET_SIZE, numBytes);}
    
        ProtoAddress::Type  addr_type;
        ManetTlvBlock       tlv_block;  // msg-tlv-blk
        bool                tlv_block_pending;
        ManetAddrBlock      addr_block_temp;
        bool                addr_block_pending;
        
        enum
        {
            OFFSET_TYPE         = 0,   
            OFFSET_SEMANTICS    = OFFSET_TYPE + 1,
            OFFSET_SIZE         = (OFFSET_SEMANTICS + 1)/2,
            OFFSET_INFO         = (OFFSET_SIZE + 1)*2,
            OFFSET_ORIGINATOR   = OFFSET_INFO/4
        };
            
}; // end class ManetMsg

class ManetPkt : public ProtoPkt
{
    public:
        ManetPkt();
        ManetPkt(UINT32* bufferPtr, unsigned int numBytes, bool freeOnDestruct = false); 
        ~ManetPkt();
        
        enum SemanticFlag
        {
            NO_SEQ_NUM  = 0x01,  // no sequence if set
            TLV_BLOCK   = 0x02   // packet tlv block present if set
        };
        
        // Packet building methods (IMPORTANT: Call these in the order given)
        bool InitIntoBuffer(UINT32* bufferPtr = NULL, unsigned int numBytes = 0);
        
        void SetSequence(UINT16 sequence)
        {
            if (SemanticIsSet(NO_SEQ_NUM)) 
            {
                ClearSemantic(NO_SEQ_NUM);
                pkt_length += 2;
            }
            ((UINT16*)buffer_ptr)[OFFSET_SEQUENCE] = htons(sequence);
        } 
        
        // Two possible methods to append TLVs to packet tlv block
        ManetTlv* AppendTlv(UINT8 type, UINT8 semantics);
        
        // This copies an existing ManetTlv into the packets's buffer_ptr space
        // if there is room.  (This lets us build TLV's in a separate 
        // buffer_ptr if we so choose for whatever reason)
        bool AppendTlv(ManetTlv& tlv);
       
        // "Attach", "build", then "pack" messages into ManetPkt
        // This returns a pointer to a ManetMsg initialized into the packet's buffer_ptr space
        ManetMsg* AppendMessage();
        
        // Call this last to "finalize" the packet and its contents
        void Pack();
        
        
        // Packet parsing methods    
        bool InitFromBuffer(unsigned int pktLength, UINT32* bufferPtr = NULL, unsigned int numBytes = 0);
                 
        bool HasSequence() const 
            {return !SemanticIsSet(NO_SEQ_NUM);}
        UINT16 GetSequence() const
        {
            ASSERT(!SemanticIsSet(NO_SEQ_NUM));
            return (ntohs(((UINT16*)buffer_ptr)[OFFSET_SEQUENCE]));
        }
        
        const char* GetPayload() const
            {return ((char*)buffer_ptr+OffsetPayload());}
        
        UINT16 GetPayloadLength() const
            {return ((pkt_length > OffsetPayload()) ? (pkt_length - OffsetPayload()) : 0);}
        
        class TlvIterator : public ManetTlvBlock::Iterator
        {
            public:
                TlvIterator(ManetPkt& pkt);
                ~TlvIterator();   
                
                void Reset() {ManetTlvBlock::Iterator::Reset();}
                
                bool GetNextTlv(ManetTlv& tlv)
                    {return ManetTlvBlock::Iterator::GetNextTlv(tlv);}
        }; //  end class ManetPkt::TlvIterator
        
        
        class MsgIterator
        {
            public:
                MsgIterator(ManetPkt& thePkt);
                ~MsgIterator();
                
                void Reset() {msg_temp.AttachBuffer(NULL, 0);}
                
                bool GetNextMessage(ManetMsg& msg, ProtoAddress::Type addrType)
                {
                    bool result = pkt.GetNextMessage(msg_temp, addrType);
                    msg = msg_temp;
                    return result;
                }
                
            private:
                ManetPkt& pkt;
                ManetMsg  msg_temp;
        }; // end class ManetPkt::MsgIterator
        
        
    private:
        bool SemanticIsSet(SemanticFlag flag) const
            {return (0 != (flag & ((UINT8*)buffer_ptr)[OFFSET_SEMANTICS]));}
        void SetSemantic(SemanticFlag flag)
            {((UINT8*)buffer_ptr)[OFFSET_SEMANTICS] |= (UINT8)flag;}
        void ClearSemantic(SemanticFlag flag)
            {((UINT8*)buffer_ptr)[OFFSET_SEMANTICS] &= (UINT8)(~flag);}
        void ClearAllSemantics()
            {((UINT8*)buffer_ptr)[OFFSET_SEMANTICS] = (UINT8)(0);}
        
        bool GetNextMessage(ManetMsg& msg, ProtoAddress::Type addrType)
        {
            char* currentBuffer = (char*)msg.GetBuffer();
            char* nextBuffer = 
                currentBuffer ? 
                    (currentBuffer + msg.GetLength()) : 
                    ((char*)buffer_ptr + OffsetPayload());
            unsigned int nextOffset = nextBuffer - (char*)buffer_ptr;
            bool result = (nextOffset < pkt_length);
            unsigned int nextBufferLen = result ? (pkt_length - nextOffset) : 0;
            return msg.InitFromBuffer(addrType, (UINT32*)nextBuffer, nextBufferLen);
        }
        
        unsigned int OffsetTlvBlock() const
            {return (OFFSET_SEMANTICS + (SemanticIsSet(NO_SEQ_NUM) ? 0 : 2));}
        
        unsigned int OffsetPayload() const
        {
            unsigned int offsetPayload = OFFSET_SEMANTICS + 1;
            offsetPayload += SemanticIsSet(NO_SEQ_NUM) ? 0 : 2;
            offsetPayload += SemanticIsSet(TLV_BLOCK) ? tlv_block.GetLength() : 0;
            return offsetPayload;   
        }
        
        static unsigned int GetMinLength(UINT8 semantics)
        {
            unsigned int minLength = 2;
            minLength += (0 != ((UINT8)NO_SEQ_NUM & semantics)) ? 0 : 2;
            minLength += (0 != ((UINT8)TLV_BLOCK & semantics)) ? 2 : 0;
            return minLength;
        }
            
        ManetTlvBlock   tlv_block;         // pkt-tlv-blk
        bool            tlv_block_pending;
        ManetMsg        msg_temp;
        bool            msg_pending;
            
        // IMPORANT NOTE: All of the offsets given here are
        // scaled according to the word size of the value
        // corresponding to the offset (e.g. offsets for 
        // 2-byte word values properly offset a UINT16 pointer)
        // 
        enum
        {
            OFFSET_ZERO       = 0,   
            OFFSET_SEMANTICS = OFFSET_ZERO + 1,
            OFFSET_SEQUENCE  = (OFFSET_SEMANTICS + 1)/2
        };
    
};  // end class ManetPkt

#endif // _MANET_MESSAGE
