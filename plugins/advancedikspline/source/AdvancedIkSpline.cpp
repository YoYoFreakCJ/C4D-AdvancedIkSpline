#include "AdvancedIkSpline.h"
#include "c4d_symbols.h"
#include "oadvancedikspline.h"
#include <sstream>
#include "LayerUtilities.h"
#include <tcaikspline.h>
#include <array>
#include <tcaconstraint.h>
#include <c4d_graphview.h>
#include <gvobject.h>
#include <gvmath.h>

using namespace std;

// ToDo: Acquire plugin ID.
#define PLUGINID 11223344

AdvancedIkSpline::AdvancedIkSpline()
{
}

AdvancedIkSpline::~AdvancedIkSpline()
{
}

Bool AdvancedIkSpline::Init(GeListNode* node)
{
	BaseObject* const op = static_cast<BaseObject*>(node);
	BaseContainer* bc = op->GetDataInstance();

	bc->SetInt32(ADVANCEDIKSPLINE_MODE, ADVANCEDIKSPLINE_MODE_SETUP);
	bc->SetFloat(ADVANCEDIKSPLINE_WIDTH, 200.0);
	bc->SetBool(ADVANCEDIKSPLINE_REVERSE, false);
	bc->SetInt32(ADVANCEDIKSPLINE_SEGMENTS_COUNT, 2);
	bc->SetInt32(ADVANCEDIKSPLINE_SEGMENTS_SUBDIVISIONS_COUNT, 2);
	bc->SetFloat(ADVANCEDIKSPLINE_SEGMENTS_MAIN_CONTROLLER_SIZE, 10);
	bc->SetFloat(ADVANCEDIKSPLINE_SEGMENTS_INTERMEDIATE_CONTROLLER_SIZE, 5.0);

	return true;
}

void AdvancedIkSpline::setupLayers(BaseObject* op)
{
	BaseDocument* doc = op->GetDocument();

	GeListHead* layersRoot = doc->GetLayerObjectRoot();

	maxon::String mouthRigRootLayerName = "Advanced IK Spline"_s;

	if (!_rootLayer)
	{
		LayerData layerData;
		layerData.color = Vector(0.062, 0.826, 0.271);

		_rootLayer = LayerObject::Alloc();
		_rootLayer->SetLayerData(doc, layerData);
		_rootLayer->SetName(mouthRigRootLayerName);

		_rootLayer->InsertUnderLast(layersRoot);
		EventAdd();
	}

	maxon::String controllersLayerName = "Controllers"_s;

	if (!_controllersLayer)
	{
		LayerData layerData;
		layerData.color = Vector(1, 0.5, 0);

		_controllersLayer = LayerObject::Alloc();
		_controllersLayer->SetLayerData(doc, layerData);
		_controllersLayer->SetName(controllersLayerName);

		_controllersLayer->InsertUnderLast(_rootLayer);
		EventAdd();
	}

	maxon::String jointsLayerName = "Joints"_s;

	if (!_jointsLayer)
	{
		LayerData layerData;
		layerData.color = Vector(0, 1, 0);

		_jointsLayer = LayerObject::Alloc();
		_jointsLayer->SetLayerData(doc, layerData);
		_jointsLayer->SetName(jointsLayerName);

		_jointsLayer->InsertUnderLast(_rootLayer);
		EventAdd();
	}

	maxon::String invisibleLayerName = "Invisible"_s;

	if (!_invisibleLayer)
	{
		LayerData layerData;
		layerData.color = Vector(0);
		layerData.view = false;
		layerData.render = false;
		layerData.manager = false;
		layerData.locked = true;

		_invisibleLayer = LayerObject::Alloc();
		_invisibleLayer->SetLayerData(doc, layerData);
		_invisibleLayer->SetName(invisibleLayerName);

		_invisibleLayer->InsertUnderLast(_rootLayer);
		EventAdd();
	}

	resetLayerLocks(op);
}

Bool AdvancedIkSpline::Message(GeListNode* node, Int32 type, void* data)
{
	BaseObject* const op = static_cast<BaseObject*>(node);

	bool requiresRebuild = false;

	if (type == MSG_DESCRIPTION_POSTSETPARAMETER)
	{
		DescriptionPostSetValue* descId = (DescriptionPostSetValue*)data;

		requiresRebuild = isPropertyRelevantForRebuild(descId);
	}
	else if (type == MSG_DOCUMENTINFO)
	{
		DocumentInfoData const* const docInfoData = static_cast<DocumentInfoData*>(data);

		if (docInfoData->type == MSG_DOCUMENTINFO_TYPE_UNDO
			|| docInfoData->type == MSG_DOCUMENTINFO_TYPE_REDO)
		{
			AdvancedIkSplineData const* data = getData(op);

			if (data->Mode == ADVANCEDIKSPLINE_MODE_SETUP)
			{
				requiresRebuild = true;
			}
		}
	}

	if (requiresRebuild)
	{
		rebuild(op);
	}

	if (type == MSG_DESCRIPTION_POSTSETPARAMETER)
	{
		DescriptionPostSetValue* descId = (DescriptionPostSetValue*)data;

		if (isPropertyRelevantForLayerLocks(descId))
		{
			resetLayerLocks(op);
		}
	}

	return true;
}

BaseObject* AdvancedIkSpline::GetVirtualObjects(BaseObject* op, HierarchyHelp* hh)
{
	if (!_initialRebuildDone)
	{
		maxon::ExecuteOnMainThread([this, op]
			{
				rebuild(op);
				EventAdd();
			},
			maxon::WAITMODE::DONT_WAIT);

		_initialRebuildDone = true;
	}

	return BaseObject::Alloc(Onull);
}

Bool AdvancedIkSpline::GetDEnabling(GeListNode* node,
	const DescID& id,
	const GeData& t_data,
	DESCFLAGS_ENABLE flags,
	const BaseContainer* itemDesc)
{
	BaseObject* const op = static_cast<BaseObject*>(node);

	AdvancedIkSplineData const* data = getData(op);

	if (id == ADVANCEDIKSPLINE_MODE)
	{
		return true;
	}

	switch (data->Mode)
	{
	case ADVANCEDIKSPLINE_MODE_SETUP:
		if (id == ADVANCEDIKSPLINE_WIDTH
			|| id == ADVANCEDIKSPLINE_REVERSE
			|| id == ADVANCEDIKSPLINE_SEGMENTS_COUNT
			|| id == ADVANCEDIKSPLINE_SEGMENTS_SUBDIVISIONS_COUNT
			|| id == ADVANCEDIKSPLINE_SEGMENTS_MAIN_CONTROLLER_SIZE
			|| id == ADVANCEDIKSPLINE_SEGMENTS_INTERMEDIATE_CONTROLLER_SIZE)
		{
			return true;
		}
		break;
	}

	return false;
}

Bool AdvancedIkSpline::isPropertyRelevantForRebuild(DescriptionPostSetValue* descId)
{
	return *descId->descid == DescID(DescLevel(ADVANCEDIKSPLINE_WIDTH, DTYPE_REAL, 0))
		|| *descId->descid == DescID(DescLevel(ADVANCEDIKSPLINE_SEGMENTS_COUNT, DTYPE_LONG, 0))
		|| *descId->descid == DescID(DescLevel(ADVANCEDIKSPLINE_SEGMENTS_SUBDIVISIONS_COUNT, DTYPE_LONG, 0))
		|| *descId->descid == DescID(DescLevel(ADVANCEDIKSPLINE_SEGMENTS_MAIN_CONTROLLER_SIZE, DTYPE_REAL, 0))
		|| *descId->descid == DescID(DescLevel(ADVANCEDIKSPLINE_SEGMENTS_INTERMEDIATE_CONTROLLER_SIZE, DTYPE_REAL, 0));
}

Bool AdvancedIkSpline::isPropertyRelevantForLayerLocks(DescriptionPostSetValue* descId)
{
	return *descId->descid == DescID(DescLevel(ADVANCEDIKSPLINE_MODE, DTYPE_LONG, 0));
}

void AdvancedIkSpline::rebuild(BaseObject* op)
{
	clear(op);

	AdvancedIkSplineData* data = getData(op);
	BaseDocument* doc = op->GetDocument();

	setupLayers(op);

	createObjects(op, data);

	unfoldAll(op);

	delete data;
}

void AdvancedIkSpline::clear(BaseObject* op)
{
	BaseObject* child = op->GetDown();
	while (child)
	{
		child->Remove();

		child = op->GetDown();
	}
}

void AdvancedIkSpline::unfoldAll(BaseObject* op)
{
	op->ChangeNBit(NBIT::OM1_FOLD, NBITCONTROL::SET);

	BaseObject* child = op->GetDown();

	while (child)
	{
		child->ChangeNBit(NBIT::OM1_FOLD, NBITCONTROL::SET);
		unfoldAll(child);

		child = child->GetNext();
	}
}

AdvancedIkSplineData* AdvancedIkSpline::getData(BaseObject* op)
{
	BaseContainer* userData = op->GetDataInstance();

	AdvancedIkSplineData* data = new AdvancedIkSplineData();

	data->Mode = userData->GetInt32(ADVANCEDIKSPLINE_MODE);
	data->Width = userData->GetFloat(ADVANCEDIKSPLINE_WIDTH);
	data->Reverse = userData->GetBool(ADVANCEDIKSPLINE_REVERSE);
	data->SegmentsCount = userData->GetInt32(ADVANCEDIKSPLINE_SEGMENTS_COUNT);
	data->SegmentsSubdivisionsCount = userData->GetInt32(ADVANCEDIKSPLINE_SEGMENTS_SUBDIVISIONS_COUNT);
	data->MainControllerSize = userData->GetFloat(ADVANCEDIKSPLINE_SEGMENTS_MAIN_CONTROLLER_SIZE);
	data->MainControllerShape = NULLOBJECT_DISPLAY_CIRCLE;
	data->IntermediateControllerSize = userData->GetFloat(ADVANCEDIKSPLINE_SEGMENTS_INTERMEDIATE_CONTROLLER_SIZE);
	data->IntermediateControllerShape = NULLOBJECT_DISPLAY_CIRCLE;

	return data;
}

void AdvancedIkSpline::createObjects(BaseObject* op, const AdvancedIkSplineData* data)
{
	op->SetLayerObject(_rootLayer);

	BaseObject* baseIkSplinesContainer = BaseObject::Alloc(Onull);
	baseIkSplinesContainer->SetName("Base.IkSplines"_s);
	baseIkSplinesContainer->InsertUnderLast(op);
	baseIkSplinesContainer->SetLayerObject(_invisibleLayer);

	BaseObject* baseJointsContainer = BaseObject::Alloc(Onull);
	baseJointsContainer->SetName("Base.Joints"_s);
	baseJointsContainer->InsertUnderLast(op);
	baseJointsContainer->SetLayerObject(_invisibleLayer);

	BaseObject* controllersContainer = BaseObject::Alloc(Onull);
	controllersContainer->SetName("Controllers"_s);
	controllersContainer->InsertUnderLast(op);
	controllersContainer->SetLayerObject(_controllersLayer);

	BaseObject* jointsContainer = BaseObject::Alloc(Onull);
	jointsContainer->SetName("Joints"_s);
	jointsContainer->InsertUnderLast(op);
	jointsContainer->SetLayerObject(_jointsLayer);

	Float leftmostPoint = data->Width / -2;

	Float segmentWidth = data->Width / data->SegmentsCount;
	Float segmentSubdivWidth = segmentWidth / (data->SegmentsSubdivisionsCount + 1);

	int jointIndex = 0;

	Vector controllerRotation = Vector(DegToRad((Float32)-90), 0, 0);

	BaseObject** mainControllers = new BaseObject * [data->SegmentsCount + 1];
	int detailIkSplinePointsCount = data->SegmentsCount + 1 + data->SegmentsCount * (data->SegmentsSubdivisionsCount);
	BaseObject** allControllers = new BaseObject * [detailIkSplinePointsCount];
	int ctrlIndex = 0;

#pragma region create start main controller

	BaseObject* firstMainController = BaseObject::Alloc(Onull);
	firstMainController->SetName("Main.Controller.0"_s);
	firstMainController->SetFrozenPos(Vector(data->Width / -2, 0, 0));
	firstMainController->SetFrozenRot(controllerRotation);
	firstMainController->SetParameter(NULLOBJECT_DISPLAY, data->MainControllerShape, DESCFLAGS_SET::NONE);
	firstMainController->SetParameter(NULLOBJECT_RADIUS, data->MainControllerSize, DESCFLAGS_SET::NONE);
	firstMainController->SetParameter(NULLOBJECT_ORIENTATION, NULLOBJECT_ORIENTATION_SCREEN, DESCFLAGS_SET::NONE);
	firstMainController->SetLayerObject(_controllersLayer);
	jointIndex++;

	firstMainController->InsertUnderLast(controllersContainer);
	mainControllers[0] = allControllers[ctrlIndex] = firstMainController;
	ctrlIndex++;

#pragma endregion

	for (int segmentI = 0; segmentI < data->SegmentsCount; segmentI++)
	{
		bool isFirstSegment = segmentI == 0;
		bool isLastSegment = segmentI == data->SegmentsCount - 1;

		int segmentJointIndex = 0;

#pragma region create base IK spline

		maxon::String baseIkSplineName = "Base.IkSpline."_s;
		baseIkSplineName.AppendInt(segmentI);
		SplineObject* baseIkSpline = SplineObject::Alloc(2, SPLINETYPE::BEZIER);
		baseIkSpline->SetName(baseIkSplineName);
		Vector* points = baseIkSpline->GetPointW();
		points[0] = Vector(leftmostPoint + segmentI * segmentWidth, 0, 0);
		points[1] = Vector(leftmostPoint + (segmentI + 1) * segmentWidth, 0, 0);

		baseIkSpline->InsertUnderLast(baseIkSplinesContainer);
		baseIkSpline->SetLayerObject(_invisibleLayer);

#pragma endregion

#pragma region create base start joint

		maxon::String baseStartJointName{ "Base." };
		baseStartJointName.AppendInt(segmentI);
		baseStartJointName += ".Joint."_s;
		baseStartJointName.AppendInt(segmentJointIndex);
		BaseObject* baseStartJoint = BaseObject::Alloc(Ojoint);
		baseStartJoint->SetName(baseStartJointName);
		baseStartJoint->SetFrozenPos(Vector(leftmostPoint + segmentI * segmentWidth, 0, 0));
		baseStartJoint->SetFrozenRot(controllerRotation);
		baseStartJoint->InsertUnderLast(baseJointsContainer);
		baseStartJoint->SetLayerObject(_invisibleLayer);

		segmentJointIndex++;

#pragma endregion

		BaseObject* parentJoint = baseStartJoint;

		for (int subdivI = 0; subdivI < data->SegmentsSubdivisionsCount; subdivI++)
		{
#pragma region create detail controller

			maxon::String ctrlName{ "Detail.Controller." };
			ctrlName.AppendInt(jointIndex);

			BaseObject* subdivCtrl = BaseObject::Alloc(Onull);
			subdivCtrl->SetName(ctrlName);
			subdivCtrl->SetFrozenPos(Vector(leftmostPoint + (subdivI + 1) * segmentSubdivWidth + segmentI * segmentWidth, 0, 0));
			subdivCtrl->SetFrozenRot(controllerRotation);
			subdivCtrl->SetParameter(NULLOBJECT_DISPLAY, data->IntermediateControllerShape, DESCFLAGS_SET::NONE);
			subdivCtrl->SetParameter(NULLOBJECT_RADIUS, data->IntermediateControllerSize, DESCFLAGS_SET::NONE);
			subdivCtrl->SetParameter(NULLOBJECT_ORIENTATION, NULLOBJECT_ORIENTATION_SCREEN, DESCFLAGS_SET::NONE);
			subdivCtrl->SetLayerObject(_controllersLayer);

			subdivCtrl->InsertUnderLast(controllersContainer);
			allControllers[ctrlIndex] = subdivCtrl;
			ctrlIndex++;

#pragma endregion

			jointIndex++;

#pragma region create base joint

			maxon::String baseJointName{ "Base." };
			baseJointName.AppendInt(segmentI);
			baseJointName += ".Joint."_s;
			baseJointName.AppendInt(segmentJointIndex);
			BaseObject* baseJoint = BaseObject::Alloc(Ojoint);
			baseJoint->SetName(baseJointName);
			baseJoint->SetFrozenPos(Vector(0, 0, segmentSubdivWidth));
			baseJoint->InsertUnderLast(parentJoint);
			baseJoint->SetLayerObject(_invisibleLayer);

			segmentJointIndex++;

			parentJoint = baseJoint;

#pragma endregion

#pragma region create detail controller parent constraint

			BaseTag* parentConstraintTag = subdivCtrl->MakeTag(Tcaconstraint);
			parentConstraintTag->SetParameter(ID_CA_CONSTRAINT_TAG_PARENT, 1, DESCFLAGS_SET::NONE);
			parentConstraintTag->SetParameter(ID_CA_CONSTRAINT_TAG_PARENT_TARGET_COUNT + 1, baseJoint, DESCFLAGS_SET::NONE);

#pragma endregion

		}

		if (!isLastSegment)
		{
#pragma region create main controller

			maxon::String ctrlName{ "Main.Controller." };
			ctrlName.AppendInt(jointIndex);

			BaseObject* segmentCtrl = BaseObject::Alloc(Onull);
			segmentCtrl->SetName(ctrlName);
			segmentCtrl->SetFrozenPos(Vector(leftmostPoint + (segmentI + 1) * segmentWidth, 0, 0));
			segmentCtrl->SetFrozenRot(controllerRotation);
			segmentCtrl->SetParameter(NULLOBJECT_DISPLAY, data->MainControllerShape, DESCFLAGS_SET::NONE);
			segmentCtrl->SetParameter(NULLOBJECT_RADIUS, data->MainControllerSize, DESCFLAGS_SET::NONE);
			segmentCtrl->SetParameter(NULLOBJECT_ORIENTATION, NULLOBJECT_ORIENTATION_SCREEN, DESCFLAGS_SET::NONE);
			segmentCtrl->SetLayerObject(_controllersLayer);

			segmentCtrl->InsertUnderLast(controllersContainer);
			mainControllers[segmentI + 1] = allControllers[ctrlIndex] = segmentCtrl;
			ctrlIndex++;

			jointIndex++;

#pragma endregion
		}

#pragma region create base end joint

		maxon::String baseEndJointName{ "Base." };
		baseEndJointName.AppendInt(segmentI);
		baseEndJointName += ".Joint."_s;
		baseEndJointName.AppendInt(segmentJointIndex);
		BaseObject* baseEndJoint = BaseObject::Alloc(Ojoint);
		baseEndJoint->SetName(baseEndJointName);
		baseEndJoint->SetFrozenPos(Vector(0, 0, segmentSubdivWidth));
		baseEndJoint->InsertUnderLast(parentJoint);
		baseEndJoint->SetLayerObject(_invisibleLayer);

#pragma endregion

		if (isLastSegment)
		{
#pragma region create end main controller

			maxon::String name{ "Main.Controller." };
			name.AppendInt(jointIndex);
			jointIndex++;

			BaseObject* lastMainController = BaseObject::Alloc(Onull);
			lastMainController->SetName(name);
			lastMainController->SetFrozenPos(Vector(data->Width / 2, 0, 0));
			lastMainController->SetFrozenRot(controllerRotation);
			lastMainController->SetParameter(NULLOBJECT_DISPLAY, data->MainControllerShape, DESCFLAGS_SET::NONE);
			lastMainController->SetParameter(NULLOBJECT_RADIUS, data->MainControllerSize, DESCFLAGS_SET::NONE);
			lastMainController->SetParameter(NULLOBJECT_ORIENTATION, NULLOBJECT_ORIENTATION_SCREEN, DESCFLAGS_SET::NONE);
			lastMainController->SetLayerObject(_controllersLayer);

			lastMainController->InsertUnderLast(controllersContainer);
			mainControllers[data->SegmentsCount] = allControllers[ctrlIndex] = lastMainController;

#pragma endregion
		}

#pragma region create base IK spline tag

		BaseTag* ikSplineTag = baseStartJoint->MakeTag(Tcaikspline);
		ikSplineTag->SetParameter(ID_CA_IKSPLINE_TAG_TYPE, ID_CA_IKSPLINE_TAG_TYPE_EQUAL, DESCFLAGS_SET::NONE);
		ikSplineTag->SetParameter(ID_CA_IKSPLINE_TAG_TWIST_TYPE, ID_CA_IKSPLINE_TAG_TWIST_TYPE_LOCAL, DESCFLAGS_SET::NONE);
		ikSplineTag->SetParameter(ID_CA_IKSPLINE_TAG_END, baseEndJoint, DESCFLAGS_SET::NONE);
		ikSplineTag->SetParameter(ID_CA_IKSPLINE_TAG_SPLINE, baseIkSpline, DESCFLAGS_SET::NONE);
		ikSplineTag->SetParameter(ID_CA_IKSPLINE_HANDLE_COUNT, 2, DESCFLAGS_SET::NONE);
		ikSplineTag->SetParameter(10002, mainControllers[segmentI], DESCFLAGS_SET::NONE); // Handle Link
		ikSplineTag->SetParameter(10003, 1, DESCFLAGS_SET::NONE); // Handle Twist
		if (!isFirstSegment)
		{
			ikSplineTag->SetParameter(10004, segmentWidth / 4, DESCFLAGS_SET::NONE); // Handle Depth
		}
		ikSplineTag->SetParameter(10011, 1, DESCFLAGS_SET::NONE); // Handle Index
		ikSplineTag->SetParameter(10012, mainControllers[segmentI + 1], DESCFLAGS_SET::NONE); // Handle Link
		ikSplineTag->SetParameter(10013, 1, DESCFLAGS_SET::NONE); // Handle Twist
		if (!isLastSegment)
		{
			ikSplineTag->SetParameter(10014, segmentWidth / 4, DESCFLAGS_SET::NONE); // Handle Depth
		}

#pragma endregion
	}

#pragma region create detail IK spline

	SplineObject* detailIkSpline = SplineObject::Alloc(detailIkSplinePointsCount, SPLINETYPE::LINEAR);
	detailIkSpline->SetName("Detail.IkSpline"_s);
	detailIkSpline->SetLayerObject(_invisibleLayer);
	Vector* points = detailIkSpline->GetPointW();

	for (int i = 0; i < detailIkSplinePointsCount; i++)
	{
		points[i] = Vector(leftmostPoint + i * segmentSubdivWidth, 0, 0);
	}

	detailIkSpline->InsertUnderLast(op);

#pragma endregion

#pragma region create detail joints

	BaseObject* parentJoint = BaseObject::Alloc(Ojoint);
	parentJoint->SetName("Detail.Joint.0"_s);
	parentJoint->SetFrozenPos(Vector(leftmostPoint, 0, 0));
	parentJoint->InsertUnderLast(jointsContainer);
	parentJoint->SetLayerObject(_jointsLayer);

#pragma region create detail IK spline tag

	BaseTag* detailIkSplineTag = parentJoint->MakeTag(Tcaikspline);
	detailIkSplineTag->SetParameter(ID_CA_IKSPLINE_TAG_TYPE, ID_CA_IKSPLINE_TAG_TYPE_EQUAL, DESCFLAGS_SET::NONE);
	detailIkSplineTag->SetParameter(ID_CA_IKSPLINE_TAG_TWIST_TYPE, ID_CA_IKSPLINE_TAG_TWIST_TYPE_LOCAL, DESCFLAGS_SET::NONE);
	detailIkSplineTag->SetParameter(ID_CA_IKSPLINE_TAG_SPLINE, detailIkSpline, DESCFLAGS_SET::NONE);

#pragma endregion

	for (int i = 1; i < detailIkSplinePointsCount; i++)
	{
		maxon::String detailJointName("Detail.Joint.");
		detailJointName.AppendInt(i);
		BaseObject* detailJoint = BaseObject::Alloc(Ojoint);
		detailJoint->SetName(detailJointName);
		detailJoint->SetFrozenPos(Vector(segmentSubdivWidth, 0, 0));
		detailJoint->SetLayerObject(_jointsLayer);

		detailJoint->InsertUnderLast(parentJoint);

		parentJoint = detailJoint;
	}

	detailIkSplineTag->SetParameter(ID_CA_IKSPLINE_TAG_END, parentJoint, DESCFLAGS_SET::NONE);
	detailIkSplineTag->SetParameter(ID_CA_IKSPLINE_HANDLE_COUNT, detailIkSplinePointsCount, DESCFLAGS_SET::NONE);

	for (int i = 0; i < detailIkSplinePointsCount; i++)
	{
		detailIkSplineTag->SetParameter(10001 + i * 10, i, DESCFLAGS_SET::NONE); // Handle Index
		detailIkSplineTag->SetParameter(10002 + i * 10, allControllers[i], DESCFLAGS_SET::NONE); // Handle Link
		detailIkSplineTag->SetParameter(10003 + i * 10, 1, DESCFLAGS_SET::NONE); // Handle Twist
	}

#pragma endregion

	delete[]mainControllers;
}

void AdvancedIkSpline::resetLayerLocks(BaseObject* op)
{
	switch (getData(op)->Mode)
	{
	case ADVANCEDIKSPLINE_MODE_SETUP:
		LayerUtilities::ControlLayer(_controllersLayer, true, false, false);
		LayerUtilities::ControlLayer(_jointsLayer, true, false, false);
		break;
	case ADVANCEDIKSPLINE_MODE_BIND:
		LayerUtilities::ControlLayer(_controllersLayer, false, false, false);
		LayerUtilities::ControlLayer(_jointsLayer, true, true, true);
		break;
	case ADVANCEDIKSPLINE_MODE_ANIMATE:
		LayerUtilities::ControlLayer(_controllersLayer, true, true, true);
		LayerUtilities::ControlLayer(_jointsLayer, false, false, false);
		break;
	}
}

EXECUTIONRESULT AdvancedIkSpline::Execute(BaseObject* op, BaseDocument* doc, BaseThread* bt, Int32 priority, EXECUTIONFLAGS flags)
{
	doc->StartUndo();

	BaseObject* sphere = BaseObject::Alloc(Osphere);
	doc->InsertObject(sphere, nullptr, nullptr);

	doc->AddUndo(UNDOTYPE::NEWOBJ, sphere);

	doc->EndUndo();

	return EXECUTIONRESULT::OK;
}

Bool RegisterAdvancedIkSpline()
{
	return RegisterObjectPlugin(PLUGINID, GeLoadString(IDS_ADVANCEDIKSPLINE), OBJECT_GENERATOR, AdvancedIkSpline::Alloc, "Oadvancedikspline"_s, AutoBitmap("AdvancedIkSpline.jpg"_s), 0);
}