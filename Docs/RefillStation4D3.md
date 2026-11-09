# Remplissage interactif — 4D.3

Source **illimitée**, conservée sur demande. Débit serveur inchangé : **0,40 arrosoir/s**.
Aucun stock fini ni état « réserve vide » ajouté.

- Placer l’arrosoir avec **A**, puis viser la réserve ou le point du robinet et appuyer sur **E**.
- Un seul arrosoir est associé à chaque réserve ; son eau existante est conservée.
- E ouvre/ferme ; plein, retrait, destruction et sortie de zone ferment automatiquement.
- Le maintien du clic gauche et ses anciens RPC ne remplissent plus l’arrosoir.
- Récupération normale, POP, arrosage des plantes et sauvegarde de l’eau réutilisés.
- Au chargement, réassociation de l’arrosoir posé, robinet fermé ; aucune ouverture sonore rejouée.
- État autoritaire répliqué, effets et zone locaux ; aucune transformation cosmétique répliquée par frame.

Réglages de l’acteur : `ZoneOffset` (95,0,0) pour le blockout, `ZoneHalfSize` (55,45,65),
`ZoneDisplayDistance` (500 cm), composant `FaucetOutlet` (95,0,130). L’ancien `CanDockRotation` est conservé pour compatibilité ; le recentrage garde le Yaw choisi.
Le mesh catalogue `cuve_d_eau` utilise `FillZoneLocalOffset` (0,105,0), calé devant son bec (+Y local), transformé avec le mesh.
Détection et rectangle partagent le composant `FillZone` ; `FaucetOutlet` suit leur XY, à sa hauteur existante.
Z de la zone suit le bas du mesh. Recentrage facultatif uniquement si sol de même hauteur et espace libre.
`BP_Item_WateringCan` hérite du point `WateringCanFillTarget`, enfant du mesh, position (8,0,16).
La réserve conserve le mesh défini par `DA_WaterReserve` ; aucun nouveau modèle ni asset Niagara.
La zone et le repère de visée ne bloquent ni les personnages ni les traces de placement.

Audio : `SW_FaucetOpen`, conversion intégrale du MP3 fourni (2,01646 s décodées, sans découpe ni changement de pitch),
`SW_ReserveRefillLoop` (cœur 1–8 s du second MP3, raccord circulaire 40 ms), `ATT_ReserveRefill` (60 + 700 cm).
Catalogue `DA_BotanicusAudio` enrichi ; ses 18 anciens feedbacks et `SW_WaterLoop` conservés.
Originaux intacts ; provenance et conversions dans `Botanicus/SourceArt/Audio/Refill/manifest.json`.

Validation ciblée : Editor et jeu Win64 Development UE 5.8.2 compilés ; PIE hôte/client,
placement A et POP, ouverture depuis le client, fermeture, plein, retrait par un autre joueur,
déplacement/destruction, hors zone, emplacement unique, ancien clic/RPC inertes, arrosage d’une plante,
connexion tardive et reprise d’une sauvegarde isolée (46 % conservés, robinet fermé).
Débit vérifié sur les horodatages de transfert : 0,400000 arrosoir/s ; une voix d’écoulement par joueur,
une lecture par ouverture, arrêt des deux Niagara et de la boucle à la fermeture.
Pas de session à quatre joueurs, de benchmark ni de suites historiques ; réserve vide non applicable.
Lecture automatisée du texte privé UMG indisponible ; widget existant visible et API contextuelle réutilisée.

Contrôle opt-in `Editor/BotanicusRefillValidation.*`, limité au PIE avec `-BotanicusRefillValidation`
et un profil `/isolated_user/`. Aucun changement des sauvegardes normales.
Renommage du seul helper `TestWorld` du diagnostic Nuit pour résoudre une collision Unity révélée à la compilation.
Aucun commit, aucun changement des verrous LFS ; carte et deux Blueprints inchangés.


Correction de position : compilation Editor UE 5.8.2 et PIE ciblé sur la classe native du catalogue validés (A, E, écoulement, rotation 90° et déplacement). Les Data Assets et la sauvegarde originale sont inchangés.

Arrosoir au sol : calibration de la base du mesh Solar à Z = -32,23605 cm, dégagement serveur 0,25 cm ; pose droite commune à l’hologramme et au serveur. La pose en main du Blueprint est conservée. Recalage unique au chargement des anciennes poses au sol, sans changement du format de sauvegarde.
