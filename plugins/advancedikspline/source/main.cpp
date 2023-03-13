#include "main.h"

Bool PluginStart()
{
	return RegisterAdvancedIkSpline();
}

void PluginEnd()
{

}

Bool PluginMessage(Int32 id, void* data)
{
	switch (id)
	{
	case C4DPL_INIT_SYS:
		if (!g_resource.Init())
			return false;
		return true;
	}
	return false;
}