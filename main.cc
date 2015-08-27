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
#include <iostream>
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>
#include "parser.hh"
#include "ast.hh"

static int enableTiming = 0;

static void logTimeSince(clock_t c1, const char *msg)
{
	if (!enableTiming) { return; }
	clock_t c2 = clock();
	struct rusage r;
	getrusage(RUSAGE_SELF, &r);
	fprintf(stderr, "%s took %f seconds.	Peak used %ldKB.\n", msg,
		((double)c2 - (double)c1) / (double)CLOCKS_PER_SEC, r.ru_maxrss);
}

int main(int argc, char **argv)
{
	int iterations = 1;
	int useJIT = 0;
	int optimiseLevel = 0;
	int gridSize = 5;
	int maxValue = 1;
	clock_t c1;
	int c;
	while ((c = getopt(argc, argv, "ji:tO:x:m:")) != -1)
	{
		switch (c)
		{
			case 'j':
				useJIT = 1;
				break;
			case 'x':
				gridSize = strtol(optarg, 0, 10);
				break;
			case 'm':
				maxValue = strtol(optarg, 0, 10);
				break;
			case 'i':
				iterations = strtol(optarg, 0, 10);
				break;
			case 't':
				enableTiming = 1;
				break;
			case 'o':
				optimiseLevel = strtol(optarg, 0, 10);
		}
	}
	argc -= optind;
	if (argc < 1)
	{
		fprintf(stderr, "usage: %s -jt -i {iterations} -o {optimisation level} -x {grid size} -m {max initial value} {file name}\n", argv[0]);
		return EXIT_FAILURE;
	}
	argv += optind;

	// Do the parsing
	Parser::CellAtomParser p;
	pegmatite::AsciiFileInput input(open(argv[0], O_RDONLY));
	std::unique_ptr<AST::StatementList> ast = 0;
	c1 = clock();
	pegmatite::ErrorReporter err =
		[](const pegmatite::InputRange& r, const std::string& msg) {
		std::cout << "error: " << msg << std::endl;
		std::cout << "line " << r.start.line
		          << ", col " << r.start.col << std::endl;
	};
	if (!p.parse(input, p.g.statements, p.g.ignored, err, ast))
	{
		return EXIT_FAILURE;
	}
	logTimeSince(c1, "Parsing program");
	assert(ast);


#ifdef DUMP_AST
	for (uintptr_t i=0 ; i<result->count ; i++)
	{
		printAST(result->list[i]);
		putchar('\n');
	}
#endif
#ifdef STATIC_TESTING_GRID
	int16_t oldgrid[] = {
		 0,0,0,0,0,
		 0,0,0,0,0,
		 0,1,1,1,0,
		 0,0,0,0,0,
		 0,0,0,0,0
	};
	int16_t newgrid[25];
	gridSize = 5;
	int16_t *g1 = oldgrid;
	int16_t *g2 = newgrid;
#else
	int16_t *g1 = (int16_t*)malloc(sizeof(int16_t) * gridSize * gridSize);
	int16_t *g2 = (int16_t*)malloc(sizeof(int16_t) * gridSize * gridSize);
	//int16_t *g2 = new int16_t[gridSize * gridSize];
	c1 = clock();
	for (int i=0 ; i<(gridSize*gridSize) ; i++)
	{
		g1[i] = random() % (maxValue + 1);
	}
	logTimeSince(c1, "Generating random grid");
#endif
	int i=0;
	if (useJIT)
	{
		c1 = clock();
		Compiler::automaton ca = Compiler::compile(ast.get(), optimiseLevel);
		logTimeSince(c1, "Compiling");
		c1 = clock();
		for (int i=0 ; i<iterations ; i++)
		{
			ca(g1, g2, gridSize, gridSize);
			std::swap(g1, g2);
		}
		logTimeSince(c1, "Running compiled version");
	}
	else
	{
		c1 = clock();
		for (int i=0 ; i<iterations ; i++)
		{
			Interpreter::runOneStep(g1, g2, gridSize, gridSize, ast.get());
			std::swap(g1, g2);
		}
		logTimeSince(c1, "Interpreting");
	}
	for (int x=0 ; x<gridSize ; x++)
	{
		for (int y=0 ; y<gridSize ; y++)
		{
			printf("%d ", g1[i++]);
		}
		putchar('\n');
	}
	return 0;
}
