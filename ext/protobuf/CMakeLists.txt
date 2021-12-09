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
# 	Date:     07 Dec 2021
# 	Author:   Alexandre Huff

add_library( rc-api STATIC api.pb.h api.pb.cc api.grpc.pb.h api.grpc.pb.cc )
target_include_directories( rc-api PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
