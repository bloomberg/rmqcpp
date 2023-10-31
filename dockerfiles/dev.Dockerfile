FROM debian:stable

RUN apt-get update && apt-get install -y \
    build-essential \
    clang-format \
    cmake \
    curl \
    gcc \ 
    gdb \
    git \
    libboost-dev \
    libssl-dev \
    net-tools \
    netcat-traditional \
    ninja-build \
    pkg-config \
    python3 \
    python3-pip  \
    python3-venv \
    sudo \
    tar \
    unzip \
    valgrind \
    zip \
    && rm -rf /var/lib/apt/lists/*

ENV VCPKG_FORCE_SYSTEM_BINARIES=1 

# clone and install vcpkg
RUN git clone https://github.com/Microsoft/vcpkg.git /build/vcpkg && \
    /build/vcpkg/bootstrap-vcpkg.sh

ENV VCPKG_ROOT=/build/vcpkg

WORKDIR /workarea
