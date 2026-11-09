# Architecture — FICSIT Roles 1.0.0 (2026-11-08)

Pure C++ SML mod (no pak, no Lua, no Blueprint asset). Game 517554 · CSS UE 5.6.1 · SML 3.12.0.

```
Mod/FICSITRoles/
  FICSITRoles.uplugin
  Resources/Config/FICSITRoles.json   declarative divisions, shared buildings, MAM clearances, costs
  Resources/Localization/*.json       15 languages (en = fallback)
  Resources/UI/*.png                  icons, card art, 9-slice chamfer pieces, logo (tools/make_assets.py)
  Source/FICSITRoles/
    FRTypes.h            replicated + saved data model
    FRConfig.*           JSON loader, class-pattern rule cache, safe defaults
    FRLoc.*              runtime localization (game language, en fallback)
    FRRolesSubsystem.*   AUTHORITY: session state, permission engine, server mutations, persistence
    FRRemote.*           RCO: client -> server requests, server -> client notifications
    FRResearch.*         MAM division trees (pool of native schematic/tree classes, reflection-bound nodes)
    FRModule.cpp         native hooks, world module, disqualifiers, /roles command
    UI/FRStyle.*         design tokens + brushes
    UI/FRLocalSubsystem.* client presentation: flow, input capture, hotkey, toasts
    UI/FRScreens.*       Slate screens: setup, waiting room, divisions, terminal, HUD overlay
```

## Runtime objects
| Object | Spawn | Role |
|---|---|---|
| `AFRRolesSubsystem` | server, replicated, saved (IFGSaveInterface) | single source of truth |
| `AFRLocalSubsystem` | every non-dedicated instance (SpawnLocal) | UI, hotkey, toasts; never decides |
| `UFRRemote` (RCO) | per player controller | requests with the caller's real controller as identity |
| `UFRWorldModule` | SML root world module | registers subsystems, RCO, MAM, chat command |

## Enforcement points
1. `AFGHologram::ValidatePlacementAndCost` (after): adds a localized disqualifier → red hologram on the client,
   and on the server the vanilla RPC path refuses (`CanConstruct` is evaluated before cost removal).
2. `AFGHologram::CanConstruct` (server only): overridden to `false` when the permission engine refuses — defence in depth.
3. `AFGResearchManager::InitiateResearch`: division MAM nodes launch only for their division (or exception/coverage).
Blueprints: classes are read from the blueprint hologram's buildable map; children holograms are walked recursively.

## Data flow
Client UI → `UFRRemote::Server_*` → `AFRRolesSubsystem::Server*` (validates, mutates, `MarkChanged`) →
replication (`OnRep_State`) → `OnStateChanged` → UI refresh. Notifications: `Client_Notify(key,args)` localized on the receiver.
