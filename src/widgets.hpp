#pragma once
#include "plugin.hpp"


// Texte dessiné en NanoVG : Rack n'affiche pas les <text> des panneaux SVG.
// Police Michroma, dans l'esprit des sérigraphies techno fin 90 / début 2000.
struct PanelLabel : TransparentWidget {
	std::string text;
	float fontSize = 6.f;
	NVGcolor color = nvgRGB(0x1a, 0x1a, 0x1a);
	int align = NVG_ALIGN_CENTER;

	void draw(const DrawArgs& args) override {
		std::shared_ptr<window::Font> font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/Michroma-Regular.ttf"));
		if (!font)
			return;
		nvgFontFaceId(args.vg, font->handle);
		nvgFontSize(args.vg, fontSize);
		nvgFillColor(args.vg, color);
		nvgTextAlign(args.vg, align | NVG_ALIGN_MIDDLE);
		nvgText(args.vg, 0.f, 0.f, text.c_str(), NULL);
	}
};
