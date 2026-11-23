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

} // namespace st::userSettings
