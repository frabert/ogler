/*
    Ogler - Use OpenGL shaders in REAPER
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
*/

#pragma once

#include <Scintilla.h>

namespace ogler {
enum OglerStyle {
  STY_Keyword = STYLE_LASTPREDEFINED + 1,
  STY_Type,
  STY_Integer,
  STY_Float,
  STY_Bool,
  STY_Ident,
  STY_Operator,
  STY_String,
  STY_BuiltinFunc,
  STY_Punctuation,
  STY_Comment,

  STY_ErrorAnnotation,
};
}