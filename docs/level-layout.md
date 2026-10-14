# Level layout

This document aims to give you a basic understanding of the level layout in the game.  

## File format

The level layout is stored in a file with the extension `.toml`. The file contains a series of lines, each representing a row of the level. Each line contains a series of characters, each representing a tile in the level.  

## Content

### Description

This contains the description of the level. The parameters are:  

- background: String (Path to the background texture for the level)
- timer: UInt (Time in seconds describing the level time)
- music: String (Path to the music to play during the level)
- player: String (Path to the player texture)

### enemies

This contains an enemy spaw point.

- type: String (The spawn type for this wave)
- direction: String (If required - Direction of the wave)
- start: UInt (Starting point of the wave)
- startY: UInt (Starting Y position of wave)
- gapY: UInt (Gap between enemies. Used in column based spawn)
- size: UInt (Size of the enemy)
- damage: UInt (Damage delt to the player)
- score: UInt (Score given to the player upon kill)
- health: UInt (Health of an enemy)
- count: UInt (Number of enemies to spawn)
- spawnSpeed: Unsigned float (Time between enemy spawn)
- speed: UInt (Speed of each enemies)
- texture: String (Path to enemy texture)
- deathSound: String (Path to sound played upon death)

### tyles

This contains the different walls troughout the level.  

- startX: UInt (Starting X position)
- startY: UInt (Starting y position)
- width: UInt (Width in pixel)
- height: UInt (Height in pixel)
- texture: String (Path to texture)
- colision: Bool (Say if the tyle has collision or not)
