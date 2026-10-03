#include "plugin.hpp"
#include "widgets.hpp"
#include "harmony.hpp"

// Fernand : la migration (comme Magellan, il traverse les mers d'île en île).
// Une carte de 3 à 8 îles, que l'on place à la souris. Chaque île est un accord (une harmonie et une
// fondamentale) et un temps de repos. La nuée part d'une île, traverse, arrive sur la suivante :
// - au départ : un GUST, le vent se lève et l'attraction des notes (PULL) baisse ;
// - à mi-chemin : HARMONY et ROOT passent à l'accord de l'île d'arrivée ;
// - à l'approche : le vent retombe, PULL remonte, la nuée se pose sur le nouvel accord, puis ARRIVE.
// La durée d'une traversée suit la distance sur la carte (TRAVEL). ROUTE choisit l'ordre des îles
// (dans l'ordre, la plus proche, au hasard selon la distance) et CAPRICE la probabilité de changer de cap.
// Avec une horloge dans CLOCK, repos et traversées se comptent en temps.


static const int MAX_ISLANDS = 8;
static const int FERNAND_DIVIDER = 32;
static const char* const NOTE_NAMES[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

struct Island {
	float x = 0.5f, y = 0.5f;  // position sur la carte, de 0 à 1
	int harmony = 4;
	int root = 0;               // demi-tons par rapport à C
	float stay = 1.f;           // multiplicateur du temps de repos
};


struct Fernand : Module {
	enum ParamId {
		ISLANDS_PARAM,
		ROUTE_PARAM,
		CAPRICE_PARAM,
		TRAVEL_PARAM,
		STAY_PARAM,
		WEATHER_PARAM,
		SELECT_PARAM,
		EDIT_HARMONY_PARAM,
		EDIT_ROOT_PARAM,
		EDIT_STAY_PARAM,
		NEW_MAP_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		CLOCK_INPUT,
		RESET_INPUT,
		NEXT_INPUT,
		HOLD_INPUT,
		WEATHER_INPUT,
		TRAVEL_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		HARMONY_OUTPUT,
		ROOT_OUTPUT,
		WIND_OUTPUT,
		PULL_OUTPUT,
		GUST_OUTPUT,
		ARRIVE_OUTPUT,
		RESTING_OUTPUT,
		PROGRESS_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		NEW_MAP_LIGHT,
		LIGHTS_LEN
	};
	enum Route {
		ROUTE_TOUR,
		ROUTE_NEAREST,
		ROUTE_WANDER,
	};

	Island islands[MAX_ISLANDS];
	int current = 0;
	int destination = 1;
	bool traveling = false;
	float progress = 0.f;      // 0 à 1 pendant une traversée
	float restElapsed = 0.f;   // secondes (ou temps d'horloge) passés sur l'île
	float journeyLength = 1.f; // secondes (ou temps) de la traversée en cours
	bool visited[MAX_ISLANDS] = {};
	int counter = 0;

	// Horloge
	dsp::SchmittTrigger clockTrigger, resetTrigger, nextTrigger;
	dsp::BooleanTrigger newMapButton;
	float clockTimer = 0.f, clockPeriod = 0.5f, sinceBeat = 0.f;
	int beatsElapsed = 0;

	dsp::PulseGenerator gustPulse, arrivePulse;

	// Édition de l'île choisie : les boutons reflètent l'île et y écrivent ce qu'on change
	int lastSelected = -1;
	float lastHarmony = -1.f, lastRoot = -99.f, lastStay = -1.f;

	Fernand() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(ISLANDS_PARAM, 3.f, MAX_ISLANDS, 5.f, "Islands");
		paramQuantities[ISLANDS_PARAM]->snapEnabled = true;
		configSwitch(ROUTE_PARAM, 0.f, 2.f, 0.f, "Route", {"In order", "Nearest first", "Wander (closer islands more likely)"});
		configParam(CAPRICE_PARAM, 0.f, 1.f, 0.f, "Caprice (chance of heading somewhere unexpected)", "%", 0.f, 100.f);
		configParam(TRAVEL_PARAM, 0.f, 1.f, 0.4f, "Travel (time to cross the map; longer for far islands)");
		configParam(STAY_PARAM, 0.f, 1.f, 0.4f, "Stay (time spent on each island)");
		configParam(WEATHER_PARAM, 0.f, 1.f, 0.4f, "Weather on the way (calm to stormy)", "%", 0.f, 100.f);
		configParam(SELECT_PARAM, 1.f, MAX_ISLANDS, 1.f, "Island to edit (or click it on the map)");
		paramQuantities[SELECT_PARAM]->snapEnabled = true;
		std::vector<std::string> names;
		for (const HarmonySet& h : HARMONIES)
			names.push_back(h.name);
		configSwitch(EDIT_HARMONY_PARAM, 0.f, HARMONY_COUNT - 1, 4.f, "Harmony of this island", names);
		configParam(EDIT_ROOT_PARAM, -12.f, 12.f, 0.f, "Root of this island (semitones from C)", " st");
		paramQuantities[EDIT_ROOT_PARAM]->snapEnabled = true;
		configParam(EDIT_STAY_PARAM, 0.25f, 4.f, 1.f, "Stay on this island (× STAY)", "×");
		configButton(NEW_MAP_PARAM, "New map (new islands and chords)");
		configInput(CLOCK_INPUT, "Clock (stays and crossings are then counted in beats)");
		configInput(RESET_INPUT, "Reset (back to island 1)");
		configInput(NEXT_INPUT, "Next (leave now, or land now if already travelling)");
		configInput(HOLD_INPUT, "Hold (stay on the island while the gate is high)");
		configInput(WEATHER_INPUT, "Weather CV (10 V = full range)");
		configInput(TRAVEL_INPUT, "Travel CV (10 V = full range)");
		configOutput(HARMONY_OUTPUT, "Chord (1 V per harmony, for Colette's HARMONY input)");
		configOutput(ROOT_OUTPUT, "Root (1V/oct)");
		configOutput(WIND_OUTPUT, "Wind (0-10 V)");
		configOutput(PULL_OUTPUT, "Pull (0-10 V)");
		configOutput(GUST_OUTPUT, "Gust (trigger on leaving an island)");
		configOutput(ARRIVE_OUTPUT, "Arrive (trigger on reaching an island)");
		configOutput(RESTING_OUTPUT, "Rest (gate while on an island)");
		configOutput(PROGRESS_OUTPUT, "Trip (progress of the crossing, 0-10 V)");
		defaultMap();
	}

	void defaultMap() {
		static const float XS[MAX_ISLANDS] = {0.18f, 0.42f, 0.78f, 0.66f, 0.28f, 0.88f, 0.52f, 0.1f};
		static const float YS[MAX_ISLANDS] = {0.3f, 0.72f, 0.24f, 0.8f, 0.52f, 0.58f, 0.18f, 0.84f};
		static const int HS[MAX_ISLANDS] = {4, 5, 3, 6, 0, 1, 2, 7};
		static const int RS[MAX_ISLANDS] = {0, -3, 5, 2, -5, 7, -2, 4};
		for (int i = 0; i < MAX_ISLANDS; i++) {
			islands[i].x = XS[i];
			islands[i].y = YS[i];
			islands[i].harmony = HS[i];
			islands[i].root = RS[i];
			islands[i].stay = 1.f;
		}
		restart();
	}

	// Nouvelle carte : îles bien espacées, accords tirés parmi les plus « aériens », fondamentales en marche de quintes
	void newMap() {
		static const int PALETTE[8] = {4, 5, 3, 6, 0, 4, 5, 8};
		int root = 0;
		for (int i = 0; i < MAX_ISLANDS; i++) {
			for (int attempt = 0; attempt < 40; attempt++) {
				islands[i].x = 0.08f + 0.84f * random::uniform();
				islands[i].y = 0.1f + 0.8f * random::uniform();
				bool farEnough = true;
				for (int j = 0; j < i; j++)
					farEnough &= distance(i, j) > 0.18f;
				if (farEnough)
					break;
			}
			islands[i].harmony = PALETTE[(int) (random::uniform() * 8) % 8];
			islands[i].root = root;
			islands[i].stay = 1.f;
			root += random::uniform() < 0.5f ? 5 : -5;
			if (root > 7)
				root -= 12;
			if (root < -7)
				root += 12;
		}
		restart();
		lastSelected = -1;
	}

	void restart() {
		current = 0;
		traveling = false;
		progress = 0.f;
		restElapsed = 0.f;
		beatsElapsed = 0;
		for (int i = 0; i < MAX_ISLANDS; i++)
			visited[i] = false;
		visited[0] = true;
		destination = 1;
	}

	void onReset() override {
		defaultMap();
		lastSelected = -1;
	}

	void onRandomize() override {
		newMap();
	}

	int count() {
		return clamp((int) std::round(params[ISLANDS_PARAM].getValue()), 3, MAX_ISLANDS);
	}

	float distance(int a, int b) {
		float dx = islands[a].x - islands[b].x, dy = islands[a].y - islands[b].y;
		return std::sqrt(dx * dx + dy * dy);
	}

	int chooseDestination() {
		int n = count();
		int route = (int) params[ROUTE_PARAM].getValue();
		// Caprice : une île au hasard, autre que celle où l'on est
		if (random::uniform() < params[CAPRICE_PARAM].getValue()) {
			int pick = (int) (random::uniform() * (n - 1)) % (n - 1);
			return pick >= current ? pick + 1 : pick;
		}
		if (route == ROUTE_TOUR)
			return (current + 1) % n;
		if (route == ROUTE_NEAREST) {
			bool any = false;
			for (int i = 0; i < n; i++)
				any |= !visited[i] && i != current;
			if (!any)
				for (int i = 0; i < n; i++)
					visited[i] = i == current;
			int best = (current + 1) % n;
			float bestD = 1e9f;
			for (int i = 0; i < n; i++)
				if (i != current && !visited[i] && distance(current, i) < bestD) {
					bestD = distance(current, i);
					best = i;
				}
			return best;
		}
		// Errance : plus une île est proche, plus elle a de chances d'être choisie
		float weights[MAX_ISLANDS], total = 0.f;
		for (int i = 0; i < n; i++) {
			float d = distance(current, i);
			weights[i] = i == current ? 0.f : 1.f / (0.02f + d * d);
			total += weights[i];
		}
		float r = random::uniform() * total;
		for (int i = 0; i < n; i++) {
			r -= weights[i];
			if (r <= 0.f && i != current)
				return i;
		}
		return (current + 1) % n;
	}

	bool clocked() {
		return inputs[CLOCK_INPUT].isConnected();
	}

	// Temps de repos sur l'île : de 1 s à 10 min (ou de 1 à 64 temps), × le réglage de l'île
	float stayLength() {
		float k = params[STAY_PARAM].getValue();
		float base = clocked() ? std::round(1.f + 63.f * k * k) : std::pow(600.f, k);
		return std::max(clocked() ? 1.f : 0.25f, base * islands[current].stay);
	}

	// Durée d'une traversée de toute la carte : de 2 s à 10 min (ou de 1 à 64 temps), × la distance
	float travelLength(float d) {
		float k = clamp(params[TRAVEL_PARAM].getValue() + inputs[TRAVEL_INPUT].getVoltage() / 10.f, 0.f, 1.f);
		if (clocked())
			return std::max(1.f, std::round((1.f + 63.f * k * k) * d / 0.7f));
		return std::max(0.5f, 2.f * std::pow(300.f, k) * d / 0.7f);
	}

	void depart() {
		destination = chooseDestination();
		journeyLength = travelLength(distance(current, destination));
		traveling = true;
		progress = 0.f;
		beatsElapsed = 0;
		sinceBeat = 0.f;
		gustPulse.trigger(1e-3f);
	}

	void arrive() {
		current = destination;
		visited[current] = true;
		traveling = false;
		progress = 0.f;
		restElapsed = 0.f;
		beatsElapsed = 0;
		arrivePulse.trigger(1e-3f);
	}

	// Les boutons d'édition suivent l'île choisie, et ce qu'on y change part dans l'île
	void syncEditor() {
		int n = count();
		int selected = clamp((int) std::round(params[SELECT_PARAM].getValue()) - 1, 0, n - 1);
		Island& isl = islands[selected];
		if (selected != lastSelected) {
			params[EDIT_HARMONY_PARAM].setValue(isl.harmony);
			params[EDIT_ROOT_PARAM].setValue(isl.root);
			params[EDIT_STAY_PARAM].setValue(isl.stay);
			lastSelected = selected;
		}
		else {
			float h = params[EDIT_HARMONY_PARAM].getValue(), r = params[EDIT_ROOT_PARAM].getValue(), s = params[EDIT_STAY_PARAM].getValue();
			if (h != lastHarmony)
				isl.harmony = clamp((int) std::round(h), 0, HARMONY_COUNT - 1);
			if (r != lastRoot)
				isl.root = clamp((int) std::round(r), -12, 12);
			if (s != lastStay)
				isl.stay = s;
		}
		lastHarmony = params[EDIT_HARMONY_PARAM].getValue();
		lastRoot = params[EDIT_ROOT_PARAM].getValue();
		lastStay = params[EDIT_STAY_PARAM].getValue();
	}

	void process(const ProcessArgs& args) override {
		const float dt = args.sampleTime;
		int n = count();
		if (current >= n)
			restart();
		if (destination >= n)
			destination = (current + 1) % n;

		if (newMapButton.process(params[NEW_MAP_PARAM].getValue() > 0.f))
			newMap();
		if (resetTrigger.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 1.f))
			restart();
		bool next = nextTrigger.process(inputs[NEXT_INPUT].getVoltage(), 0.1f, 1.f);
		bool hold = inputs[HOLD_INPUT].getVoltage() >= 1.f;

		// Horloge : on compte les temps et on mesure leur durée pour avancer en douceur entre deux temps
		bool beat = false;
		clockTimer += dt;
		sinceBeat += dt;
		if (clocked() && clockTrigger.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 1.f)) {
			if (clockTimer > 0.01f && clockTimer < 10.f)
				clockPeriod = clockTimer;
			clockTimer = 0.f;
			sinceBeat = 0.f;
			beat = true;
		}

		if (!traveling) {
			if (clocked())
				restElapsed += beat ? 1.f : 0.f;
			else
				restElapsed += dt;
			if (next || (!hold && restElapsed >= stayLength()))
				depart();
		}
		else {
			if (clocked()) {
				if (beat)
					beatsElapsed++;
				float fraction = clamp(sinceBeat / clockPeriod, 0.f, 0.999f);
				progress = clamp((beatsElapsed + fraction) / journeyLength, 0.f, 1.f);
				if (beatsElapsed >= journeyLength)
					progress = 1.f;
			}
			else {
				progress += dt / journeyLength;
			}
			if (next || progress >= 1.f)
				arrive();
		}

		if (++counter >= FERNAND_DIVIDER) {
			counter = 0;
			syncEditor();
		}

		// Ce qui part vers la nuée : l'accord bascule à mi-chemin
		const Island& from = islands[current];
		const Island& to = islands[destination];
		const Island& chord = traveling && progress >= 0.5f ? to : from;
		float weather = clamp(params[WEATHER_PARAM].getValue() + inputs[WEATHER_INPUT].getVoltage() / 10.f, 0.f, 1.f);
		float arc = traveling ? std::sin(M_PI * progress) : 0.f;
		float wind = clamp(0.05f + 0.15f * weather + (0.25f + 0.7f * weather) * arc, 0.f, 1.f);
		float pull = clamp(0.85f - (0.55f + 0.3f * weather) * arc, 0.f, 1.f);

		outputs[HARMONY_OUTPUT].setVoltage((float) chord.harmony);
		outputs[ROOT_OUTPUT].setVoltage(chord.root / 12.f);
		outputs[WIND_OUTPUT].setVoltage(10.f * wind);
		outputs[PULL_OUTPUT].setVoltage(10.f * pull);
		outputs[GUST_OUTPUT].setVoltage(gustPulse.process(dt) ? 10.f : 0.f);
		outputs[ARRIVE_OUTPUT].setVoltage(arrivePulse.process(dt) ? 10.f : 0.f);
		outputs[RESTING_OUTPUT].setVoltage(traveling ? 0.f : 10.f);
		outputs[PROGRESS_OUTPUT].setVoltage(traveling ? 10.f * progress : 0.f);
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_t* list = json_array();
		for (int i = 0; i < MAX_ISLANDS; i++) {
			json_t* o = json_object();
			json_object_set_new(o, "x", json_real(islands[i].x));
			json_object_set_new(o, "y", json_real(islands[i].y));
			json_object_set_new(o, "harmony", json_integer(islands[i].harmony));
			json_object_set_new(o, "root", json_integer(islands[i].root));
			json_object_set_new(o, "stay", json_real(islands[i].stay));
			json_array_append_new(list, o);
		}
		json_object_set_new(root, "islands", list);
		json_object_set_new(root, "current", json_integer(current));
		return root;
	}

	void dataFromJson(json_t* root) override {
		json_t* list = json_object_get(root, "islands");
		if (list) {
			for (int i = 0; i < MAX_ISLANDS && i < (int) json_array_size(list); i++) {
				json_t* o = json_array_get(list, i);
				islands[i].x = json_real_value(json_object_get(o, "x"));
				islands[i].y = json_real_value(json_object_get(o, "y"));
				islands[i].harmony = json_integer_value(json_object_get(o, "harmony"));
				islands[i].root = json_integer_value(json_object_get(o, "root"));
				islands[i].stay = json_real_value(json_object_get(o, "stay"));
			}
		}
		restart();
		json_t* c = json_object_get(root, "current");
		if (c)
			current = clamp((int) json_integer_value(c), 0, MAX_ISLANDS - 1);
		visited[current] = true;
		destination = (current + 1) % MAX_ISLANDS;
		lastSelected = -1;
	}
};


static const NVGcolor FERNAND_INK = nvgRGB(0x4a, 0x3b, 0x2a);
static const NVGcolor FERNAND_PAPER = nvgRGB(0xe9, 0xdc, 0xc0);
static const NVGcolor FERNAND_SEA = nvgRGB(0x3f, 0x6f, 0x8a);
static const NVGcolor FERNAND_FOAM = nvgRGB(0xd8, 0xea, 0xee);
static const float FERNAND_COLS[7] = {9.f, 21.24f, 33.48f, 45.72f, 57.96f, 70.2f, 82.44f};


// La carte : les îles (numéro, accord), le trajet, la nuée qui traverse. On clique une île pour l'éditer
// et on la fait glisser pour la déplacer.
struct FernandMap : OpaqueWidget {
	Fernand* module = NULL;
	int dragging = -1;
	Vec dragPos;

	Vec toScreen(float x, float y) {
		float pad = mm2px(3.f);
		return Vec(pad + x * (box.size.x - 2 * pad), pad + y * (box.size.y - 2 * pad));
	}

	void onButton(const ButtonEvent& e) override {
		if (!module || e.button != GLFW_MOUSE_BUTTON_LEFT || e.action != GLFW_PRESS) {
			OpaqueWidget::onButton(e);
			return;
		}
		int n = module->count();
		dragging = -1;
		for (int i = 0; i < n; i++) {
			Vec p = toScreen(module->islands[i].x, module->islands[i].y);
			if (p.minus(e.pos).norm() < mm2px(3.f)) {
				dragging = i;
				module->params[Fernand::SELECT_PARAM].setValue(i + 1);
				break;
			}
		}
		dragPos = e.pos;
		e.consume(this);
	}

	void onDragMove(const DragMoveEvent& e) override {
		if (!module || dragging < 0)
			return;
		dragPos = dragPos.plus(e.mouseDelta.div(getAbsoluteZoom()));
		float pad = mm2px(3.f);
		module->islands[dragging].x = clamp((dragPos.x - pad) / (box.size.x - 2 * pad), 0.f, 1.f);
		module->islands[dragging].y = clamp((dragPos.y - pad) / (box.size.y - 2 * pad), 0.f, 1.f);
	}

	void onDragEnd(const DragEndEvent& e) override {
		dragging = -1;
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1)
			return;
		std::shared_ptr<window::Font> font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/Michroma-Regular.ttf"));
		Fernand* m = module;
		int n = m ? m->count() : 5;
		auto island = [&](int i) -> const Island& {
			static Island preview[MAX_ISLANDS];
			static bool init = false;
			if (!init) {
				static const float XS[MAX_ISLANDS] = {0.18f, 0.42f, 0.78f, 0.66f, 0.28f, 0.88f, 0.52f, 0.1f};
				static const float YS[MAX_ISLANDS] = {0.3f, 0.72f, 0.24f, 0.8f, 0.52f, 0.58f, 0.18f, 0.84f};
				static const int HS[MAX_ISLANDS] = {4, 5, 3, 6, 0, 1, 2, 7};
				static const int RS[MAX_ISLANDS] = {0, -3, 5, 2, -5, 7, -2, 4};
				for (int k = 0; k < MAX_ISLANDS; k++) {
					preview[k].x = XS[k];
					preview[k].y = YS[k];
					preview[k].harmony = HS[k];
					preview[k].root = RS[k];
				}
				init = true;
			}
			return m ? m->islands[i] : preview[i];
		};
		int current = m ? m->current : 0;
		int destination = m ? m->destination : 1;
		bool traveling = m ? m->traveling : true;
		float progress = m ? m->progress : 0.35f;
		int selected = m ? clamp((int) std::round(m->params[Fernand::SELECT_PARAM].getValue()) - 1, 0, n - 1) : 0;

		// Le trajet dans l'ordre, en pointillés discrets
		for (int i = 0; i < n; i++) {
			Vec a = toScreen(island(i).x, island(i).y), b = toScreen(island((i + 1) % n).x, island((i + 1) % n).y);
			Vec d = b.minus(a);
			float len = d.norm();
			int dashes = (int) (len / 6.f);
			nvgBeginPath(args.vg);
			for (int k = 0; k < dashes; k += 2) {
				Vec p0 = a.plus(d.mult((float) k / dashes)), p1 = a.plus(d.mult((float) (k + 1) / dashes));
				nvgMoveTo(args.vg, p0.x, p0.y);
				nvgLineTo(args.vg, p1.x, p1.y);
			}
			nvgStrokeColor(args.vg, nvgTransRGBA(FERNAND_FOAM, 50));
			nvgStrokeWidth(args.vg, 0.7f);
			nvgStroke(args.vg);
		}

		// La traversée en cours : un arc, et la nuée qui avance dessus
		Vec a = toScreen(island(current).x, island(current).y), b = toScreen(island(destination).x, island(destination).y);
		Vec mid = a.plus(b).div(2.f).plus(Vec(0.f, -mm2px(4.f)));
		auto bezier = [&](float t) { return a.mult((1 - t) * (1 - t)).plus(mid.mult(2 * (1 - t) * t)).plus(b.mult(t * t)); };
		if (traveling) {
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, a.x, a.y);
			nvgQuadTo(args.vg, mid.x, mid.y, b.x, b.y);
			nvgStrokeColor(args.vg, nvgTransRGBA(FERNAND_FOAM, 130));
			nvgStrokeWidth(args.vg, 1.f);
			nvgStroke(args.vg);
		}

		// Les îles
		for (int i = 0; i < n; i++) {
			const Island& isl = island(i);
			Vec p = toScreen(isl.x, isl.y);
			bool here = !traveling && i == current;
			bool target = traveling && i == destination;
			if (i == selected) {
				nvgBeginPath(args.vg);
				nvgCircle(args.vg, p.x, p.y, 7.5f);
				nvgStrokeColor(args.vg, nvgTransRGBA(FERNAND_PAPER, 150));
				nvgStrokeWidth(args.vg, 0.8f);
				nvgStroke(args.vg);
			}
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, p.x, p.y, here || target ? 5.f : 4.f);
			nvgFillColor(args.vg, here ? FERNAND_PAPER : target ? nvgRGB(0xc9, 0xb8, 0x93) : nvgRGB(0x8a, 0x7c, 0x62));
			nvgFill(args.vg);
			if (font) {
				nvgFontFaceId(args.vg, font->handle);
				nvgFontSize(args.vg, 6.f);
				nvgFillColor(args.vg, FERNAND_INK);
				nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
				nvgText(args.vg, p.x, p.y + 0.3f, string::f("%d", i + 1).c_str(), NULL);
				int note = ((isl.root % 12) + 12) % 12;
				std::string label = std::string(NOTE_NAMES[note]) + " " + HARMONIES[clamp(isl.harmony, 0, HARMONY_COUNT - 1)].name;
				nvgFontSize(args.vg, 5.f);
				nvgFillColor(args.vg, nvgTransRGBA(FERNAND_FOAM, 190));
				nvgText(args.vg, p.x, p.y + 9.5f, label.c_str(), NULL);
			}
		}

		// La nuée : quelques points qui suivent l'arc, plus dispersés au milieu du voyage
		if (traveling) {
			float spread = std::sin(M_PI * progress);
			for (int k = 0; k < 9; k++) {
				float t = clamp(progress - 0.015f * k, 0.f, 1.f);
				Vec p = bezier(t);
				float jitterX = std::sin(k * 2.7f + progress * 40.f) * 3.f * spread;
				float jitterY = std::cos(k * 1.9f + progress * 31.f) * 2.f * spread;
				nvgBeginPath(args.vg);
				nvgCircle(args.vg, p.x + jitterX, p.y + jitterY, k == 0 ? 1.6f : 1.1f);
				nvgFillColor(args.vg, nvgTransRGBA(FERNAND_PAPER, k == 0 ? 255 : 170));
				nvgFill(args.vg);
			}
		}

		if (font) {
			std::string status = traveling ? string::f("TO %d  ·  %d %%", destination + 1, (int) (progress * 100.f))
				: string::f("AT %d", current + 1);
			nvgFontFaceId(args.vg, font->handle);
			nvgFontSize(args.vg, 7.f);
			nvgFillColor(args.vg, nvgTransRGBA(FERNAND_FOAM, 210));
			nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
			nvgText(args.vg, mm2px(2.f), mm2px(1.2f), status.c_str(), NULL);
		}
	}
};


struct FernandWidget : ModuleWidget {
	void addLabel(Vec posMm, std::string text, float fontSize = 7.f, NVGcolor color = FERNAND_INK) {
		PanelLabel* label = createWidget<PanelLabel>(mm2px(posMm));
		label->text = text;
		label->fontSize = fontSize;
		label->color = color;
		addChild(label);
	}

	FernandWidget(Fernand* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Fernand.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addLabel(Vec(45.72, 7.5), "FERNAND", 12.f);

		FernandMap* map = createWidget<FernandMap>(mm2px(Vec(5.f, 12.f)));
		map->box.size = mm2px(Vec(81.44f, 41.f));
		map->module = module;
		addChild(map);

		// Le voyage
		static const char* const NAMES[6] = {"ISLANDS", "ROUTE", "CAPRICE", "TRAVEL", "STAY", "WEATHER"};
		static const int IDS[6] = {Fernand::ISLANDS_PARAM, Fernand::ROUTE_PARAM, Fernand::CAPRICE_PARAM, Fernand::TRAVEL_PARAM, Fernand::STAY_PARAM, Fernand::WEATHER_PARAM};
		static const float XS[6] = {9.f, 23.7f, 38.4f, 53.0f, 67.7f, 82.4f};
		for (int i = 0; i < 6; i++) {
			addLabel(Vec(XS[i], 57.5f), NAMES[i], 6.f);
			if (i < 2)
				addParam(createParamCentered<RoundBlackSnapKnob>(mm2px(Vec(XS[i], 64.5f)), module, IDS[i]));
			else
				addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(XS[i], 64.5f)), module, IDS[i]));
		}
		addLabel(Vec(23.7f, 71.0f), "ORDER · NEAR · WANDER", 5.f);

		// L'île choisie
		addLabel(Vec(32.0f, 75.5f), "SELECTED ISLAND", 5.f, FERNAND_SEA);
		static const char* const EDIT_NAMES[4] = {"ISLAND", "HARMONY", "ROOT", "STAY ×"};
		static const int EDIT_IDS[4] = {Fernand::SELECT_PARAM, Fernand::EDIT_HARMONY_PARAM, Fernand::EDIT_ROOT_PARAM, Fernand::EDIT_STAY_PARAM};
		static const float EDIT_X[4] = {11.f, 26.f, 41.f, 56.f};
		for (int i = 0; i < 4; i++) {
			addLabel(Vec(EDIT_X[i], 80.5f), EDIT_NAMES[i], 5.5f);
			if (i < 3)
				addParam(createParamCentered<RoundBlackSnapKnob>(mm2px(Vec(EDIT_X[i], 87.0f)), module, EDIT_IDS[i]));
			else
				addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(EDIT_X[i], 87.0f)), module, EDIT_IDS[i]));
		}
		addLabel(Vec(73.f, 80.5f), "NEW MAP", 5.5f);
		addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(mm2px(Vec(73.f, 87.0f)), module, Fernand::NEW_MAP_PARAM, Fernand::NEW_MAP_LIGHT));

		// Entrées, puis la sortie PROGRESS
		static const char* const IN_NAMES[6] = {"CLOCK", "RESET", "NEXT", "HOLD", "WEATHER", "TRAVEL"};
		for (int i = 0; i < 6; i++) {
			addLabel(Vec(FERNAND_COLS[i], 96.0f), IN_NAMES[i], 5.f);
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(FERNAND_COLS[i], 102.0f)), module, Fernand::CLOCK_INPUT + i));
		}
		addLabel(Vec(FERNAND_COLS[6], 96.0f), "TRIP", 5.f, FERNAND_PAPER);
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(FERNAND_COLS[6], 102.0f)), module, Fernand::PROGRESS_OUTPUT));

		// Sorties vers la nuée
		static const char* const OUT_NAMES[7] = {"CHORD", "ROOT", "WIND", "PULL", "GUST", "ARRIVE", "REST"};
		for (int i = 0; i < 7; i++) {
			addLabel(Vec(FERNAND_COLS[i], 110.6f), OUT_NAMES[i], 5.f, FERNAND_PAPER);
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(FERNAND_COLS[i], 116.5f)), module, Fernand::HARMONY_OUTPUT + i));
		}
	}
};


Model* modelFernand = createModel<Fernand, FernandWidget>("Fernand");
