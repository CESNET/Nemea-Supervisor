# This is demo
FROM fedora:26

RUN dnf update -y
RUN dnf install autoconf automake gcc gcc-c++ libtool libxml2-devel make pkgconfig \
                libpcap libidn bison flex python3 python3-devel dnf-plugins-core git \
                cmake libcmocka wget -y

RUN dnf copr enable @CESNET/NEMEA -y
RUN dnf install nemea-framework nemea-common nemea-detectors nemea-modules libtrap-devel -y
RUN mkdir -p /var/run/libtrap && chmod 777 /var/run/libtrap

# Install ipfixcol
RUN dnf copr enable @CESNET/IPFIXcol -y
RUN dnf install ipfixcol ipfixcol-lnfstore-output ipfixcol-unirec-output \
                ipfixcol-profilestats-inter -y
RUN mkdir -p /data/flow/live/emails /data/ipfixcol-files


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

RUN dnf install pcre pcre-devel -y

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
		cmake -DCMAKE_INSTALL_PREFIX:PATH=/usr -DCMAKE_BUILD_TYPE:String="Release" -DGEN_PYTHON_VERSION=3 .. && \
		make -j2 && \
		make install && \
		ldconfig
WORKDIR ${SRC_FLD}

# Run sysrepo daemon
RUN sysrepod

# It's probably not needed as user running supervisor is root, but it
#  should be noted somewhere...
RUN mkdir -p /var/run/sysrepo-subscriptions/ && \
    chmod -R 777 /var/run/sysrepo-subscriptions/

# Download test data to be replayed by ipfixsend
WORKDIR /data
RUN wget https://owncloud.cesnet.cz/index.php/s/bWkw9eM0vjuXyuD/download \
         --quiet -O data.ipfix.tar.gz
RUN tar xzf data.ipfix.tar.gz && rm data.ipfix.tar.gz

# Download necessary configuration files for IPFIXcol, NEMEA reporters, etc.
RUN mkdir -p /etc/staas
RUN wget https://owncloud.cesnet.cz/index.php/s/dL1TohhLLHFkgCG/download \
         --quiet -O staas.tar.gz
RUN tar xzf staas.tar.gz -C /etc && rm staas.tar.gz

# Create directories for loggers
RUN mkdir -p /data/logs
WORKDIR /data/logs
RUN mkdir -p dns_amplification_detection voip_fraud_detection/log dns_amplification_detection brute_force_detector dnstunnel_detection haddrscan hoststatsnemea ipblacklistfilter miner_detector sip_bf_detector voip_fraud_detection vportscan

# Update this line if you want a current supervisor build
ARG CACHE_DATE=2018-04-21.1

# Install nemea-supervisor-sysrepo-edition
WORKDIR ${SRC_FLD}
RUN git clone https://github.com/zidekmat/nemea-supervisor-sysrepo-edition && \
		cd nemea-supervisor-sysrepo-edition  && mkdir build && cd build &&  \
		cmake .. && \
		make
RUN /bin/bash ${SRC_FLD}/nemea-supervisor-sysrepo-edition/bootstrap.sh
WORKDIR ${SRC_FLD}/nemea-supervisor-sysrepo-edition/build/src/

CMD ["/bin/bash"]
