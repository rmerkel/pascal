/********************************************************************************************//**
 * @file p.cc
 *
 * The front-end for the P Programming Language compilier and interpeter.
 ************************************************************************************************/

#include "pcomp.h"
#include "pinterp.h"

#include <iostream>
#include <vector>

using namespace std;

static	string	progName;						///< This programs name
static 	string	inputFile {"-"};				///< Source file name, or - for standard input
static 	bool	verbose = false;				///< Verbose messages if true

/********************************************************************************************//** 
 * Print a usage message on standard error output 
 ************************************************************************************************/

static void help() {
	cerr << "Usage: " << progName << ": [options[ [filename]\n"
		 << "Where options is zero or more of the following:\n"
		 << "-?        Print this message and exit.\n"
		 << "-help     Same as -?\n"
		 << "-verbose  Set verbose mode.\n"
		 << "-v        Same as -verbose.\n"
 		 << "-version  Print the program version.\n"
		 << "-V        Same as -version.\n"
		 << "\n"
		 << "filename  The name of the source file, or '-' or '' for standard input.\n";
}

/********************************************************************************************//**
 ************************************************************************************************/
/// Print the version number as major.minor
static void printVersion() {
	cout << progName << ": verson: 0.17\n";		// make sure to update the verison in mainpage!!
}

/********************************************************************************************//** 
 * Parse the command line arguments...
 *
 * @return false if an command line syntax error is encounter, or help requested.
 ************************************************************************************************/
static bool parseCommandline(const vector<string>& args) {
	for (auto arg : args) {
		if (arg.empty())
			continue;							// skip ""

		else if ("-" == arg)
			inputFile = arg;					// read from standard input

		else if ("-help" == arg) {
			help();
			return false;

		} else if ("-verbose" == arg)
			verbose = true;						// annoy the user with lots-o-messages...

		else if ("-version" == arg)
			printVersion();

		else if ('-' == arg[0])	{				// parse -options...
			for (unsigned n = 1; n < arg.size(); ++n)
				switch(arg[n]) {
				case '?':
					help();
					return false;
					break;

				case 'v':
					verbose = true;
					break;

				case 'V':
					printVersion();
					break;

				default:
					cerr << progName << ": unknown command line parameter: -" << arg[n] << "\n";
					return false;
				}

		} else
			inputFile = arg;				// Read from named file
	}

	if (inputFile.empty())
		inputFile = "-";					// Default to standard input
	return true;
}

/********************************************************************************************//** 
 * 'P' compiler and interpreter
 *
 * Usage: p [options] [file]
 *
 * Compiles, and if there are no errors, runs the input program.
 *
 * @return The number of compiler/interpreter errors.
 ************************************************************************************************/
int main(int argc, char* argv[]) {
	progName = argv[0];

	PComp		comp{progName};					// The compiler...
	PInterp 	machine;						// The machine...
	InstrVector	code;							// Machine instructions...
	DatumVector	consts;							// Global constants (temporaries)
	unsigned 	nErrors = 0;

	vector<string> args;						// Parse the command line arguments...
	for (int argn = 1; argn < argc; ++argn)
		args.push_back(argv[argn]);

	if (!parseCommandline(args))
		++nErrors;
												// Compile the source, run if no errors
	else if (0 == (nErrors = comp(inputFile, code, consts, verbose))) {
		if (verbose) {
			if (inputFile == "-")
				cout << progName << ": loading program from standard input, and starting P...\n";
			else
				cout << progName << ": loading program '" << inputFile << "', and starting P...\n";
		}

		const PInterp::Result r = machine(code, consts, verbose);
		if (PInterp::success != r)
			cerr << progName << ": runtime error: " << r << "!\n";

		if (verbose) cout << progName << ": Ending P after " << machine.cycles() << " machine cycles\n";
	}

	return nErrors;
}
