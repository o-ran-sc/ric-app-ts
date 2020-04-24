# vim: ts=4 sw=4 noet:
#==================================================================================
#	Copyright (c) 2018-2019 AT&T Intellectual Property.
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#	   http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#==================================================================================


# --------------------------------------------------------------------------------------
#	Mnemonic:	Dockerfile
#	Abstract:	This can be used to create a base environment for using the xAPP
#				framework.  It will install RMR and the framework libraries. It also
#				installs make and g++ so that it can be used as a builder environment.
#
#				The unit tests are executed as a part of the build process; if they are
#				not passing then the build will fail.
#
#				Building should be as simple as:
#
#					docker build -f Dockerfile -t ricxfcpp:[version]
#
#	Date:		23 March 2020
#	Author:		E. Scott Daniels
# --------------------------------------------------------------------------------------


FROM nexus3.o-ran-sc.org:10004/bldr-ubuntu18-c-go:6-u18.04-nng as buildenv
RUN mkdir /playpen

RUN apt-get update && apt-get install -y cmake gcc make git g++ wget

RUN mkdir /playpen/bin /playpen/factory /playpen/factory/src /playpen/factory/test
ARG SRC=.

WORKDIR /playpen
# Install RMr (runtime and dev) from debian package cached on packagecloud.io
ARG RMR_VER=3.6.2

# if package cloud is actually working, this is preferred
#
#RUN wget -nv --content-disposition https://packagecloud.io/o-ran-sc/staging/packages/debian/stretch/rmr_${RMR_VER}_amd64.deb/download.deb
#RUN wget -nv --content-disposition https://packagecloud.io/o-ran-sc/staging/packages/debian/stretch/rmr-dev_${RMR_VER}_amd64.deb/download.deb
#RUN dpkg -i rmr_${RMR_VER}_amd64.deb
#RUN dpkg -i rmr-dev_${RMR_VER}_amd64.deb
#
# else this:
#
RUN git config --global http.proxy http://one.proxy.att.com:8080


RUN apt-get install -y cpputest
RUN apt-get remove -y libboost-all-dev
RUN apt-get install -y  libboost-all-dev
RUN apt-get install -y libhiredis-dev
RUN apt-get install -y valgrind


RUN git clone https://gerrit.o-ran-sc.org/r/ric-plt/sdl
RUN cd sdl && \
    ./autogen.sh && \
    ./configure && \
    make all && \
    #    make test && \
    make install


COPY ${SRC}/build_rmr.sh /playpen/bin
RUN bash /playpen/bin/build_rmr.sh -t ${RMR_VER}

COPY ${SRC}/CMakeLists.txt /playpen/factory/
COPY ${SRC}/src /playpen/factory/src/
COPY ${SRC}/test /playpen/factory/test/
#COPY ${SRC}/examples /tmp/examples/


COPY ${SRC}/examples /playpen/factory/examples
COPY ${SRC}/routes.txt /playpen/factory/examples

#
# Run unit tests
#
COPY ${SRC}/test/* /playpen/factory/test/
RUN cd /playpen/factory/test; bash unit_test.sh

# Go to the factory and build our stuff
#
ENV LD_LIBRARY_PATH=/usr/local/lib64:/usr/local/lib:/usr/lib
ENV C_INCLUDE_PATH=/usr/local/include
RUN cd /playpen/factory; rm -fr .build; mkdir .build; cd .build; cmake .. -DDEV_PKG=1; make install; cmake .. -DDEV_PKG=0; make install

RUN cd /playpen/factory/examples; make

RUN ls /usr/local/lib


# -----  final, smaller image ----------------------------------
FROM ubuntu:18.04

# must add compile tools to make it a builder environmnent. If a build environment isn't needed 
# comment out the next line and reduce the image size by more than 180M.
#
RUN apt-get update && apt-get install -y --no-install-recommends make g++

# if bash doesn't cut it for run_replay grab a real shell and clean up as much as we can
RUN apt-get update; apt-get install -y ksh
RUN rm -fr /var/lib/apt/lists 

RUN mkdir -p /usr/local/include/ricxfcpp
COPY --from=buildenv /usr/local/lib /usr/local/lib/
COPY --from=buildenv /usr/local/include/ricxfcpp /usr/local/include/ricxfcpp/
COPY --from=buildenv /usr/local/include/rmr /usr/local/include/rmr/
COPY --from=buildenv /usr/local/lib /usr/local/lib
COPY --from=buildenv /usr/lib /usr/lib/

RUN mkdir -p /examples/
COPY --from=buildenv /playpen/factory/examples/ts_xapp /examples/
COPY --from=buildenv /playpen/factory/examples/routes.txt /examples/


ENV LD_LIBRARY_PATH=/usr/local/lib64:/usr/local/lib
ENV C_INCLUDE_PATH=/usr/local/include

ENV RMR_SEED_RT="routes.txt"

#WORKDIR /factory

WORKDIR /examples/

#CMD [ "make" ]

CMD ./ts_xapp
