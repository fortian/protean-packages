
#include "protoPktIP.h"
#include "protoDebug.h"

// begin ProtoPktIP implementation
ProtoPktIP::ProtoPktIP(UINT32*        bufferPtr, 
                       unsigned int   numBytes,
                       bool           freeOnDestruct)
 : ProtoPkt(bufferPtr, numBytes, freeOnDestruct)
{
}

ProtoPktIP::~ProtoPktIP()
{
}

// begin ProtoPktIPv4 implementation
ProtoPktIPv4::ProtoPktIPv4(UINT32*      bufferPtr, 
                           unsigned int numBytes, 
                           bool         initFromBuffer,
                           bool         freeOnDestruct)
 : ProtoPktIP(bufferPtr, numBytes, freeOnDestruct)
{
    if (initFromBuffer) 
        InitFromBuffer();
    else if (NULL != bufferPtr)
        InitIntoBuffer();
}

ProtoPktIPv4::ProtoPktIPv4(ProtoPktIP & ipPkt)
 : ProtoPktIP(ipPkt.AccessBuffer(), ipPkt.GetBufferLength())
{
    InitFromBuffer();
}

ProtoPktIPv4::~ProtoPktIPv4()
{
}

bool ProtoPktIPv4::InitFromBuffer(UINT32* bufferPtr, unsigned int numBytes, bool freeOnDestruct)
{
    if (NULL != bufferPtr) 
        AttachBuffer(bufferPtr, numBytes, freeOnDestruct);
    else
        ProtoPkt::SetLength(0);
    if (GetBufferLength() > (OFFSET_VERSION+1))
    {
        if (4 != (((UINT8*)buffer_ptr)[0] >> 4))
        {
            return false;  // not an IPv4 packet!
        }
        else if (GetBufferLength() > (OFFSET_LEN + 2))
        {
            // this validates that the embedded length is <= buffer_bytes
            return ProtoPkt::InitFromBuffer(GetTotalLength());
        }
        else
        {
            return false;
        }
    }
    else
    {
        return false;
    }
}  // end ProtoPktIPv4::InitFromBuffer()


bool ProtoPktIPv4::InitIntoBuffer(UINT32* bufferPtr, unsigned int bufferBytes, bool freeOnDestruct)
{
    if (NULL != bufferPtr) 
    {
        if (bufferBytes < 20)
            return false;
        else
            AttachBuffer(bufferPtr, bufferBytes, freeOnDestruct);
    }
    else if (GetBufferLength() < 20) 
    {
        return false;
    }
    SetVersion(4);
    SetHeaderLength(20);
    SetChecksum(0);
    ((UINT16*)buffer_ptr)[OFFSET_FRAGMENT] = 0;  // init flags & frag to ZERO
    // (TBD) Set some header fields to default values? (e.g. fragment, flags)
    return true;
}  // end ProtoPktIPv4::InitIntoBuffer()

void ProtoPktIPv4::SetTOS(UINT8 tos, bool updateChecksum) 
{
    if (updateChecksum)
    {
        bool oddOffset = (0 != (0x01 & OFFSET_TOS));
        UpdateChecksum(GetTOS(), tos, oddOffset);
    }
    ((UINT8*)buffer_ptr)[OFFSET_TOS] = tos;
}  // end ProtoPktIPv4::SetTOS()

void ProtoPktIPv4::SetID(UINT16 id, bool updateChecksum) 
{
    if (updateChecksum) UpdateChecksum(GetID(), id);
    ((UINT16*)buffer_ptr)[OFFSET_ID] = htons(id);
}  // end ProtoPktIPv4::SetID()
        
void ProtoPktIPv4::SetFlag(Flag flag, bool updateChecksum) 
{
    if (updateChecksum)
    {
        bool oddOffset = (0 != (0x01 & OFFSET_FLAGS));
        UINT8 oldFlags = ((UINT8*)buffer_ptr)[OFFSET_FLAGS];
        UINT8 newFlags = oldFlags | (UINT8)flag;
        ((UINT8*)buffer_ptr)[OFFSET_FLAGS] = newFlags;
        UpdateChecksum(oldFlags, newFlags, oddOffset);
    }
    else
    {
        ((UINT8*)buffer_ptr)[OFFSET_FLAGS] |= flag;
    }
}  // end ProtoPktIPv4::SetFlag()

void ProtoPktIPv4::ClearFlag(Flag flag, bool updateChecksum)
{
    if (updateChecksum)
    {
        bool oddOffset = (0 != (0x01 & OFFSET_FLAGS));
        UINT8 oldFlags = ((UINT8*)buffer_ptr)[OFFSET_FLAGS];
        UINT8 newFlags = oldFlags & ~(UINT8)flag;
        ((UINT8*)buffer_ptr)[OFFSET_FLAGS] = newFlags;
        UpdateChecksum(oldFlags, newFlags, oddOffset);
    }
    else
    {
        ((UINT8*)buffer_ptr)[OFFSET_FLAGS] &= ~flag;
    }
} // end ProtoPktIPv4::ClearFlag()

void ProtoPktIPv4::SetFragmentOffset(UINT16 fragmentOffset, bool updateChecksum)  
{
    UINT16 fragOld = ntohs(((UINT16*)buffer_ptr)[OFFSET_FRAGMENT]);
    UINT16 fragNew = fragOld & 0xe000;
    fragNew |= (fragmentOffset & 0x1fff);
    if (updateChecksum) UpdateChecksum(fragOld, fragNew);
    ((UINT16*)buffer_ptr)[OFFSET_FRAGMENT] = htons(fragNew);
} // end ProtoPktIPv4::SetFragmentOffset()


void ProtoPktIPv4::SetTTL(UINT8 ttl, bool updateChecksum) 
{
    if (updateChecksum)
    {
        bool oddOffset = (0 != (0x01 & OFFSET_TTL));
        UpdateChecksum(GetTTL(), ttl, oddOffset);
    }
    ((UINT8*)buffer_ptr)[OFFSET_TTL] = ttl;
} // end ProtoPktIPv4::SetTTL()
   
void ProtoPktIPv4::SetProtocol(Protocol protocol, bool updateChecksum) 
{
    if (updateChecksum)
    {
        bool oddOffset = (0 != (0x01 & OFFSET_PROTOCOL));
        UpdateChecksum((UINT8)GetProtocol(), (UINT8)protocol, oddOffset);
    }
    ((UINT8*)buffer_ptr)[OFFSET_PROTOCOL] = (UINT8)protocol;
}  // end ProtoPktIPv4::SetProtocol()


void ProtoPktIPv4::SetSrcAddr(const ProtoAddress& addr, bool calculateChecksum)
{
    memcpy((char*)(buffer_ptr+OFFSET_SRC_ADDR), addr.GetRawHostAddress(), 4); // (TBD) leverage alignment?
    if (calculateChecksum) CalculateChecksum();  // (TBD) is it worth it to incrementally update
}  // end ProtoPktIPv4::SetSrcAddr() 

void ProtoPktIPv4::SetDstAddr(const ProtoAddress& addr, bool calculateChecksum)
{
    memcpy((char*)(buffer_ptr+OFFSET_DST_ADDR), addr.GetRawHostAddress(), 4); // (TBD) leverage alignment?     
    if (calculateChecksum) CalculateChecksum();  // (TBD) is it worth it to incrementally update
}  // end ProtoPktIPv4::SetDstAddr()  

void ProtoPktIPv4::SetPayloadLength(UINT16 numBytes, bool calculateChecksum)
{
    SetTotalLength(numBytes + GetHeaderLength());
    if (calculateChecksum) CalculateChecksum();
}  // end ProtoPktIPv4::SetPayloadLength()

// Return checksum in host byte order
UINT16 ProtoPktIPv4::CalculateChecksum(bool set)
{
    UINT32 sum = 0;
    UINT16* ptr = (UINT16*)buffer_ptr;
    // Calculate checksum, skipping checksum field
    unsigned int i;
    for (i = 0; i < OFFSET_CHECKSUM; i++)
        sum += ntohs(ptr[i]);
    unsigned int hdrEndex = (((UINT8*)buffer_ptr)[OFFSET_HDR_LEN] & 0x0f) << 1;
    for (i = OFFSET_CHECKSUM+1; i < hdrEndex; i++)
        sum += ntohs(ptr[i]);
    while (sum >> 16)
        sum = (sum & 0x0000ffff) + (sum >> 16);
    sum = ~sum;
    if (set) SetChecksum(sum);
    return sum;
}  // ProtoPktIPv4::CalculateChecksum()

// begin ProtoPktIPv6 implementation

ProtoPktIPv6::ProtoPktIPv6(UINT32*      bufferPtr, 
                           unsigned int numBytes, 
                           bool         initFromBuffer,
                           bool         freeOnDestruct)
 : ProtoPktIP(bufferPtr, numBytes, freeOnDestruct)
{
    if (initFromBuffer) 
        InitFromBuffer();
    else if (NULL != bufferPtr)
        InitIntoBuffer();
}

ProtoPktIPv6::ProtoPktIPv6(ProtoPktIP & ipPkt)
 : ProtoPktIP(ipPkt.AccessBuffer(), ipPkt.GetBufferLength()),
   ext_pending(false)
{
    InitFromBuffer();
}

ProtoPktIPv6::~ProtoPktIPv6()
{
}

bool ProtoPktIPv6::InitFromBuffer(UINT32* bufferPtr, unsigned int numBytes, bool freeOnDestruct)
{
    ext_pending = false;
    if (NULL != bufferPtr) 
        AttachBuffer(bufferPtr, numBytes, freeOnDestruct);
    else
        ProtoPkt::SetLength(0);
    if (GetBufferLength() > OFFSET_VERSION)
    {
        if (6 != (((UINT8*)buffer_ptr)[0] >> 4))
        {
            DMSG(0, "ProtoPktIPv6::InitFromBuffer() error: invalid version number\n");
            return false;  // not an IPv6 packet!
        }
        else if (GetBufferLength() > (2*OFFSET_LENGTH + 2))
        {
            // this validates that the embedded length is <= buffer_bytes
            if (ProtoPkt::InitFromBuffer(40 + GetPayloadLength()))
            {
                return true;
            }
            else
            {
                DMSG(0, "ProtoPktIPv6::InitFromBuffer() error: invalid packet length?\n");
                return false;
            }
        }
        else
        {
            DMSG(0, "ProtoPktIPv6::InitFromBuffer() error: insufficient buffer space (2)\n");
            return false;
        }
    }
    else
    {
        DMSG(0, "ProtoPktIPv6::InitFromBuffer() error: insufficient buffer space (1)\n");
        return false;
    }
}  // end ProtoPktIPv6::InitFromBuffer()


bool ProtoPktIPv6::InitIntoBuffer(UINT32*       bufferPtr, 
                                  unsigned int  bufferBytes, 
                                  bool          freeOnDestruct)
{
    if (NULL != bufferPtr)
    {
        if (bufferBytes < 40)
            return false;
        else
            AttachBuffer(bufferPtr, bufferBytes, freeOnDestruct);
    }
    else if (GetBufferLength() < 40) 
    {
        return false;
    }
    SetVersion(6);
    SetTrafficClass(0);
    SetFlowLabel(0);
    SetPayloadLength(0);
    SetNextHeader(NONE);
    SetLength(40);
    ext_pending = false;
    return true;
}  // end ProtoPktIPv6::InitIntoBuffer()

// Map extension to end of current IPv6 packet header
ProtoPktIPv6::Extension* ProtoPktIPv6::AddExtension(Protocol extensionType)
{
    if (NONE == extensionType) return NULL;
    unsigned int hdrBytes = 40;
    if (ext_pending)
    {
        PackHeader(extensionType);
        hdrBytes = pkt_length;
    }
    else if (HasExtendedHeader())
    {
        Extension::Iterator iterator(*this);
        Extension lastExt, x;
        while (iterator.GetNextExtension(x))
        {
            hdrBytes += x.GetLength();
            lastExt = x;
        }
        lastExt.SetNextHeader(extensionType);
    }
    else
    {
        if (buffer_bytes <=  hdrBytes) return NULL;
        // (TBD) Should we return NULL if (NONE != GetNextHeader())???
        ASSERT(NONE == GetNextHeader());
        SetNextHeader(extensionType);
    }
    ASSERT(0 == (hdrBytes & 0x03));
    ext_temp.AttachBuffer(buffer_ptr + (hdrBytes >> 2), buffer_bytes - hdrBytes);
    ext_temp.SetType(extensionType);
    ext_pending = true;
    return &ext_temp;
}  // end ProtoPktIPv6::AddExtension()

bool ProtoPktIPv6::PackHeader(Protocol nextHeaderType)
{
    if (ext_pending)
    {
        if (!ext_temp.Pack()) return false;
        ext_temp.SetNextHeader(nextHeaderType);
        // Update IPv6 packet payload length field
        SetPayloadLength(GetPayloadLength() + ext_temp.GetLength());
        ext_pending = false;
    }
    else if (HasExtendedHeader())
    {
        // (TBD) Find last header extension and set next header type?
    }
    else
    {
        SetNextHeader(nextHeaderType);
    }
    return true;
}  // end ProtoPktIPv6::PackHeader()

// Copy extension to beginning of packet's set of header extensions (payload is moved if present)
bool ProtoPktIPv6::PrependExtension(Extension& ext)
{
    if (ext_pending) PackHeader();
    // 1) Is there room in the IPv6 packer buffer_ptr for the extension?
    if (buffer_bytes < (pkt_length + ext.GetLength())) return false;
    // 2) Copy current packet next header to our ext next header
    ext.SetNextHeader(GetNextHeader());
    // 3) Move the current payload to make room for the extension
    UINT16 payloadLength = GetPayloadLength();
    char* ptr = ((char*)buffer_ptr) + 40;
    memmove(ptr+ext.GetLength(), ptr, payloadLength);
    // 4) Copy extension buffer_ptr into space made available
    memcpy(ptr, (const char*)ext.GetBuffer(), ext.GetLength());
    SetNextHeader(ext.GetType());
    SetPayloadLength(payloadLength + ext.GetLength());
    return true;
}  // end ProtoPktIPv6::PrependExtension()


bool ProtoPktIPv6::ReplaceExtension(Extension& oldExt, Extension& newExt)
{
    ASSERT((oldExt.GetBuffer() >= ((char*)GetBuffer() + 40)) && 
           (oldExt.GetBuffer() < ((char*)GetBuffer() + GetLength())));
    
    if (oldExt.GetType() == newExt.GetType())
    {
        int spaceAvailable = GetBufferLength() - GetLength();
        int spaceDelta = newExt.GetLength() - oldExt.GetLength();
        if (spaceDelta > spaceAvailable)
        {
            DMSG(0, "ProtoPktIPv6::ReplaceExtension() error: insufficient buffer space!\n");
            return false;
        }
        newExt.SetNextHeader(oldExt.GetNextHeader());
        char* dataPtr = (char*)oldExt.AccessBuffer() + oldExt.GetLength();
        UINT16 dataLen = (char*)GetBuffer() + GetLength() - dataPtr;
        memmove(dataPtr+spaceDelta, dataPtr, dataLen);
        memcpy((char*)oldExt.AccessBuffer(), (char*)newExt.GetBuffer(), newExt.GetLength());
        SetPayloadLength(GetPayloadLength() + spaceDelta);
        return true;
    }
    else
    {
        // (TBD) we could mod to allow a new type of extension to replace the old one
        DMSG(0, "ProtoPktIPv6::ReplaceExtension() error: new extension is of different type!\n");
        return false;
    }
}  // end ProtoPktIPv6::ReplaceExtension()

// Copy pre-built extension to end of current IPv6 header (payload is moved if present)
bool ProtoPktIPv6::AppendExtension(Extension& ext)
{
    unsigned int hdrBytes = 40;
    if (ext_pending)
    {
        
        PackHeader(ext.GetType());
        // Is there room in the IPv6 packer buffer_ptr for the additional extension?
        if (buffer_bytes < (pkt_length + ext.GetLength())) return false;
        hdrBytes = pkt_length;
    }
    else if (HasExtendedHeader())
    {
        // Is there room in the IPv6 packer buffer_ptr for the additional extension?
        if (buffer_bytes < (pkt_length + ext.GetLength())) return false;
        Extension::Iterator iterator(*this);
        Extension lastExt, x;
        while (iterator.GetNextExtension(x))
        {
            hdrBytes += x.GetLength();
            lastExt = x;
        }
        ext.SetNextHeader(lastExt.GetNextHeader());
        lastExt.SetNextHeader(ext.GetType());
    }
    else
    {
        // Is there room in the IPv6 packer buffer_ptr for the additional extension?
        if (buffer_bytes < (pkt_length + ext.GetLength())) return false;
        ext.SetNextHeader(GetNextHeader());
        SetNextHeader(ext.GetType());
    }
    char* ptr = ((char*)buffer_ptr) + hdrBytes;
    UINT16 payloadLength = GetPayloadLength();
    UINT16 moveLength = payloadLength + 40 - hdrBytes;
    memmove(ptr + ext.GetLength(), ptr, moveLength);
    memcpy(ptr, (const char*)ext.GetBuffer(), ext.GetLength());
    SetPayloadLength(payloadLength + ext.GetLength());
    return true;
}  // end ProtoPktIPv6::AppendExtension()

bool ProtoPktIPv6::SetPayload(Protocol payloadType, const char* dataPtr, UINT16 dataLen)
{
    if (ext_pending)
    {
        // Is there room for the payload?
        if (buffer_bytes < (pkt_length + ext_temp.GetLength() + dataLen)) return false;
        PackHeader(payloadType);
    }
    else if (HasExtendedHeader())
    {
        // Is there room for the payload?
        if (buffer_bytes < (pkt_length + dataLen)) 
        {
            DMSG(0, "ProtoPktIPv6::SetPayload() error: insufficient buffer space (1)\n");
            return false;
        }
        ASSERT(GetPayloadLength() > 0);
        // Find last extension header
        Extension::Iterator iterator(*this);
        Extension lastExt, x;
        while (iterator.GetNextExtension(x))
            lastExt = x;
        ASSERT(NONE == lastExt.GetNextHeader());
        lastExt.SetNextHeader(payloadType);
    }
    else
    {
        // Is there already a payload set?
        ASSERT(NONE == GetNextHeader());
        // Is there room for the payload?
        if (buffer_bytes < (pkt_length + dataLen)) 
        {
            DMSG(0, "ProtoPktIPv6::SetPayload() error: insufficient buffer space (2)\n");
            return false;
        }
        ASSERT(0 == GetPayloadLength());
        SetNextHeader(payloadType);
    }
    // 3) Copy payload into buffer_ptr space
    memcpy(((char*)buffer_ptr) + pkt_length, dataPtr, dataLen);
    SetPayloadLength(GetPayloadLength() + dataLen);
    return true;
}  // end ProtoPktIPv6::SetPayload()
                    

ProtoPktIPv6::Extension::Extension(Protocol     extType, 
                                   UINT32*      bufferPtr, 
                                   unsigned int numBytes, 
                                   bool         initFromBuffer, 
                                   bool         freeOnDestruct)
 : ProtoPkt(bufferPtr, numBytes, freeOnDestruct), ext_type(NONE), opt_pending(false), opt_packed(false)
{
    if (initFromBuffer) 
        InitFromBuffer(extType, bufferPtr, numBytes, freeOnDestruct);
    else
        InitIntoBuffer(extType);
}

ProtoPktIPv6::Extension::~Extension()
{
}

bool ProtoPktIPv6::Extension::Copy(const ProtoPktIPv6::Extension& ext)
{
    if ((NULL == buffer_ptr) || (ext.GetLength() > buffer_bytes))
    {
        DMSG(0, "ProtoPktIPv6::Extension::Copy() error: insufficient buffer size\n");
        return false;
    }
    // a) save our buffer pointer and length
    UINT32* bufferPtr = buffer_ptr;
    UINT32 bufferBytes = buffer_bytes;
    // b) copy all member variable values
    *this = ext;
    // c) restore buffer pointer and length
    buffer_ptr = bufferPtr;
    buffer_bytes = bufferBytes;
    // d) copy buffer content
    memcpy(buffer_ptr, ext.GetBuffer(), ext.GetLength());
    return true;
}  // end ProtoPktIPv6::Extension::Copy()

bool ProtoPktIPv6::Extension::InitIntoBuffer(Protocol       extType,
                                             UINT32*        bufferPtr, 
                                             unsigned int   numBytes, 
                                             bool           freeOnDestruct)
{
    if (NULL != bufferPtr) AttachBuffer(bufferPtr, numBytes, freeOnDestruct);
    ext_type = extType;
    if (NULL == buffer_ptr) return true;  // don't worry if no buffer_ptr for moment
    if (GetBufferLength() > OFFSET_NEXT_HDR) SetNextHeader(NONE);
    switch (extType)
    {
        // These types have length fields
        default:
            DMSG(4, "ProtoPktIPv6::Extension::InitIntoBuffer() warning: unknown extension type\n");
            // For now we'll assume it has a length field ala the options and routing extensions
        case NONE:
        case HOPOPT:
        case DSTOPT:
        case RTG:
        case AUTH:
            if (GetBufferLength() > OFFSET_LENGTH) 
            {
                pkt_length = 2;
                break;
            }
            else
            {
                DMSG(0, "ProtoPktIPv6::Extension::InitIntoBuffer() error: insufficient buffer space\n");
                SetLength(0);
                if (NULL != bufferPtr)
                    buffer_ptr = buffer_allocated = NULL;
                return false;
            }
        case FRAG:
            if (GetBufferLength() > OFFSET_LENGTH) SetExtensionLength(8);
            break;
    }
    opt_pending = opt_packed = false;
    return true;
}  // ProtoPktIPv6::Extension::InitIntoBuffer()

// Set length field (as applicable) of extension header from units of bytes
void ProtoPktIPv6::Extension::SetExtensionLength(UINT16 numBytes)
{
    switch (GetType())
    {
        default:
            DMSG(0, "ProtoPktIPv6::Extension::SetExtensionLength() unknown extension type\n");
            // For now we'll assume it has a length field ala the options and routing extensions
        case HOPOPT:
        case DSTOPT:
        case RTG:
            ASSERT(0 == (0x07 & numBytes));
            ((UINT8*)buffer_ptr)[OFFSET_LENGTH] = (UINT8)((numBytes - 8) >> 3);
            break;
        case AUTH:
            ASSERT(0 == (0x03 & numBytes));
            ((UINT8*)buffer_ptr)[OFFSET_LENGTH] = (UINT8)((numBytes - 4) >> 2);
        case FRAG:
            break;
    }
    pkt_length = numBytes;
}  // end ProtoPktIPv6::Extension::SetExtensionLength()

ProtoPktIPv6::Option* ProtoPktIPv6::Extension::AddOption(Option::Type optType)
{
    if (opt_packed)
    {
        ASSERT(!opt_pending);
        // Find first PADx option and truncate extension length
        Option::Iterator iterator(*this);
        Option opt;
        while (iterator.GetNextOption(opt))
        {
            Option::Type otype = opt.GetType();
            if ((Option::PAD1 == otype) || (Option::PADN == otype))
            {
                unsigned int extLen = opt.GetBuffer() - GetBuffer();
                if (extLen != (pkt_length - opt.GetLength()))
                    DMSG(0, "ProtoPktIPv6::Extension::AddOption() warning: extension used multiple PADS ?!\n");
                pkt_length = extLen;
                break;
            }
        }
        opt_packed = false;
    }
    else if (opt_pending) 
    {
        PackOption();
    }
    // If there is any room in the buffer, init the new option into the available space
    unsigned int minLength = (Option::PAD1 == optType) ? 1 : 2;
    unsigned int bufferSpace = buffer_bytes - pkt_length;
    if ((buffer_bytes - pkt_length) < minLength)
    {
        DMSG(0, "ProtoPktIPv6::Extension::AddOption() error: insufficient buffer space\n");
        return NULL;
    }        
    opt_temp.InitIntoBuffer(optType, ((char*)buffer_ptr) + pkt_length, bufferSpace);
    if (Option::PAD1 != optType) opt_temp.SetData(NULL, 0);
    opt_pending = true;
    return &opt_temp;
}  // end ProtoPktIPv6::Extension::AddOption()

bool ProtoPktIPv6::Extension::Pack()
{
    if (IsOptionHeader()) 
    {
        if (opt_pending) PackOption();
        if (PadOptionHeader())
        {
            opt_packed = true;
            SetExtensionLength(pkt_length);
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        opt_packed = true;
        SetExtensionLength(pkt_length);
        return true;
    }   
}  // end ProtoPktIPv6::Extension::Pack()

void ProtoPktIPv6::Extension::PackOption()
{
    if (opt_pending)
    {
        // Update Extension payload length field
        //SetExtensionLength(pkt_length + opt_temp.GetLength());
        pkt_length += opt_temp.GetLength();
        opt_pending = false;
    }
}  // end ProtoPktIPv6::Extension::PackOption()

bool ProtoPktIPv6::Extension::PadOptionHeader()
{
    PackOption();
    UINT8 padBytes = (UINT8)(GetLength() & 0x07);
    if (0 != padBytes)
        padBytes = 8 - padBytes;
    else
        return true;  // no padding was needed
    Option* opt = AddOption((1 == padBytes) ? Option::PAD1 : Option::PADN);
    if ((NULL != opt) && opt->MakePad(padBytes))
    {
        PackOption();
        return true;
    }
    else
    {
        DMSG(0, "ProtoPktIPv6::Extension::PadOptionHeader() error: insufficient buffer space\n");
        return false;
    }
}  // end ProtoPktIPv6::Extension::PadOptionHeader()


bool ProtoPktIPv6::Extension::InitFromBuffer(Protocol extType, UINT32* bufferPtr, unsigned int numBytes, bool freeOnDestruct)
{
    if (NULL != bufferPtr) 
        AttachBuffer(bufferPtr, numBytes, freeOnDestruct);
    if (GetBufferLength() >= OFFSET_LENGTH)
    {
        // set ProtoPkt "pkt_length" and check that it is bounds of buffer_ptr
        ext_type = extType;
        opt_pending = false;
        opt_packed = true;
        return ProtoPkt::InitFromBuffer(GetExtensionLength());
    }
    else
    {
        if (NULL != bufferPtr) 
            buffer_ptr = buffer_allocated = NULL;
        ProtoPkt::SetLength(0);
        return false;
    }
}  // end ProtoPktIPv6::Extension::InitFromBuffer()

// Return length of extension header in bytes
UINT16 ProtoPktIPv6::Extension::GetExtensionLength() const
{
    switch (GetType())
    {
        default:
            DMSG(0, "ProtoPktIPv6::Extension::GetExtensionLength() unknown extension type\n");
            // For now we'll assume it has a length field ala the options and routing extensions
        case HOPOPT:
        case DSTOPT:
        case RTG:
            return (8 + ((UINT16)(((UINT8*)buffer_ptr)[OFFSET_LENGTH]) << 3));
        case AUTH:
            return (4 + ((UINT16)(((UINT8*)buffer_ptr)[OFFSET_LENGTH]) << 2));
        case FRAG:
            return 8;
    }
}  // end ProtoPktIPv6::Extension::GetExtensionLength()

// class ProtoPktIPv6::Extension::Iterator implementation

ProtoPktIPv6::Extension::Iterator::Iterator(const ProtoPktIPv6& pkt)
 : ipv6_pkt(pkt), next_hdr(pkt.GetNextHeader()), offset(40)
{   
}

ProtoPktIPv6::Extension::Iterator::~Iterator()
{
}

bool ProtoPktIPv6::Extension::Iterator::GetNextExtension(Extension& extension)
{
    if ((6 != ipv6_pkt.GetVersion()) || (offset >= ipv6_pkt.GetLength())) return false;
    if (IsExtension(next_hdr))
    {
        if (extension.InitFromBuffer(next_hdr, (UINT32*)ipv6_pkt.GetBuffer()+(offset >> 2), ipv6_pkt.GetLength()-offset))
        {
            next_hdr = extension.GetNextHeader();
            offset += extension.GetLength();
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        return false;
    }
}  // end ProtoPktIPv6::Extension::GetNextExtension()

ProtoPktIPv6::Option::Option(char* bufferPtr, unsigned int numBytes, bool initFromBuffer, bool freeOnDestruct)
 : buffer_ptr(bufferPtr), buffer_allocated(freeOnDestruct ? bufferPtr : NULL), buffer_bytes(numBytes)
{
    if (initFromBuffer) InitFromBuffer();
}

ProtoPktIPv6::Option::~Option()
{
    if (NULL != buffer_allocated)
    {
        delete[] buffer_allocated;
        buffer_allocated = NULL;
    }
}

bool ProtoPktIPv6::Option::InitFromBuffer(char* bufferPtr, unsigned int numBytes, bool freeOnDestruct)
{
    if (NULL != bufferPtr) AttachBuffer(bufferPtr, numBytes, freeOnDestruct);
    if (buffer_bytes > 0)
    {
        if (PAD1 == GetType())
        {
            return true;
        }
        else if (buffer_bytes > 1)
        {
            // check that the embedded option length is in bounds
            if ((unsigned int)(OFFSET_DATA + GetDataLength()) > numBytes)  
            {
                return false;       
            }
            else
            {
                return true;
            }
        }
        else
        {
            return false;
        }
    }
    else
    {
        return false;
    }
}  // end ProtoPktIPv6::Option::InitFromBuffer()

bool ProtoPktIPv6::Option::InitIntoBuffer(Type         type,
                                          char*        bufferPtr, 
                                          unsigned int numBytes, 
                                          bool         freeOnDestruct)
{
    UINT8 minLen = (type == PAD1) ? 1 : 2;
    if (NULL != bufferPtr)
    {
        if (numBytes < minLen)
            return false;
        else
            AttachBuffer(bufferPtr, numBytes, freeOnDestruct);
    }
    else if (GetBufferLength() < minLen) 
    {
        return false;
    }
    SetUnknownPolicy(SKIP);
    SetMutable(false);
    SetType(type);
    if (PAD1 != type) SetDataLength(0);
    return true;
}  // end ProtoPktIPv6::Option::InitIntoBuffer()

bool ProtoPktIPv6::Option::SetData(char* dataPtr, UINT8 dataLen)
{
    if (dataLen < (buffer_bytes - OFFSET_DATA))
    {
        memcpy(buffer_ptr + OFFSET_DATA, dataPtr, dataLen);
        SetDataLength(dataLen);
        return true;
    }
    else
    {
        return false;
    }
}  // end ProtoPktIPv6::Option::SetData()

bool ProtoPktIPv6::Option::MakePad(UINT8 numBytes)
{
    if (buffer_bytes > OFFSET_TYPE)
    {
        if (numBytes > 1)
        {
            if (buffer_bytes > (unsigned int)(OFFSET_TYPE + numBytes))
            {
                SetType(PADN);
                memset(buffer_ptr + OFFSET_DATA, 0, numBytes-2);
                SetDataLength(numBytes - (OFFSET_TYPE+2));
                return true;
            }
            else
            {
                DMSG(0, "ProtoPktIPv6::Option::MakePad() error: insufficient buffer space\n");
                return false;
            }
        }
        else if (1 == numBytes)
        {
            SetType(PAD1);
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        DMSG(0, "ProtoPktIPv6::Option::MakePad() error: no buffer space allocated\n");
        return false;
    }
}  // end ProtoPktIPv6::Option::MakePad()

ProtoPktIPv6::Option::Iterator::Iterator(const Extension& extension)
 : hdr_extension(extension), offset(2)
{
    
}

ProtoPktIPv6::Option::Iterator::~Iterator()
{
}

bool ProtoPktIPv6::Option::Iterator::GetNextOption(Option& option)
{
    if (offset >= hdr_extension.GetLength())
    {
    
        return false;
    }
    else if (option.InitFromBuffer(((char*)hdr_extension.GetBuffer()) + offset,
                                   hdr_extension.GetLength() - offset))
    {
        offset += option.GetLength();
        return true;
    }
    else
    {
        return false;
    }
}  // end ProtoPktIPv6::Option::Iterator::GetNextOption()


ProtoPktAUTH::ProtoPktAUTH(UINT32*       bufferPtr, 
                       unsigned int  numBytes, 
                       bool          initFromBuffer,
                       bool          freeOnDestruct)
 : ProtoPktIPv6::Extension(ProtoPktIP::AUTH, bufferPtr, numBytes, initFromBuffer, freeOnDestruct)
{
}

ProtoPktAUTH::~ProtoPktAUTH()
{
}

bool ProtoPktAUTH::InitIntoBuffer(UINT32*       bufferPtr, 
                                unsigned int  numBytes, 
                                bool          freeOnDestruct)
{
    if (ProtoPktIPv6::Extension::InitIntoBuffer(ProtoPktIP::AUTH, bufferPtr, numBytes, freeOnDestruct))
    {
        // Make sure it's big enough for at least reserved, spi, and sequence fields
        unsigned int minLength = ((OFFSET_SEQUENCE+1) << 2);
        if (GetBufferLength() < minLength)
        {
            SetLength(0);
            if (bufferPtr != NULL)
            {
                buffer_ptr = buffer_allocated = NULL;
                buffer_bytes = 0;
            }
            return false;
        }
        *(((UINT16*)buffer_ptr) + OFFSET_RESERVED) = 0;
        SetLength(minLength);
        return true;
    }
    else
    {
        return false;
    }
}  // end ProtoPktAUTH::InitIntoBuffer()

bool ProtoPktAUTH::InitFromBuffer(UINT32* bufferPtr, unsigned int numBytes, bool freeOnDestruct)
{
    if (ProtoPktIPv6::Extension::InitFromBuffer(ProtoPktIP::AUTH, bufferPtr, numBytes, freeOnDestruct))
    {
        if (GetBufferLength() >= ((OFFSET_SEQUENCE+1) << 2))
        {
            return true;
        }
        else
        {
            SetLength(0);
            if (NULL != bufferPtr)
            {
                buffer_ptr = buffer_allocated = NULL;
                buffer_bytes = 0;
            }
            return false;
        }
    }
    else
    {
        return false;
    }
}  // end ProtoPktAUTH::InitFromBuffer()


ProtoPktESP::ProtoPktESP(UINT32*       bufferPtr, 
                         unsigned int  numBytes, 
                         bool          freeOnDestruct)
: ProtoPkt(bufferPtr, numBytes, freeOnDestruct)
{
}
        
ProtoPktESP::~ProtoPktESP()
{
}

bool ProtoPktESP::InitIntoBuffer(UINT32*       bufferPtr, 
                                 unsigned int  numBytes, 
                                 bool          freeOnDestruct)
{
    if (NULL != bufferPtr) AttachBuffer(bufferPtr, numBytes, freeOnDestruct);
    if (NULL == buffer_ptr) return true;  // don't worry if no buffer_ptr for moment
    // Make sure it's big enough for at least spi and sequence fields
    unsigned int minLength = ((OFFSET_SEQUENCE+1) << 2);
    if (GetBufferLength() >= minLength)
    {
        ProtoPkt::SetLength(minLength);
        return true;
    }
    else
    {
        ProtoPkt::SetLength(0);
        if (NULL != bufferPtr)
            buffer_ptr = buffer_allocated = NULL;
        return false;   
    }
}  // end ProtoPktESP::InitIntoBuffer()

bool ProtoPktESP::InitFromBuffer(UINT16 espLength, UINT32* bufferPtr, unsigned int numBytes, bool freeOnDestruct)
{
    if (NULL != bufferPtr) 
        AttachBuffer(bufferPtr, numBytes, freeOnDestruct);
    // Make sure it's big enough for at least spi and sequence fields
    unsigned int minLength = ((OFFSET_SEQUENCE+1) << 2);
    if (GetBufferLength() >= minLength)
    {
        SetLength(espLength);
        return true;
    }
    else
    {
        ProtoPkt::SetLength(0);
        if (NULL != bufferPtr)
            buffer_ptr = buffer_allocated = NULL;
        return false;  
    }
}  // end ProtoPktESP::InitFromBuffer()


ProtoPktDPD::ProtoPktDPD(char*        bufferPtr, 
                         unsigned int numBytes, 
                         bool         initFromBuffer,
                         bool         freeOnDestruct)
 : ProtoPktIPv6::Option(bufferPtr, numBytes, false, freeOnDestruct)
{
    if (initFromBuffer)
        InitFromBuffer();
    else if (NULL != bufferPtr)
        InitIntoBuffer();
}

ProtoPktDPD::~ProtoPktDPD()
{
    if (NULL != buffer_allocated)
    {
        delete[] buffer_allocated;
        buffer_allocated = NULL;
    }
}

bool ProtoPktDPD::InitFromBuffer(char*          bufferPtr,
                                 unsigned int   numBytes,
                                 bool           freeOnDestruct)
{
    if (NULL != bufferPtr)
        AttachBuffer(bufferPtr, numBytes, freeOnDestruct);
    if ((numBytes < (OFFSET_DATA_LENGTH + 1)) ||
        (SMF_DPD != GetType()))
    {
        if (NULL != bufferPtr) DetachBuffer();
        return false;
    }
    unsigned int dataLength = GetDataLength();
    if ((dataLength < 1) || (numBytes < (OFFSET_DATA_LENGTH + 1 + dataLength)))
    {
        if (NULL != bufferPtr) DetachBuffer();
        return false;
    }
    if (GetTaggerIdLength() >= dataLength)
    {
        if (NULL != bufferPtr) DetachBuffer();
        return false;
    }
    return true;
}  // end ProtoPktDPD::InitFromBuffer()

bool ProtoPktDPD::GetTaggerId(ProtoAddress& addr) const
{
    switch (GetTaggerIdType())
    {
        case TID_IPv4_ADDR:
            if (4 == GetTaggerIdLength())
            {
                addr.SetRawHostAddress(ProtoAddress::IPv4, GetTaggerId(), 4);
                return true;
            }
            else
            {
                return false;
            }
        case TID_IPv6_ADDR:
            if (4 == GetTaggerIdLength())
            {
                addr.SetRawHostAddress(ProtoAddress::IPv6, GetTaggerId(), 16);
                return true;
            }
            else
            {
                return false;
            }
            return true;
        default:
            return false;
    }
}  // end ProtoPktDPD::GetTaggerId(by address)

bool ProtoPktDPD::GetPktId(UINT8& value) const
{
    if (1 == GetPktIdLength())
    {
        value = buffer_ptr[OffsetPktId()];
        return true;
    }
    else
    {
        return false;
    }
}  // end ProtoPktDPD::GetPktId(UINT8& value)

bool ProtoPktDPD::GetPktId(UINT16& value) const
{
    if (2 == GetPktIdLength())
    {
        memcpy(&value, buffer_ptr+OffsetPktId(), 2);
        value = ntohs(value);
        return true;
    }
    else
    {
        return false;
    }
}  // end ProtoPktDPD::GetPktId(UINT16& value)

bool ProtoPktDPD::GetPktId(UINT32& value) const
{
    if (4 == GetPktIdLength())
    {
        memcpy(&value, buffer_ptr+OffsetPktId(), 4);
        value = ntohl(value);
        return true;
    }
    else
    {
        return false;
    }
}  // end ProtoPktDPD::GetPktId(UINT16& value)

bool ProtoPktDPD::InitIntoBuffer(char*          bufferPtr,
                                 unsigned int   numBytes,
                                 bool           freeOnDestruct)
{
    if (NULL != bufferPtr)
        AttachBuffer(bufferPtr, numBytes, freeOnDestruct);
    if (numBytes < (OFFSET_DATA_LENGTH + 1))
    {
        if (NULL != bufferPtr) DetachBuffer();
        return false;
    }
    // Set NULL tagger id
    buffer_ptr[OFFSET_TID_TYPE] = 0;
    SetDataLength(1);
    return 0;
}  // end ProtoPktDPD::InitIntoBuffer()

bool ProtoPktDPD::SetTaggerId(TaggerIdType type, const char* taggerId, UINT8 taggerIdLength)
{
    if ((TID_NULL != type) && (0 != taggerIdLength))
    {
        unsigned int minLength = OFFSET_TID_VALUE + taggerIdLength;
        if (buffer_bytes < minLength) return false;
        buffer_ptr[OFFSET_TID_TYPE] = (char)(type << 4);
        buffer_ptr[OFFSET_TID_LENGTH] |= ((taggerIdLength - 1) & 0x0f);
        memcpy(buffer_ptr + OFFSET_TID_VALUE, taggerId, taggerIdLength);
        SetDataLength(1 + taggerIdLength);
    }
    else
    {
        buffer_ptr[OFFSET_TID_TYPE] = 0;
        SetDataLength(1);
    }
    return true;
}  // end ProtoPktDPD::SetTaggerId(by buffer)


bool ProtoPktDPD::SetTaggerId(const ProtoAddress& ipAddr)
{
    switch (ipAddr.GetType())
    {
        case ProtoAddress::IPv4:
            return SetTaggerId(TID_IPv4_ADDR, ipAddr.GetRawHostAddress(), 4);
        case ProtoAddress::IPv6:
            return SetTaggerId(TID_IPv6_ADDR, ipAddr.GetRawHostAddress(), 16);
        default:
            DMSG(0, "ProtoPktDPD::SetTaggerId() error: invalid address type\n");
            return false;
    }
}  // ProtoPktDPD::SetTaggerId(by address)

bool ProtoPktDPD::SetPktId(const char* pktId, UINT8 pktIdLength)
{
    unsigned int taggerIdLength = GetTaggerIdLength();
    unsigned int offsetPktId = OFFSET_TID_VALUE + taggerIdLength;
    unsigned int minLength = offsetPktId + pktIdLength;
    if (buffer_bytes < minLength) return false;
    memcpy(buffer_ptr + offsetPktId, pktId, pktIdLength);
    SetDataLength(1 + taggerIdLength + pktIdLength);
    return true;   
}

ProtoPktUDP::ProtoPktUDP(UINT32*        bufferPtr, 
                         unsigned int   numBytes, 
                         bool           initFromBuffer,
                         bool           freeOnDestruct)
 : ProtoPkt(bufferPtr, numBytes, freeOnDestruct)
{
    if (initFromBuffer) 
        InitFromBuffer(); 
    else if (NULL != bufferPtr)
        InitIntoBuffer();
}

ProtoPktUDP::~ProtoPktUDP()
{
}

bool ProtoPktUDP::InitFromBuffer(UINT32*        bufferPtr, 
                                 unsigned int   numBytes, 
                                 bool           freeOnDestruct)
{
    if (NULL != bufferPtr) 
        AttachBuffer(bufferPtr, numBytes, freeOnDestruct);
    UINT16 totalLen = GetPayloadLength() + 8;
    if (totalLen > buffer_bytes)
    {
        pkt_length = 0;
        if (NULL != bufferPtr) DetachBuffer();
        return false;
    }
    else
    {
        // (TBD) We could validate the checksum, too?
        pkt_length = totalLen;
        return true;
    }
}  // end bool ProtoPktUDP::InitFromBuffer()

bool ProtoPktUDP::InitIntoBuffer(UINT32*        bufferPtr, 
                                 unsigned int   numBytes, 
                                 bool           freeOnDestruct) 
{
    if (NULL != bufferPtr)
    {
        if (numBytes < 8)  // UDP header size
            return false;
        else
            AttachBuffer(bufferPtr, numBytes, freeOnDestruct);
    }
    if (GetBufferLength() < 8) return false;
    SetChecksum(0);
    return true;
}  // end ProtoPktUDP::InitIntoBuffer()
                
UINT16 ProtoPktUDP::CalculateChecksum(ProtoPktIP& ipHdr, bool set)
{
    UINT32 sum = 0;
    
    // 1) Calculate pseudo-header part
    switch(ipHdr.GetVersion())
    {
        case 4:
        {
            ProtoPktIPv4 ipv4Hdr(ipHdr.AccessBuffer(), ipHdr.GetLength());
            // a) srsc/dst addr pseudo header portion
            const UINT16* ptr = (const UINT16*)ipv4Hdr.GetAddrPtr();
            int addrEndex = ProtoPktIPv4::ADDR_LEN;  // note src + dst = 2 addresses
            for (int i = 0; i < addrEndex; i++)
                sum += ntohs(ptr[i]);
            // b) protocol & "total length" pseudo header portions
            sum += (UINT16)ipv4Hdr.GetProtocol();
            sum += ipv4Hdr.GetPayloadLength();
            break;
        }
        case 6:
        {
            ProtoPktIPv6 ipv6Hdr(ipHdr.AccessBuffer(), ipHdr.GetLength());
            // a) srsc/dst addr pseudo header portion
            const UINT16* ptr = (const UINT16*)ipv6Hdr.GetAddrPtr();
            int addrEndex = ProtoPktIPv6::ADDR_LEN;  // note src + dst = 2 addresses
            for (int i = 0; i < addrEndex; i++)
                sum += ntohs(ptr[i]);
            sum += ipv6Hdr.GetPayloadLength();
            sum += (UINT16)ipv6Hdr.GetNextHeader();
            break;
        }
        default:
            return 0;   
    }
    // 2) UDP header part, sans "checksum" field
    const UINT16* ptr = (UINT16*)GetBuffer();
    unsigned int i;
    for (i = 0; i < OFFSET_CHECKSUM; i++)
        sum += ntohs(ptr[i]);
    // 3) UDP payload part (note adjustment for odd number of payload bytes)
    unsigned int dataEndex = pkt_length;
    if (0 != (dataEndex & 0x01))
        sum += ntohs((((UINT16)((UINT8*)ptr)[dataEndex-1]) << 8));
    dataEndex >>= 1;  // convert from bytes to UINT16 index
    for (i = (OFFSET_CHECKSUM+1); i < dataEndex; i++)
        sum += ntohs(ptr[i]);
    // 4) Carry as needed
    while (0 != (sum >> 16))
        sum = (sum & 0x0000ffff) + (sum >> 16);
    sum = ~sum;
    // 5) ZERO check/correct as needed
    if (0 == sum) sum = 0xffff;
    if (set) SetChecksum(sum);
    return sum;
}  // end ProtoPktUDP::CalculateChecksum()
