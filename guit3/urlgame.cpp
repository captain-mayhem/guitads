/*
 *   urlgame.cpp - File > Open Game from Web... (urlgame.h)
 */

#include "tadsplat.h"

#include <stdio.h>
#include <string.h>

#include <string>

#include <imgui.h>
#include <platform/services.h>
#include <platform/userdirs.h>
#include <system/file.h>

#include "tadshtml.h"
#include "htmlgui.h"
#include "tadswin.h"
#include "tadsfont.h"
#include "websaves.h"
#include "urlgame.h"

/* where we are: nothing, asking for the URL, or downloading */
enum urlgame_state_t { URLGAME_IDLE, URLGAME_ASK, URLGAME_DOWNLOAD };
static urlgame_state_t S_state = URLGAME_IDLE;

/* the popup was opened for the current state (ImGui::OpenPopup() once) */
static bool S_popup_opened = false;

/* the address field; it keeps the last address for the next time */
static char S_url[2048] = "https://";

/* the address being downloaded */
static std::string S_download_url;

static const char S_title[] = "Open Game from Web";

bool urlgame_available()
{
    return CGE::Platform::canDownloadFiles();
}

const char *urlgame_get_dir()
{
    static std::string dir;
    if (dir.empty())
    {
        dir = CGE::UserDirs::getDataDir() + "/games";
        CGE::Filesystem::createDirs(dir);
    }
    return dir.c_str();
}

void urlgame_open(CHtmlSys_mainwin *)
{
    /* a download still running (its box was cancelled) blocks a new one */
    S_state = URLGAME_ASK;
    S_popup_opened = false;
}

/* a message box after this frame (tadswin_message_box() runs its own loop) */
static void message(CHtmlSys_mainwin *win, const std::string &msg,
                    bool warning)
{
    win->queue_deferred_dialog([win, msg, warning]()
    {
        tadswin_message_box(win->get_app_window(), msg.c_str(), S_title,
                            MB_OK | (warning ? MB_ICONWARNING
                                             : MB_ICONINFORMATION));
    });
}

/* the address the user typed, trimmed; "https://" added without a scheme */
static std::string typed_url()
{
    std::string url = S_url;
    size_t start = url.find_first_not_of(" \t\r\n");
    size_t end = url.find_last_not_of(" \t\r\n");
    url = (start == std::string::npos
           ? std::string() : url.substr(start, end - start + 1));
    if (!url.empty() && url.find("://") == std::string::npos)
        url = "https://" + url;
    return url;
}

/* start the download of the typed address; false to keep asking */
static bool start_download(CHtmlSys_mainwin *win)
{
    std::string url = typed_url();
    if (url.empty() || url == "https://")
        return false;

    snprintf(S_url, sizeof(S_url), "%s", url.c_str());
    if (!CGE::Platform::downloadFile(url, urlgame_get_dir(), true))
    {
        bool web_url = (strncmp(url.c_str(), "http://", 7) == 0
                        || strncmp(url.c_str(), "https://", 8) == 0);
        message(win, web_url
                ? "The download couldn't start. Another download or file "
                  "transfer may still be running; try again in a moment."
                : "Only http:// and https:// addresses can be opened.",
                true);
        return true;
    }

#ifdef __EMSCRIPTEN__
    /* a file picker left open must not take this download's outcome */
    websaves_forget_transfer();
#endif
    S_download_url = url;
    S_state = URLGAME_DOWNLOAD;
    S_popup_opened = false;
    return true;
}

/* does the file start with a TADS 3 or TADS 2 game file's signature */
static bool is_game_file(const std::string &path)
{
    static const char t3sig[] = "T3-image\015\012\032";
    static const char t2sig[] = "TADS2 bin\012\015\032";
    char buf[16] = "";
    FILE *fp = fopen(path.c_str(), "rb");
    if (fp == 0)
        return false;
    size_t n = fread(buf, 1, sizeof(buf), fp);
    fclose(fp);
    return (n >= sizeof(t3sig) - 1
            && memcmp(buf, t3sig, sizeof(t3sig) - 1) == 0)
        || (n >= sizeof(t2sig) - 1
            && memcmp(buf, t2sig, sizeof(t2sig) - 1) == 0);
}

/* the download ended: load the game, or tell what went wrong */
static void download_ended(CHtmlSys_mainwin *win,
                           CGE::Platform::TransferStatus status,
                           const std::string &name)
{
    if (status == CGE::Platform::TRANSFER_DONE)
    {
        std::string path = std::string(urlgame_get_dir()) + "/" + name;
        if (is_game_file(path))
        {
            win->load_new_game(path.c_str());
            return;
        }

        /* most likely a web page (an IFDB entry rather than its file) */
        remove(path.c_str());
#ifdef __EMSCRIPTEN__
        CGE::UserDirs::commit();
#endif
        message(win, "The file at " + S_download_url + " isn't a TADS game "
                "(.t3 or .gam). Enter the address of the game file itself, "
                "not of a web page about it.", true);
        return;
    }

    std::string msg = "The game couldn't be downloaded from "
        + S_download_url + ".";
#ifdef __EMSCRIPTEN__
    msg += " The web site may not allow downloads from other web pages "
           "(CORS), or the address is wrong.";
#else
    msg += " Check the address and the network connection.";
#endif
    message(win, msg, true);
}

/* the dialog's position and width: centered, like the confirmations */
static void place_popup(float width)
{
    const float s = CTadsFont::get_dpi_scale();
    ImGuiViewport *vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(
        ImVec2(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f),
        ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(width * s, 0), ImGuiCond_Appearing);
}

static void render_ask(CHtmlSys_mainwin *win)
{
    static const char id[] = "Open Game from Web###UrlGameAsk";
    const float s = CTadsFont::get_dpi_scale();
    bool focus = false;
    if (!S_popup_opened)
    {
        ImGui::OpenPopup(id);
        S_popup_opened = true;
        focus = true;
    }

    place_popup(520);
    if (!ImGui::BeginPopupModal(id, nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize))
        return;

    ImGui::PushTextWrapPos(500 * s);
    ImGui::TextUnformatted("Enter the address (URL) of a TADS game file "
                           "(.t3 or .gam) to download and play:");
    ImGui::PopTextWrapPos();
    ImGui::Spacing();

    if (focus)
        ImGui::SetKeyboardFocusHere();
    ImGui::SetNextItemWidth(500 * s);
    bool enter = ImGui::InputText("##UrlGameUrl", S_url, sizeof(S_url),
        ImGuiInputTextFlags_EnterReturnsTrue
        | ImGuiInputTextFlags_AutoSelectAll);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    const float btn_w = 90.0f * s;
    float total_w = btn_w * 2 + ImGui::GetStyle().ItemSpacing.x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX()
        + (ImGui::GetContentRegionAvail().x - total_w) * 0.5f);

    std::string url = typed_url();
    bool usable = !url.empty() && url != "https://";
    ImGui::BeginDisabled(!usable);
    bool open = ImGui::Button("Open", ImVec2(btn_w, 0));
    ImGui::EndDisabled();
    ImGui::SameLine();
    bool cancel = ImGui::Button("Cancel", ImVec2(btn_w, 0))
        || ImGui::IsKeyPressed(ImGuiKey_Escape);

    if (cancel)
    {
        S_state = URLGAME_IDLE;
        ImGui::CloseCurrentPopup();
    }
    else if ((open || enter) && usable && start_download(win))
    {
        /* the download's box replaces this one */
        if (S_state != URLGAME_DOWNLOAD)
            S_state = URLGAME_IDLE;
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

static void render_download(CHtmlSys_mainwin *win)
{
    /* the outcome first: a finished download needs no box */
    std::string name;
    CGE::Platform::TransferStatus status = CGE::Platform::pollTransfer(&name);
    if (status != CGE::Platform::TRANSFER_RUNNING)
    {
        S_state = URLGAME_IDLE;
        if (S_popup_opened && ImGui::BeginPopupModal(
                "Open Game from Web###UrlGameDownload"))
        {
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        download_ended(win, status, name);
        return;
    }

    static const char id[] = "Open Game from Web###UrlGameDownload";
    const float s = CTadsFont::get_dpi_scale();
    if (!S_popup_opened)
    {
        ImGui::OpenPopup(id);
        S_popup_opened = true;
    }

    place_popup(420);
    if (!ImGui::BeginPopupModal(id, nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize))
        return;

    ImGui::PushTextWrapPos(400 * s);
    ImGui::TextUnformatted("Downloading the game from");
    ImGui::TextUnformatted(S_download_url.c_str());
    ImGui::PopTextWrapPos();
    ImGui::Spacing();

    /* an indeterminate progress bar: the size isn't known */
    float t = (float)ImGui::GetTime();
    ImGui::ProgressBar(-1.0f * t, ImVec2(400 * s, 0), "");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    /*
     *   Cancel only stops waiting: the engine can't stop a download, which
     *   ends on its own and is then ignored (the next transfer resets it)
     */
    const float btn_w = 90.0f * s;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX()
        + (ImGui::GetContentRegionAvail().x - btn_w) * 0.5f);
    if (ImGui::Button("Cancel", ImVec2(btn_w, 0))
        || ImGui::IsKeyPressed(ImGuiKey_Escape))
    {
        S_state = URLGAME_IDLE;
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void urlgame_render(CHtmlSys_mainwin *win)
{
    switch (S_state)
    {
    case URLGAME_ASK:
        render_ask(win);
        break;

    case URLGAME_DOWNLOAD:
        render_download(win);
        break;

    default:
        S_popup_opened = false;
        break;
    }
}
