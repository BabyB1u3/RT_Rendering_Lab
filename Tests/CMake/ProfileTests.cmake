set(GLAB_DEV_PROFILE_TEST_SOURCES
    Integration/TestCatalogRegistryDev.cpp
    Integration/TestMountDiscoveryDev.cpp
    Integration/TestRootDiscoveryDev.cpp
)

glab_add_test_executable(rtrlab_dev_profile_tests "DevProfile." "integration;dev-profile"
    ${GLAB_DEV_PROFILE_TEST_SOURCES}
)

set(GLAB_SHIPPING_PROFILE_TEST_SOURCES
    Integration/TestMountDiscoveryShipping.cpp
    Integration/TestRootDiscoveryShipping.cpp
)

glab_add_test_executable(rtrlab_shipping_profile_tests "ShippingProfile." "integration;shipping-profile"
    ${GLAB_SHIPPING_PROFILE_TEST_SOURCES}
)
