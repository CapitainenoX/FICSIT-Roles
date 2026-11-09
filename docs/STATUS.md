# Status — 2026-11-08 (1.0.0)

| Feature | Specified | Implemented | Compiled | Installed | Solo tested | MP tested |
|---|---|---|---|---|---|---|
| Division permissions (build, blueprints, children) | ✔ | ✔ | ✔ | ✘ | ✘ | ✘ |
| Server refusal before cost (disqualifier + CanConstruct) | ✔ | ✔ | ✔ | ✘ | ✘ | ✘ |
| Session flow: setup → waiting room → division | ✔ | ✔ | ✔ | ✘ | ✘ | ✘ |
| Persistence (save/load, reconnection by online id) | ✔ | ✔ | ✔ | ✘ | ✘ | ✘ |
| Exceptions with expiry, coverage policy, reset/assign | ✔ | ✔ | ✔ | ✘ | ✘ | ✘ |
| FICSIT terminal (F7), requests, messages, announcements | ✔ | ✔ | ✔ | ✘ | ✘ | ✘ |
| MAM division trees (12 clearances) | ✔ | ✔ | ✔ | ✘ | ✘ | ✘ |
| 15 languages | ✔ | ✔ (keys/placeholders validated) | — | ✘ | ✘ | ✘ |

Build: `SMLProject\Mods\FICSITRoles\Binaries\Win64\FactoryGameSteam-FICSITRoles-Win64-Shipping.dll` (869 888 B, 09:59).
Packages: `dist\FICSITRoles-1.0.0.zip` (SMR layout), `dist\FICSITRoles-1.0.0-manual.zip` (37 entries).
Blocker: installing SML/the mod into the game folder needs the user's permission (agent action refused). Nothing validated in game yet.
Next: `scripts\install-sml.ps1` → `scripts\deploy.ps1` → new solo save → TEST-REPORT checklist → 2-player test.

## 2026-11-09 — v1.0.0 accepted by the user in game
- MAM trees are cooked Blueprint assets (UFRAssetGen editor generator -> /FICSITRoles/MAM, Alpakit pak). Runtime-built trees are gone.
- Config rev 8: lane layout (vanilla grid 7x9), every buildable/recipe/pickup/equipment owned by a division, MAM nodes = cross-division formations, costs ~4x.
- Release: `scripts/full-build.ps1` (editor module -> generate assets -> Alpakit -> dist), then `scripts/deploy.ps1` with the game closed.
