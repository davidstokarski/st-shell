namespace st::userSettings
{

static juce::PropertiesFile& file()
{
    static std::unique_ptr<juce::PropertiesFile> f = []
    {
        juce::PropertiesFile::Options o;
        o.applicationName = "settings";
        o.folderName = kCompanyName;
        o.filenameSuffix = "settings";
        o.osxLibrarySubFolder = "Application Support";
        o.millisecondsBeforeSaving = 0;   // other ST plug-ins read it too
        // settings saved before the rename to Stokarski Studios
        const auto dir = o.getDefaultFile().getParentDirectory();
        const auto legacy = dir.getSiblingFile (kLegacyCompanyName);
        if (const auto old = legacy.getChildFile (o.getDefaultFile().getFileName()); old.existsAsFile() && ! o.getDefaultFile().exists())
        {
            dir.createDirectory();
            old.moveFileTo (o.getDefaultFile());
        }
        if (legacy.isDirectory() && legacy.getNumberOfChildFiles (juce::File::findFilesAndDirectories, "*") == 0) legacy.deleteFile();
        return std::make_unique<juce::PropertiesFile> (o);
    }();
    return *f;
}

bool getBool (const juce::String& key, bool defaultValue)
{
    auto& f = file();
    f.reload();   // another ST plug-in may have changed it
    return f.getBoolValue (key, defaultValue);
}

void setBool (const juce::String& key, bool value)
{
    auto& f = file();
    f.setValue (key, value);
    f.saveIfNeeded();
}

static std::atomic<int>& frameCapCache() { static std::atomic<int> v { -1 }; return v; }

bool isFrameRateCapped() noexcept
{
    auto& c = frameCapCache();
    int v = c.load (std::memory_order_relaxed);
    if (v < 0)
    {
        v = getBool (capFrameRate, false) ? 1 : 0;
        c.store (v, std::memory_order_relaxed);
    }
    return v == 1;
}

void setFrameRateCapped (bool b)
{
    setBool (capFrameRate, b);
    frameCapCache().store (b ? 1 : 0, std::memory_order_relaxed);
}

void refreshCache()
{
    frameCapCache().store (getBool (capFrameRate, false) ? 1 : 0, std::memory_order_relaxed);
}

} // namespace st::userSettings
