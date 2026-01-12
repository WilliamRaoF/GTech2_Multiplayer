# TCP / UDP 

---

## Objectifs pédagogiques
À la fin de ce cours, vous être capable de :
- Définir les notions réseau essentielles au jeu en ligne (latence, gigue, perte, débit)
- Expliquer le rôle de la couche transport et la différence entre TCP et UDP
- Comprendre les mécanismes internes de TCP (handshake, ACK, retransmission, congestion control)
- Comprendre le fonctionnement d’UDP et ses conséquences (absence de garanties)
- Justifier le choix de TCP ou UDP selon un besoin gameplay
- Relier ces protocoles aux problématiques classiques des jeux réseau (désynchronisation, lag, prediction, interpolation)

---

# Plan

## Partie 1 : Fondations réseaux nécessaires
1. Pourquoi le réseau est difficile en jeu
2. Notions de base : latence, jitter, perte, débit
3. Couches réseau utiles (IP / TCP-UDP / application)
4. Ports, sockets
5. MTU, fragmentation

## Partie 2 : TCP en détail
1. Propriétés et garanties
2. TCP = flux (stream) et non messages
3. Handshake (3-way handshake)
4. ACK, retransmission
5. Head-Of-Line Blocking
6. Congestion control
7. Cas d’usage typiques en jeu

## Partie 3 : UDP en détail
1. Propriétés et absence de garanties
2. Datagrammes = messages
3. Gestion de la perte
4. Fiabilité partielle (reliable UDP)
5. Risques et limites d’UDP
6. Cas d’usage typiques en jeu

## Partie 4 : Application au jeu vidéo
1. Architectures réseau (P2P, client/serveur)
2. Serveur autoritaire
3. Tick rate
4. Prediction, correction, interpolation
5. Choisir TCP/UDP selon le type de donnée
6. Exemples concrets FPS / MMO / coop
7. Synthèse

---

# PARTIE 1 — Fondations réseaux utiles aux jeux

## 1. Pourquoi le réseau est “difficile” en jeu vidéo
Contrairement à de nombreuses applications (web, mail, fichiers), le jeu vidéo en ligne doit gérer :
- du temps réel : une action doit être perçue immédiatement
- de la cohérence : tous les joueurs doivent vivre un monde compatible
- de la fréquence : envoi et réception d’informations plusieurs fois par seconde

Le réseau impose des limites physiques et techniques :
- un paquet n’arrive pas instantanément
- un paquet peut être perdu
- un paquet peut arriver en retard ou dans le désordre
- le débit est limité et variable (wifi, 4G/5G, congestion)

Conséquence : un jeu en ligne est toujours un compromis entre :
- réactivité
- stabilité
- consommation réseau

---

## 2. Notions indispensables

### 2.1 Latence
La latence est le temps mis par une information pour aller d’un point A à un point B.  
En pratique, on parle souvent de ping (aller-retour).

Exemples :
- 20 ms : excellent, presque imperceptible
- 60–80 ms : jouable dans la plupart des jeux
- 150 ms : sensation de retard, avantage aux joueurs à faible ping
- Plus : expérience dégradée
  
La latence provient de :
- distance géographique
- route réseau (nombre de sauts)
- traitement routeurs / NAT
- congestion

---

### 2.2 Jitter (gigue)
Le jitter est la variation de latence dans le temps.

Exemple de latence stable :
- 35 ms / 36 ms / 35 ms / 36 ms

Exemple de jitter :
- 20 ms / 90 ms / 30 ms / 120 ms

Le jitter est souvent pire qu’une latence moyenne élevée car :
- il rend le mouvement des joueurs irrégulier
- il provoque des corrections fréquentes (snapback)
- il casse le rythme (timing) dans les jeux compétitifs

---

### 2.3 Perte de paquets (packet loss)
Certains paquets envoyés n’arrivent pas.

Exemples :
- 0% : excellent
- 1% : commence à être perceptible en FPS
- 3–5% : jeu “incontrôlable” selon le type de gameplay

Causes :
- wifi instable
- congestion
- saturation routeur
- MTU / fragmentation mal gérée

---

### 2.4 Débit (throughput)
Quantité d’information transférée par seconde.

Exemple simple :
- 20 paquets/sec de 200 octets = 4 kB/s

Mais en jeu multi :
- nombre de joueurs
- fréquence des snapshots
- événements (explosions, projectiles)
peuvent multiplier la charge.

---

## 3. Couches réseau utiles (vision simplifiée)
Sans détailler OSI complet :

- IP : acheminement des paquets (best effort)
- TCP/UDP : transport
- Application : protocole du jeu (format des messages, états, règles)

Important :
- TCP et UDP utilisent IP, mais ajoutent des comportements très différents

---

## 4. Ports et sockets
- Adresse IP : identifie une machine sur le réseau
- Port : identifie un service sur la machine
- Socket : combinaison protocole + IP + port + état

Exemples :
- serveur de jeu : 203.0.113.10:27015 (souvent UDP)
- serveur web : 203.0.113.10:443 (TCP)

---

## 5. MTU et fragmentation
MTU (Maximum Transmission Unit) : taille maximale d’un paquet sans fragmentation.  
Souvent ~1500 octets sur Ethernet.

Si un paquet dépasse MTU :
- il peut être fragmenté au niveau IP
- si un fragment est perdu, tout le paquet est inutilisable

En jeu, on préfère :
- petits paquets fréquents
- compression et quantification
- éviter les paquets trop gros

Exemple :
- snapshots de positions : petits paquets réguliers
- inventaire complet : transmission rare, éventuellement segmentée

---

# PARTIE 2 — TCP : transport fiable

## 1. Propriétés et garanties TCP
TCP fournit :
- livraison fiable (retransmission)
- ordre garanti
- anti-duplication
- contrôle de flux (flow control)
- contrôle de congestion (congestion control)

TCP est adapté aux données :
- importantes
- qui doivent arriver complètes
- pour lesquelles la latence est secondaire

---

## 2. TCP est un flux (stream), pas des messages
TCP ne préserve pas les frontières des messages. Il transmet un flux continu d’octets.

Problème :
- l’application doit reconstruire les messages

Exemple :
Vous envoyez deux messages A puis B.
La réception peut être :
- A+B d’un coup
- A seul, puis B
- A partiel, puis reste de A + B, etc.

Conséquence :
- il faut un protocole applicatif avec framing
  - longueur en header
  - séparateurs
  - structures connues

---

## 3. Connexion TCP : 3-way handshake
Avant d’échanger :
1. client -> SYN
2. serveur -> SYN-ACK
3. client -> ACK

Avantages :
- mise en place fiable
- états de connexion gérés

Inconvénients :
- délai initial supplémentaire
- surcoût réseau (paquets de contrôle)

---

## 4. ACK et retransmission
TCP utilise des ACK :
- l’émetteur sait ce qui a été reçu
- les segments non confirmés sont retransmis

Avantage :
- l’application reçoit toutes les données

Inconvénient :
- retransmettre prend du temps
- introduit des retards irréguliers

Exemple :
Un paquet contenant un état important est perdu.
TCP le retransmet, mais le temps de retransmission peut être supérieur à la durée utile pour le gameplay.

---

## 5. Head-Of-Line Blocking (HOL)
TCP impose l’ordre. Donc si un segment est manquant, tout ce qui suit est bloqué.

Exemple gameplay :
- message 100 : position
- message 101 : tir
Si 100 est perdu mais 101 arrive :
- TCP retarde 101 tant que 100 n’est pas récupéré

Impact :
- freeze du jeu
- sensation de "lag spikes"
- retard sur des actions critiques

---

## 6. Congestion control : TCP ralentit volontairement
TCP suppose qu’une perte = congestion et réduit son débit.

Sur un réseau instable (wifi, mobile) :
- pertes temporaires
- TCP ralentit
- le jeu devient irrégulier

Ce comportement est excellent pour :
- transfert de fichiers
- web
mais pas pour :
- temps réel interactif

---

## 7. Cas d’usage TCP en jeu vidéo
TCP est approprié pour :
- authentification / login
- chat textuel
- inventaire / profils
- matchmaking (parfois)
- résultats de match
- commerce / économie
- téléchargement (patch, maps)

---

# PARTIE 3 — UDP : transport minimal temps réel

## 1. Propriétés UDP
UDP est un protocole léger :
- pas de handshake
- pas de garantie de livraison
- pas de garantie d’ordre
- pas de contrôle de congestion au niveau transport

UDP transmet des datagrammes : des messages indépendants.

Avantages :
- faible latence
- pas de blocage HOL
- traitement simple

Inconvénients :
- fiabilité à gérer soi-même si nécessaire
- sécurité plus complexe

---

## 2. Datagrammes : messages conservés
Contrairement à TCP :
- 1 envoi = 1 message (datagramme)
- la réception lit le message entier

Exemple :
Vous envoyez 3 messages UDP.
Le receveur peut en recevoir 2 :
- ces 2 messages sont complets et exploitables immédiatement

---

## 3. La perte n’est pas toujours un problème
En jeu temps réel, certaines données deviennent rapidement obsolètes.

Exemple :
Positions envoyées à 30 Hz.
Si un paquet est perdu :
- le prochain paquet remplace l’information perdue

Dans ce cas, chercher à retransmettre l’ancien paquet est inutile, voire nuisible.

---

## 4. Fiabilité partielle : “reliable UDP”
Les jeux implémentent souvent une fiabilité au-dessus d’UDP uniquement pour certains messages.

Principe :
- séquence / numéro de message
- ACK au niveau application
- retransmission ciblée

Exemple :
- “position joueur” : non fiable
- “j’ai ramassé l’objet rare” : fiable

On obtient un système mixte :
- UDP pour temps réel
- sous-couche fiable partielle pour événements critiques

---

## 5. Risques et limites d’UDP
UDP a des contraintes pratiques :
- NAT / firewall : difficulté de traversée
- spoofing : usurpation d’identité d’adresse IP
- amplification / flood
- anti-cheat plus difficile

Les jeux doivent ajouter :
- chiffrement éventuel
- authentification des messages
- anti-replay, tokens, signatures

---

## 6. Cas d’usage UDP en jeu vidéo
UDP est approprié pour :
- positions, rotations, vitesses
- snapshots d’état monde
- inputs client vers serveur
- projectiles et événements fréquents
- voix (VoIP) dans certains systèmes

---

# PARTIE 4 — Application au jeu vidéo

## 1. Architectures réseau

### 1.1 Peer-to-peer (P2P)
Les joueurs communiquent entre eux.

Avantages :
- coût serveur faible
- latence potentiellement basse entre certains pairs

Inconvénients :
- NAT difficile
- triche facilitée
- host advantage (si un joueur héberge)
- synchronisation complexe

---

### 1.2 Client/Serveur (standard moderne)
Un serveur central valide et redistribue.

Avantages :
- cohérence contrôlée
- réduction triche
- meilleure stabilité

Inconvénients :
- coût infrastructure
- latence serveur pour tous

---

## 2. Serveur autoritaire
Le serveur est la référence de vérité :
- le client envoie des intentions (inputs)
- le serveur simule, valide, applique

Exemple :
Le client dit : “j’ai touché”
Le serveur décide : “hit valide ou non”

Objectif :
- limiter triche
- éviter incohérences majeures

---

## 3. Tick rate
Le serveur simule à une fréquence fixe.

Exemples :
- 20 Hz : MMO / monde large
- 30 Hz : action standard
- 60+ Hz : FPS compétitif

Plus le tick rate est élevé :
- plus c’est précis et réactif
- plus ça coûte en CPU et réseau

---

## 4. Client-side prediction
Attendre la réponse serveur avant d’afficher le mouvement rendrait le jeu injouable.

Principe :
- le client applique localement ses inputs
- le serveur renvoie l’état officiel
- si divergence : correction (reconciliation)

Exemple :
Le joueur appuie sur avancer.
Le client avance immédiatement.
150 ms plus tard le serveur renvoie la position officielle.
Si le client diffère : correction.

---

## 5. Interpolation et extrapolation
Le rendu est à 60 FPS, mais les états réseau arrivent à 20/30 Hz.

Interpolation :
- afficher un état entre deux snapshots connus

Exemple :
Snapshots à t=0 et t=50 ms.
On affiche à t=25 ms une position interpolée.

Extrapolation :
- prédire au-delà du dernier snapshot si retard

Risques :
- prédiction incorrecte -> correction visible

---

## 6. Choisir TCP/UDP selon la donnée
On raisonne selon :
- importance de la donnée
- fréquence d’envoi
- durée de validité
- tolérance à la perte

### 6.1 Données fraîches et remplaçables
Exemples :
- positions
- rotations
- vitesses
Transport :
- UDP non fiable

### 6.2 Événements critiques
Exemples :
- dégâts appliqués
- validation kill
- changement d’arme
Transport :
- UDP fiable (au-dessus) ou TCP

### 6.3 Données persistantes
Exemples :
- inventaire
- progression
- score
Transport :
- TCP

---

## 7. Exemples comparatifs concrets

### Exemple A : mouvement joueur
Données envoyées :
- position, rotation, vitesse

UDP :
- paquet perdu -> on continue, le prochain corrige
- mouvement fluide avec interpolation

TCP :
- perte -> retransmission
- blocage HOL -> freeze et saut

---

### Exemple B : chat textuel
UDP :
- message perdu -> utilisateur ne le reçoit jamais

TCP :
- ordre et fiabilité garantis
- parfait pour texte

---

### Exemple C : ramassage d’un objet rare
UDP non fiable :
- paquet perdu -> désynchronisation grave
  - client pense avoir l’objet, serveur non

UDP fiable ou TCP :
- confirmation nécessaire
- cohérence garantie

---

## 8. Synthèse de décision
TCP :
- fiable
- ordonné
- stream
- plus lent en temps réel instable (HOL + congestion control)

UDP :
- rapide
- messages indépendants
- pas de garanties
- nécessite des mécanismes applicatifs

En pratique :
- les jeux d’action utilisent UDP pour gameplay temps réel
- TCP est utilisé pour services et données persistantes
- beaucoup de jeux font un mix (UDP + fiabilité partielle)

---

# Conclusion : Points clés à retenir
- TCP garantit la livraison et l’ordre, mais peut introduire des latences irrégulières nuisibles au temps réel
- UDP n’offre aucune garantie mais permet une meilleure réactivité
- en jeu vidéo temps réel, la fraîcheur de l’information est souvent plus importante que la fiabilité absolue
- les techniques de prediction/interpolation sont indispensables pour masquer latence et jitter
- le protocole idéal dépend du type de données, pas seulement du jeu
 
