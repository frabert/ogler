/*
    Ogler - Use GLSL shaders in REAPER
    Copyright (C) 2023  Francesco Bertolaccini <francesco@bertolaccini.dev>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.

    Additional permission under GNU GPL version 3 section 7

    If you modify this Program, or any covered work, by linking or
    combining it with Sciter (or a modified version of that library),
    containing parts covered by the terms of Sciter's EULA, the licensors
    of this Program grant you additional permission to convey the
    resulting work.
*/

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <sqlite3.h>
#include <zlib.h>

size_t read_file(const std::filesystem::path &file, std::vector<char> &buffer) {
  std::ifstream stream(file, std::ios::ate);
  auto sz = stream.tellg();
  buffer.resize(sz);
  stream.seekg(0);
  stream.read(buffer.data(), sz);
  return sz;
}

int main(int argc, char *argv[]) {
  std::vector<std::string_view> args;
  args.reserve(argc);
  std::transform(std::make_reverse_iterator(argv + argc),
                 std::make_reverse_iterator(argv), std::back_inserter(args),
                 [](char *arg) { return std::string_view(arg); });
  if (args.empty()) {
    return EXIT_FAILURE;
  }
  auto executable = args.back();
  args.pop_back();

  std::vector<std::pair<std::filesystem::path, std::string_view>> directories;
  std::optional<std::string_view> depfile;
  std::optional<std::string_view> output;
  std::optional<std::string_view> varname;
  while (!args.empty()) {
    auto arg = args.back();
    args.pop_back();
    if (arg == "--depfile") {
      if (args.empty()) {
        std::cerr << "Expected file name" << std::endl;
        return EXIT_FAILURE;
      }
      depfile = args.back();
      args.pop_back();
    } else if (arg == "--output") {
      if (args.empty()) {
        std::cerr << "Expected file name" << std::endl;
        return EXIT_FAILURE;
      }
      output = args.back();
      args.pop_back();
    } else if (arg == "--varname") {
      if (args.empty()) {
        std::cerr << "Expected variable name" << std::endl;
        return EXIT_FAILURE;
      }
      varname = args.back();
      args.pop_back();
    } else {
      if (args.empty()) {
        std::cerr << "Expected target dir name" << std::endl;
        return EXIT_FAILURE;
      }
      auto name = args.back();
      args.pop_back();

      auto &[dir, _] = directories.emplace_back(arg, name);
      if (!std::filesystem::is_directory(dir)) {
        std::cerr << "Not a directory: " << dir << std::endl;
        return EXIT_FAILURE;
      }
    }
  }

  if (!output) {
    std::cerr << "No output file specified" << std::endl;
    return EXIT_FAILURE;
  }

  auto resname = varname ? *varname : "resources";

  sqlite3 *db;
  auto res = sqlite3_open(":memory:", &db);
  if (res != SQLITE_OK) {
    std::cerr << sqlite3_errstr(res) << std::endl;
    return EXIT_FAILURE;
  }

  std::vector<std::filesystem::path> dependencies;

  res = sqlite3_exec(db, "CREATE TABLE files(name, method, size, data)",
                     nullptr, nullptr, nullptr);
  if (res != SQLITE_OK) {
    std::cerr << sqlite3_errstr(res) << std::endl;
    return EXIT_FAILURE;
  }

  sqlite3_stmt *stmt;
  res = sqlite3_prepare_v2(
      db, "INSERT INTO files(name, method, size, data) VALUES (?, ?, ?, ?)", -1,
      &stmt, nullptr);
  if (res != SQLITE_OK) {
    std::cerr << sqlite3_errstr(res) << std::endl;
    return EXIT_FAILURE;
  }

  std::vector<char> file_buffer, data_buffer;
  for (auto &[dir, dname] : directories) {
    auto parent = std::filesystem::absolute(dir);
    for (auto file : std::filesystem::recursive_directory_iterator(dir)) {
      if (!file.is_regular_file()) {
        continue;
      }
      dependencies.push_back(std::filesystem::absolute(file.path()));
      auto name = (dname / std::filesystem::relative(file.path(), parent))
                      .generic_string();
      auto src_sz = read_file(file.path(), file_buffer);

      data_buffer.resize(compressBound(src_sz));
      uLongf dst_sz = data_buffer.size();
      res = compress(reinterpret_cast<Bytef *>(data_buffer.data()), &dst_sz,
                     reinterpret_cast<Bytef *>(file_buffer.data()), src_sz);
      if (res != Z_OK) {
        std::cerr << "zlib error " << res << std::endl;
        return EXIT_FAILURE;
      }

      sqlite3_reset(stmt);
      sqlite3_bind_text(stmt, 1, name.c_str(), name.size(), SQLITE_TRANSIENT);
      sqlite3_bind_int64(stmt, 3, src_sz);
      if (dst_sz < src_sz) {
        sqlite3_bind_int(stmt, 2, 1);
        sqlite3_bind_blob(stmt, 4, data_buffer.data(), dst_sz,
                          SQLITE_TRANSIENT);
      } else {
        sqlite3_bind_int(stmt, 2, 0);
        sqlite3_bind_blob(stmt, 4, file_buffer.data(), src_sz,
                          SQLITE_TRANSIENT);
      }
      sqlite3_step(stmt);
    }
  }
  sqlite3_finalize(stmt);

  {
    sqlite3_int64 db_sz;
    auto buf = sqlite3_serialize(db, "main", &db_sz, 0);

    std::ofstream resources_stream(std::filesystem::path{*output} /
                                   (std::string(resname) + ".cpp"));
    resources_stream << "#include \"" << resname
                     << ".hpp\"\nstatic const unsigned char resources";
    resources_stream << "[] = {";
    for (size_t i = 0; i < db_sz; ++i) {
      if (i % 16 == 0) {
        resources_stream << '\n';
      }
      auto chr = buf[i];
      if (chr >= 32 && chr <= 126 && chr != '\'' && chr != '\\') {
        resources_stream << '\'' << chr << "',";
      } else {
        resources_stream << std::hex << std::setw(2) << "0x" << (int)chr << ',';
      }
    }
    resources_stream << "};\nconst unsigned char *get_" << resname
                     << "() { return resources; }\n";

    std::ofstream header_stream(std::filesystem::path{*output} /
                                (std::string(resname) + ".hpp"));
    header_stream << "#pragma once\nconst unsigned char *get_" << resname
                  << "();\nstatic constexpr long long int " << resname
                  << "_size = " << db_sz << ";\n";

    sqlite3_close_v2(db);
  }

  if (depfile) {
    auto out_path =
        std::filesystem::absolute(*output) / (std::string(resname) + ".cpp");
    std::ofstream depfile_stream(std::filesystem::path{*depfile});
    depfile_stream << out_path.string() << ':';
    for (auto dep : dependencies) {
      depfile_stream << ' ' << dep.string();
    }
    depfile_stream << '\n';
  }
}