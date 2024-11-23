## Note

The project is a fork of Daniel Amthauer's UE4-HardwareBreakpointsPlugin  
[Daniel's post on public Unreal Engine forum](https://forums.unrealengine.com/t/free-hardware-breakpoints-a-plugin-to-let-you-set-data-breakpoints-on-any-variable-bp-or-c/210251)  
[Original repo](https://bitbucket.org/damthauer/ue4-hardwarebreakpointsplugin/src/master/)  

Daniel Amthauer's approved the fork creation. :)

The aim of the fork is to add and improve support of certain features:
- [x] Port to UE5
- [x] Add support for easy calls in immediate window of VS debugger
- [ ] Fix immediate window calls for breakpoints are delayed on 1 frame because calling functions in debugger has limitations on context manipulations
- [ ] Update and link example project

Changes to the original repo:
* Stripped example projects for easier integration

Note project is aimed on developers, I'm not planning on testing and/or distributing the precompiled version.

# BELOW IS ORIGINAL README
# Hardware Breakpoints
![UE4](https://img.shields.io/badge/UE4-4.24%2B-orange)

This plugin allows users to place data breakpoints during runtime in a simple way from Blueprints or C++. 
Data breakpoints can help you detect which part of your code or blueprints is modifying a variable in your object. This can help you track down difficult bugs.

## Installation:

### From HardwareBreakpoints_1.x.x_for_4.2x.x.zip file:

Unzip the contents of the .zip file to your project's Plugins folder, or to the Plugins/Marketplace folder on your UE4 installation

### From repository download:

Copy the contents of the Plugin folder, to your project's Plugins folder, or to the Plugins/Marketplace folder on your UE4 installation

## Usage notes:

* Some functionality is dependent on having the **Engine Symbols for debugging** installed. Check the *Options* for your installed UE4 version from the Epic Games Launcher.

![Engine Options](https://bitbucket.org/damthauer/ue4-hardwarebreakpointsplugin/raw/ed9a0d81f1f56f99a6890a3a174c98b980c8a5f7/Docs/Images/EngineOptions.png)

![Engine Symbols for debugging](https://bitbucket.org/damthauer/ue4-hardwarebreakpointsplugin/raw/ed9a0d81f1f56f99a6890a3a174c98b980c8a5f7/Docs/Images/EngineSymbols.png)


Licensed under the terms of the Apache License 2.0
