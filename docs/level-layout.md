# Level layout

The server loads a campaign of five TOML files in order:

1. `assets/maps/level_1.toml` — first, introductory level
2. `assets/maps/level_2.toml`
3. `assets/maps/level_3.toml`
4. `assets/maps/level_4.toml`
5. `assets/maps/level_5.toml` — final level

Each file contains one `[[enemies]]` table per spawn wave. The server starts
the next level after every wave has spawned and no enemies remain in the world.
Enemies that leave the screen count as cleared. A three-second intermission
separates levels. The HUD shows the current level and campaign length. Clearing
the fifth level ends the simulation and displays the campaign-complete screen.

## Enemy waves

| Key | Type | Meaning |
| --- | --- | --- |
| `type` | string | `linear` or `column` spawns enemies at `startY`, spaced by `gapY`; `random` chooses a random vertical position. |
| `start` | number | Seconds from the beginning of the level before this wave starts. |
| `startY` | number | First enemy's vertical position for `linear` and `column` waves. |
| `gapY` | number | Vertical spacing between enemies in `linear` and `column` waves. |
| `size` | integer | Enemy collision radius in pixels. |
| `health` | integer | Hits the enemy can take before it is destroyed. |
| `count` | integer | Number of enemies in the wave. |
| `spawnSpeed` | number | Seconds between enemies in this wave. |
| `speed` | number | Horizontal movement speed in pixels per second. |

Example:

```toml
[[enemies]]
type = "linear"
start = 0
startY = 135
gapY = 115
size = 15
health = 1
count = 5
spawnSpeed = 1.25
speed = 72
```

The campaign becomes harder through larger waves, shorter spawn intervals,
faster enemies, and higher health. Keep generated `startY` positions within the
720-pixel game world.

## Power-ups

Each level may define fixed weapon pickups with `[[powerups]]` tables. Pickups
appear when the level starts and are collected by flying into them. They are
server-authoritative and are removed if the level ends before they are taken.

| Key | Type | Meaning |
| --- | --- | --- |
| `type` | string | `heavy` increases shot damage and size; `spread` fires a three-shot fan. |
| `x` | number | Fixed horizontal position in pixels. |
| `y` | number | Fixed vertical position in pixels. |

Example:

```toml
[[powerups]]
type = "spread"
x = 440
y = 225
```

Each pickup replaces the player's current weapon mode. The selected mode is
included in authoritative snapshots and remains active across level changes.
Weapon pickups last 20 seconds; collecting another pickup replaces the mode
and restarts its timer. The client HUD shows the remaining seconds.

## Difficulty tuning

The campaign's difficulty increases by raising wave counts, health and speed,
and by reducing `spawnSpeed`. The intended total enemy counts are 16, 22, 28,
32 and 38 from levels one through five. When tuning a level, keep its threat
above the previous level while leaving enough time for players to react.
