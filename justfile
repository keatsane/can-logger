# task runner - run `just` with no args to list recipes
set shell := ["bash", "-euc"]

# the pi, over ssh. set PI_HOST=keating@<pi-address> where .local doesn't resolve
pi := env_var_or_default("PI_HOST", "keating@can-logger.local")

_default:
    @just --list

# the unit tests again under address + undefined-behavior sanitizers. can't share a build with tsan
asan:
    cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug "-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer" "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined"
    cmake --build build-asan --target unit_tests
    setarch -R ctest --test-dir build-asan --output-on-failure

# configure (only does work the first time or after CMakeLists.txt changes) and compile
build:
    cmake -S . -B build
    cmake --build build

# what the pre-commit hook runs: lint is clean and the tests pass plain and under each sanitizer
check:
    just lint
    just test
    just tsan
    just asan

# run the fake ecu on the pi (ctrl-c stops it). `just fake-ecu -q` only prints problems
fake-ecu *args: sync
    ssh -t {{pi}} 'python3 can-logger/tools/fake_ecu.py {{args}}'

# rewrite the c++, python and cmake in place to match the project style
format:
    black --quiet tests/*.py tools/*.py
    clang-format -i src/*.cpp src/*.hpp tests/*.cpp
    gersemi --in-place CMakeLists.txt

# use the hooks in tools/hooks (once per clone)
hooks:
    git config core.hooksPath tools/hooks

# check formatting and text rules everywhere without changing anything. `just format` fixes most of it
lint:
    black --check --quiet tests/*.py tools/*.py
    clang-format --dry-run --Werror src/*.cpp src/*.hpp tests/*.cpp
    gersemi --check CMakeLists.txt
    shellcheck tools/*.sh tools/hooks/pre-commit
    tools/check-text.sh

# sync, build on the pi, run the logger there (ctrl-c stops it). logs go in ~/logs
logger: pi-build
    ssh -t {{pi}} 'mkdir -p logs && cd logs && ../can-logger/build/logger'

# sync, then build on the pi
pi-build: sync
    ssh {{pi}} 'cd can-logger && cmake -S . -B build && cmake --build build'

# sync, build, and install the service and .link files on the pi. never enables anything
pi-install: pi-build
    ssh -t {{pi}} 'mkdir -p logs && sudo cp can-logger/config/systemd/*.service /etc/systemd/system/ && sudo cp can-logger/config/systemd/*.link /etc/systemd/network/ && sudo systemctl daemon-reload'

# sync, build on the pi, run the probe there
probe: pi-build
    ssh {{pi}} 'can-logger/build/probe'

# copy the logs from the pi into logs/ here
pull-logs:
    mkdir -p logs
    rsync -a {{pi}}:logs/ logs/

# copy the working tree to the pi
sync:
    PI_HOST={{pi}} tools/sync-to-pi.sh

# build, then run the c++ unit tests and the python tools' tests
test: build
    ctest --test-dir build --output-on-failure
    python3 -m unittest discover --start-directory tests --pattern '*_test.py'

# the unit tests again under threadsanitizer, which fails on any data race between threads
tsan:
    cmake -S . -B build-tsan -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS=-fsanitize=thread -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=thread
    cmake --build build-tsan --target unit_tests
    setarch -R ctest --test-dir build-tsan --output-on-failure
