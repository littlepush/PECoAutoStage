/*
    costage.cpp
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

#include "costage.h"

namespace coas {
    // Class: Stage 
    // a stage will process a costage file and read each line and execute it.
    // Create a new sub stack and return the name
    // Will create both parser item and code part
    std::string costage::stack_create_() {
        ptr_stack_type _pstack = std::make_shared< rpn::stack_type >();
        std::string _stack_name = utils::ptr_str(_pstack.get());

        // Create parser part
        ptr_parser_type _pparser = std::make_shared< rpn::parser_stack_type >();
        _pparser->name = _stack_name;

        // Switch current parser
        parser_queue_.push(_pparser);
        parser_ = _pparser;

        // Return the name
        return _stack_name;
    }

    // Jump to a stack
    // Switch current runtime envorinment
    void costage::stack_jump_(const std::string& name) {
        ptr_runtime_type _pruntime = std::make_shared< rpn::runtime_stack_type >();
        _pruntime->name = name;
        // Copy the code
        _pruntime->exec = *(code_map_[name]);
        // Push Current runtime
        runtime_queue_.push(_pruntime);
        // Switch
        exec_ = _pruntime;
    }

    // Exit a stack
    // Return from a current stack and switch to last stack
    void costage::stack_return_( ) {
        rpn::item_t _ret{rpn::IT_ERROR, Json::Value(Json::nullValue)};
        if ( exec_->data.size() == 0 ) {
            // No data in stack, return void
            _ret.type = rpn::IT_VOID;
        } else if ( exec_->data.size() == 1 ) {
            // We have one value for return
            _ret = exec_->data.top();
        } else {
            // Error, more than one value
            _ret.value = "more than one value for return";
        }

        // Pop Current stack, release it
        runtime_queue_.pop();
        // Current runtime should be the last stack
        if ( runtime_queue_.size() == 0 ) {
            exec_ = nullptr;
            result_t = _ret;
        } else {
            exec_ = runtime_queue_.top();
            exec_->data.push(_ret);
        }
    }
    // Get a node by path
    Json::Value* costage::node_by_path_(const Json::Value& path_value) {
        Json::Value* _p = &root_;
        size_t _i = 0;
        std::string _last_node = "$";
        for ( auto i = path_value.begin(); i != path_value.end(); ++i, ++_i ) {
            if ( i->isInt() ) {
                // This should be a array's index
                int _index = i->asInt();
                if ( _p->isNull() ) {
                    (*_p) = Json::Value(Json::arrayValue);
                }
                if ( !_p->isArray() ) {
                    err_ = "path node `" + _last_node + "` is not an array.";
                    return NULL;
                }
                if ( _p->size() <= _index ) {
                    if ( _index == _p->size() ) {
                        // Create a new node
                        if ( _i == (path_value.size() - 1) ) {
                            // Already reach end of the path
                            // Add a null
                            _p->append(Json::Value(Json::nullValue));
                        } else {
                            _p->append(Json::Value(Json::objectValue));
                        }
                    } else {
                        err_ = "`" + _last_node + "` index " + std::to_string(_index) + " is out of range.";
                        return NULL;
                    }
                }
                _p = &(*_p)[_index];
                _last_node += (".[" + std::to_string(_index) + "]");
            } else {
                std::string _node = (*i).asString();
                if ( !_p->isMember(_node) ) {
                    if ( _i == (path_value.size() - 1) ) {
                        // Already reach end of the path
                        // Add a null
                        (*_p)[_node] = Json::Value(Json::nullValue);
                    } else {
                        (*_p)[_node] = Json::Value(Json::objectValue);
                    }
                }
                _p = &(*_p)[_node];
                _last_node += ("." + _node);
            }
        }
        return _p;
    }
}

// Push Chen
//