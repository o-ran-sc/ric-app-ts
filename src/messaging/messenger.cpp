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
	Mnemonic:	messenger.cpp
	Abstract:	Message Router Messenger.

	Date:		10 March 2020
	Author:		E. Scott Daniels
*/

#include <string.h>
#include <unistd.h>

#include <rmr/rmr.h>
#include <rmr/RIC_message_types.h>


#include <iostream>
#include <string>
#include <map>
#include <memory>
#include <mutex>

#include "callback.hpp"
#include "default_cb.hpp"		// default callback prototypes
#include "message.hpp"
#include "messenger.hpp"


// --------------- private -----------------------------------------------------


// ---------------- C++ buggerd up way of maintining class constants ----------
const int Messenger::MAX_PAYLOAD = (1024*64);
const int Messenger::DEFAULT_CALLBACK = -1;

// --------------- builders -----------------------------------------------
/*
	If wait4table is true, then the construction of the object does not
	complete until the underlying transport has a new copy of the route
	table.

	If port is nil, then the default port is used (4560).
*/
Messenger::Messenger( char* port, bool wait4table ) {
	if( port == NULL ) {
		port = (char *) "4560";
	}

	gate = new std::mutex();
	listen_port = strdup( port );
	mrc = rmr_init( listen_port, Messenger::MAX_PAYLOAD, 0 );

	if( wait4table ) {
		this->Wait_for_cts( 0 );
	}

	Add_msg_cb( RIC_HEALTH_CHECK_REQ, Health_ck_cb, NULL );		// add our default call backs

	ok_2_run = true;
}

/*
	Move support. We DO allow the instance to be moved as only one copy 
	remains following the move.
	Given a source object instance (soi) we move the information to 
	the new object, and then DELETE what was moved so that when the
	user frees the soi, it doesn't destroy what we snarfed.
*/
Messenger::Messenger( Messenger&& soi ) {
	mrc = soi.mrc;
	listen_port = soi.listen_port;
	ok_2_run = soi.ok_2_run;
	gate = soi.gate;
		cb_hash = soi.cb_hash;				// this seems dodgy
	
	soi.gate = NULL;
	soi.listen_port = NULL;
	soi.mrc = NULL;
}

/*
	Move operator. Given a source object instance, movee it's contents
	to this insance.  We must first clean up this instance.
*/
Messenger& Messenger::operator=( Messenger&& soi ) {
	if( this != &soi ) {				// cannot move onto ourself
		if( mrc != NULL ) {
			rmr_close( mrc );
		}
		if( listen_port != NULL ) {
			free( listen_port );
		}

		mrc = soi.mrc;
		listen_port = soi.listen_port;
		ok_2_run = soi.ok_2_run;
		gate = soi.gate;
			cb_hash = soi.cb_hash;				// this seems dodgy
		
		soi.gate = NULL;
		soi.listen_port = NULL;
		soi.mrc = NULL;
	}

	return *this;
}

/*
	Destroyer.
*/
Messenger::~Messenger() {
	if( mrc != NULL ) {
		rmr_close( mrc );
	}

	if( listen_port != NULL ) {
		free( listen_port );
	}
}

/*
	Allow user to register a callback function invoked when a specific type of
	message is received.  The user may pass an optional data pointer which
	will be passed to the function when it is called.  The function signature
	must be:
		void fun( Messenger* mr, rmr_mbuf_t* mbuf,  void* data );

	The user can also invoke this function to set the "default" callback by
	passing Messenger::DEFAULT_CALLBACK as the mtype. If no other callback
	is defined for a message type, the default callback function is invoked.
	If a default is not provided, a non-matching message is silently dropped.
*/
void Messenger::Add_msg_cb( int mtype, user_callback fun_name, void* data ) {
	Callback*	cb;

	cb = new Callback( fun_name, data );
	cb_hash[mtype] = cb;

	callbacks = true;
}

/*
	Message allocation for user to send. User must destroy the message when
	finished, but may keep the message for as long as is necessary
	and reuse it over and over.
*/
//Message* Messenger::Alloc_msg( int payload_size ) {
std::unique_ptr<Message> Messenger::Alloc_msg( int payload_size ) {
	return std::unique_ptr<Message>( new Message( mrc, payload_size ) );
}

void Messenger::Listen( ) {
	int count = 0;
	rmr_mbuf_t*	mbuf = NULL;
	std::map<int,Callback*>::iterator mi;	// map iterator; silly indirect way to point at the value
	Callback*	dcb = NULL;					// default callback so we don't search
	Callback*	sel_cb;						// callback selected to invoke
	std::unique_ptr<Message>m;

	if( mrc == NULL ) {
		return;
	}

	mi = cb_hash.find( DEFAULT_CALLBACK );
	if( mi != cb_hash.end() ) {
		dcb = mi->second;					// oddly named second field is the address of the callback block
	}

	while( ok_2_run ) {
		mbuf = rmr_torcv_msg( mrc, mbuf, 2000 );		// come up for air every 2 sec to check ok2run
		if( mbuf != NULL ) {
			if( mbuf->state == RMR_OK ) {
				m = std::unique_ptr<Message>( new Message( mbuf, mrc ) );	// auto delteted when scope terminates

				sel_cb = dcb;											// start with default
				if( callbacks  && ((mi = cb_hash.find( mbuf->mtype )) != cb_hash.end()) ) {
					sel_cb = mi->second;								// override with user callback
				}
				if( sel_cb != NULL ) {
					sel_cb->Drive_cb( *m );							// drive the selected one
					mbuf = NULL;									// not safe to use after given to cb
				}
			} else {
				if( mbuf->state != RMR_ERR_TIMEOUT ) {
					fprintf( stderr, "<LISTENER> got  bad status: %d\n", mbuf->state );
				}
			}
		}
	}
}

/*
	Wait for the next message, up to a max timout, and return the message received.
*/
std::unique_ptr<Message>  Messenger::Receive( int timeout ) {
	rmr_mbuf_t*	mbuf = NULL;
	std::unique_ptr<Message> m = NULL;

	if( mrc != NULL ) {
		mbuf = rmr_torcv_msg( mrc, mbuf, timeout );		// future: do we want to reuse the mbuf here?
		if( mbuf != NULL ) {
			m = std::unique_ptr<Message>( new Message( mbuf, mrc ) );
		}
	}

	return m;
}

/*
	Called to gracefully stop all listeners.
*/
void Messenger::Stop( ) {
	ok_2_run = false;
}

/*
	RMR messages must be released by RMR as there might be transport
	buffers that have to be dealt with. Every callback is expected to
	call this function when finished with the message.
void Messenger::Release_mbuf( void* vmbuf ) {
	rmr_free_msg( (rmr_mbuf_t *)  vmbuf );
}
*/

/*
	Wait for clear to send.
	Until RMR loads a route table, all sends will fail with a
	"no endpoint" state.  This function allows the user application
	to block until RMR has a viable route table.  It does not guarentee
	that every message that the user app will try to send has an entry.

	The use of this function by the user application allows for the
	parallel initialisation of the application while waiting for the
	route table service to generate a table for the application. The
	initialisation function may be callsed with "no wait" and this
	function invoked when the application has completed initialisation
	and is ready to start sending messages.

	The max wait parameter is the maximum number of seconds to block.
	If RMR never reports ready false is returned.  A true return
	incidcates all is ready.  If max_wait is 0, then this will only
	return when RMR is ready to send.
*/
bool Messenger::Wait_for_cts( int max_wait ) {
	bool block_4ever;
	bool	state = false;

	block_4ever = max_wait == 0;
	while( block_4ever || max_wait > 0 ) {
		if( rmr_ready( mrc ) ) {
			state = true;
			break;
		}

		sleep( 1 );
		max_wait--;
	}

	return state;
}
