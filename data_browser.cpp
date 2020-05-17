#include <chrono>
#include <codecvt>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <locale>
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
#include <cstdint>
#include <cinttypes>

#include "fmt/format.h"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

auto err_logger = spdlog::stderr_color_mt("stderr");

#include "util.hpp"
#include "fobject.hpp"
#include "vm.hpp"
#include "factorio_data.hpp"

void prof::print(const std::string& name)
{
    fprintf(stderr, "elapsed (%s): %" PRId64 "ms\n", name.c_str(), int64_t(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed()).count()));
}


#if defined(VERBOSE_LOGGING)
#define verbose_log fprintf
#else
#define verbose_log
#endif

namespace fs = std::filesystem;


FILE* log_ = nullptr;

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

std::chrono::time_point<std::chrono::steady_clock> last_updated;


std::vector<char> load_file_contents(std::string const& filepath)
{
    std::ifstream ifs(filepath, std::ios::binary | std::ios::ate);

    if (!ifs)
        throw std::runtime_error(filepath + ": " + std::strerror(errno));

    auto end = ifs.tellg();
    ifs.seekg(0, std::ios::beg);

    auto size = std::size_t(end - ifs.tellg());

    if (size == 0) // avoid undefined behavior
        return {};

    std::vector<char> buffer(size);

    if (!ifs.read((char*)buffer.data(), buffer.size()))
        throw std::runtime_error(filepath + ": " + std::strerror(errno));

    return buffer;
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

static void apply_filter(const std::string& filter, nana::treebox::item_proxy node)
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

static const std::chrono::milliseconds filter_throttle_interval_ms(50);


//Base class for a value_editor, its job is to sit in a vector and hold references to widgets to they can be cleaned up on destruction
//We also call bind() on this when a new prototype is selected
struct value_editor
{
    std::unique_ptr<nana::widget> label;
    std::unique_ptr<nana::widget> editor;

    value_editor() = default;
    value_editor(nana::widget* label, nana::widget *editor) : label(label), editor(editor) {}

    virtual void bind(void *obj) = 0;
};

//There should be no default impl of this, unless we just assume we can call std::to_string on Binder->Binding :thinkingface:
template<typename T, typename Binder, typename Binding>
struct value_editor_bound : value_editor { };

template<typename Binder, typename Binding>
struct value_editor_bound<double, Binder, Binding> : value_editor
{
    Binding binding;
    value_editor_bound(Binding binding, nana::widget* label, nana::widget* editor) : value_editor(label, editor), binding(binding)
    {
    }

    void bind(void *obj) override {
        nana::textbox* double_editor = static_cast<nana::textbox*>(editor.get());
        Binder* b = static_cast<Binder*>(obj);
        double_editor->caption(std::to_string(b->*binding));
    }
};


template<typename Binder, typename Binding>
struct value_editor_bound<factorio::data::string, Binder, Binding> : value_editor
{
    Binding binding;
    value_editor_bound(Binding binding, nana::widget* label, nana::widget* editor) : value_editor(label, editor), binding(binding)
    {
    }

    void bind(void *obj) override {
        nana::textbox* double_editor = static_cast<nana::textbox*>(editor.get());
        Binder* b = static_cast<Binder*>(obj);
        double_editor->caption(b->*binding);
    }
};


//Pretty much just a list of value_editors
struct block_editor
{
    std::unique_ptr<nana::group> root;
    std::vector<std::unique_ptr<nana::group>> sections;

    std::unordered_map<std::string, std::unique_ptr<value_editor>> value_editors;

    block_editor(nana::window win)
    {
        root = std::make_unique<nana::group>(win);
        root->div("<vert sections>");
    }

    template<typename T, typename Binder, typename Binding>
    void register_value_editor(const std::string& key, Binding binding, nana::widget* label_widget, nana::widget* editor_widget)
    {
        value_editor *editor = new value_editor_bound<T, Binder, Binding>(binding, label_widget, editor_widget); 
        value_editors[key] = std::unique_ptr<value_editor>(editor);
    }

    void bind(void *obj)
    {
        for (auto &editor : value_editors)
        {
            editor.second->bind(obj);
        }
    }

    nana::window handle() { return root->handle(); }

    void collocate()
    {
        for (auto& section : sections)
        {
            section->collocate();
        }
        root->collocate();
    }
};

struct section_layout_builder
{
    std::vector<std::string> heights;
    std::string default_height = "30";
    std::string icon_height = "200";
    
    nana::group* group;
    block_editor& block;

    section_layout_builder(block_editor &block)
        : block(block)
    {
        group = new nana::group(block.handle());
        (*block.root)["sections"] << *group;
    }


    void render_column(std::string& out, const std::string& name)
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


    template<typename T>
    nana::widget *add_editor(const std::string &where, const identity<T> t)
    {
        return group->create_child<nana::textbox>(where.c_str());
    }

    template<>
    nana::widget *add_editor(const std::string &where, const identity<std::monostate> t)
    {
        return group->create_child<nana::label>(where.c_str());
    }

    template<>
    nana::widget *add_editor(const std::string &where, const identity<factorio::data::Color> t)
    {
        return group->create_child<nana::button>(where.c_str());
    }

    template<typename T, typename Binder, typename Binding>
    void add_row(Binding binding, const std::string& label)
    {
        //Allocate space in the layout
        heights.emplace_back(default_height);

        //Create the label
        nana::widget* label_widget = add_editor("key_", identity<std::monostate>{});
        label_widget->caption(label);

        //Create the editor
        nana::widget *editor_widget = add_editor("value_", identity<T>{});

        //Register the editor with the block
        block.register_value_editor<T, Binder, Binding>(label, binding, label_widget, editor_widget);
    }

    void collocate()
    {
        group->div(render().c_str());
        group->collocate();
    }
};

struct editor_builder
{
    block_editor *block;

    editor_builder(nana::form& win)
    {
        block = new block_editor(win);
    }

    void section(const std::string& name, std::function<void(section_layout_builder&)> build_section)
    {
        section_layout_builder section_builder(*block);
        build_section(section_builder);
        section_builder.collocate();

        block->sections.emplace_back(section_builder.group);
    }
};

struct UI
{
    nana::form win;

    nana::place layout;

    nana::treebox data_raw;
    nana::picture sprite_preview;
    nana::timer update_filtering;
    nana::textbox search;
    bool filter_pending = false;

    using bind_model_view = std::function<void(block_editor&, FObject&)>;
    std::unordered_map<std::string, bind_model_view> model_bindings;
    std::unordered_map<std::string, std::unique_ptr<block_editor>> editors;
    
    FObject& data;

    UI(FObject& data) : 
        win{ nana::API::make_center(1024, 1024), nana::appear::decorate<nana::appear::taskbar>() },
        data_raw(win),
        layout(win),
        search(win),
        data(data)
    {
        install_events();
    }

    std::string path_to_editor_field(const std::string& in)
    {
        std::string copy = in;
        for (auto& ch : copy)
        {
            if (ch == '/' || ch == '-')
            {
                ch = '_';
            }
        }
        return "editor_" + copy;
    }

    void create_layout()
    {
        std::string editor_switcher = "<switchable editor_root ";
        for (auto it = editors.begin(); it != editors.end(); it++)
        {
            std::string name = path_to_editor_field(it->first);
            editor_switcher += fmt::format("<{0}>", name);
            layout[name.c_str()] << *it->second->root;
        }
        editor_switcher += ">";
        std::string div_left = "< <vert left weight=300<search weight=40><mid>>";
        layout.div(fmt::format("{0} | {1}", div_left, editor_switcher));
        layout["mid"] << data_raw;

        search.multi_lines(false);
        layout["search"] << search;

        layout.collocate();
    }


    void do_filtering(const std::chrono::steady_clock::time_point& now)
    {
        fprintf(stderr, "filtering\n");
        std::string filter = search.getline(0).value();
        auto root = data_raw.find("raw");

        data_raw.auto_draw(false);
        apply_filter(filter, root);
        auto mid = prof::now();
        data_raw.auto_draw(true);
        last_updated = now;
        filter_pending = false;

        auto rendered = prof::now();

        update_filtering.stop();
        auto end = prof::now();

        fprintf(stderr, "elapsed     (filtering): %" PRId64 "ms\n", int64_t(std::chrono::duration_cast<std::chrono::milliseconds>(mid - now).count()));
        fprintf(stderr, "elapsed     (rendering): %" PRId64 "ms\n", int64_t(std::chrono::duration_cast<std::chrono::milliseconds>(rendered - mid).count()));
        fprintf(stderr, "elapsed    (stop clock): %" PRId64 "ms\n", int64_t(std::chrono::duration_cast<std::chrono::milliseconds>(end - rendered).count()));

        fprintf(stderr, "elapsed         (total): %" PRId64 "ms\n", int64_t(std::chrono::duration_cast<std::chrono::milliseconds>(end - now).count()));
    }

    void on_search_text_changed(const nana::arg_textbox& arg)
    {
        auto now = prof::now();
        auto time_since_last_update = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_updated);

        // throttle updates, basically if it's been a while since we last applied a filter, immediately appl it
        if (time_since_last_update >= filter_throttle_interval_ms)
        {
            do_filtering(now);
        }
        else
        {
            // Otherwise check if we don't already have queued up filter, schedule one for throttle-time_since_last_update i.e. the earliest possible time that is not less than throttle rate
            if (!filter_pending)
            {
                update_filtering.reset();
                update_filtering.interval(filter_throttle_interval_ms - time_since_last_update);
                update_filtering.elapse([this]() {
                    do_filtering(prof::now());
                });
                update_filtering.start();
                filter_pending = true;
            }
        }
    }

    void on_search_keypress(const nana::arg_keyboard& arg)
    {
        if (arg.ctrl)
        {
            //READLINE!

            // use stdilb.h/wctomb? :thinking:
            if (arg.key == 'L')
            {
                search.select(true);
                search.del();
            }
            arg.stop_propagation();
        }
    }

    void on_data_selected(const nana::arg_treebox& arg) {
        // ignore de-selection (could do some cache freeing)
        if (!arg.operated)
        {
            return;
        }

        const std::string& path = arg.item.key();
        fprintf(stderr, " === looking up path: %s\n", path.c_str());

        if (path == "raw")
        {
            return;
        }

        std::string truncated_path;
        std::string prototype_type;
        std::string prototype_name;

        size_t n = 0;
        auto it = model_bindings.end();

        while (true)
        {
            auto next = path.find('/', n);

            std::string current = path.substr(n, next - n);

            //If the last thing we found was a matching model_binding for prototype_type, this is the prototype_name
            if (it != model_bindings.end())
            {
                prototype_name = current;
                break;
            }

            //Only append to the path if we haven't found anything yet (i.e. don't include prototype_name in truncated_path)
            truncated_path = path.substr(0, next);

            //Look up a model_binding for the current truncated path and capture it
            if (it = model_bindings.find(truncated_path); it != model_bindings.end())
            {
                prototype_type = current;
            }

            if (next == path.npos)
            {
                break;
            }

            n = next + 1;
        }

        if (!prototype_name.empty())
        {
            FObject &prototype_table = data.table(prototype_type);
            FObject &prototype = prototype_table.table(prototype_name);
            block_editor& editor = *editors[it->first];
            layout.field_display(path_to_editor_field(truncated_path).c_str(), true);
            editor.collocate();
            layout.collocate();
            it->second(editor, prototype);
        }
    }

    void install_events()
    {
        search.events().text_changed([this](auto arg) { on_search_text_changed(arg); });
        search.events().key_press([this](auto arg) { on_search_keypress(arg); });

        data_raw.events().selected([this](auto arg) { on_data_selected(arg); });
    }


    void register_editor(const std::string& for_prototype, std::function<void(editor_builder&)> build_editor, bind_model_view model_binder)
    {
        editor_builder builder(win);
        build_editor(builder);
        builder.block->root->collocate();
        model_bindings[for_prototype] = model_binder;
        editors[for_prototype] = std::unique_ptr<block_editor>(builder.block);
    }
};


int main()
{
    //Create a new VM pointing at the factorio install
     VM vm(STR(FACTORIOPATH));

     vm.run_data_stage();
    
    //===============================================
    // IMPORTANT:
    // This is where we convert from the lua data.raw table into an FObject* (wrapped in an FValue)
    FValue data_raw = vm.get_data_raw();
    //===============================================

    FObject& obj = data_raw.obj();

    UI ui(data_raw.obj());

    //prototype_factories["data/raw/item"] = [&](const std::vector<std::string> &path) {
    //    if (path.size() < 4)
    //    {
    //        return;
    //    }
    //    const std::string &item_name = path[3];

    //    fprintf(stderr, "LOADING ITEM: %s\n", item_name.c_str());

    //    factorio::data::ItemPrototype proto(item);

    //    if (editor)
    //    {
    //        editor->close();
    //        delete editor;
    //    }

    //    editor = new nana::group(*win);

    //    layout["rightno"] << *editor;

    //    // layout["rightno"].

    //    editor_layout base(editor);
    //    base.group->caption("PrototypeBase");

    //    base.normal("name", proto.name);
    //    base.normal("type", proto.type);
    //    base.normal("localised_description", proto.localised_description);
    //    base.normal("localised_name", proto.localised_name);
    //    base.normal("order", proto.order);
    //    base.collocate();

    //    editor_layout item_group(editor);
    //    item_group.group->caption("ItemPrototype");


    //    preview_image.close();
    //    preview_image.open(proto.icon.file_path);
    //    item_group.icon("icon", preview_image);

    //    item_group.normal("fuel_acceleration_multiplier", proto.fuel_acceleration_multiplier);
    //    item_group.normal("fuel_category", proto.fuel_category);
    //    item_group.normal("fuel_emissions_multiplier", proto.fuel_emissions_multiplier);
    //    item_group.normal("fuel_glow_color", proto.fuel_glow_color);
    //    item_group.normal("fuel_top_speed_multiplier", prcontaineroto.fuel_top_speed_multiplier);
    //    item_group.normal("fuel_value", proto.fuel_value);

    //    item_group.collocate();

    //    editor->collocate();
    //    layout.collocate();
    //};

    ui.register_editor("data/raw/item", [](editor_builder& builder) {
        using base = factorio::data::PrototypeBase;
        builder.section("PrototypeBase", [](section_layout_builder& section) {
            section.add_row<factorio::data::string, base>(&base::name, "name");
        });
    }, [](block_editor& ui, FObject& table) {
    });

    ui.register_editor("data/raw/accumulator", [](editor_builder& builder) {
        using base = factorio::data::PrototypeBase;
        builder.section("PrototypeBase", [](section_layout_builder& section) {
            section.add_row<factorio::data::string, base>(&base::name, "name");
        });
    }, [](block_editor& ui, FObject& table) {
    });

        //base.group->caption("TileEffectPrototype");

        //// editor_row(base, proto, animation_scale);
        //base.normal("animation_speed", proto.animation_speed);
        //// editor_row(base, proto, dark_threshold);
        //base.normal("foam_color", proto.foam_color);
        //base.normal("foam_color_multiplier", proto.foam_color_multiplier);
        //base.normal("name", proto.name);
        //// editor_row(base, proto, reflection_threshold);
        //base.normal("specular_lightness", proto.specular_lightness);
        //// editor_row(base, proto, specular_threshold);
        //// editor_row(base, proto, texture);
        //base.normal("tick_scale", proto.tick_scale);
        //base.normal("type", proto.type);
        //base.normal("far_zoom", proto.far_zoom);
        //base.normal("near_zoom", proto.near_zoom);

    //Register an editor for a specific prototype
    //first callback is to build the ui on init (or lazily)
    //second callback is when a prototype is selected, so we update the editor to reflect the data in that prototype
    ui.register_editor("data/raw/tile-effect", [](editor_builder& builder) {
        using proto = factorio::data::TileEffectPrototype;
        builder.section("TileEffectPrototype", [](section_layout_builder& section) {
            section.add_row<double, proto>(&proto::animation_speed, "animation_speed");
        });
    }, [](block_editor& ui, FObject& table) {
        factorio::data::TileEffectPrototype proto(table);
        ui.bind(&proto);
    });

    //This has to be done after we register the prototpe factories so the ui can generate a selection layout to switch between them
    ui.create_layout();

    // Populate the data_raw tree
    auto root_unfiltered = ui.data_raw.insert("raw", "data.raw");

    FKeyValue raw("data.raw", data_raw);

    std::deque<nana::treebox::item_proxy> visual_stack;
    visual_stack.push_front(root_unfiltered);

    std::string path = "data/raw";

    ui.data_raw.auto_draw(false);
    raw.table().visit([&](int dir, FKeyValue entry)
    {
        FObject::visit_result mode = FObject::visit_result::DESCEND;

        nana::treebox::item_proxy node;
        std::string local_path = path + "/" + entry.key;
        if (dir >= 0)
        {
            std::string label;
            if (dir == 0)
                label = fmt::format("{0}: {1}", entry.key, entry.value.to_string());
            else
                label = entry.key;

            node = ui.data_raw.insert(visual_stack.front(), local_path, label);
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
    ui.data_raw.auto_draw(false);

    ui.win.show();

    nana::exec();
}

