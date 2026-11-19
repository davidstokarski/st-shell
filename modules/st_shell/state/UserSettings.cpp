namespace st::userSettings
{

static juce::PropertiesFile& file()
{
    static std::unique_ptr<juce::PropertiesFile> f = []
    {
        juce::PropertiesFile::Options o;
        o.applicationName = "settings";
        o.folderName = "ST Audio";
        o.filenameSuffix = "settings";
        o.osxLibrarySubFolder = "Application Support";
        o.millisecondsBeforeSaving = 0;   // other ST plug-ins read it too
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

} // namespace st::userSettings
