#pragma once

#include "nlohmann/detail/macro_scope.hpp"
#include "nlohmann/json.hpp"
using json = nlohmann::json;

#include <cstdint>
#include <string>
#include <vector>


namespace TXP
{
namespace Shader_Creation
{
namespace Reflection
{

// @NOTE: naming convention of vars will not match rest of project.

// Helpers.

struct Binding
{
    std::string kind;
    int32_t index;
    int32_t count{ 1 };
    int32_t offset{ 0 };
    int32_t size{ -1 };
    int32_t elementStride{ -1 };

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Binding,
                                                kind,
                                                index,
                                                count,
                                                offset,
                                                size,
                                                elementStride);
};

struct Struct_field;

struct Field_type
{
    std::string kind;

    // scalar
    std::string scalarType;

    // array, vector
    int32_t elementCount{ -1 };

    // matrix
    int32_t rowCount{ -1 };
    int32_t columnCount{ -1 };

    // resource
    std::string baseShape;
    std::string access;
    json resultType;  // Can convert to `Field_type` manually.

    // matrix, vector, array
    json elementType;  // Can convert to `Field_type` manually.

    // struct
    std::string name;
    std::vector<Struct_field> fields;

    /// @NOTE: serialization defined non-intrusive vv below vv
};

struct Struct_field
{
    std::string name;
    Field_type type;
    std::string stage;
    Binding binding;
    std::string semanticName;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Struct_field, name, type, stage, binding, semanticName);
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Field_type,
                                                kind,
                                                scalarType,
                                                elementCount,
                                                rowCount,
                                                columnCount,
                                                baseShape,
                                                access,
                                                resultType,
                                                elementType,
                                                name,
                                                fields);

// Entry points.

struct NamedBinding
{
    std::string name;
    Binding binding;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(NamedBinding, name, binding);
};

struct Entry_point_parameter
{
    std::string name;
    std::string stage;
    std::string semanticName;
    Binding binding;
    Field_type type;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Entry_point_parameter,
                                                name,
                                                stage,
                                                semanticName,
                                                binding,
                                                type);
};

struct Entry_point
{
    std::string name;
    std::string stage;

    std::vector<int32_t> threadGroupSize;  // Only in compute shader.
    std::vector<NamedBinding> bindings;

    std::vector<Entry_point_parameter> parameters;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Entry_point,
                                                name,
                                                stage,
                                                threadGroupSize,
                                                bindings,
                                                parameters);
};

// Parameters.

struct Element_type_field  // Data appear to be different depending on `elementType` or `elementVarLayout`.
{
    std::string name;
    Field_type type;
    Binding binding;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Element_type_field, name, type, binding);
};

struct Element_type
{
    std::string kind;
    std::vector<Element_type_field> fields;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Element_type, kind, fields);
};

struct Container_var_layout
{
    Binding binding;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Container_var_layout, binding);
};

struct Element_var_layout
{
    Element_type type;
    std::vector<Binding> bindings;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Element_var_layout, type, bindings);
};

struct Param_type
{
    std::string kind;
    Element_type elementType;
    Container_var_layout containerVarLayout;
    Element_var_layout elementVarLayout;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Param_type,
                                   kind,
                                   elementType,
                                   containerVarLayout,
                                   elementVarLayout);
};

struct Parameter
{
    std::string name;
    Binding binding;
    Param_type type;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Parameter, name, binding, type);
};

}  // namespace Reflection
}  // namespace Shader_Creation
}  // namespace TXP
