/*
 *   guimap.cpp - the map screen of Thaumistry: In Charm's Way.  See guimap.h.
 *
 *   The screen, as the game's own front end has it: the city photo
 *   (background.tif) fills the window; on it the map, which the player pans
 *   (drag, arrow keys) and zooms (wheel, +/-, pinch); the area's title on
 *   top, the compass and the legend of the controls at the bottom.  Tab or a
 *   double click switches between the area and the world map, Esc or Back
 *   closes the map.  Buttons in the top right corner do the same for the
 *   mouse and touch screens.
 *
 *   The big maps (3530x3140 for an area, 4000x3000 for the world) are
 *   uploaded with smaller copies (halving down to 256 pixels), and the copy
 *   closest to the size on screen is drawn: there are no mipmaps for ImGui's
 *   images, and drawing a big picture small would flicker.  Pictures are
 *   loaded when first drawn and all freed when the map closes.
 */

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <imgui.h>
#include <zlib.h>

#include <image/image.h>
#include <image/loader.h>
#include <input/touch.h>
#include <ui/imgui_impl_cge_render.h>

#include "os.h"

#include "guiengine.h"
#include "guimap.h"

namespace {

/* ------------------------------------------------------------------------ */
/*
 *   Where the rooms and areas are.  These positions aren't in maps.bin: they
 *   are the front end's (Thaumistry.exe), in the pixels of the full-size
 *   pictures.  A room's position is the center of its cloud
 *   (maps/<area>/<room>.tif) on the area's map.tif; an area's is the center
 *   of its pin (maps/label_<area>.tif) on map_overview.tif.  The rooms
 *   without a cloud picture (rm_pod) aren't listed.
 */
struct Place
{
    const char *name;
    int x;
    int y;
};

const Place rooms[] = {
    { "rm_auditorium", 1390, 1290 },
    { "rm_cloakroom", 1745, 1680 },
    { "rm_convention_center", 1390, 1680 },
    { "rm_alcove", 1460, 1280 },
    { "rm_stock_exchange", 2010, 1300 },
    { "rm_zuccotti_park", 2000, 1618 },
    { "rm_boxcar", 1767, 1250 },
    { "rm_concourse", 1780, 1550 },
    { "rm_engineer", 1370, 1550 },
    { "rm_alcove_airfulla", 1080, 1140 },
    { "rm_alcove_alcatraz", 1085, 1965 },
    { "rm_alcove_bible", 1080, 1556 },
    { "rm_alcove_orbiter", 1484, 1970 },
    { "rm_alcove_penny", 1485, 1165 },
    { "rm_chamber", 1985, 1150 },
    { "rm_training", 1980, 1970 },
    { "rm_mammal_house", 1196, 1466 },
    { "rm_path", 1600, 1460 },
    { "rm_reptile_house", 2615, 1460 },
    { "rm_snake_pit", 2615, 1160 },
    { "rm_target", 786, 1460 },
    { "rm_zoo", 2110, 1446 },
    { "rm_zoo_closet", 786, 1755 },
    { "rm_anagrammer_booth", 650, 805 },
    { "rm_anteroom", 2856, 1205 },
    { "rm_balcony", 2060, 1400 },
    { "rm_displacement_booth", 2460, 805 },
    { "rm_duplicator_booth", 2450, 1615 },
    { "rm_energy", 1560, 800 },
    { "rm_glick", 1560, 400 },
    { "rm_investor_lounge", 3260, 1200 },
    { "rm_lobby", 1555, 2010 },
    { "rm_lyric", 655, 1615 },
    { "rm_manufacturing", 655, 1206 },
    { "rm_massage", 257, 2000 },
    { "rm_museum_entrance", 1563, 2434 },
    { "rm_paranoid", 1055, 805 },
    { "rm_passage", 2060, 1710 },
    { "rm_plaza", 1555, 2840 },
    { "rm_quantum", 280, 1195 },
    { "rm_rotunda", 1560, 1205 },
    { "rm_transportation", 2462, 1205 },
    { "rm_waterstone", 2055, 805 },
};

const Place areas[] = {
    { "map_convention_center", 1445, 1120 },
    { "map_financial_district", 1170, 2330 },
    { "map_lab", 1950, 1970 },
    { "map_pennsylvania_station", 1670, 1270 },
    { "map_tech_museum", 2200, 540 },
    { "map_zoo", 2260, 860 },
};

const Place *find_place(const Place *places, size_t count, const std::string &name)
{
    for (size_t i = 0 ; i < count ; ++i)
    {
        if (name == places[i].name)
            return &places[i];
    }
    return nullptr;
}


/* ------------------------------------------------------------------------ */
/*
 *   maps.bin: a Qt resource file ("qres", as rcc writes it).  A header
 *   (magic, version, then the offsets of the tree, the data and the names,
 *   all big-endian), the files' data (each a 32-bit size and the bytes,
 *   zlib-compressed with a 32-bit size in front if the node says so), the
 *   names (16-bit length, 32-bit hash, UTF-16), and the tree of nodes (14
 *   bytes in version 1, 22 from version 2 on, which adds a timestamp).
 */
class ResourceFile
{
public:
    bool open(const std::string &path)
    {
        close();
        osfildef *fp = osfoprb(path.c_str(), OSFTBIN);
        if (fp == 0)
            return false;
        bool ok = read_index(fp);
        osfcls(fp);
        if (ok)
            path_ = path;
        else
            entries_.clear();
        return ok;
    }

    void close()
    {
        entries_.clear();
        path_.clear();
    }

    bool is_open() const { return !path_.empty(); }

    bool has(const std::string &name) const
        { return entries_.find(name) != entries_.end(); }

    /* the files directly in a folder ("maps/map_zoo"), by their names */
    std::vector<std::string> list(const std::string &folder) const
    {
        std::vector<std::string> names;
        std::string prefix = folder + "/";
        for (const auto &entry : entries_)
        {
            if (entry.first.compare(0, prefix.size(), prefix) == 0
                && entry.first.find('/', prefix.size()) == std::string::npos)
                names.push_back(entry.first.substr(prefix.size()));
        }
        return names;
    }

    bool read(const std::string &name, std::vector<unsigned char> &data) const
    {
        auto it = entries_.find(name);
        if (it == entries_.end())
            return false;
        osfildef *fp = osfoprb(path_.c_str(), OSFTBIN);
        if (fp == 0)
            return false;
        unsigned char size_buf[4];
        bool ok = osfseek(fp, (long)it->second.offset, OSFSK_SET) == 0
                  && osfrb(fp, size_buf, 4) == 0;
        uint32_t size = ok ? be32(size_buf) : 0;
        if (ok && size > 0 && size <= (256u << 20))
        {
            data.resize(size);
            ok = osfrb(fp, data.data(), size) == 0;
        }
        else
            ok = false;
        osfcls(fp);
        if (!ok)
            return false;

        if (it->second.compressed)
        {
            /* qCompress: the uncompressed size, then a zlib stream */
            if (data.size() < 4)
                return false;
            uLongf out_len = be32(data.data());
            if (out_len > (256u << 20))
                return false;
            std::vector<unsigned char> out(out_len);
            if (uncompress(out.data(), &out_len, data.data() + 4,
                           (uLong)(data.size() - 4)) != Z_OK)
                return false;
            out.resize(out_len);
            data.swap(out);
        }
        return true;
    }

private:
    struct Entry
    {
        uint32_t offset;
        bool compressed;
    };

    static uint32_t be32(const unsigned char *p)
    {
        return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
               | ((uint32_t)p[2] << 8) | p[3];
    }
    static uint16_t be16(const unsigned char *p)
        { return (uint16_t)((p[0] << 8) | p[1]); }

    bool read_index(osfildef *fp)
    {
        unsigned char header[20];
        if (osfrb(fp, header, sizeof(header)) != 0
            || memcmp(header, "qres", 4) != 0)
            return false;
        uint32_t version = be32(header + 4);
        tree_ = be32(header + 8);
        data_ = be32(header + 12);
        uint32_t names = be32(header + 16);
        if (version < 1 || version > 3)
            return false;
        node_size_ = version == 1 ? 14 : 22;

        /* the names and the tree, which rcc writes after the data */
        if (osfseek(fp, 0, OSFSK_END) != 0)
            return false;
        long file_size = osfpos(fp);
        index_start_ = (std::min)(tree_, names);
        if (file_size <= (long)index_start_ || tree_ >= (uint32_t)file_size
            || names >= (uint32_t)file_size)
            return false;
        index_.resize((size_t)(file_size - index_start_));
        if (osfseek(fp, (long)index_start_, OSFSK_SET) != 0
            || osfrb(fp, index_.data(), index_.size()) != 0)
            return false;
        names_ = names;

        bool ok = walk(0, "", 0);
        index_.clear();
        index_.shrink_to_fit();
        return ok && !entries_.empty();
    }

    /* the bytes at a file offset in the index, null if they aren't there */
    const unsigned char *at(uint32_t pos, size_t len) const
    {
        if (pos < index_start_ || pos - index_start_ + len > index_.size())
            return nullptr;
        return index_.data() + (pos - index_start_);
    }

    bool name_of(uint32_t offset, std::string &name) const
    {
        const unsigned char *p = at(names_ + offset, 6);
        if (p == nullptr)
            return false;
        uint16_t len = be16(p);
        const unsigned char *chars = at(names_ + offset + 6, (size_t)len * 2);
        if (chars == nullptr)
            return false;
        name.clear();
        for (uint16_t i = 0 ; i < len ; ++i)
        {
            /* UTF-16 to UTF-8, enough for the names of the BMP */
            unsigned c = be16(chars + i * 2);
            if (c < 0x80)
                name += (char)c;
            else if (c < 0x800)
            {
                name += (char)(0xC0 | (c >> 6));
                name += (char)(0x80 | (c & 0x3F));
            }
            else
            {
                name += (char)(0xE0 | (c >> 12));
                name += (char)(0x80 | ((c >> 6) & 0x3F));
                name += (char)(0x80 | (c & 0x3F));
            }
        }
        return true;
    }

    bool walk(uint32_t node, const std::string &path, int depth)
    {
        if (depth > 32)
            return false;
        const unsigned char *p = at(tree_ + node * node_size_, 14);
        if (p == nullptr)
            return false;
        std::string name;
        if (node != 0 && !name_of(be32(p), name))
            return false;
        std::string full = path.empty() ? name : path + "/" + name;
        uint16_t flags = be16(p + 4);
        if (flags & 2)
        {
            /* a folder: its children are consecutive nodes */
            uint32_t count = be32(p + 6);
            uint32_t first = be32(p + 10);
            if (count > 100000)
                return false;
            for (uint32_t i = 0 ; i < count ; ++i)
            {
                if (!walk(first + i, full, depth + 1))
                    return false;
            }
            return true;
        }
        /* zstd compression (flag 4, Qt 6) isn't supported */
        if (flags & 4)
            return true;
        Entry entry;
        entry.offset = data_ + be32(p + 10);
        entry.compressed = (flags & 1) != 0;
        entries_[full] = entry;
        return true;
    }

    std::string path_;
    std::map<std::string, Entry> entries_;
    std::vector<unsigned char> index_;
    uint32_t index_start_ = 0;
    uint32_t tree_ = 0;
    uint32_t data_ = 0;
    uint32_t names_ = 0;
    uint32_t node_size_ = 14;
};


/* ------------------------------------------------------------------------ */
/*
 *   The game's map state, from its flat JSON object: strings, and true or
 *   false.  Numbers and nested values aren't expected and are skipped.
 */
class MapState
{
public:
    void clear()
    {
        strings_.clear();
        flags_.clear();
    }

    void parse(const char *p, size_t len)
    {
        clear();
        const char *end = p + len;
        auto skip_ws = [&]() {
            while (p < end && (*p == ' ' || *p == '\t' || *p == '\r'
                               || *p == '\n' || *p == ','))
                ++p;
        };
        auto read_string = [&](std::string &out) -> bool {
            if (p >= end || *p != '"')
                return false;
            ++p;
            out.clear();
            while (p < end && *p != '"')
            {
                if (*p == '\\' && p + 1 < end)
                    ++p;
                out += *p++;
            }
            if (p >= end)
                return false;
            ++p;
            return true;
        };

        skip_ws();
        if (p >= end || *p != '{')
            return;
        ++p;
        for (;;)
        {
            skip_ws();
            if (p >= end || *p == '}')
                break;
            std::string key;
            if (!read_string(key))
                break;
            skip_ws();
            if (p >= end || *p != ':')
                break;
            ++p;
            skip_ws();
            if (p < end && *p == '"')
            {
                std::string value;
                if (!read_string(value))
                    break;
                strings_[key] = value;
            }
            else if (end - p >= 4 && memcmp(p, "true", 4) == 0)
            {
                flags_[key] = true;
                p += 4;
            }
            else if (end - p >= 5 && memcmp(p, "false", 5) == 0)
            {
                flags_[key] = false;
                p += 5;
            }
            else
            {
                /* anything else: skip to the next member */
                while (p < end && *p != ',' && *p != '}')
                    ++p;
            }
        }
    }

    std::string get(const std::string &key) const
    {
        auto it = strings_.find(key);
        return it != strings_.end() ? it->second : std::string();
    }

    /* has the game set this flag to true? */
    bool is_set(const std::string &key) const
    {
        auto it = flags_.find(key);
        return it != flags_.end() && it->second;
    }

    bool has_flag(const std::string &key) const
        { return flags_.find(key) != flags_.end(); }

private:
    std::map<std::string, std::string> strings_;
    std::map<std::string, bool> flags_;
};


/* ------------------------------------------------------------------------ */
/*
 *   A picture from maps.bin on the GPU, with its smaller copies
 */
struct Picture
{
    static const int MAX_LEVELS = 8;

    int width = 0;
    int height = 0;
    int levels = 0;
    int level_width[MAX_LEVELS] = {};
    int level_height[MAX_LEVELS] = {};
    CGE::Gfx::Texture *tex[MAX_LEVELS] = {};
    bool tracked = false;
    bool failed = false;

    /* the copy to draw at a width of 'screen_width' pixels */
    CGE::Gfx::Texture *for_width(float screen_width) const
    {
        int level = 0;
        while (level + 1 < levels && level_width[level + 1] >= screen_width)
            ++level;
        return tex[level];
    }

    void release()
    {
        for (int i = 0 ; i < MAX_LEVELS ; ++i)
        {
            if (tracked)
                gui_untrack_texture(&tex[i]);
            gui_delete_texture(tex[i]);
            tex[i] = nullptr;
        }
        tracked = false;
        levels = 0;
    }
};

/* the pixels as RGBA, whatever the decoder gave */
std::vector<unsigned char> to_rgba(const CGE::Image &img)
{
    const int channels = (int)img.getNumChannels();
    const size_t count = (size_t)img.getWidth() * img.getHeight();
    const unsigned char *in = img.getData();
    std::vector<unsigned char> out(count * 4);
    for (size_t i = 0 ; i < count ; ++i, in += channels)
    {
        unsigned char *o = &out[i * 4];
        switch (channels)
        {
        case 1:
            o[0] = o[1] = o[2] = in[0];
            o[3] = 255;
            break;
        case 2:
            o[0] = o[1] = o[2] = in[0];
            o[3] = in[1];
            break;
        case 3:
            o[0] = in[0]; o[1] = in[1]; o[2] = in[2];
            o[3] = 255;
            break;
        default:
            memcpy(o, in, 4);
            break;
        }
    }
    return out;
}

/*
 *   Half the size, each pixel the average of four; colors weighted by their
 *   alpha, so transparent pixels don't darken the edges
 */
std::vector<unsigned char> halve(const std::vector<unsigned char> &src,
                                 int w, int h, int &nw, int &nh)
{
    nw = (std::max)(1, w / 2);
    nh = (std::max)(1, h / 2);
    std::vector<unsigned char> dst((size_t)nw * nh * 4);
    for (int y = 0 ; y < nh ; ++y)
    {
        for (int x = 0 ; x < nw ; ++x)
        {
            unsigned sum[4] = {0, 0, 0, 0};
            unsigned alpha = 0;
            for (int dy = 0 ; dy < 2 ; ++dy)
            {
                for (int dx = 0 ; dx < 2 ; ++dx)
                {
                    int sx = (std::min)(x * 2 + dx, w - 1);
                    int sy = (std::min)(y * 2 + dy, h - 1);
                    const unsigned char *p = &src[((size_t)sy * w + sx) * 4];
                    for (int c = 0 ; c < 3 ; ++c)
                        sum[c] += p[c] * p[3];
                    alpha += p[3];
                }
            }
            unsigned char *o = &dst[((size_t)y * nw + x) * 4];
            for (int c = 0 ; c < 3 ; ++c)
                o[c] = (unsigned char)(alpha > 0 ? sum[c] / alpha : 0);
            o[3] = (unsigned char)(alpha / 4);
        }
    }
    return dst;
}


/* ------------------------------------------------------------------------ */
/*
 *   The game's map: maps.bin and what the game told
 */
struct GameMap
{
    std::string dir;
    ResourceFile file;
    MapState state;
    bool opened = false;

    bool available()
    {
        if (!opened && !dir.empty())
        {
            opened = true;
            std::string path = dir;
            if (!path.empty() && path.back() != '/' && path.back() != '\\')
                path += OSPATHCHAR;
            file.open(path + "maps.bin");
        }
        return file.is_open();
    }
};

GameMap game_map;


/* ------------------------------------------------------------------------ */
/*
 *   The map screen while it is open
 */
class MapScreen
{
public:
    MapScreen()
    {
        section_ = game_map.state.get("current_section");
        room_ = game_map.state.get("current_room");
        world_ = !has_area_map();
    }

    ~MapScreen()
    {
        for (auto &pic : pictures_)
            pic.second->release();
    }

    /* build a frame; returns true once the map is closed */
    bool frame()
    {
        ImGuiIO &io = ImGui::GetIO();
        const ImGuiViewport *vp = ImGui::GetMainViewport();
        origin_ = vp->Pos;
        size_ = vp->Size;
        ui_ = gui_content_scale();

        ImGui::SetNextWindowPos(vp->Pos);
        ImGui::SetNextWindowSize(vp->Size);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(20, 24, 28, 255));
        ImGui::Begin("##ThaumistryMap", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove
                     | ImGuiWindowFlags_NoSavedSettings
                     | ImGuiWindowFlags_NoScrollWithMouse
                     | ImGuiWindowFlags_NoFocusOnAppearing);
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
        if (!focused_)
        {
            ImGui::SetWindowFocus();
            focused_ = true;
        }
        dl_ = ImGui::GetWindowDrawList();

        draw_cover(picture("maps/background.tif"));

        /*
         *   The big pictures take a moment to decode: say so for a frame
         *   before the first frame of a view loads them
         */
        const std::string map_name = map_picture_name();
        if (pictures_.find(map_name) == pictures_.end() && !announced_)
        {
            announced_ = true;
            const char *msg = "Loading the map...";
            ImVec2 ts = ImGui::CalcTextSize(msg);
            dl_->AddText(ImVec2(origin_.x + (size_.x - ts.x) / 2,
                                origin_.y + (size_.y - ts.y) / 2),
                         IM_COL32(255, 255, 255, 255), msg);
            ImGui::End();
            return false;
        }
        announced_ = false;

        Picture *map = picture(map_name);
        if (map != nullptr && map->width > 0)
        {
            map_w_ = map->width;
            map_h_ = map->height;
            if (!fitted_)
                fit();
            draw_map(map);
            if (world_)
                draw_world_overlays();
            else
                draw_area_overlays();
        }

        draw_screen_overlays();
        bool close = draw_buttons();
        handle_input(io, close);

        ImGui::End();
        return close;
    }

private:
    /* ---- views ---- */

    bool has_area_map() const
    {
        return !section_.empty()
            && game_map.file.has("maps/" + section_ + "/map.tif");
    }

    std::string map_picture_name() const
    {
        return world_ ? std::string("maps/map_overview.tif")
                      : "maps/" + section_ + "/map.tif";
    }

    void toggle_view()
    {
        if (!world_ || has_area_map())
        {
            world_ = !world_;
            fitted_ = false;
        }
    }

    /*
     *   How far the map zooms out (all of it in the window), and the first
     *   view: an area's map has wide empty margins around its rooms, so it
     *   starts on the rooms; the world map starts whole.
     */
    void fit()
    {
        float avail_w = size_.x * 0.94f;
        float avail_h = size_.y * 0.80f;
        fit_zoom_ = (std::min)(avail_w / map_w_, avail_h / map_h_);
        zoom_ = fit_zoom_;
        center_x_ = map_w_ / 2.0f;
        center_y_ = map_h_ / 2.0f;
        fitted_ = true;

        if (world_)
            return;
        float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
        const std::string folder = "maps/" + section_;
        for (const std::string &file : game_map.file.list(folder))
        {
            const Place *place = find_place(rooms, sizeof(rooms) / sizeof(rooms[0]),
                                            file.substr(0, file.rfind('.')));
            if (place == nullptr)
                continue;
            x0 = (std::min)(x0, (float)place->x);
            y0 = (std::min)(y0, (float)place->y);
            x1 = (std::max)(x1, (float)place->x);
            y1 = (std::max)(y1, (float)place->y);
        }
        if (x0 > x1)
            return;
        /* a room's box is about 300 pixels wide; leave room around them */
        const float margin = 450.0f;
        x0 -= margin; y0 -= margin; x1 += margin; y1 += margin;
        zoom_ = (std::max)(fit_zoom_, (std::min)(avail_w / (x1 - x0), avail_h / (y1 - y0)));
        zoom_ = (std::min)(zoom_, max_zoom());
        center_x_ = (x0 + x1) / 2;
        center_y_ = (y0 + y1) / 2;
        clamp_center();
    }

    float max_zoom() const
        { return (std::max)(fit_zoom_ * 1.01f, 1.5f * ui_); }

    void zoom_at(float factor, ImVec2 screen)
    {
        float new_zoom = (std::max)(fit_zoom_, (std::min)(max_zoom(), zoom_ * factor));
        /* the map point under 'screen' stays there */
        float mx = center_x_ + (screen.x - mid().x) / zoom_;
        float my = center_y_ + (screen.y - mid().y) / zoom_;
        zoom_ = new_zoom;
        center_x_ = mx - (screen.x - mid().x) / zoom_;
        center_y_ = my - (screen.y - mid().y) / zoom_;
        clamp_center();
    }

    void pan(float dx, float dy)
    {
        center_x_ -= dx / zoom_;
        center_y_ -= dy / zoom_;
        clamp_center();
    }

    void clamp_center()
    {
        center_x_ = (std::max)(0.0f, (std::min)((float)map_w_, center_x_));
        center_y_ = (std::max)(0.0f, (std::min)((float)map_h_, center_y_));
    }

    ImVec2 mid() const
        { return ImVec2(origin_.x + size_.x / 2, origin_.y + size_.y / 2); }

    ImVec2 to_screen(float x, float y) const
    {
        return ImVec2(mid().x + (x - center_x_) * zoom_,
                      mid().y + (y - center_y_) * zoom_);
    }

    /* ---- pictures ---- */

    Picture *picture(const std::string &name)
    {
        auto it = pictures_.find(name);
        Picture *pic;
        if (it == pictures_.end())
        {
            pic = new Picture();
            pictures_[name].reset(pic);
        }
        else
            pic = it->second.get();
        /* not loaded yet, or the render device lost it (Android) */
        if (!pic->failed && pic->tex[0] == nullptr)
            load(name, *pic);
        return pic->failed ? nullptr : pic;
    }

    void load(const std::string &name, Picture &pic)
    {
        std::vector<unsigned char> data;
        std::unique_ptr<CGE::Image> img;
        if (game_map.file.read(name, data))
            img.reset(CGE::ImageLoader::load(data.data(), (unsigned)data.size(),
                                             CGE::ImageLoader::TIFF));
        if (!img || img->getWidth() == 0)
        {
            pic.failed = true;
            return;
        }
        std::vector<unsigned char> rgba = to_rgba(*img);
        int w = (int)img->getWidth();
        int h = (int)img->getHeight();
        img.reset();
        data.clear();
        data.shrink_to_fit();

        pic.width = w;
        pic.height = h;
        pic.levels = 0;
        for (int level = 0 ; level < Picture::MAX_LEVELS ; ++level)
        {
            pic.tex[level] = gui_upload_texture(pic.tex[level], w, h, rgba.data());
            pic.level_width[level] = w;
            pic.level_height[level] = h;
            pic.levels = level + 1;
            if ((std::max)(w, h) <= 256 || pic.tex[level] == nullptr)
                break;
            int nw, nh;
            rgba = halve(rgba, w, h, nw, nh);
            w = nw;
            h = nh;
        }
        if (pic.tex[0] == nullptr)
        {
            pic.failed = true;
            return;
        }
        if (!pic.tracked)
        {
            for (int i = 0 ; i < Picture::MAX_LEVELS ; ++i)
                gui_track_texture(&pic.tex[i]);
            pic.tracked = true;
        }
    }

    void draw(const Picture *pic, ImVec2 p0, ImVec2 p1, ImU32 col = IM_COL32_WHITE)
    {
        if (pic == nullptr)
            return;
        CGE::Gfx::Texture *tex = pic->for_width(p1.x - p0.x);
        if (tex != nullptr)
            dl_->AddImage(ImGui_ImplCGE_TextureID(tex), p0, p1,
                          ImVec2(0, 0), ImVec2(1, 1), col);
    }

    /* a picture over the whole window, cut to keep its proportions */
    void draw_cover(const Picture *pic)
    {
        if (pic == nullptr || pic->width == 0)
            return;
        float scale = (std::max)(size_.x / pic->width, size_.y / pic->height);
        float w = pic->width * scale;
        float h = pic->height * scale;
        ImVec2 p0(origin_.x + (size_.x - w) / 2, origin_.y + (size_.y - h) / 2);
        draw(pic, p0, ImVec2(p0.x + w, p0.y + h));
    }

    /* a picture at a map position (its center) at the map's zoom */
    void draw_on_map(const Picture *pic, float x, float y)
    {
        if (pic == nullptr)
            return;
        ImVec2 c = to_screen(x, y);
        float w = pic->width * zoom_;
        float h = pic->height * zoom_;
        draw(pic, ImVec2(c.x - w / 2, c.y - h / 2), ImVec2(c.x + w / 2, c.y + h / 2));
    }

    void draw_map(const Picture *map)
    {
        ImVec2 p0 = to_screen(0, 0);
        ImVec2 p1 = to_screen((float)map->width, (float)map->height);
        draw(map, p0, p1);
    }

    /* the current place, pulsing so it's found at a glance */
    void draw_dot(ImVec2 c)
    {
        Picture *dot = picture("maps/dot.tif");
        float r = 13.0f * ui_;
        float pulse = 0.65f + 0.35f * (float)std::sin(ImGui::GetTime() * 4.0);
        ImU32 col = IM_COL32(255, 255, 255, (int)(255 * pulse));
        if (dot != nullptr)
            draw(dot, ImVec2(c.x - r, c.y - r), ImVec2(c.x + r, c.y + r), col);
        else
            dl_->AddCircleFilled(c, r, IM_COL32(255, 0, 0, (int)(255 * pulse)));
    }

    /* clouds over the rooms not visited yet, and the dot on the current one */
    void draw_area_overlays()
    {
        const std::string folder = "maps/" + section_;
        for (const std::string &file : game_map.file.list(folder))
        {
            if (file.compare(0, 3, "rm_") != 0 || file.size() < 8)
                continue;
            std::string room = file.substr(0, file.size() - 4);
            if (game_map.state.is_set(room))
                continue;
            const Place *place = find_place(rooms, sizeof(rooms) / sizeof(rooms[0]), room);
            if (place != nullptr)
                draw_on_map(picture(folder + "/" + file), (float)place->x, (float)place->y);
        }
        const Place *here = find_place(rooms, sizeof(rooms) / sizeof(rooms[0]), room_);
        if (here != nullptr)
            draw_dot(to_screen((float)here->x, (float)here->y));
    }

    /*
     *   The pins of the areas reached.  They keep their size on the screen,
     *   so the tip (near the bottom middle) stays on the place, whatever the
     *   zoom.
     */
    /*
     *   The area with the player's pin.  The game names the section it shows
     *   ("map_overview" for places without an area map, like the lab, and
     *   "map_tech_museum_basement", which is part of the museum), so: the
     *   section if it has a pin, else the pin the section's name starts
     *   with, else the room's own area ("rm_lab" is in "map_lab").
     */
    std::string current_area() const
    {
        const size_t count = sizeof(areas) / sizeof(areas[0]);
        if (find_place(areas, count, section_) != nullptr)
            return section_;
        for (const Place &area : areas)
        {
            if (section_.compare(0, strlen(area.name), area.name) == 0)
                return area.name;
        }
        if (room_.compare(0, 3, "rm_") == 0)
        {
            std::string area = "map_" + room_.substr(3);
            if (find_place(areas, count, area) != nullptr)
                return area;
        }
        return std::string();
    }

    void draw_world_overlays()
    {
        const float scale = 0.55f * ui_;
        const std::string here = current_area();
        for (const Place &area : areas)
        {
            bool current = here == area.name;
            if (!current && !game_map.state.is_set(area.name))
                continue;
            Picture *label = picture(std::string("maps/label_") + area.name + ".tif");
            if (label == nullptr)
                continue;
            /* the tip is 10 pixels above the picture's bottom edge */
            float tip_dy = label->height / 2.0f - 10.0f;
            ImVec2 tip = to_screen((float)area.x, area.y + tip_dy);
            float w = label->width * scale;
            float h = label->height * scale;
            ImVec2 p0(tip.x - w / 2, tip.y - (label->height - 10.0f) * scale);
            draw(label, p0, ImVec2(p0.x + w, p0.y + h));
            if (current)
                draw_dot(tip);
        }
    }

    /*
     *   The picture of a set of sizes ("compass_%d") closest above the wanted
     *   width; the sizes are percentages of the full one
     */
    Picture *sized(const char *pattern, const int *percents, int count,
                   int full_width, float want_width)
    {
        int pick = percents[count - 1];
        for (int i = 0 ; i < count ; ++i)
        {
            if (full_width * percents[i] / 100.0f >= want_width)
            {
                pick = percents[i];
                break;
            }
        }
        char name[128];
        snprintf(name, sizeof(name), pattern, pick);
        return picture(name);
    }

    void draw_screen_overlays()
    {
        const float margin = 12.0f * ui_;

        /* the area's title, centered at the top */
        if (!world_)
        {
            Picture *title = picture("maps/" + section_ + "/title.tif");
            if (title != nullptr)
            {
                float s = (std::min)(0.8f * ui_, size_.x * 0.5f / title->width);
                float w = title->width * s;
                float h = title->height * s;
                ImVec2 p0(origin_.x + (size_.x - w) / 2, origin_.y + margin);
                draw(title, p0, ImVec2(p0.x + w, p0.y + h));
            }
        }

        /* the compass in the bottom right corner */
        static const int compass_sizes[] = { 25, 33, 50, 75, 100 };
        float compass_w = (std::min)(size_.x, size_.y) * 0.16f;
        Picture *compass = sized("maps/compass_%d.tif", compass_sizes, 5, 768, compass_w);
        if (compass != nullptr)
        {
            ImVec2 p1(origin_.x + size_.x - margin, origin_.y + size_.y - margin);
            draw(compass, ImVec2(p1.x - compass_w, p1.y - compass_w), p1);
        }

        /* the legend of the controls in the bottom left corner */
        Picture *legend;
        float legend_w;
#ifdef ANDROID
        static const int legend_sizes[] = { 35, 50, 75, 100 };
        /* at most 40% of the width and a quarter of the height (it's 690x428 at 100%) */
        legend_w = (std::min)((std::min)(size_.x * 0.4f, 690.0f * ui_ * 0.5f),
                              size_.y * 0.25f * 690.0f / 428.0f);
        legend = sized(has_area_map() ? "maps/legend_mobile_%d.tif"
                                      : "maps/legend_mobile_notoggle_%d.tif",
                       legend_sizes, 4, 690, legend_w);
#else
        static const int legend_sizes[] = { 25, 33, 50, 75, 100 };
        /* at most 22% of the width and 30% of the height (it's 1035x881 at 100%) */
        legend_w = (std::min)((std::min)(size_.x * 0.22f, 1035.0f * ui_ * 0.3f),
                              size_.y * 0.3f * 1035.0f / 881.0f);
        legend = sized("maps/legend_%d.tif", legend_sizes, 5, 1035, legend_w);
#endif
        if (legend != nullptr)
        {
            float h = legend_w * legend->height / legend->width;
            ImVec2 p0(origin_.x + margin, origin_.y + size_.y - margin - h);
            draw(legend, p0, ImVec2(p0.x + legend_w, p0.y + h), IM_COL32(255, 255, 255, 230));
        }
    }

    /* the buttons in the top right corner; returns true for Close */
    bool draw_buttons()
    {
        const float margin = 12.0f * ui_;
        const char *toggle = world_ ? "Area Map" : "World Map";
        const char *labels[] = { toggle, "+", "-", "Close" };
        ImGuiStyle &style = ImGui::GetStyle();
        float total = 0;
        for (const char *label : labels)
            total += ImGui::CalcTextSize(label).x + style.FramePadding.x * 2 + style.ItemSpacing.x;
        ImGui::SetCursorScreenPos(ImVec2(origin_.x + size_.x - margin - total, origin_.y + margin));

        bool close = false;
        ImGui::BeginDisabled(world_ && !has_area_map());
        if (ImGui::Button(toggle))
            toggle_view();
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("+"))
            zoom_at(1.25f, mid());
        ImGui::SameLine();
        if (ImGui::Button("-"))
            zoom_at(0.8f, mid());
        ImGui::SameLine();
        if (ImGui::Button("Close"))
            close = true;
        return close;
    }

    void handle_input(ImGuiIO &io, bool &close)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)
            || ImGui::IsKeyPressed(ImGuiKey_AppBack, false))
            close = true;
        if (ImGui::IsKeyPressed(ImGuiKey_Tab, false))
            toggle_view();
        if (ImGui::IsKeyPressed(ImGuiKey_Equal) || ImGui::IsKeyPressed(ImGuiKey_KeypadAdd))
            zoom_at(1.25f, mid());
        if (ImGui::IsKeyPressed(ImGuiKey_Minus) || ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract))
            zoom_at(0.8f, mid());
        if (ImGui::IsKeyPressed(ImGuiKey_Home, false))
            fitted_ = false;

        /* the arrow keys pan at a steady speed */
        float step = 700.0f * ui_ * io.DeltaTime;
        if (ImGui::IsKeyDown(ImGuiKey_LeftArrow))
            pan(step, 0);
        if (ImGui::IsKeyDown(ImGuiKey_RightArrow))
            pan(-step, 0);
        if (ImGui::IsKeyDown(ImGuiKey_UpArrow))
            pan(0, step);
        if (ImGui::IsKeyDown(ImGuiKey_DownArrow))
            pan(0, -step);

        /* two fingers: pinch to zoom, and move together to pan */
        Input::Touch *touch = Input::Touch::instance();
        if (touch != nullptr && touch->getFingers().size() >= 2)
        {
            const auto &fingers = touch->getFingers();
            ImVec2 scale = io.DisplayFramebufferScale;
            if (scale.x <= 0 || scale.y <= 0)
                scale = ImVec2(1, 1);
            ImVec2 a(fingers[0].windowPos.x / scale.x, fingers[0].windowPos.y / scale.y);
            ImVec2 b(fingers[1].windowPos.x / scale.x, fingers[1].windowPos.y / scale.y);
            ImVec2 m(origin_.x + (a.x + b.x) / 2, origin_.y + (a.y + b.y) / 2);
            float dist = std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
            if (pinching_ && pinch_dist_ > 1.0f && dist > 1.0f)
            {
                pan(m.x - pinch_mid_.x, m.y - pinch_mid_.y);
                zoom_at(dist / pinch_dist_, m);
            }
            pinching_ = true;
            pinch_dist_ = dist;
            pinch_mid_ = m;
            dragging_ = false;
            return;
        }
        if (pinching_)
        {
            /* the last finger of a pinch doesn't start a drag */
            if (!io.MouseDown[0])
                pinching_ = false;
            return;
        }

        /* the mouse (and one finger): drag to pan, wheel to zoom, double click to switch */
        bool over_map = ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered();
        if (over_map && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            toggle_view();
        else if (over_map && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            dragging_ = true;
        if (dragging_)
        {
            if (io.MouseDown[0])
                pan(io.MouseDelta.x, io.MouseDelta.y);
            else
                dragging_ = false;
        }
        if (over_map && io.MouseWheel != 0.0f)
            zoom_at((float)std::pow(1.15, io.MouseWheel), io.MousePos);
    }

    std::map<std::string, std::unique_ptr<Picture>> pictures_;
    std::string section_;
    std::string room_;
    bool world_ = false;
    bool fitted_ = false;
    bool announced_ = false;
    bool focused_ = false;
    bool dragging_ = false;
    bool pinching_ = false;
    float pinch_dist_ = 0;
    ImVec2 pinch_mid_;
    int map_w_ = 1;
    int map_h_ = 1;
    float fit_zoom_ = 1;
    float zoom_ = 1;
    float center_x_ = 0;
    float center_y_ = 0;
    float ui_ = 1;
    ImVec2 origin_;
    ImVec2 size_;
    ImDrawList *dl_ = nullptr;
};

}

void guimap_set_game(const char *game_dir)
{
    game_map.file.close();
    game_map.state.clear();
    game_map.opened = false;
    game_map.dir = game_dir != nullptr ? game_dir : "";
}

void guimap_update(const char *json, size_t len)
{
    game_map.state.parse(json, len);
}

bool guimap_available()
{
    return game_map.available();
}

bool guimap_show()
{
    if (!guimap_available())
        return true;
    MapScreen screen;
    bool closed = false;
    while (!closed)
    {
        if (!gui_pump_frame([&]() { closed = screen.frame(); }))
            return false;
    }
    return true;
}
