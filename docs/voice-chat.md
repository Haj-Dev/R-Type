# Voice chat

R-Type voice chat is a room-scoped, peer-relayed voice path. The game server
does not decode or mix audio: it validates each datagram, identifies the
sender from the registered UDP endpoint, and forwards the frame to the other
occupants of that room.

## Audio capture and playback

The client uses miniaudio `0.11.26` through the small
`Audio::AAudioSystem` abstraction in `src/Shared/Systems/`. The backend opens a
48 kHz, mono, 32-bit floating-point duplex device. Microphone samples are
buffered by the device callback and playback samples are consumed by the same
callback.

The client network thread processes voice once per network tick:

1. Capture one 20 ms frame (960 samples) when the microphone is enabled.
2. Encode it with Opus and send the compact frame to the server's voice UDP
   endpoint.
3. Drain received voice datagrams, validate and decode the Opus frame, and
   submit PCM samples for playback.

Audio-device startup failure is reported but does not prevent the client from
connecting or playing the game.

## Voice transport

Voice uses a separate UDP socket and port from gameplay. During the TCP hello,
the client advertises both its gameplay UDP port and its voice UDP port. The
server stores both endpoints. The default server ports are:

| Endpoint | Default port |
| --- | ---: |
| Gameplay UDP | `7001` |
| Voice UDP | `7002` |

Each voice datagram uses the shared `HAJ` header and packet type `5`, followed
by:

| Field | Size | Description |
| --- | ---: | --- |
| Gameplay ID | 1 byte | Sender's room gameplay ID |
| Sequence | 2 bytes | Little-endian wrapping frame sequence |
| Payload size | 2 bytes | Little-endian Opus payload size |
| Opus payload | Variable | Encoded mono voice frame |
| Terminator | 2 bytes | `0D 0A` |

Opus frames represent 20 ms of 48 kHz mono audio and are limited to 1275 bytes.
`Net::validVoicePacket` checks the magic, packet type, exact datagram size,
payload size, and terminator before any payload is read.

The server replaces the packet's gameplay ID with the authenticated gameplay
ID assigned at game launch. It forwards only to players and spectators in the
same room, never back to the sender. Invalid, unknown, or out-of-room packets
are discarded.

## Controls

- **M** toggles the local microphone.
- **Shift+M** toggles deafen, suppressing incoming voice.
- The room controls adjust microphone state, deafen state, and playback volume.
- Voice hotkeys are ignored while the chat input is active.

Microphone mute stops capture transmission. Deafen stops playback of received
frames; it does not affect microphone transmission.

## Limitations

Voice frames use Opus at its default voice profile, substantially reducing
bandwidth compared with raw PCM. There is no jitter buffer, voice activity
detection, echo cancellation, encryption, or per-player volume control. UDP
loss can therefore produce audible gaps.
