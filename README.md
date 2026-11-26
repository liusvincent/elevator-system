# Elevator System

An interactive multi-elevator and passenger simulation written in C++20 with
SDL3. It includes manual elevator controls, passenger spawning, capacity
limits, and animated doors.

## Run

```sh
git submodule update --init --recursive
cmake -S . -B build
cmake --build build
./build/elevator-system
```

Press `H` or `?` in the application to view the controls.
