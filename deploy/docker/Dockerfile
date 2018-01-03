FROM fedora:26


RUN dnf update -y
RUN dnf install autoconf automake gcc gcc-c++ libtool libxml2-devel make pkgconfig libpcap libidn bison flex python3 python3-devel dnf-plugins-core git -y
RUN dnf install cmake pcre -y
RUN dnf copr enable @CESNET/IPFIXcol -y
RUN dnf install ipfixcol -y
RUN dnf copr enable @CESNET/NEMEA -y
RUN dnf install nemea-framework nemea-common nemea-detectors nemea-modules libtrap libtrap-devel -y

ENV SRC_FLD /var/nemea_source
RUN mkdir ${SRC_FLD}
WORKDIR ${SRC_FLD}

# Build and install sysrepo dependencies
#  1) libredblack
RUN git clone https://github.com/sysrepo/libredblack.git && \
		cd libredblack && \
		./configure --prefix=/usr && \
		make && \
		make install && \
		ldconfig
WORKDIR ${SRC_FLD}

RUN dnf install pcre-devel -y

#  2) libyang
RUN git clone https://github.com/CESNET/libyang.git && \
		cd libyang && mkdir build && cd build && \
		cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE:String="Release" .. && \
		make -j2 && \
		make install && \
		ldconfig
WORKDIR ${SRC_FLD}

# Install sysrepo dependencies from packages
RUN dnf install swig protobuf-c-devel protobuf-c libssh openssl-devel libev-devel -y

# Install sysrepo
RUN git clone https://github.com/sysrepo/sysrepo.git && \
		cd sysrepo && mkdir build && cd build && \
		cmake -DCMAKE_BUILD_TYPE:String="Release" -DGEN_PYTHON_VERSION=3 .. && \
		make -j2 && \
		make install && \
		ldconfig
WORKDIR ${SRC_FLD}

# Install nemea-supervisor-sysrepo-edition
RUN dnf install cmocka -y
RUN git clone https://github.com/zidekmat/nemea-supervisor-sysrepo-edition && \
		cd nemea-supervisor-sysrepo-edition && mkdir build && cd build && \
		cmake .. 
#&& \
#		make
WORKDIR ${SRC_FLD}

CMD ["/bin/bash"]
