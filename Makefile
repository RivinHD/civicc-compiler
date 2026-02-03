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
	@echo "  fuzz_civicc_grammar:  Fuzz the complete civcc compiler with afl, with only syntax correct civicc programs."
	@echo "  fuzz_scanparse:  Fuzz the scanner and parser of the civcc compiler with afl, with correct and incorrect civicc programs."
	@echo "  fuzz_scanparse_grammar:  Fuzz the scanner and parser of the civcc compiler with afl, with only syntax correct civicc programs."
	@echo "  fuzz_context_grammar:  Fuzz the context analysis of the civcc compiler with afl, with only syntax correct civicc programs."
	@echo "  fuzz_codegenprep_grammar:  Fuzz the codegen preparation of the civcc compiler with afl, with only syntax correct civicc programs."
	@echo "  fuzz_codegen_grammar:  Fuzz the codegen of the civcc compiler with afl, with only syntax correct civicc programs."

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

define gen_seeds
	$(eval $@_grammar = $(1))

	@if [ ! -d "./afl/seeds/${$@_grammar}" ]; then \
		mkdir -p afl/seeds && mkdir -p afl/trees && ./build-afl/grammar_generator-${$@_grammar} 100 1000 ./afl/seeds/${$@_grammar} ./afl/trees/${$@_grammar}; \
		echo "Finished seed generation for ${$@_grammar}."; \
	else \
		echo "Seed generation skipped for ${$@_grammar}, already exists."; \
	fi
endef

.PHONY: generate_seeds
generate_seeds:
	$(call gen_seeds,civicc)
	$(call gen_seeds,civiccfixedids)

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

# We use the exit code 1 for the grammar space fuzzer which leverages the grammar
# To use ASAN with 64-bit Target we use -m none and ensure memory safty by setting the ASAN option
# soft_rss_limit_mb=256 which return null if we try to allocate more.
define fuzz_multicore
	$(eval $@_target = $(1))
	$(eval $@_grammar = $(2))
	$(eval $@_dirname = $(3))
	$(eval $@_afl_extra_args = $(4))

	@if [ ! -d "./afl/seeds/${$@_target}" ]; then \
		AFL_TMPDIR="${TMPFS_DIR}/fuzz_${$@_dirname}" AFL_CUSTOM_MUTATOR_LIBRARY=./build-afl/libgrammarmutator-${$@_grammar}.so ${$@_afl_extra_args} afl-cmin -i ./afl/seeds/${$@_grammar} -o ./afl/seeds/${$@_target} -- ./build-afl/${$@_target}; \
		echo "Finished minizing the corpus for ${$@_target}."; \
	else \
		echo "Corpus already minimized for ${$@_target}."; \
	fi
	@echo "Starting Multi-Core AFL++ on $(FUZZ_CORES) cores."
	mkdir -p afl/${$@_dirname}/multi/fuzzer1
	cp -r -u afl/trees/${$@_grammar} afl/${$@_dirname}/multi/fuzzer1
	mkdir -p "${TMPFS_DIR}/fuzz_${$@_dirname}/fuzzer1"
	tmux new-session -s fuzzer1 -d "ASAN_OPTIONS=hard_rss_limit_mb=512:soft_rss_limit_mb=256:allocator_may_return_null=1:abort_on_error=1:symbolize=0 AFL_AUTORESUME=1 AFL_FINAL_SYNC=1 AFL_TESTCACHE_SIZE=100 AFL_TMPDIR=\"${TMPFS_DIR}/fuzz_${$@_dirname}/fuzzer1\" AFL_CUSTOM_MUTATOR_LIBRARY=./build-afl/libgrammarmutator-${$@_grammar}.so ${$@_afl_extra_args} afl-fuzz -m none -t 10 -i ./afl/seeds/${$@_target} -o ./afl/${$@_dirname}/multi -M fuzzer1 -w ./build-afl/${$@_target}_asan -w ./build-afl/${$@_target}_ubsan -w ./build-afl/${$@_target}_msan -- ./build-afl/${$@_target} @@; tmux capture-pane -pS - >| afl/${$@_dirname}/multi/fuzzer1/console_output.txt"
	sleep 5 # Let Master stabelize
	@for i in $(shell seq 2 $(FUZZ_CORES)); do \
		mkdir -p afl/${$@_dirname}/multi/fuzzer$$i; \
		cp -r -u afl/trees/${$@_grammar} afl/${$@_dirname}/multi/fuzzer$$i; \
		mkdir -p "${TMPFS_DIR}/fuzz_${$@_dirname}/fuzzer$$i"; \
		tmux new-session -s fuzzer$$i -d "ASAN_OPTIONS=hard_rss_limit_mb=512:soft_rss_limit_mb=256:allocator_may_return_null=1:abort_on_error=1:symbolize=0 AFL_AUTORESUME=1 AFL_TESTCACHE_SIZE=100 AFL_TMPDIR=\"${TMPFS_DIR}/fuzz_${$@_dirname}/fuzzer$$i\" AFL_CUSTOM_MUTATOR_LIBRARY=./build-afl/libgrammarmutator-${$@_grammar}.so ${$@_afl_extra_args} afl-fuzz -m none -t 10 -i ./afl/seeds/${$@_target} -o ./afl/${$@_dirname}/multi -S fuzzer$$i -w ./build-afl/${$@_target}_asan -w ./build-afl/${$@_target}_ubsan -w ./build-afl/${$@_target}_msan -- ./build-afl/${$@_target} @@; tmux capture-pane -pS - >| afl/${$@_dirname}/multi/fuzzer$$i/console_output.txt"; \
		sleep 0.1; \
	done 
endef

define fuzz_single
	$(eval $@_target = $(1))
	$(eval $@_grammar = $(2))
	$(eval $@_dirname = $(3))
	$(eval $@_afl_extra_args = $(4))

	@if [ ! -d "./afl/seeds/${$@_target}" ]; then \
		AFL_TMPDIR="${TMPFS_DIR}/fuzz_${$@_dirname}" AFL_CUSTOM_MUTATOR_LIBRARY=./build-afl/libgrammarmutator-${$@_grammar}.so ${$@_afl_extra_args} afl-cmin -i ./afl/seeds/${$@_grammar} -o ./afl/seeds/${$@_target} -- ./build-afl/${$@_target}; \
		echo "Finished minizing the corpus for ${$@_target}."; \
	else \
		echo "Corpus already minimized for ${$@_target}."; \
	fi
	mkdir -p afl/${$@_dirname}/out/default
	cp -r -u afl/trees/${$@_grammar} afl/${$@_dirname}/out/default
	mkdir -p "${TMPFS_DIR}/fuzz_${$@_dirname}"
	ASAN_OPTIONS=hard_rss_limit_mb=512:soft_rss_limit_mb=256:allocator_may_return_null=1:abort_on_error=1:symbolize=0 AFL_AUTORESUME=1 AFL_TMPDIR="${TMPFS_DIR}/fuzz_${$@_dirname}" AFL_CUSTOM_MUTATOR_LIBRARY=./build-afl/libgrammarmutator-${$@_grammar}.so ${$@_afl_extra_args} afl-fuzz -m none -t 10 -i ./afl/seeds/${$@_target} -o ./afl/${$@_dirname}/out -w ./build-afl/${$@_target}_asan -w ./build-afl/${$@_target}_ubsan -w ./build-afl/${$@_target}_msan -- ./build-afl/${$@_target} @@
endef
# Fuzz the complete compiler
.PHONY: fuzz_civicc_multi
fuzz_civicc_multi: check_tmpfs generate_seeds
	$(call fuzz_multicore,civicc,civicc,civicc,)

.PHONY: fuzz_civicc
fuzz_civicc: check_tmpfs generate_seeds
	$(call fuzz_single,civicc,civicc,civicc,)

.PHONY: fuzz_civicc_grammar_multi
fuzz_civicc_grammar_multi: check_tmpfs generate_seeds
	$(call fuzz_multicore,civicc,civicc,civicc_grammar,AFL_CUSTOM_MUTATOR_ONLY=1)

.PHONY: fuzz_civicc_grammar
fuzz_civicc_grammar: check_tmpfs generate_seeds
	$(call fuzz_single,civicc,civicc,civicc_grammar,AFL_CUSTOM_MUTATOR_ONLY=1)

# Fuzz the scanner and parser only
.PHONY: fuzz_scanparse_multi
fuzz_scanparse_multi: check_tmpfs generate_seeds
	$(call fuzz_multicore,civicc_scanparse,civicc,civicc_scanparse,)

.PHONY: fuzz_scanparse
fuzz_scanparse: check_tmpfs generate_seeds
	$(call fuzz_single,civicc_scanparse,civicc,civicc_scanparse,)

.PHONY: fuzz_scanparse_grammar_multi
fuzz_scanparse_grammar_multi: check_tmpfs generate_seeds
	$(call fuzz_multicore,civicc_scanparse,civicc,civicc_scanparse_grammar,AFL_CRASH_EXITCODE=1 AFL_CUSTOM_MUTATOR_ONLY=1)

.PHONY: fuzz_scanparse_grammar
fuzz_scanparse_grammar: check_tmpfs generate_seeds
	$(call fuzz_single,civicc_scanparse,civicc,civicc_scanparse_grammar,AFL_CRASH_EXITCODE=1 AFL_CUSTOM_MUTATOR_ONLY=1)

.PHONY: fuzz_context_grammar_multi
fuzz_context_grammar_multi: check_tmpfs generate_seeds
	$(call fuzz_multicore,civicc_context,civicc,civicc_context_grammar,AFL_CUSTOM_MUTATOR_ONLY=1)

.PHONY: fuzz_context_grammar
fuzz_context_grammar: check_tmpfs generate_seeds
	$(call fuzz_single,civicc_context,civicc,civicc_context_grammar,AFL_CUSTOM_MUTATOR_ONLY=1)

# Switch to grammar lib civiccfixedids to be able to generate matching ids and produce more 
# syntax and context correct civicc programs
.PHONY: fuzz_codegenprep_grammar_multi
fuzz_codegenprep_grammar_multi: check_tmpfs generate_seeds
	$(call fuzz_multicore,civicc_codegenprep,civiccfixedids,civicc_codegenprep_grammar,AFL_CUSTOM_MUTATOR_ONLY=1 AFL_FAST_CAL=1)

.PHONY: fuzz_codegenprep_grammar
fuzz_codegenprep_grammar: check_tmpfs generate_seeds
	$(call fuzz_single,civicc_codegenprep,civiccfixedids,civicc_codegenprep_grammar,AFL_CUSTOM_MUTATOR_ONLY=1 AFL_FAST_CAL=1)

.PHONY: fuzz_codegen_grammar_multi
fuzz_codegen_grammar_multi: check_tmpfs generate_seeds
	$(call fuzz_multicore,civicc_codegen,civiccfixedids,civicc_codegen_grammar,AFL_CUSTOM_MUTATOR_ONLY=1 AFL_FAST_CAL=1)

.PHONY: fuzz_codegen_grammar
fuzz_codegen_grammar: check_tmpfs generate_seeds
	$(call fuzz_single,civicc_codegen,civiccfixedids,civicc_codegen_grammar,AFL_CUSTOM_MUTATOR_ONLY=1 AFL_FAST_CAL=1)

.PHONY: dist
dist:
	bash scripts/dist.sh

.PHONY: clean
clean:
	rm -f civicc.tar.gz
	rm -rf build/
	rm -rf build-afl/
