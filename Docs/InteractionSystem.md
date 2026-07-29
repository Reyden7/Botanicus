# Botanicus Interaction System

## Purpose

The interaction foundation is implemented in C++ and is designed for multiplayer
from the start. Local clients only select a target. The authoritative server
validates the request and performs the gameplay action.

## Player controls

- `E`: interact with the object under the crosshair.
- Default range: 300 cm.
- Detection channel: Visibility.

Both values can be adjusted on the `Interaction Component` inherited by the
Botanicus character Blueprint.

## Creating a simple interactable Blueprint

1. Create a Blueprint derived from `BotanicusInteractableActor`.
2. Assign its mesh.
3. Set `Interaction Action` and `Interaction Name`.
4. Implement `On Interacted (Server)` for the gameplay action.
5. Keep Visibility collision enabled on the mesh.

The actor and its enabled state already replicate. Call `Set Interaction Enabled`
from server-side logic to temporarily or permanently disable it.

## Connecting the UI

The character exposes `Get Interaction Component`. The UI can subscribe to
`On Focus Changed` and use:

- `Focused Actor` to know whether something is targeted;
- `Prompt.Action Text` for the verb;
- `Prompt.Target Name` for the object name;
- `Prompt.Can Interact` to display an enabled or disabled state.

No polling is required in the UI.

## Multiplayer security

Before an action is executed, the server checks:

- that the target still exists and implements the interaction interface;
- that it is within the allowed distance;
- that it is visible on the interaction collision channel;
- that its `Can Interact` rule accepts the player.

Gameplay logic implemented in `On Interacted (Server)` therefore runs only on
the authoritative server.
