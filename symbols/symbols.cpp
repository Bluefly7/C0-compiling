#include <tokenizer/token.h>
#include "symbols.h"
namespace miniplc0{

    void Symbols::addConstantItem(miniplc0::Symbols::string name, miniplc0::Symbols::string type, int32_t index,
                                  miniplc0::Symbols::string value) {
        Tableitem newitem = Tableitem(name, type, index, value);
        _table.emplace_back(newitem);
    }

    void Symbols::addFunctionItem(miniplc0::Symbols::string name, miniplc0::Symbols::string type, int32_t index,
                                  int32_t params) {
        Tableitem newitem = Tableitem(name, type, index, params);
        _table.emplace_back(newitem);
    }

    bool Symbols::isFunction(const std::string& s) {
        long long unsigned int i;
        for(i=0; i<_table.size(); i++)
        {
            if(_table.at(i).GetName() == s)
                return true;
        }
        return false;
    }

    bool Symbols::functionItemParamsPlus(const std::string& s) {
        long long unsigned int i;
        for(i=0; i<_table.size(); i++)
        {
            if(_table.at(i).GetName() == s)
            {
                _table.at(i).ParamsPlus();
                return true;
            }
        }
        return false;
    }

    Tableitem Symbols::getTableitem(const std::string& s) {
        long long unsigned int i;
        for(i=0; i<_table.size(); i++)
        {
            if(_table.at(i).GetName() == s)
                return _table.at(i);
        }
        return Tableitem("", "", -1, -1);
    }

    void FunctionBody::_add(const Token& tk, std::map<std::string, int32_t>& mp) {
        if (tk.GetType() != TokenType::IDENTIFIER)
            DieAndPrint("only identifier can be added to the table.");
        mp[tk.GetValueString()] = _nextTokenIndex;
        _nextTokenIndex++;
    }

    void FunctionBody::addVariable(const Token& tk) {
        _add(tk, _vars);
    }

    void FunctionBody::addConstant(const Token& tk) {
        _add(tk, _consts);
    }

    void FunctionBody::addUninitializedVariable(const Token& tk) {
        _add(tk, _uninitialized_vars);
    }

    int32_t FunctionBody::getIndex(const std::string& s) {
        if (_uninitialized_vars.find(s) != _uninitialized_vars.end())
            return _uninitialized_vars[s];
        else if (_vars.find(s) != _vars.end())
            return _vars[s];
        else
            return _consts[s];
    }

    bool FunctionBody::isDeclared(const std::string& s) {
        return isConstant(s) || isUninitializedVariable(s) || isInitializedVariable(s);
    }

    bool FunctionBody::isUninitializedVariable(const std::string& s) {
        return _uninitialized_vars.find(s) != _uninitialized_vars.end();
    }
    bool FunctionBody::isInitializedVariable(const std::string&s) {
        return _vars.find(s) != _vars.end();
    }

    bool FunctionBody::isConstant(const std::string&s) {
        return _consts.find(s) != _consts.end();
    }



}