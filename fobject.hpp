#pragma once
#include <variant>

#include "util.hpp"
#include "fmt/format.h"

struct FObject;

// helper type for the visitor #4
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...)->overloaded<Ts...>; // not needed as of C++20

struct FValue
{
    using data_type = std::variant<std::monostate, FObject*, std::string, uint64_t, double, bool>;

    data_type data;

    FValue() : data() {}
    FValue(const data_type& data) : data(data) {}


    template<typename T>
    const T* as() const { return std::get_if<T>(&data); }

    template<typename T>
    T* as() { return const_cast<T*>(std::as_const(*this).as<T>()); }

    const FObject& obj() const;
    FObject& obj();

    std::string to_string()
    {
        const static std::string nil_s = "!<>";
        const static std::string true_s = "true";
        const static std::string false_s = "false";
        const static std::string table_s = "table";

        std::visit(overloaded{
            [](std::monostate arg) {return nil_s;  },
            [](bool arg) {return arg ? true_s : false_s;  },
            [](double arg) {return fmt::to_string(arg);  },
            [](uint64_t arg) {return fmt::to_string(arg);  },
            [](const std::string& arg) { return arg;  },
            [](const FObject*& arg) { return table_s;  },
            }, data);

        __builtin_unreachable();
        return nil_s;
    }

    double to_double() const
    {
        if (auto num = as<double>(); num)
            return *num;
        return *as<uint64_t>();
    }
    double to_double()
    {
        return std::as_const(*this).to_double();
    }

    
    template<typename T>
    void try_assign(T& target) const
    {
        if (T* val = as<T>(); val)
        {
            target = *val;
        }
    }

    void try_to_double(double& target) const
    {
          std::visit(overloaded{
            [](auto arg) {},
            [&target](double arg) { target = arg; },
            [&target](uint64_t arg) { target = double(arg); },
            }, data);
    }

    operator bool() const { return data.index() != 0; }

    static FValue nil;
};



struct FKeyValue
{
    std::string key;
    FValue value;

    FKeyValue() {}
    FKeyValue(const std::string& key, const FValue&& value) : key(key), value(value) {}
    FKeyValue(const std::string& key, const FValue& value) : key(key), value(value) {}

    FObject& table() { return value.obj(); }
};


struct FObject
{
    enum class visit_result { DESCEND, CONTINUE, EXIT, };
    static FObject nil;
    std::string type;
    bool valid;

    FObject(bool valid = true) : valid(valid) {}
    FObject(const FObject& copy) = delete;

    std::vector<FKeyValue> children;
    ska::bytell_hash_map<std::string, int> name_to_child;

    const FValue& child(const std::string& key) const {
        if (auto it = name_to_child.find(key); it != name_to_child.end())
        {
            return children.at(it->second).value;
        }
        else
        {
            return FValue::nil;
        }
    }
    FValue& child(const std::string& key) {
        return const_cast<FValue&>(std::as_const(*this).child(key));
    }

    const FValue& operator[] (const std::string& key) const { return child(key); }
    FValue& operator[](const std::string& key) { return child(key); }
    const FValue& operator[] (const char* key) const { return child(key); }
    FValue& operator[](const char* key) { return child(key); }

    FObject& table(const std::string& key) {
        if (auto it = name_to_child.find(key); it != name_to_child.end())
        {
            return children.at(it->second).table();
        }
        else
        {
            return nil;
        }
    }

    void sort()
    {
        std::sort(children.begin(), children.end(), [](FKeyValue& a, FKeyValue& b) { return a.key < b.key; });
        for (size_t i = 0; i < children.size(); i++)
        {
            name_to_child[children[i].key] = int(i);
        }

    }

    template<typename T>
    void visit(T callback)
    {
        for (auto& key : children)
        {
            if (auto& object = key.value.obj(); object)
            {
                if (callback(1, key) == FObject::visit_result::DESCEND)
                {
                    object.visit(callback);
                    callback(-1, key);
                }
            }
            else
            {
                callback(0, key);
            }
        }
    }

    operator bool() const {
        return valid;
    }

};
