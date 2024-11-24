#include "flags.h"

namespace {

enum class MyFlag
{
	Flag0,
	Flag1,
	Flag2,
	Flag3,
	Flag4,
};

DECLARE_FLAGS(MyFlags, MyFlag)

static_assert(MyFlags(MyFlag::Flag1) & MyFlag::Flag1);
static_assert((MyFlags(MyFlag::Flag1) & MyFlag::Flag2) == false);
static_assert((MyFlags(MyFlag::Flag1) & MyFlag::Flag3));
static_assert((MyFlags(MyFlag::Flag3) & MyFlag::Flag1));
static_assert((MyFlags(MyFlag::Flag3) & MyFlag::Flag2));
static_assert((MyFlags(MyFlag::Flag4) & MyFlag::Flag1) == false);
static_assert((MyFlags(MyFlag::Flag4) & MyFlag::Flag2) == false);
static_assert((MyFlags(MyFlag::Flag4) & MyFlag::Flag3) == false);
static_assert((MyFlag::Flag1 | MyFlag::Flag2) & MyFlag::Flag1);
static_assert((MyFlag::Flag1 | MyFlag::Flag2) & MyFlag::Flag2);
static_assert((MyFlag::Flag1 | MyFlag::Flag2) & MyFlag::Flag3);
static_assert(((MyFlag::Flag1 | MyFlag::Flag2) & MyFlag::Flag4) == false);

} // namespace
