# Botanicus — Chantier 3C : ambiance clandestine et Noctiflores

6 novembre 2026 — UE **5.8.2**, Win64 Development. Projet : `C:\Users\quent\Documents\repos\Botanicus\Botanicus\Botanicus.uproject`.

La couche ajoute une émission lavande discrète aux **seules Noctiflores**, des spores occasionnelles mutualisées et un fondu de l’éclairage clandestin existant. Aucun modèle, stade de croissance, prix, horaire, besoin d’eau, collision ou système climatique n’est ajouté ou modifié. Aucun son téléchargé, aucun commit.

## Inspection confirmée

Les cinq documents demandés ont été relus. Au départ, Git contient les changements non commités de 3B.1 sur HEAD `a3a71a9` ; ils sont conservés. Les verrous LFS local et distant indiquent `Lvl_BuildingTest.umap`, propriétaire Reyden7, ID 61708748. Aucun verrou n’est forcé, pris ou libéré par 3C et cette carte n’est pas sauvegardée.

| Croissance actuelle | Mesh existant | Matériau original |
|---|---|---|
| De la plantation à moins de 30 % | `/Game/Botanicus/Items/itemsMesh/Trafic/plantes/1/1.1` | `/Game/Botanicus/Items/itemsMesh/Trafic/plantes/1/Material_002.Material_002` |
| 30 % à moins de 70 % | `/Game/Botanicus/Items/itemsMesh/Trafic/plantes/2/2.2` | `/Game/Botanicus/Items/itemsMesh/Trafic/plantes/2/Material_003.Material_003` |
| Dès 70 %, puis jusqu’à 100 % | `/Game/Botanicus/Items/itemsMesh/Trafic/plantes/3/3.3` | `/Game/Botanicus/Items/itemsMesh/Trafic/plantes/3/Material_004.Material_004` |

**Aucun quatrième mesh à maturité.** La maturité commerciale reste à 100 %. Les trois matériaux originaux sont des instances directes de `/InterchangeAssets/Materials/FBXLegacyPhongSurfaceMaterial`, dont l’émission possède déjà un branchement. Ce branchement est préservé.

`BotanicusIllegalPlanterActor` possède douze composants `IllegalPlantVisual_00` à `_11` préexistants ; les niveaux actuels utilisent quatre emplacements. Ils sont sans collision. Les meshes sont normalisés à la hauteur de croissance existante, puis positionnés au-dessus de la terre. 3C change seulement l’appel de liaison cosmétique pour transmettre l’identité Noctiflore et la progression ; il ne modifie pas ce calcul de pose ou de croissance.

La carte `/Game/Botanicus/Maps/Lvl_BuildingTest` lie une seule source clandestine au directeur : `PersistentLevel.RectLight_3`, label `Ambience_Clandestine`, position (−6400, −7000, 330), sans ombres. D’autres sources intérieures existent dans le blockout courant ; elles restent intactes. Le sol inspecté est vers Z = 147,36 cm. Les jardinières du banc sont placées dans la pièce, à cette hauteur, pas sous le sol ni de l’autre côté du mur X ≈ −6556.

`BotanicusGameState` demeure la référence : activité clandestine selon `IllegalTradeSettings::IsIllegalTradeMinute`, **21 h–05 h**, et boutique principale fermée. Les seuils ne changent pas. Le directeur réutilise `DayTimeMinutes`, le jour et les profils saisonniers. Aucun nouvel acteur de zone, volume de collision, calendrier ou RPC.

L’audio emploie déjà trois boîtes souples avec fondu de 200 cm : boutique (−9107, −8468, 250), préparation (−8000, −8200, 250), clandestine (−6400, −7000, 250), demi-étendue clandestine (650, 700, 250). `WeightAt` utilise la position de la caméra. Le renfort nocturne dépend de ce poids, de la nuit saisonnière et de l’activité clandestine. Ces régions audio ne sont pas des conditions de culture.

## Mise en œuvre

Les variantes de lueur partent des variantes **Living de 3B**, en conservant textures, paramètres importés, émission originale et WPO. Une collection dédiée porte intensité, teinte, vitesse et distance. Un masque de hauteur doux privilégie le haut de la plante ; ce n’est pas un masque peint pour chaque feuille. Le shader anime la pulsation, sans Tick de plante. Sa phase dépend de la position XY du végétal ; elle ne repart pas de zéro quand le mesh de stade change. La force passe progressivement de 0,65 à 1 selon la progression réelle, sans nouveau seuil de stade.

La liaison choisit le catalogue Noctiflore uniquement pour `PlantId == Noctiflore`. Les plantes ordinaires gardent le catalogue Living. Le MID déjà correct est réutilisé ; il n’est pas remplacé à chaque notification. La désactivation met l’émission additionnelle à zéro, en conservant le rendu et le mouvement préexistants. Les paramètres sont appliqués au début du monde puis via le timer local de rapprochement de 0,1 s déjà présent dans Juice.

Les spores utilisent un **nouveau système Niagara CPU à sprites**, copie Unreal du système de gerbe 3A. Un matériau unlit procédural fournit bord doux, apparition progressive et disparition douce selon l’âge normalisé. Pas de texture externe, collision Niagara, grille Fluids, renderer mesh, lumière par plante ou particule répliquée.

Le sous-système Juice conserve des références faibles vers les composants végétaux enregistrés lors de leur liaison. Il choisit à tour de rôle un végétal proche ; il ne parcourt pas tous les acteurs à chaque rapprochement. Les spores partagent un plafond **par monde rendu**, toutes Noctiflores confondues. Elles ne possèdent pas un composant permanent par plante. Destruction, récolte/masquage, changement de mesh, éloignement, baisse du budget, désactivation et fin de monde retirent leurs composants. Un acteur temporairement masqué reprend ensuite sans devoir recréer son enregistrement.

`BotanicusAmbienceDirector` garde ses sources explicites, son Tick existant et son calcul de nuit. Seule sa couche clandestine reçoit une teinte lavande bleutée, un fondu exponentiel et une variation d’intensité maximale de 1,8 %. L’intensité de référence reste **350 lumens**, multiplicateur 1. Le premier état d’un client est appliqué directement ; les changements suivants se fondent. Aucune exposition globale ni lumière intérieure n’est diminuée pour forcer un contraste. Couper 3C rétablit le comportement lumineux précédent ; cela ne coupe pas arbitrairement les sources de lisibilité.

Les réglages Lumen du projet restent `r.DynamicGlobalIlluminationMethod=1` et `r.ReflectionMethod=1`. La lueur est avant tout une propriété visible de la surface. **Aucun éclairage indirect important n’est promis** : sa faible intensité, la taille des plantes et la qualité graphique peuvent le rendre négligeable. Les tests comprennent les qualités GI/réflexions 3 puis 0. Les matériaux d’origine étant déjà roses et la pièce dotée de sources chaudes, l’ajout reste volontairement discret dans les zones très éclairées.

## Réglages artistiques

Project Settings → **Botanicus Noctiflores**, classe `UBotanicusNoctifloreSettings` ; configuration Game. Tous les paramètres sont cosmétiques.

| Groupe | Réglage par défaut |
|---|---|
| Activation | `bEnabled=true` ; `bDisableOnLowEffectsQuality=true` |
| Lueur | `bBioluminescence=true`, `GlowIntensity=0,65`, `GlowTint=(0,42 ; 0,28 ; 0,65)` en linéaire |
| Pulsation | `PulseSpeed=0,65 rad/s`, période ≈ 9,7 s ; facteur shader 0,72 à 1 |
| Distance lueur | 1800 cm ; extinction progressive sur les derniers 30 % |
| Spores | `bSpores=true`, `bSporesOnlyDuringTrade=true` |
| Fréquence | 3 spores par émission ; intervalle global 2,4 s, variation spatiale de ±12 % |
| Forme et mouvement | Taille 1 cm, vitesse 4 cm/s, durée 3,2 s ; faible dérive latérale, sans gravité |
| Couleur des spores | `(0,40 ; 0,34 ; 0,68 ; 0,40)` RGBA linéaire |
| Distance spores | 1000 cm depuis la caméra locale |
| Plafonds | 2 composants et 12 particules réservées au maximum, par monde rendu ; valeurs bornées à l’exécution |
| Éclairage | Teinte `(0,36 ; 0,42 ; 0,78)`, multiplicateur 1, transition 1,6 s vers ≈ 95 %, variation 0,018 |
| Assets | `GlowCatalog` et `SporeSystem`, références modifiables |

Avec les valeurs par défaut, deux émissions de trois sprites permettent au plus six particules simultanées. Le test renforcé passe temporairement à six par émission et vérifie le plafond de douze. Les réservations restent conservatrices jusqu’à l’expiration, même si Niagara termine une particule plus tôt. Le budget 3C est distinct du budget de gerbes 3A/3B ; il ne leur retire aucun emplacement. En qualité d’effets basse, lueur et spores sont coupées ; la variante possède aussi une branche Material Quality Low sans émission additionnelle. L’éclairage existant demeure disponible.

## Réseau et sauvegardes

Chaque pair rendu calcule sa lueur, ses spores et son fondu depuis les données existantes. Les phases organiques et le choix des spores sont locaux et peuvent différer entre pairs. Aucun RPC ni champ répliqué ajouté ; aucune particule sauvegardée. L’état persistant reste celui des slots, traité par la restauration et les OnRep existants. Le serveur listen ne reçoit pas de second déclenchement d’effet. Le chemin dédié quitte l’initialisation de rendu avant le chargement des nouveaux assets et ne rapproche pas les spores.

Les sauvegardes originales sont copiées sous `Botanicus/Saved/Diagnostics/VisualJuice3C/isolated_user`. Les helpers natifs refusent les mutations hors PIE, sans `-BotanicusNoctifloreValidation` ou sans `/isolated_user/`. Leurs actions sont inertes dans la cible jeu. Leurs fixtures peuvent déplacer des joueurs, figer la simulation et remplacer les cultures de **la copie de test uniquement**. Ces opérations n’existent pas dans le parcours normal.

## Validation

| Suite | Résultat | Preuve |
|---|---|---|
| 3C | 125 assertions réussies | [JSON](C:/Users/quent/.codex/visualizations/2026/11/05/01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb/noctiflore/validation_noctiflores.json) |
| 3A | 247 assertions réussies | [JSON](C:/Users/quent/.codex/visualizations/2026/11/05/01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb/noctiflore/regression_3a.json) |
| 3B | 153 assertions réussies | [JSON](C:/Users/quent/.codex/visualizations/2026/11/05/01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb/noctiflore/regression_3b.json) |
| Equipment | 184 assertions réussies | [JSON](C:/Users/quent/.codex/visualizations/2026/11/05/01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb/noctiflore/regression_equipment.json) |
| Visibility | 9 assertions réussies | [JSON](C:/Users/quent/.codex/visualizations/2026/11/05/01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb/noctiflore/visibility.json) |
| Lighting | 3 tests réussis | [JSON](C:/Users/quent/.codex/visualizations/2026/11/05/01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb/noctiflore/automation_lighting/index.json) |

Editor et jeu Win64 Development : compilation UE 5.8.2 réussie. [Log Editor](C:/Users/quent/.codex/visualizations/2026/11/05/01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb/noctiflore/build_editor_final.log) ; [log jeu](C:/Users/quent/.codex/visualizations/2026/11/05/01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb/noctiflore/build_game_final.log).

Le test d’intégration d’éclairage réussit avec un avertissement RecastNavMesh absent dans sa carte temporaire ; les deux autres tests réussissent sans avertissement.

La matrice 3C couvre les trois meshes (dont le dernier à 70 %), la maturité à 100 %, les plantes ordinaires témoins, les seize cultures voisines, 12 h / 22 h / 02 h, l’extinction après 05 h, le retour de l’activité, OFF/ON, les qualités basses et sans GI Lumen, le plafond renforcé de douze particules, le retrait des plantes, la destruction, l’arrivée du quatrième joueur et la sauvegarde/reprise à quatre. Le complément vérifie le démarrage désactivé, le masquage/réaffichage du pot et le rejet à distance.

Les régressions couvrent plantation/arrosage et refus, croissance/récolte des trois types de pots, hotbar, changements rapides, recharge réelle, épuisement, inventaire plein, port physique, réplication, reprise, destruction/respawn de Pawn et fermeture de connexion native. Les cas de transplantation complète et le parcours Steam restent ceux des limites de 3B.1.

Les premiers passages et leurs échecs sont conservés : sept plantes sauvegardées s’ajoutaient aux seize témoins ; le premier placement était sous le sol/hors de la pièce ; un observateur était sur le bord de la région sonore ; une sauvegarde de plantes encore en croissance continuait légitimement à croître après reprise. La matrice finale de reprise utilise seize plantes matures pour comparer exactement les données. Les jeunes et développées sont vérifiées séparément. Le contrôle équipements d’origine épuisait sa réserve pendant la deuxième séquence continue : la copie 3C ajoute une recharge native de 0,2 entre les séquences. Aucun test de refus, d’épuisement ou de réplication n’est supprimé ; aucun script 3A/3B/3B.1 préexistant n’est modifié.

Les vérifications utilisent les méthodes natives, les vraies données répliquées et les composants réellement rendus. Elles ne constituent pas un parcours exhaustif de toute l’interface à la souris, ni une revue artistique par quatre personnes. Les captures de présentation masquent les étiquettes et l’objet tenu uniquement dans leur fixture ; cela ne change pas le HUD livré.

## Performances avec/sans 3C

| Bloc | 3C | Mesures | Frame médiane ms | GT médiane ms | GT p95 ms | GPU médiane ms |
|---|---|---:|---:|---:|---:|---:|
| 1 | OFF | 223 | 18.34 | 18.19 | 23.41 | 14.31 |
| 2 | ON | 223 | 18.55 | 18.13 | 24.90 | 14.35 |
| 3 | ON | 224 | 18.16 | 18.12 | 23.75 | 14.40 |
| 4 | OFF | 223 | 17.80 | 17.69 | 22.63 | 14.31 |
| 5 | ON | 224 | 18.33 | 18.19 | 24.86 | 14.45 |
| 6 | OFF | 222 | 18.42 | 18.07 | 24.44 | 14.67 |
| 7 | OFF | 222 | 18.50 | 18.39 | 25.59 | 15.01 |
| 8 | ON | 224 | 18.18 | 17.96 | 25.26 | 14.41 |

1785 relevés. Moyenne des médianes de blocs OFF → ON : frame **18.27 → 18.30 ms**, GT **18.09 → 18.10 ms** (écart +0.01 ms), GPU **14.57 → 14.40 ms** (écart -0.17 ms). P95 GT regroupé : 24.65 → 25.00 ms.

Résolutions retournées par les quatre PlayerControllers : `[[1049, 519], [960, 542], [960, 542], [960, 542]]`. Réglage demandé par le banc : 960 × 540, quatre vues. Les dimensions effectives suivent l’intégration PIE/Editor et le DPI du poste.

Ce comparatif ne démontre pas un gain général de performance. Les fluctuations de la scène complète et la dérive des blocs limitent la précision des petits écarts. Le Render Thread presque nul retourné dans ce mode n’est pas exploité. Pas de coût GPU isolé du shader ni de borne pour d’autres machines. [Données brutes](C:/Users/quent/.codex/visualizations/2026/11/05/01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb/noctiflore/profile_noctiflores.json) ; [résumé](C:/Users/quent/.codex/visualizations/2026/11/05/01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb/noctiflore/performance_summary.json) ; [GPU avant mesure](C:/Users/quent/.codex/visualizations/2026/11/05/01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb/noctiflore/gpu_before_profile.txt).

Le mode OFF met l’émission additionnelle à zéro, coupe les spores et conserve l’éclairage clandestin antérieur. Les variantes restent liées : cette comparaison mesure la couche désactivable, pas un rollback vers un ancien binaire sans aucun shader 3C. Même scène et caméras, seize plantes, quatre mondes rendus dans un processus, heure 22 h figée, simulation des cultures de charge arrêtée, flux PNJ identifiés retirés dans la copie. Échauffement 15 s ; 5 s de stabilisation par bloc ; huit fenêtres de 25 s, OFF/ON/ON/OFF puis ON/OFF/OFF/ON. Aucun screenshot, compilation, encodage vidéo ou synchronisation des compteurs Niagara pendant les fenêtres mesurées. Les compteurs sont relevés après chaque bloc. Les valeurs incluent l’Editor et les quatre vues, avec les widgets normaux ; elles ne prédisent pas quatre machines en Shipping.

## Assets et fichiers

**8 nouveaux assets**, créés/sauvegardés uniquement via les API Unreal :

- [MPC_Noctiflore](C:/Users/quent/Documents/repos/Botanicus/Botanicus/Content/Botanicus/Materials/Juice/Noctiflore/MPC_Noctiflore.uasset) — `/Game/Botanicus/Materials/Juice/Noctiflore/MPC_Noctiflore.MPC_Noctiflore`
- [M_NoctGlow_2c94337266](C:/Users/quent/Documents/repos/Botanicus/Botanicus/Content/Botanicus/Materials/Juice/Noctiflore/M_NoctGlow_2c94337266.uasset) — `/Game/Botanicus/Materials/Juice/Noctiflore/M_NoctGlow_2c94337266.M_NoctGlow_2c94337266`
- [MI_NoctGlow_949d12d9d7](C:/Users/quent/Documents/repos/Botanicus/Botanicus/Content/Botanicus/Materials/Juice/Noctiflore/MI_NoctGlow_949d12d9d7.uasset) — `/Game/Botanicus/Materials/Juice/Noctiflore/MI_NoctGlow_949d12d9d7.MI_NoctGlow_949d12d9d7`
- [MI_NoctGlow_a49d9930f3](C:/Users/quent/Documents/repos/Botanicus/Botanicus/Content/Botanicus/Materials/Juice/Noctiflore/MI_NoctGlow_a49d9930f3.uasset) — `/Game/Botanicus/Materials/Juice/Noctiflore/MI_NoctGlow_a49d9930f3.MI_NoctGlow_a49d9930f3`
- [MI_NoctGlow_aeaff932e8](C:/Users/quent/Documents/repos/Botanicus/Botanicus/Content/Botanicus/Materials/Juice/Noctiflore/MI_NoctGlow_aeaff932e8.uasset) — `/Game/Botanicus/Materials/Juice/Noctiflore/MI_NoctGlow_aeaff932e8.MI_NoctGlow_aeaff932e8`
- [DA_NoctifloreGlow](C:/Users/quent/Documents/repos/Botanicus/Botanicus/Content/Botanicus/Materials/Juice/Noctiflore/DA_NoctifloreGlow.uasset) — `/Game/Botanicus/Materials/Juice/Noctiflore/DA_NoctifloreGlow.DA_NoctifloreGlow`
- [M_NoctifloreSpore](C:/Users/quent/Documents/repos/Botanicus/Botanicus/Content/Botanicus/Materials/Juice/Noctiflore/M_NoctifloreSpore.uasset) — `/Game/Botanicus/Materials/Juice/Noctiflore/M_NoctifloreSpore.M_NoctifloreSpore`
- [NS_NoctifloreSpores](C:/Users/quent/Documents/repos/Botanicus/Botanicus/Content/Botanicus/VFX/Noctiflore/NS_NoctifloreSpores.uasset) — `/Game/Botanicus/VFX/Noctiflore/NS_NoctifloreSpores.NS_NoctifloreSpores`

[Correspondances originales → variantes](C:/Users/quent/Documents/repos/Botanicus/Docs/VisualJuice3C-assets.json). Aucun StaticMesh, Blueprint, niveau ou asset audio créé ou modifié.

Fichiers préexistants modifiés par 3C :

- [BotanicusJuiceAssetTools.cpp](C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/Editor/BotanicusJuiceAssetTools.cpp)
- [BotanicusJuiceAssetTools.h](C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/Editor/BotanicusJuiceAssetTools.h)
- [BotanicusAmbienceDirector.cpp](C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/Environment/BotanicusAmbienceDirector.cpp)
- [BotanicusAmbienceDirector.h](C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/Environment/BotanicusAmbienceDirector.h)
- [BotanicusIllegalPlanterActor.cpp](C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/IllegalTrade/BotanicusIllegalPlanterActor.cpp)
- [BotanicusJuiceWorldSubsystem.cpp](C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/Visuals/BotanicusJuiceWorldSubsystem.cpp)
- [BotanicusJuiceWorldSubsystem.h](C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/Visuals/BotanicusJuiceWorldSubsystem.h)
- [DefaultGame.ini](C:/Users/quent/Documents/repos/Botanicus/Botanicus/Config/DefaultGame.ini)

Nouveaux C++ :

- [BotanicusNoctifloreSettings.h](C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/Visuals/BotanicusNoctifloreSettings.h)
- [BotanicusNoctifloreSettings.cpp](C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/Visuals/BotanicusNoctifloreSettings.cpp)
- [BotanicusNoctifloreJuice.cpp](C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/Visuals/BotanicusNoctifloreJuice.cpp)
- [BotanicusNoctifloreValidation.h](C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/Editor/BotanicusNoctifloreValidation.h)
- [BotanicusNoctifloreValidation.cpp](C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/Editor/BotanicusNoctifloreValidation.cpp)

Les outils reproductibles sont dans [Tools/VisualJuice3C](C:/Users/quent/Documents/repos/Botanicus/Tools/VisualJuice3C).

SHA-256 : **1339 fichiers préexistants** comparés au début de 3C, 1331 inchangés, 8 sources/config modifiés, aucun manquant. **Aucun ancien `.uasset`, `.umap` ou `.sav` modifié.** Les fichiers du chantier équipements déjà présents avant 3C sont inchangés depuis cette référence. `git diff --check` passé. [Preuve](C:/Users/quent/.codex/visualizations/2026/11/05/01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb/noctiflore/preservation.json).

## Captures et vidéo

- [22 h — 3C OFF](C:/Users/quent/Documents/repos/Botanicus/Docs/VisualJuice3C_Captures/comparison_before.png)
- [22 h — 3C ON](C:/Users/quent/Documents/repos/Botanicus/Docs/VisualJuice3C_Captures/scene_22h.png)
- [02 h — 3C ON](C:/Users/quent/Documents/repos/Botanicus/Docs/VisualJuice3C_Captures/scene_02h.png)
- [12 h — 3C ON](C:/Users/quent/Documents/repos/Botanicus/Docs/VisualJuice3C_Captures/scene_12h.png)
- [22 h — qualité basse](C:/Users/quent/Documents/repos/Botanicus/Docs/VisualJuice3C_Captures/scene_low.png)
- [22 h — GI/réflexions qualité 0, lueur conservée](C:/Users/quent/Documents/repos/Botanicus/Docs/VisualJuice3C_Captures/scene_no_lumen.png)
- [Courte vidéo avant/après](C:/Users/quent/Documents/repos/Botanicus/Docs/VisualJuice3C_Captures/noctiflores_before_after.mp4)

Comparaison à 22 h, même cadrage : les deux premières secondes sont OFF, puis la séquence ON. Les durées des images sont reconstruites depuis le temps du monde enregistré. Aucune interpolation de mouvement, retouche colorimétrique ou piste audio ajoutée. Les captures sont des lectures du viewport Unreal, sans génération d’images.

## Audio manquant et limites

Les deux fonds et le renfort clandestin existants fonctionnent avec la nouvelle couche sans redémarrage des boucles. Au centre du témoin, le gain d’insectes nocturnes est voisin de **0,052** ; un observateur au bord reçoit environ **0,040**, conformément au fondu régional. Aucun bruitage n’est ajouté à la pulsation ou aux spores.

Il manque toujours une signature dédiée aux Noctiflores : proposer une **boucle organique douce de 30–45 s**, raccord sans clic, légère texture aérienne végétale avec timbre extraterrestre, modulation très lente, sans pulsation de cœur, cri ni nappe horrifique. Fourniture souhaitée : WAV 48 kHz/24 bits, niveau confortable et marge de crête. Gain initial à auditionner autour de 0,015, sous les insectes actuels, dans la région clandestine seulement, nuit saisonnière et commerce actif, avec le fondu régional existant. Pas de son par plante ni événement répété par spore. Ressource à fournir par l’utilisateur ; aucune importation ou modification audio réalisée dans 3C.

Pas de cook complet, build Shipping, serveur dédié exécuté, session Steam multi-machines, latence/pertes simulées, ni profil GPU par passe Unreal Insights. Le chemin dédié est inspecté et compilé. La pulsation lente est locale ; sa synchronisation à la milliseconde entre clients n’est pas un objectif. Le masque de lueur n’isole pas artistiquement chaque nervure. Les stades conservent leur changement de silhouette existant ; aucune interpolation ou nouvelle géométrie n’est ajoutée. Le bilan visuel devra être revu avec l’aménagement définitif et sur une machine moins puissante.

## Reproduction

Après compilation Editor, utiliser [RunValidation.ps1](C:/Users/quent/Documents/repos/Botanicus/Tools/VisualJuice3C/RunValidation.ps1) : `BuildNoctiflores`, `ValidateNoctiflores`, `SmokeVisibility`, `RegressionEquipment`, `CaptureNoctiflores` ou `ProfileNoctiflores`. Le générateur n’enregistre que ses nouveaux packages et réutilise ceux déjà présents ; il ne remplace pas silencieusement une variante artistique existante. Les correspondances de catalogue sont complétées sans remplacer les choix déjà enregistrés. Le système de spores possède aussi ses valeurs sobres dans Niagara Editor ; une remise à zéro explicite de ces valeurs de prévisualisation est possible via la factory, sans changer les réglages Project Settings utilisés en jeu.

Les résultats frais sont sous `C:\Users\quent\Documents\repos\Botanicus\Botanicus\Saved\Diagnostics\VisualJuice3C`. Copier `SummarizeNoctiflores.py` à côté du JSON de profil et le lancer avec Python pour le tableau. L’encodage optionnel utilise `EncodeComparison.py --data-dir <dossier des résultats> --output-dir <destination>` avec Pillow et l’encodeur local imageio-ffmpeg ; aucune dépendance n’est téléchargée par ces outils. Le runner vérifie la fraîcheur et le statut du JSON, pas seulement le code de sortie Unreal.
