# ==================================================================================
# 	Copyright (c) 2021 AT&T Intellectual Property.
# 	Copyright (c) 2021 Alexandre Huff.
#
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
#
#        http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.
# ==================================================================================
#
# 	Date:     20 May 2021
# 	Author:   Alexandre Huff


cmake_minimum_required(VERSION 3.14)
project(test_apps)
set(CMAKE_CXX_STANDARD 11)

add_executable(
  ad_xapp
  ad_xapp.cpp
)
target_link_libraries(
  ad_xapp
  ricxfcpp
  rmr_si
  pthread
  curl
)

add_executable(
  qp_xapp
  qp_xapp.cpp
)
target_link_libraries(
  qp_xapp
  ricxfcpp
  rmr_si
  pthread
  curl
)

find_package(Protobuf REQUIRED)
add_subdirectory(${CMAKE_SOURCE_DIR}/../../ext/protobuf EXCLUDED_FROM_ALL)
add_executable(
  rc_xapp
  rc_xapp.cpp
)
target_link_libraries(
  rc_xapp
  rc_objects
  grpc++
  ${Protobuf_LIBRARY}
)
