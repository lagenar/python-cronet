from setuptools import Extension, setup

lib_dirs = ["src/cronet/build"]
include_dirs = ["src/cronet/build/include"]

setup(
    package_data={"cronet": ["build/*.so", "build/include/*.h"]},
    include_package_data=True,
    ext_modules=[
        Extension(
            name="cronet._cronet",
            include_dirs=include_dirs,
            library_dirs=lib_dirs,
            runtime_library_dirs=lib_dirs,
            libraries=["cronet.122.0.6261.111"],
            extra_compile_args=[],
            sources=["src/cronet/_cronet.c"],
        ),
    ],
)
