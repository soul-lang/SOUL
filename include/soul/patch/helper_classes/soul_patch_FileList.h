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
/** Manages a list of the known files in a patch, and provides methods for
    checking them for changes.
*/
struct FileList
{
    //==============================================================================
    void initialiseFromManifestFile (VirtualFile::Ptr manifestFileToOpen)
    {
        if (auto name = String::Ptr (manifestFileToOpen->getName()))
        {
            manifestName = name.toString<std::string>();

            if (choc::text::endsWith (manifestName, getManifestSuffix()))
            {
                manifestFile = manifestFileToOpen;
                root = VirtualFile::Ptr (manifestFile->getParent());
            }
        }
    }

    //==============================================================================
    /** A wrapper for a VirtualFile which keeps a few extra details alongside it. */
    struct FileState
    {
        VirtualFile::Ptr file;
        std::string path;
        int64_t lastModificationTime = 0;

        int64_t getSize() const                             { return file->getSize(); }
        int64_t getLastModificationTime() const             { return file->getLastModificationTime(); }
        bool hasFileBeenModified() const                    { return lastModificationTime != getLastModificationTime(); }
        bool hasChanged (const FileState& other) const      { return path != other.path || lastModificationTime != other.lastModificationTime; }
        bool operator< (const FileState& other) const       { return path < other.path; }
    };

    //==============================================================================
    VirtualFile::Ptr manifestFile, root;
    std::string manifestName;
    FileState manifest;
    std::vector<FileState> sourceFiles, filesToWatch;
    choc::value::Value manifestJSON;

    void reset()
    {
        manifest = {};
        manifestJSON = choc::value::Value();
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
            if (auto f = VirtualFile::Ptr (root->getChildFile (relativePath.c_str())))
                return f;

        throwPatchLoadError ("Cannot find file " + choc::text::addDoubleQuotes (relativePath));
    }

    FileState checkAndCreateFileState (const std::string& relativePath) const
    {
        FileState fs { checkAndCreateVirtualFile (relativePath), relativePath, 0 };
        fs.lastModificationTime = fs.getLastModificationTime();
        return fs;
    }

    void findManifestFile()
    {
        if (manifestFile == nullptr || ! choc::text::endsWith (manifestName, getManifestSuffix()))
            throwPatchLoadError ("Expected a .soulpatch file");

        manifest = { manifestFile, manifestName, 0 };
        manifest.lastModificationTime = manifest.getLastModificationTime();
        filesToWatch.push_back (manifest);
    }

    void parseManifest()
    {
        std::string error;
        manifestJSON = parseManifestFile (*manifest.file, error);

        if (! error.empty())
            throwPatchLoadError (std::move (error));

        checkExternalsList();
    }

    std::vector<FileState> getFileListProperty (const std::string& propertyName)
    {
        std::vector<FileState> result;
        std::vector<std::string> paths;

        auto addFile = [&] (const choc::value::ValueView& file)
        {
            if (! file.isString())
                throwPatchLoadError (manifest.path + ": error: Expected the '" + propertyName + "' variable to be a filename or array of files");

            paths.push_back (std::string (file.getString()));
        };

        auto files = manifestJSON[propertyName];

        if (files.isArray())
        {
            for (auto s : files)
                addFile (s);
        }
        else if (files.isString())
        {
            addFile (files);
        }

        for (auto& p : paths)
            result.push_back (checkAndCreateFileState (p));

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

    void addSource (BuildBundle& build, SourceFilePreprocessor* preprocessor)
    {
        for (auto& fileState : sourceFiles)
        {
            VirtualFile::Ptr source;

            if (preprocessor != nullptr)
                source = VirtualFile::Ptr (preprocessor->preprocessSourceFile (*fileState.file));

            if (source == nullptr)
                source = fileState.file;

            std::string readError;
            auto content = loadVirtualFileAsString (*source, readError);

            if (! readError.empty())
                throwPatchLoadError (std::move (readError));

            build.sourceFiles.push_back ({ fileState.path, std::move (content) });
        }
    }

    bool haveAnyReferencedFilesBeenModified() const
    {
        for (auto& f : filesToWatch)
            if (f.hasFileBeenModified())
                return true;

        return false;
    }

    choc::value::ValueView getExternalsList() const
    {
        return manifestJSON["externals"];
    }

    void checkExternalsList()
    {
        auto externals = getExternalsList();

        if (externals.isVoid())
            return;

        if (! externals.isObject())
            throwPatchLoadError ("The 'externals' field in the manifest must be a JSON object");

        externals.visitObjectMembers ([] (std::string_view memberName, const choc::value::ValueView&)
        {
            auto name = choc::text::trim (memberName);

            Identifier::Pool tempAllocator;
            auto path = IdentifierPath::fromString (tempAllocator, std::string (name));

            if (! path.isValid())
                throwPatchLoadError ("Invalid symbol name for external binding " + quoteName (std::string (name)));

            if (path.isUnqualified())
                throwPatchLoadError ("The external symbol name " + quoteName (std::string (name)) + " must include the name of the processor");
        });
    }

    bool hasChanged() const
    {
        FileList newList;
        newList.manifestFile = manifestFile;
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

private:
    struct DescriptionImpl final  : public RefCountHelper<Description, DescriptionImpl>
    {
        DescriptionImpl (VirtualFile::Ptr m, std::string desc)
            : manifestHolder (std::move (m)), stringDescription (std::move (desc))
        {
            this->manifestFile = manifestHolder.get();
            updatePointers();
        }

        DescriptionImpl (VirtualFile::Ptr m, const choc::value::ValueView& json)
            : DescriptionImpl (std::move (m), std::string())
        {
            if (json.isObject())
            {
                stringDescription    = json["description"].getWithDefault<std::string> ({});
                stringUID            = json["ID"].getWithDefault<std::string> ({});
                stringVersion        = json["version"].getWithDefault<std::string> ({});
                stringName           = json["name"].getWithDefault<std::string> ({});
                stringCategory       = json["category"].getWithDefault<std::string> ({});
                stringManufacturer   = json["manufacturer"].getWithDefault<std::string> ({});
                stringURL            = json["URL"].getWithDefault<std::string> ({});
                this->isInstrument   = json["isInstrument"].getWithDefault<bool> (false);

                updatePointers();
            }
        }

        void updatePointers()
        {
            this->description   = stringDescription.c_str();
            this->UID           = stringUID.c_str();
            this->version       = stringVersion.c_str();
            this->name          = stringName.c_str();
            this->category      = stringCategory.c_str();
            this->manufacturer  = stringManufacturer.c_str();
            this->URL           = stringURL.c_str();
        }

        DescriptionImpl (const DescriptionImpl&) = delete;
        DescriptionImpl (DescriptionImpl&&) = delete;

        VirtualFile::Ptr manifestHolder;
        std::string stringUID, stringVersion, stringName, stringDescription, stringCategory, stringManufacturer, stringURL;
    };

public:
    Description* createDescription() const
    {
        return new DescriptionImpl (manifest.file, manifestJSON);
    }

    Description::Ptr createDescriptionPtr() const
    {
        return Description::Ptr (createDescription());
    }

    Description::Ptr createDescriptionWithMessage (std::string desc) const
    {
        return Description::Ptr (new DescriptionImpl (manifest.file, std::move (desc)));
    }
};

}
