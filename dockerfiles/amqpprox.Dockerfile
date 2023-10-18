FROM conanio/clang9 AS amqpprox_build_environment
WORKDIR /source

RUN sudo apt-get update && sudo apt-get install -y llvm git make socat 
RUN git clone https://github.com/bloomberg/amqpprox.git /source

ENV BUILDDIR=/source/build
ENV CONAN_USER_HOME=/source/build 

RUN make setup && make init && make

EXPOSE 5700 5672 5671 

ENV AMQPPROX_DIR=/source/build
COPY start_proxy.sh /source/start_proxy.sh

CMD /source/start_proxy.sh
