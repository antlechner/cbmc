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
#include "format_specifier_date.h"

typedef string_constraint_generatort::format_elementt format_elementt;
typedef string_constraint_generatort::fixed_textt fixed_textt;
typedef string_constraint_generatort::format_specifiert format_specifiert;
typedef string_constraint_generatort::format_specifier_stringt
  format_specifier_stringt;
typedef string_constraint_generatort::format_specifier_datet
  format_specifier_datet;

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
exprt string_constraint_generatort::add_axioms_for_string_format(
  const array_string_exprt &res,
  const std::string &s,
  const exprt::operandst &args)
{
  format_specifier_stringt specifier;
  return specifier.add_axioms_for_general_format(*this, res, s, args);
}

/// Parse `s` and add axioms ensuring the output corresponds to the output of
/// SimpleDateFormat.format.
/// \param res: string expression for the result of the format function
/// \param s: a format string
/// \param args: a vector of arguments
/// \return code, 0 on success
exprt string_constraint_generatort::add_axioms_for_date_format(
  const array_string_exprt &res,
  const std::string &s,
  const exprt::operandst &args)
{
  format_specifier_datet specifier;
  return specifier.add_axioms_for_general_format(*this, res, s, args);
}

/// Construct a string from a constant array.
/// \param arr: an array expression containing only constants
/// \param length: an unsigned value representing the length of the array
/// \return String of length `length` represented by the array assuming each
///   field in `arr` represents a character.
static std::string
array_exprt_to_string(const array_exprt &arr, std::size_t length)
{
  std::string out(length, '?');
  unsigned int c;
  for(std::size_t i=0; i<arr.operands().size() && i<length; i++)
  {
    PRECONDITION(arr.operands()[i].id()==ID_constant);
    bool conversion_failed=to_unsigned_integer(
      to_constant_expr(arr.operands()[i]), c);
    INVARIANT(!conversion_failed, "constant should be convertible to unsigned");
    out[i]=c;
  }
  return out;
}

/// Construct a string from a constant array, escaping characters where
/// necessary. To be used in debugging output.
/// \param arr: an array expression containing only constants
/// \param length: an unsigned value representing the length of the array
/// \return String of length `length` represented by the array assuming each
///   field in `arr` represents a character.
std::string
utf16_constant_array_to_java(const array_exprt &arr, std::size_t length)
{
  std::string s(array_exprt_to_string(arr, length));
  return utf16_little_endian_to_java(
      std::wstring(s.begin(), s.end()));
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
exprt string_constraint_generatort::add_axioms_for_string_format(
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
    std::string s=array_exprt_to_string(
      to_array_expr(format_string.content()), *length);
    // List of arguments after s
    std::vector<exprt> args(f.arguments().begin() + 3, f.arguments().end());
    return add_axioms_for_string_format(res, s, args);
  }
  else
  { // format_string is nondeterministic
    message.warning()
      << "ignoring format function with non constant first argument"
      << message.eom;
    return from_integer(100, f.type());
  }
}

/// Formatted string using a date format string
///
/// Add axioms to specify the Java String.format function.
/// \todo This is correct only if the argument at index 2 (ie the format string)
///   is of type constant_exprt.
/// \param f: A function application whose first two arguments store the result
///   of an application of java.text.SimpleDateFormat.format. The argument at
///   index 2 is the format string.
///   Axioms are added to the result, i.e. the first two arguments of this
///   function application.
/// \return code, 0 on success
exprt string_constraint_generatort::add_axioms_for_date_format(
  const function_application_exprt &f)
{
  // Result and format string have to be present, no further arguments after
  // that
  PRECONDITION(f.arguments().size() == 3);
  const array_string_exprt res =
    char_array_of_pointer(f.arguments()[1], f.arguments()[0]);
  const array_string_exprt format_string = get_string_expr(f.arguments()[2]);
  const auto length = numeric_cast<unsigned>(format_string.length());

  if(length && format_string.content().id()==ID_array)
  { // format_string is constant
    std::string s=array_exprt_to_string(
      to_array_expr(format_string.content()), *length);
    // List of arguments after s (empty for now)
    std::vector<exprt> args;
    return add_axioms_for_date_format(res, s, args);
  }
  else
  { // format_string is nondeterministic
    message.warning()
      << "ignoring format function with non constant first argument"
      << message.eom;
    return from_integer(100, f.type());
  }
}
