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

#include "ogler_lexer.hpp"

#include "ogler_styles.hpp"

#include <vector>

namespace ogler {

void SCI_METHOD GlslLexer::Lex(Sci_PositionU startPos, Sci_Position lengthDoc,
                               int initStyle, Scintilla::IDocument *pAccess) {
  std::vector<char> buffer;
  buffer.resize(lengthDoc + 1);
  pAccess->GetCharRange(buffer.data(), startPos, lengthDoc);
  const char *YYCURSOR = buffer.data();
  const char *YYLIMIT = YYCURSOR + buffer.size();
  const char *YYMARKER;
  /*!include:re2c "glsl.re" */
  for (;;) {
    auto tok = YYCURSOR;
    /*!re2c
        re2c:yyfill:enable = 0;
        re2c:eof = 0;
        re2c:define:YYCTYPE = char;

        ws { continue; }
        scm | mcm {
            auto dist = tok - buffer.data();
            auto pos = startPos + dist;
            auto len = YYCURSOR - tok;
            pAccess->StartStyling(pos);
            pAccess->SetStyleFor(len, STY_Comment);
            continue;
        }
        type {
            auto dist = tok - buffer.data();
            auto pos = startPos + dist;
            auto len = YYCURSOR - tok;
            pAccess->StartStyling(pos);
            pAccess->SetStyleFor(len, STY_Type);
            continue;
        }
        type {
            auto dist = tok - buffer.data();
            auto pos = startPos + dist;
            auto len = YYCURSOR - tok;
            pAccess->StartStyling(pos);
            pAccess->SetStyleFor(len, STY_Type);
            continue;
        }
        keyword {
            auto dist = tok - buffer.data();
            auto pos = startPos + dist;
            auto len = YYCURSOR - tok;
            pAccess->StartStyling(pos);
            pAccess->SetStyleFor(len, STY_Keyword);
            continue;
        }
        punct {
            auto dist = tok - buffer.data();
            auto pos = startPos + dist;
            auto len = YYCURSOR - tok;
            pAccess->StartStyling(pos);
            pAccess->SetStyleFor(len, STY_Punctuation);
            continue;
        }
        builtins {
            auto dist = tok - buffer.data();
            auto pos = startPos + dist;
            auto len = YYCURSOR - tok;
            pAccess->StartStyling(pos);
            pAccess->SetStyleFor(len, STY_BuiltinFunc);
            continue;
        }
        ident {
            auto dist = tok - buffer.data();
            auto pos = startPos + dist;
            auto len = YYCURSOR - tok;
            pAccess->StartStyling(pos);
            pAccess->SetStyleFor(len, STY_Ident);
            continue;
        }
        int_lit {
            auto dist = tok - buffer.data();
            auto pos = startPos + dist;
            auto len = YYCURSOR - tok;
            pAccess->StartStyling(pos);
            pAccess->SetStyleFor(len, STY_Integer);
            continue;
        }
        float_lit {
            auto dist = tok - buffer.data();
            auto pos = startPos + dist;
            auto len = YYCURSOR - tok;
            pAccess->StartStyling(pos);
            pAccess->SetStyleFor(len, STY_Float);
            continue;
        }

        * { continue; }
        $ { return; }
    */
  }
}
} // namespace ogler