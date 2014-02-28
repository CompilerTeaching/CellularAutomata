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
#ifndef CELLATOM_AST_H_INCLUDED
#define CELLATOM_AST_H_INCLUDED
#include <stdint.h>
#include "Pegmatite/pegmatite.hh"

namespace AST
{
	struct StatementList;
}

namespace Interpreter
{
	/**
	 * Class encapsulating the interpreter state.
	 */
	struct State;
	/**
	 * Run the AST interpreter over a grid for one iteration.
	 */
	void runOneStep(int16_t *oldgrid,
	                int16_t *newgrid,
	                int16_t width,
	                int16_t height,
	                AST::StatementList *ast);
}

namespace Compiler
{
	/**
	 * Class encapsulating the compiler state.
	 */
	struct State;
	/**
	 * A function representing a compiled cellular automaton that will run for
	 * a single step.
	 */
	typedef void(*automaton)(int16_t *oldgrid,
	                         int16_t *newgrid,
	                         int16_t width,
	                         int16_t height);
	/**
	 * Compile the AST.  The optimisation level indicates how aggressive
	 * optimisation should be.  Zero indicates no optimisation.
	 */
	automaton compile(AST::StatementList *ast, int optimiseLevel);
}
namespace llvm
{
	class Value;
}

namespace AST
{
	/**
	 * Root class for all statement AST nodes in the Cellular Automata grammar.
	 */
	struct Statement : public pegmatite::ASTContainer
	{
		/**
		 * Interpret this node, updating the given interpreter step and
		 * returning the value (if there is one).
		 */
		virtual uint16_t interpret(Interpreter::State &) = 0;
		/**
		 * Compile this node, returning the LLVM value representing the result,
		 * if there is one.
		 */
		virtual llvm::Value *compile(Compiler::State &) = 0;
	};

	/**
	 * A list of statements that will be executed sequentially.
	 */
	struct StatementList : Statement
	{
		/**
		 * The ordered list of statements.
		 */
		pegmatite::ASTList<Statement> statements;
		uint16_t interpret(Interpreter::State&) override;
		virtual llvm::Value *compile(Compiler::State &) override;
	};

	/**
	 * A (number) literal.  This is a constant value.
	 */
	struct Literal : public Statement
	{
		/**
		 * The value for this literal.
		 */
		uint16_t value;
		virtual uint16_t interpret(Interpreter::State &) override;
		virtual void construct(const pegmatite::InputRange &r,
		                       pegmatite::ASTStack &st) override;
		virtual llvm::Value *compile(Compiler::State &) override;
	};

	/**
	 * Abstract base class for all register types in the AST.
	 */
	struct Register : public Statement
	{
		/**
		 * Returns the value in this register (when interpreting the AST).
		 */
		virtual uint16_t interpret(Interpreter::State &) = 0;
		/**
		 * Assigns a value to the register (when interpreting the AST).
		 */
		virtual void assign(Interpreter::State &, uint16_t) = 0;
		/**
		 * Returns an LLVM value representing an access to this register (when
		 * compiling).
		 */
		virtual llvm::Value *compile(Compiler::State &) = 0;
		/**
		 * Generates code for assigning the specified value to this register
		 * (when compiling).
		 */
		virtual void assign(Compiler::State &, llvm::Value*) = 0;
	};

	/**
	 * The `v` register, which represents the current value in the grid.
	 */
	struct VRegister : public Register
	{
		virtual uint16_t interpret(Interpreter::State &) override;
		virtual void assign(Interpreter::State &, uint16_t) override;
		virtual llvm::Value *compile(Compiler::State &) override;
		virtual void assign(Compiler::State &, llvm::Value*) override;
	};

	/**
	 * A reference to one of the ten local registers, whose values are
	 * initially zero on entry into the cell.
	 */
	struct LocalRegister : public Register
	{
		/**
		 * The number of the local register that this references.
		 */
		int registerNumber;
		virtual void construct(const pegmatite::InputRange &r,
		                       pegmatite::ASTStack &st) override;
		virtual uint16_t interpret(Interpreter::State &) override;
		virtual void assign(Interpreter::State &, uint16_t) override;
		virtual llvm::Value *compile(Compiler::State &) override;
		virtual void assign(Compiler::State &, llvm::Value*) override;
	};

	/**
	 * A reference to one of the ten global registers, whose values are only
	 * reset after updating an entire grid.
	 */
	struct GlobalRegister : public Register
	{
		/**
		 * The number of the global register that this references.
		 */
		int registerNumber;
		virtual void construct(const pegmatite::InputRange &r,
		                       pegmatite::ASTStack &st) override;
		virtual uint16_t interpret(Interpreter::State &) override;
		virtual void assign(Interpreter::State &, uint16_t) override;
		virtual llvm::Value *compile(Compiler::State &) override;
		virtual void assign(Compiler::State &, llvm::Value*) override;
	};
	/**
	 * Value representing the operation to use in an arithmetic / assignment
	 * statement.
	 */
	struct Op: pegmatite::ASTNode
	{
		/**
		 * The type of operation to perform.
		 */
		enum OpKind
		{
			Add,
			Assign,
			Sub,
			Mul,
			Div,
			Min,
			Max
		} op;
		virtual void construct(const pegmatite::InputRange &r,
		                       pegmatite::ASTStack &st) override;
	};
	/**
	 * Arithmetic nodes, for example '+ a0 12' (add the value 12 to register
	 * a0).
	 */
	struct Arithmetic : public Statement
	{
		/**
		 * The operation to perform.
		 */
		pegmatite::ASTPtr<Op>         op;
		/**
		 * The target register.  All operations in the cellatom language are
		 * read-modify-write.
		 */
		pegmatite::ASTPtr<Register>   target;
		/**
		 * The value to assign to the register.
		 */
		pegmatite::ASTPtr<Statement>  value;
		virtual uint16_t interpret(Interpreter::State &) override;
		virtual llvm::Value *compile(Compiler::State &) override;
	};

	/**
	 * A range within a range expression.  This describes the range of values
	 * to match and the code to run within them.
	 */
	struct Range : public pegmatite::ASTContainer
	{
		/**
		 * The start value for expressions of the form (start, end) =>
		 */
		pegmatite::ASTPtr<Literal, /* optional */true> start;
		/**
		 * The end value for values that have a start and end, or the single
		 * value where only one is given.
		 */
		pegmatite::ASTPtr<Literal> end;
		/**
		 * The value to use when if this range is matched.
		 */
		pegmatite::ASTPtr<Statement> value;
	};

	/**
	 * A range expression, for example `[ a0 | (2,3) => 1]` (if the value of
	 * register a0 is 2-3 inclusive, evaluate to 1, otherwise evaluate to 0).
	 */
	struct RangeExpr : public Statement
	{
		/**
		 * The register that is being compared against one or more ranges.
		 */
		pegmatite::ASTPtr<Register> value;
		/**
		 * The ranges in this range map.
		 */
		pegmatite::ASTList<Range>   ranges;
		virtual uint16_t interpret(Interpreter::State &) override;
		virtual llvm::Value *compile(Compiler::State &) override;
	};

	/**
	 * AST node for a neighbours expression.
	 */
	struct Neighbours : Statement
	{
		/**
		 * The statements contained within this neighbours block.
		 */
		pegmatite::ASTPtr<StatementList> statements;
		virtual uint16_t interpret(Interpreter::State &) override;
		virtual llvm::Value *compile(Compiler::State &) override;
	};


}


#endif // CELLATOM_AST_H_INCLUDED
