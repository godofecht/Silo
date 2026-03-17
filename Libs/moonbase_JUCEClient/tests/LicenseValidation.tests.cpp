#include <catch2/catch_test_macros.hpp>

#ifdef RUN_VALIDATION_BENCHMARK
    #include <catch2/benchmark/catch_benchmark.hpp>
#endif

#include "JUCEClientAPI.h"
#include "Implementations/StaticMethods.h"

#include "TestLicense.h"

TEST_CASE("Tests local license validation", "[LicenseValidation]")
{
    const auto apiOptions = GetApiOptions();

    REQUIRE(apiOptions.companyName == "Demo Co.");

    auto api = std::make_unique<Moonbase::JUCEClient::API>(apiOptions);
    api->setGracePeriodDays (99999); //so validatio

    auto& impl = *static_cast<Moonbase::JUCEClient::APIImpl*>(api->impl);
    impl.onlineValidator.reset ();

    const auto licenseFile = api->getLicenseFile();
    licenseFile.create();

    SECTION("validates a trial license")
    {
        licenseFile.replaceWithText(trialLicenseToken);
        const auto validationResult = ValidateMoonbaseLicenseContent(trialLicenseToken, impl);

        REQUIRE(validationResult.second == juce::String ());
        REQUIRE((bool)validationResult.first == true);
    }

    SECTION("validates a perpetual license token")
    {
        licenseFile.replaceWithText(perpetualLicenseToken);
        const auto validationResult = ValidateMoonbaseLicenseContent(perpetualLicenseToken, impl);

        REQUIRE(validationResult.second == juce::String ());
        REQUIRE((bool)validationResult.first == true);
    }

    SECTION("validates a offline license token")
    {
        licenseFile.replaceWithText(offlineLicenseToken);
        const auto validationResult = ValidateMoonbaseLicenseContent(offlineLicenseToken, impl);

        REQUIRE(validationResult.second == juce::String ());
        REQUIRE((bool)validationResult.first == true);
    }
    
#ifdef RUN_VALIDATION_BENCHMARK
    BENCHMARK_ADVANCED("Validation performance benchmark")(Catch::Benchmark::Chronometer meter) {
        licenseFile.replaceWithText(trialLicenseToken);
        meter.measure([&] { return ValidateMoonbaseLicenseContent(trialLicenseToken, impl); });
    };
#endif
}


TEST_CASE("Tests online license validation", "[LicenseValidation]")
{
    MB_INIT_JUCE_DISPATCH_LOOP

    const auto apiOptions = GetApiOptions();

    REQUIRE(apiOptions.companyName == "Demo Co.");

    auto api = std::make_unique<Moonbase::JUCEClient::API>(apiOptions);
    auto& impl = *static_cast<Moonbase::JUCEClient::APIImpl*>(api->impl);

    const auto licenseFile = api->getLicenseFile();
    licenseFile.create();

    struct OnlineValidationResult {
        juce::Atomic<bool> validationFinished = false;
        juce::Atomic<bool> unlocked = false;
        juce::Atomic<bool> trial = false;
        juce::Atomic<bool> offlineActivated = false;

    };

    const auto runOnlineValidation = [&]() -> OnlineValidationResult 
    {
        OnlineValidationResult result;
  
        api->addActivationStateChangedCallback ([&](bool unlocked, bool trial, bool offlineActivated) mutable {
            result.unlocked.set (unlocked);
            result.trial.set (trial);   
            result.offlineActivated.set (offlineActivated);
            result.validationFinished.set (true);
        });
        impl.onlineValidator->startNow ();

        const int timeout = 4000; // 4 seconds
        int timeElapsed = 0;
        while (!result.validationFinished.get () && timeElapsed < timeout)  {
            
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            timeElapsed += 10;
        }

        return result;
    };

    SECTION("runs online validation on a trial license")
    {
        licenseFile.replaceWithText(trialLicenseToken);

        auto validationResultFuture = std::async (std::launch::async, runOnlineValidation);
   
        MB_RUN_DISPATCH_LOOP_WHILE(validationResultFuture.wait_for(std::chrono::milliseconds(1)) != std::future_status::ready);

        const auto result = validationResultFuture.get (); 

        REQUIRE(result.validationFinished.get () == true);
        REQUIRE(result.unlocked.get ()           == true);
        REQUIRE(result.trial.get ()              == true);
        REQUIRE(result.offlineActivated.get ()   == false);
    }

    SECTION("runs online validation on a perpetual license")
    {
        licenseFile.replaceWithText(perpetualLicenseToken);

        auto validationResultFuture = std::async (std::launch::async, runOnlineValidation);
   
        MB_RUN_DISPATCH_LOOP_WHILE(validationResultFuture.wait_for(std::chrono::milliseconds(1)) != std::future_status::ready);

        const auto result = validationResultFuture.get (); 

        REQUIRE(result.validationFinished.get () == true);
        REQUIRE(result.unlocked.get ()           == true);
        REQUIRE(result.trial.get ()              == false);
        REQUIRE(result.offlineActivated.get ()   == false);
    }

}
