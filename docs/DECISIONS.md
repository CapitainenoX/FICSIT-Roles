# Decisions (2026-11-08)

1. **C++ only, no Lua, no cooked assets.** The old Lua bridge doubled the state and could not be validated in network; textures load from PNG at runtime, so no editor/cook step is needed.
2. **Disqualifier after `ValidatePlacementAndCost` + server `CanConstruct` override.** Uses the vanilla refusal path: no item is removed, nothing half-built. Masking UI is never used as security.
3. **Common by default.** Unknown classes (mods, updates) are buildable; only listed classes are reserved. Prevents breaking other mods.
4. **Coverage policy default = Vacant.** Solo and small groups are never blocked; strict RP is one click away (host).
5. **MAM clearances, not recipes.** Research is a world unlock; use is a division permission. If the MAM node template cannot be bound at runtime, research is disabled and clearances count as granted (logged + shown in terminal).
6. **Native fixed class pools** (16 schematics, 8 trees, 8 disqualifiers) because UCLASSes cannot be created from JSON; config entries are mapped onto them.
7. **No forced events, no economy, no XP.** Requests and messages are optional; no reward/penalty.
8. **Two subsystems** (authoritative replicated + local presentation) instead of UI inside the RCO: clear ownership, no UI on dedicated servers.
