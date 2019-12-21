/*
    costage.h
    2019-12-12
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

#ifndef PECO_AUTO_STAGE_COSTAGE_H__
#define PECO_AUTO_STAGE_COSTAGE_H__

#include <peutils.h>
using namespace pe;

#include "rpn.h"
#include "modules.h"

namespace coas {

    // Input State
    typedef enum {
        // Input is ok
        I_OK,
        // Input unfinished
        I_UNFINISHED,
        // Input has syntax error
        I_SYNTAX
    } I_STATE;

    // Executing State
    typedef enum {
        // Execute OK
        E_OK,
        // Assert Failed
        E_ASSERT,
        // Break and return
        E_RETURN
    } E_STATE;

    // Class: Stage 
    // a stage will process a costage file and read each line and execute it.
    class costage {
    public: 
        // Types
        typedef std::shared_ptr< rpn::stack_type >              ptr_stack_type;
        typedef std::pair< ptr_stack_type, std::string >        code_item_type;
        typedef std::list< code_item_type >                     stack_group_type;
        typedef std::shared_ptr< stack_group_type >             ptr_group_type;
        typedef std::shared_ptr< rpn::parser_stack_type >       ptr_parser_type;
    protected: 

        // Environments
        // A stage's root node
        Json::Value                         root_;
        // The error message
        std::string                         err_;
        // Void Item
        Json::Value                         void_;
        // Return Item
        rpn::item_t                         return_;
        // Assert Item
        Json::Value                         assert_;
        // Temp Object
        Json::Value                         temp_obj_;

        // This Keyword
        rpn::stack_type                     this_stack_;
        // Last Keyword
        rpn::stack_type                     last_stack_;
        // Current Keyword
        rpn::stack_type                     current_stack_;
        // Local Storage
        rpn::stack_type                     local_stack_;

        // Exection Log
        std::vector< std::pair< std::string, double > > exec_log_;

        // The main entry group name
        std::string                         entry_group_;

        // Code Map, contains all sub stacks
        std::map< std::string, ptr_group_type >         code_map_;
        // Group Stack
        std::stack< std::string >                       group_stack_;
        // Parser Stack
        std::stack< ptr_parser_type >                   parser_stack_;
        // Call Stack
        std::list< std::string >                        call_stack_;

        // Current Code Group
        std::string                         current_group_;
        ptr_group_type                      group_;
        // Current Code Stack
        ptr_stack_type                      exec_;
        // Current Parser
        ptr_parser_type                     parser_;

        // Information of a stage
        std::set< std::string >             tags_;
        std::string                         name_;
        std::string                         description_;

    protected: 
        // Internal navigation

        // Create a new code group
        std::string group_create_();

        // Leave current group and pop to the last level
        void group_leave_();

        // Create a new sub stack and return the name
        // Will create both parser item and code part
        void stack_create_(const std::string& original_code);

        // We finish to parse a code line and release the parser
        void stack_finish_();

        // Get a node by path
        // If the path is not in module list, then create a new nullObject
        // If match any module, then run the module and put the result as the node.
        Json::Value* node_by_path_(const Json::Value& path_value);

        // Parser And Runner
        // Parse the operator item
        bool operator_parser_( size_t index, rpn::item_t& op );

        // Run a line of code
        E_STATE code_line_run_( ptr_stack_type code, const std::string& original_code );

        // Run Code Group
        E_STATE code_group_run_( const std::string& group_name );
    public: 

        // Result Reference
        const rpn::item_t&          resultItem() const;

        // Return Value
        const Json::Value&          returnValue() const;

        // Root Node
        const Json::Value&          rootValue() const;
        void replace_root(const Json::Value& new_root);

        // Name
        const std::string           name() const;
        void set_name( const std::string& n );

        // Description
        const std::string           description() const;
        void set_description( const std::string& d );

        // Call stack
        const std::list< std::string >& call_stack() const;

        // Tags
        const std::set< std::string > tags() const;
        void add_tag( const std::string& t );

        // Dump self info to json
        void info_toJson(Json::Value& node) const;
        // Dump the call stack to json
        void stack_toJson(Json::Value& node) const;

        // Create an empty stage, default C'str
        costage();
        // Create a stage with some root data
        costage(const Json::Value& prepared_root);
        ~costage();

        // Get the error message
        const std::string& err_string() const;

        // Parse a code line
        I_STATE code_parser( const std::string& code, const std::string& file, uint32_t lineno );
        
        // Exec the last parsed RPN object
        E_STATE code_run();

        // Clear stack info and start a new environment
        void code_clear();

        // Public Methods

        // Search for a node
        Json::Value* get_node( const Json::Value& path_value );
        // This/Last Setting
        bool push_this( rpn::item_t&& this_path );
        void pop_this();
        bool push_last( rpn::item_t&& last_path );
        void pop_last();
        bool push_current( rpn::item_t&& current_path );
        void pop_current();
        // Invoke a sub code group
        E_STATE invoke_group( const std::string& group_name );

        // The log of external executions
        void add_exec_log( const std::string& message, double time_used );
        void dump_exec_log( Json::Value& node ) const;

        // Create a function, and the following code input for parse is in the function
        bool create_function( const std::string& func_name );
        // Mark last function to be end
        void end_function();
    };

}

#endif 

// Push Chen
//