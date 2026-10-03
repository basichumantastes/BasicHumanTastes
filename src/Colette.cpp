#include "plugin.hpp"
#include "widgets.hpp"
#include "harmony.hpp"

// Colette : une murmuration. Des oiseaux (2 à 32) volent dans l'espace des hauteurs : leur altitude est
// leur hauteur, leur place de gauche à droite leur panoramique. Ils se tiennent ensemble (COHESION),
// s'écartent pour ne pas se toucher (SCATTER) et sont attirés par des perchoirs, les notes d'un accord
// réparties sur plusieurs octaves (HARMONY, ROOT, RANGE, PULL). Un oiseau posé chante une note pure ;
// en vol il a un souffle, comme un bruit d'ailes. Le vent (WIND, GUST) les fait décoller, l'accord se
// défait en nuée puis se reforme. Chaque oiseau qui se pose envoie un trigger (LAND) et sa note (NOTE).
// Le son passe dans un petit espace stéréo (SPACE). FREEZE suspend le vol.


static const int MAX_BIRDS = 32;
static const int CONTROL_DIVIDER = 16;
static const int TAIL = 8;

struct Bird {
	float y = 0.f, x = 0.f;      // hauteur (octaves au-dessus de ROOT), panoramique (-1..1)
	float vy = 0.f, vx = 0.f;
	float windY = 0.f, windX = 0.f;
	bool landed = false;
	float perch = 0.f;
	float airborne = 0.f;        // temps de vol obligatoire après un décollage (s)
	float amp = 0.f;             // volume lissé par BLOOM
	float breath = 0.f;          // part de souffle (en vol)
	float phase = 0.f;
	float vibratoPhase = 0.f, vibratoRate = 5.f;
	float bpLow = 0.f, bpBand = 0.f;  // filtre du souffle
	float tailY[TAIL] = {}, tailX[TAIL] = {};
};


// Petit espace stéréo : quatre lignes de retard bouclées (matrice de Hadamard), amorties, un peu modulées
struct Space {
	std::vector<float> lines[4];
	int pos[4] = {};
	float damp[4] = {};
	float lfo = 0.f;
	void init(float sr) {
		static const float MS[4] = {37.1f, 53.3f, 71.9f, 89.7f};
		for (int k = 0; k < 4; k++) {
			lines[k].assign((size_t) (MS[k] * 0.001f * sr * 2.f) + 4, 0.f);
			pos[k] = 0;
			damp[k] = 0.f;
		}
	}
	void process(float inL, float inR, float amount, float sr, float& outL, float& outR) {
		static const float MS[4] = {37.1f, 53.3f, 71.9f, 89.7f};
		lfo += 0.13f / sr;
		if (lfo >= 1.f)
			lfo -= 1.f;
		float feedback = 0.62f + 0.33f * amount;
		float taps[4];
		for (int k = 0; k < 4; k++) {
			int size = (int) lines[k].size();
			float d = MS[k] * 0.001f * sr * (1.f + 0.6f * amount) * (1.f + 0.004f * std::sin(2.f * M_PI * (lfo + 0.25f * k)));
			float rp = pos[k] - d;
			while (rp < 0.f)
				rp += size;
			int i0 = (int) rp;
			float t = rp - i0;
			float a = lines[k][i0 % size], b = lines[k][(i0 + 1) % size];
			taps[k] = a + (b - a) * t;
			damp[k] += 0.35f * (taps[k] - damp[k]);
			taps[k] = damp[k];
		}
		// Hadamard 4×4 normalisée
		float h0 = 0.5f * (taps[0] + taps[1] + taps[2] + taps[3]);
		float h1 = 0.5f * (taps[0] - taps[1] + taps[2] - taps[3]);
		float h2 = 0.5f * (taps[0] + taps[1] - taps[2] - taps[3]);
		float h3 = 0.5f * (taps[0] - taps[1] - taps[2] + taps[3]);
		float back[4] = {h0, h1, h2, h3};
		float ins[4] = {inL, inR, inL, inR};
		for (int k = 0; k < 4; k++) {
			lines[k][pos[k]] = ins[k] + feedback * back[k];
			if (++pos[k] >= (int) lines[k].size())
				pos[k] = 0;
		}
		outL = 0.5f * (taps[0] + taps[2]);
		outR = 0.5f * (taps[1] + taps[3]);
	}
};


struct Colette : Module {
	enum ParamId {
		PULL_PARAM,
		WIND_PARAM,
		COHESION_PARAM,
		SCATTER_PARAM,
		HARMONY_PARAM,
		ROOT_PARAM,
		RANGE_PARAM,
		BIRDS_PARAM,
		TIMBRE_PARAM,
		BLOOM_PARAM,
		SPACE_PARAM,
		GUST_PARAM,
		FREEZE_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		VOCT_INPUT,
		HARMONY_INPUT,
		PULL_INPUT,
		WIND_INPUT,
		COHESION_INPUT,
		GUST_INPUT,
		FREEZE_INPUT,
		SCATTER_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		LEFT_OUTPUT,
		RIGHT_OUTPUT,
		LAND_OUTPUT,
		NOTE_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		GUST_LIGHT,
		FREEZE_LIGHT,
		LAND_LIGHT,
		LIGHTS_LEN
	};

	Bird birds[MAX_BIRDS];
	static const int MAX_PERCHES = 64;
	float perches[MAX_PERCHES] = {};
	int perchCount = 0;
	int perchKey = -1;
	bool pendingGust = false;
	int counter = 0;
	int tailCounter = 0;
	Space space;
	float sampleRate = 0.f;
	dsp::SchmittTrigger gustInput;
	dsp::BooleanTrigger gustButton;
	dsp::PulseGenerator landPulse, gustLight;
	float note = 0.f;
	int activeBirds = 12;
	float currentWind = 0.f;
	// Pour l'afficheur (copié à chaque pas de contrôle)
	int displayHarmony = 4;
	float displayRange = 3.f;

	Colette() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(PULL_PARAM, 0.f, 1.f, 0.55f, "Pull (how strongly the birds settle on the chord notes)", "%", 0.f, 100.f);
		configParam(WIND_PARAM, 0.f, 1.f, 0.3f, "Wind (turbulence; also makes settled birds take off)", "%", 0.f, 100.f);
		configParam(COHESION_PARAM, 0.f, 1.f, 0.45f, "Cohesion (how tightly the flock holds together)", "%", 0.f, 100.f);
		configParam(SCATTER_PARAM, 0.f, 1.f, 0.4f, "Scatter (how much the birds keep their distance)", "%", 0.f, 100.f);
		std::vector<std::string> names;
		for (const HarmonySet& h : HARMONIES)
			names.push_back(h.name);
		configSwitch(HARMONY_PARAM, 0.f, HARMONY_COUNT - 1, 4.f, "Harmony (where the birds can settle)", names);
		configParam(ROOT_PARAM, -12.f, 12.f, 0.f, "Root (semitones from C2)", " st");
		paramQuantities[ROOT_PARAM]->snapEnabled = true;
		configParam(RANGE_PARAM, 1.f, 5.f, 3.f, "Range (octaves of sky)", " oct");
		paramQuantities[RANGE_PARAM]->snapEnabled = true;
		configParam(BIRDS_PARAM, 2.f, MAX_BIRDS, 12.f, "Birds");
		paramQuantities[BIRDS_PARAM]->snapEnabled = true;
		configParam(TIMBRE_PARAM, 0.f, 1.f, 0.3f, "Timbre (pure, then reedy; more breath in flight)", "%", 0.f, 100.f);
		configParam(BLOOM_PARAM, 0.f, 1.f, 0.5f, "Bloom (how slowly voices swell and fade)", "%", 0.f, 100.f);
		configParam(SPACE_PARAM, 0.f, 1.f, 0.5f, "Space", "%", 0.f, 100.f);
		configButton(GUST_PARAM, "Gust (every bird takes off)");
		configSwitch(FREEZE_PARAM, 0.f, 1.f, 0.f, "Freeze (the flock holds still)", {"Off", "On"});
		configInput(VOCT_INPUT, "Root (1V/oct)");
		configInput(HARMONY_INPUT, "Harmony CV (1 V per harmony; replaces the knob when patched)");
		configInput(PULL_INPUT, "Pull CV (10 V = full range)");
		configInput(WIND_INPUT, "Wind CV (10 V = full range)");
		configInput(COHESION_INPUT, "Cohesion CV (10 V = full range)");
		configInput(SCATTER_INPUT, "Scatter CV (10 V = full range)");
		configInput(GUST_INPUT, "Gust (trigger)");
		configInput(FREEZE_INPUT, "Freeze (holds while the gate is high)");
		configOutput(LEFT_OUTPUT, "Left");
		configOutput(RIGHT_OUTPUT, "Right");
		configOutput(LAND_OUTPUT, "Land (a trigger each time a bird settles)");
		configOutput(NOTE_OUTPUT, "Note (1V/oct pitch of the last bird that settled)");
		scatterBirds();
	}

	void scatterBirds() {
		for (int i = 0; i < MAX_BIRDS; i++) {
			Bird& b = birds[i];
			b = Bird();
			b.y = 0.3f + 2.4f * random::uniform();
			b.x = random::uniform() * 1.6f - 0.8f;
			b.vy = random::normal() * 0.2f;
			b.vx = random::normal() * 0.2f;
			b.phase = random::uniform();
			b.vibratoRate = 3.5f + 3.f * random::uniform();
			for (int k = 0; k < TAIL; k++) {
				b.tailY[k] = b.y;
				b.tailX[k] = b.x;
			}
		}
	}

	void onReset() override {
		scatterBirds();
	}

	void onRandomize() override {
		scatterBirds();
	}

	static float knob(Module* m, int param, int input) {
		return clamp(m->params[param].getValue() + m->inputs[input].getVoltage() / 10.f, 0.f, 1.f);
	}

	// Branchée, l'entrée HARMONY choisit seule l'harmonie (1 V par harmonie) : additionnée au bouton,
	// elle débordait vite sur la dernière position (FREE)
	int harmonyIndex() {
		float h = inputs[HARMONY_INPUT].isConnected() ? inputs[HARMONY_INPUT].getVoltage() : params[HARMONY_PARAM].getValue();
		return clamp((int) std::round(h), 0, HARMONY_COUNT - 1);
	}

	void buildPerches(int harmony, int range) {
		int key = harmony * 10 + range;
		if (key == perchKey)
			return;
		perchKey = key;
		int n = 0;
		if (harmony == HARMONY_HARMONICS) {
			for (int k = 1; k <= (1 << range) && n < MAX_PERCHES; k++)
				perches[n++] = std::log2((float) k);
		}
		else if (harmony != HARMONY_FREE) {
			for (int o = 0; o <= range; o++)
				for (float st : HARMONIES[harmony].semitones) {
					float y = o + st / 12.f;
					if (y <= range + 1e-4f && n < MAX_PERCHES)
						perches[n++] = y;
				}
		}
		perchCount = n;
	}

	float nearestPerch(float y) {
		float best = y, bestDistance = 1e9f;
		for (int k = 0; k < perchCount; k++) {
			float p = perches[k];
			float d = std::fabs(p - y);
			if (d < bestDistance) {
				bestDistance = d;
				best = p;
			}
		}
		return best;
	}

	// Le vol, mis à jour tous les CONTROL_DIVIDER échantillons
	void flock(float dt, int count, float range, float pull, float wind, float cohesion, float scatter, bool gust) {
		float cy = 0.f, cx = 0.f, avy = 0.f, avx = 0.f;
		for (int i = 0; i < count; i++) {
			cy += birds[i].y;
			cx += birds[i].x;
			avy += birds[i].vy;
			avx += birds[i].vx;
		}
		cy /= count;
		cx /= count;
		avy /= count;
		avx /= count;

		float takeOffRate = wind * wind * 0.35f;
		bool perched = perchCount > 0;
		for (int i = 0; i < count; i++) {
			Bird& b = birds[i];
			// Turbulence : bruit lissé propre à chaque oiseau
			float relax = 1.f - std::exp(-dt / 0.6f);
			b.windY += (random::normal() - b.windY) * relax;
			b.windX += (random::normal() - b.windX) * relax;

			if (b.landed) {
				// Il décolle sur une rafale, si son perchoir a disparu, ou au hasard. Les quatre forces agissent
				// aussi sur les oiseaux posés, pour que les boutons répondent même quand la nuée est posée :
				// - WIND : plus il souffle, plus les décollages sont fréquents ;
				// - PULL : la prise sur le perchoir ; plus il est bas, plus l'oiseau se détache facilement ;
				// - COHESION : un oiseau posé loin du reste de la nuée part la rejoindre ;
				// - SCATTER : un oiseau posé trop près d'un voisin s'écarte.
				bool perchGone = !perched || std::fabs(nearestPerch(b.perch) - b.perch) > 1e-3f;
				// PULL à fond : la prise l'emporte sur le groupe et les voisins, seuls le vent et les rafales font partir
				float loosen = 1.f - pull;
				float restless = 0.6f * loosen * loosen * loosen;
				restless += cohesion * 0.15f * std::max(0.f, std::fabs(cy - b.y) - 1.f);
				for (int j = 0; j < count; j++) {
					if (j != i && std::fabs(birds[j].y - b.y) < 0.09f) {
						restless += scatter * scatter * 0.25f;
						break;
					}
				}
				float rate = takeOffRate + restless * (1.f - pull);
				if (gust || perchGone || random::uniform() < rate * dt) {
					b.landed = false;
					b.airborne = 0.3f + 0.5f * random::uniform();
					float lift = gust ? 1.2f : 0.5f;
					b.vy = random::normal() * lift;
					b.vx = random::normal() * lift * 0.6f;
				}
				else {
					b.y = b.perch;
					continue;
				}
			}

			b.airborne = std::max(0.f, b.airborne - dt);
			if (gust) {
				b.airborne = std::max(b.airborne, 0.3f + 0.5f * random::uniform());
				b.vy += random::normal() * 0.8f;
				b.vx += random::normal() * 0.5f;
			}
			// En approche d'un perchoir, sa note l'emporte peu à peu sur le groupe et le vent :
			// sans ça, la cohésion retient les oiseaux en équilibre juste à côté des notes
			float perch = perched ? nearestPerch(b.y) : b.y;
			float proximity = perched ? clamp(1.f - std::fabs(perch - b.y) / 0.06f, 0.f, 1.f) : 0.f;
			float others = 1.f - 0.9f * pull * proximity;
			float ay = 0.f, ax = 0.f;
			// Cohésion et alignement
			ay += cohesion * 1.6f * (cy - b.y) + cohesion * 0.8f * (avy - b.vy);
			ax += cohesion * 1.2f * (cx - b.x) + cohesion * 0.8f * (avx - b.vx);
			// Séparation : on évite les voisins trop proches (des unissons qui battent)
			const float radius = 0.1f;
			for (int j = 0; j < count; j++) {
				if (j == i)
					continue;
				float dy = b.y - birds[j].y, dx = (b.x - birds[j].x) * 0.25f;
				float d2 = dy * dy + dx * dx;
				if (d2 < radius * radius && d2 > 1e-8f) {
					float push = scatter * 0.02f / d2;
					push = std::min(push, 6.f);
					float d = std::sqrt(d2);
					ay += push * dy / d;
					ax += push * dx / d * 2.f;
				}
			}
			ay *= others;
			ax *= others;
			// Perchoir le plus proche
			if (perched && pull > 0.f) {
				float p = perch;
				ay += pull * 14.f * (p - b.y);
				// Posé : assez près et assez lent
				if (b.airborne <= 0.f && std::fabs(p - b.y) < 0.015f && std::fabs(b.vy) < 0.12f && pull > 0.05f) {
					b.landed = true;
					b.perch = p;
					b.y = p;
					b.vy = b.vx = 0.f;
					landPulse.trigger(1e-3f);
					note = p;
					continue;
				}
			}
			// Vent
			ay += others * wind * 2.2f * b.windY;
			ax += others * wind * 1.4f * b.windX;
			// Frottement de l'air, d'autant plus fort que PULL est haut (pour que les oiseaux finissent par se poser)
			float drag = 1.2f + 2.5f * pull;
			b.vy += (ay - drag * b.vy) * dt;
			b.vx += (ax - drag * b.vx) * dt;
			b.y += b.vy * dt;
			b.x += b.vx * dt;
			// Bords du ciel : on rebondit doucement
			if (b.y < 0.f) {
				b.y = 0.f;
				b.vy = std::fabs(b.vy) * 0.5f;
			}
			if (b.y > range) {
				b.y = range;
				b.vy = -std::fabs(b.vy) * 0.5f;
			}
			if (b.x < -1.f) {
				b.x = -1.f;
				b.vx = std::fabs(b.vx) * 0.5f;
			}
			if (b.x > 1.f) {
				b.x = 1.f;
				b.vx = -std::fabs(b.vx) * 0.5f;
			}
		}
	}

	void process(const ProcessArgs& args) override {
		if (args.sampleRate != sampleRate) {
			sampleRate = args.sampleRate;
			space.init(sampleRate);
		}
		const float dt = args.sampleTime;

		int count = clamp((int) std::round(params[BIRDS_PARAM].getValue()), 2, MAX_BIRDS);
		float range = clamp(std::round(params[RANGE_PARAM].getValue()), 1.f, 5.f);
		int harmony = harmonyIndex();
		float pull = knob(this, PULL_PARAM, PULL_INPUT);
		float wind = knob(this, WIND_PARAM, WIND_INPUT);
		float cohesion = knob(this, COHESION_PARAM, COHESION_INPUT);
		float scatter = knob(this, SCATTER_PARAM, SCATTER_INPUT);
		float timbre = params[TIMBRE_PARAM].getValue();
		float bloom = params[BLOOM_PARAM].getValue();
		float spaceAmount = params[SPACE_PARAM].getValue();
		bool frozen = params[FREEZE_PARAM].getValue() > 0.5f || inputs[FREEZE_INPUT].getVoltage() >= 1.f;
		currentWind = wind;

		bool gust = gustButton.process(params[GUST_PARAM].getValue() > 0.f);
		gust |= gustInput.process(inputs[GUST_INPUT].getVoltage(), 0.1f, 1.f);
		if (gust) {
			pendingGust = true;
			gustLight.trigger(0.15f);
		}

		// Le vol
		if (++counter >= CONTROL_DIVIDER) {
			counter = 0;
			buildPerches(harmony, (int) range);
			// Les oiseaux qui viennent d'arriver partent d'un endroit au hasard
			for (int i = activeBirds; i < count; i++) {
				birds[i].landed = false;
				birds[i].y = random::uniform() * range;
				birds[i].amp = 0.f;
			}
			activeBirds = count;
			if (!frozen) {
				flock(dt * CONTROL_DIVIDER, count, range, pull, wind, cohesion, scatter, pendingGust);
				pendingGust = false;
			}
			if (++tailCounter >= 90) {
				tailCounter = 0;
				for (int i = 0; i < count; i++) {
					Bird& b = birds[i];
					for (int k = TAIL - 1; k > 0; k--) {
						b.tailY[k] = b.tailY[k - 1];
						b.tailX[k] = b.tailX[k - 1];
					}
					b.tailY[0] = b.y;
					b.tailX[0] = b.x;
				}
			}
			displayHarmony = harmony;
			displayRange = range;
		}

		// Les voix
		float rootPitch = params[ROOT_PARAM].getValue() / 12.f + inputs[VOCT_INPUT].getVoltage();
		float rootFreq = dsp::FREQ_C4 * std::pow(2.f, rootPitch - 2.f);
		float bloomTime = 0.02f * std::pow(150.f, bloom);
		float ampCoeff = 1.f - std::exp(-dt / bloomTime);
		float left = 0.f, right = 0.f;
		float nyquist = 0.45f * args.sampleRate;
		for (int i = 0; i < MAX_BIRDS; i++) {
			Bird& b = birds[i];
			bool active = i < count;
			if (!active && b.amp < 1e-4f)
				continue;
			float speed = std::fabs(b.vy) + 0.5f * std::fabs(b.vx);
			float targetAmp = !active ? 0.f : b.landed ? 1.f : 0.45f + 0.55f * std::exp(-speed * 3.f);
			b.amp += (targetAmp - b.amp) * ampCoeff;
			float targetBreath = b.landed ? 0.f : clamp(speed * 2.5f, 0.f, 1.f) * (0.3f + 0.7f * timbre);
			b.breath += (targetBreath - b.breath) * (1.f - std::exp(-dt / 0.05f));

			// Un oiseau posé chante avec un léger vibrato, propre à lui
			b.vibratoPhase += b.vibratoRate * dt;
			b.vibratoPhase -= std::floor(b.vibratoPhase);
			// Le vent fait aussi trembler les oiseaux posés : le vibrato s'élargit et devient irrégulier
			float vibrato = b.landed ? (0.0025f + 0.006f * currentWind) * std::sin(2.f * M_PI * b.vibratoPhase) + 0.003f * currentWind * b.windY : 0.f;
			float freq = std::min(rootFreq * dsp::exp2_taylor5(b.y + vibrato), nyquist);
			b.phase += freq * dt;
			b.phase -= std::floor(b.phase);

			// Timbre : sinus pur, puis harmoniques 2 et 3 (de la flûte à l'anche)
			float s1 = std::sin(2.f * M_PI * b.phase), c1 = std::cos(2.f * M_PI * b.phase);
			float s2 = 2.f * s1 * c1;
			float s3 = s1 * (3.f - 4.f * s1 * s1);
			float tone = s1 + timbre * (0.45f * s2 + 0.3f * s3);
			tone /= 1.f + timbre * 0.5f;

			// Souffle : bruit filtré autour de la note de l'oiseau
			float breathSound = 0.f;
			if (b.breath > 1e-3f) {
				float f = 2.f * std::sin(M_PI * std::min(freq, args.sampleRate / 6.f) * dt);
				const float damping = 0.12f;
				float noise = (random::uniform() * 2.f - 1.f) * 1.7f;
				b.bpLow += f * b.bpBand;
				float high = noise - b.bpLow - damping * b.bpBand;
				b.bpBand += f * high;
				b.bpBand = clamp(b.bpBand, -40.f, 40.f);
				b.bpLow = clamp(b.bpLow, -40.f, 40.f);
				breathSound = b.bpBand * 0.12f;
			}
			float voice = b.amp * ((1.f - b.breath) * tone + b.breath * breathSound);

			// Panoramique à puissance constante
			float pan = (b.x + 1.f) * 0.25f * M_PI;
			left += voice * std::cos(pan);
			right += voice * std::sin(pan);
		}
		float norm = 0.9f / std::sqrt((float) count);
		left *= norm;
		right *= norm;

		float wetL, wetR;
		space.process(left, right, spaceAmount, args.sampleRate, wetL, wetR);
		float mix = 0.65f * spaceAmount;
		float outL = (1.f - mix) * left + mix * wetL * 1.6f;
		float outR = (1.f - mix) * right + mix * wetR * 1.6f;
		outputs[LEFT_OUTPUT].setVoltage(5.f * std::tanh(outL));
		outputs[RIGHT_OUTPUT].setVoltage(5.f * std::tanh(outR));

		bool landing = landPulse.process(dt);
		outputs[LAND_OUTPUT].setVoltage(landing ? 10.f : 0.f);
		outputs[NOTE_OUTPUT].setVoltage(rootPitch - 2.f + note);

		lights[GUST_LIGHT].setBrightnessSmooth(gustLight.process(dt), dt);
		lights[FREEZE_LIGHT].setBrightness(frozen);
		lights[LAND_LIGHT].setBrightnessSmooth(landing, dt * 20.f);
	}
};


static const NVGcolor COLETTE_DUSK = nvgRGB(0x2a, 0x24, 0x40);
static const NVGcolor COLETTE_PEACH = nvgRGB(0xf2, 0xb4, 0x8f);
static const NVGcolor COLETTE_CREAM = nvgRGB(0xf1, 0xe7, 0xd8);
static const float COLETTE_COLS[7] = {9.f, 22.93f, 36.87f, 50.8f, 64.73f, 78.67f, 92.6f};


// Le ciel : perchoirs en lignes fines, oiseaux et leurs traînées, ceux qui sont posés brillent
struct ColetteSky : TransparentWidget {
	Colette* module = NULL;

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1)
			return;
		float w = box.size.x, h = box.size.y;
		float pad = mm2px(2.5f);
		float range = module ? module->displayRange : 3.f;
		auto sx = [&](float x) { return pad + (x + 1.f) * 0.5f * (w - 2 * pad); };
		auto sy = [&](float y) { return h - pad - y / range * (h - 2 * pad); };

		// Perchoirs (sans module : un accord de neuvième sur trois octaves)
		std::vector<float> perches;
		if (module)
			perches.assign(module->perches, module->perches + clamp(module->perchCount, 0, Colette::MAX_PERCHES));
		else
			for (int o = 0; o <= 3; o++)
				for (float st : {0.f, 2.f, 4.f, 7.f, 11.f})
					if (o + st / 12.f <= 3.f)
						perches.push_back(o + st / 12.f);
		for (float p : perches) {
			bool root = std::fabs(p - std::round(p)) < 1e-4f;
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, pad, sy(p));
			nvgLineTo(args.vg, w - pad, sy(p));
			nvgStrokeColor(args.vg, nvgTransRGBA(COLETTE_PEACH, root ? 70 : 32));
			nvgStrokeWidth(args.vg, root ? 0.9f : 0.6f);
			nvgStroke(args.vg);
		}

		int count = module ? module->activeBirds : 12;
		for (int i = 0; i < count; i++) {
			float y, x;
			bool landed;
			const float* tailY = NULL;
			const float* tailX = NULL;
			if (module) {
				const Bird& b = module->birds[i];
				y = b.y;
				x = b.x;
				landed = b.landed;
				tailY = b.tailY;
				tailX = b.tailX;
			}
			else {
				// Aperçu dans le navigateur : une nuée immobile
				y = 0.6f + 1.8f * (0.5f + 0.5f * std::sin(i * 1.7f));
				x = std::sin(i * 2.3f) * 0.7f;
				landed = i % 3 == 0;
			}
			if (tailY && !landed) {
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg, sx(x), sy(y));
				for (int k = 0; k < TAIL; k++)
					nvgLineTo(args.vg, sx(tailX[k]), sy(tailY[k]));
				nvgStrokeColor(args.vg, nvgTransRGBA(COLETTE_CREAM, 40));
				nvgStrokeWidth(args.vg, 0.8f);
				nvgStroke(args.vg);
			}
			if (landed) {
				nvgBeginPath(args.vg);
				nvgCircle(args.vg, sx(x), sy(y), 4.5f);
				nvgFillColor(args.vg, nvgTransRGBA(COLETTE_PEACH, 60));
				nvgFill(args.vg);
			}
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, sx(x), sy(y), landed ? 2.2f : 1.6f);
			nvgFillColor(args.vg, landed ? COLETTE_PEACH : COLETTE_CREAM);
			nvgFill(args.vg);
		}

		std::shared_ptr<window::Font> font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/Michroma-Regular.ttf"));
		if (font) {
			int harmony = module ? module->displayHarmony : 4;
			nvgFontFaceId(args.vg, font->handle);
			nvgFontSize(args.vg, 8.f);
			nvgFillColor(args.vg, nvgTransRGBA(COLETTE_CREAM, 200));
			nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
			nvgText(args.vg, w - pad, pad * 0.6f, HARMONIES[harmony].name, NULL);
		}
	}
};


struct ColetteWidget : ModuleWidget {
	void addLabel(Vec posMm, std::string text, float fontSize = 7.f, NVGcolor color = COLETTE_CREAM) {
		PanelLabel* label = createWidget<PanelLabel>(mm2px(posMm));
		label->text = text;
		label->fontSize = fontSize;
		label->color = color;
		addChild(label);
	}

	ColetteWidget(Colette* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Colette.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addLabel(Vec(50.8, 7.5), "COLETTE", 12.f);

		ColetteSky* sky = createWidget<ColetteSky>(mm2px(Vec(5.f, 12.f)));
		sky->box.size = mm2px(Vec(91.6f, 45.f));
		sky->module = module;
		addChild(sky);

		// Les quatre forces du vol
		static const char* const FORCES[4] = {"PULL", "WIND", "COHESION", "SCATTER"};
		static const int FORCE_IDS[4] = {Colette::PULL_PARAM, Colette::WIND_PARAM, Colette::COHESION_PARAM, Colette::SCATTER_PARAM};
		static const float FORCE_X[4] = {14.f, 38.27f, 62.53f, 86.8f};
		for (int i = 0; i < 4; i++) {
			addLabel(Vec(FORCE_X[i], 61.5f), FORCES[i], 8.f, i == 0 ? COLETTE_PEACH : COLETTE_CREAM);
			addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(FORCE_X[i], 70.0f)), module, FORCE_IDS[i]));
		}

		// Le ciel et la voix
		static const char* const NAMES[7] = {"HARMONY", "ROOT", "RANGE", "BIRDS", "TIMBRE", "BLOOM", "SPACE"};
		static const int IDS[7] = {Colette::HARMONY_PARAM, Colette::ROOT_PARAM, Colette::RANGE_PARAM, Colette::BIRDS_PARAM, Colette::TIMBRE_PARAM, Colette::BLOOM_PARAM, Colette::SPACE_PARAM};
		for (int i = 0; i < 7; i++) {
			addLabel(Vec(COLETTE_COLS[i], 80.5f), NAMES[i], 6.f, i == 0 ? COLETTE_PEACH : COLETTE_CREAM);
			if (i <= 3)
				addParam(createParamCentered<RoundBlackSnapKnob>(mm2px(Vec(COLETTE_COLS[i], 87.0f)), module, IDS[i]));
			else
				addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COLETTE_COLS[i], 87.0f)), module, IDS[i]));
		}

		// Entrées
		static const char* const CV_NAMES[7] = {"V/OCT", "HARMONY", "PULL", "WIND", "COHESION", "SCATTER", "GUST"};
		static const int CV_IDS[7] = {Colette::VOCT_INPUT, Colette::HARMONY_INPUT, Colette::PULL_INPUT, Colette::WIND_INPUT, Colette::COHESION_INPUT, Colette::SCATTER_INPUT, Colette::GUST_INPUT};
		for (int i = 0; i < 7; i++) {
			addLabel(Vec(COLETTE_COLS[i], 96.5f), CV_NAMES[i], 5.f);
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COLETTE_COLS[i], 102.5f)), module, CV_IDS[i]));
		}

		// Rafale et gel à la main, gel en gate, puis les sorties
		addLabel(Vec(COLETTE_COLS[0], 110.5f), "GUST", 5.5f);
		addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(mm2px(Vec(COLETTE_COLS[0], 116.5f)), module, Colette::GUST_PARAM, Colette::GUST_LIGHT));
		addLabel(Vec(COLETTE_COLS[1], 110.5f), "FREEZE", 5.5f);
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<WhiteLight>>>(mm2px(Vec(COLETTE_COLS[1], 116.5f)), module, Colette::FREEZE_PARAM, Colette::FREEZE_LIGHT));
		addLabel(Vec(COLETTE_COLS[2], 110.5f), "FREEZE", 5.5f);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COLETTE_COLS[2], 116.5f)), module, Colette::FREEZE_INPUT));

		addLabel(Vec(COLETTE_COLS[3], 110.5f), "LAND", 5.5f, COLETTE_DUSK);
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(COLETTE_COLS[3], 116.5f)), module, Colette::LAND_OUTPUT));
		addChild(createLightCentered<SmallLight<WhiteLight>>(mm2px(Vec(COLETTE_COLS[3] + 6.0f, 110.5f)), module, Colette::LAND_LIGHT));
		addLabel(Vec(COLETTE_COLS[4], 110.5f), "NOTE", 5.5f, COLETTE_DUSK);
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(COLETTE_COLS[4], 116.5f)), module, Colette::NOTE_OUTPUT));
		addLabel(Vec(COLETTE_COLS[5], 110.5f), "OUT L", 5.5f, COLETTE_DUSK);
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(COLETTE_COLS[5], 116.5f)), module, Colette::LEFT_OUTPUT));
		addLabel(Vec(COLETTE_COLS[6], 110.5f), "OUT R", 5.5f, COLETTE_DUSK);
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(COLETTE_COLS[6], 116.5f)), module, Colette::RIGHT_OUTPUT));
	}
};


Model* modelColette = createModel<Colette, ColetteWidget>("Colette");
