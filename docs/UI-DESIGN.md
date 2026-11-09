# UI design

Native Slate, built in code, texts from localization files (no text in textures except logo/banner).
Style contract: `Source/FICSITRoles/Public/UI/FRStyle.h` (palette, type, chamfer shapes, motion, spacing).

## References analysed (`references/`)
| Image | Kept | Rejected / improved |
|---|---|---|
| reference-players.png (user) | 4 tall cards in 4 columns, crew illustration per count, orange selected frame, corner stripes | baked texts, screenshot composition, glow → native numbers/labels, chamfer frame, subtle hover lift |
| reference-division.png (user) | pictograms per division, orange accents, bevelled cards | 2×2 layout without information → 4 columns with domain, capabilities, restrictions, availability, detailed sheet |
| Icons.png (old project, transparent) | 4 division pictograms | split + trimmed to `div_*.png` 256 px |
| PlayersAtlas-v2.png (old project, transparent) | crew illustrations | baked frame and stripes cropped out → `players_1..4.png` |

## Screens
1. Configure the session (host): 4 cards 01–04, extended 5–8 row, honest note on the 4-player default.
2. Waiting room: real slots (connected / free / offline), host, network, coverage, start policy; host-only start/back.
3. Division: 4 columns (icon, name, domain, capabilities, restrictions, live availability), detailed sheet with MAM clearances; server-confirmed.
4. FICSIT terminal (F7): Overview · Divisions · Permissions · Communications · Administration (host).
HUD: discreet division badge top-left; toasts top-right (max 4, 4.5 s, de-duplicated).

## Input contract
One modal owner (`UFRShellWidget`, viewport Z 10000). Open: UIOnly + cursor + look/move ignored (only what we blocked is restored).
Escape: terminal → close; onboarding → close + vanilla pause menu, reopen once the pause menu closes. "Later" always available.
Close: input restored immediately (not on GC). Hotkey ignored while typing or when another game menu is open.
