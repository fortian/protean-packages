#ifndef _PROTO_MESSAGE_SINK
#define _PROTO_MESSAGE_SINK
/*!
  The ProtoMessageSink is a "middle man" base class for
  generic message passing between simulation environments
  and an mgen engine.
 */
class ProtoMessageSink
{
  public:
    ProtoMessageSink() {};
    virtual ~ProtoMessageSink() {};
    virtual bool HandleMessage(const char* txBuffer, unsigned int len,const ProtoAddress& srcAddr) {return true;}

}; // end class ProtoMessageSink
#endif // _PROTO_MESSAGE_SINK
