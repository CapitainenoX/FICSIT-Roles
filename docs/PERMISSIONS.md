# Permissions

Source of truth: `Resources/Config/FICSITRoles.json`. Class names without `_C`, `*` = prefix.
Anything not listed (vanilla common buildings, other mods, future updates) is **common**: buildable by everyone.

## Decision order (`AFRRolesSubsystem::EvaluateBuild`, server-authoritative)
1. Class not reserved → allow.
2. No player record yet → refuse "synchronising" (client UI only shows; server re-checks).
3. Player's division owns the class → allow, unless a MAM clearance is required and not researched.
4. Active host exception (player + division / class / `*`, optional expiry, server UTC) → allow.
5. Coverage policy says the owning division is uncovered (Vacant: no member / Absent: no member online) → allow (clearance still required).
6. Otherwise refuse with the owning division name.

Refusals happen before placement validation completes and before any cost is taken. A permission change never destroys existing buildings.

## Matrix (BUILD)
| Division | Reserved | MAM clearance (T1 / T2 / T3) |
|---|---|---|
| Exploration & Research | MAM, lookout tower, radar tower, portals | lookout / radar / portals |
| Energy & Grids | all generators (biomass→nuclear), batteries, priority switch, power augmenter | grid / fuel+geothermal / nuclear |
| Industry & Production | smelter, constructor, assembler, foundry, manufacturer, blender, packager, particle accelerator, quantum encoder, converter | manufacturer / blender+packager / accelerator+encoder+converter |
| Logistics & Transport | railway, signals, buffer stop, train station, platforms, truck stations, drone port | truck stations / freight platforms / drone port |
| Shared | refinery: Industry + Energy | — |
| Common | foundations, walls, storage, belts, lifts, pipes, pumps, poles, cables, miners, hypertubes, HUB, space elevator, everything else | — |

USE / INTERACT / DRIVE / CONFIGURE are never restricted: anyone uses what others built.
RESEARCH: only the division MAM nodes are restricted to their division; vanilla MAM research stays open.
DISMANTLE, CRAFT, EQUIP, COLLECT: declared in the model, intentionally not enforced (no reliable, loss-free interception) — see KNOWN-ISSUES.

## Exceptions (host)
Terminal → Administration: select engineer → target (division or all) → duration (15 / 60 / 240 min / until revoked) → reason → Grant. Revocable at any time; expiry checked against server UTC, logged and notified.
Chat fallback: `/roles grant <player> <division|Build_X|*> [minutes]`, `/roles revoke <id>`, `/roles reset <player>`, `/roles coverage off|vacant|absent`.
