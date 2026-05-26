.PHONY: all build clean test e2e bench bench-regress fuzz fuzz-smoke fmt fmt-check asan tsan docker

BUILD ?= build
JOBS  ?= 4

all: build

build:
	cmake -S . -B $(BUILD) -DCMAKE_BUILD_TYPE=Release
	cmake --build $(BUILD) -j$(JOBS)

asan:
	cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DLUMEN_SANITIZE=ON
	cmake --build build-asan -j$(JOBS)

tsan:
	cmake -S . -B build-tsan -DCMAKE_BUILD_TYPE=Debug -DLUMEN_TSAN=ON
	cmake --build build-tsan -j$(JOBS)

test: build
	ctest --test-dir $(BUILD) --output-on-failure

bench: build
	$(BUILD)/lumen_bench --json > bench/results/bench_local.json
	@cat bench/results/bench_local.json

bench-regress: build
	$(BUILD)/lumen_bench --check bench/results/bench_local.json --tolerance 0.30

fuzz:
	cmake -S . -B build-fuzz -DCMAKE_BUILD_TYPE=Debug -DLUMEN_FUZZ=ON -DCMAKE_CXX_COMPILER=clang++
	cmake --build build-fuzz -j$(JOBS) --target parser_fuzz

fuzz-smoke: fuzz
	mkdir -p fuzz-corpus
	build-fuzz/parser_fuzz -runs=5000 -max_len=512 fuzz-corpus

fmt:
	find src tests bench -type f \( -name '*.cpp' -o -name '*.h' \) -print0 | xargs -0 clang-format -i

fmt-check:
	find src tests bench -type f \( -name '*.cpp' -o -name '*.h' \) -print0 | xargs -0 clang-format -n --Werror

docker:
	docker build -t lumen-lang:local .

clean:
	rm -rf build build-asan build-tsan build-fuzz fuzz-corpus
