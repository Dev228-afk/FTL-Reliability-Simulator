.PHONY: all build test fault-regression clean

BUILD_DIR ?= build
CMAKE ?= cmake
CTEST ?= ctest

all: build

build:
	$(CMAKE) -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=RelWithDebInfo
	$(CMAKE) --build $(BUILD_DIR) -j

test: build
	cd $(BUILD_DIR) && $(CTEST) --output-on-failure

fault-regression: build
	cd $(BUILD_DIR) && $(CTEST) -R SeedRegressionTest --output-on-failure

clean:
	rm -rf $(BUILD_DIR)
