# Botanicus — Chantier 3A : plantation et arrosage

5 novembre 2026. Unreal Engine **5.8.2**, Win64 Development Editor. Dépôt : `C:\Users\quent\Documents\repos\Botanicus`. Projet : `C:\Users\quent\Documents\repos\Botanicus\Botanicus\Botanicus.uproject`. État initial propre, HEAD `475093e0b1658237a72bd913e70228bfc484f498` (« ajout des sons »).

## Résultat

Une première couche visuelle fonctionnelle ajoute une brève gerbe de terre à la plantation validée, un flux continu de gouttelettes Niagara, des impacts discrets et un lissage de l’humidité affichée. Les trois variantes de pots utilisent les mêmes systèmes. Les effets suivent les réussites natives ; ils ne valident jamais l’action à la place du serveur.

**Deux nouveaux assets Niagara CPU à sprites**, aucun mesh ajouté ou remplacé dans les assets ou les blockouts. Les acteurs et leurs collisions physiques conservent leurs dimensions. Un arrosoir déjà présent dans le projet est réutilisé temporairement comme représentation cosmétique distante lorsque la hotbar ne fournit pas de représentation répliquée. Ce réemploi local est sans collision, sans réplication et exclu de l’autosauvegarde par le marquage d’aperçu déjà reconnu par le jeu ; il n’ajoute aucun nouveau modèle.

Les fichiers audio, le catalogue sonore, BotanicusAmbienceDirector, les niveaux, les matériaux existants et la sauvegarde originale sont strictement inchangés. Aucun commit ni téléchargement. Le verrou LFS de `Lvl_BuildingTest.umap`, Reyden7, ID 61708748, est conservé.

## Diagnostic de l’ancien arrosage

Les références signalées sont réellement cassées dans le projet courant : `/Script/Botanicus.BotanicusWaterSourceComponent` et `/Script/Botanicus.BotanicusWaterVFXComponent` ne se chargent pas ; `/Game/Botanicus/VFX/Watering/NS_Arrosage_Fluids` n’existe pas dans l’Asset Registry. Le Blueprint `/Game/Botanicus/blueprints/BP_Item_WateringCan` conserve des références sérialisées aux classes absentes et un composant Niagara nommé `NS_Arrosage_Fluids`, dont le système est manquant.

Historique vérifié dans Git :

- `20a59cb` (« water effect ») utilisait une source d’eau avec profil et point de sortie, un sous-système de trajectoire, des jets/impacts Niagara et une représentation de flux par segments de mesh.
- `86ae5ec` (« reset visuel eau »), puis les reverts `e6a5a83` / `252ce70`, ont retiré puis réintroduit des parties de ce travail.
- `f250741` (« trafic illégale de stupéfiant ») a finalement supprimé l’ancienne implémentation Niagara de l’arrosoir, son Tick et le dossier de source d’eau. `SetWateringEffectActive` a été conservé comme point d’entrée vide, avec un commentaire explicite de pause du visuel.

Cela établit une **désactivation volontaire**, plutôt qu’un simple défaut de chargement de Niagara. Le motif artistique ou de performance exact n’est pas consigné dans ces messages de commit. `WaterSourceComponent` était une définition réutilisable de source/nozzle et d’activation, liée à l’ancien système de trajectoire ; le rétablir aurait réintroduit une architecture devenue étrangère à la consommation actuelle. Le code de `WaterVFXComponent` et l’asset `NS_Arrosage_Fluids` ne sont pas retrouvés dans l’historique accessible : leur contenu interne ne peut pas être affirmé sur la seule base de leur nom.

Le système encore présent `/Game/Botanicus/VFX/Watering/NS_AnimeWater` se charge, mais son émetteur `WaterCylinder01_0` utilise **NiagaraMeshRendererProperties** et `/Game/Botanicus/VFX/Watering/curve01fbx`. Il n’est donc pas un flux léger de gouttelettes paramétrables ; il n’est pas réactivé. Les nouveaux systèmes reposent sur le template moteur `SimpleSpriteBurst` du Niagara installé, avec modules standards de naissance, position, vitesse, gravité, résolution et fondu. Aucun Niagara Fluids, grille, collision de particules ni renderer mesh.

Les anciens composants Niagara du Blueprint sont désactivés et masqués au BeginPlay de l’arrosoir. Les classes absentes ne sont pas recréées. **Les références sérialisées orphelines restent dans le Blueprint et peuvent encore produire des avertissements au chargement** ; un nettoyage ciblé dans Unreal Editor reste à faire après inspection de ses données historiques. Cette passe ne réécrit pas ce Blueprint pour effacer des données dont le rôle complet ne peut être vérifié.

## Gameplay inspecté et conservé

La plantation simple et multiple consomme d’abord la graine dans `BeginPrimaryUse`, puis affecte la plante au pot/slot. Le nouveau signal est ajouté à ce seul point de réussite. La jardinière clandestine utilise son chemin `Interact_Implementation` après consommation réussie. Les restaurations de sauvegarde et les OnRep ne jouent pas de nouvelle plantation.

L’eau provient de `UBotanicusQuickBarComponent::ConsumeSelectedWateringCanWater`. Les pots simples/multiples alimentent leur `WaterLevel` dans leur action primaire autoritaire ; les jardinières clandestines alimentent `WaterAmount` par slot. Les conditions de visée, portée, outil, remplissage et quantité restent celles du jeu. La pulvérisation libre conserve `ServerPulseWateringCanSpray`, déjà limité côté serveur. Aucun calcul de collision Niagara ne fournit d’eau aux plantes.

Le rendu utilise une trace locale de visibilité pour placer l’impact au pot/sol regardé. Cette trace est uniquement cosmétique et n’écrit jamais dans le pot, l’inventaire ou le climat. La trajectoire des gouttelettes est calculée à partir du bec, du point visuel et d’un temps de vol borné ; Niagara intègre ensuite les particules. La visée distante repose sur la rotation de visée déjà répliquée du Pawn. La précision exacte avec forte latence n’est pas validée ici.

## Réseau et cycle de vie

`UBotanicusJuiceWorldSubsystem` possède localement les systèmes d’effets. Il ne ticke que lorsqu’un flux, une gerbe ou une transition de matériau existe ; un timer de 0,1 s rapproche la liste des sources actives du rendu local.

**Arrosage : aucune nouvelle réplication.** La liste existante `ABotanicusAudioRelay::GetWaterStates()` est lue sans modifier le système audio. Elle contient les Pawns dont l’apport d’eau a été accepté. Le propriétaire utilise son arrosoir local existant ; les autres vues utilisent l’arrosoir physique déjà porté, ou une représentation locale du modèle existant, uniquement pendant l’action.

**Plantation : un petit relais dédié** `ABotanicusJuiceRelay` diffuse un événement cosmétique non fiable par succès, avec séquence, pot et position. L’hôte ne joue que depuis l’implémentation multicast, sans second appel local. Les séquences déjà reçues sont ignorées. Aucune particule n’est répliquée. Comme pour d’autres événements cosmétiques non fiables, une perte de paquet peut supprimer une gerbe sans changer le gameplay.

Le maintien déplace et paramètre le même composant Niagara ; il ne réinitialise pas le système à chaque apport d’eau. L’arrêt coupe l’émission et laisse mourir les gouttelettes déjà émises pendant leur durée de vie, puis détruit le composant. Une reprise rapide peut réutiliser ce composant pendant sa vidange. La disparition du Pawn ou de l’arrosoir local provoque une suppression immédiate ; les gerbes surveillent aussi leur source et leur surface. La destruction du pot retire ses impacts, et l’état d’eau accepté expire selon les règles déjà présentes de l’audio si aucun apport ne continue.

Le serveur dédié ne charge pas les nouveaux systèmes, ne crée pas de composant Niagara, ne lisse pas les matériaux et ne reconstruit pas la terre visuelle. Il ne gère que le relais nécessaire aux événements. Ce chemin est protégé dans le code ; il n’a pas été exécuté dans un processus de serveur dédié lors de cette validation.

## Terre humide

Les pots possédaient déjà des instances dynamiques de `/Game/Botanicus/Materials/Soil/M_BotanicusSoilWetPatchesV2`, avec `Wetness`, `Dry Tint`, `Wet Tint`, rugosité et spéculaire sec/humide. Cette structure a été conservée : aucun matériau global n’a été modifié.

Le simple pot et la jardinière clandestine alimentent `ABotanicusPotSoilVisualActor`. Le pot multiple conserve son MID `MultiSoilDynamicMaterial`. La cible visuelle demeure la valeur de jeu existante, transformée par l’exposant déjà utilisé (0,5 par défaut), puis interpolée localement. Le retour vers le sec suit donc la diminution du `WaterLevel`/`WaterAmount` répliqué ; il n’existe aucune seconde simulation d’humidité.

Chaque MID est indépendant. Sur une même surface de jardinière, l’agrégation moyenne déjà utilisée par le jeu est conservée ; cette passe ne crée pas des taches d’humidité indépendantes par slot. La forme de terre existante n’est plus reconstruite pour un simple changement d’humidité : seuls les changements de forme/remplissage nécessitent la reconstruction de sa géométrie procédurale existante.

## Assets et paramètres artistiques

| Asset | Chemin réel |
|---|---|
| Gouttelettes continues | `C:\Users\quent\Documents\repos\Botanicus\Botanicus\Content\Botanicus\VFX\Juice\NS_BotanicusWaterDrops.uasset` |
| Gerbe courte réutilisée | `C:\Users\quent\Documents\repos\Botanicus\Botanicus\Content\Botanicus\VFX\Juice\NS_BotanicusJuiceBurst.uasset` |

Chemins Unreal : `/Game/Botanicus/VFX/Juice/NS_BotanicusWaterDrops` et `/Game/Botanicus/VFX/Juice/NS_BotanicusJuiceBurst`. Le dossier est ajouté à `DirectoriesToAlwaysCook` dans DefaultGame.ini. Les assets ont été créés, compilés et enregistrés avec les API Unreal/Niagara ; aucun script n’a édité leurs octets. Les graphes sont modifiables dans Niagara Editor et exposent `User.SpawnRate`, `BurstCount`, `Lifetime`, `Size`, `Tint`, `Velocity`, `Gravity`, `Radius`.

Réglages : **Project Settings → Botanicus Planting & Water Juice**, classe `UBotanicusJuiceSettings`.

| Réglage | Défaut | Usage |
|---|---:|---|
| Enabled / WaterDrops / Splashes / PlantingBurst | Activés | Désactiver la couche ou chaque famille |
| RemoteHeldCan | Activé | Réemploi du modèle d’arrosoir pour les vues distantes ; désactivable |
| DropsPerSecond | 65 | Débit de naissance Niagara, borné à 160/s |
| DropletSize | 1,4 cm | Taille des sprites, bornée entre 0,2 et 4 cm |
| DropletSpeed | 550 cm/s | Détermine un temps de vol borné entre 0,08 et 0,55 s |
| Gravity | 580 cm/s² | Courbure visuelle, indépendante de l’eau simulée |
| NozzleMeshLocalOffset | (−44, 0, 14) cm | Calage du bec sur le mesh existant ; un Water Nozzle explicitement placé prévaut |
| SplashInterval / Lifetime / Count | 0,16 s / 0,28 s / 4 | Impacts discrets |
| PlantingIntensity / Count / Lifetime | 1 / 8 / 0,4 s | Petite gerbe de terre, sans déformation de collision |
| SoilTint / WaterTint | Brun terre / bleu clair | Couleur des particules |
| WetnessIntensity / InterpolationSpeed | 1 / 7 | Amplitude et vitesse de la réaction de terre |
| WaterDisplayDistance / PlantingDisplayDistance | 2500 / 1800 cm | Distances maximales du rendu local |
| MaximumBurstComponents | 12 | Budget simultané des gerbes |

Le calage du bec a été établi à partir de la géométrie existante, exportée temporairement pour inspection dans le dossier de preuves ; ce modèle n’a pas été réimporté ni modifié. Les réglages n’ajoutent aucun nouvel objet au blockout.

## Validation réalisée

Compilation Development Editor Win64 réussie : `C:\Users\quent\.codex\visualizations\2026\11\05\01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb\juice\build.log`. Dernier banc rendu réel : **247/247 contrôles réussis**, statut global **True**. Résultats détaillés : `C:\Users\quent\.codex\visualizations\2026\11\05\01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb\juice\pie_result.json` ; journal : `C:\Users\quent\.codex\visualizations\2026\11\05\01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb\juice\pie_verified.log`.

Après les deux protections finales (dimensions de surface conservées sans rendu sur serveur dédié et annulation d’une interpolation lorsque la couche est désactivée), le binaire a été recompilé et une régression rendue à quatre joueurs a passé **69/69 contrôles**. Elle vérifie notamment une désactivation pendant la transition humide, le retour sec et le cycle plantation/arrosage/nettoyage du pot simple. Preuves : `C:\Users\quent\.codex\visualizations\2026\11\05\01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb\juice\smoke\pie_result.json` et `C:\Users\quent\.codex\visualizations\2026\11\05\01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb\juice\smoke_final.log`. Le processus dédié reste non exécuté.

Le banc utilise le vrai GameMode du projet en PIE, un hôte listen et trois clients, avec quatre périphériques audio. Les sauvegardes sont copiées sous `C:\Users\quent\.codex\visualizations\2026\11\05\01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb\juice\isolated_user` et leur emplacement est vérifié avant de démarrer. Les ticks des plantes préexistantes sont suspendus uniquement dans cette copie de test pour isoler les compteurs. Les actions exécutent les méthodes natives sur les acteurs du jeu ; les parcours complets au clavier/souris ne sont pas certifiés par ce banc.

| Parcours | Contrôles exécutés |
|---|---|
| Pot simple Blueprint | Plantation valide et refusée, particules réellement simulées, début/maintien/fin d’eau, changement d’outil, destruction du pot sans appel explicite de fin préalable |
| Pot multiple de préparation | Plantation sur slot, refus sur slot occupé, eau et maintien, arrêt, MID humide répliqué, destruction |
| Jardinière clandestine | Plantation et refus, arrosage natif, maintien/arrêt, humidité, destruction, heure nocturne |
| Quatre joueurs | Un signal par plantation et par pair ; huit particules de gerbe sur chaque pair ; quatre sources d’eau simultanées et particules présentes |
| Cycle de vie | Composants d’eau et gerbes retirés après arrêt, aucun arrosoir cosmétique conservé à la fin, budget de gerbes respecté |
| Matériaux | Humidité humide reçue par les quatre MID du pot témoin ; retour sec vérifié sur le pot simple |
| Préservation | Deux lectures initiales d’ambiance audio sur chaque pair, sans restart provoqué par ces scénarios ; sources audio et éclairage inchangées |
| Sauvegarde | Toutes les représentations cosmétiques portent le tag d’aperçu exclu par le code existant de capture du monde ; sauvegarde originale inchangée par SHA-256 |

Le compteur de particules lit les instances Niagara après synchronisation de leur travail CPU, uniquement pour le diagnostic ; ce diagnostic n’est pas exécuté à chaque frame par le produit. Un maximum observé de **116 particules** dans la couche sur un pair est enregistré sur cette passe. Les composants de gerbe sont plafonnés à 12 et les sources d’eau suivent les quatre joueurs. Ce budget ne décrit pas les autres effets éventuels du jeu.

### Coût observé

Mesures `stat unit` dans la même session rendue à quatre viewports, d’abord avec la couche désactivée, puis activée, après stabilisation. Médianes en ms ; les périodes de capture sont exclues de ces deux groupes. Les colonnes sont les valeurs brutes fournies par le viewport hôte.

| État | Échantillons | Frame | Game thread | Render thread | GPU |
|---|---:|---:|---:|---:|---:|
| Effets désactivés | 19 | 21.430 | 20.670 | 0.000 | 13.132 |
| Effets activés | 19 | 24.607 | 25.965 | 0.000 | 14.200 |

Il s’agit d’un contrôle indicatif sur une machine, avec quatre mondes et viewports dans le même éditeur. Les temps couvrent la scène entière, y compris les représentations distantes existantes, et comportent du bruit. Les valeurs de render thread quasi nulles, si présentes, ne permettent pas d’isoler ce coût sur cette capture. Ces chiffres ne constituent pas un benchmark Shipping ni une mesure GPU isolée de Niagara. Pour du matériel plus faible, désactiver d’abord RemoteHeldCan ou Splashes, puis réduire DropsPerSecond et les distances.

## Captures Unreal

Captures réelles du viewport PIE en 1280 × 720, sans retouche ni image générée. Les vues simples avant/après gardent la même scène et le même réglage lumineux ; les états de plante et d’eau changent avec l’action testée. Les vues clandestines sont prises de nuit et ne doivent pas être comparées comme une modification d’éclairage.

| Sujet | Avant | Après |
|---|---|---|
| Plantation simple | [planting_before.png](<C:/Users/quent/Documents/repos/Botanicus/Docs/VisualJuice3A_Captures/planting_before.png>) | [planting_simple_after.png](<C:/Users/quent/Documents/repos/Botanicus/Docs/VisualJuice3A_Captures/planting_simple_after.png>) |
| Arrosage simple | [watering_before.png](<C:/Users/quent/Documents/repos/Botanicus/Docs/VisualJuice3A_Captures/watering_before.png>) | [watering_after.png](<C:/Users/quent/Documents/repos/Botanicus/Docs/VisualJuice3A_Captures/watering_after.png>) |

Autres variantes : [planting_multi_after.png](<C:/Users/quent/Documents/repos/Botanicus/Docs/VisualJuice3A_Captures/planting_multi_after.png>) et [planting_illegal_after.png](<C:/Users/quent/Documents/repos/Botanicus/Docs/VisualJuice3A_Captures/planting_illegal_after.png>). Le feedback de plantation est bref ; une capture fixe n’en décrit pas entièrement la lecture en mouvement.

## Limites et problèmes restant à résoudre

- Les références orphelines de BP_Item_WateringCan restent à nettoyer dans Unreal Editor après revue du Blueprint ; les anciens effets sont neutralisés et ne pilotent pas le nouveau système. Aucun composant historique inconnu n’a été recréé.
- Les vues distantes réemploient le modèle existant et une pose calquée sur la représentation de hotbar ; cette passe ne crée pas de nouvelle animation de prise en main, de bascule du poignet ou d’IK. Le calage doit être revu si le mesh ou sa pose change.
- La terre conserve son matériau et son agrégation par surface. Une réaction localisée à chaque emplacement dans une même jardinière n’est pas ajoutée.
- Pas de validation Steam multi-machines, paquet perdu/latence simulée, arrivée tardive, destruction du Pawn/déconnexion ou serveur dédié exécuté. Le code prévoit ces nettoyages, mais ces parcours ne sont pas déclarés testés.
- Pas de cook complet, build Shipping, GPU mobile/intégré, Unreal Insights ou GPU profiler isolé. `DirectoriesToAlwaysCook` prépare les références mais ne prouve pas un packaging final.
- Le banc n’a pas parcouru toute l’interface d’équipement à la souris. La synchronisation des données natives et le rendu PIE sont testés ; une passe manuelle en session coopérative reste utile pour juger la sensation et la visée en mouvement.

## Fichiers concernés et intégrité

- [BotanicusJuiceSettings.h](<C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/Visuals/BotanicusJuiceSettings.h>)
- [BotanicusJuiceWorldSubsystem.cpp](<C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/Visuals/BotanicusJuiceWorldSubsystem.cpp>)
- [BotanicusJuiceRelay.cpp](<C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/Visuals/BotanicusJuiceRelay.cpp>)
- [BotanicusPlantPotActor.cpp](<C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/Growing/BotanicusPlantPotActor.cpp>)
- [BotanicusMultiPlantPotActor.cpp](<C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/Growing/BotanicusMultiPlantPotActor.cpp>)
- [BotanicusIllegalPlanterActor.cpp](<C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/IllegalTrade/BotanicusIllegalPlanterActor.cpp>)
- [BotanicusPotSoilVisualActor.cpp](<C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/Visuals/BotanicusPotSoilVisualActor.cpp>)
- [BotanicusWateringCanActor.cpp](<C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/Growing/BotanicusWateringCanActor.cpp>)
- [BotanicusPlayerController.h](<C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/BotanicusPlayerController.h>)
- [BotanicusJuiceAssetTools.cpp](<C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/Editor/BotanicusJuiceAssetTools.cpp>)
- [BotanicusJuiceValidation.cpp](<C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/Editor/BotanicusJuiceValidation.cpp>)

Les dépendances Niagara (runtime) et NiagaraEditor (éditeur uniquement) sont ajoutées dans Botanicus.Build.cs. Le PlayerController expose deux lectures de son état local existant ; ses règles d’interaction et ses RPC d’eau ne sont pas changés. Le helper de validation ne peut muter le jeu qu’en éditeur, dans un monde PIE, avec `-BotanicusJuiceValidation` et un SavedDir contenant `/isolated_user/`. Il est inerte hors de ce contexte.

Intégrité finale : **1240 fichiers préexistants contrôlés**, 1229 inchangés, 11 sources/config modifiés pour le chantier. Tous les 1059 fichiers protégés (audio, éclairage concerné, matériaux/meshes/Blueprints existants, maps et sauvegardes suivis) sont inchangés. Preuve : `C:\Users\quent\.codex\visualizations\2026\11\05\01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb\juice\integrity.json`. Aucun fichier binaire existant du projet n’est modifié ; seuls les deux nouveaux systèmes Niagara sont enregistrés.
