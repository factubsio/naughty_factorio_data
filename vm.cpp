#include "vm.hpp"

static void perror_l(int err)
{
    switch (err)
    {
        case LUA_OK: err_logger->critical("*** lua internal error: LUA_OK\n"); break;
        case LUA_YIELD: err_logger->critical(" *** lua internal error: LUA_YIELD\n"); break;
        case LUA_ERRRUN: err_logger->critical(" *** lua internal error: LUA_ERRRUN\n"); break;
        case LUA_ERRSYNTAX: err_logger->critical(" *** lua internal error: LUA_ERRSYNTAX\n"); break;
        case LUA_ERRMEM: err_logger->critical(" *** lua internal error: LUA_ERRMEM\n"); break;
        case LUA_ERRGCMM: err_logger->critical(" *** lua internal error: LUA_ERRGCMM\n"); break;
        case LUA_ERRERR: err_logger->critical(" *** lua internal error: LUA_ERRERR\n"); break;
        default: err_logger->critical(" *** lua internal error: UNKNOWN\n"); break;
    }
}

static std::unordered_map<std::string, Dependency::Type> depTypes = {
    {"", Dependency::Type::required},
    {"?", Dependency::Type::optional},
    {"(?)", Dependency::Type::optional_hidden},
    {"!", Dependency::Type::forbidden},
};


void anal(int err)
{
    if (err != LUA_OK)
    {
        perror_l(err);
        exit(-1);
    }
}

void VM::discover_mods()
{
    for (auto& p : fs::directory_iterator(mod_dir))
    {
        if (p.is_directory()) {
            const auto& info_path = p.path() / "info.json";
            if (fs::exists(info_path))
            {
                const auto& info_contents_raw = load_file_contents(ws2s(info_path.wstring()));
                std::string info_contents(info_contents_raw.data(), info_contents_raw.size());
                JSONValue* value = JSON::Parse(info_contents.c_str());
                if (!value)
                {
                    //std::cerr << "Could not load: " << info_path << "\n";
                    exit(-1);
                }

                JSONObject root = value->AsObject();
                JSONArray deplist = root[L"dependencies"]->AsArray();

                Mod* mod = new Mod();
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


static void* l_alloc(void* ud, void* ptr, size_t osize, size_t nsize) {
    (void)ud; (void)osize;  /* not used */
    if (nsize == 0) {
        free(ptr);
        return NULL;
    }
    else
        return realloc(ptr, nsize);
}

VM::VM(const fs::path &game_dir) :
    game_dir(game_dir),
    corelib(game_dir / "data" / "core"),
    baselib(game_dir / "data" / "base"),
    mod_dir(game_dir / "mods"),
    cwd(_getcwd(0, 0))
{

#if defined(VERBOSE_LOGGING)
    std::string logpath = ws2s(fs::temp_directory_path() / "log.txt");
    log_ = fopen(logpath.c_str(), "w");
#endif
    L = lua_newstate(l_alloc, this);
    const fs::path lualib = game_dir / "data" / "core" / "lualib";
    //std::cout << "lualib: loading\n";

    {
        Mod* core = new Mod();
        core->name = "core";
        core->path = corelib;
        iterate_mod(corelib);
        mod_name_to_mod[core->name] = core;
    }

    {
        Mod* base = new Mod();
        base->name = "base";
        base->path = baselib;
        iterate_mod(baselib);
        mod_name_to_mod[base->name] = base;
    }

    discover_mods();
    for (Mod* mod : modlist)
    {
        //std::cout << "detected mod: " << mod->name << ", checking dependencies: ";
        for (auto& dep : mod->declared_dependencies)
        {
            Mod* depmod = mod_name_to_mod[dep.modName];
            if (depmod)
            {
                // TODO: Check version and if it's forbidden!
                mod->dependencies.push_back(depmod);
            }
            else
            {
                if (dep.type == Dependency::Type::required)
                {
                    //std::cerr << "could not find required dependency: " << dep.modName << " of mod: " << mod->name << "\n";
                    exit(-1);
                }
            }

        }
        //std::cout << "success\n";

        std::sort(mod->dependencies.begin(), mod->dependencies.end(), [](Mod* a, Mod* b) { return a->name < b->name; });
    }
    std::sort(modlist.begin(), modlist.end(), [](Mod* a, Mod* b) { return a->name < b->name; });

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

}

void VM::run_data_stage()
{
    call_file(corelib / "lualib" / "dataloader.lua");
    call_file(corelib / "data.lua");
    call_file(baselib / "data.lua");

    lua_settop(L, 0);
}
