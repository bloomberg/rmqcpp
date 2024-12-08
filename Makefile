BUILD_FOLDER?=./build
SRC_FOLDER?=.

export VCPKG_ROOT?=${HOME}/vcpkg

CMAKE_PRESET?=macos-arm64-vcpkg

RMQ_HOSTNAME?=localhost
RMQ_MGMT?="http://localhost:15672"


default: build unit

init:
	cmake --fresh  --preset ${CMAKE_PRESET} -B ${BUILD_FOLDER} -S ${SRC_FOLDER} -G Ninja

build: 
	cmake --build ${BUILD_FOLDER} 

unit: 
	ctest --test-dir $(BUILD_FOLDER) --output-on-failure

full: init build 

full-test: full unit integration

integration: 
	RMQ_USER=guest RMQ_PWD=guest RMQ_USER_MGMT="guest" RMQ_PWD_MGMT="guest" RMQ_PORT="5672" RMQ_TLS_PORT="5671" \
	cmake --build $(BUILD_FOLDER) -t test_integration 

clean:
	rm -rf $(BUILD_FOLDER)

docker-setup:
	cd $(SRC_FOLDER)/dockerfiles && docker compose build --pull 

docker-build:
	cd $(SRC_FOLDER)/dockerfiles && docker compose run dev make full

docker-test:
	cd $(SRC_FOLDER)/dockerfiles && docker compose run dev make full-test

docker-shell:
	cd $(SRC_FOLDER)/dockerfiles && docker compose run dev

.PHONY: build init unit integration clean full full-test
