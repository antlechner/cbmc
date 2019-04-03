#ifndef CPROVER_JAVA_BYTECODE_JAVA_DET_OBJECT_FACTORY_H
#define CPROVER_JAVA_BYTECODE_JAVA_DET_OBJECT_FACTORY_H

#include "ci_lazy_methods_needed.h"
#include "select_pointer_type.h"

#include <util/allocate_objects.h>
#include <util/message.h>
#include <util/std_code.h>
#include <util/symbol_table.h>

struct det_creation_referencet
{
  /// Expression for the symbol that stores the value that may be reference
  /// equal to other values.
  exprt symbol;
  /// If `symbol` is an arrary, this expression stores its length.
  optionalt<exprt> array_length;
};

void static_assignments_from_json(
  const jsont &json,
  const exprt &expr,
  const irep_idt &class_name,
  code_blockt &init_body,
  symbol_table_baset &symbol_table,
  optionalt<ci_lazy_methods_neededt> &needed_lazy_methods,
  std::unordered_map<std::string, det_creation_referencet> &references,
  const source_locationt &loc);

#endif //CPROVER_JAVA_BYTECODE_JAVA_DET_OBJECT_FACTORY_H
