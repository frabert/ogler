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

#include "ogler_resources.hpp"
#include "resources.hpp"

#include <sqlite3.h>
#include <zlib.h>

namespace ogler {

struct ResourceDB {
  sqlite3 *db;
  sqlite3_stmt *stmt;

  ResourceDB() {
    sqlite3_config(SQLITE_CONFIG_SERIALIZED);
    sqlite3_open(":memory:", &db);
    sqlite3_deserialize(
        db, "main", const_cast<unsigned char *>(get_resources()),
        resources_size, resources_size, SQLITE_DESERIALIZE_READONLY);
    sqlite3_prepare_v2(db,
                       "SELECT method, size, data FROM files WHERE name = ?",
                       -1, &stmt, nullptr);
  }

  ~ResourceDB() {
    sqlite3_finalize(stmt);
    sqlite3_close_v2(db);
  }
};

enum CompressionMethod { None, Deflate };

std::vector<unsigned char> get_resource(std::wstring_view name) {
  static ResourceDB resdb;
  sqlite3_reset(resdb.stmt);
  sqlite3_bind_text16(resdb.stmt, 1, name.data(), name.size() * sizeof(wchar_t),
                      SQLITE_TRANSIENT);
  auto res = sqlite3_step(resdb.stmt);
  if (res != SQLITE_ROW) {
    return {};
  }

  auto method = sqlite3_column_int(resdb.stmt, 0);
  auto expanded_size = sqlite3_column_int(resdb.stmt, 1);
  auto blob =
      static_cast<const unsigned char *>(sqlite3_column_blob(resdb.stmt, 2));
  auto blob_sz = sqlite3_column_bytes(resdb.stmt, 2);

  if (method == None) {
    return std::vector<unsigned char>(blob, blob + blob_sz);
  }

  if (method == Deflate) {
    std::vector<unsigned char> res(expanded_size);
    unsigned long dest_sz = res.size();
    uncompress(res.data(), &dest_sz, blob, blob_sz);
    return res;
  }

  return {};
}
} // namespace ogler