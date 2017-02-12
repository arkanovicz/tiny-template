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

#ifndef __TINY_TEMPLATE__
#define __TINY_TEMPLATE__

#include <boost/any.hpp>
#include <map>
#include <string>
#include <vector>

// Tiny Template Language
//
// Usage:
//
//		ttl::tiny_template_ptr tmpl = ttl::tiny_template::parse(
//      "hello {#if $foo}{$foo.bar}{#else}john{#end}; "
//      "items = {#join $item in $coll with ','}{$item}{#end}");
//		ttl::context ctx
//		{
//			{ "foo", ttl::map({ { "bar", "world" } }) },
//			{ "coll", ttl::vector({ "item_1", "item_2" }) }
//		};
//		std::string result = tmpl->evaluate(ctx);
//		std::cout << "result: " << result << std::endl;
//
//   --> will produce the output: result: hello world - items = item_1,item_2
//
// Syntax reference ( brackets [ ] denote optional portions ):
//
//   {$reference[.property[.property...]]}
//   {#if $reference[.propery...]} ... [ {#else} ... ] {#end}
//   {#join $iterator in $collection [ with 'value' ]} ... {#end}
//

namespace ttl
{
	// context
	
  // boost::any values can be: string, std::vector<boost::any> and map<string, boost::any>
  typedef std::map<std::string, boost::any> map;
	typedef std::vector<boost::any> vector;
	using context = map;

	// utility

	std::string to_json(const context &ctx);
	
	// grammar
	
  namespace ast
	{
		struct node;
		typedef std::shared_ptr<node> node_ptr;
		
		struct node
		{
			virtual ~node() {}
			static node_ptr parse(const std::string &);
			virtual std::string evaluate(const map &) = 0;
			virtual std::string debug() = 0;
			template <typename T> T* get() { return dynamic_cast<T*>(this); }
		};
		
	} // namespace ast

	// template
	
	using tiny_template = ast::node;
	using tiny_template_ptr = ast::node_ptr;

	// errors
	
	class parsing_error : public std::exception
	{
	public:
		parsing_error() : message("parsing error") {}
		parsing_error(const std::string &msg) : message(msg) {}
		virtual const char* what() const noexcept { return message.data(); }
	private:
		std::string message;
	};

	class evaluation_error : public std::exception
	{
	public:
		evaluation_error() : message("evaluation error") {}
		evaluation_error(const std::string &msg) : message(msg) {}
		virtual const char* what() const noexcept { return message.data(); }
	private:
		std::string message;
	};
	
} // namespace ttl

#endif // __TINY_TEMPLATE__

