# Louis : cahier des charges

Générateur Krell à quatre voix. Il ne produit aucun son : chaque voix sort un trig, un gate, une hauteur et son
enveloppe, pour piloter des voix ailleurs dans le patch. Vivant tout seul, quantifié ou libre, calé sur une horloge
externe quand on en branche une, en timing libre sinon.

Le nom salue Louis Barron, qui a composé avec Bebe Barron la musique électronique de *Forbidden Planet* (1956), dont
la scène des Krell a donné son nom au patch de Todd Barton. Recherche et sources :
[Krell-research.md](Krell-research.md).

Décisions prises avec Yoann le 2026-10-22 : nom Louis, quatre jacks par voix (enveloppe comprise), sélecteur de
gammes et de modes propre au module.

## Glossaire

- **Voix** : un Krell complet, numérotées 1 à 4. Les quatre sont identiques ; elles diffèrent par leurs réglages
  TIME et OCT, par leurs dérives propres et par leurs liens.
- **Cycle** : une attaque puis un decay. À la fin du decay, le cycle suivant commence aussitôt : la voix se relance
  elle-même, en interne, sans câble.
- **Note** : un cycle joué. Un cycle peut aussi être un **silence** (voir DENSITY) : il dure, mais rien ne sort.
- **Dérive** : une tension aléatoire lente et continue (la FRV du Buchla 266). Les dérives déplacent les durées, le
  rapport attaque/decay et le penchant des hauteurs. Elles ne sautent jamais.
- **Tirage** : ce qui est décidé une fois par cycle, au début de la note : la hauteur, un petit aléa sur la durée,
  note ou silence.
- **Boucle** : la mémoire de chaque voix, ses derniers tirages, qu'elle peut rejouer (voir MEMORY).
- **Grille** : les instants permis quand une horloge est branchée, un coup d'horloge multiplié ou divisé par GRID.
- **Anneau** : l'ordre des voix pour les liens, 1 → 2 → 3 → 4 → 1.

## Langue et textes

Panneau, infobulles et menus en anglais ; commentaires du code et ce document en français (comme Gaston).

## Format

- 34 HP environ, à confirmer par l'outil de panneau : 43 prises (20 entrées, 23 sorties), 23 potards, un sélecteur à
  trois positions, 4 boutons, un écran.
- Allure (maquette validée avec Yoann le 2026-10-22, [Louis, panneau](https://claude.ai/artifact/Es8mchpaVusoknQRdvowev)) :
  instrument de laboratoire des années 1950, façade bakélite ivoire, potards noirs à index crème, écran au phosphore
  vert, plaques de sortie noires, bandeaux bleu ardoise (#2c4250) en tête du panneau et de chaque bloc, titres ivoire.
  Un seul accent lumineux, le vert phosphore. Sans métaphore illustrée : pas de soucoupes ni de monstres.
- Disposition (maquette du 2026-10-22) : l'écran sur toute la largeur ; dessous CLIMATE (huit potards, l'entrée CV de
  chacun juste en dessous, puis DRIFT A et DRIFT B sur une plaque de sortie) et PITCH (SCALE, ROOT, RANGE, SKEW, SCALE
  IN) ; en bas à gauche LINK (potard et sélecteur CHAIN, HUSH, ECHO) au-dessus de CLOCK (GRID, SNAP, entrées CLOCK et
  RESET) ; en bas à droite VOICES (TIME, OCT, HOLD, diode par voix, puis STRIKE, FREEZE et ANY) et la plaque des seize
  sorties avec leurs quatre sorties poly.
- Pas de valeur imprimée sous les potards à crans : SCALE, ROOT et GRID s'affichent à l'écran.
- Deux voyants de dérive au-dessus de DRIFT A et DRIFT B (barres bipolaires), et une **roue des notes** au-dessus de
  SCALE IN : douze points, allumés pour les notes de la gamme en cours (sélecteur ou SCALE IN), le dernier joué brille.

## Le cœur : une voix

### Durée et forme d'un cycle

- **Durée** d'un cycle : `PACE × rapport TIME de la voix × dérive de durée × aléa du tirage × corrélation`, calculée en
  domaine logarithmique (l'oreille entend des rapports de durée).
- **Attaque** = durée × rapport ; **decay** = le reste. Le rapport vient de TILT, déplacé par une dérive de forme
  propre à la voix.
- **Planchers** stricts : attaque ≥ 1 ms, decay ≥ 5 ms, cycle ≥ 15 ms. Aucun tirage ni aucune CV ne peut descendre
  plus bas : pas de rafale à la fréquence d'échantillonnage.
- Plafond : cycle ≤ 60 s.

### Les dérives

- Chaque voix a deux dérives propres : **durée** et **forme**. Le module en a deux globales : **A**, qui penche les
  hauteurs de toutes les voix, et **B**, qui fait onduler la densité.
- Une dérive glisse entre des cibles tirées au hasard, avec une interpolation douce (cubique). Sa vitesse moyenne
  vient de DRIFT, avec un écart propre à chaque dérive pour qu'elles ne marchent jamais au pas.
- Leur amplitude vient de WANDER.
- Les dérives A et B sortent sur deux prises (DRIFT A, DRIFT B), ±5 V.

### Le tirage, au début de chaque note

Dans cet ordre :

1. **Note ou silence** : tirage contre la densité effective.
2. **Rejouer ou tirer** : avec la probabilité MEMORY, la voix reprend le tirage stocké à la position courante de sa
   boucle ; sinon elle tire du neuf et l'écrit à cette position. La boucle stocke la hauteur brute (avant
   quantification), l'aléa de durée et le choix note/silence. Les dérives continuent de s'appliquer par-dessus : une
   boucle rejouée évolue lentement, elle ne se fige pas.
3. **Hauteur brute** : un nombre entre 0 et 1 dont la distribution est penchée par SKEW et la dérive A.
4. **Corrélation** : selon CORREL, la hauteur brute allonge ou raccourcit la durée du cycle.
5. **Quantification** et décalages (voir Hauteur), puis la hauteur est **tenue** pendant toute la note.

La hauteur change donc au début de la note, c'est-à-dire à la fin du cycle précédent, comme chez Barton.

### Les sorties d'une voix

| Prise | Signal |
|---|---|
| **TRIG** | 10 V pendant 10 ms au début de chaque note (raccourci si la note est plus courte) |
| **GATE** | 10 V du début de la note jusqu'à la fin de l'attaque plus GATE × decay ; retombe au moins 2 ms avant la note suivante, pour que deux notes collées se redéclenchent |
| **PITCH** | V/oct, tenu pendant la note, posé dès le début du cycle (avant le front de TRIG et de GATE) |
| **ENV** | l'enveloppe 0–10 V : attaque convexe, decay exponentiel façon analogique (linéaire au menu) |

Un cycle silencieux ne sort rien : pas de trig, pas de gate, ENV reste à 0, PITCH garde la note précédente.

Sorties poly : **TRIG**, **GATE**, **PITCH**, **ENV**, 4 canaux chacune (canal N = voix N). **ANY** : un trig
chaque fois qu'une voix joue une note.

## CLIMATE : les réglages d'ensemble

| Potard | Plage | Défaut | Rôle |
|---|---|---|---|
| **PACE** | 20 ms – 20 s, log | 600 ms | durée moyenne d'un cycle |
| **TILT** | 2 – 98 % | 15 % | part de l'attaque dans le cycle : percussif ↔ gonflé |
| **DRIFT** | 0,01 – 2 Hz, log | 0,08 Hz | vitesse des dérives |
| **WANDER** | 0 – 100 % | 40 % | amplitude des dérives : jusqu'à ×4 / ÷4 sur la durée, toute la course sur la forme ; l'aléa par note vaut le quart de la dérive |
| **DENSITY** | 0 – 100 % | 85 % | probabilité qu'un cycle soit une note ; la dérive B la fait onduler jusqu'à ±30 points × WANDER à mi-course, moins vers les bords, rien à 0 et 100 % (rafales et accalmies) |
| **MEMORY** | 0 – 100 % | 0 % | probabilité de rejouer la boucle ; 100 % = boucle fixe |
| **CORREL** | −100 – +100 % | 0 | + : l'aigu dure plus longtemps (le patch d'origine) ; − : l'aigu est bref ; jusqu'à ×2 / ÷2 |
| **GATE** | 0 – 100 % | 50 % | part du decay couverte par le gate |

À 0 % de DENSITY, Louis se tait : c'est voulu. À 0 % de WANDER et 0 % de MEMORY, les voix sont régulières, avec des
hauteurs au hasard.

## Hauteur

- **SCALE** : FREE, CHROMATIC, MAJOR, MINOR, DORIAN, PHRYGIAN, LYDIAN, MIXOLYDIAN, LOCRIAN, HARM MINOR, MAJ PENTA,
  MIN PENTA, WHOLE TONE. **FREE** = pas de quantification, la hauteur est continue (fidèle à Barton). Défaut : MIN
  PENTA.
- **ROOT** : la tonique, de C à B. La plage part de la tonique, à l'octave de C4 (0 V) : avec OCT = 0, la note la plus
  grave possible est la tonique à 0 V.
- **RANGE** : 0 à 5 octaves, défaut 2. Hauteur = tonique + OCT de la voix + hauteur penchée × RANGE.
- **SKEW** : −100 à +100 %, défaut 0. Penche la distribution vers le grave ou l'aigu sans la décaler (une courbe de
  puissance sur le tirage) : à fond vers le grave, la tonique revient sans cesse avec de rares envolées.
- La quantification prend la note de la gamme la plus proche, relative à la tonique. Elle se fait au tirage :
  PITCH ne bouge plus pendant la note.
- **SCALE IN** (poly) : branchée, elle remplace le sélecteur. Chaque canal donne une note (V/oct) ; seules les classes
  de hauteur comptent, l'octave est ignorée, la tonique aussi (les notes sont absolues). Un accord venant de Colette,
  de Gaston ou d'Édith devient la gamme. Un canal à 0 V reste une note (C) : c'est au patch de n'envoyer que des
  notes voulues.

## LINK : les voix entre elles

- **LINK** : 0 – 100 %, défaut 0. À la fin de chaque cycle joué (pas d'un silence), probabilité que la voix agisse sur
  la suivante dans l'anneau.
- **Mode** (sélecteur à trois positions) :
  - **CHAIN** : la voix suivante coupe son cycle et commence une note tout de suite (les cascades EOC → trigger de
    Maths).
  - **HUSH** : le prochain cycle de la voix suivante sera un silence.
  - **ECHO** : la prochaine note de la voix suivante reprend la hauteur de celle-ci, déplacée d'un intervalle tiré
    parmi unisson, tierce et quinte de la gamme (en FREE : unisson, quinte, octave), dans le registre de la voix
    suivante (l'écart d'OCT entre les deux voix est conservé). La deuxième voix suit la mélodie
    de la première, à son propre rythme (l'idée de la voix d'accords de J. D. Cramer).
- Chaque bouton de mode porte son effet gravé sous son nom (variante B, choisie le 2026-10-22) : CHAIN « next voice
  plays », HUSH « next voice rests », ECHO « next voice repeats ».
- Garde-fous :
  - période réfractaire : une voix ignore CHAIN si elle a commencé une note depuis moins de 50 ms ou depuis moins du
    quart de la durée prévue ;
  - un lien au plus par voix et par cycle ;
  - LINK reste une probabilité : à 100 % en CHAIN, les voix se suivent en cascade mais leurs cycles restent propres,
    elles ne se figent pas en séquenceur.

## Rangées VOICES

Pour chaque voix, de gauche à droite :

- **TIME** : le rapport de la voix, cranté : 1/4, 1/3, 1/2, 2/3, 1, 3/2, 2, 3, 4 (décision du 2026-10-22). Il multiplie
  la durée (PACE × TIME) et, horloge branchée, la grille de la voix (GRID × TIME) : à 2, la voix joue le même Krell deux
  fois plus lentement et sur un pas sur deux de la grille ; à 3/2, elle se cale sur des croches pointées quand GRID est
  la double croche, d'où un polyrythme. Défaut : 1.
- **OCT** : −3 à +3 octaves, cranté. Défaut : 0.
- État initial neutre : les quatre voix ont les mêmes réglages ; elles ne partent pas ensemble parce que chacune
  commence par un silence de durée tirée au hasard, puis leurs dérives s'écartent.
- **HOLD** (bouton allumé) : la voix rejoue sa boucle à 100 %, quel que soit MEMORY.
- Une diode qui s'allume à chaque note, puis les sorties TRIG, GATE, PITCH, ENV.

## CLOCK : contrainte d'horloge

- **CLOCK IN** débranchée : timing libre.
- **CLOCK IN** branchée : les débuts de note tombent sur la grille de leur voix, GRID × TIME.
- **GRID** : pas de grille en coups d'horloge, parmi 1/4, 1/3, 1/2, 1, 2, 3, 4, 8, 16. Défaut 1. Avec la sortie de
  Gaston à 1 coup par pas, GRID = 1 donne la double croche, 4 la noire.
- **SNAP** : 0 – 100 %, défaut 100 %. À 100 %, chaque note commence sur la grille. Plus bas, la fin du cycle n'est
  rapprochée de la grille que d'une partie de l'écart : on fond libre et cadencé.

### Comment Louis se cale, sans latence

1. Louis mesure la période de l'horloge : moyenne des quatre derniers intervalles, remise à zéro si un intervalle
   s'écarte de plus de 5 % (changement de tempo ; en deçà, la moyenne absorbe le jitter).
2. Au début d'un cycle, il connaît sa fin prévue (il vient d'en tirer la durée). Il cherche l'instant de grille prédit
   le plus proche de cette fin, au moins un pas de grille plus loin, et étire ou comprime le decay pour y tomber
   (l'attaque aussi si la fin tombe avant elle). À SNAP < 100 %, il ne parcourt qu'une partie de l'écart.
3. Le cycle se termine sur le **vrai coup d'horloge** quand le pas de grille en est un multiple : si le coup arrive un
   peu tôt, l'enveloppe finit tout de suite ; s'il arrive un peu tard, la voix attend à 0. Pour les pas plus courts
   qu'un coup (1/2, 1/3, 1/4), les instants intermédiaires sont prédits et recalés à chaque coup. Sur un tempo
   stable, l'erreur est nulle ; quand il change, elle se limite à l'erreur de prédiction.
4. Les débuts qui ne viennent pas d'une fin de cycle (STRIKE, RESET) attendent le prochain instant de grille, sauf si
   le dernier est passé depuis moins de 5 ms : la note part alors aussitôt. À SNAP < 100 %, ils partent toujours
   aussitôt. CHAIN part d'une fin de cycle, déjà sur
   la grille.
5. Tant que deux coups n'ont pas été mesurés, les notes attendent le coup suivant.
6. **Horloge arrêtée** (aucun coup depuis 4 périodes, et au moins 1 s) : les voix finissent leur note et attendent la
   reprise (WAIT, défaut). Au menu : FREE, les voix repartent en timing libre jusqu'au retour de l'horloge.

## Entrées

- **CV** (ajoutées au potard ; 10 V parcourent toute la course, ±5 V la moitié de part et d'autre) : PACE, TILT,
  DRIFT, WANDER, DENSITY, MEMORY, CORREL, GATE, RANGE, SKEW, LINK, SNAP.
- **SCALE** : 1 V par gamme, comme HARMONY sur Colette ; branchée, elle remplace le sélecteur.
- **GRID** : 1 V par cran, ajoutée au potard (borné aux neuf pas de grille).
- **ROOT** : 1 V/oct, ajoutée à la tonique ; la gamme suit (transposition de tout Louis). En FREE, simple addition.
- **SCALE IN** (poly) : voir Hauteur.
- **STRIKE** (poly, 4 canaux) : un front sur le canal N fait jouer une note à la voix N tout de suite (sur la grille
  si une horloge est branchée), quel que soit DENSITY. Mono branchée : frappe les quatre voix.
- **FREEZE** : gate ; haute, toutes les voix rejouent leur boucle à 100 %, comme HOLD.
- **CLOCK**, **RESET** : RESET relance les quatre voix (une note, quel que soit DENSITY ; boucle à sa première position) ; les dérives
  continuent. Horloge branchée, le coup qui suit RESET compte comme premier.

## Écran

- En haut, une ligne d'état : à droite la gamme et la tonique ; à gauche la valeur du dernier potard touché pendant
  1,5 s, sinon FREE TIMING ou la grille et SNAP quand une horloge est branchée.
- Une frise qui défile vers la gauche, une ligne par voix, sur 8 s (ou 32 pas de grille quand une horloge est
  branchée, avec les instants de grille en traits fins).
- Chaque note y laisse la silhouette de son enveloppe ; un point dans la ligne marque sa hauteur. Les silences ne
  laissent rien, les notes rejouées depuis la boucle sont plus pâles. Les liens se dessinent comme
  sur les pictogrammes : CHAIN, une flèche pleine vers la note déclenchée ; HUSH, des pointillés jusqu'à un soupir, et
  le cycle tu reste en fantôme pointillé ; ECHO, des pointillés de la note source à celle qui la reprend, les deux
  points de hauteur cerclés.

## Menu

- Forme de l'enveloppe : ANALOG (défaut) ou LINEAR.
- Longueur des boucles : 4 à 16 tirages, défaut 8.
- Horloge arrêtée : WAIT (défaut) ou FREE.

## Démarrage et sauvegarde

- Louis joue dès le chargement : pas de transport. Les voix démarrent décalées (chacune à une phase tirée au hasard).
- Tout est sauvé dans le patch : réglages, HOLD, réglages du menu, et le contenu des boucles (une boucle figée par
  HOLD ou MEMORY à 100 % se retrouve à la réouverture). Pas l'état des dérives ni du générateur aléatoire.

## Code et tests

- Le moteur vit dans `src/LouisCore.hpp`, sans dépendance au widget, comme Gaston et Lucienne ; banc d'essai
  `tests/test_louis.cpp`, lancé par `make -C tests run`.
- Tests minimaux :
  - aucun segment sous les planchers, quels que soient les réglages et les CV ;
  - DENSITY à 0 : aucune sortie ; à 100 % sans liens : aucun silence ;
  - MEMORY à 100 % : la boucle se répète à l'identique (hauteurs, choix note/silence) ;
  - quantification : chaque PITCH appartient à la gamme ; FREE donne des valeurs continues ; SCALE IN remplace le
    sélecteur ;
  - horloge stable à SNAP 100 % : chaque début de note tombe sur un coup (à un échantillon près), pour GRID 1, 2 et
    1/2 ; changement de tempo : retour sur la grille en moins de deux pas ;
  - CHAIN à 100 % : la période réfractaire est respectée, aucune voix ne joue deux notes à moins de 15 ms ;
  - GATE retombe avant chaque note suivante.

## Reporté en v2

- Gamme pondérée façon STEPS de Marbles (passer continûment du chromatique à la tonique seule).
- Déclenchement par une source externe (suiveur d'enveloppe sur du bruit, comme Barton) autrement que par STRIKE.
- Sortie d'horloge dérivée des notes.
- Liens configurables voix par voix (une matrice au lieu de l'anneau).
