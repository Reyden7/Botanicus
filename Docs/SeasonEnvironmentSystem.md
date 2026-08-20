# Saisons et environnement mondial

Le calendrier existant est la source unique du système de saisons. Une saison
dure par défaut 7 jours de jeu, dans l'ordre Printemps, Été, Automne, Hiver.
Après 28 jours, une nouvelle année commence.

La saison n'est pas enregistrée séparément : elle est recalculée depuis le
numéro du jour déjà présent dans la sauvegarde. Les anciennes sauvegardes sont
donc compatibles et reprennent automatiquement dans la saison correspondante.

## Valeurs par défaut

| Saison | Température nuit/jour | Humidité | Luminosité max. | Lever / coucher |
|---|---:|---:|---:|---:|
| Printemps | 10 / 22 °C | 65 % | 70 % | 06:00 / 20:00 |
| Été | 18 / 32 °C | 45 % | 95 % | 05:00 / 21:00 |
| Automne | 7 / 19 °C | 75 % | 55 % | 07:00 / 18:00 |
| Hiver | -2 / 8 °C | 60 % | 35 % | 08:00 / 17:00 |

Ces données sont modifiables dans **Project Settings > Game > Botanicus
Seasons**. La température suit un cycle continu avec un maximum vers 14 h. La
luminosité vaut zéro la nuit et suit une courbe progressive entre le lever et
le coucher du soleil.

## Utilisation en jeu

`ABotanicusGameState` calcule et réplique l'état extérieur courant : saison,
jour dans la saison, année, température, humidité et luminosité. Toutes ces
valeurs possèdent des getters Blueprint dans la catégorie `Botanicus|Season`.

`UBotanicusEnvironmentSubsystem::GetEnvironmentAtLocation` est le point
d'entrée unique pour connaître le climat à une position du monde. Les pots
simples et les jardinières l'utilisent, qu'ils se trouvent dedans ou dehors.
Les alertes et le widget de debug affichent donc également les conditions
extérieures.

La prochaine étape ajoutera les modificateurs locaux dans ce point d'entrée :
chauffage, refroidisseur, lampe et brumisateur pourront appliquer une
variation selon leur rayon et leur distance à la plante, par-dessus les valeurs
saisonnières.
