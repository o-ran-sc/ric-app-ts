// vi: ts=4 sw=4 noet:
/*
==================================================================================
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
	Abstract:	Traffic Steering xApp
	           1. Receives A1 Policy
			       2. Receives anomaly detection
			       3. Requests prediction for UE throughput on current and neighbor cells
			       4. Receives prediction
			       5. Optionally exercises Traffic Steering action over E2

	Date:     22 April 2020
	Author:		Ron Shacham

  Modified: 21 May 2021 (Alexandre Huff)
            Update for traffic steering use case in release D.
*/

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <thread>
#include <iostream>
#include <memory>

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
#include <rapidjson/prettywriter.h>

#include <curl/curl.h>
#include <rmr/RIC_message_types.h>
#include "ricxfcpp/xapp.hpp"


// Defines env name for the endpoint to POST handoff control messages
#define ENV_CONTROL_URL "TS_CONTROL_URL"


using namespace rapidjson;
using namespace std;
using namespace xapp;

using Namespace = std::string;
using Key = std::string;
using Data = std::vector<uint8_t>;
using DataMap = std::map<Key, Data>;
using Keys = std::set<Key>;


// ----------------------------------------------------------

// Stores the the URL to POST handoff control messages
const char *ts_control_url;

std::unique_ptr<Xapp> xfw;

int rsrp_threshold = 0;

/* struct UEData {
  string serving_cell;
  int serving_cell_rsrp;
}; */

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

  bool Null() { return true; }
  bool Bool(bool b) { return true; }
  bool Int(int i) {

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
  bool Int64(int64_t i) {  return true; }
  bool Uint64(uint64_t u) {  return true; }
  bool Double(double d) {  return true; }
  bool String(const char* str, SizeType length, bool copy) {

    if (curr_key.compare("operation") != 0) {
      operation = str;
    }

    return true;
  }
  bool StartObject() {

    return true;
  }
  bool Key(const char* str, SizeType length, bool copy) {

    curr_key = str;

    return true;
  }
  bool EndObject(SizeType memberCount) {  return true; }
  bool StartArray() {  return true; }
  bool EndArray(SizeType elementCount) {  return true; }

};

struct PredictionHandler : public BaseReaderHandler<UTF8<>, PredictionHandler> {
  unordered_map<string, int> cell_pred_down;
  unordered_map<string, int> cell_pred_up;
  std::string ue_id;
  bool ue_id_found = false;
  string curr_key = "";
  string curr_value = "";
  string serving_cell_id;
  bool down_val = true;
  bool Null() {  return true; }
  bool Bool(bool b) {  return true; }
  bool Int(int i) {  return true; }
  bool Uint(unsigned u) {
    // Currently, we assume the first cell in the prediction message is the serving cell
    if ( serving_cell_id.empty() ) {
      serving_cell_id = curr_key;
    }

    if (down_val) {
      cell_pred_down[curr_key] = u;
      down_val = false;
    } else {
      cell_pred_up[curr_key] = u;
      down_val = true;
    }

    return true;

  }
  bool Int64(int64_t i) {  return true; }
  bool Uint64(uint64_t u) {  return true; }
  bool Double(double d) {  return true; }
  bool String(const char* str, SizeType length, bool copy) {

    return true;
  }
  bool StartObject() {  return true; }
  bool Key(const char* str, SizeType length, bool copy) {
    if (!ue_id_found) {

      ue_id = str;
      ue_id_found = true;
    } else {
      curr_key = str;
    }
    return true;
  }
  bool EndObject(SizeType memberCount) {  return true; }
  bool StartArray() {  return true; }
  bool EndArray(SizeType elementCount) {  return true; }
};

struct AnomalyHandler : public BaseReaderHandler<UTF8<>, AnomalyHandler> {
  /*
    Assuming we receive the following payload from AD
    [{"du-id": 1010, "ue-id": "Train passenger 2", "measTimeStampRf": 1620835470108, "Degradation": "RSRP RSSINR"}]
  */
  vector<string> prediction_ues;
  string curr_key = "";

  bool Key(const Ch* str, SizeType len, bool copy) {
    curr_key = str;
    return true;
  }

  bool String(const Ch* str, SizeType len, bool copy) {
    // We are only interested in the "ue-id"
    if ( curr_key.compare( "ue-id") == 0 ) {
      prediction_ues.push_back( str );
    }
    return true;
  }
};


/* struct UEDataHandler : public BaseReaderHandler<UTF8<>, UEDataHandler> {
  unordered_map<string, string> cell_pred;
  std::string serving_cell_id;
  int serving_cell_rsrp;
  int serving_cell_rsrq;
  int serving_cell_sinr;
  bool in_serving_array = false;
  int rf_meas_index = 0;

  bool in_serving_report_object = false;

  string curr_key = "";
  string curr_value = "";
  bool Null() { return true; }
  bool Bool(bool b) { return true; }
  bool Int(int i) {

    return true;
  }

  bool Uint(unsigned i) {

    if (in_serving_report_object) {
      if (curr_key.compare("rsrp") == 0) {
	serving_cell_rsrp = i;
      } else if (curr_key.compare("rsrq") == 0) {
	serving_cell_rsrq = i;
      } else if (curr_key.compare("rssinr") == 0) {
	serving_cell_sinr = i;
      }
    }

    return true; }
  bool Int64(int64_t i) {

    return true; }
  bool Uint64(uint64_t i) {

    return true; }
  bool Double(double d) { return true; }
  bool String(const char* str, SizeType length, bool copy) {

    if (curr_key.compare("ServingCellID") == 0) {
      serving_cell_id = str;
    }

    return true;
  }
  bool StartObject() {
    if (curr_key.compare("ServingCellRF") == 0) {
      in_serving_report_object = true;
    }

    return true; }
  bool Key(const char* str, SizeType length, bool copy) {

    curr_key = str;
    return true;
  }
  bool EndObject(SizeType memberCount) {
    if (curr_key.compare("ServingCellRF") == 0) {
      in_serving_report_object = false;
    }
    return true; }
  bool StartArray() {

    if (curr_key.compare("ServingCellRF") == 0) {
      in_serving_array = true;
    }

    return true;
  }
  bool EndArray(SizeType elementCount) {

    if (curr_key.compare("servingCellRF") == 0) {
      in_serving_array = false;
      rf_meas_index = 0;
    }

    return true; }
}; */


/* unordered_map<string, UEData> get_sdl_ue_data() {

  fprintf(stderr, "In get_sdl_ue_data()\n");

  unordered_map<string, string> ue_data;

  unordered_map<string, UEData> return_ue_data_map;

  std::string prefix3="";
  Keys K2 = sdl->findKeys(nsu, prefix3);
  DataMap Dk2 = sdl->get(nsu, K2);

  string ue_json;
  string ue_id;

  for(auto si=K2.begin();si!=K2.end();++si){
    std::vector<uint8_t> val_v = Dk2[(*si)]; // 4 lines to unpack a string
    char val[val_v.size()+1];                               // from Data
    int i;

    for(i=0;i<val_v.size();++i) val[i] = (char)(val_v[i]);
    val[i]='\0';
      ue_id.assign((std::string)*si);

      ue_json.assign(val);
      ue_data[ue_id] =  ue_json;
  }

  for (auto map_iter = ue_data.begin(); map_iter != ue_data.end(); map_iter++) {
    UEDataHandler handler;
    Reader reader;
    StringStream ss(map_iter->second.c_str());
    reader.Parse(ss,handler);

    string ueID = map_iter->first;
    string serving_cell_id = handler.serving_cell_id;
    int serv_rsrp = handler.serving_cell_rsrp;

    return_ue_data_map[ueID] = {serving_cell_id, serv_rsrp};

  }

  return return_ue_data_map;
} */

void policy_callback( Message& mbuf, int mtype, int subid, int len, Msg_component payload,  void* data ) {

  int response_to = 0;	 // max timeout wating for a response
  int rmtype;		// received message type

  string arg ((const char*)payload.get(), len); // RMR payload might not have a nil terminanted char

  cout << "[INFO] Policy Callback got a message, type=" << mtype << ", length="<< len << "\n";
  cout << "[INFO] Payload is " << arg << endl;

  PolicyHandler handler;
  Reader reader;
  StringStream ss(arg.c_str());
  reader.Parse(ss,handler);

  //Set the threshold value
  if (handler.found_threshold) {
    cout << "[INFO] Setting RSRP Threshold to A1-P value: " << handler.threshold << endl;
    rsrp_threshold = handler.threshold;
  }

  mbuf.Send_response( 101, -1, 5, (unsigned char *) "OK1\n" );	// validate that we can use the same buffer for 2 rts calls
  mbuf.Send_response( 101, -1, 5, (unsigned char *) "OK2\n" );
}

// callback to handle handover reply (json http response)
size_t handoff_reply_callback( const char *in, size_t size, size_t num, string *out ) {
  const size_t totalBytes( size * num );
  out->append( in, totalBytes );
  return totalBytes;
}

// sends a handover message through REST
void send_handoff_request( string msg ) {
  CURL *curl = curl_easy_init();
  curl_easy_setopt( curl, CURLOPT_URL, ts_control_url );
  curl_easy_setopt( curl, CURLOPT_TIMEOUT, 10 );
  curl_easy_setopt( curl, CURLOPT_POST, 1L );
  // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

  // response information
  long httpCode( 0 );
  unique_ptr<string> httpData( new string() );

  curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, handoff_reply_callback );
  curl_easy_setopt( curl, CURLOPT_WRITEDATA, httpData.get());
  curl_easy_setopt( curl, CURLOPT_POSTFIELDS, msg.c_str() );

  struct curl_slist *headers = NULL;  // needs to free this after easy perform
  headers = curl_slist_append( headers, "Accept: application/json" );
  headers = curl_slist_append( headers, "Content-Type: application/json" );
  curl_easy_setopt( curl, CURLOPT_HTTPHEADER, headers );

  cout << "[INFO] Sending a HandOff CONTROL message to \"" << ts_control_url << "\"\n";
  cout << "[INFO] HandOff request is " << msg << endl;

  // sending request
  CURLcode res = curl_easy_perform( curl );
  if( res != CURLE_OK ) {
    cout << "[ERROR] curl_easy_perform() failed: " << curl_easy_strerror( res ) << endl;

  } else {

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    if( httpCode == 200 ) {
      // ============== DO SOMETHING USEFUL HERE ===============
      // Currently, we only print out the HandOff reply
      rapidjson::Document document;
      document.Parse( httpData.get()->c_str() );
      rapidjson::StringBuffer s;
	    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(s);
      document.Accept( writer );
      cout << "[INFO] HandOff reply is " << s.GetString() << endl;


    } else if ( httpCode == 404 ) {
      cout << "[ERROR] HTTP 404 Not Found: " << ts_control_url << endl;
    } else {
      cout << "[ERROR] Unexpected HTTP code " << httpCode << " from " << ts_control_url << \
              "\n[ERROR] HTTP payload is " << httpData.get()->c_str() << endl;
    }

  }

  curl_slist_free_all( headers );
  curl_easy_cleanup( curl );
}

void prediction_callback( Message& mbuf, int mtype, int subid, int len, Msg_component payload,  void* data ) {

  time_t now;
  string str_now;
  static unsigned int seq_number = 0; // static counter, not thread-safe

  int response_to = 0;	 // max timeout wating for a response

  int send_mtype = 0;
  int rmtype;							// received message type
  int delay = 1000000;				// mu-sec delay; default 1s

  string json ((char *)payload.get(), len); // RMR payload might not have a nil terminanted char

  cout << "[INFO] Prediction Callback got a message, type=" << mtype << ", length=" << len << "\n";
  cout << "[INFO] Payload is " << json << endl;

  PredictionHandler handler;
  try {
    Reader reader;
    StringStream ss(json.c_str());
    reader.Parse(ss,handler);
  } catch (...) {
    cout << "[ERROR] Got an exception on stringstream read parse\n";
  }

  // We are only considering download throughput
  unordered_map<string, int> throughput_map = handler.cell_pred_down;

  // Decision about CONTROL message
  // (1) Identify UE Id in Prediction message
  // (2) Iterate through Prediction message.
  //     If one of the cells has a higher throughput prediction than serving cell, send a CONTROL request
  //     We assume the first cell in the prediction message is the serving cell

  int serving_cell_throughput = 0;
  int highest_throughput = 0;
  string highest_throughput_cell_id;

  // Getting the current serving cell throughput prediction
  auto cell = throughput_map.find( handler.serving_cell_id );
  serving_cell_throughput = cell->second;

   // Iterating to identify the highest throughput prediction
  for (auto map_iter = throughput_map.begin(); map_iter != throughput_map.end(); map_iter++) {

    string curr_cellid = map_iter->first;
    int curr_throughput = map_iter->second;

    if ( highest_throughput < curr_throughput ) {
      highest_throughput = curr_throughput;
      highest_throughput_cell_id = curr_cellid;
    }

  }

  if ( highest_throughput > serving_cell_throughput ) {
    // building a handoff control message
    now = time( nullptr );
    str_now = ctime( &now );
    str_now.pop_back(); // removing the \n character

    seq_number++;       // static counter, not thread-safe

    rapidjson::StringBuffer s;
	  rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(s);
    writer.StartObject();
    writer.Key( "command" );
    writer.String( "HandOff" );
    writer.Key( "seqNo" );
    writer.Int( seq_number );
    writer.Key( "ue" );
    writer.String( handler.ue_id.c_str() );
    writer.Key( "fromCell" );
    writer.String( handler.serving_cell_id.c_str() );
    writer.Key( "toCell" );
    writer.String( highest_throughput_cell_id.c_str() );
    writer.Key( "timestamp" );
    writer.String( str_now.c_str() );
    writer.Key( "reason" );
    writer.String( "HandOff Control Request from TS xApp" );
    writer.Key( "ttl" );
    writer.Int( 10 );
    writer.EndObject();
    // creates a message like
    /* {
      "command": "HandOff",
      "seqNo": 1,
      "ue": "ueid-here",
      "fromCell": "CID1",
      "toCell": "CID3",
      "timestamp": "Sat May 22 10:35:33 2021",
      "reason": "HandOff Control Request from TS xApp",
      "ttl": 10
    } */

    // sending a control request message
    send_handoff_request( s.GetString() );

  } else {
    cout << "[INFO] The current serving cell \"" << handler.serving_cell_id << "\" is the best one" << endl;
  }

  // mbuf.Send_response( 101, -1, 5, (unsigned char *) "OK1\n" );	// validate that we can use the same buffer for 2 rts calls
  // mbuf.Send_response( 101, -1, 5, (unsigned char *) "OK2\n" );
}

void send_prediction_request( vector<string> ues_to_predict ) {

  std::unique_ptr<Message> msg;
  Msg_component payload;                                // special type of unique pointer to the payload

  int sz;
  int i;
  size_t plen;
  Msg_component send_payload;

  msg = xfw->Alloc_msg( 2048 );

  sz = msg->Get_available_size();  // we'll reuse a message if we received one back; ensure it's big enough
  if( sz < 2048 ) {
    fprintf( stderr, "[ERROR] message returned did not have enough size: %d [%d]\n", sz, i );
    exit( 1 );
  }

  string ues_list = "[";

  for (int i = 0; i < ues_to_predict.size(); i++) {
    if (i == ues_to_predict.size() - 1) {
      ues_list = ues_list + "\"" + ues_to_predict.at(i) + "\"]";
    } else {
      ues_list = ues_list + "\"" + ues_to_predict.at(i) + "\"" + ",";
    }
  }

  string message_body = "{\"UEPredictionSet\": " + ues_list + "}";

  send_payload = msg->Get_payload(); // direct access to payload
  snprintf( (char *) send_payload.get(), 2048, "%s", message_body.c_str() );

  plen = strlen( (char *)send_payload.get() );

  cout << "[INFO] Prediction Request length=" << plen << ", payload=" << send_payload.get() << endl;

  // payload updated in place, nothing to copy from, so payload parm is nil
  if ( ! msg->Send_msg( TS_UE_LIST, Message::NO_SUBID, plen, NULL )) { // msg type 30000
    fprintf( stderr, "[ERROR] send failed: %d\n", msg->Get_state() );
  }

}

/* This function works with Anomaly Detection(AD) xApp. It is invoked when anomalous UEs are send by AD xApp.
 * It parses the payload received from AD xApp, sends an ACK with same UEID as payload to AD xApp, and
 * sends a prediction request to the QP Driver xApp.
 */
void ad_callback( Message& mbuf, int mtype, int subid, int len, Msg_component payload,  void* data ) {
  string json ((char *)payload.get(), len); // RMR payload might not have a nil terminanted char

  cout << "[INFO] AD Callback got a message, type=" << mtype << ", length=" << len << "\n";
  cout << "[INFO] Payload is " << json << "\n";

  AnomalyHandler handler;
  Reader reader;
  StringStream ss(json.c_str());
  reader.Parse(ss,handler);

  // just sending ACK to the AD xApp
  mbuf.Send_response( TS_ANOMALY_ACK, Message::NO_SUBID, len, nullptr );  // msg type 30004

  // TODO should we use the threshold received in the A1_POLICY_REQ message and compare with Degradation in TS_ANOMALY_UPDATE?
  // if( handler.degradation < rsrp_threshold )
  send_prediction_request(handler.prediction_ues);
}

extern int main( int argc, char** argv ) {

  int nthreads = 1;
  char*	port = (char *) "4560";

  // ts_control_url = "http://127.0.0.1:5000/api/echo"; // echo-server in test/app/ directory
  if ( ( ts_control_url = getenv( ENV_CONTROL_URL ) ) == nullptr ) {
    cout << "[ERROR] TS_CONTROL_URL is not defined to POST handoff control messages" << endl;
    return 1;
  }

  fprintf( stderr, "[TS xApp] listening on port %s\n", port );
  xfw = std::unique_ptr<Xapp>( new Xapp( port, true ) );

  xfw->Add_msg_cb( A1_POLICY_REQ, policy_callback, NULL );          // msg type 20010
  xfw->Add_msg_cb( TS_QOE_PREDICTION, prediction_callback, NULL );  // msg type 30002
  xfw->Add_msg_cb( TS_ANOMALY_UPDATE, ad_callback, NULL ); /*Register a callback function for msg type 30003*/

  xfw->Run( nthreads );

}
