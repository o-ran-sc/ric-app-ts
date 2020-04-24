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
	Mnemonic:	xapp.hpp
	Abstract:	Headers for the xapp class. This class extends the messenger class
				as most of the function of this class would be just passing through
				calls to the messenger object.

	Date:		13 March 2020
	Author:		E. Scott Daniels
*/

#ifndef _xapp_hpp
#define _xapp_hpp


#include <iostream>
#include <string>
#include <mutex>
#include <map>

#include <rmr/rmr.h>

#include "callback.hpp"
#include "messenger.hpp"

class Xapp : public Messenger {

	private:
		std::string name;

		// copy and assignment are PRIVATE because we cant "clone" the listen environment
		Xapp( const Xapp& soi );
		Xapp& operator=( const Xapp& soi );

	public:
		Xapp( char* listen_port, bool wait4rt );	// builder
		Xapp( );
		~Xapp();									// destroyer

		void Run( int nthreads );					// message listen driver
		void Halt( );								// force to stop
};

#endif
