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
	@echo "  debug:  Generate all build artifacts for a debug build."
	@echo "  release:  Generate all build artifacts for a release build."
	@echo "  civcc:  Only build the civicc compiler as a release build."
	@echo "  dist:  Pack civicc into a tar.gz file. Use this for creating a submission."
	@echo "  clean:  Remove all build directories and created dist files."
	@echo "  test:  Runs all test with ctest in parallel."
	@echo "  afl_build:  Build the targets and sanatized targets with the afl compiler."
	@echo "  generate_seeds:  Generates seeds for the afl run, these will generate valid civicc programs."
	@echo "  fuzz_scanparse:  Fuzz the scanner and parser of the civcc compiler with afl, with correct and incorrect civicc programs."
	@echo "  fuzz_scanparse_grammar:  Fuzz the scanner and parser of the civcc compiler with afl, with only syntax correct civicc programs."
	@echo "  fuzz_context_grammar:  Fuzz the context analysis of the civcc compiler with afl, with only syntax correct civicc programs."
	@echo "  fuzz_codegenprep_grammar:  Fuzz the codegen preparation of the civcc compiler with afl, with only syntax correct civicc programs."
	@echo "  fuzz_codegen_grammar:  Fuzz the codegen of the civcc compiler with afl, with only syntax correct civicc programs."
	@echo "  fuzz_codegen_optimized_grammar:  Fuzz the codegen with optimizations of the civcc compiler with afl, with only syntax correct civicc programs."
	@echo "  fuzz_optimization_grammar:  Fuzz the optimizations of the civcc compiler with afl, with only syntax correct civicc programs."

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

.PHONY: civicc
civicc: jobs
	@cmake -DCMAKE_BUILD_TYPE=Release -S ./ -B build/ && cmake --build build -j $(JOBS) --target civicc

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
	@(export AFL_USE_LSAN=1 && export AFL_LLVM_ONLY_FSRV=1 && cmake -DCMAKE_BUILD_TYPE=Release -S ./ -B build-afl -DCMAKE_C_COMPILER=afl-cc -DCMAKE_CXX_COMPILER=afl-c++ && cmake --build build-afl -j $(JOBS))
	@(export AFL_USE_MSAN=1 && export AFL_LLVM_ONLY_FSRV=1 && cmake -DCMAKE_BUILD_TYPE=Release -S ./ -B build-afl -DCMAKE_C_COMPILER=afl-cc -DCMAKE_CXX_COMPILER=afl-c++ && cmake --build build-afl -j $(JOBS))
	@(export AFL_USE_UBSAN=1 && export AFL_LLVM_ONLY_FSRV=1 && export AFL_UBSAN_VERBOSE=1 && cmake -DCMAKE_BUILD_TYPE=Release -S ./ -B build-afl -DCMAKE_C_COMPILER=afl-cc -DCMAKE_CXX_COMPILER=afl-c++ && cmake --build build-afl -j $(JOBS))
	@echo "Finished Building all requiered AFL targets"
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
		AFL_CUSTOM_MUTATOR_LIBRARY=./build-afl/libgrammarmutator-${$@_grammar}.so ${$@_afl_extra_args} afl-cmin -i ./afl/seeds/${$@_grammar} -o ./afl/seeds/${$@_target} -- ./build-afl/${$@_target}; \
		echo "Finished minizing the corpus for ${$@_target}."; \
	else \
		echo "Corpus already minimized for ${$@_target}."; \
	fi
	@echo "Starting Multi-Core AFL++ on $(FUZZ_CORES) cores."
	mkdir -p afl/${$@_dirname}/multi/fuzzer1
	cp -r -u afl/trees/${$@_grammar} afl/${$@_dirname}/multi/fuzzer1
	tmux new-session -s fuzzer1 -d "LSAN_OPTIONS=hard_rss_limit_mb=512:soft_rss_limit_mb=256:allocator_may_return_null=1:detect_leaks=1:exitcode=23:abort_on_error=1:symbolize=0 ASAN_OPTIONS=hard_rss_limit_mb=512:soft_rss_limit_mb=256:allocator_may_return_null=1:abort_on_error=1:symbolize=0:detect_leaks=0 AFL_AUTORESUME=1 AFL_TESTCACHE_SIZE=100 AFL_CUSTOM_MUTATOR_LIBRARY=./build-afl/libgrammarmutator-${$@_grammar}.so ${$@_afl_extra_args} afl-fuzz -m none -t 10 -i ./afl/seeds/${$@_target} -o ./afl/${$@_dirname}/multi -M fuzzer1 -w ./build-afl/${$@_target}_asan -w ./build-afl/${$@_target}_lsan -w ./build-afl/${$@_target}_ubsan -w ./build-afl/${$@_target}_msan -- ./build-afl/${$@_target} @@; tmux capture-pane -pS - >| afl/${$@_dirname}/multi/fuzzer1/console_output.txt"
	sleep 5 # Let Master stabelize
	@percent_10=$$(($(FUZZ_CORES) * 10 / 100)); \
	if [ $$percent_10 -eq 0 ]; then percent_10=1; fi; \
	cycling_step=$$(($(FUZZ_CORES) / percent_10)); \
	no_trim_step=2; \
	RANDOM=42; \
	cpu_40=$$(($(FUZZ_CORES) * 40 / 100 + 1)); \
	cpu_20=$$(($(FUZZ_CORES) * 20 / 100 + 1 + $$cpu_40)); \
	schedule_start=$$(($(FUZZ_CORES) - 5)); \
	declare -a schedules=("fast" "coe" "lin" "quad" "rare"); \
	prand_cylcing=$$(( ($$RANDOM % 5 + 2) / 3 + 1 )); \
	prand_no_trim=$$(( $$RANDOM % 2 )); \
	for i in $(shell seq 2 $(FUZZ_CORES)); do \
		mutations=""; \
		trim=""; \
		if [ $$i -gt $$schedule_start ]; then \
			index=$$(( $$i - $$schedule_start - 1 )); \
			mutations="-p $${schedules[$$index]}"; \
		fi; \
		if [ $$(( $$i % $$cycling_step )) -eq $$prand_cylcing ]; then \
			mutations="$$mutations -Z"; \
			prand_cylcing=$$(( ($$RANDOM % 5 + 2) / 3 + 1 )); \
		fi; \
		if [ $$(( $$i % $$no_trim_step )) -eq $$prand_no_trim ]; then \
			trim="AFL_DISABLE_TRIM=1"; \
			prand_no_trim=$$(( $$RANDOM % 2 )); \
		fi; \
		if [ $$i -le $$cpu_40 ]; then \
			mutations="$$mutations -P explore"; \
		elif [ $$i -le $$cpu_20 ]; then \
			mutations="$$mutations -P exploit"; \
		fi; \
		mkdir -p afl/${$@_dirname}/multi/fuzzer$$i; \
		cp -r -u afl/trees/${$@_grammar} afl/${$@_dirname}/multi/fuzzer$$i; \
		tmux new-session -s fuzzer$$i -d "LSAN_OPTIONS=hard_rss_limit_mb=512:soft_rss_limit_mb=256:allocator_may_return_null=1:detect_leaks=1:exitcode=23:abort_on_error=1:symbolize=0 ASAN_OPTIONS=hard_rss_limit_mb=512:soft_rss_limit_mb=256:allocator_may_return_null=1:abort_on_error=1:symbolize=0:detect_leaks=0 AFL_AUTORESUME=1 AFL_TESTCACHE_SIZE=100 AFL_CUSTOM_MUTATOR_LIBRARY=./build-afl/libgrammarmutator-${$@_grammar}.so ${$@_afl_extra_args} $$trim afl-fuzz $$mutations -m none -t 10 -i ./afl/seeds/${$@_target} -o ./afl/${$@_dirname}/multi -S fuzzer$$i -w ./build-afl/${$@_target}_asan -w ./build-afl/${$@_target}_lsan -w ./build-afl/${$@_target}_ubsan -w ./build-afl/${$@_target}_msan -- ./build-afl/${$@_target} @@; tmux capture-pane -pS - >| afl/${$@_dirname}/multi/fuzzer$$i/console_output.txt"; \
		sleep 0.1; \
	done 
endef

define fuzz_single
	$(eval $@_target = $(1))
	$(eval $@_grammar = $(2))
	$(eval $@_dirname = $(3))
	$(eval $@_afl_extra_args = $(4))

	@if [ ! -d "./afl/seeds/${$@_target}" ]; then \
		AFL_CUSTOM_MUTATOR_LIBRARY=./build-afl/libgrammarmutator-${$@_grammar}.so ${$@_afl_extra_args} afl-cmin -i ./afl/seeds/${$@_grammar} -o ./afl/seeds/${$@_target} -- ./build-afl/${$@_target}; \
		echo "Finished minizing the corpus for ${$@_target}."; \
	else \
		echo "Corpus already minimized for ${$@_target}."; \
	fi
	mkdir -p afl/${$@_dirname}/out/default
	cp -r -u afl/trees/${$@_grammar} afl/${$@_dirname}/out/default
	LSAN_OPTIONS=hard_rss_limit_mb=512:soft_rss_limit_mb=256:allocator_may_return_null=1:detect_leaks=1:exitcode=23:abort_on_error=1:symbolize=0 ASAN_OPTIONS=hard_rss_limit_mb=512:soft_rss_limit_mb=256:allocator_may_return_null=1:abort_on_error=1:symbolize=0:detect_leaks=0 AFL_AUTORESUME=1 AFL_CUSTOM_MUTATOR_LIBRARY=./build-afl/libgrammarmutator-${$@_grammar}.so ${$@_afl_extra_args} afl-fuzz -m none -t 10 -i ./afl/seeds/${$@_target} -o ./afl/${$@_dirname}/out -w ./build-afl/${$@_target}_asan -w ./build-afl/${$@_target}_lsan -w ./build-afl/${$@_target}_ubsan -w ./build-afl/${$@_target}_msan -- ./build-afl/${$@_target} @@
endef

# Fuzz the scanner and parser only
.PHONY: fuzz_scanparse_multi
fuzz_scanparse_multi: generate_seeds
	$(call fuzz_multicore,civicc_scanparse,civicc,civicc_scanparse,)

.PHONY: fuzz_scanparse
fuzz_scanparse: generate_seeds
	$(call fuzz_single,civicc_scanparse,civicc,civicc_scanparse,)

.PHONY: fuzz_scanparse_grammar_multi
fuzz_scanparse_grammar_multi: generate_seeds
	$(call fuzz_multicore,civicc_scanparse,civicc,civicc_scanparse_grammar,AFL_CRASH_EXITCODE=1 AFL_CUSTOM_MUTATOR_ONLY=1)

.PHONY: fuzz_scanparse_grammar
fuzz_scanparse_grammar: generate_seeds
	$(call fuzz_single,civicc_scanparse,civicc,civicc_scanparse_grammar,AFL_CRASH_EXITCODE=1 AFL_CUSTOM_MUTATOR_ONLY=1)

.PHONY: fuzz_context_grammar_multi
fuzz_context_grammar_multi: generate_seeds
	$(call fuzz_multicore,civicc_context,civicc,civicc_context_grammar,AFL_CUSTOM_MUTATOR_ONLY=1)

.PHONY: fuzz_context_grammar
fuzz_context_grammar: generate_seeds
	$(call fuzz_single,civicc_context,civicc,civicc_context_grammar,AFL_CUSTOM_MUTATOR_ONLY=1)

# Switch to grammar lib civiccfixedids to be able to generate matching ids and produce more 
# syntax and context correct civicc programs
.PHONY: fuzz_codegenprep_grammar_multi
fuzz_codegenprep_grammar_multi: generate_seeds
	$(call fuzz_multicore,civicc_codegenprep,civiccfixedids,civicc_codegenprep_grammar,AFL_CUSTOM_MUTATOR_ONLY=1 AFL_FAST_CAL=1)

.PHONY: fuzz_codegenprep_grammar
fuzz_codegenprep_grammar: generate_seeds
	$(call fuzz_single,civicc_codegenprep,civiccfixedids,civicc_codegenprep_grammar,AFL_CUSTOM_MUTATOR_ONLY=1 AFL_FAST_CAL=1)

.PHONY: fuzz_codegen_grammar_multi
fuzz_codegen_grammar_multi: generate_seeds
	$(call fuzz_multicore,civicc_codegen,civiccfixedids,civicc_codegen_grammar,AFL_CUSTOM_MUTATOR_ONLY=1 AFL_FAST_CAL=1)

.PHONY: fuzz_codegen_grammar
fuzz_codegen_grammar: generate_seeds
	$(call fuzz_single,civicc_codegen,civiccfixedids,civicc_codegen_grammar,AFL_CUSTOM_MUTATOR_ONLY=1 AFL_FAST_CAL=1)

.PHONY: fuzz_codegen_optimized_grammar_multi
fuzz_codegen_optimized_grammar_multi: generate_seeds
	$(call fuzz_multicore,civicc_codegen_optimized,civiccfixedids,civicc_codegen_optimized_grammar,AFL_CUSTOM_MUTATOR_ONLY=1 AFL_FAST_CAL=1)

.PHONY: fuzz_codegen_optimized_grammar
fuzz_codegen_optimized_grammar: generate_seeds
	$(call fuzz_single,civicc_codegen_optimized,civiccfixedids,civicc_codegen_optimized_grammar,AFL_CUSTOM_MUTATOR_ONLY=1 AFL_FAST_CAL=1)

.PHONY: fuzz_optimization_grammar_multi
fuzz_optimization_grammar_multi: generate_seeds
	$(call fuzz_multicore,civicc_optimization,civiccfixedids,civicc_optimization_grammar,AFL_CUSTOM_MUTATOR_ONLY=1 AFL_FAST_CAL=1)

.PHONY: fuzz_optimization_grammar
fuzz_optimization_grammar: generate_seeds
	$(call fuzz_single,civicc_optimization,civiccfixedids,civicc_optimization_grammar,AFL_CUSTOM_MUTATOR_ONLY=1 AFL_FAST_CAL=1)

.PHONY: dist
dist:
	bash scripts/dist.sh

.PHONY: clean
clean:
	rm -f civicc.tar.gz
	rm -rf build/
	rm -rf build-afl/
