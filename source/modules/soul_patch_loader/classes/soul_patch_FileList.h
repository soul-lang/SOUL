/*
    _____ _____ _____ __
   |   __|     |  |  |  |      The SOUL language
   |__   |  |  |  |  |  |__    Copyright (c) 2019 - ROLI Ltd.
   |_____|_____|_____|_____|

   The code in this file is provided under the terms of the ISC license:

   Permission to use, copy, modify, and/or distribute this software for any purpose
   with or without fee is hereby granted, provided that the above copyright notice and
   this permission notice appear in all copies.

   THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD
   TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN
   NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
   DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER
   IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
   CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

namespace soul::patch
{

//==============================================================================
/** A wrapper for a VirtualFile which keeps a few extra details alongside it. */
struct FileState
{
    VirtualFile::Ptr file;
    juce::String path;
    int64_t lastModificationTime = 0;

    int64_t getSize() const                             { return file->getSize(); }
    int64_t getLastModificationTime() const             { return file->getLastModificationTime(); }
    bool hasFileBeenModified() const                    { return lastModificationTime != getLastModificationTime(); }
    bool hasChanged (const FileState& other) const      { return path != other.path || lastModificationTime != other.lastModificationTime; }
    bool operator< (const FileState& other) const       { return path < other.path; }
};

//==============================================================================
/** Manages a list of the known files in a patch, and provides methods for
    checking them for changes.
*/
struct FileList
{
    VirtualFile::Ptr root;
    std::string manifestName;
    FileState manifest;
    std::vector<FileState> sourceFiles, filesToWatch;
    juce::var manifestJSON;

    void reset()
    {
        manifest = {};
        manifestJSON = juce::var();
        sourceFiles.clear();
        filesToWatch.clear();
    }

    void refresh()
    {
        reset();
        findManifestFile();
        parseManifest();
        findSourceFiles();
        findViewFiles();
    }

    VirtualFile::Ptr checkAndCreateVirtualFile (const std::string& relativePath) const
    {
        if (relativePath.empty())
            throwPatchLoadError ("Empty file name");

        if (root != nullptr)
            if (auto f = root->getChildFile (relativePath.c_str()))
                return f;

        throwPatchLoadError ("Cannot find file " + addDoubleQuotes (relativePath));
    }

    FileState checkAndCreateFileState (const std::string& relativePath) const
    {
        FileState fs { checkAndCreateVirtualFile (relativePath), relativePath, 0 };
        fs.lastModificationTime = fs.getLastModificationTime();
        return fs;
    }

    void findManifestFile()
    {
        if (root == nullptr || ! endsWith (manifestName, getManifestSuffix()))
            throwPatchLoadError ("Expected a .soulpatch file");

        manifest = checkAndCreateFileState (manifestName);
        filesToWatch.push_back (manifest);
    }

    void parseManifest()
    {
        auto result = parseManifestFile (*manifest.file, manifestJSON);

        if (result.failed())
            throwPatchLoadError ((manifest.path + ": " + result.getErrorMessage()).toStdString());

        checkExternalsList();
    }

    std::vector<FileState> getFileListProperty (const juce::String& propertyName)
    {
        std::vector<FileState> result;
        juce::StringArray paths;

        auto addFile = [&] (const juce::var& file)
        {
            if (! file.isString())
                throwPatchLoadError (manifest.path, "Expected the '" + propertyName + "' variable to be a filename or array of files");

            paths.add (file);
        };

        auto files = manifestJSON.getProperty (propertyName, {});

        if (! files.isVoid())
        {
            if (files.isArray())
            {
                for (auto& s : *files.getArray())
                    addFile (s);
            }
            else
            {
                addFile (files);
            }

            for (auto& p : paths)
                result.push_back (checkAndCreateFileState (p.toStdString()));
        }

        return result;
    }

    void findSourceFiles()
    {
        auto files = getFileListProperty ("source");
        sourceFiles = files;
        appendVector (filesToWatch, files);
    }

    void findViewFiles()
    {
        appendVector (filesToWatch, getFileListProperty ("view"));
    }

    bool haveAnyReferencedFilesBeenModified() const
    {
        for (auto& f : filesToWatch)
            if (f.hasFileBeenModified())
                return true;

        return false;
    }

    juce::DynamicObject* getExternalsList() const
    {
        return manifestJSON["externals"].getDynamicObject();
    }

    void checkExternalsList()
    {
        auto externalsVar = manifestJSON["externals"];

        if (! (externalsVar.isVoid() || externalsVar.isObject()))
            throwPatchLoadError ("The 'externals' field in the manifest must be a JSON object");

        if (auto externals = getExternalsList())
        {
            for (auto& e : externals->getProperties())
            {
                auto name = e.name.toString().trim().toStdString();

                Identifier::Pool tempAllocator;
                auto path = IdentifierPath::fromString (tempAllocator, name);

                if (! path.isValid())
                    throwPatchLoadError ("Invalid symbol name for external binding " + quoteName (name));

                if (path.isUnqualified())
                    throwPatchLoadError ("The external symbol name " + quoteName (name) + " must include the name of the processor");
            }
        }
    }

    bool hasChanged() const
    {
        FileList newList;
        newList.root = root;
        newList.manifestName = manifestName;

        try
        {
            newList.findManifestFile();
        }
        catch (const PatchLoadError&) {}

        return manifest.hasChanged (newList.manifest)
                 || haveAnyReferencedFilesBeenModified();
    }

    int64_t getMostRecentModificationTime() const
    {
        int64_t mostRecent = -1;

        for (auto& f : filesToWatch)
            mostRecent = std::max (mostRecent, f.getLastModificationTime());

        return mostRecent;
    }

    Description createDescription() const
    {
        Description d;

        d.manifestFile   = manifest.file;
        d.UID            = makeString (manifestJSON["ID"]);
        d.version        = makeString (manifestJSON["version"]);
        d.name           = makeString (manifestJSON["name"]);
        d.description    = makeString (manifestJSON["description"]);
        d.category       = makeString (manifestJSON["category"]);
        d.manufacturer   = makeString (manifestJSON["manufacturer"]);
        d.URL            = makeString (manifestJSON["URL"]);
        d.isInstrument   = manifestJSON["isInstrument"];

        return d;
    }
};

}
