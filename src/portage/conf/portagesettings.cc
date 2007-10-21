// This file is part of the eix project and distributed under the
// terms of the GNU General Public License v2.
//
// Copyright (c)
//   Wolfgang Frisch <xororand@users.sourceforge.net>
//   Emil Beinroth <emilbeinroth@gmx.net>
//   Martin V�th <vaeth@mathematik.uni-wuerzburg.de>

#include "config.h"

#include "portagesettings.h"

#include <portage/conf/cascadingprofile.h>

#include <portage/mask.h>
#include <portage/package.h>
#include <portage/version.h>

#include <eixTk/utils.h>
#include <eixTk/stringutils.h>
#include <eixTk/filenames.h>

#include <eixrc/eixrc.h>

#include <varsreader.h>

#include <fstream>
#include <fnmatch.h>


using namespace std;

bool grab_masks(const char *file, Mask::Type type, MaskList<Mask> *cat_map, vector<Mask*> *mask_vec, bool recursive)
{
	vector<string> lines;
	if( ! pushback_lines(file, &lines, true, recursive))
		return false;
	for(vector<string>::iterator it=lines.begin(); it<lines.end(); ++it)
	{
		string line=*it;
		try {
			Mask *m = new Mask(line.c_str(), type);
			OOM_ASSERT(m);
			if(cat_map) {
				cat_map->add(m);
			}
			else {
				mask_vec->push_back(m);
			}
		}
		catch(const ExBasic &e) {
			cerr << "-- Invalid line in " << file << ": \"" << line << "\"" << endl
			     << "   " << e.getMessage() << endl;
		}
	}
	return true;
}


/** Keys that should accumulate their content rathern then replace. */
static const char *default_accumulating_keys[] = {
	"USE",
	"CONFIG_*",
	"FEATURES",
	"ACCEPT_KEYWORDS",
	NULL
};

/** Environment variables which should take effect before reading profiles. */
static const char *test_in_env_early[] = {
	"PORTAGE_PROFILE",
	"PORTDIR",
	"PORTDIR_OVERLAY",
	NULL
};

/** Environment variables which should add/override all other settings. */
static const char *test_in_env_late[] = {
	"USE",
	"CONFIG_PROTECT",
	"CONFIG_PROTECT_MASK",
	"FEATURES",
	"ARCH",
	"ACCEPT_KEYWORDS",
	NULL
};

inline static bool is_accumulating(const char **accumulating, const char *key)
{
	const char *match;
	while((match = *(accumulating++)) != NULL) {
		if(fnmatch(match, key, 0) == 0)
			return true;
	}
	return false;
}

void PortageSettings::override_by_env(const char **vars)
{
	const char *var;
	while((var = *(vars++)) != NULL)
	{
		const char *e = getenv(var);
		if(!e)
			continue;
		if(!is_accumulating(default_accumulating_keys, var))
		{
			(*this)[var] = e;
			continue;
		}
		string &ref = ((*this)[var]);
		if(ref.empty())
			ref = e;
		else
			ref.append(string("\n") + e);
	}
}

void PortageSettings::read_config(const string &name, const string &prefix)
{
	VarsReader configfile(VarsReader::SUBST_VARS|VarsReader::INTO_MAP|VarsReader::APPEND_VALUES|VarsReader::ALLOW_SOURCE);
	configfile.accumulatingKeys(default_accumulating_keys);
	configfile.useMap(this);
	configfile.setPrefix(prefix);
	configfile.read(name.c_str());
}

string PortageSettings::resolve_overlay_name(const string &path, bool resolve)
{
	if(resolve) {
		string full = m_eprefixoverlays;
		full.append(path);
		return normalize_path(full.c_str(), true);
	}
	return normalize_path(path.c_str(), false);
}

void PortageSettings::add_overlay(string &path, bool resolve, bool modify)
{

	string name = resolve_overlay_name(path, resolve);
	if(modify)
		path = name;
	/* If the overlay exists, don't add it */
	if(find_filenames(overlays.begin(), overlays.end(),
			name.c_str(), false, false) != overlays.end())
			return;
	/* If the overlay is PORTDIR, don't add it */
	if(same_filenames((*this)["PORTDIR"].c_str(), name.c_str(), false, false))
		return;
	overlays.push_back(name);
}

void PortageSettings::add_overlay_vector(vector<string> &v, bool resolve, bool modify)
{
	for(vector<string>::iterator it = v.begin(); it != v.end(); ++it)
		add_overlay(*it, resolve, modify);
}

/** Read make.globals and make.conf. */
PortageSettings::PortageSettings(EixRc &eixrc, bool getlocal)
{
	m_obsolete_minusasterisk = eixrc.getBool("OBSOLETE_MINUSASTERISK");
	m_eprefixconf     = eixrc.m_eprefixconf;
	m_eprefixprofile  = eixrc["EPREFIX_PORTAGE_PROFILE"];
	m_eprefixportdir  = eixrc["EPREFIX_PORTDIR"];
	m_eprefixoverlays = eixrc["EPREFIX_OVERLAYS"];
	m_eprefixaccessoverlays = eixrc["EPREFIX_ACCESS_OVERLAYS"];

	const string &eprefixsource = eixrc["EPREFIX_SOURCE"];
	read_config(m_eprefixconf + MAKE_GLOBALS_FILE, eprefixsource);
	read_config(m_eprefixconf + MAKE_CONF_FILE, eprefixsource);

	override_by_env(test_in_env_early);
	/* Normalize "PORTDIR": */
	{
		string &ref = (*this)["PORTDIR"];
		string full = m_eprefixportdir;
		if(ref.empty())
			full.append("/usr/portage");
		else
			full.append(ref);
		ref = normalize_path(full.c_str(), true);
		if(ref[ref.size() - 1] != '/')
			ref.append("/");
	}
	/* Normalize overlays and erase duplicates */
	{
		string &ref = (*this)["PORTDIR_OVERLAY"];
		vector<string> overlayvec = split_string(ref);
		add_overlay_vector(overlayvec, true, true);
		ref = join_vector(overlayvec);
	}

	profile     = new CascadingProfile(this);
	profile->listaddFile(((*this)["PORTDIR"] + PORTDIR_MASK_FILE).c_str());
	profile->listaddProfile();
	profile->readMakeDefaults();
	profile->readremoveFiles();
	CascadingProfile *local_profile = NULL;
	if(getlocal)
		local_profile = new CascadingProfile(*profile);
	addOverlayProfiles(profile);
	if(getlocal) {
		local_profile->listaddProfile((m_eprefixconf + USER_PROFILE_DIR).c_str());
		local_profile->readMakeDefaults();
		if(local_profile->readremoveFiles()) {
			addOverlayProfiles(local_profile);
			local_profile->readMakeDefaults();
			local_profile->readremoveFiles();
		}
		else {
			delete local_profile;
			local_profile = NULL;
		}
		profile->readremoveFiles();
	}
	else {
		profile->readMakeDefaults();
		profile->readremoveFiles();
		user_config = NULL;
	}
	override_by_env(test_in_env_late);

	m_accepted_keywords = split_string((*this)["ARCH"]);
	resolve_plus_minus(m_arch_set, m_accepted_keywords, m_obsolete_minusasterisk);
	push_backs<string>(m_accepted_keywords, split_string((*this)["ACCEPT_KEYWORDS"]));
	resolve_plus_minus(m_accepted_keywords_set, m_accepted_keywords, m_obsolete_minusasterisk);
	make_vector<string>(m_accepted_keywords, m_accepted_keywords_set);
	if(eixrc.getBool("ACCEPT_KEYWORDS_AS_ARCH"))
		m_local_arch_set = &m_accepted_keywords_set;
	else
		m_local_arch_set = &m_arch_set;

	if(getlocal)
		user_config = new PortageUserConfig(this, local_profile);
}

PortageSettings::~PortageSettings()
{
	if(profile) {
		delete profile;
	}
	if(user_config) {
		delete user_config;
	}
}

/** Return vector of all possible categories.
 * Reads categories on first call. */
vector<string> *PortageSettings::getCategories()
{
	if(m_categories.empty()) {
		/* Merge categories from /etc/portage/categories and
		 * portdir/profile/categories */
		pushback_lines((m_eprefixconf + USER_CATEGORIES_FILE).c_str(), &m_categories);

		pushback_lines(((*this)["PORTDIR"] + PORTDIR_CATEGORIES_FILE).c_str(), &m_categories);
		for(vector<string>::iterator i = overlays.begin();
			i != overlays.end();
			++i)
		{
			pushback_lines((m_eprefixaccessoverlays + (*i) + "/" + PORTDIR_CATEGORIES_FILE).c_str(),
			               &m_categories);
		}

		sort_uniquify(m_categories);
	}
	return &m_categories;
}

void
PortageSettings::addOverlayProfiles(CascadingProfile *p) const
{
	for(vector<string>::const_iterator i = overlays.begin();
		i != overlays.end(); ++i)
		p->listaddFile((m_eprefixaccessoverlays + (*i) + "/" + PORTDIR_MASK_FILE).c_str());
}

PortageUserConfig::PortageUserConfig(PortageSettings *psettings, CascadingProfile *local_profile)
{
	m_settings = psettings;
	profile    = local_profile;
	readKeywords();
	readMasks();
	read_use = read_cflags = false;
}

PortageUserConfig::~PortageUserConfig()
{
	if(profile) {
		delete profile;
	}
}

bool
PortageUserConfig::readMasks()
{
	bool mask_ok = grab_masks(((m_settings->m_eprefixconf) + USER_MASK_FILE).c_str(), Mask::maskMask, &m_localmasks, true);
	bool unmask_ok = grab_masks(((m_settings->m_eprefixconf) + USER_UNMASK_FILE).c_str(), Mask::maskUnmask, &m_localmasks, true);
	return mask_ok && unmask_ok;
}

void
PortageUserConfig::ReadVersionFile (const char *file, MaskList<KeywordMask> *list)
{
	vector<string> lines;
	pushback_lines(file, &lines, false, true);
	for(vector<string>::size_type i = 0;
		i<lines.size();
		i++)
	{
		if(lines[i].empty())
			continue;
		try {
			KeywordMask *m = NULL;
			string::size_type n = lines[i].find_first_of("\t ");
			if(n == string::npos) {
				m = new KeywordMask(lines[i].c_str());
			}
			else {
				m = new KeywordMask(lines[i].substr(0, n).c_str());
				if(m)
					m->keywords = "1"; //lines[i].substr(n + 1);
			}
			if(m)
				list->add(m);
		}
		catch(const ExBasic &e) { }
	}
}

/// @return true if some mask from list applied
bool PortageUserConfig::CheckList(Package *p, const MaskList<KeywordMask> *list, Keywords::Redundant flag_double, Keywords::Redundant flag_in)
{
	const eix::ptr_list<KeywordMask> *keyword_masks = list->get(p);
	map<Version*,char> sorted_by_versions;

	if(!keyword_masks)
		return false;
	if(keyword_masks->empty())
		return false;
	for(eix::ptr_list<KeywordMask>::const_iterator it = keyword_masks->begin();
		it != keyword_masks->end();
		++it)
	{
		eix::ptr_list<Version> matches = it->match(*p);

		for(eix::ptr_list<Version>::iterator  v = matches.begin();
			v != matches.end();
			++v)
		{
			if(it->keywords.empty())
				continue;
			char &s = sorted_by_versions[*v];
			if(s)
				s = 2;
			else
				s = 1;
		}
	}

	for(Package::iterator i = p->begin();
		i != p->end();
		++i)
	{
		char s = sorted_by_versions[*i];
		if(!s)
			continue;
		Keywords::Redundant redundant = flag_in | i->get_redundant();
		if(s == 2)
			redundant |= flag_double;
		i->set_redundant(redundant);
	}
	return true;
}

bool PortageUserConfig::CheckFile(Package *p, const char *file, MaskList<KeywordMask> *list, bool *readfile, Keywords::Redundant flag_double, Keywords::Redundant flag_in) const
{
	if(!(*readfile))
	{
		ReadVersionFile(((m_settings->m_eprefixconf) + file).c_str(), list);
		*readfile = true;
	}
	return CheckList(p, list, flag_double, flag_in);
}

typedef struct {
	string keywords;
	bool locally_double;
} KeywordsData;

bool PortageUserConfig::readKeywords() {
	// Prepend a ~ to every token.
	string fscked_arch;
	{
		vector<string> archvec;
		for(set<string>::const_iterator it = m_settings->m_arch_set.begin(); it != m_settings->m_arch_set.end(); ++it) {
			if(strchr("-~", (*it)[0]) == NULL) {
				archvec.push_back(string("~") + *it);
			}
		}
		sort_uniquify(archvec);
		fscked_arch = join_vector(archvec);
	}

	vector<string> lines;
	string filename((m_settings->m_eprefixconf) + USER_KEYWORDS_FILE);

	pushback_lines(filename.c_str(), &lines, false, true);

	/* Read only the last line for each "first" entry, e.g. in the example
		foo/bar 1
		foo/bar 2
		=foo/bar-1 3
		=foo/bar-1 4
	   the line 1 and 3 are ignored but 2 and 4 are both put to keywords
	   (even if they should influence each other).
	   This is strange, but this is the way portage does it.

	   We read in two passes, first creating the actual list in a map
	   (and remember BTW which were doubled) and then we push the map
	   in the original order to m_keywords */

	map<string, KeywordsData> have;
	for(vector<string>::size_type i = 0; i < lines.size(); ++i)
	{
		if(lines[i].empty())
			continue;

		string::size_type n = lines[i].find_first_of("\t ");
		string name, content;
		if(n == string::npos) {
			name = lines[i];
			content = fscked_arch;
		}
		else {
			name = lines[i].substr(0, n);
			content = lines[i].substr(n + 1);
		}
		lines[i] = name;
		map<string, KeywordsData>::iterator old = have.find(name);
		if(old == have.end()) {
			KeywordsData *f = &(have[name]);
			f->locally_double = false;
			f->keywords = content;
		}
		else {
			(old->second).locally_double = true;
			(old->second).keywords = content;
		}
	}

	for(vector<string>::size_type i = 0; i != lines.size(); ++i)
	{
		if(lines[i].empty())
			continue;
		try {
			KeywordMask *m = new KeywordMask(lines[i].c_str());
			if(m) {
				KeywordsData *f = &(have[lines[i]]);
				m->keywords       = f->keywords;
				m->locally_double = f->locally_double;
				m_keywords.add(m);
			}
		}
		catch(const ExBasic &e) {
			portage_parse_error(filename.c_str(), i, lines[i], e);
		}
	}
	return true;
}

typedef char ArchUsed;
static const ArchUsed
	ARCH_NOTHING        = 0,
	ARCH_STABLE         = 1,
	ARCH_UNSTABLE       = 2,
	ARCH_ALIENSTABLE    = 3,
	ARCH_ALIENUNSTABLE  = 4,
	ARCH_EVERYTHING     = 5,
	ARCH_MINUSASTERISK  = 6; // -* always matches -T WEAKER becuse it is higher than arch_needed default

static inline ArchUsed
apply_keyword(const string &key, const set<string> &keywords_set, KeywordsFlags kf,
	const set<string> *arch_set, bool obsolete_minus,
	Keywords::Redundant &redundant, Keywords::Redundant check, bool shortcut)
{
	static string tilde("~"), minus("-");
	if(!obsolete_minus) {
		if(key[0] == '-') {
			redundant |= (check & Keywords::RED_STRANGE);
			return ARCH_NOTHING;
		}
	}
	if(keywords_set.find(key) == keywords_set.end()) {
		// Not found:
		if(key == "**")
			return ARCH_EVERYTHING;
		if(key == "*") {
			if(kf.havesome(KeywordsFlags::KEY_SOMESTABLE))
				return ARCH_ALIENSTABLE;
		}
		if(key == "~*") {
			if(kf.havesome(KeywordsFlags::KEY_TILDESTARMATCH))
				return ARCH_ALIENUNSTABLE;
			redundant |= (check & Keywords::RED_STRANGE);
			return ARCH_NOTHING;
		}

		// Let us now check whether we trigger RED_STRANGE.
		// Since this test takes time, we check first whether the
		// result is required at all. Otherwise, we are done already:
		if(!(check & Keywords::RED_STRANGE))
			return ARCH_NOTHING;

		// Let s point to the "blank" keyword (without -/~)
		// have_searched is the "flag" which we have already tested.
		const string *s;
		string r;
		char have_searched = key[0];
		if((have_searched == '-') || (have_searched == '~')) {
			r = key.substr(1); s = &r;
		}
		else {
			s = &key; have_searched = '\0';
		}

		// Is the "blank" keyword in arch_set (possibly with -/~)?
		if(arch_set->find(*s) != arch_set->end())
			return ARCH_NOTHING;
		if(arch_set->find(tilde + *s) != arch_set->end())
			return ARCH_NOTHING;
		if(arch_set->find(minus + *s) != arch_set->end())
			return ARCH_NOTHING;

		// Is the "blank" keyword in KEYWORDS (possibly with -/~)?
		// (We can avoid the test which already has failed...)
		if(have_searched != '\0') {
			if(keywords_set.find(*s) != keywords_set.end())
				return ARCH_NOTHING;
		}
		if(have_searched != '~') {
			if(keywords_set.find(tilde + *s) != keywords_set.end())
				return ARCH_NOTHING;
		}
		if(have_searched != '-') {
			if(keywords_set.find(minus + *s) != keywords_set.end())
				return ARCH_NOTHING;
		}

		// None of the above tests succeeded, so have a strange key:
		redundant |= Keywords::RED_STRANGE;
		return ARCH_NOTHING;
	}
	// Found:
	if(shortcut) {
		// We do not care what stabilized it, so we speed things up:
		return ARCH_STABLE;
	}
	if(key[0] == '~') {
		// Usually, we have ARCH_UNSTABLE, but there are exceptions.
		// First, test special case:
		if(key == "~*")
			return ARCH_ALIENUNSTABLE;
		// We have an ARCH_UNSTABLE if key is in arch (with or without ~)
		if(arch_set->find(key) != arch_set->end())
			return ARCH_UNSTABLE;
		if(arch_set->find(key.substr(1)) != arch_set->end())
			return ARCH_UNSTABLE;
		return ARCH_ALIENUNSTABLE;
	}
	// Usually, we have ARCH_STABLE, but there are exceptions.
	// First, test special cases:
	if(key[0] == '-')
		return ARCH_MINUSASTERISK;
	if(key == "*")
		return ARCH_ALIENSTABLE;
	if(key == "**")
		return ARCH_EVERYTHING;
	// We have an ARCH_STABLE if key is in arch (with or without ~)
	if(arch_set->find(key) != arch_set->end())
		return ARCH_STABLE;
	if(arch_set->find(tilde + key) != arch_set->end())
		return ARCH_STABLE;
	return ARCH_ALIENSTABLE;
}

/// @return true if something from /etc/portage/package.* applied and check involves keywords
bool
PortageUserConfig::setKeyflags(Package *p, Keywords::Redundant check) const
{
	if((check & Keywords::RED_ALL_KEYWORDS) == Keywords::RED_NOTHING)
	{
		if(p->restore_keyflags(Version::SAVEKEY_USER))
			return false;
	}

	const eix::ptr_list<KeywordMask> *keyword_masks = m_keywords.get(p);
	map<Version*,vector<string> > sorted_by_versions;
	bool rvalue = false;

	bool obsolete_minusasterisk = m_settings->m_obsolete_minusasterisk;
	if(keyword_masks && (!keyword_masks->empty()))
	{
		rvalue = true;
		for(Package::iterator i = p->begin(); i != p->end(); ++i)
			push_backs<string>(sorted_by_versions[*i], m_settings->m_accepted_keywords);
		for(eix::ptr_list<KeywordMask>::const_iterator it = keyword_masks->begin();
			it != keyword_masks->end();
			++it)
		{
			eix::ptr_list<Version> matches = it->match(*p);

			for(eix::ptr_list<Version>::iterator  v = matches.begin();
				v != matches.end();
				++v)
			{
				push_backs<string>(sorted_by_versions[*v], split_string(it->keywords));
				// Set RED_DOUBLE_LINE depending on locally_double
				if(it->locally_double) {
					if(check & Keywords::RED_DOUBLE_LINE)
						v->set_redundant((v->get_redundant()) |
							Keywords::RED_DOUBLE_LINE);
				}
			}
		}
	}

	bool shortcut = !(check & (Keywords::RED_MIXED & Keywords::RED_WEAKER));
	for(Package::iterator i = p->begin();
		i != p->end(); ++i)
	{
		// Calculate ACCEPT_KEYWORDS state:

		Keywords::Redundant redundant = i->get_redundant();
		KeywordsFlags kf(i->get_keyflags(m_settings->m_accepted_keywords_set, obsolete_minusasterisk));
		i->keyflags=kf;
		i->save_keyflags(Version::SAVEKEY_ACCEPT);
		bool ori_is_stable = kf.havesome(KeywordsFlags::KEY_STABLE);

		// Were keywords added from /etc/portage/package.keywords?
		vector<string> &kv = sorted_by_versions[*i];
		bool calc_lkw = !kv.empty();
		if(calc_lkw) {
			if(kv.size() != m_settings->m_accepted_keywords.size())
				redundant |= Keywords::RED_IN_KEYWORDS;
			else if((check & (Keywords::RED_ALL_KEYWORDS &
				~(Keywords::RED_DOUBLE_LINE | Keywords::RED_IN_KEYWORDS)))
				== Keywords::RED_NOTHING)
				calc_lkw = false;
		}

		// Keywords were added or we must check for redundancy?
		if(calc_lkw)
		{
			// Create keywords_set of KEYWORDS from the ebuild
			set<string> keywords_set;
			make_set<string>(keywords_set, split_string(i->get_full_keywords()));

			// Create kv_set (of now active keywords), possibly testing for double keywords and -*
			set<string> kv_set;
			bool minusasterisk;
			if(check & Keywords::RED_DOUBLE) {
				vector<string> sorted = kv;
				if(sort_uniquify(sorted, true))
					redundant |= Keywords::RED_DOUBLE;
				bool minuskeyword = false;
				minusasterisk = resolve_plus_minus(kv_set, kv, obsolete_minusasterisk, &minuskeyword, &(m_settings->m_accepted_keywords_set));
				if(minuskeyword)
					redundant |= Keywords::RED_DOUBLE;
			}
			else
				minusasterisk = resolve_plus_minus(kv_set, kv, obsolete_minusasterisk);
			if(minusasterisk && !obsolete_minusasterisk)
					redundant |= (check & Keywords::RED_MINUSASTERISK);

			// First apply the original ACCEPT_KEYWORDS,
			// removing them from kv_set meanwhile.
			// The point is that we temporarily disable "check" so that
			// ACCEPT_KEYWORDS does not trigger any -T alarm.
			bool stable = false;
			for(vector<string>::iterator orikv = m_settings->m_accepted_keywords.begin();
				orikv != m_settings->m_accepted_keywords.end(); ++orikv)
			{
				{	// Tests whether keyword is admissible and remove it:
					set<string>::iterator where = kv_set.find(*orikv);
					if(where == kv_set.end()) {
						// The original keyword was removed by -...
						continue;
					}
					kv_set.erase(where);
				}
				if(apply_keyword(*orikv, keywords_set, kf,
					m_settings->m_local_arch_set,
					obsolete_minusasterisk,
					redundant, Keywords::RED_NOTHING, true)
					!= ARCH_NOTHING)
					stable = true;
			}

			// Now apply the remaining keywords (i.e. from /etc/portage/package.keywords)
			ArchUsed arch_used = ARCH_NOTHING;
			for(set<string>::iterator kvi = kv_set.begin();
				kvi != kv_set.end(); ++kvi) {
				ArchUsed arch_curr = apply_keyword(*kvi, keywords_set, kf,
					m_settings->m_local_arch_set,
					obsolete_minusasterisk,
					redundant, check, shortcut);
				if(arch_curr == ARCH_NOTHING)
					continue;
				if(arch_used < arch_curr)
					arch_used = arch_curr;
				if(stable || ori_is_stable)
					redundant |= (check & Keywords::RED_MIXED);
				stable = true;
			}

			// Was there a reason to trigger a WEAKER alarm?
			if(check & Keywords::RED_WEAKER)
			{
				ArchUsed arch_needed;
				if(ori_is_stable)
					arch_needed = ARCH_NOTHING;
				else if(kf.havesome(KeywordsFlags::KEY_ARCHUNSTABLE))
					arch_needed = ARCH_UNSTABLE;
				else if(kf.havesome(KeywordsFlags::KEY_ALIENSTABLE))
					arch_needed = ARCH_ALIENSTABLE;
				else if(kf.havesome(KeywordsFlags::KEY_ALIENUNSTABLE))
					arch_needed = ARCH_ALIENUNSTABLE;
				else
					arch_needed = ARCH_EVERYTHING;
				if(arch_used > arch_needed)
					redundant |= Keywords::RED_WEAKER;
			}

			// If stability was changed, note it and write it back.
			if(stable == kf.havesome(KeywordsFlags::KEY_STABLE))
				redundant |= Keywords::RED_NO_CHANGE;
			else {
				if(stable)
					kf.setbits(KeywordsFlags::KEY_STABLE);
				else
					kf.clearbits(KeywordsFlags::KEY_STABLE);
			}
		}

		// Store the result:

		i->keyflags=kf;
		i->save_keyflags(Version::SAVEKEY_USER);
		if(redundant)
			i->set_redundant(redundant);
	}
	return rvalue;
}

/// Set stability according to arch or local ACCEPT_KEYWORDS
void
PortageSettings::setKeyflags(Package *pkg, bool use_accepted_keywords) const
{
	const set<string> *accept_set;
	Version::SavedKeyIndex ind;
	if(use_accepted_keywords) {
		ind = Version::SAVEKEY_ACCEPT;
		accept_set = &m_accepted_keywords_set;
	}
	else {
		ind = Version::SAVEKEY_ARCH;
		accept_set = &m_arch_set;
	}
	if(pkg->restore_keyflags(ind))
		return;
	for(Package::iterator t = pkg->begin(); t != pkg->end(); ++t) {
		t->set_keyflags(*accept_set, m_obsolete_minusasterisk);
		t->save_keyflags(ind);
	}
}

void
PortageUserConfig::setProfileMasks(Package *p) const
{
	if(p->restore_maskflags(Version::SAVEMASK_USERPROFILE))
		return;
	if(profile)
		profile->applyMasks(p);
	else
		m_settings->setMasks(p);
	p->save_maskflags(Version::SAVEMASK_USERPROFILE);
}

/// @return true if something from /etc/portage/package.* applied and check involves masks
bool
PortageUserConfig::setMasks(Package *p, Keywords::Redundant check, bool file_mask_is_profile) const
{
	Version::SavedMaskIndex ind = file_mask_is_profile ?
		Version::SAVEMASK_USERFILE : Version::SAVEMASK_USER;
	if((check & Keywords::RED_ALL_KEYWORDS) == Keywords::RED_NOTHING)
	{
		if(p->restore_maskflags(ind))
			return false;
	}
	if(file_mask_is_profile) {
		if(!(p->restore_maskflags(Version::SAVEMASK_FILE))) {
			throw ExBasic("internal error: Tried to restore nonlocal mask without saving");
		}
	}
	else
		setProfileMasks(p);
	bool rvalue = m_localmasks.applyMasks(p, check);
	p->save_maskflags(ind);
	return rvalue;
}

void
PortageSettings::setMasks(Package *p, bool filemask_is_profile) const
{
	if(filemask_is_profile) {
		if(!(p->restore_maskflags(Version::SAVEMASK_FILE))) {
			throw ExBasic("internal error: Tried to restore nonlocal mask without saving");
		}
		return;
	}
	if(p->restore_maskflags(Version::SAVEMASK_PROFILE))
		return;
	profile->applyMasks(p);
	p->save_maskflags(Version::SAVEMASK_PROFILE);
}
