# Save compatibility across versions

Goal: a save made with any FICSIT Roles version keeps working after a mod update.

- **Save schema**: `AFRRolesSubsystem::SchemaVersion` (current 2). Older saves are upgraded in `PostLoadGame`; a save from a newer version loads what is known (warning in log).
- **Renamed ids**: config key `renamed` (`"old.id": "new.id"`, chains followed) migrates researched nodes, player divisions and grants on load.
- **Removed ids**: unknown researched ids are kept (ignored) so a node coming back is restored; removed divisions clear the player's role (they pick again).
- **MAM**: after load, purchased FICSIT schematics re-sync to researched nodes, and researched nodes whose schematic is not purchased (old id, older version) are given in the MAM so nobody pays twice.
- **Guard**: `tools/ids.lock` lists every shipped node/division id (append only). `tools/validate_config.py` (run first by `scripts/full-build.ps1`) fails if a shipped id disappears without a `renamed` entry.
- **Rules for future versions**: never reuse an id for something different; rename only through `renamed`; bump `configRevision` when the shipped config changes; keep asset names `FRS_<id>` / `FRT_<division>` stable.
- **Game updates**: the module manifest uses BuildId `SML`, so any game build loads it as long as SML supports that build; rebuild against the new SML/game headers if the game changes class layouts.
