#pragma once

#include <map>
#include <string>

namespace craftpp::gui {

// Minimal en_US.lang loader (key=value, CRLF tolerated). Missing keys
// translate to the key itself (like StringTranslate fallback).
std::map<std::string, std::string> load_lang(const std::string& path);
std::string tr(const std::map<std::string, std::string>& lang, const std::string& key);

// Java String.hashCode (UTF-16 code units) for seed words.
long java_string_hash(const std::string& utf8);

}  // namespace craftpp::gui
