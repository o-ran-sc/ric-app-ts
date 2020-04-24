// vi: ts=4 sw=4 noet:
/*
==================================================================================
	Copyright (c) 2020 Nokia
	Copyright (c) 2020 AT&T Intellectual Property.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
==================================================================================
*/

/*
	Mnemonic:	default_cb.cpp
	Abstract:	This is a set of static functions that are used as
				the default Messenger callbacks.

	Date:		11 Mar 2020
	Author:		E. Scott Daniels
*/

#include <string.h>
#include <unistd.h>

#include <rmr/rmr.h>
#include <rmr/RIC_message_types.h>


#include <cstdlib>

#include "messenger.hpp"

/*
	This is the default health check function that we provide (user
	may override it).  It will respond to health check messages by
	sending an OK back to the source.

	The mr paramter is obviously ignored, but to add this as a callback
	the function sig must match.
*/
void Health_ck_cb( Message& mbuf, int mtype, int sid, int len, Msg_component payload, void* data ) {
	unsigned char response[128];

	snprintf( (char* ) response, sizeof( response ), "OK\n" );
	mbuf.Send_response( RIC_HEALTH_CHECK_RESP, sid, strlen( (char *) response )+1, response );
}
