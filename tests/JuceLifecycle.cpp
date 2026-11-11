#include <juce_gui_basics/juce_gui_basics.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>

struct JuceLifecycle : Catch::EventListenerBase
{
    using EventListenerBase::EventListenerBase;

    JuceLifecycle(Catch::IConfig const* config)
        : EventListenerBase(config)
    {
        juce::initialiseJuce_GUI();
    }

    ~JuceLifecycle() override
    {
        juce::shutdownJuce_GUI();
    }
};

CATCH_REGISTER_LISTENER(JuceLifecycle)
