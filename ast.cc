/*
 * Copyright (c) 2014 David Chisnall
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include "ast.hh"
#include <sstream>


namespace AST
{
void Literal::construct(const pegmatite::InputRange &r,
                        pegmatite::ASTStack &st)
{
	std::stringstream stream;
	for (char c : r)
	{
		stream << c;
	}
	stream >> value;
}
void LocalRegister::construct(const pegmatite::InputRange &r,
                              pegmatite::ASTStack &st)
{
	// Skip the leading l
	registerNumber = *(++r.begin()) - '0';
}
void GlobalRegister::construct(const pegmatite::InputRange &r,
                               pegmatite::ASTStack &st)
{
	// Skip the leading g
	registerNumber = *(++r.begin()) - '0';
}
void Op::construct(const pegmatite::InputRange &r,
                   pegmatite::ASTStack &st)
{
	// If it's a one-character value:
	if (++r.begin() == r.end())
	{
		switch (*r.begin())
		{
			case '=': op = Assign ; break;
			case '+': op = Add ; break;
			case '-': op = Sub ; break;
			case '*': op = Mul ; break;
			case '/': op = Div ; break;
		}
	}
	else
	{
		if (*(++r.begin()) == 'a')
		{
			op = Max;
		}
		else
		{
			op = Min;
		}
	}
}
}  // namespace AST
