

#include "protoChannel.h"
#include "protoDebug.h"

#ifdef WIN32
const ProtoChannel::Handle ProtoChannel::INVALID_HANDLE = INVALID_HANDLE_VALUE;
#else
#include <unistd.h>
#include <fcntl.h>
const ProtoChannel::Handle ProtoChannel::INVALID_HANDLE = -1;
#endif  // if/else WIN32/UNIX

ProtoChannel::ProtoChannel()
 : listener(NULL), notifier(NULL), notify_flags(0), 
#ifdef WIN32
   input_handle(INVALID_HANDLE), input_ready(false), 
   output_handle(INVALID_HANDLE), output_ready(false) 
#else
   descriptor(INVALID_HANDLE)
#endif // if/else WIN32/UNIX
{
    
}

ProtoChannel::~ProtoChannel()
{
    if (notifier) SetNotifier(NULL);
    if (listener)
    {
        delete listener;
        listener = NULL;  
    }
}


bool ProtoChannel::SetNotifier(ProtoChannel::Notifier* theNotifier)
{
    if (notifier != theNotifier)
    {
        if (IsOpen())
        {
            // 1) Detach old notifier, if any
            if (notifier)
            {
                notifier->UpdateChannelNotification(*this, 0);
                if (!theNotifier)
                {
                    // Reset channel to "blocking"
                    if(!SetBlocking(true))
                        DMSG(0, "ProtoChannel::SetNotifier() SetBlocking(true) error\n", GetErrorString());
                }
            }
            else
            {
                // Set channel to "non-blocking"
                if(!SetBlocking(false))
                {
                    DMSG(0, "ProtoChannel::SetNotifier() SetBlocking(false) error\n", GetErrorString());
                    return false;
                }
            }   
            // 2) Set and update new notifier (if applicable)
            notifier = theNotifier;
            if (!UpdateNotification())
            {
                notifier = NULL;  
                return false;
            } 
        }
        else
        {
            notifier = theNotifier;
        }
    }
    return true;
}  // end ProtoChannel::SetNotifier()


bool ProtoChannel::UpdateNotification()
{
    if (notifier)
    {
        if (IsOpen() && !SetBlocking(false))
        {
            DMSG(0, "ProtoChannel::UpdateNotification() SetBlocking() error\n");
            return false;  
        }
        return notifier->UpdateChannelNotification(*this, notify_flags);
    }
    return true;
}  // end ProtoChannel::UpdateNotification()

bool ProtoChannel::SetBlocking(bool blocking)
{
#ifdef UNIX
    if (IsOpen())
    {
        if (blocking)
        {
            if(-1 == fcntl(descriptor, F_SETFL, fcntl(descriptor, F_GETFL, 0) & ~O_NONBLOCK))
            {
                DMSG(0, "ProtoChannel::SetBlocking() fcntl(F_SETFL(~O_NONBLOCK)) error: %s\n", GetErrorString());
                return false;
            }
        }
        else
        {
            if(-1 == fcntl(descriptor, F_SETFL, fcntl(descriptor, F_GETFL, 0) | O_NONBLOCK))
            {
                DMSG(0, "ProtoChannel::SetBlocking() fcntl(F_SETFL(O_NONBLOCK)) error: %s\n", GetErrorString());
                return false;
            }
        }
    }
#endif // UNIX
    return true;  //Note: taken care automatically under Win32 by WSAAsyncSelect(), etc
}  // end ProtoChannel::SetBlocking(bool blocking)

