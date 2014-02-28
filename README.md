CellAtom
========

CellAtom is a language for implementing cellular automata language. It is a
simple example language that has a similar abstract machine model to OpenCL.
It was originally created for a FOSDEM talk and is used as part of the [Modern
Compiler Design](http://www.cl.cam.ac.uk/teaching/1314/L25/) course at the
University of Cambridge.

This language is used for teaching, providing a simple (sequential)
implementation that students can extend.  Possible extensions include:

- Extending the compiler to vectorise the kernels.
- Extending the compiler to execute kernel instances in parallel.
- Implementing a PTX or R600 generation to run on the GPU
- Adding barrier semantics to global registers.

Abstract machine
----------------

Programs written for the cellular automata language are simple kernels that run
on a rectangular grid.  Each kernel has access to 10 local registers (private
to the kernel instance), 10 global registers (shared among all instances on the
current grid), and one special register containing the value at the current
point on the grid.

The language does not provide direct access to the grid: each kernel instance
may access the value of the current node via the `v` register and adjacent
values via the `neighbours` statement.

Kernels are *not* Turing complete.  They intentionally have highly restricted
flow control, to make vectorisation easier.  Note that most of the techniques
that apply to vectorising this language also apply to languages with more
complex flow control, but only after converting to a canonical form and after
excluding parts of the program.  The fact that it is possible to statically
determine the maximum running time for *any* kernel provides a number of
opportunities for optimisation when scheduling parallel executions.

Although individual kernels are not Turing complete, the language as a whole
can implement Conway's Game of Life, which can simulate a universal Turing
Machine.

Kernels update the value in the grid by writing to the `v` register.  This
write is not visible to other kernel instances running on the same input.

In the initial version of the language, there is *no* guaranteed ordering for
writes to global registers, however all writes are expected to be sequentially
consistent.  Students are encouraged to consider how the global registers can
be extended to provide fine-grained synchronisation between kernel instances.

Language syntax
---------------

The language has one (bounded) loop-like construct and one conditional flow
control construct.  It also contains a number of arithmetic assignments, in an
assembly-like syntax.  Integer values are treated as literals and there are
three sets of register values:

	literal  := +<digit>
	val      ::= "v"
	global   ::= "g" <digit>
	local    ::= "l" <digit>
	register ::= <val> <global> <local>

### Arithmetic and assignment

	op         ::= "+" | "-" | "*" | "/" | "min" | "max"
	arithmetic ::= <op> <register> <expression>

All arithmetic operations are in the following form 

	<op> <destination register> <expression>

For example, adding 3 to register `l4` (local register number 4) would be:

	+ l4 3

The valid operations are:

- `+` addition
- `-` subtraction
- `*` multiplication
- `/` division
- `min` minimum (assignment only if the expression is smaller than the current
  value)
- `max` maximum (assignment only if the expression is larger than the current
  value)
- `=` simple assignment

All operations (with the expression of `=`) are read-modify-write operations.
This is intentional, as it makes it easy to extend the definition of operations
on global registers to provide atomicity guarantees, without having to change
the language syntax.

### Neighbour statements

The following syntax describes the only loop-like construct in the language:

	neighbours ( <statement list> )

For each neighbour (of which there are 3 for corners, 5 for edges, and 8 for
central elements in the grid), the statements will be executed once.  The `a0`
register will give the value of the neighbour being inspected.

The following line appears in the CellAtom implementation of Conway's Game of Life:

	neighbours ( + a1 a0 )

This sets register `a1` to the number of 'living' cells (i.e. those with a
value of 1) adjacent to the current cell.


### Range expressions

Range expressions are similar to `switch` statements in other languages.  They
take a register value and a set of ranges.  Range expressions in which none of
the ranges are matched evaluate to zero.  Ranges are evaluated in the order
that they appear, if there is any overlap, otherwise the order is not
observable.

The following example shows nested range expressions from Conway's Game of
Life:

	= v [ v |
		0 => [ a1 | 3 => 1] ,
		1 => [ a1 | (2,3) => 1]
	]

The top-level statement is an assignment, setting the value of register `v`
(the current grid point) based on its old value.  The outer expression provides
cases for if the value is 0 or 1, the only two permitted values for grid nodes
in this program.  The `a1` register contains the number of living neighbours,
so in this example a 'dead' cell with 3 neighbours will be set to 1, as will a
living cell with 2 or 3 neighbours.  All other cases are handled by the default
case and so become 0.

The last expression in this example, `[ a1 | (2,3) => 1]`, shows a range,
rather than a single value, being matched.  Any value of `a1` in the range 2-3
(inclusive) will match here.
