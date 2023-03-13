#include "LayerUtilities.h"
#include <olayer.h>

LayerObject* LayerUtilities::FindLayerByName(GeListNode* parent, maxon::String layerName)
{
	LayerObject* layer = (LayerObject*)parent->GetDown();

	while (layer)
	{
		if (layer->GetName() == layerName)
		{
			return layer;
		}

		layer = layer->GetNext();
	}
	
	return nullptr;
}

void LayerUtilities::ControlLayer(LayerObject* layer, bool view, bool manager, bool enable)
{
	BaseDocument* doc = layer->GetDocument();

	LayerData layerData = *layer->GetLayerData(doc);

	layerData.view = view;
	layerData.manager = manager;
	layerData.locked = !enable;

	layer->SetLayerData(doc, layerData);
}