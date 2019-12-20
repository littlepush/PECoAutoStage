/*
    modules.cpp
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

#include <cotask.h>
#include <conet.h>
using namespace pe::co;

#include "modules.h"
#include "costage.h"

namespace coas {
    const std::string empty_string;

    // Module utilities
    // Return error object with message
    rpn::item_t module_error(const std::string& message) {
        return rpn::item_t{rpn::IT_ERROR, Json::Value(message)};
    }

    // Return a void item
    rpn::item_t module_void() {
        return rpn::item_t{rpn::IT_VOID, Json::Value(Json::nullValue)};
    }

    // Return a NULL item
    rpn::item_t module_null() {
        return rpn::item_t{rpn::IT_NULL, Json::Value(Json::nullValue)};
    }

    // Return an Array
    rpn::item_t module_array(Json::Value&& array) {
        return rpn::item_t{rpn::IT_ARRAY, std::move(array)};
    }

    // Return an Object
    rpn::item_t module_object(Json::Value&& object) {
        return rpn::item_t{rpn::IT_OBJECT, std::move(object)};
    }

    // Return a Number
    rpn::item_t module_number(Json::Value&& number) {
        return rpn::item_t{rpn::IT_NUMBER, std::move(number)};
    }

    // Return a string
    rpn::item_t module_string(Json::Value&& jstr) {
        return rpn::item_t{rpn::IT_STRING, std::move(jstr)};
    }

    // Return a Boolean
    rpn::item_t module_boolean(bool value) {
        return rpn::item_t{rpn::IT_BOOL, Json::Value(value)};
    }

    // Return an item by the value type
    rpn::item_t module_bytype(Json::Value&& value) {
        if ( value.isNumeric() ) return module_number(std::move(value));
        if ( value.isString() ) return module_string(std::move(value));
        if ( value.isObject() ) return module_object(std::move(value));
        if ( value.isArray() ) return module_array(std::move(value));
        if ( value.isBool() ) return rpn::item_t{rpn::IT_BOOL, std::move(value)};
        // Undefined type, just return null
        return module_null();
    }

    // Return an path
    rpn::item_t module_path(Json::Value&& path) {
        if ( path.isArray() == false ) return module_null();
        return rpn::item_t{rpn::IT_PATH, std::move(path)};
    }

    // Match anything, always return true
    bool module_match_anything( const Json::Value& invoker, bool is_root ) {
        return true;
    }

    // Match anything but null
    bool module_match_notnull( const Json::Value& invoker, bool is_root ) {
        return !invoker.isNull();
    }

    // Default for object and array
    bool module_match_default( const Json::Value& invoker, bool is_root ) {
        return ( invoker.isArray() || invoker.isObject() );
    }

    // Match a root invoker
    bool module_match_root( const Json::Value& invoker, bool is_root ) {
        return is_root;
    }

    // Match Only an Array invoker
    bool module_match_array( const Json::Value& invoker, bool is_root ) {
        return invoker.isArray();
    }

    // Match Only an Object
    bool module_match_object( const Json::Value& invoker, bool is_root ) {
        return invoker.isObject();
    }

    // Match Only a String
    bool module_match_string( const Json::Value& invoker, bool is_root ) {
        return invoker.isString();
    }

    // Match Only a Number
    bool module_match_number( const Json::Value& invoker, bool is_root ) {
        return invoker.isNumeric();
    }

    // Match With Specified type
    // The invoker must be an object and contains key: "__type"
    // Usage std::bind(&module_match_type, "my_type")
    bool module_match_type( const std::string& type, const Json::Value& invoker, bool is_root ) {
        if ( !invoker.isObject() ) return false;
        return (
            invoker.isMember("__type") && 
            invoker["__type"].asString() == type
        );
    }

    // Match Specifial Key
    // The invoker must be an object and contains specifial key
    bool module_match_key( const std::string& key, const Json::Value& invoker, bool is_root ) {
        if ( !invoker.isObject() ) return false;
        return (invoker.isMember(key));
    }

    bool module_match_key_and_type( 
        const std::string& key, 
        const std::string& type, 
        const Json::Value& invoker, bool is_root 
    ) {
        return (module_match_key(key, invoker, is_root) && 
            module_match_type(type, invoker, is_root));
    }

    // Match Job Object
    bool module_match_job( const Json::Value& invoker, bool is_root ) {
        return module_match_key_and_type("__stack", "coas.job", invoker, is_root);
    }

    // Match a condition
    bool module_match_condition( const Json::Value& invoker, bool is_root ) {
        return (
            module_match_key("__expr", invoker, is_root) &&
            module_match_key("__job", invoker, is_root) &&
            module_match_type("coas.cond", invoker, is_root) &&
            (invoker["__expr"].isNull() || module_match_job(invoker["__expr"], is_root)) &&
            module_match_job(invoker["__job"], is_root)
        );
    }

    // Unpack argument
    const Json::Value* module_unpack_arg( costage& stage, const rpn::item_t& arg ) {
        const Json::Value* _arg = NULL;
        if ( arg.type == rpn::IT_PATH ) {
            _arg = stage.get_node(arg.value);
        } else {
            _arg = &(arg.value);
        }
        return _arg;
    }

    const Json::Value* module_unpack_arg( 
        costage& stage, 
        const rpn::item_t& arg, 
        rpn::item_type type 
    ) {
        if ( arg.type == rpn::IT_PATH ) {
            Json::Value* _arg = stage.get_node(arg.value);
            if ( _arg == NULL ) return NULL;
            if ( _arg->isString() && type == rpn::IT_STRING ) return _arg;
            if ( _arg->isBool() && type == rpn::IT_BOOL ) return _arg;
            if ( _arg->isNumeric() && type == rpn::IT_NUMBER ) return _arg;
            if ( _arg->isArray() && type == rpn::IT_ARRAY ) return _arg;
            if ( _arg->isObject() && type == rpn::IT_ARRAY ) return _arg;
            if ( _arg->isNull() && type == rpn::IT_NULL ) return _arg;
            return NULL;
        } else {
            if ( type != arg.type ) return NULL;
            return &arg.value;
        }
    }

    // Get a Stack Name according to the arg
    std::string module_unpack_stack( costage& stage, const rpn::item_t& arg ) {
        if ( arg.type == rpn::IT_STACK ) {
            return arg.value.asString();
        } else if ( arg.type == rpn::IT_PATH ) {
            Json::Value* _parg = stage.get_node(arg.value);
            if ( _parg == NULL ) return std::string("");
            if ( !_parg->isObject() ) return std::string("");
            if ( ! module_match_key_and_type(
                "__stack", "coas.job", *_parg, false) 
            ) {
                return std::string("");
            }
            return (*_parg)["__stack"].asString();
        } else if ( arg.type == rpn::IT_OBJECT ) {
            if ( ! module_match_key_and_type(
                "__stack", "coas.job", arg.value, false)
            ) {
                return std::string("");
            }
            return arg.value["__stack"].asString();
        } else {
            return std::string("");
        }
    }

    void module_prepare_call(
        costage& stage, 
        const rpn::item_t& this_path, 
        int ithis, int ilast, int icurrent
    ) {
        // Prepare this
        rpn::item_t _this = this_path;
        if ( ithis >= 0 ) {
            _this.value.append(ithis);
        }
        stage.push_this(std::move(_this));

        // Prepare last
        if ( ilast >= 0 ) {
            rpn::item_t _last = this_path;
            _last.value.append(ilast);
            stage.push_last(std::move(_last));
        } else {
            stage.push_last(module_null());
        }

        // Prepare current
        if ( icurrent >= 0 ) {
            rpn::item_t _current = this_path;
            _current.value.append(icurrent);
            stage.push_current(std::move(_current));
        } else {
            stage.push_current(module_null());
        }
    }

    void module_prepare_call(
        costage& stage,
        const rpn::item_t& this_path,
        const std::string& cthis,
        const std::string& clast,
        const std::string& ccurrent
    ) {
        // Prepare this
        rpn::item_t _this = this_path;
        if ( cthis.size() > 0 ) {
            _this.value.append(cthis);
        }
        stage.push_this(std::move(_this));

        // Prepare last
        if ( clast.size() > 0 ) {
            rpn::item_t _last = this_path;
            _last.value.append(clast);
            stage.push_last(std::move(_last));
        } else {
            stage.push_last(module_null());
        }

        // Prepare current
        if ( ccurrent.size() > 0 ) {
            rpn::item_t _current = this_path;
            _current.value.append(ccurrent);
            stage.push_current(std::move(_current));
        } else {
            stage.push_current(module_null());
        }
    }
    // Clear the call stack
    void module_clear_call(costage& stage) {
        stage.pop_current();
        stage.pop_last();
        stage.pop_this();
    }

    rpn::item_t __func_compare(
        costage& stage, 
        const rpn::item_t& this_path, 
        const std::vector< rpn::item_t >& args,
        std::function< bool(const Json::Value&, const Json::Value&) > comp
    ) {
        Json::Value* _pthis = stage.get_node(this_path.value);
        if ( args.size() > 1 ) return module_error("too many arguments");
        std::string _cmpr_stack;
        if ( args.size() == 1 ) {
            _cmpr_stack = module_unpack_stack(stage, args[0]);
            if ( _cmpr_stack.size() == 0 ) {
                return module_error("invalidate argument");
            }
        }
        Json::Value& rthis = *_pthis;
        if ( rthis.size() == 0 ) return module_null();

        auto _result = this_path;
        if ( _cmpr_stack.size() == 0 ) {
            // Use default compare, and this must be array
            if ( !rthis.isArray() ) {
                return module_error("unknow compare method on object");
            }
            Json::ArrayIndex _current = 0;
            for ( Json::ArrayIndex i = 1; i < rthis.size(); ++i ) {
                if ( comp(rthis[i], rthis[_current]) ) _current = i;
            }
            _result.value.append(_current);
            return _result;
        }

        if ( rthis.isArray() ) {
            Json::ArrayIndex _icurrent = 0;
            for ( Json::ArrayIndex i = 1; i < rthis.size(); ++i ) {
                module_prepare_call(stage, this_path, i, i - 1, _icurrent);
                auto _e = stage.invoke_group( _cmpr_stack );
                module_clear_call(stage);

                // toas the error message
                if ( _e == E_ASSERT ) return module_error(stage.err_string());
                if ( _e == E_OK ) return module_error("missing compare result");
                if ( stage.returnValue().isBool() == false ) {
                    return module_error("invalidate return type");
                }
                if ( stage.returnValue().asBool() ) _icurrent = i;
            }
            _result.value.append(_icurrent);
        } else {
            auto _it = rthis.begin();
            std::string _ccurrent = rthis.begin().key().asString();
            std::string _clast = _ccurrent;
            ++_it;
            for ( ; _it != rthis.end(); ++_it ) {
                module_prepare_call(stage, this_path, _it.key().asString(), _clast, _ccurrent);
                auto _e = stage.invoke_group( _cmpr_stack );
                module_clear_call(stage);
                _clast = _it.key().asString();

                // toas the error message
                if ( _e == E_ASSERT ) return module_error(stage.err_string());
                if ( _e == E_OK ) return module_error("missing compare result");
                if ( stage.returnValue().isBool() == false ) {
                    return module_error("invalidate return type");
                }
                if ( stage.returnValue().asBool() ) _ccurrent = _clast;
            }
            _result.value.append(_ccurrent);
        }
        return _result;
    }
    rpn::item_t __func_max( 
        costage& stage, 
        const rpn::item_t& this_path, 
        const std::vector< rpn::item_t >& args 
    ) {
        return __func_compare(stage, this_path, args, 
            [](const Json::Value& _this, const Json::Value& _last) {
                return _this > _last;
            }
        );
    }
    rpn::item_t __func_min( 
        costage& stage, 
        const rpn::item_t& this_path, 
        const std::vector< rpn::item_t >& args 
    ) {
        return __func_compare(stage, this_path, args, 
            [](const Json::Value& _this, const Json::Value& _last) {
                return _this < _last;
            }
        );
    }

    rpn::item_t __func_fetch(
        costage& stage, 
        const rpn::item_t& this_path, 
        const std::vector< rpn::item_t >& args 
    ) {
        Json::Value* _pthis = stage.get_node(this_path.value);
        Json::Value& _rthis = *_pthis;

        // Must have the fetch stack
        if ( args.size() != 1 ) {
            return module_error("missing argument");
        }
        std::string _stack = module_unpack_stack(stage, args[0]);
        if ( _stack.size() == 0 ) {
            return module_error("invalidate argument");
        }

        auto _result = this_path;
        if ( _rthis.isArray() ) {
            for ( Json::ArrayIndex i = 0; i < _rthis.size(); ++i ) {
                module_prepare_call(stage, this_path, i, (int)i - 1);
                // Invoke the sub func to compare
                auto _e = stage.invoke_group( _stack );
                module_clear_call(stage);

                // toast the error message
                if (_e == E_ASSERT) return module_error(stage.err_string());
                if (_e == E_OK ) return module_error("missing compare result");
                if ( stage.returnValue().isBool() == false ) {
                    return module_error("invalidate return type");
                }
                // First match
                if ( stage.returnValue().asBool() ) {
                    _result.value.append(i);
                    return _result;
                }
            }
        } else {
            auto _it = _rthis.begin();
            std::string _clast;
            for (; _it != _rthis.end(); ++_it ) {
                module_prepare_call(stage, this_path, _it.key().asString(), _clast);
                // Invoke the sub func to compare
                auto _e = stage.invoke_group( _stack );
                module_clear_call(stage);
                _clast = _it.key().asString();

                // toast the error message
                if (_e == E_ASSERT) return module_error(stage.err_string());
                if (_e == E_OK ) return module_error("missing compare result");
                if ( stage.returnValue().isBool() == false ) {
                    return module_error("invalidate return type");
                }
                if ( stage.returnValue().asBool() ) {
                    _result.value.append(_clast);
                    return _result;
                }
            }
        }
        return module_void();
    }
    rpn::item_t __func_foreach(
        costage& stage, 
        const rpn::item_t& this_path, 
        const std::vector< rpn::item_t >& args 
    ) {
        Json::Value* _pthis = stage.get_node(this_path.value);
        Json::Value& _rthis = *_pthis;

        // Must have the fetch stack
        if ( args.size() != 1 ) {
            return module_error("missing argument");
        }
        std::string _stack = module_unpack_stack(stage, args[0]);
        if ( _stack.size() == 0 ) {
            return module_error("invalidate argument");
        }

        if ( _rthis.isArray() ) {
            for ( Json::ArrayIndex i = 0; i < _rthis.size(); ++i ) {
                module_prepare_call(stage, this_path, i, (int)i - 1);
                // Invoke the sub func to compare
                auto _e = stage.invoke_group( _stack );
                module_clear_call(stage);

                // toast the error message
                if (_e == E_ASSERT) return module_error(stage.err_string());
                if (_e == E_RETURN ) return module_error("foreach doesn't need return");
            }
        } else {
            auto _it = _rthis.begin();
            std::string _clast;
            for (; _it != _rthis.end(); ++_it ) {
                module_prepare_call(stage, this_path, _it.key().asString(), _clast);
                // Invoke the sub func to compare
                auto _e = stage.invoke_group( _stack );
                module_clear_call(stage);
                _clast = _it.key().asString();

                // toast the error message
                if (_e == E_ASSERT) return module_error(stage.err_string());
                if (_e == E_RETURN ) return module_error("foreach doesn't need return");
            }
        }
        return module_void();
    }
    rpn::item_t __func_filter(
        costage& stage, 
        const rpn::item_t& this_path, 
        const std::vector< rpn::item_t >& args 
    ) {
        Json::Value* _pthis = stage.get_node(this_path.value);
        Json::Value& _rthis = *_pthis;

        // Must have the fetch stack
        if ( args.size() != 1 ) {
            return module_error("missing argument");
        }
        std::string _stack = module_unpack_stack(stage, args[0]);
        if ( _stack.size() == 0 ) {
            return module_error("invalidate argument");
        }

        Json::Value _result(Json::arrayValue);
        if ( _rthis.isArray() ) {
            for ( Json::ArrayIndex i = 0; i < _rthis.size(); ++i ) {
                module_prepare_call(stage, this_path, i, (int)i - 1);
                // Invoke the sub func to compare
                auto _e = stage.invoke_group( _stack );
                module_clear_call(stage);

                // toast the error message
                if (_e == E_ASSERT) return module_error(stage.err_string());
                if (_e == E_OK ) return module_error("missing filter result");
                if ( stage.returnValue().isBool() == false ) {
                    return module_error("invalidate return type");
                }
                if ( stage.returnValue().asBool() ) {
                    _result.append(_rthis[i]);
                }
            }
        } else {
            auto _it = _rthis.begin();
            std::string _clast;
            for (; _it != _rthis.end(); ++_it ) {
                module_prepare_call(stage, this_path, _it.key().asString(), _clast);
                // Invoke the sub func to compare
                auto _e = stage.invoke_group( _stack );
                module_clear_call(stage);
                _clast = _it.key().asString();

                // toast the error message
                if (_e == E_ASSERT) return module_error(stage.err_string());
                if (_e == E_OK ) return module_error("missing filter result");
                if ( stage.returnValue().isBool() == false ) {
                    return module_error("invalidate return type");
                }
                if ( stage.returnValue().asBool() ) {
                    _result.append(*_it);
                }
            }
        }
        return module_array(std::move(_result));
    }

    rpn::item_t __func_size(
        costage& stage, 
        const rpn::item_t& this_path, 
        const std::vector< rpn::item_t >& args 
    ) {
        Json::Value* _pthis = stage.get_node(this_path.value);
        if ( args.size() != 0 ) {
            return module_error("too many arguments");
        }

        if ( _pthis->isString() ) 
            return module_number((int)_pthis->asString().size());
        if ( 
            _pthis->isNumeric() || 
            _pthis->isNull() 
        ) return module_number(0);
        return module_number(_pthis->size());
    }
    rpn::item_t __func_stdin(
        costage& stage, 
        const rpn::item_t& this_path, 
        const std::vector< rpn::item_t >& args 
    ) {
        if ( args.size() != 0 ) {
            return module_error("too many arguments");
        }

        std::string _line;
        pe::co::waiting_signals _sig = pe::co::no_signal;
        fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK );
        do {
            _sig = pe::co::this_task::wait_fd_for_event(
                STDIN_FILENO, pe::co::event_read, std::chrono::seconds(1)
            );
            if ( _sig == pe::co::bad_signal ) break;
            if ( _sig == pe::co::receive_signal ) {
                std::getline(std::cin, _line);
                return module_string(_line);
            }
        } while ( true );
        return module_error("input interrpted");
    }
    rpn::item_t __func_stdout(
        costage& stage, 
        const rpn::item_t& this_path, 
        const std::vector< rpn::item_t >& args 
    ) {
        if ( args.size() == 0 ) {
            std::cout << stage.rootValue().toStyledString();
        } else {
            for ( const auto& i : args ) {
                const Json::Value *_parg = module_unpack_arg(stage, i);
                if ( _parg == NULL ) {
                    std::cout << "null" << std::endl;
                } else {
                    std::cout << *_parg << std::endl;
                }
            }
        }
        return module_void();
    }
    rpn::item_t __func_stderr(
        costage& stage, 
        const rpn::item_t& this_path, 
        const std::vector< rpn::item_t >& args 
    ) {
        if ( args.size() == 0 ) {
            std::cerr << stage.rootValue().toStyledString();
        } else {
            for ( auto& i : args ) {
                const Json::Value *_parg = module_unpack_arg(stage, i);
                if ( _parg == NULL ) {
                    std::cerr << "null" << std::endl;
                } else {
                    std::cerr << *_parg << std::endl;
                }
            }
        }
        return module_void();
    }

    rpn::item_t __func_job(
        costage& stage, 
        const rpn::item_t& this_path, 
        const std::vector< rpn::item_t >& args 
    ) {
        if ( args.size() != 1 ) {
            return module_error("missing or invalidate job argument");
        }

        const rpn::item_t& _rarg = args[0];
        if ( _rarg.type != rpn::IT_STACK ) {
            return module_error("invalidate argument");
        }
        Json::Value _jobValue(Json::objectValue);
        _jobValue["__stack"] = _rarg.value;    // Copy the stack information
        _jobValue["__type"] = "coas.job";
        return module_object(std::move(_jobValue));
    }

    // Wrap the function name as into a job object
    rpn::item_t __func_func(
        costage& stage, 
        const rpn::item_t& this_path, 
        const std::vector< rpn::item_t >& args 
    ) {
        if ( args.size() != 1 ) {
            return module_error("missing or invalidate job argument");
        }

        const Json::Value* _parg = module_unpack_arg(stage, args[0], rpn::IT_STRING);
        if ( _parg == NULL ) {
            return module_error("invalidate argument");
        }
        Json::Value _jobValue(Json::objectValue);
        _jobValue["__stack"] = _parg->asString();
        _jobValue["__type"] = "coas.job";
        return module_object(std::move(_jobValue));
    }

    rpn::item_t __func_invoke(
        costage& stage, 
        const rpn::item_t& this_path, 
        const std::vector< rpn::item_t >& args 
    ) {
        if ( args.size() != 1 ) {
            return module_error("missing or invalidate job argument");
        }
        const Json::Value* _parg = module_unpack_arg(stage, args[0], rpn::IT_STRING);
        if ( _parg == NULL ) {
            return module_error("invalidate argument");
        }
        auto _e = stage.invoke_group(_parg->asString());
        if ( _e == E_ASSERT ) return module_error(stage.err_string());
        if ( _e == E_RETURN ) return stage.resultItem();
        return module_void();
    }

    rpn::item_t __func_do(
        costage& stage, 
        const rpn::item_t& this_path, 
        const std::vector< rpn::item_t >& args 
    ) {
        if ( args.size() != 0 ) {
            return module_error("too many arguments in do");
        }
        Json::Value* _pthis = stage.get_node(this_path.value);
        auto _e = stage.invoke_group((*_pthis)["__stack"].asString());
        if ( _e == E_ASSERT ) return module_error(stage.err_string());
        if ( _e == E_RETURN ) return stage.resultItem();
        return module_void();
    }

    rpn::item_t __func_condition(
        costage& stage, 
        const rpn::item_t& this_path, 
        const std::vector< rpn::item_t >& args 
    ) {
        if ( args.size() != 2 ) {
            return module_error("missing condition argument");
        }
        Json::Value _condValue(Json::objectValue);
        const Json::Value* _pexpr = module_unpack_arg(stage, args[0], rpn::IT_NULL);
        if ( _pexpr != NULL ) {
            _condValue["__expr"] = *_pexpr;   // NULL Value
        } else {
            _pexpr = module_unpack_arg(stage, args[0], rpn::IT_OBJECT);
            if ( _pexpr == NULL ) {
                // Not null && not object
                return module_error("invalidate expr");
            }
            if ( !module_match_job(*_pexpr, false) ) {
                return module_error("expr is not a job object");
            }
            _condValue["__expr"] = *_pexpr;
        }
        const Json::Value* _pjob = module_unpack_arg(stage, args[1], rpn::IT_OBJECT);
        if ( _pjob == NULL ) {
            return module_error("missing job");
        }
        if ( !module_match_job(*_pjob, false) ) {
            return module_error("invalidate job");
        }
        _condValue["__job"] = *_pjob;
        _condValue["__type"] = "coas.cond";
        return module_object(std::move(_condValue));
    }

    rpn::item_t __func_check(
        costage& stage, 
        const rpn::item_t& this_path, 
        const std::vector< rpn::item_t >& args 
    ) {
        if ( args.size() != 0 ) {
            return module_error("too many arguments in check");
        }
        Json::Value* _pthis = stage.get_node(this_path.value);
        if ( _pthis->size() == 0 ) {
            return module_error("need at least one condition to check");
        }
        Json::Value& _rthis = *_pthis;
        for ( Json::ArrayIndex i = 0; i < _rthis.size(); ++i ) {
            Json::Value& _cond = _rthis[i];
            if ( !module_match_condition(_cond, false) ) {
                return module_error(
                    "invalidate condition case at: " + 
                    this_path.value.toStyledString() + 
                    "[" + std::to_string(i) + "]"
                );
            }
            bool _expr_result = false;
            if ( _cond["__expr"].isNull() ) {
                if ( i != (_rthis.size() - 1) ) {
                    return module_error("null expr can only be the last case");
                }
                _expr_result = true;
            } else {
                Json::Value& _expr = _cond["__expr"];
                auto _e = stage.invoke_group(_expr["__stack"].asString());
                if ( _e == E_ASSERT ) return module_error(stage.err_string());
                if ( _e == E_OK ) return module_error("expr with no boolean return");
                if ( !stage.returnValue().isBool() ) {
                    return module_error("exrp's return value is not a boolean");
                }
                _expr_result = stage.returnValue().asBool();
            }
            // Skip next if exrp check failed
            if ( _expr_result == false ) continue;

            Json::Value& _job = _cond["__job"];
            auto _e = stage.invoke_group(_job["__stack"].asString());
            if ( _e == E_ASSERT ) return module_error(stage.err_string());
            if ( _e == E_RETURN ) return module_error("invalidate return in condition's job");
            break;  // Now we match the condition.
        }
        return module_void();
    }
    rpn::item_t __func_loop(
        costage& stage, 
        const rpn::item_t& this_path, 
        const std::vector< rpn::item_t >& args 
    ) {
        if ( args.size() != 3 ) {
            return module_error("missing loop argument");
        }

        // Unpack expr
        bool _exprAlwaysTrue = false;
        std::string _expr;
        const Json::Value* _pexpr = module_unpack_arg(stage, args[0], rpn::IT_NULL);
        if ( _pexpr != NULL ) _exprAlwaysTrue = true;
        else {
            _pexpr = module_unpack_arg(stage, args[0], rpn::IT_BOOL);
            if ( _pexpr != NULL ) {
                _exprAlwaysTrue = _pexpr->asBool();
                if ( _exprAlwaysTrue == false ) {
                    return module_error("loop expr always be false");
                }
            } else {
                _expr = module_unpack_stack(stage, args[0]);
                if ( _expr.size() == 0 ) {
                    return module_error("invalidate expr");
                }
            }
        }

        // Unpack Job
        std::string _job = module_unpack_stack(stage, args[1]);
        if ( _job.size() == 0 ) {
            return module_error("invalidate job");
        }

        // Unpack after
        std::string _after = module_unpack_stack(stage, args[2]);
        if ( _after.size() == 0 ) {
            const Json::Value *_pafter = module_unpack_arg(stage, args[2], rpn::IT_NULL);
            if ( _pafter == NULL ) {
                return module_error("missing loop after");
            }
        }

        while ( true ) {
            bool _condition = _exprAlwaysTrue;
            if ( !_condition ) {
                auto _e = stage.invoke_group(_expr);
                if ( _e == E_ASSERT ) return module_error(stage.err_string());
                if ( _e == E_OK ) return module_error("expr must has a return value");
                if ( !stage.returnValue().isBool() ) {
                    return module_error("expr must return a boolean value");
                }
                _condition = stage.returnValue().asBool();
            }
            if ( !_condition ) break;

            // Do job
            auto _ej = stage.invoke_group(_job);
            if ( _ej == E_ASSERT ) return module_error(stage.err_string());
            if ( _ej == E_RETURN ) return module_error("invalidate return in loop's job");

            // Do after
            if ( _after.size() == 0 ) continue;
            auto _ea = stage.invoke_group(_after);
            if ( _ea == E_ASSERT ) return module_error(stage.err_string());
            if ( _ea == E_RETURN ) return module_error("invalidate return in loop's after");
        }
        return module_void();
    }

    rpn::item_t __func_httpreq(
        costage& stage, 
        const rpn::item_t& this_path, 
        const std::vector< rpn::item_t >& args 
    ) {
        if ( args.size() != 1 ) {
            return module_error("missing url");
        }
        const Json::Value *_purl = module_unpack_arg(stage, args[0], rpn::IT_STRING);
        if ( _purl == NULL ) {
            return module_error("null url");
        }
        std::string _url = _purl->asString();

        // Create the url object
        net::http_request _req = net::http_request::fromURL(_url);
        Json::Value _jreq(Json::objectValue);
        _jreq["url"] = _url;
        _jreq["method"] = "GET";
        _jreq["path"] = _req.path();
        Json::Value _jh(Json::objectValue);
        for ( auto i = _req.header.begin(); i != _req.header.end(); ++i ) {
            _jh[i->first] = i->second;
        }
        _jreq["header"] = _jh;
        Json::Value _jp(Json::objectValue);
        for ( auto i = _req.params.begin(); i != _req.params.end(); ++i ) {
            _jp[i->first] = i->second;
        }
        _jreq["param"] = _jp;
        _jreq["cookie"] = Json::Value(Json::objectValue);
        _jreq["__type"] = "coas.httpreq";
        return module_object(std::move(_jreq));
    }    

    rpn::item_t __func_send(
        costage& stage, 
        const rpn::item_t& this_path, 
        const std::vector< rpn::item_t >& args 
    ) {
        size_t _timedout = 60000;   // 1 min
        if ( args.size() > 1 ) {
            return module_error("too many arguments in http send");
        }
        if ( args.size() == 1 ) {
            const Json::Value *_ptimeout = module_unpack_arg(stage, args[0], rpn::IT_NUMBER);
            if ( _ptimeout == NULL ) {
                return module_error("invalidate timedout");
            }
            _timedout = _ptimeout->asUInt();
        }
        Json::Value* _pthis = stage.get_node(this_path.value);
        net::http_request _req = net::http_request::fromURL((*_pthis)["url"].asString());
        if ( _pthis->isMember("method") && (*_pthis)["method"].isString() ) {
            _req.method() = (*_pthis)["method"].asString();
        }
        if ( _pthis->isMember("header") && (*_pthis)["header"].isObject() ) {
            Json::Value &_jh = (*_pthis)["header"];
            for ( auto i = _jh.begin(); i != _jh.end(); ++i ) {
                _req.header[i.key().asString()] = i->asString();
            }
        }
        if ( _pthis->isMember("cookie") && (*_pthis)["cookie"].isObject() ) {
            Json::Value &_jc = (*_pthis)["cookie"];
            std::list< std::string > _lcookie;
            for ( auto i = _jc.begin(); i != _jc.end(); ++i ) {
                _lcookie.emplace_back( i.key().asString() + "=" + i->asString());
            }
            _req.header["cookie"] = utils::join(_lcookie, "; ");
        }
        if ( _pthis->isMember("body") && _req.method() != "GET" ) {
            std::string _body;
            if ( _req.header.contains("Content-Type") ) {
                std::string _ct = _req.header["Content-Type"];
                if ( utils::is_string_start(_ct, "application/json") ) {
                    Json::FastWriter _fw;
                    _body = _fw.write((*_pthis)["body"]);
                }
            }
            if ( _body.size() == 0 ) {
                if ( (*_pthis)["body"].isString() ) {
                    _body = (*_pthis)["body"].asString();
                } else {
                    std::vector< std::string > _params;
                    Json::Value& _jb = (*_pthis)["body"];
                    if ( !_jb.isObject() ) {
                        Json::FastWriter _fw;
                        _body = _fw.write((*_pthis)["body"]);
                    } else {
                        bool _validate_kv = true;
                        for ( auto i = _jb.begin(); i != _jb.end(); ++i ) {
                            if ( i->isString() ) {
                                _params.emplace_back(i.key().asString() + "=" + i->asString());
                            } else {
                                _validate_kv = false;
                                break;
                            }
                        }
                        if ( !_validate_kv ) {
                            Json::FastWriter _fw;
                            _body = _fw.write((*_pthis)["body"]);
                        }
                    }
                }
            }
            if ( _body.size() > 0 ) {
                _req.body.append(std::move(_body));
            }
        }
        if ( _pthis->isMember("param") && (*_pthis)["param"].isObject() ) {
            Json::Value &_jp = (*_pthis)["param"];
            for ( auto i = _jp.begin(); i != _jp.end(); ++i ) {
                _req.params[i.key().asString()] = i->asString();
            }
        }
        this_task::begin_tick();
        net::http_response _resp = net::http_connection::send_request(
            _req, std::chrono::milliseconds(_timedout));
        if ( _resp.status_code == net::CODE_001 ) {
            return module_error("request failed, connection failed or timedout");
        }
        double _api_time = this_task::tick();
        Json::Value _jresp(Json::objectValue);
        _jresp["__type"] = "coas.httpresp";
        _jresp["time"] = _api_time;
        _jresp["status_code"] = (int)_resp.status_code;
        _jresp["message"] = _resp.message;
        Json::Value _jh(Json::objectValue);
        for ( auto i = _resp.header.begin(); i != _resp.header.end(); ++i ) {
            _jh[i->first] = i->second;
        }
        _jresp["header"] = _jh;
        if ( _resp.header.contains("cookie") ) {
            std::string _c = _resp.header["cookie"];
            auto _cp = utils::split(_c, "; ");
            Json::Value _jc(Json::objectValue);
            for ( auto& c : _cp ) {
                auto _ckv = utils::split(c, "=");
                _jc[_ckv[0]] = (_ckv.size() == 2 ? _ckv[1] : "");
            }
            _jresp["cookie"] = _jc;
        }
        std::string _body;
        for ( auto i = _resp.body.begin(); i != _resp.body.end(); ++i ) {
            _body += *i;
        }
        if ( _resp.header.contains("content-type") ) {
            std::string _ct = _resp.header["content-type"];
            auto _cts = utils::split(_ct, ";");
            bool _is_json = false;
            for ( auto& _c : _cts ) {
                if ( _c == "application/json" ) {
                    Json::Reader _jr;
                    Json::Value _broot;
                    if ( !_jr.parse(_body, _broot) ) {
                        return module_error("body is invalidate json");
                    }
                    _jresp["body"] = _broot;
                    _is_json = true;
                    break;
                }
            }
            if ( !_is_json ) {
                _jresp["body"] = _body;
            }
        } else {
            _jresp["body"] = _body;
        }
        return module_object(std::move(_jresp));
    }
    rpn::item_t __func_json(
        costage& stage, 
        const rpn::item_t& this_path, 
        const std::vector< rpn::item_t >& args 
    ) {
        if ( args.size() != 0 ) {
            return module_error("too many arguments in json");
        }
        Json::Value* _pthis = stage.get_node(this_path.value);
        Json::Value _node;
        Json::Reader _jreader;
        if ( !_jreader.parse(_pthis->asString(), _node, false) ) {
            return module_error(_jreader.getFormattedErrorMessages());
        }
        return module_object(std::move(_node));
    }

    rpn::item_t __func_jsonstr(
        costage& stage, 
        const rpn::item_t& this_path, 
        const std::vector< rpn::item_t >& args 
    ) {
        Json::Value* _pthis = stage.get_node(this_path.value);
        Json::FastWriter _w;
        return module_string(_w.write(*_pthis));
    }

    rpn::item_t __func_sleep(
        costage& stage, 
        const rpn::item_t& this_path, 
        const std::vector< rpn::item_t >& args 
    ) {
        if ( args.size() != 1 ) {
            return module_error("sleep need specified time, in milliseconds");
        }
        const Json::Value* _parg = module_unpack_arg(stage, args[0], rpn::IT_NUMBER);
        if ( _parg == NULL ) {
            return module_error("sleep need a number argument");
        }
        this_task::sleep(std::chrono::milliseconds(_parg->asUInt()));
        return module_void();
    }

    rpn::item_t __func_toInt(
        costage& stage, 
        const rpn::item_t& this_path, 
        const std::vector< rpn::item_t >& args 
    ) {
        Json::Value* _pthis = stage.get_node(this_path.value);
        return module_number(std::stoi(_pthis->asString()));
    }

    rpn::item_t __func_toNumber(
        costage& stage, 
        const rpn::item_t& this_path, 
        const std::vector< rpn::item_t >& args 
    ) {
        Json::Value* _pthis = stage.get_node(this_path.value);
        return module_number(std::stod(_pthis->asString()));
    }

    rpn::item_t __func_toDouble(
        costage& stage, 
        const rpn::item_t& this_path, 
        const std::vector< rpn::item_t >& args 
    ) {
        Json::Value* _pthis = stage.get_node(this_path.value);
        return module_number(std::stod(_pthis->asString()));
    }

    rpn::item_t __func_toFloat(
        costage& stage, 
        const rpn::item_t& this_path, 
        const std::vector< rpn::item_t >& args 
    ) {
        Json::Value* _pthis = stage.get_node(this_path.value);
        return module_number(std::stof(_pthis->asString()));
    }

    rpn::item_t __func_toString(
        costage& stage, 
        const rpn::item_t& this_path, 
        const std::vector< rpn::item_t >& args 
    ) {
        Json::Value* _pthis = stage.get_node(this_path.value);
        return module_string(_pthis->asString());
    }

    rpn::item_t __func_toFile(
        costage& stage, 
        const rpn::item_t& this_path, 
        const std::vector< rpn::item_t >& args 
    ) {
        if ( args.size() != 1 ) {
            return module_error("missing file path");
        }
        const Json::Value* _ppath = module_unpack_arg(stage, args[0], rpn::IT_STRING);
        if ( _ppath == NULL ) {
            return module_error("file path cannot be null");
        }
        std::ofstream _of(_ppath->asString());
        if ( !_of ) {
            return module_error("cannot open file for writing");
        }
        Json::Value* _pthis = stage.get_node(this_path.value);
        _of << *_pthis;
        return module_void();
    }

    rpn::item_t __func_loadFile(
        costage& stage, 
        const rpn::item_t& this_path, 
        const std::vector< rpn::item_t >& args 
    ) {
        if ( args.size() != 1 ) {
            return module_error("missing file path");
        }
        const Json::Value* _ppath = module_unpack_arg(stage, args[0], rpn::IT_STRING);
        if ( _ppath == NULL ) {
            return module_error("file path cannot be null");
        }
        std::ifstream _if(_ppath->asString());
        if ( !_if ) {
            return module_error("cannot open file for reading");
        }
        Json::Reader _jr;
        Json::Value _node;
        try {
            if ( !_jr.parse(_if, _node) ) {
                return module_error(_jr.getFormattedErrorMessages());
            }
        } catch( ... ) {
            return module_error("json file parse failed");
        }
        return module_object(std::move(_node));
    }

    // C'str
    module_manager::module_manager() {
        // nothing
    }

    // Singleton Instance
    module_manager& module_manager::singleton() {
        static module_manager _mm;
        return _mm;
    }
    module_manager::~module_manager() {
        // Nothing
    }

    // Initialize all default modules
    void module_manager::init_default_modules() {
        module_manager::register_module(module_type{
            "max", &module_match_default, &__func_max
        });
        module_manager::register_module(module_type{
            "min", &module_match_default, &__func_min
        });
        module_manager::register_module(module_type{
            "fetch", &module_match_default, &__func_fetch
        });
        module_manager::register_module(module_type{
            "foreach", &module_match_default, &__func_foreach
        });
        module_manager::register_module(module_type{
            "filter", &module_match_default, &__func_filter
        });
        module_manager::register_module(module_type{
            "size", nullptr, &__func_size
        });
        module_manager::register_module(module_type{
            "stdin", &module_match_root, &__func_stdin
        });
        module_manager::register_module(module_type{
            "stdout", &module_match_root, &__func_stdout
        });
        module_manager::register_module(module_type{
            "stderr", &module_match_root, &__func_stderr
        });
        module_manager::register_module(module_type{
            "expr", &module_match_root, &__func_job
        });
        module_manager::register_module(module_type{
            "job", &module_match_root, &__func_job
        });
        module_manager::register_module(module_type{
            "func", &module_match_root, &__func_func
        });
        module_manager::register_module(module_type{
            "after", &module_match_root, &__func_job
        });
        module_manager::register_module(module_type{
            "invoke", &module_match_root, &__func_invoke
        });
        module_manager::register_module(module_type{
            "do", &module_match_job, &__func_do
        });
        module_manager::register_module(module_type{
            "condition", &module_match_root, &__func_condition
        });
        module_manager::register_module(module_type{
            "check", &module_match_array, &__func_check
        });
        module_manager::register_module(module_type{
            "loop", &module_match_root, &__func_loop
        });
        module_manager::register_module(module_type{
            "httpreq", &module_match_root, &__func_httpreq
        });
        module_manager::register_module(module_type{
            "send", std::bind(module_match_key_and_type, 
                std::string("url"), std::string("coas.httpreq"),
                std::placeholders::_1, std::placeholders::_2), 
            &__func_send
        });
        module_manager::register_module(module_type{
            "json", &module_match_string, &__func_json
        });
        module_manager::register_module(module_type{
            "jsonstr", nullptr, &__func_jsonstr
        });
        module_manager::register_module(module_type{
            "sleep", &module_match_root, &__func_sleep
        });
        module_manager::register_module(module_type{
            "toInt", nullptr, &__func_toInt
        });
        module_manager::register_module(module_type{
            "toNumber", nullptr, &__func_toNumber
        });
        module_manager::register_module(module_type{
            "toDouble", nullptr, &__func_toDouble
        });
        module_manager::register_module(module_type{
            "toFloat", nullptr, &__func_toFloat
        });
        module_manager::register_module(module_type{
            "toString", nullptr, &__func_toString
        });
        module_manager::register_module(module_type{
            "toFile", nullptr, &__func_toFile
        });
        module_manager::register_module(module_type{
            "loadFile", &module_match_root, &__func_loadFile
        });
    }

    // Register a new module
    void module_manager::register_module( module_type&& m ) {
        ptr_module_type _pmodule = std::make_shared< module_type >(m);
        auto _mit = singleton().module_map_.find(m.name);
        if ( _mit == singleton().module_map_.end() ) {
            ptr_module_list_type _plist = std::make_shared< module_list_t >();
            _plist->push_back(_pmodule);
            singleton().module_map_[m.name] = _plist;
        } else {
            _mit->second->push_back(_pmodule);
        }
    }

    // Search if a module with name has been registered
    ptr_module_type module_manager::search_module( 
        const std::string& name, 
        const Json::Value& invoker,
        bool is_root
    ) {
        auto _mit = singleton().module_map_.find(name);
        if ( _mit == singleton().module_map_.end() ) return nullptr;
        for ( auto& _pmodule : (*_mit->second) ) {
            if ( _pmodule->is_match == nullptr ) return _pmodule;
            if ( _pmodule->is_match(invoker, is_root) ) return _pmodule;
        }
        return nullptr;
    }

}

// Push Chen
//