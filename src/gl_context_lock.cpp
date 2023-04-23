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

#include "gl_context_lock.hpp"

#include <GLFW/glfw3.h>

namespace ogler {
GlContextLock::GlContextLock(GLFWwindow &w) : wnd(&w) {}
void GlContextLock::lock() {
  while (!try_lock()) {
    // Try locking until it works. It's a spinlock, it's ugly, but should work.
  }
}
void GlContextLock::unlock() { glfwMakeContextCurrent(nullptr); }
bool GlContextLock::try_lock() {
  glfwMakeContextCurrent(wnd);
  return glfwGetCurrentContext() == wnd;
}
} // namespace ogler