.DEFAULT_GOAL := all
.PHONY: all build configure library alda editor repl clean reset test show-config \
		csound configure-csound web configure-web rebuild remake

BUILD_DIR ?= build
CMAKE ?= cmake

all: build

configure:
	@mkdir -p $(BUILD_DIR) && $(CMAKE) -S . -B $(BUILD_DIR) -DBUILD_TESTING=ON

configure-csound:
	@mkdir -p $(BUILD_DIR) && $(CMAKE) -S . -B $(BUILD_DIR) -DBUILD_CSOUND_BACKEND=ON -DBUILD_TESTING=ON

build: configure
	@$(CMAKE) --build $(BUILD_DIR) --config Release

rebuild: clean configure-csound test
	@$(CMAKE) --build $(BUILD_DIR) --config Release
	@$(CMAKE) -E chdir $(BUILD_DIR) ctest --output-on-failure

# Build with Csound synthesis backend
csound: configure-csound
	@$(CMAKE) --build $(BUILD_DIR) --config Release

configure-web:
	@mkdir -p $(BUILD_DIR) && $(CMAKE) -S . -B $(BUILD_DIR) -DBUILD_WEB_HOST=ON -DBUILD_TESTING=ON

# Build with web server host (mongoose-based HTTP/WebSocket)
web: configure-web
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
	@$(CMAKE) --build $(BUILD_DIR) --target clean 2>/dev/null || true

reset:
	@$(CMAKE) -E rm -rf $(BUILD_DIR)

remake: reset build
