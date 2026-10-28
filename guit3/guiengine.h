/*
 *   guiengine.h - guit3 on Captain's Game Engine
 *
 *   The engine owns the window, the graphics context, Dear ImGui (with its
 *   platform and renderer backends) and the main loop.  guit3 is the
 *   engine's application (guiengine.cpp): all of guit3 - its startup, the
 *   TADS VM and its shutdown - runs inside the application's first update(),
 *   which returns only when guit3 is done.  Whenever guit3 waits for the
 *   user (CHtmlSys_mainwin::event_loop(), tadswin_message_box(),
 *   CTadsFileDialog::open_blocking()), it pumps frames with
 *   gui_pump_frame(): the engine processes the window's events and renders
 *   one frame, and the application's render() calls the function the loop
 *   passed in, which builds the frame's ImGui windows between the engine's
 *   ImGui::NewFrame() and ImGui::Render().  See migration.md 5.26.
 */

#ifndef GUIENGINE_H
#define GUIENGINE_H

#include <functional>

namespace CGE {
class AppWindow;
namespace Gfx {
class RenderDevice;
class Sampler;
class Texture;
}
}

/*
 *   Run guit3 on the engine: starts the engine with guit3 as its
 *   application, which calls guimain(argc, argv) from its first update.
 *   Returns guimain's result (0 on the web, where this returns at once and
 *   the browser runs the engine's frames).
 */
int gui_run_application(int argc, char **argv,
                        int (*guimain)(int, char **));

/*
 *   Run one frame: the engine processes the window's events, then renders
 *   the frame, with 'build' building its ImGui windows.  Returns false once
 *   the application is to quit (the window was closed).  Must not be called
 *   while a frame is being built (from inside 'build').
 */
bool gui_pump_frame(const std::function<void()> &build);

/* has the application been asked to quit (the window was closed)? */
bool gui_quit_requested();

/*
 *   The engine's window for guit3's main window: the first top-level
 *   window asking for it gets it, others get null (they are drawn as ImGui
 *   windows inside it).  gui_release_window() gives it back.
 */
CGE::AppWindow *gui_claim_window();
void gui_release_window();

/* the window's content scale (1 at 96 DPI), 1 before the engine runs */
float gui_content_scale();

/* the render device */
CGE::Gfx::RenderDevice *gui_render_device();

/*
 *   Fill a texture with top-down RGBA pixels with straight alpha, for
 *   ImGui::Image() (ImGui_ImplCGE_TextureID()).  'tex' is updated in place
 *   if it has the same size; otherwise it is deleted (if not null) and a
 *   new texture is created.  Returns the texture, null on failure.
 *   gui_delete_texture() deletes it (null is fine).
 */
CGE::Gfx::Texture *gui_upload_texture(CGE::Gfx::Texture *tex,
                                      int width, int height,
                                      const unsigned char *rgba);
void gui_delete_texture(CGE::Gfx::Texture *tex);

/*
 *   Samplers for ImGui images that need other sampling than the engine's
 *   default (linear, clamped), with ImGui_ImplCGE_SetSampler()
 */
enum gui_sampler_t
{
    GUI_SAMPLER_REPEAT,     /* linear, repeating (tiled pictures) */
    GUI_SAMPLER_NEAREST     /* nearest, clamped (the toolbar's icons) */
};
CGE::Gfx::Sampler *gui_sampler(gui_sampler_t which);

#endif /* GUIENGINE_H */
