# Start Here

You do not need to understand GitHub's folders to install Cache Firmware.

## Install or update Cache Firmware

Open the **[Releases page](https://github.com/Bobabate/Cache-Firmware/releases)**.
That is where finished firmware files live. Do not use GitHub's green **Code**
button; it downloads source code, not installable firmware.

Choose the files labelled for your board:

- **MeshPocket:** UF2 for a full USB installation; ZIP for supported updates.
- **Heltec V3:** BIN for an application update; merged BIN for a clean install.

## Configure Cache Firmware

Use [config.meshcore.io](https://config.meshcore.io) and change the initial
administrator and room passwords before deployment.

## Important project files

- `README.md` — project overview and basic instructions
- `CACHE.md` — Cache Firmware design requirements
- `CHANGELOG.md` — changes by version
- `examples/simple_room_server/` — Room Server and Cache display source
- `variants/mesh_pocket/platformio.ini` — MeshPocket target
- `variants/heltec_v3/platformio.ini` — Heltec V3 target

When in doubt, return to the
**[Releases page](https://github.com/Bobabate/Cache-Firmware/releases)**.
