FROM debian:stable


RUN apt-get update && apt-get -y install \
                        ninja-build \
                        gdb \
                        cmake \
                        git \
                        netcat-traditional \
                        net-tools \
                        valgrind \
                        libboost-dev \
                        libssl-dev \
                        clang-format \
                        python3 \
                        python3-venv \
                        python3-pip  \
                        curl \
                        tar \
                        zip \
                        unzip \
                        sudo \
                        build-essential

# clone and install vcpkg
WORKDIR /build/
RUN git clone https://github.com/Microsoft/vcpkg.git

ENV VCPKG_FORCE_SYSTEM_BINARIES=1
RUN ./vcpkg/bootstrap-vcpkg.sh


WORKDIR /workarea
