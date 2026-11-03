/*
 *   guiengine.cpp - guit3 on Captain's Game Engine.  See guiengine.h.
 */

#include <cstdio>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include <imgui.h>

#include <system/engine.h>
#include <system/application.h>
#include <input/keyboard.h>
#include <platform/services.h>
#include <renderer/renderdevice.h>
#include <ui/imgui_impl_cge_platform.h>
#include <window/window.h>

#ifdef ANDROID
#include <jni.h>
#include <android_native_app_glue.h>
#endif

#include "guiengine.h"

#if defined(_WIN32) && defined(T3_DEBUG)
#include <cstdlib>
#include <new>

/*
 *   The plain global new and delete, in place of the T3 debug heap's
 *   (tads3/std_dbg.cpp, which the linker leaves out of t3htm once these
 *   exist).  On Windows a replacement only applies to the module that
 *   defines it, and the engine's DLLs hand guit3 memory they allocated
 *   (a std::string's buffer from CGE::Platform::getClipboardText(), for
 *   one) for guit3 to free: t3free() on such a block reads a header that
 *   isn't there and crashes.  The T3 debug heap still serves t3malloc().
 */
void *operator new(size_t siz)
{
    void *p = malloc(siz != 0 ? siz : 1);
    if (p == 0)
        throw std::bad_alloc();
    return p;
}

void *operator new[](size_t siz)
{
    return operator new(siz);
}

void operator delete(void *ptr) noexcept
{
    free(ptr);
}

void operator delete[](void *ptr) noexcept
{
    free(ptr);
}
#endif

namespace {

/* the function building the current frame's ImGui windows */
const std::function<void()> *frame_builder = nullptr;

/* is a frame being built? */
bool building = false;

/* has a top-level window claimed the engine's window? */
bool window_claimed = false;

/* the samplers for gui_sampler() */
std::unique_ptr<CGE::Gfx::Sampler> samplers[2];

/*
 *   the texture pointers gui_track_texture() registered; images may be
 *   created off the main thread, hence the mutex
 */
std::set<CGE::Gfx::Texture **> tracked_textures;
std::mutex tracked_textures_mutex;

/* is the on-screen keyboard shown for the game (gui_update_text_input)? */
bool game_text_input = false;

/* the background behind guit3's windows */
const CGE::Gfx::ClearColor clear_color = {0.45f, 0.55f, 0.60f, 1.00f};

class CGuiT3App : public CGE::IApplication
{
public:
    CGuiT3App(int (*guimain)(int, char **)) : guimain_(guimain) { }

    int get_result() const { return result_; }

    void init(int argc, char **argv) override
    {
        /*
         *   copied, since guimain runs later (from update): on Android the
         *   engine's argv only lasts for this call
         */
        args_.assign(argv, argv + argc);
        argv_.clear();
        for (std::string &arg : args_)
            argv_.push_back(&arg[0]);
        argv_.push_back(nullptr);

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
        result_ = guimain_((int)args_.size(), argv_.data());
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

    /*
     *   The GL context is gone (Android): delete the textures and samplers
     *   while the old device still exists; their owners upload them again
     *   when they find them null, and ImGui recreates its font textures
     */
    void onDeviceLost() override
    {
        for (auto &s : samplers)
            s.reset();
        std::lock_guard<std::mutex> lk(tracked_textures_mutex);
        for (CGE::Gfx::Texture **slot : tracked_textures)
        {
            delete *slot;
            *slot = nullptr;
        }
    }

    /* Escape is the game's, not a request to quit (the default) */
    void onKeyDown(int) override { }

private:
    int (*guimain_)(int, char **);
    std::vector<std::string> args_;
    std::vector<char *> argv_;
    bool started_ = false;
    int result_ = 0;
};

} // namespace

int gui_run_application(int argc, char **argv, int (*guimain)(int, char **),
                        void *platform_data)
{
    /*
     *   static: on the web, runApplication returns while the browser goes
     *   on running the engine's frames
     */
    static std::unique_ptr<CGuiT3App> app;
    app.reset(new CGuiT3App(guimain));
    CGE::runApplication(argc, argv, *app, platform_data);
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

void gui_request_quit()
{
    CGE::Engine::instance()->requestShutdown();
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

void gui_track_texture(CGE::Gfx::Texture **slot)
{
    std::lock_guard<std::mutex> lk(tracked_textures_mutex);
    tracked_textures.insert(slot);
}

void gui_untrack_texture(CGE::Gfx::Texture **slot)
{
    std::lock_guard<std::mutex> lk(tracked_textures_mutex);
    tracked_textures.erase(slot);
}

void gui_update_text_input(bool want)
{
#ifdef ANDROID
    ImGuiIO &io = ImGui::GetIO();
    if (want != game_text_input)
    {
        game_text_input = want;
        if (want)
            CGE::Platform::startTextInput();
        else if (!io.WantTextInput)
            CGE::Platform::stopTextInput();
        return;
    }
    if (!want)
        return;

    /*
     *   A dialog's text field hides the keyboard when it closes (ImGui's
     *   backend), though the game still reads; and a tap outside of the
     *   controls brings back a keyboard the user closed - a tap, not a
     *   drag, which scrolls (CHtmlSys_mainwin::event_loop())
     */
    if (!CGE::Platform::isTextInputActive()
        || (io.MouseReleased[ImGuiMouseButton_Left]
            && io.MouseDragMaxDistanceSqr[ImGuiMouseButton_Left]
               < io.MouseDragThreshold * io.MouseDragThreshold
            && !ImGui::IsAnyItemHovered()))
        CGE::Platform::startTextInput();
#else
    (void)want;
#endif
}

float gui_keyboard_inset()
{
#ifdef ANDROID
    /*
     *   The window keeps its size under the keyboard (CgeActivity), so the
     *   activity (GuiT3Activity, android/) keeps the height the keyboard
     *   covers from its window insets, and we ask it - only while there is
     *   text input, the keyboard is closed otherwise
     */
    if (!CGE::Platform::isTextInputActive())
        return 0.0f;
    CGE::Engine *engine = CGE::Engine::instance();
    android_app *app = (engine != nullptr ? engine->getApp() : nullptr);
    if (app == nullptr || app->activity == nullptr)
        return 0.0f;
    JavaVM *vm = app->activity->vm;
    JNIEnv *env = nullptr;
    bool attached = false;
    if (vm->GetEnv((void **)&env, JNI_VERSION_1_6) == JNI_EDETACHED)
    {
        if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK)
            return 0.0f;
        attached = true;
    }
    int inset = 0;
    jclass cls = env->GetObjectClass(app->activity->clazz);
    jmethodID method = env->GetMethodID(cls, "getKeyboardInset", "()I");
    if (method != nullptr)
        inset = env->CallIntMethod(app->activity->clazz, method);
    else
        env->ExceptionClear();  /* a plain CgeActivity has no such method */
    env->DeleteLocalRef(cls);
    if (attached)
        vm->DetachCurrentThread();
    return (inset > 0 ? (float)inset : 0.0f);
#else
    return 0.0f;
#endif
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
