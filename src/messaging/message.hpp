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
	Mnemonic:	message.hpp
	Abstract:	Headers for the message class.
				This class provides an interface to the user application
				when information from the message is needed.

	Date:		10 March 2020
	Author:		E. Scott Daniels
*/

#ifndef _MESSAGE_HPP
#define _MESSAGE_HPP


#include <iostream>
#include <string>

#include <rmr/rmr.h>

#include "msg_component.hpp"
#include "callback.hpp"
#include "default_cb.hpp"

#ifndef RMR_NO_CLONE
	#define RMR_NO_CLONE	0
	#define RMR_NO_COPY	0
	#define RMR_CLONE	1
	#define RMR_COPY	1
#endif


// ------------------------------------------------------------------------

class Message {
	private:
		rmr_mbuf_t*	mbuf;					// the underlying RMR message buffer
		void*		mrc;					// message router context
		std::shared_ptr<char> psp;			// shared pointer to the payload to give out

	public:
		static const int	NO_CHANGE = -99;			// indicates no change to a send/reply parameter
		static const int	INVALID_MTYPE = -1;
		static const int	INVALID_STATUS = -1;
		static const int	INVALID_SUBID = -2;
		static const int	NO_SUBID = -1;					// not all messages have a subid

		static const int	RESPONSE = 0;					// send types
		static const int	MESSAGE = 1;

		Message( rmr_mbuf_t* mbuf, void* mrc );		// builders
		Message( void* mrc, int payload_len );
		Message( const Message& soi );				// copy cat
		Message& operator=( const Message& soi );	// copy operator
		Message( Message&& soi );				// mover
		Message& operator=( Message&& soi );	// move operator
		~Message();									// destroyer

		std::unique_ptr<unsigned char>  Copy_payload( );		// copy the payload; deletable smart pointer

		std::unique_ptr<unsigned char> Get_meid();				// returns a copy of the meid bytes
		int Get_available_size();
		int	Get_len();
		int	Get_mtype();
		Msg_component Get_payload();
		std::unique_ptr<unsigned char>  Get_src();
		int	Get_state( );
		int	Get_subid();

		void Set_meid( std::shared_ptr<unsigned char> new_meid );
		void Set_mtype( int new_type );
		void Set_subid( int new_subid );
		void Set_len( int new_len );

		bool Reply( );
		bool Send( );
		bool Send( int mtype, int subid, int payload_len, unsigned char* payload, int stype );

		bool Send_msg( int mtype, int subid, int payload_len, std::shared_ptr<unsigned char> payload );
		bool Send_msg( int mtype, int subid, int payload_len, unsigned char* payload );
		bool Send_msg( int payload_len, std::shared_ptr<unsigned char> payload );
		bool Send_msg( int payload_len, unsigned char* payload );

		bool Send_response( int mtype, int subid, int payload_len, std::shared_ptr<unsigned char> response );
		bool Send_response( int mtype, int subid, int payload_len, unsigned char* response );
		bool Send_response( int payload_len, std::shared_ptr<unsigned char> response );
		bool Send_response( int payload_len, unsigned char* response );
};


#endif
