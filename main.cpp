/*
    main.cpp
    2019-12-10
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

#include <peutils.h>
using namespace pe;

#include <cotask.h>
#include <conet.h>
using namespace pe::co;

#include "costage.h"
using namespace coas;

int g_return = 0;

    bool operator_parser_( size_t index, RPNItem& op ) {
        if ( op.type == RPN_ITEM_TYPE::RPN_RIGHT_BRACKETS ) {
            while ( op_stack_.size() != 0 && op_stack_.top().type != RPN_ITEM_TYPE::RPN_LEFT_BRACKETS ) {
                item_stack_.push(op_stack_.top());
                op_stack_.pop();
            }
            if ( op_stack_.size() == 0 ) {
                err_msg_ = "unmatched ']' at index: " + std::to_string(index);
                return false;
            }
            RPNItem _i{RPN_ITEM_TYPE::RPN_INDEX, Json::Value(1)};
            item_stack_.push(_i);
            return true;
        }
        if ( op_stack_.size() == 0 ) {
            op_stack_.push(op); return true;
        }
        if ( op.type == RPN_ITEM_TYPE::RPN_RIGHT_PARENTHESES ) {
            while ( op_stack_.size() != 0 && op_stack_.top().type != RPN_ITEM_TYPE::RPN_LEFT_PARENTHESES ) {
                item_stack_.push(op_stack_.top());
                op_stack_.pop();
            }
            if ( op_stack_.size() == 0 ) {
                err_msg_ = "unmatched ')' at index: " + std::to_string(index);
                return false;
            }
            op_stack_.pop();
            if ( op_stack_.size() > 0 ) {
                if ( op_stack_.top().type == RPN_ITEM_TYPE::RPN_NODE ) {
                    item_stack_.push(op_stack_.top());
                    op_stack_.pop();
                }
            }
            return true;
        }
        if ( op_stack_.top().type == RPN_ITEM_TYPE::RPN_LEFT_PARENTHESES ) {
            op_stack_.push(op); return true;
        }
        if ( op_stack_.top().type < op.type ) {
            op_stack_.push(op); return true;
        }
        while (
            op_stack_.size() > 0 && 
            op_stack_.top().type > op.type && 
            op_stack_.top().type != RPN_ITEM_TYPE::RPN_LEFT_PARENTHESES 
        ) {
            item_stack_.push(op_stack_.top());
            op_stack_.pop();
        }
        op_stack_.push(op);
        return true;
    }

public: 

    // Result Reference
    const bool&         result;

    costage() : result_( true ), result(result_) { }

    const std::string& last_error_msg() const { return err_msg_; }

    CIN_STATE code_parser( std::string&& code ) {
        #define __GET_C_N   (i == pending_code_.size() - 1) ? '\0' : pending_code_[i + 1]
        pending_code_ += code;
        std::map< int, bool >   _nodelv;
        int                     _plv = 0;

        for ( size_t i = 0; i < pending_code_.size(); ++i ) {
            char c = pending_code_[i];
            char c_n = __GET_C_N;
            if ( std::isspace(c) ) continue;    // ignore unused space

            if ( c == '+' ) {
                RPNItem _i{RPN_ITEM_TYPE::RPN_PLUS, Json::Value(c)};
                if ( !operator_parser_(i, _i) ) 
                    return CIN_STATE::ERROR;
                continue;
            }

            if ( c == '-' ) {
                RPNItem _i{RPN_ITEM_TYPE::RPN_MINUS, Json::Value(c)};
                if ( !operator_parser_(i, _i) ) 
                    return CIN_STATE::ERROR;
                continue;
            }

            // Try to test if this is a number
            char *_pend = NULL;
            double _n = std::strtod(pending_code_.c_str() + i, &_pend);
            if ( _pend != (pending_code_.c_str() + i) ) {
                if ( _n == HUGE_VAL || errno == ERANGE ) {
                    // Failed to get the number
                    err_msg_ = "Invalidate number at index: " + std::to_string(i);
                    return CIN_STATE::ERROR;
                }

                // This is a number
                RPNItem _item{RPN_ITEM_TYPE::RPN_NUMBER, Json::Value(_n)};
                item_stack_.push(_item);
                size_t _l = _pend - (pending_code_.c_str() + i);
                i += (_l - 1);
                continue;
            }

            if ( c == '.' ) {
                RPNItem _item{RPN_ITEM_TYPE::RPN_NODE, Json::Value(".")};
                if ( !operator_parser_(i, _item) )
                    return CIN_STATE::ERROR;
                if ( c_n == '(' ) {
                    _nodelv[_plv] = true;
                }
                continue;
            }
            if ( c == '$' ) {
                if ( c_n == '.' ) { 
                    // Root Node
                    RPNItem _item{RPN_ITEM_TYPE::RPN_STRING, Json::Value("$")};
                    item_stack_.push(_item);
                } else {
                    err_msg_ = "Invalidate '$' at index: " + std::to_string(i);
                    return CIN_STATE::ERROR;
                }
                continue;
            }
            if ( c == '\"' ) {
                // Begin of a string
                std::string _s;
                size_t _bos = i;
                while ( c_n != '\"' ) {
                    // End of code, but still not finish reading
                    if ( c_n == '\0' ) {
                        pending_code_.erase(0, _bos);
                        return CIN_STATE::UNFINISHED;
                    }
                    _s += c_n;
                    ++i;
                    c_n = __GET_C_N;
                }
                ++i;    // Skip '\"'
                RPNItem _item{RPN_ITEM_TYPE::RPN_STRING, Json::Value(_s)};
                item_stack_.push(_item);
                continue;
            }
            if ( c == '*' ) {
                RPNItem _i{RPN_ITEM_TYPE::RPN_TIMES, Json::Value(c)};
                if ( !operator_parser_(i, _i) )
                    return CIN_STATE::ERROR;
                continue;
            }
            if ( c == '/' ) {
                RPNItem _i{RPN_ITEM_TYPE::RPN_DIVID, Json::Value(c)};
                if ( !operator_parser_(i, _i) )
                    return CIN_STATE::ERROR;
                continue;
            }
            if ( c == '>' ) {
                auto _t = (c_n == '=') ? RPN_ITEM_TYPE::RPN_GREAT_EQUAL : RPN_ITEM_TYPE::RPN_GREAT_THAN;
                if ( _t == RPN_ITEM_TYPE::RPN_GREAT_EQUAL ) ++i;  // skip another '='
                RPNItem _i{_t, Json::Value(c)};
                if ( !operator_parser_(i, _i) )
                    return CIN_STATE::ERROR;
                continue;
            }
            if ( c == '<' ) {
                auto _t = (c_n == '=') ? RPN_ITEM_TYPE::RPN_LESS_EQUAL : RPN_ITEM_TYPE::RPN_LESS_THAN;
                if ( _t == RPN_ITEM_TYPE::RPN_LESS_EQUAL ) ++i;  // skip another '='
                RPNItem _i{_t, Json::Value(c)};
                if ( !operator_parser_(i, _i) )
                    return CIN_STATE::ERROR;
                continue;
            }
            if ( c == '=' ) {
                auto _t = (c_n == '=') ? RPN_ITEM_TYPE::RPN_EQUAL : RPN_ITEM_TYPE::RPN_SET;
                if ( _t == RPN_ITEM_TYPE::RPN_EQUAL ) ++i;  // skip another '='
                RPNItem _i{_t, Json::Value(c)};
                if ( !operator_parser_(i, _i) )
                    return CIN_STATE::ERROR;
                continue;
            }
            if ( c == '(' ) {
                RPNItem _i{RPN_ITEM_TYPE::RPN_LEFT_PARENTHESES, Json::Value(c)};
                if ( !operator_parser_(i, _i) )
                    return CIN_STATE::ERROR;
                _plv += 1;
                continue;
            }
            if ( c == ')' ) {
                _plv -= 1;
                RPNItem _i{RPN_ITEM_TYPE::RPN_RIGHT_PARENTHESES, Json::Value(c)};
                if ( !operator_parser_(i, _i) )
                    return CIN_STATE::ERROR;
                if ( _nodelv.find(_plv) != _nodelv.end() ) {
                    _nodelv.erase(_plv);
                    RPNItem _n{RPN_ITEM_TYPE::RPN_NODE, Json::Value(".")};
                    if ( !operator_parser_(i, _n) ) return CIN_STATE::ERROR;
                }
                continue;
            }
            if ( c == '[' ) {
                RPNItem _i{RPN_ITEM_TYPE::RPN_LEFT_BRACKETS, Json::Value(c)};
                if ( !operator_parser_(i, _i) )
                    return CIN_STATE::ERROR;
                continue;
            }
            if ( c == ']' ) {
                RPNItem _i{RPN_ITEM_TYPE::RPN_RIGHT_BRACKETS, Json::Value(c)};
                if ( !operator_parser_(i, _i) )
                    return CIN_STATE::ERROR;
                continue;
            }
            if ( c == '{' ) {
                RPNItem _i{RPN_ITEM_TYPE::RPN_LEFT_BRACES, Json::Value(c)};
                if ( !operator_parser_(i, _i) ) 
                    return CIN_STATE::ERROR;
                continue;
            }
            if ( c == '}' ) {
                RPNItem _i{RPN_ITEM_TYPE::RPN_RIGHT_BRACES, Json::Value(c)};
                if ( !operator_parser_(i, _i) )
                    return CIN_STATE::ERROR;
                continue;
            }
            // only left case is a normal node name
            if ( !std::isalpha(c) && c != '_' ) {
                err_msg_ = std::string("Invalidate '") + std::string((char)c, 1) + 
                    "' at index: " + std::to_string(i);
                return CIN_STATE::ERROR;
            }
            std::string _s(1, c);
            while ( std::isalpha(c_n) || std::isdigit(c_n) || c_n == '_' ) {
                _s += c_n;
                ++i;
                c_n = __GET_C_N;
            }
            RPNItem _item{RPN_ITEM_TYPE::RPN_STRING, Json::Value(_s)};
            item_stack_.push(_item);
            if ( c_n != '.' ) {
                RPNItem _n{RPN_ITEM_TYPE::RPN_NODE, Json::Value(".")};
                if ( !operator_parser_(i, _n) ) return CIN_STATE::ERROR;
            }
            if ( c_n == '(' ) {
                RPNItem _n{RPN_ITEM_TYPE::RPN_EXEC, Json::Value("e")};
                if ( !operator_parser_(i, _n) ) return CIN_STATE::ERROR;
            }
        }

        while ( op_stack_.size() != 0 ) {
            item_stack_.push(op_stack_.top());
            op_stack_.pop();
        }

        if ( item_stack_.top().type != RPN_ITEM_TYPE::RPN_SET ) {
            err_msg_ = "missing '=' in code";
            return CIN_STATE::ERROR;
        }

        while ( item_stack_.size() != 0 ) {
            exec_stack_.push(item_stack_.top());
            item_stack_.pop();
        }

        pending_code_.clear();
        return CIN_STATE::DONE;
    }

    // Exec the last parsed RPN object
    CRUN_STATE code_run() {
        std::stack< RPNItem > _value_stack;
        std::string _node;

        std::stack< std::string > _node_stack;

        while ( exec_stack_.size() != 0 ) {
            RPNItem _rpn = exec_stack_.top();
            exec_stack_.pop();

            switch( _rpn.type ) {
            case RPN_NUMBER : 
            case RPN_STRING :
                _value_stack.push(_rpn); break;
            case RPN_PLUS : 
                {
                    if ( _value_stack.size() < 2 ) {
                        err_msg_ = "invalidate '+'";
                        return CRUN_STATE::ASSERT;
                    }
                    RPNItem _v1 = _value_stack.top(); _value_stack.pop();
                    RPNItem _v2 = _value_stack.top(); _value_stack.pop();
                    Json::Value &_jv1 = (_v1.type != RPN_PATH) ? _v1.value : *(node_by_path_(_v1.value));
                    Json::Value &_jv2 = (_v2.type != RPN_PATH) ? _v2.value : *(node_by_path_(_v2.value));
                    if ( _v2.type == RPN_STRING || (_v2.type == RPN_PATH && _jv2.isString() ) ) {
                        RPNItem _r{RPN_STRING, _jv2.asString() + _jv1.asString()};
                        _value_stack.push(_r);
                    } else if ( _v1.type == RPN_STRING || (_v1.type == RPN_PATH && _jv1.isString()) ) {
                        RPNItem _r{RPN_STRING, _jv2.asString() + _jv1.asString()};
                        _value_stack.push(_r);
                    } else if ( 
                        (_v1.type == RPN_NUMBER || (_v1.type == RPN_PATH && _jv1.isNumeric())) &&
                        (_v2.type == RPN_NUMBER || (_v2.type == RPN_PATH && _jv2.isNumeric()))
                    ) {
                        RPNItem _r{RPN_NUMBER, _jv2.asDouble() + _jv1.asDouble()};
                        _value_stack.push(_r);
                    } else {
                        err_msg_ = "invalidate type around '+'";
                        return CRUN_STATE::ASSERT;
                    }
                    break;
                }
            case RPN_MINUS : 
                {
                    if ( _value_stack.size() < 2 ) {
                        err_msg_ = "invalidate '-'";
                        return CRUN_STATE::ASSERT;
                    }
                    RPNItem _v1 = _value_stack.top(); _value_stack.pop();
                    RPNItem _v2 = _value_stack.top(); _value_stack.pop();
                    Json::Value &_jv1 = (_v1.type != RPN_PATH) ? _v1.value : *(node_by_path_(_v1.value));
                    Json::Value &_jv2 = (_v2.type != RPN_PATH) ? _v2.value : *(node_by_path_(_v2.value));

                    if ( 
                        (_v1.type == RPN_NUMBER || (_v1.type == RPN_PATH && _jv1.isNumeric())) &&
                        (_v2.type == RPN_NUMBER || (_v2.type == RPN_PATH && _jv2.isNumeric()))
                    ) {
                        RPNItem _r{RPN_NUMBER, _jv2.asDouble() - _jv1.asDouble()};
                        _value_stack.push(_r);
                    } else {
                        err_msg_ = "invalidate type around '-'";
                        return CRUN_STATE::ASSERT;
                    }
                    break;
                }
            case RPN_TIMES : 
                {
                    if ( _value_stack.size() < 2 ) {
                        err_msg_ = "invalidate '*'";
                        return CRUN_STATE::ASSERT;
                    }
                    RPNItem _v1 = _value_stack.top(); _value_stack.pop();
                    RPNItem _v2 = _value_stack.top(); _value_stack.pop();
                    Json::Value &_jv1 = (_v1.type != RPN_PATH) ? _v1.value : *(node_by_path_(_v1.value));
                    Json::Value &_jv2 = (_v2.type != RPN_PATH) ? _v2.value : *(node_by_path_(_v2.value));

                    if ( 
                        (_v1.type == RPN_NUMBER || (_v1.type == RPN_PATH && _jv1.isNumeric())) &&
                        (_v2.type == RPN_NUMBER || (_v2.type == RPN_PATH && _jv2.isNumeric()))
                    ) {
                        RPNItem _r{RPN_NUMBER, _jv2.asDouble() * _jv1.asDouble()};
                        _value_stack.push(_r);
                    } else {
                        err_msg_ = "invalidate type around '*'";
                        return CRUN_STATE::ASSERT;
                    }
                    break;
                }
            case RPN_DIVID : 
                {
                    if ( _value_stack.size() < 2 ) {
                        err_msg_ = "invalidate '/'";
                        return CRUN_STATE::ASSERT;
                    }
                    RPNItem _v1 = _value_stack.top(); _value_stack.pop();
                    RPNItem _v2 = _value_stack.top(); _value_stack.pop();
                    Json::Value &_jv1 = (_v1.type != RPN_PATH) ? _v1.value : *(node_by_path_(_v1.value));
                    Json::Value &_jv2 = (_v2.type != RPN_PATH) ? _v2.value : *(node_by_path_(_v2.value));

                    if ( 
                        (_v1.type == RPN_NUMBER || (_v1.type == RPN_PATH && _jv1.isNumeric())) &&
                        (_v2.type == RPN_NUMBER || (_v2.type == RPN_PATH && _jv2.isNumeric()))
                    ) {
                        RPNItem _r{RPN_NUMBER, _jv2.asDouble() / _jv1.asDouble()};
                        _value_stack.push(_r);
                    } else {
                        err_msg_ = "invalidate type around '/'";
                        return CRUN_STATE::ASSERT;
                    }
                    break;                    
                }
            case RPN_SET : 
                {
                    if ( _value_stack.size() < 2 ) {
                        err_msg_ = "invalidate '=', missing left side";
                        return CRUN_STATE::ASSERT;
                    }
                    RPNItem _v1 = _value_stack.top(); _value_stack.pop();
                    RPNItem _v2 = _value_stack.top(); _value_stack.pop();
                    if ( _v2.type != RPN_PATH ) {
                        err_msg_ = "left side must be a path";
                        return CRUN_STATE::ASSERT;
                    }
                    Json::Value &_jv1 = (_v1.type != RPN_PATH) ? _v1.value : *(node_by_path_(_v1.value));
                    Json::Value &_jv2 = *(node_by_path_(_v2.value));

                    _jv2 = _jv1;

                    break;                    
                }
            case RPN_NODE : 
                {
                    if ( _value_stack.size() < 1 ) {
                        err_msg_ = "invalidate path begin with '.'";
                        return CRUN_STATE::ASSERT;
                    }
                    if ( _value_stack.top().type != RPN_STRING ) {
                        err_msg_ = "invalidate path";
                        return CRUN_STATE::ASSERT;
                    }

                    _node = _value_stack.top().value.asString();
                    _value_stack.pop();

                    // Path unfinished
                    if ( _node != "$" ) {
                        _node_stack.push(_node);
                        break;
                    }

                    Json::Value _node_path(Json::arrayValue);
                    while ( _node_stack.size() > 0 ) {
                        _node = _node_stack.top();
                        _node_stack.pop();
                        _node_path.append(_node);
                    }
                    RPNItem _node_ref{RPN_PATH, _node_path};
                    _value_stack.push(_node_ref);
                    break;
                }
            case RPN_INDEX:
                {
                    if ( _value_stack.size() < 1 ) {
                        err_msg_ = "invalidate index";
                        return CRUN_STATE::ASSERT;
                    }
                    if ( _value_stack.top().type != )
                }
            default : 
                break;
            };
        }

        ON_DEBUG(
            std::cout << root_ << std::endl;
        )
        return CRUN_STATE::OK;
    }
};



void co_main( int argc, char* argv[] ) {
    bool _normal_return = false;
    utils::argparser::set_parser("help", "h", [&_normal_return](std::string&&) {
        _normal_return = true;
    });
    utils::argparser::set_parser("version", "v", [&_normal_return](std::string&&) {
        std::cout << "coas, ";
        #ifdef DEBUG
            std::cout << "Debug Version, ";
        #else
            std::cout << "Release Version, ";
        #endif
        std::cout << "v" << VERSION << std::endl;
        std::cout 
            << "Powered By Push Chen <littlepush@gmail.com>, as a sub project of PECo framework." 
            << std::endl;
        _normal_return = true;
    });

    // Do the parse
    if ( !utils::argparser::parse(argc, argv) ) {
        g_return = 1;
        return;
    }
    // On output info command
    if ( _normal_return ) return;

    auto _input = utils::argparser::individual_args();
    if ( _input.size() == 0 ) {
        // Begin Input from STDIN
        costage _stage;
        std::string _code;
        int _lno = 0;
        std::cout << "coas(" << _lno << "): ";
        while ( std::getline(std::cin, _code) ) {
            auto _flag = _stage.code_parser(std::move(_code));
            if ( _flag == CIN_STATE::ERROR ) {
                std::cerr << "Syntax Error: " << _stage.last_error_msg() << std::endl;
                g_return = 99;
                break;
            } else if ( _flag == CIN_STATE::UNFINISHED ) {
                continue;
            }
            // Now we get a full code, run it
            auto _ret = _stage.code_run();
            if ( _ret == CRUN_STATE::ASSERT ) {
                std::cerr << "Assert Failed: " << _stage.last_error_msg() << std::endl;
                g_return = 98;
                break;
            } else if ( _ret == CRUN_STATE::RETURN ) {
                if ( _stage.result ) {
                    std::cout << "Stage Pass" << std::endl;
                } else {
                    std::cout << "Stage Failed" << std::endl;
                }
                break;
            }
            // Run OK, then continue to get next code
            ++_lno;
            std::cout << "coas(" << _lno << "): ";
        }
    }
}

int main( int argc, char* argv[] ) {
    loop::main.do_job(std::bind(&co_main, argc, argv));
    loop::main.run();
    return g_return;
}