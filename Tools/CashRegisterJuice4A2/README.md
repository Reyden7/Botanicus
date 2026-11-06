# Validation caisse 4A.2

Exécuter depuis PowerShell les scripts `RunValidation.ps1 -Scenario ValidateCheckout`, `ValidateDisconnect`, `CaptureCheckout` ou `ProfileCheckout`.

Les scénarios démarrent un Editor séparé, quatre mondes PIE (listen server et trois clients), une copie des sauvegardes dans `Botanicus/Saved/Diagnostics/CashRegisterJuice4A2/isolated_user`, et désactivent l'autosauvegarde de l'éditeur. Les sauvegardes de jeu restent actives dans ce profil isolé. `-KeepSaves` conserve ce profil au lieu de recopier les sauvegardes d'origine. Aucun test ne doit être exécuté sur un profil utilisateur réel.

Les helpers C++ refusent d'agir hors PIE, hors du profil `isolated_user` ou sans `-BotanicusCashValidation`. Ils ne sont pas actifs dans le jeu compilé. Les actions sont programmées dans le Tick du monde concerné : les lancer directement depuis un callback Slate ne reproduit pas correctement l'acheminement des RPC en PIE sous un seul processus.

`ValidateCheckout` prépare un présentoir et un client par les fonctions natives, ajuste uniquement le prix du catalogue en mémoire pour les cas 30/50/100/57/10007, puis utilise les entrées natives E/clic gauche et les RPC réels pour les demandes concurrentes et invalides. Il redémarre réellement PIE pour tester les sauvegardes et demande une arrivée tardive. Le contrôle du commerce clandestin utilise les paramètres de commande existants et son RPC natif ; ses sources ne sont pas modifiées. Ce sont des injections d'entrées, pas des clics physiques à la souris.

`ValidateDisconnect` ferme réellement la connexion du porteur distant, puis vérifie la restitution et une reconnexion tardive. Après une perte de connexion, Unreal peut conserver un monde revenu en standalone : le test compte les mondes encore connectés et exclut ce monde.

`ProfileCheckout` compile Editor et Game avant de démarrer Unreal. Il mesure huit blocs alternés OFF/ON/ON/OFF/ON/OFF/OFF/ON avec quatre viewports, 15 s de chauffe, 6 s de stabilisation et 24 s de mesure par bloc, une transaction native de 1000 crédits toutes les 6 s et un arrosoir sélectionné. Aucune capture n'est effectuée pendant les mesures. Le coût des fixtures et des quatre mondes PIE est inclus. Cette comparaison n'établit pas un gain face à l'ancienne caisse.

`RunRegressionSuite.ps1` lance les suites existantes 3A, croissance, équipements, 4A.1, Noctiflores, les deux tests audio et le profilage, sans modifier leurs scripts. Les résultats JSON doivent être frais, `passed=true` et sans erreur. `Smoke` et `SmokeClient` servent seulement au diagnostic initial ; leur résultat `passed` vérifie l'émission des actions, pas l'ensemble des invariants financiers.

`CaptureCheckout` écrit des images BGRA brutes du viewport dans le profil isolé, avec les temps réels et l'état courant. Le HUD Slate n'est pas inclus et aucune piste audio n'est capturée. L'encodage éventuel doit respecter ces temps, sans interpolation présentée comme des images du jeu.

`PrepareAudio.py` nécessite Python, Pillow/imageio-ffmpeg pour les utilitaires de médias et les quatre MP3 fournis dans `C:/Users/quent/Documents/Botanicus/sons`. Il écrit les WAV et un manifeste dans `Botanicus/SourceArt/Audio/Checkout`, sans modifier les MP3. `BuildAssets.py` s'exécute dans Python Unreal, crée uniquement les assets absents et ajoute quatre entrées au catalogue existant. Réserver préalablement `Botanicus/Content/Botanicus/Audio/DA_BotanicusAudio.uasset` avec Git LFS ; ne pas forcer sa lecture seule. Ne pas sauvegarder les niveaux pendant cette génération.
