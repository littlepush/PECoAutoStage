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

#include "modules.h"
#include "costage.h"

namespace coas {

    rpn::item_t __func_max( 
        costage& stage, 
        const rpn::item_t& this_path, 
        const std::list< rpn::item_t >& args 
    ) {
        Json::Value* _pthis = stage.get_node(this_path.value);
        if ( _pthis == NULL ) {
            return rpn::item_t{rpn::IT_ERROR, Json::Value("this BAD_ACCESS")};
        }
        Json::Value* _pmax = NULL;
        if ( _pthis->isArray() ) {
            _pmax = &(*_pthis)[0];
            for ( Json::ArrayIndex i = 1; i < _pthis->size(); ++i ) {
                if ( *_pmax < (*_pthis)[i] ) _pmax = &(*_pthis)[i];
            }
        }
        return rpn::item_t{rpn::IT_NUMBER, *_pmax};
    }


    // C'str
    module_manager::module_manager() {
        // Register for max
        ptr_module_list_type _max_list = std::make_shared< module_list_t >();
        ptr_module_type _pmax = std::make_shared< module_type >();
        _pmax->name = "max";
        _pmax->is_match = nullptr;
        _pmax->exec = &__func_max;
        _max_list->push_back(_pmax);
        module_map_[_pmax->name] = _max_list;


    }

    // Singleton Instance
    module_manager& module_manager::singleton() {
        static module_manager _mm;
        return _mm;
    }
    module_manager::~module_manager() {
        // Nothing
    }

    // Register a new module
    void module_manager::register_module( module_type m ) {
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
    ptr_module_type module_manager::search_module( const std::string& name, const rpn::item_t& this_path ) {
        auto _mit = singleton().module_map_.find(name);
        if ( _mit == singleton().module_map_.end() ) return nullptr;
        for ( auto& _pmodule : (*_mit->second) ) {
            if ( _pmodule->is_match == nullptr ) return _pmodule;
            if ( _pmodule->is_match(this_path) ) return _pmodule;
        }
        return nullptr;
    }

}

// Push Chen
//