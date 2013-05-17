solution("CryGame")
	location("../project_files/" .. _ACTION)
	platforms("x86")

	language("C++")

	defines("FORCE_STANDARD_ASSERT")
	defines("NDEBUG")
	defines("GAMEDLL_EXPORTS")
	defines("CE3")
	defines("_XKEYCHECK_H")

	configurations{"Debug", "Release"}

	project("GameDLL")
		location("../project_files/" .. _ACTION)

		flags("FloatFast")
		flags("NoRTTI")
		flags("NoMinimalRebuild")

		buildoptions
		{
			"/Gy",
			"/GS",
			"/GF",
			"/MP",
			"/arch:SSE",
		}

		language("C++")
		kind("SharedLib")

		targetdir("../game/bin32/")
		targetname("CryGame")

		objdir("../BinTemp/")

		includedirs("../code/CryAction/")
		includedirs("../code/CryCommon/")
		includedirs("../code/CryGame/")
		includedirs("../code/SDKs/boost /")
		includedirs("../code/SDKs/STLPORT/")

		files("../code/CryGame/**.cpp")
		files("../code/CryCommon/**.h")
		files("../code/CryAction/**.h")
		files("../code/CryAction/**.cpp")

		configuration("Debug")
			flags("Symbols")

		if _ACTION:find("vs20") then debugcommand([[$(TargetDir)\DedicatedServer.exe]]) end
			
		configuration("Release")
			flags("Optimize")

		if _ACTION:find("vs20") then debugcommand([[$(TargetDir)\DedicatedServer.exe]]) end