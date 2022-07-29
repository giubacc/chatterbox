#!/bin/bash

basedir=$(realpath ${BASE_DIR:-"../"})
ninja_jobs=${NINJA_JOBS:-"6"}

usage() {
  cat << EOF
usage: $0 CMD

commands
  build             Build chatterbox.
  clean             Clean chatterbox build directory.
  build_all         Build chatterbox with all its dependencies.
  clean_all         Clean all build directories (chatterbox and dependencies).
  build_v8          Build V8 JavaScript and WebAssembly engine.
  clean_v8          Clean V8 JavaScript and WebAssembly engine.
  help              This message.

env variables
  BASE_DIR          Specifies the directory containing src (default: ../).
  NINJA_JOBS        Specifies the number of parallel ninja jobs.
EOF
}

error() {
  echo "error: $*" >/dev/stderr
}

build() {
  echo "Building chatterbox ..."
  mkdir -p $basedir/build && cd $basedir/build && cmake .. && make
}

clean() {
  echo "Cleaning chatterbox build ..."
  rm -rf $basedir/build
}

# see this issue: https://bugs.chromium.org/p/v8/issues/detail?id=13455
patch_v8_code() {
  sed -i 's/std::is_pod/std::is_standard_layout/g' $basedir/src/contrib/v8/src/base/vector.h
}

build_v8() {
  echo "Building V8 ..."
  PATH=$basedir/src/contrib/depot_tools:$PATH
  gclient
  cd $basedir/src/contrib
  fetch v8
  cd v8
  args=$(cat <<EOF
dcheck_always_on = false
is_component_build = false
is_debug = false
target_cpu = "x64"
use_custom_libcxx = false
v8_enable_sandbox = true
v8_monolithic = true
v8_use_external_startup_data = false
EOF
)
  patch_v8_code

  gn gen out/x86.release --args="${args}"
  ninja -j$ninja_jobs -C out/x86.release v8_monolith
}

clean_v8() {
  echo "Cleaning v8 build ..."
  rm -rf $basedir/src/contrib/v8/out
}

build_all() {
  echo "Building cryptopp ..."
  cd $basedir/src/contrib/cryptopp && make -f GNUmakefile

  echo "Building restclient-cpp ..."
  cd $basedir/src/contrib/restclient-cpp && ./autogen.sh && ./configure && make

  echo "Building jsoncpp-build ..."
  mkdir -p $basedir/src/contrib/jsoncpp-build && cd $basedir/src/contrib/jsoncpp-build && cmake ../jsoncpp && make

  build_v8
  build
}

clean_all() {
  echo "Cleaning cryptopp ..."
  cd $basedir/src/contrib/cryptopp && make -f GNUmakefile clean

  echo "Cleaning restclient-cpp ..."
  cd $basedir/src/contrib/restclient-cpp && make clean

  echo "Cleaning jsoncpp-build ..."
  rm -rf $basedir/src/contrib/jsoncpp-build

  clean_v8
  clean
}

cmd="${1}"
shift 1

[[ -z "${cmd}" ]] && \
  usage && exit 1

if [[ "${cmd}" == "help" ]]; then
  usage
  exit 0
fi

echo "Invoked with basedir: ${basedir}"
echo

case ${cmd} in
  build)
    build || exit 1
    ;;
  clean)
    clean || exit 1
    ;;
  build_all)
    build_all || exit 1
    ;;
  clean_all)
    clean_all || exit 1
    ;;
  build_v8)
    build_v8 || exit 1
    ;;
  clean_v8)
    clean_v8 || exit 1
    ;;
  *)
    error "unknown command '${cmd}'"
    exit 1
    ;;
esac
