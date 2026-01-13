.DEFAULT_GOAL := all
.PHONY: all build configure library alda editor repl clean test show-config

BUILD_DIR ?= build
CMAKE ?= cmake

all: build

configure:
	@mkdir -p build && $(CMAKE) -S . -B $(BUILD_DIR)

build: configure
	@$(CMAKE) --build $(BUILD_DIR) --config Release

library: configure
	@$(CMAKE) --build $(BUILD_DIR) --target libloki --config Release

# Primary target: unified aldalog binary
aldalog: configure
	@$(CMAKE) --build $(BUILD_DIR) --target alda_bin --config Release

# Compatibility aliases
editor: aldalog
repl: aldalog
loki: aldalog
alda: aldalog

show-config: configure
	@$(CMAKE) --build $(BUILD_DIR) --target show-config --config Release

test: aldalog
	@$(CMAKE) -E chdir $(BUILD_DIR) ctest --output-on-failure

clean:
	@$(CMAKE) -E rm -rf $(BUILD_DIR)
