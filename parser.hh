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
#include "AST.hh"

namespace Parser
{
	using pegmatite::Rule;
	using pegmatite::operator""_E;
	using pegmatite::ExprPtr;
	using pegmatite::BindAST;
	using pegmatite::any;
	using pegmatite::nl;

	/**
	 * Singleton class encapsulating the grammar for the CellAtom language.
	 */
	struct CellAtomGrammar
	{
		/**
		 * Whitespace: spaces, tabs, newlines
		 */
		Rule whitespace  = ' '_E | '\t' | nl('\n');
		/**
		 * Comments, including tracking newlines inside comments via the
		 * whitespace rule.
		 */
		Rule comment     = '"' >>
		                   (*(!ExprPtr('"'_E) >> (nl('\n') | any()))) >>
		                   '"';
		/**
		 * Rule for treating both comments and whitespace as ignored tokens.
		 */
		Rule ignored     = *(comment | whitespace);
		/**
		 * Decimal digits
		 */
		Rule digit       = '0'_E - '9';
		/**
		 * Literal values are one or more digits.
		 */
		Rule literal     = +digit;
		/**
		 * The current-value register `v`.
		 */
		Rule v_reg       = 'v'_E;
		/**
		 * Local registers, `l0` to `l9`.
		 */
		Rule local_reg   = 'a' >> digit;
		/**
		 * Global registers, `g0` to `g9`.
		 */
		Rule global_reg  = 'g' >> digit;
		/**
		 * Any register.
		 */
		Rule reg         = v_reg | local_reg | global_reg;
		/**
		 * Operation keywords.
		 */
		Rule op          = '+'_E | '=' |'-' | '*' | '/' | "min" | "max";
		/**
		 * Arithmetic statements are an operation, a target register, and an
		 * expression.
		 */
		Rule arithmetic  = op >> reg >> expression;
		/**
		 * Neighbours expressions are simple a keyword and then a statement
		 * list in brackets.
		 */
		Rule neighbours  = "neighbours"_E >> '(' >> statements >> ')';
		/**
		 * The range part of a range expression (either a single value or an
		 * inclusive range).
		 */
		Rule range       = literal | ('('_E >> literal >> ',' >> literal >> ')');
		/**
		 * A single entry in a range map: the source range and the target
		 * expression.
		 */
		Rule range_expr  = range >> "=>" >> expression;
		/**
		 * A range map, evaluating to a different expression depending on the
		 * value in the register argument.
		 */
		Rule range_map   = '['_E >> reg >> '|' >>
		                    +range_expr >> *(','_E >> range_expr) >>']';
		/**
		 * Valid expressions in this language are 
		 */
		Rule expression  = literal | reg | range_map;
		/**
		 * Valid statements in this language are the arithmetic statements or
		 * neighbours statements.
		 */
		Rule statement   = neighbours | arithmetic;
		/**
		 * List of zero or more statements.
		 */
		Rule statements  = *statement;
		/**
		 * Returns a singleton instance of this grammar.
		 */
		static const CellAtomGrammar& get()
		{
			static CellAtomGrammar g;
			return g;
		}
		private:
		/**
		 * Private constructor.  This class is immutable, and so only the `get()`
		 * method should be used to return the singleton instance.
		 */
		CellAtomGrammar() {};
	};

	/**
	 * Class representing a parser for the CellAtom language.  This inherits
	 * from the grammar and associates AST nodes with grammar rules.
	 */
	class CellAtomParser : public pegmatite::ASTParserDelegate
	{
#define CONNECT(cls, r) BindAST<AST::cls> r = CellAtomGrammar::get().r
		CONNECT(StatementList, statements);
		CONNECT(Literal, literal);
		CONNECT(Op, op);
		CONNECT(VRegister, v_reg);
		CONNECT(LocalRegister, local_reg);
		CONNECT(GlobalRegister, global_reg);
		CONNECT(Arithmetic, arithmetic);
		CONNECT(Range, range_expr);
		CONNECT(RangeExpr, range_map);
		CONNECT(Neighbours, neighbours);
#undef CONNECT
		public:
		const CellAtomGrammar &g = CellAtomGrammar::get();
	};
}
