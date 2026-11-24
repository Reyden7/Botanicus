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

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Features/IModularFeatures.h"

class FCascLiveLinkPresenceDetector;

// Module class for the CascLiveLink plugin. Manages the module lifecycle and provides access to the presence detector.
class FCascLiveLinkModule : public IModuleInterface
{
public:
	// Default constructor. Performs basic member initialization.
	FCascLiveLinkModule();

	// Returns a reference to the loaded CascLiveLink module instance.
	static FCascLiveLinkModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FCascLiveLinkModule>("CascLiveLink");
	}

	// Called when the module is loaded into memory. Performs all necessary initialization.
	virtual void StartupModule() override;

	// Called before the module is unloaded from memory. Performs all necessary cleanup.
	virtual void ShutdownModule() override;

	// Returns false to indicate that this module does not support dynamic reloading at runtime.
	virtual bool SupportsDynamicReloading() override { return false; }

	// Returns a reference to the presence detector responsible for discovering available CascLiveLink providers.
	virtual FCascLiveLinkPresenceDetector& GetPresenceDetector() { return *PresenceDetector; }

private:
	// Owns the presence detector instance used to discover and monitor available CascLiveLink providers.
	TUniquePtr<FCascLiveLinkPresenceDetector> PresenceDetector;
};