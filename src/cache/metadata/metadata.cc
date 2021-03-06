// vim:set noet cinoptions= sw=4 ts=4:
// This file is part of the eix project and distributed under the
// terms of the GNU General Public License v2.
//
// Copyright (c)
//   Wolfgang Frisch <xororand@users.sourceforge.net>
//   Emil Beinroth <emilbeinroth@gmx.net>
//   Martin Väth <martin@mvath.de>

#include <config.h>

#include <dirent.h>

#include <cstdlib>
#include <cstring>

#include <string>

#include "cache/common/assign_reader.h"
#include "cache/common/flat_reader.h"
#include "cache/metadata/metadata.h"
#include "eixTk/formated.h"
#include "eixTk/i18n.h"
#include "eixTk/likely.h"
#include "eixTk/null.h"
#include "eixTk/stringtypes.h"
#include "eixTk/stringutils.h"
#include "eixTk/utils.h"
#include "portage/basicversion.h"
#include "portage/extendedversion.h"
#include "portage/package.h"
#include "portage/packagetree.h"

using std::string;

/* Subpath to metadata cache */
#define METADATA_PATH		"metadata/cache"
#define METADATAMD5_PATH	"metadata/md5-cache"
/* Path to portage-2.0 cache */
#define PORTAGE_CACHE_PATH	"var/cache/edb/dep"

static int cachefiles_selector(SCANDIR_ARG3 dent);

bool MetadataCache::use_prefixport() const {
	switch(path_type) {
		case PATH_REPOSITORY:
		case PATH_FULL:
			return false;
/*
		case PATH_METADATA:
		case PATH_METADATAMD5:
		case PATH_METADATAMD5OR:
*/
		default:
			return true;
	}
}

bool MetadataCache::initialize(const string& name) {
	string pure_name(name);
	string::size_type i(pure_name.find(':'));
	if(i != string::npos) {
		pure_name.erase(i);
		have_override_path = true;
		override_path.assign(name, i + 1, string::npos);
	} else {
		have_override_path = false;
	}
	checkmd5 = false;
	if(casecontains(pure_name, "metadata")) {
		bool s_flat(casecontains(pure_name, "flat"));
		bool s_assign((!s_flat) && casecontains(pure_name, "assign"));
		if(casecontains(pure_name, "md5")) {
			if(s_flat) {
				if(have_override_path) {
					setType(PATH_METADATAMD5, false);
				} else {
					setType(PATH_METADATAMD5OR, true);
				}
			} else if(s_assign) {
				if(have_override_path) {
					setType(PATH_METADATAMD5, false);
				} else {
					setType(PATH_METADATAMD5OR, false);
				}
			} else {
				checkmd5 = true;
				setType(PATH_METADATAMD5, false);
			}
		} else {
			setType(PATH_METADATA, s_assign ||
				(pure_name.find('*') != string::npos));
		}
		return true;
	}
	if(casecontains(pure_name, "repo")) {
		if(casecontains(pure_name, "flat")) {
			setType(PATH_REPOSITORY, true);
			return true;
		}
		if(casecontains(pure_name, "assign")) {
			setType(PATH_REPOSITORY, false);
			return true;
		}
	}
	if(caseequal(pure_name, "flat") ||
		casecontains(pure_name, "portage-2.0")) {
		setType(PATH_FULL, true);
		return true;
	}
	if(caseequal(pure_name, "assign") ||
		casecontains(pure_name, "backport") ||
		casecontains(pure_name, "portage-2.1")) {
		setType(PATH_FULL, false);
		return true;
	}
	return false;
}

void MetadataCache::setType(PathType set_path_type, bool set_flat) {
	path_type = set_path_type;
	bool append_flattype(true);
	switch(set_path_type) {
		case PATH_METADATA:
			m_type = "metadata-";
			break;
		case PATH_METADATAMD5:
			m_type = "metadata-md5";
			append_flattype = false;
			break;
		case PATH_METADATAMD5OR:
			m_type = "metadata-md5-or-";
			break;
		case PATH_REPOSITORY:
			m_type = "repo-";
			break;
		case PATH_FULL:
		default:
			m_type.clear();
	}
	flat = set_flat;
	if(append_flattype) {
		m_type.append(set_flat ? "flat" : "assign");
	}
	if(have_override_path) {
		m_type.append(1, ':');
		m_type.append(override_path);
	}
	setFlat(set_flat);
}

void MetadataCache::setFlat(bool set_flat) {
	if(set_flat) {
		x_get_keywords_slot_iuse_restrict = flat_get_keywords_slot_iuse_restrict;
		x_read_file = flat_read_file;
	} else {
		x_get_keywords_slot_iuse_restrict = assign_get_keywords_slot_iuse_restrict;
		x_read_file = assign_read_file;
	}
}

static int cachefiles_selector(SCANDIR_ARG3 dent) {
	return ((dent->d_name[0] != '.')
			&& (strchr(dent->d_name, '-') != NULLPTR));
}

bool MetadataCache::readCategoryPrepare(const char *cat_name) {
	string alt;
	m_catname = cat_name;
	if(have_override_path) {
		m_catpath = override_path;
	} else {
		m_catpath = m_prefix;
		switch(path_type) {
			case PATH_METADATA:
			case PATH_METADATAMD5:
			case PATH_METADATAMD5OR:
				// m_scheme is actually the portdir
				m_catpath.append(m_scheme);
				optional_append(&m_catpath, '/');
				if(path_type == PATH_METADATA) {
					m_catpath.append(METADATA_PATH);
				} else if(path_type == PATH_METADATAMD5) {
					m_catpath.append(METADATAMD5_PATH);
				} else {
					alt = m_catpath;
					m_catpath.append(METADATAMD5_PATH);
					alt.append(METADATA_PATH);
				}
				break;
/*
			case PATH_REPOSITORY:
			case PATH_FULL:
*/
			default:
				m_catpath = m_prefix;
				optional_append(&m_catpath, '/');
				m_catpath.append(PORTAGE_CACHE_PATH);
				break;
		}
	}
	switch(path_type) {
		case PATH_FULL:
			m_catpath.append(m_scheme);
			break;
		case PATH_REPOSITORY:
			optional_append(&m_catpath, '/');
			if(m_overlay_name.empty()) {
				// Paludis' way of resolving missing repo_name:
				m_catpath.append("x-");
				string::size_type p(m_scheme.size());
				while(p) {
					string::size_type c(m_scheme.rfind('/', p));
					if(c == string::npos) {
						m_catpath.append(m_scheme, 0, p);
						break;
					}
					if(c == --p)
						continue;
					m_catpath.append(m_scheme, c + 1, p - c);
					break;
				}
			} else {
				m_catpath.append(m_overlay_name);
			}
			break;
/*
		case PATH_METADATA:
		case PATH_METADATAMD5:
		case PATH_METADATAMD5OR:
*/
		default:
			break;
	}
	optional_append(&m_catpath, '/');
	m_catpath.append(cat_name);

	bool r(scandir_cc(m_catpath, &names, cachefiles_selector));
	if(path_type != PATH_METADATAMD5OR) {
		return r;
	}
	// PATH_METADATAMD5OR:
	if(r) {  // We had found category in METADATAMD5_PATH
		if(flat) {  // We "jump" to non-flat PATH_METADATAMD5 mode:
			setFlat(false);
		}
		return true;
	}
	// We choose metadata-flat or metadata-assign:
	m_catpath = alt;
	optional_append(&m_catpath, '/');
	m_catpath.append(cat_name);
	if(flat) {  // We "jump" to flat PATH_METADATA mode:
		setFlat(true);
	}
	return scandir_cc(m_catpath, &names, cachefiles_selector);
}

void MetadataCache::readCategoryFinalize() {
	m_catname.clear();
	m_catpath.clear();
	names.clear();
}

void MetadataCache::get_version_info(const char *pkg_name, const char *ver_name, Version *version) const {
	string keywords, iuse, restr, props, slot;
	string path(m_catpath);
	path.append(1, '/');
	path.append(pkg_name);
	path.append(1, '-');
	path.append(ver_name);
	(*x_get_keywords_slot_iuse_restrict)(path, &keywords, &slot, &iuse, &restr, &props, &(version->depend), m_error_callback);
	version->set_slotname(slot);
	version->set_full_keywords(keywords);
	version->set_iuse(iuse);
	version->set_restrict(restr);
	version->set_properties(props);
	version->overlay_key = m_overlay_key;
}

const char *MetadataCache::get_md5sum(const char *pkg_name, const char *ver_name) const {
	if(!checkmd5) {
		return NULLPTR;
	}
	return assign_get_md5sum(m_catpath + "/" + pkg_name + "-" + ver_name);
}

bool MetadataCache::readCategory(Category *cat) {
	for(WordVec::const_iterator it(names.begin());
		likely(it != names.end()); ) {
		Version *newest(NULLPTR);
		string neweststring;

		/* Split string into package and version, and catch any errors. */
		char **aux(ExplodeAtom::split(it->c_str()));
		if(unlikely(aux == NULLPTR)) {
			m_error_callback(eix::format(_("cannot split %r into package and version")) % (*it));
			++it;
			continue;
		}

		/* Search for existing package */
		Package *pkg(cat->findPackage(aux[0]));

		/* If none was found create one */
		if(pkg == NULLPTR) {
			pkg = cat->addPackage(m_catname, aux[0]);
		}

		for(;;) {
			/* Make version and add it to package. */
			Version *version(new Version);
			string errtext;
			BasicVersion::ParseResult r(version->parseVersion(aux[1], &errtext));
			if(unlikely(r != BasicVersion::parsedOK)) {
				m_error_callback(errtext);
			}
			if(unlikely(r == BasicVersion::parsedError)) {
				delete version;
			} else {
				get_version_info(aux[0], aux[1], version);

				pkg->addVersion(version);
				if(*(pkg->latest()) == *version) {
					newest = version;
					neweststring = aux[1];
				}
			}

			/* Free old split */
			free(aux[0]);
			free(aux[1]);

			/* If this is the last file we break so we can get the full
			 * information after this while-loop. If we still have more files
			 * ahead we can just read the next file. */
			if(++it == names.end())
				break;

			/* Split new filename into package and version, and catch any errors. */
			aux = ExplodeAtom::split(it->c_str());
			if(unlikely(aux == NULLPTR)) {
				m_error_callback(eix::format(_("cannot split %r into package and version")) % (*it));
				++it;
				break;
			}
			if(strcmp(aux[0], pkg->name.c_str())) {
				free(aux[0]);
				free(aux[1]);
				break;
			}
		}

		/* Read the cache file of the last version completely */
		if(newest)  // provided we have read the "last" version
			get_common_info((pkg->name).c_str(), neweststring.c_str(), pkg);
	}
	return true;
}
