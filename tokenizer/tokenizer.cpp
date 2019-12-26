#include "tokenizer/tokenizer.h"

#include <cctype>
#include <sstream>

namespace miniplc0 {

    std::pair<std::optional<Token>, std::optional<CompilationError>> Tokenizer::NextToken() {
        if (!_initialized)
            readAll();
        if (_rdr.bad())
            return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(0, 0, ErrorCode::ErrStreamError));
        if (isEOF())
            return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(0, 0, ErrorCode::ErrEOF));
        auto p = nextToken();
        if (p.second.has_value())
            return std::make_pair(p.first, p.second);
        auto err = checkToken(p.first.value());
        if (err.has_value())
            return std::make_pair(p.first, err.value());
        return std::make_pair(p.first, std::optional<CompilationError>());
    }

    std::pair<std::vector<Token>, std::optional<CompilationError>> Tokenizer::AllTokens() {
        std::vector<Token> result;
        while (true) {
            auto p = NextToken();
            if (p.second.has_value()) {
                if (p.second.value().GetCode() == ErrorCode::ErrEOF)
                    return std::make_pair(result, std::optional<CompilationError>());
                else
                    return std::make_pair(std::vector<Token>(), p.second);
            }
            result.emplace_back(p.first.value());
        }
    }

    // 注意：这里的返回值中 Token 和 CompilationError 只能返回一个，不能同时返回。
    std::pair<std::optional<Token>, std::optional<CompilationError>> Tokenizer::nextToken() {
        // 用于存储已经读到的组成当前token字符
        std::stringstream ss;
        // 分析token的结果，作为此函数的返回值
        std::pair<std::optional<Token>, std::optional<CompilationError>> result;
        // <行号，列号>，表示当前token的第一个字符在源代码中的位置
        std::pair<int64_t, int64_t> pos;
        // 记录当前自动机的状态，进入此函数时是初始状态
        DFAState current_state = DFAState::INITIAL_STATE;
        // 这是一个死循环，除非主动跳出
        // 每一次执行while内的代码，都可能导致状态的变更
        while (true) {
            // 读一个字符，请注意auto推导得出的类型是std::optional<char>
            // 这里其实有两种写法
            // 1. 每次循环前立即读入一个 char
            // 2. 只有在可能会转移的状态读入一个 char
            // 因为我们实现了 unread，为了省事我们选择第一种
            auto current_char = nextChar();

            // 针对当前的状态进行不同的操作
            switch (current_state) {

                // 初始状态
                // 这个 case 我们给出了核心逻辑，但是后面的 case 不用照搬。
                case INITIAL_STATE: {
                    // 已经读到了文件尾
                    if (!current_char.has_value())
                        // 返回一个空的token，和编译错误ErrEOF：遇到了文件尾
                        return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(0, 0, ErrEOF));

                    // 获取读到的字符的值，注意auto推导出的类型是char
                    auto ch = current_char.value();
                    // 标记是否读到了不合法的字符，初始化为否
                    auto invalid = false;

                    // 使用了自己封装的判断字符类型的函数，定义于 tokenizer/utils.hpp
                    // see https://en.cppreference.com/w/cpp/string/byte/isblank
                    if (miniplc0::isspace(ch)) // 读到的字符是空白字符（空格、换行、制表符等）
                        current_state = DFAState::INITIAL_STATE; // 保留当前状态为初始状态，此处直接break也是可以的
                    else if (!miniplc0::isprint(ch)) // control codes and backspace
                        invalid = true;
                    else if (ch == '0') // 读到的字符是'0'
                        current_state = DFAState::ZERO_INTEGER_STATE; //切换到十六进制整数状态
                    else if (miniplc0::isdigit(ch)) // 读到的字符是其他数字
                        current_state = DFAState::DECIMAL_INTEGER_STATE; // 切换到十进制整数的状态
                    else if (miniplc0::isalpha(ch)) // 读到的字符是英文字母
                        current_state = DFAState::IDENTIFIER_STATE; // 切换到标识符的状态
                    else {
                        switch (ch) {
                            case '=':
                                current_state = DFAState::EQUAL_SIGN_STATE;
                                break;
                            case '<':
                                current_state = DFAState::LESS_SIGN_STATE;
                                break;
                            case '>':
                                current_state = DFAState::GREATER_SIGN_STATE;
                                break;
                            case '!':
                                current_state = DFAState::NOTEQUAL_SIGN_STATE;
                                break;
                            case '+':
                                current_state = DFAState::PLUS_SIGN_STATE;
                                break;
                            case '-':
                                current_state = DFAState::MINUS_SIGN_STATE;
                                break;
                            case '*':
                                current_state = DFAState::MULTIPLICATION_SIGN_STATE;
                                break;
                            case '/':
                                current_state = DFAState::DIVISION_SIGN_STATE;
                                break;
                            case ',':
                                current_state = DFAState::COMMA_STATE;
                                break;
                            case '(':
                                current_state = DFAState::LEFTBRACKET_STATE;
                                break;
                            case ')':
                                current_state = DFAState::RIGHTBRACKET_STATE;
                                break;
                            case '{':
                                current_state = DFAState::LEFTBRACE_STATE;
                                break;
                            case '}':
                                current_state = DFAState::RIGHTBRACE_STATE;
                                break;
                            case ';':
                                current_state = DFAState::SEMICOLON_STATE;
                                break;
                            default:
                                invalid = true;
                                break;
                        }
                    }
                    // 如果读到的字符导致了状态的转移，说明它是一个token的第一个字符
                    if (current_state != DFAState::INITIAL_STATE)
                        pos = previousPos(); // 记录该字符的的位置为token的开始位置
                    // 读到了不合法的字符
                    if (invalid) {
                        // 回退这个字符
                        unreadLast();
                        // 返回编译错误：非法的输入
                        pos = currentPos();
                        return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(pos, ErrorCode::ErrInvalidInput));
                    }
                    // 如果读到的字符导致了状态的转移，说明它是一个token的第一个字符
                    if (current_state != DFAState::INITIAL_STATE) // ignore white spaces
                        ss << ch; // 存储读到的字符
                    break;
                }
                case ZERO_INTEGER_STATE: {
                    if(!current_char.has_value())
                        return std::make_pair(std::make_optional<Token>(TokenType::DECIMAL_INTEGER, '0', pos, currentPos()), std::optional<CompilationError>());
                    auto ch = current_char.value();
                    if(ch == 'x'||ch == 'X')
                        current_state = DFAState::HEXADECIMAL_INTEGER_STATE;
                    else if(miniplc0::isdigit(ch))
                        current_state = DFAState::DECIMAL_INTEGER_STATE;
                    else
                    {
                        unreadLast();
                        return std::make_pair(std::make_optional<Token>(TokenType::DECIMAL_INTEGER, '0', pos, currentPos()), std::optional<CompilationError>());
                    }
                    ss << ch;
                    break;
                }
                case HEXADECIMAL_INTEGER_STATE: {
                    std::string str;
                    if (!current_char.has_value())
                    {
                        ss >> str;
                        int len = str.length();
                        if(len == 2)
                            return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(pos,ErrorCode::ErrIncompleteHexdecimal));
                        int i = 2;
                        for(; i<len-1; i++)
                        {
                            if(str.at(i)=='0')
                                continue;
                            else
                                break;
                        }
                        //十六进制数大于8位必定溢出
                        //十六进制数等于8位比较首位ASCII
                        if( len-i>8 || ( len-i==8 && str.at(i)>'7'))
                            return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(pos,ErrorCode::ErrIntegerOverflow));
                        std::string str1 = "0x" + str.substr(i, len-i);
                        return std::make_pair(std::make_optional<Token>(TokenType::HEXDECIMAL_INTEGER, str1, pos, currentPos()), std::optional<CompilationError>());
                    }
                    auto ch = current_char.value();
                    if(miniplc0::isdigit(ch))
                        ss << ch;
                    else if(ch == 'a')
                        ss << 'A';
                    else if(ch == 'b')
                        ss << 'B';
                    else if(ch == 'c')
                        ss << 'C';
                    else if(ch == 'd')
                        ss << 'D';
                    else if(ch == 'e')
                        ss << 'E';
                    else if(ch == 'f')
                        ss << 'F';
                    else if(ch >= 'A' && ch <= 'F')
                        ss << ch;
                    else
                    {
                        unreadLast();
                        ss >> str;
                        int len = str.length();
                        if(len == 2)
                            return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(pos,ErrorCode::ErrIncompleteHexdecimal));
                        int i = 2;
                        for(; i<len-1; i++)
                        {
                            if(str.at(i)=='0')
                                continue;
                            else
                                break;
                        }
                        //十六进制数大于8位必定溢出
                        //十六进制数等于8位比较首位ASCII
                        if( len-i>8 || ( len-i==8 && str.at(i)>'7'))
                            return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(pos,ErrorCode::ErrIntegerOverflow));
                        std::string str1 = "0x" + str.substr(i, len-i);
                        return std::make_pair(std::make_optional<Token>(TokenType::HEXDECIMAL_INTEGER, str1, pos, currentPos()), std::optional<CompilationError>());
                    }
                    break;
                }
                case DECIMAL_INTEGER_STATE: {
                    if (!current_char.has_value())
                    {
                        std::string str;
                        ss >> str;
                        int len = str.length();
                        int i = 0;
                        for(; i<len-1; i++)
                        {
                            if(str.at(i)=='0')
                                continue;
                            else
                                break;
                        }
                        //大于10位的数字必定溢出
                        //等于10位的数字比较ASCII
                        if( len-i>10 || ( len-i==10 && str.substr(i,len-i) > "2147483647"))
                            return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(pos, ErrIntegerOverflow));
                        return std::make_pair(std::make_optional<Token>(TokenType::DECIMAL_INTEGER, str.substr(i,len-i), pos, currentPos()), std::optional<CompilationError>());
                    }
                    auto ch = current_char.value();

                    if(miniplc0::isdigit(ch))
                        ss << ch;
                    else
                    {
                        unreadLast();
                        std::string str;
                        ss >> str;
                        int len = str.length();
                        int i = 0;
                        for(; i<len-1; i++)
                        {
                            if(str.at(i)=='0')
                                continue;
                            else
                                break;
                        }
                        //大于10位的数字必定溢出
                        //等于10位的数字比较ASCII
                        if( len-i>10 || ( len-i==10 && str.substr(i,len-i) > "2147483647"))
                            return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(pos, ErrIntegerOverflow));
                        return std::make_pair(std::make_optional<Token>(TokenType::DECIMAL_INTEGER, str.substr(i,len-i), pos, currentPos()), std::optional<CompilationError>());
                    }
                    break;
                }
                case IDENTIFIER_STATE: {
                    if(!current_char.has_value())
                    {
                        std::string str;
                        ss >> str;
                        if(str == "const")
                            return std::make_pair(std::make_optional<Token>(TokenType::CONST, str, pos, currentPos()), std::optional<CompilationError>());
                        else if(str == "void")
                            return std::make_pair(std::make_optional<Token>(TokenType::VOID, str, pos, currentPos()), std::optional<CompilationError>());
                        else if(str == "int")
                            return std::make_pair(std::make_optional<Token>(TokenType::INT, str, pos, currentPos()), std::optional<CompilationError>());
                        else if(str == "char")
                            return std::make_pair(std::make_optional<Token>(TokenType::CHAR, str, pos, currentPos()), std::optional<CompilationError>());
                        else if(str == "double")
                            return std::make_pair(std::make_optional<Token>(TokenType::DOUBLE, str, pos, currentPos()), std::optional<CompilationError>());
                        else if(str == "struct")
                            return std::make_pair(std::make_optional<Token>(TokenType::STRUCT, str, pos, currentPos()), std::optional<CompilationError>());
                        else if(str == "if")
                            return std::make_pair(std::make_optional<Token>(TokenType::IF, str, pos, currentPos()), std::optional<CompilationError>());
                        else if(str == "else")
                            return std::make_pair(std::make_optional<Token>(TokenType::ELSE, str, pos, currentPos()), std::optional<CompilationError>());
                        else if(str == "switch")
                            return std::make_pair(std::make_optional<Token>(TokenType::SWITCH, str, pos, currentPos()), std::optional<CompilationError>());
                        else if(str == "case")
                            return std::make_pair(std::make_optional<Token>(TokenType::CASE, str, pos, currentPos()), std::optional<CompilationError>());
                        else if(str == "default")
                            return std::make_pair(std::make_optional<Token>(TokenType::DEFAULT, str, pos, currentPos()), std::optional<CompilationError>());
                        else if(str == "while")
                            return std::make_pair(std::make_optional<Token>(TokenType::WHILE, str, pos, currentPos()), std::optional<CompilationError>());
                        else if(str == "for")
                            return std::make_pair(std::make_optional<Token>(TokenType::FOR, str, pos, currentPos()), std::optional<CompilationError>());
                        else if(str == "do")
                            return std::make_pair(std::make_optional<Token>(TokenType::DO, str, pos, currentPos()), std::optional<CompilationError>());
                        else if(str == "return")
                            return std::make_pair(std::make_optional<Token>(TokenType::RETURN, str, pos, currentPos()), std::optional<CompilationError>());
                        else if(str == "break")
                            return std::make_pair(std::make_optional<Token>(TokenType::BREAK, str, pos, currentPos()), std::optional<CompilationError>());
                        else if(str == "continue")
                            return std::make_pair(std::make_optional<Token>(TokenType::CONTINUE, str, pos, currentPos()), std::optional<CompilationError>());
                        else if(str == "print")
                            return std::make_pair(std::make_optional<Token>(TokenType::PRINT, str, pos, currentPos()), std::optional<CompilationError>());
                        else if(str == "scan")
                            return std::make_pair(std::make_optional<Token>(TokenType::SCAN, str, pos, currentPos()), std::optional<CompilationError>());
                        else if(miniplc0::isdigit(str.at(0)))
                            return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(pos, ErrInvalidIdentifier));
                        else
                            return std::make_pair(std::make_optional<Token>(TokenType::IDENTIFIER, str, pos, currentPos()), std::optional<CompilationError>());
                    }
                    auto ch = current_char.value();
                    if(miniplc0::isalpha(ch) || miniplc0::isdigit(ch))
                        ss << ch;
                    else
                    {
                        unreadLast();
                        std::string str;
                        ss >> str;
                        if(str == "const")
                            return std::make_pair(std::make_optional<Token>(TokenType::CONST, str, pos, currentPos()), std::optional<CompilationError>());
                        else if(str == "void")
                            return std::make_pair(std::make_optional<Token>(TokenType::VOID, str, pos, currentPos()), std::optional<CompilationError>());
                        else if(str == "int")
                            return std::make_pair(std::make_optional<Token>(TokenType::INT, str, pos, currentPos()), std::optional<CompilationError>());
                        else if(str == "char")
                            return std::make_pair(std::make_optional<Token>(TokenType::CHAR, str, pos, currentPos()), std::optional<CompilationError>());
                        else if(str == "double")
                            return std::make_pair(std::make_optional<Token>(TokenType::DOUBLE, str, pos, currentPos()), std::optional<CompilationError>());
                        else if(str == "struct")
                            return std::make_pair(std::make_optional<Token>(TokenType::STRUCT, str, pos, currentPos()), std::optional<CompilationError>());
                        else if(str == "if")
                            return std::make_pair(std::make_optional<Token>(TokenType::IF, str, pos, currentPos()), std::optional<CompilationError>());
                        else if(str == "else")
                            return std::make_pair(std::make_optional<Token>(TokenType::ELSE, str, pos, currentPos()), std::optional<CompilationError>());
                        else if(str == "switch")
                            return std::make_pair(std::make_optional<Token>(TokenType::SWITCH, str, pos, currentPos()), std::optional<CompilationError>());
                        else if(str == "case")
                            return std::make_pair(std::make_optional<Token>(TokenType::CASE, str, pos, currentPos()), std::optional<CompilationError>());
                        else if(str == "default")
                            return std::make_pair(std::make_optional<Token>(TokenType::DEFAULT, str, pos, currentPos()), std::optional<CompilationError>());
                        else if(str == "while")
                            return std::make_pair(std::make_optional<Token>(TokenType::WHILE, str, pos, currentPos()), std::optional<CompilationError>());
                        else if(str == "for")
                            return std::make_pair(std::make_optional<Token>(TokenType::FOR, str, pos, currentPos()), std::optional<CompilationError>());
                        else if(str == "do")
                            return std::make_pair(std::make_optional<Token>(TokenType::DO, str, pos, currentPos()), std::optional<CompilationError>());
                        else if(str == "return")
                            return std::make_pair(std::make_optional<Token>(TokenType::RETURN, str, pos, currentPos()), std::optional<CompilationError>());
                        else if(str == "break")
                            return std::make_pair(std::make_optional<Token>(TokenType::BREAK, str, pos, currentPos()), std::optional<CompilationError>());
                        else if(str == "continue")
                            return std::make_pair(std::make_optional<Token>(TokenType::CONTINUE, str, pos, currentPos()), std::optional<CompilationError>());
                        else if(str == "print")
                            return std::make_pair(std::make_optional<Token>(TokenType::PRINT, str, pos, currentPos()), std::optional<CompilationError>());
                        else if(str == "scan")
                            return std::make_pair(std::make_optional<Token>(TokenType::SCAN, str, pos, currentPos()), std::optional<CompilationError>());
                        else if(miniplc0::isdigit(str.at(0)))
                            return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(pos, ErrInvalidIdentifier));
                        else
                            return std::make_pair(std::make_optional<Token>(TokenType::IDENTIFIER, str, pos, currentPos()), std::optional<CompilationError>());
                    }
                    break;
                }
                case PLUS_SIGN_STATE: {
                    unreadLast();
                    return std::make_pair(std::make_optional<Token>(TokenType::PLUS, '+', pos, currentPos()), std::optional<CompilationError>());
                }

                case MINUS_SIGN_STATE: {
                    unreadLast();
                    return std::make_pair(std::make_optional<Token>(TokenType::MINUS, '-', pos, currentPos()), std::optional<CompilationError>());
                }
                case MULTIPLICATION_SIGN_STATE: {
                    unreadLast();
                    return std::make_pair(std::make_optional<Token>(TokenType::MULTIPLICATION, '*', pos, currentPos()), std::optional<CompilationError>());
                }
                case DIVISION_SIGN_STATE: {
                    if(current_char.has_value())
                    {
                        auto ch = current_char.value();
                        if(ch == '/')
                        {
                            current_state = DFAState::SINGLE_COMMENT_STATE;
                            break;
                        }
                        else if(ch == '*')
                        {
                            current_state = DFAState::MULTI_COMMENT_STATE;
                            break;
                        }
                    }
                    unreadLast();
                    return std::make_pair(std::make_optional<Token>(TokenType::DIVISION, '/', pos, currentPos()), std::optional<CompilationError>());
                }
                case EQUAL_SIGN_STATE: {
                    if(current_char.has_value())
                    {
                        auto ch = current_char.value();
                        if(ch == '=')
                        {
                            std::string str = "==";
                            return std::make_pair(std::make_optional<Token>(TokenType::EQUALEQUAL, str, pos, currentPos()), std::optional<CompilationError>());
                        }
                    }
                    unreadLast();
                    return std::make_pair(std::make_optional<Token>(TokenType::EQUAL, '=', pos, currentPos()), std::optional<CompilationError>());
                }
                case LESS_SIGN_STATE: {
                    if(current_char.has_value())
                    {
                        auto ch = current_char.value();
                        if(ch == '=')
                        {
                            std::string str = "<=";
                            return std::make_pair(std::make_optional<Token>(TokenType::LESSEQUAL, str, pos, currentPos()), std::optional<CompilationError>());
                        }
                    }
                    unreadLast();
                    return std::make_pair(std::make_optional<Token>(TokenType::LESS, '<', pos, currentPos()), std::optional<CompilationError>());
                }
                case GREATER_SIGN_STATE: {
                    if(current_char.has_value())
                    {
                        auto ch = current_char.value();
                        if(ch == '=')
                        {
                            std::string str = ">=";
                            return std::make_pair(std::make_optional<Token>(TokenType::GREATEREQUAL, str, pos, currentPos()), std::optional<CompilationError>());
                        }
                    }
                    unreadLast();
                    return std::make_pair(std::make_optional<Token>(TokenType::GREATER, '>', pos, currentPos()), std::optional<CompilationError>());
                }
                case LEFTBRACKET_STATE: {
                    unreadLast();
                    return std::make_pair(std::make_optional<Token>(TokenType::LEFT_BRACKET, '(', pos, currentPos()), std::optional<CompilationError>());
                }
                case RIGHTBRACKET_STATE: {
                    unreadLast();
                    return std::make_pair(std::make_optional<Token>(TokenType::RIGHT_BRACKET, ')', pos, currentPos()), std::optional<CompilationError>());
                }
                case LEFTBRACE_STATE: {
                    unreadLast();
                    return std::make_pair(std::make_optional<Token>(TokenType::LEFT_BRACE, '{', pos, currentPos()), std::optional<CompilationError>());
                }
                case RIGHTBRACE_STATE: {
                    unreadLast();
                    return std::make_pair(std::make_optional<Token>(TokenType::RIGHT_BRACE, '}', pos, currentPos()), std::optional<CompilationError>());
                }
                case SEMICOLON_STATE: {
                    unreadLast();
                    return std::make_pair(std::make_optional<Token>(TokenType::SEMICOLON, ';', pos, currentPos()), std::optional<CompilationError>());
                }
                case COMMA_STATE: {
                    unreadLast();
                    return std::make_pair(std::make_optional<Token>(TokenType::COMMA, ',', pos, currentPos()), std::optional<CompilationError>());
                }
                case SINGLE_COMMENT_STATE: {
                    std::string str;
                    if(!current_char.has_value() || current_char.value() == '\n')
                    {
                        ss.str(str);
                        unreadLast();
                        current_state = DFAState::INITIAL_STATE;
                        break;
                    }
                    else
                        break;
                }
                case MULTI_COMMENT_STATE: {
                    ss.str("");
                    if(!current_char.has_value())
                        return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(currentPos().first, currentPos().second, ErrMultiCommitNotMatch));
                    auto ch = current_char.value();
                    if(ch == '*')
                    {
                        //ss << ch;
                        current_char = nextChar();
                        if(!current_char.has_value())
                            return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(currentPos().first, currentPos().second, ErrMultiCommitNotMatch));
                        if(current_char.value() == '/')
                        {
                            current_state = DFAState::INITIAL_STATE;
                            break;
                        }
                        unreadLast();
                    }
                    break;
                }
                    // 预料之外的状态，如果执行到了这里，说明程序异常
                default:
                    DieAndPrint("unhandled state.");
                    break;
            }
        }
        // 预料之外的状态，如果执行到了这里，说明程序异常
        return std::make_pair(std::optional<Token>(), std::optional<CompilationError>());
    }

    std::optional<CompilationError> Tokenizer::checkToken(const Token& t) {
        switch (t.GetType()) {
            case IDENTIFIER: {
                auto val = t.GetValueString();
                if (miniplc0::isdigit(val[0]))
                    return std::make_optional<CompilationError>(t.GetStartPos().first, t.GetStartPos().second, ErrorCode::ErrInvalidIdentifier);
                break;
            }
            default:
                break;
        }
        return {};
    }

    void Tokenizer::readAll() {
        if (_initialized)
            return;
        for (std::string tp; std::getline(_rdr, tp);)
            _lines_buffer.emplace_back(std::move(tp + "\n"));
        _initialized = true;
        _ptr = std::make_pair<int64_t, int64_t>(0, 0);
        return;
    }

    // Note: We allow this function to return a postion which is out of bound according to the design like std::vector::end().
    std::pair<uint64_t, uint64_t> Tokenizer::nextPos() {
        //行数溢出检查
        if (_ptr.first >= _lines_buffer.size())
            DieAndPrint("advance after EOF");
        //列数溢出检查 如果达到了限制就往下一行放置
        if (_ptr.second == _lines_buffer[_ptr.first].size() - 1)
            return std::make_pair(_ptr.first + 1, 0);
        else
            return std::make_pair(_ptr.first, _ptr.second + 1);
    }

    std::pair<uint64_t, uint64_t> Tokenizer::currentPos() {
        return _ptr;
    }

    std::pair<uint64_t, uint64_t> Tokenizer::previousPos() {
        if (_ptr.first == 0 && _ptr.second == 0)
            DieAndPrint("previous position from beginning");
        if (_ptr.second == 0)
            return std::make_pair(_ptr.first - 1, _lines_buffer[_ptr.first - 1].size() - 1);
        else
            return std::make_pair(_ptr.first, _ptr.second - 1);
    }

    std::optional<char> Tokenizer::nextChar() {
        if (isEOF())
            return {}; // EOF
        auto result = _lines_buffer[_ptr.first][_ptr.second];
        _ptr = nextPos();
        return result;
    }

    bool Tokenizer::isEOF() {
        return _ptr.first >= _lines_buffer.size();
    }

    // Note: Is it evil to unread a buffer?
    void Tokenizer::unreadLast() {
        _ptr = previousPos();
    }
}