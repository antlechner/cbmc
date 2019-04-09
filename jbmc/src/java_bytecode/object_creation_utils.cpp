#include "object_creation_utils.h"
#include "java_types.h"

#include <goto-programs/class_identifier.h>
#include <util/expr_initializer.h>
#include <util/namespace.h>
#include <util/std_expr.h>

//exprt zero_initialize_with_class_identifier(
//  const exprt &expr,
//  const source_locationt &location,
//  const namespacet &ns)
//{
//  // This code mirrors the `remove_java_new` pass:
//  const java_class_typet &java_class_type =
//    to_java_class_type(ns.follow(expr.type()));
//  auto initial_object = zero_initializer(expr.type(), source_locationt(), ns);
//  CHECK_RETURN(initial_object.has_value());
//  const irep_idt qualified_clsid =
//    "java::" + id2string("java::" + id2string(java_class_type.get_tag()));
//  set_class_identifier(
//    to_struct_expr(*initial_object), ns, struct_tag_typet(qualified_clsid));
//  return *initial_object;
//}
