#include "factorio_data.hpp"
#include <string>


template<typename T>
T parse_fval(T& out, const FValue& value);

template<>
std::string parse_fval(std::string& out, const FValue& value)
{
    if (auto str = value.as<std::string>(); str)
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
uint32_t parse_fval(uint32_t& out, const FValue& value)
{
    if (auto num = value.as<uint64_t>(); num)
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
double parse_fval(double& out, const FValue& value)
{
    if (auto num = value.as<double>(); num)
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
factorio::data::Color parse_fval(factorio::data::Color& out, const FValue& value)
{
    // reset to default
    out = factorio::data::Color();
    if (auto& array = value.obj(); array)
    {
        if (auto index1 = array["1"]; index1)
        {
            out.r = index1.to_double();
            out.g = array.child("2").to_double();
            out.b = array.child("3").to_double();

            if (auto index3 = array.child("3"); index3)
            {
                out.a = index3.to_double();
            }
        }
        else
        {
            if (auto x = array.child("r"); x) out.r = x.to_double();
            if (auto x = array.child("g"); x) out.g = x.to_double();
            if (auto x = array.child("b"); x) out.b = x.to_double();
            if (auto x = array.child("a"); x) out.a = x.to_double();
        }
    }
    else
    {
        out = { -1, -1, -1, -1 };
    }
    return out;
}

template<int min, int max>
factorio::data::array_opt<double, min, max> parse_fval(factorio::data::array_opt<double, min, max>& out, const FValue& value)
{
    out.count = 0;
    if (auto& array = value.obj(); array)
    {
        for (const auto& kv : array.children)
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
T parse_fval(const FValue& value)
{
    T out;
    return parse_fval(out, value);
}

factorio::data::TileEffectPrototype::TileEffectPrototype(const FObject& obj)
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


factorio::data::ItemPrototype::ItemPrototype(VM &vm, const FObject& obj) : PrototypeBase(obj)
{

    ld(fuel_acceleration_multiplier, obj);
    ld(fuel_category, obj);
    ld(fuel_emissions_multiplier, obj);
    ld(fuel_glow_color, obj);
    ld(fuel_top_speed_multiplier, obj);
    ld(fuel_value, obj);

    icon = Icon(vm, obj);
}

factorio::data::PrototypeBase::PrototypeBase(const FObject& obj)
{
    ld(name, obj);
    ld(type, obj);
    ld(localised_description, obj);
    ld(localised_name, obj);
    ld(order, obj);
}
