BUILD_FOLDER ?= ./build
SRC_FOLDER ?= .
VCPKG_ROOT ?=/build/vcpkg

RMQ_HOSTNAME?=localhost

default: setup build


setup:
	mkdir -p $(BUILD_FOLDER)

cmake: setup
	cmake --preset ${CMAKE_PRESET} -B ${BUILD_FOLDER} -S ${SRC_FOLDER}

build:  cmake
	cmake --build ${BUILD_FOLDER} -j4

unit: build
	ctest --test-dir $(BUILD_FOLDER) 

integration: 
	RMQ_USER=guest RMQ_PWD=guest RMQ_USER_MGMT="guest" RMQ_PWD_MGMT="guest" RMQ_HOSTNAME="localhost" RMQ_PORT="5672" RMQ_TLS_PORT="5671" RMQ_MGMT="http://$(RMQ_HOSTNAME):15672" \
	cmake --build $(BUILD_FOLDER) -t test_integration  -j4
