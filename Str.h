#pragma once

//-------------------------------------------------------------------------------------------------
struct Str;

//-------------------------------------------------------------------------------------------------
// poll of Str*
extern ObjectPool<Str> StrPool;

//-------------------------------------------------------------------------------------------------
// string used in program, have reference counter
struct Str
{
	string s;
	int refs;

	inline void Release()
	{
		--refs;
		if(refs == 0)
			StrPool.Free(this);
	}
};
