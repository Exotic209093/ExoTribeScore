# ExoTribeScore

ArkApi plugin for ARK: Survival Evolved that tracks tribe score from PvP structure destruction.

## Documentation

- [Overview](docs/OVERVIEW.md)
- [Configuration](docs/CONFIGURATION.md)
- [Database](docs/DATABASE.md)
- [Code Audit](docs/AUDIT.md)

## What v1 supports

- JSON-driven per-structure score values keyed by normalized blueprint path.
- Persistent tribe scores stored by tribe ID in MySQL tables.
- Tribe member datasets stored in MySQL for each tribe.
- Score event history captured in MySQL for future admin/reporting use.
- Atomic persistence for score changes, score events, and tribe-member refreshes per scored destruction.
- Score changes when one tribe destroys another tribe's configured structure.
- Score floor clamping through `score_floor`.
- Floating score text at the destroyed structure location.
- Popup `/leaderboard` command for ranked tribe scores.
- Runtime config reload through `ExoTribeScore.Reload` console/RCON command.

## Project layout

- `ExoTribeScore.vcxproj`: Visual Studio project.
- `ArkApi.props`: ArkApi include/library path configuration.
- `ArkApi/Plugins/ExoTribeScore/config.json`: runtime config.
- `ArkApi/Plugins/ExoTribeScore/PluginInfo.json`: ArkApi plugin metadata.

## Build notes

1. Open `ExoTribeScore.vcxproj` in Visual Studio 2022.
2. If your ArkApi SDK is not at the default path in `ArkApi.props`, update `ArkApiRoot` or set `ARKAPI_ROOT`.
3. Install a MySQL ODBC driver on the host machine. The sample config uses `MySQL ODBC 8.0 Unicode Driver`.
4. Build `Release|x64`.
5. The DLL output goes directly to `ArkApi/Plugins/ExoTribeScore`.

## Config notes

- Blueprint paths are normalized before matching.
- Accepted blueprint input formats include raw `/Game/.../Object.Object_C`, `Blueprint'...'`, and `Class'...'`.
- Matching is exact after normalization.
- `mysql` in `config.json` controls the MySQL host, port, database, credentials, ODBC driver, and table prefix.
- Tables created automatically are `<table_prefix>tribe_scores`, `<table_prefix>tribe_members`, and `<table_prefix>score_events`.
- Message templates support `{amount}`, `{signed_amount}`, `{score}`, `{attacker_tribe}`, and `{victim_tribe}`.
- Tribe member snapshots are refreshed for the attacker and victim tribes whenever a scored structure destruction is processed.
- Reload now fails closed if the configured database cannot be reached or initialized.
