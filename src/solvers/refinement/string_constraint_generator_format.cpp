/*******************************************************************\

Module: Generates string constraints for the Java format function

Author: Romain Brenguier

Date:   May 2017

\*******************************************************************/

/// \file
/// Generates string constraints for the Java format function
/// \todo We currently use the following return codes:
///     0: success
///     1: whenever a java.util.UnknownFormatConversionException would be thrown
///     2: whenever a java.util.MissingFormatArgumentException would be thrown
///   100: correct Java code which we do not yet support
///   These should be declared in a different file and given more meaningful
///   names.

#include <iomanip>
#include <string>
#include <regex>
#include <vector>

#include <util/make_unique.h>
#include <util/std_expr.h>
#include <util/unicode.h>

#include "string_constraint_generator.h"

#include "format_element.h"
#include "fixed_text.h"
#include "format_specifier_string.h"

typedef string_constraint_generatort::format_elementt format_elementt;
typedef string_constraint_generatort::fixed_textt fixed_textt;
typedef string_constraint_generatort::format_specifiert format_specifiert;
typedef string_constraint_generatort::format_specifier_stringt
  format_specifier_stringt;

#if 0
// This code is deactivated as it is not used for now, but ultimately this
// should be used to throw an exception when the format string is not correct
/// Used to check first argument of `String.format` is correct.
/// \param s: a string
/// \return True if the argument is a correct format string. Any '%' found
///   between format specifier means the string is invalid.
static bool check_format_string(std::string s)
{
  std::string format_specifier=
      "%(\\d+\\$)?([-#+ 0,(\\<]*)?(\\d+)?(\\.\\d+)?([tT])?([a-zA-Z%])";
  std::regex regex(format_specifier);
  std::smatch match;

  while(std::regex_search(s, match, regex))
  {
    if(match.position()!= 0)
      for(const auto &c : match.str())
        if(c=='%')
          return false;
    s=match.suffix();
  }

  for(const auto &c : s)
    if(c=='%')
      return false;

  return true;
}
#endif

/// Parse `s` and add axioms ensuring the output corresponds to the output of
/// String.format.
/// \param res: string expression for the result of the format function
/// \param s: a format string
/// \param args: a vector of arguments
/// \return code, 0 on success
exprt string_constraint_generatort::add_axioms_for_format(
  const array_string_exprt &res,
  const std::string &s,
  const exprt::operandst &args)
{
  // Split s into its format elements
  const auto format_strings=format_specifier_stringt::parse_format_string(s);
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
          *this, arg_count, index_type, char_type, args);
    if(code_format_pair.first != from_integer(0, get_return_code_type()))
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
    add_axioms_for_constant(res, "");
    return from_integer(0, get_return_code_type());
  }

  auto it = formatted_elements.begin();
  array_string_exprt str = *(it++);
  exprt return_code = from_integer(0, get_return_code_type());
  for(; it != formatted_elements.end(); ++it)
  {
    const array_string_exprt fresh = fresh_string(index_type, char_type);
    // fresh is the result of concatenating str and it
    /// \todo `add_axioms_for_concat` currently always returns zero. In the
    ///   future we might want it to return other values depending on whether or
    ///   not the concatenation succeeded. For example, it will not succeed if
    ///   concatenating the two strings would exceed the value specified for
    ///   string-max-length.
    exprt concat_return_code = add_axioms_for_concat(fresh, str, *it);
    return_code =
      if_exprt(
          equal_exprt(return_code, from_integer(0, get_return_code_type())),
          concat_return_code,
          return_code);
    str = fresh;
  }
  // Copy
  add_axioms_for_substring(res, str, from_integer(0, index_type), str.length());
  return return_code;
}

/// Construct a string from a constant array.
/// \param arr: an array expression containing only constants
/// \param length: an unsigned value representing the length of the array
/// \return String of length `length` represented by the array assuming each
///   field in `arr` represents a character.
std::string
utf16_constant_array_to_java(const array_exprt &arr, std::size_t length)
{
  std::wstring out(length, '?');
  unsigned int c;
  for(std::size_t i=0; i<arr.operands().size() && i<length; i++)
  {
    PRECONDITION(arr.operands()[i].id()==ID_constant);
    bool conversion_failed=to_unsigned_integer(
      to_constant_expr(arr.operands()[i]), c);
    INVARIANT(!conversion_failed, "constant should be convertible to unsigned");
    out[i]=c;
  }
  return utf16_little_endian_to_java(out);
}

/// Formatted string using a format string and list of arguments
///
/// Add axioms to specify the Java String.format function.
/// \todo This is correct only if the argument at index 2 (ie the format string)
///   is of type constant_exprt.
/// \param f: A function application whose first two arguments store the result
///   of an application of java.lang.String.format. Its remaining arguments
///   correspond to the arguments passed to this call to String.format. That is,
///   the argument at index 2 is the format string, and arguments from index 3
///   onwards are elements of what is called the argument list in
///   https://docs.oracle.com/javase/7/docs/api/java/util/Formatter.html#syntax
///   Axioms are added to the result, i.e. the first two arguments of this
///   function application.
/// \return code, 0 on success
exprt string_constraint_generatort::add_axioms_for_format(
  const function_application_exprt &f)
{
  // Result and format string have to be present, argument list may be empty
  PRECONDITION(f.arguments().size() >= 3);
  const array_string_exprt res =
    char_array_of_pointer(f.arguments()[1], f.arguments()[0]);
  const array_string_exprt format_string = get_string_expr(f.arguments()[2]);
  const auto length = numeric_cast<unsigned>(format_string.length());

  if(length && format_string.content().id()==ID_array)
  { // format_string is constant
    std::string s=utf16_constant_array_to_java(
      to_array_expr(format_string.content()), *length);
    // List of arguments after s
    std::vector<exprt> args(f.arguments().begin() + 3, f.arguments().end());
    return add_axioms_for_format(res, s, args);
  }
  else
  { // format_string is nondeterministic
    message.warning()
      << "ignoring format function with non constant first argument"
      << message.eom;
    return from_integer(100, f.type());
  }
}
