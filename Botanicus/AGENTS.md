# AGENTS.md — Botanicus (Unreal Engine 5.8)

## Mission
Tu es un assistant de développement intervenant sur **Botanicus**, un jeu de simulation de pépinière extraterrestre cosy/solarpunk sous **Unreal Engine 5.8**, jouable en coopération de **1 à 4 joueurs**.

**Priorité absolue : résoudre précisément la demande de l'utilisateur avec le minimum de contexte, d'appels d'outils, de modifications et de tokens, sans sacrifier la fiabilité.**

Communique **en français**, de façon directe, concise et technique. Évite les longs comptes rendus et la répétition des informations déjà connues.

## 1. Méthode de travail économe en tokens

1. **Commence par la demande précise.** Identifie les classes, assets, Blueprints ou systèmes vraisemblablement concernés. Ne commence jamais par un audit général du projet.
2. **Recherche ciblée.** Privilégie les chemins connus, puis `rg --files` ou `rg -n` avec des filtres de répertoire, d'extension et de motif. Limite les résultats (`-m`, recherche de noms) et n'affiche que les extraits nécessaires.
3. **Lis le minimum utile.** Inspecte les fonctions et dépendances directes, pas des dizaines de fichiers complets. Élargis seulement si l'enquête l'exige.
4. **Pas de boucles de vérification inutiles.** Après une modification, contrôle le diff et les points réellement touchés. Ne refais pas une analyse complète si rien ne le justifie.
5. **Réutilise les connaissances acquises pendant la tâche.** Ne relance pas les mêmes recherches et ne redemande pas des informations déjà présentes.
6. **Évite les sorties volumineuses.** N'affiche pas le contenu intégral d'un fichier, les logs complets ou de grands inventaires d'assets si une sélection suffit.
7. **Reste sur le périmètre demandé.** Pas de refactorisation opportuniste, renommage massif, « nettoyage » global ni ajout de fonctionnalités non sollicitées.
8. **Si un point est incertain**, inspecte uniquement ce qui permet de lever le doute. Ne présume pas qu'un fichier ou un composant existe.

### Répertoires à ignorer lors des recherches ordinaires

Sauf nécessité explicite, exclure :

- `Binaries/`, `Intermediate/`, `DerivedDataCache/`, `Saved/`, `.git/`
- les caches, fichiers générés, builds, logs historiques et dépendances tierces

Ne pas ignorer `Content/` lorsqu'un asset est concerné, mais éviter d'en parcourir toute l'arborescence sans cible.

## 2. Modifications : petites, sûres et réversibles

- Préférer un **diff minimal**, localisé et lisible.
- Respecter les conventions C++ et Unreal déjà présentes ; ne pas introduire une nouvelle architecture sans nécessité.
- Modifier uniquement les fichiers indispensables à la tâche.
- Conserver les noms, chemins et références des assets existants autant que possible.
- Ne pas supprimer, remplacer ou déplacer des Blueprints, meshes, matériaux, animations, textures ou niveaux sans demande explicite.
- Éviter toute modification des réglages globaux du projet, des plugins et de la configuration moteur si une correction locale suffit.
- Ne pas créer de commit, push, branche, migration ou déploiement sans demande explicite.
- Ne pas modifier ce fichier `AGENTS.md` de sa propre initiative.

## 3. Spécificités d'Unreal Engine 5.8

- Utiliser les API et pratiques adaptées à la **version du moteur réellement présente dans le projet** ; ne pas inventer de signatures d'API.
- Distinguer correctement **C++**, **Blueprint**, **Skeletal Mesh**, **Static Mesh**, **Components**, **Materials**, **Niagara**, **UMG** et **Data Assets**.
- Quand une fonctionnalité doit être réglable dans un Blueprint enfant ou dans l'éditeur, exposer uniquement les paramètres pertinents avec `UPROPERTY`/`UFUNCTION` adaptés.
- Éviter les appels lourds ou recherches répétées dans `Tick` lorsque des événements, timers ou caches suffisent.
- Respecter les cycles de vie UE (constructeur, `BeginPlay`, initialisation des composants, réplication, destruction) et les références sûres aux `UObject`.
- Tenir compte des interactions entre code C++ et assets Blueprint : une modification de C++ peut exiger une compilation et une vérification du Blueprint, mais **pas** un audit exhaustif de tous les assets.

## 4. Multijoueur : ne pas casser le réseau

Botanicus est coopératif (1 à 4 joueurs). Pour les mécaniques partagées :

- Respecter l'**autorité du serveur** sur les états de jeu et les transactions.
- Vérifier, lorsque pertinent, la réplication des états, RPC, interactions et changements d'ownership.
- Séparer les effets purement cosmétiques des données faisant autorité.
- Ne pas introduire de logique reposant uniquement sur le client local si l'action doit être visible ou validée par les autres joueurs.
- Limiter les tests réseau au **scénario modifié** ; réserver le test PIE à 4 joueurs aux changements qui touchent réellement les interactions multijoueurs ou à une demande explicite.

## 5. Systèmes de gameplay à préserver

Éviter les régressions sur les systèmes existants, notamment :

- plantation, terreau, jardinières, pots, arrosage et croissance des plantes ;
- exigences environnementales, maladies et parasites ;
- préparation, présentoirs, étagères murales, vente, scanette, caisse et paiements ;
- inventaire, déplacement et placement d'objets ;
- cycle jour/nuit, éclairage, sons et effets Niagara ;
- zone clandestine nocturne, Noctiflores et commandes ;
- interfaces, icônes, panneaux d'information et carnet botaniste.

Cette liste est un **contexte**, pas une invitation à examiner ou retester tous ces systèmes à chaque tâche.

## 6. Vérifications : proportionnées au changement

Choisis le niveau de vérification **le moins coûteux qui permet d'obtenir une confiance raisonnable** :

| Type de modification | Vérification attendue |
| --- | --- |
| Valeur, paramètre, texte ou réglage visuel | Contrôle ciblé du diff ; indiquer le contrôle visuel à faire dans l'éditeur si nécessaire |
| Logique locale C++ | Inspection du code modifié et de ses appels directs ; compilation ciblée si disponible et pertinente |
| API ou composant utilisé par un Blueprint | Vérification des points d'intégration concernés ; signaler ce qui reste à valider dans l'éditeur |
| Réplication, RPC, vente, inventaire partagé | Contrôles serveur/client ciblés sur le scénario affecté |
| Changement structurel ou risque de régression élevé | Vérifications supplémentaires **justifiées** par le changement, sans suite exhaustive par défaut |

- **Ne lance jamais automatiquement** un packaging, une compilation complète de toutes les configurations, une suite intégrale de tests, un benchmark général ou une session PIE multi-joueurs complète.
- Si une compilation est nécessaire, privilégie la cible et la configuration les plus proches de la modification ; ne la relance pas sans nouvelle raison.
- Si l'éditeur Unreal, les assets binaires ou l'environnement d'exécution ne sont pas accessibles, **dis-le clairement**. Ne prétends pas avoir validé un Blueprint ou un test PIE.
- Ne corrige pas des problèmes anciens ou hors périmètre sans accord de l'utilisateur ; signale-les brièvement s'ils bloquent la tâche.

## 7. Débogage

1. Partir du **symptôme décrit** et des étapes de reproduction fournies.
2. Formuler une hypothèse principale, puis inspecter le code ou l'asset directement associé.
3. Corriger la cause probable avec la plus petite modification fiable.
4. Vérifier le scénario initial et les dépendances immédiates.
5. Si la piste échoue, élargir l'analyse progressivement ; ne pas multiplier des explorations parallèles sans justification.

Pour un problème d'animation Blender → Unreal, d'asset ou de matériau, commencer par les paramètres d'import, la hiérarchie et les références **de l'élément concerné uniquement**.

## 8. Format de réponse

Pendant l'exécution, rester bref. À la fin, répondre avec ce format concis :

**Modifié :** fichiers et comportement modifiés (1 à 4 lignes).

**Vérifié :** contrôles réellement effectués, sans exagérer.

**À tester dans Unreal :** uniquement si une vérification manuelle est nécessaire, avec des étapes courtes.

**Blocage :** uniquement si quelque chose empêche de terminer.

Ne pas joindre de rapport d'audit détaillé, de long résumé de toutes les recherches ni de longues suggestions non demandées. **La qualité de la correction compte plus que la quantité d'opérations ou de texte produit.**
