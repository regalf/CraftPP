// Screens logic tests: folder sanitize, seed parse, lang fallback.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

#include "gui/lang.hpp"
#include "gui/screens.hpp"

namespace {

TEST_CASE("java_string_hash matches Java", "[screens]") {
  // "hello".hashCode() == 99162322 (well-known Java value).
  CHECK(craftpp::gui::java_string_hash("hello") == 99162322L);
  CHECK(craftpp::gui::java_string_hash("") == 0L);
}

TEST_CASE("parse_seed follows GuiCreateWorld", "[screens]") {
  CHECK(craftpp::gui::parse_seed("", 42L) == 42L);
  CHECK(craftpp::gui::parse_seed("12345", 42L) == 12345L);
  CHECK(craftpp::gui::parse_seed("0", 42L) == 42L);  // zero falls back
  CHECK(craftpp::gui::parse_seed("hello", 42L) == 99162322L);
}

TEST_CASE("make_world_folder sanitizes and dedups", "[screens]") {
  const std::string root = std::filesystem::temp_directory_path().string() + "/craftpp_folder_test";
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
  std::filesystem::create_directories(root + "/World", ec);
  { std::ofstream f(root + "/World/level.dat"); }
  CHECK(craftpp::gui::make_world_folder(root, "a/b?c") == "a_b_c");
  CHECK(craftpp::gui::make_world_folder(root, "   ") == "World-");
  CHECK(craftpp::gui::make_world_folder(root, "New World") == "New World");
  std::filesystem::remove_all(root, ec);
}

TEST_CASE("lang loader falls back to the key", "[screens]") {
  const auto lang = craftpp::gui::load_lang("assets/lang/en_US.lang");
  REQUIRE(!lang.empty());
  CHECK(craftpp::gui::tr(lang, "menu.singleplayer") == "Singleplayer");
  CHECK(craftpp::gui::tr(lang, "no.such.key") == "no.such.key");
}

}  // namespace
