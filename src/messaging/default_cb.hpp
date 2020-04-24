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
	Mnemonic:	default_cb.hpp
	Abstract:	Headers/prototypes for the default callbacks. The
				default callbacks are those which we install when
				the messenger is created and handles things that
				the application might not want to (e.g. health check).

	Date:		11 March 2020
	Author:		E. Scott Daniels
*/

#ifndef _DEF_CB_H
#define _DEF_CB_H


void Health_ck_cb( Message& mbuf, int mtype, int sid, int len, Msg_component payload, void* data );


#endif
