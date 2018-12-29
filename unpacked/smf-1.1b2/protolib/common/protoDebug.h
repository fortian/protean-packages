// protoDebug.h - debugging routines
#ifndef _PROTO_DEBUG
#define _PROTO_DEBUG

#ifdef WIN32
#include <winsock2.h>
#else
#include <string.h>  // for strerror()
#include <errno.h>   // for errno
#endif // if/else WIN32/UNIX

#if defined(PROTO_DEBUG) || defined(PROTO_MSG)

void SetDebugLevel(unsigned int level);
unsigned int GetDebugLevel();
bool OpenDebugLog(const char *path);
void CloseDebugLog();
void DMSG(unsigned int level, const char *format, ...);
#ifdef WIN32
void OpenDebugWindow();
void PopupDebugWindow();
void CloseDebugWindow();
#endif // WIN32

#else
inline void SetDebugLevel(unsigned int level) {}
inline unsigned int GetDebugLevel() {return 0;}
inline bool OpenDebugLog(const char *path) {return true;}
inline void CloseDebugLog() {}
inline void DMSG(unsigned int level, const char *format, ...) {}
#ifdef WIN32
inline void OpenDebugWindow() {}
inline void PopupDebugWindow() {}
inline void CloseDebugWindow() {}
#endif // WIN32

#endif // if/else PROTO_DEBUG || PROTO_MSG


#ifdef PROTO_DEBUG

void ABORT(const char *format, ...);
#ifdef HAVE_ASSERT
#undef NDEBUG
#include <assert.h>
#ifndef ASSERT
#define ASSERT(X) assert(X)
#endif // ASSERT
#else
#define ASSERT(X) (X ? : ABORT("ASSERT(%s) failed at line %d in source file \"%s\"\n", \
            #X, __LINE__, __FILE__ ));
#endif // if/else HAVE_ASSERT

#ifdef TRACE
#undef TRACE
#endif // TRACE
void TRACE(const char *format, ...);

#else  
#ifndef ABORT
#define ABORT(X)
#endif // !ABORT
#ifndef ASSERT
#define ASSERT(X)
#endif // !ASSERT 
#ifndef TRACE
inline void TRACE(const char *format, ...) {}
#endif // !TRACE

#endif // if/else PROTO_DEBUG

inline const char* GetErrorString()
{
#ifdef WIN32
    static char errorString[256];
    errorString[255] = '\0';
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | 
                  FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL,
                  GetLastError(),
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                  (LPTSTR) errorString, 255, NULL);
    return errorString;
#else
    return strerror(errno);
#endif // if/else WIN32/UNIX
}

#endif // _PROTO_DEBUG
