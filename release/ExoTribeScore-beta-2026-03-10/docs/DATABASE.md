# Database

ExoTribeScore persists data to MySQL through ODBC. The table prefix comes from `config.json`.

If `table_prefix` is `exotribescore_`, the plugin creates these tables:

- `exotribescore_tribe_scores`
- `exotribescore_score_events`
- `exotribescore_tribe_members`

## `tribe_scores`

Purpose: current leaderboard state.

Columns:

- `tribe_id`
- `last_known_name`
- `score`
- `updated_at`

Notes:

- one row per tribe
- `/leaderboard` reads from this table
- tribe names are refreshed from live tribe data when available

## `score_events`

Purpose: audit trail of score-producing destruction events.

Columns:

- `id`
- `attacker_tribe_id`
- `victim_tribe_id`
- `blueprint`
- `delta`
- `map_name`
- `created_at`
- `attacker_score`
- `victim_score`

Notes:

- one row per processed score event
- can be used later for history views, admin tools, or season summaries

## `tribe_members`

Purpose: latest captured tribe roster snapshot for each tribe involved in a scored destruction.

Columns:

- `tribe_id`
- `player_data_id`
- `player_name`
- `rank_group`
- `updated_at`

Notes:

- primary key is `(tribe_id, player_data_id)`
- rows are fully replaced for a tribe during each roster sync
- current implementation syncs only attacker/victim tribes from scored events
- if a live roster snapshot cannot be read during a score event, the previous dataset is kept

## Data Ownership

- Tribe score is keyed by tribe ID.
- Tribe member snapshots come from ARK `FTribeData`.
- Tribe names are treated as mutable display data and may be refreshed over time.
