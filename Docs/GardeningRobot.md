# Robot jardinier N1 — 6A.1

Kit disponible dans le catalogue existant (Équipement / Entretien), clé `GardeningRobot_L1`, prix proposé **1 000 crédits** dans `DA_BotanicusItemCatalog`. Livraison et installation utilisent le carton, le mode meubles, l’hologramme et le POP existants.

## Réglages

Dans `/Game/Botanicus/blueprints/BP_Item_GardeningRobotBase_L1`, modifier **Config** : autonomie 6 h de jeu, recharge 1 h de jeu, retour préventif 10 %, capacité 1 réserve complète d’arrosoir, remplissage 0,4 réserve/s, arrosage 0,12 réserve/s maximum, vitesse 180 cm/s, rayon 2 000 cm, recherche toutes les 3 s, cible au milieu de la plage idéale, distance d’arrosage 110 cm, tolérance d’arrimage 12 cm, réessai après obstacle 30 s.

La station transmet cette configuration au robot. `RobotClass` permet une autre variante ; `Mesh` et `DockPoint` configurent la base. Dans `BP_Item_GardeningRobot_L1`, `Body` et `WaterOutlet` permettent de remplacer le blockout et d’ajuster la sortie d’eau. Aucun nouveau modèle définitif.

**Source retenue : alimentation intégrée illimitée**, désactivable par `bWaterSupplyEnabled`. Pas de nouvelle association à la réserve manuelle, dont le fonctionnement reste inchangé. Eau et charge progressent indépendamment, simultanément à l’arrimage. Couleurs de charge et de batterie pleine ajustables sur la station.

## Utilisation et calcul

E sur la station active/désactive le robot. E sur un robot actif l’arrête ; E sur le robot désactivé ou déchargé le prend. Regarder le robot porté et utiliser E pour le reposer. À moins de 150 cm de la station, le dépôt vise son point d’arrimage, avec contrôle de navigation, volume libre et balayage de collision. Aucune téléportation automatique de retour.

L’interface contextuelle existante donne batterie, eau et état ; la station indique le temps de charge restant en minutes de jeu.

Le serveur choisit uniquement les cultures sous leur minimum botanique, puis vise `Lerp(min,max,TargetWaterRatio)`. Conversion existante : une unité d’humidité requiert `0,12 / WaterAddedPerUse` réserve d’arrosoir. Chaque transfert est borné par la demande actualisée, le maximum de l’espèce, la réserve et le débit ; seule la quantité effective est débitée. L’entretien des cultures matures est conservé. Les jardinières clandestines ne sont pas inscrites dans le registre. Les espèces élémentaires du catalogue standard restent compatibles selon les règles du pot.

NavMesh et AIController Unreal, machine d’états serveur avec timer de 0,1 s, registre faible des cultures et réservation par emplacement. Recherche priorisée par déficit relatif puis proximité, avec au maximum huit candidats soumis aux calculs de navigation par recherche. Effets Niagara existants, cosmétiques, pilotés par les états répliqués ; aucun nouveau son.

Sauvegarde : identités base/robot, classe, position, batterie, eau, activation et état dans le snapshot de la station. Reprise avec réévaluation des besoins, sans recharge gratuite, simulation hors ligne ou création indépendante d’un second robot.

## Vérification ciblée

Compilation BotanicusEditor UE 5.8.2 ; sessions PIE hôte/client dans `Lvl_BuildingTest`, avec copies de sauvegarde isolées : commande/livraison/récupération (carton préparé ouvert pour cibler le contenu du kit), installation, besoins 50–70 et 20–40 %, plantes suffisamment/trop arrosées, apport partiel et retour/remplissage/reprise, arrosage manuel pendant l’approche, obstacle avec réessai différé, deuxième cycle après arrimage, pots multiples, exclusions, portage et recharge après panne, restauration sans doublon et concordance des ressources hôte/client. Calculs de décharge/recharge vérifiés avec minutes de jeu contrôlées ; aucune attente de six heures réelles.

Pas de test à quatre joueurs, de serveur dédié ou de build packagé. Le réessai espacé après échec de création sur un arrimage entièrement obstrué est protégé en code et compilé, mais ce cas particulier n’a pas été reproduit en PIE. Aucun audit général, benchmark global, ancienne suite de régression ou commit. L’outillage de validation temporaire est retiré du module ; traces et captures restent dans le dossier de travail de la session.
