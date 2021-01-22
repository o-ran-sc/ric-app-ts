#!/usr/bin/env bash
# vim: ts=4 sw=4 noet:

#==================================================================================
#       Copyright (c) 2020 Nokia
#       Copyright (c) 2020 AT&T Intellectual Property.
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

# LICENSE NOTE:
# this code is based on the unit test code in the o-ran-sc RMR repositiory which
# is covered by the original license above, and thus that license governs this
# extension as well.
# ---------------------------------------------------------------------------------

#
#	Mnemonic:	scrub_gcov.sh
#	Abstract:	Gcov (sadly) outputs for any header file that we pull.
#				this scrubs any gcov that doesnt look like it belongs
#				to our code.
#
#	Date:		24 March 2020
#	Author:		E. Scott Daniels
# -----------------------------------------------------------------------------


# Make a list of our modules under test so that we don't look at gcov
# files that are generated for system lib headers in /usr/*
# (bash makes the process of building a list of names  harder than it 
# needs to be, so use caution with the printf() call.)
#
function mk_list {
	for f in *.gcov
	do
		if ! grep -q "Source:\.\./src"  $f
		then
			printf "$f "		# do NOT use echo or add \n!
		fi
	done 
}


list=$( mk_list )
if [[ -n $list ]]
then
	rm $list
fi
