# Gossamer bioinformatics suite

## Prerequisites

This has been built on a fresh install of Ubuntu 16 using only the following
dependencies:

```bash
sudo apt-get install \
        g++ cmake libboost-all-dev pandoc \
	zlib1g-dev libbz2-dev libsqlite3-dev
```
It should run on any recent standard 64 bit Linux environment, with as little as 
2 GB of free RAM.

For best performance, we recommend using a machine with 16-32 GB of RAM.

Gossamer does not build on Ubuntu 14 because the default version of Boost was built
with the incorrect ABI.


## Building

Need to rewrite these, but here's the basics:

```bash
mkdir build
cd build
cmake ..
make
make test
make install
```

To disable building unit tests (these can take a while):

```bash
cmake -DBUILD_tests=OFF ..
```

To build translucent:

```bash
cmake -DBUILD_translucent=ON ..
```

## Documentation

Documentation for each of the executables can be found in

 * [goss](docs/goss.md)
 * [gossple](docs/gossple.md)
 * [xenome](docs/xenome.md)
 * [electus](docs/electus.md)
