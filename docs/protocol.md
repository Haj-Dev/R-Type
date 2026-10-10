# Network protocol

Connection initialization uses TCP; gameplay traffic uses UDP. Packets are
binary and use the structure shown in `docs/network.png`.

TCP listens on port `7000`. The client binds its UDP socket first, then sends
a 7-byte TCP hello containing the UDP port. The server replies with a 5-byte
TCP welcome containing the assigned player ID. The TCP peer address and
advertised UDP port identify the client's gameplay endpoint.

UDP listens on port `7001`. UDP packets use the following structure:

```text
magic (3 bytes) | packet type (1 byte) |
  command type (1 byte) | entity ID (8 bytes) | command payload (...) |
  ... repeated commands ...
terminator (2 bytes: 0D 0A)
```

The magic is the ASCII sequence `HAJ`. Multi-byte numeric values currently use
the host's little-endian representation and are serialized explicitly by
`src/Shared/Types.hpp`; C++ struct layout is never sent directly.

## Packet types

| Value | Type | Direction | Purpose |
|---:|---|---|---|
| `2` | Input | client -> server | Send the latest player input state |
| `3` | Snapshot | server -> client | Send the authoritative scene |

## Commands

| Value | Command | Payload |
|---:|---|---|
| `2` | Input | one flags byte: up, down, left, right, fire |
| `4` | Snapshot entity | entity kind (1), x (4), y (4), health (2) |

The entity ID field is always eight bytes, including for commands that do not
refer to an entity. The TCP hello and welcome are separate fixed-size messages
and do not use the UDP command layout. Receivers validate the magic, packet
type, terminator, command type, and exact payload size before reading any
field. Unknown or malformed datagrams are discarded.
