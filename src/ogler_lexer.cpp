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

#include "ogler_lexer.hpp"

#include "ogler_styles.hpp"

#include <regex>
#include <string>

namespace ogler {
int SCI_METHOD GlslLexer::Version() const { return Scintilla::lvRelease5; }

void SCI_METHOD GlslLexer::Release() { delete this; }

const char *SCI_METHOD GlslLexer::PropertyNames() { return ""; }

int SCI_METHOD GlslLexer::PropertyType(const char *name) { return 0; }

const char *SCI_METHOD GlslLexer::DescribeProperty(const char *name) {
  return "";
}

Sci_Position SCI_METHOD GlslLexer::PropertySet(const char *key,
                                               const char *val) {
  return 0;
}

const char *SCI_METHOD GlslLexer::DescribeWordListSets() { return ""; }

Sci_Position SCI_METHOD GlslLexer::WordListSet(int n, const char *wl) {
  return 0;
}

void SCI_METHOD GlslLexer::Fold(Sci_PositionU startPos, Sci_Position lengthDoc,
                                int initStyle, Scintilla::IDocument *pAccess) {}

void *SCI_METHOD GlslLexer::PrivateCall(int operation, void *pointer) {
  return nullptr;
}

int SCI_METHOD GlslLexer::LineEndTypesSupported() { return 0; }

int SCI_METHOD GlslLexer::AllocateSubStyles(int styleBase, int numberStyles) {
  return 0;
}

int SCI_METHOD GlslLexer::SubStylesStart(int styleBase) { return 0; }

int SCI_METHOD GlslLexer::SubStylesLength(int styleBase) { return 0; }

int SCI_METHOD GlslLexer::StyleFromSubStyle(int subStyle) { return 0; }

int SCI_METHOD GlslLexer::PrimaryStyleFromStyle(int style) { return 0; }

void SCI_METHOD GlslLexer::FreeSubStyles() {}

void SCI_METHOD GlslLexer::SetIdentifiers(int style, const char *identifiers) {}

int SCI_METHOD GlslLexer::DistanceToSecondaryStyles() { return 0; }

const char *SCI_METHOD GlslLexer::GetSubStyleBases() { return ""; }

int SCI_METHOD GlslLexer::NamedStyles() { return 0; }

const char *SCI_METHOD GlslLexer::NameOfStyle(int style) { return ""; }

const char *SCI_METHOD GlslLexer::TagsOfStyle(int style) { return ""; }

const char *SCI_METHOD GlslLexer::DescriptionOfStyle(int style) { return ""; }

const char *SCI_METHOD GlslLexer::GetName() { return "GLSL"; }

int SCI_METHOD GlslLexer::GetIdentifier() { return 0xF00F00B5; }

const char *SCI_METHOD GlslLexer::PropertyGet(const char *key) { return ""; }

} // namespace ogler