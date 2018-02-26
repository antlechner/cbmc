/*******************************************************************\

 Module: String solver

 Author: Diffblue Ltd.

\*******************************************************************/

/// \file
/// Collect methods needed to be loaded using the lazy method


#include "format_element_invalid.h"

typedef string_constraint_generatort::format_element_invalidt
  format_element_invalidt;

format_element_invalidt::format_element_invalidt(unsigned _return_code):
  return_code(_return_code) { }

  unsigned format_element_invalidt::get_code() const
{
  return return_code;
}

/// Given a format element, add axioms ensuring the output corresponds to the
/// output of String.format applied to that specifier with the given argument
/// list.
/// In the case of fixed text, we simply add an axiom for a constant string
/// storing the value corresponding to the fixed text.
/// \param gen: a `string_constraint_generatort` object (used for accessing
///   some of the member functions of this class)
/// \param index_type: type for indices in strings
/// \param char_type: type of characters in strings
/// \return Pair consisting of return code and string expression representing
/// the output of String.format. The return code is 0 on success, and the
/// string expression represents the constant content of the fixed text.
std::pair<exprt, array_string_exprt>
format_element_invalidt::add_axioms_for_format_element(
  string_constraint_generatort &gen,
  std::size_t &,
  const typet &index_type,
  const typet &char_type,
  const exprt::operandst &)
{
  const array_string_exprt res = gen.fresh_string(index_type, char_type);
  return {from_integer(return_code, gen.get_return_code_type()), res};
}

