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
	Mnemonic:	ts_xapp.cpp
	Abstract:	Traffic Steering xApp;
	                       1. Receives A1 Policy
			       2. Queries SDL to decide which UE to attempt Traffic Steering for
			       3. Requests prediction for UE throughput on current and neighbor cells
			       4. Receives prediction
			       5. Optionally exercises Traffic Steering action over E2

	Date:		22 April 2020
	Author:		Ron Shacham
		
*/

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <iostream>
#include <memory>

#include <sdl/syncstorage.hpp>
#include <set>
#include <map>
#include <vector>
#include <string>

#include "ricxfcpp/xapp.hpp"

using Namespace = std::string;
using Key = std::string;
using Data = std::vector<uint8_t>;
using DataMap = std::map<Key, Data>;
using Keys = std::set<Key>;


// ----------------------------------------------------------

std::unique_ptr<Xapp> xfw;


void policy_callback( Message& mbuf, int mtype, int subid, int len, Msg_component payload,  void* data ) {

  long now;
  long total_count;

  int sz;
  int i;

  int response_to = 0;	 // max timeout wating for a response

  int send_mtype = 0;
  int rmtype;							// received message type
  int delay = 1000000;				// mu-sec delay; default 1s

  std::unique_ptr<Message> msg;
  Msg_component send_payload;				// special type of unique pointer to the payload
  
  fprintf( stderr, "Policy Callback got a message, type=%d , length=%d\n" , mtype, len);
  fprintf(stderr, "payload is %s\n", payload.get());
  
  //fprintf( stderr, "callback 1 got a message type = %d len = %d\n", mtype, len );
  mbuf.Send_response( 101, -1, 5, (unsigned char *) "OK1\n" );	// validate that we can use the same buffer for 2 rts calls
  mbuf.Send_response( 101, -1, 5, (unsigned char *) "OK2\n" );

  mtype = 0;

  fprintf(stderr, "cb 1\n");

  msg = xfw->Alloc_msg( 2048 );
  
  sz = msg->Get_available_size();  // we'll reuse a message if we received one back; ensure it's big enough
  if( sz < 2048 ) {
    fprintf( stderr, "<SNDR> fail: message returned did not have enough size: %d [%d]\n", sz, i );
    exit( 1 );
  }

  fprintf(stderr, "cb 2");
  
  send_payload = msg->Get_payload(); // direct access to payload
  snprintf( (char *) send_payload.get(), 2048, "{\"UEPredictionSet\" : [\"222\", \"333\", \"444\"]}" );	

  fprintf(stderr, "cb 3");    
  
  // payload updated in place, nothing to copy from, so payload parm is nil
  if ( ! msg->Send_msg( mtype, Message::NO_SUBID, strlen( (char *) send_payload.get() )+1, NULL )) {
    fprintf( stderr, "<SNDR> send failed: %d\n", i );
  }

  fprintf(stderr, "cb 4");    

  /*
  msg = xfw->Receive( response_to );
  if( msg != NULL ) {
    rmtype = msg->Get_mtype();
    send_payload = msg->Get_payload();
    fprintf( stderr, "got: mtype=%d payload=(%s)\n", rmtype, (char *) send_payload.get() );
  } 
  */  
  
}


void prediction_callback( Message& mbuf, int mtype, int subid, int len, Msg_component payload,  void* data ) {

  long now;
  long total_count;

  int sz;
  int i;

  int response_to = 0;	 // max timeout wating for a response

  int send_mtype = 0;
  int rmtype;							// received message type
  int delay = 1000000;				// mu-sec delay; default 1s

  std::unique_ptr<Message> msg;
  Msg_component send_payload;				// special type of unique pointer to the payload
  
  fprintf( stderr, "Prediction Callback got a message, type=%d , length=%d\n" , mtype, len);
  fprintf(stderr, "payload is %s\n", payload.get());
  
  mbuf.Send_response( 101, -1, 5, (unsigned char *) "OK1\n" );	// validate that we can use the same buffer for 2 rts calls
  mbuf.Send_response( 101, -1, 5, (unsigned char *) "OK2\n" );

  mtype = 0;

  fprintf(stderr, "cb 1\n");
  
}



extern int main( int argc, char** argv ) {

  std::unique_ptr<Message> msg;
  Msg_component payload;				// special type of unique pointer to the payload

  int nthreads = 1;

  int response_to = 0;	 // max timeout wating for a response  

  int delay = 1000000;				// mu-sec delay; default 1s  

  char*	port = (char *) "4560";

  int ai;  
  
  ai = 1;
  while( ai < argc ) {				// very simple flag processing (no bounds/error checking)
    if( argv[ai][0] != '-' )  {
      break;
    }
    
    switch( argv[ai][1] ) {			// we only support -x so -xy must be -x -y
    case 'd':					// delay between messages (mu-sec)
      delay = atoi( argv[ai+1] );
      ai++;
      break;
      
    case 'p': 
      port = argv[ai+1];	
      ai++;
      break;
      
    case 't':					// timeout in seconds; we need to convert to ms for rmr calls
      response_to = atoi( argv[ai+1] ) * 1000;
      ai++;
      break;
    }
    ai++;
  }
  
  fprintf( stderr, "<XAPP> response timeout set to: %d\n", response_to );
  fprintf( stderr, "<XAPP> listening on port: %s\n", port );
  
  xfw = std::unique_ptr<Xapp>( new Xapp( port, true ) ); // new xAPP thing; wait for a route table

  fprintf(stderr, "code1\n");
  
  xfw->Add_msg_cb( 20010, policy_callback, NULL );
  xfw->Add_msg_cb( 30002, prediction_callback, NULL );

  fprintf(stderr, "code2\n");

  std::string sdl_namespace_u = "TS-UE-metrics";
  std::string sdl_namespace_c = "TS-cell-metrics";

  fprintf(stderr, "code5\n");
  
  std::unique_ptr<shareddatalayer::SyncStorage> sdl(shareddatalayer::SyncStorage::create());

  Namespace nsu(sdl_namespace_u);
  Namespace nsc(sdl_namespace_c);

    /*

  fprintf(stderr, "before sdl set\n");
  
  try{
    //connecting to the Redis and generating a random key for namespace "hwxapp"
    fprintf(stderr, "IN SDL Set Data");
    //    std::string data_string = "{\"rsrp\" : -110}";


    std::string data_string = "{\"CellID\": \"310-680-200-555001\", \"MeasTimestampPDCPBytes\": \"2020-03-18 02:23:18.220\", \"MeasPeriodPDCPBytes\": 20, \"PDCPBytesDL\": 2000000, \"PDCPBytesUL\": 1200000, \"MeasTimestampAvailPRB\": \"2020-03-18 02:23:18.220\", \"MeasPeriodAvailPRB\": 20, \"AvailPRBDL\": 30, \"AvailPRBUL\": 50  }";
      
    DataMap dmap;
    //    char key[4]="abc";
    char key[] = "310-680-200-555001";
    std::cout << "KEY: "<< key << std::endl;
    Key k = key;
    Data d;
    //    uint8_t num = 101;
    d.assign(data_string.begin(), data_string.end());
    //    d.push_back(num);
    dmap.insert({k,d});

    sdl->set(nsc, dmap);

    data_string = "{ \"CellID\": \"310-680-200-555002\", \"MeasTimestampPDCPBytes\": \"2020-03-18 02:23:18.220\", \"MeasPeriodPDCPBytes\": 20, \"PDCPBytesDL\": 800000, \"PDCPBytesUL\": 400000, \"MeasTimestampAvailPRB\": \"2020-03-18 02:23:18.220\", \"MeasPeriodAvailPRB\": 20, \"AvailPRBDL\": 30, \"AvailPRBUL\": 45  }";

    Data d2;
    DataMap dmap2;    
    char key2[] = "310-680-200-555002";
    std::cout << "KEY: "<< key2 << std::endl;
    Key k2 = key2;
    d2.assign(data_string.begin(), data_string.end());
    //    d.push_back(num);
    dmap2.insert({k2,d});

    sdl->set(nsc, dmap2);



    std::string data_string = "{ \"CellID\": \"310-680-200-555003\", \"MeasTimestampPDCPBytes\": \"2020-03-18 02:23:18.220\", \"MeasPeriodPDCPBytes\": 20, \"PDCPBytesDL\": 800000, \"PDCPBytesUL\": 400000, \"MeasTimestampAvailPRB\": \"2020-03-18 02:23:18.220\", \"MeasPeriodAvailPRB\": 20, \"AvailPRBDL\": 30, \"AvailPRBUL\": 45  }";

    Data d3;
    DataMap dmap3;
    char key3[] = "310-680-200-555003";
    std::cout << "KEY: "<< key3 << std::endl;
    Key k3 = key3;
    d3.assign(data_string.begin(), data_string.end());
    //    d.push_back(num);
    dmap3.insert({k3,d3});

    sdl->set(nsc, dmap3);



    data_string = "{ \"UEID\": 12345, \"ServingCellID\": \"310-680-200-555002\", \"MeasTimestampUEPDCPBytes\": \"2020-03-18 02:23:18.220\", \"MeasPeriodUEPDCPBytes\": 20,\"UEPDCPBytesDL\": 250000,\"UEPDCPBytesUL\": 100000, \"MeasTimestampUEPRBUsage\": \"2020-03-18 02:23:18.220\", \"MeasPeriodUEPRBUsage\": 20, \"UEPRBUsageDL\": 10, \"UEPRBUsageUL\": 30, \"MeasTimestampRF\": \"2020-03-18 02:23:18.210\",\"MeasPeriodRF\": 40, \"ServingCellRF\": [-115,-16,-5], \"NeighborCellRF\": [  {\"CID\": \"310-680-200-555001\",\"Cell-RF\": [-90,-13,-2.5 ] }, {\"CID\": \"310-680-200-555003\",	\"Cell-RF\": [-140,-17,-6 ] } ] }";
      
    Data d4;
    DataMap dmap4;
    char key4[] = "12345";
    std::cout << "KEY: "<< key << std::endl;
    d4.assign(data_string.begin(), data_string.end());
    Key k4 = key4;
    //    d.push_back(num);
    dmap4.insert({k4,d4});

    sdl->set(nsu, dmap4);

    
  }
  catch(...){
    fprintf(stderr,"SDL Error in Set Data for Namespace");
    return false;
  }
  
  fprintf(stderr, "after sdl set\n");

    */

  fprintf(stderr, "before sdl get\n");


  std::string prefix2="310";
  Keys K = sdl->findKeys(nsc, prefix2);     // just the prefix
  DataMap Dk = sdl->get(nsc, K);

  std::cout << "K contains " << K.size() << " elements.\n";  

  fprintf(stderr, "before forloop\n");

  for(auto si=K.begin();si!=K.end();++si){
    std::vector<uint8_t> val_v = Dk[(*si)]; // 4 lines to unpack a string
    char val[val_v.size()+1];                               // from Data
    int i;
    for(i=0;i<val_v.size();++i) val[i] = (char)(val_v[i]);
    val[i]='\0';
    fprintf(stderr, "KEYS and Values %s = %s\n",(*si).c_str(), val);
  }


  std::string prefix3="12";
  Keys K2 = sdl->findKeys(nsu, prefix3);     // just the prefix
  DataMap Dk2 = sdl->get(nsu, K2);

  std::cout << "K contains " << K2.size() << " elements.\n";  

  fprintf(stderr, "before forloop\n");

  for(auto si=K2.begin();si!=K2.end();++si){
    std::vector<uint8_t> val_v = Dk2[(*si)]; // 4 lines to unpack a string
    char val[val_v.size()+1];                               // from Data
    int i;
    for(i=0;i<val_v.size();++i) val[i] = (char)(val_v[i]);
    val[i]='\0';
    fprintf(stderr, "KEYS and Values %s = %s\n",(*si).c_str(), val);
  }  
  

  fprintf(stderr, "after sdl get\n");

  xfw->Run( nthreads );

  fprintf(stderr, "code3\n");  
  
  msg = xfw->Alloc_msg( 2048 );

  fprintf(stderr, "code4\n");    

  
}
