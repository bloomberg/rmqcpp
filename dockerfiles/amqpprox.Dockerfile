FROM ubuntu:24.04 AS amqpprox_build_environment

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    cmake \
    ninja-build \
    build-essential \
    pkg-config \
    python3 \
    python3-pip \
    python3-venv \
    llvm \
    clang \
    git \
    make \
    socat \
    && rm -rf /var/lib/apt/lists/*

RUN python3 -m pip install --break-system-packages "conan>=2,<3"

WORKDIR /source

RUN git clone https://github.com/bloomberg/amqpprox.git /source

ENV BUILDDIR=/source/build
ENV CONAN_USER_HOME=/source/build
ENV CONAN_HOME=/source/build/.conan2

RUN make setup && make init && make

EXPOSE 5700 5672 5671

ENV AMQPPROX_DIR=/source/build
COPY start_proxy.sh /source/start_proxy.sh

CMD /source/start_proxy.sh
