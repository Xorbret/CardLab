#include "pack.h"

#include <cstdlib>

namespace cardlab {

namespace {

bool readLine(const std::string& text, size_t& pos, std::string& line) {
  if (pos >= text.size()) return false;
  size_t nl = text.find('\n', pos);
  if (nl == std::string::npos) return false;
  line = text.substr(pos, nl - pos);
  pos = nl + 1;
  return true;
}

bool plainName(const std::string& name) {
  if (name.size() <= 4 || name.compare(name.size() - 4, 4, ".lua") != 0) return false;
  for (char c : name) {
    bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ||
              c == '_' || c == '.';
    if (!ok) return false;
  }
  return name.find("..") == std::string::npos;
}

}  // namespace

bool packPathAllowed(const std::string& path) {
  for (const char* dir : {"lessons/", "scenarios/"}) {
    std::string prefix = dir;
    if (path.compare(0, prefix.size(), prefix) == 0) return plainName(path.substr(prefix.size()));
  }
  return false;
}

bool parsePack(const std::string& text, Pack& out, std::string& err) {
  out = Pack();
  size_t pos = 0;
  std::string line;
  const std::string magic = "CARDLAB-PACK 1 ";
  if (!readLine(text, pos, line) || line.compare(0, magic.size(), magic) != 0) {
    err = "not a CardLab content pack";
    return false;
  }
  out.version = line.substr(magic.size());

  while (readLine(text, pos, line)) {
    if (line == "END") return true;
    if (line.compare(0, 5, "FILE ") != 0) {
      err = "bad pack entry: " + line.substr(0, 40);
      return false;
    }
    size_t sp = line.rfind(' ');
    std::string path = line.substr(5, sp - 5);
    char* end = nullptr;
    unsigned long size = std::strtoul(line.c_str() + sp + 1, &end, 10);
    if (sp <= 5 || *end != '\0' || !packPathAllowed(path)) {
      err = "bad pack file entry: " + path;
      return false;
    }
    if (pos + size + 1 > text.size() || text[pos + size] != '\n') {
      err = "pack is truncated at " + path;
      return false;
    }
    out.files.push_back({path, text.substr(pos, size)});
    pos += size + 1;
  }
  err = "pack is truncated (no END)";
  return false;
}

}  // namespace cardlab
