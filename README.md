# Cavl

[![Main Workflow](https://github.com/pavel-kirienko/cavl/actions/workflows/main.yml/badge.svg)](https://github.com/pavel-kirienko/cavl/actions/workflows/main.yml)

Generic single-file implementation of AVL tree suitable for deeply embedded systems.
**Simply copy `cavl.h` or `cavl.hpp` (depending on which language you need) into your project tree
and you are ready to roll.**
The usage instructions are provided in the comments.
The code is fully covered by manual and randomized tests with full state space exploration.

For development-related instructions please refer to the CI configuration files.
To release a new version, simply create a new tag.

![Tree](randomized_test_tree.png "Random tree generated by the test suite")
