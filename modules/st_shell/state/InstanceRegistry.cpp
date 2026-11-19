namespace st
{

InstanceRegistry& InstanceRegistry::get()
{
    // Function-local static: created by the first instance, destroyed when the binary unloads
    // (after every instance has been destroyed).
    static InstanceRegistry registry;
    return registry;
}

juce::Array<ProcessorBase*> InstanceRegistry::getInstances() const
{
    const juce::ScopedLock sl (lock);
    return instances;
}

int InstanceRegistry::getNumInstances() const
{
    const juce::ScopedLock sl (lock);
    return instances.size();
}

int InstanceRegistry::add (ProcessorBase* p)
{
    int n;
    {
        const juce::ScopedLock sl (lock);
        instances.addIfNotAlreadyThere (p);
        n = nextNumber++;
    }
    triggerAsyncUpdate();
    return n;
}

void InstanceRegistry::remove (ProcessorBase* p)
{
    {
        const juce::ScopedLock sl (lock);
        if (! instances.contains (p)) return;
        instances.removeFirstMatchingValue (p);
    }
    if (juce::MessageManager::getInstanceWithoutCreating() != nullptr
        && juce::MessageManager::getInstanceWithoutCreating()->isThisTheMessageThread())
        listeners.call ([p] (Listener& l) { l.instanceRemoved (p); });
    else
        triggerAsyncUpdate();
}

} // namespace st
