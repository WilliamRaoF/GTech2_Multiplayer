# TP Réseau UDP

---

## 1) Spécification du “jeu”
On simule un jeu 2D minimal :
- chaque joueur a une position `(x, y)` 
- le monde est un carré `[-50..50]`
- plusieurs joueurs peuvent bouger en même temps

Le serveur maintient :
- une liste de joueurs connectés (id + adresse réseau + position + dernier message reçu)

---

## 2) Protocole
Tous les messages sont des chaînes ASCII (un datagramme = une commande).  
Séparateur : espace.

### 2.1 Messages client -> serveur
1) `HELLO <name>`
- but : “s’enregistrer” auprès du serveur
- réponse attendue : `WELCOME <id>`

2) `INPUT <id> <seq> <dx> <dy>`
- but : envoyer un input de mouvement
- `seq` est un compteur qui augmente (1,2,3…) pour aider au debug/perte
- `dx, dy` sont petits (ex : -1, 0, 1) ou floats

3) `BYE <id>`
- but : quitter

### 2.2 Messages serveur -> client
1) `WELCOME <id>`
- id attribué par le serveur

2) `STATE <tick> <n> <id1> <x1> <y1> ... <idn> <xn> <yn>`
- snapshot du monde, envoyé régulièrement
- `tick` augmente à chaque snapshot
- `n` = nombre de joueurs inclus

3) `ERR <code> <info>`
- erreur protocole

---

## 3) Architecture attendue
### 3.1 Serveur 
- une socket UDP bind sur `27016`
- une boucle principale qui fait 2 choses :
  1) recevoir des datagrammes `recvfrom()` (non bloquant ou avec timeout court)
  2) toutes les X ms, envoyer un snapshot `STATE` à tous les joueurs

### 3.2 Client
- une socket UDP
- envoie `HELLO`
- reçoit `WELCOME id`
- boucle :
  - lit le clavier 
  - envoie `INPUT id seq dx dy` à une fréquence
  - reçoit des `STATE` et affiche positions

---
# Partie A — Enregistrement des joueurs (HELLO/WELCOME)

## A1) Objectif
Un client s’enregistre et obtient un id.

### Travail demandé
Serveur :
- si message commence par `HELLO` :
  - attribuer un id unique (1,2,3…)
  - enregistrer l’adresse de l’expéditeur (IP+port) comme “joueur”
  - répondre `WELCOME <id>` via `sendto()`

Client :
- envoyer `HELLO <name>`
- attendre `WELCOME <id>`
- stocker l’id local

### Indices
- l’adresse UDP à enregistrer est le `sockaddr_in from` reçu dans `recvfrom()`
- pour reconnaître un joueur : comparer IP+port
- si un joueur renvoie `HELLO` alors qu’il est déjà connu : renvoyer son id existant (ou `ERR`)

### Checkpoint
- 2 clients lancés : ils reçoivent des ids différents.

---

# Partie B — Inputs simultanés (INPUT)

## B1) Objectif
Chaque client envoie des inputs, le serveur met à jour les positions.

### Travail demandé
Serveur :
- gérer `INPUT <id> <seq> <dx> <dy>`
- valider :
  - l’id existe
  - dx,dy dans un intervalle raisonnable (ex : [-1..1])
- appliquer :
  - `x += dx * speed * dt` (simplifié)
  - `y += dy * speed * dt`
- clamp dans `[-50..50]`

Client :
- envoyer des `INPUT` à fréquence fixe (ex : 20 fois/seconde)
- `seq` augmente à chaque envoi

### Indices
- pas besoin d’un vrai dt précis : vous pouvez choisir `speed=1` et appliquer directement
- loggez les inputs reçus côté serveur avec `id` et `seq` pour voir la simultanéité

### Checkpoint
- Lancer 2 clients, générer des inputs différents :
  - le serveur affiche que les positions changent pour les deux ids.

---

# Partie C — Snapshots serveur (STATE) à fréquence fixe

## C1) Objectif
Le serveur envoie l’état du monde régulièrement à tous les clients.

### Travail demandé
Serveur :
- choisir un tick rate (ex : 10 Hz)
- toutes les 100 ms :
  - construire `STATE <tick> <n> ...`
  - envoyer à tous les joueurs enregistrés

Client :
- écouter les messages `STATE`
- à réception :
  - parser la liste
  - afficher un tableau simple en console :
    - tick, positions de chaque joueur

### Indices
- pour faire “toutes les 100 ms”, utilisez un chrono et un accumulateur
- si vous ne voulez pas de socket non bloquante : utilisez un timeout sur `recvfrom()` (select() est autorisé)
- ne cherchez pas la perfection : on veut voir “ça bouge et ça s’actualise”

### Checkpoint
- Le client voit un flux de `STATE` avec des ticks qui augmentent.
- Deux joueurs apparaissent simultanément et bougent.

---

# Partie D — Timeouts et “déconnexion” 

## D1) Objectif
Retirer un joueur silencieux.

### Travail demandé
Serveur :
- stocker `lastSeenTime` pour chaque joueur (date du dernier message reçu)
- si `now - lastSeenTime > 3 secondes` :
  - retirer le joueur
  - il n’apparaît plus dans `STATE`

Client :
- `BYE <id>` : demander à quitter proprement (optionnel, bonus)

### Indices
- on ne peut pas “savoir” qu’un client est mort en UDP : on déduit via timeout

### Checkpoint
- Fermer un client brutalement : après 3s, il disparaît des snapshots.

---

# Bonus (facultatif)
### Bonus : Simulation de perte
Serveur :
- ignorer aléatoirement 10% des `INPUT` (au hasard)
Observer :
- le client continue de fonctionner
- l’état reste cohérent grâce aux snapshots

### Bonus 2 : Rate limiting
Serveur :
- refuser un client qui envoie plus de 60 INPUT/sec (anti-spam)
