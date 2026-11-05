from conan import ConanFile
from conan.tools.layout import basic_layout

class CppAppConan(ConanFile):
    name = "CppApp"
    version = "0.1.0"
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"
    exports_sources = "CMakeLists.txt", "src/*"

    def layout(self):
        basic_layout(self)
