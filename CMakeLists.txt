#==================================================================================
#	Copyright (c) 2019 Nokia
#	Copyright (c) 2018-2019 AT&T Intellectual Property.
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

# This CMake definition supports several -D command line options:
#
#	-DDEBUG=n			Enable debugging level n
#	-DDEV_PKG=1			Development package configuration
#	-DBUILD_DOC=1		Man pages generated
#	-DPRESERVE_PTYPE=1	Do not change the processor type when naming deb packages
#	-DGPROF=1			Enable profiling compile time flags
#	-DMAN_PREFIX=<path>	Supply a path where man pages are installed (default: /usr/share/man)

project( ricxfcpp )
cmake_minimum_required( VERSION 3.5 )

set( major_version "0" )		# should be automatically populated from git tag later, but until CI process sets a tag we use this
set( minor_version "1" )
set( patch_level "2" )

set( install_root "${CMAKE_INSTALL_PREFIX}" )
set( install_inc "include/ricxfcpp" )
if( MAN_PREFIX )
	set( install_man ${MAN_PREFIX} )			# is there a cmake var for this -- can't find one
else()
	set( install_man "/usr/share/man" )			# this needs to be fixed so it's not hard coded
endif()

# Must use GNUInstallDirs to install libraries into correct
# locations on all platforms.
include( GNUInstallDirs )

# externals may install using LIBDIR as established by the gnu include; it varies from system
# to system, and we don't trust that it is always set, so we default to lib if it is missing.
#
if( NOT CMAKE_INSTALL_LIBDIR )
	set( CMAKE_INSTALL_LIBDIR "lib" )
endif()

set( install_lib "${CMAKE_INSTALL_LIBDIR}" )
message( "+++ ricxfcpp library install target directory: ${install_lib}" )

# ---------------- extract some things from git ------------------------------

# commit id for the version string
execute_process(
	COMMAND bash -c "git rev-parse --short HEAD|awk '{printf\"%s\", $0}'"
	OUTPUT_VARIABLE git_id
)

# version information for library names and version string
execute_process(
	COMMAND bash -c "git describe --tags --abbrev=0 HEAD 2>/dev/null | awk -v tag=0.0.4095 ' { tag=$1 } END{ print  tag suffix }'|sed 's/\\./;/g' "
	OUTPUT_VARIABLE mmp_version_str
	ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE
)
message( "+++ mmp version from tag: '${mmp_version_str}'" )

# extra indicator to show that the build was based on modified file(s) and not the true commit
# (no hope of reproducing the exact library for debugging). Used only for the internal version
# string.
execute_process(
	COMMAND bash -c "git diff --shortstat|awk -v fmt=%s -v r=-rotten '{ s=r } END { printf( fmt, s ) }'"
	OUTPUT_VARIABLE spoiled_str
)

# uncomment these lines once CI starts adding a tag on merge
#set( mmp_version ${mmp_version_str} )
#list( GET mmp_version 0 major_version )
#list( GET mmp_version 1 minor_version )
#list( GET mmp_version 2 patch_level )

if( DEBUG )					# if set, we'll set debugging on in the compile
	set( debugging ${DEBUG} )
	message( "+++ debugging is being set to ${DEBUG}" )
else()
	set( debugging 0 )
	message( "+++ debugging is set to off" )
endif()
unset( DEBUG CACHE )					# we don't want this to persist


# define constants used in the version string, debugging, etc.
add_definitions(
	-DGIT_ID=${git_id}
	-DMAJOR_VER=${major_version}
	-DMINOR_VER=${minor_version}
	-DPATCH_VER=${patch_level}
	-DDEBUG=${debugging}
)

# ---------------- suss out pkg gen tools so we don't fail generating packages that the system cannot support --------------

# deb packages use underbars, and package manager(s) seem to flip the *_64 processor type
# to the old (non-standard) amd64 string, so we do it here for consistency. Set -DPRESERVE_PTYPE=1
# to prevent the flip. RPM packages will always be given the system generated processor type string.
#
if( ${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64" )
	if( NOT PRESERVE_PTYPE )
		set( deb_sys_name "amd64" )
	else()
		set( deb_sys_name ${CMAKE_SYSTEM_PROCESSOR} )
	endif()
else()
	set( deb_sys_name ${CMAKE_SYSTEM_PROCESSOR} )
endif()
unset( PRESERVE_PTYPE CACHE )					# we don't want this to persist

set( rpm_sys_name ${CMAKE_SYSTEM_PROCESSOR} )

if( DEV_PKG )
	set( deb_pkg_name "ricxfcpp-dev" )
	set( rpm_pkg_name "ricxfcpp-devel" )
else()
	set( deb_pkg_name "ricxfcpp" )
	set( rpm_pkg_name "ricxfcpp" )
endif()

set( pkg_label "ricxfcpp${spoiled_str}-${major_version}.${minor_version}.${patch_level}-${sys_name}" )
set( rpm_pkg_label "${rpm_pkg_name}${spoiled_str}-${major_version}.${minor_version}.${patch_level}-${rpm_sys_name}" )
set( deb_pkg_label "${deb_pkg_name}${spoiled_str}_${major_version}.${minor_version}.${patch_level}_${deb_sys_name}" )
message( "+++ pkg name: ${deb_pkg_label}.deb" )

set( gen_rpm 0 )
find_program( rpm NAMES rpmbuild )					# rpm package gen requires this to be installed
if( "${rpm}" MATCHES "rpm-NOTFOUND" )				# cannot build rpm
	set( pkg_list "DEB" )
	message( "### make package will generate only deb package; cannot find support to generate rpm packages" )
else()
	message( "+++ pkg name: ${rpm_pkg_label}.rpm" )		# debugging if we think we can gen rpm too
	set( pkg_list "DEB;RPM" )
	set( gen_rpm 1 )
	message( "+++ make package will generate both deb and rpm packages" )
endif()



# this gets us round a chicken/egg problem. include files don't exist until make is run
# but Cmake insists on having these exist when we add them to include directories to
# enable code to find them after we build them.
#
include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/src/messaging" )


# Compiler flags
#
set( CMAKE_POSITION_INDEPENDENT_CODE ON )
set( CMAKE_C_FLAGS "-g " )
set( CMAKE_CPP_FLAGS "-g " )
if( GPROF )					# if set, we'll set profiling flag on compiles
	message( "+++ profiling is on" )
	set( CMAKE_C_FLAGS "-pg " )
	set( CMAKE_CPP_FLAGS "-pg " )
else()
	message( "+++ profiling is off" )
endif()
unset( GPROF CACHE )					# we don't want this to persist

# Include modules
add_subdirectory( src/messaging )
add_subdirectory( src/xapp )
#add_subdirectory( doc )				# this will auto skip if {X}fm is not available


# shared and static libraries are built from the same object files.
#
add_library( ricxfcpp_shared SHARED "$<TARGET_OBJECTS:message_objects>;$<TARGET_OBJECTS:xapp_objects>" )
set_target_properties( ricxfcpp_shared
	PROPERTIES
	OUTPUT_NAME "ricxfcpp"
	SOVERSION ${major_version}
	VERSION ${major_version}.${minor_version}.${patch_level} 
)
target_include_directories( ricxfcpp_shared PUBLIC "src/messenger" "src/xapp" )

# we only build/export the static archive (.a) if generating a dev package
if( DEV_PKG )
	add_library( ricxfcpp_static STATIC "$<TARGET_OBJECTS:message_objects>;$<TARGET_OBJECTS:xapp_objects>" )
	set_target_properties( ricxfcpp_static
		PROPERTIES
		OUTPUT_NAME "ricxfcpp"
		SOVERSION ${major_version}
		VERSION ${major_version}.${minor_version}.${patch_level} 
	)
	target_include_directories( ricxfcpp_static PUBLIC "src/messenger" "src/xapp" )
endif()

# -------- unit testing -------------------------------------------------------
enable_testing()
add_test(
	NAME drive_unit_tests
	COMMAND bash ../test/unit_test.sh -q
	WORKING_DIRECTORY ../test
)


# ------------- packaging -----------------------------------------------------

# Define what should be installed, and where they should go. For dev package we install
# only the RMr headers, man pages and archive (.a) files.  The run-time package gets just
# the library (.so) files and nothing more.
#
if( DEV_PKG )
	set( target_list "ricxfcpp_static" )
else()
	set( target_list "ricxfcpp_shared" )
endif()

install( TARGETS ${target_list} EXPORT LibraryConfig
   	LIBRARY  DESTINATION ${install_lib}
   	ARCHIVE  DESTINATION ${install_lib}
   	PUBLIC_HEADER DESTINATION ${install_inc}
)

unset( DEV_PKG  CACHE )			# prevent from being a hidden setting if user redoes things

IF( EXISTS "${CMAKE_ROOT}/Modules/CPack.cmake" )
	include( InstallRequiredSystemLibraries )

	set( CPACK_DEBIAN_PACKAGE_NAME ${deb_pkg_name} )
	set( CPACK_RPM_PACKAGE_NAME ${rpm_pkg_name} )

	set( CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION "/usr/local;/usr/local/bin;/usr/local/include;/usr/local/share;/usr/local/lib" )

	set( CPACK_set_DESTDIR "on" )
	set( CPACK_PACKAGING_INSTALL_PREFIX "${install_root}" )
	set( CPACK_GENERATOR "${pkg_list}" )

	set( CPACK_PACKAGE_DESCRIPTION "C++ framework for RIC xAPPs based on RMR." )
	set( CPACK_PACKAGE_DESCRIPTION_SUMMARY "RIC xAPP C++ framework library" )
	set( CPACK_PACKAGE_VENDOR "None" )
	set( CPACK_PACKAGE_CONTACT "None" )
	set( CPACK_PACKAGE_VERSION_MAJOR "${major_version}" )
	set( CPACK_PACKAGE_VERSION_MINOR "${minor_version}" )
	set( CPACK_PACKAGE_VERSION_PATCH "${patch_level}" )
	set( CPACK_PACKAGE "${pkg_label}" )						# generic name for old versions of cpack
	set( CPACK_DEBIAN_FILE_NAME "${deb_pkg_label}.deb" )
	set( CPACK_RPM_FILE_NAME "${rpm_pkg_label}.rpm" )

	# Future: define dependencies on RMR and other libs

	set( CPACK_DEBIAN_PACKAGE_PRIORITY "optional" )
	set( CPACK_DEBIAN_PACKAGE_SECTION "ric" )
	set( CPACK_DEBIAN_ARCHITECTURE ${CMAKE_SYSTEM_PROCESSOR} )
	set( CPACK_RPM_ARCHITECTURE ${CMAKE_SYSTEM_PROCESSOR} )

	# this seems ingnored if included
	#set( CPACK_COMPONENTS_ALL Libraries ApplicationData )

	INCLUDE( CPack )
ENDIF()
