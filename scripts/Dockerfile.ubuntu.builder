FROM ubuntu:22.04 AS builder
LABEL Name=chatterbox-ubuntu-builder

RUN apt-get update && apt-get -y install --no-install-recommends \
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
    libtool \
    libssl-dev \
    libcurl4-openssl-dev \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

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
