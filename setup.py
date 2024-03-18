from setuptools import Extension, setup

include_dirs = ["src/build/include"]

setup(
    package_data={"cronet": ["build/include/*.h"]},
    include_package_data=True,
    ext_modules=[
        Extension(
            name="cronet._cronet",
            include_dirs=include_dirs,
            libraries=["cronet.122.0.6261.111"],
            sources=["src/_cronet.c"],
        ),
    ],
)
