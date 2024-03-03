from setuptools import Extension, setup

lib_dirs = ["src/cronet/lib"]
include_dirs = ["src/cronet/include"]

setup(
    package_data={"cronet": ["include/*.h"]},
    include_package_data=True,
    ext_modules=[
        Extension(
            name="cronet._cronet",
            include_dirs=include_dirs,
            library_dirs=lib_dirs,
            runtime_library_dirs=lib_dirs,
            libraries=["cronet.123.0.6300.0"],
            extra_compile_args=[],
            sources=["src/cronet/_cronet.c"],
        ),
    ],
)
