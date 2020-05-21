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

#include <thread>
#include <iostream>
#include <memory>

#include <sdl/syncstorage.hpp>
#include <set>
#include <map>
#include <vector>
#include <string>
#include <unordered_map>

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/schema.h>
#include <rapidjson/reader.h>


#include "ricxfcpp/xapp.hpp"

using namespace rapidjson;
using namespace std;
using Namespace = std::string;
using Key = std::string;
using Data = std::vector<uint8_t>;
using DataMap = std::map<Key, Data>;
using Keys = std::set<Key>;


// ----------------------------------------------------------

std::unique_ptr<Xapp> xfw;

std::string sdl_namespace_u = "TS-UE-metrics";
std::string sdl_namespace_c = "TS-cell-metrics";

int rsrp_threshold = 0;

std::unique_ptr<shareddatalayer::SyncStorage> sdl;

Namespace nsu;
Namespace nsc;

struct UEData {
  string serving_cell;
  int serving_cell_rsrp;

};

struct PolicyHandler : public BaseReaderHandler<UTF8<>, PolicyHandler> {
  unordered_map<string, string> cell_pred;
  std::string ue_id;
  bool ue_id_found = false;
  string curr_key = "";
  string curr_value = "";
  int policy_type_id;
  int policy_instance_id;
  int threshold;
  std::string operation;
  bool found_threshold = false;

  
  bool Null() { cout << "Null()" << endl; return true; }
  bool Bool(bool b) { cout << "Bool(" << boolalpha << b << ")" << endl; return true; }
  bool Int(int i) {
    cout << "Int(" << i << ")" << endl;
    if (curr_key.compare("policy_type_id") == 0) {
      policy_type_id = i;
    } else if (curr_key.compare("policy_instance_id") == 0) {
      policy_instance_id = i;
    } else if (curr_key.compare("threshold") == 0) {
      found_threshold = true;
      threshold = i;
    }

    return true;
  }
  bool Uint(unsigned u) {
    cout << "Int(" << u << ")" << endl;
    if (curr_key.compare("policy_type_id") == 0) {
      policy_type_id = u;
    } else if (curr_key.compare("policy_instance_id") == 0) {
      policy_instance_id = u;
    } else if (curr_key.compare("threshold") == 0) {
      found_threshold = true;
      threshold = u;
    }

    return true;
  }    
  bool Int64(int64_t i) { cout << "Int64(" << i << ")" << endl; return true; }
  bool Uint64(uint64_t u) { cout << "Uint64(" << u << ")" << endl; return true; }
  bool Double(double d) { cout << "Double(" << d << ")" << endl; return true; }
  bool String(const char* str, SizeType length, bool copy) {
    cout << "String(" << str << ", " << length << ", " << boolalpha << copy << ")" << endl;
    if (curr_key.compare("operation") != 0) {
      operation = str;
    }

    return true;
  }
  bool StartObject() {
    cout << "StartObject()" << endl;
    return true;
  }
  bool Key(const char* str, SizeType length, bool copy) {
    cout << "Key(" << str << ", " << length << ", " << boolalpha << copy << ")" << endl;
    curr_key = str;

    return true;
  }
  bool EndObject(SizeType memberCount) { cout << "EndObject(" << memberCount << ")" << endl; return true; }
  bool StartArray() { cout << "StartArray()" << endl; return true; }
  bool EndArray(SizeType elementCount) { cout << "EndArray(" << elementCount << ")" << endl; return true; }

};

struct PredictionHandler : public BaseReaderHandler<UTF8<>, PredictionHandler> {
  unordered_map<string, string> cell_pred;
  std::string ue_id;
  bool ue_id_found = false;
  string curr_key = "";
  string curr_value = "";
  bool Null() { cout << "Null()" << endl; return true; }
  bool Bool(bool b) { cout << "Bool(" << boolalpha << b << ")" << endl; return true; }
  bool Int(int i) { cout << "Int(" << i << ")" << endl; return true; }
  bool Uint(unsigned u) { cout << "Uint(" << u << ")" << endl; return true; }
  bool Int64(int64_t i) { cout << "Int64(" << i << ")" << endl; return true; }
  bool Uint64(uint64_t u) { cout << "Uint64(" << u << ")" << endl; return true; }
  bool Double(double d) { cout << "Double(" << d << ")" << endl; return true; }
  bool String(const char* str, SizeType length, bool copy) {
    cout << "String(" << str << ", " << length << ", " << boolalpha << copy << ")" << endl;
    if (curr_key.compare("") != 0) {
      cout << "Found throughput\n";
      curr_value = str;
      cell_pred[curr_key] = curr_value;
      curr_key = "";
      curr_value = "";
    }

    return true;
  }
  bool StartObject() { cout << "StartObject()" << endl; return true; }
  bool Key(const char* str, SizeType length, bool copy) {
    cout << "Key(" << str << ", " << length << ", " << boolalpha << copy << ")" << endl;
    if (!ue_id_found) {
      cout << "Found UE ID\n";
      ue_id = str;
      ue_id_found = true;
    } else {
      curr_key = str;
    }
    return true;
  }
  bool EndObject(SizeType memberCount) { cout << "EndObject(" << memberCount << ")" << endl; return true; }
  bool StartArray() { cout << "StartArray()" << endl; return true; }
  bool EndArray(SizeType elementCount) { cout << "EndArray(" << elementCount << ")" << endl; return true; }
};


struct UEDataHandler : public BaseReaderHandler<UTF8<>, UEDataHandler> {
  unordered_map<string, string> cell_pred;
  std::string serving_cell_id;
  int serving_cell_rsrp;
  int serving_cell_rsrq;
  int serving_cell_sinr;
  bool in_serving_array = false;
  int rf_meas_index = 0;

  string curr_key = "";
  string curr_value = "";
  bool Null() { cout << "Null()" << endl; return true; }
  bool Bool(bool b) { cout << "Bool(" << boolalpha << b << ")" << endl; return true; }
  bool Int(int i) {
    fprintf(stderr, "Int(%d)\n", i);
    if (in_serving_array) {
      fprintf(stderr, "we are in serving array\n");
      switch(rf_meas_index) {
      case 0:
	serving_cell_rsrp = i;
	break;
      case 1:
	serving_cell_rsrq = i;
	break;
      case 2:
	serving_cell_sinr = i;
	break;
      }
      rf_meas_index++;
    }
    return true;
  }
  bool Uint(unsigned u) {
    fprintf(stderr, "Int(%d)\n", u); return true; }
  bool Int64(int64_t i) { cout << "Int64(" << i << ")" << endl; return true; }
  bool Uint64(uint64_t u) { cout << "Uint64(" << u << ")" << endl; return true; }
  bool Double(double d) { cout << "Double(" << d << ")" << endl; return true; }
  bool String(const char* str, SizeType length, bool copy) {
    fprintf(stderr,"String(%s)\n", str);
    if (curr_key.compare("ServingCellID") == 0) {
      serving_cell_id = str;
    } 

    return true;
  }
  bool StartObject() { cout << "StartObject()" << endl; return true; }
  bool Key(const char* str, SizeType length, bool copy) {
    fprintf(stderr,"Key(%s)\n", str);
    curr_key = str;
    return true;
  }
  bool EndObject(SizeType memberCount) { cout << "EndObject(" << memberCount << ")" << endl; return true; }
  bool StartArray() {
    fprintf(stderr,"StartArray()");
    if (curr_key.compare("ServingCellRF") == 0) {
      in_serving_array = true;
    }
    
    return true;
  }
  bool EndArray(SizeType elementCount) {
    fprintf(stderr, "EndArray()\n");
    if (curr_key.compare("servingCellRF") == 0) {
      in_serving_array = false;
      rf_meas_index = 0;
    }

    return true; }
};


unordered_map<string, UEData> get_sdl_ue_data() {

  fprintf(stderr, "In get_sdl_ue_data()\n");

  unordered_map<string, string> ue_data;

  unordered_map<string, UEData> return_ue_data_map;
    
  std::string prefix3="12";
  Keys K2 = sdl->findKeys(nsu, prefix3);
  DataMap Dk2 = sdl->get(nsu, K2);
  
  string ue_json;
  string ue_id;
  
  for(auto si=K2.begin();si!=K2.end();++si){
    std::vector<uint8_t> val_v = Dk2[(*si)]; // 4 lines to unpack a string
    char val[val_v.size()+1];                               // from Data
    int i;
    fprintf(stderr, "val size %d\n", val_v.size());
    for(i=0;i<val_v.size();++i) val[i] = (char)(val_v[i]);
    val[i]='\0';
      ue_id.assign((std::string)*si);
      
      ue_json.assign(val);
      ue_data[ue_id] =  ue_json;
  }
  
  fprintf(stderr, "after sdl get of ue data\n");
  
  fprintf(stderr, "From UE data map\n");
  
  for (auto map_iter = ue_data.begin(); map_iter != ue_data.end(); map_iter++) {
    UEDataHandler handler;
    Reader reader;
    StringStream ss(map_iter->second.c_str());
    reader.Parse(ss,handler);

    string ueID = map_iter->first;
    string serving_cell_id = handler.serving_cell_id;
    int serv_rsrp = handler.serving_cell_rsrp;
    
    fprintf(stderr,"UE data for %s\n", ueID.c_str());
    fprintf(stderr,"Serving cell %s\n", serving_cell_id.c_str());
    fprintf(stderr,"RSRP for UE %d\n", serv_rsrp);

    return_ue_data_map[ueID] = {serving_cell_id, serv_rsrp};
    
  }
  
  fprintf(stderr, "\n");
  return return_ue_data_map;
}

void policy_callback( Message& mbuf, int mtype, int subid, int len, Msg_component payload,  void* data ) {

  int response_to = 0;	 // max timeout wating for a response
  int rmtype;		// received message type

  
  fprintf( stderr, "Policy Callback got a message, type=%d , length=%d\n" , mtype, len);
  fprintf(stderr, "payload is %s\n", payload.get());
  
  //fprintf( stderr, "callback 1 got a message type = %d len = %d\n", mtype, len );
  mbuf.Send_response( 101, -1, 5, (unsigned char *) "OK1\n" );	// validate that we can use the same buffer for 2 rts calls
  mbuf.Send_response( 101, -1, 5, (unsigned char *) "OK2\n" );

  const char *arg = (const char*)payload.get();

  PolicyHandler handler;
  Reader reader;
  StringStream ss(arg);
  reader.Parse(ss,handler);

  //Set the threshold value

  if (handler.found_threshold) {
    rsrp_threshold = handler.threshold;
  }
  
}

void send_prediction_request(vector<string> ues_to_predict) {

  std::unique_ptr<Message> msg;
  Msg_component payload;                                // special type of unique pointer to the payload
  
  int nthreads = 1;  
  int response_to = 0;   // max timeout wating for a response
  int mtype = 30000;
  int sz;
  int i;
  Msg_component send_payload;
  
  fprintf(stderr, "cb 1\n");

  msg = xfw->Alloc_msg( 2048 );
  
  sz = msg->Get_available_size();  // we'll reuse a message if we received one back; ensure it's big enough
  if( sz < 2048 ) {
    fprintf( stderr, "<SNDR> fail: message returned did not have enough size: %d [%d]\n", sz, i );
    exit( 1 );
  }

  fprintf(stderr, "cb 2");

  string ues_list = "[";

  for (int i = 0; i < ues_to_predict.size(); i++) {
    if (i == ues_to_predict.size() - 1) {
      ues_list = ues_list + " \"" + ues_to_predict.at(i) + "\"";
    } else {
      ues_list = ues_list + " \"" + ues_to_predict.at(i) + "\"" + ",";
    }
  }

  string message_body = "{\"UEPredictionSet\": " + ues_list + "}";

  const char *body = message_body.c_str();

  //  char *body = "{\"UEPredictionSet\": [\"12345\"]}";
  
  send_payload = msg->Get_payload(); // direct access to payload
  //  snprintf( (char *) send_payload.get(), 2048, '{"UEPredictionSet" : ["12345"]}', 1 );
  //  snprintf( (char *) send_payload.get(), 2048, body);
  snprintf( (char *) send_payload.get(), 2048, "{\"UEPredictionSet\": [\"12345\"]}");

  fprintf(stderr, "message body %s\n", send_payload.get());
  
  fprintf(stderr, "cb 3");
  fprintf(stderr, "payload length %d\n", strlen( (char *) send_payload.get() ));
  
  // payload updated in place, nothing to copy from, so payload parm is nil
  if ( ! msg->Send_msg( mtype, Message::NO_SUBID, strlen( (char *) send_payload.get() ), NULL )) {
    fprintf( stderr, "<SNDR> send failed: %d\n", msg->Get_state() );
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

  fprintf( stderr, "Prediction Callback got a message, type=%d , length=%d\n" , mtype, len);
  fprintf(stderr, "payload is %s\n", payload.get());
  
  mbuf.Send_response( 101, -1, 5, (unsigned char *) "OK1\n" );	// validate that we can use the same buffer for 2 rts calls
  mbuf.Send_response( 101, -1, 5, (unsigned char *) "OK2\n" );

  mtype = 0;

  fprintf(stderr, "cb 1\n");

  const char* arg = (const char*)payload.get();

  PredictionHandler handler;
  Reader reader;
  StringStream ss(arg);
  reader.Parse(ss,handler);

  std::string pred_ue_id = handler.ue_id;

  cout << "Prediction for " << pred_ue_id << endl;

  unordered_map<string, string> throughput_map = handler.cell_pred;


  cout << endl;
 
  unordered_map<string, UEData> sdl_data = get_sdl_ue_data();

  //Decision about CONTROL message
  //(1) Identify UE Id in Prediction message
  //(2) Get UEData struct for this UE Id
  //(3) Identify the UE's service cell ID
  //(4) Iterate through Prediction message.
  //    If one of the cells, have a higher throughput prediction than serving cell, log a CONTROL request

  UEData pred_ue_data = sdl_data[pred_ue_id];
  std::string serving_cell_id = pred_ue_data.serving_cell;

  int serving_cell_throughput;
  int highest_throughput;
  std::string highest_throughput_cell_id;
  std::string::size_type str_size;

  cout << "Going through throughtput map:" << endl;

  for (auto map_iter = throughput_map.begin(); map_iter != throughput_map.end(); map_iter++) {
    cout << map_iter->first << " : " << map_iter->second << endl;    
    std::string curr_cellid = map_iter->first;
    cout << "Cell ID is " << curr_cellid;
    int curr_throughput = stoi(map_iter->second, &str_size);
    cout << "Throughput is " << curr_throughput << endl;

    if (curr_cellid.compare(serving_cell_id) == 0) {
      serving_cell_throughput = curr_throughput;
      highest_throughput = serving_cell_throughput;
    }

  }

  //Iterating again to identify the highest throughput prediction

  for (auto map_iter = throughput_map.begin(); map_iter != throughput_map.end(); map_iter++) {
    cout << map_iter->first << " : " << map_iter->second << endl;    
    std::string curr_cellid = map_iter->first;
    cout << "Cell ID is " << curr_cellid;
    int curr_throughput = stoi(map_iter->second, &str_size);
    cout << "Throughput is " << curr_throughput << endl;

    if (curr_throughput > serving_cell_throughput) {
      highest_throughput = curr_throughput;
      highest_throughput_cell_id = curr_cellid;
    }
  }

  if (highest_throughput > serving_cell_throughput) {
    cout << "WE WOULD SEND A CONTROL REQUEST NOW" << endl;
    cout << "UE ID: " << pred_ue_id << endl;
    cout << "Source cell " << serving_cell_id << endl;
    cout << "Target cell " << highest_throughput_cell_id << endl;
  }
  
  
}


//This function runs a loop that continuously checks SDL for any UE

void run_loop() {

  fprintf(stderr, "in run_loop()\n");

  unordered_map<string, UEData> uemap;

  vector<string> prediction_ues;

  while (1) {

    fprintf(stderr, "in while loop\n");

    uemap = get_sdl_ue_data();

    for (auto map_iter = uemap.begin(); map_iter != uemap.end(); map_iter++) {
      string ueid = map_iter->first;
      UEData data = map_iter->second;
      if (data.serving_cell_rsrp < rsrp_threshold) {
	prediction_ues.push_back(ueid);
      }
    }

    if (prediction_ues.size() > 0) {
      send_prediction_request(prediction_ues);
    }

    sleep(20);
  }
}



extern int main( int argc, char** argv ) {

  int nthreads = 1;

  char*	port = (char *) "4560";

  sdl = shareddatalayer::SyncStorage::create();

  nsu = Namespace(sdl_namespace_u);
  nsc = Namespace(sdl_namespace_c);

  
  fprintf( stderr, "<XAPP> listening on port: %s\n", port );
  xfw = std::unique_ptr<Xapp>( new Xapp( port, true ) ); // new xAPP thing; wait for a route table
  fprintf(stderr, "code1\n");

  
  xfw->Add_msg_cb( 20010, policy_callback, NULL );
  xfw->Add_msg_cb( 30002, prediction_callback, NULL );

  fprintf(stderr, "code2\n");
  
  std::thread loop_thread;

  loop_thread = std::thread(&run_loop);

  xfw->Run( nthreads );
  
}
