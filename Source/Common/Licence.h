#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <atomic>
#include <functional>
#include <memory>

namespace spark
{
// Licence keys sold through Gumroad. A new install runs as a full trial for a few days; after that,
// until a key is activated, the plugin runs restricted (the processor and editor decide what that
// means). Activation checks the key with Gumroad's licence API once, then the key is kept in a small
// signed file next to the user's settings and re-checked now and then, with a long offline grace.
//
// This is a deterrent, not DRM: the file is signed with the computer's id so it can't simply be
// copied to another machine, and refunded or charged-back purchases stop working at the next check.
class Licence : public juce::ChangeBroadcaster,
                private juce::Timer
{
public:
    struct Config
    {
        juce::String productName;          // "OBSDN"
        juce::String productId;            // Gumroad product_id; empty = licensing switched off
        juce::String buyUrl;               // the Gumroad page
        int trialDays = 14;
        int maxActivations = 5;            // computers per key (Gumroad's uses count)
        int recheckDays = 14;              // re-verify this often when online
        int offlineGraceDays = 60;         // keep working this long without reaching Gumroad
        juce::File storage;                // where the key is kept (empty: the user's settings folder)
        std::function<juce::Time()> clock; // for tests (empty: the real time)
    };

    enum class State { disabled, trial, trialOver, active, needsRecheck };

    // What the licence server answered. The default talks to api.gumroad.com; tests replace it.
    struct Reply
    {
        bool reached = false;              // false: no network or no answer
        bool success = false;
        int uses = 0;
        juce::String message, email;
        bool refunded = false, chargebacked = false, disputed = false, subscriptionEnded = false;
    };
    using Transport = std::function<Reply (const juce::String& productId, const juce::String& key, bool countThisActivation)>;

    explicit Licence (Config);
    ~Licence() override;

    State getState() const;
    bool isRestricted() const noexcept { return restricted.load(); }   // any thread
    bool isEnabled() const noexcept { return config.productId.isNotEmpty(); }
    int trialDaysLeft() const;
    juce::String getEmail() const { return email; }
    juce::String getMaskedKey() const;
    const Config& getConfig() const noexcept { return config; }
    juce::String statusLine() const;   // one line for the window's footer

    // Activation (message thread). The callback runs on the message thread: ok, and a message for the person.
    void activate (const juce::String& key, std::function<void (bool ok, const juce::String& message)> done);
    void removeFromThisComputer();
    void recheckIfDue();    // quietly, in the background, when the last check is old
    bool isBusy() const noexcept { return busy.load(); }

    // ---- for tests
    void setTransport (Transport t) { transport = std::move (t); }
    void setClock (std::function<juce::Time()> c) { clock = std::move (c); refresh(); }
    void setStorageFile (const juce::File& f);
    void reload() { load(); refresh(); }
    void update() { refresh(); }   // what the once-a-minute timer does (trial days and grace run out on their own)
    static Transport gumroadTransport();

private:
    void load();
    void save() const;
    void refresh();                                 // recomputes 'restricted' and tells listeners
    void timerCallback() override { if (restricted.load() != (getState() == State::trialOver || getState() == State::needsRecheck)) refresh(); }
    juce::String signatureFor (const juce::String& payload) const;
    static juce::String checkReply (const Reply&, int maxActivations, bool counting);
    juce::Time now() const { return clock ? clock() : juce::Time::getCurrentTime(); }

    Config config;
    juce::File storage;
    juce::String key, email;
    juce::Time trialStart, activatedAt, lastVerified;
    std::atomic<bool> restricted { false }, busy { false };
    Transport transport;
    std::function<juce::Time()> clock;
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);
};
} // namespace spark
