# Changelog

## Unreleased

- Remove the installation-specific `Recursive Cache` node name from firmware
  defaults. Clean installs now use each board's upstream Room Server name until
  the owner configures the actual cache name; configured names still appear on
  the display.

## 1.17.1-C1 - 2026-08-20

- Start Cache Firmware from upstream MeshCore v1.17.1 Room Server.
- Add a dedicated Heltec MeshPocket Room Server target named Recursive Cache.
- Add a dedicated Heltec WiFi LoRa 32 V3 Room Server target.
- Retain standard Room Server behaviour, radio defaults, and initial passwords.
- Add a manual GitHub Actions test-build workflow producing UF2 and ZIP
  packages for MeshPocket and BIN packages for Heltec V3, with tagged releases.
- Add a persistent MeshPocket screen showing the complete firmware version,
  configured cache name, and battery percentage and voltage.
