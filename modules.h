/*
    modules.h
    2019-12-13
    PECoAutoStage
    Push Chen
*/

/*
MIT License

Copyright (c) 2019 Push Chen

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once

#ifndef PECO_AUTO_STAGE_MODULES_H__
#define PECO_AUTO_STAGE_MODULES_H__

#include <peutils.h>
using namespace pe;

#include "rpn.h"

namespace coas {

    // Pre-Definition
    class costage;

    typedef std::function< rpn::item_t ( costage&, const rpn::item_t&, const std::list< rpn::item_t >& ) > 
        module_function_t;
    typedef rpn::item_t(*module_f)(costage&, const rpn::item_t&, const std::list< rpn::item_t >&);

    typedef std::function< bool ( const Json::Value&, bool ) > 
        module_match_t;
    typedef bool(*module_mf)(const Json::Value&, bool);

    typedef std::string(*module_nf)(void);

    typedef struct {
        std::string             name;
        module_match_t          is_match;
        module_function_t       exec;
    } module_type;

    typedef std::shared_ptr< module_type >  ptr_module_type;

    class module_manager {

        typedef std::list< ptr_module_type >                module_list_t;
        typedef std::shared_ptr< module_list_t >            ptr_module_list_type;
        std::map< std::string, ptr_module_list_type >       module_map_;

        // C'str
        module_manager();

        // Singleton Instance
        static module_manager& singleton();
    public: 

        ~module_manager();

        // Initialize all default modules
        static void init_default_modules();

        // Register a new module
        static void register_module( module_type&& m );

        // Search if a module with name has been registered
        static ptr_module_type search_module( 
            const std::string& name, 
            const Json::Value& invoker,
            bool is_root
        );

        // Utility
        // Create a error object for returning
        static rpn::item_t ret_error( const std::string& message );
    };

}

#endif 

// Push Chen
//