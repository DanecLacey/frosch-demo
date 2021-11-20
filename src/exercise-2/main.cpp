// Copyright 2021 Alexander Heinlein
// Contact: Alexander Heinlein (a.heinlein@tudelft.nl)

#include "../headers_and_helpers.hpp"

int main (int argc, char *argv[])
{
    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    // Start MPI session
    oblackholestream blackhole;
    GlobalMPISession mpiSession(&argc,&argv,&blackhole);

    RCP<FancyOStream> out = fancyOStream(rcpFromRef(cout));

    // Read input parameters from command line
    CommandLineProcessor clp;
    int dimension = 2; clp.setOption("dim",&dimension,"Dimension: 2 or 3");
    string equation = "laplace"; clp.setOption("eq",&equation,"Type of problem: 'laplace' or 'elasticity'");
    int M = 10; clp.setOption("m",&M,"H/h (default 10)");
    string xmlFile = "parameters-2d.xml"; clp.setOption("xml",&xmlFile,"File name of the parameter list (default ParameterList.xml).");
    bool useEpetra = false; clp.setOption("epetra","tpetra",&useEpetra,"Linear algebra framework: 'epetra' or 'tpetra' (default)");
    int V = 0; clp.setOption("v",&V,"Verbosity Level.\nVERB_DEFAULT=-1, VERB_NONE=0 (default), VERB_LOW=1, VERB_MEDIUM=2, VERB_HIGH=3, VERB_EXTREME=4");
    bool write = false; clp.setOption("write","no-write",&write,"Write VTK files of the partitioned solution: 'write' or 'no-write' (default)");
    bool timers = false; clp.setOption("timers","no-timers",&timers,"Show timer overview: 'timers' or 'no-timers' (default)");
    clp.recogniseAllOptions(true);
    clp.throwExceptions(true);
    CommandLineProcessor::EParseCommandLineReturn parseReturn = clp.parse(argc,argv);
    if (parseReturn == CommandLineProcessor::PARSE_HELP_PRINTED) {
        return(EXIT_SUCCESS);
    }

    // Create communicator
    auto comm = Tpetra::getDefaultComm ();

    // Get the size of the MPI communicator and the local rank
    const size_t myRank = comm->getRank();
    const size_t numProcs = comm->getSize();

    // Initialize stacked timers
    comm->barrier();
    RCP<StackedTimer> stackedTimer = rcp(new StackedTimer("FROSch Example"));
    TimeMonitor::setStackedTimer(stackedTimer);

    // Set verbosity
    const bool verbose = (myRank == 0);
    EVerbosityLevel verbosityLevel = static_cast<EVerbosityLevel>(V);

    // Read parameter list from file
    RCP<ParameterList> parameterList = getParametersFromXmlFile(xmlFile);
    RCP<ParameterList> belosList = sublist(parameterList,"Belos List");
    RCP<ParameterList> precList = sublist(parameterList,"Preconditioner List");

    // Determine the number of subdomains per direction (if numProcs != N^dim stop)
    int N = 0;
    if (dimension == 2) {
        N = pow(comm->getSize(),1/2.) + 100*numeric_limits<double>::epsilon();
        FROSCH_ASSERT(N*N==numProcs,"#MPI ranks != N^2")
    } else if (dimension == 3) {
        N = pow(comm->getSize(),1/3.) + 100*numeric_limits<double>::epsilon();
        FROSCH_ASSERT(N*N*N==numProcs,"#MPI ranks != N^3")
    } else {
        assert(false);
    }

    // Set the linear algebra framework
    UnderlyingLib xpetraLib = UseTpetra;
    if (useEpetra) {
        xpetraLib = UseEpetra;
    } else {
        xpetraLib = UseTpetra;
    }

    if (verbose) cout << endl;
    if (verbose) cout << "###########################" << endl;
    if (verbose) cout << "# Demonstration of FROSch #" << endl;
    if (verbose) cout << "###########################" << endl;
    if (verbose) cout << "Solving " << dimension << "D ";
    if (!equation.compare("laplace")) {
        if (verbose) cout << "Laplace";
    } else {
        if (verbose) cout << "linear elasticity";
    }
    if (verbose) cout << " model problem using ";
    if (useEpetra) {
        if (verbose) cout << "Epetra";
    } else {
        if (verbose) cout << "Tpetra";
    }
    if (verbose) cout << " linear algebra throug Xpetra." << endl;
    if (verbose) cout << "\tNumber of MPI ranks: \t\t\t\t" << numProcs << endl;
    if (verbose) cout << "\tNumber of subdomains per dimension: \t\t" << N << endl;
    if (verbose) cout << "\tNumber of nodes per subdomains and dimension: \t" << M << endl;

    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    if (verbose) cout << endl;
    if (verbose) cout << ">> I. Assemble the system matrix using Galeri\n";
    if (verbose) cout << endl;

    RCP<matrix_type> A;
    RCP<multivector_type> coordinates;
    assembleSystemMatrix(comm,xpetraLib,equation,dimension,N,M,A,coordinates);

    A->describe(*out,verbosityLevel);

    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    if (verbose) cout << endl;
    if (verbose) cout << ">> II. Construct iteration and right hand side vectors\n";
    if (verbose) cout << endl;

    RCP<const map_type> rowMap = A->getRowMap();
    RCP<multivector_type> x = multivectorfactory_type::Build(rowMap,1);
    x->putScalar(0.0);
    x->describe(*out,verbosityLevel);

    RCP<multivector_type> b = multivectorfactory_type::Build(rowMap,1);
    b->putScalar(1.0);
    b->describe(*out,verbosityLevel);

    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    if (verbose) cout << endl;
    if (verbose) cout << ">> III. Construct Schwarz Preconditioner\n";
    if (verbose) cout << endl;

    RCP<operatort_type> belosPrec;
    /*
    ============================================================================
    !! INSERT CODE !!
    ----------------------------------------------------------------------------
    + Create a FROSch::OneLevelPreconditioner prec
    + initialize() prec
    + compute() prec
    + Convert prec into the RCP<OperatorT<multivector_type> > or RCP<operatort_type>, respectively, belosPrec
    ============================================================================
    */

    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    if (verbose) cout << endl;
    if (verbose) cout << ">> IV. Solve the linear equation system using GMRES\n";
    if (verbose) cout << endl;

    RCP<operatort_type> belosA = rcp(new xpetraop_type(A));
    RCP<linear_problem_type> linear_problem (new linear_problem_type(belosA,x,b));
    linear_problem->setProblem(x,b);

    /*
    ============================================================================
    !! INSERT CODE !!
    ----------------------------------------------------------------------------
    + use belosPrec as a right preconditioner for Belos
    ============================================================================
    */

    solverfactory_type solverfactory;
    RCP<solver_type> solver = solverfactory.create(parameterList->get("Belos Solver Type","GMRES"),belosList);
    solver->setProblem(linear_problem);
    solver->solve();
    x->describe(*out,verbosityLevel);

    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    if (verbose) cout << endl;
    if (verbose) cout << ">> V. Test solution\n";
    if (verbose) cout << endl;

    A->apply(*x,*b,Teuchos::NO_TRANS,static_cast<scalar_type> (-1.0),static_cast<scalar_type> (1.0));
    double normRes = b->getVector(0)->norm2();
    if (verbose) cout << "2-Norm of the residual = " << normRes << endl;

    if (write) {
        ////////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////
        if (verbose) cout << endl;
        if (verbose) cout << ">> VI. Write the result\n";
        if (verbose) cout << endl;

        writeVTK(equation,dimension,N,M,coordinates,x);
    }

    if (verbose) cout << "Finished!" << endl;

    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    // Print timers
    comm->barrier();
    stackedTimer->stop("FROSch Example");
    StackedTimer::OutputOptions options;
    options.output_fraction = options.output_minmax = true; //options.output_histogram =
    if (timers) stackedTimer->report(*out,comm,options);

    return 0;
}