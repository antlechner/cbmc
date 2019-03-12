#ifndef CPROVER_JAVA_BYTECODE_JAVA_DET_OBJECT_FACTORY_H
#define CPROVER_JAVA_BYTECODE_JAVA_DET_OBJECT_FACTORY_H

#include "java_bytecode_language.h"
#include "select_pointer_type.h"

#include <util/allocate_objects.h>
#include <util/message.h>
#include <util/std_code.h>
#include <util/symbol_table.h>

void static_assignments_from_json(
  const jsont &json,
  const exprt &expr,
  const irep_idt &class_name,
  code_blockt &init_body,
  symbol_table_baset &symbol_table,
  const source_locationt &loc);

#endif //CPROVER_JAVA_BYTECODE_JAVA_DET_OBJECT_FACTORY_H
