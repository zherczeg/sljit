#!/bin/bash -e

MAKE=${MAKE:-make}

if [ -d bin ]; then
  $MAKE clean
fi

if [ -e /etc/os-release ]; then
  source /etc/os-release
  VERSION_ID=${VERSION_ID/./} # 20.04 becomes 2004
else
  VERSION_ID=0
  ID=unknown
fi

$MAKE CC=gcc
bin/sljit_test -s

case $ID in
  ubuntu|debian|redhat|centos|fedora)
    ARCH=$(uname -m)
    if [ $? -eq 0 ] && [ "$ARCH" != "x86_64" ]; then
      exit 0
    fi
    ;;
  *)
    exit 0
esac

$MAKE clean
if [ "$ID" != "debian" ] && [ "$ID" != "ubuntu" ]; then
  # only do 32-bit as other architectures are custom
  $MAKE CC="gcc -m32" sljit_test
  bin/sljit_test -s
else
  $MAKE CC="aarch64-linux-gnu-gcc -static" bin/sljit_test
  qemu-aarch64-static bin/sljit_test -s

  $MAKE clean
  $MAKE CC="arm-linux-gnueabihf-gcc -static" bin/sljit_test
  qemu-arm-static bin/sljit_test -s

  $MAKE clean
  $MAKE CC="arm-linux-gnueabihf-gcc -marm -static" bin/sljit_test
  qemu-arm-static bin/sljit_test -s

  $MAKE clean
  $MAKE CC="arm-linux-gnueabihf-gcc -march=armv4 -marm -static" bin/sljit_test
  qemu-arm-static bin/sljit_test -s

  if [ $VERSION_ID -ne 1910 ]; then
    $MAKE clean
    $MAKE CC="mips64el-linux-gnuabi64-gcc -static" bin/sljit_test
    qemu-mips64el-static bin/sljit_test -s

    $MAKE clean
    $MAKE CC="mips-linux-gnu-gcc -static" bin/sljit_test
    qemu-mips-static bin/sljit_test -s
  fi

  $MAKE clean
  $MAKE CC="sparc64-linux-gnu-gcc -m32 -static" bin/sljit_test
  qemu-sparc32plus-static bin/sljit_test -s

  $MAKE clean
  $MAKE CC="powerpc64le-linux-gnu-gcc -static" bin/sljit_test
  qemu-ppc64le-static bin/sljit_test -s

  $MAKE clean
  $MAKE CC="powerpc-linux-gnu-gcc -static" bin/sljit_test
  qemu-ppc-static bin/sljit_test -s
fi
