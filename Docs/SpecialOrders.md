# Commandes spéciales des visiteurs

Implémentation du 8 septembre 2026, Unreal Engine 5.8.1.

## Parcours joueur

1. Un visiteur admis dans le magasin peut être choisi pour une commande spéciale. Depuis le parking, il conserve tout le circuit PNJ configuré, avec ses virages et ses raccordements, jusqu’à la zone de caisse. Il se place ensuite sur le côté d’un comptoir opérationnel, sans prendre de plante sur un présentoir ni rejoindre la file de paiement classique. Il ne cherche jamais à couper directement du parking vers le comptoir à travers le décor.
2. À son arrivée, il fait retentir la sonnette fournie. Il sonne de nouveau après **60 secondes** si personne n’a accepté, puis part mécontent à **120 secondes** sans réponse. Lorsque le joueur est assez proche et regarde le visiteur, `WBP_HUD_Interaction` affiche la touche **E**, l’action **POUR PRENDRE LA COMMANDE** et l’identifiant du client. La bulle du visiteur présente le détail de sa demande avant acceptation.
3. Après l’appui sur **E**, la commande apparaît sur le HUD de tous les joueurs, directement sous le panneau **Objectifs niveau** : critères, quantités livrées, récompense et temps restant. Le texte devient orange dans la dernière minute. La proposition en attente n’est pas affichée dans cette liste partagée avant son acceptation.
4. Chaque joueur peut sélectionner une plante dans sa barre rapide et appuyer sur **E** auprès du même client pour livrer une unité. Les contributions sont communes ; seul l’inventaire du contributeur est débité.
5. La dernière livraison verse la récompense aux fonds communs, comptabilise les plantes vendues et produit un avis positif. Le client remercie l’équipe puis repart par le circuit de sortie.
6. Une attente excessive provoque le départ du client et un avis négatif. La fermeture du magasin ou la disparition du comptoir annule aussi une commande en attente.

Les plantes déjà livrées sont engagées : elles ne sont pas rendues si la commande échoue. La récompense est versée uniquement lorsque toute la commande est terminée. Un pot de vente préparé est livré avec son contenu.

## Demandes et critères

Les demandes automatiques couvrent une espèce précise, deux espèces différentes, deux plantes d’un élément donné et une belle plante ou mieux préparée dans un pot de vente. Les critères d’une même ligne sont cumulatifs : espèce, élément, qualité minimale, rareté, mutation, couleur et préparation en pot.

Une commande accepte jusqu’à quatre lignes, chacune demandant de une à dix plantes. Une unité ne remplit qu’une seule ligne. Si plusieurs lignes conviennent, une ligne plus spécifique est privilégiée avant une ligne générique. Une ligne déjà satisfaite ne consomme plus rien.

Les métadonnées `ItemTraits`, indexées par la clé de l’objet récolté, permettent de définir `RarityTag` et `MutationTags`, ainsi qu’une correspondance `PlantKey` pour une nouvelle variante. **La qualité et la rareté sont distinctes.** Aucun système de production de mutations n’est ajouté par cette fonctionnalité : les variantes doivent réellement exister et être obtenables dans le jeu. Les demandes automatiques de rareté ou de mutation apparaissent seulement si des variantes correspondantes sont configurées.

Le générateur vérifie que chaque ligne correspond à une plante du catalogue dont l’espèce possède une graine achetable. Cette vérification de catalogue ne remplace pas l’équilibrage des coûts, des déblocages ou des probabilités de production d’une future mutation. Les modèles configurés dans `Templates` complètent les demandes automatiques ; les modèles invalides ou sans correspondance sont ignorés.

## Réglages

Dans **Project Settings → Game → Botanicus Special Orders** :

| Réglage | Valeur initiale | Effet |
| --- | --- | --- |
| Enabled | Activé | Autorise les nouvelles commandes |
| Visitor Chance | 25 % | Probabilité par visiteur admis et éligible |
| Offer Interval Seconds | 120 s | Intervalle minimal entre deux offres |
| Maximum Concurrent Orders | 2 | Commandes en trajet, proposées ou acceptées simultanément |
| Acceptance Wait Seconds | 120 s | Attente au comptoir avant acceptation |
| Minimum Production Seconds | 360 s | Base minimale des délais automatiques ; certains modèles ajoutent une marge |
| Reward Multiplier | 1,5 | Multiplicateur appliqué aux prix de vente de référence des plantes demandées |
| Item Traits | Vide | Métadonnées des variantes rares ou mutées obtenables |
| Templates | Vide | Modèles supplémentaires, critères, niveau de magasin et délai |

Le premier tirage est possible après 30 secondes de temps de monde. Le trajet vers le comptoir est limité à 120 secondes et chaque comptoir accueille au plus deux commandes. Les nouvelles propositions sont filtrées en fin de journée pour laisser du temps avant la fermeture prévue ; une fermeture anticipée reste susceptible d’annuler une commande.

La satisfaction utilise le système de réputation existant : succès à 95, échec après acceptation à 30 et offre ignorée à 50. Avec la formule actuelle, cela donne respectivement **+6, −5 et −2 points**, dans les limites globales de réputation. Un trajet abandonné avant proposition ne pénalise pas le magasin. Le résultat reste visible huit secondes sur le HUD.

## Sonnette

- Source conservée dans `SourceArt/Audio/freesound_community-bell-98033.mp3`, identique au fichier fourni par le joueur.
- Son importé : `/Game/Botanicus/Audio/S_SpecialOrderBell`, mono, environ 5,355 secondes, sans boucle.
- Atténuation : `/Game/Botanicus/Audio/ATT_SpecialOrderBell`, spatialisée, rayon intérieur de 700 cm puis atténuation sur 2 300 cm supplémentaires.
- Volume initial : `SpecialOrderBellVolume = 0.8` dans les valeurs par défaut du visiteur, catégorie **Botanicus → Special Orders → Audio**. Le son peut aussi y être remplacé.
- Le serveur déclenche un RPC multicast fiable à la transition vers l’attente au comptoir. Le serveur dédié ne joue pas d’audio. Un garde empêche de rejouer cette sonnerie initiale.
- Après 60 secondes sans acceptation, le serveur déclenche un rappel, au plus une fois par minute. L’acceptation et le départ arrêtent les rappels. Une commande acceptée ne sonne plus pendant sa préparation. Le rappel ne prolonge pas la patience : avec l’attente initiale de 120 secondes, il sonne à l’arrivée puis à 60 secondes et part mécontent à 120 secondes sans réponse. Il ne sonne pas à nouveau à l’expiration. Les délais suivent le temps du monde.
- Une connexion tardive ne rejoue pas cet événement passé. Après rechargement d’une sauvegarde, un nouveau visiteur qui rejoint physiquement le comptoir pour reprendre une commande sonne à son arrivée.

Le son est référencé directement par la classe du visiteur et l’atténuation par le son pour être inclus dans la préparation du jeu. Le script `Tools/import_special_order_bell.py` permet de recréer les assets manquants dans Unreal à partir de la source conservée. Il préserve les paramètres d’atténuation d’un asset déjà existant.

## Réseau, HUD et sauvegarde

`UBotanicusSpecialOrderComponent` appartient au `GameState` répliqué. Le serveur choisit les commandes et valide l’acceptation, les critères, la distance, le stock, les échéances et les récompenses. Le HUD calcule le décompte à partir de l’horloge serveur ; le temps suit la vitesse du monde.

Le widget natif s’insère dans `SpecialOrdersSlot` si le Blueprint du HUD l’expose. Avec le Blueprint actuel, il est ajouté au même Canvas que `ObjectivesSlot`, reprend ses ancres et son alignement, puis se place 12 pixels sous sa hauteur configurée. Sa largeur de 370 pixels correspond à celle du panneau d’objectifs. Un ancien HUD sans `ObjectivesSlot` utilise une position de repli en haut à droite.

La sauvegarde du monde passe en version **32**. Elle conserve uniquement les commandes acceptées, leurs livraisons partielles et leur temps restant. Les références aux acteurs PNJ ne sont pas enregistrées. Le prochain visiteur éligible reprend une commande restaurée avant la génération de nouvelles offres. Le temps hors jeu n’est pas déduit ; dès le chargement, le compte à rebours reprend, y compris pendant le retour du client au comptoir. Les sauvegardes antérieures démarrent sans commande spéciale.

## Validation réalisée

Compilation **BotanicusEditor / Win64 / Development** réussie. Deux suites automatisées passent :

- `Botanicus.SpecialOrders.Criteria` : cumul des critères, qualité distincte de la rareté, mutation, priorité des lignes et absence de double comptage.
- `Botanicus.SpecialOrders.Lifecycle` : acceptation, deux personnages contribuant à la même commande, consommation du bon inventaire, sauvegarde en mémoire, récompense et pénalité uniques, refus d’une livraison tardive, génération depuis le catalogue réel, conservation d’un trajet PNJ non rectiligne du parking à la caisse, arrivée au comptoir, sonnette initiale unique, rappel après une minute sans acceptation, arrêt des rappels après acceptation ou expiration, exclusion de la file de caisse classique, annulation si le comptoir disparaît et compatibilité structurelle du HUD existant.

Exécution depuis le dossier `Botanicus` contenant le `.uproject`, après compilation :

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
    '.\Botanicus.uproject' -run=BotanicusSpecialOrderTest `
    -unattended -nop4 -nosplash -nosound -nullrhi -NoShaderCompile -stdout
```

Les tests utilisent un monde transitoire et une sérialisation en mémoire, sans charger ni modifier la sauvegarde du joueur. Les deux personnages de test partagent ce monde : cela ne constitue pas un essai entre deux clients réseau. Deux avertissements de matériau proviennent de la reconstruction visuelle de la zone de caisse existante ; aucune erreur de test.

À vérifier dans une session PIE avec deux clients : rendu et encombrement du HUD, accessibilité physique du comptoir dans le magasin aménagé, synchronisation et connexion tardive, puis écoute de la sonnette depuis plusieurs distances. Les tests sans rendu et sans son ne valident pas ces aspects perceptifs. Une compilation Shipping et un packaging complet n’ont pas été exécutés.
