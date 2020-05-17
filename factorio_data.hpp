#pragma once
#include "util.hpp"
#include "fobject.hpp"
#include "vm.hpp"

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
            Icon(VM &vm, const FObject& obj)
            {
                std::string path = *obj.child("icon").as<std::string>();
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
            PrototypeBase(const FObject& obj);
            string name;
            string type;
            LocalisedString localised_description;
            LocalisedString localised_name;
            Order order;
        };

        struct TileEffectPrototype
        {
            TileEffectPrototype(const FObject& obj);
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
            ItemPrototype(VM &vm, const FObject& obj);
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


