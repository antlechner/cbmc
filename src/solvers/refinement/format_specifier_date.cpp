/*******************************************************************\

 Module: String solver

 Author: Diffblue Ltd.

\*******************************************************************/


#include <algorithm>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "format_specifier_date.h"
#include "format_element.h"
#include "format_specifier.h"
#include "fixed_text.h"
#include "format_element_invalid.h"

#include <util/make_unique.h>

typedef string_constraint_generatort::format_elementt format_elementt;
typedef string_constraint_generatort::fixed_textt fixed_textt;
typedef string_constraint_generatort::format_element_invalidt
  format_element_invalidt;
typedef string_constraint_generatort::format_specifiert format_specifiert;
typedef string_constraint_generatort::format_specifier_datet
  format_specifier_datet;

format_specifier_datet::format_specifier_datet(
  char letter,
  unsigned _length):
    pattern_letter(letter),
    length(_length)
{ }

/// Parse the given date pattern into format specifiers and text.
/// \param pattern: a string storing a date pattern
/// \return a pair whose first element is a boolean value (true if there is an
///         even number of quotes in the string, false otherwise) and whose
///         second element is a vector of format_elementt.
std::vector<std::unique_ptr<format_elementt>>
format_specifier_datet::parse_format_string(std::string pattern)
{
  std::vector<std::unique_ptr<format_elementt>> elements;
  size_t i = 0;
  while(i < pattern.length())
  {
    const char current = pattern.at(i);
    if(isalpha(current)) // start of a (valid or invalid) pattern letter
    {
      const size_t j = pattern.find_first_not_of(current, i+1);
      elements.push_back(
          util_make_unique<format_specifier_datet>(
            current, (j != pattern.npos) ? j-i : pattern.length() - i));
      // set index to the character just after the pattern letter
      i = j;
    }
    else if(current == '\'') // start of a quoted fixed text part
    {
      const size_t j = pattern.find_first_of('\'', i+1);
      if(j == pattern.npos)
      {
        // number of quote characters is odd, format string is invalid
        std::vector<std::unique_ptr<format_elementt>> invalid_elements;
        invalid_elements.push_back(
            util_make_unique<format_element_invalidt>(4));
        return invalid_elements;
      }
      // substring of pattern from index i+1 to j-1 (not including the quotes)
      // '' is interpreted not as the empty string, but as a single quote
      const std::string fixed = (j == i+1) ? "'" : pattern.substr(i+1, j-i-1);
      elements.push_back(util_make_unique<fixed_textt>(fixed));
      // set index to the character just after the second quote
      i = j + 1;
    }
    else // start of an unquoted fixed text part
    {
      size_t j = i + 1;
      while(j < pattern.length() && !(isalpha(pattern.at(j)))
          && pattern.at(j) != '\'')
      {
        j++;
      }
      const std::string fixed = pattern.substr(i, j - i);
      elements.push_back(util_make_unique<fixed_textt>(fixed));
      i = j;
    }
  }
  return elements;
}

/// Given a date format specifier, add axioms ensuring the output corresponds to
/// the output of SimpleDateFormat.format applied to that specifier. Assumes the
/// argument is a structured expression which contains a field int.
/// \param fs: a format specifier
/// \param arg: a struct containing the possible value of the argument to format
/// \param index_type: type for indexes in strings
/// \param char_type: type of characters
/// \return Pair consisting of return code and string expression representing
/// the output of SimpleDateFormat.format. The return code is 0 on success, 3
/// for invalid format specifiers and 100 for format specifiers that we do not
/// yet support.
std::pair<exprt, array_string_exprt>
format_specifier_datet::add_axioms_for_format_specifier(
  string_constraint_generatort &gen,
  const struct_exprt &,
  const typet &index_type,
  const typet &char_type)
{
  const array_string_exprt res = gen.fresh_string(index_type, char_type);
  exprt return_code;
  std::ostringstream stream;
  size_t width;
  switch(pattern_letter)
  {
    case ERA_DESIGNATOR:
      return_code = gen.add_axioms_for_constant(res, "AD");
      return {return_code, res};
    case DAY_NAME_IN_WEEK:
      if(length < 4)
      {
        return_code = gen.add_axioms_for_constant(res, "Wed");
      }
      else
      {
        return_code = gen.add_axioms_for_constant(res, "Wednesday");
      }
      return {return_code, res};
    case AM_PM_MARKER:
      return_code = gen.add_axioms_for_constant(res, "PM");
      return {return_code, res};
    case MONTH:
      if(length == 1)
      {
        return_code = gen.add_axioms_for_constant(res, "1");
      }
      else if(length == 2)
      {
        return_code = gen.add_axioms_for_constant(res, "01");
      }
      else if(length == 3)
      {
        return_code = gen.add_axioms_for_constant(res, "Jan");
      }
      else
      {
        return_code = gen.add_axioms_for_constant(res, "January");
      }
      return {return_code, res};
    case YEAR:
    case WEEK_YEAR:
      if(length == 2)
      {
        return_code = gen.add_axioms_for_constant(res, "18");
      }
      if(length == 1 || length == 3 || length == 4)
      {
        return_code = gen.add_axioms_for_constant(res, "2018");
      }
      else
      {
        std::string zeros(length - 4, '0');
        return_code = gen.add_axioms_for_constant(res, zeros + "2018");
      }
      return {return_code, res};
    // "number" cases
    case WEEK_IN_YEAR:
    case WEEK_IN_MONTH:
      return_code = gen.add_axioms_for_constant(
          res, std::string(length - 1, '0') + "2");
      return {return_code, res};
    case DAY_IN_YEAR:
    case DAY_IN_MONTH:
      width = std::max<size_t>(length, 2);
      stream.width(width);
      stream << "10";
      stream.fill('0');
      return_code = gen.add_axioms_for_constant(res, stream.str());
      return {return_code, res};
    case DAY_OF_WEEK_IN_MONTH:
      return_code = gen.add_axioms_for_constant(
          res, std::string(length - 1, '0') + "2");
      return {return_code, res};
    case DAY_NUMBER_OF_WEEK:
      return_code = gen.add_axioms_for_constant(
          res, std::string(length - 1, '0') + "3");
      return {return_code, res};
    case HOUR_IN_DAY_FROM_ZERO:
    case HOUR_IN_DAY_FROM_ONE:
      width = std::max<size_t>(length, 2);
      stream.width(width);
      stream << "12";
      stream.fill('0');
      return_code = gen.add_axioms_for_constant(res, stream.str());
      return {return_code, res};
    case HOUR_IN_AM_PM_FROM_ZERO:
      return_code = gen.add_axioms_for_constant(
          res, std::string(length - 1, '0') + "0");
      return {return_code, res};
    case HOUR_IN_AM_PM_FROM_ONE:
      width = std::max<size_t>(length, 2);
      stream.width(width);
      stream << "12";
      stream.fill('0');
      return_code = gen.add_axioms_for_constant(res, stream.str());
      return {return_code, res};
    case MINUTE_IN_HOUR:
    case SECOND_IN_MINUTE:
    case MILLISECOND:
      return_code = gen.add_axioms_for_constant(
          res, std::string(length - 1, '0') + "0");
      return {return_code, res};
    case TIME_ZONE_GENERAL:
      return_code = gen.add_axioms_for_constant(res, "GMT");
      return {return_code, res};
    case TIME_ZONE_RFC:
      return_code = gen.add_axioms_for_constant(res, "+0000");
      return {return_code, res};
    case TIME_ZONE_ISO:
      return_code = gen.add_axioms_for_constant(res, "Z");
      return {return_code, res};
    default:
      /// \todo Throwing exceptions for invalid format specifiers is not yet
      ///   implemented. In Java, a java.lang.IllegalArgumentException is
      ///   thrown in this case. Instead, we currently just return a
      ///   nondeterministic string.
      gen.message.error() << "invalid pattern letter: " << pattern_letter
        << ". pattern letter must belong to [GyYMwWDdFEuaHkKhmsSzZX]"
        << gen.message.eom;
      return_code = from_integer(3, gen.get_return_code_type());
      return {return_code, res};
  }

}

/// Given a date format element, add axioms ensuring the output corresponds to
/// the output of SimpleDateFormat.format applied to that element.
/// In the case of pattern letters, we first need to check for letters that
/// correspond to exception cases in the Java program. Then the formatting is
/// done by a call to add_axioms_for_format_specifier.
/// \param gen: a `string_constraint_generatort` object (used for accessing
///   some of the member functions of this class)
/// \param arg_count: the number of arguments in the argument list that have
///   already been processed using format specifiers without argument index
/// \param index_type: type for indices in strings
/// \param char_type: type of characters in strings
/// \param args: argument list passed to String.format method
/// \return Pair consisting of return code and string expression representing
/// the output of String.format. The return code is 0 on success, 1 for
/// invalid conversion characters, 2 for an insufficient number of arguments,
/// and 100 for format specifiers that we do not yet support.
std::pair<exprt, array_string_exprt>
format_specifier_datet::add_axioms_for_format_element(
  string_constraint_generatort &gen,
  std::size_t &,
  const typet &index_type,
  const typet &char_type,
  const exprt::operandst &)
{
  return add_axioms_for_format_specifier(
      gen, struct_exprt(), index_type, char_type);
}
