#!/bin/bash

container_mng=${CNT_MNG:-"podman"}
basedir=$(realpath ${BASE_DIR:-"../"})
ninja_jobs=${NINJA_JOBS:-"6"}
builder_image=${BUILDER_IMAGE:-"chatterbox-builder"}
contrib_path=$(realpath ${CONTRIB_PATH:-"../contrib"})

usage() {
  cat << EOF
usage: $0 CMD

commands
  build                     Build chatterbox binary.
  build-test                Build chatterbox-test binary.
  clean                     Clean chatterbox build directory.
  build-all                 Build chatterbox binary with all its dependencies.
  clean-all                 Clean all build directories (chatterbox and dependencies).
  build-deps                Build chatterbox's dependencies.
  clean-deps                Clean chatterbox's dependencies.
  build-v8                  Build V8 JavaScript and WebAssembly engine.
  clean-v8                  Clean V8 JavaScript and WebAssembly engine.
  builder-create            Create the chatterbox builder image.
  builder-build             Build chatterbox binary with the chatterbox builder.
  builder-build-test        Build chatterbox-test binary with the chatterbox builder.
  chatterbox-create         Create the chatterbox image.
  chatterbox-test-create    Create the chatterbox-test image.
  help                      This message.

env variables
  CNT_MNG             Specify the container manager.
  BASE_DIR            Specify the directory containing src (default: ../).
  NINJA_JOBS          Specify the number of parallel ninja jobs.
  BUILDER_IMAGE       Specify the builder's image to use.
  CONTRIB_PATH        Specify the path where contrib resources are placed.
EOF
}

error() {
  echo "error: $*" >/dev/stderr
}

create_builder() {
  echo "Creating the chatterbox builder image ..."
  $container_mng build -t chatterbox-builder \
    -f Dockerfile.builder . || exit 1
}

builder_build() {
  echo "Building chatterbox binary with the chatterbox builder ..."
  volumes="-v $basedir:/project"

  $container_mng run -it \
    -e BASE_DIR=/project \
    -e NINJA_JOBS=${ninja_jobs} \
    -e CONTRIB_PATH=/contrib \
    ${volumes[@]} \
    ${builder_image} build || exit 1
}

builder_build_test() {
  echo "Building chatterbox-test binary with the chatterbox builder ..."
  volumes="-v $basedir:/project"

  $container_mng run -it \
    -e BASE_DIR=/project \
    -e NINJA_JOBS=${ninja_jobs} \
    -e CONTRIB_PATH=/contrib \
    ${volumes[@]} \
    ${builder_image} build-test || exit 1
}

create_chatterbox() {
  echo "Creating the chatterbox image ..."
  $container_mng build -t chatterbox \
    -f Dockerfile.chatterbox ${basedir}/build/chatterbox || exit 1
}

create_chatterbox_test() {
  echo "Creating the chatterbox-test image ..."
  cp -R $basedir/test/scenarios $basedir/build/test
  $container_mng build -t chatterbox-test \
    -f Dockerfile.chatterbox-test ${basedir}/build/test || exit 1
}

build() {
  echo "Building chatterbox binary ..."
  mkdir -p $basedir/build && cd $basedir/build && cmake -DCONTRIB_PATH=$contrib_path .. && make chatterbox
}

build_test() {
  echo "Building chatterbox-test binary ..."
  mkdir -p $basedir/build && cd $basedir/build && cmake -DCONTRIB_PATH=$contrib_path .. && make chatterbox_test
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

build_googletest() {
  echo "Building googletest ..."
  mkdir -p $contrib_path/googletest/build && cd $contrib_path/googletest/build && cmake .. -DBUILD_GMOCK=OFF && make
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

clean_googletest() {
  echo "Cleaning googletest ..."
  rm -rf $contrib_path/googletest/build
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
  ninja -j$ninja_jobs -C out/x86.release v8_monolith || exit 1
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
  build_googletest
}

clean_deps() {
  clean_cryptopp
  clean_restclient_cpp
  clean_jsoncpp
  clean_v8
  clean_googletest
}

build_all() {
  build_deps
  build
  build_test
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
  build)
    build || exit 1
    ;;
  build-test)
    build_test || exit 1
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
  builder-create)
    create_builder || exit 1
    ;;
  builder-build)
    builder_build || exit 1
    ;;
  builder-build-test)
    builder_build_test || exit 1
    ;;
  chatterbox-create)
    create_chatterbox || exit 1
    ;;
  chatterbox-test-create)
    create_chatterbox_test || exit 1
    ;;
  *)
    error "unknown command '${cmd}'"
    exit 1
    ;;
esac
