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
	Mnemonic:	messenger.hpp
	Abstract:	Headers for the messenger class

	Date:		10 March 2020
	Author:		E. Scott Daniels
*/

#ifndef _messenger_hpp
#define _messenger_hpp


#include <iostream>
#include <string>
#include <map>
#include <memory>
#include <mutex>

#include <rmr/rmr.h>

#include "message.hpp"

#ifndef RMR_FALSE
	#define RMR_FALSE	0
	#define RMR_TRUE	1
#endif

class Messenger {

	private:
		std::map<int,Callback*> cb_hash;	// callback functions associated with message types
		std::mutex*	gate;					// overall mutex should we need searialisation
		bool		ok_2_run;
		bool		callbacks = false;		// true if callbacks are defined
		void*		mrc;					// message router context
		char*		listen_port;			// port we ask msg router to listen on

		// copy and assignment are PRIVATE so that they fail if xapp tries; messenger cannot be copied!
		Messenger( const Messenger& soi );	
		Messenger& operator=( const Messenger& soi );

	public:
		// -- constants which need an instance; that happens as a global in the .cpp file (wtf C++)
		static const int MAX_PAYLOAD;			// max message size we'll handle
		static const int DEFAULT_CALLBACK;		// parm for add callback to set default

		Messenger( char* port, bool wait4table );	// builder
		Messenger( Messenger&& soi );				// move construction
		Messenger& operator=( Messenger&& soi );	// move operator
		~Messenger();								// destroyer

		void Add_msg_cb( int mtype, user_callback fun_name, void* data );
		std::unique_ptr<Message> Alloc_msg( int payload_size );			// message allocation
		void Listen( );													// lisen driver
		std::unique_ptr<Message> Receive( int timeout );				// receive 1 message
		void Stop( );													// force to stop
		//void Release_mbuf( void* vmbuf );
		bool Wait_for_cts( int max_wait );
};

#endif
