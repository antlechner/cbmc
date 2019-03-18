#include "java_det_object_factory.h"

#include <util/expr_initializer.h>
#include <util/nondet.h>
#include <util/nondet_bool.h>
#include <util/pointer_offset_size.h>
#include <util/prefix.h>
#include <util/unicode.h>

#include <goto-programs/class_identifier.h>
#include <goto-programs/goto_functions.h>

#include "generic_parameter_specialization_map_keys.h"
#include "java_root_class.h"
#include "java_string_literals.h"
#include "java_utils.h"

static bool has_type(const jsont &json)
{
  return json.is_object() &&
         static_cast<const json_objectt &>(json).find("@type") !=
           static_cast<const json_objectt &>(json).end();
}

/// For typed versions of non-reference types (primitive, string or array types)
/// we retrieve their untyped contents by looking them up with the key
/// specific to their type.
static jsont get_untyped(const jsont &json, const std::string &object_key)
{
  if(has_type(json))
  {
    return json[object_key];
  }
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

/// Compile-time primitive type with no "@type" field.
static void primitive_assignment(
  const exprt &expr,
  const jsont &json,
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

static void array_assignment(
  const jsont &json,
  const exprt &expr,
  const irep_idt &class_name,
  code_blockt &init_body,
  symbol_table_baset &symbol_table,
  const source_locationt &loc)
{
  PRECONDITION(json.is_array());
  const json_arrayt &json_array = static_cast<const json_arrayt&>(json);
  const size_t array_size = json_array.size();
  const pointer_typet &pointer = to_pointer_type(expr.type());
  namespacet ns{symbol_table};
  const java_class_typet &java_class_type =
    to_java_class_type(ns.follow(pointer.subtype()));
  const typet &element_type =
    static_cast<const typet &>(pointer.subtype().find(ID_element_type));

  const auto array_size_expr = from_integer(array_size, java_int_type());
  side_effect_exprt java_new_array(ID_java_new_array, pointer, loc);
  java_new_array.copy_to_operands(array_size_expr);
  java_new_array.set(ID_length_upper_bound, array_size_expr);
  java_new_array.type().subtype().set(ID_element_type, element_type);
  code_assignt assign(expr, java_new_array);
  assign.add_source_location() = loc;
  init_body.add(assign);

  dereference_exprt deref_expr(expr, java_class_type);
  const auto &comps = java_class_type.components();
  const member_exprt length_expr(deref_expr, "length", comps[1].type());
  exprt init_array_expr = member_exprt(deref_expr, "data", comps[2].type());

  if(init_array_expr.type() != pointer_type(element_type))
    init_array_expr =
      typecast_exprt(init_array_expr, pointer_type(element_type));

  allocate_objectst allocate_objects(
    ID_java,
    source_locationt(),
    clinit_wrapper_name(class_name),
    symbol_table);
  const symbol_exprt &array_init_data =
    allocate_objects.allocate_automatic_local_object(
      init_array_expr.type(), "array_data_init");
  (void) array_init_data;
  code_assignt data_assign(array_init_data, init_array_expr);
  data_assign.add_source_location()=loc;
  init_body.add(data_assign);

  size_t index = 0;
  for(auto it = json_array.begin(); it < json_array.end(); it++)
  {
    const auto index_expr = from_integer(index, java_int_type());
    const dereference_exprt element_at_index(
      plus_exprt(array_init_data, index_expr, array_init_data.type()),
      array_init_data.type().subtype());
    static_assignments_from_json(
      *it, element_at_index, class_name, init_body, symbol_table, loc);
    index++;
  }
}

static void string_assignment(
  const jsont &json,
  const exprt &expr,
  const irep_idt &class_name,
  code_blockt &init_body,
  symbol_table_baset &symbol_table,
  const source_locationt &loc)
{
  PRECONDITION(json.is_string());
  exprt literal(ID_java_string_literal);
  literal.set(ID_value, json.value);
  const symbol_exprt literal_symbol =
    get_or_create_string_literal_symbol(literal, symbol_table, true);
  init_body.add(code_assignt(expr, literal_symbol));
}

static void components_assignment(
  const jsont &json,
  const exprt &expr,
  const irep_idt &class_name,
  code_blockt &init_body,
  const java_class_typet &java_class_type,
  symbol_table_baset &symbol_table,
  const source_locationt &loc)
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
      namespacet ns{symbol_table};
      const java_class_typet &base_type = to_java_class_type(ns.follow(me.type()));
      components_assignment(json, me, class_name, init_body, base_type, symbol_table, loc);
    }
    else
    {
      const jsont member_json=json[id2string(component_name)];
      static_assignments_from_json(
        member_json, me, class_name, init_body, symbol_table, loc);
    }
  }
}

static void struct_assignment(
  const jsont &json,
  const exprt &expr,
  const irep_idt &class_name,
  code_blockt &init_body,
  const java_class_typet &java_class_type,
  symbol_table_baset &symbol_table,
  const source_locationt &loc)
{
  if(
    java_string_library_preprocesst::implements_java_char_sequence(
      java_class_type) &&
    java_class_type.has_component("length") &&
    java_class_type.has_component("data"))
  {
    string_assignment(json, expr, class_name, init_body, symbol_table, loc);
    return;
  }

  namespacet ns{symbol_table};
  auto initial_object =
    zero_initializer(expr.type(), source_locationt(), ns);
  CHECK_RETURN(initial_object.has_value());
  const irep_idt qualified_clsid =
    "java::" + id2string(java_class_type.get_tag());
  set_class_identifier(
    to_struct_expr(*initial_object), ns, struct_tag_typet(qualified_clsid));
  init_body.add(code_assignt(expr, *initial_object));
  components_assignment(
    json, expr, class_name, init_body, java_class_type, symbol_table, loc);
}

void static_assignments_from_json(
  const jsont &json,
  const exprt &expr,
  const irep_idt &class_name,
  code_blockt &init_body,
  symbol_table_baset &symbol_table,
  const source_locationt &loc)
{
  if(expr.type().id() == ID_pointer)
  {
    const pointer_typet &pointer_type = to_pointer_type(expr.type());
    if(json.is_null())
    {
      init_body.add(code_assignt(expr, null_pointer_exprt(pointer_type)));
      return;
    }
    namespacet ns{symbol_table};
    const java_class_typet &java_class_type =
      to_java_class_type(ns.follow(pointer_type.subtype()));

    if(has_prefix(id2string(java_class_type.get_tag()), "java::array["))
    {
      array_assignment(
        get_untyped_array(json), expr, class_name, init_body, symbol_table, loc);
      return;
    }

    allocate_objectst allocate_objects(
      ID_java,
      source_locationt(),
      clinit_wrapper_name(class_name),
      symbol_table);
    exprt init_expr = allocate_objects.allocate_object(
      init_body,
      expr,
      pointer_type.subtype(),
      lifetimet::DYNAMIC,
      "tmp_prototype");

    struct_assignment(
      json,
      init_expr,
      class_name,
      init_body,
      java_class_type,
      symbol_table,
      loc);
  }
  else
  {
    primitive_assignment(expr, get_untyped_primitive(json), init_body);
  }
}
