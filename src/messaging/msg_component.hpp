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
	Mnemonic:	msg_component.hpp
	Abstract:	Defines a message component type which is needed in order
				to use smart pointers (unique) to point at bytes in the
				RMR message (which are not directly allocated and cannot
				be freed/deleted outside of RMR (require a special destruction
				call in the smart pointer).

	Date:		17 March 2020
	Author:		E. Scott Daniels
*/

#ifndef _MSG_COMPONENT_HPP
#define _MSG_COMPONENT_HPP

#include <memory>

//  -------------- smart pointer support  --------------------------------
/*
	Pointers to a lot of things in the RMR message aren't directly
	allocated and thus cannot be directly freed. To return a smart
	pointer to these we have to ensure that no attempt to free/delete
	the reference is made.  This struct defines a type with a function
	pointer (operator) that is 'registered' as the delete function for
	such a smart pointer, and does _nothing_ when called.
*/
typedef struct {
	void operator()( unsigned char * p ){}
} unfreeable;

/*
	A 'generic' smart pointer to a component in the message which cannot
	be directly freed (e.g. the payload, meid, etc).
*/
using Msg_component = std::unique_ptr<unsigned char, unfreeable>;

#endif
