// vim:set noet cinoptions= sw=4 ts=4:
// This file is part of the eix project and distributed under the
// terms of the GNU General Public License v2.
//
// Copyright (c)
//   Wolfgang Frisch <xororand@users.sourceforge.net>
//   Emil Beinroth <emilbeinroth@gmx.net>
//   Martin Väth <vaeth@mathematik.uni-wuerzburg.de>

#ifndef EIX__CASCADINGPROFILE_H__
#define EIX__CASCADINGPROFILE_H__ 1

#include <eixTk/null.h>
#include <portage/mask.h>
#include <portage/mask_list.h>
#include <portage/overlay.h>

#include <string>
#include <vector>

class Package;
class PortageSettings;
class ProfileFilenames;

class ProfileFile {
		std::string filename;
	public:
		OverlayIdent overlay;

		ProfileFile(const std::string &s, const char *path = NULLPTR) :
			filename(s), overlay(path)
		{ }

		const std::string &name() const
		{ return filename; }

		const char *c_str() const
		{ return filename.c_str(); }

		/// This has a side effect of reading the repository
		bool have_repo();

		/// Must call have_repo() (with return true) before
		const std::string &get_repo() const
		{ return overlay.label; }
};

/** Access to the cascading profile pointed to by /etc/make.profile. */
class CascadingProfile {
		friend class ProfileFilenames;
	public:
		bool use_world, finalized;
		MaskList<Mask> m_world;          /**< Packages in world. This must be set externally */

	protected:
		bool m_init_world;
		std::vector<ProfileFile> m_profile_files; /**< List of files in profile. */
		PortageSettings *m_portagesettings; /**< Profilesettings to which this instance "belongs" */

		MaskList<Mask> m_system;         /**< Packages in system profile. */
		MaskList<Mask> m_system_allowed; /**< Packages that are not in system profile but only allowed to have specific versions.*/
		MaskList<Mask> m_package_masks;  /**< Masks from package.mask */
		MaskList<Mask> m_package_unmasks;/**< Masks from package.unmask */
		MaskList<PKeywordMask> m_package_keywords;/**< Masks from package.keywords */
		MaskList<KeywordMask> m_package_accept_keywords;/**< Masks from package.accept_keywords */
		PreList p_system, p_system_allowed, p_package_masks, p_package_unmasks,
			p_package_keywords, p_package_accept_keywords;

	private:

		/** Add all files from profile and its parents to m_profile_files. */
		bool addProfile(const char *profile, unsigned int depth = 0);

		/** Handler functions follow for reading a file */
		typedef bool (CascadingProfile::*Handler)(const std::vector<std::string> &lines, const std::string &filename, const char *repo);

		/** Read all "packages" files found in profile.
		 * Populate p_system and p_system_allowed.
		 * @return true if data was changed */
		bool readPackages(const std::vector<std::string> &lines, const std::string &filename, const char *repo);

		/** Read all "package.mask" files found in profile.
		 * Populate p_package_masks.
		 * @return true if data was changed */
		bool readPackageMasks(const std::vector<std::string> &lines, const std::string &filename, const char *repo);

		/** Read all "package.unmask" files found in profile.
		 * Populate p_package_unmasks.
		 * @return true if data was changed */
		bool readPackageUnmasks(const std::vector<std::string> &lines, const std::string &filename, const char *repo);

		/** Read all "package.keywords" files found in profile.
		 * Populate p_package_keywords.
		 * @return true if data was changed */
		bool readPackageKeywords(const std::vector<std::string> &lines, const std::string &filename, const char *repo);

		/** Read all "package.accept_keywords" files found in profile.
		 * Populate p_package_accept_keywords.
		 * @return true if data was changed */
		bool readPackageAcceptKeywords(const std::vector<std::string> &lines, const std::string &filename, const char *repo);
	public:
		CascadingProfile(PortageSettings *portagesettings, bool init_world) :
			use_world(false), finalized(false),
			m_init_world(init_world),
			m_portagesettings(portagesettings)
		{ }

		/** Populate MaskLists from PreLists.
		    All files must have been read and m_raised_arch
		    must be known when this is called. */
		void finalize();

		/** Read all "make.defaults" files previously added by listadd... */
		void readMakeDefaults();

		/** Read all mask/system files previously added by listadd...
		 * and clear this list of files afterwards.
		 * @return true if at least one file changed data. */
		bool readremoveFiles();

		/** Cycle through profile and put path to files into
		 * m_profile_files. */
		void listaddProfile(const char *profile_dir = NULLPTR);

		/** Put file into m_profile_files */
		void listaddFile(const std::string &file, const char *path)
		{ m_profile_files.push_back(ProfileFile(file, path)); }

		/** Clear m_profile_files */
		void listclear()
		{ m_profile_files.clear(); }

		void applyMasks(Package *p) const;
		void applyKeywords(Package *p) const;
};


#endif /* EIX__CASCADINGPROFILE_H__ */
