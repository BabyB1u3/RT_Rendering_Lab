set(GLAB_DEV_PROFILE_TEST_SOURCES
    Dev/Integration/Core/Resource/TestCatalogRegistryDev.cpp
    Dev/Integration/Core/Resource/TestMountDiscoveryDev.cpp
    Dev/Integration/Core/Resource/TestRootDiscoveryDev.cpp
)

glab_add_test_executable(rtrlab_dev_profile_tests "DevProfile." "integration;dev-profile"
    ${GLAB_DEV_PROFILE_TEST_SOURCES}
)

set(GLAB_SHIPPING_PROFILE_TEST_SOURCES
    Shipping/Integration/Core/Resource/TestMountDiscoveryShipping.cpp
    Shipping/Integration/Core/Resource/TestRootDiscoveryShipping.cpp
)

glab_add_test_executable(rtrlab_shipping_profile_tests "ShippingProfile." "integration;shipping-profile"
    ${GLAB_SHIPPING_PROFILE_TEST_SOURCES}
)
