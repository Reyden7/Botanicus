# Botanicus — Chantier 3B.1 : représentation des équipements

5 novembre 2026. Unreal Engine **5.8.2**, Win64 Development. Projet réel : `C:\Users\quent\Documents\repos\Botanicus\Botanicus\Botanicus.uproject`. HEAD initial `a3a71a9` (« ambiance »), état Git propre. Aucun commit créé.

## Résultat

L’aperçu tenu est réutilisé quand il représente encore le bon objet. Les notifications d’inventaire continuent d’être émises, répliquées et reçues. Un changement de réserve d’eau met à jour les données de l’acteur existant. Les changements de représentation restent immédiats au prochain Tick existant : **aucun cooldown**, aucun nouveau Tick ou timer de produit.

Sur le banc contrôlé, 100 secondes par version et quatre joueurs PIE : **3 840 créations et 3 840 destructions locales avant, zéro après**, avec 3 849 et 3 841 notifications reçues respectivement. Le GT passe de **21,22 à 19,64 ms** pour la moyenne des médianes de blocs (−1,58 ms, environ −7,5 %). Le p95 regroupé passe de **29,56 à 26,67 ms**. Ces gains sont ceux de ce banc instrumenté, pas une promesse de framerate en build distribué.

Documents préalables lus : [VisualJuice3B](C:/Users/quent/Documents/repos/Botanicus/Docs/VisualJuice3B.md), [VisualJuice3A](C:/Users/quent/Documents/repos/Botanicus/Docs/VisualJuice3A.md), [SoundDesign](C:/Users/quent/Documents/repos/Botanicus/Docs/SoundDesign.md).

## Cause confirmée

Dans [BotanicusQuickBarComponent.cpp](C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/QuickBar/BotanicusQuickBarComponent.cpp), les modifications de contenu et d’état diffusent `OnQuickBarChanged`. Cela inclut l’ajout/retrait, la consommation, les changements d’état porté, le chargement d’un snapshot et les permutations. **Chaque consommation et chaque recharge d’eau** diffusent également cet événement. `OnRep_Slots` le diffuse chez le propriétaire.

[BotanicusPlayerController.cpp](C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/BotanicusPlayerController.cpp) lie `HandleEquippedQuickBarChanged` à ce délégué. Avant correction, le handler marquait l’aperçu dirty ; dans `UpdateEquippedQuickBarItem`, **dirty suffisait à déclarer un changement de sélection**, indépendamment de la nature des données. Cette branche appelait réellement `DestroyEquippedQuickBarItem`, puis `SpawnActor` à la fin du même rafraîchissement.

Le slot, l’InstanceId et la quantité forçaient également cette branche. Pourtant, l’aperçu était initialisé avec une quantité **toujours égale à 1** ; la diminution d’une pile non vide ne change donc pas sa représentation. L’InstanceId n’est pas transmis à son initialisation visuelle.

Le relevé initial, effectué **avant l’optimisation**, confirme l’allocation et la destruction : Niagara coupé, **344 acteurs locaux créés et détruits en 25,047 s**, avec cinq composants par aperçu, soit **1 720 composants recensés sur les instances créées et autant sur les instances retirées**. Les callbacks natifs de destruction confirment les destructions d’acteurs. Ce comptage ne prétend pas dater leur libération mémoire par le GC. Les quelque 300 noms observés auparavant étaient un échantillonnage ; ils ne mesuraient pas toutes les allocations.

Le premier relevé chronométré était perturbé par un autre jeu : GPU à environ 94 %, 8,5 Go occupés sur 12 Go, et temps GPU de plusieurs centaines de ms. **Il n’est pas utilisé pour annoncer un gain de temps.** Après libération du GPU par l’utilisateur, le relevé préalable au comparatif indique 3 % de charge et 1 356 Mio occupés. [Diagnostic natif initial](C:/Users/quent/.codex/visualizations/2026/11/05/01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb/equipment/before_native.json) ; [état GPU avant le comparatif](C:/Users/quent/.codex/visualizations/2026/11/05/01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb/equipment/gpu_before_benchmark.txt).

## Décision de réutilisation

Les notifications demandent désormais une **vérification**, pas une reconstruction systématique. Le contrôleur conserve un snapshot local de l’état porté et des données de présentation : clé affichée, classe de catalogue, mesh de catalogue, mesh ItemData, échelle et mesh effectivement appliqué. Ce cache n’est ni sauvegardé ni répliqué.

| Événement | Conséquence après correction |
|---|---|
| Notification concernant un autre slot | Même acteur si sa représentation reste valide |
| Consommation/recharge du même arrosoir | `RestoreWaterLevel` sur l’aperçu existant ; aucun changement de l’eau d’inventaire par ce chemin |
| Passage à un autre arrosoir identique avec une autre réserve | Même acteur, nouvelles données de réserve ; identité de sélection actualisée |
| Quantité différente mais pile encore non vide | Même aperçu, toujours un exemplaire ; métadonnées de sélection actualisées |
| Nouvelle clé affichée, classe, mesh ItemData/catalogue ou échelle | Reconstruction conservée |
| Mesh d’aperçu remplacé, acteur/racine/composant invalide | Reconstruction conservée |
| Autre état porté que le seul réservoir | Reconstruction conservée, pour rejouer l’application complète existante de l’état |
| État simulé d’un pot d’aperçu ayant dérivé du snapshot d’inventaire | Reconstruction conservée lors de la prochaine notification, comme auparavant |
| OnRep explicite de transplantation portée | Reconstruction spécialisée conservée ; pas d’oubli du stade/mesh porté |
| Slot vide, entrée en placement/déplacement, nouveau Pawn/QuickBar, EndPlay | Retrait ou reconstruction nécessaires conservés |

La comparaison d’état passe par les propriétés réfléchies du struct, sans nouveau seuil temporel ni arrondi introduit. Le cas de l’eau est isolé en neutralisant seulement ses deux champs pour comparer le reste du snapshot. Les vieux états sans réserve sérialisée conservent le comportement existant : arrosoir considéré plein.

La pose de l’outil continue d’être calculée chaque frame par le code existant, même si aucun rafraîchissement d’inventaire n’est nécessaire. Le rayon, les offsets, les échelles d’inspection, la silhouette de placement et les chemins de transplantation restent ceux du projet.

Les aperçus complexes de pots possédaient déjà leur simulation native. Cette passe ne la modifie pas : un écart entre leur état courant et le snapshot empêche une réutilisation incorrecte. Les reconstructions légitimes ne sont donc pas supprimées au seul motif que l’ItemKey est identique.

## Multijoueur et systèmes préservés

La QuickBar reste owner-only (`Slots` et `SelectedSlotIndex`). Aucun nouveau RPC ni champ répliqué. Les aperçus locaux restent non répliqués, sans collision, exclus de la sauvegarde par les mécanismes existants.

Les arrosoirs physiques de `ABotanicusCharacter::HeldWateringCan`, leur `Carrier`, leur attachement à `hand_r` et les représentations distantes 3A restent inchangés. Le test vérifie les trois aperçus distants par monde pendant les quatre arrosages, ainsi que le port/dépôt d’un arrosoir physique répliqué dans les quatre mondes. La hotbar privée ne fournissait pas une réplication générique de tous les outils en troisième personne : cette passe n’en ajoute pas.

Les règles de consommation/recharge, l’inventaire, les conditions de prise/placement, les fichiers d’audio, les classes d’éclairage et les systèmes Niagara ne sont pas modifiés. Les anciennes références manquantes du Blueprint d’arrosoir restent intactes. Aucun nouveau modèle ou asset Unreal.

## Instrumentation et protocole

Le helper natif Editor collecte les événements du contrôleur et les callbacks `UWorld::AddOnActorSpawnedHandler` / `AddOnActorDestroyedHandler`. Les créations comptées sont les créations réussies de l’aperçu ; les demandes de destruction locales sont confrontées aux destructions réellement notifiées par le monde. Les autres arrosoirs sont distingués par leurs tags : locaux, aperçus distants 3A, physiques.

Un chronométrage de portée mesure aussi `UpdateEquippedQuickBarItem`, seulement lorsque le banc est activé. Pendant le benchmark, le recensement des composants est coupé afin d’éviter de faire payer à la branche historique les parcours supplémentaires de composants ; les compteurs d’acteurs restent actifs dans les deux versions.

Le banc peut sélectionner **la branche historique de décision** dans le même binaire Editor, ce qui évite de comparer deux sessions différentes. Elle saute les nouvelles vérifications de descripteur/état et conserve la même fabrique d’acteurs. Les cinq secondes de stabilisation après une bascule excluent les reconstructions de transition du comptage. Cette option est réservée au banc : monde PIE, drapeau `-BotanicusEquipmentValidation`, sauvegardes sous `/isolated_user/`, observation explicitement enregistrée. **Aucune branche historique ni instrumentation de frame n’est compilée dans la cible jeu.** Les UFUNCTION de diagnostic y sont inertes.

Huit fenêtres de **25 s**, ordre **historique / optimisé / optimisé / historique**, puis **optimisé / historique / historique / optimisé**. Échauffement préalable, 5 s de stabilisation par bloc, environ 10 relevés/s. Même niveau, caméras fixes, quatre sources d’eau, Niagara 3A/3B activé dans tous les blocs, refills de fixture toutes les 2 s dans les deux versions. Horloge climatique et simulation des plantes préexistantes suspendues dans la fixture ; flux PNJ identifiés supprimés. Aucun screenshot ni énumération d’acteurs pendant les fenêtres mesurées. Les autres tâches de compilation et de test sont arrêtées pendant cette mesure.

| Bloc | Décision | Mesures | Créations locales | Destructions locales | GT médiane ms | GT p95 ms | Temps moyen fonction µs |
|---|---|---:|---:|---:|---:|---:|---:|
| 1 | Historique | 220 | 968 | 968 | 21.20 | 29.32 | 256.79 |
| 2 | Optimisée | 219 | 0 | 0 | 20.19 | 27.79 | 6.92 |
| 3 | Optimisée | 220 | 0 | 0 | 19.23 | 25.44 | 7.15 |
| 4 | Historique | 220 | 948 | 948 | 20.73 | 31.29 | 253.71 |
| 5 | Optimisée | 223 | 0 | 0 | 19.71 | 27.30 | 7.27 |
| 6 | Historique | 220 | 956 | 956 | 22.18 | 29.20 | 252.27 |
| 7 | Historique | 219 | 968 | 968 | 20.78 | 29.35 | 255.12 |
| 8 | Optimisée | 222 | 0 | 0 | 19.41 | 25.90 | 6.90 |

**1 763 relevés**, dont 879 historiques et 884 optimisés. Les quatre blocs optimisés ont chacun une médiane GT inférieure aux quatre blocs historiques. Les notifications restent de même ordre (3 849 / 3 841, variation de cadence de moins de 0,3 %) ; leur suppression n’explique pas le résultat.

Le temps moyen mesuré dans la fonction descend de **254,47 à 7,06 µs/appel**. C’est la durée de la portée complète, appels inclus, et non un self-time exclusif. Elle comprend la pose et l’instrumentation de portée et ne représente pas le coût complet différé de la création/destruction des composants dans le reste du frame. Le temps GPU observé passe de 15,60 à 14,66 ms ; sans capture GPU détaillée, sa ventilation n’est pas attribuée à un module précis.

La frame médiane moyenne change peu : **20,95 → 20,79 ms**. On ne déduit donc pas une hausse générale des FPS de la seule baisse du GT. Le Render Thread quasi nul retourné par ce mode `stat unit` n’est pas interprété. Mesures PIE dans un processus avec quatre mondes : ni Unreal Insights complet, ni build Shipping, ni quatre machines.

[Données brutes du benchmark](C:/Users/quent/.codex/visualizations/2026/11/05/01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb/equipment/profile_equipment.json) ; [résumé chiffré](C:/Users/quent/.codex/visualizations/2026/11/05/01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb/equipment/performance_summary.json).

## Validation fonctionnelle

**179 assertions de la matrice équipements réussies**, sur un hôte listen et trois clients rendus :

- Notifications d’un autre slot, réserve consommée/rechargée, changement entre deux arrosoirs de même modèle, quantité de terreau encore non nulle : identité de l’acteur conservée, compteur de créations/destructions nul.
- Pulvérisation continue, interrompue, reprise, arrosoirs vides : consommation native conservée, rendu et audio présents sur les quatre pairs, extinction après arrêt.
- Recharge par la vraie réserve et le RPC serveur : hôte puis client. Les positions et visées sont préparées par le banc ; les contrôles de portée/visibilité et de réserve restent exécutés.
- Changements rapides d’outil et sélection : l’outil tenu suit le slot courant ; les changements de classe/catalogue, mesh et état de pot reconstruisent toujours la représentation.
- Inventaire plein et prise refusée : pas de remplacement de l’aperçu actuel. Prise réelle d’un objet vers la hotbar puis placement au sol via le résolveur et le RPC natifs.
- Arrosoir physique pris puis déposé par le chemin serveur : réplication, attachement/détachement et composants visibles vérifiés sur les quatre mondes. Il ne s’agit pas d’une nouvelle implémentation de portage.
- Sauvegarde native, fin de PIE puis retour à quatre : clé, InstanceId, slot, quantité et réserve conservés ; aperçu reconstitué à partir des données restaurées.
- Destruction d’un Pawn distant pendant l’arrosage : aperçu propriétaire retiré, pas de quatrième source orpheline. Respawn et nouveau composant QuickBar : rebranchement correct sur les quatre propriétaires.
- Fermeture native d’une connexion PIE : le serveur passe de quatre à trois PlayerControllers. Cette vérification ne certifie pas le parcours complet du menu réseau/Steam côté client.

Ce sont des appels natifs et de vrais RPC sur des états préparés, pas une certification de tous les parcours à la souris. La lecture visuelle distante est vérifiée par les composants/attachements dans les mondes rendus ; aucune revue artistique par quatre utilisateurs humains n’est annoncée. Le scénario complet de transplantation portée reste raccordé et compilé, sans parcours spécifique ajouté à cette matrice.

Régressions : **247 assertions 3A, 153 assertions croissance/récolte 3B, 11 assertions arrivée tardive/reprise : toutes réussies.** Voir [les résultats agrégés](C:/Users/quent/Documents/repos/Botanicus/Docs/EquipmentPresentation3B1-results.json), [3A](C:/Users/quent/.codex/visualizations/2026/11/05/01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb/equipment/regression_3a.json), [croissance 3B](C:/Users/quent/.codex/visualizations/2026/11/05/01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb/equipment/validation_growth.json) et [retour](C:/Users/quent/.codex/visualizations/2026/11/05/01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb/equipment/regression_return.json). Le premier passage 3B a relevé un composant de gerbe vide encore suivi après une capture : il disparaissait à l’étape suivante. Son expiration utilise le temps du monde, alors que le banc n’attendait que le temps réel. La copie de régression de ce chantier exige aussi 1,2 s de simulation écoulée dans **chaque** monde ; aucun délai Niagara ou code d’effet n’a été modifié. [Premier relevé conservé](C:/Users/quent/.codex/visualizations/2026/11/05/01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb/equipment/regression_3b_initial_capture_timing.json).

Les premiers tests du nouveau harnais ont également nécessité de corriger ses attentes pour les changements rapides d’outil, la distance de recharge et une réserve qui masquait le témoin de prise en main. Les règles de jeu n’ont pas été assouplies pour faire passer ces tests.

## Compilation, fichiers et intégrité

**Editor Win64 Development et jeu Win64 Development compilés avec succès sous UE 5.8.2.** [Journal Editor](C:/Users/quent/.codex/visualizations/2026/11/05/01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb/equipment/build_editor_final.log) ; [journal jeu](C:/Users/quent/.codex/visualizations/2026/11/05/01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb/equipment/build_game_final.log). `git diff --check` est exécuté à la livraison. L’instrumentation requiert `Json` et `NetCore` **uniquement** dans les dépendances Editor.

Fichiers préexistants modifiés :

- [Botanicus.Build.cs](C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/Botanicus.Build.cs)
- [BotanicusPlayerController.cpp](C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/BotanicusPlayerController.cpp)
- [BotanicusPlayerController.h](C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/BotanicusPlayerController.h)

Nouveaux helpers :

- [BotanicusEquipmentValidation.h](C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/Editor/BotanicusEquipmentValidation.h)
- [BotanicusEquipmentValidation.cpp](C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/Editor/BotanicusEquipmentValidation.cpp)

Nouveaux outils sous `C:\Users\quent\Documents\repos\Botanicus\Tools\EquipmentPresentation` : runner PowerShell, matrice équipements, benchmark alterné, agrégateur de résultats et copie de la régression de croissance avec attente en temps de simulation.

Préservation SHA-256 : **1317 fichiers préexistants suivis**, 1314 inchangés, 3 sources modifiées, 0 manquant. Aucun ancien `.uasset`, `.umap` ou `.sav` modifié. QuickBar, Character, classes d’eau, sources et assets audio/éclairage/Niagara, documents et outils 3A/3B existants sont conservés. [Preuve d’intégrité](C:/Users/quent/.codex/visualizations/2026/11/05/01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb/equipment/preservation.json).

## Reproduire

Après compilation Editor, lancer [RunValidation.ps1](C:/Users/quent/Documents/repos/Botanicus/Tools/EquipmentPresentation/RunValidation.ps1) avec `-Scenario Validate`, `Profile`, `CountLegacy`, `RegressionGrowth` ou `RegressionReturn`. Le runner prépare des copies des sauvegardes et un profil PIE séparé sous `C:\Users\quent\Documents\repos\Botanicus\Botanicus\Saved\Diagnostics\EquipmentPresentation\isolated_user`. Il contrôle que le résultat JSON est frais et positif, pas seulement le code de sortie du processus. `CountLegacy` active le recensement des composants et compare Niagara coupé/actif ; `Profile` réalise l’ordre alterné complet. Lancer ensuite `python SummarizeEquipment.py` dans le dossier de résultats pour produire le résumé numérique.

La régression 3A utilise l’outil 3B existant. La copie `RegressionGrowth.py` ajoute uniquement l’attente en temps du monde pour le nettoyage. `RegressionReturn.py` reprend le test existant d’arrivée tardive et de sauvegarde, attend la réplication de l’état initial avant de déclencher la maturité et place les observateurs près de l’hôte, comme le fait déjà le test de croissance : l’effet applique sa portée normale de 1 800 cm. Les deux premiers essais de retour avaient reçu la maturité mais compté zéro gerbe sur un observateur ; leurs échecs sont conservés. Les anciens scripts restent inchangés.

## Limites

Pas de cook complet, Shipping, serveur dédié autonome, réseau Steam ou multi-machines avec latence/pertes. Les améliorations mesurées concernent le port continu des arrosoirs dans ce banc, pas tous les objets de tous les Blueprints. Les reconstructions pour états complexes et transplantation restent conservatrices. Les références historiques du Blueprint d’arrosoir ne sont pas nettoyées. Aucune modification de modèle 3D, mécanique d’inventaire, consommation, audio, Niagara ou éclairage ; aucun commit.
