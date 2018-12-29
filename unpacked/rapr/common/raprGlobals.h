#ifndef _RAPR_GLOBALS
#define _RAPR_GLOBALS

enum {SCRIPT_LINE_MAX = 512}; // maximum script line length

#define STREAM_STOP_TIME 300
#define NSTREAMS_DEFAULT 2 // nprocs (1) + 1 (extra stream is
                           // common to all processes (for sprng)
#endif // _RAPR_GLOBALS
