# openxr-renderdoc-layer
[RenderDoc](https://github.com/baldurk/renderdoc) capturing layer for OpenXR

![Sample](/img/capturesample.png)

## Usage (Windows)

* Use [OpenXR Layer GUI](https://github.com/fredemmott/OpenXR-API-Layers-GUI) to add and enable the *renderdoc-layer.json* if your running with evelvated context to avoid the loader rejecting the layer:
```
Error [GENERAL | platform_utils | OpenXR-Loader] : !!! WARNING !!! Environment variable XR_API_LAYER_PATH is being ignored due to running from an elevated context.
```
* Alternatively set XR_API_LAYER_PATH=path\to\renderdoc-layer.json 
* Launch your application trough RenderDoc
    * ![Launch](/img/launchrd.png)
* Click YES in the capture popup
    * ![Capture](/img/capturemsg.png)

## License
MIT License

Copyright (c) 2024 Fabian Wahlster
