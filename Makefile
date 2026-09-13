CXX ?= g++
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra -Wpedantic
LDFLAGS ?=

HOST_ARCH := $(shell getarch 2>/dev/null)
ifeq ($(HOST_ARCH),x86_gcc2)
ARCH_RUN := setarch x86
else
ARCH_RUN :=
endif

BUILD_DIR ?= build
PROBE := $(BUILD_DIR)/RChromiumNativeProbe
SOURCES := probe/main.cpp probe/SoftwareSurface.cpp

.PHONY: all run clean verify-no-qt

all: verify-no-qt $(PROBE)

$(PROBE): $(SOURCES) probe/SoftwareSurface.h
	mkdir -p $(BUILD_DIR)
	$(ARCH_RUN) $(CXX) $(CXXFLAGS) -o $@ $(SOURCES) $(LDFLAGS) -lbe

run: $(PROBE)
	$(ARCH_RUN) ./$(PROBE)

verify-no-qt:
	@if grep -R -n -E '#include[[:space:]]*<Q|Qt(WebEngine|Core|Gui|Widgets)|-lQt' \
		--include='*.cpp' --include='*.h' --include='Makefile' \
		probe chromium_overlay chromium87_overlay; then \
		echo 'Qt dependency found in native port'; exit 1; \
	fi

clean:
	rm -rf $(BUILD_DIR)
