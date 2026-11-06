# Botanicus — Caisse manuelle 4A.2

Rapport du 6 novembre 2026. Projet réel : `C:/Users/quent/Documents/repos/Botanicus/Botanicus/Botanicus.uproject`, Unreal Engine **5.8.2**. Les ressources externes fournies restent dans `C:/Users/quent/Documents/Botanicus/sons`.

## Résultat

Le parcours manuel utilise désormais une scanette partagée, un scan validé par le serveur, un tiroir animé, le dépôt du paiement puis une collecte en un clic. Le crédit passe toujours par `CreditPlantSale` et le portefeuille partagé existant. Les caisses automatiques et le commerce clandestin gardent leurs parcours natifs.

Le niveau et le Blueprint de caisse existant n'ont pas été sauvegardés ni remplacés. Les composants natifs ajoutés apparaissent sur la caisse existante au chargement. Aucun modèle 3D importé ; les nouvelles formes utilisent les primitives de l'Engine.

## Inspection et éléments concernés

Le niveau inspecté est `/Game/Botanicus/Maps/Lvl_BuildingTest`, disque `C:/Users/quent/Documents/repos/Botanicus/Botanicus/Content/Botanicus/Maps/Lvl_BuildingTest.umap`. La caisse présente est `BP_Item_CashRegister_C_0`, issue de `/Game/Botanicus/blueprints/BP_Item_CashRegister`, parent `ABotanicusCashRegisterActor`, avec `bUseBlueprintAppearance=true`. Elle possède un comptoir, un bloc de caisse et un écran, sans tiroir initial. Ses collisions physiques restent inchangées. Les six emplacements de caisses automatiques restent gérés par le code existant.

Les visiteurs sont des `ABotanicusVisitorCharacter` natifs créés par le gestionnaire de visiteurs ; aucun Blueprint `/Game/Botanicus/blueprints/BP_Visitor` n'a été trouvé. La vente est portée par `ABotanicusSalesDisplayActor`, son visiteur réservé, la plante emportée et le vendeur. L'ancien parcours manuel utilisait `HandlePlayerCheckoutAction`, d'abord pour scanner puis pour appeler `CompleteVisitorPurchase`.

Le portefeuille, les objectifs et les compteurs restent dans `ABotanicusGameState`. `ABotanicusPlayerController::CreditPlantSale` reste l'entrée de crédit public ; un argument facultatif, vrai par défaut, permet seulement de supprimer le feedback `Sale` pendant le nouveau parcours manuel. Le chemin automatique conserve cet argument par défaut. Le calcul de satisfaction `RecordVisitOutcome` est conservé ; le résultat positif est consigné une fois à l'acceptation du scan, afin qu'une sauvegarde pendant l'ouverture ne perde pas cet avis. Le départ ultérieur ne le compte pas à nouveau. Les règles de qualité, tendances et attente restent celles du calcul natif.

La QuickBar conserve ses objets et notifications. La scanette est un acteur dédié appartenant à la caisse, sans nouvel objet d'inventaire. L'objet de QuickBar local est temporairement masqué puis restauré ; il n'est pas détruit pour équiper la scanette. La pose locale suit la caméra ; les représentations distantes utilisent `hand_r` lorsqu'il existe, avec un repli sur la pose de visée. Une disparition du Pawn ou de son contrôleur restitue le même acteur au support. Une reprise de sauvegarde remet la scanette sur son support.

## Fichiers C++ et configuration

Racine des sources : `C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus`.

| Fichiers sous cette racine | Rôle |
| --- | --- |
| `Sales/Checkout/BotanicusCheckoutTypes.h` | Reçu autoritaire et données persistées, identités client/plante/transaction/caisse. |
| `Sales/Checkout/BotanicusCheckoutSettings.h/.cpp` | Références d'assets, durées, arcs, rotation et budgets exposés dans Project Settings. |
| `Sales/Checkout/BotanicusCheckoutScanner.h/.cpp` | Scanette unique, possession répliquée, support, pose locale/distante et restitution. |
| `Sales/Checkout/BotanicusPaymentCoins.h/.cpp` | Un acteur de paiement réutilisé par caisse, pièces en ISM, reliquat explicite et animation locale. |
| `Sales/BotanicusCashRegisterCheckout.cpp` et `Sales/BotanicusCashRegisterActor.h/.cpp` | Extension de la caisse existante, machine à états, points de référence et transitions serveur. |
| `BotanicusPlayerControllerCheckout.cpp` et `BotanicusPlayerController.h/.cpp` | Entrées E/clic gauche, RPC, contrôles de visée et indication HUD unique. |
| `Sales/BotanicusSalesDisplayActor.h/.cpp`, `Visitors/BotanicusVisitorCharacter.h/.cpp` | Transfert du lot vers le reçu sans crédit anticipé ; cible de scan sur la plante réellement portée ; exclusion d'un client déjà traité. |
| `Save/BotanicusWorldSaveGame.h`, `BotanicusGameMode.cpp` | Schéma 33, reçus des caisses et identités des lots ; anciens champs conservés. |
| `Visuals/BotanicusCheckoutVisualTypes.h`, `Visuals/BotanicusCheckoutJuice.cpp`, `Visuals/BotanicusJuiceWorldSubsystem.h/.cpp` | Rayon et impact courts dans le sous-système cosmétique existant. |
| `Audio/BotanicusAudioWorldSubsystem.h/.cpp` | Contacts de pièces locaux avec pitch discret ; mêmes catalogue, atténuation et limitation existants. Le relais réseau audio est inchangé. |
| `UI/BotanicusInteractionTargetWidget.h/.cpp` | Statut contextuel sans fausse touche E, puis restitution des décorations des interactions normales. |
| `Editor/BotanicusCashRegisterValidation.h/.cpp` | Fixtures protégées, entrées natives, fermetures de connexions, mesures d'état et capture brute. |

`C:/Users/quent/Documents/repos/Botanicus/Botanicus/Config/DefaultGame.ini` déclare la section des réglages de caisse. Les classes QuickBar, les sources de croissance/arrosage/commerce clandestin, la logique du relais audio, l'éclairage et les scripts des chantiers précédents ne sont pas modifiés.

## Machine à états et sécurité financière

| État du reçu | Transition serveur | Présentation |
| --- | --- | --- |
| `Idle` | Scan accepté avec outil possédé, client et plante attendus, caisse opérationnelle et disponible. | Indications de prise de scanette ou de scan. |
| `Opening` | Le présentoir cède le lot au reçu, sans crédit ; montant et identités sont figés. | Rayon bleu, bip et ouverture. |
| `AwaitingCollection` | Ouverture terminée ; appel au départ natif du client. | Dépôt court puis pièces persistantes sur le comptoir. |
| `Collecting` | Clic accepté : reçu consommé **avant** le crédit natif, une seule fois. | Pièces successives vers le tiroir. |
| `Closing` | Délai serveur après la durée bornée du flux. | Fermeture et son mécanique, puis `Idle`. |

Les RPC fiables sont portés par le PlayerController possédé, pas par un acteur de paiement accessible arbitrairement. Le serveur refait le premier impact de visibilité, vérifie l'outil et son porteur, l'identité du client, la clé de sa plante et la caisse concernée. La distance utilise `MaximumWorldInteractionDistance`, 230 cm par défaut. Une mauvaise cible, un objet interposé, un reçu périmé ou un deuxième scan ne produisent aucun effet de réussite.

La consommation du paiement est atomique dans la séquence du Game Thread serveur : identifiant attendu valide, phase `AwaitingCollection`, montant positif, puis `bCredited=true` et passage en `Collecting`, **avant** `CreditPlantSale(..., false)`. Les requêtes concurrentes ou réentrantes ne peuvent plus satisfaire ces conditions. Un garde en int64 évite de déborder le portefeuille ou le chiffre d'affaires int32 ; le paiement reste en attente si le crédit ne peut pas être représenté exactement. Aucune arrivée de pièce, callback sonore ou fermeture du tiroir ne déclenche de crédit.

Chaque pièce dorée vaut exactement 10 crédits. Un bon distinct et étiqueté représente le reste exact, sans arrondir. Au-delà du budget visuel, le même bon représente la valeur restante ; il ne s'agit pas de pièces dorées de valeur variable.

| Prix natif testé | Pièces dorées | Bon exact | Durée du flux |
| ---: | ---: | ---: | ---: |
| 30 | 3 | 0 | 0,42 s |
| 50 | 5 | 0 | 0,58 s |
| 100 | 10 | 0 | 0,98 s |
| 57 | 5 | 7 | 0,66 s |
| 10 007 | 96 | 9 047 | 2,60 s |

Les pièces ne sont pas des objets d'inventaire. Il y a deux acteurs supplémentaires réutilisés par caisse : scanette et paiement, et non un acteur par pièce. Les transforms des pièces sont calculés localement à partir du reçu, de l'horloge serveur et des composants de référence réels ; ils ne sont pas répliqués à chaque frame.

## Sauvegarde et reprise

Le reçu contient des GUID distincts pour la transaction, le client et le lot de plante ; la caisse possède aussi son GUID. Le reçu n'est pas la propriété du PNJ : son départ ou sa destruction ne suppriment pas le paiement. Les données de caisse sont ajoutées à la sauvegarde existante, avec le portefeuille natif. Le schéma 33 ajoute des champs optionnels ; une sauvegarde antérieure fournit une liste de reçus vide. Les sauvegardes originales ont été chargées via des copies, sans être écrites.

À la restauration, un reçu ouvert mais non crédité redevient un paiement disponible et visible, sans rejouer le scan. Un reçu déjà crédité, en collecte ou en fermeture revient fermé et consommé. Le test redémarre réellement PIE pour chacun de ces cas et inclut une arrivée tardive. Le test de paiement en attente vérifie aussi le passage par l'autosauvegarde native de fin de session.

## Assets et sons

Neuf nouveaux assets ont été créés via les API Unreal, sans écriture binaire artisanale :

- `/Game/Botanicus/blueprints/Checkout/BP_BotanicusScanner` et `BP_BotanicusPaymentCoins` ;
- `/Game/Botanicus/Materials/Juice/Checkout/M_CheckoutBody` et `M_CheckoutBeam` ;
- `/Game/Botanicus/Audio/Settings/SCON_CheckoutCoins` ;
- `/Game/Botanicus/Audio/Sounds/SW_CheckoutScannerBeep`, `SW_CheckoutRegisterOpen`, `SW_CheckoutCoinPickup`, `SW_CheckoutRegisterClose`.

Leur racine disque est `C:/Users/quent/Documents/repos/Botanicus/Botanicus/Content/Botanicus`. Les primitives viennent de `/Engine/BasicShapes`. `FutureScannerMesh` reste configurable pour un modèle ultérieur ; aucun n'a été importé.

Le catalogue existant `C:/Users/quent/Documents/repos/Botanicus/Botanicus/Content/Botanicus/Audio/DA_BotanicusAudio.uasset` reçoit quatre événements. Les 14 entrées de feedback précédentes ont été comparées sémantiquement et conservées. Ni les boucles ni les sons précédents ne sont remplacés. Le nouveau parcours manuel coupe seulement le feedback `Sale` historique ; la caisse automatique continue de l'utiliser.

| MP3 source vérifié dans `C:/Users/quent/Documents/Botanicus/sons` | Événement | WAV : durée | Gain | Crête sortie |
| --- | --- | ---: | ---: | ---: |
| `scaner bip.mp3` | `ScannerBeep` | 0,480 s | −7,98 dB | −6,00 dBFS |
| `cash registrer open.mp3` | `RegisterOpen` | 1,806 s | −8,49 dB | −6,00 dBFS |
| `money pickup.mp3` | `CoinPickup` | 0,366 s | −6,52 dB | −6,00 dBFS |
| `close Caisse.mp3` | `RegisterClose` | 0,919 s | +6,00 dB | −9,24 dBFS |

Les quatre WAV sont PCM16, mono, 48 kHz, dans `C:/Users/quent/Documents/repos/Botanicus/Botanicus/SourceArt/Audio/Checkout`, avec `manifest.json` : empreinte de chaque MP3, durées, seuils, gains et niveaux RMS/crête. La conversion utilise un seuil prudent de −58 dBFS, garde une marge de résonance de 120 ms et applique des fondus de 1/5 ms. Seules les fins silencieuses excessives sont retirées. Ce contrôle de crêtes/RMS ne constitue pas une normalisation perceptuelle LUFS ni une validation d'écoute sur tous les appareils. `money pickup(1).mp3` n'est pas présent localement ; aucune comparaison de cette variante n'est revendiquée.

Les contacts utilisent l'atténuation existante, une concurrence maximale de trois voix, une réactivation de 40 ms, une limitation de 55 ms au catalogue et un pitch entre 0,97 et 1,03. Les flux denses peuvent donc produire moins de sons que de pièces : c'est la limitation voulue. Le scan et les mouvements mécaniques passent par le relais existant ; les contacts sont locaux et ne rejouent pas l'historique lors d'une arrivée tardive.

## Réglages et budgets

Project Settings → Game → **Botanicus Cash Register Juice** expose le rayon (0,20 s, largeur 0,6 cm), l'ouverture (0,45 s), la fermeture (0,35 s), le dépôt (0,25 s, arc 5 cm), l'intervalle (0,08 s), le vol (0,26 s, arc 8 cm), la rotation (30°), la pause finale (0,08 s), le plafond (96 pièces) et la durée maximale du flux (2,60 s). Le flux accélère pour les gros paiements. Les réglages sont bornés ; désactiver les animations ou utiliser la faible qualité ne désactive pas la transaction.

Les composants `ScannerDock`, `ScannerMuzzle`, `CoinDropPoint`, `DrawerTarget`, `DrawerClosedTarget` et `DrawerOpenTarget` sont éditables. Le tiroir final est intégré à la façade du comptoir, de `(-23,0,69)` à `(-48,0,69)` en coordonnées locales de la caisse ; les collisions du comptoir sont conservées. Le bloc de caisse et l'écran historiques restent à leurs emplacements existants.

Le rayon utilise deux composants temporaires sans collision ni ombre, avec un maximum de quatre rayons actifs par monde et une limite de distance. Aucun Niagara supplémentaire ni Fluids. Les acteurs de paiement ne tickent que pendant les mouvements ; la scanette ne ticke que lorsqu'elle est portée. La caisse utilisait déjà un Tick ; son extension y applique les transitions et la pose du tiroir. Les chemins cosmétiques ajoutés sont gardés contre `NM_DedicatedServer` ; aucun serveur dédié n'a été exécuté dans cette validation.

## Validation effectuée

| Validation | Résultat | Preuve |
| --- | --- | --- |
| Caisse, identité, refus, concurrence, reprise, caisse automatique et commerce clandestin | 238 vérifications, réussite | `C:/Users/quent/Documents/repos/Botanicus/Botanicus/Saved/Diagnostics/CashRegisterJuice4A2/validation_checkout.json` |
| Déconnexions réelles et arrivées tardives | 13 vérifications, réussite | `C:/Users/quent/Documents/repos/Botanicus/Botanicus/Saved/Diagnostics/CashRegisterJuice4A2/validation_disconnect.json` |
| Plantation/arrosage 3A | 247 vérifications, réussite | `C:/Users/quent/Documents/repos/Botanicus/Botanicus/Saved/Diagnostics/VisualJuice3B/pie_result.json` |
| Croissance/récolte 3B | 153 vérifications, réussite | `C:/Users/quent/Documents/repos/Botanicus/Botanicus/Saved/Diagnostics/PlacementJuice4A1/validation_growth.json` |
| Équipements et optimisation 3B.1 | 185 vérifications, réussite | `C:/Users/quent/Documents/repos/Botanicus/Botanicus/Saved/Diagnostics/PlacementJuice4A1/validation_equipment.json` |
| Placement/hologramme/POP 4A.1 | 123 vérifications, réussite | `C:/Users/quent/Documents/repos/Botanicus/Botanicus/Saved/Diagnostics/PlacementJuice4A1/validation_placement.json` |
| Noctiflores 3C | 125 vérifications, réussite | `C:/Users/quent/Documents/repos/Botanicus/Botanicus/Saved/Diagnostics/VisualJuice3C/validation_noctiflores.json` |
| Audio : `StageCrossings`, `RegionTransitions` | 2 tests Automation, réussite | `C:/Users/quent/Documents/repos/Botanicus/Botanicus/Saved/Diagnostics/CashRegisterJuice4A2/AudioAutomation/index.json` |
| Compilation Editor et Game Development Win64 | Réussite UE 5.8.2 | `C:/Users/quent/.codex/visualizations/2026/11/05/01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb/cashregister/build_final_editor.log` et `build_final_game.log` |

Les résultats natifs sont dans `C:/Users/quent/Documents/repos/Botanicus/Botanicus/Saved/Diagnostics/CashRegisterJuice4A2` ; ceux des régressions restent dans les dossiers `VisualJuice3B`, `PlacementJuice4A1` et `VisualJuice3C` sous `Saved/Diagnostics`.

Les scénarios utilisent de vrais mondes PIE et les classes du jeu. Les fixtures préparent un client et un présentoir par les fonctions natives et modifient uniquement un prix du catalogue **en mémoire**, pour couvrir plusieurs montants. Les commandes E/clic gauche sont injectées via `InputKey` ; les demandes invalides, répétées et concurrentes utilisent les RPC natifs. **Aucun test à la souris physique n'est revendiqué.** Les actions sont programmées dans le Tick du monde concerné : le diagnostic initial a montré que des appels immédiats depuis Slate hors contexte PIE pouvaient fausser l'acheminement des RPC. Cette correction concerne le banc, pas le relais audio du jeu.

Les fermetures de connexion utilisent réellement `NetConnection::Close` sur le porteur distant, avant puis pendant une collecte. Unreal peut garder le monde déconnecté en standalone ; le test l'exclut du compte des mondes connectés et vérifie 4 → 3 → 4 avec une vraie arrivée tardive. La destruction d'un Pawn et sa réapparition sont aussi testées. La suite vérifie les GUID, les refus sans succès, la valeur exacte, les crédits immédiats, les compteurs natifs, le reliquat, les flux bornés, les arrivées décalées, le nettoyage et les reprises.

La suite finale de caisse compte **238 vérifications réussies**. Les cinq montants du tableau sont relevés dans les reçus et le rendu instancié. 735 observations de présentation montrent une première pièce déjà arrivée alors que la dernière reste visible. Après les flux, les quatre mondes reviennent à `Idle`, sans pièces dorées restantes ni rayon actif. Le feedback `Sale` ne se rejoue pas lors des collectes manuelles ; la caisse automatique crédite 50 crédits via son parcours natif. La vente clandestine native retire 3 produits, crédite 450 crédits et augmente la suspicion de 8, sans modifier le reçu public ni ses compteurs. La validation nocturne est effectuée après les reprises de caisse pour garder ces scénarios indépendants des changements de journée et de leurs interfaces.

## Mesures de performance

Machine : Intel Core i5-14600KF (14 cœurs/20 threads), 32 Go, RTX 5070 12 Go, pilote 616.92. Au contrôle précédant le benchmark : GPU 0 %, 2 422 Mio occupés sur 12 227 Mio. Aucun autre jeu 3D lourd n'a été identifié avant les mesures. Les compilations et captures sont terminées avant la chauffe du benchmark.

Le protocole utilise huit blocs OFF/ON/ON/OFF/ON/OFF/OFF/ON, 15 s de chauffe, 6 s de stabilisation et 24 s mesurées par bloc. Une transaction native de 1 000 crédits (96 pièces et bon de 40) est déclenchée toutes les 6 s. Les quatre viewports sont `[[1049, 519], [960, 542], [960, 542], [960, 542]]`, avec Effects/GI/Reflection/Shading/Shadow/PostProcess à 3 et Lumen. **1690 échantillons** sont relevés sur **192.02 s**, à environ 8,8 Hz effectifs. Les quatre encaissements par bloc produisent bien 4 000 crédits, soit 32 encaissements dans les fenêtres mesurées.

| Mesure (ms) | OFF, moyenne des 4 blocs | ON, moyenne des 4 blocs | Écart ON−OFF | P95 OFF | P95 ON |
| --- | ---: | ---: | ---: | ---: | ---: |
| Frame | 22.650 | 22.315 | -0.334 | 30.835 | 28.824 |
| Game Thread | 20.347 | 20.085 | -0.261 | 27.413 | 26.190 |
| GPU | 17.539 | 17.619 | +0.081 | 22.055 | 22.551 |

Les écarts Game Thread des quatre paires de blocs sont -0.259 ms, -0.221 ms, -0.738 ms, +0.173 ms. Leur signe varie ; **aucun gain CPU ni absence de coût global n’est revendiqué**. OFF conserve les états, les sons et le calendrier local des arrivées, avec une partie des calculs de présentation ; cette comparaison ne retire donc pas tout le travail CPU du système de pièces. Les coûts des fixtures et des quatre mondes sont également inclus.

L’instrumentation d’équipement relève **0 création et 0 destruction d’arrosoir** pendant les blocs, avec `BP_Item_WateringCan_C_0` conservé par monde. La même scanette `BP_BotanicusScanner_C_0` reste réutilisée. Après vidange des animations, les quatre mondes sont à `Idle`, zéro rayon et zéro pièce dorée rendue. Les instances sont bornées à 96 par caisse ; ces mesures ne qualifient pas une scène avec de nombreuses caisses.

Données brutes : `C:/Users/quent/Documents/repos/Botanicus/Botanicus/Saved/Diagnostics/CashRegisterJuice4A2/profile_checkout.json`. Synthèse : `C:/Users/quent/.codex/visualizations/2026/11/05/01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb/cashregister/performance_summary.json`.

La comparaison OFF/ON conserve le nouveau parcours, ses acteurs, les RPC, les sons et les opérations financières. Elle mesure donc le supplément de présentation animée dans ce banc PIE ; elle ne mesure ni un gain face à l'ancienne caisse, ni le coût total du chantier dans un exécutable distribué. Les échantillons d'un même bloc sont corrélés. Le Render Thread quasi nul renvoyé par cette API n'est pas interprété comme un coût de rendu réel.

## Intégrité, limites et reprise manuelle

Le contrôle SHA-256 porte sur **1443 fichiers préexistants** : aucune suppression, la sauvegarde originale et les **37 fichiers de ressources sonores sources** sont intacts. Aucun `.umap` ni ancien son n’a changé. Le **seul asset binaire préexistant modifié** est le catalogue audio prévu. Les assets de lumière, Niagara, Noctiflores, plantation/croissance et placement 4A.1 sont intacts. `git diff --check` passe. Détails : `C:/Users/quent/.codex/visualizations/2026/11/05/01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb/cashregister/integrity.json`.

Le verrou LFS existant du niveau (`61708748`, Reyden7) est conservé. Le catalogue audio a été réservé sous le même propriétaire (`62491395`) pour permettre sa sauvegarde légitime ; ce verrou est conservé pour la revue. HEAD reste `42497d6f93930e9d7604c9b4a01a8bf2968c40eb`. **Aucun commit.**

Les scripts reproductibles sont dans `C:/Users/quent/Documents/repos/Botanicus/Tools/CashRegisterJuice4A2`, avec un README décrivant les flags, profils isolés et protocoles. `PrepareAudio.py` et `BuildAssets.py` reproduisent la préparation et les créations via les API adaptées. Les helpers refusent les profils utilisateur normaux.

La vidéo réelle est `C:/Users/quent/.codex/visualizations/2026/11/05/01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb/cashregister/Botanicus_Checkout4A2.mp4` : **198 images réellement rendues**, environ **9.53 s**, **20.7 images/s observées**, cadence variable respectée, aucune interpolation. Elle montre la prise de scanette, le rayon bleu, le tiroir et le rangement successif des pièces. Elle est **muette et sans la couche HUD Slate**, car elle capture le viewport 3D. Les compteurs audio sont vérifiés séparément ; les captures ne prouvent pas à elles seules l’ergonomie complète de l’interface.

La fixture prépare directement un visiteur en attente de paiement et ne dispose pas d'un circuit de navigation. Le départ natif est déclenché, puis le visiteur est détruit par son comportement existant de fin de route vide. La marche jusqu'à la sortie avec une visite complète n'est donc **pas validée** par cette vidéo ou par cette fixture. Aucun correctif de navigation ni chemin inventé n'a été ajouté dans ce chantier.

Restent une passe à la souris/clavier dans l'éditeur visible pour juger confort de visée, lisibilité du bon, équilibre sonore et trajet de sortie du visiteur, et une vérification en build packagé/cook et sur machines moins puissantes. Les matériaux et les proportions sont ceux d'un blockout réutilisable. Les primitives des nouveaux acteurs sont initialisées au BeginPlay ; leur inspection et leur calibration visuelles se font en PIE, avec les points de référence éditables. Aucun arrêt brutal du processus ni scénario de corruption de sauvegarde n'a été simulé. La mesure à quatre mondes sous un processus ne remplace pas une session réseau sur quatre machines.
