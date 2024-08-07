#!/bin/bash

contrib_path=$(realpath ${CONTRIB_PATH:-"../contrib"})

mkdir -p $contrib_path

echo "Cloning contrib resources ..."

cd $contrib_path
git clone --recursive https://github.com/biojppm/rapidyaml
git clone https://github.com/pistacheio/pistache.git
git clone https://github.com/weidai11/cryptopp.git
git clone https://github.com/gabime/spdlog.git
git clone https://github.com/muellan/clipp.git
git clone https://github.com/giubacc/restclient-cpp.git
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
git clone https://github.com/google/googletest.git

echo "Bumping contrib resources ..."

cd $contrib_path/clipp && git checkout v1.2.3
cd $contrib_path/rapidyaml && git checkout 7c0036e7c41318ec75368f9a2b2148c31b3121db
cd $contrib_path/pistache && git checkout fe5639a81a4392a7b8ab22c29a3d5c761e8e98d6
cd $contrib_path/restclient-cpp && git checkout set-verify-host
cd $contrib_path/spdlog && git checkout v1.9.2
cd $contrib_path/cryptopp && git checkout CRYPTOPP_8_7_0
cd $contrib_path/googletest && git checkout release-1.12.1
