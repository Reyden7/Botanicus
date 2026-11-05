# Botanicus — Chantier 3B : croissance vivante, maturité et récolte

Livraison du 5 novembre 2026. Projet vérifié : `C:\Users\quent\Documents\repos\Botanicus\Botanicus\Botanicus.uproject`. Moteur installé : **Unreal Engine 5.8.2**, changelist 56702186. État Git initial propre, HEAD `22aa81093abb7b5377bac81847806996935975e2`. Aucun commit créé.

## Résultat livré

Le feuillage des plantes du catalogue et des Noctiflores reçoit une oscillation WPO lente, enracinée et déphasée selon la position. Les seuils de croissance produisent une petite gerbe végétale ; la maturité produit six particules dorées, puis s’éteint. La récolte produit cinq particules après validation de l’ajout à l’inventaire. Aucun Niagara permanent sur les plantes matures.

Les meshes et matériaux d’origine restent intacts. Les variantes sont appliquées aux seuls composants de végétation à l’exécution. Aucun modèle, téléchargement, nouvelle horloge, nouveau relais réseau ou gestionnaire mondial n’a été ajouté. Les règles de croissance, d’environnement, de récolte, de quantité et d’économie sont conservées.

## Inspection préalable et certitude des constats

Les documents précédents ont été lus : [VisualJuice3A](C:/Users/quent/Documents/repos/Botanicus/Docs/VisualJuice3A.md), [SoundDesign](C:/Users/quent/Documents/repos/Botanicus/Docs/SoundDesign.md), [DayNightLighting](C:/Users/quent/Documents/repos/Botanicus/Docs/DayNightLighting.md).

Constats établis dans le code et les assets chargés par Unreal :

- **Pots simples** : quatre meshes, sélectionnés avant 30 %, à 30 %, à 70 % et à maturité. Composant `Foliage`. La hauteur reste interpolée à partir de la progression globale, indépendamment du changement de mesh.
- **Pots multiples** : même sélection, par emplacement, sur `MultiFlower_0…3`. Les emplacements et la normalisation de hauteur existants sont conservés.
- **Jardinières clandestines** : trois meshes seulement, sur `IllegalPlantVisual_00…03`. Le dernier apparaît à 70 %. La maturité de gameplay arrive ensuite à 100 % : aucun quatrième mesh n’a été inventé.
- Le catalogue contient 21 définitions, avec 31 meshes uniques de croissance et 31 matériaux. Les trois meshes clandestins ajoutent trois matériaux. Les 34 matériaux examinés sont opaques, sans entrée WPO existante ni Material Attributes : ils permettent une extension isolée.
- L’audio possède déjà les événements `GrowthYoung`, `GrowthLarge`, `Mature`, `Harvest`. Le nouveau code consulte uniquement la fonction pure de sélection de seuil `GrowthEvent`. Il n’émet aucun son supplémentaire.
- La maturité utilise la tolérance **`1 - KINDA_SMALL_NUMBER`**, déjà présente dans l’audio et la sélection des meshes. Aucun seuil de gameplay n’a été déplacé.

Les anciennes références manquantes du Blueprint d’arrosoir n’ont pas été nettoyées. Leur suppression reste un chantier Editor séparé : examiner les références et le Blueprint, sauvegarder et compiler explicitement après revue. Le comportement de neutralisation hérité de 3A reste en place.

## Architecture et fonctionnement

### Mouvement organique

[BotanicusJuiceWorldSubsystem](C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/Visuals/BotanicusJuiceWorldSubsystem.cpp) applique des variantes via `BindFoliage`, depuis les rafraîchissements visuels existants. Un MID est créé à la première liaison ou au changement de matériau de stade ; il est conservé pendant le stade. Les plantes mortes arrêtent leur mouvement.

Le shader ajoute deux sinus lents en XY, multipliés par un masque de hauteur au carré. La phase dépend de la position XY de la plante. L’amplitude est en **centimètres du monde**, et reste indépendante de l’échelle du mesh. Aucun déplacement de composant, de pot, de terre, de collision ou de point d’interaction n’est utilisé. Aucun nouveau Tick par plante.

`MPC_PlantMotion` fournit l’amplitude, la vitesse et la distance à tous les matériaux d’un monde. Le timer de réconciliation existant du sous-système vérifie les paramètres toutes les 0,1 s et n’écrit la collection que s’ils changent. Le shader réduit progressivement le mouvement entre 70 % et 100 % de la distance maximale. Le chemin de qualité matérielle Low retourne zéro ; l’option de qualité d’effets basse coupe aussi l’amplitude de la collection.

Les variantes sont des copies Unreal des matériaux ou instances existants. Textures, paramètres et branchements de surface restent hérités. Un catalogue explicite associe chaque original à sa variante. Un futur matériau absent du catalogue conserve son rendu initial ; le générateur refuse les bases déjà dotées de WPO ou de Material Attributes et les chaînes d’instances non prises en charge, plutôt que de les réécrire.

### Transitions et récolte

Les appels sont placés dans la simulation autoritaire, immédiatement après les appels audio existants. Le calcul choisit **un seul événement**, le seuil le plus élevé franchi. Ni les OnRep, ni les méthodes de restauration, ni `BindFoliage` n’émettent de transition.

Le [JuiceRelay existant](C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/Visuals/BotanicusJuiceRelay.cpp) reçoit un multicast cosmétique supplémentaire. Numéro de séquence, déduplication locale, filtrage par distance et budget commun évitent les doubles émissions côté hôte et les empilements sans limite. Les particules ne sont pas répliquées. Les RPC visuels sont **Unreliable** : un événement bref peut être perdu en réseau dégradé ; l’état mature répliqué reste la référence.

Les événements réutilisent `/Game/Botanicus/VFX/Juice/NS_BotanicusJuiceBurst`. Il n’y a aucun nouveau système Niagara. Les positions proviennent du composant végétal de l’emplacement concerné.

La récolte visuelle intervient uniquement dans la branche où `AddItem` réussit. La disparition immédiate de la plante et le moment d’attribution du produit restent ceux du jeu. Aucun mesh fantôme, délai d’inventaire ou modification de quantité. Le serveur dédié relaie l’événement sans charger le catalogue de rendu ni simuler de Niagara.

## Paramètres artistiques

Classe [UBotanicusJuiceSettings](C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/Visuals/BotanicusJuiceSettings.h), libellé de recherche dans Project Settings : **Botanicus Plant Juice**. Configuration `Game`.

| Paramètre | Défaut | Rôle |
|---|---:|---|
| `bEnabled` | vrai | Activation globale, commune avec 3A |
| `bOrganicMotion` / `bLifecycleBursts` | vrai / vrai | Désactivation séparée du mouvement et des gerbes 3B |
| `MotionAmplitude` | 0,25 cm | Amplitude maximale, réglable de 0 à 0,75 cm |
| `MotionSpeed` | 0,8 rad/s | Période principale d’environ 7,85 s |
| `bDisableMotionOnLowEffectsQuality` | vrai | Coupe l’amplitude si EffectsQuality = 0 |
| `TransitionIntensity` | 1 | Multiplie les quantités et la taille des gerbes ; zéro les coupe |
| `YoungCount` / `DevelopedCount` | 3 / 4 | Particules à 30 % / 70 % |
| `MatureCount` / `HarvestCount` | 6 / 5 | Particules à maturité / récolte |
| `GrowthTint` | (0,25 ; 0,65 ; 0,35 ; 0,8) | Teinte végétale RGBA linéaire |
| `MatureTint` | (0,85 ; 0,70 ; 0,25 ; 0,8) | Teinte dorée |
| `HarvestTint` | (0,45 ; 0,75 ; 0,50 ; 0,8) | Teinte de récolte |
| `TransitionLifetime` / `MatureLifetime` / `HarvestLifetime` | 0,45 / 0,65 / 0,40 s | Durées brèves |
| `GrowthDisplayDistance` | 1 800 cm | Limite des gerbes et extinction du mouvement |
| `MaximumBurstComponents` | 12 | Budget **partagé** avec plantation et éclaboussures, par monde rendu |

Les quantités sont bornées à l’exécution à 16 par événement. Le déplacement WPO des matériaux est borné à 1 cm. Les couleurs finales dépendent de l’éclairage, de l’exposition et du matériau sprite déjà utilisé par 3A.

## Fichiers réellement modifiés et ajoutés

Fichiers préexistants modifiés :

- [BotanicusJuiceValidation.cpp](C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/Editor/BotanicusJuiceValidation.cpp)
- [BotanicusJuiceValidation.h](C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/Editor/BotanicusJuiceValidation.h)
- [BotanicusMultiPlantPotActor.cpp](C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/Growing/BotanicusMultiPlantPotActor.cpp)
- [BotanicusPlantPotActor.cpp](C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/Growing/BotanicusPlantPotActor.cpp)
- [BotanicusIllegalPlanterActor.cpp](C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/IllegalTrade/BotanicusIllegalPlanterActor.cpp)
- [BotanicusJuiceRelay.cpp](C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/Visuals/BotanicusJuiceRelay.cpp)
- [BotanicusJuiceRelay.h](C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/Visuals/BotanicusJuiceRelay.h)
- [BotanicusJuiceSettings.cpp](C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/Visuals/BotanicusJuiceSettings.cpp)
- [BotanicusJuiceSettings.h](C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/Visuals/BotanicusJuiceSettings.h)
- [BotanicusJuiceWorldSubsystem.cpp](C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/Visuals/BotanicusJuiceWorldSubsystem.cpp)
- [BotanicusJuiceWorldSubsystem.h](C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/Visuals/BotanicusJuiceWorldSubsystem.h)
- [DefaultGame.ini](C:/Users/quent/Documents/repos/Botanicus/Botanicus/Config/DefaultGame.ini)

La seule modification de configuration est l’ajout du dossier des nouveaux matériaux à `DirectoriesToAlwaysCook`. Les réglages audio et éclairage ne changent pas.

**37 nouveaux assets** dans `C:\Users\quent\Documents\repos\Botanicus\Botanicus\Content\Botanicus\Materials\Juice\Growth` : 21 `M_Living_*`, 14 `MI_Living_*`, `MPC_PlantMotion` et `DA_PlantMotion`. Le [manifeste des correspondances](C:/Users/quent/Documents/repos/Botanicus/Docs/VisualJuice3B-materials.json) donne les 34 chemins originaux et leurs variantes, sans noms approximatifs. Aucun asset original n’a été remplacé.

Outils ajoutés dans `C:\Users\quent\Documents\repos\Botanicus\Tools\VisualJuice3B` : `BuildPlantMotion.py`, `ValidateGrowth.py`, `ValidateReturn.py`, `ProfilePlantJuice.py`, `Regression3A.py`, `CaptureMotion.py`, `RunValidation.ps1`.

Les fonctions natives de diagnostic sont protégées par PIE + `-BotanicusJuiceValidation` + un répertoire Saved contenant `/isolated_user/`. Elles ne constituent pas des chemins de gameplay utilisables dans une partie normale.

## Validation réellement exécutée

| Vérification | Résultat |
|---|---|
| Compilation Editor Win64 Development, UE 5.8.2 | Réussie |
| Compilation jeu Win64 Development | Réussie |
| Matériaux créés et sauvegardés par API Unreal | 34 associations, aucun matériau incompatible dans cet inventaire |
| Pots simples, multiples, clandestins : simulation native vers 30 %, 70 %, maturité | Réussie, une réception sur chacun des quatre pairs |
| Récolte réussie / inventaire plein | Réussie sur les trois types ; aucun effet lors du refus |
| Restauration et OnRep | Aucun rejeu, sur les quatre mondes PIE |
| Transition rapide | Simulation clandestine franchissant plusieurs seuils : un seul effet de maturité ; cas simple et multiple détaillés ci-dessous |
| Déduplication et nettoyage des composants | Réussis ; retour à zéro après expiration / destruction |
| Vraie arrivée tardive PIE, troisième client ajouté après maturité | État mature reçu, aucun rejeu côté nouveau client ou anciens pairs |
| Sauvegarde, fin de PIE, reprise avec quatre joueurs | Jardinière retrouvée à sa position, maturité conservée, zéro transition artificielle |
| Régressions 3A : plantation, refus, arrosage, changement d’outil, destruction, humidité, quatre arrosages | **247 assertions réussies** |
| Matrice 3B | **153 assertions réussies** |
| Arrivée tardive et sauvegarde | **10 assertions réussies** |
| Mouvement activé / désactivé / EffectsQuality 0 | Collections des quatre mondes : 0,25 / 0 / 0 cm |
| Transformations et modes de collision pendant le mouvement | Identiques avant / après sur les composants du pot simple de contrôle |

La simulation rapide de 300 s des pots simples et multiples assèche leur réserve et ne produit pas de progression dans cette fixture : absence d’effet vérifiée, **pas une preuve d’un franchissement multiple réel** pour ces deux types. Le choix du seuil le plus élevé est partagé par les trois raccordements ; le franchissement multiple réel a été exercé sur la jardinière clandestine.

La matrice de croissance utilise les vrais Tick natifs depuis des états restaurés juste avant les seuils. Elle ne simule pas une partie entière par saisie manuelle. Les captures comptent les particules réellement simulées. Les tests réseau sont un hôte et trois clients rendus **dans un même processus PIE**, sans émulation de pertes ou de latence.

Les premiers essais du harnais ont nécessité des corrections d’accès à l’API Python, de préparation du seuil de maturité et de visée de l’emplacement planté du pot multiple. Les résultats annoncés sont ceux des exécutions finales. La capture haute résolution différée perdait des demandes rapprochées ; un outil de lecture directe du viewport fournit la séquence finale. Ces outils ne sont pas utilisés dans les mesures de performance.

Résultats agrégés : [VisualJuice3B-results.json](C:/Users/quent/Documents/repos/Botanicus/Docs/VisualJuice3B-results.json). Journaux et données brutes : [matrice 3B](C:/Users/quent/.codex/visualizations/2026/11/05/01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb/growth/validation_growth.json), [arrivée tardive / reprise](C:/Users/quent/.codex/visualizations/2026/11/05/01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb/growth/validation_return.json), [régressions 3A](C:/Users/quent/.codex/visualizations/2026/11/05/01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb/growth/pie_result.json), [contrôles du mouvement](C:/Users/quent/.codex/visualizations/2026/11/05/01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb/growth/capture_motion.json).

## Performances

Mesures en millisecondes. Pour chaque comparaison : **OFF / ON / ON / OFF**, quatre fenêtres de 20 s, 5 s de stabilisation entre blocs, échauffement préalable, échantillonnage environ 4 Hz. Aucun screenshot pendant la mesure. Caméras fixes, même niveau `/Game/Botanicus/Maps/Lvl_BuildingTest`, même liste de joueurs et mêmes arrosages. Horloge GameState figée, simulation des plantes arrêtée sur le serveur ; les traitements visuels existants des clients restent présents. Flux PNJ supprimés dans la fixture isolée. Les arrosoirs sont remplis toutes les 2 s dans **les deux conditions**.

### Risque CPU hérité de 3A, mesuré avant l’implémentation 3B

| Bloc | Effets comparés | Frame médiane | GT médiane | GT p95 | GPU médiane | Échantillons |
|---|---|---:|---:|---:|---:|---:|
| 1 | OFF | 19.36 | 19.55 | 29.28 | 13.13 | 75 |
| 2 | ON | 20.57 | 20.77 | 28.85 | 13.33 | 74 |
| 3 | ON | 20.68 | 20.15 | 29.13 | 13.54 | 74 |
| 4 | OFF | 19.78 | 20.45 | 27.34 | 13.08 | 75 |

298 échantillons. Moyenne des deux médianes de blocs : GT **20,00 → 20,46 ms** (+0,46 ms), GPU **13,11 → 13,44 ms** (+0,33 ms). La hausse antérieure d’environ 5 ms n’est pas reproduite sur ce protocole ; cela n’établit pas que Niagara était sa seule cause.

Le code existant `HandleEquippedQuickBarChanged` rend l’équipement visuel sale à chaque notification d’inventaire, puis `UpdateEquippedQuickBarItem` le reconstruit. Le comptage échantillonné observe environ 300 nouveaux noms d’arrosoirs par bloc de 20 s, **effets activés comme désactivés**. C’est un indice concret de travail CPU commun, non une mesure exhaustive des allocations. Ce comportement n’a pas été modifié dans 3B.

### Coût du chantier 3B en régime stable

Même rig, **32 pots supplémentaires** répartis devant la caméra et quatre joueurs arrosant. Le chantier 3A est activé dans tous les blocs ; seuls mouvement et gerbes 3B sont basculés. Les 32 plantes de charge ne franchissent pas de stade pendant cette mesure.

| Bloc | Effets comparés | Frame médiane | GT médiane | GT p95 | GPU médiane | Échantillons |
|---|---|---:|---:|---:|---:|---:|
| 1 | OFF | 43.14 | 33.31 | 39.12 | 19.27 | 72 |
| 2 | ON | 43.32 | 33.18 | 40.97 | 19.25 | 72 |
| 3 | ON | 43.67 | 32.04 | 40.31 | 19.22 | 71 |
| 4 | OFF | 42.87 | 31.00 | 40.59 | 19.03 | 72 |

287 échantillons, quatre sources d’arrosage actives sur tous les pairs, maximum observé de 118 particules dans cette scène. Moyenne des médianes de blocs : frame **43,00 → 43,50 ms**, GT **32,16 → 32,61 ms** (+0,45 ms), GPU **19,15 → 19,24 ms** (+0,08 ms). Les distributions et p95 se recouvrent largement ; il existe une dérive entre blocs. Ces faibles écarts ne donnent pas une borne précise du coût WPO.

Les deux tableaux portent sur des scènes différentes et **ne doivent pas être soustraits l’un de l’autre**. Le second isole principalement le rendu du mouvement en régime stable. Il n’isole pas le coût de la liaison des MID pendant une croissance active ni une rafale maximale de transitions simultanées. Le temps Render Thread retourné par `stat unit` dans ce mode est presque nul et n’est pas exploité. Ces valeurs incluent l’Editor, Python, quatre mondes, les widgets et les sauvegardes de fixture : elles ne prédisent pas le framerate d’un build distribué sur quatre machines.

Données brutes : [profil préalable 3A](C:/Users/quent/.codex/visualizations/2026/11/05/01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb/growth/profile_3a.json), [profil 3B](C:/Users/quent/.codex/visualizations/2026/11/05/01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb/growth/profile_3b.json), [résumé des blocs](C:/Users/quent/.codex/visualizations/2026/11/05/01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb/growth/perf_summary.json).

## Captures et séquence

- [Passage à maturité, pot simple](C:/Users/quent/.codex/visualizations/2026/11/05/01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb/growth/captures/simple_grow1.0.png) ; [récolte, pot simple](C:/Users/quent/.codex/visualizations/2026/11/05/01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb/growth/captures/simple_harvest.png).
- [Passage à maturité, pot multiple](C:/Users/quent/.codex/visualizations/2026/11/05/01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb/growth/captures/multi_grow1.0.png) ; [récolte, pot multiple](C:/Users/quent/.codex/visualizations/2026/11/05/01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb/growth/captures/multi_harvest.png).
- [Mouvement désactivé](C:/Users/quent/.codex/visualizations/2026/11/05/01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb/growth/captures/organic_before.png) ; [séquence avant / après](C:/Users/quent/.codex/visualizations/2026/11/05/01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb/growth/organic_before_after.mp4) : deux secondes d’image immobile avant, puis mouvement par défaut, sans amplification artistique. Pas de son ajouté à la vidéo. Le mouvement de 0,25 cm reste volontairement discret.

Les captures de la jardinière clandestine utilisent une fixture trop proche pour cadrer entièrement le sommet. Elles servent de trace technique, sans constituer une présentation complète de la scène nocturne. Les paramètres d’éclairage n’ont pas été retouchés pour les captures.

## Reproduction et limites restantes

Après compilation Editor, lancer [RunValidation.ps1](C:/Users/quent/Documents/repos/Botanicus/Tools/VisualJuice3B/RunValidation.ps1) avec `-Scenario ValidateGrowth`, `ValidateReturn`, `Regression3A`, `ProfilePlantJuice` ou `CaptureMotion`. Le runner copie les sauvegardes vers `C:\Users\quent\Documents\repos\Botanicus\Botanicus\Saved\Diagnostics\VisualJuice3B\isolated_user`, impose un profil PIE listen server séparé et écrit ses résultats sous `Saved/Diagnostics/VisualJuice3B`. Il vérifie les résultats JSON en plus du code de sortie de l’Editor. Aucun effacement du dossier Saved d’origine.

Le générateur [BuildPlantMotion.py](C:/Users/quent/Documents/repos/Botanicus/Tools/VisualJuice3B/BuildPlantMotion.py) s’exécute dans l’Editor Python ou via le commandlet Python avec rendu autorisé. Il ne sauvegarde que ses nouveaux assets dans le dossier Growth. Les associations complètes figurent dans le manifeste livré.

**Vérifié par SHA-256** sur 1 252 fichiers préexistants : 1 240 inchangés, 12 sources/configurations modifiées, aucun fichier manquant, aucun ancien `.uasset`, `.umap` ou `.sav` modifié. Les ressources audio, cartes, Blueprints d’arrosoir et assets d’éclairage existants restent identiques. [Relevé de préservation](C:/Users/quent/.codex/visualizations/2026/11/05/01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb/growth/preservation.json).

Restent à valider hors de cette livraison : build packagé et cook complet, serveur dédié exécuté avec clients séparés, pertes réseau / latence, plusieurs configurations GPU, toutes les espèces vues manuellement à tous leurs stades et budget en rafale de transitions. Le chemin dédié est compilé dans la cible jeu et protégé dans le code ; il n’a pas été exercé comme serveur dédié autonome. Aucun test Shipping ni Unreal Insights détaillé n’est annoncé.
