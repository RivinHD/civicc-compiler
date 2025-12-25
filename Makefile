.DEFAULT_GOAL := debug

# Fallback to number of CPUs
CORE_COUNT := $(shell (command -v nproc >/dev/null 2>&1 && nproc) || (uname | grep -i Darwin >/dev/null 2>&1 && sysctl -n hw.ncpu) || echo 1)

# Try to extract -j value from MAKEFLAGS (handles both '-j4' and '-j 4')
JOBS_FROM_MAKE := $(shell echo $(MAKEFLAGS) | sed -n 's/.*-j[[:space:]]*\([0-9][0-9]*\).*/\1/p')

  # use JOBS if provided, otherwise use detected CORE_COUNT
ifeq ($(JOBS_FROM_MAKE),)
  JOBS ?= $(CORE_COUNT)
else
  override JOBS := $(JOBS_FROM_MAKE)
endif

ifeq ($(FUZZ_CORES),)
	override FUZZ_CORES := $(CORE_COUNT)
endif

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
	@echo "  fuzz_civicc_grammer:  Fuzz the complete civcc compiler with afl, with only syntax correct civicc programs."
	@echo "  fuzz_scanparse:  Fuzz the scanner and parser of the civcc compiler with afl, with correct and incorrect civicc programs."
	@echo "  fuzz_scanparse_grammer:  Fuzz the scanner and parser of the civcc compiler with afl, with only syntax correct civicc programs."

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

.PHONY: grammar_generator
grammar_generator:
	@cmake -S ./ -B build -DBUILD_GRAMMAR_GENERATOR=ON && cmake --build build -j $(JOBS) --target grammar_generator

# Everything we need for fuzzing with AFL++
# We build the sanitizer extra to use SAND (https://aflplus.plus/docs/sand/).
# LSAN does not work, because of map size/memory issue
.PHONY: afl_build
afl_build:
	@cmake -DCMAKE_BUILD_TYPE=Release -S ./ -B build-afl -DCMAKE_C_COMPILER=afl-cc -DCMAKE_CXX_COMPILER=afl-c++ && cmake --build build-afl -j $(JOBS)
	@(export AFL_USE_ASAN=1 && export AFL_LLVM_ONLY_FSRV=1 && cmake -DCMAKE_BUILD_TYPE=Release -S ./ -B build-afl -DCMAKE_C_COMPILER=afl-cc -DCMAKE_CXX_COMPILER=afl-c++ && cmake --build build-afl -j $(JOBS))
	@#(export AFL_USE_LSAN=1 && export AFL_LLVM_ONLY_FSRV=1 && cmake -DCMAKE_BUILD_TYPE=Release -S ./ -B build-afl -DCMAKE_C_COMPILER=afl-cc -DCMAKE_CXX_COMPILER=afl-c++ && cmake --build build-afl -j $(JOBS))
	@(export AFL_USE_UBSAN=1 && export AFL_LLVM_ONLY_FSRV=1 && export AFL_UBSAN_VERBOSE=1 && cmake -DCMAKE_BUILD_TYPE=Release -S ./ -B build-afl -DCMAKE_C_COMPILER=afl-cc -DCMAKE_CXX_COMPILER=afl-c++ && cmake --build build-afl -j $(JOBS))
	@(export AFL_USE_MSAN=1 && export AFL_LLVM_ONLY_FSRV=1 && cmake -DCMAKE_BUILD_TYPE=Release -S ./ -B build-afl -DCMAKE_C_COMPILER=afl-cc -DCMAKE_CXX_COMPILER=afl-c++ && cmake --build build-afl -j $(JOBS))
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


.PHONY: stop_fuzzer_sessions
stop_fuzzer_sessions:
	@read -p "Stop all fuzzer in tmux session starting with 'fuzzer'? [y/n] " -r -n 1; \
	echo; \
	if [[ "$$REPLY" =~ ^[Yy]$$ ]]; then \
		echo "Sending stop signal to fuzzer"; \
		tmux list-sessions -F '#{session_name}' | grep "^fuzzer" | while read session; do \
			tmux send-keys -t $$session C-c; \
		done; \
		seconds=0; \
		running_fuzzer=$$(tmux list-sessions -F '#{session_name}' | grep "^fuzzer" | tr '\n' ', '); \
		while [ -n "$$running_fuzzer" ]; do \
			echo "Waiting ($$seconds seconds) for the fuzzer to finish:"; \
			echo "$$running_fuzzer"; \
			seconds=$$((seconds + 1)); \
			sleep 1; \
			printf '\033[2A\033[J'; \
			running_fuzzer=$$(tmux list-sessions -F '#{session_name}' | grep "^fuzzer" | tr '\n' ', '); \
		done; \
		echo "All fuzzer stopped!"; \
	fi

# We use the exit code 1 for the grammer space fuzzer which leverages the grammar
# To use ASAN with 64-bit Target we use -m none and ensure memory safty by setting the ASAN option
# soft_rss_limit_mb=256 which return null if we try to allocate more.
define fuzz_multicore
	$(eval $@_target = $(1))
	$(eval $@_dirname = $(2))
	$(eval $@_afl_extra_args = $(3))

	@echo "Starting Multi-Core AFL++ on $(FUZZ_CORES) cores."
	@mkdir -p afl/${$@_dirname}/multi/fuzzer1
	@cp -r -u afl/trees afl/${$@_dirname}/multi/fuzzer1
	@mkdir -p "${TMPFS_DIR}/fuzz_${$@_dirname}/fuzzer1"
	tmux new-session -s fuzzer1 -d "ASAN_OPTIONS=hard_rss_limit_mb=512:soft_rss_limit_mb=256:allocator_may_return_null=1:abort_on_error=1:symbolize=0 AFL_AUTORESUME=1 AFL_FINAL_SYNC=1 AFL_TESTCACHE_SIZE=100 AFL_TMPDIR=\"${TMPFS_DIR}/fuzz_${$@_dirname}/fuzzer1\" AFL_CUSTOM_MUTATOR_LIBRARY=./build-afl/libgrammarmutator-civicc.so ${$@_afl_extra_args} afl-fuzz -m none -t 5 -i ./afl/seeds -o ./afl/${$@_dirname}/multi -M fuzzer1 -w ./build-afl/${$@_target}_asan -w ./build-afl/${$@_target}_ubsan -w ./build-afl/${$@_target}_msan -- ./build-afl/${$@_target} @@; tmux capture-pane -pS - >| afl/${$@_dirname}/multi/fuzzer1/console_output.txt"
	sleep 5 # Let Master stabelize
	@for i in $(shell seq 2 $(FUZZ_CORES)); do \
		mkdir -p afl/${$@_dirname}/multi/fuzzer$$i; \
		cp -r -u afl/trees afl/${$@_dirname}/multi/fuzzer$$i; \
		mkdir -p "${TMPFS_DIR}/fuzz_${$@_dirname}/fuzzer$$i"; \
		tmux new-session -s fuzzer$$i -d "ASAN_OPTIONS=hard_rss_limit_mb=512:soft_rss_limit_mb=256:allocator_may_return_null=1:abort_on_error=1:symbolize=0 AFL_AUTORESUME=1 AFL_TESTCACHE_SIZE=100 AFL_TMPDIR=\"${TMPFS_DIR}/fuzz_${$@_dirname}/fuzzer$$i\" AFL_CUSTOM_MUTATOR_LIBRARY=./build-afl/libgrammarmutator-civicc.so ${$@_afl_extra_args} afl-fuzz -m none -t 5 -i ./afl/seeds -o ./afl/${$@_dirname}/multi -S fuzzer$$i -w ./build-afl/${$@_target}_asan -w ./build-afl/${$@_target}_ubsan -w ./build-afl/${$@_target}_msan -- ./build-afl/${$@_target} @@; tmux capture-pane -pS - >| afl/${$@_dirname}/multi/fuzzer$$i/console_output.txt"; \
		sleep 0.1; \
	done 
endef

define fuzz_single
	$(eval $@_target = $(1))
	$(eval $@_dirname = $(2))
	$(eval $@_afl_extra_args = $(3))

	@mkdir -p afl/${$@_dirname}/out/default
	@cp -r -u afl/trees afl/${$@_dirname}/out/default
	@mkdir -p "${TMPFS_DIR}/fuzz_${$@_dirname}"
	ASAN_OPTIONS=hard_rss_limit_mb=512:soft_rss_limit_mb=256:allocator_may_return_null=1:abort_on_error=1:symbolize=0 AFL_AUTORESUME=1 AFL_TMPDIR="${TMPFS_DIR}/fuzz_${$@_dirname}" AFL_CUSTOM_MUTATOR_LIBRARY=./build-afl/libgrammarmutator-civicc.so ${$@_afl_extra_args} afl-fuzz -m none -t 5 -i ./afl/seeds -o ./afl/${$@_dirname}/out -w ./build-afl/${$@_target}_asan -w ./build-afl/${$@_target}_ubsan -w ./build-afl/${$@_target}_msan -- ./build-afl/${$@_target} @@
endef

# Fuzz the complete compiler
.PHONY: fuzz_civicc_multi
fuzz_civicc_multi: 
	$(call fuzz_multicore,civicc,civicc,)

.PHONY: fuzz_civicc
fuzz_civicc: 
	$(call fuzz_single,civicc,civicc,)

.PHONY: fuzz_civicc_grammar_multi
fuzz_civicc_grammar_multi: 
	$(call fuzz_multicore,civicc,civicc_grammar,AFL_CUSTOM_MUTATOR_ONLY=1)

.PHONY: fuzz_civicc_grammar
fuzz_civicc_grammar: 
	$(call fuzz_single,civicc,civicc_grammar,AFL_CUSTOM_MUTATOR_ONLY=1)

# Fuzz the scanner and parser only
.PHONY: fuzz_scanparse_multi
fuzz_scanparse_multi: 
	$(call fuzz_multicore,civicc_scanparse,civicc_scanparse,)

.PHONY: fuzz_scanparse
fuzz_scanparse: 
	$(call fuzz_single,civicc_scanparse,civicc_scanparse,)

.PHONY: fuzz_scanparse_grammar_multi
fuzz_scanparse_grammar_multi: 
	$(call fuzz_multicore,civicc_scanparse,civicc_scanparse_grammar,AFL_CRASH_EXITCODE=1 AFL_CUSTOM_MUTATOR_ONLY=1)

.PHONY: fuzz_scanparse_grammar
fuzz_scanparse_grammar: 
	$(call fuzz_single,civicc_scanparse,civicc_scanparse_grammar,AFL_CRASH_EXITCODE=1 AFL_CUSTOM_MUTATOR_ONLY=1)

.PHONY: dist
dist:
	bash scripts/dist.sh

.PHONY: clean
clean:
	rm -f civicc.tar.gz
	rm -rf build/
	rm -rf build-afl/
