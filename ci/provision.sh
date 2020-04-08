#!/bin/sh

# IMPORTANT NOTE
# assumes build host is linux 64bit and either ubuntu >= 16.04, debian > 8
# fedora/centos/redhat are minimally supported by allowing multilib
# validation, the rest Linux should work the same as far as they have
# 32-bit libraries/headers installed when
# alpine is enabled for multiple compiler tests
# other Unix just added for fun

set -e

usage() {
  echo "usage: $0 [--help] [windows]"
  echo
  echo "windows: installs wine (EXPENSIVE)"
  exit 1
}

if [ $# -gt 0 ]; then
  if [ $# -gt 1 ] || [ "$1" != "windows" ]; then
    usage
  else
    WINE=1
  fi
fi

OS=$(uname -s)

case $OS in
  SunOS)
    pkg install git
    pkg install gnu-make
    ;;
  OpenBSD)
    pkg_add -r git
    pkg_add -r gmake
    ;;
  NetBSD)
    pkgin install git
    pkgin install gmake
    ;;
  Dragonfly)
    pkg install -y gmake
    ;;
  FreeBSD)
    pkg install -y git
    pkg install -y gmake
    ;;
  Darwin)
    ;;
  Linux)
    if [ -e /etc/os-release ]; then
      . /etc/os-release
    else
      ID=unknown
    fi

    ARCH=$(uname -m)
    if [ $? -eq 0 ] && [ "$ARCH" != "x86_64" ]; then
      if [ "$ID" != "alpine" ]; then
        exit 0
      fi
    fi

    USERID=$(id -u)
    if [ $? -eq 0 ] && [ $USERID -gt 0 ]; then
      type sudo >/dev/null
      if [ $? -eq 0 ]; then
        SUDO=sudo
      else
        echo "error: need root privileges"
        exit 1
      fi
    fi

    case $ID in
      debian|ubuntu)
        $SUDO apt-get update
        $SUDO apt-get install -y gcc make
        $SUDO apt-get install -y qemu-user-static
        $SUDO apt-get install -y gcc-aarch64-linux-gnu gcc-arm-linux-gnueabihf
        $SUDO apt-get install -y gcc-multilib-sparc64-linux-gnu
        if [ "$VERSION_ID" != "19.10" ]; then
          # ubuntu 19.10 broke mips crosscompilers, 20.04 fixed it
          $SUDO apt-get install -y gcc-mips-linux-gnu gcc-mips64el-linux-gnuabi64
        fi
        $SUDO apt-get install -y gcc-powerpc-linux-gnu gcc-powerpc64le-linux-gnu

        if [ ! -z "$WINE" ]; then
          $SUDO apt-get install -y gcc-mingw-w64-i686
          $SUDO dpkg --add-architecture i386
          $SUDO apt-get update
          DEBIAN_FRONTEND=noninteractive
          export DEBIAN_FRONTEND
          if [ "$VERSION_ID" = "16.04" ]; then
            $SUDO apt-get install -yq wine
          else
            $SUDO apt-get install -yq wine32
          fi
        fi
        ;;
      redhat|centos|ol)
        $SUDO yum -y update
        $SUDO yum -y install gcc make
        $SUDO yum -y install glibc-devel.i686
        ;;
      fedora)
        $SUDO dnf -y update
        $SUDO dnf -y install gcc make
        $SUDO dnf -y install glibc-devel.i686
        ;;
      alpine)
        $SUDO apk add bash
        $SUDO apk add make musl-dev
        $SUDO apk add gcc pcc clang
        ;;
      *)
        exit 1
        ;;
    esac
    ;;
  *)
     exit 1
esac
