import unreal, re
out = []
for name in ["AlienOrganisms","AlienTech","Caterium","Mycelia","Nutrients","PowerSlugs","Quartz","Sulfur","XMas"]:
    cls = unreal.load_class(None, "/Game/FactoryGame/Schematics/Research/BPD_ResearchTree_%s.BPD_ResearchTree_%s_C" % (name, name))
    cdo = unreal.get_default_object(cls)
    try:
        nodes = cdo.get_editor_property("mNodes")
    except Exception as e:
        out.append("%s ERR %s" % (name, e)); continue
    coords = []
    for n in nodes:
        try:
            t = n.get_editor_property("mNodeDataStruct").export_text()
        except Exception as e:
            t = str(e)
        m = re.search(r"Coordinates[^=]*=\(X[^=]*=(-?\d+),Y[^=]*=(-?\d+)\)", t)
        coords.append(m.groups() if m else ("?", t[:200]))
    xs = [int(c[0]) for c in coords if c[0] != "?"]; ys = [int(c[1]) for c in coords if c[0] != "?"]
    out.append("%s n=%d X[%s..%s] Y[%s..%s] %s" % (name, len(nodes), min(xs or [0]), max(xs or [0]), min(ys or [0]), max(ys or [0]), coords[:40]))
open(r"F:\FICSIT-Roles-Rebuild\diagnostics\vanilla-mam.txt", "w").write("\n".join(out))
