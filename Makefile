BUILD_FOLDER?=./build
SRC_FOLDER?=.

export VCPKG_ROOT?=${HOME}/vcpkg

CMAKE_PRESET?=macos-arm64-vcpkg

RMQ_HOSTNAME?=localhost
RMQ_MGMT?="http://localhost:15672"

IMPOSTER_FOLDER?=$(SRC_FOLDER)/src/tests/amqp_imposter
IMPOSTER_VENV?=$(BUILD_FOLDER)/imposter-venv


default: build unit

init:
	cmake --fresh  --preset ${CMAKE_PRESET} -B ${BUILD_FOLDER} -S ${SRC_FOLDER} -G Ninja

build: 
	cmake --build ${BUILD_FOLDER} 

unit: 
	ctest --test-dir $(BUILD_FOLDER) --output-on-failure

full: init build 

full-test: full unit integration imposter-test

integration: 
	RMQ_USER=guest RMQ_PWD=guest RMQ_USER_MGMT="guest" RMQ_PWD_MGMT="guest" RMQ_PORT="5672" RMQ_TLS_PORT="5671" RMQ_PROXY_CONTROL_PORT="5700" \
	cmake --build $(BUILD_FOLDER) -t test_integration 

imposter-venv:
	uv venv --allow-existing --python 3.15 $(IMPOSTER_VENV)
	uv pip install --python $(IMPOSTER_VENV) -e $(IMPOSTER_FOLDER) -r $(IMPOSTER_FOLDER)/requirements-dev.txt

imposter-test: imposter-venv
	cd $(IMPOSTER_FOLDER) && $(abspath $(IMPOSTER_VENV))/bin/python -m pytest -n auto

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

.PHONY: build init unit integration clean full full-test imposter-venv imposter-test
