#include "gtest/gtest.h"
#include "conf.h"

int main(int argc, char **argv) {
	InitTestConf();
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
