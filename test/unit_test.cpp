// vim: ts=4 sw=4 noet :
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
	Mnemonic:	Unit_test.cpp
	Abstract:	This is the unit test driver for the C++ xAPP framework. It 
				operates by including all of the modules directly (in order
				to build them with the necessary coverage flags), then 
				drives all that it can.  The RMR emulation module provides
				emulated RMR functions which simulate the creation, sending
				and receiving of messages etc.

	Date:		20 March 2020
	Author: 	E. Scott Daniels
*/

#include <memory>


#include "../src/messaging/callback.hpp"
#include "../src/messaging/default_cb.hpp"
#include "../src/messaging/message.hpp"
#include "../src/messaging/messenger.hpp"
#include "../src/messaging/msg_component.hpp"
#include "../src/xapp/xapp.hpp"

#include "../src/messaging/callback.cpp"
#include "../src/messaging/default_cb.cpp"
#include "../src/messaging/message.cpp"
#include "../src/messaging/messenger.cpp"
#include "../src/xapp/xapp.cpp"

/*
	callback error counts are global for ease. They track the number of times each callback
	was invoked with the expected message type(s) and any times they were not.
*/
int err_cb1 = 0;
int err_cb2 = 0;
int err_cbd = 0;

int good_cb1 = 0;
int good_cb2 = 0;
int good_cbd = 0;

/*
	callback functions to register; driven as we "receive" messages (the RMR emulation package 
	will generate a message every time the receive function is called).
*/
void cb1( Message& mbuf, int mtype, int subid, int len, Msg_component payload,  void* data ) {
	if( mtype != 1 ) {	// should only be driven for type 1 messages
		err_cb1++;
	} else {
		good_cb1++;
	}
}
void cb2( Message& mbuf, int mtype, int subid, int len, Msg_component payload,  void* data ) {
	if( mtype != 2 ) {	// should only be driven for type 2 messages
		err_cb2++;
	} else {
		good_cb2++;
	}
}
void cbd( Message& mbuf, int mtype, int subid, int len, Msg_component payload,  void* data ) {
	if( mtype > 0 && mtype < 3 ) {				// should only be driven for types that arent 1 or 2 
		if( err_cbd < 10 ) {
			fprintf( stderr, "<FAIL> cbd: bad message type: %d\n", mtype );
		}
		err_cbd++;
	} else {
		good_cbd++;
	}
}

/*
	The Xapp Run() function only returns when Xapp is asked to stop, and that
	isn't supported from inside any of the callbacks.  This funciton is 
	started in a thread and after a few seconds it will drive the halt 
	function in the Xapp instance to stop the run function and allow the
	unit test to finish.
*/
void killer( std::shared_ptr<Xapp> x ) {
	fprintf( stderr, "<INFO> killer is waiting in the shadows\n" );
	sleep( 2 );
	fprintf( stderr, "<INFO> killer is on the loose\n" );
	x->Halt();
}

int main( int argc, char** argv ) {
	std::thread* tinfo;					// we'll start a thread that will shut things down after a few seconds
	std::unique_ptr<Message> msg;
	std::shared_ptr<Xapp> x;
	Msg_component payload;
	std::unique_ptr<unsigned char> ucs;
	unsigned char* new_payload;
	std::shared_ptr<unsigned char> new_p_ref;	// reference to payload to pass to send functions
	char*	port = (char *) "4560";
	int		ai = 1;							// arg processing index
	int		nthreads = 2;					// ensure the for loop is executed in setup
	int		i;
	int		len;
	int		errors = 0;
	char	wbuf[256];

	ai = 1;
	while( ai < argc ) {				// very simple flag processing (no bounds/error checking)
		if( argv[ai][0] != '-' )  {
			break;
		}

		switch( argv[ai][1] ) {			// we only support -x so -xy must be -x -y
			case 'p': 
				port = argv[ai+1];	
				ai++;
				break;

			case 't':
				nthreads = atoi( argv[ai+1] );
				ai++;
				break;
		}

		ai++;
	}
	
	x = std::shared_ptr<Xapp>( new Xapp( port, true ) );
	x->Add_msg_cb( 1, cb1, NULL );
	x->Add_msg_cb( 2, cb2, NULL );
	x->Add_msg_cb( -1, cbd, NULL );

	msg = x->Alloc_msg( 2048 );
	payload = msg->Get_payload();
	msg->Set_len( 128 );
	msg->Set_mtype( 100 );
	msg->Set_subid( -10 );

	ucs = msg->Copy_payload( );
	if( ucs == NULL ) {
		fprintf( stderr, "<FAIL> expected pointer to copy payload but got nil\n" );
		errors++;
	}

	ucs = msg->Get_meid();
	if( ucs == NULL ) {
		fprintf( stderr, "<FAIL> expected pointer to meid copy but got nil\n" );
		errors++;
	}

	ucs = msg->Get_src();
	if( ucs == NULL ) {
		fprintf( stderr, "<FAIL> expected pointer to src copy but got nil\n" );
		errors++;
	}

	i = msg->Get_available_size();
	if( i != 2048 ) {
		fprintf( stderr, "<FAIL> len expected payload avail size of 2048 but got %d\n", i );
		errors++;
	}

	i = msg->Get_mtype();
	if( i != 100 ) {
		fprintf( stderr, "<FAIL> expected mtype of 100 but got %d\n", i );
		errors++;
	}

	i = msg->Get_state( );
	if( i != 0 ) {
		fprintf( stderr, "<FAIL> expected state of 0 but got %d\n", i );
		errors++;
	}

	i = msg->Get_subid();
	if( i != -10 ) {
		fprintf( stderr, "<FAIL> expected subid of -10 but got %d\n", i );
		errors++;
	}

	i = msg->Get_len();
	if( i != 128 ) {
		fprintf( stderr, "<FAIL> len expected 128 but got %d\n", i );
		errors++;
	}

	msg->Send();				// generic send as is functions
	msg->Reply();

	new_payload = (unsigned char *) malloc( sizeof( unsigned char ) * 2048 );	// a large payload
	memset( new_payload, 0 , sizeof( unsigned char ) * 2048 );
	new_p_ref = std::shared_ptr<unsigned char>( new_payload );					// reference it for send calls

	msg->Set_meid( new_p_ref );

	msg->Send_msg( 255, new_p_ref );											// send without changing the message type/subid from above
	msg->Send_msg( 255, new_payload );											// drive the alternate prototype
	msg->Send_msg( 100, 1, 128, new_p_ref );									// send using just 128 bytes of payload
	msg->Send_msg( 100, 1, 128, new_payload );									// drive with alternate prototype

	msg->Set_len( 128 );
	msg->Set_mtype( 100 );
	msg->Set_subid( -10 );

	msg->Send_response( 100, new_p_ref );										// send a response (rts) with establisehd message type etc
	msg->Send_response( 100, new_payload );
	msg->Send_response( 100, 10, 100, new_p_ref );
	msg->Send_response( 100, 10, 100, new_payload );


	msg = NULL;								// should drive the message destroyer for coverage

	msg = x->Receive( 2000 );
	if( msg == NULL ) {
		fprintf( stderr, "<FAIL> expected message from receive but got nil\n" );
		errors++;
	}

	tinfo = new std::thread;				// start killer thread to terminate things so that run doesn't hang forever
	tinfo = new std::thread( killer, x );

	x->Run( nthreads );
	x->Halt();			// drive for coverage

	if( err_cb1 + err_cb2 + err_cbd > 0 ) {
		fprintf( stderr, "<FAIL> one or more callbacks reported an error:  [%d] [%d] [%d]\n", err_cb1, err_cb2, err_cbd );
		fprintf( stderr, "<INFO> callback good values:  [%d] [%d] [%d]\n", good_cb1, good_cb2, good_cbd );
		errors++;
	}

	// -----  specific move/copy coverage drivers ---------------------------
	
	Messenger m1( (char *) "1234", false );		// messenger class does NOT permit copies, so no need to test
	Messenger m2( (char *) "9999", false );
	m1 = std::move( m2 );						// drives move operator= function
	Messenger m3 = std::move( m1 );				// drives move constructor function

	std::unique_ptr<Message> msg2 = x->Alloc_msg( 2048 );
	std::unique_ptr<Message> msg3 = x->Alloc_msg( 4096 );

	snprintf( wbuf, sizeof( wbuf ), "Stand up and cheer!!" );
	msg3->Set_len( strlen( wbuf ) );
	strcpy( (char *) (msg3->Get_payload()).get(), wbuf );			// populate the payload to vet copy later
	fprintf( stderr, "<DBUG> set string (%s) \n", (char *) (msg3->Get_payload()).get() );

	Message msg4 = *(msg3.get());									// drive copy builder; msg4 should have a 4096 byte payload
	fprintf( stderr, "<DBUG> copy string (%s) \n", (char *) (msg4.Get_payload()).get() );	// and payload should be coppied
	if( msg4.Get_available_size() != 4096 ) {
		errors++;
		fprintf( stderr, "<FAIL> message copy builder payload size smells: expected 4096, got %d\n", msg4.Get_available_size() );
	}
	if( strcmp( (char *) msg4.Get_payload().get(), wbuf ) != 0 ) {
		errors++;
		fprintf( stderr, "<FAIL> message copy builder payload of copy not the expected string\n" );
	}

	snprintf( wbuf, sizeof( wbuf ), "Rambling Wreck; GT!" );		// different string into msg 2 to ensure copy replaced msg3 string
	strcpy( (char *) (msg2->Get_payload()).get(), wbuf );			// populate the msg2 payload to vet copy
	msg2->Set_len( strlen( wbuf ) );
	*msg3 = *msg2;													// drive the copy operator= function
	if( msg3->Get_available_size() != 2048 ) {
		errors++;
		fprintf( stderr, "<FAIL> message copy operator payload size smells: expected 2048, got %d\n", msg3->Get_available_size() );
	}
	if( strcmp( (char *) msg3->Get_payload().get(), wbuf ) != 0 ) {
		errors++;
		fprintf( stderr, "<FAIL> message copy builder payload of copy not the expected string\n" );
	}

	Message msg5 = std::move( *(msg3.get()) );					// drive move constructor
	if( msg5.Get_available_size() != 2048 ) {
		errors++;
		fprintf( stderr, "<FAIL> message copy operator payload size smells: expected 2048, got %d\n", msg5.Get_available_size() );
	}
	if( strcmp( (char *) msg5.Get_payload().get(), wbuf ) != 0 ) {
		errors++;
		fprintf( stderr, "<FAIL> message copy builder payload of copy not the expected string\n" );
	}

	msg5.Set_len( 2 );					// bogus len for vetting later
	msg5 = std::move( *(msg3.get()) );
	if( msg5.Get_len() == 21 ) {
		errors++;
		fprintf( stderr, "<FAIL> message move operator payload len smells: expected 21, got %d\n", msg5.Get_len() );
	}

	return errors > 0;
}
