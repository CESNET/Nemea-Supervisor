# How to install nagios nrpe plugin

Append the content of `conf/nrpe-part.cfg` into `/etc/nagios/nrpe.cfg`.

Copy files from `scripts/` into `/usr/lib64/nagios/plugins/nemea/`.

Restart `nrpe` using `service nrpe restart`.

You should have allowed 5666/tcp in firewall and access from nagios-server in the `/etc/nagios/nrpe.cfg`.

