#ifndef CC0_SYMBOLS_H
#define CC0_SYMBOLS_H

#include <string>
#include <vector>
#include <instruction/instruction.h>
#include <map>

namespace miniplc0{

    class FunctionBody{

    private:
        using string = std::string;
        using int32_t = std::int32_t;

    public:
        FunctionBody(): _instruction({}), _uninitialized_vars({}), _vars({}), _consts({}), _nextTokenIndex(0) {}

        //指令集
        std::vector<Instruction> _instruction;
        std::map<std::string, int32_t> _uninitialized_vars;
        std::map<std::string, int32_t> _vars;
        std::map<std::string, int32_t> _consts;

    private:
        int32_t _nextTokenIndex;

    public:

        // helper function
        void _add(const Token&, std::map<std::string, int32_t>&);
        // 添加变量、常量、未初始化的变量
        void addVariable(const Token&);
        void addConstant(const Token&);
        void addUninitializedVariable(const Token&);
        // 是否被声明过
        bool isDeclared(const std::string&);
        // 是否是未初始化的变量
        bool isUninitializedVariable(const std::string&);
        // 是否是已初始化的变量
        bool isInitializedVariable(const std::string&);
        // 是否是常量
        bool isConstant(const std::string&);
        // 获得 {变量，常量} 在栈上的偏移
        int32_t getIndex(const std::string&);

    };

    class Tableitem{

    private:
        using string = std::string;
        using int32_t = std::int32_t;

    public:
        //Constants
        Tableitem(string name, string type, int32_t index, string value): _name(name), _type(type), _index(index), _value(value) {}
        //Functions
        Tableitem(string name, string type, int32_t index, int32_t params): _name(name), _type(type), _index(index), _params(params) {}

    private:
        string _name;
        string _type;
        int32_t _index;
        string _value;
        int32_t _params;

    public:
        string GetName() const { return _name; }
        string GetType() const { return _type; }
        int32_t GetIndex() const { return _index; }
        string GetValue() const { return _value; }
        int32_t GetParams() const { return _params; }
        void ParamsPlus() { _params = _params + 1; }
    };

    class Symbols{

    private:
        using string = std::string;
        using int32_t = std::int32_t;
    public:
        Symbols(): _table({}) {}

        std::vector<Tableitem> _table;

        void addConstantItem(string name, string type, int32_t index, string value);
        void addFunctionItem(string name, string type, int32_t index, int32_t params);
        bool isFunction(const std::string&);
        Tableitem getTableitem(const std::string&);
        bool functionItemParamsPlus(const std::string&);
    };
}


#endif //CC0_SYMBOLS_H
