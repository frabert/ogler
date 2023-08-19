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

export const aboutModal =
    <info caption="About">
        <h1>ogler - Use GLSL shaders in REAPER</h1>
        <h2>Copyright (C) 2023 Francesco Bertolaccini</h2>
        <p>
            This program is free software: you can redistribute it and/or modify
            it under the terms of the GNU General Public License as published by
            the Free Software Foundation, either version 3 of the License, or
            (at your option) any later version.
        </p>
        <p>
            This program is distributed in the hope that it will be useful,
            but WITHOUT ANY WARRANTY; without even the implied warranty of
            MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
            GNU General Public License for more details.
        </p>
        <p>
            You should have received a copy of the GNU General Public License
            along with this program.  If not, see
            &lt;<a href="https://www.gnu.org/licenses/">https://www.gnu.org/licenses/</a>&gt;.
        </p>
        <p>
            Additional permission under GNU GPL version 3 section 7
        </p>
        <p>
            If you modify this Program, or any covered work, by linking or
            combining it with Sciter (or a modified version of that library),
            containing parts covered by the terms of Sciter's EULA, the licensors
            of this Program grant you additional permission to convey the
            resulting work.
        </p>
        <p>
            This Application uses Sciter Engine (<a href="http://sciter.com/">http://sciter.com/</a>),
            copyright Terra Informatica Software, Inc.
        </p>

        <h3>CLAP SDK</h3>
        <p>MIT License</p>
        <p>Copyright (c) 2021 Alexandre BIQUE</p>
        <p>
            Permission is hereby granted, free of charge, to any person obtaining a copy
            of this software and associated documentation files (the "Software"), to deal
            in the Software without restriction, including without limitation the rights
            to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
            copies of the Software, and to permit persons to whom the Software is
            furnished to do so, subject to the following conditions:
        </p>
        <p>
            The above copyright notice and this permission notice shall be included in all
            copies or substantial portions of the Software.
        </p>
        <p>
            THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
            IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
            FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
            AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
            LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
            OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
            SOFTWARE.
        </p>

        <h3>Glslang</h3>
        <p>
            Glslang uses a bunch of licenses, see
            <a href="https://github.com/KhronosGroup/glslang/blob/main/LICENSE.txt">LICENSE.txt</a>
        </p>

        <h3>Scintilla</h3>
        <p>License for Lexilla, Scintilla, and SciTE</p>
        <p>
            Copyright 1998-2021 by Neil Hodgson
            &lt;<a href="mailto:neilh@scintilla.org">neilh@scintilla.org</a>&gt;
        </p>
        <p>All Rights Reserved</p>
        <p>
            Permission to use, copy, modify, and distribute this software and its
            documentation for any purpose and without fee is hereby granted,
            provided that the above copyright notice appear in all copies and that
            both that copyright notice and this permission notice appear in
            supporting documentation.
        </p>
        <p>
            NEIL HODGSON DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
            SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
            AND FITNESS, IN NO EVENT SHALL NEIL HODGSON BE LIABLE FOR ANY
            SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
            WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
            WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
            TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
            OR PERFORMANCE OF THIS SOFTWARE.
        </p>
    </info>;


export const compileErrorModal = err =>
    <error caption="Shader compilation error">
        <pre>{err}</pre>
    </error>;