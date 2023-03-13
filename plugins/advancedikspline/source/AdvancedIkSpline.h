#pragma once

#include "c4d.h"
#include <memory>
#include "c4d_operatorplugin.h"

#define GvCall(op,fnc) (((GvOperatorData* )op)->*((OPERATORPLUGIN* )C4DOS.Bl->RetrieveTableX((NodeData* )op,1))->fnc)

struct AdvancedIkSplineData
{
public:
	Int32 Mode;
	Float Width;
	Bool Reverse;
	Int32 SegmentsCount;
	Int32 SegmentsSubdivisionsCount;
	Float MainControllerSize;
	Int32 MainControllerShape;
	Float IntermediateControllerSize;
	Int32 IntermediateControllerShape;
};

class AdvancedIkSpline : public ObjectData
{
public:
	AdvancedIkSpline();
	virtual ~AdvancedIkSpline();

	virtual Bool Init(GeListNode* node);
	virtual EXECUTIONRESULT Execute(BaseObject* op, BaseDocument* doc, BaseThread* bt, Int32 priority, EXECUTIONFLAGS flags);
	virtual Bool Message(GeListNode* node, Int32 type, void* data);
	virtual BaseObject* GetVirtualObjects(BaseObject* op, HierarchyHelp* hh);
	virtual Bool GetDEnabling(GeListNode* node,
		const DescID& id,
		const GeData& t_data,
		DESCFLAGS_ENABLE flags,
		const BaseContainer* itemDesc);

	static NodeData* Alloc()
	{
		return NewObj(AdvancedIkSpline) iferr_ignore("AdvancedIkSpline Plugin not instanced");
	}

private:
	LayerObject* _rootLayer = nullptr;
	LayerObject* _controllersLayer = nullptr;
	LayerObject* _jointsLayer = nullptr;
	LayerObject* _invisibleLayer = nullptr;

	Bool _initialRebuildDone = false;

	void rebuild(BaseObject* op);
	void setupLayers(BaseObject* op);
	void clear(BaseObject* op);
	void unfoldAll(BaseObject* op);
	Bool isPropertyRelevantForRebuild(DescriptionPostSetValue* descId);
	Bool isPropertyRelevantForLayerLocks(DescriptionPostSetValue* descId);
	AdvancedIkSplineData* getData(BaseObject* op);
	void createObjects(BaseObject* op, const AdvancedIkSplineData* data);
	void resetLayerLocks(BaseObject* op);
};