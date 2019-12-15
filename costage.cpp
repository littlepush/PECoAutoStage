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
#include <cotask.h>

using namespace pe::co;

namespace coas {
    // Class: Stage 
    // a stage will process a costage file and read each line and execute it.

    // Create a new code group
    std::string costage::group_create_() {
        ptr_group_type _ngroup = std::make_shared< stack_group_type >();
        std::string _group_name = utils::ptr_str(_ngroup.get());
        code_map_[_group_name] = _ngroup;
        group_stack_.push(_group_name);

        // Current Group Switch to the new group
        group_ = _ngroup;
        current_group_ = _group_name;

        exec_ = nullptr;
        parser_ = nullptr;

        return _group_name;
    }

    // Leave current group and pop to the last level
    void costage::group_leave_() {
        if ( group_stack_.size() > 0 ) {
            group_stack_.pop();
        }
        if ( group_stack_.size() > 0 ) {
            current_group_ = group_stack_.top();
            group_ = code_map_[current_group_];
        } else {
            group_ = nullptr;
        }
        if ( group_ != nullptr ) {
            parser_stack_.pop();
            parser_ = parser_stack_.top();
            exec_ = (*group_->rbegin()).first;            
        } else {
            parser_ = nullptr;
            exec_ = nullptr;
        }
    }

    // Create a new sub stack and return the name
    // Will create both parser item and code part
    void costage::stack_create_(const std::string& original_code) {
        if ( group_ == nullptr ) {
            entry_group_ = group_create_();
        }

        // Last code is UNFINISHED
        if ( exec_ != nullptr ) return;

        exec_ = std::make_shared< rpn::stack_type >();
        group_->push_back(std::make_pair(exec_, original_code));

        // Create parser part
        ptr_parser_type _pparser = std::make_shared< rpn::parser_stack_type >();
        _pparser->plv = 0;

        // Switch current parser
        parser_stack_.push(_pparser);
        parser_ = _pparser;
    }

    // We finish to parse a code line and release the parser
    void costage::stack_finish_() {
        parser_stack_.pop();
        parser_ = nullptr;
        exec_ = nullptr;
    }

    // Get a node by path
    Json::Value* costage::node_by_path_(const Json::Value& path_value) {
        Json::Value* _p = &root_;
        std::string _root_item = path_value[0].asString();
        if ( _root_item == "@" ) {
            // Local Item
            _p = &local_stack_.top().value;
        } else if ( _root_item == "_" ) {
            // Temp Item
            _p = &temp_obj_;
        }
        if ( path_value.size() == 1 ) return _p;    // Only Root
        size_t _i = 1;

        if ( path_value[1].isString() && _root_item == "$" ) {
            std::string _keyword = path_value[1].asString();
            if ( _keyword == "this" ) {
                if ( this_stack_.size() == 0 ) {
                    err_ = "`this` = 0x0"; return NULL;
                }
                if ( this_stack_.top().type != rpn::IT_PATH ) {
                    err_ = "`this` is not validate path"; return NULL;
                }
                Json::Value _this_path = this_stack_.top().value;
                for ( Json::ArrayIndex i = 2; i < path_value.size(); ++i ) {
                    _this_path.append(path_value[i]);
                }
                return node_by_path_(_this_path);
            } else if ( _keyword == "last" ) {
                if ( last_stack_.size() == 0 ) {
                    err_ = "`last` = 0x0"; return NULL;
                }
                if ( last_stack_.top().type != rpn::IT_PATH ) {
                    err_ = "`last` is not a validate path"; return NULL;
                }
                Json::Value _last_path = last_stack_.top().value;
                for ( Json::ArrayIndex i = 2; i < path_value.size(); ++i ) {
                    _last_path.append(path_value[i]);
                }
                return node_by_path_(_last_path);
            } else if ( _keyword == "void" ) {
                if ( path_value.size() != 2 ) {
                    err_ = "`void` cannot have subpath"; return NULL;
                }
                return &void_;
            } else if ( _keyword == "return" ) {
                if ( path_value.size() != 2 ) {
                    err_ = "`return` cannot have subpath"; return NULL;
                }
                return &return_;
            } else if ( _keyword == "assert" ) {
                if ( path_value.size() != 2 ) {
                    err_ = "`assert` cannot have subpath"; return NULL;
                }
                return &assert_;
            }
        }

        std::string _last_node = _root_item;
        auto i = path_value.begin();
        ++i;
        for ( ; i != path_value.end(); ++i, ++_i ) {
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
                    // alwasy add a null
                    (*_p)[_node] = Json::Value(Json::nullValue);
                }
                if ( _p->isNull() ) {
                    // Replace the null object
                    *_p = Json::Value(Json::objectValue);
                }
                _p = &(*_p)[_node];
                _last_node += ("." + _node);
            }
        }
        return _p;
    }
    bool costage::operator_parser_( size_t index, rpn::item_t& op ) {
        // We get the end of []
        if ( op.type == rpn::IT_RIGHT_BRACKETS ) {
            while ( 
                parser_->op.size() != 0 && 
                parser_->op.top().type != rpn::IT_LEFT_BRACKETS
            ) {
                parser_->item.push(parser_->op.top());
                parser_->op.pop();
            }
            if ( parser_->op.size() == 0 ) {
                err_ = "unmatched ']' at index: " + std::to_string(index);
                return false;
            }
            // Remove the '['
            parser_->op.pop();
            return true;
        }

        // If no op
        if ( parser_->op.size() == 0 ) {
            parser_->op.push(op); return true;
        }

        // Meet a ')', need to pop until '('
        if ( op.type == rpn::IT_RIGHT_PARENTHESES || op.type == rpn::IT_COMMA ) {
            while ( 
                parser_->op.size() != 0 && 
                parser_->op.top().type != rpn::IT_LEFT_PARENTHESES
            ) {
                parser_->item.push(parser_->op.top());
                parser_->op.pop();
            }
            if ( op.type == rpn::IT_RIGHT_PARENTHESES ) {
                if ( parser_->op.size() == 0 ) {
                    err_ = "unmatched ')' at index: " + std::to_string(index);
                    return false;
                }
                // Remove the '('
                parser_->op.pop();
            }
            return true;
        }

        // If last op is '(', means we are in a 
        // high priority code block
        if ( parser_->op.top().type == rpn::IT_LEFT_PARENTHESES ) {
            parser_->op.push(op); return true;
        }

        if ( parser_->op.top().type == rpn::IT_LEFT_BRACKETS ) {
            parser_->op.push(op); return true;
        }

        // If current op has a higher level, we need
        // to execute it first
        if ( parser_->op.top().type < op.type ) {
            parser_->op.push(op); return true;
        }

        // Create RPN order, pop all pending order until a '('
        while (
            parser_->op.size() > 0 && 
            parser_->op.top().type > op.type && 
            parser_->op.top().type != rpn::IT_LEFT_PARENTHESES &&
            parser_->op.top().type != rpn::IT_LEFT_BRACKETS
        ) {
            parser_->item.push(parser_->op.top());
            parser_->op.pop();
        }
        parser_->op.push(op);
        return true;
    }
    // Create an empty stage, default C'str
    costage::costage() : 
        void_(Json::nullValue), return_(Json::nullValue),
        group_(nullptr), exec_(nullptr), parser_(nullptr), 
        result(result_), returnValue(return_), rootValue(root_) 
    {
        result_.type = rpn::IT_VOID;
    }
    // Create a stage with some root data
    costage::costage(const Json::Value& prepared_root) :
        root_(prepared_root),
        void_(Json::nullValue), return_(Json::nullValue),
        group_(nullptr), exec_(nullptr), parser_(nullptr), 
        result(result_), returnValue(return_), rootValue(root_) 
    {
        result_.type = rpn::IT_VOID;
    }

    costage::~costage() {
        code_clear();
    }

    // Get the error message
    const std::string& costage::err_string() const {
        return err_;
    }

    // Parse a code line
    I_STATE costage::code_parser( const std::string& code ) {

        std::string _code = code;
        #define __GET_C_N   (i == _code.size() - 1) ? '\0' : _code[i + 1]
        #define __GET_C     _code[i];
        #define __GET_C_P   (i == 0) ? '\0' : _code[i - 1]

        utils::trim(_code);
        utils::code_filter_comment(_code);
        if ( _code.size() == 0 ) return I_UNFINISHED;

        stack_create_(_code);

        char                    _c_p = '\0';
        for ( size_t i = 0; i < _code.size(); ++i ) {
            char c = __GET_C;
            char c_n = __GET_C_N;

            if ( std::isspace(c) ) continue;    // ignore unused space
            if ( c == ';' && c_n == '\0' ) continue;    // ignore the ';' at the end of line

            // Check if last checked char is an op one.
            bool _is_p_op = (
                _c_p == '+' || 
                _c_p == '-' ||
                _c_p == '*' ||
                _c_p == '/' ||
                _c_p == '=' ||
                _c_p == '(' ||
                _c_p == '[' || 
                _c_p == '{'
            );

            do {
                // If last char is not op, we should
                // treat '+' & '-' as an operator
                if ( !_is_p_op ) {
                    if ( c == '+' ) {
                        rpn::item_t _i{rpn::IT_PLUS, Json::Value("+")};
                        if ( !operator_parser_(i, _i) ) return I_SYNTAX;
                        break;
                    }
                    if ( c == '-' ) {
                        rpn::item_t _i{rpn::IT_MINUS, Json::Value("-")};
                        if ( !operator_parser_(i, _i) ) return I_SYNTAX;
                        break;
                    }
                }

                // Test if is true of false or null
                if ( _c_p != '.' && (c == 't' || c == 'f' || c == 'n') ) {
                    char _s[10];
                    size_t _i = 0, _si = i;
                    for ( ; 
                        (_i < 9) && (_si < _code.size()) && ::isalpha(_code[_si]);
                        ++_si, ++_i 
                    ) {
                        _s[_i] = _code[_si];
                    }
                    _s[_i] = '\0';
                    std::string _bs(_s);
                    if ( _bs == "true" ) {
                        rpn::item_t _b{rpn::IT_BOOL, Json::Value(true)};
                        parser_->item.push(_b);
                    } else if ( _bs == "false" ) {
                        rpn::item_t _b{rpn::IT_BOOL, Json::Value(false)};
                        parser_->item.push(_b);
                    } else if ( _bs == "null" ) {
                        rpn::item_t _n{rpn::IT_NULL, Json::Value(Json::nullValue)};
                        parser_->item.push(_n);
                    } else {
                        err_ = "invalidate word at index: " + std::to_string(i);
                        return I_SYNTAX;
                    }
                    i += _i;
                    break;
                }

                // Try to test if this is a number
                char *_pend = NULL;
                double _n = std::strtod(_code.c_str() + i, &_pend);
                if ( _pend != (_code.c_str() + i) ) {
                    if ( _n == HUGE_VAL || errno == ERANGE ) {
                        // Failed to get the number
                        err_ = "Invalidate number at index: " + std::to_string(i);
                        return I_SYNTAX;
                    }

                    // This is a number
                    rpn::item_t _i{rpn::IT_NUMBER, Json::Value(_n)};
                    parser_->item.push(_i);
                    size_t _l = _pend - (_code.c_str() + i);
                    i += (_l - 1);
                    break;
                }
                if ( c == ',' ) {
                    rpn::item_t _i{rpn::IT_COMMA, Json::Value(",")};
                    if ( !operator_parser_(i, _i) ) return I_SYNTAX;
                    break;
                }

                // A new node
                if ( c == '.' ) {
                    rpn::item_t _i{rpn::IT_NODE, Json::Value(".")};
                    if ( !operator_parser_(i, _i) ) return I_SYNTAX;
                    if ( c_n == '(' ) parser_->nodelv[parser_->plv] = true;
                    break;
                }

                // Begin of a path
                if ( c == '$' ) {
                    if ( c_n != '.' ) {
                        err_ = "Invalidate '$' at index: " + std::to_string(i);
                        return I_SYNTAX;
                    }
                    rpn::item_t _i{rpn::IT_STRING, Json::Value("$")};
                    parser_->item.push(_i);
                    break;
                }

                if ( c == '\"' ) {
                    // Try to get a string until '"'
                    std::string _s;
                    while ( c_n != '\"' && c_n != '\0' ) {
                        _s += c_n;
                        ++i;
                        c = __GET_C;
                        c_n = __GET_C_N;
                    }
                    if ( c_n == '\0' ) {
                        err_ = "invalidate end of string";
                        return I_SYNTAX;
                    }
                    ++i;    // Skip '"'
                    rpn::item_t _i{rpn::IT_STRING, Json::Value(_s)};
                    parser_->item.push(_i);
                    break;
                }

                if ( c == '*' ) {
                    rpn::item_t _i{rpn::IT_TIMES, Json::Value("*")};
                    if ( !operator_parser_(i, _i) ) return I_SYNTAX;
                    break;
                }
                if ( c == '/' ) {
                    rpn::item_t _i{rpn::IT_DIVID, Json::Value("/")};
                    if ( !operator_parser_(i, _i) ) return I_SYNTAX;
                    break;
                }
                if ( c == '%' ) {
                    rpn::item_t _i{rpn::IT_MOD, Json::Value("%")};
                    if ( !operator_parser_(i, _i) ) return I_SYNTAX;
                    break;
                }
                if ( c == '^' ) {
                    rpn::item_t _i{rpn::IT_POW, Json::Value("^")};
                    if ( !operator_parser_(i, _i) ) return I_SYNTAX;
                    break;
                }

                if ( c == '>' ) {
                    auto _t = ((c_n == '=') ? 
                        rpn::IT_GREAT_EQUAL : 
                        rpn::IT_GREAT_THAN
                    );
                    rpn::item_t _i{_t, Json::Value((c_n == '=') ? ">=" : ">")};
                    if ( !operator_parser_(i, _i) ) return I_SYNTAX;
                    i += (c_n == '=' ? 1 : 0);
                    break;
                }
                if ( c == '<' ) {
                    auto _t = ((c_n == '=') ? 
                        rpn::IT_LESS_EQUAL : 
                        rpn::IT_LESS_THAN
                    );
                    rpn::item_t _i{_t, Json::Value((c_n == '=') ? "<=" : "<")};
                    if ( !operator_parser_(i, _i) ) return I_SYNTAX;
                    i += (c_n == '=' ? 1 : 0);
                    break;
                }
                if ( c == '=' ) {
                    auto _t = ((c_n == '=') ? 
                        rpn::IT_EQUAL : 
                        rpn::IT_SET
                    );
                    rpn::item_t _i{_t, Json::Value((c_n == '=') ? "==" : "=")};
                    if ( !operator_parser_(i, _i) ) return I_SYNTAX;
                    i += (c_n == '=' ? 1 : 0);
                    break;
                }
                if ( c == '!' ) {
                    auto _t = ((c_n == '=') ?
                        rpn::IT_NOT_EQUAL : 
                        rpn::IT_NOT
                    );
                    rpn::item_t _i{_t, Json::Value((c_n == '=') ? "!=" : "!")};
                    if ( !operator_parser_(i, _i) ) return I_SYNTAX;
                    i += (c_n == '=' ? 1 : 0);
                    break;
                }
                if ( c == '(' ) {
                    rpn::item_t _i{rpn::IT_LEFT_PARENTHESES, Json::Value("(")};
                    if ( !operator_parser_(i, _i) ) return I_SYNTAX;
                    parser_->plv += 1;
                    break;
                }
                if ( c == ')' ) {
                    parser_->plv -= 1;
                    rpn::item_t _i{rpn::IT_RIGHT_PARENTHESES, Json::Value(")")};
                    if ( !operator_parser_(i, _i) ) return I_SYNTAX;
                    // Check if current () code block is a path node
                    if ( parser_->nodelv.find(parser_->plv) != parser_->nodelv.end() ) {
                        parser_->nodelv.erase(parser_->plv);
                        if ( c_n != '.' ) {
                            rpn::item_t _j{rpn::IT_NODE, Json::Value(".")};
                            if ( !operator_parser_(i, _j) ) return I_SYNTAX;
                        }
                    } else if ( parser_->arglv.find(parser_->plv) != parser_->arglv.end() ) {
                        parser_->arglv.erase(parser_->plv);
                        rpn::item_t _ea{rpn::IT_EOA, Json::Value(-2)};
                        parser_->item.push(_ea);
                    }
                    break;
                }
                if ( c == '[' ) {
                    rpn::item_t _i{rpn::IT_LEFT_BRACKETS, Json::Value("[")};
                    if ( !operator_parser_(i, _i) ) return I_SYNTAX;
                    break;
                }
                if ( c == ']' ) {
                    rpn::item_t _i{rpn::IT_RIGHT_BRACKETS, Json::Value("]")};
                    if ( !operator_parser_(i, _i) ) return I_SYNTAX;
                    if ( c_n != '.' ) {
                        rpn::item_t _n{rpn::IT_NODE, Json::Value(".")};
                        if ( !operator_parser_(i, _n) ) return I_SYNTAX;
                    }
                    break;
                }
                // TODO: When add EXEC? or JUMP?
                // Create a sub stack when find '{' in next line
                if ( c == '{' ) {
                    if ( c_n != '\0' ) {
                        err_ = "invalidate '{', following code must begin in a new line";
                        return I_SYNTAX;
                    }
                    // Create a new code group and wait for code lines
                    group_create_();
                    return I_UNFINISHED;
                }
                // Until we meet matched '}', all status should be I_UNFINISHED
                if ( c == '}' ) {
                    if ( parser_stack_.size() == 1 ) {
                        err_ = "invalidate '}', missing '{'";
                        return I_SYNTAX;
                    }
                    rpn::item_t _s{rpn::IT_STACK, Json::Value(current_group_)};
                    group_leave_();
                    parser_->item.push(_s);
                    break;
                }

                // only left case is a normal node name
                if ( !std::isalpha(c) && c != '_' ) {
                    err_ = std::string("Invalidate '") + std::string((char)c, 1) + 
                        "' at index: " + std::to_string(i);
                    return I_SYNTAX;
                }
                std::string _s(1, c);
                while ( std::isalpha(c_n) || std::isdigit(c_n) || c_n == '_' ) {
                    _s += c_n;
                    ++i;
                    c = __GET_C;
                    c_n = __GET_C_N;
                }
                // Exec, not a node
                if ( c_n == '(' ) {
                    rpn::item_t _e{rpn::IT_EXEC, Json::Value(_s)};
                    if ( !operator_parser_(i, _e) ) return I_SYNTAX;
                    rpn::item_t _ba{rpn::IT_BOA, Json::Value(-1)};
                    parser_->item.push(_ba);
                    parser_->arglv[parser_->plv] = true;
                    break;
                }
                rpn::item_t _i{rpn::IT_STRING, Json::Value(_s)};
                parser_->item.push(_i);
                // If this is the last part of the path
                // add a node op
                if ( c_n != '.' ) {
                    rpn::item_t _n{rpn::IT_NODE, Json::Value(".")};
                    if ( !operator_parser_(i, _n) ) return I_SYNTAX;
                }
            } while ( false );

            // Update Previous Char
            _c_p = _code[i];
        }

        while ( parser_->op.size() != 0 ) {
            parser_->item.push(parser_->op.top());
            parser_->op.pop();
        }

        if ( parser_->item.top().type != rpn::IT_SET ) {
            err_ = "missing '=' in code";
            return I_SYNTAX;
        }

        // Get current code block
        while ( parser_->item.size() != 0 ) {
            exec_->push(parser_->item.top());
            parser_->item.pop();
        }

        stack_finish_();
        return (parser_stack_.size() == 0) ? I_OK : I_UNFINISHED;
    }

    // Run a line of code
    E_STATE costage::code_line_run_( ptr_stack_type code, const std::string& original_code ) {
        // Copy the code
        rpn::stack_type _runtime(*code);
        // New data stack
        rpn::stack_type _data;

        // Node Path stack
        std::stack< Json::Value >   _node_stack;

        while ( _runtime.size() != 0 ) {
            rpn::item_t _rpn = _runtime.top();
            _runtime.pop();

            switch (_rpn.type) {
                case rpn::IT_NUMBER:
                {
                    if ( _rpn.value.isDouble() == false ) {
                        _rpn.value = Json::Value(_rpn.value.asDouble());
                    }
                }
                case rpn::IT_STRING:
                case rpn::IT_OBJECT:
                case rpn::IT_ARRAY:
                case rpn::IT_BOOL:
                case rpn::IT_NULL:
                case rpn::IT_STACK:
                case rpn::IT_BOA:
                case rpn::IT_EOA:
                    _data.push(_rpn); break;
                case rpn::IT_PLUS: 
                {
                    if ( _data.size() < 2 ) {
                        err_ = original_code + ", syntax error near '+'"; return E_ASSERT;
                    }
                    auto _v1 = _data.top(); _data.pop();
                    auto _v2 = _data.top(); _data.pop();
                    Json::Value *_pv1 = (_v1.type != rpn::IT_PATH) ? &_v1.value : node_by_path_(_v1.value);
                    if ( _pv1 == NULL ) return E_ASSERT;
                    Json::Value *_pv2 = (_v2.type != rpn::IT_PATH) ? &_v2.value : node_by_path_(_v2.value);
                    if ( _pv2 == NULL ) return E_ASSERT;
                    Json::Value &_jv1 = *_pv1;
                    Json::Value &_jv2 = *_pv2;
                    // Any string object beside '+', consider as string concat string
                    if ( _v2.type == rpn::IT_STRING || (_v2.type == rpn::IT_PATH && _jv2.isString() ) ) {
                        rpn::item_t _r{rpn::IT_STRING, _jv2.asString() + _jv1.asString()};
                        _data.push(_r);
                    } else if ( _v1.type == rpn::IT_STRING || (_v1.type == rpn::IT_PATH && _jv1.isString()) ) {
                        rpn::item_t _r{rpn::IT_STRING, _jv2.asString() + _jv1.asString()};
                        _data.push(_r);
                    } else if ( 
                        // Both is number, do normal add
                        (_v1.type == rpn::IT_NUMBER || (_v1.type == rpn::IT_PATH && _jv1.isNumeric())) &&
                        (_v2.type == rpn::IT_NUMBER || (_v2.type == rpn::IT_PATH && _jv2.isNumeric()))
                    ) {
                        rpn::item_t _r{rpn::IT_NUMBER, _jv2.asDouble() + _jv1.asDouble()};
                        _data.push(_r);
                    } else {
                        err_ =  original_code + ", invalidate type around '+'";
                        return E_ASSERT;
                    }
                    break;
                }
                case rpn::IT_MINUS:
                {
                    if ( _data.size() < 2 ) {
                        err_ =  original_code + ", syntax error near '-'"; return E_ASSERT;
                    }
                    auto _v1 = _data.top(); _data.pop();
                    auto _v2 = _data.top(); _data.pop();
                    Json::Value *_pv1 = (_v1.type != rpn::IT_PATH) ? &_v1.value : node_by_path_(_v1.value);
                    if ( _pv1 == NULL ) return E_ASSERT;
                    Json::Value *_pv2 = (_v2.type != rpn::IT_PATH) ? &_v2.value : node_by_path_(_v2.value);
                    if ( _pv2 == NULL ) return E_ASSERT;
                    Json::Value &_jv1 = *_pv1;
                    Json::Value &_jv2 = *_pv2;
                    if ( 
                        (_v1.type == rpn::IT_NUMBER || (_v1.type == rpn::IT_PATH && _jv1.isNumeric())) &&
                        (_v2.type == rpn::IT_NUMBER || (_v2.type == rpn::IT_PATH && _jv2.isNumeric()))
                    ) {
                        rpn::item_t _r{rpn::IT_NUMBER, _jv2.asDouble() - _jv1.asDouble()};
                        _data.push(_r);
                    } else {
                        err_ =  original_code + ", invalidate type around '-'";
                        return E_ASSERT;
                    }
                    break;
                }
                case rpn::IT_TIMES:
                {
                    if ( _data.size() < 2 ) {
                        err_ =  original_code + ", syntax error near '*'"; return E_ASSERT;
                    }
                    auto _v1 = _data.top(); _data.pop();
                    auto _v2 = _data.top(); _data.pop();
                    Json::Value *_pv1 = (_v1.type != rpn::IT_PATH) ? &_v1.value : node_by_path_(_v1.value);
                    if ( _pv1 == NULL ) return E_ASSERT;
                    Json::Value *_pv2 = (_v2.type != rpn::IT_PATH) ? &_v2.value : node_by_path_(_v2.value);
                    if ( _pv2 == NULL ) return E_ASSERT;
                    Json::Value &_jv1 = *_pv1;
                    Json::Value &_jv2 = *_pv2;
                    if ( 
                        (_v1.type == rpn::IT_NUMBER || (_v1.type == rpn::IT_PATH && _jv1.isNumeric())) &&
                        (_v2.type == rpn::IT_NUMBER || (_v2.type == rpn::IT_PATH && _jv2.isNumeric()))
                    ) {
                        rpn::item_t _r{rpn::IT_NUMBER, _jv2.asDouble() * _jv1.asDouble()};
                        _data.push(_r);
                    } else {
                        err_ =  original_code + ", invalidate type around '*'";
                        return E_ASSERT;
                    }
                    break;
                }
                case rpn::IT_MOD:
                {
                    if ( _data.size() < 2 ) {
                        err_ =  original_code + ", syntax error near '%'"; return E_ASSERT;
                    }
                    auto _v1 = _data.top(); _data.pop();
                    auto _v2 = _data.top(); _data.pop();
                    Json::Value *_pv1 = (_v1.type != rpn::IT_PATH) ? &_v1.value : node_by_path_(_v1.value);
                    if ( _pv1 == NULL ) return E_ASSERT;
                    Json::Value *_pv2 = (_v2.type != rpn::IT_PATH) ? &_v2.value : node_by_path_(_v2.value);
                    if ( _pv2 == NULL ) return E_ASSERT;
                    Json::Value &_jv1 = *_pv1;
                    Json::Value &_jv2 = *_pv2;
                    if ( 
                        (_v1.type == rpn::IT_NUMBER || (_v1.type == rpn::IT_PATH && _jv1.isNumeric())) &&
                        (_v2.type == rpn::IT_NUMBER || (_v2.type == rpn::IT_PATH && _jv2.isNumeric()))
                    ) {
                        rpn::item_t _r{rpn::IT_NUMBER, (double)(_jv2.asInt64() % _jv1.asInt64())};
                        _data.push(_r);
                    } else {
                        err_ =  original_code + ", invalidate type around '%'";
                        return E_ASSERT;
                    }
                    break;
                }
                case rpn::IT_POW:
                {
                    if ( _data.size() < 2 ) {
                        err_ =  original_code + ", syntax error near '^'"; return E_ASSERT;
                    }
                    auto _v1 = _data.top(); _data.pop();
                    auto _v2 = _data.top(); _data.pop();
                    Json::Value *_pv1 = (_v1.type != rpn::IT_PATH) ? &_v1.value : node_by_path_(_v1.value);
                    if ( _pv1 == NULL ) return E_ASSERT;
                    Json::Value *_pv2 = (_v2.type != rpn::IT_PATH) ? &_v2.value : node_by_path_(_v2.value);
                    if ( _pv2 == NULL ) return E_ASSERT;
                    Json::Value &_jv1 = *_pv1;
                    Json::Value &_jv2 = *_pv2;
                    if ( 
                        (_v1.type == rpn::IT_NUMBER || (_v1.type == rpn::IT_PATH && _jv1.isNumeric())) &&
                        (_v2.type == rpn::IT_NUMBER || (_v2.type == rpn::IT_PATH && _jv2.isNumeric()))
                    ) {
                        rpn::item_t _r{rpn::IT_NUMBER, pow(_jv2.asDouble(), _jv1.asDouble())};
                        _data.push(_r);
                    } else {
                        err_ =  original_code + ", invalidate type around '^'";
                        return E_ASSERT;
                    }
                    break;
                }
                case rpn::IT_DIVID:
                {
                    if ( _data.size() < 2 ) {
                        err_ =  original_code + ", syntax error near '/'"; return E_ASSERT;
                    }
                    auto _v1 = _data.top(); _data.pop();
                    auto _v2 = _data.top(); _data.pop();
                    Json::Value *_pv1 = (_v1.type != rpn::IT_PATH) ? &_v1.value : node_by_path_(_v1.value);
                    if ( _pv1 == NULL ) return E_ASSERT;
                    Json::Value *_pv2 = (_v2.type != rpn::IT_PATH) ? &_v2.value : node_by_path_(_v2.value);
                    if ( _pv2 == NULL ) return E_ASSERT;
                    Json::Value &_jv1 = *_pv1;
                    Json::Value &_jv2 = *_pv2;
                    if ( 
                        (_v1.type == rpn::IT_NUMBER || (_v1.type == rpn::IT_PATH && _jv1.isNumeric())) &&
                        (_v2.type == rpn::IT_NUMBER || (_v2.type == rpn::IT_PATH && _jv2.isNumeric()))
                    ) {
                        if ( _jv1.asDouble() == 0.f ) {
                            err_ =  original_code + ", zero overflow";
                            return E_ASSERT;
                        }
                        rpn::item_t _r{rpn::IT_NUMBER, _jv2.asDouble() / _jv1.asDouble()};
                        _data.push(_r);
                    } else {
                        err_ =  original_code + ", invalidate type around '/'";
                        return E_ASSERT;
                    }
                    break;
                }
                case rpn::IT_EQUAL:
                {
                    if ( _data.size() < 2 ) {
                        err_ =  original_code + ", syntax error near '/'"; return E_ASSERT;
                    }
                    auto _v1 = _data.top(); _data.pop();
                    auto _v2 = _data.top(); _data.pop();
                    Json::Value *_pv1 = (_v1.type != rpn::IT_PATH) ? &_v1.value : node_by_path_(_v1.value);
                    if ( _pv1 == NULL ) return E_ASSERT;
                    Json::Value *_pv2 = (_v2.type != rpn::IT_PATH) ? &_v2.value : node_by_path_(_v2.value);
                    if ( _pv2 == NULL ) return E_ASSERT;
                    Json::Value &_jv1 = *_pv1;
                    Json::Value &_jv2 = *_pv2;

                    if ( _jv1.isNumeric() && _jv2.isNumeric() ) {
                        rpn::item_t _b{rpn::IT_BOOL, Json::Value(_jv2.asDouble() == _jv1.asDouble())};
                        _data.push(_b);
                    } else {
                        rpn::item_t _b{rpn::IT_BOOL, Json::Value(_jv2 == _jv1)};
                        _data.push(_b);
                    }
                    break;
                }
                case rpn::IT_LESS_THAN:
                {
                    if ( _data.size() < 2 ) {
                        err_ =  original_code + ", syntax error near '<'"; return E_ASSERT;
                    }
                    auto _v1 = _data.top(); _data.pop();
                    auto _v2 = _data.top(); _data.pop();
                    Json::Value *_pv1 = (_v1.type != rpn::IT_PATH) ? &_v1.value : node_by_path_(_v1.value);
                    if ( _pv1 == NULL ) return E_ASSERT;
                    Json::Value *_pv2 = (_v2.type != rpn::IT_PATH) ? &_v2.value : node_by_path_(_v2.value);
                    if ( _pv2 == NULL ) return E_ASSERT;
                    Json::Value &_jv1 = *_pv1;
                    Json::Value &_jv2 = *_pv2;
                    if ( 
                        (_v1.type == rpn::IT_NUMBER || (_v1.type == rpn::IT_PATH && _jv1.isNumeric())) &&
                        (_v2.type == rpn::IT_NUMBER || (_v2.type == rpn::IT_PATH && _jv2.isNumeric()))
                    ) {
                        rpn::item_t _r{rpn::IT_BOOL, _jv2.asDouble() < _jv1.asDouble()};
                        _data.push(_r);
                    } else {
                        err_ =  original_code + ", invalidate type around '<'";
                        return E_ASSERT;
                    }
                    break;
                }
                case rpn::IT_LESS_EQUAL:
                {
                    if ( _data.size() < 2 ) {
                        err_ =  original_code + ", syntax error near '<='"; return E_ASSERT;
                    }
                    auto _v1 = _data.top(); _data.pop();
                    auto _v2 = _data.top(); _data.pop();
                    Json::Value *_pv1 = (_v1.type != rpn::IT_PATH) ? &_v1.value : node_by_path_(_v1.value);
                    if ( _pv1 == NULL ) return E_ASSERT;
                    Json::Value *_pv2 = (_v2.type != rpn::IT_PATH) ? &_v2.value : node_by_path_(_v2.value);
                    if ( _pv2 == NULL ) return E_ASSERT;
                    Json::Value &_jv1 = *_pv1;
                    Json::Value &_jv2 = *_pv2;
                    if ( 
                        (_v1.type == rpn::IT_NUMBER || (_v1.type == rpn::IT_PATH && _jv1.isNumeric())) &&
                        (_v2.type == rpn::IT_NUMBER || (_v2.type == rpn::IT_PATH && _jv2.isNumeric()))
                    ) {
                        rpn::item_t _r{rpn::IT_BOOL, _jv2.asDouble() <= _jv1.asDouble()};
                        _data.push(_r);
                    } else {
                        err_ =  original_code + ", invalidate type around '<='";
                        return E_ASSERT;
                    }
                    break;

                }
                case rpn::IT_GREAT_THAN:
                {
                    if ( _data.size() < 2 ) {
                        err_ =  original_code + ", syntax error near '>'"; return E_ASSERT;
                    }
                    auto _v1 = _data.top(); _data.pop();
                    auto _v2 = _data.top(); _data.pop();
                    Json::Value *_pv1 = (_v1.type != rpn::IT_PATH) ? &_v1.value : node_by_path_(_v1.value);
                    if ( _pv1 == NULL ) return E_ASSERT;
                    Json::Value *_pv2 = (_v2.type != rpn::IT_PATH) ? &_v2.value : node_by_path_(_v2.value);
                    if ( _pv2 == NULL ) return E_ASSERT;
                    Json::Value &_jv1 = *_pv1;
                    Json::Value &_jv2 = *_pv2;
                    if ( 
                        (_v1.type == rpn::IT_NUMBER || (_v1.type == rpn::IT_PATH && _jv1.isNumeric())) &&
                        (_v2.type == rpn::IT_NUMBER || (_v2.type == rpn::IT_PATH && _jv2.isNumeric()))
                    ) {
                        rpn::item_t _r{rpn::IT_BOOL, _jv2.asDouble() > _jv1.asDouble()};
                        _data.push(_r);
                    } else {
                        err_ =  original_code + ", invalidate type around '>'";
                        return E_ASSERT;
                    }
                    break;
                }
                case rpn::IT_GREAT_EQUAL:
                {
                    if ( _data.size() < 2 ) {
                        err_ =  original_code + ", syntax error near '>='"; return E_ASSERT;
                    }
                    auto _v1 = _data.top(); _data.pop();
                    auto _v2 = _data.top(); _data.pop();
                    Json::Value *_pv1 = (_v1.type != rpn::IT_PATH) ? &_v1.value : node_by_path_(_v1.value);
                    if ( _pv1 == NULL ) return E_ASSERT;
                    Json::Value *_pv2 = (_v2.type != rpn::IT_PATH) ? &_v2.value : node_by_path_(_v2.value);
                    if ( _pv2 == NULL ) return E_ASSERT;
                    Json::Value &_jv1 = *_pv1;
                    Json::Value &_jv2 = *_pv2;
                    if ( 
                        (_v1.type == rpn::IT_NUMBER || (_v1.type == rpn::IT_PATH && _jv1.isNumeric())) &&
                        (_v2.type == rpn::IT_NUMBER || (_v2.type == rpn::IT_PATH && _jv2.isNumeric()))
                    ) {
                        rpn::item_t _r{rpn::IT_BOOL, _jv2.asDouble() >= _jv1.asDouble()};
                        _data.push(_r);
                    } else {
                        err_ =  original_code + ", invalidate type around '>='";
                        return E_ASSERT;
                    }
                    break;
                }
                case rpn::IT_NOT_EQUAL:
                {
                    if ( _data.size() < 2 ) {
                        err_ =  original_code + ", syntax error near '/'"; return E_ASSERT;
                    }
                    auto _v1 = _data.top(); _data.pop();
                    auto _v2 = _data.top(); _data.pop();
                    Json::Value *_pv1 = (_v1.type != rpn::IT_PATH) ? &_v1.value : node_by_path_(_v1.value);
                    if ( _pv1 == NULL ) return E_ASSERT;
                    Json::Value *_pv2 = (_v2.type != rpn::IT_PATH) ? &_v2.value : node_by_path_(_v2.value);
                    if ( _pv2 == NULL ) return E_ASSERT;
                    Json::Value &_jv1 = *_pv1;
                    Json::Value &_jv2 = *_pv2;

                    rpn::item_t _b{rpn::IT_BOOL, Json::Value(_jv2 != _jv1)};
                    _data.push(_b);
                    break;
                }
                case rpn::IT_NOT:
                {
                    if ( _data.size() < 1 ) {
                        err_ = original_code + ", invalidate `!`";
                        return E_ASSERT;
                    }
                    auto _v1 = _data.top(); _data.pop();
                    Json::Value *_pv1 = (_v1.type != rpn::IT_PATH) ? &_v1.value : node_by_path_(_v1.value);
                    if ( _pv1 == NULL ) return E_ASSERT;
                    Json::Value &_jv1 = *_pv1;
                    if ( _jv1.isBool() == false ) {
                        err_ = original_code + ", cannot process `!` on non-bool object";
                        return E_ASSERT;
                    }
                    // Create a temp value
                    Json::Value _newValue = !_jv1.asBool();
                    rpn::item_t _b{rpn::IT_BOOL, _newValue};
                    _data.push(_b);
                    break;
                }
                case rpn::IT_SET:
                {
                    if ( _data.size() < 2 ) {
                        err_ =  original_code + ", syntax error near '='"; return E_ASSERT;
                    }
                    auto _v1 = _data.top(); _data.pop();
                    auto _v2 = _data.top(); _data.pop();
                    if ( _v2.type != rpn::IT_PATH ) {
                        err_ =  original_code + ", left side must be a path";
                        return E_ASSERT;
                    }
                    Json::Value *_pv1 = (_v1.type != rpn::IT_PATH) ? &_v1.value : node_by_path_(_v1.value);
                    if ( _pv1 == NULL ) return E_ASSERT;
                    Json::Value &_jv1 = *_pv1;
                    Json::Value* _pv2 = node_by_path_(_v2.value);
                    if ( _pv2 == NULL ) return E_ASSERT;
                    if ( _pv2 == &assert_ ) {
                        if ( !_jv1.isBool() ) {
                            err_ =  original_code + ", `assert` must set a boolean value";
                            return E_ASSERT;
                        }
                        if ( _jv1.asBool() == false ) {
                            err_ = original_code;
                            return E_ASSERT;
                        }
                    } else if ( _pv2 == &return_ ) {
                        *_pv2 = _jv1;
                        return E_RETURN;
                    }
                    Json::Value &_jv2 = *_pv2;

                    // Do copy
                    _jv2 = _jv1;
                    break;                    
                }
                case rpn::IT_NODE:
                {
                    if ( _data.size() < 1 ) {
                        err_ =  original_code + ", invalidate path begin with '.'";
                        return E_ASSERT;
                    }
                    if ( _data.top().type == rpn::IT_PATH ) {
                        auto _pitem = _data.top();
                        _data.pop();
                        Json::Value* _pv = node_by_path_(_pitem.value);
                        if ( _pv == NULL ) return E_ASSERT; // Path not found
                        rpn::item_t _n{rpn::IT_STRING, *_pv};
                        if ( _pv->isObject() ) _n.type = rpn::IT_OBJECT;
                        else if ( _pv->isNumeric() ) _n.type = rpn::IT_NUMBER;
                        else if ( _pv->isArray() ) _n.type = rpn::IT_ARRAY;
                        else if ( _pv->isBool() ) _n.type = rpn::IT_BOOL;
                        _data.push(_n);
                    }
                    if ( _data.top().type != rpn::IT_STRING && 
                        _data.top().type != rpn::IT_NUMBER &&
                        _data.top().type != rpn::IT_EOA 
                    ) {
                        err_ =  original_code + ", invalidate path";
                        return E_ASSERT;
                    }

                    if ( _data.top().type == rpn::IT_EOA ) {
                        _node_stack.push(Json::Value("_"));
                        break;
                    }

                    Json::Value _node = _data.top().value;
                    _data.pop();

                    _node_stack.push(_node);

                    // Path unfinished
                    if ( _node.asString() != "$" ) {
                        break;
                    }

                    Json::Value _node_path(Json::arrayValue);
                    while ( _node_stack.size() > 0 ) {
                        _node = _node_stack.top();
                        _node_stack.pop();
                        _node_path.append(_node);
                    }
                    rpn::item_t _node_ref{rpn::IT_PATH, _node_path};
                    _data.push(_node_ref);
                    break;
                }
                case rpn::IT_EXEC: 
                {
                    if ( _data.size() == 0 ) {
                        err_ =  original_code + ", missing run path for function";
                        return E_ASSERT;
                    }
                    if ( _data.top().type != rpn::IT_EOA ) {
                        err_ = original_code + ", syntax error of function call <" + 
                            _rpn.value.asString() + ">";
                        return E_ASSERT;
                    }
                    _data.pop();    // Ignore the EOA
                    std::list< rpn::item_t > _args;
                    while ( _data.size() > 1 && _data.top().type != rpn::IT_BOA ) {
                        _args.push_front(_data.top());
                        _data.pop();
                    }
                    if ( _data.size() <= 1 ) {
                        err_ =  original_code + ", logic error, too many arguments";
                        return E_ASSERT;
                    }
                    // Pop BOA
                    _data.pop();
                    // Next should be the node who invoke the function
                    if ( _data.top().type != rpn::IT_PATH ) {
                        err_ =  original_code + ", logic error, missing `this`";
                        return E_ASSERT;
                    }

                    // _args.push_front(_data.top());
                    Json::Value *_pthis = node_by_path_(_data.top().value);
                    if ( _pthis == NULL ) return E_ASSERT;
                    // Tell to run
                    auto _pmodule = module_manager::search_module(
                        _rpn.value.asString(), 
                        *_pthis, 
                        (_pthis == &root_)
                    );
                    if ( _pmodule == nullptr ) {
                        err_ = original_code + ", method `" + _rpn.value.asString() + "` not found";
                        return E_ASSERT;
                    }
                    if ( _pmodule->exec == nullptr ) {
                        err_ = original_code + ", method `" + _rpn.value.asString() + "` with empty exec";
                        return E_ASSERT;
                    }
                    // Now push this
                    this_stack_.push(_data.top());
                    _data.pop();
                    // Invoke the method
                    auto _ret = _pmodule->exec(*this, this_stack_.top(), _args);
                    if ( _ret.type == rpn::IT_ERROR ) {
                        err_ = original_code + ", " + _ret.value.asString();
                        return E_ASSERT;
                    }
                    if ( _node_stack.size() > 0 ) {
                        if ( _ret.type == rpn::IT_PATH ) {
                            Json::Value* _pret = node_by_path_(_ret.value);
                            if ( _pret == NULL ) {
                                err_ = original_code + ", invalidate return object";
                                return E_ASSERT;
                            }
                            temp_obj_ = *_pret;
                        } else {
                            temp_obj_ = _ret.value;
                        }
                        Json::Value _node_path(Json::arrayValue);
                        while ( _node_stack.size() > 0 ) {
                            _node_path.append(_node_stack.top());
                            _node_stack.pop();
                        }
                        rpn::item_t _node_ref{rpn::IT_PATH, _node_path};
                        _data.push(_node_ref);
                    } else {
                        _data.push(_ret);
                    }
                    // pop this stack
                    this_stack_.pop();
                    break;
                }
                default: break;
            };
        }
        if ( _data.size() != 0 ) {
            err_ = "unfinished code running";
            return E_ASSERT;
        }
        return E_OK;
    }
    // Run Code Group
    E_STATE costage::code_group_run_( const std::string& group_name ) {
        if ( code_map_.find(group_name) == code_map_.end() ) {
            err_ = "`" + group_name + "` BAD_ACCESS";
            return E_ASSERT;
        }
        ptr_group_type _pgroup = code_map_[group_name];

        E_STATE _r = E_OK;
        rpn::item_t _l{rpn::IT_OBJECT, Json::Value(Json::objectValue)};
        local_stack_.push(_l);
        for ( auto& _c : (*_pgroup) ) {
            if ( !return_.isNull() ) {
                return_ = Json::Value(Json::nullValue);
            }
            _r = code_line_run_(_c.first, _c.second);
            if ( _r == E_OK ) continue;
            break;
        }
        local_stack_.pop();
        return _r;
    }

    // Exec the last parsed RPN object
    E_STATE costage::code_run() {
        return code_group_run_(entry_group_);
    }

    // Clear stack info and start a new environment
    void costage::code_clear() {
        while ( this_stack_.size() > 0 ) { this_stack_.pop(); }
        while ( last_stack_.size() > 0 ) { last_stack_.pop(); }
        entry_group_ = "";
        code_map_.clear();
        while ( group_stack_.size() > 0 ) { group_stack_.pop(); }
        while ( parser_stack_.size() > 0 ) { parser_stack_.pop(); }
        current_group_ = "";
        group_ = nullptr;
        exec_ = nullptr;
        parser_ = nullptr;
    }
    // Public Methods
    
    // Search for a node
    Json::Value* costage::get_node( const Json::Value& path_value ) { return node_by_path_(path_value); }
    // This/Last Setting
    bool costage::push_this( rpn::item_t&& this_path ) { 
        if ( this_path.type != rpn::IT_PATH ) {
            err_ = "push invalidate this path";
            return false;
        }
        this_stack_.push(this_path);
        return true;
    }
    void costage::pop_this() {
        if ( this_stack_.size() > 0 ) this_stack_.pop();
    }
    bool costage::push_last( rpn::item_t&& last_path ) {
        if ( last_path.type != rpn::IT_PATH ) {
            err_ = "push invalidate last path";
            return false;
        }
        last_stack_.push(last_path);
        return true;
    }
    void costage::pop_last() {
        if ( last_stack_.size() > 0 ) last_stack_.pop();
    }
    // Invoke a sub code group
    E_STATE costage::invoke_group( const std::string& group_name ) {
        return code_group_run_(group_name);
    }
}

// Push Chen
//