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

#include <util/std_expr.h>
#include <util/unicode.h>

#include "string_constraint_generator.h"

/// A format specifier is of the form
/// %[arg_index$][flags][width][.precision]conversion
/// and is applied to an element of the argument list passed to String.format.
/// It describes how this value should be printed. For details see
/// https://docs.oracle.com/javase/7/docs/api/java/util/Formatter.html#syntax
class string_constraint_generatort::format_specifiert
{
public:
  // Constants describing the meaning of conversion characters in format
  // specifiers.
  static const char DECIMAL_INTEGER          ='d';
  static const char OCTAL_INTEGER            ='o';
  static const char HEXADECIMAL_INTEGER      ='x';
  static const char HEXADECIMAL_INTEGER_UPPER='X';
  static const char SCIENTIFIC               ='e';
  static const char SCIENTIFIC_UPPER         ='E';
  static const char GENERAL                  ='g';
  static const char GENERAL_UPPER            ='G';
  static const char DECIMAL_FLOAT            ='f';
  static const char HEXADECIMAL_FLOAT        ='a';
  static const char HEXADECIMAL_FLOAT_UPPER  ='A';
  static const char CHARACTER                ='c';
  static const char CHARACTER_UPPER          ='C';
  static const char DATE_TIME                ='t';
  static const char DATE_TIME_UPPER          ='T';
  static const char BOOLEAN                  ='b';
  static const char BOOLEAN_UPPER            ='B';
  static const char STRING                   ='s';
  static const char STRING_UPPER             ='S';
  static const char HASHCODE                 ='h';
  static const char HASHCODE_UPPER           ='H';
  static const char LINE_SEPARATOR           ='n';
  static const char PERCENT_SIGN             ='%';

  // Class members representing the structure of the format specifier
  int arg_index=-1;
  std::string flag;
  int width;
  int precision;
  bool dt=false;
  char conversion;

  format_specifiert(
    int _arg_index,
    std::string _flag,
    int _width,
    int _precision,
    bool _dt,
    char _conversion):
      arg_index(_arg_index),
      flag(_flag),
      width(_width),
      precision(_precision),
      dt(_dt),
      conversion(_conversion)
  { }
};

/// Class to represent fixed text in a format string. The contents of it are
/// unchanged by calls to java.lang.String.format.
class fixed_textt
{
public:
  explicit fixed_textt(std::string _content): content(_content) { }

  fixed_textt(const fixed_textt &fs): content(fs.content) { }

  std::string get_content() const
  {
    return content;
  }

private:
  std::string content;
};

/// A format string consists of format elements. A format element is either a
/// format specifier or fixed text.
class format_elementt
{
public:
  /// Type representing the two different types of format elements
  typedef enum {SPECIFIER, TEXT} format_typet;

  /// Given only a `format_typet` and no further information, we assign this to
  /// the `type` of this format element and leave the `fstring` field set to the
  /// empty string.
  explicit format_elementt(format_typet _type): type(_type), fstring("")
  {
  }

  /// A string argument means that this format element is a fixed text.
  explicit format_elementt(std::string s): type(TEXT), fstring(s)
  {
  }

  /// A format specifier argument means that this format element is a format
  /// specifier.
  explicit format_elementt(string_constraint_generatort::format_specifiert fs):
    type(SPECIFIER),
    fstring("")
  {
    fspec.push_back(fs);
  }

  // Getters and setters

  bool is_format_specifier() const
  {
    return type==SPECIFIER;
  }

  bool is_fixed_text() const
  {
    return type==TEXT;
  }

  string_constraint_generatort::format_specifiert get_format_specifier() const
  {
    PRECONDITION(is_format_specifier());
    return fspec.back();
  }

  fixed_textt &get_fixed_text()
  {
    PRECONDITION(is_fixed_text());
    return fstring;
  }

  const fixed_textt &get_fixed_text() const
  {
    PRECONDITION(is_fixed_text());
    return fstring;
  }

private:
  /// Type of this format element: format specifier (SPECIFIER) or fixed text
  /// (TEXT)
  format_typet type;
  /// String storing the fixed text if the object is of type TEXT, and storing
  /// the empty string if it is of type SPECIFIER.
  fixed_textt fstring;
  /// This vector is of length 0 if the type is TEXT, and of length 1 if the
  /// type is SPECIFIER.
  std::vector<string_constraint_generatort::format_specifiert> fspec;
};

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

/// Helper function for parsing format strings.
/// This follows the implementation in openJDK of the java.util.Formatter class:
/// http://hg.openjdk.java.net/jdk7/jdk7/jdk/file/9b8c96f96a0f/src/share/classes/java/util/Formatter.java#l2660
/// \param m: a match in a regular expression
/// \return Format specifier represented by the matched string. The groups in
///   the match should represent: argument index, flag, width, precision, date
///   and conversion type.
static string_constraint_generatort::format_specifiert
  format_specifier_of_match(std::smatch &m)
{
  int arg_index=m[1].str().empty()?-1:std::stoi(m[1].str());
  std::string flag=m[2].str().empty()?"":m[2].str();
  int width=m[3].str().empty()?-1:std::stoi(m[3].str());
  int precision=m[4].str().empty()?-1:std::stoi(m[4].str());
  std::string tT=m[5].str();

  bool dt=(tT!="");
  if(tT=="T")
    flag.push_back(
      string_constraint_generatort::format_specifiert::DATE_TIME_UPPER);

  INVARIANT(
    m[6].str().length()==1, "format conversion should be one character");
  char conversion=m[6].str()[0];

  return string_constraint_generatort::format_specifiert(
    arg_index, flag, width, precision, dt, conversion);
}

/// Parse the given string into format specifiers and text.
/// This follows the implementation in openJDK of the java.util.Formatter class:
/// http://hg.openjdk.java.net/jdk7/jdk7/jdk/file/9b8c96f96a0f/src/share/classes/java/util/Formatter.java#l2513
/// \param s: a string
/// \return A vector of format_elementt.
static std::vector<format_elementt> parse_format_string(std::string s)
{
  std::string format_specifier=
    "%(\\d+\\$)?([-#+ 0,(\\<]*)?(\\d+)?(\\.\\d+)?([tT])?([a-zA-Z%])";
  std::regex regex(format_specifier);
  std::vector<format_elementt> elements;
  std::smatch match;

  while(std::regex_search(s, match, regex))
  {
    if(match.position()!=0)
    {
      std::string pre_match=s.substr(0, match.position());
      elements.emplace_back(pre_match);
    }

    elements.emplace_back(format_specifier_of_match(match));
    s=match.suffix();
  }

  elements.emplace_back(s);
  return elements;
}

/// Helper for add_axioms_for_format_specifier
/// \param expr: a structured expression
/// \param component_name: name of the desired component
/// \return Expression in the component of `expr` named `component_name`.
static exprt get_component_in_struct(
  const struct_exprt &expr, irep_idt component_name)
{
  const struct_typet &type=to_struct_type(expr.type());
  std::size_t number=type.component_number(component_name);
  return expr.operands()[number];
}

/// Given a format specifier, add axioms ensuring the output corresponds to the
/// output of String.format applied to that specifier. Assumes the argument is a
/// structured expression which contains the fields: string expr, int, float,
/// char, boolean, hashcode, date_time. The correct component will be fetched
/// depending on the format specifier. We do not yet support %o, %g, %G, %a, %A,
/// %t and %T format specifiers.
/// \param fs: a format specifier
/// \param arg: a struct containing the possible value of the argument to format
/// \param index_type: type for indexes in strings
/// \param char_type: type of characters
/// \return Pair consisting of return code and string expression representing
/// the output of String.format. The return code is 0 on success, 1 for invalid
/// format specifiers and 100 for format specifiers that we do not yet support.
std::pair<exprt, array_string_exprt>
string_constraint_generatort::add_axioms_for_format_specifier(
  const format_specifiert &fs,
  const struct_exprt &arg,
  const typet &index_type,
  const typet &char_type)
{
  const array_string_exprt res = fresh_string(index_type, char_type);
  exprt return_code;
  switch(fs.conversion)
  {
  case format_specifiert::DECIMAL_INTEGER:
    return_code =
      add_axioms_from_int(res, get_component_in_struct(arg, ID_int));
    return {return_code, res};
  case format_specifiert::HEXADECIMAL_INTEGER:
    return_code =
      add_axioms_from_int_hex(res, get_component_in_struct(arg, ID_int));
    return {return_code, res};
  case format_specifiert::SCIENTIFIC:
    add_axioms_from_float_scientific_notation(
      res, get_component_in_struct(arg, ID_float));
    return {return_code, res};
  case format_specifiert::DECIMAL_FLOAT:
    add_axioms_for_string_of_float(res, get_component_in_struct(arg, ID_float));
    return {return_code, res};
  case format_specifiert::CHARACTER:
    return_code =
      add_axioms_from_char(res, get_component_in_struct(arg, ID_char));
    return {return_code, res};
  case format_specifiert::BOOLEAN:
    return_code =
      add_axioms_from_bool(res, get_component_in_struct(arg, ID_boolean));
    return {return_code, res};
  case format_specifiert::STRING:
  {
    return_code = from_integer(0, get_return_code_type());
    const array_string_exprt string_res = get_string_expr(
        get_component_in_struct(arg, "string_expr"));
    return {return_code, string_res};
  }
  case format_specifiert::HASHCODE:
    return_code =
      add_axioms_from_int(res, get_component_in_struct(arg, "hashcode"));
    return {return_code, res};
  case format_specifiert::LINE_SEPARATOR:
    // TODO: the constant should depend on the system: System.lineSeparator()
    return_code = add_axioms_for_constant(res, "\n");
    return {return_code, res};
  case format_specifiert::PERCENT_SIGN:
    return_code = add_axioms_for_constant(res, "%");
    return {return_code, res};
  case format_specifiert::SCIENTIFIC_UPPER:
  case format_specifiert::GENERAL_UPPER:
  case format_specifiert::HEXADECIMAL_FLOAT_UPPER:
  case format_specifiert::CHARACTER_UPPER:
  case format_specifiert::DATE_TIME_UPPER:
  case format_specifiert::BOOLEAN_UPPER:
  case format_specifiert::STRING_UPPER:
  case format_specifiert::HASHCODE_UPPER:
  {
    string_constraint_generatort::format_specifiert fs_lower=fs;
    fs_lower.conversion=tolower(fs.conversion);
    const auto lower_case_code_format_pair =
      add_axioms_for_format_specifier(fs_lower, arg, index_type, char_type);
    add_axioms_for_to_upper_case(res, lower_case_code_format_pair.second);
    return {lower_case_code_format_pair.first, res};
  }
  case format_specifiert::OCTAL_INTEGER:
  /// \todo Conversion of octal is not implemented.
  case format_specifiert::GENERAL:
  /// \todo Conversion for format specifier general is not implemented.
  case format_specifiert::HEXADECIMAL_FLOAT:
  /// \todo Conversion of hexadecimal float is not implemented.
  case format_specifiert::DATE_TIME:
    /// \todo Conversion of date-time is not implemented.
    ///   For all these unimplemented cases we return a non-deterministic
    ///   string.
    message.warning() << "unimplemented format specifier: " << fs.conversion
                        << message.eom;
    return_code = from_integer(100, get_return_code_type());
    return {return_code, res};
  default:
    /// \todo Throwing exceptions for invalid format specifiers is not yet
    ///   implemented. In Java, a java.util.UnknownFormatConversionException is
    ///   thrown in this case. Instead, we currently just return a
    ///   nondeterministic string.
    message.error() << "invalid format specifier: " << fs.conversion
      << ". format specifier must belong to [bBhHsScCdoxXeEfgGaAtT%n]"
      << message.eom;
    return_code = from_integer(1, get_return_code_type());
    return {return_code, res};
  }
}

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
  const std::vector<format_elementt> format_strings=parse_format_string(s);
  // We will format each format element according to the specification of
  // java.lang.String.format and store the results in a temporary vector
  // variable.
  std::vector<array_string_exprt> formatted_elements;
  // Number of format specifiers that we have processed that did not specify an
  // argument index
  std::size_t arg_count=0;
  const typet &char_type = res.content().type().subtype();
  const typet &index_type = res.length().type();

  for(const format_elementt &fe : format_strings)
  {
    if(fe.is_format_specifier())
    {
      const format_specifiert &fs=fe.get_format_specifier();
      struct_exprt arg;
      // Per cent sign (%) and line separator (n) do not take any arguments.
      if(fs.conversion!=format_specifiert::PERCENT_SIGN &&
         fs.conversion!=format_specifiert::LINE_SEPARATOR)
      {
        // -1 means no value specified. 0 is a special case that is treated by
        // Java as if no value was specified.
        if(fs.arg_index == -1 || fs.arg_index == 0)
        {
          /// \todo In java, a java.util.MissingFormatArgumentException is
          ///   thrown when the number of arguments is less than the number of
          ///   format specifiers without argument index. We do not yet support
          ///   throwing the exception in this case and instead do not put any
          ///   additional constraints on the string.
          if(arg_count >= args.size())
          {
            message.warning() << "number of arguments must be at least number "
                              << "of format specifiers without argument index"
                              << message.eom;
            return from_integer(2, get_return_code_type());
          }
          arg=to_struct_expr(args[arg_count++]);
        }
        else
        {
          /// \todo In Java, a java.util.UnknownFormatConversionException is
          ///   thrown when the argument index in the format specifier is
          ///   negative. We do not yet support throwing the exception in this
          ///   case and instead do not put any additional constraints on the
          ///   string.
          if(fs.arg_index < 0)
          {
            message.warning() << "index in format should be nonnegative"
                              << message.eom;
            return from_integer(1, get_return_code_type());
          }
          /// \todo In Java, a java.util.MissingFormatArgumentException is
          ///   thrown when the argument index in the format specifier is bigger
          ///   than the number of arguments. We do not yet support throwing the
          ///   exception in this case and instead do not put any additional
          ///   constraints on the string.
          if(static_cast<std::size_t>(fs.arg_index) > args.size())
          {
            message.warning() << "argument index in format specifier cannot be "
                              << "bigger than number of arguments"
                              << message.eom;
            return from_integer(2, get_return_code_type());
          }
          // first argument `args[0]` corresponds to argument index 1
          arg=to_struct_expr(args[fs.arg_index-1]);
        }
      }
      const auto code_format_pair =
        add_axioms_for_format_specifier(fs, arg, index_type, char_type);
      if(code_format_pair.first
            != from_integer(0, get_return_code_type()))
      {
        // A nonzero exit code means an exception was thrown.
        /// \todo Add support for exceptions in add_axioms_for_format_specifier
        return code_format_pair.first;
      }
      formatted_elements.push_back(code_format_pair.second);
    }
    else // fe.is_fixed_text() == true
    {
      const array_string_exprt str = fresh_string(index_type, char_type);
      const exprt return_code =
        add_axioms_for_constant(str, fe.get_fixed_text().get_content());
      formatted_elements.push_back(str);
    }
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
