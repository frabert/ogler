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

import Parameter from './parameter.js'

export default class ParamList extends Element {
    this(props) {
        this.parameters = props.parameters;
    }

    render() {
        return <section id="params">
            {
                this.parameters.map((param, index) =>
                    <Parameter
                        class="param"
                        paramValue={param.value}
                        paramName={param.name}
                        defaultValue={param.defaultValue}
                        displayName={param.displayName}
                        minimumVal={param.minimumVal}
                        maximumVal={param.maximumVal}
                        middleValue={param.middleValue}
                        stepSize={param.stepSize}
                        index={index} />)
            }
        </section>;
    }

    ["on valueChange at .param"](event) {
        const name = event.target.paramName;
        this.dispatchEvent(new CustomEvent('valueChange', {
            bubbles: true,
            detail: {
                name: name,
                index: event.target.index,
                old: event.detail.old,
                new: event.detail.new
            }
        }));
    }
}