BUILD_FOLDER ?= ./build
SRC_FOLDER ?= .
VCPKG ?=/build/vcpkg
CC =gcc
CXX ?=g++

default: setup build

setup:
	mkdir -p $(BUILD_FOLDER)

cmake: setup
	cmake -DCMAKE_INSTALL_LIBDIR=lib64 -DCMAKE_TOOLCHAIN_FILE=$(VCPKG)/scripts/buildsystems/vcpkg.cmake  -DCMAKE_C_COMPILER=$(CC) -DCMAKE_CXX_COMPILER=$(CXX) -DCMAKE_CXX_STANDARD=17 -DVCPKG_INSTALL_OPTIONS=--allow-unsupported  -B $(BUILD_FOLDER) -S $(SRC_FOLDER)

build:  cmake
	cmake --build $(BUILD_FOLDER) -j4

local-integration-broker:
	docker-compose -f src/tests/integration/docker-compose.yml up -d

# For local testing
local-rmqapitests: local-integration-broker setup-rmqapitests build
	RMQ_USER=rmqcpp RMQ_PWD=guest RMQ_USER_MGMT="guest" RMQ_PWD_MGMT="rmqcpp" RMQ_HOSTNAME="localhost" RMQ_PORT="5672" RMQ_TLS_PORT="5671" RMQ_MGMT="http://rabbit:15672" cd $(BUILD_FOLDER); ninja test_integration

run-rmqapitests: setup-rmqapitests
	. .venv/bin/activate && cd $(BUILD_FOLDER) && ninja test_integration

setup-rmqapitests:
	python3.8 -mvenv .venv
	. .venv/bin/activate && python -mpip install pytest requests 
