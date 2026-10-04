# Gaston : cahier des charges

Séquenceur maître du patch, pensé pour piloter des Ernest. Huit voies de trigs avec probabilité par pas, quatre voies CV
indépendantes, polyrythme par longueur et rapport d'horloge propres à chaque voie, horloge intégrée et transport façon CDJ.

Décisions prises avec Yoann le 2026-10-04 (séance de questions), maquette de référence :
[Gaston, panneau laque et laiton](https://claude.ai/artifact/LLtDG75ZLsznkWBAdriYmq).

## Glossaire

- **Voie** : une piste du séquenceur. Huit voies de trigs (1 à 8), quatre voies CV (A à D). On en édite une à la fois,
  choisie dans le sélecteur.
- **Pas** : une case de la voie. 64 pas au plus par voie.
- **Page** : une tranche de 16 pas. Quatre pages ; le ruban et la carte affichent la page choisie, à la main.
- **Ruban** : la rangée de 16 pas de la voie sélectionnée, où l'on édite.
- **Carte** : sous le ruban, les 12 voies en petits carrés sur la page affichée, avec le bandeau de progression.
- **LENGTH** (le *last step* des Korg) : le nombre de pas joués par la voie, de 1 à 64.
- **Rapport** (DIVISION) : la vitesse de la voie par rapport au pas maître.
- **Pas maître** : la double croche de l'horloge. Le tempo est en noires, quatre pas maîtres par noire.
- **CUE** : le point de départ, toujours le pas 1 de toutes les voies.

## Langue et textes

Panneau, infobulles et menus en anglais (le contenu technique) ; commentaires du code et ce document en français.
Les tailles de texte des outils de panneau sont des em ; NanoVG prenant la taille pour la hauteur de ligne, le code
les multiplie par 1/0,70 (`G_FONT_SCALE`), pour que Rack affiche ce que montrent les aperçus.

## Format

- 36 HP, panneau « laque et laiton » : laque bleu nuit, filets et plaques de sortie laiton. Ambre pour les trigs,
  vert-de-gris (la patine du laiton) pour les CV, un seul accent rouge-orangé pour REC.
- Le panneau est compartimenté : colonne de gauche HORLOGE, TRANSPORT, ENTRÉES, SORTIES ; à droite l'écran de voie,
  le bloc PAS (ruban, carte, pages, réglages de voie) et le bloc VOIES (sélecteur, pads, mutes, sorties).

## Voies

### Voies de trigs (1 à 8)

- Chaque pas est allumé ou éteint, avec une probabilité de 1 à 100 %. Un pas qu'on allume démarre à 100 %.
- À chaque passage sur un pas allumé, on tire au sort : le trig part si le tirage est sous la probabilité.
- Sorties : un trig de 10 ms à 10 V par voie, et une sortie POLY à 8 canaux.

### Voies CV (A à D)

- Chaque pas porte une tension. Plage ±5 V (défaut) ou 0–10 V, libre ou quantifiée au demi-ton, réglées sur le panneau.
- Chaque pas est actif (défaut) ou inactif. Un pas inactif ne pose pas de tension : la sortie garde celle du dernier
  pas actif (sample & hold) jusqu'au prochain. On l'active ou le désactive sur la rangée fine sous les barres.
- SLEW par voie : glissement entre les pas.
- Indépendantes des trigs : leur propre longueur, rapport et sens.

### Réglages par voie

- **LENGTH** de 1 à 64 ; Ctrl-clic (Cmd sur Mac) sur un pas en fait le dernier.
- **DIVISION**, parmi ÷4 ÷3 ÷2 ×2/3 ×3/4 ×1 ×5/4 ×4/3 ×3/2 ×2 ×3 ×4 (voies CV comprises).
- **SENS** : avant, arrière, aller-retour, hasard (bouton qui fait défiler).
- Les potards LENGTH, DIVISION, SLEW agissent sur la voie sélectionnée et sautent à sa valeur quand on change de voie.
- Les réglages affichés changent avec la voie : trig → LENGTH, DIVISION, SENS ; CV → LENGTH, DIVISION, SENS, SLEW,
  plage et quantification. Tout le bloc PAS passe au vert-de-gris sur une voie CV.

## Édition

- **Ruban, voie de trigs** : 16 cases carrées ; un clic allume ou éteint le pas. Sous chaque case, une jauge fine de
  probabilité, sans chiffre : on glisse verticalement pour la régler (continu, sans aimant). Pas éteint : jauge vide en
  pointillés.
- **Ruban, voie CV** : barres verticales autour d'une ligne zéro (ou depuis le bas en 0–10 V) ; glisser verticalement
  règle la tension. En bas de chaque case, une bande de lecture affiche la valeur du pas : la tension (« +2.3 »,
  « 7.5 » en 0–10 V) ou la note (« Eb4 ») si la voie est quantifiée ; atténuée quand le pas est inactif. Un clic sur
  la lecture ne change pas la valeur, le glisser l'ajuste sans saut. Sous chaque case, la rangée d'activation
  (pleine : actif ; pointillés : inactif, en creux).
- Les numéros des pas sont au-dessus, collés. Les pas au-delà de LENGTH sont grisés mais restent modifiables.
- Les pages se changent à la main (pas de suivi automatique) ; le bouton de la page où joue la voie clignote.
- **Menu de voie** (clic droit sur le ruban) : effacer, copier et coller la voie, copier et coller la page,
  remplir au hasard, décaler d'un pas à gauche ou à droite.

## Carte

- Sous le ruban, dans son propre cadre : 12 lignes (une par voie), 16 petits carrés alignés sur les colonnes du ruban,
  la page affichée seulement.
- Pas allumés lumineux (plus pâles quand la probabilité baisse), pas éteints à peine marqués.
- Un **bandeau** (rail fin) avance du début du cycle jusqu'au pas courant, avec un bord lumineux ; il retombe quand
  la voie reboucle. En arrière il part de la droite, en aller-retour il suit la direction, en hasard il n'y en a pas.
- Un carré **s'allume en blanc seulement quand un trig part vraiment** (probabilité réussie, voie non mutée).
  Pas de flash sur les voies CV.
- Un clic sur une ligne sélectionne la voie.

## Horloge

- TEMPO de 30 à 300 BPM, affiché ; SWING de 50 à 75 %.
- Le swing retarde le second pas de chaque paire de doubles croches (formule Linn). Il s'applique aux rapports binaires
  (÷4 ÷2 ×1 ×2 ×4) ; les autres (triolets, quintolets) restent droits.
- **CLOCK IN** : branchée, l'horloge externe remplace l'horloge interne (résolution au menu).
- **CLOCK OUT** : swinguée elle aussi, quelle que soit la résolution choisie au menu : 1 par pas (4 PPQN), 2 par pas,
  24 PPQN ou 48 PPQN.
- **Timing vintage (96 PPQN)**, au menu, éteint par défaut : tout le timing (swing compris) est arrondi à une grille de
  96 PPQN, plus un aléa de ±0,4 ms par événement. C'est ce que mesurent les tests d'Innerclock sur la MPC60 ; le swing
  devient cranté (environ 2 % par cran).
- **Réalignement**, au menu : toutes les voies reviennent au pas 1 tous les 16, 32 ou 64 pas maîtres, ou jamais (défaut).

## Transport (façon CDJ)

- **PLAY/PAUSE** : démarre ou met en pause, reprend là où on s'était arrêté.
- **CUE** : à l'arrêt, maintenu, joue depuis le début tant qu'on le tient ; relâché, retour au début et arrêt. Pendant la
  lecture : retour au début et arrêt. CUE maintenu puis PLAY : la lecture continue au relâché.
- Au chargement d'un patch, Gaston est en pause, calé au début.
- **PLAY IN** : un trig bascule lecture et pause. **RESET IN** : retour au début sans arrêter.
- **RUN OUT** : gate haute pendant la lecture. **RESET OUT** : impulsion à chaque retour au début (CUE compris).

## Jeu et enregistrement

- **MUTE** par voie de trigs, immédiat. Les pads jouent même sur une voie mutée.
- **PADS** : un pad par voie de trigs ; il joue toujours la sortie.
- **REC** : armé pendant la lecture, frapper un pad écrit un trig à 100 % sur le pas le plus proche de la voie (en
  retard : le pas courant ; en avance : le suivant, qui ne rejoue pas). Overdub seulement, rien n'est effacé.
  Pas d'enregistrement pas à pas.
- **PADS IN** (poly, 8 canaux) : le canal N joue et enregistre comme le pad N.

## Sauvegarde

Tout est sauvé dans le patch (pas, probabilités, tensions, longueurs, rapports, sens, plages, quantification, slew,
mutes, voie et page affichées, réglages du menu), sauf l'état de lecture.

## Gastounet : mémoires et morceau

Expander de 20 HP, **collé à gauche** de Gaston uniquement (Gaston affiche alors « PATTERN A03 » en tête, et « EDITED »
si le motif a changé depuis sa mémoire). Les mémoires et le morceau vivent dans Gaston (moteur et sauvegarde) :
Gastounet est la surface de commande, ses boutons et ses prises arrivent par message à chaque échantillon.
Maquette : même canevas que Gaston.

- **Mémoire** : toutes les voies (pas, probabilités, tensions, activations) et leurs réglages (LENGTH, rapport, sens,
  plage, quantification, slew). Ni les mutes (gestes de live) ni le tempo et le swing (globaux).
- **64 motifs** : 4 banques (A à D) de 16, une grille 4 × 4 et 4 boutons de banque.
- **Motif de travail** : Gaston contient le motif chargé. **WRITE puis un slot** l'écrit (la mémoire en cours, ou une
  autre, qui devient l'active). Changer de motif sans écrire perd les modifications, comme sur une Korg : recharger le
  même slot revient à l'original. Pas de sécurité. Le motif de travail est sauvé avec le patch.
- **COPY**, puis la source, puis la destination. Effacer un slot, une banque, tout, ou le morceau : menu clic droit.
- **NEXT** (la mémoire pleine suivante de la banque) et **RANDOM** (une autre mémoire pleine de la banque, au hasard).
- **Pas de longueur de motif.** LAUNCH quantifie le moment du changement, sur une grille comptée depuis le départ du
  transport : NOW (le pas suivant), 1 BEAT, 1 BAR (défaut), 2 BARS, 4 BARS, ou TRACK 1 (quand la voie 1 reboucle).
  Le slot attendu clignote vite ; à l'arrêt, le motif se charge tout de suite.
- **RESTART** (défaut) : au changement, toutes les voies repartent du pas 1. **LEGATO** : elles gardent leur position et
  leur cycle, seul le contenu change.
- **Morceau** : jusqu'à 64 lignes « motif + durée en mesures (1 à 64) », un morceau par patch. Le morceau change de
  ligne en fin de mesure. END: LOOP ou END: STOP (arrêt du transport, calé au début). En mode SONG, le retour au
  début (CUE, RESET) ramène à la première ligne ; entrer en mode SONG en lecture démarre à la mesure suivante.
- **Écrire le morceau** : SONG REC, puis chaque slot ajoute une ligne de 4 mesures ; dans la liste de l'écran (4 lignes
  visibles), on choisit une ligne, on glisse verticalement sur la mémoire ou la durée (ou la molette : durée, Maj :
  mémoire). À droite de l'écran, la section ROWS a trois vrais boutons (assignables en MIDI) qui agissent sur
  la ligne choisie : + ROW, DUP, − ROW.
- **Prises** : PATTERN (0 à 10 V parcourent les 16 slots de la banque affichée), NEXT, RANDOM, RESET (retour à la
  première ligne du morceau, à la mesure suivante en lecture) ; sortie END (un trig à chaque changement de motif, et
  à la fin du morceau).
- Pas d'enregistrement d'événements (mutes, potards) dans le morceau. Tempo du morceau : celui de Gaston.

## Reporté en v2

- Humanize audible (un aléa de plusieurs millisecondes, distinct du timing vintage).
- Enregistrement d'une entrée CV dans une voie CV.
- Conditions de trig façon Elektron.
- Accent ou vélocité (passerait plutôt par une entrée LEVEL sur Ernest).
