# Équilibrage — FICSIT Roles (schema 2, révision 8)

Source : `Mod/FICSITRoles/Resources/Config/FICSITRoles.json` (sauvegarde rev 7 : `backups/config-rev7.json`). Noms vérifiés contre `diagnostics/inventory/*.txt`. Contrôle : `python tools/validate_config.py` → `RESULT: CLEAN`.

## Principes
- **Tout appartient à une base.** Plus de « commun » : `_common` reste vide (documenté). Le validateur vérifie chaque Build_, Equip_, recette d'établi/atelier et ramassable ; seules les classes internes/test sont ignorées.
- **La base couvre tout le métier, tous paliers** : l'Industrie construit toutes les machines dès le départ, la Logistique tous les convoyeurs/trains/drones/tuyaux, l'Énergie tous les générateurs. Les nœuds MAM sont des **formations** : surtout des habilitations croisées, parfois une spécialité (ex. Énergie « Manipulation nucléaire » = Hazmat). Le HUB/MAM vanilla reste obligatoire pour débloquer les recettes.
- **Attribution** : convoyeurs/lifts/séparateurs/fusionneurs (tous Mk) et tuyaux → Logistique ; poteaux, câbles, prises (tous Mk), générateurs, extracteurs de fluides (eau, pétrole, puits) → Énergie ; machines et foreuses → Industrie (les autres : formation « Habilitation forage ») ; MAM, tours, radar, portails, armes, équipement, artefacts aliens → Exploration ; structure et décor → Logistique + Généraliste. HUB, établi, atelier : toutes les divisions. Pièces de base à la main (plaques, tiges, vis, fils, câbles, béton, biomasse), Xeno-Zapper, inhalateur, baies/noix/champignons : toutes les divisions.
- **Généraliste** = palier 0-2 de tout (Mk1 convoyeurs/poteaux, fondeuse, constructeur, assembleuse, foreuse Mk1, brûleur, structure) : le solo n'est jamais bloqué. Rien d'avancé.
- **Grille MAM 7×9** : une racine (palier 1, colonne 3), puis des **couloirs** = une colonne = une carrière ; chaque nœud dépend du nœud au-dessus, le premier d'un couloir de la racine (route libre). Rangée = palier − 1.
- **Coûts ≈ 4× rev 7** : T1 200-500 pièces de base → T5 plastique/caoutchouc/circuits par centaines → T7 aluminium/dissipateurs/batteries → T8 supercalculateurs/cadres fusionnés. Artefacts en coût pour les formations aliens (sphères de Mercer).

## Exploration — 25 nœuds, 6 couloirs
Racine : Formation de terrain (échelles, stockage Mk1). Avant-postes : biomasse + poteaux Mk1 → charbon + tuyaux → batterie → poteaux Mk2 → géothermie → augmenteur alien. Forage : mineur portable → habilitation forage → foreuse Mk2 → pétrole → puits. Logistique de terrain : convoyeurs Mk1 → dépôt dimensionnel → stockage Mk2. Mobilité : tremplins → hypertubes → ascenseur → tyrolienne. Construction : fondations → murs → passerelles/aménagements. Atelier : fondeuse + constructeur → sink → conditionneur.
Kit : Xeno-Basher, scanner, 5 inhalateurs, 20 plaques, 20 tiges.

## Énergie — 24 nœuds, 7 couloirs
Racine : Formation de centrale. Fluides : tuyaux → pompes/vannes → Mk2 → camions fluides. Forage du charbon : Mk1 → Mk2. Convoyage : Mk1 → Mk2-3 → routage. Usinage : forge → assembleuse → fonderie → raffinerie → manufacturier → accélérateur. Labo & sécurité : limaces/fragments → violets → masque → manipulation nucléaire (Hazmat). Construction (3). Reconnaissance : tour + radar.
Kit : 60 fils, 30 câbles, 60 biomasse, 100 feuilles, 20 tiges.

## Industrie — 25 nœuds, 7 couloirs
Racine : Formation de ligne. Convoyeurs : Mk1 → Mk2 → Mk3 + stockage Mk2 → tri. Énergie d'usine : poteaux Mk1 → brûleur → charbon → réseau Mk2. Fluides : tuyaux → eau → pétrole → Mk2 → puits. Overclocking : bleus → jaunes → violets → Somersloops. Sécurité : masque → Hazmat. Construction (3). Fret : camions → rail.
Kit : 60 lingots de fer, 40 de cuivre, 30 plaques, 30 tiges, 80 vis, mineur portable.

## Logistique — 24 nœuds, 6 couloirs
Racine : Repérage (tour). Ateliers : forge → assembleuse → fonderie → conditionneur → raffinerie. Énergie de dépôt : poteaux Mk1 → brûleur → charbon → pylônes + Mk2 → interrupteurs/batteries. Équipes : masque → Blade Runners/parachute → jetpack → tyrolienne → hoverpack. Plans : sink → Blueprint Designer → Mk2. Forage : Mk1 → Mk2. Réseau (T6-8) : radar → sphères de Mercer → portails.
Kit : 50 plaques, 40 tiges, 60 vis, 40 béton, 30 fils.

## Généraliste — 14 nœuds (T1-T4)
Racine : AWESOME Sink. Fonderie → foreuse Mk2 ; charbon → poteaux Mk2 → batteries ; eau + tuyaux → pompes ; convoyeurs Mk3 → stockage Mk2 → camions ; fusil à barres → masque ; overclocking (fragments bleus, limaces jaunes).
Kit : 30 plaques, 30 tiges, 30 fils, 10 câbles, 50 feuilles, mineur portable, 2 inhalateurs.
