# Appareils climatiques locaux

Quatre appareils sont disponibles comme équipements dans le catalogue de
jardinage. Ils sont livrés en colis et se placent comme des meubles devant le
joueur, sans passer par le mode de construction top-down.

| Appareil | Prix | Rayon | Effet maximal au centre |
|---|---:|---:|---:|
| Chauffage | 450 crédits | 5 m | +12 °C |
| Refroidisseur | 550 crédits | 5 m | -12 °C |
| Lampe horticole | 350 crédits | 5 m | +70 % de luminosité |
| Brumisateur | 400 crédits | 5 m | +45 % d'humidité |

L'effet diminue linéairement avec la distance et atteint zéro au bord du
rayon. Plusieurs appareils se cumulent. Après calcul, la température est
limitée entre -50 et 100 °C, et l'humidité et la luminosité entre 0 et 100 %.

La zone est affichée sous forme d'anneau coloré pendant le placement et lorsque
le mode meubles (touche B) est actif. Un appareil porté ou en cours de placement
n'applique aucun effet.

Hors mode meubles, l'interaction ouvre le WBP `WBP_ClimateDeviceControl`. Son
bouton central allume ou éteint l'appareil et son slider inférieur règle sa
puissance. Pour le chauffage, le slider utilise directement une plage de
0 à +12 °C avec un pas de 0,5 °C. Les autres appareils restent affichés en
pourcentage. Le bouton supérieur droit et Échap ferment le panneau.
En mode meubles, ce panneau est désactivé : l'interaction sert uniquement à
sélectionner l'appareil pour le déplacer avec le même aperçu, la même rotation
et les mêmes contrôles que les autres meubles.
Son état, son rayon et sa puissance sont répliqués en multijoueur et conservés
dans la sauvegarde version 29.

Les réglages par instance sont exposés sur `ABotanicusClimateDeviceActor` :
`InfluenceRadius`, `MaximumEffectStrength` et `bEnabled`. Toutes les influences
sont composées dans `UBotanicusEnvironmentSubsystem::GetEnvironmentAtLocation`
avant l'évaluation des besoins de chaque plante.
