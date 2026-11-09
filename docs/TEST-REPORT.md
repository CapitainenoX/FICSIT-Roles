# Test report — 2026-11-08

| Test | Result | Evidence |
|---|---|---|
| Compilation | PASS | build-3, UBT exit 0, DLL produced |
| Config JSON valid | PASS | python json.load |
| 15 localization files: same keys, placeholders, bullet counts | PASS | validation script "15 files []" |
| Assets: separate, real alpha, no text in functional UI | PASS | docs/ASSET-MANIFEST.json, contact sheet reviewed |
| Installation / mod loaded (`FICSIT Roles 1.0.0 loaded` in FactoryGame.log) | NOT TESTED | game install pending user permission |
| New solo game → setup → division → play | NOT TESTED | |
| Constructor refused for Exploration, allowed for Industry, inventory unchanged | NOT TESTED | |
| Blueprint containing a reserved machine refused | NOT TESTED | |
| Escape → pause → menu returns; close restores controls | NOT TESTED | |
| Save / reload keeps roles | NOT TESTED | |
| MAM division tree visible, research + clearance effect | NOT TESTED | |
| Exception grant + expiry | NOT TESTED | |
| UI 720p / 1080p / 1440p | NOT TESTED | |
| Remote client: refusal, lobby, reconnection | BLOCKED | needs a second real player |

## In-game checklist (5 min, solo)
1. Creative or new save, skip intro → setup screen appears (Escape = pause, comes back after).
2. 01 → Confirm → choose Exploration → toast + badge top-left.
3. Build gun → Constructor: red hologram "Reserved to Industry & Production", click does nothing, inventory unchanged.
4. F7 → Administration → Coverage "Strict" → still refused; grant yourself Industry 15 min → constructor builds.
5. MAM: tree "FICSIT – Exploration & Research" with 3 nodes.
6. Save, reload → no division menu, badge still Exploration.
Logs: `%LOCALAPPDATA%\FactoryGame\Saved\Logs\FactoryGame.log`, search `LogFR`.
