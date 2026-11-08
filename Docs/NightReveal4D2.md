# Transformation nocturne 4D.2 — 8 novembre 2026

Implémentée dans `Lvl_BuildingTest`, sans déplacement des 372 composants géométriques préexistants contrôlés.
Les disponibilités proviennent exclusivement de `ABotanicusGameState::IsMainShopOpen/IsIllegalTradeActive`.
Aucune modification des transactions, PNJ, cultures, exposition ou format de sauvegarde.

- Blueprint : `/Game/Botanicus/blueprints/BP_BotanicusClandestineDoor`, parent `ABotanicusClandestineDoor`.
- Passage mesuré : largeur 665,38 cm, sol Z 147,356, sous-plafond arrière Z 470,346.
- Porte : origine (-7569,207 ; -7670,8235 ; 147,406), yaw 0° (rotation corrigée de 180°), largeur 663,379, hauteur 320,99, épaisseur 6 cm.
- Huit sections sur trajet paramétrique vertical/courbe 90°/horizontal, rayon 55 cm. Rétraction côté clandestin. Le plafond bas impose une partie haute fixe de 63 cm intégrée à la porte : hauteur libre 257,99 cm, implantation et hauteur fermée conservées. Le bâtiment reste inchangé.
- Panneaux remplaçables, primitives Cube existantes ; matériaux `Materials/NightReveal/M_SectionalDoor` et `M_ClandestineNeon`.
- Collision physique autoritaire distincte de l’obstacle NavMesh, suffisamment large pour la grille Recast. Blocage Pawn réaffirmé, maintenu jusqu’à la fin effective de l’ouverture. NavMesh dynamique existant réutilisé. Aucune réplication des transforms de panneaux.
- Fermeture différée si un Pawn occupe le passage ou un joueur se trouve dans les trois volumes arrière configurés. La détection utilise la capsule du Character ; la sortie de secours exige que son centre soit réellement dans une zone arrière. Les chevauchements de corps/mains et la proximité depuis le public ne déclenchent pas d’ouverture diurne. Extinction avant fermeture.

## Réglages artistiques

Porte : dimensions, nombre de sections, mesh/matériaux, rayon, orientation de l’acteur, courbe et durées (3 s), dégagement des Pawns (220 cm), volumes de sécurité, délai néon (1,75 s), allumage (1,4 s), extinction (1,2 s), trois scintillements doux (profondeur 0,32), teinte lavande et émission, références/volumes audio.
`bCosmeticEffects` coupe les sons et oscillations cosmétiques ; l’accès, l’éclairage stable et le HUD restent fonctionnels.
Director : veilleuse 200 lm sur les trois sources référencées, teinte ambrée, transition 3 s. Les éclairages des joueurs restent indépendants.
HUD : bandeau existant dans `UBotanicusClockWidget`, fond vert original en journée, sombre fermé, bordeaux clandestin ; transition 0,25 s. Aucun asset HUD modifié.

## Audio et limites

Les trois MP3 fournis sous `C:/Users/quent/Documents/Botanicus/sons` restent inchangés.
Dérivés mono PCM 48 kHz importés officiellement dans `Audio/Clandestine/S_ClandestineDoor_Open`, `S_ClandestineDoor_Close`, `S_ClandestineNeon_Start`.
Moteurs longs (12,888 / 12,408 s) coupés avec fondu à la fin des mouvements ; crépitement extrait à la lecture sur 1,4 s. Pas d’accélération du son.
Architecture audio et atténuation d’interaction existantes réutilisées (rayon 80 cm + décroissance 1500 cm), sélection locale synchronisée sans RPC audio supplémentaire.
Contact mécanique facultatif : référence `EndContactSound` disponible, non renseignée faute de son dédié identifié. Bourdonnement facultatif non ajouté.
À calibrer à l’écoute dans Editor : niveau des moteurs et du crépitement. L’enregistrement audio offscreen était silencieux ; la vidéo livrée est muette et ne certifie pas le mix audible.

## Validation ciblée

Compilations Development Win64 **BotanicusEditor et Botanicus réussies sous UE 5.8.2**.
PIE hôte + client, troisième joueur en connexion tardive, puis restauration avec trois joueurs : états 14 h, 19 h 30, 21 h, 02 h, 05 h, 08 h ; rétraction, néons différés, HUD, sweeps joueur/PNJ, navigation fermée/ouverte, passage/arrière occupés, minuit et sauts rapides validés. Après restauration : état nocturne stable, aucun son de transition rejoué. Contrôle PIE hôte/client supplémentaire sur le binaire final réussi.
Les compteurs confirment un déclenchement moteur et un crépitement par pair pour l’ouverture observée. Aucun benchmark, test à quatre joueurs ou régression historique exécuté ; pas de certification d’un parcours commercial PNJ complet ni d’écoute artistique.
Rejeu optionnel : `Tools/NightReveal4D2/ValidateNightReveal.py`, via Python Unreal avec `-BotanicusNightValidation -BotanicusCashValidation` et un `-UserDir` dont le SavedDir contient `/isolated_user/`. Le script refuse un profil normal, manipule l’heure et sauvegarde seulement dans ce profil de test, puis quitte l’éditeur.
Vidéo : `Botanicus/Saved/Diagnostics/Night4D2/NightReveal.mp4` (captures PIE, 8 images/s rééchantillonnées à 30).
Git non commité ; verrous LFS antérieurs conservés. Catalogue audio, personnage/footsteps, sauvegardes normales et MP3 contrôlés par empreintes : inchangés.

Correction orientation/collision : PIE listen-server + client avec déplacements réels via CharacterMovement à 14 h, 20 h, pendant l’ouverture de 21 h, à 22 h, 02 h, 05 h et 08 h validés ; contrôle PNJ/NavMesh, passage et arrière occupés, connexion tardive et sauvegarde/restauration dans un profil isolé. Enveloppes des panneaux relevées pendant l’ouverture sans intersection avec les enveloppes des murs/plafonds existants. Aucun autre chantier testé ou modifié.
