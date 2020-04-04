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
# throws illegal instruction from qemu in ubuntu 16.04
# fails test54 case 19 in ubuntu 19.x and debian 10 because of NaN bug
  if [[ ("$ID" = "ubuntu" && ($VERSION_ID -eq 1804 || $VERSION_ID -ge 2004)) ||
        ("$ID" = "debian" && $VERSION_ID -ne 10) ]]; then
    qemu-ppc64le-static bin/sljit_test -s
  else
    if [ "$ID" = "ubuntu" ] && [ $VERSION_ID -eq 1604 ]; then
      echo -e "SLJIT tests: all tests are \e[94mSKIPPED\e[0m because of bogus QEMU"
    else
      qemu-ppc64le-static bin/sljit_test -s > out || true
      cat out
      if grep -q FAILED out; then
        grep -q "test54 case 18 failed" out
        echo -e "known failure \e[94mIGNORED\e[0m because of QEMU NaN bug #1821444"
      fi
      rm -f out
    fi
  fi

  $MAKE clean
  $MAKE CC="powerpc-linux-gnu-gcc -static" bin/sljit_test
# fails test54 case 18, 19 in debian 10 and ubuntu 19.x
  if [[ ("$ID" = "debian" && $VERSION_ID -ne 10) ||
        ("$ID" = "ubuntu" &&
        ($VERSION_ID -eq 1604 || $VERSION_ID -eq 1804 || $VERSION_ID -ge 2004)) ]]
  then
    qemu-ppc-static bin/sljit_test -s
  else
    qemu-ppc-static bin/sljit_test -s > out || true
    cat out
    if grep -q FAILED out; then
      grep -q "test54 case 18 failed" out
      echo -e "known failure \e[94mIGNORED\e[0m because of QEMU NaN bug #1821444"
    fi
    rm -f out
  fi
fi

if [ ! -z "$WINE" ]; then
  $MAKE clean
  $MAKE CROSS_COMPILER=i686-w64-mingw32-gcc
  wine ./bin/sljit_test -s
fi
