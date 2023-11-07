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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "ogler_editor.hpp"
#include "sciter_scintilla.hpp"

static HINSTANCE hinst;

static ogler::WindowHandle<ogler::Editor> editor;

class MockEditorInterface final : public ogler::EditorInterface {
  std::string source;
  int zoom{1};
  int w;
  int h;
  std::vector<ogler::Parameter> params;

public:
  void recompile_shaders() final {}

  void set_shader_source(const std::string &source) final {
    this->source = source;
  }

  const std::string &get_shader_source() final { return source; }

  int get_zoom() final { return zoom; }

  void set_zoom(int zoom) final { this->zoom = zoom; }

  int get_width() final { return w; }

  int get_height() final { return h; }

  void set_width(int w) final { this->w = w; }

  void set_height(int h) final { this->h = h; }

  void set_parameter(size_t idx, float value) final {}
};

static MockEditorInterface editor_interface;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
  case WM_CREATE: {
    editor = ogler::Editor::create(hwnd, hinst, 100, 100, "ogler",
                                   std::make_unique<MockEditorInterface>());
  } break;
  case WM_SIZE: {
    UINT width = LOWORD(lParam);
    UINT height = HIWORD(lParam);
    SetWindowPos(editor, nullptr, 0, 0, width, height, SWP_NOMOVE);
  } break;
  case WM_DESTROY:
    PostQuitMessage(0);
    break;
  }

  return DefWindowProc(hwnd, msg, wParam, lParam);
}

extern "C" {
int Scintilla_RegisterClasses(void *hInstance);
int Scintilla_ReleaseResources();
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {
  hinst = hInstance;
  if (!Scintilla_RegisterClasses(hInstance)) {
    return false;
  }

  MSG msg;
  WNDCLASS wc{
      .style = CS_HREDRAW | CS_VREDRAW,
      .lpfnWndProc = WndProc,
      .hInstance = hInstance,
      .lpszClassName = "Window",
  };

  RegisterClass(&wc);

  ogler::ScintillaEditorFactory factory(hInstance);
  HWND hwnd =
      CreateWindow(wc.lpszClassName, "Window", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                   100, 100, 350, 250, NULL, NULL, hInstance, NULL);

  ShowWindow(hwnd, nCmdShow);
  UpdateWindow(hwnd);

  while (GetMessage(&msg, NULL, 0, 0)) {

    DispatchMessage(&msg);
  }

  return (int)msg.wParam;
}