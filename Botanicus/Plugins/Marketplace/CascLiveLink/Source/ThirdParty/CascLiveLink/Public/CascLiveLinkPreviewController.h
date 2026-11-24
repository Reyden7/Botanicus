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

#include "PersonaPreviewSceneController.h"

#include "CascLiveLinkPreviewController.generated.h"

// Preview scene controller that integrates CascLiveLink data into the Persona editor viewport.
UCLASS()
class UCascLiveLinkPreviewController : public UPersonaPreviewSceneController
{
public:
	GENERATED_BODY()

	// If true, synchronizes the Persona editor camera with the incoming LiveLink camera data.
	UPROPERTY(EditAnywhere, Category = "Live Link")
	bool bEnableCameraSync = true;

	// If true, drives the AnimSequence editor timeline position from the incoming LiveLink time data.
	UPROPERTY(EditAnywhere, Category = "Live Link")
	bool bEnableAnimSequenceEditorTime = true;

	// Sets up the LiveLink connection and applies initial controller state to the given preview scene.
	virtual void InitializeView(UPersonaPreviewSceneDescription* SceneDescription, IPersonaPreviewScene* PreviewScene) const override;

	// Tears down the LiveLink connection and cleans up the controller state from the given preview scene.
	virtual void UninitializeView(UPersonaPreviewSceneDescription* SceneDescription, IPersonaPreviewScene* PreviewScene) const override;
};
