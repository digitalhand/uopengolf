# Open Golf Online notices

The original plugin code is covered by the repository's MIT license.

The QR encoder in `Source/OpenGolfOnline/Private/ThirdParty/` is Project Nayuki's
QR Code generator, version 1.8.0, under the MIT license. Its complete copyright,
permission and warranty notice is retained at the top of both source files.

Source: https://github.com/nayuki/QR-Code-generator/tree/v1.8.0/c

The C implementation is compiled as C++ for UnrealBuildTool. Its own header was
moved before the system includes to satisfy Unreal's include-order check.

OpenSSL and libcurl are provided by Unreal Engine; their applicable engine-shipped
third-party notices continue to apply. They are not copied into this plugin.
