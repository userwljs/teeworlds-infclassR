# MIT License
#
# Copyright (c) 2026 Andrey Filipenkov
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

# https://github.com/kambala-decapitator/conan-luajit/tree/04788646d31531e7c7b71f460f6efebed31826ce

from conan import ConanFile
from conan.tools.scm import Version
from conan.tools.files import get, chdir, replace_in_file, copy, rmdir
from conan.tools.microsoft import is_msvc, MSBuildToolchain, VCVars, unix_path
from conan.tools.layout import basic_layout
from conan.tools.gnu import Autotools, AutotoolsToolchain
from conan.tools.apple import XCRun
from conan.tools.build import cross_building
from conan.tools.env import VirtualBuildEnv
from conan.errors import ConanInvalidConfiguration
import os


required_conan_version = ">=2.4"


class LuajitConan(ConanFile):
    name = "luajit"
    license = "MIT"
    url = "https://github.com/conan-io/conan-center-index"
    homepage = "http://luajit.org"
    description = "LuaJIT is a Just-In-Time Compiler (JIT) for the Lua programming language."
    topics = ("lua", "jit")
    provides = "lua"
    languages = "C"
    package_type = "library"
    settings = "os", "arch", "compiler", "build_type"
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = {"shared": False, "fPIC": True}

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def layout(self):
        basic_layout(self, src_folder="src")

    @property
    def _is_host_32bit(self):
        return self.settings.arch in ["armv7", "x86"]

    def validate_build(self):
        if self._is_host_32bit and self.settings_build.os == "Macos":
            # well, technically it should work on macOS <= 10.14
            raise ConanInvalidConfiguration(f"{self.ref} cannot be cross-built to a 32-bit platform on macOS, see https://github.com/LuaJIT/LuaJIT/issues/664")

    def validate(self):
        if self.settings.os == "Macos" and self.settings.arch == "armv8" and cross_building(self):
            raise ConanInvalidConfiguration(f"{self.ref} can not be cross-built to Mac M1.")
        if self.settings.os == "Macos" and not self._apple_deployment_target():
            raise ConanInvalidConfiguration("macOS build requires os.version (deployment target) to be set")

    def source(self):
        filename = f"LuaJIT-{self.version}.zip"
        get(self, f"https://github.com/LuaJIT/LuaJIT/archive/{self.version}.zip", destination=self.source_folder, filename=filename, strip_root=True)
        self._patch_sources()

    def generate(self):
        if is_msvc(self):
            tc = MSBuildToolchain(self)
            tc.generate()
            tc = VCVars(self)
            tc.generate()
        else:
            tc = AutotoolsToolchain(self)
            env = tc.environment()
            self._adjustAutotoolsToolchainEnv(env)
            tc.generate(env)

    def _adjustAutotoolsToolchainEnv(self, env):
        envVars = env.vars(self)
        cflags = ""
        if self.settings.os == "iOS" or self.settings.os == "Android":
            cflags = envVars.get("CFLAGS")
            env.unset("CFLAGS")

            ldflags = envVars.get("LDFLAGS")
            env.define("TARGET_LDFLAGS", ldflags)
            env.define("TARGET_SHLDFLAGS", ldflags)
            env.unset("LDFLAGS")
        # upstream doesn't read CPPFLAGS, inject them manually
        env.define("TARGET_CFLAGS", f"{cflags} {envVars.get(name='CPPFLAGS')}")

    def _patch_sources(self):
        makefile = os.path.join(self.source_folder, 'src', 'Makefile')
        replace_in_file(self, makefile,
                                'TARGET_DYLIBPATH= $(TARGET_LIBPATH)/$(TARGET_DYLIBNAME)',
                                'TARGET_DYLIBPATH= $(TARGET_DYLIBNAME)')

    def _apple_deployment_target(self, default=None):
        return self.settings.get_safe("os.version", default=default)

    @property
    def _make_arguments(self):
        args = [
            f"PREFIX={unix_path(self, self.package_folder)}",
            f"BUILDMODE={'dynamic' if self.options.shared else 'static'}",
        ]
        if "clang" in str(self.settings.compiler):
            args.append("DEFAULT_CC=clang")

        if self.settings.os == "Macos" and self._apple_deployment_target():
            args.append(f"MACOSX_DEPLOYMENT_TARGET={self._apple_deployment_target()}")
        elif self.settings.os == "iOS":
            args.extend([
                f"CROSS='{os.path.dirname(XCRun(self).cc)}/'",
                "TARGET_SYS=iOS",
            ])
        elif self.settings.os == "Android":
            buildenv_vars = VirtualBuildEnv(self).vars()
            compiler_path = buildenv_vars.get("CC")
            triplet_prefix = f"{buildenv_vars.get('CHOST')}-"
            cross_prefix = os.path.join(buildenv_vars.get("NDK_ROOT"), "bin", triplet_prefix)
            args.extend([
                f"CROSS='{cross_prefix}'",
                f"DYNAMIC_CC='{compiler_path} -fPIC'",
                f"STATIC_CC='{compiler_path}'",
                f"TARGET_AR='{buildenv_vars.get('AR')} rcus'",
                f"TARGET_LD='{compiler_path}'",
                f"TARGET_STRIP='{buildenv_vars.get('STRIP')}'",
                "TARGET_SYS=Linux",
            ])
            if self._is_host_32bit:
                args.append("HOST_CC='gcc -m32'")
            if self.settings_build.os == "Macos":
                # must look for headers in macOS SDK, having NDK clang in PATH breaks this default behavior
                xcrun_build = XCRun(self, sdk='macosx')
                isysroot_flag = f'-isysroot "{xcrun_build.sdk_path}"'
                args.extend([
                    f"HOST_CC='{xcrun_build.cc}'",
                    f"HOST_CFLAGS='{isysroot_flag}'",
                    f"HOST_LDFLAGS='{isysroot_flag}'",
                ])

        if self.conf.get("tools.build:verbosity", choices=("quiet", "verbose")) == "quiet":
            args.append("E=@:")
        if self.conf.get("tools.compilation:verbosity", choices=("quiet", "verbose")) == "verbose":
            args.append("Q=")
        return args

    @property
    def _luajit_include_folder(self):
        luaversion = Version(self.version)
        if luaversion.major == "2":
            return f"luajit-{luaversion.major}.{luaversion.minor}"
        return "luajit-2.1"

    def build(self):
        if is_msvc(self):
            with chdir(self, os.path.join(self.source_folder, "src")):
                build_command = ["msvcbuild.bat"]
                if self.settings.build_type in ["Debug", "RelWithDebInfo"]:
                    build_command.append("debug")
                if not self.options.shared:
                    build_command.append("static")
                self.run(" ".join(build_command), env="conanbuild")
        else:
            with chdir(self, self.source_folder):
                autotools = Autotools(self)
                autotools.make(args=self._make_arguments)

    def package(self):
        copy(self, "COPYRIGHT", dst=os.path.join(self.package_folder, "licenses"), src=self.source_folder)
        src_folder = os.path.join(self.source_folder, "src")
        include_folder = os.path.join(self.package_folder, "include", self._luajit_include_folder)
        if is_msvc(self):
            copy(self, "lua.h", src=src_folder, dst=include_folder)
            copy(self, "lualib.h", src=src_folder, dst=include_folder)
            copy(self, "lauxlib.h", src=src_folder, dst=include_folder)
            copy(self, "luaconf.h", src=src_folder, dst=include_folder)
            copy(self, "lua.hpp", src=src_folder, dst=include_folder)
            copy(self, "luajit.h", src=src_folder, dst=include_folder)
            copy(self, "lua51.lib", src=src_folder, dst=os.path.join(self.package_folder, "lib"))
            copy(self, "lua51.dll", src=src_folder, dst=os.path.join(self.package_folder, "bin"))
        else:
            with chdir(self, self.source_folder):
                autotools = Autotools(self)
                autotools.install(args=self._make_arguments + ["DESTDIR="])
            rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))
            rmdir(self, os.path.join(self.package_folder, "share"))

    def package_info(self):
        self.cpp_info.libs = ["lua51" if is_msvc(self) else "luajit-5.1"]
        self.cpp_info.set_property("pkg_config_name", "luajit")
        self.cpp_info.includedirs = [os.path.join("include", self._luajit_include_folder)]
        if self.settings.os in ["Linux", "FreeBSD", "Android"]:
            self.cpp_info.system_libs.extend(["m", "dl"])
