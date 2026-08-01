// Blueprint Internals Toolset.

#include "BlueprintInternalsToolset.h"

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "ToolsetRegistry/UToolsetRegistry.h"

class FBlueprintInternalsToolsetModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UToolsetRegistry::RegisterToolsetClass(UBlueprintInternalsToolset::StaticClass());
	}

	virtual void ShutdownModule() override
	{
		UToolsetRegistry::UnregisterToolsetClass(UBlueprintInternalsToolset::StaticClass());
	}
};

IMPLEMENT_MODULE(FBlueprintInternalsToolsetModule, BlueprintInternalsToolset);
