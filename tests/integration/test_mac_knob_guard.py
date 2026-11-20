"""Mac knob guard: unpairs the Anticater knob from the Mac so it stays with the ESP32 bridge."""

from __future__ import annotations

import json
import os
import stat
import subprocess
from pathlib import Path

SCRIPT = Path(__file__).resolve().parents[2] / "scripts" / "mac_knob_guard.sh"
KNOB = {"address": "ee-aa-b8-d6-82-5e", "name": "ANTICATER_MINI", "connected": True}
OTHER = {"address": "e0-eb-40-f1-43-3e", "name": "Magic Trackpad 2", "connected": True}


def fake_blueutil(tmp_path: Path, paired: list[dict]) -> tuple[Path, Path]:
    """A behaviour fake of blueutil: answers --paired from a fixture and records mutating calls."""
    calls = tmp_path / "calls.log"
    fake = tmp_path / "blueutil"
    fake.write_text(
        "#!/bin/sh\n"
        f"echo \"$@\" >> '{calls}'\n"
        "case \"$1\" in\n"
        f"  --paired) echo '{json.dumps(paired)}' ;;\n"
        "esac\n"
    )
    fake.chmod(fake.stat().st_mode | stat.S_IEXEC)
    return fake, calls


def run_guard(fake: Path) -> subprocess.CompletedProcess[str]:
    env = {**os.environ, "BLUEUTIL": str(fake)}
    return subprocess.run(
        ["/bin/sh", str(SCRIPT)], env=env, capture_output=True, text=True, check=False
    )


def mutating(calls: Path) -> list[str]:
    if not calls.exists():
        return []
    return [c for c in calls.read_text().splitlines() if not c.startswith("--paired")]


def test_given_knob_paired_to_mac_when_guard_runs_then_unpairs_only_the_knob(tmp_path):
    """
    Test Doc:
    - Why: the Mac re-pairs the knob at times and steals it from the ESP32 bridge.
    - Contract: when the knob (by address or ANTICATER name) is paired, the guard disconnects then
      unpairs exactly that device and logs one JSON line; other devices are never touched.
    - Usage Notes: BLUEUTIL overrides the blueutil binary (tests and non-Homebrew installs).
    - Quality Contribution: catches the guard touching the wrong device or doing nothing.
    - Worked Example: paired [knob, trackpad] -> calls --disconnect ee-aa-…, --unpair ee-aa-…
    """
    fake, calls = fake_blueutil(tmp_path, [OTHER, KNOB])
    result = run_guard(fake)
    assert result.returncode == 0
    assert mutating(calls) == ["--disconnect ee-aa-b8-d6-82-5e", "--unpair ee-aa-b8-d6-82-5e"]
    assert json.loads(result.stdout.strip().splitlines()[-1])["guard"] == "unpaired"


def test_given_knob_not_paired_when_guard_runs_then_touches_nothing(tmp_path):
    """
    Test Doc:
    - Why: runs every 10 s; it must be silent and harmless when there is nothing to do.
    - Contract: no knob in the paired list -> no mutating blueutil call, no output, exit 0.
    - Usage Notes: launchd logs stdout, so silence keeps the log small.
    - Quality Contribution: catches noisy logs and accidental unpairs.
    - Worked Example: paired [trackpad] -> calls only --paired.
    """
    fake, calls = fake_blueutil(tmp_path, [OTHER])
    result = run_guard(fake)
    assert result.returncode == 0
    assert mutating(calls) == []
    assert result.stdout == ""


def test_given_knob_renamed_but_same_address_when_guard_runs_then_still_unpaired(tmp_path):
    """
    Test Doc:
    - Why: macOS can show the knob without its name (as the ESP32 saw after bonding).
    - Contract: matching is by address OR by an ANTICATER name.
    - Usage Notes: address is the knob's BLE identity (ee-aa-b8-d6-82-5e).
    - Quality Contribution: catches name-only matching.
    - Worked Example: {address: ee-aa-…, name: ""} -> unpaired.
    """
    fake, calls = fake_blueutil(tmp_path, [{**KNOB, "name": ""}])
    run_guard(fake)
    assert "--unpair ee-aa-b8-d6-82-5e" in mutating(calls)


def test_given_blueutil_cannot_list_devices_when_guard_runs_then_reports_error(tmp_path):
    """
    Test Doc:
    - Why: under launchd, missing blueutil or a missing Bluetooth permission would otherwise
      look like "nothing to do" forever.
    - Contract: a failing --paired query logs one {"guard":"error"} line and exits
      non-zero; nothing is unpaired.
    - Usage Notes: visible via just mac-guard-status.
    - Quality Contribution: catches a guard that silently never works.
    - Worked Example: blueutil exits 1 -> {"guard":"error",...}, exit 1.
    """
    fake = tmp_path / "blueutil"
    fake.write_text("#!/bin/sh\necho 'no permission' >&2\nexit 1\n")
    fake.chmod(fake.stat().st_mode | stat.S_IEXEC)
    result = run_guard(fake)
    assert result.returncode == 1
    assert json.loads(result.stdout.strip())["guard"] == "error"
