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
	Mnemonic:	callback.hpp
	Abstract:	class description for a callback managment object.
	Date:		9 March 2020
	Author:		E. Scott Daniels
*/


#ifndef _CALLBACK_HPP
#define _CALLBACK_HPP

#include <memory>

class Messenger;
class Message;
#include "message.hpp"

typedef void(*user_callback)( Message& m, int mtype, int subid, int payload_len, Msg_component payload, void* usr_data );

class Callback {

	private:
		user_callback user_fun;
		void*	udata;									// user data

	public:
		Callback( user_callback, void* data );					// builder
		void Drive_cb( Message& m );							// invoker
};


#endif
