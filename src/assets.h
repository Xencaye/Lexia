#pragma once
// The files the UI draws with (the icons, the engravings, the font) as resources of the exe (K.1a,
// ADR-0009). CMake lists ui/icons/*.svg, ui/img/*.jpg and ui/fonts/*.woff2 into a generated assets.rc
// (RCDATA, named "<KIND>/<stem>"), so a new file needs no hand edit and the program has no asset
// folder to read: nothing outside the exe can change what the UI loads. lexia.exe and lexia_barshot.exe
// carry them; lexia_core and the tests do not.
//
// Win32 (the module's own resources), so it is in lexia.exe, not lexia_core. Any thread.
#include <string_view>

enum class AssetKind {
    Icon,     // ui/icons/<name>.svg
    Picture,  // ui/img/<name>.jpg
    Font,     // ui/fonts/<name>.woff2
};

// The bytes of the asset `name` (the file name without its extension: "home-05", "bee",
// "AtkinsonHyperlegibleNext-Bold") from the exe's own resources. The view points into the loaded
// image and is valid for the life of the process. A name that is not there (or not a plain name)
// gives an empty view and one log line for that name. The files of the disk are never read.
std::string_view AssetBytes(AssetKind kind, std::string_view name);
