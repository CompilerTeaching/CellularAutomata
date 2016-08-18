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
#include <stdio.h>
#include <strings.h>

using namespace AST;

namespace Interpreter {
/**
 * The current state for the interpreter.
 */
struct State
{
  /** The local registers */
  int16_t a[10] = {0};
  /** The global registers */
  int16_t g[10] = {0};
  /** The current cell value */
  int16_t v = 0;
  /** The width of the grid */
  int16_t width = 0;
  /** The height of the grid */
  int16_t height = 0;
  /** The x coordinate of the current cell */
  int16_t x = 0;
  /** The y coordinate of the current cell */
  int16_t y = 0;
  /** The grid itself (non-owning pointer) */
  int16_t *grid = 0;
};
/**
 * Runs the interpreter for a single step over the entire grid.
 */
void runOneStep(int16_t *oldgrid,
                int16_t *newgrid,
                int16_t width,
                int16_t height,
                AST::StatementList *ast)
{
	Interpreter::State state;
	state.grid = oldgrid;
	state.width = width;
	state.height = height;
	int i=0;
	for (int x=0 ; x<width ; x++)
	{
		for (int y=0 ; y<height ; y++,i++)
		{
			state.v = oldgrid[i];
			state.x = x;
			state.y = y;
			bzero(state.a, sizeof(state.a));
			ast->interpret(state);
			newgrid[i] = state.v;
		}
	}
}

}  // namespace Interpreter

uint16_t Literal::interpret(Interpreter::State &s)
{
	return value;
}
uint16_t LocalRegister::interpret(Interpreter::State &s)
{
	assert(registerNumber >= 0 && registerNumber < 10);
	return s.a[registerNumber];
}
void LocalRegister::assign(Interpreter::State &s, uint16_t val)
{
	assert(registerNumber >= 0 && registerNumber < 10);
	s.a[registerNumber] = val;
}
uint16_t GlobalRegister::interpret(Interpreter::State &s)
{
	return s.g[registerNumber];
}
void GlobalRegister::assign(Interpreter::State &s, uint16_t val)
{
	s.g[registerNumber] = val;
}
uint16_t VRegister::interpret(Interpreter::State &s)
{
	return s.v;
}
void VRegister::assign(Interpreter::State &s, uint16_t val)
{
	s.v = val;
}

uint16_t Arithmetic::interpret(Interpreter::State &s)
{
	uint16_t v = value->interpret(s);
	uint16_t o = target->interpret(s);
	uint16_t result;
	switch (op->op)
	{
		case Op::Add:
			result = o+v;
			break;
		case Op::Assign:
			result = v;
			break;
		case Op::Sub:
			result = o-v;
			break;
		case Op::Mul:
			result = o*v;
			break;
		case Op::Div:
			result = o/v;
			break;
		case Op::Min:
			result = std::min(o,v);
			break;
		case Op::Max:
			result = std::max(o,v);
			break;
	}
	target->assign(s, result);
	return 0;
}

uint16_t RangeExpr::interpret(Interpreter::State &s)
{
	uint16_t input = value->interpret(s);
	for (auto &range : ranges.objects())
	{
		uint16_t end = range->end->value;
		if (range->start.get())
		{
			uint16_t start = range->start->value;
			if ((input >= start) && (input <= end))
			{
				return range->value->interpret(s);
			}
		}
		else if (input == end)
		{
			return range->value->interpret(s);
		}
	}
	return 0;
}

uint16_t Neighbours::interpret(Interpreter::State &state)
{
	// For each of the (valid) neighbours
	for (int x = state.x - 1 ; x <= state.x + 1 ; x++)
	{
		if (x < 0 || x >= state.width) { continue; }
		for (int y = state.y - 1 ; y <= state.y + 1 ; y++)
		{
			if (y < 0 || y >= state.height) { continue; }
			if (x == state.x && y == state.y) { continue; }
			// a0 contains the value for the currently visited neighbour
			state.a[0] = state.grid[x*state.height + y];
			// Run all of the statements
			statements->interpret(state);
		}
	}
	return 0;
}

uint16_t StatementList::interpret(Interpreter::State& state)
{
	for (auto &s: statements.objects())
	{
		s->interpret(state);
	}
	return 0;
}
