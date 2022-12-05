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
            07 Dec 2021 (Alexandre Huff)
            Update for traffic steering use case in release E.
*/

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <thread>
#include <iostream>
#include <memory>
#include <algorithm>
#include <set>
#include <map>
#include <vector>
#include <string>
#include <unordered_map>
#include<deque>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/schema.h>
#include <rapidjson/reader.h>
#include <rapidjson/prettywriter.h>

#include <rmr/RIC_message_types.h>
#include <ricxfcpp/xapp.hpp>
#include <ricxfcpp/config.hpp>
#include<sstream>

/*
  FIXME unfortunately this RMR flag has to be disabled
  due to name resolution conflicts.
  RC xApp defines the same name for gRPC control messages.
*/
#undef RIC_CONTROL_ACK

#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include "protobuf/rc.grpc.pb.h"

#include "utils/restclient.hpp"


using namespace rapidjson;
using namespace std;
using namespace xapp;

using Namespace = std::string;
using Key = std::string;
using Data = std::vector<uint8_t>;
using DataMap = std::map<Key, Data>;
using Keys = std::set<Key>;


// ----------------------------------------------------------
std::unique_ptr<Xapp> xfw;
std::unique_ptr<rc::MsgComm::Stub> rc_stub;

int downlink_threshold = 0;  // A1 policy type 20008 (in percentage)

// scoped enum to identify which API is used to send control messages
enum class TsControlApi { REST, gRPC };
TsControlApi ts_control_api;  // api to send control messages
string ts_control_ep;         // api target endpoint

typedef struct nodeb {
  string ran_name;
  struct {
    string plmn_id;
    string nb_id;
  } global_nb_id;
} nodeb_t;

unordered_map<string, shared_ptr<nodeb_t>> cell_map; // maps each cell to its nodeb

/* struct UEData {
  string serving_cell;
  int serving_cell_rsrp;
}; */


//https://stackoverflow.com/a/34571089/15098882

static std::string base64_decode(const std::string &in) {

	std::string out;

	std::vector<int> T(256, -1);
	for (int i = 0; i < 64; i++) T["ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[i]] = i;

	int val = 0, valb = -8;
	for (unsigned char c : in) {
		if (T[c] == -1) break;
		val = (val << 6) + T[c];
		valb += 6;
		if (valb >= 0) {
			out.push_back(char((val >> valb) & 0xFF));
			valb -= 8;
		}
	}
	return out;
}

struct PolicyHandler : public BaseReaderHandler<UTF8<>, PolicyHandler> {
  /*
    Assuming we receive the following payload from A1 Mediator
    {"operation": "CREATE", "policy_type_id": 20008, "policy_instance_id": "tsapolicy145", "payload": {"threshold": 5}}
  */
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

struct NodebListHandler : public BaseReaderHandler<UTF8<>, NodebListHandler> {
  vector<string> nodeb_list;
  string curr_key = "";

  bool Key(const Ch* str, SizeType length, bool copy) {
    curr_key = str;
    return true;
  }

  bool String(const Ch* str, SizeType length, bool copy) {
    if( curr_key.compare( "inventoryName" ) == 0 ) {
      nodeb_list.push_back( str );
    }
    return true;
  }
};

struct NodebHandler : public BaseReaderHandler<UTF8<>, NodebHandler> {
	string curr_key = "";
	shared_ptr<nodeb_t> nodeb = make_shared<nodeb_t>();
	std::string meid;
	std::vector<string> cells;

	bool Key(const Ch* str, SizeType length, bool copy) {
		curr_key = str;
		return true;
	}

	bool String(const Ch* str, SizeType length, bool copy) {

		if (curr_key.compare("ranName") == 0) {
			//std::cout << str << "\n";
			nodeb->ran_name = str;
			meid= str;
			//std::cout << "\n meid = " << meid;

		}
		else if (curr_key.compare("plmnId") == 0) {
			//std::cout << str << "\n";
			nodeb->global_nb_id.plmn_id = str;
		}
		else if (curr_key.compare("nbId") == 0) {
			//std::cout <<str<< "\n";
			nodeb->global_nb_id.nb_id = str;
		}
		else if (curr_key.compare("e2nodeComponentRequestPart") == 0) {
			//std::cout << str<<"\n";
			auto message = base64_decode(str);
			//std::cout << message<<"\n";
			int len = meid.length();
			//std::cout << "\n meid = " << meid;
			int counter = 0;
				for (int i = 0; i <len; i++ ){
					if (meid[i] == '_') {
						counter++;
					}
					if( counter == 3) {
						counter = i + 1;
						break;
					}
				}
				std::string last_matching_bits = meid.substr(counter, meid.length());
				len = last_matching_bits.size();
				char b;

				for (int i = 0; i < len; i++) {
					b = last_matching_bits[i];
					b = toupper(b);
					// b = to lower(b); //alternately
					last_matching_bits[i] = b;
				}
				len = message.length();
				//std::cout << "\nlast_matching_bits = " << last_matching_bits;
				int matching_len = last_matching_bits.length();;

					for (int i = 0; i <= len - matching_len; i++ ){
						//std::cout << "\n" << message.substr(i, matching_len);

						if (message.substr(i,matching_len)== last_matching_bits){
							//std::cout << "\nmatched!\n";
							cells.push_back(message.substr(i,10));//cell id is 36 bit long , last  4 bit unused

						}
					}
					len = cells.size();
					for (int i = 0; i < len; i++) {
						cell_map[cells[i]] = nodeb;
					}

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
  string arg ((const char*)payload.get(), len); // RMR payload might not have a nil terminanted char

  cout << "[INFO] Policy Callback got a message, type=" << mtype << ", length=" << len << "\n";
  cout << "[INFO] Payload is " << arg << endl;

  PolicyHandler handler;
  Reader reader;
  StringStream ss(arg.c_str());
  reader.Parse(ss,handler);

  //Set the threshold value
  if (handler.found_threshold) {
    cout << "[INFO] Setting Threshold for A1-P value: " << handler.threshold << "%\n";
    downlink_threshold = handler.threshold;
  }

}

// sends a handover message through REST
void send_rest_control_request( string ue_id, string serving_cell_id, string target_cell_id ) {
  time_t now;
  string str_now;
  static unsigned int seq_number = 0; // static counter, not thread-safe

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
  writer.String( ue_id.c_str() );
  writer.Key( "fromCell" );
  writer.String( serving_cell_id.c_str() );
  writer.Key( "toCell" );
  writer.String( target_cell_id.c_str() );
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

  string msg = s.GetString();

  cout << "[INFO] Sending a HandOff CONTROL message to \"" << ts_control_ep << "\"\n";
  cout << "[INFO] HandOff request is " << msg << endl;

  try {
    // sending request
    restclient::RestClient client( ts_control_ep );
    restclient::response_t resp = client.do_post( "", msg ); // we already have the full path in ts_control_ep

    if( resp.status_code == 200 ) {
        // ============== DO SOMETHING USEFUL HERE ===============
        // Currently, we only print out the HandOff reply
        rapidjson::Document document;
        document.Parse( resp.body.c_str() );
        rapidjson::StringBuffer s;
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(s);
        document.Accept( writer );
        cout << "[INFO] HandOff reply is " << s.GetString() << endl;

    } else {
        cout << "[ERROR] Unexpected HTTP code " << resp.status_code << " from " << \
                client.getBaseUrl() << \
                "\n[ERROR] HTTP payload is " << resp.body.c_str() << endl;
    }

  } catch( const restclient::RestClientException &e ) {
    cout << "[ERROR] " << e.what() << endl;

  }

}

// sends a handover message to RC xApp through gRPC
void send_grpc_control_request( string ue_id, string target_cell_id ) {
  grpc::ClientContext context;

  rc::RicControlGrpcRsp response;
  shared_ptr<rc::RicControlGrpcReq> request = make_shared<rc::RicControlGrpcReq>();

  rc::RICE2APHeader *apHeader = request->mutable_rice2apheaderdata();
  apHeader->set_ranfuncid(3);
  apHeader->set_ricrequestorid( 1 );

  rc::RICControlHeader *ctrlHeader = request->mutable_riccontrolheaderdata();
  ctrlHeader->set_controlstyle( 3 );
  ctrlHeader->set_controlactionid( 1 );
  rc::UeId *ueid =  ctrlHeader->mutable_ueid();
  rc::gNBUEID* gnbue= ueid->mutable_gnbueid();
  gnbue->set_amfuengapid(stoi(ue_id));
  gnbue->add_gnbcuuef1apid(stoi(ue_id));
  gnbue->add_gnbcucpuee1apid(stoi(ue_id));
  rc::Guami* gumi=gnbue->mutable_guami();
  //As of now hardcoded according to the value setted in VIAVI RSG TOOL
  gumi->set_amfregionid("10100000");
  gumi->set_amfsetid("0000000000");
  gumi->set_amfpointer("000001");
  
  //ctrlHeader->set_ueid( ue_id );

  rc::RICControlMessage *ctrlMsg = request->mutable_riccontrolmessagedata();
  ctrlMsg->set_riccontrolcelltypeval( rc::RICControlCellTypeEnum::RIC_CONTROL_CELL_UNKWON );
  //ctrlMsg->set_riccontrolcelltypeval( api::RIC_CONTROL_CELL_UNKWON);
    
    ctrlMsg->set_targetcellid( target_cell_id);

  auto data = cell_map.find(target_cell_id);
  if( data != cell_map.end() ) {
    request->set_e2nodeid( data->second->global_nb_id.nb_id );
    request->set_plmnid( data->second->global_nb_id.plmn_id );
    request->set_ranname( data->second->ran_name );
    gumi->set_plmnidentity(data->second->global_nb_id.plmn_id);
  } else {
    cout << "[INFO] Cannot find RAN name corresponding to cell id = "<<target_cell_id<<endl;
    return;
    request->set_e2nodeid( "unknown_e2nodeid" );
    request->set_plmnid( "unknown_plmnid" );
    request->set_ranname( "unknown_ranname" );
    gumi->set_plmnidentity("unknown_plmnid");
  }
  request->set_riccontrolackreqval( rc::RICControlAckEnum::RIC_CONTROL_ACK_UNKWON );
  //request->set_riccontrolackreqval( api::RIC_CONTROL_ACK_UNKWON);  // not yet used in api.proto
 cout<<"\nin ts xapp grpc message content \n"<< request->DebugString()<<"\n"; 
  grpc::Status status = rc_stub->SendRICControlReqServiceGrpc( &context, *request, &response );

  if( status.ok() ) {
    if( response.rspcode() == 0 ) {
      cout << "[INFO] Control Request succeeded with code=0, description=" << response.description() << endl;
    } else {
      cout << "[ERROR] Control Request failed with code=" << response.rspcode()
           << ", description=" << response.description() << endl;
    }

  } else {
    cout << "[ERROR] failed to send a RIC Control Request message to RC xApp, error_code="
         << status.error_code() << ", error_msg=" << status.error_message() << endl;
  }

}

void prediction_callback( Message& mbuf, int mtype, int subid, int len, Msg_component payload,  void* data ) {
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

  float thresh = 0;
  if( downlink_threshold > 0 ) {  // we also take into account the threshold in A1 policy type 20008
    thresh = serving_cell_throughput * (downlink_threshold / 100.0);
  }

  if ( highest_throughput > ( serving_cell_throughput + thresh ) ) {

    // sending a control request message
    if ( ts_control_api == TsControlApi::REST ) {
      send_rest_control_request( handler.ue_id, handler.serving_cell_id, highest_throughput_cell_id );
    } else {
      send_grpc_control_request( handler.ue_id, highest_throughput_cell_id );
    }

  } else {
    cout << "[INFO] The current serving cell \"" << handler.serving_cell_id << "\" is the best one" << endl;
  }

}

void send_prediction_request( vector<string> ues_to_predict ) {
  std::unique_ptr<Message> msg;
  Msg_component payload;           // special type of unique pointer to the payload

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
void ad_callback( Message& mbuf, int mtype, int subid, int len, Msg_component payload, void* data ) {
  string json ((char *)payload.get(), len); // RMR payload might not have a nil terminanted char

  cout << "[INFO] AD Callback got a message, type=" << mtype << ", length=" << len << "\n";
  cout << "[INFO] Payload is " << json << "\n";

  AnomalyHandler handler;
  Reader reader;
  StringStream ss(json.c_str());
  reader.Parse(ss,handler);

  // just sending ACK to the AD xApp
  mbuf.Send_response( TS_ANOMALY_ACK, Message::NO_SUBID, len, nullptr );  // msg type 30004

  send_prediction_request(handler.prediction_ues);
}

vector<string> get_nodeb_list( restclient::RestClient& client ) {

  restclient::response_t response = client.do_get( "/v1/nodeb/states" );

  NodebListHandler handler;
  if( response.status_code == 200 ) {
    Reader reader;
    StringStream ss( response.body.c_str() );
    reader.Parse( ss, handler );

    cout << "[INFO] nodeb list is " << response.body.c_str() << endl;

  } else {
    if( response.body.empty() ) {
      cout << "[ERROR] Unexpected HTTP code " << response.status_code << " from " << client.getBaseUrl() << endl;
    } else {
      cout << "[ERROR] Unexpected HTTP code " << response.status_code << " from " << client.getBaseUrl() <<
              ". HTTP payload is " << response.body.c_str() << endl;
    }
  }

  return handler.nodeb_list;
}

bool build_cell_mapping() {
  string base_url;
  char *data = getenv( "SERVICE_E2MGR_HTTP_BASE_URL" );
  if ( data == NULL ) {
    base_url = "http://service-ricplt-e2mgr-http.ricplt:3800";
  } else {
    base_url = string( data );
  }

  try {
    restclient::RestClient client( base_url );

    vector<string> nb_list = get_nodeb_list( client );

    for( string nb : nb_list ) {
      string full_path = string("/v1/nodeb/") + nb;
      restclient::response_t response = client.do_get( full_path );
      if( response.status_code != 200 ) {
        if( response.body.empty() ) {
          cout << "[ERROR] Unexpected HTTP code " << response.status_code << " from " << \
                  client.getBaseUrl() + full_path << endl;
        } else {
          cout << "[ERROR] Unexpected HTTP code " << response.status_code << " from " << \
                client.getBaseUrl() + full_path << ". HTTP payload is " << response.body.c_str() << endl;
        }
        return false;
      }

      try {
        NodebHandler handler;
        Reader reader;
        StringStream ss( response.body.c_str() );
        reader.Parse( ss, handler );
      } catch (...) {
        cout << "[ERROR] Got an exception on parsing nodeb (stringstream read parse)\n";
        return false;
      }
    }

  } catch( const restclient::RestClientException &e ) {
    cout << "[ERROR] " << e.what() << endl;
    return false;
  }

  return true;
}

extern int main( int argc, char** argv ) {
  int nthreads = 1;
  char*	port = (char *) "4560";
  shared_ptr<grpc::Channel> channel;

  Config *config = new Config();
  string api = config->Get_control_str("ts_control_api");
  ts_control_ep = config->Get_control_str("ts_control_ep");
  if ( api.empty() ) {
    cout << "[ERROR] a control api (rest/grpc) is required in xApp descriptor\n";
    exit(1);
  }
  if ( api.compare("rest") == 0 ) {
    ts_control_api = TsControlApi::REST;
  } else {
    ts_control_api = TsControlApi::gRPC;

    if( !build_cell_mapping() ) {
      cout << "[ERROR] unable to map cells to nodeb\n";
    }

    channel = grpc::CreateChannel(ts_control_ep, grpc::InsecureChannelCredentials());
    rc_stub = rc::MsgComm::NewStub(channel, grpc::StubOptions());
  }

  fprintf( stderr, "[INFO] listening on port %s\n", port );
  xfw = std::unique_ptr<Xapp>( new Xapp( port, true ) );

  xfw->Add_msg_cb( A1_POLICY_REQ, policy_callback, NULL );          // msg type 20010
  xfw->Add_msg_cb( TS_QOE_PREDICTION, prediction_callback, NULL );  // msg type 30002
  xfw->Add_msg_cb( TS_ANOMALY_UPDATE, ad_callback, NULL ); /*Register a callback function for msg type 30003*/

  xfw->Run( nthreads );

}
