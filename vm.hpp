#pragma once

#include <filesystem>
#include <regex>
#include <string>
#include <unordered_map>

#include "util.hpp"
#include <JSON.h>

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#include "fobject.hpp"

namespace fs = std::filesystem;

struct ModVersion {
    int major, minor, patch;
};

struct Dependency
{
    enum class Operator { le, lt, eq, gt, ge };
    enum class Type { optional, required, forbidden, optional_hidden };
    std::string modName;
    Type type;
    Operator op;
    ModVersion versionComp;
};

struct Mod
{
    std::string name;
    fs::path path;
    std::vector<Dependency> declared_dependencies;
    std::vector<Mod*> dependencies;
    int loaded = -1;
};


void anal(int err);
//{
//    if (err != LUA_OK)
//    {
//        perror_l(err);
//        exit(-1);
//    }
//}
typedef int (*lua_CFunction) (lua_State* L);


struct VM
{

    static std::string normalize(const fs::path& path)
    {
        return ws2s(fs::canonical(path).wstring());
    }

    lua_State* L = nullptr;

    const fs::path game_dir;
    const fs::path corelib;
    const fs::path baselib;
    const fs::path mod_dir;
    const fs::path cwd;

    std::unordered_map<std::string, fs::path> script_path_to_mod_path;
    std::unordered_map<std::string, Mod*> mod_name_to_mod;
    std::vector<Mod*> modlist;

    void iterate_mod(fs::path dir)
    {
        for (auto& p : fs::recursive_directory_iterator(dir))
        {
            if (p.is_regular_file() && p.path().extension() == ".lua")
            {
                std::string key = normalize(p.path().wstring());
                script_path_to_mod_path.emplace(key, dir);
            }
        }
    }


    template<typename T>
    void load_mods(std::vector<Mod*>& mods, int stage, T func)
    {
        for (Mod* mod : mods)
        {
            if (mod->loaded == stage) continue;

            load_mods(mod->dependencies, stage, func);

            func(mod);
            mod->loaded = stage;
        }
    }

    // const std::string dep_pattern_str = R"(^(([!?]|\(\?\)) *)?([a-zA-Z0-9_-]+)( *([<>=]=?) *((\d+\.){1,2}\d+))?$)";
    // const std::string dep_pattern_str = R"(^(?>(?<prefix>[!?]|\(\?\)) *)?(?<name>[a-zA-Z0-9_-]+)(?> *(?<operator>[<>=]=?) *(?<version>(?>\d+\.){1,2}\d+))?$)";
    // const std::string dep_pattern_str = R"(^(?>([!?]|\(\?\)) *)?)";
    const std::string dep_pattern_str = R"(^(?:([!?]|\(\?\)) *)?([a-zA-Z0-9_-]+).*)";

    const std::regex dep_pattern = std::regex(dep_pattern_str);

    void discover_mods();

    VM(const fs::path &game_dir);

    void run_data_stage();
    

    static std::string lua_type_to_string(int type)
    {
        switch (type)
        {
            case LUA_TNONE: return "LUA_TNONE";
            case LUA_TNIL: return "LUA_TNIL";
            case LUA_TBOOLEAN: return "LUA_TBOOLEAN";
            case LUA_TLIGHTUSERDATA: return "LUA_TLIGHTUSERDATA";
            case LUA_TNUMBER: return "LUA_TNUMBER";
            case LUA_TSTRING: return "LUA_TSTRING";
            case LUA_TTABLE: return "LUA_TTABLE";
            case LUA_TFUNCTION: return "LUA_TFUNCTION";
            case LUA_TUSERDATA: return "LUA_TUSERDATA";
            case LUA_TTHREAD: return "LUA_TTHREAD";
            default: return "!<unknown>";
        }
    }

    static std::string lua_key(lua_State* L, int index)
    {
        int type = lua_type(L, index);

        switch (type)
        {
            case LUA_TSTRING:
            {
                return std::string(lua_tostring(L, index));
            }
            case LUA_TNUMBER:
            {
                int isnum;
                if (lua_Integer d = lua_tointegerx(L, index, &isnum); isnum)
                {
                    return std::to_string(uint64_t(d));
                }
                else
                {
                    return std::to_string(double(lua_tonumber(L, index)));
                }
            }
            case LUA_TBOOLEAN:
            {
                return lua_toboolean(L, index) ? "true" : "false";
                break;
            }
            default:
            {
                err_logger->critical("unsupported lua type for key: {0}", lua_type_to_string(type));
                abort();
            }
        }
        abort();
    }

#if defined(VERBOSE_LOGGING)
#define lua_fvalue_params lua_State *L, int index, const std::string &path, int depth
#else
#define lua_fvalue_params lua_State *L, int index
#endif    

    static void cb(lua_State* L, void* obj_)
    {
        FObject* obj = (FObject*)obj_;
        std::string key = lua_key(L, -2);

#if defined(VERBOSE_LOGGING)
        for (int indent = 0; indent < depth; indent++)
            fprintf(log_, "  ");

        verbose_log(log_, "loading %s.%s", path.c_str(), key.c_str());
        FValue value = lua_fvalue(-1, path + "." + key, depth + 1);
#else
        FValue value = lua_fvalue(L, -1);
#endif

        obj->children.emplace_back(key, value);

    }

    static FValue lua_fvalue(lua_fvalue_params)
    {
        int type = lua_type(L, index);

        switch (type)
        {
            case LUA_TSTRING:
            {
                //verbose_log(log_, " [string]\n");
                return std::string(lua_tostring(L, index));
            }
            case LUA_TNUMBER:
            {
                int isnum;
                if (lua_Integer d = lua_tointegerx(L, index, &isnum); isnum)
                {
                    //verbose_log(log_, " [num/int]\n");
                    return uint64_t(d);
                }
                else
                {
                    //fprintf(log_, " [num/double]\n");
                    return double(lua_tonumber(L, index));
                }
            }
            case LUA_TBOOLEAN:
            {
                //verbose_log(log_, " [bool]\n");
                return bool(lua_toboolean(L, index));
                break;
            }
            case LUA_TTABLE:
            {
                //verbose_log(log_, " [table]\n");

                FObject* obj = new FObject();
                int i = 0;

                // Currently only 5-10% faster, will do more.
#define USE_FOREACH 0
#if USE_FOREACH
                lua_foreach(L, -1, obj, cb);
#else
                lua_pushnil(L);
                while (lua_next(L, -2) != 0)
                {
                    std::string key = lua_key(L, -2);

#if defined(VERBOSE_LOGGING)
                    for (int indent = 0; indent < depth; indent++)
                        fprintf(log_, "  ");

                    //verbose_log(log_, "loading %s.%s", path.c_str(), key.c_str());
                    FValue value = lua_fvalue(-1, path + "." + key, depth + 1);
#else
                    FValue value = lua_fvalue(L, -1);
#endif

                    obj->children.emplace_back(key, value);
                    lua_pop(L, 1);
                    i++;
                }
#endif
                obj->sort();
                return obj;
            }
            default:
            {
                abort();
            }
        }
        abort();
    }

    FObject* get_data_raw()
    {
        prof get_data;

        lua_getglobal(L, "data"); //1
        lua_getfield(L, 1, "raw"); //2

        get_data.start();

#if defined(VERBOSE_LOGGING)
        FValue value = lua_fvalue(-1, "data.raw", 0);
        fflush(log_);
#else
        FValue value = lua_fvalue(L, -1);
#endif
        get_data.stop();
        get_data.print("convert data.raw");
        return *value.as<FObject*>();
    }


    using cpp_lua_call = int (VM::*)(void);
    template<cpp_lua_call member_call>
    static int c_bridge(lua_State* L)
    {
        VM* vm = (VM*)lua_getuserdata(L);
        return (vm->*member_call)();
    }

    int log()
    {
        //use this->L here
        return 0;
    }


    ~VM()
    {
        lua_close(L);
    }

    void load_file(const fs::path& p)
    {
        std::string str = normalize(p);
        // printf("    loading: '%s' ... ", str.c_str());
        int err = luaL_loadfile(L, str.c_str());
        if (err)
        {
            err_logger->critical("ERROR:\n**{0}\n", lua_tostring(L, -1));
        }
        anal(err);
        // printf("success\n");
    }

    void call_file(const fs::path& p)
    {
        load_file(p);
        lua_call(L, 0, 1);
        // std::cerr << "executed " << p << "!\n";
    }

    std::string str()
    {
        size_t len;
        const char* buffer = lua_getstring(L, -1, &len);
        return std::string(buffer, len);
    }


    void emplace_global(const std::string& name, lua_CFunction func)
    {
        lua_pushcfunction(L, func);
        lua_setglobal(L, name.c_str());

    }

    bool check_path(const std::string& label, const fs::path& from, const fs::path& request, std::string& out)
    {
        auto path = from / request;
        // std::cerr << "trying #" << label << " -> " << path << "\n";
        if (fs::exists(path))
        {
            // std::cerr << "success\n";
            out = normalize(path);
            return true;
        }
        else
        {
            // std::cerr << "not found\n";
            return false;
        }

    }

    static int c_require(lua_State* L)
    {
        VM* vm = (VM*)lua_getuserdata(L);
        return vm->require();
    }

    void resolve()
    {

    }
    int require()
    {
        // take a copy so we can turn . -> /
        std::string path = str();
        // std::cout << "require: <" << path << ">\n";

        constexpr int package = 2;
        constexpr int loaded = 3;

        lua_getglobal(L, "package");
        lua_getfield(L, package, "loaded");
        lua_getfield(L, loaded, path.c_str());
        if (lua_isnil(L, -1))
        {
            // std::cerr << "not loaded yet, loading...\n";

            std::string actual_path;

            lua_Debug ar;
            if (lua_getstack(L, 1, &ar)) {
                lua_getinfo(L, "Sl", &ar);  /* get info about it */
            }

            for (auto& ch : path)
            {
                if (ch == '.') ch = '/';
            }

            if (path[0] == '_')
            {
                // std::cerr << "mod-relative path help\n";
                exit(-1);
            }

            fs::path requested = path;
            requested.replace_extension(".lua");
            requested.make_preferred();

            fs::path current = ar.source + 1;

            fs::path current_dir = current.parent_path();
            fs::path mod_dir = script_path_to_mod_path[normalize(current)];

            if (check_path("current_dir", current_dir, requested, actual_path)) goto load;
            if (check_path("mod_root", mod_dir, requested, actual_path)) goto load;
            if (check_path("core lualib", corelib / "lualib", requested, actual_path)) goto load;

        load:

            load_file(actual_path);
            //call it
            lua_call(L, 0, 1);
            // dup it
            lua_pushvalue(L, -1);
            //package.loaded[path] = module
            lua_setfield(L, loaded, path.c_str());
        }
        else
        {
            // std::cerr << "returning previously loaded module\n";
        }

        return 1;
    }



    operator lua_State* () {
        return L;
    }

    fs::path resolve_mod_path(const std::string& raw)
    {
        fs::path path = raw;
        auto it = path.begin();
        auto root = ws2s(it->wstring());
        it++;
        root = root.substr(2, root.length() - 4);

        fs::path actual_base = mod_name_to_mod[root]->path;
        for (; it != path.end(); it++)
        {
            actual_base /= *it;
        }

        return actual_base;
    }
};
