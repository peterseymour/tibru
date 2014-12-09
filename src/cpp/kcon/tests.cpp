#include "tests.h"
#include "Allocator.h"
#include "stream.h"
#include "Parser.h"
#include <sstream>

void test_ostream()
{
	Allocator a;
	pcell_t p = new (a) Cell<value_t,pcell_t>{
					0,
					new (a) Cell<pcell_t,value_t>{
						new (a) Cell<value_t,value_t>{3,3},
						2 } };

	std::ostringstream os;
	KConOStream( os ) << "flat = " << p;
	KConOStream( os ) << "deep = " << deep << p;
	assert( os.str() == "flat = [0 [3 3] 2]deep = [0 [[3 3] 2]]", "Incorrect printing found '%s'", os.str().c_str()  );
}

#include <iostream>

void test_parser()
{
	Allocator a;
	std::istringstream iss("[0 [1 [2 3] 4] 5 6]");
	KConOStream( std::cout ) << Parser( a ).parse( iss );
}