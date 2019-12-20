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

    // Const empty string object
    extern const std::string empty_string;

    // Pre-Definition
    class costage;

    typedef std::function< rpn::item_t ( costage&, const rpn::item_t&, const std::vector< rpn::item_t >& ) > 
        module_function_t;
    typedef rpn::item_t(*module_f)(costage&, const rpn::item_t&, const std::vector< rpn::item_t >&);

    typedef std::function< bool ( const Json::Value&, bool ) > 
        module_match_t;
    typedef bool(*module_mf)(const Json::Value&, bool);

    typedef std::string(*module_nf)(void);

    typedef struct {
        std::string             name;
        module_match_t          is_match;
        module_function_t       exec;
    } module_type;

    // Module utilities
    // Return error object with message
    rpn::item_t module_error(const std::string& message);

    // Return a void item
    rpn::item_t module_void();

    // Return a NULL item
    rpn::item_t module_null();

    // Return an Array
    rpn::item_t module_array(Json::Value&& array);

    // Return an Object
    rpn::item_t module_object(Json::Value&& object);

    // Return a Number
    rpn::item_t module_number(Json::Value&& number);

    // Return a string
    rpn::item_t module_string(Json::Value&& jstr);

    // Return a Boolean
    rpn::item_t module_boolean(bool value);

    // Return an item by the value type
    rpn::item_t module_bytype(Json::Value&& value);

    // Return an path
    rpn::item_t module_path(Json::Value&& path);

    // Built-in Match Call

    // Match anything, always return true
    bool module_match_anything( const Json::Value& invoker, bool is_root );

    // Match anything but null
    bool module_match_notnull( const Json::Value& invoker, bool is_root );

    // Default for object and array
    bool module_match_default( const Json::Value& invoker, bool is_root );

    // Match a root invoker
    bool module_match_root( const Json::Value& invoker, bool is_root );

    // Match Only an Array invoker
    bool module_match_array( const Json::Value& invoker, bool is_root );

    // Match Only an Object
    bool module_match_object( const Json::Value& invoker, bool is_root );

    // Match Only a String
    bool module_match_string( const Json::Value& invoker, bool is_root );

    // Match Only a Number
    bool module_match_number( const Json::Value& invoker, bool is_root );

    // Match With Specified type
    // The invoker must be an object and contains key: "__type"
    // Usage std::bind(&module_match_type, "my_type")
    bool module_match_type( const std::string& type, const Json::Value& invoker, bool is_root );

    // Match Specifial Key
    // The invoker must be an object and contains specifial key
    bool module_match_key( const std::string& key, const Json::Value& invoker, bool is_root );

    // Match both key and type
    bool module_match_key_and_type( 
        const std::string& key, 
        const std::string& type, 
        const Json::Value& invoker, bool is_root 
    );

    // Match Job Object
    bool module_match_job( const Json::Value& invoker, bool is_root );

    // Match a condition
    bool module_match_condition( const Json::Value& invoker, bool is_root );

    // Prepare to call with array index
    void module_prepare_call(
        costage& stage, 
        const rpn::item_t& this_path, 
        int ithis = -1, int ilast = -1, int icurrent = -1
    );

    // Prepare to call with object key
    void module_prepare_call(
        costage& stage,
        const rpn::item_t& this_path,
        const std::string& cthis = empty_string,
        const std::string& clast = empty_string,
        const std::string& ccurrent = empty_string
    );

    // Clear the call stack
    void module_clear_call(costage& stage);

    // Unpack argument
    const Json::Value* module_unpack_arg( costage& stage, const rpn::item_t& arg );
    const Json::Value* module_unpack_arg( 
        costage& stage, 
        const rpn::item_t& arg, 
        rpn::item_type type 
    );

    // Get a Stack Name according to the arg
    std::string module_unpack_stack( costage& stage, const rpn::item_t& arg );

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
    };

}

#endif 

// Push Chen
//