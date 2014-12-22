#pragma once

//-------------------------------------------------------------------------------------------------
// function inside script
struct ScriptFunction
{
	int pos, args;
	bool have_result;
};
extern ObjectPool<ScriptFunction> ScriptFunctionPool;
