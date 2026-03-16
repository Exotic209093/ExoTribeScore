# ExoTribeScore Beta Notes

## Beta Status

This build is suitable for a controlled beta with a small number of server operators.

This build is still not recommended for a wide or public beta until live ARK and MySQL validation is complete.

## Known Risks

- Live ARK gameplay scenarios still need validation.
- MySQL permissions and ODBC driver availability still need validation on the target host.
- Damage attribution should be tested against real PvP edge cases.

See `docs/AUDIT.md` for the current audit summary.

## Requirements

- ARK Server API installed on the target server.
- MySQL server reachable from the ARK host.
- A compatible MySQL ODBC driver installed on the ARK host.
- `config.json` updated with real MySQL connection details before use.

## Package Contents

- `ArkApi/Plugins/ExoTribeScore/ExoTribeScore.dll`
- `ArkApi/Plugins/ExoTribeScore/config.json`
- `ArkApi/Plugins/ExoTribeScore/PluginInfo.json`
- `README.md`
- `docs/`

## Install

1. Copy the contents of `ArkApi/Plugins/ExoTribeScore` into the target server's `ArkApi/Plugins/ExoTribeScore`.
2. Edit `config.json` with the target MySQL details.
3. Confirm the ODBC driver name in `config.json` matches the driver installed on the target machine.
4. Start the server and check the plugin logs.
