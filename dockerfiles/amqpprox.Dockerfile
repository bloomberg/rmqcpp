FROM conanio/clang9 AS amqpprox_build_environment
WORKDIR /source

RUN sudo apt-get update && sudo apt-get install -y \
    git \
    llvm \
    make \
    socat \
    && rm -rf /var/lib/apt/lists/*

RUN git clone https://github.com/bloomberg/amqpprox.git /source

ENV BUILDDIR=/source/build
ENV CONAN_USER_HOME=/source/build 

RUN make setup && make init && make

EXPOSE 5700 5672 5671 

ENV AMQPPROX_DIR=/source/build
COPY start_proxy.sh /source/start_proxy.sh

CMD /source/start_proxy.sh
