#include "Licence.h"
#include <thread>

namespace spark
{
namespace
{
    const char* salt = "obsidian-hex-ring-7f3a";   // mixes into the file's signature

    juce::File defaultStorage (const juce::String& product)
    {
        auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
       #if JUCE_MAC
        dir = dir.getChildFile ("Application Support");
       #endif
        return dir.getChildFile ("Spark Audio").getChildFile (product).getChildFile ("licence.dat");
    }

    juce::String normaliseKey (juce::String k)
    {
        return k.trim().removeCharacters (" \t\r\n").toUpperCase();
    }
}

Licence::Transport Licence::gumroadTransport()
{
    return [] (const juce::String& productId, const juce::String& key, bool count) -> Reply
    {
        Reply r;
        const juce::String post = "product_id=" + juce::URL::addEscapeChars (productId, true)
                                + "&license_key=" + juce::URL::addEscapeChars (key, true)
                                + "&increment_uses_count=" + (count ? "true" : "false");
        const auto url = juce::URL ("https://api.gumroad.com/v2/licenses/verify").withPOSTData (post);
        int status = 0;
        auto stream = url.createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                                                 .withConnectionTimeoutMs (12000)
                                                 .withStatusCode (&status)
                                                 .withExtraHeaders ("Content-Type: application/x-www-form-urlencoded"));
        if (stream == nullptr)
            return r;
        const auto json = juce::JSON::parse (stream->readEntireStreamAsString());
        if (! json.isObject())
            return r;
        r.reached = true;
        r.success = (bool) json.getProperty ("success", false);
        r.uses = (int) json.getProperty ("uses", 0);
        r.message = json.getProperty ("message", {}).toString();
        const auto purchase = json.getProperty ("purchase", {});
        r.email = purchase.getProperty ("email", {}).toString();
        r.refunded = (bool) purchase.getProperty ("refunded", false);
        r.chargebacked = (bool) purchase.getProperty ("chargebacked", false);
        r.disputed = (bool) purchase.getProperty ("disputed", false) && ! (bool) purchase.getProperty ("dispute_won", false);
        r.subscriptionEnded = ! purchase.getProperty ("subscription_ended_at", {}).isVoid()
                              && purchase.getProperty ("subscription_ended_at", {}).toString().isNotEmpty();
        return r;
    };
}

Licence::Licence (Config c) : config (std::move (c)), transport (gumroadTransport()), clock (config.clock)
{
    storage = config.storage != juce::File() ? config.storage : defaultStorage (config.productName);
    load();
    refresh();
    startTimer (60 * 1000);
}

Licence::~Licence()
{
    stopTimer();
    *alive = false;
}

void Licence::setStorageFile (const juce::File& f)
{
    storage = f;
    reload();
}

juce::String Licence::signatureFor (const juce::String& payload) const
{
    const auto text = payload + "|" + juce::SystemStats::getUniqueDeviceID() + "|" + config.productName + "|" + salt;
    return juce::String::toHexString (text.hashCode64()) + juce::String::toHexString ((text + salt).hashCode64());
}

void Licence::load()
{
    key = {}; email = {};
    trialStart = activatedAt = lastVerified = {};
    if (storage.existsAsFile())
    {
        const auto json = juce::JSON::parse (storage.loadFileAsString());
        const auto payload = json.getProperty ("data", {}).toString();
        const auto data = juce::JSON::parse (payload);
        if (data.isObject() && json.getProperty ("sig", {}).toString() == signatureFor (payload))
        {
            trialStart = juce::Time ((juce::int64) data.getProperty ("trialStart", 0));
            key = data.getProperty ("key", {}).toString();
            email = data.getProperty ("email", {}).toString();
            activatedAt = juce::Time ((juce::int64) data.getProperty ("activatedAt", 0));
            lastVerified = juce::Time ((juce::int64) data.getProperty ("lastVerified", 0));
        }
        else if (data.isObject())
        {
            // copied from another computer or edited: keep the trial date (so the trial doesn't restart), drop the key
            trialStart = juce::Time ((juce::int64) data.getProperty ("trialStart", 0));
        }
    }
    if (trialStart.toMilliseconds() <= 0 || trialStart > now())
    {
        trialStart = now();
        if (isEnabled()) save();
    }
}

void Licence::save() const
{
    auto* data = new juce::DynamicObject();
    data->setProperty ("trialStart", trialStart.toMilliseconds());
    data->setProperty ("key", key);
    data->setProperty ("email", email);
    data->setProperty ("activatedAt", activatedAt.toMilliseconds());
    data->setProperty ("lastVerified", lastVerified.toMilliseconds());
    const auto payload = juce::JSON::toString (juce::var (data), true);
    auto* outer = new juce::DynamicObject();
    outer->setProperty ("data", payload);
    outer->setProperty ("sig", signatureFor (payload));
    storage.getParentDirectory().createDirectory();
    storage.replaceWithText (juce::JSON::toString (juce::var (outer)));
}

Licence::State Licence::getState() const
{
    if (! isEnabled()) return State::disabled;
    if (key.isNotEmpty())
    {
        const double sinceCheck = (now() - lastVerified).inDays();
        return sinceCheck > config.recheckDays + config.offlineGraceDays ? State::needsRecheck : State::active;
    }
    return trialDaysLeft() > 0 ? State::trial : State::trialOver;
}

int Licence::trialDaysLeft() const
{
    const double used = (now() - trialStart).inDays();
    return juce::jmax (0, (int) std::ceil ((double) config.trialDays - used));
}

juce::String Licence::getMaskedKey() const
{
    return key.length() > 8 ? "****-" + key.getLastCharacters (8) : key;
}

juce::String Licence::statusLine() const
{
    switch (getState())
    {
        case State::trial:        return juce::String (juce::CharPointer_UTF8 ("TRIAL \xc2\xb7 ")) + juce::String (trialDaysLeft()) + (trialDaysLeft() == 1 ? " DAY LEFT" : " DAYS LEFT");
        case State::trialOver:    return juce::String (juce::CharPointer_UTF8 ("TRIAL ENDED \xc2\xb7 SOUND DROPS OUT UNTIL YOU ACTIVATE"));
        case State::needsRecheck: return "CONNECT TO THE INTERNET TO CONFIRM YOUR LICENCE";
        case State::active:       return "LICENSED" + (email.isNotEmpty() ? " TO " + email.toUpperCase() : juce::String());
        case State::disabled:     break;
    }
    return {};
}

void Licence::refresh()
{
    const auto s = getState();
    restricted = s == State::trialOver || s == State::needsRecheck;
    sendChangeMessage();
}

juce::String Licence::checkReply (const Reply& r, int maxActivations, bool counting)
{
    if (! r.reached)
        return "Couldn't reach Gumroad to check your key. Check your internet connection and try again.";
    if (! r.success)
        return r.message.isNotEmpty() ? r.message : "That key wasn't recognised. Copy it from your Gumroad receipt and try again.";
    if (r.refunded || r.chargebacked || r.disputed)
        return "This purchase was refunded or disputed, so the key no longer works.";
    if (r.subscriptionEnded)
        return "The subscription for this key has ended.";
    if (counting && r.uses > maxActivations)
        return "This key is already activated on " + juce::String (maxActivations)
             + " computers. Email support with your receipt and we'll reset it.";
    return {};
}

void Licence::activate (const juce::String& rawKey, std::function<void (bool, const juce::String&)> done)
{
    const auto k = normaliseKey (rawKey);
    if (k.isEmpty())
    {
        if (done) done (false, "Paste the licence key from your Gumroad receipt.");
        return;
    }
    if (busy.exchange (true))
        return;
    auto t = transport;
    auto flag = alive;
    const auto productId = config.productId;
    std::thread ([this, t, flag, productId, k, done]
    {
        const auto reply = t (productId, k, true);
        juce::MessageManager::callAsync ([this, flag, reply, k, done]
        {
            if (! *flag) return;
            busy = false;
            const auto problem = checkReply (reply, config.maxActivations, true);
            if (problem.isEmpty())
            {
                key = k;
                email = reply.email;
                activatedAt = lastVerified = now();
                save();
            }
            refresh();
            if (done) done (problem.isEmpty(), problem.isEmpty() ? "Activated. Thanks for supporting " + config.productName + "!" : problem);
        });
    }).detach();
}

void Licence::removeFromThisComputer()
{
    key = {}; email = {};
    activatedAt = lastVerified = {};
    save();
    refresh();
}

void Licence::recheckIfDue()
{
    if (key.isEmpty() || busy.load() || (now() - lastVerified).inDays() < config.recheckDays)
        return;
    busy = true;
    auto t = transport;
    auto flag = alive;
    const auto productId = config.productId;
    const auto k = key;
    std::thread ([this, t, flag, productId, k]
    {
        const auto reply = t (productId, k, false);
        juce::MessageManager::callAsync ([this, flag, reply]
        {
            if (! *flag) return;
            busy = false;
            if (! reply.reached)
                return;   // offline: keep going on the grace period
            if (checkReply (reply, config.maxActivations, false).isEmpty())
                lastVerified = now();
            else
                key = email = {};   // refunded, charged back or revoked
            save();
            refresh();
        });
    }).detach();
}
} // namespace spark
