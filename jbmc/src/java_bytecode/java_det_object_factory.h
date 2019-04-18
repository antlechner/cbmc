/*******************************************************************\

Module:

Author: Diffblue Ltd.

\*******************************************************************/

/// \file

#ifndef CPROVER_JAVA_BYTECODE_JAVA_DET_OBJECT_FACTORY_H
#define CPROVER_JAVA_BYTECODE_JAVA_DET_OBJECT_FACTORY_H

#include <util/std_code.h>

/// Information to store when several references point to the same Java object.
struct det_creation_referencet
{
  /// Expression for the symbol that stores the value that may be reference
  /// equal to other values.
  exprt expr;

  /// If `symbol` is an array, this expression stores its length.
  optionalt<symbol_exprt> array_length;
};

class jsont;
class symbol_table_baset;
class ci_lazy_methods_neededt;
class message_handlert;

/// Given an expression \p expr representing a Java object or primitive and a
/// JSON representation \p json of a Java object or primitive of a compatible
/// type, adds statements to \p block to assign \p expr to the deterministic
/// value specified by \p json.
void assign_from_json(
  const exprt &expr,
  const jsont &json,
  const irep_idt &class_name,
  code_blockt &block,
  symbol_table_baset &symbol_table,
  optionalt<ci_lazy_methods_neededt> &needed_lazy_methods,
  size_t max_user_array_length,
  std::unordered_map<std::string, det_creation_referencet> &references,
  const source_locationt &loc);

#endif //CPROVER_JAVA_BYTECODE_JAVA_DET_OBJECT_FACTORY_H
