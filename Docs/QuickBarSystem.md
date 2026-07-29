# Botanicus Quick Bar

## Design

The native Quick Bar contains eight slots selected with `1` to `8` or the mouse
wheel. It stores only Item Data Framework keys. Names, icons, meshes and custom
properties continue to come from each `UItemDataAsset`, so there is no duplicated
item database.

Item quantities and ownership are intentionally not stored in the Quick Bar. The
future inventory will remain authoritative and will call `Set Slot Item` or
`Clear Slot` on the server.

## Controls

- `1`–`8`: select a specific slot.
- Mouse wheel up/down: select the previous/next slot.
- Item activation is exposed as `Activate Selected Slot`; its final gameplay input
  will be chosen when the tool/equipment system is implemented.

## UI connection

From the Botanicus character, call `Get Quick Bar Component`.

- Build or refresh all slots when `On Quick Bar Changed` fires.
- Move the selection highlight when `On Selected Slot Changed` fires.
- Use `Get Slots`, `Get Slot` and `Get Item Data For Slot` to populate the UI.
- `UItemDataAsset` provides the display name and icon.

The events are push-based, so a UMG widget does not need to poll every frame.

## Multiplayer rules

- The eight slot keys and selected index replicate only to their owning player.
- Selection is locally predicted for responsive UI, then validated by the server.
- Slot assignment and clearing only work on the authoritative server.
- Activation is validated and broadcast on the server through `On Slot Activated`.
- Equipped-world visuals will be replicated separately by the future equipment
  system, without exposing a player's entire private bar to other clients.
