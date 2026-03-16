# ExoTribeScore Overview

## Purpose

ExoTribeScore is an ArkServerAPI plugin for ARK: Survival Evolved that tracks tribe score from configured PvP structure destruction events.

When a configured enemy structure is destroyed:

- the attacking tribe gains score
- the victim tribe loses score
- both tribes are persisted to MySQL
- a score event is written to MySQL
- tribe member snapshots for both tribes are refreshed in MySQL
- floating text is shown to the attacker and nearby online victim tribe members

## Runtime Flow

1. `Plugin_Init` loads the plugin and registers the `APrimalStructure.Die` hook.
2. `TribeScoreService::LoadInitialState()` loads `config.json`, configures the MySQL connection, and loads persisted scores.
3. On structure death, `CaptureDestructionContext()` inspects the structure before the original game function runs.
4. If the original destruction succeeds, `ProcessDestroyedStructure()` applies score changes in memory.
5. The plugin writes the tribe score change, score event, and available tribe-member refreshes in one MySQL transaction.
6. The in-memory cache is updated only after the database write succeeds.
7. The plugin renders configured floating text and serves `/leaderboard` from persisted data.

## Main Components

- [Plugin.cpp](E:\My projects\Ark Plugins\ExoTribeScore\src\Plugin.cpp)
  Handles plugin bootstrap, hook registration, chat command registration, and reload command plumbing.
- [Config.cpp](E:\My projects\Ark Plugins\ExoTribeScore\src\Config.cpp)
  Parses and validates the JSON config contract.
- [TribeScoreService.cpp](E:\My projects\Ark Plugins\ExoTribeScore\src\TribeScoreService.cpp)
  Owns the gameplay logic: attack attribution, tribe score changes, floating text, tribe name refresh, and leaderboard text generation.
- [ScoreStore.cpp](E:\My projects\Ark Plugins\ExoTribeScore\src\ScoreStore.cpp)
  Owns MySQL persistence through ODBC.

## Public Interfaces

- Chat command: `/leaderboard`
- Console command: `ExoTribeScore.Reload`
- RCON command: `ExoTribeScore.Reload`

## Important Constraints

- Only blueprints listed in `config.json` participate in scoring.
- Score changes are processed only when the plugin can resolve an attacker tribe that differs from the victim tribe.
- Tribe score storage is persisted by tribe ID, not tribe name.
- Tribe member snapshots currently refresh only for tribes involved in a scored destruction.
- If live tribe-member data is temporarily unavailable, the plugin preserves the last stored member dataset instead of deleting it.
