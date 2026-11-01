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

# run the fake ecu on the pi (ctrl-c stops it)
fake-ecu: sync
    ssh -t {{pi}} 'python3 can-logger/tools/fake_ecu.py'
