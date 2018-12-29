// Rapr
#include "raprVersion.h"
#include "rapr.h"
#include "raprApp.h"
#include "raprPayload.h"
#include "behaviorEvent.h"
// Protolib
#include "protokit.h"
// Mgen
#include "mgenVersion.h"
#include "mgen.h"

#include <libgen.h>

#include <ctype.h>  // for toupper
#include <stdio.h>

RaprApp::RaprApp()
  : rapr(GetTimerMgr(),GetSocketNotifier(),
         rapr_control_pipe,rapr_control_remote,
         mgen),
    convert(false),
    rapr_control_pipe(ProtoPipe::MESSAGE), rapr_control_remote(false),
    mgen(GetTimerMgr(),GetSocketNotifier())
{
    // Control pipe for rapr run time interface
    rapr_control_pipe.SetNotifier(&GetSocketNotifier());
    rapr_control_pipe.SetListener(this,&RaprApp::OnControlEvent);
    
    mgen.SetController(this);
    
} // End RaprApp::RaprApp

RaprApp::~RaprApp()
{
}

const char* const RaprApp::CMD_LIST[] = 
  {
    "+instance",    // indicate rapr instance name
    "+error",       // mgen error
    "-stop",        // exit program instance
    "--version",    // print rapr version and exit
    "-help",        // print usage and exit
    NULL
  };

void RaprApp::Usage()
{
    fprintf(stderr,"rapr\n [input <scriptFile>]\n"
            "[overwrite_mgenlog <logFile>|mgenlog <logFile>]\n"
            "[overwrite_raprlog <logFile>|raprlog <logFile>]\n"
            "[event \"<mgen event>\"]\n"
            "[instance <name>]\n"
            "[offset <offsetTime>]\n"
            "[start <startTime>]\n"
            "[verbose]\n"
            "[enable_load_emulator]\n"
            "[hostid <hostid>]\n"
            "[initial_seed <value>]\n"
            "[load_dictionary <fileName>]\n"
            "[txlog] [nolog] [flush]\n"
            "[localtime]\n"
            "[interfaee <name>]\n"
            "[ttl <timeToLive>]\n"
            "[tos <typeOfService>]\n"
            "[txbuffer <bufferSize>]\n"
            "[rxbuffer <bufferSize>]\n"
            "[txcheck] [rxcheck] [check]\n"
            "[stop]\n");
} // end RaprApp::Usage

RaprApp::CmdType RaprApp::GetCmdType(const char* cmd)
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
}  // end RaprApp::GetCmdType()

void RaprApp::OnOffEvent(char* buffer, int len)
{
    rapr.OnCommand(Rapr::OFFEVENT,buffer,BehaviorEvent::MGEN_EVENT,false);
    
} // end RaprApp::OnOffEvent

void RaprApp::OnMsgReceive(MgenMsg& msg)
{
    RaprPayload raprPayload;
    const char * payld = msg.GetPayload();
    raprPayload.SetHex(payld);
    delete [] payld;
    
    DMSG(1,"RAPRAPP MSG RECEIVED: src> %s/%hu dst> %s/%hu,logicID>%d\n",msg.GetSrcAddr().GetHostString(),msg.GetSrcAddr().GetPort(),msg.GetDstAddr().GetHostString(),msg.GetDstAddr().GetPort(),raprPayload.GetLogicID());
    
    // We ignore stream payload (if any) once
    // we're participating in the coversation.
    
    if (raprPayload.GetStreamID())
    {    
        BehaviorEvent* tmpBehaviorEvent;
        if ((tmpBehaviorEvent = rapr.GetBehaviorEventList().FindBEByStreamUBI(raprPayload.GetStreamID())))
        {
            DMSG(2,"I found a matching streamUbi> %lu\n",raprPayload.GetStreamID());
            tmpBehaviorEvent->ProcessEvent(msg);
            // logic id?   ljt what is this return doing ehre?
            return;  
        }      
        else 
        {
            DMSG(2,"RaprApp::OnMsgReceive() Error: No matching stream object found for ubi>%lu\n",raprPayload.GetStreamID());
        }
    }
    else
    {
        if (raprPayload.GetForeignUBI())
        {
            // If it's ours, process the event
            if (rapr.GetNumberGenerator().GetHostIDFromUBI(raprPayload.GetForeignUBI()) == rapr.GetNumberGenerator().GetHostID()) 
            {
                BehaviorEvent* tmpBehaviorEvent;
                if ((tmpBehaviorEvent = rapr.GetBehaviorEventList().FindBEByUBI(raprPayload.GetForeignUBI())))
                {
                    tmpBehaviorEvent->ProcessEvent(msg);
                }
                
                else 
                {
                    // If multiple objects respond to the interrogative
                    // object, the first response will trigger a succesful
                    // response.  The fact that we no longer have an
                    // object is not necessarily an error.
                    DMSG(1,"RaprApp::OnMsgReceive() Error: behavior event for ubi>%lu doesn't exist!\n",raprPayload.GetForeignUBI());
                }
            }
        }
    }
    
    // Process the payload logic id regardless
    if (raprPayload.GetLogicID())
    {
        // ljt 020807 don't need to pass src addr any more - in the msg
        rapr.LogicTable().DoLogicID(raprPayload.GetLogicID(),msg,msg.GetSrcAddr(),NULL);
    }
    
} // end RaprApp::OnMsgReceive

bool RaprApp::ProcessCommands(int argc, const char*const* argv)
{  
    int i = 1;
    convert = false;
    //  strncpy(filename,basename(argv[0]),512);
    char tmpString[512];
    strncpy(tmpString,argv[0],512);
    strncpy(filename,basename(tmpString),512);
    while (i < argc)
    {
        CmdType cmdType = GetCmdType(argv[i]);
        if (CMD_INVALID == cmdType)
        {
            // Is it a class Rapr command?
            switch(Rapr::GetCmdType(argv[i]))
            {
            case Rapr::CMD_INVALID:
              break;
            case Rapr::CMD_NOARG:
              cmdType = CMD_NOARG;
              break;
            case Rapr::CMD_ARG:
              cmdType = CMD_ARG;
              break;
            }
        }
        switch (cmdType)
        {
        case CMD_INVALID:
          DMSG(0,"RaprApp::ProcessCommands() Error: invalid command: %s\n",
               argv[i]);
          return false;
        case CMD_NOARG:
          if (!OnCommand(argv[i],NULL,BehaviorEvent::SCRIPT_EVENT))
          {
              DMSG(0,"RaprApp::ProcessCommands() Error: OnCommand(%s) error\n",
                   argv[i]);
              return false;
          }
          i++;
          break;
        case CMD_ARG:
          if (!OnCommand(argv[i],argv[i+1],BehaviorEvent::SCRIPT_EVENT))
          {
              DMSG(0,"RaprApp::ProcessCommands() Error: OnCommand(%s, %s) error\n",
                   argv[i],argv[i+1]);
              return false;	      
          }
          i += 2;
          break;
        }
        
    }
    return true;
} // end RaprApp::ProcessCommands()

bool RaprApp::OnCommand(const char* cmd, const char* val,BehaviorEvent::EventSource eventSource)
{
    
    if (rapr_control_remote)
    {
        char buffer[8192];
        strcpy(buffer, cmd);
        if (val)
        {
            strcat(buffer, " ");
            strncat(buffer, val, 8192 - strlen(buffer));
        }
        unsigned int len = strlen(buffer);
        if (rapr_control_pipe.Send(buffer, len))
        {
            return true;
        }
        else
        {
            DMSG(0, "RaprApp::ProcessCommand(%s) Error: error sending command to remote process\n", cmd);    
            return false;
        }        
    }
    
    
    CmdType type = GetCmdType(cmd);
    unsigned int len = strlen(cmd);
    if (CMD_INVALID == type)
    {
        if (Rapr::CMD_INVALID != Rapr::GetCmdType(cmd))
        {
            return rapr.OnCommand(Rapr::GetCommandFromString(cmd),val,eventSource,true);
        }
        else {
            
            DMSG(0,"RaprApp::OnCommand(%s) Error: invalid command \n",cmd);
            return false;
        }
    }
    else if ((CMD_ARG == type) && !val)
    {
        DMSG(0,"RaprApp::OnCommand(%s) Error: missing argument\n",cmd);
        return false;
    }
    else if (!strncmp("instance",cmd,len))
    {
        
        // In case they are already open...
        
        if (rapr_control_pipe.IsOpen())
          rapr_control_pipe.Close();
        
        if (rapr_control_pipe.Connect(val))
        {
            rapr_control_remote = true; // ljt take this out probably...
        }
        else if (!rapr_control_pipe.Listen(val))
        {
            DMSG(0,"RaprApp::OnCommand(instance) Error: error opening rapr pipe\n");
            return false;
        }
    }    
    
    else if (!strncmp("stop",cmd,len))
    {
        Stop();
    }
    else if (!strncmp("error",cmd,len))
    {
        char msgBuffer[512];
        sprintf(msgBuffer,"TYPE> ERROR received from Mgen for command (%s)",val);
        rapr.LogEvent("MGEN",msgBuffer);
        
        DMSG(0,"RaprApp::Oncommand() Error: received from mgen for command: (%s)\n",val);
        
        return true;
    }
    else if (!strncmp("-version",cmd,len) || !strncmp("-v",cmd,len))
    {
        fprintf(stderr,"%s:version %s\n",filename,RAPR_VERSION);
        exit(0);
        //      return false;
    }
    
    else if (!strncmp("help",cmd,len))
    {
        fprintf(stderr,"%s:version %s\n",filename,RAPR_VERSION);
        Usage();
        return false;
    }
    return true;
} // end RaprApp::OnCommand()


bool RaprApp::OnStartup(int argc, const char*const* argv)
{
    
    // Seed the system rand() function
    struct timeval currentTime;
    ProtoSystemTime(currentTime);
    srand(currentTime.tv_usec);
    
    rapr.SetLogFile(stdout);  // log to stdout by default
    mgen.SetLogFile(stdout);
    
#ifdef HAVE_IPV6    
    if (ProtoSocket::HostIsIPv6Capable()) 
      mgen.SetDefaultSocketType(ProtoAddress::IPv6);
#endif // HAVE_IPV6
    
#ifdef HAVE_GPS
    // ljt 11/13/06 - was not in rapr svn repo
    
    //  gps_handle = GPSSubscribe(NULL);
    //if (gps_handle)
    // mgen.SetPositionCallback(RaprApp::GetPosition,gps_handle);
    //payload_handle = GPSSubscribe("/tmp/mgenPayloadKey");
    //mgen.SetPayloadHandle(payload_handle);
#endif // HAVE_GPS
    
    if (!rapr.LoadState()) return false;
    
    if (!ProcessCommands(argc, argv))
    {
        fprintf(stderr,"RaprApp::OnStartup error while processing startup commands\n");
        return false;
    }
    
    if (rapr_control_remote)
    {
        // We remoted commands, so we're finished...
        return false;
    }
    
    fprintf(stderr,"%s:version %s\n",filename,RAPR_VERSION);
    fprintf(stderr, "mgen:version %s\n", MGEN_VERSION);
    
    if (rapr.DelayedStart())
    {
        char startTime[256];
        rapr.GetStartTime(startTime);
        fprintf(stderr, "rapr: delaying start until %s ...\n", startTime);
    }
    else
    {
        fprintf(stderr, "rapr: starting now ...\n");
    }

    return rapr.Start();
    
} // end RaprApp::OnStartup()

void RaprApp::OnShutdown()
{
    rapr.Stop();
    mgen.Stop();
    
    rapr_control_pipe.Close();
#ifdef HAVE_GPS
    // ljt 11/13/06 - was not in rapr svn repo
    
    //    if (gps_handle) 
    // {
    //    GPSUnsubscribe(gps_handle);
    //    gps_handle = NULL;
    //    mgen.SetPositionCallback(NULL, NULL);
    //}
    //if (payload_handle)
    //{
    //    GPSUnsubscribe(payload_handle);
    //    payload_handle = NULL;
    //    mgen.SetPayloadHandle(NULL);
    //}
#endif // HAVE_GPS    
    
}  // end RaprApp::OnShutdown()

#ifdef HAVE_GPS
// ljt is this the right code?  11/13/06 

//bool RaprApp::GetPosition(const void* gpsHandle, GPSPosition& gpsPosition)
//{
//    GPSGetCurrentPosition((GPSHandle)gpsHandle, &gpsPosition);
//    return true;    
//}
#endif // HAVE_GPS

void RaprApp::OnControlEvent(ProtoSocket& /* theSocket */,
                             ProtoSocket::Event theEvent)
{
    if (ProtoSocket::RECV == theEvent)
    {
        char buffer[8192];
        unsigned int len = 8191;
        if (rapr_control_pipe.Recv(buffer,len))
        {
            buffer[len] = '\0';
            char* cmd = buffer;
            char* arg = NULL;
            for (unsigned int i = 0; i < len; i++)
            {
                if ('\0' == buffer[i])
                {
                    break;
                }
                else if (isspace(buffer[i]))
                {
                    buffer[i] = '\0';
                    arg = buffer+i+1;
                    break;
                }
            }
            if (!OnCommand(cmd,arg,BehaviorEvent::RTI_EVENT)) {
                // ljt make sure msgBuffer is big enough
                char msgBuffer [512];
                DMSG(0,"RaprApp::OnControlEvent() Error: error processing command %s arg: %s\n",cmd,arg);
                
                sprintf(msgBuffer,"TYPE>ERROR processing command %s arg: %s",cmd,arg);
                rapr.LogEvent("RTI_EVENT",msgBuffer);
            } else {
                
                // Don't log mgenevents for now
                unsigned int len = strlen(cmd);
                if (strncmp("mgenevent",cmd,len)) 
                {
                    char msgBuffer [512];
                    sprintf(msgBuffer,"TYPE>Command %s arg: %s",cmd,arg);
                    rapr.LogEvent("RTI_EVENT",msgBuffer);
                }
            }
        }
        else
        {
            DMSG(0,"RaprApp::OnControlEvent() Error: receive error");
            rapr.LogEvent("RTI_EVENT","TYPE>ERROR control pipe receive error");
            
        }
    }
    else
    {
        rapr.LogEvent("RAPR","TYPE>ERROR RaprApp::OnControlEvent() unhandled event type\n");
    }
} // end RaprApp::OnControlEvent

MgenSinkTransport* MgenSinkTransport::Create(Mgen& theMgen)
{
    DMSG(0,"Rapr does not support sink transport.\n");
    return NULL;
}


PROTO_INSTANTIATE_APP(RaprApp)
  
