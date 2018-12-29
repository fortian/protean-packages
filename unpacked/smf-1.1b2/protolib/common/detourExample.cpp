
#include "protoApp.h"
#include "protoDetour.h"

#include <stdlib.h>  // for atoi()
#include <stdio.h>   // for stdout/stderr printouts
#include <string.h>

// This example uses a ProtoDetour class instance to intercept
// outbound packets

class DetourExample : public ProtoApp
{
    public:
        DetourExample();
        ~DetourExample();

        // Overrides from ProtoApp or NsProtoSimAgent base
        bool OnStartup(int argc, const char*const* argv);
        bool ProcessCommands(int argc, const char*const* argv);
        void OnShutdown();

    private:
        void DetourEventHandler(ProtoChannel&               theChannel,
                                ProtoChannel::Notification  theNotification);
            
        enum CmdType {CMD_INVALID, CMD_ARG, CMD_NOARG};
        static const char* const CMD_LIST[];
        static CmdType GetCmdType(const char* string);
        bool OnCommand(const char* cmd, const char* val);        
        void Usage();
        
        ProtoDetour*    detour;
        bool            ipv6_mode;
        bool            allow;  // toggled variable for test
        
}; // end class DetourExample

const char* const DetourExample::CMD_LIST[] =
{
    "-ipv6",     // IPv6 test (instead of IPv4)
    NULL
};

// This macro creates our ProtoApp derived application instance 
PROTO_INSTANTIATE_APP(DetourExample) 
        
DetourExample::DetourExample()
 : detour(NULL), ipv6_mode(false), allow(true)
   
{       
   
}

DetourExample::~DetourExample()
{
}

void DetourExample::Usage()
{
#ifdef WIN32
    fprintf(stderr, "detourExample [background]\n");
#else
    fprintf(stderr, "detourExample\n");
#endif // if/else WIN32/UNIX
}  // end DetourExample::Usage()


DetourExample::CmdType DetourExample::GetCmdType(const char* cmd)
{
    if (!cmd) return CMD_INVALID;
    unsigned int len = strlen(cmd);
    bool matched = false;
    CmdType type = CMD_INVALID;
    const char* const* nextCmd = CMD_LIST;
    while (*nextCmd)
    {
        if (!strncmp(cmd, *nextCmd+1, len))
        {
            if (matched)
            {
                // ambiguous command (command should match only once)
                return CMD_INVALID;
            }
            else
            {
                matched = true;   
                if ('+' == *nextCmd[0])
                    type = CMD_ARG;
                else
                    type = CMD_NOARG;
            }
        }
        nextCmd++;
    }
    return type; 
};  // end DetourExample::GetCmdType()


bool DetourExample::OnStartup(int argc, const char*const* argv)
{
    
    if (!ProcessCommands(argc, argv))
    {
        DMSG(0, "detourExample::OnStartup() error processing command line options\n");
        return false;   
    }
    
    // Create our cap_rcvr instance and initialize ...
    if (!(detour = ProtoDetour::Create()))
    {
        DMSG(0, "detourExample::OnStartup() new ProtoDetour error: %s\n", GetErrorString());
        return false;
    }  
     
    detour->SetNotifier(static_cast<ProtoChannel::Notifier*>(&dispatcher));
    detour->SetListener(this, &DetourExample::DetourEventHandler);
    
    // Set detour filter for outbound multicast packets
    ProtoAddress srcFilter;
    ProtoAddress dstFilter;
    unsigned int dstFilterMask;
    if (ipv6_mode)
    {
        srcFilter.Reset(ProtoAddress::IPv6);   // unspecified address 
        dstFilter.ResolveFromString("ff00::");
        dstFilterMask = 8;
    }
    else
    {
        srcFilter.Reset(ProtoAddress::IPv4);  // unspecified address
        dstFilter.ResolveFromString("239.0.0.0");
        dstFilterMask = 4;
    }
    
    if (!detour->Open(ProtoDetour::OUTPUT, srcFilter, 0, dstFilter, dstFilterMask))
    {
        DMSG(0, "detourExample::OnStartup() ProtoDetour::Open() error\n");
    }
    
#ifdef NEVER //HAVE_SCHED
    // Boost process priority for real-time operation
    // (This _may_ work on Linux-only at this point)
    struct sched_param schp;
    memset(&schp, 0, sizeof(schp));
    schp.sched_priority =  sched_get_priority_max(SCHED_FIFO);
    if (sched_setscheduler(0, SCHED_FIFO, &schp))
    {
        schp.sched_priority =  sched_get_priority_max(SCHED_OTHER);
        if (sched_setscheduler(0, SCHED_OTHER, &schp))
            DMSG(0, "detourExample: Warning! Couldn't set any real-time priority: %s\n", GetErrorString());
    }
#endif // HAVE_SCHED

#ifdef WIN32
#ifndef _WIN32_WCE
    if (!SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS))
	    DMSG(0, "detourExample: Warning! SetPriorityClass() error: %s\n", GetErrorString());
#endif // !_WIN32_WCE
	if (!SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL))
	    DMSG(0, "detourExample: Warning! SetThreadPriority() error: %s\n", GetErrorString());
#endif // WIN32

    return true;
}  // end DetourExample::OnStartup()

void DetourExample::OnShutdown()
{
   if (NULL != detour)
   {
       detour->Close();
       delete detour;
       detour = NULL;
   }
   DMSG(0, "detourExample: Done.\n"); 
   CloseDebugLog();
}  // end DetourExample::OnShutdown()

bool DetourExample::ProcessCommands(int argc, const char*const* argv)
{
    // Dispatch command-line commands to our OnCommand() method
    int i = 1;
    while ( i < argc)
    {
        // Is it a class DetourExample command?
        switch (GetCmdType(argv[i]))
        {
            case CMD_INVALID:
            {
                DMSG(0, "DetourExample::ProcessCommands() Invalid command:%s\n", 
                        argv[i]);
                return false;
            }
            case CMD_NOARG:
                if (!OnCommand(argv[i], NULL))
                {
                    DMSG(0, "DetourExample::ProcessCommands() ProcessCommand(%s) error\n", 
                            argv[i]);
                    return false;
                }
                i++;
                break;
            case CMD_ARG:
                if (!OnCommand(argv[i], argv[i+1]))
                {
                    DMSG(0, "DetourExample::ProcessCommands() ProcessCommand(%s, %s) error\n", 
                            argv[i], argv[i+1]);
                    return false;
                }
                i += 2;
                break;
        }
    }
    return true;  
}  // end DetourExample::ProcessCommands()

bool DetourExample::OnCommand(const char* cmd, const char* val)
{
    // (TBD) move command processing into Mgen class ???
    CmdType type = GetCmdType(cmd);
    ASSERT(CMD_INVALID != type);
    size_t len = strlen(cmd);
    if ((CMD_ARG == type) && !val)
    {
        DMSG(0, "DetourExample::ProcessCommand(%s) missing argument\n", cmd);
        return false;
    }
    else if (!strncmp("ipv6", cmd, len))
    {
        ipv6_mode = true;
    }
    else
    {
        DMSG(0, "detourExample:: invalid command\n");
        return false;
    }
    return true;
}  // end DetourExample::OnCommand()

void DetourExample::DetourEventHandler(ProtoChannel&               theChannel, 
                                       ProtoChannel::Notification  theNotification)
{
    if (ProtoChannel::NOTIFY_INPUT == theNotification)
    {
        char buffer[8192];
        unsigned int numBytes = 8192; 
        if (detour->Recv(buffer, numBytes))
        {
            TRACE("detour recv'd packet ...\n");
            if (0 != numBytes)
            {
                if (allow)
                {
                    TRACE("allowing packet len:%u\n", numBytes);
                    detour->Allow(buffer, numBytes);
                    //allow = false;
                }
                else
                {    
                    TRACE("detour dropping/injecting packet ...\n");
                    detour->Drop();//Allow(buffer, numBytes);
                    detour->Inject(buffer, numBytes);
                    allow = true;
                }
            }
            numBytes = 8192;
        }                  
    }
}  // end DetourExample::DetourEventHandler()
