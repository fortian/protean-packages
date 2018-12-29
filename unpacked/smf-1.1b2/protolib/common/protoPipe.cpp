#include "protoPipe.h"
#include "protoDebug.h"

#ifdef WIN32
#include <winreg.h>
#else
#include <unistd.h>  // for unlink(), close()
#include <sys/un.h>  // for unix domain sockets
#include <sys/stat.h>
#include <stdlib.h>  // for mkstemp()
#ifdef NO_SCM_RIGHTS
#undef SCM_RIGHTS
#endif  // NO_SCM_RIGHTS
#define CLI_PERM    S_IRWXU
#endif // if/else WIN32

ProtoPipe::ProtoPipe(Type theType)
 : ProtoSocket((MESSAGE == theType) ? UDP : TCP)
#ifdef WIN32
#ifndef _WIN32_WCE
   , pipe_handle(INVALID_HANDLE_VALUE), is_mailslot(false),
   read_count(0), read_index(0)
#else
   , named_event_handle(INVALID_HANDLE_VALUE)
#endif // if/else !WIN32_WCE
#endif // WIN32
{
    domain = LOCAL;
    path[0] = '\0';
}

ProtoPipe::~ProtoPipe()
{
    Close();
}

#ifdef WIN32

void ProtoPipe::Close()
{
    ProtoSocket::Close();
#ifndef _WIN32_WCE
    if (INVALID_HANDLE_VALUE != pipe_handle)
    {
        CloseHandle(pipe_handle);
        pipe_handle = INVALID_HANDLE_VALUE;
    }
    if (INVALID_HANDLE_VALUE != input_event_handle)
    {
        CloseHandle(input_event_handle);
        input_event_handle = INVALID_HANDLE_VALUE;
    }   
    if (INVALID_HANDLE_VALUE != output_event_handle)
    {
        CloseHandle(output_event_handle);
        output_event_handle = INVALID_HANDLE_VALUE;
    }
    output_ready = input_ready = false;
	is_mailslot = false;
#else
    // Remove registry entry if we're a listener
    if ('\0' != path[0])
    {
#ifdef _UNICODE
        wchar_t wideBuffer[PATH_MAX];
        mbstowcs(wideBuffer, path, strlen(path)+1);
        LPCTSTR namePtr = wideBuffer;
#else
        LPCSTR namePtr = path;
#endif // if/else _UNICODE
        HKEY hKey;
	    if(ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, 
                                         _T("Software\\Protokit"), 
                                         0, KEY_SET_VALUE, &hKey))
	    {
            if (ERROR_SUCCESS != RegDeleteValue(hKey, namePtr))
                DMSG(0, "ProtoPipe::Close() RegDeleteValue() error: %s\n", ::GetErrorString());
            RegCloseKey(hKey);
        }
        else
        {
            DMSG(0, "ProtoPipe::Close() RegOpenKeyEx() error: %s\n", ::GetErrorString());
        }   
    }
    if (INVALID_HANDLE_VALUE != named_event_handle)
    {
        CloseHandle(named_event_handle);
        named_event_handle = INVALID_HANDLE_VALUE;
    }
#endif  // if/else !_WIN32_WCE
}  // end ProtoPipe::Close();

bool ProtoPipe::Listen(const char* theName)
{
#ifndef _WIN32_WCE
    if (IsOpen()) Close();

    // Setup event handles and overlapped structs
    if (NULL == (input_event_handle = CreateEvent(NULL, TRUE, FALSE, NULL)))
    {
        input_event_handle = INVALID_HANDLE_VALUE;
        DMSG(0, "ProtoPipe::Listen() CreateEvent(input_event_handle) error: %s\n", ::GetErrorString());
        Close();
        return false;
    }
    if (NULL == (output_event_handle = CreateEvent(NULL, TRUE, FALSE, NULL)))
    {
        output_event_handle = INVALID_HANDLE_VALUE;
        DMSG(0, "ProtoPipe::Listen() CreateEvent(output_event_handle) error: %s\n", ::GetErrorString());
        Close();
        return false;
    }
    memset(&read_overlapped, 0, sizeof(read_overlapped));
    read_overlapped.hEvent = input_event_handle;
    memset(&write_overlapped, 0, sizeof(write_overlapped));
    write_overlapped.hEvent = output_event_handle;
      
    if (TCP == protocol)
    {
        // Create a named pipe for "STREAM" type ProtoPipe
        char pipeName[PATH_MAX];
        strcpy(pipeName, "\\\\.\\pipe\\");
        strncat(pipeName, theName, PATH_MAX-strlen(pipeName));
        pipe_handle = CreateNamedPipe(pipeName, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, 
                                      PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                                      PIPE_UNLIMITED_INSTANCES,
                                      0, 0, 0, 0);
        if (INVALID_HANDLE_VALUE == pipe_handle)
        {
            DMSG(0, "ProtoPipe::Listen() CreateNamedPipe() error: %s\n", ::GetErrorString());
            return false;
        }
        if (NULL != notifier)
        {
            if (0 == ConnectNamedPipe(pipe_handle, &read_overlapped))
            {
                switch (GetLastError())
                {
                    case ERROR_IO_PENDING:
                    case ERROR_PIPE_LISTENING:
                        break;
                    default:
                        DMSG(0, "ProtoPipe::Listen() ConnectNamedPipe() error: %s\n", ::GetErrorString());
                        Close();
                        return false;
                }
            }
            else
            {
                DMSG(0, "ProtoPipe::Listen() ConnectNamedPipe() connected?: %s\n", ::GetErrorString());
                Close();
                return false;
            }
        }    
        state = LISTENING;
    }
    else  // (UDP == protocol)
    {
        // Create a mail slot for "MESSAGE" type ProtoPipe
        char pipeName[PATH_MAX];
        strcpy(pipeName, "\\\\.\\mailslot\\"); 
        strncat(pipeName, theName, PATH_MAX-strlen(pipeName));
        pipe_handle = CreateMailslot(pipeName, 
                                     8192, // 8192-byte maximum message size to emulate UDP
                                     MAILSLOT_WAIT_FOREVER, // no time-out for operations 
                                     NULL); // no security attributes 
        if (INVALID_HANDLE_VALUE == pipe_handle) 
        { 
            DMSG(0, "ProtoSocket::Listen() CreateMailSlot() error: %s\n", ::GetErrorString());
            return false; 
        } 
		is_mailslot = true;
        // Start overlapped notifications
        if (NULL != notifier)
        {
            if (notify_output) output_ready = true;
            // Initiate overlapped I/O ...
            DWORD bytesRead;
            if (0 != ReadFile(pipe_handle, read_buffer, BUFFER_MAX, &bytesRead, &read_overlapped))
            {
                input_ready = true;
                read_count = bytesRead;
                read_index = 0;
            }
            else
            {
                switch(GetLastError())
                {
                    case ERROR_IO_PENDING:
                        read_count = read_index = 0;
                        input_ready = false;
                        break;
                    case ERROR_BROKEN_PIPE:
                        DMSG(0, "ProtoPipe::Listen() ReadFile() error: %s\n", ::GetErrorString());
                        Close();
                        return false;
                }
            }
        }
        state = IDLE;
    }

    port = 0; // to fake binding
    if (!UpdateNotification())
    {
        DMSG(0, "ProtoSocket::Listen() error updating notification\n");
        Close();
        return false;
    }
#else
    // This "pipe" implementation uses the registry to find "local" sockets
    // which are used to provide interprocess "pipe" connectivity
    if (NULL != named_event_handle) CloseHandle(named_event_handle);

    // 1) Try to open name event of using given name
    char pipeName[MAX_PATH];
    strcpy(pipeName, "Global\\protoPipe-");
    strncat(pipeName, theName, MAX_PATH - strlen(pipeName));
#ifdef _UNICODE
    wchar_t wideBuffer[MAX_PATH];
    mbstowcs(wideBuffer, pipeName, strlen(pipeName)+1);
    LPCTSTR namePtr = wideBuffer;
#else
    LPCTSTR namePtr = pipeName;
#endif // if/else _UNICODE

    named_event_handle = CreateEvent(NULL, TRUE, TRUE, namePtr);
    if (NULL == named_event_handle)
    {
        named_event_handle = INVALID_HANDLE_VALUE;
        DMSG(0, "ProtoPipe::Listen() CreateEvent() error: %s\n", ::GetErrorString());
        return false;
    }
    else if (ERROR_ALREADY_EXISTS == GetLastError())
    {
        DMSG(0, "ProtoPipe::Listen() error: pipe already exists\n");
        Close();
        return false;
    }

    // 2) Open/Listen
    if (ProtoSocket::Listen())
    {
        // Make a registry entry so "connecting" pipes can find the port number
        HKEY hKey;
	    DWORD dwAction;
	    if (ERROR_SUCCESS == RegCreateKeyEx(HKEY_LOCAL_MACHINE,
									        _T("Software\\Protokit"),
									        0L,
									        NULL,
									        REG_OPTION_NON_VOLATILE,
									        KEY_ALL_ACCESS,
									        NULL,
									        &hKey,
									        &dwAction))
	    {
#ifdef _UNICODE
            mbstowcs(wideBuffer, theName, strlen(theName)+1);
            namePtr = wideBuffer;
#else
            namePtr = theName;
#endif // if/else _UNICODE;
            DWORD thePort = (DWORD)ProtoSocket::GetPort();
            if (ERROR_SUCCESS != RegSetValueEx(hKey, namePtr, 0L, REG_DWORD, 
                                               (BYTE*)&thePort, sizeof(DWORD)))
			{
				DMSG(0, "ProtoPipe::Listen() RegSetValueEx() error: %s\n", ::GetErrorString());
                RegCloseKey(hKey);
                Close();
                return false;
			}
            RegCloseKey(hKey);
        }
        else
        {
            DMSG(0, "ProtoPipe::Listen() RegCreateKeyEx() error: %s\n", ::GetErrorString());
            Close();
            return false;
        }
    }
    else
    {
        DMSG(0, "ProtoPipe::Listen() error listening to socket\n");
        Close();
        return false;
    }
#endif // if/else !_WIN32_WCE
    strncpy(path, theName, PATH_MAX);
    return true;
}  // ProtoPipe::Listen()

bool ProtoPipe::Accept(ProtoPipe* thePipe)
{
#ifndef _WIN32_WCE
    if (UDP == protocol) 
    {
        DMSG(0, "ProtoPipe::Accept() invald operation on MESSAGE pipe\n");
        return false;
    }
    if (NULL != notifier)
    {
        DWORD numBytes;
        if (FALSE == GetOverlappedResult(pipe_handle, &read_overlapped, &numBytes, FALSE))
        {
            DMSG(0, "ProtoPipe::Accept() GetOverlappedResult() error (%d): %s\n", GetLastError(), ::GetErrorString());
            // (TBD) close/reopen named pipe ???
            return false;
        }
        if (notify_output) output_ready = true;
        // Initiate overlapped I/O ...
        DWORD bytesRead;
        if (0 != ReadFile(pipe_handle, read_buffer, BUFFER_MAX, &bytesRead, &read_overlapped))
        {
            input_ready = true;
            read_count = bytesRead;
            read_index = 0;
        }
        else
        {
            switch(GetLastError())
            {
                case ERROR_IO_PENDING:
                    read_count = read_index = 0;
                    input_ready = false;
                    break;
                case ERROR_BROKEN_PIPE:
                    DMSG(0, "ProtoPipe::Accept() ReadFile() error: %s\n", ::GetErrorString());
                    return false;
            }
        }
    }
    else
    {
        if (0 == ConnectNamedPipe(pipe_handle, NULL))
        {
            DMSG(0, "ProtoPipe::Accept() ConnectNamedPipe() error: %s\n", ::GetErrorString());
            return false;
        }
    }
    if (thePipe && (thePipe != this))
    {
        // 1) Clear current notification
        state = CLOSED;
        if (!UpdateNotification())
        {
            DMSG(0, "ProtoPipe::Accept() error updating notification\n");
            Close();
            return false;
        }
        
        // 2) Copy "this" to "thePipe";
        *thePipe = *this;
        // 3) Manually reset "this" to fully closed state and re-open
        pipe_handle = NULL;
        input_ready = false;
        output_ready = false;
        char pipeName[PATH_MAX];
        strncpy(pipeName, path, PATH_MAX);
        if (!Listen(pipeName))
        {
            DMSG(0, "ProtoPipe::Accept() error re-opening server pipe\n");
            thePipe->Close();
            return false;
        }
        // 4) Set up notification for thePipe
        thePipe->state = CONNECTED;
        if (!thePipe->UpdateNotification())
        {
            DMSG(0, "ProtoPipe::Accept() error setting up new pipe notification\n");
            thePipe->Close();
            return false;
        }
    }
    else
    {
        state = CONNECTED;    
        if (!UpdateNotification())
        {
            DMSG(0, "ProtoSocket::Accept() error updating notification\n");
            Close();
            return false;
        }
    }
    return true;
#else
    return ProtoSocket::Accept(static_cast<ProtoSocket*>(thePipe));
#endif // if/else !_WIN32_WCE
}  // end ProtoPipe::Accept()

#ifndef _WIN32_WCE
bool ProtoPipe::SetBlocking(bool blocking)
{
    if (TCP == protocol)
    {
        DWORD mode = blocking ? PIPE_WAIT : PIPE_NOWAIT; 
        if (0 == SetNamedPipeHandleState(pipe_handle, &mode, NULL, NULL))
        {
            DMSG(0, "ProtoPipe::SetBlocking() SetNamedPipeHandleState() error: %s\n", ::GetErrorString());
            return false;
        }
    }
    else if (is_mailslot)
    {
        DWORD timeout = MAILSLOT_WAIT_FOREVER; //blocking ? MAILSLOT_WAIT_FOREVER : 0;
		if (0 == SetMailslotInfo(pipe_handle, timeout))
        {
            DMSG(0, "ProtoPipe::SetBlocking() SetMailslotInfo() error: %s\n", ::GetErrorString());
            return false;
        }
    }
    return true;
}  // end ProtoPipe::SetBlocking()
#endif // !_WIN32_WCE

bool ProtoPipe::Connect(const char* theName)
{
#ifndef _WIN32_WCE
    char pipeName[PATH_MAX];
    if (TCP == protocol)
    {
        // Connect to named pipe for STREAM type ProtoPipes
        strcpy(pipeName, "\\\\.\\pipe\\");
        strncat(pipeName, theName, PATH_MAX - strlen(pipeName));
        if (INVALID_HANDLE_VALUE == (pipe_handle = CreateFile(pipeName, GENERIC_WRITE | GENERIC_READ, 0, 
                                                              NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED,
                                                              NULL)))
        {
            DMSG(1, "ProtoPipe::Connect() CreateFile() error: %s\n", ::GetErrorString());
            return false;
        }
        state = CONNECTED;
    }
    else
    {
        // Write to mailslot for MESSAGE type ProtoPipes
        strcpy(pipeName, "\\\\.\\mailslot\\"); 
        strncat(pipeName, theName, PATH_MAX - strlen(pipeName));
        if (INVALID_HANDLE_VALUE == (pipe_handle = CreateFile(pipeName, GENERIC_WRITE | GENERIC_READ,
                                                              FILE_SHARE_READ, NULL, OPEN_EXISTING, 
                                                              FILE_FLAG_OVERLAPPED, NULL)))
        {
            DMSG(1, "ProtoPipe::Connect() CreateFile() error: %s\n", ::GetErrorString());
            return false;
        } 
        state = IDLE;
    }
    port = 0;
    if (NULL == (input_event_handle = CreateEvent(NULL, TRUE, FALSE, NULL)))
    {
        input_event_handle = INVALID_HANDLE_VALUE;
        DMSG(0, "ProtoPipe::Connect() CreateEvent(input_event_handle) error: %s\n", ::GetErrorString());
        Close();
        return false;
    }
    if (NULL == (output_event_handle = CreateEvent(NULL, TRUE, FALSE, NULL)))
    {
        output_event_handle = INVALID_HANDLE_VALUE;
        DMSG(0, "ProtoPipe::Connect() CreateEvent(output_event_handle) error: %s\n", ::GetErrorString());
        Close();
        return false;
    }
    memset(&read_overlapped, 0, sizeof(read_overlapped));
    read_overlapped.hEvent = input_event_handle;
    memset(&write_overlapped, 0, sizeof(write_overlapped));
    write_overlapped.hEvent = output_event_handle;
    if (NULL != notifier)
    {
        if (notify_output) output_ready = true;
        // Initiate overlapped I/O ...
        DWORD bytesRead;
        if (0 != ReadFile(pipe_handle, read_buffer, BUFFER_MAX, &bytesRead, &read_overlapped))
        {
            input_ready = true;
            read_count = bytesRead;
            read_index = 0;
        }
        else
        {
            switch(GetLastError())
            {
                case ERROR_IO_PENDING:
                    read_count = read_index = 0;
                    input_ready = false;
                    break;
                case ERROR_BROKEN_PIPE:
                    DMSG(0, "ProtoPipe::Accept() ReadFile() error: %s\n", ::GetErrorString());
                    Close();
                    return false;
            }
        }
    }
    if (!UpdateNotification())
    {
        DMSG(0, "ProtoSocket::Connect() error updating notification\n");
        Close();
        return false;
    }
#else
    // 1) Open the named event given the pipe name
    // This "pipe" implementation uses the registry to find "local" sockets
    // which are used to provide interprocess "pipe" connectivity
    if (INVALID_HANDLE_VALUE != named_event_handle) CloseHandle(named_event_handle);

    // 1) Try to open name event of using given name
    char pipeName[MAX_PATH];
    strcpy(pipeName, "Global\\protoPipe-");
    strncat(pipeName, theName, MAX_PATH - strlen(pipeName));
#ifdef _UNICODE
    wchar_t wideBuffer[MAX_PATH];
    mbstowcs(wideBuffer, pipeName, strlen(pipeName)+1);
    LPCTSTR namePtr = wideBuffer;
#else
    LPCTSTR namePtr = pipeName;
#endif // if/else _UNICODE

    named_event_handle = OpenEvent(EVENT_ALL_ACCESS, FALSE, namePtr);
    if (NULL != named_event_handle)
    {
        CloseHandle(named_event_handle);
        named_event_handle = INVALID_HANDLE_VALUE;
#ifdef _UNICODE
        mbstowcs(wideBuffer, theName, strlen(theName)+1);
        namePtr = wideBuffer;
#else
        namePtr = theName;
#endif // if/else _UNICODE
        
        // Now find the port number in the registry
        DWORD thePort = 0;
        HKEY hKey;
	    if(ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, 
                                         _T("Software\\Protokit"), 
                                         0, KEY_READ, &hKey))
	    {
            DWORD dwType;
            DWORD dwLen = sizeof(DWORD);
            if (ERROR_SUCCESS == RegQueryValueEx(hKey, namePtr, NULL, &dwType, 
                                                 (BYTE*)&thePort, &dwLen))
		    {
                if (REG_DWORD != dwType)
                {
                    DMSG(0, "ProtoPipe::Connect() registry entry type mismatch!\n");
                    RegCloseKey(hKey);
                    Close();
                    return false;
                }
            }
            else
            {
                DMSG(0, "ProtoPipe::Connect() RegQueryValueEx() error: %s\n", ::GetErrorString());
                RegCloseKey(hKey);
                Close();
                return false;
            }
            RegCloseKey(hKey);
        }
        else
        {
            DMSG(0, "ProtoPipe::Connect() RegOpenKeyEx() error: %s\n", ::GetErrorString());
            Close();
            return false;
        }
        
        if ((thePort < 1) || (thePort > 0x0ffff))
        {
            DMSG(0, "ProtoPipe::Connect() error: bad port value!?\n");
            Close();
            return false;
        }

        // Now connect to loopback address for given port
        ProtoAddress pipeAddr;
        pipeAddr.ResolveFromString("127.0.0.1");
        pipeAddr.SetPort((UINT16)thePort);
        if (!ProtoSocket::Connect(pipeAddr))
        {
            DMSG(0, "ProtoPipe::Connect() error connecting socket!\n");
            Close();
            return false;
        }
    }
    else
    {
        DMSG(0, "ProtoPipe::Connect() error (pipe not found): %s\n", ::GetErrorString());
        return false;
    }
#endif // if/else !_WIN32_WCE
    return true;
}  // end ProtoPipe::Connect()

#ifndef _WIN32_WCE
bool ProtoPipe::Send(const char* buffer, unsigned int& numBytes)
{
    DWORD bytesWritten;
    LPOVERLAPPED overlappedPtr = NULL;
    const char* bufPtr = buffer;
    if (NULL != notifier)
    {
        if (notify_output && !output_ready)
        {
            if (FALSE != GetOverlappedResult(pipe_handle, &write_overlapped, &bytesWritten, FALSE))
            {
                output_ready = true;
                overlappedPtr = &write_overlapped;
                bufPtr = write_buffer;
                if (numBytes > BUFFER_MAX) numBytes = BUFFER_MAX;
                memcpy(write_buffer, buffer, numBytes);
            }
            else
            {
                DMSG(0, "ProtoPipe::Send() GetOverlappedResult() error: %d\n", ::GetErrorString());
                return false;
            }
            
        }
    }

    if (0 != WriteFile(pipe_handle, bufPtr, numBytes, &bytesWritten, overlappedPtr))
    {
        numBytes = bytesWritten;
        if (0 == numBytes) output_ready = false;
        return true;
    }
    else
    {
        numBytes = 0;
        switch (GetLastError())
        {
            case ERROR_IO_PENDING:
                return true;
            case ERROR_BROKEN_PIPE:
                OnNotify(NOTIFY_NONE);
                break;
            default:
                DMSG(0, "ProtoPipe::Send() WriteFile() error(%d): %s\n", GetLastError(), ::GetErrorString());
                break;
        }
        return false;
    }
}  // end ProtoPipe::Send()

bool ProtoPipe::Recv(char* buffer, unsigned int& numBytes)
{
    DWORD want = numBytes;
    unsigned int got = 0;
    LPOVERLAPPED overlapPtr = NULL;
    if (NULL != notifier)
    {
        if (!input_ready)
        {
            // We had a pending overlapped read operation
            DWORD bytesRead;
            if (FALSE != GetOverlappedResult(pipe_handle, &read_overlapped, &bytesRead, FALSE))
            {
                
                unsigned int len = (bytesRead < want) ? bytesRead : want;
                memcpy(buffer, read_buffer, len);
                got += len;
                read_index = (len < bytesRead) ? len : 0;
                input_ready = true;
            }
            else
            {
                numBytes = 0;
                switch (GetLastError())
                {
                    case ERROR_BROKEN_PIPE:
                        OnNotify(NOTIFY_NONE);
                        return false;
                    default:
                        DMSG(0, "ProtoPipe::Recv() GetOverlappedResult() error(%d): %s\n", GetLastError(), ::GetErrorString());
                        return false;
                }
            }
        }
        overlapPtr = &read_overlapped;
    }

    // Do we have any data remaining in our "read_buffer"?
    if ((read_count > 0) && (got < want))
    {   
        unsigned int len  = want - got;
        if (len > read_count) len = read_count;
        memcpy(buffer+got, read_buffer+read_index, len);
        read_count -= len;
        read_index += len;
        got += len;
    }

    // Read more as needed, triggering overlapped I/O
    if (got < want)
    {
        DWORD bytesRead;
        unsigned int len = want - got;
        if (len > BUFFER_MAX) len = BUFFER_MAX;
        if (0 != ReadFile(pipe_handle, read_buffer, len, &bytesRead, overlapPtr))
        {
            memcpy(buffer+got, read_buffer, bytesRead);
            got += bytesRead;
        }
        else
        {
            switch(GetLastError())
            {
                case ERROR_IO_PENDING:
                    read_count = read_index = 0;
                    input_ready = false;
                    break;
                case ERROR_BROKEN_PIPE:
                    if (0 == got)
                    {
                        OnNotify(NOTIFY_NONE);
                        return false;
                    }
                    break;
                default:
                    DMSG(0, "ProtoPipe::Recv() ReadFile(%d) error: %s\n", GetLastError(), ::GetErrorString());
                    if (0 == got) return false;
                    break;
                
            }
        }
    }
    numBytes = got;
    return true;
}  // end ProtoPipe::Recv()

#endif // !_WIN32_WCE

#else

/**
 * This method opens a Unix-domain socket to serve as the ProtoPipe
 */

bool ProtoPipe::Open(const char* theName)
{
    // (TBD) use a semaphore to avoid issue of stale
    // Unix domain socket paths causing Open() to fail ???
    if (IsOpen()) Close();
    char pipeName[PATH_MAX];
    strcpy(pipeName, "/tmp/");
    strncat(pipeName, theName, PATH_MAX-strlen(pipeName));
    struct sockaddr_un sockAddr;
    memset(&sockAddr, 0, sizeof(sockAddr));
    sockAddr.sun_family = AF_UNIX;
    strcpy(sockAddr.sun_path, pipeName);
#ifdef SCM_RIGHTS  /* 4.3BSD Reno and later */
    int len = sizeof(sockAddr.sun_len) + sizeof(sockAddr.sun_family) + 
	          strlen(sockAddr.sun_path) + 1;
#else
    int len = strlen(sockAddr.sun_path) + sizeof(sockAddr.sun_family);
#endif // if/else SCM_RIGHTS    
    int socketType = (UDP == protocol) ? SOCK_DGRAM : SOCK_STREAM;      
    if ((handle = socket(AF_UNIX, socketType, 0)) < 0)
    {
        DMSG(0, "ProtoPipe::Open() socket() error: %s\n", GetErrorString());
        Close();
        return false;   
    }
    if (bind(handle, (struct sockaddr*)&sockAddr,  len) < 0)
    {
        DMSG(2, "ProtoPipe::Open() bind(%s) error: %s\n", pipeName, GetErrorString());
        Close();
        return false; 
    }
    state = IDLE;
    port = 0;
    if (!UpdateNotification())
    {
        DMSG(0, "ProtoPipe::Open() error updating notification\n");
        Close();
        return false;    
    }    
    strncpy(path, theName, PATH_MAX);
    return true;
}  // end ProtoPipe::Open(const char* theName)


void ProtoPipe::Close()
{
    if ('\0' != path[0])
    {
        Unlink(path);
        path[0] = '\0';
    } 
    ProtoSocket::Close();
}  // end ProtoPipe::Close();

void ProtoPipe::Unlink(const char* theName)
{
    char pipeName[PATH_MAX];
    strcpy(pipeName, "/tmp/");
    strncat(pipeName, theName, PATH_MAX - strlen(pipeName));
    unlink(pipeName);
}  // end ProtoPipe::Unlink()

bool ProtoPipe::Listen(const char* theName)
{
    if (IsOpen()) Close();
    if (Open(theName))
    {
        if (TCP == protocol)
        {
            state = LISTENING;
            if (!UpdateNotification())
            {
                DMSG(0, "ProtoSocket::Listen() error updating notification\n");
                Close();
                return false;
            }
            if (listen(handle, 5) < 0)
            {
                DMSG(0, "ProtoSocket:Listen() listen() error: %s\n", GetErrorString());
                Close();
                return false;
            }
        }
        return true;    
    }
    else
    {
        if (Connect(theName))
        {
            Close();
            DMSG(4, "ProtoPipe::Listen() error: name already in use\n");
            return false;
        }
        else
        {
#ifndef WIN32
            Unlink(theName);
            if (Listen(theName)) return true;
#endif       
            DMSG(0, "ProtoPipe::Listen() error opening pipe\n");     
        }
        
        return false;
    }   
}  // end ProtoPipe::Listen

bool ProtoPipe::Accept(ProtoPipe* thePipe)
{
    return ProtoSocket::Accept(static_cast<ProtoSocket*>(thePipe));   
}  // end ProtoPipe::Accept()

bool ProtoPipe::Connect(const char* theName)
{
    // (TBD) Don't "bind()" connecting sockets???
    // Open a socket as needed
    if (!IsOpen())
    {
        char pipeName[PATH_MAX];
        strcpy(pipeName, "/tmp/protoSocketXXXXXX");
        int fd = mkstemp(pipeName); 
        if (fd < 0)
        {
            DMSG(0, "ProtoPipe::Connect() mkstemp() error: %s\n", GetErrorString());
            return false;
        }
        else
        {
            close(fd);
            unlink(pipeName);
        } 
        if (!Open(pipeName+5))
        {
            DMSG(0, "ProtoPipe::Connect() error opening local domain socket\n");
            return false;
        }
        
        // Try to make socket flush before closing
        if (TCP == protocol)
        {
            struct linger so_linger;
            so_linger.l_onoff = 1;
            so_linger.l_linger = 5000;
            if (setsockopt(handle, SOL_SOCKET, SO_LINGER, &so_linger, sizeof(so_linger)) < 0)
                DMSG(0, "ProtoPipe::Connect() setsockopt(SO_LINGER) error: %s\n", GetErrorString());
        }
        // I can't remember why we do this step ...
        if (chmod(pipeName, CLI_PERM) < 0)
        {
	        DMSG(0, "ProtoPipe::Connect(): chmod() error: %s\n", GetErrorString());
            Close();
	        return false;
        }
    }
    
    // Now try to connect to server 
    struct sockaddr_un serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sun_family = AF_UNIX;
    strcpy(serverAddr.sun_path, "/tmp/");
    strncat(serverAddr.sun_path, theName, PATH_MAX - strlen(serverAddr.sun_path));
#ifdef SCM_RIGHTS  // 4.3BSD Reno and later 
    int addrLen = sizeof(serverAddr.sun_len) + sizeof(serverAddr.sun_family) + 
	              strlen(serverAddr.sun_path) + 1;
#else
    int addrLen = strlen(serverAddr.sun_path) + sizeof(serverAddr.sun_family);
#endif // SCM_RIGHTS
    // Make sure socket is "blocking" before connect attempt for "local socket
    ProtoPipe::Notifier* savedNotifier = notifier;
    if (NULL != savedNotifier) SetNotifier((ProtoPipe::Notifier*)NULL);   
    if (connect(handle, (struct sockaddr*)&serverAddr, addrLen) < 0)
    {
	    DMSG(1, "ProtoPipe::Connect(): connect() error: %s\n", GetErrorString());
	    Close();
        // Restore socket notification if applicable
        if (NULL != savedNotifier) SetNotifier(savedNotifier);
	    return false;
    }
    // Restore socket notification if applicable
    if (NULL != savedNotifier) SetNotifier(savedNotifier);
    state = CONNECTED;
    if (!UpdateNotification())
    {   
        DMSG(0, "ProtoPipe::Connect() error updating notification\n");
        Close();
        return false;
    } 
    return true;
}  // end ProtoPipe::Connect()


#endif // if/else WIN32/UNIX
