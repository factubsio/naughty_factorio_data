#include <JSON.h>
#include <bytell_hash_map.hpp>
#include <chrono>
#include <codecvt>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <lauxlib.h>
#include <locale>
#include <lua.h>
#include <lualib.h>
#include <nana/gui.hpp>
#include <nana/gui/widgets/group.hpp>
#include <nana/gui/widgets/button.hpp>
#include <nana/gui/widgets/label.hpp>
#include <nana/gui/widgets/listbox.hpp>
#include <nana/gui/widgets/picture.hpp>
#include <nana/gui/widgets/textbox.hpp>
#include <nana/gui/widgets/treebox.hpp>
#include <regex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>
#include <zip.h>

#ifdef __linux__
#include <unistd.h>
#define _getcwd getcwd
#else
#include <direct.h>
#endif


namespace fs = std::filesystem;

template<typename T>
struct identity {
    const T &ref;
    operator const T() const { return ref; }
    const T *operator->() const { return &ref; }
    identity(const T&ref) : ref(ref) {}
};

struct FObject;
using FValue = std::variant<std::monostate, FObject *, std::string, uint64_t, double, bool>;

double to_double(const FValue &value)
{
    if (auto num = std::get_if<double>(&value); num)
    {
        return *num;
    }
        
    return double(std::get<uint64_t>(value));
}


FILE *log_ = nullptr;

struct FKeyValue
{
    std::string key;
    FValue value;

    FKeyValue() {}
    FKeyValue(const std::string &key, const FValue &&value) : key(key), value(value) {}
    FKeyValue(const std::string &key, const FValue &value) : key(key), value(value) {}

    FObject &table() { return *std::get<FObject *>(value); }
};

FValue nil;

struct FObject
{
    enum class visit_result { DESCEND, CONTINUE, EXIT, };
    static FObject empty_object;
    std::string type;

    std::vector<FKeyValue> children;
    ska::bytell_hash_map<std::string, int> name_to_child;

    const FValue &child(const std::string &key) const {
        if (auto it = name_to_child.find(key); it != name_to_child.end())
        {
            return children.at(it->second).value;
        }
        else
        {
            return nil;
        }
    }
    FValue &child(const std::string &key) {
        if (auto it = name_to_child.find(key); it != name_to_child.end())
        {
            return children.at(it->second).value;
        }
        else
        {
            return nil;
        }
    }

    FObject &table(const std::string &key) {
        if (auto it = name_to_child.find(key); it != name_to_child.end())
        {
            return children.at(it->second).table();
        }
        else
        {
            return empty_object;
        }
    }

    void sort()
    {
        std::sort(children.begin(), children.end(), [](FKeyValue &a, FKeyValue &b) { return a.key < b.key; });
        for (size_t i = 0; i < children.size(); i++)
        {
            name_to_child[children[i].key] = int(i);
        }

    }

    template<typename T>
    void visit(T callback)
    {
        for (auto &key : children)
        {
            if (auto object = std::get_if<FObject *>(&key.value); object)
            {
                if (callback(1, key) == FObject::visit_result::DESCEND)
                {
                    (*object)->visit(callback);
                    callback(-1, key);
                }
            }
            else
            {
                callback(0, key);
            }
        }
    }


};
FObject FObject::empty_object;

std::wstring s2ws(const std::string& str)
{
    using convert_typeX = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_typeX, wchar_t> converterX;

    return converterX.from_bytes(str);
}

std::string ws2s(const std::wstring& wstr)
{
    using convert_typeX = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_typeX, wchar_t> converterX;

    return converterX.to_bytes(wstr);
}

/*
** stripped-down 'require'. Calls 'openf' to open a module,
** registers the result in 'package.loaded' table and, if 'glb'
** is true, also registers the result in the global table.
** Leaves resulting module on the top.
*/

#if 0
LUALIB_API void luaL_requiref (lua_State *L, const char *modname,
                               lua_CFunction openf, int glb) {
  lua_pushcfunction(L, openf);
  lua_pushstring(L, modname);  /* argument to open function */
  lua_call(L, 1, 1);  /* open module */
  luaL_getsubtable(L, LUA_REGISTRYINDEX, "_LOADED");
  lua_pushvalue(L, -2);  /* make copy of module (call result) */
  lua_setfield(L, -2, modname);  /* _LOADED[modname] = module */
  lua_pop(L, 1);  /* remove _LOADED table */
  if (glb) {
    lua_pushvalue(L, -1);  /* copy of 'mod' */
    lua_setglobal(L, modname);  /* _G[modname] = module */
  }
}
#endif


std::chrono::time_point<std::chrono::steady_clock> last_updated;

void perror_l(int err)
{
    switch (err)
    {
        case LUA_OK: std::cerr << " *** lua internal error: LUA_OK\n"; break;
        case LUA_YIELD: std::cerr << " *** lua internal error: LUA_YIELD\n"; break;
        case LUA_ERRRUN: std::cerr << " *** lua internal error: LUA_ERRRUN\n"; break;
        case LUA_ERRSYNTAX: std::cerr << " *** lua internal error: LUA_ERRSYNTAX\n"; break;
        case LUA_ERRMEM: std::cerr << " *** lua internal error: LUA_ERRMEM\n"; break;
        case LUA_ERRGCMM: std::cerr << " *** lua internal error: LUA_ERRGCMM\n"; break;
        case LUA_ERRERR: std::cerr << " *** lua internal error: LUA_ERRERR\n"; break;
        default: std::cerr << " *** lua internal error: UNKNOWN\n"; break;
    }
}

void anal(int err)
{
    if (err != LUA_OK)
    {
        perror_l(err);
        exit(-1);
    }
}
typedef int (*lua_CFunction) (lua_State *L);




// #if 0
// LUALIB_API void luaL_requiref (lua_State *L, const char *modname,
//                                lua_CFunction openf, int glb) {
//   lua_pushcfunction(L, openf);
//   lua_pushstring(L, modname);  /* argument to open function */
//   lua_call(L, 1, 1);  /* open module */
//   luaL_getsubtable(L, LUA_REGISTRYINDEX, "_LOADED");
//   lua_pushvalue(L, -2);  /* make copy of module (call result) */
//   lua_setfield(L, -2, modname);  /* _LOADED[modname] = module */
//   lua_pop(L, 1);  /* remove _LOADED table */
//   if (glb) {
//     lua_pushvalue(L, -1);  /* copy of 'mod' */
//     lua_setglobal(L, modname);  /* _G[modname] = module */
//   }
// }
// #endif

static void *l_alloc (void *ud, void *ptr, size_t osize, size_t nsize) {
  (void)ud; (void)osize;  /* not used */
  if (nsize == 0) {
    free(ptr);
    return NULL;
  }
  else
    return realloc(ptr, nsize);
}

std::vector<char> load_file_contents(std::string const& filepath)
{
    std::ifstream ifs(filepath, std::ios::binary|std::ios::ate);

    if(!ifs)
        throw std::runtime_error(filepath + ": " + std::strerror(errno));

    auto end = ifs.tellg();
    ifs.seekg(0, std::ios::beg);

    auto size = std::size_t(end - ifs.tellg());

    if(size == 0) // avoid undefined behavior
        return {};

    std::vector<char> buffer(size);

    if(!ifs.read((char*)buffer.data(), buffer.size()))
        throw std::runtime_error(filepath + ": " + std::strerror(errno));

    return buffer;
}
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
    std::unordered_map<std::string, Dependency::Type> depTypes = {
        {"", Dependency::Type::required},
        {"?", Dependency::Type::optional},
        {"(?)", Dependency::Type::optional_hidden},
        {"!", Dependency::Type::forbidden},

    };


    struct Mod
    {
        std::string name;
        fs::path path;
        std::vector<Dependency> declared_dependencies;
        std::vector<Mod *> dependencies;
        int loaded = -1;
    };


std::string normalize(const fs::path &path)
{
    return ws2s(fs::canonical(path).wstring());
}
std::string weakly_normalize(const fs::path &path)
{
    return ws2s(fs::weakly_canonical(path).wstring());
}


#define STR_(x) #x
#define STR(x) STR_(x)

struct VM
{
    lua_State *L = nullptr;

    const fs::path game_dir = STR(FACTORIOPATH);
    const fs::path corelib = game_dir / "data" / "core";
    const fs::path baselib = game_dir / "data" / "base";
    const fs::path mod_dir = game_dir / "mods";
    const fs::path cwd = _getcwd(0, 0);

    std::unordered_map<std::string, fs::path> script_path_to_mod_path;
    std::unordered_map<std::string, Mod *> mod_name_to_mod;
    std::vector<Mod *> modlist;

    void iterate_mod(fs::path dir)
    {
        for (auto &p : fs::recursive_directory_iterator(dir))
        {
            if (p.is_regular_file() && p.path().extension() == ".lua")
            {
                std::string key = normalize(p.path().wstring());
                script_path_to_mod_path.emplace(key, dir);
            }
        }
    }


    template<typename T>
    void load_mods(std::vector<Mod *> &mods, int stage, T func)
    {
        for (Mod *mod : mods)
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

    void discover_mods()
    {
        for (auto &p : fs::directory_iterator(mod_dir))
        {
            if (p.is_directory()) {
                const auto& info_path = p.path()/"info.json";
                if (fs::exists(info_path))
                {
                    const auto &info_contents_raw = load_file_contents(ws2s(info_path.wstring()));
                    std::string info_contents(info_contents_raw.data(), info_contents_raw.size());
                    JSONValue *value = JSON::Parse(info_contents.c_str());
                    if (!value)
                    {
                        std::cerr << "Could not load: " << info_path << "\n";
                        exit(-1);
                    }

                    JSONObject root = value->AsObject();
                    JSONArray deplist = root[L"dependencies"]->AsArray();

                    Mod *mod = new Mod();
                    mod->name = ws2s(root[L"name"]->AsString());
                    mod->path = p.path();
                    mod_name_to_mod[mod->name] = mod;

                    modlist.push_back(mod);

                    for (auto dep : deplist)
                    {
                        std::string depval = ws2s(dep->AsString());
                        std::smatch m;
                        // std::cout << "matching: '" << depval << "'\n";
                        if (std::regex_match(depval, m, dep_pattern))
                        {
                            // std::cout << "matched type: " << m[1] << "name: " << m[2] << "\n";
                            Dependency dep;
                            dep.modName = m[2];
                            dep.type = depTypes[m[1]];
                            mod->declared_dependencies.push_back(dep);
                        }
                        else
                        {
                            // std::cout << "couldn't match\n";
                        }
                    }

                    iterate_mod(p);
                }
            }
            // auto current = mods.find(p);
        }
    }

    VM()
    {
        log_ = fopen("C:/tmp/log.txt", "w");
        L = lua_newstate(l_alloc, this);
        const fs::path lualib = game_dir / "data" / "core" / "lualib";
        std::cout << "lualib: loading\n";



        {
            Mod *core = new Mod();
            core->name = "core";
            core->path = corelib;
            iterate_mod(corelib);
            mod_name_to_mod[core->name] = core;
        }

        {
            Mod *base = new Mod();
            base->name = "base";
            base->path = baselib;
            iterate_mod(baselib);
            mod_name_to_mod[base->name] = base;
        }

        discover_mods();
        for (Mod *mod : modlist)
        {
            std::cout << "detetced mod: " << mod->name << ", checking dependencies: ";
            for (auto &dep : mod->declared_dependencies)
            {
                Mod *depmod = mod_name_to_mod[dep.modName];
                if (depmod)
                {
                    // TODO: Check version and if it's forbidden!
                    mod->dependencies.push_back(depmod);
                }
                else
                {
                    if (dep.type == Dependency::Type::required)
                    {
                        std::cerr << "could not find required dependency: " << dep.modName << " of mod: " << mod->name << "\n";
                        exit(-1);
                    }
                }

            }
            std::cout << "success\n";

            std::sort(mod->dependencies.begin(), mod->dependencies.end(), [](Mod *a, Mod *b) { return a->name < b->name; });
        }
        std::sort(modlist.begin(), modlist.end(), [](Mod *a, Mod *b) { return a->name < b->name; });

        lua_newtable(L); //defines @1
        lua_newtable(L); //packages
        lua_setfield(L, -2, "loaded");
        lua_setglobal(L, "package");

        luaopen_base(L);
        luaL_openlibs(L);

        call_file(cwd / "serpent.lua");
        lua_setglobal(L, "serpent");

        emplace_global("require", c_require);
        emplace_global("log", c_bridge<&VM::log>);
        emplace_global("log_localised", c_bridge<&VM::log>);

        call_file(cwd / "bootstrap.lua");
        lua_pop(L, 1);

        call_file(cwd / "test.lua");
        lua_pop(L, 1);

        call_file(corelib /"lualib"/ "dataloader.lua");
        call_file(corelib /"data.lua");
        call_file(baselib /"data.lua");

        lua_settop(L, 0);
    }


    std::string lua_key(int index)
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
                return lua_toboolean(L, index) ? "true" : "false" ;
                break;
            }
            default:
            {
                abort();
            }
        }
        abort();
    }

    FValue lua_fvalue(int index, const std::string &path, int depth)
    {
        int type = lua_type(L, index);

        switch (type)
        {
            case LUA_TSTRING:
            {
                fprintf(log_, " [string]\n");
                return std::string(lua_tostring(L, index));
            }
            case LUA_TNUMBER:
            {
                int isnum;
                if (lua_Integer d = lua_tointegerx(L, index, &isnum); isnum)
                {
                    fprintf(log_, " [num/int]\n");
                    return uint64_t(d);
                }
                else
                {
                    fprintf(log_, " [num/double]\n");
                    return double(lua_tonumber(L, index));
                }
            }
            case LUA_TBOOLEAN:
            {
                fprintf(log_, " [bool]\n");
                return bool(lua_toboolean(L, index));
                break;
            }
            case LUA_TTABLE:
            {
                fprintf(log_, " [table]\n");

                FObject *obj = new FObject();
                int i = 0;
                lua_pushnil(L); /* first key  (3) */
                while (lua_next(L, -2) != 0)
                {
                    std::string key = lua_key(-2);
                    for (int indent = 0; indent < depth; indent++)
                        fprintf(log_, "  ");
                    fprintf(log_, "loading %s.%s", path.c_str(), key.c_str());
                    FValue value = lua_fvalue(-1, path + "." + key, depth + 1);

                    obj->children.emplace_back(key, value);
                    lua_pop(L, 1);
                    i++;
                }
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

    FObject *get_data_raw()
    {
        lua_getglobal(L, "data"); //1
        lua_getfield(L, 1, "raw"); //2

        FValue value = lua_fvalue(-1, "data.raw", 0);

        fflush(log_);
        return std::get<FObject *>(value);
    }


    using cpp_lua_call = int (VM::*)(void);
    template<cpp_lua_call member_call>
    static int c_bridge(lua_State *L)
    {
        VM *vm = (VM *)lua_getuserdata(L);
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

    void load_file(const fs::path &p)
    {
        std::string str = normalize(p);
        // printf("    loading: '%s' ... ", str.c_str());
        int err = luaL_loadfile(L, str.c_str());
        if (err)
        {
            std::cerr << "ERROR:\n" <<  lua_tostring(L, -1) << '\n';
        }
        anal(err);
        // printf("success\n");
    }

    void call_file(const fs::path &p)
    {
        load_file(p);
        lua_call(L, 0, 1);
        // std::cerr << "executed " << p << "!\n";
    }

    std::string str()
    {
        size_t len;
        const char *buffer = lua_getstring(L, -1, &len);
        return std::string(buffer, len);
    }


    void emplace_global(const std::string &name, lua_CFunction func)
    {
        lua_pushcfunction(L, func);
        lua_setglobal(L, name.c_str());

    }

    bool check_path(const std::string &label, const fs::path &from, const fs::path &request, std::string &out)
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

    static int c_require(lua_State *L)
    {
        VM *vm = (VM *)lua_getuserdata(L);
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

            for (auto &ch : path)
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
            if (check_path("core lualib", corelib/"lualib", requested, actual_path)) goto load;

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



    operator lua_State *() {
        return L;
    }

    fs::path resolve_mod_path(const std::string &raw)
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


VM vm;


nana::form *win;

struct factorio
{
    struct data
    {
        using string = std::string;
        using LocalisedString = string;
        using Order = string;

        using Energy = string; //bleh

        struct Icon
        {
            fs::path file_path;
            Icon() = default;
            Icon(const FObject &obj)
            {
                std::string path = std::get<std::string>(obj.child("icon"));
                file_path = vm.resolve_mod_path(path);
            }
        };

        struct Color {
            double r = 0;
            double g = 0;
            double b = 0;
            double a = 1;
        };

        template<typename T, int min, int max>
        struct array_opt
        {
            int count = 0;
            T arr[max];
        };

        #define ld(x, obj) parse_fval(x, obj.child(#x))

        struct PrototypeBase
        {
            PrototypeBase(const FObject &obj);
            string name;
            string type;
            LocalisedString localised_description;
            LocalisedString localised_name;
            Order order;
        };

        struct TileEffectPrototype
        {
            TileEffectPrototype(const FObject &obj);
            array_opt<double, 1, 2> animation_scale;
            double animation_speed;
            array_opt<double, 1, 2> dark_threshold;
            Color foam_color;
            double foam_color_multiplier;
            string name;
            array_opt<double, 1, 2> reflection_threshold;
            Color specular_lightness;
            array_opt<double, 1, 2> specular_threshold;
            // texture ::Sprite
            double tick_scale;
            string type;
            double far_zoom;
            double near_zoom;
        };

        struct ItemPrototype : PrototypeBase
        {
            ItemPrototype(const FObject &obj);
            Icon icon;
            // icons, icon, icon_size (IconSpecification)	::	IconSpecification
            uint32_t stack_size;
            string burnt_result;
            uint32_t default_request_amount;
            // dark_background_icons, dark_background_icon, icon_size (IconSpecification)	::	IconSpecification (optional)
            // flags	::	ItemPrototypeFlags (optional)
            double fuel_acceleration_multiplier;
            string fuel_category;
            double fuel_emissions_multiplier;
            Color fuel_glow_color;
            double fuel_top_speed_multiplier;
            Energy fuel_value;
            // pictures	::	SpriteVariations (optional)
            // place_as_tile	::	PlaceAsTile (optional)
            string place_result;
            string placed_as_equipment_result;
            // rocket_launch_product	::	ItemProductPrototype (optional)
            // rocket_launch_products	::	table (array) of ItemProductPrototype (optional)
            string subgroup;
            uint32_t wire_count;
        };
    };
};



template<typename T>
T parse_fval(T& out, const FValue &value);

template<>
std::string parse_fval(std::string &out, const FValue &value)
{
    if (auto str = std::get_if<std::string>(&value); str)
    {
        out = *str;
    }
    else
    {
        out = "!<>";
    }
    return out;
}

template<>
uint32_t parse_fval(uint32_t &out, const FValue &value)
{
    if (auto num = std::get_if<uint64_t>(&value); num)
    {
        out = uint32_t(*num);
    }
    else
    {
        out = -1;
    }
    return out;
}

template<>
double parse_fval(double &out, const FValue &value)
{
    if (auto num = std::get_if<double>(&value); num)
    {
        out = double(*num);
    }
    else
    {
        out = -1;
    }
    return out;
}


template<>
factorio::data::Color parse_fval(factorio::data::Color &out, const FValue &value)
{
    // reset to default
    out = factorio::data::Color();
    if (auto arr_ptr = std::get_if<FObject *>(&value); arr_ptr)
    {
        auto array = *arr_ptr;
        if (auto index1 = array->child("1"); index1.index())
        {
            out.r = to_double(index1);
            out.g = to_double(array->child("2"));
            out.b = to_double(array->child("3"));

            if (auto index3 = array->child("3"); index3.index())
            {
                out.a = to_double(index3);
            }
        }
        else
        {
            if (auto x = array->child("r"); x.index()) out.r = to_double(x);
            if (auto x = array->child("g"); x.index()) out.g = to_double(x);
            if (auto x = array->child("b"); x.index()) out.b = to_double(x);
            if (auto x = array->child("a"); x.index()) out.a = to_double(x);
        }
    }
    else
    {
        out = {-1, -1, -1, -1};
    }
    return out;
}

template<int min, int max>
factorio::data::array_opt<double, min, max> parse_fval(factorio::data::array_opt<double, min, max> &out, const FValue &value)
{
    out.count = 0;
    if (auto arr_ptr = std::get_if<FObject *>(&value); arr_ptr)
    {
        auto array = *arr_ptr;
        for (const auto &kv : array->children)
        {
            parse_fval(out.arr[out.count], kv.value);
            out.count++;
            if (out.count > max)
                abort(); //THROW AN ERROR YOU NINNY
        }
        if (out.count < min)
            abort(); //THROW AN ERROR YOU NINNY
    }
    else
    {
        // could be optional so let upstream handle it
    }
    return out;
}

template<typename T>
T parse_fval(const FValue &value)
{
    T out;
    return parse_fval(out, value);
}

std::string to_string(const FValue &value)
{

    if (auto str = std::get_if<std::string>(&value); str)
    {
        return *str;
    }
    if (auto num = std::get_if<uint64_t>(&value); num)
    {
        return std::to_string(*num);
    }
    if (auto num = std::get_if<double>(&value); num)
    {
        return std::to_string(*num);
    }
    if (auto b = std::get_if<bool>(&value); b)
    {
        return (*b)? "true" : "false";
    }
    return "!<>";
}

factorio::data::TileEffectPrototype::TileEffectPrototype(const FObject &obj)
{
    ld(animation_scale, obj);
    ld(animation_speed, obj);
    ld(dark_threshold, obj);
    ld(foam_color, obj);
    ld(foam_color_multiplier, obj);
    ld(name, obj);
    ld(reflection_threshold, obj);
    ld(specular_lightness, obj);
    ld(specular_threshold, obj);
    // ld(texture, obj);
    ld(tick_scale, obj);
    ld(type, obj);
    ld(far_zoom, obj);
    ld(near_zoom, obj);
}


factorio::data::ItemPrototype::ItemPrototype(const FObject &obj) : PrototypeBase(obj)
{

    ld(fuel_acceleration_multiplier, obj);
    ld(fuel_category, obj);
    ld(fuel_emissions_multiplier, obj);
    ld(fuel_glow_color, obj);
    ld(fuel_top_speed_multiplier, obj);
    ld(fuel_value, obj);

    icon = Icon(obj);
}

factorio::data::PrototypeBase::PrototypeBase(const FObject &obj)
{
    ld(name, obj);
    ld(type, obj);
    ld(localised_description, obj);
    ld(localised_name, obj);
    ld(order, obj);
}

static void unhide_recursive_up(nana::treebox::item_proxy node)
{
    node.hide(false);
    node.expand(true);
    if (node.level())
        unhide_recursive_up(node.owner());
}
static void unhide_recursive_down(nana::treebox::item_proxy node)
{
    if (node.level() >= 4) return;

    node.hide(false);
    for (auto ch : node)
        unhide_recursive_down(ch);
}

static void apply_filter(const std::string &filter, nana::treebox::item_proxy node)
{
    // Don't filter too far down?
    if (node.level() >= 4) return;

    node.hide(true);

    // check if this node matches
    if (filter.length() == 0 || node.key().find(filter) != std::string::npos)
    {
        unhide_recursive_up(node);
        unhide_recursive_down(node);
    }
    // or if our children do
    else
    {
        for (auto ch : node)
        {
            apply_filter(filter, ch);
        }
    }
}

static nana::group *editor;
static nana::picture *sprite_preview = nullptr;
static nana::timer update_filtering;
static nana::treebox *data_raw_tree = nullptr;
static nana::textbox *search = nullptr;
static bool filter_pending = false;
static const std::chrono::milliseconds filter_throttle_interval_ms(500);

static void do_filtering(const std::chrono::steady_clock::time_point &now)
{
    fprintf(stderr, "filtering\n");

    std::string filter = search->getline(0).value();
    auto root = data_raw_tree->find("raw");

    data_raw_tree->auto_draw(false);
    apply_filter(filter, root);
    data_raw_tree->auto_draw(true);
    last_updated = now;
    filter_pending = false;
    update_filtering.stop();

    auto end = std::chrono::steady_clock::now();
    fprintf(stderr, "elapsed: %ldms\n", std::chrono::duration_cast<std::chrono::milliseconds>(end - now).count());
}

static void trigger_do_filtering()
{
    printf("trigger\n");
    auto now = std::chrono::steady_clock::now();
    do_filtering(now);
}


    struct editor_layout
    {
        std::vector<std::string> heights;
        std::string default_height = "30";
        std::string icon_height = "200";

        void render_column(std::string &out, const std::string &name)
        {
            out += "<vert arrange=[" + heights.front();
            for (auto it = heights.begin() + 1; it != heights.end(); it++)
            {
                out += "," + *it;
            }
            out += "] ";
            out += name;
            out += ">";
        }

        std::string render()
        {
            std::string out;
            render_column(out, "key_");
            render_column(out, "value_");
            return out;
        }

        nana::group *group;

        editor_layout(nana::group *parent)
        {
            group = new nana::group(static_cast<nana::window>(*parent));
            (*parent)["sections"] << *group;
        }

        void row(const std::string &key, const std::string &height)
        {
            heights.emplace_back(default_height);
            group->create_child<nana::label>("key_")->caption(key);
        }

        template<typename T>
        void data(const identity<T> &value)
        {
            group->create_child<nana::label>("value_")->caption(std::to_string(value));
        }
        void data(const identity<factorio::data::string> &value)
        {
            group->create_child<nana::label>("value_")->caption(value);
        }
        void data(const identity<factorio::data::Color> &value)
        {
            nana::color color(value->r, value->g, value->b, value->a);
            group->create_child<nana::button>("value_")->bgcolor(color);
        }

        template<typename T>
        void normal(const std::string &key, const T &value)
        {
            row(key, default_height);
            data(identity<T>(value));
        }


        // void normal(const std::string &key, double value)
        // {
        //     heights.emplace_back(default_height);

        //     group->create_child<nana::label>("key_")->caption(key);
        //     group->create_child<nana::label>("value_")->caption(std::to_string(value));
        // }

        void icon(const std::string &key, nana::paint::image &image)
        {
            row(key, icon_height);
            group->create_child<nana::picture>("value_")->load(image);
        }

        void collocate()
        {
            group->div(render().c_str());
            group->collocate();
        }


    };

int main()
{

    win = new nana::form{nana::API::make_center(1024, 1024), nana::appear::decorate<nana::appear::taskbar>()};

    nana::label label(*win);
    label.caption("Hello");
    label.bgcolor(nana::colors::blue_violet);

    data_raw_tree = new nana::treebox(*win);

    nana::place layout(*win);
    layout.div("< <vert left weight=300<search weight=40><mid>>|<<rightno>>");
    layout["mid"] << *data_raw_tree;

    search = new nana::textbox(*win);
    search->multi_lines(false);
    layout["search"] << *search;

    editor = new nana::group(*win);
    // editor->append_header("name");
    // editor->append_header("value");
    // sprite_preview = new nana::picture(*win);
    // sprite_preview->caption("hello");
    layout["rightno"] << *editor;

    layout.collocate();

    auto root_unfiltered = data_raw_tree->insert("raw", "data.raw");

    search->events().text_changed([&](const nana::arg_textbox &arg) {
        auto now = std::chrono::steady_clock::now();
        auto time_since_last_update = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_updated);

        // throttle updates
        if (time_since_last_update >= filter_throttle_interval_ms)
        {
            // assert on this.
            do_filtering(now);
        }
        else
        {
            fprintf(stderr, "throttling\n");
            if (!filter_pending)
            {
                update_filtering.reset();
                update_filtering.interval(filter_throttle_interval_ms - time_since_last_update);
                update_filtering.elapse(trigger_do_filtering);
                update_filtering.start();
                filter_pending = true;
            }
        }
    });

    search->events().key_press([&](const nana::arg_keyboard &arg) {
        if (arg.ctrl)
        {
            //READLINE!

            // use stdilb.h/wctomb? :thinking:
            if (arg.key == 'L')
            {
                search->select(true);
                search->del();
            }
            arg.stop_propagation();
        }
    });

    ska::bytell_hash_map<std::string, factorio::data::ItemPrototype> items;

    FKeyValue raw("data.raw", vm.get_data_raw());

    std::deque<nana::treebox::item_proxy> visual_stack;
    visual_stack.push_front(root_unfiltered);

    std::string path = "data/raw";

    nana::paint::image preview_image;


    ska::bytell_hash_map<std::string, std::function<void(const std::vector<std::string> &path)>> prototype_factories;
    prototype_factories["data/raw/item"] = [&](const std::vector<std::string> &path) {
        if (path.size() < 4)
        {
            return;
        }
        const std::string &item_name = path[3];

        fprintf(stderr, "LOADING ITEM: %s\n", item_name.c_str());

        auto rawtbl = raw.table();
        auto itemtbl = rawtbl.table("item");
        auto item = itemtbl.table(item_name);

        factorio::data::ItemPrototype proto(item);

        if (editor)
        {
            editor->close();
            delete editor;
        }

        editor = new nana::group(*win);
        editor->div("<vert margin=10 sections>");

        layout["rightno"] << *editor;

        // layout["rightno"].

        editor_layout base(editor);
        base.group->caption("PrototypeBase");

        base.normal("name", proto.name);
        base.normal("type", proto.type);
        base.normal("localised_description", proto.localised_description);
        base.normal("localised_name", proto.localised_name);
        base.normal("order", proto.order);
        base.collocate();

        editor_layout item_group(editor);
        item_group.group->caption("ItemPrototype");

        preview_image.close();
        preview_image.open(proto.icon.file_path);
        item_group.icon("icon", preview_image);

        item_group.normal("fuel_acceleration_multiplier", proto.fuel_acceleration_multiplier);
        item_group.normal("fuel_category", proto.fuel_category);
        item_group.normal("fuel_emissions_multiplier", proto.fuel_emissions_multiplier);
        item_group.normal("fuel_glow_color", proto.fuel_glow_color);
        item_group.normal("fuel_top_speed_multiplier", proto.fuel_top_speed_multiplier);
        item_group.normal("fuel_value", proto.fuel_value);

        item_group.collocate();

        editor->collocate();
        layout.collocate();
    };
    prototype_factories["data/raw/tile-effect"] = [&](const std::vector<std::string> &path) {
        if (path.size() < 4)
        {
            return;
        }
        const std::string &effect_name = path[3];

        fprintf(stderr, "LOADING EFFECT: %s\n", effect_name.c_str());

        auto rawtbl = raw.table();
        auto effecttbl = rawtbl.table("tile-effect");
        auto effect = effecttbl.table(effect_name);

        factorio::data::TileEffectPrototype proto(effect);

        if (editor)
        {
            editor->close();
            delete editor;
        }

        editor = new nana::group(*win);
        editor->div("<vert margin=10 sections>");

        layout["rightno"] << *editor;

        // layout["rightno"].

        editor_layout base(editor);
        base.group->caption("TileEffectPrototype");

        // editor_row(base, proto, animation_scale);
        base.normal("animation_speed", proto.animation_speed);
        // editor_row(base, proto, dark_threshold);
        base.normal("foam_color", proto.foam_color);
        base.normal("foam_color_multiplier", proto.foam_color_multiplier);
        base.normal("name", proto.name);
        // editor_row(base, proto, reflection_threshold);
        base.normal("specular_lightness", proto.specular_lightness);
        // editor_row(base, proto, specular_threshold);
        // editor_row(base, proto, texture);
        base.normal("tick_scale", proto.tick_scale);
        base.normal("type", proto.type);
        base.normal("far_zoom", proto.far_zoom);
        base.normal("near_zoom", proto.near_zoom);

        base.collocate();

        editor->collocate();
        layout.collocate();
    };

    data_raw_tree->events().selected([&](const nana::arg_treebox& arg) {
        // ignore de-selection (could do some cache freeing)
        if (!arg.operated)
        {
            return;
        }

        // take copy so we can chop it in place
        std::string path = arg.item.key();
        fprintf(stderr, " === looking up path: %s\n", path.c_str());

        if (path == "raw")
        {
            return;
        }

        size_t n = 0;
        auto it = prototype_factories.end();
        // shoul dbe strin gview but we are cowboys
        std::vector<std::string> components;
        while (true)
        {
            auto next = path.find('/', n);

            components.emplace_back(path.substr(n, next-n));
            auto key = path.substr(0, next);

            if (it == prototype_factories.end()) it = prototype_factories.find(key);
            if (next == path.npos)
            {
                break;
            }
            n = next + 1;
        }
        if (it != prototype_factories.end())
        {
            it->second(components);

        }
    });

    data_raw_tree->auto_draw(false);
    raw.table().visit([&](int dir, FKeyValue entry)
    {
        FObject::visit_result mode = FObject::visit_result::DESCEND;

        nana::treebox::item_proxy node;
        std::string local_path = path + "/" + entry.key;
        if (dir >= 0)
        {

            std::string label = dir == 0 ? (entry.key + ": " + to_string(entry.value)) : entry.key;
            node = data_raw_tree->insert(visual_stack.front(), local_path, label);
        }
        if (dir > 0)
        {
            if (visual_stack.size() > 1)
            {
                mode = FObject::visit_result::CONTINUE;
            }
            else
            {
                path = local_path;
                visual_stack.push_front(node);
            }
        }
        else if (dir < 0)
        {
            path.resize(path.rfind('/'), 0);
            visual_stack.pop_front();
        }

        return mode;
    });
    data_raw_tree->auto_draw(true);

    win->show();
    nana::exec();

    update_filtering.reset();
    delete editor;
    delete search;
    delete data_raw_tree;
    delete win;
    // lua_getglobal(L, "data");
    // std::cout << lua_tostring(L, -1);

    // std::cout << "Hello" << "\n";
}
