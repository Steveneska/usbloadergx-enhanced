#ifndef _LSTUB_H_
#define _LSTUB_H_

// To set the "return to" stub for a certain ID
//!reqID is the Requested ID to return to
//!returns WII_EINTERNAL if it cant get the list of installed titles with ES functions
//!retuns -69 if the ID is not installed
//!1 if successful
s32 Set_Stub(u64 reqID);

// load the default HBC stub into memory.  as long as nothing writes to the 0x80001800
// +0xDC7 memory block it will stay there.  by default it has 0x00010001/UNEO.
void loadStub();

// Returns 0 or 1 depending on wether the stub is available
u8 hbcStubAvailable();

// Returns a valid title to return to
u64 returnTo(bool onlyHBC = false);

#endif
