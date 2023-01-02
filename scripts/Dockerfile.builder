FROM opensuse/tumbleweed
LABEL Name=chatterbox-builder

RUN zypper -n install --no-recommends \
    find \
    make \
    cmake \
    git \
    ccache \
    ninja \
    binutils \
    gcc-c++ \
    automake \
    autogen \
    libtool \
    libcurl-devel \
    libopenssl-devel \
    && zypper clean --all

COPY contrib_init.sh /usr/bin/contrib_init.sh
COPY build.sh /usr/bin/build.sh

ENV CONTRIB_PATH /contrib
ENV TAR_OPTIONS --no-same-owner

RUN contrib_init.sh && build.sh build-deps \
&& find /contrib -name "*.o" -type f -delete \
&& find /contrib -name "*.exe" -type f -delete \
&& cp -R --parents /contrib/jsoncpp/include /tmp \
&& cp -R --parents /contrib/restclient-cpp/include /tmp \
&& cp -R --parents /contrib/cryptopp /tmp \
&& cp -R --parents /contrib/spdlog/include /tmp \
&& cp -R --parents /contrib/clipp/include /tmp \
&& cp -R --parents /contrib/v8/include /tmp \
&& cp -R --parents /contrib/googletest/googletest/include /tmp \
&& cp -R --parents /contrib/restclient-cpp/.libs /tmp \
&& cp -R --parents /contrib/jsoncpp-build/lib /tmp \
&& cp -R --parents /contrib/v8/out/x86.release/obj /tmp \
&& cp -R --parents /contrib/googletest/build/lib /tmp \
&& rm -rf /contrib \
&& mv /tmp/contrib /contrib

ENTRYPOINT ["build.sh"]
CMD ["build"]
