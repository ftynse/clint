#include "clayparser.h"
#include "transformation.h"

#include <QtGui>
#include <QtCore>

#include <clay/clay.h>

#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix.hpp>

#include <boost/fusion/adapted/struct/adapt_struct.hpp>
#include <boost/fusion/include/adapt_struct.hpp>

#include <boost/variant/get.hpp>

#include <boost/algorithm/string.hpp>

#include <functional>
#include <tuple>
#include <unordered_map>

#include <iostream>
#include <sstream>
#include <string>

namespace {

class string_builder {
public:
  template <typename... Args>
  std::string operator () (Args... args){
    std::stringstream stream;
    build(stream, args...);
    return std::move(stream.str());
  }

private:
  template <typename Arg, typename... Args>
  std::enable_if_t<sizeof...(Args) != 0>
  build (std::stringstream &stream, Arg arg, Args... args) {
    stream << arg;
    build(stream, args...);
  }

  template <typename Arg>
  void build (std::stringstream &stream, Arg arg) {
    stream << arg;
  }
};

namespace clay_parser {
  namespace fusion = boost::fusion;
  namespace phoenix = boost::phoenix;
  namespace qi = boost::spirit::qi;
  namespace ascii = boost::spirit::ascii;

struct clay_parser_list {
  std::vector<int> first;
  std::vector<int> second;
  std::vector<int> third;
  std::vector<int> fourth;
};

typedef boost::variant<std::vector<int>, int, clay_parser_list> clay_parser_arg;

struct clay_parser_command {
  std::string name;
  std::vector<clay_parser_arg> arguments;
};

} // end namespace clay_parser

} // end anonymous namespace

BOOST_FUSION_ADAPT_STRUCT(
    clay_parser::clay_parser_list,
    (std::vector<int>, first)
    (std::vector<int>, second)
    (std::vector<int>, third)
    (std::vector<int>, fourth)
)

BOOST_FUSION_ADAPT_STRUCT(
    clay_parser::clay_parser_command,
    (std::string, name)
    (std::vector<clay_parser::clay_parser_arg>, arguments)
)

namespace {

namespace clay_parser {

template <class Iterator>
struct clay_grammar : qi::grammar<Iterator, std::vector<clay_parser_command>(), ascii::space_type> {
  clay_grammar() : clay_grammar::base_type(start) {
    using qi::lexeme;
    using qi::int_;
    using ascii::char_;
    using ascii::string;
    using namespace qi::labels;

    using phoenix::at_c;
    using phoenix::push_back;
    text = lexeme[+(char_ - '(')  [_val += _1]];

    int_list = int_ % ',';

    beta_empty = (lexeme["["] >> ']')  [_val = std::vector<int>()];

    beta_full = '['
             >> int_list     [_val = _1]
             >> ']';

    beta = (beta_full | beta_empty)    [_val = _1];

    constant = int_;

    list = '{'
        >> -int_list    [at_c<3>(_val) = _1]
        >> -lexeme["|"] [at_c<2>(_val) = at_c<3>(_val), at_c<3>(_val) = std::vector<int>()]
        >> -int_list    [at_c<3>(_val) = _1]
        >> -lexeme["|"] [at_c<1>(_val) = at_c<2>(_val), at_c<2>(_val) = at_c<3>(_val), at_c<3>(_val) = std::vector<int>()]
        >> -int_list    [at_c<3>(_val) = _1]
        >> -lexeme["|"] [at_c<0>(_val) = at_c<1>(_val), at_c<1>(_val) = at_c<2>(_val), at_c<2>(_val) = at_c<3>(_val), at_c<3>(_val) = std::vector<int>()]
        >> -int_list    [at_c<3>(_val) = _1]
        >> '}';

    arg = (beta | constant | list)    [_val = _1];

    arg_list = arg % ',';

    command = text         [at_c<0>(_val) = _1]
            >> '('
            >> arg_list    [at_c<1>(_val) = _1]
            >> ')';

    sequence = command % ';';

    start = sequence      [_val = _1]
         >> ';';
  }

  qi::rule<Iterator, std::vector<clay_parser_command>(), ascii::space_type> start;
  qi::rule<Iterator, std::vector<clay_parser_command>(), ascii::space_type> sequence;
  qi::rule<Iterator, clay_parser_command(), ascii::space_type> command;
  qi::rule<Iterator, std::vector<clay_parser_arg>(), ascii::space_type> arg_list;
  qi::rule<Iterator, clay_parser_arg(), ascii::space_type> arg;
  qi::rule<Iterator, clay_parser_list(), ascii::space_type> list;
  qi::rule<Iterator, int(), ascii::space_type> constant;
  qi::rule<Iterator, std::vector<int>(), ascii::space_type> beta_full;
  qi::rule<Iterator, std::vector<int>(), ascii::space_type> beta_empty;
  qi::rule<Iterator, std::vector<int>(), ascii::space_type> beta;
  qi::rule<Iterator, std::vector<int>(), ascii::space_type> int_list;
  qi::rule<Iterator, std::string(), ascii::space_type> text;
};

  class syntax_error : public std::logic_error {
    using std::logic_error::logic_error;
  };

  class semantic_error : public std::logic_error {
    using std::logic_error::logic_error;
  };
}

enum clay_parser_arg_kind : size_t {
  vector,
  constant,
  list
};

#define szck_block(n) \
  if (cmd.arguments.size() != n) \
    throw clay_parser::semantic_error( \
      string_builder()(cmd.arguments.size() < n ? "Not enough" : "Too many", \
                       " arguments for [", cmd.name, "], expecting ", n, " got ", cmd.arguments.size())); \


#define uwrp_block(n) \
  boost::optional<T##n> arg##n##_optional; \
  try { arg##n##_optional = boost::get<T##n>(cmd.arguments[(n - 1)]); } \
  catch (boost::bad_get) { \
    throw clay_parser::semantic_error(string_builder()("Argument ", n, " has incorrect type.  Expected ", typeid(T##n).name())); }\
  T##n arg##n = arg##n##_optional.get();

template <typename T1>
std::function<Transformation (const clay_parser::clay_parser_command &)>
unwrap(std::function<Transformation (T1)> f) {
  return [f] (const clay_parser::clay_parser_command &cmd) {
    szck_block(1);
    uwrp_block(1);
    return f(arg1);
  };
}

template <typename T1, typename T2>
std::function<Transformation (const clay_parser::clay_parser_command &)>
unwrap(std::function<Transformation (T1, T2)> f) {
  return [f] (const clay_parser::clay_parser_command &cmd) {
    szck_block(2);
    uwrp_block(1);
    uwrp_block(2);
    return f(arg1, arg2);
  };
}

template <typename T1, typename T2, typename T3>
std::function<Transformation (const clay_parser::clay_parser_command &)>
unwrap(std::function<Transformation (T1, T2, T3)> f) {
  return [f] (const clay_parser::clay_parser_command &cmd) {
    szck_block(3);
    uwrp_block(1);
    uwrp_block(2);
    uwrp_block(3);
    return f(arg1, arg2, arg3);
  };
}

template <typename T1, typename T2, typename T3, typename T4>
std::function<Transformation (const clay_parser::clay_parser_command &)>
unwrap(std::function<Transformation (T1, T2, T3, T4)> f) {
  return [f] (const clay_parser::clay_parser_command &cmd) {
    szck_block(4);
    uwrp_block(1);
    uwrp_block(2);
    uwrp_block(3);
    uwrp_block(4);
    return f(arg1, arg2, arg3, arg4);
  };
}

template <typename T1, typename T2, typename T3, typename T4, typename T5>
std::function<Transformation (const clay_parser::clay_parser_command &)>
unwrap(std::function<Transformation (T1, T2, T3, T4, T5)> f) {
  return [f] (const clay_parser::clay_parser_command &cmd) {
    szck_block(5);
    uwrp_block(1);
    uwrp_block(2);
    uwrp_block(3);
    uwrp_block(4);
    uwrp_block(5);
    return f(arg1, arg2, arg3, arg4, arg5);
  };
}

#undef uwrp_block

Transformation create_transformation(const clay_parser::clay_parser_command &cmd,
                                     const std::unordered_map<std::string, std::function<Transformation (const clay_parser::clay_parser_command &)>> &mapping) {
  auto iterator = mapping.find(cmd.name);
  if (iterator == std::end(mapping))
    throw clay_parser::semantic_error(string_builder()("Transformation [", cmd.name, "] not found."));

  std::function<Transformation (const clay_parser::clay_parser_command &)> descriptor = iterator->second;

  return descriptor(cmd);
}

static std::vector<std::vector<int>> wrapClayParserList(const clay_parser::clay_parser_list &&list) {
  std::vector<std::vector<int>> wrapped(4);
  wrapped[0] = list.first;
  wrapped[1] = list.second;
  wrapped[2] = list.third;
  wrapped[3] = list.fourth;
  return std::move(wrapped);
}

Transformation wrappedIss(const std::vector<int> &beta, const clay_parser::clay_parser_list &&list) {
  return Transformation::rawIss(beta, wrapClayParserList(std::forward<const clay_parser::clay_parser_list>(list)));
}

TransformationSequence create_sequence(const std::vector<clay_parser::clay_parser_command> &commands) {
  TransformationSequence sequence;

  std::unordered_map<std::string, std::function<Transformation (const clay_parser::clay_parser_command &)>> mapping;
  mapping["split"]       = unwrap<std::vector<int>, int>                               (Transformation::rawSplit);
  mapping["fission"]     = mapping["split"];
  mapping["distribute"]  = mapping["split"];
  mapping["fuse"]        = unwrap<std::vector<int>>                                    (Transformation::fuseNext);
  mapping["reorder"]     = unwrap<std::vector<int>, std::vector<int>>                  (Transformation::rawReorder);
  mapping["shift"]       = unwrap<std::vector<int>, int, std::vector<int>, int>        (Transformation::rawShift);
  mapping["skew"]        = unwrap<std::vector<int>, int, int, int>                     (Transformation::skew);
  mapping["grain"]       = unwrap<std::vector<int>, int, int>                          (Transformation::grain);
  mapping["interchange"] = unwrap<std::vector<int>, int, int, int>                     (Transformation::interchange);
  mapping["reverse"]     = unwrap<std::vector<int>, int>                               (Transformation::reverse);
  mapping["tile"]        = unwrap<std::vector<int>, int, int, int, int>                (Transformation::rawTile);
  mapping["iss"]         = unwrap<std::vector<int>, clay_parser::clay_parser_list>     (wrappedIss);
  mapping["reshape"]     = unwrap<std::vector<int>, int, int, int>                     (Transformation::reshape);
  mapping["linearize"]   = unwrap<std::vector<int>, int>                               (Transformation::linearize);
  mapping["collapse"]    = unwrap<std::vector<int>>                                    (Transformation::collapse);
  mapping["embed"]       = unwrap<std::vector<int>>                                    (Transformation::embed);
  mapping["unembed"]     = unwrap<std::vector<int>>                                    (Transformation::unembed);
  mapping["densify"]     = unwrap<std::vector<int>, int>                               (Transformation::densify);


  for (const clay_parser::clay_parser_command &cmd : commands) {
    TransformationGroup group;
    group.transformations.push_back(create_transformation(cmd, mapping));
    sequence.groups.push_back(group);
  }
  return std::move(sequence);
}

void print_parser_command(std::vector<clay_parser::clay_parser_command> parser_command_list) {
  for (const clay_parser::clay_parser_command &cmd : parser_command_list) {
    std::cout << cmd.name << " ";
    for (const clay_parser::clay_parser_arg &arg : cmd.arguments) {
      if (const int *constant = boost::get<int>(&arg)) {
        std::cout << *constant << " ";
      } else if (const std::vector<int> *beta = boost::get<std::vector<int>>(&arg)) {
        std::cout << "[";
        std::copy(beta->cbegin(), beta->cend(), std::ostream_iterator<int>(std::cout, ","));
        std::cout << "] ";
      } else if (const clay_parser::clay_parser_list *list = boost::get<clay_parser::clay_parser_list>(&arg)) {
        std::cout << "{";
        std::copy(list->first.cbegin(), list->first.cend(), std::ostream_iterator<int>(std::cout, ","));
        std::cout << "|";
        std::copy(list->second.cbegin(), list->second.cend(), std::ostream_iterator<int>(std::cout, ","));
        std::cout << "|";
        std::copy(list->third.cbegin(), list->third.cend(), std::ostream_iterator<int>(std::cout, ","));
        std::cout << "|";
        std::copy(list->fourth.cbegin(), list->fourth.cend(), std::ostream_iterator<int>(std::cout, ","));
        std::cout << "} ";
      } else {
        std::cout << "wtf";
      }
    }
    std::cout << std::endl;
  }
}


} // end anonymous namespace

ClayParser::ClayParser() {
}

TransformationSequence ClayParser::parse(const std::string &text) {
  clay_parser::clay_grammar<std::string::const_iterator> grammar;

  std::list<boost::iterator_range<std::string::const_iterator>> groups;
  for (size_t i = 0, e = text.size(); i < e; ) {
    size_t p = text.find("\n\n", i);
    if (p == std::string::npos)
      p = e;
    groups.push_back(boost::iterator_range<std::string::const_iterator>(
                       text.begin() + i,
                       text.begin() + p));
    i = p;
  }

  TransformationSequence sequence;
  for (boost::iterator_range<std::string::const_iterator> &r : groups) {
    std::vector<clay_parser::clay_parser_command> parsed;
    std::string::const_iterator iter = r.begin(),
                                end  = r.end();
    bool result = boost::spirit::qi::phrase_parse(
          iter,
          end,
          grammar,
          boost::spirit::ascii::space,
          parsed);
    if (result && iter == end) {
      sequence = create_sequence(parsed);
    } else {
      throw clay_parser::syntax_error("Input text is not a clay transformation sequence");
    }
  }

  return std::move(sequence);
}

