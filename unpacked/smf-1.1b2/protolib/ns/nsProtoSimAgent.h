#ifndef _NS_PROTO_SIM_AGENT
#define _NS_PROTO_SIM_AGENT

// The "NsProtoSimAgent" plays a role similar to that of the 
// "ProtoDispatcher" class which provides liason between Protolib 
// classes and the computer operating system.

// ns-2 agents based on Protolib code can use this as a base
// class.  ProtoSockets used by the Protolib code _must_ have
// their "notifier" set as a pointer to their corresponding
// NsProtoSimAgent instance.  Similarly, ProtoTimerMgrs used by the 
// Protolib code _must_ have their "installer" set to their 
// corresponding "NsProtoSimAgent" instance.

#include "protoSimAgent.h"
#include "protoSocket.h"
#include "protoTimer.h"
#include "agent.h"  // for class Agent
#include "timer-handler.h"  // for class TimerHandler
#include "udp.h"  // for class UdpAgent

class NsProtoSimAgent : public Agent, public TimerHandler, public ProtoSimAgent
{
    public:
        virtual ~NsProtoSimAgent();
              
        virtual bool OnStartup(int argc, const char*const* argv) = 0;
        virtual bool ProcessCommands(int argc, const char*const* argv) = 0;
        virtual void OnShutdown() = 0;    
        
        // Some helper methods
        void ActivateTimer(ProtoTimer& theTimer)
            {ProtoSimAgent::ActivateTimer(theTimer);}
        
        void DeactivateTimer(ProtoTimer& theTimer)
            {ProtoSimAgent::DeactivateTimer(theTimer);}
        
        ProtoSocket::Notifier& GetSocketNotifier() 
            {return ProtoSimAgent::GetSocketNotifier();}
        
        ProtoTimerMgr& GetTimerMgr()
            {return ProtoSimAgent::GetTimerMgr();}
        
        // Override ProtoTimerMgr::GetSystemTime()
        virtual void GetSystemTime(struct timeval& currentTime)
        {
            ASSERT(scheduler);
            double now = scheduler->clock();
            currentTime.tv_sec = (unsigned long)(now);
            currentTime.tv_usec = (unsigned long)((now - currentTime.tv_sec) * 1.0e06);
        }
        
        bool GetLocalAddress(ProtoAddress& localAddr)
        {
            // if attached to node
            localAddr.SimSetAddress(Agent::addr());
            localAddr.SetPort(Agent::port());
            return true;   
            // (TB) if not attached to node, return false;
        }
        
        bool InvokeSystemCommand(const char* cmd);
  
		// I.T. - I made this public so I could subclass it from outside of an NSProtoSimAgent's scope 
		//
		// The "NSSocketProxy" provides liason between a "ProtoSocket"
        // instance and a corresponding ns-2 transport "ProtoSocketAgent" 
        // instance.
        class NSSocketProxy : public ProtoSimAgent::SocketProxy
        {
            friend class NsProtoSimAgent;
            
            public:
                virtual ~NSSocketProxy();
            
                bool Bind(UINT16& thePort);
                bool RecvFrom(char* buffer, unsigned int& numBytes, ProtoAddress& srcAddr);
                                
                bool JoinGroup(const ProtoAddress& groupAddr);
                bool LeaveGroup(const ProtoAddress& groupAddr);
                void SetTTL(unsigned char ttl) {mcast_ttl = ttl;}
                void SetLoopback(bool loopback) {mcast_loopback = loopback;}
                
                virtual bool SendTo(const char*         buffer, 
                                    unsigned int&       numBytes, 
                                    const ProtoAddress& dstAddr) = 0;
                
                // (TBD) These need to be implemented properly for TCP support
                virtual bool Connect(const ProtoAddress& theAddress) {return false;}
			    virtual bool Accept(ProtoSocket* theSocket) {return false;}
			    virtual bool Listen(UINT16 thePort) {return false;}
                
            protected:
                NSSocketProxy();
                
                unsigned char   mcast_ttl;
                bool            mcast_loopback;
                char*           recv_data;
                UINT16          recv_data_len;
                ProtoAddress    src_addr;
        };  // end class NsProtoSimAgent::NSSocketProxy
       
    protected:
        NsProtoSimAgent();
    
        // override of ns-2 Agent::command() method
        virtual int command(int argc, const char*const* argv);
        
        // override of ns-2 TimerHandler::expire() method
        void expire(Event*)
        {
            force_cancel();
            ProtoSimAgent::OnSystemTimeout();
        } 
        
        // override of ProtoSimAgent::UpdateSystemTimer() method
        bool UpdateSystemTimer(ProtoTimer::Command command,
                               double              delay)
        {
            switch (command)
            {
                case ProtoTimer::INSTALL:
                case ProtoTimer::MODIFY:
		  //TRACE("rescheduling system timer delay:%lf\n", delay);
                    resched(delay);
                    break;
                case ProtoTimer::REMOVE:
                    TRACE("canceling system timer\n");
                    force_cancel();
                    break;   
            }
            return true;
        }
        
 // here was NSSocketProxy
        
        ProtoSimAgent::SocketProxy* OpenSocket(ProtoSocket& theSocket);
        void CloseSocket(ProtoSocket& theSocket);
        
        // Extends ns-2 UdpAgent to provide interface to NSSocketProxy
        friend class ProtoSocketUdpAgentClass;
        class UdpSocketAgent : public Agent, public NSSocketProxy
        {
            friend class ProtoSocketUdpAgentClass;
            public:
                bool SendTo(const char*         buffer, 
                            unsigned int&       numBytes, 
                            const ProtoAddress& dstAddr);
                void recv(Packet* pkt, Handler* h);
            private:
                UdpSocketAgent();
                
        };  // end class NsProtoSimAgent::UdpSocketAgent
        
        // (TBD) Extend an appropriate ns-2 TCP agent
        
        ProtoSimAgent::SocketProxy::List  socket_proxy_list; 
        Scheduler*                        scheduler;
        Tcl*                              tcl;
        
};  // end class NsProtoSimAgent


// See common/protoExample.cpp for an example application/agent
// using Protolib code.

#endif  //_NS_PROTO_SIM_AGENT
