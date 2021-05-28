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
#	Abstract:	This dockerfile is used to create an image that can be used to
#				run the traffic steering xAPP in a container.
#
#				Building should be as simple as:
#
#					docker build -f Dockerfile -t ric-app-ts:[version] .
#
#	Date:		27 April 2020
#	Author:		E. Scott Daniels
# --------------------------------------------------------------------------------------

# the builder has: git, wget, cmake, gcc/g++, make, python2/3. v7 dropped nng support
#
FROM nexus3.o-ran-sc.org:10002/o-ran-sc/bldr-ubuntu18-c-go:1.9.0 as buildenv

# spaces to save things in the build image to copy to final image
RUN mkdir -p /playpen/assets /playpen/src /playpen/bin
ARG SRC=.

WORKDIR /playpen

# versions we snarf from package cloud
ARG RMR_VER=4.7.4
# ARG SDL_VER=1.0.4
ARG XFCPP_VER=2.3.3

# package cloud urls for wget
ARG PC_REL_URL=https://packagecloud.io/o-ran-sc/release/packages/debian/stretch
# ARG PC_STG_URL=https://packagecloud.io/o-ran-sc/staging/packages/debian/stretch

# pull in rmr
RUN wget -nv --content-disposition ${PC_REL_URL}/rmr_${RMR_VER}_amd64.deb/download.deb && \
	wget -nv --content-disposition ${PC_REL_URL}/rmr-dev_${RMR_VER}_amd64.deb/download.deb && \
	dpkg -i rmr_${RMR_VER}_amd64.deb rmr-dev_${RMR_VER}_amd64.deb

# pull in xapp framework c++
RUN wget -nv --content-disposition ${PC_REL_URL}/ricxfcpp-dev_${XFCPP_VER}_amd64.deb/download.deb && \
	wget -nv --content-disposition ${PC_REL_URL}/ricxfcpp_${XFCPP_VER}_amd64.deb/download.deb && \
	dpkg -i ricxfcpp-dev_${XFCPP_VER}_amd64.deb ricxfcpp_${XFCPP_VER}_amd64.deb

# # snarf up SDL dependencies, then pull SDL package and install
# RUN apt-get update
# RUN apt-get install -y libboost-filesystem1.65.1 libboost-system1.65.1 libhiredis0.13
# RUN wget -nv --content-disposition ${PC_STG_URL}/sdl_${SDL_VER}-1_amd64.deb/download.deb && \
# 	wget -nv --content-disposition ${PC_STG_URL}/sdl-dev_${SDL_VER}-1_amd64.deb/download.deb &&\
# 	dpkg -i sdl-dev_${SDL_VER}-1_amd64.deb sdl_${SDL_VER}-1_amd64.deb

RUN git clone https://github.com/Tencent/rapidjson && \
   cd rapidjson && \
   mkdir build && \
   cd build && \
   cmake -DCMAKE_INSTALL_PREFIX=/usr/local .. && \
   make install && \
   cd ${STAGE_DIR} && \
   rm -rf rapidjson

# install TS curl dependencies
RUN apt-get update && \
	apt-get install -y libcurl4-openssl-dev

#
# build and install the application(s)
#
COPY . /playpen/src/
RUN cd /playpen/src && \
	rm -fr .build &&\
	mkdir  .build && \
	cd .build && \
	cmake .. && \
	make install

# non-programme things that we need to push to final image
#
COPY assets/bootstrap.rt /playpen/assets

#
# any scripts that are needed; copy to /playpen/bin
#


# -----  create final, smaller, image ----------------------------------
FROM ubuntu:18.04

# # package cloud urls for wget
# ARG PC_REL_URL=https://packagecloud.io/o-ran-sc/release/packages/debian/stretch
# ARG PC_STG_URL=https://packagecloud.io/o-ran-sc/staging/packages/debian/stretch
# ARG SDL_VER=1.0.4

# # sdl doesn't install into /usr/local like everybody else, and we don't want to
# # hunt for it or copy all of /usr, so we must pull and reinstall it.
# RUN apt-get update
# RUN apt-get install -y libboost-filesystem1.65.1 libboost-system1.65.1 libhiredis0.13 wget
# RUN wget -nv --content-disposition ${PC_STG_URL}/sdl_${SDL_VER}-1_amd64.deb/download.deb && \
# 	wget -nv --content-disposition ${PC_STG_URL}/sdl-dev_${SDL_VER}-1_amd64.deb/download.deb &&\
# 	dpkg -i sdl-dev_${SDL_VER}-1_amd64.deb sdl_${SDL_VER}-1_amd64.deb

# RUN rm -fr /var/lib/apt/lists

# install TS curl dependencies in the final image
RUN apt-get update && \
	apt-get install -y libcurl4-openssl-dev && \
	apt-get clean

# snarf the various sdl, rmr, and cpp-framework libraries as well as any binaries
# created (e.g. rmr_rprobe) and the application binary itself
#
COPY --from=buildenv /usr/local/lib /usr/local/lib/
COPY --from=buildenv /usr/local/bin/rmr_probe /usr/local/bin/ts_xapp /usr/local/bin/
COPY --from=buildenv /playpen/bin /usr/local/bin/
COPY --from=buildenv /playpen/assets /data


ENV PATH=/usr/local/bin:$PATH
ENV LD_LIBRARY_PATH=/usr/local/lib64:/usr/local/lib

WORKDIR /data
COPY --from=buildenv /playpen/assets/* /data

# if needed, set RMR vars
ENV RMR_SEED_RT=/data/bootstrap.rt
#ENV RMR_RTG_SVC=rm-host:port
ENV RMR_SRC_ID=service-ricxapp-trafficxapp-rmr.ricxapp:4560
ENV RMR_VCTL_FILE=/tmp/rmr.v
RUN echo "2" >/tmp/rmr.v

# set TS env vars
ENV TS_CONTROL_URL=http://localhost:5000/api/echo

CMD [ "/usr/local/bin/ts_xapp" ]
