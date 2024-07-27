#include <iostream>

int main(int argc, const char **argv)
{
	std::cout << "" << std::endl;
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Save, Desc) \
	std::cout << "---@field " << #ScriptName << " number " << Desc << std::endl;
#define MACRO_CONFIG_FLOAT(Name, ScriptName, Def, Min, Max, Save, Desc) \
	std::cout << "---@field " << #ScriptName << " number " << Desc << std::endl;
#define MACRO_CONFIG_COL(Name, ScriptName, Def, Save, Desc) ;
#define MACRO_CONFIG_STR(Name, ScriptName, Len, Def, Save, Desc) \
	; \
	std::cout << "---@field " << #ScriptName << " string " << Desc << std::endl;
#include <game/server/infclass/ic_config_variables.h>

#undef MACRO_CONFIG_INT
#undef MACRO_CONFIG_FLOAT
#undef MACRO_CONFIG_COL
#undef MACRO_CONFIG_STR

	return 0;
}
