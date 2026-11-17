# Execution log — Phase 1: Knob to screen over BLE

## 2026-11-17 16:25 — Phase start
- Harness: repo not adopted (no `.harness/engineering-harness.md`); boot chore resolved noop → standard testing.
- **Noteworthy**: tk-2101 (BLE spike) runs after the no-hardware tasks (tk-2102..2106), because it requires unpairing the knob from the Mac, which stops the live Mac daemon. Asked Vaughan on tmwk885d1c.

## tk-2102 — scaffold `firmware/` + justfile recipes — BLOCKED
- Wrote `firmware/platformio.ini` (envs `m5stick`: espressif32 / m5stick-c / Arduino + M5Unified + NimBLE-Arduino; `native`: unity), `firmware/src/main.cpp` boot stub, `firmware/.gitignore`, justfile `fw-build`/`fw-test`/`fw-flash`/`fw-monitor` via `uvx --from platformio pio`.
- `just fw-build` → exit 2: uv could not fetch `platformio-6.2.0-py3-none-any.whl.metadata` from files.pythonhosted.org ("Socket is not connected", os error 57).
- Probe: `curl https://files.pythonhosted.org/` → curl 35 Recv failure (×4, 3 s apart); `pypi.org/simple/platformio/` → 200; github.com → 200; dl.registry.platformio.org → 301. DNS resolves to Fastly 151.101.x.223.
- `harness observe` DL-001 recorded.

## Discoveries & Learnings
| # | Tag | Discovery |
|---|-----|-----------|
| 1 | Noteworthy | Spike reordered after no-hardware tasks (knob unpair stops Mac daemon). |
| 2 | Deferred | Toolchain blocked by files.pythonhosted.org reachability; options: `brew install platformio` (global) or wait. |

## 16:35 — unblocked: Vaughan "use brew install"; also "you can disconnect it from the mac (the knob)"
- `brew install platformio` → PlatformIO Core 6.2.0 (/opt/homebrew/bin/pio); justfile `pio := "pio"`. **Noteworthy**: machine-wide install, approved by Vaughan.

## tk-2103 — RED decoder tests — done
- `firmware/test/test_decoder/test_decoder.cpp`: 5 Given-When-Then tests with Test Doc blocks (real capture vectors `03e900…037000`, report-id-less BLE form, release, unmapped/truncated, action names).
- `just fw-test` against a stub: 4 FAILED on assertions, 1 passed (unknown-path) → RED.

## tk-2104 — GREEN decoder — done
- `firmware/lib/knob_core/src/decoder.{h,cpp}`: 16-bit LE usage scan + release rule, table identical to `normalizer.py:7-13`.
- `just fw-test` → 5 test cases: 5 succeeded.

## tk-2102 — scaffold — done
- First `just fw-build` downloaded espressif32@8.0.0 + xtensa toolchain (≈7.5 min, one-off); rebuild → SUCCESS, RAM 12.9% (42 KB static), Flash 61.0% (800 KB of 1.31 MB) with NimBLE-Arduino + M5Unified.
- `git status` shows no `.pio/` (firmware/.gitignore).
