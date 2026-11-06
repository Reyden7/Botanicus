# Botanicus — Placement Juice 4A.1

Rapport du 6 novembre 2026. Projet réel : `C:/Users/quent/Documents/repos/Botanicus/Botanicus/Botanicus.uproject`. Unreal Engine **5.8.2**. Aucun commit effectué.

Le placement dispose d'un hologramme cyan/corail et d'un POP cosmétique de **0,37 s** après validation serveur. Le transform de l'acteur physique n'est jamais animé. Les collisions, la réplication et les données sauvegardées restent celles du placement natif. Aucun modèle, mesh, asset audio, Niagara ou éclairage existant n'est modifié.

## Référence et rendu

Vidéo consultée : `C:/Users/quent/Videos/Enregistrements d’écran/Enregistrement de l'écran 2026-11-06 115246.mp4`, 806 × 460, 30 images/s, 13,6 s. Le fauteuil reste holographique jusqu'à environ 2,000 s, apparaît minuscule à 2,033–2,067 s, grandit fortement vers 2,100–2,167 s et se stabilise vers 2,200–2,300 s. La lecture image par image suggère une croissance très brève suivie d'une stabilisation ; elle ne permet pas de mesurer une courbe physique exacte ni l'opacité du matériau. Le départ exact depuis une main n'est pas clairement discernable : cette phase suit les durées demandées dans le brief, sans prétendre l'avoir mesurée dans la référence. [Planche image par image](<C:/Users/quent/.codex/visualizations/2026/11/05/01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb/placement/reference_pop_detail.jpg>).

[Vidéo du placement complet](<C:/Users/quent/Documents/repos/Botanicus/Docs/PlacementJuice4A1_Captures/PlacementJuice4A1.mp4>) : hologramme bleu, obstacle et rouge, retour au bleu, rotation native, confirmation, trajet, croissance et rebond. Capture réelle d'un viewport PIE, sans changement de vitesse du jeu, muette. Les images capturées sont replacées suivant leurs temps de jeu dans un MP4 à 30 images/s ; ce conteneur ne signifie pas 30 images distinctes capturées par seconde. 96 images sources, durée 4.56 s, intervalle maximal 63.3 ms. La capture de présentation est à un joueur ; les tests fonctionnels et le profilage utilisent quatre mondes rendus.

| Bleu | Rouge | Croissance | Dépassement | Repos |
|---|---|---|---|---|
| [PNG](<C:/Users/quent/Documents/repos/Botanicus/Docs/PlacementJuice4A1_Captures/hologram_blue.png>) | [PNG](<C:/Users/quent/Documents/repos/Botanicus/Docs/PlacementJuice4A1_Captures/hologram_red.png>) | [PNG](<C:/Users/quent/Documents/repos/Botanicus/Docs/PlacementJuice4A1_Captures/pop_grow.png>) | [PNG](<C:/Users/quent/Documents/repos/Botanicus/Docs/PlacementJuice4A1_Captures/pop_overshoot.png>) | [PNG](<C:/Users/quent/Documents/repos/Botanicus/Docs/PlacementJuice4A1_Captures/placed_final.png>) |

## Systèmes existants réutilisés

Niveau testé : `/Game/Botanicus/Maps/Lvl_BuildingTest`. Dans [BotanicusPlayerController.cpp](<C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/BotanicusPlayerController.cpp>), les résolveurs `ResolveQuickBarItemPlacement` et `ResolveLargeEquipmentPlacement` restent responsables du sol, des collisions, des dimensions, des surfaces, des réservations et des portées. Aucune condition de ces résolveurs n'est modifiée.

Les trois points d'émission réussie sont `ServerPlaceQuickBarItem_Implementation`, `ServerConfirmPlaceableItemMove_Implementation` et `ServerConfirmLargeEquipmentPlacement_Implementation`. L'événement cosmétique est émis après la validation et l'application du transform. Les parcours particuliers existants de fusion de piles et de montage sur un présentoir gardent leurs retours anticipés ; cette passe ne remplace pas leur présentation spécifique.

`LocalQuickBarItemPreview` et l'état de placement des gros équipements restent les prévisualisations natives. L'hologramme suit leur validité. `LocalEquippedQuickBarItem` et `LocalInspectedQuickBarItem` fournissent le départ tenu lorsqu'ils correspondent à l'objet. Sinon, le système utilise une position proche de la main/caméra ; à distance, le socket/bone `hand_r` est utilisé s'il existe, puis un fallback à partir de la vue du Pawn. L'identité et les mises à jour des outils équipés du chantier 3B.1 restent inchangées.

## Implémentation

Le gestionnaire existant [BotanicusJuiceWorldSubsystem](<C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/Visuals/BotanicusJuiceWorldSubsystem.h>) est étendu par [BotanicusPlacementJuice.cpp](<C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/Visuals/BotanicusPlacementJuice.cpp>). Aucun second manager ni acteur d'animation n'est ajouté. Son Tick est actif uniquement lorsqu'un feedback existant, une prévisualisation, une attente de réplication ou un POP en a besoin. Aucun Tick permanent de meuble n'est ajouté.

### Hologramme

Les types de cache et de copie sont définis dans [BotanicusPlacementJuiceTypes.h](<C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/Visuals/BotanicusPlacementJuiceTypes.h>). Les fonctions de banc restent dans [BotanicusPlacementValidation.cpp](<C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/Editor/BotanicusPlacementValidation.cpp>).

Un seul nouvel asset est créé avec `MaterialEditingLibrary` et les API d'assets Unreal :

- `/Game/Botanicus/Materials/Juice/Placement/M_PlacementHologram`
- Fichier : [M_PlacementHologram.uasset](<C:/Users/quent/Documents/repos/Botanicus/Botanicus/Content/Botanicus/Materials/Juice/Placement/M_PlacementHologram.uasset>).

Matériau Unlit, translucide, deux faces, Fresnel discret et pulsation de 1,5 %. Une MID par prévisualisation colore les meshes et les acteurs attachés ; les widgets sont exclus. Les matériaux d'origine sont conservés puis restaurés. Les meshes Nanite basculent temporairement sur leur fallback **au niveau du composant** avec `SetForceDisableNanite`, car le matériau translucide n'est pas accepté par leur chemin Nanite. Le drapeau initial est restauré ; aucun asset de mesh n'est réenregistré. Ce point a été vérifié dans les logs et les captures.

La transition approche sa couleur cible à 95 % en 0,10 s. La validité provient toujours du résolveur existant, jamais d'une autorisation inventée par le shader. Aucun contour global de scène n'est ajouté. Le matériau bleu historique et `MI_CanNot_Build` restent disponibles pour le mode classique.

### POP et acteur physique

| Phase | Durée par défaut | Courbe / résultat |
|---|---:|---|
| Trajet | 0,10 s | SmoothStep, arc de 7 cm ; silhouette tenue plafonnée à 55 cm |
| Croissance | 0,15 s | Ease Out cubique, échelle 0,30 → 1,08 |
| Stabilisation | 0,12 s | Oscillation amortie, retour exact à 1, petite élévation parabolique de 2,5 cm |

Les phases sont contiguës, pour 0,37 s hors délai de transport réseau/attente du premier Tick. Des `CurveFloat` optionnelles normalisées 0–1 peuvent remplacer les courbes natives. Le sol est abordé à partir des bounds ; la politique de pivot de mobilier déjà utilisée par le contrôleur est réutilisée, avec une surcharge artistique `GroundPivotItems`. L'échelle et la rotation finales des composants sont mémorisées et restaurées exactement côté rendu. Le placement physique demeure exactement celui du résolveur, y compris les décalages déjà présents dans les blockouts.

Les copies sont des composants transitoires locaux, sans acteur propriétaire, collision, overlaps, navigation ni réplication. Les StaticMesh multi-composants sont copiés avec leurs matériaux et transforms ; les ISM simples sont bornés à 1024 instances ; les sections procédurales à 12 000 sommets cumulés par objet. Les ISM avec données personnalisées, les géométries animées non prises en charge ou les dépassements de budget gardent simplement l'acteur normal. Les politiques Nanite et WPO des composants statiques sont conservées sur les copies.

Le rendu de l'acteur réel est suspendu avec ses drapeaux locaux MainPass/DepthPass/Shadow/CustomDepth/RayTracing. **Ni `SetActorHiddenInGame` ni `SetVisibility` ne sont utilisés sur l'acteur réel par le POP** : `bVisible` peut être répliqué. Le transform, la collision et la visibilité logique restent actifs. Les drapeaux sont rétablis à la fin et les copies détruites ; la destruction, le changement de transform, la perte d'un composant et la fin de monde nettoient également les copies. Le contact sonore différé est abandonné si l'animation est interrompue.

Les MID de feuillage 3B/3C sont conservées derrière le matériau holographique : `BindFoliage` consulte le cache natif au lieu de recréer une MID de mouvement chaque fois qu'il rencontre la MID de l’hologramme. Cette adaptation reste dans le gestionnaire visuel existant.

### Réseau et audio

Le [relais cosmétique existant](<C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/Visuals/BotanicusJuiceRelay.cpp>) transporte un lot non fiable de succès de pose ; il ne transporte aucune frame d'animation. L'hôte écoute son succès immédiatement et ignore le retour multicast local, ce qui évite un doublon. Les succès d'un même tour serveur sont réunis au Tick suivant dans un message borné à 32 événements, avec un timer uniquement lorsqu'il y a du travail. Les clients dédoublonnent par séquence et attendent brièvement la représentation répliquée correspondante, au plus 0,20 s. En cas d'événement perdu ou de représentation indisponible, l'objet physique apparaît normalement. Aucun historique d'animation n'est sauvegardé ou rejoué à l'arrivée tardive.

Le test de charge a confirmé la limite UE `net.MaxRPCPerNetUpdate=2`, déclarée dans `Engine/Source/Runtime/Engine/Private/DataReplication.cpp`. Le premier prototype à un RPC par pose produisait 96 animations côté hôte mais 48 par client sur un bloc de 25 s. Le regroupement supprime ce dépassement sans modifier la CVar globale, la fiabilité des autres événements ou les règles de gameplay. Les mesures définitives ci-dessous concernent la version groupée.

Les anciens points `EmitEquipmentPlace` des trois chemins réussis sont remplacés par ce succès cosmétique ; ils ne sont pas conservés en parallèle. [BotanicusAudioWorldSubsystem](<C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/Audio/BotanicusAudioWorldSubsystem.cpp>) expose une entrée locale limitée à `PotPlace`/`EquipmentPlace`, qui réemploie le catalogue, les sons, l'atténuation, les cooldowns et la concurrence existants. Elle ne réutilise pas les numéros de séquence du relais audio. Le son est joué au contact final, une fois dans les essais isolés. Les placements simultanés restent soumis à la concurrence audio existante. Aucun son n'est importé et aucun whoosh n'est ajouté ; une future passe peut évaluer son intérêt à l'écoute.

## Réglages artistiques et budgets

[BotanicusPlacementJuiceSettings.h](<C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/Visuals/BotanicusPlacementJuiceSettings.h>) expose **Botanicus Placement Juice** dans les paramètres du projet, configuration Game.

- `bPlacementJuiceEnabled` désactive l'ensemble de cette présentation ; `bPlacementHologramEnabled` ne désactive que le nouvel hologramme.
- Bleu `#46B8FF`, liseré `#8CE6FF` ; corail `#FF4F65`, liseré `#FF9AA5` ; opacité 0,42, Fresnel 0,65, émission 1,1, transition 0,10 s.
- `bFlyFromHand`, durées, échelle initiale, dépassement, hauteur de rebond, arc, taille maximale en main et courbes optionnelles.
- Maximum **4 animations**, **48 composants par objet**, **128 composants cosmétiques par monde**, distance **18 m** ; limites absolues supplémentaires dans le code.
- `NoFlyItems`, `NoAnimationItems` et `GroundPivotItems` règlent la politique par clé d'article.
- Par défaut, Effects Quality 0 supprime le POP tout en conservant la prévisualisation utile. Le mode classique reste utilisable et les règles natives sont inchangées.

Les scalaires/couleurs sont lus en cours de jeu ; les durées sont figées au début d'un POP. Les références de matériau/courbes sont chargées au début du monde : relancer PIE après leur remplacement. Le dossier du nouveau matériau est ajouté au cook dans [DefaultGame.ini](<C:/Users/quent/Documents/repos/Botanicus/Botanicus/Config/DefaultGame.ini>).

## Validation réelle

Builds **BotanicusEditor Win64 Development** et **Botanicus Win64 Development** réussis avec UE 5.8.2. [Log Editor](<C:/Users/quent/.codex/visualizations/2026/11/05/01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb/placement/build_editor_batch.log>) et [log jeu](<C:/Users/quent/.codex/visualizations/2026/11/05/01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb/placement/build_game_final.log>). PIE : un listen host et trois clients réels dans le même processus, viewports rendus et audio par joueur, profils de sauvegarde isolés copiés depuis les sauvegardes originales. Aucun test n'écrit dans le dossier de sauvegarde réel.

| Suite | Résultat | Preuve |
|---|---|---|
| Placement 4A.1 | 123 contrôles, réussis | [JSON](<C:/Users/quent/Documents/repos/Botanicus/Botanicus/Saved/Diagnostics/PlacementJuice4A1/validation_placement.json>) |
| Plantation/arrosage 3A | 247 contrôles, réussis | [JSON](<C:/Users/quent/Documents/repos/Botanicus/Botanicus/Saved/Diagnostics/VisualJuice3B/pie_result.json>) |
| Croissance/récolte 3B | 153 contrôles, réussis | [JSON](<C:/Users/quent/Documents/repos/Botanicus/Botanicus/Saved/Diagnostics/PlacementJuice4A1/validation_growth.json>) |
| Équipements 3B.1, audio et arrosage | 185 contrôles, réussis | [JSON](<C:/Users/quent/Documents/repos/Botanicus/Botanicus/Saved/Diagnostics/PlacementJuice4A1/validation_equipment.json>) |
| Noctiflores 3C | 125 contrôles, réussis | [JSON](<C:/Users/quent/Documents/repos/Botanicus/Botanicus/Saved/Diagnostics/VisualJuice3C/validation_noctiflores.json>) |
| Visibilité 3C | 9 contrôles, réussis | [JSON](<C:/Users/quent/Documents/repos/Botanicus/Botanicus/Saved/Diagnostics/VisualJuice3C/visibility.json>) |

Les essais 4A.1 couvrent bleu/rouge déterminés par obstacle, retour au bleu sans reconstruction, rotation, annulation, refus serveur sans consommation, pose hôte et client, poses rapprochées, quatre poses simultanées visibles une fois par joueur, pot vide, réserve d'eau, présentoir, `StorageShelfFloorLarge`, `PreparationWorkbench`, déplacement d'un pot planté, destruction pendant le POP, désactivation, qualité basse, exclusion par article, suppression du vol, budget réduit, sauvegarde/restauration et quatrième joueur arrivant tardivement. Les classes utilisées proviennent des Blueprints existants `/Game/Botanicus/blueprints/BP_Item_PlantPot`, `BP_Item_WaterReserve`, `BP_Item_SalesDisplay`, `BP_Item_StorageShelfFloorLarge` et `BP_WorkBench`. Certaines représentations de catalogue restent des formes de blockout ; elles n'ont pas été remplacées.

Des snapshots pris pendant l'animation contrôlent les transforms physiques inchangés, collisions et réplication actives, absence de masquage global, et copies non répliquées sans collision. Le remplissage natif de terreau démarre alors que le POP est encore actif et fait progresser l'état du pot. Les sons de pose sont comptés une fois par vue après contact. Les maxima échantillonnés sont 1.0800 d'échelle, 2.500 cm de rebond, 5 composants et 4 animations actives dans la suite fonctionnelle ; ce sont des valeurs observées, pas une exploration exhaustive des plafonds configurables.

Deux anciens bancs ont nécessité des adaptations de **fixture uniquement**, conservées dans les nouveaux outils 4A.1, sans modifier les anciens scripts : attente 3B sur les secondes de monde, comme les durées Niagara, plutôt que sur l'horloge murale incluant les captures ; placement des personnages à portée et sans occlusion de Pawn pour le remplissage de réserve. Le premier passage avait échoué sur un seul contrôle de nettoyage 3B et un seul contrôle de recharge client ; ils ne sont pas présentés comme réussis. Les mêmes assertions de fond sont relancées et réussies avec ces préconditions corrigées. Le helper est limité à WITH_EDITOR, PIE, `-BotanicusPlacementValidation` et un SavedDir isolé.

Une collision de noms `TestWorld` entre helpers historiques dans une compilation Unity a également nécessité le renommage local du helper anonyme de [BotanicusNoctifloreValidation.cpp](<C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/Editor/BotanicusNoctifloreValidation.cpp>) en `NoctifloreValidationWorld`. Aucun comportement 3C n'y est changé.

## Performances et comptages

Machine : i5-14600KF, RTX 5070, 32 Go, pilote 616.92. L'autre jeu a été fermé ; GPU au repos vérifié avant les mesures. Profilage DX12 avec quatre viewports rendus, qualité Effects/GI/Reflection/Shading/Shadow/PostProcess 3, Lumen. Les tailles réelles sont `[[1049, 519], [960, 542], [960, 542], [960, 542]]`. Les métriques sont celles de `stat unit` via le helper existant, échantillonnées environ toutes les 0,1 s ; le Render Thread quasi nul de cette API n'est pas interprétable ici.

Protocole : 15 s de chauffe, huit blocs de 25 s avec 5 s de stabilisation, ordre **OFF/ON/ON/OFF puis ON/OFF/OFF/ON**. Quatre pots sont posés simultanément environ chaque seconde, les précédents sont retirés dans le banc. Les mêmes appels de résolveur et RPC serveur, inventaire, réplication et sauvegarde native s'exécutent dans les deux conditions. Les arrosoirs restent sélectionnés ; pour solliciter les notifications sans les remplacer, le banc consomme le pot dans un autre slot via le RPC natif déjà existant. Ce stress n'est pas une nouvelle commande de gameplay. PNJ et horloge sont stabilisés. Aucune capture, compilation ou analyse lourde n'est exécutée pendant la mesure.

| Temps en ms | OFF médiane | ON médiane | Écart ON−OFF | OFF p95 | ON p95 |
|---|---:|---:|---:|---:|---:|
| Game Thread | 20.365 | 20.713 | +0.348 | 84.238 | 85.034 |
| Frame | 20.446 | 20.446 | +0.000 | 37.286 | 37.096 |
| GPU | 16.125 | 16.065 | -0.060 | 17.846 | 17.421 |

871 échantillons OFF et 872 ON. La prévisualisation holographique n’est pas active dans ce benchmark : son coût n’est pas isolé ici. La prévisualisation holographique n’est pas active dans ce benchmark : son coût n’est pas isolé ici. Ces écarts sont descriptifs de toute la scène dans un seul éditeur ; ils ne constituent ni un gain annoncé, ni le coût isolé du gestionnaire. Les créations de gameplay, les sauvegardes et le GC participent aux pics. L'ordre alterné réduit les biais sans les éliminer ; pas d'inférence statistique à partir de points temporellement indépendants fictifs.

| Bloc | État | Échantillons | GT médiane ms | GT p95 ms | Copies créées hôte / C1 / C2 / C3 | Détruites |
|---|---|---:|---:|---:|---|---|
| 1 | OFF | 221 | 18.728 | 83.098 | 0 / 0 / 0 / 0 | 0 / 0 / 0 / 0 |
| 2 | ON | 218 | 19.558 | 83.672 | 96 / 96 / 96 / 96 | 96 / 96 / 96 / 96 |
| 3 | ON | 218 | 21.174 | 85.813 | 100 / 100 / 100 / 100 | 96 / 96 / 96 / 96 |
| 4 | OFF | 218 | 20.972 | 84.662 | 0 / 0 / 0 / 0 | 0 / 0 / 0 / 0 |
| 5 | ON | 218 | 20.713 | 84.490 | 100 / 100 / 100 / 100 | 96 / 96 / 96 / 96 |
| 6 | OFF | 218 | 20.911 | 86.197 | 0 / 0 / 0 / 0 | 0 / 0 / 0 / 0 |
| 7 | OFF | 214 | 20.967 | 52.268 | 0 / 0 / 0 / 0 | 0 / 0 / 0 / 0 |
| 8 | ON | 218 | 21.238 | 86.554 | 100 / 100 / 100 / 100 | 96 / 96 / 96 / 96 |

Le contrôle final après arrêt des émissions relève 468 créations et 468 destructions par monde, sans copie restante. Les nombres de copies suivent les poses effectivement représentées, pas les notifications d'inventaire. Tous les blocs se terminent sans animation active et avec leurs créations/détructions équilibrées. Les compteurs d'acteurs arrosoirs pendant les fenêtres mesurées indiquent **0 création et 0 destruction**. Aucun gain CPU n'est revendiqué ; le nouveau feedback a un coût borné et mesuré dans ces conditions. [Données brutes](<C:/Users/quent/Documents/repos/Botanicus/Botanicus/Saved/Diagnostics/PlacementJuice4A1/profile_placement.json>) ; [synthèse par bloc](<C:/Users/quent/.codex/visualizations/2026/11/05/01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb/placement/performance_summary.json>).

## Intégrité et limites

1371 fichiers préexistants ont été comparés à leur SHA-256 initial. Dans le périmètre initial (Source, Config, Content/Botanicus, SaveGames, Docs et Tools), les fichiers binaires existants du contenu et les sauvegardes originales sont inchangés : **0 différence protégée**. [Détail de préservation](<C:/Users/quent/.codex/visualizations/2026/11/05/01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb/placement/integrity.json>). Le verrou LFS de `Lvl_BuildingTest.umap` reste celui de Reyden7 ; la map n'est pas enregistrée. Aucun commit et aucun nettoyage historique du Blueprint d'arrosoir.

Les outils reproductibles sont dans [Tools/PlacementJuice4A1](<C:/Users/quent/Documents/repos/Botanicus/Tools/PlacementJuice4A1>), notamment `BuildHologram.py`, `ValidatePlacement.py`, `RegressionGrowth.py`, `RegressionEquipment.py`, `ProfilePlacement.py`, `CapturePlacement.py` et `RunValidation.ps1`. Les diagnostics et lectures de pixels sont inertes en dehors de leur contexte isolé autorisé. Le matériau a été créé et sauvegardé par Unreal ; aucun script n'édite des octets `.uasset`/`.umap`.

Limites : validation Steam multi-machines, serveur dédié exécuté, simulation de perte/latence, cook complet et Shipping non réalisés. Le build jeu n'est pas une preuve de packaging. Les événements cosmétiques sont volontairement non fiables : une perte laisse l'acteur normal, sans replay. La copie n'est pas un moteur générique de rendu Blueprint/skinning ; les représentations non prises en charge ou trop lourdes retombent sur le rendu natif, et les autres cas artistiques particuliers peuvent être exclus par clé. Les pivots/décalages physiques existants du catalogue ne sont pas corrigés par ce chantier. Le rendu audio subjectif et le ressenti en coopération avec visée mobile restent à juger en jeu ; la capture est muette et les bancs valident la logique native plutôt qu'un parcours intégral à la souris.
