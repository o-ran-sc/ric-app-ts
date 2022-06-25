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
	Mnemonic:	rc_xapp.cpp
	Abstract:   Implements a simple echo server just for testing gRPC calls
                from TS xApp.

	Date:		08 Dec 2021
	Author:		Alexandre Huff
*/

#include <iostream>

#include <grpc/grpc.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>

#include "../../ext/protobuf/rc.grpc.pb.h"

using namespace std;

class ControlServiceImpl : public rc::MsgComm::Service {
    ::grpc::Status SendRICControlReqServiceGrpc(::grpc::ServerContext* context, const ::rc::RicControlGrpcReq* request,
                                                ::rc::RicControlGrpcRsp* response) override {

        cout << "[RC] gRPC message received\n==============================\n"
                << request->DebugString() << "==============================\n";

        /*
            TODO check if this is related to RICControlAckEnum
            if yes, then ACK value should be 2 (RIC_CONTROL_ACK)
            api.proto assumes that 0 is an ACK
        */
        response->set_rspcode(0);
        response->set_description("ACK");

        return ::grpc::Status::OK;
    }
};

void RunServer() {
    string server_address("0.0.0.0:50051");
    ControlServiceImpl service;

    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    unique_ptr<grpc::Server> server(builder.BuildAndStart());

    cout << "[RC] Server listening on " << server_address << std::endl;
    server->Wait();
}

int main(int argc, char const *argv[]) {
    RunServer();

    return 0;
}
