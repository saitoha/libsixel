from setuptools import Extension, setup

setup(
    ext_modules=[
        Extension(
            "libsixel_wheel._bootstrap",
            sources=["src/libsixel_wheel/_bootstrap.c"],
        )
    ],
)
