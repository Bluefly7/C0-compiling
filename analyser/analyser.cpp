#include "analyser.h"

#include <climits>
#include <string>

namespace miniplc0 {
	std::pair<std::vector<FunctionBody>, std::optional<CompilationError>> Analyser::Analyse() {
		auto err = analyseC0Program();
		if (err.has_value())
		    return std::make_pair(std::vector<miniplc0::FunctionBody>(), err);
		else
			return std::make_pair(_function_body, std::optional<CompilationError>());
	}

    //<C0-program> ::= {<variable-declaration>} {<function-definition>}
    std::optional<CompilationError> Analyser::analyseC0Program()
    {
	    //first check
        auto next1 = nextToken();
        if(!next1.has_value())
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNoMain);
        auto next2 = nextToken();
        if(!next2.has_value())
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNoMain);
        auto next3 = nextToken();
        if(!next3.has_value())
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNoMain);
        if(next3.value().GetType() != TokenType::LEFT_BRACKET)
        {
            unreadToken();
            unreadToken();
            unreadToken();
            //{<variable-declaration>}
            while(true)
            {
                auto err = analyseVariableDeclaration();
                if(err.has_value())
                    return err;
                next1 = nextToken();
                if(!next1.has_value())
                    return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNoMain);
                next2 = nextToken();
                if(!next2.has_value())
                    return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNoMain);
                next3 = nextToken();
                if(!next3.has_value())
                    return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNoMain);
                unreadToken();
                unreadToken();
                unreadToken();
                if(next3.value().GetType() == TokenType::LEFT_BRACKET)
                    break;
            }
        } else{
            unreadToken();
            unreadToken();
            unreadToken();
        }

        _stage = true;

        auto next = nextToken();
        if(!next.has_value())
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNoMain);
        unreadToken();
        //{<function-definition>}
        while(true)
        {
            auto err = analyseFunctionDefinition();
            if(err.has_value())
                return err;
            next = nextToken();
            if(!next.has_value())
                break;
            unreadToken();
        }
        if(!_functions.isFunction("main"))
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNoMain);
        return {};
    }

    //<variable-declaration>    ::= ['const']           <type-specifier>        <init-declarator-list> ';'
    //<type-specifier>          ::= 'void'|'int'
    //<init-declarator-list>    ::= <init-declarator>   {',' <init-declarator>}
    //<init-declarator>         ::= <identifier>        [<initializer>]
    //<initializer>             ::= '='                 <expression>
    std::optional<CompilationError> Analyser::analyseVariableDeclaration()
    {
	    int32_t idx;

	    //['const']
	    int32_t isConst = 0;
        auto next = nextToken();
        if(!next.has_value())
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrWhattheFuck);
        if(next.value().GetType() != TokenType::CONST && next.value().GetType() != TokenType::INT)
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrInvalidType);
        if(next.value().GetType() == TokenType::CONST)
        {
            isConst = 1;
            next = nextToken();
            if(!next.has_value())
                return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrInvalidVariableDeclaration);
        }
        //'int'
        if(next.value().GetType() != TokenType::INT)
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrInvalidVariableDeclaration);

        //<init-declarator-list>
        while(true)
        {
            //<identifier>
            next = nextToken();
            if(!next.has_value())
                return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrInvalidVariableDeclaration);
            if(next.value().GetType() != TokenType::IDENTIFIER)
                return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNeedIdentifier);

            //same name check
            if(!_stage && isDeclared(next.value().GetValueString()))
                return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrDuplicateDeclaration);
            else if(_stage && _function_body.at(_function_num).isDeclared(next.value().GetValueString()))
                return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrDuplicateDeclaration);

            auto idtoken = next.value();
            //对局部变量 无论是否后面会赋值 先加入uninitialized
            if(!_stage)
                addGlobalUninitializedVariable(idtoken);
            else
                _function_body.at(_function_num).addUninitializedVariable(idtoken);

            next = nextToken();
            if(!next.has_value())
                return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrInvalidVariableDeclaration);
            //'='
            if(next.value().GetType() == TokenType::EQUAL)
            {
                auto err = analyseExpression();
                if(err.has_value())
                    return err;

                //启动代码阶段
                if(!_stage)
                {
                    idx = getIndex(idtoken.GetValueString());
                    //该标识符的值已经通过表达式存在栈顶了，不需要分配内存
                    if(isConst == 1)
                    {
                        _global_uninitialized_vars.erase(idtoken.GetValueString());
                        _global_consts.insert(std::pair<std::string, int32_t >(idtoken.GetValueString(), idx));
                    }
                    else
                    {
                        _global_uninitialized_vars.erase(idtoken.GetValueString());
                        _global_vars.insert(std::pair<std::string, int32_t>(idtoken.GetValueString(), idx));
                    }
                }
                else
                {
                    idx = _function_body.at(_function_num).getIndex(idtoken.GetValueString());
                    if(isConst == 1)
                    {
                        _function_body.at(_function_num)._uninitialized_vars.erase(idtoken.GetValueString());
                        _function_body.at(_function_num)._consts.insert(std::pair<std::string, int32_t>(idtoken.GetValueString(), idx));
                    }
                    else
                    {
                        _function_body.at(_function_num)._uninitialized_vars.erase(idtoken.GetValueString());
                        _function_body.at(_function_num)._vars.insert(std::pair<std::string, int32_t>(idtoken.GetValueString(), idx));
                    }
                }
                next = nextToken();
                if(!next.has_value())
                    return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrInvalidVariableDeclaration);
                if(next.value().GetType() == TokenType::SEMICOLON)
                    return {};
                else if(next.value().GetType() != TokenType::COMMA)
                    return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrInvalidVariableDeclaration);
            }
            //','
            else if(next.value().GetType() == TokenType::COMMA)
            {
                if(isConst == 1)
                    return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrConstantNeedValue);
                if(!_stage)
                {
                    _start.emplace_back(Operation::SNEW, 1, 0);
                }
                else
                {
                    _function_body.at(_function_num)._instruction.emplace_back(Operation::SNEW, 1, 0);
                }
            }
            //';'
            else if(next.value().GetType() == TokenType::SEMICOLON)
            {
                if(isConst == 1)
                    return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrConstantNeedValue);
                if(!_stage)
                {
                    _start.emplace_back(Operation::SNEW, 1, 0);
                }
                else
                {
                    _function_body.at(_function_num)._instruction.emplace_back(Operation::SNEW, 1, 0);
                }
                return {};
            }
        }
    }

    //<function-definition>         ::= <type-specifier>        <identifier>                    <parameter-clause>      <compound-statement>
    //<parameter-clause>            ::= '('                     [<parameter-declaration-list>]  ')'
    //<type-specifier>              ::= 'void'|'int'
    std::optional<CompilationError> Analyser::analyseFunctionDefinition()
    {
        std::string type;
        auto next = nextToken();
        if(!next.has_value())
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrInvalidFunctionDefinition);
        if(next.value().GetType() != TokenType::VOID && next.value().GetType() != TokenType::INT)
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrInvalidType);
        if(next.value().GetType() == TokenType::VOID)
            type = "VOID";
        else
            type = "INT";

        next = nextToken();
        if(!next.has_value())
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrInvalidFunctionDefinition);
        if(next.value().GetType() != TokenType::IDENTIFIER)
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrInvalidFunctionDefinition);
        std::string funcname = next.value().GetValueString();
        //same name check
        if(isDeclared(funcname))
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrDuplicateDeclaration);
        if(_functions.isFunction(funcname))
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrDuplicateDeclaration);

        int32_t index = _constants._table.size();

        _constants.addConstantItem(funcname, type, index, funcname);
        _functions.addFunctionItem(funcname, type, index, 0);
        if(_functions.getTableitem(funcname).GetIndex() == -1)
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNotDeclared);
        _function_num = _functions.getTableitem(funcname).GetIndex();

        _function_body.emplace_back(FunctionBody());

        next = nextToken();
        if(!next.has_value())
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrInvalidFunctionDefinition);
        if(next.value().GetType() != TokenType::LEFT_BRACKET)
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrInvalidFunctionDefinition);

        next = nextToken();
        if(!next.has_value())
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrInvalidFunctionDefinition);
        if(next.value().GetType() != TokenType::RIGHT_BRACKET)
        {
            unreadToken();
            while(true)
            {
                //参数作为局部变量
                auto err = analyseParameterDeclaration();
                if(err.has_value())
                    return err;

                _functions.functionItemParamsPlus(funcname);

                next = nextToken();
                if(!next.has_value())
                    return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrInvalidFunctionDefinition);
                if(next.value().GetType() == TokenType::RIGHT_BRACKET)
                    break;
                else if(next.value().GetType() != TokenType::COMMA)
                    return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrInvalidParams);
            }
        }

        auto err = analyseCompoundStatement();
        if(err.has_value())
            return err;

        if(type == "VOID")
            _function_body.at(_function_num)._instruction.emplace_back(Operation::RET, 0, 0);
        else{
            _function_body.at(_function_num)._instruction.emplace_back(Operation::IPUSH, 0, 0);
            _function_body.at(_function_num)._instruction.emplace_back(Operation::IRET, 0, 0);
        }

        return {};
    }

    //<parameter-declaration>       ::= ['const']               <type-specifier>                <identifier>
    std::optional<CompilationError> Analyser::analyseParameterDeclaration() {
        int32_t isConst = 0;

	    auto next = nextToken();
        if(!next.has_value())
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrInvalidParams);
        if(next.value().GetType() == TokenType::CONST)
        {
            isConst = 1;
            next = nextToken();
            if(!next.has_value())
                return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrInvalidParams);
        }
        else if(next.value().GetType() != TokenType::INT)
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrInvalidType);

        next = nextToken();
        if(next.value().GetType() != TokenType::IDENTIFIER)
            return std::make_optional<CompilationError>(_current_pos, ErrNeedIdentifier);

        if(_function_body.at(_function_num).isDeclared(next.value().GetValueString()))
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrDuplicateDeclaration);
        if(isConst == 0)
            _function_body.at(_function_num).addVariable(next.value());
        else
            _function_body.at(_function_num).addConstant(next.value());
        return {};
	}

    //<compound-statement> ::= '{' {<variable-declaration>} <statement-seq> '}'
    std::optional<CompilationError> Analyser::analyseCompoundStatement()
    {
        auto next = nextToken();
        if(!next.has_value() || next.value().GetType() != TokenType::LEFT_BRACE)
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteFunction);

        next = nextToken();
        if(!next.has_value())
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteFunction);
        while(next.value().GetType() == TokenType::INT || next.value().GetType() == TokenType::CONST)
        {
            unreadToken();
            auto err = analyseVariableDeclaration();
            if(err.has_value())
                return err;
            next = nextToken();
            if(!next.has_value())
                return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteFunction);
        }

        unreadToken();
        auto err = analyseStatementSeq();
        if(err.has_value())
            return err;

        next = nextToken();
        if(!next.has_value() || next.value().GetType() != TokenType::RIGHT_BRACE)
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteFunction);
        return {};
    }

    //<statement-seq> ::= {<statement>}
    std::optional<CompilationError> Analyser::analyseStatementSeq()
    {
	    auto next = nextToken();
	    if(!next.has_value())
	        return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteStatement);
	    while(next.value().GetType() != TokenType::RIGHT_BRACE)
        {
	        unreadToken();
	        auto err = analyseStatement();
	        if(err.has_value())
	            return err;
	        next = nextToken();
            if(!next.has_value())
                return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteStatement);
        }
	    unreadToken();
	    return {};
	}

    //<statement> ::= '{' <statement-seq> '}'
    //                  |<condition-statement>
    //                  |<loop-statement>
    //                  |<jump-statement>
    //                  |<scan-statement>
    //                  |<print-statement>
    //                  |<assignment-expression>';'
    //                  |<function-call>';'
    //                  |';'
    std::optional<CompilationError> Analyser::analyseStatement()
    {
        auto next = nextToken();
        if(!next.has_value())
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteStatement);
        if(next.value().GetType() == TokenType::LEFT_BRACE)
        {
            auto err = analyseStatementSeq();
            if(err.has_value())
                return err;
            next = nextToken();
            if(!next.has_value() || next.value().GetType() != TokenType::RIGHT_BRACE)
                return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteStatement);
        }
        else if(next.value().GetType() == TokenType::IF)
        {
            unreadToken();
            auto err = analyseConditionStatement();
            if(err.has_value())
                return err;
        }
        else if(next.value().GetType() == TokenType::WHILE)
        {
            unreadToken();
            auto err = analyseLoopStatement();
            if(err.has_value())
                return err;
        }
        else if(next.value().GetType() == TokenType::RETURN)
        {
            unreadToken();
            auto err = analyseJumpStatement();
            if(err.has_value())
                return err;
        }
        else if(next.value().GetType() == TokenType::SCAN)
        {
            unreadToken();
            auto err = analyseScanStatement();
            if(err.has_value())
                return err;
        }
        else if(next.value().GetType() == TokenType::PRINT)
        {
            unreadToken();
            auto err = analysePrintStatement();
            if(err.has_value())
                return err;
        }
        else if(next.value().GetType() == TokenType::IDENTIFIER)
        {
            next = nextToken();
            if(!next.has_value())
                return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteStatement);
            if(next.value().GetType() == TokenType::EQUAL)
            {
                unreadToken();
                unreadToken();
                auto err = analyseAssignmentExpression();
                if(err.has_value())
                    return err;
            }
            else if(next.value().GetType() == TokenType::LEFT_BRACKET)
            {
                unreadToken();
                unreadToken();

                popret = true;

                auto err = analyseFunctionCall();
                if(err.has_value())
                    return err;
            }
            else
                return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteStatement);
            next = nextToken();
            if(!next.has_value() || next.value().GetType() != TokenType::SEMICOLON)
                return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteStatement);
        }
        else if(next.value().GetType() != TokenType::SEMICOLON)
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrWrongToken);

        return {};
    }

    //<condition-statement> ::= 'if'    '('     <condition>     ')'     <statement>     ['else'     <statement>]
    std::optional<CompilationError> Analyser::analyseConditionStatement()
    {
	    int32_t change;
	    int32_t setN;

        auto next = nextToken();
        if(!next.has_value() || next.value().GetType() != TokenType::IF)
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrWhattheFuck);

        next = nextToken();
        if(!next.has_value() || next.value().GetType() != TokenType::LEFT_BRACKET)
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrWrongToken);

        //analyseCondition中存入jcond
        auto err = analyseCondition();
        if(err.has_value())
            return err;

        //size-1为jcond
        change = _function_body.at(_function_num)._instruction.size() - 1;

        next = nextToken();
        if(!next.has_value() || next.value().GetType() != TokenType::RIGHT_BRACKET)
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrWrongToken);

        err = analyseStatement();
        if(err.has_value())
            return err;

        next = nextToken();
        if(!next.has_value() || next.value().GetType() != TokenType::ELSE)
        {
            setN = _function_body.at(_function_num)._instruction.size();
            _function_body.at(_function_num)._instruction.at(change).SetX(setN);

            unreadToken();
            return {};
        }

        setN = _function_body.at(_function_num)._instruction.size();
        _function_body.at(_function_num)._instruction.at(change).SetX(setN+1);

        //ifstatement结束 跳过elsestatement
        change = _function_body.at(_function_num)._instruction.size();
        _function_body.at(_function_num)._instruction.emplace_back(Operation::JMP, 0, 0);

        err = analyseStatement();
        if(err.has_value())
            return err;

        setN = _function_body.at(_function_num)._instruction.size();
        _function_body.at(_function_num)._instruction.at(change).SetX(setN);

        return {};
    }

    //<loop-statement>      ::= 'while'     '('     <condition>     ')'     <statement>
    std::optional<CompilationError> Analyser::analyseLoopStatement()
    {
        int32_t before_con;
        int32_t after_con;
        int32_t setN;

	    auto next = nextToken();
        if(!next.has_value() || next.value().GetType() != TokenType::WHILE)
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrWhattheFuck);

        before_con = _function_body.at(_function_num)._instruction.size();

        next = nextToken();
        if(!next.has_value() || next.value().GetType() != TokenType::LEFT_BRACKET)
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrWrongToken);

        auto err = analyseCondition();
        if(err.has_value())
            return err;

        next = nextToken();
        if(!next.has_value() || next.value().GetType() != TokenType::RIGHT_BRACKET)
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrWrongToken);

        //size-1为jcond
        after_con = _function_body.at(_function_num)._instruction.size() - 1;

        err = analyseStatement();
        if(err.has_value())
            return err;

        //循环体后无条件回到condition前 进行condition判断
        _function_body.at(_function_num)._instruction.emplace_back(Operation::JMP, before_con, 0);

        //回填conditon判断为false后 跳出循环体
        setN = _function_body.at(_function_num)._instruction.size();
        _function_body.at(_function_num)._instruction.at(after_con).SetX(setN);

        return {};
    }

    //<jump-statement>      ::= <return-statement>
    //<return-statement>    ::= 'return'            [<expression>]  ';'
    std::optional<CompilationError> Analyser::analyseJumpStatement()
    {
        auto next = nextToken();
        if(!next.has_value() || next.value().GetType() != TokenType::RETURN)
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrWhattheFuck);

        std::string type = _constants._table.at(_function_num).GetType();

        if(type == "INT")
        {
            auto err = analyseExpression();
            if(err.has_value())
                return err;
            _function_body.at(_function_num)._instruction.emplace_back(Operation::IRET, 0, 0);
        }
        else if(type == "VOID")
            _function_body.at(_function_num)._instruction.emplace_back(Operation::RET, 0, 0);
        else
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrWhattheFuck);

        next = nextToken();
        if(!next.has_value() || next.value().GetType() != TokenType::SEMICOLON)
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrWrongToken);

        return {};
    }

    //<scan-statement>      ::= 'scan'  '('     <identifier>    ')'     ';'
    std::optional<CompilationError> Analyser::analyseScanStatement()
    {
	    int32_t dx;

        auto next = nextToken();
        if(!next.has_value() || next.value().GetType() != TokenType::SCAN)
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrWhattheFuck);

        next = nextToken();
        if(!next.has_value() || next.value().GetType() != TokenType::LEFT_BRACKET)
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrWrongToken);

        next = nextToken();
        if(!next.has_value() || next.value().GetType() != TokenType::IDENTIFIER)
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrWrongToken);

        if(!_function_body.at(_function_num).isDeclared(next.value().GetValueString()) && !isDeclared(next.value().GetValueString()))
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNotDeclared);

        //局部变量
        if(_function_body.at(_function_num).isDeclared(next.value().GetValueString()))
        {
            if(_function_body.at(_function_num).isConstant(next.value().GetValueString()))
                return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrAssignToConstant);

            //保证index不变
            dx = _function_body.at(_function_num).getIndex(next.value().GetValueString());
            //有定义
            if(_function_body.at(_function_num).isUninitializedVariable(next.value().GetValueString()))
            {
                _function_body.at(_function_num)._uninitialized_vars.erase(next.value().GetValueString());
                _function_body.at(_function_num)._vars.insert(std::pair<std::string, int32_t>(next.value().GetValueString(), dx));
            }
            _function_body.at(_function_num)._instruction.emplace_back(Operation::LOADA, 0, dx);
        }
        //全局变量
        else if(isDeclared(next.value().GetValueString()))
        {
            if(isGlobalConstant(next.value().GetValueString()))
                return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrAssignToConstant);

            dx = getIndex(next.value().GetValueString());
            if(isGlobalUninitializedVariable(next.value().GetValueString()))
            {
                _global_uninitialized_vars.erase(next.value().GetValueString());
                _global_vars.insert(std::pair<std::string, int32_t>(next.value().GetValueString(), dx));
            }
            _function_body.at(_function_num)._instruction.emplace_back(Operation::LOADA, 1, dx);
        }
        else
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrWhattheFuck);

        _function_body.at(_function_num)._instruction.emplace_back(Operation::ISCAN, 0, 0);
        _function_body.at(_function_num)._instruction.emplace_back(Operation::ISTORE, 0, 0);

        next = nextToken();
        if(!next.has_value() || next.value().GetType() != TokenType::RIGHT_BRACKET)
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrWrongToken);

        next = nextToken();
        if(!next.has_value() || next.value().GetType() != TokenType::SEMICOLON)
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrWrongToken);
        return {};
    }

    //<print-statement> ::= 'print'         '('     [<printable-list>]      ')'     ';'
    //<printable-list>  ::= <printable>     {','    <printable>}
    //<printable>       ::= <expression>
    std::optional<CompilationError> Analyser::analysePrintStatement()
    {
        auto next = nextToken();
        if(!next.has_value() || next.value().GetType() != TokenType::PRINT)
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrWhattheFuck);

        next = nextToken();
        if(!next.has_value() || next.value().GetType() != TokenType::LEFT_BRACKET)
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrWrongToken);

        auto err = analyseExpression();
        if(err.has_value())
            return err;
        _function_body.at(_function_num)._instruction.emplace_back(Operation::IPRINT, 0, 0);

        while(true)
        {
            next = nextToken();
            if(!next.has_value())
                return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrInvalidPrint);
            if(next.value().GetType() == TokenType::RIGHT_BRACKET)
            {
                _function_body.at(_function_num)._instruction.emplace_back(Operation::PRINTL, 0, 0);
                break;
            }
            else if(next.value().GetType() == TokenType::COMMA)
            {
                _function_body.at(_function_num)._instruction.emplace_back(Operation::BIPUSH, 32, 0);
                _function_body.at(_function_num)._instruction.emplace_back(Operation::CPRINT, 0, 0);
            }
            else
                return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrInvalidPrint);

            err = analyseExpression();
            if(err.has_value())
                return err;
            _function_body.at(_function_num)._instruction.emplace_back(Operation::IPRINT, 0, 0);
        }

        next = nextToken();
        if(!next.has_value() || next.value().GetType() != TokenType::SEMICOLON)
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrWrongToken);
        return {};
    }

    //<assignment-expression>   ::= <identifier>    <assignment-operator>       <expression>
    //<assignment-operator>     ::= '='
    std::optional<CompilationError> Analyser::analyseAssignmentExpression()
    {
	    int32_t dx;

        auto next = nextToken();
        if(!next.has_value() || next.value().GetType() != TokenType::IDENTIFIER)
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrWhattheFuck);

        if(!_function_body.at(_function_num).isDeclared(next.value().GetValueString()) && !isDeclared(next.value().GetValueString()))
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNotDeclared);

        //局部变量
        if(_function_body.at(_function_num).isDeclared(next.value().GetValueString()))
        {
            if(_function_body.at(_function_num).isConstant(next.value().GetValueString()))
                return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrAssignToConstant);

            //保证index不变
            dx = _function_body.at(_function_num).getIndex(next.value().GetValueString());
            //有定义
            if(_function_body.at(_function_num).isUninitializedVariable(next.value().GetValueString()))
            {
                _function_body.at(_function_num)._uninitialized_vars.erase(next.value().GetValueString());
                _function_body.at(_function_num)._vars.insert(std::pair<std::string, int32_t>(next.value().GetValueString(), dx));
            }
            _function_body.at(_function_num)._instruction.emplace_back(Operation::LOADA, 0, dx);
        }
        //全局变量
        else if(isDeclared(next.value().GetValueString()))
        {
            if(isGlobalConstant(next.value().GetValueString()))
                return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrAssignToConstant);

            dx = getIndex(next.value().GetValueString());
            if(isGlobalUninitializedVariable(next.value().GetValueString()))
            {
                _global_uninitialized_vars.erase(next.value().GetValueString());
                _global_vars.insert(std::pair<std::string, int32_t>(next.value().GetValueString(), dx));
            }
            _function_body.at(_function_num)._instruction.emplace_back(Operation::LOADA, 1, dx);
        }
        else
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrWhattheFuck);

        next = nextToken();
        if(!next.has_value() || next.value().GetType() != TokenType::EQUAL)
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrWrongToken);

        auto err = analyseExpression();
        if(err.has_value())
            return err;

        _function_body.at(_function_num)._instruction.emplace_back(Operation::ISTORE, 0, 0);

        return {};
    }

    //<condition>               ::= <expression>    [<relational-operator>  <expression>]
    //<relational-operator>     ::= '<' | '<=' | '>' | '>=' | '!=' | '=='
    std::optional<CompilationError> Analyser::analyseCondition()
    {
        auto err = analyseExpression();
        if(err.has_value())
            return err;

        auto next = nextToken();
        if(!next.has_value() || ( next.value().GetType() != TokenType::LESS
                                && next.value().GetType() != TokenType::LESSEQUAL
                                && next.value().GetType() != TokenType::GREATER
                                && next.value().GetType() != TokenType::GREATEREQUAL
                                && next.value().GetType() != TokenType::EQUALEQUAL
                                && next.value().GetType() != TokenType::NOTEQUAL))
        {
            unreadToken();
            _function_body.at(_function_num)._instruction.emplace_back(Operation::JE, 0, 0);
            return {};
        }

        err = analyseExpression();
        if(err.has_value())
            return err;

        _function_body.at(_function_num)._instruction.emplace_back(Operation::ISUB, 0, 0);
        switch(next.value().GetType())
        {
            case TokenType::LESS:
                _function_body.at(_function_num)._instruction.emplace_back(Operation::JGE, 0, 0);
                break;
            case TokenType::LESSEQUAL:
                _function_body.at(_function_num)._instruction.emplace_back(Operation::JG, 0, 0);
                break;
            case TokenType::GREATER:
                _function_body.at(_function_num)._instruction.emplace_back(Operation:: JLE, 0, 0);
                break;
            case TokenType::GREATEREQUAL:
                _function_body.at(_function_num)._instruction.emplace_back(Operation::JL, 0, 0);
                break;
            case TokenType::EQUALEQUAL:
                _function_body.at(_function_num)._instruction.emplace_back(Operation::JNE, 0, 0);
                break;
            case TokenType::NOTEQUAL:
                _function_body.at(_function_num)._instruction.emplace_back(Operation::JE, 0, 0);
                break;
            default:
                return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrWhattheFuck);
        }
        return {};
    }

    //<expression> ::= <additive-expression>
    std::optional<CompilationError> Analyser::analyseExpression()
    {
        auto err = analyseAdditiveExpression();
        if(err.has_value())
            return err;
        return {};
    }

    //<additive-expression> ::= <multiplicative-expression> {<additive-operator> <multiplicative-expression>}
    //<additive-operator>       ::= '+' | '-'
    std::optional<CompilationError> Analyser::analyseAdditiveExpression()
    {
	    int32_t prefix = 0;
	    //<multiplicative-expression>
        auto err = analyseMultiplicativeExpression();
        if(err.has_value())
            return err;
        auto next = nextToken();
        if(!next.has_value())
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteExpression);
        if(next.value().GetType() != TokenType::PLUS && next.value().GetType() != TokenType::MINUS)
        {
            unreadToken();
            return {};
        }
        while(true)
        {
            //'+'|'-'
            if(next.value().GetType() == TokenType::PLUS)
                prefix = 1;
            else if(next.value().GetType() == TokenType::MINUS)
                prefix = -1;

            //<multiplicative-expression>
            err = analyseMultiplicativeExpression();
            if(err.has_value())
                return err;
            //启动代码
            if(!_stage)
            {
                if(prefix>0)
                    _start.emplace_back(Operation::IADD, 0, 0);
                else if(prefix<0)
                    _start.emplace_back(Operation::ISUB, 0, 0);
                else
                    return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrWhattheFuck);
            }
            //函数体
            else
            {
                if(prefix>0)
                    _function_body.at(_function_num)._instruction.emplace_back(Operation::IADD, 0, 0);
                else if(prefix<0)
                    _function_body.at(_function_num)._instruction.emplace_back(Operation::ISUB, 0, 0);
                else
                    return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrWhattheFuck);
            }

            next = nextToken();
            if(!next.has_value())
                return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteExpression);
            if(next.value().GetType() != TokenType::PLUS && next.value().GetType() != TokenType::MINUS)
            {
                unreadToken();
                return {};
            }
        }
    }

    //<multiplicative-expression> ::= <unary-expression> {<multiplicative-operator> <unary-expression>}
    //<multiplicative-operator> ::= '*' | '/'
    std::optional<CompilationError> Analyser::analyseMultiplicativeExpression()
    {
        int32_t prefix = 0;
        //<unary-expression>
        auto err = analyseUnaryExpression();
        if(err.has_value())
            return err;
        auto next = nextToken();
        if(!next.has_value())
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteExpression);
        if(next.value().GetType() != TokenType::MULTIPLICATION && next.value().GetType() != TokenType::DIVISION)
        {
            unreadToken();
            return {};
        }
        while(true)
        {
            //'*'|'/'
            if(next.value().GetType() == TokenType::MULTIPLICATION)
                prefix = 1;
            else if(next.value().GetType() == TokenType::DIVISION)
                prefix = -1;

            //<unary-expression>
            err = analyseUnaryExpression();
            if(err.has_value())
                return err;
            //启动代码
            if(!_stage)
            {
                if(prefix>0)
                    _start.emplace_back(Operation::IMUL, 0, 0);
                else if(prefix<0)
                    _start.emplace_back(Operation::IDIV, 0, 0);
                else
                    return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrWhattheFuck);
            }
            //函数体
            else
            {
                if(prefix>0)
                    _function_body.at(_function_num)._instruction.emplace_back(Operation::IMUL, 0, 0);
                else if(prefix<0)
                    _function_body.at(_function_num)._instruction.emplace_back(Operation::IDIV, 0, 0);
                else
                    return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrWhattheFuck);
            }

            next = nextToken();
            if(!next.has_value())
                return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteExpression);
            if(next.value().GetType() != TokenType::MULTIPLICATION && next.value().GetType() != TokenType::DIVISION)
            {
                unreadToken();
                return {};
            }
        }

    }

    //<unary-expression> ::= [<unary-operator>] <primary-expression>
    //<unary-operator>          ::= '+' | '-'
    std::optional<CompilationError> Analyser::analyseUnaryExpression()
    {
	    int32_t prefix = 0;
        auto next = nextToken();
        if(!next.has_value())
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteExpression);

        if(next.value().GetType() == TokenType::PLUS)
            prefix = 1;
        else if(next.value().GetType() == TokenType::MINUS)
            prefix = -1;
        else
            unreadToken();

        auto err = analysePrimaryExpression();
        if(err.has_value())
            return err;
        if(!_stage)
        {
            if(prefix<0)
                _start.emplace_back(Operation::INEG, 0, 0);
            else
                return {};
        }
        else
        {
            if(prefix<0)
                _function_body.at(_function_num)._instruction.emplace_back(Operation::INEG, 0, 0);
            else
                return {};
        }
        return {};
    }

    //<primary-expression> ::= '(' <expression> ')'
    //                          |<identifier>
    //                          |<integer-literal>
    //                          |<function-call>
    std::optional<CompilationError> Analyser::analysePrimaryExpression() {
        auto next = nextToken();
        if (!next.has_value())
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteExpression);
        if (next.value().GetType() == TokenType::LEFT_BRACKET) {
            auto err = analyseExpression();
            if (err.has_value())
                return err;
            next = nextToken();
            if (next.value().GetType() == TokenType::RIGHT_BRACKET)
                return {};
            else
                return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrBracketNotMatch);
        } else if (next.value().GetType() == TokenType::DECIMAL_INTEGER) {
            int32_t out = std::stoi(next.value().GetValueString(), 0, 10);
            if (!_stage)
                _start.emplace_back(Operation::IPUSH, out, 0);
            else
                _function_body.at(_function_num)._instruction.emplace_back(Operation::IPUSH, out, 0);
            return {};
        } else if (next.value().GetType() == TokenType::HEXDECIMAL_INTEGER) {
            int32_t out = std::stoi(next.value().GetValueString(), 0, 16);
            if (!_stage)
                _start.emplace_back(Operation::IPUSH, out, 0);
            else
                _function_body.at(_function_num)._instruction.emplace_back(Operation::IPUSH, out, 0);
            return {};
        } else if (next.value().GetType() == TokenType::IDENTIFIER) {
            auto next2 = nextToken();
            if (!next2.has_value())
                return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteExpression);
            if (next2.value().GetType() == TokenType::LEFT_BRACKET) {
                if(!_functions.isFunction(next.value().GetValueString()))
                    return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNeedFunctionIdentifier);
                if(_functions.getTableitem(next.value().GetValueString()).GetType() == "VOID")
                    return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrInvalidType);
                unreadToken();
                unreadToken();

                popret = false;

                auto err = analyseFunctionCall();
                if (err.has_value())
                    return err;
                return {};
            } else {
                unreadToken();
                //启动阶段
                if (!_stage) {
                    if (!isDeclared(next.value().GetValueString()))
                        return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNotDeclared);
                    else if (isGlobalUninitializedVariable(next.value().GetValueString()))
                        return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNotInitialized);
                    //已声明&&已初始化
                    int32_t offset = getIndex(next.value().GetValueString());
                    _start.emplace_back(Operation::LOADA, 0, offset);
                    _start.emplace_back(Operation::ILOAD, 0, 0);
                }
                    //函数阶段
                else {
                    //局部变量是否定义
                    if (!_function_body.at(_function_num).isDeclared(next.value().GetValueString())) {
                        //全局变量是否定义
                        if (!isDeclared(next.value().GetValueString()))
                            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNotDeclared);
                        else if (isGlobalUninitializedVariable(next.value().GetValueString()))
                            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNotInitialized);
                        int32_t offset = getIndex(next.value().GetValueString());
                        _function_body.at(_function_num)._instruction.emplace_back(Operation::LOADA, 1, offset);
                        _function_body.at(_function_num)._instruction.emplace_back(Operation::ILOAD, 0, 0);
                        return {};
                    } else if (_function_body.at(_function_num).isUninitializedVariable(next.value().GetValueString()))
                        return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNotInitialized);
                    int32_t offset = _function_body.at(_function_num).getIndex(next.value().GetValueString());
                    _function_body.at(_function_num)._instruction.emplace_back(Operation::LOADA, 0, offset);
                    _function_body.at(_function_num)._instruction.emplace_back(Operation::ILOAD, 0, 0);
                }
                return {};
            }
        } else{
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrWrongToken);
        }
    }

    //<function-call>       ::= <identifier>    '('     [<expression-list>]     ')'
    //<expression-list>     ::= <expression>    {','    <expression>}
    std::optional<CompilationError> Analyser::analyseFunctionCall()
    {
	    int32_t params = 0;
	    std::string type;

        auto next = nextToken();
        if(!next.has_value())
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrInvalidFunctionCall);
        if(next.value().GetType() != TokenType::IDENTIFIER)
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNeedIdentifier);
        if(_function_body.at(_function_num).isDeclared(next.value().GetValueString()))
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNeedFunctionIdentifier);
        if(!_functions.isFunction(next.value().GetValueString()))
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNeedFunctionIdentifier);

        type = _functions.getTableitem(next.value().GetValueString()).GetType();
        int32_t index = _functions.getTableitem(next.value().GetValueString()).GetIndex();
        if(index == -1)
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNotDeclaredFunction);
        int32_t needparams = _functions.getTableitem(next.value().GetValueString()).GetParams();

        next = nextToken();
        if(!next.has_value())
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrInvalidFunctionCall);
        if(next.value().GetType() != TokenType::LEFT_BRACKET)
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrInvalidFunctionCall);

        auto err = analyseExpression();
        if(err.has_value())
            return err;
        params++;

        while(true)
        {
            next = nextToken();
            if(!next.has_value())
                return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteParams);
            if(next.value().GetType() == TokenType::RIGHT_BRACKET)
                break;
            else if(next.value().GetType() != TokenType::COMMA)
                return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrWrongToken);
            err = analyseExpression();
            if(err.has_value())
                return err;
            params++;
        }

        if(needparams != params)
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteParams);

        _function_body.at(_function_num)._instruction.emplace_back(Operation::CALL, index, 0);

        if(popret && type == "INT")
            _function_body.at(_function_num)._instruction.emplace_back(Operation::POP, 0, 0);

        return {};
    }


	std::optional<Token> Analyser::nextToken() {
		if (_offset == _tokens.size())
			return {};
		// 考虑到 _tokens[0..._offset-1] 已经被分析过了
		// 所以我们选择 _tokens[0..._offset-1] 的 EndPos 作为当前位置
		_current_pos = _tokens[_offset].GetEndPos();
		return _tokens[_offset++];
	}

	void Analyser::unreadToken() {
		if (_offset == 0)
			DieAndPrint("analyser unreads token from the begining.");
		_current_pos = _tokens[_offset - 1].GetEndPos();
		_offset--;
	}

	void Analyser::_add(const Token& tk, std::map<std::string, int32_t>& mp) {
		if (tk.GetType() != TokenType::IDENTIFIER)
			DieAndPrint("only identifier can be added to the table.");
		mp[tk.GetValueString()] = _nextTokenIndex;
		_nextTokenIndex++;
	}

	void Analyser::addGlobalVariable(const Token& tk) {
		_add(tk, _global_vars);
	}

	void Analyser::addGlobalConstant(const Token& tk) {
		_add(tk, _global_consts);
	}

	void Analyser::addGlobalUninitializedVariable(const Token& tk) {
		_add(tk, _global_uninitialized_vars);
	}

	int32_t Analyser::getIndex(const std::string& s) {
		if (_global_uninitialized_vars.find(s) != _global_uninitialized_vars.end())
			return _global_uninitialized_vars[s];
		else if (_global_vars.find(s) != _global_vars.end())
			return _global_vars[s];
		else
			return _global_consts[s];
	}

	bool Analyser::isFunction(const std::string& s) {
        return _functions.isFunction(s);
	}


	bool Analyser::isDeclared(const std::string& s) {
		return isGlobalConstant(s) || isGlobalUninitializedVariable(s) || isGlobalInitializedVariable(s);
	}

	bool Analyser::isGlobalUninitializedVariable(const std::string& s) {
		return _global_uninitialized_vars.find(s) != _global_uninitialized_vars.end();
	}
	bool Analyser::isGlobalInitializedVariable(const std::string&s) {
		return _global_vars.find(s) != _global_vars.end();
	}

	bool Analyser::isGlobalConstant(const std::string&s) {
		return _global_consts.find(s) != _global_consts.end();
	}
}