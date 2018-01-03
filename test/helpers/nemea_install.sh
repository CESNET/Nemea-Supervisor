#!/bin/bash

#https://github.com/sysrepo/sysrepo/blob/master/deploy/docker/sysrepo-netopeer2/platforms/Dockerfile.arch

# cli perks
sudo dnf install zsh htop valgrind -y
sh -c "$(curl -fsSL https://raw.githubusercontent.com/robbyrussell/oh-my-zsh/master/tools/install.sh)"

sudo dnf install autoconf automake gcc gcc-c++ libtool libxml2-devel make pkgconfig libpcap libidn bison flex python3 python3-devel -y
sudo dnf copr enable @CESNET/IPFIXcol
sudo dnf install ipfixcol ipfixcol-json-output ipfixcol-unirec-output ipfixcol-lnfstore-output ipfixcol-profiler-inter ipfixcol-profilestats-inter -y
sudo dnf copr enable @CESNET/NEMEA -y
sudo dnf install nemea-framework-devel -y

# sysrepo deps
sudo dnf install protobuf-c-devel libev-devel protobuf-c pcre-devel swig cmake -y
#  libredblack
  git clone https://github.com/sysrepo/libredblack.git && \
  cd libredblack && \
  ./configure --prefix=/usr && \
  make && \
  sudo make install && \
  sudo ldconfig
cd ~
#  libyang
  git clone https://github.com/CESNET/libyang.git && \
  cd libyang && mkdir build && cd build && \
  cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE:String="Release" .. && \
  make -j2 && \
  sudo make install && \
  sudo ldconfig
cd ~


#  sysrepo
# installs to /etc/sysrepo & /usr/local/bin
  git clone https://github.com/sysrepo/sysrepo.git && \
  cd sysrepo && mkdir build && cd build && \
	cmake -DCMAKE_BUILD_TYPE:String="Release" .. && \
  make -j2 && \
  sudo make install && \
  sudo ldconfig
#  sysrepo python3 bindings
# sudo dnf install python-devel
  mkdir ~/sysrepo/python3_bindings_build/ && cd ~/sysrepo/python3_bindings_build && \
	cmake -DGEN_PYTHON_VERSION=3 .. && \
	make -j2 && sudo make install
cd ~



# dep for flow_meter
sudo dnf install libpcap-devel -y
# install nemea
git clone --recursive https://github.com/CESNET/nemea && \
	cd nemea && \
	./bootstrap.sh && \
	./configure --prefix=/usr && \
	make -j2 && \
	sudo make install
#	sudo mkdir /data && \
#	sudo ./configure --datadir=/data --sysconfdir=/etc/nemea --prefix=/usr --bindir=/usr/bin/nemea --sysconfdir=/etc/nemea --libdir=/usr/lib64 && \
#	sudo make -j 2 && \
#	sudo make install
#

sudo chown -R $(whoami):$(whoami) /etc/sysrepo/
sudo mkdir /var/run/sysrepo-subscriptions
sudo chown -R $(whoami):$(whoami) /var/run/sysrepo-subscriptions/


sudo sysrepoctl --install --module=nemea --yang=configs/nemea.yang --owner=jelen:jelen --permission=644
sudo sysrepocfg --import=nemea-staas-startup-config.xml --datastore startup nemea

FAKE_ROOT=/home/ioku/dropbox/school/diplomka/libs_from_git/installed_libs

yaourt rpm-org
rpm install 
#kvuli ipfixcol
#git clone https://github.com/CESNET/libfastbit 
#autoreconf -i
#./configure --prefix=$FAKE_ROOT --exec-prefix=$FAKE_ROOT --oldincludedir=$FAKE_ROOT/include

## openssl 1.0 required ...
#git clone https://github.com/openssl/openssl/
#OpenSSL_1_0_2-stable
# install ipfixcol
# yaourt lksctp-tools-1.0.17-1 # libsctp
# clone
# ./configure --prefix=/home/ioku/dropbox/school/diplomka/libs_from_git/installed_libs/ --exec-prefix=/home/ioku/dropbox/school/diplomka/libs_from_git/installed_libs/ --oldincludedir=/home/ioku/dropbox/school/diplomka/libs_from_git/installed_libs/include --without-openssl
