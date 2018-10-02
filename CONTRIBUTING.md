# Contributing
Contributing is by all means welcome. Please remember to:
 * write a good commit description
 * make sure that tests pass
 * preferrably add a test for added functionality
 * check if it build and works in both compilation profiles (`-DCMAKE_BUILD_TYPE=Debug` and `-DCMAKE_BUILD_TYPE=Release` cmake arguments)
 * make sure scripts/format.sh doesn't report any issues; you can actually `./scripts/format | patch` -p0 to reformat your changes
 * make sure scripts/lint.sh doesn't report any issues
 * make sure scripts/tidy.sh doesn't report any issues
