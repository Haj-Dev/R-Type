# R-Type network protocol

This document describes the wire protocol used by an R-Type client and server.
It is language-agnostic and describes the format currently implemented by the
project.

This document describes the network protocol of the part 1 of the project.

The protocol has two phases:

1. A reliable TCP handshake establishes the client's gameplay endpoint and
   assigns a player ID.
2. Gameplay data is exchanged as UDP datagrams. Clients send input; the server
   sends authoritative scene snapshots.

The protocol is binary. Multi-byte values are encoded in the host's current
little-endian representation. Implementations must serialize each field
explicitly; the in-memory layout or alignment of a language-specific
structure is not part of the protocol.

## Transport endpoints

| Transport | Port | Use |
| --- | ---: | --- |
| TCP | `7000` | Connection handshake and connection liveness |
| UDP | `7001` | Server gameplay endpoint |

The client binds its UDP socket to an available local port before opening the
TCP connection. The client advertises that local UDP port in the TCP hello.
The server uses the TCP peer address together with the advertised UDP port as
the client's UDP endpoint.

The server identifies UDP input by its source endpoint. A datagram from an
endpoint that was not registered by the TCP handshake is ignored.

## Common values

### Magic

Every protocol message begins with the three ASCII bytes:

```text
H A J
```

### Terminator

UDP packets end with the two bytes:

```text
0D 0A
```

This is a carriage return followed by a line feed. The terminator is part of
the packet framing and is not part of a command payload.

### Packet header

Every UDP packet starts with a four-byte header:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 3 bytes | Magic: `H`, `A`, `J` |
| 3 | 1 byte | Packet type |

### Command header

Commands inside UDP packets use a nine-byte header:

| Offset within command | Size | Field |
| ---: | ---: | --- |
| 0 | 1 byte | Command type |
| 1 | 8 bytes | Entity ID |

The entity ID field is present for every command, including commands that do
not refer to an entity. For such commands, its value is zero and must be
ignored.

A UDP packet has this general form:

```text
packet header
command header
command payload
...
terminator
```

Snapshot packets may contain multiple commands. The receiver reads commands
until it reaches the packet terminator.

## Connection handshake

The handshake uses TCP and does not use the UDP command layout.

### Client hello

The client sends exactly seven bytes after establishing the TCP connection:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 3 bytes | Magic: `H`, `A`, `J` |
| 3 | 1 byte | Packet type: `1` (`HELLO`) |
| 4 | 2 bytes | Client UDP port, unsigned 16-bit integer |
| 6 | 1 byte | `0D` |

The port is encoded little-endian. A port value of zero is invalid.

The server accepts the client only if the message is exactly seven bytes,
contains the expected magic and packet type, ends in `0D`, and contains a
non-zero UDP port.

### Server welcome

The server replies with exactly five bytes:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 3 bytes | Magic: `H`, `A`, `J` |
| 3 | 1 byte | Welcome type: `3` (`WELCOME`) |
| 4 | 1 byte | Assigned player ID |

The player ID must be in the range `0` through `3`. The welcome message has
no UDP terminator.

After sending the welcome, the server associates the TCP peer address and the
advertised UDP port with the assigned player ID.

## Packet and command types

### Packet types

| Value | Name | Transport | Direction | Meaning |
| ---: | --- | --- | --- | --- |
| `1` | `HELLO` | TCP | Client to server | Client handshake |
| `2` | `INPUT` | UDP | Client to server | Latest player input state |
| `3` | `SNAPSHOT` | UDP | Server to client | Authoritative scene state |

The TCP welcome uses command value `3` in its fourth byte, but is a dedicated
five-byte TCP message rather than a UDP packet.

### Command types

| Value | Name | Meaning |
| ---: | --- | --- |
| `1` | `HELLO` | Defined for the generic packet format |
| `2` | `INPUT` | Player input payload |
| `3` | `WELCOME` | Player assignment |
| `4` | `SNAPSHOT_ENTITY` | One entity in a scene snapshot |

The `HELLO` and `WELCOME` command forms used by the TCP handshake are
specialized fixed-size messages described above.

here is a diagram of the packet structure:
![alt text](example_packet_strcuture.svg)

## Input packet

An input packet has packet type `2` and contains exactly one input command.
Its layout is:

```text
4 bytes  packet header
1 byte   command type: 2
8 bytes  entity ID: zero
1 byte   input flags
2 bytes  terminator: 0D 0A
```

The complete packet is 16 bytes.

The input flags byte uses one bit per action:

| Bit | Action |
| ---: | --- |
| 0 | Up |
| 1 | Down |
| 2 | Left |
| 3 | Right |
| 4 | Fire |
| 5-7 | Unused |

An action is active when its bit is set to `1`. The server replaces the
player's current input state with the most recently received valid input
packet.

## Snapshot packet

A snapshot packet has packet type `3` and contains zero or more
`SNAPSHOT_ENTITY` commands. The server sends the latest authoritative scene
to each connected client.

Each entity command has the common nine-byte command header followed by this
payload:

| Field | Size | Description |
| --- | ---: | --- |
| Entity kind | 1 byte | Numeric entity kind |
| X position | 4 bytes | 32-bit floating-point value |
| Y position | 4 bytes | 32-bit floating-point value |
| Health | 2 bytes | Signed 16-bit integer |
| Layer | 4 bytes | Signed 32-bit integer |
| Scale | 4 bytes | 32-bit floating-point value |
| X offset | 4 bytes | 32-bit floating-point value |
| Y offset | 4 bytes | 32-bit floating-point value |
| Rotation | 4 bytes | 32-bit floating-point value |
| Atlas flag | 1 byte | Boolean: zero is false, non-zero is true |
| Base width | 4 bytes | Unsigned 32-bit integer |
| Base height | 4 bytes | Unsigned 32-bit integer |
| Frame count | 8 bytes | Unsigned 64-bit integer |
| Frame duration | 4 bytes | 32-bit floating-point value |
| Variant | 8 bytes | Unsigned 64-bit integer |
| Texture name | Variable | Raw text bytes |
| Texture delimiter | 1 byte | Line-feed byte `0A` |

The fixed portion of the entity payload is 60 bytes. The texture name follows
immediately after the variant field and is terminated by a single `0A` byte.
Texture names must not contain an unescaped `0A` byte. An empty texture is
represented by an immediate delimiter.

The entity ID in the command header identifies the entity represented by the
payload. It is encoded as an unsigned 64-bit value on the wire; current
entities use the lower 32 bits.

## Validation and handling

A receiver must validate a packet before reading its fields:

1. Verify the expected packet type.
2. Verify the three magic bytes.
3. Verify the final `0D 0A` terminator for UDP packets.
4. Verify the command type.
5. Verify fixed-size packets have their exact required length.
6. Verify variable-length snapshot commands remain within the datagram.
7. Reject malformed or unknown data without applying it to game state.

UDP datagrams are processed independently. The protocol does not include
sequence numbers, acknowledgements, retransmission, encryption, or
authentication. A lost or malformed gameplay datagram is discarded and the
next valid input or snapshot is used.

## Message flow

```text
Client                                      Server
  |                                           |
  |--- TCP connect to port 7000 ------------>|
  |--- 7-byte client hello ------------------>|
  |<-- 5-byte server welcome -----------------|
  |                                           |
  |--- UDP input to port 7001 --------------->|
  |                                           |
  |                              simulate     |
  |                              scene state  |
  |<-- UDP snapshot --------------------------|
  |                                           |
```

The server simulation is authoritative. Input packets describe player intent;
snapshot packets describe the resulting world state that the client should
display.
