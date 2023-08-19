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

export default class Parameter extends Element {
    this(props) {
        this.paramValue = props.paramValue;
        this.paramName = props.paramName;
        this.defaultValue = props.defaultValue;
        this.displayName = props.displayName;
        this.minimumVal = props.minimumVal;
        this.maximumVal = props.maximumVal;
        this.middleValue = props.middleValue;
        this.stepSize = props.stepSize;
        this.index = props.index;
    }

    updateValue(v) {
        const old = this.paramValue;
        this.componentUpdate({ paramValue: v });
        this.dispatchEvent(new CustomEvent('valueChange', {
            bubbles: true,
            detail: {
                old: old,
                new: v
            }
        }));
    }

    render() {
        return <p>
            <input
                class="valueInput"
                type="hslider"
                min={this.minimumVal * 100}
                max={this.maximumVal * 100}
                value={this.paramValue * 100}
                step={this.stepSize * 100} />
            {this.displayName} ({this.paramValue.toFixed(2)})
        </p>;
    }

    ["on change at .valueInput"](event) {
        this.updateValue(event.target.value / 100);
    }

    ["on ^dblclick at .valueInput"]() {
        this.updateValue(this.defaultValue);
    }
}