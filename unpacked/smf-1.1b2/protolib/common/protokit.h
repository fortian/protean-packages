#ifndef _PROTOKIT
#define _PROTOKIT

#include "protoVersion.h"
#include "protoDefs.h"
#include "protoSocket.h"
#include "protoDebug.h"
#include "protoTimer.h"
#include "protoTree.h"
#include "protoRouteTable.h"
#include "protoRouteMgr.h"
#include "protoPipe.h"

#ifdef SIMULATE
#ifdef NS2
#include "nsProtoSimAgent.h"
#endif // NS2
#else
#include "protoDispatcher.h"
#include "protoApp.h"
#endif // if/else SIMULATE

#endif // _PROTOKIT
