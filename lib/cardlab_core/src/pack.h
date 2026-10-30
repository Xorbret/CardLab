// Content packs: one file that carries every lesson and scenario, so the
// device can update its SD card content with a single download.
//
//   CARDLAB-PACK 1 <version>
//   FILE lessons/t0-01-hello-lua.lua <size>
//   <size bytes>
//   ...
//   END
#pragma once

#include <string>
#include <vector>

namespace cardlab {

struct PackFile {
  std::string path;  // relative to /cardlab, e.g. "lessons/x.lua"
  std::string data;
};

struct Pack {
  std::string version;
  std::vector<PackFile> files;
};

// Only lessons/*.lua and scenarios/*.lua with plain names are accepted, so
// a pack can never write outside its two folders.
bool packPathAllowed(const std::string& path);

bool parsePack(const std::string& text, Pack& out, std::string& err);

}  // namespace cardlab
