/*******************************************************************\

Module: Java Static Initializers

Author: Chris Smowton, chris.smowton@diffblue.com

\*******************************************************************/

#ifndef CPROVER_JAVA_BYTECODE_JAVA_STATIC_INITIALIZERS_H
#define CPROVER_JAVA_BYTECODE_JAVA_STATIC_INITIALIZERS_H

#include "assignments_from_json.h"
#include "ci_lazy_methods_needed.h"
#include "java_object_factory_parameters.h"
#include "select_pointer_type.h"
#include "synthetic_methods_map.h"

#include <unordered_set>

#include <util/message.h>
#include <util/std_code.h>
#include <util/symbol_table.h>

/// General exception for something invalid with the JSON file
class static_field_list_errort : public std::runtime_error
{
public:
  explicit static_field_list_errort(const std::string &error_message)
    : std::runtime_error(error_message)
  {
  }
};

irep_idt clinit_wrapper_name(const irep_idt &class_name);
irep_idt json_clinit_name(const irep_idt &class_name);

bool is_clinit_wrapper_function(const irep_idt &function_id);

void create_static_initializer_symbols(
  symbol_tablet &symbol_table,
  synthetic_methods_mapt &synthetic_methods,
  const bool thread_safe,
  const std::string &static_values_file);

code_blockt get_thread_safe_clinit_wrapper_body(
  const irep_idt &function_id,
  symbol_table_baset &symbol_table,
  const bool nondet_static,
  const std::string &static_values_file,
  const java_object_factory_parameterst &object_factory_parameters,
  const select_pointer_typet &pointer_type_selector,
  message_handlert &message_handler);

code_ifthenelset get_clinit_wrapper_body(
  const irep_idt &function_id,
  symbol_table_baset &symbol_table,
  const bool nondet_static,
  const std::string &static_values_file,
  const java_object_factory_parameterst &object_factory_parameters,
  const select_pointer_typet &pointer_type_selector,
  message_handlert &message_handler);

/// Creates the body of a json_clinit function, which includes assignments for
/// all static fields of a class to values read from a JSON file. If the JSON
/// file could not be parsed, the function will only include a call to the
/// "real" clinit function, and not include any assignments itself.
code_blockt get_json_clinit_body(
  const irep_idt &function_id,
  const std::string &static_values_file,
  symbol_table_baset &symbol_table,
  optionalt<ci_lazy_methods_neededt> needed_lazy_methods,
  size_t max_user_array_length,
  message_handlert &message_handler,
  std::unordered_map<std::string, det_creation_referencet> &references);

class stub_global_initializer_factoryt
{
  /// Maps class symbols onto the stub globals that belong to them
  typedef std::unordered_multimap<irep_idt, irep_idt> stub_globals_by_classt;
  stub_globals_by_classt stub_globals_by_class;

public:
  void create_stub_global_initializer_symbols(
    symbol_tablet &symbol_table,
    const std::unordered_set<irep_idt> &stub_globals_set,
    synthetic_methods_mapt &synthetic_methods);

  code_blockt get_stub_initializer_body(
    const irep_idt &function_id,
    symbol_table_baset &symbol_table,
    const java_object_factory_parameterst &object_factory_parameters,
    const select_pointer_typet &pointer_type_selector,
    message_handlert &message_handler);
};

void create_stub_global_initializers(
  symbol_tablet &symbol_table,
  const std::unordered_set<irep_idt> &stub_globals_set,
  const java_object_factory_parameterst &object_factory_parameters,
  const select_pointer_typet &pointer_type_selector);

#endif
