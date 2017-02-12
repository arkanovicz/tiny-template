/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifdef _WIN32
#pragma warning(disable: 4018 4100 4127 4389 4459 4800 4996)
#endif

#include <boost/algorithm/string/join.hpp>
#include <boost/fusion/include/at_c.hpp>
#include <boost/optional/optional_io.hpp>
// uncomment to display parsing debugging infos
//#define BOOST_SPIRIT_X3_DEBUG
#include <boost/spirit/home/x3.hpp>
#include <boost/tuple/tuple.hpp>
#include <exception>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "tiny_template.h"

// hack for windows
#ifdef _WIN32
#undef BOOST_SPIRIT_DEFINE_
#undef BOOST_SPIRIT_DEFINE

#define BOOST_SPIRIT_DEFINE_(r, data, rule_name)                                \
    using BOOST_PP_CAT(rule_name, _synonym) = decltype(rule_name);              \
    template <typename Iterator, typename Context, typename Attribute>          \
    inline bool parse_rule(                                                     \
        BOOST_PP_CAT(rule_name, _synonym) rule_                                 \
      , Iterator& first, Iterator const& last                                   \
      , Context const& context, Attribute& attr)                                \
    {                                                                           \
        using boost::spirit::x3::unused;                                        \
        static auto const def_ = (rule_name = BOOST_PP_CAT(rule_name, _def));   \
        return def_.parse(first, last, context, unused, attr);                  \
    }                                                                           \
    /***/

#define BOOST_SPIRIT_DEFINE(...) BOOST_PP_SEQ_FOR_EACH(                         \
    BOOST_SPIRIT_DEFINE_, _, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__))             \
    /***/
#endif

// temporary debugging
#ifdef _DEBUG
#include <iostream>
#define LOG(op) std::cerr << op << std::endl << std::flush;
#else
#define LOG(op)
#endif

namespace ttl
{
	namespace x3 = boost::spirit::x3;

	// utility

	void to_json_map(const map &m, std::ostream &out);
	void to_json_array(const vector &v, std::ostream &out);
	
	void to_json_array(const vector &v, std::ostream &out)
	{
		out << '[';
		bool first = true;
		for (auto &item : v)
		{
			if (first) first = false;
			else out << ',';
			const std::string *str = boost::any_cast<std::string>(&item);
			if (str) out << '"' << *str << '"';
			else
			{
				const vector *vect = boost::any_cast<vector>(&item);
				if (vect) to_json_array(*vect, out);
				else
				{
					const map *m = boost::any_cast<map>(&item);
					if (m) to_json_map(*m, out);
					else throw evaluation_error("invalid type");
				}
			}
		}
		out << ']';
	}
	
	void to_json_map(const map &m, std::ostream &out)
	{
		out << '{';
		bool first = true;
		for (auto &pair : m)
		{
			if (first) first = false;
			else out << ',';
			out << '"' << pair.first << "\":";
			const std::string *str = boost::any_cast<std::string>(&pair.second);
			if (str) out << '"' << *str << '"';
			else
			{
				const vector *vect = boost::any_cast<vector>(&pair.second);
				if (vect) to_json_array(*vect, out);
				else
				{
					const map * submap = boost::any_cast<map>(&pair.second);
					if (submap) to_json_map(*submap, out);
					else throw evaluation_error("invalid type");
				}
			}
		}
		out << '}';
	}
	
	std::string to_json(const context &ctx)
	{
		std::ostringstream oss;
		to_json_map(ctx, oss);
		return oss.str();
	}
	
	// AST

	namespace ast
	{
		// global empty string value as boost::any
		boost::any empty_string = std::string();

		struct parent_node : node
		{
			template <typename T> parent_node(std::vector<T> &v)
			{
				for (T &t : v)
				{
					children.push_back(boost::get<node_ptr>(t));
				}
			}
			virtual ~parent_node() {}
			virtual std::string evaluate(const map &params)
			{
				std::string ret;
				for (node_ptr &node : children) ret += node->evaluate(params);
				return ret;
			}
			virtual std::string debug()
			{
				std::string ret;
				for (node_ptr &node : children) ret += node->debug();
				return ret;
			}
			std::vector<node_ptr> children;
		};

		struct text : node
		{
			text(const std::string s) : value(s) {}
			virtual ~text() {}
			virtual std::string evaluate(const map &params) { return value; }
			virtual std::string debug() { return value; }
			std::string value;
		};
		
		struct reference : node
		{
			reference(const std::vector<std::string> &vect) : identifiers(vect) {}
			virtual ~reference() {}

			virtual std::string evaluate(const map &params)
			{
				const std::string * str = boost::any_cast<std::string>(&resolve(params));
				if (!str) throw evaluation_error("wrong type");
				return *str;
			}

			const boost::any & resolve(const map &params)
			{
				const map *p = &params;
				map::const_iterator it;
				for (int i = 0; i < identifiers.size(); ++i)
				{
					it = p->find(identifiers[i]);
					if (it == p->end())
					{
						if (i == identifiers.size() - 1) return empty_string; // empty string for empty references
						throw evaluation_error("parameter '" + identifiers[i] + "' not found");
					}
					if (i == identifiers.size() - 1) break;
					p = &boost::any_cast<const map &>(it->second);
				}
				return it->second;
			}
			
			virtual std::string debug() { return "{$" + boost::join(identifiers, ".") + "}"; }

			bool test(const map &params)
			{
				boost::any prop;
				try { prop = resolve(params); } catch (std::exception &) { return false; }
				const std::string *value = boost::any_cast<std::string>(&prop);
				if (value) return !value->empty();  /* empty strings are false */
				const map *m = boost::any_cast<const map>(&prop);
				if (m) return !m->empty();
				const vector *v = boost::any_cast<const vector>(&prop);
				if (v) return !v->empty();
				throw evaluation_error("invalid type");
			}
			std::vector<std::string> identifiers;
		};

		struct if_directive : node
		{
			template <typename tuple> if_directive(tuple & t)
			{
				using boost::fusion::at_c;
				condition = at_c<0>(t);
				if_true = at_c<1>(t);
				if (at_c<2>(t))
					if_false = *at_c<2>(t);
			}
			virtual ~if_directive() {}
			virtual std::string evaluate(const map &params)
			{
				std::string ret;
				reference *ref = condition->get<reference>();
				if (!ref) throw evaluation_error("malformed #if directive");
				if (ref->test(params)) ret = if_true->evaluate(params);
				else if(if_false) ret = if_false->evaluate(params);
				else ret = "";
				return ret;
			}
			virtual std::string debug()
			{
				return "{#if $" + boost::join(condition->get<reference>()->identifiers, ".") + "}" + if_true->debug() + ( if_false ? "{#else}" + if_false->debug() : std::string() ) + "{#end}";
			}
			node_ptr condition;
			node_ptr if_true;
			node_ptr if_false;
		};
		
		struct join_directive : node
		{
			template <typename tuple> join_directive(tuple & t)
			{
				using boost::fusion::at_c;
				iterator = at_c<0>(t);
				if (!iterator->get<reference>() || iterator->get<reference>()->identifiers.size() != 1)
					throw parsing_error("malformed #join directive");
				collection = at_c<1>(t);
				if (at_c<2>(t)) separator = *at_c<2>(t);
				content = at_c<3>(t);
			}
			virtual ~join_directive() {}
			virtual std::string evaluate(const map &params)
			{
				std::string ret;
				const boost::any & values = collection->get<reference>()->resolve(params);
				const vector *objects = boost::any_cast<const vector>(&values);
				if (!objects)
				{
					const std::string *str = boost::any_cast<std::string>(&values);
					if (str) return *str; // a single value
					else return std::string();
				}
				map loop_params(params);
				std::string itname = iterator->get<reference>()->identifiers[0];
				bool first = true;
				for (boost::any const &value : *objects)
				{
					if (first) first = false;
					else if (separator) ret += separator->evaluate(params);						
					loop_params[itname] = value;
					ret += content->evaluate(loop_params);
				}
				return ret;
			}
			
			virtual std::string debug() { return "{#join $" + iterator->get<reference>()->identifiers[0] + " in $" + boost::join(collection->get<reference>()->identifiers, ".") + ( separator ? " with '" + separator->debug() + "'" : std::string() ) + "}" + content->debug() + "{#end}";
			}
			
			node_ptr iterator;	
			node_ptr collection;
			node_ptr separator;
			node_ptr content;
		};
		
	} // namespace ast

	// Grammar
	
#define DECLARE_RULE(name, value)							 \
	struct name ## _id;													 \
	rule<name ## _id, value> name = #name;

#define DEFINE_RULE(name, definition)						\
	auto const name ## _def = definition;					\
	BOOST_SPIRIT_DEFINE(name);
	
	namespace parser
	{
		using namespace x3;

		// helpers
		template <typename ItRange> std::string to_string(ItRange &range) { return std::string(range.begin(), range.end()); }
		
		// semantic actions
		auto empty_node = [](auto& ctx) { _val(ctx) = ast::node_ptr(); };
		auto new_text = [](auto& ctx) { _val(ctx) = ast::node_ptr(new ast::text(to_string(_attr(ctx)))); };
		auto new_reference = [](auto& ctx) { _val(ctx) = ast::node_ptr(new ast::reference(_attr(ctx))); };
		auto new_parent = [](auto& ctx) { _val(ctx) = ast::node_ptr(new ast::parent_node(_attr(ctx))); };
		auto new_if_directive = [](auto &ctx) { _val(ctx) = ast::node_ptr(new ast::if_directive(_attr(ctx))); };
		auto new_join_directive = [](auto &ctx) { _val(ctx) = ast::node_ptr(new ast::join_directive(_attr(ctx))); };

		// rules
		DECLARE_RULE( tiny_template, ast::node_ptr )
		DECLARE_RULE( template_part, ast::node_ptr )
		DECLARE_RULE( directive, ast::node_ptr )
		DECLARE_RULE( if_directive, ast::node_ptr )
		DECLARE_RULE( condition, ast::node_ptr )
		DECLARE_RULE( join_directive, ast::node_ptr )
		DECLARE_RULE( value, ast::node_ptr )
		DECLARE_RULE( variable, ast::node_ptr )
		DECLARE_RULE( reference, ast::node_ptr )
		DECLARE_RULE( identifier, std::string )
		DECLARE_RULE( literal_string, ast::node_ptr )
		DECLARE_RULE( plain_text, ast::node_ptr )

		DEFINE_RULE( tiny_template, template_part >> eoi )
		DEFINE_RULE( template_part, ( *( variable | directive | plain_text) ) [ new_parent ] )
		DEFINE_RULE( directive, if_directive | join_directive )
		DEFINE_RULE( if_directive, ( "{#if" >> omit[+space] >> condition >> '}' >> template_part >> -( "{#else}" >> template_part ) >> "{#end}" ) [ new_if_directive ] )
		// CB TODO - the only supported condition, for now, is a boolean check on a reference
		DEFINE_RULE( condition, reference )
		DEFINE_RULE( join_directive, ( "{#join" >> omit[+space] >> reference >> omit[+space] >> "in" >> omit[+space] >> reference >> -( omit[+space >> "with" >> +space] >> value ) >> '}' >> template_part >> "{#end}" ) [ new_join_directive] )
		DEFINE_RULE( value, reference | literal_string )
		DEFINE_RULE( variable, '{' >> reference >> '}' )
 		DEFINE_RULE( reference, '$' >> ( identifier % '.' ) [ new_reference ] )
		DEFINE_RULE( identifier, +( alnum | char_('_') ) )
		DEFINE_RULE( literal_string, '\'' >> raw[ +(char_ - '\'') ] [ new_text ] >> '\'' ) // CB TODO - escaping apos
		DEFINE_RULE( plain_text, raw[ +( char_ - '{' ) ] [ new_text ] )
		
	} // namespace parser
	
	ast::node_ptr ast::node::parse(const std::string &str)
	{
		node_ptr parsed;

		if (!x3::parse(str.begin(), str.end(), parser::template_part, parsed))
		{
			throw parsing_error();
		}
		return parsed;
	}
	
} // namespace ttl
