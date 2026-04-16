#include <gtest/gtest.h>

#include "Core/Diagnostics/Logging/LogCategories.h"

TEST(LogCategoriesTests, IsKnownCategoryRecognizesPredefinedNames)
{
    EXPECT_TRUE(LogCategory::IsKnownCategory(LogCategory::k_Core));
    EXPECT_TRUE(LogCategory::IsKnownCategory(LogCategory::k_Demo));
    EXPECT_TRUE(LogCategory::IsKnownCategory(LogCategory::k_Crash));
}

TEST(LogCategoriesTests, IsKnownCategoryRejectsUnknownNames)
{
    EXPECT_FALSE(LogCategory::IsKnownCategory("MyPlugin"));
    EXPECT_FALSE(LogCategory::IsKnownCategory("DefinitelyNotReal"));
}
