# Nemea Status web

Simple web frontend showing current set of Nemea modules and several statisics - counters of messages transferred over IFCs and CPU and memory consupmption of each module.

The web is written in Python-Flask. It gets data from Nemea Supervisor running on the same machine.

TODO: screenshot

## Requirements

- Nemea Supervisor 1.4.3 or later
- Python 3.x or Python 2.6+
- Flask


## Installation/deployment

For testing, just run `nemea_status.py`, it creates a simple web server listening on port 5000.

For production, configure your web server to load WSGI script `wsgi.py`. How to do this depends on your server and its configuration.

For Apache, the following should probably work:
- Install mod_wsgi.
- Copy `nemea_status` directory to a path where web pages are usally stored in your system, e.g. `/var/www/html`
- Add this to your `/etc/httpd/conf/httpd.conf` (change paths if neccessary):
```
# Point /nemea_status to the WSGI script
WSGIScriptAlias /nemea_status /var/www/html/nemea_status/wsgi.py
WSGIPythonPath /var/www/html/nemea_status

# Static files should be served directly by Apache
Alias /nemea_status/static/ /var/www/html/nemea_status/static/
```
- Consider adding some kind of access control, for example:
```<Directory /var/www/html/nemea_status>
    AuthType Basic
    AuthName "Nemea"

    # AuthUserFile - use accounts specified in .htpasswd file (use htpasswd tool to manage it)
    AuthUserFile "/var/www/html/nemea_status/.htpasswd"

    # AuthShadow - use the same accounts as for SSH access (i.e. use /etc/passwd)
    # (Needs mod_auth_shadow module)
    #AuthShadow on

    require valid-user
</Directory>
```

If it doesn't work, consult documentation of your web server.
