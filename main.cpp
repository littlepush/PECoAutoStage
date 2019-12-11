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

int g_return = 0;

enum CIN_STATE {
    DONE,
    UNFINISHED,
    ERROR
};

enum CRUN_STATE {
    OK,
    ASSERT,
    RETURN
};

enum RPN_ITEM_TYPE {
    RPN_DUMMY               = -1,

    // Items
    RPN_NUMBER              = 0,
    RPN_STRING,

    // Operators
    RPN_SET,
    RPN_PLUS,
    RPN_MINUS,
    RPN_TIMES,
    RPN_DIVID,
    RPN_EQUAL,
    RPN_LESS_THAN,
    RPN_LESS_EQUAL,
    RPN_GREAT_THAN,
    RPN_GREAT_EQUAL,
    RPN_NODE,

    RPN_LEFT_BRACKETS,      // [
    RPN_RIGHT_BRACKETS,     // ]

    RPN_LEFT_BRACES,        // {
    RPN_RIGHT_BRACES,       // }

    RPN_LEFT_PARENTHESES,   // (
    RPN_RIGHT_PARENTHESES   // )
};

struct RPNItem {
    RPN_ITEM_TYPE           type;
    Json::Value             value;
};

class costage {
    typedef  std::stack< RPNItem >  stack_type;


    Json::Value         root_;      // Root Node of every stage
    Json::Value*        pnode_;
    std::string         err_msg_;   // Last Error Message
    bool                result_;    // The final result of the stage

    std::string         pending_code_;

    // The RPN Stack
    stack_type          op_stack_;
    stack_type          item_stack_;
    stack_type          exec_stack_;

protected: 
    bool operator_parser_( size_t index, RPNItem& op ) {
        if ( op_stack_.size() == 0 ) {
            op_stack_.push(op); return true;
        }
        if ( op.type == RPN_ITEM_TYPE::RPN_LEFT_PARENTHESES ) {
            RPNItem& _top = op_stack_.top();
            if ( _top.type == RPN_ITEM_TYPE::RPN_NODE ) {
                RPNItem _d{RPN_ITEM_TYPE::RPN_DUMMY};
                item_stack_.push(_d);
            }
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
            op_stack_.top().type != RPN_ITEM_TYPE::RPN_RIGHT_PARENTHESES 
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

    costage() : pnode_(NULL), result_( true ), result(result_) { }

    const std::string& last_error_msg() const { return err_msg_; }

    CIN_STATE code_parser( std::string&& code ) {
        #define __GET_C_N   (i == pending_code_.size() - 1) ? '\0' : pending_code_[i + 1]
        pending_code_ += code;
        for ( size_t i = 0; i < pending_code_.size(); ++i ) {
            char c = pending_code_[i];
            char c_n = __GET_C_N;
            if ( std::isspace(c) ) continue;    // ignore unused space

            // Try to test if this is a number
            char *_pend = NULL;
            double _n = std::strtod(pending_code_.c_str() + i, &_pend);
            if ( _pend != (pending_code_.c_str() + i) ) {
                if ( _n == HUGE_VAL || errno == ERANGE ) {
                    // Failed to get the number
                    err_msg_ = "Invalidate number at index: " + std::to_string(i + 1);
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
                continue;
            }
            if ( c == '$' ) {
                if ( c_n == '.' ) { 
                    // Root Node
                    RPNItem _item{RPN_ITEM_TYPE::RPN_STRING, Json::Value("$")};
                    item_stack_.push(_item);
                } else {
                    err_msg_ = "Invalidate '$' at index: " + std::to_string(i + 1);
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
                RPNItem _i{_t, Json::Value(c)};
                if ( !operator_parser_(i, _i) )
                    return CIN_STATE::ERROR;
                continue;
            }
            if ( c == '<' ) {
                auto _t = (c_n == '=') ? RPN_ITEM_TYPE::RPN_LESS_EQUAL : RPN_ITEM_TYPE::RPN_LESS_THAN;
                RPNItem _i{_t, Json::Value(c)};
                if ( !operator_parser_(i, _i) )
                    return CIN_STATE::ERROR;
                continue;
            }
            if ( c == '=' ) {
                auto _t = (c_n == '=') ? RPN_ITEM_TYPE::RPN_EQUAL : RPN_ITEM_TYPE::RPN_SET;
                RPNItem _i{_t, Json::Value(c)};
                if ( !operator_parser_(i, _i) )
                    return CIN_STATE::ERROR;
                continue;
            }
            if ( c == '(' ) {
                RPNItem _i{RPN_ITEM_TYPE::RPN_LEFT_PARENTHESES, Json::Value(c)};
                if ( !operator_parser_(i, _i) )
                    return CIN_STATE::ERROR;
                continue;
            }
            if ( c == ')' ) {
                RPNItem _i{RPN_ITEM_TYPE::RPN_RIGHT_PARENTHESES, Json::Value(c)};
                if ( !operator_parser_(i, _i) )
                    return CIN_STATE::ERROR;
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
                err_msg_ = std::string("Invalidate '") + std::string((char)c, 1) + "' at index: " + std::to_string(i + 1);
                return CIN_STATE::ERROR;
            }
            std::string _s(1, c);
            while ( std::isalpha(c_n) || std::isdigit(c_n) || c_n == '_' || c_n == '-' ) {
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
            case RPN_DUMMY : 
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
                    if ( _v1.type == RPN_DUMMY || _v2.type == RPN_DUMMY ) {
                        err_msg_ = "missing argument around '+'";
                        return CRUN_STATE::ASSERT;
                    } else if ( _v1.type == RPN_STRING || _v2.type == RPN_STRING ) {
                        RPNItem _r{RPN_STRING, _v2.value.asString() + _v1.value.asString()};
                        _value_stack.push(_r);
                    } else {
                        RPNItem _r{RPN_NUMBER, _v2.value.asDouble() + _v1.value.asDouble()};
                        _value_stack.push(_r);
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
                    if ( _v1.type == RPN_DUMMY || _v2.type == RPN_DUMMY ) {
                        err_msg_ = "missing argument around '-'";
                        return CRUN_STATE::ASSERT;
                    } else if ( _v1.type == RPN_STRING || _v2.type == RPN_STRING ) {
                        err_msg_ = "string not support operator '-'";
                        return CRUN_STATE::ASSERT;
                    } else {
                        RPNItem _r{RPN_NUMBER, _v2.value.asDouble() - _v1.value.asDouble()};
                        _value_stack.push(_r);
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
                    if ( _v1.type == RPN_DUMMY || _v2.type == RPN_DUMMY ) {
                        err_msg_ = "missing argument around '*'";
                        return CRUN_STATE::ASSERT;
                    } else if ( _v1.type == RPN_STRING || _v2.type == RPN_STRING ) {
                        err_msg_ = "string not support operator '*'";
                        return CRUN_STATE::ASSERT;
                    } else {
                        RPNItem _r{RPN_NUMBER, _v2.value.asDouble() * _v1.value.asDouble()};
                        _value_stack.push(_r);
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
                    if ( _v1.type == RPN_DUMMY || _v2.type == RPN_DUMMY ) {
                        err_msg_ = "missing argument around '/'";
                        return CRUN_STATE::ASSERT;
                    } else if ( _v1.type == RPN_STRING || _v2.type == RPN_STRING ) {
                        err_msg_ = "string not support operator '/'";
                        return CRUN_STATE::ASSERT;
                    } else {
                        RPNItem _r{RPN_NUMBER, _v2.value.asDouble() / _v1.value.asDouble()};
                        _value_stack.push(_r);
                    }
                    break;                    
                }
            case RPN_SET : 
                {
                    if ( pnode_ == NULL ) {
                        err_msg_ = "invalidate left value of '='";
                        return CRUN_STATE::ASSERT;
                    }
                    if ( pnode_ == &root_ ) {
                        err_msg_ = "cannot set a value direct to root node";
                        return CRUN_STATE::ASSERT;
                    }
                    if ( _value_stack.size() == 0 ) {
                        err_msg_ = "missing right value '='";
                        return CRUN_STATE::ASSERT;
                    }
                    RPNItem _v1 = _value_stack.top(); _value_stack.pop();
                    *pnode_ = _v1.value;
                    pnode_ = NULL;
                    break;                    
                }
            case RPN_NODE : 
                {
                    if ( _value_stack.size() < 1 ) {
                        err_msg_ = "invalidate path begin with '.'";
                        return CRUN_STATE::ASSERT;
                    }
                    _node = _value_stack.top().value.asString();
                    _value_stack.pop();

                    if ( pnode_ != NULL ) {
                        // Next operator need two node, so push current node to the stack
                        if ( pnode_->isConvertibleTo(Json::stringValue) ) {
                            RPNItem _i{RPN_STRING, *pnode_};
                            _value_stack.push(_i);
                        } else if ( pnode_->isConvertibleTo(Json::realValue) ) {
                            RPNItem _i{RPN_NUMBER, *pnode_};
                            _value_stack.push(_i);
                        } else {
                            err_msg_ = "undefined error arround '$'";
                            return CRUN_STATE::ASSERT;
                        }
                        pnode_ = NULL;
                    }

                    // Path unfinished
                    if ( _node != "$" ) {
                        _node_stack.push(_node);
                        break;
                    }

                    pnode_ = &root_;
                    while ( _node_stack.size() > 0 ) {
                        _node = _node_stack.top();
                        _node_stack.pop();
                        if ( !pnode_->isMember(_node) ) {
                            if ( _node_stack.size() == 0 ) {
                                // Already reach end of the path
                                // Add a null
                                (*pnode_)[_node] = Json::Value(Json::nullValue);
                            } else {
                                (*pnode_)[_node] = Json::Value(Json::objectValue);
                            }
                        }
                        pnode_ = &(*pnode_)[_node];
                    }
                    break;
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