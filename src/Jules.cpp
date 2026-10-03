#include "plugin.hpp"
#include "widgets.hpp"

// Jules : six pentes liées qui partagent les mêmes réglages (Jules et Jim, et leurs amis).
// - RATE règle la vitesse de la première pente, SPREAD écarte les cinq autres selon leur rang :
//   à fond à droite la pente N va N fois plus vite (série harmonique), à fond à gauche N fois moins vite.
// - RISE partage la durée entre montée et descente, CURVE plie la forme (rectangle, log, linéaire,
//   expo, sinus) sans changer les durées.
// - Plage CV (enveloppes et LFO, 0-8 V) ou AUDIO (oscillateurs, ±5 V).
// - MODE à 6 crans : chaque mode de base (ONE-SHOT, GATE, LOOP) suivi de sa variante (RETRIGGER,
//   SUSTAIN, BURST en CV ; SUBHARMONIC, PLUCK, FM en audio), réglée par TWEAK et expliquée sur l'écran.
// - Les entrées TRIG sont normalisées de droite à gauche : un trigger dans TRIG 6 déclenche les six.
// - CHANCE : probabilité qu'un trigger reçu par normalisation passe à chaque pente (hasard contrôlé
//   dans la cascade). Sortie polyphonique à six canaux.


static const int SLOPES = 6;


// Pliage de CURVE appliqué à l'avancement p (0 à 1) d'une montée ; la descente utilise 1 - g(1 - v).
// curve : -1 rectangle, -0,5 log, 0 linéaire, +0,5 expo, +1 sinus.
static float curveShape(float p, float curve) {
	p = clamp(p, 0.f, 1.f);
	const float k = 4.f;
	auto expo = [&](float x) { return (std::exp(k * x) - 1.f) / (std::exp(k) - 1.f); };
	if (curve >= 0.f) {
		if (curve <= 0.5f)
			return crossfade(p, expo(p), curve * 2.f);
		float sine = 0.5f - 0.5f * std::cos(M_PI * p);
		return crossfade(expo(p), sine, (curve - 0.5f) * 2.f);
	}
	float log = 1.f - expo(1.f - p);
	if (curve >= -0.5f)
		return crossfade(p, log, -curve * 2.f);
	float rect = p > 0.f ? 1.f : 0.f;
	return crossfade(log, rect, (-curve - 0.5f) * 2.f);
}


struct Slope {
	enum Stage {
		IDLE,
		RISE,
		FALL,
		HOLD
	};
	Stage stage = IDLE;
	// Valeur linéaire avant CURVE, de 0 à 1
	float value = 0.f;
	// Phase de 0 à 1 pour les modes cycliques et ONE_SHOT (montée tant que phase < rise)
	float phase = 0.f;
	int burstLeft = 0;
	float modPhase = 0.f;
	float lpgLevel = 0.f;
	float lpgFilter = 0.f;
	bool gate = false;

	bool rising() const {
		return stage == RISE;
	}

	// Sortie après CURVE, de 0 à 1
	float shaped(float curve) const {
		if (stage == IDLE)
			return 0.f;
		if (stage == RISE)
			return curveShape(value, curve);
		return 1.f - curveShape(1.f - value, curve);
	}

	// Déroule la phase (montée de 0 à rise, descente de rise à 1). Renvoie vrai en fin de cycle.
	bool advancePhase(float delta, float rise, bool wrap) {
		phase += delta;
		bool endOfCycle = false;
		if (phase >= 1.f) {
			endOfCycle = true;
			phase = wrap ? phase - std::floor(phase) : 1.f;
		}
		else if (phase < 0.f) {
			phase = wrap ? phase - std::floor(phase) : 0.f;
		}
		if (phase < rise) {
			stage = RISE;
			value = phase / rise;
		}
		else {
			stage = FALL;
			value = (1.f - phase) / (1.f - rise);
		}
		return endOfCycle;
	}

	// Reprend la montée depuis la valeur actuelle (sans repartir de zéro)
	void turnAround(float rise) {
		phase = value * rise;
		stage = RISE;
	}
};


struct Jules : Module {
	enum ParamId {
		RATE_PARAM,
		SPREAD_PARAM,
		RISE_PARAM,
		CURVE_PARAM,
		FM_PARAM,
		RANGE_PARAM,
		MODE_PARAM,
		CHANCE_PARAM,
		TWEAK_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		RATE_INPUT,
		SPREAD_INPUT,
		RISE_INPUT,
		CURVE_INPUT,
		FM_INPUT,
		TWEAK_INPUT,
		CHANCE_INPUT,
		ENUMS(TRIG_INPUT, SLOPES),
		INPUTS_LEN
	};
	enum OutputId {
		ENUMS(SLOPE_OUTPUT, SLOPES),
		MIX_OUTPUT,
		POLY_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		TWEAK_LIGHT,
		LIGHTS_LEN
	};
	enum Mode {
		ONE_SHOT,
		GATE,
		LOOP
	};
	// Les six crans du sélecteur MODE : chaque mode classique, puis sa variante
	enum ModeIndex {
		MODE_ONE_SHOT,
		MODE_RETRIGGER,
		MODE_GATE,
		MODE_SUSTAIN,
		MODE_LOOP,
		MODE_BURST,
		MODES_LEN
	};

	Slope slopes[SLOPES];
	dsp::SchmittTrigger triggers[SLOPES];
	bool accepted[SLOPES] = {};
	bool lastCascade[SLOPES] = {};
	float fmBlocker = 0.f;
	// Pour l'afficheur
	float display[SLOPES + 1] = {};
	float displayDc[SLOPES + 1] = {};

	Jules() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(RATE_PARAM, -1.f, 1.f, 0.f, "Rate (speed of slope 1)");
		configParam(SPREAD_PARAM, -1.f, 1.f, 0.f, "Spread (left: slope N is N times slower, center: all equal, right: N times faster)", "%", 0.f, 100.f);
		configParam(RISE_PARAM, -1.f, 1.f, 0.f, "Rise (share of each cycle spent rising)", "%", 0.f, 50.f, 50.f);
		configParam(CURVE_PARAM, -1.f, 1.f, 0.f, "Curve (square, log, linear, exponential, sine)", "%", 0.f, 100.f);
		configParam(FM_PARAM, -1.f, 1.f, 0.f, "FM amount (left: modulates SPREAD, right: modulates RATE)", "%", 0.f, 100.f);
		configSwitch(RANGE_PARAM, 0.f, 1.f, 0.f, "Range", {"CV (envelopes and LFOs)", "Audio (oscillators)"});
		configSwitch(MODE_PARAM, 0.f, MODES_LEN - 1, MODE_ONE_SHOT, "Mode", {"One-shot (CV) / Impulse (audio)", "Retrigger (CV) / Subharmonic (audio)", "Gate (CV) / Pulse (audio)", "Sustain (CV) / Pluck (audio)", "Loop (CV) / Osc (audio)", "Burst (CV) / FM (audio)"});
		configParam(TWEAK_PARAM, -1.f, 1.f, 0.f, "Tweak (what it does depends on the mode: see the screen)", " V", 0.f, 5.f);
		configParam(CHANCE_PARAM, 0.f, 1.f, 1.f, "Chance (probability that a trigger passed along from the right gets through)", "%", 0.f, 100.f);
		configInput(RATE_INPUT, "Rate (1V/oct)");
		configInput(SPREAD_INPUT, "Spread CV (±5 V)");
		configInput(RISE_INPUT, "Rise CV (±5 V)");
		configInput(CURVE_INPUT, "Curve CV (±5 V)");
		configInput(FM_INPUT, "FM (in FM mode: FM depth)");
		configInput(TWEAK_INPUT, "Tweak CV (±5 V, added to the knob)");
		configInput(CHANCE_INPUT, "Chance CV (10 V = 100 %)");
		for (int i = 0; i < SLOPES; i++) {
			configInput(TRIG_INPUT + i, string::f("Trigger %d (also fed by the triggers to its right)", i + 1));
			configOutput(SLOPE_OUTPUT + i, string::f("Slope %d", i + 1));
		}
		configOutput(MIX_OUTPUT, "Mix (CV: the highest of slope N / N ; audio: all six summed)");
		configOutput(POLY_OUTPUT, "All six slopes (polyphonic)");
	}

	void onReset() override {
		for (int i = 0; i < SLOPES; i++) {
			slopes[i] = Slope();
			accepted[i] = lastCascade[i] = false;
		}
	}

	// Seuil de réception d'un nouveau trigger pendant qu'une pente est active (RETRIGGER, SUBHARMONIC)
	static bool receptive(const Slope& s, float variant, float rise) {
		if (s.stage == Slope::IDLE)
			return true;
		if (variant <= -5.f)
			return true;
		if (variant <= 0.f)
			return s.rising() ? s.value >= (variant + 5.f) / 5.f : true;
		if (variant >= 5.f)
			return false;
		if (s.rising())
			return false;
		float fallProgress = 1.f - s.value;
		return fallProgress >= variant / 5.f;
	}

	// Tension de la variante : bouton ALT (±5 V) + entrée CV
	float tweakVoltage() {
		return clamp(params[TWEAK_PARAM].getValue() * 5.f + inputs[TWEAK_INPUT].getVoltage(), -5.f, 5.f);
	}

	// Ce que fait ALT dans le mode choisi, en clair pour l'afficheur (vide hors variantes)
	std::string tweakText() {
		int modeIndex = (int) params[MODE_PARAM].getValue();
		bool sound = params[RANGE_PARAM].getValue() > 0.5f;
		float v = tweakVoltage();
		float rise = clamp((params[RISE_PARAM].getValue() + inputs[RISE_INPUT].getVoltage() / 5.f + 1.f) / 2.f, 0.f, 1.f);
		// Point du cycle à partir duquel une pente accepte d'être relancée (RETRIGGER, SUBHARMONIC)
		auto retrigText = [&]() {
			if (v <= -5.f)
				return std::string("RETRIG: ANYTIME");
			if (v >= 5.f)
				return std::string("RETRIG: AT END");
			float point = v <= 0.f ? (v + 5.f) / 5.f * rise : rise + v / 5.f * (1.f - rise);
			return string::f("RETRIG AFTER %d %%", (int) std::round(point * 100.f));
		};
		switch (modeIndex) {
			case MODE_RETRIGGER:
				return retrigText();
			case MODE_SUSTAIN:
				if (sound)
					return string::f("DECAY %d MS", (int) std::round(300.f * std::pow(2.f, -v / 2.5f)));
				return string::f("LEVEL %d %%", (int) std::round((v + 5.f) * 10.f));
			case MODE_BURST: {
				if (sound)
					return string::f("RATIO x%.2f", std::pow(2.f, v / 5.f));
				if (v < -4.f)
					return std::string("BURST: OFF");
				int count = (int) std::round(v <= 0.f ? rescale(v, -4.f, 0.f, 1.f, 6.f) : rescale(v, 0.f, 5.f, 6.f, 36.f));
				return string::f("BURST x%d", count);
			}
			default:
				return "";
		}
	}

	// Niveau pour l'afficheur : la tension en SHAPE, l'amplitude autour de la moyenne en SOUND
	float displayLevel(int i, float volts, float fullScale, bool sound, float dt) {
		if (!sound)
			return volts / fullScale;
		displayDc[i] += (volts - displayDc[i]) * (1.f - std::exp(-dt / 0.05f));
		return std::fabs(volts - displayDc[i]) / fullScale;
	}

	void process(const ProcessArgs& args) override {
		const float dt = args.sampleTime;
		bool sound = params[RANGE_PARAM].getValue() > 0.5f;
		int modeIndex = (int) params[MODE_PARAM].getValue();
		int mode = modeIndex / 2;
		bool variant = modeIndex % 2 == 1;
		float tweakV = tweakVoltage();
		bool fmMode = variant && sound && mode == LOOP;
		bool pluck = variant && sound && mode == GATE;
		bool subharmonic = variant && sound && mode == ONE_SHOT;
		bool burst = variant && !sound && mode == LOOP;
		bool sustainLevelMode = variant && !sound && mode == GATE;
		bool retrigger = variant && !sound && mode == ONE_SHOT;

		// FM : couplage continu en SHAPE, sans continu en SOUND (bloqueur à 5 Hz)
		float fmIn = inputs[FM_INPUT].getVoltage() / 5.f;
		if (sound && !fmMode) {
			fmBlocker += (fmIn - fmBlocker) * (1.f - std::exp(-2.f * M_PI * 5.f * dt));
			fmIn -= fmBlocker;
		}
		float fmKnob = params[FM_PARAM].getValue();
		bool fmPatched = inputs[FM_INPUT].isConnected();

		// SPREAD, avec une courbe plus fine autour du centre
		float spread = clamp(params[SPREAD_PARAM].getValue() + inputs[SPREAD_INPUT].getVoltage() / 5.f, -1.f, 1.f);
		spread = 0.5f * spread + 0.5f * spread * spread * spread;
		if (!fmMode && fmPatched && fmKnob < 0.f)
			spread += -fmKnob * fmIn;

		float time = params[RATE_PARAM].getValue();
		float timeCv = clamp(inputs[RATE_INPUT].getVoltage(), -5.f, 8.f);
		// SHAPE : de 2 minutes à 8 ms environ (±7 octaves autour de 1 Hz) ; SOUND : ±4,5 octaves autour de C4
		float baseFreq = sound ? dsp::FREQ_C4 * std::pow(2.f, time * 4.5f + timeCv) : std::pow(2.f, time * 7.f + timeCv);
		if (!fmMode && fmPatched && fmKnob > 0.f)
			baseFreq *= 1.f + fmKnob * fmIn;

		float rise = clamp((params[RISE_PARAM].getValue() + inputs[RISE_INPUT].getVoltage() / 5.f + 1.f) / 2.f, 0.f, 1.f);
		float riseSafe = clamp(rise, 1e-4f, 1.f - 1e-4f);
		float curve = clamp(params[CURVE_PARAM].getValue() + inputs[CURVE_INPUT].getVoltage() / 5.f, -1.f, 1.f);
		float chance = clamp(params[CHANCE_PARAM].getValue() + inputs[CHANCE_INPUT].getVoltage() / 10.f, 0.f, 1.f);

		// Triggers normalisés de droite à gauche. Une pente qui reçoit le trigger d'une voisine
		// le laisse passer avec la probabilité CHANCE, tirée à chaque front montant.
		bool gates[SLOPES];
		bool edges[SLOPES];
		float cascade = 0.f;
		for (int i = SLOPES - 1; i >= 0; i--) {
			bool own = inputs[TRIG_INPUT + i].isConnected();
			if (own)
				cascade = inputs[TRIG_INPUT + i].getVoltage();
			bool high = cascade >= 1.f;
			if (own) {
				accepted[i] = true;
			}
			else if (high && !lastCascade[i]) {
				accepted[i] = random::uniform() < chance;
			}
			lastCascade[i] = high;
			bool gateIn = high && accepted[i];
			edges[i] = triggers[i].process(gateIn ? 10.f : 0.f, 0.1f, 1.f);
			gates[i] = gateIn;
		}

		// BURST : nombre de cycles par trigger, de 1 (à −4 V) à 6 (0 V) puis 36 (+5 V)
		int burstCount = 0;
		if (burst) {
			if (tweakV < -4.f)
				burstCount = 0;
			else if (tweakV <= 0.f)
				burstCount = (int) std::round(rescale(tweakV, -4.f, 0.f, 1.f, 6.f));
			else
				burstCount = (int) std::round(rescale(tweakV, 0.f, 5.f, 6.f, 36.f));
		}

		bool identityEnd = false;
		float mix = 0.f;
		float mixSound = 0.f;
		outputs[POLY_OUTPUT].setChannels(SLOPES);

		for (int i = 0; i < SLOPES; i++) {
			Slope& s = slopes[i];
			int n = i + 1;
			float freq = baseFreq * std::pow((float) n, spread);
			float maxFreq = 0.45f * args.sampleRate;
			freq = clamp(freq, -maxFreq, maxFreq);
			bool cyclic = (mode == LOOP && !burst) || pluck || fmMode || (subharmonic && i == 0);

			if (fmMode) {
				// Modulateur sinus interne, rapport ×0,5 (−5 V) à ×2 (+5 V) ; FM n'est plus qu'une profondeur
				float ratio = std::pow(2.f, tweakV / 5.f);
				s.modPhase += freq * ratio * dt;
				s.modPhase -= std::floor(s.modPhase);
				float depth = fmKnob >= 0.f ? fmKnob : -fmKnob * (n - 1) / 5.f;
				if (fmPatched)
					depth *= inputs[FM_INPUT].getVoltage() / 5.f;
				freq *= 1.f + 4.f * depth * std::sin(2.f * M_PI * s.modPhase);
			}

			if (cyclic) {
				// Trigger = retour au début du cycle (sauf PLUCK, où il ne fait qu'ouvrir le lowpass gate)
				if (!pluck && edges[i])
					s.phase = 0.f;
				bool end = s.advancePhase(freq * dt, riseSafe, true);
				if (i == 0 && end)
					identityEnd = true;
			}
			else if (burst) {
				if (edges[i] && burstCount > 0) {
					s.burstLeft = burstCount;
					s.phase = 0.f;
				}
				if (s.burstLeft > 0) {
					if (s.advancePhase(std::fabs(freq) * dt, riseSafe, true)) {
						s.burstLeft--;
						if (s.burstLeft == 0) {
							s.stage = Slope::IDLE;
							s.value = 0.f;
						}
					}
				}
			}
			else if (mode == ONE_SHOT) {
				bool fire = edges[i] || (subharmonic && identityEnd && i > 0);
				if (fire) {
					if (s.stage == Slope::IDLE) {
						s.phase = 0.f;
						s.stage = Slope::RISE;
						s.value = 0.f;
					}
					else if (retrigger && receptive(s, tweakV, riseSafe)) {
						// RETRIGGER : on repart de zéro
						s.phase = 0.f;
					}
					else if (subharmonic && receptive(s, tweakV, riseSafe)) {
						// SUBHARMONIC : on fait demi-tour depuis la valeur actuelle
						s.turnAround(riseSafe);
					}
				}
				if (s.stage != Slope::IDLE) {
					if (s.advancePhase(std::fabs(freq) * dt, riseSafe, false)) {
						s.stage = Slope::IDLE;
						s.value = 0.f;
						s.phase = 0.f;
					}
				}
			}
			else {
				// GATE (et SUSTAIN) : on suit la gate en montant ou en descendant depuis la valeur actuelle
				float riseRate = std::fabs(freq) / riseSafe;
				float fallRate = std::fabs(freq) / (1.f - riseSafe);
				float sustainLevel = sustainLevelMode ? (tweakV + 5.f) / 10.f : 1.f;
				// Nouvelle gate : on remonte depuis la valeur actuelle
				if (gates[i] && !s.gate)
					s.stage = Slope::RISE;
				if (gates[i]) {
					if (s.stage == Slope::RISE) {
						s.value += riseRate * dt;
						if (s.value >= 1.f) {
							s.value = 1.f;
							s.stage = sustainLevelMode ? Slope::FALL : Slope::HOLD;
						}
					}
					else if (sustainLevelMode) {
						// Release 1 jusqu'au niveau de sustain, puis on le suit (limiteur de pente)
						if (s.value > sustainLevel) {
							s.stage = Slope::FALL;
							s.value = std::max(sustainLevel, s.value - fallRate * dt);
						}
						else if (s.value < sustainLevel) {
							s.stage = Slope::HOLD;
							s.value = std::min(sustainLevel, s.value + riseRate * dt);
						}
						else {
							s.stage = Slope::HOLD;
						}
					}
				}
				else if (s.stage != Slope::IDLE) {
					s.stage = Slope::FALL;
					s.value -= fallRate * dt;
					if (s.value <= 0.f) {
						s.value = 0.f;
						s.stage = Slope::IDLE;
					}
				}
			}

			s.gate = gates[i];
			float out = s.shaped(curve);
			float volts;
			if (pluck) {
				// PLUCK : chaque oscillateur passe dans un lowpass gate façon vactrol
				float speed = std::pow(2.f, -tweakV / 2.5f);
				float attack = 0.002f * speed;
				float decay = 0.3f * speed;
				float target = gates[i] ? 1.f : 0.f;
				float tau = target > s.lpgLevel ? attack : decay;
				s.lpgLevel += (target - s.lpgLevel) * (1.f - std::exp(-dt / tau));
				float cutoff = 30.f + 14000.f * s.lpgLevel * s.lpgLevel;
				float coeff = 1.f - std::exp(-2.f * M_PI * std::min(cutoff, maxFreq) * dt);
				s.lpgFilter += coeff * ((2.f * out - 1.f) - s.lpgFilter);
				volts = 5.f * s.lpgFilter * s.lpgLevel;
			}
			else if (sound) {
				volts = 10.f * out - 5.f;
			}
			else {
				volts = 8.f * out;
			}

			outputs[SLOPE_OUTPUT + i].setVoltage(volts);
			outputs[POLY_OUTPUT].setVoltage(volts, i);
			mix = std::max(mix, volts / n);
			mixSound += volts;

			display[i] += (displayLevel(i, volts, sound ? 5.f : 8.f, sound, dt) - display[i]) * (1.f - std::exp(-dt / 0.03f));
		}

		float mixVolts = sound ? 7.5f * std::tanh(mixSound / 15.f) : mix;
		outputs[MIX_OUTPUT].setVoltage(mixVolts);
		display[SLOPES] += (displayLevel(SLOPES, mixVolts, sound ? 7.5f : 8.f, sound, dt) - display[SLOPES]) * (1.f - std::exp(-dt / 0.03f));

		// Le voyant d'ALT s'allume quand la variante est active, donc quand ALT sert à quelque chose
		lights[TWEAK_LIGHT].setBrightness(variant);
	}
};


static const NVGcolor JULES_CREAM = nvgRGB(0xf3, 0xec, 0xdc);
static const NVGcolor JULES_CORAL = nvgRGB(0xe8, 0x76, 0x5c);
static const NVGcolor JULES_INK = nvgRGB(0x1c, 0x2a, 0x24);

static const float COLUMNS[7] = {9.f, 21.24f, 33.48f, 45.72f, 57.96f, 70.2f, 82.44f};


// Afficheur : six barres alignées sur les colonnes des pentes, plus le MIX, et le mode en cours
struct JulesDisplay : TransparentWidget {
	Jules* module = NULL;

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1)
			return;
		std::shared_ptr<window::Font> font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/Michroma-Regular.ttf"));
		float top = mm2px(5.f);
		float bottom = box.size.y - mm2px(3.f);
		float width = mm2px(4.f);

		for (int i = 0; i < SLOPES + 1; i++) {
			float level = module ? clamp(module->display[i], 0.f, 1.f) : 0.3f + 0.1f * i;
			float x = mm2px(COLUMNS[i]) - box.pos.x - width / 2.f;
			// Fond de la barre
			nvgBeginPath(args.vg);
			nvgRect(args.vg, x, top, width, bottom - top);
			nvgFillColor(args.vg, nvgRGBA(0xf3, 0xec, 0xdc, 24));
			nvgFill(args.vg);
			// Niveau
			float h = (bottom - top) * level;
			nvgBeginPath(args.vg);
			nvgRect(args.vg, x, bottom - h, width, h);
			nvgFillColor(args.vg, i < SLOPES ? JULES_CORAL : JULES_CREAM);
			nvgFill(args.vg);
		}

		if (!font)
			return;
		static const char* const MODE_NAMES[2][6] = {
			{"ONE-SHOT", "RETRIGGER", "GATE", "SUSTAIN", "LOOP", "BURST"},
			{"IMPULSE", "SUBHARMONIC", "PULSE", "PLUCK", "OSC", "FM"},
		};
		std::string text = "ONE-SHOT / CV";
		std::string alt;
		if (module) {
			int sound = module->params[Jules::RANGE_PARAM].getValue() > 0.5f;
			int modeIndex = clamp((int) module->params[Jules::MODE_PARAM].getValue(), 0, 5);
			text = std::string(MODE_NAMES[sound][modeIndex]) + (sound ? " / AUDIO" : " / CV");
			alt = module->tweakText();
		}
		nvgFontFaceId(args.vg, font->handle);
		nvgFontSize(args.vg, 8.f);
		nvgFillColor(args.vg, JULES_CREAM);
		nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
		nvgText(args.vg, box.size.x - mm2px(2.f), mm2px(0.8f), text.c_str(), NULL);
		if (!alt.empty()) {
			nvgFillColor(args.vg, JULES_CORAL);
			nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
			nvgText(args.vg, mm2px(2.f), mm2px(0.8f), alt.c_str(), NULL);
		}
	}
};


struct JulesWidget : ModuleWidget {
	void addLabel(Vec posMm, std::string text, float fontSize = 7.f, NVGcolor color = JULES_CREAM, int align = NVG_ALIGN_CENTER) {
		PanelLabel* label = createWidget<PanelLabel>(mm2px(posMm));
		label->text = text;
		label->fontSize = fontSize;
		label->color = color;
		label->align = align;
		addChild(label);
	}

	JulesWidget(Jules* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Jules.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addLabel(Vec(45.72, 7.5), "JULES", 12.f);

		JulesDisplay* display = createWidget<JulesDisplay>(mm2px(Vec(4.f, 12.f)));
		display->box.size = mm2px(Vec(83.44f, 19.f));
		display->module = module;
		addChild(display);
		for (int i = 0; i < SLOPES; i++)
			addLabel(Vec(COLUMNS[i], 33.5f), string::f("%d", i + 1), 6.f);
		addLabel(Vec(COLUMNS[6], 33.5f), "MIX", 6.f);

		// Grands boutons
		addLabel(Vec(16.0, 39.5), "RATE", 8.f);
		addParam(createParamCentered<RoundBigBlackKnob>(mm2px(Vec(16.0, 49.0)), module, Jules::RATE_PARAM));
		addLabel(Vec(45.72, 39.5), "SPREAD", 8.f);
		addParam(createParamCentered<RoundBigBlackKnob>(mm2px(Vec(45.72, 49.0)), module, Jules::SPREAD_PARAM));
		addLabel(Vec(32.0, 54.5), "SLOWER", 5.f);
		addLabel(Vec(59.4, 54.5), "FASTER", 5.f);
		addLabel(Vec(75.0, 40.5), "FM", 8.f);
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(75.0, 48.0)), module, Jules::FM_PARAM));

		// Forme, switches, chance
		addLabel(Vec(10.0, 61.0), "RISE");
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(10.0, 68.0)), module, Jules::RISE_PARAM));
		addLabel(Vec(25.0, 61.0), "CURVE");
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(25.0, 68.0)), module, Jules::CURVE_PARAM));

		addParam(createParamCentered<CKSS>(mm2px(Vec(37.0, 68.0)), module, Jules::RANGE_PARAM));
		addLabel(Vec(37.0, 62.0), "AUDIO", 5.5f);
		addLabel(Vec(37.0, 74.0), "CV", 5.5f);

		// Le nom du mode s'affiche en haut à droite de l'écran
		addLabel(Vec(50.0, 61.0), "MODE");
		addParam(createParamCentered<RoundBlackSnapKnob>(mm2px(Vec(50.0, 68.0)), module, Jules::MODE_PARAM));
		addLabel(Vec(63.5, 61.0), "TWEAK", 7.f, JULES_CORAL);
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(63.5, 68.0)), module, Jules::TWEAK_PARAM));
		addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(69.8, 66.5)), module, Jules::TWEAK_LIGHT));

		addLabel(Vec(80.0, 61.0), "CHANCE", 7.f, JULES_CORAL);
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(80.0, 68.0)), module, Jules::CHANCE_PARAM));

		// Entrées CV
		static const char* const CV_NAMES[7] = {"V/OCT", "SPREAD", "RISE", "CURVE", "FM", "TWEAK", "CHANCE"};
		for (int i = 0; i < 7; i++) {
			addLabel(Vec(COLUMNS[i], 79.0f), CV_NAMES[i], 5.5f, i >= 5 ? JULES_CORAL : JULES_CREAM);
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COLUMNS[i], 85.0f)), module, Jules::RATE_INPUT + i));
		}

		// Triggers, puis sorties sur la plaque crème
		for (int i = 0; i < SLOPES; i++) {
			addLabel(Vec(COLUMNS[i], 94.0f), "TRIG", 5.5f);
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COLUMNS[i], 100.0f)), module, Jules::TRIG_INPUT + i));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(COLUMNS[i], 115.5f)), module, Jules::SLOPE_OUTPUT + i));
			addLabel(Vec(COLUMNS[i], 109.6f), string::f("%d", i + 1), 6.f, JULES_INK);
		}
		addLabel(Vec(COLUMNS[6], 94.0f), "POLY", 5.5f, JULES_INK);
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(COLUMNS[6], 100.0f)), module, Jules::POLY_OUTPUT));
		addLabel(Vec(COLUMNS[6], 109.6f), "MIX", 5.5f, JULES_INK);
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(COLUMNS[6], 115.5f)), module, Jules::MIX_OUTPUT));
	}
};


Model* modelJules = createModel<Jules, JulesWidget>("Jules");
