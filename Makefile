.DEFAULT_GOAL := debug
.PHONY: help
help:
	@echo "Targets:"
	@echo "  debug:  Generate build artifacts for a debug build."
	@echo "  release:  Generate build artifacts for a release build."
	@echo "  dist:  Pack civicc into a tar.gz file. Use this for creating a submission."
	@echo "  clean:  Remove all build directories and created dist files."
	@echo "  test:  Runs all test with ctest in parallel."
	@echo "  afl_build:  Build the targets and sanatized targets with the afl compiler."
	@echo "  check_tmpfs:  Check if the tmpfs directory is mounted to a mount of type tmpfs, only requiered for fuzzing with afl."
	@echo "  generate_seeds:  Generates seeds for the afl run, these will generate valid civicc programs."
	@echo "  afl_tooling:  Build the targets: check_tmpfs, afl_build, generate_seeds."
	@echo "  fuzz_civicc:  Fuzz the complete civcc compiler with afl, with correct and incorrect civicc programs."
	@echo "  fuzz_civicc_positive:  Fuzz the complete civcc compiler with afl, with only correct civicc programs."
	@echo "  fuzz_scanner:  Fuzz the scanner of the civcc compiler with afl, with correct and incorrect civicc programs."
	@echo "  fuzz_scanner_positive:  Fuzz the scanner of the civcc compiler with afl, with only correct civicc programs."


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

.PHONY: jobs
jobs:
	@echo "Running with $(JOBS) jobs!"

# Change to @cmake -DDISABLE_ASAN=true -DCMAKE_BUILD_TYPE=... to disable address
# sanitizer
.PHONY: debug
debug: jobs
	@cmake -DCMAKE_BUILD_TYPE=Debug -S ./ -B build/ && cmake --build build -j $(JOBS)

.PHONY: release
release: jobs
	@cmake -DCMAKE_BUILD_TYPE=Release -S ./ -B build/ && cmake --build build -j $(JOBS)

# We exclude the specified tests from the cocount framework
.PHONY: test
test: 
	@ctest --test-dir build --output-on-failure -E "^(GoodDSLfiles|BadDSLfiles)" -j $(JOBS)

# Everything we need for fuzzing with AFL++
# We build the sanitizer extra to use SAND (https://aflplus.plus/docs/sand/).
.PHONY: afl_build
afl_build:
	@cmake -DCMAKE_BUILD_TYPE=Debug -S ./ -B build-afl -DCMAKE_C_COMPILER=afl-cc -DCMAKE_CXX_COMPILER=afl-c++ && cmake --build build-afl -j $(JOBS)
	@cmake -E env AFL_USE_ASAN=1 AFL_SAN_NO_INST=1 -- cmake -DCMAKE_BUILD_TYPE=Debug -S ./ -B build-afl -DCMAKE_C_COMPILER=afl-cc -DCMAKE_CXX_COMPILER=afl-c++ && cmake --build build-afl -j $(JOBS)
	@cmake -E env AFL_USE_UBSAN=1 AFL_SAN_NO_INST=1 -- cmake -DCMAKE_BUILD_TYPE=Debug -S ./ -B build-afl -DCMAKE_C_COMPILER=afl-cc -DCMAKE_CXX_COMPILER=afl-c++ && cmake --build build-afl -j $(JOBS)
	@cmake -E env AFL_USE_MSAN=1 AFL_SAN_NO_INST=1 -- cmake -DCMAKE_BUILD_TYPE=Debug -S ./ -B build-afl -DCMAKE_C_COMPILER=afl-cc -DCMAKE_CXX_COMPILER=afl-c++ && cmake --build build-afl -j $(JOBS)
	@echo "Finished Building all requiered AFL targets"

# Directory to use for tmpfs for Fuzzing; override on the make command line:
#   make TMPFS_DIR=/your/dir 
TMPFS_DIR ?= /mnt/tmpfs
SEED_DIR ?= ./afl/seeds

.PHONY: check_tmpfs
check_tmpfs:
	@if [ ! -d "$(TMPFS_DIR)" ]; then \
	  echo "ERROR: $(TMPFS_DIR) does not exist."; exit 1; \
	fi
	@# Prefer /proc/mounts on Linux, fall back to mount output
	@if [ -f /proc/mounts ]; then \
	  grep -E "^[^ ]+ $(TMPFS_DIR) tmpfs " /proc/mounts >/dev/null 2>&1 || { echo "ERROR: $(TMPFS_DIR) is not mounted as tmpfs."; exit 1; }; \
	else \
	  mount | grep -E "tmpfs( on)? $(TMPFS_DIR)( |$)" >/dev/null 2>&1 || { echo "ERROR: $(TMPFS_DIR) is not mounted as tmpfs."; exit 1; }; \
	fi
	@echo "$(TMPFS_DIR) exists and is mounted as tmpfs."


.PHONY: generate_seeds
generate_seeds:
	@if [ "${SEED_DIR}" != "./afl/seeds" ]; then \
		echo "Seed generation skipped, using custom seed directory."; \
	elif [ ! -d "./afl/seeds" ]; then \
		mkdir -p afl && ./build-afl/grammar_generator-civicc 100 1000 ./afl/seeds ./afl/trees; \
		echo "Finished seed generation."; \
	else \
		echo "Seed generation skipped, already exists."; \
	fi

.PHONY: afl_tooling
afl_tooling: check_tmpfs afl_build generate_seeds
	@echo "Finished Building Tooling & Setup"

# Fuzz the complete compiler
# We use the exit code 1 for the positive space fuzzer which leverages the grammar
.PHONY: fuzz_civicc
fuzz_civicc: afl_tooling
	@mkdir -p afl/civicc/out/default
	@cp -r -u afl/trees afl/civicc/out/default
	AFL_TMPDIR="${TMPFS_DIR}" AFL_CUSTOM_MUTATOR_LIBRARY=./build-afl/libgrammarmutator-civicc.so afl-fuzz -i ./afl/seeds -o ./afl/civicc/out -w ./build-afl/civicc_asan -w ./build-afl/civicc_ubsan -w ./build-afl/civicc_msan -- ./build-afl/civicc @@

.PHONY: fuzz_civicc_positive
fuzz_civicc_positive: afl_tooling
	@mkdir -p afl/civicc/out/default
	@cp -r -u afl/trees afl/civicc/out/default
	AFL_TMPDIR="${TMPFS_DIR}" AFL_CRASH_EXITCODE='1' AFL_CUSTOM_MUTATOR_LIBRARY=./build-afl/libgrammarmutator-civicc.so AFL_CUSTOM_MUTATOR_ONLY=1 afl-fuzz -m 256 -i ./afl/seeds -o ./afl/civicc/out -w ./build-afl/civicc_asan -w ./build-afl/civicc_ubsan -w ./build-afl/civicc_msan -- ./build-afl/civicc @@

# Fuzz the scanner only
.PHONY: fuzz_scanner
fuzz_scanner: afl_tooling
	@mkdir -p afl/civicc_scanner/out/default
	@cp -r -u afl/trees afl/civicc_scanner/out/default
	AFL_TMPDIR="${TMPFS_DIR}" AFL_CUSTOM_MUTATOR_LIBRARY=./build-afl/libgrammarmutator-civicc.so afl-fuzz -i ./afl/seeds -o ./afl/civicc_scanner/out -w ./build-afl/civicc_scanner_asan -w ./build-afl/civicc_scanner_ubsan -w ./build-afl/civicc_scanner_msan -- ./build-afl/civicc_scanner @@

.PHONY: fuzz_scanner_positive
fuzz_scanner_positive: afl_tooling
	@mkdir -p afl/civicc_scanner/out/default
	@cp -r -u afl/trees afl/civicc_scanner/out/default
	AFL_TMPDIR="${TMPFS_DIR}" AFL_CRASH_EXITCODE='1' AFL_CUSTOM_MUTATOR_LIBRARY=./build-afl/libgrammarmutator-civicc.so AFL_CUSTOM_MUTATOR_ONLY=1 afl-fuzz -m 256 -i ./afl/seeds -o ./afl/civicc_scanner/out -w ./build-afl/civicc_scanner_asan -w ./build-afl/civicc_scanner_ubsan -w ./build-afl/civicc_scanner_msan -- ./build-afl/civicc_scanner @@

.PHONY: dist
dist:
	bash scripts/dist.sh

.PHONY: clean
clean:
	rm -f civicc.tar.gz
	rm -rf build/
	rm -rf build-afl/
