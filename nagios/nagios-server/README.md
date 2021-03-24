# How to add NEMEA into nagios server

Copy all files from `conf/` into `/etc/nagios/conf.d/`.

Edit `/etc/nagios/conf.d/nemea-hosts.cfg` in order to contain
appropriate information such as IP address of the host running
NEMEA.

You may add multiple hosts using `define host`, just make sure you
add them into `nemea-collectors` hostgroup as well.

Run `service nagios restart` after changes in configuration.

