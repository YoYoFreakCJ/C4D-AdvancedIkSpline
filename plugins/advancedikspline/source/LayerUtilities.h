#pragma once

#include "c4d.h"
#include <memory>

class LayerUtilities
{
public:
	static LayerObject* FindLayerByName(GeListNode* parent, maxon::String layerName);
	static void ControlLayer(LayerObject* layer, bool view, bool manager, bool enable);
};