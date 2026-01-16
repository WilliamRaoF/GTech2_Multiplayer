# UE 5.6 – Multijoueur et Réplication (Blueprint)

## Informations générales
- Moteur : Unreal Engine 5.6
- Langage : Blueprint uniquement

## Objectifs pédagogiques
À la fin du module, vous êtes capable de :
1. Expliquer le modèle client/serveur d’Unreal Engine.
2. Comprendre le principe server-authoritative et ses implications.
3. Utiliser la réplication (Actors et variables) correctement en Blueprint.
4. Utiliser les RPC (Run on Server, Multicast, Run on owning client) correctement.
5. Construire un prototype multijoueur jouable (déplacement, tir, dégâts, UI, respawn, score).
6. Déboguer les problèmes typiques de réseau (ownership, variables non répliquées, événements appelés du mauvais côté).

## Références (documentation)
- Unreal Engine Documentation – Networking and Multiplayer
- Unreal Engine Documentation – Actor Replication
- Unreal Engine Documentation – RPCs (Remote Procedure Calls)
- Epic Developer Community – Crash Course in Blueprint Replication

---

# Partie 1 – Théorie

## Fondations du multijoueur UE : modèle réseau et réplication

### 1.1 Définition : jeu multijoueur dans Unreal Engine
Dans Unreal Engine, le multijoueur repose sur un modèle réseau client/serveur :
- Un serveur exécute la simulation de jeu officielle.
- Les clients affichent une représentation de cette simulation.
- Les clients envoient des intentions au serveur (input, requêtes d’action).
- Le serveur valide ces intentions et réplique l’état aux clients.

Deux conséquences majeures :
1. La cohérence de l’état du jeu est garantie par le serveur.
2. Les clients ne doivent pas pouvoir imposer un rappel arbitraire du gameplay (anti-triche et robustesse).

### 1.2 Les modes de lancement réseau
Unreal Engine permet plusieurs modes utiles au développement.

#### 1.2.1 Standalone
- Un seul processus.
- Aucune couche réseau.
- Sert à développer rapidement des features non réseau.

#### 1.2.2 Listen Server
- Un joueur héberge la session.
- Le serveur est aussi un joueur (il a une fenêtre de jeu).
- Simple à mettre en place et adapté au prototypage.

Avantages :
- Test rapide dans l’éditeur.
- Facile à déboguer.

Inconvénients :
- Déséquilibre : l’hébergeur a souvent moins de latence.
- Le serveur dépend d’une machine joueur.

#### 1.2.3 Dedicated Server
- Serveur sans rendu (headless).
- Clients séparés.
- Mode de production recommandé pour les jeux compétitifs.

Avantages :
- Meilleure équité réseau.
- Scalabilité et contrôle.

Inconvénients :
- Plus complexe à compiler/configurer.
- Nécessite pipeline build et déploiement.

### 1.3 Le principe central : server-authoritative
Définition :
- Le serveur est la seule autorité pour les décisions gameplay.

Le client ne doit jamais décider :
- dégâts finaux
- positions finales “officielles” (hors prediction locale)
- ouverture d’une porte validée
- acquisition d’un loot
- score

Le client doit se limiter à :
- envoyer une intention
- afficher les résultats répliqués

Exemple :
- Client : “Je tire”
- Serveur : trace, détecte la collision, applique dégâts
- Serveur : réplique les PV mis à jour

### 1.4 Authority et rôle d’un Actor (notion simplifiée)
Les mêmes Blueprints existent en plusieurs copies :
- une copie serveur
- une copie sur chaque client

Notions pratiques en Blueprint :
- `Switch Has Authority` permet de différencier serveur vs client
- `Is Locally Controlled` permet de savoir si un Pawn est contrôlé par le client local

Interprétation :
- `HasAuthority == true` signifie que l’exécution se fait sur la copie serveur de l’Actor
- `Is Locally Controlled == true` signifie que cet Actor appartient au joueur local (client)

### 1.5 La réplication : état et événements
Unreal propose deux mécanismes complémentaires.

#### 1.5.1 Réplication d’état (State replication)
Objectif :
- synchroniser des données entre serveur et clients

On réplique :
- variables gameplay (PV, score, arme)
- état d’objets (porte ouverte/fermée)
- phase de match (préparation / en cours / fin)

En Blueprint :
- Actor : cocher `Replicates`
- éventuellement cocher `Replicate Movement`
- variable : `Replicated` ou `RepNotify`

#### 1.5.2 Réplication d’événements (RPC)
Objectif :
- exécuter une fonction sur une autre machine

Les RPC sont des “Custom Events” configurés dans Blueprint :
- Run on Server
- Run on owning Client
- Multicast

RPC typiques :
- Client -> Server : intention (tir, interaction)
- Server -> Client : événements personnalisés (UI spécifique joueur)
- Server -> All : effet global (explosion, son global)

### 1.6 RepNotify : mise à jour réactive côté client
Une variable RepNotify déclenche automatiquement un événement sur les clients lorsque la variable change.

Exemple :
- Health (RepNotify)
- OnRep_Health : mettre à jour la barre de vie, jouer un son local

Points importants :
- La variable change côté serveur.
- La réplication transporte la nouvelle valeur.
- OnRep s’exécute côté client lors de la réception.

Usage recommandé :
- UI
- effets cosmétiques
- animation/feedback

### 1.7 Erreurs classiques
1. Modifier le gameplay côté client :
   - mauvaise pratique : le client décrémente Health
2. Appeler un Multicast depuis un client :
   - incorrect : Multicast doit être déclenché par le serveur
3. Spawner des Actors côté client :
   - seuls les clients qui l’ont spawné les voient
4. Oublier `Replicates` sur un Actor :
   - variables et RPC semblent “ne pas marcher”
5. Confondre UI et gameplay :
   - UI locale, gameplay serveur

---

## Architecture réseau du framework UE et patterns gameplay

### 2.1 Les classes Unreal et leur rôle réseau
Comprendre où mettre les données et la logique est essentiel.

#### 2.1.1 GameMode
- existe uniquement sur le serveur
- non répliqué
- contient les règles de jeu
- gère respawn, conditions de victoire, bots éventuels

Conclusion :
- tout ce qui est “décision officielle” appartient au GameMode

#### 2.1.2 GameState
- existe sur serveur + clients
- répliqué à tous
- contient l’état global synchronisé :
  - temps restant
  - phase du match
  - score d’équipe

#### 2.1.3 PlayerState
- un par joueur
- répliqué à tous
- données du joueur :
  - nom
  - kills / deaths
  - équipe
  - ping

#### 2.1.4 PlayerController
- existe sur serveur et sur le client propriétaire
- gère :
  - input local
  - interface utilisateur
  - envoi RPC au serveur

#### 2.1.5 Pawn / Character
- représentation jouable
- souvent répliqué
- mouvement généralement pris en charge par le CharacterMovementComponent
- variables gameplay (PV, arme active)

#### 2.1.6 GameInstance
- locale à chaque client
- persistance hors match
- menus, paramètres

### 2.2 Ownership et RPC : règle qui explique 80% des bugs
Un client ne peut appeler un Server RPC que sur un Actor qu’il possède.

Conséquences :
- une porte est souvent possédée par le serveur
- un pickup est souvent possédé par le serveur
- un client ne peut pas appeler `Server_OpenDoor` sur la porte directement

Pattern correct :
- PlayerController ou Pawn reçoit l’input
- Server RPC “TryInteract”
- le serveur valide et modifie l’objet

### 2.3 Patterns de gameplay “standard” en réseau

#### 2.3.1 Pattern Tir / Impact
1. client : input fire
2. client -> serveur : Server_Fire
3. serveur : line trace, validation, application dégâts
4. serveur : modifie PV (variable répliquée)
5. serveur : multicast FX (optionnel)

#### 2.3.2 Pattern Interaction
1. client : input interact
2. client -> serveur : Server_TryInteract(Target)
3. serveur :
   - vérifie distance
   - vérifie line of sight
   - vérifie conditions
4. serveur : modifie état répliqué (ex: DoorOpen = true)

#### 2.3.3 Pattern Respawn
1. serveur détecte mort
2. GameMode gère respawn
3. serveur spawn nouveau Pawn
4. serveur possède le Pawn via PlayerController
5. clients voient le spawn via réplication

### 2.4 Séparation Gameplay vs Cosmétique
Gameplay :
- doit être serveur

Cosmétique :
- peut être local
- synchronisé par RepNotify ou multicast si nécessaire

Exemples cosmétiques :
- muzzle flash
- sound hit
- decal impact

---

# Partie 2 – Pratique
Objectif : produire un prototype jouable à 2 joueurs (Listen Server) dans l’éditeur.

## Organisation générale des Blueprints
Blueprints à créer :
- BP_GameMode_Multi
- BP_GameState_Multi (optionnel mais recommandé)
- BP_PlayerState_Multi
- BP_PlayerController_Multi
- BP_ThirdPersonCharacter_Multi (ou modification du template)
- WBP_HUD (UI de base)

---

## TP1  – Mise en place réseau et tests PIE

### 1. Création projet
- Nouveau projet : Third Person Template
- Blueprint uniquement
- Starter Content optionnel

### 2. Paramétrage PIE multijoueur
Dans l’éditeur :
- Play -> Advanced Settings
- Number of Players : 2
- Net Mode : Play as Listen Server

Objectif :
- observer deux fenêtres
- vérifier que deux Pawns existent

### 3. Préparer le Character
Dans le Character :
- cocher `Replicates`
- cocher `Replicate Movement`

Test attendu :
- si le serveur bouge, le client voit le mouvement
- si le client bouge, le serveur voit le mouvement

---

## TP2 – PV répliqués et HUD synchronisé

### 1. Variables PV
Dans Character :
- Float `MaxHealth` = 100
- Float `Health` = 100
Paramètres :
- Health : RepNotify

Créer automatiquement :
- Function/Event `OnRep_Health`

### 2. Création du widget HUD
Créer Widget Blueprint : `WBP_HUD`

Ajouter :
- ProgressBar : `PB_Health`
- Text : `TXT_Health` (optionnel)

### 3. Affichage HUD
Créer `BP_PlayerController_Multi` :
- Event BeginPlay :
  - `Create Widget (WBP_HUD)`
  - `Add to Viewport`
  - stocker une référence à l’HUD (variable `HUDRef`)

Attention :
- le HUD doit être créé uniquement sur le client local
- utiliser `Is Local Controller`

### 4. Mise à jour UI via RepNotify
Dans Character :
- OnRep_Health :
  - récupérer PlayerController local
  - récupérer HUDRef
  - mettre à jour :
    - PB_Health = Health / MaxHealth
    - TXT_Health = Health

Test attendu :
- quand serveur modifie Health, client voit UI changer
- quand client modifie Health (si autorisé serveur), serveur voit UI changer

---

## TP3 – Tir server-authoritative et dégâts

### 1. Input Fire
Project Settings -> Input :
- Action Mapping : Fire
- Key : Left Mouse Button

### 2. Appel serveur
Dans Character :
- InputAction Fire :
  - Branch `Is Locally Controlled`
  - appeler Custom Event `Server_Fire`

Créer `Server_Fire` :
- Custom Event
- Replication : Run on Server
- Reliable : true (dans un prototype ; en prod on ajuste)

### 3. Trace côté serveur
Dans `Server_Fire` :
- Start = Camera location ou muzzle socket
- End = Start + ForwardVector * Range
- `LineTraceByChannel`

Si Hit :
- récupérer Hit Actor
- vérifier si c’est un Character
- appliquer dégâts

### 4. Appliquer dégâts (serveur uniquement)
Créer Custom Event `Server_ApplyDamage` sur Character :
- Run on Server
- Entrée : DamageAmount

Implémentation :
- Switch Has Authority :
  - Health = Clamp(Health - DamageAmount, 0, MaxHealth)
  - si Health == 0 :
    - appeler Die

Important :
- seul le serveur modifie Health

### 5. Retour visuel (option)
Créer `Multicast_PlayShotFX` :
- Multicast
- jouer son tir, particules muzzle

Appelé uniquement depuis serveur (dans Server_Fire).

Test attendu :
- tirer depuis un client inflige bien des dégâts
- le client ne peut pas “forcer” le résultat

---

## TP4 – Mort, respawn, score PlayerState

### 1. PlayerState score
Créer `BP_PlayerState_Multi`
Variables :
- Int `Kills` (RepNotify)
- Int `Deaths` (Replicated)

### 2. Attribution de score
Lorsqu’un joueur tue un autre :
- serveur récupère PlayerState du tueur
- Kills += 1
- serveur récupère PlayerState de la victime
- Deaths += 1

Remarque :
- en Blueprint, on peut stocker le dernier instigateur (celui qui tire) dans le hit/damage

### 3. Mort et respawn via GameMode
Dans le Character :
- Event `Die` (serveur) :
  - désactiver collisions
  - désactiver input
  - demander respawn au GameMode

Dans `BP_GameMode_Multi` :
- fonction `Respawn(Controller)` :
  - choisir un PlayerStart
  - SpawnActor Character
  - Possess

Test attendu :
- la victime réapparaît
- son score de morts a augmenté
- le tueur a gagné un kill


# Partie 3 – Player Host / Menu Host-Join + gestion retour Menu (Listen Server)
Objectif : ajouter une **vraie boucle de jeu multi** :
- un **menu** avec boutons **HOST / JOIN**
- un joueur peut **héberger** (Listen Server)
- un joueur peut **rejoindre** via IP
- gestion **retour menu / quitter partie**
- gestion **Network Failure** (si le host quitte)

---

## Organisation générale des Blueprints (Partie 3)

Blueprints à créer :
- `BP_GameInstance_Multi`
- `WBP_MainMenu`
- `WBP_InGameMenu`

Maps :
- `Map_Menu`
- `Map_Game`

---

# TP5 — Menu Host / Join (Listen Server)

## 1. Création des maps

### 1.1 Créer `Map_Menu`
Créer une map menu simple :
- vide (ou avec décor)
- caméra (optionnel)

### 1.2 Vérifier `Map_Game`
Ta map multi doit contenir au minimum :
- `PlayerStart`
- ton `BP_GameMode_Multi` assigné dans World Settings

---

## 2. Création du GameInstance

### 2.1 Créer `BP_GameInstance_Multi`
- Blueprint Class : `GameInstance`
- Nom : `BP_GameInstance_Multi`

Ensuite dans :
**Project Settings → Maps & Modes**
- Game Instance Class = `BP_GameInstance_Multi`

---

## 3. Création du menu UI

### 3.1 Créer widget `WBP_MainMenu`
Créer Widget Blueprint : `WBP_MainMenu`

Ajouter dans le Designer :
- Bouton `BTN_Host`
- Bouton `BTN_Join`
- EditableTextBox `ETB_IP`
  - Hint : `127.0.0.1`
  - default value : `127.0.0.1`
- TextBlock `TXT_Status` (optionnel)

---

## 4. Affichage du menu dans `Map_Menu`

### 4.1 Level Blueprint de Map_Menu
Dans `Map_Menu` → Level Blueprint :

**Event BeginPlay**
- `Create Widget` (WBP_MainMenu)
- `Add To Viewport`
- `Get Player Controller`
- `Set Show Mouse Cursor(true)`
- `Set Input Mode UI Only`

> À ce stade, quand tu Play, tu dois arriver sur le menu.

---

## 5. Logique Host / Join (dans GameInstance)

> Pourquoi GameInstance ?
> - persiste entre les maps (menu → game)
> - endroit idéal pour gérer réseau/retour menu/erreurs

### 5.1 Fonctions / Events à créer dans BP_GameInstance_Multi

Créer ces Custom Events :
- `HostGame`
- `JoinGame` (Input : `IP` string)
- `ReturnToMenu`
- `HandleNetworkFailure` (plus tard)

---

## 6. Implémentation HOST (Listen Server)

### 6.1 Event `HostGame`
Dans `BP_GameInstance_Multi` :

`HostGame` :
- `Open Level`
  - Level Name : `Map_Game`
  - Options : `listen`


Résultat : le host lance la partie et devient serveur.

---

## 7. Implémentation JOIN (open IP)

### 7.1 Event `JoinGame(IP)`
Dans `BP_GameInstance_Multi` :

`JoinGame` :
- `Get First Local Player Controller`
- `Execute Console Command`
  - Command : `"open " + IP`

Exemples IP :
- `127.0.0.1` (tests sur la même machine)
- `192.168.X.X` (LAN)

---

## 8. Brancher les boutons du menu

### 8.1 BTN_Host → OnClicked
Dans `WBP_MainMenu` Graph :
- `Get Game Instance`
- `Cast to BP_GameInstance_Multi`
- Appeler `HostGame`

### 8.2 BTN_Join → OnClicked
- `ETB_IP → GetText`
- Convert Text -> String
- `Get Game Instance`
- Cast
- Appeler `JoinGame(IPString)`

---

# TP6 — Quitter une partie + retour menu propre

## 1. Créer un menu in-game (optionnel mais recommandé)

### 1.1 Widget `WBP_InGameMenu`
Créer Widget Blueprint : `WBP_InGameMenu`

Ajouter :
- Bouton `BTN_Resume`
- Bouton `BTN_QuitToMenu`

---

## 2. Afficher le menu InGame (dans PlayerController)

### 2.1 Modifier `BP_PlayerController_Multi`
Ajouter variables :
- `InGameMenuRef` (type `WBP_InGameMenu`)
- `bIsMenuOpen` (bool)

Créer un Input :
**Project Settings → Input**
- Action Mapping : `Menu`
- Key : `Escape`

Dans `BP_PlayerController_Multi` :

**InputAction Menu**
- Branch si `bIsMenuOpen`
  - si false → Open Menu
  - si true → Close Menu

#### Open Menu (fonction)
- `Create Widget (WBP_InGameMenu)` si `InGameMenuRef` est None
- `Add To Viewport`
- `Set Show Mouse Cursor(true)`
- `Set Input Mode UI Only`
- `bIsMenuOpen = true`

#### Close Menu
- `Remove From Parent(InGameMenuRef)`
- `Set Show Mouse Cursor(false)`
- `Set Input Mode Game Only`
- `bIsMenuOpen = false`

---

## 3. Quitter la partie

### 3.1 BTN_QuitToMenu OnClicked
Dans `WBP_InGameMenu` :

- `Get Game Instance`
- Cast `BP_GameInstance_Multi`
- Appeler `ReturnToMenu`

---

## 4. Implémenter ReturnToMenu

### 4.1 Event `ReturnToMenu` dans GameInstance
Dans `BP_GameInstance_Multi` :

`ReturnToMenu` :
- `Get First Local Player Controller`
- `Execute Console Command` : `disconnect`
- puis `Open Level(Map_Menu)`

> Note :
> - `disconnect` force la fin de connexion si on est client
> - sur host listen server : la partie s’arrête (normal)

---

# TP7 — Gestion Network Failure (host quitte / perte réseau)

Objectif :
- si le host ferme la fenêtre → le client retourne au menu au lieu de rester bloqué

## 1. Configurer Network Failure Event
Dans `BP_GameInstance_Multi` :

Utiliser l’event :
- `Event Network Error` / `On Network Failure` (selon version UE / nodes disponibles)

### 1.1 Implémentation
- Afficher Print String : "NETWORK FAILURE"
- `Open Level(Map_Menu)`

Optionnel :
- afficher une popup UI avec message propre
