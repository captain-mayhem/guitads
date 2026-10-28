/*
 *   tadssettings.cpp - the guit3 settings store (tadssettings.h) on the
 *   engine's settings file
 *
 *   One backend for every platform: CGE::Settings::user(), the engine's
 *   settings.ini in UserDirs::getConfigDir() (Windows: %APPDATA%\guit3,
 *   Linux: $XDG_CONFIG_HOME/guit3, the web: IndexedDB through IDBFS; "guit3"
 *   is the appName in guit3's engine.ini).  It replaces the registry backend
 *   (tadssettings_w32.cpp) and the INI file backend for the other platforms
 *   (tadssettings_portable.cpp).  See migration.md 5.27.
 *
 *   A key path "A\B\C" is the engine group "A/B/C", and a value "name" under
 *   it is the engine key "A/B/C/name".  Key and value names are
 *   percent-encoded where the engine's file format gives a character a
 *   meaning ('/' separates groups, '=' ends a name, a line break ends the
 *   line, a leading '[', ';' or '#' starts a section or a comment).  Value
 *   names are game file paths in "Game Profiles", so this matters.  Values
 *   are stored as text exactly as the old backends stored them (numbers in
 *   decimal, booleans as "Yes"/"No"), and binary values as binary.
 *
 *   The engine keeps a group only while it has values, while the registry
 *   also keeps empty keys.  Keys created empty with open_key(path, TRUE) are
 *   therefore remembered for the rest of the session, so that they exist and
 *   enumerate as before; an empty key isn't saved.
 *
 *   The engine saves the file at shutdown, and on the web after every
 *   frame.  The old backends wrote every change at once, so on the desktop
 *   this one also saves when a key is closed and when one is deleted.
 *
 *   On the first start, the settings of the old backends are imported once:
 *   on Windows the registry tree under HKEY_CURRENT_USER\<w32_pref_key_name>,
 *   on Linux the old file $XDG_CONFIG_HOME/HTML TADS 3/settings.ini.  The
 *   value "imported" at the top level of the engine file marks that it's
 *   done.  The old settings stay where they are; the classic HTML TADS
 *   interpreter, which shares the registry key, keeps using them.
 */

#ifdef _WIN32
#include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include <platform/settings.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef TADSSETTINGS_H
#include "tadssettings.h"
#endif

/*
 *   guitrt3.cpp's registry key name and application folder name.  Declared
 *   directly rather than via guimain.h, which also declares a page of
 *   Win32-only prototypes that need tadsplat.h.
 */
extern const char w32_pref_key_name[];
extern const char w32_appdata_dir[];


/* an open key - the engine group it refers to */
struct tads_settings_key_opaque
{
    std::string group;
};


/* ------------------------------------------------------------------------ */
/*
 *   Names and paths
 */
namespace {

/* percent-encode the characters the engine's file format gives a meaning */
std::string encode_name(const char *name)
{
    static const char digits[] = "0123456789ABCDEF";
    std::string out;
    for (const char *p = name ; *p != '\0' ; ++p)
    {
        unsigned char c = (unsigned char)*p;
        if (c == '%' || c == '/' || c == '=' || c == '\r' || c == '\n'
            || (p == name && (c == '[' || c == ';' || c == '#')))
        {
            out += '%';
            out += digits[c >> 4];
            out += digits[c & 0xf];
        }
        else
            out += (char)c;
    }
    return out;
}

int hex_digit(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

std::string decode_name(const std::string &name)
{
    std::string out;
    for (size_t i = 0 ; i < name.size() ; ++i)
    {
        int hi, lo;
        if (name[i] == '%' && i + 2 < name.size()
            && (hi = hex_digit(name[i + 1])) >= 0
            && (lo = hex_digit(name[i + 2])) >= 0)
        {
            out += (char)((hi << 4) | lo);
            i += 2;
        }
        else
            out += name[i];
    }
    return out;
}

/* the engine group of a backslash-delimited key path */
std::string path_to_group(const char *path)
{
    std::string group;
    std::string part;
    for (const char *p = path ; ; ++p)
    {
        if (*p == '\\' || *p == '\0')
        {
            if (!part.empty())
            {
                if (!group.empty())
                    group += '/';
                group += encode_name(part.c_str());
                part.clear();
            }
            if (*p == '\0')
                break;
        }
        else
            part += *p;
    }
    return group;
}

std::string value_key(tads_settings_key_t key, const textchar_t *valname)
{
    return key->group + "/" + encode_name(valname);
}

/* copy a string into a caller's buffer, truncating it */
size_t copy_out(const std::string &s, textchar_t *buf, size_t bufsiz)
{
    if (bufsiz == 0)
        return 0;
    size_t len = s.size() < bufsiz - 1 ? s.size() : bufsiz - 1;
    memcpy(buf, s.data(), len);
    buf[len] = '\0';
    return len;
}

/* keys created empty in this session (see the header comment) */
std::set<std::string> &empty_keys()
{
    static std::set<std::string> keys;
    return keys;
}

/* the immediate child keys of a group, sorted */
std::vector<std::string> child_keys(const std::string &group)
{
    std::vector<std::string> children =
        CGE::Settings::user().getGroups(group);

    std::string prefix = group + "/";
    for (const std::string &k : empty_keys())
    {
        if (k.size() > prefix.size()
            && k.compare(0, prefix.size(), prefix) == 0)
        {
            std::string rest = k.substr(prefix.size());
            children.push_back(rest.substr(0, rest.find('/')));
        }
    }

    std::sort(children.begin(), children.end());
    children.erase(std::unique(children.begin(), children.end()),
                   children.end());
    return children;
}

bool key_exists(const std::string &group)
{
    return !CGE::Settings::user().getKeys(group).empty()
        || empty_keys().count(group) != 0
        || !child_keys(group).empty();
}

/*
 *   A binary value has no text, so getString() returns the default for it -
 *   whichever default it's given, which no text value can match twice.
 */
bool is_binary(const std::string &key)
{
    CGE::Settings &s = CGE::Settings::user();
    return s.has(key) && s.getString(key, "a") == "a"
        && s.getString(key, "b") == "b";
}


/* ------------------------------------------------------------------------ */
/*
 *   Importing the old backends' settings, once
 */

#ifdef _WIN32

/* copy one registry key, its values and its child keys */
void import_reg_key(CGE::Settings &s, const std::string &path)
{
    HKEY key;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, path.c_str(), 0, KEY_READ, &key)
        != ERROR_SUCCESS)
        return;

    DWORD max_subkey_len = 0, max_name_len = 0, max_data_len = 0;
    RegQueryInfoKeyA(key, 0, 0, 0, 0, &max_subkey_len, 0, 0,
                     &max_name_len, &max_data_len, 0, 0);

    std::string group = path_to_group(path.c_str());
    std::vector<char> name(max_name_len + 1);
    std::vector<BYTE> data(max_data_len + 1);
    for (DWORD i = 0 ; ; ++i)
    {
        DWORD name_len = (DWORD)name.size();
        DWORD data_len = (DWORD)data.size();
        DWORD type;
        if (RegEnumValueA(key, i, name.data(), &name_len, 0, &type,
                          data.data(), &data_len) != ERROR_SUCCESS)
            break;

        std::string k = group + "/" + encode_name(name.data());
        if (type == REG_SZ)
        {
            /* the stored length may or may not include the terminator */
            std::string text((const char *)data.data(), data_len);
            text.erase(std::find(text.begin(), text.end(), '\0'), text.end());
            s.setString(k, text);
        }
        else if (type == REG_BINARY)
            s.setBinary(k, std::vector<uint8_t>(data.data(),
                                                 data.data() + data_len));
    }

    std::vector<char> subkey(max_subkey_len + 1);
    std::vector<std::string> subkeys;
    for (DWORD i = 0 ; ; ++i)
    {
        DWORD len = (DWORD)subkey.size();
        if (RegEnumKeyExA(key, i, subkey.data(), &len, 0, 0, 0, 0)
            != ERROR_SUCCESS)
            break;
        subkeys.push_back(subkey.data());
    }
    RegCloseKey(key);

    for (const std::string &sub : subkeys)
        import_reg_key(s, path + "\\" + sub);
}

void import_old_settings(CGE::Settings &s)
{
    import_reg_key(s, w32_pref_key_name);
}

#elif !defined(__EMSCRIPTEN__)

/* the old file backend's escapes: "\\", "\n" and "\r" */
std::string unescape_value(const std::string &v)
{
    std::string out;
    for (size_t i = 0 ; i < v.size() ; ++i)
    {
        if (v[i] == '\\' && i + 1 < v.size())
        {
            char c = v[++i];
            out += (c == 'n' ? '\n' : c == 'r' ? '\r' : c);
        }
        else
            out += v[i];
    }
    return out;
}

/*
 *   Read the old file ($XDG_CONFIG_HOME/HTML TADS 3/settings.ini): a section
 *   per backslash-delimited key path, "name=value" lines, and hex-encoded
 *   binary values marked with a ":bin" suffix on the name.  Its names were
 *   written as they are, so they can contain '/'; the engine's own parser
 *   can't read them back, hence this one.
 */
void import_old_settings(CGE::Settings &s)
{
    std::string path;
    const char *xdg = getenv("XDG_CONFIG_HOME");
    const char *home = getenv("HOME");
    if (xdg != 0 && xdg[0] != '\0')
        path = xdg;
    else if (home != 0)
        path = std::string(home) + "/.config";
    else
        return;
    path += std::string("/") + w32_appdata_dir + "/settings.ini";

    FILE *fp = fopen(path.c_str(), "r");
    if (fp == 0)
        return;

    std::string group;
    std::string line;
    char buf[4096];
    while (fgets(buf, sizeof(buf), fp) != 0)
    {
        line += buf;
        if (line.empty() || line.back() != '\n')
        {
            if (!feof(fp))
                continue;
        }
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
            line.pop_back();

        if (line.size() >= 2 && line.front() == '[' && line.back() == ']')
            group = path_to_group(line.substr(1, line.size() - 2).c_str());
        else if (!group.empty() && line.find('=') != std::string::npos)
        {
            size_t eq = line.find('=');
            std::string name = line.substr(0, eq);
            std::string val = line.substr(eq + 1);
            static const std::string bin_suffix = ":bin";
            if (name.size() > bin_suffix.size()
                && name.compare(name.size() - bin_suffix.size(),
                                bin_suffix.size(), bin_suffix) == 0)
            {
                name.erase(name.size() - bin_suffix.size());
                std::vector<uint8_t> data;
                for (size_t i = 0 ; i + 1 < val.size() ; i += 2)
                {
                    int hi = hex_digit(val[i]), lo = hex_digit(val[i + 1]);
                    if (hi < 0 || lo < 0)
                        break;
                    data.push_back((uint8_t)((hi << 4) | lo));
                }
                s.setBinary(group + "/" + encode_name(name.c_str()), data);
            }
            else
                s.setString(group + "/" + encode_name(name.c_str()),
                            unescape_value(val));
        }
        line.clear();
    }

    fclose(fp);
}

#else

/* the old file backend wrote to the browser's memory only */
void import_old_settings(CGE::Settings &) { }

#endif

/*
 *   Write the changes now.  On the web the engine saves after every frame,
 *   pumped ones too, and each save starts a sync to IndexedDB, so saving
 *   here as well would only start overlapping syncs.
 */
void save_changes()
{
#ifndef __EMSCRIPTEN__
    CGE::Settings::saveUser();
#endif
}

/* the engine's settings, with the old settings imported on first use */
CGE::Settings &store()
{
    static bool checked = false;
    CGE::Settings &s = CGE::Settings::user();
    if (!checked)
    {
        checked = true;
        if (!s.has("imported"))
        {
            import_old_settings(s);
            s.setBool("imported", true);
            save_changes();
        }
    }
    return s;
}

} // namespace


/* ------------------------------------------------------------------------ */
tads_settings_key_t CTadsSettings::open_key(const textchar_t *path, int create)
{
    store();
    std::string group = path_to_group(path);
    if (!key_exists(group))
    {
        if (!create)
            return 0;
        empty_keys().insert(group);
    }

    tads_settings_key_opaque *key = new tads_settings_key_opaque();
    key->group = group;
    return key;
}

void CTadsSettings::close_key(tads_settings_key_t key)
{
    delete key;
    save_changes();
}

int CTadsSettings::value_exists(tads_settings_key_t key,
                                const textchar_t *valname)
{
    return key != 0 && store().has(value_key(key, valname));
}

long CTadsSettings::query_key_long(tads_settings_key_t key,
                                   const textchar_t *valname)
{
    char buf[128];
    query_key_str(key, valname, buf, sizeof(buf));
    return get_atol(buf);
}

int CTadsSettings::query_key_bool(tads_settings_key_t key,
                                  const textchar_t *valname)
{
    char buf[128];
    query_key_str(key, valname, buf, sizeof(buf));
    return (buf[0] == 'y' || buf[0] == 'Y');
}

size_t CTadsSettings::query_key_str(tads_settings_key_t key,
                                    const textchar_t *valname,
                                    textchar_t *buf, size_t bufsiz)
{
    if (key == 0)
        return copy_out("", buf, bufsiz);
    return copy_out(store().getString(value_key(key, valname)), buf, bufsiz);
}

size_t CTadsSettings::query_key_binary(tads_settings_key_t key,
                                       const textchar_t *valname,
                                       void *buf, size_t bufsiz)
{
    if (key == 0)
        return 0;
    std::vector<uint8_t> data = store().getBinary(value_key(key, valname));

    /* like RegQueryValueEx(), fail rather than truncate */
    if (data.size() > bufsiz)
        return 0;
    if (!data.empty())
        memcpy(buf, data.data(), data.size());
    return data.size();
}

void CTadsSettings::set_key_long(tads_settings_key_t key,
                                 const textchar_t *valname, long val)
{
    char buf[32];
    sprintf(buf, "%ld", val);
    set_key_str(key, valname, buf, strlen(buf));
}

void CTadsSettings::set_key_str(tads_settings_key_t key,
                                const textchar_t *valname,
                                const textchar_t *str, size_t len)
{
    if (key != 0)
        store().setString(value_key(key, valname), std::string(str, len));
}

void CTadsSettings::set_key_bool(tads_settings_key_t key,
                                 const textchar_t *valname, int val)
{
    set_key_str(key, valname, val ? "Yes" : "No", val ? 3 : 2);
}

void CTadsSettings::set_key_binary(tads_settings_key_t key,
                                   const textchar_t *valname,
                                   void *buf, size_t bufsiz)
{
    if (key != 0)
        store().setBinary(value_key(key, valname),
                          std::vector<uint8_t>((uint8_t *)buf,
                                               (uint8_t *)buf + bufsiz));
}

int CTadsSettings::enum_subkeys(tads_settings_key_t key, unsigned int idx,
                                textchar_t *buf, size_t bufsiz)
{
    if (key == 0)
        return TADS_SETTINGS_ENUM_END;

    std::vector<std::string> children = child_keys(key->group);
    if (idx >= children.size())
        return TADS_SETTINGS_ENUM_END;

    copy_out(decode_name(children[idx]), buf, bufsiz);
    return TADS_SETTINGS_ENUM_OK;
}

int CTadsSettings::enum_str_values(tads_settings_key_t key, unsigned int idx,
                                   textchar_t *namebuf, size_t namebufsiz,
                                   textchar_t *valbuf, size_t valbufsiz)
{
    if (key == 0)
        return TADS_SETTINGS_ENUM_END;

    std::vector<std::string> names = store().getKeys(key->group);
    if (idx >= names.size())
        return TADS_SETTINGS_ENUM_END;

    copy_out(decode_name(names[idx]), namebuf, namebufsiz);
    std::string k = key->group + "/" + names[idx];
    if (is_binary(k))
        return TADS_SETTINGS_ENUM_SKIP;

    copy_out(store().getString(k), valbuf, valbufsiz);
    return TADS_SETTINGS_ENUM_OK;
}

int CTadsSettings::delete_key(const textchar_t *path)
{
    std::string group = path_to_group(path);
    store().removeGroup(group);

    std::set<std::string> &keys = empty_keys();
    std::string prefix = group + "/";
    for (std::set<std::string>::iterator it = keys.begin() ; it != keys.end() ; )
    {
        if (*it == group || it->compare(0, prefix.size(), prefix) == 0)
            it = keys.erase(it);
        else
            ++it;
    }

    save_changes();
    return 0;
}
