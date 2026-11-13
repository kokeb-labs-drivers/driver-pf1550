# Testing and CI

How the driver is checked: host tests against an independent model of the chip, a fuzzer that corrupts what the chip
sends back, compiled documentation, an install test, ESP32 and Cortex-M builds, static analysis and measured quality
gates, on every pull request.

## Host tests

The tests build the real driver against a register model of the PF1550: the 90 documented registers with their
access types (read only, write-1-to-clear status, mask, OTP-loaded, command), the ten start-up options A0 to A9 of
Table 85, the state machine, the charger (VBUS, thermistor, timers, faults, watchdog), DVS stepping with a DVS-done
delay, the interrupt groups with their categories and sense registers, SHIP and CORE_OFF, the power-up and power-down
sequencer and the coin cell charge. Each ambiguity of the datasheet that changes behaviour is a knob of the model
([Ambiguities](ambiguities.md)).
The tests build with the firmware's flags, `-fno-exceptions -fno-rtti`, and warnings as errors, on GCC and Clang.

```bash
cmake -S . -B build -DPF1550_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

| File | Covers |
| --- | --- |
| [`tests/test_lifecycle.cpp`](../tests/test_lifecycle.cpp) | `EnsureInitialized()`: identity, the as-found charger check under both policies, every start-up option, a failure at each transfer |
| [`tests/test_config.cpp`](../tests/test_config.cpp) | `Validate()`: limits, grids, acknowledgements, rail windows, DVS declaration, at compile time and at run time |
| [`tests/test_configure.cpp`](../tests/test_configure.cpp) | `Configure()`: block order, the charger written parked and last, read-back, reserved bits preserved, every block |
| [`tests/test_charger.cpp`](../tests/test_charger.cpp) | Charger status and fault classification, modes, restart limit, watchdog, limits re-check |
| [`tests/test_rails.cpp`](../tests/test_rails.cpp) | Rail reads and writes, DVS wait and its timeout, SW3, load switch, low-power acknowledgement |
| [`tests/test_power.cpp`](../tests/test_power.cpp) | States, STANDBY, SHIP and CORE_OFF preconditions, power-down order, the sequencer functions against the model |
| [`tests/test_irq.cpp`](../tests/test_irq.cpp) | Masks, categories, clear-after-copy, INTB skip, the three clearing readings |
| [`tests/test_review.cpp`](../tests/test_review.cpp) | One test per finding of the first independent review: stuck and flipped reads, the status latch, held voltages, the DVS declaration, an unsafe charger switched off, shutdown cancel, the load switch, a chip reset |
| [`tests/test_round2.cpp`](../tests/test_round2.cpp) | One test per finding of the second review: switch-offs that are never refused, the read-agreement and reset checks on every call, the DVS wait, and the session run under every reading of the model's knobs |
| [`tests/test_round4.cpp`](../tests/test_round4.cpp) | One test per finding of the fourth review: a cable removal while charging is put right or reported, a refused charge start leaves the charger off, the buffers of reads and writes, and the guards that survived (DVS-declaration start, charge-current limit, INTB shortcut, shutdown and cancel reads, one wrong DEVICE_ID read) |
| [`tests/test_round5.cpp`](../tests/test_round5.cpp) | One test per finding of the fifth review: the guards of the cable-removal check (a limit inside Limits that is not the configured one, a refused charge start with an unacknowledged protection off, single reads), the check without `watchdog.enable`, a VBUSIN that stays invalid, the check order, LinearOnly, a DEVICE_ID line stuck at zero |
| [`tests/test_round6.cpp`](../tests/test_round6.cpp) | One test per finding of the sixth review: no write of the input limit after an unseen chip reset (status and interrupt paths), a failed check in `ServiceInterrupts()` is made again and its error returned, a status read with the limit right writes nothing |
| [`tests/test_round7.cpp`](../tests/test_round7.cpp) | One test per finding of the seventh review: a pending input limit check survives `ClearLatchedEvents()`, a bus error while collecting, a port without `GpioGet()`, a switch-off after an unseen reset, a failed verification read of a switch-off, the pass bound and count, the charger-group trigger, the pending flag cleared |
| [`tests/test_round9.cpp`](../tests/test_round9.cpp) | One test per finding of the eighth review: a charger interrupt read by a failed service still owes its input limit check (each transfer of the call failed in turn, the count taken from a clean run, with and without `ClearLatchedEvents()`), a failed mask write leaves no idle shortcut, a failed service keeps its bits in the latch, `ClearLatchedEvents()` empties it, a failed switch-off is a bus error, a failed VBUS_SNS read is an error |
| [`tests/test_round10.cpp`](../tests/test_round10.cpp) | The ninth review: a CHG_INT byte read by `ReadRegister()` still starts the input limit check of the next `ServiceInterrupts()` (clear on read and write-one-to-clear) |
| [`tests/test_round8.cpp`](../tests/test_round8.cpp) | The reset evidence of a rail control register is the bits the driver changed and verified; a bit it left alone is not a sentinel (a fuzz finding) |
| [`tests/test_round3.cpp`](../tests/test_round3.cpp) | One test per finding of the third review: a cable removal is not a chip reset (`VbusRemoved()`), a switch-off after a failed re-start, the transfer buffers inside the driver object, and the guards that no earlier test reached (a flipped sentinel read, the mask of a sentinel, the reset check of `SetIrqMask()` and `SetPowerDownSequence()`, the protection read-back guard, masked INTB sources, a clear that writes only the bits read) |
| [`tests/fuzz/`](../tests/fuzz/fuzz_pf1550.cpp) | The fuzz target; CTest replays its corpus (`pf1550_fuzz_pf1550_corpus`), see below |
| [`tests/test_errors.cpp`](../tests/test_errors.cpp) | The base `Error` values are 0 to 9; every chip-specific value (16 and up) is returned by some call |
| [`tests/test_version.cpp`](../tests/test_version.cpp) | The version getters follow the generated header (also against a 9.8.7 probe) |
| [`tests/test_kconfig.cpp`](../tests/test_kconfig.cpp) | `KconfigDefaults()` with and without `CONFIG_` values |
| [`tests/compile_fail/`](../tests/compile_fail/port_without_delay.cpp) | Code that must not compile, each for its `EXPECT:` diagnostic: a port without `DelayUs()`, a `void GpioSet()`, a `DelayUs(uint16_t)`, a hook with a narrowed parameter or without `noexcept`, a `Config` without limits, a class that is not a port |
| [`tests/mock_pf1550.hpp`](../tests/mock_pf1550.hpp) | The register model, with its own register map; the bus framing is in `mock_pf1550_bus.hpp` |
| [`tests/test_model.cpp`](../tests/test_model.cpp) | The model's register map against the driver's access table, and its reset values against the data sheet |
| [`tools/coverage_matrix.py`](../tools/coverage_matrix.py) | The coverage matrix ([`docs/coverage-matrix.csv`](coverage-matrix.csv)): totals equal [Coverage](coverage.md), every API name exists in `inc/` or `src/`, every test file exists (`pf1550_coverage_matrix`) |
| [`tests/check_version_copy.cmake`](../tests/check_version_copy.cmake) | `_config/Doxyfile` carries the CMake version |
| [`examples/host/`](../examples/host/README.md) | The host example, built with the test flags and run (`pf1550_host_example`) |
| [`tests/check_install.cmake`](../tests/check_install.cmake) | Installs into a scratch prefix and builds [`tests/install_consumer/`](../tests/install_consumer/CMakeLists.txt) with `find_package(pf1550)` |
| [`tests/check_doxygen.cmake`](../tests/check_doxygen.cmake) | Doxygen with `_config/Doxyfile` and warnings as errors, when `doxygen` is installed |

### The model stands alone

[`tests/mock_pf1550.hpp`](../tests/mock_pf1550.hpp) is written from the datasheet, not from the driver: it does
not include a driver header. Its register map, field masks, reset values, option table and timing are its own, each
with the datasheet table, so a wrong constant in the driver cannot hide behind the same wrong constant in the model
(handbook KB-TST-030). [`tests/test_model.cpp`](../tests/test_model.cpp) compares the two: every register the driver
may write is one the model does not protect, every register the driver does not document is protected in the model,
and the model's reset values match Table 85.

When a test fails here, one of the two maps is wrong: check it against the table the model names, and fix that one.
A new register goes into both, with its check. Every behaviour found on hardware becomes a knob of the model and a test
that fails on the code before the fix; the model has a virtual clock (`Elapse()`, driven by the mock port's
`DelayUs()`) for the DVS-done delay.

## Fuzz tests

Everything the driver reads comes from the chip over the bus, so the data read is what a noisy cable, a wrong part
or a broken chip controls.
[`tests/fuzz/fuzz_pf1550.cpp`](../tests/fuzz/fuzz_pf1550.cpp) lets libFuzzer choose a start-up option, a configuration
(sometimes unsafe on purpose), then up to 64 driver calls, and on every bus transfer what the bus does: pass it to the
model, fail it, flip bits of the data read or replace it (and lose an acknowledged write). A corrupted read never
repeats the corrupted value of the read before it from the same register: noise is not repeatable, and what does
repeat on a real bus is a line stuck at one value: the fuzzer can stick one of the four protection registers
(BATFET_CNFG, CHG_OPER, CHG_TMR, THM_REG_CNFG) at a chosen value and free it again, and the driver must not write a
protection off from it. The unit tests inject stuck and flipped reads with `MockBusState` as well. A second
configuration byte accepts single acknowledgements, so that each downgrade is tried alone. Between calls it changes the
world: a supply, an ONKEY press, a fault, any register poked to any value, a raised interrupt, a reset. After every call
it checks the driver's promises and aborts, naming the line, when one breaks:

- a failed call returns an `Error` the enum defines;
- before `EnsureInitialized()` no call makes a transfer, apart from the calls that work once DEVICE_ID has
  passed (switching the charger off, switching a rail off, the off step of a restart, the as-found check,
  cancelling a shutdown request);
- the driver never writes a read-only, key-protected or undocumented register;
- no charge voltage or current written exceeds the `Limits`;
- no write newly switches off a protection the application did not acknowledge: battery overcurrent protection
  (BATFET_OC = 0), the fast-charge timer, the thermistor, DISBATFET; the reset paths and the undervoltage threshold
  change only with their acknowledgements; no write carries both shutdown commands;
- a rail enable newly written (any of the six rails, in the DVS register of the state enabled) finds the rail's held
  voltage inside its `Limits` window unless a line is stuck on that register, and the load switch bit is set only
  during `EnableLoadSwitch()`;
- after `DeviceReset` the driver is uninitialised;
- SHIP and CORE_OFF are entered only after the matching call returned `Ok`;
- after a limit-checking call that returned `Ok` with a clean bus (`ServiceChargerWatchdog()`, `ReadChargerStatus()`,
  a `ServiceInterrupts()` that reported a charger event or owed a check) and the charger in Charge, the input limit
  is the configured one when the charger block is applied and VBUS_SNS shows VBUSIN valid (the target decodes
  VBUS_SNS itself, Table 153), else a valid limit inside `Limits`. A check is owed once the latch has shown a
  charger byte (a `ReadRegister()` of CHG_INT, or a service that failed after reading it), whatever
  `ClearLatchedEvents()` did; the next clean `ServiceInterrupts()` must have passed it;
- a call that returns `DeviceReset` wrote no input limit;
- a bit the last call set and verified is a sentinel: when the chip then reads differently there (a reset, or any
  other writer), `SelfCheck()` does not return `Ok`;
- `ClearLatchedEvents()` makes no transfer and leaves the latch empty;
- every log line is a non-empty plain-text message.

The input limit and reset-seen invariants ignore a call during which a line was stuck (at its start or its end): two
agreeing reads of a stuck line are what the driver trusts, by design.

```bash
# libFuzzer needs Clang (on macOS, Homebrew's LLVM)
cmake -S . -B build-fuzz -DCMAKE_CXX_COMPILER=clang++ -DPF1550_BUILD_TESTS=ON -DPF1550_BUILD_FUZZERS=ON
cmake --build build-fuzz --target fuzz_pf1550
./build-fuzz/tests/fuzz/fuzz_pf1550 -max_total_time=120 tests/fuzz/corpus/fuzz_pf1550
```

The same file also builds without libFuzzer as `fuzz_replay_fuzz_pf1550`, and CTest replays every input in
[`tests/fuzz/corpus/fuzz_pf1550/`](../tests/fuzz/corpus/fuzz_pf1550) on GCC and Clang
(`pf1550_fuzz_pf1550_corpus`): the seeds, and every input that ever broke a promise (name it `regress-*`), so a
fixed bug stays fixed. CI fuzzes for 120 s on every pull request, from the seeds plus a corpus it keeps between runs
(job `Host quality / fuzz`).

## Quality gates

The `Host quality` job measures the library code, `inc/` and `src/` only (tests and examples are left out), and
fails when a value is over the limits of this repository's assurance level, `high` (set in
[`.kokeb/handbook.yml`](../.kokeb/handbook.yml), defined by the handbook's ADR-0020):

| Gate | Tool and how it is measured | Limit at `high` |
| --- | --- | --- |
| Complexity | lizard 1.24.1, modified cyclomatic complexity (`-m`: a `switch` counts once) per function | CCN at most 15 |
| Function length | lizard, lines of code per function | at most 60 |
| Parameters | lizard, per function | at most 6 |
| Line coverage | GCC 14 `--coverage -O0`, every CTest, gcovr 8.7 `--merge-lines` (a template's lines count once across its instances), `--exclude-unreachable-branches --exclude-throw-branches` | at least 85 % |
| Branch coverage | same run | at least 75 % |
| MC/DC | GCC `-fcondition-coverage`, same run | measured, not gated at `high` (100 % at `safety`) |
| Sanitizers | Clang, `-fsanitize=address,undefined -fno-sanitize-recover=all`, every CTest | no report |
| Fuzzing | libFuzzer with ASan and UBSan, 120 s per target | no broken promise |

The reports are in the run's `host-quality-reports` artifact and its step summary. To run the gates locally:

```bash
# Coverage (GCC 14 or later), then the complexity limits
cmake -S . -B build-cov -DCMAKE_CXX_COMPILER=g++-14 -DPF1550_BUILD_TESTS=ON \
  -DCMAKE_CXX_FLAGS="--coverage -O0 -fcondition-coverage"
cmake --build build-cov && ctest --test-dir build-cov
gcovr --root . --filter inc/ --filter src/ --merge-lines --exclude-unreachable-branches --txt build-cov
lizard -m -C 15 -L 60 -a 6 inc src

# Sanitizers (Clang)
cmake -S . -B build-san -DCMAKE_CXX_COMPILER=clang++ -DPF1550_BUILD_TESTS=ON \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-sanitize-recover=all -O1 -g"
cmake --build build-san && ctest --test-dir build-san --output-on-failure
```

The fuzz gate runs locally as shown in [Fuzz tests](#fuzz-tests).

clang-tidy 20 ([`_config/.clang-tidy`](../_config/.clang-tidy): bugprone, CERT, clang-analyzer,
cppcoreguidelines, modernize, performance, portability and readability, each disabled check with its reason) and
cppcheck (strict) run in the `C++ lint` and `Repository checks` jobs and fail on any finding.

The `MISRA` job ([`ci-misra.yml`](../.github/workflows/ci-misra.yml)) runs the shared `ru-misra` workflow of
ci-tools at level `high`: it traces a host build and the Cortex-M build (`pf1550_optimised_build`) of every
translation unit, checks them against MISRA C++:2023 and the handbook's C++20 profile (the CodeQL coding-standards
and profile packs, plus the validated clang-tidy and cppcheck checks), and reports each finding against the
handbook's compliance matrix and deviation records. A finding is fixed in the code; a deliberate exception names
its deviation on the line before it (`// codeql::misra_deviation_next_line(MD-CPP-009)` above the `.ipp` include
of `inc/pf1550.hpp`). Until the repository is public the job runs with `open_source_prerelease: true`, the
declaration that the code is released under the licence in `LICENSE` and is to be published.

## Documentation snippets

Every C++ block on this site is included from [`tests/docs/`](../tests/docs/my_port.hpp), which the host build
compiles (target `pf1550_doc_snippets`) with the driver instantiated over the documented port. A change to
the API that breaks a page breaks the build first. The README's example is a generated copy of the same file;
`kokeb-docs check` fails when it drifts.

## CI workflows

Everything runs from [`ci.yml`](../.github/workflows/ci.yml), which calls one workflow file per check and ends in
the `CI gate` job, the single required check (`CI gate / gate`).

| Job | What it checks |
| --- | --- |
| Host tests | GCC, then Clang (one job), build and run every test above, then build and run `examples/host` on its own |
| Host quality | The gates above: complexity, coverage, sanitizers, fuzzing |
| Cross-compile | `arm-none-eabi-g++` for Cortex-M0+, M4F and M7F with the strict flags, warnings as errors (nothing linked) |
| C++ lint | clang-format and clang-tidy over the sources |
| MISRA | MISRA C++:2023 and the C++20 profile on traced host and Cortex-M builds, at level `high` (see [Quality gates](#quality-gates)) |
| Docs | Strict site build (`_config/mkdocs.yml`), figure and generated-region drift, Doxygen (`_config/Doxyfile`), offline link check, Lighthouse |
| Repository checks | One job: DCO on pull requests, the org standard, Markdown and YAML lint, the [Kokeb Labs handbook](https://github.com/kokeb-labs/handbook) rules for `.kokeb/handbook.yml` (by rule ID), template drift (warns only) and cppcheck over the driver sources; a second job runs the handbook's source checks (include guards, bus requirements, raw access) |
| CI gate | Passes only if every job above passed: the one required check |

The **ESP32 examples** job builds every app in `examples/esp32/app_config.yml`, for each listed ESP-IDF version and
build type, with the repository root as the driver's component, and lints the example sources. The gate needs it
too.

The workflows call the shared, versioned workflows of
[kokeb-labs/ci-tools](https://github.com/kokeb-labs/ci-tools) at `@v1`.

## On hardware

Nothing in this driver has been verified on hardware. An MCU example ([Examples](examples.md)) is the bench test: it
configures a charger and an LDO and logs their state once a second.
[Datasheet map](datasheet.md#what-to-verify-on-the-bench) lists the bench step for each behaviour that rests on the
datasheet text alone. Record the board and the result in the
pull request's bench-status section.
