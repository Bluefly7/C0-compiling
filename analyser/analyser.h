#pragma once

#include "error/error.h"
#include "instruction/instruction.h"
#include "tokenizer/token.h"
#include "symbols/symbols.h"

#include <vector>
#include <optional>
#include <utility>
#include <map>
#include <cstdint>
#include <cstddef> // for std::size_t

namespace miniplc0 {

	class Analyser final {
	private:
		using uint64_t = std::uint64_t;
		using int64_t = std::int64_t;
		using uint32_t = std::uint32_t;
		using int32_t = std::int32_t;
		using FunctionBody = miniplc0::FunctionBody;
	public:
		Analyser(std::vector<Token> v)
			: _tokens(std::move(v)), _offset(0), _function_body({}), _current_pos(0, 0),
			_global_uninitialized_vars({}), _global_vars({}), _global_consts({}), _nextTokenIndex(0), _stage(false), _function_num(0) {}
		Analyser(Analyser&&) = delete;
		Analyser(const Analyser&) = delete;
		Analyser& operator=(Analyser) = delete;

		// 唯一接口
		std::pair<std::vector<FunctionBody>, std::optional<CompilationError>> Analyse();

	private:
		// 所有的递归子程序

        std::optional<CompilationError> analyseC0Program();

		std::optional<CompilationError> analyseVariableDeclaration();

        std::optional<CompilationError> analyseFunctionDefinition();

        std::optional<CompilationError> analyseParameterDeclaration();

        std::optional<CompilationError> analyseCompoundStatement();

        std::optional<CompilationError> analyseStatementSeq();

        std::optional<CompilationError> analyseStatement();

        std::optional<CompilationError> analyseConditionStatement();

        std::optional<CompilationError> analyseLoopStatement();

        std::optional<CompilationError> analyseJumpStatement();

        std::optional<CompilationError> analyseScanStatement();

        std::optional<CompilationError> analysePrintStatement();

        std::optional<CompilationError> analyseAssignmentExpression();

        std::optional<CompilationError> analyseCondition();

        std::optional<CompilationError> analyseExpression();

        std::optional<CompilationError> analyseAdditiveExpression();

        std::optional<CompilationError> analyseMultiplicativeExpression();

        std::optional<CompilationError> analyseUnaryExpression();

        std::optional<CompilationError> analysePrimaryExpression();

        std::optional<CompilationError> analyseFunctionCall();


		// Token 缓冲区相关操作

		// 返回下一个 token
		std::optional<Token> nextToken();
		// 回退一个 token
		void unreadToken();

		// 下面是符号表相关操作

		// helper function
		void _add(const Token&, std::map<std::string, int32_t>&);
		// 添加变量、常量、未初始化的变量
		void addGlobalVariable(const Token&);
		void addGlobalConstant(const Token&);
		void addGlobalUninitializedVariable(const Token&);
		// 是否被声明过
		bool isDeclared(const std::string&);
		// 是否是全局的未初始化的变量
		bool isGlobalUninitializedVariable(const std::string&);
		// 是否是全局的已初始化的变量
		bool isGlobalInitializedVariable(const std::string&);
		// 是否是全局的常量
		bool isGlobalConstant(const std::string&);
		// 获得 {变量，常量} 在栈上的偏移
		int32_t getIndex(const std::string&);

		//函数表中查找
		bool isFunction(const std::string&);
	public:
		std::vector<Token> _tokens;
		std::size_t _offset;
        //函数体
        std::vector<FunctionBody> _function_body;
		std::pair<uint64_t, uint64_t> _current_pos;

        std::map<std::string, int32_t> _global_uninitialized_vars;
        std::map<std::string, int32_t> _global_vars;
        std::map<std::string, int32_t> _global_consts;
		// 下一个 token 在栈的偏移
		int32_t _nextTokenIndex;

    public:
	    //启动代码
        std::vector<Instruction> _start;
        //常量表
        Symbols _constants;
        //函数表
        Symbols _functions;

        //启动代码阶段 = False
        //函数体阶段 = True
        bool _stage;

        //在statement中直接function-call时 弹出返回值 True
        //在primaryexpression中function-call时不弹出 False
        bool popret;

        //当前函数在函数体里的下标
        int32_t _function_num;

	};
}