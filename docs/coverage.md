# Coverage

How much of the PF1550 the driver implements, models and tests. The datasheet (Rev. 7) is covered feature by feature
in a per-parameter matrix of 1090 rows; this page gives its totals and what each status means. Every row has a status:

| Status | Meaning |
| --- | --- |
| OK | The API decision, the model's behaviour and a test are all present for the behaviour |
| INFO | An explicit decision that there is nothing to model or test beyond storage or documentation: an analog value, a bit the driver never programs, a reserved field |
| TODO | A gap: the datasheet does not support it, or the driver does not do it yet |

**Totals: 1090 rows; OK 729, INFO 360, TODO 1; covered (OK + INFO) 1089 of 1090.**

| Group | Rows | OK | INFO | TODO |
| --- | --- | --- | --- | --- |
| Registers (the 90 documented registers) | 90 | 85 | 5 | 0 |
| Bit fields | 357 | 248 | 109 | 0 |
| Interrupt sources | 31 | 31 | 0 | 0 |
| PMIC and charger states, transitions, status codes | 42 | 42 | 0 | 0 |
| Electrical characteristics and ratings (Tables 3 to 26, per parameter) | 272 | 109 | 163 | 0 |
| Start-up and power-down timing (Table 84) | 10 | 5 | 5 | 0 |
| Digital pin levels, debounce and polarity (Tables 73 to 82) | 25 | 13 | 12 | 0 |
| Pins (Table 2) | 41 | 7 | 34 | 0 |
| OTP start-up values (Table 85, per parameter) | 46 | 46 | 0 | 0 |
| OTP image registers (Table 176) | 28 | 28 | 0 | 0 |
| OTP image fields (Table 176) | 102 | 75 | 26 | 1 |
| Functional-chapter tables (Tables 27 to 72) | 46 | 40 | 6 | 0 |

## What changed since the first matrix

The first matrix left 43 rows as TODO: the power-up and power-down sequencer and the start-up timing were not modelled,
the power-down order registers were stored only, and the coin cell charge had no effect in the model. The datasheet
defines behaviour for all of them, so they are now implemented, modelled and tested:

| Rows | Now |
| --- | --- |
| `*_PWRDN_SEQ` registers and fields (14) | `ReadPowerDownSequence()`, `SetPowerDownSequence()` (acknowledgement), `PowerDownDelayUs()`; the model mirrors the power-up order after reset (section 10.7) and runs the schedule |
| `OTP_*_PWRUP_SEQ`, `OTP_SEQ_CLK_SPEED` and their image fields (16) | `StartupProfileOf(Option)` for A1 to A9, from Table 85; the model's option table is its own copy |
| Table 84 timing (10) | `kVsnvsTurnOnDelayUs`, `kSlotFastUs`, `kSlotSlowUs`, `kPowerDownStepUs`, `ColdStartToResetReleaseUs()`; the rise times and the user-determined delay `tD2` are analog or the application's and are INFO rows |
| `COINCELL_CONTROL`, `COINCHEN` (2) | `CoinCell`, with the model charging LICELL to `VCOIN` while `COINCHEN` is set |

The one row left is the OTP image read-back window (`OF-RB`): the public datasheet does not document it, so the driver
cannot read the fuses over I2C. [Datasheet map](datasheet.md#what-is-not-covered) says how to close it.

## Where the rows are

The per-parameter matrix (register, bit field, interrupt source, state, electrical parameter, timing, pin, OTP value) names
for each row the datasheet table and page, the driver API, the model behaviour, the test suite and the test files. It is
in this repository as [`coverage-matrix.csv`](coverage-matrix.csv), with a generated page,
[Coverage matrix](coverage-matrix.md). `python3 tools/coverage_matrix.py check` (also a CTest,
`pf1550_coverage_matrix`) verifies that its totals equal the ones on this page, that every API name in it exists in
`inc/` or `src/`, and that every test file it names exists. The pages that explain the same ground:

| You want | Read |
| --- | --- |
| One datasheet fact, its API, model behaviour and test | [Coverage matrix](coverage-matrix.md) |
| Which datasheet section the driver relies on, and which API implements it | [Datasheet map](datasheet.md) |
| The register map with each register's access type | [`inc/pf1550_registers.hpp`](../inc/pf1550_registers.hpp) |
| The places where the datasheet is silent or inconsistent | [Ambiguities](ambiguities.md) |
| What the tests check | [Testing and CI](testing.md) |
