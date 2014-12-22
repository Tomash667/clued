#include "Pch.h"
#include "Base.h"
#include "Parse.h"
#include "Run.h"

//=================================================================================================
int main()
{
	ParseOutput out;
	if(parse("tests/7.txt", out))
		try_run(&out.code[0], out.strs, out.funcs);

	return 0;
}
