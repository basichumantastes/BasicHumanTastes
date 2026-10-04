#include "plugin.hpp"


Plugin* pluginInstance;


void init(Plugin* p) {
	pluginInstance = p;

	// Add modules here
	p->addModel(modelErnest);
	p->addModel(modelMarcel);
	p->addModel(modelJules);
	p->addModel(modelOdette);
	p->addModel(modelColette);
	p->addModel(modelLucienne);
	p->addModel(modelGaston);
	p->addModel(modelGastounet);

	// Any other plugin initialization may go here.
	// As an alternative, consider lazy-loading assets and lookup tables when your module is created to reduce startup times of Rack.
}
