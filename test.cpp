#include <fstream>
#include <streambuf>
#include <iostream>
#include <string>
#include "tiny_template.h"

// compile with:
// g++ -std=c++14 test.cpp tiny_template.cpp -o test_ttl

void test1()
{
    ttl::tiny_template_ptr tmpl = ttl::tiny_template::parse(
        "hello {#if $name}{$name}{#elseif $surname}{$surname}{#else}John{#end}, you have the following items: "
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

/* in progress...
void test2()
{
    std::ifstream in("test_complex.ttl");
    std::string instr((std::istreambuf_iterator<char>(in)),
                    std::istreambuf_iterator<char>());
    ttl::tiny_template_ptr tmpl = ttl::tiny_template::parse(instr);
    ttl::context context;
    context["name"] = "arthur";
    context["items"] = ttl::vector( { "foo", "bar" } );

    std::cout << "parsed template:" << std::endl;
    std::cout << tmpl->debug() << std::endl;

    std::cout << "result:" << std::endl;
    
    std::string result = tmpl->evaluate(context);
    
    std::cout << result << std::endl;
}
*/

int main(int argv, char* argc[])
{
    test1();
//    test2();
    return 0;
}
