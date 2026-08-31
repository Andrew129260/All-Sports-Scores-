#include "unity/unity.h"
#include "../src/c/data/model/models.h"
#include "mocks/pebble.h"

void setUp(void) {}
void tearDown(void) {}

void test_sport_get_icon_res_small(void) {
    TEST_ASSERT_EQUAL(RESOURCE_ID_FOOTBALL_16, sport_get_icon_res_small(SportNFL));
    TEST_ASSERT_EQUAL(RESOURCE_ID_BASEBALL_16, sport_get_icon_res_small(SportMLB));
    TEST_ASSERT_EQUAL(RESOURCE_ID_HOCKEY_16, sport_get_icon_res_small(SportNHL));
    TEST_ASSERT_EQUAL(RESOURCE_ID_BASKETBALL_16, sport_get_icon_res_small(SportNBA));
    TEST_ASSERT_EQUAL(RESOURCE_ID_Mls_16, sport_get_icon_res_small(SportMLS));
    TEST_ASSERT_EQUAL(RESOURCE_ID_Rugby_16, sport_get_icon_res_small(SportRugby));
    TEST_ASSERT_EQUAL(RESOURCE_ID_Cricket_16, sport_get_icon_res_small(SportCricket));
    TEST_ASSERT_EQUAL(RESOURCE_ID_IMAGE_SPORT_TENNIS16, sport_get_icon_res_small(SportTennis));
    TEST_ASSERT_EQUAL(RESOURCE_ID_IMAGE_SPORT_AFL16, sport_get_icon_res_small(SportAFL));
    TEST_ASSERT_EQUAL(RESOURCE_ID_IMAGE_SPORT_MMA16, sport_get_icon_res_small(SportMMA));

    // Default case
    TEST_ASSERT_EQUAL(RESOURCE_ID_STAR_16, sport_get_icon_res_small(Favorites));
    TEST_ASSERT_EQUAL(RESOURCE_ID_STAR_16, sport_get_icon_res_small(999));
}

void test_sport_get_icon_res_large(void) {
    TEST_ASSERT_EQUAL(RESOURCE_ID_FOOTBALL_25, sport_get_icon_res_large(SportNFL));
    TEST_ASSERT_EQUAL(RESOURCE_ID_BASEBALL_25, sport_get_icon_res_large(SportMLB));
    TEST_ASSERT_EQUAL(RESOURCE_ID_HOCKEY_25, sport_get_icon_res_large(SportNHL));
    TEST_ASSERT_EQUAL(RESOURCE_ID_BASKETBALL_25, sport_get_icon_res_large(SportNBA));
    TEST_ASSERT_EQUAL(RESOURCE_ID_Mls_25, sport_get_icon_res_large(SportMLS));
    TEST_ASSERT_EQUAL(RESOURCE_ID_Rugby_25, sport_get_icon_res_large(SportRugby));
    TEST_ASSERT_EQUAL(RESOURCE_ID_Cricket_25, sport_get_icon_res_large(SportCricket));
    TEST_ASSERT_EQUAL(RESOURCE_ID_IMAGE_SPORT_TENNIS25, sport_get_icon_res_large(SportTennis));
    TEST_ASSERT_EQUAL(RESOURCE_ID_IMAGE_SPORT_AFL25, sport_get_icon_res_large(SportAFL));
    TEST_ASSERT_EQUAL(RESOURCE_ID_IMAGE_SPORT_MMA25, sport_get_icon_res_large(SportMMA));

    // Default case
    TEST_ASSERT_EQUAL(RESOURCE_ID_STAR_25, sport_get_icon_res_large(Favorites));
    TEST_ASSERT_EQUAL(RESOURCE_ID_STAR_25, sport_get_icon_res_large(999));
}

void test_sport_get_name(void) {
    TEST_ASSERT_EQUAL_STRING("NFL", sport_get_name(SportNFL));
    TEST_ASSERT_EQUAL_STRING("MLB", sport_get_name(SportMLB));
    TEST_ASSERT_EQUAL_STRING("NHL", sport_get_name(SportNHL));
    TEST_ASSERT_EQUAL_STRING("NBA", sport_get_name(SportNBA));
    TEST_ASSERT_EQUAL_STRING("MLS", sport_get_name(SportMLS));
    TEST_ASSERT_EQUAL_STRING("Rugby", sport_get_name(SportRugby));
    TEST_ASSERT_EQUAL_STRING("Cricket", sport_get_name(SportCricket));
    TEST_ASSERT_EQUAL_STRING("Tennis", sport_get_name(SportTennis));
    TEST_ASSERT_EQUAL_STRING("AFL", sport_get_name(SportAFL));
    TEST_ASSERT_EQUAL_STRING("MMA", sport_get_name(SportMMA));

    // Default case
    TEST_ASSERT_EQUAL_STRING("Favorites", sport_get_name(Favorites));
    TEST_ASSERT_EQUAL_STRING("Favorites", sport_get_name(999));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_sport_get_icon_res_small);
    RUN_TEST(test_sport_get_icon_res_large);
    RUN_TEST(test_sport_get_name);
    return UNITY_END();
}
