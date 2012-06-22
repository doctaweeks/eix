// vim:set noet cinoptions= sw=4 ts=4:
// This file is part of the eix project and distributed under the
// terms of the GNU General Public License v2.
//
// Copyright (c)
//   Wolfgang Frisch <xororand@users.sourceforge.net>
//   Emil Beinroth <emilbeinroth@gmx.net>
//   Martin Väth <vaeth@mathematik.uni-wuerzburg.de>

#include "cli.h"
#include <database/header.h>
#include <eixTk/argsreader.h>
#include <eixTk/likely.h>
#include <eixTk/null.h>
#include <eixTk/stringutils.h>
#include <eixrc/eixrc.h>
#include <output/formatstring.h>
#include <portage/basicversion.h>
#include <portage/conf/portagesettings.h>
#include <portage/set_stability.h>
#include <portage/vardbpkg.h>
#include <search/matchtree.h>
#include <search/packagetest.h>

#include <iostream>
#include <string>
#include <vector>

#include <cstdlib>

using namespace std;

#define FINISH_FORCE do { \
	matchtree->parse_test(test, curr_pipe); \
	force_test = curr_pipe = false; \
	test = NULLPTR; \
} while(0)

#define FINISH_TEST do { \
	if(force_test) { \
		FINISH_FORCE; \
	} \
} while(0)

#define NEW_TEST do { \
	test = new PackageTest(varpkg_db, portagesettings, stability, header); \
} while(0)

#define USE_TEST do { \
	if(test == NULLPTR) { \
		NEW_TEST; \
		force_test = true; \
	} \
} while(0)

inline bool
optional_increase(ArgumentReader::const_iterator &arg, const ArgumentReader &ar)
{
	ArgumentReader::const_iterator next(arg);
	if(unlikely(++next == ar.end()))
		return false;
	arg = next;
	return true;
}

void
parse_cli(MatchTree *matchtree, EixRc &eixrc, VarDbPkg &varpkg_db, PortageSettings &portagesettings, const SetStability &stability, const DBHeader &header, MarkedList **marked_list, const ArgumentReader& ar)
{
	bool	use_pipe(false),   // A pipe is used somewhere
		force_test(false), // There is a current test or a pipe
		curr_pipe(false);  // There is a current pipe
	short	pipe_mode(0);      // Do we force pipes of a particular mode?
	PackageTest *test(NULLPTR);   // The current test

	for(ArgumentReader::const_iterator arg(ar.begin());
		likely(arg != ar.end()); ++arg) {
		switch(**arg) {
			// Check for logical operator {{{
			case 'a': FINISH_TEST;
				matchtree->parse_and();
				break;
			case 'o': FINISH_TEST;
				matchtree->parse_or();
				break;
			case '!': FINISH_TEST;
				matchtree->parse_negate();
				break;
			case '(': FINISH_TEST;
				matchtree->parse_open();
				break;
			case ')': FINISH_TEST;
				matchtree->parse_close();
				break;
			// }}}

			// Check local options {{{
			// --pipe is a "faked" local option but actually treated by matchtree...
			// Note that we must *not* FINISH_TEST here!
			case O_PIPE_NAME:
			case O_PIPE_VERSION:
				pipe_mode = ((**arg == O_PIPE_VERSION) ? 1 : -1);
			case '|': force_test = curr_pipe = use_pipe = true;
				break;
			case 'I': USE_TEST;
				test->Installed();
				break;
			case 'i': USE_TEST;
				test->MultiInstalled();
				break;
			case '1': USE_TEST;
				test->Slotted();
				break;
			case '2': USE_TEST;
				test->MultiSlotted();
				break;
			case 'u': USE_TEST;
				test->Upgrade(eixrc.getLocalMode("UPGRADE_LOCAL_MODE"));
				break;
			case O_UPGRADE_LOCAL: USE_TEST;
				test->Upgrade(LOCALMODE_LOCAL);
				break;
			case O_UPGRADE_NONLOCAL: USE_TEST;
				test->Upgrade(LOCALMODE_NONLOCAL);
				break;
			case O_STABLE_DEFAULT: USE_TEST;
				test->SetStabilityDefault(PackageTest::STABLE_FULL);
				break;
			case O_TESTING_DEFAULT: USE_TEST;
				test->SetStabilityDefault(PackageTest::STABLE_TESTING);
				break;
			case O_NONMASKED_DEFAULT: USE_TEST;
				test->SetStabilityDefault(PackageTest::STABLE_NONMASKED);
				break;
			case O_BINARY: USE_TEST;
				test->Binary();
				break;
			case O_SELECTED_FILE: USE_TEST;
				test->SelectedFile();
				break;
			case O_SELECTED_SET: USE_TEST;
				test->SelectedSet();
				break;
			case O_SELECTED_ALL: USE_TEST;
				test->SelectedAll();
				break;
			case O_WORLD_FILE: USE_TEST;
				test->WorldFile();
				break;
			case O_WORLD_SET: USE_TEST;
				test->WorldSet();
				break;
			case O_WORLD_ALL: USE_TEST;
				test->WorldAll();
				break;
			case O_SYSTEM_DEFAULT: USE_TEST;
				test->SetStabilityDefault(PackageTest::STABLE_SYSTEM);
				break;
			case O_STABLE_LOCAL: USE_TEST;
				test->SetStabilityLocal(PackageTest::STABLE_FULL);
				break;
			case O_TESTING_LOCAL: USE_TEST;
				test->SetStabilityLocal(PackageTest::STABLE_TESTING);
				break;
			case O_NONMASKED_LOCAL: USE_TEST;
				test->SetStabilityLocal(PackageTest::STABLE_NONMASKED);
				break;
			case O_SYSTEM_LOCAL: USE_TEST;
				test->SetStabilityLocal(PackageTest::STABLE_SYSTEM);
				break;
			case O_STABLE_NONLOCAL: USE_TEST;
				test->SetStabilityNonlocal(PackageTest::STABLE_FULL);
				break;
			case O_TESTING_NONLOCAL: USE_TEST;
				test->SetStabilityNonlocal(PackageTest::STABLE_TESTING);
				break;
			case O_NONMASKED_NONLOCAL: USE_TEST;
				test->SetStabilityNonlocal(PackageTest::STABLE_NONMASKED);
				break;
			case O_SYSTEM_NONLOCAL: USE_TEST;
				test->SetStabilityNonlocal(PackageTest::STABLE_SYSTEM);
				break;
			case O_INSTALLED_UNSTABLE: USE_TEST;
				test->SetInstability(PackageTest::STABLE_FULL);
				break;
			case O_INSTALLED_TESTING: USE_TEST;
				test->SetInstability(PackageTest::STABLE_TESTING);
				break;
			case O_INSTALLED_MASKED: USE_TEST;
				test->SetInstability(PackageTest::STABLE_NONMASKED);
				break;
			case O_OVERLAY: USE_TEST;
				if(optional_increase(arg, ar)) {
					header.get_overlay_vector(
						test->OverlayList(),
						arg->m_argument,
						portagesettings["PORTDIR"].c_str());
					break;
				}
				// No break here...
			case 'O': USE_TEST;
				test->Overlay();
				break;
			case O_ONLY_OVERLAY: USE_TEST;
				if(optional_increase(arg, ar)) {
					header.get_overlay_vector(
						test->OverlayOnlyList(),
						arg->m_argument,
						portagesettings["PORTDIR"].c_str());
					break;
				}
				header.get_overlay_vector(
					test->OverlayOnlyList(),
					"",
					portagesettings["PORTDIR"].c_str());
				break;
			case O_INSTALLED_OVERLAY: USE_TEST;
				if(optional_increase(arg, ar)) {
					header.get_overlay_vector(
						test->InOverlayInstList(),
						arg->m_argument,
						portagesettings["PORTDIR"].c_str());
					break;
				}
				// No break here...
			case O_INSTALLED_SOME: USE_TEST;
				header.get_overlay_vector(
					test->InOverlayInstList(),
					"",
					portagesettings["PORTDIR"].c_str());
				break;
			case O_FROM_OVERLAY: USE_TEST;
				if(optional_increase(arg, ar)) {
					header.get_overlay_vector(
						test->FromOverlayInstList(),
						arg->m_argument,
						portagesettings["PORTDIR"].c_str());
					test->FromForeignOverlayInstList()->push_back(arg->m_argument);
					break;
				}
				// No break here...
			case 'J': USE_TEST;
				header.get_overlay_vector(
					test->FromOverlayInstList(),
					"",
					portagesettings["PORTDIR"].c_str());
				test->FromForeignOverlayInstList()->push_back("");
				break;
			case 'd': USE_TEST;
				test->DuplPackages(eixrc.getBool("DUP_PACKAGES_ONLY_OVERLAYS"));
				break;
			case 'D': USE_TEST;
				test->DuplVersions(eixrc.getBool("DUP_VERSIONS_ONLY_OVERLAYS"));
				break;
			case O_RESTRICT_FETCH: USE_TEST;
				test->Restrictions(ExtendedVersion::RESTRICT_FETCH);
				break;
			case O_RESTRICT_MIRROR: USE_TEST;
				test->Restrictions(ExtendedVersion::RESTRICT_MIRROR);
				break;
			case O_RESTRICT_PRIMARYURI: USE_TEST;
				test->Restrictions(ExtendedVersion::RESTRICT_PRIMARYURI);
				break;
			case O_RESTRICT_BINCHECKS: USE_TEST;
				test->Restrictions(ExtendedVersion::RESTRICT_BINCHECKS);
				break;
			case O_RESTRICT_STRIP: USE_TEST;
				test->Restrictions(ExtendedVersion::RESTRICT_STRIP);
				break;
			case O_RESTRICT_TEST: USE_TEST;
				test->Restrictions(ExtendedVersion::RESTRICT_TEST);
				break;
			case O_RESTRICT_USERPRIV: USE_TEST;
				test->Restrictions(ExtendedVersion::RESTRICT_USERPRIV);
				break;
			case O_RESTRICT_INSTALLSOURCES: USE_TEST;
				test->Restrictions(ExtendedVersion::RESTRICT_INSTALLSOURCES);
				break;
			case O_RESTRICT_BINDIST: USE_TEST;
				test->Restrictions(ExtendedVersion::RESTRICT_BINDIST);
				break;
			case O_RESTRICT_PARALLEL: USE_TEST;
				test->Restrictions(ExtendedVersion::RESTRICT_PARALLEL);
				break;
			case O_PROPERTIES_INTERACTIVE: USE_TEST;
				test->Properties(ExtendedVersion::PROPERTIES_INTERACTIVE);
				break;
			case O_PROPERTIES_LIVE: USE_TEST;
				test->Properties(ExtendedVersion::PROPERTIES_LIVE);
				break;
			case O_PROPERTIES_VIRTUAL: USE_TEST;
				test->Properties(ExtendedVersion::PROPERTIES_VIRTUAL);
				break;
			case O_PROPERTIES_SET: USE_TEST;
				test->Properties(ExtendedVersion::PROPERTIES_SET);
				break;
			case 'T': USE_TEST;
			{
				EixRc::RedPair red;
				red.first = red.second = RedAtom();
				if(likely(eixrc.getBool("TEST_FOR_REDUNDANCY"))) {
					eixrc.getRedundantFlags("REDUNDANT_IF_DOUBLE",
						Keywords::RED_DOUBLE, red);
					eixrc.getRedundantFlags("REDUNDANT_IF_DOUBLE_LINE",
						Keywords::RED_DOUBLE_LINE, red);
					eixrc.getRedundantFlags("REDUNDANT_IF_MIXED",
						Keywords::RED_MIXED, red);
					eixrc.getRedundantFlags("REDUNDANT_IF_WEAKER",
						Keywords::RED_WEAKER, red);
					eixrc.getRedundantFlags("REDUNDANT_IF_STRANGE",
						Keywords::RED_STRANGE, red);
					eixrc.getRedundantFlags("REDUNDANT_IF_NO_CHANGE",
						Keywords::RED_NO_CHANGE, red);
					eixrc.getRedundantFlags("REDUNDANT_IF_MASK_NO_CHANGE",
						Keywords::RED_MASK, red);
					eixrc.getRedundantFlags("REDUNDANT_IF_UNMASK_NO_CHANGE",
						Keywords::RED_UNMASK, red);
					eixrc.getRedundantFlags("REDUNDANT_IF_DOUBLE_MASKED",
						Keywords::RED_DOUBLE_MASK, red);
					eixrc.getRedundantFlags("REDUNDANT_IF_DOUBLE_UNMASKED",
						Keywords::RED_DOUBLE_UNMASK, red);
					eixrc.getRedundantFlags("REDUNDANT_IF_DOUBLE_USE",
						Keywords::RED_DOUBLE_USE, red);
					eixrc.getRedundantFlags("REDUNDANT_IF_DOUBLE_ENV",
						Keywords::RED_DOUBLE_ENV, red);
					eixrc.getRedundantFlags("REDUNDANT_IF_DOUBLE_LICENSE",
						Keywords::RED_DOUBLE_LICENSE, red);
					eixrc.getRedundantFlags("REDUNDANT_IF_DOUBLE_CFLAGS",
						Keywords::RED_DOUBLE_CFLAGS, red);
					eixrc.getRedundantFlags("REDUNDANT_IF_IN_KEYWORDS",
						Keywords::RED_IN_KEYWORDS, red);
					eixrc.getRedundantFlags("REDUNDANT_IF_IN_MASK",
						Keywords::RED_IN_MASK, red);
					eixrc.getRedundantFlags("REDUNDANT_IF_IN_UNMASK",
						Keywords::RED_IN_UNMASK, red);
					eixrc.getRedundantFlags("REDUNDANT_IF_IN_USE",
						Keywords::RED_IN_USE, red);
					eixrc.getRedundantFlags("REDUNDANT_IF_IN_ENV",
						Keywords::RED_IN_ENV, red);
					eixrc.getRedundantFlags("REDUNDANT_IF_IN_LICENSE",
						Keywords::RED_IN_LICENSE, red);
					eixrc.getRedundantFlags("REDUNDANT_IF_IN_CFLAGS",
						Keywords::RED_IN_CFLAGS, red);
				}
				PackageTest::TestInstalled test_installed = PackageTest::INS_NONE;
				if(likely(eixrc.getBool("TEST_FOR_NONEXISTENT"))) {
					test_installed |= PackageTest::INS_NONEXISTENT;
					if(eixrc.getBool("NONEXISTENT_IF_MASKED"))
						test_installed |= PackageTest::INS_MASKED;
					if(eixrc.getBool("NONEXISTENT_IF_OTHER_OVERLAY")) {
						test_installed |= PackageTest::INS_OVERLAY;
					}
				}
				test->ObsoleteCfg(red.first, red.second, test_installed);
			}
				break;
			// }}}

			// Check for field-designators {{{
			case 's': USE_TEST;
				*test |= PackageTest::NAME;
				break;
			case 'C': USE_TEST;
				*test |= PackageTest::CATEGORY;
				break;
			case 'A': USE_TEST;
				*test |= PackageTest::CATEGORY_NAME;
				break;
			case 'S': USE_TEST;
				*test |= PackageTest::DESCRIPTION;
				break;
			case 'L': USE_TEST;
				*test |= PackageTest::LICENSE;
				break;
			case 'H': USE_TEST;
				*test |= PackageTest::HOMEPAGE;
				break;
			case 'U': USE_TEST;
				*test |= PackageTest::IUSE;
				break;
			case O_DEPEND: USE_TEST;
				*test |= PackageTest::DEPEND;
				break;
			case O_PDEPEND: USE_TEST;
				*test |= PackageTest::PDEPEND;
				break;
			case O_RDEPEND: USE_TEST;
				*test |= PackageTest::RDEPEND;
				break;
			case O_DEPS: USE_TEST;
				*test |= PackageTest::DEPS;
				break;
			case O_SEARCH_SET: USE_TEST;
				*test |= PackageTest::SET;
				break;
			case O_SEARCH_SLOT: USE_TEST;
				*test |= PackageTest::SLOT;
				break;
			case O_INSTALLED_WITH_USE: USE_TEST;
				*test |= PackageTest::USE_ENABLED;
				break;
			case O_INSTALLED_WITHOUT_USE: USE_TEST;
				*test |= PackageTest::USE_DISABLED;
				break;
			case O_INSTALLED_SLOT: USE_TEST;
				*test |= PackageTest::INSTALLED_SLOT;
				break;
			case 'y': USE_TEST;
				*test |= PackageTest::ANY;
				break;
			// }}}

			// Check for algorithms {{{
			case 'f': USE_TEST;
				if(unlikely((++arg != ar.end())
					&& (arg->type == Parameter::ARGUMENT)
					&& is_numeric(arg->m_argument))) {
					test->setAlgorithm(new FuzzyAlgorithm(my_atoi(arg->m_argument)));
				}
				else {
					test->setAlgorithm(PackageTest::ALGO_FUZZY);
					--arg;
				}
				break;
			case 'r': USE_TEST;
				test->setAlgorithm(new RegexAlgorithm());
				break;
			case 'e': USE_TEST;
				test->setAlgorithm(new ExactAlgorithm());
				break;
			case 'b': USE_TEST;
				test->setAlgorithm(new BeginAlgorithm());
				break;
			case O_END_ALGO: USE_TEST;
				test->setAlgorithm(new EndAlgorithm());
				break;
			case 'z': USE_TEST;
				test->setAlgorithm(new SubstringAlgorithm());
				break;
			case 'p': USE_TEST;
				test->setAlgorithm(new PatternAlgorithm());
				break;
			// }}}

			// String arguments .. finally! {{{
			case -1: USE_TEST;
				test->setPattern(arg->m_argument);
				FINISH_FORCE;
				break;
			// }}}
			default:
				break;
		}
	}
	FINISH_TEST;
	matchtree->end_parse();
	*marked_list = NULLPTR;

	if(!use_pipe) {
		return;
	}

	// If we have a pipe, we must call matchtree->set_pipetest()
	// and also fill the marked_list;
	while(likely(!cin.eof())) {
		string line;
		getline(cin, line);
		trim(line);
		vector<string> wordlist;
		split_string(wordlist, line);
		for(vector<string>::iterator word(wordlist.begin());
			likely(word != wordlist.end()); ++word) {
			string::size_type i(word->find("/"));
			if((i == string::npos) || (i == 0) || (i == word->size() - 1))
				continue;
			if(word->find("/", i + 1) != string::npos)
				continue;
			if((*word)[0] == '=') {
				word->erase(0, 1);
			}
			bool success(false);
			char **name_ver(NULLPTR);
			const char *name, *ver;
			if(pipe_mode >= 0) {
				if((name_ver = ExplodeAtom::split(word->c_str())) != NULLPTR) {
					name = name_ver[0];
					ver  = name_ver[1];
					// parseVersion() to split only valid versions:
					BasicVersion b;
					success = b.parseVersion(ver, true, NULLPTR);
				}
			}
			if((pipe_mode <= 0) && (!success) && !word->empty()) {
				name = word->c_str();
				ver  = NULLPTR;
				success = true;
			}
			if(success) {
				if(unlikely(*marked_list == NULLPTR)) {
					*marked_list = new MarkedList();
				}
				(*marked_list)->add(name, ver);

				NEW_TEST;
				*test = PackageTest::CATEGORY_NAME;
				test->setAlgorithm(new ExactAlgorithm());
				test->setPattern(name);
				matchtree->set_pipetest(test);
			}
			if(name_ver != NULLPTR) {
				free(name_ver[0]);
				free(name_ver[1]);
			}
		}
	}
}

// vim:set foldmethod=marker foldlevel=0:
