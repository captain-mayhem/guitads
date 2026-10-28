/*
 *   tadslicensedlg.cpp - ImGui-native "License Information" dialog (guit3)
 *
 *   See tadslicensedlg.h for the overall design.  The old Win32 LicenseDlg
 *   loaded the IDX_LICENSE_TEXT "TEXTFILE" resource (../notes3/license.txt)
 *   into its EDITTEXT control; the text now comes from the repository's
 *   LICENSE file, embedded at build time (guires_data.h, embed_file.cmake)
 *   and fetched through os_load_license_text() (migration.md 5.4/B).
 */

#include <string>

#include <imgui.h>

#include "tadshtml.h"     /* th_free() */
#include "tadslicensedlg.h"
#include "tadsfont.h"     /* CTadsFont::get_dpi_scale() - see migration.md 3.5a */
#include "guios.h"

namespace
{
    struct LicenseDlgState
    {
        /* open() was called; waiting for the next render() to show it */
        bool pending = false;

        /* the dialog is currently showing (OpenPopup has been issued) */
        bool is_open = false;

        /* license text, loaded lazily on first open() and cached thereafter */
        std::string text;
        bool text_loaded = false;
    };

    LicenseDlgState s_dlg;

    /* load the license text from its resource, if we haven't already */
    void load_license_text()
    {
        s_dlg.text_loaded = true;

        /*
         *   Build the string from the exact byte count the hook reports
         *   rather than relying on the bytes being null-terminated (the old
         *   Win32 code handed the resource pointer straight to EM_REPLACESEL
         *   as if it were a C string, which happened to work but wasn't
         *   guaranteed by the resource format).
         */
        size_t len = 0;
        char *bytes = os_load_license_text(&len);
        if (bytes == 0)
            return;

        s_dlg.text.assign(bytes, len);
        th_free(bytes);
    }

    /*
     *   Draw the dialog's popup and contents for the current frame.
     */
    void draw_frame()
    {
        const char *popup_id = "License Information###TadsLicenseDlg";

        bool just_opened = s_dlg.pending;
        if (s_dlg.pending)
        {
            ImGui::OpenPopup(popup_id);
            s_dlg.pending = false;
            s_dlg.is_open = true;
        }

        /* scale the fixed pixel size for the display - see migration.md 3.5a */
        const float s = CTadsFont::get_dpi_scale();

        ImGuiViewport *vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(
            ImVec2(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f),
            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        /*
         *   The old DLG_LICENSE resource's height (300), but wide enough for
         *   the text's longest line - the multiline text field only scrolls
         *   vertically.  Never narrower than the old 400, never wider than
         *   the main window.
         */
        if (just_opened)
        {
            const ImGuiStyle &style = ImGui::GetStyle();
            float wid = ImGui::CalcTextSize(s_dlg.text.c_str()).x
                        + 2.0f * style.FramePadding.x
                        + 2.0f * style.WindowPadding.x
                        + style.ScrollbarSize;
            if (wid < 400 * s)
                wid = 400 * s;
            if (wid > vp->Size.x)
                wid = vp->Size.x;
            ImGui::SetNextWindowSize(ImVec2(wid, 300 * s), ImGuiCond_Appearing);
        }

        bool p_open = true;
        if (!ImGui::BeginPopupModal(popup_id, &p_open, ImGuiWindowFlags_NoResize))
            return;

        if (!p_open)
        {
            /* the window's own [x] close button was clicked */
            ImGui::EndPopup();
            s_dlg.is_open = false;
            return;
        }

        /* the read-only license text, filling everything above the OK button */
        float ok_row_ht = ImGui::GetFrameHeightWithSpacing();
        ImGui::InputTextMultiline("##LicenseText",
            const_cast<char *>(s_dlg.text.c_str()), s_dlg.text.size() + 1,
            ImVec2(-1.0f, -ok_row_ht), ImGuiInputTextFlags_ReadOnly);

        /* centered OK button, matching the old dialog's DEFPUSHBUTTON */
        float ok_wid = 50.0f * s;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ok_wid) * 0.5f);
        if (ImGui::Button("OK", ImVec2(ok_wid, 0))
            || ImGui::IsKeyPressed(ImGuiKey_Escape)
            || ImGui::IsKeyPressed(ImGuiKey_Enter))
        {
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            s_dlg.is_open = false;
            return;
        }

        ImGui::EndPopup();
    }
}

/* ------------------------------------------------------------------------ */

void CTadsLicenseDlg::open()
{
    if (!s_dlg.text_loaded)
        load_license_text();

    s_dlg.pending = true;
}

void CTadsLicenseDlg::render()
{
    if (!s_dlg.pending && !s_dlg.is_open)
        return;

    draw_frame();
}
