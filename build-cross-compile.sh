# !/bin/sh

# the type of host and arch
export HOST=x86_64
export ARCH=arm-linux-gnueabihf
export TARGET=${ARCH}

# inject cross compile tool-chain
export CROSS_ROOT=#the root dir path of cross compile tool-chain
export CROSS_COMPILE=${CROSS_ROOT}/bin/arm-linux-gnueabihf-
export CC=${CROSS_COMPILE}gcc
export CXX=${CROSS_COMPILE}g++
export LD=${CROSS_COMPILE}ld
export AR=${CROSS_COMPILE}ar
export RANLIB=${CROSS_COMPILE}ranlib
export STRIP=${CROSS_COMPILE}strip

# compile leda sdk and demo
make prepare && make && make install
