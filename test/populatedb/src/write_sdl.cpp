
/*
# ==================================================================================
#       Copyright (c) 2020 AT&T Intellectual Property.
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#          http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
# ==================================================================================
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


using namespace std;
using Namespace = std::string;
using Key = std::string;
using Data = std::vector<uint8_t>;
using DataMap = std::map<Key, Data>;
using Keys = std::set<Key>;


// ----------------------------------------------------------

std::string sdl_namespace_u = "TS-UE-metrics";
std::string sdl_namespace_c = "TS-cell-metrics";


std::unique_ptr<shareddatalayer::SyncStorage> sdl;

Namespace nsu;
Namespace nsc;

void get_sdl_data() {

  std::string prefix2="310";
  Keys K = sdl->findKeys(nsc, prefix2);     // just the prefix
  DataMap Dk = sdl->get(nsc, K);

  std::cout << "K contains " << K.size() << " elements.\n";

  cout << "before forloop\n";


  for(auto si=K.begin();si!=K.end();++si){
    std::vector<uint8_t> val_v = Dk[(*si)]; // 4 lines to unpack a string
    char val[val_v.size()+1];                               // from Data
    int i;
    for(i=0;i<val_v.size();++i) val[i] = (char)(val_v[i]);
    val[i]='\0';
    cout << "KEYS and Values " << (*si).c_str()  << " = " <<  val << "\n";
  }
  
  std::string prefix3="12";
  Keys K2 = sdl->findKeys(nsu, prefix3);     // just the prefix
  DataMap Dk2 = sdl->get(nsu, K2);

  std::cout << "K contains " << K2.size() << " elements.\n";

  cout << "before forloop\n";

  for(auto si=K2.begin();si!=K2.end();++si){
    std::vector<uint8_t> val_v = Dk2[(*si)]; // 4 lines to unpack a string
    char val[val_v.size()+1];                               // from Data
    int i;
    for(i=0;i<val_v.size();++i) val[i] = (char)(val_v[i]);
    val[i]='\0';
    cout << "KEYS and Values " << (*si).c_str()  << " = " <<  val << "\n";
  }
  
  cout << endl;

}


void delete_sdl_data() {


  try{

    std::cout << "Removing All Keys from cell namespace\n";

    sdl->removeAll(nsc);

    std::cout << "Removing All Keys from UE namespace\n";

    sdl->removeAll(nsu);
    
  }
  catch(...){
    cout << "SDL Error in Removing Data for Namespace" << endl;

  }    

}


void write_sdl_data() {

  cout << "before sdl set\n";
  
  try{
    //connecting to the Redis and generating a random key for namespace "hwxapp"
    cout << "IN SDL Set Data";
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

    std::string data_string_cell2 = "{ \"CellID\": \"310-680-200-555002\", \"MeasTimestampPDCPBytes\": \"2020-03-18 02:23:18.220\", \"MeasPeriodPDCPBytes\": 20, \"PDCPBytesDL\": 800000, \"PDCPBytesUL\": 400000, \"MeasTimestampAvailPRB\": \"2020-03-18 02:23:18.220\", \"MeasPeriodAvailPRB\": 20, \"AvailPRBDL\": 30, \"AvailPRBUL\": 45  }";

    Data d2;
    DataMap dmap2;    
    char key2[] = "310-680-200-555002";
    std::cout << "KEY: "<< key2 << std::endl;
    Key k2 = key2;
    d2.assign(data_string_cell2.begin(), data_string_cell2.end());
    //    d.push_back(num);
    dmap2.insert({k2,d});

    sdl->set(nsc, dmap2);



    std::string data_string_cell3 = "{ \"CellID\": \"310-680-200-555003\", \"MeasTimestampPDCPBytes\": \"2020-03-18 02:23:18.220\", \"MeasPeriodPDCPBytes\": 20, \"PDCPBytesDL\": 800000, \"PDCPBytesUL\": 400000, \"MeasTimestampAvailPRB\": \"2020-03-18 02:23:18.220\", \"MeasPeriodAvailPRB\": 20, \"AvailPRBDL\": 30, \"AvailPRBUL\": 45  }";

    Data d3;
    DataMap dmap3;
    char key3[] = "310-680-200-555003";
    std::cout << "KEY: "<< key3 << std::endl;
    Key k3 = key3;
    d3.assign(data_string_cell3.begin(), data_string_cell3.end());
    //    d.push_back(num);
    dmap3.insert({k3,d3});

    sdl->set(nsc, dmap3);


    std::string data_string_ue = "{ \"UEID\": 12345, \"ServingCellID\": \"310-680-200-555002\", \"MeasTimestampUEPDCPBytes\": \"2020-03-18 02:23:18.220\", \"MeasPeriodUEPDCPBytes\": 20,\"UEPDCPBytesDL\": 250000,\"UEPDCPBytesUL\": 100000, \"MeasTimestampUEPRBUsage\": \"2020-03-18 02:23:18.220\", \"MeasPeriodUEPRBUsage\": 20, \"UEPRBUsageDL\": 10, \"UEPRBUsageUL\": 30, \"MeasTimestampRF\": \"2020-03-18 02:23:18.210\",\"MeasPeriodRF\": 40, \"ServingCellRF\": [-115,-16,-5], \"NeighborCellRF\": [  {\"CID\": \"310-680-200-555001\",\"CellRF\": [-90,-13,-2.5 ] }, {\"CID\": \"310-680-200-555003\",	\"CellRF\": [-140,-17,-6 ] } ] }";
      
    Data d4;
    DataMap dmap4;
    char key4[] = "12345";
    std::cout << "KEY: "<< key4 << std::endl;
    d4.assign(data_string_ue.begin(), data_string_ue.end());
    Key k4 = key4;
    //    d.push_back(num);
    dmap4.insert({k4,d4});

    sdl->set(nsu, dmap4);
    
  }
  catch(...){
    cout << "SDL Error in Set Data for Namespace" << endl;

  }

  cout << "after sdl set\n";

}


extern int main( int argc, char** argv ) {

  sdl = shareddatalayer::SyncStorage::create();

  nsu = Namespace(sdl_namespace_u);
  nsc = Namespace(sdl_namespace_c);

  cout << "\n\n";
  get_sdl_data();
  cout << "\n\n";
  cout << "Deleting Data\n";  
  delete_sdl_data();
  cout << "\n\n";  
  get_sdl_data();
  cout << "\n\n";
  cout << "Writing Data\n";
  write_sdl_data();
  cout << "\n\n";
  get_sdl_data();

  return 0;
  
}
