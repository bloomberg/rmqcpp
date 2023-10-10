FROM artprod.dev.bloomberg.com/rhel7-dpkg:latest

WORKDIR /tmp

RUN apt-get update && apt-get install -y amqpprox socat

#  The opensource/raw version needs to be compiled so might require installs of build tools like:
#     build-essential \
#     cmake \
#     make \
#     wget \
#     python3.8 \
#     python3.8-pip

# RUN python3.8 -m pip install pyyaml distro conan

# RUN yum install -y devtoolset-10 gcc-toolset-10

# RUN wget --header 'Authorization: token 92923612f247b08efb9213ed19fadab399df6c26' https://bbgithub.dev.bloomberg.com/queuing/amqpprox/archive/refs/tags/1.5.0.tar.gz && \
# OR
# RUN wget https://github.com/bloomberg/amqpprox/archive/refs/tags/v1.0.0.tar.gz && \
#
# then use the appropriate tarball/paths
#     tar -vxzf ./1.5.0.tar.gz && \
#     cd amqpprox-1.5.0 && \
#     conan profile detect && \
#     make setup && \
#     make init && \
#     make

# but, since the required boost and other build tools connan tries to fetch are external to bb we can't really do this now
