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
	Mnemonic:	callback.cpp
	Abstract:	Function defs for the callback managment object
	Date:		9 March 2020
	Author:		E. Scott Daniels
*/

#include <cstdlib>

#include <rmr/rmr.h>

#include "message.hpp"
//class Messenger;


/*
	Builder.
*/
Callback::Callback( user_callback ufun, void* data ) {		// builder
	user_fun = ufun;
	udata = data;
}

/*
	there is nothing to be done from a destruction perspective, so no
	destruction function needed at the moment.
*/

/*
	Drive_cb will invoke the callback and pass along the stuff passed here.
*/
void Callback::Drive_cb( Message& m ) {
	if( user_fun != NULL ) {
		user_fun( m, m.Get_mtype(), m.Get_subid(), m.Get_len(), m.Get_payload(),  udata );
	}
}


