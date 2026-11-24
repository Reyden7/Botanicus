// MIT License

// Copyright (c) 2026 Nekki Limited.
// Copyright (c) 2022 Autodesk, Inc.

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

// Module class for the CascLiveLinkInterface plugin. Manages the module lifecycle and provides access to version information.
class FCascLiveLinkInterfaceModule : public IModuleInterface
{
public:
	// Called when the module is loaded into memory. Performs all necessary initialization.
	virtual void StartupModule() override;

	// Called before the module is unloaded from memory. Performs all necessary cleanup.
	virtual void ShutdownModule() override;

	// Returns a reference to the loaded CascLiveLinkInterface module instance. Loads the module if it has not been loaded yet.
	static inline FCascLiveLinkInterfaceModule& GetModule()
	{
		static const FName ModuleName = "CascLiveLinkInterface";
		return FModuleManager::LoadModuleChecked<FCascLiveLinkInterfaceModule>(ModuleName);
	}

	// Returns true if the CascLiveLinkInterface module is currently loaded.
	static inline bool IsModuleLoaded()
	{
		static const FName ModuleName = "CascLiveLinkInterface";
		return FModuleManager::Get().IsModuleLoaded(ModuleName);
	}

	// Returns the current version string of the CascLiveLink plugin.
	static CASCLIVELINKINTERFACE_API const FString& GetPluginVersion();

	// Returns the current Unreal Engine version string.
	static CASCLIVELINKINTERFACE_API const FString& GetEngineVersion();
};
