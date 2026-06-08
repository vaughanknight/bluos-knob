from dataclasses import replace
from decimal import Decimal

from bluos_knob.device_input.contracts import (
    DeviceIdentity,
    HidDeviceInfo,
    NormalizedAction,
    NormalizedKnobEvent,
    TransportMode,
)
from scripts.bluos_daemon import (
    DaemonConfig,
    discover_anticater_paths,
    execute_command_for_daemon,
    planned_command,
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


class _Platform:
    def __init__(self, devices):
        self.devices = devices

    def enumerate_devices(self):
        return self.devices


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
