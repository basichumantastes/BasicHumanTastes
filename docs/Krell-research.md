# Le patch Krell : recherche pour un générateur 4 voix (gate/trig + CV)

Recherche du 2026-10-22, en vue d'un module VCV Rack sans audio : 4 voix de gate/trig + CV inspirées du Krell,
modulables, vivantes seules, quantifiables en hauteur et calables sur une horloge externe.

Méthode : on remonte chaque affirmation à sa source la plus proche de Barton ou du fabricant. Niveaux de confiance
utilisés dans le texte :

- **[primaire]** : Todd Barton lui-même (descriptions de ses vidéos, ses propos filmés), Bebe Barron en interview.
- **[fabricant]** : manuels et pages produit (Buchla, Make Noise, 4ms, Mutable Instruments), code source publié.
- **[forum]** : fils de forum de première main (ModWiggler, VCV Community) qui documentent un patch précis.
- **[secondaire]** : articles, blogs, livres qui résument.

---

## 1. Qu'est-ce qu'un patch Krell

### 1.1 Le mécanisme, selon Barton

Barton décrit lui-même le « Krell de base » dans l'entretien filmé à Buchla & Friends (janvier 2026) **[primaire]**
([vidéo DivKid, transcription automatique consultée](https://www.youtube.com/watch?v=xY3qq9ZEhZg)) :

1. un **générateur de fonction attack/decay qui cycle** (sur Buchla, un canal de 281 en mode cyclique) ;
2. une **tension aléatoire fluctuante** (FRV, *fluctuating random voltage*) sur la CV d'attaque, **une autre FRV**
   sur la CV de decay ;
3. la sortie du générateur commande un **VCA** dans lequel passe un oscillateur ;
4. à chaque cycle, une impulsion du générateur **frappe une source aléatoire** (échantillonneur) dont la sortie
   va à la hauteur de l'oscillateur : une nouvelle hauteur par note.

Il résume le résultat : attaques douces et lentes, attaques dures, staccato, legato, longs decays. Et ailleurs, dans la
description qu'il a écrite pour Endorphin.es (2018), il désigne le cœur du patch : randomiser l'attaque et le
decay d'un générateur de fonction pour que chaque itération ait une enveloppe différente
([Endorphin.es, « The Krell Patch for the Shuttle System by Todd Barton »](https://www.youtube.com/watch?v=8To-zUeyWGc),
description citant Barton) **[primaire]**.

Point important pour la conception : **dans la version de Barton, les temps d'attaque et de decay ne sont pas
retirés au sort à chaque cycle**. Ils suivent en continu des FRV lentes ; c'est la hauteur qui est échantillonnée à
chaque fin de cycle. Le « nouveau temps tiré à chaque cycle » est une simplification répandue dans les
réimplémentations logicielles (crow *crell*, carte *Krell* du Workshop System, Hutchinson, webapp JS, voir §4), qui
donne un caractère plus haché, moins « respirant ».

### 1.2 Le patch d'origine (Buchla 200e, 2012)

Modules utilisés, d'après la description de la vidéo de montage par Barton : **261e** (Complex Waveform Generator),
**266e** (Source of Uncertainty), **281e** (Quad Function Generator), **292e** (Quad Dynamics Manager, VCA/LPG) et
**291e** (Triple Morphing Filter)
([Vimeo 48466272, « Building The Krell Muzak Patch », 2012-08-29](https://vimeo.com/48466272)) **[primaire]**.
Fonctions vérifiées sur les pages Buchla : [261e](https://buchla.com/product/261e/),
[292e](https://buchla.com/product/292e/) **[fabricant]**.

Le schéma le plus précis du câblage d'origine vient de **fhars** sur MuffWiggler, qui a suivi chaque câble sur une
photo du patch de Barton ([« Understanding the Krell Patch », t=159798](https://www.muffwiggler.com/forum/viewtopic.php?t=159798),
consulté via l'archive Wayback de 2019) **[forum, reconstitution d'après photo]** :

- **Voix** : un canal du 281e cycle avec attaque et decay courts (potentiomètres vers 0,3). Son enveloppe pilote le
  VCA (292e) ; son impulsion de fin de cycle (que fhars propose de lire comme « début du cycle suivant ») déclenche
  les **Stored Random Voltages** (SRV) du 266e, qui donnent la hauteur du 261e. Sortie : VCA puis filtre passe-bande.
- **Timbre** : un second canal du 281e, cyclant lentement (attaque et decay vers 3), sert de LFO sur le timbre du 261e.
- **SRV** : *range* et *chaos* au maximum.
- **Évolution lente** : deux **FRV** du 266e à des vitesses lentes et différentes. La plus lente module le **skew**
  des SRV (tendance vers les notes graves ou aiguës) ; l'autre module la fréquence centrale du filtre. Elles modulent
  aussi deux autres générateurs lents (l'une leur decay, l'autre leur attaque), lesquels modulent à leur tour
  l'attaque et le decay du générateur principal.
- Conséquence relevée par fhars : quand l'attaque de la voix s'allonge lentement, les SRV tirent plutôt des notes
  aiguës. **Corrélation temps ↔ hauteur** par construction (il doute qu'elle soit audible).

Un autre participant (carlosnyb) résume la « méthode Barton » en trois motifs à durée sous CV : deux sont déformés
par deux tensions variables différentes, le troisième est modulé par les deux premiers, sert d'enveloppe de dynamique
et, à sa fin de cycle, déclenche l'échantillonnage de la hauteur (même fil) **[forum]**.

Barton raconte qu'il est tombé dessus par hasard : deux générateurs de fonction en rétroaction, des heures à tourner
lentement attaque et decay, jusqu'à un point d'équilibre qui sonnait « organique »
([entretien 2026](https://www.youtube.com/watch?v=xY3qq9ZEhZg)) **[primaire]**.

### 1.3 Flux de signal (Krell monovoix canonique)

```
          FRV lente A ──► CV attaque ┐
          FRV lente B ──► CV decay   ├─► [AD cyclique] ──env──► VCA/LPG ◄── VCO ◄── hauteur
                                     │        │                    │            ▲
                                     │        └─EOC (fin de cycle)─┼──► S&H / SRV (skew ◄─ FRV lente)
                                     │                             ▼
  (option) AD lents imbriqués ──► attaque/decay du principal      filtre ◄── FRV (centre)
```

Variante documentée par [manual-synthesis.info, « Behold the Ancient Krell », 2017](https://web.archive.org/web/20181226024200/http://manual-synthesis.info/2017/01/26/krell/)
**[secondaire]** : l'EOC tire aussi une modulation FM aléatoire ; filtre passe-bande à centre aléatoire ; l'auteur
insiste sur le fait que **l'EOC cadence tout le patch** et recommande de **corréler** les tensions aléatoires entre
elles (hauteur, profondeur de FM, filtre) et de doser chaque aléa par un VCA. (Le domaine d'origine redirige
aujourd'hui vers un site sans rapport : seule l'archive est fiable.)

### 1.4 Le rôle des sources aléatoires (Buchla 266e)

D'après la page Buchla du [266e](https://buchla.com/product/266e/) **[fabricant]** :

- **Fluctuating Random Voltages** : variation continue, bande passante sous CV de 0,05 Hz à 50 Hz (dérive quasi
  imperceptible à fluctuation rapide). C'est le « smooth random » du Krell : il déforme les temps et le filtre.
- **Quantized Random Voltages** : change à chaque impulsion, nombre d'états sous CV de 2 à 24.
- **Stored Random Voltages** : trois paramètres sous CV, *degree* (quantité d'aléa), *chaos* (de légèrement
  incertain à chaos total), *skew* (penche vers un extrême). C'est le S&H « intelligent » de la hauteur.

Détail de circuit utile : sur le 266 d'origine, le S&H et les QRV se déclenchent sur front montant, mais les **SRV
sur front descendant** ([modularsynthesis.com, 266 Source of Uncertainty Module](https://www.modularsynthesis.com/roman/buchla_266/266sou.htm))
**[secondaire, technicien qui a construit le module]**. Avec une impulsion de fin de cycle très courte la différence
est minime, mais elle rappelle que « quand exactement on échantillonne » fait partie du son.

Le 281e : attaque et decay de 0,001 à 10 s chacun, somme CV + bouton, impulsion en fin de decay, mode cyclique qui
répète, mode quadrature (A et B décalés de 90°) ([page 281e](https://buchla.com/product/281e/)) **[fabricant]**.

### 1.5 La hauteur

- Barton ne parle pas de quantification : l'aléa est envoyé directement à l'oscillateur
  ([entretien 2026](https://www.youtube.com/watch?v=xY3qq9ZEhZg)) **[primaire]**. Les SRV du 266e produisent une
  tension continue ; la lecture courante est donc **une voix non quantifiée**. fhars écrit pourtant que les SRV
  donnent une « valeur de hauteur quantifiée » (fil MuffWiggler cité) **[forum]**. Non tranché (voir §6).
- Dans sa démonstration du 266t (2022), Barton envoie les **QRV** (quantifiées) vers la forme d'onde, pas vers la
  hauteur ([« Tiptop 266t part 4 Basic Krell »](https://www.youtube.com/watch?v=f4O3oFneUvE)) **[primaire]**.
- La quantification apparaît dans les dérivés : Workshop System *Krell* (octaves ou chromatique), forum VCV
  (« VCV Random quantisé » échantillonné), chorus d'accords chez J. D. Cramer (voir §3 et §4).

### 1.6 Le caractère

Barton : un mouvement perpétuel qui change sans cesse ; des choses apparaissent et disparaissent, des rafales, des
arabesques ([entretien 2026](https://www.youtube.com/watch?v=xY3qq9ZEhZg)) ; ailleurs un « kaléidoscope sonore » qui
lui rappelle la musique concrète des années 1950 ([description Endorphin.es](https://www.youtube.com/watch?v=8To-zUeyWGc))
**[primaire]**. Sur MuffWiggler, MacroDX résume ce que le patch cherche : de l'aléa, mais avec un motif qui revient
(fil t=159798) **[forum]**. Les lecteurs y entendent les « bloops » de science-fiction ; c'est une perception
commune, pas une intention déclarée de Barton.

---

## 2. Contexte historique

### 2.1 Les Barron et *Forbidden Planet* (1956)

- Louis et Bebe Barron signent ce qui est considéré comme la première partition entièrement électronique d'un film.
  Louis construisait des circuits d'après *Cybernetics* de Norbert Wiener (1948) ; un circuit par thème ou leitmotiv
  (notes de pochette du LP, 1956) ; le crédit voulu (« Electronic Music ») devient « Electronic Tonalities » sous la
  pression du syndicat des musiciens ([Wikipédia, Bebe and Louis Barron](https://en.wikipedia.org/wiki/Bebe_and_Louis_Barron),
  qui cite NPR 2005 et UPI 2008) **[secondaire]**.
- Bebe Barron (entretien avec Jane Brockman, *The Score*, 1992) : chaque circuit « prenait vie », montait, atteignait
  un plateau, culminait puis mourait par surcharge, sans pouvoir être relancé ; elle y voit l'entropie d'un système
  clos, et revendique le goût de l'aléa et de la probabilité (Cage, cybernétique) ; la musique se composait ensuite
  au montage de bandes ([reproduction Effectrode](https://www.effectrode.com/knowledge-base/the-first-electronic-filmscore-forbidden-planet-a-conversation-with-bebe-barron/))
  **[primaire, entretien]**. L'entretien ne parle pas spécifiquement de la « musique des Krell ».
- Barton reprend ce récit en 2026 : un premier circuit muet pendant une heure, puis un son qui grossit, bifurque et
  explose. Il précise que **son patch ne traverse pas ce cycle de vie** : il nomme le patch en hommage, sans chercher
  à imiter ([entretien 2026](https://www.youtube.com/watch?v=xY3qq9ZEhZg)) **[primaire]**.

À retenir pour le design : le Krell de Barton est un **système stationnaire qui change sans cesse** ; l'idée Barron
d'un **arc** (naissance, plateau, mort) est une piste distincte, que le Krell classique n'a pas.

### 2.2 Todd Barton et la diffusion

| Date | Jalon | Source |
|---|---|---|
| 2012-08-28 | « Krell Muzak the patch » (Vimeo, 65 s) ; enregistrements *Krell Muzak 1* (7 min) et *2* sur SoundCloud | [Vimeo 48382205](https://vimeo.com/48382205), [MATRIXSYNTH](https://www.matrixsynth.com/2012/08/krell-muzak-by-todd-barton-on-buchla.html) **[primaire]** |
| 2012-08-29 | « Building The Krell Muzak Patch », montage câble par câble sur 200e | [Vimeo 48466272](https://vimeo.com/48466272) **[primaire]** |
| 2013-10-29 | « Easel Krell Patch i » (Music Easel) ; Barton y appelle la version 200e « my original Krell patch » | [YouTube jioAtlkkVyw](https://www.youtube.com/watch?v=jioAtlkkVyw) **[primaire]** |
| avant 2019 | MuffWiggler « Understanding the Krell Patch » (schéma de fhars), et un fil « Krell » plus ancien (t=68872) | [t=159798](https://www.muffwiggler.com/forum/viewtopic.php?t=159798) **[forum]** |
| 2017 | Make Noise, *0-Coast Patch of the Week* #11 : Krell | [MATRIXSYNTH](https://www.matrixsynth.com/2017/04/0-coast-patch-of-week-11-krell.html) **[fabricant, relayé]** |
| 2018 | Krell sur Hordijk ; préréglage Krell pour l'Endorphin.es Shuttle System | [YouTube SUtSeCn4p1U](https://www.youtube.com/watch?v=SUtSeCn4p1U), [YouTube 8To-zUeyWGc](https://www.youtube.com/watch?v=8To-zUeyWGc) **[primaire]** |
| 2018-02 | Learning Modular (Chris Meyer) : glossaire, renvoie à Barton | [learningmodular.com](https://learningmodular.com/glossary/krell-patch/) **[secondaire]** |
| 2019-03 | VCV Community : « Let's build a Krell Patch in VCV Rack » (Omri Cohen) | [fil 2614](https://community.vcvrack.com/t/lets-build-a-krell-patch-in-vcv-rack/2614) **[forum]** |
| 2021-02 | Krell sur Easel Command (+ Mimeophon, Planar 2) | [MATRIXSYNTH](https://www.matrixsynth.com/2021/02/buchla-easel-command-exploration-6.html) **[primaire, relayé]** |
| 2022-04 | « Quad Krell » : 4 oscillateurs, 4 générateurs de fonction, 4 LPG | [YouTube KsaiGyT66X0](https://www.youtube.com/watch?v=KsaiGyT66X0) **[primaire]** |
| 2022-11/12 | Tiptop/Buchla 266t : Krell auto-patché, puis tutoriel « Basic Krell » | [QfS2hayLe84](https://www.youtube.com/watch?v=QfS2hayLe84), [f4O3oFneUvE](https://www.youtube.com/watch?v=f4O3oFneUvE) **[primaire]** |
| 2023 | A. Olney, *Computational Thinking through Modular Sound Synthesis*, chap. 14 (recréation VCV) | [olney.ai](https://olney.ai/ct-modular-book/krell.html) **[secondaire]** |
| 2024-02 | Frap Tools : « At last: the Krell Patch on a Frap Tools modular system » | [frap.tools](https://frap.tools/at-last-the-krell-patch-on-a-frap-tools-modular-system/) **[fabricant, blog]** |
| 2026-01-19 | Barton à Buchla & Friends : origines, bases, extensions | [YouTube xY3qq9ZEhZg](https://www.youtube.com/watch?v=xY3qq9ZEhZg) **[primaire]** |

Le nom a semé la confusion : sur MuffWiggler, un participant s'étonne que « Krell » désigne un patch Buchla et non la
musique de 1956 ; réponse : l'original était de la musique sur bande, le patch en reprend le son et la démarche
compositionnelle (fil t=159798) **[forum]**.

---

## 3. Variantes et extensions connues

- **Plusieurs voix qui interagissent.** Pour Barton, l'étape deux est d'en construire un second et de les faire
  interagir, ou de renvoyer les tensions des générateurs (inversées, etc.) vers filtres, timbres et autres hauteurs
  ([entretien 2026](https://www.youtube.com/watch?v=xY3qq9ZEhZg)) **[primaire]**. *Quad Krell* (2022) : quatre
  chaînes oscillateur → générateur de fonction → LPG ; la description ne dit pas comment elles se croisent
  ([vidéo](https://www.youtube.com/watch?v=KsaiGyT66X0)) **[primaire, détail non documenté]**.
- **Deuxième voix dérivée de la première.** J. Daniel Cramer (Dcramer) : version eurorack à deux voix, la seconde
  joue des **accords dont fondamentale et qualité dérivent de la mélodie**, mais changent moins souvent ; puis une
  voix mélodique plus complexe et un « Form Engine » qui fait évoluer tout le patch ; projet de Krell entièrement
  polyphonique (fil MuffWiggler t=159798) **[forum, première main]**. Ce travail a abouti aux « Krellberg
  Variations » (patch de 467 câbles, quatre albums, d'après le
  [fil VCV de Rofilm, 2022](https://community.vcvrack.com/t/transferring-j-daniel-cramer-s-huge-generative-polykrell-patch-to-vcv-rack/17580))
  **[forum, secondaire]**.
- **Déclenchement par autre chose qu'un cycle.** Barton déclenche aussi par une **FRV utilisée comme déclencheur**
  (266t, 2022) et par un **suiveur d'enveloppe sur du bruit blanc** qui déclenche générateur et QRV
  ([« Noise Triggered Krell », 2022](https://www.youtube.com/watch?v=LtXaKwzPHHo)) **[primaire]**. Le temps entre
  notes devient alors indépendant de la forme de l'enveloppe.
- **Densité / probabilité.** Pas chez Barton. La carte Workshop System *Krell* a un bouton « chance d'enveloppes plus
  longues » qui agit comme une humeur calme/frénétique (§4) ; Marbles règle la probabilité de répéter le passé
  (DEJA VU) et la répartition des gates (BIAS) ([manuel Marbles](https://pichenette.readthedocs.io/en/latest/modules/marbles/manual/))
  **[fabricant]**.
- **Quantification.** Voir §1.5 et §5.3 : présente dans les dérivés (Workshop *Krell*, patch VCV de safetycopy avec
  VCV Random quantisé ([fil VCV 22224](https://community.vcvrack.com/t/krell-patch-in-vcv-rack/22224))
  **[forum]**), pas documentée chez Barton.
- **Krell cadencé.** Une horloge déclenche l'échantillonnage au lieu de l'EOC, ou l'enveloppe est en multiples de
  l'horloge (4ms PEG, §5.4). Un participant VCV ajoute une **voix toutes les 4 notes** via un commutateur 1→4
  ([fil VCV 22224](https://community.vcvrack.com/t/krell-patch-in-vcv-rack/22224)) **[forum]**.
- **Logique, cascades.** Le manuel Maths propose de croiser les deux canaux : EOR ou EOC de l'un vers Trigger, Signal
  et Cycle de l'autre, et des patches « Bouncing Ball », « trill » (l'EOR d'un canal active le cycle de l'autre)
  ([manuel Maths 2013](https://www.makenoisemusic.com/wp-content/uploads/2024/03/MATHSmanual2013.pdf)) **[fabricant]**.
  Le 281e a un mode quadrature où A déclenche B (page 281e) **[fabricant]**.
- **Chaos au lieu d'aléa.** Le script norns *krill* remplace l'aléa par un **attracteur de Lorenz** (entre aléatoire
  et prévisible) et a un mode « krell » où chaque note naît du montée/descente d'une enveloppe
  ([README krill](https://github.com/jaseknighter/krill)) **[fabricant/auteur]**.

---

## 4. Implémentations dédiées au Krell (modules, cartes, scripts)

### 4.1 VCV Rack

**Aucun module de la VCV Library ne porte le nom Krell ni ne se décrit comme tel.** Vérifié le 2026-10-22 sur les
574 manifestes du dépôt officiel [VCVRack/library](https://github.com/VCVRack/library) (noms, descriptions, tags ;
dernier commit du 2026-10-22). Des modules hors Library ou des manuels qui mentionnent Krell sans le dire dans leur
description ont pu échapper à cette recherche.

Ce qui existe côté VCV, ce sont des **patches** et des **briques** :

- Patches : Omri Cohen (vidéo, 2019) et *Krell Evolution.vcv* d'Ananda
  ([fil 2614](https://community.vcvrack.com/t/lets-build-a-krell-patch-in-vcv-rack/2614)) ; DivKid, patch VCV gratuit
  « 3 voice, polyphonic musical Krell » pour la démonstration du NANO Modules VCV Random
  ([post ModWiggler, 2024-03](https://www.modwiggler.com/forum/viewtopic.php?t=282573), lien
  [gumroad](https://divkid.gumroad.com/l/vcvrandom)) **[forum]** ; recréation pas à pas dans le livre d'Olney (Bogaudio
  AD, holdah, mixah…) ([chap. 14](https://olney.ai/ct-modular-book/krell.html)) **[secondaire]**.
- Briques citées par la communauté ou proches du 266/281 (descriptions tirées des manifestes de la Library) :
  - **Befaco Rampage** (rampes doubles, EOC) : base courante d'un Krell VCV (fils VCV cités) ;
    **Frozen Wasteland Drunken Rampage** (Rampage probabiliste, dépassement aléatoire de la cible)
    ([README](https://github.com/almostEric/FrozenWasteland)).
  - **NYSTHI SoyModelSOU** (« Source of uncertainty Buchla266/Doepfer149 wannabe ») ; **tapestry Drift** (random
    fluctuant inspiré des Source of Uncertainty, avec « glissements » occasionnels)
    ([source](https://github.com/shortwavlabs/vcv-tapestry)).
  - **Audible Instruments Marbles** (« Random Sampler », portage du firmware Mutable), **Tides**, **Stages**
    ([AudibleInstruments](https://github.com/VCVRack/AudibleInstruments)).
  - **VCV Random** (Fundamental), dont NANO Modules a fait une version matérielle.
  - **CVfunk Impulse Controller** (enveloppes de decay qui se propagent dans un réseau de 24 nœuds : modèle de
    cascade) ([source](https://github.com/codygeary/CVfunk-Modules)).
  - **Jules** (Basic Human Tastes) : six pentes liées avec chance sur la cascade de déclenchement
    ([plugin.json local](../plugin.json)). À garder en tête pour ne pas doublonner.

### 4.2 Matériel et scripts dédiés

| Nom | Auteur | Ce que ça fait | Points forts | Limites | Source |
|---|---|---|---|---|---|
| **Krell** (carte n° 56 du Music Thing Workshop System Computer) | Benjamin Reily, 2026, statut beta / draft | 2 canaux indépendants : enveloppe AD en boucle (attaque et decay retirés au sort à chaque cycle), CV de hauteur aléatoire, impulsion de fin de cycle. Bouton principal = tendance aux enveloppes longues (humeur), X et Y = longueur par canal. Inter : octaves / chromatique / plage 1 à 3 octaves. Entrées CV = S&H quantifié à la place de l'aléa interne | Seul module matériel trouvé qui se nomme Krell ; très simple ; quantif. intégrée | Le code publié utilise X pour les **deux** canaux (Y lu mais pas utilisé) et partage les variables de temps entre canaux ; minima de temps proches de 0 (risque de boucle très rapide) ; pas d'horloge externe | [README](https://github.com/TomWhitwell/Workshop_Computer/blob/main/releases/56_Krell/README.md), [Krell_1.0.lua](https://github.com/TomWhitwell/Workshop_Computer/blob/main/releases/56_Krell/Krell_1.0.lua) **[fabricant/code]** |
| **crell** (script monome crow) | Dan Sim, 2020 | Enveloppe en boucle, attaque et decay tirés à chaque cycle (1 à 500 ms autour d'une médiane), deux sorties aléatoires (±2 V hauteur, 0–10 V mod), impulsion d'horloge ; entrées : bascule attaque/decay (« tilt ») et longueur totale (plancher 50 ms) | Paramétrage « tilt + longueur » élégant ; temps arrondis au ms supérieur (jamais 0) | Hauteur non quantifiée, une voix | [crell.lua](https://github.com/dansimco/crell) **[code]** |
| **krill** (script norns) | jaseknighter, idée de Matt Allison | Séquenceur à attracteur de Lorenz ; mode *krell* (notes nées de l'enveloppe, rise/fall en ms, nombre d'octaves) et mode *vuja de* inspiré de Marbles ; gammes et quantification | Chaos déterministe plutôt qu'aléa ; gammes | Environnement norns, pas un module | [README](https://github.com/jaseknighter/krill) **[code]** |
| **Krell patch pour Shuttle System** | Barton / Endorphin.es, 2018 | Préréglage pour Shuttle Control + tutoriel | Version Barton sur matériel eurorack | Patch, pas un module | [vidéo et liens](https://www.youtube.com/watch?v=8To-zUeyWGc) **[primaire]** |
| Webapp JS, patch Pd pour Digitone | robschmidtdev, Elektromatic | Notes tirées dans une gamme pentatonique, intervalle entre notes tiré au sort | Démo simple | `Math.random() * cycleSpread` peut donner 0 (pas de plancher) | [krellpatch](https://github.com/robschmidtdev/krellpatch), [patchKrell](https://github.com/Elektromatic/patchKrell) **[code]** |

Pas de mode « Krell » trouvé dans les firmwares Disting, Hemisphere ou Plaits (recherche web seulement, manuels non
dépouillés). Pas de module eurorack commercial nommé Krell trouvé (le « Krell display » cité sur Mastodon est un
afficheur DIY sans rapport).

### 4.3 Modules matériels qui servent de référence de comportement

- **Make Noise Maths** : EOR (canal 1) et EOC (canal 4) en 0/10 V ; l'EOC est **haut au repos** ; Maths se
  redéclenche pendant la descente mais **pas pendant la montée** ; fonctions de 25 min à 1 kHz ; CV « Both » inverse
  des CV Rise/Fall ([manuel 2013](https://www.makenoisemusic.com/wp-content/uploads/2024/03/MATHSmanual2013.pdf))
  **[fabricant]**. C'est le générateur le plus utilisé pour les Krell eurorack (fils ModularGrid, Gearspace).
- **4ms PEG** : enveloppes en divisions/multiplications d'une horloge « ping » (/8 à ×8) ; jack **QNT** = l'enveloppe
  attend le prochain coup d'horloge ; jack **Async** = départ immédiat ; en mode Cycle, à l'activation, le PEG calcule
  où se placer dans son cycle pour **finir sur le prochain ping** ([manuel v2011-11-25](https://www.analoguehaven.com/4ms/pingable-envelope-generator/manual.pdf))
  **[fabricant]**.
- **Mutable Instruments Marbles** (et son portage VCV) : horloge aléatoire t (jitter, BIAS, modes), tensions X (SPREAD
  en distribution, BIAS, STEPS qui va du lissé au quantifié puis retire des notes par saillance), DEJA VU et LENGTH
  pour la répétition ; horloge externe : RATE devient division/multiplication et le jitter s'applique à l'horloge
  entrante ([manuel](https://pichenette.readthedocs.io/en/latest/modules/marbles/manual/)) **[fabricant]**.

---

## 5. Enseignements pour la conception

### 5.1 Les paramètres qui comptent

1. **Échelles de temps.** 281e : 1 ms à 10 s par segment ; Maths : jusqu'à 25 min. Valeurs de départ observées : voix
   principale vers 0,25–0,3 (Barton d'après fhars ; Olney : 0,25 s), modulateurs lents vers 3 s
   ([Olney](https://olney.ai/ct-modular-book/krell.html), fil fhars) ; crow *crell* : 1–500 ms par segment ; carte
   Workshop : 0 à ~1 s selon les boutons. Proposition : attaque 2 ms–5 s, decay 5 ms–20 s, tirage **en domaine
   logarithmique** (l'oreille perçoit les rapports de durée), avec **plancher** strict.
2. **Deux niveaux d'aléa, pas un.** (a) des sources **lentes et continues** (FRV) qui déplacent le *centre* des temps,
   du skew de hauteur, du timbre ; (b) un **échantillonnage par cycle** (S&H) pour la hauteur. Barton n'a que (a) sur
   les temps ; ajouter un peu de (b) sur les temps est une option, pas le cœur.
3. **Profondeur d'aléa par paramètre** (attaque, decay, hauteur, CV auxiliaire), plus **skew/bias** (266e *skew*,
   Marbles BIAS) et **forme de distribution** (Marbles SPREAD : constante → cloche → uniforme → extrêmes).
4. **Corrélations.** La source qui penche la hauteur est aussi celle qui allonge l'attaque (patch d'origine selon
   fhars) ; corréler hauteur, FM et filtre (manual-synthesis). Un réglage « corrélation temps ↔ hauteur »
   (aigu = court, grave = lent, ou l'inverse) est fidèle à l'esprit et audible.
5. **Rapport attaque/decay** séparé de la **durée totale** : c'est le modèle « tilt + longueur » de *crell*, et la
   CV « Both » de Maths. Deux boutons plus parlants que quatre.
6. **Mémoire / récurrence** : sans répétition, l'aléa uniforme devient un brouillard. DEJA VU de Marbles (probabilité de
   rejouer une décision passée, boucles de 5 à 14 valeurs par décision) est le modèle à suivre.

### 5.2 Pièges

- **Boucle qui cale.** En matériel, une boucle EOC → trigger meurt si une impulsion est perdue. Maths ignore les
  déclenchements pendant la montée et son EOC est haut au repos : un canal auto-patché qui n'a jamais démarré ne voit
  pas de front montant ([manuel Maths](https://www.makenoisemusic.com/wp-content/uploads/2024/03/MATHSmanual2013.pdf)).
  Barton le dit à sa façon : « What if I do this? Oh, I killed it. » ([entretien 2026](https://www.youtube.com/watch?v=xY3qq9ZEhZg)).
  En logiciel : **le cycle se relance en interne** (pas par un câble), avec un démarrage automatique et un garde-fou
  (si aucune voix n'a émis depuis T, en relancer une).
- **Segments de durée nulle.** Un temps tiré à 0 donne une boucle à la fréquence d'échantillonnage (bourdonnement
  audio, rafale de trigs) : c'est le cas potentiel de la carte Workshop (`math.random() * …`) et de la webapp JS ;
  *crell* l'évite par un arrondi au ms supérieur et un plancher de 50 ms sur la longueur. Imposer un plancher par
  segment et une **période réfractaire** minimale entre deux trigs d'une même voix.
- **Interblocage entre voix.** Inhibitions croisées (A coupe B, B coupe A) : tout se tait. Déclenchements croisés à
  probabilité 1 : emballement, toutes les voix se synchronisent et le Krell devient un séquenceur. Remèdes : période
  réfractaire (comme Maths qui n'écoute pas pendant la montée), probabilités < 1, délais de propagation, garde-fou
  global de densité.
- **Monotonie.** Un seul niveau d'aléa uniforme sonne pareil au bout d'une minute. Il faut des dérives lentes du centre
  et de la distribution (le skew du patch d'origine), des variations de densité (fondus, rafales, comme le décrit
  Barton) et de la récurrence (DEJA VU).
- **Quand échantillonner.** Échantillonner la hauteur **à la fin du cycle, avant la note suivante** et la tenir pendant
  toute la note ; front montant ou descendant change la phase (SRV sur front descendant, §1.4).

### 5.3 Quantifier la hauteur

- **Tirer, puis quantifier, puis tenir** : la quantification se fait au moment de l'échantillonnage, la CV ne bouge
  plus pendant la note (comme la carte Workshop : S&H puis quantif).
- **Gamme pondérée** plutôt que liste plate : Marbles STEPS retire les notes d'une gamme par ordre de saillance
  (apprise en jouant 50 notes ou plus), ce qui permet de passer continûment de chromatique à fondamentale seule
  ([manuel Marbles](https://pichenette.readthedocs.io/en/latest/modules/marbles/manual/)).
- **Plage en octaves** réglable (carte Workshop : 1 à 3 octaves ; *krill* : nombre d'octaves) et **skew** de la
  distribution plutôt que simple offset (Marbles BIAS est un décalage probabiliste, pas un décalage de tension).
- Garder un mode **non quantifié**, fidèle à Barton.

### 5.4 Caler des trigs libres sur une horloge externe

Trois stratégies observées :

1. **Retarder au prochain coup d'horloge** (4ms PEG, jack QNT : l'enveloppe attend le prochain ping). Simple et
   déterministe, mais **toujours en retard**, jusqu'à une période d'horloge, et un trig libre peut sauter un temps.
2. **Prédire l'horloge et se caler en phase.** Marbles extrait une rampe de phase de l'horloge entrante en **prédisant
   la période suivante** à partir des 16 dernières impulsions (moyennes glissantes lente et rapide, table de hachage,
   détection de motifs de période 1 à 10) ([ramp_extractor.h](https://github.com/pichenettes/eurorack/blob/master/marbles/ramp/ramp_extractor.h)) ;
   son jitter accumule l'écart de phase avec l'horloge droite et pousse l'horloge jitterée à **rattraper son retard**
   d'autant plus que l'écart est grand ([t_generator.cc](https://github.com/pichenettes/eurorack/blob/master/marbles/random/t_generator.cc),
   commentaire du code) **[code fabricant]**. Le PEG fait pareil à l'activation du cycle : il calcule où se placer
   pour finir sur le prochain ping (manuel PEG). Maths, à l'inverse, rappelle qu'un circuit ne peut pas « voir dans le
   futur » d'un signal externe (manuel Maths).
3. **Exprimer les durées en unités d'horloge.** PEG : longueur d'enveloppe = ping × (1/8 … 8). On tire un nombre entier
   de pas pour attaque + decay, et la forme s'étire pour tenir dedans : le Krell est cadencé par construction.

Enseignement propre à un Krell : **le module connaît la fin de chaque cycle dès son début**, puisqu'il fixe lui-même
les temps d'attaque et de decay. Il n'a donc pas besoin d'attendre pour « quantifier au plus proche » : au départ du
cycle, il prévoit l'instant de l'EOC, cherche le coup d'horloge prédit le plus proche (période mesurée ou prédite à la
Marbles) et **étire ou comprime légèrement le decay** pour tomber dessus. Pas de latence ; il ne reste que l'erreur
de prédiction quand le tempo change. Un réglage « force du calage » (0 = libre, 1 = sur la grille) permet de
fondre libre et cadencé, comme le jitter de Marbles fond horloge droite et chaos. La stratégie 1 reste utile en
secours (horloge irrégulière, premier coup après un arrêt).

---

## 6. Ce qui n'a pas pu être vérifié

- **Le câblage exact de 2012** : les vidéos Vimeo n'ont pas pu être visionnées (contrôle anti-robot) ; seules leurs
  métadonnées (titres, dates, descriptions) ont été lues via l'API oEmbed. Le détail vient du schéma de fhars (photo
  du patch), non confirmé par Barton.
- **Hauteur quantifiée ou non dans l'original** : contradiction entre fhars (« quantifiée ») et les SRV du 266e
  (continues) ; Barton n'en parle pas.
- **Les interactions dans *Quad Krell*** : la description ne dit pas si les voix se déclenchent entre elles.
- **Le tutoriel « Origins » et les vidéos YouTube** n'ont été lus que par leurs transcriptions automatiques (qualité
  moyenne, ex. « Bller » pour Buchla) ; le sens général est clair, pas chaque mot.
- **La date exacte du fil de fhars** et le contenu du fil ModWiggler t=207670 (cité ailleurs comme affirmant que le
  Krell d'origine est une voix non quantifiée) : non accessibles (Cloudflare, pas d'archive).
- **llllllll.co** : aucun fil sur le Krell trouvé par la recherche web.
- **Modules VCV hors Library** ou dont seul le manuel mentionne Krell : non couverts.

---

## Sources

| Source | Niveau | Note |
|---|---|---|
| [YouTube xY3qq9ZEhZg](https://www.youtube.com/watch?v=xY3qq9ZEhZg), Barton à Buchla & Friends (DivKid, 2026-01-19) | primaire | Propos de Barton : origine du nom, Krell de base, étape deux ; lu par transcription automatique |
| [YouTube 8To-zUeyWGc](https://www.youtube.com/watch?v=8To-zUeyWGc), Endorphin.es, 2018 | primaire | Description écrite par Barton |
| [Vimeo 48382205](https://vimeo.com/48382205) et [48466272](https://vimeo.com/48466272), Barton, 2012 | primaire | Métadonnées oEmbed (dates, modules) |
| Vidéos YouTube de Barton : [jioAtlkkVyw](https://www.youtube.com/watch?v=jioAtlkkVyw), [KsaiGyT66X0](https://www.youtube.com/watch?v=KsaiGyT66X0), [f4O3oFneUvE](https://www.youtube.com/watch?v=f4O3oFneUvE), [QfS2hayLe84](https://www.youtube.com/watch?v=QfS2hayLe84), [LtXaKwzPHHo](https://www.youtube.com/watch?v=LtXaKwzPHHo), [SUtSeCn4p1U](https://www.youtube.com/watch?v=SUtSeCn4p1U) | primaire | Descriptions (et une transcription) |
| [Effectrode, entretien Bebe Barron](https://www.effectrode.com/knowledge-base/the-first-electronic-filmscore-forbidden-planet-a-conversation-with-bebe-barron/) (*The Score*, 1992) | primaire | Entretien reproduit |
| [Buchla 266e](https://buchla.com/product/266e/), [281e](https://buchla.com/product/281e/), [292e](https://buchla.com/product/292e/), [261e](https://buchla.com/product/261e/) | fabricant | Pages produit |
| [Manuel Make Noise Maths 2013](https://www.makenoisemusic.com/wp-content/uploads/2024/03/MATHSmanual2013.pdf) | fabricant | Citations vérifiées dans le PDF officiel |
| [Manuel 4ms PEG](https://www.analoguehaven.com/4ms/pingable-envelope-generator/manual.pdf) (v2011-11-25) | fabricant | Copie hébergée par un revendeur |
| [Manuel Marbles](https://pichenette.readthedocs.io/en/latest/modules/marbles/manual/) | fabricant | Documentation de l'auteur (Émilie Gillet) |
| [pichenettes/eurorack, Marbles](https://github.com/pichenettes/eurorack/tree/master/marbles) | fabricant (code) | ramp_extractor, t_generator |
| [Workshop Computer, carte 56 Krell](https://github.com/TomWhitwell/Workshop_Computer/tree/main/releases/56_Krell) | fabricant (code) | Beta / draft, 2026 |
| [crell](https://github.com/dansimco/crell), [krill](https://github.com/jaseknighter/krill), [krellpatch](https://github.com/robschmidtdev/krellpatch), [patchKrell](https://github.com/Elektromatic/patchKrell) | code tiers | Implémentations lues |
| [VCVRack/library](https://github.com/VCVRack/library) | fabricant (registre) | Manifestes au 2026-10-22 |
| [MuffWiggler t=159798](https://www.muffwiggler.com/forum/viewtopic.php?t=159798) (fhars, Dcramer, carlosnyb, MacroDX) | forum | Reconstitution du patch d'après photo ; propos de Cramer en première main |
| [VCV Community 2614](https://community.vcvrack.com/t/lets-build-a-krell-patch-in-vcv-rack/2614), [22224](https://community.vcvrack.com/t/krell-patch-in-vcv-rack/22224), [17580](https://community.vcvrack.com/t/transferring-j-daniel-cramer-s-huge-generative-polykrell-patch-to-vcv-rack/17580) | forum | Patches VCV |
| [ModWiggler t=282573](https://www.modwiggler.com/forum/viewtopic.php?t=282573) (DivKid, NANO VCV Random) | forum | Lien vers le patch VCV 3 voix |
| [modularsynthesis.com, 266](https://www.modularsynthesis.com/roman/buchla_266/266sou.htm) | secondaire (technicien) | Détails de circuit (front des SRV) |
| [manual-synthesis.info (archive)](https://web.archive.org/web/20181226024200/http://manual-synthesis.info/2017/01/26/krell/) | secondaire | Flux de signal simplifié ; domaine d'origine détourné |
| [Olney, chap. 14](https://olney.ai/ct-modular-book/krell.html) | secondaire | Recréation VCV détaillée |
| [Learning Modular](https://learningmodular.com/glossary/krell-patch/), [MATRIXSYNTH](https://www.matrixsynth.com/2012/08/krell-muzak-by-todd-barton-on-buchla.html), [Frap Tools](https://frap.tools/at-last-the-krell-patch-on-a-frap-tools-modular-system/), [Wikipédia, Barron](https://en.wikipedia.org/wiki/Bebe_and_Louis_Barron) | secondaire | Contexte et diffusion |
