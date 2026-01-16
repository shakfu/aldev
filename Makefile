.DEFAULT_GOAL := all
.PHONY: all build configure library alda editor repl clean test show-config \
		csound configure-csound

BUILD_DIR ?= build
CMAKE ?= cmake

all: build

configure:
	@mkdir -p $(BUILD_DIR) && $(CMAKE) -S . -B $(BUILD_DIR) -DBUILD_TESTING=ON

configure-csound:
	@mkdir -p $(BUILD_DIR) && $(CMAKE) -S . -B $(BUILD_DIR) -DBUILD_CSOUND_BACKEND=ON -DBUILD_TESTING=ON

build: configure
	@$(CMAKE) --build $(BUILD_DIR) --config Release

# Build with Csound synthesis backend
csound: configure-csound
	@$(CMAKE) --build $(BUILD_DIR) --config Release

library: configure
	@$(CMAKE) --build $(BUILD_DIR) --target libloki --config Release

# Primary target: unified psnd binary
psnd: configure
	@$(CMAKE) --build $(BUILD_DIR) --target psnd_bin --config Release

show-config: configure
	@$(CMAKE) --build $(BUILD_DIR) --target show-config --config Release

test: build
	@$(CMAKE) -E chdir $(BUILD_DIR) ctest --output-on-failure

clean:
	@$(CMAKE) -E rm -rf $(BUILD_DIR)
