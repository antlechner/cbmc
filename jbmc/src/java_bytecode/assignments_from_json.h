/*******************************************************************\

Module:

Author: Diffblue Ltd.

\*******************************************************************/

/// \file

#ifndef CPROVER_JAVA_BYTECODE_ASSIGNMENTS_FROM_JSON_H
#define CPROVER_JAVA_BYTECODE_ASSIGNMENTS_FROM_JSON_H

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

/// Given an expression \p expr representing a Java object or primitive and a
/// JSON representation \p json of the value of a Java object or primitive of a
/// compatible type, adds statements to \p block to assign \p expr to the
/// deterministic value specified by \p json.
/// The expected format of the JSON representation is mostly the same as that
/// of the json-io serialization library (https://github.com/jdereg/json-io) if
/// run with the following options, as of version 4.10.1:
/// - A type name map with identity mappings such as
///   `("java.lang.Boolean", "java.lang.Boolean")` for all primitive wrapper
///   types, java.lang.Class, java.lang.String and java.util.Date. That is, we
///   are not using the json-io default shorthands for those types.
/// - `WRITE_LONGS_AS_STRINGS` should be set to `true` to avoid a loss of
///   precision when representing longs.
/// This rule has the following exceptions:
/// - It seems that strings are always printed in "primitive" representation by
///   json-io, i.e.\ they are always JSON strings, and never JSON objects with
///   a `@type` key. For cases where we don't know that an expression has a
///   string type (e.g.\ if its type is generic and specialized to
///   java.lang.String), we need to sometimes represent strings as JSON objects
///   with a `@type` key. In this case, the content of the string will be the
///   value associated with a `value` key (similarly to StringBuilder in
///   json-io).
/// - json-io does not include the `ordinal` field of enums in its
///   representation, but our algorithm depends on it being present. It may be
///   possible to rewrite parts of it to set the ordinal depending on the order
///   of elements seen in the `$VALUES` array, but it would generally make
///   things more complicated.
/// For examples of JSON representations of objects, see the regression tests
/// for this feature in
/// jbmc/regression/jbmc/deterministic_assignments_json.
void assign_from_json(
  const exprt &expr,
  const jsont &json,
  const irep_idt &function_name,
  code_blockt &block,
  symbol_table_baset &symbol_table,
  optionalt<ci_lazy_methods_neededt> &needed_lazy_methods,
  size_t max_user_array_length,
  std::unordered_map<std::string, det_creation_referencet> &references);

#endif //CPROVER_JAVA_BYTECODE_ASSIGNMENTS_FROM_JSON_H
