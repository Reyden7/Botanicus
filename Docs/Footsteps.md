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
