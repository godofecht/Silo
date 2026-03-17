#include <catch2/catch_test_macros.hpp>

#include "JUCEClientAPI.cpp"
#include "TestLicense.h"

TEST_CASE("GetFieldFromLicense parses license metadata", "[LicenseSerde]")
{
    const auto apiOptions = GetApiOptions();

    REQUIRE(apiOptions.companyName == "Demo Co.");

    auto api = std::make_unique<Moonbase::JUCEClient::API>(apiOptions);
    auto& impl = *static_cast<Moonbase::JUCEClient::APIImpl*>(api->impl);

    const auto licenseFile = api->getLicenseFile();
    licenseFile.create();

    SECTION("given a license token, it parses fields")
    {
        licenseFile.replaceWithText(trialLicenseToken);
        auto result = GetFieldFromLicense(impl, "computerSignature");

        REQUIRE(result == "test-signature");
    };

    SECTION("given a license token, it returns an empty string on invalid field")
    {
        licenseFile.replaceWithText(trialLicenseToken);
        auto result = GetFieldFromLicense(impl, "invalidField");

        REQUIRE(result == "");
    };

    SECTION("given a trial license token, gets common license fields")
    {
        licenseFile.replaceWithText(trialLicenseToken);

        REQUIRE(api->getLicenseId() == "44304922708dcf30198b903ac51bd99e");

        REQUIRE(api->getCurrentReleaseVersion() == "1.0.0");
        REQUIRE(api->getOwnedSubProducts() == juce::StringArray());

        REQUIRE(api->getUserId() == "00000000-0000-0000-0000-000000000000");
        REQUIRE(api->getUserName() == "Trial");
        REQUIRE(api->getUserEmail() == "anonymous");

        REQUIRE(api->isTrial());
        REQUIRE(!api->isOfflineActivated());

        REQUIRE(api->getLastOnlineVerification().toISO8601(true) == juce::Time(2025, 6, 31, 13, 52, 17, 275, false).toISO8601(true));
        REQUIRE(api->getLicenseExpiration().toISO8601(true) == juce::Time(2028, 3, 25, 13, 52, 17, 274, false).toISO8601(true));
    };

    SECTION("given a perpetual license token, gets common license fields")
    {
        licenseFile.replaceWithText(perpetualLicenseToken);

        REQUIRE(api->getLicenseId() == "6ff5689d-fb3e-44c5-983a-0e2c905f0503");

        REQUIRE(api->getCurrentReleaseVersion() == "1.0.0");
        REQUIRE(api->getOwnedSubProducts() == juce::StringArray({"preset-pack-1"}));

        REQUIRE(api->getUserId() == "ec24e8d3-abbd-4712-87cd-dd423d2229f5");
        REQUIRE(api->getUserName() == "Tobias");
        REQUIRE(api->getUserEmail() == "test@moonwater.no");

        REQUIRE(!api->isTrial());
        REQUIRE(!api->isOfflineActivated());

        REQUIRE(api->getLastOnlineVerification().toISO8601(true) == juce::Time(2025, 6, 31, 13, 50, 36, 603, false).toISO8601(true));
    };

    SECTION("given a offline license token, gets common license fields")
    {
        licenseFile.replaceWithText(offlineLicenseToken);

        REQUIRE(api->getLicenseId() == "6ff5689d-fb3e-44c5-983a-0e2c905f0503");

        REQUIRE(api->getCurrentReleaseVersion() == "1.0.0");
        REQUIRE(api->getOwnedSubProducts() == juce::StringArray({"preset-pack-1"}));

        REQUIRE(api->getUserId() == "ec24e8d3-abbd-4712-87cd-dd423d2229f5");
        REQUIRE(api->getUserName() == "Tobias");
        REQUIRE(api->getUserEmail() == "test@moonwater.no");

        REQUIRE(!api->isTrial());
        REQUIRE(api->isOfflineActivated());

        REQUIRE(api->getLastOnlineVerification().toISO8601(true) == juce::Time(2025, 6, 31, 14, 06, 43, 578, false).toISO8601(true));
    };

}
