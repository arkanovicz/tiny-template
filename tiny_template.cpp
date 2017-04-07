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
                const char * const* chars = boost::any_cast<const char*>(&item);
                if (chars) out << '"' << *str << '"';
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
                const char * const* chars = boost::any_cast<const char*>(&pair.second);
                if (chars) out << '"' << *str << '"';
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
            virtual bool test(const map &) { throw evaluation_error("parent_node cannot be tested"); }
			std::vector<node_ptr> children;
		};

		struct text : node
		{
			text(const std::string s) : value(s) {}
			virtual ~text() {}
			virtual std::string evaluate(const map &params) { return value; }
			virtual std::string debug() { return value; }
            virtual bool test(const map &) { return !value.empty(); }
			std::string value;
		};
		
		struct reference : node
		{
			reference(const std::vector<std::string> &vect) : identifiers(vect) {}
			virtual ~reference() {}

			virtual std::string evaluate(const map &params)
			{
                std::string ret;
				const std::string * str = boost::any_cast<std::string>(&resolve(params));
                if (str) { ret = *str; }
                else
                {
                    const char * const* chars = boost::any_cast<const char*>(&resolve(params));
                    if (chars) { ret = *chars; }
                    else throw evaluation_error("wrong type");
                }
                return ret;
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
			
			virtual std::string debug() { return "{" + debug_inner() + "}"; }
			std::string debug_inner() { return "$" + boost::join(identifiers, "."); }

			virtual bool test(const map &params)
			{
				boost::any prop;
				try { prop = resolve(params); } catch (std::exception &) { return false; }
				const std::string *value = boost::any_cast<std::string>(&prop);
				if (value) return !value->empty();  /* empty strings are false */
                const char** chars = boost::any_cast<const char*>(&prop);
                if (chars) return *chars && **chars; /* empty and null char* strings are false */
				const map *m = boost::any_cast<const map>(&prop);
				if (m) return !m->empty();
				const vector *v = boost::any_cast<const vector>(&prop);
				if (v) return !v->empty();
				throw evaluation_error("invalid type");
			}
			std::vector<std::string> identifiers;
		};

        struct condition : node
        {
			condition() {}
            virtual ~condition() {}
            virtual std::string evaluate(const map &) { throw evaluation_error("condition cannot be evaluated"); }
            virtual std::string operator_string() = 0;
        };
    
        struct binary_operator : condition
        {
			binary_operator(node_ptr left_, node_ptr right_)
			{
                left = left_;
                right = right_;
            }
            virtual ~binary_operator() {}
            virtual std::string debug()
            {
                return ( left->get<reference>() ? left->get<reference>()->debug_inner() : left->debug() ) + " " +
                        operator_string() + " " +
                        ( right->get<reference>() ? right->get<reference>()->debug_inner() : right->debug() );
            }
			virtual bool test(const map &params)
            {
                std::string left_value = left->evaluate(params);
                std::string right_value = right->evaluate(params);
                return apply_operator(left_value, right_value);
            }
            virtual bool apply_operator(const std::string &left_value, const std::string &right_value) = 0;
            node_ptr left;
            node_ptr right;
        };

        struct equals_operator : binary_operator
        {
            equals_operator(node_ptr left, node_ptr right) : binary_operator(left, right)
            {
            }

            virtual std::string operator_string() { return "=="; }
            virtual bool apply_operator(const std::string &left_value, const std::string &right_value)
            {
                return left_value == right_value;
            }
        };
    
		struct if_directive : node
		{
			template <typename tuple> if_directive(tuple & t)
			{
				using boost::fusion::at_c;
				condition_nodes.push_back(at_c<0>(t));
				part_nodes.push_back(at_c<1>(t));
				for (auto &elseif : at_c<2>(t))
				{
					condition_nodes.push_back(at_c<0>(elseif));
					part_nodes.push_back(at_c<1>(elseif));
				}
                
				if (at_c<3>(t)) part_nodes.push_back(*at_c<3>(t));
			}
			virtual ~if_directive() {}
			virtual std::string evaluate(const map &params)
			{
				if (!condition_nodes.size() || part_nodes.size() < condition_nodes.size() || part_nodes.size() > condition_nodes.size() + 1) throw evaluation_error("malformed #if directive");
                int cond = 0;
                for (; cond < condition_nodes.size(); ++cond)
                {
                    if (condition_nodes[cond]->test(params)) return part_nodes[cond]->evaluate(params);
                }
                return part_nodes.size() > cond ? part_nodes[cond]->evaluate(params) : std::string();
			}
			virtual std::string debug()
			{
                std::string dbg;
                int cond = 0;
                for (; cond < condition_nodes.size(); ++cond)
                {
                    if (cond == 0) dbg = "{#if "; else dbg += "{#elseif ";
                    reference* ref = condition_nodes[cond]->get<reference>();
					std::string cond_string;
                    if (ref) cond_string = ref->debug_inner();
                    else cond_string = condition_nodes[cond]->debug();
                    dbg += cond_string + "}" + part_nodes[cond]->debug();
                }
                if (part_nodes.size() > cond) dbg + "{#else}" + part_nodes[cond]->debug();
                dbg += "{#end}";
                return dbg;
			}
            virtual bool test(const map &) { throw evaluation_error("#if directive cannot be tested"); }
			std::vector<node_ptr> condition_nodes;
            std::vector<node_ptr> part_nodes;
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
				std::string itname = iterator->get<reference>()->identifiers[0];
				const boost::any & values = collection->get<reference>()->resolve(params);
				const vector *objects = boost::any_cast<const vector>(&values);
				vector fallback_vector;
				if (!objects)
				{
					fallback_vector.push_back(values);
					objects = &fallback_vector;
				}
				map loop_params(params);
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

            virtual bool test(const map &) { throw evaluation_error("#join directive cannot be tested"); }
            
			virtual std::string debug()
			{
				return "{#join $" + iterator->get<reference>()->identifiers[0] + " in $" + boost::join(collection->get<reference>()->identifiers, ".") +
					( separator ? " with '" + separator->debug() + "'" : std::string() ) + "}" + content->debug() + "{#end}";
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
        auto new_condition = [](auto &ctx)
        {
            using boost::fusion::at_c;
            auto attr = _attr(ctx);
            if (at_c<1>(attr)) _val(ctx) = ast::node_ptr(new ast::equals_operator(at_c<0>(attr), *at_c<1>(attr)));
            else _val(ctx) = at_c<0>(attr);
        };
    
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
        DECLARE_RULE( binary_operator, ast::node_ptr )

		DEFINE_RULE( tiny_template, template_part >> eoi )
		DEFINE_RULE( template_part, ( *( variable | directive | plain_text) ) [ new_parent ] )
		DEFINE_RULE( directive, if_directive | join_directive )
		DEFINE_RULE( if_directive, ( "{#if" >> omit[+space] >> condition >> '}' >> template_part >> *( "{#elseif" >> omit[+space] >> condition >> '}' >> template_part ) >> -( "{#else}" >> template_part ) >> "{#end}" ) [ new_if_directive ] )
		// CB TODO - only supported conditions, for now, are a boolean check on a reference and the equality operator "=="
		DEFINE_RULE( condition, ( omit[*space] >> value >> omit[*space] >> -( omit[raw["=="]] >> omit[*space] >> value ) ) [new_condition] )
		DEFINE_RULE( join_directive, ( "{#join" >> omit[+space] >> reference >> omit[+space] >> "in" >> omit[+space] >> reference >> -( omit[+space >> "with" >> +space] >> value ) >> '}' >> template_part >> "{#end}" ) [ new_join_directive] )
		DEFINE_RULE( value, reference | literal_string )
		DEFINE_RULE( variable, '{' >> reference >> '}' )
 		DEFINE_RULE( reference, '$' >> ( identifier % '.' ) [ new_reference ] )
		DEFINE_RULE( identifier, +( alnum | char_('_') ) )
		DEFINE_RULE( literal_string, '\'' >> raw[ +(char_ - '\'') ] [ new_text ] >> '\'' ) // CB TODO - escaping apos
		DEFINE_RULE( plain_text, raw[ +( char_ - '{' ) ] [ new_text ] )
        // DEFINE_RULE( binary_operator, ( raw["=="] | ... ) [ new_text ] ) TODO
		
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
