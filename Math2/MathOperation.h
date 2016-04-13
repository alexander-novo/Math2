#pragma once

#include <functional>
#include <vector>

class MathOperation {
	public:
		const static unsigned char TWO_DIG_MULT = 0x01; //On difficulty 1 and 2, only gives a one-digit multiplier. Difficulty 3+ gives two digits
		const static unsigned char REM_OUT = 0x02; //Format output with a remainder
		const static unsigned char SECOND_LTHAN_FIRST = 0x04; //Second operator should be less than the first
	
		static const std::vector<MathOperation*> getOperations() { return MathOperation::operations; }
	
		const unsigned char flag;
		const char* name;
		const unsigned char _operator;

		const std::function<int(int, int)> op;
	private:
		static std::vector<MathOperation*> operations;
	
		MathOperation(char* a, char b, std::function<int(int, int)> c, unsigned char d = 0x00) : name(a), _operator(b), op(c), flag(d) { MathOperation::operations.push_back(this); }

		//STATIC CONSTRUCTOR
		static class _init {
			public:
				_init() {
					new MathOperation("Addition", '+', std::plus<int>());
					new MathOperation("Subtraction", '-', std::minus<int>(), SECOND_LTHAN_FIRST);
					new MathOperation("Multiplication", 'x', std::multiplies<int>(), TWO_DIG_MULT); 
					new MathOperation("Division", 246, std::divides<int>(), TWO_DIG_MULT | REM_OUT | SECOND_LTHAN_FIRST);
				}
		} _initialise;
};