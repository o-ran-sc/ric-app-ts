# vim: sw=4 ts=4 noet:
#
#==================================================================================
#   Copyright (c) 2022 Nokia
#   Copyright (c) 2022 AT&T Intellectual Property.
#
#   Licensed under the Apache License, Version 2.0 (the "License");
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


# For clarity: this generates object, not a lib as the CM command implies.
#
add_library( utils_objects OBJECT
	restclient.cpp
)

target_include_directories (utils_objects PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
	$<INSTALL_INTERFACE:include>
)

# header files should go into .../include/ts_xapp/
if( DEV_PKG )
	install( FILES
		restclient.hpp
		DESTINATION ${install_inc}
	)
endif()
