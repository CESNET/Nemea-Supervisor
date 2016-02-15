#!/bin/sh
#
# Copyright (C) 2015 CESNET
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

configfile="$1"

if [ -r "$configfile" -a -w "$configfile" ]; then
        NIC_NAME=`ip route 2>/dev/null | fgrep default | sed 's/^.*dev\s\+\([^ ]\+\)\s.*/\1/'`

        if [ -z "$NIC_NAME" ]; then
                # fallback in case the system does not know ip route
                NIC_NAME=`route -n 2>/dev/null | awk '/^0\.0\.0\.0/ {print $NF}'`
        fi

        echo "Detected NIC according to routing table: $NIC_NAME"
        t=`mktemp`
        sed '/<name>flow_meter/,/<trapinterfaces>/ s/-I \([a-zA-Z0-9]\+\)/-I '$NIC_NAME'/' "$configfile" > "$t" && mv "$t" "$configfile"
        rm -f "$t"
else
        echo "Cannot open '$configfile' (need read&write permissions)" > /dev/stderr
fi

