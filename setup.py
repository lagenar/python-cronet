from setuptools import Extension, setup

include_dirs = ["src/cronet/build/include"]

setup(
    package_data={"cronet": ["build/include/*.h"]},
    include_package_data=True,
    ext_modules=[
        Extension(
            name="cronet._cronet",
            include_dirs=include_dirs,
            libraries=["cronet.133.0.6943.98"],
            sources=["src/cronet/_cronet.c"],
        ),
    ],
)
