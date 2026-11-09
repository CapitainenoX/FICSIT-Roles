"""MAM nodes without parent links (crash suspect); F7 = overview only (no tech tab, no key change)."""
import os

SRC = r"F:\FICSIT-Roles-Rebuild\Mod\FICSITRoles\Source\FICSITRoles"


def rw(rel, fn):
    p = os.path.join(SRC, rel)
    s = open(p, encoding="utf-8").read()
    open(p, "w", encoding="utf-8").write(fn(s))


def must(s, a):
    assert a in s, a[:100]
    return a


def research(s):
    old = "			Layout.Write(TreeNode, SchematicClass, Coords[N->Id], Parents);"
    new = ("			// Parent links are NOT written: the game pairs them with an internal ChildrenAndRoads map we cannot\n"
           "			// fill safely; every crash happened with links set. Tier order is enforced by the research hook.\n"
           "			Layout.Write(TreeNode, SchematicClass, Coords[N->Id], TArray<FIntPoint>());")
    return s.replace(must(s, old), new)


rw("Private/FRResearch.cpp", research)


def screens(s):
    # F7: overview only
    a = s.index("			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 0, 0, 18)\n			[\n				SNew(SHorizontalBox)\n				+ SHorizontalBox::Slot().AutoWidth()[ Tab(0")
    b = s.index("			+ SVerticalBox::Slot().FillHeight(1).VAlign(VAlign_Center)[ SAssignNew(Body, SBox) ]", a)
    s = s[:a] + s[b:]
    s = s.replace(must(s, "		Body->SetContent(Page == 0 ? Overview(*Roles, *Self) : TechTree(*Roles, *Self));"),
                  "		Body->SetContent(Overview(*Roles, *Self));")
    # remove the key row (from the key.label slot to the end of the overview chain)
    a = s.index("			+ SVerticalBox::Slot().AutoHeight().Padding(0, 26, 0, 0)\n			[\n				SNew(SHorizontalBox)\n				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 14, 0)[ Label(FFRLoc::T(TEXT(\"key.label\"))")
    b = s.index("			];", a) + len("			];")
    s = s[:a] + "			;" + s[b:]
    return s


rw("Private/UI/FRScreens.cpp", screens)
print("patched v10")
