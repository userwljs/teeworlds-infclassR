import os

from conan import ConanFile
from conan.api.conan_api import ConanAPI
from conan.tools.build import check_min_cppstd
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.microsoft import is_msvc

LUAJIT_REF = "1edc3e52b67eaf6ce5f809be8e17d6862594b8bc"  # From Aug 3, 2026, v2.1 branch


class InfClassRecipe(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps"
    exports = "other/luajit-recipe.py"

    options = {
        "with_geolocation": [True, False],
        "with_lua": [True, False],
    }

    default_options = {
        "with_geolocation": True,
        "with_lua": True,
    }

    def requirements(self):
        self.requires("zlib/1.3.2")
        # TODO: Load system CA store on Windows
        # https://stackoverflow.com/a/37553616
        self.requires("libcurl/8.21.0")
        self.requires("libpng/1.6.58")
        self.requires("sqlite3/3.53.4")
        icu_options = {"data_packaging": "static"}
        if self.settings.os == "Windows":
            del icu_options["data_packaging"]
        self.requires("icu/78.2", options=icu_options)
        self.requires("openssl/3.6.3")

        if self.options.with_geolocation:
            self.requires("libmaxminddb/1.12.2")

        if self.options.with_lua:
            self.requires(f"luajit/{LUAJIT_REF}")

    def build_requirements(self):
        self.tool_requires("cmake/4.4.2")
        """
        cpython_options = {
            # https://github.com/conan-io/conan-center-index/blob/15bb0e6e7412633a3efb5240e15e3c730fb8be60/recipes/cpython/all/conanfile.py#L173-L176
            "shared": True,
            "with_bz2": False,
            "with_gdbm": False,
            "with_nis": False,
            "with_sqlite3": False,
            "with_tkinter": False,
            "with_curses": False,
            "with_lzma": False,
        }
        if is_msvc(self):
            # This needs refinement, but digging into Conan is super annoying
            # Passing a option that doesn't exist leads to an error
            # https://github.com/conan-io/conan-center-index/blob/15bb0e6e7412633a3efb5240e15e3c730fb8be60/recipes/cpython/all/conanfile.py#L82-L88
            del cpython_options["with_curses"]
            del cpython_options["with_gdbm"]
            del cpython_options["with_nis"]
        self.tool_requires("cpython/3.12.7", options=cpython_options)
        """
        # There seems to be some issues while building CPython on Windows
        self.tool_requires("ninja/1.13.2")

    def generate(self):
        tc = CMakeToolchain(self)

        tc.variables["_USING_CONAN"] = True
        if is_msvc(self):
            tc.variables["_MSVC_STATIC_CRT"] = (
                self.settings_build.compiler.runtime == "static"
            )

        tc.cache_variables["USE_LUA"] = self.options.with_lua
        tc.cache_variables["GEOLOCATION"] = self.options.with_geolocation
        tc.cache_variables["MYSQL"] = False

        tc.generate()

    def layout(self):
        cmake_layout(self)

    def configure(self):
        # https://github.com/conan-io/conan-center-index/issues/25032
        # https://github.com/conan-io/conan-center-index/issues/30024
        # The Conan Center version doesn't build on Windows and is extremely outdated
        # TODO: Find a better solution
        ConanAPI().export.export(
            os.path.join(self.recipe_folder, "other/luajit-recipe.py"),
            "luajit",
            LUAJIT_REF,
        )

    def validate(self):
        check_min_cppstd(self, 20)
        """
        if self.settings_build.os == "Windows" and not is_msvc(self):
            raise ConanInvalidConfiguration("LuaJIT doesn't build on Windows without MSVC'")
        """
        # The resaon may be the lack of POSIX coreutils

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
