# TP Réseau TCP
---

## Ce que vous avez déjà (base fournie)
### Serveur TCP
- ouvre une socket TCP
- bind sur un port
- listen
- accept 1 client
- boucle recv() / send() et renvoie `echo: <message>`

### Client TCP
- socket TCP
- connect vers localhost
- lit une ligne
- send
- recv la réponse du serveur

Vous devez **modifier** et **étendre** ce code, pas repartir de zéro.

---

# PARTIE A — Protocole de commandes (TCP) 

## A1) Objectif
Transformer l’echo en un serveur de commandes.

Le client envoie une commande texte, le serveur répond.

---

## A2) Commandes obligatoires
### Commande 1 : PING
Client envoie : `PING`
Serveur répond : `PONG`

### Commande 2 : NAME
Client envoie : `NAME Alice`
Serveur répond : `WELCOME Alice`

### Commande 3 : QUIT
Client envoie : `QUIT`
Serveur répond : `BYE`
Puis le serveur ferme la connexion proprement.

---

## A3) Contraintes
- Le serveur doit être **insensible à la casse** (PING, ping, Ping → OK).
- Le serveur doit ignorer les espaces au début/fin de ligne.

---

## A4) Indices (sans réponse)
- Le serveur reçoit une string : vous devez la **parser**.
- Pensez à faire une fonction utilitaire :
  - une fonction qui supprime `\n` et `\r`
  - une fonction qui découpe en mots (tokenization)
- Vous n’avez pas besoin de regex.

---

## A5) Checkpoints de validation
- Tapez `PING` dans le client → vous recevez `PONG`
- Tapez `NAME Bob` → vous recevez `WELCOME Bob`
- Tapez `QUIT` → le client se termine et le serveur affiche “Client disconnected”

---

# PARTIE B — Ajout d’un état serveur (mini serveur de jeu) 

## B1) Objectif
Le serveur doit stocker un état simple : position du joueur.

Le joueur n’existe que côté serveur :
- x (entier)
- y (entier)

Au démarrage : (0,0)

---

## B2) Commandes à ajouter
### MOVE
Client : `MOVE dx dy`
- `dx` et `dy` sont des entiers (peuvent être négatifs)

Serveur :
- met à jour la position : `x += dx`, `y += dy`
- répond : `OK POS x y`

### POS
Client : `POS`
Serveur répond : `POS x y`

---

## B3) Indices
- Déclarez x et y **dans le scope de la boucle principale serveur**.
- Pour convertir un string en int :
  - utilisez une fonction standard
  - gérez les cas invalides (ex: `MOVE toto 5`)
- Si la commande est mal formée : répondre `ERR`

---

## B4) Checkpoints
- POS → POS 0 0
- MOVE 3 4 → OK POS 3 4
- MOVE -1 2 → OK POS 2 6

---

# PARTIE C — Protocole correct : framing par taille

## C1) Objectif
Corriger définitivement le problème des messages collés/coupés.

On n’envoie plus directement `PING\n`.

On envoie :
- 4 octets = taille du message (uint32)
- N octets = message (ex: "PING")

---

## C2) Travail demandé
Implémenter :
- `send_packet(sock, msg)`
- `recv_packet(sock)`

Et remplacer dans le serveur/client l’utilisation de `send/recv` direct par ces fonctions.

---

## C3) Indices importants
- Vous devez faire une fonction `recv_exact(sock, buffer, n)` :
  - boucle tant qu’on n’a pas reçu n octets
  - attention : recv peut retourner moins que demandé
- Vous devez convertir le uint32 en “ordre réseau”
  - utiliser les fonctions standard

---

## C4) Checkpoints
- Envoyer 10 commandes rapidement sans erreur.
- Envoyer 2 commandes collées dans le même `send_packet` (optionnel) :
  - le serveur doit encore les comprendre correctement.

---

# PARTIE D — Bonus (facultatif) : multi-client

## D1) Objectif
Le serveur doit accepter plusieurs clients.
Chaque client peut envoyer ses commandes indépendamment.

---

## D2) Indices
- Vous pouvez faire :
  - un thread par client
  - ou un système `select()`
- Stratégie simple :
  - dans le main, accepter et créer un thread
  - le thread gère recv_packet + parsing commandes

