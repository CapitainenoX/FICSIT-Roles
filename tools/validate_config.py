"""Validate Mod/FICSITRoles/Resources/Config/FICSITRoles.json (rev 8+) against the SDK class inventory.
Usage: python tools/validate_config.py [config.json]   (exit code 1 on error)
Checks: names exist (trailing * = prefix), MAM lane grid 7x9, requirements, costs, ownership by division bases."""
import json, os, re, sys
from collections import Counter, defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CFG = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "Mod", "FICSITRoles", "Resources", "Config", "FICSITRoles.json")
INV_DIR = os.path.join(ROOT, "diagnostics", "inventory")
PERMS = ("build", "craft", "collect", "equip")
PREFIX = {"build": "Build_", "craft": "Recipe_", "collect": "Desc_", "equip": "Equip_"}
COLS, ROWS, ROOT_COL = 7, 9, 3
# Internal / test classes only (not placed or used by players): excluded from ownership.
IGNORE = {
    r"^Build_Cheat": "cheat/test class",
    r"^Build_TetrominoGame": "easter-egg arcade",
    r"^Build_VehiclePath": "auto-recorded vehicle paths",
    r"^Build_Blueprint$": "blueprint proxy (internal)",
    r"^Build_StorageBlueprint$": "blueprint dismantle crate (internal)",
    r"^Build_StoragePlayer$": "player death/dismantle crate (internal)",
    r"Integrated": "HUB-integrated part (internal)",
    r"^Build_AutomatedWorkBench$": "internal workbench automation",
    r"^Equip_DowsingStick$": "unused dev tool",
    r"^Equip_ToolBelt$": "internal inventory component",
}
# Craft-bench / equipment-workshop recipes used by players (curated; the inventory does not say which are bench-enabled).
BENCH = ["Recipe_" + x for x in (
    "IronPlate IronRod Screw Wire Cable Concrete CopperSheet IronPlateReinforced Rotor ModularFrame SteelBeam SteelPipe "
    "EncasedIndustrialBeam Stator Motor Quickwire Silica CircuitBoard AILimiter Fabric Computer ModularFrameHeavy "
    "HighSpeedConnector Biomass_Leaves Biomass_Wood Biomass_Mycelia Biomass_AlienProtein Biofuel PortableMiner "
    "XenoZapper XenoBasher ObjectScanner RebarGun SpikedRebar Rebar_Stunshot Rebar_Spreadshot Rebar_Explosive Nobelisk "
    "NobeliskGas NobeliskShockwave NobeliskCluster NobeliskNuke NobeliskDetonator Gunpowder GunpowderMK2 SpaceRifleMk1 "
    "Cartridge CartridgeSmart CartridgeChaos CartridgeChaos_Packaged JetPack Hoverpack BladeRunners Parachute Gasmask "
    "FilterGasMask HazmatSuit FilterHazmat Chainsaw ZipLine MedicinalInhaler MedicinalInhalerAlienOrgans "
    "NutritionalInhaler TherapeuticInhaler AlienDNACapsule Protein_Hog Protein_Spitter Protein_Stinger Protein_Crab "
    "PowerCrystalShard_1 PowerCrystalShard_2 PowerCrystalShard_3 FactoryCart GoldenCart CandyCaneBasher Snowball "
    "CandyCane Fireworks_01 Fireworks_02 Fireworks_03").split()]
# Collectable world pickups.
PICKUPS = ["DropPod"] + ["Desc_" + x for x in (
    "Leaves Wood Mycelia BerryBush NutBush Berry Nut Shroom HogParts SpitterParts StingerParts HatcherParts "
    "Crystal Crystal_mk2 Crystal_mk3 WAT1 WAT2").split()]

errors, warns = [], []
err = errors.append

def load_inv():
    names = set()
    for f in os.listdir(INV_DIR):
        if f.endswith(".txt"):
            names |= {l.strip() for l in open(os.path.join(INV_DIR, f), encoding="utf-8") if l.strip()}
    return names

def matches(pat, name):
    return name.startswith(pat[:-1]) if pat.endswith("*") else pat == name

def exists(name, inv):
    return name == "DropPod" or (any(n.startswith(name[:-1]) for n in inv) if name.endswith("*") else name in inv)

def main():
    try:
        cfg = json.load(open(CFG, encoding="utf-8"))
    except Exception as e:
        print("FAIL: JSON does not load:", e); return 1
    inv = load_inv()
    for k in ("schema", "configRevision", "divisions", "techTree"):
        if k not in cfg: err(f"missing top-level key {k}")
    if cfg.get("schema") != 2: err("schema must be 2")
    if cfg.get("defaultCoverage") != "Off": warns.append("defaultCoverage is not Off")
    if any(cfg.get("_common", {}).get(p) for p in PERMS): err("_common must stay empty (ownership is by bases)")

    def check_names(where, perm, names):
        for n in names:
            if not isinstance(n, str): err(f"{where}: non-string {n!r}"); continue
            if n != "DropPod" and not n.startswith(PREFIX[perm]): err(f"{where}: {n} is not a {PREFIX[perm]}* name")
            if not exists(n, inv): err(f"{where}: unknown class {n}")

    divs = [d["id"] for d in cfg["divisions"]]
    owned = defaultdict(list)          # perm -> patterns from all bases
    base_of = {}
    for d in cfg["divisions"]:
        base_of[d["id"]] = d.get("base", {})
        for p in PERMS:
            v = d.get("base", {}).get(p, [])
            check_names(f"{d['id']}.base.{p}", p, v); owned[p] += v
        for item, cnt in d.get("startKit", []):
            if not exists(item, inv): err(f"{d['id']}.startKit: unknown {item}")
            if not (isinstance(cnt, int) and cnt > 0): err(f"{d['id']}.startKit: bad count for {item}")
        for r in d.get("startRecipes", []):
            if r.rsplit(".", 1)[-1].removesuffix("_C") not in inv: err(f"{d['id']}.startRecipes: unknown {r}")

    # ---- nodes ----
    nodes = cfg["techTree"]
    for i, c in Counter(n["id"] for n in nodes).items():
        if c > 1: err(f"duplicate node id {i}")
    by_id = {n["id"]: n for n in nodes}
    for n in nodes:
        nid, div, t, col = n["id"], n.get("division"), n.get("tier"), n.get("col")
        if div not in divs: err(f"{nid}: unknown division {div}")
        if not nid.startswith(f"{div}."): err(f"{nid}: id must start with '{div}.'")
        if not (isinstance(t, int) and 1 <= t <= ROWS): err(f"{nid}: tier {t} out of 1..{ROWS}")
        if not (isinstance(col, int) and 0 <= col < COLS): err(f"{nid}: col {col} out of 0..{COLS - 1}")
        for item, cnt in n.get("cost", []):
            if not exists(item, inv): err(f"{nid}: unknown cost item {item}")
            if not (isinstance(cnt, int) and cnt > 0): err(f"{nid}: bad cost count {item}")
        total, useful = 0, False
        own = base_of.get(div, {})
        for p in PERMS:
            v = n.get("grants", {}).get(p, [])
            check_names(f"{nid}.grants.{p}", p, v); total += len(v)
            for g in v:
                if not any(matches(o, g.rstrip("*")) for o in own.get(p, [])): useful = True
        if total == 0: err(f"{nid}: grants nothing")
        elif not useful: warns.append(f"{nid}: grants only what its own base already owns")

    # ---- MAM lane grid, per division ----
    for div in divs:
        dn = [n for n in nodes if n.get("division") == div]
        if not dn: continue
        if len(dn) > 1 + COLS * (ROWS - 1): err(f"{div}: {len(dn)} nodes > grid capacity")
        cells = {}
        for n in dn:
            k = (n["col"], n["tier"] - 1)
            if k in cells: err(f"{div}: {n['id']} and {cells[k]['id']} share cell {k}")
            cells[k] = n
        roots = [n for n in dn if n["tier"] == 1]
        if len(roots) != 1 or roots[0]["col"] != ROOT_COL or roots[0].get("requires"):
            err(f"{div}: need exactly one root at tier 1, col {ROOT_COL}, no requires (got {[r['id'] for r in roots]})")
            continue
        root = roots[0]
        for n in dn:
            if n is root: continue
            req = n.get("requires", [])
            if len(req) != 1: err(f"{n['id']}: needs exactly one parent (has {len(req)})"); continue
            p = by_id.get(req[0])
            if not p: err(f"{n['id']}: unknown requirement {req[0]}"); continue
            if p.get("division") != div: err(f"{n['id']}: parent {p['id']} from another division"); continue
            c, r = n["col"], n["tier"] - 1
            if p["col"] == c and p["tier"] == n["tier"] - 1:
                road = []
            elif p is root:
                step = 1 if c > ROOT_COL else -1
                road = [(x, 0) for x in range(ROOT_COL + step, c + step, step)] if c != ROOT_COL else []
                road += [(c, y) for y in range(1, r)]
            else:
                err(f"{n['id']}: parent {p['id']} is neither the cell above nor the root"); continue
            for cell in road:
                if cell in cells: err(f"{n['id']}: road from {p['id']} crosses {cells[cell]['id']} at {cell}")

    # ---- ownership (bases only) ----
    ignored, unowned = {}, []
    required = [n for n in sorted(inv) if n.startswith(("Build_", "Equip_"))] + BENCH + PICKUPS
    for n in required:
        why = next((w for rx, w in IGNORE.items() if re.search(rx, n)), None)
        if why: ignored[n] = why; continue
        if not exists(n, inv): err(f"required list names unknown class {n}"); continue
        perm = {"Build_": "build", "Equip_": "equip", "Recipe_": "craft"}.get(n[:n.find("_") + 1], "collect")
        if not any(matches(a, n) for a in owned[perm]): unowned.append(n)

    cnt = Counter(n["division"] for n in nodes)
    tiers = defaultdict(Counter)
    for n in nodes: tiers[n["division"]][n["tier"]] += 1
    print(f"config rev {cfg.get('configRevision')} | {len(nodes)} nodes")
    for d in divs:
        t = tiers[d]
        lanes = len({n["col"] for n in nodes if n["division"] == d and n["tier"] > 1})
        print(f"  {d:12s} {cnt[d]:3d}  lanes {lanes}  max/tier {max(t.values()) if t else 0}  "
              + " ".join(f"T{k}:{v}" for k, v in sorted(t.items())))
    print(f"ownership: {len(required) - len(ignored)} targets checked, {len(ignored)} ignored "
          f"({', '.join(sorted(set(ignored.values())))})")
    print(f"unowned: {len(unowned)}" + (f" -> {unowned}" if unowned else ""))
    for w in warns: print("WARN:", w)
    for e in errors: print("ERROR:", e)
    ok = not errors and not unowned
    print("RESULT:", "CLEAN" if ok else f"{len(errors)} error(s), {len(unowned)} unowned")
    return 0 if ok else 1



# --- Save compatibility: every shipped id must still exist or be migrated through "renamed" ---
def _check_ids_lock():
    import json as _j, os as _o
    root = _o.path.dirname(_o.path.abspath(__file__))
    lock = _o.path.join(root, 'ids.lock')
    cfg = _j.load(open(_o.path.join(root, '..', 'Mod', 'FICSITRoles', 'Resources', 'Config', 'FICSITRoles.json'), encoding='utf-8'))
    live = {n['id'] for n in cfg['techTree']} | {v['id'] for v in cfg['divisions']}
    ren = {k: v for k, v in cfg.get('renamed', {}).items() if not k.startswith('_')}
    bad = []
    ids = [l.strip() for l in open(lock) if l.strip() and not l.startswith('#')]
    for i in ids:
        if i in live: continue
        t, seen = i, 0
        while t in ren and seen < 8: t, seen = ren[t], seen + 1
        if t not in live: bad.append(i)
    for k, v in ren.items():
        if v not in live and v not in ren: bad.append('renamed target missing: %s -> %s' % (k, v))
    print('ids.lock: %s' % ('OK' if not bad else 'BROKEN SAVES: ' + ', '.join(bad)))
    with open(lock, 'a') as f:  # append newly shipped ids
        known = set(ids)
        for i in sorted(live - known): f.write(i + '\n')
    return not bad

if __name__ == "__main__":
    _rc = main()
    sys.exit(_rc if _check_ids_lock() else 1)
