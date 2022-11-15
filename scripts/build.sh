#!/bin/bash

basedir=$(realpath ${BASE_DIR:-"../"})
ninja_jobs=${NINJA_JOBS:-"6"}
base_build_image=${BASE_BUILD_IMAGE:-"opensuse"}
contrib_path=$(realpath ${CONTRIB_PATH:-"../contrib"})

usage() {
  cat << EOF
usage: $0 CMD

commands
  build                     Build chatterbox.
  clean                     Clean chatterbox build directory.
  build-all                 Build chatterbox with all its dependencies.
  clean-all                 Clean all build directories (chatterbox and dependencies).
  build-deps                Build chatterbox's dependencies.
  clean-deps                Clean chatterbox's dependencies.
  build-v8                  Build V8 JavaScript and WebAssembly engine.
  clean-v8                  Clean V8 JavaScript and WebAssembly engine.
  builder-create            Create the chatterbox builder.
  builder-build             Build chatterbox with the chatterbox builder.
  help                      This message.

env variables
  BASE_DIR            Specifies the directory containing src (default: ../).
  NINJA_JOBS          Specifies the number of parallel ninja jobs.
  BASE_BUILD_IMAGE    Specifies the base image for the Docker chatterbox build image.
  CONTRIB_PATH        Specifies the path where contrib resources are placed.
EOF
}

error() {
  echo "error: $*" >/dev/stderr
}

create_builder() {
  echo "Creating the chatterbox builder ..."
  podman build -t chatterbox-builder-${base_build_image} \
    -f Dockerfile.build-${base_build_image} . || exit 1
}

builder_build() {
  echo "Building chatterbox with the chatterbox builder ..."
  volumes="-v $basedir:/project"

  podman run -it \
    -e BASE_DIR=/project \
    -e NINJA_JOBS=${ninja_jobs} \
    -e CONTRIB_PATH="/contrib" \
    ${volumes[@]} \
    chatterbox-builder-${base_build_image} build || exit 1
}

build() {
  echo "Building chatterbox ..."
  if [[ -n "${contrib_path}" ]]; then
    echo ${contrib_path}
  fi
  mkdir -p $basedir/build && cd $basedir/build && cmake .. && make
}

clean() {
  echo "Cleaning chatterbox build ..."
  rm -rf $basedir/build
}

build_cryptopp() {
  echo "Building cryptopp ..."
  cd $contrib_path/cryptopp && make -f GNUmakefile
}

build_restclient_cpp() {
  echo "Building restclient-cpp ..."
  cd $contrib_path/restclient-cpp && ./autogen.sh && ./configure && make
}

build_jsoncpp() {
  echo "Building jsoncpp-build ..."
  mkdir -p $contrib_path/jsoncpp-build && cd $contrib_path/jsoncpp-build && cmake ../jsoncpp && make
}

clean_cryptopp() {
  echo "Cleaning cryptopp ..."
  cd $contrib_path/cryptopp && make -f GNUmakefile clean
}

clean_restclient_cpp() {
  echo "Cleaning restclient-cpp ..."
  cd $contrib_path/restclient-cpp && make clean
}

clean_jsoncpp() {
  echo "Cleaning jsoncpp-build ..."
  rm -rf $contrib_path/jsoncpp-build
}

# see this issue: https://bugs.chromium.org/p/v8/issues/detail?id=13455
patch_v8_code() {
  sed -i 's/std::is_pod/std::is_standard_layout/g' $basedir/contrib/v8/src/base/vector.h
}

build_v8() {
  echo "Building V8 ..."
  PATH=$contrib_path/depot_tools:$PATH
  gclient
  cd $contrib_path
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
  rm -rf $contrib_path/v8/out
}

build_deps() {
  build_cryptopp
  build_restclient_cpp
  build_jsoncpp
  build_v8
}

clean_deps() {
  clean_cryptopp
  clean_restclient_cpp
  clean_jsoncpp
  clean_v8
}

build_all() {
  build_deps
  build
}

clean_all() {
  clean_deps
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
  builder-create)
    create_builder || exit 1
    ;;
  builder-build)
    builder_build || exit 1
    ;;
  build)
    build || exit 1
    ;;
  clean)
    clean || exit 1
    ;;
  build-all)
    build_all || exit 1
    ;;
  clean-all)
    clean_all || exit 1
    ;;
  build-deps)
    build_deps || exit 1
    ;;
  clean-deps)
    clean_deps || exit 1
    ;;
  build-v8)
    build_v8 || exit 1
    ;;
  clean-v8)
    clean_v8 || exit 1
    ;;
  *)
    error "unknown command '${cmd}'"
    exit 1
    ;;
esac
