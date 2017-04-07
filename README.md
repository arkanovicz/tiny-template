# tiny_template

Tiny C++ template language based on boost::spirit x3.

## Usage
    
    #include "tiny_templates.h"
    
    ...
    
    // parse a template
    ttl::tiny_template_ptr tmpl = ttl::tiny_template::parse(
      "hello {#if $foo}{$foo.bar}{#else}john{#end}; "
      "items = {#join $item in $coll with ','}{$item}{#end}");
    
    // create a context
    ttl::context ctx
    {
        { "foo", ttl::map({ { "bar", "world" } }) },
        { "coll", ttl::vector({ "item_1", "item_2" }) }
    };
    
    // evaluate the template against the context
    std::string result = tmpl->evaluate(ctx);
    std::cout << "result: " << result << std::endl;
   
   
This code will produce the output:

    result: hello world - items = item_1,item_2

## Syntax reference

( brackets denote optional portions )

    {$reference[.property[.property...]]}
    {#if <condition> } ... [ {#elseif <condition>} ... ] [ {#else} ... ] {#end}
    where <condition> is: $reference[.propery...] [ == <value> ]
    {#join $object in $collection [ with 'value' ]} ...${object}...  {#end}


## Requirements

Boost v1.61 or more recent, and a decent c++11 compiler.
