// vim:set noet cinoptions=g0,t0,^-2 sw=4 ts=4:
// This file is part of the eix project and distributed under the
// terms of the GNU General Public License v2.
//
// Copyright (c)
//   Wolfgang Frisch <xororand@users.sourceforge.net>
//   Emil Beinroth <emilbeinroth@gmx.net>
//   Martin Väth <vaeth@mathematik.uni-wuerzburg.de>

#ifndef EIX__BASICVERSION_H__
#define EIX__BASICVERSION_H__ 1

#include <config.h>

#include <list>
#include <string>

class BasicPart
{
public:
	enum PartType
	{
		garbage,
		alpha,
		beta,
		pre,
		rc,
		revision,
		inter_rev,
		patch,
		character,
		primary,
		first
	};
	// This must be larger than PartType elements and should be a power of 2.
	static const std::string::size_type max_type = 32;
	PartType parttype;
	std::string partcontent;

	BasicPart()
	{ }

	BasicPart(PartType p) : parttype(p), partcontent()
	{ }

	BasicPart(PartType p, std::string s) : parttype(p), partcontent(s)
	{ }

	BasicPart(PartType p, std::string s, std::string::size_type start) : parttype(p), partcontent(s, start)
	{ }

	BasicPart(PartType p, std::string s, std::string::size_type start, std::string::size_type end) : parttype(p), partcontent(s, start, end)
	{ }

	BasicPart(PartType p, char c) : parttype(p), partcontent(1, c)
	{ }

	BasicPart(PartType p, const char *s) : parttype(p), partcontent(s)
	{ }

	static short compare(const BasicPart& left, const BasicPart& right) ATTRIBUTE_PURE;
};


/** Parse and represent a portage version-string. */
class BasicVersion
{
public:
	virtual ~BasicVersion() { }

	/// Parse the version-string pointed to by str.
	bool parseVersion(const std::string& str, bool garbage_fatal, std::string *errtext);

	/// Compare all except gentoo revisions
	static short compareTilde(const BasicVersion& right, const BasicVersion& left) ATTRIBUTE_PURE;

	/// Compare the version.
	static short compare(const BasicVersion& right, const BasicVersion& left) ATTRIBUTE_PURE;

	std::string getFull() const;

	std::string getPlain() const;

	std::string getRevision() const;

protected:
	/// Splitted m_primsplit-version.
	std::list<BasicPart> m_parts;
};


// Short compare-stuff
inline bool operator <  (const BasicVersion& left, const BasicVersion& right) ATTRIBUTE_PURE;
inline bool
operator <  (const BasicVersion& left, const BasicVersion& right)
{ return BasicVersion::compare(left, right) < 0; }

inline bool operator >  (const BasicVersion& left, const BasicVersion& right) ATTRIBUTE_PURE;
inline bool
operator >  (const BasicVersion& left, const BasicVersion& right)
{ return BasicVersion::compare(left, right) > 0; }

inline bool operator == (const BasicVersion& left, const BasicVersion& right) ATTRIBUTE_PURE;
inline bool
operator == (const BasicVersion& left, const BasicVersion& right)
{ return BasicVersion::compare(left, right) == 0; }

inline bool operator != (const BasicVersion& left, const BasicVersion& right) ATTRIBUTE_PURE;
inline bool
operator != (const BasicVersion& left, const BasicVersion& right)
{ return BasicVersion::compare(left, right) != 0; }

inline bool operator >= (const BasicVersion& left, const BasicVersion& right) ATTRIBUTE_PURE;
inline bool
operator >= (const BasicVersion& left, const BasicVersion& right)
{ return BasicVersion::compare(left, right) >= 0; }

inline bool operator <= (const BasicVersion& left, const BasicVersion& right) ATTRIBUTE_PURE;
inline bool
operator <= (const BasicVersion& left, const BasicVersion& right)
{ return BasicVersion::compare(left, right) <= 0; }

#endif /* EIX__BASICVERSION_H__ */
