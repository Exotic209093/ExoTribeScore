# Code Audit

Audit date: 2026-03-10

Scope:

- [Plugin.cpp](E:\My projects\Ark Plugins\ExoTribeScore\src\Plugin.cpp)
- [Config.cpp](E:\My projects\Ark Plugins\ExoTribeScore\src\Config.cpp)
- [ScoreStore.cpp](E:\My projects\Ark Plugins\ExoTribeScore\src\ScoreStore.cpp)
- [TribeScoreService.cpp](E:\My projects\Ark Plugins\ExoTribeScore\src\TribeScoreService.cpp)

## Result

The prior beta blockers identified in the earlier audit have been addressed:

- reload now fails closed if the configured MySQL connection cannot be validated
- score changes, score events, and tribe-member refreshes now persist atomically per scored destruction
- `/leaderboard` no longer writes back to the database
- ODBC connection-string values are now escaped safely

## Residual Risks

- This has been compile-verified, but not validated against a live ARK server session or a live MySQL server.
- Attacker attribution still depends on the current resolver in [TribeScoreService.cpp](E:\My projects\Ark Plugins\ExoTribeScore\src\TribeScoreService.cpp), so unusual PvP damage sources may still require follow-up testing.
- Deployment still depends on the target machine having the expected MySQL ODBC driver installed.

## Recommended Beta Scope

- small closed beta first
- verify real PvP cases for player, explosive, turret, and tamed-creature damage
- verify MySQL permissions allow table creation, inserts, updates, and deletes for the configured user
