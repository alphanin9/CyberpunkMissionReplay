set_project("Mission Replay")
set_version("0.0.0", {build="%y%m%d%H%M"})

set_arch("x64")
set_languages("c++latest")
add_cxxflags("/MP /GR- /EHsc")

-- Make the binary small, optimized and not static linked
-- I don't know if separate debug build is necessarily worth it
set_symbols("debug")
set_strip("all")
set_optimize("faster")
add_cxxflags("/Zi /Ob2 /Oi /GL")
set_runtimes("MD")

add_requires("minhook", "semver")

local cp2077_path = os.getenv("CP2077_PATH")

target("Mission Replay")
    set_default(true)
    set_kind("shared")
    set_filename("MissionReplay.dll")
    set_warnings("more")
    add_files("src/**.cpp", "src/**.rc")
    add_headerfiles("src/**.hpp")
    add_includedirs("src/")
    add_deps("red4ext.sdk", "redlib")
    add_packages("minhook", "semver")
    add_syslinks("Version", "User32")
    add_defines("WINVER=0x0601", "WIN32_LEAN_AND_MEAN", "NOMINMAX")
    set_configdir("src")
    add_configfiles("config/ProjectTemplate.hpp.in", {prefixdir="Config"})
    add_configfiles("config/ProjectMetadata.rc.in", {prefixdir="Config"})
    set_configvar("NAME", "Mission Replay")
    set_configvar("DESC", "Mission Replay for Cyberpunk 2077")
    set_configvar("AUTHOR_NAME", "not_alphanine")
    set_rundir(path.join(cp2077_path, "bin", "x64"))
    on_package(function(target)
        --[[
        os.rm("packaging/*")
        os.rm("packaging_pdb/*")

        os.mkdir("packaging/red4ext/plugins/MissionReplay")
        os.mkdir("packaging/red4ext/plugins/MissionReplay/redscript")
        os.mkdir("packaging/red4ext/plugins/MissionReplay/tweaks")

        os.cp("LICENSE", "packaging/red4ext/plugins/MissionReplay")
        os.cp("THIRDPARTY_LICENSES", "packaging/red4ext/plugins/MissionReplay")

        os.cp("wolvenkit/packed/archive/pc/mod/*", "packaging/red4ext/plugins/MissionReplay")
        os.cp("scripting/*", "packaging/red4ext/plugins/NewGamePlus/redscript")
        os.cp("tweaks/*", "packaging/red4ext/plugins/NewGamePlus/tweaks")
        
        local target_file = target:targetfile()

        os.cp(target_file, "packaging/red4ext/plugins/NewGamePlus")
        os.mkdir("packaging_pdb/red4ext/plugins/NewGamePlus")

        os.cp(path.join(
            path.directory(target_file),
            path.basename(target_file)..".pdb" -- Evil hack
        ), "packaging_pdb/red4ext/plugins/NewGamePlus")
        ]]
    end)
    on_install(function(target)
        local plugin_folder = path.join(cp2077_path, "red4ext/plugins/MissionReplay")

        os.mkdir(plugin_folder)

        local target_file = target:targetfile()

        os.cp(target_file, plugin_folder)
        os.cp(path.join(
            path.directory(target_file),
            path.basename(target_file)..".pdb" -- Evil hack #2
        ), plugin_folder)

        cprint("${bright green}Installed plugin to "..plugin_folder)
    end)
    on_run(function(target)
        os.run(path.join(cp2077_path, "bin", "x64", "Cyberpunk2077.exe"))
    end)

target("red4ext.sdk")
    set_default(false)
    set_kind("headeronly")
    set_group("deps")
    add_headerfiles("deps/red4ext.sdk/include/**.hpp")
    add_includedirs("deps/red4ext.sdk/include/", { public = true })

target("redlib")
    set_default(false)
    set_kind("headeronly")
    set_group("deps")
    add_defines("NOMINMAX")
    add_headerfiles("deps/redlib/vendor/**.hpp")
    add_headerfiles("deps/redlib/include/**.hpp")
    add_includedirs("deps/redlib/vendor/", { public = true })
    add_includedirs("deps/redlib/include/", { public = true })

add_rules("plugin.vsxmake.autoupdate")