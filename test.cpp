#include <iostream>
#include <string>
#include "tiny_template.h"

// compile with:
// g++ -std=c++14 test.cpp tiny_template.cpp -o test_ttl

int main(int argv, char* argc[])
{
    ttl::tiny_template_ptr tmpl = ttl::tiny_template::parse(
        "hello {$name}, you have the following items: "
        "{#join $item in $items with ', '}{$item}{#if $item == 'foo'}!{#end}{#end}");
    ttl::context context;
    context["name"] = "arthur";
    context["items"] = ttl::vector( { "foo", "bar" } );

    std::cout << "parsed template:" << std::endl;
    std::cout << tmpl->debug() << std::endl;

    std::cout << "result:" << std::endl;
    
    std::string result = tmpl->evaluate(context);
    
    std::cout << result << std::endl;
}

