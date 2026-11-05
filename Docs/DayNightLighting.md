# Éclairage et cycle visuel Botanicus

Implémentation du 5 novembre 2026, Unreal Engine 5.8.2. Carte :
`/Game/Botanicus/Maps/Lvl_BuildingTest`.

## Architecture

Un `BotanicusAmbienceDirector` est placé dans la carte, sans réplication.
Chaque monde de jeu rendu possède sa copie. Il lit `DayTimeMinutes`, le numéro
de jour et l’activation clandestine du `BotanicusGameState` existant. Aucun
second calendrier, aucune avance autonome et aucun RPC de lumière ne sont ajoutés.

Le lissage suit les échantillons reçus sur un cercle de 1440 minutes. Le premier
échantillon et les sauts supérieurs à 15 minutes sont appliqués immédiatement.
Le temps de réponse est 0,12 seconde de jeu et respecte la dilatation existante.
L’absence de GameState est tolérée jusqu’à son arrivée. Le calendrier commercial
et les horaires de gameplay restent inchangés. Les profils de lever/coucher
proviennent de `BotanicusSeasonSettings` ; ils doivent être identiques dans les
configurations distribuées aux clients et au serveur.

Le directeur actualise la DirectionalLight, la SkyLight, le brouillard et les
sources explicitement liées. Son unique Tick sert à lisser le rendu. Il ne
parcourt pas tous les acteurs, ne crée pas de composant par frame, et son Tick
est interdit sur serveur dédié. `BeginPlay` y désactive aussi le contrôleur.

Le Sky Sphere existant est conservé. Son EventGraph exporté par Unreal est vide :
son graphe de construction ne constitue pas un pilote de temps de jeu.
Le directeur appelle sa fonction existante `UpdateSunDirection` à au plus 10 Hz
réels, avec mise à jour immédiate à l’initialisation ou au changement explicite
d’heure. L’adaptateur gère les nombres Blueprint `float` et `double` d’UE5.
Ni Blueprint du moteur ni matériau du moteur ne sont modifiés.

La SkyLight conserve son acteur, mais utilise désormais la TextureCube existante
`/Engine/MapTemplates/Sky/DaylightAmbientCubemap`. Intensité et teinte varient
localement. **Aucune capture de scène, recapture périodique ou capture temps réel**
n’est effectuée par le directeur. La convolution initiale du cubemap reste un
travail normal du moteur. Compromis : le détail de la réflexion du ciel ne tourne
pas avec le disque solaire. La lumière ambiante et son niveau nocturne restent
cohérents et déterministes entre les pairs. Une pile SkyAtmosphere avec capture
temps réel serait un chantier distinct à profiler.

## Corrections et intégration du niveau

Le relevé Unreal a confirmé : soleil mobile d’intensité 10,5 et rotation fixe,
SkyLight mobile en Captured Scene sans capture temps réel, Sky Sphere moteur,
AtmosphericFog hérité et ExponentialHeightFog avec volumétrique déjà activé.
Aucun SkyAtmosphere n’est utilisé dans cette carte inspectée.

Le PostProcess global, actif, unbound, priorité 1 et poids 1, forçait
**Min EV100 = 9 et Max EV100 = 1**. Cet intervalle inversé est corrigé. Il explique
un réglage d’exposition incohérent ; l’éclairage et les matériaux participent
également au résultat. La disparition du ciel diurne repose sur le pilotage
effectif du soleil et du Sky Sphere, pas sur une correction de l’horloge.

Les 258 acteurs d’origine sont conservés. Le niveau comporte un directeur et
quatre RectLights supplémentaires, sans aucun mesh. Trois sources éclairent
vente, caisse et préparation ; une source bleutée sert l’arrière clandestin.
Leurs repères sont dans le dossier Outliner `Render/BotanicusAmbience`.
Les meshes, Blueprints existants, acteurs de zones et équipements ne sont pas
remplacés. L’AtmosphericFog et la configuration volumétrique existante sont conservés.

Le multiplicateur d’éclairage indirect du PostProcess revient de 2 à 1 ; les
sources locales portent la lisibilité intérieure. Le LUT et les réglages de style
existants sont conservés. Les lumières décoratives ne sont jamais enregistrées
comme appareils climatiques et n’affectent pas la luminosité de simulation des plantes.

## Paramètres dans Details

Sélectionner `BotanicusAmbienceDirector` dans `Lvl_BuildingTest`.

| Groupe | Paramètres et réglage retenu |
|---|---|
| Bindings | Sun, SkyLight, LegacySkySphere, HeightFog, ExposureVolume : références explicites aux acteurs existants |
| Time | réponse 0,12 s ; seuil de saut 15 min |
| Sun | intensité jour 8 ; azimut 38° ; élévation maximale 60° ; teintes horizon/midi ; courbe d’intensité optionnelle |
| Sky | jour 0,75 ; nuit 0,15 ; teinte nocturne bleue ; soleil du matériau 30 ; étoiles 0,25 ; intervalle matériau 0,1 s réelle |
| Fog | densités jour 0,008 / nuit 0,012 ; couleurs jour/nuit |
| Exposure | gestion activée ; EV100 −1 à 12 ; compensation 0 ; vitesses 3 / 1,5 ; contrastes Local Exposure 0,8 / 0,8 |
| Interior | références explicites ; intensités jour 900 / nuit 700 ; teinte chaude |
| Clandestine | références explicites ; intensité 350 ; teinte bleue ; activation lue du GameState, sans modification d’horaire |

Les RectLights utilisent des lumens, source 120 × 100 cm, rayons de 450 à 650 cm,
distance de rendu 2200 cm et fondu de 400 cm. Les trois sources intérieures
projettent des ombres ; la petite source clandestine ne le fait pas. Modifier
leurs positions et rayons dans leurs propres composants si l’aménagement évolue.
Les lumières lointaines sont volontairement coupées ; une vue générale éloignée
n’est pas un bon indicateur de la lecture au niveau du joueur.

`Apply Exposure Settings` réapplique les paramètres d’exposition. En Python Editor,
`director.preview_at_hour(22.0, 1, True)` prévisualise la nuit clandestine sans
changer le GameState et sans sauvegarde de partie. Cette fonction ne s’applique
pas dans un monde de jeu. Après prévisualisation, sauvegarder explicitement le
niveau si l’état de prévisualisation doit être conservé. En jeu, l’heure partagée
remplace la prévisualisation sauvegardée.

## Réinstallation et édition sûre

`Tools/ConfigureDayNightLighting.py` configure la carte via les API Unreal.
Compiler d’abord `BotanicusEditor`, vérifier l’état Git et prendre le verrou LFS
de `Botanicus/Content/Botanicus/Maps/Lvl_BuildingTest.umap`. Aucun verrou d’autrui
ne doit être forcé. Fermer les autres processus qui chargent cette carte avant
de la sauvegarder : Windows interdit le remplacement d’un package ouvert ailleurs.

Le script réutilise les labels et références existants, crée seulement ce qui
manque, ne supprime aucun acteur et sauvegarde uniquement cette carte. Il ne faut
pas le lancer à l’aveugle sur une autre carte. Il conserve les réglages artistiques
d’un directeur déjà présent. Pour un nouveau directeur, les valeurs natives
correspondent aux valeurs du tableau. Une sauvegarde initiale du niveau a été
conservée hors dépôt dans le dossier de preuves de cette conversation.

## Validation et limites

Les tests `Botanicus.Lighting.SeasonalPhases`, `ClockSampling` et
`GameStateAndLegacySky` couvrent les quatre saisons, 02/06/12/18/22 h, les limites
lever/coucher, le passage à minuit, un saut d’heure, l’absence d’avance indépendante,
le premier échantillon après absence de GameState, le vrai Blueprint moteur,
l’extinction solaire nocturne, l’exposition et l’interdiction de Tick dédié.
Le test d’intégration utilise une carte temporaire sans sauvegarde de jeu.

Les captures Editor DX12/SM6 avec la pile Lumen du projet vérifient les cinq heures
et des vues fixes intérieure/extérieure. Elles ne constituent ni un parcours
jouable complet ni un benchmark. Attendre l’adaptation après un saut jour/nuit avant
de comparer les images ; le jeu normal progresse beaucoup plus lentement.

À compléter avant validation artistique finale : parcours continu entrée/fenêtres/
préparation/arrière/parking, boutique peuplée avec luminaires achetés, différentes
scalabilities et machine cible. Le coût des trois nouvelles sources avec ombres
doit être profilé en GPU. Aucun gain de FPS ou budget GPU chiffré n’est revendiqué.
Les fenêtres, portes et surfaces translucides doivent encore être contrôlées dans
une partie peuplée. La capture cubemap fixe constitue le compromis décrit plus haut.

La compilation et les résultats du banc réseau de cette session sont consignés
dans le compte rendu externe livré avec ce fichier. Aucun packaging Shipping,
session Steam entre machines ou exécutable de serveur dédié n’est certifié par
ces tests. Les sons, HUD, PNJ, Niagara, plantes et fichiers de sauvegarde n’ont
pas été modifiés par ce chantier.
