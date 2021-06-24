// vi: ts=4 sw=4 noet:
/*
==================================================================================
	Copyright (c) 2021 AT&T Intellectual Property.
	Copyright (c) 2021 Alexandre Huff.

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
	Mnemonic:	qp_xapp.cpp
	Abstract:   Simulates both, the QP Driver and QP xApp for testing the behavior
                of the TS xApp.

	Date:		20 May 2021
	Author:		Alexandre Huff
*/

#include <iostream>
#include <memory>
#include <thread>
#include <unistd.h>
#include <string.h>

#include <cstdlib>
#include <ctime>

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/schema.h>
#include <rapidjson/reader.h>

#include <rmr/RIC_message_types.h>
#include "ricxfcpp/xapp.hpp"

using namespace std;
using namespace xapp;
using namespace rapidjson;

unique_ptr<Xapp> xfw;


void prediction_callback( Message& mbuf, int mtype, int subid, int len, Msg_component payload,  void* data ) {
    string json ((char *) payload.get(), len);

    cout << "[QP] Prediction Callback got a message, type=" << mtype << ", length=" << len << "\n";
    cout << "[QP] Payload is " << json << endl;

    Document document;
    document.Parse(json.c_str());

    const Value& uePred = document["UEPredictionSet"];
    if ( uePred.Size() > 0 ) {
        string ueid = uePred[0].GetString();
        // we want to create "{"ueid-user1": {"CID1": [10, 20], "CID2": [30, 40], "CID3": [50, 60]}}";
        string body = "{\"" + ueid + "\": {";
        for ( int i = 1; i <= 3; i++ ) {
            int down = rand() % 100;
            int up = rand() % 100;
            if ( i != 3 ) {
                body += "\"CID" + to_string(i) + "\": [" + to_string(down) + ", " + to_string(up) + "], ";
            } else {
                body += "\"CID" + to_string(i) + "\": [" + to_string(down) + ", " + to_string(up) + "]}}";
            }
        }

        int len = body.size();

        cout << "[QP] Sending a message to TS, length=" << len << "\n";
        cout << "[QP] Message body " << body << endl;

        // payload updated in place, nothing to copy from, so payload parm is nil
        if ( ! mbuf.Send_response( TS_QOE_PREDICTION, Message::NO_SUBID, len, (unsigned char *) body.c_str() ) ) // msg type 30002
            cout << "[ERROR] unable to send a message to TS xApp, state: " << mbuf.Get_state() << endl;
    }
}

int main(int argc, char const *argv[]) {
    int nthreads = 1;

    srand( (unsigned int) time(0) );

    char* port = (char *) "4580";

    cout << "[QP] listening on port " << port << endl;
    xfw = std::unique_ptr<Xapp>( new Xapp( port, true ) );

    xfw->Add_msg_cb( TS_UE_LIST, prediction_callback, NULL ); /*Register a callback function for msg type 30000*/

    xfw->Run( nthreads );

    return 0;
}
