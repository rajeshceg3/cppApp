from conan import ConanFile
from conan.tools.cmake import cmake_layout # Keep for CMakeToolchain/CMakeDeps if still needed by them, or remove if not.
from conan.tools.layout import basic_layout # Added import

class CppAppConan(ConanFile):
    name = "CppApp"
    version = "0.1.0"
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"
    exports_sources = "CMakeLists.txt", "src/*"

    def requirements(self):
        self.requires("fmt/10.2.1")

    def layout(self):
        basic_layout(self) # Changed to basic_layout
