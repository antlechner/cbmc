#include "java_det_object_factory.h"

#include <util/expr_initializer.h>
#include <util/nondet.h>
#include <util/nondet_bool.h>
#include <util/pointer_offset_size.h>
#include <util/prefix.h>

#include <goto-programs/class_identifier.h>
#include <goto-programs/goto_functions.h>

#include "generic_parameter_specialization_map_keys.h"
#include "java_root_class.h"
#include "java_string_literals.h"
#include "java_utils.h"

static void static_assignments_from_json_rec(
  const jsont &json,
  const exprt &expr,
  const irep_idt &class_name,
  code_blockt &init_body,
  symbol_table_baset &symbol_table)
{
  if(expr.type().id() == ID_pointer)
  {
    const pointer_typet &pointer_type = to_pointer_type(expr.type());
    if(json.is_null()) {
      init_body.add(code_assignt(expr, null_pointer_exprt(pointer_type)));
      return;
    }
    namespacet ns{symbol_table};
    allocate_objectst allocate_objects(ID_java, source_locationt(), clinit_wrapper_name(class_name), symbol_table);
    exprt init_expr = allocate_objects.allocate_object(init_body, expr, pointer_type.subtype(), lifetimet::DYNAMIC, "tmp_prototype");
    const struct_typet &struct_type = to_struct_type(ns.follow(init_expr.type()));
    auto initial_object =
      zero_initializer(init_expr.type(), source_locationt(), ns);
    CHECK_RETURN(initial_object.has_value());
    const irep_idt qualified_clsid = "java::" + id2string(struct_type.get_tag());
    set_class_identifier(
      to_struct_expr(*initial_object), ns, struct_tag_typet(qualified_clsid));
    init_body.add(code_assignt(init_expr, *initial_object));
    for(const auto &component : struct_type.components()) {
      if(component == *struct_type.components().begin())
        continue;
      const typet &component_type = component.type();
      irep_idt component_name = component.get_name();
      if(component_name == "@class_identifier" || component_name == "cproverMonitorCount")
        continue;
      member_exprt me(init_expr, component_name, component_type);
      const jsont member_json = json[id2string(component_name)];
      static_assignments_from_json_rec(member_json, me, class_name, init_body, symbol_table);
    }
  }
  else if(expr.type() == java_int_type())
  {
    const std::string &number_string = json.value;
    const exprt &rhs = from_integer(std::stoi(number_string), java_int_type());
    const code_assignt assignment(expr, rhs);
    init_body.add(assignment);
  }
}

void static_assignments_from_json(
  const jsont &json,
  const symbol_exprt &expr,
  const irep_idt &class_name,
  code_blockt &init_body,
  symbol_table_baset &symbol_table,
  const source_locationt &loc)
{
  if(expr.type().id() == ID_pointer)
    static_assignments_from_json_rec(json, expr, class_name, init_body, symbol_table);
  else
  {
//    const std::string &type_name = json["@type"].value;
//    if(type_name == "int")
    if(expr.type() == java_int_type())
    {
      const std::string &number_string = json["value"].value;
      const exprt &rhs = from_integer(std::stoi(number_string), java_int_type());
      const code_assignt assignment(expr, rhs);
      init_body.add(assignment);
    }
  }
}
