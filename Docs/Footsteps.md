# Footsteps — ancien système supprimé (08/11/2026)

L’ancien composant C++, ses déclencheurs/traces, son catalogue et ses 24 SoundWaves ont été retirés.
L’atténuation, la concurrence, les outils d’import/test et le réglage de cook spécifiques ont été supprimés.
Aucune animation de locomotion ni autre son n’a été modifié. Aucun nouveau système de pas n’est activé.

Conservés pour le futur système manuel :
- `/Game/Botanicus/Audio/Footsteps/PhysicalMaterials/PM_Botanicus_Floor` — SurfaceType1, BotanicusFloor ; affecté à M_Floor_Terracotta_WorldTiled1.
- `/Game/Botanicus/Audio/Footsteps/PhysicalMaterials/PM_Botanicus_Concrete` — SurfaceType2, BotanicusConcrete ; affecté à M_beton_Inst.
- `/Game/Botanicus/Audio/Footsteps/PhysicalMaterials/PM_Botanicus_Wood` — SurfaceType3, BotanicusWood ; affecté à M_Floor_Terracotta_WorldWood_Inst.
Les affectations des trois matériaux de sol, leurs paramètres physiques et les verrous LFS restent inchangés.

Les 34 WAV originaux restent intacts dans `C:/Users/quent/Documents/Botanicus/sons/bruit de pas/FootstepsSounds/Footsteps`.
Source fournie : https://www.youtube.com/watch?v=HrQiArch39c&t=63s. Licence commerciale et redistribution toujours non confirmées.
Les anciens diagnostics sous Saved ne sont pas exécutés et ne font pas partie du système actif.

Validation ciblée : marche, course, transition carrelage→bois et arrêt en PIE hôte/client ; zéro ancien composant/voix de pas, ambiances actives et autres propriétés du catalogue inchangées. Aucun test historique ni benchmark.
Compilation finale BotanicusEditor UE 5.8.2 réussie (module 4122), après retrait du banc temporaire.

## Raccordement direct aux ressources utilisateur — 08/11/2026

`BP_BotanicusBuilderCharacter` porte un seul composant `BotanicusSurfaceContactAudio` : association directe ConcretMat→ConcretSoundEffet, WoodMat→WoodSoundEffet, WaterMat→WaterSoundEffect ; inconnu→ConcretSoundEffet. Aucun catalogue, asset audio, import ou RPC supplémentaire.
PIE : corps SK_Mannequin/ABP_EBS_BuilderCharacter ; Manny de première personne copie la pose via ABP_FP_Copy. ABP_Unarmed est présent mais non actif ; les contacts nommés demandés ne sont pas émis par cette locomotion. Repli autorisé : OnCharacterMovementUpdated, aucun Tick ajouté, foulées 80/200/245 cm à 150/600/900 cm/s. Le mode AnimationContact(bool RightFoot) reste désactivé ; il nécessite un véritable câblage des notifies du corps avant activation, sans cumuler les deux déclencheurs.
Les trois cues existants ont déjà un Random sans remplacement vers 5/5/7 WavePlayers non bouclés. Aucun cue/WAV/Physical Material modifié ; pitch de lecture 0,98–1,02. Voix locales privées, distantes spatialisées avec fondu 150–1400 cm ; concurrence transitoire 4 voix/joueur, nettoyage à EndPlay.
Les sources conservées ont des attaques marquées variables (environ 26–281 ms) et des transitoires secondaires : un seul WAV est joué par contact, ce qui ne garantit pas un seul impact audible dans le fichier. Aucun montage, découpage ou réimport réalisé.
Tests ciblés PIE hôte/client : trois associations physiques, transition, marche lente/normale/course, arrêt/saut, alternance et absence de doublons validés. Intervalles moyens ≈534/333/273 ms selon allure, écarts-types 3,5–7,6 ms. Timestamps et mesures sous Saved/Diagnostics/FootContact. Lecture/spatialisation vérifiées ; écoute subjective et capture audio automatique non certifiées (API Python de capture indisponible sous le nom testé).
Compilation finale BotanicusEditor UE 5.8.2 réussie (module 4126), après retrait du contrôle temporaire de course. Anciens systèmes et suites non relancés ; map, sons, matériaux et sauvegardes préservés. Aucun commit.
