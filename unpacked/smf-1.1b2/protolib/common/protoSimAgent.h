#ifndef _PROTO_SIM_AGENT
#define _PROTO_SIM_AGENT

#include "protoTimer.h"
#include "protoSocket.h"

/*! The "ProtoSimAgent" class provides a base class for developing
 support for Protolib code in various network simulation environments
 (e.g. ns-2, OPNET, etc).  Protolib-based code must init its
 ProtoTimerMgr instances such that
*/
class ProtoSimAgent : public ProtoSocket::Notifier, public ProtoTimerMgr
{
    public:
        virtual ~ProtoSimAgent();
              
        /*void ActivateTimer(ProtoTimer& theTimer)
            {timer_mgr.ActivateTimer(theTimer);}
        
        void DeactivateTimer(ProtoTimer& theTimer)
            {timer_mgr.DeactivateTimer(theTimer);}*/
                
        ProtoSocket::Notifier& GetSocketNotifier() 
        {
            return static_cast<ProtoSocket::Notifier&>(*this);
        }
        
        ProtoTimerMgr& GetTimerMgr() 
        {
            return static_cast<ProtoTimerMgr&>(*this);
            //return timer_mgr;
        }
        
        virtual bool GetLocalAddress(ProtoAddress& localAddr) = 0;
        virtual bool InvokeSystemCommand(const char* cmd) {
          fprintf(stderr,"ProtoSimAgent: invoking system command \"%s\"\n", cmd);
          return false;
        }
        
        // The "SocketProxy" provides liason between a "ProtoSocket"
        // instance and a corresponding simulation transport "Agent" 
        // instance.
        friend class ProtoSocket;
        class SocketProxy : public ProtoSocket::Proxy
        {
			// I.T. Added 27/2/07
            int socketProxyID_;
			
			friend class ProtoSimAgent;
            public:
                virtual ~SocketProxy();
                        
                virtual bool Bind(UINT16& thePort) = 0;
		virtual bool Connect(const ProtoAddress& theAddress) = 0; // JPH 7/11/2006 - TCP support
		
		virtual bool Accept(ProtoSocket* theSocket) = 0; // JPH 7/11/2006 - TCP support
		
		virtual bool Listen(UINT16 thePort) = 0; // JPH 7/11/2006 - TCP support
                virtual bool SendTo(const char*         buffer, 
                                    unsigned int&       numBytes, 
                                    const ProtoAddress& dstAddr) = 0;
                virtual bool RecvFrom(char*         buffer, 
                                      unsigned int& numBytes, 
                                      ProtoAddress& srcAddr) = 0;                
                
                virtual bool JoinGroup(const ProtoAddress& groupAddr) = 0;
                virtual bool LeaveGroup(const ProtoAddress& groupAddr) = 0;
                virtual void SetTTL(unsigned char ttl) = 0;
                virtual void SetLoopback(bool loopback) = 0;
                ProtoSocket* GetSocket() {return proto_socket;}
                UINT16 GetPort()
                    {return (proto_socket ? proto_socket->GetPort() : 0);}
   
				// I.T. Added generic labelling and access mechanism - 27/2/07
				 /**
				  * Sets the internal identifier for this socket. You can use the findBySocketProxyID
				  * method to find the socket you name with this id 
				  */
				 void SetSocketProxyID(int ID) { socketProxyID_=ID; }
				 
				 /**
				  * Gets the internal identifier for this socket.  
				  */
				 int GetSocketProxyID() { return socketProxyID_; }
				
            protected:
                SocketProxy();
                void AttachSocket(ProtoSocket& protoSocket)
                    {proto_socket = &protoSocket;}
                
                SocketProxy* GetPrev() {return prev;}
                void SetPrev(SocketProxy* broker) {prev = broker;}
                SocketProxy* GetNext() {return next;}
                void SetNext(SocketProxy* broker) {next = broker;}
                
                class List
                {
                    public:
                        List();
                        void Prepend(SocketProxy& broker);
                        void Remove(SocketProxy& broker);
                        SocketProxy* FindProxyByPort(UINT16 thePort);
						
						// Added by I.T. 27/feb 2007
						/**
						 * Finds the Socket proxy by searching the socket proxies IDs which
						 * can be set and retrieved using the GetSocketPrioxyID and 
						 * GetSocketPrioxyID respectively. 
						 */
						SocketProxy* FindProxyByIdentifier(int socketProxyID);
                        
					protected:  // JPH 8/14/2006 - allow derived class TcpSockList to inspect head
								// I.T. 27/2/07 - I think a generic search facility should be incuded here ...
								// and keep this private ... I added one above. This could be used by
								// Opnet too rather than having subclasses have to write their own
						SocketProxy*  head;
                };  // end class ProtoSimAgent::SocketProxy::List
                friend class List;
                     
                ProtoSocket*        proto_socket;
                
                SocketProxy*        prev;    
                SocketProxy*        next;    
        };  // end class ProtoSimAgent::SocketProxy
    
    protected:
        ProtoSimAgent();
    
        virtual bool UpdateSystemTimer(ProtoTimer::Command command,
                                       double              delay) = 0;
        
        //void OnSystemTimeout() {timer_mgr.OnSystemTimeout();}
        
        virtual SocketProxy* OpenSocket(ProtoSocket& theSocket) = 0;
        virtual void CloseSocket(ProtoSocket& theSocket) = 0;
        
    private:
        /*class TimerMgr;
        friend class TimerMgr;
        class TimerMgr : public ProtoTimerMgr
        {
            public:
                TimerMgr(ProtoSimAgent& theAgent);
                bool UpdateSystemTimer(ProtoTimer::Command command,
                                       double              delay)
                {
                    return agent.UpdateSystemTimer(command, delay);   
                }
                
            private:
                ProtoSimAgent& agent;
        };  // end class TimerMgr
        
        
        TimerMgr    timer_mgr;*/
};  // end class ProtoSimAgent

#endif // _PROTO_SIM_AGENT
