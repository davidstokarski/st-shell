#pragma once

namespace st
{

/** The company every ST plug-in is published under: shown in hosts (cmake ST_COMPANY_NAME, keep them equal) and
    the name of the folders the plug-ins keep their files in (presets, shared settings, caches). */
inline constexpr const char* kCompanyName = "Stokarski Studios";
/** The company name before the rename. Plug-in identity (manufacturer code StAu, bundle ids com.staudio.*) is
    unchanged so sessions still load; only the folders named after it move (see adoptLegacyFolder). */
inline constexpr const char* kLegacyCompanyName = "ST Audio";

/** Moves what's in `legacy` into `dir` (files `dir` already has are kept), then deletes `legacy` and, if that
    leaves it empty, its parent (the old company folder). Call before using `dir`; a no-op once moved. */
inline void adoptLegacyFolder (const juce::File& dir, const juce::File& legacy)
{
    if (! legacy.isDirectory() || legacy == dir) return;
    dir.createDirectory();
    for (const auto& entry : juce::RangedDirectoryIterator (legacy, false, "*", juce::File::findFilesAndDirectories))
    {
        const auto& f = entry.getFile();
        const auto target = dir.getChildFile (f.getFileName());
        if (target.exists())
        {
            if (f.isDirectory() && target.isDirectory()) adoptLegacyFolder (target, f);   // merge sub-folders
            continue;
        }
        f.moveFileTo (target);
    }
    if (legacy.getNumberOfChildFiles (juce::File::findFilesAndDirectories, "*") == 0)
    {
        legacy.deleteRecursively();
        const auto company = legacy.getParentDirectory();
        if (company.getFileName() == kLegacyCompanyName
            && company.getNumberOfChildFiles (juce::File::findFilesAndDirectories, "*") == 0)
            company.deleteRecursively();
    }
}

} // namespace st
