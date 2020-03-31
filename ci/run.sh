#!/bin/bash -e

usage() {
  echo "usage: $0 [--help] [windows]"
  echo
  echo "windows: use wine to test x86 32bit"
  exit 1
}

if [ $# -gt 0 ]; then
  if [ $# -gt 1 ] || [ "$1" != "windows" ]; then
    usage
  else
    WINE=1
  fi
fi

MAKE=${MAKE:-make}

if [ -d bin ]; then
  $MAKE clean
fi

if [ -e /etc/os-release ]; then
  source /etc/os-release
  VERSION_ID=${VERSION_ID/_*/} # non released version differences ignored
  VERSION_ID=${VERSION_ID/./} # 20.04 becomes 2004 and 3.0.8 becomes 308
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
  alpine)
    ;;
  *)
    exit 0
esac

$MAKE clean
if [ "$ID" = "alpine" ]; then
  # multiple 64-bit compiler
  for COMPILER in c89 clang pcc; do
    $MAKE CC=$COMPILER
    echo -n "$COMPILER: " && bin/sljit_test -s
    $MAKE clean
  done
elif [ "$ID" != "debian" ] && [ "$ID" != "ubuntu" ]; then
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

if [ ! -z "$WINE" ]; then
  $MAKE clean
  $MAKE CROSS_COMPILER=i686-w64-mingw32-gcc
  wine ./bin/sljit_test -s
fi
