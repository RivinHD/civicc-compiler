.DEFAULT_GOAL := debug
.PHONY: help debug release dist clean jobs

help:
	@echo "Targets:"
	@echo "  debug  : Generate build artifacts for a debug build in build-debug"
	@echo "  release: Generate build artifacts for a release build in build-release"
	@echo "  dist   : Pack civicc and coconut into a tar.gz file. Use this for creating a submission"
	@echo "  clean  : Remove all build directories and created dist files"


# Fallback to number of CPUs
DEFAULT_JOBS := $(shell (command -v nproc >/dev/null 2>&1 && nproc) || (uname | grep -i Darwin >/dev/null 2>&1 && sysctl -n hw.ncpu) || echo 1)

# Try to extract -j value from MAKEFLAGS (handles both '-j4' and '-j 4')
JOBS_FROM_MAKE := $(shell echo $(MAKEFLAGS) | sed -n 's/.*-j[[:space:]]*\([0-9][0-9]*\).*/\1/p')

ifeq ($(JOBS_FROM_MAKE),)
  # No -j in MAKEFLAGS: use JOBS if provided, otherwise use detected default
  JOBS ?= $(DEFAULT_JOBS)
else
  # If -j was given to make, use that value to drive cmake builds.
  # Use override so '-j' wins over a command-line JOBS=... if present, per request.
  override JOBS := $(JOBS_FROM_MAKE)
endif

jobs:
	@echo "Running with $(JOBS) jobs!"

# Change to @cmake -DDISABLE_ASAN=true -DCMAKE_BUILD_TYPE=... to disable address
# sanitizer
debug: jobs
	@cmake -DCMAKE_BUILD_TYPE=Debug -S ./ -B build/ && cmake --build build -j $(JOBS)

release: jobs
	@cmake -DCMAKE_BUILD_TYPE=Release -S ./ -B build/ && cmake --build build -j $(JOBS)


dist:
	bash scripts/dist.sh

clean:
	rm civicc.tar.gz
	rm -rf build/
