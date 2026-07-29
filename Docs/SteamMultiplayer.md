# Botanicus Steam Multiplayer

## Architecture

Botanicus uses Unreal Engine's Online Subsystem Steam and its session/lobby API.
No EOS dependency or third-party Steam integration plugin is required.

During development, `SteamDevAppId=480` uses Valve's shared Spacewar test
application. It must be replaced with the real Botanicus App ID before release.

## Blueprint API

Get the `Botanicus Multiplayer Subsystem` from the Game Instance.

- `Host Session`: creates a lobby and opens the selected map as a listen server.
- `Find Sessions`: searches for compatible Botanicus development lobbies.
- `Join Session By Index`: joins an entry returned by `On Sessions Found`.
- `Leave Session`: destroys the session and can return to a menu map.
- `Show Steam Invite Overlay`: opens Steam's friend invitation interface.

Menus should listen to:

- `On Online State Changed` to enable or disable buttons and show loading states;
- `On Online Error` to display a localized error;
- `On Session Created`, `On Sessions Found`, `On Session Joined`, and
  `On Session Left` for operation results.

## Development console commands

Until the final UMG menu exists, open Unreal's console with `~` and use:

- `BotanicusOnlineStatus`: print the active subsystem and session state;
- `BotanicusHost 4`: create a four-player lobby;
- `BotanicusFind`: search and print compatible lobbies with their indexes;
- `BotanicusJoin 0`: join result zero;
- `BotanicusInvite`: open Steam's friend invitation overlay;
- `BotanicusLeave`: destroy or leave the current session.

The output is available in the Unreal log under `LogBotanicus`.

## Session privacy and filtering

The current development lobby:

- supports 2 to 8 public players;
- allows invitations and joining in progress;
- uses Steam lobbies and presence;
- advertises `BOTANICUS_BUILD=Botanicus-Dev-1`;
- searches only for the same Botanicus development build marker.

The marker must become a release/build compatibility identifier later so players
running incompatible versions cannot join one another.

## Testing requirements

- Steam must be running and logged in.
- Two real Steam accounts are required for a genuine two-machine test.
- App ID 480 is shared by many projects; Botanicus filters results with its build
  marker, but the real App ID is still required before release.
- Testing two Steam clients on one Windows account/machine is unreliable. Prefer
  two computers or a second Windows/Steam environment.
- Steam overlay and invitations should be validated in a packaged Development
  build, not only in PIE.
