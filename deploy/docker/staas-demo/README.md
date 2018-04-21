### To build use:
```
docker build -t staas-demo:latest .
```

### To run use:
```
# get shell inside docker container
docker run -ti staas-demo /bin/bash

# start supervisor
./nemea-supervisor  --logs-path=. --verbosity 3
```
