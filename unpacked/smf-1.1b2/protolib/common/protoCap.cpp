#include "protoCap.h"

// (TBD) How bad would it really be to inline these in the "ProtoCap" class definition?
//       (The we could get rid of this file)

ProtoCap::ProtoCap()
 :   if_index(-1), user_data(NULL)
{
    // Enable input notification by default for ProtoCap
    StartInputNotification();
}


ProtoCap::~ProtoCap()
{
    if (IsOpen()) Close();
}
