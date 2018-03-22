/*******************************************************************\

 Module: String solver

 Author: Diffblue Ltd.

\*******************************************************************/


#include <iomanip>
#include <string>
#include <regex>
#include <vector>

#include "format_specifier_string.h"
#include "format_element.h"
#include "format_specifier.h"
#include "fixed_text.h"

#include <util/make_unique.h>

typedef string_constraint_generatort::format_specifiert format_specifiert;

exprt format_specifiert::add_axioms_for_general_format(
  string_constraint_generatort &gen,
  const array_string_exprt &res,
  const std::string &format_string,
  const exprt::operandst &args)
{
  // Split s into its format elements
  const auto format_strings=parse_format_string(format_string);
  // We will format each format element according to the specification of
  // java.lang.String.format and store the results in a temporary vector
  // variable.
  std::vector<array_string_exprt> formatted_elements;
  // Number of format specifiers that we have processed that did not specify an
  // argument index
  std::size_t arg_count=0;
  const typet &char_type = res.content().type().subtype();
  const typet &index_type = res.length().type();

  for(const auto &format : format_strings)
  {
    const auto code_format_pair =
      format->add_axioms_for_format_element(
          gen, arg_count, index_type, char_type, args);
    if(code_format_pair.first != from_integer(0, gen.get_return_code_type()))
    {
      // A nonzero exit code means an exception was thrown.
      /// \todo Add support for exceptions
      return code_format_pair.first;
    }
    const auto string_expr = code_format_pair.second;
    formatted_elements.push_back(string_expr);
  }

  if(formatted_elements.empty())
  {
    // Formatting an empty string results in an empty string
    gen.add_axioms_for_constant(res, "");
    return from_integer(0, gen.get_return_code_type());
  }

  auto it = formatted_elements.begin();
  array_string_exprt str = *(it++);
  exprt return_code = from_integer(0, gen.get_return_code_type());
  for(; it != formatted_elements.end(); ++it)
  {
    const array_string_exprt fresh = gen.fresh_string(index_type, char_type);
    // fresh is the result of concatenating str and it
    /// \todo `add_axioms_for_concat` currently always returns zero. In the
    ///   future we might want it to return other values depending on whether or
    ///   not the concatenation succeeded. For example, it will not succeed if
    ///   concatenating the two strings would exceed the value specified for
    ///   string-max-length.
    exprt concat_return_code = gen.add_axioms_for_concat(fresh, str, *it);
    return_code =
      if_exprt(
          equal_exprt(return_code, from_integer(0, gen.get_return_code_type())),
          concat_return_code,
          return_code);
    str = fresh;
  }
  // Copy
  gen.add_axioms_for_substring(res, str, from_integer(0, index_type), str.length());
  return return_code;
}
