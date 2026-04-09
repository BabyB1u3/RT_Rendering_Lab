#include <gtest/gtest.h>

#include "Core/Diagnostics/Logging/LogCategories.h"

TEST(LogCategoriesTests, IsKnownCategoryRecognizesPredefinedNames)
{
    EXPECT_TRUE(LogCategory::IsKnownCategory(LogCategory::Core));
    EXPECT_TRUE(LogCategory::IsKnownCategory(LogCategory::Demo));
    EXPECT_TRUE(LogCategory::IsKnownCategory(LogCategory::Crash));
}

TEST(LogCategoriesTests, IsKnownCategoryRejectsUnknownNames)
{
    EXPECT_FALSE(LogCategory::IsKnownCategory("MyPlugin"));
    EXPECT_FALSE(LogCategory::IsKnownCategory("DefinitelyNotReal"));
}
