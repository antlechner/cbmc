#include "java_det_object_factory.h"

#include <util/expr_initializer.h>
#include <util/nondet.h>
#include <util/nondet_bool.h>
#include <util/pointer_offset_size.h>
#include <util/prefix.h>
#include <util/unicode.h>

#include <goto-programs/class_identifier.h>
#include <goto-programs/goto_functions.h>
#include <util/arith_tools.h>
#include <util/ieee_float.h>

#include "generic_parameter_specialization_map_keys.h"
#include "java_root_class.h"
#include "java_static_initializers.h"
#include "java_string_library_preprocess.h"
#include "java_string_literals.h"
#include "java_utils.h"

/// Values passed around between most functions of the recursive deterministic
/// assignment algorithm.
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
  /// that any two reference-equal objects have the same ID) and a struct that
  /// stores values related to the object in memory that all these references
  /// point to.
  std::unordered_map<std::string, det_creation_referencet> &references;

  /// Source location associated with the newly added codet.
  const source_locationt &loc;
};

/// Entry point of the recursive deterministic assignment algorithm.
/// \param expr: expression to assign a deterministic value to. Either a pointer
///   to a struct, or an expression corresponding to a Java primitive.
/// \param json: a JSON representation of the deterministic value to assign.
/// \param type_from_array: if `expr` was found as an element of an array,
///   the element type of this array.
/// \param info: references used throughout the recursive algorithm.
void assign_from_json_rec(
  const exprt &expr,
  const jsont &json,
  const optionalt<std::string> &type_from_array,
  det_creation_infot &info);

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

static jsont get_untyped_primitive(const jsont &json)
{
  return get_untyped(json, "value");
}

static jsont get_untyped_array(const jsont &json)
{
  return get_untyped(json, "@items");
}

/// Given a JSON representation of a (non-array) reference-typed object and a
/// type inferred from the type of a containing array, get the runtime type of
/// the corresponding pointer expression.
/// \param json: JSON representation of a non-array object. If it contains a
///   `@type` field, this takes priority over \p type_from_array. Types for non-
///   array objects are stored in the JSON in the format
///   "my.package.name.ClassName".
/// \param type_from_array: may contain an element type name given by a
///   containing array. Such types is stored in the form
///   "Lmy.package.name.ClassName;".
/// \return TODO
static optionalt<std::string> given_runtime_type(
  const jsont &json,
  const optionalt<std::string> &type_from_array)
{
  if(has_type(json))
    return "java::" + json["@type"].value;
  if(!type_from_array)
    return {};
  PRECONDITION(
    type_from_array->find('L') == 0 &&
    type_from_array->rfind(';') == type_from_array->length() - 1);
  return "java::" + type_from_array->substr(1, type_from_array->length() - 2);
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

/// Compile-time primitive type with no "@type" field.
static void assign_primitive_from_json(
  const jsont &json,
  const exprt &expr,
  code_blockt &init_body)
{
  if(json.is_null())
    return;
  if(
    expr.type() == java_int_type() || expr.type() == java_byte_type() ||
    expr.type() == java_short_type() || expr.type() == java_long_type())
  {
    const exprt rhs = from_integer(std::stoi(json.value), expr.type());
    init_body.add(code_assignt(expr, rhs));
  }
  else if(expr.type() == java_char_type())
  {
    const std::wstring wide_value = utf8_to_utf16_native_endian(json.value);
    if(wide_value.length() == 1)
    {
      const exprt rhs = from_integer(wide_value.front(), expr.type());
      init_body.add(code_assignt(expr, rhs));
    }
    else
    {
      // Hack to work around JSON parser bug: Unicode representation does not
      // get parsed correctly, e.g. \u0001 appears as "0001" in json.value.
      unsigned hex;
      std::stringstream stream;
      stream << std::hex << json.value;
      stream >> hex;
      const exprt rhs = from_integer(hex, expr.type());
      init_body.add(code_assignt(expr, rhs));
    }
  }
  else if(expr.type() == java_boolean_type())
  {
    if(json.is_true())
      init_body.add(code_assignt(expr, true_exprt()));
    else
      init_body.add(code_assignt(expr, false_exprt()));
  }
  else if(expr.type() == java_double_type())
  {
    ieee_floatt ieee_float(to_floatbv_type(expr.type()));
    ieee_float.from_double(std::stod(json.value));
    const exprt &rhs = ieee_float.to_expr();
    init_body.add(code_assignt(expr, rhs));
  }
  else if(expr.type() == java_float_type())
  {
    ieee_floatt ieee_float(to_floatbv_type(expr.type()));
    ieee_float.from_float(std::stof(json.value));
    const exprt &rhs = ieee_float.to_expr();
    init_body.add(code_assignt(expr, rhs));
  }
}

static void assign_array_data_component_from_json(
  const exprt &deref_expr,
  const jsont &json,
  const optionalt<std::string> &type_from_array,
  const java_class_typet &java_class_type,
  const typet &element_type,
  det_creation_infot &info)
{
  const jsont untyped_json = get_untyped_array(json);
  PRECONDITION(untyped_json.is_array());
  const auto &json_array =
    static_cast<const json_arrayt &>(untyped_json);
  const auto &comps = java_class_type.components();
  const member_exprt length_expr(deref_expr, "length", comps[1].type());
  exprt init_array_expr = member_exprt(deref_expr, "data", comps[2].type());

  if(init_array_expr.type() != pointer_type(element_type))
    init_array_expr =
      typecast_exprt(init_array_expr, pointer_type(element_type));

  const symbol_exprt &array_init_data =
    info.allocate_objects.allocate_automatic_local_object(
      init_array_expr.type(), "prototype_array_data_init");
  (void)array_init_data;
  code_assignt data_assign(array_init_data, init_array_expr);
  data_assign.add_source_location() = info.loc;
  info.block.add(data_assign);

  size_t index = 0;
  const optionalt<std::string> inferred_element_type =
    element_type_from_array_type(json, type_from_array);
  for(auto it = json_array.begin(); it < json_array.end(); it++)
  {
    const auto index_expr = from_integer(index, java_int_type());
    const dereference_exprt element_at_index(
      plus_exprt(array_init_data, index_expr, array_init_data.type()),
      array_init_data.type().subtype());
    assign_from_json_rec(
      element_at_index, *it, inferred_element_type, info);
    index++;
  }
}

static void allocate_array(
  const exprt &expr,
  const jsont &json,
  const exprt &array_size_expr,
  det_creation_infot &info)
{
  const pointer_typet &pointer = to_pointer_type(expr.type());
  const typet &element_type =
    static_cast<const typet &>(pointer.subtype().find(ID_element_type));
  side_effect_exprt java_new_array(ID_java_new_array, pointer, info.loc);
  java_new_array.copy_to_operands(array_size_expr);
  java_new_array.type().subtype().set(ID_element_type, element_type);
  code_assignt assign(expr, java_new_array, info.loc);
  info.block.add(assign);
}

static void assign_array_from_json(
  const exprt &expr,
  const jsont &json,
  const optionalt<exprt> &given_length_expr,
  const optionalt<std::string> &type_from_array,
  det_creation_infot &info)
{
  const jsont untyped_json = get_untyped_array(json);
  PRECONDITION(untyped_json.is_array());
  const auto &json_array =
    static_cast<const json_arrayt &>(untyped_json);
  const size_t array_size = json_array.size();
  const pointer_typet &pointer = to_pointer_type(expr.type());
  const typet &element_type =
    static_cast<const typet &>(pointer.subtype().find(ID_element_type));

  exprt length_expr;
  if(given_length_expr)
    length_expr = *given_length_expr;
  else
  {
    length_expr = info.allocate_objects.allocate_automatic_local_object(
      java_int_type(), "tmp_prototype_length");
    info.block.add(code_assignt(
      length_expr, side_effect_expr_nondett(java_int_type(), info.loc)));
    allocate_array(expr, json, length_expr, info);
  }
  const auto array_size_expr = from_integer(array_size, java_int_type());
  // TODO de-duplicate from nondet.cpp ?
  // Declare a symbol for the non deterministic integer.
  info.block.add(code_assumet(binary_predicate_exprt(
    length_expr, ID_ge, array_size_expr)));
  if(!has_nondet_length(json))
  {
    info.block.add(code_assumet(binary_predicate_exprt(
      length_expr, ID_le, array_size_expr)));
  }

  namespacet ns{info.symbol_table};
  const java_class_typet &java_class_type =
    to_java_class_type(ns.follow(pointer.subtype()));
  dereference_exprt deref_expr(expr, java_class_type);
  assign_array_data_component_from_json(
    deref_expr, json, type_from_array, java_class_type, element_type, info);
}

static void assign_enum_from_json(
  const exprt &expr,
  const jsont &json,
  const java_class_typet &java_class_type,
  det_creation_infot &info)
{
  const std::string enum_name = id2string(java_class_type.get_name());
  if(const auto func = info.symbol_table.lookup(clinit_wrapper_name(enum_name)))
    info.block.add(code_function_callt{func->symbol_expr()});
  namespacet ns{info.symbol_table};
  const irep_idt values_name = enum_name + ".$VALUES";
  // TODO add check that $VALUES is in the symbol table / refactor from JOF
  const symbolt &values = ns.lookup(values_name);

  // Access members (length and data) of $VALUES array
  dereference_exprt deref_expr(values.symbol_expr());
  const auto &deref_struct_type = to_struct_type(ns.follow(deref_expr.type()));
  PRECONDITION(is_valid_java_array(deref_struct_type));
  const auto &comps = deref_struct_type.components();
  const member_exprt length_expr(deref_expr, "length", comps[1].type());
  const member_exprt enum_array_expr =
    member_exprt(deref_expr, "data", comps[2].type());

  const exprt ordinal_expr =
    from_integer(std::stoi(json["ordinal"].value), java_int_type());

  plus_exprt plus(enum_array_expr, ordinal_expr);
  const dereference_exprt arraycellref(plus);
  info.block.add(
    code_assignt(expr, typecast_exprt(arraycellref, expr.type())));
}

static void assign_string_from_json(
  const jsont &json,
  const exprt &expr,
  det_creation_infot &info)
{
  const auto json_string = get_untyped_primitive(json);
  PRECONDITION(json_string.is_string());
  exprt literal(ID_java_string_literal);
  literal.set(ID_value, json_string.value);
  const symbol_exprt literal_symbol =
    get_or_create_string_literal_symbol(literal, info.symbol_table, true);
  info.block.add(code_assignt(expr, literal_symbol));
}

static void assign_struct_components_from_json(
  const exprt &expr,
  const jsont &json,
  const java_class_typet &java_class_type,
  det_creation_infot &info)
{
  for(const auto &component : java_class_type.components())
  {
    const typet &component_type = component.type();
    irep_idt component_name = component.get_name();
    if(
      component_name == "@class_identifier" ||
      component_name == "cproverMonitorCount")
    {
      continue;
    }
    member_exprt me(expr, component_name, component_type);
    if(component_name[0] == '@')
    {
      namespacet ns{info.symbol_table};
      const java_class_typet &base_type =
        to_java_class_type(ns.follow(me.type()));
      assign_struct_components_from_json(me, json, base_type, info);
    }
    else
    {
      const jsont member_json = json[id2string(component_name)];
      assign_from_json_rec(me, member_json, {}, info);
    }
  }
}

static void assign_struct_from_json(
  const exprt &expr,
  const jsont &json,
  const java_class_typet &java_class_type,
  det_creation_infot &info)
{
  if(
    java_string_library_preprocesst::implements_java_char_sequence(
      java_class_type) &&
    java_class_type.has_component("length") &&
    java_class_type.has_component("data"))
  {
    assign_string_from_json(json, expr, info);
    return;
  }

  namespacet ns{info.symbol_table};
  auto initial_object = zero_initializer(expr.type(), source_locationt(), ns);
  CHECK_RETURN(initial_object.has_value());
  const irep_idt qualified_clsid =
    "java::" + id2string(java_class_type.get_tag());
  set_class_identifier(
    to_struct_expr(*initial_object), ns, struct_tag_typet(qualified_clsid));
  info.block.add(code_assignt(expr, *initial_object));
  assign_struct_components_from_json(expr, json, java_class_type, info);
}

//static symbol_exprt pointer_allocation(
//  const pointer_typet &pointer,
//  det_creation_infot &info)
//{
//}

static void assign_pointer_from_json(
  const exprt &expr,
  const jsont &json,
  const pointer_typet &pointer,
  const java_class_typet &java_class_type,
  det_creation_infot &info)
{
//  namespacet ns {info.symbol_table};
//  const symbolt &outer = ns.lookup(info.class_name);
//  const auto &outer_class_type = to_java_class_type(ns.follow(outer.type));

  if(java_class_type.get_base("java::java.lang.Enum"))
//    &&!outer_class_type.get_base("java::java.lang.Enum"))
  {
    assign_enum_from_json(expr, json, java_class_type, info);
    return;
  }

  exprt init_expr = info.allocate_objects.allocate_dynamic_object(
    info.block, expr, pointer.subtype());
  assign_struct_from_json(init_expr, json, java_class_type, info);
  //    const auto &reference_expr = pointer_allocation(pointer, info);
  //    struct_assignment(
  //      json, dereference_exprt(reference_expr), java_class_type, info);
}

static pointer_typet pointer_to_subtype(
  const pointer_typet &pointer,
  const java_class_typet &replacement_class_type)
{
  if(can_cast_type<struct_tag_typet>(pointer.subtype()))
  {
    struct_tag_typet struct_tag_subtype(replacement_class_type.get_name());
    return pointer_type(struct_tag_subtype);
  }
  return pointer_type(replacement_class_type);
}

static void assign_pointer_with_given_type_from_json(
  const exprt &expr,
  const jsont &json,
  const pointer_typet &pointer,
  const java_class_typet &java_class_type,
  const std::string &runtime_type,
  det_creation_infot &info)
{
  namespacet ns{info.symbol_table};
  if(!info.symbol_table.has_symbol(runtime_type))
    return; // TODO add warning here, falling back to default value
  const auto &replacement_class_type =
    to_java_class_type(info.symbol_table.lookup(runtime_type)->type);
  pointer_typet replacement_pointer =
    pointer_to_subtype(pointer, replacement_class_type);
  if(!equal_java_types(pointer, replacement_pointer))
  {
    const auto &new_symbol =
      info.allocate_objects.allocate_automatic_local_object(
        replacement_pointer, "temp_prototype_fresh");
    if(info.needed_lazy_methods)
      info.needed_lazy_methods->add_all_needed_classes(replacement_pointer);

    assign_pointer_from_json(
      new_symbol,
      json,
      replacement_pointer,
      to_java_class_type(ns.follow(replacement_class_type)),
      info);
    info.block.add(
      code_assignt(expr, typecast_exprt(new_symbol, pointer)));
  }
  else
    assign_pointer_from_json(expr, json, pointer, java_class_type, info);
}

void assign_reference_from_json(
  const exprt &expr,
  const jsont &json,
  const pointer_typet &pointer,
  const java_class_typet &java_class_type,
  const optionalt<std::string> &type_from_array,
  det_creation_infot &info)
{
  const auto id_it = info.references.find(get_id(json));
  det_creation_referencet reference;
  if(id_it == info.references.end())
  {
    if(has_prefix(id2string(java_class_type.get_tag()), "java::array["))
    {
      reference.expr = info.allocate_objects.allocate_automatic_local_object(
        pointer, "temp_prototype_ref");
      reference.array_length =
        info.allocate_objects.allocate_automatic_local_object(
          java_int_type(), "tmp_unknown_length");
      info.block.add(code_assignt(
        *reference.array_length, side_effect_expr_nondett(java_int_type(), info.loc)));
      allocate_array(reference.expr, json, *reference.array_length, info);
      info.references.insert({get_id(json), reference});
    }
    else
    {
      reference.expr = info.allocate_objects.allocate_dynamic_object_symbol(
        info.block, expr, pointer.subtype());
//          exprt my_expr = info.allocate_objects.allocate_dynamic_object(
//            info.block, expr, pointer.subtype());
//          info.block.add(code_assignt{reference.symbol, address_of_exprt{my_expr}});
      info.references.insert({get_id(json), reference});
    }
  }
  else
    reference = id_it->second;
  if(has_id(json))
  {
    if(has_prefix(id2string(java_class_type.get_tag()), "java::array["))
    {
      assign_array_from_json(
        reference.expr, json, reference.array_length, type_from_array, info);
    }
    else
    {
      assign_struct_from_json(
        dereference_exprt(reference.expr), json, java_class_type, info);
    }
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
  if(expr.type().id() == ID_pointer)
  {
    const pointer_typet &pointer = to_pointer_type(expr.type());
    if(json.is_null())
    {
      info.block.add(code_assignt(expr, null_pointer_exprt(pointer)));
      return;
    }
    namespacet ns{info.symbol_table};
    const java_class_typet &java_class_type =
      to_java_class_type(ns.follow(pointer.subtype()));

    if(is_reference(json) || has_id(json))
    {
      assign_reference_from_json(
        expr, json, pointer, java_class_type, type_from_array, info);
      return;
    }

    if(has_prefix(id2string(java_class_type.get_tag()), "java::array["))
    {
      assign_array_from_json(expr, json, {}, type_from_array, info);
      return;
    }
    const auto runtime_type = given_runtime_type(json, type_from_array);
    if(runtime_type)
    {
      assign_pointer_with_given_type_from_json(
        expr, json, pointer, java_class_type, *runtime_type, info);
      return;
    }
    assign_pointer_from_json(expr, json, pointer, java_class_type, info);
  }
  else
    assign_primitive_from_json(
      get_untyped_primitive(json), expr, info.block);
}

void assign_from_json(
  const exprt &expr,
  const jsont &json,
  const irep_idt &class_name,
  code_blockt &assignments,
  symbol_table_baset &symbol_table,
  optionalt<ci_lazy_methods_neededt> &needed_lazy_methods,
  std::unordered_map<std::string, det_creation_referencet> &references,
  const source_locationt &loc)
{
  allocate_objectst allocate(
    ID_java,
    loc,
    id2string(class_name) + "::fast_clinit",
    symbol_table);
  code_blockt body_rec;
  det_creation_infot info{
    body_rec, allocate, symbol_table, needed_lazy_methods, references, loc};
  assign_from_json_rec(expr, json, {}, info);
  allocate.declare_created_symbols(assignments);
  assignments.append(body_rec);
}
