# Fixture format

Fixtures are newline-delimited JSON. Each row preserves raw HID evidence and
enough metadata to replay without hardware.

Required fields:

| Field | Meaning |
|---|---|
| `schema_version` | Fixture schema version. Current value: `1`. |
| `timestamp` | Capture timestamp as data; replay does not sleep or reproduce timing. |
| `sequence` | Ordering token/counter when known. |
| `report_id` | HID report ID. |
| `data` | Raw report payload as a lowercase hex string. |
| `transport` | `usb`, `bluetooth`, `2.4ghz`, or `unknown`. |
| `connection_state` | `connected`, `disconnected`, or `unknown`. |
| `device` | HID identity metadata. |
| `operation_label` | Optional human label such as `rotate-right` or `click`. |
| `notes` | Optional capture notes. |

`device.path` and `device.serial_number` are local identity fields. Redact them
before sharing fixtures publicly.

Replay command:

```bash
python -m bluos_knob.cli.main replay --fixture tests/fixtures/anticater/sample_reports.jsonl
```

