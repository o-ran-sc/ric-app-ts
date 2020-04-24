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
	Mnemonic:	xapp.cpp
	Abstract:	The main xapp class.
				This class actually extends the messenger class as most of what
				would be done here is just passing through to the messenger.

	Date:		13 March 2020
	Author:		E. Scott Daniels
*/

#include <string.h>
#include <unistd.h>

#include <rmr/rmr.h>
#include <rmr/RIC_message_types.h>


#include <iostream>
#include <thread>

#include <messenger.hpp>
#include "xapp.hpp"

// --------------- private -----------------------------------------------------



// --------------- builders -----------------------------------------------

/*
	If wait4table is true, then the construction of the object does not
	complete until the underlying transport has a new copy of the route
	table.

	If port is nil, then the default port is used (4560).
*/
Xapp::Xapp( char* port, bool wait4table ) : Messenger( port, wait4table ) {
	// nothing to do; all handled in Messenger constructor
}

/*
	Destroyer.
*/
Xapp::~Xapp() {
	// nothing to destroy; superclass destruction is magically invoked.
}

/*
	Start n threads, each running a listener which will drive callbacks.

	The final listener is started in the main thread; in otherwords this
	function won't return unless that listener crashes.
*/
void Xapp::Run( int nthreads ) {
	int i;
	std::thread** tinfo;				// each thread we'll start

	tinfo = new std::thread* [nthreads-1];

	for( i = 0; i < nthreads - 1; i++ ) {				// thread for each n-1; last runs here
		tinfo[i] = new std::thread( &Xapp::Listen, this );
	}

	this->Listen();						// will return only when halted

	for( i = 0; i < nthreads - 1; i++ ) {				// wait for others to stop
		tinfo[i]->join();
	}

	delete tinfo;
}

/*
	Halt the xapp. This will drive the messenger's stop function to prevent any
	active listeners from running, and will shut things down.
*/
void Xapp::Halt() {
	this->Stop();			// messenger shut off
}

