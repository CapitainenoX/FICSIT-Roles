# Known issues & real limits (2026-11-08)

| # | Limit | Impact | Workaround |
|---|---|---|---|
| 1 | DISMANTLE / CRAFT / EQUIP / COLLECT are modelled but not enforced | Anyone can dismantle a reserved machine (refund = vanilla) | Deliberate: no reliable loss-free interception; RP rule, host journal |
| 2 | Windows client build only (no WindowsServer/LinuxServer binaries yet) | Dedicated servers cannot load the mod | Listen-host sessions (host plays) work; build `FactoryServer` targets to add servers |
| 3 | Dedicated server admin = first player of an unconfigured session | Wrong person may become host | Configure right after creating the server |
| 4 | MAM node coordinates/links are bound by reflection on the cooked `BPD_ResearchTreeNode` | If a game update renames its fields, division research disables itself | Clearances then count as granted (never blocks); terminal shows "Unavailable" |
| 5 | Research launched by the wrong division is cancelled after items were deposited in the MAM | Items stay deposited (vanilla behaviour), research not started | Right division launches it |
| 6 | `ficsit apply` removes non-profile mods | Manual install disappears after a ficsit-cli apply | Redeploy (`scripts\deploy.ps1`) or install via mod manager |
| 7 | Language is read at world load (MAM texts) | Changing language mid-game updates UI/toasts, MAM names after reload | Reload the save |
| 8 | Old saves from the 0.x Lua prototype | Their roles are not migrated (different storage) | Players pick again once |
