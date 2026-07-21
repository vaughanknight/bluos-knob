from dataclasses import replace
from decimal import Decimal

import pytest

from bluos_knob.device_input.contracts import (
    DeviceIdentity,
    HidDeviceInfo,
    NormalizedAction,
    NormalizedKnobEvent,
    TransportMode,
)
from bluos_knob.platform_adapter.hidapi_reader import PlatformHidError
from scripts.bluos_daemon import (
    DaemonConfig,
    _check_memory_watchdog,
    discover_anticater_paths,
    execute_command_for_daemon,
    open_reader_with_retry,
    planned_command,
    reopen_idle_reader,
)

CONFIG = DaemonConfig(
    anticater_path="fake",
    bluos_host="192.168.1.67",
    bluos_port=11000,
    step_db=Decimal("1"),
    max_db=Decimal("-24"),
    source_safe_db="-40",
    optical_input_type_index="optical-2",
    spotify_source_name="Spotify",
    timeout_ms=1000,
    request_timeout=5,
    reconnect_delay=0,
    idle_reopen_timeouts=30,
    absent_poll_delay=0,
    max_rss_mb=0,
    log_file=None,
    log_max_bytes=5_000_000,
    log_backup_count=3,
    dry_run=True,
)


def test_given_volume_up_event_when_planned_then_bounded_positive_step_is_used():
    """
    Test Doc:
    - Why: The live daemon should map knob turns to guarded BluOS dB steps.
    - Contract: volume_up plans a +step_db command, not an absolute loudness jump.
    - Usage Notes: Execution still checks max dB against current amplifier state.
    - Quality Contribution: Prevents accidental unbounded volume increases.
    - Worked Example: volume_up -> volume_step +1.
    """
    assert planned_command(_event(NormalizedAction.VOLUME_UP), CONFIG) == {
        "kind": "volume_step",
        "step_db": "1",
    }


def test_given_volume_down_event_when_planned_then_negative_step_is_used():
    """
    Test Doc:
    - Why: Turning down should always request a quieter relative dB change.
    - Contract: volume_down plans a negative step.
    - Usage Notes: This is separate from source safety volume.
    - Quality Contribution: Ensures knob direction is not inverted.
    - Worked Example: volume_down -> volume_step -1.
    """
    assert planned_command(_event(NormalizedAction.VOLUME_DOWN), CONFIG)["step_db"] == "-1"


def test_given_brightness_events_when_planned_then_source_shortcuts_include_safe_db():
    """
    Test Doc:
    - Why: Hold-rotate gestures are source shortcuts with post-switch safety volume.
    - Contract: brightness_down selects Optical; brightness_up resumes Spotify.
    - Usage Notes: Both include -40 dB safe target.
    - Quality Contribution: Prevents loud source changes.
    - Worked Example: brightness_down -> optical-2, brightness_up -> Spotify.
    """
    assert planned_command(_event(NormalizedAction.BRIGHTNESS_DOWN), CONFIG) == {
        "kind": "source_optical",
        "input_type_index": "optical-2",
        "safe_db": "-40",
    }
    assert planned_command(_event(NormalizedAction.BRIGHTNESS_UP), CONFIG) == {
        "kind": "source_spotify",
        "source": "Spotify",
        "safe_db": "-40",
    }


def test_given_release_event_when_planned_then_it_is_ignored():
    """
    Test Doc:
    - Why: The real Anticater emits release/no-op reports between actions.
    - Contract: no_op does not send any BluOS command.
    - Usage Notes: Unknown reports are also ignored by the daemon loop.
    - Quality Contribution: Prevents repeated release reports from changing volume.
    - Worked Example: no_op -> ignore.
    """
    assert planned_command(_event(NormalizedAction.NO_OP), CONFIG) == {"kind": "ignore"}


def test_given_volume_up_at_max_when_executed_then_daemon_logs_refusal(monkeypatch):
    """
    Test Doc:
    - Why: The daemon must keep running when a volume-up event hits the max dB guard.
    - Contract: The daemon command wrapper returns an unexecuted command_refused result.
    - Usage Notes: The live loop logs this and continues reading HID reports.
    - Quality Contribution: Prevents the process from dying at the safety ceiling.
    - Worked Example: -24 + 1 with max -24 is refused.
    """
    monkeypatch.setattr(
        "scripts.bluos_daemon._fetch_summary",
        lambda config, command: {"db": "-24", "mute": "0", "volume": "95"},
    )

    result = execute_command_for_daemon(
        {"kind": "volume_step", "step_db": "1"},
        replace(CONFIG, dry_run=False),
    )

    assert result["executed"] is False
    assert result["reason"] == "command_refused"
    assert "would exceed max -24" in result["message"]


def test_given_auto_path_when_device_reenumerates_then_current_anticater_path_is_discovered():
    """
    Test Doc:
    - Why: macOS changes the Anticater DevSrvsID after sleep/reconnect.
    - Contract: The daemon can discover the current Anticater path instead of
      retrying only a stale path.
    - Usage Notes: Consumer-control usage page 12 is preferred when present.
    - Quality Contribution: Lets the daemon recover after Mac sleep.
    - Worked Example: auto discovers DevSrvsID:new.
    """
    platform = _Platform(
        [
            HidDeviceInfo(
                DeviceIdentity(product="ANTICATER_MINI", path="DevSrvsID:new", usage_page=12)
            ),
            HidDeviceInfo(
                DeviceIdentity(product="ANTICATER_MINI", path="DevSrvsID:other", usage_page=1)
            ),
        ]
    )

    assert discover_anticater_paths(platform, "auto")[:2] == [
        "DevSrvsID:new",
        "DevSrvsID:other",
    ]


def test_given_sustained_idle_when_reopened_then_old_reader_is_closed_and_current_path_is_used():
    """
    Test Doc:
    - Why: macOS sleep can leave HID reads timing out forever without raising.
    - Contract: The daemon can proactively close an idle handle and reopen the
      currently discovered Anticater path.
    - Usage Notes: The live loop triggers this after idle-reopen-timeouts.
    - Quality Contribution: Restores knob control after sleep/wake without a
      manual daemon restart.
    - Worked Example: idle stale reader -> close -> open DevSrvsID:new.
    """
    old_reader = _Reader()
    platform = _OpeningPlatform(
        devices=[
            HidDeviceInfo(
                DeviceIdentity(product="ANTICATER_MINI", path="DevSrvsID:new", usage_page=12)
            )
        ]
    )

    new_reader = reopen_idle_reader(
        platform, replace(CONFIG, anticater_path="auto"), old_reader, idle_reads=30
    )

    assert old_reader.closed is True
    assert platform.opened_paths == ["DevSrvsID:new"]
    assert new_reader is platform.reader


def test_given_idle_reopen_when_cached_path_still_opens_then_no_reenumerate():
    """
    Test Doc:
    - Why: hid.enumerate() is the expensive/leaky native call; reopening fires on a
      timer while the knob is idle, so it must not re-enumerate every cycle.
    - Contract: reopen reuses the last-opened path and skips enumeration when it opens.
    - Usage Notes: Falls back to a full scan only if the cached path fails.
    - Quality Contribution: Removes the per-reopen enumerate leak behind the OOM growth.
    - Worked Example: reader at DevSrvsID:a -> reopen opens DevSrvsID:a, enumerate never called.
    """
    old_reader = _Reader(path="DevSrvsID:a")
    platform = _OpeningPlatform(devices=[])

    new_reader = reopen_idle_reader(
        platform, replace(CONFIG, anticater_path="auto"), old_reader, idle_reads=300
    )

    assert old_reader.closed is True
    assert platform.opened_paths == ["DevSrvsID:a"]
    assert platform.enumerate_calls == 0
    assert new_reader is platform.reader


def test_given_idle_reopen_when_cached_path_dead_then_falls_back_to_enumerate():
    """
    Test Doc:
    - Why: After sleep/wake the Bluetooth DevSrvsID can change, so the cached path dies.
    - Contract: When the cached path no longer opens, reopen re-enumerates and opens the
      currently discovered path.
    - Usage Notes: The failed cached open is logged as hid_open_error, then the scan runs.
    - Quality Contribution: Preserves sleep/wake recovery without enumerating every cycle.
    - Worked Example: stale DevSrvsID:a fails -> enumerate -> open DevSrvsID:new.
    """
    old_reader = _Reader(path="DevSrvsID:a")
    platform = _OpeningPlatform(
        devices=[
            HidDeviceInfo(
                DeviceIdentity(product="ANTICATER_MINI", path="DevSrvsID:new", usage_page=12)
            )
        ],
        fail_paths=["DevSrvsID:a"],
    )

    new_reader = reopen_idle_reader(
        platform, replace(CONFIG, anticater_path="auto"), old_reader, idle_reads=300
    )

    assert old_reader.closed is True
    assert platform.opened_paths == ["DevSrvsID:a", "DevSrvsID:new"]
    assert platform.enumerate_calls == 1
    assert new_reader is platform.reader


def test_given_absent_knob_when_opening_then_slow_polls_before_it_reappears(monkeypatch):
    """
    Test Doc:
    - Why: A disconnected knob must not spin a tight enumerate/open loop that burns CPU
      and churns native HID allocations.
    - Contract: When no candidate paths are found, open waits absent_poll_delay before the
      next scan; when the knob reappears it opens normally.
    - Usage Notes: A present-but-unopenable knob still uses the shorter reconnect_delay.
    - Quality Contribution: Caps idle churn while the knob is away.
    - Worked Example: enumerate [] -> sleep 30 -> enumerate [device] -> open.
    """
    sleeps = []
    monkeypatch.setattr("scripts.bluos_daemon.time.sleep", lambda seconds: sleeps.append(seconds))
    device = HidDeviceInfo(
        DeviceIdentity(product="ANTICATER_MINI", path="DevSrvsID:back", usage_page=12)
    )
    platform = _SequencedPlatform(enumerations=[[], [device]])

    reader = open_reader_with_retry(
        platform,
        replace(CONFIG, anticater_path="auto", absent_poll_delay=30, reconnect_delay=2),
    )

    assert sleeps == [30]
    assert platform.opened_paths == ["DevSrvsID:back"]
    assert reader is platform.reader


def test_given_rss_over_limit_when_watchdog_checks_then_it_exits_for_respawn(monkeypatch):
    """
    Test Doc:
    - Why: Any future native leak should self-recover instead of growing until the OS
      OOM-kills the machine (the original ~18 GB failure).
    - Contract: When measured RSS reaches max_rss_mb, the watchdog raises SystemExit so
      launchd KeepAlive respawns a clean process.
    - Usage Notes: max_rss_mb=0 disables the check entirely.
    - Quality Contribution: Bounds worst-case memory growth.
    - Worked Example: 900 MB RSS with a 512 MB limit -> SystemExit.
    """
    monkeypatch.setattr("scripts.bluos_daemon._current_rss_mb", lambda: 900.0)

    with pytest.raises(SystemExit):
        _check_memory_watchdog(replace(CONFIG, max_rss_mb=512))

    # Disabled and under-limit checks must not exit.
    _check_memory_watchdog(replace(CONFIG, max_rss_mb=0))
    monkeypatch.setattr("scripts.bluos_daemon._current_rss_mb", lambda: 10.0)
    _check_memory_watchdog(replace(CONFIG, max_rss_mb=512))


class _Platform:
    def __init__(self, devices):
        self.devices = devices

    def enumerate_devices(self):
        return self.devices


class _SequencedPlatform:
    def __init__(self, enumerations):
        self._enumerations = list(enumerations)
        self.reader = _Reader()
        self.opened_paths = []

    def enumerate_devices(self):
        if len(self._enumerations) > 1:
            return self._enumerations.pop(0)
        return self._enumerations[0]

    def open(self, path):
        self.opened_paths.append(path)
        return self.reader


class _Reader:
    def __init__(self, path=None):
        self.closed = False
        self.path = path

    def close(self):
        self.closed = True


class _OpeningPlatform:
    def __init__(self, devices, fail_paths=()):
        self.devices = devices
        self.reader = _Reader()
        self.opened_paths = []
        self.enumerate_calls = 0
        self._fail_paths = set(fail_paths)

    def enumerate_devices(self):
        self.enumerate_calls += 1
        return self.devices

    def open(self, path):
        self.opened_paths.append(path)
        if path in self._fail_paths:
            raise PlatformHidError(f"failed to open HID path {path!r}")
        return self.reader


def _event(action: NormalizedAction) -> NormalizedKnobEvent:
    return NormalizedKnobEvent(
        action=action,
        magnitude=1,
        source_device=DeviceIdentity(path="fake"),
        sequence=1,
        raw_report_id=0,
        raw_data_hex="",
        transport=TransportMode.UNKNOWN,
        connection_state="connected",
    )
