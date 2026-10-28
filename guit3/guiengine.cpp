/*
 *   guiengine.cpp - guit3 on Captain's Game Engine.  See guiengine.h.
 */

#include <cstdio>
#include <memory>

#include <imgui.h>

#include <system/engine.h>
#include <system/application.h>
#include <input/keyboard.h>
#include <renderer/renderdevice.h>
#include <ui/imgui_impl_cge_platform.h>
#include <window/window.h>

#include "guiengine.h"

namespace {

/* the function building the current frame's ImGui windows */
const std::function<void()> *frame_builder = nullptr;

/* is a frame being built? */
bool building = false;

/* has a top-level window claimed the engine's window? */
bool window_claimed = false;

/* the samplers for gui_sampler() */
std::unique_ptr<CGE::Gfx::Sampler> samplers[2];

/* the background behind guit3's windows */
const CGE::Gfx::ClearColor clear_color = {0.45f, 0.55f, 0.60f, 1.00f};

class CGuiT3App : public CGE::IApplication
{
public:
    CGuiT3App(int (*guimain)(int, char **)) : guimain_(guimain) { }

    int get_result() const { return result_; }

    void init(int argc, char **argv) override
    {
        argc_ = argc;
        argv_ = argv;

        /* '~' is the game's to type, not the engine console's */
        Input::Keyboard::instance()->enableBuitInConsole(false);

        /*
         *   CTadsFont sizes every font in screen pixels from its point size
         *   and the content scale already, so the engine only scales the
         *   style, not the fonts again (migration.md 3.5a)
         */
        ImGui_ImplCGE_Platform_SetScaleFonts(false);
    }

    void update(double) override
    {
        /*
         *   All of guit3 runs in the first update: it pumps the frames it
         *   waits in itself (gui_pump_frame), and the engine quits when it
         *   returns
         */
        if (started_)
            return;
        started_ = true;
        result_ = guimain_(argc_, argv_);
        CGE::Engine::instance()->requestShutdown();
    }

    void render() override
    {
        CGE::Gfx::RenderDevice *device =
            CGE::Engine::instance()->getRenderDevice();
        CGE::Gfx::PassDesc pass;
        pass.clearColor = clear_color;
        if (device->beginPass(pass))
            device->endPass();

        if (frame_builder != nullptr && *frame_builder)
        {
            building = true;
            (*frame_builder)();
            building = false;
        }
    }

    void shutdown() override
    {
        for (auto &s : samplers)
            s.reset();
    }

    /* Escape is the game's, not a request to quit (the default) */
    void onKeyDown(int) override { }

private:
    int (*guimain_)(int, char **);
    int argc_ = 0;
    char **argv_ = nullptr;
    bool started_ = false;
    int result_ = 0;
};

} // namespace

int gui_run_application(int argc, char **argv, int (*guimain)(int, char **))
{
    /*
     *   static: on the web, runApplication returns while the browser goes
     *   on running the engine's frames
     */
    static CGuiT3App *app = nullptr;
    app = new CGuiT3App(guimain);
    CGE::runApplication(argc, argv, *app);
    return app->get_result();
}

bool gui_pump_frame(const std::function<void()> &build)
{
    if (building)
    {
        /*
         *   a nested wait from inside a frame can't render frames of its
         *   own; give up instead of waiting for input that never comes
         */
        fprintf(stderr, "guit3: gui_pump_frame() called while building a frame\n");
        return false;
    }

    const std::function<void()> *prev = frame_builder;
    frame_builder = &build;
    bool ok = CGE::Engine::instance()->pumpFrame();
    frame_builder = prev;
    return ok;
}

bool gui_quit_requested()
{
    return CGE::Engine::instance()->isShutdownRequested();
}

CGE::AppWindow *gui_claim_window()
{
    if (window_claimed)
        return nullptr;
    window_claimed = true;
    return CGE::Engine::instance()->getWindow();
}

void gui_release_window()
{
    window_claimed = false;
}

float gui_content_scale()
{
    CGE::Engine *engine = CGE::Engine::instance();
    float s = (engine != nullptr && engine->getWindow() != nullptr
               ? engine->getWindow()->getContentScale() : 1.0f);
    return (s > 0.0f ? s : 1.0f);
}

CGE::Gfx::RenderDevice *gui_render_device()
{
    return CGE::Engine::instance()->getRenderDevice();
}

CGE::Gfx::Texture *gui_upload_texture(CGE::Gfx::Texture *tex,
                                      int width, int height,
                                      const unsigned char *rgba)
{
    using namespace CGE::Gfx;
    if (tex != nullptr && tex->getDesc().width == width
        && tex->getDesc().height == height)
    {
        TextureRegion region;
        region.width = width;
        region.height = height;
        gui_render_device()->updateTexture(tex, region, rgba);
        return tex;
    }

    delete tex;
    TextureDesc desc;
    desc.width = width;
    desc.height = height;
    return gui_render_device()->createTexture(desc, rgba).release();
}

void gui_delete_texture(CGE::Gfx::Texture *tex)
{
    delete tex;
}

CGE::Gfx::Sampler *gui_sampler(gui_sampler_t which)
{
    using namespace CGE::Gfx;
    std::unique_ptr<Sampler> &s = samplers[which];
    if (!s)
    {
        SamplerDesc desc;
        if (which == GUI_SAMPLER_NEAREST)
        {
            /*
             *   the toolbar's icons are packed edge to edge, and linear
             *   filtering would bleed each icon's edge into its neighbor
             */
            desc.minFilter = desc.magFilter = Filter::Nearest;
            desc.addressU = desc.addressV = AddressMode::ClampToEdge;
        }
        s = gui_render_device()->createSampler(desc);
    }
    return s.get();
}
