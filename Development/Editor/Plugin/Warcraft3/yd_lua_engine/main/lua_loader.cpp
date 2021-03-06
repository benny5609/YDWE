#include "../lua/callback.h"
#include "../lua/helper.h"
#include "../main/lua_loader.h"
#include "../misc/storm.h"
#include "../main/open_lua_engine.h"
#include <base/lua/state.h>
#include <base/warcraft3/jass.h>
#include <base/warcraft3/jass/func_value.h>
#include <base/warcraft3/jass/hook.h>
#include <base/util/singleton.h>
#include <base/util/format.h>
#include <aero/function/fp_call.hpp>

namespace base { namespace warcraft3 { namespace lua_engine { namespace lua_loader {

	int open_libs(lua::state* ls)
	{
		luaL_requiref(ls->self(), "_G",            luaopen_base, 1);      ls->pop(1);
		luaL_requiref(ls->self(), LUA_LOADLIBNAME, luaopen_package, 1);   ls->pop(1);
		luaL_requiref(ls->self(), LUA_COLIBNAME,   luaopen_coroutine, 1); ls->pop(1);
		luaL_requiref(ls->self(), LUA_TABLIBNAME,  luaopen_table, 1);     ls->pop(1);
		luaL_requiref(ls->self(), LUA_STRLIBNAME,  luaopen_string, 1);    ls->pop(1);
		luaL_requiref(ls->self(), LUA_BITLIBNAME,  luaopen_bit32, 1);     ls->pop(1);
		luaL_requiref(ls->self(), LUA_MATHLIBNAME, luaopen_math, 1);      ls->pop(1);

		return 1;
	}

	lua::state* initialize_lua()
	{
		lua::state* ls = (lua::state*)luaL_newstate();
		open_libs(ls);
		clear_searchers_table(ls);
		return ls;
	}

	uintptr_t RealCheat = 0;
	void __cdecl FakeCheat(jass::jstring_t cheat_str)
	{
		const char* cheat = jass::from_string(cheat_str);

		if (!cheat)
		{
			aero::c_call<uint32_t>(RealCheat, cheat_str);
			return ;
		}

		std::string cheat_s = cheat;
		if (cheat_s.compare(0, 4, "run ") == 0)
		{
			if (!instance())
			{
				open_lua_engine(initialize_lua());
			}

			cheat_s = cheat_s.substr(4);

			const char* buffer = nullptr;
			size_t      size   = 0;
			storm& s = util::singleton_nonthreadsafe<storm>::instance();
			if (s.load_file(cheat_s.c_str(), (const void**)&buffer, &size))
			{
				do_buffer(cheat_s.c_str(), buffer, size);
				s.unload_file(buffer);
			}
		}

		aero::c_call<uint32_t>(RealCheat, cheat_str);
	}


	jass::jstring_t __cdecl EXExecuteScript(jass::jstring_t script)
	{
		if (!instance())
		{
			open_lua_engine(initialize_lua());
		}

		lua::state*& ls = instance();

		std::string str_script = util::format("return (%s)", jass::from_trigstring(jass::from_string(script)));
		if (luaL_loadbuffer(ls->self(), str_script.c_str(), str_script.size(), str_script.c_str()) != LUA_OK)
		{
			printf("%s\n", ls->tostring(-1));
			ls->pop(1);
			return 0;
		}

		if (LUA_OK != safe_pcall(ls->self(), 0, 1))
		{
			return 0;
		}

		jass::jstring_t result = 0;
		if (ls->isstring(-1))
		{
			result = jass::create_string(ls->tostring(-1));
		}
		ls->pop(1);
		return result;
	}

	void initialize()
	{
		jass::async_hook("Cheat", (uintptr_t*)&RealCheat, (uintptr_t)FakeCheat);
		jass::japi_add((uintptr_t)EXExecuteScript, "EXExecuteScript", "(S)S");
	}
}}}}
