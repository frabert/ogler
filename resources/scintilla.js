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

export const STYLE_DEFAULT = 32;
export const STYLE_LINENUMBER = 33;
export const STYLE_BRACELIGHT = 34;
export const STYLE_BRACEBAD = 35;
export const STYLE_CONTROLCHAR = 36;
export const STYLE_INDENTGUIDE = 37;
export const STYLE_CALLTIP = 38;
export const STYLE_FOLDDISPLAYTEXT = 39;

export const STY_Keyword = 40;
export const STY_Type = 41;
export const STY_Integer = 42;
export const STY_Float = 43;
export const STY_Bool = 44;
export const STY_Ident = 45;
export const STY_Operator = 46;
export const STY_String = 47;
export const STY_BuiltinFunc = 48;
export const STY_Punctuation = 49;
export const STY_Comment = 50;
export const STY_ErrorAnnotation = 51;

export const AnnotationHidden = 0;
export const AnnotationStandard = 1;
export const AnnotationBoxed = 2;
export const AnnotationIndented = 3;

export const WS_INVISIBLE = 0;
export const WS_VISIBLEALWAYS = 1;
export const WS_VISIBLEAFTERINDENT = 2;
export const WS_VISIBLEONLYININDENT = 3;

export const ELEMENT_LIST = 0;
export const ELEMENT_LIST_BACK = 1;
export const ELEMENT_LIST_SELECTED = 2;
export const ELEMENT_LIST_SELECTED_BACK = 3;
export const ELEMENT_SELECTION_TEXT = 10;
export const ELEMENT_SELECTION_BACK = 11;
export const ELEMENT_SELECTION_ADDITIONAL_TEXT = 12;
export const ELEMENT_SELECTION_ADDITIONAL_BACK = 13;
export const ELEMENT_SELECTION_SECONDARY_TEXT = 14;
export const ELEMENT_SELECTION_SECONDARY_BACK = 15;
export const ELEMENT_SELECTION_INACTIVE_TEXT = 16;
export const ELEMENT_SELECTION_INACTIVE_BACK = 17;
export const ELEMENT_CARET = 40;
export const ELEMENT_CARET_ADDITIONAL = 41;
export const ELEMENT_CARET_LINE_BACK = 50;
export const ELEMENT_WHITE_SPACE = 60;
export const ELEMENT_WHITE_SPACE_BACK = 61;
export const ELEMENT_HOT_SPOT_ACTIVE = 70;
export const ELEMENT_HOT_SPOT_ACTIVE_BACK = 71;
export const ELEMENT_FOLD_LINE = 80;
export const ELEMENT_HIDDEN_LINE = 81;