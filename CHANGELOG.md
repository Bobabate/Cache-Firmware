# Changelog

## 1.17.1-C2 - 2026-08-20

- Remove the installation-specific `Recursive Cache` node name from firmware
  defaults. Clean installs now use each board's upstream Room Server name until
  the owner configures the actual cache name; configured names still appear on
  the display.
- Add MeshPocket-only cache interaction: persist up to 100 posts using
  alternating checksummed snapshots, suppress automatic live post fan-out, and
  show a temporary instruction message followed by three posts at a time.
- Add MeshPocket post navigation commands: `!older`, `!newer`, and `!latest`.
- Require direct, zero-hop radio access for the MeshPocket cache interaction.
- Add persistent optional RSSI calibration with `rssi near`, `rssi far`,
  `rssi`, and USB-only `rssi reset` commands.
- Leave the Heltec V3 target on the existing C1 Room Server behaviour.

## 1.17.1-C1 - 2026-08-20

- Start Cache Firmware from upstream MeshCore v1.17.1 Room Server.
- Add a dedicated Heltec MeshPocket Room Server target named Recursive Cache.
- Add a dedicated Heltec WiFi LoRa 32 V3 Room Server target.
- Retain standard Room Server behaviour, radio defaults, and initial passwords.
- Add a manual GitHub Actions test-build workflow producing UF2 and ZIP
  packages for MeshPocket and BIN packages for Heltec V3, with tagged releases.
- Add a persistent MeshPocket screen showing the complete firmware version,
  configured cache name, and battery percentage and voltage.
