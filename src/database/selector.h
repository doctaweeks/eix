/***************************************************************************
 *   eix is a small utility for searching ebuilds in the                   *
 *   Gentoo Linux portage system. It uses indexing to allow quick searches *
 *   in package descriptions with regular expressions.                     *
 *                                                                         *
 *   https://sourceforge.net/projects/eix                                  *
 *                                                                         *
 *   Copyright (c)                                                         *
 *     Wolfgang Frisch <xororand@users.sourceforge.net>                    *
 *     Emil Beinroth <emilbeinroth@gmx.net>                                *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#ifndef __DBSELECTOR_H__
#define __DBSELECTOR_H__

#include <database/header.h>

#include <search/dbmatchcriteria.h>

#include <portage/package.h>

/** Select packages that matches a Matchatom. */
class DatabaseMatchIterator {

	private:
		DBHeader        *_header;    /**< Header of the database. */
		Matchatom *_criterium; /**< Criterium to match the packages. */
		FILE            *_input;     /**< Input stream for database. */

		string       _catname; /**< Name of current category. */
		unsigned int _pkgs,    /**< Packages left in the current category. */
					 _cats;    /**< Categories left in database. */

	public:
		/** Set header, tree and matcher you want to use. */
		DatabaseMatchIterator(DBHeader *header, FILE *istream, Matchatom *criterium) {
			_header    = header;
			_input     = istream;
			_criterium = criterium;
			_pkgs      = 0;
			_cats      = header->numcategories;
		}

		/** Get next matching package or NULL if no more packages match. */
		Package *next();
};

#endif /* __DBSELECTOR_H__ */
