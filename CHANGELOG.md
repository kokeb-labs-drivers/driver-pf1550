# Changelog

All notable changes to this repository are listed here. Versions follow [Semantic Versioning](https://semver.org/).
Entries under **Breaking** need source changes in code that used the affected API; any of them calls for a major
version bump (a minor bump before 1.0).

## [Unreleased]

### Added

- First version of the PF1550 driver (NXP PF1550 PMIC, I2C): typed, limit-checked and read-back configuration of the
  charger, the three buck regulators, the three LDOs, VREFDDR, VSNVS, the coin cell charger, the USBPHY LDO and the
  charger LED; the state machine, SHIP and CORE_OFF behind confirmation tokens; the power-down order behind an
  acknowledgement; the interrupt groups with clear-after-copy; and the power-up and power-down sequencer as `constexpr`
  data for the pre-programmed options A1 to A9.
- Mandatory `Limits` and `Acknowledgements`; no public raw register write.
- A host-test model of the chip, a fuzz target, compile-fail tests, a host example and ESP-IDF examples (configure and
  charge, interrupts and the charger watchdog).
- Not verified on hardware.
- Ninth review: a CHG_INT byte read by `ReadRegister()` also owes the input limit check (the flag is raised where
  every status byte is noted, so the next `ServiceInterrupts()` restores the limit; `tests/test_round10.cpp`); the
  fuzz target drops its last exemption (a raw CHG_INT read) and owes the check for every charger byte the latch shows;
  the round-9 sweeps take their bound from a clean run of the same call; a corpus input walks charging, a cable
  removal with its interrupt, a failure at the second-pass INT_CATEGORY read, `ClearLatchedEvents()` and a clean
  service, so the corpus replay finds a lost pending check; contract and testing pages list the INTB shortcut's
  pending-check condition and every fuzz promise.
- Eighth review: a charger interrupt read by a `ServiceInterrupts()` call that then failed (a later pass, the clear
  write) owes the input limit check, so the next successful call restores a limit that went back to its OTP value,
  with or without `ClearLatchedEvents()` between; `tests/test_round9.cpp` fails each transfer of the call in turn and
  pins the five untested guards (mask write failure, latch kept after an error, failed switch-off is `Bus`,
  `ClearLatchedEvents()` empties the latch, a failed VBUS_SNS read is an error); the fuzz target calls
  `ClearLatchedEvents()` and no longer exempts a failed service from the limit invariant.
- Fuzz finding after the seventh review: the reset-seen invariant armed bits the driver had not changed (a rail mode
  bit left alone is no sentinel); it now arms only the changed bits (all mode bits for a switch-off), documented and
  pinned by `tests/test_round8.cpp`; the input is in the corpus.
- Seventh review: the idle shortcut of `ServiceInterrupts()` is not taken while an input limit check is pending (a
  `ClearLatchedEvents()` between a failed and the next call no longer drops it); the `ServiceInterrupts()` header says
  "no source masked"; the VBUS operating range follows Table 5 (UVLO rising 3.8 to 4.2 V); the unseen-reset behaviour of
  the status and interrupt paths is documented; the fuzz target asserts the input limit invariant after
  `ReadChargerStatus()` and `ServiceInterrupts()` with its own VBUS_SNS decode, and no limit write in a call that
  returns `DeviceReset`; its invariants ignore a line that was stuck during the call.
- Sixth review: the input limit restore of `ReadChargerStatus()` and `ServiceInterrupts()` looks for a chip reset first
  (`DeviceReset`, nothing written); a failed check in `ServiceInterrupts()` stays pending until it passes; the
  `(void)` casts of results in the tests are `static_cast<void>`; the remaining comparisons under `&&` and `||` in the
  examples and compile-fail tests are parenthesised; ambiguity C60 (Table 163 default 400 mA against OTP 1500 mA).
- Fifth review: the input limit check after a cable removal also runs from `ReadChargerStatus()` and
  `ServiceInterrupts()` (on a charger interrupt), so it does not need `watchdog.enable`, and from
  `ServiceChargerWatchdog()` before the BATFET_CNFG checks; while VBUSIN is invalid the limit field is held in reset and
  the restore waits for a valid supply (no `VerifyFailed` on every call); a configured limit is kept with LinearOnly
  too; a DEVICE_ID line stuck at 0x00 is a `Bus` error and does not take the switch-off away; tests for a limit inside
  `Limits` that is not the configured one, for a refused charge start with an unacknowledged protection off and for
  single reads; the error-name test covers every enumerator.
- Fourth review: with the charger in Charge, `ServiceChargerWatchdog()` reads the input limit a cable removal reverts
  and writes the configured one again, or switches the charger off with `OutOfLimits`; `SelfCheck()` reports
  `input_limit_ok`; a refused charge start leaves the charger off; DEVICE_ID is read twice before another device is
  assumed; the fuzz target injects cable removals; comparisons under `&&` and `||` are parenthesised; the examples show
  the `DeviceReset` recovery.
- Third review: a cable removal (`VBUS_LIN_ILIM` reverts, Table 163) is not reported as a chip reset (`VbusRemoved()`
  in the model); a switch-off is sent even after a re-start that failed on a bus error; the register transfer
  buffers are members of the driver object; the shutdown gates read twice; argument errors send nothing; a rail
  the driver switched off is remembered as off; `Configure()` looks for a reset before the charger block; the fuzz
  target checks that a reset of the chip is seen by `SelfCheck()`.
- Safety fixes from the independent review, each with a model mode and a test: a read that is written back must be
  stable (two reads agree, no all-ones) and every typed write is compared over the whole byte;
  `ServiceChargerWatchdog()` refuses when BATFET_CNFG reads with the overcurrent protection off; status bits read by
  any call are kept in a latch and reported by `ServiceInterrupts()` (`GetLatchedEvents()`, `ClearLatchedEvents()`); a
  rail is enabled only at a held voltage inside its `Limits` window; the DVS declaration is checked against Table 85
  (`Acknowledgements::dvs_on_unlisted_part` for other parts); an unsafe charger found at start-up can be switched off
  (`SetChargerMode(Off)`, `CheckChargerAgainstLimits()` work once DEVICE_ID has passed); `CancelShutdownRequest()`
  clears GOTO_SHIP and GOTO_CORE_OFF, and the CORE_OFF documentation now says it is armed, not immediate;
  `LoadSwitchConfirm{.input_mv}` carries the LDO's input voltage and is checked against `Limits`; a chip reset in the
  session is detected (a changed register reads at its start-up value again; PWRUP_I is not a reset flag, C59) and
  reported as `Error::DeviceReset`; switching off is never refused for a reset, a failed start or an unreadable
  register; DISBATFET found at start-up is reported.
- `PowerConfig` writes its unguarded fields only with their own flags (`write_standby`, `write_debounce`,
  `write_regscp`, `write_low_sys_warn`); `BatfetConfig::oc_without_vbus` defaults to true and false needs
  `allow_batfet_oc_off`; standby or sleep without run is `InvalidArgument`.
- The coverage matrix (1090 rows) is in the repository as `docs/coverage-matrix.csv` with a generated page and a
  check (`tools/coverage_matrix.py`, CTest `pf1550_coverage_matrix`). Ambiguities C57 and C58 added.
- The fuzz target also checks that no write newly switches off a protection (BATFET_OC, the fast-charge timer, the
  thermistor, DISBATFET), that guarded power fields change only with their acknowledgements, that a rail enabled by
  a write held a voltage inside its window, and it runs with read corruption.
