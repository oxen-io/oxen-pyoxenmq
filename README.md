# pylokimq

pybind layer for oxenmq

## building

dependencies

- python3-dev
- cmake 
- pkg-config 
- liboxenmq-dev


check the source out (recursive repo):

    $ git clone --recursive https://github.com/oxen-io/loki-pylokimq
    $ cd loki-pylokimq

build:

    $ python3 setup.py build
    
install as user:

    $ python3 setup.py install --user

