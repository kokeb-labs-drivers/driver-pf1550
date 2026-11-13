# PF1550 host example

The driver on your PC, with no hardware: the host port in
[`host_pf1550_port.hpp`](host_pf1550_port.hpp) connects the real driver to a simulated PF1550 (the register model
the host tests use), and [`main.cpp`](main.cpp) starts it with a configuration that has limits, configures the
charger and LDO1, then reads the charger status, the
rail and the interrupts.

## Build and run

From the repository root, with CMake 3.16+ and a C++20 compiler:

```bash
cmake -S examples/host -B build-example
cmake --build build-example
./build-example/pf1550_host_example
```

## Expected output

```text
PF1550 driver 0.1.0 on the host
pf1550: initialised
charger state 1, fault 0, VBUS valid 1
LDO1 1800000 uV, run 1
0 interrupt passes
```

The host tests build this folder too and run the example (CTest `pf1550_host_example`), so CI fails if it stops
compiling or a call fails.

## Files

| File | What it is |
| --- | --- |
| [`main.cpp`](main.cpp) | The example: start, configure, read, print |
| [`host_pf1550_port.hpp`](host_pf1550_port.hpp) | The host port: bus hooks over the simulated chip, `DelayUs()` and `Log()` |
| [`CMakeLists.txt`](CMakeLists.txt) | Standalone project; adds the driver with `add_subdirectory(../..)` |
