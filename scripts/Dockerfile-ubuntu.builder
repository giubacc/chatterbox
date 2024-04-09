FROM ubuntu:22.04
LABEL Name=chatterbox-builder

RUN apt-get update && apt-get -y install \
    build-essential \
    ninja-build \
    wget \
    make \
    cmake \
    git \
    python3 \
    python3-venv \
    libglib2.0-dev \
    automake \
    autogen \
    libssl-dev \
    libcurl4-openssl-dev \
    && apt-get autoremove

RUN mkdir -p /project/contrib
ENV CONTRIB_PATH /project/contrib
ENV TAR_OPTIONS --no-same-owner

COPY contrib_init.sh /usr/bin/contrib_init.sh
COPY rapidyaml-build /project/contrib/rapidyaml-build
COPY build.sh /usr/bin/build.sh

RUN contrib_init.sh && build.sh build-deps \
&& find /contrib -name "*.o" -type f -delete \
&& find /contrib -name "*.so" -type f -delete \
&& find /contrib -name "*.exe" -type f -delete \
&& cp -R --parents /contrib/rapidyaml-build /tmp \
&& cp -R --parents /contrib/pistache/include /tmp \
&& cp -R --parents /contrib/pistache/build/src /tmp \
&& cp -R --parents /contrib/restclient-cpp/include /tmp \
&& cp -R --parents /contrib/cryptopp /tmp \
&& cp -R --parents /contrib/spdlog/include /tmp \
&& cp -R --parents /contrib/clipp/include /tmp \
&& cp -R --parents /contrib/v8/include /tmp \
&& cp -R --parents /contrib/googletest/googletest/include /tmp \
&& cp -R --parents /contrib/restclient-cpp/.libs /tmp \
&& cp -R --parents /contrib/v8/out/x64.release/obj /tmp \
&& cp -R --parents /contrib/googletest/build/lib /tmp \
&& rm -rf /contrib \
&& mv /tmp/contrib /contrib

ENTRYPOINT ["build.sh"]
CMD ["build"]
