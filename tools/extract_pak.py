"""Copies the cooked pak (Alpakit archive) to build/cooked/Content and points deploy/package at it."""
import os, re, zipfile

ROOT = r"F:\FICSIT-Roles-Rebuild"
z = zipfile.ZipFile(os.path.join(ROOT, r"SMLProject\Saved\ArchivedPlugins\FICSITRoles\FICSITRoles.zip"))
for n in z.namelist():
    p = n.replace(chr(92), "/")
    if p.startswith("Windows/Content/") and not p.endswith("/"):
        out = os.path.join(ROOT, "build", "cooked", *p.split("/")[1:])
        os.makedirs(os.path.dirname(out), exist_ok=True)
        open(out, "wb").write(z.read(n))
        print(out)
OLD = r"'SMLProject\Mods\FICSITRoles\Saved\StagedBuilds\Windows\FactoryGame\Mods\FICSITRoles\Content'"
NEW = r"'build\cooked\Content'"
for f in ("deploy.ps1", "package.ps1"):
    path = os.path.join(ROOT, "scripts", f)
    s = open(path, encoding="utf-8").read()
    open(path, "w", encoding="utf-8").write(s.replace(OLD, NEW))
