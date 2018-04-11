### To build use:
```
docker build -t nsr-demo:latest -f Dockerfile.demo .
```

### To run use:
```
# get shell inside docker container
docker run -ti nemea-supervisor-sysrepo-edition-demo /bin/bash

# start supervisor
```
./nemea-supervisor  --logs-path=. --verbosity 3
```
