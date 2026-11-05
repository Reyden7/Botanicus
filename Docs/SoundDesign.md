# Botanicus — Sound Design & Ambiance, première passe

5 novembre 2026 — Unreal Engine **5.8.2**, vérifié dans `Engine/Build/Build.version`. Dépôt réel : `C:\Users\quent\Documents\repos\Botanicus`. Projet : `C:\Users\quent\Documents\repos\Botanicus\Botanicus\Botanicus.uproject`. Point de départ Git : `05637f2ac55b0ba18546d8c4eff979da13bfb990`.

## Résultat et périmètre

Couche audio intégrée en C++ et dans les assets Unreal : deux fonds continus jour/nuit, filtrage intérieur, renforcement nocturne clandestin, feedbacks botaniques spatialisés et confirmations privées de transactions. **21 assets ajoutés**, dont **14 SoundWaves**, 5 SoundConcurrency, une atténuation et un catalogue. Les imports utilisent AssetImportTask/SoundFactory et les copies d’ambiance EditorAssetLibrary ; aucun binaire Unreal n’a été modifié directement.

Le vent et les insectes du projet donnent une première distinction jour/nuit. Ils constituent des ressources naturelles provisoires ; une faune extraterrestre, un fond d’équipements et une signature propre aux Noctiflores restent à fournir. La préparation reçoit actuellement le même traitement intérieur doux que la boutique.

**Aucun mesh, blockout, niveau, Blueprint existant, fichier d’éclairage ni paramètre climatique n’a été modifié.** Les fonctions de jeu reçoivent uniquement des appels audio à leurs points de réussite. Aucun prix, délai, stock, seuil de croissance, consommation ou condition d’interaction n’a changé. Aucun commit créé, aucun verrou LFS libéré.

## Inspection préalable

L’audit et l’inventaire Markdown/JSON du dossier `C:\Users\quent\.codex\visualizations\2026\11\05\01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb` ont été relus. Les 31 MP3 ont été comparés aux SHA-256 de l’inventaire : inchangés, 30 contenus uniques. Le dossier contient aussi le PDF et le classeur de catalogue ; aucune source WAV originale n’a été trouvée. Leurs données de catalogue ne remplacent pas une preuve de licence.

La recherche des appels de lecture dans les sources du module ne montrait pas de feedback audio natif pour ces interactions. L’inspection Unreal a confirmé les SoundWaves d’ambiance existants du dossier `/Game/EasyBuildingSystem/Audio/Sounds/Ambience`, dont :

| Ressource existante réutilisée | Durée | Configuration constatée |
|---|---:|---|
| SFX_Wind_001 | 60,504 s | Stéréo, 44,1 kHz, boucle |
| SFX_CricketLongAwayNightLoop | 94,507 s | Stéréo, 44,1 kHz, boucle |

Ces deux assets ont été **dupliqués**, puis configurés dans le nouveau dossier audio. Les originaux EasyBuildingSystem sont conservés. Les cues de forêt présents n’ont pas été empilés sur ces fonds.

## Assets et réglages dans Unreal

Catalogue : `/Game/Botanicus/Audio/DA_BotanicusAudio`, fichier `C:\Users\quent\Documents\repos\Botanicus\Botanicus\Content\Botanicus\Audio\DA_BotanicusAudio.uasset`.

Les SoundWaves se trouvent dans `/Game/Botanicus/Audio/Sounds`, les fonds dans `/Game/Botanicus/Audio/Ambience`, l’atténuation et les limites de voix dans `/Game/Botanicus/Audio/Settings`. Les WAV préparés et leur manifeste sont conservés dans `C:\Users\quent\Documents\repos\Botanicus\Botanicus\SourceArt\Audio`. Les MP3 de `C:\Users\quent\Documents\Botanicus\sons` n’ont été ni déplacés ni réécrits.

Ouvrir le catalogue pour ajuster les gains, pitchs fixes, intervalles minimaux, sons et concurrences par événement ; les volumes du vent, des insectes et de l’arrosage ; les régions et la durée des transitions. **Project Settings → Botanicus Audio** expose le choix du catalogue via UDeveloperSettings. Le dossier audio est ajouté à `DirectoriesToAlwaysCook` dans DefaultGame.ini afin de conserver aussi les références chargées par configuration. Un build distribué/cook complet n’a pas été validé dans cette passe.

| Réglage initial | Valeur |
|---|---|
| Vent extérieur / intérieur | 0,080 / 0,018 |
| Insectes nocturnes extérieur / intérieur | 0,055 / 0,012 |
| Renfort clandestin nocturne | +0,040 dans sa région, commerce clandestin actif |
| Transition de volume | Approche lissée, ~95 % en 1,5 s ; aucun restart |
| Bord de région | Fondu de 200 cm au-delà de la boîte |
| Passe-bas vent extérieur → intérieur | 18 kHz → 2,8 kHz |
| Passe-bas insectes extérieur → intérieur | 16 kHz → 3,5 kHz |
| Arrosage | Gain 0,45 ; attaque 60 ms ; sortie 120 ms |
| Interactions 3D | Sphère 80 cm, falloff 1 500 cm, modèle logarithmique |
| Rejet préalable des one-shots éloignés | Au-delà de 1 800 cm de la caméra locale |
| Gain des feedbacks | 0,60, plantation 0,80 ; pitch 1, pas d’aléatoire imposé |

Les régions sont des données audio, sans collision, chevauchement physique ni effet sur la culture. Centres et demi-étendues en coordonnées Unreal :

| Région | Centre XYZ | Demi-étendue XYZ |
|---|---|---|
| Boutique | (−9107, −8468, 250) | (600, 1150, 250) |
| Préparation | (−8000, −8200, 250) | (750, 900, 250) |
| Clandestine | (−6400, −7000, 250) | (650, 700, 250) |

Ce sont des boîtes souples ajustables adaptées au blockout actuel. Elles ne suivent pas automatiquement un déplacement ultérieur du bâtiment. Parking et livraison prennent le fond extérieur. La boutique et la préparation ont une enveloppe commune ; pas de faux bruit mécanique ajouté. Une occlusion acoustique par murs et une réverbération de pièce ne sont pas implémentées.

## Correspondance des événements

« Testé » ci-dessous précise le parcours natif réellement exécuté. « Raccordé et compilé » n’affirme pas une validation fonctionnelle de l’interaction complète à la souris.

| ID catalogue | Source locale | Déclencheur existant | Validation réalisée |
|---|---|---|---|
| PotPlace | Poser pot vide.mp3 | Placement serveur d’un nouveau pot ou déplacement validé | Raccordé et compilé ; placement souris complet non rejoué |
| EquipmentPlace | poser gros items sur sol.mp3 | Équipement : confirmation existante ou placement d’objet générique | Transport réseau testé ; placement coopératif complet non rejoué |
| Plant | Déposer graine.mp3 | Après consommation réussie de la graine ; pots simple, multiple et clandestin | Pot simple testé à 4 ; variantes raccordées et compilées |
| WaterLoop | Arrosage boucle.mp3 | Après consommation effective d’eau : pot simple/multiple/clandestin ou pulvérisation libre | Pot simple et état réseau à 4 sources testés ; autres parcours non rejoués |
| GrowthYoung | Passage stade jeune.mp3 | Franchissement 30 % en simulation serveur | Croissance native du pot simple testée à 4 |
| GrowthLarge | Passage stade mature.mp3 | Franchissement 70 % en simulation serveur | Croissance native du pot simple testée à 4 |
| Mature | Plante prète a vendre.mp3 | Franchissement 1 − KINDA_SMALL_NUMBER en simulation serveur | Croissance native du pot simple testée à 4 |
| Harvest | Récolter plante.mp3 | Produit effectivement accepté par l’inventaire | Pot simple testé à 4 ; multiple et clandestin raccordés |
| Remove | Passage stade mature.mp3 = Retirer Plante.mp3 | Dépotage réussi par truelle, hors retour d’une graine non développée | Raccordé et compilé ; non rejoué en interaction |
| Transfer | Poser pot vide.mp3 | Rempotage réussi d’une plante portée dans un pot de culture | Réemploi de contact de pot ; raccordé, non rejoué |
| Prepared | Plante prète a vendre.mp3 | Ajout réussi d’une plante compatible au pot de vente, ou rempotage porté | Raccordé et compilé ; préparation à l’établi non rejouée |
| Listed | Validation.mp3, conversion mono | TryMountSalePotState accepté sur un présentoir | Point natif testé à 4 ; parcours UI complet non rejoué |
| Purchase | Confirmation.mp3, stéréo 2D | Commande validée, débit accepté et livraison programmée | Vraie commande de terreau testée ; propriétaire hôte uniquement |
| Sale | Validation.mp3, stéréo 2D | CreditPlantSale après crédit accepté | Point de crédit testé hôte puis client distant ; cycle PNJ complet non rejoué |
| Delivery | Notification.mp3, stéréo 2D | CompleteCatalogOrder avec DeliverCatalogItem réussi | Livraison réellement terminée par son timer et notification propriétaire testées |

Les stades sonores de croissance reprennent les passages visuels existants 30 %, 70 % et maturité. Un seul feedback, le stade le plus élevé, est émis si une mise à jour franchit plusieurs seuils. Les restaurations de sauvegarde et les OnRep ne rejouent pas les feedbacks de croissance. Aucun événement artificiel de germination ajouté.

Le retrait réutilise volontairement `SW_GrowthLarge` : « Retirer Plante.mp3 » et « Passage stade mature.mp3 » sont strictement identiques. `SW_Mature` sert aussi à la plante préparée ; `SW_PotPlace` au transfert. La mise en vente reprend une validation générique, sans prétendre disposer d’une texture dédiée.

## Architecture et multijoueur

`UBotanicusAudioWorldSubsystem` existe uniquement dans les mondes Game/PIE. Un timer local de 0,1 s ajuste les fonds et les boucles ; aucun Tick d’acteur audio n’est ajouté. Le temps, la saison et l’activation clandestine sont **lus** depuis BotanicusGameState / BotanicusSeasonSettings. BotanicusAmbienceDirector n’est pas modifié et ne reçoit pas de dépendance audio. Le son n’écrit jamais dans la simulation climatique.

`ABotanicusAudioRelay`, toujours pertinent et sans Tick, est créé uniquement côté autorité. Il diffuse les événements 3D par multicast non fiable : une perte éventuelle ne modifie pas le jeu. Le serveur listen joue uniquement dans l’implémentation du multicast, sans deuxième appel local. Chaque client rejette les séquences déjà reçues. Un délai par ID/cellule spatiale de 40 cm coalesce les répétitions au même endroit ; la table est bornée. Concurrency complète cette protection.

L’arrosage réplique **les sources actives**, pas la position ou l’évolution de chaque échantillon audio. Les confirmations d’eau déjà présentes alimentent un heartbeat serveur ; seules les entrées/sorties changent le tableau répliqué. Une source a un composant persistant réutilisé localement. Le maintien ne relance pas le fichier. L’arrêt explicite de l’action termine son état ; la pulvérisation libre utilise aussi un délai de sécurité de 0,35 s sans nouvelle consommation. Compter jusqu’à environ 0,65 s avec timers et fondu, hors latence réseau, pour cet arrêt par timeout. Les références invalides sont nettoyées. L’état répliqué et le contrôle local périodique permettent de reconstituer une boucle en cours ; l’arrivée tardive n’a pas fait l’objet d’un test dédié.

`ClientPlayFeedbackAudio` est un RPC fiable sur le PlayerController propriétaire pour achats/ventes/livraisons. Il appelle une lecture 2D locale. Aucun parsing des messages HUD, aucun son joué depuis un OnRep d’inventaire, aucun clic UMG générique ajouté.

| Groupe de concurrence | Voix maximales | Politique |
|---|---:|---|
| Feedback | 8 | Remplacer le plus éloigné, puis le plus ancien |
| Growth | 2 | Même politique ; retrigger global 0,25 s, délai local 0,60 s |
| UI | 3 | Remplacer le plus ancien |
| Water | 4 | Remplacer le plus éloigné, puis le plus ancien |
| Ambience | 2 | Remplacer le plus ancien |

Au plus 19 voix dans ces groupes par périphérique, dont 2 fonds et 4 arrosages. Ce plafond n’inclut pas d’éventuels autres sons du jeu. Les fonds et l’arrosage utilisent PlayWhenSilent pour préserver la continuité ; les fonds restent vivants même avec un gain nul. Les composants sont arrêtés/détruits au démontage du monde. Aucun Niagara, shader ou modèle nouveau ; le coût GPU propre à cette couche audio n’a pas été profilé.

## Préparation audio mesurée

Conversion FFmpeg locale, sans téléchargement : PCM 16 bits à 48 kHz ; mono pour les événements 3D et l’arrosage, stéréo conservée pour les notifications privées. Les gains ci-dessous sont appliqués **après le downmix**, puis de nouveau mesurés. Ils ne constituent pas une normalisation LUFS. La crête de downmix peut différer de celle du décodage stéréo de l’audit. Aucun WAV produit ne dépasse 0 dBFS.

La boucle d’arrosage reçoit un raccord croisé linéaire de 50 ms entre sa fin et son début ; sa durée passe de 48,024 à 47,974 s. Les attaques/sorties sont aussi fondues par le composant. La continuité artistique et le niveau de bruit de fond nécessitent encore une écoute au casque. Le fichier de graine reçoit +10 dB après downmix car son original est très faible ; sa définition sonore reste à écouter/éventuellement refaire.

| WAV | Source | Gain dB | Canaux | Durée s | Crête dBFS | RMS dBFS |
|---|---|---:|---:|---:|---:|---:|
| PotPlace | Poser pot vide.mp3 | -4 | 1 | 0.744 | -4.3 | -24.7 |
| EquipmentPlace | poser gros items sur sol.mp3 | -3 | 1 | 0.888 | -5.3 | -26.3 |
| Plant | Déposer graine.mp3 | +10 | 1 | 0.696 | -16.7 | -40.9 |
| GrowthYoung | Passage stade jeune.mp3 | -2 | 1 | 0.936 | -8.4 | -34.0 |
| GrowthLarge | Passage stade mature.mp3 | -2 | 1 | 1.344 | -8.6 | -32.9 |
| Mature | Plante prète a vendre.mp3 | +0 | 1 | 3.030 | -11.3 | -30.1 |
| Harvest | Récolter plante.mp3 | -1 | 1 | 1.123 | -9.2 | -28.7 |
| WaterLoop | Arrosage boucle.mp3 | -6 | 1 | 47.974 | -3.0 | -24.8 |
| Purchase | Confirmation.mp3 | -4 | 2 | 2.040 | -8.6 | -28.9 |
| Sale | Validation.mp3 | -4 | 2 | 0.744 | -10.0 | -31.0 |
| Delivery | Notification.mp3 | -4 | 2 | 2.456 | -6.3 | -32.1 |
| Listed | Validation.mp3 | -4 | 1 | 0.744 | -9.1 | -29.1 |

`manifest.json` conserve les SHA-256 des sources, les gains, durées et mesures. Peak = crête échantillon, pas true peak. RMS global ne signifie pas LUFS. Aucun mastering artistique ou équilibre perceptif n’est certifié par ces mesures.

## Tests et preuves

Résultat du dernier banc PIE : **77/77 contrôles**, statut global **True**. Ce résultat doit être lu avec la matrice des événements ci-dessus.

- Compilation Development Editor Win64 UE 5.8.2 avec UBT : `C:\Users\quent\.codex\visualizations\2026\11\05\01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb\audio\build.log`.
- Imports officiels : 21 assets enregistrés, aucune erreur ; `C:\Users\quent\.codex\visualizations\2026\11\05\01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb\audio\import_result.json` et `C:\Users\quent\.codex\visualizations\2026\11\05\01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb\audio\import.log`.
- PIE réel : hôte listen + trois clients, GameMode du projet, quatre périphériques audio distincts, sources proches pour tester le partage. Dernier journal : `C:\Users\quent\.codex\visualizations\2026\11\05\01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb\audio\pie_rendered_final.log`. Résultats détaillés avant/après : `C:\Users\quent\.codex\visualizations\2026\11\05\01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb\audio\pie_validation.json`. Le banc suspend les ticks des plantes préexistantes uniquement dans sa copie du monde, pour isoler les compteurs ; les franchissements testés exécutent le Tick natif du pot témoin.
- Compteurs de feedback incrémentés seulement lorsqu’un AudioComponent a effectivement été créé et se trouve en lecture à la sortie de SpawnSound. Vérification distincte des périphériques et des composants de boucle actifs. Cela confirme l’émission/lecture native ; cela ne remplace pas une écoute subjective du son entendu par le joueur.
- Plantation refusée : aucun nouveau feedback. Croissance native simple : jeune, grande, mature une fois sur les quatre pairs. Récolte et montage sur présentoir : une lecture par pair. Confirmations privées : hôte puis client distant, sans lecture supplémentaire chez l’hôte. Commande de terreau et livraison par leur vrai timer.
- Boucle maintenue sans restart, arrêt explicite et timeout ; quatre sources simultanées = quatre boucles par client ; fonds continus, gain jour/nuit cohérent à 12 h et 22 h, intérieur atténué et clandestin renforcé.
- Tentative d’émission depuis un client ignorée. Séquence dupliquée rejetée immédiatement puis après expiration du cooldown.
- Tests C++ `Botanicus.Audio.StageCrossings` et `Botanicus.Audio.RegionTransitions` : **2 réussis, 0 échoué**, résultats dans `C:\Users\quent\.codex\visualizations\2026\11\05\01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb\audio\automation` et `C:\Users\quent\.codex\visualizations\2026\11\05\01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb\audio\automation.log`. L’index JSON distingue ces tests des diagnostics internes émis au démarrage d’Unreal.

Deux limites du banc ont été identifiées puis séparées du comportement produit : les callbacks Python Unreal activent par défaut un drapeau qui force les RPC locaux ; le helper de validation rétablit leur routage normal uniquement pendant les scénarios isolés. Les essais NullRHI sans viewport ne mettent pas correctement à jour l’écouteur des sons courts ; la vérification finale des composants doit être faite avec les viewports actifs (`-RenderOffscreen`). Les premiers essais de routage ne constituaient donc pas à eux seuls une preuve de lecture réelle.

Le helper C++ de validation est inerte hors éditeur. Ses mutations demandent simultanément un monde PIE, `-BotanicusAudioValidation` et un SavedDir contenant `/isolated_user/`. Les sauvegardes originales ont été copiées dans ce profil pour les scénarios. Leur SHA-256 d’origine est inchangé après les tests. Il n’est pas ajouté d’outil de triche actif en jeu distribué.

### Limites de validation

Pas de test Steam, réseau physique multi-machines, serveur dédié, perte/latence simulée, join-in-progress, reconnexion ni build Shipping/cook complet. Le plafond des voix est contrôlé, mais pas de capture Unreal Insights ni de mesure comparative CPU/mémoire en build distribué. Les SoundWaves ajoutés occupent environ 31,3 Mo de fichiers uasset bruts, ce qui ne mesure pas la mémoire de lecture ou la taille compressée cuite. Les réglages de mix restent une première passe mesurée, à auditionner en jeu avec casque et haut-parleurs.

Les journaux de chargement révèlent aussi des dépendances préexistantes manquantes dans `BP_Item_WateringCan` : classes `BotanicusWaterSourceComponent` / `BotanicusWaterVFXComponent` et paquet `/Game/Botanicus/VFX/Watering/NS_Arrosage_Fluids`. Elles n’ont pas été fabriquées ni modifiées dans le chantier audio. Le chemin natif de consommation d’eau utilisé pour le son a été testé séparément. L’avertissement de double installation de NiagaraUIRenderer était également présent avant les imports.

## Sons absents, réserves et licences

| Ressource à fournir | Usage | Type attendu |
|---|---|---|
| Extérieur extraterrestre jour / nuit | Remplacer progressivement le caractère terrestre des insectes existants | Boucles naturelles longues, discrètes, sans parole ni musique imposée |
| Intérieur de boutique | Donner une identité accueillante propre au lieu | Fond doux, sparse, raccord sans clic ; aucun nouveau objet suggéré |
| Équipements de préparation existants | Fond intérieur lié aux appareils réellement présents/en service | Léger ronronnement ou ventilation en boucle, attaque et extinction séparées |
| Noctiflores / clandestin | Signature organique nocturne plus mystérieuse | Pulsation végétale douce et événements rares, non horrifiques |
| Début / fin d’arrosage | Préciser les transitoires au-delà du fondu de boucle | Courts gestes de bec/écoulement, compatibles avec la boucle actuelle |
| Transfert / présentation | Distinguer le rempotage de la simple pose | Contact terre/racines et pot très court |

Aucun fichier téléchargé. La licence de redistribution n’est documentée dans les éléments inspectés pour **aucun des 31 MP3**, ni établie dans cet audit pour les deux sons EasyBuildingSystem repris. Conserver les preuves de provenance et les droits correspondants avant distribution. Les fichiers de catalogue PDF/XLSX présents ne suffisent pas, à eux seuls, à établir ces droits.

## Inventaire complet et décisions

Les mesures ci-dessous reprennent l’inventaire initial puisque les sources sont restées identiques. Le tableau des WAV plus haut décrit les conversions réellement livrées. Les fichiers non sélectionnés restent disponibles dans `sons`.

| Source MP3 | Durée s | Format | Crête dBFS | RMS dBFS | Décision |
|---|---:|---|---:|---:|---|
| Allumage lampe.mp3 | 8.808 | mp3 (mp3float), 48000 Hz, mono, fltp, 256 kb/s | -40.1 | -57.0 | Allumage très faible ; ce fichier ne constitue pas un fond d’équipement. |
| Arrosage boucle.mp3 | 48.024 | mp3 (mp3float), 48000 Hz, stereo, fltp, 256 kb/s | 0.2 | -21.7 | Importé dans cette passe. |
| Changement onglet.mp3 | 0.096 | mp3 (mp3float), 48000 Hz, mono, fltp, 256 kb/s | -17.1 | -26.7 | Disponible pour une passe dédiée ; événement et timing non validés, non importé. |
| click button.mp3 | 0.183 | mp3 (mp3float), 44100 Hz, stereo, fltp, 256 kb/s | -5.7 | -25.8 | Disponible pour une passe dédiée ; événement et timing non validés, non importé. |
| Confirmation.mp3 | 2.040 | mp3 (mp3float), 48000 Hz, stereo, fltp, 256 kb/s | -4.6 | -24.9 | Importé dans cette passe. |
| DetruirePot.mp3 | 1.358 | mp3 (mp3float), 44100 Hz, mono, fltp, 256 kb/s | -3.3 | -29.1 | Disponible pour une passe dédiée ; événement et timing non validés, non importé. |
| Déposer graine.mp3 | 0.696 | mp3 (mp3float), 24000 Hz, stereo, fltp, 160 kb/s | -29.7 | -53.9 | Importé dans cette passe. |
| Erreur.mp3 | 1.620 | mp3 (mp3float), 44100 Hz, stereo, fltp, 256 kb/s | -21.2 | -36.0 | Disponible pour une passe dédiée ; événement et timing non validés, non importé. |
| Fenêtre contextuelle.mp3 | 2.112 | mp3 (mp3float), 48000 Hz, stereo, fltp, 256 kb/s | -3.7 | -18.9 | Disponible pour une passe dédiée ; événement et timing non validés, non importé. |
| Germination.mp3 | 0.888 | mp3 (mp3float), 48000 Hz, stereo, fltp, 256 kb/s | -8.4 | -31.5 | Pas de déclencheur de germination distinct vérifié ; aucun stade de gameplay ajouté. |
| interupteur UI.mp3 | 0.575 | mp3 (mp3float), 44100 Hz, stereo, fltp, 256 kb/s | 0.2 | -30.3 | Disponible pour une passe dédiée ; événement et timing non validés, non importé. |
| Inventory full.mp3 | 3.072 | mp3 (mp3float), 24000 Hz, stereo, fltp, 160 kb/s | -1.7 | -20.6 | 3,07 s dont 2,70 s de fin sous −50 dBFS ; pas d’écoute/montage ni hook transversal. |
| Notification.mp3 | 2.456 | mp3 (mp3float), 44100 Hz, stereo, fltp, 256 kb/s | -2.3 | -28.1 | Importé dans cette passe. |
| Passage stade jeune.mp3 | 0.936 | mp3 (mp3float), 48000 Hz, stereo, fltp, 256 kb/s | -9.4 | -35.0 | Importé dans cette passe. |
| Passage stade mature.mp3 | 1.344 | mp3 (mp3float), 48000 Hz, stereo, fltp, 256 kb/s | -9.5 | -33.9 | Importé dans cette passe. |
| Plante prète a vendre.mp3 | 3.030 | mp3 (mp3float), 44100 Hz, stereo, fltp, 256 kb/s | -13.9 | -33.0 | Importé dans cette passe. |
| Poser arosoir.mp3 | 0.199 | mp3 (mp3float), 44100 Hz, stereo, fltp, 165 kb/s | -3.5 | -22.8 | Disponible pour une passe dédiée ; événement et timing non validés, non importé. |
| poser gros items sur sol.mp3 | 0.888 | mp3 (mp3float), 48000 Hz, stereo, fltp, 256 kb/s | -4.9 | -26.3 | Importé dans cette passe. |
| Poser pot vide.mp3 | 0.744 | mp3 (mp3float), 48000 Hz, stereo, fltp, 256 kb/s | -3.3 | -23.5 | Importé dans cette passe. |
| poser sac de terreau.mp3 | 1.207 | mp3 (mp3float), 44100 Hz, mono, fltp, 70 kb/s | -10.0 | -30.3 | Disponible pour une passe dédiée ; événement et timing non validés, non importé. |
| Prendre item.mp3 | 0.810 | mp3 (mp3float), 44100 Hz, stereo, fltp, 256 kb/s | -0.6 | -31.1 | Disponible pour une passe dédiée ; événement et timing non validés, non importé. |
| Prendre Item2.mp3 | 1.541 | mp3 (mp3float), 44100 Hz, stereo, fltp, 256 kb/s | -10.4 | -39.7 | Disponible pour une passe dédiée ; événement et timing non validés, non importé. |
| Remplir arosoir avec reserve d'eau.mp3 | 9.796 | mp3 (mp3float), 44100 Hz, mono, fltp, 256 kb/s | -17.1 | -34.1 | Remplissage de réserve distinct de l’arrosage : boucle et fin d’action non raccordées. |
| Retirer Plante.mp3 | 1.344 | mp3 (mp3float), 48000 Hz, stereo, fltp, 256 kb/s | -9.5 | -33.9 | Doublon exact : référence au même SW_GrowthLarge, pas de second import. |
| Robinet ouvert.mp3 | 2.016 | mp3 (mp3float), 44100 Hz, stereo, fltp, 144 kb/s | -9.9 | -30.0 | Disponible pour une passe dédiée ; événement et timing non validés, non importé. |
| Récolter plante.mp3 | 1.123 | mp3 (mp3float), 44100 Hz, stereo, fltp, 256 kb/s | -11.2 | -30.7 | Importé dans cette passe. |
| SurvolButton.mp3 | 14.211 | mp3 (mp3float), 44100 Hz, stereo, fltp, 256 kb/s | -0.1 | -28.8 | 14,21 s dont 13,76 s de fin sous −50 dBFS ; montage et raccord UMG à valider. |
| Validation.mp3 | 0.744 | mp3 (mp3float), 48000 Hz, stereo, fltp, 256 kb/s | -6.0 | -27.0 | Importé dans cette passe. |
| Verser terreau.mp3 | 7.896 | mp3 (mp3float), 48000 Hz, stereo, fltp, 256 kb/s | -0.9 | -23.9 | 7,90 s : nécessite une boucle pilotée par remplissage, pas un son long après validation. |
| Warning.mp3 | 1.228 | mp3 (mp3float), 44100 Hz, stereo, fltp, 256 kb/s | -10.4 | -25.1 | Disponible pour une passe dédiée ; événement et timing non validés, non importé. |
| watering.mp3 | 20.898 | mp3 (mp3float), 44100 Hz, mono, fltp, 256 kb/s | -1.6 | -28.3 | Alternative conservée ; une seule texture d’arrosage retenue. |

## Fichiers source concernés

- [BotanicusAudioCatalog.h](<C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/Audio/BotanicusAudioCatalog.h>)
- [BotanicusAudioWorldSubsystem.cpp](<C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/Audio/BotanicusAudioWorldSubsystem.cpp>)
- [BotanicusAudioRelay.cpp](<C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/Audio/BotanicusAudioRelay.cpp>)
- [BotanicusPlayerController.cpp](<C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/BotanicusPlayerController.cpp>)
- [BotanicusPlantPotActor.cpp](<C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/Growing/BotanicusPlantPotActor.cpp>)
- [BotanicusMultiPlantPotActor.cpp](<C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/Growing/BotanicusMultiPlantPotActor.cpp>)
- [BotanicusIllegalPlanterActor.cpp](<C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/IllegalTrade/BotanicusIllegalPlanterActor.cpp>)
- [BotanicusSalePotActor.cpp](<C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/Sales/BotanicusSalePotActor.cpp>)
- [BotanicusSalesDisplayActor.cpp](<C:/Users/quent/Documents/repos/Botanicus/Botanicus/Source/Botanicus/Sales/BotanicusSalesDisplayActor.cpp>)
- [DefaultGame.ini](<C:/Users/quent/Documents/repos/Botanicus/Botanicus/Config/DefaultGame.ini>)

S’ajoutent les nouveaux tests et le helper dans `C:\Users\quent\Documents\repos\Botanicus\Botanicus\Source\Botanicus\Audio`. Le dossier `/Game/Botanicus/Audio` peut être retiré du catalogue pour désactiver les lectures sans modifier la simulation. Aucun acteur à placer manuellement dans le niveau pour que cette première passe fonctionne.

Intégrité : 218 fichiers préexistants suivis par SHA-256, 210 strictement inchangés, 8 fichiers de source/config modifiés pour les raccordements. Toutes les maps, tous les fichiers de BotanicusAmbienceDirector, DefaultEngine.ini et tous les fichiers du dossier sons contrôlés sont inchangés. Preuve détaillée : `C:\Users\quent\.codex\visualizations\2026\11\05\01a1ab22-a1c2-7da2-a0d3-9414cc2eafdb\audio\integrity.json`. Le verrou existant `Lvl_BuildingTest.umap`, Reyden7, ID 61708748 est conservé.

## Suite recommandée

1. Écouter ensemble cette passe dans la boutique et sur quatre actions successives : poser, planter, arroser, récolter. Ajuster le catalogue en priorité, sans changement de gameplay.
2. Valider à la souris les variantes multi-pots/clandestines, le rempotage, la préparation et le placement coopératif ; vérifier annulation, inventaire plein, changement d’outil, sortie de portée et destruction de l’objet. Ces parcours ne sont pas déclarés fonctionnels ici sur la seule base de la compilation.
3. Tester sur deux machines puis quatre, avec latence/perte, arrivée tardive et déconnexion pendant l’arrosage ; mesurer audio thread, streaming et voix avec les outils Unreal. Tester un cook ciblé puis un build distribué.
4. Remplacer les seules ambiances provisoires lorsque les ressources manquantes et leurs licences sont disponibles ; faire ensuite la passe UI dédiée (clic, survol nettoyé, onglets, refus), avec des événements typés fiables.
