# Configuration

The runtime config file is [config.json](../ArkApi/Plugins/ExoTribeScore/config.json).

## Full Shape

```json
{
  "version": 1,
  "score_floor": 0,
  "leaderboard_limit": 10,
  "mysql": {
    "driver": "MySQL ODBC 8.0 Unicode Driver",
    "host": "127.0.0.1",
    "port": 3306,
    "database": "ark",
    "username": "root",
    "password": "change-me",
    "table_prefix": "exotribescore_",
    "connection_timeout_seconds": 5
  },
  "floating_text": {
    "positive_color": { "r": 90, "g": 220, "b": 90, "a": 255 },
    "negative_color": { "r": 220, "g": 90, "b": 90, "a": 255 },
    "scale_x": 1.0,
    "scale_y": 1.0,
    "lifespan": 3.0,
    "min_scale": 0.6,
    "fade_in_time": 0.1,
    "fade_out_time": 0.5,
    "velocity": { "x": 0.0, "y": 0.0, "z": 40.0 }
  },
  "messages": {
    "attacker": "+{amount} Tribe Score",
    "victim": "-{amount} Tribe Score"
  },
  "structures": [
    {
      "blueprint": "/Game/PrimalEarth/Structures/Stone/StoneWall/StoneWall.StoneWall_C",
      "score_value": 10
    }
  ]
}
```

## Root Fields

- `version`
  Must be `1`.
- `score_floor`
  Minimum score any tribe can have after score loss is applied.
- `leaderboard_limit`
  Maximum number of ranked tribes shown by `/leaderboard`.
- `mysql`
  MySQL connection settings used by the persistence layer.
- `floating_text`
  Controls colors, scaling, velocity, lifespan, and fade timing for score popups.
- `messages`
  Message templates for attacker and victim notifications.
- `structures`
  List of blueprint paths and score values that are eligible for scoring.

## MySQL Fields

- `driver`
  ODBC driver name installed on the host.
- `host`
  Database server host or IP.
- `port`
  Database server port.
- `database`
  Database name to connect to.
- `username`
  Database login username.
- `password`
  Database login password.
- `table_prefix`
  Prefix applied to all tables created by this plugin.
- `connection_timeout_seconds`
  ODBC login and connection timeout in seconds.

## Message Tokens

The `messages.attacker` and `messages.victim` templates support:

- `{amount}`
- `{signed_amount}`
- `{score}`
- `{attacker_tribe}`
- `{victim_tribe}`

## Blueprint Normalization

Blueprint values are normalized before matching.

Accepted input styles include:

- `/Game/Path/Object.Object_C`
- `Blueprint'/Game/Path/Object.Object_C'`
- `Class'/Game/Path/Object.Object_C'`

After normalization, matching is exact and case-insensitive because normalized values are stored in lowercase.
