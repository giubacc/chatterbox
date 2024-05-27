FROM opensuse/leap:15.5
LABEL Name=chatterbox-leap-builder

RUN zypper -n install --no-recommends \
    xz \
    awk \
    wget \
    find \
    make \
    cmake \
    git \
    ccache \
    ninja \
    binutils \
    python311 \
    gcc12-c++ \
    automake \
    autogen \
    libtool \
    libcurl-devel \
    libopenssl-devel \
    && zypper clean --all

RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-12 12 \
&& update-alternatives --install /usr/bin/cc cc /usr/bin/gcc-12 12 \
&& update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-12 12 \
&& update-alternatives --install /usr/bin/c++ c++ /usr/bin/g++-12 12 \
&& update-alternatives --install /usr/bin/python3 python /usr/bin/python3.11 311

RUN mkdir -p /project/contrib && mkdir -p /project/chatterbox
ENV CONTRIB_PATH=/project/contrib
ENV TAR_OPTIONS=--no-same-owner

COPY contrib_init.sh /usr/bin/contrib_init.sh
COPY rapidyaml-build /project/contrib/rapidyaml-build
COPY build.sh /usr/bin/build.sh

RUN contrib_init.sh && build.sh build-deps \
&& find /project/contrib -name "*.o" -type f -delete \
&& find /project/contrib -name "*.so" -type f -delete \
&& find /project/contrib -name "*.exe" -type f -delete \
&& cp -R --parents /project/contrib/rapidyaml-build /tmp \
&& cp -R --parents /project/contrib/pistache/include /tmp \
&& cp -R --parents /project/contrib/pistache/build/src /tmp \
&& cp -R --parents /project/contrib/restclient-cpp/include /tmp \
&& cp -R --parents /project/contrib/cryptopp /tmp \
&& cp -R --parents /project/contrib/spdlog/include /tmp \
&& cp -R --parents /project/contrib/clipp/include /tmp \
&& cp -R --parents /project/contrib/v8/include /tmp \
&& cp -R --parents /project/contrib/googletest/googletest/include /tmp \
&& cp -R --parents /project/contrib/restclient-cpp/.libs /tmp \
&& cp -R --parents /project/contrib/v8/out/x64.release/obj /tmp \
&& cp -R --parents /project/contrib/googletest/build/lib /tmp \
&& rm -rf /project/contrib \
&& mv /tmp/project/contrib /project/contrib

ENTRYPOINT ["build.sh"]
CMD ["build"]
