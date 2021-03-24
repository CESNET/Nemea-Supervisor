#!/bin/sh
#
# Copyright (C) 2015,2016 CESNET
#
# LICENSE TERMS
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in
#    the documentation and/or other materials provided with the
#    distribution.
# 3. Neither the name of the Company nor the names of its contributors
#    may be used to endorse or promote products derived from this
#    software without specific prior written permission.
#
# ALTERNATIVELY, provided that this notice is retained in full, this
# product may be distributed under the terms of the GNU General Public
# License (GPL) version 2 or later, in which case the provisions
# of the GPL apply INSTEAD OF those given above.
#
# This software is provided ``as is'', and any express or implied
# warranties, including, but not limited to, the implied warranties of
# merchantability and fitness for a particular purpose are disclaimed.
# In no event shall the company or contributors be liable for any
# direct, indirect, incidental, special, exemplary, or consequential
# damages (including, but not limited to, procurement of substitute
# goods or services; loss of use, data, or profits; or business
# interruption) however caused and on any theory of liability, whether
# in contract, strict liability, or tort (including negligence or
# otherwise) arising in any way out of the use of this software, even
# if advised of the possibility of such damage.


if [ "$1" = "install" ] && [ "$2" != "" ]; then
    exit 0
elif [ "$1" = "uninstall" ]; then
    /usr/sbin/userdel nemead
    rm -f /usr/local/bin/supcli
    exit 0
fi


prefix="/usr/local"
exec_prefix="${prefix}"
includedir="${prefix}/include"
libdir="${exec_prefix}/lib"
datarootdir="${prefix}/share"
datadir="${datarootdir}"
pkgdatadir="${datarootdir}/nemea-supervisor"
sysconfdir="${prefix}/etc"

CONFIG_DIRS=(detectors loggers reporters data-sources others)

# Create user and group "nemead"
/usr/bin/getent group nemead >/dev/null || /usr/sbin/groupadd -r nemead
/usr/bin/getent passwd nemead >/dev/null || /usr/sbin/useradd -r -d ${prefix}/etc/ -s /sbin/nologin -g nemead nemead


# creation of needed directories and permissions setup
if ls *.mkdir >/dev/null 2>/dev/null; then
        directories=*.mkdir
else
        directories="${pkgdatadir}"/*.mkdir
fi

for f in $directories; do
        for d in `cat "$f"`; do
                mkdir -p "$d"
                ## This is not secure enough! It grants full access to the directory:
                #chmod 777 "$d"
                if getent passwd nemead > /dev/null && getent group nemead >/dev/null; then
                        chown nemead:nemead "$d"
                fi
        done
done


# sysconfdir
for dirname in ${CONFIG_DIRS[*]}; do
    mkdir -p -m 775 ${prefix}/etc/$dirname
    chown nemead:nemead ${prefix}/etc/$dirname
done
chown nemead:nemead ${prefix}/etc/
chmod 775 ${prefix}/etc/
chown nemead:nemead ${prefix}/etc/supervisor_config_template.xml
chmod 664 ${prefix}/etc/supervisor_config_template.xml
chown nemead:nemead ${prefix}/etc/ipfixcol-startup.xml
chmod 664 ${prefix}/etc/ipfixcol-startup.xml


# localstatedir
mkdir -p -m 775 ${prefix}/var/run/nemea-supervisor
mkdir -p -m 775 ${prefix}/var/log/nemea-supervisor
chown nemead:nemead ${prefix}/var/run/nemea-supervisor
chown nemead:nemead ${prefix}/var/log/nemea-supervisor


# datadir
chown -R nemead:nemead ${prefix}/share/nemea-supervisor
chmod 664 ${prefix}/share/nemea-supervisor/*
for dirname in ${CONFIG_DIRS[*]}; do
    chmod 775 ${prefix}/share/nemea-supervisor/$dirname
done


# Add and enable the nemea-supervisor service
chkconfig --add nemea-supervisor 2> /dev/null || true
systemctl daemon-reload
systemctl enable nemea-supervisor.service 2> /dev/null || true


# Create symlink "supcli"
ln -fs /usr/local/bin/supervisor_cli /usr/local/bin/supcli
