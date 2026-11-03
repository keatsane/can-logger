# task runner - run `just` with no args to list recipes
set shell := ["bash", "-euc"]

# the pi, over ssh. set PI_HOST=keating@<pi-address> where .local doesn't resolve
pi := env_var_or_default("PI_HOST", "keating@can-logger.local")

_default:
    @just --list --unsorted

# configure (only does work the first time or after CMakeLists.txt changes) and compile
build:
    cmake -S . -B build
    cmake --build build

# build, then run the unit tests
test: build
    ctest --test-dir build --output-on-failure

# the unit tests again under threadsanitizer, which fails on any data race between threads
tsan:
    cmake -S . -B build-tsan -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS=-fsanitize=thread -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=thread
    cmake --build build-tsan --target unit_tests
    setarch -R ctest --test-dir build-tsan --output-on-failure

# the unit tests again under address + undefined-behavior sanitizers: dangling pointers,
# out-of-bounds reads, leaks, signed overflow. can't share a build with tsan
asan:
    cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug "-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer" "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined"
    cmake --build build-asan --target unit_tests
    setarch -R ctest --test-dir build-asan --output-on-failure

# what the pre-commit hook runs: formatting is clean and the tests pass plain and under each sanitizer
check:
    clang-format --dry-run --Werror src/*.cpp src/*.hpp tests/*.cpp
    just test
    just tsan
    just asan

# use the hooks in tools/hooks (once per clone)
hooks:
    git config core.hooksPath tools/hooks

# clang-format the c++ in place (python is formatted by black on save in vs code)
format:
    clang-format -i src/*.cpp src/*.hpp tests/*.cpp

# copy the working tree to the pi
sync:
    PI_HOST={{pi}} tools/sync-to-pi.sh

# sync, then build on the pi
pi-build: sync
    ssh {{pi}} 'cd can-logger && cmake -S . -B build && cmake --build build'

# sync, build on the pi, run the probe there
probe: pi-build
    ssh {{pi}} 'can-logger/build/probe'

# sync, build on the pi, run the logger there (ctrl-c stops it). logs go in ~/logs
logger: pi-build
    ssh -t {{pi}} 'mkdir -p logs && cd logs && ../can-logger/build/logger'

# run the fake ecu on the pi (ctrl-c stops it). `just fake-ecu -q` only prints problems
fake-ecu *args: sync
    ssh -t {{pi}} 'python3 can-logger/tools/fake_ecu.py {{args}}'
