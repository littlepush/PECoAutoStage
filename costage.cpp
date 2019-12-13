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
    // Create a new sub stack and return the name
    // Will create both parser item and code part
    std::string costage::stack_create_() {
        ptr_stack_type _pstack = std::make_shared< rpn::stack_type >();
        std::string _stack_name = utils::ptr_str(_pstack.get());
        code_map_[_stack_name] = _pstack;

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
            result_ = _ret;
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
            rpn::item_t _n{rpn::IT_NODE, Json::Value(".")};
            parser_->item.push(_n);
            return true;
        }

        // If no op
        if ( parser_->op.size() == 0 ) {
            parser_->op.push(op); return true;
        }

        // Meet a ')', need to pop until '('
        if ( op.type == rpn::IT_RIGHT_PARENTHESES ) {
            while ( 
                parser_->op.size() != 0 && 
                parser_->op.top().type != rpn::IT_LEFT_PARENTHESES
            ) {
                parser_->item.push(parser_->op.top());
                parser_->op.pop();
            }
            if ( parser_->op.size() == 0 ) {
                err_ = "unmatched ')' at index: " + std::to_string(index);
                return false;
            }
            // Remove the '('
            parser_->op.pop();
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
    costage::costage() : exec_(nullptr), parser_(nullptr), result(result_) {
        result_.type = rpn::IT_VOID;
    }

    // Get the error message
    const std::string& costage::err_string() const {
        return err_;
    }

    // Parse a code line
    I_STATE costage::code_parser( std::string&& code ) {

        #define __GET_C_N   (i == code_.size() - 1) ? '\0' : code_[i + 1]
        #define __GET_C     code_[i];
        #define __GET_C_P   (i == 0) ? '\0' : code_[i - 1]

        code_ = code;   // Do Coyp
        utils::trim(code_);
        utils::code_filter_comment(code_);
        if ( code_.size() == 0 ) return I_UNFINISHED;

        stack_create_();

        std::map< int, bool >   _nodelv;
        int                     _plv = 0;
        char                    _c_p = '\0';
        for ( size_t i = 0; i < code_.size(); ++i ) {
            char c = __GET_C;
            char c_n = __GET_C_N;

            if ( std::isspace(c) ) continue;    // ignore unused space

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

                // Test if is true of false
                if ( _c_p != '.' && (c == 't' || c == 'f') ) {
                    char _s[10];
                    int _r = sscanf(code_.c_str() + i, "%s", _s);
                    if ( _r != 1 ) {
                        err_ = "Invalidate word at index: " + std::to_string(i);
                        return I_SYNTAX;
                    }
                    std::string _bs(_s);
                    if ( _bs == "true" ) {
                        rpn::item_t _b{rpn::IT_BOOL, Json::Value(true)};
                        parser_->item.push(_b);
                    } else if ( _bs == "false" ) {
                        rpn::item_t _b{rpn::IT_BOOL, Json::Value(false)};
                        parser_->item.push(_b);
                    } else {
                        err_ = "invalidate word at index: " + std::to_string(i);
                        return I_SYNTAX;
                    }
                    i += _r;
                    break;
                }

                // Try to test if this is a number
                char *_pend = NULL;
                double _n = std::strtod(code_.c_str() + i, &_pend);
                if ( _pend != (code_.c_str() + i) ) {
                    if ( _n == HUGE_VAL || errno == ERANGE ) {
                        // Failed to get the number
                        err_ = "Invalidate number at index: " + std::to_string(i);
                        return I_SYNTAX;
                    }

                    // This is a number
                    rpn::item_t _i{rpn::IT_NUMBER, Json::Value(_n)};
                    parser_->item.push(_i);
                    size_t _l = _pend - (code_.c_str() + i);
                    i += (_l - 1);
                    break;
                }

                // A new node
                if ( c == '.' ) {
                    rpn::item_t _i{rpn::IT_NODE, Json::Value(".")};
                    if ( !operator_parser_(i, _i) ) return I_SYNTAX;
                    if ( c_n == '(' ) _nodelv[_plv] = true;
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
                if ( c == '(' ) {
                    rpn::item_t _i{rpn::IT_LEFT_PARENTHESES, Json::Value("(")};
                    if ( !operator_parser_(i, _i) ) return I_SYNTAX;
                    _plv += 1;
                    break;
                }
                if ( c == ')' ) {
                    _plv -= 1;
                    rpn::item_t _i{rpn::IT_RIGHT_PARENTHESES, Json::Value(")")};
                    if ( !operator_parser_(i, _i) ) return I_SYNTAX;
                    // Check if current () code block is a path node
                    if ( _nodelv.find(_plv) != _nodelv.end() ) {
                        _nodelv.erase(_plv);
                        rpn::item_t _j{rpn::IT_NODE, Json::Value(".")};
                        if ( !operator_parser_(i, _j) ) return I_SYNTAX;
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
                    break;
                }
                // TODO: When add EXEC? or JUMP?
                // Create a sub stack when find '{' in next line
                if ( c == '{' ) {
                    if ( c_n != '\0' ) {
                        err_ = "invalidate '{', following code must begin in a new line";
                        return I_SYNTAX;
                    }
                    return I_UNFINISHED;
                }
                // Until we meet matched '}', all status should be I_UNFINISHED
                if ( c == '}' ) {
                    if ( parser_queue_.size() == 1 ) {
                        err_ = "invalidate '}', missing '{'";
                        return I_SYNTAX;
                    }
                    std::string _substack_name = parser_->name;
                    parser_queue_.pop();
                    parser_ = parser_queue_.top();
                    rpn::item_t _s{rpn::IT_STACK, Json::Value(_substack_name)};
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
            _c_p = code_[i];
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
        ptr_stack_type _code = code_map_[parser_->name];
        while ( parser_->item.size() != 0 ) {
            _code->push(parser_->item.top());
            parser_->item.pop();
        }

        parser_queue_.pop();
        if ( parser_queue_.size() != 0 ) {
            parser_ = parser_queue_.top();
        }
        return (parser_queue_.size() == 0) ? I_OK : I_UNFINISHED;
    }
    // Exec the last parsed RPN object
    E_STATE costage::code_run() {
        // Jump to Entry point
        stack_jump_(parser_->name);

        std::stack< Json::Value >   _node_stack;

        while ( exec_->exec.size() != 0 ) {
            rpn::item_t _rpn = exec_->exec.top();
            exec_->exec.pop();

            switch (_rpn.type) {
                case rpn::IT_NUMBER:
                case rpn::IT_STRING:
                case rpn::IT_BOOL:
                case rpn::IT_STACK:
                case rpn::IT_BOA:
                    exec_->data.push(_rpn); break;
                case rpn::IT_PLUS: 
                {
                    if ( exec_->data.size() < 2 ) {
                        err_ = "syntax error near '+'"; return E_ASSERT;
                    }
                    auto _v1 = exec_->data.top(); exec_->data.pop();
                    auto _v2 = exec_->data.top(); exec_->data.pop();
                    Json::Value *_pv1 = (_v1.type != rpn::IT_PATH) ? &_v1.value : node_by_path_(_v1.value);
                    if ( _pv1 == NULL ) return E_ASSERT;
                    Json::Value *_pv2 = (_v2.type != rpn::IT_PATH) ? &_v2.value : node_by_path_(_v2.value);
                    if ( _pv2 == NULL ) return E_ASSERT;
                    Json::Value &_jv1 = *_pv1;
                    Json::Value &_jv2 = *_pv2;
                    // Any string object beside '+', consider as string concat string
                    if ( _v2.type == rpn::IT_STRING || (_v2.type == rpn::IT_PATH && _jv2.isString() ) ) {
                        rpn::item_t _r{rpn::IT_STRING, _jv2.asString() + _jv1.asString()};
                        exec_->data.push(_r);
                    } else if ( _v1.type == rpn::IT_STRING || (_v1.type == rpn::IT_PATH && _jv1.isString()) ) {
                        rpn::item_t _r{rpn::IT_STRING, _jv2.asString() + _jv1.asString()};
                        exec_->data.push(_r);
                    } else if ( 
                        // Both is number, do normal add
                        (_v1.type == rpn::IT_NUMBER || (_v1.type == rpn::IT_PATH && _jv1.isNumeric())) &&
                        (_v2.type == rpn::IT_NUMBER || (_v2.type == rpn::IT_PATH && _jv2.isNumeric()))
                    ) {
                        rpn::item_t _r{rpn::IT_NUMBER, _jv2.asDouble() + _jv1.asDouble()};
                        exec_->data.push(_r);
                    } else {
                        err_ = "invalidate type around '+'";
                        return E_ASSERT;
                    }
                    break;
                }
                case rpn::IT_MINUS:
                {
                    if ( exec_->data.size() < 2 ) {
                        err_ = "syntax error near '-'"; return E_ASSERT;
                    }
                    auto _v1 = exec_->data.top(); exec_->data.pop();
                    auto _v2 = exec_->data.top(); exec_->data.pop();
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
                        exec_->data.push(_r);
                    } else {
                        err_ = "invalidate type around '-'";
                        return E_ASSERT;
                    }
                    break;
                }
                case rpn::IT_TIMES:
                {
                    if ( exec_->data.size() < 2 ) {
                        err_ = "syntax error near '*'"; return E_ASSERT;
                    }
                    auto _v1 = exec_->data.top(); exec_->data.pop();
                    auto _v2 = exec_->data.top(); exec_->data.pop();
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
                        exec_->data.push(_r);
                    } else {
                        err_ = "invalidate type around '*'";
                        return E_ASSERT;
                    }
                    break;
                }
                case rpn::IT_DIVID:
                {
                    if ( exec_->data.size() < 2 ) {
                        err_ = "syntax error near '/'"; return E_ASSERT;
                    }
                    auto _v1 = exec_->data.top(); exec_->data.pop();
                    auto _v2 = exec_->data.top(); exec_->data.pop();
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
                            err_ = "zero overflow";
                            return E_ASSERT;
                        }
                        rpn::item_t _r{rpn::IT_NUMBER, _jv2.asDouble() / _jv1.asDouble()};
                        exec_->data.push(_r);
                    } else {
                        err_ = "invalidate type around '/'";
                        return E_ASSERT;
                    }
                    break;
                }
                case rpn::IT_EQUAL:
                {
                    if ( exec_->data.size() < 2 ) {
                        err_ = "syntax error near '/'"; return E_ASSERT;
                    }
                    auto _v1 = exec_->data.top(); exec_->data.pop();
                    auto _v2 = exec_->data.top(); exec_->data.pop();
                    Json::Value *_pv1 = (_v1.type != rpn::IT_PATH) ? &_v1.value : node_by_path_(_v1.value);
                    if ( _pv1 == NULL ) return E_ASSERT;
                    Json::Value *_pv2 = (_v2.type != rpn::IT_PATH) ? &_v2.value : node_by_path_(_v2.value);
                    if ( _pv2 == NULL ) return E_ASSERT;
                    Json::Value &_jv1 = *_pv1;
                    Json::Value &_jv2 = *_pv2;

                    rpn::item_t _b{rpn::IT_BOOL, Json::Value(_jv2 == _jv1)};
                    exec_->data.push(_b);
                    break;
                }
                case rpn::IT_LESS_THAN:
                {
                    if ( exec_->data.size() < 2 ) {
                        err_ = "syntax error near '<'"; return E_ASSERT;
                    }
                    auto _v1 = exec_->data.top(); exec_->data.pop();
                    auto _v2 = exec_->data.top(); exec_->data.pop();
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
                        exec_->data.push(_r);
                    } else {
                        err_ = "invalidate type around '<'";
                        return E_ASSERT;
                    }
                    break;
                }
                case rpn::IT_LESS_EQUAL:
                {
                    if ( exec_->data.size() < 2 ) {
                        err_ = "syntax error near '<='"; return E_ASSERT;
                    }
                    auto _v1 = exec_->data.top(); exec_->data.pop();
                    auto _v2 = exec_->data.top(); exec_->data.pop();
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
                        exec_->data.push(_r);
                    } else {
                        err_ = "invalidate type around '<='";
                        return E_ASSERT;
                    }
                    break;

                }
                case rpn::IT_GREAT_THAN:
                {
                    if ( exec_->data.size() < 2 ) {
                        err_ = "syntax error near '>'"; return E_ASSERT;
                    }
                    auto _v1 = exec_->data.top(); exec_->data.pop();
                    auto _v2 = exec_->data.top(); exec_->data.pop();
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
                        exec_->data.push(_r);
                    } else {
                        err_ = "invalidate type around '>'";
                        return E_ASSERT;
                    }
                    break;
                }
                case rpn::IT_GREAT_EQUAL:
                {
                    if ( exec_->data.size() < 2 ) {
                        err_ = "syntax error near '>='"; return E_ASSERT;
                    }
                    auto _v1 = exec_->data.top(); exec_->data.pop();
                    auto _v2 = exec_->data.top(); exec_->data.pop();
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
                        exec_->data.push(_r);
                    } else {
                        err_ = "invalidate type around '>='";
                        return E_ASSERT;
                    }
                    break;
                }
                case rpn::IT_SET:
                {
                    if ( exec_->data.size() < 2 ) {
                        err_ = "syntax error near '='"; return E_ASSERT;
                    }
                    auto _v1 = exec_->data.top(); exec_->data.pop();
                    auto _v2 = exec_->data.top(); exec_->data.pop();
                    if ( _v2.type != rpn::IT_PATH ) {
                        err_ = "left side must be a path";
                        return E_ASSERT;
                    }
                    Json::Value *_pv1 = (_v1.type != rpn::IT_PATH) ? &_v1.value : node_by_path_(_v1.value);
                    if ( _pv1 == NULL ) return E_ASSERT;
                    Json::Value &_jv1 = *_pv1;
                    Json::Value* _pv2 = node_by_path_(_v2.value);
                    if ( _pv2 == NULL ) return E_ASSERT;
                    Json::Value &_jv2 = *_pv2;

                    // Do copy
                    _jv2 = _jv1;
                    break;                    
                }
                case rpn::IT_NODE:
                {
                    if ( exec_->data.size() < 1 ) {
                        err_ = "invalidate path begin with '.'";
                        return E_ASSERT;
                    }
                    if ( exec_->data.top().type == rpn::IT_PATH ) {
                        auto _pitem = exec_->data.top();
                        exec_->data.pop();
                        Json::Value* _pv = node_by_path_(_pitem.value);
                        if ( _pv == NULL ) return E_ASSERT; // Path not found
                        rpn::item_t _n{rpn::IT_STRING, *_pv};
                        if ( _pv->isNumeric() ) _n.type = rpn::IT_NUMBER;
                        exec_->data.push(_n);
                    }
                    if ( exec_->data.top().type != rpn::IT_STRING && 
                        exec_->data.top().type != rpn::IT_NUMBER ) {
                        err_ = "invalidate path";
                        return E_ASSERT;
                    }

                    Json::Value _node = exec_->data.top().value;
                    exec_->data.pop();

                    // Path unfinished
                    if ( _node.asString() != "$" ) {
                        _node_stack.push(_node);
                        break;
                    }

                    Json::Value _node_path(Json::arrayValue);
                    while ( _node_stack.size() > 0 ) {
                        _node = _node_stack.top();
                        _node_stack.pop();
                        _node_path.append(_node);
                    }
                    rpn::item_t _node_ref{rpn::IT_PATH, _node_path};
                    exec_->data.push(_node_ref);
                    break;
                }
                case rpn::IT_EXEC: 
                {
                    if ( exec_->data.size() == 0 ) {
                        err_ = "missing run path for function";
                        return E_ASSERT;
                    }
                    std::list< rpn::item_t > _args;
                    while ( exec_->data.size() > 1 && exec_->data.top().type != rpn::IT_BOA ) {
                        _args.push_front(exec_->data.top());
                        exec_->data.pop();
                    }
                    if ( exec_->data.size() <= 1 ) {
                        err_ = "logic error, too many arguments";
                        return E_ASSERT;
                    }
                    // Pop BOA
                    exec_->data.pop();
                    // Next should be the node who invoke the function
                    if ( exec_->data.top().type != rpn::IT_PATH ) {
                        err_ = "logic error, missing `this`";
                        return E_ASSERT;
                    }
                    _args.push_front(exec_->data.top());
                    exec_->data.pop();

                    // Tell to run
                    ON_DEBUG(
                        std::cout << "run " << _rpn.value.asString() << 
                            " with " << _args.size() << " arguments" << std::endl;
                        rpn::item_t _r{rpn::IT_NUMBER, Json::Value(1)};
                        exec_->data.push(_r);
                    )
                    // To do: Invoke the function
                    break;
                }
                default: break;
            };
        }
        stack_return_();
        ON_DEBUG(
            std::cout << root_ << std::endl;
        )
        return E_OK;
    }
}

// Push Chen
//