#include "assignments_from_json.h"

#include "ci_lazy_methods_needed.h"
#include "java_static_initializers.h"
#include "java_string_library_preprocess.h"
#include "java_string_literals.h"
#include "java_utils.h"

#include <goto-programs/class_identifier.h>
#include <util/allocate_objects.h>
#include <util/expr_initializer.h>
#include <util/message.h>
#include <util/prefix.h>
#include <util/unicode.h>

/// Values passed around between most functions of the recursive deterministic
/// assignment algorithm entered from \ref assign_from_json.
/// The values in a given `det_creation_infot` are never reassigned, but the
/// ones that are not marked `const` may be mutated.
struct det_creation_infot
{
  /// Code block to append all new code to for the deterministic assignments.
  code_blockt &block;

  /// Handles allocation of new symbols, adds them to its symbol table (which
  /// will usually be the same as the `symbol_table` of this struct) and keeps
  /// track of them so declarations for them can be added by the caller before
  /// `block`.
  allocate_objectst &allocate_objects;

  /// Used for looking up symbols corresponding to Java classes and methods.
  symbol_table_baset &symbol_table;

  /// Where runtime types differ from compile-time types, we need to mark the
  /// runtime types as needed by lazy methods.
  optionalt<ci_lazy_methods_neededt> &needed_lazy_methods;

  /// Map to keep track of reference-equal objects. Each entry has an ID (such
  /// that any two reference-equal objects have the same ID) and the expression
  /// for the symbol that all these references point to.
  std::unordered_map<std::string, det_creation_referencet> &references;

  /// Source location associated with the newly added codet.
  const source_locationt &loc;

  /// Maximum value allowed for any (constant or variable length) arrays in user
  /// code.
  size_t max_user_array_length;

  /// Used for the workaround for enums only.
  /// See \ref assign_enum_from_json.
  const java_class_typet &declaring_class_type;
};

static java_class_typet followed_class_type(
  const exprt &expr,
  const symbol_table_baset &symbol_table)
{
  const pointer_typet &pointer_type = to_pointer_type(expr.type());
  const java_class_typet &java_class_type =
    to_java_class_type(namespacet{symbol_table}.follow(pointer_type.subtype()));
  return java_class_type;
}

static bool
has_array_type(const exprt &expr, const symbol_table_baset &symbol_table)
{
  return has_prefix(
    id2string(followed_class_type(expr, symbol_table).get_tag()),
    "java::array[");
}

static bool
is_enum_type(const java_class_typet &class_type)
{
  return class_type.get_base("java::java.lang.Enum").has_value();
}

static bool
has_enum_type(const exprt &expr, const symbol_table_baset &symbol_table)
{
  return is_enum_type(followed_class_type(expr, symbol_table));
}

/// This function is used as a workaround until reference-equal objects defined
/// across several classes are tracked correctly. Once reference-equality works
/// in all cases, this function can be removed.
/// Until then, in the case of an enum expression that needs to be assigned a
/// value, we distinguish between two cases:
/// 1) the enum expression is declared in a class of its own type - in this
///    case, initialize it just as a regular object that has known reference-
///    equal objects. (Corresponds to creating the enum constant in Java.)
///    See \ref assign_reference_from_json.
/// 2) otherwise, initialize it by indexing the $VALUES array with the given
///    ordinal. (Corresponds to retrieving the enum constant in Java.)
///    See \ref assign_enum_from_json.
/// \param expr: an expression representing a Java object.
/// \param symbol_table: used for looking up the type of \p expr.
/// \param declaring_class_type: type of the class where \p expr is declared.
/// \return `true` if \p expr has an enum type and is declared within the
///   definition of that same type, `false` otherwise.
bool is_enum_definition(
  const exprt &expr,
  const symbol_table_baset &symbol_table,
  const java_class_typet &declaring_class_type)
{
  PRECONDITION(can_cast_type<pointer_typet>(expr.type()));
  return followed_class_type(expr, symbol_table) == declaring_class_type &&
         is_enum_type(declaring_class_type);
}

/// Returns true iff the argument has a "@type" key.
/// A runtime type that is different from the objects compile-time type should
/// be specified in `json` in this way.
/// Type values are of the format "my.package.name.ClassName".
static bool has_type(const jsont &json)
{
  if(!json.is_object())
    return false;
  const auto &json_object = static_cast<const json_objectt &>(json);
  return json_object.find("@type") != json_object.end();
}

/// Returns true iff the argument has a "@id" key.
/// The presence of such a key means that there exist objects that are
/// reference-equal to this object.
/// The corresponding value is the unique ID of all objects that are reference-
/// equal to this one.
/// All other key-value pairs of `json` should be as usual.
static bool has_id(const jsont &json)
{
  if(!json.is_object())
    return false;
  const auto &json_object = static_cast<const json_objectt &>(json);
  return json_object.find("@id") != json_object.end();
}

/// Returns true iff the argument has a "@ref" key.
/// The corresponding value is the unique ID of all objects that are reference-
/// equal to this one.
/// Any other key-value pairs of `json` will be ignored.
static bool is_reference(const jsont &json)
{
  if(!json.is_object())
    return false;
  const auto &json_object = static_cast<const json_objectt &>(json);
  return json_object.find("@ref") != json_object.end();
}

/// Returns the unique ID of all objects that are reference-equal to this one.
/// See \ref has_id and \ref is_reference.
static std::string get_id(const jsont &json)
{
  PRECONDITION(has_id(json) || is_reference(json));
  if(has_id(json))
    return json["@id"].value;
  return json["@ref"].value;
}

static std::string get_enum_id(
  const exprt &expr,
  const jsont &json,
  symbol_table_baset &symbol_table)
{
  PRECONDITION(json.is_object());
  const auto &json_object = static_cast<const json_objectt &>(json);
  PRECONDITION(json_object.find("name") != json_object.end());
  return id2string(followed_class_type(expr, symbol_table).get_tag()) + '.' +
         (json["name"].value);
}

/// Returns true iff the argument has a "@nondetLength: true" entry.
/// If such an entry is present on a JSON representation of an array, it means
/// that the array should be assigned a nondeterministic length, constrained to
/// be at least the number of elements specified for this array.
static bool has_nondet_length(const jsont &json)
{
  if(!json.is_object())
    return false;
  const auto &json_object = static_cast<const json_objectt &>(json);
  if(json_object.find("@nondetLength") != json_object.end())
    return (json["@nondetLength"].is_true());
  return false;
}

/// For typed versions primitive, string or array types, looks up their untyped
/// contents with the key specific to their type.
static jsont get_untyped(const jsont &json, const std::string &object_key)
{
  if(has_type(json) || has_nondet_length(json))
    return json[object_key];
  return json;
}

/// \ref get_untyped for primitive types.
static jsont get_untyped_primitive(const jsont &json)
{
  return get_untyped(json, "value");
}

/// \ref get_untyped for array types.
static jsont get_untyped_array(const jsont &json)
{
  return get_untyped(json, "@items");
}

/// \ref get_untyped for string types.
/// Note that this differs from the standard serialization of java.lang.String
/// in json-io, but is consistent with the serialization of StringBuilder and
/// StringBuffer.
static jsont get_untyped_string(const jsont &json)
{
  return get_untyped(json, "value");
}

/// Given a JSON representation of a (non-array) reference-typed object and a
/// type inferred from the type of a containing array, get the runtime type of
/// the corresponding pointer expression.
/// \param json: JSON representation of a non-array object. If it contains a
///   `@type` field, this takes priority over \p type_from_array. Types for non-
///   array objects are stored in the JSON in the format
///   "my.package.name.ClassName".
/// \param type_from_array: may contain an element type name given by a
///   containing array. Such types are stored in the form
///   "Lmy.package.name.ClassName;".
/// \param symbol_table: used to look up the type given its name.
/// \return runtime type of the object, if specified by at least one of the
///   parameters.
static optionalt<java_class_typet> given_runtime_type(
  const jsont &json,
  const optionalt<std::string> &type_from_array,
  symbol_table_baset &symbol_table)
{
  std::string runtime_type;
  if(!has_type(json) && !type_from_array)
    return {};
  if(has_type(json))
    runtime_type = "java::" + json["@type"].value;
  else
  {
    PRECONDITION(
      type_from_array->find('L') == 0 &&
      type_from_array->rfind(';') == type_from_array->length() - 1);
    runtime_type =
      "java::" + type_from_array->substr(1, type_from_array->length() - 2);
  }
  if(!symbol_table.has_symbol(runtime_type))
    return {}; // TODO add warning here, falling back to default value
  const auto &replacement_class_type =
    to_java_class_type(symbol_table.lookup(runtime_type)->type);
  return replacement_class_type;
}

/// Given a JSON representation of an array and a type inferred from the type of
/// a containing array, get the element type by removing the leading '['.
/// Types for arrays are stored in the format "[Lmy.package.name.ClassName;".
/// In this case, the returned value would be "Lmy.package.name.ClassName;".
/// \p type_from_array would only have a value if this array is stored within
/// another array, i.e.\ within a ClassName[][].
/// Keeping track of array types in this way is necessary to assign generic
/// arrays with no compile-time types.
/// \param json: JSON representation of an array. If it contains a `@type`
///   field, this takes priority over \p type_from_array.
/// \param type_from_array: may contain a type name from a containing array.
/// \return if the type of an array was given, the type of its elements.
static optionalt<std::string> element_type_from_array_type(
  const jsont &json,
  const optionalt<std::string> &type_from_array)
{
  if(has_type(json))
  {
    const std::string &json_array_type = json["@type"].value;
    PRECONDITION(json_array_type.find('[') == 0);
    return json_array_type.substr(1);
  }
  else if(type_from_array)
  {
    PRECONDITION(type_from_array->find('[') == 0);
    return type_from_array->substr(1);
  }
  return {};
}

/// Entry point of the recursive deterministic assignment algorithm.
/// \param expr: expression to assign a deterministic value to. In the case of
///   the entry point, this is either a pointer to a struct, or an expression
///   corresponding to a Java primitive.
/// \param json: a JSON representation of the deterministic value to assign.
/// \param type_from_array: if `expr` was found as an element of an array,
///   the element type of this array.
/// \param info: references used throughout the recursive algorithm.
void assign_from_json_rec(
  const exprt &expr,
  const jsont &json,
  const optionalt<std::string> &type_from_array,
  det_creation_infot &info);

/// One of the base cases (primitive case) of the recursion.
/// For characters, the encoding in \p json is assumed to be UTF-8.
/// See \ref assign_from_json_rec.
static void assign_primitive_from_json(
  const exprt &expr,
  const jsont &json,
  code_blockt &init_body)
{
  if(json.is_null()) // field is not mentioned in json, leave as default value
    return;
  if(expr.type() == java_boolean_type())
  {
    if(json.is_true())
      init_body.add(code_assignt(expr, true_exprt()));
    else
      init_body.add(code_assignt(expr, false_exprt()));
  }
  else if(
    expr.type() == java_int_type() || expr.type() == java_byte_type() ||
    expr.type() == java_short_type() || expr.type() == java_long_type())
  {
    init_body.add(
      code_assignt(expr, from_integer(std::stoi(json.value), expr.type())));
  }
  else if(expr.type() == java_double_type())
  {
    ieee_floatt ieee_float(to_floatbv_type(expr.type()));
    ieee_float.from_double(std::stod(json.value));
    init_body.add(code_assignt(expr, ieee_float.to_expr()));
  }
  else if(expr.type() == java_float_type())
  {
    ieee_floatt ieee_float(to_floatbv_type(expr.type()));
    ieee_float.from_float(std::stof(json.value));
    init_body.add(code_assignt(expr, ieee_float.to_expr()));
  }
  else if(expr.type() == java_char_type())
  {
    const std::wstring wide_value = utf8_to_utf16_native_endian(json.value);
    if(wide_value.length() == 1)
    {
      init_body.add(
        code_assignt(expr, from_integer(wide_value.front(), expr.type())));
    }
    else
    {
      // Workaround for JSON parser bug: Unicode representation does not get
      // parsed correctly, e.g. \u0001 appears as "0001" in json.value.
      // So we assume here that json.value is just a sequence of four digits.
      unsigned value;
      std::stringstream stream;
      stream << std::hex << json.value;
      stream >> value;
      init_body.add(code_assignt(expr, from_integer(value, expr.type())));
    }
  }
}

/// One of the base cases of the recursive algorithm. See
/// \ref assign_from_json_rec.
static void assign_null(const exprt &expr, code_blockt &block)
{
  block.add(
    code_assignt(expr, null_pointer_exprt(to_pointer_type(expr.type()))));
}

/// TODO remove after rebase
dereference_exprt
array_element_from_pointer(const exprt &pointer, const exprt &index)
{
  return dereference_exprt{plus_exprt{pointer, index}};
}

/// In the case of an assignment of an array given a JSON representation, this
/// function assigns the data component of the array, which contains the array
/// elements. \p expr is a pointer to the array containing the component.
static void assign_array_data_component_from_json(
  const exprt &expr,
  const jsont &json,
  const optionalt<std::string> &type_from_array,
  det_creation_infot &info)
{
  const jsont untyped_json = get_untyped_array(json);
  PRECONDITION(untyped_json.is_array());
  const auto &json_array =
    static_cast<const json_arrayt &>(untyped_json);

  const auto &java_class_type = followed_class_type(expr, info.symbol_table);
  const auto &components = java_class_type.components();
  const auto &element_type = static_cast<const typet &>(
    to_pointer_type(expr.type()).subtype().find(ID_element_type));
  const exprt data_member_expr = typecast_exprt::conditional_cast(
    member_exprt{dereference_exprt{expr}, "data", components[2].type()},
    pointer_type(element_type));

  const symbol_exprt &array_init_data =
    info.allocate_objects.allocate_automatic_local_object(
      data_member_expr.type(), "det_array_data_init");
  code_assignt data_assign{array_init_data, data_member_expr, info.loc};
  info.block.add(data_assign);

  size_t index = 0;
  const optionalt<std::string> inferred_element_type =
    element_type_from_array_type(json, type_from_array);
  for(auto it = json_array.begin(); it < json_array.end(); it++, index++)
  {
    const dereference_exprt element_at_index = array_element_from_pointer(
      array_init_data, from_integer(index, java_int_type()));
    assign_from_json_rec(element_at_index, *it, inferred_element_type, info);
  }
}

/// Allocates a fresh array of length \p array_length_expr and assigns \p expr
/// to it.
static void allocate_array(
  const exprt &expr,
  const exprt &array_length_expr,
  det_creation_infot &info)
{
  const pointer_typet &pointer_type = to_pointer_type(expr.type());
  const auto &element_type =
    static_cast<const typet &>(pointer_type.subtype().find(ID_element_type));
  side_effect_exprt java_new_array{ID_java_new_array, pointer_type, info.loc};
  java_new_array.copy_to_operands(array_length_expr);
  java_new_array.type().subtype().set(ID_element_type, element_type);
  code_assignt assign{expr, java_new_array, info.loc};
  info.block.add(assign);
}

/// One of the cases in the recursive algorithm: the case where \p expr
/// represents an array.
/// The length of the array is given by a symbol: \p given_length_expr if it is
/// specified (this will be the case when there are two or more reference-equal
/// arrays), or a fresh symbol otherwise.
/// If \p given_length_expr is specified, we assume that an array with this
/// symbol as its length has already been allocated and that \p expr has been
/// assigned to it.
/// Either way, the length symbol stores a nondet integer, and we add
/// constraints on this: if "nondetLength" is specified in \p json, then the
/// number of elements specified in \p json should be the minimum length of the
/// array. Otherwise the number of elements should be the exact length of the
/// array.
/// For the assignment of the array elements, see
/// \ref assign_array_data_component_from_json.
/// For the overall algorithm, see \ref assign_from_json_rec.
static void assign_array_from_json(
  const exprt &expr,
  const jsont &json,
  const optionalt<symbol_exprt> &given_length_expr,
  const optionalt<std::string> &type_from_array,
  det_creation_infot &info)
{
  PRECONDITION(can_cast_type<pointer_typet>(expr.type()));
  PRECONDITION(has_array_type(expr, info.symbol_table));
  const jsont untyped_json = get_untyped_array(json);
  PRECONDITION(untyped_json.is_array());
  const auto &json_array =
    static_cast<const json_arrayt &>(untyped_json);

  exprt length_expr;
  if(given_length_expr)
    length_expr = *given_length_expr;
  else
  {
    length_expr = info.allocate_objects.allocate_automatic_local_object(
      java_int_type(), "det_array_length");
    info.block.add(code_assignt{
      length_expr, side_effect_expr_nondett{java_int_type(), info.loc}});
    info.block.add(code_assumet{binary_predicate_exprt{
      length_expr, ID_ge, from_integer(0, java_int_type())}});
    allocate_array(expr, length_expr, info);
  }
  const auto number_of_elements =
    from_integer(json_array.size(), java_int_type());
  info.block.add(code_assumet{binary_predicate_exprt{
    length_expr, ID_ge, number_of_elements}});
  if(has_nondet_length(json))
  {
    info.block.add(code_assumet{binary_predicate_exprt{
      length_expr, ID_le, from_integer(info.max_user_array_length, java_int_type())}});
  }
  else
  {
    info.block.add(code_assumet{binary_predicate_exprt{
      length_expr, ID_le, number_of_elements}});
  }

  assign_array_data_component_from_json(expr, json, type_from_array, info);
}

/// One of the cases in the recursive algorithm: the case where \p expr
/// represents a string.
/// See \ref assign_from_json_rec.
static void assign_string_from_json(
  const jsont &json,
  const exprt &expr,
  det_creation_infot &info)
{
  const auto json_string = get_untyped_string(json);
  PRECONDITION(json_string.is_string());
  info.block.add(code_assignt(
    expr,
    get_or_create_string_literal_symbol(
      json_string.value, info.symbol_table, true)));
}

/// Helper function for \ref assign_struct_from_json which recursively assigns
/// values to all of the fields of the Java object represented by \p expr (the
/// components of its type and all of its parent types).
static void assign_struct_components_from_json(
  const exprt &expr,
  const jsont &json,
  det_creation_infot &info)
{
  const java_class_typet &java_class_type =
    to_java_class_type(namespacet{info.symbol_table}.follow(expr.type()));
  for(const auto &component : java_class_type.components())
  {
    const irep_idt &component_name = component.get_name();
    if(
      component_name == "@class_identifier" ||
      component_name == "cproverMonitorCount")
    {
      continue;
    }
    member_exprt member_expr{expr, component_name, component.type()};
    if(component_name[0] == '@') // component is parent struct type
      assign_struct_components_from_json(member_expr, json, info);
    else // component is class field (pointer to struct)
    {
      const jsont member_json = json[id2string(component_name)];
      assign_from_json_rec(member_expr, member_json, {}, info);
    }
  }
}

/// One of the cases in the recursive algorithm: the case where \p expr is a
/// struct, which is the result of dereferencing a pointer that corresponds to
/// the Java object described in \p json.
/// See \ref assign_from_json_rec.
static void assign_struct_from_json(
  const exprt &expr,
  const jsont &json,
  det_creation_infot &info)
{
  const namespacet ns{info.symbol_table};
  const java_class_typet &java_class_type =
    to_java_class_type(ns.follow(expr.type()));
  if(is_java_string_type(java_class_type))
    assign_string_from_json(json, expr, info);
  else
  {
    auto initial_object = zero_initializer(expr.type(), source_locationt(), ns);
    CHECK_RETURN(initial_object.has_value());
    set_class_identifier(
      to_struct_expr(*initial_object),
      ns,
      struct_tag_typet("java::" + id2string(java_class_type.get_tag())));
    info.block.add(code_assignt(expr, *initial_object));
    assign_struct_components_from_json(expr, json, info);
  }
}

/// Same as \ref assign_pointer_from_json without special cases (enums).
static void assign_general_pointer_from_json(
  const exprt &expr,
  const jsont &json,
  det_creation_infot &info)
{
  exprt dereferenced_symbol_expr =
    info.allocate_objects.allocate_dynamic_object(
      info.block, expr, to_pointer_type(expr.type()).subtype());
  assign_struct_from_json(dereferenced_symbol_expr, json, info);
}

/// One of the cases in the recursive algorithm: the case where the expression
/// to be assigned a value is an enum constant that is referenced outside of the
/// definition of its type. (See \ref is_enum_definition for this temporary
/// distinction. See \ref assign_from_json for details about the recursion.)
/// Once reference-equality of fields in different classes is supported, this
/// function can be removed.
static void assign_enum_from_json(
  const exprt &expr,
  const jsont &json,
  det_creation_infot &info)
{
  const auto &java_class_type = followed_class_type(expr, info.symbol_table);
  const std::string &enum_name = id2string(java_class_type.get_name());
  if(const auto func = info.symbol_table.lookup(clinit_wrapper_name(enum_name)))
    info.block.add(code_function_callt{func->symbol_expr()});

  const irep_idt values_name = enum_name + ".$VALUES";
  if(!info.symbol_table.has_symbol(values_name))
  {
    // Fallback: generate a new enum instance instead of getting it from $VALUES
    assign_general_pointer_from_json(expr, json, info);
    return;
  }

  dereference_exprt values_struct{
    info.symbol_table.lookup_ref(values_name).symbol_expr()};
  const auto &values_struct_type = to_struct_type(
    namespacet{info.symbol_table}.follow(values_struct.type()));
  PRECONDITION(is_valid_java_array(values_struct_type));
  const member_exprt values_data = member_exprt{
    values_struct, "data", values_struct_type.components()[2].type()};

  const exprt ordinal_expr =
    from_integer(std::stoi(json["ordinal"].value), java_int_type());

  info.block.add(code_assignt(
    expr,
    typecast_exprt::conditional_cast(
      array_element_from_pointer(values_data, ordinal_expr), expr.type())));
}

/// One of the cases in the recursive algorithm: the case where \p expr is a
/// pointer to a struct, whose type is the same as the runtime-type of the
/// corresponding Java object.
/// See \ref assign_from_json_rec.
static void assign_pointer_from_json(
  const exprt &expr,
  const jsont &json,
  det_creation_infot &info)
{
  // This check can be removed when tracking reference-equal objects across
  // different classes has been implemented.
  if(has_enum_type(expr, info.symbol_table))
    assign_enum_from_json(expr, json, info);
  else
    assign_general_pointer_from_json(expr, json, info);
}

/// One of the cases in the recursive algorithm: the case where \p expr is a
/// pointer to a struct, and \p runtime_type is the runtime type of the
/// corresponding Java object, which may be more specific than the type pointed
/// to by `expr.type()` (the compile-time type of the object).
/// See \ref assign_from_json_rec.
static void assign_pointer_with_given_type_from_json(
  const exprt &expr,
  const jsont &json,
  const java_class_typet &runtime_type,
  det_creation_infot &info)
{
  const auto &pointer_type = to_pointer_type(expr.type());
  pointer_typet replacement_pointer_type =
    pointer_to_replacement_type(pointer_type, runtime_type);
  if(!equal_java_types(pointer_type, replacement_pointer_type))
  {
    const auto &new_symbol =
      info.allocate_objects.allocate_automatic_local_object(
        replacement_pointer_type, "det_subtype_symbol");
    if(info.needed_lazy_methods)
      info.needed_lazy_methods->add_all_needed_classes(
        replacement_pointer_type);

    assign_pointer_from_json(new_symbol, json, info);
    info.block.add(
      code_assignt(expr, typecast_exprt(new_symbol, pointer_type)));
  }
  else
    assign_pointer_from_json(expr, json, info);
}

/// Helper function for \ref assign_reference_from_json.
/// Looks up the given \p id in the reference map and gets or creates the symbol
/// for it.
/// In the case of arrays, if the first time we see an ID is in a `@ref` object
/// (rather than `@id`), we do not know what the length of the array will be, so
/// we need to allocate an array of nondeterministic length. The length will
/// be constrained (in \ref assign_array_from_json) once we find the
/// corresponding `@id` object.
/// \param expr: expression representing the Java object for which a symbol is
///   retrieved or allocated.
/// \param id: key in the reference map for this object
/// \param info: references used throughout the recursive algorithm.
/// \return a pair: the first element is true if a new symbol was allocated for
///   the given ID and false if the ID was found in the reference map. The
///   second element has the symbol expression(s) for this ID.
static std::pair<bool, det_creation_referencet> get_or_create_reference(
  const exprt &expr,
  const std::string &id,
  det_creation_infot &info)
{
  const auto &pointer_type = to_pointer_type(expr.type());
  const auto id_it = info.references.find(id);
  if(id_it == info.references.end())
  {
    det_creation_referencet reference;
    if(has_array_type(expr, info.symbol_table))
    {
      reference.expr = info.allocate_objects.allocate_automatic_local_object(
        pointer_type, "det_array_ref");
      reference.array_length =
        info.allocate_objects.allocate_automatic_local_object(
          java_int_type(), "det_array_length");
      info.block.add(code_assignt(
        *reference.array_length,
        side_effect_expr_nondett(java_int_type(), info.loc)));
      info.block.add(code_assumet{binary_predicate_exprt{
        *reference.array_length, ID_ge, from_integer(0, java_int_type())}});
      allocate_array(reference.expr, *reference.array_length, info);
      info.references.insert({id, reference});
    }
    else
    {
      reference.expr = info.allocate_objects.allocate_dynamic_object_symbol(
        info.block, expr, pointer_type.subtype());
      info.references.insert({id, reference});
    }
    return {true, reference};
  }
  return {false, id_it->second};

}

/// One of the cases in the recursive algorithm: the case where \p expr
/// corresponds to a Java object that is reference-equal to one or more other
/// Java objects represented in the initial JSON file.
/// See \ref assign_from_json_rec.
/// Such an object will either have the key-value pair `@id: some_key` in
/// \p json, together with a full representation of the object, or it will only
/// have one key-value pair, `@ref: some_key`. For each key, there is only one
/// `@id` field in the JSON file.
/// A special case is enums, which are always represented as a full object
/// without any `@id` or `@ref` keys. This is mostly the same as the output from
/// json-io for enums, except that in our representation, we need to include the
/// ordinal field so that e.g. switch statements on enums will work.
/// We keep track of object IDs using a map from IDs to symbol expressions.
/// Usually the ID is the `some_key` from the example above, except for enums,
/// where the ID is of the form `my.package.name.EnumName.CONSTANT`.
/// The first time we see an ID (`@id`, `@ref` or enum constant), we allocate a
/// symbol for it. The first time we see the full representation of the object
/// (`@id` or enum constant) we initialize the allocated memory. This strategy
/// may need to be changed to support reference-equality of fields across
/// several different classes (e.g.\ as soon as we find a `@ref` for the first
/// time we might want to search the whole initial JSON file for the
/// corresponding `@id`).
static void assign_reference_from_json(
  const exprt &expr,
  const jsont &json,
  const optionalt<std::string> &type_from_array,
  det_creation_infot &info)
{
  const std::string &id = has_enum_type(expr, info.symbol_table)
                            ? get_enum_id(expr, json, info.symbol_table)
                            : get_id(json);
  const auto bool_reference_pair = get_or_create_reference(expr, id, info);
  const bool is_new_id = bool_reference_pair.first;
  const det_creation_referencet &reference = bool_reference_pair.second;
  if(is_new_id && has_enum_type(expr, info.symbol_table))
    assign_struct_from_json(dereference_exprt(reference.expr), json, info);
  else if(has_id(json))
  {
    if(has_array_type(expr, info.symbol_table))
    {
      assign_array_from_json(
        reference.expr, json, reference.array_length, type_from_array, info);
    }
    else
      assign_struct_from_json(dereference_exprt(reference.expr), json, info);
  }
  info.block.add(
    code_assignt{expr, typecast_exprt{reference.expr, expr.type()}});
}

void assign_from_json_rec(
  const exprt &expr,
  const jsont &json,
  const optionalt<std::string> &type_from_array,
  det_creation_infot &info)
{
  if(can_cast_type<pointer_typet>(expr.type()))
  {
    if(json.is_null())
      assign_null(expr, info.block);
    else if(
      is_reference(json) || has_id(json) ||
      is_enum_definition(expr, info.symbol_table, info.declaring_class_type))
      // The last condition can be replaced with
      // has_enum_type(expr, info.symbol_table) once tracking reference-equality
      // across different classes has been implemented.
    {
      assign_reference_from_json(expr, json, type_from_array, info);
    }
    else if(has_array_type(expr, info.symbol_table))
      assign_array_from_json(expr, json, {}, type_from_array, info);
    else if(
      const auto runtime_type =
        given_runtime_type(json, type_from_array, info.symbol_table))
    {
      assign_pointer_with_given_type_from_json(expr, json, *runtime_type, info);
    }
    else
      assign_pointer_from_json(expr, json, info);
  }
  else
    assign_primitive_from_json(expr, get_untyped_primitive(json), info.block);
}

void assign_from_json(
  const exprt &expr,
  const jsont &json,
  const irep_idt &function_id,
  code_blockt &assignments,
  symbol_table_baset &symbol_table,
  optionalt<ci_lazy_methods_neededt> &needed_lazy_methods,
  size_t max_user_array_length,
  std::unordered_map<std::string, det_creation_referencet> &references)
{
  source_locationt location{};
  allocate_objectst allocate(
    ID_java,
    location,
    function_id,
    symbol_table);
  location.set_function(function_id);
  code_blockt body_rec;
  const auto class_name = declaring_class(symbol_table.lookup_ref(function_id));
  INVARIANT(
    class_name,
    "Function " + id2string(function_id) + " must be declared by a class.");
  const auto &class_type =
    to_java_class_type(symbol_table.lookup_ref(*class_name).type);
  det_creation_infot info{body_rec,
                          allocate,
                          symbol_table,
                          needed_lazy_methods,
                          references,
                          location,
                          max_user_array_length,
                          class_type};
  assign_from_json_rec(expr, json, {}, info);
  allocate.declare_created_symbols(assignments);
  assignments.append(body_rec);
}