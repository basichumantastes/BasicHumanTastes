#include "plugin.hpp"
#include "widgets.hpp"

// Odette : répéteur stéréo (Odette et Odile, les deux cygnes en miroir : la gauche et la droite).
// - 8 tailles (SIZE) imbriquées, de 1,3 ms (cordes, flanger) à 41,8 s (boucles). Le son est écrit dans une
//   seule mémoire : passer d'une petite taille à une grande fait réapparaître ce qui a été joué avant.
//   Changer de taille saute sans glissement de hauteur (fondu entre deux têtes de lecture).
// - TIME règle le temps entre les répétitions dans la taille, en continu : le changer fait glisser la hauteur.
// - FEEDBACK : nombre de répétitions, de 1 à l'infini et un peu au-delà.
// - TONE : le timbre de la boucle, du très sombre au fin et grésillant, neutre vers 3 h ; comme il est dans
//   la boucle, son effet s'accumule. BLUR : une traînée stéréo, réinjectée dans la boucle quand on monte.
// - BEND : plie le temps des répétitions (1V/oct dans la taille 1, pour jouer des cordes).
// - CLOCK : synchronise les répétitions sur une horloge, avec des rapports de ×2 à /2 choisis par TIME.
// - REVERSE, FREEZE (boucle figée sans rien détruire, FEEDBACK choisit alors le point de départ),
//   SPREAD et BOUNCE pour la stéréo, DRIFT (dérive lente façon bande), DUCK (les répétitions reculent
//   pendant que le son entre), sortie PULSE (une impulsion à chaque répétition).


static const int SIZES = 8;
// Durées des répétitions par taille (secondes), de la plus courte à la plus longue
static const float SIZE_SHORTEST[SIZES] = {0.0013f, 0.0204f, 0.0816f, 0.1633f, 0.3265f, 0.6531f, 1.306f, 2.612f};
static const float SIZE_LONGEST[SIZES] = {0.0204f, 0.0816f, 0.3265f, 0.6531f, 1.306f, 2.612f, 5.225f, 41.796f};
// Rapports de durée quand CLOCK est branché, choisis par TIME (de ×2 à /2)
static const float CLOCK_FACTORS[13] = {2.f, 1.75f, 5.f / 3.f, 1.5f, 4.f / 3.f, 1.25f, 1.f, 1.f / 1.25f, 0.75f, 1.f / 1.5f, 0.6f, 1.f / 1.75f, 0.5f};
static const char* const CLOCK_NAMES[13] = {"x2", "x1 3/4", "x1 2/3", "x1 1/2", "x1 1/3", "x1 1/4", "1/1", "/1 1/4", "/1 1/3", "/1 1/2", "/1 2/3", "/1 3/4", "/2"};
// Mémoire : de quoi tenir la sizeIndex 7 et un point de départ HOLD
static const float MEMORY_SECONDS = 64.f;


// Lecture interpolée (Hermite à 4 points) dans une mémoire circulaire
static float readCubic(const std::vector<float>& buf, double position) {
	int size = (int) buf.size();
	double fl = std::floor(position);
	float t = (float) (position - fl);
	int i = (int) fl;
	auto at = [&](int k) { int j = (i + k) % size; if (j < 0) j += size; return buf[j]; };
	float y0 = at(-1), y1 = at(0), y2 = at(1), y3 = at(2);
	float c1 = 0.5f * (y2 - y0);
	float c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3;
	float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
	return ((c3 * t + c2) * t + c1) * t + y1;
}


// Passe-tout pour BLUR
struct Allpass {
	std::vector<float> buf;
	int pos = 0;
	void resize(int n) {
		buf.assign(std::max(n, 1), 0.f);
		pos = 0;
	}
	float process(float x, float g) {
		float delayed = buf[pos];
		float y = -g * x + delayed;
		buf[pos] = x + g * y;
		if (++pos >= (int) buf.size())
			pos = 0;
		return y;
	}
};


struct Channel {
	std::vector<float> memory;
	// Têtes de lecture : la courante et la précédente, en fondu après un saut
	float delay = 0.3f;
	float previousDelay = 0.3f;
	float fade = 1.f;
	int jumpKey = -1;
	// FLIP : deux grains de lecture à l'envers, décalés d'une demi-période
	float reversePhase = 0.f;
	// HOLD : position de lecture dans la boucle figée
	double freezePhase = 0.f;
	// TONE
	float lowA = 0.f, lowB = 0.f, highState = 0.f;
	Allpass blur[4];
	float drift = 0.f, driftTarget = 0.f;
	float pulsePhase = 0.f;
	float output = 0.f;
};


struct Odette : Module {
	enum ParamId {
		SIZE_PARAM,
		TIME_PARAM,
		FEEDBACK_PARAM,
		TONE_PARAM,
		BLUR_PARAM,
		SPREAD_PARAM,
		MIX_PARAM,
		DRIFT_PARAM,
		DUCK_PARAM,
		SIZE_ATTEN_PARAM,
		TIME_ATTEN_PARAM,
		TONE_ATTEN_PARAM,
		REVERSE_PARAM,
		FREEZE_PARAM,
		BOUNCE_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		LEFT_INPUT,
		RIGHT_INPUT,
		SIZE_INPUT,
		TIME_INPUT,
		FEEDBACK_INPUT,
		TONE_INPUT,
		BLUR_INPUT,
		MIX_INPUT,
		BEND_INPUT,
		CLOCK_INPUT,
		REVERSE_INPUT,
		FREEZE_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		LEFT_OUTPUT,
		RIGHT_OUTPUT,
		PULSE_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		REVERSE_LIGHT,
		FREEZE_LIGHT,
		BOUNCE_LIGHT,
		PULSE_LIGHT,
		LIGHTS_LEN
	};

	Channel channels[2];
	int writePos = 0;
	float sampleRate = 48000.f;
	dsp::SchmittTrigger clockTrigger, reverseTrigger, freezeTrigger;
	float clockTimer = 0.f;
	float clockPeriod = 0.f;
	dsp::PulseGenerator pulse;
	float duckEnv = 0.f;
	bool wasFrozen = false;
	int freezeWritePos = 0;
	// Pour l'afficheur
	int displaySize = 3;
	float displayDelay = 0.3f;
	int displayClockStep = -1;

	Odette() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configSwitch(SIZE_PARAM, 0.f, SIZES - 1, 3.f, "Size (range of repeat times)", {"1: 1.3-20 ms (string, flanger)", "2: 20-82 ms (chorus, slapback)", "3: 82-327 ms (short echo)", "4: 163-653 ms (echo)", "5: 0.33-1.3 s (long echo)", "6: 0.65-2.6 s (phrases)", "7: 1.3-5.2 s (phrases)", "8: 2.6-42 s (loops)"});
		configParam(TIME_PARAM, 0.f, 1.f, 0.5f, "Time (time between repeats, within the size range)", "%", 0.f, 100.f);
		configParam(FEEDBACK_PARAM, 0.f, 1.f, 0.45f, "Feedback (number of repeats, up to endless; when frozen: start point)", "%", 0.f, 100.f);
		configParam(TONE_PARAM, 0.f, 1.f, 0.75f, "Tone (left: dark and warm, around 3 o'clock: neutral, right: thin and crisp)", "%", 0.f, 100.f);
		configParam(BLUR_PARAM, 0.f, 1.f, 0.f, "Blur (smears the repeats in stereo)", "%", 0.f, 100.f);
		configParam(SPREAD_PARAM, -1.f, 1.f, 0.f, "Spread (left and right repeat at different times)", "%", 0.f, 100.f);
		configParam(MIX_PARAM, 0.f, 1.f, 0.5f, "Mix (dry / repeats; scales MIX CV when patched)", "%", 0.f, 100.f);
		configParam(DRIFT_PARAM, 0.f, 1.f, 0.f, "Drift (slow tape-like wobble)", "%", 0.f, 100.f);
		configParam(DUCK_PARAM, 0.f, 1.f, 0.f, "Duck (repeats step back while you play)", "%", 0.f, 100.f);
		configParam(SIZE_ATTEN_PARAM, -1.f, 1.f, 0.f, "Size CV amount", "%", 0.f, 100.f);
		configParam(TIME_ATTEN_PARAM, -1.f, 1.f, 0.f, "Time CV amount", "%", 0.f, 100.f);
		configParam(TONE_ATTEN_PARAM, -1.f, 1.f, 0.f, "Tone CV amount", "%", 0.f, 100.f);
		configSwitch(REVERSE_PARAM, 0.f, 1.f, 0.f, "Reverse (repeats play backwards)", {"Off", "On"});
		configSwitch(FREEZE_PARAM, 0.f, 1.f, 0.f, "Freeze (loop what is there, nothing new comes in)", {"Off", "On"});
		configSwitch(BOUNCE_PARAM, 0.f, 1.f, 0.f, "Bounce (ping-pong: repeats alternate left and right)", {"Off", "On"});
		configInput(LEFT_INPUT, "Left (mono)");
		configInput(RIGHT_INPUT, "Right (copies the left when empty)");
		configInput(SIZE_INPUT, "Size CV (5 V = all sizes)");
		configInput(TIME_INPUT, "Time CV (5 V = full range)");
		configInput(FEEDBACK_INPUT, "Feedback CV (5 V = full range)");
		configInput(TONE_INPUT, "Tone CV (5 V = full range)");
		configInput(BLUR_INPUT, "Blur CV (5 V = full range)");
		configInput(MIX_INPUT, "Mix CV (0-8 V)");
		configInput(BEND_INPUT, "Bend (pitch-bends the repeats; 1V/oct in size 1, for strings)");
		configInput(CLOCK_INPUT, "Clock (repeats lock to it; TIME picks the ratio)");
		configInput(REVERSE_INPUT, "Reverse toggle (gate)");
		configInput(FREEZE_INPUT, "Freeze toggle (gate)");
		configOutput(LEFT_OUTPUT, "Left");
		configOutput(RIGHT_OUTPUT, "Right");
		configOutput(PULSE_OUTPUT, "Pulse (a trigger on every repeat, left or right)");
		configBypass(LEFT_INPUT, LEFT_OUTPUT);
		configBypass(RIGHT_INPUT, RIGHT_OUTPUT);
		allocate(48000.f);
	}

	void allocate(float sr) {
		sampleRate = sr;
		// Délais des passe-tout de BLUR (ms), différents à gauche et à droite pour élargir
		static const float BLUR_MS[2][4] = {{4.7f, 7.1f, 11.3f, 16.9f}, {5.3f, 8.3f, 12.7f, 18.1f}};
		for (int c = 0; c < 2; c++) {
			channels[c].memory.assign((size_t) (MEMORY_SECONDS * sr), 0.f);
			for (int k = 0; k < 4; k++)
				channels[c].blur[k].resize((int) (BLUR_MS[c][k] * 0.001f * sr));
		}
		writePos = 0;
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		allocate(e.sampleRate);
	}

	void onReset() override {
		allocate(sampleRate);
		for (int c = 0; c < 2; c++)
			channels[c] = Channel();
		allocate(sampleRate);
		params[REVERSE_PARAM].setValue(0.f);
		params[FREEZE_PARAM].setValue(0.f);
	}

	int sizeIndex() {
		float z = params[SIZE_PARAM].getValue() + params[SIZE_ATTEN_PARAM].getValue() * inputs[SIZE_INPUT].getVoltage() / 5.f * (SIZES - 1);
		return clamp((int) std::round(z), 0, SIZES - 1);
	}

	// Position « rate » interne (0 = répétitions espacées, 1 = rapprochées) : le bouton TIME tourne
	// dans l'autre sens, vers la droite le temps entre deux répétitions s'allonge
	float rate() {
		return 1.f - clamp(params[TIME_PARAM].getValue() + params[TIME_ATTEN_PARAM].getValue() * inputs[TIME_INPUT].getVoltage() / 5.f, 0.f, 1.f);
	}

	// Taille la plus proche d'une durée donnée (pour placer le 1/1 de CLOCK)
	static int sizeFor(float seconds) {
		int best = 0;
		float bestDistance = 1e9f;
		for (int z = 0; z < SIZES; z++) {
			float center = std::sqrt(SIZE_SHORTEST[z] * SIZE_LONGEST[z]);
			float d = std::fabs(std::log2(seconds / center));
			if (d < bestDistance) {
				bestDistance = d;
				best = z;
			}
		}
		return best;
	}

	// TONE : deux passe-bas et un passe-haut d'un pôle, plus une saturation « BBD » au milieu
	float tone(Channel& ch, float x, float c, float dt) {
		float lowCut, highCut;
		if (c <= 0.75f) {
			lowCut = 400.f * std::pow(18000.f / 400.f, c / 0.75f);
			highCut = 20.f;
		}
		else {
			lowCut = 20000.f;
			highCut = 20.f * std::pow(1500.f / 20.f, (c - 0.75f) / 0.25f);
		}
		float nyquist = 0.45f * sampleRate;
		float a = 1.f - std::exp(-2.f * M_PI * std::min(lowCut, nyquist) * dt);
		float h = 1.f - std::exp(-2.f * M_PI * std::min(highCut, nyquist) * dt);
		ch.lowA += a * (x - ch.lowA);
		ch.lowB += a * (ch.lowA - ch.lowB);
		ch.highState += h * (ch.lowB - ch.highState);
		float y = ch.lowB - ch.highState;
		float drive = 1.f + 2.f * std::exp(-std::pow((c - 0.45f) / 0.12f, 2.f));
		return std::tanh(drive * y) / drive;
	}

	float diffuse(Channel& ch, float x, float blur) {
		float g = 0.55f + 0.2f * blur;
		for (int k = 0; k < 4; k++)
			x = ch.blur[k].process(x, g);
		return x;
	}

	void process(const ProcessArgs& args) override {
		// La mémoire suit la fréquence d'échantillonnage réelle du moteur
		if (args.sampleRate != sampleRate)
			allocate(args.sampleRate);
		const float dt = args.sampleTime;
		const int memSize = (int) channels[0].memory.size();

		// Boutons et gates FLIP / HOLD : chaque gate bascule l'état du bouton
		if (reverseTrigger.process(inputs[REVERSE_INPUT].getVoltage(), 0.5f, 2.5f))
			params[REVERSE_PARAM].setValue(1.f - params[REVERSE_PARAM].getValue());
		if (freezeTrigger.process(inputs[FREEZE_INPUT].getVoltage(), 0.5f, 2.5f))
			params[FREEZE_PARAM].setValue(1.f - params[FREEZE_PARAM].getValue());
		bool reverse = params[REVERSE_PARAM].getValue() > 0.5f;
		bool frozen = params[FREEZE_PARAM].getValue() > 0.5f;
		bool bounce = params[BOUNCE_PARAM].getValue() > 0.5f;
		if (frozen && !wasFrozen)
			freezeWritePos = writePos;
		wasFrozen = frozen;

		// CLOCK : période entre deux fronts, entre 20 ms et 4 s
		clockTimer += dt;
		if (clockTrigger.process(inputs[CLOCK_INPUT].getVoltage(), 0.5f, 2.5f)) {
			if (clockTimer > 0.02f && clockTimer < 4.1f)
				clockPeriod = clockTimer;
			clockTimer = 0.f;
		}
		bool clocked = inputs[CLOCK_INPUT].isConnected() && clockPeriod > 0.f;
		if (!inputs[CLOCK_INPUT].isConnected())
			clockPeriod = 0.f;

		int z = sizeIndex();
		float r = rate();
		float baseDelay;
		int clockStep = -1;
		if (clocked) {
			clockStep = (int) std::round(r * 12.f);
			baseDelay = clockPeriod * CLOCK_FACTORS[clockStep] * std::pow(2.f, (float) (z - sizeFor(clockPeriod)));
		}
		else {
			baseDelay = SIZE_LONGEST[z] * std::pow(SIZE_SHORTEST[z] / SIZE_LONGEST[z], r);
		}
		baseDelay = clamp(baseDelay, 0.0005f, MEMORY_SECONDS * 0.6f);

		float feedbackKnob = clamp(params[FEEDBACK_PARAM].getValue() + inputs[FEEDBACK_INPUT].getVoltage() / 5.f, 0.f, 1.f);
		// Jusqu'à un peu plus que l'infini : la saturation de la boucle tient le niveau
		float feedback = 1.08f * feedbackKnob;
		float toneValue = clamp(params[TONE_PARAM].getValue() + params[TONE_ATTEN_PARAM].getValue() * inputs[TONE_INPUT].getVoltage() / 5.f, 0.f, 1.f);
		float blur = clamp(params[BLUR_PARAM].getValue() + inputs[BLUR_INPUT].getVoltage() / 5.f, 0.f, 1.f);
		float spread = params[SPREAD_PARAM].getValue();
		float mix = params[MIX_PARAM].getValue();
		if (inputs[MIX_INPUT].isConnected())
			mix *= clamp(inputs[MIX_INPUT].getVoltage() / 8.f, 0.f, 1.f);
		float drift = params[DRIFT_PARAM].getValue();
		float bend = clamp(inputs[BEND_INPUT].getVoltage(), -5.f, 5.f);

		// Entrées (droite normalisée sur la gauche), en interne ±0,5 pour ±5 V
		float in[2];
		in[0] = inputs[LEFT_INPUT].getVoltage() / 10.f;
		in[1] = inputs[RIGHT_INPUT].isConnected() ? inputs[RIGHT_INPUT].getVoltage() / 10.f : in[0];

		// DUCK : suiveur d'enveloppe de l'entrée
		float level = std::max(std::fabs(in[0]), std::fabs(in[1]));
		float duckCoeff = level > duckEnv ? 1.f - std::exp(-dt / 0.005f) : 1.f - std::exp(-dt / 0.25f);
		duckEnv += (level - duckEnv) * duckCoeff;
		float wetGain = 1.f - params[DUCK_PARAM].getValue() * clamp(duckEnv / 0.25f, 0.f, 1.f);

		float read[2];
		float fadeStep = dt / 0.03f;
		float slew = 1.f - std::exp(-dt / 0.08f);
		// Clé des sauts (sizeIndex, pas de clocked, sens de lecture) : toujours positive, -1 veut dire « pas encore lue »
		int key = z * 100 + (clockStep + 1) + (clocked ? 1000 : 0) + (reverse ? 10000 : 0);
		bool pulseNow = false;

		for (int c = 0; c < 2; c++) {
			Channel& ch = channels[c];
			// Vitesse de chaque côté : SKEW écarte gauche et droite d'une octave au plus
			float target = baseDelay * std::pow(2.f, c == 0 ? -spread : spread);
			// Changement de taille (ou de rapport d'horloge) : saut avec fondu, sans glissement de hauteur
			if (key != ch.jumpKey) {
				if (ch.jumpKey >= 0) {
					ch.previousDelay = ch.delay;
					ch.fade = 0.f;
				}
				ch.delay = target;
				ch.jumpKey = key;
			}
			// Sinon TIME glisse (effet Doppler), sauf avec CLOCK où chaque pas est un saut net
			ch.delay += (target - ch.delay) * slew;
			ch.fade = std::min(1.f, ch.fade + fadeStep);

			// DRIFT : marche lente au hasard, différente à gauche et à droite
			if (random::uniform() < dt * 0.5f)
				ch.driftTarget = random::uniform() * 2.f - 1.f;
			ch.drift += (ch.driftTarget - ch.drift) * (1.f - std::exp(-dt / 0.6f));

			// BEND : 1V/oct dans la taille 1, Doppler linéaire ailleurs (inversé à droite si SPREAD est décentré)
			auto modulated = [&](float d) {
				if (z == 0)
					d *= std::pow(2.f, -bend);
				else
					d += 0.002f * bend * ((c == 1 && std::fabs(spread) > 0.01f) ? -1.f : 1.f);
				d *= 1.f + 0.015f * drift * ch.drift;
				return clamp(d, 4.f / sampleRate, MEMORY_SECONDS * 0.6f);
			};
			float d = modulated(ch.delay);
			float dPrev = modulated(ch.previousDelay);

			float value;
			if (frozen) {
				// Boucle figée : on relit la tranche de durée d qui commence FEEDBACK plus tôt
				float span = d * sampleRate;
				float start = feedbackKnob * SIZE_LONGEST[z] * sampleRate;
				start = std::min(start, memSize - span - 8.f);
				ch.freezePhase += reverse ? -1.0 : 1.0;
				if (ch.freezePhase >= span)
					ch.freezePhase -= span;
				if (ch.freezePhase < 0.0)
					ch.freezePhase += span;
				// La case freezeWritePos est la plus ancienne de la mémoire : on s'arrête juste avant
				double base = freezeWritePos - 2.0 - start - span;
				double p = ch.freezePhase;
				value = readCubic(ch.memory, base + p);
				// Petit fondu au raccord de la boucle
				float fadeLen = std::min(0.005f * sampleRate, span * 0.25f);
				if (p > span - fadeLen) {
					float a = (float) ((p - (span - fadeLen)) / fadeLen);
					value = crossfade(value, readCubic(ch.memory, base + p - span), a);
				}
			}
			else if (reverse) {
				// Lecture à l'envers par deux grains fenêtrés, décalés d'une demi-période
				float span = std::min(d, MEMORY_SECONDS * 0.45f) * sampleRate;
				ch.reversePhase += 1.f / span;
				ch.reversePhase -= std::floor(ch.reversePhase);
				value = 0.f;
				for (int g = 0; g < 2; g++) {
					float ph = ch.reversePhase + 0.5f * g;
					ph -= std::floor(ph);
					float w = std::sin(M_PI * ph);
					value += w * w * readCubic(ch.memory, writePos - 4.0 - 2.0 * span * ph);
				}
			}
			else {
				float now = readCubic(ch.memory, writePos - d * sampleRate);
				if (ch.fade < 1.f)
					now = crossfade(readCubic(ch.memory, writePos - dPrev * sampleRate), now, ch.fade);
				value = now;
			}
			read[c] = value;

			// Sortie RATE : une impulsion à chaque répétition
			ch.pulsePhase += dt / d;
			if (ch.pulsePhase >= 1.f) {
				ch.pulsePhase -= std::floor(ch.pulsePhase);
				pulseNow = true;
			}
		}

		// BLUR : traînée diffuse autour des répétitions
		float diffused[2], wet[2];
		for (int c = 0; c < 2; c++) {
			diffused[c] = diffuse(channels[c], read[c], blur);
			wet[c] = crossfade(read[c], diffused[c], blur);
		}
		float blurSend = std::max(0.f, (blur - 0.5f) * 2.f);
		for (int c = 0; c < 2; c++) {
			Channel& ch = channels[c];
			if (frozen) {
				// En FREEZE, TONE ne touche que la sortie : la boucle reste intacte
				ch.output = tone(ch, wet[c], toneValue, dt);
			}
			else {
				// Boucle : BOUNCE croise les côtés, BLUR s'y injecte dans sa moitié haute,
				// TONE colore à chaque passage (son effet s'accumule), la saturation tient le niveau
				int from = bounce ? 1 - c : c;
				float back = crossfade(read[from], diffused[from], blurSend);
				float written = tone(ch, in[c] + feedback * back, toneValue, dt);
				ch.memory[writePos] = std::tanh(written * 1.2f) / 1.2f;
				ch.output = wet[c];
			}
		}
		if (!frozen)
			writePos = (writePos + 1) % memSize;

		if (pulseNow)
			pulse.trigger(1e-3f);
		bool pulseHigh = pulse.process(dt);

		for (int c = 0; c < 2; c++) {
			float out = crossfade(in[c], channels[c].output * wetGain, mix);
			outputs[LEFT_OUTPUT + c].setVoltage(10.f * out);
		}
		outputs[PULSE_OUTPUT].setVoltage(pulseHigh ? 10.f : 0.f);

		lights[REVERSE_LIGHT].setBrightness(reverse);
		lights[FREEZE_LIGHT].setBrightness(frozen);
		lights[BOUNCE_LIGHT].setBrightness(bounce);
		lights[PULSE_LIGHT].setBrightnessSmooth(pulseHigh, dt);

		displaySize = z;
		displayDelay = channels[0].delay;
		displayClockStep = clockStep;
	}
};


static const NVGcolor ODETTE_INK = nvgRGB(0x16, 0x16, 0x18);
static const NVGcolor ODETTE_IVORY = nvgRGB(0xf2, 0xef, 0xe8);
static const NVGcolor ODETTE_TEAL = nvgRGB(0x2a, 0x7f, 0x7a);

static const float COLS[7] = {9.f, 21.24f, 33.48f, 45.72f, 57.96f, 70.2f, 82.44f};


// Afficheur : les huit tailles en bandeau (de la plus courte à la plus longue), la taille active et sa durée
struct OdetteDisplay : TransparentWidget {
	Odette* module = NULL;

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1)
			return;
		int sizeIndex = module ? module->displaySize : 3;
		float delay = module ? module->displayDelay : 0.3f;
		int clockStep = module ? module->displayClockStep : -1;
		float cellW = (box.size.x - mm2px(4.f)) / SIZES;
		float y = mm2px(1.5f), h = mm2px(4.f);
		for (int z = 0; z < SIZES; z++) {
			float x = mm2px(2.f) + z * cellW;
			nvgBeginPath(args.vg);
			nvgRect(args.vg, x + 1.f, y, cellW - 2.f, h);
			// Du sarcelle clair (court) au sarcelle profond (long)
			float k = z / 7.f;
			NVGcolor col = nvgRGB((int) (0x9f - 0x80 * k), (int) (0xd8 - 0x70 * k), (int) (0xd2 - 0x68 * k));
			nvgFillColor(args.vg, z == sizeIndex ? col : nvgTransRGBA(col, 50));
			nvgFill(args.vg);
		}
		std::shared_ptr<window::Font> font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/Michroma-Regular.ttf"));
		if (!font)
			return;
		std::string time = clockStep >= 0 ? std::string("CLOCK ") + CLOCK_NAMES[clockStep]
			: delay < 1.f ? string::f("%.1f MS", delay * 1000.f) : string::f("%.2f S", delay);
		nvgFontFaceId(args.vg, font->handle);
		nvgFontSize(args.vg, 9.f);
		nvgFillColor(args.vg, ODETTE_IVORY);
		nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
		nvgText(args.vg, mm2px(2.5f), box.size.y - mm2px(1.2f), string::f("SIZE %d", sizeIndex + 1).c_str(), NULL);
		nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BOTTOM);
		nvgText(args.vg, box.size.x - mm2px(2.5f), box.size.y - mm2px(1.2f), time.c_str(), NULL);
	}
};


struct OdetteWidget : ModuleWidget {
	void addLabel(Vec posMm, std::string text, float fontSize = 7.f, NVGcolor color = ODETTE_INK) {
		PanelLabel* label = createWidget<PanelLabel>(mm2px(posMm));
		label->text = text;
		label->fontSize = fontSize;
		label->color = color;
		addChild(label);
	}

	OdetteWidget(Odette* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Odette.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addLabel(Vec(45.72, 7.5), "ODETTE", 12.f);

		OdetteDisplay* display = createWidget<OdetteDisplay>(mm2px(Vec(6.f, 12.5f)));
		display->box.size = mm2px(Vec(79.44f, 14.f));
		display->module = module;
		addChild(display);

		// Grands boutons
		addLabel(Vec(15.0, 31.5), "SIZE", 8.f);
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(15.0, 41.0)), module, Odette::SIZE_PARAM));
		addLabel(Vec(45.72, 31.0), "TIME", 8.f);
		addParam(createParamCentered<RoundBigBlackKnob>(mm2px(Vec(45.72, 41.0)), module, Odette::TIME_PARAM));
		addChild(createLightCentered<SmallLight<WhiteLight>>(mm2px(Vec(54.5, 32.0)), module, Odette::PULSE_LIGHT));
		addLabel(Vec(76.44, 31.5), "FEEDBACK", 8.f);
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(76.44, 41.0)), module, Odette::FEEDBACK_PARAM));

		// Timbre, espace, mélange et ajouts
		static const char* const NAMES[6] = {"TONE", "BLUR", "SPREAD", "MIX", "DRIFT", "DUCK"};
		static const int IDS[6] = {Odette::TONE_PARAM, Odette::BLUR_PARAM, Odette::SPREAD_PARAM, Odette::MIX_PARAM, Odette::DRIFT_PARAM, Odette::DUCK_PARAM};
		static const float XS[6] = {10.f, 24.3f, 38.6f, 52.8f, 67.1f, 81.4f};
		for (int i = 0; i < 6; i++) {
			addLabel(Vec(XS[i], 54.5f), NAMES[i], 7.f, i >= 4 ? ODETTE_TEAL : ODETTE_INK);
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(XS[i], 61.5f)), module, IDS[i]));
		}

		// Atténuverseurs au-dessus de leurs CV, boutons, sortie RATE
		addLabel(Vec(COLS[0], 74.0f), "SIZE", 5.5f);
		addParam(createParamCentered<Trimpot>(mm2px(Vec(COLS[0], 80.0f)), module, Odette::SIZE_ATTEN_PARAM));
		addLabel(Vec(COLS[1], 74.0f), "TIME", 5.5f);
		addParam(createParamCentered<Trimpot>(mm2px(Vec(COLS[1], 80.0f)), module, Odette::TIME_ATTEN_PARAM));
		addLabel(Vec(COLS[2], 74.0f), "TONE", 5.5f);
		addParam(createParamCentered<Trimpot>(mm2px(Vec(COLS[2], 80.0f)), module, Odette::TONE_ATTEN_PARAM));
		addLabel(Vec(COLS[3], 74.0f), "REVERSE", 5.f);
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<WhiteLight>>>(mm2px(Vec(COLS[3], 80.0f)), module, Odette::REVERSE_PARAM, Odette::REVERSE_LIGHT));
		addLabel(Vec(COLS[4], 74.0f), "FREEZE", 5.f);
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<WhiteLight>>>(mm2px(Vec(COLS[4], 80.0f)), module, Odette::FREEZE_PARAM, Odette::FREEZE_LIGHT));
		addLabel(Vec(COLS[5], 74.0f), "BOUNCE", 5.f);
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<WhiteLight>>>(mm2px(Vec(COLS[5], 80.0f)), module, Odette::BOUNCE_PARAM, Odette::BOUNCE_LIGHT));
		addLabel(Vec(COLS[6], 74.0f), "PULSE", 5.5f, ODETTE_IVORY);
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(COLS[6], 80.0f)), module, Odette::PULSE_OUTPUT));

		// CV et gates
		static const char* const CV_NAMES[7] = {"SIZE", "TIME", "TONE", "REVERSE", "FREEZE", "CLOCK", "BEND"};
		static const int CV_IDS[7] = {Odette::SIZE_INPUT, Odette::TIME_INPUT, Odette::TONE_INPUT, Odette::REVERSE_INPUT, Odette::FREEZE_INPUT, Odette::CLOCK_INPUT, Odette::BEND_INPUT};
		for (int i = 0; i < 7; i++) {
			addLabel(Vec(COLS[i], 89.5f), CV_NAMES[i], 5.f);
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COLS[i], 95.5f)), module, CV_IDS[i]));
		}

		// CV restantes, entrées et sorties audio
		static const char* const IO_NAMES[7] = {"FEEDBACK", "BLUR", "MIX", "IN L", "IN R", "OUT L", "OUT R"};
		for (int i = 0; i < 7; i++)
			addLabel(Vec(COLS[i], 106.5f), IO_NAMES[i], i == 0 ? 5.f : 5.5f, i >= 5 ? ODETTE_IVORY : ODETTE_INK);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COLS[0], 112.5f)), module, Odette::FEEDBACK_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COLS[1], 112.5f)), module, Odette::BLUR_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COLS[2], 112.5f)), module, Odette::MIX_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COLS[3], 112.5f)), module, Odette::LEFT_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COLS[4], 112.5f)), module, Odette::RIGHT_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(COLS[5], 112.5f)), module, Odette::LEFT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(COLS[6], 112.5f)), module, Odette::RIGHT_OUTPUT));
	}
};


Model* modelOdette = createModel<Odette, OdetteWidget>("Odette");
