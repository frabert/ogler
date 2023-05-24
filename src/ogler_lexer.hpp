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
*/

#pragma once

#include <ILexer.h>

namespace ogler {

class GlslLexer : public Scintilla::ILexer5 {
public:
  int SCI_METHOD Version() const final;
  void SCI_METHOD Release() final;
  const char *SCI_METHOD PropertyNames() final;
  int SCI_METHOD PropertyType(const char *name) final;
  const char *SCI_METHOD DescribeProperty(const char *name) final;
  Sci_Position SCI_METHOD PropertySet(const char *key, const char *val) final;
  const char *SCI_METHOD DescribeWordListSets() final;
  Sci_Position SCI_METHOD WordListSet(int n, const char *wl) final;
  void SCI_METHOD Lex(Sci_PositionU startPos, Sci_Position lengthDoc,
                      int initStyle, Scintilla::IDocument *pAccess) final;
  void SCI_METHOD Fold(Sci_PositionU startPos, Sci_Position lengthDoc,
                       int initStyle, Scintilla::IDocument *pAccess) final;
  void *SCI_METHOD PrivateCall(int operation, void *pointer) final;
  int SCI_METHOD LineEndTypesSupported() final;
  int SCI_METHOD AllocateSubStyles(int styleBase, int numberStyles) final;
  int SCI_METHOD SubStylesStart(int styleBase) final;
  int SCI_METHOD SubStylesLength(int styleBase) final;
  int SCI_METHOD StyleFromSubStyle(int subStyle) final;
  int SCI_METHOD PrimaryStyleFromStyle(int style) final;
  void SCI_METHOD FreeSubStyles() final;
  void SCI_METHOD SetIdentifiers(int style, const char *identifiers) final;
  int SCI_METHOD DistanceToSecondaryStyles() final;
  const char *SCI_METHOD GetSubStyleBases() final;
  int SCI_METHOD NamedStyles() final;
  const char *SCI_METHOD NameOfStyle(int style) final;
  const char *SCI_METHOD TagsOfStyle(int style) final;
  const char *SCI_METHOD DescriptionOfStyle(int style) final;
  const char *SCI_METHOD GetName() final;
  int SCI_METHOD GetIdentifier() final;
  const char *SCI_METHOD PropertyGet(const char *key) final;
};

} // namespace ogler