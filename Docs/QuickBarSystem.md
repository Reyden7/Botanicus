# Botanicus Inventory and Quick Bar

## Design

The native inventory contains exactly ten slots, as required by the game design.
Every occupied slot stores:

- an Item Data Framework key;
- a quantity (for example, the seeds remaining in one packet);
- a unique instance ID.

Two packets of the same seed species therefore remain two distinct physical
items. They are not automatically merged. Names, icons, meshes and custom
properties still come from `UItemDataAsset`, so there is no duplicated item
database.

## Controls

- `1` to `9`: select slots 1 to 9.
- `0`: select slot 10.
- Mouse wheel up/down: select the previous/next slot.
- Number keys always select inventory slots. The mouse wheel is suspended in
  top-down building placement, where it rotates the complete building.
- Item activation is exposed as `Activate Selected Slot`; its final gameplay
  input will be connected when tools and seed packets are implemented.

## Server operations

- `Add Item`: creates a distinct item instance in the first empty slot.
- `Remove Quantity`: removes uses from one item and clears it at zero.
- `Consume Selected Item`: consumes the selected item.
- `Set Slot Item` and `Clear Slot`: compatibility helpers for Blueprint systems.

All mutations require server authority. A full inventory rejects `Add Item`
without replacing or dropping an existing item.

## UI connection

From the Botanicus character, call `Get Quick Bar Component`.

- Build or refresh all slots when `On Quick Bar Changed` fires.
- Move the selection highlight when `On Selected Slot Changed` fires.
- Use `Get Slots`, `Get Slot` and `Get Item Data For Slot` to populate the UI.
- Read `Quantity` on the slot and the display name/icon on `UItemDataAsset`.

The events are push-based, so a UMG widget does not need to poll every frame.

## Multiplayer rules

- The ten slots and selected index replicate only to their owning player.
- Selection is locally predicted for responsive UI, then validated by the server.
- Item addition, removal, consumption, assignment and clearing only work on the
  authoritative server.
- Activation is validated and broadcast on the server through `On Slot Activated`.
- Equipped-world visuals will be replicated separately by the equipment system,
  without exposing a player's complete private inventory to other clients.

## Prototype test item and temporary UI

In PIE only, every authoritative player character receives one distinct
`Paquet de graines test` containing ten seeds in slot 1. This bootstrap is
compiled out of packaged builds.

EBS demonstration tools are not Botanicus inventory items. The complete EBS
demonstration HUD is collapsed and its damage/mallet actions are disabled.

The native `UBotanicusQuickBarWidget` displays the ten slots above EBS' temporary
tool strip. It is deliberately neutral and contains no final art:

- the selected slot is yellow;
- occupied slots are dark green;
- every slot shows its keyboard shortcut;
- the item name and remaining quantity update through Quick Bar events.

The widget is presentation only. Replacing it with a designer-authored Widget
Blueprint will not change inventory storage, input or multiplayer replication.

### PIE verification

1. Launch a two-player PIE session.
2. Confirm both players have `Paquet de graines test x10` in slot 1.
3. Use `1` to `0` and the mouse wheel; only the local player's yellow selection
   should move.
4. Select slot 1 and left-click several times. Only that player's quantity must
   decrease.
5. Confirm `Q` no longer opens the modular EBS building menu.
6. Enter top-down view with `T`; the wheel must rotate a selected complete building
   without moving the inventory selection.
