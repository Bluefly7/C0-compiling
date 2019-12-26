#include "argparse.hpp"
#include "fmt/core.h"

#include "tokenizer/tokenizer.h"
#include "analyser/analyser.h"
#include "fmts.hpp"

#include <iostream>
#include <fstream>

// i2,i3,i4的内容，以大端序（big-endian）写入文件
typedef int8_t  i1;
typedef int16_t i2;
typedef int32_t i4;
typedef int64_t i8;

// u2,u3,u4的内容，以大端序（big-endian）写入文件
typedef uint8_t  u1;
typedef uint16_t u2;
typedef uint32_t u4;
typedef uint64_t u8;

u2 transToInt16(u2 x){ return (x >> 8) | (x << 8); }

u4 transToInt32(u4 x){ return ((x & 0x000000FF) << 24) | ((x & 0x0000FF00) << 8) | ((x & 0x00FF0000) >> 8) | ((x & 0xFF000000) >> 24); }

void instructionBinaryOutput(miniplc0::Instruction instruction, std::ostream& output){
    int xLeng = 0;
    int yLeng = 0;
    switch(instruction.GetOperation())
    {
        case miniplc0::BIPUSH:
            xLeng = 1;
            break;
        case miniplc0::LOADC:
        case miniplc0::JMP:
        case miniplc0::JE:
        case miniplc0::JNE:
        case miniplc0::JL:
        case miniplc0::JGE:
        case miniplc0::JG:
        case miniplc0::JLE:
        case miniplc0::CALL:
            xLeng = 2;
            break;
        case miniplc0::IPUSH:
        case miniplc0::POPN:
        case miniplc0::SNEW:
            xLeng = 4;
            break;
        case miniplc0::LOADA:
            xLeng = 2;
            yLeng = 4;
            break;
        default:
            break;
    }
    u1 opcode = (u1)instruction.GetOperation();
    u4 operand1 = (u4)instruction.GetX();
    u4 operand2 = (u4)instruction.GetY();

    output.write((char*)&opcode, sizeof(u1));

    if(xLeng == 0)
        return;
    else if(xLeng == 1)
    {
        u1 op = (u1)operand1;
        output.write((char*)&op, sizeof(u1));
    }
    else if(xLeng == 2 && yLeng == 0)
    {
        u2 op = transToInt16((u2)operand1);
        output.write((char*)&op, sizeof(u2));
    }
    else if(xLeng == 4)
    {
        u4 op = transToInt32((u4)operand1);
        output.write((char*)&op, sizeof(u4));
    }
    else
    {
        u2 op1 = transToInt16((u2)operand1);
        u4 op2 = transToInt32((u4)operand2);
        output.write((char*)&op1, sizeof(u2));
        output.write((char*)&op2, sizeof(u4));
    }
    return ;
}


std::vector<miniplc0::Token> _tokenize(std::istream& input) {
	miniplc0::Tokenizer tkz(input);
	auto p = tkz.AllTokens();
	if (p.second.has_value()) {
		fmt::print(stderr, "Tokenization error: {}\n", p.second.value());
		exit(2);
	}
	return p.first;
}

void Tokenize(std::istream& input, std::ostream& output) {
	auto v = _tokenize(input);
	for (auto& it : v)
		output << fmt::format("{}\n", it);
	return;
}

void Analyse(std::istream& input, std::ostream& output){
	auto tks = _tokenize(input);
	miniplc0::Analyser analyser(tks);
	auto p = analyser.Analyse();
	if (p.second.has_value()) {
		fmt::print(stderr, "Syntactic analysis error: {}\n", p.second.value());
		exit(2);
	}

	miniplc0::Symbols constants = analyser._constants;
	miniplc0::Symbols functions = analyser._functions;
	std::vector<miniplc0::Instruction> start = analyser._start;
    std::vector<miniplc0::FunctionBody> functionbody = analyser._function_body;

    long long unsigned int i,j;

    output << ".constants:" << std::endl;
    for(i=0; i<constants._table.size(); i++)
    {
        output << i << " S " << "\"" << constants._table.at(i).GetName() << "\"" << std::endl;
    }

    output << ".start:" << std::endl;
    for(i=0; i<start.size(); i++)
    {
        output << i << " " << fmt::format("{}", start.at(i)) << std::endl;
    }

    output << ".functions:" << std::endl;
    for(i=0; i<functions._table.size(); i++) {
        output << i << " " << i << " " << functions._table.at(i).GetParams() << " " << "1" << std::endl;
    }

	auto v = p.first;
	for(i=0; i<p.first.size(); i++)
    {
	    auto it = p.first.at(i)._instruction;
	    output << ".F" << i << ":" << std::endl;
	    for(j=0; j<it.size(); j++)
        {
	        output << j << " " << fmt::format("{}", it.at(j)) << std::endl;
        }
    }
	return;
}

void BinaryAnalyse(std::istream& input, std::ostream& output){
    auto tks = _tokenize(input);
    miniplc0::Analyser analyser(tks);
    auto p = analyser.Analyse();
    if (p.second.has_value()) {
        fmt::print(stderr, "Syntactic analysis error: {}\n", p.second.value());
        exit(2);
    }

    miniplc0::Symbols constants = analyser._constants;
    miniplc0::Symbols functions = analyser._functions;
    std::vector<miniplc0::Instruction> start = analyser._start;
    std::vector<miniplc0::FunctionBody> functionbody = analyser._function_body;


    u4 magic = 0x43303a29;
    magic = transToInt32(magic);
    output.write((char*)&magic, sizeof(u4));

    u4 version = 1;
    version = transToInt32(version);
    output.write((char*)&version, sizeof(u4));

    u2 constants_count = (u2)constants._table.size();
    constants_count = transToInt16(constants_count);
    output.write((char*)&constants_count, sizeof(u2));

    long long unsigned int i;
    for(i=0; i<constants._table.size(); i++)
    {
        u1 type = 0;
        output.write((char*)&type, sizeof(u1));
        u2 length = constants._table.at(i).GetName().length();
        length = transToInt16(length);
        output.write((char*)&length, sizeof(u2));
        output << constants._table.at(i).GetName();
    }

    u2 instructions_count = (u2)start.size();
    instructions_count = transToInt16(instructions_count);
    output.write((char*)&instructions_count, sizeof(u2));
    for(i=0; i<start.size(); i++)
        instructionBinaryOutput(start.at(i), output);

    u2 functions_count = (u2)functions._table.size();
    functions_count = transToInt16(functions_count);
    output.write((char*)&functions_count, sizeof(u2));

    for(i=0; i<functionbody.size(); i++)
    {
        u2 name_index = (u2)functions._table.at(i).GetIndex();
        u2 params_size = (u2)functions._table.at(i).GetParams();
        u2 level = (u2)1;
        u2 instructions_count = (u2)functionbody.at(i)._instruction.size();
        name_index = transToInt16(name_index);
        params_size = transToInt16(params_size);
        level = transToInt16(level);
        instructions_count = transToInt16(instructions_count);
        output.write((char*)&name_index, sizeof(u2));
        output.write((char*)&params_size, sizeof(u2));
        output.write((char*)&level, sizeof(u2));
        output.write((char*)&instructions_count, sizeof(u2));
        long long unsigned int j;
        for(j=0; j<functionbody.at(i)._instruction.size(); j++)
            instructionBinaryOutput(functionbody.at(i)._instruction.at(j), output);
    }
}

int main(int argc, char** argv) {
	argparse::ArgumentParser program("cc0");
	program.add_argument("input")
		.help("speicify the file to be compiled.");
	program.add_argument("-t")
		.default_value(false)
		.implicit_value(true)
		.help("perform tokenization for the input file.");
	program.add_argument("-s")
	    .default_value(false)
	    .implicit_value(true)
	    .help("translate c0 source code to the text assembly file.");
	program.add_argument("-c")
        .default_value(false)
        .implicit_value(true)
        .help("translate c0 source code to the binary object file.");
	program.add_argument("-o", "--output")
		.required()
		.default_value(std::string("-"))
		.help("specify the output file.");

	try {
		program.parse_args(argc, argv);
	}
	catch (const std::runtime_error& err) {
		fmt::print(stderr, "{}\n\n", err.what());
		program.print_help();
		exit(2);
	}

	auto input_file = program.get<std::string>("input");
	auto output_file = program.get<std::string>("--output");
	std::istream* input;
	std::ostream* output;
	std::ifstream inf;
	std::ofstream outf;
	if (input_file != "-") {
		inf.open(input_file, std::ios::in);
		if (!inf) {
			fmt::print(stderr, "Fail to open {} for reading.\n", input_file);
			exit(2);
		}
		input = &inf;
	}
	else
		input = &std::cin;

	/*if (output_file != "-") {
		outf.open(output_file, std::ios::out | std::ios::trunc);
		if (!outf) {
			fmt::print(stderr, "Fail to open {} for writing.\n", output_file);
			exit(2);
		}
		output = &outf;
	}
	else
		output = &std::cout;*/


	if (program["-s"] == true && program["-c"] == true) {
		fmt::print(stderr, "You can only translate c0 source code to one file.");
		exit(2);
	}

    if (program["-t"] == true) {
        if (output_file != "-") {
            outf.open(output_file, std::ios::out | std::ios::trunc);
            if (!outf) {
                fmt::print(stderr, "Fail to open {} for writing.\n", output_file);
                exit(2);
            }
            output = &outf;
        }
        else
        {
            outf.open("out", std::ios::out | std::ios:: trunc);
            if(!outf){
                fmt::print(stderr, "Fail to open out for writing.\n", output_file);
                exit(2);
            }
            output = &outf;
        }
		Tokenize(*input, *output);
	}

	else if (program["-s"] == true) {
        if (output_file != "-") {
            outf.open(output_file, std::ios::out | std::ios::trunc);
            if (!outf) {
                fmt::print(stderr, "Fail to open {} for writing.\n", output_file);
                exit(2);
            }
            output = &outf;
        }
        else
        {
            outf.open("out", std::ios::out | std::ios:: trunc);
            if(!outf){
                fmt::print(stderr, "Fail to open out for writing.\n", output_file);
                exit(2);
            }
            output = &outf;
        }
        Analyse(*input, *output);
	}
	else if (program["-c"] == true) {
        if (output_file != "-") {
            outf.open(output_file, std::ios::out | std::ios::trunc | std::ios::binary);
            if (!outf) {
                fmt::print(stderr, "Fail to open {} for writing.\n", output_file);
                exit(2);
            }
            output = &outf;
        }
        else
        {
            outf.open("out", std::ios::out | std::ios:: trunc | std::ios::binary);
            if(!outf){
                fmt::print(stderr, "Fail to open out for writing.\n", output_file);
                exit(2);
            }
            output = &outf;
        }
        //二进制输出
        BinaryAnalyse(*input, *output);
	}
	else {
		fmt::print(stderr, "You must choose tokenization or syntactic analysis.");
		exit(2);
	}
	return 0;
}