/*********************************************************************
 *
 * AUTHORIZATION TO USE AND DISTRIBUTE
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: 
 *
 * (1) source code distributions retain this paragraph in its entirety, 
 *  
 * (2) distributions including binary code include this paragraph in
 *     its entirety in the documentation or other materials provided 
 *     with the distribution, and 
 *
 * (3) all advertising materials mentioning features or use of this 
 *     software display the following acknowledgment:
 * 
 *      "This product includes software written and developed 
 *       by Brian Adamson and Joe Macker of the Naval Research 
 *       Laboratory (NRL)." 
 *         
 *  The name of NRL, the name(s) of NRL  employee(s), or any entity
 *  of the United States Government may not be used to endorse or
 *  promote  products derived from this software, nor does the 
 *  inclusion of the NRL written and developed software  directly or
 *  indirectly suggest NRL or United States  Government endorsement
 *  of this product.
 * 
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 ********************************************************************/

#ifndef _PROTO_DISPATCHER
#define _PROTO_DISPATCHER

#include "protoTimer.h"
#include "protoSocket.h"
#include "protoChannel.h"

#ifdef WIN32
#ifndef _WIN32_WCE
#include <process.h>  // for _beginthreadex/_endthreadex
#endif // !_WIN32_WCE
#else
#include <pthread.h>
#include <unistd.h>  // for read()pwd
#endif // if/else WIN32/UNIX

/*!
This class provides a core around which Unix and Win32 applications
using Protolib can be implemented.  It's "Run()" method provides a
"main loop" which uses the "select()" system call on Unix and the
similar "MsgWaitForMultipleObjectsEx()" system call on Win32.  It
is planned to eventually provide some built-in support for threading
in the future (e.g. the ProtoDispatcher::Run() method might execute
in a thread, dispatching events to a parent thread).
*/

// Asynchronous event dispatcher class
class ProtoDispatcher : public ProtoTimerMgr, 
                        public ProtoSocket::Notifier,
                        public ProtoChannel::Notifier
{
    public:    
    // Construction
        ProtoDispatcher();
        virtual ~ProtoDispatcher();
        void Destroy();
        
    // ProtoTimerMgr overrides
        void ActivateTimer(ProtoTimer& theTimer)
        {
            SuspendThread();
            ProtoTimerMgr::ActivateTimer(theTimer);
            ResumeThread();
        }
        void DeactivateTimer(ProtoTimer& theTimer)
        {
            SuspendThread();
            ProtoTimerMgr::DeactivateTimer(theTimer);
            ResumeThread();
        }
        
    // Methods to manage generic input/output streams (pipes, files, devices, etc)
#ifdef WIN32
        typedef HANDLE Descriptor;  // WIN32 uses "HANDLE" type for descriptors
#else
        typedef int Descriptor;     // UNIX uses "int" type for descriptors
#endif // if/selse WIN32
        static const Descriptor INVALID_DESCRIPTOR;
        enum Event {EVENT_INPUT, EVENT_OUTPUT};
        typedef void (Callback)(ProtoDispatcher::Descriptor descriptor, 
                                ProtoDispatcher::Event      theEvent, 
                                const void*                 userData);
        bool InstallGenericInput(ProtoDispatcher::Descriptor descriptor, 
                                 ProtoDispatcher::Callback*  callback, 
                                 const void*                 clientData)
        {
            SignalThread();
            bool result = InstallGenericStream(descriptor, callback, clientData, Stream::INPUT); 
            UnsignalThread();
            return result;  
        }
        void RemoveGenericInput(Descriptor descriptor)
        {
            SignalThread();
            GenericStream* stream = FindGenericStream(descriptor);
            if (stream)
            {
                stream->UnsetFlag(Stream::INPUT);
                if (!stream->HasFlags()) RemoveGenericStream(stream);
            }   
            UnsignalThread(); 
        }
        bool InstallGenericOutput(ProtoDispatcher::Descriptor descriptor, 
                                  ProtoDispatcher::Callback*  callback, 
                                  const void*                 clientData)
        {
            return InstallGenericStream(descriptor, callback, clientData, Stream::OUTPUT);   
        }
        void RemoveGenericOutput(Descriptor descriptor)
        {
            GenericStream* stream = FindGenericStream(descriptor);
            if (stream)
            {
                stream->UnsetFlag(Stream::OUTPUT);
                if (!stream->HasFlags()) RemoveGenericStream(stream);
            }   
        }

        // Methods to set up and explicitly control ProtoDispatcher operation
        // Are there any pending timeouts/descriptors?
        bool IsPending() 
        {
            return (IsThreaded() ||
                    (NULL != generic_stream_list) || 
                    (NULL != socket_stream_list) ||
                    (NULL != channel_stream_list) ||
                    (ProtoTimerMgr::GetTimeRemaining() >= 0.0));  
        }
        
        void SetPriorityBoost(bool state)
            {priority_boost = state;}
        // Block until next event (timer, socket, other, occurs)
        // (Note this will block forever if !IsPending())
        void Wait();
        // Dispatch round of events 
        void Dispatch();
        
        // OR this "main loop" can be used.
        int Run(bool oneShot = false);
        void Stop(int exitCode = 0);
        
        /*
         * Controls whether time of day is polled for ultra-precise timing
         * If "precise timing" is set to "true", the time of day is polled
         * to achieve precise timer timeouts.  However, this can consume
         * considerable CPU resources.  The default state is "false".
         */
        void SetPreciseTiming(bool state) {precise_timing = state;}
        
        // The ProtoDispatcher::Controller helper class allows for
        // thread-safe synchronization when using the "StartThread()"
        // mode. 
#ifdef WIN32
        typedef CRITICAL_SECTION    Mutex;
#else
        typedef pthread_mutex_t     Mutex;
#endif // if/else WIN32/UNIX  
        class Controller
        {
            public:
                virtual ~Controller();
                void OnDispatch();

            protected:
                Controller(ProtoDispatcher& theDispatcher);
                
            private:
                friend class ProtoDispatcher;
                bool DoDispatch();  // only called by ProtoDispatcher
                virtual bool SignalDispatchReady() = 0;
                void OnThreadStop()
                {
                    if (use_lock_a)
                        ProtoDispatcher::Unlock(lock_a);
                    else
                        ProtoDispatcher::Unlock(lock_b);   
                }
                
                ProtoDispatcher&       dispatcher;
                Mutex                  lock_a;
                Mutex                  lock_b;
                bool                   use_lock_a;
        };  // end class ProtoDispatcher::Controller()  
        friend class Controller;
        
        // OR the dispatcher can be run in a thread n(w/ optional Controller)
        bool StartThread(bool                         priorityBoost = false,
                         ProtoDispatcher::Controller* theController = NULL);
        bool IsThreaded() {return ((ThreadId)(NULL) != thread_id);}
        
        // (NOTE: If dispatcher instance is threaded, you MUST successfully call
        //        "SuspendThread()" before calling _any_ dispatcher methods or
        //        manipulating any objects (ProtoTimers, ProtoSockets, etc)
        //        which affect dispatcher operation!!!!
        //        (Call "ResumeThread()" to allow the dispatcher to continue)
        //        This can be used to create thread-safe protocols/applications
        //        which use a ProtoDispatcher for async timing and I/O)
        bool SuspendThread();
        void ResumeThread();

#ifdef WIN32
        // WIN32 apps can call this to create a msg window if desired
        bool Win32Init();
        void Win32Cleanup();
#endif // WIN32 
        
    private:
        bool SignalThread();
        void UnsignalThread();
        bool WasSignaled()
        {
            // Check for and reset "break" signal
            if (IsThreaded())
            {
#ifdef WIN32
                if ((WAIT_OBJECT_0 <= wait_status) && (wait_status < (WAIT_OBJECT_0 + stream_count)))
                {
                    GenericStream* theStream = 
                        static_cast<GenericStream*>(stream_ptrs_array[wait_status - WAIT_OBJECT_0]);
                    if (break_event_stream == theStream) 
                    {
                        ResetEvent(break_event);
                        return true;
                    }
                }
#else        
                if ((wait_status > 0) && (FD_ISSET(break_pipe_fd[0], &input_set)))
                {
                    // Reset by emptying pipe
                    char byte[32];
                    while (read(break_pipe_fd[0], byte, 32) > 0);
                    return true;
                }  
#endif // if/else WIN32/UNIX
            }
            return false;
        }


        // Associated ProtoTimerMgrs will use this as needed
        bool UpdateSystemTimer(ProtoTimer::Command /*command*/,
                               double              delay) 
        {
            // ProtoDispatcher::Dispatch() queries ProtoTimerMgr::GetTimeRemaining() instead
            SignalThread();
            UnsignalThread();
            return true;
        }
        // Associated ProtoSockets will use this as needed
        bool UpdateSocketNotification(ProtoSocket& theSocket,
                                      int          notifyFlags);
        
        // Associated ProtoChannels will use this as needed
        bool UpdateChannelNotification(ProtoChannel& theChannel,
                                       int           notifyFlags);
        // Thread/Mutex stuff
#ifdef WIN32
        typedef DWORD WaitStatus;
        typedef DWORD ThreadId;
        ThreadId GetCurrentThread() {return ::GetCurrentThreadId();} 
#ifdef _WIN32_WCE
        typedef DWORD ExitStatus;
        static void DoThreadExit(ExitStatus exitStatus) {ExitThread(exitStatus);}
#else
        typedef unsigned int ExitStatus;
        static void DoThreadExit(ExitStatus exitStatus) {_endthreadex(exitStatus);}
#endif // if/else _WIN32_WCE
        static void Init(Mutex& m) {InitializeCriticalSection(&m);}
        static void Destroy(Mutex& m) {DeleteCriticalSection(&m);}
        static void Lock(Mutex& m) {EnterCriticalSection(&m);}
        static void Unlock(Mutex& m) {LeaveCriticalSection(&m);}
#else
        typedef int WaitStatus;
        typedef pthread_t           ThreadId;
        static ThreadId GetCurrentThread() {return pthread_self();}
        typedef void* ExitStatus;
        static void DoThreadExit(ExitStatus exitStatus) {pthread_exit(exitStatus);}
        static void Init(Mutex& m) {pthread_mutex_init(&m, NULL);}
        static void Destroy(Mutex& m) {pthread_mutex_destroy(&m);}
        static void Lock(Mutex& m) {pthread_mutex_lock(&m);}
        static void Unlock(Mutex& m) {pthread_mutex_unlock(&m);}
#endif // if/else WIN32/UNIX
                
        bool IsMyself() {return (GetCurrentThread() == thread_id);}
        void DestroyThread();
        static void DoThreadBreak(ProtoDispatcher::Descriptor descriptor, 
                                  ProtoDispatcher::Event      theEvent, 
                                  const void*                 userData);
        bool InstallBreak();
        void RemoveBreak();
        
                   
        // These classes help us manage notification for ProtoSockets and generic I/O descriptors
        class Stream
        {
            public:
                enum Type {GENERIC, SOCKET, CHANNEL};
                enum Flag {NONE = 0x00, INPUT = 0x01, OUTPUT = 0x02, EXCEPTION = 0x04};
                
                Type GetType() const {return type;}
                bool IsInput() const {return FlagIsSet(INPUT);}
                bool IsOutput() const {return FlagIsSet(OUTPUT);}
                bool FlagIsSet(Flag theFlag) const {return (0 != (flags & theFlag));}
                void SetFlag(Flag theFlag) {flags |= theFlag;}
                void UnsetFlag(Flag theFlag) {flags &= ~theFlag;}
                void SetFlags(int theFlags) {flags = theFlags;}
                bool HasFlags() {return (0 != flags);}
                void ClearFlags() {flags = 0;}
                
                Stream* GetNext() const {return next;}
                void SetNext(Stream* stream) {next = stream;}   
                Stream* GetPrev() const {return prev;}
                void SetPrev(Stream* stream) {prev = stream;}
#ifdef WIN32                
                int GetIndex() const {return index;}
                void SetIndex(int theIndex) {index = theIndex;} 
                int GetOutdex() const {return outdex;}
                void SetOutdex(int theOutdex) {outdex = theOutdex;}
#endif // WIN32
            protected:
                Stream(Type theType);
            
            private:
                Type  type;
                int   flags;
#ifdef WIN32
                int   index;
                int   outdex;
#endif // WIN32
                Stream* prev;
                Stream* next;
        };  // end class Stream
        
        class SocketStream : public Stream
        {
            public:
                SocketStream(ProtoSocket& theSocket);
                ProtoSocket& GetSocket() {return *socket;}
                void SetSocket(ProtoSocket& theSocket) {socket = &theSocket;}
            private:
                ProtoSocket*    socket;
        };  // end class SocketStream
        SocketStream* GetSocketStream(ProtoSocket& theSocket);
        void ReleaseSocketStream(SocketStream* socketStream);
        
        class ChannelStream : public Stream
        {
            public:
                ChannelStream(ProtoChannel& theChannel);
                ProtoChannel& GetChannel() {return *channel;}
                void SetChannel(ProtoChannel& theChannel) {channel = &theChannel;}
            private:
                ProtoChannel*    channel;
        };  // end class ChannelStream
        ChannelStream* GetChannelStream(ProtoChannel& theChannel);
        void ReleaseChannelStream(ChannelStream* channelStream);
        
        class GenericStream : public Stream
        {
            public:
                GenericStream(Descriptor theDescriptor);
                Descriptor GetDescriptor() {return descriptor;}
                void SetDescriptor(Descriptor theDescriptor) {descriptor = theDescriptor;}
                void SetCallback(Callback* theCallback, const void* clientData = NULL)
                {
                    callback = theCallback;
                    client_data = clientData;  
                }
                void OnEvent(Event theEvent)
                    {if (callback) callback(descriptor, theEvent, client_data);} 
            private:
                Descriptor  descriptor;
                Callback*   callback;
                const void* client_data;
        };  // end class GenericStream
        
        bool InstallGenericStream(ProtoDispatcher::Descriptor   descriptor, 
                                  Callback*                     callback, 
                                  const void*                   userData,
                                  Stream::Flag                  flag);
        void RemoveGenericStream(GenericStream* stream)
            {ReleaseGenericStream(stream);} 
        GenericStream* GetGenericStream(Descriptor descriptor);
        GenericStream* FindGenericStream(Descriptor descriptor) const;
        void ReleaseGenericStream(GenericStream* stream);


    // Members
        SocketStream*            socket_stream_pool;        
        SocketStream*            socket_stream_list;
        ChannelStream*           channel_stream_pool;
        ChannelStream*           channel_stream_list;        
        GenericStream*           generic_stream_pool;       
        GenericStream*           generic_stream_list;       
        
        volatile bool            run;           
        WaitStatus               wait_status;            
        int                      exit_code;  
        double                   timer_delay;  // ( timer_delay < 0.0) means INFINITY
        bool                     precise_timing;
        ThreadId                 thread_id;
        bool                     priority_boost;
        volatile bool            thread_started;
        Mutex                    suspend_mutex;
        Mutex                    signal_mutex;
        ThreadId                 thread_master;
        unsigned int             suspend_count;
        unsigned int             signal_count;
        bool                     break_signal_pending;
        Controller*              controller;
        
#ifdef WIN32
        int Win32AddStream(Stream& theStream, HANDLE theHandle);
        void Win32RemoveStream(int index);
        bool Win32IncreaseStreamArraySize();
        static LRESULT CALLBACK MessageHandler(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
#ifdef _WIN32_WCE
        static DWORD WINAPI DoThreadStart(LPVOID lpParameter);
#else
        static unsigned int  __stdcall DoThreadStart(void* lpParameter);
#endif // if/else _WIN32_WCE
        enum {DEFAULT_ITEM_ARRAY_SIZE = 32};
        HANDLE                   stream_handles_default[DEFAULT_ITEM_ARRAY_SIZE];    
        Stream*                  stream_ptrs_default[DEFAULT_ITEM_ARRAY_SIZE];       
        DWORD                    stream_array_size;                                  
        HANDLE*                  stream_handles_array;                                
        Stream**                 stream_ptrs_array;                                  
        DWORD                    stream_count;                                       
        HWND                     msg_window;
        HANDLE                   break_event;
        GenericStream*           break_event_stream;
        bool                     socket_io_pending;
        HANDLE                   actual_thread_handle;
#else
        static void* DoThreadStart(void* arg);
        fd_set                   input_set;
        fd_set                   output_set;
        int                      break_pipe_fd[2];                                         
#endif // if/else WIN32/UNIX
};  // end class ProtoDispatcher

#endif // _PROTO_DISPATCHER
