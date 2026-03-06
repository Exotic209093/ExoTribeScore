# ExoTribeScore

ArkApi plugin for ARK: Survival Evolved that tracks tribe score from PvP structure destruction.

## What v1 supports

- JSON-driven per-structure score values keyed by normalized blueprint path.
- Persistent tribe scores stored by tribe ID in `scores.json`.
- Score changes when one tribe destroys another tribe's configured structure.
- Score floor clamping through `score_floor`.
- Floating score text at the destroyed structure location.
- Popup `/leaderboard` command for ranked tribe scores.
- Runtime config reload through `ExoTribeScore.Reload` console/RCON command.

## Project layout

- `ExoTribeScore.vcxproj`: Visual Studio project.
- `ArkApi.props`: ArkApi include/library path configuration.
- `ArkApi/Plugins/ExoTribeScore/config.json`: runtime config.
- `ArkApi/Plugins/ExoTribeScore/scores.json`: persisted score state.
- `ArkApi/Plugins/ExoTribeScore/PluginInfo.json`: ArkApi plugin metadata.

## Build notes

1. Open `ExoTribeScore.vcxproj` in Visual Studio 2022.
2. If your ArkApi SDK is not at the default path in `ArkApi.props`, update `ArkApiRoot` or set `ARKAPI_ROOT`.
3. Build `Release|x64`.
4. The DLL output goes directly to `ArkApi/Plugins/ExoTribeScore`.

## Config notes

- Blueprint paths are normalized before matching.
- Accepted blueprint input formats include raw `/Game/.../Object.Object_C`, `Blueprint'...'`, and `Class'...'`.
- Matching is exact after normalization.
- Message templates support `{amount}`, `{signed_amount}`, `{score}`, `{attacker_tribe}`, and `{victim_tribe}`.
