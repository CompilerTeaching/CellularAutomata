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

#define LLVM_BEFORE(major,minor)\
	((LLVM_MAJOR < major) || ((LLVM_MAJOR == major) && (LLVM_MINOR < minor)))
#define LLVM_AFTER(major,minor)\
	((LLVM_MAJOR > major) || ((LLVM_MAJOR == major) && (LLVM_MINOR > minor)))

#include <llvm/Analysis/Passes.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#if LLVM_BEFORE(3,5)
#include <llvm/Support/system_error.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/Linker.h>
#else
#include <llvm/IR/Verifier.h>
#include <llvm/Linker/Linker.h>
#endif
#include <llvm/PassManager.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>
#include <llvm/Transforms/IPO.h>

#include <iostream>

#include "ast.hh"

using namespace llvm;

namespace Compiler
{
struct State
{
	/** LLVM uses a context object to allow multiple threads */
	LLVMContext &C;
	/** The compilation unit that we are generating */
	Module *Mod;
	/** The function representing the program */
	Function *F;
	/** A helper class for generating instructions */
	IRBuilder<> B;
	/** The 10 local registers in the source language */
	Value *a[10];
	/** The 10 global registers in the source language */
	Value *g[10];
	/** The input grid (passed as an argument) */
	Value *oldGrid;
	/** The output grid (passed as an argument) */
	Value *newGrid;
	/** The width of the grid (passed as an argument) */
	Value *width;
	/** The height of the grid (passed as an argument) */
	Value *height;
	/** The x coordinate of the current cell (passed as an argument) */
	Value *x;
	/** The y coordinate of the current cell (passed as an argument) */
	Value *y;
	/**
	 * The value of the current cell (passed as an argument, returned at the end)
	 */
	Value *v;
	/** The type of our registers (currently i16) */
	Type *regTy;

	/**
	 * Construct the compiler state object.  This loads the runtime.bc support
	 * file and prepares the module, including setting up all of the LLVM state
	 * required.
	 */
	State() : C(getGlobalContext()), B(C)
	{
		// Load the bitcode for the runtime helper code
#if LLVM_BEFORE(3,5)
		OwningPtr<MemoryBuffer> buffer;
		error_code ec = MemoryBuffer::getFile("runtime.bc", buffer);
		if (ec)
		{
			std::cerr << "Failed to open runtime.bc: " << ec.message() << "\n";
			exit(EXIT_FAILURE);
		}
		Mod = ParseBitcodeFile(buffer.get(), C);
#else
		auto buffer = MemoryBuffer::getFile("runtime.bc");
		std::error_code ec;
		if ((ec = buffer.getError()))
		{
			std::cerr << "Failed to open runtime.bc: " << ec.message() << "\n";
			exit(EXIT_FAILURE);
		}
		ErrorOr<Module*> e = parseBitcodeFile(buffer.get().get(), C);
		if ((ec = e.getError()))
		{
			std::cerr << "Failed to parse runtime.bc: " << ec.message() << "\n";
			exit(EXIT_FAILURE);
		}
		Mod = e.get();
#endif
		// Get the stub (prototype) for the cell function
		F = Mod->getFunction("cell");
		// Set it to have private linkage, so that it can be removed after being
		// inlined.
		F->setLinkage(GlobalValue::PrivateLinkage);
		// Add an entry basic block to this function and set it
		BasicBlock *entry = BasicBlock::Create(C, "entry", F);
		B.SetInsertPoint(entry);
		// Cache the type of registers
		regTy = Type::getInt16Ty(C);

		// Collect the function parameters
		auto args = F->arg_begin();
		oldGrid = args++;
		newGrid = args++;
		width = args++;
		height = args++;
		x = args++;
		y = args++;

		// Create space on the stack for the local registers
		for (int i=0 ; i<10 ; i++)
		{
			a[i] = B.CreateAlloca(regTy);
		}
		// Create a space on the stack for the current value.  This can be
		// assigned to, and will be returned at the end.  Store the value passed
		// as a parameter in this.
		v = B.CreateAlloca(regTy);
		B.CreateStore(args++, v);

		// Create a load of pointers to the global registers.
		Value *gArg = args;
		for (int i=0 ; i<10 ; i++)
		{
			B.CreateStore(ConstantInt::get(regTy, 0), a[i]);
			g[i] = B.CreateConstGEP1_32(gArg, i);
		}
	}

	/**
	 * Returns a function pointer for the automaton at the specified
	 * optimisation level.
	 */
	automaton getAutomaton(int optimiseLevel)
	{
		// We've finished generating code, so add a return statement - we're
		// returning the value of the v register.
		B.CreateRet(B.CreateLoad(v));
#ifdef DEBUG_CODEGEN
		// If we're debugging, then print the module in human-readable form to
		// the standard error and verify it.
		Mod->dump();
		verifyModule(*Mod);
#endif
		// Now we need to construct the set of optimisations that we're going to
		// run.
		PassManagerBuilder PMBuilder;
		// Set the optimisation level.  This defines what optimisation passes
		// will be added.
		PMBuilder.OptLevel = optimiseLevel;
		// Create a basic inliner.  This will inline the cell function that we've
		// just created into the automaton function that we're going to create.
		PMBuilder.Inliner = createFunctionInliningPass(275);
		// Now create a function pass manager that is responsible for running
		// passes that optimise functions, and populate it.
		FunctionPassManager *PerFunctionPasses= new FunctionPassManager(Mod);
		PMBuilder.populateFunctionPassManager(*PerFunctionPasses);

		// Run all of the function passes on the functions in our module
		for (auto &I : *Mod)
		{
			if (!I.isDeclaration())
			{
				PerFunctionPasses->run(I);
			}
		}
		// Clean up
		PerFunctionPasses->doFinalization();
		delete PerFunctionPasses;
		// Run the per-module passes
		PassManager *PerModulePasses = new PassManager();
		PMBuilder.populateModulePassManager(*PerModulePasses);
		PerModulePasses->run(*Mod);
		delete PerModulePasses;

		// Now we are ready to generate some code.  First create the execution
		// engine (JIT)
		std::string error;
		ExecutionEngine *EE = ExecutionEngine::create(Mod, false, &error);
		if (!EE)
		{
			fprintf(stderr, "Error: %s\n", error.c_str());
			exit(-1);
		}
		// Now tell it to compile
		return (automaton)EE->getPointerToFunction(Mod->getFunction("automaton"));
	}

};

automaton compile(AST::StatementList *ast, int optimiseLevel)
{
	// These functions do nothing, they just ensure that the correct modules are
	// not removed by the linker.
	InitializeNativeTarget();
	LLVMLinkInJIT();
	
	State s;
	ast->compile(s);
	// And then return the compiled version.
	return s.getAutomaton(optimiseLevel);
}

} // namespace Compiler

namespace AST
{

Value* Literal::compile(Compiler::State &s)
{
	return ConstantInt::get(s.regTy, value);
}
Value* LocalRegister::compile(Compiler::State &s)
{
	assert(registerNumber >= 0 && registerNumber < 10);
	return s.B.CreateLoad(s.a[registerNumber]);
}
void LocalRegister::assign(Compiler::State &s, Value* val)
{
	assert(registerNumber >= 0 && registerNumber < 10);
	s.B.CreateStore(val, s.a[registerNumber]);
}
Value* GlobalRegister::compile(Compiler::State &s)
{
	return s.B.CreateLoad(s.g[registerNumber]);
}
void GlobalRegister::assign(Compiler::State &s, Value* val)
{
	s.B.CreateStore(val, s.a[registerNumber]);
}
Value* VRegister::compile(Compiler::State &s)
{
	return s.B.CreateLoad(s.v);
}
void VRegister::assign(Compiler::State &s, Value* val)
{
	s.B.CreateStore(val, s.v);
}

Value* Arithmetic::compile(Compiler::State &s)
{
	// Keep a reference to the builder so we don't have to type s.B everywhere
	IRBuilder<> &B = s.B;
	Value *v = value->compile(s);
	Value *o = target->compile(s);
	Value *result;
	// For most operations, we can just create a single IR instruction and then
	// store the result.  For min and max, we create a comparison and a select
	switch (op->op)
	{
		case Op::Add:
			result = B.CreateAdd(v, o);
			break;
		case Op::Assign:
			result = v;
			break;
		case Op::Sub:
			result = B.CreateSub(o, v);
			break;
		case Op::Mul:
			result = B.CreateMul(o, v);
			break;
		case Op::Div:
			result = B.CreateSDiv(o, v);
			break;
		case Op::Min:
		{
			Value *gt = B.CreateICmpSGT(o, v);
			result = B.CreateSelect(gt, v, o);
			break;
		}
		case Op::Max:
		{
			Value *gt = B.CreateICmpSGT(o, v);
			result = B.CreateSelect(gt, o, v);
			break;
		}
	}
	target->assign(s, result);
	return target->compile(s);
}

Value* RangeExpr::compile(Compiler::State &s)
{
	// Keep a reference to the builder so we don't have to type s.B everywhere
	IRBuilder<> &B = s.B;
	LLVMContext &C = s.C;
	Function    *F = s.F;
	// Load the register that we're mapping
	Value *reg = value->compile(s);
	// Now create a basic block for continuation.  This is the block that
	// will be reached after the range expression.
	BasicBlock *cont = BasicBlock::Create(s.C, "range_continue", s.F);
	// In this block, create a PHI node that contains the result.
	PHINode *phi = PHINode::Create(s.regTy, ranges.objects().size(),
	                               "range_result", cont);
	// Now loop over all of the possible ranges and create a test for each one
	BasicBlock *current = B.GetInsertBlock();
	for (const auto &re : ranges.objects())
	{
		Value *match;
		// If there is just one range value then we just need an
		// equals-comparison
		if (re->start.get() == nullptr)
		{
			Value *val = re->end->compile(s);
			match = B.CreateICmpEQ(reg, val);
		}
		else
		{
			// Otherwise we need to emit both values and then compare if
			// we're greater-than-or-equal-to the smaller, and
			// less-than-or-equal-to the larger.
			Value *min = re->start->compile(s);
			Value *max = re->end->compile(s);
			match = B.CreateAnd(B.CreateICmpSGE(reg, min),
			                    B.CreateICmpSLE(reg, max));
		}
		// The match value is now a boolean (i1) indicating whether the
		// value matches this range.  Create a pair of basic blocks, one
		// for the case where we did match the specified range, and one for
		// the case where we didn't.
		BasicBlock *expr = BasicBlock::Create(C, "range_result", F);
		BasicBlock *next = BasicBlock::Create(C, "range_next", F);
		// Branch to the correct block
		B.CreateCondBr(match, expr, next);
		// Now construct the block for the case where we matched a value
		B.SetInsertPoint(expr);
		// Compiling the statement may emit some complex code, so we need to
		// leave everything set up for it to (potentially) write lots of
		// instructions and create more basic blocks (imagine nested range
		// expressions).  If this is just a constant, then the next basic block
		// will be empty, but the SimplifyCFG pass will remove it.
		Value *output = re->value->compile(s);
		phi->addIncoming(output, B.GetInsertBlock());
		//phi->addIncoming(re->value->compile(s), B.GetInsertBlock());
		// Now that we've generated the correct value, branch to the
		// continuation block.
		B.CreateBr(cont);
		// ...and repeat
		current = next;
		B.SetInsertPoint(current);
	}
	// If we've fallen off the end, set the default value of zero and branch to
	// the continuation point.
	B.CreateBr(cont);
	phi->addIncoming(ConstantInt::get(s.regTy, 0), B.GetInsertBlock());
	B.SetInsertPoint(cont);
	return phi;
}

Value* Neighbours::compile(Compiler::State &s)
{
	// Grab some local names for things in state
	IRBuilder<> &B = s.B;
	Value *x = s.x;
	Value *y = s.y;
	Value *width = s.width;
	Value *height = s.height;
	Type  *regTy = s.regTy;
	LLVMContext &C = s.C;
	Function    *F = s.F;
	// Some useful constants.
	Value *Zero  = ConstantInt::get(regTy, 0);
	Value *One  = ConstantInt::get(regTy, 1);
	// For each of the (valid) neighbours Start by identifying the bounds
	Value *XMin = B.CreateSub(x, One);
	Value *XMax = B.CreateAdd(x, One);
	Value *YMin = B.CreateSub(y, One);
	Value *YMax = B.CreateAdd(y, One);
	// Now clamp them to the grid
	XMin = B.CreateSelect(B.CreateICmpSLT(XMin, Zero), x, XMin);
	YMin = B.CreateSelect(B.CreateICmpSLT(YMin, Zero), y, YMin);
	XMax = B.CreateSelect(B.CreateICmpSGE(XMax, width), x, XMax);
	YMax = B.CreateSelect(B.CreateICmpSGE(YMax, height), y, YMax);

	// Now create the loops.  We're going to create two nested loops, an outer
	// one for x and an inner one for y.
	BasicBlock *start = B.GetInsertBlock();
	// The entry basic blocks for the outer and inner loops.
	BasicBlock *xLoopStart = BasicBlock::Create(C, "x_loop_start", F);
	BasicBlock *yLoopStart = BasicBlock::Create(C, "y_loop_start", F);
	// Branch to the start of the outer loop and start inserting instructions there
	B.CreateBr(xLoopStart);
	B.SetInsertPoint(xLoopStart);
	// Create the Phi node representing the x index and fill in its value for
	// the initial entry into the loop.  We'll fill in its value for back
	// branches later.
	PHINode *XPhi = B.CreatePHI(regTy, 2);
	XPhi->addIncoming(XMin, start);
	// Branch to the inner loop and set up the y value in the same way.
	B.CreateBr(yLoopStart);
	B.SetInsertPoint(yLoopStart);
	PHINode *YPhi = B.CreatePHI(regTy, 2);
	YPhi->addIncoming(YMin, xLoopStart);

	// Create basic blocks for the end of the inner (y) loop and for the loop body
	BasicBlock *endY = BasicBlock::Create(C, "y_loop_end", F);
	BasicBlock *body = BasicBlock::Create(C, "body", F);

	// If we're in the (x,y) point, we're not in a neighbour, so skip the
	// current loop iteration
	B.CreateCondBr(B.CreateAnd(B.CreateICmpEQ(x, XPhi),
	                           B.CreateICmpEQ(y, YPhi)),
	               endY, body);
	// Now we start emitting the body of the loop
	B.SetInsertPoint(body);

	// Load the value at the current grid point into a0
	Value *idx = B.CreateAdd(YPhi, B.CreateMul(XPhi, width));
	B.CreateStore(B.CreateLoad(B.CreateGEP(s.oldGrid, idx)), s.a[0]);

	// Compile each of the statements inside the loop
	statements->compile(s);
	// Branch to endY.  This is needed if any of the statements have created
	// basic blocks.
	B.CreateBr(endY);
	B.SetInsertPoint(endY);
	BasicBlock *endX = BasicBlock::Create(C, "x_loop_end", F);
	BasicBlock *cont = BasicBlock::Create(C, "continue", F);
	// Increment the loop country for the next iteration
	YPhi->addIncoming(B.CreateAdd(YPhi, ConstantInt::get(regTy, 1)), endY);
	B.CreateCondBr(B.CreateICmpEQ(YPhi, YMax), endX, yLoopStart);

	B.SetInsertPoint(endX);
	XPhi->addIncoming(B.CreateAdd(XPhi, ConstantInt::get(regTy, 1)), endX);
	B.CreateCondBr(B.CreateICmpEQ(XPhi, XMax), cont, xLoopStart);
	B.SetInsertPoint(cont);
	return nullptr;
}

Value* StatementList::compile(Compiler::State& state)
{
	for (auto &s: statements.objects())
	{
		s->compile(state);
	}
	return nullptr;
}

} // namespace AST
