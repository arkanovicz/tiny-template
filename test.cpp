#include <iostream>
#include <string>
#include "tiny_template.h"

// compile with:
// g++ -std=c++14 test.cpp tiny_template.cpp -o test_ttl

int main(int argv, char* argc[])
{
    ttl::tiny_template_ptr tmpl = ttl::tiny_template::parse(
        "hello {$name}, you have the following items: "
        "{#join $item in $items with ', '}{$item}{#end}");
    
    ttl::context context;
    context["name"] = "arthur";
    context["items"] = ttl::vector( { "foo", "bar" } );

    std::string result = tmpl->evaluate(context);
    
    std::cout << result << std::endl;
}

