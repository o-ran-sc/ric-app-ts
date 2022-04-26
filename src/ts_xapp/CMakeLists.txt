#==================================================================================
#	Copyright (c) 2020 AT&T Intellectual Property.
#
#   Licensed under the Apache License, Version 2.0 (the "License"),
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#==================================================================================
#

find_package(Protobuf REQUIRED)

add_executable( ts_xapp ts_xapp.cpp )
target_include_directories( ts_xapp PUBLIC ${srcd}/src ${srcd}/ext )
target_link_libraries( ts_xapp
                        ricxfcpp
                        rmr_si
                        pthread
                        rc_objects
                        grpc++
                        ${Protobuf_LIBRARY}
                        utils_objects
                        curl
)

install(
    TARGETS ts_xapp
    DESTINATION ${install_bin}
)

