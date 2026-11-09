# FICSIT Roles 1.0.0

Industrial roleplay for Satisfactory (solo & co-op): every engineer joins a FICSIT division with its own reserved
buildings and MAM clearances. No currency, no XP, no forced quest — vanilla progression untouched.
15 languages (follows the game language): EN, FR, DE, ES, IT, PT-BR, PL, RU, UK, CS, HU, TR, JA, KO, ZH.

Requires: Satisfactory 1.x build ≥ 517554 · SML 3.12.0. Every player of a session needs the mod.

## Install
**Mod manager (recommended):** install SML 3.12 then FICSIT Roles from the archive `FICSITRoles-1.0.0.zip`.
**Manual:** close the game, extract `FICSITRoles-1.0.0-manual.zip` into
`<Satisfactory>\FactoryGame\Mods\` (you get `Mods\FICSITRoles\FICSITRoles.uplugin`). SML must already be installed.
**From source (this repo):** `scripts\install-sml.ps1` → `scripts\build.ps1` → `scripts\deploy.ps1`.
Note: running `ficsit apply` later removes mods that are not in the ficsit-cli profile — redeploy afterwards.

## Uninstall / rollback
Close the game, delete `FactoryGame\Mods\FICSITRoles`. Saves stay loadable without the mod (the mod's data is simply ignored).
`scripts\deploy.ps1` keeps every previous install in `backups\<date>` — copy it back to roll back.

## Player guide
1. Load or create a save. The host chooses the number of players (01–04, extended 5–8).
2. Waiting room: see who is connected; pick your division ("Choose my division").
3. Division screen: read capabilities and restrictions, select, confirm. The server validates — two players can't take the same division (≤4 players).
4. Play. Common buildings (foundations, walls, storage, belts, pipes, poles, miners…) are open to everyone.
   Reserved buildings turn red with the reason ("Reserved to Industry & Production") and are refused by the server before any cost.
5. **F7** opens the FICSIT terminal: overview, divisions, your permissions, assistance requests, messages.
6. Escape always reaches the pause menu. "Later" lets you explore before choosing (reserved buildings stay locked).

Divisions — Exploration & Research (MAM, towers, portals) · Energy & Grids (all generators, batteries) ·
Industry & Production (all production machines) · Logistics & Transport (rail, stations, drones). Refinery: Industry + Energy.

**MAM:** each division has a "FICSIT – <division>" tree with 3 clearances (T1–T3). Research is shared by the session,
launching it is reserved to the division. Clearances unlock no recipe; they authorise the division to build advanced
buildings once vanilla has unlocked them.

## Host / admin guide (terminal → Administration)
- Expected players, coverage policy: *Strict* / *Vacant divisions covered* (default) / *Absent specialists covered*.
- Per engineer: reset role (they choose again), assign a division (replacement / transfer).
- Temporary permission: engineer → division or all → 15/60/240 min or until revoked → reason → Grant. Revoke any time.
- Announcement to everyone (also posted in the game chat). Event journal + config diagnostics.
Chat fallback: `/roles`, `/roles reset <player>`, `/roles grant <player> <division|Build_X|*> [min]`, `/roles revoke <id>`, `/roles coverage off|vacant|absent`.
Dedicated server: the first player who joins an unconfigured session becomes its host.

## Configuration
`Resources/Config/FICSITRoles.json`: divisions, reserved class patterns, shared buildings, clearance tiers, costs, research time,
terminal key (`terminalKey`, default F7), default coverage. Texts: `Resources/Localization/<lang>.json`.
An invalid file never crashes the game: defaults are used and the problem is listed in the terminal journal.

Docs: `docs/` (architecture, permissions, network, UI, decisions, status, tests, known issues).
