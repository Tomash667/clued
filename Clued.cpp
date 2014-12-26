#include "Pch.h"
#include "Base.h"
#include "Parse.h"
#include "Run.h"
#include "Op.h"

//=================================================================================================
void disasm(vector<byte>& code)
{
	byte* c = &code[0];
	byte* end = c + code.size();
	while (c < end)
	{
		const OpInfo& info = op_info[*c];
		Op op = (Op)*c;
		++c;

		switch (info.arg)
		{
		case OpInfo::A_NONE:
			printf("%s\n", info.name);
			break;
		case OpInfo::A_BYTE:
		{
			byte b = *c;
			++c;
			printf("%s %u\n", info.name, (uint)b);
			break;
		}
		case OpInfo::A_SHORT:
		{
			short s = *(short*)c;
			c += 2;
			printf("%s %d\n", info.name, (int)s);
			break;
		}
		case OpInfo::A_INT:
		{
			int i = *(int*)c;
			c += 4;
			printf("%s %d\n", info.name, i);
			break;
		}
		case OpInfo::A_FLOAT:
		{
			float f = *(float*)c;
			c += 4;
			printf("%s %g\n", info.name, f);
			break;
		}
		}
	}
}

//=================================================================================================
int main()
{
	ParseOutput out;
	if (parse("tests/quadric_eq.txt", out))
	{
		//disasm(out.code);
		//printf("-------------------------\n");
		try_run(&out.code[0], out.strs, out.funcs);
	}

	return 0;
}
