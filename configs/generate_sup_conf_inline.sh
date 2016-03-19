#!/bin/sh
#
# Copyright (C) 2016 CESNET
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

# Input xml file with the template
input_file=sup_conf_inline_templ.xml
# Generated output file
output_file=sup_conf_inline_gener.xml

# Init tmp output file
echo -n "" > "tmp_sup_config_gener.xml"

# Parse the input file with template and replace "include" parts with corresponding parts from .sup files:
cat "$input_file" | while read line; do
	inlined="$(echo "$line" | sed -n 's/^\s*<!--\s*include\s\s*\([^ ]*\)\s*-->.*/\1/p' | xargs cat;)";
	echo -n "$inlined" >> "tmp_sup_config_gener.xml";
	if test -z "$inlined"; then
		echo "$line" >> "tmp_sup_config_gener.xml";
	fi;
done

# Format the final output:
xmllint --format "tmp_sup_config_gener.xml" > "$output_file" 2> /dev/null

if [ $? -ne 0 ]; then
	echo "Warning: xmllint is not installed, cannot format the final $output_file file"
	cat "tmp_sup_config_gener.xml" > "$output_file"
fi

# Delete tmp output file
rm -f "tmp_sup_config_gener.xml"