# Botanicus multiplayer ping system

## Status

The Fab `Mark And Ping Multiplayer` plugin is not compatible with Unreal
Engine 5.8 and is not used. Botanicus provides a native C++ replacement under:

`Source/Botanicus/Ping`

The system compiles and starts successfully with Unreal Engine 5.8.

## Controls

- middle mouse button in first person: ping the visible surface at the center
  of the screen;
- middle mouse button in top-down mode: ping the surface under the cursor.

The initial yellow exclamation marker is a functional placeholder. Its visual
can be replaced later without changing placement or replication code.

## Networking and validation

- the owning client requests a world position;
- the server rejects positions farther than 300 metres from the pawn;
- the server accepts at most one ping per player per second;
- the server spawns the replicated marker;
- the marker is always network relevant and is destroyed after eight seconds.

The automated smoke test placed a server-owned marker successfully. A second
immediate request was rejected by the cooldown.

## Manual multiplayer acceptance test

1. Start a two-player Listen Server PIE session.
2. Place a ping with the host and verify it appears for the client.
3. Place a ping with the client and verify it appears for the host.
4. Verify first-person and top-down targeting.
5. Verify that the marker disappears after eight seconds.
